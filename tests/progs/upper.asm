; UPPER.COM - filtro de prueba: copia la entrada estándar en mayúsculas
        bits 16
        org 0x100
.l:
        mov ah, 0x3F
        xor bx, bx
        mov cx, 1
        mov dx, ch_buf
        int 0x21
        jc .e
        test ax, ax
        jz .e
        mov al, [ch_buf]
        cmp al, 0x1A
        je .e
        cmp al, 'a'
        jb .w
        cmp al, 'z'
        ja .w
        sub al, 32
        mov [ch_buf], al
.w:
        mov ah, 0x40
        mov bx, 1
        mov cx, 1
        mov dx, ch_buf
        int 0x21
        jmp .l
.e:
        mov ax, 0x4C00
        int 0x21
ch_buf  db 0
