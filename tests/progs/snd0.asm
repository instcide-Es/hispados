; SND0.COM - HSPSND 01h (reproducir) con DX = 0 Hz: tiene que volver con CF
; (antes el remuestreo no avanzaba y se colgaba); con 8000 Hz, bien.
; HSPSND: zero sample rate.
        cpu 386
        bits 16
        org 0x100
        mov ax, 0xD700
        int 0x2F
        cmp al, 0xFF
        jne .no
        mov [ent], bx
        mov [ent+2], es
        mov ah, 1
        xor al, al
        mov si, buf
        mov cx, 400
        xor dx, dx
        call far [ent]
        mov dx, m_0no
        jnc .p1
        mov dx, m_0ok
.p1:
        push dx
        mov ah, 9
        pop dx
        int 0x21
        mov ah, 1
        xor al, al
        mov si, buf
        mov cx, 400
        mov dx, 8000
        call far [ent]
        mov dx, m_8no
        jc .p2
        mov dx, m_8ok
.p2:
        mov ah, 9
        int 0x21
        mov ah, 2
        call far [ent]
        mov dx, m_fin
        jmp .p
.no:
        mov dx, m_nosnd
.p:
        mov ah, 9
        int 0x21
        mov ax, 0x4C00
        int 0x21
ent     dd 0
m_0ok   db "SND0: 0 Hz rechazado", 13, 10, "$"
m_0no   db "SND0: 0 Hz aceptado", 13, 10, "$"
m_8ok   db "SND0: 8000 Hz bien", 13, 10, "$"
m_8no   db "SND0: 8000 Hz MAL", 13, 10, "$"
m_fin   db "SND0: fin", 13, 10, "$"
m_nosnd db "SND0: sin HSPSND", 13, 10, "$"
buf     times 400 db 0x80
