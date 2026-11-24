; =============================================================================
;  DERRIBO.COM - Derribo: pala, pelota y ladrillos, para Hispa-8086
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Un juego nuestro del género de «romper muros»: con la pala se devuelve la
;  pelota y se derriban los ladrillos de diez muros propios. Hay ladrillos de
;  uno, dos y tres golpes, y otros que no se rompen; de algunos cae un premio
;  (pala ancha, pelota lenta, tres pelotas o una vida más).
;
;  Nada se distingue solo por el color (Carlos es daltónico, y en la época
;  muchos jugaban con monitor de fósforo verde o ámbar): cada ladrillo tiene
;  su forma (lleno, hueco con un 2, lleno con un 3 calado, a cuadros) y cada
;  premio lleva su letra (A, L, T, V).
;
;    DERRIBO        en CGA, EGA o VGA: gráficos de 320×200 a 4 colores; con
;                   MDA o Hercules, en modo de texto (se ve solo)
;    DERRIBO /M     monocromo: todo en un solo color (y el modo 5 de la
;                   CGA, sin la señal de color en la salida compuesta)
;    DERRIBO /T     en modo de texto aunque haya tarjeta gráfica
;    DERRIBO /P     prueba del motor, sin pantalla: juega los diez muros
;                   solo, dos veces, y dice si alguna pelota ha atravesado
;                   un ladrillo, una pared o la pala
;
;  Teclas: flechas o Z y X, la pala; Espacio, lanzar; P, pausa; S, sonido;
;  M (en la portada), monocromo; Esc, acabar. También el ratón (INT 33h) y
;  la palanca de juegos (puerto 201h).
;
;  La velocidad es la misma en cualquier máquina: el temporizador 8253/8254
;  se pone a 72,8 veces por segundo (cuatro veces lo normal, y a la BIOS se
;  le sigue llamando 18,2 veces, para que el reloj no se adelante) y el juego
;  avanza un paso por cada tic; si una máquina lenta no llega a pintar todos,
;  se dan los pasos que faltan antes de pintar. La pelota se mueve como mucho
;  un punto cada vez y se miran por separado los dos ejes: no puede
;  atravesar un ladrillo por muy deprisa que vaya.
;
;  La mejor puntuación se guarda en DERRIBO.REC (texto), en el directorio
;  actual. Solo instrucciones del 8086.
; =============================================================================
        cpu 8086
        org 0x100

NC      equ 13                  ; columnas de ladrillos
NR      equ 10                  ; filas como mucho
NCEL    equ NC * NR
XL      equ 4                   ; el campo: de x = 4 a 315
XR      equ 316
YT      equ 16                  ; y desde 16 (encima, la pared de arriba)
LY0     equ 32                  ; la primera fila de ladrillos
PY      equ 184                 ; la pala: y de 184 a 187
PH      equ 4
YFUERA  equ 188                 ; una pelota que llega aquí se ha perdido
CAPFIN  equ 185                 ; y un premio
VBASE   equ 24                  ; velocidades, en 1/16 de punto por tic
VMAX    equ 56
VLENTA  equ 16
VPRUEBA equ 80                  ; (la prueba va aún más deprisa que el juego)
VPALA   equ 4
ANCHO1  equ 32
ANCHO2  equ 48
NBOLAS  equ 3
BOLA_T      equ 16                  ; lo que ocupa cada pelota en «bolas»
NNIV    equ 10
DIVISOR equ 16384               ; 1193182 / 16384 = 72,8 tics por segundo
TICS_S  equ 73
TICS_PRUEBA equ 50000           ; tics como mucho para cada muro en la prueba

; cada pelota (en «bolas»)
B_X     equ 0                   ; posición, en 1/16 de punto
B_Y     equ 2
B_ANG   equ 4                   ; ángulo, 0 (casi vertical) a 7 (tendida)
B_SX    equ 5                   ; sentido: 1 o -1
B_SY    equ 6
B_EST   equ 7                   ; 0 no está, 1 en juego, 2 pegada a la pala
B_REB   equ 8                   ; rebotes sin romper nada
B_VIS   equ 9                   ; está pintada
B_DX    equ 10                  ; dónde se pintó (puntos, o columna y fila)
B_DY    equ 12

; teclas (códigos del teclado)
T_ESC   equ 0x01
T_ESP   equ 0x39
T_INTRO equ 0x1C
T_P     equ 0x19
T_S     equ 0x1F
T_M     equ 0x32

%macro FILA_SIG 0               ; di: a la línea siguiente de la CGA
        xor di, 0x2000
        test di, 0x2000
        jnz %%r
        add di, 80
%%r:
%endmacro

%macro SHR4 1
        mov cl, 4
        shr %1, cl
%endmacro

; =============================================================================
inicio:
        cld
        mov di, bss_ini
        mov cx, bss_fin - bss_ini
        xor al, al
        rep stosb
        mov si, 0x81
.par:   lodsb
        cmp al, 13
        je .fpar
        cmp al, '/'
        jne .par
        lodsb
        cmp al, 13
        je .fpar
        and al, 0xDF
        cmp al, 'P'
        jne .p1
        jmp prueba
.p1:    cmp al, 'M'
        jne .p2
        mov byte [mono], 1
.p2:    cmp al, 'T'
        jne .par
        mov byte [modo], 1
        jmp .par
.fpar:
        int 0x11                ; ¿MDA o Hercules? Entonces, en texto
        and al, 0x30
        cmp al, 0x30
        jne .color
        mov byte [modo], 1
        mov byte [mtexto], 7
        mov word [vseg], 0xB000
.color:
        cmp byte [modo], 1
        jne .graf
        mov word [attr], 0x07 + 80 * 256    ; attr y ancho_txt
        mov byte [base], 20
.graf:
        mov ah, 0x0F
        int 0x10
        mov [modo_orig], al
        call lee_record
        push es
        mov ax, 0x40
        mov es, ax
        mov ax, [es:0x6C]
        mov [semilla], ax
        ; el ratón, si hay controlador
        mov ax, 0x3533
        int 0x21
        mov ax, es
        or ax, bx
        jz .sinraton
        cmp byte [es:bx], 0xCF
        je .sinraton
        xor ax, ax
        int 0x33
        or ax, ax
        jz .sinraton
        mov byte [raton], 1
.sinraton:
        pop es
        ; la palanca: cuánto tarda en el centro
        call lee_palanca
        cmp cx, 2
        jb .sinpal
        cmp cx, 0xFFF0
        jae .sinpal
        in al, dx
        and al, 0x30
        cmp al, 0x30
        jne .sinpal
        mov [centro], cx
        mov byte [palanca], 1
.sinpal:
        ; los vectores de antes
        mov al, 0x08
        mov di, viejo8
        call guarda_vec
        mov al, 0x09
        mov di, viejo9
        call guarda_vec
        mov al, 0x24
        mov di, viejo24
        call guarda_vec
        mov al, 0x1F
        mov di, viejo1f
        call guarda_vec
        ; en una CGA, la BIOS no tiene letras por encima de la 127 en los
        ; modos gráficos: se le hacen las acentuadas que se usan
        cmp byte [modo], 0
        jne .ya1f
        mov ax, [viejo1f]
        or ax, [viejo1f + 2]
        jnz .ya1f
        call acentos_cga
.ya1f:
        mov ax, 0x2508
        mov dx, int8
        int 0x21
        mov ax, 0x2509
        mov dx, int9
        int 0x21
        mov ax, 0x2524
        mov dx, int24
        int 0x21
        cli
        mov al, 0x36
        out 0x43, al
        mov ax, DIVISOR
        out 0x40, al
        mov al, ah
        out 0x40, al
        sti
        mov es, [vseg]
        call hace_imagenes
        jmp titulo

salir:
        cli
        mov al, 0x36
        out 0x43, al
        xor al, al
        out 0x40, al
        out 0x40, al
        sti
        call calla
        mov al, 0x08
        mov si, viejo8
        call pon_vec
        mov al, 0x09
        mov si, viejo9
        call pon_vec
        mov al, 0x24
        mov si, viejo24
        call pon_vec
        mov al, 0x1F
        mov si, viejo1f
        call pon_vec
        ; las mayúsculas y demás que la BIOS creyera pulsadas, y su búfer
        mov ax, 0x40
        mov es, ax
        and byte [es:0x17], 0xF0
        mov ax, [es:0x1A]
        mov [es:0x1C], ax
        mov al, [modo_orig]
        xor ah, ah
        int 0x10
        mov ax, 0x4C00
        int 0x21

guarda_vec:                     ; al = interrupción, di = dónde
        mov ah, 0x35
        push es
        int 0x21
        mov [di], bx
        mov [di + 2], es
        pop es
        ret

pon_vec:                        ; al = interrupción, si = el vector guardado
        mov ah, 0x25
        push ds
        mov dx, [si]
        mov ds, [si + 2]
        int 0x21
        pop ds
        ret

; =============================================================================
;  Interrupciones: el reloj (y el sonido), el teclado y los errores de disco
; =============================================================================
int8:
        push ax
        inc word [cs:tics]
        cmp byte [cs:son_t], 0
        je .cadena
        dec byte [cs:son_t]
        jnz .cadena
        push si
        mov si, [cs:son_p]
        mov ax, [cs:si]
        or ax, ax
        jz .calla
        push ax
        mov al, 0xB6
        out 0x43, al
        pop ax
        out 0x42, al
        mov al, ah
        out 0x42, al
        in al, 0x61
        or al, 3
        out 0x61, al
        mov al, [cs:si + 2]
        mov [cs:son_t], al
        add si, 3
        mov [cs:son_p], si
        pop si
        jmp .cadena
.calla: in al, 0x61
        and al, 0xFC
        out 0x61, al
        pop si
.cadena:
        add word [cs:acum], DIVISOR     ; uno de cada cuatro, a la BIOS
        jc .bios
        mov al, 0x20
        out 0x20, al
        pop ax
        iret
.bios:  pop ax
        jmp far [cs:viejo8]

int9:
        push ax
        push bx
        in al, 0x60
        mov ah, al
        in al, 0x61             ; el acuse de recibo del XT
        or al, 0x80
        out 0x61, al
        and al, 0x7F
        out 0x61, al
        mov al, 0x20
        out 0x20, al
        cmp ah, 0xE0
        je .fin
        mov al, ah
        and al, 0x7F
        mov bl, 1
        cmp al, 0x4B            ; flecha izquierda (y el 4 del teclado numérico)
        je .pala
        cmp al, 0x2C            ; Z
        je .pala
        mov bl, 2
        cmp al, 0x4D            ; flecha derecha
        je .pala
        cmp al, 0x2D            ; X
        je .pala
        test ah, 0x80
        jnz .fin
        mov [cs:ult], al
        jmp .fin
.pala:  test ah, 0x80
        jnz .suelta
        or [cs:teclas], bl
        jmp .fin
.suelta:
        not bl
        and [cs:teclas], bl
.fin:   pop bx
        pop ax
        iret

int24:  mov al, 3               ; error de disco: que la llamada falle
        iret

; =============================================================================
;  La portada
; =============================================================================
titulo:
        call pon_modo
        mov si, tit_textos
.t:     lodsb
        cmp al, 0xFF
        je .leyenda
        mov dh, al
        lodsb
        mov dl, al
        call pon_texto
        jmp .t
.leyenda:
        mov bx, 3 * NC + 1      ; los cuatro ladrillos, en las filas 3, 5, 7 y 9
        mov al, 1
.l:     push ax
        call pinta_celda
        pop ax
        add bx, 2 * NC
        inc al
        cmp al, 5
        jb .l
        xor cx, cx              ; y los cuatro premios
.c:     push cx
        mov ax, cx
        mov ah, 16
        mul ah
        add ax, 56
        cmp byte [modo], 0
        jne .ctxt
        mov bx, 172
        call xor_cap
        jmp .csig
.ctxt:  mov dh, al
        shr dh, 1
        shr dh, 1
        shr dh, 1
        mov dl, 43
        call dir_txt
        mov ah, 0x07
        mov al, '('
        stosw
        mov bx, cx
        mov al, [letras + bx]
        stosw
        mov al, ')'
        stosw
.csig:  pop cx
        inc cx
        cmp cx, 4
        jb .c
        mov ax, [mejor]
        mov di, m_mejor_n
        call fmt6
        mov si, m_mejor
        mov dx, 21 * 256 + 255
        call pon_texto
        mov si, m_copy
        call pon_msg
.k:     hlt
        call boton
        jc .juega
        call tecla
        cmp al, T_ESC
        jne .k1
        jmp salir
.k1:    cmp al, T_ESP
        je .juega
        cmp al, T_INTRO
        je .juega
        cmp al, T_S
        jne .k2
        call alterna_sonido
        mov si, m_consonido
        cmp byte [sonido], 0
        jne .ks
        mov si, m_sinsonido
.ks:    call pon_msg
        jmp .k
.k2:    cmp al, T_M
        jne .k
        cmp byte [modo], 0
        jne .k
        xor byte [mono], 1
        call hace_imagenes
        jmp titulo
.juega: call juego
        jmp titulo

tecla:                          ; al = la última tecla pulsada (0 si ninguna)
        xor al, al
        cli
        xchg al, [ult]
        sti
        ret

; =============================================================================
;  Una partida
; =============================================================================
juego:
        mov word [puntos], 0
        mov byte [vidas], 3
        mov word [nivel], 0             ; nivel y vuelta
.nivel:
        call carga_nivel
        mov al, [vuelta]
        mov cl, 3
        shl al, cl
        add al, [nivel]
        add al, VBASE
        cmp al, VMAX - 8
        jbe .v
        mov al, VMAX - 8
.v:     mov [vbase], al
        call pinta_todo
.vida:
        call nueva_vida
        mov ax, [tics]
        mov [ultimo], ax
.bucle:
        call espera
        call ocultar
        push cx
        call entrada
        pop cx
        jc .fin
.p:     push cx
        call paso
        pop cx
        loop .p
        call mostrar
        call estado_cambios
        cmp word [quedan], 0
        je .superado
        call cuenta_bolas
        jnz .bucle
        mov ax, s_pierde                ; la pelota perdida
        call suena
        dec byte [vidas]
        or byte [cambio], 2
        call estado_cambios
        mov ax, TICS_S
        call retardo
        cmp byte [vidas], 0
        je .fin
        call ocultar
        jmp .vida
.superado:
        mov ax, s_nivel
        call suena
        mov si, m_superado
        call pon_msg
        mov al, [nivel]
        inc al
        mov ah, 10
        mul ah
        add [puntos], ax                ; 100 puntos por el número del muro
        or byte [cambio], 1
        call estado_cambios
        mov ax, 2 * TICS_S
        call retardo
        inc byte [nivel]
        cmp byte [nivel], NNIV
        jb .nivel
        mov byte [nivel], 0
        inc byte [vuelta]
        jmp .nivel
.fin:
        mov si, m_fin
        mov ax, [puntos]
        cmp ax, [mejor]
        jbe .m
        mov [mejor], ax
        call graba_record
        or byte [cambio], 2
        call estado_cambios
        mov si, m_record
.m:     call pon_msg
.w:     hlt
        call boton
        jc .r
        call tecla
        cmp al, T_ESP
        je .r
        cmp al, T_ESC
        je .r
        cmp al, T_INTRO
        jne .w
.r:     ret

espera:                         ; espera un tic; cx = los que han pasado (1 a 8)
.w:     mov ax, [tics]
        sub ax, [ultimo]
        jnz .ok
        hlt
        jmp .w
.ok:    add [ultimo], ax
        cmp ax, 8
        jbe .r
        mov ax, 8
.r:     xchg ax, cx
        ret

retardo:                        ; ax = tics
        mov dx, [tics]
.w:     hlt
        mov cx, [tics]
        sub cx, dx
        cmp cx, ax
        jb .w
        mov byte [ult], 0
        mov ax, [tics]
        mov [ultimo], ax
        ret

cuenta_bolas:                   ; ZF=1 si no queda ninguna
        mov si, bolas
        mov cx, NBOLAS
        xor al, al
.l:     or al, [si + B_EST]
        add si, BOLA_T
        loop .l
        or al, al
        ret

nueva_vida:
        mov word [ancho], ANCHO1
        mov word [padf], 160 - ANCHO1 / 2
        call limita_pala
        mov al, [vbase]
        mov [vel], al
        mov byte [cap_est], 0
        mov si, bolas
        mov cx, NBOLAS
.l:     mov byte [si + B_EST], 0
        add si, BOLA_T
        loop .l
        mov si, bolas
        mov byte [si + B_EST], 2
        mov byte [si + B_REB], 0
        call pega
        mov si, m_lanzar
        jmp pon_msg

entrada:                        ; ratón, palanca y teclas; CF=1: Esc
        cmp byte [raton], 0
        je .j
        mov ax, 0x0B
        int 0x33
        add [padf], cx
.j:     mov byte [joy], 0
        cmp byte [palanca], 0
        je .b
        call lee_palanca
        mov ax, [centro]
        shr ax, 1
        shr ax, 1
        mov dx, [centro]
        sub dx, ax
        cmp cx, dx
        jae .d
        mov byte [joy], 1
        jmp .b
.d:     add dx, ax
        add dx, ax
        cmp cx, dx
        jbe .b
        mov byte [joy], 2
.b:     call boton
        jnc .k
        call lanza
.k:     call tecla
        cmp al, T_ESC
        jne .k1
        stc
        ret
.k1:    cmp al, T_ESP
        jne .k2
        call lanza
        jmp .ok
.k2:    cmp al, T_P
        jne .k3
        call pausa
        jmp .ok
.k3:    cmp al, T_S
        jne .ok
        call alterna_sonido
.ok:    clc
        ret

boton:                          ; CF=1 si se acaba de pulsar un botón
        mov byte [bot], 0
        cmp byte [raton], 0
        je .j
        mov ax, 3
        int 0x33
        and bl, 1
        mov [bot], bl
.j:     cmp byte [palanca], 0
        je .c
        mov dx, 0x201
        in al, dx
        test al, 0x10
        jnz .c
        mov byte [bot], 1
.c:     mov al, [bot]
        xchg al, [bot_ant]
        not al
        and al, [bot_ant]
        shr al, 1
        ret

lee_palanca:                    ; cx = lo que tarda el eje X (dx = 201h)
        mov dx, 0x201
        mov cx, 0xFFFF
        cli
        out dx, al
.b:     in al, dx
        test al, 1
        loopnz .b
        sti
        not cx
        ret

pausa:
        call mostrar
        mov si, m_pausa
        call pon_msg
.w:     hlt
        call tecla
        cmp al, T_P
        je .f
        cmp al, T_ESP
        je .f
        cmp al, T_ESC
        jne .w
.f:     mov si, m_juego
        call pon_msg
        mov ax, [tics]
        mov [ultimo], ax
        xor al, al
        jmp ocultar

alterna_sonido:
        xor byte [sonido], 1
        or byte [cambio], 2
calla:
        cli
        mov byte [son_t], 0
        in al, 0x61
        and al, 0xFC
        out 0x61, al
        sti
        ret

suena:                          ; ax = la lista de notas
        cmp byte [sonido], 0
        je .r
        cli
        mov [son_p], ax
        mov byte [son_t], 1
        sti
.r:     ret

lanza:                          ; suelta las pelotas pegadas a la pala
        push ax
        mov si, bolas
        mov cx, NBOLAS
.l:     cmp byte [si + B_EST], 2
        jne .n
        mov word [si + B_EST], 1        ; y B_REB = 0
        mov word [si + B_ANG], 0x0102   ; ángulo 2, hacia la derecha
        mov byte [si + B_SY], -1
        push cx
        call azar
        pop cx
        test ah, 1
        jz .m
        neg byte [si + B_SX]
.m:     push si
        push cx
        mov si, m_juego
        call pon_msg
        pop cx
        pop si
.n:     add si, BOLA_T
        loop .l
        pop ax
        ret

azar:                           ; ax = un número al azar
        mov ax, [semilla]
        mov dx, 25173
        mul dx
        add ax, 13849
        mov [semilla], ax
        ret

; =============================================================================
;  El motor: un tic de juego
; =============================================================================
paso:
        mov al, [teclas]
        or al, [joy]
        test al, 1
        jz .d
        sub word [padf], VPALA
.d:     test al, 2
        jz .c
        add word [padf], VPALA
.c:     call limita_pala
        mov si, bolas
        mov cx, NBOLAS
.b:     push cx
        cmp byte [si + B_EST], 2
        jne .m
        call pega
        jmp .n
.m:     call mueve_bola
.n:     pop cx
        add si, BOLA_T
        loop .b
        jmp mueve_premio

limita_pala:
        mov ax, [padf]
        cmp ax, XL
        jge .a
        mov ax, XL
.a:     mov dx, XR
        sub dx, [ancho]
        cmp ax, dx
        jle .b
        mov ax, dx
.b:     mov [padf], ax
        and al, 0xFC
        mov [padx], ax
        ret

pega:                           ; la pelota si, encima del centro de la pala
        mov ax, [ancho]
        shr ax, 1
        add ax, [padx]
        sub ax, 2
        mov cl, 4
        shl ax, cl
        mov [si + B_X], ax
        mov word [si + B_Y], (PY - 4) * 16
        ret

mueve_bola:                     ; si = la pelota
        cmp byte [si + B_EST], 1
        jne .r
        mov bl, [si + B_ANG]
        xor bh, bh
        mov cl, 6
        mov al, [tx + bx]
        mul byte [vel]
        shr ax, cl
        mov [pdx], ax
        mov al, [ty + bx]
        mul byte [vel]
        shr ax, cl
        mov [pdy], ax
        ; en pasos de menos de un punto (16)
        mov ax, [pdx]
        cmp ax, [pdy]
        jae .m
        mov ax, [pdy]
.m:     SHR4 ax
        inc ax
        mov [npas], ax
        mov ax, [pdx]
        xor dx, dx
        div word [npas]
        cmp byte [si + B_SX], 0
        jge .a
        neg ax
.a:     mov [pdx], ax
        mov ax, [pdy]
        xor dx, dx
        div word [npas]
        cmp byte [si + B_SY], 0
        jge .b
        neg ax
.b:     mov [pdy], ax
.paso:
        ; primero en horizontal
        mov ax, [pdx]
        add [si + B_X], ax
        call choca
        jnc .y
        mov ax, [pdx]
        sub [si + B_X], ax
        neg word [pdx]
        neg byte [si + B_SX]
        call rebotes
.y:     ; luego en vertical
        mov ax, [pdy]
        add [si + B_Y], ax
        call choca
        jnc .pala
        mov ax, [pdy]
        sub [si + B_Y], ax
        neg word [pdy]
        neg byte [si + B_SY]
        call rebotes
        jmp .sig
.pala:  cmp byte [si + B_SY], 0
        jl .sig
        mov ax, [si + B_Y]
        SHR4 ax
        add ax, 3
        cmp ax, PY
        jb .sig
        mov ax, [si + B_Y]
        sub ax, [pdy]
        shr ax, cl
        add ax, 3
        cmp ax, PY
        jae .sig                ; ya estaba por debajo
        mov ax, [si + B_X]
        shr ax, cl
        mov dx, ax
        add dx, 3
        cmp dx, [padx]
        jb .sig
        mov dx, [padx]
        add dx, [ancho]
        dec dx
        cmp ax, dx
        ja .sig
        mov dx, [pdy]
        sub [si + B_Y], dx
        call angulo_pala        ; con el ángulo nuevo, en el tic siguiente
        cmp byte [modo], 2
        jne .r
        jmp vigila
.sig:
        mov ax, [si + B_Y]
        SHR4 ax
        cmp ax, YFUERA
        jb .vigila
        mov byte [si + B_EST], 0
        cmp byte [modo], 2
        jne .r
        inc word [fallos]       ; en la prueba la pala lo cubre todo
        ret
.vigila:
        cmp byte [modo], 2
        jne .v
        call vigila
        cmp byte [si + B_EST], 1
        jne .r
.v:     cmp byte [si + B_EST], 1
        jne .r
        dec word [npas]
        jz .r
        jmp .paso
.r:     ret

rebotes:                        ; tras rebotar: si no ha roto nada, se cuenta
        test byte [tipo], 2
        jz .mas
        mov byte [si + B_REB], 0
        ret
.mas:   inc byte [si + B_REB]
        cmp byte [si + B_REB], 24       ; un bucle: se le tuerce el camino
        jb .r
        mov byte [si + B_REB], 0
        add byte [si + B_ANG], 3
        and byte [si + B_ANG], 7
.r:     ret

angulo_pala:                    ; ax = x de la pelota: el ángulo, según dónde da
        add ax, 3
        sub ax, [padx]          ; 0 a ancho + 2
        mov cl, 4
        shl ax, cl
        mov bx, [ancho]
        add bx, 4
        xor dx, dx
        div bx                  ; 0 a 15
        cmp al, 8
        jb .izq
        sub al, 8
        mov byte [si + B_SX], 1
        jmp .p
.izq:   mov ah, 7
        sub ah, al
        mov al, ah
        mov byte [si + B_SX], -1
.p:     mov [si + B_ANG], al
        mov byte [si + B_SY], -1
        mov byte [tipo], 0
        call rebotes
        mov ax, s_pala
        jmp suena

vigila:                         ; (prueba) la pelota no puede estar dentro de nada
        mov ax, [pdx]           ; ni dar saltos de más de un punto
        mov dx, [pdy]
        mov cx, 2
.salto: or ax, ax
        jns .pos
        neg ax
.pos:   cmp ax, 16
        jbe .bien
        inc word [fallos]
.bien:  xchg ax, dx
        loop .salto
        mov byte [mirar], 1
        call choca
        mov byte [mirar], 0
        jnc .a
        inc word [fallos]
.a:     mov ax, [si + B_Y]
        SHR4 ax
        add ax, 3
        cmp ax, PY
        jb .r
        inc word [fallos]
.r:     ret

; choca: ¿la pelota si toca una pared o un ladrillo? CF=1 si sí, y los
; ladrillos que toca reciben el golpe (salvo con [mirar]). [tipo]: bit 0,
; pared o ladrillo que no se rompe; bit 1, uno que se rompe.
choca:
        mov byte [tipo], 0
        mov ax, [si + B_X]
        SHR4 ax
        mov bx, ax
        mov ax, [si + B_Y]
        shr ax, cl
        mov dx, ax
        cmp bx, XL
        jb .pared
        cmp bx, XR - 4
        ja .pared
        cmp dx, YT
        jb .pared
        mov cl, 3
        mov ax, dx
        sub ax, LY0
        sar ax, cl
        mov [r0], al
        mov ax, dx
        add ax, 3 - LY0
        sar ax, cl
        mov [r1], al
        mov cl, 24
        mov ax, bx
        sub ax, XL
        div cl
        mov [c0], al
        mov ax, bx
        add ax, 3 - XL
        div cl
        mov [c1], al
        mov dh, [r0]
.fr:    cmp dh, NR
        jae .sigr
        mov dl, [c0]
.fc:    mov al, NC
        mul dh
        add al, dl
        adc ah, 0
        mov bx, ax
        cmp byte [rejilla + bx], 0
        je .sigc
        cmp byte [mirar], 0
        je .golpe
        or byte [tipo], 1
        jmp .sigc
.golpe: push dx
        call golpea
        pop dx
.sigc:  inc dl
        cmp dl, [c1]
        jbe .fc
.sigr:  inc dh
        cmp dh, [r1]
        jle .fr
        cmp byte [tipo], 0
        je .no
        stc
        ret
.no:    clc
        ret
.pared: mov byte [tipo], 1
        stc
        ret

golpea:                         ; bx = el ladrillo
        push si
        mov al, [rejilla + bx]
        cmp al, 4
        jne .rompe
        or byte [tipo], 1
        mov ax, s_metal
        jmp .suena
.rompe: or byte [tipo], 2
        inc word [golpes]
        dec al
        mov [rejilla + bx], al
        inc word [puntos]
        or byte [cambio], 1
        test byte [golpes], 7   ; cada ocho golpes, un poco más deprisa
        jnz .v
        cmp byte [vel], VMAX
        jae .v
        inc byte [vel]
.v:     or al, al
        mov ax, s_golpe
        jnz .pinta
        dec word [quedan]
        call suelta_premio
        mov ax, s_romper
.pinta: push ax
        mov al, [rejilla + bx]
        call pinta_celda
        pop ax
.suena: call suena
        pop si
        ret

suelta_premio:                  ; bx = el ladrillo roto
        cmp byte [modo], 2
        je .r
        cmp byte [cap_est], 0
        jne .r
        call azar
        cmp ah, 64              ; uno de cada cuatro
        jae .r
        and al, 7
        push bx
        mov bx, reparto
        xlatb
        pop bx
        mov [cap_t], al
        mov ax, bx
        mov dl, NC
        div dl                  ; al = fila, ah = columna
        mov dl, al
        mov al, 24
        mul ah
        add ax, XL + 8
        mov [cap_x], ax
        mov al, 8
        mul dl
        add ax, LY0
        mov [cap_y], ax
        mov byte [cap_est], 1
.r:     ret

mueve_premio:
        cmp byte [cap_est], 0
        je .r
        inc word [cap_y]
        mov ax, [cap_y]
        add ax, 6
        cmp ax, PY
        jb .r
        cmp word [cap_y], PY + PH
        ja .cae
        mov ax, [cap_x]
        add ax, 11
        cmp ax, [padx]
        jb .cae
        mov ax, [padx]
        add ax, [ancho]
        dec ax
        cmp ax, [cap_x]
        jb .cae
        mov byte [cap_est], 0           ; cogido
        add word [puntos], 5
        or byte [cambio], 1
        mov ax, s_premio
        call suena
        mov al, [cap_t]
        or al, al
        jnz .e1
        cmp word [ancho], ANCHO1        ; A: pala ancha
        jne .r
        mov word [ancho], ANCHO2
        sub word [padf], (ANCHO2 - ANCHO1) / 2
        jmp limita_pala
.e1:    dec al
        jnz .e2
        mov byte [vel], VLENTA          ; L: pelota lenta
        ret
.e2:    dec al
        jnz .e3
        jmp tres                        ; T: tres pelotas
.e3:    cmp byte [vidas], 9             ; V: una vida más
        jae .r
        inc byte [vidas]
        or byte [cambio], 2
        ret
.cae:   cmp word [cap_y], CAPFIN
        jbe .r
        mov byte [cap_est], 0
.r:     ret

tres:                           ; las pelotas que faltan, copiadas de una que hay
        call lanza
        mov si, bolas
        mov cx, NBOLAS
.b:     cmp byte [si + B_EST], 1
        je .hay
        add si, BOLA_T
        loop .b
        ret
.hay:   mov di, bolas
        mov cx, NBOLAS
        mov dl, 3
.l:     cmp byte [di + B_EST], 0
        jne .n
        mov ax, [si + B_X]
        mov [di + B_X], ax
        mov ax, [si + B_Y]
        mov [di + B_Y], ax
        mov ax, [si + B_ANG]
        add al, dl
        and al, 7
        neg ah
        mov [di + B_ANG], ax
        mov ax, [si + B_SY]
        mov [di + B_SY], ax             ; (y B_EST = 1)
        mov byte [di + B_REB], 0
        add dl, 2
.n:     add di, BOLA_T
        loop .l
        ret

; =============================================================================
;  Los muros
; =============================================================================
carga_nivel:                    ; el muro [nivel] a la rejilla
        mov di, rejilla
        mov cx, NCEL
.z:     mov byte [di], 0
        inc di
        loop .z
        mov word [quedan], 0
        mov si, niveles
        xor bh, bh              ; bits que quedan en bl
        mov dl, 0               ; el muro que se lee
.muro:  mov cx, 4
        xor ah, ah
.f:     call bit
        rcl ah, 1
        loop .f
        mov dh, ah              ; filas
        mov di, rejilla
.fila:  xor cx, cx              ; columna 0 a 6
.col:   xor ah, ah
.cod:   call bit
        jnc .ya
        inc ah
        cmp ah, 4
        jb .cod
.ya:    cmp dl, [nivel]
        jne .sigc
        mov bp, cx
        mov [di + bp], ah
        neg bp
        mov [di + bp + NC - 1], ah
        or ah, ah
        jz .sigc
        cmp ah, 4
        je .sigc
        inc word [quedan]
        cmp cx, 6
        je .sigc
        inc word [quedan]
.sigc:  inc cx
        cmp cx, 7
        jb .col
        add di, NC
        dec dh
        jnz .fila
        inc dl
        cmp dl, [nivel]
        jbe .muro
        ret

bit:                            ; CF = el bit siguiente de si
        or bh, bh
        jnz .t
        lodsb
        mov bl, al
        mov bh, 8
.t:     dec bh
        shl bl, 1
        ret

; =============================================================================
;  Pantalla
; =============================================================================
pon_modo:
        cmp byte [modo], 1
        je .txt
        mov al, 4
        add al, [mono]
        xor ah, ah
        int 0x10
        mov ax, 0x0B00          ; fondo negro y colores brillantes
        mov bx, 0x0010
        int 0x10
        mov ah, 0x0B
        mov bx, 0x0101          ; la paleta de azul claro, morado y blanco
        int 0x10
        ret
.txt:   mov al, [mtexto]
        xor ah, ah
        int 0x10
        mov ah, 1
        mov cx, 0x2000          ; sin cursor
        int 0x10
        ret

pinta_todo:
        call pon_modo
        mov si, bolas
        mov cx, NBOLAS
.v:     mov byte [si + B_VIS], 0
        add si, BOLA_T
        loop .v
        mov byte [cap_vis], 0
        mov word [pad_dib], -1
        cmp byte [modo], 0
        jne .ptxt
        mov ax, 8               ; las paredes: arriba...
        xor bx, bx
        call dir_cga
        mov byte [bop], 2
        mov al, [c1b]
        mov [brell], al
        mov cx, 8 * 256 + 80
        call blit
        mov ax, YT              ; ...y a los lados
        xor bx, bx
        call dir_cga
        mov cx, PY + PH + 4 - YT
        mov al, [c1b]
.s:     mov [es:di], al
        mov [es:di + 79], al
        FILA_SIG
        loop .s
        jmp .ladr
.ptxt:  mov di, 160
        mov ax, 0x07DC
        mov cx, 80
        rep stosw
        mov cx, 22
.st:    mov word [es:di], 0x07DE
        mov word [es:di + 158], 0x07DD
        add di, 160
        loop .st
.ladr:  xor bx, bx
.l:     mov al, [rejilla + bx]
        or al, al
        jz .n
        call pinta_celda
.n:     inc bx
        cmp bx, NCEL
        jb .l
        or byte [cambio], 2
        ret

pinta_celda:                    ; bx = la casilla, al = el ladrillo (0 a 4)
        cmp byte [modo], 2
        je .r
        push si
        push bx
        mov cl, al
        mov ax, bx
        mov dl, NC
        div dl                  ; al = fila, ah = columna
        cmp byte [modo], 1
        je .txt
        mov ch, ah
        mov dl, 8
        mul dl
        add ax, LY0
        mov bl, ch
        mov bh, 6
        xchg ax, bx
        mul ah
        inc ax
        xchg ax, bx             ; ax = y, bx = octeto de la x
        call dir_cga
        mov byte [bop], 0
        mov byte [brell], 0
        or cl, cl
        jnz .img
        mov byte [bop], 2
.img:   mov al, 48
        dec cl
        mul cl
        add ax, img_lad
        mov si, ax
        mov cx, 8 * 256 + 6
        call blit
        jmp .fin
.txt:   add al, 4
        mov dh, al
        mov al, 6
        mul ah
        inc al
        mov dl, al
        call dir_txt
        mov al, 6
        mul cl
        add ax, tl_str
        mov si, ax
        mov cx, 6
        mov ah, 0x07
.t:     lodsb
        stosw
        loop .t
.fin:   pop bx
        pop si
.r:     ret

dir_cga:                        ; ax = y, bx = octeto de la x: di
        push dx
        shr ax, 1
        sbb di, di
        and di, 0x2000
        mov dl, 80
        mul dl
        add di, ax
        add di, bx
        pop dx
        ret

dir_txt:                        ; dh = fila, dl = columna: di
        mov al, 80
        mul dh
        add al, dl
        adc ah, 0
        shl ax, 1
        mov di, ax
        ret

; blit: si = de dónde, di = adónde, cl = octetos por línea, ch = líneas;
; [bop]: 0 copiar, 1 con XOR, 2 rellenar con [brell]
blit:
.f:     push di
        mov dl, cl
.b:     mov al, [brell]
        cmp byte [bop], 2
        je .w
        lodsb
        cmp byte [bop], 1
        jne .w
        xor al, [es:di]
.w:     stosb
        dec dl
        jnz .b
        pop di
        FILA_SIG
        dec ch
        jnz .f
        ret

xor_bola:                       ; bx = x, ax = y (puntos)
        mov cl, bl
        and cl, 3
        shl cl, 1
        shr bx, 1
        shr bx, 1
        call dir_cga
        push si
        mov si, forma_bola
        mov ch, 4
.f:     lodsb
        mov ah, al
        xor al, al
        shr ax, cl
        xor [es:di], ah
        xor [es:di + 1], al
        FILA_SIG
        dec ch
        jnz .f
        pop si
        ret

xor_cap:                        ; bx = x, ax = y, cl = el premio
        push si
        push ax
        mov al, 21
        mul cl
        add ax, img_cap
        mov si, ax
        pop ax
        shr bx, 1
        shr bx, 1
        call dir_cga
        mov byte [bop], 1
        mov cx, 7 * 256 + 3
        call blit
        pop si
        ret

pinta_pala:                     ; (CGA) las cuatro líneas de la pala
        mov ax, PY
        mov bx, 1
        call dir_cga
        mov bx, [padx]
        shr bx, 1
        shr bx, 1               ; primer octeto
        mov dx, [ancho]
        shr dx, 1
        shr dx, 1
        add dx, bx              ; el de después del último
        mov ch, PH
.f:     push di
        mov cl, 1
.b:     xor al, al
        cmp cl, bl
        jb .w
        cmp cl, dl
        jae .w
        mov al, [c3b]
        cmp cl, bl
        je .borde
        mov ah, dl
        dec ah
        cmp cl, ah
        jne .w
.borde: mov al, [c2b]
.w:     stosb
        inc cl
        cmp cl, 79
        jb .b
        pop di
        FILA_SIG
        dec ch
        jnz .f
        ret

; ocultar y mostrar lo que se mueve: en la CGA, la pelota y el premio se
; pintan con XOR y se quitan igual antes de mover nada; en texto, se vuelven
; a componer las casillas donde estaban y donde están
ocultar:
        cmp byte [modo], 0
        jne .r
        push cx
        mov si, bolas
        mov cx, NBOLAS
.b:     cmp byte [si + B_VIS], 0
        je .n
        mov byte [si + B_VIS], 0
        push cx
        mov bx, [si + B_DX]
        mov ax, [si + B_DY]
        call xor_bola
        pop cx
.n:     add si, BOLA_T
        loop .b
        cmp byte [cap_vis], 0
        je .f
        mov byte [cap_vis], 0
        mov bx, [cap_dx]
        mov ax, [cap_dy]
        mov cl, [cap_dt]
        call xor_cap
.f:     pop cx
.r:     ret

mostrar:
        cmp byte [modo], 1
        ja .r
        je mostrar_txt
        mov ax, [padx]
        mov dx, [ancho]
        cmp ax, [pad_dib]
        jne .pp
        cmp dx, [anc_dib]
        je .np
.pp:    mov [pad_dib], ax
        mov [anc_dib], dx
        call pinta_pala
.np:    mov si, bolas
        mov cx, NBOLAS
.b:     cmp byte [si + B_EST], 0
        je .n
        push cx
        mov ax, [si + B_X]
        SHR4 ax
        mov bx, ax
        mov ax, [si + B_Y]
        shr ax, cl
        mov [si + B_DX], bx
        mov [si + B_DY], ax
        mov byte [si + B_VIS], 1
        call xor_bola
        pop cx
.n:     add si, BOLA_T
        loop .b
        cmp byte [cap_est], 0
        je .r
        mov bx, [cap_x]
        mov ax, [cap_y]
        mov cl, [cap_t]
        mov [cap_dx], bx
        mov [cap_dy], ax
        mov [cap_dt], cl
        mov byte [cap_vis], 1
        call xor_cap
.r:     ret

mostrar_txt:
        mov ax, [padx]
        mov dx, [ancho]
        cmp ax, [pad_dib]
        jne .pp
        cmp dx, [anc_dib]
        je .np
.pp:    mov [pad_dib], ax
        mov [anc_dib], dx
        mov dx, 23 * 256 + 1
.pl:    call compone
        inc dl
        cmp dl, 79
        jb .pl
.np:    mov si, bolas           ; donde estaban
        mov cx, NBOLAS
.b:     cmp byte [si + B_VIS], 0
        je .n
        mov byte [si + B_VIS], 0
        mov dx, [si + B_DX]
        call compone
.n:     add si, BOLA_T
        loop .b
        mov si, bolas           ; y donde están
        mov cx, NBOLAS
.b2:    cmp byte [si + B_EST], 0
        je .n2
        call celda_bola
        mov [si + B_DX], bx
        mov byte [si + B_VIS], 1
        mov dx, bx
        call compone
.n2:    add si, BOLA_T
        loop .b2
        cmp byte [cap_vis], 0
        je .c
        mov byte [cap_vis], 0
        mov dx, [cap_dx]
        call compone3
.c:     cmp byte [cap_est], 0
        je .r
        call celda_cap
        mov [cap_dx], bx
        mov byte [cap_vis], 1
        mov dx, bx
        call compone3
.r:     ret

compone3:
        call compone
        inc dl
        call compone
        inc dl
        jmp compone

celda_bola:                     ; bl = columna, bh = fila de la pelota si
        push cx
        mov ax, [si + B_X]
        add ax, 32
        mov cl, 6
        shr ax, cl
        mov bl, al
        mov ax, [si + B_Y]
        add ax, 32
        inc cl
        shr ax, cl
        mov bh, al
        pop cx
        ret

celda_cap:
        push cx
        mov ax, [cap_x]
        shr ax, 1
        shr ax, 1
        mov bl, al
        mov ax, [cap_y]
        add ax, 3
        mov cl, 3
        shr ax, cl
        mov bh, al
        pop cx
        ret

; compone: escribe en la casilla dl, dh lo que hay en ella (por orden: una
; pelota, el premio, la pala, un ladrillo o nada)
compone:
        push si
        push cx
        push bx
        mov si, bolas
        mov cx, NBOLAS
.b:     cmp byte [si + B_EST], 0
        je .bn
        call celda_bola
        cmp bx, dx
        mov al, 7
        je .esc
.bn:    add si, BOLA_T
        loop .b
        cmp byte [cap_est], 0
        je .p
        call celda_cap
        cmp dh, bh
        jne .p
        mov al, dl
        sub al, bl
        cmp al, 3
        jae .p
        mov bl, [cap_t]
        xor bh, bh
        mov ah, [letras + bx]
        cmp al, 1
        mov al, ah
        je .esc
        mov al, '('
        jb .esc
        mov al, ')'
        jmp .esc
.p:     cmp dh, 23
        jne .l
        mov ax, [padx]
        shr ax, 1
        shr ax, 1
        cmp dl, al
        jb .l
        mov bx, [ancho]
        shr bx, 1
        shr bx, 1
        add al, bl
        cmp dl, al
        mov al, 0xDF
        jb .esc
.l:     mov al, dh
        sub al, 4
        cmp al, NR
        jae .nada
        mov ah, dl
        dec ah
        cmp ah, 78
        jae .nada
        mov bl, NC
        mul bl
        mov bx, ax              ; fila * 13
        mov al, dl
        dec al
        xor ah, ah
        mov cl, 6
        div cl                  ; al = columna, ah = qué carácter del ladrillo
        mov ch, ah
        add bl, al
        adc bh, 0
        mov al, [rejilla + bx]
        mov cl, 6
        mul cl
        add al, ch
        mov bx, ax
        mov al, [tl_str + bx]
        jmp .esc
.nada:  mov al, ' '
.esc:   push ax
        call dir_txt
        pop ax
        mov ah, 0x07
        stosw
        pop bx
        pop cx
        pop si
        ret

; pon_texto: el texto si (acaba en 0) en la fila dh, columna dl: 255, en el
; centro; de 128 en adelante, (dl - 128) doblada en el modo de texto (para
; que caiga junto a lo pintado); si no, dl más [base]
pon_texto:
        cmp dl, 255
        jne .nc
        push si
        xor cx, cx
.len:   lodsb
        or al, al
        jz .l2
        inc cx
        jmp .len
.l2:    pop si
        mov dl, [ancho_txt]
        sub dl, cl
        shr dl, 1
        jmp .c
.nc:    test dl, 0x80
        jz .lit
        and dl, 0x7F
        cmp byte [modo], 1
        jne .c
        shl dl, 1
        jmp .c
.lit:   add dl, [base]
.c:     lodsb
        or al, al
        jz .r
        mov ah, 2
        xor bh, bh
        int 0x10
        mov ah, 9
        mov bl, [attr]
        mov cx, 1
        int 0x10
        inc dl
        jmp .c
.r:     ret

pon_msg:                        ; el texto si, en el centro de la última fila
        push si
        mov dx, 24 * 256
        mov ah, 2
        xor bh, bh
        int 0x10
        mov ax, 0x0920
        mov bl, [attr]
        mov cl, [ancho_txt]
        xor ch, ch
        int 0x10
        pop si
        mov dx, 24 * 256 + 255
        jmp pon_texto

estado_cambios:
        xor al, al
        xchg al, [cambio]
        test al, 2
        jnz estado
        test al, 1
        jz .r
        mov ax, [puntos]
        mov di, cifras
        call fmt6
        mov si, cifras
        mov dx, 7
        jmp pon_texto
.r:     ret

estado:                         ; la primera fila entera
        mov si, plantilla
        mov di, linea
        mov cx, 41
.c:     lodsb
        mov [di], al
        inc di
        loop .c
        mov ax, [puntos]
        mov di, linea + 7
        call fmt6
        mov ax, [mejor]
        mov di, linea + 21
        call fmt6
        mov al, [vuelta]
        mov ah, 10
        mul ah
        add al, [nivel]
        inc al
        aam
        add ax, '00'
        xchg al, ah
        mov [linea + 34], ax
        cmp byte [sonido], 0
        jne .s
        mov byte [linea + 37], ' '
.s:     mov al, [vidas]
        add al, '0'
        mov [linea + 39], al
        mov si, linea
        xor dx, dx
        jmp pon_texto

fmt6:                           ; ax (en decenas) a seis cifras en di
        push bx
        mov bx, 10
        mov cx, 5
        add di, 4
.l:     xor dx, dx
        div bx
        add dl, '0'
        mov [di], dl
        dec di
        loop .l
        mov byte [di + 6], '0'
        pop bx
        ret

; =============================================================================
;  Las imágenes de los ladrillos y los premios (en la CGA)
; =============================================================================
hace_imagenes:
        mov al, 0x55            ; los octetos de un color, o todos blancos
        mov ah, 0xAA
        cmp byte [mono], 0
        je .c
        mov ax, 0xFFFF
.c:     mov [c1b], ax
        mov byte [c3b], 0xFF
        mov di, img_lad
        mov cx, img_fin - img_lad
.z:     mov byte [di], 0
        inc di
        loop .z
        mov di, img_lad
        mov byte [k], 1
.k:     xor dh, dh
.y:     xor bx, bx
.x:     call color_lad
        call pon_pix
        inc bx
        cmp bx, 24
        jb .x
        add di, 6
        inc dh
        cmp dh, 8
        jb .y
        inc byte [k]
        cmp byte [k], 5
        jb .k
        mov byte [k], 2         ; los premios: letras 2 a 5 de «glifos»
.ck:    xor dh, dh
.cy:    xor bx, bx
.cx:    ; el borde
        mov al, 2
        cmp dh, 0
        je .hb
        cmp dh, 6
        jne .vb
.hb:    cmp bl, 0
        je .cero
        cmp bl, 11
        je .cero
        jmp .cp
.vb:    cmp bl, 0
        je .cp
        cmp bl, 11
        je .cp
        mov ch, bl              ; la letra, de doble ancho
        sub ch, 3
        shr ch, 1
        mov cl, dh
        dec cl
        mov al, [k]
        call pix_glifo
        mov al, 3
        jc .cp
.cero:  xor al, al
.cp:    call pon_pix
        inc bx
        cmp bx, 12
        jb .cx
        add di, 3
        inc dh
        cmp dh, 7
        jb .cy
        inc byte [k]
        cmp byte [k], 6
        jb .ck
        ret

color_lad:                      ; al = el color del punto bx, dh del ladrillo [k]
        xor al, al
        cmp bl, 23
        je .r
        cmp dh, 7
        je .r
        mov ah, [k]
        cmp ah, 4
        jne .n4
        mov al, bl              ; el que no se rompe: a cuadros
        shr al, 1
        xor al, dh
        and al, 1
        jz .r
        mov al, 3
        ret
.n4:    cmp ah, 1
        jne .n1
        mov al, 1               ; un golpe: lleno
        ret
.n1:    mov ch, bl              ; dos y tres: con su número
        sub ch, 9
        shr ch, 1
        mov cl, dh
        dec cl
        mov al, ah
        sub al, 2
        call pix_glifo
        mov al, 0
        cmc
        rcl al, 1               ; al = 1 si no es del número
        cmp byte [k], 3
        jne .dos
        shl al, 1               ; tres: lleno, con el número calado
        ret
.dos:   cmp dh, 0               ; dos: hueco, con el borde y el número
        je .b
        cmp dh, 6
        je .b
        cmp bl, 0
        je .b
        cmp bl, 22
        je .b
        xor al, 1
        jz .r
        mov al, 3
        ret
.b:     mov al, 2
.r:     ret

pix_glifo:                      ; al = letra, ch = x, cl = y: CF = punto puesto
        cmp ch, 3
        jae .no
        cmp cl, 5
        jae .no
        push bx
        mov bl, al
        xor bh, bh
        shl bx, 1
        mov bx, [glifos + bx]
        mov al, 3
        mul cl
        add al, ch
        add al, 2
        mov cl, al
        shl bx, cl
        pop bx
        ret
.no:    clc
        ret

pon_pix:                        ; al = color, bx = x, di = la línea de la imagen
        or al, al
        jz .r
        cmp byte [mono], 0
        je .c
        mov al, 3
.c:     mov cl, bl
        and cl, 3
        xor cl, 3
        shl cl, 1
        shl al, cl
        push bx
        shr bx, 1
        shr bx, 1
        or [di + bx], al
        pop bx
.r:     ret

acentos_cga:                    ; copia letras de la ROM y les pone el acento
        push es
        push cs
        pop es
        mov si, lista_acentos
.l:     mov al, [si]
        or al, al
        jz .fin
        mov cl, 3
        xor ah, ah
        shl ax, cl
        add ax, 0xFA6E          ; las letras de la BIOS del PC
        mov bx, ax
        mov al, [si + 1]
        sub al, 0x80
        xor ah, ah
        shl ax, cl
        add ax, tabla_1f
        mov di, ax
        push si
        push ds
        mov si, bx
        mov ax, 0xF000
        mov ds, ax
        mov cx, 8
        rep movsb
        pop ds
        pop si
        mov al, [si + 2]
        or al, al
        jz .vuelta
        mov [di - 8], al
        mov al, [si + 3]
        mov [di - 7], al
        jmp .sig
.vuelta:                        ; ¡: la ! cabeza abajo
        mov bx, di
        sub bx, 8
        mov cx, 4
.v:     dec di
        mov al, [bx]
        xchg al, [di]
        mov [bx], al
        inc bx
        loop .v
.sig:   add si, 4
        jmp .l
.fin:   pop es
        mov ax, 0x251F
        mov dx, tabla_1f
        int 0x21
        ret

; =============================================================================
;  El récord
; =============================================================================
lee_record:
        mov ax, 0x3D00
        mov dx, n_record
        int 0x21
        jc .r
        mov bx, ax
        mov ah, 0x3F
        mov cx, 6
        mov dx, rec_buf
        int 0x21
        mov ah, 0x3E
        int 0x21
        mov si, rec_buf
        xor ax, ax
        mov cx, 5
.l:     mov bl, [si]
        sub bl, '0'
        cmp bl, 9
        ja .fin
        mov dx, 10
        mul dx
        xor bh, bh
        add ax, bx
        inc si
        loop .l
.fin:   mov [mejor], ax
.r:     ret

graba_record:
        mov ax, [mejor]
        mov di, rec_buf
        call fmt6
        mov ah, 0x3C
        xor cx, cx
        mov dx, n_record
        int 0x21
        jc .r
        mov bx, ax
        mov ah, 0x40
        mov cx, 8
        mov dx, rec_buf
        int 0x21
        mov ah, 0x3E
        int 0x21
.r:     ret

; =============================================================================
;  DERRIBO /P: la prueba del motor, sin pantalla
; =============================================================================
prueba:
        mov byte [modo], 2
        mov byte [sonido], 0
        mov word [semilla], 12345
.pas:   mov byte [nivel], 0
.niv:   call carga_nivel
        mov ax, [quedan]
        or ax, ax
        jnz .hay
        inc word [fallos]       ; un muro vacío: los datos están mal
.hay:   cmp byte [pasada], 0
        jne .x
        add [total], ax
.x:     mov word [ancho], XR - XL       ; la pala, de pared a pared
        mov word [padf], XL
        call limita_pala
        mov al, VPRUEBA
        cmp byte [pasada], 0
        je .vv
        mov al, VBASE
.vv:    mov [vel], al
        mov si, bolas
        mov cx, NBOLAS
        mov ax, 20 * 16
        mov dx, 0x0101
.b:     mov [si + B_X], ax
        mov word [si + B_Y], (PY - 40) * 16
        mov [si + B_ANG], dx
        mov word [si + B_SY], 0x01FF    ; hacia arriba; B_EST = 1
        mov byte [si + B_REB], 0
        add ax, 100 * 16
        add dl, 3
        neg dh
        add si, BOLA_T
        loop .b
        mov word [cuenta], TICS_PRUEBA
.tic:   call paso
        cmp word [quedan], 0
        je .hecho
        call cuenta_bolas
        jz .sig
        dec word [cuenta]
        jnz .tic
        jmp .sig
.hecho: inc word [despejados]
.sig:   inc byte [nivel]
        cmp byte [nivel], NNIV
        jb .niv
        inc byte [pasada]
        cmp byte [pasada], 2
        jb .pas
        mov dx, m_p1
        mov ax, [despejados]
        call informa
        mov dx, m_p2
        mov ax, [total]
        call informa
        mov dx, m_p3
        mov ax, [golpes]
        call informa
        mov dx, m_p4
        mov ax, [fallos]
        call informa
        mov al, 0
        cmp word [fallos], 0
        jne .mal
        cmp word [despejados], 2 * NNIV
        je .bien
.mal:   mov al, 1
.bien:  mov ah, 0x4C
        int 0x21

informa:                        ; «Motor: », el número ax y el texto dx
        push dx
        push ax
        mov dx, m_motor
        mov ah, 9
        int 0x21
        pop ax
        call num
        pop dx
        mov ah, 9
        int 0x21
        ret

num:                            ; ax, en decimal y sin ceros delante
        xor dx, dx
        mov cx, 10
        div cx
        push dx
        or ax, ax
        jz .e
        call num
.e:     pop dx
        add dl, '0'
        mov ah, 2
        int 0x21
        ret

; =============================================================================
;  Datos
; =============================================================================
; las velocidades de cada ángulo (en 1/64): de 72 grados (casi vertical) a
; 25 (tendida); nunca horizontal, para que no rebote eternamente de lado a lado
tx      db 20, 27, 34, 40, 46, 51, 55, 58
ty      db 61, 58, 54, 50, 44, 39, 33, 27

forma_bola db 0x3C, 0xFF, 0xFF, 0x3C

; letras de 3×5 puntos (de arriba abajo, de izquierda a derecha)
glifos  dw 0x73E7               ; 2
        dw 0x72CF               ; 3
        dw 0x2BED               ; A
        dw 0x4927               ; L
        dw 0x7492               ; T
        dw 0x5B6A               ; V
letras  db "ALTV"
reparto db 0, 0, 0, 1, 1, 2, 2, 3       ; qué premio cae, de ocho veces

; los ladrillos en el modo de texto
tl_str  db "      "
        db 0xDB, 0xDB, 0xDB, 0xDB, 0xDB, ' '
        db "[ 2 ] "
        db 0xB2, 0xB2, "3", 0xB2, 0xB2, ' '
        db 0xB1, 0xB1, 0xB1, 0xB1, 0xB1, ' '

; letras para la CGA: la de la BIOS, la que sale y las dos primeras líneas
; (0: la letra cabeza abajo)
lista_acentos:
        db 'a', 0xA0, 0x0C, 0x18
        db 'e', 0x82, 0x0C, 0x18
        db 'i', 0xA1, 0x0C, 0x18
        db 'o', 0xA2, 0x0C, 0x18
        db 'u', 0xA3, 0x0C, 0x18
        db 'n', 0xA4, 0x76, 0xDC
        db '!', 0xAD, 0, 0
        db 0

; los sonidos: divisor del 8253 (1193182 / hercios) y tics; 0, se acaba
s_pala   dw 1356
         db 3
         dw 0
s_golpe  dw 904
         db 2
         dw 0
s_romper dw 680
         db 3
         dw 0
s_metal  dw 597
         db 1
         dw 0
s_premio dw 2280
         db 3
         dw 1810
         db 3
         dw 1521
         db 3
         dw 1140
         db 4
         dw 0
s_pierde dw 3043
         db 7
         dw 3615
         db 7
         dw 4560
         db 7
         dw 6087
         db 12
         dw 0
s_nivel  dw 2280
         db 5
         dw 1810
         db 5
         dw 1521
         db 5
         dw 1140
         db 8
         dw 1521
         db 4
         dw 1140
         db 12
         dw 0

; ---- los diez muros ----
; Cada muro: cuántas filas (cuatro bits) y, por fila, las siete columnas de
; la izquierda (la de en medio es la séptima; las otras seis son su espejo).
; Cada casilla, con un código de longitud variable: 0 nada, 10 un golpe,
; 110 dos, 1110 tres, 1111 no se rompe. Así los diez caben en menos de
; doscientos octetos y se leen como un dibujo: «.» nada, «1», «2», «3»
; los golpes, «#» el que no se rompe.
%assign ACUM 0
%assign NBIT 0
%macro BITS 2
 %assign BI %2
 %rep %2
  %assign BI BI-1
  %assign ACUM (ACUM << 1) | ((%1 >> BI) & 1)
  %assign NBIT NBIT+1
  %if NBIT = 8
        db ACUM
   %assign ACUM 0
   %assign NBIT 0
  %endif
 %endrep
%endmacro
%macro FILA 1
 %assign FK 1
 %rep 7
  %substr CAR %1 FK
  %if CAR = '.'
   BITS 0, 1
  %elif CAR = '1'
   BITS 2, 2
  %elif CAR = '2'
   BITS 6, 3
  %elif CAR = '3'
   BITS 14, 4
  %elif CAR = '#'
   BITS 15, 4
  %else
   %error "casilla desconocida en un muro"
  %endif
  %assign FK FK+1
 %endrep
%endmacro
%macro MURO 1-*
 BITS %0, 4
 %rep %0
  FILA %1
  %rotate 1
 %endrep
%endmacro

niveles:
        MURO "1111111", "1111111", "2222222", "1111111", "1111111"   ; 1 el andamio
        MURO "......2", ".....21", "....211", "...2111", "..21111", ".211111" ; 2 el tejado
        MURO "2222222", "2..2..2", "2..2..2", "2222222", "1111111", "1111111" ; 3 ventanas
        MURO ".......", "3333333", "1111111", "#.#.#.#", "1111111", "2222222" ; 4 la reja
        MURO "......3", ".....32", "....321", "...3211", "....321", ".....32", "......3" ; 5 la cometa
        MURO "1.1.1.1", "2.2.2.2", "3.3.3.3", "2.2.2.2", "1.1.1.1", ".......", "#.....#", "1111111" ; 6 las columnas
        MURO "2222222", ".......", "...####", "...#333", "...#222", "...1111" ; 7 el cofre
        MURO "3......", "23.....", "123....", "1123...", "11123..", "111123.", "1111123" ; 8 la escalera
        MURO "1#1#1#1", "1111111", "2.2.2.2", "2222222", ".......", "3#3#3#3", "3333333" ; 9 almenas
        MURO "3333333", "#2#2#2#", "2222222", "#1#1#1#", "1111111", "3333333", "#.....#", "1111111", ".......", "2222222" ; 10 la fortaleza
%if NBIT > 0
        db ACUM << (8 - NBIT)
%endif
fin_niveles:

; los textos
plantilla db "Puntos 000000  Mejor 000000 Nivel 01 ", 0x0E, 3, "3", 0
tit_textos:
        db 2, 255, "D E R R I B O", 0
        db 4, 255, "Un juego de Hispa-8086", 0
        db 7, 128 + 8, "1 golpe", 0
        db 9, 128 + 8, "2 golpes", 0
        db 11, 128 + 8, "3 golpes", 0
        db 13, 128 + 8, "No se rompe", 0
        db 7, 128 + 24, "Pala ancha", 0
        db 9, 128 + 24, "Pelota lenta", 0
        db 11, 128 + 24, "Tres pelotas", 0
        db 13, 128 + 24, "Una vida más", 0
        db 16, 255, "Flechas o Z X: mover la pala", 0
        db 17, 255, "Espacio: lanzar   P: pausa", 0
        db 18, 255, "S: sonido  M: monocromo  Esc: salir", 0
        db 19, 255, "También con ratón o palanca", 0
        db 23, 255, "Pulse Espacio para empezar", 0
        db 0xFF
m_mejor   db "Mejor puntuación: "
m_mejor_n db "000000", 0
m_copy    db "(C) 2026 Carlos Sanz y contribuidores", 0
m_lanzar  db "Espacio: lanzar   P: pausa   Esc: fin", 0
m_juego   db "P: pausa   S: sonido   Esc: terminar", 0
m_pausa   db "Pausa. Pulse P para seguir", 0
m_superado db "¡Muro derribado!", 0
m_fin     db "Fin de la partida. Pulse Espacio", 0
m_record  db "¡Nueva mejor puntuación! Pulse Espacio", 0
m_consonido db "Con sonido", 0
m_sinsonido db "Sin sonido", 0
n_record  db "DERRIBO.REC", 0
m_motor   db "Motor: $"
m_p1      db " de 20 muros derribados", 13, 10, '$'
m_p2      db " ladrillos en los diez muros", 13, 10, '$'
m_p3      db " golpes", 13, 10, '$'
m_p4      db " fallos (pelotas dentro de un ladrillo o que pasan la pala)", 13, 10, '$'

cifras  db "000000", 0
rec_buf db "000000", 13, 10

vseg    dw 0xB800
mtexto  db 2
sonido  db 1
attr    db 3
ancho_txt db 40

fin_programa:

; ---- sin valor inicial (no ocupan sitio en el archivo) ----
absolute fin_programa
bss_ini:
modo        resb 1              ; 0 gráficos CGA, 1 texto, 2 sin pantalla
mono        resb 1
base        resb 1
modo_orig   resb 1
raton       resb 1
palanca     resb 1
centro      resw 1
joy         resb 1
bot         resb 1
bot_ant     resb 1
teclas      resb 1
ult         resb 1
tics        resw 1
ultimo      resw 1
acum        resw 1
son_p       resw 1
son_t       resb 1
viejo8      resd 1
viejo9      resd 1
viejo24     resd 1
viejo1f     resd 1
semilla     resw 1
puntos      resw 1
mejor       resw 1
nivel       resb 1
vuelta      resb 1
vidas       resb 1
cambio      resb 1
vel         resb 1
vbase       resb 1
quedan      resw 1
golpes      resw 1
padf        resw 1
padx        resw 1
ancho       resw 1
pad_dib     resw 1
anc_dib     resw 1
pdx         resw 1
pdy         resw 1
npas        resw 1
tipo        resb 1
mirar       resb 1
r0          resb 1
r1          resb 1
c0          resb 1
c1          resb 1
cap_est     resb 1
cap_t       resb 1
cap_x       resw 1
cap_y       resw 1
cap_vis     resb 1
cap_dt      resb 1
cap_dx      resw 1
cap_dy      resw 1
bop         resb 1
brell       resb 1
c1b         resb 1
c2b         resb 1
c3b         resb 1
k           resb 1
pasada      resb 1
total       resw 1
fallos      resw 1
despejados  resw 1
cuenta      resw 1
bolas       resb NBOLAS * BOLA_T
rejilla     resb NCEL
linea       resb 41
img_lad     resb 4 * 48
img_cap     resb 4 * 21
img_fin:
tabla_1f    resb 48 * 8
bss_fin:
