; DOSHAND.COM - dos handles escribiendo el mismo archivo: comparten primer
; grupo y tamaño (antes, al cerrar ganaba el último, y en un archivo vacío
; cada uno se hacía su propia cadena: grupos perdidos).
;  1) D1.BIN: h1 crea y escribe 100 'a'; h2 lo abre y escribe 10 'b' al
;     principio; h1 escribe 100 'a' más; se cierran h1 y luego h2 -> 200
;     octetos, «bbbbbbbbbb» delante
;  2) D2.BIN vacío abierto dos veces; h1 escribe 3000, h2 escribe 3000 desde
;     el principio -> 3000 octetos, sin grupos perdidos
; Two handles writing the same file.
        cpu 386
        bits 16
        org 0x100
        mov dx, n1
        xor cx, cx
        mov ah, 0x3C
        int 0x21
        jc fail
        mov [h1], ax
        mov bx, ax
        mov dx, bufa
        mov cx, 100
        mov ah, 0x40
        int 0x21
        mov dx, n1
        mov ax, 0x3D02
        int 0x21
        jc fail
        mov [h2], ax
        mov bx, ax
        mov dx, bufb
        mov cx, 10
        mov ah, 0x40
        int 0x21
        mov bx, [h1]
        mov dx, bufa
        mov cx, 100
        mov ah, 0x40
        int 0x21
        mov bx, [h1]
        mov ah, 0x3E
        int 0x21
        mov bx, [h2]
        mov ah, 0x3E
        int 0x21
        ; 2)
        mov dx, n2
        xor cx, cx
        mov ah, 0x3C
        int 0x21
        jc fail
        mov [h1], ax
        mov dx, n2
        mov ax, 0x3D02
        int 0x21
        jc fail
        mov [h2], ax
        mov bx, [h1]
        mov dx, bufa
        mov cx, 3000
        mov ah, 0x40
        int 0x21
        mov bx, [h2]
        mov dx, bufb
        mov cx, 3000
        mov ah, 0x40
        int 0x21
        mov bx, [h2]
        mov ah, 0x3E
        int 0x21
        mov bx, [h1]
        mov ah, 0x3E
        int 0x21
        mov dx, m_fin
        jmp p
fail:
        mov dx, m_fail
p:
        mov ah, 9
        int 0x21
        mov ax, 0x4C00
        int 0x21
h1      dw 0
h2      dw 0
n1      db "D1.BIN", 0
n2      db "D2.BIN", 0
m_fin   db "DOSHAND: fin", 13, 10, "$"
m_fail  db "DOSHAND: fallo", 13, 10, "$"
bufa    times 3000 db 'a'
bufb    times 3000 db 'b'
