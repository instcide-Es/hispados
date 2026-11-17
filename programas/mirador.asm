; =============================================================================
;  MIRADOR.HVP - Mirador, el visor de imágenes de Hispa Ventanas
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Abre una imagen y la enseña: BMP, PCX, GIF, PNG, TGA, ICO, PBM/PGM/PPM y
;  JPEG, reconocidos por lo que llevan dentro y no por la extensión. Se pasa
;  a la imagen anterior y a la siguiente de la misma carpeta (o se elige en
;  una rejilla con las miniaturas de todas), se amplía y se reduce (de 25 a
;  800 %, ajustada a la ventana o a su tamaño), se desplaza con las barras,
;  las flechas o arrastrándola, se gira y se voltea, se ve a pantalla
;  completa y en un pase de diapositivas, se guarda como BMP, PNG o PCX y se
;  imprime (impresoras de agujas, ESC/P, en gráficos). Los GIF animados se
;  mueven. Y se retoca: recortar a una zona elegida con el ratón, cambiar de
;  tamaño, pasar a grises, invertir, brillo y contraste, con deshacer.
;
;  La pantalla de Hispa Ventanas tiene 256 colores fijos (los 16 con nombre,
;  un cubo de 6x6x6 y 24 grises): una foto se lleva a esa paleta repartiendo
;  el error (difusión de Floyd-Steinberg, lo normal), con tramado ordenado o
;  al color más cercano (menú Ver). Una imagen de 16 colores o menos, o con
;  una paleta que ya está en la de Hispa Ventanas, va color a color, sin
;  tramar.
;
;  Cómo está hecho:
;     programas/mirador/nucleo.inc    la capa de formatos: una tabla (firma ->
;                                     «abre», «prepara», «fila») y las filas en
;                                     rojo, verde y azul, con lo transparente
;                                     ya mezclado con el fondo; y la lectura
;                                     del archivo a trozos (mi_trozo)
;     programas/mirador/bmp.inc, pcx.inc, gif.inc, png.inc, tga.inc, ico.inc,
;                       pnm.inc       un formato cada uno
;     programas/mirador/jpg.inc       el puente con programas/comun/jpeg.inc
;     programas/comun/desinfla_t.inc  el descompresor de los PNG (por tramos)
;     programas/comun/apaleta.inc     de color verdadero a la paleta
;     programas/comun/reduce.inc      cambiar de tamaño suavizando
;     programas/mirador/carga.inc     abrir un archivo y hacer su mapa
;     programas/mirador/carpeta.inc   las imágenes de la carpeta
;     programas/mirador/vista.inc     ampliación, vista reducida, girar
;     programas/mirador/salida.inc    guardar, imprimir, información
;     programas/mirador/mini.inc      las miniaturas de la carpeta
;     programas/mirador/guarda.inc    guardar como PNG y como PCX
;     programas/mirador/retoca.inc    los retoques, deshacer y elegir una zona
;     programas/mirador/anima.inc     las animaciones (GIF)
;  El núcleo no sabe nada de Hispa Ventanas y se prueba aparte, en Linux,
;  contra Pillow (tests/tools/mirador_formatos.py y mirador_color.py; el
;  descompresor, contra zlib: mirador_desinfla_t.py); el programa entero, en
;  QEMU (tests/run_mirador_tests.sh).
;
;  La memoria. Del archivo se lee la cabecera (4 K). Un PNG, o un BMP o un
;  TGA sin comprimir, se va leyendo del disco según hacen falta sus filas: ni
;  el archivo ni la imagen sin comprimir están nunca enteros en memoria, así
;  que se abren aunque sean mucho mayores que la memoria que hay (un PNG pide
;  unos 100 K y dos filas; con Adam7, la imagen entera sin comprimir). Los
;  demás formatos se leen enteros a un bloque de memoria extendida y piden su
;  memoria de trabajo (un GIF, un octeto por punto; un JPEG, la suya). Las
;  filas salen de una en una hacia el «mapa»: la imagen ya en la paleta, un
;  octeto por punto, que es lo que se queda. Si hay sitio de sobra se guarda
;  además el original en color verdadero (tres octetos por punto): con él se
;  guarda en 24 bits, las vistas reducidas salen mejor y los retoques no
;  pierden color. Una imagen de más de 4 megapuntos (o que no quepa) se
;  reduce al vuelo a la mitad, a un cuarto..., promediando, y el pie lo dice;
;  un JPEG lo hace su propio descodificador, sin llegar a hacer la imagen
;  grande. Al acabar de leer se sueltan el archivo y la memoria de trabajo.
;  Antes de leer una imagen se suelta la anterior: en memoria no hay nunca
;  dos (salvo la de antes del último retoque, para deshacerlo, y los cuadros
;  de una animación, que van todos ya hechos en un bloque).
;  El módulo pide unos 220 K fijos; al ampliar por encima del 100 % pide
;  además un mapa del tamaño de la pantalla (medio mega en 800x600).
;
;  Una foto de móvil se gira sola (la orientación de su EXIF), también en las
;  miniaturas, y la información dice la cámara y la fecha de la toma. Cambiar
;  el tramado o el fondo de lo transparente relee el archivo y lo deja girado
;  como estaba (retocada, no se relee: se trama otra vez lo que hay).
;
;  Lo que no hace: dibujar (eso es de Pintura); copiar la imagen al
;  portapapeles (el de Hispa Ventanas es de texto: se copia el nombre del
;  archivo) ni ponerla de fondo del escritorio (Hispa Ventanas solo admite de
;  fondo un mosaico de 16x16); guardar animaciones (se guarda el cuadro que
;  se ve); leer archivos de más de 1 GB, ni de más de 64 MB los que hay que
;  tener enteros en memoria (JPEG, GIF, PCX, PNM, ICO y los BMP y TGA con
;  RLE); arrastrar con el botón del medio (Hispa Ventanas no avisa de él).
;
;  Parámetros del acceso directo: el nombre de una imagen, que se abre.
; =============================================================================
%include "hvp.inc"
%define MI_CON_JPEG                 ; (quitar si programas/comun/jpeg.inc no está)

TB_AL       equ 30                  ; la barra de botones
AVISO_AL    equ 20                  ; la franja del aviso
ID_PUENTE   equ 900
ID_BARRA_V  equ 901
ID_BARRA_H  equ 902
NUC_PILA    equ 32 * 1024           ; la pila del núcleo
ARCH_MAX    equ 64 * 1024 * 1024    ; un archivo mayor no se intenta
TOPE_PUNTOS equ 4 * 1024 * 1024     ; una imagen mayor se reduce al leerla
ORIG_MAX    equ 12 * 1024 * 1024    ; el original en color verdadero, si no pasa de aquí
RESERVA     equ 1024 * 1024         ; lo que tiene que quedar libre tras guardarlo
LST_MAX     equ 1000                ; imágenes de una carpeta
LST_TAM     equ 24                  ; de cada una: nombre (13), hora, fecha, tamaño
L_HORA      equ 14
L_FECHA     equ 16
L_TAMANO    equ 20
ZOOM_MIN    equ 0x0400              ; 1,5 %
PAG_AN      equ 576                 ; puntos de ancho de la hoja impresa (8 pulgadas a 72)
PAG_AL      equ 760                 ; y de alto que se usan

; ---- órdenes ----
O_ABRIR     equ 101
O_GUARDAR   equ 102
O_GUARDAR8  equ 103
O_INFO      equ 104
O_IMPRIMIR  equ 105
O_SALIR     equ 106
O_GUARDA_PNG equ 107
O_GUARDA_PCX equ 108
O_COPIAR    equ 111
O_DESHACER  equ 112
O_ELEGIR_T  equ 113
O_ANT       equ 121
O_SIG       equ 122
O_PRIMERA   equ 123
O_ULTIMA    equ 124
O_AMPLIAR   equ 125
O_REDUCIR   equ 126
O_AJUSTAR   equ 127
O_REAL      equ 128
O_COMPLETA  equ 129
O_PASE      equ 130
O_PASE3     equ 131
O_PASE5     equ 132
O_PASE10    equ 133
O_DIFUSION  equ 134
O_ORDENADO  equ 135
O_CERCANO   equ 136
O_AJEDREZ   equ 137
O_GIRA_IZQ  equ 141
O_GIRA_DER  equ 142
O_VOLT_H    equ 143
O_VOLT_V    equ 144
O_ELEGIR    equ 145
O_RECORTAR  equ 146
O_TAMANO    equ 147
O_GRISES    equ 148
O_INVERTIR  equ 149
O_BRILLO    equ 150
O_ACERCA    equ 151
O_MINI      equ 152

HVP_CABECERA "Mirador", entrada, 1
        db "................................"
        db "................................"
        db "................................"
        db "..1111111111111111111111111111.."
        db "..1aaaaaaaaaaaaaaaaaaaaaaaaaa1.."
        db "..1a111111111111111111111111a17."
        db "..1a1eeeeeeeeeeeeeeeeeeeeee1a17."
        db "..1a1eeeeeeeeeeeeeeeee99eee1a17."
        db "..1a1eeeeaaaeeeeeeee999999e1a17."
        db "..1a1eeeaaaaaeeeeeeee9999ee1a17."
        db "..1a1eeeaaaaaeeeeeeeeeeeeee1a17."
        db "..1a1eeeeaaaeeeeee2eeeeeeee1a17."
        db "..1a1eeeeeeeeeeee222eeeeeee1a17."
        db "..1a1eeeeeeeeeee22222eeeeee1a17."
        db "..1a1eeeeebeeee2222222eeeee1a17."
        db "..1a1eeeebbbee222222222eeee1a17."
        db "..1a1eeebbbbb22222222222eee1a17."
        db "..1a1eebbbbbbb22222222222ee1a17."
        db "..1a1ebbbbbbbbb22222222222e1a17."
        db "..1a1bbbbbbbbbbb22222222222a17.."
        db "..1a1bbbbbbbbbbbb2222222221a17.."
        db "..1a1cccccccccccccccccccccc1a17."
        db "..1a1ceccceccccecccecccceec1a17."
        db "..1a1cccccccccccccccccccccc1a17."
        db "..1a111111111111111111111111a17."
        db "..1aaaaaaaaaaaaaaaaaaaaaaaaaa17."
        db "..11111111111111111111111111117."
        db "...7777777777777777777777777777."
        db "................................"
        db "................................"
        db "................................"
        db "................................"
HVP_CODIGO

c_mirador   dd mir_proc
            db SIN_FONDO, 0, 0, 0   ; lo pinta todo ella
mir_plant:
        PLANTILLA c_mirador, titulo, E_NORMAL, 40, 34, 720, 540, 0
hijos:
        PLANTILLA HV_c_edit, 0, E_HIJA | E_OCULTA | ES_MULTI, 0, 0, 40, 40, ID_PUENTE
        PLANTILLA HV_c_barra, 0, E_HIJA, 0, 0, BD_AN, 50, ID_BARRA_V
        PLANTILLA HV_c_barra, 0, E_HIJA | BDS_HORIZ, 0, 0, 50, BD_AN, ID_BARRA_H
        dd 0

mir_menu:
        db "&Archivo", 0
        dw O_ABRIR
        db "&Abrir...                       Ctrl+A", 0
        dw 0
        db 0
        dw O_GUARDAR
        db "&Guardar como BMP...            Ctrl+G", 0
        dw O_GUARDAR8
        db "Guardar como BMP de &256 colores...", 0
        dw O_GUARDA_PNG
        db "Guardar como &PNG...", 0
        dw O_GUARDA_PCX
        db "Guardar como PC&X...", 0
        dw 0
        db 0
        dw O_INFO
        db "I&nformación de la imagen...         I", 0
        dw 0
        db 0
        dw O_IMPRIMIR
        db "&Imprimir                       Ctrl+P", 0
        dw 0
        db 0
        dw O_SALIR
        db "&Salir", 0
        dw 0xFFFF
        db "&Edición", 0
        dw O_DESHACER
        db "&Deshacer el último retoque     Ctrl+Z", 0
        dw 0
        db 0
        dw O_COPIAR
        db "&Copiar el nombre del archivo   Ctrl+C", 0
        dw O_ELEGIR_T
        db "&Elegir toda la imagen          Ctrl+E", 0
        dw 0xFFFF
        db "&Ver", 0
        dw O_ANT
        db "  Imagen &anterior             RePág", 0
        dw O_SIG
        db "  Imagen &siguiente            AvPág", 0
        dw O_PRIMERA
        db "  La &primera de la carpeta   Inicio", 0
        dw O_ULTIMA
        db "  La ú&ltima de la carpeta       Fin", 0
        dw 0
        db 0
        dw O_AMPLIAR
        db "  A&mpliar                         +", 0
        dw O_REDUCIR
        db "  &Reducir                         -", 0
        dw O_AJUSTAR
mm_ajustar  db "  A&justar a la ventana            *", 0
        dw O_REAL
        db "  &Tamaño real (100 %)             1", 0
        dw 0
        db 0
        dw O_COMPLETA
mm_completa db "  Pantalla &completa      F11, Intro", 0
        dw O_PASE
mm_pase     db "  Pase de &diapositivas           F5", 0
        dw O_PASE3
mm_pase3    db "  Pase: cada &3 segundos", 0
        dw O_PASE5
mm_pase5    db "  Pase: cada &5 segundos", 0
        dw O_PASE10
mm_pase10   db "  Pase: cada 1&0 segundos", 0
        dw 0
        db 0
        dw O_DIFUSION
mm_difusion db "  Colores: di&fusión de error", 0
        dw O_ORDENADO
mm_ordenado db "  Colores: tramado &ordenado", 0
        dw O_CERCANO
mm_cercano  db "  Colores: el más cerca&no, sin tramar", 0
        dw O_AJEDREZ
mm_ajedrez  db "  Lo transparente, a&jedrezado", 0
        dw 0
        db 0
        dw O_MINI
mm_mini     db "  M&iniaturas de la carpeta        M", 0
        dw 0xFFFF
        db "&Imagen", 0
        dw O_GIRA_IZQ
        db "Girar a la &izquierda           L", 0
        dw O_GIRA_DER
        db "Girar a la &derecha             R", 0
        dw 0
        db 0
        dw O_VOLT_H
        db "Voltear en &horizontal          H", 0
        dw O_VOLT_V
        db "Voltear en &vertical            V", 0
        dw 0
        db 0
        dw O_ELEGIR
        db "&Elegir una zona                E", 0
        dw O_RECORTAR
        db "Re&cortar a lo elegido      Intro", 0
        dw O_TAMANO
        db "Cambiar de &tamaño...", 0
        dw 0
        db 0
        dw O_GRISES
        db "A &grises", 0
        dw O_INVERTIR
        db "I&nvertir los colores", 0
        dw O_BRILLO
        db "&Brillo y contraste...", 0
        dw 0xFFFF
        db "&?", 0
        dw O_ACERCA
        db "&Acerca de Mirador...", 0
        dw 0xFFFF
        db 0
MARCA       equ '*'                 ; la marca de «puesto» de una orden del menú (como en los emuladores)

; ---- la barra de botones (programas/comun/barra.inc) ----
botones     dd 6, 26, ic_abrir, O_ABRIR, nb_abrir, BT_ICONO
            dd 34, 26, ic_guardar, O_GUARDAR, nb_guardar, BT_ICONO
            dd 62, 26, ic_imprimir, O_IMPRIMIR, nb_imprimir, BT_ICONO
            dd 98, 26, ic_img_ant, O_ANT, nb_ant, BT_ICONO
            dd 126, 26, ic_img_sig, O_SIG, nb_sig, BT_ICONO
            dd 162, 26, ic_menos, O_REDUCIR, nb_reducir, BT_ICONO
            dd 190, 26, ic_mas, O_AMPLIAR, nb_ampliar, BT_ICONO
            dd 218, 26, ic_ajustar, O_AJUSTAR, nb_ajustar, BT_ICONO
            dd 246, 26, ic_real, O_REAL, nb_real, BT_ICONO
            dd 282, 26, ic_gira_izq, O_GIRA_IZQ, nb_gira_izq, BT_ICONO
            dd 310, 26, ic_gira_der, O_GIRA_DER, nb_gira_der, BT_ICONO
            dd 346, 26, ic_completa, O_COMPLETA, nb_completa, BT_ICONO
            dd 374, 26, ic_mini, O_MINI, nb_mini, BT_ICONO
            dd 410, 26, ic_deshacer, O_DESHACER, nb_deshacer, BT_ICONO
            dd 438, 26, ic_elegir, O_ELEGIR, nb_elegir, BT_ICONO
            dd 0, 0, 0, 0, 0, 0
nb_abrir    db "Abrir una imagen (Ctrl+A)", 0
nb_guardar  db "Guardar la imagen como BMP (Ctrl+G)", 0
nb_imprimir db "Imprimir la imagen, ajustada a la hoja (Ctrl+P)", 0
nb_ant      db "Imagen anterior de la carpeta (RePág)", 0
nb_sig      db "Imagen siguiente de la carpeta (AvPág)", 0
nb_reducir  db "Reducir: ver la imagen más pequeña (-)", 0
nb_ampliar  db "Ampliar: ver la imagen más grande (+)", 0
nb_ajustar  db "Ajustar a la ventana: verla entera (*)", 0
nb_real     db "Tamaño real: un punto de la imagen, uno de la pantalla (1)", 0
nb_gira_izq db "Girar un cuarto de vuelta a la izquierda (L)", 0
nb_gira_der db "Girar un cuarto de vuelta a la derecha (R)", 0
nb_completa db "Pantalla completa (F11 o Intro; Esc para volver)", 0
nb_mini     db "Miniaturas de las imágenes de la carpeta (M)", 0
nb_deshacer db "Deshacer el último retoque (Ctrl+Z)", 0
nb_elegir   db "Elegir una zona con el ratón para recortar (E)", 0
; los iconos propios, con la forma de los de barra.inc
ic_mini:
        db "................"
        db ".0000..0000..000"
        db ".0ee0..0aa0..0bb"
        db ".0ee0..0aa0..0bb"
        db ".0000..0000..000"
        db "................"
        db ".0000..0000..000"
        db ".0cc0..0dd0..0ee"
        db ".0cc0..0dd0..0ee"
        db ".0000..0000..000"
        db "................"
        db ".0000..0000..000"
        db ".0bb0..0ee0..0aa"
        db ".0bb0..0ee0..0aa"
        db ".0000..0000..000"
        db "................"
ic_elegir:
        db "................"
        db ".00.00.00.00.00."
        db ".0............0."
        db "................"
        db ".0............0."
        db ".0............0."
        db "................"
        db ".0............0."
        db ".0............0."
        db "................"
        db ".0............0."
        db ".0.........2..0."
        db "...........22..."
        db ".0.........222.."
        db ".00.00.00..2222."
        db "...........22..."
ic_img_ant:
        db "................"
        db "................"
        db "................"
        db "..........2....."
        db "........222....."
        db ".......2222....."
        db "......2222222..."
        db "....222222222..."
        db "....222222222..."
        db "......2222222..."
        db ".......2222....."
        db "........222....."
        db "..........2....."
        db "................"
        db "................"
        db "................"
ic_img_sig:
        db "................"
        db "................"
        db "................"
        db ".....2.........."
        db ".....222........"
        db ".....2222......."
        db "...2222222......"
        db "...222222222...."
        db "...222222222...."
        db "...2222222......"
        db ".....2222......."
        db ".....222........"
        db ".....2.........."
        db "................"
        db "................"
        db "................"
ic_ajustar:
        db "................"
        db ".00000....00000."
        db ".0............0."
        db ".0..22222222..0."
        db ".0..2eeeeee2..0."
        db ".0..2eeeeee2..0."
        db "....2eeaeee2...."
        db "....2eeeebe2...."
        db "....2eeebbb2...."
        db "....2ebbbbb2...."
        db ".0..2bbbbbb2..0."
        db ".0..22222222..0."
        db ".0............0."
        db ".0............0."
        db ".00000....00000."
        db "................"
ic_real:
        db "................"
        db "................"
        db "................"
        db "...0........0..."
        db "..00.......00..."
        db ".000..00..000..."
        db "...0..00....0..."
        db "...0........0..."
        db "...0........0..."
        db "...0..00....0..."
        db "...0..00....0..."
        db "...0........0..."
        db ".00000....00000."
        db "................"
        db "................"
        db "................"
ic_gira_izq:
        db "................"
        db "................"
        db "....0..........."
        db "...00..........."
        db "..0000000......."
        db "...00....00....."
        db "....0......0...."
        db "............0..."
        db ".22222222...0..."
        db ".2eeeeee2...0..."
        db ".2eeeeee2...0..."
        db ".2eeaeee2..0...."
        db ".2ebbbbb2.0....."
        db ".2bbbbbb2......."
        db ".22222222......."
        db "................"
ic_gira_der:
        db "................"
        db "................"
        db "...........0...."
        db "...........00..."
        db ".......0000000.."
        db ".....00....00..."
        db "....0......0...."
        db "...0............"
        db "...0...22222222."
        db "...0...2eeeeee2."
        db "...0...2eeeeee2."
        db "....0..2eeaeee2."
        db ".....0.2ebbbbb2."
        db ".......2bbbbbb2."
        db ".......22222222."
        db "................"
ic_completa:
        db "................"
        db ".00000....00000."
        db ".00..........00."
        db ".0.0........0.0."
        db ".0..0......0..0."
        db ".0...0....0...0."
        db "................"
        db "................"
        db "................"
        db "................"
        db ".0...0....0...0."
        db ".0..0......0..0."
        db ".0.0........0.0."
        db ".00..........00."
        db ".00000....00000."
        db "................"

; las ampliaciones fijas, en tantos por ciento
zooms       dd 25, 33, 50, 67, 75, 100, 150, 200, 300, 400, 600, 800
ZOOMS_N     equ 12
; las órdenes de Ctrl+letra (de la A a la Z; 0: ninguna)
ctrl_ord    db 0, O_ABRIR, 0, O_COPIAR, 0, O_ELEGIR_T, 0, O_GUARDAR, 0, 0, 0
            db 0, O_GIRA_IZQ, 0, 0, 0, O_IMPRIMIR, 0, O_GIRA_DER, 0, 0
            db 0, 0, 0, 0, 0, O_DESHACER
; y las de una letra sola (en mayúscula) o un signo
letra_ord   db '+', O_AMPLIAR, '-', O_REDUCIR, '*', O_AJUSTAR, '1', O_REAL
            db 'I', O_INFO, 'L', O_GIRA_IZQ, 'R', O_GIRA_DER, 'H', O_VOLT_H
            db 'V', O_VOLT_V, 'M', O_MINI, 'E', O_ELEGIR, 0

; ---- textos ----
t_mirador   db "Mirador", 0
t_guion     db " - ", 0
t_abrir     db "Abrir una imagen", 0
t_guardar   db "Guardar la imagen", 0
t_acerca_tit db "Acerca de Mirador", 0
t_acerca    db "Mirador", 10
            db "El visor de imágenes de Hispa Ventanas:", 10
            db "BMP, PCX, GIF, PNG, TGA, ICO, PBM, PGM, PPM y JPEG.", 10
            db "Las pasa, las amplía, las gira, las retoca, las guarda", 10
            db "y las imprime; y mueve los GIF animados.", 10, 10
            db "Copyright (C) 2026 Carlos Sanz y contribuidores.", 10
            db "GPL-2.0 o posterior.", 0
t_sin_img   db "No hay ninguna imagen abierta.", 0
t_pista     db "Archivo > Abrir (Ctrl+A) para ver una imagen", 0
t_primero   db "Primero abra una imagen: Archivo > Abrir (Ctrl+A).", 0
t_x         db " x ", 0
t_pct       db " %", 0
t_sep       db "    ", 0
t_de        db " de ", 0
t_reducida  db "reducida a 1/", 0
t_pausa     db "En pausa (Espacio: seguir; Esc: salir)", 0
t_en_pase   db "pase de diapositivas", 0
t_abriendo  db "Abriendo la imagen...", 0
t_imprimiendo db "Imprimiendo la imagen...", 0
t_av_cortado db "Aviso: el archivo está cortado o dañado; se ve lo que se ha podido leer.", 0

; =============================================================================
entrada:
        cld
        mov [param], edi
        mov esi, hijos
        call hvl_clases
        mov esi, dlg_tam            ; (y las de los diálogos de retoca.inc)
        call hvl_clases
        mov esi, dlg_bri
        call hvl_clases
        mov esi, [hv_api + HV_pal_rgb * 4]
        call ap_inicia
        mov dword [trama], AP_DIFUSION
        mov byte [ajedrez], 1
        mov dword [pase_seg], 5
        mov byte [zajuste], 1
        mov dword [zoom], 0x10000
        call pon_marcas
        call pon_titulo
        HV pantalla                 ; EAX=ancho EBX=alto: que la ventana quepa
        mov [vb_an], eax            ; (y lo que medirá el mapa de ampliar)
        mov [vb_al], ebx
        sub ebx, BARRA_H + 12
        cmp ebx, 540
        jae .a
        mov [mir_plant+P_AL], ebx
        mov dword [mir_plant+P_Y], BARRA_H + 4
.a:
        cmp eax, 800
        jae .b
        sub eax, 8
        mov [mir_plant+P_AN], eax
        mov dword [mir_plant+P_X], 4
.b:
        mov esi, mir_plant
        xor ebp, ebp
        HV ven_crear
        test eax, eax
        jz .y
        mov esi, [param]
        call nombre_param
        jc .bucle
        call abre_archivo
.bucle:
        HV app_bucle
        call suelta_imagen
.y:
        ret

; ESI=parámetros: el primer nombre, a nom_tmp; CF=1 si no hay
nombre_param:
        test esi, esi
        jz .no
.e:
        lodsb
        cmp al, ' '
        je .e
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

; el título: «Mirador - NOMBRE.PNG»
pon_titulo:
        pushad
        mov edi, titulo
        mov esi, t_mirador
        call hvl_cad
        cmp byte [nombre], 0
        je .n
        mov esi, t_guion
        call hvl_cad
        mov esi, [nom_corto]
        call hvl_cad
.n:
        mov byte [edi], 0
        mov ebp, [ven]
        test ebp, ebp
        jz .x
        mov esi, titulo
        HV ven_texto
.x:
        popad
        ret

; las marcas de las órdenes del menú que están «puestas»
pon_marcas:
        push eax
        push ebx
        mov al, [zajuste]
        mov ah, 1
        mov ebx, mm_ajustar
        call .una
        mov al, [pantc]
        mov ebx, mm_completa
        call .una
        mov al, [pase]
        mov ebx, mm_pase
        call .una
        mov al, [pase_seg]
        mov ah, 3
        mov ebx, mm_pase3
        call .una
        mov ah, 5
        mov ebx, mm_pase5
        call .una
        mov ah, 10
        mov ebx, mm_pase10
        call .una
        mov al, [trama]
        mov ah, AP_DIFUSION
        mov ebx, mm_difusion
        call .una
        mov ah, AP_ORDENADO
        mov ebx, mm_ordenado
        call .una
        mov ah, AP_CERCANO
        mov ebx, mm_cercano
        call .una
        mov al, [ajedrez]
        mov ah, 1
        mov ebx, mm_ajedrez
        call .una
        mov al, [mini]
        mov ebx, mm_mini
        call .una
        pop ebx
        pop eax
        ret
.una:
        mov byte [ebx], ' '
        cmp al, ah
        jne .u1
        mov byte [ebx], MARCA
.u1:
        ret

; llama a una función del núcleo (EDX) con la pila del núcleo; los demás
; registros (y el acarreo) van y vuelven tal cual
nuc:
        mov [nuc_esp], esp
        mov esp, nuc_pila + NUC_PILA
        call edx
        mov esp, [nuc_esp]
        ret

; =============================================================================
;  El procedimiento de la ventana
; =============================================================================
mir_proc:
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
        cmp eax, WM_LBUTTONDBLCLK
        je raton_doble
        cmp eax, WM_MOUSEMOVE
        je raton_mueve
        cmp eax, WM_LBUTTONUP
        je raton_suelta
        cmp eax, WM_MOUSEWHEEL
        je .rueda
        cmp eax, WM_VSCROLL
        je .barra_v
        cmp eax, WM_HSCROLL
        je .barra_h
        cmp eax, WM_SIZE
        je .tam
        cmp eax, WM_TIMER
        je .reloj
        cmp eax, WM_HV_ABRIR
        je .hvabrir
        cmp eax, WM_GETMINMAXINFO
        jne .nmin
        mov eax, 480 + 240 * 65536
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
        mov dword [ebp+V_MENU], mir_menu
        mov dword [ebp+V_ICONO], hvp_icono
        mov esi, hijos
        HV ven_lista
        mov ecx, ID_PUENTE
        HV ven_hija_id
        mov [puente], eax
        mov ecx, ID_BARRA_V
        HV ven_hija_id
        mov [barra_v], eax
        mov ecx, ID_BARRA_H
        HV ven_hija_id
        mov [barra_h], eax
        push ebp
        mov ebp, [barra_v]
        mov eax, BDM_LINEA
        mov ecx, 32
        HV msg_enviar
        mov ebp, [barra_h]
        mov eax, BDM_LINEA
        mov ecx, 32
        HV msg_enviar
        pop ebp
.tam:
        mov ebp, [ven]
        HV ven_cli                  ; ECX=ancho EDX=alto de la zona
        mov [cli_an], ecx
        mov [cli_al], edx
        call dispone
        call inval
        jmp hecho
.rueda:
        cmp byte [mini], 0          ; en las miniaturas, las pasa de fila en fila
        je .rueda1
        mov eax, [mini_fila0]
        test ecx, ecx
        js .r_m
        add eax, 2
.r_m:
        dec eax
        call mini_desplaza
        jmp hecho
.rueda1:
        ; la rueda amplía y reduce; con Ctrl, pasa de imagen
        push ecx
        HV tec_estado
        pop ecx
        test eax, TM_CTRL
        jz .r_zoom
        mov eax, O_SIG
        test ecx, ecx
        jns .r_o
        mov eax, O_ANT
.r_o:
        mov ecx, eax
        jmp orden
.r_zoom:
        mov eax, O_REDUCIR
        test ecx, ecx
        jns .r_o
        mov eax, O_AMPLIAR
        jmp .r_o
.barra_v:
        mov eax, ecx
        cmp byte [mini], 0
        je .bv1
        call mini_desplaza
        jmp hecho
.bv1:
        call desplaza_v
        jmp hecho
.barra_h:
        mov eax, ecx
        call desplaza_h
        jmp hecho
.reloj:
        cmp ecx, ID_RELOJ_MINI      ; otra miniatura
        jne .reloj0
        call mini_paso
        jmp hecho
.reloj0:
        cmp ecx, ID_RELOJ_ANIMA     ; la animación: ¿otro cuadro?
        jne .reloj1
        call an_reloj
        jmp hecho
.reloj1:
        cmp byte [pase], 0          ; el pase de diapositivas: la siguiente
        je hecho
        cmp byte [pase_pausa], 0
        jne hecho
        mov ecx, O_SIG
        call orden_haz
        jmp hecho
.cerrar:
        call confirma_retoque       ; (retocada y sin guardar: se pregunta)
        jc hecho
        mov ebp, [ven]
        HV ven_destruir
        jmp hecho
.destruir:
        mov dword [ven], 0
        HV msg_salir
        jmp hecho
.hvabrir:
        mov esi, edx
        call nombre_param
        jc hecho
        call abre_archivo
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
; solo el pie
inval_pie:
        pushad
        mov ebp, [ven]
        test ebp, ebp
        jz .x
        xor eax, eax
        mov ebx, [cli_al]
        sub ebx, BR_PIE
        mov ecx, [cli_an]
        mov edx, BR_PIE
        HV ven_invalidar_rect
.x:
        popad
        ret

; ESI=un texto: lo pone en el pie ahora mismo, antes de hacer algo que tarda
; (leer de un disquete, imprimir): se pinta y se le da el turno al sistema
; para que lo lleve a la pantalla. A pantalla completa no hay pie
pie_dice:
        pushad
        mov ebp, [ven]
        test ebp, ebp
        jz .x
        cmp byte [pantc], 0
        jne .x
        HV dc_abrir
        COLOR C_ARENA
        xor eax, eax
        mov ebx, [cli_al]
        sub ebx, BR_PIE - 1
        mov ecx, [cli_an]
        mov edx, BR_PIE - 1
        HV dc_rect
        COLOR C_TINTA
        mov eax, 8
        add ebx, 2
        HV dc_texto
        HV dc_cerrar
        HV ceder
.x:
        popad
        ret

; hay que rehacer lo que se ve de la imagen
ensucia:
        mov byte [sucio], 1
        jmp inval

; =============================================================================
;  Las órdenes: ECX=número
; =============================================================================
orden:
        call orden_haz
        jmp hecho
orden_haz:
        mov dword [br_sobre], 0
        cmp ecx, O_ABRIR
        je abrir
        cmp ecx, O_ACERCA
        je .acerca
        cmp ecx, O_SALIR
        je .salir
        cmp ecx, O_MINI
        jne .nmini
        call mini_alterna
        jmp pon_marcas
.nmini:
        cmp byte [mini], 0          ; con las miniaturas a la vista, lo demás no toca
        jne .x
        cmp ecx, O_COMPLETA
        je completa
        cmp ecx, O_PASE3
        jb .n0
        cmp ecx, O_AJEDREZ
        jbe .ajuste
.n0:
        cmp ecx, 100                ; (un aviso de las barras: nada)
        jb .x
        cmp ecx, 200
        jae .x
        cmp ecx, O_ANT              ; pasar de imagen vale aunque la de ahora no se vea
        jb .n1
        cmp ecx, O_ULTIMA
        jbe pasa
        cmp ecx, O_PASE
        je pase_alterna
.n1:
        cmp byte [hay_img], 0       ; las demás necesitan una imagen
        jne .con
        cmp ecx, O_ANT              ; (ampliar o girar sin imagen: nada; lo demás, se dice)
        jae .x
        cmp byte [pantc], 0         ; (a pantalla completa no se abren diálogos)
        jne .x
        mov esi, t_primero
        jmp aviso
.con:
        cmp ecx, O_GUARDAR
        je guardar
        cmp ecx, O_GUARDAR8
        je guardar
        cmp ecx, O_GUARDA_PNG
        je guardar
        cmp ecx, O_GUARDA_PCX
        je guardar
        cmp ecx, O_INFO
        je informacion
        cmp ecx, O_IMPRIMIR
        je imprimir
        cmp ecx, O_COPIAR
        je copiar
        cmp ecx, O_AMPLIAR
        je ampliar
        cmp ecx, O_REDUCIR
        je reducir
        cmp ecx, O_AJUSTAR
        je ajustar
        cmp ecx, O_REAL
        je real
        cmp ecx, O_DESHACER
        je deshacer
        cmp ecx, O_ELEGIR_T
        je elegir_todo
        cmp ecx, O_ELEGIR
        je elegir_alterna
        cmp ecx, O_RECORTAR
        je recortar
        cmp ecx, O_TAMANO
        je cambia_tamano
        cmp ecx, O_GRISES
        je a_grises
        cmp ecx, O_INVERTIR
        je invertir
        cmp ecx, O_BRILLO
        je brillo
        cmp ecx, O_GIRA_IZQ
        jb .x
        cmp ecx, O_VOLT_V
        ja .x
        call el_sal                 ; (el marco no gira con la imagen)
        jmp transforma
.x:
        ret
.acerca:
        mov ebp, [ven]
        mov esi, t_acerca
        mov edi, t_acerca_tit
        mov eax, DM_ACEPTAR
        HV dlg_mensaje
        ret
.salir:
        call confirma_retoque
        jc .x
        mov ebp, [ven]
        HV ven_destruir
        ret
; lo que se elige en el menú Ver y se queda puesto
.ajuste:
        cmp ecx, O_PASE10
        ja .colores
        mov eax, 3
        cmp ecx, O_PASE3
        je .seg
        mov eax, 5
        cmp ecx, O_PASE5
        je .seg
        mov eax, 10
.seg:
        mov [pase_seg], eax
        call pon_marcas
        cmp byte [pase], 0          ; (en marcha: con el paso nuevo)
        je .x
        jmp pase_reloj
.colores:
        cmp ecx, O_AJEDREZ
        je .ajedrez
        mov eax, AP_DIFUSION
        cmp ecx, O_DIFUSION
        je .tr
        mov eax, AP_ORDENADO
        cmp ecx, O_ORDENADO
        je .tr
        mov eax, AP_CERCANO
.tr:
        cmp eax, [trama]
        je .x
        mov [trama], eax
        call pon_marcas
        cmp byte [retocada], 0      ; retocada: no se lee del archivo, se trama lo que hay
        jne remapa
        jmp recarga
.ajedrez:
        xor byte [ajedrez], 1
        call pon_marcas
        cmp byte [hay_img], 0       ; solo cambia algo si la imagen tiene transparencia
        je .x
        cmp byte [f_alfa], 0
        je .x
        cmp byte [retocada], 0      ; (retocada, lo transparente ya está mezclado: para la siguiente)
        jne .x
        jmp recarga

; mensaje de aviso: ESI=texto (a pantalla completa no se abre: taparía la imagen)
aviso:
        pushad
        mov ebp, [ven]
        mov edi, t_mirador
        mov eax, DM_ACEPTAR
        HV dlg_mensaje
        popad
        ret

; =============================================================================
;  El teclado y el ratón
; =============================================================================
tecla:
        cmp ecx, TC_CTRL
        jae .ctrl
        cmp byte [mini], 0
        jne mini_tecla
        cmp ecx, TC_ESC
        jne .t1
        cmp byte [elegir], 0        ; Esc, eligiendo: quita el marco y deja de elegir
        je .esc0
        call el_fuera
        jmp hecho
.esc0:
        call pase_para              ; Esc: fuera del pase y de la pantalla completa
        cmp byte [pantc], 0
        je .esc1
        call completa_sal
.esc1:
        call inval
        jmp hecho
.t1:
        cmp ecx, TC_INTRO
        jne .t1b
        cmp byte [elegir], 0        ; Intro, eligiendo: recorta
        je .completa
        mov ecx, O_RECORTAR
        jmp orden
.t1b:
        cmp ecx, 0x57               ; F11 (según el teclado, llega como 57h o como 85h)
        je .completa
        cmp ecx, 0x85
        je .completa
        cmp ecx, TC_F1 + 4          ; F5
        jne .t2
        mov ecx, O_PASE
        jmp orden
.completa:
        mov ecx, O_COMPLETA
        jmp orden
.t2:
        mov eax, O_SIG
        cmp ecx, TC_AVPAG
        je .o
        mov eax, O_ANT
        cmp ecx, TC_REPAG
        je .o
        cmp ecx, TC_RETRO
        je .o
        mov eax, O_PRIMERA
        cmp ecx, TC_INICIO
        je .o
        mov eax, O_ULTIMA
        cmp ecx, TC_FIN
        je .o
        ; las flechas desplazan la imagen si no cabe; si cabe a lo ancho, las
        ; de los lados pasan a la anterior y a la siguiente
        cmp ecx, TC_DER
        jne .t3
        mov eax, O_SIG
        cmp dword [sx_max], 0
        je .o
        mov eax, [sx]
        add eax, 32
        call desplaza_h
        jmp hecho
.t3:
        cmp ecx, TC_IZQ
        jne .t4
        mov eax, O_ANT
        cmp dword [sx_max], 0
        je .o
        mov eax, [sx]
        sub eax, 32
        call desplaza_h
        jmp hecho
.t4:
        mov eax, [sy]
        cmp ecx, TC_ABAJO
        jne .t5
        add eax, 32
        call desplaza_v
        jmp hecho
.t5:
        cmp ecx, TC_ARRIBA
        jne hecho
        sub eax, 32
        call desplaza_v
        jmp hecho
.o:
        mov ecx, eax
        jmp orden
.ctrl:
        sub ecx, TC_CTRL
        cmp ecx, 26
        ja hecho
        movzx ecx, byte [ctrl_ord+ecx]
        test ecx, ecx
        jz hecho
        jmp orden
letra:
        cmp ecx, ' '
        jne .l0
        cmp byte [mini], 0
        jne hecho
        mov ecx, O_SIG              ; el espacio: la siguiente; en el pase, pausa
        cmp byte [pase], 0
        jne .l_pase
        cmp dword [an_n], 2         ; y en una animación (fuera del pase), la para y la sigue
        jb orden
        call an_espacio
        jmp hecho
.l_pase:
        xor byte [pase_pausa], 1
        call inval
        cmp byte [pase_pausa], 0    ; al seguir, la cuenta empieza otra vez
        jne hecho
        mov ebp, [ven]
        mov ecx, 1
        HV tmp_quitar
        call pase_reloj
        jmp hecho
.l0:
        cmp ecx, 'a'
        jb .l1
        cmp ecx, 'z'
        ja .l1
        sub ecx, 0x20
.l1:
        mov esi, letra_ord
.l2:
        lodsw
        test al, al
        jz hecho
        cmp al, cl
        jne .l2
        movzx ecx, ah
        jmp orden

raton:
        cmp byte [pantc], 0
        jne .hoja
        mov ebx, edx
        sar ebx, 16
        cmp ebx, TB_AL
        jge .hoja
        movsx eax, dx
        mov esi, botones
        call br_boton_en
        jc hecho
        mov ecx, [esi+12]
        jmp orden
.hoja:
        cmp byte [mini], 0          ; en las miniaturas: se elige
        je .hoja1
        call mini_clic
        jmp hecho
.hoja1:
        ; en la imagen: se coge y se arrastra
        cmp byte [hay_img], 0
        je hecho
        mov ebx, edx
        sar ebx, 16
        mov eax, [vy]
        cmp ebx, eax
        jl hecho
        add eax, [vh]
        cmp ebx, eax
        jge hecho
        movsx eax, dx
        cmp eax, [vw]
        jge hecho
        cmp byte [elegir], 0        ; eligiendo: el ratón marca la zona
        je .coge
        call el_raton
        jmp hecho
.coge:
        mov [arr_x], eax
        mov [arr_y], ebx
        mov byte [arrastra], 1
        mov ebp, [ven]
        HV ven_capturar
        jmp hecho
raton_doble:
        ; doble clic en la imagen: a pantalla completa, y de vuelta
        mov ebx, edx
        sar ebx, 16
        cmp ebx, [vy]
        jl raton
        mov eax, [vy]
        add eax, [vh]
        cmp ebx, eax
        jge hecho
        cmp byte [mini], 0          ; en las miniaturas: se abre
        jne .mini
        cmp byte [elegir], 0        ; (eligiendo, es un clic más)
        jne raton
        jmp .completa
.mini:
        call mini_clic
        jc hecho
        jmp mini_abre
.completa:
        mov ecx, O_COMPLETA
        jmp orden
raton_suelta:
        cmp byte [arrastra], 0
        je hecho
        mov byte [arrastra], 0
        HV ven_soltar
        jmp hecho
raton_mueve:
        cmp byte [arrastra], 0
        je .barra
        cmp byte [arrastra], 2      ; arrastrando el marco de elegir
        jne .lleva
        call el_mueve
        jmp hecho
.lleva:
        movsx eax, dx               ; la imagen sigue al ratón
        mov ebx, edx
        sar ebx, 16
        mov ecx, [arr_x]
        sub ecx, eax
        mov [arr_x], eax
        mov edx, [arr_y]
        sub edx, ebx
        mov [arr_y], ebx
        mov eax, [sx]
        add eax, ecx
        call desplaza_h
        mov eax, [sy]
        add eax, edx
        call desplaza_v
        jmp hecho
.barra:
        cmp byte [elegir], 0        ; eligiendo: el cursor dice qué se coge del marco
        je .barra0
        call el_sobre
.barra0:
        ; sobre un botón de la barra: su nombre, abajo
        cmp byte [pantc], 0
        jne hecho
        xor edi, edi
        mov ebx, edx
        sar ebx, 16
        cmp ebx, TB_AL
        jae .nombre
        test ebx, ebx
        js .nombre
        movsx eax, dx
        mov esi, botones
        call br_boton_en
        jc .nombre
        mov edi, [esi+16]
.nombre:
        call br_sobre_pon
        je hecho
        call inval_pie
        jmp hecho

; =============================================================================
;  La pantalla completa y el pase de diapositivas
; =============================================================================
; Como los emuladores (lib/emufull.inc): a la ficha de la ventana se le quitan
; el título, el marco y el menú, y se pone a cubrir la pantalla; el núcleo ya
; sabe, con eso, dar toda la ventana como zona y no pintar nada alrededor
completa:
        cmp byte [pantc], 0
        jne completa_sal
completa_entra:
        pushad
        mov ebp, [ven]
        test ebp, ebp
        jz .x
        cmp byte [pantc], 0
        jne .x
        mov eax, [ebp+V_ESTILO]
        mov [pc_estilo], eax
        mov eax, [ebp+V_MENU]
        mov [pc_menu], eax
        mov eax, [ebp+V_X]
        mov [pc_x], eax
        mov eax, [ebp+V_Y]
        mov [pc_y], eax
        mov eax, [ebp+V_AN]
        mov [pc_an], eax
        mov eax, [ebp+V_AL]
        mov [pc_al], eax
        mov al, [zajuste]           ; la ampliación de la ventana, para la vuelta
        mov [pc_zaj], al
        mov eax, [zoom]
        mov [pc_zoom], eax
        mov byte [zajuste], 1
        and dword [ebp+V_ESTILO], ~(E_TITULO | E_MAXIM | E_MINIM | E_TAMANO)
        mov dword [ebp+V_MENU], 0
        mov byte [pantc], 1
        mov dword [br_sobre], 0
        push ebp                    ; sin barras de desplazamiento
        mov ebp, [barra_v]
        xor eax, eax
        HV ven_mostrar
        mov ebp, [barra_h]
        xor eax, eax
        HV ven_mostrar
        pop ebp
        HV ven_activar
        HV pantalla
        mov ecx, eax
        mov edx, ebx
        xor eax, eax
        xor ebx, ebx
        HV ven_mover                ; (llega WM_SIZE: se dispone todo otra vez)
        call pon_marcas
        mov ebp, [ven]
        HV ven_cli
        mov [cli_an], ecx
        mov [cli_al], edx
        call dispone
        call inval
.x:
        popad
        ret
completa_sal:
        pushad
        mov ebp, [ven]
        test ebp, ebp
        jz .x
        cmp byte [pantc], 0
        je .x
        mov byte [pantc], 0
        mov eax, [pc_estilo]
        mov [ebp+V_ESTILO], eax
        mov eax, [pc_menu]
        mov [ebp+V_MENU], eax
        mov al, [pc_zaj]
        mov [zajuste], al
        mov eax, [pc_zoom]
        mov [zoom], eax
        mov eax, [pc_x]
        mov ebx, [pc_y]
        mov ecx, [pc_an]
        mov edx, [pc_al]
        HV ven_mover
        push ebp
        mov ebp, [barra_v]
        mov eax, 1
        HV ven_mostrar
        mov ebp, [barra_h]
        mov eax, 1
        HV ven_mostrar
        pop ebp
        HV ven_activar
        call pon_marcas
        mov ebp, [ven]
        HV ven_cli
        mov [cli_an], ecx
        mov [cli_al], edx
        call dispone
        call inval
.x:
        popad
        ret

; el pase: cada [pase_seg] segundos, la siguiente; en la ventana o a pantalla
; completa, como se esté (y se puede cambiar con el pase en marcha)
pase_alterna:
        cmp byte [pase], 0
        jne pase_fuera
        cmp dword [lst_n], 0
        jne .hay
        mov esi, t_primero
        jmp aviso
.hay:
        mov byte [pase], 1
        mov byte [pase_pausa], 0
        call pon_marcas
        call inval
pase_reloj:
        pushad
        mov ebp, [ven]
        mov ecx, 1
        mov edx, [pase_seg]
        imul edx, 1000
        HV tmp_poner
        popad
        ret
pase_fuera:
        call pase_para
        jmp inval
pase_para:
        pushad
        cmp byte [pase], 0
        je .x
        mov byte [pase], 0
        mov byte [pase_pausa], 0
        mov ebp, [ven]
        mov ecx, 1
        HV tmp_quitar
        call pon_marcas
.x:
        popad
        ret

; =============================================================================
;  Pintar
; =============================================================================
pintar:
        HV dc_pintar
        cmp byte [pantc], 0
        jne .zona
        ; la barra de botones
        mov ecx, [cli_an]
        mov edx, TB_AL
        call br_fondo
        mov edi, botones
        mov ebx, 4
        call br_fila
        ; la franja del aviso
        cmp dword [av_txt], 0
        je .rincon
        COLOR C_CREMA
        xor eax, eax
        mov ebx, TB_AL
        mov ecx, [cli_an]
        mov edx, AVISO_AL
        HV dc_rect
        COLOR C_ARENA_OSC
        mov ebx, TB_AL + AVISO_AL - 1
        mov edx, 1
        HV dc_rect
        COLOR C_GRANATE
        mov eax, 8
        mov ebx, TB_AL + 2
        mov esi, [av_txt]
        HV dc_texto
.rincon:
        COLOR C_ARENA               ; el rincón entre las dos barras
        mov eax, [cli_an]
        sub eax, BD_AN
        mov ebx, [vy]
        add ebx, [vh]
        mov ecx, BD_AN
        mov edx, BD_AN
        HV dc_rect
        call pinta_pie
.zona:
        cmp dword [vw], 0
        jle .fin
        cmp dword [vh], 0
        jle .fin
        cmp byte [mini], 0
        je .zona1
        call mini_pinta
        jmp .fin
.zona1:
        cmp byte [hay_img], 0
        jne .imagen
        ; sin imagen: el fondo y lo que haya que decir
        call color_fondo
        xor eax, eax
        mov ebx, [vy]
        mov ecx, [vw]
        mov edx, [vh]
        HV dc_rect
        mov esi, [err_txt]
        COLOR C_GRANATE
        test esi, esi
        jnz .msg
        mov esi, t_pista
        COLOR C_MARRON
        cmp byte [pantc], 0
        je .msg
        COLOR C_ARENA
.msg:
        cmp byte [pantc], 0
        je .msg1
        test dword [err_txt], -1
        jz .msg1
        COLOR C_TERRACOTA_CL        ; (sobre negro, el granate no se lee)
.msg1:
        call pinta_mensaje
        jmp .fin
.imagen:
        call pinta_imagen
        cmp byte [elegir], 0
        je .fin
        call el_pinta
.fin:
        HV dc_cerrar
        ; el pase, en pausa: se dice arriba, encima de todo (con otro
        ; contexto: el de antes ha quedado recortado a la imagen)
        cmp byte [pase], 0
        je hecho
        cmp byte [pase_pausa], 0
        je hecho
        mov ebp, [ven]
        HV dc_abrir
        COLOR C_TINTA
        mov eax, 8
        mov ebx, [vy]
        add ebx, 8
        mov ecx, 38 * 8 + 12
        mov edx, 22
        HV dc_rect
        COLOR C_ARENA
        HV dc_marco
        COLOR C_CREMA
        add eax, 6
        add ebx, 3
        mov esi, t_pausa
        HV dc_texto
        HV dc_cerrar
        jmp hecho

; el color de alrededor de la imagen: crema en la ventana, negro a pantalla completa
color_fondo:
        COLOR C_CREMA
        cmp byte [pantc], 0
        je .x
        COLOR C_TINTA
.x:
        ret

; ESI=un texto de varias líneas (separadas por un 10): centrado en la zona
pinta_mensaje:
        pushad
        mov edi, esi                ; cuántas líneas
        mov ecx, 1
.n:
        mov al, [edi]
        inc edi
        test al, al
        jz .n1
        cmp al, 10
        jne .n
        inc ecx
        jmp .n
.n1:
        shl ecx, 4
        mov ebx, [vh]
        sub ebx, ecx
        sar ebx, 1
        jns .n2
        xor ebx, ebx
.n2:
        add ebx, [vy]               ; EBX=y de la primera
.l:
        xor ecx, ecx                ; ECX=letras de la línea
.l1:
        mov al, [esi+ecx]
        test al, al
        jz .l2
        cmp al, 10
        je .l2
        inc ecx
        jmp .l1
.l2:
        mov eax, ecx
        shl eax, 3
        neg eax
        add eax, [vw]
        sar eax, 1
        jns .l3
        xor eax, eax
.l3:
        HV dc_texto_n               ; ESI=texto ECX=letras
        add esi, ecx
        add ebx, 16
        cmp byte [esi], 0
        je .x
        inc esi
        jmp .l
.x:
        popad
        ret

; lo que pregunta la barra común de cada botón: «Ajustar» se queda puesto
barra_estado:
        xor ebp, ebp
        cmp dword [edi+12], O_MINI
        jne .n
        movzx ebp, byte [mini]
        ret
.n:
        cmp dword [edi+12], O_ELEGIR
        jne .n1
        movzx ebp, byte [elegir]
        ret
.n1:
        cmp dword [edi+12], O_AJUSTAR
        jne .x
        cmp byte [mini], 0
        jne .x
        movzx ebp, byte [zajuste]
        cmp byte [hay_img], 0
        jne .x
        xor ebp, ebp
.x:
        ret

; el pie: «FOTO.PNG    640 x 480    100 %    3 de 27»
pinta_pie:
        pushad
        mov ecx, [cli_an]
        mov edx, [cli_al]
        call br_pie                 ; (CF=1: ha puesto el nombre de un botón)
        jc .x
        mov esi, t_sin_img
        cmp byte [nombre], 0
        je .pon
        mov edi, linea
        cmp byte [mini], 0
        je .normal
        mov esi, [mini_sel]         ; en las miniaturas: la elegida
        imul esi, LST_TAM
        add esi, lst
        call hvl_cad
        mov esi, t_sep
        call hvl_cad
        mov eax, [mini_sel]
        inc eax
        call hvl_num
        mov esi, t_de
        call hvl_cad
        mov eax, [lst_n]
        call hvl_num
        mov esi, t_sep
        call hvl_cad
        mov esi, t_mini_pie
        call hvl_cad
        jmp .fin
.normal:
        mov esi, [nom_corto]
        call hvl_cad
        cmp byte [hay_img], 0
        je .cuenta
        cmp byte [retocada], 0
        je .entera
        mov esi, t_retocada
        call hvl_cad
.entera:
        cmp byte [elegir], 0
        je .medidas
        ; eligiendo: lo elegido y qué se puede hacer con ello
        mov esi, t_sep
        call hvl_cad
        mov esi, t_el_pista
        cmp byte [el_hay], 0
        je .el1
        mov esi, t_el_hay
        call hvl_cad
        mov eax, [el_x1]
        sub eax, [el_x0]
        call hvl_num
        mov esi, t_x
        call hvl_cad
        mov eax, [el_y1]
        sub eax, [el_y0]
        call hvl_num
        mov esi, t_sep
        call hvl_cad
        mov esi, t_el_que
.el1:
        call hvl_cad
        jmp .fin1
.medidas:
        mov esi, t_sep
        call hvl_cad
        mov eax, [g_an]
        call hvl_num
        mov esi, t_x
        call hvl_cad
        mov eax, [g_al]
        call hvl_num
        mov esi, t_sep
        call hvl_cad
        call zoom_pct
        call hvl_num
        mov esi, t_pct
        call hvl_cad
.cuenta:
        cmp dword [lst_n], 0
        je .red
        mov esi, t_sep
        call hvl_cad
        mov eax, [lst_i]
        inc eax
        call hvl_num
        mov esi, t_de
        call hvl_cad
        mov eax, [lst_n]
        call hvl_num
.red:
        cmp byte [hay_img], 0
        je .fin
        call an_pie
        cmp dword [f_red], 1
        jbe .fin
        mov esi, t_sep
        call hvl_cad
        mov esi, t_reducida
        call hvl_cad
        mov eax, [f_red]
        call hvl_num
.fin:
        cmp byte [pase], 0
        je .fin1
        mov esi, t_sep
        call hvl_cad
        mov esi, t_en_pase
        call hvl_cad
.fin1:
        mov byte [edi], 0
        mov esi, linea
.pon:
        COLOR C_TINTA
        mov eax, 8
        mov ebx, [cli_al]
        sub ebx, BR_PIE - 3
        HV dc_texto
.x:
        popad
        ret

%include "barra.inc"
%include "hvplib.inc"
%include "desinfla_t.inc"
%ifdef MI_CON_JPEG
%include "jpeg.inc"
%endif
%include "apaleta.inc"
%include "reduce.inc"
%include "nucleo.inc"
%include "bmp.inc"
%include "ico.inc"
%include "pcx.inc"
%include "gif.inc"
%include "png.inc"
%include "tga.inc"
%include "pnm.inc"
%include "jpg.inc"
%include "carga.inc"
%include "carpeta.inc"
%include "vista.inc"
%include "salida.inc"
%include "mini.inc"
%include "guarda.inc"
%include "retoca.inc"
%include "anima.inc"

        section .bss
        alignb 4
param       resd 1
ven         resd 1
puente      resd 1
barra_v     resd 1
barra_h     resd 1
cli_an      resd 1
cli_al      resd 1
; ---- la imagen ----
mapa_p      resd 1                  ; la imagen en la paleta, un octeto por punto
mapa_h      resd 1
orig_p      resd 1                  ; el original en color verdadero (si hay_orig)
orig_h      resd 1
vista_p     resd 1                  ; la imagen reducida a la ampliación de ahora (si hay_vista)
vista_h     resd 1
vista_zoom  resd 1
vi_an       resd 1
vi_al       resd 1
vbuf_p      resd 1                  ; lo que se ve, cuando hay que ampliar
vbuf_h      resd 1
vb_an       resd 1
vb_al       resd 1
ma_an       resd 1                  ; medidas del mapa
ma_al       resd 1
g_an        resd 1                  ; medidas de la imagen del archivo (como está girada)
g_al        resd 1
f_red       resd 1                  ; reducida a 1/f_red al leerla
f_clase     resd 1
f_bits      resd 1
f_ncol      resd 1
f_fmt       resd 1                  ; el nombre del formato
f_extra     resd 1
f_tam       resd 1
av_txt      resd 1
err_txt     resd 1                  ; por qué no se ve el archivo de ahora
; ---- cómo se ve ----
zoom        resd 1                  ; 16.16 (0x10000 = 100 %)
vy          resd 1                  ; la zona de la imagen, dentro de la ventana
vw          resd 1
vh          resd 1
pa_an       resd 1                  ; lo que mide la imagen con la ampliación de ahora
pa_al       resd 1
ox          resd 1                  ; dónde cae dentro de la zona, si es menor
oy          resd 1
sx          resd 1                  ; el desplazamiento
sy          resd 1
sx_max      resd 1
sy_max      resd 1
vis_an      resd 1                  ; lo que se ve de ella
vis_al      resd 1
arr_x       resd 1
arr_y       resd 1
trama       resd 1
or_rot      resd 1                  ; cómo está girada: cuartos de vuelta a la derecha...
or_volt     resd 1                  ; ...y después, volteada en horizontal o no
tr_calla    resd 1                  ; 1: se está rehaciendo el giro (no se anota)
pase_seg    resd 1
pc_estilo   resd 1
pc_menu     resd 1
pc_x        resd 1
pc_y        resd 1
pc_an       resd 1
pc_al       resd 1
pc_zoom     resd 1
nom_corto   resd 1                  ; el nombre sin la carpeta, dentro de «nombre»
nuc_esp     resd 1
pc_zaj      resb 1
hay_img     resb 1
hay_orig    resb 1
hay_vista   resb 1
hay_vbuf    resb 1
f_alfa      resb 1
f_aviso     resb 1
zajuste     resb 1
sucio       resb 1
arrastra    resb 1
ajedrez     resb 1
pantc       resb 1
pase        resb 1
pase_pausa  resb 1
            alignb 4
titulo      resb 160
nombre      resb 132                ; el archivo, con su ruta entera
nom_tmp     resb 132
ruta        resb 132
linea       resb 1100
rl          resb RL_TAM
            CARGA_BSS
            CARPETA_BSS
            SALIDA_BSS
            MINI_BSS
            GUARDA_BSS
            RETOCA_BSS
            ANIMA_BSS
            BARRA_BSS
            DESINFLA_T_BSS
%ifdef MI_CON_JPEG
            JPEG_BSS
%endif
            APALETA_BSS
            REDUCE_BSS
            NUCLEO_BSS
            BMP_BSS
            ICO_BSS
            PCX_BSS
            GIF_BSS
            PNG_BSS
            TGA_BSS
            PNM_BSS
            JPG_BSS
            alignb 4
nuc_pila    resb NUC_PILA
HVP_FIN
