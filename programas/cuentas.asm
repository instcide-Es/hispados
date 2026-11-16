; =============================================================================
;  CUENTAS.HVP - Cuentas, la hoja de cálculo de Hispa Ventanas
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Una hoja de 64 columnas (A, B... Z, AA... BL) por 1.000 filas, con el
;  aspecto y el manejo de la familia de oficina (docs/OFICINA.md): barra de
;  botones de dos filas y pie comunes (barra.inc), los mismos menús y los
;  mismos atajos que Pluma.
;
;  La ventana: la barra de botones; la barra de fórmulas (la celda activa y
;  su contenido tal como se tecleó); la rejilla, que ocupa todo lo demás y
;  crece con la ventana, con sus rótulos y sus dos barras de desplazamiento;
;  y el pie, que dice lo que hace el botón que hay bajo el ratón o, si no, lo
;  elegido (de un rango: su suma, su promedio y cuántos números tiene).
;
;  Moverse y elegir: flechas, Tab, Intro, Inicio, Fin, Re Pág, Av Pág,
;  Ctrl+Inicio y Ctrl+Fin; con Mayús se elige un rango; con el ratón, pulsando
;  y arrastrando. Pulsar el rótulo de una columna o de una fila la elige
;  entera (arrastrando, varias); la esquina o Ctrl+E, toda la hoja. El borde
;  derecho del rótulo de una columna se arrastra para cambiarle el ancho.
;
;  Escribir: al teclear se empieza una celda nueva; F2 (o doble clic, o
;  pulsar la barra de fórmulas) corrige la que hay, con cursor. Intro acepta
;  y baja, Tab acepta y va a la derecha, Esc deja la celda como estaba.
;    - texto:   lo que no es número ni empieza por «=»; a la izquierda; si no
;               cabe, sigue sobre las celdas vacías de su derecha
;    - número:  a la derecha (con coma o con punto decimal); una fecha
;               (16/11/2026) es el número de su día y toma formato de fecha
;    - fórmula: empieza por «=» (ver cuentas/calculo.inc: operadores,
;               comparaciones, textos, referencias con «$» y las funciones
;               SUMA, PROMEDIO, MAX, MIN, CONTAR, CONTARA, SI, Y, O, NO,
;               ENTERO, REDONDEAR, RAIZ, POTENCIA, RESIDUO, ABS, CONTAR.SI,
;               SUMAR.SI, HOY, CONCATENAR, LARGO, MAYUSC y MINUSC). Errores:
;               «#ERROR», «#CIRC» (se pide a sí misma) y «#REF» (una celda
;               que no existe). Se calcula todo cada vez que se pinta.
;  Las cuentas son de coma fija, con ocho decimales exactos (0,1 + 0,2 es
;  0,3), y un número nunca se enseña cortado: si no cabe, almohadillas.
;
;  Edición: deshacer y rehacer (un paso), cortar, copiar y pegar rangos
;  (corriendo las referencias de las fórmulas; lo copiado va también, como
;  texto, al portapapeles de los demás programas), rellenar hacia abajo y
;  hacia la derecha, borrar, insertar y eliminar filas y columnas (corrigiendo
;  las fórmulas), buscar e ir a una celda (cuentas/hoja.inc).
;  Formato de cada celda: negrita, cursiva, alineación, color de la letra y
;  formato del número (con decimales fijos, con millares, moneda, porcentaje
;  o fecha); ancho de cada columna. Herramientas: ordenar las filas de un
;  rango por una columna, autosuma y gráficos (cuentas/grafico.inc).
;
;  Archivos: el formato propio, .HOJ, lo guarda todo; .CSV, para llevar y
;  traer datos (cuentas/archivo.inc). Imprime como texto por LPT1.
;
;  Es un módulo .HVP (ventanas/hvp.inc). Dibuja la rejilla él mismo.
;  Parámetros del acceso directo: el nombre de un archivo, que se abre al
;  entrar.
; =============================================================================
%include "hvp.inc"
%define CON_GRAFICOS                ; (sin esto, Cuentas sale sin los gráficos)

; ---- la hoja ----
NCOL        equ 64                  ; columnas: de la A a la BL
NFIL        equ 1000                ; filas
NCEL        equ NCOL * NFIL
CEL_MAX     equ 128                 ; octetos del contenido de una celda (con el 0)
; ---- medidas, en puntos ----
TB_AL       equ 58                  ; alto de la barra de botones (dos filas)
BARRA       equ 22                  ; alto de la barra de fórmulas
REF_AN      equ 60                  ; ancho del sitio de la referencia («B7»)
ROT_AN      equ 36                  ; ancho de los rótulos de las filas
ROT_AL      equ 18                  ; alto de los rótulos de las columnas
CEL_AL      equ 18                  ; alto de una fila
HY0         equ TB_AL + BARRA       ; y de los rótulos de las columnas
GY0         equ HY0 + ROT_AL        ; y de la primera fila de celdas
VEN_X       equ 60                  ; la ventana, al abrirse
VEN_Y       equ 36
VEN_AN      equ 660
VEN_AL      equ 520
ID_PUENTE   equ 900                 ; el cuadro de texto escondido (portapapeles)
ID_BARRA_V  equ 901
ID_BARRA_H  equ 902
; ---- órdenes ----
O_NUEVO     equ 101
O_ABRIR     equ 102
O_GUARDAR   equ 103
O_COMO      equ 104
O_IMPRIMIR  equ 105
O_SALIR     equ 106
O_DESHACER  equ 110
O_REHACER   equ 111
O_CORTAR    equ 112
O_COPIAR    equ 113
O_PEGAR     equ 114
O_BORRAR    equ 115
O_REL_ABAJO equ 116
O_REL_DER   equ 117
O_TODO      equ 118
O_BUSCAR    equ 119
O_SIGUIENTE equ 120
O_IRA       equ 121
O_ELI_FILA  equ 122
O_ELI_COL   equ 123
O_INS_FILA  equ 130
O_INS_COL   equ 131
O_AUTOSUMA  equ 132
O_FECHA     equ 133
O_CELDAS    equ 140
O_NEG       equ 141
O_CUR       equ 142
O_IZQ       equ 143                 ; (las tres alineaciones, seguidas)
O_CEN       equ 144
O_DER       equ 145
O_MONEDA    equ 146
O_PORC      equ 147
O_DEC_MAS   equ 148
O_DEC_MENOS equ 149
O_ANCHO     equ 150
O_SIMBOLO   equ 151
O_COLOR     equ 152                 ; 152-155: negro, rojo, azul, verde
O_ORD_ASC   equ 160
O_ORD_DESC  equ 161
O_GRAFICO   equ 162                 ; 162-164: barras, líneas, sectores
O_ACERCA    equ 170

HVP_CABECERA "Cuentas", entrada, 4
        db "................................"
        db "................................"
        db "................................"
        db "..0000000000000000000000000000.."
        db "..0eeeeeeeeeeeeeeeeeeeeeeeeee0.."
        db "..0eeeeeeeeeeeeeeeeeeeeeeeeee0.."
        db "..0777777777777777777777777770.."
        db "..0ee9999979999979999979999970.."
        db "..0ee9999979999979999979999970.."
        db "..0ee9999979999979999979999970.."
        db "..0ee9999979999979999979999970.."
        db "..0777777777777777777777777770.."
        db "..0ee9999979999979999979999970.."
        db "..0ee9999979999979999979999970.."
        db "..0ee9999979999979999979999970.."
        db "..0ee9999979999979999979999970.."
        db "..0777777777777777777777777770.."
        db "..0ee9999979999979999979999970.."
        db "..0ee9999979999979999979999970.."
        db "..0ee9999979999979999979999970.."
        db "..0ee9999979999979999979999970.."
        db "..0777777777777777777777777770.."
        db "..0ee9999979999979999979999970.."
        db "..0ee9999979999979999979999970.."
        db "..0ee9999979999979999979999970.."
        db "..0ee9999979999979999979999970.."
        db "..0777777777777777777777777770.."
        db "..0ee9999979999979999979999970.."
        db "..0000000000000000000000000000.."
        db "................................"
        db "................................"
        db "................................"
HVP_CODIGO

; ---- la clase de la ventana, su plantilla y sus hijas ----
c_cuentas   dd cue_proc
            db SIN_FONDO, 0, 0, 0   ; lo pinta todo ella: sin parpadeo
cue_plant:
        PLANTILLA c_cuentas, titulo, E_NORMAL, VEN_X, VEN_Y, VEN_AN, VEN_AL, 0
hijos:
        PLANTILLA HV_c_edit, 0, E_HIJA | E_OCULTA | ES_MULTI, 0, 0, 40, 40, ID_PUENTE
        PLANTILLA HV_c_barra, 0, E_HIJA, 0, 0, BD_AN, 50, ID_BARRA_V
        PLANTILLA HV_c_barra, 0, E_HIJA | BDS_HORIZ, 0, 0, 50, BD_AN, ID_BARRA_H
        dd 0

; ---- la barra de menús ----
cue_menu:
        db "&Archivo", 0
        dw O_NUEVO
        db "&Nuevo                 Ctrl+U", 0
        dw O_ABRIR
        db "&Abrir...              Ctrl+A", 0
        dw O_GUARDAR
        db "&Guardar               Ctrl+G", 0
        dw O_COMO
        db "Guardar &como...", 0
        dw 0
        db 0
        dw O_IMPRIMIR
        db "&Imprimir              Ctrl+P", 0
        dw 0
        db 0
        dw O_SALIR
        db "&Salir", 0
        dw 0xFFFF
        db "&Edición", 0
        dw O_DESHACER
        db "&Deshacer                   Ctrl+Z", 0
        dw O_REHACER
        db "&Rehacer                    Ctrl+Y", 0
        dw 0
        db 0
        dw O_CORTAR
        db "Cor&tar                     Ctrl+X", 0
        dw O_COPIAR
        db "&Copiar                     Ctrl+C", 0
        dw O_PEGAR
        db "&Pegar                      Ctrl+V", 0
        dw O_BORRAR
        db "&Borrar                     Supr", 0
        dw 0
        db 0
        dw O_REL_ABAJO
        db "Rellenar hacia &abajo       Ctrl+J", 0
        dw O_REL_DER
        db "Rellenar hacia la derec&ha  Ctrl+D", 0
        dw 0
        db 0
        dw O_ELI_FILA
        db "Eliminar &filas", 0
        dw O_ELI_COL
        db "Eliminar co&lumnas", 0
        dw 0
        db 0
        dw O_TODO
        db "&Seleccionar todo           Ctrl+E", 0
        dw O_BUSCAR
        db "B&uscar...                  Ctrl+B", 0
        dw O_SIGUIENTE
        db "Buscar siguie&nte           F3", 0
        dw O_IRA
        db "&Ir a...                    F5", 0
        dw 0xFFFF
        db "&Insertar", 0
        dw O_INS_FILA
        db "&Filas", 0
        dw O_INS_COL
        db "&Columnas", 0
        dw 0
        db 0
        dw O_AUTOSUMA
        db "&Autosuma", 0
        dw O_FECHA
        db "Fecha de &hoy", 0
%ifdef CON_GRAFICOS
        dw 0
        db 0
        dw O_GRAFICO
        db "Gráfico de &barras", 0
        dw O_GRAFICO + 1
        db "Gráfico de &líneas", 0
        dw O_GRAFICO + 2
        db "Gráfico de &sectores", 0
%endif
        dw 0xFFFF
        db "&Formato", 0
        dw O_CELDAS
        db "&Celdas...", 0
        dw 0
        db 0
        dw O_NEG
        db "&Negrita             Ctrl+N", 0
        dw O_CUR
        db "C&ursiva             Ctrl+K", 0
        dw 0
        db 0
        dw O_IZQ
        db "A la &izquierda", 0
        dw O_CEN
        db "C&entrado", 0
        dw O_DER
        db "A la &derecha", 0
        dw 0
        db 0
        dw O_ANCHO
        db "&Ancho de columna...", 0
        dw O_SIMBOLO
        db "&Moneda...", 0
        dw 0xFFFF
        db "&Herramientas", 0
        dw O_ORD_ASC
        db "Ordenar de &menor a mayor", 0
        dw O_ORD_DESC
        db "Ordenar de ma&yor a menor", 0
        dw 0xFFFF
        db "&?", 0
        dw O_ACERCA
        db "&Acerca de Cuentas...", 0
        dw 0xFFFF
        db 0

; ---- los atajos del teclado: la tecla (Ctrl+letra) y su orden ----
            align 2
atajos      dw TC_CTRL + 21, O_NUEVO            ; Ctrl+U
            dw TC_CTRL + 1, O_ABRIR             ; A
            dw TC_CTRL + 7, O_GUARDAR           ; G
            dw TC_CTRL + 16, O_IMPRIMIR         ; P
            dw TC_CTRL + 26, O_DESHACER         ; Z
            dw TC_CTRL + 25, O_REHACER          ; Y
            dw TC_CTRL + 24, O_CORTAR           ; X
            dw TC_CTRL + 3, O_COPIAR            ; C
            dw TC_CTRL + 22, O_PEGAR            ; V
            dw TC_CTRL + 5, O_TODO              ; E
            dw TC_CTRL + 2, O_BUSCAR            ; B
            dw TC_CTRL + 14, O_NEG              ; N
            dw TC_CTRL + 11, O_CUR              ; K
            dw TC_CTRL + 10, O_REL_ABAJO        ; J
            dw TC_CTRL + 4, O_REL_DER           ; D
            dw TC_F1 + 2, O_SIGUIENTE           ; F3
            dw TC_F1 + 4, O_IRA                 ; F5
            dw 0
; ---- las órdenes: su número, su rutina y lo que se le da en EAX ----
%macro ORDEN 2-3 0
        dw %1
        dd %2, %3
%endmacro
ordenes:
        ORDEN O_NUEVO, nuevo
        ORDEN O_ABRIR, abrir
        ORDEN O_GUARDAR, guardar
        ORDEN O_COMO, guarda_como
        ORDEN O_IMPRIMIR, imprimir
        ORDEN O_SALIR, salir
        ORDEN O_DESHACER, deshacer
        ORDEN O_REHACER, rehacer
        ORDEN O_CORTAR, cortar
        ORDEN O_COPIAR, copiar
        ORDEN O_PEGAR, pegar
        ORDEN O_BORRAR, borrar_rango
        ORDEN O_REL_ABAJO, rellenar, 0
        ORDEN O_REL_DER, rellenar, 1
        ORDEN O_TODO, todo
        ORDEN O_BUSCAR, buscar
        ORDEN O_SIGUIENTE, buscar_sig
        ORDEN O_IRA, ir_a
        ORDEN O_ELI_FILA, filas_orden, 1
        ORDEN O_ELI_COL, cols_orden, 1
        ORDEN O_INS_FILA, filas_orden, 0
        ORDEN O_INS_COL, cols_orden, 0
        ORDEN O_AUTOSUMA, autosuma
        ORDEN O_FECHA, pon_fecha
        ORDEN O_CELDAS, celdas
        ORDEN O_NEG, fmt_bit, F_NEG
        ORDEN O_CUR, fmt_bit, F_CUR
        ORDEN O_IZQ, fmt_alin, 1
        ORDEN O_CEN, fmt_alin, 2
        ORDEN O_DER, fmt_alin, 3
        ORDEN O_MONEDA, fmt_clase, FN_MONEDA
        ORDEN O_PORC, fmt_clase, FN_PORC
        ORDEN O_DEC_MAS, fmt_dec, 1
        ORDEN O_DEC_MENOS, fmt_dec, -1
        ORDEN O_ANCHO, ancho_col
        ORDEN O_SIMBOLO, simbolo
        ORDEN O_COLOR, fmt_color, 0
        ORDEN O_COLOR + 1, fmt_color, 1
        ORDEN O_COLOR + 2, fmt_color, 2
        ORDEN O_COLOR + 3, fmt_color, 3
        ORDEN O_ORD_ASC, ordenar, 0
        ORDEN O_ORD_DESC, ordenar, 1
%ifdef CON_GRAFICOS
        ORDEN O_GRAFICO, grafico, 0
        ORDEN O_GRAFICO + 1, grafico, 1
        ORDEN O_GRAFICO + 2, grafico, 2
%endif
        ORDEN O_ACERCA, acerca
        dw 0

; ---- los botones de la barra, en dos filas: arriba, el archivo, la edición
;      y las herramientas; abajo, el formato. De cada uno: x, ancho, su icono
;      (o su texto), la orden, su nombre (sale en el pie al pasar el ratón) y
;      su clase (barra.inc) ----
            align 4
botones1    dd 6, 24, ic_nuevo, O_NUEVO, n_nuevo, BT_ICONO
            dd 32, 24, ic_abrir, O_ABRIR, n_abrir, BT_ICONO
            dd 58, 24, ic_guardar, O_GUARDAR, n_guardar, BT_ICONO
            dd 90, 24, ic_imprimir, O_IMPRIMIR, n_imprimir, BT_ICONO
            dd 122, 24, ic_cortar, O_CORTAR, n_cortar, BT_ICONO
            dd 148, 24, ic_copiar, O_COPIAR, n_copiar, BT_ICONO
            dd 174, 24, ic_pegar, O_PEGAR, n_pegar, BT_ICONO
            dd 206, 24, ic_deshacer, O_DESHACER, n_deshacer, BT_ICONO
            dd 232, 24, ic_rehacer, O_REHACER, n_rehacer, BT_ICONO
            dd 264, 24, ic_sigma, O_AUTOSUMA, n_autosuma, BT_ICONO
            dd 296, 34, t_b_az, O_ORD_ASC, n_ord_asc, BT_TEXTO
            dd 332, 34, t_b_za, O_ORD_DESC, n_ord_desc, BT_TEXTO
            dd 374, 24, ic_buscar, O_BUSCAR, n_buscar, BT_ICONO
%ifdef CON_GRAFICOS
            dd 406, 24, ic_grafico, O_GRAFICO, n_grafico, BT_ICONO
%endif
            dd 0, 0, 0, 0, 0, 0
botones2    dd 6, 24, t_b_n, O_NEG, n_neg, BT_NEGRITA
            dd 32, 24, t_b_k, O_CUR, n_cur, BT_TEXTO
            dd 64, 24, ic_izq, O_IZQ, n_izq, BT_ICONO
            dd 90, 24, ic_cen, O_CEN, n_cen, BT_ICONO
            dd 116, 24, ic_der, O_DER, n_der, BT_ICONO
            dd 148, 24, ic_moneda, O_MONEDA, n_moneda, BT_ICONO
            dd 174, 24, t_b_porc, O_PORC, n_porc, BT_TEXTO
            dd 200, 24, ic_dec_mas, O_DEC_MAS, n_dec_mas, BT_ICONO
            dd 226, 24, ic_dec_menos, O_DEC_MENOS, n_dec_menos, BT_ICONO
            dd 258, 22, C_TINTA, O_COLOR, n_c0, BT_COLOR
            dd 282, 22, C_GRANATE, O_COLOR + 1, n_c1, BT_COLOR
            dd 306, 22, C_ANIL, O_COLOR + 2, n_c2, BT_COLOR
            dd 330, 22, C_OLIVA, O_COLOR + 3, n_c3, BT_COLOR
            dd 0, 0, 0, 0, 0, 0
n_nuevo     db "Nuevo: una hoja en blanco (Ctrl+U)", 0
n_abrir     db "Abrir una hoja guardada (Ctrl+A)", 0
n_guardar   db "Guardar la hoja (Ctrl+G)", 0
n_imprimir  db "Imprimir lo elegido o, si es una celda, toda la hoja (Ctrl+P)", 0
n_cortar    db "Cortar lo elegido (Ctrl+X)", 0
n_copiar    db "Copiar lo elegido (Ctrl+C)", 0
n_pegar     db "Pegar (Ctrl+V)", 0
n_deshacer  db "Deshacer el último cambio (Ctrl+Z)", 0
n_rehacer   db "Rehacer lo deshecho (Ctrl+Y)", 0
n_autosuma  db "Autosuma: la suma de los números de encima (o de la izquierda)", 0
n_ord_asc   db "Ordenar las filas elegidas de menor a mayor, por la columna de la celda activa", 0
n_ord_desc  db "Ordenar las filas elegidas de mayor a menor, por la columna de la celda activa", 0
n_buscar    db "Buscar un texto en la hoja (Ctrl+B)", 0
n_grafico   db "Gráfico de barras de lo elegido", 0
n_neg       db "Negrita (Ctrl+N)", 0
n_cur       db "Cursiva (Ctrl+K)", 0
n_izq       db "Alinear a la izquierda", 0
n_cen       db "Centrar", 0
n_der       db "Alinear a la derecha", 0
n_moneda    db "Formato de moneda: 1.234,50 EUR", 0
n_porc      db "Formato de porcentaje: 12%", 0
n_dec_mas   db "Un decimal más", 0
n_dec_menos db "Un decimal menos", 0
n_c0        db "Letra negra", 0
n_c1        db "Letra roja", 0
n_c2        db "Letra azul", 0
n_c3        db "Letra verde", 0
t_b_n       db "N", 0
t_b_k       db "K", 0
t_b_az      db "A-Z", 0
t_b_za      db "Z-A", 0
t_b_porc    db "%", 0
; los colores de la letra (de la paleta de Hispa Ventanas), como en Pluma
colores     db C_TINTA, C_GRANATE, C_ANIL, C_OLIVA, C_TERRACOTA, C_PETROLEO, C_MARRON, C_PIEDRA

; ---- iconos propios, con la forma de los de barra.inc ----
ic_moneda:
        db "................"
        db "................"
        db "......ccccc....."
        db ".....cc...cc...."
        db "....cc.........."
        db "....cc.........."
        db "..cccccccc......"
        db "....cc.........."
        db "..cccccccc......"
        db "....cc.........."
        db "....cc.........."
        db ".....cc...cc...."
        db "......ccccc....."
        db "................"
        db "................"
        db "................"
ic_dec_mas:
        db "............b..."
        db "............b..."
        db "..........bbbbb."
        db "............b..."
        db "............b..."
        db "....000...000..."
        db "...0...0.0...0.."
        db "...0...0.0...0.."
        db "...0...0.0...0.."
        db "...0...0.0...0.."
        db "...0...0.0...0.."
        db ".0..000...000..."
        db ".0.............."
        db "0..............."
        db "................"
        db "................"
ic_dec_menos:
        db "................"
        db "................"
        db "..........ddddd."
        db "................"
        db "................"
        db "........000....."
        db ".......0...0...."
        db ".......0...0...."
        db ".......0...0...."
        db ".......0...0...."
        db ".......0...0...."
        db ".....0..000....."
        db ".....0.........."
        db "....0..........."
        db "................"
        db "................"

; ---- diálogo: el formato de las celdas ----
dlg_fmt:
        PLANTILLA HV_c_dialogo, t_d_fmt, E_DIALOGO, 0, 0, 430, 312, 0
        PLANTILLA HV_c_estatico, t_f_numero, E_HIJA | SS_NEGRITA, 20, 12, 180, 18, 100
        PLANTILLA HV_c_boton, t_f_n0, E_HIJA | E_TABULA | BS_OPCION, 20, 34, 190, 20, 40
        PLANTILLA HV_c_boton, t_f_n1, E_HIJA | E_TABULA | BS_OPCION, 20, 56, 190, 20, 41
        PLANTILLA HV_c_boton, t_f_n2, E_HIJA | E_TABULA | BS_OPCION, 20, 78, 190, 20, 42
        PLANTILLA HV_c_boton, t_f_n3, E_HIJA | E_TABULA | BS_OPCION, 20, 100, 190, 20, 43
        PLANTILLA HV_c_boton, t_f_n4, E_HIJA | E_TABULA | BS_OPCION, 20, 122, 190, 20, 44
        PLANTILLA HV_c_boton, t_f_n5, E_HIJA | E_TABULA | BS_OPCION, 20, 144, 190, 20, 45
        PLANTILLA HV_c_estatico, t_f_dec, E_HIJA, 20, 178, 84, 18, 101
        PLANTILLA HV_c_edit, 0, E_HIJA | E_TABULA, 108, 174, 40, 24, 50
        PLANTILLA HV_c_estatico, t_f_letra, E_HIJA | SS_NEGRITA, 230, 12, 180, 18, 102
        PLANTILLA HV_c_boton, t_f_neg, E_HIJA | E_TABULA | BS_CASILLA, 230, 34, 90, 20, 20
        PLANTILLA HV_c_boton, t_f_cur, E_HIJA | E_TABULA | BS_CASILLA, 326, 34, 90, 20, 21
        PLANTILLA HV_c_estatico, t_f_color, E_HIJA, 230, 60, 180, 18, 103
        PLANTILLA HV_c_lista, t_nada, E_HIJA | E_TABULA, 230, 80, 180, 70, 61
        PLANTILLA HV_c_estatico, t_f_alin, E_HIJA | SS_NEGRITA, 230, 158, 180, 18, 104
        PLANTILLA HV_c_lista, t_nada, E_HIJA | E_TABULA, 230, 178, 180, 70, 60
        PLANTILLA HV_c_boton, t_aceptar, E_HIJA | E_TABULA | BS_DEFECTO, 120, 248, 90, 26, ID_ACEPTAR
        PLANTILLA HV_c_boton, t_cancelar, E_HIJA | E_TABULA, 220, 248, 90, 26, ID_CANCELAR
        dd 0

; ---- textos (todo en español) ----
t_cuentas   db "Cuentas", 0
t_guion     db " - ", 0
t_sin_nombre db "(sin nombre)", 0
t_nada      db 0
t_ext_hoj   db ".HOJ", 0
t_hoj_cab   db "HOJA 1", 0
t_abrir     db "Abrir", 0
t_guardar_como db "Guardar como", 0
t_aceptar   db "Aceptar", 0
t_cancelar  db "Cancelar", 0
t_err_err   db "#ERROR", 0
t_err_circ  db "#CIRC", 0
t_err_ref   db "#REF", 0
t_cambios   db "La hoja tiene cambios sin guardar.", 10, "¿Quiere guardarlos?", 0
t_no_abrir  db "No se ha podido abrir el archivo.", 0
t_no_guardar db "No se ha podido guardar la hoja.", 10
            db "Compruebe que el disco no está lleno ni protegido.", 0
t_llena     db "La hoja está llena: no cabe más texto.", 10
            db "Lo último que se ha escrito o pegado puede haber quedado sin poner.", 0
t_clip_grande db "Lo elegido es demasiado grande para copiarlo de una vez.", 10
            db "Elija un rango más pequeño.", 0
t_no_cabe   db "No se puede insertar: las últimas filas o columnas de la hoja", 10
            db "tienen algo escrito y se perdería.", 0
t_ordenar_elija db "Para ordenar, elija antes las filas (dos o más), empezando", 10
            db "por una celda de la columna que manda en el orden.", 0
t_no_esta   db "No se ha encontrado ese texto en la hoja.", 0
t_buscar    db "Buscar", 0
t_q_buscar  db "Texto que se busca:", 0
t_ira       db "Ir a", 0
t_q_ira     db "Celda (por ejemplo, B7):", 0
t_no_celda  db "Eso no es una celda de la hoja. Las celdas van de A1 a BL1000.", 0
t_ancho     db "Ancho de columna", 0
t_q_ancho   db "Ancho, en letras (de 1 a 59):", 0
t_simbolo   db "Moneda", 0
t_q_simbolo db "Lo que va detrás de la cantidad:", 0
t_imp_vacia db "La hoja está vacía: no hay nada que imprimir.", 0
t_impreso   db "La hoja se ha enviado a la impresora.", 0
t_e_imprimir db "No se ha podido imprimir.", 10
            db "Compruebe que la impresora (la del puerto paralelo, LPT1)", 10
            db "está encendida y tiene papel.", 0
t_suma_ini  db "=SUMA(", 0
t_pie_edita db "Intro: aceptar    Esc: dejarla como estaba    F2: mover el cursor", 0
t_pie_vacia db ": celda vacía", 0
t_pie_formula db ": una fórmula", 0
t_pie_numero db ": un número", 0
t_pie_texto db ": un texto", 0
t_pie_suma  db "    Suma: ", 0
t_pie_promedio db "    Promedio: ", 0
t_pie_cuenta db "    Números: ", 0
t_d_fmt     db "Formato de celdas", 0
t_f_numero  db "Número", 0
t_f_n0      db "General", 0
t_f_n1      db "Decimales fijos", 0
t_f_n2      db "Con millares: 1.234", 0
t_f_n3      db "Moneda", 0
t_f_n4      db "Porcentaje", 0
t_f_n5      db "Fecha: 16/11/2026", 0
t_f_dec     db "Decimales:", 0
t_f_letra   db "Letra", 0
t_f_neg     db "Negrita", 0
t_f_cur     db "Cursiva", 0
t_f_color   db "Color:", 0
t_f_alin    db "Alineación", 0
t_alins     db "La normal", 0, "A la izquierda", 0, "Centrado", 0, "A la derecha", 0, 0
t_colores   db "Negro", 0, "Rojo", 0, "Azul", 0, "Verde", 0, "Terracota", 0, "Petróleo", 0, "Marrón", 0, "Gris", 0, 0
t_acerca_tit db "Acerca de Cuentas", 0
t_acerca    db "Cuentas", 10
            db "La hoja de cálculo de Hispa Ventanas.", 10
            db "Copyright (C) 2026 Carlos Sanz y contribuidores.", 10
            db "GPL-2.0 o posterior.", 0

; =============================================================================
;  La entrada: EBX=base ESI=ruta del módulo EDI=parámetros
; =============================================================================
entrada:
        cld
        mov [param], edi
        mov esi, hijos
        call hvl_clases
        mov esi, dlg_fmt
        call hvl_clases
        call hoja_vacia
        call pon_titulo
        HV pantalla                 ; EAX=ancho EBX=alto: que quepa
        sub ebx, BARRA_H + 12
        cmp ebx, VEN_AL
        jae .a
        mov [cue_plant+P_AL], ebx
        mov dword [cue_plant+P_Y], BARRA_H + 4
.a:
        cmp eax, VEN_AN + 8
        jae .b
        sub eax, 8
        mov [cue_plant+P_AN], eax
        mov dword [cue_plant+P_X], 4
.b:
        mov esi, cue_plant
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

; ESI=parámetros: el primer nombre, a nom_tmp -> ESI=nom_tmp; CF=1 si no hay
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
        mov esi, nom_tmp
        clc
        ret
.no:
        stc
        ret

; el título: «Cuentas - NOMBRE»
pon_titulo:
        pushad
        mov edi, titulo
        mov esi, t_cuentas
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

; =============================================================================
;  El procedimiento de la ventana
; =============================================================================
cue_proc:
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
        je .barra_v
        cmp eax, WM_HSCROLL
        je .barra_h
        cmp eax, WM_SIZE
        je .tam
        cmp eax, WM_KILLFOCUS
        je raton_suelta
        cmp eax, WM_ENTERMENULOOP   ; se abre un menú: el ratón ya no está
        jne .nmenu                  ; sobre ningún botón
        mov dword [br_sobre], 0
        call inval
        jmp hecho
.nmenu:
        cmp eax, WM_HV_ABRIR
        je .hvabrir
        cmp eax, WM_GETMINMAXINFO   ; lo más pequeño
        jne .nmin
        mov eax, 420 + 260 * 65536
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
        mov dword [ebp+V_MENU], cue_menu
        mov dword [ebp+V_ICONO], hvp_icono
        mov esi, hijos
        HV ven_lista
        mov ecx, ID_PUENTE
        HV ven_hija_id
        mov [puente], eax
        mov ecx, ID_BARRA_H
        HV ven_hija_id
        mov [barra_h], eax
        mov ecx, ID_BARRA_V
        HV ven_hija_id
        mov [barra_v], eax
.tam:
        cmp dword [barra_v], 0
        je hecho
        call medidas
        mov ebp, [barra_v]          ; la barra vertical, a la derecha de la rejilla
        mov eax, [gx1]
        mov ebx, HY0
        mov ecx, BD_AN
        mov edx, [gy1]
        sub edx, ebx
        HV ven_mover
        mov ebp, [barra_h]          ; y la horizontal, debajo
        xor eax, eax
        mov ebx, [gy1]
        mov ecx, [gx1]
        mov edx, BD_AN
        HV ven_mover
        call pon_barras
        call inval
        jmp hecho
.rueda:
        lea eax, [ecx+ecx*2]        ; tres filas por cada paso
        add eax, [desp_fil]
        jmp .bv
.barra_v:
        mov eax, ecx
.bv:
        mov ebx, NFIL
        sub ebx, [vis_fil]
        cmp eax, ebx
        jle .bv1
        mov eax, ebx
.bv1:
        test eax, eax
        jns .bv2
        xor eax, eax
.bv2:
        mov [desp_fil], eax
        call pon_barras
        call inval
        jmp hecho
.barra_h:
        cmp ecx, NCOL - 1
        jbe .bh1
        mov ecx, NCOL - 1
.bh1:
        mov [desp_col], ecx
        call cuenta_cols
        call inval
        jmp hecho
.cerrar:
        cmp byte [editando], 0
        je .c1
        call commit_edit
.c1:
        call preguntar
        jc hecho
        mov ebp, [ven]
        HV ven_destruir
        jmp hecho
.destruir:
        mov dword [ven], 0
        mov dword [barra_v], 0
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

; la ventana, a repintar
inval:
        push ebp
        mov ebp, [ven]
        test ebp, ebp
        jz .x
        pushad
        HV ven_invalidar
        popad
.x:
        pop ebp
        ret
; ...porque ha cambiado algo de la hoja (lo calculado ya no vale)
invalida:
        call calc_olvida
%ifdef CON_GRAFICOS
        call gr_inval
%endif
        jmp inval

; mensaje de aviso: ESI=texto. Conserva los registros
aviso:
        pushad
        mov ebp, [ven]
        mov edi, t_cuentas
        mov eax, DM_ACEPTAR
        HV dlg_mensaje
        popad
        ret

; el cursor, a la primera celda, y la vista, al principio
al_principio:
        push eax
        xor eax, eax
        mov [cur_col], eax
        mov [cur_fil], eax
        mov [sel_c2], eax
        mov [sel_f2], eax
        mov [desp_col], eax
        mov [desp_fil], eax
        mov [editando], al
        pop eax
        cmp dword [barra_v], 0
        je .x
        call cuenta_cols
        call pon_barras
.x:
        ret

; =============================================================================
;  Teclado: WM_KEYDOWN (ECX=código, EDX=estado de mayúsculas)
; =============================================================================
tecla:
        mov [tec_may], edx
        mov esi, atajos             ; ¿un atajo de una orden?
.at:
        movzx eax, word [esi]
        test eax, eax
        jz .no_at
        cmp eax, ecx
        je .es_at
        add esi, 4
        jmp .at
.es_at:
        movzx ecx, word [esi+2]
        jmp orden
.no_at:
        cmp byte [editando], 0
        jne tecla_edita
        xor eax, eax
        xor ebx, ebx
        cmp ecx, TC_IZQ
        je .izq
        cmp ecx, TC_DER
        je .der
        cmp ecx, TC_ARRIBA
        je .arr
        cmp ecx, TC_ABAJO
        je .aba
        cmp ecx, TC_INTRO
        je .intro
        cmp ecx, TC_TAB
        je .tab
        cmp ecx, TC_INICIO
        je .inicio
        cmp ecx, TC_FIN
        je .fin
        cmp ecx, TC_REPAG
        je .repag
        cmp ecx, TC_AVPAG
        je .avpag
        cmp ecx, TC_CTRL_INICIO
        je .cinicio
        cmp ecx, TC_CTRL_FIN
        je .cfin
        cmp ecx, TC_SUPR
        je .supr
        cmp ecx, TC_F1 + 1          ; F2
        jne hecho
        call edita_f2
        jmp hecho
.supr:
        call borrar_rango
        jmp hecho
.tab:
        mov dword [tec_may], 0
.der:
        inc eax
        jmp .mueve
.izq:
        dec eax
        jmp .mueve
.intro:
        mov dword [tec_may], 0
.aba:
        inc ebx
        jmp .mueve
.arr:
        dec ebx
        jmp .mueve
.repag:
        sub ebx, [vis_fil]
        jmp .mueve
.avpag:
        mov ebx, [vis_fil]
        jmp .mueve
.inicio:
        mov eax, -NCOL
        jmp .mueve
.fin:
        mov eax, [lim_c]            ; a la última columna con algo escrito
        dec eax
        jns .f1
        xor eax, eax
.f1:
        call .base
        sub eax, ecx
        jmp .mueve
.cinicio:
        mov eax, -NCOL
        mov ebx, -NFIL
        jmp .mueve
.cfin:
        mov eax, [lim_c]            ; a la última celda escrita
        dec eax
        jns .cf1
        xor eax, eax
.cf1:
        mov ebx, [lim_f]
        dec ebx
        jns .cf2
        xor ebx, ebx
.cf2:
        call .base
        sub eax, ecx
        sub ebx, edx
.mueve:
        call mueve_rel
        jmp hecho
; (interna) desde dónde se mueve: ECX=columna EDX=fila
.base:
        mov ecx, [cur_col]
        mov edx, [cur_fil]
        test dword [tec_may], TM_MAYUS
        jz .bx
        mov ecx, [sel_c2]
        mov edx, [sel_f2]
.bx:
        ret

; mueve la celda activa EAX columnas y EBX filas; con Mayús ([tec_may]), lo
; que se mueve es la otra esquina de lo elegido
mueve_rel:
        test dword [tec_may], TM_MAYUS
        jnz .may
        add eax, [cur_col]          ; sin Mayús: desde la celda activa
        add ebx, [cur_fil]
        jmp .a0
.may:
        add eax, [sel_c2]
        add ebx, [sel_f2]
.a0:
        test eax, eax
        jns .a1
        xor eax, eax
.a1:
        cmp eax, NCOL
        jl .a2
        mov eax, NCOL - 1
.a2:
        test ebx, ebx
        jns .b1
        xor ebx, ebx
.b1:
        cmp ebx, NFIL
        jl .b2
        mov ebx, NFIL - 1
.b2:
        mov [sel_c2], eax
        mov [sel_f2], ebx
        test dword [tec_may], TM_MAYUS
        jnz .x
        mov [cur_col], eax
        mov [cur_fil], ebx
.x:
        call a_la_vista
        jmp invalida

; ---- mientras se escribe en una celda ----
tecla_edita:
        cmp ecx, TC_INTRO
        je .intro
        cmp ecx, TC_TAB
        je .tab
        cmp ecx, TC_ESC
        je .esc
        cmp ecx, TC_RETRO
        je .retro
        cmp ecx, TC_SUPR
        je .supr
        cmp ecx, TC_INICIO
        je .inicio
        cmp ecx, TC_FIN
        je .fin
        cmp ecx, TC_F1 + 1          ; F2: las flechas mueven el cursor
        je .f2
        xor eax, eax
        xor ebx, ebx
        cmp ecx, TC_ARRIBA
        je .arr
        cmp ecx, TC_ABAJO
        je .aba
        cmp ecx, TC_IZQ
        je .izq
        cmp ecx, TC_DER
        jne hecho
        cmp byte [edt_modo], 0
        je .der
        mov eax, [edt_pos]
        cmp byte [edt_buf+eax], 0
        je hecho
        inc dword [edt_pos]
        jmp .pinta
.izq:
        cmp byte [edt_modo], 0
        je .izq1
        cmp dword [edt_pos], 0
        je hecho
        dec dword [edt_pos]
        jmp .pinta
.izq1:
        dec eax
        jmp .acepta
.tab:
.der:
        inc eax
        jmp .acepta
.arr:
        dec ebx
        jmp .acepta
.intro:
        xor eax, eax
        xor ebx, ebx
.aba:
        inc ebx
.acepta:
        cmp ecx, TC_TAB
        jne .ac1
        mov eax, 1
        xor ebx, ebx
.ac1:
        push eax
        push ebx
        call commit_edit
        pop ebx
        pop eax
        mov dword [tec_may], 0
        call mueve_rel
        jmp hecho
.esc:
        mov byte [editando], 0
        jmp .pinta
.f2:
        mov byte [edt_modo], 1
        jmp hecho
.inicio:
        mov dword [edt_pos], 0
        jmp .pinta
.fin:
        call edt_largo
        mov [edt_pos], eax
        jmp .pinta
.retro:
        cmp dword [edt_pos], 0
        je hecho
        dec dword [edt_pos]
.supr:
        mov esi, [edt_pos]          ; quita la letra del cursor
        add esi, edt_buf
        cmp byte [esi], 0
        je .pinta
.s1:
        mov al, [esi+1]
        mov [esi], al
        inc esi
        test al, al
        jnz .s1
.pinta:
        call inval
        jmp hecho

; EAX=las letras de lo que se está escribiendo
edt_largo:
        xor eax, eax
.l:
        cmp byte [edt_buf+eax], 0
        je .x
        inc eax
        jmp .l
.x:
        ret

; WM_CHAR (ECX=carácter)
letra:
        mov al, cl
        cmp al, 32
        jb hecho
        cmp byte [editando], 0
        jne .mete
        mov byte [edt_buf], 0       ; empieza una celda nueva
        mov dword [edt_pos], 0
        mov byte [edt_modo], 0
        call edita_empieza
.mete:
        mov bl, al
        call edt_largo
        cmp eax, CEL_MAX - 2
        jae hecho
        lea esi, [edt_buf+eax]      ; hace sitio en el cursor
        mov ecx, eax
        sub ecx, [edt_pos]
        inc ecx
.h:
        mov al, [esi]
        mov [esi+1], al
        dec esi
        dec ecx
        jnz .h
        mov eax, [edt_pos]
        mov [edt_buf+eax], bl
        inc dword [edt_pos]
        call inval
        jmp hecho

; F2: corregir lo que hay en la celda activa, con el cursor al final
edita_f2:
        pushad
        mov ecx, [cur_col]
        mov edx, [cur_fil]
        call cel_puntero
        mov edi, edt_buf
        xor ecx, ecx
.l:
        lodsb
        stosb
        test al, al
        jz .f
        inc ecx
        jmp .l
.f:
        mov [edt_pos], ecx
        mov byte [edt_modo], 1
        popad
; (interna) empieza a escribir en la celda activa: lo elegido pasa a ser ella
; sola, y se trae a la vista
edita_empieza:
        push eax
        mov byte [editando], 1
        mov eax, [cur_col]
        mov [sel_c2], eax
        mov eax, [cur_fil]
        mov [sel_f2], eax
        call a_la_vista
        call inval
        pop eax
        ret

; acepta lo escrito: a la celda activa
commit_edit:
        pushad
        call instantanea
        mov ecx, [cur_col]
        mov edx, [cur_fil]
        mov esi, edt_buf
        call cel_pon
        call fecha_auto
        mov byte [editando], 0
        popad
        jmp fin_cambio

; =============================================================================
;  Ratón (ECX=estado de mayúsculas, EDX = x + y*65536)
; =============================================================================
raton:
        mov [tec_may], ecx
        movsx eax, dx
        mov ebx, edx
        sar ebx, 16
        cmp ebx, TB_AL
        jl .barra
        cmp byte [editando], 0      ; pulsar fuera de la barra acepta lo escrito
        je .r1
        call commit_edit
.r1:
        cmp ebx, HY0
        jl .formula
        cmp ebx, [gy1]
        jge hecho
        cmp eax, [gx1]
        jge hecho
        cmp ebx, GY0
        jl .rot_col
        cmp eax, ROT_AN
        jl .rot_fil
        call x_col                  ; una celda
        call y_fil
        mov byte [arrastra], 1
        test dword [tec_may], TM_MAYUS
        jnz .ext
        mov [cur_col], ecx
        mov [cur_fil], edx
.ext:
        mov [sel_c2], ecx
        mov [sel_f2], edx
.captura:
        mov ebp, [ven]
        HV ven_capturar
        call invalida
        jmp hecho
.formula:
        call edita_f2
        jmp hecho
.rot_col:
        cmp eax, ROT_AN
        jl .todo
        call borde_col              ; ¿el borde de una columna? Su ancho
        jc .rc1
        mov [arr_col], ecx
        call col_x
        mov [arr_x0], eax
        call instantanea
        mov byte [arrastra], 2
        jmp .captura
.rc1:
        call x_col                  ; la columna entera
        mov byte [arrastra], 3
        mov dword [cur_fil], 0
        mov dword [sel_f2], NFIL - 1
        mov [sel_c2], ecx
        test dword [tec_may], TM_MAYUS
        jnz .captura
        mov [cur_col], ecx
        jmp .captura
.rot_fil:
        call y_fil                  ; la fila entera
        mov byte [arrastra], 4
        mov dword [cur_col], 0
        mov dword [sel_c2], NCOL - 1
        mov [sel_f2], edx
        test dword [tec_may], TM_MAYUS
        jnz .captura
        mov [cur_fil], edx
        jmp .captura
.todo:
        call todo
        jmp hecho
.barra:
        mov esi, botones1           ; la fila de arriba o la de abajo
        cmp ebx, 30
        jl .b
        mov esi, botones2
.b:
        call br_boton_en
        jc hecho
        mov ecx, [esi+12]
        jmp orden

raton_mueve:
        movsx eax, dx
        mov ebx, edx
        sar ebx, 16
        movzx ecx, byte [arrastra]
        jmp [.que+ecx*4]
            align 4
.que        dd .nada, .celdas, .ancho, .columnas, .filas
.nada:
        xor edi, edi                ; sobre un botón de la barra: su nombre, al pie
        cmp ebx, TB_AL
        jae .forma
        test ebx, ebx
        js .forma
        mov esi, botones1
        cmp ebx, 30
        jl .f
        mov esi, botones2
.f:
        push eax
        call br_boton_en
        pop eax
        jc .forma
        mov edi, [esi+16]
.forma:
        xor edx, edx                ; sobre el borde del rótulo de una columna,
        cmp ebx, HY0                ; la flecha doble: se puede arrastrar
        jl .f2
        cmp ebx, GY0
        jge .f2
        cmp eax, ROT_AN
        jl .f2
        cmp eax, [gx1]
        jge .f2
        call borde_col
        jc .f2
        mov edx, CUR_HORIZ
.f2:
        cmp edx, [forma]
        je .f3
        mov [forma], edx
        mov eax, edx
        mov ebp, [ven]
        HV cur_forma
.f3:
        call br_sobre_pon
        je hecho
        call inval
        jmp hecho
.celdas:
        xor esi, esi                ; fuera de la rejilla: la vista se corre
        cmp ebx, [gy1]
        jl .c1
        mov edx, NFIL
        sub edx, [vis_fil]
        cmp [desp_fil], edx
        jge .c1
        inc dword [desp_fil]
        inc esi
.c1:
        cmp ebx, GY0
        jge .c2
        cmp dword [desp_fil], 0
        je .c2
        dec dword [desp_fil]
        inc esi
.c2:
        cmp eax, [gx1]
        jl .c3
        cmp dword [desp_col], NCOL - 1
        jae .c3
        inc dword [desp_col]
        inc esi
.c3:
        cmp eax, ROT_AN
        jge .c4
        cmp dword [desp_col], 0
        je .c4
        dec dword [desp_col]
        inc esi
.c4:
        call .acota
        call x_col
        call y_fil
        cmp ecx, [sel_c2]
        jne .c5
        cmp edx, [sel_f2]
        jne .c5
        test esi, esi
        jz hecho
.c5:
        mov [sel_c2], ecx
        mov [sel_f2], edx
.vista:
        call cuenta_cols
        call pon_barras
        call inval
        jmp hecho
.ancho:
        sub eax, [arr_x0]           ; el ancho nuevo de la columna
        cmp eax, AN_MIN
        jge .a1
        mov eax, AN_MIN
.a1:
        cmp eax, AN_MAX
        jle .a2
        mov eax, AN_MAX
.a2:
        mov ecx, [arr_col]
        cmp ax, [col_an+ecx*2]
        je hecho
        mov [col_an+ecx*2], ax
        jmp .vista
.columnas:
        call .acota
        call x_col
        cmp ecx, [sel_c2]
        je hecho
        mov [sel_c2], ecx
        jmp .vista
.filas:
        call .acota
        call y_fil
        cmp edx, [sel_f2]
        je hecho
        mov [sel_f2], edx
        jmp .vista
; (interna) el punto, dentro de la rejilla
.acota:
        cmp eax, ROT_AN
        jge .k1
        mov eax, ROT_AN
.k1:
        cmp eax, [gx1]
        jl .k2
        mov eax, [gx1]
        dec eax
.k2:
        cmp ebx, GY0
        jge .k3
        mov ebx, GY0
.k3:
        cmp ebx, [gy1]
        jl .k4
        mov ebx, [gy1]
        dec ebx
.k4:
        ret

raton_suelta:
        cmp byte [arrastra], 0
        je hecho
        mov byte [arrastra], 0
        mov ebp, [ven]
        HV ven_soltar
        jmp hecho

; doble clic en una celda: corregirla
raton_doble:
        movsx eax, dx
        mov ebx, edx
        sar ebx, 16
        cmp ebx, GY0
        jl raton
        cmp ebx, [gy1]
        jge raton
        cmp eax, ROT_AN
        jl raton
        cmp eax, [gx1]
        jge raton
        cmp byte [editando], 0
        jne hecho
        call x_col
        call y_fil
        mov [cur_col], ecx
        mov [cur_fil], edx
        call edita_f2
        jmp hecho

; =============================================================================
;  Las órdenes del menú, de los botones y de los atajos: ECX=número
; =============================================================================
orden:
        mov dword [br_sobre], 0
        cmp ecx, 100                ; (los avisos de las ventanas hijas, no)
        jb hecho
        cmp ecx, 0x10000
        jae hecho
        cmp byte [editando], 0      ; lo que se estaba escribiendo se acepta
        je .o
        push ecx
        call commit_edit
        pop ecx
.o:
        mov esi, ordenes
.b:
        movzx eax, word [esi]
        test eax, eax
        jz hecho
        cmp eax, ecx
        je .es
        add esi, 10
        jmp .b
.es:
        mov eax, [esi+6]
        call [esi+2]
        jmp hecho

nuevo:
        call preguntar
        jc .x
        call hoja_vacia
        mov byte [nombre], 0
        call al_principio
        mov byte [modif], 0
        call pon_titulo
        call invalida
.x:
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
        mov esi, nom_tmp
        call cargar
.x:
        ret
salir:
        mov ebp, [ven]
        mov eax, WM_CLOSE
        xor ecx, ecx
        xor edx, edx
        HV msg_poner
        ret
acerca:
        mov ebp, [ven]
        mov esi, t_acerca
        mov edi, t_acerca_tit
        mov eax, DM_ACEPTAR
        HV dlg_mensaje
        ret

; si hay cambios, pregunta si se guardan. CF=1 si no hay que seguir
preguntar:
        cmp byte [modif], 0
        je .si
        mov ebp, [ven]
        mov esi, t_cambios
        mov edi, t_cuentas
        mov eax, DM_SI_NO_CANCELAR
        HV dlg_mensaje
        cmp eax, ID_NO
        je .si
        cmp eax, ID_SI
        jne .no
        call guardar
        jc .no
.si:
        clc
        ret
.no:
        stc
        ret

; ---- Seleccionar todo ----
todo:
        xor eax, eax
        mov [cur_col], eax
        mov [cur_fil], eax
        mov dword [sel_c2], NCOL - 1
        mov dword [sel_f2], NFIL - 1
        jmp inval

; ---- Buscar ----
buscar:
        mov ebp, [ven]
        mov esi, t_q_buscar
        mov edi, t_buscar
        mov ebx, busca_txt
        mov ecx, 40
        HV dlg_pedir
        test eax, eax
        jz .x
        call buscar_sig
.x:
        ret

; ---- Ir a una celda ----
ir_a:
        mov byte [num_txt], 0
        mov ebp, [ven]
        mov esi, t_q_ira
        mov edi, t_ira
        mov ebx, num_txt
        mov ecx, 10
        HV dlg_pedir
        test eax, eax
        jz .x
        mov esi, num_txt
.sp:
        cmp byte [esi], ' '
        jne .r
        inc esi
        jmp .sp
.r:
        call lee_ref
        jc .mal
        cmp ecx, NCOL
        jae .mal
        cmp edx, NFIL
        jae .mal
        mov [cur_col], ecx
        mov [sel_c2], ecx
        mov [cur_fil], edx
        mov [sel_f2], edx
        call a_la_vista
        jmp invalida
.mal:
        mov esi, t_no_celda
        call aviso
.x:
        ret

; ---- Insertar > Fecha de hoy: en la celda activa ----
pon_fecha:
        call hoy_serie
        mov edi, edt_buf
        call fecha_txt
        mov byte [edi], 0
        jmp commit_edit

; ---- Autosuma: propone la suma de los números que hay encima de la celda
;      activa (o, si no hay, a su izquierda); Intro la acepta ----
autosuma:
        mov ecx, [cur_col]
        mov edx, [cur_fil]
        mov ebx, edx                ; hacia arriba, mientras haya números
.arr:
        test edx, edx
        jz .arrf
        dec edx
        call .es_num
        je .arr
        inc edx
.arrf:
        cmp edx, ebx
        je .izq
        dec ebx                     ; de (ECX, EDX) a (ECX, EBX)
        mov eax, ecx
        jmp .pon
.izq:
        mov eax, ecx                ; hacia la izquierda
.izq1:
        test ecx, ecx
        jz .izqf
        dec ecx
        call .es_num
        je .izq1
        inc ecx
.izqf:
        cmp ecx, eax
        je .nada
        dec eax                     ; de (ECX, EDX) a (EAX, EBX)
.pon:
        mov edi, edt_buf
        mov esi, t_suma_ini
        call hvl_cad
        call ref_nombre
        mov byte [edi], ':'
        inc edi
        mov ecx, eax
        mov edx, ebx
        call ref_nombre
        mov word [edi], ')'
        inc edi
        jmp .edita
.nada:
        mov edi, edt_buf            ; nada que sumar: «=SUMA()», con el cursor dentro
        mov esi, t_suma_ini
        call hvl_cad
        mov word [edi], ')'
.edita:
        sub edi, edt_buf
        mov [edt_pos], edi
        mov byte [edt_modo], 1
        jmp edita_empieza
; (interna) ¿la celda ECX, EDX tiene un número? -> ZF=1 si sí
.es_num:
        push eax
        push edx
        push esi
        call cel_puntero
        cmp byte [esi], 0
        je .no
        call valor_celda
        cmp byte [ev_err], 0
        jne .no
        cmp byte [cv_tipo], 1
        jmp .f
.no:
        or esi, esi
.f:
        pop esi
        pop edx
        pop eax
        ret

; =============================================================================
;  El formato de lo elegido
; =============================================================================
; quita los bits EAX y pone los EBX en el formato de todas las celdas elegidas
fmt_aplica:
        pushad
        call instantanea
        call sel_rect
        not eax
        mov edx, [r_f1]
.f:
        mov ecx, [r_c1]
.c:
        mov esi, edx
        shl esi, 6
        add esi, ecx
        and [cel_fmt+esi*2], ax
        or [cel_fmt+esi*2], bx
        inc ecx
        cmp ecx, [r_c2]
        jbe .c
        inc edx
        cmp edx, [r_f2]
        jbe .f
        popad
        jmp invalida
; EAX=el formato de la celda activa
fmt_activa:
        push ecx
        push edx
        mov ecx, [cur_col]
        mov edx, [cur_fil]
        call cel_formato
        pop edx
        pop ecx
        ret
; negrita o cursiva (EAX=el bit): se quita si la celda activa lo tiene
fmt_bit:
        mov ebx, eax
        mov ecx, eax
        call fmt_activa
        test eax, ecx
        mov eax, ecx
        jz fmt_aplica
        xor ebx, ebx
        jmp fmt_aplica
; alineación (EAX: 1 izquierda, 2 centro, 3 derecha): la que ya tiene la
; celda activa se quita (vuelve a la normal)
fmt_alin:
        mov ebx, eax
        shl ebx, F_ALIN_D
        call fmt_activa
        and eax, F_ALIN
        cmp eax, ebx
        mov eax, F_ALIN
        jne fmt_aplica
        xor ebx, ebx
        jmp fmt_aplica
; moneda o porcentaje (EAX=FN_*): si la celda activa ya lo tiene, se quita
fmt_clase:
        mov ebx, eax
        shl ebx, F_NUM_D
        cmp eax, FN_MONEDA
        jne .a
        or ebx, 2 << F_DEC_D        ; la moneda, con dos decimales
.a:
        call fmt_activa
        xor eax, ebx
        test eax, F_NUM
        mov eax, F_NUM | F_DEC
        jnz fmt_aplica
        xor ebx, ebx
        jmp fmt_aplica
; un decimal más o menos (EAX=1 o -1). Una celda sin formato de número pasa
; a tener decimales fijos: los que se le ven, y uno más o uno menos
fmt_dec:
        mov esi, eax
        call fmt_activa
        mov ebx, eax
        and ebx, F_NUM
        mov ecx, eax
        shr ecx, F_DEC_D
        and ecx, 15
        test ebx, ebx
        jnz .a
        mov ebx, FN_FIJO << F_NUM_D
        push edx
        mov ecx, [cur_col]
        mov edx, [cur_fil]
        call celda_ver
        pop edx
        xor ecx, ecx                ; las cifras que hay detrás de la coma
        mov edi, disp
.c:
        mov al, [edi]
        inc edi
        test al, al
        jz .a
        cmp al, ','
        jne .c
.c1:
        cmp byte [edi], 0
        je .a
        inc edi
        inc ecx
        jmp .c1
.a:
        add ecx, esi
        jns .a1
        xor ecx, ecx
.a1:
        cmp ecx, CU_DEC
        jbe .a2
        mov ecx, CU_DEC
.a2:
        shl ecx, F_DEC_D
        or ebx, ecx
        mov eax, F_NUM | F_DEC
        jmp fmt_aplica
; el color de la letra (EAX: de 0 a 7)
fmt_color:
        mov ebx, eax
        shl ebx, F_COLOR_D
        mov eax, F_COLOR
        jmp fmt_aplica

; ---- Formato > Celdas: el diálogo ----
celdas:
        call fmt_activa
        mov [df_fmt], eax
        mov ebp, [ven]
        mov esi, dlg_fmt
        mov edi, dlg_fmt + P_TAM
        mov ebx, df_gancho
        xor edx, edx
        HV dlg_modal
        cmp eax, ID_ACEPTAR
        jne .x
        mov eax, 0xFFFF
        mov ebx, [df_fmt]
        call fmt_aplica
.x:
        ret
df_gancho:
        cmp eax, DM_INICIO
        je .ini
        cmp eax, WM_COMMAND
        jne .no
        cmp ecx, ID_ACEPTAR
        jne .no
        pushad
        xor edi, edi                ; EDI=el formato que sale
        mov ecx, 40                 ; la clase del número: la opción marcada
        xor ebx, ebx
.g:
        call marca_lee
        jz .g1
        mov edi, ebx
        shl edi, F_NUM_D
.g1:
        inc ebx
        inc ecx
        cmp ecx, 46
        jb .g
        mov ecx, 50                 ; los decimales
        mov eax, WM_GETTEXT
        mov ebx, 8
        mov edx, num_txt
        call hija_msg
        mov esi, num_txt
        call hvl_lee_num
        cmp eax, CU_DEC
        jbe .g2
        mov eax, CU_DEC
.g2:
        shl eax, F_DEC_D
        or edi, eax
        mov ecx, 20
        call marca_lee
        jz .g3
        or edi, F_NEG
.g3:
        mov ecx, 21
        call marca_lee
        jz .g4
        or edi, F_CUR
.g4:
        mov ecx, 60                 ; la alineación y el color, de sus listas
        mov eax, LM_SELEC
        call hija_msg
        test eax, eax
        js .g5
        and eax, 3
        shl eax, F_ALIN_D
        or edi, eax
.g5:
        mov ecx, 61
        mov eax, LM_SELEC
        call hija_msg
        test eax, eax
        js .g6
        and eax, 7
        shl eax, F_COLOR_D
        or edi, eax
.g6:
        mov [df_fmt], edi
        popad
        jmp .no
.ini:
        pushad
        mov edi, [df_fmt]
        mov ecx, edi                ; la clase del número
        shr ecx, F_NUM_D
        and ecx, 7
        cmp ecx, 5
        jbe .i1
        xor ecx, ecx
.i1:
        add ecx, 40
        mov edx, 1
        call marca_pon
        mov eax, edi                ; los decimales
        shr eax, F_DEC_D
        and eax, 15
        push edi
        mov edi, num_txt
        call hvl_num
        mov byte [edi], 0
        pop edi
        mov ecx, 50
        mov eax, WM_SETTEXT
        mov edx, num_txt
        call hija_msg
        mov ecx, 20
        mov edx, edi
        and edx, F_NEG
        call marca_pon
        mov ecx, 21
        mov edx, edi
        and edx, F_CUR
        shr edx, 1
        call marca_pon
        mov ecx, 60                 ; las listas
        mov esi, t_alins
        call .lista
        mov ebx, edi
        shr ebx, F_ALIN_D
        and ebx, 3
        mov eax, LM_PONER_SELEC
        call hija_msg
        mov ecx, 61
        mov esi, t_colores
        call .lista
        mov ebx, edi
        shr ebx, F_COLOR_D
        and ebx, 7
        mov eax, LM_PONER_SELEC
        call hija_msg
        popad
.no:
        xor eax, eax
        ret
; (interna) llena la lista ECX con los textos de ESI (seguidos; acaba uno vacío)
.lista:
        cmp byte [esi], 0
        je .lf
        mov eax, LM_ANADIR
        mov edx, esi
        call hija_msg
.l1:
        lodsb
        test al, al
        jnz .l1
        jmp .lista
.lf:
        ret

; EBP=el diálogo ECX=el número de una hija: le manda el mensaje EAX con
; EBX y EDX -> EAX. Conserva los demás registros
hija_msg:
        push ebx
        push ecx
        push edx
        push esi
        push edi
        push ebp
        push eax
        push edx
        HV ven_hija_id
        mov ebp, eax
        pop edx
        pop eax
        mov ecx, ebx
        HV msg_enviar
        pop ebp
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        ret
; la casilla o la opción ECX del diálogo EBP: ZF=0 si está marcada
marca_lee:
        push eax
        push ebx
        mov eax, BM_MARCA
        call hija_msg
        test eax, eax
        pop ebx
        pop eax
        ret
; ...y la marca (EDX distinto de 0) o la desmarca
marca_pon:
        push eax
        push ebx
        xor ebx, ebx
        test edx, edx
        jz .a
        inc ebx
.a:
        mov eax, BM_PONER_MARCA
        call hija_msg
        pop ebx
        pop eax
        ret

; ---- Formato > Ancho de columna: el de las columnas elegidas, en letras ----
ancho_col:
        mov ecx, [cur_col]
        movzx eax, word [col_an+ecx*2]
        sub eax, 8
        jns .a
        xor eax, eax
.a:
        shr eax, 3
        mov edi, num_txt
        call hvl_num
        mov byte [edi], 0
        mov ebp, [ven]
        mov esi, t_q_ancho
        mov edi, t_ancho
        mov ebx, num_txt
        mov ecx, 4
        HV dlg_pedir
        test eax, eax
        jz .x
        mov esi, num_txt
        call hvl_lee_num
        test eax, eax
        jz .x
        cmp eax, 59
        jbe .b
        mov eax, 59
.b:
        lea eax, [eax*8+8]
        call instantanea
        call sel_rect
        mov ecx, [r_c1]
.c:
        mov [col_an+ecx*2], ax
        inc ecx
        cmp ecx, [r_c2]
        jbe .c
        call cuenta_cols
        call invalida
.x:
        ret

; ---- Formato > Moneda: lo que va detrás de las cantidades ----
simbolo:
        mov eax, [moneda_txt]
        mov [num_txt], eax
        mov eax, [moneda_txt+4]
        mov [num_txt+4], eax
        mov ebp, [ven]
        mov esi, t_q_simbolo
        mov edi, t_simbolo
        mov ebx, num_txt
        mov ecx, 8
        HV dlg_pedir
        test eax, eax
        jz .x
        call instantanea
        xor eax, eax
        mov [moneda_txt], eax
        mov [moneda_txt+4], eax
        mov esi, num_txt
        mov edi, moneda_txt
        mov ecx, 7
.c:
        lodsb
        test al, al
        jz .f
        stosb
        dec ecx
        jnz .c
.f:
        call invalida
.x:
        ret

; =============================================================================
;  Lo demás: lo que se ve, la hoja, los archivos, el cálculo y lo común
; =============================================================================
%include "vista.inc"
%include "hoja.inc"
%include "archivo.inc"
%ifdef CON_GRAFICOS
%include "grafico.inc"
%endif
; el cálculo se prueba aparte, contra un modelo exacto, con
; tests/tools/cuentas_calc.py
%include "calculo.inc"
%include "barra.inc"
%include "hvplib.inc"

; =============================================================================
;  Variables
; =============================================================================
        section .bss
        alignb 4
ven         resd 1                  ; la ventana
puente      resd 1                  ; el cuadro de texto escondido
barra_v     resd 1                  ; las barras de desplazamiento
barra_h     resd 1
param       resd 1                  ; parámetros del acceso directo
cur_col     resd 1                  ; la celda activa
cur_fil     resd 1
sel_c2      resd 1                  ; la otra esquina de lo elegido (si es
sel_f2      resd 1                  ; una celda sola, la misma)
desp_col    resd 1                  ; la primera columna y la primera fila
desp_fil    resd 1                  ; que se ven
tec_may     resd 1                  ; las mayúsculas de la última tecla o clic
edt_pos     resd 1                  ; el cursor de lo que se escribe
arr_col     resd 1                  ; la columna cuyo ancho se arrastra
arr_x0      resd 1                  ; y dónde empieza
forma       resd 1                  ; la forma del cursor del ratón
df_fmt      resd 1                  ; el diálogo del formato: el que sale
editando    resb 1                  ; se está escribiendo en la celda activa
edt_modo    resb 1                  ; 1: las flechas mueven el cursor (F2)
modif       resb 1                  ; hay cambios sin guardar
arrastra    resb 1                  ; con el botón apretado: 1 celdas, 2 el
                                    ; ancho de una columna, 3 columnas, 4 filas
            alignb 4
num_txt     resb 16
edt_buf     resb CEL_MAX + 4        ; lo que se escribe
nombre      resb 80                 ; el nombre del archivo
nom_tmp     resb 80
titulo      resb 120
BARRA_BSS

HVP_FIN
