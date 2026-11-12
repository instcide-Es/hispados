; NUCCON.SYS - controlador CON mínimo para las pruebas (como ANSI.SYS: sustituye
; al CON del núcleo). Lee del teclado de la BIOS y del puerto serie COM1 y
; escribe en la pantalla y en COM1, para que las pruebas lo manejen por serie.
; Minimal replacement CON driver for the tests (BIOS keyboard + COM1).
        cpu 386
        bits 16
        org 0
cab:
        dd 0xFFFFFFFF
        dw 0x8003                   ; carácter, entrada y salida estándar
        dw estrategia
        dw interrupcion
        db "CON     "
peticion dd 0
hay     db 0
tecla   db 0

estrategia:
        mov [cs:peticion], bx
        mov [cs:peticion+2], es
        retf

interrupcion:
        pushad
        push ds
        push es
        push cs
        pop ds
        les bx, [peticion]
        mov word [es:bx+3], 0x0100
        mov al, [es:bx+2]
        cmp al, 0
        je .init
        cmp al, 4
        je .leer
        cmp al, 5
        je .mirar
        cmp al, 6
        je .estado
        cmp al, 7
        je .vaciar
        cmp al, 8
        je .escribir
        cmp al, 9
        je .escribir
.fin:
        pop es
        pop ds
        popad
        retf
.init:
        mov word [es:bx+0x0E], final
        mov [es:bx+0x10], cs
        jmp .fin
.leer:
        mov cx, [es:bx+0x12]
        push es
        push bx
        les di, [es:bx+0x0E]
.l1:
        jcxz .l2
.esp:
        call sondear
        jz .esp
        mov byte [hay], 0
        mov al, [tecla]
        stosb
        dec cx
        jmp .l1
.l2:
        pop bx
        pop es
        jmp .fin
.mirar:
        call sondear
        jz .ocupado
        mov al, [tecla]
        mov [es:bx+0x0D], al
        jmp .fin
.estado:
        call sondear
        jnz .fin
.ocupado:
        mov word [es:bx+3], 0x0300
        jmp .fin
.vaciar:
        call sondear
        jz .fin
        mov byte [hay], 0
        jmp .vaciar
.escribir:
        mov cx, [es:bx+0x12]
        push ds
        lds si, [es:bx+0x0E]
.e1:
        jcxz .e2
        lodsb
        call sacar
        dec cx
        jmp .e1
.e2:
        pop ds
        jmp .fin

; ZF=0 si hay una tecla en [tecla]
sondear:
        cmp byte [hay], 0
        jne .si
        mov dx, 0x3FD
        in al, dx
        test al, 1
        jz .bios
        mov dx, 0x3F8
        in al, dx
        jmp .pon
.bios:
        mov ah, 1
        int 0x16
        jz .no
        xor ah, ah
        int 0x16
.pon:
        mov [tecla], al
        mov byte [hay], 1
.si:
        or al, 1
        ret
.no:
        xor al, al
        ret

; AL -> pantalla y COM1
sacar:
        push ax
        push bx
        push dx
        push bp
        mov ah, al
        mov dx, 0x3FD
.w:
        in al, dx
        test al, 0x20
        jz .w
        mov al, ah
        mov dx, 0x3F8
        out dx, al
        mov ah, 0x0E
        mov bx, 7
        int 0x10
        pop bp
        pop dx
        pop bx
        pop ax
        ret
final:
