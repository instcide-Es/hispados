; LD01.COM - prueba de INT 21h AX=4B01h (cargar sin ejecutar)
        cpu 386
        bits 16
        org 0x100
        mov sp, 0x400
        mov bx, 0x40            ; dejar memoria libre
        mov ah, 0x4A
        int 0x21
        mov ah, 0x62
        int 0x21
        mov [mypsp], bx
        mov [pb+4], cs
        mov dx, fname
        mov bx, pb
        mov ax, 0x4B01
        int 0x21
        push cs
        pop ds
        jc .err
        mov si, m_ok
        call puts
        mov ax, [pb+0x14]
        call hex4
        mov al, ':'
        call putc
        mov ax, [pb+0x12]
        call hex4
        mov si, m_ss
        call puts
        mov ax, [pb+0x10]
        call hex4
        mov al, ':'
        call putc
        mov ax, [pb+0x0E]
        call hex4
        mov si, m_psp
        call puts
        mov ah, 0x62
        int 0x21
        mov ax, bx
        call hex4
        mov si, m_parent
        call puts
        mov es, bx
        mov ax, [es:0x16]
        call hex4
        mov si, m_crlf
        call puts
        ; terminar el hijo cargado y volver aquí
        mov ah, 0x50
        mov bx, [mypsp]
        int 0x21
        mov ax, 0x4C00
        int 0x21
.err:
        push ax
        mov si, m_err
        call puts
        pop ax
        call hex4
        mov si, m_crlf
        call puts
        mov ax, 0x4C01
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
puts:
        lodsb
        test al, al
        jz .r
        call putc
        jmp puts
.r:
        ret
fname   db "A:\FD\TREE.COM", 0
pb      dw 0
        dw tail, 0
        dw 0xFFFF, 0xFFFF
        dw 0xFFFF, 0xFFFF
        dd 0, 0
tail    db 0, 13
mypsp   dw 0
m_ok    db "4B01 OK CS:IP=", 0
m_ss    db " SS:SP=", 0
m_psp   db " PSP=", 0
m_parent db " padre=", 0
m_err   db "4B01 error ", 0
m_crlf  db 13, 10, 0
