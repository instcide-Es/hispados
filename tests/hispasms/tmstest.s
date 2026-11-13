; =============================================================================
;  TMSTEST.SMS - prueba propia de los modos heredados del TMS9918
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Pone el modo gráfico II (el que usan casi todos los juegos de la consola
;  anterior): las letras propias en los tres tercios de la pantalla, con un
;  color distinto en cada fila de cada letra, un objeto de 16x16 al doble y
;  dos de 8x8, y mira la bandera del quinto objeto en una línea (con su
;  número). Resultado como VDPTEST: en la RAM del cartucho (8000h).
;  Se ensambla con tools/asmz80.py.
; =============================================================================
VDATOS      = $BE
VCTRL       = $BF
NOMBRES     = $3800
COLORES     = $2000
OBJETOS     = $3B00
DIB_OBJ     = $1800
fallos      = $C000

        .org $0000
        di
        im 1
        ld sp,$DFF0
        jp inicio
        .pad $0038
        ei
        reti
        .pad $0066
        retn

vdp_reg:
        ld a,c
        out (VCTRL),a
        ld a,b
        or $80
        out (VCTRL),a
        ret
vdp_esc:
        ld a,l
        out (VCTRL),a
        ld a,h
        or $40
        out (VCTRL),a
        ret
espera_linea:
        ld b,a
@e:
        in a,($7E)
        cp b
        jr nz,@e
        ret
; HL=origen BC=cuántos: a la VRAM
copia:
        ld a,(hl)
        out (VDATOS),a
        inc hl
        dec bc
        ld a,b
        or c
        jr nz,copia
        ret
; A=octeto BC=cuántos
llena:
        ld d,a
@l:
        ld a,d
        out (VDATOS),a
        dec bc
        ld a,b
        or c
        jr nz,@l
        ret

regs:   .byte $02, $80, $0E, $FF, $03, $76, $03, $F4
regs_fin:

inicio:
        xor a
        ld (fallos),a
        ld hl,regs
        ld b,0
@r:
        ld c,(hl)
        push hl
        push bc
        call vdp_reg
        pop bc
        pop hl
        inc hl
        inc b
        ld a,b
        cp regs_fin - regs
        jr nz,@r
        ; los dibujos: las letras (el plano 0 de las del modo 4), en los
        ; nombres 20h-5Ah de cada tercio
        ld e,0                      ; el tercio
@t:
        ld h,e
        ld l,$20
        add hl,hl
        add hl,hl
        add hl,hl                   ; tercio * 800h + 20h * 8
        call vdp_esc
        ld hl,letras
        ld bc,59 * 8
@d:
        ld a,(hl)
        out (VDATOS),a
        inc hl
        inc hl
        inc hl
        inc hl
        dec bc
        ld a,b
        or c
        jr nz,@d
        inc e
        ld a,e
        cp 3
        jr nz,@t
        ; los colores: cada fila de cada dibujo, el suyo (tinta arriba, fondo abajo)
        ld hl,COLORES
        call vdp_esc
        ld bc,$1800
        ld e,0
@c:
        ld a,e                      ; tintas 8-15 sobre negro
        and 7
        add a,8
        rlca
        rlca
        rlca
        rlca
        or 1
        out (VDATOS),a
        inc e
        dec bc
        ld a,b
        or c
        jr nz,@c
        ; la tabla de nombres: el texto en las filas 2, 10 y 18, y el resto
        ; en blanco
        ld hl,NOMBRES
        call vdp_esc
        ld a,' '
        ld bc,768
        call llena
        ld hl,NOMBRES + 2 * 32 + 2
        call vdp_esc
        ld hl,texto1
        ld bc,texto1_fin - texto1
        call copia
        ld hl,NOMBRES + 10 * 32 + 2
        call vdp_esc
        ld hl,texto2
        ld bc,texto2_fin - texto2
        call copia
        ld hl,NOMBRES + 18 * 32 + 2
        call vdp_esc
        ld hl,texto2
        ld bc,texto2_fin - texto2
        call copia
        ; los objetos: dibujo 0 (16x16, un rombo) y 4 (8x8, lleno)
        ld hl,DIB_OBJ
        call vdp_esc
        ld hl,dib_rombo
        ld bc,32 + 8
        call copia
        ; primero, cinco en la misma línea: el aviso del quinto
        ld hl,OBJETOS
        call vdp_esc
        ld hl,cinco
        ld bc,cinco_fin - cinco
        call copia
        ld bc,$01C0                 ; imagen, 8x8
        call vdp_reg
        ld a,$C8                    ; un cuadro entero, sin leer el estado
        call espera_linea
        in a,(VCTRL)
        ld a,$10
        call espera_linea
        ld a,$C8
        call espera_linea
        in a,(VCTRL)
        and $5F                     ; el aviso y el número (4)
        cp $44
        jr z,@b5
        ld hl,fallos
        inc (hl)
@b5:
        ld hl,OBJETOS
        call vdp_esc
        ld hl,objetos
        ld bc,objetos_fin - objetos
        call copia
        ld bc,$01C3                 ; 16x16 al doble
        call vdp_reg
        ; el resultado
        ld a,$08
        ld ($FFFC),a
        ld a,(fallos)
        ld ($8000),a
        ld hl,$61B0
        ld ($8002),hl
        ld a,$DE
        ld ($8001),a
        ld hl,t_bien
        ld a,(fallos)
        or a
        jr z,@ok
        ld hl,t_mal
@ok:
        ld de,$8004
        ld bc,32
        ldir
        xor a
        ld ($FFFC),a
@fin:
        halt
        jr @fin

t_bien:     .byte "TMSTEST BIEN", 0, 0, 0, 0
t_mal:      .byte "QUINTO OBJETO\nTMSTEST MAL", 0
texto1:     .byte "HISPA-SYSTEM: MODO GRAFICO II"
texto1_fin:
texto2:     .byte "0123456789 ABCDEFGHIJKLMNOPQ"
texto2_fin:
cinco:      .byte 99, 0, 0, 4, 99, 20, 0, 6, 99, 40, 0, 8, 99, 60, 0, 10, 99, 80, 0, 12, $D0
cinco_fin:
objetos:    .byte 40, 100, 0, 9, 140, 30, 4, 7, 140, 220, 4, $8F, $D0
objetos_fin:
dib_rombo:
        .byte %00000001, %00000011, %00000111, %00001111, %00011111, %00111111, %01111111, %11111111
        .byte %11111111, %01111111, %00111111, %00011111, %00001111, %00000111, %00000011, %00000001
        .byte %10000000, %11000000, %11100000, %11110000, %11111000, %11111100, %11111110, %11111111
        .byte %11111111, %11111110, %11111100, %11111000, %11110000, %11100000, %11000000, %10000000
        .byte $FF, $FF, $FF, $FF, $FF, $FF, $FF, $FF
letras:
        .include "../../sms/comun/letras.s"
        .pad $7FF0, 0
        .cabecera_sega 0, 0, $4C
