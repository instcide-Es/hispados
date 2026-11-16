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
;    - los diseños: título, título y texto, dos columnas, solo título, en
;      blanco, título e imagen. Sus cajas dicen para qué son («Haga clic para
;      poner el título») hasta que se escribe en ellas;
;    - las listas: cada párrafo con su punto; Tab y Mayús+Tab cambian de nivel;
;    - los temas: el fondo, el adorno y los colores y tipos de letra a juego
;      para toda la presentación;
;    - la presentación a pantalla completa (F5; Mayús+F5 desde la diapositiva
;      en la que se está), con transiciones y, si se quiere, los puntos de las
;      listas de uno en uno;
;    - las notas del orador de cada diapositiva, que se ven debajo;
;    - imprimir: una diapositiva por página, folletos de 2, 3 o 6, o las notas.
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
NOTAS_MAX   equ 1000                ; letras de las notas de una diapositiva
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
        dw O_VISTA
        db "&Diapositiva entera", 0
        dw O_VISTA + 1
        db "Al &50 %", 0
        dw O_VISTA + 2
        db "Al &75 %", 0
        dw O_VISTA + 3
        db "Al &100 % (tamaño real)", 0
        dw 0
        db 0
        dw O_MARGENES
        db "Ver los &márgenes", 0
        dw O_REJILLA
        db "Ver la &rejilla", 0
        dw O_AJUSTAR
        db "A&justar a la rejilla (5 mm): poner o quitar", 0
        dw 0
        db 0
        dw O_PAG_ANT
        db "Diapositiva &anterior    RePág", 0
        dw O_PAG_SIG
        db "Diapositiva &siguiente   AvPág", 0
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
        dw 0
        db 0
        dw O_FRENTE
        db "Traer al &frente       Ctrl+F", 0
        dw O_ALFONDO
        db "Enviar al fond&o       Ctrl+O", 0
        dw 0xFFFF
        db "&Presentación", 0
        dw O_PRES
        db "&Empezar desde el principio       F5", 0
        dw O_PRES_AQUI
        db "Empezar desde esta &diapositiva   Mayús+F5", 0
        dw 0
        db 0
        dw O_TRANS
        db "&Transición entre diapositivas...", 0
        dw O_PASOS
        db "&Puntos de las listas de uno en uno: poner o quitar", 0
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
ctrl_ord    db 0, O_ABRIR, 0, O_COPIAR, O_DER, 0, O_FRENTE, O_GUARDAR, 0, 0, 0
            db O_CUR, 0, 0, O_NEG, O_ALFONDO, O_IMPRIMIR, O_IZQ, O_DUPLICAR, O_SUB, O_CEN
            db O_NUEVO, O_PEGAR, O_GRANDE, O_CORTAR, O_REHACER, O_DESHACER
; las órdenes propias, y las del editor que aquí se hacen de otra manera
            align 4
ordenes_prog dd O_PAG_NUEVA, dia_nueva, O_IMAGEN, imagen_dia, O_DIA_DUP, dia_dup
            dd O_DIA_SUBE, dia_sube, O_DIA_BAJA, dia_baja, O_NOTAS, notas
            dd O_TEMA, tema_elige, O_PRES, pres_inicio, O_PRES_AQUI, pres_aqui
            dd O_TRANS, trans_elige, O_PASOS, pasos_orden
            dd O_FOLLETO2, folleto2, O_FOLLETO3, folleto3, O_FOLLETO6, folleto6
            dd O_IMP_NOTAS, imp_notas, 0

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
        jne .pres
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
        cmp eax, WM_DESTROY
        jne ed_proc
        call pre_suelta             ; la memoria pedida, de vuelta
        mov ebx, mini
        call lam_bloque_suelta
        jmp ed_proc
.pres:
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
        jz tecla
        cmp ecx, TC_ARRIBA
        je .sube
        cmp ecx, TC_IZQ
        je .sube
        cmp ecx, TC_ABAJO
        je .baja
        cmp ecx, TC_DER
        je .baja
        jmp tecla
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
        mov ecx, O_NOTAS
        jmp orden
.editor:
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
        call des_cambia             ; a medias: como estaba
        mov byte [des_hay], 0
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
        dec dword [sel]
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
        mov byte [esi+LO_LETRA], 9
        mov dword [esi+LO_AN], 600
        mov dword [esi+LO_AL], 400
        jmp .pon
.hay:
        test edx, edx               ; sin texto: la caja, fuera
        jnz .pon
        call lam_obj_quita
        cmp dword [sel], 0          ; (lo elegido puede haber bajado un puesto)
        jl .hecho
        cmp eax, [sel]
        jae .hecho
        dec dword [sel]
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
        mov ecx, ebx
        call lam_pinta
        call .reduce
        mov byte [lam_sucio], 1     ; (en lam_buf ya no está la que se ve)
.os:
        inc ebx
        jmp .o
.act:
        call al_dia
        mov ebx, [pag]
        cmp byte [mini_ok+ebx], 0
        jne .x
        call .reduce
.x:
        popad
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

; ---- las notas del orador, debajo de la diapositiva: las tres primeras
;      líneas (un clic abre el cuadro para escribirlas) ----
pinta_notas:
        pushad
        mov ebx, [cli_al]
        sub ebx, BR_PIE + NOTAS_AL
        cmp ebx, TB_AL
        jl .x
        mov [n_y], ebx
        COLOR C_BLANCO
        mov eax, TIRA_AN
        mov ecx, [cli_an]
        sub ecx, TIRA_AN
        jle .x
        mov edx, NOTAS_AL
        HV dc_rect
        COLOR C_ARENA_OSC
        mov edx, 1
        HV dc_rect
        COLOR C_PETROLEO
        NEGRITA 1
        mov eax, TIRA_AN + 6
        add ebx, 4
        mov esi, t_notas
        HV dc_texto
        NEGRITA 0
        sub ecx, 6 + 64 + 8         ; ECX=las letras que caben en una línea
        shr ecx, 3
        cmp ecx, 8
        jl .x
        cmp ecx, 110
        jbe .a
        mov ecx, 110
.a:
        mov [n_cabe], ecx
        call notas_de
        test eax, eax
        jns .hay
        COLOR C_ARENA_OSC
        mov eax, TIRA_AN + 70
        mov esi, t_sin_notas
        HV dc_texto
        jmp .x
.hay:
        COLOR C_TINTA
        mov ebp, [esi+LO_LARGO]     ; EBP=lo que queda
        mov esi, [esi+LO_DAT]
        add esi, [lam_dat]            ; ESI=por dónde va
        mov dword [n_lin], 0
.l:
        test ebp, ebp
        jz .x
        cmp dword [n_lin], 3
        jae .x
        mov edi, linea              ; una línea: hasta el salto, o lo que quepa
        xor ecx, ecx                ; (partida por el último espacio)
        mov edx, -1
.c:
        cmp ecx, ebp
        jae .fl
        mov al, [esi+ecx]
        cmp al, 10
        je .fl
        cmp ecx, [n_cabe]
        jae .parte
        cmp al, ' '
        ja .c1
        mov al, ' '
        mov edx, ecx
.c1:
        mov [edi+ecx], al
        inc ecx
        jmp .c
.parte:
        cmp edx, -1
        je .fl
        mov ecx, edx
.fl:
        mov byte [edi+ecx], 0
        add esi, ecx
        sub ebp, ecx
        jz .pl
        cmp byte [esi], ' '         ; (el espacio o el salto por el que se parte)
        ja .pl
        inc esi
        dec ebp
.pl:
        push esi
        mov eax, [n_lin]
        shl eax, 4
        add eax, [n_y]
        lea ebx, [eax+4]
        mov eax, TIRA_AN + 70
        mov esi, linea
        HV dc_texto
        pop esi
        inc dword [n_lin]
        jmp .l
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
n_y         resd 1
n_cabe      resd 1
n_lin       resd 1
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
pres        resb 1                  ; se está presentando a pantalla completa
pres_fin    resb 1                  ; se ha pasado de la última
mini_ok     resb LAM_MAX_PAG        ; qué miniaturas están al día
            alignb 4
ficha_tmp   resb LO_TAM
notas_buf   resb NOTAS_MAX + 8
; lo grande son bloques que se piden al usarlos (lam_bloque) y se devuelven
; al acabar de presentar o de imprimir (pre_suelta) y al cerrar
mini        resd 3                  ; las miniaturas (MINI_AN x MINI_AL cada una)
pres_buf    resd 3                  ; la diapositiva, al tamaño de la pantalla
hoja_buf    resd 3                  ; la hoja de un folleto (y, al presentar, lo
                                    ; que se va poniendo en una transición)
HVP_FIN
