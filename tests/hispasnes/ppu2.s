; =============================================================================
;  PPU2: modo 1 con casi todo: tres fondos (dibujos de 4 y 2 bits, volteados,
;  paletas, prioridades, el fondo 3 delante), objetos de 8x8 y 16x16 (con
;  volteo, x negativa, prioridad detrás de un fondo y paleta que suma), la
;  suma de colores a la mitad con la pantalla secundaria, una ventana que
;  tapa el fondo 1 y HDMA que ondula el fondo 1 en la parte de abajo
; =============================================================================
TIPO = 0
SRAM = 0
        .include "comun.s"
        .org $008800
principal:
        jsr pinta_ppu2
@fin:   bra @fin
nmi:
irq:    rti
        .include "ppu2_cuerpo.s"
