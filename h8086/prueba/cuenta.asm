; =============================================================================
;  CUENTA.COM - dice cuántos sectores ha leído y escrito el núcleo de verdad
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Para medir en la prueba lo que ahorra la reserva de sectores (BUFFERS=):
;  INT 21h/E8h con AL=4 da las dos cuentas desde el arranque.
; =============================================================================
        cpu 8086
        org 0x100
        mov ax, 0xE804
        int 0x21
        push dx
        mov di, m_l + 4
        call num
        pop ax
        mov di, m_e + 4
        call num
        mov dx, m
        mov ah, 9
        int 0x21
        mov ax, 0x4C00
        int 0x21
num:
        mov bx, 10
        mov cx, 5
.c:
        xor dx, dx
        div bx
        add dl, '0'
        mov [di], dl
        dec di
        loop .c
        ret
m           db "CUENTA: leidos "
m_l         db "00000 escritos "
m_e         db "00000", 13, 10, "$"
