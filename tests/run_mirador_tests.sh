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
#     información; el menú Ver (tramado, más cercano, lo transparente);
#  D. guardar como BMP (24 bits y 256 colores), copiar el nombre e imprimir
#     (LPT1 a un archivo), derecha y apaisada: lo guardado y lo impreso se
#     comparan con la imagen.
# Con MIRADOR_SOLO=ABCD (las letras que se quieran) se pasan solo esas partes
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
SOLO=${MIRADOR_SOLO:-NABCD}
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
  tecla f5; sleep 1.5; shot e1                                    # el pase (cada 5 segundos): ANCHA.JPG a pantalla completa
  sleep 3.5; shot e2                                              # a los cinco segundos, la siguiente
  tecla spc; sleep 6.5; shot e3                                   # en pausa: no pasa
  tecla spc; sleep 6.5; shot e4                                   # sigue: a los cinco segundos, la tercera
  tecla esc; sleep 2; shot e5; sleep 6; shot e6                   # fuera: en la ventana, y ya no pasa sola
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
mete LAGO.GIF NOTA.BMP RARO.BMP CORTADA.PNG ENORME.PNG FOTON.JPG FLECHA.PNG PAISAJE.PNG
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
  sal; } | run 280 -vga std
chk $T/f1.ppm titulo 40 34 activa "Mirador"
pie f1 "No hay ninguna imagen abierta."
chk $T/f1.ppm busca 150 250 500 400 1 8 "Archivo > Abrir (Ctrl+A) para ver una imagen"
dice f2 "Primero abra una imagen: Archivo > Abrir (Ctrl+A)."
chk $T/f3.ppm titulo 40 34 activa "Mirador - LAGO.GIF"
ve $T/f3.ppm 232 222 320 200 $I/ref_lago.png "" 8 "abierta con el diálogo"
chk $T/f4.ppm titulo 40 34 activa "Mirador - NOTA.BMP"
nove f4 "Este archivo no es una imagen de las que Mirador conoce:"
pie f4 "NOTA.BMP    6 de 8"
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
pie g1 "ENORME.PNG    3000 x 2000    21 %    2 de 8    reducida a 1/2"
ve $T/g1.ppm 71 108 641 427 $I/ref_enorme.png "" 6 "un PNG de 3000x2000: reducido al leerlo, entero en la ventana"
dice g2 "Medidas: 3.000 x 2.000 puntos"
dice g2 "Colores: 5 colores, con paleta"
dice g2 "En memoria: 1.500 x 1.000 puntos a 256 colores (reducida a 1/2)"
pie g3 "ENORME.PNG    3000 x 2000    50 %    2 de 8    reducida a 1/2"
chk $T/g3.ppm rgb 250 200 FF0000 "el mapa reducido, punto por punto: el centro, con el cuadrante rojo"
chk $T/g3.ppm rgb 550 200 00FF00 "el verde"
chk $T/g3.ppm rgb 250 450 0000FF "el azul"
chk $T/g3.ppm rgb 550 450 FFFF00 "y el amarillo"
pie g4 "FOTON.JPG    2560 x 1920    22 %    4 de 8    reducida a 1/2"
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
muestra f5 mirador_no_sabe; muestra f7 mirador_cortada; muestra g1 mirador_enorme; muestra g5 mirador_info
muestra h1 mirador_ordenado; muestra h2 mirador_cercano; muestra h3 mirador_difusion
fi

if [[ $SOLO == *D* ]]; then
echo "== Mirador D: guardar como BMP, copiar el nombre e imprimir =="
disco
mete PAISAJE.PNG TORRE.PNG
reabre 'A:\PAISAJE.PNG'
{ arranca
  tecla ctrl-g; sleep 1.2; tecla end; shot s1; tecla ret; sleep 3; shot s2; tecla ret; sleep 1     # el nombre que propone: PAISAJE.BMP (24 bits)
  clic 83 69; sleep 0.8; clic 150 139; sleep 1.2; escribe "ocho"; tecla ret; sleep 3; shot s3; tecla ret; sleep 1   # de 256 colores: OCHO.BMP
  tecla r; sleep 1.5; tecla ctrl-g; sleep 1.2; escribe "girada.bmp"; tecla ret; sleep 3; tecla ret; sleep 1          # girada: se guarda girada
  tecla l; sleep 1.5; shot s4
  tecla ctrl-c; sleep 0.6; tecla ctrl-a; sleep 1.2; tecla ctrl-v; sleep 0.8; shot s5; tecla esc; sleep 0.8          # el nombre, copiado: se pega en el diálogo
  tecla ctrl-p; sleep 5; shot p0; sleep 26; shot p1; tecla ret; sleep 1   # imprimir: apaisada (es más ancha que alta)
  sal; } | PARALELO=file:$T/prn1.bin run 240 -vga std
# (detrás de la última letra está el cursor: no se busca)
chk $T/s1.ppm busca 150 150 600 450 0 9 "PAISAJE.BM"
dice s2 "La imagen se ha guardado."
dice s3 "La imagen se ha guardado con los 256 colores de Hispa Ventanas."
pie s4 "PAISAJE.PNG    320 x 200    100 %    4 de 5"            # (con los tres BMP guardados)
chk $T/s5.ppm busca 150 150 600 450 0 9 "A:\\PAISAJE.PN"
pie p0 "Imprimiendo la imagen..."
dice p1 "La imagen se ha enviado a la impresora."
for f in PAISAJE.BMP OCHO.BMP GIRADA.BMP; do mcopy -n -i $T/f.img ::$f $T/$f 2>/dev/null || ko "no se ha guardado $f"; done
$PY tests/tools/mirador_imagenes.py bmp $T/PAISAJE.BMP $I/ref_paisaje.png 24 "" 0.01 "guardada en 24 bits: idéntica al original" || fail=1
$PY tests/tools/mirador_imagenes.py bmp $T/OCHO.BMP $I/ref_paisaje.png 8 "" 8 "guardada con 256 colores: lo que se ve" || fail=1
$PY tests/tools/mirador_imagenes.py bmp $T/GIRADA.BMP $I/ref_paisaje.png 24 der 0.01 "guardada girada" || fail=1
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
muestra s1 mirador_guardar
fi

[ -n "${MIRADOR_DEJA:-}" ] && echo "(las capturas quedan en $T)" || rm -rf $T
[ $fail -eq 0 ] && echo "MIRADOR: TODO OK" || echo "MIRADOR: HAY FALLOS"
exit $fail
