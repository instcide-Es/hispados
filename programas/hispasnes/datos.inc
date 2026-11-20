; =============================================================================
;  Hispa-Snes - las variables de la consola (sin valor inicial: van a cero)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
; =============================================================================
        section .bss
        alignb 4
; Entre el código y las variables, 4 KB sin usar: escribir en la misma
; página de memoria en la que hay código que se ejecuta sale carísimo (el
; procesador, o QEMU, cree que el programa se está cambiando a sí mismo), y
; las banderas se escriben en casi cada instrucción.
            resb 4096
; ---- el 65C816 ----
r_a         resd 1                  ; A (16 bits; en 8, la parte alta es B)
r_x         resd 1
r_y         resd 1
r_s         resd 1                  ; S (16 bits; en emulación, 01xxh)
r_d         resd 1                  ; D, la página directa
r_dbr       resd 1                  ; el banco de datos, ya << 16
r_pbr       resd 1                  ; el banco del programa, ya << 16
r_pc        resd 1                  ; (fuera de cpu_correr) el contador
pcb         resd 1                  ; ESI - pcb = el contador de programa
d_extra     resd 1                  ; 1 si la parte baja de D no es 0 (un ciclo más)
cpu_romper  resd 1                  ; lo que quedaba al pedir parar
mv_paso     resd 1                  ; MVN/MVP: +1 o -1
mv_org      resd 1                  ;   y el banco de origen << 16
bcd_a       resd 1                  ; el modo decimal
bcd_m       resd 1
bcd_n       resd 1
tmp_rmw     resd 1
f_z         resd 1                  ; Z vale cuando es 0 (16 bits)
f_n         resb 1                  ; N es su bit 7
f_c         resb 1
f_v         resb 1
f_d         resb 1
f_i         resb 1
f_m         resb 1                  ; 1: el acumulador es de 8 bits
f_x         resb 1                  ; 1: los índices son de 8 bits
f_e         resb 1                  ; 1: modo de emulación
bcd_sub     resb 1
int_brk     resb 1
cpu_wai     resb 1
cpu_stp     resb 1
cpu_volver  resb 1                  ; (con cpu_romper) volver al bucle de la consola
nmi_pend    resb 1
irq_linea   resb 1                  ; la línea de IRQ (los temporizadores)
            alignb 4
; ---- la memoria del procesador: un puntero por cada 8 KB (memoria.inc) ----
pag_lee     resd 2048
pag_esc     resd 2048
pag_vacia   resb 0x10000 + 16       ; (código donde no hay memoria: STP)
; ---- el SPC700 ----
s_ya        resd 1                  ; A (bajo) e Y (alto)
s_pc        resd 1
s_dp        resd 1                  ; la página directa: 0 o 100h (P)
s_pres      resd 1                  ; los ciclos dados a spc_correr
spc_t       resd 1                  ; el ciclo del SPC700 (al acabar cada tramo)
s_z         resd 1
s_x         resb 1
s_sp        resb 1
s_n         resb 1
s_c         resb 1
s_v         resb 1
s_h         resb 1
s_i         resb 1
s_b         resb 1
s_tmp       resb 1
s_bit_v     resb 1
s_bit_m     resb 1
s_parado    resb 1                  ; SLEEP o STOP
s_ipl       resb 1                  ; el arranque, en FFC0h
s_plano     resb 1                  ; (las pruebas: sin registros)
s_dsp_dir   resb 1
            alignb 4
s_ent       resb 4                  ; los puertos: lo que escribe el 65C816
s_sal       resb 4                  ;   y lo que escribe el SPC700
s_tim       resb 16 * 3
s_debajo    resb 64                 ; la RAM de debajo del arranque
            alignb 16
aram        resb 0x10000 + 16       ; los 64 KB del sonido
; ---- el bus y el reloj ----
            alignb 4
bus_regs:                           ; (snes_reiniciar los pone a cero)
htime       resd 1
vtime       resd 1
wm_dir      resd 1                  ; la dirección de $2180 (17 bits)
dma_paso    resd 1
hdma_vivos  resd 1                  ; (bits: los canales de HDMA que siguen)
hdma_hace   resd 1                  ;   y los que transfieren en esta línea
irq_h       resd 1                  ; el punto de la IRQ en esta línea
joy_ocup    resd 1                  ; hasta qué línea se leen los mandos
diva        resw 1
joy_serie   resw 2                  ; los mandos uno a uno ($4016/$4017)
rddiv       resw 1                  ; $4214: el cociente
rdmpy       resw 1                  ; $4216: el producto o el resto
joy_res     resb 8                  ; $4218-$421F: los mandos (leídos solos)
nmitimen    resb 1
wrio        resb 1
mpya        resb 1
memsel      resb 1
hdmaen      resb 1
rdnmi       resb 1
timeup      resb 1
en_vblank   resb 1
en_hblank   resb 1
replan      resb 1
campo       resb 1
joy_tira    resb 1
dma_canales resb 1
dma_dato    resb 1
            alignb 4
BUS_REGS_TAM equ $ - bus_regs
linea       resd 1                  ; la línea (0-261)
lineas      resd 1                  ; 262 o 312
vis_fin     resd 1                  ; la primera del borrado vertical
hpos        resd 1                  ; el punto de la línea (ciclos del reloj)
hasta       resd 1
t_linea     resd 1                  ; el reloj al empezar la línea
spc_m       resd 1                  ; hasta dónde va el SPC700 (en el reloj)
spc_resto   resd 1
rel_mul     resd 1                  ; 6 u 8: ciclos del reloj por ciclo
seg_n       resd 1                  ; (el tramo que corre el procesador)
seg_h0      resd 1
dma_m       resd 1                  ; el tiempo del DMA (para el procesador)
cuadros     resd 1
pad         resw 2                  ; los mandos 1 y 2 (como $4218)
dma_reg     resb 8 * 16
dibujar     resb 1
            alignb 4
; ---- la imagen ----
ppu_regs:
oam_recarga resd 1
oam_dir     resd 1
vram_paso   resd 1
mpy         resd 1
fijo15      resd 1
obj_base    resd 1
obj_hueco   resd 1
brillo_p    resd 1
bg_hofs     resw 4
bg_vofs     resw 4
m7hofs      resw 1
m7vofs      resw 1
vram_dir    resw 1
vram_latch  resw 1
m7a         resw 6                  ; M7A, M7B, M7C, M7D, M7X, M7Y
ophct       resw 1
opvct       resw 1
inidisp     resb 1
obsel       resb 1
oam_rot     resb 1
oam_pri     resb 1
oam_latch   resb 1
bgmode      resb 1
mosaic      resb 1
bgsc        resb 4
bgnba       resb 2
ofs_latch   resb 1
m7_latch    resb 1
vmain       resb 1
m7sel       resb 1
cg_dir      resb 1
cg_pri      resb 1
cg_latch    resb 1
cg_pri_l    resb 1
wsel        resb 3
wh          resb 4
wlog        resb 2
tm          resb 1
ts          resb 1
tmw         resb 1
tsw         resb 1
cgwsel      resb 1
cgadsub     resb 1
fijo_r      resb 1
fijo_g      resb 1
fijo_b      resb 1
setini      resb 1
hc_pri      resb 1
vc_pri      resb 1
stat77      resb 1
stat78      resb 1
ppu_abierto resb 1
obj_tam     resb 4
            alignb 4
PPU_REGS_TAM equ $ - ppu_regs
; (lo de pintar una línea)
lin_y       resd 1
modo        resd 1
capas       resd 1
cap_n       resd 1
cap_bpp     resd 1
fl_sc       resd 1
fl_mapa     resd 1
fl_dib      resd 1
fl_16       resd 1
fl_palb     resd 1
fl_z0       resd 1
fl_z1       resd 1
fl_cz       resd 1
fl_mask     resd 1
fl_hofs     resd 1
fl_vofs     resd 1
fl_x        resd 1
fl_col      resd 1
fc_h        resd 1
fc_v        resd 1
fc_y        resd 1
fc_xb       resd 1
fc_ent      resd 1
fc_fila     resd 1
m7_cx       resd 1
m7_cy       resd 1
m7_hx       resd 1
m7_vy       resd 1
m7_x0       resd 1
m7_y0       resd 1
m7_dx       resd 1
m7_dy       resd 1
ob_y        resd 1
ob_prim     resd 1
ob_n        resd 1
ob_zt       resd 1
ob_ox       resd 1
ob_oy       resd 1
ob_t        resd 1
ob_an       resd 1
ob_al       resd 1
ob_fila     resd 1
ob_px       resd 1
ob_lista    resd 32
pc_dest     resd 1
pc_dest15   resd 1
pc_c1       resd 1
pc_c2       resd 1
vmask_m     resd 6                  ; (las dos seguidas: se borran juntas)
vmask_s     resd 6
fl_dir      resb 1
fl_opt      resb 1
fo_pal      resb 1
fo_z        resb 1
m7_z1       resb 1
ob_pal      resb 1
ob_pri      resb 1
pc_negro    resb 1
pc_mitad    resb 1
con_sub     resb 1
hay_colv    resb 1
w_logica    resb 1
ppu_15      resb 1                  ; (las pruebas: la imagen en 15 bits)
            alignb 4
pant_m      resb 256 + 512          ; la principal: la altura y el color
pant_s      resb 256 + 512          ; la secundaria
vmasc       resb 6 * 256            ; las máscaras de las ventanas
m7_lin      resb 256
ob_c        resb 256
ob_p        resb 256
cg_host     resb 256                ; cada color, ya en Hispa Ventanas
cgram_b     resw 256                ;   y con el brillo
cgram       resb 512
oam         resb 544 + 32
brillo_tab  resb 16 * 32
plano_lo    resb 256 * 8
sucio2      resb 4096               ; (las tres seguidas)
sucio4      resb 2048
sucio8      resb 1024
            alignb 16
cache2      resb 4096 * 64
cache4      resb 2048 * 64
cache8      resb 1024 * 64
vram        resb 0x10000 + 64
lut15       resb 32768              ; de 15 bits a la paleta de Hispa Ventanas
imagen      resb 256 * 224
imagen15    resw 256 * 224
; ---- el DSP ----
            alignb 4
dsp_t       resd 1
dsp_n       resd 1
dsp_cuenta  resd 1
eco_pos     resd 1
eco_i       resd 1
ruido       resd 1
brr_filtro  resd 1
mez_l       resd 1
mez_r       resd 1
eco_l       resd 1
eco_r       resd 1
eco_sal_l   resd 1
eco_sal_r   resd 1
snd_sal_n   resd 1                  ; las muestras que ha dejado dsp_cuadro
snd_hz_sal  resd 1                  ; a qué frecuencia (0: ninguna)
snd_fase    resd 1
eco_hist    resd 16
dsp_reg     resb 128
dsp_voz     resb 96 * 8
snd_estereo resb 1
            alignb 4
dsp_buf     resd 1200
snd_sal     resd 2048
; ---- el cartucho ----
rom         resd 1                  ; dónde está la ROM y lo que mide
rom_tam     resd 1
rom_mascara resd 1
mejor_p     resd 1
cab_dir     resd 1
cart_mapa   resd 1
cart_chip   resd 1
cart_pal    resd 1
sram_tam    resd 1
cart_titulo resb 24
pag_tipo    resb 2048
            alignb 16
wram        resb 0x20000 + 16
sram        resb 0x20000 + 16
