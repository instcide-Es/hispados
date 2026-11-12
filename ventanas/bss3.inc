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
