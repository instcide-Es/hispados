; Hispa Ventanas - [BARRA] variables sin valor inicial de barra.inc (en la
; parte de 32 bits; se ponen a cero al entrar en modo protegido)
; Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
            alignb 4
area_y0     resd 1                  ; el área de trabajo: de aquí (BARRA_H; en Hispa-486, 0)...
area_y1     resd 1                  ; ...hasta aquí (vid_alto; en Hispa-486, menos la barra)
bt_ven      resd 1                  ; la barra de tareas (0: no hay: el escritorio de Hispa-386)
bt_n        resd 1                  ; ventanas con botón
bt_vis      resd 1                  ; cuántos botones caben
bt_an_bot   resd 1                  ; de un botón al siguiente
bt_x1       resd 1                  ; donde acaban los botones (empieza la bandeja)
bt_fbandeja resd 1                  ; la firma de la bandeja (hora, batería, volumen, red)
bt_lista    resd BT_MAX             ; las ventanas con botón, por orden de llegada
bt_firma    resd BT_MAX             ; lo que se ve de cada una (si cambia, se repinta)
bt_lz_acc   resd 1                  ; el acceso directo que abre la tarea lanzadora
; ---- el menú Inicio ----
mi_ven      resd 1                  ; el menú (0: cerrado)
ms_ven      resd 1                  ; el submenú de un grupo (0: cerrado)
mi_n        resd 1                  ; órdenes del menú
mi_sel      resd 1                  ; la marcada (-1: ninguna)
mi_an       resd 1                  ; ancho de su zona
ms_n        resd 1                  ; programas del submenú
ms_sel      resd 1
ms_grupo    resd 1                  ; la orden (un grupo) que tiene el submenú abierto (-1)
ms_filas    resd 1
ms_colan    resd 1                  ; ancho de una columna del submenú
ms_gr       resd 1                  ; el grupo del submenú
ini_pend    resd 1                  ; lo que queda por hacer en la próxima vuelta (INI_P_*)
ini_dato    resd 1
ini_tic     resd 1                  ; cuándo se puso el ratón en la orden marcada
mi_dato     resd INI_MAX             ; de cada orden: el grupo o el número de la orden fija
mi_y        resd INI_MAX             ; dónde empieza
ms_it       resd INI_SMAX             ; los accesos directos del submenú (0: «(vacío)»)
mi_tipo     resb INI_MAX             ; 0 raya, 1 grupo, 2 orden fija
ini_nivel   resb 1                  ; 0: se maneja el menú; 1: el submenú
ms_mas      resb 1                  ; 1: no cabían todos: la última es «Más...»
bt_win_ant  resb 1                  ; la tecla de Windows, la última vez
bt_ico16    resb 256                ; un icono a mitad de tamaño
; ---- los iconos del escritorio (los accesos de los grupos con «Escritorio=1») ----
            alignb 4
be_n        resd 1                  ; cuántos
be_sel      resd 1                  ; el marcado (el de Intro)
be_tic      resd 1                  ; cuándo se abrió uno (para no abrirlo dos veces)
be_sx       resd 1                  ; (al pintar) la casilla,
be_sy       resd 1
be_w        resd 1                  ;   la etiqueta
be_h        resd 1
be_cnt      resd 1                  ;   y sus letras
be_ty       resd 1                  ; (be_texto2) la y
be_acc      resd BE_MAX             ; los accesos
be_dob      resb 1                  ; la etiqueta, a doble tamaño
be_hecho    resb 1                  ; (be_tecla) la tecla era suya
bt_ico64    resb BE_ICO * BE_ICO    ; un icono a doble tamaño
bt_buf      resb BT_BUF             ; VENTANAS.INI, para buscar «Escritorio=486»
