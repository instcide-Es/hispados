; =============================================================================
;  HispaBIOS (IA32) - parte UEFI para firmware de 32 bits
; =============================================================================
;  Lo mismo que efi64.asm, para los equipos con UEFI de 32 bits (las tabletas
;  y los portátiles pequeños con Atom «Bay Trail» —Acer One 10—, aunque su
;  procesador sea de 64 bits, y algunos Mac antiguos). Por orden (cada paso se
;  enseña en pantalla y se apunta en el registro; si algo falla se dice qué
;  falta y qué hacer, y el registro se guarda en la memoria USB como
;  \HISPABIOS.LOG):
;    1. quita el vigilante del firmware (reinicia el equipo a los 5 minutos);
;    2. lee el mapa de memoria (GetMemoryMap, con el búfer del tamaño que el
;       firmware pida) y enseña lo que hay por debajo de 1 MB;
;    3. pide al firmware, página a página, la memoria baja (0 a 9FFFFh);
;    4. elige dónde poner la BIOS de modo real (bios64.asm ensamblada con
;       ARQ32 = BIOS32.BIN): 60 KB seguidos en 90000h, 70000h o 60000h;
;    5. busca la pantalla (GOP) y los discos (Block I/O: la memoria USB, la
;       eMMC...), copia la BIOS y la arranca. Después atiende sus peticiones
;       (despachador).
;  Y todo lo de la 0.3 y la 0.4 de efi64.asm (docs/HISPABIOS.md): las páginas
;  sueltas del firmware prestadas a DOS, las entradas de ACPI y SMBIOS a la
;  vista del modo real, la pantalla con escritura combinada y su medida, VBE
;  con marco lineal, el modo seguro (tecla S) y el reloj (8254, HPET, APIC).
;  Propio de esta parte:
;    - LA PANTALLA GIRADA. Muchas tabletas llevan un panel vertical (800x1280)
;      puesto de lado: el firmware da un modo de 800x1280 y todo saldría
;      tumbado. Si el modo es más alto que ancho se gira 90 grados: el texto y
;      los modos de VBE se pintan en una pantalla «lógica» apaisada (memoria
;      normal) y a la pantalla de verdad va, girado, solo lo que cambia
;      (efi32_pant.inc, girar_sucio). El giro se cambia pulsando G al arrancar
;      o con Ctrl+Alt+G, y queda guardado en \HBGIRO.CFG de la memoria USB;
;    - el equipo (fabricante y modelo de SMBIOS) y cada disco, al registro.
;
;  Convención de UEFI en IA32: la de C (cdecl), con los argumentos en la pila
;  de derecha a izquierda, el que llama la limpia, el resultado en EAX y se
;  conservan EBX, ESI, EDI y EBP. Los valores de 64 bits (EFI_LBA,
;  EFI_PHYSICAL_ADDRESS) ocupan dos palabras de la pila, la baja primero.
;  Las estructuras siguen la alineación natural (los UINT64 a 8). LA PILA VA
;  ALINEADA A 16 OCTETOS EN CADA LLAMADA (lo pide la especificación y hay
;  firmware compilado con SSE): todas las llamadas van por la macro FW.
;
;  La imagen no tiene reubicaciones: EBP lleva siempre la dirección donde el
;  firmware la ha cargado y todo se direcciona como [V(etiqueta)].
;
;  Opciones de ensamblado para las pruebas (tests/run_hispabios_tests.sh), las
;  de efi64.asm: PRUEBA_OCUPA, PRUEBA_SIN_GOP, PRUEBA_LENTA, PRUEBA_SIN_WC,
;  PRUEBA_SIN_PRESTAR, PANTALLA_NATIVA, PRUEBA_SEGURO, PRUEBA_P2SB,
;  PRUEBA_RELOJ y REGISTRO_PASOS; y además
;    -DPRUEBA_GIRO=n           el giro por omisión (1: 90 grados a la derecha,
;                              2: 180, 3: 90 a la izquierda) aunque la
;                              pantalla sea apaisada
;    -DPRUEBA_FW_SIN_RELOJ     el firmware no recibe interrupciones en sus viajes
;    -DPRUEBA_FWT=n            los tics al firmware: 0 no se dan; 2 por su rutina
;                              de la interrupción (los dos, en efi32_tecl.inc
;                              y también en efi64.asm)
;    -DPRUEBA_SIN_PAG          las tablas propias de escritura combinada se
;                              ponen y se quitan desde un estado sin
;                              paginación (como en un firmware que no la usa)
;
;  Referencias: especificación UEFI 2.x (capítulos 2.3.2, 4, 7, 12 y 13).
; =============================================================================
%define PE_ARQ 32
%include "pe.inc"

%define HB_VERSION '0.6'

; ---- tablas de UEFI (IA32: punteros de 4 octetos) ----
ST_VENDOR       equ 24          ; FirmwareVendor (CHAR16 *)
ST_FWREV        equ 28          ; FirmwareRevision
ST_CONINH       equ 32          ; ConsoleInHandle (para SimpleTextInputEx)
ST_CONIN        equ 36
ST_CONOUT       equ 44
ST_RUNTIME      equ 56
ST_BOOTSERV     equ 60
ST_NTABLAS      equ 64          ; NumberOfTableEntries
ST_TABLAS       equ 68          ; ConfigurationTable (GUID de 16 octetos y puntero: 20)
TXT_OUTSTR      equ 4
TXT_READKEY     equ 4
BS_ALLOCPAGES   equ 32
BS_GETMEMMAP    equ 40
BS_ALLOCPOOL    equ 44
BS_FREEPOOL     equ 48
BS_HANDLEPROT   equ 88
BS_STALL        equ 136
BS_WATCHDOG     equ 140
BS_CONNECT      equ 144         ; ConnectController
BS_LOCATEHBUF   equ 168
BS_LOCATEPROT   equ 172
RT_RESET        equ 64
LI_DEVICE       equ 12          ; EFI_LOADED_IMAGE.DeviceHandle
BIO_MEDIA       equ 8           ; EFI_BLOCK_IO.Media
BIO_READ        equ 16
BIO_WRITE       equ 20
P8259_GETVEC    equ 16
PCIIO_LEER      equ 24          ; EFI_PCI_IO_PROTOCOL.Pci.Read
GOP_QUERY       equ 0
GOP_SETMODE     equ 4
GOP_MODE        equ 12
GM_INFO         equ 8
GM_FB           equ 16
FS_OPENVOL      equ 8           ; EFI_SIMPLE_FILE_SYSTEM.OpenVolume
F_OPEN          equ 8           ; EFI_FILE: Open, Close, Delete, Read, Write
F_CLOSE         equ 12
F_DELETE        equ 16
F_READ          equ 20
F_WRITE         equ 24

ALLOC_ANY       equ 0           ; AllocateAnyPages
ALLOC_MAX       equ 1           ; AllocateMaxAddress
ALLOC_ADDRESS   equ 2           ; AllocateAddress
MEM_RESERVADA   equ 0           ; EfiReservedMemoryType
MEM_LOADERDATA  equ 2           ; EfiLoaderData
EFI_CHICO       equ 80000005h   ; EFI_BUFFER_TOO_SMALL
VBE_MEM         equ 300000h     ; el marco lineal de VBE (1024x768 de 32 bits)
VBE_PAGS        equ (2 * VBE_MEM) >> 12   ; el marco y la copia de lo ya pintado
REG_BIOS        equ 9000h       ; el registro de la BIOS de modo real (zona REG)
REG_BIOS_TAM    equ 0FF0h
LIN_MAX         equ 8192        ; puntos de la línea de trabajo (linea_buf)
T_LENTO         equ 100000      ; microsegundos: pintar la zona entera no debe pasar de aquí
EBDA            equ 8FA0h       ; en la BIOS: las entradas de ACPI (+10h) y SMBIOS (+40h)
PRESTA_MAX      equ 4           ; páginas del firmware que se prestan a DOS, como mucho
BIOS_TAM        equ 0F000h      ; la BIOS de modo real y sus zonas (bios64.asm)
BIOS_PAGS       equ BIOS_TAM >> 12
TX              equ 8000h       ; su pantalla de texto (80x25x2)
PASO            equ 0C000h      ; su búfer de paso para el disco
NPAG            equ 160         ; páginas de 0 a 9FFFFh
PAG_ARRANQUE    equ 80h         ; la página del sector de arranque de Hispa-DOS (80000h)
MEM_MIN_KB      equ 256         ; con menos memoria convencional no se arranca
REG_MAX         equ 60000       ; octetos del registro (HISPABIOS.LOG)
PANT_MAX        equ 14          ; líneas del mapa que salen en pantalla
DISCO_MIN       equ 65536       ; sectores (32 MB): un disco fijo más pequeño que no sea
                                ; el de arranque no se da a DOS (las particiones de
                                ; arranque de una eMMC: 4 MB cada una)
TES_AN          equ 256         ; la tesela con la que se gira la pantalla (girar_sucio)
TES_AL          equ 16

%define V(x) ebp + (x)          ; una variable de la imagen

; ---- base: EBP = dirección de pe_inicio (donde se ha cargado la imagen) ----
%macro BASE_EBP 0
        call %%aqui
%%aqui: pop ebp
        sub ebp, %%aqui
%endmacro

; DI: «escribir» un texto (conserva todos los registros)
%macro DI 1
        push esi
        lea esi, [V(%1)]
        call escribir
        pop esi
%endmacro

; FW: una llamada al firmware con la pila alineada a 16 octetos.
;       FW función, argumento, argumento...
; La función es un operando de memoria ([eax + BS_STALL]); cada argumento, un
; registro, un número, una doble palabra de memoria ([V(x)]) o «&nombre», la
; dirección de una variable de la imagen. Los argumentos se pueden apoyar en
; EAX (se toman antes de cargar la función), no en ESP. Como cualquier llamada
; de C, cambia EAX (el resultado), ECX y EDX.
%macro FW 1-9
  %assign %%i 0
  %rep %0 - 1
    %rotate 1
    %defstr %%s %1
    %substr %%c %%s 1
    %if %%c == '&'
      %substr %%r %%s 2,-1
      %deftok %%t %%r
        push eax
        lea eax, [V(%%t)]
        xchg eax, [esp]
    %else
        push dword %1
    %endif
        pop dword [V(fw_a) + 4 * %%i]
    %assign %%i %%i + 1
  %endrep
  %rotate 1
        mov eax, %1
        call fw_llamar
%endmacro

pe_entrada:
        push ebp
        mov eax, esp
        push ebx
        push esi
        push edi
        BASE_EBP
        mov ecx, [eax + 8]
        mov [V(imagen)], ecx
        mov ebx, [eax + 12]             ; EBX = tabla del sistema
        mov [V(sistema)], ebx
        mov ecx, [ebx + ST_CONOUT]
        mov [V(conout)], ecx
        mov ecx, [ebx + ST_BOOTSERV]
        mov [V(bootserv)], ecx
        cld

        ; el vigilante del firmware reinicia el equipo a los 5 minutos de
        ; arrancar un cargador: fuera (0 segundos; el código es de 64 bits)
        mov eax, [V(bootserv)]
        FW [eax + BS_WATCHDOG], 0, 0, 0, 0, 0

        ; la memoria del registro
        mov eax, [V(bootserv)]
        FW [eax + BS_ALLOCPOOL], MEM_LOADERDATA, REG_MAX, &reg_dir
        test eax, eax
        jz .con_registro
        mov dword [V(reg_dir)], 0
.con_registro:

        DI t_titulo
        call anotar_firmware
        call seguro_mirar               ; ¿se ha pulsado S? -> modo seguro (y G: el giro)
        call registro_paso

%ifdef PRUEBA_OCUPA
        ; (pruebas: -DPRUEBA_OCUPA=dir,n,...) páginas ocupadas de antemano,
        ; como en un equipo cuyo firmware las usa
        lea ebx, [V(prueba_ocupa)]
.ocupa: mov eax, [ebx]
        cmp eax, -1
        je .ocupadas
        mov [V(dir_pag)], eax
        mov dword [V(dir_pag) + 4], 0
        mov eax, [V(bootserv)]
        FW [eax + BS_ALLOCPAGES], ALLOC_ADDRESS, MEM_RESERVADA, [ebx + 4], &dir_pag
        add ebx, 8
        jmp .ocupa
.ocupadas:
%endif

        ; ---- el mapa de memoria ----
        mov byte [V(callado)], 1
        DI t_p_mapa
        mov byte [V(callado)], 0
        call mapa_leer
        jnc .mapa_bien
        DI t_mapa_mal
        mov ecx, 8
        call hex
        DI t_nl
        jmp .fallo
.mapa_bien:
        mov eax, [V(desc_tam)]
        cmp eax, 40                     ; (EFI_MEMORY_DESCRIPTOR: 40 octetos o más)
        jb .desc_mal
        cmp eax, 4096
        jbe .desc_bien
.desc_mal:
        DI t_desc_mal
        mov eax, [V(desc_tam)]
        call decimal
        DI t_nl
        jmp .fallo
.desc_bien:
        call mapa_ensenar
        call registro_paso

        ; ---- pide las páginas bajas una a una (de 0 a 9F000h) ----
        DI t_pide
        xor edi, edi                    ; EDI = número de página
        xor ebx, ebx                    ; EBX = cuántas se han dado
.pagina:
        mov eax, edi
        shl eax, 12
        mov [V(dir_pag)], eax
        mov dword [V(dir_pag) + 4], 0
        mov eax, [V(bootserv)]
        FW [eax + BS_ALLOCPAGES], ALLOC_ADDRESS, MEM_LOADERDATA, 1, &dir_pag
        test eax, eax
        setz byte [V(dada) + edi]
        jnz .otra
        inc ebx
.otra:  inc edi
        cmp edi, NPAG
        jb .pagina
        call paginas_ensenar            ; las que no da, por tramos
        DI t_dadas
        mov eax, ebx
        call decimal
        DI t_de160

        ; ---- dónde va la BIOS de modo real: 60 KB seguidos que haya dado ----
        lea ebx, [V(bases)]
.base:  mov eax, [ebx]
        test eax, eax
        jz .sin_base
        shr eax, 12
        lea esi, [V(dada) + eax]
        mov ecx, BIOS_PAGS
.bp:    cmp byte [esi], 0
        je .otra_base
        inc esi
        dec ecx
        jnz .bp
        jmp .hay_base
.otra_base:
        add ebx, 12
        jmp .base
.sin_arranque:                          ; (la página que usa el sector de
        DI t_sin_arranque               ; arranque de Hispa-DOS no está libre)
.sin_base:
        DI t_sin_base
        jmp .fallo
.hay_base:
        mov eax, [ebx]
        mov [V(bios_base)], eax
        mov [V(bios_cual)], ebx
        call paginas_prestar            ; las del firmware que hay por debajo
        cmp byte [V(dada) + PAG_ARRANQUE], 0
        je .sin_arranque
        ; la memoria convencional de DOS: hasta la primera página (de la 1 en
        ; adelante) que el firmware no ha dado, o hasta la BIOS
        mov eax, 1
.tope:  cmp byte [V(dada) + eax], 0
        je .tope_ya
        inc eax
        cmp eax, NPAG
        jb .tope
.tope_ya:
        shl eax, 12
        cmp eax, [V(bios_base)]
        jbe .tope_bien
        mov eax, [V(bios_base)]
.tope_bien:
        mov [V(bajo_tope)], eax
        DI t_base
        mov eax, [V(bios_base)]
        mov ecx, 5
        call hex
        DI t_base2
        mov eax, [V(bajo_tope)]
        shr eax, 10
        call decimal
        DI t_kb
        cmp dword [V(bajo_tope)], MEM_MIN_KB * 1024
        jae .mem_bien
        DI t_poca
        jmp .fallo
.mem_bien:
        call registro_paso

        ; ---- la pantalla y la BIOS de modo real ----
        mov eax, [V(reg_n)]             ; (de aquí en adelante, también al
        mov [V(reg_marca)], eax         ; registro de la BIOS: registro_a_bios)
        call gop_buscar
        setc byte [V(m_singop)]
        mov byte [V(callado)], 1        ; (solo al registro)
        call gop_decir
        mov byte [V(callado)], 0
        cmp byte [V(m_singop)], 0
        jne .sin_gop
        call giro_decidir               ; ¿la pantalla es vertical? -> el giro
        mov edi, [V(bios_base)]         ; la zona de la BIOS, a cero
        mov ecx, BIOS_TAM / 4
        xor eax, eax
        cld
        rep stosd
        mov ebx, [V(bios_cual)]         ; la BIOS (la ensamblada para esa
        mov esi, [ebx + 4]              ; base), a su sitio
        add esi, ebp
        mov edi, [V(bios_base)]
        mov ecx, [ebx + 8]
        rep movsb
        mov ebx, [V(bios_base)]         ; EBX = la tabla de traspaso
        lea eax, [V(despachador)]
        mov [ebx + 8], eax              ; T_DESPACHADOR
        mov al, [V(presta_n)]           ; T_INT_*: las páginas prestadas
        mov [ebx + 229], al
        mov eax, [V(presta_guarda)]
        mov [ebx + 232], eax
        mov eax, [V(presta_dir)]
        mov [ebx + 236], eax
        mov eax, [V(presta_dir) + 4]
        mov [ebx + 240], eax
        mov eax, [V(presta_dir) + 8]
        mov [ebx + 244], eax
        mov eax, [V(presta_dir) + 12]
        mov [ebx + 248], eax
        call tablas_copiar              ; ACPI y SMBIOS, a la vista del modo real
        mov eax, [V(bajo_tope)]         ; T_MEMKB: hasta la primera página ocupada
        shr eax, 10
        mov [ebx + 28], ax
        call memoria_ext                ; T_EXTKB
        mov [ebx + 32], eax
        mov [V(m_ext)], eax
        call tsc_calibrar               ; T_TSC_TICK
        mov [ebx + 36], eax
        mov [V(tsc_tic)], eax
        call vbe_preparar               ; T_VBE_*: el marco lineal de los modos gráficos
        call vector_irq0                ; T_VEC0
        mov [ebx + 182], al
        mov [V(m_vec)], eax
        mov byte [V(callado)], 1        ; (solo al registro)
        call discos_buscar              ; T_NDISCOS y T_DISCOS
        mov [ebx + 30], al
        DI t_r_ext
        mov eax, [V(m_ext)]
        call decimal
        DI t_r_tsc
        mov eax, [V(tsc_tic)]
        call decimal
        DI t_r_vec
        mov eax, [V(m_vec)]
        mov ecx, 2
        call hex
        DI t_r_discos
        movzx eax, byte [V(ndiscos)]
        call decimal
        DI t_nl
        mov byte [V(callado)], 0
        cmp byte [V(ndiscos)], 0
        jne .hay_discos
        DI t_sin_discos
        jmp .fallo
.hay_discos:
        mov byte [V(callado)], 1        ; (solo al registro)
        call raton_preparar             ; el ratón: del firmware o USB (T_RAT_HAY)
        call teclado_preparar           ; el teclado del firmware y los tics de su reloj
        call registro_paso
        call gop_modos                  ; los modos que da el firmware
        call wc_preparar                ; la pantalla con escritura combinada y su velocidad
%ifndef PANTALLA_NATIVA
        ; si aun así es lenta, un modo más pequeño del firmware, si lo da (el
        ; equipo lo amplía a toda la pantalla y hay menos puntos que escribir)
        cmp dword [V(t_pant)], T_LENTO
        jbe .pant_ya
        cmp byte [V(seguro)], 0
        jne .pant_ya
        call gop_menor
        jc .pant_ya
        call gop_buscar
        call giro_poner                 ; (la pantalla lógica del modo nuevo)
        DI t_g_cambio
        call gop_decir2
        call wc_preparar
.pant_ya:
%endif
        call reloj_mirar                ; el 8254, el HPET y los APIC: T_PIT, T_HPET...
        call registro_a_bios            ; (lo de la pantalla, también para HBLOG)
        mov byte [V(callado)], 0
        DI t_viaje
        call registro_paso
        mov eax, [V(bios_base)]
        call eax                        ; (no vuelve mientras DOS funcione)
        BASE_EBP
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
        pop edi
        pop esi
        pop ebx
        pop ebp
        ret

; ---- fw_llamar: EAX = la función del firmware; sus argumentos (hasta 8
;      palabras), en fw_a -> EAX = el resultado. La pila, alineada a 16. ----
fw_llamar:
        push esi
        push edi
        mov esi, esp
        and esp, -16
        sub esp, 32
        mov edi, [V(fw_a)]
        mov [esp], edi
        mov edi, [V(fw_a) + 4]
        mov [esp + 4], edi
        mov edi, [V(fw_a) + 8]
        mov [esp + 8], edi
        mov edi, [V(fw_a) + 12]
        mov [esp + 12], edi
        mov edi, [V(fw_a) + 16]
        mov [esp + 16], edi
        mov edi, [V(fw_a) + 20]
        mov [esp + 20], edi
        mov edi, [V(fw_a) + 24]
        mov [esp + 24], edi
        mov edi, [V(fw_a) + 28]
        mov [esp + 28], edi
        cld
        call eax
        cld
        mov esp, esi
        pop edi
        pop esi
        ret

; ---- mapa_leer: GetMemoryMap con el búfer que haga falta -> mapa_dir, mapa_tam,
;      desc_tam; CF y EAX = código del firmware si no se puede ----
; (El mapa de un equipo actual tiene cientos de entradas y pedir memoria para
; el búfer puede añadir alguna: se pide de más y se repite si aun así no cabe.)
mapa_leer:
        push ebx
        mov ebx, 5                      ; intentos
        lea eax, [V(mapa_min)]          ; la primera vez, solo el tamaño
        mov [V(mapa_dir)], eax
        mov dword [V(mapa_cabe)], 0
.pide:  mov eax, [V(mapa_cabe)]
        mov [V(mapa_tam)], eax
        mov eax, [V(bootserv)]
        FW [eax + BS_GETMEMMAP], &mapa_tam, [V(mapa_dir)], &mapa_llave, &desc_tam, &desc_ver
        test eax, eax
        jz .bien
        cmp eax, EFI_CHICO
        jne .mal
        dec ebx
        jz .mal
        cmp dword [V(mapa_cabe)], 0     ; el búfer anterior, fuera
        je .nuevo
        mov eax, [V(bootserv)]
        FW [eax + BS_FREEPOOL], [V(mapa_dir)]
.nuevo: mov edx, [V(mapa_tam)]          ; lo que pide y 16 entradas de más
        add edx, 16 * 64
        mov [V(mapa_cabe)], edx
        mov eax, [V(bootserv)]
        FW [eax + BS_ALLOCPOOL], MEM_LOADERDATA, edx, &mapa_dir
        test eax, eax
        jz .pide
.mal:   stc
        jmp .fin
.bien:  clc
.fin:   pop ebx
        ret

; ---- mapa_ensenar: el mapa entero al registro; lo de debajo de 1 MB, además,
;      a la pantalla (las primeras PANT_MAX líneas) ----
mapa_ensenar:
        pushad
        mov byte [V(callado)], 1
        DI t_r_mapa
        mov eax, [V(mapa_tam)]
        xor edx, edx
        div dword [V(desc_tam)]
        call decimal
        DI t_r_mapa2
        mov eax, [V(desc_tam)]
        call decimal
        DI t_r_mapa3
        mov byte [V(callado)], 0
        DI t_mapa
        mov ebx, [V(mapa_dir)]
        mov eax, [V(mapa_tam)]
        add eax, ebx
        mov [V(me_fin)], eax            ; el final del mapa
        xor eax, eax
        mov [V(me_vis)], eax            ; líneas en pantalla
        mov [V(me_no)], eax             ; líneas que no han cabido
        mov [V(me_tot)], eax            ; páginas de memoria en total
        mov [V(me_tot) + 4], eax
.entrada:
        lea eax, [ebx + 40]
        cmp eax, [V(me_fin)]
        ja .fin_mapa
        mov eax, [ebx]                  ; memoria de verdad (no entrada/salida
        cmp eax, 11                     ; ni reservada): para el total
        je .sin_sumar
        cmp eax, 12
        je .sin_sumar
        test eax, eax
        jz .sin_sumar
        mov eax, [ebx + 24]
        add [V(me_tot)], eax
        mov eax, [ebx + 28]
        adc [V(me_tot) + 4], eax
.sin_sumar:
        mov byte [V(callado)], 1
        cmp dword [ebx + 12], 0         ; PhysicalStart (64 bits)
        jne .linea
        cmp dword [ebx + 8], 100000h
        jae .linea
        cmp dword [V(me_vis)], PANT_MAX
        jb .visible
        inc dword [V(me_no)]
        jmp .linea
.visible:
        inc dword [V(me_vis)]
        mov byte [V(callado)], 0
.linea: DI t_sangria
        mov eax, [ebx + 12]
        mov ecx, 4
        call hex
        mov eax, [ebx + 8]
        mov ecx, 8
        call hex
        DI t_paginas
        mov eax, [ebx + 28]             ; NumberOfPages
        mov ecx, 1
        call hex
        mov eax, [ebx + 24]
        mov ecx, 8
        call hex
        DI t_tipo
        mov eax, [ebx]                  ; Type
        mov ecx, 8
        cmp eax, 100h
        jae .tipo_largo
        mov ecx, 2
.tipo_largo:
        call hex
        DI t_espacio
        mov eax, [ebx]
        lea esi, [V(t_tipo_otro)]
        cmp eax, 15
        ja .nombre
        imul eax, eax, 12
        lea esi, [V(t_tipos) + eax]
.nombre:
        call escribir
        mov al, [V(callado)]            ; los atributos, solo al registro
        push eax
        mov byte [V(callado)], 1
        DI t_atr
        mov edx, [ebx + 36]
        mov eax, [ebx + 32]
        call hex64
        pop eax
        mov [V(callado)], al
        DI t_nl
        add ebx, [V(desc_tam)]
        jmp .entrada
.fin_mapa:
        mov byte [V(callado)], 0
        cmp dword [V(me_no)], 0
        je .total
        DI t_mas
        mov eax, [V(me_no)]
        call decimal
        DI t_mas2
.total: DI t_total
        mov eax, [V(me_tot)]
        mov edx, [V(me_tot) + 4]
        shrd eax, edx, 8                ; páginas -> MB
        call decimal
        DI t_mb
        popad
        ret

; ---- paginas_ensenar: los tramos de páginas bajas que el firmware no ha dado ----
paginas_ensenar:
        pushad
        xor edi, edi                    ; EDI = página
.busca: cmp edi, NPAG
        jae .fin
        cmp byte [V(dada) + edi], 0
        je .tramo
        inc edi
        jmp .busca
.tramo: mov ebx, edi                    ; EBX = primera del tramo
.sigue: inc edi
        cmp edi, NPAG
        jae .tramo_fin
        cmp byte [V(dada) + edi], 0
        je .sigue
.tramo_fin:
        DI t_no_dada
        mov eax, ebx
        shl eax, 12
        mov ecx, 5
        call hex
        DI t_a
        mov eax, edi
        shl eax, 12
        dec eax
        call hex
        DI t_parentesis
        mov eax, edi
        sub eax, ebx
        call decimal
        DI t_pag_fin
        jmp .busca
.fin:   popad
        ret

; ---- anotar_firmware: quién ha hecho el firmware y su versión, al registro ----
anotar_firmware:
        pushad
        mov byte [V(callado)], 1
        DI t_r_fab
        mov ebx, [V(sistema)]
        mov esi, [ebx + ST_VENDOR]
        lea edi, [V(buf_txt)]
        mov ecx, 60
        test esi, esi
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
        mov byte [edi], 0
        DI buf_txt
        DI t_r_rev
        mov eax, [ebx + ST_FWREV]
        mov ecx, 8
        call hex
        DI t_r_uefi
        movzx eax, word [ebx + 10]      ; Hdr.Revision: mayor y menor
        call decimal
        DI t_punto
        movzx eax, word [ebx + 8]
        call decimal
        DI t_r_conout
        mov eax, [V(conout)]
        mov ecx, 8
        call hex
        DI t_nl
        mov byte [V(callado)], 0
        popad
        ret

; ---- seguro_mirar: el modo seguro y la tecla del giro ----
; Si al arrancar se va pulsando la tecla S, HispaBIOS no hace nada de lo que
; depende del equipo y podría sentarle mal a algún firmware: ni las tablas de
; escritura combinada, ni cambiar de modo de pantalla, ni prestar a DOS las
; páginas bajas del firmware, ni tocar el PCH, el HPET o el IOAPIC. (Con
; -DPRUEBA_SEGURO, siempre.) Con la tecla G, la pantalla se gira un cuarto de
; vuelta más de lo que estaba (giro_decidir).
seguro_mirar:
        pushad
        DI t_seguro_s
%ifdef PRUEBA_SEGURO
        mov byte [V(seguro)], 1
%endif
        mov eax, [V(bootserv)]
        FW [eax + BS_STALL], 400000     ; 0,4 s para que dé tiempo a la tecla
        mov ebx, 32                     ; se leen las teclas que haya
.otra:  mov eax, [V(sistema)]
        mov ecx, [eax + ST_CONIN]
        test ecx, ecx
        jz .fin
        FW [ecx + TXT_READKEY], ecx, &tecla_efi
        test eax, eax
        jnz .fin
        mov ax, [V(tecla_efi) + 2]      ; el carácter
        or al, 20h
        cmp ax, 's'
        jne .g
        mov byte [V(seguro)], 1
.g:     cmp ax, 'g'
        jne .sig
        mov byte [V(giro_tecla)], 1
.sig:   dec ebx
        jnz .otra
.fin:   cmp byte [V(seguro)], 0
        je .f
        DI t_seguro
.f:     popad
        ret

; ---- tecla_esperar: hasta que se pulse una tecla o pasen 2 minutos ----
tecla_esperar:
        pushad
        mov ebx, 1200
.otra:  mov eax, [V(bootserv)]
        FW [eax + BS_STALL], 100000     ; 0,1 s
        mov eax, [V(sistema)]
        mov ecx, [eax + ST_CONIN]
        test ecx, ecx
        jz .sin_teclado
        FW [ecx + TXT_READKEY], ecx, &tecla_efi
        test eax, eax
        jz .fin
.sin_teclado:
        dec ebx
        jnz .otra
.fin:   popad
        ret

; ---- registro_paso: en la imagen de prueba (-DREGISTRO_PASOS) el registro se
;      guarda en cada paso, para que quede aunque el equipo se pare después ----
registro_paso:
%ifdef REGISTRO_PASOS
        pushfd
        call registro_guardar
        popfd
%endif
        ret

; ---- registro_guardar: el registro a \HISPABIOS.LOG de la memoria USB (el
;      volumen del que se ha arrancado); CF si no se puede ----
registro_guardar:
        push esi
        push edi
        push ecx
        mov edi, [V(reg_dir)]
        test edi, edi
        stc
        jz .f
        lea esi, [V(nombre_reg)]
        mov ecx, [V(reg_n)]
        call fich_guardar
.f:     pop ecx
        pop edi
        pop esi
        ret

; raiz_abrir: la carpeta raíz del volumen del que se ha arrancado -> reg_raiz; CF si no
raiz_abrir:
        mov eax, [V(bootserv)]
        FW [eax + BS_HANDLEPROT], [V(imagen)], &guid_imagen, &imagen_cargada
        test eax, eax
        jnz .mal
        mov ecx, [V(imagen_cargada)]
        mov eax, [V(bootserv)]
        FW [eax + BS_HANDLEPROT], [ecx + LI_DEVICE], &guid_ficheros, &reg_vol
        test eax, eax
        jnz .mal
        mov ecx, [V(reg_vol)]
        FW [ecx + FS_OPENVOL], ecx, &reg_raiz
        test eax, eax
        jnz .mal
        clc
        ret
.mal:   stc
        ret

; ---- fich_guardar: ESI = nombre (UTF-16), EDI = datos, ECX = octetos -> el
;      archivo, nuevo, en la raíz de la memoria USB; CF si no se puede ----
fich_guardar:
        pushad
        mov [V(fg_nom)], esi
        mov [V(fg_dat)], edi
        mov [V(fg_n)], ecx
        call raiz_abrir
        jc .mal
        mov ebx, [V(reg_raiz)]
        ; si ya hay uno, se borra (así no queda la cola del anterior)
        FW [ebx + F_OPEN], ebx, &reg_fich, [V(fg_nom)], 3, 0, 0, 0
        test eax, eax
        jnz .crear
        mov ecx, [V(reg_fich)]
        FW [ecx + F_DELETE], ecx        ; (cierra también)
.crear: FW [ebx + F_OPEN], ebx, &reg_fich, [V(fg_nom)], 3, 80000000h, 0, 0
        mov [V(fg_res)], eax            ; (crear, leer y escribir)
        test eax, eax
        jnz .cierra_raiz
        mov eax, [V(fg_n)]
        mov [V(reg_escrito)], eax
        mov ecx, [V(reg_fich)]
        FW [ecx + F_WRITE], ecx, &reg_escrito, [V(fg_dat)]
        mov [V(fg_res)], eax
        mov ecx, [V(reg_fich)]
        FW [ecx + F_CLOSE], ecx
.cierra_raiz:
        FW [ebx + F_CLOSE], ebx
        cmp dword [V(fg_res)], 0
        jne .mal
        popad
        clc
        ret
.mal:   popad
        stc
        ret

; ---- fich_leer: ESI = nombre (UTF-16), EDI = búfer, ECX = cuánto cabe ->
;      EAX = octetos leídos; CF si no está o no se puede ----
fich_leer:
        pushad
        mov [V(fg_nom)], esi
        mov [V(fg_dat)], edi
        mov [V(reg_escrito)], ecx
        call raiz_abrir
        jc .mal
        mov ebx, [V(reg_raiz)]
        FW [ebx + F_OPEN], ebx, &reg_fich, [V(fg_nom)], 1, 0, 0, 0
        mov [V(fg_res)], eax
        test eax, eax
        jnz .cierra_raiz
        mov ecx, [V(reg_fich)]
        FW [ecx + F_READ], ecx, &reg_escrito, [V(fg_dat)]
        mov [V(fg_res)], eax
        mov ecx, [V(reg_fich)]
        FW [ecx + F_CLOSE], ecx
.cierra_raiz:
        FW [ebx + F_CLOSE], ebx
        cmp dword [V(fg_res)], 0
        jne .mal
        popad
        mov eax, [V(reg_escrito)]
        clc
        ret
.mal:   popad
        xor eax, eax
        stc
        ret

; ---- memoria_ext: pide al firmware la memoria seguida desde 1 MB -> EAX = KB ----
; Lo que diga el mapa: el tramo libre que empieza en 1 MB (y los libres que
; le sigan), 64 MB como mucho. Si el firmware no lo da entero, o el mapa no
; dice nada, a potencias de dos: 64 MB, 32...
memoria_ext:
        push ebx
        push ecx
        push edx
        push esi
        mov ebx, 100h                   ; EBX = hasta qué página llega lo libre
        mov esi, [V(mapa_dir)]
        mov ecx, [V(mapa_tam)]
        add ecx, esi
.e:     lea eax, [esi + 40]
        cmp eax, ecx
        ja .visto
        cmp dword [esi], 7              ; libre (EfiConventionalMemory)
        jne .sig
        cmp dword [esi + 12], 0
        jne .sig
        mov eax, [esi + 8]
        shr eax, 12                     ; su primera página y cuántas
        mov edx, [esi + 24]
        cmp dword [esi + 28], 0
        jne .largo
        cmp edx, 100000h
        jb .mide
.largo: mov edx, 100000h
.mide:  add edx, eax
        cmp eax, ebx                    ; ¿sigue (o contiene) lo que se lleva?
        ja .sig
        cmp edx, ebx
        jbe .sig
        mov ebx, edx
        mov esi, [V(mapa_dir)]          ; (el mapa no tiene por qué venir en orden:
        jmp .e                          ; otra vuelta desde el principio)
.sig:   add esi, [V(desc_tam)]
        jmp .e
.visto: sub ebx, 100h                   ; páginas libres seguidas desde 1 MB
        cmp ebx, 16384
        jbe .tope
        mov ebx, 16384                  ; 64 MB en páginas
.tope:  cmp ebx, 256
        jae .prueba
        mov ebx, 16384                  ; (el mapa no da nada: a probar)
.prueba:
        mov dword [V(dir_pag)], 100000h
        mov dword [V(dir_pag) + 4], 0
        mov eax, [V(bootserv)]
        FW [eax + BS_ALLOCPAGES], ALLOC_ADDRESS, MEM_LOADERDATA, ebx, &dir_pag
        test eax, eax
        jz .dada
        lea eax, [ebx - 1]              ; si no era una potencia de dos, la de debajo;
        test eax, ebx                   ; si lo era, la mitad
        jz .mitad
        bsr ecx, ebx
        mov ebx, 1
        shl ebx, cl
        jmp .menos
.mitad: shr ebx, 1
.menos: cmp ebx, 256                    ; menos de 1 MB: nada
        jae .prueba
        xor eax, eax
        jmp .fin
.dada:  mov eax, ebx
        shl eax, 2                      ; páginas -> KB
.fin:   pop esi
        pop edx
        pop ecx
        pop ebx
        ret

; ---- tsc_calibrar: -> EAX = cuentas del TSC en un tic de la BIOS (1/18,2 s) ----
tsc_calibrar:
        push ebx
        push ecx
        push edx
        push esi
        rdtsc
        mov ebx, eax
        mov esi, edx
        mov eax, [V(bootserv)]
        FW [eax + BS_STALL], 50000      ; 50 ms
        rdtsc
        sub eax, ebx
        sbb edx, esi                    ; EDX:EAX = cuentas en 50 ms
        test edx, edx                   ; (más de 4.000 millones: no cabe)
        jnz .no
        mov ecx, 54925                  ; un tic = 54,925 ms
        mul ecx
        mov ecx, 50000
        cmp edx, ecx                    ; el cociente tiene que caber
        jae .no
        div ecx
        jmp .fin
.no:    xor eax, eax                    ; (la BIOS se arregla sin el TSC)
.fin:   pop esi
        pop edx
        pop ecx
        pop ebx
        ret

; ---- discos_buscar: el disco de arranque (80h) y los demás -> AL = cuántos ----
; Cada disco entero que da el firmware (Block I/O) con sectores de 512: la
; memoria USB, un disco SATA, la eMMC de una tableta. Primero el que contiene
; el camino del que se ha arrancado (será el 80h) y después los demás, por el
; orden del firmware. Una eMMC se presenta además con sus dos particiones de
; arranque (4 MB cada una) como discos aparte: los discos fijos de menos de
; 32 MB que no son el de arranque no se dan a DOS. Todo queda en el registro.
discos_buscar:
        pushad
        mov byte [V(ndiscos)], 0
        call discos_conectar
        DI t_d_tit
        ; el camino del dispositivo del que se ha arrancado
        mov eax, [V(bootserv)]
        FW [eax + BS_HANDLEPROT], [V(imagen)], &guid_imagen, &imagen_cargada
        test eax, eax
        jnz .fin
        mov ecx, [V(imagen_cargada)]
        mov eax, [V(bootserv)]
        FW [eax + BS_HANDLEPROT], [ecx + LI_DEVICE], &guid_camino, &camino_arranque
        test eax, eax
        jnz .fin
        ; todos los manejadores con Block I/O
        mov eax, [V(bootserv)]
        FW [eax + BS_LOCATEHBUF], 2, &guid_bloques, 0, &n_manejadores, &manejadores
        test eax, eax
        jnz .fin
        ; dos pasadas: primero el disco que contiene el camino de arranque
        ; (será el 80h) y después los demás discos enteros
        mov dword [V(db_pasada)], 0
.pasada:
        mov dword [V(db_i)], 0
.cada:
        mov eax, [V(db_i)]
        cmp eax, [V(n_manejadores)]
        jae .fin_pasada
        mov ecx, [V(manejadores)]
        mov ebx, [ecx + eax * 4]
        mov [V(db_man)], ebx            ; el manejador
        mov eax, [V(bootserv)]
        FW [eax + BS_HANDLEPROT], ebx, &guid_bloques, &bloques
        test eax, eax
        jnz .otro
        mov ebx, [V(bloques)]
        mov esi, [ebx + BIO_MEDIA]
        cmp byte [esi + 5], 0           ; MediaPresent
        je .otro
        cmp byte [esi + 6], 0           ; LogicalPartition: solo discos enteros
        jne .otro
        ; ¿contiene el camino de arranque?
        mov eax, [V(bootserv)]
        FW [eax + BS_HANDLEPROT], [V(db_man)], &guid_camino, &camino
        mov dword [V(db_arr)], 0        ; 1 si es el de arranque
        test eax, eax
        jnz .comparado
        mov esi, [V(camino)]
        call camino_largo               ; ECX = largo sin el nodo final
        mov edi, [V(camino_arranque)]
        mov esi, [V(camino)]
        cld
        repe cmpsb
        jne .comparado
        mov dword [V(db_arr)], 1
.comparado:
        mov eax, 1
        sub eax, [V(db_pasada)]         ; la pasada 0 quiere el de arranque; la 1, los demás
        cmp eax, [V(db_arr)]
        jne .otro
        mov esi, [ebx + BIO_MEDIA]
        ; al registro: sectores, tamaño del sector, MB, fijo o extraíble
        DI t_d_disco
        mov eax, [esi + 24]             ; LastBlock + 1
        mov edx, [esi + 28]
        add eax, 1
        adc edx, 0
        mov [V(db_sec)], eax
        mov [V(db_sec) + 4], edx
        call hex64
        DI t_d_sec
        mov eax, [esi + 12]
        call decimal
        DI t_d_oct
        cmp dword [esi + 12], 512
        jne .sin_mb
        DI t_parentesis
        mov eax, [V(db_sec)]
        mov edx, [V(db_sec) + 4]
        shrd eax, edx, 11               ; sectores de 512 -> MB
        call decimal
        DI t_d_mb
.sin_mb:
        push esi
        lea eax, [V(t_d_fijo)]
        cmp byte [esi + 4], 0           ; RemovableMedia
        je .fijo
        lea eax, [V(t_d_extra)]
.fijo:  mov esi, eax
        call escribir
        pop esi
        cmp byte [esi + 7], 0           ; ReadOnly
        je .escribe
        DI t_d_sololee
.escribe:
        cmp dword [V(db_arr)], 0
        je .no_arr
        DI t_d_arr
.no_arr:
        cmp dword [esi + 12], 512       ; sectores de 512 octetos
        je .de512
        DI t_d_no512
        jmp .otro
.de512: cmp dword [V(db_arr)], 0        ; (el de arranque, siempre)
        jne .vale
        cmp byte [esi + 4], 0
        jne .vale
        cmp dword [V(db_sec) + 4], 0
        jne .vale
        cmp dword [V(db_sec)], DISCO_MIN
        jae .vale
        DI t_d_chico
        jmp .otro
.vale:  ; apunta el disco
        movzx eax, byte [V(ndiscos)]
        cmp eax, 8
        jb .cabe
        DI t_d_sobra
        jmp .otro
.cabe:  lea edx, [V(discos)]
        mov [edx + eax * 8], ebx        ; Block I/O
        mov ecx, [esi]                  ; MediaId
        mov [edx + eax * 8 + 4], ecx
        mov edx, eax
        shl edx, 4
        add edx, [V(bios_base)]
        mov ecx, [V(db_sec)]
        mov [edx + 40], ecx             ; T_DISCOS: sectores
        mov ecx, [V(db_sec) + 4]
        mov [edx + 44], ecx
        mov dword [edx + 48], 512
        DI t_d_es
        add eax, 80h
        mov ecx, 2
        call hex
        DI t_d_h
        inc byte [V(ndiscos)]
.otro:
        inc dword [V(db_i)]
        jmp .cada
.fin_pasada:
        inc dword [V(db_pasada)]
        cmp dword [V(db_pasada)], 2
        jb .pasada
.fin:
        popad
        movzx eax, byte [V(ndiscos)]
        ret

; ---- discos_conectar: que el firmware ponga en marcha todas sus controladoras
;      de disco ----
; Muchos firmware, para arrancar deprisa, solo ponen en marcha el dispositivo
; del que arrancan: arrancando de la memoria USB, la eMMC o el disco SATA no
; aparecerían entre los discos (Block I/O). Se pide al firmware que conecte sus
; controladores a cada dispositivo PCI de almacenamiento (clase 01h) o lector
; de tarjetas SD/eMMC (clase 0805h): ConnectController, recursivo. Lo que ya
; estaba en marcha se queda como está. En el modo seguro no se hace.
discos_conectar:
        pushad
        cmp byte [V(seguro)], 0
        jne .f
        mov eax, [V(bootserv)]
        FW [eax + BS_LOCATEHBUF], 2, &guid_pciio, 0, &n_manejadores, &manejadores
        test eax, eax
        jnz .f
        xor esi, esi
.c:     cmp esi, [V(n_manejadores)]
        jae .libera
        mov ecx, [V(manejadores)]
        mov ebx, [ecx + esi * 4]
        mov eax, [V(bootserv)]
        FW [eax + BS_HANDLEPROT], ebx, &guid_pciio, &dc_pci
        test eax, eax
        jnz .sig
        mov ecx, [V(dc_pci)]            ; Pci.Read: las 4 primeras dobles palabras
        FW [ecx + PCIIO_LEER], ecx, 2, 0, 4, &dc_cfg
        test eax, eax
        jnz .sig
        cmp byte [V(dc_cfg) + 11], 01h  ; almacenamiento
        je .es
        cmp word [V(dc_cfg) + 10], 0805h ; controladora SD/eMMC
        jne .sig
.es:    DI t_dc
        movzx eax, word [V(dc_cfg)]
        mov ecx, 4
        call hex
        DI t_dc2
        movzx eax, word [V(dc_cfg) + 2]
        call hex
        DI t_dc3
        movzx eax, word [V(dc_cfg) + 10]
        call hex
        DI t_dc4
        call registro_paso              ; (por si el equipo se queda aquí)
        mov eax, [V(bootserv)]
        FW [eax + BS_CONNECT], ebx, 0, 0, 1
        mov ecx, 8
        call hex
        DI t_nl
.sig:   inc esi
        jmp .c
.libera:
        mov eax, [V(bootserv)]
        FW [eax + BS_FREEPOOL], [V(manejadores)]
.f:     popad
        ret

; camino_largo: ESI = camino de dispositivo -> ECX = octetos antes del nodo final
camino_largo:
        push esi
        push eax
        xor ecx, ecx
.n:     cmp byte [esi], 7Fh             ; nodo final
        je .f
        movzx eax, word [esi + 2]
        cmp eax, 4                      ; (un nodo mal hecho: hasta aquí)
        jb .f
        add esi, eax
        add ecx, eax
        cmp ecx, 2000
        jb .n
.f:     pop eax
        pop esi
        ret

; ---- vector_irq0: el vector de la IRQ 0 en el 8259 del firmware -> AL (0: no hay) ----
vector_irq0:
        push ecx
        push edx
        mov eax, [V(bootserv)]
        FW [eax + BS_LOCATEPROT], &guid_8259, 0, &p8259
        test eax, eax
        jnz .no
        mov ecx, [V(p8259)]
        FW [ecx + P8259_GETVEC], ecx, 0, &vec0
        test eax, eax
        jnz .no
        movzx eax, byte [V(vec0)]
        jmp .f
.no:    xor eax, eax
.f:     pop edx
        pop ecx
        ret

; =============================================================================
;  La memoria baja que usa el firmware y las tablas del equipo
; =============================================================================
; ---- paginas_prestar: las páginas de memoria baja que el firmware no da ----
; En casi todos los equipos el firmware se queda una o dos páginas sueltas de
; la memoria baja (el arranque de los otros procesadores), y DOS se quedaba
; sin lo que hay por encima. Esas páginas se PRESTAN: como la página 0, en
; cada cambio de modo se intercambia su contenido (bios64.asm,
; paginas_cambiar), de manera que el firmware ve siempre lo suyo y DOS lo
; suyo. Solo las que son memoria normal del firmware (tipos 0 a 4 del mapa,
; con atributos de caché); como mucho PRESTA_MAX, por debajo de la BIOS.
paginas_prestar:
        pushad
%ifdef PRUEBA_SIN_PRESTAR
        jmp .f
%endif
        cmp byte [V(seguro)], 0
        jne .f
        mov edi, 1                      ; EDI = página
        mov ebx, [V(bios_base)]
        shr ebx, 12                     ; EBX = la primera de la BIOS
.p:     cmp edi, ebx
        jae .hecho
        cmp byte [V(dada) + edi], 0
        jne .sig
        movzx eax, byte [V(presta_n)]
        cmp eax, PRESTA_MAX
        jae .hecho
        mov eax, edi
        shl eax, 12
        call pagina_tipo
        jc .hecho
        cmp eax, 4
        ja .hecho
        test edx, 0Fh                   ; (sin atributos de caché no es memoria)
        jz .hecho
        movzx eax, byte [V(presta_n)]
        mov edx, edi
        shl edx, 12
        mov [V(presta_dir) + eax * 4], edx
        inc byte [V(presta_n)]
        mov byte [V(dada) + edi], 2
.sig:   inc edi
        jmp .p
.hecho: cmp byte [V(presta_n)], 0
        je .f
        mov dword [V(dir_pag)], 0FFFFFFFFh  ; dónde se guarda la otra versión de
        mov dword [V(dir_pag) + 4], 0       ; cada página
        mov eax, [V(bootserv)]
        FW [eax + BS_ALLOCPAGES], ALLOC_MAX, MEM_LOADERDATA, PRESTA_MAX, &dir_pag
        test eax, eax
        jnz .sin_mem
        mov edi, [V(dir_pag)]
        mov [V(presta_guarda)], edi
        mov ecx, PRESTA_MAX * 1024      ; (DOS las ve a cero)
        xor eax, eax
        cld
        rep stosd
        DI t_presta
        xor edi, edi
.e:     mov eax, [V(presta_dir) + edi * 4]
        mov ecx, 5
        call hex
        DI t_espacio
        inc edi
        movzx eax, byte [V(presta_n)]
        cmp edi, eax
        jb .e
        DI t_nl
        jmp .f
.sin_mem:                               ; sin sitio para guardarlas: no se prestan
        movzx ecx, byte [V(presta_n)]
.q:     mov eax, [V(presta_dir) + ecx * 4 - 4]
        shr eax, 12
        mov byte [V(dada) + eax], 0
        dec ecx
        jnz .q
        mov byte [V(presta_n)], 0
.f:     popad
        ret

; pagina_tipo: EAX = dirección -> EAX = tipo en el mapa de memoria y EDX = sus
; atributos (la mitad baja); CF si no está en el mapa
pagina_tipo:
        push ebx
        push ecx
        push esi
        mov esi, [V(mapa_dir)]
        mov ecx, [V(mapa_tam)]
        add ecx, esi
.e:     cmp esi, ecx
        jae .no
        cmp dword [esi + 12], 0         ; (un tramo por encima de 4 GB)
        jne .sig
        mov ebx, [esi + 8]
        cmp eax, ebx
        jb .sig
        mov edx, [esi + 24]
        cmp edx, 100000h                ; (hasta el final de los 4 GB)
        jae .es
        shl edx, 12
        add edx, ebx
        jc .es
        cmp eax, edx
        jae .sig
.es:    mov eax, [esi]
        mov edx, [esi + 32]
        clc
        jmp .f
.sig:   add esi, [V(desc_tam)]
        jmp .e
.no:    stc
.f:     pop esi
        pop ecx
        pop ebx
        ret

; guid_igual: ESI y EDI = dos GUID -> ZF = 1 si son el mismo
guid_igual:
        push esi
        push edi
        push ecx
        mov ecx, 4
        cld
        repe cmpsd
        pop ecx
        pop edi
        pop esi
        ret

; ---- tablas_copiar: las entradas de ACPI (RSDP) y de SMBIOS, a la BIOS ----
; Con UEFI esas entradas no están donde las busca un programa de modo real
; (la EBDA y de E0000h a FFFFFh): solo se llega a ellas por la tabla de
; configuración del firmware. Se copian a la zona EBDA de la BIOS de modo
; real (RSDP en +10h, SMBIOS en +40h), que las pone a la vista (bios_inicio):
; 40:0Eh apunta a la EBDA y, si F0000h es memoria que se puede escribir,
; también allí. Las tablas en sí se quedan donde están. De paso, el
; fabricante y el modelo del equipo (SMBIOS) van al registro y a la pantalla.
tablas_copiar:
        pushad
        mov byte [V(callado)], 1
        xor eax, eax
        mov [V(tc_acpi)], eax           ; la RSDP
        mov [V(tc_smb)], eax            ; la entrada de SMBIOS («_SM_»)
        mov [V(tc_smb3)], eax           ; la de SMBIOS 3 («_SM3_»)
        mov eax, [V(sistema)]
        mov ecx, [eax + ST_NTABLAS]
        mov ebx, [eax + ST_TABLAS]
        test ebx, ebx
        jz .buscadas
.t:     test ecx, ecx
        jz .buscadas
        mov esi, ebx
        mov eax, [ebx + 16]             ; el puntero de esta entrada
        lea edi, [V(guid_acpi2)]
        call guid_igual
        jne .t1
        mov [V(tc_acpi)], eax           ; (la de ACPI 2.0 manda)
        mov byte [V(tc_acpi_es2)], 1
        jmp .ts
.t1:    lea edi, [V(guid_acpi1)]
        call guid_igual
        jne .t2
        cmp byte [V(tc_acpi_es2)], 0
        jne .ts
        mov [V(tc_acpi)], eax
        jmp .ts
.t2:    lea edi, [V(guid_smbios)]
        call guid_igual
        jne .t3
        mov [V(tc_smb)], eax
        jmp .ts
.t3:    lea edi, [V(guid_smbios3)]
        call guid_igual
        jne .ts
        mov [V(tc_smb3)], eax
.ts:    add ebx, 20
        dec ecx
        jmp .t
.buscadas:
        mov edx, [V(bios_base)]
        mov eax, [V(tc_acpi)]
        mov [V(rsdp)], eax              ; (para reloj_mirar: las tablas HPET y APIC)
        xor ebx, ebx                    ; EBX = T_TABLAS
        DI t_t_acpi
        test eax, eax
        jz .sin_acpi
        mov ecx, 8
        call hex
        DI t_t_rev
        mov esi, eax
        movzx eax, byte [esi + 15]
        call decimal
        DI t_parentesis2
        lea edi, [edx + EBDA + 10h]     ; 20 octetos (ACPI 1.0) o 36
        mov ecx, 20
        cmp byte [esi + 15], 2
        jb .ac
        mov ecx, 36
.ac:    cld
        push esi
        rep movsb
        pop esi
        or ebx, 1
        cmp dword [esi + 16], 0         ; ¿hay RSDT (de 32 bits)?
        jne .smbios
        DI t_t_xsdt
        jmp .smbios
.sin_acpi:
        DI t_t_no
.smbios:
        DI t_t_smbios
        mov esi, [V(tc_smb)]
        mov ecx, 31                     ; «_SM_»: 31 octetos
        test esi, esi
        jnz .sm
        mov esi, [V(tc_smb3)]
        mov ecx, 24                     ; «_SM3_»: 24
        test esi, esi
        jz .sin_smbios
.sm:    mov eax, esi
        push ecx
        mov ecx, 8
        call hex
        pop ecx
        lea edi, [edx + EBDA + 40h]
        cld
        rep movsb
        or ebx, 2
        jmp .fin
.sin_smbios:
        DI t_t_no
.fin:   DI t_nl
        mov [edx + 228], bl             ; T_TABLAS
        mov byte [V(callado)], 0
        call smbios_decir
        popad
        ret

; ---- smbios_decir: el equipo y su firmware según SMBIOS, al registro y a la
;      pantalla: «Equipo: fabricante modelo (versión); placa ...; BIOS ...» ----
; (Especificación SMBIOS de la DMTF: la entrada «_SM_» lleva la tabla en +18h
; y su tamaño en +16h; la «_SM3_», en +10h y +0Ch. Cada estructura: tipo,
; largo de la parte fija y, detrás, sus textos acabados en 0 con otro 0 al
; final. Tipo 0: la BIOS; 1: el equipo; 2: la placa.)
smbios_decir:
        pushad
        mov esi, [V(tc_smb)]
        test esi, esi
        jz .sm3
        cmp dword [esi], '_SM_'
        jne .sm3
        mov ebx, [esi + 18h]            ; la tabla
        movzx ecx, word [esi + 16h]     ; y lo que ocupa
        jmp .tabla
.sm3:   mov esi, [V(tc_smb3)]
        test esi, esi
        jz .f
        cmp dword [esi], '_SM3'
        jne .f
        cmp dword [esi + 14h], 0        ; (por encima de 4 GB)
        jne .f
        mov ebx, [esi + 10h]
        mov ecx, [esi + 0Ch]
.tabla: test ebx, ebx
        jz .f
        cmp ecx, 100000h
        ja .f
        add ecx, ebx
        mov [V(sd_fin)], ecx
        xor eax, eax
        mov [V(sd_t0)], eax
        mov [V(sd_t1)], eax
        mov [V(sd_t2)], eax
        mov edx, 400                    ; (como mucho, tantas estructuras)
.e:     lea eax, [ebx + 4]
        cmp eax, [V(sd_fin)]
        ja .vistas
        movzx eax, byte [ebx]
        movzx ecx, byte [ebx + 1]
        cmp ecx, 4
        jb .vistas
        cmp eax, 127                    ; el final
        je .vistas
        cmp eax, 2
        ja .salta
        cmp dword [V(sd_t0) + eax * 4], 0
        jne .salta
        mov [V(sd_t0) + eax * 4], ebx
.salta: add ebx, ecx                    ; los textos: hasta dos ceros seguidos
.z:     lea eax, [ebx + 2]
        cmp eax, [V(sd_fin)]
        ja .vistas
        cmp word [ebx], 0
        je .zf
        inc ebx
        jmp .z
.zf:    add ebx, 2
        dec edx
        jnz .e
.vistas:
        mov ebx, [V(sd_t1)]
        test ebx, ebx
        jz .placa
        DI t_s_equipo
        mov al, [ebx + 4]               ; fabricante
        call smb_texto
        DI t_espacio
        mov al, [ebx + 5]               ; modelo
        call smb_texto
        DI t_parentesis
        mov al, [ebx + 6]               ; versión
        call smb_texto
        DI t_parentesis2
.placa: mov ebx, [V(sd_t2)]
        test ebx, ebx
        jz .bios
        cmp byte [ebx + 1], 6
        jb .bios
        DI t_s_placa
        mov al, [ebx + 4]
        call smb_texto
        DI t_espacio
        mov al, [ebx + 5]
        call smb_texto
.bios:  mov ebx, [V(sd_t0)]
        test ebx, ebx
        jz .nl
        cmp byte [ebx + 1], 9
        jb .nl
        DI t_s_bios
        mov al, [ebx + 4]               ; quién la ha hecho
        call smb_texto
        DI t_espacio
        mov al, [ebx + 5]               ; versión
        call smb_texto
        DI t_espacio
        mov al, [ebx + 8]               ; fecha
        call smb_texto
.nl:    mov eax, [V(sd_t0)]
        or eax, [V(sd_t1)]
        or eax, [V(sd_t2)]
        jz .f
        DI t_nl
.f:     popad
        ret

; smb_texto: EBX = estructura de SMBIOS, AL = número de texto (1...; 0: no hay)
; -> lo escribe (40 letras como mucho; lo que no sea ASCII, «?»)
smb_texto:
        pushad
        movzx edx, al
        test edx, edx
        jz .no
        movzx eax, byte [ebx + 1]
        lea esi, [ebx + eax]
.b:     dec edx
        jz .es
.s:     cmp esi, [V(sd_fin)]
        jae .no
        lodsb
        test al, al
        jnz .s
        cmp byte [esi], 0               ; (se han acabado los textos)
        je .no
        jmp .b
.es:    lea edi, [V(buf_txt)]
        mov ecx, 40
.c:     cmp esi, [V(sd_fin)]
        jae .cf
        lodsb
        test al, al
        jz .cf
        cmp al, 20h
        jb .raro
        cmp al, 7Eh
        jbe .pon
.raro:  mov al, '?'
.pon:   stosb
        dec ecx
        jnz .c
.cf:    mov byte [edi], 0
        DI buf_txt
        popad
        ret
.no:    DI t_s_nada
        popad
        ret

%include "efi32_raton.inc"
%include "efi32_tecl.inc"
%include "efi32_reloj.inc"
%include "efi32_pant.inc"
%include "efi32_datos.inc"

PE_FIN
