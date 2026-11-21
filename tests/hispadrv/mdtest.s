| =============================================================================
|  MDTEST.MD - la ROM de prueba de Hispa-Drive (la consola de 16 bits de 1988)
|  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
|
|  Propia: se ensambla con GNU as para el 68000 (binutils-m68k-linux-gnu):
|    m68k-linux-gnu-as --register-prefix-optional -o mdtest.o mdtest.s
|    m68k-linux-gnu-ld -Ttext=0 --oformat binary -o MDTEST.MD mdtest.o
|  Prueba lo de la consola, no el procesador (el 68000 lo prueban las de una
|  instrucción, tests/hispadrv/sst68k.py): el VDP (VRAM, CRAM, VSRAM, las
|  tres DMA, el estado, los contadores), las interrupciones de cuadro y de
|  línea, el Z80 (pedir el bus, su RAM, el reinicio, la ventana al cartucho,
|  sus interrupciones), el YM2612 (su temporizador A), los mandos (de tres y
|  de seis botones) y la RAM del cartucho. Lo que sale va a la pantalla y, en
|  FF0000h, «HDRV» y el texto (lo apunta HISPADRV.LOG con /T). Luego queda
|  una pantalla con todo lo de la imagen (planos, desplazamientos, ventana,
|  objetos girados y con prioridad) y los botones del mando 1 en directo.
| =============================================================================
        .equ VDP_DATA, 0xC00000
        .equ VDP_CTRL, 0xC00004
        .equ VDP_HV,   0xC00008
        .equ PSG,      0xC00011
        .equ Z80_RAM,  0xA00000
        .equ Z80_BUS,  0xA11100
        .equ Z80_RST,  0xA11200
        .equ YM0,      0xA04000
        .equ IO_DATA1, 0xA10003
        .equ IO_CTRL1, 0xA10009
        .equ SRAM,     0x200001
        .equ SALIDA,   0xFF0000         | «HDRV» y el texto
        .equ CUADROS,  0xFF0100         | cuántas interrupciones de cuadro
        .equ LINEAS,   0xFF0104         | cuántas de línea (en total)
        .equ LIN_CUA,  0xFF0108         | de línea en el último cuadro
        .equ LIN_ANT,  0xFF010C
        .equ FALLOS,   0xFF0110
        .equ PUNTERO,  0xFF0114         | por dónde va el texto
        .equ FILA,     0xFF0118         | la fila de la pantalla
        .equ PAD,      0xFF011C
        .equ ARRANQUES, 0xFF0120
        .equ COLUMNA,  0xFF0124

| ---- los vectores ----
        .org 0
        .long 0x00FFFE00, inicio
        .rept 22
        .long excepcion
        .endr
        .long excepcion, excepcion, excepcion, excepcion   | 24-27
        .long hint                       | 28: nivel 4, la de línea
        .long excepcion
        .long vint                       | 30: nivel 6, la de cuadro
        .long excepcion
        .rept 32
        .long excepcion
        .endr

| ---- la cabecera (sin nada de nadie: no hace falta para nuestro emulador) ----
        .org 0x100
        .ascii "HISPA-DOS PRUEBA"
        .ascii "(C) HISPA 2026  "
        .ascii "PRUEBA DE HISPA-DRIVE                           "
        .ascii "PRUEBA DE HISPA-DRIVE                           "
        .ascii "GM 00000000-00"
        .word 0
        .ascii "J6              "
        .long 0, 0x1FFFF, 0xFF0000, 0xFFFFFF
        .ascii "RA"
        .byte 0xF8, 0x20
        .long 0x200001, 0x203FFF
        .org 0x1F0
        .ascii "JUE             "

| =============================================================================
        .org 0x200
inicio:
        move.w #0x2700, sr
        lea 0xFFFE00, sp
        lea SALIDA, a0                   | la RAM, a cero
        move.w #0x3FFF, d0
1:      clr.l (a0)+
        dbra d0, 1b
        move.l #0x48445256, SALIDA       | «HDRV»
        move.l #SALIDA+4, PUNTERO
        moveq #1, d0
        bsr pon_fila
        lea VDP_CTRL, a5
        lea VDP_DATA, a4
        move.w (a5), d0                  | (lee el estado: sin orden a medias)
        lea regs_vdp(pc), a0
        move.w #0x8000, d1
        moveq #18, d2
2:      move.b (a0)+, d1
        move.w d1, (a5)
        add.w #0x100, d1
        dbra d2, 2b
        bsr limpia_vram
        bsr paleta
        bsr letras
        lea t_titulo(pc), a0
        bsr escribe
        bsr fila_sig

| ---- 1. la VRAM, la CRAM y la VSRAM: escribir y leer ----
        lea t_vram(pc), a0
        bsr escribe
        moveq #0, d7
        move.l #0x40000000, (a5)         | VRAM 0000h, escribir
        move.w #0x1234, (a4)
        move.w #0xABCD, (a4)
        move.l #0x00000000, (a5)         | VRAM 0000h, leer
        move.w (a4), d0
        cmp.w #0x1234, d0
        beq 3f
        moveq #1, d7
3:      move.w (a4), d0
        cmp.w #0xABCD, d0
        beq 3f
        moveq #2, d7
3:      move.l #0xC01E0000, (a5)         | CRAM 15 (1Eh), escribir
        move.w #0x0EEE, (a4)
        move.l #0x001E0020, (a5)         | CRAM 15, leer
        move.w (a4), d0
        cmp.w #0x0EEE, d0
        beq 3f
        moveq #3, d7
3:      move.l #0x40020010, (a5)         | VSRAM 2, escribir
        move.w #0x0123, (a4)
        move.l #0x00020010, (a5)
        move.w (a4), d0
        cmp.w #0x0123, d0
        beq 3f
        moveq #4, d7
3:      move.l #0x40020010, (a5)
        move.w #0, (a4)
        move.l #0x40000000, (a5)         | (el dibujo 0, otra vez transparente)
        move.l #0, (a4)
        bsr resultado

| ---- 2. las DMA: relleno, copia y del 68000 ----
        lea t_dma(pc), a0
        bsr escribe
        moveq #0, d7
        move.w #0x8F01, (a5)             | de uno en uno
        move.w #0x9300, (a5)             | 256 octetos
        move.w #0x9401, (a5)
        move.w #0x9780, (a5)             | relleno
        move.l #0x50000080, (a5)         | VRAM 1000h, con DMA
        move.w #0x5A00, (a4)
        move.w #0x8F02, (a5)
        move.l #0x10800000, (a5)         | VRAM 1080h, leer
        move.w (a4), d0
        cmp.w #0x5A5A, d0
        beq 3f
        moveq #1, d7
3:      move.w #0x8F01, (a5)             | copia: 1000h -> 2000h, 256
        move.w #0x9300, (a5)
        move.w #0x9401, (a5)
        move.w #0x9500, (a5)
        move.w #0x9610, (a5)
        move.w #0x97C0, (a5)
        move.l #0x200000C0, (a5)         | VRAM 2000h, copia
        move.w #0x8F02, (a5)
        move.l #0x20800000, (a5)
        move.w (a4), d0
        cmp.w #0x5A5A, d0
        beq 3f
        moveq #2, d7
3:      lea tabla_dma, a0                | del 68000: 32 palabras a 3000h
        move.l a0, d0
        lsr.l #1, d0
        move.w #0x9320, (a5)
        move.w #0x9400, (a5)
        move.w d0, d1
        and.w #0xFF, d1
        or.w #0x9500, d1
        move.w d1, (a5)
        lsr.l #8, d0
        move.w d0, d1
        and.w #0xFF, d1
        or.w #0x9600, d1
        move.w d1, (a5)
        lsr.l #8, d0
        and.w #0x7F, d0
        or.w #0x9700, d0
        move.w d0, (a5)
        move.l #0x70000080, (a5)         | VRAM 3000h, con DMA
        move.l #0x303E0000, (a5)         | la última palabra: 303Eh
        move.w (a4), d0
        cmp.w #0x1F1F, d0
        beq 3f
        moveq #3, d7
3:      bsr resultado

| ---- 3. las interrupciones de cuadro y de línea ----
        lea t_int(pc), a0
        bsr escribe
        moveq #0, d7
        move.w #0x8174, (a5)             | imagen, cuadro y DMA
        move.w #0x8A63, (a5)             | de línea: cada 100 líneas
        move.w #0x8014, (a5)
        move.w #0x2000, sr
        moveq #10, d0
        bsr espera
        move.l CUADROS, d0
        cmp.l #10, d0
        bcc 3f
        moveq #1, d7
3:      move.l LIN_ANT, d0               | (las del cuadro de antes: 99 y 199)
        cmp.l #2, d0
        beq 3f
        moveq #2, d7
3:      move.w VDP_HV, d0                | el contador de líneas avanza
        moveq #0, d1
        move.w #500, d2
4:      move.w VDP_HV, d1
        cmp.w d0, d1
        bne 5f
        dbra d2, 4b
        moveq #3, d7
5:      move.w (a5), d0                  | el estado: el bit de «cuadro» (en el borde)
        bsr resultado

| ---- 4. el Z80: su RAM, su programa y sus interrupciones ----
        lea t_z80(pc), a0
        bsr escribe
        moveq #0, d7
        move.w #0x100, Z80_BUS           | el bus...
        move.w #0x000, Z80_RST           | ...y en reinicio
4:      btst #0, Z80_BUS
        bne 4b
        lea z80_prog, a0
        lea Z80_RAM, a1
        move.w #z80_fin-z80_prog-1, d0
1:      move.b (a0)+, (a1)+
        dbra d0, 1b
        move.b #0x77, Z80_RAM+0x1000     | (lo pisa el Z80)
        move.w #0x100, Z80_RST           | en marcha
        move.w #0, Z80_BUS
        moveq #8, d0
        bsr espera
        move.w #0x100, Z80_BUS
4:      btst #0, Z80_BUS
        bne 4b
        cmp.b #0x42, Z80_RAM+0x1000
        beq 3f
        moveq #1, d7
3:      cmp.b #0xC3, Z80_RAM+0x1001      | el octeto 10000h del cartucho
        beq 3f
        moveq #2, d7
3:      move.b Z80_RAM+0x1002, d0        | el YM: el temporizador A ha avisado
        and.b #1, d0
        bne 3f
        moveq #3, d7
3:      cmp.b #3, Z80_RAM+0x1003         | interrupciones del VDP
        bcc 3f
        moveq #4, d7
3:      move.w #0, Z80_BUS
        bsr resultado

| ---- 5. el YM2612 desde el 68000 ----
        lea t_ym(pc), a0
        bsr escribe
        moveq #0, d7
        move.w #0x100, Z80_BUS           | (el Z80, quieto: el bus es nuestro)
        move.w #0x000, Z80_RST
4:      btst #0, Z80_BUS
        bne 4b
        move.b #0x27, YM0
        move.b #0x30, YM0+1              | los avisos, quitados
        move.b YM0, d0
        and.b #3, d0
        beq 3f
        moveq #1, d7
3:      move.b #0x26, YM0                | el temporizador B, lo más rápido
        move.b #0xFF, YM0+1
        move.b #0x27, YM0
        move.b #0x0A, YM0+1              | B en marcha, con aviso
        moveq #2, d0
        bsr espera
        move.b YM0, d0
        and.b #2, d0
        bne 3f
        moveq #2, d7
3:      move.b #0x27, YM0
        move.b #0x30, YM0+1
        bsr resultado

| ---- 6. los mandos (lo que dé el guion de la prueba) ----
        lea t_mando(pc), a0
        bsr escribe
        moveq #0, d7
        move.b #0x40, IO_CTRL1           | TH, de salida
        move.b #0x40, IO_DATA1
        nop
        nop
        move.b IO_DATA1, d0              | TH=1: ?1CBRLDU
        move.b #0x00, IO_DATA1
        nop
        nop
        move.b IO_DATA1, d1              | TH=0: ?0SA00DU
        lsl.w #8, d0
        move.b d1, d0
        move.w d0, d6
        bsr hex4
        move.b #0x20, d0
        bsr letra
        | seis botones: TH arriba, un rato quieto (vuelve a empezar la cuenta)
        | y 1 0 1 0 1 0 1: la tercera vez abajo y la tercera vez arriba
        move.b #0x40, IO_DATA1
        moveq #1, d0
        bsr espera
        move.b #0x40, IO_DATA1
        move.b #0x00, IO_DATA1
        move.b #0x40, IO_DATA1
        move.b #0x00, IO_DATA1
        move.b #0x40, IO_DATA1
        move.b #0x00, IO_DATA1
        move.b IO_DATA1, d1              | (la tercera abajo: ?0SA0000)
        move.b #0x40, IO_DATA1
        move.b IO_DATA1, d0              | ?1CBMXYZ
        move.b #0x00, IO_DATA1
        lsl.w #8, d0
        move.b d1, d0
        bsr hex4
        move.b #0x40, IO_DATA1
        bsr fila_sig

| ---- 7. la RAM del cartucho ----
        lea t_sram(pc), a0
        bsr escribe
        moveq #0, d7
        move.b #0x5A, SRAM
        move.b #0xA5, SRAM+2
        cmp.b #0x5A, SRAM
        beq 3f
        moveq #1, d7
3:      cmp.b #0xA5, SRAM+2
        beq 3f
        moveq #2, d7
3:      move.b SRAM+4, d0                | las veces que se ha encendido
        addq.b #1, d0
        move.b d0, SRAM+4
        moveq #0, d1
        move.b d0, d1
        move.l d1, ARRANQUES
        bsr resultado
        lea t_arranques(pc), a0
        bsr escribe
        move.l ARRANQUES, d0
        bsr hex4
        bsr fila_sig

| ---- 8. una nota FM: el canal 1, solo el operador 4 (algoritmo 7), un La
| de 440 Hz (F=1083, bloque 4), y el SN76489 callado; se queda sonando ----
        move.b #0x9F, PSG                | el SN76489, callado
        move.b #0xBF, PSG
        move.b #0xDF, PSG
        move.b #0xFF, PSG
        move.w #0x100, Z80_BUS
4:      btst #0, Z80_BUS
        bne 4b
        lea nota_fm(pc), a0
5:      move.b (a0)+, d0
        beq 6f
        move.b d0, YM0
        move.b (a0)+, YM0+1
        bra 5b
6:
| ---- el final: «FIN» con los fallos y la pantalla de la imagen ----
        lea t_fin(pc), a0
        bsr escribe
        move.l FALLOS, d0
        bsr hex4
        bsr fila_sig
        movea.l PUNTERO, a0
        clr.b (a0)
        bsr pantalla
bucle:
        moveq #1, d0
        bsr espera
        bsr lee_mando
        moveq #26, d0
        bsr pon_fila
        lea t_pad(pc), a0
        bsr escribe_sin
        move.w PAD, d0
        bsr hex4_sin
        bra bucle

| =============================================================================
|  Las rutinas
| =============================================================================
excepcion:
        rte

vint:
        addq.l #1, CUADROS
        move.l LIN_CUA, LIN_ANT
        clr.l LIN_CUA
        rte
hint:
        addq.l #1, LINEAS
        addq.l #1, LIN_CUA
        rte

| D0=cuadros que esperar
espera:
        move.l CUADROS, d1
        add.l d0, d1
        move.l #300000, d2
1:      cmp.l CUADROS, d1
        bls 2f
        subq.l #1, d2
        bne 1b
2:      rts

limpia_vram:
        move.l #0x40000000, (a5)
        move.w #0x7FFF, d0
1:      move.w #0, (a4)
        dbra d0, 1b
        rts

paleta:
        move.l #0xC0000000, (a5)
        lea colores(pc), a0
        moveq #47, d0
1:      move.w (a0)+, (a4)
        dbra d0, 1b
        rts

| las letras, en los dibujos 20h-5Ah (el 0 queda transparente)
letras:
        move.l #0x44000000, (a5)         | 20h * 32 = 400h
        lea letras_bin, a0
        move.w #(59*32/2)-1, d0
1:      move.w (a0)+, (a4)
        dbra d0, 1b
        rts

| A0=texto: lo escribe en la pantalla (donde va COLUMNA) y en SALIDA
escribe:
        move.l a0, -(sp)
        bsr escribe_sin
        movea.l (sp)+, a0
        movem.l d0/a1, -(sp)
        movea.l PUNTERO, a1
1:      move.b (a0)+, d0
        beq 2f
        move.b d0, (a1)+
        bra 1b
2:      move.l a1, PUNTERO
        movem.l (sp)+, d0/a1
        rts
| A0=texto, solo en la pantalla
escribe_sin:
        move.l d0, -(sp)
1:      move.b (a0)+, d0
        beq 2f
        bsr letra_sin
        bra 1b
2:      move.l (sp)+, d0
        rts
| D0=fila: COLUMNA, al principio de esa fila (columna 1)
pon_fila:
        move.w d0, FILA
        move.l d0, -(sp)
        lsl.w #7, d0                     | 64 casillas x 2 octetos
        add.w #0xC002, d0
        move.w d0, COLUMNA
        move.l (sp)+, d0
        rts
| la dirección de escritura de la VRAM: COLUMNA
pon_dir:
        movem.l d0-d1, -(sp)
        moveq #0, d0
        move.w COLUMNA, d0
        move.l d0, d1
        and.w #0x3FFF, d0
        or.w #0x4000, d0
        swap d0
        rol.w #2, d1
        and.w #3, d1
        move.w d1, d0
        move.l d0, (a5)
        movem.l (sp)+, d0-d1
        rts
| D0=letra: a la pantalla y a SALIDA
letra:
        movea.l PUNTERO, a1
        move.b d0, (a1)+
        move.l a1, PUNTERO
letra_sin:
        move.l d0, -(sp)
        bsr pon_dir
        and.w #0xFF, d0
        cmp.b #0x20, d0
        bcc 1f
        moveq #0x20, d0
1:      move.w d0, (a4)
        addq.w #2, COLUMNA
        move.l (sp)+, d0
        rts
| D0 (16 bits) en hexadecimal
hex4:
        movem.l d0-d3, -(sp)
        move.w d0, d2
        moveq #3, d3
1:      rol.w #4, d2
        move.w d2, d0
        and.w #15, d0
        move.b cifras(pc,d0.w), d0
        bsr letra
        dbra d3, 1b
        movem.l (sp)+, d0-d3
        rts
hex4_sin:
        movem.l d0-d3, -(sp)
        move.w d0, d2
        moveq #3, d3
1:      rol.w #4, d2
        move.w d2, d0
        and.w #15, d0
        move.b cifras(pc,d0.w), d0
        bsr letra_sin
        dbra d3, 1b
        movem.l (sp)+, d0-d3
        rts
cifras: .ascii "0123456789ABCDEF"

| D7=0: «BIEN»; si no, «MAL» y el número
resultado:
        tst.w d7
        bne 1f
        lea t_bien(pc), a0
        bsr escribe
        bra fila_sig
1:      lea t_mal(pc), a0
        bsr escribe
        move.b #0x30, d0
        add.b d7, d0
        bsr letra
        addq.l #1, FALLOS
fila_sig:
        move.b #0x0A, d0                 | (en SALIDA, un salto de línea)
        movea.l PUNTERO, a1
        move.b d0, (a1)+
        move.l a1, PUNTERO
        move.l d0, -(sp)
        move.w FILA, d0
        addq.w #1, d0
        bsr pon_fila
        move.l (sp)+, d0
        rts

lee_mando:
        move.b #0x40, IO_DATA1
        nop
        nop
        move.b IO_DATA1, d0
        move.b #0x00, IO_DATA1
        nop
        nop
        move.b IO_DATA1, d1
        move.b #0x40, IO_DATA1
        lsl.w #8, d0
        move.b d1, d0
        not.w d0
        and.w #0x3F33, d0
        move.w d0, PAD
        rts

| ---- la pantalla de la imagen: el plano B con un damero desplazado por
| líneas, la ventana arriba (dos filas), objetos girados, uno de ellos de
| prioridad alta encima de todo y otro baja detrás del plano A alto ----
pantalla:
        | dos dibujos para el damero: 5Bh (color 2) y 5Ch (color 3)
        move.l #0x4B600000, (a5)         | 5Bh * 32 = B60h
        move.w #7, d0
1:      move.l #0x22222222, (a4)
        dbra d0, 1b
        move.w #7, d0
1:      move.l #0x33333333, (a4)
        dbra d0, 1b
        | el damero en el plano B (E000h, 64x32)
        move.l #0x60000003, (a5)         | VRAM E000h
        move.w #31, d1
2:      move.w #31, d0
3:      move.w d1, d2
        add.w d0, d2
        and.w #1, d2
        add.w #0x5B, d2
        or.w #0x2000, d2                 | paleta 1
        move.w d2, (a4)
        move.w d2, (a4)
        dbra d0, 3b
        dbra d1, 2b
        | el desplazamiento por líneas: una onda
        move.w #0x8B03, (a5)
        move.l #0x5C000003, (a5)         | DC00h
        move.w #223, d0
        moveq #0, d1
4:      move.w d1, d2
        and.w #31, d2
        cmp.w #16, d2
        bcs 5f
        neg.w d2
        add.w #32, d2
5:      move.w #0, (a4)                  | A: quieto
        move.w d2, (a4)                  | B: la onda
        addq.w #1, d1
        dbra d0, 4b
        | la ventana: las dos primeras filas (registro 18), con el título
        move.w #0x9202, (a5)
        move.l #0x70000002, (a5)         | B000h
        lea t_ventana(pc), a0
        move.w #63, d1
6:      moveq #0, d0
        move.b (a0)+, d0
        bne 7f
        subq.l #1, a0
        moveq #0x20, d0
7:      or.w #0xC000, d0                 | prioridad alta, paleta 2
        move.w d0, (a4)
        dbra d1, 6b
        | los objetos: cuatro de 2x2 casillas con la letra «R» (52h)
        move.l #0x58000003, (a5)         | D800h
        lea objetos(pc), a0
        moveq #4*4-1, d0
8:      move.w (a0)+, (a4)
        dbra d0, 8b
        | en una casilla del plano A, prioridad alta (tapa al objeto 3)
        move.w #0xC000+(10*128)+60, COLUMNA
        bsr pon_dir
        move.w #0x8000+0x5C, (a4)
        move.w #0x8000+0x5C, (a4)
        rts

        .align 2
regs_vdp:
        .byte 0x04, 0x14, 0x30, 0x2C, 0x07, 0x6C, 0x00, 0x00
        .byte 0x00, 0x00, 0xFF, 0x00, 0x81, 0x37, 0x00, 0x02
        .byte 0x01, 0x00, 0x00
        .align 2
colores:
        .word 0x0000, 0x0EEE, 0x0E00, 0x00E0, 0x000E, 0x0EE0, 0x0E0E, 0x00EE
        .word 0x0888, 0x0444, 0x0A60, 0x006A, 0x0A06, 0x0246, 0x0642, 0x0EEE
        .word 0x0000, 0x0EE0, 0x0420, 0x0640, 0x0000, 0x0000, 0x0000, 0x0000
        .word 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
        .word 0x0000, 0x00EE, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
        .word 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
| los objetos: y, tamaño y enlace, dibujo, x
objetos:
        .word 128+120, 0x0501, 0x0052, 128+40     | 2x2, al derecho
        .word 128+120, 0x0502, 0x0852, 128+80     | girado en horizontal
        .word 128+120, 0x0503, 0x1052, 128+120    | girado en vertical
        .word 128+76,  0x0500, 0x0052, 128+232    | detrás de la casilla alta

| registro, valor (y 0 al final): el canal 1 (parte 1, canal 0)
nota_fm:
        .byte 0x22, 0x00, 0x27, 0x00, 0x28, 0x00, 0x2B, 0x00
        .byte 0xB0, 0x07, 0xB4, 0xC0
        .byte 0x30, 0x01, 0x34, 0x01, 0x38, 0x01, 0x3C, 0x01      | MUL 1
        .byte 0x40, 0x7F, 0x44, 0x7F, 0x48, 0x7F, 0x4C, 0x00      | solo el 4
        .byte 0x50, 0x1F, 0x54, 0x1F, 0x58, 0x1F, 0x5C, 0x1F      | AR 31
        .byte 0x60, 0x00, 0x64, 0x00, 0x68, 0x00, 0x6C, 0x00
        .byte 0x70, 0x00, 0x74, 0x00, 0x78, 0x00, 0x7C, 0x00
        .byte 0x80, 0x0F, 0x84, 0x0F, 0x88, 0x0F, 0x8C, 0x0F
        .byte 0xA4, 0x24, 0xA0, 0x3B                               | bloque 4, F=43Bh
        .byte 0x28, 0xF0                                           | pulsada
        .byte 0, 0
t_titulo:    .asciz "HISPA-DRIVE: PRUEBA DE LA CONSOLA"
t_ventana:   .asciz " HISPA-DRIVE  VENTANA FIJA                 "
t_vram:      .asciz "VRAM CRAM VSRAM: "
t_dma:       .asciz "DMA: "
t_int:       .asciz "INTERRUPCIONES: "
t_z80:       .asciz "Z80: "
t_ym:        .asciz "YM2612: "
t_mando:     .asciz "MANDO: "
t_sram:      .asciz "RAM DEL CARTUCHO: "
t_arranques: .asciz "ENCENDIDO: "
t_fin:       .asciz "FIN. FALLOS: "
t_bien:      .asciz "BIEN"
t_mal:       .asciz "MAL "
t_pad:       .asciz "BOTONES: "
        .align 2
tabla_dma:
        .word 0x0000, 0x0101, 0x0202, 0x0303, 0x0404, 0x0505, 0x0606, 0x0707
        .word 0x0808, 0x0909, 0x0A0A, 0x0B0B, 0x0C0C, 0x0D0D, 0x0E0E, 0x0F0F
        .word 0x1010, 0x1111, 0x1212, 0x1313, 0x1414, 0x1515, 0x1616, 0x1717
        .word 0x1818, 0x1919, 0x1A1A, 0x1B1B, 0x1C1C, 0x1D1D, 0x1E1E, 0x1F1F
letras_bin:
        .incbin "letras.bin"
        .align 2
z80_prog:
        .incbin "z80prueba.bin"
z80_fin:
        .align 2
| el octeto 10000h: C3h (lo lee el Z80 por su ventana)
        .org 0x10000
        .byte 0xC3, 0x00
        .org 0x1FFFE
        .word 0
