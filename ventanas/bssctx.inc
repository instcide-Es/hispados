; Hispa Ventanas - [CONTEXTO] variables sin valor inicial de contexto.inc (en
; la parte de 32 bits; se ponen a cero al entrar en modo protegido)
            alignb 4
ctx_gris    resd 1                  ; las órdenes en gris del emergente abierto (un bit cada una)
ctx_gris_q  resd 1                  ; (ctx_menu) las que se piden
ctx_qx      resd 1                  ; (ctx_menu) dónde se pide
ctx_qy      resd 1
ctx_px      resd 1                  ; dónde se ha pedido el último menú (pantalla)
ctx_py      resd 1
ctx_sx      resd 1                  ; (ctx_sistema)
ctx_sy      resd 1
ctx_lp      resd 1                  ; la lista que se está haciendo: por dónde va,
ctx_lgris   resd 1                  ;   las que van en gris
ctx_ln      resd 1                  ;   y cuántas lleva
ctx_mn      resd 1                  ; (mosaico) ventanas, columnas, filas y casilla
ctx_mcols   resd 1
ctx_mfilas  resd 1
ctx_man     resd 1
ctx_mal     resd 1
ctx_activo  resb 1                  ; 1: el desplegable abierto es un emergente
ctx_sis     resb 1                  ; 1: y es el menú de la ventana (órdenes SC_*)
ctx_q_sis   resb 1                  ; (ctx_sistema -> ctx_menu)
ctx_q_sis2  resb 1
ctx_der_menu resb 1                 ; el botón derecho se apretó en un menú
ctx_ctrl    resb 1                  ; 1: un Ctrl+clic en marcha (el izquierdo hace de derecho)
ctx_tmenu   resb 1                  ; la tecla de menú, la última vez
aj_ctrl_no  resb 1                  ; 1: Ctrl+clic NO es el botón derecho (AJ_CTRL_DER = 0)
ctx_nom     resb 32                 ; el nombre nuevo de un acceso
ctx_lista   resb 160                ; el menú de la ventana
            alignb 4
ctx_acc     resb AC_TAM             ; el acceso con que se abre Configuración
