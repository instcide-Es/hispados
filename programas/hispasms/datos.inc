; =============================================================================
;  Hispa-System - las variables de la consola (sin valor inicial: van a cero)
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
; =============================================================================
        section .bss
        alignb 4
; Entre el código y las variables, 4 KB sin usar: escribir en la misma
; página de memoria en la que hay código que se ejecuta sale carísimo (el
; procesador, o QEMU, cree que el programa se está cambiando a sí mismo), y
; los registros del Z80 se escriben en cada instrucción.
            resb 4096
; ---- el procesador (z80_reiniciar los pone a cero) ----
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
sram_sucia  resb 1                  ; se ha escrito en la RAM del cartucho
            alignb 4
pg_lee      resd 65                 ; un puntero por cada KB (y la 0 otra vez)
pg_esc      resd 65                 ; (0: lo hace map_escribe)
tab_sz53    resb 256
tab_sz53p   resb 256
%ifdef TRAZA
traza_fn    resd 1
%endif
; ---- el cartucho ----
rom         resd 1                  ; la ROM (su tamaño, redondeado a una potencia de 2)
rom_tam     resd 1                  ; lo que mide de verdad
rom_mask    resd 1                  ; bancos de 16 KB - 1
map_tipo    resd 1                  ; 1 el de Sega, 2 el de Codemasters, 0 RAM plana (CP/M)
map_forzar  resd 1                  ; (lo pone quien llama) 0 averiguarlo, 1 Sega, 2 Codemasters
map_reg     resb 4                  ; FFFC, FFFD, FFFE, FFFF (o 0, 4000h y 8000h)
sram_usada  resb 1                  ; el juego ha encendido alguna vez la RAM del cartucho
            alignb 4
; ---- la imagen (VDP) ----
vdp_reg     resb 16
vdp_dir     resd 1                  ; la dirección (14 bits)
vdp_cod     resb 1                  ; 0 leer VRAM, 1 escribir, 2 registro, 3 paleta
vdp_segundo resb 1                  ; 1: falta el segundo octeto del control
vdp_primero resb 1
vdp_buf     resb 1                  ; lo que se lee del puerto de datos
vdp_st      resb 1                  ; estado: 80h cuadro, 40h más de 8 (o 5º objeto), 20h choque
vdp_linpend resb 1                  ; interrupción de línea pendiente
vdp_cnt     resb 1                  ; el contador de líneas
vdp_vs      resb 1                  ; el desplazamiento vertical, del principio del cuadro
linea       resd 1
cuadros     resd 1
io_ctl      resb 1                  ; el puerto 3Fh (patillas TH y TR)
mem_ctl     resb 1                  ; el puerto 3Eh
pausa_tecla resb 1                  ; (lo pone quien llama) el botón de pausa
pausa_ant   resb 1
            alignb 4
pad_estado  resb 2                  ; (lo pone quien llama) los botones de los dos mandos:
                                    ; bit 0 arriba, 1 abajo, 2 izquierda, 3 derecha,
                                    ; 4 botón 1, 5 botón 2
            resb 2
r_obj       resd 8                  ; (de paso, al pintar) los objetos de la línea
r_nobj      resd 1
r_fino      resd 1
r_nt        resd 1
r_x         resd 1
r_col       resd 1
r_dest      resd 1
r_px        resb 8
r_fondo     resb 1
            alignb 4
r_alto      resd 1
lin_buf     resb 256 + 32           ; la línea: número de color (0-31), +40h si va delante
spr_buf     resb 256 + 32           ; 1 donde ya hay un objeto en la línea
tab_bits    resd 256 * 2            ; cada octeto de un plano, en ocho puntos (0 o 1)
tab_bitsr   resd 256 * 2            ;   y al revés (dibujos girados)
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
; ---- la memoria de la consola (sms_arrancar la pone a cero) ----
            alignb 4
ram:        resb 0x2000
sram        resb 0x8000             ; RAM del cartucho (la de la pila): dos bancos de 16 KB
vram        resb 0x4000
cram        resb 32
ram_fin:
basura      resb 0x400              ; (lo que se escribe en la ROM)
nada        resb 0x400              ; (lo que se lee donde no hay nada: FFh)
imagen      resb 256 * 192          ; el cuadro: colores de la consola (0-63)
