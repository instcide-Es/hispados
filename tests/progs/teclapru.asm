; =============================================================================
;  TECLAPRU.COM - prueba del teclado para la imagen de prueba de HispaBIOS
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
; =============================================================================
;  Durante unos segundos (20, o los que diga el argumento: TECLAPRU 5) espera
;  teclas como las espera el núcleo de Hispa-DOS: pregunta a la BIOS (INT 16h
;  01h) y descansa con HLT hasta el tic siguiente. Es justo la espera con la
;  que el teclado USB del firmware se quedaba mudo en el MEDION S2217
;  (docs/HISPABIOS.md). No hace falta pulsar nada para que acabe: enseña las
;  teclas que llegan, dice cuántas han llegado y lo guarda en \TECLAPRU.TXT.
;  (Texto sin acentos: sale igual en cualquier página de códigos.)
; =============================================================================
        org 100h
        cpu 386

        cld
        mov si, 81h                     ; el argumento: segundos (1 a 99)
.esp:   lodsb
        cmp al, ' '
        je .esp
        xor bx, bx
.cif:   cmp al, '0'
        jb .arg_f
        cmp al, '9'
        ja .arg_f
        sub al, '0'
        cbw
        imul bx, bx, 10
        add bx, ax
        lodsb
        jmp .cif
.arg_f: test bx, bx
        jz .omision
        cmp bx, 99
        ja .omision
        mov [segundos], bx
.omision:
        mov ax, [segundos]              ; en el texto y en tics (18,2 por segundo)
        mov bl, 10
        div bl
        add ax, '00'
        mov [m_seg], ax
        mov ax, [segundos]
        mov bx, 182
        mul bx
        mov bx, 10
        div bx
        mov [tics], ax
        mov dx, m_inicio
        mov ah, 9
        int 21h
        call tic
        mov [t0], ax
.bucle: mov ah, 1                       ; ¿hay tecla?
        int 16h
        jz .nada
        xor ah, ah
        int 16h
        inc word [n]
        mov di, [fin_teclas]
        cmp di, teclas + 60
        jae .llena
        cmp al, 20h
        jae .letra
        mov al, '.'                     ; (Intro, flechas, F1...: un punto)
.letra: stosb
        mov [fin_teclas], di
.llena: mov dl, al
        mov ah, 2
        int 21h
        jmp .bucle
.nada:  call tic
        sub ax, [t0]
        cmp ax, [tics]
        jae .fin
        hlt                             ; hasta el tic siguiente, como el núcleo
        jmp .bucle
.fin:   mov ax, [n]                     ; el número, en el texto
        mov di, m_n + 4
        mov bx, 10
        mov cx, 5
.d:     xor dx, dx
        div bx
        add dl, '0'
        mov [di], dl
        dec di
        loop .d
        mov di, [fin_teclas]
        mov ax, 0A0Dh
        stosw
        mov byte [di], '$'
        mov [fin_teclas], di
        mov dx, m_fin
        mov ah, 9
        int 21h
        mov dx, teclas
        mov ah, 9
        int 21h
        mov ah, 3Ch                     ; \TECLAPRU.TXT
        xor cx, cx
        mov dx, nombre
        int 21h
        jc .salir
        mov bx, ax
        mov ah, 40h
        mov cx, m_fin_f - m_fin2
        mov dx, m_fin2
        int 21h
        mov ah, 40h
        mov cx, [fin_teclas]
        mov dx, teclas
        sub cx, dx
        int 21h
        mov ah, 3Eh
        int 21h
.salir: mov ax, 4C00h
        int 21h

; tic -> AX = la palabra baja del contador de tics de la BIOS (40:6Ch)
tic:    push ds
        xor ax, ax
        mov ds, ax
        mov ax, [46Ch]
        pop ds
        ret

segundos        dw 20
tics            dw 364
t0              dw 0
n               dw 0
fin_teclas      dw teclas
nombre          db '\TECLAPRU.TXT', 0
m_inicio        db 13, 10, 'TECLAPRU: prueba del teclado. Pulse teclas (letras, Intro, flechas) durante '
m_seg           db '20 segundos;', 13, 10, 'acaba sola. Teclas: $'
m_fin           db 13, 10
m_fin2          db 'TECLAPRU: teclas recibidas: '
m_n             db '00000', 13, 10, 'Las teclas: '
m_fin_f         db '$'
teclas          times 64 db 0
