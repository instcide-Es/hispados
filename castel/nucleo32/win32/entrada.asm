; =============================================================================
;  Hispa-Pentium - WIN32.BIN, los programas de Windows de 32 bits: la
;  cabecera de programa del anillo 3 (HPU3, docs/PENTIUM.md sección 6) y la
;  entrada (docs/pentium/WIN32.md).
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Todo WIN32.BIN está enlazado para la dirección 0 (pentium.ld): corre con
;  los segmentos de los programas de Windows (cpu.cas, win32.cas), de base
;  0x40000000, donde el núcleo pone los programas. El núcleo lo arranca con
;  los planos (CS = 0x1B): lo primero es saltar al segmento de código suyo
;  (0x33); desde ahí, si una interrupción lo para, el núcleo le devuelve sus
;  DS y ES (0x3B) al volver. Luego, la pila (SS = 0x3B: la misma pila, con la
;  dirección menos 0x40000000) y FS (0x43, la TEB).
; =============================================================================
        bits 32
        extern w32_principal, _fin_imagen, _fin_bss
        global _inicio

BASE    equ 0x40000000

        section .entrada progbits alloc exec
_inicio:
        jmp short salto
        align 4, db 0
        dd 0x33555048               ; «HPU3»
        dd _inicio + BASE           ; dónde va (0x40000000)
        dd _fin_imagen + BASE       ; hasta dónde llega lo del archivo
        dd _fin_bss + BASE          ; hasta dónde lo que empieza a cero
        dd salto + BASE             ; por dónde se empieza
salto:
        jmp 0x33:segmentado         ; (corre en 0x40000000 + salto, con CS plano)
segmentado:
        mov ax, 0x3B
        mov ds, ax
        mov es, ax
        mov ss, ax                  ; (con «mov ss», la instrucción de detrás va sin interrupciones)
        sub esp, BASE
        mov ax, 0x43
        mov fs, ax
        xor eax, eax
        mov gs, ax
        cld
        and esp, 0xFFFFFFF0
        call w32_principal
        mov ebx, eax                ; acabar (llamada 2) con lo que devuelve
        mov eax, 2
        int 0x80
.nunca:
        jmp .nunca
