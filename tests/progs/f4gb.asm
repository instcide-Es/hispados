; F4GB.COM - GRANDE.BIN mide FFFFFE00h (FAT32): escribir 1024 octetos desde
; FFFFFE00h escribe solo los 511 que caben hasta 4 GB - 1, y el principio
; del archivo sigue a cero (antes la posición daba la vuelta).
; Writes past 4 GB - 1 on FAT32.
        cpu 386
        bits 16
        org 0x100
        mov ax, 0x3D02
        mov dx, nombre
        int 0x21
        jc .mal
        mov bx, ax
        mov ax, 0x4200
        mov cx, 0xFFFF
        mov dx, 0xFE00
        int 0x21
        mov dx, buf
        mov cx, 1024
        mov ah, 0x40
        int 0x21
        mov dx, m_511
        jc .p1
        cmp ax, 511
        je .p1
        mov dx, m_otro
.p1:
        push bx
        mov ah, 9
        int 0x21
        pop bx
        mov ax, 0x4200
        xor cx, cx
        xor dx, dx
        int 0x21
        mov dx, rbuf
        mov cx, 16
        mov ah, 0x3F
        int 0x21
        mov dx, m_ini0
        cmp dword [rbuf], 0
        je .p2
        mov dx, m_inimal
.p2:
        push bx
        mov ah, 9
        int 0x21
        pop bx
        mov ah, 0x3E
        int 0x21
        mov dx, m_fin
        jmp .p
.mal:
        mov dx, m_noab
.p:
        mov ah, 9
        int 0x21
        mov ax, 0x4C00
        int 0x21
nombre  db "C:\GRANDE.BIN", 0
m_511   db "F4GB: escritos 511 (hasta 4 GB - 1)", 13, 10, "$"
m_otro  db "F4GB: escritos otros", 13, 10, "$"
m_ini0  db "F4GB: el principio sigue a cero", 13, 10, "$"
m_inimal db "F4GB: el principio ESCRITO", 13, 10, "$"
m_noab  db "F4GB: no se abre", 13, 10, "$"
m_fin   db "F4GB: fin", 13, 10, "$"
rbuf    times 16 db 0
buf     times 1024 db 'Z'
