; Hispa Ventanas - [FONDO] variables sin valor inicial de fondo.inc (en la
; parte de 32 bits; se ponen a cero al entrar en modo protegido)
; Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
            alignb 4
fon_ptr     resd 1                  ; la imagen lista: un octeto por punto
fon_mango   resd 1                  ; (su bloque de memoria extendida)
fon_an      resd 1                  ; su tamaño
fon_al      resd 1
fon_x       resd 1                  ; dónde va en la pantalla
fon_y       resd 1
fon_serie   resd 1                  ; cambia cada vez que se pone otra
; ---- mientras se carga ----
fon_man     resd 1                  ; el archivo
fon_bpp     resd 1                  ; 8 o 24 bits por punto
fon_san     resd 1                  ; el original: ancho,
fon_sal     resd 1                  ;   alto,
fon_fila    resd 1                  ;   octetos de una fila en el archivo,
fon_sptr    resd 1                  ;   sus puntos (ya con los colores de aquí)
fon_smango  resd 1
fon_lin     resd 1                  ; una fila del archivo
fon_ty      resd 1                  ; la fila que se hace (el difuminado)
fon_rx      resd 1                  ; el trozo del original que se usa
fon_ry      resd 1
fon_rw      resd 1
fon_rh      resd 1
fon_xt      resd 1                  ; dónde empieza cada columna en el original
fon_y0      resd 1                  ; las filas del original de la que se hace
fon_y1      resd 1
fon_sr      resd 1                  ; sumas de rojo, verde y azul de una caja
fon_sg      resd 1
fon_sb      resd 1
fon_hay     resb 1                  ; 1: hay imagen (se pinta ella y no el mosaico)
fon_ajuste  resb 1                  ; FON_AJUSTADA, FON_CENTRADA, FON_MOSAICO...
fon_ajuste_ant resb 1
fon_arriba  resb 1                  ; el BMP va de arriba abajo
fon_cbarras resb 1                  ; el color de lo que la imagen no tapa
            alignb 4
fon_ruta    resb 80                 ; «Fondo=» («»: sin imagen)
fon_ruta_ant resb 80
fon_tmp     resb 168                ; una ruta completa
fon_cab     resb 56                 ; la cabecera del BMP
; Lo que solo hace falta un momento va en bt_buf (bssbarra.inc: el búfer con
; el que barra.inc lee VENTANAS.INI al arrancar, que luego no usa nadie), para
; no gastar más memoria de la del programa (que es convencional):
fon_buf     equ bt_buf              ; VENTANAS.INI, para buscar «Fondo=» (al arrancar)
fon_pal     equ bt_buf              ; la paleta del BMP: rojo, verde, azul, 0 (256 dd)
fon_exacto  equ bt_buf + 1024       ; cada color: su número aquí (FFFFh: no está; 256 dw)
fon_hist    equ bt_buf + 1536       ; cuántas veces sale cada color en el mosaico (256 dw)
%if BT_BUF < 2048 || BT_BUF < FON_INI
%error "bt_buf es pequeño para fondo.inc"
%endif
