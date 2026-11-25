; RAT17.COM - INT 33h 17h con un estado cuya proporción mickeys/puntos es 0
; (no viene de 16h o está estropeado): HSPRATON no puede dividir por cero en
; la siguiente IRQ 12. Guarda (16h), pone la proporción a 0, restaura (17h)
; y espera a que el ratón se mueva (unos 8 s) mirando 03h.
; INT 33h function 17h with a zero mickey ratio.
        cpu 386
        bits 16
        org 0x100
        xor ax, ax
        int 0x33
        cmp ax, 0xFFFF
        jne .no
        mov ax, 0x15
        int 0x33
        cmp bx, 256
        ja .no
        push ds
        pop es
        mov dx, buf
        mov ax, 0x16
        int 0x33
        mov word [buf+24], 0        ; ratx
        mov word [buf+26], 0        ; raty
        mov dx, buf
        mov ax, 0x17
        int 0x33
        mov dx, m_listo
        mov ah, 9
        int 0x21
        mov ax, 3
        int 0x33
        mov [x0], cx
        mov [y0], dx
        push ds
        xor ax, ax
        mov ds, ax
        mov bx, [0x46C]
        pop ds
        add bx, 18*12               ; unos 12 s
.l:
        mov ax, 3
        int 0x33
        cmp cx, [x0]
        jne .movido
        cmp dx, [y0]
        jne .movido
        push ds
        xor ax, ax
        mov ds, ax
        mov ax, [0x46C]
        pop ds
        cmp ax, bx
        jne .l
        mov dx, m_quieto
        jmp .p
.movido:
        mov dx, m_movido
        jmp .p
.no:
        mov dx, m_no
.p:
        mov ah, 9
        int 0x21
        mov ax, 0x4C00
        int 0x21
x0      dw 0
y0      dw 0
m_listo  db "RAT17: estado restaurado", 13, 10, "$"
m_movido db "RAT17: el raton se mueve", 13, 10, "$"
m_quieto db "RAT17: el raton NO se ha movido", 13, 10, "$"
m_no     db "RAT17: sin raton", 13, 10, "$"
buf     times 256 db 0
