; =============================================================================
;  HOLA.COM - programa de prueba de Hispa-8086
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Usa lo que un programa de DOS espera encontrar: escribe con INT 21h/09h,
;  pregunta la versión (30h), enseña sus parámetros (del prefijo, en 80h),
;  pone y lee un vector (25h y 35h) y termina con 4Ch.
; =============================================================================
        cpu 8086
        org 0x100
        mov dx, m_hola
        mov ah, 9
        int 0x21
        mov ah, 0x30                ; la versión: AL.AH
        int 0x21
        push ax
        add al, '0'
        mov dl, al
        mov ah, 2
        int 0x21
        mov dl, '.'
        int 0x21
        pop ax
        mov al, ah
        xor ah, ah
        mov bl, 10
        div bl
        push ax
        add al, '0'
        mov dl, al
        mov ah, 2
        int 0x21
        pop ax
        mov dl, ah
        add dl, '0'
        mov ah, 2
        int 0x21
        ; los parámetros
        mov dx, m_param
        mov ah, 9
        int 0x21
        mov si, 0x81
        mov cl, [0x80]
        xor ch, ch
        jcxz .sin
.p:
        lodsb
        mov dl, al
        mov ah, 2
        int 0x21
        loop .p
.sin:
        ; un vector: se pone el 60h y se lee
        mov dx, 0x1234
        mov ax, 0x2560
        int 0x21
        mov ax, 0x3560
        int 0x21
        mov dx, m_vec_mal
        cmp bx, 0x1234
        jne .v
        mov ax, es
        mov cx, cs
        cmp ax, cx
        jne .v
        mov dx, m_vec_bien
.v:
        mov ah, 9
        int 0x21
        mov ax, 0x4C07
        int 0x21
m_hola      db "Hola desde un programa .COM. DOS ", "$"
m_param     db 13, 10, "Parametros:$"
m_vec_bien  db 13, 10, "Vectores: bien", 13, 10, "$"
m_vec_mal   db 13, 10, "Vectores: MAL", 13, 10, "$"
