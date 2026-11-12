; Hispa Ventanas - variables sin valor inicial que también usa la parte de
; modo real (tienen que estar en los primeros 64 KB del programa)
; ---- driver de pantalla ----
            alignb 4
vid_ancho   resd 1
vid_alto    resd 1
vid_paso    resd 1                  ; octetos por línea de la pantalla
vid_fis     resd 1                  ; dirección física del marco lineal
vid_ptr     resd 1                  ; la pantalla, como puntero cercano
vid_presenta resd 1                 ; presentador del formato en uso
pr_salto_s  resd 1
pr_salto_d  resd 1
pr_n        resd 1
pr_alto     resd 1
vbe_ver     resw 1
op_alto     resw 1
vid_puntos  resw 1
vid_modo    resw 1
vid_tipo    resb 1                  ; 1 VESA, 0 VGA de 16 colores
vid_bits    resb 1                  ; 4, 8, 15, 16, 24 o 32
vid_octetos resb 1                  ; octetos por punto en la pantalla
vid_masc    resb 6                  ; tamaño y posición de rojo, verde y azul
            alignb 4
pal_rgb     resb 1024               ; la paleta: rojo, verde, azul, 0
pal_nat     resd 256                ; cada color en el formato de la pantalla
vbe_info    resb 512
modo_info   resb 256
; ---- dibujo ----
sup_ptr     resd 1                  ; la superficie, como puntero cercano
sup_ancho   resd 1
sup_alto    resd 1
rec_x0      resd 1
rec_y0      resd 1
rec_x1      resd 1
rec_y1      resd 1
gdi_color   resd 1
gdi_negrita resd 1
ln_sx       resd 1
ln_sy       resd 1
ln_dx       resd 1
ln_dy       resd 1
ms_ancho    resd 1
cp_x        resd 1
cp_y        resd 1
cp_salto    resd 1
cp_modo     resd 1
; ---- ratón y cursor ----
rat_x       resd 1
rat_y       resd 1
cur_x       resd 1
cur_y       resd 1
rat_boton   resb 1
cur_puesto  resb 1
cur_img     resb CUR_ANCHO * CUR_ALTO
cur_fondo   resb CUR_ANCHO * CUR_ALTO
; ---- archivos ----
dos_buf     resb 2048 + 16          ; búfer de paso de las llamadas a DOS
