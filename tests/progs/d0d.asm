; =============================================================================
;  D0D.COM - prueba: INT 21h AH=0Dh (vaciar los búferes) y AX=336Ah (propia
;  de Hispa-DOS: volver a mirar los discos, lo que usan FDISK y FORMAT)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;  Lo usan tests/run_criterr_tests.sh (con DISCO81.COM), run_wifi_tests.sh y
;  run_disco0d_tests.sh. Con -DSOLO0D, solo 0Dh (CERO.COM).
; =============================================================================
        org 0x100
        mov ah, 0x0D
        int 0x21
%ifndef SOLO0D
        xor bx, bx
        mov ax, 0x336A
        int 0x21
        cmp bx, 'HD'
        je .ok
        mov dx, m_no
        mov ah, 9
        int 0x21
.ok:
%endif
        mov dx, m
        mov ah, 9
        int 0x21
        mov ax, 0x4C00
        int 0x21
%ifdef SOLO0D
m       db "solo-0D-hecha", 13, 10, "$"
%else
m       db "funcion-0D-hecha", 13, 10, "$"
m_no    db "sin-336A", 13, 10, "$"
%endif
