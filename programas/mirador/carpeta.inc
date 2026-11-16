; =============================================================================
;  MIRADOR.HVP - las imágenes de la carpeta: para pasar a la anterior y a la
;  siguiente, y para saber la fecha de cada una
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  La interfaz de siempre no tiene «buscar archivos»: se hace con las
;  funciones 4Eh y 4Fh de DOS por rm_interrup, con el patrón y la zona de
;  transferencia en dos_buf (como el Administrador de archivos). De la
;  carpeta se toman los archivos con extensión de imagen (y el que se está
;  viendo, se llame como se llame), hasta LST_MAX, y se ordenan por nombre.
; =============================================================================
DB_DTA      equ 256                 ; dentro de dos_buf: la zona de transferencia
DTA_ATR     equ 0x15
DTA_HORA    equ 0x16
DTA_FECHA   equ 0x18
DTA_TAMANO  equ 0x1A
DTA_NOMBRE  equ 0x1E

%macro CARPETA_BSS 0
            alignb 4
lst_n       resd 1                  ; cuántas hay
lst_i       resd 1                  ; cuál es la que se ve
lst_fuerza  resd 1                  ; 1: hay que leer la carpeta otra vez (se ha guardado algo)
dir_ruta    resb 132                ; la carpeta, acabada en «\»
lst_tmp     resb LST_TAM
lst         resb LST_MAX * LST_TAM
%endmacro

; las extensiones que se toman por imágenes
ext_img     db "BMP", 0, "DIB", 0, "RLE", 0, "PCX", 0, "GIF", 0, "PNG", 0, "TGA", 0
            db "ICO", 0, "CUR", 0, "PBM", 0, "PGM", 0, "PPM", 0, "PNM", 0, "JPG", 0
            db "JPE", 0, "JFI", 0
            dd 0
t_todos     db "*.*", 0

; (interna) INT 21h con el bloque rl; CF=1 si DOS dice que no
c_int21:
        push esi
        mov byte [rl+RL_INT], 0x21
        mov dword [rl+RL_BAND], 0
        mov esi, rl
        HV rm_interrup
        pop esi
        test byte [rl+RL_BAND], 1
        jz .bien
        stc
        ret
.bien:
        clc
        ret

; con «nombre» puesto: su carpeta, la lista de sus imágenes y cuál es esta
lista_carpeta:
        pushad
        ; dónde acaba la carpeta: detrás de la última barra
        mov esi, nombre
        mov ebx, esi
.c:
        lodsb
        test al, al
        jz .c1
        cmp al, '\'
        je .c0
        cmp al, ':'
        jne .c
.c0:
        mov ebx, esi
        jmp .c
.c1:
        mov [nom_corto], ebx
        mov ecx, ebx
        sub ecx, nombre             ; ECX=letras de la carpeta
        ; si es la misma carpeta de antes y el archivo está en la lista, vale
        ; la que hay (leer la carpeta de un disquete cada vez se nota)
        cmp byte [lst_fuerza], 0
        jne .lista
        cmp dword [lst_n], 0
        je .lista
        cmp byte [dir_ruta+ecx], 0
        jne .lista
        test ecx, ecx
        jz .misma
        push ecx
        mov esi, nombre
        mov edi, dir_ruta
        repe cmpsb
        pop ecx
        jne .lista
.misma:
        call .busca
        jnc .x
.lista:
        call mini_suelta            ; (las miniaturas eran de la lista de antes)
        mov byte [lst_fuerza], 0
        mov esi, nombre
        mov edi, dir_ruta
        rep movsb
        mov byte [edi], 0
        mov dword [lst_n], 0
        mov dword [lst_i], 0
        ; el patrón, a dos_buf
        mov ebp, [hv_api + HV_dos_buf * 4]
        mov edi, ebp
        mov esi, dir_ruta
        call hvl_cad
        mov esi, t_todos
        call hvl_cad
        mov byte [edi], 0
        mov edi, rl
        mov ecx, RL_TAM / 4
        xor eax, eax
        rep stosd
        mov dword [rl+RL_EAX], 0x1A00   ; la zona de transferencia
        lea eax, [ebp+DB_DTA]
        mov [rl+RL_EDX], eax
        call c_int21
        mov dword [rl+RL_EAX], 0x4E00   ; el primero (también los ocultos y de solo lectura)
        mov dword [rl+RL_ECX], 0x27
        mov [rl+RL_EDX], ebp
        call c_int21
        jc .listo
.uno:
        lea esi, [ebp+DB_DTA]
        test byte [esi+DTA_ATR], 0x18   ; ni carpetas ni la etiqueta del disco
        jnz .sig
        call .vale
        jc .sig
        mov eax, [lst_n]
        cmp eax, LST_MAX
        jae .listo
        imul eax, LST_TAM
        lea edi, [lst+eax]
        push esi
        add esi, DTA_NOMBRE
        mov ecx, 13
        rep movsb
        pop esi
        mov byte [edi-1], 0
        mov ax, [esi+DTA_HORA]
        mov [edi-13+L_HORA], ax
        mov ax, [esi+DTA_FECHA]
        mov [edi-13+L_FECHA], ax
        mov eax, [esi+DTA_TAMANO]
        mov [edi-13+L_TAMANO], eax
        inc dword [lst_n]
.sig:
        mov dword [rl+RL_EAX], 0x4F00
        call c_int21
        jnc .uno
.listo:
        call .ordena
        call .busca
.x:
        popad
        ret
; (interna) ¿cuál de la lista es la que se ve? La pone en lst_i; CF=1 si no está
.busca:
        xor ebx, ebx
.b:
        cmp ebx, [lst_n]
        jae .b_no
        mov eax, ebx
        imul eax, LST_TAM
        lea esi, [lst+eax]
        mov edi, [nom_corto]
        call .igual
        je .es
        inc ebx
        jmp .b
.es:
        mov [lst_i], ebx
        clc
        ret
.b_no:
        stc
        ret
; (interna) ESI=la entrada de DOS: CF=0 si es una imagen (por la extensión)
; o el archivo que se está viendo
.vale:
        push esi
        push edi
        add esi, DTA_NOMBRE
        mov edi, [nom_corto]
        push esi
        call .igual
        pop esi
        je .v_si
.v1:
        lodsb
        test al, al
        jz .v_no
        cmp al, '.'
        jne .v1
        mov eax, [esi]              ; la extensión y su cero (o tres letras y el cero)
        mov edi, ext_img
.v2:
        cmp dword [edi], 0
        je .v_no
        cmp eax, [edi]
        je .v_si
        add edi, 4
        jmp .v2
.v_si:
        pop edi
        pop esi
        clc
        ret
.v_no:
        pop edi
        pop esi
        stc
        ret
; (interna) ¿son iguales los nombres ESI y EDI (acabados en 0)? -> ZF
.igual:
        mov al, [esi]
        cmp al, [edi]
        jne .i9
        inc esi
        inc edi
        test al, al
        jnz .igual
.i9:
        ret
; (interna) ordena la lista por nombre (por inserción: son pocas)
.ordena:
        mov ebx, 1
.o1:
        cmp ebx, [lst_n]
        jae .o9
        mov eax, ebx
        imul eax, LST_TAM
        lea esi, [lst+eax]
        mov edi, lst_tmp
        mov ecx, LST_TAM
        rep movsb
        mov edx, ebx                ; EDX=el hueco
.o2:
        test edx, edx
        jz .o4
        lea eax, [edx-1]
        imul eax, LST_TAM
        lea esi, [lst+eax]          ; ¿el de antes va detrás del que se coloca?
        mov edi, lst_tmp
        mov ecx, 13
        repe cmpsb
        jbe .o4
        lea esi, [lst+eax]
        lea edi, [esi+LST_TAM]
        mov ecx, LST_TAM
        rep movsb
        dec edx
        jmp .o2
.o4:
        mov eax, edx
        imul eax, LST_TAM
        lea edi, [lst+eax]
        mov esi, lst_tmp
        mov ecx, LST_TAM
        rep movsb
        inc ebx
        jmp .o1
.o9:
        ret

; ECX=O_ANT, O_SIG, O_PRIMERA u O_ULTIMA: a esa imagen de la carpeta (la
; siguiente de la última es la primera)
pasa:
        pushad
        mov eax, [lst_n]
        test eax, eax
        jz .x
        mov ebx, [lst_i]
        cmp ecx, O_SIG
        jne .p1
        inc ebx
        cmp ebx, eax
        jb .ir
        xor ebx, ebx
        jmp .ir
.p1:
        cmp ecx, O_ANT
        jne .p2
        dec ebx
        jns .ir
        lea ebx, [eax-1]
        jmp .ir
.p2:
        xor ebx, ebx
        cmp ecx, O_PRIMERA
        je .ir
        lea ebx, [eax-1]
.ir:
        cmp eax, 1                  ; (si no hay otra, no se lee otra vez)
        jne .ir1
        cmp byte [hay_img], 0
        jne .x
.ir1:
        cmp ebx, [lst_i]
        jne .ir2
        cmp byte [hay_img], 0
        jne .x
.ir2:
        mov edi, nom_tmp
        mov esi, dir_ruta
        call hvl_cad
        imul ebx, LST_TAM
        lea esi, [lst+ebx]
        call hvl_cad
        mov byte [edi], 0
        call abre_archivo
.x:
        popad
        ret
