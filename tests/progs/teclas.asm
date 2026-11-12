; TECLAS.COM - prueba del teclado: por cada tecla escribe lo que da la BIOS
;   X=hhhh  con INT 16h AH=11h (teclado ampliado, sin retirar la tecla)
;   N=hhhh  con INT 16h AH=00h (como la lee un programa de DOS de siempre)
;   I=0/1   el indicador de Insertar de la BIOS (0040:0017, bit 7)
; Acaba con Esc.
        bits 16
        org 0x100
.l:
        mov ah, 0x11
        int 0x16
        jnz .k
        hlt
        jmp .l
.k:
        ; primero se leen las dos y después se escribe: al escribir, DOS mira
        ; el teclado (Ctrl+C) y se llevaría la tecla que aún no se ha leído
        mov [kx], ax
        xor ah, ah
        int 0x16
        mov [kn], ax
        push ds
        mov ax, 0x40
        mov ds, ax
        mov al, [0x17]
        pop ds
        shr al, 7
        add al, '0'
        mov [ki], al
        mov dl, 'X'
        call tag
        mov ax, [kx]
        call hex4
        mov dl, 'N'
        call tag
        mov ax, [kn]
        call hex4
        mov dl, 'I'
        call tag
        mov dl, [ki]
        mov ah, 2
        int 0x21
        mov dl, 13
        int 0x21
        mov dl, 10
        int 0x21
        cmp word [kn], 0x011B
        jne .l
        mov ax, 0x4C00
        int 0x21

; " L="
tag:
        push dx
        mov dl, ' '
        mov ah, 2
        int 0x21
        pop dx
        int 0x21
        mov dl, '='
        int 0x21
        ret

hex4:
        mov cx, 4
.h:
        rol ax, 4
        push ax
        and al, 0x0F
        add al, '0'
        cmp al, '9'
        jbe .p
        add al, 7
.p:
        mov dl, al
        mov ah, 2
        int 0x21
        pop ax
        loop .h
        ret

kx      dw 0
kn      dw 0
ki      db 0
