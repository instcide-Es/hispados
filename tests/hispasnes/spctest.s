; =============================================================================
;  SPCTEST: el procesador del sonido. Le sube un programa con el trato del
;  arranque (el mismo que usan los juegos), el programa hace cuentas (MUL,
;  DIV, DAA, ADDW) y las devuelve por los puertos, y luego toca una melodía
;  con una muestra BRR propia (una onda cuadrada) y eco. En pantalla, si las
;  cuentas salen bien; y en la RAM del cartucho, «HSN» y el resultado (para
;  la prueba /T de HISPASNS.HVP)
; =============================================================================
TIPO = 2
SRAM = 1
        .include "comun.s"
        .org $008800
principal:
        .a8
        .i16
        rep #$20
        .a16
        lda #$0000
        ldx #letras
        ldy #fin_letras-letras
        jsr vram_dma
        sep #$20
        .a8
        lda #$40
        sta $2107
        stz $210B
        stz $2105
        lda #$01
        sta $212C
        stz $2121
        lda #$00
        sta $2122
        lda #$28
        sta $2122
        lda #$FF
        sta $2122
        lda #$7F
        sta $2122
        rep #$20
        .a16
        lda #$4000 + 2*32 + 2
        ldx #t_tit
        ldy #$0000
        jsr texto
        sep #$20
        .a8
        lda #$0F
        sta $2100
        ; el programa, al SPC700 (en 0200h)
        jsr espera_listo
        ldx #spc_prog
        ldy #fin_spc_prog-spc_prog
        rep #$20
        .a16
        lda #$0200
        jsr subir
        sep #$20
        .a8
        ; las cuentas: el programa pone 5Ah en el puerto 0 cuando acaba
@ok:    lda $2140
        cmp #$5A
        bne @ok
        lda $2141
        sta $20
        lda $2142
        sta $21
        lda $2143
        sta $22
        ; el resultado, en la RAM del cartucho: «HSN» y el texto
        lda #'H'
        sta $700000
        lda #'S'
        sta $700001
        lda #'N'
        sta $700002
        lda #$00
        sta $700003
        ldx #t_mal
        lda $20
        cmp #$A8                ; MUL: 12h * 34h = 03A8h
        bne @mal
        lda $21
        cmp #$12                ; DIV: 0123h / 10h = 12h (resto 3)
        bne @mal
        lda $22
        cmp #$42                ; DAA: 15h + 27h = 42h
        bne @mal
        ldx #t_bien
@mal:   stx $24
        ldy $24
        ldx #$0000
@c2:    lda $0000,y
        sta $700004,x
        beq @c3
        inx
        iny
        bra @c2
@c3:    rep #$20
        .a16
        lda #$4000 + 5*32 + 2
        ldx $24
        ldy #$0000
        jsr texto
        sep #$20
        .a8
@fin:   bra @fin
nmi:
irq:    rti

t_tit:  .asciiz "HISPA-SNES SONIDO"
t_bien: .asciiz "SPC700 BIEN"
t_mal:  .asciiz "SPC700 MAL"

        .include "sonido.s"
