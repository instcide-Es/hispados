; =============================================================================
;  NESJOY.COM - programa de prueba: un mando «pulsado» para HSPJOY
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  QEMU no tiene mandos que se puedan mover desde las pruebas. Este programa
;  le entrega a HSPJOY (ya cargado, con /R) el estado de un mando USB, como
;  hace HSPUSB (INT 2Fh AX=D903h, docs/MANDOS.md): cuatro botones, la cruceta
;  a la derecha y el botón 4 pulsado. Para tests/run_hispanes_tests.sh: con
;  eso Hispa-Nes tiene que ver «derecha» y «Start» sin tocar el teclado.
;  «NESJOY n» (n de 1 a 4) pulsa el botón n en vez del 4: así lo usa
;  tests/run_hispasms_tests.sh (Hispa-System) con el botón 1.
; =============================================================================
        cpu 386
        bits 16
        org 0x100
        mov si, 0x81                ; ¿un número de botón en la línea de órdenes?
.p:
        lodsb
        cmp al, ' '
        je .p
        sub al, '1'
        cmp al, 3
        ja .np
        mov cl, al
        mov ax, 1
        shl ax, cl
        mov [botones], ax
.np:
        mov ax, 0xD903
        xor dl, dl
        push cs
        pop es
        mov di, estado
        int 0x2F
        mov dx, m_bien
        cmp al, 0xFF
        je .x
        mov dx, m_mal
.x:
        mov ah, 0x09
        int 0x21
        mov ax, 0x4C00
        int 0x21
estado      db 1, 1, 2, 4           ; USB HID, presente, 2 ejes, 4 botones
            dw 0x1234, 0x5678       ; fabricante y producto
            db 128, 128, 128, 128, 0, 0     ; ejes, en el centro
            db 0x08, 0              ; la cruceta, a la derecha
botones     dw 0x0008               ; botón 4
m_bien      db "NESJOY: mando de prueba entregado a HSPJOY.", 13, 10, "$"
m_mal       db "NESJOY: HSPJOY no esta cargado.", 13, 10, "$"
