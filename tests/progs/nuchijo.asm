; NUCHIJO.COM - hijo de NUCTEST: comprueba su entorno y devuelve 55h si
; está bien (AAh si no). Cola " E": entorno vacío; " N": nombre del programa.
; Child of NUCTEST: checks its environment, returns 55h if right.
        cpu 386
        bits 16
        org 0x100
        mov al, [0x82]
        mov es, [0x2C]
        cmp al, 'E'
        je vacio
        cmp al, 'N'
        je nombre
bien:
        mov ax, 0x4C55
        int 0x21
mal:
        mov ax, 0x4CAA
        int 0x21
; entorno vacío: 00 00, 01 00, ruta
vacio:
        cmp word [es:0], 0
        jne mal
        cmp word [es:2], 1
        jne mal
        mov di, 4
        jmp comparar
; buscar el doble cero como hacen los programas (palabra 0000h)
nombre:
        xor di, di
.l:
        cmp word [es:di], 0
        je .f
        inc di
        cmp di, 0x7FF0
        jb .l
        jmp mal
.f:
        add di, 2
        cmp word [es:di], 1
        jne mal
        add di, 2
comparar:
        mov si, ruta
.c:
        lodsb
        cmp al, [es:di]
        jne mal
        inc di
        test al, al
        jnz .c
        jmp bien
ruta    db "A:\BIN\NUCHIJO.COM", 0
