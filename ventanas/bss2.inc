; Hispa Ventanas - variables sin valor inicial de las capas de 32 bits (van
; detrás de la pila; se ponen a cero al entrar en modo protegido)
            alignb 4
; ---- driver de pantalla (lo que no usa la parte de modo real) ----
pal_rgb     resb 1024               ; la paleta: rojo, verde, azul, 0
pal_nat     resd 256                ; cada color en el formato de la pantalla
; ---- cursor del ratón (el dibujo y lo que tapa) ----
cur_fondo   resb CUR_ANCHO * CUR_ALTO    ; [TAMAÑO] (los dibujos, en bsstam.inc)
            alignb 4
; ---- núcleo ----
mon_mango   resd 1
mon_ini     resd 1
tar_tabla   resd 1
tar_act     resd 1
cero        resd 1                  ; puntero a la dirección lineal 0
temporizadores resb TMP_MAX * 16
; ---- regiones y contexto ----
reg_n       resd 1
rr_x0       resd 1
rr_y0       resd 1
rr_x1       resd 1
rr_y1       resd 1
pres_x0     resd 1                  ; lo que hay que llevar a la pantalla
pres_y0     resd 1
pres_x1     resd 1
pres_y1     resd 1
dc_ox       resd 1
dc_oy       resd 1
reg         resb REG_MAX * 16
reg2        resb REG_MAX * 16
ico_tmp     resb 32 * 32
; ---- gestor ----
ven_tabla   resd 1
n_orden     resd 1
orden       resd MAX_TOPE           ; de la del fondo a la de delante
activa      resd 1
foco        resd 1
captura     resd 1
escritorio  resd 1
arr_ven     resd 1
arr_dx      resd 1
arr_dy      resd 1
nc_zona     resd 1
pan_x       resd 1                  ; el cursor, en la pantalla
pan_y       resd 1
dbl_tics    resd 1
dbl_x       resd 1
dbl_y       resd 1
ult_si      resw 1
ult_di      resw 1
arrastre    resb 1                  ; 1 mover, 2 tamaño, 3 botón del título
arr_bordes  resb 1
nc_apretado resb 1
pan_puesto  resb 1
rat_doble   resb 1
tec_mayus   resb 1
sal_armado  resb 1
ev_movido   resb 1
rat_der     resb 1                  ; el botón derecho está apretado
cap_der     resb 1                  ; la captura es del botón derecho
actividad   resb 1
            alignb 4
; ---- menús ----
menu_ven    resd 1
menu_duena  resd 1
menu_idx    resd 1
menu_sel    resd 1
menu_items  resd 1
menu_n      resd 1
menu_plant  resb P_TAM
; ---- escritorio ----
plant_tmp   resb P_TAM
mosaico     resb 256
t_pantalla  resb 64
t_video     resb 64
t_dpmi      resb 64
mem_info    resb 0x30
