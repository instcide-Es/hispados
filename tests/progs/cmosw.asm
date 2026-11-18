; =============================================================================
;  CMOSW.COM - (pruebas) escribe un octeto en la memoria CMOS: CMOSW ii vv
;  (en hexadecimal). Lo usa tests/run_acpi_tests.sh para cambiar la batería
;  de mentira de QEMU (tests/acpi/bateria-qemu.asl).
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
; =============================================================================
        bits 16
        org 0x100
        mov si, 0x81
        call hex
        mov bl, al
        call hex
        mov bh, al
        mov al, bl
        out 0x70, al
        jmp short $+2
        mov al, bh
        out 0x71, al
        mov ax, 0x4C00
        int 0x21
; el siguiente número hexadecimal de la línea -> AL
hex:
        xor dx, dx
.e:
        lodsb
        cmp al, ' '
        je .e
.d:
        cmp al, '0'
        jb .f
        cmp al, '9'
        jbe .n
        and al, 0xDF
        cmp al, 'A'
        jb .f
        cmp al, 'F'
        ja .f
        sub al, 7
.n:
        sub al, '0'
        shl dl, 4
        or dl, al
        lodsb
        jmp .d
.f:
        dec si
        mov al, dl
        ret
