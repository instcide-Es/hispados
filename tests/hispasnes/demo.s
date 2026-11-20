; =============================================================================
;  PRUEBA.SFC: la ROM de muestra de Hispa-Snes (va en JUEGOS\SNES): la
;  pantalla de PPU2 (fondos, objetos, suma de colores, ventana, HDMA) con la
;  melodía de SPCTEST. Propia (GPL), hecha con tools/asm816.py
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
; =============================================================================
TIPO = 0
SRAM = 0
        .include "comun.s"
        .org $008800
principal:
        jsr pinta_ppu2
        jsr espera_listo
        ldx #spc_prog
        ldy #fin_spc_prog-spc_prog
        rep #$20
        .a16
        lda #$0200
        jsr subir
        sep #$20
        .a8
@fin:   bra @fin
nmi:
irq:    rti
        .include "ppu2_cuerpo.s"
        .include "sonido.s"
