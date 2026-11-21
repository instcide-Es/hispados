; =============================================================================
;  El programa del Z80 de MDTEST.MD (tests/hispadrv/mdtest.s), que lo copia
;  a la RAM del Z80 y lo pone en marcha. Copyright (C) 2026 Carlos Sanz y
;  contribuidores. GPL-2.0 o posterior. Se ensambla con tools/asmz80.py.
;  Deja en 1000h: 42h (ha corrido), el octeto 10000h del cartucho (leído por
;  la ventana de 8000h con el banco 2), el estado del YM2612 tras poner en
;  marcha su temporizador A, y cuántas interrupciones (del VDP) ha visto.
; =============================================================================
        .org $0000
        di
        jp inicio
        .pad $0038, 0
        push af
        ld a, ($1003)
        inc a
        ld ($1003), a
        pop af
        ei
        reti
inicio:
        im 1
        ld sp, $1FF0
        xor a
        ld ($1003), a
        ld a, $42
        ld ($1000), a
        ; el banco 2 (68000: 10000h): nueve escrituras, el bit 0 primero
        ld hl, $6000
        ld (hl), 0
        ld (hl), 1
        ld (hl), 0
        ld (hl), 0
        ld (hl), 0
        ld (hl), 0
        ld (hl), 0
        ld (hl), 0
        ld (hl), 0
        ld a, ($8000)
        ld ($1001), a
        ; el SN76489: un tono en el canal 0
        ld a, $8E
        ld ($7F11), a
        ld a, $0F
        ld ($7F11), a
        ld a, $90
        ld ($7F11), a
        ; el YM2612: temporizador A al máximo (1023) y en marcha, con aviso
        ld a, $24
        ld ($4000), a
        ld a, $FF
        ld ($4001), a
        ld a, $25
        ld ($4000), a
        ld a, $03
        ld ($4001), a
        ld a, $27
        ld ($4000), a
        ld a, $05
        ld ($4001), a
        ld b, 0
@espera:
        djnz @espera
        ld a, ($4000)
        ld ($1002), a
        ei
@bucle:
        halt
        jr @bucle
