| =============================================================================
|  SOMBRAS.MD - una ROM propia para probar las sombras y los brillos del VDP
|  de Hispa-Drive (el bit 3 del registro 12; tests/run_hispadrv_tests.sh)
|  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
|
|    m68k-linux-gnu-as --register-prefix-optional -o sombras.o sombras.s
|    m68k-linux-gnu-ld -e 0 -Ttext=0 --oformat binary -o SOMBRAS.MD sombras.o
|  Una pantalla quieta:
|    - el plano B, gris (color 1), de prioridad baja a la izquierda (x < 160:
|      a la mitad) y alta a la derecha (normal);
|    - el plano A: una franja roja de prioridad baja (filas 10 y 11, y 80-95)
|      y, a la izquierda, dos filas transparentes de prioridad alta (20 y 21,
|      y 160-175: el gris de debajo sale normal);
|    - los objetos (de 32x32): el color 14 de la paleta 3 (sube la luz un
|      paso) bajo a la izquierda (40,40: normal) y alto a la derecha (200,40:
|      brillo); el 15 de la paleta 3 (sombra) bajo a la derecha (240,100:
|      tapado por el plano B alto, no hace nada) y alto (264,40: a la mitad);
|      uno verde bajo (40,150: a la mitad), otro alto (80,150: normal), el
|      color 14 de la paleta 0 bajo (120,150: normal siempre) y otro de luz
|      bajo (8,152) sobre las filas que ya eran normales (brillo) y las de
|      sombra (normal).
| =============================================================================
        .equ VDP_DATA, 0xC00000
        .equ VDP_CTRL, 0xC00004

        .org 0
        .long 0x00FFFE00, inicio
        .rept 62
        .long nada
        .endr
        .org 0x100
        .ascii "HISPA-DOS SOMBRA"
        .org 0x1F0
        .ascii "JUE             "

        .org 0x200
inicio:
        move.w #0x2700, sr
        lea 0xFFFE00, sp
        lea VDP_CTRL, a5
        lea VDP_DATA, a4
        move.w (a5), d0
        lea regs(pc), a0
        move.w #0x8000, d1
        moveq #18, d2
1:      move.b (a0)+, d1
        move.w d1, (a5)
        add.w #0x100, d1
        dbra d2, 1b
        | la VRAM a cero
        move.l #0x40000000, (a5)
        move.w #0x3FFF, d0
2:      move.l #0, (a4)
        dbra d0, 2b
        | los dibujos: 1 gris, 2 rojo; 16-31 color 14, 32-47 color 15, 48-63
        | color 1 (para los objetos de 4x4 casillas)
        move.l #0x40200000, (a5)         | VRAM 20h: la casilla 1
        move.l #0x11111111, d1
        bsr casilla
        move.l #0x22222222, d1
        bsr casilla
        move.l #0x42000000, (a5)         | VRAM 200h: la 16
        moveq #15, d3
3:      move.l #0xEEEEEEEE, d1
        bsr casilla
        dbra d3, 3b
        moveq #15, d3
4:      move.l #0xFFFFFFFF, d1
        bsr casilla
        dbra d3, 4b
        moveq #15, d3
5:      move.l #0x11111111, d1
        bsr casilla
        dbra d3, 5b
        | el plano B (E000h): 64x32, la casilla 1; prioridad alta de la
        | columna 20 en adelante
        move.l #0x60000003, (a5)
        moveq #31, d0
6:      moveq #63, d2
7:      move.w #0x0001, d1
        cmp.w #43, d2                    | (columnas 20-63: d2 = 43..0)
        bhi 8f
        move.w #0x8001, d1
8:      move.w d1, (a4)
        dbra d2, 7b
        dbra d0, 6b
        | el plano A (C000h): la franja roja (filas 10 y 11)...
        move.l #0x45000003, (a5)         | C000h + 10 x 128
        move.w #127, d0
9:      move.w #0x0002, (a4)
        dbra d0, 9b
        | ...y lo transparente de prioridad alta (filas 20 y 21, columnas 0-19)
        move.l #0x4A000003, (a5)         | C000h + 20 x 128
        moveq #19, d0
10:     move.w #0x8000, (a4)
        dbra d0, 10b
        move.l #0x4A800003, (a5)         | C000h + 21 x 128
        moveq #19, d0
11:     move.w #0x8000, (a4)
        dbra d0, 11b
        | los objetos (D800h)
        move.l #0x58000003, (a5)
        lea objs(pc), a0
        moveq #8 * 4 - 1, d0
12:     move.w (a0)+, (a4)
        dbra d0, 12b
        | los colores
        move.l #0xC0000000, (a5)
        lea cols(pc), a0
        moveq #63, d0
13:     move.w (a0)+, (a4)
        dbra d0, 13b
        | la imagen encendida
        move.w #0x8144, (a5)
14:     bra 14b

| una casilla de un color (D1): 8 largos
casilla:
        moveq #7, d2
1:      move.l d1, (a4)
        dbra d2, 1b
        rts

nada:
        rte

        .align 2
regs:   | como CARGA.MD, con las sombras y los brillos (registro 12: 89h)
        .byte 0x04, 0x04, 0x30, 0x2C, 0x07, 0x6C, 0x00, 0x00
        .byte 0x00, 0x00, 0xFF, 0x00, 0x89, 0x37, 0x00, 0x02
        .byte 0x01, 0x00, 0x00
        .align 2
| y, tamaño y enlace, dibujo, x (todo + 128)
objs:
        .word 40 + 128, 0x0F01, 0x6010, 40 + 128     | luz, bajo, en sombra
        .word 40 + 128, 0x0F02, 0xE010, 200 + 128    | luz, alto, en normal
        .word 100 + 128, 0x0F03, 0x6020, 240 + 128   | sombra, bajo, tapado
        .word 40 + 128, 0x0F04, 0xE020, 264 + 128    | sombra, alto
        .word 150 + 128, 0x0F05, 0x2030, 40 + 128    | verde, bajo
        .word 150 + 128, 0x0F06, 0xA030, 80 + 128    | verde, alto
        .word 150 + 128, 0x0F07, 0x0010, 120 + 128   | color 14 de la paleta 0
        .word 152 + 128, 0x0F00, 0x6010, 8 + 128     | luz, bajo
cols:   | ----BBB-GGG-RRR-
        .word 0x0444, 0x0888, 0x000E, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x0E0E, 0
        .word 0, 0x00E0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
        .word 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
        .word 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x0E00, 0x00EE
        .org 0x2000
        .word 0
