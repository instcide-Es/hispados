; =============================================================================
;  PLUMA.HVP - Pluma, el procesador de textos de Hispa Ventanas
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Un procesador de textos: se escribe sobre una hoja de 72 letras de ancho y
;  las líneas se parten solas por las palabras. Cada letra lleva su formato:
;  negrita, cursiva, subrayado, tamaño (normal o grande, el doble) y color; y
;  cada párrafo, su alineación (izquierda, centro o derecha). El formato se da
;  a lo elegido (con Mayús y las flechas o arrastrando el ratón) o, si no hay
;  nada elegido, a lo que se escriba a continuación.
;
;  Los menús siguen el orden de siempre en un procesador de textos (Archivo,
;  Edición, Ver, Insertar, Formato, Herramientas y «?»), con lo que Pluma
;  sabe hacer hoy; arriba, dos filas de botones: el archivo y la edición, y
;  el formato, con iconos (y, al pasar el ratón por un botón, abajo sale lo
;  que hace). Los párrafos pueden ir justificados, con sangría, con viñetas o
;  numerados; «¶» enseña los espacios y los finales de párrafo; la brocha
;  copia el formato de un texto a otro. Hay tres tipos de letra: «Hispa» (la
;  de Hispa Ventanas, de ancho fijo), «Roble» (con remates) y «Llano» (de palo
;  seco), las dos de ancho variable (programas/pluma/*.inc, hechos con
;  tools/hazfuente.py). Además de lo de abajo: Reemplazar, Ir a, Repetir, insertar la
;  fecha o un archivo, los diálogos de Fuentes y Párrafo, cambiar mayúsculas
;  y minúsculas, contar palabras y pantalla completa.
;
;  Archivo: Nuevo, Abrir, Guardar, Guardar como e Imprimir. Los documentos se
;  guardan en .PLU (el texto y, detrás, el formato de cada letra); con la
;  extensión .TXT se guarda solo el texto, y cualquier archivo de texto se
;  puede abrir. Edición: cortar, copiar y pegar (con el portapapeles de Hispa
;  Ventanas: el texto va y viene del Bloc de notas; dentro de Pluma conserva
;  el formato), deshacer el último cambio, seleccionar todo y buscar. Se imprime por el puerto paralelo
;  (INT 17h), en texto llano o con formato (códigos ESC/P de Epson).
;
;  Cómo está hecho. El documento son dos tablas parejas, txt (una letra por
;  octeto, página 437; el 10 acaba el párrafo) y atr (su formato). El último
;  octeto es siempre un 10 que no se puede borrar: así todo párrafo tiene su
;  marca, y en el atr de la marca va la alineación. Tras cada cambio se
;  recompone la tabla de líneas (dónde empieza cada una y a qué altura). Cada
;  línea se dibuja entera en un mapa de puntos propio (con la fuente de Hispa
;  Ventanas, que el módulo recibe como datos) y se copia a la ventana de una
;  vez, así que no parpadea.
;
;  Parámetros del acceso directo: el nombre de un documento, que se abre.
; =============================================================================
%include "hvp.inc"

MAX_TXT     equ 30000               ; letras de un documento
PAG_AN      equ 576                 ; ancho del texto: 72 letras normales
PX          equ 4                   ; dónde empieza la hoja
MARG        equ 8                   ; su margen a cada lado
PAPW        equ PAG_AN + 2 * MARG
TOPM        equ 12                  ; el margen de arriba (y de abajo)
TB_AL       equ 58                  ; la barra de botones: dos filas
STAT_AL     equ 20                  ; la barra de estado
TX0         equ PX + MARG           ; la x del texto en la ventana

; ---- el formato de una letra ----
A_NEG       equ 1
A_CUR       equ 2
A_SUB       equ 4
A_GRA       equ 8                   ; grande: el doble de ancha y de alta
A_COLOR     equ 0x30                ; el color: 0-3 en los bits 4-5
A_FUENTE    equ 0xC0                ; el tipo de letra: 0-3 en los bits 6-7
N_FUENTES   equ 3                   ; (0: la de Hispa Ventanas; 1 Roble; 2 Llano)
; En la marca de párrafo (el 10), el octeto dice cómo es el párrafo:
;   bits 0-1  la alineación: 0 izquierda, 1 centro, 2 derecha, 3 justificado
;   bit 2     con viñeta          bit 7     numerado
;   bit 3     A_GRA, para la línea vacía
;   bits 4-6  la sangría: de 0 a 7 pasos de cuatro letras
; (Un Pluma anterior que abra el documento se queda con la alineación y pierde
; lo demás, sin estropear nada.)
P_VIN       equ 0x04
P_NUM       equ 0x80
P_SANGRIA   equ 0x70
SANGRIA_PX  equ 32                  ; un paso de sangría
MARCA_PX    equ 24                  ; el sitio de la viñeta o del número

ID_PUENTE   equ 900
ID_BARRA    equ 901

; ---- órdenes ----
O_NUEVO     equ 101
O_ABRIR     equ 102
O_GUARDAR   equ 103
O_COMO      equ 104
O_IMPRIMIR  equ 105
O_IMPRIMIRF equ 106
O_SALIR     equ 107
O_CERRAR    equ 108
O_RESUMEN   equ 109
O_CORTAR    equ 111
O_COPIAR    equ 112
O_PEGAR     equ 113
O_TODO      equ 114
O_BUSCAR    equ 115
O_SIGUIENTE equ 116
O_DESHACER  equ 117
O_REHACER   equ 118
O_BORRAR    equ 119
O_NEG       equ 121
O_CUR       equ 122
O_SUB       equ 123
O_GRANDE    equ 124
O_COLOR     equ 130                 ; 130-133: negro, rojo, azul, verde
O_IZQ       equ 141
O_CEN       equ 142
O_DER       equ 143
O_JUST      equ 144
O_ACERCA    equ 151
O_REEMPLAZAR equ 161
O_IRA       equ 162
O_FECHA     equ 163
O_INSARCH   equ 164
O_FUENTES   equ 165
O_PARRAFO   equ 166
O_MAYMIN    equ 167
O_COMPLETA  equ 168
O_BROCHA    equ 169
O_MARCAS    equ 170
O_VINETAS   equ 171
O_NUMERA    equ 172
O_SANG_MAS  equ 173
O_SANG_MENOS equ 174
O_TIPO      equ 175                 ; el tipo de letra siguiente
O_FUENTE    equ 176                 ; +0..2: ese tipo de letra

HVP_CABECERA "Pluma", entrada, 4
        db "................................"
        db "................................"
        db "................................"
        db ".....................0222220...."
        db "....................0222220....."
        db "...................0222220......"
        db "..................0222220......."
        db ".................fffffff........"
        db "................0ceccc0........."
        db "...............0ceccc0.........."
        db "..............0ceccc0..........."
        db ".............0ceccc0............"
        db "............0ceccc0............."
        db "...........0ceccc0.............."
        db "..........0ceccc0..............."
        db ".........0ceccc0................"
        db "........0ceccc0................."
        db ".......0ceccc0.................."
        db "......0ceccc0..................."
        db ".....0ceccc0...................."
        db "....0ceccc0....................."
        db "...0ceccc0......................"
        db "..0ceccc0......................."
        db ".0ceccc0........................"
        db "0ff0ff0........................."
        db "0f0f0..........................."
        db "f0f0............................"
        db "000............................."
        db "00.............................."
        db "00.............................."
        db "...ce..........................."
        db "...c............................"
HVP_CODIGO

c_pluma     dd plu_proc
            db SIN_FONDO, 0, 0, 0   ; lo pinta todo ella: sin parpadeo
plu_plant:
        PLANTILLA c_pluma, titulo, E_NORMAL, 70, 40, 624, 500, 0
hijos:
        PLANTILLA HV_c_edit, 0, E_HIJA | E_OCULTA | ES_MULTI, 0, 0, 40, 40, ID_PUENTE
        PLANTILLA HV_c_barra, 0, E_HIJA, 0, 0, BD_AN, 50, ID_BARRA
        dd 0

plu_menu:
        db "&Archivo", 0
        dw O_NUEVO
        db "&Nuevo                  Ctrl+U", 0
        dw O_ABRIR
        db "&Abrir...               Ctrl+A", 0
        dw O_CERRAR
        db "&Cerrar", 0
        dw 0
        db 0
        dw O_GUARDAR
        db "&Guardar                Ctrl+G", 0
        dw O_COMO
        db "Guardar c&omo...", 0
        dw 0
        db 0
        dw O_RESUMEN
        db "&Resumen...", 0
        dw 0
        db 0
        dw O_IMPRIMIR
        db "&Imprimir (texto llano) Ctrl+P", 0
        dw O_IMPRIMIRF
        db "Imprimir con &formato (ESC/P)", 0
        dw 0
        db 0
        dw O_SALIR
        db "&Salir", 0
        dw 0xFFFF
        db "&Edición", 0
        dw O_DESHACER
        db "&Deshacer          Ctrl+Z", 0
        dw O_REHACER
        db "&Repetir           Ctrl+Y", 0
        dw 0
        db 0
        dw O_CORTAR
        db "Cor&tar            Ctrl+X", 0
        dw O_COPIAR
        db "&Copiar            Ctrl+C", 0
        dw O_PEGAR
        db "&Pegar             Ctrl+V", 0
        dw O_BORRAR
        db "B&orrar              Supr", 0
        dw O_TODO
        db "&Seleccionar todo  Ctrl+E", 0
        dw 0
        db 0
        dw O_BUSCAR
        db "&Buscar...         Ctrl+B", 0
        dw O_SIGUIENTE
        db "Buscar si&guiente      F3", 0
        dw O_REEMPLAZAR
        db "Reemp&lazar...     Ctrl+L", 0
        dw O_IRA
        db "&Ir a...           Ctrl+I", 0
        dw 0xFFFF
        db "&Ver", 0
        dw O_MARCAS
        db "&Mostrar las marcas (¶)", 0
        dw O_COMPLETA
        db "&Pantalla completa", 0
        dw 0xFFFF
        db "&Insertar", 0
        dw O_FECHA
        db "&Fecha y hora", 0
        dw O_INSARCH
        db "&Archivo...", 0
        dw 0xFFFF
        db "&Formato", 0
        dw O_FUENTES
        db "&Fuentes...", 0
        dw O_PARRAFO
        db "&Párrafo...", 0
        dw O_MAYMIN
        db "Cambiar &mayús/minús", 0
        dw 0
        db 0
        dw O_NEG
        db "&Negrita           Ctrl+N", 0
        dw O_CUR
        db "&Cursiva           Ctrl+K", 0
        dw O_SUB
        db "&Subrayado         Ctrl+S", 0
        dw O_GRANDE
        db "Letra &grande      Ctrl+W", 0
        dw 0
        db 0
        dw O_IZQ
        db "A la &izquierda    Ctrl+Q", 0
        dw O_CEN
        db "C&entrado          Ctrl+T", 0
        dw O_DER
        db "A la &derecha      Ctrl+D", 0
        dw O_JUST
        db "&Justificado       Ctrl+J", 0
        dw 0
        db 0
        dw O_VINETAS
        db "&Viñetas", 0
        dw O_NUMERA
        db "Nume&ración", 0
        dw O_SANG_MAS
        db "A&umentar sangría", 0
        dw O_SANG_MENOS
        db "Dismin&uir sangría", 0
        dw 0
        db 0
        dw O_FUENTE
        db "Letra &Hispa", 0
        dw O_FUENTE + 1
        db "Letra R&oble", 0
        dw O_FUENTE + 2
        db "Letra &Llano", 0
        dw 0
        db 0
        dw O_BROCHA
        db "Copiar &formato", 0
        dw 0xFFFF
        db "&Herramientas", 0
        dw O_RESUMEN
        db "&Contar palabras...", 0
        dw 0xFFFF
        db "&?", 0
        dw O_ACERCA
        db "&Acerca de Pluma...", 0
        dw 0xFFFF
        db 0

; ---- diálogo: las fuentes ----
dlg_fue:
        PLANTILLA HV_c_dialogo, t_d_fue, E_DIALOGO, 0, 0, 320, 324, 0
        PLANTILLA HV_c_estatico, t_f_tipo, E_HIJA, 20, 196, 200, 18, 101
        PLANTILLA HV_c_boton, t_fu0, E_HIJA | E_TABULA | BS_OPCION, 20, 218, 90, 20, 50
        PLANTILLA HV_c_boton, t_fu1, E_HIJA | E_TABULA | BS_OPCION, 116, 218, 90, 20, 51
        PLANTILLA HV_c_boton, t_fu2, E_HIJA | E_TABULA | BS_OPCION, 212, 218, 90, 20, 52
        PLANTILLA HV_c_boton, t_f_neg, E_HIJA | E_TABULA | BS_CASILLA, 20, 14, 200, 20, 20
        PLANTILLA HV_c_boton, t_f_cur, E_HIJA | E_TABULA | BS_CASILLA, 20, 38, 200, 20, 21
        PLANTILLA HV_c_boton, t_f_sub, E_HIJA | E_TABULA | BS_CASILLA, 20, 62, 200, 20, 22
        PLANTILLA HV_c_boton, t_f_gra, E_HIJA | E_TABULA | BS_CASILLA, 20, 86, 280, 20, 23
        PLANTILLA HV_c_estatico, t_f_color, E_HIJA, 20, 118, 100, 18, 100
        PLANTILLA HV_c_boton, t_f_c0, E_HIJA | E_TABULA | BS_OPCION, 20, 140, 120, 20, 30
        PLANTILLA HV_c_boton, t_f_c1, E_HIJA | E_TABULA | BS_OPCION, 160, 140, 120, 20, 31
        PLANTILLA HV_c_boton, t_f_c2, E_HIJA | E_TABULA | BS_OPCION, 20, 164, 120, 20, 32
        PLANTILLA HV_c_boton, t_f_c3, E_HIJA | E_TABULA | BS_OPCION, 160, 164, 120, 20, 33
        PLANTILLA HV_c_boton, t_aceptar, E_HIJA | E_TABULA | BS_DEFECTO, 60, 260, 90, 26, ID_ACEPTAR
        PLANTILLA HV_c_boton, t_cancelar, E_HIJA | E_TABULA, 160, 260, 90, 26, ID_CANCELAR
        dd 0
; ---- diálogo: el párrafo ----
dlg_par:
        PLANTILLA HV_c_dialogo, t_d_par, E_DIALOGO, 0, 0, 280, 208, 0
        PLANTILLA HV_c_boton, t_p_izq, E_HIJA | E_TABULA | BS_OPCION, 20, 14, 220, 20, 40
        PLANTILLA HV_c_boton, t_p_cen, E_HIJA | E_TABULA | BS_OPCION, 20, 38, 220, 20, 41
        PLANTILLA HV_c_boton, t_p_der, E_HIJA | E_TABULA | BS_OPCION, 20, 62, 220, 20, 42
        PLANTILLA HV_c_boton, t_p_just, E_HIJA | E_TABULA | BS_OPCION, 20, 86, 220, 20, 43
        PLANTILLA HV_c_boton, t_aceptar, E_HIJA | E_TABULA | BS_DEFECTO, 40, 128, 90, 26, ID_ACEPTAR
        PLANTILLA HV_c_boton, t_cancelar, E_HIJA | E_TABULA, 140, 128, 90, 26, ID_CANCELAR
        dd 0

; ---- los botones de la barra, en dos filas: arriba, el archivo y la edición;
;      abajo, el formato. De cada uno: x, ancho, su icono (o su texto), la
;      orden, su nombre (sale abajo al pasar el ratón) y si es icono (1) ----
            align 4
botones1    dd 6, 24, ic_nuevo, O_NUEVO, n_nuevo, 1
            dd 32, 24, ic_abrir, O_ABRIR, n_abrir, 1
            dd 58, 24, ic_guardar, O_GUARDAR, n_guardar, 1
            dd 90, 24, ic_imprimir, O_IMPRIMIR, n_imprimir, 1
            dd 122, 24, ic_cortar, O_CORTAR, n_cortar, 1
            dd 148, 24, ic_copiar, O_COPIAR, n_copiar, 1
            dd 174, 24, ic_pegar, O_PEGAR, n_pegar, 1
            dd 200, 24, ic_brocha, O_BROCHA, n_brocha, 1
            dd 232, 24, ic_deshacer, O_DESHACER, n_deshacer, 1
            dd 258, 24, ic_rehacer, O_REHACER, n_rehacer, 1
            dd 290, 24, ic_marcas, O_MARCAS, n_marcas, 1
            dd 0, 0, 0, 0, 0, 0
botones     dd 6, 64, t_fu0, O_TIPO, n_tipo, 0
            dd 74, 24, t_b_n, O_NEG, n_neg, 0
            dd 100, 24, t_b_k, O_CUR, n_cur, 0
            dd 126, 24, t_b_s, O_SUB, n_sub, 0
            dd 154, 60, t_b_grande, O_GRANDE, n_grande, 0
            dd 222, 24, ic_izq, O_IZQ, n_izq, 1
            dd 248, 24, ic_cen, O_CEN, n_cen, 1
            dd 274, 24, ic_der, O_DER, n_der, 1
            dd 300, 24, ic_just, O_JUST, n_just, 1
            dd 332, 24, ic_numera, O_NUMERA, n_numera, 1
            dd 358, 24, ic_vinetas, O_VINETAS, n_vinetas, 1
            dd 384, 24, ic_sang_menos, O_SANG_MENOS, n_sang_menos, 1
            dd 410, 24, ic_sang_mas, O_SANG_MAS, n_sang_mas, 1
            dd 444, 22, t_nada, O_COLOR, n_c0, 0
            dd 468, 22, t_nada, O_COLOR + 1, n_c1, 0
            dd 492, 22, t_nada, O_COLOR + 2, n_c2, 0
            dd 516, 22, t_nada, O_COLOR + 3, n_c3, 0
            dd 0, 0, 0, 0, 0, 0
B_TAM       equ 24                  ; lo que ocupa cada botón en la tabla
n_nuevo     db "Nuevo: un documento en blanco (Ctrl+U)", 0
n_abrir     db "Abrir un documento guardado (Ctrl+A)", 0
n_guardar   db "Guardar el documento (Ctrl+G)", 0
n_imprimir  db "Imprimir (Ctrl+P)", 0
n_cortar    db "Cortar lo elegido (Ctrl+X)", 0
n_copiar    db "Copiar lo elegido (Ctrl+C)", 0
n_pegar     db "Pegar (Ctrl+V)", 0
n_brocha    db "Copiar formato: pulse y luego elija el texto que lo recibe", 0
n_deshacer  db "Deshacer (Ctrl+Z)", 0
n_rehacer   db "Rehacer lo deshecho (Ctrl+Y)", 0
n_marcas    db "Mostrar u ocultar los espacios y las marcas de párrafo", 0
n_tipo      db "Tipo de letra: pulse para pasar al siguiente (Hispa, Roble, Llano)", 0
n_neg       db "Negrita (Ctrl+N)", 0
n_cur       db "Cursiva (Ctrl+K)", 0
n_sub       db "Subrayado (Ctrl+S)", 0
n_grande    db "Letra grande, el doble (Ctrl+W)", 0
n_izq       db "Alinear a la izquierda (Ctrl+Q)", 0
n_cen       db "Centrar (Ctrl+T)", 0
n_der       db "Alinear a la derecha (Ctrl+D)", 0
n_just      db "Justificar: los dos márgenes rectos (Ctrl+J)", 0
n_numera    db "Numeración: una lista con números", 0
n_vinetas   db "Viñetas: una lista con puntos", 0
n_sang_menos db "Disminuir la sangría", 0
n_sang_mas  db "Aumentar la sangría", 0
n_c0        db "Letra negra", 0
n_c1        db "Letra roja", 0
n_c2        db "Letra azul", 0
n_c3        db "Letra verde", 0
; los signos que la fuente no trae, de 8x16: la viñeta, el punto de un espacio
; y el calderón del final de párrafo
m_vineta    db 0, 0, 0, 0, 0, 0, 0x3C, 0x7E, 0x7E, 0x7E, 0x7E, 0x3C, 0, 0, 0, 0
m_punto     db 0, 0, 0, 0, 0, 0, 0, 0, 0x18, 0x18, 0, 0, 0, 0, 0, 0
m_calderon  db 0, 0, 0x7E, 0xF4, 0xF4, 0xF4, 0x74, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0, 0, 0
ic_marcas:
        db "................"
        db "................"
        db "....000000000..."
        db "...0000.00......"
        db "..00000.00......"
        db "..00000.00......"
        db "..00000.00......"
        db "...0000.00......"
        db "....000.00......"
        db "......0.00......"
        db "......0.00......"
        db "......0.00......"
        db "......0.00......"
        db "......0.00......"
        db "................"
        db "................"
; ---- los iconos: 16x16, un carácter por punto («.» deja ver el botón; las
;      cifras y las letras, los colores de la paleta) ----
ic_nuevo:
        db "................"
        db "...00000000....."
        db "...099999900...."
        db "...0999999000..."
        db "...0999999990..."
        db "...0999999990..."
        db "...0999999990..."
        db "...0999999990..."
        db "...0999999990..."
        db "...0999999990..."
        db "...0999999990..."
        db "...0999999990..."
        db "...0999999990..."
        db "...0999999990..."
        db "...0000000000..."
        db "................"
ic_abrir:
        db "................"
        db "................"
        db "................"
        db "..000000........"
        db "..0aaaa0........"
        db ".00000000000000."
        db ".0aaaaaaaaaaaa0."
        db ".09999999999990."
        db ".0aaaaaaaaaaaa0."
        db ".0aaaaaaaaaaaa0."
        db ".0aaaaaaaaaaaa0."
        db ".0aaaaaaaaaaaa0."
        db ".00000000000000."
        db "................"
        db "................"
        db "................"
ic_guardar:
        db "................"
        db "................"
        db "..000000000000.."
        db "..0c09999990c0.."
        db "..0c09999990c0.."
        db "..0c09999990c0.."
        db "..0c00000000c0.."
        db "..0cccccccccc0.."
        db "..0cccccccccc0.."
        db "..0cc000000cc0.."
        db "..0cc0ff0f0cc0.."
        db "..0cc0ff0f0cc0.."
        db "..0cc0ffff0cc0.."
        db "..000000000000.."
        db "................"
        db "................"
ic_imprimir:
        db "................"
        db "....00000000...."
        db "....09999990...."
        db "....09999990...."
        db "....09999990...."
        db "....09999990...."
        db ".00000000000000."
        db ".0ffffffffffff0."
        db ".0ffffffffffbf0."
        db ".0ffffffffffff0."
        db ".0ff00000000ff0."
        db ".00009999990000."
        db "....09777790...."
        db "....09999990...."
        db "....00000000...."
        db "................"
ic_cortar:
        db "................"
        db "....0......0...."
        db ".....0....0....."
        db "......0...0....."
        db "......0..0......"
        db ".......00......."
        db ".......00......."
        db "......0.0......."
        db "......0..0......"
        db ".....0....0....."
        db "...dd......dd..."
        db "..d..d....d..d.."
        db "..d..d....d..d.."
        db "...dd......dd..."
        db "................"
        db "................"
ic_copiar:
        db "................"
        db "..00000000......"
        db "..09999990......"
        db "..09999990......"
        db "..09999990......"
        db "..099900000000.."
        db "..099909999990.."
        db "..099909999990.."
        db "..099909777790.."
        db "..099909999990.."
        db "..000009777790.."
        db "......09999990.."
        db "......09777790.."
        db "......09999990.."
        db "......00000000.."
        db "................"
ic_pegar:
        db "................"
        db "......0000......"
        db "..00000ff00000.."
        db "..0aaa0000aaa0.."
        db "..0aaaaaaaaaa0.."
        db "..0aaaaaaaaaa0.."
        db "..0aa000000000.."
        db "..0aa099999900.."
        db "..0aa097777900.."
        db "..0aa099999900.."
        db "..0aa097777900.."
        db "..0aa099999900.."
        db "..0aa099999900.."
        db "..0aa000000000.."
        db "..000000000000.."
        db "................"
ic_brocha:
        db "................"
        db ".............1.."
        db "............111."
        db "...........111.."
        db "..........111..."
        db ".........111...."
        db "........111....."
        db "....0000.1......"
        db "...0aaaa0......."
        db "..0aaaaa0......."
        db ".0aaaaa0........"
        db ".0aaaa0........."
        db ".0aaa0.........."
        db "..000..........."
        db "................"
        db "................"
ic_deshacer:
        db "................"
        db "................"
        db ".....00000......"
        db "...00ccccc00...."
        db "..0cc00000cc0..."
        db "..0c0.....0c0..."
        db ".0c0.......0c0.."
        db "00c00......0c0.."
        db "0ccc0......0c0.."
        db ".0c0.......0c0.."
        db "..0.......0c0..."
        db ".........0c0...."
        db ".......00c0....."
        db ".......000......"
        db "................"
        db "................"
ic_rehacer:
        db "................"
        db "................"
        db "......00000....."
        db "....00ccccc00..."
        db "...0cc00000cc0.."
        db "...0c0.....0c0.."
        db "..0c0.......0c0."
        db "..0c0......00c00"
        db "..0c0......0ccc0"
        db "..0c0.......0c0."
        db "...0c0.......0.."
        db "....0c0........."
        db ".....0c00......."
        db "......000......."
        db "................"
        db "................"
ic_izq:
        db "................"
        db "................"
        db "..00000000000..."
        db "................"
        db "..0000000......."
        db "................"
        db "..00000000000..."
        db "................"
        db "..00000........."
        db "................"
        db "..00000000000..."
        db "................"
        db "..00000000......"
        db "................"
        db "................"
        db "................"
ic_cen:
        db "................"
        db "................"
        db "..000000000000.."
        db "................"
        db "....00000000...."
        db "................"
        db "..000000000000.."
        db "................"
        db ".....000000....."
        db "................"
        db "..000000000000.."
        db "................"
        db "....00000000...."
        db "................"
        db "................"
        db "................"
ic_der:
        db "................"
        db "................"
        db "...00000000000.."
        db "................"
        db ".......0000000.."
        db "................"
        db "...00000000000.."
        db "................"
        db ".........00000.."
        db "................"
        db "...00000000000.."
        db "................"
        db "......00000000.."
        db "................"
        db "................"
        db "................"
ic_just:
        db "................"
        db "................"
        db "..000000000000.."
        db "................"
        db "..000000000000.."
        db "................"
        db "..000000000000.."
        db "................"
        db "..000000000000.."
        db "................"
        db "..000000000000.."
        db "................"
        db "..000000000000.."
        db "................"
        db "................"
        db "................"
ic_numera:
        db "................"
        db "................"
        db "..c............."
        db ".cc...00000000.."
        db "..c............."
        db "................"
        db ".cc............."
        db "..cc..00000000.."
        db ".cc............."
        db "................"
        db ".ccc............"
        db "..cc..00000000.."
        db ".ccc............"
        db "................"
        db "................"
        db "................"
ic_vinetas:
        db "................"
        db "................"
        db "..cc............"
        db "..cc..00000000.."
        db "................"
        db "................"
        db "..cc............"
        db "..cc..00000000.."
        db "................"
        db "................"
        db "..cc............"
        db "..cc..00000000.."
        db "................"
        db "................"
        db "................"
        db "................"
ic_sang_mas:
        db "................"
        db "................"
        db "..000000000000.."
        db "................"
        db "................"
        db ".......0000000.."
        db ".c.............."
        db ".cc............."
        db ".cccc..0000000.."
        db ".cc............."
        db ".c.............."
        db ".......0000000.."
        db "................"
        db "................"
        db "..000000000000.."
        db "................"
ic_sang_menos:
        db "................"
        db "................"
        db "..000000000000.."
        db "................"
        db "................"
        db ".......0000000.."
        db "....c..........."
        db "...cc..........."
        db ".cccc..0000000.."
        db "...cc..........."
        db "....c..........."
        db ".......0000000.."
        db "................"
        db "................"
        db "..000000000000.."
        db "................"
; los colores de la letra (de la paleta de Hispa Ventanas)
colores     db C_TINTA, C_GRANATE, C_ANIL, C_OLIVA, C_TERRACOTA, C_PETROLEO, C_MARRON, C_PIEDRA

; ---- textos ----
t_pluma     db "Pluma", 0
t_guion     db " - ", 0
t_sin_nombre db "(sin nombre)", 0
t_nada      db 0
t_b_n       db "N", 0
t_b_k       db "K", 0
t_b_s       db "S", 0
t_b_grande  db "Grande", 0
t_b_izq     db "Izda.", 0
t_b_cen     db "Centro", 0
t_b_der     db "Dcha.", 0
t_linea     db "Línea ", 0
t_columna   db ", columna ", 0
t_letras    db " letras", 0
t_de        db " de ", 0
t_ext_plu   db ".PLU", 0
t_abrir     db "Abrir", 0
t_guardar_como db "Guardar como", 0
t_buscar    db "Buscar", 0
t_q_buscar  db "Texto que se busca:", 0
t_no_esta   db "No se encuentra ese texto.", 0
t_cambios   db "El documento tiene cambios sin guardar.", 10, "¿Quiere guardarlos?", 0
t_no_abrir  db "No se ha podido abrir el archivo.", 0
t_no_guardar db "No se ha podido guardar el documento.", 0
t_largo     db "El documento es demasiado largo para Pluma:", 10
            db "se han abierto solo las primeras 30.000 letras.", 0
t_lleno     db "No cabe más texto en el documento (30.000 letras).", 0
t_impreso   db "El documento se ha enviado a la impresora.", 0
t_e_imprimir db "No se ha podido imprimir.", 10
            db "Compruebe que la impresora (la del puerto paralelo, LPT1)", 10
            db "está encendida, en línea y con papel.", 0
t_aceptar   db "Aceptar", 0
t_cancelar  db "Cancelar", 0
t_d_fue     db "Fuentes", 0
t_f_neg     db "Negrita", 0
t_f_cur     db "Cursiva", 0
t_f_sub     db "Subrayado", 0
t_f_gra     db "Letra grande (el doble)", 0
t_f_color   db "Color:", 0
t_f_tipo    db "Tipo de letra:", 0
; los tipos de letra: su nombre y su tabla (0: la de Hispa Ventanas)
t_fu0       db "Hispa", 0
t_fu1       db "Roble", 0
t_fu2       db "Llano", 0
            align 4
fu_nombres  dd t_fu0, t_fu1, t_fu2, t_fu0
fu_tablas   dd 0, fu_roble, fu_llano, 0
t_f_c0      db "Negro", 0
t_f_c1      db "Rojo", 0
t_f_c2      db "Azul", 0
t_f_c3      db "Verde", 0
t_d_par     db "Párrafo", 0
t_p_izq     db "A la izquierda", 0
t_p_cen     db "Centrado", 0
t_p_der     db "A la derecha", 0
t_p_just    db "Justificado (los dos márgenes rectos)", 0
t_reemplazar db "Reemplazar", 0
t_q_por     db "Reemplazar por:", 0
t_ira       db "Ir a", 0
t_q_ira     db "Número de línea:", 0
t_insertar  db "Insertar un archivo de texto", 0
t_cambios_n db " cambios.", 0
t_un_cambio db "1 cambio.", 0
t_elija     db "Antes hay que elegir el texto (con Mayús y las flechas, o arrastrando el ratón).", 0
t_r_letras  db "Letras:    ", 0
t_r_palabras db 10, "Palabras:  ", 0
t_r_lineas  db 10, "Líneas:    ", 0
t_r_parrafos db 10, "Párrafos:  ", 0
t_resumen   db "Resumen del documento", 0
t_acerca_tit db "Acerca de Pluma", 0
t_acerca    db "Pluma", 10
            db "El procesador de textos de Hispa Ventanas:", 10
            db "negrita, cursiva, subrayado, letra grande, colores", 10
            db "y párrafos alineados, justificados, con sangría o en lista.", 10, 10
            db "Copyright (C) 2026 Carlos Sanz y contribuidores.", 10
            db "GPL-2.0 o posterior.", 0
firma       db "PLUMA1", 26, 0      ; el principio de un .PLU (8 octetos)

; =============================================================================
;  La entrada: EBX=base ESI=ruta del módulo EDI=parámetros
; =============================================================================
entrada:
        cld
        mov [param], edi
        mov esi, hijos
        call hvl_clases
        mov esi, dlg_fue
        call hvl_clases
        mov esi, dlg_par
        call hvl_clases
        call doc_vacio
        call pon_titulo
        HV pantalla                 ; EAX=ancho EBX=alto: que quepa
        sub ebx, BARRA_H + 12
        cmp ebx, 500
        jae .a
        mov [plu_plant+P_AL], ebx
        mov dword [plu_plant+P_Y], BARRA_H + 4
.a:
        sub eax, 624
        jae .b
        xor eax, eax
.b:
        shr eax, 1
        cmp eax, 70
        jae .c
        mov [plu_plant+P_X], eax
.c:
        mov esi, plu_plant
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

; el título: «Pluma - NOMBRE»
pon_titulo:
        pushad
        mov edi, titulo
        mov esi, t_pluma
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

; el documento, vacío: solo la marca del último párrafo
doc_vacio:
        mov dword [n], 1
        mov byte [txt], 10
        mov byte [atr], 0
        mov dword [cur], 0
        mov dword [anc], -1
        mov dword [sy], 0
        mov byte [at_act], 0
        mov byte [modif], 0
        mov byte [des_hay], 0
        mov byte [tecleando], 0
        jmp compone

; =============================================================================
;  El procedimiento de la ventana
; =============================================================================
plu_proc:
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
        je .barra
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
        mov eax, 624 + 200 * 65536
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
        mov dword [ebp+V_MENU], plu_menu
        mov dword [ebp+V_ICONO], hvp_icono
        mov esi, hijos
        HV ven_lista
        mov ecx, ID_PUENTE
        HV ven_hija_id
        mov [puente], eax
        mov ecx, ID_BARRA
        HV ven_hija_id
        mov [barra], eax
        push ebp
        mov ebp, eax
        mov eax, BDM_LINEA
        mov ecx, 16
        HV msg_enviar
        pop ebp
.tam:
        mov ebp, [ven]
        HV ven_cli                  ; ECX=ancho EDX=alto de la zona
        mov [cli_an], ecx
        mov [cli_al], edx
        sub edx, TB_AL + STAT_AL
        jns .t1
        xor edx, edx
.t1:
        mov [vh], edx
        mov eax, ecx
        sub eax, BD_AN
        mov ebx, TB_AL
        mov ecx, BD_AN
        mov ebp, [barra]
        test ebp, ebp
        jz hecho
        HV ven_mover
        call pon_barra
        call inval
        jmp hecho
.foco:
        mov byte [foco], 1
        call inval
        jmp hecho
.sin_foco:
        mov byte [foco], 0
        mov byte [arrastra], 0
        call inval
        jmp hecho
.rueda:
        mov eax, ecx
        shl eax, 5
        add eax, [sy]
        call desplaza
        jmp hecho
.barra:
        mov eax, ecx
        call desplaza
        jmp hecho
.cerrar:
        call preguntar
        jc hecho
        mov ebp, [ven]
        HV ven_destruir
        jmp hecho
.destruir:
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

; pone el desplazamiento en EAX (dentro de sus límites) y repinta
desplaza:
        pushad
        cmp eax, [sy_max]
        jle .a
        mov eax, [sy_max]
.a:
        test eax, eax
        jns .b
        xor eax, eax
.b:
        mov [sy], eax
        call pon_barra
        call inval
        popad
        ret

; la barra de desplazamiento, al día: lo que mide el documento y dónde está
pon_barra:
        pushad
        mov eax, [nl]
        mov eax, [lin_y+eax*4]
        add eax, 2 * TOPM
        sub eax, [vh]
        jns .a
        xor eax, eax
.a:
        mov [sy_max], eax
        cmp [sy], eax
        jle .b
        mov [sy], eax
.b:
        mov ebp, [barra]
        test ebp, ebp
        jz .x
        mov ecx, eax
        mov edx, [vh]
        mov eax, BDM_PONER
        HV msg_enviar
        mov eax, BDM_POS
        mov ecx, [sy]
        HV msg_enviar
.x:
        popad
        ret

; =============================================================================
;  Las órdenes: ECX=número
; =============================================================================
orden:
        call orden_haz
        jmp hecho
orden_haz:
        mov dword [bt_sobre], 0
        mov byte [tecleando], 0     ; (cualquier orden acaba la tanda de teclas)
        cmp ecx, O_NUEVO
        je nuevo
        cmp ecx, O_ABRIR
        je abrir
        cmp ecx, O_GUARDAR
        je guardar
        cmp ecx, O_COMO
        je guardar_como
        cmp ecx, O_IMPRIMIR
        je imprimir
        cmp ecx, O_IMPRIMIRF
        je imprimir_f
        cmp ecx, O_CORTAR
        je cortar
        cmp ecx, O_COPIAR
        je copiar
        cmp ecx, O_PEGAR
        je pegar
        cmp ecx, O_TODO
        je todo
        cmp ecx, O_BUSCAR
        je buscar
        cmp ecx, O_SIGUIENTE
        je buscar_sig
        cmp ecx, O_DESHACER
        je deshacer
        cmp ecx, O_REHACER
        je rehacer
        cmp ecx, O_CERRAR
        je nuevo
        cmp ecx, O_RESUMEN
        je resumen
        cmp ecx, O_BORRAR
        je borrar_orden
        cmp ecx, O_REEMPLAZAR
        je reemplazar
        cmp ecx, O_IRA
        je ir_a
        cmp ecx, O_FECHA
        je ins_fecha
        cmp ecx, O_INSARCH
        je ins_archivo
        cmp ecx, O_FUENTES
        je fuentes
        cmp ecx, O_PARRAFO
        je parrafo
        cmp ecx, O_MAYMIN
        je may_min
        cmp ecx, O_COMPLETA
        je completa
        cmp ecx, O_BROCHA
        je brocha_orden
        cmp ecx, O_MARCAS
        je marcas
        mov al, P_VIN
        cmp ecx, O_VINETAS
        je lista
        mov al, P_NUM
        cmp ecx, O_NUMERA
        je lista
        mov eax, 1
        cmp ecx, O_SANG_MAS
        je sangria
        mov eax, -1
        cmp ecx, O_SANG_MENOS
        je sangria
        mov al, A_NEG
        cmp ecx, O_NEG
        je formato
        mov al, A_CUR
        cmp ecx, O_CUR
        je formato
        mov al, A_SUB
        cmp ecx, O_SUB
        je formato
        mov al, A_GRA
        cmp ecx, O_GRANDE
        je formato
        mov eax, ecx
        sub eax, O_COLOR
        cmp eax, 3
        jbe color
        cmp ecx, O_TIPO
        je tipo_sig
        mov eax, ecx
        sub eax, O_FUENTE
        cmp eax, N_FUENTES
        jb tipo
        mov eax, ecx
        sub eax, O_IZQ
        cmp eax, 3
        jbe alinea
        cmp ecx, O_ACERCA
        je .acerca
        cmp ecx, O_SALIR
        jne .x
        call preguntar
        jc .x
        mov ebp, [ven]
        HV ven_destruir
.x:
        ret
.acerca:
        mov ebp, [ven]
        mov esi, t_acerca
        mov edi, t_acerca_tit
        mov eax, DM_ACEPTAR
        HV dlg_mensaje
        ret

; mensaje de aviso: ESI=texto
aviso:
        pushad
        mov ebp, [ven]
        mov edi, t_pluma
        mov eax, DM_ACEPTAR
        HV dlg_mensaje
        popad
        ret

; =============================================================================
;  Las líneas
; =============================================================================
; EAX=sitio en el documento -> EAX=ancho de esa letra (8 o 16)
ancho_de:
        push ebx
        push ecx
        movzx ebx, byte [atr+eax]
        movzx ecx, byte [txt+eax]
        mov eax, 8
        cmp ecx, 32                 ; (la marca de párrafo: como una letra de
        jb .g                       ; la fuente de Hispa Ventanas)
        push ebx
        shr ebx, 6
        mov ebx, [fu_tablas+ebx*4]
        test ebx, ebx
        jz .g0
        sub ecx, 32
        imul ecx, 34
        movzx ecx, word [ebx+ecx]
        test ecx, ecx               ; (0: ese carácter no lo trae)
        jz .g0
        mov eax, ecx
.g0:
        pop ebx
.g:
        test bl, A_GRA
        jz .x
        shl eax, 1
.x:
        pop ecx
        pop ebx
        ret

; recompone la tabla de líneas: lin_ini (dónde empieza cada una) y lin_y (a
; qué altura); la entrada [nl] es el final del documento y su alto
compone:
        pushad
        xor esi, esi                ; por dónde va
        xor edi, edi                ; la línea
        xor ebp, ebp                ; la altura
        mov dword [cp_num], 0
        mov byte [cp_ini], 1        ; la línea que viene empieza un párrafo
.lin:
        cmp esi, [n]
        jae .fin
        mov [lin_ini+edi*4], esi
        mov [lin_y+edi*4], ebp
        mov word [lin_marca+edi*2], 0
        cmp byte [cp_ini], 0
        je .sigue
        ; un párrafo nuevo: cuánto sitio tiene (su sangría y su marca) y, si
        ; es de una lista, qué lleva delante su primera línea
        mov eax, esi
        call par_de
        mov ebx, eax
        call par_izq
        mov ecx, PAG_AN
        sub ecx, eax
        mov [cp_ancho], ecx
        test bl, P_NUM
        jz .nn
        inc dword [cp_num]          ; (los numerados seguidos cuentan 1, 2, 3...)
        mov eax, [cp_num]
        mov [lin_marca+edi*2], ax
        jmp short .sigue
.nn:
        mov dword [cp_num], 0
        test bl, P_VIN
        jz .sigue
        mov word [lin_marca+edi*2], 0xFFFF
.sigue:
        mov byte [cp_ini], 0
        mov ebx, esi                ; donde empieza
        xor ecx, ecx                ; lo que ocupa
        mov edx, -1                 ; el último espacio
.c:
        mov al, [txt+esi]
        cmp al, 10
        jne .c1
        inc esi
        mov byte [cp_ini], 1
        jmp .hecha
.c1:
        push eax
        mov eax, esi
        call ancho_de
        add ecx, eax
        pop eax
        cmp ecx, [cp_ancho]
        ja .corta
        cmp al, ' '
        jne .c2
        mov edx, esi
.c2:
        inc esi
        jmp .c
.corta:
        cmp edx, -1                 ; se parte detrás del último espacio
        je .c3
        lea esi, [edx+1]
        jmp .hecha
.c3:
        cmp esi, ebx                ; (una palabra más larga que la línea: por
        jne .hecha                  ; donde caiga; y al menos una letra)
        inc esi
.hecha:
        call alto_linea
        add ebp, eax
        inc edi
        jmp .lin
.fin:
        mov [nl], edi
        mov [lin_ini+edi*4], esi
        mov [lin_y+edi*4], ebp
        popad
        ret

; EAX=sitio -> EAX=el octeto de la marca de su párrafo (cómo es el párrafo)
par_de:
        cmp byte [txt+eax], 10
        je .x
        inc eax
        jmp par_de
.x:
        movzx eax, byte [atr+eax]
        ret
; AL=octeto de un párrafo -> EAX=lo que deja a la izquierda, en puntos: la
; sangría y, si es de una lista, el sitio de la viñeta o del número
par_izq:
        push ecx
        movzx ecx, al
        mov eax, ecx
        and eax, P_SANGRIA
        shl eax, 1                  ; (pasos x 16 x 2 = pasos x SANGRIA_PX)
        test cl, P_VIN | P_NUM
        jz .x
        add eax, MARCA_PX
.x:
        pop ecx
        ret

; EBX=principio ESI=final de una línea -> EAX=su alto (16, o 32 si tiene
; alguna letra grande)
alto_linea:
        push ebx
        mov eax, 16
        cmp byte [txt+ebx], 10      ; la línea vacía: lo que diga su marca
        jne .b
        test byte [atr+ebx], A_GRA
        jz .x
        mov eax, 32
        jmp .x
.b:
        cmp ebx, esi
        jae .x
        cmp byte [txt+ebx], 10
        je .x
        test byte [atr+ebx], A_GRA
        jnz .g
        inc ebx
        jmp .b
.g:
        mov eax, 32
.x:
        pop ebx
        ret

; EAX=sitio -> EAX=la línea donde está
linea_de:
        push ecx
        push edx
        xor ecx, ecx
.b:
        lea edx, [ecx+1]
        cmp edx, [nl]
        jae .x
        cmp eax, [lin_ini+edx*4]
        jb .x
        inc ecx
        jmp .b
.x:
        mov eax, ecx
        pop edx
        pop ecx
        ret

; EAX=sitio -> EAX=la alineación de su párrafo (la de su marca)
alin_de:
        cmp byte [txt+eax], 10
        je .x
        inc eax
        jmp alin_de
.x:
        movzx eax, byte [atr+eax]
        and eax, 3
        ret

; mide la línea EDI: [m_s] y [m_e] (principio y final), [m_al] (alto),
; [m_x0] (dónde empieza, según la alineación del párrafo)
mide:
        pushad
        mov ebx, [lin_ini+edi*4]
        mov esi, [lin_ini+edi*4+4]
        mov [m_s], ebx
        mov [m_e], esi
        call alto_linea
        mov [m_al], eax
        xor ecx, ecx                ; el ancho
        xor edx, edx                ; el ancho sin los espacios del final
        xor ebp, ebp                ; los espacios vistos
        mov dword [m_huecos], 0
        mov [m_fin_txt], ebx
.b:
        cmp ebx, esi
        jae .f
        cmp byte [txt+ebx], 10
        je .f
        mov eax, ebx
        call ancho_de
        add ecx, eax
        cmp byte [txt+ebx], ' '
        jne .l
        inc ebp
        jmp short .s
.l:
        mov edx, ecx
        mov [m_huecos], ebp         ; los espacios de antes de la última letra
        lea eax, [ebx+1]
        mov [m_fin_txt], eax
.s:
        inc ebx
        jmp .b
.f:
        mov eax, [m_s]              ; cómo es su párrafo
        call par_de
        mov ebx, eax
        call par_izq
        mov [m_izq], eax
        mov ecx, PAG_AN             ; lo que sobra a la derecha
        sub ecx, eax
        sub ecx, edx
        jns .f1
        xor ecx, ecx
.f1:
        mov byte [m_just], 0
        and ebx, 3
        jz .p                       ; a la izquierda
        cmp ebx, 3
        je .just
        cmp ebx, 1
        jne .der
        shr ecx, 1                  ; centrada: la mitad de lo que sobra
.der:
        add eax, ecx
        jmp short .p
.just:
        ; justificada: lo que sobra se reparte entre sus espacios; la última
        ; línea del párrafo (y la que no tiene espacios) se queda como está
        mov esi, [m_e]
        cmp byte [txt+esi-1], 10
        je .p
        cmp dword [m_huecos], 0
        je .p
        push eax
        mov eax, ecx
        xor edx, edx
        div dword [m_huecos]
        mov [m_jq], eax
        mov [m_jr], edx
        pop eax
        mov byte [m_just], 1
.p:
        mov [m_x0], eax
        mov dword [m_jn], 0
        popad
        ret

; EAX=un sitio de la línea medida (hay que ir en orden desde su principio) ->
; EAX=lo que ocupa, con lo que le toca del reparto si la línea va justificada
ancho_j:
        push ebx
        mov ebx, eax
        call ancho_de
        cmp byte [m_just], 0
        je .x
        cmp byte [txt+ebx], ' '
        jne .x
        cmp ebx, [m_fin_txt]
        jae .x
        add eax, [m_jq]
        push ecx
        mov ecx, [m_jn]
        inc dword [m_jn]
        cmp ecx, [m_jr]
        pop ecx
        jae .x
        inc eax
.x:
        pop ebx
        ret

; EAX=sitio -> [c_lin], [c_x], [c_y] y [c_al]: la línea, la x en el texto, la
; altura en el documento y el alto de su línea
sitio_xy:
        pushad
        mov ebx, eax
        call linea_de
        mov edi, eax
        mov [c_lin], eax
        call mide
        mov ecx, [m_x0]
        mov esi, [m_s]
.b:
        cmp esi, ebx
        jae .f
        mov eax, esi
        call ancho_j
        add ecx, eax
        inc esi
        jmp .b
.f:
        mov [c_x], ecx
        mov eax, [lin_y+edi*4]
        mov [c_y], eax
        mov eax, [m_al]
        mov [c_al], eax
        popad
        ret

; EAX=x en el texto EDI=línea -> EAX=el sitio más cercano
xy_sitio:
        push ebx
        push ecx
        push edx
        push esi
        mov edx, eax
        call mide
        mov ecx, [m_x0]
        mov esi, [m_s]
.b:
        cmp esi, [m_e]
        jae .fin
        cmp byte [txt+esi], 10
        je .x
        mov eax, esi
        call ancho_j
        mov ebx, eax
        shr ebx, 1
        add ebx, ecx
        cmp edx, ebx
        jl .x
        add ecx, eax
        inc esi
        jmp .b
.fin:
        cmp esi, [m_s]              ; una línea partida: delante de su último
        je .x                       ; espacio
        cmp byte [txt+esi-1], ' '
        jne .x
        dec esi
.x:
        mov eax, [n]
        dec eax
        cmp esi, eax
        jbe .x1
        mov esi, eax
.x1:
        mov eax, esi
        pop esi
        pop edx
        pop ecx
        pop ebx
        ret

; -> EAX y EBX: principio y final de lo elegido; ZF=1 si no hay nada
elegido:
        mov eax, [anc]
        mov ebx, [cur]
        cmp eax, -1
        jne .hay
        mov eax, ebx
.hay:
        cmp eax, ebx
        jbe .x
        xchg eax, ebx
.x:
        cmp eax, ebx
        ret

; =============================================================================
;  Mover el cursor
; =============================================================================
; después de mover el cursor: el formato de lo que se escriba es el de la
; letra de antes; que se vea; y a repintar
tras_mover:
        pushad
        mov byte [tecleando], 0
        mov eax, [cur]
        test eax, eax
        jz .de
        cmp byte [txt+eax-1], 10
        je .de
        mov bl, [atr+eax-1]
        jmp .p
.de:
        mov bl, [atr+eax]           ; al principio del párrafo: la de después
        cmp byte [txt+eax], 10
        jne .p
        and bl, A_GRA               ; (un párrafo vacío: solo su tamaño)
.p:
        mov [at_act], bl
        call a_la_vista
        call inval
        popad
        ret

; que el cursor se vea
a_la_vista:
        pushad
        mov eax, [cur]
        call sitio_xy
        mov eax, [c_y]              ; por arriba
        cmp eax, [sy]
        jge .a
        mov [sy], eax
.a:
        add eax, [c_al]             ; por abajo
        add eax, 2 * TOPM
        sub eax, [vh]
        cmp eax, [sy]
        jle .b
        mov [sy], eax
.b:
        call pon_barra
        popad
        ret

; pone el cursor en EAX; con EDX=mayúsculas, elige desde donde estaba
cur_a:
        push ebx
        test edx, TM_MAYUS
        jz .sin
        cmp dword [anc], -1
        jne .p
        mov ebx, [cur]
        mov [anc], ebx
        jmp .p
.sin:
        mov dword [anc], -1
.p:
        mov [cur], eax
        pop ebx
        ret

; ---- teclado: ECX=tecla EDX=mayúsculas ----
tecla:
        cmp ecx, TC_CTRL
        jae .ctrl
        cmp ecx, TC_IZQ
        je .izq
        cmp ecx, TC_DER
        je .der
        cmp ecx, TC_ARRIBA
        je .arr
        cmp ecx, TC_ABAJO
        je .aba
        cmp ecx, TC_INICIO
        je .ini
        cmp ecx, TC_FIN
        je .fin
        cmp ecx, TC_REPAG
        je .rep
        cmp ecx, TC_AVPAG
        je .avp
        cmp ecx, TC_CTRL_INICIO
        je .cini
        cmp ecx, TC_CTRL_FIN
        je .cfin
        cmp ecx, TC_INTRO
        je .intro
        cmp ecx, TC_RETRO
        je .retro
        cmp ecx, TC_SUPR
        je .supr
        cmp ecx, TC_TAB
        je .tab
        cmp ecx, TC_F1 + 4          ; F5
        jne .f3
        mov ecx, O_IRA
        jmp orden
.f3:
        cmp ecx, TC_F1 + 2          ; F3
        jne hecho
        mov ecx, O_SIGUIENTE
        jmp orden
.ctrl:
        sub ecx, TC_CTRL
        cmp ecx, 26
        ja hecho
        movzx ecx, byte [ctrl_ord+ecx]
        test ecx, ecx
        jz hecho
        jmp orden
.izq:
        mov eax, [cur]
        test eax, eax
        jz .mov
        dec eax
        jmp .mov
.der:
        mov eax, [cur]
        inc eax
        cmp eax, [n]
        jb .mov
        dec eax
        jmp .mov
.ini:
        mov eax, [cur]
        call linea_de
        mov eax, [lin_ini+eax*4]
        jmp .mov
.fin:
        mov eax, [cur]
        call linea_de
        mov eax, [lin_ini+eax*4+4]
        dec eax                     ; delante de la marca o del último espacio
        jmp .mov
.cini:
        xor eax, eax
        jmp .mov
.cfin:
        mov eax, [n]
        dec eax
.mov:
        call cur_a
        call tras_mover
        mov eax, [c_x]
        mov [col_x], eax
        jmp hecho
.arr:
        mov ebx, -1
        jmp .vert
.aba:
        mov ebx, 1
        jmp .vert
.rep:
        mov ebx, [vh]
        shr ebx, 4
        neg ebx
        jmp .vert
.avp:
        mov ebx, [vh]
        shr ebx, 4
.vert:
        mov eax, [cur]              ; EBX líneas arriba o abajo, en la misma x
        call linea_de
        add eax, ebx
        jns .v1
        xor eax, eax
.v1:
        mov ebx, [nl]
        dec ebx
        cmp eax, ebx
        jle .v2
        mov eax, ebx
.v2:
        mov edi, eax
        mov eax, [col_x]
        call xy_sitio
        call cur_a
        call tras_mover
        jmp hecho
.intro:
        mov al, 10
        jmp escribe
.tab:
        call inst_tecleo
        mov al, ' '
        call escribe1
        call escribe1
        call escribe1
        jmp escribe
.retro:
        call elegido
        jne .borra
        test eax, eax
        jz hecho
        dec eax
        jmp .borra
.supr:
        call elegido
        jne .borra
        inc ebx
.borra:
        call inst_tecleo
        call borra
        call cambio
        jmp hecho
; las órdenes de Ctrl+letra (de la A a la Z; 0: ninguna)
; (Ctrl+H no vale de atajo: para el teclado es Retroceso. Ctrl+I y Ctrl+M se
; distinguen de Tab y de Intro desde Hispa-386 0.9.20; «Ir a» es también F5)
ctrl_ord    db 0, O_ABRIR, O_BUSCAR, O_COPIAR, O_DER, O_TODO, 0, O_GUARDAR, 0, O_IRA, O_JUST
            db O_CUR, O_REEMPLAZAR, 0, O_NEG, 0, O_IMPRIMIR, O_IZQ, 0, O_SUB, O_CEN
            db O_NUEVO, O_PEGAR, O_GRANDE, O_CORTAR, O_REHACER, O_DESHACER

letra:
        cmp ecx, 32
        jb hecho
        cmp ecx, 255
        ja hecho
        mov al, cl
escribe:
        mov dword [bt_sobre], 0     ; (al escribir, abajo vuelve la línea y la columna)
        call inst_tecleo
        call escribe1
        call cambio
        jmp hecho
; (interna) escribe la letra AL (10: acaba el párrafo) donde el cursor, en
; lugar de lo elegido
escribe1:
        pushad
        mov dl, al
        call elegido
        je .s
        call borra
.s:
        mov ecx, 1
        call hueco
        jc .x
        mov eax, [cur]
        mov [txt+eax], dl
        mov bl, [at_act]
        cmp dl, 10
        jne .p
        push eax                    ; la marca del párrafo que se parte: como
        inc eax                     ; la del suyo (alineación, sangría, lista),
        call par_de                 ; y el tamaño de ahora
        and al, ~A_GRA
        and bl, A_GRA
        or bl, al
        pop eax
.p:
        mov [atr+eax], bl
        inc dword [cur]
.x:
        popad
        ret

; abre un hueco de ECX letras donde el cursor; CF=1 (y lo dice) si no cabe
hueco:
        pushad
        mov eax, [n]
        add eax, ecx
        cmp eax, MAX_TXT
        ja .no
        mov edx, ecx
        mov ecx, [n]
        sub ecx, [cur]              ; lo que hay que correr
        std
        mov esi, [n]
        lea edi, [txt+esi+edx-1]
        lea esi, [txt+esi-1]
        push ecx
        rep movsb
        pop ecx
        mov esi, [n]
        lea edi, [atr+esi+edx-1]
        lea esi, [atr+esi-1]
        rep movsb
        cld
        add [n], edx
        popad
        clc
        ret
.no:
        mov esi, t_lleno
        call aviso
        popad
        stc
        ret

; borra de EAX a EBX (la marca del final no se borra); el cursor queda en EAX
borra:
        pushad
        mov ecx, [n]
        dec ecx
        cmp ebx, ecx
        jbe .a
        mov ebx, ecx
.a:
        cmp eax, ebx
        jae .x
        mov ecx, [n]
        sub ecx, ebx
        mov edx, ebx
        sub edx, eax                ; lo que se quita
        push ecx
        lea esi, [txt+ebx]
        lea edi, [txt+eax]
        rep movsb
        pop ecx
        lea esi, [atr+ebx]
        lea edi, [atr+eax]
        rep movsb
        sub [n], edx
.x:
        mov [cur], eax
        mov dword [anc], -1
        popad
        ret

; después de cambiar el texto
cambio:
        pushad
        mov byte [modif], 1
        call compone
        call a_la_vista
        mov eax, [c_x]
        mov [col_x], eax
        call inval
        popad
        ret

; ---- ratón: EDX=x + y*65536 ECX=mayúsculas ----
; (interna) EDX=x + y*65536 -> EAX=el sitio que hay ahí
raton_sitio:
        push ebx
        push ecx
        push edi
        movsx eax, dx
        sub eax, TX0
        mov ebx, edx
        sar ebx, 16
        sub ebx, TB_AL + TOPM
        add ebx, [sy]
        xor edi, edi                ; la línea de esa altura
.b:
        lea ecx, [edi+1]
        cmp ecx, [nl]
        jae .f
        cmp ebx, [lin_y+ecx*4]
        jl .f
        inc edi
        jmp .b
.f:
        call xy_sitio
        pop edi
        pop ecx
        pop ebx
        ret

raton:
        mov ebx, edx
        sar ebx, 16
        cmp ebx, TB_AL
        jl .barra
        mov eax, [cli_al]
        sub eax, STAT_AL
        cmp ebx, eax
        jge hecho
        call raton_sitio
        mov edx, ecx                ; con Mayús, elige hasta ahí
        call cur_a
        mov byte [arrastra], 1
        call tras_mover
        mov eax, [c_x]
        mov [col_x], eax
        jmp hecho
.barra:
        movsx eax, dx
        mov esi, botones1           ; la fila de arriba o la de abajo
        cmp ebx, 30
        jl .b
        mov esi, botones
.b:
        call boton_en
        jc hecho
        mov ecx, [esi+12]
        jmp orden
; (interna) EAX=x ESI=la tabla de una fila de botones -> ESI=el botón que hay
; ahí; CF=1 si no hay ninguno
boton_en:
        push ecx
.b:
        cmp dword [esi+8], 0
        je .no
        mov ecx, eax
        sub ecx, [esi]
        cmp ecx, [esi+4]
        jb .es
        add esi, B_TAM
        jmp .b
.es:
        pop ecx
        clc
        ret
.no:
        pop ecx
        stc
        ret

raton_mueve:
        cmp byte [arrastra], 0
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
        call boton_en
        jc .nombre
        mov edi, [esi+16]
.nombre:
        cmp edi, [bt_sobre]
        je hecho
        mov [bt_sobre], edi
        call inval
        jmp hecho
.arrastra:
        call raton_sitio
        cmp eax, [cur]
        je hecho
        mov edx, TM_MAYUS
        call cur_a
        call tras_mover
        jmp hecho
raton_suelta:
        cmp byte [arrastra], 0
        je hecho
        mov byte [arrastra], 0
        mov eax, [anc]              ; (un clic sin arrastrar no elige nada)
        cmp eax, [cur]
        jne .elegido
        mov dword [anc], -1
        jmp hecho
.elegido:
        cmp byte [brocha], 0        ; con la brocha: lo elegido recibe el formato
        je hecho                    ; copiado, y la brocha se suelta
        mov byte [brocha], 0
        mov dl, [brocha_atr]
        call pon_formato
        jmp hecho

; doble clic: elige la palabra
raton_doble:
        mov ebx, edx
        sar ebx, 16
        cmp ebx, TB_AL
        jl raton
        call raton_sitio
        mov ebx, eax
.i:
        test eax, eax               ; hacia atrás, hasta un espacio
        jz .d
        cmp byte [txt+eax-1], ' '
        jbe .d
        dec eax
        jmp .i
.d:
        cmp byte [txt+ebx], ' '     ; y hacia delante
        jbe .f
        inc ebx
        jmp .d
.f:
        mov [anc], eax
        mov [cur], ebx
        call tras_mover
        jmp hecho

; =============================================================================
;  El formato
; =============================================================================
; AL=el bit: lo pone o lo quita en lo elegido (lo quita si todo lo tiene) o,
; si no hay nada elegido, en lo que se escriba
formato:
        mov dl, al
        call elegido
        jne .sel
        xor [at_act], dl
        jmp inval
.sel:
        call instantanea
        mov esi, eax                ; ¿lo tiene todo?
        xor ecx, ecx                ; (cuántas letras no lo tienen)
.b:
        cmp esi, ebx
        jae .f
        call .toca
        jne .s
        test [atr+esi], dl
        jnz .s
        inc ecx
.s:
        inc esi
        jmp .b
.f:
        mov esi, eax
.p:
        cmp esi, ebx
        jae .hecho
        call .toca
        jne .q
        or [atr+esi], dl
        test ecx, ecx
        jnz .q
        xor [atr+esi], dl
.q:
        inc esi
        jmp .p
.hecho:
        mov al, [at_act]            ; y lo que se escriba, igual
        or al, dl
        test ecx, ecx
        jnz .h1
        xor al, dl
.h1:
        mov [at_act], al
        jmp cambio
; (interna) ZF=1 si a la letra ESI le toca el bit DL (a las marcas de
; párrafo, solo el tamaño)
.toca:
        cmp byte [txt+esi], 10
        jne .si
        cmp dl, A_GRA
        ret
.si:
        cmp eax, eax
        ret

; EAX=color (0-7): a lo elegido o a lo que se escriba
color:
        shl eax, 4
        mov dl, al
        and byte [at_act], ~A_COLOR
        or [at_act], dl
        call elegido
        je inval
        call instantanea
.b:
        cmp eax, ebx
        jae cambio
        cmp byte [txt+eax], 10
        je .s
        and byte [atr+eax], ~A_COLOR
        or [atr+eax], dl
.s:
        inc eax
        jmp .b

; ---- el tipo de letra: el siguiente al de ahora, o el EAX ----
tipo_sig:
        movzx eax, byte [at_act]
        shr eax, 6
        inc eax
        cmp eax, N_FUENTES
        jb tipo
        xor eax, eax
tipo:
        shl eax, 6
        mov dl, al
        and byte [at_act], ~A_FUENTE
        or [at_act], dl
        call elegido
        je inval
        call instantanea
.b:
        cmp eax, ebx
        jae cambio
        cmp byte [txt+eax], 10
        je .s
        and byte [atr+eax], ~A_FUENTE
        or [atr+eax], dl
.s:
        inc eax
        jmp .b

; EAX=alineación (0-3): a los párrafos de lo elegido, o al del cursor
alinea:
        mov edx, eax
        call instantanea
        call elegido
.b:
        cmp byte [txt+eax], 10      ; la marca de cada párrafo, hasta la del
        jne .s                      ; que acaba en el final de lo elegido o
        and byte [atr+eax], ~3      ; detrás
        or [atr+eax], dl
        cmp eax, ebx
        jae cambio
.s:
        inc eax
        jmp .b

; ---- Deshacer ----
; Antes de cada cambio se guarda el documento entero (son pocos octetos): una
; tanda de teclas seguidas, sin mover el cursor, cuenta como un solo cambio.
; Deshacer cambia el documento por lo guardado, así que deshacer otra vez
; rehace.
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
        mov ecx, [n]
        mov [des_n], ecx
        mov esi, txt
        mov edi, des_txt
        rep movsb
        mov ecx, [n]
        mov esi, atr
        mov edi, des_atr
        rep movsb
        mov eax, [cur]
        mov [des_cur], eax
        mov byte [des_hay], 1
        popad
.x:
        ret
deshacer:
        cmp byte [des_hecho], 0     ; (deshacer dos veces no rehace: para eso
        jne des_cambia.x            ; está Repetir)
        mov byte [des_hecho], 2
rehacer:
        dec byte [des_hecho]        ; (Repetir solo vale detrás de Deshacer)
        cmp byte [des_hecho], 0
        jl .no
des_cambia:
        cmp byte [des_hay], 0
        je .x
        pushad
        mov ecx, [n]                ; se cambian uno por otro, hasta el más largo
        cmp ecx, [des_n]
        jae .l
        mov ecx, [des_n]
.l:
        xor esi, esi
.b:
        mov al, [txt+esi]
        xchg al, [des_txt+esi]
        mov [txt+esi], al
        mov al, [atr+esi]
        xchg al, [des_atr+esi]
        mov [atr+esi], al
        inc esi
        dec ecx
        jnz .b
        mov eax, [n]
        xchg eax, [des_n]
        mov [n], eax
        mov eax, [cur]
        xchg eax, [des_cur]
        mov [cur], eax
        mov dword [anc], -1
        mov byte [tecleando], 0
        popad
        call cambio
.x:
        ret
rehacer.no:
        mov byte [des_hecho], 0
        ret

; ---- Borrar: lo elegido, o la letra del cursor ----
borrar_orden:
        call elegido
        jne .b
        inc ebx
.b:
        call inst_tecleo
        call borra
        jmp cambio

; inserta el texto de ESI (ECX letras; los saltos de línea, 10) donde el
; cursor, en lugar de lo elegido y con el formato de ahora
inserta_texto:
        pushad
        test ecx, ecx
        jz .x
        call instantanea
        call elegido
        je .h
        call borra
.h:
        mov eax, [n]                ; lo que quepa
        add eax, ecx
        sub eax, MAX_TXT
        jbe .cabe
        sub ecx, eax
        jbe .x
.cabe:
        call hueco
        jc .x
        mov eax, [cur]
        call par_de
        and al, ~A_GRA
        mov dh, al                  ; (la marca de un párrafo nuevo: su
        mov al, [at_act]            ; alineación y el tamaño de ahora)
        and al, A_GRA
        or dh, al
        mov edi, [cur]
.c:
        lodsb
        mov dl, [at_act]
        cmp al, 10
        jne .c1
        mov dl, dh
        jmp short .c2
.c1:
        cmp al, 32
        jae .c2
        mov al, ' '
.c2:
        mov [txt+edi], al
        mov [atr+edi], dl
        inc edi
        dec ecx
        jnz .c
        mov [cur], edi
        call cambio
.x:
        popad
        ret

; ---- Insertar > Fecha y hora: «16/11/2026 12:30» ----
ins_fecha:
        mov eax, 0x2A00
        mov bl, 0x21
        call bios
        mov edi, linea
        movzx eax, byte [rl+RL_EDX]
        call .dos
        mov al, '/'
        stosb
        movzx eax, byte [rl+RL_EDX+1]
        call .dos
        mov al, '/'
        stosb
        movzx eax, word [rl+RL_ECX]
        call hvl_num
        mov al, ' '
        stosb
        push edi
        mov eax, 0x2C00
        mov bl, 0x21
        call bios
        pop edi
        movzx eax, byte [rl+RL_ECX+1]
        call .dos
        mov al, ':'
        stosb
        movzx eax, byte [rl+RL_ECX]
        call .dos
        mov ecx, edi
        sub ecx, linea
        mov esi, linea
        jmp inserta_texto
.dos:
        cmp eax, 10
        jae .d1
        mov byte [edi], '0'
        inc edi
.d1:
        jmp hvl_num

; ---- Insertar > Archivo: un archivo de texto, donde el cursor ----
ins_archivo:
        mov byte [nom_tmp], 0
        mov ebp, [ven]
        mov ebx, nom_tmp
        mov edi, t_insertar
        HV dlg_archivo
        test eax, eax
        jz .x
        mov esi, nom_tmp
        mov edi, fbuf
        mov ecx, FBUF_TAM
        call hvl_leer
        test eax, eax
        jz .x
        mov ecx, eax                ; sin los retornos, y hasta el fin de archivo
        mov esi, fbuf
        mov edi, fbuf
.c:
        lodsb
        cmp al, 26
        je .f
        cmp al, 13
        je .s
        stosb
.s:
        dec ecx
        jnz .c
.f:
        mov ecx, edi
        sub ecx, fbuf
        mov esi, fbuf
        call inserta_texto
.x:
        ret

; ---- Edición > Reemplazar: todas las veces que aparece ----
reemplazar:
        mov ebp, [ven]
        mov esi, t_q_buscar
        mov edi, t_reemplazar
        mov ebx, busca_txt
        mov ecx, 40
        HV dlg_pedir
        test eax, eax
        jz .x
        cmp byte [busca_txt], 0
        je .x
        mov ebp, [ven]
        mov esi, t_q_por
        mov edi, t_reemplazar
        mov ebx, reemp_txt
        mov ecx, 40
        HV dlg_pedir
        test eax, eax
        jz .x
        pushad
        call instantanea
        mov dword [anc], -1
        xor edx, edx                ; el largo de lo que se busca
.l1:
        cmp byte [busca_txt+edx], 0
        je .l2
        inc edx
        jmp .l1
.l2:
        xor ebp, ebp                ; y el de lo que se pone
.l3:
        cmp byte [reemp_txt+ebp], 0
        je .l4
        inc ebp
        jmp .l3
.l4:
        mov dword [n_cambios], 0
        xor edi, edi                ; por dónde se va
.b:
        mov eax, [n]
        dec eax                     ; (la marca del final no cuenta)
        sub eax, edi
        cmp eax, edx
        jl .fin
        xor ebx, ebx                ; ¿está aquí?
.c:
        cmp ebx, edx
        jae .esta
        mov al, [txt+edi+ebx]
        mov ah, [busca_txt+ebx]
        call buscar_sig.may
        xchg al, ah
        call buscar_sig.may
        cmp al, ah
        jne .sig
        inc ebx
        jmp .c
.sig:
        inc edi
        jmp .b
.esta:
        push edx
        push ebp
        push edi
        mov cl, [atr+edi]           ; con el formato de lo que había
        push ecx
        mov eax, edi
        lea ebx, [edi+edx]
        call borra                  ; (deja el cursor en EAX)
        pop ecx
        mov [at_act], cl
        mov ecx, [esp+4]            ; EBP: lo que se pone
        test ecx, ecx
        jz .puesto
        call hueco
        jc .puesto
        mov edi, [cur]
        xor ebx, ebx
.p:
        mov al, [reemp_txt+ebx]
        mov [txt+edi+ebx], al
        mov al, [at_act]
        mov [atr+edi+ebx], al
        inc ebx
        cmp ebx, ecx
        jb .p
.puesto:
        pop edi
        pop ebp
        pop edx
        add edi, ebp
        inc dword [n_cambios]
        jmp .b
.fin:
        popad
        call cambio
        mov edi, linea              ; cuántos
        mov eax, [n_cambios]
        mov esi, t_un_cambio
        cmp eax, 1
        je .m
        call hvl_num
        mov esi, t_cambios_n
        call hvl_cad
        mov byte [edi], 0
        mov esi, linea
.m:
        call aviso
.x:
        ret

; ---- Edición > Ir a: el cursor, al principio de una línea ----
ir_a:
        mov byte [num_txt], 0
        mov ebp, [ven]
        mov esi, t_q_ira
        mov edi, t_ira
        mov ebx, num_txt
        mov ecx, 8
        HV dlg_pedir
        test eax, eax
        jz .x
        mov esi, num_txt
        call hvl_lee_num
        test eax, eax
        jz .x
        cmp eax, [nl]
        jbe .l
        mov eax, [nl]
.l:
        mov eax, [lin_ini+eax*4-4]
        mov [cur], eax
        mov dword [anc], -1
        call tras_mover
        mov eax, [c_x]
        mov [col_x], eax
.x:
        ret

; ---- Formato > Cambiar mayús/minús: lo elegido, a mayúsculas; si ya lo
;      está, a minúsculas ----
may_min:
        call elegido
        jne .hay
        mov esi, t_elija
        jmp aviso
.hay:
        call instantanea
        mov esi, eax                ; ¿hay alguna minúscula?
        xor edx, edx
.m:
        cmp esi, ebx
        jae .cambia
        mov cl, [txt+esi]
        inc esi
        cmp cl, 0xA4                ; ñ
        je .si
        cmp cl, 'a'
        jb .m
        cmp cl, 'z'
        ja .m
.si:
        inc edx
.cambia:
        mov esi, eax
.c:
        cmp esi, ebx
        jae cambio
        mov cl, [txt+esi]
        test edx, edx
        jz .min
        cmp cl, 0xA4
        je .inc
        cmp cl, 'a'
        jb .s
        cmp cl, 'z'
        ja .s
        sub cl, 32
        jmp short .pon
.min:
        cmp cl, 0xA5                ; Ñ
        je .dec
        cmp cl, 'A'
        jb .s
        cmp cl, 'Z'
        ja .s
        add cl, 32
        jmp short .pon
.inc:
        inc cl
        jmp short .pon
.dec:
        dec cl
.pon:
        mov [txt+esi], cl
.s:
        inc esi
        jmp .c

; ---- Archivo > Resumen y Herramientas > Contar palabras ----
resumen:
        pushad
        xor ebx, ebx                ; letras
        xor ecx, ecx                ; palabras
        xor edx, edx                ; párrafos
        xor esi, esi
        xor ebp, ebp                ; 1: dentro de una palabra
.b:
        cmp esi, [n]
        jae .f
        mov al, [txt+esi]
        inc esi
        cmp al, 10
        jne .l
        inc edx
        xor ebp, ebp
        jmp .b
.l:
        inc ebx
        cmp al, ' '
        jne .p
        xor ebp, ebp
        jmp .b
.p:
        test ebp, ebp
        jnz .b
        inc ebp
        inc ecx
        jmp .b
.f:
        mov edi, fbuf
        mov esi, t_r_letras
        call hvl_cad
        mov eax, ebx
        call hvl_num
        mov esi, t_r_palabras
        call hvl_cad
        mov eax, ecx
        call hvl_num
        mov esi, t_r_lineas
        call hvl_cad
        mov eax, [nl]
        call hvl_num
        mov esi, t_r_parrafos
        call hvl_cad
        mov eax, edx
        call hvl_num
        mov byte [edi], 0
        mov ebp, [ven]
        mov esi, fbuf
        mov edi, t_resumen
        mov eax, DM_ACEPTAR
        HV dlg_mensaje
        popad
        ret

; ---- Ver > Pantalla completa: la ventana, a toda la pantalla (y de vuelta) ----
completa:
        xor byte [completa_si], 1
        mov eax, 3
        cmp byte [completa_si], 0
        jne .m
        mov eax, 4
.m:
        mov ebp, [ven]
        HV ven_mostrar
        ret

; ---- Formato > Fuentes: todo el formato de la letra, de una vez ----
fuentes:
        mov ebp, [ven]
        mov esi, dlg_fue
        mov edi, dlg_fue + P_TAM
        mov ebx, fue_gancho
        xor edx, edx
        HV dlg_modal
        cmp eax, ID_ACEPTAR
        jne .x
        mov dl, [dl_atr]            ; a lo elegido, o a lo que se escriba
        jmp pon_formato
.x:
        ret
; el gancho del diálogo: al empezar, las casillas como el formato de ahora;
; al aceptar, el formato como las casillas
fue_gancho:
        cmp eax, DM_INICIO
        je .ini
        cmp eax, WM_COMMAND
        jne .no
        cmp ecx, ID_ACEPTAR
        je .acepta
        ; el color y el tipo de letra son dos grupos de opciones en el mismo
        ; diálogo (Ventanas solo lleva uno): se llevan aquí
        mov eax, ecx
        sub eax, 30
        cmp eax, 3
        ja .tipo
        mov [dl_color], al
        jmp short .opciones
.tipo:
        mov eax, ecx
        sub eax, 50
        cmp eax, N_FUENTES
        jae .no
        mov [dl_tipo], al
.opciones:
        mov ecx, 30
.o1:
        lea eax, [ecx-30]
        xor edx, edx
        cmp al, [dl_color]
        jne .o2
        inc edx
.o2:
        call marca_pon
        inc ecx
        cmp ecx, 34
        jb .o1
        mov ecx, 50
.o3:
        lea eax, [ecx-50]
        xor edx, edx
        cmp al, [dl_tipo]
        jne .o4
        inc edx
.o4:
        call marca_pon
        inc ecx
        cmp ecx, 50 + N_FUENTES
        jb .o3
        jmp short .no
.acepta:
        xor bl, bl
        mov ecx, 20
        mov bh, A_NEG
.g:
        call marca_lee
        jz .g1
        or bl, bh
.g1:
        shl bh, 1
        inc ecx
        cmp ecx, 24
        jb .g
        mov al, [dl_color]
        shl al, 4
        or bl, al
        mov al, [dl_tipo]
        shl al, 6
        or bl, al
        mov [dl_atr], bl
        jmp short .no
.ini:
        mov bl, [at_act]
        mov ecx, 20
        mov bh, A_NEG
.i:
        xor edx, edx
        test bl, bh
        jz .i1
        inc edx
.i1:
        call marca_pon
        shl bh, 1
        inc ecx
        cmp ecx, 24
        jb .i
        mov al, bl                  ; el color y el tipo de letra
        shr al, 4
        and al, 3
        mov [dl_color], al
        mov al, bl
        shr al, 6
        cmp al, N_FUENTES
        jb .i2
        xor al, al
.i2:
        mov [dl_tipo], al
        jmp .opciones
.no:
        xor eax, eax
        ret
; (internas) la marca del control ECX del diálogo EBP: leerla (ZF=1 si no está
; puesta) y ponerla (EDX). Conservan los registros
marca_lee:
        pushad
        HV ven_hija_id
        mov ebp, eax
        mov eax, BM_MARCA
        HV msg_enviar
        test eax, eax
        popad
        ret
marca_pon:
        pushad
        HV ven_hija_id
        mov ebp, eax
        mov eax, BM_PONER_MARCA
        mov ecx, edx
        HV msg_enviar
        popad
        ret

; pone el formato DL a lo elegido (y a lo que se escriba): lo usan Fuentes y
; la brocha
pon_formato:
        mov [at_act], dl
        call elegido
        je inval
        call instantanea
.b:
        cmp eax, ebx
        jae cambio
        cmp byte [txt+eax], 10
        jne .l
        and byte [atr+eax], ~A_GRA  ; (a una marca de párrafo, solo el tamaño)
        mov cl, dl
        and cl, A_GRA
        or [atr+eax], cl
        jmp short .s
.l:
        mov [atr+eax], dl
.s:
        inc eax
        jmp .b

; ---- Copiar formato (la brocha): se queda con el formato de donde está el
;      cursor (o del principio de lo elegido); lo siguiente que se elija con
;      el ratón lo recibe. Pulsarla otra vez la suelta ----
brocha_orden:
        xor byte [brocha], 1
        jz inval
        call elegido
        mov dl, [at_act]
        je .p
        mov dl, [atr+eax]
.p:
        mov [brocha_atr], dl
        mov dword [anc], -1
        jmp inval

; ---- Ver > Mostrar las marcas: los espacios y los finales de párrafo ----
marcas:
        xor byte [ver_marcas], 1
        jmp inval

; ---- Viñetas y Numeración (AL = P_VIN o P_NUM): a los párrafos de lo
;      elegido, o al del cursor. Si todos lo tienen ya, se les quita ----
lista:
        mov dl, al
        call instantanea
        call elegido
        push eax
        xor ecx, ecx                ; ¿hay alguno que no lo tenga?
.m:
        cmp byte [txt+eax], 10
        jne .ms
        test [atr+eax], dl
        jnz .m1
        inc ecx
.m1:
        cmp eax, ebx
        jae .mf
.ms:
        inc eax
        jmp .m
.mf:
        pop eax
.b:
        cmp byte [txt+eax], 10
        jne .s
        and byte [atr+eax], ~(P_VIN | P_NUM)
        test ecx, ecx
        jz .q
        or [atr+eax], dl
.q:
        cmp eax, ebx
        jae cambio
.s:
        inc eax
        jmp .b

; ---- Aumentar o disminuir la sangría (EAX = 1 o -1) de los párrafos de lo
;      elegido, o del cursor: de 0 a 7 pasos ----
sangria:
        mov edx, eax
        call instantanea
        call elegido
.b:
        cmp byte [txt+eax], 10
        jne .s
        movzx ecx, byte [atr+eax]
        shr ecx, 4
        and ecx, 7
        add ecx, edx
        jns .p
        xor ecx, ecx
.p:
        cmp ecx, 7
        jbe .p1
        mov ecx, 7
.p1:
        shl ecx, 4
        and byte [atr+eax], ~P_SANGRIA
        or [atr+eax], cl
        cmp eax, ebx
        jae cambio
.s:
        inc eax
        jmp .b

; ---- Formato > Párrafo: la alineación ----
parrafo:
        mov ebp, [ven]
        mov esi, dlg_par
        mov edi, dlg_par + P_TAM
        mov ebx, par_gancho
        xor edx, edx
        HV dlg_modal
        cmp eax, ID_ACEPTAR
        jne .x
        movzx eax, byte [dl_atr]
        jmp alinea
.x:
        ret
par_gancho:
        cmp eax, DM_INICIO
        je .ini
        cmp eax, WM_COMMAND
        jne .no
        cmp ecx, ID_ACEPTAR
        jne .no
        xor bl, bl
        mov ecx, 40
.g:
        call marca_lee
        jz .g1
        mov [dl_atr], bl
.g1:
        inc bl
        inc ecx
        cmp ecx, 44
        jb .g
        jmp short .no
.ini:
        mov eax, [cur]
        call alin_de
        lea ecx, [eax+40]
        mov edx, 1
        call marca_pon
.no:
        xor eax, eax
        ret

; ---- Seleccionar todo ----
todo:
        mov dword [anc], 0
        mov eax, [n]
        dec eax
        mov [cur], eax
        jmp inval

; =============================================================================
;  El portapapeles. Hispa Ventanas no da su portapapeles a los módulos, pero
;  sí un cuadro de texto que sabe copiar y pegar: Pluma tiene uno escondido (el
;  «puente») y pasa el texto por él. Lo último que copió lo guarda además con
;  su formato: si lo que se pega es eso mismo, lo pega con formato.
; =============================================================================
copiar:
        pushad
        call elegido
        je .x
        mov ecx, ebx
        sub ecx, eax
        mov [clip_len], ecx
        push ecx
        lea esi, [txt+eax]
        mov edi, clip_txt
        rep movsb
        mov byte [edi], 0
        pop ecx
        lea esi, [atr+eax]
        mov edi, clip_atr
        rep movsb
        mov ebp, [puente]
        mov eax, WM_SETTEXT
        mov edx, clip_txt
        HV msg_enviar
        mov eax, EM_SEL_TODO
        HV msg_enviar
        mov eax, WM_COPY
        HV msg_enviar
.x:
        popad
        ret
cortar:
        call copiar
        call elegido
        je .x
        call instantanea
        call borra
        call cambio
.x:
        ret
pegar:
        pushad
        mov ebp, [puente]
        mov eax, WM_SETTEXT
        mov edx, t_nada
        HV msg_enviar
        mov eax, WM_PASTE
        HV msg_enviar
        mov eax, WM_GETTEXT
        mov ecx, MAX_TXT
        mov edx, pega_txt
        HV msg_enviar
        mov esi, pega_txt           ; cuánto es
        xor ecx, ecx
.l:
        cmp byte [esi+ecx], 0
        je .l1
        inc ecx
        jmp .l
.l1:
        test ecx, ecx
        jz .x
        xor ebp, ebp                ; ¿es lo último que copió Pluma?
        cmp ecx, [clip_len]
        jne .dist
        push ecx
        mov edi, clip_txt
        repe cmpsb
        pop ecx
        jne .dist
        inc ebp
.dist:
        call instantanea
        call elegido
        je .h
        call borra
.h:
        mov eax, [n]                ; lo que quepa
        add eax, ecx
        sub eax, MAX_TXT
        jbe .cabe
        sub ecx, eax
        jbe .x
.cabe:
        call hueco
        jc .x
        mov eax, [cur]
        call par_de
        and al, ~A_GRA
        mov dh, al                  ; la alineación del párrafo donde se pega
        mov bl, [at_act]
        and bl, A_GRA
        or dh, bl
        mov edi, [cur]
        xor esi, esi
.c:
        mov al, [pega_txt+esi]
        mov dl, [at_act]
        cmp al, 10
        jne .c1
        mov dl, dh
        jmp .c2
.c1:
        cmp al, 32                  ; (lo que no sea letra, un espacio)
        jae .c2
        mov al, ' '
.c2:
        test ebp, ebp
        jz .c3
        mov dl, [clip_atr+esi]
.c3:
        mov [txt+edi], al
        mov [atr+edi], dl
        inc edi
        inc esi
        dec ecx
        jnz .c
        mov [cur], edi
        call cambio
.x:
        popad
        ret

; =============================================================================
;  Buscar
; =============================================================================
buscar:
        mov ebp, [ven]
        mov esi, t_q_buscar
        mov edi, t_buscar
        mov ebx, busca_txt
        mov ecx, 40
        HV dlg_pedir
        test eax, eax
        jz buscar_sig.x
buscar_sig:
        pushad
        cmp byte [busca_txt], 0
        je .fin
        mov esi, busca_txt          ; EDX=su largo
        xor edx, edx
.l:
        cmp byte [esi+edx], 0
        je .l1
        inc edx
        jmp .l
.l1:
        call elegido                ; desde detrás del principio de lo elegido
        lea edi, [eax+1]            ; (o del cursor) y dando la vuelta
        cmp eax, ebx
        jne .e
        mov edi, eax
.e:
        mov ecx, [n]                ; cuántos sitios se miran
.b:
        cmp edi, [n]
        jb .b1
        xor edi, edi
.b1:
        mov eax, [n]
        sub eax, edi
        cmp eax, edx
        jb .sig
        xor ebx, ebx
.c:
        cmp ebx, edx
        jae .esta
        mov al, [txt+edi+ebx]
        mov ah, [busca_txt+ebx]
        call .may
        xchg al, ah
        call .may
        cmp al, ah
        jne .sig
        inc ebx
        jmp .c
.sig:
        inc edi
        loop .b
        mov esi, t_no_esta
        call aviso
        jmp .fin
.esta:
        mov [anc], edi
        add edi, edx
        mov [cur], edi
        call tras_mover
.fin:
        popad
.x:
        ret
; (interna) AL, a mayúscula
.may:
        cmp al, 'a'
        jb .m
        cmp al, 'z'
        ja .m
        sub al, 32
.m:
        ret

; =============================================================================
;  Archivos
; =============================================================================
; si hay cambios sin guardar, pregunta; CF=1 si no se sigue
preguntar:
        cmp byte [modif], 0
        je .si
        pushad
        mov ebp, [ven]
        mov esi, t_cambios
        mov edi, t_pluma
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

nuevo:
        call preguntar
        jc .x
        mov byte [nombre], 0
        call doc_vacio
        call pon_titulo
        call pon_barra
        call inval
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
        call cargar
.x:
        ret

; abre el archivo nom_tmp
cargar:
        pushad
        mov esi, nom_tmp
        mov edi, fbuf
        mov ecx, FBUF_TAM
        call hvl_leer
        test eax, eax
        jnz .hay
        mov esi, nom_tmp            ; ¿no está, o está vacío?
        xor al, al
        HV dos_abrir
        jnc .vacio
        mov esi, t_no_abrir
        call aviso
        jmp .x
.vacio:
        mov ebx, eax
        HV dos_cerrar
        xor eax, eax
.hay:
        mov edx, eax                ; EDX=octetos leídos
        call doc_vacio
        mov esi, fbuf
        mov edi, firma
        mov ecx, 8
        repe cmpsb
        jne .texto
        ; un .PLU: dd letras, el texto y el formato
        mov ecx, [fbuf+8]
        cmp ecx, MAX_TXT
        ja .texto
        lea eax, [ecx*2+12]
        cmp eax, edx
        ja .texto
        test ecx, ecx
        jz .fin
        push ecx
        mov esi, fbuf + 12
        mov edi, txt
        rep movsb
        pop ecx
        mov edi, atr
        push ecx
        rep movsb
        pop ecx
        mov [n], ecx
        jmp .repasa
.texto:
        ; texto llano: sin los CR; cada LF, una marca
        mov esi, fbuf
        xor edi, edi
        mov ecx, edx
        test ecx, ecx
        jz .fin
.t:
        lodsb
        cmp al, 13
        je .ts
        cmp al, 26                  ; (el fin de archivo de DOS)
        je .tf
        cmp edi, MAX_TXT - 1
        jae .largo
        mov [txt+edi], al
        mov byte [atr+edi], 0
        inc edi
.ts:
        loop .t
        jmp .tf
.largo:
        mov esi, t_largo
        call aviso
.tf:
        mov byte [txt+edi], 10      ; y la marca del final
        mov byte [atr+edi], 0
        inc edi
        mov [n], edi
.repasa:
        ; que nada se salga de lo que Pluma entiende
        mov ecx, [n]
        xor esi, esi
.r:
        mov al, [txt+esi]
        cmp al, 10
        je .rm
        cmp al, 32
        jae .rl
        mov byte [txt+esi], ' '
.rl:
        and byte [atr+esi], 0x7F
        jmp .rs
.rm:                                ; (la marca de párrafo: tal cual)
.rs:
        inc esi
        dec ecx
        jnz .r
        mov eax, [n]                ; y que acabe en una marca
        cmp byte [txt+eax-1], 10
        je .fin
        cmp eax, MAX_TXT
        jb .fm
        dec eax
.fm:
        mov byte [txt+eax], 10
        mov byte [atr+eax], 0
        inc eax
        mov [n], eax
.fin:
        mov esi, nom_tmp
        mov edi, nombre
        call copia
        call compone
        call pon_titulo
        call pon_barra
        mov byte [modif], 0
        call tras_mover
.x:
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
        mov esi, nom_tmp            ; sin extensión, .PLU
        mov edi, nombre
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
        mov esi, t_ext_plu
        call hvl_cad
.f2:
        mov byte [edi], 0
        popad
        jmp guarda_escribe
.no:
        popad
        stc
        ret

; escribe el documento en [nombre]: con .TXT, solo el texto; si no, un .PLU
guarda_escribe:
        pushad
        mov esi, nombre             ; ¿acaba en .TXT?
.e:
        cmp byte [esi], 0
        je .e1
        inc esi
        jmp .e
.e1:
        sub esi, 4
        cmp esi, nombre
        jb .plu
        mov eax, [esi]
        and eax, 0xDFDFDFFF         ; (a mayúsculas)
        cmp eax, '.TXT'
        je .txt
.plu:
        mov esi, firma
        mov edi, fbuf
        mov ecx, 8
        rep movsb
        mov ecx, [n]
        mov eax, ecx
        stosd
        mov esi, txt
        rep movsb
        mov ecx, eax
        mov esi, atr
        rep movsb
        jmp .escribe
.txt:
        mov esi, txt                ; el texto, con CR+LF, sin la marca final
        mov edi, fbuf
        mov ecx, [n]
        dec ecx
        jecxz .escribe
.t:
        lodsb
        cmp al, 10
        jne .t1
        mov al, 13
        stosb
        mov al, 10
.t1:
        stosb
        loop .t
.escribe:
        mov ecx, edi
        sub ecx, fbuf
        mov esi, nombre
        mov edx, fbuf
        call hvl_escribir
        jc .mal
        mov byte [modif], 0
        call pon_titulo
        popad
        clc
        ret
.mal:
        mov esi, t_no_guardar
        call aviso
        popad
        stc
        ret

; =============================================================================
;  Imprimir: por el puerto paralelo (LPT1) con la BIOS, INT 17h. Cada línea
;  de la hoja, una línea del papel, con un margen de cuatro espacios.
; =============================================================================
imprimir_f:
        mov byte [imp_fmt], 1
        jmp imprime
imprimir:
        mov byte [imp_fmt], 0
imprime:
        pushad
        mov eax, 0                  ; ¿hay puerto paralelo? (INT 11h)
        mov bl, 0x11
        call bios
        test ah, 0xC0
        jz .mal
        mov eax, 0x0200             ; ¿y la impresora está encendida?
        mov bl, 0x17
        call bios
        test ah, 0x28
        jnz .mal
        test ah, 0x10
        jz .mal
        mov byte [imp_atr], 0
        cmp byte [imp_fmt], 0
        je .l0
        mov al, 27                  ; ESC @: la impresora, como recién encendida
        call imp_car
        jc .mal
        mov al, '@'
        call imp_car
        jc .mal
.l0:
        xor edi, edi                ; la línea
.lin:
        cmp edi, [nl]
        jae .fin
        call mide
        mov ecx, [m_x0]             ; el margen y la alineación, en espacios
        shr ecx, 3
        add ecx, 4
        movzx ebx, word [lin_marca+edi*2]
        test ebx, ebx               ; una lista: los cuatro últimos huecos son
        jz .esp                     ; para la viñeta o el número y un espacio
        sub ecx, 4
.esp:
        mov al, ' '
        call imp_car
        jc .mal
        dec ecx
        jnz .esp
        test ebx, ebx
        jz .sin_marca
        cmp ebx, 0xFFFF
        jne .inum
        mov al, ' '
        call imp_car
        jc .mal
        mov al, ' '
        call imp_car
        jc .mal
        mov al, '-'                 ; (la viñeta: un guion, que lo imprime todo)
        call imp_car
        jc .mal
        mov al, ' '
        call imp_car
        jc .mal
        jmp short .sin_marca
.inum:
        push edi
        mov eax, ebx
        mov edi, num_txt
        call hvl_num
        mov al, '.'
        stosb
        mov byte [edi], 0
        mov ecx, edi
        sub ecx, num_txt
        pop edi
        mov ebx, 3                  ; el número y su punto, a la derecha de tres
        sub ebx, ecx
        jle .in2
.in1:
        mov al, ' '
        call imp_car
        jc .mal
        dec ebx
        jnz .in1
.in2:
        mov esi, num_txt
.in3:
        lodsb
        test al, al
        jz .in4
        call imp_car
        jc .mal
        jmp .in3
.in4:
        mov al, ' '
        call imp_car
        jc .mal
.sin_marca:
        mov esi, [m_s]
        mov edx, [m_e]              ; sin la marca ni los espacios del final
.q:
        cmp edx, esi
        jbe .c
        cmp byte [txt+edx-1], ' '
        ja .c
        dec edx
        jmp .q
.c:
        cmp esi, edx
        jae .nl
        mov al, [txt+esi]
        cmp byte [imp_fmt], 0
        je .c1
        mov ah, [atr+esi]
        call imp_formato
        jc .mal
.c1:
        call imp_car
        jc .mal
        inc esi
        jmp .c
.nl:
        mov al, 13
        call imp_car
        jc .mal
        mov al, 10
        call imp_car
        jc .mal
        inc edi
        jmp .lin
.fin:
        cmp byte [imp_fmt], 0
        je .ff
        xor ah, ah                  ; el formato, quitado
        call imp_formato
        jc .mal
.ff:
        mov al, 12                  ; y la hoja, fuera
        call imp_car
        jc .mal
        call imp_lista
        jc .mal
        mov esi, t_impreso
        call aviso
        popad
        ret
.mal:
        mov esi, t_e_imprimir
        call aviso
        popad
        ret

; pone en la impresora el formato AH (códigos ESC/P): solo lo que cambia
; respecto a lo que tiene puesto. Conserva AL. CF=1 si falla
imp_formato:
        push eax
        push ebx
        push esi
        mov bl, ah
        and bl, A_NEG | A_CUR | A_SUB | A_GRA
        mov bh, bl
        xor bh, [imp_atr]           ; BH=lo que cambia
        mov [imp_atr], bl
        mov esi, imp_cod
.b:
        mov ah, [esi]               ; el bit
        test ah, ah
        jz .ok
        test bh, ah
        jz .s
        mov al, 27
        call imp_car
        jc .x
        test bl, ah
        mov ax, [esi+1]             ; poner
        jnz .p
        mov ax, [esi+3]             ; quitar
.p:
        call imp_car
        jc .x
        test ah, ah                 ; (algunos códigos llevan un dato)
        jz .s
        mov al, ah
        cmp al, 0xFF                ; (0xFF: el dato es un 0)
        jne .p1
        xor al, al
.p1:
        call imp_car
        jc .x
.s:
        add esi, 5
        jmp .b
.ok:
        clc
.x:
        pop esi
        pop ebx
        pop eax
        ret
; el bit; el código que lo pone y su dato (0: sin dato); el que lo quita
imp_cod     db A_NEG, 'E', 0, 'F', 0
            db A_CUR, '4', 0, '5', 0
            db A_SUB, '-', 1, '-', 0xFF
            db A_GRA, 'W', 1, 'W', 0xFF
            db 0

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
pintar:
        HV dc_pintar
        call pinta_botones
        call pinta_estado
        ; la zona del texto
        xor eax, eax
        mov ebx, TB_AL
        mov ecx, [cli_an]
        sub ecx, BD_AN
        mov edx, [vh]
        HV dc_cortar
        COLOR C_CREMA               ; lo que no es hoja
        xor eax, eax
        mov ecx, PX
        HV dc_rect
        mov eax, PX + PAPW
        mov ecx, [cli_an]
        sub ecx, PX + PAPW
        jle .h
        HV dc_rect
.h:
        COLOR C_BLANCO              ; los márgenes de la hoja
        mov eax, PX
        mov ecx, MARG
        HV dc_rect
        mov eax, TX0 + PAG_AN
        HV dc_rect
        mov eax, TX0                ; el de arriba
        mov ebx, TB_AL
        sub ebx, [sy]
        mov ecx, PAG_AN
        mov edx, TOPM
        HV dc_rect
        ; las líneas que se ven
        call elegido
        mov [sel_a], eax
        mov [sel_b], ebx
        xor edi, edi
.l:
        cmp edi, [nl]
        jae .resto
        mov ebx, [lin_y+edi*4]
        add ebx, TB_AL + TOPM
        sub ebx, [sy]               ; EBX=y en la ventana
        mov eax, [cli_al]
        sub eax, STAT_AL
        cmp ebx, eax
        jge .cursor
        mov eax, [lin_y+edi*4+4]    ; ¿acaba por encima de lo que se ve?
        add eax, TB_AL + TOPM
        sub eax, [sy]
        cmp eax, TB_AL
        jle .sig
        call pinta_linea
.sig:
        inc edi
        jmp .l
.resto:
        COLOR C_BLANCO              ; la hoja, por debajo del texto
        mov eax, [nl]
        mov ebx, [lin_y+eax*4]
        add ebx, TB_AL + TOPM
        sub ebx, [sy]
        mov eax, TX0
        mov ecx, PAG_AN
        mov edx, [cli_al]
        sub edx, ebx
        jle .cursor
        HV dc_rect
.cursor:
        cmp byte [foco], 0
        je .fin
        mov eax, [cur]
        call sitio_xy
        COLOR C_TINTA
        mov eax, [c_x]
        add eax, TX0
        mov ebx, [c_y]
        add ebx, TB_AL + TOPM
        sub ebx, [sy]
        mov ecx, 2
        mov edx, [c_al]
        HV dc_rect
.fin:
        HV dc_cerrar
        jmp hecho

; ---- la barra de botones ----
pinta_botones:
        pushad
        COLOR C_ARENA
        xor eax, eax
        xor ebx, ebx
        mov ecx, [cli_an]
        mov edx, TB_AL
        HV dc_rect
        COLOR C_ARENA_OSC
        mov ebx, TB_AL - 1
        mov edx, 1
        HV dc_rect
        mov eax, [cur]              ; la alineación del párrafo del cursor
        call alin_de
        mov [al_act], eax
        mov eax, [cur]
        call par_de
        mov [par_act], al
        mov dword [bt_y], 4
        mov edi, botones1
.b:
        mov esi, [edi+8]
        test esi, esi
        jz .fila
        call .activo                ; EBP=1 si está puesto
        mov eax, [edi+12]
        sub eax, O_COLOR
        cmp eax, 3
        ja .normal
        ; un color: su muestra, con doble marco si es el de ahora
        movzx eax, byte [colores+eax]
        HV dc_color
        mov eax, [edi]
        mov ebx, [bt_y]
        mov ecx, [edi+4]
        mov edx, 22
        HV dc_rect
        COLOR C_TINTA
        HV dc_marco
        test ebp, ebp
        jz .sig
        COLOR C_BLANCO
        add eax, 2
        add ebx, 2
        sub ecx, 4
        sub edx, 4
        HV dc_marco
        jmp .sig
.normal:
        COLOR C_CREMA
        test ebp, ebp
        jz .c1
        COLOR C_PETROLEO
.c1:
        mov eax, [edi]
        mov ebx, [bt_y]
        mov ecx, [edi+4]
        mov edx, 22
        HV dc_rect
        COLOR C_TINTA
        HV dc_marco
        cmp dword [edi+12], O_TIPO  ; el del tipo de letra: el nombre del de ahora
        jne .nt
        movzx esi, byte [at_act]
        shr esi, 6
        mov esi, [fu_nombres+esi*4]
.nt:
        cmp dword [edi+20], 0       ; ¿un icono?
        je .txt
        mov al, C_CREMA
        test ebp, ebp
        jz .i1
        mov al, C_PETROLEO
.i1:
        mov [ic_fondo], al
        mov eax, [edi+4]
        sub eax, 16
        sar eax, 1
        add eax, [edi]
        mov ebx, [bt_y]
        add ebx, 3
        call pinta_icono
        jmp .sig
.txt:
        test ebp, ebp
        jz .c2
        COLOR C_CREMA
.c2:
        xor ecx, ecx                ; el texto, centrado
.lg:
        cmp byte [esi+ecx], 0
        je .lg1
        inc ecx
        jmp .lg
.lg1:
        shl ecx, 3
        mov eax, [edi+4]
        sub eax, ecx
        sar eax, 1
        add eax, [edi]
        mov ebx, [bt_y]
        add ebx, 3
        cmp dword [edi+12], O_NEG   ; la N, en negrita
        jne .t
        NEGRITA 1
.t:
        HV dc_texto
        NEGRITA 0
        cmp dword [edi+12], O_SUB   ; la S, subrayada
        jne .sig
        add ebx, 15
        mov ecx, 8
        mov edx, 1
        HV dc_rect
.sig:
        add edi, B_TAM
        jmp .b
.fila:
        cmp dword [bt_y], 4         ; y la fila de abajo
        jne .x
        mov dword [bt_y], 32
        mov edi, botones
        jmp .b
.x:
        popad
        ret
; (interna) EBP=1 si el botón EDI está puesto ahora
.activo:
        xor ebp, ebp
        mov eax, [edi+12]
        mov dl, [ver_marcas]
        cmp eax, O_MARCAS
        je .si_dl
        mov dl, [brocha]
        cmp eax, O_BROCHA
        je .si_dl
        mov dl, [par_act]
        and dl, P_VIN
        cmp eax, O_VINETAS
        je .si_dl
        mov dl, [par_act]
        and dl, P_NUM
        cmp eax, O_NUMERA
        je .si_dl
        movzx edx, byte [at_act]
        mov cl, A_NEG
        cmp eax, O_NEG
        je .bit
        mov cl, A_CUR
        cmp eax, O_CUR
        je .bit
        mov cl, A_SUB
        cmp eax, O_SUB
        je .bit
        mov cl, A_GRA
        cmp eax, O_GRANDE
        je .bit
        mov ecx, eax
        sub ecx, O_IZQ
        cmp ecx, 3
        ja .col
        cmp ecx, [al_act]
        jne .a0
        inc ebp
.a0:
        ret
.si_dl:
        test dl, dl
        jz .a0
        inc ebp
        ret
.col:
        mov ecx, eax
        sub ecx, O_COLOR
        shr edx, 4
        and edx, 7
        cmp ecx, edx
        jne .a0
        inc ebp
        ret
.bit:
        test dl, cl
        jz .a0
        inc ebp
        ret

; pinta el icono de ESI (16x16 caracteres) en EAX, EBX; los puntos «.» van del
; color [ic_fondo], el del botón
pinta_icono:
        pushad
        mov edi, ic_buf
        mov ecx, 256
.c:
        lodsb
        cmp al, '.'
        jne .c1
        mov al, [ic_fondo]
        jmp short .c3
.c1:
        cmp al, '9'
        ja .c2
        sub al, '0'
        jmp short .c3
.c2:
        sub al, 'a' - 10
.c3:
        stosb
        dec ecx
        jnz .c
        mov eax, [esp+28]
        mov ebx, [esp+16]
        mov ecx, 16
        mov edx, 16
        mov esi, ic_buf
        HV dc_copiar
        popad
        ret

; ---- la barra de estado: línea y columna, y cuánto hay escrito (o, con el
;      ratón sobre un botón, lo que hace) ----
pinta_estado:
        pushad
        COLOR C_ARENA
        xor eax, eax
        mov ebx, [cli_al]
        sub ebx, STAT_AL
        mov ecx, [cli_an]
        mov edx, STAT_AL
        HV dc_rect
        COLOR C_ARENA_OSC
        mov edx, 1
        HV dc_rect
        mov esi, [bt_sobre]
        test esi, esi
        jz .donde
        COLOR C_TINTA
        mov eax, 8
        mov ebx, [cli_al]
        sub ebx, STAT_AL - 3
        HV dc_texto
        popad
        ret
.donde:
        mov eax, [cur]
        call linea_de
        mov edx, eax
        mov edi, linea
        mov esi, t_linea
        call hvl_cad
        lea eax, [edx+1]
        call hvl_num
        mov esi, t_de
        call hvl_cad
        mov eax, [nl]
        call hvl_num
        mov esi, t_columna
        call hvl_cad
        mov eax, [cur]
        sub eax, [lin_ini+edx*4]
        inc eax
        call hvl_num
        mov al, ' '
        stosb
        stosb
        stosb
        stosb
        mov eax, [n]
        dec eax
        call hvl_num
        mov esi, t_letras
        call hvl_cad
        mov byte [edi], 0
        COLOR C_TINTA
        mov eax, 8
        mov ebx, [cli_al]
        sub ebx, STAT_AL - 3
        mov esi, linea
        HV dc_texto
        popad
        ret

; ---- una línea: EDI=la línea EBX=su y en la ventana ----
; Se dibuja en lbuf (PAG_AN x su alto, un octeto por punto) y se copia.
pinta_linea:
        pushad
        mov [pl_y], ebx
        mov [pl_lin], edi
        call mide
        mov ecx, [m_al]             ; en blanco
        imul ecx, PAG_AN
        mov edi, lbuf
        mov al, C_BLANCO
        rep stosb
        ; delante de la primera línea de un párrafo de lista: su viñeta o su
        ; número, en el sitio que le deja la sangría
        mov edi, [pl_lin]
        movzx eax, word [lin_marca+edi*2]
        test eax, eax
        jz .texto
        mov byte [g_atr], 0
        mov byte [g_color], C_TINTA
        mov ebx, [m_izq]
        cmp eax, 0xFFFF
        jne .numero
        sub ebx, 16
        mov dword [g_mapa], m_vineta
        call pl_letra
        mov dword [g_mapa], 0
        jmp short .texto
.numero:
        mov edi, num_txt            ; «12.», acabando cuatro puntos antes del texto
        call hvl_num
        mov al, '.'
        stosb
        mov ecx, edi
        sub ecx, num_txt
        mov eax, ecx
        shl eax, 3
        sub ebx, 4
        sub ebx, eax
        mov esi, num_txt
.nd:
        lodsb
        mov [g_car], al
        call pl_letra
        add ebx, 8
        dec ecx
        jnz .nd
.texto:
        mov esi, [m_s]
        mov ebx, [m_x0]             ; EBX=la x de la letra
        mov dword [m_jn], 0
.c:
        cmp esi, [m_e]
        jae .copia
        mov al, [txt+esi]
        mov ah, [atr+esi]
        mov [g_car], al
        mov [g_atr], ah
        mov ecx, 8
        cmp al, 10
        je .ancho                   ; (la marca: un hueco de una letra normal)
        mov eax, esi
        call ancho_j
        mov ecx, eax
        mov ah, [g_atr]
.ancho:
        mov [g_an], ecx
        movzx eax, ah               ; su color
        shr eax, 4
        and eax, 3
        mov al, [colores+eax]
        mov [g_color], al
        cmp byte [ver_marcas], 0    ; con las marcas a la vista: un punto en
        je .sel                     ; cada espacio y un calderón en cada marca
        mov al, [g_car]
        cmp al, 10
        je .calderon
        cmp al, ' '
        jne .sel
        mov dword [g_mapa], m_punto
        jmp short .gris
.calderon:
        mov dword [g_mapa], m_calderon
        mov byte [g_atr], 0
        mov byte [g_color], C_PIEDRA
        cmp esi, [sel_a]
        jb .cal1
        cmp esi, [sel_b]
        jae .cal1
        mov byte [g_color], C_BLANCO
        call pl_fondo
.cal1:
        call pl_letra
        mov dword [g_mapa], 0
        jmp short .copia
.gris:
        and byte [g_atr], A_GRA
        mov byte [g_color], C_PIEDRA
.sel:
        cmp esi, [sel_a]            ; lo elegido: claro sobre petróleo
        jb .let
        cmp esi, [sel_b]
        jae .let
        mov byte [g_color], C_BLANCO
        call pl_fondo
.let:
        cmp byte [g_car], 10
        je .copia
        call pl_letra
        mov dword [g_mapa], 0
        add ebx, [g_an]
        inc esi
        jmp .c
.copia:
        mov eax, TX0
        mov ebx, [pl_y]
        mov ecx, PAG_AN
        mov edx, [m_al]
        mov esi, lbuf
        HV dc_copiar
        popad
        ret

; (interna) el fondo de lo elegido: de EBX, [g_an] de ancho y todo el alto
pl_fondo:
        pushad
        mov edx, [m_al]
        lea edi, [lbuf+ebx]
.f:
        mov ecx, [g_an]
        mov eax, PAG_AN             ; (lo que cabe)
        sub eax, ebx
        jle .x
        cmp ecx, eax
        jbe .f1
        mov ecx, eax
.f1:
        push edi
        mov al, C_PETROLEO
        rep stosb
        pop edi
        add edi, PAG_AN
        dec edx
        jnz .f
.x:
        popad
        ret

; (interna) la letra [g_car] con el formato [g_atr] y el color [g_color], en
; la x EBX de lbuf, apoyada en el pie de la línea
pl_letra:
        pushad
        mov esi, [g_mapa]           ; un signo propio (viñeta, marcas)...
        test esi, esi
        jnz .mapa
        mov byte [g_16], 0
        movzx esi, byte [g_car]     ; ...o la letra: de su tipo, si lo trae...
        movzx eax, byte [g_atr]
        shr eax, 6
        mov eax, [fu_tablas+eax*4]
        test eax, eax
        jz .hispa
        cmp esi, 32
        jb .hispa
        lea ecx, [esi-32]
        imul ecx, 34
        cmp word [eax+ecx], 0
        je .hispa
        lea esi, [eax+ecx+2]
        mov byte [g_16], 1          ; (filas de 16 puntos)
        jmp short .mapa
.hispa:
        shl esi, 4                  ; ...o de la fuente de Hispa Ventanas
        add esi, [hv_api + HV_fuente8x16 * 4]
.mapa:
        mov ebp, 1                  ; la escala
        test byte [g_atr], A_GRA
        jz .e
        inc ebp
.e:
        mov edx, [m_al]             ; EDX=la y de su primera fila
        mov eax, ebp
        shl eax, 4
        sub edx, eax
        xor ecx, ecx                ; la fila de la fuente
.fila:
        cmp byte [g_16], 0
        je .f8
        movzx eax, word [esi+ecx*2]
        shl eax, 16                 ; el punto de la izquierda, el bit 31
        jmp short .f0
.f8:
        movzx eax, byte [esi+ecx]
        shl eax, 24
.f0:
        test byte [g_atr], A_NEG
        jz .f1
        mov edi, eax                ; negrita: cada punto, otra vez a su derecha
        shr edi, 1
        or eax, edi
.f1:
        test byte [g_atr], A_SUB
        jz .f2
        cmp ecx, 14
        jne .f2
        push ecx                    ; subrayado: la fila 14, todo su ancho
        mov ecx, [g_an]
        test byte [g_atr], A_GRA
        jz .su1
        shr ecx, 1
.su1:
        cmp ecx, 32
        jb .su2
        mov ecx, 32
.su2:
        neg ecx
        add ecx, 32
        mov eax, -1
        shl eax, cl
        pop ecx
.f2:
        xor edi, edi                ; cursiva: las filas de arriba, a la derecha
        test byte [g_atr], A_CUR
        jz .f3
        mov edi, 15
        sub edi, ecx
        shr edi, 2
        dec edi                     ; 2, 1, 0 y -1 puntos
.f3:
        add edi, ebx                ; EDI=la x del primer punto de la fila
        test byte [g_atr], A_GRA
        jz .f4
        sub edi, ebx                ; (el desplazamiento también es doble)
        shl edi, 1
        add edi, ebx
.f4:
        push ecx
.pt:                                ; los puntos que haya
        test eax, eax
        jz .pf
        jns .ps
        call .punto
.ps:
        shl eax, 1
        add edi, ebp
        jmp .pt
.pf:
        pop ecx
        add edx, ebp
        inc ecx
        cmp ecx, 16
        jb .fila
        popad
        ret
; (interna) un punto de la fuente en EDI, EDX: un cuadrado de EBP de lado
.punto:
        push eax
        push ebx
        push ecx
        push esi
        mov bl, [g_color]
        xor ecx, ecx                ; la fila del cuadrado
.q:
        mov eax, edx
        add eax, ecx
        cmp eax, [m_al]             ; (con signo no hace falta: de 0 arriba)
        jae .qs
        imul eax, PAG_AN
        xor esi, esi                ; la columna del cuadrado
.r:
        push edi
        add edi, esi
        cmp edi, PAG_AN
        jae .rs
        mov [lbuf+eax+edi], bl
.rs:
        pop edi
        inc esi
        cmp esi, ebp
        jb .r
.qs:
        inc ecx
        cmp ecx, ebp
        jb .q
        pop esi
        pop ecx
        pop ebx
        pop eax
        ret

%include "roble.inc"
%include "llano.inc"
%include "hvplib.inc"

FBUF_TAM    equ 2 * MAX_TXT + 16

        section .bss
ven         resd 1
puente      resd 1                  ; el cuadro de texto escondido del portapapeles
barra       resd 1                  ; la barra de desplazamiento
param       resd 1
n           resd 1                  ; letras del documento (con la marca final)
cur         resd 1                  ; el cursor
anc         resd 1                  ; el otro extremo de lo elegido (-1: nada)
col_x       resd 1                  ; la x que se quiere al subir y bajar
nl          resd 1                  ; cuántas líneas hay
sy          resd 1                  ; lo que está desplazado el documento
sy_max      resd 1
vh          resd 1                  ; el alto de la zona del texto
cli_an      resd 1
cli_al      resd 1
m_s         resd 1
m_e         resd 1
m_al        resd 1
m_x0        resd 1
c_lin       resd 1
c_x         resd 1
c_y         resd 1
c_al        resd 1
sel_a       resd 1
sel_b       resd 1
pl_y        resd 1
g_an        resd 1
al_act      resd 1
bt_y        resd 1
g_16        resb 1
            alignb 4
g_mapa      resd 1                  ; si no es 0, el dibujo que va en vez de la letra
bt_sobre    resd 1                  ; el nombre del botón que hay bajo el ratón
pl_lin      resd 1
m_izq       resd 1                  ; lo que deja a la izquierda el párrafo
m_huecos    resd 1                  ; justificar: espacios entre los que se reparte
m_fin_txt   resd 1
m_jq        resd 1                  ; lo que le toca a cada uno...
m_jr        resd 1                  ; ...y a cuántos, uno más
m_jn        resd 1
cp_num      resd 1
cp_ancho    resd 1
clip_len    resd 1
n_cambios   resd 1
des_n       resd 1
des_cur     resd 1
at_act      resb 1                  ; el formato de lo que se escriba
modif       resb 1
foco        resb 1
arrastra    resb 1
g_car       resb 1
g_atr       resb 1
g_color     resb 1
imp_fmt     resb 1
imp_atr     resb 1
des_hay     resb 1                  ; hay algo que deshacer
des_hecho   resb 1                  ; 1: lo último fue deshacer (se puede repetir)
completa_si resb 1
dl_atr      resb 1
dl_color    resb 1
dl_tipo     resb 1
m_just      resb 1
cp_ini      resb 1
ver_marcas  resb 1                  ; se ven los espacios y las marcas de párrafo
brocha      resb 1                  ; la brocha está cogida
brocha_atr  resb 1
par_act     resb 1
ic_fondo    resb 1
tecleando   resb 1                  ; en una tanda de teclas seguidas
            alignb 4
rl          resb RL_TAM
titulo      resb 100
nombre      resb 96
nom_tmp     resb 96
busca_txt   resb 44
reemp_txt   resb 44
num_txt     resb 12
linea       resb 120
            alignb 4
txt         resb MAX_TXT + 4
atr         resb MAX_TXT + 4
des_txt     resb MAX_TXT + 4        ; el documento de antes del último cambio
des_atr     resb MAX_TXT + 4
clip_txt    resb MAX_TXT + 4
clip_atr    resb MAX_TXT + 4
pega_txt    resb MAX_TXT + 4
fbuf        resb FBUF_TAM + 4
lbuf        resb PAG_AN * 32
            alignb 4
lin_ini     resd MAX_TXT + 2
lin_y       resd MAX_TXT + 2
lin_marca   resw MAX_TXT + 2        ; delante de cada línea: 0 nada, FFFFh viñeta, n su número
ic_buf      resb 256
HVP_FIN
