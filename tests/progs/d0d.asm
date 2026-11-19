; =============================================================================
;  D0D.COM - prueba: INT 21h AH=0Dh (en Hispa-DOS, volver a mirar los discos)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;  Lo usa tests/run_criterr_tests.sh (con DISCO81.COM).
; =============================================================================
        org 0x100
        mov ah, 0x0D
        int 0x21
        mov dx, m
        mov ah, 9
        int 0x21
        mov ax, 0x4C00
        int 0x21
m       db "funcion-0D-hecha", 13, 10, "$"
