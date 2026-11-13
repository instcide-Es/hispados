; =============================================================================
;  Cometas (HISPA1.NES) - el primer juego propio de Hispa-Nes
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Un juego de naves pequeño, de diseño propio, escrito para el hardware de
;  la consola (funciona en Hispa-Nes, en otros emuladores y en una consola
;  de verdad con un cartucho sencillo: 16 KB de programa, 8 KB de dibujos y
;  RAM con pila). Se ensambla con tools/asm6502.py.
;
;  La nave va por un campo de estrellas que corre hacia la izquierda y le
;  salen cometas al paso. Cruceta: mover. A: disparar (tres disparos a la
;  vez). Start: empezar y pausa. Cada cometa deshecho son 10 puntos y cada
;  100 puntos salen más seguidos; un choque quita una vida (hay tres) y la
;  nave parpadea un momento sin que le afecten. La mejor puntuación se
;  guarda en la RAM del cartucho (en Hispa-Nes, HISPA1.SAV).
;
;  Cómo está hecho (sirve de muestra para los siguientes):
;  - arriba, el marcador, que no se mueve; debajo, las estrellas, que sí: el
;    objeto 0, escondido detrás de la barra del marcador, avisa de por dónde
;    va la imagen y entonces se cambia el desplazamiento;
;  - todo lo que se escribe en la memoria de imagen se hace en el retorno
;    vertical (la NMI) o con la imagen apagada;
;  - los números aleatorios salen de un registro de desplazamiento que solo
;    avanza al sacar un cometa: con los mismos botones en los mismos cuadros
;    la partida es siempre igual (así se prueba el emulador).
; =============================================================================

; ---- registros de la consola ----
PPUCTRL     = $2000
PPUMASK     = $2001
PPUSTATUS   = $2002
OAMADDR     = $2003
PPUSCROLL   = $2005
PPUADDR     = $2006
PPUDATA     = $2007
OAMDMA      = $4014
SNDCHN      = $4015
MANDO1      = $4016
CUADROS     = $4017

; ---- botones (como salen de leer el mando) ----
B_A         = $01
B_B         = $02
B_SELECT    = $04
B_START     = $08
B_ARRIBA    = $10
B_ABAJO     = $20
B_IZQ       = $40
B_DER       = $80

CTRL        = %10001000             ; NMI, objetos en 1000h, fondo en 0000h
MASCARA     = %00011110             ; fondo y objetos, sin tapar la izquierda

; ---- estados ----
E_TITULO    = 0
E_JUEGO     = 1
E_FIN       = 2
E_PAUSA     = 3

N_BALAS     = 3
N_COMETAS   = 4
T_NAVE      = 4                     ; dibujos de los objetos
T_COMETA    = 8
T_EXPLO     = 12
T_BALA      = 1

; ---- variables (página cero) ----
nmi_listo   = $00
cuadro      = $01
mando       = $02
mando_ant   = $03
mando_nuevo = $04
estado      = $05
corre       = $06                   ; desplazamiento de las estrellas (0-255)
corre_t     = $07                   ;   y en cuál de las dos tablas empieza
nave_x      = $08
nave_y      = $09
vidas       = $0A
inmune      = $0B
azar        = $0C                   ; (2 octetos)
sale_en     = $0E                   ; cuadros que faltan para otro cometa
sale_cada   = $0F
marc_sucio  = $10                   ; hay que pintar el marcador
mensaje     = $11                   ; hay que pintar el mensaje n (0: no)
t0          = $12
t1          = $13
t2          = $14
t3          = $15
ptr         = $16                   ; (2 octetos)
puntos      = $18                   ; 5 cifras, la primera la de más peso
mejor       = $1D                   ; 5 cifras
b_vive      = $22                   ; disparos
b_x         = $25
b_y         = $28
c_vive      = $2B                   ; cometas: 0 no hay, 1 vuela, 2-17 explota
c_x         = $2F
c_y         = $33
c_v         = $37
oam         = $0200                 ; copia de los objetos

; ---- cabecera iNES: 16 KB, 8 KB, tablas una al lado de otra, con pila ----
        .byte "NES", $1A, 1, 1, $03, 0
        .res 8, 0

        .org $C000
inicio:
        sei
        cld
        ldx #$40
        stx CUADROS                 ; sin interrupción del sonido
        ldx #$FF
        txs
        inx
        stx PPUCTRL
        stx PPUMASK
        stx $4010
        bit PPUSTATUS
@v1:
        bit PPUSTATUS
        bpl @v1
        lda #0
        tax
@limpia:
        sta $00,x
        sta $0100,x
        sta $0300,x
        sta $0400,x
        sta $0500,x
        sta $0600,x
        sta $0700,x
        inx
        bne @limpia
@v2:
        bit PPUSTATUS
        bpl @v2
        lda #$0F
        sta SNDCHN
        jsr pila_lee
        jsr pantalla_titulo

; ---- el bucle: una vuelta por cuadro ----
bucle:
        lda nmi_listo
        beq bucle
        lda #0
        sta nmi_listo
        jsr lee_mando
        lda estado
        beq paso_titulo
        jsr corte                   ; (con marcador: las estrellas, aparte)
        lda estado
        cmp #E_JUEGO
        beq paso_juego
        cmp #E_PAUSA
        beq paso_pausa
        ; fin de la partida: Start vuelve al título
        lda mando_nuevo
        and #B_START
        beq bucle
        jsr pantalla_titulo
        jmp bucle

paso_titulo:
        lda mando_nuevo
        and #B_START
        beq bucle
        jsr pantalla_juego
        jmp bucle

paso_pausa:
        lda mando_nuevo
        and #B_START
        beq bucle
        lda #E_JUEGO
        sta estado
        lda #1
        sta mensaje
        jmp bucle

paso_juego:
        lda mando_nuevo
        and #B_START
        beq @sigue
        lda #E_PAUSA
        sta estado
        lda #2
        sta mensaje
        jmp bucle
@sigue:
        jsr mueve_nave
        jsr dispara
        jsr mueve_balas
        jsr saca_cometa
        jsr mueve_cometas
        lda inmune
        beq @si
        dec inmune
@si:
        inc corre
        bne @so
        inc corre_t
@so:
        jsr pon_objetos
        jmp bucle

; ---- el corte: espera al objeto 0 y desplaza lo de debajo ----
corte:
@a:
        bit PPUSTATUS               ; (el aviso del cuadro anterior se quita
        bvs @a                      ; solo al empezar la imagen)
@b:
        bit PPUSTATUS
        bvc @b
        lda corre
        sta PPUSCROLL
        lda #0
        sta PPUSCROLL
        lda corre_t
        and #1
        ora #CTRL
        sta PPUCTRL
        rts

; ---- la NMI: el retorno vertical ----
nmi:
        pha
        txa
        pha
        tya
        pha
        lda #0
        sta OAMADDR
        lda #>oam
        sta OAMDMA
        lda marc_sucio
        beq @sm
        jsr pinta_marcador
        lda #0
        sta marc_sucio
@sm:
        lda mensaje
        beq @sn
        jsr pinta_mensaje
        lda #0
        sta mensaje
@sn:
        lda #CTRL                   ; arriba, sin desplazar
        sta PPUCTRL
        lda #0
        sta PPUSCROLL
        sta PPUSCROLL
        inc cuadro
        lda #1
        sta nmi_listo
        pla
        tay
        pla
        tax
        pla
irq:
        rti

; puntos, mejor puntuación y vidas, a su sitio en el marcador
pinta_marcador:
        lda #$20
        sta PPUADDR
        lda #$29
        sta PPUADDR
        ldx #0
@p:
        lda puntos,x
        ora #$30
        sta PPUDATA
        inx
        cpx #5
        bne @p
        lda #$20
        sta PPUADDR
        lda #$36
        sta PPUADDR
        ldx #0
@m:
        lda mejor,x
        ora #$30
        sta PPUDATA
        inx
        cpx #5
        bne @m
        lda #$20
        sta PPUADDR
        lda #$48
        sta PPUADDR
        lda vidas
        ora #$30
        sta PPUDATA
        rts

; el mensaje n (1: ninguno) en la segunda fila del marcador
pinta_mensaje:
        asl a
        asl a
        asl a
        asl a
        tax
        lda #$20
        sta PPUADDR
        lda #$4C
        sta PPUADDR
        ldy #16
@l:
        lda mensajes-16,x
        sta PPUDATA
        inx
        dey
        bne @l
        rts
mensajes:
        .byte "                "
        .byte "PAUSA           "
        .byte "FIN: PULSA START"

; ---- el mando: los ocho botones, A el primero (bit 0) ----
lee_mando:
        lda mando
        sta mando_ant
        lda #1
        sta MANDO1
        lda #0
        sta MANDO1
        ldx #8
@l:
        lda MANDO1
        lsr a
        ror mando
        dex
        bne @l
        lda mando_ant
        eor #$FF
        and mando
        sta mando_nuevo
        rts

; ---- la nave ----
mueve_nave:
        lda mando
        and #B_DER
        beq @i
        lda nave_x
        cmp #200
        bcs @i
        adc #2
        sta nave_x
@i:
        lda mando
        and #B_IZQ
        beq @a
        lda nave_x
        cmp #10
        bcc @a
        sbc #2
        sta nave_x
@a:
        lda mando
        and #B_ARRIBA
        beq @b
        lda nave_y
        cmp #42
        bcc @b
        sbc #2
        sta nave_y
@b:
        lda mando
        and #B_ABAJO
        beq @x
        lda nave_y
        cmp #206
        bcs @x
        adc #2
        sta nave_y
@x:
        rts

dispara:
        lda mando_nuevo
        and #B_A
        beq @x
        ldx #N_BALAS-1
@b:
        lda b_vive,x
        beq @libre
        dex
        bpl @b
@x:
        rts
@libre:
        lda #1
        sta b_vive,x
        lda nave_x
        clc
        adc #14
        sta b_x,x
        lda nave_y
        clc
        adc #4
        sta b_y,x
        lda #$84                    ; un pitido que baja
        sta $4000
        lda #$83
        sta $4001
        lda #$70
        sta $4002
        lda #$08
        sta $4003
        rts

mueve_balas:
        ldx #N_BALAS-1
@b:
        lda b_vive,x
        beq @s
        lda b_x,x
        clc
        adc #4
        sta b_x,x
        cmp #248
        bcc @s
        lda #0
        sta b_vive,x
@s:
        dex
        bpl @b
        rts

; ---- los cometas ----
; el siguiente número: registro de desplazamiento de 16 bits -> A
al_azar:
        lsr azar+1
        ror azar
        bcc @x
        lda azar+1
        eor #$B4
        sta azar+1
@x:
        lda azar
        rts

saca_cometa:
        dec sale_en
        bne @x
        lda sale_cada
        sta sale_en
        ldx #N_COMETAS-1
@b:
        lda c_vive,x
        beq @libre
        dex
        bpl @b
@x:
        rts
@libre:
        jsr al_azar
        and #7
        cmp #7
        bne @c
        lda #3
@c:
        sta t0                      ; uno de siete carriles, cada 24 puntos
        asl a
        adc t0
        asl a
        asl a
        asl a
        adc #40
        sta t0
        ldy #N_COMETAS-1            ; en un carril ocupado no sale
@o:
        lda c_vive,y
        beq @os
        lda c_y,y
        cmp t0
        beq @x
@os:
        dey
        bpl @o
        lda t0
        sta c_y,x
        lda #240
        sta c_x,x
        jsr al_azar
        and #1
        clc
        adc #1
        sta c_v,x
        lda #1
        sta c_vive,x
        rts

mueve_cometas:
        ldx #N_COMETAS-1
@c:
        lda c_vive,x
        bne @hay
        jmp @s
@hay:
        cmp #1
        beq @vuela
        inc c_vive,x                ; explota: unos cuadros y desaparece
        lda c_vive,x
        cmp #18
        bcc @s0
        lda #0
        sta c_vive,x
@s0:
        jmp @s
@vuela:
        lda c_x,x
        sec
        sbc c_v,x
        sta c_x,x
        cmp #4
        bcs @dentro
        lda #0                      ; se ha ido por la izquierda
        sta c_vive,x
        jmp @s
@dentro:
        ; ¿le da algún disparo?
        ldy #N_BALAS-1
@b:
        lda b_vive,y
        beq @bs
        lda b_x,y
        sec
        sbc c_x,x
        clc
        adc #6
        cmp #20
        bcs @bs
        lda b_y,y
        sec
        sbc c_y,x
        clc
        adc #4
        cmp #16
        bcs @bs
        lda #0
        sta b_vive,y
        jsr revienta
        jsr suma_diez
        jmp @s
@bs:
        dey
        bpl @b
        ; ¿choca con la nave?
        lda inmune
        bne @s
        lda c_x,x
        sec
        sbc nave_x
        clc
        adc #12
        cmp #24
        bcs @s
        lda c_y,x
        sec
        sbc nave_y
        clc
        adc #12
        cmp #24
        bcs @s
        jsr revienta
        lda #90
        sta inmune
        lda #1
        sta marc_sucio
        dec vidas
        bne @s
        lda #E_FIN                  ; era la última
        sta estado
        lda #3
        sta mensaje
        jsr nuevo_mejor
@s:
        dex
        bmi @x
        jmp @c
@x:
        rts

revienta:
        lda #2
        sta c_vive,x
        lda #$06                    ; ruido que se apaga
        sta $400C
        lda #$0A
        sta $400E
        lda #$08
        sta $400F
        rts

; diez puntos más (y cada cien, los cometas salen más seguidos)
suma_diez:
        stx t1
        lda #1
        sta marc_sucio
        ldx #3
@l:
        inc puntos,x
        lda puntos,x
        cmp #10
        bcc @x
        lda #0
        sta puntos,x
        cpx #3
        bne @n
        lda sale_cada               ; otra centena
        cmp #24
        bcc @n
        sbc #6
        sta sale_cada
@n:
        dex
        bpl @l
@x:
        ldx t1
        rts

; al acabar: si es la mejor puntuación, se apunta (y a la pila)
nuevo_mejor:
        stx t1
        ldx #0
@c:
        lda puntos,x
        cmp mejor,x
        bcc @x
        bne @si
        inx
        cpx #5
        bne @c
        beq @x
@si:
        ldx #4
@l:
        lda puntos,x
        sta mejor,x
        sta $6003,x
        dex
        bpl @l
        lda #'H'
        sta $6000
        lda #'N'
        sta $6001
        lda #'1'
        sta $6002
@x:
        ldx t1
        rts

; la mejor puntuación guardada, si la hay
pila_lee:
        lda $6000
        cmp #'H'
        bne @x
        lda $6001
        cmp #'N'
        bne @x
        lda $6002
        cmp #'1'
        bne @x
        ldx #4
@l:
        lda $6003,x
        cmp #10
        bcs @mal
        sta mejor,x
        dex
        bpl @l
@x:
        rts
@mal:
        lda #0
        ldx #4
@m:
        sta mejor,x
        dex
        bpl @m
        rts

; ---- los objetos del cuadro, a la copia ----
pon_objetos:
        ldx #4                      ; (el 0 es la marca del corte)
        lda inmune
        beq @nave
        lda cuadro
        and #4
        bne @balas                  ; recién chocada: parpadea
@nave:
        lda nave_x
        sta t0
        lda nave_y
        sta t1
        lda #T_NAVE
        sta t2
        lda #0
        sta t3
        jsr pon16
@balas:
        ldy #N_BALAS-1
@b:
        lda b_vive,y
        beq @bs
        lda b_y,y
        sta oam,x
        lda #T_BALA
        sta oam+1,x
        lda #2
        sta oam+2,x
        lda b_x,y
        sta oam+3,x
        inx
        inx
        inx
        inx
@bs:
        dey
        bpl @b
        ldy #N_COMETAS-1
@c:
        lda c_vive,y
        beq @cs
        cmp #1
        beq @vuela
        lda #T_EXPLO
        sta t2
        lda #2
        bne @p
@vuela:
        lda #T_COMETA
        sta t2
        lda #1
@p:
        sta t3
        lda c_x,y
        sta t0
        lda c_y,y
        sta t1
        jsr pon16
@cs:
        dey
        bpl @c
        ; el resto, fuera de la pantalla
        lda #$FF
@r:
        sta oam,x
        inx
        inx
        inx
        inx
        bne @r
        rts

; un objeto de 16x16 (cuatro de 8x8): t0=x t1=y t2=primer dibujo t3=paleta
pon16:
        lda t1
        sta oam,x
        sta oam+4,x
        clc
        adc #8
        sta oam+8,x
        sta oam+12,x
        lda t0
        sta oam+3,x
        sta oam+11,x
        clc
        adc #8
        sta oam+7,x
        sta oam+15,x
        lda t2
        sta oam+1,x
        adc #1
        sta oam+5,x
        adc #1
        sta oam+9,x
        adc #1
        sta oam+13,x
        lda t3
        sta oam+2,x
        sta oam+6,x
        sta oam+10,x
        sta oam+14,x
        txa
        clc
        adc #16
        tax
        rts

todos_fuera:
        ldx #0
        lda #$FF
@l:
        sta oam,x
        inx
        bne @l
        rts

; ---- las pantallas (con la imagen apagada) ----
apaga:
        lda #0
        sta PPUCTRL
        sta PPUMASK
        rts

enciende:
        bit PPUSTATUS
@v:
        bit PPUSTATUS
        bpl @v
        lda #0
        sta OAMADDR
        lda #>oam
        sta OAMDMA
        lda #CTRL
        sta PPUCTRL
        lda #0
        sta PPUSCROLL
        sta PPUSCROLL
        lda #MASCARA
        sta PPUMASK
        rts

; las dos tablas de nombres con estrellas (siempre las mismas) y la paleta
estrellas:
        lda #$20
        sta PPUADDR
        lda #$00
        sta PPUADDR
        lda #$5A
        sta t0
        lda #2
        sta t2                      ; dos tablas
@tabla:
        ldx #4                      ; 4 x 240 = 960 casillas
@x:
        ldy #240
@y:
        lda t0                      ; (un azar aparte, de 8 bits)
        asl a
        bcc @n
        eor #$1D
@n:
        sta t0
        and #$3F
        cmp #5
        bcc @pone
        lda #0
@pone:
        cmp #3
        bne @p2
        lda #1                      ; (el 3 es la barra)
@p2:
        sta PPUDATA
        dey
        bne @y
        dex
        bne @x
        ldy #64                     ; los atributos: todo con la paleta 0
        lda #0
@a:
        sta PPUDATA
        dey
        bne @a
        dec t2
        bne @tabla
        ; las paletas
        lda #$3F
        sta PPUADDR
        lda #$00
        sta PPUADDR
        ldx #0
@c:
        lda paletas,x
        sta PPUDATA
        inx
        cpx #32
        bne @c
        rts
paletas:
        .byte $0F, $30, $10, $21,  $0F, $28, $10, $21,  $0F, $2A, $1A, $21,  $0F, $30, $10, $21
        .byte $0F, $20, $11, $16,  $0F, $17, $07, $27,  $0F, $28, $16, $30,  $0F, $30, $10, $21

; escribe los textos de una lista: dirección (alto, bajo), texto y 0; un 0
; en el alto acaba la lista. La lista, en ptr
textos:
        ldy #0
@t:
        lda (ptr),y
        beq @x
        sta PPUADDR
        iny
        lda (ptr),y
        sta PPUADDR
        iny
@c:
        lda (ptr),y
        beq @f
        sta PPUDATA
        iny
        bne @c
@f:
        iny
        bne @t
@x:
        rts

pantalla_titulo:
        jsr apaga
        jsr estrellas
        lda #<txt_titulo
        sta ptr
        lda #>txt_titulo
        sta ptr+1
        jsr textos
        lda #$23                    ; el nombre, con la paleta 2 (verde)
        sta PPUADDR
        lda #$D0
        sta PPUADDR
        ldx #8
        lda #$AA
@a:
        sta PPUDATA
        dex
        bne @a
        jsr todos_fuera
        lda #E_TITULO
        sta estado
        jmp enciende
txt_titulo:
        .byte $21, $0C, "COMETAS", 0
        .byte $21, $48, "UN JUEGO DE HISPA-NES", 0
        .byte $22, $2A, "PULSA START", 0
        .byte $22, $C5, "CRUCETA: MOVER LA NAVE", 0
        .byte $23, $05, "A: DISPARAR   START: PAUSA", 0
        .byte 0

pantalla_juego:
        jsr apaga
        jsr estrellas
        ; el marcador: tres filas vacías y la barra, en las dos tablas
        lda #$20
        jsr @marco
        lda #$24
        jsr @marco
        lda #<txt_juego
        sta ptr
        lda #>txt_juego
        sta ptr+1
        jsr textos
        lda #$23                    ; el marcador, con la paleta 1 (amarillo)
        sta PPUADDR
        lda #$C0
        sta PPUADDR
        ldx #8
        lda #$55
@a:
        sta PPUDATA
        dex
        bne @a
        ; la partida
        lda #0
        ldx #b_vive
@v:
        sta $00,x
        inx
        cpx #c_v+N_COMETAS
        bne @v
        ldx #4
@p:
        sta puntos,x
        dex
        bpl @p
        sta corre
        sta corre_t
        sta inmune
        lda #3
        sta vidas
        lda #24
        sta nave_x
        lda #112
        sta nave_y
        lda #$A7
        sta azar
        lda #$1D
        sta azar+1
        lda #60
        sta sale_cada
        lda #40
        sta sale_en
        jsr todos_fuera
        lda #23                     ; el objeto 0: la marca, detrás de la barra
        sta oam
        lda #0
        sta oam+1
        lda #$20
        sta oam+2
        lda #16
        sta oam+3
        jsr pon_objetos
        jsr pinta_marcador
        lda #E_JUEGO
        sta estado
        jmp enciende
@marco:
        sta PPUADDR
        lda #$00
        sta PPUADDR
        ldx #96
        lda #0
@m1:
        sta PPUDATA
        dex
        bne @m1
        ldx #32
        lda #3
@m2:
        sta PPUDATA
        dex
        bne @m2
        rts
txt_juego:
        .byte $20, $22, "PUNTOS 00000  MEJOR 00000", 0
        .byte $20, $42, "VIDAS", 0
        .byte 0

; ---- los vectores ----
        .pad $FFFA
        .word nmi, inicio, irq

        .include "dibujos.s"
