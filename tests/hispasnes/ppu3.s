; =============================================================================
;  PPU3: el modo 7 (un plano de 1024x1024 girado 30 grados), la ventana del
;  color (a la izquierda, se le resta un color fijo: más oscuro) y el brillo
; =============================================================================
TIPO = 0
SRAM = 0
        .include "comun.s"
        .org $008800
principal:
        .a8
        .i16
        ; el mapa (la parte baja de cada palabra): dibujo (x + y) & 3
        stz $2115               ; avanza al escribir la parte baja
        rep #$20
        .a16
        stz $2116
        ldx #$0000
@m:     txa
        and #$007F
        sta $00
        txa
        lsr a
        lsr a
        lsr a
        lsr a
        lsr a
        lsr a
        lsr a
        clc
        adc $00
        and #$0003
        sep #$20
        .a8
        sta $2118
        rep #$20
        .a16
        inx
        cpx #$4000
        bne @m
        ; los dibujos (la parte alta): 4 de 64 puntos
        sep #$20
        .a8
        lda #$80
        sta $2115               ; avanza al escribir la parte alta
        rep #$20
        .a16
        stz $2116
        sep #$20
        .a8
        lda #$00                ; un registro ($2119), A -> B
        sta $4300
        lda #$19
        sta $4301
        ldx #dib7
        stx $4302
        stz $4304
        ldx #256
        stx $4305
        lda #$01
        sta $420B
        ; la paleta
        stz $2121
        ldx #$0000
@p:     lda pal7,x
        sta $2122
        inx
        cpx #16
        bne @p
        ; el modo 7: A = D = cos 30 (DEh), B = sen 30 (80h), C = -B
        lda #$07
        sta $2105
        stz $211A
        lda #$DE
        sta $211B
        stz $211B
        lda #$80
        sta $211C
        stz $211C
        lda #$80
        sta $211D
        lda #$FF
        sta $211D
        lda #$DE
        sta $211E
        stz $211E
        lda #128                ; el centro
        sta $211F
        stz $211F
        lda #112
        sta $2120
        stz $2120
        lda #$40                ; M7HOFS 40h, M7VOFS 20h
        sta $210D
        stz $210D
        lda #$20
        sta $210E
        stz $210E
        lda #$01
        sta $212C
        ; la ventana 1 (0-100), la del color; se resta el color fijo
        stz $2126
        lda #100
        sta $2127
        lda #$20
        sta $2125               ; WOBJSEL: el color con la ventana 1
        lda #$10
        sta $2130               ; CGWSEL: sin suma fuera de la ventana
        lda #$81
        sta $2131               ; CGADSUB: resta, en el fondo 1
        lda #$EA                ; el color fijo: 10 en los tres
        sta $2132
        lda #$0C                ; brillo 12
        sta $2100
@fin:   bra @fin
nmi:
irq:    rti
pal7:   .word $0000, $001F, $03E0, $7C00, $7FFF, $0000, $0000, $0000
dib7:
        ; 0: lleno del 1
        .res 64, $01
        ; 1: el 2 con borde 4
        .byte 4, 4, 4, 4, 4, 4, 4, 4
        .byte 4, 2, 2, 2, 2, 2, 2, 4
        .byte 4, 2, 2, 2, 2, 2, 2, 4
        .byte 4, 2, 2, 2, 2, 2, 2, 4
        .byte 4, 2, 2, 2, 2, 2, 2, 4
        .byte 4, 2, 2, 2, 2, 2, 2, 4
        .byte 4, 2, 2, 2, 2, 2, 2, 4
        .byte 4, 4, 4, 4, 4, 4, 4, 4
        ; 2: el 3
        .res 64, $03
        ; 3: damero de 1 y 4
        .byte 1, 4, 1, 4, 1, 4, 1, 4
        .byte 4, 1, 4, 1, 4, 1, 4, 1
        .byte 1, 4, 1, 4, 1, 4, 1, 4
        .byte 4, 1, 4, 1, 4, 1, 4, 1
        .byte 1, 4, 1, 4, 1, 4, 1, 4
        .byte 4, 1, 4, 1, 4, 1, 4, 1
        .byte 1, 4, 1, 4, 1, 4, 1, 4
        .byte 4, 1, 4, 1, 4, 1, 4, 1
