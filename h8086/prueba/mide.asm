; =============================================================================
;  MIDE.COM - programa de prueba de Hispa-8086: cuánta memoria hay delante
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Escribe, en octetos, dónde empieza el prefijo de quien lo lanza (el
;  intérprete: todo lo de delante es del núcleo), dónde empieza el suyo (lo
;  de en medio es lo que el intérprete deja en memoria mientras corre un
;  programa, con su entorno) y el bloque libre mayor, tras quedarse él con
;  512 octetos:  «MIDE: 19856 24272 629536».
; =============================================================================
        cpu 8086
        org 0x100
        mov dx, m_mide
        mov ah, 9
        int 0x21
        mov ax, [0x16]              ; el prefijo del padre
        call num
        mov ax, cs
        call num
        mov bx, 0x20
        mov ah, 0x4A
        int 0x21
        mov bx, 0xFFFF
        mov ah, 0x48
        int 0x21
        mov ax, bx
        call num
        mov dx, m_nl
        mov ah, 9
        int 0x21
        mov ax, 0x4C00
        int 0x21
; AX párrafos, en octetos (32 bits), en decimal y con un espacio delante
num:
        push ax
        mov dl, ' '
        mov ah, 2
        int 0x21
        pop ax
        mov dx, ax
        mov cl, 4
        shl ax, cl
        mov cl, 12
        shr dx, cl
        mov bx, 10
        xor cx, cx
.d:
        push ax                     ; DX:AX entre 10, en dos pasos de 16 bits
        mov ax, dx
        xor dx, dx
        div bx
        mov si, ax
        pop ax
        div bx
        push dx
        inc cx
        mov dx, si
        or si, ax
        jnz .d
.p:
        pop dx
        add dl, '0'
        mov ah, 2
        int 0x21
        loop .p
        ret
m_mide      db "MIDE:$"
m_nl        db 13, 10, "$"
