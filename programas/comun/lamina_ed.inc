; =============================================================================
;  lamina_ed.inc - el editor de láminas: lo que tienen igual Publicaciones y
;  Presentaciones (y lo que venga) para manejar una lámina con objetos
;
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  El documento y su dibujo son de lamina.inc (el motor). Aquí está todo lo
;  que hace falta para trabajar con él en una ventana, igual en todos los
;  programas: la vista (a qué tamaño se ve la lámina y dónde cae), elegir,
;  mover y cambiar de tamaño con el ratón (tiradores), poner objetos con las
;  herramientas, escribir en las cajas, la fila de botones del formato (tipo y
;  tamaño de letra, negrita, cursiva, subrayado, alineación, viñetas, colores,
;  grosor, orden), las listas de elegir, deshacer y rehacer, duplicar, el
;  portapapeles, las láminas (añadir, quitar, pasar), abrir y guardar,
;  exportar a BMP, imprimir en una matricial y poner plantillas. El usuario
;  tiene que sentir que son el mismo programa con dos oficios.
;
;  CÓMO SE USA. El programa incluye, por este orden, tipos.inc, lamina.inc,
;  lamina_ed.inc, barra.inc y hvplib.inc, y pone en su «section .bss»
;  TIPOS_BSS, LAMINA_BSS, LAMINA_ED_BSS y BARRA_BSS. Su ventana puede usar
;  directamente ed_proc como procedimiento, o uno suyo que atienda lo que le
;  interese y salte a ed_proc con lo demás. Al empezar llama a ed_inicia
;  (ESI=ruta del módulo, EDI=parámetros) antes de crear la ventana.
;
;  LO QUE PONE EL PROGRAMA (etiquetas con estos nombres):
;    menu_prog      su menú                 botones1  su fila de botones de
;    ctrl_ord       sus órdenes de Ctrl+letra (27 octetos)      arriba
;    ordenes_prog   sus órdenes: dd número, rutina... y un 0 (se miran antes
;                   que las de aquí: puede cambiar cualquiera)
;    medidas        con [cli_an] y [cli_al], pone dónde va la vista de la
;                   lámina: [vx0], [vy0], [vw], [vh] (las barras de
;                   desplazamiento van pegadas a ella, abajo y a la derecha)
;    pintar         WM_PAINT: HV dc_pintar, pinta_botones, pinta_estado, lo
;                   suyo y, lo último, pinta_vista (estrecha el contexto) y
;                   HV dc_cerrar; acaba en «jmp hecho»
;    barra_propia   como barra_estado (barra.inc) para sus botones: EDI=botón
;                   -> EBP=1 si está puesto (puede no hacer nada: ret)
;    pie_papel      escribe en EDI (que avanza) lo que dice el pie cuando no
;                   hay nada elegido (el papel, el tema...)
;    nuevo          la orden Nuevo (cada programa, con sus plantillas)
;    firma          los 8 octetos con los que empiezan sus archivos
;    y los textos t_programa (su nombre), t_ext (".PUB"), t_pagina ("Página "),
;    t_arrastre, t_cambios, t_no_valido, t_no_guardar, t_exportar,
;    t_exportado, t_lleno, t_sin_sitio, t_max_pag, t_unica, t_q_quitar,
;    t_bmp_grande, t_impreso, t_acerca_tit y t_acerca.
;
;  LO QUE HAY AQUÍ para el programa (además de las órdenes O_*, que llegan
;  solas por el menú, la barra y el teclado):
;    ed_proc        el procedimiento de la ventana
;    orden, tecla, letra, raton, raton_mueve, raton_suelta, raton_doble
;                   lo que hace ed_proc con cada mensaje (para el programa
;                   que tenga su procedimiento y quiera pasarles algunos)
;    hecho          el final de todo mensaje atendido (xor eax, eax / ret)
;    inval          la ventana, a repintar          aviso  ESI=texto: lo dice
;    cambio         el documento ha cambiado (hay cambios; se redibuja)
;    sucio          hay que dibujar la lámina otra vez (sin contar un cambio)
;    instantanea    antes de cambiar el documento: lo guarda para deshacer
;    calc_vista     después de cambiar [vista], la zona o el tamaño de la lámina
;    doc_estrena    después de poner otro documento
;    pag_ir         EAX=lámina: pasa a ella         fin_edicion  se deja de escribir
;    elige          EAX=objeto (-1: ninguno): queda elegido
;    al_dia         deja en lam_buf la lámina [pag] (si había cambiado)
;    pinta_botones, pinta_estado, pinta_vista       para «pintar»
;    plantilla_pon  ESI=plantilla (macros PL y PLX) CL=lámina: pone sus objetos
;    lista_elige    elegir de una lista ([le_titulo], [le_n], [le_texto], [le_sel])
;    imp_abre, imp_prepara, imp_pagina, imp_cierra  imprimir un búfer cualquiera
;    [ed_todo]      lo pone a 1 lo que cambia más de una lámina (deshacer,
;                   añadir o quitar láminas, otro documento): para el programa
;                   que guarde algo de cada una (miniaturas)
; =============================================================================
TB_AL       equ 58                  ; la barra de botones: dos filas
PAD         equ 8                   ; del borde de la vista a la lámina
ESC_TAM     equ 520 * 1024          ; puntos de la lámina reducida (un A4 al 75 %)
TXT_MAX     equ 4000                ; letras de una caja de texto
PASO        equ 10                  ; lo que se aparta una copia de su original
ID_PUENTE   equ 900
ID_BV       equ 901
ID_BH       equ 902
; lo que se está haciendo con el ratón apretado
D_MOVER     equ 1
D_TAM       equ 2
D_CREAR     equ 3

; ---- las órdenes comunes (las propias de cada programa, de 200 en adelante) ----
O_NUEVO     equ 101
O_ABRIR     equ 102
O_GUARDAR   equ 103
O_COMO      equ 104
O_IMPRIMIR  equ 105
O_BORRADOR  equ 106
O_SALIR     equ 107
O_EXPORTAR  equ 108
O_CORTAR    equ 111
O_COPIAR    equ 112
O_PEGAR     equ 113
O_BORRAR    equ 114
O_DUPLICAR  equ 115
O_ESCRIBIR  equ 116
O_DESHACER  equ 117
O_REHACER   equ 118
O_FRENTE    equ 119
O_ALFONDO   equ 120
O_NEG       equ 121
O_CUR       equ 122
O_SUB       equ 123
O_GRANDE    equ 124
O_TIPO      equ 125
O_TAM       equ 126
O_C_LETRA   equ 127
O_C_FONDO   equ 128
O_C_BORDE   equ 129
O_GROSOR    equ 130
O_VINETAS   equ 131
O_IZQ       equ 141
O_CEN       equ 142
O_DER       equ 143
O_ACERCA    equ 151
O_HERR      equ 160                 ; 160-164: elegir, texto, rectángulo, elipse, línea
O_IMAGEN    equ 165
O_VISTA     equ 170                 ; 170-173: lámina entera, 50 %, 75 %, 100 %
O_REJILLA   equ 174
O_AJUSTAR   equ 175
O_MARGENES  equ 176
O_CAMBIA_V  equ 177                 ; el botón: de lámina entera a 100 % y vuelta
O_PAG_ANT   equ 180
O_PAG_SIG   equ 181
O_PAG_NUEVA equ 182
O_PAG_QUITA equ 183

; las listas de los diálogos: la de Hispa Ventanas (c_lista), salvo el
; tabulador (ver lista2_proc). El fondo y las banderas se copian al empezar
c_lista2    dd lista2_proc
            db 0, 0, 0, 0
; las ventanas hijas de todo editor: el cuadro de texto escondido por el que
; pasa el portapapeles y las dos barras de desplazamiento de la vista
ed_hijos:
        PLANTILLA HV_c_edit, 0, E_HIJA | E_OCULTA | ES_MULTI, 0, 0, 40, 40, ID_PUENTE
        PLANTILLA HV_c_barra, 0, E_HIJA, 0, 0, BD_AN, 50, ID_BV
        PLANTILLA HV_c_barra, 0, E_HIJA | BDS_HORIZ, 0, 0, 50, BD_AN, ID_BH
        dd 0
; ---- diálogo: elegir de una lista (larga y corta) ----
dlg_lista:
        PLANTILLA HV_c_dialogo, t_nada, E_DIALOGO, 0, 0, 240, 340, 0
        PLANTILLA c_lista2, t_nada, E_HIJA | E_TABULA, 16, 12, 204, 244, 60
        PLANTILLA HV_c_boton, t_aceptar, E_HIJA | E_TABULA | BS_DEFECTO, 20, 268, 90, 26, ID_ACEPTAR
        PLANTILLA HV_c_boton, t_cancelar, E_HIJA | E_TABULA, 120, 268, 90, 26, ID_CANCELAR
        dd 0
dlg_listac:
        PLANTILLA HV_c_dialogo, t_nada, E_DIALOGO, 0, 0, 240, 212, 0
        PLANTILLA c_lista2, t_nada, E_HIJA | E_TABULA, 16, 12, 204, 116, 60
        PLANTILLA HV_c_boton, t_aceptar, E_HIJA | E_TABULA | BS_DEFECTO, 20, 140, 90, 26, ID_ACEPTAR
        PLANTILLA HV_c_boton, t_cancelar, E_HIJA | E_TABULA, 120, 140, 90, 26, ID_CANCELAR
        dd 0

; ---- la fila de botones de abajo, la del formato: igual en todos ----
            align 4
botones     dd 6, 96, t_nada, O_TIPO, n_tipo, BT_TEXTO
            dd 106, 36, t_nada, O_TAM, n_tam, BT_TEXTO
            dd 146, 24, t_b_n, O_NEG, n_neg, BT_NEGRITA
            dd 172, 24, t_b_k, O_CUR, n_cur, BT_TEXTO
            dd 198, 24, t_b_s, O_SUB, n_sub, BT_SUBRAYA
            dd 230, 24, ic_izq, O_IZQ, n_izq, BT_ICONO
            dd 256, 24, ic_cen, O_CEN, n_cen, BT_ICONO
            dd 282, 24, ic_der, O_DER, n_der, BT_ICONO
            dd BX_LETRA, 22, C_TINTA, O_C_LETRA, n_c_letra, BT_COLOR
            dd BX_FONDO, 22, C_TINTA, O_C_FONDO, n_c_fondo, BT_COLOR
            dd BX_BORDE, 22, C_TINTA, O_C_BORDE, n_c_borde, BT_COLOR
            dd 554, 28, t_nada, O_GROSOR, n_grosor, BT_TEXTO
            dd 590, 24, ic_frente, O_FRENTE, n_frente, BT_ICONO
            dd 616, 24, ic_alfondo, O_ALFONDO, n_alfondo, BT_ICONO
            dd 648, 24, ic_vinetas, O_VINETAS, n_vinetas, BT_ICONO
            dd 0, 0, 0, 0, 0, 0
BX_LETRA    equ 364                 ; las muestras de color, con su rótulo delante
BX_FONDO    equ 444
BX_BORDE    equ 524
; los nombres de los botones que son iguales en todos (salen en el pie)
n_imprimir  db "Imprimir (Ctrl+P)", 0
n_cortar    db "Cortar el objeto elegido (Ctrl+X)", 0
n_copiar    db "Copiar el objeto elegido (Ctrl+C)", 0
n_pegar     db "Pegar (Ctrl+V)", 0
n_deshacer  db "Deshacer (Ctrl+Z)", 0
n_rehacer   db "Rehacer lo deshecho (Ctrl+Y)", 0
n_elegir    db "Elegir: un clic elige un objeto; arrastrando se mueve", 0
n_imagen    db "Insertar una imagen de un archivo BMP", 0
n_vista     db "Vista: cambia entre verla entera y el tamaño real", 0
n_tipo      db "Tipo de letra: pulse para elegirlo de la lista", 0
n_tam       db "Tamaño de la letra, en puntos: pulse para elegirlo", 0
n_neg       db "Negrita (Ctrl+N)", 0
n_cur       db "Cursiva (Ctrl+K)", 0
n_sub       db "Subrayado (Ctrl+S)", 0
n_izq       db "Alinear a la izquierda (Ctrl+Q)", 0
n_cen       db "Centrar (Ctrl+T)", 0
n_der       db "Alinear a la derecha (Ctrl+D)", 0
n_c_letra   db "Color de la letra", 0
n_c_fondo   db "Color del relleno (o sin relleno)", 0
n_c_borde   db "Color del borde o de la línea (o sin borde)", 0
n_grosor    db "Grosor del borde o de la línea", 0
n_frente    db "Traer al frente (Ctrl+F)", 0
n_alfondo   db "Enviar al fondo (Ctrl+O)", 0
n_vinetas   db "Viñetas: cada párrafo de la caja, con su punto (Tab cambia de nivel)", 0

; ---- los iconos propios del editor (los comunes están en barra.inc, y con
;      la misma forma): las herramientas, pasar de lámina y el orden ----
ic_elegir:
        db "................"
        db "...0............"
        db "...00..........."
        db "...090.........."
        db "...0990........."
        db "...09990........"
        db "...099990......."
        db "...0999990......"
        db "...09999990....."
        db "...099999990...."
        db "...09999000....."
        db "...0900990......"
        db "...00.0990......"
        db "...0...0990....."
        db "........000....."
        db "................"
ic_caja:
        db "................"
        db ".00..........00."
        db ".00777777777700."
        db "..7..........7.."
        db "..7.00000000.7.."
        db "..7.00.00.00.7.."
        db "..7....00....7.."
        db "..7....00....7.."
        db "..7....00....7.."
        db "..7....00....7.."
        db "..7....00....7.."
        db "..7...0000...7.."
        db "..7..........7.."
        db ".00777777777700."
        db ".00..........00."
        db "................"
ic_rect:
        db "................"
        db "................"
        db "................"
        db ".00000000000000."
        db ".0cccccccccccc0."
        db ".0cccccccccccc0."
        db ".0cccccccccccc0."
        db ".0cccccccccccc0."
        db ".0cccccccccccc0."
        db ".0cccccccccccc0."
        db ".0cccccccccccc0."
        db ".0cccccccccccc0."
        db ".00000000000000."
        db "................"
        db "................"
        db "................"
ic_elipse:
        db "................"
        db "................"
        db "................"
        db ".....000000....."
        db "...00aaaaaa00..."
        db "..0aaaaaaaaaa0.."
        db ".0aaaaaaaaaaaa0."
        db ".0aaaaaaaaaaaa0."
        db ".0aaaaaaaaaaaa0."
        db ".0aaaaaaaaaaaa0."
        db "..0aaaaaaaaaa0.."
        db "...00aaaaaa00..."
        db ".....000000....."
        db "................"
        db "................"
        db "................"
ic_linea:
        db "................"
        db "................"
        db ".............00."
        db "............00.."
        db "...........00..."
        db "..........00...."
        db ".........00....."
        db "........00......"
        db ".......00......."
        db "......00........"
        db ".....00........."
        db "....00.........."
        db "...00..........."
        db "..00............"
        db "................"
        db "................"
ic_imagen:
        db "................"
        db "................"
        db ".00000000000000."
        db ".0eeeeeeeeeeee0."
        db ".0eeeeeeeeaaee0."
        db ".0eeeeeeeeaaee0."
        db ".0eeeeeeeeeeee0."
        db ".0eee0eeeeeeee0."
        db ".0ee0b0eee0eee0."
        db ".0e0bbb0e0b0ee0."
        db ".00bbbbb0bbb0e0."
        db ".0bbbbbbbbbbb00."
        db ".0bbbbbbbbbbbb0."
        db ".00000000000000."
        db "................"
        db "................"
ic_pag_ant:
        db "................"
        db "................"
        db "................"
        db ".........c......"
        db "........cc......"
        db ".......ccc......"
        db "......cccc......"
        db ".....ccccc......"
        db ".....ccccc......"
        db "......cccc......"
        db ".......ccc......"
        db "........cc......"
        db ".........c......"
        db "................"
        db "................"
        db "................"
ic_pag_sig:
        db "................"
        db "................"
        db "................"
        db "......c........."
        db "......cc........"
        db "......ccc......."
        db "......cccc......"
        db "......ccccc....."
        db "......ccccc....."
        db "......cccc......"
        db "......ccc......."
        db "......cc........"
        db "......c........."
        db "................"
        db "................"
        db "................"
ic_frente:
        db "................"
        db "................"
        db ".000000000......"
        db ".0aaaaaaa0......"
        db ".0aaaaaaa0......"
        db ".0aaaaaaa0000..."
        db ".0aaaaaaa0770..."
        db ".0aaaaaaa0770..."
        db ".0aaaaaaa0770..."
        db ".00000000077000."
        db "....0777777770.."
        db "....0777777770.."
        db "....0777777770.."
        db "....0000000000.."
        db "................"
        db "................"
ic_alfondo:
        db "................"
        db "................"
        db ".000000000......"
        db ".077777770......"
        db ".077777770......"
        db ".0777000000000.."
        db ".07770aaaaaaa0.."
        db ".07770aaaaaaa0.."
        db ".07770aaaaaaa0.."
        db ".00000aaaaaaa0.."
        db ".....0aaaaaaa0.."
        db ".....0aaaaaaa0.."
        db ".....0aaaaaaa0.."
        db ".....000000000.."
        db "................"
        db "................"
; las vistas: 256 es el 100 % (0: la que quepa entera)
zooms       dd 0, 128, 192, 256
; los tiradores, en el orden de las agujas del reloj desde arriba a la
; izquierda: dónde están (0, 1, 2: principio, medio y final de cada lado) y
; qué lados mueven (1 izquierdo, 2 derecho, 4 de arriba, 8 de abajo)
tir_fx      db 0, 1, 2, 2, 2, 1, 0, 0
tir_fy      db 0, 0, 0, 1, 2, 2, 2, 1
tir_lados   db 5, 4, 6, 2, 10, 8, 9, 1
; y la forma del cursor sobre cada uno (hacia dónde tira)
tir_cursor  db CUR_DIAG1, CUR_VERT, CUR_DIAG2, CUR_HORIZ, CUR_DIAG1, CUR_VERT, CUR_DIAG2, CUR_HORIZ
; los grosores que se ofrecen
grosores    db 1, 2, 3, 4, 6, 8
N_GROSORES  equ 6
; el tramado de la impresora (4x4): un punto se imprime si es más oscuro
tramado     db 8, 136, 40, 168, 200, 72, 232, 104, 56, 184, 24, 152, 248, 120, 216, 88
; las órdenes sencillas: número y rutina
            align 4
ordenes     dd O_NUEVO, nuevo, O_ABRIR, abrir, O_GUARDAR, guardar, O_COMO, guardar_como
            dd O_EXPORTAR, exportar, O_IMPRIMIR, imprimir, O_BORRADOR, imprimir_b
            dd O_CORTAR, cortar, O_COPIAR, copiar, O_PEGAR, pegar, O_BORRAR, borrar
            dd O_DUPLICAR, duplicar, O_ESCRIBIR, escribir, O_DESHACER, deshacer
            dd O_REHACER, rehacer, O_FRENTE, al_frente, O_ALFONDO, al_fondo
            dd O_GRANDE, grande, O_TIPO, tipo_elige, O_TAM, tam_elige
            dd O_C_LETRA, color_letra, O_C_FONDO, color_fondo, O_C_BORDE, color_borde
            dd O_GROSOR, grosor_elige, O_IMAGEN, imagen, O_REJILLA, rejilla
            dd O_AJUSTAR, ajustar_orden, O_MARGENES, margenes, O_CAMBIA_V, cambia_vista
            dd O_PAG_ANT, pag_ant, O_PAG_SIG, pag_sig, O_PAG_NUEVA, pag_nueva
            dd O_PAG_QUITA, pag_quita, O_ACERCA, acerca, O_VINETAS, vinetas
            dd O_SALIR, salir, 0

; ---- textos ----
t_guion     db " - ", 0
t_sin_nombre db "(sin nombre)", 0
t_nada      db 0
t_b_n       db "N", 0
t_b_k       db "K", 0
t_b_s       db "S", 0
t_r_letra   db "Letra", 0
t_r_fondo   db "Fondo", 0
t_r_borde   db "Borde", 0
t_entera    db "Entera", 0
t_pct       db " %", 0
t_de        db " de ", 0
t_sep       db "    ", 0
t_x         db ": x ", 0
t_y         db ", y ", 0
t_mm_pc     db " mm; ", 0
t_por       db " x ", 0
t_mm        db " mm", 0
t_coma      db ", ", 0
t_vista     db "Vista: ", 0
t_o_caja    db "Caja de texto", 0
t_o_rect    db "Rectángulo", 0
t_o_elipse  db "Elipse", 0
t_o_linea   db "Línea", 0
t_o_imagen  db "Imagen", 0
            align 4
t_objetos   dd t_nada, t_o_caja, t_o_rect, t_o_elipse, t_o_linea, t_o_imagen
t_escriba   db ": escriba; Esc o un clic fuera para acabar", 0
t_ajustado  db "    Ajuste a la rejilla", 0
t_no_cabe   db "; el texto no cabe", 0
t_sin_mem   db "No hay memoria bastante para hacer eso.", 10
            db "Cierre otros programas y pruebe otra vez.", 0
t_sin_mem_v db "No hay memoria bastante para dibujar esto. Cierre otros programas.", 0
t_ext_bmp   db ".BMP", 0
t_abrir     db "Abrir", 0
t_guardar_como db "Guardar como", 0
t_ins_img   db "Insertar una imagen (BMP)", 0
t_no_abrir  db "No se ha podido abrir el archivo.", 0
t_no_export db "No se ha podido escribir la imagen.", 10
            db "Compruebe que el disco no está lleno ni protegido.", 0
t_caja_llena db "No cabe más texto en esta caja (4.000 letras).", 0
t_no_bmp    db "Ese archivo no es una imagen BMP de las que se admiten", 10
            db "(sin comprimir, de 2, 16 o 256 colores o de color verdadero).", 0
t_e_imprimir db "No se ha podido imprimir.", 10
            db "Compruebe que la impresora (la del puerto paralelo, LPT1)", 10
            db "está encendida, en línea y con papel.", 0
t_aceptar   db "Aceptar", 0
t_cancelar  db "Cancelar", 0
t_d_tipo    db "Tipo de letra", 0
t_d_tam     db "Tamaño (puntos)", 0
t_d_c_letra db "Color de la letra", 0
t_d_c_fondo db "Color del relleno", 0
t_d_c_borde db "Color del borde", 0
t_d_grosor  db "Grosor", 0
t_ninguno   db "Ninguno", 0
t_c0        db "Tinta (negro)", 0
t_c1        db "Marrón", 0
t_c2        db "Azul petróleo", 0
t_c3        db "Petróleo claro", 0
t_c4        db "Terracota", 0
t_c5        db "Terracota claro", 0
t_c6        db "Arena", 0
t_c7        db "Arena oscura", 0
t_c8        db "Crema", 0
t_c9        db "Blanco", 0
t_c10       db "Ocre", 0
t_c11       db "Verde oliva", 0
t_c12       db "Añil", 0
t_c13       db "Granate", 0
t_c14       db "Turquesa", 0
t_c15       db "Piedra", 0
            align 4
t_colores   dd t_c0, t_c1, t_c2, t_c3, t_c4, t_c5, t_c6, t_c7
            dd t_c8, t_c9, t_c10, t_c11, t_c12, t_c13, t_c14, t_c15

; ---- las plantillas: una tabla de objetos que plantilla_pon pone en una
;      lámina. De cada uno (macro PL): tipo; x, y, ancho y alto en milésimas
;      de la lámina (así valen para cualquier tamaño); estilo de la letra,
;      alineación, color de la letra, del relleno y del borde, grosor, tipo
;      de letra, tamaño de la letra en una lámina grande y en una pequeña (A5
;      o menos); y su texto (0: ninguno). PLX lleva además el papel del
;      objeto en la lámina (LO_PAPEL) y sus banderas (LO_BAND). La tabla
;      acaba en un 0 ----
%macro PLX 17
        db %1, %6, %7, %8, %9, %10, %11, %12, %13, %14, %16, %17
        dw %2, %3, %4, %5
        dd %15
%endmacro
%macro PL 15
        PLX %1, %2, %3, %4, %5, %6, %7, %8, %9, %10, %11, %12, %13, %14, %15, 0, 0
%endmacro
PL_TAM      equ 24
N           equ LAM_NADA

; ESI=parámetros: el primer nombre, a nom_tmp; CF=1 si no hay
nombre_param:
        lodsb
        cmp al, ' '
        je nombre_param
        jb .no
        cmp al, '/'
        je .no
        dec esi
        mov edi, nom_tmp
        mov ecx, 79
.c:
        lodsb
        cmp al, ' '
        jbe .f
        stosb
        dec ecx
        jnz .c
.f:
        mov byte [edi], 0
        clc
        ret
.no:
        stc
        ret

; el título: «Publicaciones - NOMBRE»
pon_titulo:
        pushad
        mov edi, titulo
        mov esi, t_programa
        call hvl_cad
        mov esi, t_guion
        call hvl_cad
        mov esi, nombre
        cmp byte [esi], 0
        jne .n
        mov esi, t_sin_nombre
.n:
        call hvl_cad
        mov byte [edi], 0
        mov ebp, [ven]
        test ebp, ebp
        jz .x
        mov esi, titulo
        HV ven_texto
.x:
        popad
        ret

; (interna) después de poner otro documento: nada elegido, la primera página,
; nada que deshacer y sin cambios
doc_estrena:
        mov dword [sel], -1
        mov byte [editando], 0
        mov byte [drag], 0
        mov dword [herr], 0
        mov dword [pag], 0
        mov dword [sx], 0
        mov dword [sy], 0
        mov byte [des_hay], 0
        mov byte [des_hecho], 0
        mov byte [tecleando], 0
        mov byte [moviendo], 0
        mov byte [modif], 0
        mov byte [lam_sucio], 1
        mov byte [ed_todo], 1
        call calc_vista
        call pon_titulo
        jmp inval

; =============================================================================
;  El procedimiento de la ventana
; =============================================================================
ed_proc:
        cmp eax, WM_PAINT
        je pintar
        cmp eax, WM_COMMAND
        je orden
        cmp eax, WM_KEYDOWN
        je tecla
        cmp eax, WM_CHAR
        je letra
        cmp eax, WM_LBUTTONDOWN
        je raton
        cmp eax, WM_MOUSEMOVE
        je raton_mueve
        cmp eax, WM_LBUTTONUP
        je raton_suelta
        cmp eax, WM_LBUTTONDBLCLK
        je raton_doble
        cmp eax, WM_MOUSEWHEEL
        je .rueda
        cmp eax, WM_VSCROLL
        je .bv
        cmp eax, WM_HSCROLL
        je .bh
        cmp eax, WM_SIZE
        je .tam
        cmp eax, WM_SETFOCUS
        je .foco
        cmp eax, WM_KILLFOCUS
        je .sin_foco
        cmp eax, WM_HV_ABRIR
        je .hvabrir
        cmp eax, WM_GETMINMAXINFO
        jne .nmin
        mov eax, 660 + 300 * 65536
        ret
.nmin:
        cmp eax, WM_CREATE
        je .crear
        cmp eax, WM_CLOSE
        je .cerrar
        cmp eax, WM_DESTROY
        je .destruir
        HVJ ven_defecto
.crear:
        mov [ven], ebp
        mov dword [ebp+V_MENU], menu_prog
        mov dword [ebp+V_ICONO], hvp_icono
        mov esi, ed_hijos
        HV ven_lista
        mov ecx, ID_PUENTE
        HV ven_hija_id
        mov [puente], eax
        mov ecx, ID_BV
        HV ven_hija_id
        mov [barra_v], eax
        mov ecx, ID_BH
        HV ven_hija_id
        mov [barra_h], eax
        mov ebp, [barra_v]
        mov eax, BDM_LINEA
        mov ecx, 16
        HV msg_enviar
        mov ebp, [barra_h]
        mov eax, BDM_LINEA
        mov ecx, 16
        HV msg_enviar
.tam:
        mov ebp, [ven]
        HV ven_cli                  ; ECX=ancho EDX=alto de la zona
        mov [cli_an], ecx
        mov [cli_al], edx
        call medidas                ; (del programa) dónde va la vista
        call ed_barras
        call calc_vista
        call inval
        jmp hecho
.foco:
        mov byte [foco], 1
        call inval
        jmp hecho
.sin_foco:
        mov byte [foco], 0
        cmp byte [drag], 0          ; (si se estaba arrastrando, se deja)
        je .sf1
        mov byte [drag], 0
        HV ven_soltar
.sf1:
        call inval
        jmp hecho
.rueda:
        mov eax, ecx
        shl eax, 5
        add eax, [sy]
        mov [sy], eax
        call calc_vista
        call inval
        jmp hecho
.bv:
        mov [sy], ecx
        call calc_vista
        call inval
        jmp hecho
.bh:
        mov [sx], ecx
        call calc_vista
        call inval
        jmp hecho
.cerrar:
        call preguntar
        jc hecho
        mov ebp, [ven]
        HV ven_destruir
        jmp hecho
.destruir:
        call ed_suelta              ; la memoria pedida, de vuelta
        mov dword [ven], 0
        HV msg_salir
        jmp hecho
.hvabrir:
        push edx
        call preguntar
        pop esi
        jc hecho
        call nombre_param
        jc hecho
        call cargar
hecho:
        xor eax, eax
        ret

; devuelve la memoria que se ha ido pidiendo (al cerrar la ventana)
ed_suelta:
        push ebx
        mov ebx, des_dat
        call lam_bloque_suelta
        mov ebx, clip_dat
        call lam_bloque_suelta
        mov ebx, esc_buf
        call lam_bloque_suelta
        call lam_suelta
        pop ebx
        ret

; (interna) las barras de desplazamiento, pegadas a la vista
ed_barras:
        pushad
        mov ebp, [barra_v]
        test ebp, ebp
        jz .x
        mov eax, [vx0]
        add eax, [vw]
        mov ebx, [vy0]
        mov ecx, BD_AN
        mov edx, [vh]
        HV ven_mover
        mov eax, [vx0]
        mov ebx, [vy0]
        add ebx, [vh]
        mov ecx, [vw]
        mov edx, BD_AN
        mov ebp, [barra_h]
        HV ven_mover
.x:
        popad
        ret
; (interna) EAX=x EBX=y de la ventana: CF=1 si no está en la vista
en_vista:
        push ecx
        mov ecx, eax
        sub ecx, [vx0]
        cmp ecx, [vw]
        jae .no
        mov ecx, ebx
        sub ecx, [vy0]
        cmp ecx, [vh]
        jae .no
        pop ecx
        clc
        ret
.no:
        pop ecx
        stc
        ret

; lo primero que hace el programa: ESI=ruta del módulo EDI=parámetros
ed_inicia:
        pushad
        call tl_cargar              ; los tipos de letra
        mov [param], edi
        mov esi, ed_hijos
        call hvl_clases
        mov esi, dlg_lista
        call hvl_clases
        mov esi, dlg_listac
        call hvl_clases
        mov eax, [hv_api + HV_c_lista * 4]  ; la lista propia: como la de siempre
        mov ebx, [eax+C_PROC]
        mov [lista_orig], ebx
        mov ebx, [eax+C_FONDO]
        mov [c_lista2+C_FONDO], ebx
        mov eax, AJ_VID_BITS        ; con menos de 256 colores no hay cubo con
        HV aj_leer                  ; el que suavizar la lámina reducida
        cmp eax, 8
        setae al
        movzx eax, al
        mov [suave], eax
        mov byte [margen_ver], 1
        mov byte [act_borde+1], C_TINTA     ; las formas: con borde negro
        mov byte [act_fondo], LAM_NADA      ; el texto: sin fondo ni borde
        mov byte [act_fondo+1], LAM_NADA
        mov byte [act_borde], LAM_NADA
        mov word [act_grosor], 0x0101
        popad
        ret

; la ventana, a repintar
inval:
        pushad
        mov ebp, [ven]
        test ebp, ebp
        jz .x
        HV ven_invalidar
.x:
        popad
        ret
; después de cambiar el documento: hay cambios, y la página se dibuja otra vez
cambio:
        mov byte [modif], 1
sucio:
        mov byte [lam_sucio], 1
        jmp inval

; mensaje de aviso: ESI=texto
aviso:
        pushad
        mov ebp, [ven]
        mov edi, t_programa
        mov eax, DM_ACEPTAR
        HV dlg_mensaje
        popad
        ret

; copia ESI (acabada en 0) a EDI, con su 0
copia:
        push eax
        push esi
        push edi
.c:
        lodsb
        stosb
        test al, al
        jnz .c
        pop edi
        pop esi
        pop eax
        ret

; =============================================================================
;  La vista: a qué tamaño se ve la página y dónde cae en la ventana
; =============================================================================
calc_vista:
        pushad
        mov eax, [vista]
        mov eax, [zooms+eax*4]
        test eax, eax
        jnz .z
        ; la página entera: lo que quepa en la vista, sin pasar del 100 %
        mov eax, [vw]
        sub eax, 2 * PAD
        jg .e1
        mov eax, 1
.e1:
        shl eax, 8
        xor edx, edx
        div dword [lam_an]
        mov ecx, eax
        mov eax, [vh]
        sub eax, 2 * PAD
        jg .e2
        mov eax, 1
.e2:
        shl eax, 8
        xor edx, edx
        div dword [lam_al]
        cmp eax, ecx
        jbe .e3
        mov eax, ecx
.e3:
        cmp eax, 256
        jbe .e4
        mov eax, 256
.e4:
        cmp eax, 16
        jae .z
        mov eax, 16
.z:
        mov [zoom], eax
.mide:
        mov eax, [lam_an]           ; lo que mide la página en la ventana
        imul eax, [zoom]
        shr eax, 8
        mov [pw], eax
        mov ebx, [lam_al]
        imul ebx, [zoom]
        shr ebx, 8
        mov [ph], ebx
        cmp dword [zoom], 256
        je .cabe
        imul eax, ebx               ; reducida, tiene que caber en esc_buf
        cmp eax, ESC_TAM
        jbe .cabe
        sub dword [zoom], 8
        jmp .mide
.cabe:
        mov eax, [zoom]
        cmp eax, [zoom_ant]
        je .igual
        mov [zoom_ant], eax
        mov byte [esc_sucio], 1
.igual:
        ; a lo ancho: centrada si cabe; si no, se desplaza
        mov eax, [pw]
        add eax, 2 * PAD
        sub eax, [vw]
        jg .hx
        neg eax
        sar eax, 1
        add eax, PAD
        add eax, [vx0]
        mov [wx0], eax
        mov dword [sx_max], 0
        mov dword [sx], 0
        jmp .v
.hx:
        mov [sx_max], eax
        mov ecx, [sx]
        test ecx, ecx
        jns .hx1
        xor ecx, ecx
.hx1:
        cmp ecx, eax
        jle .hx2
        mov ecx, eax
.hx2:
        mov [sx], ecx
        mov eax, PAD
        add eax, [vx0]
        sub eax, ecx
        mov [wx0], eax
.v:
        mov eax, [ph]               ; y a lo alto
        add eax, 2 * PAD
        sub eax, [vh]
        jg .hy
        neg eax
        sar eax, 1
        add eax, PAD
        add eax, [vy0]
        mov [wy0], eax
        mov dword [sy_max], 0
        mov dword [sy], 0
        jmp .b
.hy:
        mov [sy_max], eax
        mov ecx, [sy]
        test ecx, ecx
        jns .hy1
        xor ecx, ecx
.hy1:
        cmp ecx, eax
        jle .hy2
        mov ecx, eax
.hy2:
        mov [sy], ecx
        mov eax, PAD
        add eax, [vy0]
        sub eax, ecx
        mov [wy0], eax
.b:
        mov ebp, [barra_v]          ; las barras de desplazamiento, al día
        test ebp, ebp
        jz .x
        mov eax, BDM_PONER
        mov ecx, [sy_max]
        mov edx, [vh]
        HV msg_enviar
        mov eax, BDM_POS
        mov ecx, [sy]
        HV msg_enviar
        mov ebp, [barra_h]
        mov eax, BDM_PONER
        mov ecx, [sx_max]
        mov edx, [vw]
        HV msg_enviar
        mov eax, BDM_POS
        mov ecx, [sx]
        HV msg_enviar
.x:
        popad
        ret

; EAX=x EBX=y de la ventana -> EAX, EBX de la página
a_pagina:
        push ecx
        push edx
        sub eax, [wx0]
        shl eax, 8
        cdq
        idiv dword [zoom]
        mov ecx, eax
        mov eax, ebx
        sub eax, [wy0]
        shl eax, 8
        cdq
        idiv dword [zoom]
        mov ebx, eax
        mov eax, ecx
        pop edx
        pop ecx
        ret
; EAX=x EBX=y de la página -> EAX, EBX de la ventana
a_ventana:
        imul eax, [zoom]
        sar eax, 8
        add eax, [wx0]
        imul ebx, [zoom]
        sar ebx, 8
        add ebx, [wy0]
        ret
; EAX=x EBX=y ECX=ancho EDX=alto de la página -> [sr_x0], [sr_y0], [sr_x1] y
; [sr_y1]: sus dos esquinas en la ventana
rect_v:
        pushad
        add ecx, eax
        add edx, ebx
        call a_ventana
        mov [sr_x0], eax
        mov [sr_y0], ebx
        mov eax, ecx
        mov ebx, edx
        call a_ventana
        mov [sr_x1], eax
        mov [sr_y1], ebx
        popad
        ret
; -> EDX=la holgura para acertar con el ratón: cuatro puntos de pantalla
holgura:
        push eax
        mov eax, 4 * 256
        xor edx, edx
        div dword [zoom]
        lea edx, [eax+1]
        pop eax
        ret

; EAX=una medida -> EAX: a la rejilla de 5 mm, si está puesto el ajuste (un
; milímetro son 480/127 puntos de lámina)
ajusta:
        cmp byte [ajustar], 0
        je .x
        test eax, eax
        js .x
        push ecx
        push edx
        imul eax, 127
        add eax, 1200
        xor edx, edx
        mov ecx, 2400
        div ecx
        imul eax, 2400
        add eax, 63
        xor edx, edx
        mov ecx, 127
        div ecx
        pop edx
        pop ecx
.x:
        ret

; EAX=puntos de lámina -> EAX=milímetros (redondeando)
a_mm:
        push ecx
        push edx
        mov ecx, 960
        test eax, eax
        js .neg
        imul eax, 254
        add eax, 480
        xor edx, edx
        div ecx
        jmp .x
.neg:
        neg eax
        imul eax, 254
        add eax, 480
        xor edx, edx
        div ecx
        neg eax
.x:
        pop edx
        pop ecx
        ret

; =============================================================================
;  Las órdenes: ECX=número
; =============================================================================
orden:
        call orden_haz
        jmp hecho
orden_haz:
        mov dword [br_sobre], 0
        mov byte [tecleando], 0     ; (cualquier orden acaba la tanda de teclas)
        mov byte [moviendo], 0
        mov esi, ordenes_prog       ; antes, las del programa
.p:
        mov eax, [esi]
        test eax, eax
        jz .com
        cmp eax, ecx
        je .es
        add esi, 8
        jmp .p
.com:
        mov esi, ordenes
.b:
        mov eax, [esi]
        test eax, eax
        jz .otras
        cmp eax, ecx
        je .es
        add esi, 8
        jmp .b
.es:
        jmp [esi+4]
.otras:
        mov eax, ecx
        sub eax, O_HERR
        cmp eax, 4
        jbe herramienta
        mov eax, ecx
        sub eax, O_VISTA
        cmp eax, 3
        jbe pon_vista
        mov eax, ecx
        sub eax, O_IZQ
        cmp eax, 2
        jbe alinea
        mov al, LE_NEG
        cmp ecx, O_NEG
        je estilo
        mov al, LE_CUR
        cmp ecx, O_CUR
        je estilo
        mov al, LE_SUB
        cmp ecx, O_SUB
        je estilo
        ret

salir:
        call preguntar
        jc .x
        mov ebp, [ven]
        HV ven_destruir
.x:
        ret
acerca:
        mov ebp, [ven]
        mov esi, t_acerca
        mov edi, t_acerca_tit
        mov eax, DM_ACEPTAR
        HV dlg_mensaje
        ret

; EAX=herramienta (0 elegir, 1 texto, 2 rectángulo, 3 elipse, 4 línea)
herramienta:
        call fin_edicion
        mov [herr], eax
        test eax, eax
        jz inval
        mov dword [sel], -1         ; con una herramienta no hay nada elegido
        jmp inval

; EAX=vista (0 página entera, 1 al 50 %, 2 al 75 %, 3 al 100 %)
pon_vista:
        mov [vista], eax
        mov dword [sx], 0
        mov dword [sy], 0
        call calc_vista
        jmp inval
cambia_vista:
        xor eax, eax
        cmp dword [vista], 0
        jne pon_vista
        mov eax, 3
        jmp pon_vista
rejilla:
        xor byte [rejilla_ver], 1
        jmp inval
margenes:
        xor byte [margen_ver], 1
        jmp inval
ajustar_orden:
        xor byte [ajustar], 1
        jmp inval

; ---- las páginas ----
pag_ant:
        mov eax, [pag]
        test eax, eax
        jz pag_ir.x
        dec eax
        jmp pag_ir
pag_sig:
        mov eax, [pag]
        inc eax
        cmp eax, [lam_pags]
        jae pag_ir.x
; EAX=página: pasa a ella
pag_ir:
        call fin_edicion
        mov [pag], eax
        mov dword [sel], -1
        mov dword [sx], 0
        mov dword [sy], 0
        call calc_vista
        call sucio
.x:
        ret
pag_nueva:
        cmp dword [lam_pags], LAM_MAX_PAG
        jb .cabe
        mov esi, t_max_pag
        jmp aviso
.cabe:
        call fin_edicion
        call instantanea
        mov ecx, [pag]
        call lam_pag_nueva
        mov byte [ed_todo], 1
        mov byte [modif], 1
        mov eax, [pag]
        inc eax
        jmp pag_ir
pag_quita:
        cmp dword [lam_pags], 1
        ja .hay
        mov esi, t_unica
        jmp aviso
.hay:
        mov ebp, [ven]
        mov esi, t_q_quitar
        mov edi, t_programa
        mov eax, DM_ACEPTAR_CANCELAR
        HV dlg_mensaje
        cmp eax, ID_ACEPTAR
        jne pag_ir.x
        call fin_edicion
        call instantanea
        mov ecx, [pag]
        call lam_pag_quita
        mov byte [ed_todo], 1
        mov byte [modif], 1
        mov eax, [pag]
        cmp eax, [lam_pags]
        jb pag_ir
        mov eax, [lam_pags]
        dec eax
        jmp pag_ir

; =============================================================================
;  Elegir, escribir y lo que se ve en la barra
; =============================================================================
; -> EAX=de qué clase es lo que se maneja: 0 una caja de texto, 1 una forma.
; Cada clase recuerda su relleno, su borde y su grosor
clase:
        push esi
        mov eax, [sel]
        test eax, eax
        js .herr
        call lam_ptr
        cmp byte [esi+LO_TIPO], LT_TEXTO
        jmp .x
.herr:
        cmp dword [herr], 1
.x:
        mov eax, 0
        je .y
        inc eax
.y:
        pop esi
        ret

; EAX=objeto (-1: ninguno): queda elegido, y la barra enseña su formato.
; Conserva EAX
elige:
        push esi
        push ebx
        push ecx
        mov [sel], eax
        mov byte [tecleando], 0
        mov byte [moviendo], 0
        test eax, eax
        js .x
        call lam_ptr
        movzx ebx, byte [esi+LO_TIPO]
        xor ecx, ecx
        cmp ebx, LT_TEXTO
        jne .forma
        mov bl, [esi+LO_LETRA]
        mov [act_letra], bl
        mov bl, [esi+LO_CUERPO]
        mov [act_cuerpo], bl
        mov bl, [esi+LO_ESTILO]
        mov [act_estilo], bl
        mov bl, [esi+LO_ALIN]
        mov [act_alin], bl
        mov bl, [esi+LO_TINTA]
        mov [act_tinta], bl
        jmp .caja
.forma:
        inc ecx
        cmp ebx, LT_LINEA
        je .borde
        cmp ebx, LT_IMAGEN
        je .borde
.caja:
        mov bl, [esi+LO_FONDO]
        mov [act_fondo+ecx], bl
.borde:
        mov bl, [esi+LO_BORDE]
        mov [act_borde+ecx], bl
        mov bl, [esi+LO_GROSOR]
        mov [act_grosor+ecx], bl
.x:
        pop ecx
        pop ebx
        pop esi
        ret

; se acaba de escribir en la caja. Una caja que se queda vacía y sin fondo ni
; borde no se ve: se quita (¡los objetos de detrás cambian de número!)
fin_edicion:
        cmp byte [editando], 0
        je .x
        pushad
        mov byte [editando], 0
        mov byte [tecleando], 0
        mov eax, [sel]
        test eax, eax
        js .f
        call lam_ptr
        cmp dword [esi+LO_LARGO], 0
        jne .f
        cmp byte [esi+LO_PAPEL], 0  ; (las que tienen un papel en la lámina, no)
        jne .f
        cmp byte [esi+LO_FONDO], LAM_NADA
        jne .f
        cmp byte [esi+LO_BORDE], LAM_NADA
        jne .f
        call lam_obj_quita
        mov dword [sel], -1
        mov byte [lam_sucio], 1
.f:
        call inval
        popad
.x:
        ret

; Edición > Escribir en la caja: el cursor, al final de su texto
escribir:
        mov eax, [sel]
        test eax, eax
        js .x
        call lam_ptr
        cmp byte [esi+LO_TIPO], LT_TEXTO
        jne .x
        test byte [esi+LO_BAND], LB_HUECO
        jnz .x
        mov eax, [esi+LO_LARGO]
        mov [cur], eax
        mov byte [editando], 1
        call inval
.x:
        ret

; (interna) la ficha de la caja en la que se escribe, compuesta -> ESI
ed_ficha:
        push eax
        mov eax, [sel]
        call lam_ptr
        call lam_compone
        pop eax
        ret

; escribe la letra AL (10: salto de línea) donde el cursor
ed_inserta:
        pushad
        mov [ed_car], al
        mov eax, [sel]
        call lam_ptr
        cmp dword [esi+LO_LARGO], TXT_MAX
        jae .llena
        call inst_tecleo
        mov eax, [sel]
        mov ebx, [cur]
        xor ecx, ecx
        mov esi, ed_car
        mov edx, 1
        call lam_dat_cambia
        jc .sin_sitio
        inc dword [cur]
        call cambio
        popad
        ret
.llena:
        mov esi, t_caja_llena
        call aviso
        popad
        ret
.sin_sitio:
        mov esi, t_sin_sitio
        call aviso
        popad
        ret
; borra la letra del sitio EAX de la caja
ed_borra:
        pushad
        mov ebx, eax
        call inst_tecleo
        mov eax, [sel]
        mov ecx, 1
        xor edx, edx
        call lam_dat_cambia
        mov [cur], ebx
        call cambio
        popad
        ret

; ---- teclado: ECX=tecla EDX=mayúsculas ----
tecla:
        cmp ecx, TC_CTRL
        jae .ctrl
        cmp byte [editando], 0
        jne .editando
        ; ---- con un objeto elegido (o ninguno) ----
        xor eax, eax
        xor ebx, ebx
        cmp ecx, TC_IZQ
        je .m_izq
        cmp ecx, TC_DER
        je .m_der
        cmp ecx, TC_ARRIBA
        je .m_arr
        cmp ecx, TC_ABAJO
        je .m_aba
        mov byte [moviendo], 0
        cmp ecx, TC_SUPR
        je .o_borrar
        cmp ecx, TC_INTRO
        je .o_escribir
        cmp ecx, TC_F1 + 1          ; F2: escribir, como Intro
        je .o_escribir
        cmp ecx, TC_ESC
        je .esc
        cmp ecx, TC_TAB
        je .tab
        cmp ecx, TC_REPAG
        je .repag
        cmp ecx, TC_AVPAG
        jne hecho
        mov ecx, O_PAG_SIG
        jmp orden
.repag:
        mov ecx, O_PAG_ANT
        jmp orden
.o_borrar:
        mov ecx, O_BORRAR
        jmp orden
.o_escribir:
        mov ecx, O_ESCRIBIR
        jmp orden
.esc:
        mov dword [herr], 0         ; fuera la herramienta y lo elegido
        mov dword [sel], -1
        mov byte [drag], 0
        call inval
        jmp hecho
.tab:
        ; elige el objeto siguiente de la página (con Mayús, el anterior)
        mov ebx, 1
        test edx, TM_MAYUS
        jz .t0
        neg ebx
.t0:
        mov ecx, [lam_n]
        test ecx, ecx
        jz hecho
        mov eax, [sel]
        test eax, eax
        jns .t1
        xor eax, eax                ; (sin nada elegido: desde un extremo)
        test ebx, ebx
        js .t1
        mov eax, [lam_n]
        dec eax
.t1:
        add eax, ebx
        jns .t2
        mov eax, [lam_n]
        dec eax
.t2:
        cmp eax, [lam_n]
        jb .t3
        xor eax, eax
.t3:
        call lam_ptr
        movzx edi, byte [esi+LO_PAG]
        cmp edi, [pag]
        je .t4
        dec ecx
        jnz .t1
        jmp hecho
.t4:
        mov dword [herr], 0
        call elige
        call inval
        jmp hecho
.m_izq:
        dec eax
        jmp .mueve
.m_der:
        inc eax
        jmp .mueve
.m_arr:
        dec ebx
        jmp .mueve
.m_aba:
        inc ebx
.mueve:
        ; las flechas mueven lo elegido: un punto (con Mayús, diez) o, con el
        ; ajuste puesto, un paso de la rejilla
        cmp dword [sel], 0
        jl hecho
        test edx, TM_MAYUS
        jz .mv1
        imul eax, 10
        imul ebx, 10
.mv1:
        cmp byte [moviendo], 0      ; (una tanda de flechas, un solo cambio)
        jne .mv2
        call instantanea
        mov byte [moviendo], 1
.mv2:
        mov ecx, eax
        mov eax, [sel]
        call lam_ptr
        cmp byte [ajustar], 0
        je .libre
        imul eax, ecx, 19
        add eax, [esi+LO_X]
        call ajusta
        mov [esi+LO_X], eax
        imul eax, ebx, 19
        add eax, [esi+LO_Y]
        call ajusta
        mov [esi+LO_Y], eax
        jmp .movido
.libre:
        add [esi+LO_X], ecx
        add [esi+LO_Y], ebx
.movido:
        call cambio
        jmp hecho
.ctrl:
        sub ecx, TC_CTRL
        cmp ecx, 26
        ja hecho
        movzx ecx, byte [ctrl_ord+ecx]
        test ecx, ecx
        jz hecho
        jmp orden
        ; ---- escribiendo en una caja ----
.editando:
        mov dword [br_sobre], 0
        cmp ecx, TC_ESC
        je .e_esc
        cmp ecx, TC_INTRO
        je .e_intro
        cmp ecx, TC_RETRO
        je .e_retro
        cmp ecx, TC_SUPR
        je .e_supr
        cmp ecx, TC_IZQ
        je .e_izq
        cmp ecx, TC_DER
        je .e_der
        cmp ecx, TC_ARRIBA
        je .e_arr
        cmp ecx, TC_ABAJO
        je .e_aba
        cmp ecx, TC_INICIO
        je .e_ini
        cmp ecx, TC_FIN
        je .e_fin
        cmp ecx, TC_TAB
        je .e_tab
        jmp hecho
.e_tab:
        ; en una lista, el tabulador baja de nivel el párrafo del cursor (le
        ; pone delante un tabulador, hasta dos) y Mayús+Tab lo sube
        call ed_ficha
        test byte [esi+LO_BAND], LB_VINETAS
        jz hecho
        mov ebx, [cur]              ; EBX=donde empieza su párrafo
        mov edi, [lam_t_txt]
.et1:
        test ebx, ebx
        jz .et2
        cmp byte [edi+ebx-1], 10
        je .et2
        dec ebx
        jmp .et1
.et2:
        xor ecx, ecx                ; ECX=los tabuladores que tiene
.et3:
        lea eax, [ebx+ecx]
        cmp eax, [esi+LO_LARGO]
        jae .et4
        cmp byte [edi+eax], 9
        jne .et4
        inc ecx
        jmp .et3
.et4:
        test edx, TM_MAYUS
        jnz .et_menos
        cmp ecx, 2
        jae hecho
        call inst_tecleo
        mov byte [ed_car], 9
        mov eax, [sel]
        xor ecx, ecx
        mov esi, ed_car
        mov edx, 1
        call lam_dat_cambia
        jc hecho
        inc dword [cur]
        call cambio
        jmp hecho
.et_menos:
        test ecx, ecx
        jz hecho
        call inst_tecleo
        mov eax, [sel]
        mov ecx, 1
        xor edx, edx
        call lam_dat_cambia
        cmp [cur], ebx
        jbe .et5
        dec dword [cur]
.et5:
        call cambio
        jmp hecho
.e_esc:
        call fin_edicion
        jmp hecho
.e_intro:
        ; en una lista, el párrafo nuevo sigue en el nivel del que se parte
        call ed_ficha
        xor ebp, ebp                ; EBP=los tabuladores que lleva delante
        test byte [esi+LO_BAND], LB_VINETAS
        jz .ei3
        mov ebx, [cur]
        mov edi, [lam_t_txt]
.ei1:
        test ebx, ebx
        jz .ei2
        cmp byte [edi+ebx-1], 10
        je .ei2
        dec ebx
        jmp .ei1
.ei2:
        cmp ebx, [cur]
        jae .ei3
        cmp byte [edi+ebx], 9
        jne .ei3
        inc ebp
        inc ebx
        jmp .ei2
.ei3:
        mov al, 10
        call ed_inserta
.ei4:
        test ebp, ebp
        jz hecho
        mov al, 9
        call ed_inserta
        dec ebp
        jmp .ei4
.e_retro:
        mov eax, [cur]
        test eax, eax
        jz hecho
        dec eax
        call ed_borra
        jmp hecho
.e_supr:
        mov eax, [sel]
        call lam_ptr
        mov eax, [cur]
        cmp eax, [esi+LO_LARGO]
        jae hecho
        call ed_borra
        jmp hecho
.e_izq:
        mov eax, [cur]
        test eax, eax
        jz hecho
        dec eax
        jmp .e_pon
.e_der:
        mov eax, [sel]
        call lam_ptr
        mov eax, [cur]
        cmp eax, [esi+LO_LARGO]
        jae hecho
        inc eax
        jmp .e_pon
.e_arr:
        call ed_ficha
        mov eax, [cur]
        call lam_sitio_xy           ; EAX=x EBX=y ECX=alto
        sub ebx, ecx
        cmp ebx, LAM_SEP
        jl hecho
        call lam_xy_sitio
        jmp .e_pon
.e_aba:
        call ed_ficha
        mov eax, [cur]
        call lam_sitio_xy
        add ebx, ecx
        call lam_xy_sitio
        jmp .e_pon
.e_ini:
        call ed_ficha
        mov eax, [cur]
        call lam_sitio_xy
        xor eax, eax
        call lam_xy_sitio
        jmp .e_pon
.e_fin:
        call ed_ficha
        mov eax, [cur]
        call lam_sitio_xy
        mov eax, 100000
        call lam_xy_sitio
.e_pon:
        mov [cur], eax
        mov byte [tecleando], 0
        call inval
        jmp hecho

; una letra: ECX. Con una caja de texto elegida, ponerse a escribir es
; escribir en ella, al final de lo que tiene
letra:
        cmp ecx, 32
        jb hecho
        cmp ecx, 255
        ja hecho
        mov dword [br_sobre], 0
        cmp byte [editando], 0
        jne .pon
        mov eax, [sel]
        test eax, eax
        js hecho
        call lam_ptr
        cmp byte [esi+LO_TIPO], LT_TEXTO
        jne hecho
        test byte [esi+LO_BAND], LB_HUECO
        jnz hecho
        mov eax, [esi+LO_LARGO]
        mov [cur], eax
        mov byte [editando], 1
.pon:
        mov al, cl
        call ed_inserta
        jmp hecho

; =============================================================================
;  El ratón: EDX=x + y*65536 ECX=mayúsculas
; =============================================================================
; (interna) los tiradores de lo elegido: sus esquinas en la ventana (sr_*) y
; [sr_linea] (una línea solo tiene dos, en sus extremos); CF=1 si no hay
; nada elegido en esta página
sel_rect:
        push eax
        push ebx
        push ecx
        push edx
        push esi
        mov eax, [sel]
        test eax, eax
        js .no
        call lam_ptr
        movzx eax, byte [esi+LO_PAG]
        cmp eax, [pag]
        jne .no
        cmp byte [esi+LO_TIPO], LT_LINEA
        sete [sr_linea]
        mov eax, [esi+LO_X]
        mov ebx, [esi+LO_Y]
        mov ecx, [esi+LO_AN]
        mov edx, [esi+LO_AL]
        call rect_v
        clc
        jmp .x
.no:
        stc
.x:
        pop esi
        pop edx
        pop ecx
        pop ebx
        pop eax
        ret
; (interna) ECX=tirador -> EAX, EBX=su centro en la ventana
tirador_xy:
        cmp byte [sr_linea], 0
        jne .lin
        movzx eax, byte [tir_fx+ecx]
        mov ebx, [sr_x1]
        sub ebx, [sr_x0]
        imul eax, ebx
        sar eax, 1
        add eax, [sr_x0]
        push eax
        movzx eax, byte [tir_fy+ecx]
        mov ebx, [sr_y1]
        sub ebx, [sr_y0]
        imul eax, ebx
        sar eax, 1
        add eax, [sr_y0]
        mov ebx, eax
        pop eax
        ret
.lin:
        mov eax, [sr_x0]
        mov ebx, [sr_y0]
        test ecx, ecx
        jz .x
        mov eax, [sr_x1]
        mov ebx, [sr_y1]
.x:
        ret
; (interna) EAX=x EBX=y de la ventana -> ECX=el tirador que hay ahí; CF=1 si
; ninguno
tirador_en:
        push edx
        push esi
        push edi
        call sel_rect
        jc .no
        mov esi, eax
        mov edi, ebx
        mov edx, 8
        cmp byte [sr_linea], 0
        je .n
        mov edx, 2
.n:
        xor ecx, ecx
.b:
        call tirador_xy
        sub eax, esi
        add eax, 4
        cmp eax, 8
        ja .s
        sub ebx, edi
        add ebx, 4
        cmp ebx, 8
        ja .s
        mov eax, esi
        mov ebx, edi
        clc
        jmp .x
.s:
        inc ecx
        cmp ecx, edx
        jb .b
        mov eax, esi
        mov ebx, edi
.no:
        stc
.x:
        pop edi
        pop esi
        pop edx
        ret

; (interna) ESI=ficha: su sitio y su tamaño, a lo que se arrastra (dr_*) y a
; como estaba (or_*)
dr_toma:
        push eax
        mov eax, [esi+LO_X]
        mov [dr_x], eax
        mov [or_x], eax
        mov eax, [esi+LO_Y]
        mov [dr_y], eax
        mov [or_y], eax
        mov eax, [esi+LO_AN]
        mov [dr_an], eax
        mov [or_an], eax
        mov eax, [esi+LO_AL]
        mov [dr_al], eax
        mov [or_al], eax
        pop eax
        ret

raton:
        movsx eax, dx
        mov ebx, edx
        sar ebx, 16
        cmp ebx, TB_AL
        jl .barra
        call en_vista               ; fuera de la vista (las barras, el pie)
        jc hecho
        mov [m_wx], eax
        mov [m_wy], ebx
        mov dword [br_sobre], 0
        call a_pagina
        mov [m_ux], eax
        mov [m_uy], ebx
        cmp dword [herr], 0
        je .elegir
        ; con una herramienta: se empieza a arrastrar un objeto nuevo
        call fin_edicion
        mov dword [sel], -1
        mov eax, [m_ux]
        call ajusta
        mov [dr_ax], eax
        mov [dr_x], eax
        mov eax, [m_uy]
        call ajusta
        mov [dr_ay], eax
        mov [dr_y], eax
        mov dword [dr_an], 0
        mov dword [dr_al], 0
        mov byte [drag], D_CREAR
        jmp .captura
.elegir:
        cmp byte [editando], 0
        je .sin_ed
        ; escribiendo: un clic dentro de la caja lleva el cursor ahí; fuera,
        ; se acaba de escribir
        mov eax, [sel]
        call lam_ptr
        mov eax, [m_ux]
        sub eax, [esi+LO_X]
        js .fuera
        cmp eax, [esi+LO_AN]
        jge .fuera
        mov ebx, [m_uy]
        sub ebx, [esi+LO_Y]
        js .fuera
        cmp ebx, [esi+LO_AL]
        jge .fuera
        call lam_compone
        call lam_xy_sitio
        mov [cur], eax
        mov byte [tecleando], 0
        call inval
        jmp hecho
.fuera:
        call fin_edicion
.sin_ed:
        cmp dword [sel], 0
        jl .busca
        mov eax, [m_wx]             ; ¿en un tirador de lo elegido?
        mov ebx, [m_wy]
        call tirador_en
        jc .busca
        mov [dr_h], ecx
        mov eax, [sel]
        call lam_ptr
        call dr_toma
        mov byte [drag], D_TAM
        jmp .captura
.busca:
        mov eax, [m_ux]             ; el objeto que hay ahí, elegido
        mov ebx, [m_uy]
        mov ecx, [pag]
        call holgura
        call lam_en_punto
        call elige
        test eax, eax
        js .nada
        call lam_ptr
        cmp byte [esi+LO_TIPO], LT_TEXTO    ; una caja vacía que enseña su texto
        jne .coge                           ; de ejemplo («haga clic para...»):
        cmp byte [esi+LO_PAPEL], 0          ; un clic basta para escribir en ella
        je .coge
        cmp dword [esi+LO_LARGO], 0
        jne .coge
        test byte [esi+LO_BAND], LB_HUECO
        jnz .coge
        mov dword [cur], 0
        mov byte [editando], 1
        jmp .nada
.coge:
        call dr_toma
        mov eax, [m_ux]             ; (de dónde se le coge)
        sub eax, [esi+LO_X]
        mov [m_dx], eax
        mov eax, [m_uy]
        sub eax, [esi+LO_Y]
        mov [m_dy], eax
        mov byte [dr_movido], 0
        mov byte [drag], D_MOVER
.captura:
        mov ebp, [ven]
        HV ven_capturar
.nada:
        call inval
        jmp hecho
.barra:
        mov esi, botones1           ; la fila de arriba o la de abajo
        cmp ebx, 30
        jl .b
        mov esi, botones
.b:
        call br_boton_en
        jc hecho
        mov ecx, [esi+12]
        jmp orden

raton_mueve:
        cmp byte [drag], 0
        jne .arrastra
        ; sobre un botón de la barra: su nombre, abajo
        xor edi, edi
        mov ebx, edx
        sar ebx, 16
        cmp ebx, TB_AL
        jae .nombre
        test ebx, ebx
        js .nombre
        movsx eax, dx
        mov esi, botones1
        cmp ebx, 30
        jl .f
        mov esi, botones
.f:
        call br_boton_en
        jc .nombre
        mov edi, [esi+16]
.nombre:
        call br_sobre_pon
        je .forma
        call inval
.forma:
        ; sobre un tirador de lo elegido, el cursor dice hacia dónde tira
        xor esi, esi                ; (CUR_FLECHA)
        cmp byte [editando], 0
        jne .fpon
        cmp dword [herr], 0
        jne .fpon
        movsx eax, dx
        mov ebx, edx
        sar ebx, 16
        cmp ebx, TB_AL
        jl .fpon
        call tirador_en
        jc .fpon
        mov esi, CUR_DIAG1          ; (el extremo de una línea)
        cmp byte [sr_linea], 0
        jne .fpon
        movzx esi, byte [tir_cursor+ecx]
.fpon:
        cmp esi, [cur_forma_act]
        je hecho
        mov [cur_forma_act], esi
        mov eax, esi
        mov ebp, [ven]
        HV cur_forma
        jmp hecho
.arrastra:
        movsx eax, dx
        mov ebx, edx
        sar ebx, 16
        mov [m_wx2], eax
        mov [m_wy2], ebx
        call a_pagina
        mov [m_ux], eax
        mov [m_uy], ebx
        cmp byte [drag], D_TAM
        je .tam
        cmp byte [drag], D_CREAR
        je .crear
        ; ---- mover: hasta que el ratón no se aparta un poco no cuenta (un
        ;      clic para elegir no mueve nada) ----
        cmp byte [dr_movido], 0
        jne .mv
        mov eax, [m_wx2]
        sub eax, [m_wx]
        add eax, 2
        cmp eax, 4
        ja .mv0
        mov eax, [m_wy2]
        sub eax, [m_wy]
        add eax, 2
        cmp eax, 4
        jbe hecho
.mv0:
        mov byte [dr_movido], 1
.mv:
        mov eax, [m_ux]
        sub eax, [m_dx]
        call ajusta
        mov [dr_x], eax
        mov eax, [m_uy]
        sub eax, [m_dy]
        call ajusta
        mov [dr_y], eax
        call inval
        jmp hecho
.crear:
        mov eax, [m_ux]
        call ajusta
        mov ecx, eax
        mov eax, [m_uy]
        call ajusta
        mov edx, eax                ; ECX, EDX=hasta dónde va
        cmp dword [herr], 4
        jne .caja
        mov eax, [dr_ax]            ; una línea: del primer punto a este
        sub ecx, eax
        mov [dr_an], ecx
        mov eax, [dr_ay]
        sub edx, eax
        mov [dr_al], edx
        call inval
        jmp hecho
.caja:
        mov eax, [dr_ax]            ; una caja: entre los dos puntos
        cmp eax, ecx
        jle .c1
        xchg eax, ecx
.c1:
        mov [dr_x], eax
        sub ecx, eax
        mov [dr_an], ecx
        mov eax, [dr_ay]
        cmp eax, edx
        jle .c2
        xchg eax, edx
.c2:
        mov [dr_y], eax
        sub edx, eax
        mov [dr_al], edx
        call inval
        jmp hecho
.tam:
        mov eax, [sel]
        call lam_ptr
        cmp byte [esi+LO_TIPO], LT_LINEA
        je .tam_linea
        ; una caja: cada tirador mueve uno o dos lados; los otros se quedan
        mov ecx, [dr_h]
        movzx ecx, byte [tir_lados+ecx]
        mov eax, [or_x]             ; EAX, EBX=lados izquierdo y derecho
        mov ebx, eax
        add ebx, [or_an]
        test cl, 1
        jz .t1
        mov eax, [m_ux]
        call ajusta
        lea edx, [ebx-4]            ; (que no se cruce con el otro)
        cmp eax, edx
        jle .t1
        mov eax, edx
.t1:
        test cl, 2
        jz .t2
        push eax
        mov eax, [m_ux]
        call ajusta
        mov ebx, eax
        pop eax
        lea edx, [eax+4]
        cmp ebx, edx
        jge .t2
        mov ebx, edx
.t2:
        mov [dr_x], eax
        sub ebx, eax
        mov [dr_an], ebx
        mov eax, [or_y]             ; y los de arriba y abajo
        mov ebx, eax
        add ebx, [or_al]
        test cl, 4
        jz .t3
        mov eax, [m_uy]
        call ajusta
        lea edx, [ebx-4]
        cmp eax, edx
        jle .t3
        mov eax, edx
.t3:
        test cl, 8
        jz .t4
        push eax
        mov eax, [m_uy]
        call ajusta
        mov ebx, eax
        pop eax
        lea edx, [eax+4]
        cmp ebx, edx
        jge .t4
        mov ebx, edx
.t4:
        mov [dr_y], eax
        sub ebx, eax
        mov [dr_al], ebx
        call inval
        jmp hecho
.tam_linea:
        mov eax, [m_ux]
        call ajusta
        mov ecx, eax
        mov eax, [m_uy]
        call ajusta
        mov edx, eax
        cmp dword [dr_h], 0
        jne .tl2
        mov eax, [or_x]             ; el primer extremo se mueve; el segundo no
        add eax, [or_an]
        sub eax, ecx
        mov [dr_x], ecx
        mov [dr_an], eax
        mov eax, [or_y]
        add eax, [or_al]
        sub eax, edx
        mov [dr_y], edx
        mov [dr_al], eax
        call inval
        jmp hecho
.tl2:
        sub ecx, [or_x]             ; el segundo extremo
        mov [dr_an], ecx
        sub edx, [or_y]
        mov [dr_al], edx
        call inval
        jmp hecho

raton_suelta:
        cmp byte [drag], 0
        je hecho
        mov ebp, [ven]
        HV ven_soltar
        mov al, [drag]
        mov byte [drag], 0
        cmp al, D_CREAR
        je .crear
        cmp al, D_TAM
        je .pon
        cmp byte [dr_movido], 0
        je .nada
.pon:
        call instantanea            ; el objeto, a donde se ha dejado
        mov eax, [sel]
        call lam_ptr
        mov eax, [dr_x]
        mov [esi+LO_X], eax
        mov eax, [dr_y]
        mov [esi+LO_Y], eax
        mov eax, [dr_an]
        mov [esi+LO_AN], eax
        mov eax, [dr_al]
        mov [esi+LO_AL], eax
        call cambio
        jmp hecho
.nada:
        call inval
        jmp hecho
.crear:
        call crea_obj
        jmp hecho

; pone el objeto que se acaba de arrastrar con la herramienta [herr], con el
; formato de ahora; un clic sin arrastrar lo pone de un tamaño corriente
crea_obj:
        pushad
        call instantanea
        mov eax, [herr]
        mov cl, [pag]
        call lam_obj_nuevo
        jc .lleno
        mov [sel], eax
        mov eax, [dr_an]            ; ¿se ha arrastrado algo?
        cdq
        xor eax, edx
        sub eax, edx
        mov ebx, eax
        mov eax, [dr_al]
        cdq
        xor eax, edx
        sub eax, edx
        add eax, ebx
        cmp eax, 6
        jae .tam
        mov dword [dr_an], 150
        mov dword [dr_al], 100
        cmp dword [herr], 1
        jne .t1
        mov dword [dr_an], 220
        mov dword [dr_al], 60
.t1:
        cmp dword [herr], 4
        jne .tam
        mov dword [dr_al], 0
.tam:
        mov eax, [dr_x]
        mov [esi+LO_X], eax
        mov eax, [dr_y]
        mov [esi+LO_Y], eax
        mov eax, [dr_an]
        mov [esi+LO_AN], eax
        mov eax, [dr_al]
        mov [esi+LO_AL], eax
        cmp dword [herr], 4
        je .forma
        cmp dword [esi+LO_AN], 8    ; (una caja mide algo)
        jge .t2
        mov dword [esi+LO_AN], 8
.t2:
        cmp dword [esi+LO_AL], 8
        jge .t3
        mov dword [esi+LO_AL], 8
.t3:
        xor ecx, ecx                ; el formato: el de las cajas de texto...
        cmp dword [herr], 1
        jne .forma
        mov al, [act_letra]
        mov [esi+LO_LETRA], al
        mov al, [act_cuerpo]
        mov [esi+LO_CUERPO], al
        mov al, [act_estilo]
        mov [esi+LO_ESTILO], al
        mov al, [act_alin]
        mov [esi+LO_ALIN], al
        mov al, [act_tinta]
        mov [esi+LO_TINTA], al
        jmp .color
.forma:
        mov ecx, 1                  ; ...o el de las formas
.color:
        mov al, [act_fondo+ecx]
        mov [esi+LO_FONDO], al
        mov al, [act_borde+ecx]
        mov [esi+LO_BORDE], al
        mov al, [act_grosor+ecx]
        mov [esi+LO_GROSOR], al
        cmp dword [herr], 4         ; una línea sin color no se vería
        je .visible
        cmp dword [herr], 1
        je .hecho
        cmp byte [esi+LO_FONDO], LAM_NADA   ; ni una forma sin relleno ni borde
        jne .hecho
.visible:
        cmp byte [esi+LO_BORDE], LAM_NADA
        jne .hecho
        mov byte [esi+LO_BORDE], C_TINTA
.hecho:
        cmp dword [herr], 1         ; en una caja de texto nueva se escribe ya
        jne .fin
        mov dword [cur], 0
        mov byte [editando], 1
.fin:
        mov dword [herr], 0
        call cambio
        popad
        ret
.lleno:
        mov dword [herr], 0
        mov esi, t_lleno
        call aviso
        call inval
        popad
        ret

; doble clic: en una caja de texto, a escribir dentro, donde se ha pulsado
raton_doble:
        movsx eax, dx
        mov ebx, edx
        sar ebx, 16
        cmp ebx, TB_AL
        jl raton
        cmp dword [herr], 0
        jne raton
        call en_vista
        jc hecho
        call a_pagina
        mov [m_ux], eax
        mov [m_uy], ebx
        call fin_edicion
        mov ecx, [pag]
        call holgura
        call lam_en_punto
        call elige
        test eax, eax
        js .x
        call lam_ptr
        cmp byte [esi+LO_TIPO], LT_TEXTO
        jne .x
        test byte [esi+LO_BAND], LB_HUECO
        jnz .x
        call lam_compone
        mov eax, [m_ux]
        sub eax, [esi+LO_X]
        mov ebx, [m_uy]
        sub ebx, [esi+LO_Y]
        call lam_xy_sitio
        mov [cur], eax
        mov byte [editando], 1
.x:
        call inval
        jmp hecho

; =============================================================================
;  El formato
; =============================================================================
; (interna) pone AL en el campo EBX de la ficha de lo elegido, si lo hay y es
; de uno de los tipos de la máscara EDX (bit 1 texto, 2 rectángulo...)
pon_campo:
        pushad
        mov edi, eax
        mov eax, [sel]
        test eax, eax
        js .no
        call lam_ptr
        movzx ecx, byte [esi+LO_TIPO]
        bt edx, ecx
        jnc .no
        call instantanea
        mov eax, edi
        mov [esi+ebx], al
        call cambio
        popad
        ret
.no:
        call inval                  ; (la barra enseña el formato nuevo)
        popad
        ret
M_TEXTO     equ 1 << LT_TEXTO
M_CAJAS     equ (1 << LT_TEXTO) | (1 << LT_RECT) | (1 << LT_ELIPSE)
M_TODOS     equ 0x3E

; AL=el bit (negrita, cursiva, subrayado): lo pone o lo quita
estilo:
        xor [act_estilo], al
        mov al, [act_estilo]
        mov ebx, LO_ESTILO
        mov edx, M_TEXTO
        jmp pon_campo
; EAX=alineación (0-2)
alinea:
        mov [act_alin], al
        mov ebx, LO_ALIN
        mov edx, M_TEXTO
        jmp pon_campo
; Ctrl+W: 24 puntos, o el tamaño normal si ya lo estaba
grande:
        mov al, 3
        cmp [act_cuerpo], al
        jne tam_pon
        xor al, al
tam_pon:
        mov [act_cuerpo], al
        mov ebx, LO_CUERPO
        mov edx, M_TEXTO
        jmp pon_campo

; ---- elegir de una lista: [le_titulo], [le_n] cosas, [le_texto] (la rutina
;      que da el texto de la cosa EAX en ESI) y [le_sel] (la de ahora) ->
;      ZF=1 si se acepta, y [le_sel] es la elegida ----
lista_elige:
        mov esi, dlg_lista
        cmp dword [le_n], 7
        ja .l
        mov esi, dlg_listac
.l:
        mov eax, [le_titulo]
        mov [esi+P_TEXTO], eax
        lea edi, [esi+P_TAM]
        mov ebp, [ven]
        mov ebx, lista_gancho
        xor edx, edx
        HV dlg_modal
        cmp eax, ID_ACEPTAR
        ret
lista_gancho:
        cmp eax, DM_INICIO
        je .ini
        cmp eax, WM_COMMAND
        jne .no
        cmp cx, 60                  ; la lista: doble clic o Intro, aceptar
        jne .boton
        shr ecx, 16
        cmp ecx, LN_DOBLE
        jne .no
        mov eax, WM_COMMAND
        mov ecx, ID_ACEPTAR
        xor edx, edx
        HV msg_enviar
        jmp .no
.boton:
        cmp ecx, ID_ACEPTAR
        jne .no
        pushad
        mov ecx, 60
        HV ven_hija_id
        mov ebp, eax
        mov eax, LM_SELEC
        HV msg_enviar
        test eax, eax
        js .b1
        mov [le_sel], eax
.b1:
        popad
        jmp .no
.ini:
        pushad
        mov ecx, 60
        HV ven_hija_id
        mov ebp, eax
        xor ebx, ebx
.i1:
        cmp ebx, [le_n]
        jae .i2
        mov eax, ebx
        call [le_texto]
        mov edx, esi
        mov eax, LM_ANADIR
        HV msg_enviar
        inc ebx
        jmp .i1
.i2:
        mov ecx, [le_sel]
        cmp ecx, [le_n]
        jb .i3
        xor ecx, ecx
.i3:
        mov eax, LM_PONER_SELEC
        HV msg_enviar
        popad
.no:
        xor eax, eax
        ret

; El procedimiento de las listas de los diálogos: el de c_lista, salvo el
; tabulador. c_lista, al pasar a ven_defecto las teclas que no usa, ha perdido
; ya las mayúsculas (EDX), y según cuántas cosas tenga la lista el tabulador
; va hacia atrás: aquí se le da antes de que llegue a ella
lista2_proc:
        cmp eax, WM_KEYDOWN
        jne .suya
        cmp ecx, TC_TAB
        jne .suya
        HVJ ven_defecto
.suya:
        jmp [lista_orig]

; ---- el tipo de letra ----
tipo_elige:
        mov dword [le_titulo], t_d_tipo
        mov eax, [tl_n]
        mov [le_n], eax
        mov dword [le_texto], tl_nombre
        movzx eax, byte [act_letra]
        mov [le_sel], eax
        call lista_elige
        jne .x
        mov al, [le_sel]
        mov [act_letra], al
        mov ebx, LO_LETRA
        mov edx, M_TEXTO
        jmp pon_campo
.x:
        ret
; ---- el tamaño de la letra (en la lista, de menor a mayor) ----
tam_elige:
        mov dword [le_titulo], t_d_tam
        mov dword [le_n], TL_TAMANOS
        mov dword [le_texto], .texto
        movzx eax, byte [act_cuerpo]
        movzx eax, byte [tl_orden+eax]
        mov [le_sel], eax
        call lista_elige
        jne .x
        mov eax, [le_sel]
        mov al, [tl_orden+eax]
        jmp tam_pon
.x:
        ret
.texto:
        movzx edx, byte [tl_orden+eax]
        jmp tl_puntos

; ---- los colores: el de la letra, de los 16; el relleno y el borde pueden
;      además no tener («Ninguno», el primero de la lista) ----
color_letra:
        mov dword [le_titulo], t_d_c_letra
        mov dword [le_n], 16
        mov dword [le_texto], .texto
        movzx eax, byte [act_tinta]
        mov [le_sel], eax
        call lista_elige
        jne .x
        mov al, [le_sel]
        mov [act_tinta], al
        mov ebx, LO_TINTA
        mov edx, M_TEXTO
        jmp pon_campo
.x:
        ret
.texto:
        mov esi, [t_colores+eax*4]
        ret
color_fondo:
        mov dword [le_titulo], t_d_c_fondo
        call clase
        movzx eax, byte [act_fondo+eax]
        call color_o_nada
        jne color_letra.x
        call clase
        mov [act_fondo+eax], dl
        mov al, dl
        mov ebx, LO_FONDO
        mov edx, M_CAJAS
        jmp pon_campo
color_borde:
        mov dword [le_titulo], t_d_c_borde
        call clase
        movzx eax, byte [act_borde+eax]
        call color_o_nada
        jne color_letra.x
        call clase
        mov [act_borde+eax], dl
        mov al, dl
        mov ebx, LO_BORDE
        mov edx, M_TODOS
        cmp al, LAM_NADA            ; (a una línea no se le quita el color:
        jne pon_campo               ; no se vería)
        mov edx, M_TODOS & ~(1 << LT_LINEA)
        jmp pon_campo
; (interna) EAX=el color de ahora (LAM_NADA: ninguno): lo pide -> ZF=1 si se
; acepta y DL=el elegido
color_o_nada:
        mov dword [le_n], 17
        mov dword [le_texto], .texto
        inc eax                     ; (en la lista: 0 ninguno, 1 a 16 los colores)
        and eax, 0xFF
        mov [le_sel], eax
        call lista_elige
        jne .x
        mov dl, [le_sel]
        dec dl                      ; (el 0 pasa a FFh: LAM_NADA)
        cmp eax, eax
.x:
        ret
.texto:
        mov esi, t_ninguno
        test eax, eax
        jz .t
        mov esi, [t_colores+eax*4-4]
.t:
        ret
; ---- el grosor del borde o de la línea ----
grosor_elige:
        mov dword [le_titulo], t_d_grosor
        mov dword [le_n], N_GROSORES
        mov dword [le_texto], .texto
        call clase
        mov dl, [act_grosor+eax]
        xor eax, eax
.b:
        cmp [grosores+eax], dl
        jae .e
        inc eax
        cmp eax, N_GROSORES - 1
        jb .b
.e:
        mov [le_sel], eax
        call lista_elige
        jne .x
        mov edx, [le_sel]
        mov dl, [grosores+edx]
        call clase
        mov [act_grosor+eax], dl
        mov al, dl
        mov ebx, LO_GROSOR
        mov edx, M_TODOS
        jmp pon_campo
.x:
        ret
.texto:
        movzx eax, byte [grosores+eax]
        mov edi, num_txt
        call hvl_num
        mov byte [edi], 0
        mov esi, num_txt
        ret

; =============================================================================
;  Deshacer. Antes de cada cambio se guarda el documento entero (los dos
;  trozos del motor: lam_doc y lo usado de lam_dat); una tanda de teclas o de
;  flechas seguidas cuenta como un solo cambio. Deshacer cambia el documento
;  por lo guardado: por eso Rehacer es volver a cambiarlos.
; =============================================================================
inst_tecleo:
        cmp byte [tecleando], 0
        jne instantanea.x
        call instantanea
        mov byte [tecleando], 1
        ret
instantanea:
        pushad
        mov byte [des_hecho], 0
        mov byte [tecleando], 0
        mov byte [moviendo], 0
        mov ecx, [lam_dat_n]        ; sitio para los datos (se pide al usarlo);
        mov ebx, des_dat            ; si no hay memoria, no hay qué deshacer
        xor al, al
        call lam_bloque
        jnc .hay
        mov byte [des_hay], 0
        popad
        stc
        ret
.hay:
        mov esi, lam_doc
        mov edi, des_doc
        mov ecx, LAM_DOC_TAM / 4
        rep movsd
        mov esi, [lam_dat]
        mov edi, [des_dat]
        mov ecx, [lam_dat_n]
        add ecx, 3
        shr ecx, 2
        rep movsd
        mov eax, [sel]
        mov [des_sel], eax
        mov eax, [pag]
        mov [des_pag], eax
        mov byte [des_hay], 1
        popad
        clc
.x:
        ret
deshacer:
        cmp byte [des_hecho], 0     ; (deshacer dos veces no rehace: para eso
        jne des_cambia.x            ; está Rehacer)
        mov byte [des_hecho], 2
rehacer:
        dec byte [des_hecho]        ; (Rehacer solo vale detrás de Deshacer)
        cmp byte [des_hecho], 0
        jl .no
        cmp byte [des_hay], 0
        je des_cambia.x
        call des_cambia
        mov byte [modif], 1
        ret
.no:
        mov byte [des_hecho], 0
        ret
; (interna) cambia el documento por el guardado, y al revés
des_cambia:
        pushad
        mov byte [editando], 0
        mov byte [drag], 0
        mov byte [tecleando], 0
        mov byte [moviendo], 0
        mov ecx, [lam_dat_n]        ; los datos: hasta donde llegue el más largo
        cmp ecx, [des_doc+20]
        jae .l
        mov ecx, [des_doc+20]
.l:
        mov al, 1                   ; (y que quepa en los dos sitios)
        mov ebx, lam_dat
        call lam_bloque
        jc .sin
        mov ebx, des_dat
        call lam_bloque
        jnc .cabe
.sin:
        mov esi, t_sin_mem
        call aviso
        popad
        ret
.cabe:
        add ecx, 3
        shr ecx, 2
        mov esi, [lam_dat]
        mov edi, [des_dat]
        call .trueca
        mov esi, lam_doc
        mov edi, des_doc
        mov ecx, LAM_DOC_TAM / 4
        call .trueca
        mov eax, [sel]
        xchg eax, [des_sel]
        cmp eax, [lam_n]
        jb .s
        mov eax, -1
.s:
        mov [sel], eax
        mov eax, [pag]
        xchg eax, [des_pag]
        cmp eax, [lam_pags]
        jb .p
        mov eax, [lam_pags]
        dec eax
.p:
        mov [pag], eax
        mov eax, [sel]              ; lo elegido, solo si está en esta página
        test eax, eax
        js .v
        call lam_ptr
        movzx eax, byte [esi+LO_PAG]
        cmp eax, [pag]
        je .v
        mov dword [sel], -1
.v:
        mov eax, [sel]
        call elige
        mov byte [esc_sucio], 1
        mov byte [ed_todo], 1
        call calc_vista             ; (puede haber cambiado el papel)
        call sucio
        popad
.x:
        ret
.trueca:
        jecxz .t2
.t1:
        mov eax, [esi]
        mov edx, [edi]
        mov [esi], edx
        mov [edi], eax
        add esi, 4
        add edi, 4
        dec ecx
        jnz .t1
.t2:
        ret

; =============================================================================
;  Borrar, duplicar, el orden y el portapapeles
; =============================================================================
borrar:
        call fin_edicion
        mov eax, [sel]
        test eax, eax
        js .x
        call instantanea
        call lam_obj_quita
        mov dword [sel], -1
        call cambio
.x:
        ret

duplicar:
        call fin_edicion
        mov eax, [sel]
        test eax, eax
        js .x
        call instantanea
        call lam_obj_dup
        jc sin_sitio
        call lam_ptr
        add dword [esi+LO_X], PASO  ; un poco apartada, para que se vea
        add dword [esi+LO_Y], PASO
        call elige
        call cambio
.x:
        ret
sin_sitio:
        mov esi, t_sin_sitio
        cmp dword [lam_n], LAM_MAX_OBJ
        jb .a
        mov esi, t_lleno
.a:
        jmp aviso

al_frente:
        mov edx, 1
        jmp orden_z
al_fondo:
        xor edx, edx
orden_z:
        call fin_edicion
        mov eax, [sel]
        test eax, eax
        js .x
        call instantanea
        call lam_obj_orden
        mov [sel], eax
        call cambio
.x:
        ret

; Copiar: el objeto elegido, a un portapapeles propio (la ficha y sus datos).
; Si es una caja de texto, su texto va además al portapapeles de Hispa
; Ventanas, por el cuadro de texto escondido (el «puente»), para pegarlo en
; otro programa
copiar:
        pushad
        mov eax, [sel]
        test eax, eax
        js .x
        call lam_ptr
        mov ebx, esi
        push ebx                    ; sitio para sus datos (se pide al usarlo)
        mov ecx, [ebx+LO_LARGO]
        mov ebx, clip_dat
        xor al, al
        call lam_bloque
        pop ebx
        jnc .sitio
        mov byte [clip_hay], 0
        mov esi, t_sin_mem
        call aviso
        jmp .x
.sitio:
        mov edi, clip_ficha
        mov ecx, LO_TAM
        rep movsb
        mov esi, [ebx+LO_DAT]
        add esi, [lam_dat]
        mov edi, [clip_dat]
        mov ecx, [ebx+LO_LARGO]
        rep movsb
        mov byte [clip_hay], 1
        cmp byte [ebx+LO_TIPO], LT_TEXTO
        jne .x
        mov esi, [ebx+LO_DAT]
        add esi, [lam_dat]
        mov edi, pega_txt
        mov ecx, [ebx+LO_LARGO]
        cmp ecx, TXT_MAX
        jbe .c
        mov ecx, TXT_MAX
.c:
        rep movsb
        mov byte [edi], 0
        mov ebp, [puente]
        mov eax, WM_SETTEXT
        mov edx, pega_txt
        HV msg_enviar
        mov eax, EM_SEL_TODO
        HV msg_enviar
        mov eax, WM_COPY
        HV msg_enviar
.x:
        popad
        ret
cortar:
        cmp dword [sel], 0
        jl .x
        call copiar
        call borrar
.x:
        ret
; Pegar: escribiendo en una caja, el texto del portapapeles de Hispa
; Ventanas, donde el cursor; si no, el objeto copiado, en esta página
pegar:
        cmp byte [editando], 0
        jne pegar_texto
        cmp byte [clip_hay], 0
        je .x
        call instantanea
        mov al, [clip_ficha+LO_TIPO]
        mov cl, [pag]
        call lam_obj_nuevo
        jc sin_sitio
        mov ebx, eax                ; EBX=el objeto nuevo
        add dword [clip_ficha+LO_X], PASO   ; (cada copia, un poco más allá)
        add dword [clip_ficha+LO_Y], PASO
        mov edx, [esi+LO_DAT]
        push esi
        mov edi, esi
        mov esi, clip_ficha
        mov ecx, LO_TAM
        rep movsb
        pop esi
        mov [esi+LO_DAT], edx
        mov dword [esi+LO_LARGO], 0
        mov al, [pag]
        mov [esi+LO_PAG], al
        mov eax, ebx
        mov edx, [clip_ficha+LO_LARGO]
        mov esi, [clip_dat]
        xor ebx, ebx
        xor ecx, ecx
        call lam_dat_cambia
        jnc .cabe
        dec dword [lam_n]           ; no caben sus datos: fuera
        jmp sin_sitio
.cabe:
        mov dword [herr], 0
        call elige
        call cambio
.x:
        ret
pegar_texto:
        pushad
        mov ebp, [puente]
        mov eax, WM_SETTEXT
        mov edx, t_nada
        HV msg_enviar
        mov eax, WM_PASTE
        HV msg_enviar
        mov eax, WM_GETTEXT
        mov ecx, TXT_MAX
        mov edx, pega_txt
        HV msg_enviar
        mov esi, pega_txt           ; sin los retornos de carro; ECX=cuánto es
        mov edi, pega_txt
.l:
        lodsb
        test al, al
        jz .l1
        cmp al, 13
        je .l
        stosb
        jmp .l
.l1:
        mov ecx, edi
        sub ecx, pega_txt
        jz .x
        mov eax, [sel]
        call lam_ptr
        mov eax, [esi+LO_LARGO]     ; lo que quepa en la caja
        add eax, ecx
        sub eax, TXT_MAX
        jbe .cabe
        sub ecx, eax
        jbe .x
.cabe:
        call instantanea
        mov eax, [sel]
        mov ebx, [cur]
        mov edx, ecx
        xor ecx, ecx
        mov esi, pega_txt
        call lam_dat_cambia
        jc .x
        add [cur], edx
        call cambio
.x:
        popad
        ret

; ---- Insertar > Imagen: un BMP, a su tamaño (o al de la página, si es
;      mayor), en la esquina de los márgenes ----
imagen:
        call fin_edicion
        mov byte [nom_tmp], 0
        mov ebp, [ven]
        mov ebx, nom_tmp
        mov edi, t_ins_img
        HV dlg_archivo
        test eax, eax
        jz .x
        call instantanea
        mov esi, nom_tmp
        mov cl, [pag]
        call lam_imagen_bmp
        jc .mal
        mov ebx, [lam_margen]
        mov [esi+LO_X], ebx
        mov [esi+LO_Y], ebx
        mov byte [esi+LO_BORDE], LAM_NADA
        ; si no cabe entre los márgenes, se encoge sin deformarla
        mov ecx, [lam_an]
        sub ecx, ebx
        sub ecx, ebx
        mov edi, [lam_al]
        sub edi, ebx
        sub edi, ebx                ; ECX, EDI=el sitio que hay
        push eax
        mov eax, [esi+LO_AN]
        cmp eax, ecx
        jbe .alto
        mov eax, [esi+LO_AL]        ; alto x sitio / ancho
        mul ecx
        div dword [esi+LO_AN]
        test eax, eax
        jnz .a1
        inc eax
.a1:
        mov [esi+LO_AL], eax
        mov [esi+LO_AN], ecx
.alto:
        mov eax, [esi+LO_AL]
        cmp eax, edi
        jbe .cabe
        mov eax, [esi+LO_AN]
        mul edi
        div dword [esi+LO_AL]
        test eax, eax
        jnz .a2
        inc eax
.a2:
        mov [esi+LO_AN], eax
        mov [esi+LO_AL], edi
.cabe:
        pop eax
        mov dword [herr], 0
        call elige
        call cambio
.x:
        ret
.mal:
        mov esi, t_no_abrir
        test eax, eax
        jz .m
        mov esi, t_no_bmp
        cmp eax, 1
        je .m
        mov esi, t_bmp_grande
.m:
        jmp aviso

; =============================================================================
;  Archivos
; =============================================================================
; si hay cambios sin guardar, pregunta; CF=1 si no se sigue
preguntar:
        call fin_edicion
        cmp byte [modif], 0
        je .si
        pushad
        mov ebp, [ven]
        mov esi, t_cambios
        mov edi, t_programa
        mov eax, DM_SI_NO_CANCELAR
        HV dlg_mensaje
        mov [esp+28], eax
        popad
        cmp eax, ID_NO
        je .si
        cmp eax, ID_SI
        jne .no
        call guardar
        ret
.si:
        clc
        ret
.no:
        stc
        ret

; ESI=plantilla CL=lámina: pone ahí sus objetos
plantilla_pon:
        pushad
        mov ebx, esi
        mov [pl_pag], cl
        mov eax, [lam_an]           ; en un papel pequeño, la letra pequeña
        imul eax, [lam_al]
        cmp eax, 600000
        setb [pl_peq]
.b:
        mov al, [ebx]
        test al, al
        jz .x
        mov cl, [pl_pag]
        call lam_obj_nuevo
        jc .x
        mov edi, eax                ; EDI=el objeto
        mov al, [ebx+10]
        mov [esi+LO_PAPEL], al
        mov al, [ebx+11]
        mov [esi+LO_BAND], al
        mov al, [ebx+1]
        mov [esi+LO_ESTILO], al
        mov al, [ebx+2]
        mov [esi+LO_ALIN], al
        mov al, [ebx+3]
        mov [esi+LO_TINTA], al
        mov al, [ebx+4]
        mov [esi+LO_FONDO], al
        mov al, [ebx+5]
        mov [esi+LO_BORDE], al
        mov al, [ebx+6]
        mov [esi+LO_GROSOR], al
        mov al, [ebx+7]
        mov [esi+LO_LETRA], al
        mov al, [ebx+8]
        cmp byte [pl_peq], 0
        je .c
        mov al, [ebx+9]
.c:
        mov [esi+LO_CUERPO], al
        mov ecx, 1000               ; las medidas, de milésimas a puntos
        movsx eax, word [ebx+12]
        imul dword [lam_an]
        idiv ecx
        mov [esi+LO_X], eax
        movsx eax, word [ebx+14]
        imul dword [lam_al]
        idiv ecx
        mov [esi+LO_Y], eax
        movsx eax, word [ebx+16]
        imul dword [lam_an]
        idiv ecx
        mov [esi+LO_AN], eax
        movsx eax, word [ebx+18]
        imul dword [lam_al]
        idiv ecx
        mov [esi+LO_AL], eax
        mov esi, [ebx+20]           ; y su texto
        test esi, esi
        jz .s
        xor edx, edx
.l:
        cmp byte [esi+edx], 0
        je .l1
        inc edx
        jmp .l
.l1:
        push ebx
        mov eax, edi
        xor ebx, ebx
        xor ecx, ecx
        call lam_dat_cambia
        pop ebx
.s:
        add ebx, PL_TAM
        jmp .b
.x:
        popad
        ret

abrir:
        call preguntar
        jc .x
        mov byte [nom_tmp], 0
        mov ebp, [ven]
        mov ebx, nom_tmp
        mov edi, t_abrir
        HV dlg_archivo
        test eax, eax
        jz .x
        call cargar
.x:
        ret

; abre el archivo nom_tmp. Si no se puede, se queda el documento que había
; (el motor lo puede haber estropeado a medias: se guarda antes)
cargar:
        pushad
        call fin_edicion
        mov al, [des_hay]
        mov ah, [des_hecho]
        push eax
        call instantanea
        jnc .sigue
        pop eax                     ; sin memoria para guardar lo que hay: no se abre
        mov esi, t_sin_mem
        call aviso
        popad
        ret
.sigue:
        mov esi, nom_tmp
        mov edi, firma
        call lam_carga
        jnc .bien
        mov ebx, eax
        call des_cambia             ; como estaba
        pop eax
        mov byte [des_hay], 0       ; (lo que había para deshacer se ha perdido)
        mov byte [des_hecho], 0
        mov esi, t_no_abrir
        test ebx, ebx
        jz .m
        mov esi, t_no_valido
.m:
        call aviso
        popad
        ret
.bien:
        pop eax
        mov esi, nom_tmp
        mov edi, nombre
        call copia
        call doc_estrena
        popad
        ret

; Guardar; CF=1 si no se guarda
guardar:
        cmp byte [nombre], 0
        jne guarda_escribe
guardar_como:
        pushad
        mov esi, nombre
        mov edi, nom_tmp
        call copia
        mov ebp, [ven]
        mov ebx, nom_tmp
        mov edi, t_guardar_como
        HV dlg_archivo
        test eax, eax
        jz .no
        mov esi, nom_tmp            ; sin extensión, .PUB
        mov edi, nombre
        mov ebx, t_ext
        call con_extension
        popad
        jmp guarda_escribe
.no:
        popad
        stc
        ret
; (interna) copia el nombre ESI a EDI y, si no trae extensión, le pone EBX
con_extension:
        xor edx, edx
.c:
        lodsb
        test al, al
        jz .f
        cmp al, '.'
        jne .c1
        inc edx
.c1:
        cmp al, '\'
        jne .c2
        xor edx, edx
.c2:
        stosb
        jmp .c
.f:
        test edx, edx
        jnz .f2
        mov esi, ebx
        call hvl_cad
.f2:
        mov byte [edi], 0
        ret

; escribe el documento en [nombre]. Primero entero en un temporal (el mismo
; nombre, con .TMP); solo cuando está bien escrito se quita el de antes y el
; temporal toma su nombre: si el disco se llena o se va la luz a mitad, en el
; disco sigue habiendo un documento entero (el de antes o, como mucho, el
; nuevo en el .TMP)
guarda_escribe:
        pushad
        mov byte [tecleando], 0
        mov esi, nombre
        mov edi, nom_seg
        mov eax, 'TMP'
        call hvl_ruta
        mov esi, nombre             ; (un nombre sin extensión, o que ya es
        mov edi, nom_seg            ; .TMP: se escribe directamente)
.c:
        mov al, [esi]
        cmp al, [edi]
        jne .seguro
        inc esi
        inc edi
        test al, al
        jnz .c
        mov esi, nombre
        mov edi, firma
        call lam_guarda
        jc .mal
        jmp .bien
.seguro:
        mov esi, nom_seg
        mov edi, firma
        call lam_guarda
        jc .mal_tmp
        mov esi, nombre
        HV dos_borrar               ; (si es la primera vez, no hay de antes)
        mov esi, nom_seg
        mov edi, nombre
        call renombra
        jc .mal_tmp
.bien:
        mov byte [modif], 0
        call pon_titulo
        popad
        clc
        ret
.mal_tmp:
        mov esi, nom_seg            ; (el temporal a medias, fuera)
        HV dos_borrar
.mal:
        mov esi, t_no_guardar
        call aviso
        popad
        stc
        ret
; ESI=nombre de antes EDI=el nuevo: cambia de nombre un archivo (INT 21h,
; 56h, con los dos nombres en el búfer de paso de DOS). CF=1 si no se puede
renombra:
        pushad
        mov ebx, [hv_api + HV_dos_buf * 4]
        push edi
        mov edi, ebx
        call copia
        pop esi
        lea edi, [ebx+128]
        call copia
        mov edi, rl
        mov ecx, RL_TAM / 4
        xor eax, eax
        rep stosd
        mov dword [rl+RL_EAX], 0x5600
        mov [rl+RL_EDX], ebx
        add ebx, 128
        mov [rl+RL_EDI], ebx
        mov byte [rl+RL_INT], 0x21
        mov esi, rl
        HV rm_interrup
        popad
        ret

; ---- Exportar la página como BMP: de 256 colores, con la paleta de Hispa
;      Ventanas, a tamaño real (96 puntos por pulgada) ----
exportar:
        pushad
        call fin_edicion
        mov byte [nom_tmp], 0
        mov ebp, [ven]
        mov ebx, nom_tmp
        mov edi, t_exportar
        HV dlg_archivo
        test eax, eax
        jz .x
        mov esi, nom_tmp
        mov edi, nom_exp
        mov ebx, t_ext_bmp
        call con_extension
        mov ecx, [pag]              ; la página, recién dibujada (y sin los
        push dword [lam_guia_fn]    ; textos de ejemplo de las cajas vacías)
        mov dword [lam_guia_fn], 0
        call lam_pinta
        pop dword [lam_guia_fn]
        cmp byte [lam_sin_mem], 0
        je .dibujada
        mov esi, t_sin_mem
        call aviso
        popad
        ret
.dibujada:
        mov byte [lam_sucio], 1
        mov esi, nom_exp
        HV dos_crear
        jc .mal0
        mov ebx, eax
        mov edi, cab
        xor eax, eax
        mov ecx, 14
        rep stosd
        mov word [cab], 'BM'
        mov eax, [lam_an]
        add eax, 3
        and eax, ~3
        mov [bmp_fila], eax
        imul eax, [lam_al]
        mov [cab+34], eax           ; octetos de la imagen
        add eax, 1078
        mov [cab+2], eax            ; y del archivo
        mov dword [cab+10], 1078    ; dónde empieza la imagen
        mov dword [cab+14], 40
        mov eax, [lam_an]
        mov [cab+18], eax
        mov eax, [lam_al]
        mov [cab+22], eax
        mov word [cab+26], 1
        mov word [cab+28], 8
        mov dword [cab+38], 3780    ; 96 puntos por pulgada
        mov dword [cab+42], 3780
        mov esi, cab
        mov ecx, 54
        HV dos_escribir
        jc .mal
        mov esi, [hv_api + HV_pal_rgb * 4]  ; la paleta: azul, verde, rojo, 0
        mov edi, fila
        mov ecx, 256
.p:
        mov eax, [esi]
        add esi, 4
        mov edx, eax
        shr edx, 16
        mov [edi], dl
        mov [edi+1], ah
        mov [edi+2], al
        mov byte [edi+3], 0
        add edi, 4
        dec ecx
        jnz .p
        mov esi, fila
        mov ecx, 1024
        HV dos_escribir
        jc .mal
        cmp eax, ecx
        jb .mal
        mov edx, [lam_al]           ; las filas, de abajo arriba
.f:
        dec edx
        js .fin
        mov esi, edx
        imul esi, [lam_an]
        add esi, [lam_buf]
        mov edi, fila
        mov ecx, [lam_an]
        rep movsb
        xor eax, eax
        stosd
        mov esi, fila
        mov ecx, [bmp_fila]
        HV dos_escribir
        jc .mal
        cmp eax, ecx                ; (disco lleno)
        jb .mal
        jmp .f
.fin:
        HV dos_cerrar
        mov esi, t_exportado
        call aviso
.x:
        popad
        ret
.mal:
        HV dos_cerrar
        mov esi, nom_exp            ; (lo que haya quedado a medias, fuera)
        HV dos_borrar
.mal0:
        mov esi, t_no_export
        call aviso
        popad
        ret

; =============================================================================
;  Imprimir: en una impresora matricial (ESC/P, imagen de bits de 8 agujas)
;  por el puerto paralelo (LPT1) con la BIOS, INT 17h. Cada página se dibuja
;  y se manda por tiras de 8 puntos de alto: a 120 puntos por pulgada (ESC L)
;  o, en borrador, a 60 (ESC K), y 72 a lo alto. Los colores van tramados
;  según lo oscuros que son. Una página apaisada que no cabe a lo ancho (8
;  pulgadas) se imprime girada.
; =============================================================================
IMP_ANCHO   equ 768                 ; lo que cabe a lo ancho: 8 pulgadas
imprimir_b:
        mov byte [imp_borr], 1
        jmp imprime
imprimir:
        mov byte [imp_borr], 0
imprime:
        pushad
        call fin_edicion
        call imp_abre
        jc .mal
        xor edi, edi
        call imp_prepara
        xor ecx, ecx
.pag:
        cmp ecx, [lam_pags]
        jae .fin
        call lam_pinta
        cmp byte [lam_sin_mem], 0
        jne .sin
        call imp_pagina
        jc .mal
        inc ecx
        jmp .pag
.sin:
        call imp_cierra
        mov esi, t_sin_mem
        call aviso
        popad
        ret
.fin:
        call imp_lista
        jc .mal
        call imp_cierra
        mov esi, t_impreso
        call aviso
        popad
        ret
.mal:
        call imp_cierra
        mov esi, t_e_imprimir
        call aviso
        popad
        ret
; antes de imprimir: CF=1 si no hay impresora. Mientras se imprime no se
; pintan los textos de ejemplo de las cajas vacías
imp_abre:
        push eax
        push ebx
        mov eax, 0                  ; ¿hay puerto paralelo? (INT 11h)
        mov bl, 0x11
        call bios
        test ah, 0xC0
        jz .no
        mov eax, 0x0200             ; ¿y la impresora está encendida?
        mov bl, 0x17
        call bios
        test ah, 0x28
        jnz .no
        test ah, 0x10
        jz .no
        mov eax, [lam_guia_fn]
        mov [imp_guia], eax
        mov dword [lam_guia_fn], 0
        mov byte [imp_abierta], 1
        pop ebx
        pop eax
        clc
        ret
.no:
        pop ebx
        pop eax
        stc
        ret
; después de imprimir (haya ido bien o mal): todo como estaba
imp_cierra:
        cmp byte [imp_abierta], 0
        je .x
        push eax
        mov byte [imp_abierta], 0
        mov eax, [imp_guia]
        mov [lam_guia_fn], eax
        pop eax
.x:
        mov byte [lam_sucio], 1     ; (en lam_buf ha quedado lo último impreso)
        jmp inval

; imp_prepara: EDI=búfer ECX=ancho EDX=alto de lo que se va a imprimir (EDI=0:
; lam_buf, del tamaño de la lámina). Después, imp_pagina por cada hoja
; (lo oscuro que es cada color de la paleta y las medidas del papel)
imp_prepara:
        pushad
        mov esi, [hv_api + HV_pal_rgb * 4]
        xor ecx, ecx
.l:
        mov eax, [esi+ecx*4]        ; rojo, verde, azul
        movzx ebx, al
        imul ebx, 77
        movzx edx, ah
        imul edx, 150
        add ebx, edx
        shr eax, 16
        and eax, 0xFF
        imul eax, 29
        add eax, ebx
        shr eax, 8
        cmp eax, 56                 ; lo casi negro, negro del todo (la tinta
        jae .l1                     ; de las letras no se trama)
        xor eax, eax
.l1:
        cmp eax, 236                ; y lo casi blanco, blanco
        jb .l2
        mov eax, 255
.l2:
        mov [imp_luz+ecx], al
        inc ecx
        cmp ecx, 256
        jb .l
        mov edi, [esp]              ; (lo que se pidió al llamar)
        mov ecx, [esp+24]
        mov edx, [esp+20]
        test edi, edi               ; lo que se imprime: lam_buf, si no se dice
        jnz .otro
        mov edi, [lam_buf]
        mov ecx, [lam_an]
        mov edx, [lam_al]
.otro:
        mov [imp_src], edi
        mov [imp_an], ecx
        mov [imp_al], edx
        mov eax, ecx                ; ¿girada?
        mov ebx, edx
        mov byte [imp_gira], 0
        cmp eax, IMP_ANCHO
        jbe .g
        cmp eax, ebx
        jbe .g
        mov byte [imp_gira], 1
        xchg eax, ebx
.g:
        mov dword [imp_off], 0      ; EAX=ancho EBX=largo de lo que se imprime
        cmp eax, IMP_ANCHO
        jbe .a
        sub eax, IMP_ANCHO          ; no cabe: se centra, y se pierden las
        shr eax, 1                  ; orillas
        mov [imp_off], eax
        mov eax, IMP_ANCHO
.a:
        mov ecx, 120
        cmp byte [imp_borr], 0
        je .p
        mov ecx, 60
.p:
        mov [imp_ppp], ecx
        mul ecx
        mov ecx, 96
        div ecx
        mov [imp_ncol], eax
        lea eax, [ebx+ebx*2]
        shr eax, 2
        mov [imp_nfil], eax
        xor ecx, ecx                ; a qué punto de la página va cada columna
.c:
        mov eax, ecx
        mov ebx, 96
        mul ebx
        div dword [imp_ppp]
        add eax, [imp_off]
        mov [imp_cx+ecx*4], eax
        inc ecx
        cmp ecx, [imp_ncol]
        jbe .c
        popad
        ret

; imprime el búfer preparado, en una hoja; CF=1 si la impresora falla
imp_pagina:
        pushad
        mov esi, imp_ini            ; la impresora, como recién encendida, y
        mov ecx, 5                  ; el salto de línea, de 8/72 de pulgada
        call imp_varios
        jc .mal
        xor ebp, ebp                ; EBP=la primera fila de la tira
.tira:
        cmp ebp, [imp_nfil]
        jae .ff
        xor edi, edi                ; EDI=la columna
        mov dword [imp_n], 0        ; hasta dónde hay algo que imprimir
.col:
        cmp edi, [imp_ncol]
        jae .manda
        xor bl, bl                  ; las ocho agujas: la de arriba, el bit 7
        mov bh, 0x80
        mov edx, ebp
.aguja:
        cmp edx, [imp_nfil]
        jae .ag1
        call imp_punto
        jnc .ag1
        or bl, bh
.ag1:
        inc edx
        shr bh, 1
        jnz .aguja
        mov [imp_buf+edi], bl
        inc edi
        test bl, bl
        jz .col
        mov [imp_n], edi
        jmp .col
.manda:
        mov ecx, [imp_n]            ; (lo blanco del final de la tira no se manda)
        test ecx, ecx
        jz .salto
        mov al, 27
        call imp_car
        jc .mal
        mov al, 'L'
        cmp byte [imp_borr], 0
        je .m1
        mov al, 'K'
.m1:
        call imp_car
        jc .mal
        mov al, cl
        call imp_car
        jc .mal
        mov al, ch
        call imp_car
        jc .mal
        mov esi, imp_buf
        call imp_varios
        jc .mal
.salto:
        mov al, 13
        call imp_car
        jc .mal
        mov al, 10
        call imp_car
        jc .mal
        add ebp, 8
        jmp .tira
.ff:
        mov al, 12                  ; y la hoja, fuera
        call imp_car
        jc .mal
        popad
        clc
        ret
.mal:
        popad
        stc
        ret
imp_ini     db 27, '@', 27, 'A', 8

; (interna) ¿se imprime el punto de la columna EDI y la fila EDX del papel?
; CF=1 si sí: si lo más oscuro que le toca de la página pasa del tramado
imp_punto:
        pushad
        mov eax, edi                ; el umbral de este punto
        and eax, 3
        mov ebx, edx
        and ebx, 3
        movzx ebp, byte [tramado+ebx*4+eax]
        mov eax, edx                ; las filas de la página que le tocan
        shl eax, 2
        xor edx, edx
        mov ecx, 3
        div ecx
        mov esi, eax                ; ESI=la primera
        lea ebx, [esi+1]            ; EBX=la siguiente a la última
        cmp edx, 2
        jne .f
        inc ebx                     ; (cada tres filas del papel, cuatro de la página)
.f:
        mov ecx, [imp_cx+edi*4]     ; y las columnas
        mov edx, [imp_cx+edi*4+4]
        cmp edx, ecx
        ja .y
        lea edx, [ecx+1]
.y:
        mov edi, ecx
.x:
        mov eax, edi
        push ebx
        mov ebx, esi
        call imp_luz_de             ; AL=lo claro que es ese punto
        pop ebx
        movzx eax, al
        cmp eax, ebp
        jb .si
        inc edi
        cmp edi, edx
        jb .x
        inc esi
        cmp esi, ebx
        jb .y
        popad
        clc
        ret
.si:
        popad
        stc
        ret
; (interna) EAX=x EBX=y de lo que se imprime -> AL=lo claro que es (255 fuera
; de la página). Girada, la x del papel es la y de la página, de abajo arriba
imp_luz_de:
        cmp byte [imp_gira], 0
        je .d
        xchg eax, ebx
        neg ebx
        add ebx, [imp_al]
        dec ebx
.d:
        cmp eax, [imp_an]
        jae .fuera
        cmp ebx, [imp_al]
        jae .fuera
        imul ebx, [imp_an]
        add ebx, [imp_src]
        movzx eax, byte [ebx+eax]
        mov al, [imp_luz+eax]
        ret
.fuera:
        mov al, 255
        ret

; (interna) imprime ECX octetos de ESI; CF=1 si la impresora falla
imp_varios:
        push eax
        push ecx
        push esi
.b:
        lodsb
        call imp_car
        jc .x
        dec ecx
        jnz .b
        clc
.x:
        pop esi
        pop ecx
        pop eax
        ret
; imprime el carácter AL; CF=1 si la impresora falla. Conserva los registros
imp_car:
        push eax
        push ebx
        call imp_lista
        jc .x
        movzx eax, al
        mov bl, 0x17
        call bios
        test ah, 0x28               ; error o sin papel
        jz .x                       ; (CF=0)
        stc
.x:
        pop ebx
        pop eax
        ret
; espera a que la impresora admita otro carácter, hasta veinte segundos (una
; de verdad se para a ratos, con la memoria llena); CF=1 si no contesta
imp_lista:
        push eax
        push ebx
        push edx
        HV tics
        mov edx, eax
.l:
        mov eax, 0x0200
        mov bl, 0x17
        call bios
        test ah, 0x28
        jnz .no
        test ah, 0x80               ; no está ocupada
        jnz .si
        HV tics
        sub eax, edx
        cmp eax, 20 * 18
        jb .l
.no:
        stc
        jmp .x
.si:
        clc
.x:
        pop edx
        pop ebx
        pop eax
        ret
; la interrupción BL de la BIOS con EAX y DX=0 (LPT1) -> EAX
bios:
        push ecx
        push esi
        push edi
        push eax
        mov edi, rl
        mov ecx, RL_TAM / 4
        xor eax, eax
        rep stosd
        pop eax
        mov [rl+RL_EAX], eax
        mov [rl+RL_INT], bl
        mov esi, rl
        HV rm_interrup
        mov eax, [rl+RL_EAX]
        pop edi
        pop esi
        pop ecx
        ret

; =============================================================================
;  Pintar
; =============================================================================
; (interna) la página, dibujada y reducida, si ha cambiado
al_dia:
        pushad
        ; la caja en la que se escribe no enseña su texto de ejemplo: si ha
        ; cambiado cuál es, la lámina se dibuja otra vez
        xor esi, esi
        cmp byte [editando], 0
        je .g
        mov eax, [sel]
        test eax, eax
        js .g
        call lam_ptr
.g:
        cmp esi, [lam_guia_salta]
        je .g1
        mov [lam_guia_salta], esi
        mov byte [lam_sucio], 1
.g1:
        cmp byte [lam_sucio], 0
        je .e
        mov ecx, [pag]
        call lam_pinta
        mov byte [lam_sucio], 0
        mov byte [esc_sucio], 1
.e:
        cmp dword [zoom], 256
        je .x
        cmp byte [esc_sucio], 0
        je .x
        mov ecx, [pw]               ; sitio para la lámina reducida (se pide al usarlo)
        imul ecx, [ph]
        mov ebx, esc_buf
        xor al, al
        call lam_bloque
        jnc .red
        mov byte [esc_falta], 1
        jmp .x
.red:
        mov byte [esc_falta], 0
        mov edi, [esc_buf]
        mov ecx, [pw]
        mov edx, [ph]
        mov ebx, [suave]
        call lam_reduce
        mov byte [esc_sucio], 0
.x:
        popad
        ret

; la vista de la lámina, en su zona de la ventana: la lámina (dibujada y
; reducida si hace falta), lo que la rodea y lo que va por encima (guías, lo
; elegido, lo que se arrastra). Con el contexto abierto; lo estrecha a la
; vista, así que es lo último que se pinta
pinta_vista:
        pushad
        COLOR C_ARENA               ; la esquina entre las dos barras
        mov eax, [vx0]
        add eax, [vw]
        mov ebx, [vy0]
        add ebx, [vh]
        mov ecx, BD_AN
        mov edx, BD_AN
        HV dc_rect
        call al_dia
        mov eax, [vx0]
        mov ebx, [vy0]
        mov ecx, [vw]
        mov edx, [vh]
        HV dc_cortar
        ; lo que no es lámina, en crema: por arriba, por abajo y a los lados
        COLOR C_CREMA
        mov edx, [wy0]
        sub edx, ebx
        jle .f1
        HV dc_rect
.f1:
        mov ebx, [wy0]
        add ebx, [ph]
        mov edx, [vy0]
        add edx, [vh]
        sub edx, ebx
        jle .f2
        HV dc_rect
.f2:
        mov ebx, [wy0]
        mov edx, [ph]
        mov ecx, [wx0]
        sub ecx, eax
        jle .f3
        HV dc_rect
.f3:
        mov eax, [wx0]
        add eax, [pw]
        mov ecx, [vx0]
        add ecx, [vw]
        sub ecx, eax
        jle .f4
        HV dc_rect
.f4:
        COLOR C_ARENA_OSC           ; el filo del papel
        mov eax, [wx0]
        dec eax
        mov ebx, [wy0]
        dec ebx
        mov ecx, [pw]
        add ecx, 2
        mov edx, [ph]
        add edx, 2
        HV dc_marco
        mov eax, [wx0]
        mov ebx, [wy0]
        mov ecx, [pw]
        mov edx, [ph]
        cmp dword [zoom], 256       ; sin memoria para dibujarla (o para
        je .m1                      ; reducirla): se dice ahí mismo
        cmp byte [esc_falta], 0
        jne .m2
.m1:
        cmp byte [lam_sin_mem], 0
        je .hay
.m2:
        COLOR C_BLANCO
        HV dc_rect
        COLOR C_GRANATE
        mov eax, [vx0]
        add eax, 12
        mov ebx, [vy0]
        add ebx, 12
        mov esi, t_sin_mem_v
        HV dc_texto
        popad
        ret
.hay:
        mov esi, [lam_buf]          ; la lámina
        cmp dword [zoom], 256
        je .p
        mov esi, [esc_buf]
.p:
        HV dc_copiar
        call pinta_guias
        call pinta_sel
        call pinta_arrastre
        popad
        ret

; ---- las guías: los márgenes y la rejilla (no se imprimen) ----
pinta_guias:
        pushad
        cmp byte [margen_ver], 0
        je .rej
        mov eax, [lam_margen]
        test eax, eax
        jz .rej
        mov ebx, eax
        mov ecx, [lam_an]
        sub ecx, eax
        sub ecx, eax
        mov edx, [lam_al]
        sub edx, eax
        sub edx, eax
        call rect_v
        COLOR C_TURQUESA
        mov eax, [sr_x0]
        mov ebx, [sr_y0]
        mov ecx, [sr_x1]
        sub ecx, eax
        mov edx, [sr_y1]
        sub edx, ebx
        HV dc_marco
.rej:
        cmp byte [rejilla_ver], 0
        je .x
        ; un punto cada 5 mm (cada 10, si la página se ve pequeña); solo los
        ; que caen en la vista
        COLOR C_ARENA_OSC
        mov ebp, 2400
        cmp dword [zoom], 128
        jae .r0
        mov ebp, 4800
.r0:
        xor edi, edi                ; EDI=y, en 127 avos de punto de lámina
.fy:
        mov eax, edi
        add eax, 63
        xor edx, edx
        mov ecx, 127
        div ecx
        cmp eax, [lam_al]
        jae .x
        mov ebx, eax
        xor eax, eax
        call a_ventana
        cmp ebx, [vy0]
        jl .sy
        mov eax, [vh]
        add eax, [vy0]
        cmp ebx, eax
        jge .x
        mov [g_wy], ebx
        xor esi, esi                ; ESI=x
.fx:
        mov eax, esi
        add eax, 63
        xor edx, edx
        mov ecx, 127
        div ecx
        cmp eax, [lam_an]
        jae .sy
        xor ebx, ebx
        call a_ventana
        cmp eax, [vx0]
        jl .sx
        mov ebx, [vx0]
        add ebx, [vw]
        cmp eax, ebx
        jge .sy
        mov ebx, [g_wy]
        HV dc_punto
.sx:
        add esi, ebp
        jmp .fx
.sy:
        add edi, ebp
        jmp .fy
.x:
        popad
        ret

; -> CF=1 si lo elegido es una caja de texto y su texto no cabe en ella (lo
; que sobra por abajo no se ve ni se imprime)
texto_sobra:
        push eax
        push esi
        mov eax, [sel]
        test eax, eax
        js .no
        call lam_ptr
        cmp byte [esi+LO_TIPO], LT_TEXTO
        jne .no
        cmp dword [esi+LO_LARGO], 0
        je .no
        call lam_compone
        mov eax, [lam_l_n]
        mov eax, [lam_l_y+eax*4]
        add eax, LAM_SEP
        cmp eax, [esi+LO_AL]
        jle .no
        pop esi
        pop eax
        stc
        ret
.no:
        pop esi
        pop eax
        clc
        ret

; ---- lo elegido: su contorno y sus tiradores; escribiendo, el cursor ----
pinta_sel:
        pushad
        call sel_rect
        jc .x
        cmp byte [sr_linea], 0
        jne .tiradores
        COLOR C_PETROLEO
        mov eax, [sr_x0]
        dec eax
        mov ebx, [sr_y0]
        dec ebx
        mov ecx, [sr_x1]
        sub ecx, eax
        inc ecx
        mov edx, [sr_y1]
        sub edx, ebx
        inc edx
        HV dc_marco
        call texto_sobra            ; una caja con más texto del que cabe: una
        jnc .cabe                   ; marca granate abajo a la derecha
        COLOR C_GRANATE
        mov eax, [sr_x1]
        sub eax, 12
        mov ebx, [sr_y1]
        sub ebx, 12
        mov ecx, 9
        mov edx, 9
        HV dc_rect
        COLOR C_BLANCO
        add eax, 2
        add ebx, 4
        mov ecx, 5
        mov edx, 1
        HV dc_rect
        add eax, 2
        sub ebx, 2
        mov ecx, 1
        mov edx, 5
        HV dc_rect
.cabe:
        cmp byte [editando], 0
        jne .cursor
.tiradores:
        mov edi, 8
        cmp byte [sr_linea], 0
        je .t0
        mov edi, 2
.t0:
        xor ecx, ecx
.t:
        push ecx
        call tirador_xy
        sub eax, 3
        sub ebx, 3
        mov ecx, 7
        mov edx, 7
        COLOR C_TINTA
        HV dc_rect
        add eax, 2
        add ebx, 2
        mov ecx, 3
        mov edx, 3
        COLOR C_BLANCO
        HV dc_rect
        pop ecx
        inc ecx
        cmp ecx, edi
        jb .t
        jmp .x
.cursor:
        cmp byte [foco], 0
        je .x
        call ed_ficha
        mov eax, [cur]
        call lam_sitio_xy           ; EAX=x EBX=y ECX=alto, desde la esquina
        add eax, [esi+LO_X]
        add ebx, [esi+LO_Y]
        mov edx, ecx
        imul edx, [zoom]
        shr edx, 8
        cmp edx, 3
        jae .c1
        mov edx, 3
.c1:
        call a_ventana
        mov ecx, 2
        COLOR C_TINTA
        HV dc_rect
.x:
        popad
        ret

; ---- el contorno de lo que se arrastra ----
pinta_arrastre:
        pushad
        mov al, [drag]
        test al, al
        jz .x
        cmp al, D_CREAR
        je .crear
        cmp al, D_MOVER
        jne .obj
        cmp byte [dr_movido], 0
        je .x
.obj:
        mov eax, [sel]
        test eax, eax
        js .x
        call lam_ptr
        cmp byte [esi+LO_TIPO], LT_LINEA
        je .linea
        jmp .caja
.crear:
        cmp dword [herr], 4
        je .linea
.caja:
        mov eax, [dr_x]
        mov ebx, [dr_y]
        mov ecx, [dr_an]
        mov edx, [dr_al]
        call rect_v
        COLOR C_GRANATE
        mov eax, [sr_x0]
        mov ebx, [sr_y0]
        mov ecx, [sr_x1]
        sub ecx, eax
        mov edx, [sr_y1]
        sub edx, ebx
        HV dc_marco
        jmp .x
.linea:
        mov eax, [dr_x]
        mov ebx, [dr_y]
        mov ecx, [dr_an]
        mov edx, [dr_al]
        call rect_v
        COLOR C_GRANATE
        mov eax, [sr_x0]
        mov ebx, [sr_y0]
        mov ecx, [sr_x1]
        mov edx, [sr_y1]
        HV dc_linea
.x:
        popad
        ret

; ---- la barra de botones ----
pinta_botones:
        pushad
        mov ecx, [cli_an]
        mov edx, TB_AL
        call br_fondo
        call clase
        mov [act_clase], eax
        mov edi, t_vista_b          ; el botón de la vista: «Entera» o «100 %»
        mov esi, t_entera
        cmp dword [vista], 0
        je .v
        mov eax, [zoom]
        imul eax, 100
        add eax, 128
        shr eax, 8
        call hvl_num
        mov esi, t_pct
.v:
        call hvl_cad
        mov byte [edi], 0
        mov edi, t_grosor_b         ; y el del grosor
        mov eax, [act_clase]
        movzx eax, byte [act_grosor+eax]
        call hvl_num
        mov byte [edi], 0
        mov edi, botones1
        mov ebx, 4
        call br_fila
        mov edi, botones            ; y la fila de abajo
        mov ebx, 32
        call br_fila
        COLOR C_TINTA               ; los rótulos de las muestras de color
        mov ebx, 35
        mov eax, BX_LETRA - 44
        mov esi, t_r_letra
        HV dc_texto
        mov eax, BX_FONDO - 44
        mov esi, t_r_fondo
        HV dc_texto
        mov eax, BX_BORDE - 44
        mov esi, t_r_borde
        HV dc_texto
        ; «sin color»: la muestra, en blanco y cruzada
        mov ecx, [act_clase]
        mov eax, BX_FONDO
        cmp byte [act_fondo+ecx], LAM_NADA
        jne .n1
        call .cruz
.n1:
        mov eax, BX_BORDE
        cmp byte [act_borde+ecx], LAM_NADA
        jne .n2
        call .cruz
.n2:
        popad
        ret
.cruz:
        pushad
        COLOR C_GRANATE
        add eax, 3
        mov ebx, 32 + BR_AL - 4
        lea ecx, [eax+16]
        mov edx, 32 + 3
        HV dc_linea
        inc eax
        inc ecx
        HV dc_linea
        popad
        ret
; lo que pregunta la barra común de cada botón (EDI): EBP=1 si está puesto
; ahora; y el texto o el color de los que lo cambian (ESI)
barra_estado:
        xor ebp, ebp
        mov eax, [edi+12]
        mov ecx, [act_clase]
        cmp eax, O_TIPO             ; el tipo de letra: el nombre del de ahora
        jne .n1
        movzx eax, byte [act_letra]
        jmp tl_nombre
.n1:
        cmp eax, O_TAM              ; el tamaño, en puntos
        jne .n2
        movzx edx, byte [act_cuerpo]
        jmp tl_puntos
.n2:
        cmp eax, O_CAMBIA_V
        jne .n3
        mov esi, t_vista_b
        ret
.n3:
        cmp eax, O_GROSOR
        jne .n4
        mov esi, t_grosor_b
        ret
.n4:
        cmp eax, O_C_LETRA
        jne .n5
        movzx esi, byte [act_tinta]
        ret
.n5:
        cmp eax, O_C_FONDO
        jne .n6
        movzx esi, byte [act_fondo+ecx]
        jmp .color
.n6:
        cmp eax, O_C_BORDE
        jne .n7
        movzx esi, byte [act_borde+ecx]
.color:
        cmp esi, LAM_NADA
        jne .c1
        mov esi, C_BLANCO
.c1:
        ret
.n7:
        mov ecx, eax                ; las herramientas
        sub ecx, O_HERR
        cmp ecx, 4
        ja .n8
        cmp ecx, [herr]
        je .si
        ret
.n8:
        mov ecx, eax                ; la alineación
        sub ecx, O_IZQ
        cmp ecx, 2
        ja .n9
        cmp cl, [act_alin]
        je .si
        ret
.n9:
        mov cl, LE_NEG
        cmp eax, O_NEG
        je .bit
        mov cl, LE_CUR
        cmp eax, O_CUR
        je .bit
        mov cl, LE_SUB
        cmp eax, O_SUB
        je .bit
        cmp eax, O_VINETAS
        jne barra_propia            ; (del programa) sus botones
        mov eax, [sel]              ; las viñetas: las de la caja elegida
        test eax, eax
        js .no
        push esi
        call lam_ptr
        test byte [esi+LO_BAND], LB_VINETAS
        pop esi
        jnz .si
        ret
.bit:
        test [act_estilo], cl
        jz .no
.si:
        inc ebp
.no:
        ret

; ---- el pie: la página y, de lo elegido (o de lo que se arrastra), qué es,
;      dónde está y cuánto mide, en milímetros; si no hay nada elegido, el
;      papel y la vista (o, con el ratón sobre un botón, lo que hace) ----
pinta_estado:
        pushad
        mov ecx, [cli_an]
        mov edx, [cli_al]
        call br_pie                 ; (CF=1: ha puesto el nombre de un botón)
        jnc .donde
        popad
        ret
.donde:
        mov edi, linea
        mov esi, t_pagina
        call hvl_cad
        mov eax, [pag]
        inc eax
        call hvl_num
        mov esi, t_de
        call hvl_cad
        mov eax, [lam_pags]
        call hvl_num
        mov esi, t_sep
        call hvl_cad
        mov eax, [herr]             ; con una herramienta: qué hay que hacer
        test eax, eax
        jz .sin_herr
        cmp byte [drag], D_CREAR
        je .medidas_dr
        mov esi, [t_objetos+eax*4]
        call hvl_cad
        mov esi, t_arrastre
        call hvl_cad
        jmp .fin
.medidas_dr:
        mov esi, [t_objetos+eax*4]
        call hvl_cad
        jmp .dr
.sin_herr:
        mov eax, [sel]
        test eax, eax
        js .papel
        call lam_ptr
        mov ebx, esi
        movzx eax, byte [ebx+LO_TIPO]
        mov esi, [t_objetos+eax*4]
        call hvl_cad
        cmp byte [editando], 0
        je .medidas
        mov esi, t_escriba
        call hvl_cad
        jmp .fin
.medidas:
        cmp byte [drag], 0
        jne .dr
        mov eax, [ebx+LO_X]
        mov [p_x], eax
        mov eax, [ebx+LO_Y]
        mov [p_y], eax
        mov eax, [ebx+LO_AN]
        mov [p_an], eax
        mov eax, [ebx+LO_AL]
        mov [p_al], eax
        jmp .mm
.dr:
        mov eax, [dr_x]
        mov [p_x], eax
        mov eax, [dr_y]
        mov [p_y], eax
        mov eax, [dr_an]
        mov [p_an], eax
        mov eax, [dr_al]
        mov [p_al], eax
.mm:
        mov esi, t_x
        call hvl_cad
        mov eax, [p_x]
        call num_mm
        mov esi, t_y
        call hvl_cad
        mov eax, [p_y]
        call num_mm
        mov esi, t_mm_pc
        call hvl_cad
        mov eax, [p_an]
        call num_mm
        mov esi, t_por
        call hvl_cad
        mov eax, [p_al]
        call num_mm
        mov esi, t_mm
        call hvl_cad
        call texto_sobra
        jnc .fin
        mov esi, t_no_cabe
        call hvl_cad
        jmp .fin
.papel:
        call pie_papel              ; (del programa) el papel, el tema...
        mov esi, t_sep
        call hvl_cad
        mov esi, t_vista
        call hvl_cad
        mov eax, [zoom]
        imul eax, 100
        add eax, 128
        shr eax, 8
        call hvl_num
        mov esi, t_pct
        call hvl_cad
.fin:
        cmp byte [ajustar], 0
        je .f1
        mov esi, t_ajustado
        call hvl_cad
.f1:
        mov byte [edi], 0
        COLOR C_TINTA
        mov eax, 8
        mov ebx, [cli_al]
        sub ebx, BR_PIE - 3
        mov esi, linea
        HV dc_texto
        popad
        ret
; escribe en EDI (que avanza) EAX, puntos de lámina, en milímetros, con su signo
num_mm:
        call a_mm
        test eax, eax
        jns .n
        mov byte [edi], '-'
        inc edi
        neg eax
.n:
        jmp hvl_num


; ---- Formato > Viñetas: la caja elegida pasa a ser una lista (cada párrafo,
;      con su punto), o deja de serlo ----
vinetas:
        mov eax, [sel]
        test eax, eax
        js .x
        call lam_ptr
        cmp byte [esi+LO_TIPO], LT_TEXTO
        jne .x
        call instantanea
        xor byte [esi+LO_BAND], LB_VINETAS
        call cambio
.x:
        ret

%macro LAMINA_ED_BSS 0
ven         resd 1
puente      resd 1                  ; el cuadro de texto escondido del portapapeles
barra_v     resd 1                  ; las barras de desplazamiento
barra_h     resd 1
param       resd 1
cli_an      resd 1
cli_al      resd 1
vx0         resd 1                  ; dónde está la vista en la ventana
vy0         resd 1
vw          resd 1                  ; ancho y alto de la vista (donde va la página)
vh          resd 1
vista       resd 1                  ; 0 página entera, 1 al 50 %, 2 al 75 %, 3 al 100 %
zoom        resd 1                  ; a cuánto se ve: 256 es el 100 %
zoom_ant    resd 1
suave       resd 1                  ; 1: la página reducida se suaviza
pw          resd 1                  ; lo que mide la página en la ventana
ph          resd 1
wx0         resd 1                  ; dónde cae su esquina en la ventana
wy0         resd 1
sx          resd 1                  ; lo que está desplazada
sy          resd 1
sx_max      resd 1
sy_max      resd 1
pag         resd 1                  ; la página que se ve
sel         resd 1                  ; el objeto elegido (-1: ninguno)
herr        resd 1                  ; la herramienta: 0 elegir, 1 texto, 2 rectángulo...
cur         resd 1                  ; escribiendo: el cursor en el texto de la caja
m_wx        resd 1                  ; el ratón al apretar, en la ventana
m_wy        resd 1
m_wx2       resd 1
m_wy2       resd 1
m_ux        resd 1                  ; y en la página
m_uy        resd 1
m_dx        resd 1                  ; de dónde se ha cogido el objeto
m_dy        resd 1
dr_x        resd 1                  ; lo que se arrastra: dónde quedaría
dr_y        resd 1
dr_an       resd 1
dr_al       resd 1
dr_ax       resd 1                  ; donde se empezó un objeto nuevo
dr_ay       resd 1
dr_h        resd 1                  ; el tirador
or_x        resd 1                  ; el objeto, como estaba al empezar
or_y        resd 1
or_an       resd 1
or_al       resd 1
sr_x0       resd 1                  ; las esquinas de un objeto en la ventana
sr_y0       resd 1
sr_x1       resd 1
sr_y1       resd 1
g_wy        resd 1
p_x         resd 1
p_y         resd 1
p_an        resd 1
p_al        resd 1
act_clase   resd 1
le_titulo   resd 1
le_n        resd 1
le_texto    resd 1
le_sel      resd 1
cur_forma_act resd 1                ; la forma que tiene ahora el cursor
lista_orig  resd 1                  ; el procedimiento de las listas de Hispa Ventanas
dl_plant    resd 1
dl_papel    resd 1
des_sel     resd 1
des_pag     resd 1
bmp_fila    resd 1
imp_ppp     resd 1                  ; la impresora: puntos por pulgada a lo ancho,
imp_ncol    resd 1                  ; columnas y filas de puntos de una página,
imp_nfil    resd 1
imp_off     resd 1                  ; lo que se pierde por la izquierda
imp_n       resd 1
imp_src     resd 1                  ; lo que se imprime: el búfer y sus medidas
imp_an      resd 1
imp_al      resd 1
imp_guia    resd 1
act_letra   resb 1                  ; el formato de ahora: el de lo elegido, y
act_cuerpo  resb 1                  ; el que llevará lo que se ponga
act_estilo  resb 1
act_alin    resb 1
act_tinta   resb 1
act_fondo   resb 2                  ; de las cajas de texto y de las formas
act_borde   resb 2
act_grosor  resb 2
sr_linea    resb 1
drag        resb 1                  ; lo que se hace con el ratón apretado (D_*)
dr_movido   resb 1
editando    resb 1                  ; se está escribiendo en la caja elegida
modif       resb 1
foco        resb 1
lam_sucio   resb 1                  ; hay que dibujar la página otra vez
esc_sucio   resb 1                  ; y reducirla
rejilla_ver resb 1
margen_ver  resb 1
ajustar     resb 1                  ; las medidas se ajustan a la rejilla
tecleando   resb 1                  ; en una tanda de teclas seguidas
moviendo    resb 1                  ; o de flechas
des_hay     resb 1                  ; hay algo que deshacer
des_hecho   resb 1                  ; 1: lo último fue deshacer (se puede rehacer)
clip_hay    resb 1
ed_car      resb 1
pl_peq      resb 1
imp_borr    resb 1
imp_gira    resb 1
esc_falta   resb 1                  ; no ha habido memoria para la lámina reducida
imp_abierta resb 1
ed_todo     resb 1                  ; ha cambiado más de una lámina
pl_pag      resb 1
            alignb 4
rl          resb RL_TAM
cab         resb 56
titulo      resb 112
nombre      resb 96
nom_tmp     resb 96
nom_exp     resb 100
nom_seg     resb 100                ; el temporal en el que se guarda
num_txt     resb 12
t_vista_b   resb 12
t_grosor_b  resb 8
linea       resb 200
clip_ficha  resb LO_TAM
imp_luz     resb 256
            alignb 4
imp_cx      resd 1024
imp_buf     resb 1024
fila        resb LAM_MAX_LADO + 1024 + 8
pega_txt    resb TXT_MAX + 4
des_doc     resb LAM_DOC_TAM        ; el documento de antes del último cambio
; lo grande no está aquí: son bloques que se piden al usarlos (lam_bloque:
; puntero, lo que cabe y mango) y se devuelven al cerrar (ed_suelta)
des_dat     resd 3                  ; los datos del documento de antes del último cambio
clip_dat    resd 3                  ; los datos del objeto copiado
esc_buf     resd 3                  ; la lámina, reducida
%endmacro
