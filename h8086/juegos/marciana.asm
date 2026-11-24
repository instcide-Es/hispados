; =============================================================================
;  MARCIANA.COM - Marcianada, un juego de marcianitos para Hispa-8086
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Una flota de cuarenta bichos baja en formación: Boinas arriba, Botijos en
;  medio y Abanicos abajo. Tu churrera, abajo del todo, les tira churros; tras
;  cuatro murallas que se desgastan con cada golpe te escondes de lo que
;  tiran ellos (aceitunas, que caen despacio, y guindillas en zigzag, que
;  caen deprisa). De cuando en cuando cruza por arriba la Bombona: si la
;  aciertas da muchos puntos y suelta un premio, un Corazón (una vida más) o
;  unos Churros (tiro doble un rato). Desde la segunda oleada las Boinas
;  llevan casco (dos tiros), y desde la tercera algún Abanico se suelta de la
;  flota y baja dando bandazos. Cada oleada empieza más abajo, va más deprisa
;  y tira más. Si la flota llega al suelo, se acabó.
;
;  Teclas: flechas o Z y X mueven; Espacio (o flecha arriba, o Control)
;  dispara; P pausa; S quita o pone el sonido; Esc acaba la partida (y, en la
;  portada, sale). También vale la palanca del PC (puerto 201h), si la hay.
;
;  Hecho para un 8088 a 4,77 MHz con 256 K y CGA (y las EGA y VGA, que la
;  imitan): modo 4, 320x200 a cuatro colores, sprites por programa y solo lo
;  que cambia. El ritmo lo marca el temporizador (canal 0 del 8254 a 60 Hz,
;  sin descuadrar el reloj de la BIOS), no la velocidad del procesador. El
;  teclado, por INT 9 (se pueden pulsar varias teclas a la vez). El sonido,
;  por el altavoz (canal 2 del 8254), desde la misma interrupción del reloj.
;  El récord se guarda al lado del programa, en MARCIANA.REC (dos octetos).
;
;  Pensado para Carlos, que es daltónico, y para los monitores de fósforo
;  verde o ámbar: nada se distingue solo por el color. Cada bicho, cada
;  disparo y cada premio tiene su forma (y en modo texto, su letra).
;    MARCIANA /M   monocromo con CGA: modo 5 (sin color en la salida
;                  compuesta), todo en el color más claro y las murallas
;                  rayadas.
;    MARCIANA /T   modo texto de 80x25 con letras gráficas: lo que se usa
;                  solo con una MDA o una Hercules (se detecta por INT 11h).
;    MARCIANA /S   empieza sin sonido.
;
;  Solo instrucciones del 8086. Con -DPRUEBA se ensambla MARPRUEB.COM, que
;  no pinta en la pantalla (pinta en memoria) y prueba el motor: choques de
;  los disparos con los bichos y con las murallas, el casco, la muerte y el
;  récord en el disco.
; =============================================================================
        cpu 8086
        org 0x100

TICK_DIV    equ 19886               ; 1193182 / 19886 = 60 interrupciones por segundo
JY          equ 176                 ; la fila de arriba de la churrera
SUELO_Y     equ 186
DEF_Y       equ 152                 ; las murallas: de 152 a 163
DEF_X       equ 36                  ; la primera, y de una a otra 72 puntos
BOMB_Y      equ 16
COLS        equ 8
FILAS       equ 5
N_BICHOS    equ COLS * FILAS
FIN         equ 1                   ; [estado]

; los dibujos (ver «tipos», más abajo)
K_EXPLO     equ 8
K_JUG       equ 9
K_ROTO      equ 10
K_BOMB      equ 12
K_PREMIO    equ 12                  ; + 1 el corazón, + 2 los churros
K_CHURRO    equ 15
K_BALA      equ 15                  ; + 1 la aceituna, + 2 la guindilla

inicio:
        cld
        mov sp, pila_fin
        mov di, bss_ini             ; las variables, a cero
        mov cx, bss_fin - bss_ini
        xor al, al
        rep stosb
        mov byte [sonido_si], 1
        mov word [rs], 1
%ifdef PRUEBA
        jmp prueba
%endif
        ; ---- lo que dice la línea de órdenes ----
        mov si, 0x81
.arg:   lodsb
        cmp al, 13
        je .fin_arg
        and al, 0xDF
        cmp al, 'M'
        jne .a1
        mov byte [mono], 1
.a1:    cmp al, 'T'
        jne .a2
        mov byte [modo_txt], 1
.a2:    cmp al, 'S'
        jne .arg
        mov byte [sonido_si], 0
        jmp short .arg
.fin_arg:
        call ruta_record
        call lee_record
        mov ah, 0x0F                ; el modo de vídeo de antes, para volver a él
        int 0x10
        mov [modo_ant], al
        int 0x11                    ; ¿una MDA o una Hercules? Modo texto
        and al, 0x30
        cmp al, 0x30
        jne .color
        mov byte [modo_txt], 2
.color: call pon_video
        ; la semilla, del reloj de la BIOS
        xor ax, ax
        int 0x1A
        mov [semilla], dx
        call busca_palanca
        mov ax, 0x2523              ; Ctrl+C no corta el juego (con el reloj y
        mov dx, nada                ; el teclado enganchados, sería un cuelgue)
        int 0x21
        call instala
        jmp titulo

; ---- salir: todo como estaba ----
salir:
        call desinstala
        mov al, [modo_ant]
        mov ah, 0
        int 0x10
        mov ax, 0x4C00
        int 0x21

; =============================================================================
;  Vídeo
; =============================================================================
pon_video:
        mov word [vseg], 0xB800
        mov byte [col_base], 0
        cmp byte [modo_txt], 0
        je .graf
        mov byte [col_base], 20
        mov byte [color_hud], 0x07
        mov ax, 3
        cmp byte [modo_txt], 2
        jne .t
        mov word [vseg], 0xB000
        mov al, 7
.t:     int 0x10
        mov ah, 1                   ; sin cursor
        mov cx, 0x2000
        int 0x10
        jmp short .es
.graf:  mov byte [color_hud], 3
        mov byte [def_mask], 0x55   ; las murallas, de color 1
        mov byte [suelo_pat], 0x11
        mov ax, 4
        cmp byte [mono], 0
        je .c4
        mov al, 5                   ; sin color (la salida compuesta, en gris)
        int 0x10
        mov byte [def_mask], 0xCC   ; rayadas
        mov byte [suelo_pat], 0x33
        mov si, dibujos             ; y todo en el color 3, el más claro
.m:     lodsb
        mov ah, al
        shr ah, 1
        or al, ah
        and al, 0x55
        mov ah, al
        shl ah, 1
        or al, ah
        mov [si-1], al
        cmp si, dibujos_fin
        jb .m
        jmp short .es
.c4:    int 0x10
        mov ax, 0x0B00              ; fondo negro, y los colores claros
        mov bx, 0x0010
        int 0x10
        mov ax, 0x0B00              ; paleta 1: azul claro, morado y blanco
        mov bx, 0x0101
        int 0x10
.es:    mov es, [vseg]
        ret

borra_pantalla:
        xor di, di
        mov cx, 8192
        xor ax, ax
        cmp byte [modo_txt], 0
        je .g
        mov cx, 2000
        mov ax, 0x0720
.g:     rep stosw
        ret

; espera el principio del retrazado vertical (solo en los modos gráficos de la
; CGA: lo que se pinta entonces no se ve a medias)
retrazo:
        cmp byte [modo_txt], 0
        jne .f
        mov dx, 0x3DA
        mov cx, 0xFFFF
.a:     in al, dx
        test al, 8
        jz .b
        loop .a
.b:     in al, dx
        test al, 8
        jnz .f
        loop .b
.f:     ret

; DI = la dirección de la fila DX (0..199) en la memoria de la CGA
dir_fila:
        mov al, dl
        shr al, 1
        mov ah, 80
        mul ah
        mov di, ax
        test dl, 1
        jz .p
        add di, 0x2000
.p:     ret

; ---- pinta un dibujo: AL = cuál, AH = modo, BX = x, DX = y ----
;  modo 0: copia (con un octeto en blanco a cada lado, que borra el rastro de
;          lo que se mueve hasta 4 puntos de lado); hace falta x >= 4
;  modo 1: lo suma a lo que haya (OR)
;  modo 2: borra solo sus puntos
;  En modo texto pone sus letras (y al borrar, lo que haya de fondo).
;  No cambia ningún registro.
pinta:
        push ax
        push bx
        push cx
        push dx
        push si
        push di
        push bp
        mov [p_modo], ah
        mov ah, 0
        mov si, ax
        shl si, 1
        mov di, si
        shl si, 1
        shl si, 1
        add si, di
        add si, tipos               ; SI = la ficha del dibujo (10 octetos)
        cmp byte [modo_txt], 0
        je .cga
        jmp pinta_txt
.cga:   mov al, [si+2]
        mov [p_w], al
        mov al, [si+3]
        mov [p_h], al
        mov si, [si]
        call dir_fila
        mov ax, bx
        shr ax, 1
        shr ax, 1
        add di, ax
        mov cl, bl
        and cl, 3
        shl cl, 1                   ; CL = lo que se corre a la derecha
        jnz .fila
        cmp byte [p_modo], 0
        jne .fila
        ; sin correr y copiando: lo más rápido (así se pintan los bichos)
.rap:   mov byte [es:di-1], 0
        mov cl, [p_w]
        mov ch, 0
        rep movsb
        mov byte [es:di], 0
        mov al, [p_w]
        cbw
        sub di, ax
        call baja_fila
        dec byte [p_h]
        jnz .rap
        jmp short .fin
.fila:  push di
        cmp byte [p_modo], 0
        jne .nb
        mov byte [es:di-1], 0
.nb:    xor dl, dl
        mov ch, [p_w]
.oct:   lodsb
        mov ah, al
        xor al, al
        shr ax, cl
        or ah, dl
        mov dl, al
        mov al, ah
        call pon
        dec ch
        jnz .oct
        mov al, dl
        call pon
        pop di
        call baja_fila
        dec byte [p_h]
        jnz .fila
.fin:   pop bp
        pop di
        pop si
        pop dx
        pop cx
        pop bx
        pop ax
        ret

baja_fila:
        add di, 0x2000
        cmp di, 0x4000
        jb .f
        sub di, 0x4000 - 80
.f:     ret

pon:    cmp byte [p_modo], 1
        jb .c
        je .o
        mov ah, al                  ; los puntos que no son 0, a 11...
        shr ah, 1
        or ah, al
        and ah, 0x55
        mov al, ah
        shl al, 1
        or al, ah
        not al                      ; ...y esos se borran
        and [es:di], al
        inc di
        ret
.o:     or [es:di], al
        inc di
        ret
.c:     stosb
        ret

; en modo texto: columna x/4, fila (y + alto/2)/8
pinta_txt:
        mov al, [si+3]
        shr al, 1
        cbw
        add ax, dx
        mov cl, 3
        shr ax, cl
        mov ch, al                  ; CH = fila
        mov ax, bx
        shr ax, 1
        shr ax, 1
        mov cl, al                  ; CL = columna
        mov al, ch
        mov ah, 160
        mul ah
        mov di, ax
        mov al, cl
        mov ah, 0
        shl ax, 1
        add di, ax
        mov bl, [si+4]              ; BL = cuántas letras, BH = su atributo
        mov bh, [si+5]
        add si, 6
        cmp byte [p_modo], 0
        jne .l
        or cl, cl
        jz .l
        dec cl
        call fondo
        mov [es:di-2], ax
        inc cl
.l:     cmp byte [p_modo], 2
        je .b
        lodsb
        mov ah, bh
        jmp short .w
.b:     call fondo
.w:     stosw
        inc cl
        dec bl
        jnz .l
        cmp byte [p_modo], 0
        jne .f
        cmp cl, 80
        jae .f
        call fondo
        stosw
.f:     jmp pinta.fin

; AX = lo que va de fondo en la casilla de texto CH = fila, CL = columna: el
; suelo, un trozo de muralla (más o menos lleno según lo que le quede) o nada
fondo:
        mov ax, 0x07C4
        cmp ch, 23
        je .r
        mov ax, 0x0720
        cmp ch, 19
        jb .r
        cmp ch, 20
        ja .r
        push bx
        push cx
        push dx
        push si
        mov al, cl
        sub al, DEF_X / 4
        jb .no
        mov ah, 0
        mov dl, 18
        div dl                      ; AL = la muralla, AH = la columna en ella
        cmp ah, 6
        jae .no
        cmp al, 4
        jae .no
        mov dl, 36
        mul dl
        add ax, defensas
        mov si, ax
        mov dx, 0x0808              ; DH = filas, DL = puntos que caben / 4
        cmp ch, 20
        jne .c0
        add si, 8 * 3
        mov dx, 0x0404
.c0:    mov al, cl
        sub al, DEF_X / 4
        mov ah, 0
        mov cl, 18
        div cl
        mov cl, ah                  ; CL = columna en la muralla (0..5)
        mov al, cl
        shr al, 1
        cbw
        add si, ax
        mov ch, 0xF0                ; el medio octeto de la izquierda o el de la derecha
        test cl, 1
        jz .c1
        mov ch, 0x0F
.c1:    xor bx, bx                  ; BX = los puntos que quedan
.c2:    mov al, [si]
        and al, ch
.c3:    shl al, 1
        adc bl, 0
        or al, al
        jnz .c3
        add si, 3
        dec dh
        jnz .c2
        mov ax, 0x0720
        or bl, bl
        jz .fin
        mov al, 0xB0                ; menos de la mitad
        mov bh, dl
        shl bh, 1
        cmp bl, bh
        jb .fin
        mov al, 0xB1                ; de la mitad a tres cuartos
        add bh, dl
        cmp bl, bh
        jb .fin
        mov al, 0xB2                ; más de tres cuartos
        add bh, dl
        cmp bl, bh
        jb .fin
        mov al, 0xDB                ; entera
.fin:   pop si
        pop dx
        pop cx
        pop bx
.r:     ret
.no:    mov ax, 0x0720
        jmp short .fin

; ---- las murallas ----
pinta_defs:
        mov al, 0
.l:     call pinta_def
        inc al
        cmp al, 4
        jb .l
        ret

; pinta la muralla AL (no cambia AX)
pinta_def:
        push ax
        push bx
        push cx
        push dx
        push si
        push di
        mov bl, al
        mov ah, 36
        mul ah
        add ax, defensas
        mov si, ax
        mov al, 18
        mul bl
        add ax, DEF_X / 4
        cmp byte [modo_txt], 0
        jne .txt
        mov bx, ax
        mov dx, DEF_Y
        call dir_fila
        add di, bx
        mov ch, 12
.f:     mov cl, 3
.o:     lodsb
        mov ah, al
        shr al, 1
        shr al, 1
        shr al, 1
        shr al, 1
        mov bx, medios
        xlat
        and al, [def_mask]
        stosb
        mov al, ah
        and al, 0x0F
        xlat
        and al, [def_mask]
        stosb
        dec cl
        jnz .o
        sub di, 6
        call baja_fila
        dec ch
        jnz .f
        jmp short .fin
.txt:   mov cl, al
        mov ch, 19
        mov bl, 2
.t1:    push cx
        mov al, ch
        mov ah, 160
        mul ah
        mov di, ax
        mov al, cl
        mov ah, 0
        shl ax, 1
        add di, ax
        mov bh, 6
.t2:    call fondo
        stosw
        inc cl
        dec bh
        jnz .t2
        pop cx
        inc ch
        dec bl
        jnz .t1
.fin:   pop di
        pop si
        pop dx
        pop cx
        pop bx
        pop ax
        ret

pinta_suelo:
        cmp byte [modo_txt], 0
        jne .txt
        mov dx, SUELO_Y
        call dir_fila
        mov al, [suelo_pat]
        mov cx, 80
        rep stosb
        ret
.txt:   mov di, 23 * 160
        mov cx, 80
        mov ax, 0x07C4
        rep stosw
        ret

; =============================================================================
;  Texto (por la BIOS: vale en los modos gráficos y en los de texto)
; =============================================================================
; escribe la cadena SI (acaba en 0) en DH = fila, DL = columna (de 40)
texto:  cmp byte [sin_hud], 0
        jne .f
.l:     lodsb
        or al, al
        jz .f
        call letra
        inc dl
        jmp short .l
.f:     ret

letra:  push ax
        push bx
        push cx
        push dx
        push ax
        add dl, [col_base]
        mov ah, 2
        mov bh, 0
        int 0x10
        pop ax
        mov ah, 9
        mov bh, 0
        mov bl, [color_hud]
        mov cx, 1
        int 0x10
        pop dx
        pop cx
        pop bx
        pop ax
        ret

; una lista de cadenas: fila, columna, texto, 0; y 0FFh al final
lista:  lodsw
        cmp al, 0xFF
        je .f
        xchg ah, al
        mov dx, ax
        call texto
        jmp short lista
.f:     ret

; AX en decimal, con CL cifras, en DH, DL
numero: push dx
        mov di, cifras + 5
        mov byte [di], 0
        mov bx, 10
.l:     xor dx, dx
        div bx
        add dl, '0'
        dec di
        mov [di], dl
        dec cl
        jnz .l
        pop dx
        mov si, di
        jmp texto

; los números del marcador (si ha cambiado algo)
marcador:
        cmp byte [hud_sucio], 0
        je .f
        mov byte [hud_sucio], 0
        mov ax, [puntos]
        mov cl, 5
        mov dx, 0x0007
        call numero
        mov ax, [record]
        cmp ax, [puntos]
        jae .r
        mov ax, [puntos]
.r:     mov cl, 5
        mov dx, 0x0015
        call numero
        mov al, [oleada]
        mov ah, 0
        mov cl, 2
        mov dx, 0x0023
        call numero
        mov al, [vidas]
        mov ah, 0
        mov cl, 1
        mov dx, 0x1806
        call numero
        mov si, t_si
        cmp byte [sonido_si], 0
        jne .s
        mov si, t_no
.s:     mov dx, 0x1811
        call texto
.f:     ret

etiquetas:
        mov si, l_marcador
        call lista
        mov byte [hud_sucio], 1
        jmp marcador

; =============================================================================
;  Interrupciones: reloj (con el sonido) y teclado
; =============================================================================
instala:
        cli
        xor ax, ax
        mov ds, ax
        mov ax, [8 * 4]
        mov [cs:viejo8], ax
        mov ax, [8 * 4 + 2]
        mov [cs:viejo8 + 2], ax
        mov ax, [9 * 4]
        mov [cs:viejo9], ax
        mov ax, [9 * 4 + 2]
        mov [cs:viejo9 + 2], ax
        mov word [8 * 4], int8
        mov [8 * 4 + 2], cs
        mov word [9 * 4], int9
        mov [9 * 4 + 2], cs
        push cs
        pop ds
        mov al, 0x36                ; canal 0, a 60 Hz
        out 0x43, al
        mov ax, TICK_DIV
        out 0x40, al
        mov al, ah
        out 0x40, al
        mov al, 0xB6                ; canal 2 (el altavoz), onda cuadrada
        out 0x43, al
        sti
        ret

desinstala:
        cli
        mov al, 0x36                ; el canal 0, como lo deja la BIOS
        out 0x43, al
        xor al, al
        out 0x40, al
        out 0x40, al
        in al, 0x61                 ; el altavoz, callado
        and al, 0xFC
        out 0x61, al
        xor ax, ax
        mov es, ax
        mov ax, [viejo8]
        mov [es:8 * 4], ax
        mov ax, [viejo8 + 2]
        mov [es:8 * 4 + 2], ax
        mov ax, [viejo9]
        mov [es:9 * 4], ax
        mov ax, [viejo9 + 2]
        mov [es:9 * 4 + 2], ax
        sti
        ret

nada:   iret

int8:   push ax
        push bx
        push ds
        push cs
        pop ds
        inc word [ticks]
        call sonido
        add word [acum], TICK_DIV   ; cada 65536 cuentas, el reloj de la BIOS
        jc .bios
        mov al, 0x20
        out 0x20, al
        pop ds
        pop bx
        pop ax
        iret
.bios:  pop ds
        pop bx
        pop ax
        jmp far [cs:viejo8]

int9:   push ax
        push bx
        push ds
        push cs
        pop ds
        in al, 0x60
        mov bl, al
        in al, 0x61                 ; la tecla, atendida (lo pide el XT)
        mov ah, al
        or al, 0x80
        out 0x61, al
        mov al, ah
        out 0x61, al
        cmp bl, 0xE0
        je .f
        mov al, bl
        mov bh, 0
        and bl, 0x7F
        test al, 0x80
        jnz .suelta
        mov byte [teclas + bx], 1
        mov [ultima], bl
        jmp short .f
.suelta:
        mov byte [teclas + bx], 0
.f:     mov al, 0x20
        out 0x20, al
        pop ds
        pop bx
        pop ax
        iret

; ---- el sonido: un efecto (tono que sube o baja, o ruido) o una melodía ----
sonido: cmp byte [s_t], 0
        jne .suena
        mov bx, [m_ptr]
        or bx, bx
        jz .calla
        mov al, [bx]
        or al, al
        jz .fin_mel
        mov [s_t], al
        mov ax, [bx+1]
        mov [s_div], ax
        mov word [s_del], 0
        mov byte [s_ru], 0
        add bx, 3
        mov [m_ptr], bx
        jmp short .suena
.fin_mel:
        mov word [m_ptr], 0
.calla: in al, 0x61
        and al, 0xFC
        out 0x61, al
        ret
.suena: dec byte [s_t]
        cmp byte [sonido_si], 0
        je .calla
        mov ax, [s_div]
        mov bx, [s_del]
        add [s_div], bx
        cmp byte [s_ru], 0
        je .tono
        mov bx, [rs]                ; ruido: un registro de desplazamiento
        shl bx, 1
        jnc .r
        xor bx, 0x100B
.r:     mov [rs], bx
        and bx, 0x0FFF
        add ax, bx
.tono:  or ax, ax
        jz .calla
        out 0x42, al
        mov al, ah
        out 0x42, al
        in al, 0x61
        or al, 3
        out 0x61, al
        ret

; SI = el efecto (dw tono, dw cambio por tic, db tics, db ruido)
suena:  push ax
        push di
        push es
        push ds
        pop es
        cli
        mov word [m_ptr], 0
        mov di, s_div
        movsw
        movsw
        movsb
        movsb
        sub si, 6
        sti
        pop es
        pop di
        pop ax
        ret

; lo mismo, pero solo si no suena nada
suena_libre:
        cmp byte [s_t], 0
        jne .f
        cmp word [m_ptr], 0
        jne .f
        call suena
.f:     ret

melodia:
        cli
        mov byte [s_t], 0
        mov [m_ptr], si
        sti
        ret

; =============================================================================
;  La palanca (puerto 201h)
; =============================================================================
LIM_PAL equ 3000

eje:    mov dx, 0x201
        cli
        out dx, al
        xor cx, cx
.l:     in al, dx
        test al, 1
        jz .f
        inc cx
        cmp cx, LIM_PAL
        jb .l
.f:     sti
        ret

busca_palanca:
        call eje
        cmp cx, LIM_PAL
        jae .no
        jcxz .no
        mov [pal_centro], cx
        mov byte [hay_pal], 1
.no:    ret

palanca:
        cmp byte [hay_pal], 0
        je .f
        mov dx, 0x201
        in al, dx
        not al
        and al, 0x30
        mov [pal_fue], al
        call eje
        mov ax, [pal_centro]
        mov bx, ax
        shr bx, 1
        shr bx, 1                   ; un cuarto del centro, de margen
        xor dx, dx                  ; DL = izquierda, DH = derecha
        sub ax, bx
        cmp cx, ax
        jae .d
        inc dl
.d:     add ax, bx
        add ax, bx
        cmp cx, ax
        jbe .p
        inc dh
.p:     mov [pal_izq], dx
.f:     ret

; =============================================================================
;  El récord
; =============================================================================
; la ruta: la del programa (detrás de su entorno) con .REC en vez de .COM
ruta_record:
        push es
        mov es, [0x2C]
        mov ax, es
        or ax, ax
        jz .def
        xor di, di
.b:     cmp word [es:di], 0
        je .e
        inc di
        cmp di, 2048
        jb .b
        jmp short .def
.e:     cmp word [es:di+2], 1
        jb .def
        lea si, [di+4]
        mov di, ruta
        xor bx, bx                  ; BX = dónde va el punto
.c:     mov al, [es:si]
        inc si
        mov [di], al
        cmp al, '.'
        jne .c1
        mov bx, di
.c1:    cmp al, '\'
        jne .c2
        xor bx, bx
.c2:    inc di
        or al, al
        jz .ext
        cmp di, ruta + 76
        jb .c
        jmp short .def
.ext:   or bx, bx
        jz .def
        mov word [bx+1], 'RE'
        mov word [bx+3], 'C'
        pop es
        ret
.def:   pop es
        mov si, t_rec
        mov di, ruta
        push es
        push ds
        pop es
        mov cx, 13
        rep movsb
        pop es
        ret

lee_record:
        mov ax, 0x3D00
        mov dx, ruta
        int 0x21
        jc .f
        mov bx, ax
        mov ah, 0x3F
        mov cx, 2
        mov dx, record
        int 0x21
        mov ah, 0x3E
        int 0x21
.f:     ret

guarda_record:
        mov ax, [viejo9]            ; el teclado, a la BIOS mientras tanto: si
        mov dx, [viejo9 + 2]        ; hace falta cambiar de disquete, el núcleo
        call pon9                   ; lo pide y espera una tecla (INT 16h)
        mov ah, 0x3C
        xor cx, cx
        mov dx, ruta
        int 0x21
        jc .f
        mov bx, ax
        mov ah, 0x40
        mov cx, 2
        mov dx, record
        int 0x21
        mov ah, 0x3E
        int 0x21
.f:     mov ax, int9
        mov dx, cs
pon9:   cmp word [viejo9 + 2], 0    ; (sin instalar, como en la prueba: nada)
        je .f
        cli
        push es
        xor bx, bx
        mov es, bx
        mov [es:9 * 4], ax
        mov [es:9 * 4 + 2], dx
        pop es
        sti
.f:     ret

; =============================================================================
;  Lo de siempre
; =============================================================================
; AX = un número al azar (y AL, su parte más revuelta); no cambia DX
azar:   push dx
        mov ax, [semilla]
        mov dx, 25173
        mul dx
        add ax, 13849
        mov [semilla], ax
        xchg al, ah
        pop dx
        ret

suma:   add [puntos], ax
        jnc .f
        mov word [puntos], 0xFFFF
.f:     mov byte [hud_sucio], 1
        ret

; ¿qué teclas (o palanca) están pulsadas? AL = izquierda, AH = derecha,
; BL = disparo (distinto de 0 si sí)
mandos: mov al, [teclas + 0x4B]
        or al, [teclas + 0x2C]
        or al, [pal_izq]
        mov ah, [teclas + 0x4D]
        or ah, [teclas + 0x2D]
        or ah, [pal_der]
        mov bl, [teclas + 0x39]
        or bl, [teclas + 0x48]
        or bl, [teclas + 0x1D]
        or bl, [pal_fue]
        ret

; espera un tic del reloj; CX = cuántos han pasado (de 1 a 6)
espera_tic:
        sti
.h:     mov ax, [ticks]
        cmp ax, [hechos]
        jne .t
        hlt
        jmp short .h
.t:     mov cx, ax
        sub cx, [hechos]
        mov [hechos], ax
        cmp cx, 6
        jbe .f
        mov cx, 6
.f:     ret

; =============================================================================
;  La portada
; =============================================================================
titulo: call borra_pantalla
        mov si, l_titulo
        call lista
        mov si, leyenda             ; los dibujos al lado de sus nombres
.l:     lodsw                       ; AL = dibujo, AH = fila
        cmp al, 0xFF
        je .ley
        mov dl, ah
        mov dh, 0
        mov cl, 3
        shl dx, cl                  ; y = fila * 8
        mov bx, 72                  ; x = la columna 9 de 40 (o la 28 de 80)
        cmp byte [modo_txt], 0
        je .p
        mov bx, 28 * 4
.p:     mov ah, 0
        call pinta
        jmp short .l
.ley:   mov ax, [record]
        mov cl, 5
        mov dx, 0x0116
        call numero
.espera:
        call espera_tic
        call palanca
        xor al, al
        xchg al, [ultima]
        cmp al, 1                   ; Esc
        jne .e1
        jmp salir
.e1:    cmp al, 0x1F                ; S
        jne .e2
        xor byte [sonido_si], 1
.e2:    cmp al, 0x39                ; Espacio o Intro
        je juego
        cmp al, 0x1C
        je juego
        cmp byte [pal_fue], 0
        jne juego
        jmp short .espera

; =============================================================================
;  La partida
; =============================================================================
juego:  call nueva_partida
.b:     call espera_tic
        push cx
        call palanca
        xor al, al
        xchg al, [ultima]
        pop cx
        cmp al, 1
        je fin_partida
        cmp al, 0x19                ; P
        jne .s
        xor byte [pausa], 1
        call para_sonido
        mov si, t_pausa
        mov dx, 0x1800
        cmp byte [pausa], 0
        jne .t
        call etiquetas
        jmp short .c
.t:     call texto
        jmp short .c
.s:     cmp al, 0x1F                ; S
        jne .c
        xor byte [sonido_si], 1
        mov byte [hud_sucio], 1
.c:     cmp byte [pausa], 0
        jne .b
        push cx
        call retrazo
        pop cx
.p:     push cx
        call paso
        pop cx
        cmp byte [estado], FIN
        je fin_partida
        loop .p
        call marcador
        jmp short .b

fin_partida:
        call para_sonido
        mov byte [pausa], 0
        mov si, t_fin
        mov dx, 0x0C0B
        call texto
        mov ax, [puntos]
        cmp ax, [record]
        jbe .n
        mov [record], ax
        call guarda_record
        mov si, t_nuevo
        mov dx, 0x0E0D
        call texto
.n:     mov byte [hud_sucio], 1
        call marcador
        mov si, ef_muerte
        call suena
        mov word [espera], 0
.e:     call espera_tic
        add [espera], cx
        xor al, al
        xchg al, [ultima]
        cmp word [espera], 60       ; el primer segundo no vale tecla
        jb .e
        cmp word [espera], 300
        jae .t
        or al, al
        jz .e
.t:     jmp titulo

para_sonido:
        cli
        mov byte [s_t], 0
        mov word [m_ptr], 0
        in al, 0x61
        and al, 0xFC
        out 0x61, al
        sti
        ret

nueva_partida:
        xor ax, ax
        mov [puntos], ax
        mov [estado], al
        mov [jm], al
        mov [doble], ax
        mov [dcool], al
        mov byte [vidas], 3
        mov byte [oleada], 1
        mov word [jx], 152

; ---- una oleada nueva: todo a su sitio, y a pintarlo ----
nueva_oleada:
        xor ax, ax
        mov di, limpia_ini
        mov cx, limpia_fin - limpia_ini
        push es
        push ds
        pop es
        rep stosb
        mov di, vivos
        mov cx, N_BICHOS
        mov al, 1
        rep stosb
        mov al, [oleada]            ; con casco: las Boinas desde la 2.ª, los
        mov cx, 0                   ; Botijos de arriba desde la 4.ª y todos
        cmp al, 2                   ; los Botijos desde la 6.ª
        jb .c
        mov cl, 8
        cmp al, 4
        jb .c
        mov cl, 16
        cmp al, 6
        jb .c
        mov cl, 24
.c:     mov di, vivos
        mov al, 2
        rep stosb
        mov si, muralla             ; las murallas, enteras
        mov di, defensas
        mov dl, 4
.m:     mov cx, 36
        rep movsb
        sub si, 36
        dec dl
        jnz .m
        pop es
        mov byte [n_vivos], N_BICHOS
        mov byte [fx], 28
        mov al, [oleada]
        dec al
        cmp al, 4
        jbe .y
        mov al, 4
.y:     mov cl, 3
        shl al, cl
        add al, 24
        mov ah, 0
        mov [fy], ax
        mov byte [fdir], 1
        mov byte [fcuenta], 30
        mov byte [fcool], 90
        call azar
        and ax, 0x1FF
        add ax, 600
        mov [bomb_t], ax
        mov word [buzo_t], 400
        ; y a pintar
        call borra_pantalla
        call etiquetas
        call pinta_suelo
        call pinta_defs
        mov ah, 0
        call pinta_flota
        cmp byte [jm], 0
        jne .f
        mov al, K_JUG
        mov ah, 0
        mov bx, [jx]
        mov dx, JY
        call pinta
.f:     mov si, m_oleada
        jmp melodia

; ---- un tic de juego ----
paso:   call p_jugador
        call p_disparos
        call p_balas
        call p_flota
        call p_fuego
        call p_bomb
        call p_buzo
        call p_premio
        call p_explos
        cmp word [doble], 0
        je .d
        dec word [doble]
.d:     cmp byte [n_vivos], 0
        jne .f
        cmp byte [buzo_act], 0
        jne .f
        cmp byte [oleada], 99
        jae .o
        inc byte [oleada]
.o:     call nueva_oleada
.f:     ret

; ---- la churrera ----
p_jugador:
        cmp byte [jm], 0
        je .viva
        dec byte [jm]
        jz .vuelve
        mov al, [jm]
        test al, 7
        jnz .r
        mov cl, 3
        shr al, cl
        and al, 1
        add al, K_ROTO
        mov ah, 0
        mov bx, [jx]
        mov dx, JY
        call pinta
.r:     ret
.vuelve:
        mov ah, 2
        mov al, K_ROTO
        mov bx, [jx]
        mov dx, JY
        call pinta
        inc al
        call pinta
        cmp byte [vidas], 0
        jne .otra
        mov byte [estado], FIN
        ret
.otra:  mov word [jx], 152
        mov al, K_JUG
        mov ah, 0
        mov bx, 152
        call pinta
        ret
.viva:  call mandos
        mov dx, [jx]
        or al, al
        jz .d
        sub dx, 2
        cmp dx, 4
        jge .d
        mov dx, 4
.d:     or ah, ah
        jz .m
        add dx, 2
        cmp dx, 300
        jle .m
        mov dx, 300
.m:     cmp dx, [jx]
        je .fuego
        mov [jx], dx
        push bx
        mov bx, dx
        mov al, K_JUG
        mov ah, 0
        mov dx, JY
        call pinta
        pop bx
.fuego: cmp byte [dcool], 0
        je .f1
        dec byte [dcool]
        ret
.f1:    or bl, bl
        jz .r2
        mov si, 0
        cmp word [dp_y], 0
        je .tira
        cmp word [doble], 0
        je .r2
        mov si, 2
        cmp word [dp_y + 2], 0
        jne .r2
.tira:  mov ax, [jx]
        add ax, 8
        mov [dp_x + si], ax
        mov word [dp_y + si], JY - 6
        mov byte [dcool], 14
        cmp word [doble], 0
        je .s
        mov byte [dcool], 7
.s:     mov si, ef_tiro
        call suena
.r2:    ret

; la churrera, alcanzada
muere:  cmp byte [jm], 0
        jne .f
        mov byte [jm], 120
        dec byte [vidas]
        mov byte [hud_sucio], 1
        mov si, ef_muerte
        call suena
        mov al, K_ROTO
        mov ah, 0
        mov bx, [jx]
        mov dx, JY
        call pinta
        ; lo que estaban tirando, fuera
        xor si, si
.b:     cmp byte [de_t + si], 0
        je .n
        call borra_bala
        mov byte [de_t + si], 0
.n:     inc si
        cmp si, 6
        jb .b
.f:     ret

; ---- los churros (tus disparos): 4 puntos por tic, mirando cada punto ----
p_disparos:
        xor si, si
.b:     mov dx, [dp_y + si]
        or dx, dx
        jz .sig
        mov bx, [dp_x + si]
        dec bx
        mov ax, (2 << 8) | K_CHURRO
        call pinta
        inc bx
        mov cx, 4
.p:     dec dx
        cmp dx, 9
        jb .muere
        push cx
        push si
        call choca_churro
        pop si
        pop cx
        jc .muere
        loop .p
        mov [dp_y + si], dx
        dec bx
        mov ax, (1 << 8) | K_CHURRO
        call pinta
        jmp short .sig
.muere: mov word [dp_y + si], 0
.sig:   add si, 2
        cmp si, 4
        jb .b
        ret

; ¿choca el churro que está en BX, DX? CF = sí (y lo que haga falta, hecho)
choca_churro:
        ; la Bombona
        cmp byte [bomb_act], 0
        je .bu
        mov ax, dx
        sub ax, BOMB_Y
        cmp ax, 8
        jae .bu
        mov ax, bx
        sub ax, [bomb_x]
        cmp ax, 16
        jae .bu
        call mata_bomb
        stc
        ret
        ; el Abanico suelto
.bu:    cmp byte [buzo_act], 0
        je .fl
        mov ax, dx
        sub ax, [buzo_y]
        cmp ax, 8
        jae .fl
        mov ax, bx
        sub ax, [buzo_x]
        cmp ax, 8
        jae .fl
        call mata_buzo
        stc
        ret
        ; la flota
.fl:    call toca_bicho
        jnc .mu
        call mata_bicho
        stc
        ret
        ; las murallas
.mu:    call def_pos
        jnc .no
        test [si], al
        jz .no
        mov al, -1
        call erosiona
        stc
        ret
.no:    clc
        ret

; ---- lo que tiran ellos ----
borra_bala:                         ; SI = cuál
        push bx
        push si
        mov al, [de_t + si]
        add al, K_BALA
        mov ah, 2
        shl si, 1
        mov bx, [de_x + si]
        dec bx
        mov dx, [de_y + si]
        call pinta
        pop si
        pop bx
        ret

p_balas:
        xor si, si
.b:     cmp byte [de_t + si], 0
        je .sig
        call borra_bala
        mov di, si
        shl di, 1
        mov bx, [de_x + di]
        mov dx, [de_y + di]
        mov cl, [de_t + si]         ; 1 o 2 puntos por tic
        mov ch, 0
.p:     inc dx
        push cx
        call choca_bala
        pop cx
        jc .muere
        loop .p
        mov [de_y + di], dx
        mov al, [de_t + si]
        add al, K_BALA
        mov ah, 1
        dec bx
        call pinta
        jmp short .sig
.muere: mov byte [de_t + si], 0
.sig:   inc si
        cmp si, 6
        jb .b
        ret

; ¿choca la bala SI (arriba en BX, DX)? Mira la fila de abajo, sus tres puntos
choca_bala:
        push bx
        push dx
        push si
        push di
        mov al, 3                   ; el alto: 4 la aceituna, 6 la guindilla
        cmp byte [de_t + si], 1
        je .a
        mov al, 5
.a:     cbw
        add dx, ax                  ; DX = su fila de abajo
        cmp dx, SUELO_Y
        jae .si
        cmp byte [jm], 0
        jne .mu
        mov ax, dx
        sub ax, JY
        cmp ax, 8
        jae .mu
        mov ax, bx
        sub ax, [jx]
        cmp ax, 16
        jae .mu
        call muere
        jmp short .si
.mu:    dec bx
        mov cx, 3
.m:     push cx
        call def_pos
        pop cx
        jnc .n
        test [si], al
        jz .n
        mov al, 1
        call erosiona
        jmp short .si
.n:     inc bx
        loop .m
        clc
        jmp short .f
.si:    stc
.f:     pop di
        pop si
        pop dx
        pop bx
        ret

; ---- las murallas: ¿qué punto es BX, DX? ----
; CF = 1 si cae en una muralla: SI = su octeto, AL = su bit, AH = cuál
def_pos:
        push bx
        push cx
        push dx
        sub dx, DEF_Y
        jb .no
        cmp dx, 12
        jae .no
        mov ax, bx
        sub ax, DEF_X
        jb .no
        mov cl, 72
        div cl
        cmp ah, 24
        jae .no
        cmp al, 4
        jae .no
        mov cl, ah                  ; CL = x dentro de la muralla
        mov ch, al                  ; CH = cuál
        mov ah, 36
        mul ah
        mov si, ax
        mov ax, dx
        mov dl, 3
        mul dl
        add si, ax
        mov al, cl
        shr al, 1
        shr al, 1
        shr al, 1
        cbw
        add si, ax
        add si, defensas
        and cl, 7
        mov al, 0x80
        shr al, cl
        mov ah, ch
        stc
        jmp short .f
.no:    clc
.f:     pop dx
        pop cx
        pop bx
        ret

; un golpe en la muralla, en BX, DX: un bocado de 5 x 4 puntos (con los
; bordes al azar) hacia arriba (AL = -1) o hacia abajo (AL = 1)
erosiona:
        push bx
        push cx
        push dx
        push si
        push bp
        cbw
        mov bp, ax                  ; BP = el paso
        call def_pos
        push ax                     ; AH = la muralla
        call azar
        mov [e_bits], ax
        mov ch, 4
.f:     push bx
        sub bx, 2
        mov cl, 5
.c:     cmp cl, 1
        je .borde
        cmp cl, 5
        jne .quita
.borde: shr word [e_bits], 1
        jnc .sig
.quita: push ax
        call def_pos
        jnc .x
        not al
        and [si], al
.x:     pop ax
.sig:   inc bx
        dec cl
        jnz .c
        pop bx
        add dx, bp
        dec ch
        jnz .f
        pop ax
        mov al, ah
        call pinta_def
        mov si, ef_muro
        call suena_libre
        pop bp
        pop si
        pop dx
        pop cx
        pop bx
        ret

; quita de las murallas lo que hay debajo de un bicho de 8 x 8 en BX, DX, y
; las repinta
raspa:  push bx
        push cx
        push dx
        push si
        mov byte [e_bits], 0
        mov ch, 8
.f:     push bx
        mov cl, 8
.c:     call def_pos
        jnc .n
        test [si], al
        jz .n
        not al
        and [si], al
        mov [e_bits], ah
        or byte [e_bits], 0x80
.n:     inc bx
        dec cl
        jnz .c
        pop bx
        inc dx
        dec ch
        jnz .f
        mov al, [e_bits]
        test al, 0x80
        jz .fin
        and al, 3
        call pinta_def
        add bx, 7                   ; (y la de la derecha, si era otra)
        mov dx, DEF_Y
        call def_pos
        jnc .fin
        mov al, ah
        call pinta_def
.fin:   pop si
        pop dx
        pop cx
        pop bx
        ret

; ---- la flota ----
; BP = la casilla (0..39): AL = el dibujo, BX = x, DX = y
celda:  mov ax, bp
        and al, 7
        mov ah, 3
        mul ah
        add al, [fx]
        shl ax, 1
        shl ax, 1
        mov bx, ax
        mov ax, bp
        mov cl, 3
        shr al, cl
        mov dl, al
        mov cl, 4
        shl dl, cl
        mov dh, 0
        add dx, [fy]
        mov ah, al                  ; AH = la fila
        mov al, 3                   ; con casco
        cmp byte [vivos + bp], 2
        je .k
        mov al, ah                  ; fila 0: Boina; 1 y 2: Botijo; 3 y 4: Abanico
        inc al
        shr al, 1
.k:     shl al, 1
        add al, [fframe]
        ret

; pinta (AH = modo) todos los bichos vivos
pinta_flota:
        mov [modo_flota], ah
        xor bp, bp
.l:     cmp byte [vivos + bp], 0
        je .n
        call celda
        mov ah, [modo_flota]
        call pinta
.n:     inc bp
        cmp bp, N_BICHOS
        jb .l
        ret

; ¿hay un bicho vivo en BX, DX? CF = sí, y BP = cuál
toca_bicho:
        push ax
        push cx
        mov al, [fx]
        mov ah, 0
        shl ax, 1
        shl ax, 1
        mov cx, bx
        sub cx, ax
        jb .no
        mov ax, cx
        mov cl, 12
        div cl
        cmp al, COLS
        jae .no
        cmp ah, 8
        jae .no
        mov ch, al                  ; CH = columna
        mov ax, dx
        sub ax, [fy]
        jb .no
        test al, 8
        jnz .no
        mov cl, 4
        shr ax, cl
        cmp ax, FILAS
        jae .no
        mov cl, 3
        shl al, cl
        add al, ch
        mov bp, ax
        cmp byte [vivos + bp], 0
        je .no
        stc
        jmp short .f
.no:    clc
.f:     pop cx
        pop ax
        ret

; el churro le da al bicho BP
mata_bicho:
        push bx
        push dx
        cmp byte [vivos + bp], 2
        jne .muere
        mov byte [vivos + bp], 1    ; pierde el casco
        call celda
        mov ah, 2
        push ax
        mov al, 6
        add al, [fframe]
        call pinta                  ; (borra el casco...)
        pop ax
        mov ah, 0
        call pinta                  ; (...y pinta lo de debajo)
        mov ax, 5
        call suma
        mov si, ef_casco
        call suena
        jmp short .f
.muere: mov byte [vivos + bp], 0
        dec byte [n_vivos]
        call celda
        push ax
        mov al, K_EXPLO
        call explota
        pop ax
        shr al, 1                   ; 30, 20 o 10 puntos
        mov ah, 0
        mov si, ax
        mov al, [puntos_bicho + si]
        mov ah, 0
        call suma
        mov si, ef_explo
        call suena
.f:     pop dx
        pop bx
        ret

; ---- la flota se mueve (cuando le toca): de lado, o baja y da la vuelta ----
p_flota:
        dec byte [fcuenta]
        jz .toca
        ret
.toca:  mov al, [n_vivos]           ; cuanto menos quedan, más deprisa
        shr al, 1
        shr al, 1
        add al, 3
        mov ah, [oleada]
        shr ah, 1
        sub al, ah
        jg .c
        mov al, 1
.c:     mov [fcuenta], al
        cmp byte [n_vivos], 0
        jne .hay
        ret
.hay:   mov cx, 0x00FF              ; CL = la primera columna con bichos, CH = la última
        mov dh, 0                   ; DH = la fila más baja con bichos
        xor bp, bp
.l:     cmp byte [vivos + bp], 0
        je .n
        mov ax, bp
        and al, 7
        cmp al, cl
        jae .l1
        mov cl, al
.l1:    cmp al, ch
        jbe .l2
        mov ch, al
.l2:    mov ax, bp
        shr al, 1
        shr al, 1
        shr al, 1
        cmp al, dh
        jbe .n
        mov dh, al
.n:     inc bp
        cmp bp, N_BICHOS
        jb .l
        mov [fila_baja], dh
        mov al, [fx]
        add al, [fdir]
        mov dl, ch                  ; ¿se sale por la derecha?
        mov ah, 3
        mov bl, al
        mov al, dl
        mul ah
        add al, bl
        cmp al, 77
        jg .baja
        mov al, cl                  ; ¿o por la izquierda?
        mov ah, 3
        mul ah
        add al, bl
        cmp al, 1
        jl .baja
        mov [fx], bl
        xor byte [fframe], 1
        mov ah, 0
        call pinta_flota
        mov si, ef_tic
        jmp suena_libre
.baja:  mov ah, 2
        call pinta_flota
        add word [fy], 4
        neg byte [fdir]
        xor byte [fframe], 1
        ; ¿llega a las murallas? Se las va comiendo
        mov al, [fila_baja]
        mov cl, 4
        shl al, cl
        mov ah, 0
        add ax, [fy]
        add ax, 8                   ; AX = por donde llega la flota
        mov [p_fondo], ax
        cmp ax, DEF_Y
        jbe .pinta
        xor bp, bp
.r:     cmp byte [vivos + bp], 0
        je .rn
        call celda
        mov ax, dx
        add ax, 8
        cmp ax, DEF_Y
        jbe .rn
        call raspa
.rn:    inc bp
        cmp bp, N_BICHOS
        jb .r
.pinta: mov ah, 0
        call pinta_flota
        cmp word [p_fondo], JY      ; ¿ha llegado al suelo? Se acabó
        jb .f
        mov byte [vidas], 1
        call muere
.f:     ret

; ---- tiran ellos ----
p_fuego:
        cmp byte [jm], 0
        jne .f
        dec byte [fcool]
        jnz .f
        mov al, [oleada]            ; la próxima: cada vez antes
        mov ah, 8
        mul ah
        mov ah, 90
        sub ah, al
        jb .m
        cmp ah, 10
        jae .m1
.m:     mov ah, 10
.m1:    call azar
        and al, 31
        add al, ah
        mov [fcool], al
        mov cl, [oleada]            ; cuántas a la vez: 2 + la oleada, hasta 6
        add cl, 2
        cmp cl, 6
        jbe .c
        mov cl, 6
.c:     xor si, si
        mov di, -1                  ; DI = un hueco libre
.h:     cmp byte [de_t + si], 0
        jne .h1
        mov di, si
        jmp short .h2
.h1:    dec cl
        jz .f
.h2:    inc si
        cmp si, 6
        jb .h
        cmp di, -1
        je .f
        call azar                   ; ¿quién tira? El más bajo de una columna
        and al, 7
        mov dl, al
        mov ch, COLS
.col:   mov al, dl
        add al, 4 * COLS
        mov ah, 0
        mov bp, ax
.fil:   cmp byte [vivos + bp], 0
        jne .tira
        sub bp, COLS
        jge .fil
        inc dl
        and dl, 7
        dec ch
        jnz .col
.f:     ret
.tira:  call celda
        add bx, 4
        add dx, 8
        mov si, di
        shl di, 1
        mov [de_x + di], bx
        mov [de_y + di], dx
        mov byte [de_t + si], 1     ; aceituna, o guindilla (cada vez más)
        cmp byte [oleada], 2
        jb .f
        call azar
        mov ah, 3
        cmp byte [oleada], 5
        jb .g
        mov ah, 1
.g:     test al, ah
        jnz .f
        mov byte [de_t + si], 2
        ret

; ---- la Bombona ----
p_bomb: cmp byte [bomb_act], 0
        jne .vuela
        cmp byte [n_vivos], 6
        jb .f
        dec word [bomb_t]
        jnz .f
        mov byte [bomb_act], 1
        mov word [bomb_x], 4
        mov word [bomb_dir], 1
        call azar
        test al, 1
        jz .f
        mov word [bomb_x], 300
        mov word [bomb_dir], -1
.f:     ret
.vuela: mov bx, [bomb_x]
        add bx, [bomb_dir]
        cmp bx, 4
        jl .fuera
        cmp bx, 300
        jg .fuera
        mov [bomb_x], bx
        mov ax, K_BOMB
        mov dx, BOMB_Y
        call pinta
        test byte [ticks], 15       ; y su runrún
        jnz .r
        mov si, ef_bomb
        jmp suena_libre
.r:     ret
.fuera: call quita_bomb
        ret

quita_bomb:
        mov ax, (2 << 8) | K_BOMB
        mov bx, [bomb_x]
        mov dx, BOMB_Y
        call pinta
        mov byte [bomb_act], 0
        call azar
        and ax, 0x1FF
        add ax, 900
        mov [bomb_t], ax
        ret

mata_bomb:
        push bx
        push dx
        call quita_bomb
        mov bx, [bomb_x]
        add bx, 4
        mov dx, BOMB_Y
        mov al, K_EXPLO
        call explota
        call azar
        and al, 3
        cbw
        mov si, ax
        shl si, 1
        mov ax, [puntos_bomb + si]
        call suma
        mov si, ef_explo
        call suena
        cmp byte [prem_t], 0        ; y suelta un premio
        jne .f
        mov al, [prem_sig]
        inc al
        mov [prem_t], al
        xor byte [prem_sig], 1
        mov [prem_x], bx
        mov word [prem_y], BOMB_Y + 8
.f:     pop dx
        pop bx
        ret

; ---- el premio, cayendo ----
p_premio:
        mov al, [prem_t]
        or al, al
        jz .f
        add al, K_PREMIO
        mov ah, 2
        mov bx, [prem_x]
        mov dx, [prem_y]
        call pinta
        call repinta_def_bajo
        inc dx
        mov [prem_y], dx
        cmp dx, JY + 8
        jae .fuera
        cmp byte [jm], 0            ; ¿lo coge la churrera?
        jne .p
        cmp dx, JY - 7
        jb .p
        mov cx, bx
        sub cx, [jx]
        add cx, 7
        cmp cx, 16 + 7
        jae .p
        cmp byte [prem_t], 1
        jne .churros
        cmp byte [vidas], 6
        jae .coge
        inc byte [vidas]
        mov byte [hud_sucio], 1
        jmp short .coge
.churros:
        mov word [doble], 900       ; quince segundos de tiro doble
.coge:  mov byte [prem_t], 0
        mov si, ef_premio
        call suena
        mov ax, K_JUG               ; (por si el premio le ha borrado algo)
        mov bx, [jx]
        mov dx, JY
        jmp pinta
.p:     mov al, [prem_t]
        add al, K_PREMIO
        mov ah, 1
        jmp pinta
.fuera: mov byte [prem_t], 0
.f:     ret

; si algo de 8 de ancho en BX, DX acaba de borrarse encima de una muralla,
; se repinta la muralla
repinta_def_bajo:
        push bx
        push dx
        mov ax, dx
        add ax, 8
        cmp ax, DEF_Y
        jbe .f
        cmp dx, DEF_Y + 12
        jae .f
        mov dx, DEF_Y
        call def_pos
        jc .p
        add bx, 7
        call def_pos
        jnc .f
.p:     mov al, ah
        call pinta_def
.f:     pop dx
        pop bx
        ret

; ---- el Abanico que se suelta ----
p_buzo: cmp byte [buzo_act], 0
        jne .baja
        cmp byte [oleada], 3
        jb .f
        cmp byte [jm], 0
        jne .f
        cmp byte [n_vivos], 4
        jb .f
        dec word [buzo_t]
        jnz .f
        call nuevo_buzo_t
        call azar                   ; uno de abajo de una columna al azar
        and al, 7
        add al, 4 * COLS
        mov ah, 0
        mov bp, ax
.b:     cmp byte [vivos + bp], 0
        jne .suelta
        sub bp, COLS
        jge .b
.f:     ret
.suelta:
        call celda
        mov ah, 2
        call pinta
        mov byte [vivos + bp], 0
        dec byte [n_vivos]
        mov [buzo_x], bx
        mov [buzo_y], dx
        mov byte [buzo_act], 1
        mov word [buzo_dx], 1
        call azar
        test al, 1
        jz .r
        mov word [buzo_dx], -1
.r:     ret
.baja:  call k_buzo
        mov ah, 2
        mov bx, [buzo_x]
        mov dx, [buzo_y]
        call pinta
        inc dx
        add bx, [buzo_dx]
        cmp bx, 4
        jl .rebota
        cmp bx, 300
        jle .m
.rebota:
        neg word [buzo_dx]
        add bx, [buzo_dx]
        add bx, [buzo_dx]
.m:     mov [buzo_x], bx
        mov [buzo_y], dx
        cmp dx, JY + 8
        jae .fuera
        mov ax, dx                  ; ¿se come una muralla?
        add ax, 8
        cmp ax, DEF_Y
        jbe .j
        cmp dx, DEF_Y + 12
        jae .j
        call raspa
.j:     cmp byte [jm], 0            ; ¿le da a la churrera?
        jne .p
        cmp dx, JY - 7
        jb .p
        mov ax, bx
        sub ax, [jx]
        add ax, 7
        cmp ax, 16 + 7
        jae .p
        mov byte [buzo_act], 0
        mov al, K_EXPLO
        call explota
        jmp muere
.p:     call k_buzo
        mov ah, 1
        jmp pinta
.fuera: mov byte [buzo_act], 0
        ret

k_buzo: mov al, [buzo_y]            ; el Abanico, aleteando
        shr al, 1
        shr al, 1
        shr al, 1
        and al, 1
        add al, 4
        ret

nuevo_buzo_t:
        call azar
        and ax, 0xFF
        add ax, 300
        mov [buzo_t], ax
        ret

mata_buzo:
        push bx
        push dx
        call k_buzo
        mov ah, 2
        mov bx, [buzo_x]
        mov dx, [buzo_y]
        call pinta
        mov byte [buzo_act], 0
        call repinta_def_bajo
        mov al, K_EXPLO
        call explota
        mov ax, 50
        call suma
        mov si, ef_explo
        call suena
        pop dx
        pop bx
        ret

; ---- los estallidos: AL = dibujo en BX, DX, que dura 12 tics ----
explota:
        push si
        push ax
        xor si, si
.b:     cmp byte [ex_t + si], 0
        je .h
        inc si
        cmp si, 4
        jb .b
        push bx                     ; no hay sitio: el primero se acaba ya
        push dx
        push di
        mov byte [ex_t], 1
        call p_explos
        pop di
        pop dx
        pop bx
        xor si, si
.h:     pop ax
        mov byte [ex_t + si], 12
        mov [ex_k + si], al
        shl si, 1
        mov [ex_x + si], bx
        mov [ex_y + si], dx
        mov ah, 0
        call pinta
        pop si
        ret

p_explos:
        xor si, si
.b:     cmp byte [ex_t + si], 0
        je .n
        dec byte [ex_t + si]
        jnz .n
        mov al, [ex_k + si]
        mov ah, 2
        mov di, si
        shl di, 1
        mov bx, [ex_x + di]
        mov dx, [ex_y + di]
        call pinta
.n:     inc si
        cmp si, 4
        jb .b
        ret

%ifdef PRUEBA
; =============================================================================
;  La prueba del motor (MARPRUEB.COM): pinta en memoria, no en la pantalla
; =============================================================================
prueba: mov byte [sin_hud], 1
        mov ax, cs
        add ax, 0x1000
        mov [vseg], ax
        mov es, ax
        mov byte [def_mask], 0x55
        mov word [semilla], 1
        call ruta_record
        mov si, t_p0
        call di_txt
        mov si, t_p1
        call di_txt
        ; 1. un churro debajo del Abanico de la columna 2: lo mata
        call nueva_partida
        mov bp, 4 * COLS + 2
        call celda
        add bx, 4
        add dx, 20
        call tira_aqui
        mov cx, 10
.c1:    push cx
        call p_disparos
        pop cx
        loop .c1
        cmp byte [vivos + 4 * COLS + 2], 0
        jne .m1
        cmp word [puntos], 10
        jne .m1
        cmp word [dp_y], 0
        jne .m1
        cmp byte [n_vivos], N_BICHOS - 1
        jne .m1
        call bien
        jmp short .p2
.m1:    call mal
        ; 2. por el hueco entre dos columnas pasa sin tocar a nadie
.p2:    mov si, t_p2
        call di_txt
        call nueva_partida
        mov bp, 4 * COLS + 2
        call celda
        add bx, 9
        add dx, 20
        call tira_aqui
        mov cx, 60
.c2:    push cx
        call p_disparos
        pop cx
        loop .c2
        cmp byte [n_vivos], N_BICHOS
        jne .m2
        cmp word [dp_y], 0
        jne .m2
        cmp word [puntos], 0
        jne .m2
        call bien
        jmp short .p3
.m2:    call mal
        ; 3. un churro desde abajo contra la primera muralla: se para y la muerde
.p3:    mov si, t_p3
        call di_txt
        call nueva_partida
        call cuenta_muro
        mov [p_antes], cx
        mov bx, DEF_X + 5
        mov dx, JY - 6
        call tira_aqui
        mov cx, 30
.c3:    push cx
        call p_disparos
        pop cx
        loop .c3
        cmp word [dp_y], 0
        jne .m3
        call cuenta_muro
        cmp cx, [p_antes]
        jae .m3
        mov bx, DEF_X + 5           ; el punto de abajo del todo, comido
        mov dx, DEF_Y + 11
        call def_pos
        test [si], al
        jnz .m3
        mov bx, DEF_X + 5           ; y el de arriba, en su sitio
        mov dx, DEF_Y + 2
        call def_pos
        test [si], al
        jz .m3
        call bien
        jmp short .p4
.m3:    call mal
        ; 4. una aceituna desde arriba contra la muralla
.p4:    mov si, t_p4
        call di_txt
        call nueva_partida
        call cuenta_muro
        mov [p_antes], cx
        mov byte [de_t], 1
        mov word [de_x], DEF_X + 8
        mov word [de_y], DEF_Y - 20
        mov cx, 40
.c4:    push cx
        call p_balas
        pop cx
        loop .c4
        cmp byte [de_t], 0
        jne .m4
        call cuenta_muro
        cmp cx, [p_antes]
        jae .m4
        mov bx, DEF_X + 8
        mov dx, DEF_Y
        call def_pos
        test [si], al
        jnz .m4
        call bien
        jmp short .p5
.m4:    call mal
        ; 5. en la oleada 2 la Boina lleva casco: dos churros
.p5:    mov si, t_p5
        call di_txt
        call nueva_partida
        mov byte [oleada], 2
        call nueva_oleada
        mov bp, 3
        call celda
        add bx, 4
        add dx, 12
        push bx
        push dx
        call tira_aqui
        mov cx, 5
.c5:    push cx
        call p_disparos
        pop cx
        loop .c5
        pop dx
        pop bx
        cmp byte [vivos + 3], 1
        jne .m5
        cmp byte [n_vivos], N_BICHOS
        jne .m5
        call tira_aqui
        mov cx, 5
.c6:    push cx
        call p_disparos
        pop cx
        loop .c6
        cmp byte [vivos + 3], 0
        jne .m5
        cmp word [puntos], 5 + 30
        jne .m5
        call bien
        jmp short .p6
.m5:    call mal
        ; 6. una guindilla que cae en la churrera: una vida menos
.p6:    mov si, t_p6
        call di_txt
        call nueva_partida
        mov word [jx], 72
        mov byte [de_t], 2
        mov word [de_x], 80
        mov word [de_y], JY - 20
        mov cx, 20
.c7:    push cx
        call p_balas
        pop cx
        loop .c7
        cmp byte [vidas], 2
        jne .m6
        cmp byte [jm], 0
        je .m6
        cmp byte [de_t], 0
        jne .m6
        call bien
        jmp short .p7
.m6:    call mal
        ; 7. la flota llega abajo: se acaba la partida
.p7:    mov si, t_p7
        call di_txt
        call nueva_partida
        mov word [fy], JY - 4 * 16 - 8 - 4
        mov byte [fx], 77 - 7 * 3   ; (en el borde: le toca bajar)
        mov byte [fcuenta], 1
        call p_flota
        cmp byte [vidas], 0
        jne .m7
        call bien
        jmp short .p8
.m7:    call mal
        ; 8. el récord: se escribe y se vuelve a leer
.p8:    mov si, t_p8
        call di_txt
        mov word [record], 12345
        call guarda_record
        mov word [record], 0
        call lee_record
        cmp word [record], 12345
        jne .m8
        call bien
        jmp short .fin
.m8:    call mal
.fin:   mov si, t_pfin
        call di_txt
        mov al, [fallos]
        mov ah, 0x4C
        int 0x21

tira_aqui:
        mov [dp_x], bx
        mov [dp_y], dx
        ret
; CX = los puntos que tiene la primera muralla
cuenta_muro:
        mov si, defensas
        xor cx, cx
.o:     lodsb
.b:     shl al, 1
        adc cx, 0
        or al, al
        jnz .b
        cmp si, defensas + 36
        jb .o
        ret
bien:   mov si, t_bien
        jmp di_txt
mal:    inc byte [fallos]
        mov si, t_mal
di_txt: mov dx, si
        mov ah, 9
        int 0x21
        ret
t_p0    db 'MARCIANADA: prueba del motor', 13, 10, '$'
t_p1    db 'churro contra bicho: $'
t_p2    db 'hueco entre columnas: $'
t_p3    db 'churro contra muralla: $'
t_p4    db 'aceituna contra muralla: $'
t_p5    db 'casco, dos tiros: $'
t_p6    db 'guindilla a la churrera: $'
t_p7    db 'la flota llega al suelo: $'
t_p8    db 'record en el disco: $'
t_bien  db 'BIEN', 13, 10, '$'
t_mal   db 'MAL', 13, 10, '$'
t_pfin  db 'MARCIANADA: fin de la prueba', 13, 10, '$'
%endif

; =============================================================================
;  Datos
; =============================================================================
t_rec   db 'MARCIANA.REC', 0
t_si    db 'SI', 0
t_no    db 'NO', 0
t_pausa db 'PAUSA: P PARA SEGUIR                    ', 0
t_fin   db 'FIN DE LA PARTIDA', 0
t_nuevo db 'NUEVO RECORD', 0

l_marcador:
        db 0, 0, 'PUNTOS', 0
        db 0, 14, 'RECORD', 0
        db 0, 28, 'OLEADA', 0
        db 24, 0, 'VIDAS', 0
        db 24, 10, 'SONIDO', 0
        db 24, 22, 'P PAUSA  ESC SALIR', 0
        db 0xFF, 0xFF

l_titulo:
        db 1, 15, 'RECORD', 0
        db 3, 10, 'M A R C I A N A D A', 0
        db 5, 5, 'QUE VIENEN LOS BOTIJOS VOLADORES', 0
        db 8, 12, 'BOINA ........ 30', 0
        db 10, 12, 'BOTIJO ....... 20', 0
        db 12, 12, 'ABANICO ...... 10', 0
        db 14, 12, 'CASCO: DOS TIROS', 0
        db 16, 12, 'BOMBONA .. 50-300', 0
        db 18, 12, 'CORAZON: UNA VIDA', 0
        db 20, 12, 'CHURROS: TIRO DOBLE', 0
        db 22, 0, 'FLECHAS O Z X MUEVEN  ESPACIO DISPARA', 0
        db 23, 3, 'P PAUSA  S SONIDO  ESC SALIR', 0
        db 24, 1, '(C) 2026 CARLOS SANZ Y CONTRIBUIDORES', 0
        db 0xFF, 0xFF

leyenda:
        db 0, 8, 2, 10, 4, 12, 6, 14, K_BOMB, 16, K_PREMIO + 1, 18, K_PREMIO + 2, 20
        db 0xFF, 0xFF

puntos_bicho db 30, 20, 10, 0
puntos_bomb  dw 50, 100, 150, 300

; los efectos: dw tono (divisor del 8254), dw cambio por tic, db tics, db ruido
ef_tiro   dw 1400, 60
          db 6, 0
ef_explo  dw 1200, 40
          db 14, 1
ef_muerte dw 2500, 30
          db 70, 1
ef_tic    dw 200, 0
          db 1, 1
ef_muro   dw 3000, 0
          db 3, 1
ef_casco  dw 700, 120
          db 5, 0
ef_bomb   dw 900, 20
          db 8, 0
ef_premio dw 2400, -50
          db 30, 0

; al empezar cada oleada: una escala andaluza (frigia dominante) en mi
%define MI4  3620
%define FA4  3417
%define SOL4 2873
%define LA4  2712
%define SI4  2416
%define DO5  2280
m_oleada:
        db 7
        dw MI4
        db 7
        dw FA4
        db 7
        dw SOL4
        db 7
        dw LA4
        db 14
        dw SI4
        db 7
        dw DO5
        db 7
        dw SI4
        db 7
        dw LA4
        db 7
        dw SOL4
        db 7
        dw FA4
        db 21
        dw MI4
        db 0

; medio octeto (4 puntos de 1 bit) a un octeto de la CGA (4 puntos de 2 bits)
medios: db 0x00, 0x03, 0x0C, 0x0F, 0x30, 0x33, 0x3C, 0x3F
        db 0xC0, 0xC3, 0xCC, 0xCF, 0xF0, 0xF3, 0xFC, 0xFF

; ---- los dibujos: filas de puntos, «.» el fondo y 1, 2 o 3 el color ----
%macro F 1
 %strlen %%n %1
 %assign %%i 1
 %rep %%n / 4
  %assign %%b 0
  %rep 4
   %substr %%c %1 %%i
   %if %%c == '.'
    %assign %%v 0
   %else
    %assign %%v %%c - '0'
   %endif
   %assign %%b (%%b << 2) | %%v
   %assign %%i %%i + 1
  %endrep
  db %%b
 %endrep
%endmacro
; y las murallas, de un bit por punto
%macro M 1
 %strlen %%n %1
 %assign %%i 1
 %rep %%n / 8
  %assign %%b 0
  %rep 8
   %substr %%c %1 %%i
   %assign %%b (%%b << 1) | (%%c == '#')
   %assign %%i %%i + 1
  %endrep
  db %%b
 %endrep
%endmacro

; cada dibujo: dw sus puntos, db ancho (octetos), alto, letras en modo
; texto, su atributo y las letras
%macro TIPO 5-9 0, 0, 0, 0
        dw %1
        db %2, %3, %4, %5, %6, %7, %8, %9
%endmacro
tipos:  TIPO d_boina0,  2, 8, 2, 0x07, 0xCD, 0xCD
        TIPO d_boina1,  2, 8, 2, 0x07, 0xCD, 0xCD
        TIPO d_botijo0, 2, 8, 2, 0x07, 0x93, ')'
        TIPO d_botijo1, 2, 8, 2, 0x07, 0x93, ')'
        TIPO d_abanico0, 2, 8, 2, 0x07, '\', '/'
        TIPO d_abanico1, 2, 8, 2, 0x07, '/', '\'
        TIPO d_casco0,  2, 8, 2, 0x0F, 0xC9, 0xBB
        TIPO d_casco1,  2, 8, 2, 0x0F, 0xC9, 0xBB
        TIPO d_explo,   2, 8, 2, 0x0F, '*', '*'
        TIPO d_jug,     4, 8, 4, 0x0F, 0xDC, 0xDB, 0xDB, 0xDC
        TIPO d_roto0,   4, 8, 4, 0x07, 0xB1, 0xB0, 0xB1, 0xB0
        TIPO d_roto1,   4, 8, 4, 0x07, 0xB0, 0xB1, 0xB0, 0xB1
        TIPO d_bomb,    4, 8, 4, 0x70, '(', 0xB2, 0xB2, ')'
        TIPO d_corazon, 2, 8, 2, 0x8F, 0x03, 0x03
        TIPO d_churros, 2, 8, 2, 0x8F, 0xBA, 0xBA
        TIPO d_churro,  1, 6, 1, 0x0F, 0xB3
        TIPO d_aceituna, 1, 4, 1, 0x07, 0x07
        TIPO d_guindilla, 1, 6, 1, 0x07, 0x15

dibujos:
d_boina0:
        F '...22...'
        F '.222222.'
        F '22222222'
        F '.3.33.3.'
        F '.333333.'
        F '..3..3..'
        F '.3....3.'
        F '3......3'
d_boina1:
        F '...22...'
        F '.222222.'
        F '22222222'
        F '.3.33.3.'
        F '.333333.'
        F '..3..3..'
        F '..3..3..'
        F '.33..33.'
d_botijo0:
        F '..1111..'
        F '..1..1.1'
        F '.111111.'
        F '11311311'
        F '11111111'
        F '11111111'
        F '.111111.'
        F '..1..1..'
d_botijo1:
        F '..1111..'
        F '..1..1.1'
        F '.111111.'
        F '11311311'
        F '11111111'
        F '11111111'
        F '.111111.'
        F '.1....1.'
d_abanico0:
        F '.333333.'
        F '3.3333.3'
        F '33.33.33'
        F '.3.33.3.'
        F '..3333..'
        F '...33...'
        F '...33...'
        F '..3..3..'
d_abanico1:
        F '..3333..'
        F '.3.33.3.'
        F '.33..33.'
        F '..3333..'
        F '..3333..'
        F '...33...'
        F '...33...'
        F '.3....3.'
d_casco0:
        F '..3333..'
        F '.333333.'
        F '33333333'
        F '32222223'
        F '.2.22.2.'
        F '.222222.'
        F '.2....2.'
        F '2......2'
d_casco1:
        F '..3333..'
        F '.333333.'
        F '33333333'
        F '32222223'
        F '.2.22.2.'
        F '.222222.'
        F '..2..2..'
        F '.22..22.'
d_explo:
        F '3..3..3.'
        F '.3.3.3..'
        F '..333...'
        F '3333333.'
        F '..333...'
        F '.3.3.3..'
        F '3..3..3.'
        F '........'
d_jug:
        F '.......33.......'
        F '......3333......'
        F '......3333......'
        F '.33333333333333.'
        F '3333333333333333'
        F '3333333333333333'
        F '.333........333.'
        F '..3..........3..'
d_roto0:
        F '...3......3.....'
        F '.3....3.......3.'
        F '....3...33..3...'
        F '..3..333..3.....'
        F '.3.33333333.3.3.'
        F '3333.33.33333.33'
        F '.333.3....3.333.'
        F '..3..........3..'
d_roto1:
        F '.3.....3....3...'
        F '....3.....3...3.'
        F '..3...33..3.....'
        F '....3333.3...3..'
        F '.333.33333333.3.'
        F '33.333333.333333'
        F '.333..3.3...333.'
        F '..3..........3..'
d_bomb:
        F '......3333......'
        F '....22222222....'
        F '..222222222222..'
        F '.22333333333322.'
        F '.22222222222222.'
        F '..222222222222..'
        F '...2222222222...'
        F '....33....33....'
d_corazon:
        F '.22..22.'
        F '22222222'
        F '22222222'
        F '22222222'
        F '.222222.'
        F '..2222..'
        F '...22...'
        F '........'
d_churros:
        F '.3...3..'
        F '.33..33.'
        F '.3...3..'
        F '.33..33.'
        F '.3...3..'
        F '.33..33.'
        F '.3...3..'
        F '........'
d_churro:
        F '.3..'
        F '.3..'
        F '.3..'
        F '.3..'
        F '.3..'
        F '.3..'
d_aceituna:
        F '.2..'
        F '222.'
        F '222.'
        F '.2..'
d_guindilla:
        F '2...'
        F '.2..'
        F '..2.'
        F '.2..'
        F '2...'
        F '.2..'
dibujos_fin:

muralla:
        M '.####..####..####..####.'
        M '.####..####..####..####.'
        M '.######################.'
        M '########################'
        M '########################'
        M '########################'
        M '########################'
        M '########################'
        M '#########......#########'
        M '########........########'
        M '########........########'
        M '########........########'

; =============================================================================
;  Variables (no ocupan sitio en el archivo)
; =============================================================================
        section .bss
bss_ini:
viejo8      resd 1
viejo9      resd 1
ticks       resw 1
hechos      resw 1
acum        resw 1
teclas      resb 128
ultima      resb 1
s_div       resw 1                  ; el efecto que suena (en este orden)
s_del       resw 1
s_t         resb 1
s_ru        resb 1
m_ptr       resw 1
rs          resw 1
sonido_si   resb 1
semilla     resw 1
modo_ant    resb 1
modo_txt    resb 1                  ; 0 gráficos, 1 texto pedido (/T), 2 MDA o Hercules
mono        resb 1
vseg        resw 1
col_base    resb 1
color_hud   resb 1
def_mask    resb 1
suelo_pat   resb 1
sin_hud     resb 1
hud_sucio   resb 1
hay_pal     resb 1
pal_centro  resw 1
pal_izq     resb 1                  ; (estas dos, juntas)
pal_der     resb 1
pal_fue     resb 1
p_modo      resb 1
p_w         resb 1
p_h         resb 1
modo_flota  resb 1
p_fondo     resw 1
fila_baja   resb 1
e_bits      resw 1
cifras      resb 6
ruta        resb 80
espera      resw 1
pausa       resb 1
estado      resb 1
puntos      resw 1
record      resw 1
vidas       resb 1
oleada      resb 1
jx          resw 1
jm          resb 1
dcool       resb 1
doble       resw 1
vivos       resb N_BICHOS
n_vivos     resb 1
fx          resb 1
fdir        resb 1
fframe      resb 1
fcuenta     resb 1
fcool       resb 1
fy          resw 1
defensas    resb 4 * 36
prem_sig    resb 1
limpia_ini:                         ; lo que se pone a cero en cada oleada
dp_x        resw 2
dp_y        resw 2
de_t        resb 6
de_x        resw 6
de_y        resw 6
bomb_act    resb 1
bomb_x      resw 1
bomb_dir    resw 1
bomb_t      resw 1
buzo_act    resb 1
buzo_x      resw 1
buzo_y      resw 1
buzo_dx     resw 1
buzo_t      resw 1
prem_t      resb 1
prem_x      resw 1
prem_y      resw 1
ex_t        resb 4
ex_k        resb 4
ex_x        resw 4
ex_y        resw 4
limpia_fin:
%ifdef PRUEBA
p_antes     resw 1
fallos      resb 1
%endif
bss_fin:
            resb 512
pila_fin:
