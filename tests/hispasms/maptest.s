; =============================================================================
;  MAPTEST - prueba propia de los mapeadores de Hispa-System
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  128 KB: ocho bancos de 16 KB con su número en el primer octeto. Pide cada
;  banco en 4000h y en 8000h y mira que es el que se ve. Se ensambla dos
;  veces con tools/asmz80.py:
;    -D BANCO1=0xFFFE -D BANCO2=0xFFFF -D SUMA=0       el mapeador de Sega
;    -D BANCO1=0x4000 -D BANCO2=0x8000 -D SUMA=0x1234  el de Codemasters (con
;        su cabecera en 7FE0h: la suma en 7FE6h y su contraria en 7FE8h)
;  El resultado, en la RAM de la consola (DF00h): fallos, DEh B0h 61h y el
;  texto («MAPTEST BIEN» o «MAPTEST MAL»).
; =============================================================================
RES         = $DF00

        .org $0000
        di
        im 1
        ld sp,$DEF0
        jp inicio
        .pad $0038
        reti
        .pad $0066
        retn

inicio:
        xor a
        ld (RES),a
        ld c,0                      ; C: los que fallan
        ld b,1                      ; B: el banco (del 1 al 7)
@b:
        ld a,b
        ld (BANCO2),a
        ld a,($8000)
        cp b
        jr z,@b1
        inc c
@b1:
        ld a,b
        ld (BANCO1),a
        ld a,($4000)
        cp b
        jr z,@b2
        inc c
@b2:
        inc b
        ld a,b
        cp 8
        jr nz,@b
        ld a,1                      ; como estaban
        ld (BANCO1),a
        ld a,2
        ld (BANCO2),a
        ld a,c
        ld (RES),a
        ld a,$DE
        ld (RES+1),a
        ld a,$B0
        ld (RES+2),a
        ld a,$61
        ld (RES+3),a
        ld hl,t_bien
        ld a,c
        or a
        jr z,@t
        ld hl,t_mal
@t:
        ld de,RES+4
        ld bc,16
        ldir
@fin:
        halt
        jr @fin

t_bien:     .byte "MAPTEST BIEN", 0, 0, 0, 0
t_mal:      .byte "MAPTEST MAL", 0, 0, 0, 0, 0

        .pad $4000, 0
        .byte 1
        .pad $7FE6, 0
        .word SUMA, ($10000 - SUMA) & $FFFF
        .pad $7FF0, 0
        .cabecera_sega 0, 0, $4F
        .byte 2
        .pad $C000, 0
        .byte 3
        .pad $10000, 0
        .byte 4
        .pad $14000, 0
        .byte 5
        .pad $18000, 0
        .byte 6
        .pad $1C000, 0
        .byte 7
        .pad $20000, 0
