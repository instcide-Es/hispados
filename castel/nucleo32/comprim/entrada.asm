; =============================================================================
;  Hispa-Pentium - COMPRIM.BIN, ZIP.BIN, UNZIP.BIN y MIRADOR.BIN: la cabecera
;  de programa del anillo 3 (HPU3, docs/PENTIUM.md sección 6) y la entrada.
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  El C (comprim.c, lanza.c o mirador.c) enlazado para 0x40000000, donde el
;  núcleo pone los programas; como MINIAT.BIN (nucleo32/miniat/entrada.asm).
;  PRINCIPAL (nasm -D): la función de C por la que se empieza.
; =============================================================================
        bits 32
        extern PRINCIPAL, _fin_imagen, _fin_bss
        global _inicio

        section .entrada progbits alloc exec
_inicio:
        jmp short salto
        align 4, db 0
        dd 0x33555048               ; «HPU3»
        dd _inicio                  ; dónde va (0x40000000)
        dd _fin_imagen              ; hasta dónde llega lo del archivo
        dd _fin_bss                 ; hasta dónde lo que empieza a cero
        dd salto                    ; por dónde se empieza
salto:
        cld
        and esp, 0xFFFFFFF0
        call PRINCIPAL
        mov ebx, eax                ; acabar (llamada 2) con lo que devuelve
        mov eax, 2
        int 0x80
.nunca:
        jmp .nunca
