; =============================================================================
;  Hispa-Drive - las variables de la consola (sin valor inicial: van a cero)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
; =============================================================================
        section .bss
        alignb 4
; 4 KB sin usar entre el código y las variables (escribir en la página de
; código que se ejecuta sale carísimo: el procesador cree que el programa se
; cambia a sí mismo)
            resb 4096
; ---- el 68000 (m68k_reiniciar pone a cero de m_regs a m_regs_fin) ----
m_regs:
m_d         resd 8                  ; D0-D7
m_a         resd 8                  ; A0-A7 (A7: la pila que está en uso)
m_osp       resd 1                  ; la otra pila (USP en supervisor, SSP si no)
m_pcbase    resd 1                  ; ESI - m_pcbase = el PC
m_esi       resd 1                  ; ESI entre una vez y otra
m_vf:
m_v         resb 1                  ; V (0 o 1)
m_f         resb 1                  ; N Z C, como el AH de LAHF
m_x         resb 1
m_srh       resb 1                  ; los bits altos del SR: T, S y la máscara
m_parado    resb 1                  ; tras STOP
            alignb 4
m_regs_fin:
m_resto     resd 1
m_aparte    resd 1
m_presup    resd 1                  ; los ciclos que se dieron a m68k_correr
m_ipl       resb 1                  ; la interrupción que se pide (0-7)
m_tas       resb 1                  ; 1: TAS escribe (en la consola no llega)
m_corre     resb 1                  ; 1 mientras corre m68k_correr
            alignb 4
m_pl        resd 256                ; la memoria por trozos de 64 KB: leer,
m_pe        resd 256                ;   escribir
m_px        resd 256                ;   y ejecutar (este nunca es 0)
m_f2ccr     resb 256
m_ccr2f     resb 32
m_unos      resb 256
m_tab       resd 65536              ; la tabla de saltos de las instrucciones

; ---- el Z80 (z80_reiniciar los pone a cero; las mismas que Hispa-System) ----
z_regs:
z_af:
z_f         resb 1
z_a         resb 1
z_bc:
z_c         resb 1
z_b         resb 1
z_de:                               ; (BC y DE seguidos: EXX los cambia de una vez)
z_e         resb 1
z_d         resb 1
z_hl:
z_l         resb 1
z_h         resb 1
z_ix:
z_ixl       resb 1
z_ixh       resb 1
z_iy:
z_iyl       resb 1
z_iyh       resb 1
z_sp        resw 1
z_wz        resw 1                  ; el registro oculto («MEMPTR»)
z_af2       resw 1
z_bc2       resw 1
z_de2       resw 1
z_hl2       resw 1
z_pc        resd 1
z_i         resb 1
z_r         resb 1                  ; (se cuenta entero; el bit 7 va en z_r7)
z_r7        resb 1
z_iff1      resb 1
z_iff2      resb 1
z_im        resb 1
z_halt      resb 1
z_apfase    resb 1                  ; 1: tras EI, una instrucción más y mirar
z_mira      resb 1                  ; un puerto pide mirar las interrupciones
            alignb 4
z_regs_fin:
z_resto     resd 1
z_aparte    resd 1
z_in        resd 1                  ; las rutinas de los puertos
z_out       resd 1
irq_linea   resb 1                  ; la patilla de interrupción (la pide el VDP)
nmi_pend    resb 1
dibujar     resb 1                  ; sms_cuadro pinta la imagen
            alignb 4
pg_lee      resd 65                 ; un puntero por cada KB (y la 0 otra vez)
pg_esc      resd 65                 ; (0: lo hace map_escribe)
tab_sz53    resb 256
tab_sz53p   resb 256
%ifdef TRAZA
traza_fn    resd 1
%endif
; ---- el cartucho y la consola ----
rom         resd 1                  ; la ROM en su orden (la lee el Z80)
rom_tam     resd 1                  ; lo que mide
rom_sw      resd 1                  ; la copia por palabras (la del 68000)
rom_pot     resd 1                  ; su tamaño: una potencia de 2
sram_ini    resd 1                  ; la RAM del cartucho: dónde empieza
sram_fin    resd 1                  ;   y acaba (en el mapa del 68000)
md_ciclos   resd 1                  ; ciclos del 68000 desde que se encendió
th_cuando   resd 2                  ; cuándo cambió TH por última vez
cuadros     resd 1
sram_cab    resb 1                  ; la cabecera dice que hay RAM con pila
sram_on     resb 1                  ; A130F1h: encendida (las ROM grandes)
sram_usada  resb 1                  ; el juego ha escrito en ella
region      resb 1                  ; 80h América, 00h Japón, C0h Europa
region_forzar resb 1                ; (lo pone quien llama) 0 o la región + 1
z80_bus     resb 1                  ; el 68000 ha pedido el bus del Z80
z80_reset   resb 1                  ; el Z80, en reinicio
            alignb 2
z80_banco   resw 1                  ; la ventana de 8000h (9 bits)
io_dat      resb 2                  ; los registros de datos de los mandos
            resb 2
io_ctl      resb 2                  ;   y los de control (qué bits salen)
            resb 2
th_fase     resb 2                  ; cuántas veces ha cambiado TH (6 botones)
            resb 2
pad_md      resw 2                  ; (lo pone quien llama) los botones: bit 0
                                    ; arriba, 1 abajo, 2 izq., 3 der., 4 B,
                                    ; 5 C, 6 A, 7 Start, 8 Z, 9 Y, 10 X, 11 Mode
seis_md     resb 2                  ; (lo pone quien llama) 1: mando de seis botones
            alignb 4
md_tl8      resd 256                ; las rutinas lentas, por trozos de 64 KB
md_tl16     resd 256
md_te8      resd 256
md_te16     resd 256
; ---- el VDP ----
v_reg       resb 32
v_dir       resd 1
v_linea     resd 1                  ; la línea en la que va
v_lineas    resd 1                  ; 262 o 313
v_alto      resd 1                  ; las que se ven: 224 o 240
v_ancho     resd 1                  ; 256 o 320
v_c68       resd 1                  ; los ciclos del 68000 de esta línea
v_cic0      resd 1                  ; los que ya llevaba al empezar el trozo
v_cod       resb 1                  ; la orden (CD5-CD0)
v_pend      resb 1                  ; falta la segunda palabra
v_relleno   resb 1                  ; DMA de relleno: con la siguiente escritura
v_vint      resb 1                  ; interrupción de cuadro pendiente
v_hint      resb 1                  ;   y de línea
v_st        resb 1                  ; el estado: 80h cuadro, 40h demasiados objetos, 20h choque
v_hcnt      resb 1                  ; el contador de líneas
v_frac      resb 1
v_buf       resw 1
            alignb 4
cram        resw 64                 ; 64 colores de 9 bits (----BBB-GGG-RRR-)
vsram       resw 40                 ; el desplazamiento vertical
v_col       resb 64                 ; cada color de la CRAM, en el del equipo
md_col9     resb 512                ; (lo pone quien llama) los 512 de la consola
            alignb 4
r_linea     resd 1                  ; (de paso, al pintar)
r_ancho     resd 1
r_nt        resd 1
r_plano     resd 1
r_x0        resd 1
r_x1        resd 1
r_wx0       resd 1
r_wx1       resd 1
r_cw        resd 1
r_hmask     resd 1
r_px        resd 1
r_sx        resd 1
r_nalto     resd 1
r_smin      resd 1
r_smax      resd 1
r_sat       resd 1
r_smaxn     resd 1
r_spuntos   resd 1
r_stotal    resd 1
r_sn        resd 1
r_sx0vis    resd 1
r_ow        resd 1
r_oh        resd 1
r_ox        resd 1
r_ofy       resd 1
r_ofila     resd 1
r_opal      resd 1
r_altos     resb R_ALTOS * 32
lin         resb 320 + 2 * LIN_M    ; la línea: índices de la CRAM
spr         resb 320 + 2 * LIN_M    ; los objetos de la línea
tab_px      resw 256                ; cada octeto de un dibujo: sus dos puntos
tab_pxr     resw 256                ;   (girado)
tab_mk      resw 256                ;   y su máscara
tab_mkr     resw 256
; ---- el YM2612 ----
ym_ini:
ym_regs     resb 512                ; las dos partes
ym_ta       resd 1
ym_tb       resd 1
ym_cta      resd 1                  ; lo que queda de los temporizadores (16.16)
ym_ctb      resd 1
ym_dir      resb 1
ym_parte    resb 1
ym_t27      resb 1
ym_estado   resb 1                  ; bit 0: A ha dado la vuelta; bit 1: B
ym_lfo      resb 1                  ; el registro 22h
ym_lfo_n    resb 1
ym_dacon    resb 1
            alignb 4
ym_dac      resd 1
ym_c3f      resd 3                  ; el canal 3 en modo especial: A8h, A9h, AAh
ym_c3latch  resd 1
ym_egcnt    resd 1
ym_egacc    resd 1
ym_lfo_fase resd 1
ym_amv      resd 1
ym_amc      resd 1
ym_ch       resb 6 * CH_TAM         ; los seis canales (con sus operadores)
ym_fin:
ym_k        resd 1                  ; (ym_frecuencia) la fase: 2^12 x 53267 / Hz, en 16.16
ym_egpaso   resd 1                  ; las envolventes: pasos por muestra (16.16)
ym_lfo_inc  resd 8
ym_tmp      resd 1
ym_tmpch    resd 1
; ---- el sonido (SN76489) ----
psg_ini:
psg_per     resd 4                  ; los periodos (el 3, el del ruido)
psg_cnt     resd 4                  ; lo que queda de cada uno (en 1/65536 de paso)
psg_sal     resd 4                  ; la salida de cada canal: 0 o 1
psg_vol     resb 4                  ; la atenuación (0-15)
psg_ruido   resb 1                  ; el registro del ruido
psg_reg     resb 1                  ; el último registro elegido (0-7)
            alignb 4
psg_lfsr    resd 1
psg_acc     resd 1                  ; cuántas muestras tocan en esta línea
psg_fin:
psg_hz      resd 1                  ; (lo pone quien llama) muestras por segundo
psg_paso    resd 1                  ; tics del reloj del sonido por muestra (16.16)
psg_n       resd 1                  ; muestras en psg_buf
psg_buf     resw PSG_MAX            ; (16 bits con signo, un canal)
; ---- la memoria ----
            alignb 4
md_ram      resb 0x10000 + 16       ; los 64 KB del 68000 (por palabras al revés)
z80_ram     resb 0x2000             ; los 8 KB del Z80
ym_pag      resb 0x400              ; (lo que lee el Z80 en 4000h-7FFFh)
sram        resb 0x10000            ; la RAM del cartucho
md_nada     resb 0x10000 + 16       ; FFh: donde no hay nada
vram        resb 0x10000 + 16
smd_tmp     resb 0x4000
imagen      resb 320 * 240          ; el cuadro, en los colores del equipo
