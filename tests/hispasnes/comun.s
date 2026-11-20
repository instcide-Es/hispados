; =============================================================================
;  Lo común de las ROM de prueba de Hispa-Snes (tools/asm816.py)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;  Con .include al principio de cada prueba: el arranque (inicia), poner la
;  consola a cero, copiar a la VRAM con DMA (vram_dma), las letras propias
;  (las de sms/comun/letras.s, en 2 bits) y escribir texto (texto).
;  Cada prueba define «principal» (lo que hace, en modo nativo, A de 8 bits
;  e índices de 16), «nmi» e «irq» (o rti); y antes de incluir esto,
;  TIPO y SRAM (la cabecera: 0 y 0 sin RAM; 2 y n con 1<<n KB con pila).
; =============================================================================
        .lorom
        .rom 64
        .org $008000
reset:
        sei
        clc
        xce                     ; modo nativo
        rep #$38                ; A, X e Y de 16 bits; sin decimal
        .a16
        .i16
        ldx #$1FFF
        txs
        lda #$0000
        tcd
        sep #$20
        .a8
        pha
        plb                     ; banco de datos 0
        lda #$80
        sta $2100               ; pantalla apagada
        stz $4200               ; sin NMI ni IRQ
        ; los registros de la PPU, a cero
        ldx #$2101
@ppu:   stz $00,x
        inx
        cpx #$2134
        bne @ppu
        lda #$80
        sta $2115               ; la VRAM, de palabra en palabra
        lda #$01
        sta $4200               ; (los mandos se leen solos)
        stz $420C
        ; la RAM baja (0000-1FFF), a cero
        rep #$20
        .a16
        ldx #$0000
@ram:   stz $0000,x
        inx
        inx
        cpx #$1F00
        bne @ram
        sep #$20
        .a8
        jmp principal

; copia a la VRAM con el canal 0: X = origen (banco 0), Y = octetos,
; A (16 bits, con REP antes) = dirección de la VRAM (palabra)
        .a16
vram_dma:
        sta $2116
        stx $4302
        sty $4305
        sep #$20
        .a8
        lda #$01                ; dos registros, A -> B
        sta $4300
        lda #$18
        sta $4301
        stz $4304
        lda #$01
        sta $420B
        rep #$20
        .a16
        rts
        .a8

; escribe el texto de X (banco 0, acabado en 0) en el mapa de la VRAM a
; partir de la palabra A (16 bits); Y = lo que se suma a cada letra (el
; primer dibujo y la paleta)
        .a16
texto:
        sta $2116
        sty $10
@l:     lda $0000,x
        and #$00FF
        beq @x
        sec
        sbc #$0020
        clc
        adc $10
        sta $2118
        inx
        bra @l
@x:     rts
        .a8

nmi_nada:
        rti

letras:
        .include "../../sms/comun/letras.s"
fin_letras:

        .org $00FFC0
        .cabecera "HISPA SNES PRUEBA", $20, TIPO, $07, SRAM
        .org $00FFE4
        .word nmi_nada, nmi_nada, nmi_nada, nmi, nmi_nada, irq
        .org $00FFF4
        .word nmi_nada, nmi_nada, nmi_nada, nmi_nada, reset, nmi_nada
