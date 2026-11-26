; =============================================================================
;  SALUDOX.SYS - un controlador de carácter en formato .EXE, para la prueba
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Como los de MS-DOS 5 en adelante (EMM386.EXE, SETVER.EXE...): el archivo
;  es un .EXE (cabecera «MZ» con su tabla de reubicaciones) y la imagen,
;  detrás, empieza por la cabecera del dispositivo. Lleva una reubicación: la
;  palabra `mi_seg`, que vale 0 en el archivo y, cargada, tiene que valer su
;  segmento. Al leerlo («MOSTRAR EXEDRV») dice si es así.
; =============================================================================
        cpu 8086
        section cab start=0
        db "MZ"
        dw (32 + img_tam) % 512     ; los octetos de la última página
        dw (32 + img_tam + 511) / 512
        dw 1                        ; una reubicación
        dw 2                        ; la cabecera: dos párrafos
        dw 0, 0xFFFF
        dw 0, 0, 0, 0, 0
        dw 0x1C                     ; la tabla de reubicaciones
        dw 0
        dw mi_seg, 0                ; (la única)
        times 32 - ($ - $$) db 0

        section img follows=cab vstart=0
inicio_img:
        dd -1
        dw 0x8000
        dw estrategia
        dw interrupcion
        db "EXEDRV  "
mi_seg      dw 0                    ; (lo pone la reubicación)
peticion    dd 0
dado        db 0
estrategia:
        mov [cs:peticion], bx
        mov [cs:peticion+2], es
        retf
interrupcion:
        push ax
        push cx
        push si
        push di
        push ds
        push es
        push bx
        push cs
        pop ds
        cld
        les bx, [peticion]
        mov word [es:bx+3], 0x0100
        mov al, [es:bx+2]
        test al, al
        jnz .n0
        mov word [es:bx+14], fin_img
        mov [es:bx+16], cs
        mov ax, cs                  ; ¿la reubicación, bien hecha?
        cmp ax, [mi_seg]
        je .x
        mov word [m_que], "MA"
        mov word [m_que+2], "L "
        jmp short .x
.n0:
        cmp al, 4
        jne .x
        xor byte [dado], 1
        jnz .da
        mov word [es:bx+18], 0
        jmp short .x
.da:
        mov cx, m_fin - mensaje
        cmp cx, [es:bx+18]
        jbe .c
        mov cx, [es:bx+18]
.c:
        mov [es:bx+18], cx
        les di, [es:bx+14]
        mov si, mensaje
        rep movsb
.x:
        pop bx
        pop es
        pop ds
        pop di
        pop si
        pop cx
        pop ax
        retf
mensaje     db "EXEDRV: controlador .EXE, reubicado "
m_que       db "bien", 13, 10
m_fin:
fin_img:
img_tam     equ fin_img - inicio_img
