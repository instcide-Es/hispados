; =============================================================================
;  Lo que pinta PPU2 (pinta_ppu2), para ppu2.s y demo.s
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
; =============================================================================; (con A de 8 bits e índices de 16; vuelve igual)
pinta_ppu2:
        .a8
        .i16
        rep #$20
        .a16
        lda #$0000              ; dibujos de 4 bits en 0000h
        ldx #dib4
        ldy #fin_dib4-dib4
        jsr vram_dma
        lda #$3000              ; las letras (2 bits) en 3000h
        ldx #letras
        ldy #fin_letras-letras
        jsr vram_dma
        lda #$4000              ; los objetos en 4000h
        ldx #dibobj
        ldy #fin_dibobj-dibobj
        jsr vram_dma
        lda #$4100              ; (los de la fila de abajo: 16-19)
        ldx #dibobj2
        ldy #64*2
        jsr vram_dma
        ; el mapa del fondo 1 (2000h): un damero de dibujos 1 y 2, con
        ; paletas y volteos según la fila
        lda #$2000
        sta $2116
        ldx #$0000
@m1:    txa
        and #$0001
        sta $10
        txa
        lsr a
        lsr a
        lsr a
        lsr a
        lsr a
        eor $10
        and #$0001
        inc a                   ; dibujo 1 o 2
        sta $12
        txa
        and #$0040              ; (cada dos filas, la paleta 1)
        asl a
        asl a
        asl a
        asl a
        ora $12
        sta $12
        txa
        and #$0100              ; (filas 8-15: volteados)
        beq @m1b
        lda $12
        ora #$C000
        sta $12
@m1b:   lda $12
        sta $2118
        inx
        cpx #$0400
        bne @m1
        ; el fondo 2 (2400h): rayas (dibujo 3) en las filas 8 a 19; desde
        ; la columna 16, con prioridad
        lda #$2400
        sta $2116
        ldx #$0000
@m2:    lda #$0000
        cpx #8*32
        bcc @m2b
        cpx #20*32
        bcs @m2b
        txa
        and #$0010
        beq @m2a
        lda #$2003
        bra @m2b
@m2a:   lda #$0003
@m2b:   sta $2118
        inx
        cpx #$0400
        bne @m2
        ; el fondo 3 (2800h): vacío y texto
        lda #$2800
        sta $2116
        ldx #$0400
@m3:    stz $2118
        dex
        bne @m3
        lda #$2800 + 1*32 + 2
        ldx #t_tit
        ldy #$2400              ; paleta 1, con prioridad: delante de todo
        jsr texto
        lda #$2800 + 26*32 + 2
        ldx #t_pie
        ldy #$0400              ; paleta 1 (2 bits) y sin prioridad
        jsr texto
        sep #$20
        .a8
        ; los registros
        lda #$09                ; modo 1, el fondo 3 delante
        sta $2105
        lda #$20                ; BG1SC 2000h
        sta $2107
        lda #$24
        sta $2108
        lda #$28
        sta $2109
        lda #$00                ; BG1 y BG2: dibujos en 0
        sta $210B
        lda #$03                ; BG3: en 3000h
        sta $210C
        lda #$04                ; BG1HOFS = 4, BG1VOFS = 2
        sta $210D
        stz $210D
        lda #$02
        sta $210E
        stz $210E
        lda #$02                ; OBSEL: base 4000h, 8x8 y 16x16
        sta $2101
        lda #$17                ; TM: BG1, BG2, BG3, OBJ
        sta $212C
        lda #$01                ; TS: BG1
        sta $212D
        lda #$02                ; CGWSEL: suma la secundaria
        sta $2130
        lda #$42                ; CGADSUB: a la mitad, en BG2
        sta $2131
        ; la ventana 1 (200-230) tapa el fondo 1 en la secundaria
        lda #200
        sta $2126
        lda #230
        sta $2127
        lda #$02
        sta $2123               ; W12SEL: BG1 con la ventana 1
        lda #$01
        sta $212F               ; TSW: BG1
        sta $212E               ; TMW: BG1
        ; la paleta
        stz $2121
        ldx #$0000
@p:     lda paleta,x
        sta $2122
        inx
        cpx #fin_paleta-paleta
        bne @p
        lda #128                ; los de los objetos
        sta $2121
        ldx #$0000
@po:    lda paleta_obj,x
        sta $2122
        inx
        cpx #32
        bne @po
        lda #192                ; (la paleta 4 de objetos: la misma)
        sta $2121
        ldx #$0000
@po2:   lda paleta_obj,x
        sta $2122
        inx
        cpx #32
        bne @po2
        ; la OAM: todos fuera (y = 240) y luego los nuestros
        stz $2102
        stz $2103
        ldx #$0000
@o:     lda #$00
        sta $2104
        lda #240
        sta $2104
        stz $2104
        stz $2104
        inx
        cpx #128
        bne @o
        stz $2102
        stz $2103
        ldx #$0000
@o2:    lda objetos,x
        sta $2104
        inx
        cpx #fin_objetos-objetos
        bne @o2
        lda #$00                ; la tabla alta: el objeto 4 con x negativa
        sta $2102               ; y el 3 grande
        lda #$01
        sta $2103
        lda #%10000000          ; objetos 0-3: el 3 grande
        sta $2104
        lda #%00000001          ; objetos 4-7: el 4 con el bit 8 de x
        sta $2104
        ; el HDMA: canal 1, BG1HOFS (dos escrituras), desde la línea 160
        lda #$02
        sta $4310
        lda #$0D
        sta $4311
        ldx #tabla_hdma
        stx $4312
        stz $4314
        lda #$02
        sta $420C
        lda #$0F
        sta $2100
        rts

t_tit:  .asciiz "HISPA-SNES PPU2 MODO 1"
t_pie:  .asciiz "FONDOS OBJETOS SUMA VENTANA"

tabla_hdma:
        .byte 127, 4, 0
        .byte 33, 4, 0
        .byte $88, 6, 0, 8, 0, 10, 0, 12, 0, 14, 0, 12, 0, 10, 0, 8, 0
        .byte $88, 6, 0, 4, 0, 2, 0, 0, 0, 2, 0, 4, 0, 6, 0, 8, 0
        .byte 50, 4, 0
        .byte 0

; los colores (15 bits: 0BBBBBGGGGGRRRRR)
paleta:
        .word $0C63, $001F, $03E0, $7C00, $03FF, $7FE0, $7C1F, $7FFF
        .word $4210, $0010, $0200, $4000, $0210, $4200, $4010, $2108
        .word $0000, $021F, $7E00, $41F0, $3DEF, $0C1F, $7C10, $6318
        .word $1084, $5294, $294A, $7BDE, $0118, $3006, $00C6, $7FFF
fin_paleta:
paleta_obj:
        .word $0000, $7FFF, $001F, $03E0, $7C00, $03FF, $7FE0, $7C1F
        .word $0000, $0000, $0000, $0000, $0000, $0000, $0000, $0000
; los objetos: x, y, dibujo, atributos (vhoopppN)
objetos:
        .byte 40, 120, $00, %00110000    ; 0: prioridad 3
        .byte 60, 120, $00, %01110000    ; 1: volteado izq.-der.
        .byte 80, 120, $00, %10110000    ; 2: volteado arriba-abajo
        .byte 100, 112, $02, %00111000   ; 3: 16x16, paleta 4 (suma)
        .byte 252, 136, $00, %00110000   ; 4: x = -4
        .byte 140, 64, $00, %00000000    ; 5: prioridad 0 (detrás del fondo 2)
fin_objetos:

dib4:
        .tiles 4
; 0: nada
........
........
........
........
........
........
........
........
; 1: lleno del color 1, con borde 7
77777777
71111117
71111117
71111117
71111117
71111117
71111117
77777777
; 2: damero de 2 y 3 (con una esquina 4, para ver el volteo)
44232323
43232323
23232323
32323232
23232323
32323232
23232323
32323232
; 3: rayas con huecos (0)
5555....
.5555...
..5555..
...5555.
....5555
5....555
55....55
555....5
fin_dib4:

dibobj:
        .tiles 4
; 0: una flecha
...11...
..1221..
.122221.
11233211
...33...
...33...
...44...
...44...
; 1: nada
........
........
........
........
........
........
........
........
; 2, 3 (y 18, 19 debajo): un cuadro de 16x16
11111111
12222222
12333333
12344444
12345555
12345666
12345677
12345671
11111111
22222221
33333321
44444321
55554321
66654321
76654321
17654321
fin_dibobj:
dibobj2:
        .tiles 4
; 16 (vacío), 17 (vacío), 18, 19: la mitad de abajo del cuadro
........
........
........
........
........
........
........
........
........
........
........
........
........
........
........
........
12345671
12345677
12345666
12345555
12344444
12333333
12222222
11111111
17654321
76654321
66654321
55554321
44444321
33333321
22222221
11111111
