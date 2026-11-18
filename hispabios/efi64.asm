; =============================================================================
;  HispaBIOS (x64) - parte UEFI
; =============================================================================
;  Lo que hace, por orden (cada paso se enseña en pantalla y se apunta en el
;  registro; si algo falla se dice qué falta y qué hacer, y el registro se
;  guarda en la memoria USB como \HISPABIOS.LOG):
;    1. quita el vigilante del firmware (reinicia el equipo a los 5 minutos);
;    2. lee el mapa de memoria (GetMemoryMap, con el búfer del tamaño que el
;       firmware pida) y enseña lo que hay por debajo de 1 MB;
;    3. pide al firmware, página a página, la memoria baja (0 a 9FFFFh): la
;       que da es la que DOS puede usar;
;    4. elige dónde poner la BIOS de modo real (bios64.asm): 60 KB seguidos
;       en 90000h o, si el firmware tiene algo ahí, en 70000h o 60000h
;       (hay una copia de la BIOS ensamblada para cada sitio);
;    5. busca la pantalla (GOP) y los discos (Block I/O), copia la BIOS y la
;       arranca. Después atiende sus peticiones (despachador).
;
;  Convención de llamadas de UEFI en x64 (la de Microsoft): argumentos en RCX,
;  RDX, R8, R9 y la pila; 32 octetos de hueco por encima de la dirección de
;  vuelta; LA PILA ALINEADA A 16 OCTETOS EN CADA LLAMADA (el firmware de los
;  equipos actuales usa SSE con datos alineados en la pila: con la pila mal,
;  se cuelga; OVMF no, y por eso no se veía en QEMU); se conservan RBX, RBP,
;  RSI, RDI y R12 a R15. Todas las llamadas al firmware van con la macro FW,
;  que en las pruebas (-DPRUEBA_PILA) comprueba la alineación.
;
;  Opciones de ensamblado para las pruebas (tests/run_hispabios_tests.sh):
;    -DPRUEBA_PILA             avisa por COM1 («PILA-MAL») de una llamada al
;                              firmware con la pila sin alinear
;    -DPRUEBA_OCUPA=dir,n,...  ocupa antes esas páginas (el firmware no las
;                              dará: como un equipo con la memoria baja usada)
;    -DPRUEBA_SIN_GOP          como si no hubiera pantalla gráfica
;    -DREGISTRO_PASOS          guarda HISPABIOS.LOG en cada paso aunque todo
;                              vaya bien (la imagen de prueba para un equipo
;                              que se queda parado: tools/mkprueba_hispabios.sh)
;
;  Referencias: especificación UEFI 2.x (2.3.4, 4, 7, 12, 13).
; =============================================================================
%define PE_ARQ 64
%include "pe.inc"

%define HB_VERSION '0.2'

; ---- tablas de UEFI (x64) ----
ST_VENDOR       equ 24          ; FirmwareVendor (CHAR16 *)
ST_FWREV        equ 32          ; FirmwareRevision
ST_CONOUT       equ 64
ST_BOOTSERV     equ 96
TXT_OUTSTR      equ 8
BS_ALLOCPAGES   equ 40
BS_GETMEMMAP    equ 56
BS_ALLOCPOOL    equ 64
BS_FREEPOOL     equ 72
BS_STALL        equ 248
BS_WATCHDOG     equ 256
BS_LOCATEPROT   equ 320
BS_HANDLEPROT   equ 152
BS_LOCATEHBUF   equ 312
ST_CONIN        equ 48
ST_CONINH       equ 40           ; ConsoleInHandle (para SimpleTextInputEx)
ST_RUNTIME      equ 88
RT_RESET        equ 104
FS_OPENVOL      equ 8           ; EFI_SIMPLE_FILE_SYSTEM.OpenVolume
F_OPEN          equ 8           ; EFI_FILE: Open, Close, Delete, Read, Write
F_CLOSE         equ 16
F_DELETE        equ 24
F_WRITE         equ 40

ALLOC_ADDRESS   equ 2           ; AllocateAddress
MEM_RESERVADA   equ 0           ; EfiReservedMemoryType
MEM_LOADERDATA  equ 2           ; EfiLoaderData
EFI_CHICO       equ 8000000000000005h   ; EFI_BUFFER_TOO_SMALL
BIOS_TAM        equ 0F000h      ; la BIOS de modo real y sus zonas (bios64.asm)
BIOS_PAGS       equ BIOS_TAM >> 12
TX              equ 8000h       ; su pantalla de texto (80x25x2)
PASO            equ 0C000h      ; su búfer de paso para el disco
NPAG            equ 160         ; páginas de 0 a 9FFFFh
PAG_ARRANQUE    equ 80h         ; la página del sector de arranque de Hispa-DOS (80000h)
MEM_MIN_KB      equ 256         ; con menos memoria convencional no se arranca
REG_MAX         equ 60000       ; octetos del registro (HISPABIOS.LOG)
PANT_MAX        equ 14          ; líneas del mapa que salen en pantalla

; FW: una llamada al firmware (la pila tiene que estar alineada a 16)
%macro FW 1
%ifdef PRUEBA_PILA
        test spl, 0Fh
        jz %%bien
        call pila_mal
%%bien:
%endif
        call %1
%endmacro

; DI: «escribir» un texto
%macro DI 1
        lea rsi, [rel %1]
        call escribir
%endmacro

pe_entrada:
        push rbx
        push rbp
        push rsi
        push rdi
        push r12
        push r13
        push r14
        push r15
        sub rsp, 56                     ; 32 de la convención + 3 argumentos
                                        ; (8 + 64 + 56 = 128: pila alineada)
        mov [rel imagen], rcx
        mov [rel sistema], rdx
        mov rax, [rdx + ST_CONOUT]
        mov [rel conout], rax
        mov rax, [rdx + ST_BOOTSERV]
        mov [rel bootserv], rax
        cld

        ; el vigilante del firmware reinicia el equipo a los 5 minutos de
        ; arrancar un cargador: fuera
        xor ecx, ecx
        xor edx, edx
        xor r8d, r8d
        xor r9d, r9d
        FW [rax + BS_WATCHDOG]

        ; la memoria del registro
        mov rax, [rel bootserv]
        mov ecx, MEM_LOADERDATA
        mov edx, REG_MAX
        lea r8, [rel reg_dir]
        FW [rax + BS_ALLOCPOOL]
        test rax, rax
        jz .con_registro
        mov qword [rel reg_dir], 0
.con_registro:

        DI t_titulo
        call anotar_firmware
        call registro_paso

%ifdef PRUEBA_OCUPA
        ; (pruebas) páginas ocupadas de antemano, como en un equipo que las usa
        lea rbx, [rel prueba_ocupa]
.ocupa: mov rax, [rbx]
        cmp rax, -1
        je .ocupadas
        mov [rel dir_pag], rax
        mov rax, [rel bootserv]
        mov ecx, ALLOC_ADDRESS
        mov edx, MEM_RESERVADA
        mov r8, [rbx + 8]
        lea r9, [rel dir_pag]
        FW [rax + BS_ALLOCPAGES]
        add rbx, 16
        jmp .ocupa
.ocupadas:
%endif

        ; ---- el mapa de memoria ----
        mov byte [rel callado], 1
        DI t_p_mapa
        mov byte [rel callado], 0
        call mapa_leer
        jnc .mapa_bien
        DI t_mapa_mal
        call hex64
        DI t_nl
        jmp .fallo
.mapa_bien:
        mov rax, [rel desc_tam]
        cmp rax, 40                     ; (EFI_MEMORY_DESCRIPTOR: 40 octetos o más)
        jb .desc_mal
        cmp rax, 4096
        jbe .desc_bien
.desc_mal:
        DI t_desc_mal
        mov rax, [rel desc_tam]
        call decimal
        DI t_nl
        jmp .fallo
.desc_bien:
        call mapa_ensenar
        call registro_paso

        ; ---- pide las páginas bajas una a una (de 0 a 9F000h) ----
        DI t_pide
        xor r13d, r13d                  ; R13 = número de página
        xor r12d, r12d                  ; R12 = cuántas se han dado
        lea rbx, [rel dada]
.pagina:
        mov rax, r13
        shl rax, 12
        mov [rel dir_pag], rax
        mov rax, [rel bootserv]
        mov ecx, ALLOC_ADDRESS
        mov edx, MEM_LOADERDATA
        mov r8d, 1
        lea r9, [rel dir_pag]
        FW [rax + BS_ALLOCPAGES]
        test rax, rax
        setz byte [rbx + r13]
        jnz .otra
        inc r12
.otra:  inc r13
        cmp r13, NPAG
        jb .pagina
        call paginas_ensenar            ; las que no da, por tramos
        DI t_dadas
        mov rax, r12
        call decimal
        DI t_de160

        ; ---- dónde va la BIOS de modo real: 60 KB seguidos que haya dado ----
        lea rbx, [rel bases]
.base:  mov eax, [rbx]
        test eax, eax
        jz .sin_base
        shr eax, 12
        lea rsi, [rel dada]
        add rsi, rax
        mov ecx, BIOS_PAGS
.bp:    cmp byte [rsi], 0
        je .otra_base
        inc rsi
        dec ecx
        jnz .bp
        jmp .hay_base
.otra_base:
        add rbx, 12
        jmp .base
.sin_arranque:                          ; (la página que usa el sector de
        DI t_sin_arranque               ; arranque de Hispa-DOS no está libre)
.sin_base:
        DI t_sin_base
        jmp .fallo
.hay_base:
        cmp byte [rel dada + PAG_ARRANQUE], 0
        je .sin_arranque
        mov r15d, [rbx]                 ; R15 = base de la BIOS
        mov [rel bios_base], r15
        mov [rel bios_cual], rbx
        ; la memoria convencional de DOS: hasta la primera página (de la 1 en
        ; adelante) que el firmware no ha dado, o hasta la BIOS
        lea rsi, [rel dada]
        mov eax, 1
.tope:  cmp byte [rsi + rax], 0
        je .tope_ya
        inc eax
        cmp eax, NPAG
        jb .tope
.tope_ya:
        shl eax, 12
        cmp rax, r15
        jbe .tope_bien
        mov rax, r15
.tope_bien:
        mov [rel bajo_tope], rax
        DI t_base
        mov rax, r15
        mov ecx, 5
        call hex
        DI t_base2
        mov rax, [rel bajo_tope]
        shr rax, 10
        call decimal
        DI t_kb
        mov rax, [rel bajo_tope]
        cmp rax, MEM_MIN_KB * 1024
        jae .mem_bien
        DI t_poca
        jmp .fallo
.mem_bien:
        call registro_paso

        ; ---- la pantalla y la BIOS de modo real ----
        call gop_buscar
        setc r12b                       ; R12B = 1: no hay pantalla que valga
        mov byte [rel callado], 1       ; (solo al registro)
        DI t_gop
        mov eax, [rel fb_ancho]
        call decimal
        DI t_por
        mov eax, [rel fb_alto]
        call decimal
        DI t_gop_f
        mov eax, [rel fb_formato]
        call decimal
        DI t_gop_l
        mov eax, [rel fb_linea]
        call decimal
        DI t_gop_d
        mov rax, [rel fb]
        call hex64
        DI t_nl
        mov byte [rel callado], 0
        test r12b, r12b
        jnz .sin_gop
        mov rdi, r15                    ; la zona de la BIOS, a cero
        mov ecx, BIOS_TAM / 8
        xor eax, eax
        cld
        rep stosq
        mov rbx, [rel bios_cual]        ; la BIOS (la ensamblada para esa
        mov eax, [rbx + 4]              ; base), a su sitio
        lea rsi, [rel pe_inicio]
        add rsi, rax
        mov rdi, r15
        mov ecx, [rbx + 8]
        rep movsb
        lea rax, [rel despachador]
        mov [r15 + 8], rax              ; T_DESPACHADOR
        mov rax, [rel bajo_tope]        ; T_MEMKB: hasta la primera página ocupada
        shr eax, 10
        mov [r15 + 28], ax
        call memoria_ext                ; T_EXTKB
        mov [r15 + 32], eax
        mov r12d, eax
        call tsc_calibrar               ; T_TSC_TICK
        mov [r15 + 36], eax
        mov r13d, eax
        call vector_irq0                ; T_VEC0
        mov [r15 + 182], al
        movzx r14d, al
        call discos_buscar              ; T_NDISCOS y T_DISCOS
        mov [r15 + 30], al
        movzx ebx, al
        mov byte [rel callado], 1       ; (solo al registro)
        DI t_r_ext
        mov eax, r12d
        call decimal
        DI t_r_tsc
        mov eax, r13d
        call decimal
        DI t_r_vec
        mov eax, r14d
        mov ecx, 2
        call hex
        DI t_r_discos
        mov eax, ebx
        call decimal
        DI t_nl
        mov byte [rel callado], 0
        test ebx, ebx
        jnz .hay_discos
        DI t_sin_discos
        jmp .fallo
.hay_discos:
        DI t_viaje
        call registro_paso
        mov rax, r15
        call rax                        ; (no vuelve mientras DOS funcione)
        DI t_vuelta
        jmp .fallo
.sin_gop:
        DI t_sin_gop

        ; ---- algo ha fallado: el registro a la memoria USB y a esperar ----
.fallo:
        call registro_guardar
        jc .sin_registro
        DI t_guardado
        jmp .espera
.sin_registro:
        DI t_no_guardado
.espera:
        DI t_tecla
        call tecla_esperar
        xor eax, eax
        add rsp, 56
        pop r15
        pop r14
        pop r13
        pop r12
        pop rdi
        pop rsi
        pop rbp
        pop rbx
        ret

; ---- mapa_leer: GetMemoryMap con el búfer que haga falta -> mapa_dir, mapa_tam,
;      desc_tam; CF y RAX = código del firmware si no se puede ----
; (El mapa de un equipo actual tiene cientos de entradas y pedir memoria para
; el búfer puede añadir alguna: se pide de más y se repite si aun así no cabe.)
mapa_leer:
        push rbx
        push r12
        sub rsp, 56                     ; (8 + 16 + 56 = 80: alineada)
        mov ebx, 5                      ; intentos
        lea rax, [rel mapa_min]         ; la primera vez, solo el tamaño
        mov [rel mapa_dir], rax
        mov qword [rel mapa_cabe], 0
.pide:  mov rax, [rel mapa_cabe]
        mov [rel mapa_tam], rax
        mov rax, [rel bootserv]
        lea rcx, [rel mapa_tam]
        mov rdx, [rel mapa_dir]
        lea r8, [rel mapa_llave]
        lea r9, [rel desc_tam]
        lea r10, [rel desc_ver]
        mov [rsp + 32], r10
        FW [rax + BS_GETMEMMAP]
        test rax, rax
        jz .bien
        mov r12, EFI_CHICO
        cmp rax, r12
        jne .mal
        dec ebx
        jz .mal
        cmp qword [rel mapa_cabe], 0    ; el búfer anterior, fuera
        je .nuevo
        mov rax, [rel bootserv]
        mov rcx, [rel mapa_dir]
        FW [rax + BS_FREEPOOL]
.nuevo: mov rdx, [rel mapa_tam]         ; lo que pide y 16 entradas de más
        add rdx, 16 * 64
        mov [rel mapa_cabe], rdx
        mov rax, [rel bootserv]
        mov ecx, MEM_LOADERDATA
        lea r8, [rel mapa_dir]
        FW [rax + BS_ALLOCPOOL]
        test rax, rax
        jz .pide
.mal:   stc
        jmp .fin
.bien:  clc
.fin:   lea rsp, [rsp + 56]
        pop r12
        pop rbx
        ret

; ---- mapa_ensenar: el mapa entero al registro; lo de debajo de 1 MB, además,
;      a la pantalla (las primeras PANT_MAX líneas) ----
mapa_ensenar:
        push rbx
        push r12
        push r13
        push r14
        push r15
        mov byte [rel callado], 1
        DI t_r_mapa
        mov rax, [rel mapa_tam]
        xor edx, edx
        div qword [rel desc_tam]
        call decimal
        DI t_r_mapa2
        mov rax, [rel desc_tam]
        call decimal
        DI t_r_mapa3
        mov byte [rel callado], 0
        DI t_mapa
        mov rbx, [rel mapa_dir]
        mov r12, [rel mapa_tam]
        add r12, rbx                    ; R12 = final del mapa
        xor r13d, r13d                  ; R13 = líneas en pantalla
        xor r14d, r14d                  ; R14 = páginas de memoria en total
        xor r15d, r15d                  ; R15 = líneas que no han cabido
.entrada:
        lea rax, [rbx + 40]
        cmp rax, r12
        ja .fin_mapa
        mov eax, [rbx]                  ; memoria de verdad (no entrada/salida
        cmp eax, 11                     ; ni reservada): para el total
        je .sin_sumar
        cmp eax, 12
        je .sin_sumar
        test eax, eax
        jz .sin_sumar
        add r14, [rbx + 24]
.sin_sumar:
        mov byte [rel callado], 1
        cmp qword [rbx + 8], 100000h    ; PhysicalStart
        jae .linea
        cmp r13d, PANT_MAX
        jb .visible
        inc r15d
        jmp .linea
.visible:
        inc r13d
        mov byte [rel callado], 0
.linea: DI t_sangria
        mov rax, [rbx + 8]
        mov ecx, 12
        call hex
        DI t_paginas
        mov rax, [rbx + 24]             ; NumberOfPages
        mov ecx, 9
        call hex
        DI t_tipo
        mov eax, [rbx]                  ; Type
        mov ecx, 8
        cmp eax, 100h
        jae .tipo_largo
        mov ecx, 2
.tipo_largo:
        call hex
        DI t_espacio
        mov eax, [rbx]
        lea rsi, [rel t_tipo_otro]
        cmp eax, 15
        ja .nombre
        lea rsi, [rel t_tipos]
        imul eax, eax, 12
        add rsi, rax
.nombre:
        call escribir
        mov al, [rel callado]           ; los atributos, solo al registro
        push rax
        mov byte [rel callado], 1
        DI t_atr
        mov rax, [rbx + 32]
        call hex64
        pop rax
        mov [rel callado], al
        DI t_nl
        add rbx, [rel desc_tam]
        jmp .entrada
.fin_mapa:
        mov byte [rel callado], 0
        test r15d, r15d
        jz .total
        DI t_mas
        mov eax, r15d
        call decimal
        DI t_mas2
.total: DI t_total
        mov rax, r14
        shr rax, 8                      ; páginas -> MB
        call decimal
        DI t_mb
        pop r15
        pop r14
        pop r13
        pop r12
        pop rbx
        ret

; ---- paginas_ensenar: los tramos de páginas bajas que el firmware no ha dado ----
paginas_ensenar:
        push rbx
        push r12
        push r13
        lea rbx, [rel dada]
        xor r12d, r12d                  ; R12 = página
.busca: cmp r12d, NPAG
        jae .fin
        cmp byte [rbx + r12], 0
        je .tramo
        inc r12d
        jmp .busca
.tramo: mov r13d, r12d                  ; R13 = primera del tramo
.sigue: inc r12d
        cmp r12d, NPAG
        jae .tramo_fin
        cmp byte [rbx + r12], 0
        je .sigue
.tramo_fin:
        DI t_no_dada
        mov eax, r13d
        shl eax, 12
        mov ecx, 5
        call hex
        DI t_a
        mov eax, r12d
        shl eax, 12
        dec eax
        mov ecx, 5
        call hex
        DI t_parentesis
        mov eax, r12d
        sub eax, r13d
        call decimal
        DI t_pag_fin
        jmp .busca
.fin:   pop r13
        pop r12
        pop rbx
        ret

; ---- anotar_firmware: quién ha hecho el firmware y su versión, al registro ----
anotar_firmware:
        push rbx
        push rsi
        push rdi
        mov byte [rel callado], 1
        DI t_r_fab
        mov rbx, [rel sistema]
        mov rsi, [rbx + ST_VENDOR]
        lea rdi, [rel buf_txt]
        mov ecx, 60
        test rsi, rsi
        jz .fab_fin
.fab:   lodsw
        test ax, ax
        jz .fab_fin
        cmp ax, 20h
        jb .raro
        cmp ax, 7Eh
        jbe .pon
.raro:  mov al, '?'
.pon:   stosb
        dec ecx
        jnz .fab
.fab_fin:
        mov byte [rdi], 0
        DI buf_txt
        DI t_r_rev
        mov eax, [rbx + ST_FWREV]
        mov ecx, 8
        call hex
        DI t_r_uefi
        movzx eax, word [rbx + 10]      ; Hdr.Revision: mayor y menor
        call decimal
        DI t_punto
        movzx eax, word [rbx + 8]
        call decimal
        DI t_r_conout
        mov rax, [rel conout]
        call hex64
        DI t_nl
        mov byte [rel callado], 0
        pop rdi
        pop rsi
        pop rbx
        ret

; ---- tecla_esperar: hasta que se pulse una tecla o pasen 2 minutos ----
tecla_esperar:
        push rbx
        sub rsp, 32                     ; (8 + 8 + 32 = 48: alineada)
        mov ebx, 1200
.otra:  mov rax, [rel bootserv]
        mov ecx, 100000                 ; 0,1 s
        FW [rax + BS_STALL]
        mov rax, [rel sistema]
        mov rcx, [rax + ST_CONIN]
        test rcx, rcx
        jz .sin_teclado
        lea rdx, [rel tecla_efi]
        FW [rcx + 8]                    ; ReadKeyStroke
        test rax, rax
        jz .fin
.sin_teclado:
        dec ebx
        jnz .otra
.fin:   add rsp, 32
        pop rbx
        ret

; ---- registro_paso: en la imagen de prueba (-DREGISTRO_PASOS) el registro se
;      guarda en cada paso, para que quede aunque el equipo se pare después ----
registro_paso:
%ifdef REGISTRO_PASOS
        push rax
        call registro_guardar
        pop rax
%endif
        ret

; ---- registro_guardar: el registro a \HISPABIOS.LOG de la memoria USB (el
;      volumen del que se ha arrancado); CF si no se puede ----
registro_guardar:
        push rbx
        push rsi
        push rdi
        push r12
        push r13
        sub rsp, 64                     ; (8 + 40 + 64 = 112: alineada)
        cmp qword [rel reg_dir], 0
        je .mal
        mov rax, [rel bootserv]
        mov rcx, [rel imagen]
        lea rdx, [rel guid_imagen]
        lea r8, [rel imagen_cargada]
        FW [rax + BS_HANDLEPROT]
        test rax, rax
        jnz .mal
        mov rax, [rel imagen_cargada]
        mov rcx, [rax + 24]             ; DeviceHandle
        mov rax, [rel bootserv]
        lea rdx, [rel guid_ficheros]
        lea r8, [rel reg_vol]
        FW [rax + BS_HANDLEPROT]
        test rax, rax
        jnz .mal
        mov rcx, [rel reg_vol]
        lea rdx, [rel reg_raiz]
        FW [rcx + FS_OPENVOL]
        test rax, rax
        jnz .mal
        mov r12, [rel reg_raiz]
        ; si ya hay uno, se borra (así no queda la cola del anterior)
        mov rcx, r12
        lea rdx, [rel reg_fich]
        lea r8, [rel nombre_reg]
        mov r9d, 3                      ; leer y escribir
        mov qword [rsp + 32], 0
        FW [r12 + F_OPEN]
        test rax, rax
        jnz .crear
        mov rcx, [rel reg_fich]
        FW [rcx + F_DELETE]             ; (cierra también)
.crear: mov rcx, r12
        lea rdx, [rel reg_fich]
        lea r8, [rel nombre_reg]
        mov r9, 8000000000000003h       ; crear, leer y escribir
        mov qword [rsp + 32], 0
        FW [r12 + F_OPEN]
        mov r13, rax
        test rax, rax
        jnz .cierra_raiz
        mov rcx, [rel reg_fich]
        mov rax, [rel reg_n]
        mov [rel reg_escrito], rax
        lea rdx, [rel reg_escrito]
        mov r8, [rel reg_dir]
        FW [rcx + F_WRITE]
        mov r13, rax
        mov rcx, [rel reg_fich]
        FW [rcx + F_CLOSE]
.cierra_raiz:
        mov rcx, r12
        FW [r12 + F_CLOSE]
        test r13, r13
        jnz .mal
        clc
        jmp .fin
.mal:   stc
.fin:   lea rsp, [rsp + 64]
        pop r13
        pop r12
        pop rdi
        pop rsi
        pop rbx
        ret

%ifdef PRUEBA_PILA
; pila_mal: (pruebas) avisa por COM1 de una llamada al firmware con la pila mal
pila_mal:
        push rax
        push rdx
        push rsi
        lea rsi, [rel t_pila_mal]
.c:     lodsb
        test al, al
        jz .f
        mov dx, 3F8h
        out dx, al
        jmp .c
.f:     pop rsi
        pop rdx
        pop rax
        ret
t_pila_mal      db 13, 10, 'PILA-MAL', 13, 10, 0
%endif

; ---- memoria_ext: pide al firmware la memoria seguida desde 1 MB -> EAX = KB ----
memoria_ext:
        push rbx
        sub rsp, 32                     ; (8 + 8 + 32 = 48: pila alineada)
        mov ebx, 16384                  ; 64 MB en páginas; si no, la mitad...
.prueba:
        mov qword [rel dir_pag], 100000h
        mov rax, [rel bootserv]
        mov ecx, ALLOC_ADDRESS
        mov edx, MEM_LOADERDATA
        mov r8d, ebx
        lea r9, [rel dir_pag]
        FW [rax + BS_ALLOCPAGES]
        test rax, rax
        jz .dada
        shr ebx, 1
        cmp ebx, 256                    ; menos de 1 MB: nada
        jae .prueba
        xor eax, eax
        jmp .fin
.dada:  mov eax, ebx
        shl eax, 2                      ; páginas -> KB
.fin:   add rsp, 32
        pop rbx
        ret

; ---- tsc_calibrar: -> EAX = cuentas del TSC en un tic de la BIOS (1/18,2 s) ----
tsc_calibrar:
        push rbx
        sub rsp, 32
        rdtsc
        shl rdx, 32
        or rax, rdx
        mov rbx, rax
        mov rax, [rel bootserv]
        mov ecx, 50000                  ; 50 ms
        FW [rax + BS_STALL]
        rdtsc
        shl rdx, 32
        or rax, rdx
        sub rax, rbx                    ; cuentas en 50 ms
        mov ecx, 54925                  ; un tic = 54,925 ms
        mul rcx
        mov ecx, 50000
        div rcx
        add rsp, 32
        pop rbx
        ret

; ---- discos_buscar: el disco de arranque (80h) y los demás -> AL = cuántos ----
discos_buscar:
        push rbx
        push rsi
        push rdi
        push r12
        push r13
        push r14
        sub rsp, 56
        mov byte [rel ndiscos], 0
        ; el camino del dispositivo del que se ha arrancado
        mov rax, [rel bootserv]
        mov rcx, [rel imagen]
        lea rdx, [rel guid_imagen]
        lea r8, [rel imagen_cargada]
        FW [rax + BS_HANDLEPROT]
        test rax, rax
        jnz .fin
        mov rax, [rel imagen_cargada]
        mov rcx, [rax + 24]             ; DeviceHandle
        mov rax, [rel bootserv]
        lea rdx, [rel guid_camino]
        lea r8, [rel camino_arranque]
        FW [rax + BS_HANDLEPROT]
        test rax, rax
        jnz .fin
        ; todos los manejadores con Block I/O
        mov rax, [rel bootserv]
        mov ecx, 2                      ; ByProtocol
        lea rdx, [rel guid_bloques]
        xor r8d, r8d
        lea r9, [rel n_manejadores]
        lea r10, [rel manejadores]
        mov [rsp + 32], r10
        FW [rax + BS_LOCATEHBUF]
        test rax, rax
        jnz .fin
        ; dos pasadas: primero el disco que contiene el camino de arranque
        ; (será el 80h) y después los demás discos enteros
        xor r14d, r14d                  ; R14 = pasada (0 = arranque, 1 = resto)
.pasada:
        xor r12d, r12d                  ; R12 = índice del manejador
.cada:
        cmp r12, [rel n_manejadores]
        jae .fin_pasada
        mov rax, [rel manejadores]
        mov r13, [rax + r12 * 8]        ; R13 = manejador
        mov rax, [rel bootserv]
        mov rcx, r13
        lea rdx, [rel guid_bloques]
        lea r8, [rel bloques]
        FW [rax + BS_HANDLEPROT]
        test rax, rax
        jnz .otro
        mov rbx, [rel bloques]
        mov rsi, [rbx + 8]              ; Media
        cmp byte [rsi + 5], 0           ; MediaPresent
        je .otro
        cmp byte [rsi + 6], 0           ; LogicalPartition: solo discos enteros
        jne .otro
        cmp dword [rsi + 12], 512       ; sectores de 512 octetos
        jne .otro
        ; ¿contiene el camino de arranque?
        mov rax, [rel bootserv]
        mov rcx, r13
        lea rdx, [rel guid_camino]
        lea r8, [rel camino]
        FW [rax + BS_HANDLEPROT]
        xor edi, edi                    ; EDI = 1 si es el de arranque
        test rax, rax
        jnz .comparado
        mov rsi, [rel camino]
        call camino_largo               ; RCX = largo sin el nodo final
        mov rdi, [rel camino_arranque]
        mov rsi, [rel camino]
        cld
        repe cmpsb
        mov edi, 0
        jne .comparado
        mov edi, 1
.comparado:
        mov eax, 1
        sub eax, r14d                   ; pasada 0 quiere EDI = 1; pasada 1, EDI = 0
        cmp edi, eax
        jne .otro
        ; apunta el disco
        movzx eax, byte [rel ndiscos]
        cmp eax, 8
        jae .otro
        lea rdx, [rel discos]
        shl eax, 4
        mov [rdx + rax], rbx            ; Block I/O
        mov rsi, [rbx + 8]
        mov ecx, [rsi]                  ; MediaId
        mov [rdx + rax + 8], ecx
        mov rcx, [rsi + 24]             ; LastBlock
        inc rcx
        movzx eax, byte [rel ndiscos]
        imul eax, 16
        mov rdi, [rel bios_base]
        mov [rdi + 40 + rax], rcx       ; T_DISCOS: sectores
        mov dword [rdi + 48 + rax], 512
        inc byte [rel ndiscos]
.otro:
        inc r12
        jmp .cada
.fin_pasada:
        inc r14d
        cmp r14d, 2
        jb .pasada
.fin:
        movzx eax, byte [rel ndiscos]
        add rsp, 56
        pop r14
        pop r13
        pop r12
        pop rdi
        pop rsi
        pop rbx
        ret

; camino_largo: RSI = camino de dispositivo -> RCX = octetos antes del nodo final
camino_largo:
        push rsi
        xor ecx, ecx
.n:     cmp byte [rsi], 7Fh             ; nodo final
        je .f
        movzx eax, word [rsi + 2]
        add rsi, rax
        add rcx, rax
        jmp .n
.f:     pop rsi
        ret


; ---- vector_irq0: el vector de la IRQ 0 en el 8259 del firmware -> AL (0: no hay) ----
vector_irq0:
        push rbx
        sub rsp, 48
        mov rax, [rel bootserv]
        lea rcx, [rel guid_8259]
        xor edx, edx
        lea r8, [rel p8259]
        FW [rax + BS_LOCATEPROT]
        test rax, rax
        jnz .no
        mov rcx, [rel p8259]
        xor edx, edx                    ; IRQ 0
        lea r8, [rel vec0]
        FW [rcx + 32]                 ; GetVector
        test rax, rax
        jnz .no
        movzx eax, byte [rel vec0]
        jmp .fin
.no:    xor eax, eax
.fin:   add rsp, 48
        pop rbx
        ret

; ---- despachador: las peticiones de la BIOS (ECX = petición, EDX = argumento) ----
; (llega con las interrupciones cerradas; las abre mientras trabaja el firmware)
despachador:
        push rbx
        push rsi
        push rdi
        sub rsp, 48
        cmp ecx, 1                      ; dibujar la pantalla
        je .dibujar
        cmp ecx, 2                      ; esperar EDX segundos
        je .esperar
        cmp ecx, 3                      ; leer sectores (bloque P_*)
        je .leer
        cmp ecx, 4                      ; escribir sectores
        je .escribir
        cmp ecx, 5                      ; una tecla, si hay
        je .tecla
        cmp ecx, 6                      ; esperar EDX microsegundos
        je .microseg
        cmp ecx, 7                      ; reiniciar (EDX = 0) o apagar (2)
        je .reiniciar
        mov eax, -1
        jmp .fin
.dibujar:
        call pantalla_dibujar
        xor eax, eax
        jmp .fin
.esperar:
        imul edx, edx, 1000000
.microseg:
        mov rax, [rel bootserv]
        mov ecx, edx
        sti
        FW [rax + BS_STALL]
        cli
        xor eax, eax
        jmp .fin
.leer:
.escribir:
        mov ebx, ecx                    ; 3 = leer, 4 = escribir
        mov rdi, [rel bios_base]
        movzx eax, byte [rdi + 168]     ; P_DISCO
        cmp al, [rel ndiscos]
        jae .mal
        shl eax, 4
        lea rsi, [rel discos]
        mov rcx, [rsi + rax]            ; Block I/O
        mov edx, [rsi + rax + 8]        ; MediaId
        mov r8, [rdi + 172]             ; P_LBA
        movzx r9d, word [rdi + 170]     ; P_N
        shl r9d, 9                      ; octetos
        lea r10, [rdi + PASO]           ; el búfer de paso
        mov [rsp + 32], r10
        sti
        cmp ebx, 3
        jne .w
        FW [rcx + 24]                 ; ReadBlocks
        jmp .rw
.w:     FW [rcx + 32]                 ; WriteBlocks
.rw:    cli
        test rax, rax
        jnz .mal
        xor eax, eax
        jmp .fin
.mal:   mov eax, 1
        jmp .fin
.tecla:
        mov rax, [rel bootserv]         ; un momento para que el firmware
        mov ecx, 1000                   ; atienda el teclado (USB)
        sti
        FW [rax + BS_STALL]
        cli
        cmp qword [rel coninex], 0      ; el protocolo con el estado de Ctrl/Alt
        jg .ex                          ; (SimpleTextInputEx): localizado y hay
        jl .plano                       ; localizado y no hay -> sin modificadores
        mov qword [rel coninex], -1     ; primera vez: localizarlo
        mov rax, [rel sistema]
        mov rcx, [rax + ST_CONINH]
        lea rdx, [rel guid_inex]
        lea r8, [rel coninex]
        mov rax, [rel bootserv]
        FW [rax + BS_HANDLEPROT]
        test rax, rax
        jz .ex
        mov qword [rel coninex], -1
        jmp .plano
.ex:
        sti
        mov rcx, [rel coninex]
        lea rdx, [rel keydata]
        FW [rcx + 8]                  ; ReadKeyStrokeEx (tecla + Ctrl/Alt/...)
        cli
        test rax, rax
        jnz .sin_tecla
        mov eax, [rel keydata + 4]      ; estado de las teclas modificadoras
        test eax, 0x80000000            ; ¿válido?
        jz .devolver
        test eax, 0x0C                  ; ¿algún Ctrl?
        jz .devolver
        test eax, 0x30                  ; ¿algún Alt?
        jz .devolver
        cmp word [rel keydata], 0x08    ; ¿Supr? (código de UEFI) -> reiniciar
        jne .devolver
        xor edx, edx                    ; EfiResetCold
        jmp .reiniciar
.devolver:
        movzx eax, word [rel keydata]   ; ScanCode (bajo)
        movzx edx, word [rel keydata + 2]  ; UnicodeChar (alto)
        shl edx, 16
        or eax, edx
        jmp .fin
.plano:
        sti
        mov rax, [rel sistema]
        mov rcx, [rax + ST_CONIN]
        lea rdx, [rel tecla_efi]
        FW [rcx + 8]                  ; ReadKeyStroke (sin modificadores)
        cli
        test rax, rax
        jnz .sin_tecla
        mov eax, [rel tecla_efi]        ; ScanCode (bajo) y UnicodeChar (alto)
        jmp .fin
.sin_tecla:
        mov eax, -1
        jmp .fin
.reiniciar:
        mov rax, [rel sistema]
        mov rax, [rax + ST_RUNTIME]
        mov ecx, edx                    ; EfiResetCold = 0, EfiResetShutdown = 2
        xor edx, edx
        xor r8d, r8d
        xor r9d, r9d
        FW [rax + RT_RESET]
        mov eax, -1
.fin:   add rsp, 48
        pop rdi
        pop rsi
        pop rbx
        ret

; ---- gop_buscar: localiza la salida gráfica -> fb, ancho, alto, línea; CF si no ----
gop_buscar:
        push rbx
        sub rsp, 32
%ifdef PRUEBA_SIN_GOP
        jmp .no
%endif
        mov rax, [rel bootserv]
        lea rcx, [rel guid_gop]
        xor edx, edx
        lea r8, [rel gop]
        FW [rax + BS_LOCATEPROT]
        test rax, rax
        jnz .no
        mov rbx, [rel gop]
        mov rbx, [rbx + 24]             ; Mode
        mov rax, [rbx + 24]             ; FrameBufferBase
        mov [rel fb], rax
        mov rbx, [rbx + 8]              ; Info
        mov eax, [rbx + 4]
        mov [rel fb_ancho], eax
        mov eax, [rbx + 8]
        mov [rel fb_alto], eax
        mov eax, [rbx + 12]
        mov [rel fb_formato], eax       ; 0 = RGBX, 1 = BGRX
        mov eax, [rbx + 32]
        mov [rel fb_linea], eax
        cmp dword [rel fb_ancho], 640
        jb .no
        cmp dword [rel fb_alto], 400
        jb .no
        cmp dword [rel fb_formato], 1
        ja .no
        add rsp, 32
        pop rbx
        clc
        ret
.no:    add rsp, 32
        pop rbx
        stc
        ret

; ---- escala: tablas de dónde cae cada punto de 640x400 en la pantalla real ----
; Se amplía lo más posible sin deformar (8:5): en 1920x1080, a 1728x1080.
escala:
        push rbx
        push rsi
        push rdi
        mov eax, [rel fb_ancho]
        mov ecx, [rel fb_alto]
        lea edx, [eax * 4 + eax]        ; An*5 <= Al*8: manda el ancho
        lea ebx, [ecx * 8]
        cmp edx, ebx
        ja .alto
        mov [rel dib_an], eax
        shr edx, 3
        mov [rel dib_al], edx
        jmp .origen
.alto:  mov [rel dib_al], ecx
        mov eax, ebx                    ; Al*8/5
        xor edx, edx
        mov ebx, 5
        div ebx
        mov [rel dib_an], eax
.origen:
        lea rdi, [rel tab_x]
        mov eax, [rel fb_ancho]
        sub eax, [rel dib_an]
        shr eax, 1
        mov esi, eax
        xor ecx, ecx
.tx:    mov eax, ecx
        mul dword [rel dib_an]
        mov ebx, 640
        div ebx
        add eax, esi
        mov [rdi + rcx * 4], eax
        inc ecx
        cmp ecx, 640
        jbe .tx
        lea rdi, [rel tab_y]
        mov eax, [rel fb_alto]
        sub eax, [rel dib_al]
        shr eax, 1
        mov esi, eax
        xor ecx, ecx
.ty:    mov eax, ecx
        mul dword [rel dib_al]
        mov ebx, 400
        div ebx
        add eax, esi
        mov [rdi + rcx * 4], eax
        inc ecx
        cmp ecx, 400
        jbe .ty
        pop rdi
        pop rsi
        pop rbx
        ret

; ---- pantalla_dibujar: pinta la pantalla de texto de la BIOS (80x25), ampliada ----
pantalla_dibujar:
        push rbx
        push rsi
        push rdi
        push r12
        push r13
        mov r13, [rel bios_base]
        ; la primera vez se borra todo en negro; después solo se pintan las
        ; celdas que han cambiado (escribir en la memoria de vídeo real es
        ; lento y mientras tanto el reloj de DOS no corre)
        cld
        cmp byte [rel borrada], 0
        jne .ya_borrada
        mov byte [rel borrada], 1
        call escala
        mov rdi, [rel fb]
        mov eax, [rel fb_linea]
        imul eax, [rel fb_alto]
        mov ecx, eax
        xor eax, eax
        rep stosd
        lea rdi, [rel sombra]           ; nada pintado todavía
        mov ecx, 2000
        mov ax, 0FFFFh
        rep stosw
.ya_borrada:
        xor r12d, r12d                  ; R12 = número de celda (0..1999)
.celda:
        movzx ebx, word [r13 + TX + r12 * 2] ; BL = carácter, BH = atributo
        lea rsi, [rel sombra]
        cmp bx, [rsi + r12 * 2]
        je .siguiente
        mov [rsi + r12 * 2], bx
        lea rsi, [rel paleta]
        movzx eax, bh
        and eax, 0Fh
        mov ecx, [rsi + rax * 4]        ; color de la letra
        movzx eax, bh
        shr eax, 4
        and eax, 7
        mov edx, [rsi + rax * 4]        ; color del fondo
        cmp dword [rel fb_formato], 0
        jne .bgr
        bswap ecx
        shr ecx, 8
        bswap edx
        shr edx, 8
.bgr:   movzx eax, bl
        shl eax, 4
        lea rsi, [rel fuente]
        add rsi, rax
        mov r8d, edx
        mov eax, r12d
        xor edx, edx
        mov ebx, 80
        div ebx                         ; EAX = fila, EDX = columna
        mov ebx, edx
        mov edx, r8d
        call pintar_celda
.siguiente:
        inc r12d
        cmp r12d, 2000
        jb .celda
        pop r13
        pop r12
        pop rdi
        pop rsi
        pop rbx
        ret

; pintar_celda: EAX = fila, EBX = columna, RSI = letra, ECX = tinta, EDX = fondo
pintar_celda:
        push rbx
        push rsi
        push rdi
        push r12
        push r13
        push r14
        push r15
        mov r14d, ecx                   ; R14 = tinta, R15 = fondo
        mov r15d, edx
        lea r12, [rel tab_x]
        lea r13, [rel tab_y]
        shl eax, 4
        mov r8d, eax                    ; R8 = fila de puntos
        shl ebx, 3
        mov r9d, ebx                    ; R9 = primera columna de puntos
        mov r10d, 16
.fila:  mov r11b, [rsi]
        inc rsi
        push rsi
        mov esi, [r13 + r8 * 4]         ; ESI = y0
        mov ebx, r9d
        mov ecx, 8
.punto: mov eax, r15d
        shl r11b, 1
        cmovc eax, r14d
        push rcx
        mov ecx, [r12 + rbx * 4]        ; x
        mov edx, [r12 + rbx * 4 + 4]
        sub edx, ecx                    ; ancho
        jz .sig
        push rsi
.y:     cmp esi, [r13 + r8 * 4 + 4]
        jae .ya
        mov edi, esi
        imul edi, [rel fb_linea]
        add edi, ecx
        shl rdi, 2
        add rdi, [rel fb]
        push rcx
        mov ecx, edx
        rep stosd
        pop rcx
        inc esi
        jmp .y
.ya:    pop rsi
.sig:   pop rcx
        inc ebx
        dec ecx
        jnz .punto
        pop rsi
        inc r8d
        dec r10d
        jnz .fila
        pop r15
        pop r14
        pop r13
        pop r12
        pop rdi
        pop rsi
        pop rbx
        ret

; ---- escribir: RSI = texto ASCII terminado en 0 -> al
;      registro y, si no está «callado», a la pantalla por el servicio de texto.
;      Conserva todos los registros; alinea la pila ella misma. ----
escribir:
        push rax
        push rcx
        push rdx
        push rsi
        push rdi
        push r8
        push r9
        push r10
        push r11
        push rbp
        mov rbp, rsp
        and rsp, -16                    ; la pila, alineada venga de donde venga
        sub rsp, 32
        cld
        mov rdx, rsi
        mov rdi, [rel reg_dir]          ; al registro
        test rdi, rdi
        jz .pantalla
        mov rcx, [rel reg_n]
.r:     mov al, [rsi]
        test al, al
        jz .rf
        cmp rcx, REG_MAX
        jae .rf
        mov [rdi + rcx], al
        inc rcx
        inc rsi
        jmp .r
.rf:    mov [rel reg_n], rcx
.pantalla:
        cmp byte [rel callado], 0
        jne .fin
        mov rcx, [rel conout]           ; (hay equipos sin consola de texto)
        test rcx, rcx
        jz .fin
        mov rsi, rdx
.trozo: lea rdi, [rel buf16]            ; por trozos de 255 letras
        mov edx, 255
.c:     lodsb
        movzx eax, al
        stosw
        test al, al
        jz .ya
        dec edx
        jnz .c
        xor eax, eax
        stosw
        mov rcx, [rel conout]
        lea rdx, [rel buf16]
        FW [rcx + TXT_OUTSTR]           ; (RSI lo conserva el firmware)
        jmp .trozo
.ya:    mov rcx, [rel conout]
        lea rdx, [rel buf16]
        FW [rcx + TXT_OUTSTR]
.fin:   mov rsp, rbp
        pop rbp
        pop r11
        pop r10
        pop r9
        pop r8
        pop rdi
        pop rsi
        pop rdx
        pop rcx
        pop rax
        ret

; ---- hex64: RAX en hexadecimal (16 cifras); hex: con ECX cifras ----
hex64:
        push rcx
        mov ecx, 16
        call hex
        pop rcx
        ret
hex:
        push rax
        push rcx
        push rdx
        push rdi
        push rsi
        lea rdi, [rel buf_txt]
        add rdi, rcx
        mov byte [rdi], 0
.d:     dec rdi
        mov edx, eax
        and edx, 0Fh
        cmp dl, 10
        jb .n
        add dl, 'A' - 10 - '0'
.n:     add dl, '0'
        mov [rdi], dl
        shr rax, 4
        dec ecx
        jnz .d
        lea rsi, [rel buf_txt]
        call escribir
        pop rsi
        pop rdi
        pop rdx
        pop rcx
        pop rax
        ret

; ---- decimal: RAX en decimal ----
decimal:
        push rax
        push rcx
        push rdx
        push rsi
        lea rsi, [rel buf_txt + 30]
        mov byte [rsi], 0
        mov ecx, 10
.d:     xor edx, edx
        div rcx
        add dl, '0'
        dec rsi
        mov [rsi], dl
        test rax, rax
        jnz .d
        call escribir
        pop rsi
        pop rdx
        pop rcx
        pop rax
        ret

t_titulo        db 'HispaBIOS ', HB_VERSION, ' (x64)', 13, 10, 0
t_r_fab         db '[registro de HispaBIOS ', HB_VERSION, ' x64]', 13, 10, 'Firmware: ', 0
t_r_rev         db ', revision ', 0
t_r_uefi        db 'h, UEFI ', 0
t_punto         db '.', 0
t_r_conout      db ', ConOut ', 0
t_p_mapa        db '[paso] GetMemoryMap', 13, 10, 0
t_mapa_mal      db 'No se ha podido leer el mapa de memoria del firmware. Codigo: ', 0
t_desc_mal      db 'El mapa de memoria del firmware no se entiende (entradas de ', 0
t_r_mapa        db 'Mapa de memoria: ', 0
t_r_mapa2       db ' entradas de ', 0
t_r_mapa3       db ' octetos (inicio, paginas, tipo, atributos)', 13, 10, 0
t_mapa          db 'Memoria por debajo de 1 MB (inicio, paginas, tipo):', 13, 10, 0
t_sangria       db '  ', 0
t_paginas       db ' pag. ', 0
t_tipo          db ' tipo ', 0
t_espacio       db ' ', 0
t_atr           db ' atr. ', 0
t_nl            db 13, 10, 0
t_mas           db '  (y ', 0
t_mas2          db ' tramos mas: en el registro)', 13, 10, 0
t_total         db 'Memoria del equipo: ', 0
t_mb            db ' MB', 13, 10, 0
; los tipos de memoria de UEFI (12 octetos cada nombre)
t_tipos         db 'reservada', 0, 0, 0
                db 'cargador c.', 0
                db 'cargador d.', 0
                db 'firmware c.', 0
                db 'firmware d.', 0
                db 'servicios c', 0
                db 'servicios d', 0
                db 'libre', 0, 0, 0, 0, 0, 0, 0
                db 'averiada', 0, 0, 0, 0
                db 'ACPI', 0, 0, 0, 0, 0, 0, 0, 0
                db 'ACPI NVS', 0, 0, 0, 0
                db 'E/S', 0, 0, 0, 0, 0, 0, 0, 0, 0
                db 'E/S puertos', 0
                db 'PAL', 0, 0, 0, 0, 0, 0, 0, 0, 0
                db 'persistente', 0
                db 'sin aceptar', 0
t_tipo_otro     db 'del fabricante', 0
t_pide          db 'Pidiendo al firmware la memoria baja (0 a 9FFFF)...', 13, 10, 0
t_no_dada       db '  no da de ', 0
t_a             db ' a ', 0
t_parentesis    db ' (', 0
t_pag_fin       db ' pag.)', 13, 10, 0
t_dadas         db 'Paginas dadas: ', 0
t_de160         db ' de 160', 13, 10, 0
t_base          db 'La BIOS de modo real va en ', 0
t_base2         db 'h. Memoria convencional para DOS: ', 0
t_kb            db ' KB', 13, 10, 0
t_sin_arranque  db 13, 10, 'El firmware usa la pagina 80000h (la del sector de arranque de Hispa-DOS).', 0
t_sin_base      db 13, 10
                db 'ESTE EQUIPO NO DEJA LIBRE LA MEMORIA BAJA QUE NECESITA HISPA-DOS', 13, 10
                db '(60 KB seguidos entre 384 y 636 KB): la usa su firmware.', 13, 10
                db 'Que hacer: entre en la configuracion del equipo (BIOS/UEFI) y, si tiene', 13, 10
                db '"CSM", "Legacy" o "arranque heredado", activelo y arranque la memoria', 13, 10
                db 'USB en ese modo (sin "UEFI:" delante en el menu de arranque).', 13, 10, 0
t_poca          db 13, 10
                db 'ESTE EQUIPO DEJA MUY POCA MEMORIA CONVENCIONAL (menos de 256 KB):', 13, 10
                db 'su firmware usa la memoria baja y Hispa-DOS no cabe.', 13, 10
                db 'Que hacer: entre en la configuracion del equipo (BIOS/UEFI) y, si tiene', 13, 10
                db '"CSM", "Legacy" o "arranque heredado", activelo y arranque la memoria', 13, 10
                db 'USB en ese modo (sin "UEFI:" delante en el menu de arranque).', 13, 10, 0
t_gop           db 'Pantalla (GOP): ', 0
t_por           db 'x', 0
t_gop_f         db ', formato ', 0
t_gop_l         db ', puntos por linea ', 0
t_gop_d         db ', en ', 0
t_r_ext         db 'Memoria extendida: ', 0
t_r_tsc         db ' KB; TSC por tic: ', 0
t_r_vec         db '; vector de la IRQ 0 (8259): ', 0
t_r_discos      db 'h; discos: ', 0
t_sin_discos    db 13, 10, 'NO SE ENCUENTRA EL DISCO DE ARRANQUE (la memoria USB) entre los discos', 13, 10
                db 'del firmware (hacen falta sectores de 512 octetos).', 13, 10, 0
t_viaje         db 'Arrancando la BIOS de modo real...', 13, 10, 0
t_sin_gop       db 13, 10, 'ESTE EQUIPO NO DA UNA PANTALLA GRAFICA (GOP) DE 640x400 O MAS con color', 13, 10
                db 'de 32 bits, que es donde HispaBIOS dibuja la pantalla de DOS.', 13, 10
                db 'Que hacer: si la configuracion del equipo (BIOS/UEFI) tiene "CSM" o', 13, 10
                db '"arranque heredado", activelo y arranque la memoria USB en ese modo.', 13, 10, 0
t_vuelta        db 13, 10, 'La BIOS de modo real ha vuelto al firmware.', 13, 10, 0
t_guardado      db 13, 10, 'Se ha guardado el registro en la memoria USB: HISPABIOS.LOG', 13, 10
                db '(para saber que pasa, hace falta ese archivo).', 13, 10, 0
t_no_guardado   db 13, 10, 'No se ha podido guardar el registro (HISPABIOS.LOG) en la memoria USB:', 13, 10
                db 'haga una foto de esta pantalla.', 13, 10, 0
t_tecla         db 'Pulse una tecla para volver al firmware (o espere 2 minutos).', 13, 10, 0

; dónde puede ir la BIOS de modo real: base, dónde está su copia en esta
; imagen y cuánto ocupa (por orden de preferencia; acaba en 0). En 80000h no:
; ahí se copia el sector de arranque de Hispa-DOS (boot/boot16.asm, RELOC_SEG)
        align 4
bases:
        dd 90000h, bios_9 - pe_inicio, bios_9_fin - bios_9
        dd 70000h, bios_7 - pe_inicio, bios_7_fin - bios_7
        dd 60000h, bios_6 - pe_inicio, bios_6_fin - bios_6
        dd 0
%ifdef PRUEBA_OCUPA
        align 8
prueba_ocupa    dq PRUEBA_OCUPA, -1
%endif
        align 2
nombre_reg      dw 'H','I','S','P','A','B','I','O','S','.','L','O','G', 0
guid_ficheros   dd 964E5B22h            ; EFI_SIMPLE_FILE_SYSTEM_PROTOCOL
                dw 6459h, 11D2h
                db 8Eh, 39h, 00h, 0A0h, 0C9h, 69h, 72h, 3Bh
guid_imagen     dd 5B1B31A1h
                dw 9562h, 11D2h
                db 8Eh, 3Fh, 00h, 0A0h, 0C9h, 69h, 72h, 3Bh
guid_camino     dd 09576E91h
                dw 6D3Fh, 11D2h
                db 8Eh, 39h, 00h, 0A0h, 0C9h, 69h, 72h, 3Bh
guid_bloques    dd 964E5B21h
                dw 6459h, 11D2h
                db 8Eh, 39h, 00h, 0A0h, 0C9h, 69h, 72h, 3Bh
guid_8259       dd 38321DBAh
                dw 4FE0h, 4E17h
                db 8Ah, 0ECh, 41h, 30h, 55h, 0EAh, 0EDh, 0C1h
        align 8
p8259           dq 0
vec0            dq 0
imagen_cargada  dq 0
camino_arranque dq 0
camino          dq 0
bloques         dq 0
n_manejadores   dq 0
manejadores     dq 0
bajo_tope       dq 0
tecla_efi       dd 0
coninex         dq 0                    ; SimpleTextInputEx (0 = sin mirar, -1 = no hay)
keydata         times 12 db 0           ; tecla + estado de Ctrl/Alt/...
guid_inex       dd 0DD9E7534h           ; EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL
                dw 7762h, 4698h
                db 8Ch, 14h, 0F5h, 85h, 17h, 0A6h, 25h, 0AAh
ndiscos         db 0
borrada         db 0
        align 8
discos          times 16 dq 0           ; Block I/O y MediaId de cada disco (8)
        align 8
gop             dq 0
fb              dq 0
fb_ancho        dd 0
fb_alto         dd 0
fb_formato      dd 0
fb_linea        dd 0
guid_gop        dd 9042A9DEh
                dw 23DCh, 4A38h
                db 96h, 0FBh, 7Ah, 0DEh, 0D0h, 80h, 51h, 6Ah
; los 16 colores del modo texto (00RRGGBB)
paleta          dd 000000h, 0000AAh, 00AA00h, 00AAAAh, 0AA0000h, 0AA00AAh, 0AA5500h, 0AAAAAAh
                dd 555555h, 5555FFh, 55FF55h, 55FFFFh, 0FF5555h, 0FF55FFh, 0FFFF55h, 0FFFFFFh
fuente:         incbin "FUENTE16.BIN"
        align 8
imagen          dq 0
sistema         dq 0
conout          dq 0
bootserv        dq 0
bios_base       dq 0                    ; dónde está la BIOS de modo real
bios_cual       dq 0                    ; su entrada en «bases»
mapa_dir        dq 0                    ; el mapa de memoria
mapa_cabe       dq 0
mapa_tam        dq 0
mapa_llave      dq 0
desc_tam        dq 0
desc_ver        dq 0
mapa_min        dq 0, 0
dir_pag         dq 0
reg_dir         dq 0                    ; el registro (texto) y cuánto lleva
reg_n           dq 0
reg_escrito     dq 0
reg_vol         dq 0
reg_raiz        dq 0
reg_fich        dq 0
callado         db 0                    ; 1: «escribir» solo apunta en el registro
dada            times NPAG db 0         ; 1: el firmware ha dado esa página baja
buf_txt         times 64 db 0
                align 8
buf16           times 256 dw 0
sombra          times 2000 dw 0         ; lo que hay pintado (carácter y atributo)
tab_x           times 641 dd 0
tab_y           times 401 dd 0
dib_an          dd 0
dib_al          dd 0
; la BIOS de modo real, ensamblada para cada base posible
bios_9:         incbin "BIOS64.BIN"
bios_9_fin:
bios_7:         incbin "BIOS64_7.BIN"
bios_7_fin:
bios_6:         incbin "BIOS64_6.BIN"
bios_6_fin:

PE_FIN
