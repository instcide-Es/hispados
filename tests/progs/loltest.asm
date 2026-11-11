; LOLTEST.COM - muestra la CDS de A: (lista de listas) y su DPB (INT 21h 32h)
        cpu 386
        bits 16
        org 0x100
        mov ah, 0x52
        int 0x21
        les si, [es:bx+0x16]        ; CDS
        mov si, m_cds
        call puts
        ; ruta de la CDS de A:
        push ds
        mov ah, 0x52
        int 0x21
        lds si, [es:bx+0x16]
        push si
.p:
        lodsb
        test al, al
        jz .pe
        call putc
        jmp .p
.pe:
        pop si
        mov ax, [si+0x43]
        pop ds
        push ax
        mov si, m_flags
        call puts
        pop ax
        call hex4
        ; DPB de A:
        mov ah, 0x32
        mov dl, 1
        int 0x21
        push cs
        pop es
        cmp al, 0
        jne .nodpb
        push ds
        mov al, [bx]
        mov ah, [bx+4]
        mov cx, [bx+2]
        mov dx, [bx+0x0D]
        pop ds
        push cs
        pop ds
        push dx
        push cx
        push ax
        mov si, m_dpb
        call puts
        pop ax
        push ax
        add al, 'A'
        call putc
        mov si, m_spc
        call puts
        pop ax
        mov al, ah
        inc al
        movzx ax, al
        call hex4
        mov si, m_bps
        call puts
        pop ax
        call hex4
        mov si, m_max
        call puts
        pop ax
        call hex4
        mov si, m_crlf
        call puts
        mov ax, 0x4C00
        int 0x21
.nodpb:
        mov si, m_nodpb
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
m_cds   db "CDS A: ", 0
m_flags db " flags=", 0
m_dpb   db 13, 10, "DPB drive ", 0
m_spc   db ": spc=", 0
m_bps   db " bps=", 0
m_max   db " maxclus=", 0
m_nodpb db 13, 10, "no DPB", 13, 10, 0
m_crlf  db 13, 10, 0
