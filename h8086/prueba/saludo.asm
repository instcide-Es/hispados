; =============================================================================
;  SALUDO.SYS - un controlador de dispositivo de carácter, para la prueba
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  DEVICE=\SALUDO.SYS deja un dispositivo «SALUDO»: al leerlo da una frase
;  (una vez sí y otra no, para que quien lee hasta el final acabe), y lo que
;  se escribe en él lo cuenta: la frase dice cuántos octetos lleva recibidos.
; =============================================================================
        cpu 8086
        org 0
        dd -1
        dw 0x8000
        dw estrategia
        dw interrupcion
        db "SALUDO  "
peticion    dd 0
dado        db 0
estrategia:
        mov [cs:peticion], bx
        mov [cs:peticion+2], es
        retf
interrupcion:
        push ax
        push bx
        push cx
        push si
        push di
        push ds
        push es
        push cs
        pop ds
        cld
        les bx, [peticion]
        mov word [es:bx+3], 0x0100
        mov al, [es:bx+2]
        test al, al
        jnz .n0
        mov word [es:bx+14], fin    ; iniciar: hasta dónde me quedo
        mov [es:bx+16], cs
        jmp short .x
.n0:
        cmp al, 8
        jne .n1
        mov ax, [es:bx+18]          ; escribir: se cuenta (hasta 99)
        add [recibidos], al
        jmp short .x
.n1:
        cmp al, 4
        jne .x
        xor byte [dado], 1
        jnz .da
        mov word [es:bx+18], 0      ; la segunda vez, nada
        jmp short .x
.da:
        mov al, [recibidos]
        aam
        add ax, '00'
        xchg al, ah
        mov [m_n], ax
        mov cx, m_fin - mensaje
        cmp cx, [es:bx+18]
        jbe .d1
        mov cx, [es:bx+18]
.d1:
        mov [es:bx+18], cx
        les di, [es:bx+14]
        mov si, mensaje
        rep movsb
.x:
        pop es
        pop ds
        pop di
        pop si
        pop cx
        pop bx
        pop ax
        retf
recibidos   db 0
mensaje     db "Saludos del controlador: "
m_n         db "00 octetos recibidos.", 13, 10
m_fin:
fin:
