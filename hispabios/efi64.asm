; =============================================================================
;  HispaBIOS (x64) - parte UEFI
; =============================================================================
;  Paso 2: lee el mapa de memoria del firmware, enseña lo que hay por debajo
;  de 1 MB y pide al firmware, página a página, la memoria baja que necesita
;  DOS (de 0 a 9FFFFh), para saber qué deja libre.
; Paso 3: baja al modo real con el trampolín (tramp64.asm) y vuelve.
; =============================================================================
%define PE_ARQ 64
%include "pe.inc"

; ---- tablas de UEFI (x64) ----
ST_CONOUT       equ 64
ST_BOOTSERV     equ 96
TXT_OUTSTR      equ 8
BS_ALLOCPAGES   equ 40
BS_GETMEMMAP    equ 56
BS_STALL        equ 248
BS_LOCATEPROT   equ 320
BS_HANDLEPROT   equ 152
BS_LOCATEHBUF   equ 312
ST_CONIN        equ 48
ST_RUNTIME      equ 88
RT_RESET        equ 104

ALLOC_ADDRESS   equ 2           ; AllocateAddress
MEM_LOADERDATA  equ 2           ; EfiLoaderData
MAPA_MAX        equ 16384       ; octetos para el mapa de memoria
BIOS_BASE       equ 90000h      ; la BIOS de modo real (bios64.asm)
TX_BASE         equ BIOS_BASE + 8000h   ; su pantalla de texto (80x25x2)

pe_entrada:
        push rbx
        push rbp
        push rsi
        push rdi
        push r12
        push r13
        sub rsp, 56                     ; 32 de la convención + 3 argumentos
        mov [rel imagen], rcx
        mov [rel sistema], rdx
        mov rax, [rdx + ST_CONOUT]
        mov [rel conout], rax
        mov rax, [rdx + ST_BOOTSERV]
        mov [rel bootserv], rax

        lea rsi, [rel t_titulo]
        call escribir

        ; ---- el mapa de memoria ----
        mov qword [rel mapa_tam], MAPA_MAX
        mov rax, [rel bootserv]
        lea rcx, [rel mapa_tam]
        lea rdx, [rel mapa]
        lea r8, [rel mapa_llave]
        lea r9, [rel desc_tam]
        lea r10, [rel desc_ver]
        mov [rsp + 32], r10
        call [rax + BS_GETMEMMAP]
        test rax, rax
        jz .mapa_bien
        lea rsi, [rel t_mapa_mal]
        call escribir
        mov rax, [rsp]                  ; (código de error ya en RAX al llamar)
        jmp .fin
.mapa_bien:
        lea rsi, [rel t_mapa]
        call escribir
        ; recorre las entradas: enseña las que empiezan por debajo de 1 MB
        lea rbx, [rel mapa]
        mov r12, [rel mapa_tam]
        add r12, rbx                    ; R12 = final del mapa
.entrada:
        cmp rbx, r12
        jae .fin_mapa
        mov rax, [rbx + 8]              ; PhysicalStart
        cmp rax, 100000h
        jae .siguiente
        lea rsi, [rel t_sangria]
        call escribir
        mov rax, [rbx + 8]
        call hex64
        lea rsi, [rel t_paginas]
        call escribir
        mov rax, [rbx + 24]             ; NumberOfPages
        call hex64
        lea rsi, [rel t_tipo]
        call escribir
        mov eax, [rbx]                  ; Type
        call hex64
        lea rsi, [rel t_nl]
        call escribir
.siguiente:
        add rbx, [rel desc_tam]
        jmp .entrada
.fin_mapa:

        ; ---- pide las páginas bajas una a una (de 0 a 9F000h) ----
        lea rsi, [rel t_pide]
        call escribir
        xor r13d, r13d                  ; R13 = dirección de la página
        xor r12d, r12d                  ; R12 = cuántas se han dado
        mov qword [rel bajo_tope], BIOS_BASE   ; la primera que no da (desde 1000h)
.pagina:
        mov [rel dir_pag], r13
        mov rax, [rel bootserv]
        mov ecx, ALLOC_ADDRESS
        mov edx, MEM_LOADERDATA
        mov r8d, 1
        lea r9, [rel dir_pag]
        call [rax + BS_ALLOCPAGES]
        test rax, rax
        jnz .no_dada
        inc r12
        jmp .otra
.no_dada:
        cmp r13, 1000h
        jb .no_cuenta
        cmp r13, [rel bajo_tope]
        jae .no_cuenta
        mov [rel bajo_tope], r13
.no_cuenta:
        lea rsi, [rel t_no_dada]        ; avisa de cada página que no da
        call escribir
        mov rax, r13
        call hex64
        lea rsi, [rel t_nl]
        call escribir
.otra:
        add r13, 1000h
        cmp r13, 0A0000h
        jb .pagina
        lea rsi, [rel t_dadas]
        call escribir
        mov rax, r12
        call hex64
        lea rsi, [rel t_de160]
        call escribir

        ; ---- paso 5: la BIOS de modo real ----
        call gop_buscar
        jc .sin_gop
        lea rsi, [rel t_viaje]
        call escribir
        mov edi, BIOS_BASE              ; la zona de la BIOS, a cero
        mov ecx, 0F000h / 8
        xor eax, eax
        cld
        rep stosq
        lea rsi, [rel bios]             ; la BIOS, a su sitio
        mov edi, BIOS_BASE
        mov ecx, bios_finbin - bios
        rep movsb
        lea rax, [rel despachador]
        mov [BIOS_BASE + 8], rax        ; T_DESPACHADOR
        mov rax, [rel bajo_tope]        ; T_MEMKB: hasta la primera página ocupada
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
        lea rsi, [rel t_sin_discos]
        call escribir
        jmp .fin
.hay_discos:
        mov eax, BIOS_BASE
        call rax                        ; (no vuelve mientras DOS funcione)
        jmp .fin
.sin_gop:
        lea rsi, [rel t_sin_gop]
        call escribir

.fin:
        mov rax, [rel bootserv]
        mov ecx, 60000000               ; 60 s, para poder leerlo
        call [rax + BS_STALL]
        xor eax, eax
        add rsp, 56
        pop r13
        pop r12
        pop rdi
        pop rsi
        pop rbp
        pop rbx
        ret




; ---- memoria_ext: pide al firmware la memoria seguida desde 1 MB -> EAX = KB ----
memoria_ext:
        push rbx
        sub rsp, 40
        mov ebx, 16384                  ; 64 MB en páginas; si no, la mitad...
.prueba:
        mov qword [rel dir_pag], 100000h
        mov rax, [rel bootserv]
        mov ecx, ALLOC_ADDRESS
        mov edx, MEM_LOADERDATA
        mov r8d, ebx
        lea r9, [rel dir_pag]
        call [rax + BS_ALLOCPAGES]
        test rax, rax
        jz .dada
        shr ebx, 1
        cmp ebx, 256                    ; menos de 1 MB: nada
        jae .prueba
        xor eax, eax
        jmp .fin
.dada:  mov eax, ebx
        shl eax, 2                      ; páginas -> KB
.fin:   add rsp, 40
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
        call [rax + BS_STALL]
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
        call [rax + BS_HANDLEPROT]
        test rax, rax
        jnz .fin
        mov rax, [rel imagen_cargada]
        mov rcx, [rax + 24]             ; DeviceHandle
        mov rax, [rel bootserv]
        lea rdx, [rel guid_camino]
        lea r8, [rel camino_arranque]
        call [rax + BS_HANDLEPROT]
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
        call [rax + BS_LOCATEHBUF]
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
        call [rax + BS_HANDLEPROT]
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
        call [rax + BS_HANDLEPROT]
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
        mov [BIOS_BASE + 40 + rax], rcx ; T_DISCOS: sectores
        mov dword [BIOS_BASE + 48 + rax], 512
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
        call [rax + BS_LOCATEPROT]
        test rax, rax
        jnz .no
        mov rcx, [rel p8259]
        xor edx, edx                    ; IRQ 0
        lea r8, [rel vec0]
        call [rcx + 32]                 ; GetVector
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
        call [rax + BS_STALL]
        cli
        xor eax, eax
        jmp .fin
.leer:
.escribir:
        mov ebx, ecx                    ; 3 = leer, 4 = escribir
        movzx eax, byte [BIOS_BASE + 168]       ; P_DISCO
        cmp al, [rel ndiscos]
        jae .mal
        shl eax, 4
        lea rsi, [rel discos]
        mov rcx, [rsi + rax]            ; Block I/O
        mov edx, [rsi + rax + 8]        ; MediaId
        mov r8, [BIOS_BASE + 172]       ; P_LBA
        movzx r9d, word [BIOS_BASE + 170]       ; P_N
        shl r9d, 9                      ; octetos
        mov r10d, BIOS_BASE + 0C000h    ; el búfer de paso
        mov [rsp + 32], r10
        sti
        cmp ebx, 3
        jne .w
        call [rcx + 24]                 ; ReadBlocks
        jmp .rw
.w:     call [rcx + 32]                 ; WriteBlocks
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
        call [rax + BS_STALL]
        mov rax, [rel sistema]
        mov rcx, [rax + ST_CONIN]
        lea rdx, [rel tecla_efi]
        call [rcx + 8]                  ; ReadKeyStroke
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
        call [rax + RT_RESET]
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
        mov rax, [rel bootserv]
        lea rcx, [rel guid_gop]
        xor edx, edx
        lea r8, [rel gop]
        call [rax + BS_LOCATEPROT]
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

; ---- pantalla_dibujar: pinta la pantalla de texto TX_BASE (80x25) centrada ----
pantalla_dibujar:
        push rbx
        push rsi
        push rdi
        push r12
        push r13
        push r14
        push r15
        ; la primera vez se borra todo en negro; después solo se pintan las
        ; celdas que han cambiado (escribir en la memoria de vídeo real es
        ; lento y mientras tanto el reloj de DOS no corre)
        cld
        cmp byte [rel borrada], 0
        jne .ya_borrada
        mov byte [rel borrada], 1
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
        ; origen: centrado
        mov eax, [rel fb_ancho]
        sub eax, 640
        shr eax, 1
        mov r14d, eax                   ; R14 = x de origen
        mov eax, [rel fb_alto]
        sub eax, 400
        shr eax, 1
        mov r15d, eax                   ; R15 = y de origen
        xor r12d, r12d                  ; R12 = fila
.fila:
        xor r13d, r13d                  ; R13 = columna
.celda:
        mov eax, r12d
        imul eax, 80
        add eax, r13d
        movzx ebx, word [TX_BASE + rax * 2]  ; BL = carácter, BH = atributo
        lea rsi, [rel sombra]
        cmp bx, [rsi + rax * 2]
        je .siguiente                   ; igual que la última vez
        mov [rsi + rax * 2], bx
        movzx eax, bh
        and eax, 0Fh
        lea rsi, [rel paleta]
        mov r8d, [rsi + rax * 4]        ; R8 = color de la letra
        movzx eax, bh
        shr eax, 4
        and eax, 7
        mov r9d, [rsi + rax * 4]        ; R9 = color del fondo
        cmp dword [rel fb_formato], 0
        jne .bgr
        bswap r8d                       ; RGBX: R y B cambiados (de 00RRGGBB a BBGGRR00)
        shr r8d, 8
        bswap r9d
        shr r9d, 8
.bgr:
        movzx eax, bl
        shl eax, 4
        lea rsi, [rel fuente]
        add rsi, rax                    ; RSI = las 16 filas del carácter
        ; RDI = fb + ((y0 + fila*16) * línea + x0 + col*8) * 4
        mov eax, r12d
        shl eax, 4
        add eax, r15d
        imul eax, [rel fb_linea]
        mov edx, r13d
        shl edx, 3
        add eax, edx
        add eax, r14d
        mov rdi, [rel fb]
        lea rdi, [rdi + rax * 4]
        mov ecx, 16
.linea:
        movzx edx, byte [rsi]
        inc rsi
        mov r10d, 8
.punto:
        shl dl, 1
        mov eax, r9d
        cmovc eax, r8d
        stosd
        dec r10d
        jnz .punto
        mov eax, [rel fb_linea]
        sub eax, 8
        lea rdi, [rdi + rax * 4]
        loop .linea
.siguiente:
        inc r13d
        cmp r13d, 80
        jb .celda
        inc r12d
        cmp r12d, 25
        jb .fila
        pop r15
        pop r14
        pop r13
        pop r12
        pop rdi
        pop rsi
        pop rbx
        ret

; ---- escribir: RSI = texto ASCII terminado en 0 -> por el servicio de texto ----
escribir:
        push rax
        push rcx
        push rdx
        push rdi
        push r8
        push r9
        push r10
        push r11
        lea rdi, [rel buf16]
.c:     lodsb
        movzx eax, al
        stosw
        test al, al
        jnz .c
        mov rcx, [rel conout]
        lea rdx, [rel buf16]
        sub rsp, 40                     ; 32 + 8 para alinear la pila
        call [rcx + TXT_OUTSTR]
        add rsp, 40
        pop r11
        pop r10
        pop r9
        pop r8
        pop rdi
        pop rdx
        pop rcx
        pop rax
        ret

; ---- hex64: RAX en hexadecimal (16 cifras) ----
hex64:
        push rax
        push rcx
        push rdi
        push rsi
        lea rdi, [rel buf_hex + 15]
        mov ecx, 16
.d:     mov edx, eax
        and edx, 0Fh
        cmp dl, 10
        jb .n
        add dl, 'A' - 10 - '0'
.n:     add dl, '0'
        mov [rdi], dl
        dec rdi
        shr rax, 4
        loop .d
        lea rsi, [rel buf_hex]
        call escribir
        pop rsi
        pop rdi
        pop rcx
        pop rax
        ret

t_titulo        db 'HispaBIOS 0.1 (x64) - paso 2: la memoria baja', 13, 10, 0
t_mapa_mal      db 'No se ha podido leer el mapa de memoria.', 13, 10, 0
t_mapa          db 'Mapa de memoria por debajo de 1 MB (inicio, paginas, tipo):', 13, 10, 0
t_sangria       db '  ', 0
t_paginas       db ' pag. ', 0
t_tipo          db ' tipo ', 0
t_nl            db 13, 10, 0
t_pide          db 'Pidiendo al firmware las paginas de 0 a 9F000h...', 13, 10, 0
t_no_dada       db '  no la da: ', 0
t_dadas         db 'Paginas dadas: ', 0
t_de160         db ' (de 00000000000000A0)', 13, 10, 0
t_sin_discos    db 'No se encuentra el disco de arranque.', 13, 10, 0
t_viaje         db 'Arrancando la BIOS de modo real...', 13, 10, 0
t_sin_gop       db 'No hay salida grafica (GOP) de 640x400 o mas.', 13, 10, 0
t_vuelta        db 'De vuelta en el firmware. Marca del modo real: ', 0



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
mapa_tam        dq 0
mapa_llave      dq 0
desc_tam        dq 0
desc_ver        dq 0
dir_pag         dq 0
buf_hex         times 16 db '0'
                db 0
                align 8
buf16           times 256 dw 0
sombra          times 2000 dw 0         ; lo que hay pintado (carácter y atributo)
mapa            times MAPA_MAX db 0
bios:           incbin "BIOS64.BIN"
bios_finbin:

PE_FIN
