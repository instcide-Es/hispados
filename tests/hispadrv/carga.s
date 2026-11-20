| =============================================================================
|  CARGA.MD - una ROM propia que hace trabajar a Hispa-Drive como un juego
|  exigente, para medir la velocidad (tests/run_hispadrv_tests.sh, /T)
|  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
|
|    m68k-linux-gnu-as --register-prefix-optional -o carga.o carga.s
|    m68k-linux-gnu-ld -e 0 -Ttext=0 --oformat binary -o CARGA.MD carga.o
|  En cada cuadro: los dos planos llenos (64x32 casillas), el B con
|  desplazamiento horizontal por línea (una onda que se mueve) y el A por
|  columnas en vertical; 80 objetos de 2x2 casillas que rebotan (hasta 20
|  en una línea); en la interrupción de cuadro, tres DMA (los objetos, la
|  tabla del desplazamiento y la paleta, que cambia); y el 68000, entre
|  medias, con la lógica de los objetos y cuentas (MULS, DIVU) como un
|  juego. El Z80 hace de programa de sonido: en cada interrupción escribe en
|  el YM2612 (notas en los seis canales, que cambian cada 8 cuadros) y en el
|  SN76489.
| =============================================================================
        .equ VDP_DATA, 0xC00000
        .equ VDP_CTRL, 0xC00004
        .equ PSG,      0xC00011
        .equ Z80_RAM,  0xA00000
        .equ Z80_BUS,  0xA11100
        .equ Z80_RST,  0xA11200
        .equ CUADROS,  0xFF0000
        .equ OBJ,      0xFF0100         | 80 objetos: x, y, dx, dy (palabras)
        .equ SAT,      0xFF0800         | la tabla de objetos, para la DMA
        .equ HS,       0xFF1000         | el desplazamiento por línea (224 x 4)
        .equ PAL,      0xFF1400         | la paleta (64 palabras)
        .equ CUENTA,   0xFF1500

        .org 0
        .long 0x00FFFE00, inicio
        .rept 28
        .long nada
        .endr
        .long vint                       | 30: nivel 6
        .rept 33
        .long nada
        .endr
        .org 0x100
        .ascii "HISPA-DOS CARGA "
        .org 0x1F0
        .ascii "JUE             "

        .org 0x200
inicio:
        move.w #0x2700, sr
        lea 0xFFFE00, sp
        lea 0xFF0000, a0
        move.w #0x3FFF, d0
1:      clr.l (a0)+
        dbra d0, 1b
        lea VDP_CTRL, a5
        lea VDP_DATA, a4
        move.w (a5), d0
        lea regs(pc), a0
        move.w #0x8000, d1
        moveq #18, d2
2:      move.b (a0)+, d1
        move.w d1, (a5)
        add.w #0x100, d1
        dbra d2, 2b
        | los dibujos: 64 casillas de rayas de colores (de 1 a 64)
        move.l #0x40200000, (a5)         | VRAM 20h
        moveq #63, d1
3:      moveq #7, d2
4:      move.w d1, d3
        add.w d2, d3
        and.w #15, d3
        move.w d3, d4
        lsl.w #4, d4
        or.w d3, d4
        move.w d4, d5
        lsl.w #8, d5
        or.w d4, d5
        move.w d5, (a4)
        move.w d5, (a4)
        dbra d2, 4b
        dbra d1, 3b
        | los planos A (C000h) y B (E000h): 64x32 casillas cada uno
        move.l #0x40000003, (a5)
        move.w #2047, d0
        moveq #0, d1
5:      move.w d1, d2
        and.w #63, d2
        addq.w #1, d2
        move.w d1, d3
        and.w #0x6000, d3
        or.w d3, d2
        move.w d2, (a4)
        add.w #37, d1
        dbra d0, 5b
        move.l #0x60000003, (a5)
        move.w #2047, d0
        moveq #5, d1
6:      move.w d1, d2
        and.w #63, d2
        addq.w #1, d2
        or.w #0x2000, d2
        move.w d2, (a4)
        add.w #11, d1
        dbra d0, 6b
        | los objetos: posiciones y velocidades
        lea OBJ, a0
        moveq #79, d0
        moveq #1, d1
7:      move.w d1, d2
        mulu #97, d2
        and.w #255, d2
        add.w #128, d2
        move.w d2, (a0)+                 | x
        move.w d1, d2
        mulu #61, d2
        and.w #127, d2
        add.w #160, d2
        move.w d2, (a0)+                 | y
        move.w d1, d2
        and.w #3, d2
        addq.w #1, d2
        move.w d2, (a0)+                 | dx
        move.w d1, d2
        lsr.w #2, d2
        and.w #1, d2
        addq.w #1, d2
        move.w d2, (a0)+                 | dy
        addq.w #1, d1
        dbra d0, 7b
        | el Z80: su programa, y en marcha
        move.w #0x100, Z80_BUS
        move.w #0x000, Z80_RST
8:      btst #0, Z80_BUS
        bne 8b
        lea z80_prog, a0
        lea Z80_RAM, a1
        move.w #z80_fin-z80_prog-1, d0
9:      move.b (a0)+, (a1)+
        dbra d0, 9b
        move.w #0x100, Z80_RST
        move.w #0, Z80_BUS
        move.w #0x8174, (a5)             | la imagen, la de cuadro y la DMA
        move.w #0x2000, sr

| ---- el bucle del juego ----
bucle:
        | los objetos: se mueven y rebotan; la tabla para la DMA
        lea OBJ, a0
        lea SAT, a1
        moveq #79, d7
        moveq #1, d6                     | el enlace
1:      movem.w (a0), d0-d3
        add.w d2, d0
        cmp.w #128, d0
        bgt 2f
        neg.w d2
2:      cmp.w #128+304, d0
        blt 3f
        neg.w d2
3:      add.w d3, d1
        cmp.w #128, d1
        bgt 4f
        neg.w d3
4:      cmp.w #128+208, d1
        blt 5f
        neg.w d3
5:      movem.w d0-d3, (a0)
        addq.l #8, a0
        move.w d1, (a1)+                 | y
        move.w d6, d4
        cmp.w #80, d4
        bne 6f
        moveq #0, d4
6:      or.w #0x0500, d4
        move.w d4, (a1)+                 | 2x2 y el enlace
        move.w d6, d4
        and.w #63, d4
        addq.w #1, d4
        or.w #0x4000, d4
        move.w d4, (a1)+                 | el dibujo
        move.w d0, (a1)+                 | x
        addq.w #1, d6
        dbra d7, 1b
        | la onda del desplazamiento, por línea
        lea HS, a0
        move.l CUADROS, d0
        move.w #223, d7
        moveq #0, d1
7:      move.w d1, d2
        add.w d0, d2
        and.w #63, d2
        cmp.w #32, d2
        bcs 8f
        neg.w d2
        add.w #64, d2
8:      muls #3, d2
        move.w d0, (a0)+                 | A: corre
        move.w d2, (a0)+                 | B: la onda
        addq.w #1, d1
        dbra d7, 7b
        | la paleta: cambia poco a poco
        lea PAL, a0
        move.l CUADROS, d0
        moveq #63, d7
9:      move.w d7, d1
        add.w d0, d1
        mulu #0x0123, d1
        divu #7, d1
        and.w #0x0EEE, d1
        move.w d1, (a0)+
        dbra d7, 9b
        | cuentas, como las de un juego
        move.l #1234567, d0
        moveq #31, d7
10:     divu #77, d0
        swap d0
        muls #13, d0
        add.l #99991, d0
        dbra d7, 10b
        move.l d0, CUENTA
        | y a esperar el cuadro siguiente
        move.l CUADROS, d0
11:     cmp.l CUADROS, d0
        beq 11b
        bra bucle

nada:
        rte

| la de cuadro: las tres DMA y el desplazamiento vertical
vint:
        movem.l d0-d2/a0, -(sp)
        addq.l #1, CUADROS
        lea VDP_CTRL, a0
        move.l #SAT, d0                  | los objetos: 320 palabras a D800h
        move.w #320, d1
        move.l #0x58000083, d2
        bsr dma
        move.l #HS, d0                   | el desplazamiento: 448 palabras a DC00h
        move.w #448, d1
        move.l #0x5C000083, d2
        bsr dma
        move.l #PAL, d0                  | la paleta: 64 palabras a la CRAM
        moveq #64, d1
        move.l #0xC0000080, d2
        bsr dma
        move.l #0x40000010, (a0)         | VSRAM: A por columnas
        move.l CUADROS, d0
        moveq #19, d1
1:      move.w d0, VDP_DATA
        move.w d1, VDP_DATA
        add.w #3, d0
        dbra d1, 1b
        movem.l (sp)+, d0-d2/a0
        rte
| D0=fuente D1=palabras D2=la orden (con CD5)
dma:
        move.w d1, -(sp)
        and.w #0xFF, d1
        or.w #0x9300, d1
        move.w d1, (a0)
        move.w (sp)+, d1
        lsr.w #8, d1
        or.w #0x9400, d1
        move.w d1, (a0)
        lsr.l #1, d0
        move.w d0, d1
        and.w #0xFF, d1
        or.w #0x9500, d1
        move.w d1, (a0)
        lsr.l #8, d0
        move.w d0, d1
        and.w #0xFF, d1
        or.w #0x9600, d1
        move.w d1, (a0)
        lsr.l #8, d0
        and.w #0x7F, d0
        or.w #0x9700, d0
        move.w d0, (a0)
        move.l d2, (a0)
        rts

        .align 2
regs:
        .byte 0x04, 0x34, 0x30, 0x2C, 0x07, 0x6C, 0x00, 0x00
        .byte 0x00, 0x00, 0xFF, 0x07, 0x81, 0x37, 0x00, 0x02
        .byte 0x01, 0x00, 0x00
        .align 2
z80_prog:
        .incbin "z80carga.bin"
z80_fin:
        .align 2
        .org 0x8000
        .word 0
