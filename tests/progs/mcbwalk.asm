; MCBWALK.COM - herramienta de diagnóstico: recorre la cadena de MCB
        cpu 386
        bits 16
        org 0x100
        mov ah, 0x52
        int 0x21
        mov ax, [es:bx-2]
.l:
        mov es, ax
        push ax
        call hex4
        mov al, ' '
        call putc
        mov al, [es:0]
        call putc
        mov al, ' '
        call putc
        mov ax, [es:1]
        call hex4
        mov al, ' '
        call putc
        mov ax, [es:3]
        call hex4
        mov al, ' '
        call putc
        mov cx, 8
        mov si, 8
.n:
        mov al, [es:si]
        cmp al, 32
        jae .p
        mov al, '.'
.p:
        call putc
        inc si
        loop .n
        mov al, 13
        call putc
        mov al, 10
        call putc
        pop ax
        cmp byte [es:0], 'M'
        jne .end
        add ax, [es:3]
        inc ax
        jmp .l
.end:
        mov ax, 0x4C00
        int 0x21
hex4:
        mov cx, 4
.h:
        rol ax, 4
        push ax
        and al, 15
        add al, '0'
        cmp al, '9'
        jbe .d
        add al, 7
.d:
        call putc
        pop ax
        loop .h
        ret
putc:
        push ax
        push dx
        mov dl, al
        mov ah, 2
        int 0x21
        pop dx
        pop ax
        ret
