; =============================================================================
;  PPU4: el mosaico (4x4) en el fondo 1, con el texto de PPU1
; =============================================================================
TIPO = 0
SRAM = 0
        .include "comun.s"
        .org $008800
principal:
        .a8
        .i16
        ; las letras, en la VRAM 0000h (2 bits por punto)
        rep #$20
        .a16
        lda #$0000
        ldx #letras
        ldy #fin_letras-letras
        jsr vram_dma
        ; el mapa del fondo 1 en 4000h (palabras): vacío
        sep #$20
        .a8
        lda #$40
        sta $2107               ; BG1SC: 4000h, 32x32
        stz $210B               ; BG12NBA: dibujos en 0
        stz $2105               ; modo 0
        lda #$01
        sta $212C               ; TM: el fondo 1
        ; la paleta: 0 azul oscuro, 1 blanco
        stz $2121
        lda #$00
        sta $2122
        lda #$28
        sta $2122
        lda #$FF
        sta $2122
        lda #$7F
        sta $2122
        ; el texto
        rep #$20
        .a16
        lda #$4000 + 3*32 + 2
        ldx #t_hola
        ldy #$0000
        jsr texto
        lda #$4000 + 5*32 + 2
        ldx #t_dos
        ldy #$0000
        jsr texto
        sep #$20
        .a8
        lda #$31                ; mosaico de 4x4 en el fondo 1
        sta $2106
        lda #$0F
        sta $2100               ; encendida, brillo 15
@fin:   bra @fin
nmi:
irq:    rti
t_hola: .asciiz "HISPA-SNES: PRUEBA 1"
t_dos:  .asciiz "MODO 0, FONDO 1"
