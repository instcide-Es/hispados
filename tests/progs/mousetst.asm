; MOUSETST.COM - prueba del driver de ratón (INT 33h)
; Escribe la posición y los botones cada vez que cambian; termina con una tecla
; y escribe cuántas veces se llamó a la rutina del usuario (botón izquierdo).
        cpu 386
        bits 16
        org 0x100

        xor ax, ax
        int 0x33
        cmp ax, 0xFFFF
        je .ok
        mov dx, m_none
        mov ah, 9
        int 0x21
        mov ax, 0x4C01
        int 0x21
.ok:
        push bx
        ; INT 33h no debe cambiar SI, DI ni (en estas funciones) CX
        mov si, 0x1234
        mov di, 0x5678
        mov cx, 0x9ABC
        xor ax, ax
        int 0x33
        cmp si, 0x1234
        jne .rbad
        cmp di, 0x5678
        jne .rbad
        cmp cx, 0x9ABC
        jne .rbad
        mov ax, 5
        xor bx, bx
        int 0x33
        cmp si, 0x1234
        jne .rbad
        cmp di, 0x5678
        jne .rbad
        push cs
        pop es
        mov dx, stbuf
        mov cx, 0x9ABC
        mov ax, 0x16
        int 0x33
        cmp si, 0x1234
        jne .rbad
        cmp di, 0x5678
        jne .rbad
        cmp cx, 0x9ABC
        jne .rbad
        mov dx, m_regs
        jmp .rpr
.rbad:
        mov dx, m_regsf
.rpr:
        mov ah, 9
        int 0x21
        mov dx, m_reset
        mov ah, 9
        int 0x21
        pop ax
        call pnum
        call crlf
        ; versión
        mov ax, 0x24
        int 0x33
        mov dx, m_ver
        mov ah, 9
        int 0x21
        push bx
        movzx ax, bh
        call pnum
        pop bx
        mov dl, '.'
        mov ah, 2
        int 0x21
        push bx
        movzx ax, bl
        shr al, 4
        call pnum
        pop bx
        movzx ax, bl
        and al, 15
        call pnum
        call crlf
        ; rutina del usuario: botón izquierdo pulsado
        push cs
        pop es
        mov dx, handler
        mov cx, 2
        mov ax, 0x0C
        int 0x33
        mov ax, 1
        int 0x33
.l:
        mov ah, 1
        int 0x16
        jnz .key
        mov ax, 3
        int 0x33
        cmp cx, [lx]
        jne .ch
        cmp dx, [ly]
        jne .ch
        cmp bx, [lb]
        je .l
.ch:
        mov [lx], cx
        mov [ly], dx
        mov [lb], bx
        push dx
        push bx
        push cx
        mov dx, m_x
        mov ah, 9
        int 0x21
        pop ax
        call pnum
        mov dx, m_y
        mov ah, 9
        int 0x21
        pop bx
        pop ax
        push bx
        call pnum
        mov dx, m_b
        mov ah, 9
        int 0x21
        pop ax
        call pnum
        call crlf
        jmp .l
.key:
        xor ah, ah
        int 0x16
        mov ax, 2
        int 0x33
        ; contadores de pulsaciones del botón izquierdo (función 05h)
        mov ax, 5
        xor bx, bx
        int 0x33
        push bx
        mov dx, m_cnt
        mov ah, 9
        int 0x21
        pop ax
        call pnum
        call crlf
        mov dx, m_calls
        mov ah, 9
        int 0x21
        mov ax, [calls]
        call pnum
        call crlf
        ; quitar la rutina
        xor cx, cx
        mov ax, 0x0C
        int 0x33
        mov ax, 0x4C00
        int 0x21

handler:
        inc word [cs:calls]
        retf

pnum:
        xor cx, cx
        mov bx, 10
.d:
        xor dx, dx
        div bx
        push dx
        inc cx
        test ax, ax
        jnz .d
.p:
        pop dx
        add dl, '0'
        mov ah, 2
        int 0x21
        loop .p
        ret
crlf:
        mov dx, m_crlf
        mov ah, 9
        int 0x21
        ret

lx      dw 0xFFFF
ly      dw 0xFFFF
lb      dw 0xFFFF
calls   dw 0
m_none  db "No hay driver de ratón.", 13, 10, "$"
m_reset db "Ratón: botones $"
m_ver   db "Versión del driver: $"
m_x     db "X=$"
m_regs  db "Registros conservados", 13, 10, "$"
m_regsf db "INT 33h cambia registros (SI, DI o CX)", 13, 10, "$"
stbuf   times 512 db 0
m_y     db " Y=$"
m_b     db " B=$"
m_cnt   db "Pulsaciones del botón izquierdo: $"
m_calls db "Llamadas a la rutina: $"
m_crlf  db 13, 10, "$"
