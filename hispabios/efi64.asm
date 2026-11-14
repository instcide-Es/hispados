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

ALLOC_ADDRESS   equ 2           ; AllocateAddress
MEM_LOADERDATA  equ 2           ; EfiLoaderData
MAPA_MAX        equ 16384       ; octetos para el mapa de memoria
TR_BASE         equ 8000h       ; donde va el trampolín (tramp64.asm)
TR_MARCA        equ TR_BASE + 0E00h

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
t_vuelta        db 'De vuelta en el firmware. Marca del modo real: ', 0

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
