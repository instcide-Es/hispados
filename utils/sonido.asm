; =============================================================================
;  SONIDO.COM - volumen del sonido de Hispa-DOS (habla con HSPSND)
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  SONIDO          muestra el volumen
;  SONIDO n        lo pone (0 a 100)
;  SONIDO + | -    lo sube o lo baja un 10 %
;  SONIDO /M       silencio; otra vez, vuelve el volumen de antes
;  SONIDO /A       ajuste con las flechas (Esc sale)
;
;  API de HSPSND: INT 2Fh AX=D700h -> AL=FFh, ES:BX = entrada (llamada lejana,
;  AH = función): 05h poner volumen (AL = %), 06h leer (AL = %, AH = silencio),
;  07h silencio (AL = 0 quitar, 1 poner, 2 cambiar).
; =============================================================================
        cpu 386
        bits 16
        org 0x100

BARLEN      equ 20

start:
        mov si, 0x81
        call skip_sp
        cmp word [si], '/?'
        jne .nohelp
        mov si, msg_help
        call puts
        mov ax, 0x4C00
        int 0x21
.nohelp:
        push si
        mov ax, 0xD700
        int 0x2F
        pop si
        cmp al, 0xFF
        jne nodrv
        mov [drv], bx
        mov [drv+2], es
        push cs
        pop es
        mov ah, 0
        call far [drv]
        push cs
        pop es
        cmp bx, 2                   ; el volumen legible llegó con la versión 2
        jb olddrv
        mov al, [si]
        cmp al, 13
        je show
        cmp al, '+'
        je plus
        cmp al, '-'
        je minus
        cmp al, '/'
        je option
        cmp al, '0'
        jb usage
        cmp al, '9'
        ja usage
        ; SONIDO n
        xor ax, ax
        xor bx, bx
.d:
        mov bl, [si]
        sub bl, '0'
        jb .e
        cmp bl, 9
        ja .e
        imul ax, ax, 10
        add ax, bx
        cmp ax, 100
        ja range
        inc si
        jmp .d
.e:
        call skip_sp
        cmp byte [si], 13
        jne usage
        call setvol
        jmp show

plus:
        mov bl, 10
        jmp step
minus:
        mov bl, -10
step:
        inc si
        call skip_sp
        cmp byte [si], 13
        jne usage
        mov al, bl
        call addvol
        jmp show

option:
        mov al, [si+1]
        and al, 0xDF
        cmp al, 'M'
        je .mute
        cmp al, 'A'
        je adjust
        jmp usage
.mute:
        mov ax, 0x0702
        call far [drv]
        ; (sigue)
show:
        call line
        mov si, msg_crlf
        call puts
        mov ax, 0x4C00
        int 0x21

usage:
        mov si, msg_usage
        call puts
        mov ax, 0x4C01
        int 0x21
range:
        mov si, msg_range
        call puts
        mov ax, 0x4C01
        int 0x21
nodrv:
        mov si, msg_nodrv
        call puts
        mov ax, 0x4C02
        int 0x21
olddrv:
        mov si, msg_olddrv
        call puts
        mov ax, 0x4C02
        int 0x21

; ---- SONIDO /A: la barra se mueve con las flechas ----
adjust:
        mov si, msg_keys
        call puts
.l:
        mov al, 13
        call putc
        call line
        mov si, msg_pad
        call puts
        mov ah, 0x07
        int 0x21
        test al, al
        jnz .ch
        mov ah, 0x07                ; tecla extendida
        int 0x21
        cmp al, 0x48
        je .up
        cmp al, 0x4D
        je .up
        cmp al, 0x50
        je .down
        cmp al, 0x4B
        je .down
        jmp .l
.ch:
        cmp al, 27
        je .end
        cmp al, 13
        je .end
        cmp al, '+'
        je .up
        cmp al, '-'
        je .down
        cmp al, ' '
        je .mute
        and al, 0xDF
        cmp al, 'M'
        je .mute
        jmp .l
.up:
        mov al, 5
        call addvol
        jmp .l
.down:
        mov al, -5
        call addvol
        jmp .l
.mute:
        mov ax, 0x0702
        call far [drv]
        jmp .l
.end:
        mov si, msg_crlf
        call puts
        mov ax, 0x4C00
        int 0x21

; AL = volumen nuevo (0..100)
setvol:
        mov ah, 5
        call far [drv]
        ret
; AL = cuánto sumar (con signo)
addvol:
        push bx
        mov bl, al
        mov ah, 6
        call far [drv]
        add al, bl
        jns .p
        xor al, al
.p:
        cmp al, 100
        jbe .s
        mov al, 100
.s:
        call setvol
        pop bx
        ret

; «Volumen:  80 %  [################....]» (sin salto de línea)
line:
        mov ah, 6
        call far [drv]
        mov [pct], al
        mov [mute], ah
        mov si, msg_vol
        call puts
        movzx ax, byte [pct]
        call putnum
        mov si, msg_pct
        call puts
        ; casillas llenas: una por cada 5 %
        movzx ax, byte [pct]
        add al, 2
        mov bl, 100 / BARLEN
        div bl
        movzx bx, al
        cmp byte [mute], 0
        je .b
        xor bx, bx                  ; en silencio: barra vacía
.b:
        mov cx, BARLEN
.c:
        mov al, 0xB0                ; casilla vacía
        test bx, bx
        jz .f
        mov al, 0xDB                ; casilla llena
        dec bx
.f:
        call putc
        loop .c
        mov al, ']'
        call putc
        cmp byte [mute], 0
        je .r
        mov si, msg_muted
        call puts
.r:
        ret

skip_sp:
        cmp byte [si], ' '
        je .n
        cmp byte [si], 9
        jne .r
.n:
        inc si
        jmp skip_sp
.r:
        ret

; AX en decimal
putnum:
        push bx
        push cx
        push dx
        mov bx, 10
        xor cx, cx
.l:
        xor dx, dx
        div bx
        push dx
        inc cx
        test ax, ax
        jnz .l
.p:
        pop ax
        add al, '0'
        call putc
        loop .p
        pop dx
        pop cx
        pop bx
        ret
putc:
        push ax
        push dx
        mov dl, al
        mov ah, 2
        int 0x21
        pop dx
        pop ax
        ret
puts:
        lodsb
        test al, al
        jz .r
        call putc
        jmp puts
.r:
        ret

msg_vol     db "Volumen: ", 0
msg_pct     db " %  [", 0
msg_muted   db " (en silencio)", 0
msg_pad     db "               ", 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 0
msg_crlf    db 13, 10, 0
msg_keys    db "Flechas o + y -: volumen.  M: silencio.  Esc: salir.", 13, 10, 0
msg_nodrv   db "HSPSND no está cargado: escriba HSPSND para cargar el sonido.", 13, 10, 0
msg_olddrv  db "El HSPSND cargado es antiguo y no deja leer el volumen.", 13, 10, 0
msg_range   db "El volumen va de 0 a 100.", 13, 10, 0
msg_usage   db "Uso: SONIDO [n | + | - | /M | /A]   (SONIDO /? lo explica)", 13, 10, 0
msg_help    db "Muestra o cambia el volumen del sonido (necesita HSPSND cargado).", 13, 10
            db 13, 10
            db "SONIDO        muestra el volumen", 13, 10
            db "SONIDO n      pone el volumen (de 0 a 100)", 13, 10
            db "SONIDO +      lo sube un 10 %", 13, 10
            db "SONIDO -      lo baja un 10 %", 13, 10
            db "SONIDO /M     silencio; otra vez, vuelve el volumen de antes", 13, 10
            db "SONIDO /A     ajuste con las flechas (Esc sale)", 13, 10
            db 13, 10
            db "Con el teclado, en cualquier programa: Ctrl+Alt+flecha arriba y abajo", 13, 10
            db "suben y bajan el volumen, y Ctrl+Alt+Fin lo silencia.", 13, 10, 0

drv         dd 0
pct         db 0
mute        db 0
