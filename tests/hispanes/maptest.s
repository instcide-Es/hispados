; =============================================================================
;  MAPTEST - prueba propia de los cartuchos de Hispa-Nes (cambio de bancos)
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Un mismo programa para los tipos de cartucho 1, 2, 3, 4, 7 y 66. Se
;  ensambla con  tools/asm6502.py -D MAPA=n  y sale solo el trozo de 16 KB de
;  C000h a FFFFh; tests/hispanes/mkmaptest.py hace con él la ROM entera:
;  pone delante la cabecera y los demás bancos, cada uno con su número en el
;  primer octeto (los de programa y los de dibujos), y este trozo donde el
;  cartucho lo deja fijo (o en todos los bancos, si no deja ninguno).
;  El programa pide cada banco y mira que el número que se lee es el suyo:
;    1  registro en serie: bancos de 16 KB en 8000h y el espejo de las tablas
;    2  bancos de 16 KB en 8000h
;    3  bancos de dibujos de 8 KB
;    4  bancos de 8 KB en 8000h y en A000h, de dibujos de 1 KB, y el contador
;       de líneas: con el tope en 3, unas 60 interrupciones por cuadro
;    7  bancos de 32 KB y cuál de las dos tablas de nombres se ve
;   66  bancos de 32 KB y de dibujos de 8 KB
;  El resultado, como CPUTEST: 6000h = 0 (o el paso que falla), DEh B0h 61h
;  y un texto; pantalla verde o roja.
; =============================================================================
num         = $00
t0          = $01
irqs        = $02

        .org $C000
identidad:                          ; (para escribir en ROM el mismo valor que
        .byte $00,$01,$02,$03,$04,$05,$06,$07,$08,$09,$0A,$0B,$0C,$0D,$0E,$0F    ; hay: así no chocan)
        .byte $10,$11,$12,$13,$14,$15,$16,$17,$18,$19,$1A,$1B,$1C,$1D,$1E,$1F
        .byte $20,$21,$22,$23,$24,$25,$26,$27,$28,$29,$2A,$2B,$2C,$2D,$2E,$2F
        .byte $30,$31,$32,$33,$34,$35,$36,$37,$38,$39,$3A,$3B,$3C,$3D,$3E,$3F

inicio:
        sei
        cld
        ldx #$FF
        txs
        inx
        stx $2000
        stx $2001
        lda #$40
        sta $4017
        lda #$80
        sta $6000
        lda #$DE
        sta $6001
        lda #$B0
        sta $6002
        lda #$61
        sta $6003
        lda #0
        sta $6004
        sta irqs
        bit $2002
@v1:
        bit $2002
        bpl @v1
@v2:
        bit $2002
        bpl @v2
        lda #1
        sta num
        lda #MAPA
        cmp #1
        bne @n1
        jmp mapa1
@n1:
        cmp #2
        bne @n2
        jmp mapa2
@n2:
        cmp #3
        bne @n3
        jmp mapa3
@n3:
        cmp #4
        bne @n4
        jmp mapa4
@n4:
        cmp #7
        bne @n7
        jmp mapa7
@n7:
        jmp mapa66

; ---- 2: el banco de 16 KB de 8000h ----
mapa2:
        ldx #0
@b:
        txa
        sta identidad,x
        cpx $8000
        bne mal_a
        inx
        cpx #3
        bne @b
        jmp bien

; ---- 3: el banco de dibujos ----
mapa3:
        ldx #0
@b:
        txa
        sta identidad,x
        jsr lee_chr
        stx t0
        cmp t0
        bne mal_a
        inx
        cpx #4
        bne @b
        jmp bien
mal_a:
        jmp falla

; ---- 7: 32 KB y una sola tabla de nombres ----
mapa7:
        ldx #0
@b:
        txa
        sta identidad,x
        cpx $8000
        bne mal_a
        inx
        cpx #4
        bne @b
        inc num                     ; con el bit 4 se ve la otra tabla
        lda #$00
        sta identidad
        lda #$11
        jsr pon_nt
        lda #$10
        sta identidad+$10
        lda #$22
        jsr pon_nt
        lda #$00
        sta identidad
        jsr lee_nt
        cmp #$11
        bne mal_a
        lda #$10
        sta identidad+$10
        jsr lee_nt
        cmp #$22
        bne mal_a
        jmp bien

; ---- 66: 32 KB de programa (bits 4 y 5) y 8 KB de dibujos (bits 0 y 1) ----
mapa66:
        ldx #0
@b:
        lda @valor,x
        tay
        sta identidad,y
        cpx $8000
        bne mal_a
        stx t0
        lda #3
        sec
        sbc t0
        sta t0                      ; los dibujos, al revés: 3, 2, 1, 0
        jsr lee_chr
        cmp t0
        bne mal_a
        inx
        cpx #4
        bne @b
        jmp bien
@valor:
        .byte $03, $12, $21, $30

; ---- 1: registro en serie ----
mapa1:
        lda #$80                    ; bit 7: a empezar (último banco en C000h)
        sta $8000
        ldx #0
@b:
        txa
        jsr serie_prg
        cpx $8000
        bne mal_b
        inx
        cpx #7
        bne @b
        inc num                     ; el espejo: tablas una al lado de otra...
        lda #$0E
        jsr serie_ctl
        lda #$20
        ldx #$11
        jsr pon_en
        lda #$24
        ldx #$22
        jsr pon_en
        lda #$20
        jsr lee_en
        cmp #$11
        bne mal_b
        lda #$0F                    ; ...y una encima de otra
        jsr serie_ctl
        lda #$20
        ldx #$33
        jsr pon_en
        lda #$24
        jsr lee_en
        cmp #$33
        bne mal_b
        inc num                     ; una escritura con el bit 7 a medias lo
        lda #1                      ; deja como al principio
        sta $E000
        sta $E000
        lda #$80
        sta $E000
        lda #5
        jsr serie_prg
        lda $8000
        cmp #5
        bne mal_b
        jmp bien
mal_b:
        jmp falla
; A -> el registro de bancos de programa (E000h), bit a bit
serie_prg:
        ldy #5
@s:
        sta $E000
        lsr a
        dey
        bne @s
        rts
; A -> el registro de control (8000h)
serie_ctl:
        ldy #5
@s:
        sta $8000
        lsr a
        dey
        bne @s
        rts

; ---- 4: bancos de 8 KB, dibujos de 1 KB y contador de líneas ----
mapa4:
        ldx #0
@b:
        lda #6
        sta $8000
        stx $8001
        cpx $8000
        bne mal_b
        lda #7
        sta $8000
        stx $8001
        cpx $A000
        bne mal_b
        inx
        cpx #6
        bne @b
        inc num
        ldx #0                      ; el registro 2: el KB de 1000h
@c:
        lda #2
        sta $8000
        stx $8001
        lda #$10
        sta $2006
        lda #$00
        sta $2006
        lda $2007
        lda $2007
        stx t0
        cmp t0
        bne mal_b
        inx
        cpx #8
        bne @c
        inc num                     ; el contador de líneas
        lda #0
        sta $8000
        sta $8001
        lda #3
        sta $C000
        sta $C001
        sta $E000
        bit $2002
@v:
        bit $2002
        bpl @v
        lda #$08                    ; fondo en 0000h y objetos en 1000h
        sta $2000
        lda #0
        sta $2005
        sta $2005
        lda #$18
        sta $2001
        sta $E001
        lda #0
        sta irqs
        cli
        ldx #2                      ; dos cuadros enteros
@e:
        bit $2002
@w:
        bit $2002
        bpl @w
        dex
        bne @e
        sei
        sta $E000
        lda #0
        sta $2001
        lda irqs                    ; 241 líneas de cada 4, dos veces: unas 120
        cmp #110
        bcc mal_c
        cmp #131
        bcs mal_c
        jmp bien
mal_c:
        jmp falla

; ---- lo que usan varios ----
; el primer octeto de los dibujos que se ven (el número de su banco) -> A
lee_chr:
        lda #$00
        sta $2006
        sta $2006
        lda $2007
        lda $2007
        rts
; A -> la casilla 5 de la tabla de nombres que se ve en 2000h
pon_nt:
        pha
        lda #$20
        sta $2006
        lda #$05
        sta $2006
        pla
        sta $2007
        rts
lee_nt:
        lda #$20
lee_en:                             ; (A=octeto alto de la dirección)
        sta $2006
        lda #$05
        sta $2006
        lda $2007
        lda $2007
        rts
; X -> la casilla 5 de la tabla de dirección alta A
pon_en:
        sta $2006
        lda #$05
        sta $2006
        stx $2007
        rts

bien:
        lda #0
        sta $6000
        ldx #0
@t:
        lda txt_bien,x
        sta $6004,x
        beq @f
        inx
        bne @t
@f:
        lda #$2A
        jmp color
falla:
        ldx #0
@t:
        lda txt_falla,x
        sta $6004,x
        beq @n
        inx
        bne @t
@n:
        lda num
        ora #$30
        sta $6004,x
        lda #0
        sta $6005,x
        lda num
        sta $6000
        lda #$16
color:
        pha
        lda #0
        sta $2001
        bit $2002
@v:
        bit $2002
        bpl @v
        lda #$3F
        sta $2006
        lda #$00
        sta $2006
        pla
        sta $2007
        lda #$00
        sta $2006
        sta $2006
        sta $2000
@fin:
        jmp @fin
txt_bien:
        .byte "MAPTEST BIEN", 0
txt_falla:
        .byte "MAPTEST FALLA ", 0

nmi:
        rti
irq:
        pha
        sta $E000                   ; (cartucho 4) visto: otra vez
        sta $E001
        inc irqs
        pla
        rti

        .pad $FFFA
        .word nmi, inicio, irq
