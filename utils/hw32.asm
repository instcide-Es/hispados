; =============================================================================
;  HW32.COM - Hispa-486: ejecuta programas Win32 (PE) de consola encima de
;  Hispa-DOS y HSPDPMI. Fase 1 (docs/486/HISPA-486.md).
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;       HW32 PROGRAMA.EXE [parámetros]
;
;  Cómo funciona:
;  - Entra en modo protegido como cliente DPMI de 32 bits (HSPDPMI).
;  - Pide un bloque de memoria (INT 31h, 0501h) para la imagen, los
;    trampolines, lo suyo (línea de órdenes, TIB) y la pila; los selectores
;    del programa tienen como base «bloque - ImageBase» y límite de 4 GB: el
;    programa ve sus direcciones de siempre aunque no traiga reubicados.
;  - Cada función importada apunta a un trampolín dentro de su espacio:
;    «push número; jmp lejos despacho». El despacho (en nuestro código) llama
;    a la función nuestra con los argumentos en [EBP], y vuelve al programa
;    quitando los argumentos (stdcall) con un RETF.
;  - Lo de DOS (archivos, consola) va por un búfer en memoria convencional y
;    INT 31h, 0300h. Lo que no está hecho dice «falta» con su nombre.
;  Sala limpia: hecho a partir de la documentación pública del formato PE y
;  del API Win32.
; =============================================================================
        cpu 386
        bits 16
        org 0x100

TB_PARR     equ 0x400               ; el búfer de DOS: 16 KB
TB_TAM      equ TB_PARR * 16
; las zonas del búfer de DOS (con los programas ya en marcha)
TB_TROZO    equ 0x3000              ; lo que se lee o escribe de una vez
TB_DTA      equ 0x3000              ; las búsquedas de archivos (64 cada una)
N_BUSCA     equ 8
TB_NOM1     equ 0x3200              ; nombres de archivo
TB_NOM2     equ 0x3400
TB_SAL      equ 0x3600              ; respuestas de DOS (directorio, nombre)
MAX_IMP     equ 512                 ; funciones importadas como mucho
TRAMP       equ 12                  ; lo que ocupa un trampolín (push 5 + jmp lejos 7)
MISC_TAM    equ 0x5000              ; lo nuestro dentro del bloque
PILA_MAX    equ 0x100000            ; la pila: lo que pida, hasta 1 MB
MONTON_TAM  equ 0x100000            ; cada trozo de montón que se pide

inicio:
        ; («strict»: el salto mide tres octetos siempre y la firma queda en
        ; 103h; con «near» a secas NASM 3 lo acorta a dos y Hispa Ventanas no
        ; encuentra el módulo)
        jmp strict near inicio16
; ---- [fase 3c] HW32 como MÓDULO de Hispa Ventanas. Hispa Ventanas carga este
; mismo archivo en memoria extendida, le hace dos selectores de 32 bits (código
; y datos, con la base en «copia − 100h»), rellena mod_d y llama (lejos) a
; mod_cargar: el programa de ventanas tiene así el mismo KERNEL32 y el mismo
; cargador de DLL que uno de consola. Lo que es del entorno (USER32, GDI32...)
; se lo pide a Hispa Ventanas por md_resolver. Aquí, en 103h, la firma y la
; tabla que busca Hispa Ventanas (ventanas/win32.inc) ----
        db 'HW32MOD', 1
        dd mod_cargar               ; +0  (lejos) carga el programa -> EAX=0 si bien
        dd mod_fin                  ; +4  (lejos) suelta todo lo del programa
        dd mod_d                    ; +8  los datos que se cruzan
        dd nombre                   ; +12 el programa (hasta 129 letras)
        dd mod_param                ; +16 sus parámetros (con el espacio delante)
        dd hw32_dir                 ; +20 la carpeta de HW32 (acabada en barra)
%if ($ - inicio) != 3 + 8 + 6 * 4
 %error "la cabecera de módulo de HW32 no está en 103h"
%endif
inicio16:
        mov sp, pila16_cima
        mov ax, [0x2C]              ; el entorno de DOS
        mov [env_seg], ax
        ; las mitades altas de los registros, a cero: DOS no las toca y pueden
        ; traer lo del programa anterior (y DPMI las usa con los clientes de
        ; 32 bits)
        xor eax, eax
        xor ebx, ebx
        xor ecx, ecx
        xor edx, edx
        xor esi, esi
        xor edi, edi
        xor ebp, ebp
        mov [segrm], cs
        ; la línea de órdenes, a orden (ASCIZ)
        mov si, 0x81
        mov di, orden
        xor cx, cx
        mov cl, [0x80]
        rep movsb
        mov byte [di], 0
        ; menos memoria para nosotros, y el búfer de DOS
        mov bx, (fin_prog - inicio + 0x100 + 15) / 16
        mov ah, 0x4A
        int 0x21
        mov bx, TB_PARR
        mov ah, 0x48
        int 0x21
        jc no_mem
        mov [tb_seg], ax
        ; DPMI
        mov ax, 0x1687
        int 0x2F
        test ax, ax
        jnz no_dpmi
        test bl, 1
        jz no_dpmi
        mov [entrada], di
        mov [entrada + 2], es
        mov bx, si
        xor ax, ax
        mov es, ax
        test bx, bx
        jz .sin_priv
        mov ah, 0x48
        int 0x21
        jc no_mem
        mov es, ax
.sin_priv:
        mov ax, 1
        call far [entrada]
        jc no_dpmi
        ; modo protegido, en código de 16: hacer los de 32
        mov [sel_psp], es
        mov [sel_ds], ds
        push ds
        pop es
        xor ax, ax
        mov cx, 2
        int 0x31
        jc no_sel
        mov [sel_cs32], ax
        add ax, 8
        mov [sel_ss32], ax
        mov bx, cs
        mov ax, 0x000B
        mov edi, desc               ; (EDI entero: somos cliente de 32 bits y
        int 0x31                    ; la mitad alta puede traer basura)
        or byte [desc + 6], 0x40
        mov bx, [sel_cs32]
        mov ax, 0x000C
        int 0x31
        mov bx, ds
        mov ax, 0x000B
        int 0x31
        or byte [desc + 6], 0x40
        mov bx, [sel_ss32]
        mov ax, 0x000C
        int 0x31
        movzx esp, sp
        mov ss, [sel_ss32]
        push word [sel_cs32]
        push word principal
        retf
no_dpmi:
        mov dx, m_no_dpmi
        jmp sal16
no_mem:
        mov dx, m_no_mem
        jmp sal16
no_sel:
        mov dx, m_no_sel
sal16:
        mov ah, 0x09
        int 0x21
        mov ax, 0x4CFF
        int 0x21

; =============================================================================
;  32 bits
; =============================================================================
        bits 32
principal:
        cld
        ; el búfer de DOS, también con selector
        mov ax, 0x0002
        mov bx, [tb_seg]
        int 0x31
        jc .mal_sel
        mov [sel_tb], ax
        ; la zona de la BIOS, la pantalla y el entorno
        mov bx, 0x40
        call sel_de_seg
        mov [sel_bios], ax
        mov bx, 0xB800
        call sel_de_seg
        mov [sel_vid], ax
        mov bx, 0xB000
        call sel_de_seg
        mov [sel_mda], ax
        mov bx, [env_seg]
        test bx, bx
        jz .sin_env
        call sel_de_seg
        mov [sel_env], ax
.sin_env:
        call carpeta_hw32
        ; el nombre del programa y lo demás de la línea
        mov esi, orden
.e:
        lodsb
        cmp al, ' '
        je .e
        dec esi
        test al, al
        jz uso
        ; /T: la traza (cada llamada a KERNEL32, con sus argumentos y lo que
        ; devuelve, por la salida de errores)
        cmp al, '/'
        jne .no_t
        mov al, [esi + 1]
        and al, 0xDF
        cmp al, 'T'
        jne .no_t
        cmp byte [esi + 2], ' '
        ja .no_t
        mov byte [traza], 1
        add esi, 2
        jmp .e
.no_t:
        mov edi, nombre
.n:
        lodsb
        cmp al, ' '
        jbe .n_f
        stosb
        jmp .n
.n_f:
        dec esi
        mov byte [edi], 0
        mov [resto], esi            ; los parámetros del programa
        call carga_pe
        jmp ejecuta
.mal_sel:
        mov esi, m_no_sel32
        jmp falla
uso:
        mov esi, m_uso
        call pon_cad
        mov ax, 0x4C01
        int 0x21

; ---- escribe el texto ASCIZ de DS:ESI por la salida (DOS, por el búfer) ----
pon_cad:
        push eax
        push ecx
        push esi
        push edi
        push es
        mov es, [sel_tb]
        xor edi, edi
.c:
        lodsb
        test al, al
        jz .f
        stosb
        cmp edi, TB_TROZO - 1
        jb .c
.f:
        mov ecx, edi
        call escribe_tb             ; ECX octetos del búfer a la salida
        pop es
        pop edi
        pop esi
        pop ecx
        pop eax
        ret
; escribe ECX octetos del búfer de DOS en el handle 1 (o el de [h_esc])
escribe_tb:
        mov word [rm_bx], 1
escribe_tb_h:                       ; (rm_bx = el handle)
        mov [rm_cx], cx
        mov word [rm_dx], 0
        mov word [rm_ax], 0x4000
        jmp dos
; ---- falla con el mensaje DS:ESI ----
falla:
        cmp byte [md_modulo], 0
        jne falla_mod
        call pon_cad
        call red_soltar
        mov ax, 0x4CFF
        int 0x21
; (módulo) al cargar: se vuelve a Hispa Ventanas con el texto; ya en marcha:
; se le pide que lo diga y que acabe el programa
falla_mod:
        cmp byte [en_marcha], 0
        jne .ya
        mov [md_error], esi
        mov eax, 1
        jmp mod_cargar.vuelve
.ya:
        mov eax, esi
        mov ecx, 1
        jmp aviso_y_fin
; ---- (módulo) Hispa Ventanas dice el aviso y acaba el programa: ECX=0, falta
; la función EAX (su nombre, en el programa; 0: por número); 1, el texto EAX
; (nuestro); 2, el texto EAX (del programa) ----
aviso_y_fin:
        mov edx, [md_ficha]
        jmp dword far [md_falta]
; ---- el programa acaba con el código AL ----
acaba:
        call red_soltar             ; (la red, si la abrió: ver hw32k.inc)
        cmp byte [md_modulo], 0
        jne .mod
        mov ah, 0x4C
        int 0x21
.mod:
        movzx eax, al
        mov edx, [md_ficha]
        jmp dword far [md_salir]

; ---- la carpeta de HW32 (de lo que hay tras el entorno) a hw32_dir ----
carpeta_hw32:
        cmp word [sel_env], 0
        je .x
        push ds
        push es
        push ds
        pop es                      ; ES: lo nuestro
        mov ax, [sel_env]
        mov ds, ax                  ; DS: el entorno
        xor esi, esi
.b:
        cmp word [esi], 0           ; el final del entorno
        je .f
        inc esi
        cmp esi, 0x7FF0
        jb .b
        pop es
        pop ds
        ret
.f:
        add esi, 4                  ; los dos ceros y la cuenta
        mov edi, hw32_dir
        mov edx, edi                ; tras la última barra
        mov ecx, 120
.c:
        lodsb
        stosb
        test al, al
        jz .c_f
        cmp al, '\'
        jne .c1
        mov edx, edi
.c1:
        loop .c
.c_f:
        pop es
        pop ds
        mov byte [edx], 0
.x:
        ret

; ---- un selector para el segmento de modo real BX (DPMI 0002h) ----
sel_de_seg:
        mov ax, 0x0002
        int 0x31
        jnc .x
        mov esi, m_no_sel32
        jmp falla
.x:
        ret

; ---- una llamada a DOS (INT 21h) en modo real con los rm_*; DS y ES de
; modo real = el búfer. CF=1 si DOS devuelve acarreo. int_rm: la
; interrupción BL ----
dos:
        push ebx
        mov bl, 0x21
        call int_rm
        pop ebx
        ret
int_rm:
        cmp byte [md_modulo], 0
        jne int_rm_mod
int_rm_real:
        push eax
        mov ax, [tb_seg]
        mov [rm_ds], ax
        mov [rm_es], ax
        pop eax
int_rm_cru:                         ; (con rm_ds y rm_es ya puestos)
        push eax
        push ebx
        push ecx
        push edi
        push es
        mov dword [rm_ss_sp], 0
        mov word [rm_flags], 0x0202
        push ds
        pop es
        mov edi, rmr
        mov ax, 0x0300
        movzx ebx, bl
        xor ecx, ecx
        int 0x31
        pop es
        pop edi
        pop ecx
        pop ebx
        pop eax
        bt word [rm_flags], 0
        ret
; ---- (módulo) dentro de Hispa Ventanas no hay consola: lo que va a la
; pantalla de texto o viene del teclado por la BIOS o por los handles 0 a 2
; no se hace (lo escrito se da por escrito; de leer, nada); no se ejecutan
; otros programas; se lleva la cuenta de los archivos abiertos (mod_fin los
; cierra) y, tras una búsqueda, la DTA vuelve a ser la de Hispa Ventanas ----
int_rm_mod:
        cmp bl, 0x21
        je .dos
        cmp bl, 0x16
        je .tecla
        cmp bl, 0x10
        jne int_rm_real
.nada:
        mov word [rm_flags], 0x0202
        clc
        ret
.tecla:
        mov word [rm_flags], 0x0242 ; (ZF: no hay tecla)
        mov word [rm_ax], 0
        clc
        ret
.dos:
        push eax
        mov ah, [rm_ax + 1]
        mov [mod_ah], ah
        cmp ah, 0x0C
        jbe .dos_nada               ; consola de DOS
        cmp ah, 0x4B                ; EXEC: no
        je .dos_no
        cmp ah, 0x40
        je .dos_esc
        cmp ah, 0x3F
        je .dos_lee
        cmp ah, 0x3E
        jne .dos_si
        cmp word [rm_bx], 4         ; los de serie no se cierran
        ja .dos_si
.dos_nada:
        pop eax
        jmp .nada
.dos_no:
        pop eax
        mov word [rm_ax], 8
        mov word [rm_flags], 0x0203
        stc
        ret
.dos_esc:
        cmp word [rm_bx], 2
        ja .dos_si
        mov ax, [rm_cx]
        mov [rm_ax], ax
        pop eax
        jmp .nada
.dos_lee:
        cmp word [rm_bx], 2
        ja .dos_si
        mov word [rm_ax], 0
        pop eax
        jmp .nada
.dos_si:
        pop eax
        call int_rm_real
        pushfd
        push eax
        push ebx
        mov al, [mod_ah]
        cmp al, 0x4E
        je .dta
        cmp al, 0x4F
        je .dta
        test byte [esp + 8], 1      ; (el acarreo de DOS)
        jnz .dos_x
        movzx ebx, word [rm_ax]
        cmp al, 0x3C
        je .abre
        cmp al, 0x3D
        je .abre
        cmp al, 0x5B
        je .abre
        cmp al, 0x5A
        je .abre
        cmp al, 0x45
        je .abre
        cmp al, 0x6C
        je .abre
        movzx ebx, word [rm_bx]
        cmp al, 0x3E
        je .cierra
        jmp .dos_x
.dta:
        call dta_anfitrion
        jmp .dos_x
.abre:
        cmp ebx, 255
        ja .dos_x
        bts [mod_abiertos], ebx
        jmp .dos_x
.cierra:
        cmp ebx, 255
        ja .dos_x
        btr [mod_abiertos], ebx
.dos_x:
        pop ebx
        pop eax
        popfd
        ret
; (módulo) la DTA de DOS, otra vez la de Hispa Ventanas (PSP:80h). Conserva
; los rm_*
dta_anfitrion:
        push dword [rm_eax]
        push dword [rm_edx]
        push dword [rm_flags]
        mov ax, [md_psp]
        mov [rm_ds], ax
        mov [rm_es], ax
        mov word [rm_dx], 0x80
        mov word [rm_ax], 0x1A00
        push ebx
        mov bl, 0x21
        call int_rm_cru
        pop ebx
        pop dword [rm_flags]
        pop dword [rm_edx]
        pop dword [rm_eax]
        ret

; =============================================================================
;  Cargar el PE
; =============================================================================
; ---- las fichas de los módulos: el programa (la 0) y sus DLL ----
N_MOD       equ 16
MOD_TAM     equ 64
M_DIR       equ 0                   ; dónde está (en el programa)
M_TAM       equ 4
M_EXP       equ 8                   ; las exportaciones (RVA, tamaño)
M_EXPN      equ 12
M_IMP       equ 16                  ; las importaciones (RVA)
M_ENT       equ 20                  ; la entrada (RVA; 0: sin DllMain)
M_DLL       equ 24
M_NOM       equ 32                  ; el nombre (en mayúsculas)

carga_pe:
        mov esi, nombre
        call abre_pe
        jc .no_esta
        mov eax, [h_base]
        mov [img_base], eax
        ; la pila
        mov ebp, [pila_res]
        cmp ebp, PILA_MAX
        jbe .p
        mov ebp, PILA_MAX
.p:
        cmp ebp, 0x10000
        jae .p2
        mov ebp, 0x10000
.p2:
        add ebp, 0xFFF
        and ebp, ~0xFFF
        mov [pila_tam], ebp
        ; el bloque: imagen + trampolines + lo nuestro + pila
        mov eax, [img_tam]
        add eax, 0xFFF
        and eax, ~0xFFF
        mov [tramp_off], eax
        add eax, MAX_IMP * TRAMP
        add eax, 0xFFF
        and eax, ~0xFFF
        mov [misc_off], eax
        add eax, MISC_TAM
        mov [pila_off], eax
        add eax, [pila_tam]
        mov [bloque_tam], eax
        call pide_bloque            ; EAX tamaño -> EBX lineal
        jc .sin_mem
        mov [bloque_lin], ebx
        ; la base del programa: bloque - ImageBase
        sub ebx, [img_base]
        mov [prog_base], ebx
        mov [md_base], ebx          ; (módulo: Hispa Ventanas ya la necesita al importar)
        call hace_selectores
        ; a cero
        push es
        mov es, [sel_pd]
        mov edi, [img_base]
        mov ecx, [bloque_tam]
        shr ecx, 2
        xor eax, eax
        rep stosd
        pop es
        mov eax, [img_base]
        mov [mod_dir], eax
        call copia_secciones
        call cierra_pe
        ; su ficha (con el nombre del archivo, sin la ruta)
        mov ebp, mods
        mov esi, nombre
        mov edi, nombre
.n:
        lodsb
        test al, al
        jz .n_f
        cmp al, '\'
        je .n_b
        cmp al, ':'
        jne .n
.n_b:
        mov edi, esi
        jmp .n
.n_f:
        mov esi, edi
        xor bl, bl
        call llena_ficha
        ; y la carpeta del programa (donde se buscan sus DLL primero)
        mov ecx, edi
        sub ecx, nombre
        mov esi, nombre
        mov edi, exe_dir
        rep movsb
        mov byte [edi], 0
        mov dword [n_mods], 1
        call importaciones          ; (EBP = la ficha)
        ret
.no_esta:
        mov esi, m_no_esta
        jmp falla
.sin_mem:
        mov esi, m_sin_mem
        jmp falla

; ---- abre el PE de nombre DS:ESI y lee sus cabeceras (h_base, img_tam,
; n_sec...); CF=1 si no se puede abrir; si no es un PE, falla ----
abre_pe:
        push es
        mov es, [sel_tb]
        xor edi, edi
.cp:
        lodsb
        stosb
        test al, al
        jnz .cp
        pop es
        mov word [rm_ax], 0x3D00
        mov word [rm_dx], 0
        call dos
        jc .x
        mov ax, [rm_ax]
        mov [h_exe], ax
        ; las primeras 4 KB: MZ, PE, cabeceras y secciones
        xor eax, eax
        call lee_en_tb_desde
        push ds
        mov ds, [sel_tb]
        cmp word [0], 'MZ'
        jne .no_pe_ds
        mov eax, [0x3C]
        cmp eax, 0x1000 - 0xF8
        ja .no_pe_ds
        cmp dword [eax], 'PE'
        jne .no_pe_ds
        cmp word [eax + 4], 0x014C
        jne .no_pe_ds
        mov ebx, eax
        movzx ecx, word [ebx + 6]   ; secciones
        movzx edx, word [ebx + 0x14] ; tamaño de la cabecera opcional
        mov esi, [ebx + 0x18 + 0x10] ; punto de entrada
        mov edi, [ebx + 0x18 + 0x1C] ; ImageBase
        mov eax, [ebx + 0x18 + 0x38] ; SizeOfImage
        mov ebp, [ebx + 0x18 + 0x48] ; pila reservada
        pop ds
        mov [n_sec], ecx
        mov [entrada_rva], esi
        mov [h_base], edi
        mov [img_tam], eax
        mov [pila_res], ebp
        lea eax, [ebx + 0x18 + edx]
        mov [tabla_sec], eax
        push ds
        mov ds, [sel_tb]
        mov eax, [ebx + 0x18 + 0x60]       ; 0: exportaciones
        mov ecx, [ebx + 0x18 + 0x60 + 4]
        mov edx, [ebx + 0x18 + 0x60 + 8]   ; 1: importaciones
        mov esi, [ebx + 0x18 + 0x60 + 40]  ; 5: reubicados
        mov edi, [ebx + 0x18 + 0x60 + 44]
        mov ebp, [ebx + 0x18 + 0x3C]       ; SizeOfHeaders (fase 3d: antes 0x54, el
                                           ; SizeOfHeapCommit; con FASM, 0)
        pop ds
        mov [exp_rva], eax
        mov [exp_tam], ecx
        mov [imp_rva], edx
        mov [rel_rva], esi
        mov [rel_tam], edi
        mov [cab_tam], ebp
        clc
.x:
        ret
.no_pe_ds:
        pop ds
        mov esi, m_no_pe
        jmp falla
cierra_pe:
        mov ax, [h_exe]
        mov [rm_bx], ax
        mov word [rm_ax], 0x3E00
        jmp dos
; ---- las cabeceras y las secciones del archivo abierto a [mod_dir] ----
copia_secciones:
        xor eax, eax
        mov ecx, [cab_tam]
        mov edx, [mod_dir]
        call copia_del_exe          ; EAX posición, ECX octetos -> ES(pd):EDX
        mov ecx, [n_sec]
        mov esi, [tabla_sec]
.s:
        push ecx
        push esi
        ; (la tabla de secciones, de las cabeceras ya copiadas)
        push es
        mov es, [sel_pd]
        mov ebx, [mod_dir]
        add ebx, esi
        mov edx, [es:ebx + 12]      ; VirtualAddress
        mov ecx, [es:ebx + 16]      ; SizeOfRawData
        mov eax, [es:ebx + 20]      ; PointerToRawData
        mov edi, [es:ebx + 8]       ; VirtualSize
        pop es
        test edi, edi
        jz .s_bien
        cmp ecx, edi
        jbe .s_bien
        mov ecx, edi
.s_bien:
        add edx, [mod_dir]
        test ecx, ecx
        jz .s_sig
        test eax, eax
        jz .s_sig
        call copia_del_exe
.s_sig:
        pop esi
        pop ecx
        add esi, 40
        loop .s
        ret
; ---- la ficha EBP con lo leído de las cabeceras; ESI = el nombre (DS),
; BL = 1 si es una DLL ----
llena_ficha:
        push edi
        mov eax, [mod_dir]
        mov [ebp + M_DIR], eax
        mov eax, [img_tam]
        mov [ebp + M_TAM], eax
        mov eax, [exp_rva]
        mov [ebp + M_EXP], eax
        mov eax, [exp_tam]
        mov [ebp + M_EXPN], eax
        mov eax, [imp_rva]
        mov [ebp + M_IMP], eax
        mov eax, [entrada_rva]
        mov [ebp + M_ENT], eax
        mov [ebp + M_DLL], bl
        lea edi, [ebp + M_NOM]
        mov ecx, 31
.c:
        lodsb
        cmp al, 'a'
        jb .c1
        cmp al, 'z'
        ja .c1
        sub al, 32
.c1:
        mov [edi], al               ; (con DS: ES puede ser el programa)
        inc edi
        test al, al
        jz .x
        loop .c
        mov byte [edi], 0
.x:
        pop edi
        ret

; ---- carga la DLL de nombre b_dll (en mayúsculas): se busca en la carpeta
; del programa, en la de HW32 y en la de ahora -> EBP = su ficha ----
carga_dll:
        cmp dword [n_mods], N_MOD
        jae .muchas
        mov esi, exe_dir
        call prueba_dll
        jnc .abierta
        mov esi, hw32_dir
        call prueba_dll
        jnc .abierta
        mov esi, b_vacio
        call prueba_dll
        jnc .abierta
        cmp byte [md_modulo], 0
        je .dos
        mov dword [md_error2], b_dll
        mov esi, m_falta_dll
        jmp falla
.dos:
        mov esi, m_falta_dll
        call pon_cad
        mov esi, b_dll
        call pon_cad
        mov esi, m_nl
        call pon_cad
        mov ax, 0x4CFF
        int 0x21
.muchas:
        mov esi, m_muchas_dll
        jmp falla
.abierta:
        mov eax, [img_tam]
        add eax, 0xFFF
        and eax, ~0xFFF
        mov [img_tam], eax
        call pide_bloque
        jc .sin_mem
        sub ebx, [prog_base]
        mov [mod_dir], ebx
        push es
        mov es, [sel_pd]
        mov edi, ebx
        mov ecx, [img_tam]
        shr ecx, 2
        xor eax, eax
        rep stosd
        pop es
        call copia_secciones
        call cierra_pe
        call reubica
        ; la ficha
        mov ebp, [n_mods]
        imul ebp, MOD_TAM
        add ebp, mods
        mov esi, b_dll
        mov bl, 1
        call llena_ficha
        inc dword [n_mods]
        ; sus importaciones (puede cargar otras) y, luego, a la lista de
        ; inicio (así las de las que depende van antes)
        push ebp
        call importaciones
        pop ebp
        mov eax, [n_init]
        mov [init_lista + eax * 4], ebp
        inc dword [n_init]
        ret
.sin_mem:
        mov esi, m_sin_mem
        jmp falla
; la carpeta DS:ESI + b_dll a b_ruta, y abrirla (CF=1 si no está)
prueba_dll:                         ; (con DS: aquí ES es el programa)
        mov edi, b_ruta
.c:
        lodsb
        test al, al
        jz .n
        mov [edi], al
        inc edi
        jmp .c
.n:
        mov esi, b_dll
.d:
        lodsb
        mov [edi], al
        inc edi
        test al, al
        jnz .d
        mov esi, b_ruta
        jmp abre_pe
; ---- aplica los reubicados del módulo [mod_dir] (cargado en otro sitio que
; su ImageBase [h_base]) ----
reubica:
        mov edx, [mod_dir]
        sub edx, [h_base]           ; la diferencia
        jz .x
        mov esi, [rel_rva]
        test esi, esi
        jz .x
        push es
        mov es, [sel_pd]
        add esi, [mod_dir]
        mov ebp, esi
        add ebp, [rel_tam]          ; el final
.b:
        cmp esi, ebp
        jae .f
        mov ebx, [es:esi]           ; la página (RVA)
        mov ecx, [es:esi + 4]       ; el tamaño del bloque
        cmp ecx, 8
        jb .f
        add ebx, [mod_dir]
        lea edi, [esi + 8]
        add esi, ecx
.e:
        cmp edi, esi
        jae .b
        movzx eax, word [es:edi]
        add edi, 2
        mov ecx, eax
        shr ecx, 12
        cmp ecx, 3                  ; IMAGE_REL_BASED_HIGHLOW
        jne .e
        and eax, 0xFFF
        add [es:ebx + eax], edx
        jmp .e
.f:
        pop es
.x:
        ret

; lee TB_TAM octetos del exe desde la posición EAX al búfer: ECX = leídos
lee_en_tb_desde:
        mov [rm_dx], ax
        shr eax, 16
        mov [rm_cx], ax
        mov ax, [h_exe]
        mov [rm_bx], ax
        mov word [rm_ax], 0x4200
        call dos
        mov ax, [h_exe]
        mov [rm_bx], ax
        mov word [rm_cx], TB_TAM
        mov word [rm_dx], 0
        mov word [rm_ax], 0x3F00
        call dos
        movzx ecx, word [rm_ax]
        ret
; copia ECX octetos del exe (desde la posición EAX) al programa en EDX
copia_del_exe:
        push eax
        push ecx
        push edx
        push esi
        push edi
.t:
        test ecx, ecx
        jz .x
        push ecx
        push edx
        push eax
        call lee_en_tb_desde        ; ECX = leídos
        pop eax
        pop edx
        mov ebx, ecx
        pop ecx
        test ebx, ebx
        jz .x                       ; (el archivo es más corto: lo demás, 0)
        cmp ebx, ecx
        jbe .t1
        mov ebx, ecx
.t1:
        push ecx
        push ds
        push es
        mov es, [sel_pd]
        mov edi, edx
        mov ds, [sel_tb]
        xor esi, esi
        mov ecx, ebx
        rep movsb
        pop es
        pop ds
        pop ecx
        add eax, ebx
        add edx, ebx
        sub ecx, ebx
        jmp .t
.x:
        pop edi
        pop esi
        pop edx
        pop ecx
        pop eax
        ret

; pide a DPMI un bloque de EAX octetos: EBX = su dirección lineal; CF=1 si no
pide_bloque:
        push eax
        push ecx
        push esi
        push edi
        mov cx, ax
        shr eax, 16
        mov bx, ax
        mov ax, 0x0501
        int 0x31
        jc .x
        shl ebx, 16
        mov bx, cx
        shl esi, 16                 ; el handle del bloque (para soltarlo)
        mov si, di
        mov [ult_handle], esi
        ; se apunta (mod_fin los suelta todos); sin sitio y de módulo: no
        mov eax, [n_blq]
        cmp eax, N_BLQ
        jae .lleno
        mov [blq_tab + eax * 4], esi
        inc dword [n_blq]
        clc
        jmp .x
.lleno:
        cmp byte [md_modulo], 0
        je .x                       ; (CF=0)
        mov edi, esi
        shr esi, 16
        mov ax, 0x0502
        int 0x31
        stc
.x:
        pop edi
        pop esi
        pop ecx
        pop eax
        ret

; suelta el bloque de DPMI de handle ESI (y lo borra de los apuntados)
suelta_dpmi:
        push eax
        push ecx
        push esi
        push edi
        mov ecx, [n_blq]
.b:
        jecxz .s
        dec ecx
        cmp [blq_tab + ecx * 4], esi
        jne .b
        dec dword [n_blq]
        mov eax, [n_blq]
        mov eax, [blq_tab + eax * 4]
        mov [blq_tab + ecx * 4], eax
.s:
        mov edi, esi
        shr esi, 16
        mov ax, 0x0502
        int 0x31
        pop edi
        pop esi
        pop ecx
        pop eax
        ret

; los selectores del programa: código y datos con base prog_base y 4 GB;
; FS, la TIB (en lo nuestro del bloque)
hace_selectores:
        xor ax, ax
        mov cx, 3
        int 0x31
        jc .mal
        mov [sel_pc], ax
        add ax, 8
        mov [sel_pd], ax
        add ax, 8
        mov [sel_fs], ax
        mov ebx, [prog_base]
        mov bx, [sel_pc]
        call base_y_limite_4g
        mov bx, [sel_pd]
        call base_y_limite_4g
        mov bx, [sel_fs]
        call base_y_limite_4g
        ; derechos: código de 32 bits legible / datos de 32 bits escribibles
        mov bx, [sel_pc]
        mov cx, 0xC0FA              ; G, D, P, DPL 3, código legible
        mov ax, 0x0009
        int 0x31
        mov bx, [sel_pd]
        mov cx, 0xC0F2              ; G, B, P, DPL 3, datos escribibles
        mov ax, 0x0009
        int 0x31
        mov bx, [sel_fs]
        mov cx, 0xC0F2
        mov ax, 0x0009
        int 0x31
        ; la base de FS: la TIB, al principio de lo nuestro
        mov eax, [bloque_lin]
        add eax, [misc_off]
        mov dx, ax
        shr eax, 16
        mov cx, ax
        mov bx, [sel_fs]
        mov ax, 0x0007
        int 0x31
        ret
.mal:
        mov esi, m_no_sel32
        jmp falla
; BX = selector: base = [prog_base], límite = 4 GB
base_y_limite_4g:
        push ebx
        mov eax, [prog_base]
        mov dx, ax
        shr eax, 16
        mov cx, ax
        mov ax, 0x0007
        int 0x31
        pop ebx
        push ebx
        mov cx, 0xFFFF
        mov dx, 0xFFFF
        mov ax, 0x0008
        int 0x31
        pop ebx
        ret

; =============================================================================
;  Las importaciones: cada función, un trampolín y su ficha en imp_tab
;  (manejador 4, argumentos 2, el nombre: su dirección en el programa 4)
; =============================================================================
IMP_FICHA   equ 10
; ---- las importaciones del módulo de la ficha EBP ----
importaciones:
        mov eax, [ebp + M_IMP]
        test eax, eax
        jz .x
        push es
        mov es, [sel_pd]
        mov esi, [ebp + M_DIR]
        add esi, eax                ; el primer descriptor
.d:
        mov eax, [es:esi + 12]      ; el nombre de la DLL
        test eax, eax
        jz .fin
        add eax, [ebp + M_DIR]
        mov [imp_dll], eax
        mov ebx, [es:esi]           ; la tabla de nombres (o la IAT)
        test ebx, ebx
        jnz .ilt
        mov ebx, [es:esi + 16]
.ilt:
        add ebx, [ebp + M_DIR]
        mov edi, [es:esi + 16]      ; la IAT
        add edi, [ebp + M_DIR]
        call es_kernel32_nom        ; ES:EAX: ¿KERNEL32? (ZF=1)
        je .f
        ; (módulo) ¿una del entorno (USER32, GDI32)? Sus funciones las pone
        ; Hispa Ventanas (la que no tenga, con su «falta»)
        cmp byte [md_modulo], 0
        je .otra
        push esi
        push eax
        mov esi, eax
        mov al, 1
        call anfitrion
        test eax, eax
        pop eax
        pop esi
        jz .otra
.h:
        mov eax, [es:ebx]
        test eax, eax
        jz .d_sig
        push esi
        push edi
        xor edi, edi
        test eax, 0x80000000
        jnz .h1
        lea edi, [eax + 2]
        add edi, [ebp + M_DIR]
.h1:
        mov esi, [imp_dll]
        xor al, al
        call anfitrion
        pop edi
        pop esi
        test eax, eax
        jz .muchas
        mov [es:edi], eax
        add ebx, 4
        add edi, 4
        jmp .h
.otra:
        ; otra DLL: la suya (cargándola si hace falta) -> EDX
        push esi
        push ebx
        push edi
        call dll_de_nombre
        pop edi
        pop ebx
        pop esi
.g:
        mov eax, [es:ebx]
        test eax, eax
        jz .d_sig
        test eax, 0x80000000
        jnz .g_ord
        add eax, [ebp + M_DIR]
        add eax, 2                  ; (la pista) -> el nombre
        mov ecx, eax
        call busca_export           ; EDX ficha, ES:EAX nombre -> EAX o 0
        test eax, eax
        jnz .g_pon
        mov eax, ecx                ; no la tiene: que lo diga al llamarla
        jmp .g_falta
.g_ord:
        movzx eax, ax
        call export_numero          ; EDX ficha, EAX número -> EAX o 0
        test eax, eax
        jnz .g_pon
.g_falta:
        mov ecx, f_falta
        push edx
        xor edx, edx
        call nuevo_tramp
        pop edx
        jc .muchas
.g_pon:
        mov [es:edi], eax
        add ebx, 4
        add edi, 4
        jmp .g
        ; KERNEL32: las nuestras, por sus trampolines
.f:
        mov eax, [es:ebx]
        test eax, eax
        jz .d_sig
        test eax, 0x80000000        ; por número: no se resuelve
        jnz .ordinal
        add eax, [ebp + M_DIR]
        add eax, 2
        cmp byte [md_modulo], 0     ; (módulo) las que hace el entorno en vez
        je .f_n                     ; de nosotros (Sleep: allí cede el turno)
        push esi
        push edi
        push eax
        mov edi, eax
        mov esi, [imp_dll]
        xor al, al
        call anfitrion
        mov ecx, eax
        pop eax
        pop edi
        pop esi
        test ecx, ecx
        jz .f_n
        mov eax, ecx
        jmp .pon2
.f_n:
        call busca_funcion          ; ES:EAX nombre -> ECX manejador, DX args
        jmp .pon
.ordinal:
        xor eax, eax
        mov ecx, f_falta
        xor edx, edx
.pon:
        call nuevo_tramp            ; -> EAX el trampolín
        jc .muchas
.pon2:
        mov [es:edi], eax
        add ebx, 4
        add edi, 4
        jmp .f
.d_sig:
        add esi, 20
        jmp .d
.fin:
        pop es
.x:
        ret
.muchas:
        pop es
        mov esi, m_muchas
        jmp falla

; ¿el texto ES:EAX es «KERNEL32» (con .DLL o sin él)? ZF=1 si lo es
es_kernel32_nom:
        push esi
        push edi
        push eax
        mov esi, eax
        mov edi, s_k32
.c:
        mov al, [es:esi]
        mov ah, [edi]
        test ah, ah
        jz .f
        cmp al, 'a'
        jb .c1
        sub al, 32
.c1:
        cmp al, ah
        jne .x
        inc esi
        inc edi
        jmp .c
.f:
        test al, al
        jz .x
        cmp al, '.'
.x:
        pop eax
        pop edi
        pop esi
        ret
s_k32   db 'KERNEL32', 0
; ---- (módulo) pregunta a Hispa Ventanas. AL=0: la función EDI (su nombre;
; 0: por número) de la DLL de nombre ESI -> EAX=su trampolín (0: no es cosa
; suya). AL=1: la DLL de nombre ESI -> EAX=su mango (0: no la lleva). AL=2:
; la función EDI del mango EBX -> EAX (0: no). Todo, direcciones del programa
anfitrion:
        push edx
        mov edx, [md_ficha]
        call dword far [md_resolver]
        pop edx
        ret

; ---- la ficha de la DLL de nombre ES:EAX, cargándola si no está -> EDX ----
dll_de_nombre:
        push ebp
        push ecx
        ; el nombre, a b_dll en mayúsculas
        mov esi, eax
        mov edi, b_dll
        mov ecx, 30
.c:
        mov al, [es:esi]
        cmp al, 'a'
        jb .c1
        cmp al, 'z'
        ja .c1
        sub al, 32
.c1:
        mov [edi], al
        test al, al
        jz .c_f
        inc esi
        inc edi
        loop .c
        mov byte [edi], 0
.c_f:
        call ficha_de_b_dll
        jnc .x
        call carga_dll              ; -> EBP
        mov edx, ebp
.x:
        pop ecx
        pop ebp
        ret
; busca b_dll entre las DLL cargadas: EDX = su ficha; CF=1 si no está
ficha_de_b_dll:
        push esi
        push edi
        push ecx
        mov ecx, 1
.b:
        cmp ecx, [n_mods]
        jae .no
        mov edx, ecx
        imul edx, MOD_TAM
        add edx, mods
        lea esi, [edx + M_NOM]
        mov edi, b_dll
.c:
        mov al, [esi]
        cmp al, [edi]
        jne .sig
        test al, al
        jz .si
        inc esi
        inc edi
        jmp .c
.sig:
        inc ecx
        jmp .b
.si:
        clc
        jmp .x
.no:
        stc
.x:
        pop ecx
        pop edi
        pop esi
        ret
; ---- la exportación de nombre ES:EAX del módulo de la ficha EDX -> EAX (en
; el programa) o 0 ----
busca_export:
        push ebx
        push ecx
        push esi
        push edi
        push ebp
        mov ebp, [edx + M_EXP]
        test ebp, ebp
        jz .no
        add ebp, [edx + M_DIR]      ; el directorio de exportaciones
        mov ecx, [es:ebp + 24]      ; NumberOfNames
        mov ebx, [es:ebp + 32]
        add ebx, [edx + M_DIR]      ; los nombres
        xor esi, esi
.b:
        cmp esi, ecx
        jae .no
        mov edi, [es:ebx + esi * 4]
        add edi, [edx + M_DIR]
        push eax
        push esi
        mov esi, eax
.c:
        mov al, [es:esi]
        cmp al, [es:edi]
        jne .sig
        test al, al
        jz .si
        inc esi
        inc edi
        jmp .c
.sig:
        pop esi
        pop eax
        inc esi
        jmp .b
.si:
        pop esi
        pop eax
        mov ebx, [es:ebp + 36]
        add ebx, [edx + M_DIR]
        movzx eax, word [es:ebx + esi * 2]
        jmp export_indice
.no:
        xor eax, eax
        pop ebp
        pop edi
        pop esi
        pop ecx
        pop ebx
        ret
; la exportación de número EAX (con su Base) de la ficha EDX -> EAX o 0
export_numero:
        push ebx
        push ecx
        push esi
        push edi
        push ebp
        mov ebp, [edx + M_EXP]
        test ebp, ebp
        jz busca_export.no
        add ebp, [edx + M_DIR]
        sub eax, [es:ebp + 16]      ; - Base
        jb busca_export.no
; (sigue: EAX = el índice en las funciones; EBP = el directorio)
export_indice:
        cmp eax, [es:ebp + 20]      ; NumberOfFunctions
        jae busca_export.no
        mov ebx, [es:ebp + 28]
        add ebx, [edx + M_DIR]
        mov eax, [es:ebx + eax * 4]
        test eax, eax
        jz busca_export.no
        ; (las que reenvían a otra DLL caen dentro del directorio: no)
        cmp eax, [edx + M_EXP]
        jb .bien
        mov ecx, [edx + M_EXP]
        add ecx, [edx + M_EXPN]
        cmp eax, ecx
        jb busca_export.no
.bien:
        add eax, [edx + M_DIR]
        pop ebp
        pop edi
        pop esi
        pop ecx
        pop ebx
        ret

; ---- una función nueva: su ficha (ECX manejador, DX octetos de argumentos,
; EAX su nombre en el programa o 0) y su trampolín «push n; jmp lejos
; despacho» -> EAX = la dirección del trampolín (en el programa); ES = el
; programa. CF=1 si no caben más ----
nuevo_tramp:
        push edi
        push edx
        mov edi, [n_imp]
        cmp edi, MAX_IMP
        jae .no
        imul edi, IMP_FICHA
        add edi, imp_tab
        mov [edi], ecx
        mov [edi + 4], dx
        mov [edi + 6], eax
        mov edi, [n_imp]
        imul edi, TRAMP
        add edi, [tramp_off]
        add edi, [img_base]
        mov byte [es:edi], 0x68
        mov eax, [n_imp]
        mov [es:edi + 1], eax
        mov byte [es:edi + 5], 0xEA
        mov dword [es:edi + 6], despacho
        mov ax, [sel_cs32]
        mov [es:edi + 10], ax
        mov eax, edi
        inc dword [n_imp]
        pop edx
        pop edi
        clc
        ret
.no:
        pop edx
        pop edi
        stc
        ret

; ---- busca la función de nombre ES:EAX en la tabla: ECX = manejador, DX =
; octetos de argumentos (si no está: f_falta) ----
busca_funcion:
        push esi
        push edi
        push ebx
        mov esi, funciones
.b:
        mov ecx, [esi]
        test ecx, ecx
        jz .no
        movzx edx, word [esi + 4]
        lea ebx, [esi + 6]          ; el nombre
        mov edi, eax
.c:
        mov cl, [ebx]
        cmp cl, [es:edi]
        jne .sig
        test cl, cl
        jz .si
        inc ebx
        inc edi
        jmp .c
.sig:
        ; a la siguiente: tras el 0 del nombre
.s:
        cmp byte [ebx], 0
        je .s1
        inc ebx
        jmp .s
.s1:
        lea esi, [ebx + 1]
        jmp .b
.si:
        mov ecx, [esi]
        jmp .x
.no:
        mov ecx, f_falta
        xor edx, edx
.x:
        pop ebx
        pop edi
        pop esi
        ret

; =============================================================================
;  Ejecutar
; =============================================================================
ejecuta:
        call prepara
        ; al programa
        mov ax, [sel_pc]
        mov [salto + 4], ax
        mov ebx, [img_base]
        add ebx, [pila_off]
        add ebx, [pila_tam]
        sub ebx, 16
        mov fs, [sel_fs]
        mov ax, [sel_pd]
        mov es, ax
        mov ss, ax
        mov esp, ebx
        xor eax, eax
        xor ebx, ebx
        xor ecx, ecx
        xor edx, edx
        xor esi, esi
        xor edi, edi
        xor ebp, ebp
        push dword [salto + 4]       ; (con DS aún nuestro)
        push dword [salto]
        mov ds, [cs:sel_pd_cs]
        retf
; ---- lo del programa antes de empezar: su línea de órdenes, el entorno, la
; TIB y el trampolín de inicio ([salto]) ----
prepara:
        ; la línea de órdenes del programa, en lo nuestro (+0x1000)
        push es
        mov es, [sel_pd]
        mov edi, [img_base]
        add edi, [misc_off]
        add edi, 0x1000
        mov [cmd_dir], edi
        mov esi, nombre
.c1:
        lodsb
        test al, al
        jz .c2
        stosb
        jmp .c1
.c2:
        mov esi, [resto]
.c3:
        lodsb
        stosb
        test al, al
        jnz .c3
        ; el entorno de DOS (las cadenas «NOMBRE=valor» y un 0 más), en lo
        ; nuestro +0x2000 (hasta 8 K)
        mov edi, [img_base]
        add edi, [misc_off]
        add edi, 0x2000
        mov [env_dir], edi
        cmp word [sel_env], 0
        je .env_f
        push ds
        mov ds, [sel_env]
        xor esi, esi
        mov ecx, 0x1FF0
.env:
        lodsb
        stosb
        test al, al
        jnz .env_s
        cmp byte [esi], 0           ; ¿dos ceros? se acabó
        je .env_f0
.env_s:
        loop .env
.env_f0:
        pop ds
.env_f:
        mov word [es:edi], 0
        ; la TIB (FS:0): SEH -1, la pila, ella misma en +18h
        mov edi, [img_base]
        add edi, [misc_off]
        mov dword [es:edi], -1
        mov eax, [img_base]
        add eax, [pila_off]
        add eax, [pila_tam]
        mov [es:edi + 4], eax       ; la cima de la pila
        sub eax, [pila_tam]
        mov [es:edi + 8], eax       ; el fondo
        mov [es:edi + 0x18], edi    ; (dirección lineal de la TIB, en el programa)
        ; el trampolín de inicio (en lo nuestro +0x4000): el DllMain de cada
        ; DLL (las de las que dependen otras, antes) y luego el programa
        mov edi, [img_base]
        add edi, [misc_off]
        add edi, 0x4000
        mov [salto], edi
        xor ecx, ecx
.ini:
        cmp ecx, [n_init]
        jae .ini_f
        mov ebx, [init_lista + ecx * 4]
        inc ecx
        mov eax, [ebx + M_ENT]
        test eax, eax
        jz .ini
        mov dword [es:edi], 0x016A006A      ; push 0 ; push 1
        mov byte [es:edi + 4], 0x68         ; push hinst
        mov edx, [ebx + M_DIR]
        mov [es:edi + 5], edx
        mov byte [es:edi + 9], 0xB8         ; mov eax, DllMain
        add eax, edx
        mov [es:edi + 10], eax
        mov word [es:edi + 14], 0xD0FF      ; call eax
        add edi, 16
        jmp .ini
.ini_f:
        mov byte [es:edi], 0xB8             ; mov eax, la entrada
        mov eax, [img_base]
        add eax, [mods + M_ENT]
        mov [es:edi + 1], eax
        mov word [es:edi + 5], 0xE0FF       ; jmp eax
        pop es
        ret

; =============================================================================
;  [fase 3c] El módulo: HW32 dentro de Hispa Ventanas
; =============================================================================
; ---- (lejos, desde Hispa Ventanas) carga el programa «nombre»
; con sus DLL y lo deja listo para empezar -> EAX=0 y md_selc, md_seld,
; md_self, md_base, md_img, md_entrada y md_esp; si no, EAX=1 y md_error (y
; md_error2) con el porqué. En los dos casos hay que llamar luego a mod_fin ----
mod_cargar:
        pushad
        push ds
        push es
        push gs
        mov ds, [cs:md_ds]
        cld
        ; a nuestra pila (tras el archivo: Hispa Ventanas deja 4 KB): el
        ; cargador cuenta con que la pila y los datos son el mismo segmento
        mov [mod_ss], ss
        mov [mod_esp0], esp
        mov ss, [md_ds]
        mov esp, fin_prog + 0x1000
        mov [mod_esp], esp
        push ds                     ; (y ES, el nuestro: como al arrancar HW32)
        pop es
        mov ax, [md_cs]
        mov [sel_cs32], ax
        mov ax, [md_ds]
        mov [sel_ds], ax
        mov [sel_ss32], ax
        mov ax, [md_env]
        mov [env_seg], ax
        ; el búfer de DOS: memoria convencional, con su selector
        mov ax, 0x0100
        mov bx, TB_PARR
        int 0x31
        mov esi, m_sin_conv
        jc falla
        mov [tb_seg], ax
        mov [sel_tb], dx
        mov [segrm], ax             ; (el número de proceso)
        mov gs, dx
        mov bx, 0x40
        call sel_de_seg
        mov [sel_bios], ax
        mov bx, 0xB800
        call sel_de_seg
        mov [sel_vid], ax
        mov bx, 0xB000
        call sel_de_seg
        mov [sel_mda], ax
        mov bx, [env_seg]
        test bx, bx
        jz .sin_env
        call sel_de_seg
        mov [sel_env], ax
.sin_env:
        mov dword [resto], mod_param
        call carga_pe
        call prepara
        mov byte [en_marcha], 1
        movzx eax, word [sel_pc]
        mov [md_selc], eax
        mov ax, [sel_pd]
        mov [md_seld], eax
        mov ax, [sel_fs]
        mov [md_self], eax
        mov eax, [prog_base]
        mov [md_base], eax
        mov eax, [img_base]
        mov [md_img], eax
        add eax, [pila_off]
        add eax, [pila_tam]
        sub eax, 16
        mov [md_esp], eax
        mov eax, [salto]
        mov [md_entrada], eax
        xor eax, eax
.vuelve:
        mov ds, [cs:md_ds]
        mov ss, [mod_ss]
        mov esp, [mod_esp0]
        pop gs
        pop es
        pop ds
        mov [esp + 28], eax
        popad
        retf
; ---- (lejos, desde Hispa Ventanas) suelta todo lo del programa: cierra los
; archivos que dejó abiertos, la memoria (el programa, sus DLL, el montón),
; el búfer de DOS y los selectores ----
mod_fin:
        pushad
        push ds
        push es
        mov ds, [cs:md_ds]
        cld
        mov [mod_ss], ss
        mov [mod_esp0], esp
        mov ss, [md_ds]
        mov esp, fin_prog + 0x1000
        push ds
        pop es
        cmp word [tb_seg], 0
        je .mem
        call red_soltar             ; la red, si la dejó abierta
        xor ebx, ebx
.arch:
        bt [mod_abiertos], ebx
        jnc .arch_s
        mov [rm_bx], bx
        mov word [rm_ax], 0x3E00
        push ebx
        mov bl, 0x21
        call int_rm_real
        pop ebx
.arch_s:
        inc ebx
        cmp ebx, 256
        jb .arch
.mem:
        mov ecx, [n_blq]
.blq:
        jecxz .blq_f
        dec ecx
        mov esi, [blq_tab + ecx * 4]
        mov edi, esi
        shr esi, 16
        mov ax, 0x0502
        int 0x31
        jmp .blq
.blq_f:
        mov [n_blq], ecx
        mov dx, [sel_tb]
        test dx, dx
        jz .sel
        mov ax, 0x0101
        int 0x31
.sel:
        mov bx, [sel_pc]
        test bx, bx
        jz .x
        mov ecx, 3
.s:
        mov ax, 0x0001
        int 0x31
        add bx, 8
        loop .s
.x:
        mov ss, [mod_ss]
        mov esp, [mod_esp0]
        pop es
        pop ds
        popad
        retf

; =============================================================================
;  El despacho: [ESP] = el número de la función (del trampolín), [ESP+4] =
;  la vuelta al programa, [ESP+8...] = los argumentos
; =============================================================================
despacho:
        push ds
        push es
        push gs
        push ebp
        push ebx
        push esi
        push edi
        mov es, [cs:sel_pd_cs]
        mov ds, [cs:sel_ds_cs]
        mov gs, [cs:sel_tb_cs]      ; GS: el búfer de DOS, a mano
        cld
        lea ebp, [esp + 36]         ; los argumentos
        mov eax, [esp + 28]         ; el número
        imul eax, IMP_FICHA
        add eax, imp_tab
        mov [ficha_act], eax
        cmp byte [traza], 0
        jne .con_traza
        call [eax]                  ; EAX (y EDX): lo que devuelve
.vuelve:
        ; volver: EIP y CS del programa encima de los argumentos (RETF)
        mov ecx, [ficha_act]
        movzx ecx, word [ecx + 4]   ; los octetos de argumentos
        mov ebx, [esp + 32]         ; la vuelta
        mov [esp + 28 + ecx], ebx
        mov bx, [sel_pc]
        movzx ebx, bx
        mov [esp + 32 + ecx], ebx
        pop edi
        pop esi
        pop ebx
        pop ebp
        pop gs
        pop es
        pop ds
        add esp, ecx
        retf
.con_traza:
        call traza_antes
        mov eax, [ficha_act]
        call [eax]
        call traza_despues
        jmp .vuelve

; ---- la traza (HW32 /T), por el handle 2: «Nombre(a, b, c, d)» antes y
; « = valor» después ----
traza_antes:
        pushad
        mov edi, b_traza
        mov word [edi], '· '
        add edi, 2
        mov ebx, [ficha_act]
        mov esi, [ebx + 6]          ; el nombre (en el programa)
        test esi, esi
        jz .sin_nom
.n:
        mov al, [es:esi]
        test al, al
        jz .n_f
        mov [edi], al
        inc esi
        inc edi
        cmp edi, b_traza + 60
        jb .n
        jmp .n_f
.sin_nom:
        mov dword [edi], '(num'
        add edi, 4
.n_f:
        mov byte [edi], '('
        inc edi
        movzx ecx, word [ebx + 4]   ; los octetos de argumentos
        shr ecx, 2
        cmp ecx, 6
        jbe .a
        mov ecx, 6
.a:
        xor edx, edx
.a1:
        cmp edx, ecx
        jae .a_f
        test edx, edx
        jz .a2
        mov word [edi], ', '
        add edi, 2
.a2:
        mov eax, [ebp + edx * 4]
        call hex_a_edi
        inc edx
        jmp .a1
.a_f:
        mov byte [edi], ')'
        inc edi
        call traza_sale
        popad
        ret
traza_despues:
        pushad
        mov edi, b_traza
        mov dword [edi], ' = '
        add edi, 3
        call hex_a_edi
        mov word [edi], 0x0A0D
        add edi, 2
        call traza_sale
        popad
        ret
; EAX en hexadecimal (8 cifras) en DS:EDI
hex_a_edi:
        push ecx
        push ebx
        mov ecx, 8
.c:
        rol eax, 4
        mov bl, al
        and bl, 15
        add bl, '0'
        cmp bl, '9'
        jbe .c1
        add bl, 7
.c1:
        mov [edi], bl
        inc edi
        loop .c
        pop ebx
        pop ecx
        ret
; lo que hay en b_traza hasta EDI, por el handle 2
traza_sale:
        push dword [rm_bx]
        push dword [rm_cx]
        push dword [rm_dx]
        push dword [rm_ax]
        push dword [rm_flags]
        mov ecx, edi
        sub ecx, b_traza
        xor esi, esi
.c:
        mov al, [b_traza + esi]
        mov [gs:TB_TROZO - 256 + esi], al
        inc esi
        cmp esi, ecx
        jb .c
        mov word [rm_bx], 2
        mov [rm_cx], cx
        mov word [rm_dx], TB_TROZO - 256
        mov word [rm_ax], 0x4000
        call dos
        pop dword [rm_flags]
        pop dword [rm_ax]
        pop dword [rm_dx]
        pop dword [rm_cx]
        pop dword [rm_bx]
        ret

; ---- una función que no está: lo dice y acaba ----
f_falta:
        cmp byte [md_modulo], 0
        je .dos
        mov ebx, [ficha_act]        ; (módulo) lo dice Hispa Ventanas
        mov eax, [ebx + 6]
        xor ecx, ecx
        jmp aviso_y_fin
.dos:
        mov esi, m_falta
        call pon_cad
        mov ebx, [ficha_act]
        mov esi, [ebx + 6]          ; el nombre (en el programa)
        test esi, esi
        jz .ord
        push ds
        push es
        mov ax, es
        mov ds, ax                  ; (el nombre, del programa)
        mov es, [cs:sel_tb_cs]
        xor edi, edi
.c:
        lodsb
        stosb
        test al, al
        jnz .c
        mov word [es:edi - 1], 0x0A0D
        pop es
        pop ds
        lea ecx, [edi + 1]
        call escribe_tb
        jmp .fin
.ord:
        mov esi, m_ordinal
        call pon_cad
.fin:
        call red_soltar
        mov ax, 0x4CFF
        int 0x21

%include "utils/hw32k.inc"

m_uso       db "Hispa-486: ejecuta programas Win32 de consola.", 13, 10
            db "  HW32 [/T] PROGRAMA.EXE [parametros]", 13, 10
            db "  /T  traza: cada llamada a KERNEL32 (por la salida de errores)", 13, 10
            db "Necesita un 386 o superior, HSPXMS y HSPDPMI.", 13, 10, 0
m_no_esta   db "HW32: no existe el programa.", 13, 10, 0
m_no_pe     db "HW32: no es un programa Win32 (PE de 32 bits).", 13, 10, 0
m_sin_mem   db "HW32: no hay memoria bastante.", 13, 10, 0
m_no_sel32  db "HW32: DPMI no da los selectores.", 13, 10, 0
m_muchas    db "HW32: el programa importa demasiadas funciones.", 13, 10, 0
m_muchas_dll db "HW32: demasiadas DLL.", 13, 10, 0
m_falta_dll db "Hispa-486: falta la DLL ", 0
m_nl        db 13, 10, 0
m_falta     db "Hispa-486: falta la funcion ", 0
m_ordinal   db "(por numero)", 13, 10, 0
m_sin_conv  db "No hay memoria convencional para el programa de Windows.", 0
m_no_dpmi   db "HW32: hace falta HSPDPMI (cliente DPMI de 32 bits).", 13, 10, "$"
m_no_mem    db "HW32: no hay memoria convencional bastante.", 13, 10, "$"
m_no_sel    db "HW32: DPMI no da los selectores.", 13, 10, "$"

; los selectores, también donde los ve CS (el despacho entra con DS del
; programa: los lee por CS)
sel_pd_cs   equ sel_pd
sel_ds_cs   equ sel_ds
sel_tb_cs   equ sel_tb

; =============================================================================
;  Datos
; =============================================================================
        align 4
segrm       dw 0
tb_seg      dw 0
entrada     dd 0
sel_psp     dw 0
sel_ds      dw 0
sel_cs32    dw 0
sel_ss32    dw 0
sel_tb      dw 0
sel_pc      dw 0
sel_pd      dw 0
sel_fs      dw 0
sel_bios    dw 0
sel_vid     dw 0
sel_mda     dw 0
sel_env     dw 0
env_seg     dw 0
h_exe       dw 0
        align 4
pe_cab      dd 0
n_sec       dd 0
tabla_sec   dd 0
entrada_rva dd 0
img_base    dd 0
img_tam     dd 0
cab_tam     dd 0
imp_rva     dd 0
pila_tam    dd 0
tramp_off   dd 0
misc_off    dd 0
pila_off    dd 0
bloque_tam  dd 0
bloque_lin  dd 0
prog_base   dd 0
n_imp       dd 0
dll_nom     dd 0
resto       dd 0
cmd_dir     dd 0
ficha_act   dd 0
ult_error   dd 0
ult_handle  dd 0
traza       db 0
b_traza     times 160 db 0
h_base      dd 0
pila_res    dd 0
exp_rva     dd 0
exp_tam     dd 0
rel_rva     dd 0
rel_tam     dd 0
mod_dir     dd 0
n_mods      dd 0
n_init      dd 0
init_lista  times N_MOD dd 0
mods        times N_MOD * MOD_TAM db 0
b_dll       times 32 db 0
b_ruta      times 160 db 0
b_vacio     db 0
exe_dir     times 130 db 0
hw32_dir    times 130 db 0
env_dir     dd 0
monton_ptr  dd 0
monton_libre dd 0
salto       dd 0, 0
desc        times 8 db 0
; [fase 3c] el módulo (HW32 dentro de Hispa Ventanas)
N_BLQ       equ 256
n_blq       dd 0                    ; los bloques de DPMI pedidos
blq_tab     times N_BLQ dd 0
imp_dll     dd 0                    ; el nombre de la DLL que se está importando
mod_esp     dd 0
mod_esp0    dd 0                    ; la pila de Hispa Ventanas mientras se carga
mod_ss      dd 0
mod_abiertos times 32 db 0          ; los archivos de DOS abiertos (un bit cada uno)
mod_ah      db 0
en_marcha   db 0                    ; el programa ya corre
        align 4
mod_d:
md_modulo   dd 0                    ; +0  1: somos el módulo de Hispa Ventanas
md_cs       dd 0                    ; +4  nuestros selectores (código y datos)
md_ds       dd 0                    ; +8
md_env      dd 0                    ; +12 el segmento (de modo real) del entorno
md_psp      dd 0                    ; +16 el del PSP de Hispa Ventanas (su DTA)
md_ficha    dd 0                    ; +20 la ficha del programa en Hispa Ventanas
md_resolver dd 0, 0                 ; +24 (lejos) sus funciones: ver «anfitrion»
md_salir    dd 0, 0                 ; +32 (lejos) el programa acaba: EAX=código
md_falta    dd 0, 0                 ; +40 (lejos) un aviso y acaba: ver «aviso_y_fin»
md_selc     dd 0                    ; +48 lo que devuelve mod_cargar: los
md_seld     dd 0                    ; +52   selectores del programa,
md_self     dd 0                    ; +56
md_base     dd 0                    ; +60   su base (lineal),
md_img      dd 0                    ; +64   su ImageBase,
md_entrada  dd 0                    ; +68   donde empieza (sus DllMain y él)
md_esp      dd 0                    ; +72   y su pila
md_error    dd 0                    ; +76 el porqué (texto nuestro) si no carga
md_error2   dd 0                    ; +80   y su segunda parte (0: no hay)
mod_param   times 130 db 0
; la estructura de registros de 0300h
rmr:
rm_edi      dd 0
rm_esi      dd 0
rm_ebp      dd 0
            dd 0
rm_ebx:
rm_bx       dd 0
rm_edx:
rm_dx       dd 0
rm_ecx:
rm_cx       dd 0
rm_eax:
rm_ax       dd 0
rm_flags    dw 0
rm_es       dw 0
rm_ds       dw 0
rm_fs       dw 0
rm_gs       dw 0
rm_ip       dw 0
rm_cs       dw 0
rm_ss_sp    dd 0
orden       times 130 db 0
nombre      times 130 db 0
imp_tab     times MAX_IMP * IMP_FICHA db 0
            times 512 db 0
pila16_cima:
fin_prog:
