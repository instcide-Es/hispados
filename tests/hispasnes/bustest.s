; =============================================================================
;  BUSTEST: lo que hay alrededor del procesador. La NMI de cada cuadro (y la
;  bandera de $4210 y el borrado vertical de $4212), WAI, la IRQ de los
;  temporizadores (V sola y H con V, con los contadores de $2137), la
;  multiplicación y la división de $4202-$4206, el puerto de la RAM ($2180),
;  DMA a la RAM por $2180, HDMA directo e indirecto (contra $2180), MVN y la
;  RAM del cartucho. Una línea por prueba en pantalla, «BIEN» o «MAL», y en
;  la RAM del cartucho «HSN» y el resumen (para /T de HISPASNS.HVP)
; =============================================================================
TIPO = 2
SRAM = 1
        .include "comun.s"
        .org $008800
N_NMI   = $00                   ; (las variables, en la página directa)
EN_VB   = $02
IRQ_V   = $04
IRQ_H   = $06
FALLOS  = $08
FILA    = $0A
TMP     = $0C
principal:
        .a8
        .i16
        rep #$20
        .a16
        lda #$0000
        ldx #letras
        ldy #fin_letras-letras
        jsr vram_dma
        lda #$4000              ; el mapa, vacío
        sta $2116
        ldx #$0400
@v:     stz $2118
        dex
        bne @v
        stz FALLOS
        lda #$4000 + 1*32 + 1
        sta FILA
        ldx #t_tit
        jsr linea
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
        ; ---- 1. la NMI: 10 cuadros con WAI ----
        rep #$20
        .a16
        stz N_NMI
        sep #$20
        .a8
        lda $4210               ; (la bandera, borrada)
        lda #$81                ; NMI y los mandos
        sta $4200
        ldx #10
@w:     wai
        dex
        bne @w
        lda #$01
        sta $4200
        rep #$20
        .a16
        lda N_NMI
        cmp #10
        jsr resultado
        ldx #t_nmi
        jsr linea
        ; ---- 2. en el borrado vertical, $4212 tiene el bit 7 ----
        lda EN_VB
        and #$0080
        cmp #$0080
        jsr resultado
        ldx #t_vb
        jsr linea
        ; ---- 3. la IRQ en la línea 100 (V sola) ----
        sep #$20
        .a8
        lda #100
        sta $4209
        stz $420A
        lda $4211
        stz IRQ_V
        stz IRQ_V+1
        cli
        lda #$21                ; IRQ V
        sta $4200
@i1:    lda IRQ_V+1
        beq @i1
        sei
        lda #$01
        sta $4200
        rep #$20
        .a16
        lda IRQ_V
        and #$01FF
        cmp #100
        jsr resultado
        ldx #t_irqv
        jsr linea
        ; ---- 4. la IRQ en el punto 128 de la línea 50 (H y V) ----
        sep #$20
        .a8
        lda #50
        sta $4209
        lda #128
        sta $4207
        stz $4208
        lda $4211
        stz IRQ_V+1
        cli
        lda #$31
        sta $4200
@i2:    lda IRQ_V+1
        beq @i2
        sei
        lda #$01
        sta $4200
        rep #$20
        .a16
        lda IRQ_V
        and #$01FF
        cmp #50
        bne @mal4
        lda IRQ_H               ; (el punto: de 128 a 250)
        and #$01FF
        cmp #128
        bcc @mal4
        cmp #251
        bcs @mal4
        lda #$0000
@mal4:  jsr resultado
        ldx #t_irqh
        jsr linea
        ; ---- 5. multiplicar y dividir ----
        sep #$20
        .a8
        lda #$12
        sta $4202
        lda #$34
        sta $4203
        nop
        nop
        nop
        nop
        rep #$20
        .a16
        lda $4216
        cmp #$03A8
        bne @mal5
        lda #$1234
        sta $4204
        sep #$20
        .a8
        lda #$56
        sta $4206
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        rep #$20
        .a16
        lda $4214
        cmp #$0036
        bne @mal5
        lda $4216
        cmp #$0010
@mal5:  jsr resultado
        ldx #t_mul
        jsr linea
        ; ---- 6. el puerto de la RAM ($2180) ----
        sep #$20
        .a8
        stz $2181
        lda #$10
        sta $2182
        lda #$01
        sta $2183               ; 7F:1000
        lda #$A1
        sta $2180
        lda #$B2
        sta $2180
        lda #$C3
        sta $2180
        rep #$20
        .a16
        lda $7F1000
        cmp #$B2A1
        bne @mal6
        lda $7F1002
        and #$00FF
        cmp #$00C3
@mal6:  jsr resultado
        ldx #t_wram
        jsr linea
        ; ---- 7. DMA a la RAM por $2180: 16 octetos a 7E:2000 ----
        sep #$20
        .a8
        stz $2181
        lda #$20
        sta $2182
        stz $2183
        stz $4300               ; A -> B, un registro, sumando
        lda #$80
        sta $4301
        ldx #datos16
        stx $4302
        stz $4304
        ldx #16
        stx $4305
        lda #$01
        sta $420B
        rep #$20
        .a16
        ldx #$0000
@d7:    lda $7E2000,x
        cmp datos16,x
        bne @mal7
        inx
        inx
        cpx #16
        bne @d7
        lda $4305               ; (y la cuenta, a 0)
@mal7:  jsr resultado
        ldx #t_dma
        jsr linea
        ; ---- 8. HDMA directo y luego indirecto, contra $2180 ----
        sep #$20
        .a8
        stz $2181
        lda #$30
        sta $2182
        stz $2183               ; 7E:3000
        stz $4310               ; canal 1: directo, un registro
        lda #$80
        sta $4311
        ldx #tabla_dir
        stx $4312
        stz $4314
        lda #$40                ; canal 2: indirecto
        sta $4320
        lda #$80
        sta $4321
        ldx #tabla_ind
        stx $4322
        stz $4324
        stz $4327               ; (el banco de los datos indirectos)
        jsr espera_vblank
        lda #$02
        sta $420C               ; un cuadro con el canal 1
        jsr espera_vblank
        lda #$04
        sta $420C               ; y otro con el 2
        jsr espera_vblank
        stz $420C
        rep #$20
        .a16
        lda $7E3000             ; directo: 11 22 33; indirecto: 44 55 66
        cmp #$2211
        bne @mal8
        lda $7E3002
        cmp #$4433
        bne @mal8
        lda $7E3004
        cmp #$6655
@mal8:  jsr resultado
        ldx #t_hdma
        jsr linea
        ; ---- 9. MVN: 8 octetos de la ROM a 7E:4000 ----
        lda #$0007
        ldx #datos16
        ldy #$4000
        mvn $00, $7E
        sep #$20
        .a8
        lda #$00
        pha
        plb
        rep #$20
        .a16
        lda $7E4006
        cmp datos16+6
        bne @mal9
        cpx #datos16+8
        bne @mal9
        cpy #$4008
@mal9:  jsr resultado
        ldx #t_mvn
        jsr linea
        ; ---- 10. la RAM del cartucho (2 KB, con espejos) ----
        sep #$20
        .a8
        lda #$5A
        sta $700100
        lda $700900             ; (2 KB: 0900h es 0100h otra vez)
        cmp #$5A
        rep #$20
        .a16
        jsr resultado
        ldx #t_sram
        jsr linea
        ; el resumen
        ldx #t_bien
        lda FALLOS
        beq @todo
        ldx #t_malres
@todo:  stx TMP
        jsr linea
        sep #$20
        .a8
        lda #'H'
        sta $700000
        lda #'S'
        sta $700001
        lda #'N'
        sta $700002
        lda #$00
        sta $700003
        ldy TMP
        ldx #$0000
@c:     lda $0000,y
        sta $700004,x
        beq @c1
        inx
        iny
        bra @c
@c1:    lda #$0F
        sta $2100
@fin:   bra @fin

        .a16
; Z=1 (después de un CMP): bien; si no, mal. Escribe «BIEN» o «MAL» al
; principio de la fila
resultado:
        php
        ldx #t_b
        plp
        beq @b
        ldx #t_m
        inc FALLOS
@b:     lda FILA
        ldy #$0000
        jsr texto
        rts
; escribe el texto de X en la fila (detrás de BIEN/MAL) y pasa a la siguiente
linea:
        lda FILA
        clc
        adc #$0005
        ldy #$0000
        jsr texto
        lda FILA
        clc
        adc #$0020
        sta FILA
        rts
        .a8

espera_vblank:
@a:     lda $4212
        bmi @a
@b:     lda $4212
        bpl @b
        rts

nmi:
        rep #$20
        .a16
        pha
        inc N_NMI
        sep #$20
        .a8
        lda $4212
        sta EN_VB
        lda $4210
        rep #$20
        .a16
        pla
        rti
        .a8
irq:
        rep #$20
        .a16
        pha
        sep #$20
        .a8
        lda $2137               ; los contadores, guardados
        lda $213F
        lda $213C
        sta IRQ_H
        lda $213C
        and #$01
        sta IRQ_H+1
        lda $213D
        sta IRQ_V
        lda $213D
        and #$01
        ora #$80
        sta IRQ_V+1
        lda $4211
        stz $4200
        rep #$20
        .a16
        pla
        rti
        .a8

datos16:
        .byte $01, $23, $45, $67, $89, $AB, $CD, $EF, $10, $32, $54, $76, $98, $BA, $DC, $FE
tabla_dir:
        .byte $02, $11          ; una escritura (y otra línea sin nada)
        .byte $82, $22, $33     ; dos líneas seguidas
        .byte $00
tabla_ind:
        .byte $01
        .word ind1
        .byte $82
        .word ind2
        .byte $00
ind1:   .byte $44
ind2:   .byte $55, $66
t_tit:  .asciiz "HISPA-SNES BUS"
t_b:    .asciiz "BIEN "
t_m:    .asciiz "MAL  "
t_nmi:  .asciiz "NMI Y WAI"
t_vb:   .asciiz "4212 VBLANK"
t_irqv: .asciiz "IRQ V"
t_irqh: .asciiz "IRQ H+V"
t_mul:  .asciiz "MUL Y DIV"
t_wram: .asciiz "2180"
t_dma:  .asciiz "DMA"
t_hdma: .asciiz "HDMA"
t_mvn:  .asciiz "MVN"
t_sram: .asciiz "SRAM"
t_bien: .asciiz "TODO BIEN"
t_malres: .asciiz "HAY FALLOS"
