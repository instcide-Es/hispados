; =============================================================================
;  HispaBIOS (IA32) - parte UEFI para firmware de 32 bits
; =============================================================================
;  Lo mismo que efi64.asm, para los equipos con UEFI de 32 bits (muchas
;  tabletas y portátiles pequeños con Atom, y algunos Mac antiguos): pide al
;  firmware la memoria baja, busca la pantalla (GOP) y los discos (Block I/O),
;  copia la BIOS de modo real (bios64.asm ensamblada con ARQ32 = BIOS32.BIN)
;  a BASE y la arranca. Después atiende sus peticiones (despachador).
;
;  Convención de UEFI en IA32: la de C (cdecl), con los argumentos en la pila
;  de derecha a izquierda, el que llama la limpia, el resultado en EAX y se
;  conservan EBX, ESI, EDI y EBP. Los valores de 64 bits (EFI_LBA,
;  EFI_PHYSICAL_ADDRESS) ocupan dos palabras de la pila, la baja primero.
;  Las estructuras siguen la alineación natural (los UINT64 a 8).
;
;  La imagen no tiene reubicaciones: EBP lleva siempre la dirección donde el
;  firmware la ha cargado y todo se direcciona como [ebp + etiqueta].
;
;  Referencias: especificación UEFI 2.x (capítulos 2.3.2, 4, 7, 11, 12 y 13).
; =============================================================================
%define PE_ARQ 32
%include "pe.inc"

; ---- tablas de UEFI (IA32: punteros de 4 octetos) ----
ST_CONIN        equ 36
ST_CONOUT       equ 44
ST_RUNTIME      equ 56
ST_BOOTSERV     equ 60
TXT_OUTSTR      equ 4
TXT_READKEY     equ 4
BS_ALLOCPAGES   equ 32
BS_HANDLEPROT   equ 88
BS_STALL        equ 136
BS_LOCATEHBUF   equ 168
BS_LOCATEPROT   equ 172
RT_RESET        equ 64
LI_DEVICE       equ 12          ; EFI_LOADED_IMAGE.DeviceHandle
BIO_MEDIA       equ 8           ; EFI_BLOCK_IO.Media
BIO_READ        equ 16
BIO_WRITE       equ 20
P8259_GETVEC    equ 16
GOP_MODE        equ 12
GM_INFO         equ 8
GM_FB           equ 16

ALLOC_ADDRESS   equ 2           ; AllocateAddress
MEM_LOADERDATA  equ 2           ; EfiLoaderData
BIOS_BASE       equ 90000h      ; la BIOS de modo real
TX_BASE         equ BIOS_BASE + 8000h   ; su pantalla de texto (80x25x2)

%define V(x) ebp + (x)          ; una variable de la imagen

; ---- base: EBP = dirección de pe_inicio (donde se ha cargado la imagen) ----
%macro BASE_EBP 0
        call %%aqui
%%aqui: pop ebp
        sub ebp, %%aqui
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

        lea esi, [V(t_titulo)]
        call escribir

        ; ---- pide las páginas bajas una a una (de 0 a 9F000h) ----
        xor edi, edi                    ; EDI = dirección de la página
        xor ebx, ebx                    ; EBX = cuántas se han dado
        mov dword [V(bajo_tope)], BIOS_BASE
.pagina:
        mov [V(dir_pag)], edi
        mov dword [V(dir_pag) + 4], 0
        lea eax, [V(dir_pag)]
        push eax
        push dword 1
        push dword MEM_LOADERDATA
        push dword ALLOC_ADDRESS
        mov eax, [V(bootserv)]
        call [eax + BS_ALLOCPAGES]
        add esp, 16
        test eax, eax
        jnz .no_dada
        inc ebx
        jmp .otra
.no_dada:
        cmp edi, 1000h
        jb .no_cuenta
        cmp edi, [V(bajo_tope)]
        jae .no_cuenta
        mov [V(bajo_tope)], edi
.no_cuenta:
        lea esi, [V(t_no_dada)]
        call escribir
        mov eax, edi
        call hex32
        lea esi, [V(t_nl)]
        call escribir
.otra:
        add edi, 1000h
        cmp edi, 0A0000h
        jb .pagina
        lea esi, [V(t_dadas)]
        call escribir
        mov eax, ebx
        call hex32
        lea esi, [V(t_de160)]
        call escribir

        ; ---- la BIOS de modo real ----
        call gop_buscar
        jc .sin_gop
        lea esi, [V(t_viaje)]
        call escribir
        mov edi, BIOS_BASE              ; la zona de la BIOS, a cero
        mov ecx, 0F000h / 4
        xor eax, eax
        cld
        rep stosd
        lea esi, [V(bios)]              ; la BIOS, a su sitio
        mov edi, BIOS_BASE
        mov ecx, bios_finbin - bios
        rep movsb
        lea eax, [V(despachador)]
        mov [BIOS_BASE + 8], eax        ; T_DESPACHADOR
        mov eax, [V(bajo_tope)]         ; T_MEMKB
        shr eax, 10
        mov [BIOS_BASE + 28], ax
        call memoria_ext                ; T_EXTKB
        mov [BIOS_BASE + 32], eax
        call tsc_calibrar               ; T_TSC_TICK
        mov [BIOS_BASE + 36], eax
        call vector_irq0                ; T_VEC0
        mov [BIOS_BASE + 182], al
        call discos_buscar              ; T_NDISCOS y T_DISCOS
        mov [BIOS_BASE + 30], al
        test al, al
        jnz .hay_discos
        lea esi, [V(t_sin_discos)]
        call escribir
        jmp .fin
.hay_discos:
        mov eax, BIOS_BASE
        call eax                        ; (no vuelve mientras DOS funcione)
        jmp .fin
.sin_gop:
        lea esi, [V(t_sin_gop)]
        call escribir

.fin:
        push dword 60000000             ; 60 s, para poder leerlo
        mov eax, [V(bootserv)]
        call [eax + BS_STALL]
        add esp, 4
        xor eax, eax
        pop edi
        pop esi
        pop ebx
        pop ebp
        ret

; ---- memoria_ext: pide al firmware la memoria seguida desde 1 MB -> EAX = KB ----
memoria_ext:
        push ebx
        mov ebx, 16384                  ; 64 MB en páginas; si no, la mitad...
.prueba:
        mov dword [V(dir_pag)], 100000h
        mov dword [V(dir_pag) + 4], 0
        lea eax, [V(dir_pag)]
        push eax
        push ebx
        push dword MEM_LOADERDATA
        push dword ALLOC_ADDRESS
        mov eax, [V(bootserv)]
        call [eax + BS_ALLOCPAGES]
        add esp, 16
        test eax, eax
        jz .dada
        shr ebx, 1
        cmp ebx, 256                    ; menos de 1 MB: nada
        jae .prueba
        xor eax, eax
        jmp .fin
.dada:  mov eax, ebx
        shl eax, 2                      ; páginas -> KB
.fin:   pop ebx
        ret

; ---- tsc_calibrar: -> EAX = cuentas del TSC en un tic de la BIOS (1/18,2 s) ----
tsc_calibrar:
        push ebx
        push esi
        rdtsc
        mov ebx, eax
        mov esi, edx
        push dword 50000                ; 50 ms
        mov eax, [V(bootserv)]
        call [eax + BS_STALL]
        add esp, 4
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
        pop ebx
        ret

; ---- discos_buscar: el disco de arranque (80h) y los demás -> AL = cuántos ----
discos_buscar:
        push ebx
        push esi
        push edi
        sub esp, 12                     ; [esp] = índice, [esp+4] = pasada, [esp+8] = manejador
        mov byte [V(ndiscos)], 0
        ; el camino del dispositivo del que se ha arrancado
        lea eax, [V(imagen_cargada)]
        push eax
        lea eax, [V(guid_imagen)]
        push eax
        push dword [V(imagen)]
        mov eax, [V(bootserv)]
        call [eax + BS_HANDLEPROT]
        add esp, 12
        test eax, eax
        jnz .fin
        mov eax, [V(imagen_cargada)]
        lea ecx, [V(camino_arranque)]
        push ecx
        lea ecx, [V(guid_camino)]
        push ecx
        push dword [eax + LI_DEVICE]
        mov eax, [V(bootserv)]
        call [eax + BS_HANDLEPROT]
        add esp, 12
        test eax, eax
        jnz .fin
        ; todos los manejadores con Block I/O
        lea eax, [V(manejadores)]
        push eax
        lea eax, [V(n_manejadores)]
        push eax
        push dword 0
        lea eax, [V(guid_bloques)]
        push eax
        push dword 2                    ; ByProtocol
        mov eax, [V(bootserv)]
        call [eax + BS_LOCATEHBUF]
        add esp, 20
        test eax, eax
        jnz .fin
        ; dos pasadas: primero el disco que contiene el camino de arranque
        ; (será el 80h) y después los demás discos enteros
        mov dword [esp + 4], 0
.pasada:
        mov dword [esp], 0
.cada:
        mov eax, [esp]
        cmp eax, [V(n_manejadores)]
        jae .fin_pasada
        mov ecx, [V(manejadores)]
        mov eax, [ecx + eax * 4]
        mov [esp + 8], eax              ; el manejador
        lea ecx, [V(bloques)]
        push ecx
        lea ecx, [V(guid_bloques)]
        push ecx
        push eax
        mov eax, [V(bootserv)]
        call [eax + BS_HANDLEPROT]
        add esp, 12
        test eax, eax
        jnz .otro
        mov ebx, [V(bloques)]
        mov esi, [ebx + BIO_MEDIA]
        cmp byte [esi + 5], 0           ; MediaPresent
        je .otro
        cmp byte [esi + 6], 0           ; LogicalPartition: solo discos enteros
        jne .otro
        cmp dword [esi + 12], 512       ; sectores de 512 octetos
        jne .otro
        ; ¿contiene el camino de arranque?
        lea ecx, [V(camino)]
        push ecx
        lea ecx, [V(guid_camino)]
        push ecx
        push dword [esp + 16]           ; (el manejador, 8 + 2 empujados)
        mov eax, [V(bootserv)]
        call [eax + BS_HANDLEPROT]
        add esp, 12
        xor edx, edx                    ; EDX = 1 si es el de arranque
        test eax, eax
        jnz .comparado
        mov esi, [V(camino)]
        call camino_largo               ; ECX = largo sin el nodo final
        mov edi, [V(camino_arranque)]
        mov esi, [V(camino)]
        cld
        repe cmpsb
        mov edx, 0
        jne .comparado
        mov edx, 1
.comparado:
        mov eax, 1
        sub eax, [esp + 4]              ; pasada 0 quiere EDX = 1; pasada 1, EDX = 0
        cmp edx, eax
        jne .otro
        ; apunta el disco
        movzx eax, byte [V(ndiscos)]
        cmp eax, 8
        jae .otro
        lea edx, [V(discos)]
        mov [edx + eax * 8], ebx        ; Block I/O
        mov esi, [ebx + BIO_MEDIA]
        mov ecx, [esi]                  ; MediaId
        mov [edx + eax * 8 + 4], ecx
        shl eax, 4
        mov ecx, [esi + 24]             ; LastBlock + 1
        mov edx, [esi + 28]
        add ecx, 1
        adc edx, 0
        mov [BIOS_BASE + 40 + eax], ecx ; T_DISCOS: sectores
        mov [BIOS_BASE + 44 + eax], edx
        mov dword [BIOS_BASE + 48 + eax], 512
        inc byte [V(ndiscos)]
.otro:
        inc dword [esp]
        jmp .cada
.fin_pasada:
        inc dword [esp + 4]
        cmp dword [esp + 4], 2
        jb .pasada
.fin:
        movzx eax, byte [V(ndiscos)]
        add esp, 12
        pop edi
        pop esi
        pop ebx
        ret

; camino_largo: ESI = camino de dispositivo -> ECX = octetos antes del nodo final
camino_largo:
        push esi
        xor ecx, ecx
.n:     cmp byte [esi], 7Fh             ; nodo final
        je .f
        movzx eax, word [esi + 2]
        add esi, eax
        add ecx, eax
        jmp .n
.f:     pop esi
        ret

; ---- vector_irq0: el vector de la IRQ 0 en el 8259 del firmware -> AL (0: no hay) ----
vector_irq0:
        lea eax, [V(p8259)]
        push eax
        push dword 0
        lea eax, [V(guid_8259)]
        push eax
        mov eax, [V(bootserv)]
        call [eax + BS_LOCATEPROT]
        add esp, 12
        test eax, eax
        jnz .no
        lea eax, [V(vec0)]
        push eax
        push dword 0                    ; IRQ 0
        mov eax, [V(p8259)]
        push eax
        call [eax + P8259_GETVEC]
        add esp, 12
        test eax, eax
        jnz .no
        movzx eax, byte [V(vec0)]
        ret
.no:    xor eax, eax
        ret

; ---- despachador: las peticiones de la BIOS (ECX = petición, EDX = argumento) ----
; (llega con las interrupciones cerradas; las abre mientras trabaja el firmware)
despachador:
        push ebx
        push esi
        push edi
        push ebp
        BASE_EBP
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
        push edx
        mov eax, [V(bootserv)]
        sti
        call [eax + BS_STALL]
        cli
        add esp, 4
        xor eax, eax
        jmp .fin
.leer:
.escribir:
        mov ebx, ecx                    ; 3 = leer, 4 = escribir
        movzx eax, byte [BIOS_BASE + 168]       ; P_DISCO
        cmp al, [V(ndiscos)]
        jae .mal
        lea esi, [V(discos)]
        mov edi, [esi + eax * 8]        ; Block I/O
        push dword BIOS_BASE + 0C000h   ; el búfer de paso
        movzx eax, word [BIOS_BASE + 170]       ; P_N
        shl eax, 9                      ; octetos
        push eax
        push dword [BIOS_BASE + 176]    ; P_LBA (alto)
        push dword [BIOS_BASE + 172]    ; P_LBA (bajo)
        movzx eax, byte [BIOS_BASE + 168]
        push dword [esi + eax * 8 + 4]  ; MediaId
        push edi                        ; This
        sti
        cmp ebx, 3
        jne .w
        call [edi + BIO_READ]
        jmp .rw
.w:     call [edi + BIO_WRITE]
.rw:    cli
        add esp, 24
        test eax, eax
        jnz .mal
        xor eax, eax
        jmp .fin
.mal:   mov eax, 1
        jmp .fin
.tecla:
        push dword 1000                 ; un momento para que el firmware
        mov eax, [V(bootserv)]          ; atienda el teclado (USB)
        sti
        call [eax + BS_STALL]
        add esp, 4
        lea eax, [V(tecla_efi)]
        push eax
        mov eax, [V(sistema)]
        mov eax, [eax + ST_CONIN]
        push eax
        call [eax + TXT_READKEY]
        cli
        add esp, 8
        test eax, eax
        jnz .sin_tecla
        mov eax, [V(tecla_efi)]         ; ScanCode (bajo) y UnicodeChar (alto)
        jmp .fin
.sin_tecla:
        mov eax, -1
        jmp .fin
.reiniciar:
        push dword 0                    ; ResetData
        push dword 0                    ; DataSize
        push dword 0                    ; ResetStatus
        push edx                        ; EfiResetCold = 0, EfiResetShutdown = 2
        mov eax, [V(sistema)]
        mov eax, [eax + ST_RUNTIME]
        call [eax + RT_RESET]
        add esp, 16
        mov eax, -1
.fin:   pop ebp
        pop edi
        pop esi
        pop ebx
        ret

; ---- gop_buscar: localiza la salida gráfica -> fb, ancho, alto, línea; CF si no ----
gop_buscar:
        push ebx
        lea eax, [V(gop)]
        push eax
        push dword 0
        lea eax, [V(guid_gop)]
        push eax
        mov eax, [V(bootserv)]
        call [eax + BS_LOCATEPROT]
        add esp, 12
        test eax, eax
        jnz .no
        mov ebx, [V(gop)]
        mov ebx, [ebx + GOP_MODE]
        cmp dword [ebx + GM_FB + 4], 0  ; (en 32 bits, por debajo de 4 GB)
        jne .no
        mov eax, [ebx + GM_FB]
        mov [V(fb)], eax
        mov ebx, [ebx + GM_INFO]
        mov eax, [ebx + 4]
        mov [V(fb_ancho)], eax
        mov eax, [ebx + 8]
        mov [V(fb_alto)], eax
        mov eax, [ebx + 12]
        mov [V(fb_formato)], eax        ; 0 = RGBX, 1 = BGRX
        mov eax, [ebx + 32]
        mov [V(fb_linea)], eax
        cmp dword [V(fb_ancho)], 640
        jb .no
        cmp dword [V(fb_alto)], 400
        jb .no
        cmp dword [V(fb_formato)], 1
        ja .no
        pop ebx
        clc
        ret
.no:    pop ebx
        stc
        ret

; ---- escala: tablas de dónde cae cada punto de 640x400 en la pantalla real ----
; Se amplía lo más posible sin deformar (8:5): en 1920x1080, a 1728x1080.
; tab_x[i] = x0 + i*An/640 (i = 0..640) y tab_y[j] = y0 + j*Al/400 (j = 0..400)
escala:
        pushad
        mov eax, [V(fb_ancho)]
        mov ecx, [V(fb_alto)]
        lea edx, [eax * 4 + eax]        ; An*5 <= Al*8: manda el ancho
        lea ebx, [ecx * 8]
        cmp edx, ebx
        ja .alto
        mov [V(dib_an)], eax
        shr edx, 3
        mov [V(dib_al)], edx
        jmp .origen
.alto:  mov [V(dib_al)], ecx
        mov eax, ebx                    ; Al*8/5
        xor edx, edx
        mov ebx, 5
        div ebx
        mov [V(dib_an)], eax
.origen:
        mov eax, [V(fb_ancho)]
        sub eax, [V(dib_an)]
        shr eax, 1
        mov esi, eax                    ; ESI = x0
        xor ecx, ecx
.tx:    mov eax, ecx
        mul dword [V(dib_an)]
        mov ebx, 640
        div ebx
        add eax, esi
        mov [V(tab_x) + ecx * 4], eax
        inc ecx
        cmp ecx, 640
        jbe .tx
        mov eax, [V(fb_alto)]
        sub eax, [V(dib_al)]
        shr eax, 1
        mov esi, eax                    ; ESI = y0
        xor ecx, ecx
.ty:    mov eax, ecx
        mul dword [V(dib_al)]
        mov ebx, 400
        div ebx
        add eax, esi
        mov [V(tab_y) + ecx * 4], eax
        inc ecx
        cmp ecx, 400
        jbe .ty
        popad
        ret

; ---- pantalla_dibujar: pinta la pantalla de texto TX_BASE (80x25), ampliada ----
pantalla_dibujar:
        pushad
        ; la primera vez se borra todo en negro; después solo se pintan las
        ; celdas que han cambiado (escribir en la memoria de vídeo real es
        ; lento y mientras tanto el reloj de DOS no corre)
        cld
        cmp byte [V(borrada)], 0
        jne .ya_borrada
        mov byte [V(borrada)], 1
        call escala
        mov edi, [V(fb)]
        mov eax, [V(fb_linea)]
        imul eax, [V(fb_alto)]
        mov ecx, eax
        xor eax, eax
        rep stosd
        lea edi, [V(sombra)]            ; nada pintado todavía
        mov ecx, 2000
        mov ax, 0FFFFh
        rep stosw
.ya_borrada:
        xor edi, edi                    ; EDI = número de celda (0..1999)
.celda:
        movzx ebx, word [TX_BASE + edi * 2]  ; BL = carácter, BH = atributo
        cmp bx, [V(sombra) + edi * 2]
        je .siguiente                   ; igual que la última vez
        mov [V(sombra) + edi * 2], bx
        movzx eax, bh
        and eax, 0Fh
        mov ecx, [V(paleta) + eax * 4]  ; color de la letra
        movzx eax, bh
        shr eax, 4
        and eax, 7
        mov edx, [V(paleta) + eax * 4]  ; color del fondo
        cmp dword [V(fb_formato)], 0
        jne .bgr
        bswap ecx                       ; RGBX: R y B cambiados
        shr ecx, 8
        bswap edx
        shr edx, 8
.bgr:   movzx eax, bl
        shl eax, 4
        lea esi, [V(fuente) + eax]      ; ESI = las 16 filas del carácter
        mov eax, edi
        push edx
        xor edx, edx
        mov ebx, 80
        div ebx                         ; EAX = fila, EDX = columna
        mov ebx, edx
        pop edx
        call pintar_celda
.siguiente:
        inc edi
        cmp edi, 2000
        jb .celda
        popad
        ret

; pintar_celda: EAX = fila, EBX = columna, ESI = letra (16 octetos),
; ECX = color de la letra, EDX = del fondo. Cada punto de 640x400 es un
; rectángulo de la pantalla real (tab_x, tab_y).
pintar_celda:
        pushad
        mov [V(pc_tin)], ecx
        mov [V(pc_fon)], edx
        shl eax, 4
        mov [V(pc_y)], eax
        shl ebx, 3
        mov [V(pc_x)], ebx
        mov dword [V(pc_gy)], 16
.fila:  mov al, [esi]
        inc esi
        mov [V(pc_bits)], al
        mov eax, [V(pc_y)]
        mov ecx, [V(tab_y) + eax * 4]
        mov [V(pc_y0)], ecx
        mov ecx, [V(tab_y) + eax * 4 + 4]
        mov [V(pc_y1)], ecx
        mov ebx, [V(pc_x)]
        mov dword [V(pc_gx)], 8
.punto: mov eax, [V(pc_fon)]
        shl byte [V(pc_bits)], 1
        jnc .color
        mov eax, [V(pc_tin)]
.color: mov ecx, [V(tab_x) + ebx * 4]   ; ECX = x, EDX = ancho
        mov edx, [V(tab_x) + ebx * 4 + 4]
        sub edx, ecx
        jz .sig
        push ebx
        mov ebx, [V(pc_y0)]
.y:     cmp ebx, [V(pc_y1)]
        jae .ya
        mov edi, ebx
        imul edi, [V(fb_linea)]
        add edi, ecx
        shl edi, 2
        add edi, [V(fb)]
        push ecx
        mov ecx, edx
        rep stosd
        pop ecx
        inc ebx
        jmp .y
.ya:    pop ebx
.sig:   inc ebx
        dec dword [V(pc_gx)]
        jnz .punto
        inc dword [V(pc_y)]
        dec dword [V(pc_gy)]
        jnz .fila
        popad
        ret

; ---- escribir: ESI = texto ASCII terminado en 0 -> por el servicio de texto ----
escribir:
        pushad
        lea edi, [V(buf16)]
.c:     lodsb
        movzx eax, al
        stosw
        test al, al
        jnz .c
        lea eax, [V(buf16)]
        push eax
        mov eax, [V(conout)]
        push eax
        call [eax + TXT_OUTSTR]
        add esp, 8
        popad
        ret

; ---- hex32: EAX en hexadecimal (8 cifras) ----
hex32:
        pushad
        lea edi, [V(buf_hex) + 7]
        mov ecx, 8
.d:     mov edx, eax
        and edx, 0Fh
        cmp dl, 10
        jb .n
        add dl, 'A' - 10 - '0'
.n:     add dl, '0'
        mov [edi], dl
        dec edi
        shr eax, 4
        loop .d
        lea esi, [V(buf_hex)]
        call escribir
        popad
        ret

t_titulo        db 'HispaBIOS 0.1 (IA32)', 13, 10, 0
t_nl            db 13, 10, 0
t_no_dada       db '  el firmware no da la pagina ', 0
t_dadas         db 'Paginas bajas dadas: ', 0
t_de160         db ' (de 000000A0)', 13, 10, 0
t_sin_discos    db 'No se encuentra el disco de arranque.', 13, 10, 0
t_viaje         db 'Arrancando la BIOS de modo real...', 13, 10, 0
t_sin_gop       db 'No hay salida grafica (GOP) de 640x400 o mas.', 13, 10, 0

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
guid_gop        dd 9042A9DEh
                dw 23DCh, 4A38h
                db 96h, 0FBh, 7Ah, 0DEh, 0D0h, 80h, 51h, 6Ah
        align 8
p8259           dd 0
vec0            dd 0
imagen_cargada  dd 0
camino_arranque dd 0
camino          dd 0
bloques         dd 0
n_manejadores   dd 0
manejadores     dd 0
bajo_tope       dd 0
tecla_efi       dd 0
ndiscos         db 0
borrada         db 0
        align 8
discos          times 16 dd 0           ; Block I/O y MediaId de cada disco (8)
gop             dd 0
fb              dd 0
fb_ancho        dd 0
fb_alto         dd 0
fb_formato      dd 0
fb_linea        dd 0
; los 16 colores del modo texto (00RRGGBB)
paleta          dd 000000h, 0000AAh, 00AA00h, 00AAAAh, 0AA0000h, 0AA00AAh, 0AA5500h, 0AAAAAAh
                dd 555555h, 5555FFh, 55FF55h, 55FFFFh, 0FF5555h, 0FF55FFh, 0FFFF55h, 0FFFFFFh
fuente:         incbin "FUENTE16.BIN"
        align 8
imagen          dd 0
sistema         dd 0
conout          dd 0
bootserv        dd 0
        align 8
dir_pag         dq 0                    ; EFI_PHYSICAL_ADDRESS (64 bits)
buf_hex         times 8 db '0'
                db 0
                align 4
buf16           times 256 dw 0
sombra          times 2000 dw 0         ; lo que hay pintado (carácter y atributo)
tab_x           times 641 dd 0
tab_y           times 401 dd 0
dib_an          dd 0
dib_al          dd 0
pc_tin          dd 0
pc_fon          dd 0
pc_x            dd 0
pc_y            dd 0
pc_y0           dd 0
pc_y1           dd 0
pc_gx           dd 0
pc_gy           dd 0
pc_bits         db 0
bios:           incbin "BIOS32.BIN"
bios_finbin:

PE_FIN
