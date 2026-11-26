; =============================================================================
;  IOCTL.COM - prueba de INT 21h/44h y 52h con los controladores (batería)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  IOCTL X [n]   X = la letra de la unidad de DISQUITO.SYS; con n (1 o 2),
;                cambia el disco que tiene puesto. Escribe, en líneas que
;                empiezan por «IOCTL:», lo que contestan:
;    - la lista de listas (52h): la primera cabecera de memoria y los
;      controladores de la cadena (los de carácter por su nombre, los de
;      bloque como «[n]», con sus unidades), empezando por NUL;
;    - 4404h (lectura de control de la unidad), 4408h, 4409h y 440Dh;
;    - 4400h y 4406h de la consola y del dispositivo SALUDO (que no entiende
;      IOCTL: 4402h tiene que dar el error 1).
; =============================================================================
        cpu 8086
        org 0x100

inicio:
        mov si, 0x81
.esp:
        lodsb
        cmp al, ' '
        je .esp
        and al, 0xDF
        sub al, 'A' - 1
        mov [unidad], al            ; (1 = A:)
.esp2:
        lodsb
        cmp al, ' '
        je .esp2
        mov [nuevo], al
        ; ---- 52h ----
        mov ah, 0x52
        int 0x21
        mov dx, m_mcb
        call cad
        mov ax, [es:bx-2]
        push es
        mov es, ax
        mov al, [es:0]
        pop es
        call car
        mov dx, m_cadena
        call cad
        lea si, [bx+0x22]
.c:
        test byte [es:si+5], 0x80
        jz .bloque
        mov cx, 8
        push si
        add si, 10
.n:
        mov al, [es:si]
        inc si
        cmp al, ' '
        je .n1
        call car
.n1:
        loop .n
        pop si
        jmp short .sig
.bloque:
        mov al, '['
        call car
        mov al, [es:si+10]
        add al, '0'
        call car
        mov al, ']'
        call car
.sig:
        mov al, ' '
        call car
        les si, [es:si]
        cmp si, 0xFFFF
        jne .c
        push cs
        pop es
        call nl
        ; ---- el disco que se pone ----
        cmp byte [nuevo], '1'
        jb .sin
        mov ax, 0x4405
        mov bl, [unidad]
        mov cx, 1
        mov dx, nuevo
        int 0x21
        mov dx, m_puesto
        jnc .p
        mov dx, m_mal
.p:
        call cad
.sin:
        ; ---- 4404h ----
        mov ax, 0x4404
        mov bl, [unidad]
        mov cx, 60
        mov dx, buf
        int 0x21
        jc .e4
        mov si, buf
        mov cx, ax
        mov dx, m_4404
        call cad
.t:
        lodsb
        call car
        loop .t
        call nl
        jmp short .n8
.e4:
        mov dx, m_mal
        call cad
.n8:
        ; ---- 4408h, 4409h ----
        mov dx, m_4408
        call cad
        mov ax, 0x4408
        mov bl, [unidad]
        int 0x21
        call hex4
        mov dx, m_4409
        call cad
        mov ax, 0x4409
        mov bl, [unidad]
        int 0x21
        mov ax, dx
        call hex4
        ; ---- 440Dh ----
        mov dx, m_440d
        call cad
        mov byte [buf], '?'
        mov ax, 0x440D
        mov bl, [unidad]
        mov cx, 0x0860
        mov dx, buf
        int 0x21
        mov al, [buf]
        call car
        ; ---- la consola ----
        mov dx, m_con
        call cad
        mov ax, 0x4400
        mov bx, 1
        int 0x21
        mov ax, dx
        call hex4
        call nl
        ; ---- SALUDO ----
        mov ax, 0x3D02
        mov dx, t_saludo
        int 0x21
        jc .fin
        mov [asa], ax
        mov dx, m_sal
        call cad
        mov ax, 0x4400
        mov bx, [asa]
        int 0x21
        mov ax, dx
        call hex4
        mov dx, m_4406
        call cad
        mov ax, 0x4406
        mov bx, [asa]
        int 0x21
        call hex4
        mov dx, m_4402
        call cad
        mov ax, 0x4402
        mov bx, [asa]
        mov cx, 4
        mov dx, buf
        int 0x21
        call hex4                   ; (el error: 0001)
        mov ah, 0x3E
        mov bx, [asa]
        int 0x21
        call nl
.fin:
        mov ax, 0x4C00
        int 0x21

cad:
        mov ah, 9
        int 0x21
        ret
nl:
        mov dx, m_nl
        jmp cad
car:
        push dx
        push ax
        mov dl, al
        mov ah, 2
        int 0x21
        pop ax
        pop dx
        ret
hex4:
        push ax
        mov al, ah
        call hex2
        pop ax
hex2:
        push ax
        mov cl, 4
        shr al, cl
        call .c
        pop ax
.c:
        and al, 15
        add al, '0'
        cmp al, '9'
        jbe .p
        add al, 7
.p:
        jmp car

m_mcb       db "IOCTL: memoria $"
m_cadena    db "; cadena: $"
m_puesto    db "IOCTL: disco cambiado", 13, 10, "$"
m_mal       db "IOCTL: MAL", 13, 10, "$"
m_4404      db "IOCTL: 4404 $"
m_4408      db "IOCTL: 4408 $"
m_4409      db "; 4409 $"
m_440d      db "; 440D $"
m_con       db "; consola $"
m_sal       db "IOCTL: SALUDO $"
m_4406      db "; 4406 $"
m_4402      db "; 4402 $"
m_nl        db 13, 10, "$"
t_saludo    db "SALUDO", 0
unidad      db 0
nuevo       db 0
asa         dw 0
buf         times 64 db 0
