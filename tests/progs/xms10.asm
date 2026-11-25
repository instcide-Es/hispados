; XMS10.COM - XMS 10h (pedir memoria superior) con DX = 0: no se da un
; bloque vacío (AX = 0, BL = B0h, DX = el mayor); con 1 párrafo, sí.
; XMS UMB request of 0 paragraphs.
        cpu 386
        bits 16
        org 0x100
        mov ax, 0x4300
        int 0x2F
        cmp al, 0x80
        jne .no
        mov ax, 0x4310
        int 0x2F
        mov [xms], bx
        mov [xms+2], es
        mov ah, 0x10
        xor dx, dx
        call far [xms]
        mov dx, m_0mal
        test ax, ax
        jnz .p1
        cmp bl, 0xB0
        jne .p1
        mov dx, m_0ok
.p1:
        mov ah, 9
        int 0x21
        mov ah, 0x10
        mov dx, 1
        call far [xms]
        mov dx, m_1mal
        test ax, ax
        jz .p2
        mov dx, bx
        mov ah, 0x11
        call far [xms]
        mov dx, m_1ok
.p2:
        jmp .p
.no:
        mov dx, m_no
.p:
        mov ah, 9
        int 0x21
        mov ax, 0x4C00
        int 0x21
xms     dd 0
m_0ok   db "XMS10: 0 parrafos -> B0h", 13, 10, "$"
m_0mal  db "XMS10: 0 parrafos ACEPTADO", 13, 10, "$"
m_1ok   db "XMS10: 1 parrafo bien", 13, 10, "$"
m_1mal  db "XMS10: 1 parrafo MAL", 13, 10, "$"
m_no    db "XMS10: sin XMS", 13, 10, "$"
