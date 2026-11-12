; A20OFF.COM - deja la línea A20 cerrada a través del XMS (abrir y cerrar
; local, como hace cualquier programa que usa la HMA) y dice cómo queda.
        cpu 386
        bits 16
        org 0x100
        mov ax, 0x4300
        int 0x2F
        cmp al, 0x80
        jne .nox
        mov ax, 0x4310
        int 0x2F
        mov [xms], bx
        mov [xms+2], es
        mov ah, 0x05                ; abrir (local)
        call far [xms]
        mov ah, 0x06                ; cerrar (local)
        call far [xms]
        mov ah, 0x04                ; cerrar (global)
        call far [xms]
        mov ah, 0x07                ; ¿cómo está?
        call far [xms]
        mov dx, m_on
        test ax, ax
        jnz .pr
        mov dx, m_off
        jmp .pr
.nox:
        mov dx, m_nox
.pr:
        mov ah, 9
        int 0x21
        mov ax, 0x4C00
        int 0x21
xms     dd 0
m_on    db "A20 sigue abierta", 13, 10, "$"
m_off   db "A20 cerrada", 13, 10, "$"
m_nox   db "No hay XMS", 13, 10, "$"
