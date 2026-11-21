; =============================================================================
;  El programa de sonido de CARGA.MD (tests/hispadrv/carga.s): en el Z80.
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;  Al empezar, los seis canales del YM2612 con un sonido (algoritmo 4: dos
;  pares de operadores); en cada interrupción del VDP, cada 8 cuadros, notas
;  nuevas en los seis (soltar y pulsar) y un pitido en el SN76489.
; =============================================================================
        .org $0000
        di
        jp inicio
        .pad $0038, 0
        jp cuadro
inicio:
        im 1
        ld sp, $1FF0
        xor a
        ld ($1F00), a               ; los cuadros
        ld ($1F01), a               ; la nota
        ; los registros de los seis canales (parte 1 y parte 2)
        ld hl, sonido
@r:
        ld a, (hl)
        or a
        jr z, @fin
        ld b, a                     ; el registro
        inc hl
        ld c, (hl)                  ; el valor
        inc hl
        ld a, b
        ld ($4000), a
        ld a, c
        ld ($4001), a
        ld a, b
        ld ($4002), a
        ld a, c
        ld ($4003), a
        jr @r
@fin:
        ei
@bucle:
        halt
        jr @bucle

; la interrupción de cuadro
cuadro:
        push af
        push bc
        push de
        push hl
        ld a, ($1F00)
        inc a
        ld ($1F00), a
        and 7
        jp nz, @x
        ; notas nuevas: cada canal, una de la tabla (y la siguiente cada vez)
        ld a, ($1F01)
        inc a
        and 7
        ld ($1F01), a
        ld e, a
        ld c, 0                     ; el canal (0-2 en cada parte)
@c:
        ; soltar
        ld a, $28
        ld ($4000), a
        ld a, c
        ld ($4001), a
        ld a, c
        or 4
        ld b, a
        ld a, $28
        ld ($4000), a
        ld a, b
        ld ($4001), a
        ; la nota: tabla[(e + c) & 7]
        ld a, e
        add a, c
        and 7
        add a, a
        ld hl, notas
        add a, l
        ld l, a
        ld a, 0
        adc a, h
        ld h, a
        ld a, $A4
        add a, c
        ld ($4000), a
        ld a, (hl)
        ld ($4001), a
        ld a, $A4
        add a, c
        ld ($4002), a
        ld a, (hl)
        ld ($4003), a
        inc hl
        ld a, $A0
        add a, c
        ld ($4000), a
        ld a, (hl)
        ld ($4001), a
        ld a, $A0
        add a, c
        ld ($4002), a
        ld a, (hl)
        ld ($4003), a
        ; pulsar
        ld a, $28
        ld ($4000), a
        ld a, c
        or $F0
        ld ($4001), a
        ld a, $28
        ld ($4000), a
        ld a, c
        or $F4
        ld ($4001), a
        inc c
        ld a, c
        cp 3
        jp nz, @c
        ; el SN76489: un pitido corto
        ld a, $8E
        ld ($7F11), a
        ld a, e
        ld ($7F11), a
        ld a, $92
        ld ($7F11), a
@x:
        pop hl
        pop de
        pop bc
        pop af
        ei
        reti

; el sonido de cada canal: registro, valor (0: fin)
sonido:
        .byte $22, $08              ; el LFO, en marcha
        .byte $27, $00
        .byte $B0, $3C, $B1, $3C, $B2, $3C   ; realimentación 7, algoritmo 4
        .byte $B4, $C0, $B5, $C0, $B6, $C0
        .byte $30, $71, $31, $71, $32, $71   ; DT y MUL de los cuatro operadores
        .byte $34, $02, $35, $02, $36, $02
        .byte $38, $31, $39, $31, $3A, $31
        .byte $3C, $01, $3D, $01, $3E, $01
        .byte $40, $23, $41, $23, $42, $23   ; nivel total
        .byte $44, $1A, $45, $1A, $46, $1A
        .byte $48, $08, $49, $08, $4A, $08
        .byte $4C, $04, $4D, $04, $4E, $04
        .byte $50, $1F, $51, $1F, $52, $1F, $54, $1F, $55, $1F, $56, $1F
        .byte $58, $1F, $59, $1F, $5A, $1F, $5C, $1F, $5D, $1F, $5E, $1F
        .byte $60, $85, $61, $85, $62, $85, $64, $05, $65, $05, $66, $05
        .byte $68, $05, $69, $05, $6A, $05, $6C, $05, $6D, $05, $6E, $05
        .byte $70, $02, $71, $02, $72, $02, $74, $02, $75, $02, $76, $02
        .byte $78, $02, $79, $02, $7A, $02, $7C, $02, $7D, $02, $7E, $02
        .byte $80, $1F, $81, $1F, $82, $1F, $84, $1F, $85, $1F, $86, $1F
        .byte $88, $1F, $89, $1F, $8A, $1F, $8C, $1F, $8D, $1F, $8E, $1F
        .byte 0, 0
; las notas: bloque y F alta, F baja (una escala)
notas:
        .byte $22, $69, $22, $8F, $22, $B8, $22, $E4, $23, $13, $23, $45, $23, $7B, $23, $B4
