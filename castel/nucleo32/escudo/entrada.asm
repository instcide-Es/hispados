; =============================================================================
;  Hispa-Pentium - ESCUDO.BIN, el antivirus: la cabecera de programa del
;  anillo 3 (HPU3, docs/PENTIUM.md sección 6) y la entrada.
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  El C de escudo.c y el motor (motor.c; la cuenta de RSA de los .cvd, con
;  BearSSL, terceros/bearssl, licencia MIT) enlazado para 0x40000000, donde
;  el núcleo pone los programas; como TLS.BIN (nucleo32/tls/entrada.asm). El
;  núcleo pone la pila (arriba del todo de la zona del programa, crece sola)
;  y deja a cero lo que va detrás del archivo.
; =============================================================================
        bits 32
        extern escudo_principal, _fin_imagen, _fin_bss
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
        call escudo_principal
        mov ebx, eax                ; acabar (llamada 2) con lo que devuelve
        mov eax, 2
        int 0x80
.nunca:
        jmp .nunca
