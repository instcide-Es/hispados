; =============================================================================
;  El sonido de las ROM de prueba: subir un programa al SPC700 (subir) y el
;  programa de SPCTEST (las cuentas y la melodía), para spctest.s y demo.s
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
; =============================================================================
; espera a que el SPC700 diga que está listo (AAh y BBh)
espera_listo:
        rep #$20
        .a16
@e:     lda $2140
        cmp #$BBAA
        bne @e
        sep #$20
        .a8
        rts

; sube Y octetos de X (banco 0) a la dirección A (16 bits) del SPC700 y
; salta allí
        .a16
subir:
        sta $2142               ; la dirección
        sep #$20
        .a8
        lda #$01                ; un bloque
        sta $2141
        lda #$CC
        sta $2140
@k:     cmp $2140
        bne @k
        stz $26                 ; el número del octeto
@o:     lda $0000,x
        sta $2141
        lda $26
        sta $2140
@e:     cmp $2140
        bne @e
        inc $26
        inx
        dey
        bne @o
        ; saltar a 0200h: la dirección, la orden 0 y un número por delante
        rep #$20
        .a16
        lda #$0200
        sta $2142
        sep #$20
        .a8
        stz $2141
        lda $26
        clc
        adc #$02
        sta $2140
@f:     cmp $2140
        bne @f
        rts

; ---- el programa del SPC700 ----
spc_prog:
        .cpu spc700
        .base $0200
        mov a, #$12             ; MUL
        mov y, #$34
        mul ya
        mov $10, a              ; (la parte baja: A8h)
        mov a, #$23             ; DIV: 0123h / 10h
        mov y, #$01
        mov x, #$10
        div ya, x
        mov $11, a
        mov a, #$15             ; DAA: 15h + 27h
        clrc
        adc a, #$27
        daa a
        mov $12, a
        ; la muestra: un bloque de 16 que vuelve sobre sí mismo
        mov x, #$00
@cp:    mov a, !muestra+x
        mov !$1000+x, a
        inc x
        cmp x, #9
        bne @cp
        ; la tabla de muestras (0F00h): empieza y vuelve en 1000h
        mov a, #$00
        mov !$0F00, a
        mov !$0F02, a
        mov a, #$10
        mov !$0F01, a
        mov !$0F03, a
        ; el DSP
        mov $F2, #$6C
        mov $F3, #$20           ; FLG: sin reinicio, con sonido, sin escribir eco
        mov $F2, #$5D
        mov $F3, #$0F           ; DIR
        mov $F2, #$0C
        mov $F3, #$60           ; volumen general
        mov $F2, #$1C
        mov $F3, #$60
        mov $F2, #$00
        mov $F3, #$50           ; voz 0: volumen
        mov $F2, #$01
        mov $F3, #$50
        mov $F2, #$02
        mov $F3, #$85           ; tono 0385h (unos 440 Hz)
        mov $F2, #$03
        mov $F3, #$03
        mov $F2, #$04
        mov $F3, #$00           ; muestra 0
        mov $F2, #$05
        mov $F3, #$8F           ; ADSR: ataque rápido, caída lenta
        mov $F2, #$06
        mov $F3, #$E0           ; sostener alto
        mov $F2, #$4C
        mov $F3, #$01           ; KON: la voz 0
        ; las cuentas, al 65C816
        mov a, $10
        mov $F5, a
        mov a, $11
        mov $F6, a
        mov a, $12
        mov $F7, a
        mov $F4, #$5A
        ; la melodía: el temporizador 0 (8 kHz / 250 = 32 veces por segundo)
        mov $FA, #250
        mov $F1, #$01
        mov x, #$00
@nota:  mov y, #8               ; 8 vueltas del temporizador: un cuarto de segundo
@esp:   mov a, $FD
        beq @esp
        dbnz y, @esp
        mov a, !notas+x
        mov $F2, #$02
        mov $F3, a
        inc x
        mov a, !notas+x
        mov $F2, #$03
        mov $F3, a
        inc x
        cmp x, #16
        bne @nota
        mov x, #$00
        bra @nota
muestra:
        .byte $B3, $77, $77, $77, $77, $88, $88, $88, $88
notas:
        .word $0385, $03F3, $0470, $04B8, $0548, $04B8, $0470, $03F3
        .endbase
        .cpu 65816
fin_spc_prog:
