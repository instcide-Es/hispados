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

ALLOC_ADDRESS   equ 2           ; AllocateAddress
MEM_LOADERDATA  equ 2           ; EfiLoaderData
MAPA_MAX        equ 16384       ; octetos para el mapa de memoria
TR_BASE         equ 8000h       ; donde va el trampolín (tramp64.asm)
TR_MARCA        equ TR_BASE + 0E00h
TX_BASE         equ 9000h       ; pantalla de texto de DOS (80x25x2)

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

        ; ---- paso 3: ida y vuelta al modo real con el trampolín ----
        lea rsi, [rel t_viaje]
        call escribir
        lea rsi, [rel trampolin]
        mov edi, TR_BASE
        mov ecx, trampolin_fin - trampolin
        cld
        rep movsb
        mov word [TR_MARCA], 0
        mov eax, TR_BASE
        call rax
        lea rsi, [rel t_vuelta]
        call escribir
        movzx eax, word [TR_MARCA]
        call hex64
        lea rsi, [rel t_nl]
        call escribir
        ; ---- paso 4: la pantalla gráfica (GOP) y el texto dibujado ----
        call gop_buscar
        jc .sin_gop
        call pantalla_dibujar
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
        ; borra todo en negro
        mov rdi, [rel fb]
        mov eax, [rel fb_linea]
        imul eax, [rel fb_alto]
        mov ecx, eax
        xor eax, eax
        rep stosd
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
t_viaje         db 'Bajando al modo real y volviendo...', 13, 10, 0
t_sin_gop       db 'No hay salida grafica (GOP) de 640x400 o mas.', 13, 10, 0
t_vuelta        db 'De vuelta en el firmware. Marca del modo real: ', 0


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
mapa            times MAPA_MAX db 0
trampolin:      incbin "TRAMP64.BIN"
trampolin_fin:

PE_FIN
