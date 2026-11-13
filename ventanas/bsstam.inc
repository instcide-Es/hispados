; Hispa Ventanas - [TAMAÑO] variables sin valor inicial de tamano.inc (en la
; parte de 32 bits; se ponen a cero al entrar en modo protegido)
            alignb 4
; ---- el cursor: sus formas ----
cur_imgs    resb CUR_N * CUR_ANCHO * CUR_ALTO
cur_img_p   resd 1                  ; el dibujo de la forma puesta
cur_hx      resd 1                  ; su punta
cur_hy      resd 1
cur_forma_v resd 1                  ; la forma puesta (CUR_*)
cur_duena   resd 1                  ; la ventana que ha pedido una forma (cur_forma)
cur_pedida  resd 1                  ;   y cuál
; ---- mover y cambiar el tamaño ----
arr_min_an  resd 1                  ; el tamaño mínimo de la que se arrastra
arr_min_al  resd 1
arr_ux      resd 1                  ; el último rectángulo puesto
arr_uy      resd 1
arr_uan     resd 1
arr_ual     resd 1
arr_ian     resd 1                  ; el tamaño al empezar
arr_ial     resd 1
con_x       resd 1                  ; el contorno
con_y       resd 1
con_an      resd 1
con_al      resd 1
con_buf     resb 4 * CON_MAX * CON_G ; lo que tapan sus cuatro tiras
bg_largo    resd 1                  ; medidas de la barra (bar_geo)
bg_pista    resd 1
bg_t0       resd 1
bg_tl       resd 1
vt_x        resd 1                  ; la vista de iconos (vista_disponer)
vt_y        resd 1
vt_an       resd 1
vt_al       resd 1
vt_max      resd 1
bloc_hbar   resd 1                  ; la barra horizontal del Bloc de notas
con_op      resb 1
con_puesto  resb 1
con_activo  resb 1                  ; se está arrastrando solo el contorno
vel_medido  resb 1
vel_lento   resb 1                  ; la pantalla es lenta: solo el contorno
aj_contorno resb 1                  ; AJ_CONTORNO
hay_rueda   resb 1
            alignb 4
