; =============================================================================
;  Hispa-BASIC - el segmento de datos
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  BASIC.COM pide al sistema un segmento de 64 K (o lo que haya) y lo usa
;  con DS = ES = SS. Aquí están las variables del intérprete, las memorias
;  de paso, la pila y, desde PROG_INI, lo del usuario:
;
;     PROG_INI   el programa (líneas tokenizadas y enlazadas)
;     v_vars     las variables simples
;     v_mats     las matrices
;     v_libre    lo libre (hasta v_cads)
;     v_cads     las cadenas, de arriba abajo, hasta v_cima
;
;  Lo constante (tablas, mensajes) va en el segmento del código y se lee con
;  CS: delante.
; =============================================================================

; ---- los tipos (los mismos números que usa GW-BASIC) ----
T_ENT       equ 2                   ; entero, 16 bits
T_CAD       equ 3                   ; cadena (descriptor de 3 octetos)
T_SIM       equ 4                   ; simple precisión (4 octetos MBF)
T_DOB       equ 8                   ; doble precisión (8 octetos MBF)

; ---- el acumulador (FAC) y el segundo operando (ARG) ----
; Un número con decimales va «desempaquetado»: signo, exponente y una
; mantisa de 64 bits con el bit alto puesto (valor = 0,1mmm × 2^exp).
; El cero es mantisa 0. Un entero va en ac_m0; una cadena, la dirección de
; su descriptor en ac_m0.
AC_TIPO     equ 0
AC_SIG      equ 1                   ; 0 positivo, 80h negativo
AC_EXP      equ 2                   ; con signo
AC_M0       equ 4                   ; la palabra baja de la mantisa
AC_M1       equ 6
AC_M2       equ 8
AC_M3       equ 10                  ; la alta
AC_TAM      equ 12

N_TEMP      equ 12                  ; descriptores de cadenas de paso
PILA_TAM    equ 0x700               ; la pila de la máquina
CTRL_TAM    equ 0x300               ; la de PARA, LLAMAR y MIENTRAS
LIN_TAM     equ 256

absolute 0
v_firma     resw 1                  ; (el 0 no se usa: un enlace 0 es «fin»)
v_prog      resw 1                  ; dónde empieza el programa (PROG_INI)
v_vars      resw 1                  ; dónde empiezan las variables simples
v_mats      resw 1                  ; dónde empiezan las matrices
v_libre     resw 1                  ; el primer octeto libre
v_cads      resw 1                  ; dónde empiezan las cadenas (bajan)
v_cima      resw 1                  ; la cima de la memoria del usuario
v_linea     resw 1                  ; la línea que se ejecuta (FFFFh: directo)
v_pila0     resw 1                  ; SP con la pila vacía
v_ctrl      resw 1                  ; la cima de la pila de control
v_dat_lin   resw 1                  ; READ: dónde se va leyendo
v_dat_ptr   resw 1
v_cont_lin  resw 1                  ; CONT: dónde se paró (0: no se puede)
v_cont_ptr  resw 1
v_col       resw 1                  ; la columna de la pantalla (desde 0)
v_ult_car   resb 1                  ; lo último escrito (para LIST en español)
            alignb 2
v_ancho     resw 1                  ; el ancho de la línea (80)
v_ultima    resw 1                  ; la línea en que hubo un error (ERL)
v_err       resw 1                  ; el número de error (ERR)
v_tron      resb 1                  ; TRON
v_idioma    resb 1                  ; LIST: 0 inglés, 1 español
v_corte     resb 1                  ; Ctrl+Inter pulsado
v_seg_cod   resw 1                  ; el segmento del código
v_int1b     resd 1                  ; el INT 1Bh de antes
v_azar      resd 1                  ; RND: la semilla (24 bits)
v_ult_azar  resd 1                  ; RND: el último (en MBF)
v_fn_fin    resw 1                  ; (DEF FN) dónde acaba la llamada
v_tipo_var  resb 1                  ; lo que encuentra busca_var
v_temp_n    resw 1                  ; cuántos descriptores de paso hay
v_crunch    resb 1                  ; estado del tokenizador
v_opnd      resb 1
v_ini_sent  resb 1
v_lineas    resb 1
v_dim_auto  resb 1                  ; busca_var: crear matriz con 10
v_aux       resw 8                  ; para lo que haga falta de paso
v_aux2      resw 8
v_fichero   resw 1                  ; LOAD/SAVE: el número del archivo
v_lee_pos   resw 1                  ; LOAD: lo leído que queda en el búfer
v_lee_fin   resw 1
v_salir     resb 1                  ; SYSTEM pedido
v_cargar_y_ejecutar resb 1
v_listar_es resb 1                  ; se tecleó LISTAR (no LIST)
v_dat_dentro resb 1                 ; READ: dentro de un DATA
v_sc_data   resb 1                  ; siguiente_codigo: saltar el DATA
v_in_preg   resb 1                  ; INPUT: con «? »
v_in_pasada resb 1                  ; INPUT: 0 mirar, 1 asignar
            alignb 2
v_in_txt    resw 1                  ; INPUT: el texto (descriptor) o 0
v_in_vars   resw 1                  ; INPUT: dónde empiezan las variables
v_in_tras   resw 1                  ; INPUT: por dónde va la línea
v_em_n      resw 1                  ; elem_matriz: cuántos índices
v_nom_n     resw 1                  ; la pila de nombres
v_fnp       resw 1                  ; la pila de FN (cima)
v_fn_tipo   resb 1
            alignb 2
v_defseg    resw 1                  ; DEF SEG
v_rc_cima   resw 1                  ; recoge_cadenas
v_rc_lim    resw 1
v_rc_mejor  resw 1
v_rc_dir    resw 1
v_ult_rnd   resw 2                  ; RND: el último (24 bits)
v_sal_h     resw 1                  ; SAVE: el archivo al que va la salida
v_sal_n     resw 1                  ;  lo que hay en su búfer
v_dpos      resw 1                  ; LOAD: el búfer del archivo
v_dfin      resw 1
v_cero_val  resb 8                  ; el valor de una variable que no existe
v_sent_ptr  resw 1                  ; dónde empieza la sentencia que se ejecuta
v_sent_lin  resw 1                  ;  y en qué línea (para ON ERROR)
v_onerr     resw 1                  ; ON ERROR GOTO: la línea (0: no)
v_err_ptr   resw 1                  ; RESUME: dónde se vuelve
v_en_trampa resb 1                  ; dentro de la rutina de ON ERROR
            alignb 2
v_sal_f     resw 1                  ; PRINT #: el archivo al que va (0: pantalla)
v_col_guard resw 1                  ;  la columna y el ancho de la pantalla, guardados
v_ancho_g   resw 1
v_lp_x      resw 1                  ; gráficos (grafico.inc): el último punto
v_lp_y      resw 1
v_gmax_x    resw 1                  ;  el ancho en puntos
v_g_x1      resw 1
v_g_y1      resw 1
v_g_x2      resw 1
v_g_y2      resw 1
v_g_ya      resw 1
v_g_yb      resw 1
v_g_ax      resw 1
v_g_ay      resw 1
v_g_bx      resw 1
v_g_by      resw 1
v_g_dx      resw 1
v_g_dy      resw 1
v_g_sx      resw 1
v_g_sy      resw 1
v_g_r       resw 1
v_g_ry      resw 1
v_g_px      resw 1
v_g_py      resw 1
v_d_fin     resw 1                  ;  DRAW
v_d_escala  resw 1
v_modo      resb 1                  ; SCREEN: 0 texto, 1 o 2
v_attr      resb 1                  ; COLOR en texto (7: el normal)
v_gcolor    resb 1                  ; el color de dibujo
v_gmask     resb 1
v_g_col     resb 1
v_g_caja    resb 1
v_g_borde   resb 1
v_g_arr     resb 1
v_g_aba     resb 1
v_d_b       resb 1
v_d_n       resb 1
v_es_pant   resb 1                  ; la salida va a la pantalla (se mira al empezar)
v_p_listo   resb 1                  ; PLAY (sonido.inc)
v_p_oct     resb 1
v_p_estilo  resb 1
            alignb 2
v_p_largo   resw 1
v_p_tempo   resw 1
v_p_fin     resw 1
            alignb 2
RELLENO_TAM equ 1024
b_relleno   resb RELLENO_TAM        ; PAINT: las semillas
v_auto_n    resw 1                  ; AUTO: la línea siguiente y el paso
v_auto_paso resw 1
v_auto      resb 1
            alignb 2
v_us_largo  resw 1                  ; PRINT USING (using.inc)
v_us_pos    resw 1
v_us_campo  resw 1
v_us_fin    resw 1
v_us_ent    resw 1
v_us_dec    resw 1
v_us_ndec   resw 1
v_us_nent   resw 1
v_us_sig    resw 1
v_us_e10    resw 1
v_us_hubo   resb 1
v_us_mas    resb 1
v_us_ast    resb 1
v_us_dolar  resb 1
v_us_coma   resb 1
v_us_exp    resb 1
v_us_neg    resb 1
v_us_cero   resb 1
b_us        resb 256                ; el formato de PRINT USING
v_cero_tmp  resb 8                  ; INPUT: para probar un número
            alignb 2
v_def       resb 26                 ; DEFINT/DEFSTR...: el tipo por letra
            alignb 2
ac          resb AC_TAM             ; el acumulador
arg         resb AC_TAM             ; el segundo operando
ac2         resb AC_TAM             ; de paso para las funciones
ac3         resb AC_TAM
ac4         resb AC_TAM
ac5         resb AC_TAM             ; (la base de f_pot10)
ac_tan      resb AC_TAM             ; TAN
ac_suma     resb AC_TAM             ; (de paso, solo para f_suma)
ac_pot      resb AC_TAM             ; (^: el exponente, mientras se hace LOG)
ac_tan2     resb AC_TAM
v_temps     resb 3 * N_TEMP         ; los descriptores de paso
v_desc_aux  resb 3                  ; uno más, para lo que no se guarda
            alignb 2
b_lin       resb LIN_TAM            ; la línea tecleada
b_tok       resb LIN_TAM + 64       ; la línea tokenizada
b_num       resb 48                 ; un número escrito
b_cifras    resb 24                 ; las cifras de un número (num_a_texto)
b_nom       resb 44                 ; un nombre de variable (largo y letras)
b_arch      resb 68                 ; un nombre de archivo (ASCIZ)
b_ent       resb LIN_TAM            ; la entrada (handle 0)
N_NOMS      equ 8
v_nom_pila  resb 42 * N_NOMS        ; nombres guardados (matrices anidadas)
FN_TAM      equ 12
N_FNP       equ 16
v_fn_pila   resb FN_TAM * N_FNP     ; FN: los valores de antes de los parámetros
; los archivos de OPEN (#1 a #3): una ficha de F_TAM octetos cada uno
N_ARCH      equ 3
F_HAN       equ 0                   ; el handle de DOS
F_MODO      equ 2                   ; 0 cerrado; 'I', 'O' o 'A'
F_FIN       equ 3                   ; 1: ya no hay más que leer
F_POS       equ 4                   ; el búfer: por dónde va
F_LEN       equ 6                   ;  y cuánto hay (al leer) / lleno (al escribir)
F_COL       equ 8                   ; la columna (para PRINT #)
F_BUF       equ 16
F_BUF_TAM   equ 256
F_TAM       equ F_BUF + F_BUF_TAM
v_arch      resb F_TAM * N_ARCH
            alignb 2
b_disco     resb 512                ; LOAD: lo leído del disco
            alignb 2
b_ctrl      resb CTRL_TAM           ; la pila de control (crece hacia arriba)
            alignb 2
b_pila      resb PILA_TAM
pila_cima:
            resw 2                  ; (un hueco: la pila no puede tocar el 0 de
            alignb 16               ;  PROG_INI-1, delante de la primera línea)
PROG_INI:
