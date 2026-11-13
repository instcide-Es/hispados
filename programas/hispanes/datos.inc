; =============================================================================
;  Hispa-Nes - las variables de la consola (sin valor inicial: van a cero)
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
; =============================================================================
        section .bss
        alignb 4
; Entre el código y las variables, 4 KB sin usar: escribir en la misma
; página de memoria en la que hay código que se ejecuta sale carísimo (el
; procesador, o QEMU, cree que el programa se está cambiando a sí mismo), y
; las banderas del 6502 se escriben en casi cada instrucción.
            resb 4096
; ---- el procesador ----
reg_a       resb 1                  ; (A, X, Y y S, seguidos)
reg_x       resb 1
reg_y       resb 1
reg_s       resb 1
reg_pc      resd 1
pcb         resd 1                  ; ESI - pcb = contador de programa
cpu_resto   resd 1                  ; tiempo que sobró del tramo anterior
cpu_aparte  resd 1                  ; tiempo apartado para mirar las interrupciones
f_c         resb 1
f_z         resb 1                  ; Z vale cuando es 0
f_n         resb 1                  ; N es su bit 7
f_v         resb 1
f_i         resb 1                  ; 0 o 4
f_d         resb 1                  ; 0 u 8
nmi_pend    resb 1
cpu_ap_fase resb 1
irq_linea   resb 1                  ; bit 0 sonido, 1 muestras, 2 cartucho
rmw_ant     resb 1
bus         resb 1
sram_sucia  resb 1                  ; se ha escrito en la RAM de la batería
dibujar     resb 1                  ; nes_cuadro pinta la imagen
            alignb 4
pg          resd 9                  ; un puntero por cada 8 KB (y uno de más)
map_ini     resd 1
map_escribir resd 1
map_linea   resd 1
%ifdef TRAZA
traza_fn    resd 1
%endif
; ---- la ROM ----
rom_prg     resd 1                  ; el programa y cuántos bancos de 8 KB
rom_prg_n   resd 1
rom_chr     resd 1                  ; los dibujos y cuántos bancos de 1 KB
rom_chr_n   resd 1
rom_mapa    resd 1
rom_espejo  resd 1
rom_bateria resd 1
rom_entren  resd 1
chr_es_ram  resb 1
            alignb 4
; ---- la imagen ----
ppu_v       resd 1
ppu_t       resd 1
ppu_x       resd 1
linea       resd 1
cuadros     resd 1
s0_punto    resd 1                  ; punto del aviso del objeto 0 (-1: no hay)
punto_fin   resd 1                  ; punto en el que acaba el tramo que corre
ppu_ctrl    resb 1
ppu_mask    resb 1
ppu_status  resb 1
oam_addr    resb 1
ppu_w       resb 1
ppu_buf     resb 1
pad_strobe  resb 1
            resb 1
pad_estado  resb 2                  ; los botones de los dos mandos
pad_reg     resb 2
chr_pg      resd 8                  ; un puntero por cada KB de dibujos
nt_pg       resd 4                  ; las cuatro tablas de nombres
r_base      resd 1                  ; (de paso, al pintar una línea)
r_nt        resd 1
r_fila      resd 1
r_afila     resd 1
r_ash       resd 1
r_n         resd 1
r_alto      resd 1
r_nobj      resd 1
r_obj       resd 8
r_px        resb 8
lin_buf     resb 8 + 256 + 24       ; la línea: números de la paleta (0-31)
spr_buf     resb 256 + 8            ; los objetos de la línea
tab_plano   resb 256 * 8
; ---- el cartucho 1 y el 4 ----
m1_serie    resd 1
m1_cuenta   resd 1
m1_reg      resd 4
m4_reg      resb 8
m4_sel      resb 1
m4_tope     resb 1
m4_cnt      resb 1
m4_rec      resb 1
m4_irq      resb 1
m4_fin:
            alignb 4
; ---- el sonido ----
apu_ini:
can_p1      resb C_TAM
can_p2      resb C_TAM
can_tr      resb C_TAM
can_ru      resb C_TAM
dmc_periodo resd 1
dmc_nivel   resd 1
dmc_dir0    resd 1
dmc_largo0  resd 1
dmc_dir     resd 1
dmc_quedan  resd 1
dmc_reg     resd 1
dmc_bits    resd 1
dmc_fase    resd 1
fc_acc      resd 1
fc_paso     resd 1
mu_acc      resd 1
mu_cc       resd 1
dmc_ctrl    resb 1
dmc_reserva resb 1
dmc_lleno   resb 1
fc_modo     resb 1
            alignb 4
apu_fin:
apu_n       resd 1
apu_buf     resb APU_MAX
; ---- la memoria de la consola (nes_arrancar la pone a cero) ----
            alignb 4
ram:        resb 0x800
            resb 0x1800             ; (por si el programa se sale de la RAM)
sram        resb 0x2000             ; RAM del cartucho (la de la batería)
nt_ram      resb 0x1000             ; las tablas de nombres (dos o cuatro)
pal         resb 32
pal_vis     resb 32                 ; la paleta como se ve
oam         resb 256
chr_ram     resb 0x2000             ; dibujos, si el cartucho no los trae
nada8k      resb 0x2000
ram_fin:
imagen      resb 256 * 240          ; el cuadro: colores de la consola (0-63)
