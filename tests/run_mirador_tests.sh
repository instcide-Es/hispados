#!/bin/bash
# Prueba de Mirador (MIRADOR.HVP), el visor de imágenes, del grupo «Accesorios».
#
# Primero, fuera de Hispa Ventanas (deprisa, en Linux, con el mismo código):
#  - los formatos (programas/mirador/*.inc): BMP, PCX, GIF, PNG, TGA, ICO,
#    PBM/PGM/PPM y JPEG, en todas sus variantes, punto a punto contra lo que
#    tiene que salir (y contra Pillow), lo que no se admite, archivos cortados
#    y miles de archivos estropeados al azar: tests/tools/mirador_formatos.py;
#  - el paso a la paleta de Hispa Ventanas (difusión de error, tramado y más
#    cercano) y el cambio de tamaño: tests/tools/mirador_color.py.
# Después abre Hispa Ventanas con Mirador (clave «Reabrir=» de CENTRO.INI) y:
#  A. abre una imagen dada como parámetro y pasa por todas las de la carpeta
#     (una de cada formato): cada una se reconoce en la pantalla; el pie
#     («3 de 10»), el nombre de los botones, Inicio y Fin, la vuelta;
#  B. ampliar, reducir, tamaño real, ajustar, la rueda; desplazar con las
#     flechas, con las barras y arrastrando; Ctrl y la rueda; girar y voltear; pantalla
#     completa y pase de diapositivas (pausa y salida);
#  C. abrir con el diálogo; lo que no sabe (lo que no es una imagen, una
#     variante que no admite, un archivo que no existe, uno cortado, que se ve
#     a medias con su aviso); las enormes, que se reducen al leerlas; la
#     información; el menú Ver (tramado, más cercano, lo transparente); la
#     foto de un móvil, que se gira sola (EXIF), con su cámara y su fecha;
#  D. guardar como BMP (24 bits y 256 colores), como PNG y como PCX, copiar
#     el nombre e imprimir
#     (LPT1 a un archivo), derecha y apaisada: lo guardado y lo impreso se
#     comparan con la imagen;
#  E. las miniaturas de la carpeta: la rejilla con una de cada imagen y su
#     nombre, elegir con las flechas y con un clic, abrir con Intro y con
#     doble clic, y volver con Esc.
#  F. los retoques: a grises, invertir, brillo y contraste, cambiar de tamaño
#     (a un tanto por ciento y a unas medidas) y recortar a lo elegido con el
#     ratón (el marco, sus bordes, moverlo, Esc e Intro); deshacer y rehacer;
#     «(retocada)» en el pie; la pregunta al pasar a otra imagen o cerrar; lo
#     guardado, punto por punto contra las mismas cuentas hechas aquí; y lo
#     mismo sin el original en color verdadero (con 4 MB).
#  G. los GIF animados: se pasan solos, con sus tiempos y en orden (cuadros
#     parciales que se acumulan); Espacio los para y los sigue; a pantalla
#     completa, ampliados y en el pase; el que se ve una sola vez se queda en
#     el último cuadro; la información; girar lo deja en un cuadro; y el que
#     no cabe en la memoria (4 MB) se queda en el primero y lo dice.
# Con MIRADOR_SOLO=ABCDEFG (las letras que se quieran) se pasan solo esas partes
# de Hispa Ventanas; con MIRADOR_SOLO=N, solo lo de fuera.
#
# Necesita qemu-system-i386, mtools, socat, ImageMagick y Pillow (de Python).
# Si el Python del sistema no tiene Pillow, se usa el de MIRADOR_PY (un
# entorno virtual, por ejemplo: python3 -m venv DIR; DIR/bin/pip install
# Pillow; MIRADOR_PY=DIR/bin/python).
set -u
IMG=${1:-build/hispados.img}
. tests/tools/hvp_arnes.sh
HVP=$B/MIRADOR.HVP
SOLO=${MIRADOR_SOLO:-NABCDEFG}
PY=${MIRADOR_PY:-python3}
$PY -c 'import PIL' 2>/dev/null || { ko "falta Pillow: ponga en MIRADOR_PY un Python que lo tenga"; exit 1; }
NASM=${NASM:-nasm} python3 tools/mkhvp.py programas/mirador.asm $HVP > $T/mkhvp.txt 2>&1 || { cat $T/mkhvp.txt; ko "MIRADOR.HVP no se ensambla"; exit 1; }
I=$T/img
$PY tests/tools/mirador_imagenes.py haz $I || { ko "mirador_imagenes.py no hace las imágenes"; exit 1; }
reabre() { printf '[Centro]\r\nReabrir=A:\\HISPADOS\\PROGS\\MIRADOR.HVP %s\r\n' "$1" > $T/CENTRO.INI
           mcopy -o -i $T/f.img $T/CENTRO.INI ::HISPADOS/CENTRO.INI; }
# el disquete, con Mirador y sitio para las imágenes (los tipos de letra no le hacen falta)
disco() { prepara $HVP > /dev/null; mdel -i $T/f.img '::HISPADOS/PROGS/TIPOS*.DAT' 2>/dev/null; }
mete() { local f; for f in "$@"; do mcopy -o -i $T/f.img $I/$f :: || ko "no cabe $f en el disquete de la prueba"; done; }
ve() { $PY tests/tools/mirador_imagenes.py ve "$@" || fail=1; }
pie() { chk $T/$1.ppm busca 46 551 56 559 0 6 "$2"; }           # el pie de la ventana
avisa() { chk $T/$1.ppm busca 46 106 56 114 13 8 "$2"; }         # la franja del aviso, en granate
dice() { chk $T/$1.ppm busca 120 150 680 440 0 8 "$2"; }         # un mensaje, en su diálogo
nove() { chk $T/$1.ppm busca 42 108 700 520 13 8 "$2"; }         # por qué no se ve, en la zona de la imagen
rueda() { mon "mouse_move 0 0 $1" 0.5; }                           # (-1: hacia abajo)
arranca_rueda() { sleep 6; send 'KEYB SP' 2; send 'A:\HISPADOS\HSPRATON /DIRECTO' 3; send 'VENTANAS' 6; aparca; }
# la zona de la imagen va de 42,108 a 741,535 (700 x 428); una de 320x200 a su tamaño cae en 232,222

if [[ $SOLO == *N* ]]; then
echo "== Mirador: los formatos, punto a punto, cortados y estropeados =="
$PY tests/tools/mirador_formatos.py --rotos ${MIRADOR_ROTOS:-6000} || fail=1
echo "== Mirador: a la paleta de Hispa Ventanas y cambios de tamaño =="
python3 tests/tools/mirador_color.py || fail=1
fi

if [[ $SOLO == *A* ]]; then
echo "== Mirador A: una imagen de cada formato; pasar por la carpeta =="
disco
mete BARRAS.BMP BARRAS.PCX CASA.PPM FLECHA.ICO FLECHA.PNG LAGO.GIF MONTE.TGA PAISAJE.PNG PLAYA.JPG
mcopy -o -i $T/f.img $I/NOTA.BMP ::LEEME.TXT                    # (uno que no es imagen ni lo parece: no cuenta)
reabre 'A:\PAISAJE.PNG'
{ arranca
  shot a1
  ir 181 90; shot a2; clic 181 90; sleep 1.5; aparca; shot a3    # el nombre del botón, en el pie; y la siguiente
  clic 153 90; sleep 1.5; clic 153 90; sleep 1.5; aparca; shot a4 # dos veces la anterior
  tecla home; sleep 1.5; shot a5                                  # la primera
  tecla pgdn; sleep 1.5; shot a6
  tecla pgdn; sleep 1.5; shot a7
  tecla pgdn; sleep 1.5; shot a8
  tecla pgdn; sleep 1.5; shot a9
  tecla pgdn; sleep 1.5; shot a10
  tecla end; sleep 1.5; shot a11                                  # la última
  tecla spc; sleep 1.5; shot a12                                  # y la siguiente de la última: la primera
  tecla backspace; sleep 1.5; shot a13                            # y la anterior de la primera: la última
  tecla left; sleep 1.5; shot a14                                 # (cabe entera: la flecha pasa de imagen)
  clic 310 69; sleep 0.8; clic 350 91; sleep 1.2; shot a15; tecla ret; sleep 0.8   # ? > Acerca de
  sal; } | run 200 -vga std
chk $T/a1.ppm titulo 40 34 activa "Mirador - PAISAJE.PNG"
pie a1 "PAISAJE.PNG    320 x 200    100 %    8 de 9"
ve $T/a1.ppm 232 222 320 200 $I/ref_paisaje.png "" 8 "PNG: el paisaje, a su tamaño y en su sitio"
chk $T/a1.ppm px 100 300 8 "alrededor de la imagen, crema"
pie a2 "Imagen siguiente de la carpeta (AvPág)"
chk $T/a3.ppm titulo 40 34 activa "Mirador - PLAYA.JPG"
pie a3 "PLAYA.JPG    320 x 200    100 %    9 de 9"
ve $T/a3.ppm 232 222 320 200 $I/ref_playa.png "" 8 "JPEG: el mismo paisaje"
pie a4 "MONTE.TGA    320 x 200    100 %    7 de 9"
ve $T/a4.ppm 232 222 320 200 $I/ref_paisaje.png "" 8 "TGA con RLE: el paisaje"
pie a5 "BARRAS.BMP    320 x 160    100 %    1 de 9"
ve $T/a5.ppm 232 242 320 160 $I/ref_barras.png "" 3 "BMP de 256 colores: las barras"
chk $T/a5.ppm rgb 250 260 FFFFFF "BMP: la barra blanca, exacta (sin tramar)"
chk $T/a5.ppm rgb 290 260 FFFF00 "BMP: la amarilla"
chk $T/a5.ppm rgb 330 260 00FFFF "BMP: la turquesa"
chk $T/a5.ppm rgb 370 260 00FF00 "BMP: la verde"
chk $T/a5.ppm rgb 410 260 FF00FF "BMP: la magenta"
chk $T/a5.ppm rgb 450 260 FF0000 "BMP: la roja"
chk $T/a5.ppm rgb 490 260 0000FF "BMP: la azul"
chk $T/a5.ppm rgb 530 260 000000 "BMP: la negra"
chk $T/a5.ppm rgb 300 390 CCCCCC "BMP: el gris claro de abajo"
chk $T/a5.ppm rgb 500 390 666666 "BMP: el gris oscuro"
pie a6 "BARRAS.PCX    320 x 160    100 %    2 de 9"
ve $T/a6.ppm 232 242 320 160 $I/ref_barras.png "" 3 "PCX de 16 colores: las barras"
chk $T/a6.ppm rgb 450 260 FF0000 "PCX: la barra roja, exacta"
chk $T/a6.ppm rgb 500 390 666666 "PCX: el gris oscuro"
pie a7 "CASA.PPM    160 x 100    100 %    3 de 9"
ve $T/a7.ppm 312 272 160 100 $I/ref_casa.png "" 8 "PPM: el paisaje pequeño"
pie a8 "FLECHA.ICO    48 x 48    100 %    4 de 9"
ve $T/a8.ppm 368 298 48 48 $I/ref_flecha48.png "" 10 "ICO: la imagen mayor (48x48), con lo transparente ajedrezado"
pie a9 "FLECHA.PNG    64 x 64    100 %    5 de 9"
ve $T/a9.ppm 360 290 64 64 $I/ref_flecha.png "" 6 "PNG con opacidad: la flecha sobre el ajedrezado"
chk $T/a9.ppm rgb 372 350 FFFFFF "lo transparente: un cuadro blanco del ajedrezado"
chk $T/a9.ppm rgb 364 350 CCCCCC "y uno gris"
pie a10 "LAGO.GIF    320 x 200    100 %    6 de 9"
ve $T/a10.ppm 232 222 320 200 $I/ref_lago.png "" 8 "GIF entrelazado: el paisaje"
pie a11 "PLAYA.JPG    320 x 200    100 %    9 de 9"
pie a12 "BARRAS.BMP    320 x 160    100 %    1 de 9"
pie a13 "PLAYA.JPG    320 x 200    100 %    9 de 9"
pie a14 "PAISAJE.PNG    320 x 200    100 %    8 de 9"
dice a15 "El visor de imágenes de Hispa Ventanas:"
dice a15 "Copyright (C) 2026 Carlos Sanz y contribuidores."
muestra a1 mirador_paisaje; muestra a9 mirador_flecha; muestra a5 mirador_barras
fi

if [[ $SOLO == *B* ]]; then
echo "== Mirador B: ampliar, desplazar, girar, pantalla completa y pase =="
disco
mete ANCHA.JPG PAISAJE.PNG TORRE.PNG FLECHA.PNG
reabre 'A:\ANCHA.JPG'
{ arranca_rueda
  shot b1                                                         # no cabe: ajustada a la ventana
  tecla 1; sleep 1; shot b2                                       # tamaño real: la esquina de arriba
  tecla right; sleep 0.6; shot b3; tecla down; sleep 0.6; shot b4 # las flechas desplazan
  clic 734 544; sleep 0.6; aparca; shot b5                        # la flecha derecha de la barra horizontal
  clic 750 528; sleep 0.6; aparca; shot b6                        # y la de abajo de la vertical
  ir 400 300; mon "mouse_button 1" 0.3; ir 360 250; mon "mouse_button 0" 0.4; aparca; shot b7   # arrastrada
  tecla shift-bracket_right; sleep 1.5; shot b8                   # * : ajustada otra vez
  tecla slash; sleep 1.5; shot b9                                 # - : al 50 %
  tecla bracket_right bracket_right; sleep 1.5; shot b10          # + + : al 75 %
  ir 400 300; rueda -1; sleep 1.2; shot b11; rueda 1; sleep 1.2; aparca; shot b12   # la rueda reduce y amplía
  ir 400 300; mon "sendkey ctrl 2500" 0.4; rueda -1; sleep 3.5; aparca; shot c1   # Ctrl y la rueda: la siguiente, FLECHA.PNG (64x64)
  tecla bracket_right bracket_right bracket_right bracket_right; sleep 1; shot c2   # al 400 %: cada punto, un cuadro
  tecla pgdn; sleep 2; shot c3                                    # PAISAJE.PNG
  tecla r; sleep 1.5; shot c4                                     # girada a la derecha
  tecla r; sleep 1.5; shot c5                                     # media vuelta
  tecla l l; sleep 1.5; tecla l; sleep 1.5; shot c6               # a la izquierda
  clic 365 90; sleep 1.5; aparca; shot c7                         # botón: girar a la derecha (derecha otra vez)
  tecla h; sleep 1.5; shot c8                                     # volteada en horizontal
  tecla h v; sleep 1.5; shot c9                                   # y en vertical
  clic 266 69; sleep 0.8; shot m1; clic 300 159; sleep 1.5; aparca; shot c10   # Imagen > Voltear en vertical: como estaba
  tecla f11; sleep 2; shot d1                                     # pantalla completa
  tecla pgdn; sleep 2.5; shot d2                                  # allí también se pasa: TORRE.PNG
  tecla esc; sleep 2; shot d3
  tecla ret; sleep 2; shot d4; tecla ret; sleep 2; shot d5        # Intro entra y sale
  tecla home; sleep 3
  tecla f11; sleep 2.5; tecla f5; sleep 1.5; shot e1              # el pase (cada 5 segundos), a pantalla completa: ANCHA.JPG
  sleep 3.5; shot e2                                              # a los cinco segundos, la siguiente
  tecla spc; sleep 6.5; shot e3                                   # en pausa: no pasa
  tecla spc; sleep 6.5; shot e4                                   # sigue: a los cinco segundos, la tercera
  tecla esc; sleep 2; shot e5; sleep 6; shot e6                   # fuera: en la ventana, y ya no pasa sola
  tecla f5; sleep 6.5; shot e7; tecla esc; sleep 1; shot e8       # el pase, en la ventana: a los cinco segundos, la cuarta
  tecla r; sleep 1.5; clic 211 69; sleep 0.8; clic 260 395; sleep 2.5; aparca; shot e9   # girada, y se cambia el tramado: sigue girada
  clic 211 69; sleep 0.8; clic 260 375; sleep 2.5; aparca         # (y otra vez con difusión)
  clic 211 69; sleep 0.8; clic 260 307; sleep 0.8                 # Ver > Pase: cada 3 segundos
  clic 211 69; sleep 0.8; shot m2; tecla esc; sleep 0.5           # el menú Ver, con sus marcas
  sal; } | run 280 -vga std
chk $T/b1.ppm titulo 40 34 activa "Mirador - ANCHA.JPG"
pie b1 "ANCHA.JPG    1280 x 800    53 %    1 de 4"
ve $T/b1.ppm 50 108 684 427 $I/ref_ancha.png "" 8 "una de 1280x800, ajustada a la ventana y suavizada"
pie b2 "ANCHA.JPG    1280 x 800    100 %    1 de 4"
# (al pasar de ajustada a tamaño real se queda viendo el centro: de 290,186 en adelante)
$PY tests/tools/mirador_imagenes.py ve $T/b2.ppm 42 108 700 428 $I/ref_ancha.png "290 186" 8 "a tamaño real se ve el centro de la imagen, punto por punto" || fail=1
chk $T/b3.ppm igual 60 120 600 380 $T/b2.ppm 92 120 0       # flecha derecha: 32 puntos
chk $T/b4.ppm igual 60 120 600 380 $T/b3.ppm 60 152 0       # flecha abajo: 32 puntos
chk $T/b5.ppm igual 60 120 600 380 $T/b4.ppm 92 120 0       # la barra horizontal: otros 32
chk $T/b6.ppm igual 60 120 600 380 $T/b5.ppm 60 152 0       # la vertical: otros 32
chk $T/b7.ppm igual 60 120 600 360 $T/b6.ppm 100 170 0      # arrastrada 40 a la izquierda y 50 arriba
pie b8 "ANCHA.JPG    1280 x 800    53 %    1 de 4"
chk $T/b8.ppm igual 50 108 684 427 $T/b1.ppm 50 108 0       # ajustada: como al abrirla
pie b9 "ANCHA.JPG    1280 x 800    50 %    1 de 4"
ve $T/b9.ppm 72 122 640 400 $I/ref_ancha.png "" 8 "al 50 %: entera y centrada"
pie b10 "ANCHA.JPG    1280 x 800    75 %    1 de 4"
pie b11 "ANCHA.JPG    1280 x 800    67 %    1 de 4"
pie b12 "ANCHA.JPG    1280 x 800    75 %    1 de 4"
pie c1 "FLECHA.PNG    64 x 64    100 %    2 de 4"
pie c2 "FLECHA.PNG    64 x 64    400 %    2 de 4"
ve $T/c2.ppm 264 194 256 256 $I/ref_flecha.png "" 8 "al 400 %: la flecha, con cada punto hecho un cuadro"
chk $T/c2.ppm rgb 290 222 0066CC "al 400 %: el cuadrado azul"
pie c3 "PAISAJE.PNG    320 x 200    100 %    3 de 4"
pie c4 "PAISAJE.PNG    200 x 320    100 %    3 de 4"
ve $T/c4.ppm 292 162 200 320 $I/ref_paisaje.png der 8 "girada a la derecha"
pie c5 "PAISAJE.PNG    320 x 200    100 %    3 de 4"
ve $T/c5.ppm 232 222 320 200 $I/ref_paisaje.png media 8 "girada media vuelta"
ve $T/c6.ppm 292 162 200 320 $I/ref_paisaje.png izq 8 "girada a la izquierda"
ve $T/c7.ppm 232 222 320 200 $I/ref_paisaje.png "" 8 "y otra vez derecha, con el botón"
ve $T/c8.ppm 232 222 320 200 $I/ref_paisaje.png h 8 "volteada en horizontal"
ve $T/c9.ppm 232 222 320 200 $I/ref_paisaje.png v 8 "volteada en vertical"
# (en los menús no se busca la letra subrayada)
chk $T/m1.ppm busca 240 145 260 165 0 8 "Voltear en "
ve $T/c10.ppm 232 222 320 200 $I/ref_paisaje.png "" 8 "y volteada otra vez, con el menú: como estaba"
chk $T/d1.ppm px 10 10 0 "pantalla completa: negro hasta la esquina"
chk $T/d1.ppm px 790 590 0 "pantalla completa: y hasta la otra"
ve $T/d1.ppm 240 200 320 200 $I/ref_paisaje.png "" 8 "pantalla completa: la imagen, en el centro"
ve $T/d2.ppm 240 200 320 200 $I/ref_torre.png "" 8 "pantalla completa: la siguiente"
chk $T/d3.ppm titulo 40 34 activa "Mirador - TORRE.PNG"
pie d3 "TORRE.PNG    320 x 200    100 %    4 de 4"
chk $T/d4.ppm px 10 10 0 "Intro: a pantalla completa"
chk $T/d5.ppm titulo 40 34 activa "Mirador - TORRE.PNG"
ve $T/e1.ppm 0 50 800 500 $I/ref_ancha.png "" 8 "el pase: la primera, a toda la pantalla"
ve $T/e2.ppm 368 268 64 64 $I/ref_flecha.png "" 6 "el pase: a los cinco segundos, la segunda"
chk $T/e3.ppm busca 8 8 30 30 8 0 "En pausa (Espacio: seguir; Esc: salir)"
ve $T/e3.ppm 368 268 64 64 $I/ref_flecha.png "" 6 "en pausa no pasa: sigue la segunda"
ve $T/e4.ppm 240 200 320 200 $I/ref_paisaje.png "" 8 "el pase: tras la pausa, la tercera"
chk $T/e5.ppm titulo 40 34 activa "Mirador - PAISAJE.PNG"
chk $T/e6.ppm igual 60 120 600 380 $T/e5.ppm 60 120 0       # parado: ya no pasa
chk $T/e7.ppm titulo 40 34 activa "Mirador - TORRE.PNG"
pie e7 "TORRE.PNG    320 x 200    100 %    4 de 4    pase de diapositivas"
pie e8 "TORRE.PNG    320 x 200    100 %    4 de 4"
chk $T/e8.ppm nobusca 46 551 400 559 0 6 "pase de diapositivas"
pie e9 "TORRE.PNG    200 x 320    100 %    4 de 4"
ve $T/e9.ppm 292 162 200 320 $I/ref_torre.png der 8 "al cambiar el tramado sigue girada"
chk $T/m2.ppm busca 200 294 216 304 0 8 "* Pase: cada "           # la marca, en «cada 3 segundos»...
chk $T/m2.ppm nobusca 200 314 216 324 0 8 "* Pase: cada "         # ...y ya no en «cada 5»
chk $T/m2.ppm busca 200 206 216 216 0 8 "* A"                    # Ajustar a la ventana
chk $T/m2.ppm busca 200 362 216 372 0 8 "* Colores: di"          # difusión de error
chk $T/m2.ppm nobusca 200 382 216 392 0 8 "* Colores: tramado "
chk $T/m2.ppm busca 200 422 216 432 0 8 "* Lo transparente, a"
muestra b1 mirador_ancha; muestra c2 mirador_400; muestra c4 mirador_girada; muestra e1 mirador_pase; muestra m2 mirador_menu_ver; muestra e3 mirador_pausa
fi

if [[ $SOLO == *C* ]]; then
echo "== Mirador C: el diálogo de abrir; lo que no sabe; las enormes; información y colores =="
disco
mete LAGO.GIF NOTA.BMP RARO.BMP CORTADA.PNG ENORME.PNG FOTON.JPG FLECHA.PNG PAISAJE.PNG MOVIL.JPG
{ arranca
  shot f1                                                         # sin imagen
  tecla i; sleep 1; shot f2; tecla ret; sleep 0.6                 # información sin imagen: lo dice
  tecla ctrl-a; sleep 1.2; escribe "lago.gif"; tecla ret; sleep 2.5; shot f3
  tecla ctrl-a; sleep 1.2; escribe "nota.bmp"; tecla ret; sleep 2; shot f4       # no es una imagen
  tecla ctrl-a; sleep 1.2; escribe "raro.bmp"; tecla ret; sleep 2; shot f5       # una variante que no se admite
  tecla ctrl-a; sleep 1.2; escribe "noesta.png"; tecla ret; sleep 2; shot f6     # no existe
  tecla ctrl-a; sleep 1.2; escribe "cortada.png"; tecla ret; sleep 2.5; shot f7  # cortada: se ve la mitad
  tecla i; sleep 1; shot f8; tecla ret; sleep 0.6
  tecla ctrl-a; sleep 1.2; escribe "enorme.png"; tecla ret; sleep 12; shot g1    # 3000x2000: reducida a la mitad
  tecla i; sleep 1; shot g2; tecla ret; sleep 0.6
  tecla 1; sleep 1.5; shot g3                                     # punto por punto (1500x1000): el centro
  tecla ctrl-a; sleep 1.2; escribe "foton.jpg"; tecla ret; sleep 12; shot g4     # 2560x1920: la reduce el propio JPEG
  tecla ctrl-a; sleep 1.2; escribe "paisaje.png"; tecla ret; sleep 2.5
  tecla i; sleep 1; shot g5; tecla ret; sleep 0.6
  clic 211 69; sleep 0.8; clic 260 395; sleep 2.5; aparca; shot h1               # Ver > Colores: tramado ordenado
  clic 211 69; sleep 0.8; clic 260 415; sleep 2.5; aparca; shot h2               # Ver > Colores: el más cercano
  clic 211 69; sleep 0.8; shot m3; clic 260 375; sleep 2.5; aparca; shot h3      # Ver > Colores: difusión de error
  tecla ctrl-a; sleep 1.2; escribe "flecha.png"; tecla ret; sleep 2.5
  clic 211 69; sleep 0.8; clic 260 435; sleep 2.5; aparca; shot h4               # Ver > Lo transparente, ajedrezado: fuera
  tecla ctrl-a; sleep 1.2; escribe "movil.jpg"; tecla ret; sleep 2.5; shot j1    # la foto de un móvil: se gira sola
  tecla i; sleep 1; shot j2; tecla ret; sleep 0.6
  sal; } | run 280 -vga std
chk $T/f1.ppm titulo 40 34 activa "Mirador"
pie f1 "No hay ninguna imagen abierta."
chk $T/f1.ppm busca 150 250 500 400 1 8 "Archivo > Abrir (Ctrl+A) para ver una imagen"
dice f2 "Primero abra una imagen: Archivo > Abrir (Ctrl+A)."
chk $T/f3.ppm titulo 40 34 activa "Mirador - LAGO.GIF"
ve $T/f3.ppm 232 222 320 200 $I/ref_lago.png "" 8 "abierta con el diálogo"
chk $T/f4.ppm titulo 40 34 activa "Mirador - NOTA.BMP"
nove f4 "Este archivo no es una imagen de las que Mirador conoce:"
pie f4 "NOTA.BMP    7 de 9"
nove f5 "Mirador no sabe ver esta imagen: es "
nove f5 "un BMP con una compresión que Mirador no conoce (JPEG, PNG o Huffman)."
nove f6 "No se ha podido abrir el archivo."
chk $T/f7.ppm titulo 40 34 activa "Mirador - CORTADA.PNG"
avisa f7 "Aviso: el archivo está cortado o dañado; se ve lo que se ha podido leer."
ve $T/f7.ppm 232 232 320 96 $I/ref_paisaje.png "" 200 "(la cortada)" > /dev/null
$PY - $T/f7.ppm $I/ref_paisaje.png <<'EOF' || fail=1
import sys, re
from PIL import Image
d = open(sys.argv[1], 'rb').read()
m = re.match(rb'P6\s+(\d+)\s+(\d+)\s+(\d+)\s', d)
cap = Image.frombytes('RGB', (int(m.group(1)), int(m.group(2))), d[m.end():])
ref = Image.open(sys.argv[2]).convert('RGB')
def dif(y0, y1):
    a = cap.crop((232, 232 + y0, 552, 232 + y1)).resize((40, (y1 - y0) // 8), Image.Resampling.BOX)
    b = ref.crop((0, y0, 320, y1)).resize((40, (y1 - y0) // 8), Image.Resampling.BOX)
    return sum(abs(p - q) for u, v in zip(a.getdata(), b.getdata()) for p, q in zip(u, v)) / (a.size[0] * a.size[1] * 3)
arriba, abajo = dif(0, 64), dif(160, 200)
liso = cap.getpixel((300, 232 + 190))
ok = arriba < 8 and abajo > 20 and all(abs(a - b) <= 10 for a, b in zip(liso, (0xD8, 0xD2, 0xC4)))
print('  %s la cortada: arriba es el paisaje (%.1f) y lo que falta va liso, de color piedra (%s)' % ('OK  ' if ok else 'FAIL', arriba, '%02X%02X%02X' % liso))
sys.exit(0 if ok else 1)
EOF
dice f8 "El archivo está cortado o dañado: falta una parte."
pie g1 "ENORME.PNG    3000 x 2000    21 %    2 de 9    reducida a 1/2"
ve $T/g1.ppm 71 108 641 427 $I/ref_enorme.png "" 6 "un PNG de 3000x2000: reducido al leerlo, entero en la ventana"
dice g2 "Medidas: 3.000 x 2.000 puntos"
dice g2 "Colores: 5 colores, con paleta"
dice g2 "En memoria: 1.500 x 1.000 puntos a 256 colores (reducida a 1/2)"
pie g3 "ENORME.PNG    3000 x 2000    50 %    2 de 9    reducida a 1/2"
chk $T/g3.ppm rgb 250 200 FF0000 "el mapa reducido, punto por punto: el centro, con el cuadrante rojo"
chk $T/g3.ppm rgb 550 200 00FF00 "el verde"
chk $T/g3.ppm rgb 250 450 0000FF "el azul"
chk $T/g3.ppm rgb 550 450 FFFF00 "y el amarillo"
pie g4 "FOTON.JPG    2560 x 1920    22 %    4 de 9    reducida a 1/2"
ve $T/g4.ppm 107 108 570 427 $I/ref_foton.png "" 6 "un JPEG de 2560x1920: lo reduce su descodificador"
dice g5 "Archivo: A:\\PAISAJE.PNG"
dice g5 "Formato: PNG"
dice g5 "Medidas: 320 x 200 puntos"
dice g5 "Colores: color verdadero de 24 bits"
dice g5 "Tamaño del archivo: $(python3 -c "import os,sys; print(format(os.path.getsize(sys.argv[1]), ',').replace(',', '.'))" $I/PAISAJE.PNG) octetos"
dice g5 "En memoria: 320 x 200 puntos a 256 colores,"
dice g5 "y además en color verdadero (se puede guardar en 24 bits)"
ve $T/h1.ppm 232 222 320 200 $I/ref_paisaje.png "" 8 "con tramado ordenado"
chk $T/h1.ppm cambia 232 222 320 200 $T/h3.ppm 8000
ve $T/h2.ppm 232 222 320 200 $I/ref_paisaje.png "" 22 "al color más cercano (se parece menos: sin tramar)"
chk $T/h2.ppm cambia 232 222 320 200 $T/h3.ppm 8000
chk $T/m3.ppm busca 200 402 216 412 0 8 "* Colores: el más cerca"
ve $T/h3.ppm 232 222 320 200 $I/ref_paisaje.png "" 8 "y otra vez con difusión de error"
ve $T/h4.ppm 360 290 64 64 $I/ref_flecha_b.png "" 6 "lo transparente, sobre blanco liso"
chk $T/h4.ppm rgb 364 350 FFFFFF "donde había un cuadro gris, blanco"
chk $T/j1.ppm titulo 40 34 activa "Mirador - MOVIL.JPG"
pie j1 "MOVIL.JPG    320 x 200    100 %    6 de 9"
ve $T/j1.ppm 232 222 320 200 $I/ref_paisaje.png "" 8 "la foto del móvil (guardada tumbada, EXIF 6): se ve derecha"
dice j2 "Cámara: Hispa Camara 1"
dice j2 "Foto hecha el: 16/11/2026 12:34"
muestra f5 mirador_no_sabe; muestra f7 mirador_cortada; muestra g1 mirador_enorme; muestra g5 mirador_info
muestra h1 mirador_ordenado; muestra h2 mirador_cercano; muestra h3 mirador_difusion
fi

if [[ $SOLO == *D* ]]; then
echo "== Mirador D: guardar como BMP, copiar el nombre e imprimir =="
disco
mete CASA.PPM PAISAJE.PNG TORRE.PNG
reabre 'A:\PAISAJE.PNG'
{ arranca
  tecla ctrl-g; sleep 1.2; tecla end; shot s1; tecla ret; sleep 3; shot s2; tecla ret; sleep 1     # el nombre que propone: PAISAJE.BMP (24 bits)
  clic 83 69; sleep 0.8; clic 150 139; sleep 1.2; escribe "ocho"; tecla ret; sleep 3; shot s3; tecla ret; sleep 1   # de 256 colores: OCHO.BMP
  tecla r; sleep 1.5; tecla ctrl-g; sleep 1.2; escribe "girada.bmp"; tecla ret; sleep 3; tecla ret; sleep 1          # girada: se guarda girada
  tecla l; sleep 1.5
  tecla home; sleep 2.5                                           # CASA.PPM (más pequeña: el disquete se llena)
  clic 83 69; sleep 0.8; clic 150 159; sleep 1.2; escribe "copia"; tecla ret; sleep 3; shot s6; tecla ret; sleep 1  # como PNG: COPIA.PNG
  clic 83 69; sleep 0.8; clic 150 179; sleep 1.2; escribe "copia"; tecla ret; sleep 3; tecla ret; sleep 1           # como PCX: COPIA.PCX
  tecla pgdn; sleep 2.5; shot s7                                  # la siguiente de la carpeta: COPIA.PCX, la que se acaba de guardar
  tecla end; sleep 2; tecla pgup; sleep 2; shot s4                # y otra vez PAISAJE.PNG
  tecla ctrl-c; sleep 0.6; tecla ctrl-a; sleep 1.2; tecla ctrl-v; sleep 0.8; shot s5; tecla esc; sleep 0.8          # el nombre, copiado: se pega en el diálogo
  tecla ctrl-p; sleep 5; shot p0; sleep 26; shot p1; tecla ret; sleep 1   # imprimir: apaisada (es más ancha que alta)
  sal; } | PARALELO=file:$T/prn1.bin run 240 -vga std
# (detrás de la última letra está el cursor: no se busca)
chk $T/s1.ppm busca 150 150 600 450 0 9 "PAISAJE.BM"
dice s2 "La imagen se ha guardado."
dice s3 "La imagen se ha guardado con los 256 colores de Hispa Ventanas."
pie s4 "PAISAJE.PNG    320 x 200    100 %    7 de 8"            # (con los tres BMP, los dos PNG y el PCX guardados)
dice s6 "La imagen se ha guardado."
pie s7 "COPIA.PCX    160 x 100    100 %    2 de 8"
ve $T/s7.ppm 312 272 160 100 $I/ref_casa.png "" 8 "el PCX guardado se abre con Mirador y es la misma imagen"
chk $T/s5.ppm busca 150 150 600 450 0 9 "A:\\PAISAJE.PN"
pie p0 "Imprimiendo la imagen..."
dice p1 "La imagen se ha enviado a la impresora."
for f in PAISAJE.BMP OCHO.BMP GIRADA.BMP; do mcopy -n -i $T/f.img ::$f $T/$f 2>/dev/null || ko "no se ha guardado $f"; done
$PY tests/tools/mirador_imagenes.py bmp $T/PAISAJE.BMP $I/ref_paisaje.png 24 "" 0.01 "guardada en 24 bits: idéntica al original" || fail=1
$PY tests/tools/mirador_imagenes.py bmp $T/OCHO.BMP $I/ref_paisaje.png 8 "" 8 "guardada con 256 colores: lo que se ve" || fail=1
$PY tests/tools/mirador_imagenes.py bmp $T/GIRADA.BMP $I/ref_paisaje.png 24 der 0.01 "guardada girada" || fail=1
for f in COPIA.PNG COPIA.PCX; do mcopy -n -i $T/f.img ::$f $T/$f 2>/dev/null || ko "no se ha guardado $f"; done
$PY tests/tools/mirador_imagenes.py guardada $T/COPIA.PNG $I/ref_casa.png RGB "" 0.01 "guardada como PNG: idéntica al original" || fail=1
$PY tests/tools/mirador_imagenes.py guardada $T/COPIA.PCX $I/ref_casa.png RGB "" 0.01 "guardada como PCX: idéntica al original" || fail=1
mcopy -n -i $T/f.img ::PAISAJE.PNG $T/sal.png 2>/dev/null
cmp -s $T/sal.png $I/PAISAJE.PNG && ok "PAISAJE.PNG sigue como estaba" || ko "PAISAJE.PNG ha cambiado en el disco"
$PY tests/tools/mirador_imagenes.py impreso $T/prn1.bin $I/ref_paisaje.png apaisada "impresa apaisada, ajustada a la hoja" || fail=1
# una más alta que ancha: derecha
disco
mete TORRE.PNG
reabre 'A:\TORRE.PNG'
{ arranca
  tecla r; sleep 1.5                                              # (de pie: 200 x 320)
  clic 117 90; sleep 30; shot p2; tecla ret; sleep 1              # el botón de imprimir
  sal; } | PARALELO=file:$T/prn2.bin run 160 -vga std
dice p2 "La imagen se ha enviado a la impresora."
$PY - $I/ref_torre.png $T/ref_torre_pie.png <<'EOF'
import sys
from PIL import Image
Image.open(sys.argv[1]).transpose(Image.Transpose.ROTATE_270).save(sys.argv[2])
EOF
$PY tests/tools/mirador_imagenes.py impreso $T/prn2.bin $T/ref_torre_pie.png derecha "impresa derecha, ajustada a la hoja" || fail=1
# sin impresora: lo dice
disco
mete PAISAJE.PNG
reabre 'A:\PAISAJE.PNG'
{ arranca
  tecla ctrl-p; sleep 3; shot p3; tecla ret; sleep 1
  sal; } | run 100 -vga std
dice p3 "No se ha podido imprimir."
# con 4 MB no se guarda el original en color verdadero: lo que se guarda es lo que se ve, con paleta
disco
mete BARRAS.BMP PAISAJE.PNG
reabre 'A:\BARRAS.BMP'
{ arranca
  clic 83 69; sleep 0.8; clic 150 159; sleep 1.2; escribe "b"; tecla ret; sleep 3; shot s8; tecla ret; sleep 1      # B.PNG
  clic 83 69; sleep 0.8; clic 150 179; sleep 1.2; escribe "b"; tecla ret; sleep 3; tecla ret; sleep 1               # B.PCX
  tecla end; sleep 2.5
  clic 83 69; sleep 0.8; clic 150 159; sleep 1.2; escribe "p"; tecla ret; sleep 3; tecla ret; sleep 1               # P.PNG
  clic 83 69; sleep 0.8; clic 150 179; sleep 1.2; escribe "p"; tecla ret; sleep 3; tecla ret; sleep 1               # P.PCX
  tecla ctrl-g; sleep 1.2; escribe "p"; tecla ret; sleep 3; tecla ret; sleep 1                                      # P.BMP
  sal; } | run 150 -vga std -m 4
dice s8 "La imagen se ha guardado con los 256 colores de Hispa Ventanas."
for f in B.PNG B.PCX P.PNG P.PCX P.BMP; do mcopy -n -i $T/f.img ::$f $T/$f 2>/dev/null || ko "no se ha guardado $f"; done
$PY tests/tools/mirador_imagenes.py guardada $T/B.PNG $I/ref_barras.png P "" 0.01 "con paleta, como PNG: las barras, exactas" || fail=1
$PY tests/tools/mirador_imagenes.py guardada $T/B.PCX $I/ref_barras.png P "" 0.01 "con paleta, como PCX: las barras, exactas" || fail=1
$PY tests/tools/mirador_imagenes.py guardada $T/P.PNG $I/ref_paisaje.png P "" 8 "con paleta, como PNG: el paisaje, tramado" || fail=1
$PY tests/tools/mirador_imagenes.py guardada $T/P.PCX $I/ref_paisaje.png P "" 8 "con paleta, como PCX: el paisaje, tramado" || fail=1
$PY tests/tools/mirador_imagenes.py bmp $T/P.BMP $I/ref_paisaje.png 8 "" 8 "sin el original, Ctrl+G guarda el BMP de 256 colores" || fail=1
[ $(stat -c %s $T/B.PNG) -lt 8000 ] && ok "el PNG de las barras ocupa $(stat -c %s $T/B.PNG) octetos (su BMP, 52.278)" || ko "el PNG de las barras ocupa $(stat -c %s $T/B.PNG) octetos: no comprime"
muestra s1 mirador_guardar
fi

if [[ $SOLO == *E* ]]; then
echo "== Mirador E: las miniaturas de la carpeta =="
disco
mete BARRAS.BMP BARRAS.PCX CASA.PPM FLECHA.ICO FLECHA.PNG FOTON.JPG LAGO.GIF MONTE.TGA NOTA.BMP PAISAJE.PNG PLAYA.JPG
reabre 'A:\PAISAJE.PNG'
{ arranca
  tecla m; sleep 0.4; tecla right; sleep 0.8; shot k0             # mientras se llena, el teclado responde
  sleep 9; shot k1                                                # la rejilla, llena
  tecla left left; shot k2; tecla up; shot k3                     # las flechas
  tecla end; shot k4; tecla home; shot k5
  clic 112 260; aparca; shot k6                                   # un clic elige (LAGO.GIF)
  tecla esc; sleep 1; shot k7                                     # Esc: a la imagen que se veía
  clic 429 90; sleep 1.5; aparca; tecla home right right; sleep 0.5; tecla ret; sleep 2.5; shot k8   # el botón; Intro abre la elegida (CASA.PPM)
  tecla m; sleep 1.5; ir 232 260; mon "mouse_button 1" 0.1; mon "mouse_button 0" 0.1; mon "mouse_button 1" 0.1; mon "mouse_button 0" 0.3
  sleep 2.5; aparca; shot k9                                      # doble clic: abre MONTE.TGA
  sal; } | run 200 -vga std
pie k0 "PLAYA.JPG    11 de 11    Miniaturas: Intro abre la elegida; Esc vuelve"
ve $T/k1.ppm 64 128 96 48 $I/ref_barras.png "" 8 "miniatura de BARRAS.BMP"
ve $T/k1.ppm 288 122 96 60 $I/ref_casa.png "" 10 "miniatura de CASA.PPM"
ve $T/k1.ppm 624 116 96 72 $I/ref_foton.png "" 8 "miniatura de FOTON.JPG (2560x1920)"
ve $T/k1.ppm 64 226 96 60 $I/ref_lago.png "" 10 "miniatura de LAGO.GIF, en la segunda fila"
ve $T/k1.ppm 176 226 96 60 $I/ref_paisaje.png "" 10 "miniatura de MONTE.TGA"
ve $T/k1.ppm 400 226 96 60 $I/ref_paisaje.png "" 10 "miniatura de PAISAJE.PNG"
ve $T/k1.ppm 512 226 96 60 $I/ref_playa.png "" 10 "miniatura de PLAYA.JPG"
chk $T/k1.ppm busca 76 294 84 302 0 8 "LAGO.GIF"
chk $T/k1.ppm busca 60 190 80 200 0 8 "BARRAS.BMP"
chk $T/k1.ppm busca 320 240 340 256 13 8 "?"                     # NOTA.BMP no es una imagen
chk $T/k1.ppm px 506 214 2 "la elegida (PLAYA.JPG), sobre petróleo"
chk $T/k1.ppm busca 520 294 536 302 8 2 "PLAYA.JPG"
pie k2 "NOTA.BMP    9 de 11    Miniaturas: Intro abre la elegida; Esc vuelve"
pie k3 "CASA.PPM    3 de 11    Miniaturas: Intro abre la elegida; Esc vuelve"
pie k4 "PLAYA.JPG    11 de 11    Miniaturas: Intro abre la elegida; Esc vuelve"
pie k5 "BARRAS.BMP    1 de 11    Miniaturas: Intro abre la elegida; Esc vuelve"
pie k6 "LAGO.GIF    7 de 11    Miniaturas: Intro abre la elegida; Esc vuelve"
chk $T/k6.ppm px 58 214 2 "la del clic, sobre petróleo"
chk $T/k7.ppm titulo 40 34 activa "Mirador - PAISAJE.PNG"
pie k7 "PAISAJE.PNG    320 x 200    100 %    10 de 11"
ve $T/k7.ppm 232 222 320 200 $I/ref_paisaje.png "" 8 "Esc: otra vez la imagen que se veía"
chk $T/k8.ppm titulo 40 34 activa "Mirador - CASA.PPM"
ve $T/k8.ppm 312 272 160 100 $I/ref_casa.png "" 8 "Intro: abre la elegida"
chk $T/k9.ppm titulo 40 34 activa "Mirador - MONTE.TGA"
ve $T/k9.ppm 232 222 320 200 $I/ref_paisaje.png "" 8 "doble clic: abre esa"
muestra k1 mirador_miniaturas
fi

if [[ $SOLO == *F* ]]; then
echo "== Mirador F: los retoques, deshacer y guardar lo retocado =="
retoque() { $PY tests/tools/mirador_imagenes.py retoque "$@" || fail=1; }
exacta() { $PY tests/tools/mirador_imagenes.py exacta "$@" || fail=1; }
disco
mete PAISAJE.PNG TORRE.PNG
reabre 'A:\PAISAJE.PNG'
{ arranca
  clic 266 69; sleep 0.8; shot n1; clic 300 255; sleep 2; aparca; shot r1          # Imagen > A grises
  tecla ctrl-z; sleep 1.2; shot r2                                # deshacer: en color
  tecla ctrl-z; sleep 1.2; shot r3                                # y otra vez: rehace (gris)
  clic 140 69; sleep 0.8; shot n2; clic 200 91; sleep 1.2; aparca; shot r4         # Edición > Deshacer
  clic 266 69; sleep 0.8; clic 300 275; sleep 2; aparca; shot r5  # Imagen > Invertir los colores
  tecla i; sleep 1; shot r6; tecla ret; sleep 0.6                 # la información lo dice
  clic 211 69; sleep 0.8; clic 260 395; sleep 2; aparca; shot r7  # Ver > tramado ordenado: sigue invertida (no se lee del disco)
  clic 211 69; sleep 0.8; clic 260 375; sleep 2; aparca; shot r8  # y con difusión otra vez
  tecla f11; sleep 2; tecla pgdn; sleep 2; shot q1                # pasar a otra, a pantalla completa: vuelve a la ventana y pregunta
  tecla esc; sleep 1; shot q2                                     # Cancelar: se queda
  clic 747 47; sleep 1.2; shot q3; tecla esc; sleep 1             # cerrar la ventana: pregunta también
  tecla pgdn; sleep 1.2; clic 400 336; sleep 2.5; aparca; shot q4 # No: pasa a TORRE.PNG sin guardar
  tecla pgup; sleep 2.5; shot q5                                  # PAISAJE.PNG, como está en el disco
  clic 266 69; sleep 0.8; clic 300 295; sleep 1.2; aparca; shot d1        # Imagen > Brillo y contraste
  escribe "300"; tecla ret; sleep 1; shot d0; tecla ret; sleep 1  # (un valor que no vale: lo dice y vuelve a preguntar)
  escribe "30"; tecla tab; escribe "-40"; tecla ret; sleep 2; shot d2
  clic 266 69; sleep 0.8; clic 300 227; sleep 1.2; aparca; shot d3        # Imagen > Cambiar de tamaño
  tecla tab; escribe "50"; tecla ret; sleep 2; shot d4            # al 50 %
  clic 83 69; sleep 0.8; clic 150 159; sleep 1.2; escribe "bc"; tecla ret; sleep 3; shot d5; tecla ret; sleep 3; shot d6   # Guardar como PNG: BC.PNG pasa a ser la que se ve
  clic 266 69; sleep 0.8; clic 300 227; sleep 1.2; clic 265 289; tecla tab; escribe "100"; tecla tab; escribe "100"; aparca; shot d7
  tecla ret; sleep 2; shot d8                                     # a 100 x 100 conservando la proporción: 100 x 63
  clic 266 69; sleep 0.8; clic 300 227; sleep 1.2; clic 265 289; tecla tab; escribe "40"; tecla tab; escribe "90"
  clic 285 349; tecla ret; sleep 2; shot d9                       # a 40 x 90, sin conservarla
  tecla ctrl-g; sleep 1.2; escribe "medidas"; tecla ret; sleep 3; tecla ret; sleep 3; shot d10   # MEDIDAS.BMP
  tecla ctrl-a; sleep 1.2; escribe "paisaje.png"; tecla ret; sleep 2.5
  tecla e; sleep 0.6; shot e1                                     # el modo «elegir»
  ir 300 280; mon "mouse_button 1" 0.3; ir 420 360; mon "mouse_button 0" 0.4; aparca; shot e2   # una zona de 120 x 80
  ir 418 320; mon "mouse_button 1" 0.3; ir 460 320; mon "mouse_button 0" 0.4                    # el borde derecho, 40 más allá
  ir 350 320; mon "mouse_button 1" 0.3; ir 330 310; mon "mouse_button 0" 0.4; aparca; shot e3   # y el marco entero, 20 a la izquierda y 10 arriba
  tecla ret; sleep 1.5; shot e4                                   # Intro: recortada
  clic 83 69; sleep 0.8; clic 150 179; sleep 1.2; escribe "trozo"; tecla ret; sleep 3; tecla ret; sleep 3; shot e5  # TROZO.PCX
  tecla e; sleep 0.6; ir 380 300; mon "mouse_button 1" 0.3; ir 420 330; mon "mouse_button 0" 0.4; aparca
  tecla esc; sleep 0.8; shot e6                                   # Esc quita lo elegido
  tecla ctrl-e; sleep 0.8; shot e7; tecla ret; sleep 1            # Ctrl+E: toda la imagen (recortar a toda: no cambia nada)
  clic 266 69; sleep 0.8; clic 300 207; sleep 1.2; shot e8; tecla ret; sleep 0.8   # Recortar sin elegir: dice cómo
  clic 266 69; sleep 0.8; clic 300 255; sleep 2                   # a grises...
  tecla pgdn; sleep 1.2; clic 310 336; sleep 1.5; shot g1         # ...y a otra: Sí, se guarda (propone TROZO.PCX)
  escribe "gris"; tecla ret; sleep 3; shot g2; tecla ret; sleep 3; aparca; shot g3    # GRIS.PCX, y pasa a la siguiente
  sal; } | run 330 -vga std
chk $T/n1.ppm busca 264 196 284 212 0 8 "ortar a lo elegido"
pie r1 "PAISAJE.PNG (retocada)    320 x 200    100 %    1 de 2"
retoque $I/ref_paisaje.png $T/ref_gris.png gris
ve $T/r1.ppm 232 222 320 200 $T/ref_gris.png "" 8 "a grises: se ve el paisaje en grises"
pie r2 "PAISAJE.PNG    320 x 200    100 %    1 de 2"
ve $T/r2.ppm 232 222 320 200 $I/ref_paisaje.png "" 8 "Ctrl+Z: otra vez en color, y ya no está retocada"
pie r3 "PAISAJE.PNG (retocada)    320 x 200    100 %    1 de 2"
ve $T/r3.ppm 232 222 320 200 $T/ref_gris.png "" 8 "Ctrl+Z otra vez: rehace el retoque"
chk $T/n2.ppm busca 160 78 176 98 0 8 "acer el "
ve $T/r4.ppm 232 222 320 200 $I/ref_paisaje.png "" 8 "Edición > Deshacer: en color"
retoque $I/ref_paisaje.png $T/ref_inv.png inv
ve $T/r5.ppm 232 222 320 200 $T/ref_inv.png "" 8 "invertir los colores: el negativo"
dice r6 "Retocada: los cambios a"
ve $T/r7.ppm 232 222 320 200 $T/ref_inv.png "" 8 "retocada, al cambiar el tramado sigue invertida"
chk $T/r7.ppm cambia 232 222 320 200 $T/r5.ppm 8000
chk $T/r8.ppm igual 232 222 320 200 $T/r5.ppm 232 222 0     # y con difusión, como antes
chk $T/q1.ppm titulo 40 34 no "Mirador - PAISAJE.PNG"       # (en la ventana, detrás de la pregunta)
dice q1 "La imagen está retocada y no se ha guardado."
pie q2 "PAISAJE.PNG (retocada)    320 x 200    100 %    1 de 2"
dice q3 "¿Quiere guardarla?"
chk $T/q4.ppm titulo 40 34 activa "Mirador - TORRE.PNG"
pie q4 "TORRE.PNG    320 x 200    100 %    2 de 2"
ve $T/q5.ppm 232 222 320 200 $I/ref_paisaje.png "" 8 "no se guardó: PAISAJE.PNG sigue como estaba"
dice d1 "Brillo (de -100 a 100):"
dice d0 "El brillo y el contraste tienen que ser n"
retoque $I/ref_paisaje.png $T/ref_bc.png bc:30:-40
ve $T/d2.ppm 232 222 320 200 $T/ref_bc.png "" 8 "brillo 30 y contraste -40"
dice d3 "Ahora mide 320 x 200 puntos."
pie d4 "PAISAJE.PNG (retocada)    160 x 100    100 %    1 de 2"
dice d5 "La imagen se ha guardado."
chk $T/d6.ppm titulo 40 34 activa "Mirador - BC.PNG"
pie d6 "BC.PNG    160 x 100    100 %    1 de 3"
mcopy -n -i $T/f.img ::BC.PNG $T/BC.PNG 2>/dev/null || ko "no se ha guardado BC.PNG"
retoque $T/ref_bc.png $T/ref_bc2.png tam:160:100
exacta $T/BC.PNG $T/ref_bc2.png "brillo, contraste y la mitad de tamaño, guardado como PNG: punto por punto"
chk $T/d7.ppm busca 250 298 330 324 0 8 "Ancho:"
pie d8 "BC.PNG (retocada)    100 x 63    100 %    1 de 3"
pie d9 "BC.PNG (retocada)    40 x 90    100 %    1 de 3"
chk $T/d10.ppm titulo 40 34 activa "Mirador - MEDIDAS.BMP"
mcopy -n -i $T/f.img ::MEDIDAS.BMP $T/MEDIDAS.BMP 2>/dev/null || ko "no se ha guardado MEDIDAS.BMP"
retoque $T/ref_bc2.png $T/ref_med.png tam:100:63 tam:40:90
exacta $T/MEDIDAS.BMP $T/ref_med.png "a 100 x 63 (con proporción) y a 40 x 90 (sin ella), guardado como BMP: punto por punto"
pie e1 "PAISAJE.PNG    Elija la zona arrastrando el ratón sobre la imagen (Esc: dejarlo)"
pie e2 "PAISAJE.PNG    Elegido: 120 x 80    Intro recorta; Esc lo quita"
chk $T/e2.ppm px 350 280 0 "el marco de lo elegido: la raya negra de arriba"
chk $T/e2.ppm px 350 279 9 "y la blanca de fuera"
chk $T/e2.ppm px 419 320 9 "un tirador, en mitad del lado derecho"
chk $T/e2.ppm igual 232 364 320 56 $T/q5.ppm 232 364 0      # fuera del marco, la imagen como estaba
pie e3 "PAISAJE.PNG    Elegido: 160 x 80    Intro recorta; Esc lo quita"
chk $T/e3.ppm px 320 270 0 "el marco, movido y agrandado: su raya de arriba"
pie e4 "PAISAJE.PNG (retocada)    160 x 80    100 %    3 de 4"
chk $T/e5.ppm titulo 40 34 activa "Mirador - TROZO.PCX"
mcopy -n -i $T/f.img ::TROZO.PCX $T/TROZO.PCX 2>/dev/null || ko "no se ha guardado TROZO.PCX"
retoque $I/ref_paisaje.png $T/ref_trozo.png recorte:48:48:208:128
exacta $T/TROZO.PCX $T/ref_trozo.png "recortada a lo elegido y guardada como PCX: punto por punto"
pie e6 "TROZO.PCX    160 x 80    100 %    5 de 5"
pie e7 "TROZO.PCX    Elegido: 160 x 80    Intro recorta; Esc lo quita"
dice e8 "Primero elija la zona que quiere conservar:"
dice g1 "Nombre del archivo:"
dice g2 "La imagen se ha guardado."
chk $T/g3.ppm titulo 40 34 activa "Mirador - BC.PNG"
mcopy -n -i $T/f.img ::GRIS.PCX $T/GRIS.PCX 2>/dev/null || ko "no se ha guardado GRIS.PCX"
retoque $T/ref_trozo.png $T/ref_tgris.png gris
exacta $T/GRIS.PCX $T/ref_tgris.png "retocada, al pasar a otra se dice que sí: guardada (a grises), punto por punto"
mcopy -n -i $T/f.img ::PAISAJE.PNG $T/sal.png 2>/dev/null
cmp -s $T/sal.png $I/PAISAJE.PNG && ok "PAISAJE.PNG sigue como estaba en el disco" || ko "PAISAJE.PNG ha cambiado en el disco"
muestra r1 mirador_grises; muestra d1 mirador_brillo; muestra d3 mirador_tamano; muestra e3 mirador_elegir; muestra q1 mirador_pregunta
# sin el original en color verdadero (4 MB): se retoca lo que se ve, con paleta
disco
mete BARRAS.BMP PAISAJE.PNG
reabre 'A:\BARRAS.BMP'
{ arranca
  clic 266 69; sleep 0.8; clic 300 275; sleep 2; aparca; shot u1  # invertir
  tecla e; sleep 0.6; ir 272 262; mon "mouse_button 1" 0.3; ir 512 382; mon "mouse_button 0" 0.4; aparca
  tecla ret; sleep 1.5; shot u2                                   # recortar: 240 x 120
  clic 83 69; sleep 0.8; clic 150 159; sleep 1.2; escribe "b"; tecla ret; sleep 3; tecla ret; sleep 3    # B.PNG
  tecla end; sleep 2.5
  clic 266 69; sleep 0.8; clic 300 227; sleep 1.2; tecla tab; escribe "50"; tecla ret; sleep 2; shot u3  # PAISAJE al 50 %
  clic 266 69; sleep 0.8; clic 300 255; sleep 2; shot u4          # y a grises
  tecla ctrl-z; sleep 1.2; shot u5
  clic 83 69; sleep 0.8; clic 150 159; sleep 1.2; escribe "p"; tecla ret; sleep 3; tecla ret; sleep 3    # P.PNG
  sal; } | run 150 -vga std -m 4
chk $T/u1.ppm rgb 250 260 000000 "sin original: la barra blanca, invertida, negra exacta"
chk $T/u1.ppm rgb 290 260 0000FF "la amarilla, azul"
chk $T/u1.ppm rgb 450 260 00FFFF "la roja, turquesa"
pie u2 "BARRAS.BMP (retocada)    240 x 120    100 %    1 de 2"
mcopy -n -i $T/f.img ::B.PNG $T/B.PNG 2>/dev/null || ko "no se ha guardado B.PNG"
retoque $I/ref_barras.png $T/ref_b.png inv recorte:40:20:280:140
$PY tests/tools/mirador_imagenes.py guardada $T/B.PNG $T/ref_b.png P "" 0.5 "sin original: invertida y recortada, guardada con paleta" || fail=1
pie u3 "PAISAJE.PNG (retocada)    160 x 100    100 %    3 de 3"
retoque $I/ref_paisaje.png $T/ref_p.png tam:160:100
retoque $T/ref_p.png $T/ref_pg.png gris
ve $T/u4.ppm 312 272 160 100 $T/ref_pg.png "" 8 "sin original: reducida y a grises"
ve $T/u5.ppm 312 272 160 100 $T/ref_p.png "" 8 "sin original: deshacer devuelve el color"
mcopy -n -i $T/f.img ::P.PNG $T/P.PNG 2>/dev/null || ko "no se ha guardado P.PNG"
$PY tests/tools/mirador_imagenes.py guardada $T/P.PNG $T/ref_p.png P "" 8 "sin original: el paisaje a la mitad, guardado con paleta" || fail=1
fi

if [[ $SOLO == *G* ]]; then
echo "== Mirador G: los GIF animados =="
anima() { $PY tests/tools/mirador_imagenes.py anima "$@" || fail=1; }
disco
mete ANIMA.GIF PAISAJE.PNG UNAVEZ.GIF
reabre 'A:\\ANIMA.GIF'
{ arranca
  shot v0; shot v1; shot v2; shot v3; shot v4; shot v5; shot v6; shot v7; shot v8; shot v9    # (una captura cada 1,1 segundos)
  tecla spc; sleep 0.6; shot w1; sleep 3; shot w2                 # Espacio: en pausa
  tecla spc; shot w3; shot w5; shot w6                            # y sigue
  tecla i; sleep 1; shot w4; tecla ret; sleep 0.6                 # la información
  tecla f11; sleep 2; shot x1; sleep 1.5; shot x2; sleep 1.5; shot x3; tecla esc; sleep 2    # a pantalla completa
  tecla bracket_right; sleep 1.2; shot z1; sleep 1.5; shot z2; sleep 1.5; shot z3            # ampliada al 150 %
  tecla 1; sleep 1
  tecla f5; shot p1; shot p2; shot p3                             # en el pase (cada 5 segundos) también se mueve
  tecla esc; sleep 1
  tecla home; sleep 2.5; tecla r; sleep 1.5; shot g1; sleep 2.6; shot g2                     # girada: se queda en ese cuadro
  tecla end; sleep 5.5; shot u1                                   # UNAVEZ.GIF: tres cuadros de un segundo, una sola vez
  tecla spc; sleep 0.3; shot u2                                   # Espacio: otra vez desde el principio
  sal; } | run 200 -vga std
chk $T/v0.ppm titulo 40 34 activa "Mirador - ANIMA.GIF"
pie v0 "ANIMA.GIF    160 x 120    100 %    1 de 3    animación (Espacio: pausa)"
anima 312 262 4 6 "la animación pasa sola, en orden y a su paso (un cuadro cada dos segundos; diez capturas en once)" $T/v0.ppm $T/v1.ppm $T/v2.ppm $T/v3.ppm $T/v4.ppm $T/v5.ppm $T/v6.ppm $T/v7.ppm $T/v8.ppm $T/v9.ppm
chk $T/w1.ppm busca 46 551 600 559 0 6 "en pausa: cuadro "
chk $T/w2.ppm igual 312 262 160 120 $T/w1.ppm 312 262 0       # en pausa no se mueve
anima 312 262 1 2 "tras la pausa, sigue por donde iba" $T/w2.ppm $T/w3.ppm $T/w5.ppm $T/w6.ppm
dice w4 "Animación: 4 cuadros; una vuelta dura 8,00 segundos;"
dice w4 "se repite sin fin."
chk $T/x1.ppm px 10 10 0 "la animación, a pantalla completa"
anima 320 240 1 2 "a pantalla completa sigue pasando" $T/x1.ppm $T/x2.ppm $T/x3.ppm
chk $T/z1.ppm busca 46 551 300 559 0 6 "150 %"
chk $T/z3.ppm cambia 272 232 240 180 $T/z1.ppm 800
chk $T/z1.ppm rgb 280 240 333333 "ampliada: el fondo gris del primer cuadro, liso"
pie p1 "ANIMA.GIF    160 x 120    100 %    1 de 3    pase de diapositivas"
anima 312 262 1 2 "en el pase de diapositivas sigue pasando" $T/p1.ppm $T/p2.ppm $T/p3.ppm
pie g1 "ANIMA.GIF    120 x 160    100 %    1 de 3"
chk $T/g1.ppm nobusca 46 551 600 559 0 6 "animaci"
chk $T/g2.ppm igual 332 242 120 160 $T/g1.ppm 332 242 0       # girada, ya no se mueve
pie u1 "UNAVEZ.GIF    96 x 64    100 %    3 de 3    animación acabada (Espacio: otra vez)"
chk $T/u1.ppm rgb 392 322 0000FF "la que se ve una vez se queda en el último cuadro (azul)"
chk $T/u2.ppm rgb 392 322 FF0000 "Espacio: otra vez desde el primero (rojo)"
muestra v2 mirador_anima; muestra w1 mirador_anima_pausa; muestra w4 mirador_anima_info
# la que no cabe en la memoria: el primer cuadro, y se dice
disco
mete GORDA.GIF
reabre 'A:\\GORDA.GIF'
{ arranca
  sleep 4; shot y1; sleep 1.5; shot y2
  tecla i; sleep 1; shot y3; tecla ret; sleep 0.6
  sal; } | run 100 -vga std -m 4
avisa y1 "Aviso: animación de 40 cuadros; no hay memoria para todos y se ve solo el primero."
chk $T/y1.ppm rgb 392 330 FF0000 "la animación que no cabe en 4 MB: su primer cuadro (rojo)"
chk $T/y2.ppm rgb 392 330 FF0000 "y ahí se queda"
dice y3 "No cabe en la memoria: se ve el primer cuadro."
muestra y1 mirador_anima_no_cabe
fi

[ -n "${MIRADOR_DEJA:-}" ] && echo "(las capturas quedan en $T)" || rm -rf $T
[ $fail -eq 0 ] && echo "MIRADOR: TODO OK" || echo "MIRADOR: HAY FALLOS"
exit $fail
