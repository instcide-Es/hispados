; ABS25.COM X - INT 25h (paquete, CX=FFFFh) del sector 0 de la unidad X:
; -> «ABS25 X: leido» o «ABS25 X: rechazado hhhh».
; Absolute disk read probe.
        cpu 386
        bits 16
        org 0x100
        mov al, [0x82]
        and al, 0xDF
        mov [m_ok+6], al
        mov [m_no+6], al
        sub al, 'A'
        mov [pk_seg], ds
        mov cx, 0xFFFF
        mov bx, pk
        int 0x25
        pop dx
        jc .no
        mov dx, m_ok
        jmp .p
.no:
        mov cx, 4
        mov di, m_no+19
.h:
        rol ax, 4
        push ax
        and al, 15
        add al, '0'
        cmp al, '9'
        jbe .d
        add al, 7
.d:
        mov [di], al
        inc di
        pop ax
        loop .h
        mov dx, m_no
.p:
        mov ah, 9
        int 0x21
        mov ax, 0x4C00
        int 0x21
pk      dd 0
        dw 1
        dw buf
pk_seg  dw 0
m_ok    db "ABS25 X: leido", 13, 10, "$"
m_no    db "ABS25 X: rechazado hhhh", 13, 10, "$"
buf     times 512 db 0
