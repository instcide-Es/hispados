; Hispa Ventanas - variables sin valor inicial de los controles, los diálogos
; y los programas
            alignb 4
; ---- controles ----
est_m       resd 1
bd_agarre   resd 1
pp_buf      resd 1                  ; portapapeles
pp_len      resd 1
ed_an       resd 1
ed_al       resd 1
ed_cols     resd 1
ed_filas    resd 1
ed_tx       resd 1
ed_ty       resd 1
ed_s0       resd 1
ed_s1       resd 1
ed_p        resd 1
ed_pri      resd 1
ed_fila     resd 1
ed_y        resd 1
ed_a        resd 1
ed_b        resd 1
lis_x       resd 1
lis_y       resd 1
ed_car      resb 4
            alignb 4
; ---- programas ----
sisinfo_ven resd 1
bloc_ven    resd 1
bloc_edit   resd 1
calc_ven    resd 1
calc_acum   resd 2
calc_m      resd 4
reloj_ven   resd 1
reloj_cx    resd 1
reloj_cy    resd 1
reloj_r     resd 1
bloc_nombre resb 80
bloc_titulo resb 120
calc_pant   resb 32
reloj_txt   resb 12
calc_op     resb 1
calc_nuevo  resb 1
calc_error  resb 1
calc_neg    resb 1
reloj_h     resb 1
reloj_m     resb 1
reloj_s     resb 1
bloc_busca  resb 44
            alignb 4
; ---- Centro de programas: grupos, accesos directos y CENTRO.INI ----
gr_tabla    resd 1                  ; los grupos (MAX_GR de GR_TAM)
ac_tabla    resd 1                  ; los accesos directos (MAX_AC de AC_TAM)
cfg_buf     resd 1                  ; el texto de CENTRO.INI
cfg_len     resd 1                  ; largo y suma de lo que hay en el disco
cfg_suma    resd 1
cfg_secc    resd 1
cfg_grupo   resd 1
cfg_base    resd 1
cfg_sig     resd 1
cen_ven     resd 1                  ; la ventana del Centro
cen_rect    resd 4
cen_activo  resd 1
vis_cols    resd 1
vis_alto    resd 1
vis_n       resd 1
vis_nom     resd 1
vis_x       resd 1
vis_y       resd 1
arr_acc     resd 1                  ; el acceso que se está arrastrando
bp_grupo    resd 1
bp_ndir     resd 1
bp_nuevos   resd 1
bp_sub      resd 1
bp_n        resd 1
; ---- volumen ----
vol_pct     resd 1                  ; 0-100 (-1: no se sabe)
vol_mudo    resd 1
vol_ven     resd 1
vol_i       resd 1
snd_hay     resb 1
tec_puesto  resb 1                  ; [Hispa-Nes] INT 15h apunta a rm_int15
            alignb 2
snd_ver     resw 1                  ; versión de la API de HSPSND
vol_arr     resb 1
cfg_term    resb 1
vol_txt     resb 24
cfg_ruta    resb 96
dir_propia  resb 80
cen_txt     resb 200
cen_nom     resb 32
cen_arch    resb 100
cen_mapa    resb BP_MAXCAND
cen_lista   resb BP_MAXCAND * 13 + 120
bp_dir      resb 64
bp_dirs     resb BP_MAXDIR * 13
; ---- módulos .HVP ----
            alignb 4
azar_x      resd 1
hvp_vistos  resd 1                  ; los .HVP que había en JUEGOS la última vez
hvi_sig     resd 1
hvi_de      resd HVI_N              ; de qué acceso es cada icono guardado
hvp_tabla   resb HVP_MAX * HS_TAM
hvi_buf     resb HVI_N * 512
; ---- [Configuración] ajustes (ajustes.inc) ----
            alignb 4
aj_rat_zurdo resd 1                 ; 1: botones del ratón intercambiados
aj_ult_act  resd 1                  ; tics del último ratón o teclado
aj_resto    resd 2                  ; lo que sobra del movimiento (x, y)
aj_acc      resb AC_TAM             ; el «acceso directo» con el que se abre CONFIG.HVP
