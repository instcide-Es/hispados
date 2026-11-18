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
;  Desde la 0.3 (docs/HISPABIOS.md): presta a DOS las páginas sueltas de
;  memoria baja del firmware (paginas_prestar), copia las entradas de ACPI y
;  SMBIOS a la BIOS (tablas_copiar), pinta la pantalla por un alias con
;  escritura combinada y mide su velocidad (wc_preparar), da a la BIOS el
;  marco lineal de sus modos VBE (vbe_preparar, graf_presentar) y tiene un
;  modo seguro (tecla S al arrancar: seguro_mirar) que no hace nada de eso.
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
;    -DPRUEBA_LENTA=us         como si pintar un millón de puntos costara eso
;    -DPRUEBA_SIN_WC           sin el alias de escritura combinada
;    -DPRUEBA_SIN_PRESTAR      sin prestar a DOS las páginas bajas del firmware
;    -DPANTALLA_NATIVA         no cambiar de modo de pantalla aunque sea lenta
;    -DPRUEBA_SEGURO           como si se hubiera pulsado S al arrancar (modo seguro)
;    -DPRUEBA_P2SB             un P2SB de mentira con el 8254 bloqueado (el camino
;                              de quitar el «clock gating», que QEMU no tiene)
;    -DPRUEBA_RELOJ=mascara    fuentes del reloj que la BIOS no prueba (bit 0: el
;                              8254 por LINT0; 1: por el IOAPIC; 2 y 3: el HPET;
;                              4 y 5: el reloj CMOS), como si no dieran la IRQ 0
;    -DREGISTRO_PASOS          guarda HISPABIOS.LOG en cada paso aunque todo
;                              vaya bien (la imagen de prueba para un equipo
;                              que se queda parado: tools/mkprueba_hispabios.sh)
;
;  Referencias: especificación UEFI 2.x (2.3.4, 4, 7, 12, 13).
; =============================================================================
%define PE_ARQ 64
%include "pe.inc"

%define HB_VERSION '0.4'

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

ALLOC_ANY       equ 0           ; AllocateAnyPages
ALLOC_MAX       equ 1           ; AllocateMaxAddress
ALLOC_ADDRESS   equ 2           ; AllocateAddress
VBE_MEM         equ 300000h     ; el marco lineal de VBE (1024x768 de 32 bits)
VBE_PAGS        equ (2 * VBE_MEM) >> 12   ; el marco y la copia de lo ya pintado
REG_BIOS        equ 9000h       ; el registro de la BIOS de modo real (zona REG)
REG_BIOS_TAM    equ 0FF0h
LIN_MAX         equ 8192        ; puntos de la línea de trabajo (linea_buf)
T_LENTO         equ 100000      ; microsegundos: pintar la zona entera no debe pasar de aquí
EBDA            equ 8FA0h       ; en la BIOS: las entradas de ACPI (+10h) y SMBIOS (+40h)
PRESTA_MAX      equ 4           ; páginas del firmware que se prestan a DOS, como mucho
ST_NTABLAS      equ 104         ; NumberOfTableEntries
ST_TABLAS       equ 112         ; ConfigurationTable (GUID de 16 octetos y puntero)
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
        call seguro_mirar               ; ¿se ha pulsado S? -> modo seguro
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
        mov r15d, [rbx]                 ; R15 = base de la BIOS
        mov [rel bios_base], r15
        mov [rel bios_cual], rbx
        call paginas_prestar            ; las del firmware que hay por debajo
        cmp byte [rel dada + PAG_ARRANQUE], 0
        je .sin_arranque
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
        mov rax, [rel reg_n]            ; (de aquí en adelante, también al
        mov [rel reg_marca], rax        ; registro de la BIOS: registro_a_bios)
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
        mov al, [rel presta_n]          ; T_INT_*: las páginas prestadas
        mov [r15 + 229], al
        mov eax, [rel presta_guarda]
        mov [r15 + 232], eax
        mov rax, [rel presta_dir]
        mov [r15 + 236], rax
        mov rax, [rel presta_dir + 8]
        mov [r15 + 244], rax
        call tablas_copiar              ; ACPI y SMBIOS, a la vista del modo real
        mov rax, [rel bajo_tope]        ; T_MEMKB: hasta la primera página ocupada
        shr eax, 10
        mov [r15 + 28], ax
        call memoria_ext                ; T_EXTKB
        mov [r15 + 32], eax
        mov r12d, eax
        call tsc_calibrar               ; T_TSC_TICK
        mov [r15 + 36], eax
        mov [rel tsc_tic], eax
        mov r13d, eax
        call vbe_preparar               ; T_VBE_*: el marco lineal por debajo de 4 GB
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
        mov byte [rel callado], 1       ; (solo al registro)
        call gop_modos                  ; los modos que da el firmware
        call wc_preparar                ; la pantalla con escritura combinada y su velocidad
%ifndef PANTALLA_NATIVA
        ; si aun así es lenta, un modo más pequeño del firmware, si lo da (el
        ; equipo lo amplía a toda la pantalla y hay menos puntos que escribir)
        cmp dword [rel t_pant], T_LENTO
        jbe .pant_ya
        cmp byte [rel seguro], 0
        jne .pant_ya
        call gop_menor
        jc .pant_ya
        call gop_buscar
        mov ecx, 0FFFFh
        mov eax, [rel fb_ancho]
        cmp eax, ecx
        cmova eax, ecx
        mov [r15 + 188], ax             ; T_VBE_AN
        mov eax, [rel fb_alto]
        cmp eax, ecx
        cmova eax, ecx
        mov [r15 + 190], ax             ; T_VBE_AL
        DI t_g_cambio
        mov eax, [rel fb_ancho]
        call decimal
        DI t_por
        mov eax, [rel fb_alto]
        call decimal
        DI t_gop_l
        mov eax, [rel fb_linea]
        call decimal
        DI t_gop_d
        mov rax, [rel fb]
        call hex64
        DI t_nl
        call wc_preparar
.pant_ya:
%endif
        call reloj_mirar                ; el 8254, el HPET y los APIC: T_PIT, T_HPET...
        call registro_a_bios            ; (lo de la pantalla, también para HBLOG)
        mov byte [rel callado], 0
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
; ---- seguro_mirar: el modo seguro ----
; Si al arrancar se va pulsando la tecla S, HispaBIOS no hace nada de lo que
; depende del equipo y podría sentarle mal a algún firmware: ni el alias de
; escritura combinada, ni cambiar de modo de pantalla, ni prestar a DOS las
; páginas bajas del firmware. (Con -DPRUEBA_SEGURO, siempre.)
seguro_mirar:
        push rbx
        sub rsp, 32                     ; (8 + 8 + 32 = 48: alineada)
        DI t_seguro_s
%ifdef PRUEBA_SEGURO
        mov byte [rel seguro], 1
%endif
        mov rax, [rel bootserv]
        mov ecx, 400000                 ; 0,4 s para que dé tiempo a la tecla
        FW [rax + BS_STALL]
        mov ebx, 32                     ; se leen las teclas que haya
.otra:  mov rax, [rel sistema]
        mov rcx, [rax + ST_CONIN]
        test rcx, rcx
        jz .fin
        lea rdx, [rel tecla_efi]
        FW [rcx + 8]                    ; ReadKeyStroke
        test rax, rax
        jnz .fin
        mov ax, [rel tecla_efi + 2]     ; el carácter
        or al, 20h
        cmp ax, 's'
        jne .sig
        mov byte [rel seguro], 1
.sig:   dec ebx
        jnz .otra
.fin:   cmp byte [rel seguro], 0
        je .f
        DI t_seguro
.f:     add rsp, 32
        pop rbx
        ret

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

; =============================================================================
;  reloj_mirar: el reloj de DOS (IRQ 0) en este equipo
; =============================================================================
;  DOS vive del 8254 (PIT) por la IRQ 0. En los equipos de Intel recientes el
;  firmware lo deja PARADO («8254 clock gating»: Windows y Linux usan el HPET
;  o el APIC) y entonces no cuenta ni interrumpe: Hispa-DOS se quedaba a la
;  mitad del arranque (Monovate 14.1, Core i3-1115G4). Aquí:
;   1. se mira si el contador del canal 0 se mueve;
;   2. si no, y el puente es un PCH de Intel, se le quita el bloqueo: registro
;      ITSSPRC (desplazamiento 3300h del puerto ITSS, C4h, de la banda lateral
;      del PCH), bit 2 = 8254CGE. Los registros de la banda lateral están en
;      SBREG_BAR, la BAR 10h del dispositivo P2SB (00:1F.1), que el firmware
;      deja oculto (bit 8 de su registro E0h): se muestra, se lee la BAR y se
;      vuelve a ocultar. (coreboot: soc/intel/common/block/itss y p2sb; Linux:
;      drivers/platform/x86/p2sb.c; hojas de datos de los PCH de Intel de las
;      series 100 a 700, vol. 2, «ITSS Power Reduction Control».)
;   3. se apunta lo que hay (tablas HPET y APIC de ACPI, el HPET, el APIC local
;      y el IOAPIC) y se pasa a la BIOS la dirección del HPET y del IOAPIC: es
;      ella la que prueba, con las interrupciones abiertas, qué fuente da la
;      IRQ 0 (bios64.asm, reloj_elegir).
;  En el modo seguro no se toca nada de eso y la BIOS solo prueba el 8254 y
;  el reloj CMOS por LINT0.
;  Los registros que no estén en las tablas de páginas del firmware no se
;  tocan (mapeada). R15 = la BIOS.
P2SB            equ (31 << 11) | (1 << 8)   ; 00:1F.1 en CF8h
LPC             equ (31 << 11)              ; 00:1F.0
ITSSPRC         equ (0C4h << 16) + 3300h    ; puerto ITSS, registro ITSSPRC
reloj_mirar:
        push rbx
        push r12
        push r13
        push r14
        sub rsp, 40                     ; (8 + 32 + 40 = 80: pila alineada)
        mov al, [rel callado]
        mov [rel rj_callado], al
        mov byte [rel callado], 1       ; (solo al registro)
%ifdef PRUEBA_RELOJ
        mov byte [r15 + 255], PRUEBA_RELOJ      ; T_PRUEBA
%endif
        cmp byte [rel seguro], 0
        je .mirar
        or byte [r15 + 255], 2Eh        ; modo seguro: solo el 8254 y el reloj CMOS
                                        ; por LINT0 (puertos de siempre)
        DI t_k_seguro
        jmp .fin
.mirar:
        ; ---- 1. ¿cuenta el 8254? ----
        call pit_cuenta
        DI t_k_pit
        call pit_decir
        jc .parado
        or byte [r15 + 254], 3          ; T_PIT: contaba y cuenta
        jmp .tablas
.parado:
        ; ---- 2. el bloqueo del 8254 en el PCH de Intel ----
        call itss_abrir
        jc .tablas
%ifdef PRUEBA_P2SB
        DI t_sim                        ; (pruebas) cómo ha quedado el P2SB de mentira
        movzx eax, byte [rel sim_oculto]
        call decimal
        DI t_sim2
        movzx eax, byte [rel sim_cambios]
        call decimal
        DI t_sim3
        mov eax, [rel sim_prc]
        mov ecx, 8
        call hex
        DI t_nl
%endif
        call pit_cuenta
        DI t_k_pit2
        call pit_decir
        jc .tablas
        or byte [r15 + 254], 6          ; T_PIT: cuenta ahora, tras quitar el bloqueo
.tablas:
        ; ---- 3. lo que hay: HPET, APIC local, IOAPIC ----
        mov eax, 'HPET'
        call acpi_tabla
        test rax, rax
        jnz .hpet
        DI t_k_sin_hpet
        jmp .apic
.hpet:  mov rbx, [rax + 44]             ; la dirección (en +40, el tipo de espacio)
        mov r12d, [rax + 36]            ; el identificador del bloque
        DI t_k_hpet
        mov rax, rbx
        call hex64
        DI t_k_hpet_id
        mov eax, r12d
        mov ecx, 8
        call hex
        mov rax, rbx
        shr rax, 32
        jnz .hpet_alto
        test rbx, rbx
        jz .hpet_nl
        mov [r15 + 256], ebx            ; T_HPET
        mov rax, rbx
        call mapeada
        jc .hpet_nomap
        DI t_k_hpet_cap
        mov rax, [rbx]
        call hex64
        DI t_k_hpet_cfg
        mov eax, [rbx + 10h]
        mov ecx, 8
        call hex
        DI t_k_hpet_t0
        mov rax, [rbx + 100h]
        call hex64
        jmp .hpet_nl
.hpet_alto:
        DI t_k_alto
        jmp .hpet_nl
.hpet_nomap:
        DI t_k_nomap
.hpet_nl:
        DI t_nl
.apic:  mov ecx, 1Bh                    ; IA32_APIC_BASE
        rdmsr
        shl rdx, 32
        or rax, rdx
        mov r12, rax
        DI t_k_lapic
        mov rax, r12
        call hex64
        bt r12, 11
        jnc .lapic_nl
        bt r12, 10
        jc .x2
        mov rbx, r12
        mov rax, 0FFFFFF000h
        and rbx, rax
        mov rax, rbx
        call mapeada
        jc .lapic_nomap
        mov r13d, [rbx + 0F0h]          ; SVR, LINT0, LINT1, temporizador
        mov r14d, [rbx + 350h]
        mov eax, [rbx + 360h]
        mov [rsp + 32], eax
        mov r12d, [rbx + 320h]
        jmp .lapic_decir
.x2:    DI t_k_x2
        mov ecx, 80Fh
        rdmsr
        mov r13d, eax
        mov ecx, 835h
        rdmsr
        mov r14d, eax
        mov ecx, 836h
        rdmsr
        mov [rsp + 32], eax
        mov ecx, 832h
        rdmsr
        mov r12d, eax
.lapic_decir:
        DI t_k_svr
        mov eax, r13d
        mov ecx, 8
        call hex
        DI t_k_lint0
        mov eax, r14d
        call hex
        DI t_k_lint1
        mov eax, [rsp + 32]
        call hex
        DI t_k_lvtt
        mov eax, r12d
        call hex
        jmp .lapic_nl
.lapic_nomap:
        DI t_k_nomap
.lapic_nl:
        DI t_nl
        ; la tabla APIC (MADT): el IOAPIC y a qué entrada va la IRQ 0
        mov eax, 'APIC'
        call acpi_tabla
        test rax, rax
        jnz .madt
        DI t_k_sin_madt
        jmp .fin
.madt:  mov rbx, rax
        DI t_k_madt
        mov eax, [rbx + 40]
        mov ecx, 8
        call hex
        mov r12d, [rbx + 4]             ; R12 = fin de la tabla
        add r12, rbx
        lea r13, [rbx + 44]
        xor r14d, r14d                  ; R14 = el IOAPIC de la entrada 0
.e:     lea rax, [r13 + 2]
        cmp rax, r12
        ja .madt_f
        movzx eax, byte [r13 + 1]
        cmp eax, 2
        jb .madt_f
        cmp byte [r13], 1               ; IOAPIC
        jne .e2
        DI t_k_ioapic
        mov eax, [r13 + 4]
        mov ecx, 8
        call hex
        DI t_k_gsi
        mov eax, [r13 + 8]
        call decimal
        cmp dword [r13 + 8], 0
        jne .es
        mov r14d, [r13 + 4]
        jmp .es
.e2:    cmp byte [r13], 2               ; una IRQ que va a otra entrada
        jne .es
        cmp byte [r13 + 3], 0
        jne .es
        DI t_k_irq0
        mov eax, [r13 + 4]
        call decimal
.es:    movzx eax, byte [r13 + 1]
        add r13, rax
        jmp .e
.madt_f:
        test r14, r14
        jz .madt_nl
        mov [r15 + 260], r14d           ; T_IOAPIC
        mov rax, r14
        call mapeada
        jc .ioa_nomap
        pushfq                          ; sus entradas 0 (el 8259) y 2 (la IRQ 0)
        cli
        mov dword [r14], 10h
        mov ebx, [r14 + 10h]
        mov dword [r14], 14h
        mov r12d, [r14 + 10h]
        popfq
        DI t_k_ioa0
        mov eax, ebx
        mov ecx, 8
        call hex
        DI t_k_ioa2
        mov eax, r12d
        call hex
        jmp .madt_nl
.ioa_nomap:
        DI t_k_nomap
.madt_nl:
        DI t_nl
.fin:   mov al, [rel rj_callado]
        mov [rel callado], al
        add rsp, 40
        pop r14
        pop r13
        pop r12
        pop rbx
        ret

; pit_cuenta: programa el canal 0 del 8254 como lo quiere DOS (modo 3, 65536
; cuentas: 18,2 Hz) y lee el contador dos veces con 2 ms entre medias -> EAX =
; primera lectura, EDX = segunda; CF si no se ha movido
pit_cuenta:
        push rbx
        push rcx
        pushfq
        cli
        mov al, 36h
        out 43h, al
        xor al, al
        out 40h, al
        out 40h, al
        popfq
        call .espera
        call .leer
        mov [rel pit_l1], eax
        call .espera
        call .leer
        mov [rel pit_l2], eax
        mov edx, eax
        mov eax, [rel pit_l1]
        cmp eax, edx
        clc
        jne .f
        stc
.f:     pop rcx
        pop rbx
        ret
.leer:  pushfq
        cli
        xor eax, eax
        out 43h, al                     ; (la cuenta del canal 0, retenida)
        in al, 40h
        mov ah, al
        in al, 40h
        xchg al, ah
        popfq
        ret
.espera:                                ; 2 ms por el TSC (o unas vueltas, sin él)
        push rax
        push rdx
        mov ecx, [rel tsc_tic]
        shr ecx, 5
        jnz .t
        mov ecx, 2000000
.v:     pause
        dec ecx
        jnz .v
        jmp .ef
.t:     rdtsc
        shl rdx, 32
        or rax, rdx
        mov rbx, rax
.te:    pause
        rdtsc
        shl rdx, 32
        or rax, rdx
        sub rax, rbx
        cmp rax, rcx
        jb .te
.ef:    pop rdx
        pop rax
        ret
; pit_decir: apunta «cuenta» o «NO cuenta» con las dos lecturas -> CF si no cuenta
pit_decir:
        mov eax, [rel pit_l1]
        cmp eax, [rel pit_l2]
        jne .si
        DI t_k_no
        jmp .l
.si:    DI t_k_si
.l:     mov eax, [rel pit_l1]
        mov ecx, 4
        call hex
        DI t_k_y
        mov eax, [rel pit_l2]
        call hex
        DI t_k_fin
        mov eax, [rel pit_l1]
        cmp eax, [rel pit_l2]
        clc
        jne .f
        stc
.f:     ret

; pci_leer: EAX = dispositivo y registro (bus 0) -> EAX = la doble palabra
pci_leer:
%ifdef PRUEBA_P2SB
        ; (pruebas) un P2SB de mentira, oculto, con el bit 8254CGE puesto en un
        ; ITSSPRC de mentira (sim_prc): QEMU no tiene PCH con banda lateral
        push rax
        and eax, 0FF00h
        cmp eax, P2SB
        pop rax
        jne .real
        cmp byte [rel sim_oculto], 0
        jne .sim_ff
        cmp al, 00h
        je .sim_id
        cmp al, 08h
        je .sim_clase
        cmp al, 10h
        je .sim_bar
        cmp al, 14h
        je .sim_bar2
        xor eax, eax
        ret
.sim_ff:
        mov eax, -1
        ret
.sim_id:
        mov eax, 0A0A08086h
        ret
.sim_clase:
        mov eax, 05800020h
        ret
.sim_bar:
        lea rax, [rel sim_prc]
        sub rax, ITSSPRC
        and eax, 0FFFFFFF0h
        or eax, 4
        ret
.sim_bar2:
        lea rax, [rel sim_prc]
        sub rax, ITSSPRC
        shr rax, 32
        ret
.real:
%endif
        push rdx
        pushfq
        cli
        or eax, 80000000h
        and al, 0FCh
        mov dx, 0CF8h
        out dx, eax
        mov dx, 0CFCh
        in eax, dx
        popfq
        pop rdx
        ret
; pci_poner8: EAX = dispositivo y registro, CL = el octeto
pci_poner8:
%ifdef PRUEBA_P2SB
        cmp eax, P2SB + 0E1h
        jne .real
        mov [rel sim_oculto], cl
        inc byte [rel sim_cambios]
        ret
.real:
%endif
        push rax
        push rbx
        push rdx
        pushfq
        cli
        mov ebx, eax
        or eax, 80000000h
        and al, 0FCh
        mov dx, 0CF8h
        out dx, eax
        mov edx, ebx
        and edx, 3
        add dx, 0CFCh
        mov al, cl
        out dx, al
        popfq
        pop rdx
        pop rbx
        pop rax
        ret

; itss_abrir: quita el «clock gating» del 8254 en un PCH de Intel -> CF = 0 si
; el bit estaba puesto y se ha quitado. Apunta cada cosa que ve.
itss_abrir:
        push rbx
        push r12
        push r13
        push r14
        sub rsp, 40
        xor r14d, r14d                  ; R14 = 1: hay que volver a ocultar el P2SB
        mov eax, LPC
        call pci_leer
        mov r12d, eax
        DI t_i_lpc
        mov eax, r12d
        mov ecx, 8
        call hex
        cmp r12w, 8086h
        je .intel
        DI t_i_no_intel
        jmp .no
.intel: mov eax, P2SB
        call pci_leer
        mov r12d, eax                   ; R12 = identificador del P2SB
        mov eax, P2SB + 0E0h
        call pci_leer
        mov r13d, eax
        DI t_i_p2sb
        mov eax, r12d
        mov ecx, 8
        call hex
        DI t_i_e0
        mov eax, r13d
        call hex
        cmp r12d, -1
        jne .visible
        mov eax, P2SB + 0E1h            ; oculto: se muestra (bit 8 de E0h a 0)
        xor ecx, ecx
        call pci_poner8
        mov r14d, 1
        mov eax, P2SB
        call pci_leer
        mov r12d, eax
        DI t_i_mostrado
        mov eax, r12d
        mov ecx, 8
        call hex
        cmp r12d, -1
        jne .visible
        DI t_i_no_hay
        jmp .ocultar_no
.visible:
        cmp r12w, 8086h
        jne .raro
        mov eax, P2SB + 08h             ; clase 0580h: memoria (otra)
        call pci_leer
        shr eax, 16
        cmp eax, 0580h
        jne .raro
        mov eax, P2SB + 10h             ; SBREG_BAR (de 64 bits)
        call pci_leer
        mov ebx, eax
        mov eax, P2SB + 14h
        call pci_leer
        mov r13d, eax
        call .ocultar
        test bl, 1                      ; (de E/S: no es)
        jnz .sin_bar
        mov eax, ebx
        and eax, 6
        cmp eax, 4                      ; ¿BAR de 64 bits?
        je .b64
        xor r13d, r13d
.b64:   and ebx, 0FFFFFFF0h
        shl r13, 32
        or rbx, r13
        DI t_i_bar
        mov rax, rbx
        call hex64
        test rbx, rbx
        jz .sin_bar
        lea rax, [rbx + ITSSPRC]
        call mapeada
        jc .nomap
        mov r12d, [rbx + ITSSPRC]
        DI t_i_prc
        mov eax, r12d
        mov ecx, 8
        call hex
        cmp r12d, -1
        je .sin_bar
        test r12d, 4                    ; 8254CGE
        jnz .quitar
        DI t_i_ya
        jmp .no
.quitar:
        and r12d, ~4
        mov [rbx + ITSSPRC], r12d
        mov r12d, [rbx + ITSSPRC]
        DI t_i_quitado
        mov eax, r12d
        mov ecx, 8
        call hex
        DI t_nl
        test r12d, 4
        jnz .no_nl
        clc
        jmp .fin
.nomap: DI t_k_nomap
        jmp .no
.sin_bar:
        DI t_i_sin_bar
        jmp .no
.raro:  DI t_i_raro
.ocultar_no:
        call .ocultar
.no:    DI t_nl
.no_nl: stc
.fin:   lea rsp, [rsp + 40]
        pop r14
        pop r13
        pop r12
        pop rbx
        ret
.ocultar:
        test r14d, r14d
        jz .of
        push rax
        push rcx
        mov eax, P2SB + 0E1h
        mov ecx, 1
        call pci_poner8
        pop rcx
        pop rax
        xor r14d, r14d
.of:    ret

; mapeada: RAX = dirección -> CF = 1 si no está en las tablas de páginas del
; firmware (cuatro niveles; con cinco no se mira y se da por buena)
mapeada:
        push rbx
        push rcx
        push rdx
        mov rdx, cr4
        bt rdx, 12                      ; LA57
        jc .si
        mov rcx, 000FFFFFFFFFF000h
        mov rbx, cr3
        and rbx, rcx
        mov rdx, rax
        shr rdx, 39
        and edx, 1FFh
        mov rbx, [rbx + rdx * 8]
        test bl, 1
        jz .no
        and rbx, rcx
        mov rdx, rax
        shr rdx, 30
        and edx, 1FFh
        mov rbx, [rbx + rdx * 8]
        test bl, 1
        jz .no
        test bl, 80h                    ; página de 1 GB
        jnz .si
        and rbx, rcx
        mov rdx, rax
        shr rdx, 21
        and edx, 1FFh
        mov rbx, [rbx + rdx * 8]
        test bl, 1
        jz .no
        test bl, 80h                    ; página de 2 MB
        jnz .si
        and rbx, rcx
        mov rdx, rax
        shr rdx, 12
        and edx, 1FFh
        mov rbx, [rbx + rdx * 8]
        test bl, 1
        jz .no
.si:    clc
        jmp .f
.no:    stc
.f:     pop rdx
        pop rcx
        pop rbx
        ret

; acpi_tabla: EAX = firma -> RAX = la tabla de ACPI con esa firma (0: no hay)
acpi_tabla:
        push rbx
        push rcx
        push rdx
        push rsi
        mov edx, eax
        mov rbx, [rel rsdp]
        test rbx, rbx
        jz .no
        cmp byte [rbx + 15], 2          ; ACPI 2.0: la XSDT (punteros de 64 bits)
        jb .rsdt
        mov rsi, [rbx + 24]
        test rsi, rsi
        jz .rsdt
        mov ecx, [rsi + 4]
        sub ecx, 36
        jbe .no
        shr ecx, 3
        add rsi, 36
.x:     mov rax, [rsi]
        test rax, rax
        jz .xs
        cmp [rax], edx
        je .f
.xs:    add rsi, 8
        dec ecx
        jnz .x
        jmp .no
.rsdt:  mov esi, [rbx + 16]
        test rsi, rsi
        jz .no
        mov ecx, [rsi + 4]
        sub ecx, 36
        jbe .no
        shr ecx, 2
        add rsi, 36
.r:     mov eax, [rsi]
        test rax, rax
        jz .rs
        cmp [rax], edx
        je .f
.rs:    add rsi, 4
        dec ecx
        jnz .r
.no:    xor eax, eax
.f:     pop rsi
        pop rdx
        pop rcx
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
        cmp ecx, 8                      ; modo de pantalla: texto (0) o VBE (1...)
        je .modo
        cmp ecx, 9                      ; lo apuntado por la BIOS, a HISPABIOS.LOG
        je .registro
        mov eax, -1                     ; (0: un viaje sin más)
        jmp .fin
.registro:
        ; EDX = desde dónde (desplazamiento en el registro de la BIOS); bit 16:
        ; guardarlo ya en la memoria USB (algo va mal)
        mov ebx, edx
        mov rdi, [rel bios_base]
        movzx ecx, word [rdi + 194]     ; T_REG_N
        movzx esi, bx
        cmp ecx, REG_BIOS_TAM
        ja .reg_f
        sub ecx, esi
        jbe .reg_f
        lea rsi, [rdi + rsi + REG_BIOS]
        mov dil, [rel callado]
        mov byte [rel callado], 1       ; (solo al registro: la pantalla es de DOS)
        sti
.reg_c: mov al, [rsi]
        mov [rel buf_car], al
        push rsi
        lea rsi, [rel buf_car]
        call escribir
        pop rsi
        inc rsi
        dec ecx
        jnz .reg_c
        mov [rel callado], dil
        bt ebx, 16
        jnc .reg_p
        call registro_guardar
        jmp .reg_g
.reg_p: call registro_paso
.reg_g: cli
.reg_f: xor eax, eax
        jmp .fin
.modo:
        call modo_poner
        xor eax, eax
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

; =============================================================================
;  La pantalla
; =============================================================================
;  La pantalla del firmware (GOP) es un marco de 32 bits por punto que puede
;  estar por encima de los 4 GB. Escribir en él es lento si su memoria no tiene
;  «escritura combinada» (WC): el firmware la suele dejar sin caché (UC) y cada
;  punto escrito va al bus por separado; pintar 1920x1200 así cuesta del orden
;  de un segundo (el «1 FPS» del ThinkPad E14 y del MacBook Air de 2017). Por
;  eso:
;    - se pinta por un ALIAS del marco con escritura combinada: unas tablas de
;      páginas propias (copia del nivel 4 del firmware más una entrada con el
;      marco en páginas de 2 MB de tipo WC, por el PAT), que solo se usan
;      mientras se dibuja (wc_preparar);
;    - se mide lo que cuesta pintar la pantalla entera, directamente y por el
;      alias, se usa el camino más rápido y queda en el registro;
;    - si aun así es lenta, se amplía menos (un múltiplo entero de 640x400):
;      menos puntos que escribir (escala_tope);
;    - solo se pintan las celdas que han cambiado, cada fila de puntos se
;      prepara en memoria normal (linea_buf) y va a la pantalla de una vez.
;  Los modos gráficos (VBE) se dan a los programas en un marco de memoria
;  normal por debajo de 4 GB (vbe_lfb); en cada tic se llevan a la pantalla
;  las líneas que han cambiado (graf_presentar).
;  Referencias: Intel SDM vol. 3, 4.5 (paginación), 11.11 (MTRR) y 11.12 (PAT).

; ---- vbe_preparar: la memoria del marco lineal de VBE (por debajo de 4 GB) ----
vbe_preparar:
        push rbx
        sub rsp, 32
        mov byte [rel callado], 1
        mov rax, 0FFFFFFFFh
        mov [rel dir_pag], rax
        mov rax, [rel bootserv]
        mov ecx, ALLOC_MAX
        mov edx, MEM_LOADERDATA
        mov r8d, VBE_PAGS
        lea r9, [rel dir_pag]
        FW [rax + BS_ALLOCPAGES]
        test rax, rax
        jnz .no
        mov rax, [rel dir_pag]
        mov [rel vbe_lfb], rax
        mov rbx, [rel bios_base]
        mov [rbx + 184], eax            ; T_VBE_LFB
        add rax, VBE_MEM
        mov [rel vbe_ant], rax
        mov ecx, 0FFFFh
        mov eax, [rel fb_ancho]
        cmp eax, ecx
        cmova eax, ecx
        mov [rbx + 188], ax             ; T_VBE_AN
        mov eax, [rel fb_alto]
        cmp eax, ecx
        cmova eax, ecx
        mov [rbx + 190], ax             ; T_VBE_AL
        DI t_v_lfb
        mov rax, [rel vbe_lfb]
        call hex64
        DI t_v_lfb2
        jmp .f
.no:    DI t_v_no
.f:     mov byte [rel callado], 0
        add rsp, 32
        pop rbx
        ret

; ---- gop_modos: los modos de pantalla que da el firmware, al registro ----
gop_modos:
        push rbx
        push r12
        push r13
        sub rsp, 48                     ; (8 + 24 + 48 = 80: pila alineada)
        DI t_g_modos
        mov rbx, [rel gop]
        mov rax, [rbx + 24]             ; Mode
        mov r13d, [rax]                 ; MaxMode
        xor r12d, r12d
.m:     cmp r12d, r13d
        jae .f
        cmp r12d, 40
        jae .mas
        mov rcx, rbx
        mov edx, r12d
        lea r8, [rsp + 32]
        lea r9, [rsp + 40]
        FW [rbx]                        ; QueryMode
        test rax, rax
        jnz .sig
        test r12d, r12d
        jz .p
        DI t_g_coma
.p:     mov r8, [rsp + 40]
        mov eax, [r8 + 4]
        call decimal
        DI t_por
        mov eax, [r8 + 8]
        call decimal
        mov rax, [rbx + 24]
        cmp r12d, [rax + 4]
        jne .sig
        DI t_g_ast
.sig:   inc r12d
        jmp .m
.mas:   DI t_g_mas
.f:     DI t_nl
        add rsp, 48
        pop r13
        pop r12
        pop rbx
        ret

; ---- gop_menor: con la pantalla lenta, pone el modo del firmware de 800x600 o
;      más (32 bits) que se pinte entero en T_LENTO (el mayor que lo cumpla o,
;      si ninguno, el más pequeño). CF si se queda el que había. ----
gop_menor:
        push rbx
        push r12
        push r13
        push r14
        push r15
        sub rsp, 64                     ; (8 + 40 + 64 = 112: pila alineada)
        mov rbx, [rel gop]
        mov rax, [rbx + 24]             ; Mode
        mov r13d, [rax]                 ; MaxMode
        mov r14d, -1                    ; R14 = el elegido, R15 = sus puntos
        xor r15d, r15d
        mov byte [rsp + 56], 0          ; 1: el elegido se pinta en T_LENTO
        xor r12d, r12d
.m:     cmp r12d, r13d
        jae .elegido
        mov rcx, rbx
        mov edx, r12d
        lea r8, [rsp + 32]
        lea r9, [rsp + 40]
        FW [rbx]                        ; QueryMode
        test rax, rax
        jnz .sig
        mov r8, [rsp + 40]
        cmp dword [r8 + 12], 1          ; color de 32 bits
        ja .sig
        mov eax, [r8 + 4]
        cmp eax, 800
        jb .sig
        cmp eax, LIN_MAX
        ja .sig
        mov ecx, [r8 + 8]
        cmp ecx, 600
        jb .sig
        imul rax, rcx                   ; RAX = puntos del modo
        mov ecx, [rel fb_ancho]
        mov edx, [rel fb_alto]
        imul rcx, rdx                   ; RCX = los del modo puesto
        cmp rax, rcx
        jae .sig
        mov r9, rax
        mov edx, [rel t_pant]
        mul rdx
        div rcx                         ; RAX = lo que costaría pintarlo
        cmp rax, T_LENTO
        setbe dl                        ; DL = 1: cumple
        cmp r14d, -1
        je .toma
        test dl, dl
        jz .no_cumple
        cmp byte [rsp + 56], 0          ; cumple: mejor que uno que no, o el mayor
        je .toma
        cmp r9, r15
        ja .toma
        jmp .sig
.no_cumple:
        cmp byte [rsp + 56], 0          ; no cumple: solo si es más pequeño que
        jne .sig                        ; otro que tampoco
        cmp r9, r15
        jae .sig
.toma:  mov r14d, r12d
        mov r15, r9
        mov [rsp + 56], dl
.sig:   inc r12d
        jmp .m
.elegido:
        cmp r14d, -1
        je .no
        mov rcx, rbx
        mov edx, r14d
        FW [rbx + 8]                    ; SetMode
        test rax, rax
        jnz .no
        clc
        jmp .f
.no:    stc
.f:     lea rsp, [rsp + 64]
        pop r15
        pop r14
        pop r13
        pop r12
        pop rbx
        ret

; ---- wc_preparar: el alias de la pantalla con escritura combinada, la medida
;      de la velocidad y la ampliación del texto; todo al registro ----
wc_preparar:
        push rbx
        push r12
        push r13
        push r14
        push r15
        sub rsp, 32                     ; (8 + 40 + 32 = 80: pila alineada)
        mov rax, [rel fb]
        mov [rel fbv], rax
        mov qword [rel wc_cr3], 0
        mov byte [rel wc_usar], 0
        ; el tipo de caché que tiene la pantalla según los MTRR
        DI t_w_mtrr
        mov ecx, 2FFh                   ; IA32_MTRR_DEF_TYPE
        rdmsr
        and eax, 0FFFh
        mov ecx, 3
        call hex
        DI t_w_mtrr2
        mov ecx, 0FEh                   ; IA32_MTRRCAP: cuántos variables
        rdmsr
        movzx r13d, al
        xor r14d, r14d
.v:     cmp r14d, r13d
        jae .ninguno
        lea ecx, [r14 * 2 + 201h]       ; máscara
        rdmsr
        shl rdx, 32
        or rax, rdx
        mov rbx, rax
        bt rbx, 11                      ; ¿en uso?
        jnc .vs
        lea ecx, [r14 * 2 + 200h]       ; base y tipo
        rdmsr
        shl rdx, 32
        or rax, rdx
        mov r15, rax
        and rbx, -4096
        mov rdx, [rel fb]
        and rdx, rbx
        and rax, -4096
        and rax, rbx
        cmp rax, rdx
        jne .vs
        DI t_w_var
        mov eax, r14d
        call decimal
        DI t_w_tipo
        movzx eax, r15b
        and eax, 7
        lea rsi, [rel t_w_tipos]
        lea rsi, [rsi + rax * 4]
        call escribir
        jmp .pat
.vs:    inc r14d
        jmp .v
.ninguno:
        DI t_w_nomtrr
.pat:   mov eax, 1
        cpuid                           ; (RBX está guardado)
        bt edx, 16                      ; ¿PAT?
        jc .con_pat
        DI t_nl
        DI t_w_no_pat
        jmp .medir
.con_pat:
        DI t_w_pat
        mov ecx, 277h                   ; IA32_PAT
        rdmsr
        shl rdx, 32
        or rax, rdx
        call hex64
        DI t_nl
%ifdef PRUEBA_SIN_WC
        jmp .medir
%endif
        cmp byte [rel seguro], 0
        jne .medir
        mov rax, cr4
        bt rax, 12                      ; LA57: tablas de 5 niveles (no se toca)
        jnc .niveles4
        DI t_w_no_la57
        jmp .medir
.niveles4:
        ; cuántas páginas de 2 MB ocupa el marco
        mov eax, [rel fb_linea]
        mov edx, [rel fb_alto]
        imul rax, rdx
        shl rax, 2
        mov rdx, [rel fb]
        and edx, 1FFFFFh
        add rax, rdx
        add rax, 1FFFFFh
        shr rax, 21
        cmp rax, 512
        ja .no_mem
        mov r13, rax
        ; tres páginas: copia del nivel 4, y los niveles 3 y 2 del alias
        mov rax, [rel bootserv]
        mov ecx, ALLOC_ANY
        mov edx, MEM_LOADERDATA
        mov r8d, 3
        lea r9, [rel dir_pag]
        FW [rax + BS_ALLOCPAGES]
        test rax, rax
        jnz .no_mem
        cld
        mov rbx, [rel dir_pag]
        mov rdi, rbx
        mov ecx, 3 * 512
        xor eax, eax
        rep stosq
        mov rsi, cr3
        and rsi, -4096
        mov rdi, rbx
        mov ecx, 512
        rep movsq
        mov ecx, 255                    ; una entrada libre de la mitad baja
.libre: cmp qword [rbx + rcx * 8], 0
        je .hay
        dec ecx
        jnz .libre
        jmp .no_mem
.hay:   mov r14d, ecx
        lea rax, [rbx + 1000h]
        or rax, 3                       ; presente, se puede escribir
        mov [rbx + rcx * 8], rax
        lea rax, [rbx + 2000h]
        or rax, 3
        mov [rbx + 1000h], rax
        mov rax, [rel fb]
        and rax, -200000h
        or rax, 108Bh                   ; presente, escribir, 2 MB, PAT = 1 y PWT = 1:
        lea rdi, [rbx + 2000h]          ; la entrada 5 del PAT
        mov rcx, r13
.pd:    stosq
        add rax, 200000h
        dec rcx
        jnz .pd
        mov ecx, 277h                   ; la entrada 5 del PAT, de WT a WC (01h);
        rdmsr                           ; el firmware no usa el bit PAT de sus
        and edx, 0FFFF00FFh             ; tablas, así que a él no le cambia nada
        or edx, 00000100h
        wrmsr
        shl rdx, 32
        mov eax, eax
        or rax, rdx
        mov r15, rax
        mov [rel wc_cr3], rbx
        mov rax, r14
        shl rax, 39
        mov rdx, [rel fb]
        and edx, 1FFFFFh
        or rax, rdx
        mov [rel wc_alias], rax
        DI t_w_alias
        call hex64
        DI t_w_alias2
        mov rax, rbx
        call hex64
        DI t_w_alias3
        mov eax, r14d
        call decimal
        DI t_w_alias4
        mov rax, r15
        call hex64
        DI t_nl
        jmp .medir
.no_mem:
        DI t_w_no_mem
.medir:
        ; lo que cuesta pintar la pantalla entera (en negro) por cada camino
        pushfq
        cli
        mov rdi, [rel fb]
        call medir_borrado
        mov r12d, eax
        xor r13d, r13d
        cmp qword [rel wc_cr3], 0
        je .medido
        mov rbx, cr3
        mov rax, [rel wc_cr3]
        mov cr3, rax
        mov rdi, [rel wc_alias]
        call medir_borrado
        mov r13d, eax
        mov cr3, rbx
.medido:
        popfq
        mov [rel t_pant], r12d
        cmp qword [rel wc_cr3], 0
        je .log
        mov eax, r12d                   ; por el alias, salvo que salga claramente
        shr eax, 2                      ; más lento (un cuarto más): en un equipo
        add eax, r12d                   ; de verdad el alias es mucho más rápido o,
        cmp r13d, eax                   ; si la pantalla ya tenía WC, igual
        ja .log
        mov byte [rel wc_usar], 1
        mov rax, [rel wc_alias]
        mov [rel fbv], rax
        mov [rel t_pant], r13d
.log:
%ifdef PRUEBA_LENTA
        mov eax, [rel fb_linea]         ; (pruebas) como una pantalla lenta:
        imul eax, [rel fb_alto]         ; PRUEBA_LENTA microsegundos por cada
        mov edx, PRUEBA_LENTA           ; millón de puntos
        mul rdx
        mov ecx, 1000000
        div rcx
        mov [rel t_pant], eax
        mov r12d, eax
        mov r13d, eax
%endif
        DI t_w_medida
        mov eax, r12d
        call decimal
        DI t_w_medida2
        mov eax, r13d
        call decimal
        DI t_w_medida3
        lea rsi, [rel t_w_directo]
        cmp byte [rel wc_usar], 0
        je .cual
        lea rsi, [rel t_w_poralias]
.cual:  call escribir
        DI t_nl
        ; la ampliación del texto
        call escala
        DI t_w_esc
        cmp dword [rel esc_k], 0
        jne .ek
        DI t_w_esc_todo
        jmp .et
.ek:    DI t_w_esc_k
        mov eax, [rel esc_k]
        call decimal
        DI t_w_esc_k2
.et:    mov eax, [rel dib_an]
        call decimal
        DI t_por
        mov eax, [rel dib_al]
        call decimal
        DI t_w_cierra
        add rsp, 32
        pop r15
        pop r14
        pop r13
        pop r12
        pop rbx
        ret

; medir_borrado: pone en negro la pantalla entera escribiendo desde RDI
; -> EAX = microsegundos que ha costado (0 si no hay medida del TSC)
medir_borrado:
        push rcx
        push rdx
        push r8
        rdtsc
        shl rdx, 32
        or rax, rdx
        mov r8, rax
        mov ecx, [rel fb_linea]
        imul ecx, [rel fb_alto]
        xor eax, eax
        cld
        rep stosd
        sfence
        rdtsc
        shl rdx, 32
        or rax, rdx
        sub rax, r8
        mov ecx, [rel tsc_tic]
        test ecx, ecx
        jz .cero
        mov edx, 54925                  ; un tic = 54925 us
        mul rdx
        div rcx
        jmp .f
.cero:  xor eax, eax
.f:     pop r8
        pop rdx
        pop rcx
        ret

; registro_a_bios: lo apuntado desde reg_marca (pantalla, memoria, VBE) se copia
; al registro de la BIOS de modo real, que HBLOG.COM enseña desde DOS
registro_a_bios:
        push rax
        push rcx
        push rsi
        push rdi
        mov rsi, [rel reg_dir]
        test rsi, rsi
        jz .f
        mov rax, [rel reg_marca]
        mov rcx, [rel reg_n]
        sub rcx, rax
        jbe .f
        cmp rcx, REG_BIOS_TAM - 1280    ; (queda sitio para lo de VBE)
        jbe .c
        mov ecx, REG_BIOS_TAM - 1280
.c:     add rsi, rax
        mov rdi, [rel bios_base]
        mov [rdi + 194], cx             ; T_REG_N
        add rdi, REG_BIOS
        cld
        rep movsb
.f:     pop rdi
        pop rsi
        pop rcx
        pop rax
        ret

; escala_tope: EAX = ancho, EDX = alto de lo que hay que enseñar -> ECX = por
; cuánto (entero) se amplía: lo que quepa en la pantalla y en linea_buf y, si
; la pantalla es lenta, lo que se pinte entero en T_LENTO. Cambia RAX y RDX.
escala_tope:
        push rbx
        push rsi
        push rdi
        push r8
        push r9
        mov r8d, eax
        mov r9d, edx
        mov eax, [rel fb_ancho]
        xor edx, edx
        div r8d
        mov ecx, eax
        mov eax, [rel fb_alto]
        xor edx, edx
        div r9d
        cmp eax, ecx
        cmovb ecx, eax
        mov eax, LIN_MAX
        xor edx, edx
        div r8d
        cmp eax, ecx
        cmovb ecx, eax
        cmp ecx, 1
        jae .vel
        mov ecx, 1
.vel:   cmp dword [rel t_pant], T_LENTO
        jbe .f
.prueba:
        cmp ecx, 1
        jbe .f
        mov eax, r8d                    ; lo que costaría: t_pant por la parte
        imul eax, ecx                   ; de la pantalla que se pinta
        mov ebx, r9d
        imul ebx, ecx
        imul rax, rbx
        mov ebx, [rel t_pant]
        imul rax, rbx
        mov esi, [rel fb_linea]
        mov edi, [rel fb_alto]
        imul rsi, rdi
        xor edx, edx
        div rsi
        cmp rax, T_LENTO
        jbe .f
        dec ecx
        jmp .prueba
.f:     pop r9
        pop r8
        pop rdi
        pop rsi
        pop rbx
        ret

; ---- modo_poner: EDX = 0 texto; 1... modo VBE (g_modos), bit 8: sin borrar ----
modo_poner:
        push rbx
        push rsi
        push rdi
        movzx eax, dl
        test eax, eax
        jz .texto
        cmp eax, G_NMODOS
        ja .texto
        cmp qword [rel vbe_lfb], 0
        je .texto
        lea rsi, [rel g_modos]
        mov ecx, [rsi + rax * 8 - 8]
        mov ebx, [rsi + rax * 8 - 4]
        cmp ecx, [rel fb_ancho]
        ja .texto
        cmp ebx, [rel fb_alto]
        ja .texto
        mov [rel graf], al
        mov [rel g_an], ecx
        mov [rel g_al], ebx
        push rdx
        mov eax, ecx
        mov edx, ebx
        call escala_tope
        pop rdx
        mov [rel g_k], ecx
        mov eax, [rel g_an]             ; centrado
        imul eax, ecx
        mov ebx, [rel fb_ancho]
        sub ebx, eax
        shr ebx, 1
        mov [rel g_x0], ebx
        mov eax, [rel g_al]
        imul eax, ecx
        mov ebx, [rel fb_alto]
        sub ebx, eax
        shr ebx, 1
        mov [rel g_y0], ebx
        mov byte [rel g_todo], 1
        test edx, 100h
        jnz .f
        mov rdi, [rel vbe_lfb]          ; el marco, en negro
        mov ecx, [rel g_an]
        imul ecx, [rel g_al]
        xor eax, eax
        cld
        rep stosd
        jmp .f
.texto: mov byte [rel graf], 0
        mov byte [rel borrada], 0       ; (el texto se pinta entero otra vez)
.f:     pop rdi
        pop rsi
        pop rbx
        ret

; =============================================================================
;  La memoria baja que usa el firmware y las tablas del equipo
; =============================================================================
; ---- paginas_prestar: las páginas de memoria baja que el firmware no da ----
; En casi todos los equipos el firmware se queda una o dos páginas sueltas de
; la memoria baja (el arranque de los otros procesadores: 87000h en el ThinkPad
; E14, 58000h en el MacBook Air), y DOS se quedaba sin lo que hay por encima
; (352 KB en el Mac). Esas páginas se PRESTAN: como la página 0, en cada cambio
; de modo se intercambia su contenido (bios64.asm, paginas_cambiar), de manera
; que el firmware ve siempre lo suyo y DOS lo suyo. Solo las que son memoria
; normal del firmware (tipos 0 a 4 del mapa, con atributos de caché); como
; mucho PRESTA_MAX, por debajo de la BIOS de modo real. R15 = base de la BIOS.
paginas_prestar:
        push rbx
        push r12
        push r13
        sub rsp, 32                     ; (8 + 24 + 32 = 64: pila alineada)
%ifdef PRUEBA_SIN_PRESTAR
        jmp .f
%endif
        cmp byte [rel seguro], 0
        jne .f
        mov r12d, 1                     ; R12 = página
        mov r13, r15
        shr r13, 12                     ; R13 = la primera de la BIOS
        lea rbx, [rel dada]
.p:     cmp r12, r13
        jae .hecho
        cmp byte [rbx + r12], 0
        jne .sig
        movzx eax, byte [rel presta_n]
        cmp eax, PRESTA_MAX
        jae .hecho
        mov rax, r12
        shl rax, 12
        call pagina_tipo
        jc .hecho
        cmp eax, 4
        ja .hecho
        test edx, 0Fh                   ; (sin atributos de caché no es memoria)
        jz .hecho
        movzx eax, byte [rel presta_n]
        lea rcx, [rel presta_dir]
        mov rdx, r12
        shl rdx, 12
        mov [rcx + rax * 4], edx
        inc byte [rel presta_n]
        mov byte [rbx + r12], 2
.sig:   inc r12
        jmp .p
.hecho: cmp byte [rel presta_n], 0
        je .f
        mov rax, 0FFFFFFFFh             ; dónde se guarda la otra versión de
        mov [rel dir_pag], rax          ; cada página (por debajo de 4 GB)
        mov rax, [rel bootserv]
        mov ecx, ALLOC_MAX
        mov edx, MEM_LOADERDATA
        mov r8d, PRESTA_MAX
        lea r9, [rel dir_pag]
        FW [rax + BS_ALLOCPAGES]
        test rax, rax
        jnz .sin_mem
        mov rdi, [rel dir_pag]
        mov [rel presta_guarda], edi
        mov ecx, PRESTA_MAX * 512       ; (DOS las ve a cero)
        xor eax, eax
        cld
        rep stosq
        DI t_presta
        xor r12d, r12d
.e:     lea rcx, [rel presta_dir]
        mov eax, [rcx + r12 * 4]
        mov ecx, 5
        call hex
        DI t_espacio
        inc r12d
        cmp r12b, [rel presta_n]
        jb .e
        DI t_nl
        jmp .f
.sin_mem:                               ; sin sitio para guardarlas: no se prestan
        movzx ecx, byte [rel presta_n]
        lea rdx, [rel presta_dir]
.q:     mov eax, [rdx + rcx * 4 - 4]
        shr eax, 12
        mov byte [rbx + rax], 0
        dec ecx
        jnz .q
        mov byte [rel presta_n], 0
.f:     add rsp, 32
        pop r13
        pop r12
        pop rbx
        ret

; pagina_tipo: RAX = dirección -> EAX = tipo en el mapa de memoria y RDX = sus
; atributos; CF si no está en el mapa
pagina_tipo:
        push rbx
        push rcx
        push rsi
        mov rsi, [rel mapa_dir]
        mov rcx, [rel mapa_tam]
        add rcx, rsi
.e:     cmp rsi, rcx
        jae .no
        mov rbx, [rsi + 8]
        cmp rax, rbx
        jb .sig
        mov rdx, [rsi + 24]
        shl rdx, 12
        add rdx, rbx
        cmp rax, rdx
        jae .sig
        mov eax, [rsi]
        mov rdx, [rsi + 32]
        clc
        jmp .f
.sig:   add rsi, [rel desc_tam]
        jmp .e
.no:    stc
.f:     pop rsi
        pop rcx
        pop rbx
        ret

; ---- tablas_copiar: las entradas de ACPI (RSDP) y de SMBIOS, a la BIOS ----
; Con UEFI esas entradas no están donde las busca un programa de modo real
; (la EBDA y de E0000h a FFFFFh): solo se llega a ellas por la tabla de
; configuración del firmware. Se copian a la zona EBDA de la BIOS de modo
; real (RSDP en +10h, SMBIOS en +40h), que las pone a la vista (bios_inicio):
; 40:0Eh apunta a la EBDA y, si F0000h es memoria que se puede escribir,
; también allí. Las tablas en sí se quedan donde están. R15 = la BIOS.
tablas_copiar:
        push rbx
        push r12
        push r13
        push r14
        sub rsp, 40                     ; (8 + 32 + 40 = 80: pila alineada)
        mov byte [rel callado], 1
        xor r12d, r12d                  ; R12 = RSDP, R13 = SMBIOS, R14 = SMBIOS 3
        xor r13d, r13d
        xor r14d, r14d
        mov rax, [rel sistema]
        mov rcx, [rax + ST_NTABLAS]
        mov rbx, [rax + ST_TABLAS]
        test rbx, rbx
        jz .buscadas
.t:     test rcx, rcx
        jz .buscadas
        mov rax, [rbx]
        mov rdx, [rbx + 8]
        cmp rax, [rel guid_acpi2]
        jne .t1
        cmp rdx, [rel guid_acpi2 + 8]
        jne .t1
        mov r12, [rbx + 16]             ; (la de ACPI 2.0 manda)
        jmp .ts
.t1:    cmp rax, [rel guid_acpi1]
        jne .t2
        cmp rdx, [rel guid_acpi1 + 8]
        jne .t2
        test r12, r12
        jnz .ts
        mov r12, [rbx + 16]
        jmp .ts
.t2:    cmp rax, [rel guid_smbios]
        jne .t3
        cmp rdx, [rel guid_smbios + 8]
        jne .t3
        mov r13, [rbx + 16]
        jmp .ts
.t3:    cmp rax, [rel guid_smbios3]
        jne .ts
        cmp rdx, [rel guid_smbios3 + 8]
        jne .ts
        mov r14, [rbx + 16]
.ts:    add rbx, 24
        dec rcx
        jmp .t
.buscadas:
        mov [rel rsdp], r12             ; (para reloj_mirar: las tablas HPET y APIC)
        xor ebx, ebx                    ; EBX = T_TABLAS
        DI t_t_acpi
        test r12, r12
        jz .sin_acpi
        mov rax, r12
        call hex64
        DI t_t_rev
        movzx eax, byte [r12 + 15]
        call decimal
        DI t_parentesis2
        mov rsi, r12                    ; 20 octetos (ACPI 1.0) o 36
        lea rdi, [r15 + EBDA + 10h]
        mov ecx, 20
        cmp byte [r12 + 15], 2
        jb .ac
        mov ecx, 36
.ac:    cld
        rep movsb
        or ebx, 1
        cmp dword [r12 + 16], 0         ; ¿hay RSDT (de 32 bits)?
        jne .smbios
        DI t_t_xsdt
        jmp .smbios
.sin_acpi:
        DI t_t_no
.smbios:
        DI t_t_smbios
        mov rsi, r13
        mov ecx, 31                     ; «_SM_»: 31 octetos
        test rsi, rsi
        jnz .sm
        mov rsi, r14
        mov ecx, 24                     ; «_SM3_»: 24
        test rsi, rsi
        jz .sin_smbios
.sm:    mov rax, rsi
        call hex64
        lea rdi, [r15 + EBDA + 40h]
        cld
        rep movsb
        or ebx, 2
        jmp .fin
.sin_smbios:
        DI t_t_no
.fin:   DI t_nl
        mov [r15 + 228], bl             ; T_TABLAS
        mov byte [rel callado], 0
        add rsp, 40
        pop r14
        pop r13
        pop r12
        pop rbx
        ret

; ---- escala: tablas de dónde cae cada punto de 640x400 en la pantalla real ----
; Se amplía lo más posible sin deformar (8:5): en 1920x1080, a 1728x1080. Si la
; pantalla es lenta (t_pant), por un entero (esc_k): menos puntos que pintar.
escala:
        push rbx
        push rsi
        push rdi
        mov dword [rel esc_k], 0
        mov eax, [rel fb_ancho]
        mov ecx, [rel fb_alto]
        lea edx, [eax * 4 + eax]        ; An*5 <= Al*8: manda el ancho
        lea ebx, [ecx * 8]
        cmp edx, ebx
        ja .alto
        mov [rel dib_an], eax
        shr edx, 3
        mov [rel dib_al], edx
        jmp .lenta
.alto:  mov [rel dib_al], ecx
        mov eax, ebx                    ; Al*8/5
        xor edx, edx
        mov ebx, 5
        div ebx
        mov [rel dib_an], eax
.lenta: cmp dword [rel t_pant], T_LENTO
        jbe .origen
        mov eax, 640
        mov edx, 400
        call escala_tope
        mov [rel esc_k], ecx
        imul eax, ecx, 640
        mov [rel dib_an], eax
        imul eax, ecx, 400
        mov [rel dib_al], eax
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

; ---- pantalla_dibujar: lleva a la pantalla lo que haya cambiado (el texto o
;      el modo gráfico) y apunta lo que ha tardado (T_DIB_*) ----
; (con las interrupciones cerradas; mientras, con las tablas del alias WC)
pantalla_dibujar:
        push rbx
        push rsi
        push rdi
        push r12
        push r13
        push r14
        push r15
        rdtsc
        shl rdx, 32
        or rax, rdx
        mov r14, rax
        xor r15d, r15d
        cmp byte [rel wc_usar], 0
        je .sin
        mov r15, cr3
        mov rax, [rel wc_cr3]
        mov cr3, rax
.sin:   cld
        cmp byte [rel graf], 0
        je .texto
        call graf_presentar
        jmp .hecho
.texto: call texto_dibujar
.hecho: test r15, r15
        jz .cuenta
        mov cr3, r15
.cuenta:
        rdtsc
        shl rdx, 32
        or rax, rdx
        sub rax, r14
        mov ecx, [rel tsc_tic]
        test ecx, ecx
        jz .fin
        mov edx, 54925
        mul rdx
        div rcx                         ; RAX = microsegundos
        mov rbx, [rel bios_base]
        inc dword [rbx + 196]           ; T_DIB_N
        mov [rbx + 200], eax            ; T_DIB_ULT
        add [rbx + 224], eax            ; T_DIB_TOT
        cmp eax, [rbx + 204]            ; T_DIB_MAX
        jbe .fin
        mov [rbx + 204], eax
.fin:   pop r15
        pop r14
        pop r13
        pop r12
        pop rdi
        pop rsi
        pop rbx
        ret

; texto_dibujar: pinta la pantalla de texto de la BIOS (80x25), ampliada
texto_dibujar:
        push rbx
        push rsi
        push rdi
        push r12
        push r13
        mov r13, [rel bios_base]
        ; la primera vez se borra todo en negro; después solo se pintan las
        ; celdas que han cambiado
        cmp byte [rel borrada], 0
        jne .ya_borrada
        mov byte [rel borrada], 1
        mov rdi, [rel fbv]
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
; Cada fila de puntos de la letra se amplía una vez en linea_buf (memoria
; normal) y se copia a la pantalla tantas veces como líneas ocupe.
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
        mov edx, [r13 + r8 * 4]         ; EDX = primera línea de la pantalla
        cmp edx, [r13 + r8 * 4 + 4]
        jae .sig
        lea rdi, [rel linea_buf]
        mov ebx, r9d
.punto: mov eax, r15d
        shl r11b, 1
        cmovc eax, r14d
        mov ecx, [r12 + rbx * 4 + 4]
        sub ecx, [r12 + rbx * 4]
        rep stosd
        inc ebx
        lea ecx, [r9 + 8]
        cmp ebx, ecx
        jb .punto
        mov ebx, [r12 + r9 * 4 + 32]
        sub ebx, [r12 + r9 * 4]         ; EBX = ancho de la celda en la pantalla
        jz .sig
        push rsi
.y:     mov edi, [rel fb_linea]
        imul rdi, rdx
        mov eax, [r12 + r9 * 4]
        add rdi, rax
        shl rdi, 2
        add rdi, [rel fbv]
        lea rsi, [rel linea_buf]
        mov ecx, ebx
        rep movsd
        inc edx
        cmp edx, [r13 + r8 * 4 + 4]
        jb .y
        pop rsi
.sig:   inc r8d
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

; graf_presentar: lleva a la pantalla las líneas del marco lineal de VBE que
; han cambiado desde la última vez (se comparan con la copia vbe_ant), cada
; punto ampliado G_K veces y la imagen centrada
graf_presentar:
        push rbx
        push rsi
        push rdi
        push r12
        push r13
        push r14
        push r15
        cmp byte [rel g_todo], 0
        je .lineas
        mov rdi, [rel fbv]              ; modo recién puesto: todo en negro
        mov ecx, [rel fb_linea]
        imul ecx, [rel fb_alto]
        xor eax, eax
        rep stosd
.lineas:
        xor r12d, r12d                  ; R12 = línea
        mov r13d, [rel g_an]
        mov r14d, [rel g_k]
        mov r15d, [rel fb_formato]
.linea: mov eax, r12d
        imul eax, r13d
        shl rax, 2
        mov rbx, rax                    ; RBX = dónde empieza la línea en el marco
        cmp byte [rel g_todo], 0
        jne .pinta
        mov rsi, [rel vbe_lfb]
        add rsi, rbx
        mov rdi, [rel vbe_ant]
        add rdi, rbx
        mov ecx, r13d
        repe cmpsd
        je .sig
.pinta: mov rsi, [rel vbe_lfb]
        add rsi, rbx
        mov rdi, [rel vbe_ant]
        add rdi, rbx
        mov ecx, r13d
        rep movsd
        mov rsi, [rel vbe_ant]          ; la línea, ampliada, en linea_buf
        add rsi, rbx
        lea rdi, [rel linea_buf]
        mov edx, r13d
.px:    lodsd
        test r15d, r15d
        jnz .bgr
        bswap eax                       ; (pantalla con el rojo abajo)
        shr eax, 8
.bgr:   mov ecx, r14d
        rep stosd
        dec edx
        jnz .px
        mov edx, r12d                   ; y a la pantalla, G_K líneas iguales
        imul edx, r14d
        add edx, [rel g_y0]
        mov ebx, r14d
.fila:  mov edi, [rel fb_linea]
        imul rdi, rdx
        mov eax, [rel g_x0]
        add rdi, rax
        shl rdi, 2
        add rdi, [rel fbv]
        lea rsi, [rel linea_buf]
        mov ecx, r13d
        imul ecx, r14d
        rep movsd
        inc edx
        dec ebx
        jnz .fila
.sig:   inc r12d
        cmp r12d, [rel g_al]
        jb .linea
        mov byte [rel g_todo], 0
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
t_presta        db 'Paginas del firmware en la memoria baja que se prestan a DOS (se cambian en cada viaje): ', 0
t_t_acpi        db 'Tablas del equipo: ACPI (RSDP) en ', 0
t_t_rev         db ' (revision ', 0
t_t_xsdt        db '; OJO: sin RSDT de 32 bits, solo XSDT', 0
t_t_smbios      db '; SMBIOS en ', 0
t_parentesis2   db ')', 0
t_t_no          db 'no hay', 0
        align 8
guid_acpi2      dd 8868E871h
                dw 0E4F1h, 11D3h
                db 0BCh, 22h, 00h, 80h, 0C7h, 3Ch, 88h, 81h
guid_acpi1      dd 0EB9D2D30h
                dw 2D88h, 11D3h
                db 9Ah, 16h, 00h, 90h, 27h, 3Fh, 0C1h, 4Dh
guid_smbios     dd 0EB9D2D31h
                dw 2D88h, 11D3h
                db 9Ah, 16h, 00h, 90h, 27h, 3Fh, 0C1h, 4Dh
guid_smbios3    dd 0F2FD1544h
                dw 9794h, 4A2Ch
                db 99h, 2Eh, 0E5h, 0BBh, 0CFh, 20h, 0E3h, 94h
presta_dir      dd 0, 0, 0, 0           ; las páginas prestadas
presta_guarda   dd 0                    ; dónde se guarda su otra versión
presta_n        db 0
seguro          db 0                    ; 1: modo seguro (tecla S al arrancar)
t_seguro_s      db '(Si no arranca o va mal: reinicie y vaya pulsando la tecla S = modo seguro)', 13, 10, 0
t_seguro        db 'MODO SEGURO: sin escritura combinada, sin cambiar de modo de pantalla y sin prestar paginas', 13, 10, 0
t_g_modos       db 'Modos de pantalla del firmware (GOP; * el puesto): ', 0
t_g_cambio      db 'Pantalla lenta: se pasa a un modo mas pequeno del firmware: ', 0
t_g_coma        db ', ', 0
t_g_ast         db '*', 0
t_g_mas         db '...', 0
t_w_mtrr        db 'Cache de la pantalla: MTRR por omision ', 0
t_w_mtrr2       db 'h; la pantalla, ', 0
t_w_var         db 'en el MTRR variable ', 0
t_w_tipo        db ' de tipo ', 0
t_w_nomtrr      db 'en ninguno (vale el tipo por omision)', 0
t_w_tipos       db 'UC', 0, 0, 'WC', 0, 0, '??', 0, 0, '??', 0, 0, 'WT', 0, 0, 'WP', 0, 0, 'WB', 0, 0, '??', 0, 0
t_w_pat         db '; PAT ', 0
t_w_alias       db 'Escritura combinada (WC): alias de la pantalla en ', 0
t_w_alias2      db ' (tablas en ', 0
t_w_alias3      db ', entrada ', 0
t_w_alias4      db ' del nivel 4), PAT nuevo ', 0
t_w_no_pat      db 'Escritura combinada (WC): no se pone (el procesador no tiene PAT)', 13, 10, 0
t_w_no_la57     db 'Escritura combinada (WC): no se pone (tablas de paginas de 5 niveles)', 13, 10, 0
t_w_no_mem      db 'Escritura combinada (WC): no se pone (sin memoria o sin hueco en las tablas)', 13, 10, 0
t_w_medida      db 'Pintar la pantalla entera: ', 0
t_w_medida2     db ' us escribiendo directamente, ', 0
t_w_medida3     db ' us por el alias con WC. Se pinta ', 0
t_w_directo     db 'directamente', 0
t_w_poralias    db 'por el alias', 0
t_w_esc         db 'Texto: ', 0
t_w_esc_todo    db 'ampliado a toda la pantalla (', 0
t_w_esc_k       db 'la pantalla es lenta: ampliado solo por ', 0
t_w_esc_k2      db ' (', 0
t_w_cierra      db ')', 13, 10, 0
t_v_lfb         db 'VBE: marco lineal para los programas en ', 0
t_v_lfb2        db ' (3 MB, por debajo de 4 GB); modos de 32 bits: 640x480, 800x600 y 1024x768 (los que quepan)', 13, 10, 0
t_v_no          db 'VBE: no hay memoria por debajo de 4 GB para el marco lineal: sin modos graficos', 13, 10, 0
t_r_tsc         db ' KB; TSC por tic: ', 0
t_r_vec         db '; vector de la IRQ 0 (8259): ', 0
t_r_discos      db 'h; discos: ', 0
t_sin_discos    db 13, 10, 'NO SE ENCUENTRA EL DISCO DE ARRANQUE (la memoria USB) entre los discos', 13, 10
                db 'del firmware (hacen falta sectores de 512 octetos).', 13, 10, 0
t_viaje         db 'Arrancando la BIOS de modo real...', 13, 10, 0
; el reloj (reloj_mirar)
t_k_seguro      db 'Reloj: modo seguro, no se toca el PCH, el HPET ni el IOAPIC; la BIOS solo prueba el 8254 y el reloj CMOS', 13, 10, 0
t_k_pit         db 'Reloj 8254 (PIT), canal 0: ', 0
t_k_pit2        db 'Reloj 8254 (PIT) despues de quitar el bloqueo: ', 0
t_k_si          db 'CUENTA (lecturas ', 0
t_k_no          db 'NO CUENTA: esta parado (lecturas ', 0
t_k_y           db 'h y ', 0
t_k_fin         db 'h con 2 ms entre medias)', 13, 10, 0
t_i_lpc         db 'Bloqueo del 8254 (clock gating) del PCH: el puente 00:1F.0 es ', 0
t_i_no_intel    db ', que no es de Intel: no se sabe quitar', 0
t_i_p2sb        db '; P2SB (00:1F.1): identificador ', 0
t_i_e0          db ', registro E0h ', 0
t_i_mostrado    db '; al mostrarlo, ', 0
t_i_no_hay      db ': no aparece (este puente no lo tiene)', 0
t_i_raro        db ': no es el P2SB de un PCH', 0
t_i_bar         db '; SBREG_BAR ', 0
t_i_sin_bar     db ': sin registros que valgan', 0
t_i_prc         db '; ITSSPRC (puerto C4h, 3300h) = ', 0
t_i_ya          db ': el bit 2 (8254CGE) ya esta a 0, el bloqueo no es este', 0
t_i_quitado     db ' con el bit 2 (8254CGE) puesto: se quita y queda ', 0
t_k_sin_hpet    db 'HPET: la tabla de ACPI no esta (no hay HPET)', 13, 10, 0
t_k_hpet        db 'HPET: en ', 0
t_k_hpet_id     db ', identificador ', 0
t_k_hpet_cap    db '; capacidades ', 0
t_k_hpet_cfg    db ', configuracion ', 0
t_k_hpet_t0     db ', temporizador 0 ', 0
t_k_alto        db ' (por encima de 4 GB: no se usa)', 0
t_k_nomap       db ' (no esta en las tablas de paginas del firmware: no se mira)', 0
t_k_lapic       db 'APIC local: IA32_APIC_BASE ', 0
t_k_x2          db ' (x2APIC)', 0
t_k_svr         db '; SVR ', 0
t_k_lint0       db ', LINT0 ', 0
t_k_lint1       db ', LINT1 ', 0
t_k_lvtt        db ', temporizador ', 0
t_k_sin_madt    db 'Tabla APIC de ACPI: no esta', 13, 10, 0
t_k_madt        db 'Tabla APIC de ACPI: banderas ', 0
t_k_ioapic      db '; IOAPIC en ', 0
t_k_gsi         db ' desde la entrada ', 0
t_k_irq0        db '; la IRQ 0 va a la entrada ', 0
t_k_ioa0        db '; el IOAPIC tiene la entrada 0 = ', 0
t_k_ioa2        db ' y la 2 = ', 0
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
rsdp            dq 0                    ; la RSDP de ACPI (tablas_copiar)
%ifdef PRUEBA_P2SB
        align 16
sim_prc         dd 00000007h            ; ITSSPRC de mentira: 8254CGE (bit 2) puesto
sim_oculto      db 1
sim_cambios     db 0
t_sim           db '(prueba) P2SB de mentira: oculto = ', 0
t_sim2          db ', cambios de ocultacion = ', 0
t_sim3          db ', ITSSPRC = ', 0
%endif
pit_l1          dd 0                    ; las dos lecturas del 8254 (pit_cuenta)
pit_l2          dd 0
rj_callado      db 0
buf_car         db 0, 0
        align 8
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
fbv             dq 0                    ; por dónde se escribe en la pantalla: fb o su
                                        ; alias con escritura combinada (wc_alias)
wc_alias        dq 0
wc_cr3          dq 0                    ; tablas de páginas con el alias (0: no hay)
reg_marca       dq 0
vbe_lfb         dq 0                    ; el marco lineal que ven los programas
vbe_ant         dq 0                    ; lo que hay pintado de él
tsc_tic         dd 0                    ; cuentas del TSC por tic (54925 us)
t_pant          dd 0                    ; microsegundos que cuesta pintar la pantalla entera
esc_k           dd 0                    ; texto: 0 = ampliado a toda la pantalla; k = 640k x 400k
g_an            dd 0                    ; el modo VBE puesto: ancho, alto, ampliación
g_al            dd 0                    ; y dónde cae en la pantalla
g_k             dd 0
g_x0            dd 0
g_y0            dd 0
; los modos VBE (los mismos de vbe_modos en bios64.asm): ancho, alto
g_modos         dd 640, 480, 800, 600, 1024, 768
G_NMODOS        equ 3
wc_usar         db 0                    ; 1: se pinta por el alias
graf            db 0                    ; 0: texto; 1...: modo VBE puesto
g_todo          db 0                    ; 1: hay que pintar el marco entero
        align 4
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
        align 16
linea_buf       times LIN_MAX dd 0      ; una línea de puntos ya ampliada
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
