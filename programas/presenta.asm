; =============================================================================
;  PRESENTA.HVP - Presentaciones, el programa de diapositivas de Hispa
;  Ventanas
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Una presentación son diapositivas apaisadas (4:3) con cajas de texto,
;  formas, líneas e imágenes, que se enseñan a pantalla completa. Es el mismo
;  editor de láminas que Publicaciones (se maneja igual: un clic elige,
;  arrastrando se mueve, los tiradores cambian el tamaño, la fila del formato
;  es la misma), con lo propio de una presentación:
;    - a la izquierda, la tira de miniaturas: un clic va a esa diapositiva y
;      arrastrándola (o con Ctrl y las flechas) cambia de sitio; se añaden
;      (eligiendo un diseño), se duplican y se quitan;
;    - el clasificador (menú Ver): todas las miniaturas en una rejilla, para
;      ver la presentación entera y ordenarla arrastrando;
;    - los diseños: título, título y texto, dos columnas, solo título, en
;      blanco, título e imagen. Sus cajas dicen para qué son («Haga clic para
;      poner el título») hasta que se escribe en ellas. A una diapositiva ya
;      hecha se le puede cambiar el diseño: sus cajas van a su sitio nuevo;
;    - las listas: cada párrafo con su punto o con su número; Tab y Mayús+Tab
;      cambian de nivel;
;    - los temas: el fondo, el adorno y los colores y tipos de letra a juego
;      para toda la presentación;
;    - la presentación a pantalla completa (F5; Mayús+F5 desde la diapositiva
;      en la que se está), con transiciones y, si se quiere, los puntos de las
;      listas de uno en uno;
;    - las notas del orador de cada diapositiva, que se ven y se escriben
;      debajo, en su recuadro;
;    - imprimir: una diapositiva por página, folletos de 2, 3 o 6 (cada una
;      con su número), o las notas.
;
;  Cómo está hecho. El documento y su dibujo son de programas/comun/lamina.inc
;  (el motor) y el manejo, de programas/comun/lamina_ed.inc (el editor de
;  láminas): aquí queda lo propio. Una diapositiva es una lámina de 800 x 600
;  puntos: así, en la pantalla de siempre (800 x 600), la presentación sale
;  punto por punto, con las letras tal como son (en 640 x 480 se reduce a 4/5 y
;  en pantallas mayores se amplía, hasta 1024 x 768). Lo que hace cada caja en
;  su diapositiva va en su ficha (LO_PAPEL: título, texto, hueco de una
;  imagen); las notas son una caja más, escondida (LB_OCULTO), así que viajan
;  con su diapositiva al moverla, copiarla, deshacer y guardar. El tema, la
;  transición y lo de los puntos van en [lam_extra]. El archivo (.PRE) es el
;  del motor, con su firma.
;
;  Parámetros del acceso directo: el nombre de una presentación, que se abre.
; =============================================================================
%include "hvp.inc"

DIA_AN      equ 800                 ; una diapositiva, en puntos de lámina
DIA_AL      equ 600
DIA_MARGEN  equ 24
TIRA_AN     equ 140                 ; la tira de miniaturas
MINI_AN     equ 112                 ; una miniatura
MINI_AL     equ 84
MINI_X      equ 22
MINI_PASO   equ 100                 ; de una a la siguiente
NOTAS_AL    equ 56                  ; el recuadro de las notas
NOTAS_MAX   equ 4000                ; letras de las notas de una diapositiva (las de una caja)
NOTAS_X     equ 66                  ; dónde empieza su texto en el recuadro
notas_buf   equ pega_txt            ; el cuadro de las notas usa el búfer de pegar texto del editor
CL_X0       equ 20                  ; el clasificador: dónde va la primera miniatura
CL_Y0       equ 14
CL_PASO_X   equ 136                 ; y de una a la siguiente
CL_PASO_Y   equ 112
PRES_AN     equ 1024                ; lo más grande que se presenta
PRES_AL     equ 768
HOJA_AN     equ 794                 ; la hoja de los folletos: un A4 de pie
HOJA_AL     equ 1123
; lo que hace una caja en su diapositiva (LO_PAPEL)
LP_TITULO   equ 1
LP_CUERPO   equ 2
LP_CUERPO2  equ 3
LP_SUBTIT   equ 4
LP_IMAGEN   equ 5
LP_NOTAS    equ 9
; órdenes propias
O_DIA_DUP   equ 200
O_DIA_SUBE  equ 201
O_DIA_BAJA  equ 202
O_NOTAS     equ 203
O_TEMA      equ 204
O_PRES      equ 205
O_PRES_AQUI equ 206
O_TRANS     equ 207
O_PASOS     equ 208
O_FOLLETO2  equ 209
O_FOLLETO3  equ 210
O_FOLLETO6  equ 211
O_IMP_NOTAS equ 212
O_DISENO    equ 213
O_CLASIF    equ 214

HVP_CABECERA "Presentaciones", entrada, 4
        db "................................"
        db "................................"
        db "..0000000000000000000000000000.."
        db "..0777777777777777777777777770.."
        db "..0000000000000000000000000000.."
        db "....09999999999999999999990....."
        db "....09999999999999999999990....."
        db "....09922222222222222229990....."
        db "....09922222222222222229990....."
        db "....09999999999999999999990....."
        db "....09999999999999999999990....."
        db "....0994499777777777999aa90....."
        db "....099449999999999999aaaa0....."
        db "....09999999999999999aaaaa0....."
        db "....0994499777777779aaaaaa0....."
        db "....09944999999999999aaaaa0....."
        db "....099999999999999999aaaa0....."
        db "....0994499777777999999aa90....."
        db "....09944999999999999999990....."
        db "....09999999999999999999990....."
        db "....00000000000000000000000....."
        db "...............00..............."
        db "...............00..............."
        db "..............0000.............."
        db ".............00..00............."
        db "............00....00............"
        db "...........00......00..........."
        db "..........00........00.........."
        db ".........00..........00........."
        db "........00............00........"
        db "................................"
        db "................................"
HVP_CODIGO

c_presenta  dd pre_proc
            db SIN_FONDO, 0, 0, 0   ; lo pinta todo ella: sin parpadeo
pre_plant:
        PLANTILLA c_presenta, titulo, E_NORMAL, 8, 30, 784, 562, 0

menu_prog:
        db "&Archivo", 0
        dw O_NUEVO
        db "&Nuevo...               Ctrl+U", 0
        dw O_ABRIR
        db "&Abrir...               Ctrl+A", 0
        dw 0
        db 0
        dw O_GUARDAR
        db "&Guardar                Ctrl+G", 0
        dw O_COMO
        db "Guardar c&omo...", 0
        dw O_EXPORTAR
        db "&Exportar la diapositiva como BMP...", 0
        dw 0
        db 0
        dw O_IMPRIMIR
        db "&Imprimir (una por página)  Ctrl+P", 0
        dw O_BORRADOR
        db "Imprimir en &borrador (rápido)", 0
        dw O_FOLLETO2
        db "Imprimir un folleto de &2 por página", 0
        dw O_FOLLETO3
        db "Imprimir un folleto de &3, con renglones", 0
        dw O_FOLLETO6
        db "Imprimir un folleto de &6 por página", 0
        dw O_IMP_NOTAS
        db "Imprimir las no&tas del orador", 0
        dw 0
        db 0
        dw O_SALIR
        db "&Salir", 0
        dw 0xFFFF
        db "&Edición", 0
        dw O_DESHACER
        db "&Deshacer              Ctrl+Z", 0
        dw O_REHACER
        db "&Rehacer               Ctrl+Y", 0
        dw 0
        db 0
        dw O_CORTAR
        db "Cor&tar                Ctrl+X", 0
        dw O_COPIAR
        db "&Copiar                Ctrl+C", 0
        dw O_PEGAR
        db "&Pegar                 Ctrl+V", 0
        dw O_BORRAR
        db "&Borrar el objeto        Supr", 0
        dw O_DUPLICAR
        db "D&uplicar el objeto    Ctrl+R", 0
        dw O_TODO
        db "Ele&gir todo           Ctrl+E", 0
        dw 0
        db 0
        dw O_ESCRIBIR
        db "&Escribir en la caja    Intro", 0
        dw 0
        db 0
        dw O_DIA_DUP
        db "Duplicar la d&iapositiva", 0
        dw O_PAG_QUITA
        db "&Quitar esta diapositiva", 0
        dw O_DIA_SUBE
        db "&Subirla un puesto   Ctrl+Izda.", 0
        dw O_DIA_BAJA
        db "Ba&jarla un puesto   Ctrl+Dcha.", 0
        dw 0xFFFF
        db "&Ver", 0
        dw O_VISTA                  ; (delante de cada orden, el sitio de su
mk_v0   db "  &Diapositiva entera", 0   ; marca: ver «marcas» en lamina_ed.inc)
        dw O_VISTA + 1
mk_v1   db "  Al &50 %", 0
        dw O_VISTA + 2
mk_v2   db "  Al &75 %", 0
        dw O_VISTA + 3
mk_v3   db "  Al &100 % (tamaño real)", 0
        dw 0
        db 0
        dw O_MARGENES
mk_marg db "  Ver los &márgenes", 0
        dw O_REJILLA
mk_rej  db "  Ver la &rejilla", 0
        dw O_AJUSTAR
mk_ajus db "  A&justar a la rejilla (5 mm)", 0
        dw 0
        db 0
        dw O_PAG_ANT
        db "  Diapositiva &anterior    RePág", 0
        dw O_PAG_SIG
        db "  Diapositiva &siguiente   AvPág", 0
        dw 0
        db 0
        dw O_CLASIF
mk_clas db "  &Clasificador de diapositivas", 0
        dw 0xFFFF
        db "&Insertar", 0
        dw O_PAG_NUEVA
        db "&Diapositiva nueva...", 0
        dw 0
        db 0
        dw O_HERR + 1
        db "Caja de &texto", 0
        dw O_HERR + 2
        db "&Rectángulo", 0
        dw O_HERR + 3
        db "&Elipse", 0
        dw O_HERR + 4
        db "&Línea", 0
        dw O_IMAGEN
        db "&Imagen (BMP)...", 0
        dw 0
        db 0
        dw O_NOTAS
        db "&Notas del orador...", 0
        dw 0xFFFF
        db "&Formato", 0
        dw O_TEMA
        db "T&ema de la presentación...", 0
        dw O_DISENO
        db "&Diseño de esta diapositiva...", 0
        dw 0
        db 0
        dw O_TIPO
        db "&Tipo de letra...", 0
        dw O_TAM
        db "Ta&maño de letra...", 0
        dw O_NEG
        db "&Negrita               Ctrl+N", 0
        dw O_CUR
        db "&Cursiva               Ctrl+K", 0
        dw O_SUB
        db "&Subrayado             Ctrl+S", 0
        dw O_GRANDE
        db "Letra &grande          Ctrl+W", 0
        dw 0
        db 0
        dw O_IZQ
        db "A la &izquierda        Ctrl+Q", 0
        dw O_CEN
        db "C&entrado              Ctrl+T", 0
        dw O_DER
        db "A la &derecha          Ctrl+D", 0
        dw O_VINETAS
        db "&Viñetas (una lista en la caja)", 0
        dw O_NUMEROS
        db "N&umeración (una lista con números)", 0
        dw 0
        db 0
        dw O_C_LETRA
        db "Color de la &letra...", 0
        dw O_C_FONDO
        db "Color del &relleno...", 0
        dw O_C_BORDE
        db "Color del &borde o de la línea...", 0
        dw O_GROSOR
        db "Gr&osor del borde o de la línea...", 0
        dw 0xFFFF
        db "&Disponer", 0
        dw O_FRENTE
        db "Traer al &frente           Ctrl+F", 0
        dw O_ALFONDO
        db "Enviar al fond&o           Ctrl+O", 0
        dw 0
        db 0
        dw O_ALINEAR
        db "Alinear a la &izquierda", 0
        dw O_ALINEAR + 1
        db "&Centrar a lo ancho", 0
        dw O_ALINEAR + 2
        db "Alinear a la &derecha", 0
        dw O_ALINEAR + 3
        db "Alinear &arriba", 0
        dw O_ALINEAR + 4
        db "Centrar a lo a&lto", 0
        dw O_ALINEAR + 5
        db "Alinear a&bajo", 0
        dw 0
        db 0
        dw O_REPARTIR
        db "Repartir a lo anc&ho", 0
        dw O_REPARTIR + 1
        db "Repartir a lo al&to", 0
        dw 0
        db 0
        dw O_AGRUPAR
        db "A&grupar", 0
        dw O_DESAGRUPAR
        db "Desagr&upar", 0
        dw 0
        db 0
        dw O_GIRAR
        db "Gi&rar un cuarto de vuelta  Ctrl+L", 0
        dw 0xFFFF
        db "&Presentación", 0
        dw O_PRES
        db "  &Empezar desde el principio       F5", 0
        dw O_PRES_AQUI
        db "  Empezar desde esta &diapositiva   Mayús+F5", 0
        dw 0
        db 0
        dw O_TRANS
        db "  &Transición entre diapositivas...", 0
        dw O_PASOS
mk_pasos db "  &Puntos de las listas de uno en uno", 0
        dw 0xFFFF
        db "&?", 0
        dw O_ACERCA
        db "&Acerca de Presentaciones...", 0
        dw 0xFFFF
        db 0

; ---- diálogo: una presentación nueva (la plantilla y el tema) ----
dlg_nuevo:
        PLANTILLA HV_c_dialogo, t_d_nuevo, E_DIALOGO, 0, 0, 440, 236, 0
        PLANTILLA HV_c_estatico, t_n_plant, E_HIJA, 16, 10, 200, 18, 100
        PLANTILLA c_lista2, t_nada, E_HIJA | E_TABULA, 16, 30, 200, 116, 60
        PLANTILLA HV_c_estatico, t_n_tema, E_HIJA, 224, 10, 200, 18, 101
        PLANTILLA c_lista2, t_nada, E_HIJA | E_TABULA, 224, 30, 200, 116, 61
        PLANTILLA HV_c_boton, t_aceptar, E_HIJA | E_TABULA | BS_DEFECTO, 124, 160, 90, 26, ID_ACEPTAR
        PLANTILLA HV_c_boton, t_cancelar, E_HIJA | E_TABULA, 224, 160, 90, 26, ID_CANCELAR
        dd 0
; ---- diálogo: las notas del orador ----
dlg_notas:
        PLANTILLA HV_c_dialogo, t_d_notas, E_DIALOGO, 0, 0, 500, 262, 0
        PLANTILLA HV_c_edit, 0, E_HIJA | E_TABULA | ES_MULTI, 12, 12, 472, 160, 70
        PLANTILLA HV_c_boton, t_aceptar, E_HIJA | E_TABULA | BS_DEFECTO, 154, 186, 90, 26, ID_ACEPTAR
        PLANTILLA HV_c_boton, t_cancelar, E_HIJA | E_TABULA, 254, 186, 90, 26, ID_CANCELAR
        dd 0

; ---- la fila de botones de arriba (la de abajo es la del editor de láminas) ----
            align 4
botones1    dd 6, 24, ic_nuevo, O_NUEVO, n_nuevo, BT_ICONO
            dd 32, 24, ic_abrir, O_ABRIR, n_abrir, BT_ICONO
            dd 58, 24, ic_guardar, O_GUARDAR, n_guardar, BT_ICONO
            dd 84, 24, ic_imprimir, O_IMPRIMIR, n_imprimir, BT_ICONO
            dd 116, 24, ic_cortar, O_CORTAR, n_cortar, BT_ICONO
            dd 142, 24, ic_copiar, O_COPIAR, n_copiar, BT_ICONO
            dd 168, 24, ic_pegar, O_PEGAR, n_pegar, BT_ICONO
            dd 200, 24, ic_deshacer, O_DESHACER, n_deshacer, BT_ICONO
            dd 226, 24, ic_rehacer, O_REHACER, n_rehacer, BT_ICONO
            dd 258, 24, ic_elegir, O_HERR, n_elegir, BT_ICONO
            dd 284, 24, ic_caja, O_HERR + 1, n_caja, BT_ICONO
            dd 310, 24, ic_rect, O_HERR + 2, n_rect, BT_ICONO
            dd 336, 24, ic_elipse, O_HERR + 3, n_elipse, BT_ICONO
            dd 362, 24, ic_linea, O_HERR + 4, n_linea, BT_ICONO
            dd 388, 24, ic_imagen, O_IMAGEN, n_imagen, BT_ICONO
            dd 420, 24, ic_pag_ant, O_PAG_ANT, n_pag_ant, BT_ICONO
            dd 446, 24, ic_pag_sig, O_PAG_SIG, n_pag_sig, BT_ICONO
            dd 472, 24, ic_mas, O_PAG_NUEVA, n_pag_nueva, BT_ICONO
            dd 504, 60, t_nada, O_CAMBIA_V, n_vista, BT_TEXTO
            dd 572, 24, ic_presentar, O_PRES, n_presentar, BT_ICONO
            dd 0, 0, 0, 0, 0, 0
n_nuevo     db "Nuevo: otra presentación, en blanco o de una plantilla (Ctrl+U)", 0
n_abrir     db "Abrir una presentación guardada (Ctrl+A)", 0
n_guardar   db "Guardar la presentación (Ctrl+G)", 0
n_caja      db "Caja de texto: arrastre sobre la diapositiva y escriba", 0
n_rect      db "Rectángulo: arrastre sobre la diapositiva", 0
n_elipse    db "Elipse: arrastre sobre la diapositiva", 0
n_linea     db "Línea: arrastre sobre la diapositiva", 0
n_pag_ant   db "Diapositiva anterior (RePág)", 0
n_pag_sig   db "Diapositiva siguiente (AvPág)", 0
n_pag_nueva db "Añadir una diapositiva detrás de esta: se elige su diseño", 0
n_presentar db "Presentar a pantalla completa desde el principio (F5)", 0
ic_presentar:
        db "................"
        db ".00000000000000."
        db ".09999999999990."
        db ".0999b999999990."
        db ".0999bb99999990."
        db ".0999bbb9999990."
        db ".0999bbbb999990."
        db ".0999bbbbb99990."
        db ".0999bbbb999990."
        db ".0999bbb9999990."
        db ".0999bb99999990."
        db ".0999b999999990."
        db ".09999999999990."
        db ".00000000000000."
        db "......0000......"
        db "....00000000...."

; las órdenes de Ctrl+letra (de la A a la Z; 0: ninguna): las de la familia
ctrl_ord    db 0, O_ABRIR, 0, O_COPIAR, O_DER, O_TODO, O_FRENTE, O_GUARDAR, 0, 0, 0
            db O_CUR, O_GIRAR, 0, O_NEG, O_ALFONDO, O_IMPRIMIR, O_IZQ, O_DUPLICAR, O_SUB, O_CEN
            db O_NUEVO, O_PEGAR, O_GRANDE, O_CORTAR, O_REHACER, O_DESHACER
; las órdenes propias, y las del editor que aquí se hacen de otra manera
            align 4
ordenes_prog dd O_PAG_NUEVA, dia_nueva, O_IMAGEN, imagen_dia, O_DIA_DUP, dia_dup
            dd O_DIA_SUBE, dia_sube, O_DIA_BAJA, dia_baja, O_NOTAS, notas
            dd O_TEMA, tema_elige, O_PRES, pres_inicio, O_PRES_AQUI, pres_aqui
            dd O_TRANS, trans_elige, O_PASOS, pasos_orden
            dd O_FOLLETO2, folleto2, O_FOLLETO3, folleto3, O_FOLLETO6, folleto6
            dd O_IMP_NOTAS, imp_notas, O_DISENO, diseno_elige, O_CLASIF, clasif_orden, 0
; las marcas del menú: dónde va cada una, qué se mira y con qué valor se pone
marcas_prog dd mk_v0, vista, 0, mk_v1, vista, 1, mk_v2, vista, 2, mk_v3, vista, 3
            dd mk_marg, margen_ver, 1, mk_rej, rejilla_ver, 1, mk_ajus, ajustar, 1
            dd mk_pasos, lam_extra + 2, 1, mk_clas, clasif, 1, 0
; las órdenes que se pueden dar sin salir del clasificador (las demás son de
; trabajar en una diapositiva: antes de hacerlas se vuelve a ella)
cl_ordenes  db O_NUEVO, O_ABRIR, O_GUARDAR, O_COMO, O_EXPORTAR, O_IMPRIMIR, O_BORRADOR
            db O_FOLLETO2, O_FOLLETO3, O_FOLLETO6, O_IMP_NOTAS, O_SALIR, O_DESHACER
            db O_REHACER, O_PAG_ANT, O_PAG_SIG, O_PAG_NUEVA, O_PAG_QUITA, O_DIA_DUP
            db O_DIA_SUBE, O_DIA_BAJA, O_TEMA, O_DISENO, O_TRANS, O_PASOS, O_ACERCA
            db O_CLASIF, 0

; ---- textos (los que pide el editor de láminas, y los propios) ----
t_programa  db "Presentaciones", 0
t_ext       db ".PRE", 0
t_pagina    db "Diapositiva ", 0
t_arrastre  db ": arrastre sobre la diapositiva (o haga un clic en ella)", 0
t_exportar  db "Exportar la diapositiva como BMP", 0
t_cambios   db "La presentación tiene cambios sin guardar.", 10, "¿Quiere guardarlos?", 0
t_no_valido db "Ese archivo no es una presentación de Presentaciones (.PRE).", 0
t_no_guardar db "No se ha podido guardar la presentación.", 10
            db "Compruebe que el disco no está lleno ni protegido.", 0
t_exportado db "La diapositiva se ha guardado como imagen BMP.", 0
t_lleno     db "No caben más objetos en la presentación (200).", 0
t_sin_sitio db "No cabe: la presentación ha llegado al límite de texto e imágenes.", 0
t_max_pag   db "La presentación ya tiene todas las diapositivas que caben (32).", 0
t_unica     db "Es la única diapositiva: no se puede quitar.", 0
t_q_quitar  db "Se va a quitar esta diapositiva con todo lo que tiene.", 0
t_bmp_grande db "La imagen es demasiado grande para la presentación.", 0
t_impreso   db "La presentación se ha enviado a la impresora.", 0
t_d_nuevo   db "Presentación nueva", 0
t_n_plant   db "Plantilla:", 0
t_n_tema    db "Tema:", 0
t_d_notas   db "Notas del orador de esta diapositiva", 0
t_d_tema    db "Tema de la presentación", 0
t_d_diseno  db "Diseño de la diapositiva nueva", 0
t_d_diseno2 db "Diseño de esta diapositiva", 0
t_d_trans   db "Transición", 0
t_tema      db "Tema: ", 0
t_notas     db "Notas:", 0
t_sin_notas db "(haga clic aquí para escribir las notas del orador)", 0
t_p_trans   db ", transición: ", 0
t_p_pasos   db ", puntos de uno en uno", 0
t_fin       db "Fin de la presentación. Pulse una tecla para volver.", 0
t_g_titulo  db "Haga clic para poner el título", 0
t_g_texto   db "Haga clic para poner el texto", 0
t_g_subtit  db "Haga clic para poner el subtítulo", 0
t_g_imagen  db "Doble clic para poner una imagen (BMP)", 0
t_ds0       db "Título (portada)", 0
t_ds1       db "Título y texto", 0
t_ds2       db "Título y dos columnas", 0
t_ds3       db "Solo título", 0
t_ds4       db "En blanco", 0
t_ds5       db "Título e imagen", 0
t_tr0       db "Ninguna", 0
t_tr1       db "Cortina (de arriba abajo)", 0
t_tr2       db "Barrido (de izquierda a derecha)", 0
t_tr3       db "Disolver", 0
t_tr1c      db "cortina", 0
t_tr2c      db "barrido", 0
t_tr3c      db "disolver", 0
t_mz0       db "En blanco", 0
t_mz1       db "Una asociación", 0
t_mz2       db "Informe", 0
t_mz3       db "Clase", 0
            align 4
t_disenos   dd t_ds0, t_ds1, t_ds2, t_ds3, t_ds4, t_ds5
disenos     dd ds_portada, ds_texto, ds_columnas, ds_titulo, ds_blanco, ds_imagen
N_DISENOS   equ 6
t_trans     dd t_tr0, t_tr1, t_tr2, t_tr3
t_trans_c   dd t_nada, t_tr1c, t_tr2c, t_tr3c
N_TRANS     equ 4
t_mazos     dd t_mz0, t_mz1, t_mz2, t_mz3
mazos       dd mz_blanco, mz_asoc, mz_informe, mz_clase
N_MAZOS     equ 4
t_guias     dd 0, t_g_titulo, t_g_texto, t_g_texto, t_g_subtit, t_g_imagen
t_acerca_tit db "Acerca de Presentaciones", 0
t_acerca    db "Presentaciones", 10
            db "Diapositivas con texto, listas, formas e imágenes,", 10
            db "con temas a juego, para enseñarlas a pantalla completa", 10
            db "o imprimirlas en folletos.", 10, 10
            db "Copyright (C) 2026 Carlos Sanz y contribuidores.", 10
            db "GPL-2.0 o posterior.", 0
firma       db "PRESENT", 26        ; el principio de un .PRE (8 octetos)

%include "tipos.inc"
%include "lamina.inc"
%include "lamina_ed.inc"
%include "temas.inc"                ; los temas, los diseños y las plantillas (programas/presenta/)
%include "pase.inc"                 ; la presentación a pantalla completa (programas/presenta/)
%include "folletos.inc"             ; imprimir folletos y notas (programas/presenta/)

; =============================================================================
;  La entrada: EBX=base ESI=ruta del módulo EDI=parámetros
; =============================================================================
entrada:
        cld
        call ed_inicia              ; el editor de láminas (ESI, EDI: lo que llega)
        mov esi, dlg_nuevo
        call hvl_clases
        mov esi, dlg_notas
        call hvl_clases
        mov dword [lam_fondo_fn], fondo_tema    ; el motor pinta el fondo del tema
        mov dword [lam_guia_fn], guia_texto     ; y el texto de ejemplo de las cajas
        mov byte [margen_ver], 0
        xor eax, eax                ; una presentación en blanco, con el tema claro
        xor ebx, ebx
        call pre_nueva
        HV pantalla                 ; EAX=ancho EBX=alto: que quepa
        sub ebx, BARRA_H + 8
        cmp ebx, 562
        jae .a
        mov [pre_plant+P_AL], ebx
.a:
        sub eax, 16
        cmp eax, 784
        jae .b
        mov [pre_plant+P_AN], eax
.b:
        mov esi, pre_plant
        xor ebp, ebp
        HV ven_crear
        test eax, eax
        jz .x
        mov esi, [param]
        call nombre_param
        jc .bucle
        call cargar
.bucle:
        HV app_bucle
.x:
        ret

; EAX=plantilla de presentación EBX=tema: un documento nuevo, sin nombre
pre_nueva:
        pushad
        mov eax, DIA_AN
        mov ebx, DIA_AL
        mov ecx, DIA_MARGEN
        call lam_nueva
        mov eax, [esp+16]           ; (el tema; la transición y lo demás, a cero)
        mov [lam_extra], eax
        mov eax, [esp+28]
        mov esi, [mazos+eax*4]
        call mazo_pon
        mov byte [nombre], 0
        call doc_estrena
        popad
        ret

; =============================================================================
;  El procedimiento de la ventana: lo propio, y lo demás al editor de láminas
; =============================================================================
pre_proc:
        cmp byte [pres], 0
        jne pre_pres
        cmp byte [clasif], 0
        jne cl_proc
        cmp eax, WM_LBUTTONDOWN
        je pr_raton
        cmp eax, WM_MOUSEMOVE
        je pr_mueve
        cmp eax, WM_LBUTTONUP
        je pr_suelta
        cmp eax, WM_LBUTTONDBLCLK
        je pr_doble
        cmp eax, WM_KEYDOWN
        je pr_tecla
pre_proc2:
        cmp eax, WM_DESTROY
        jne ed_proc
        call pre_suelta             ; la memoria pedida, de vuelta
        mov ebx, mini
        call lam_bloque_suelta
        mov ebx, notas_blq
        call lam_bloque_suelta
        jmp ed_proc
pre_pres:
        cmp eax, WM_KEYDOWN
        je pres_tecla
        cmp eax, WM_CHAR
        je pres_letra
        cmp eax, WM_LBUTTONDOWN
        je pres_clic
        cmp eax, WM_LBUTTONDBLCLK
        je pres_clic
        cmp eax, WM_MOUSEMOVE
        je hecho
        cmp eax, WM_LBUTTONUP
        je hecho
        cmp eax, WM_COMMAND
        je hecho
        cmp eax, WM_CLOSE
        jne ed_proc
        call pres_acaba
        jmp hecho

; dónde va la vista: entre la tira de miniaturas, la barra, las notas y el pie
medidas:
        mov dword [vx0], TIRA_AN
        mov dword [vy0], TB_AL
        mov eax, [cli_an]
        sub eax, TIRA_AN + BD_AN
        jns .a
        xor eax, eax
.a:
        mov [vw], eax
        mov eax, [cli_al]
        sub eax, TB_AL + BR_PIE + NOTAS_AL + BD_AN
        jns .b
        xor eax, eax
.b:
        mov [vh], eax
        ret

barra_propia:
        ret

; el pie, sin nada elegido: el tema y cómo se presenta
pie_papel:
        mov esi, t_tema
        call hvl_cad
        call tema_ptr
        mov esi, [esi+T_NOMBRE]
        call hvl_cad
        movzx eax, byte [lam_extra+1]
        cmp eax, N_TRANS
        jae .p
        test eax, eax
        jz .p
        mov esi, t_p_trans
        call hvl_cad
        mov esi, [t_trans_c+eax*4]
        call hvl_cad
.p:
        test byte [lam_extra+2], 1
        jz .x
        mov esi, t_p_pasos
        call hvl_cad
.x:
        ret

; ---- teclado: lo propio (F5, Ctrl y las flechas) y lo demás al editor ----
pr_tecla:
        cmp ecx, TC_F1 + 4          ; F5: a presentar (con Mayús, desde esta)
        jne .n1
        mov ecx, O_PRES
        test edx, TM_MAYUS
        jz orden
        mov ecx, O_PRES_AQUI
        jmp orden
.n1:
        cmp ecx, 0x58               ; (Mayús+F5 llega con su propio código)
        jne .n2
        mov ecx, O_PRES_AQUI
        jmp orden
.n2:
        cmp byte [editando], 0
        jne tecla
        cmp ecx, TC_CTRL_IZQ        ; Ctrl y las flechas: la diapositiva cambia de sitio
        je .sube
        cmp ecx, TC_CTRL_DER
        je .baja
        cmp ecx, 0x8D               ; (Ctrl+arriba y Ctrl+abajo, donde el teclado los dé)
        je .sube
        cmp ecx, 0x91
        je .baja
        test edx, TM_CTRL
        jz .resto
        cmp ecx, TC_ARRIBA
        je .sube
        cmp ecx, TC_IZQ
        je .sube
        cmp ecx, TC_ABAJO
        je .baja
        cmp ecx, TC_DER
        je .baja
.resto:
        cmp byte [clasif], 0
        je tecla
        jmp cl_tecla
.sube:
        mov ecx, O_DIA_SUBE
        jmp orden
.baja:
        mov ecx, O_DIA_BAJA
        jmp orden

; ---- ratón: la tira de miniaturas y las notas; lo demás, al editor ----
; (interna) EDX=x + y*65536 -> EAX=x EBX=y; CF=0 y ECX=1 si cae en la tira,
; ECX=2 si en las notas; CF=1 si en otro sitio
pr_zona:
        movsx eax, dx
        mov ebx, edx
        sar ebx, 16
        cmp ebx, TB_AL
        jl .no
        mov ecx, [cli_al]
        sub ecx, BR_PIE
        cmp ebx, ecx
        jge .no
        cmp eax, TIRA_AN
        jl .tira
        sub ecx, NOTAS_AL
        cmp ebx, ecx
        jl .no
        mov ecx, 2
        clc
        ret
.tira:
        mov ecx, 1
        clc
        ret
.no:
        stc
        ret
; (interna) EBX=y de la ventana -> EAX=la diapositiva de la tira que hay a esa
; altura (la última, si cae más abajo)
tira_cual:
        push edx
        mov eax, ebx
        sub eax, TB_AL
        add eax, [tira_y]
        jns .a
        xor eax, eax
.a:
        xor edx, edx
        push ecx
        mov ecx, MINI_PASO
        div ecx
        pop ecx
        mov edx, [lam_pags]
        dec edx
        cmp eax, edx
        jbe .x
        mov eax, edx
.x:
        pop edx
        ret

pr_raton:
        push edx
        push ecx
        call pr_zona
        jc .editor
        cmp ecx, 2
        je .notas
        add esp, 8
        mov dword [br_sobre], 0     ; en la tira: a esa diapositiva, y se coge
        call tira_cual
        mov [tira_de], eax
        mov [tira_a], eax
        cmp eax, [pag]
        je .coge
        call pag_ir
.coge:
        mov byte [tira_coge], 1
        mov ebp, [ven]
        HV ven_capturar
        call inval
        jmp hecho
.notas:
        add esp, 8
        call notas_clic             ; en las notas: a escribirlas ahí mismo
        jmp hecho
.editor:
        call notas_en               ; (si se estaban escribiendo las notas, se acaba)
        jnc .ed1
        call fin_edicion
.ed1:
        pop ecx
        pop edx
        jmp raton
pr_mueve:
        cmp byte [tira_coge], 0
        je raton_mueve
        mov ebx, edx                ; arrastrando una miniatura: a dónde iría
        sar ebx, 16
        call tira_cual
        cmp eax, [tira_a]
        je hecho
        mov [tira_a], eax
        call inval
        jmp hecho
pr_suelta:
        cmp byte [tira_coge], 0
        je raton_suelta
        mov byte [tira_coge], 0
        mov ebp, [ven]
        HV ven_soltar
        mov ecx, [tira_de]
        mov edx, [tira_a]
        cmp ecx, edx
        je .nada
        call dia_mueve
        jmp hecho
.nada:
        call inval
        jmp hecho
pr_doble:
        push edx
        push ecx
        call pr_zona
        jc .editor
        pop ecx
        pop edx
        jmp pr_raton                ; (en la tira o en las notas, como un clic)
.editor:
        ; en el hueco de una imagen: a elegirla
        call en_vista
        jc .ed
        cmp dword [herr], 0
        jne .ed
        call a_pagina
        mov ecx, [pag]
        call holgura
        call lam_en_punto
        test eax, eax
        js .ed
        call lam_ptr
        test byte [esi+LO_BAND], LB_HUECO
        jz .ed
        add esp, 8
        mov ecx, O_IMAGEN
        jmp orden
.ed:
        call notas_en
        jnc .ed1
        call fin_edicion
.ed1:
        pop ecx
        pop edx
        jmp raton_doble

; =============================================================================
;  Las diapositivas: añadir, duplicar y cambiar de sitio
; =============================================================================
; Insertar > Diapositiva nueva: se elige su diseño y va detrás de esta
dia_nueva:
        cmp dword [lam_pags], LAM_MAX_PAG
        jb .cabe
        mov esi, t_max_pag
        jmp aviso
.cabe:
        mov dword [le_titulo], t_d_diseno
        mov dword [le_n], N_DISENOS
        mov dword [le_texto], .texto
        mov dword [le_sel], 1
        call lista_elige
        jne .x
        call fin_edicion
        call instantanea
        mov ecx, [pag]
        call lam_pag_nueva
        mov eax, [le_sel]
        mov esi, [disenos+eax*4]
        mov ecx, [pag]
        inc ecx
        call plantilla_pon
        call tema_colores
        mov byte [ed_todo], 1
        mov byte [modif], 1
        mov eax, [pag]
        inc eax
        call pag_ir
.x:
        ret
.texto:
        mov esi, [t_disenos+eax*4]
        ret

dia_dup:
        cmp dword [lam_pags], LAM_MAX_PAG
        jb .cabe
        mov esi, t_max_pag
        jmp aviso
.cabe:
        call fin_edicion
        call instantanea
        mov ecx, [pag]
        call lam_pag_dup
        jc .mal
        mov byte [ed_todo], 1
        mov byte [modif], 1
        mov eax, [pag]
        inc eax
        jmp pag_ir
.mal:
        call des_vuelve             ; a medias: como estaba
        jmp sin_sitio

dia_sube:
        mov ecx, [pag]
        test ecx, ecx
        jz dia_mueve.x
        lea edx, [ecx-1]
        jmp dia_mueve
dia_baja:
        mov ecx, [pag]
        lea edx, [ecx+1]
        cmp edx, [lam_pags]
        jae dia_mueve.x
; la diapositiva ECX pasa al sitio EDX, y se queda a la vista
dia_mueve:
        call fin_edicion
        call instantanea
        call lam_pag_mueve
        mov byte [ed_todo], 1
        mov byte [modif], 1
        mov eax, edx
        call pag_ir
.x:
        ret

; Insertar > Imagen: como en el editor; y si la diapositiva tiene un hueco
; para una imagen, va a ese hueco (a su medida, sin deformarla)
imagen_dia:
        mov eax, [lam_n]
        push eax
        call imagen
        pop eax
        cmp eax, [lam_n]
        jae .x                      ; (no se ha puesto ninguna)
        xor eax, eax
.b:
        cmp eax, [lam_n]
        jae .x
        call lam_ptr
        movzx ecx, byte [esi+LO_PAG]
        cmp ecx, [pag]
        jne .s
        test byte [esi+LO_BAND], LB_HUECO
        jnz .hueco
.s:
        inc eax
        jmp .b
.hueco:
        mov ebx, esi                ; EBX=el hueco
        push eax
        mov eax, [sel]
        call lam_ptr                ; ESI=la imagen
        mov eax, [esi+LO_AN]        ; ¿qué manda, el ancho o el alto?
        mul dword [ebx+LO_AL]
        mov ecx, eax
        mov eax, [esi+LO_AL]
        mul dword [ebx+LO_AN]
        cmp ecx, eax
        jae .ancho
        mov eax, [esi+LO_AN]        ; el alto del hueco; el ancho, lo que toque
        mul dword [ebx+LO_AL]
        div dword [esi+LO_AL]
        mov ecx, eax
        mov edx, [ebx+LO_AL]
        jmp .pon
.ancho:
        mov eax, [esi+LO_AL]
        mul dword [ebx+LO_AN]
        div dword [esi+LO_AN]
        mov edx, eax
        mov ecx, [ebx+LO_AN]
.pon:
        test ecx, ecx
        jnz .p1
        inc ecx
.p1:
        test edx, edx
        jnz .p2
        inc edx
.p2:
        mov [esi+LO_AN], ecx
        mov [esi+LO_AL], edx
        mov eax, [ebx+LO_AN]        ; centrada en el hueco
        sub eax, ecx
        sar eax, 1
        add eax, [ebx+LO_X]
        mov [esi+LO_X], eax
        mov eax, [ebx+LO_AL]
        sub eax, edx
        sar eax, 1
        add eax, [ebx+LO_Y]
        mov [esi+LO_Y], eax
        pop eax                     ; y el hueco, fuera (la imagen, que va
        call lam_obj_quita          ; detrás en la tabla, baja un puesto)
        mov eax, [sel]
        dec eax
        call elige
        call cambio
.x:
        ret

; =============================================================================
;  Las notas del orador: una caja de texto escondida en la diapositiva
; =============================================================================
; -> EAX=la caja de las notas de la diapositiva [pag] (-1: no tiene)
notas_de:
        push ecx
        mov ecx, [pag]
        call notas_de_pag
        pop ecx
        ret
; ECX=diapositiva -> EAX=su caja de notas (-1: no tiene) y ESI=su ficha
notas_de_pag:
        push edx
        xor eax, eax
.b:
        cmp eax, [lam_n]
        jae .no
        call lam_ptr
        movzx edx, byte [esi+LO_PAG]
        cmp edx, ecx
        jne .s
        cmp byte [esi+LO_PAPEL], LP_NOTAS
        je .x
.s:
        inc eax
        jmp .b
.no:
        mov eax, -1
.x:
        pop edx
        ret

notas:
        call fin_edicion
        mov byte [notas_buf], 0     ; las de ahora, al cuadro de texto
        call notas_de
        test eax, eax
        js .d
        mov ecx, [esi+LO_LARGO]
        cmp ecx, NOTAS_MAX
        jbe .c
        mov ecx, NOTAS_MAX
.c:
        mov esi, [esi+LO_DAT]
        add esi, [lam_dat]
        mov edi, notas_buf
        rep movsb
        mov byte [edi], 0
.d:
        mov ebp, [ven]
        mov esi, dlg_notas
        mov edi, dlg_notas + P_TAM
        mov ebx, notas_gancho
        xor edx, edx
        HV dlg_modal
        cmp eax, ID_ACEPTAR
        jne .x
        mov esi, notas_buf          ; sin los retornos de carro; EDX=cuánto es
        mov edi, notas_buf
.l:
        lodsb
        test al, al
        jz .l1
        cmp al, 13
        je .l
        stosb
        jmp .l
.l1:
        mov edx, edi
        sub edx, notas_buf
        call instantanea
        call notas_de
        test eax, eax
        jns .hay
        test edx, edx
        jz .x
        mov al, LT_TEXTO            ; no tenía: una caja nueva, escondida
        mov cl, [pag]
        call lam_obj_nuevo
        jc sin_sitio
        mov byte [esi+LO_PAPEL], LP_NOTAS
        mov byte [esi+LO_BAND], LB_OCULTO
        mov dword [esi+LO_AN], 600  ; (notas_norma le da sus medidas)
        mov dword [esi+LO_AL], 400
        jmp .pon
.hay:
        test edx, edx               ; sin texto: la caja, fuera
        jnz .pon
        call lam_obj_quita
        mov ecx, [sel]              ; (lo elegido puede haber bajado un puesto)
        test ecx, ecx
        js .hecho
        cmp eax, ecx
        jae .q1
        dec ecx
.q1:
        mov eax, ecx
        call elige
        jmp .hecho
.pon:
        xor ebx, ebx
        mov ecx, -1
        mov esi, notas_buf
        call lam_dat_cambia
        jc sin_sitio
.hecho:
        mov byte [modif], 1
        call inval
.x:
        ret
notas_gancho:
        cmp eax, DM_INICIO
        je .ini
        cmp eax, WM_COMMAND
        jne .no
        cmp ecx, ID_ACEPTAR
        jne .no
        pushad
        mov ecx, 70
        HV ven_hija_id
        mov ebp, eax
        mov eax, WM_GETTEXT
        mov ecx, NOTAS_MAX
        mov edx, notas_buf
        HV msg_enviar
        popad
        jmp .no
.ini:
        pushad
        mov ecx, 70
        HV ven_hija_id
        mov ebp, eax
        mov eax, WM_SETTEXT
        mov edx, notas_buf
        HV msg_enviar
        popad
.no:
        xor eax, eax
        ret

; =============================================================================
;  Pintar
; =============================================================================
pintar:
        cmp byte [pres], 0
        jne pres_pinta
        cmp byte [clasif], 0
        je .normal
        HV dc_pintar                ; el clasificador: las miniaturas, en rejilla
        call minis_al_dia
        call pinta_clasif           ; (antes que la barra y el pie, como la tira)
        call pinta_botones
        call pinta_estado
        HV dc_cerrar
        jmp hecho
.normal:
        HV dc_pintar
        call minis_al_dia
        call pinta_tira             ; (antes que la barra y el pie, que tapan
        call pinta_botones          ; la miniatura que asome por arriba o por abajo)
        call pinta_estado
        call pinta_notas
        call pinta_vista
        HV dc_cerrar
        jmp hecho

; ---- las miniaturas, al día: cada diapositiva se dibuja y se reduce solo
;      cuando ha cambiado (la que se ve, con cada cambio; las demás, cuando
;      cambia más de una: otro tema, otro orden, deshacer, otro documento) ----
minis_al_dia:
        pushad
        mov ecx, [lam_an]           ; (antes que nada, la memoria de la lámina:
        imul ecx, [lam_al]          ; si falta, que no sea por las miniaturas)
        mov ebx, lam_buf
        xor al, al
        call lam_bloque
        mov ecx, [lam_pags]         ; sitio para las miniaturas (se pide al
        add ecx, 3                  ; usarlo, con holgura para tres más)
        cmp ecx, LAM_MAX_PAG
        jbe .m1
        mov ecx, LAM_MAX_PAG
.m1:
        imul ecx, MINI_AN * MINI_AL
        mov eax, [lam_pags]
        imul eax, MINI_AN * MINI_AL
        cmp eax, [mini+4]
        jbe .m2
        mov ebx, mini
        xor al, al
        call lam_bloque
        jnc .m3
        call al_dia                 ; sin memoria: no hay miniaturas
        popad
        ret
.m3:
        mov byte [ed_todo], 1
.m2:
        cmp byte [ed_todo], 0
        je .a
        mov byte [ed_todo], 0
        mov edi, mini_ok
        mov ecx, LAM_MAX_PAG
        xor al, al
        rep stosb
.a:
        cmp byte [lam_sucio], 0
        je .b
        mov eax, [pag]
        mov byte [mini_ok+eax], 0
.b:
        xor ebx, ebx
.o:
        cmp ebx, [lam_pags]
        jae .act
        cmp ebx, [pag]
        je .os
        cmp byte [mini_ok+ebx], 0
        jne .os
        call .sin_guia
        mov byte [lam_sucio], 1     ; (en lam_buf ya no está la que se ve)
.os:
        inc ebx
        jmp .o
.act:
        ; la que se ve: su miniatura sale de la lámina ya dibujada, salvo que
        ; tenga cajas vacías con su texto de ejemplo (en la miniatura no van):
        ; entonces se dibuja una vez más, sin ellos
        mov ebx, [pag]
        cmp byte [mini_ok+ebx], 0
        jne .act2
        call .con_guia
        jnc .act1
        call .sin_guia
        mov byte [lam_sucio], 1
        jmp .act2
.act1:
        call al_dia
        call .reduce
.act2:
        call al_dia
        popad
        ret
; (interna) dibuja la diapositiva EBX sin los textos de ejemplo de las cajas
; vacías y la deja en su miniatura
.sin_guia:
        push dword [lam_guia_fn]
        mov dword [lam_guia_fn], 0
        mov ecx, ebx
        call lam_pinta
        call .reduce
        pop dword [lam_guia_fn]
        ret
; (interna) CF=1 si la diapositiva EBX tiene alguna caja vacía con texto de ejemplo
.con_guia:
        push eax
        push esi
        xor eax, eax
.cg:
        cmp eax, [lam_n]
        jae .cg_no
        call lam_ptr
        movzx ecx, byte [esi+LO_PAG]
        cmp ecx, ebx
        jne .cg1
        cmp byte [esi+LO_TIPO], LT_TEXTO
        jne .cg1
        cmp dword [esi+LO_LARGO], 0
        jne .cg1
        mov cl, [esi+LO_PAPEL]
        dec cl
        cmp cl, LP_IMAGEN - 1
        jbe .cg_si
.cg1:
        inc eax
        jmp .cg
.cg_si:
        pop esi
        pop eax
        stc
        ret
.cg_no:
        pop esi
        pop eax
        clc
        ret
; (interna) lam_buf, reducida, a la miniatura EBX
.reduce:
        pushad
        mov byte [mini_ok+ebx], 1
        imul edi, ebx, MINI_AN * MINI_AL
        add edi, [mini]
        mov ecx, MINI_AN
        mov edx, MINI_AL
        mov ebx, [suave]
        call lam_reduce
        cmp byte [lam_sin_mem], 0   ; (sin lámina dibujada: en blanco)
        je .rx
        mov ecx, MINI_AN * MINI_AL
        mov al, C_BLANCO
        rep stosb
.rx:
        popad
        ret

; ---- la tira de miniaturas: cada una con su número; la que se ve, con un
;      marco petróleo. Arrastrando una, una raya ocre dice dónde iría ----
pinta_tira:
        pushad
        mov edx, [cli_al]           ; EDX=el alto de la tira
        sub edx, TB_AL + BR_PIE
        jle .x
        mov [tira_al], edx
        mov eax, [pag]              ; que la que se ve quede dentro
        imul eax, MINI_PASO
        cmp eax, [tira_y]
        jge .v1
        mov [tira_y], eax
.v1:
        add eax, MINI_PASO
        sub eax, edx
        cmp eax, [tira_y]
        jle .v2
        mov [tira_y], eax
.v2:
        COLOR C_PIEDRA
        xor eax, eax
        mov ebx, TB_AL
        mov ecx, TIRA_AN
        HV dc_rect
        COLOR C_ARENA_OSC
        mov eax, TIRA_AN - 1
        mov ecx, 1
        HV dc_rect
        xor edi, edi                ; EDI=la diapositiva
.b:
        cmp edi, [lam_pags]
        jae .x
        mov ebx, edi
        imul ebx, MINI_PASO
        add ebx, TB_AL + 8
        sub ebx, [tira_y]           ; EBX=su y en la ventana
        mov eax, [cli_al]
        sub eax, BR_PIE
        cmp ebx, eax
        jge .x
        lea eax, [ebx+MINI_AL]
        cmp eax, TB_AL
        jle .s
        cmp edi, [pag]              ; el marco
        jne .m1
        COLOR C_PETROLEO
        mov eax, MINI_X - 4
        sub ebx, 4
        mov ecx, MINI_AN + 8
        mov edx, MINI_AL + 8
        HV dc_rect
        add ebx, 4
        jmp .m2
.m1:
        COLOR C_ARENA_OSC
        mov eax, MINI_X - 1
        dec ebx
        mov ecx, MINI_AN + 2
        mov edx, MINI_AL + 2
        HV dc_marco
        inc ebx
.m2:
        mov eax, MINI_X
        mov ecx, MINI_AN
        mov edx, MINI_AL
        lea esi, [edi+1]            ; (si no ha habido memoria para ella, solo su marco)
        imul esi, MINI_AN * MINI_AL
        cmp esi, [mini+4]
        ja .m3
        sub esi, MINI_AN * MINI_AL
        add esi, [mini]
        HV dc_copiar
.m3:
        push edi                    ; su número
        lea eax, [edi+1]
        mov edi, num_txt
        call hvl_num
        mov byte [edi], 0
        pop edi
        COLOR C_TINTA
        mov eax, 2
        mov esi, num_txt
        HV dc_texto
        cmp byte [tira_coge], 0     ; arrastrando: la raya de dónde iría
        je .s
        cmp edi, [tira_a]
        jne .s
        COLOR C_OCRE
        mov eax, [tira_a]
        cmp eax, [tira_de]
        je .s
        jb .r1
        add ebx, MINI_AL + 5        ; (hacia abajo: debajo de esa)
        jmp .r2
.r1:
        sub ebx, 8
.r2:
        mov eax, MINI_X - 6
        mov ecx, MINI_AN + 12
        mov edx, 3
        HV dc_rect
.s:
        inc edi
        jmp .b
.x:
        popad
        ret

; ---- las notas del orador, debajo de la diapositiva. Las escribe el motor
;      en un búfer del tamaño del recuadro (una copia de la ficha de su caja,
;      puesta ahí); escribiéndolas, el texto sube lo que haga falta para que
;      se vea el cursor ----
pinta_notas:
        pushad
        mov ebx, [cli_al]
        sub ebx, BR_PIE + NOTAS_AL
        cmp ebx, TB_AL
        jl .x
        mov [n_y], ebx
        mov ecx, [cli_an]
        sub ecx, TIRA_AN
        jle .x
        mov [n_an], ecx
        COLOR C_BLANCO
        mov eax, TIRA_AN
        mov edx, NOTAS_AL
        HV dc_rect
        COLOR C_ARENA_OSC
        mov edx, 1
        HV dc_rect
        cmp ecx, NOTAS_X + 80       ; (si casi no hay sitio, solo el recuadro)
        jl .x
        call notas_de
        test eax, eax
        jns .hay
        call .rotulo
        COLOR C_ARENA_OSC
        mov eax, TIRA_AN + NOTAS_X + 4
        mov ebx, [n_y]
        add ebx, 4
        mov esi, t_sin_notas
        HV dc_texto
        jmp .x
.hay:
        call notas_norma
        mov ecx, [n_an]             ; el búfer del recuadro (se pide al usarlo)
        imul ecx, NOTAS_AL - 1
        mov ebx, notas_blq
        xor al, al
        call lam_bloque
        jc .x
        mov edi, [notas_blq]
        mov al, C_BLANCO
        rep stosb
        mov dword [n_cur_al], 0
        call notas_en               ; escribiéndolas: que se vea el cursor
        jc .cursor
        mov dword [n_sube], 0
        jmp .dibuja
.cursor:
        mov eax, [sel]
        call lam_ptr
        call lam_compone
        mov eax, [cur]
        call lam_sitio_xy           ; EAX=x EBX=y ECX=alto, desde la esquina de la caja
        mov [n_cur_x], eax
        mov [n_cur_y], ebx
        mov [n_cur_al], ecx
        lea eax, [ebx+ecx+2-NOTAS_AL]   ; lo menos que tiene que subir...
        test eax, eax
        jns .c1
        xor eax, eax
.c1:
        cmp [n_sube], eax
        jge .c2
        mov [n_sube], eax
.c2:
        lea eax, [ebx-3]            ; ...y lo más
        cmp [n_sube], eax
        jle .dibuja
        mov [n_sube], eax
.dibuja:
        call notas_de               ; una copia de su ficha, en su sitio del búfer
        mov edi, ficha_tmp
        mov ecx, LO_TAM
        rep movsb
        mov esi, ficha_tmp
        mov dword [esi+LO_X], NOTAS_X
        mov eax, -1
        sub eax, [n_sube]
        mov [esi+LO_Y], eax
        mov byte [esi+LO_TINTA], C_TINTA
        mov byte [esi+LO_FONDO], LAM_NADA
        mov byte [esi+LO_BORDE], LAM_NADA
        mov byte [esi+LO_BAND], 0
        mov edi, [notas_blq]
        mov ecx, [n_an]
        mov edx, NOTAS_AL - 1
        call lam_destino
        call lam_pinta_obj
        xor edi, edi
        call lam_destino
        mov eax, TIRA_AN
        mov ebx, [n_y]
        inc ebx
        mov ecx, [n_an]
        mov edx, NOTAS_AL - 1
        mov esi, [notas_blq]
        HV dc_copiar
        call .rotulo
        cmp dword [n_cur_al], 0     ; y el cursor
        je .x
        cmp byte [foco], 0
        je .x
        mov eax, [n_cur_x]
        add eax, TIRA_AN + NOTAS_X
        mov ebx, [n_cur_y]
        sub ebx, [n_sube]
        add ebx, [n_y]
        mov ecx, 2
        mov edx, [n_cur_al]
        COLOR C_TINTA
        HV dc_rect
.x:
        popad
        ret
; (interna) el rótulo «Notas:», a la izquierda
.rotulo:
        pushad
        COLOR C_BLANCO
        mov eax, TIRA_AN
        mov ebx, [n_y]
        inc ebx
        mov ecx, NOTAS_X
        mov edx, NOTAS_AL - 1
        HV dc_rect
        COLOR C_PETROLEO
        NEGRITA 1
        mov eax, TIRA_AN + 6
        add ebx, 3
        mov esi, t_notas
        HV dc_texto
        NEGRITA 0
        popad
        ret

; -> CF=1 si se está escribiendo en las notas (la caja en la que se escribe
; es la de las notas de la diapositiva)
notas_en:
        push eax
        push esi
        cmp byte [editando], 0
        je .no
        mov eax, [sel]
        test eax, eax
        js .no
        call lam_ptr
        cmp byte [esi+LO_PAPEL], LP_NOTAS
        jne .no
        pop esi
        pop eax
        stc
        ret
.no:
        pop esi
        pop eax
        clc
        ret
; (interna) la caja de las notas de la diapositiva, con las medidas del
; recuadro y la letra de siempre: así el motor parte las líneas igual al
; pintarlas que al mover el cursor por ellas. (Al imprimirlas se les dan
; otras: hoja_notas)
notas_norma:
        pushad
        call notas_de
        test eax, eax
        js .x
        xor eax, eax
        mov [esi+LO_X], eax
        mov [esi+LO_Y], eax
        mov [esi+LO_LETRA], al
        mov [esi+LO_CUERPO], al
        mov [esi+LO_ESTILO], al
        mov [esi+LO_ALIN], al
        mov byte [esi+LO_BAND], LB_OCULTO
        mov eax, [cli_an]
        sub eax, TIRA_AN + NOTAS_X + 8
        cmp eax, 80
        jge .a
        mov eax, 80
.a:
        mov [esi+LO_AN], eax
        mov dword [esi+LO_AL], 4000
.x:
        popad
        ret
; un clic en el recuadro de las notas, en (EAX, EBX) de la ventana: se escribe
; ahí mismo (si la diapositiva no tenía notas, se le pone su caja, escondida)
notas_clic:
        push eax
        push ebx
        call notas_en
        jc .dentro
        call fin_edicion
        mov dword [herr], 0
        call notas_de
        test eax, eax
        jns .hay
        call instantanea
        mov al, LT_TEXTO
        mov cl, [pag]
        call lam_obj_nuevo
        jc .lleno
        mov byte [esi+LO_PAPEL], LP_NOTAS
        mov byte [esi+LO_BAND], LB_OCULTO
.hay:
        call elige
        mov byte [editando], 1
        mov dword [n_sube], 0
.dentro:
        call notas_norma
        pop ebx
        pop eax
        sub eax, TIRA_AN + NOTAS_X  ; el cursor, a donde se ha pulsado
        sub ebx, [n_y]
        add ebx, [n_sube]
        push eax
        mov eax, [sel]
        call lam_ptr
        call lam_compone
        pop eax
        call lam_xy_sitio
        mov [cur], eax
        mov byte [tecleando], 0
        jmp inval
.lleno:
        pop ebx
        pop eax
        jmp sin_sitio

; =============================================================================
;  Cambiar el diseño de una diapositiva ya hecha
; =============================================================================
; (interna) DL=papel -> EAX=la caja de la diapositiva [pag] que lo tiene (-1:
; ninguna) y ESI=su ficha
dia_caja:
        push ecx
        xor eax, eax
.b:
        cmp eax, [lam_n]
        jae .no
        call lam_ptr
        movzx ecx, byte [esi+LO_PAG]
        cmp ecx, [pag]
        jne .s
        cmp [esi+LO_PAPEL], dl
        je .x
.s:
        inc eax
        jmp .b
.no:
        mov eax, -1
.x:
        pop ecx
        test eax, eax
        ret
; (interna) AL=papel: CF=1 si el diseño [ds_nuevo] no trae una caja con él
ds_tiene:
        push ebx
        mov ebx, [ds_nuevo]
.b:
        cmp byte [ebx], 0
        je .no
        cmp [ebx+10], al
        je .si
        add ebx, PL_TAM
        jmp .b
.no:
        pop ebx
        stc
        ret
.si:
        pop ebx
        clc
        ret
; Formato > Diseño de esta diapositiva. Las cajas que ya tiene van al sitio y
; al tamaño que les da el diseño nuevo, cada una por su papel (el título, el
; texto...), con lo que tengan escrito; las que le faltan se añaden; las que
; sobran se quitan si están vacías (las que tienen algo se quedan donde
; están). El subtítulo de una portada y el texto de las demás hacen el mismo
; oficio: si el diseño trae uno y la diapositiva tiene el otro, se aprovecha
diseno_elige:
        mov dword [le_titulo], t_d_diseno2
        mov dword [le_n], N_DISENOS
        mov dword [le_texto], dia_nueva.texto
        mov dword [le_sel], 1
        call lista_elige
        jne .x
        call fin_edicion
        call instantanea
        call sel_nada
        mov eax, [le_sel]
        mov eax, [disenos+eax*4]
        mov [ds_nuevo], eax
        mov al, LP_CUERPO           ; el subtítulo pasa a ser el texto...
        mov ah, LP_SUBTIT
        call .oficio
        mov al, LP_SUBTIT           ; ...o el texto, el subtítulo
        mov ah, LP_CUERPO
        call .oficio
        mov eax, [lam_n]            ; lo que sobra y está vacío, fuera
.q:
        dec eax
        js .pon
        call lam_ptr
        movzx ecx, byte [esi+LO_PAG]
        cmp ecx, [pag]
        jne .q
        mov cl, [esi+LO_PAPEL]
        test cl, cl
        jz .q
        cmp cl, LP_IMAGEN
        ja .q
        cmp dword [esi+LO_LARGO], 0
        jne .q
        push eax
        mov al, cl
        call ds_tiene
        pop eax
        jnc .q
        call lam_obj_quita
        jmp .q
.pon:
        mov al, [pag]               ; y las cajas del diseño: a su sitio las que
        mov [pl_pag], al            ; hay, y nuevas las que faltan
        mov byte [pl_peq], 1
        mov ebx, [ds_nuevo]
.e:
        cmp byte [ebx], 0
        je .hecho
        mov dl, [ebx+10]
        call dia_caja
        js .falta
        call plantilla_sitio
        mov al, [ebx+9]             ; con el tamaño de letra y la alineación del
        mov [esi+LO_CUERPO], al     ; diseño; y lista o no, como él diga (si ya
        mov al, [ebx+2]             ; era una lista con números, lo sigue siendo)
        mov [esi+LO_ALIN], al
        mov al, [esi+LO_BAND]
        mov ah, al
        and al, 0xFF - LB_LISTA
        test byte [ebx+11], LB_VINETAS
        jz .e0
        or al, LB_VINETAS
        test ah, LB_NUMEROS
        jz .e0
        xor al, LB_LISTA
.e0:
        mov [esi+LO_BAND], al
        jmp .e1
.falta:
        call plantilla_uno
.e1:
        add ebx, PL_TAM
        jmp .e
.hecho:
        call tema_colores
        call cambio
.x:
        ret
; (interna) si el diseño nuevo trae una caja con el papel AL y la diapositiva
; no la tiene, pero tiene la del papel AH (que el diseño no trae), esa cambia
; de papel
.oficio:
        call ds_tiene
        jc .o1
        mov dl, al
        push eax
        call dia_caja
        pop eax
        jns .o1
        xchg al, ah
        call ds_tiene
        xchg al, ah
        jnc .o1
        mov dl, ah
        push eax
        call dia_caja
        pop eax
        js .o1
        mov [esi+LO_PAPEL], al
.o1:
        ret

; =============================================================================
;  El clasificador: todas las miniaturas en una rejilla, para ver la
;  presentación entera y ordenarla. Un clic elige una diapositiva;
;  arrastrándola cambia de sitio; un doble clic (o Intro, o Esc) vuelve a
;  ella. Mientras está puesto solo valen las órdenes de la presentación
;  entera (cl_ordenes); cualquier otra vuelve antes a la diapositiva
; =============================================================================
; (del editor de láminas) antes de cada orden: ECX=la orden
orden_antes:
        cmp byte [clasif], 0
        je .x
        push esi
        mov esi, cl_ordenes
.b:
        cmp byte [esi], 0
        je .sale
        cmp [esi], cl
        je .vale
        inc esi
        jmp .b
.sale:
        call clasif_sale
.vale:
        pop esi
.x:
        ret
; Ver > Clasificador de diapositivas: ponerlo o quitarlo
clasif_orden:
        cmp byte [clasif], 0
        jne clasif_sale
        call fin_edicion
        call sel_nada
        mov dword [herr], 0
        mov byte [clasif], 1
        mov byte [cl_coge], 0
        xor eax, eax                ; (las barras de desplazamiento, escondidas)
        jmp clasif_sale.barras
clasif_sale:
        cmp byte [clasif], 0
        je .x
        mov byte [clasif], 0
        mov byte [cl_coge], 0
        mov eax, 1
.barras:
        push ebp
        mov ebp, [barra_v]
        HV ven_mostrar
        mov ebp, [barra_h]
        HV ven_mostrar
        pop ebp
        call marcas
        call inval
.x:
        ret
; el procedimiento de la ventana con el clasificador puesto
cl_proc:
        cmp eax, WM_LBUTTONDOWN
        je cl_raton
        cmp eax, WM_LBUTTONDBLCLK
        je cl_doble
        cmp eax, WM_MOUSEMOVE
        je cl_mueve
        cmp eax, WM_LBUTTONUP
        je cl_suelta
        cmp eax, WM_KEYDOWN
        je pr_tecla
        cmp eax, WM_CHAR
        je hecho
        cmp eax, WM_MOUSEWHEEL
        je hecho
        jmp pre_proc2
; (interna) cuántas miniaturas caben en una fila -> ECX
cl_cols:
        push eax
        push edx
        mov eax, [cli_an]
        sub eax, CL_X0
        jns .a
        xor eax, eax
.a:
        xor edx, edx
        mov ecx, CL_PASO_X
        div ecx
        mov ecx, eax
        test ecx, ecx
        jnz .x
        inc ecx
.x:
        pop edx
        pop eax
        ret
; (interna) EDX=x + y*65536 de la ventana -> EAX=la diapositiva que hay en ese
; sitio de la rejilla (la última, si cae más allá); CF=1 si es en la barra
cl_cual:
        push ebx
        push ecx
        push edx
        movsx eax, dx
        mov ebx, edx
        sar ebx, 16
        cmp ebx, TB_AL
        jl .barra
        call cl_cols
        sub eax, CL_X0 - 12         ; la columna
        jns .a
        xor eax, eax
.a:
        xor edx, edx
        push ecx
        mov ecx, CL_PASO_X
        div ecx
        pop ecx
        cmp eax, ecx
        jb .b
        lea eax, [ecx-1]
.b:
        push eax
        mov eax, ebx                ; la fila
        sub eax, TB_AL + CL_Y0 - 8
        add eax, [cl_y]
        jns .c
        xor eax, eax
.c:
        xor edx, edx
        mov ebx, CL_PASO_Y
        div ebx
        imul eax, ecx
        pop edx
        add eax, edx
        mov edx, [lam_pags]
        dec edx
        cmp eax, edx
        jbe .d
        mov eax, edx
.d:
        pop edx
        pop ecx
        pop ebx
        clc
        ret
.barra:
        pop edx
        pop ecx
        pop ebx
        stc
        ret
cl_raton:
        call cl_cual
        jc raton                    ; (la barra de botones, como siempre)
        mov dword [br_sobre], 0
        mov [cl_de], eax
        mov [cl_a], eax
        cmp eax, [pag]
        je .coge
        call pag_ir
.coge:
        mov byte [cl_coge], 1
        mov ebp, [ven]
        HV ven_capturar
        call inval
        jmp hecho
cl_doble:
        call cl_cual
        jc raton
        cmp eax, [pag]
        je .va
        call pag_ir
.va:
        call clasif_sale
        jmp hecho
cl_mueve:
        cmp byte [cl_coge], 0
        je raton_mueve
        call cl_cual
        jc hecho
        cmp eax, [cl_a]
        je hecho
        mov [cl_a], eax
        call inval
        jmp hecho
cl_suelta:
        cmp byte [cl_coge], 0
        je raton_suelta
        mov byte [cl_coge], 0
        mov ebp, [ven]
        HV ven_soltar
        mov ecx, [cl_de]
        mov edx, [cl_a]
        cmp ecx, edx
        je .nada
        call dia_mueve
        jmp hecho
.nada:
        call inval
        jmp hecho
; el teclado: las flechas van de una a otra; Intro y Esc vuelven a la
; diapositiva; Supr la quita (lo demás llega aquí desde pr_tecla)
cl_tecla:
        cmp ecx, TC_CTRL
        jae tecla
        cmp ecx, TC_ESC
        je .sale
        cmp ecx, TC_INTRO
        je .sale
        cmp ecx, TC_REPAG
        je tecla
        cmp ecx, TC_AVPAG
        je tecla
        cmp ecx, TC_SUPR
        jne .n1
        mov ecx, O_PAG_QUITA
        jmp orden
.n1:
        mov eax, [pag]
        mov ebx, ecx
        call cl_cols
        cmp ebx, TC_IZQ
        jne .n2
        dec eax
        jmp .ir
.n2:
        cmp ebx, TC_DER
        jne .n3
        inc eax
        jmp .ir
.n3:
        cmp ebx, TC_ARRIBA
        jne .n4
        sub eax, ecx
        jmp .ir
.n4:
        cmp ebx, TC_ABAJO
        jne hecho
        add eax, ecx
.ir:
        test eax, eax
        js hecho
        cmp eax, [lam_pags]
        jae hecho
        call pag_ir
        jmp hecho
.sale:
        call clasif_sale
        jmp hecho
; pinta la rejilla: cada miniatura con su número debajo; la de la diapositiva
; en la que se está, con un marco petróleo. Arrastrando una, una raya ocre
; dice dónde iría
pinta_clasif:
        pushad
        mov edx, [cli_al]
        sub edx, TB_AL + BR_PIE
        jle .x
        mov [tira_al], edx
        COLOR C_PIEDRA
        xor eax, eax
        mov ebx, TB_AL
        mov ecx, [cli_an]
        HV dc_rect
        call cl_cols
        mov [cl_n], ecx
        mov eax, [pag]              ; que la que se ve quede dentro
        xor edx, edx
        div ecx
        imul eax, CL_PASO_Y
        cmp eax, [cl_y]
        jge .v1
        mov [cl_y], eax
.v1:
        add eax, CL_PASO_Y + CL_Y0
        sub eax, [tira_al]
        cmp eax, [cl_y]
        jle .v2
        mov [cl_y], eax
.v2:
        xor edi, edi                ; EDI=la diapositiva
.b:
        cmp edi, [lam_pags]
        jae .x
        mov eax, edi
        xor edx, edx
        div dword [cl_n]
        imul ebx, eax, CL_PASO_Y
        add ebx, TB_AL + CL_Y0
        sub ebx, [cl_y]             ; EBX=su y en la ventana
        imul ebp, edx, CL_PASO_X
        add ebp, CL_X0              ; EBP=su x
        mov eax, [cli_al]
        sub eax, BR_PIE
        cmp ebx, eax
        jge .x
        lea eax, [ebx+CL_PASO_Y]
        cmp eax, TB_AL
        jle .s
        cmp edi, [pag]              ; el marco
        jne .m1
        COLOR C_PETROLEO
        lea eax, [ebp-4]
        sub ebx, 4
        mov ecx, MINI_AN + 8
        mov edx, MINI_AL + 8
        HV dc_rect
        add ebx, 4
        jmp .m2
.m1:
        COLOR C_ARENA_OSC
        lea eax, [ebp-1]
        dec ebx
        mov ecx, MINI_AN + 2
        mov edx, MINI_AL + 2
        HV dc_marco
        inc ebx
.m2:
        mov eax, ebp
        mov ecx, MINI_AN
        mov edx, MINI_AL
        lea esi, [edi+1]            ; (si no ha habido memoria para ella, solo su marco)
        imul esi, MINI_AN * MINI_AL
        cmp esi, [mini+4]
        ja .m3
        sub esi, MINI_AN * MINI_AL
        add esi, [mini]
        HV dc_copiar
.m3:
        push edi                    ; su número, debajo
        lea eax, [edi+1]
        mov edi, num_txt
        call hvl_num
        mov byte [edi], 0
        pop edi
        COLOR C_TINTA
        lea eax, [ebp+MINI_AN/2-8]
        push ebx
        add ebx, MINI_AL + 6
        mov esi, num_txt
        HV dc_texto
        pop ebx
        cmp byte [cl_coge], 0       ; arrastrando: la raya de dónde iría
        je .s
        cmp edi, [cl_a]
        jne .s
        COLOR C_OCRE
        mov eax, [cl_a]
        cmp eax, [cl_de]
        je .s
        lea eax, [ebp-11]           ; (hacia atrás: delante de esa)
        jb .r1
        lea eax, [ebp+MINI_AN+8]    ; (hacia delante: detrás)
.r1:
        sub ebx, 4
        mov ecx, 3
        mov edx, MINI_AL + 8
        HV dc_rect
.s:
        inc edi
        jmp .b
.x:
        popad
        ret

%include "barra.inc"
%include "hvplib.inc"

        section .bss
            TIPOS_BSS
            LAMINA_BSS
            LAMINA_ED_BSS
            BARRA_BSS
tira_y      resd 1                  ; lo que está desplazada la tira
tira_al     resd 1
tira_de     resd 1                  ; la miniatura que se arrastra
tira_a      resd 1                  ; y a qué sitio iría
n_y         resd 1                  ; el recuadro de las notas: dónde empieza,
n_an        resd 1                  ; su ancho,
n_sube      resd 1                  ; lo que está subido el texto (escribiendo)
n_cur_x     resd 1                  ; y el cursor
n_cur_y     resd 1
n_cur_al    resd 1
ds_nuevo    resd 1                  ; el diseño que se le pone a una diapositiva
cl_y        resd 1                  ; el clasificador: lo que está desplazado,
cl_n        resd 1                  ; las miniaturas de una fila,
cl_de       resd 1                  ; la que se arrastra
cl_a        resd 1                  ; y a qué sitio iría
pc_estilo   resd 1                  ; la ventana, como estaba antes de presentar
pc_menu     resd 1
pc_x        resd 1
pc_y        resd 1
pc_an       resd 1
pc_al       resd 1
pres_pag    resd 1                  ; presentando: la diapositiva,
pres_paso   resd 1                  ; los puntos que se ven de sus listas,
pres_num    resd 1                  ; el número que se va tecleando
pres_x      resd 1                  ; y dónde cae y cuánto mide en la pantalla
pres_y      resd 1
pres_an     resd 1
pres_al     resd 1
pres_src    resd 1                  ; de dónde se pinta (lam_buf, o pres_buf)
pan_an      resd 1
pan_al      resd 1
tr_k        resd 1
fo_modo     resd 1
fo_k        resd 1
tira_coge   resb 1
clasif      resb 1                  ; está puesto el clasificador
cl_coge     resb 1                  ; se está arrastrando una de sus miniaturas
pres        resb 1                  ; se está presentando a pantalla completa
pres_fin    resb 1                  ; se ha pasado de la última
mini_ok     resb LAM_MAX_PAG        ; qué miniaturas están al día
            alignb 4
ficha_tmp   resb LO_TAM
; lo grande son bloques que se piden al usarlos (lam_bloque) y se devuelven
; al acabar de presentar o de imprimir (pre_suelta) y al cerrar
mini        resd 3                  ; las miniaturas (MINI_AN x MINI_AL cada una)
pres_buf    resd 3                  ; la diapositiva, al tamaño de la pantalla
hoja_buf    resd 3                  ; la hoja de un folleto (y, al presentar, lo
                                    ; que se va poniendo en una transición)
notas_blq   resd 3                  ; el recuadro de las notas, dibujado
HVP_FIN
