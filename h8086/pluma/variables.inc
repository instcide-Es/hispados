; =============================================================================
;  PLUMA - los textos y las variables
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
; =============================================================================

m_poca      db "PLUMA necesita 64 K de memoria libres.", 13, 10, "$"
m_pluma     db "Pluma  ", 0
m_sin_nombre db "(sin nombre)", 0
m_col       db "Col ", 0
m_neg       db "Negrita ", 0
m_sub       db "Subray. ", 0
m_cur       db "Cursiva ", 0
m_alin      db "Izq.CentDchaJust"
m_ins       db "Ins", 0
m_sob       db "Sob", 0
m_codigos   db " Cód", 0
m_ayuda     db " F1 Ayuda  F2 Guardar  F3 Buscar  F7 Marcar  F9 Vista  Esc Menú  Alt+X Salir", 0
m_ayuda_bloque db " Bloque: F4 copiar  F5 cortar  Alt+N S C formato  Alt+Q quitarlo  F7 fijar", 0
m_pre_k     db " ^K B,K bloque C copia V mueve Y borra S guarda X sale Q deja R lee W escribe", 0
m_pre_q     db " ^Q S,D línea  R,C texto  F busca  A cambia  L otra  Y borra al final  B,K marca", 0
m_pre_o     db " ^O C centrar  J justificar  D códigos  L margen  R ancho  G sangría  P vista", 0
m_pre_p     db " ^P B negrita  S subrayado  Y cursiva  L salto de página", 0
m_reajusta  db " Pluma ajusta las líneas sola: no hace falta ^B.", 0
m_menu      db "Pluma:", 0
m_cambios   db " Hay cambios sin guardar. ¿Guardarlos? (S: sí, N: no, Esc: seguir)", 0
m_salir_sin db " ¿Salir sin guardar los cambios? (S: sí, otra: no)", 0
m_p_mizq    db " Margen izquierdo del papel (letras): ", 0
m_p_msup    db " Margen de arriba (líneas): ", 0
m_p_minf    db " Margen de abajo (líneas): ", 0
m_p_ancho   db " Ancho de la línea (de 20 a 78 letras): ", 0
m_p_sang1   db " Sangría de la primera línea de los párrafos (letras): ", 0
m_p_largo   db " Líneas por página (66 en papel continuo, 70 en A4): ", 0
m_p_pag1    db " Número de la primera página: ", 0
m_p_enc     db " Encabezado (# es el número de página): ", 0
m_p_pie     db " Pie (# es el número de página): ", 0
m_p_hojas   db " ¿Hojas sueltas, con una pausa entre página y página? (S/N)", 0
m_disco     db " No se puede usar el disco para el documento (¿lleno o protegido?).", 0
m_nuevo     db " Documento nuevo.", 0
m_de_ws     db " Abierto de WordStar: se guardará como .PLU.", 0
m_e_leer    db " No se ha podido leer el archivo. Pulse una tecla.", 0
m_e_corto   db " No ha cabido entero: solo se ha leído una parte. Pulse una tecla.", 0
m_abrir     db " Abrir: ", 0
m_leer      db " Insertar el archivo: ", 0
m_no_existe db " No existe ese archivo. Pulse una tecla.", 0
m_guardar_como db " Guardar como: ", 0
m_escribir  db " Escribir el bloque en: ", 0
m_guardado  db " Guardado.", 0
m_e_guardar db " No se ha podido guardar (¿disco lleno o protegido?). Pulse una tecla.", 0
m_exportar  db " Exportar a: ", 0
m_exportado db " Exportado.", 0
m_v_pagina  db " fin de la página ", 0
m_otra_hoja db " Ponga otra hoja y pulse una tecla (Esc: dejarlo).", 0
m_imprimir  db " Imprimir en (PRN, la impresora, o un archivo): ", 0
m_imprimiendo db " Imprimiendo... (Esc: dejarlo)", 0
m_impreso   db " Impreso.", 0
m_e_imprimir db " No se ha podido imprimir. Pulse una tecla.", 0
m_vista     db " Vista: flechas, RePág y AvPág para moverse; Esc o F9 para volver", 0
m_vista_arriba db " Pluma - vista de la página: así saldrá en el papel", 0
m_contando  db " Contando...", 0
m_k_pal     db " palabras, ", 0
m_k_let     db " letras, ", 0
m_k_par     db " párrafos y ", 0
m_k_pag     db " páginas.", 0
m_k_pag1    db " página.", 0
m_buscar    db " Buscar: ", 0
m_por       db " Cambiar por: ", 0
m_no_esta   db " No está.", 0
m_reempl    db " ¿Cambiarlo? (S: sí, N: no, T: todos, Esc: acabar)", 0
m_cambiados db " cambios.", 0
firma       db "PLUMA86", 0x1A
n_tmp       db "~PLUMA_A.$$$", 0
e_plu       db ".PLU", 0
e_txt       db ".TXT", 0
e_tmp       db ".$$$", 0
e_bak       db ".BAK", 0
e_rtf       db ".RTF", 0
imp_nombre  db "PRN", 0
            times 62 db 0

; el documento de siempre: 65 letras de ancho y 10 de margen (en papel de
; 8 1/2 pulgadas, a 10 letras por pulgada), 66 líneas, el número de página
; abajo, y una Epson de las que tienen la tabla PC 437
cab_normal:
        db "PLUMA86", 0x1A
        db 1                        ; la versión del formato
        db 10, 65, 0, 66, 3, 3, 1, 1, 0
        times CAB_ENC - ($ - cab_normal) db 0
        times 48 db 0               ; el encabezado
        db "- # -", 0               ; el pie
        times CAB_TAM - ($ - cab_normal) db 0

; RTF
r_cab       db "{\rtf1\ansi\ansicpg1252\deff0{\fonttbl{\f0\fmodern Courier New;}}", 13, 10
            db "\paperw11906\paperh16838", 0
r_margl     db "\margl", 0
r_margr     db "\margr", 0
r_margt     db "\margt", 0
r_margb     db "\margb", 0
r_pgn       db "\pgnstarts", 0
r_encab     db 13, 10, "{\header\pard\plain\qc\f0\fs24 ", 0
r_pie       db 13, 10, "{\footer\pard\plain\qc\f0\fs24 ", 0
r_cierra    db "\par}", 0
r_campo     db "{\field{\*\fldinst PAGE}{\fldrslt 1}}", 0
r_pard      db 13, 10, "\pard\plain\f0\fs24", 0
r_alin      db "\qc", 0, "\qr", 0, "\qj", 0
r_li        db "\li", 0
r_fi        db "\fi", 0
r_tab       db "\tab ", 0
r_par       db "\par", 0
r_page      db "\page", 0
r_b         db "\b ", 0, 0, 0, 0, 0, "\b0 ", 0, 0, 0, 0
r_ul        db "\ul ", 0, 0, 0, 0, "\ul0 ", 0, 0, 0
r_i         db "\i ", 0, 0, 0, 0, 0, "\i0 ", 0, 0, 0, 0

; ---- la ayuda (F1): # al principio, la línea va al revés; 12, otra página ----
t_ayuda:
 db "#  PLUMA, el procesador de textos de Hispa-8086 - Ayuda (1 de 3)                ", 10
 db 10
 db " Se escribe sin pulsar Intro al final de las líneas: se parten solas.", 10
 db " Intro acaba el párrafo. Esc o F10 abre el menú (se elige con la letra).", 10
 db 10
 db "#  Moverse                                                                      ", 10
 db "  Flechas, Inicio y Fin de la línea, RePág y AvPág, Ctrl+Inicio y Ctrl+Fin", 10
 db "  del texto, Ctrl+flechas de palabra en palabra.", 10
 db 10
 db "#  Escribir                                                                     ", 10
 db "  Ins     insertar o sobrescribir     Supr, Retroceso   borrar", 10
 db "  Tab     tabulador (cada 5)          Alt+T  tilde a la letra de antes:", 10
 db "  Alt+E   el euro                            a á à ä â, A Á..., n ñ, c ç, ? ¿", 10
 db "  Ctrl+Z  deshacer (también ^U)       Alt+P  salto de página", 10
 db 10
 db "#  Formato                                                                      ", 10
 db "  Alt+N negrita  Alt+S subrayado  Alt+C cursiva (lo que se escriba, o el bloque)", 10
 db "  Alt+I izquierda  Alt+M centrado  Alt+D derecha  Alt+J justificado", 10
 db "  Alt+G sangría (5 letras más)    Alt+Q quitar el formato del bloque", 10
 db "  Alt+V ver los códigos (como Reveal Codes de WordPerfect)", 10
 db 10
 db "  En la pantalla: negrita con brillo, subrayado (o inverso en las de color),", 10
 db "  cursiva al revés (o con fondo oscuro). La línea de arriba dice cuál hay.", 10
 db 12
 db "#  PLUMA - Ayuda (2 de 3)                                                        ", 10
 db 10
 db "#  Bloques                                                                      ", 10
 db "  F7  empezar a marcar (se mueve el cursor) y otra vez F7 para fijarlo", 10
 db "  F4  copiar   F5  cortar   F6  pegar     Edición › Borrar, Escribir bloque", 10
 db 10
 db "#  Buscar                                                                       ", 10
 db "  F3  buscar (sin mirar mayúsculas ni tildes); F3 e Intro, el siguiente", 10
 db "  F8  reemplazar (S sí, N no, T todos)", 10
 db 10
 db "#  Archivos                                                                     ", 10
 db "  F2  guardar (el de antes queda como .BAK)    Alt+X  salir", 10
 db "  .PLU es el formato de Pluma; .TXT se guarda en texto, sin formato.", 10
 db "  Abre texto y documentos de WordStar.  Archivo › Exportar: texto de DOS,", 10
 db "  texto de hoy (UTF-8) y RTF (para Word o LibreOffice).", 10
 db 10
 db "#  Páginas                                                                      ", 10
 db "  F9  la vista de la página    Imprimir › Imprimir (en PRN o en un archivo)", 10
 db "  Página: márgenes, ancho, sangría, largo, encabezado y pie (# = número),", 10
 db "  la impresora (Epson FX, Epson moderna, IBM, solo texto) y hojas sueltas.", 10
 db 12
 db "#  PLUMA - Ayuda (3 de 3): las teclas de WordStar                               ", 10
 db 10
 db "  ^E ^X ^S ^D  arriba, abajo, izquierda, derecha     ^A ^F  palabra", 10
 db "  ^R ^C  pantalla arriba y abajo     ^G  borrar      ^T  borrar la palabra", 10
 db "  ^Y  borrar la línea     ^V  insertar/sobrescribir     ^N  partir aquí", 10
 db "  ^L  buscar otra vez     ^U  deshacer", 10
 db 10
 db "  ^QS ^QD  principio y fin de la línea    ^QR ^QC  del texto", 10
 db "  ^QF buscar  ^QA reemplazar  ^QY borrar hasta el final  ^QB ^QK al bloque", 10
 db 10
 db "  ^KB ^KK  principio y fin del bloque  ^KC copiar  ^KV mover  ^KY borrar", 10
 db "  ^KH quitar la marca  ^KS guardar  ^KD ^KX guardar y salir  ^KQ salir", 10
 db "  ^KR leer un archivo  ^KW escribir el bloque  ^KP imprimir", 10
 db 10
 db "  ^PB negrita  ^PS subrayado  ^PY cursiva  ^PL salto de página", 10
 db "  ^OC centrar  ^OJ justificar  ^OD ver códigos  ^OL margen  ^OR ancho", 10
 db "  ^OG sangría  ^OP vista       ^B no hace falta: Pluma ajusta solo", 10
 db 10
 db " Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.", 10
 db 0

; =============================================================================
        absolute $
        alignb 2
bss_ini:
pre         resw 1                  ; lo de antes del cursor, en la memoria
post        resw 1                  ; y lo de después
pstart      resw 1                  ; dónde empieza lo de después
g_ini       resw 1                  ; el hueco: de dónde se salta...
g_fin       resw 1                  ; ...y adónde
bfin        resw 1                  ; el final de la memoria del texto
lenA        resd 1                  ; lo que hay en A
nB          resw 1                  ; los trozos de B
cpos        resd 1                  ; por dónde va C
clen        resd 1                  ; y lo que mide
hA          resw 1                  ; los archivos A, B, C y P (el bloque)
hB          resw 1
hC          resw 1
hP          resw 1
ir_pos      resd 1
ins_n       resw 1
bor_n       resw 1
bor_q       resw 1
b_ini       resd 1                  ; el bloque
b_fin       resd 1
clip_n      resd 1                  ; lo copiado
it_pos      resd 1                  ; leer el documento: por dónde va
it_n        resw 1
it_si       resw 1
u_top       resw 1                  ; deshacer
u_src       resw 1
u_pos       resd 1
esp_cx      resw 1
top_w       resw 1                  ; la primera línea de la pantalla:
top_k       resw 1                  ; su párrafo y cuántas se salta
vseg        resw 1
s_ini       resw 1                  ; lo marcado, en la memoria
s_fin       resw 1
abajo_col   resw 1
mensaje     resw 1                  ; un aviso para la línea de abajo
l_p         resw 1                  ; la línea que se compone
m_fin       resw 1                  ; (mide)
m_nsp       resw 1
m_rup       resw 1
m_snsp      resw 1
k_cada      resw 1                  ; (compon)
k_ultp      resw 1
p_preg      resw 1                  ; (pide)
p_buf       resw 1
n_ptr       resw 1
n_lim       resw 1
mn_def      resw 1                  ; (menu)
a_hace      resw 1
a_delta     resw 1
cg_h        resw 1                  ; (cargar)
cv_n        resw 1
s_h         resw 1                  ; (la salida)
s_n         resw 1
e_ini       resd 1                  ; (escribir)
e_fin       resd 1
e_nom       resw 1
r_tmp       resw 1
c_pag       resw 1                  ; (las páginas)
c_lin       resw 1
c_vlin      resw 1
c_cuerpo    resw 1
c_hasta     resw 1
v_top       resw 1
pb_atras    resw 1
mg_n        resw 1
mg_t        resw 1
mg_d        resw 1
mg_i        resw 1
d_ini       resw 1
d_fin       resw 1
d_cods      resw 6
k_pal       resd 1                  ; (contar)
k_let       resd 1
k_par       resd 1
k_pags      resw 1
bs_n        resw 1                  ; (buscar)
bs_k        resw 1
bs_pos      resd 1
bs_ini      resd 1
bs_fin      resd 1
r_cuenta    resw 1
mn_sel      resb 1                  ; (que sea palabra: se guarda con push)
mn_n        resb 1
modo        resb 1
letras_ok   resb 1
cambiado    resb 1
insertar    resb 1
ver_cod     resb 1
marcando    resb 1
clip_f      resb 1
filas_ok    resb 1
centrar     resb 1
sin_ajuste  resb 1
con_video   resb 1
intentos    resb 1
fin_texto   resb 1
vert        resb 1
vert_ant    resb 1
col_des     resb 1
era_letra   resb 1
forzar_ws   resb 1
es_escr     resb 1
una         resb 1
u_apag      resb 1
u_roto      resb 1
u_grupo     resb 1
u_cual      resb 1
l_par       resb 1
l_est       resb 1
l_pri       resb 1
m_ult       resb 1                  ; (estas dos, juntas)
m_acaba     resb 1
m_tab       resb 1                  ; (y estas)
m_pag       resb 1
m_wv        resb 1
m_swv       resb 1
m_vio       resb 1
m_esp       resb 1
m_est       resb 1
k_extra     resb 1
k_vio       resb 1
k_esp       resb 1
k_col       resb 1
c_col       resb 1
c_est       resb 1
c_hay       resb 1
cur_fila    resb 1
cur_fila_ant resb 1
cur_col     resb 1
cur_est     resb 1
cur_par     resb 1
cur_ok      resb 1
i_est       resb 1
f_cod       resb 1
f_est       resb 1
f_bit       resb 1
f_quiere    resb 1
f_orig      resb 1
f_ahora     resb 1
f_forzar    resb 1
a_cod       resb 1
p_max       resb 1
p_virgen    resb 1
cg_ins      resb 1
cg_modo     resb 1
cg_aC       resb 1
cg_corto    resb 1                  ; lo leído no ha cabido entero
cg_fin      resb 1
cg_blando   resb 1
cg_ult      resb 1
s_err       resb 1
e_txt_f     resb 1
r_nuevo     resb 1
r_est       resb 1
r_par_f     resb 1
c_modo      resb 1
c_vercod    resb 1
c_parar     resb 1
c_estp      resb 1
c_disp      resb 1
pb_cortado  resb 1
pb_sigue    resb 1
d_tipo      resb 1
d_est       resb 1
d_ch        resb 1
k_dentro    resb 1
k_ultima    resb 1
r_todos     resb 1
bs_cero     resb 1
puntos      resb 1
n_txt       resb 12
tmp         resb 64
nombre      resb 80
tmp_nombre  resb 80
n_bak       resb 80
busca       resb 32
cambio      resb 32
bs_pat      resb 32
bs_ani      resb 32
bs_dnd      resb 128
lin_abajo   resb 100
atributos   resb 8
vis         resb 128
pliega      resb 256
        alignb 2
fila        resb 160                ; la línea que se compone
fpos        resb 160
fila2       resb 160
filas       resb NFILAS * 8         ; las de la pantalla, para rehacerlas
cab         resb CAB_TAM            ; la cabecera del documento
ibuf        resb 512
sec         resb 512
obuf        resb 512
cbuf        resb 520
PB          resb PB_TAM
UB          resb UB_TAM
CLIP        resb CLIP_TAM
BUF:                                ; y el texto, hasta [bfin]
