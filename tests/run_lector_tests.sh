#!/bin/bash
# Prueba de Lector (LECTOR.HVP), el lector de documentos PDF, del grupo
# «Productividad».
#
# Primero, fuera de Hispa Ventanas (deprisa, en Linux, con el mismo código):
#  - el descompresor (programas/comun/desinfla.inc) contra el «zlib» de
#    Python: tests/tools/lector_desinfla.py;
#  - el núcleo (leer el PDF y componer cada página) contra los PDF que hace
#    tests/tools/hazpdf.py, que sabe lo que ha metido en cada uno; PDF
#    estropeados al azar (que no se cuelgue) y, si la máquina tiene con qué,
#    PDF «de verdad» hechos por Chrome, Ghostscript, LibreOffice y qpdf,
#    contra el texto que saca «pdftotext»: tests/tools/lector_pdf.py.
# Después abre Hispa Ventanas con Lector (clave «Reabrir=» de CENTRO.INI) y:
#  A. abre una factura dada como parámetro: el pie, el aviso de las
#     imágenes, colores, rayas y letras en su sitio; ampliar, reducir, tamaño
#     real, página entera y ancho de página; página siguiente y anterior con
#     los botones, AvPág y RePág al final de la página, Ctrl+Inicio y
#     Ctrl+Fin, «Ir a»; Información y Acerca de;
#  B. arranca vacío, abre con el diálogo un libro de 40 páginas (PDF 1.5) y
#     busca una palabra: la encuentra en otra página, la marca, sigue con F3
#     y da la vuelta; una que no está; la rueda del ratón;
#  C. guarda el texto de una página y de un documento en .TXT y lo imprime
#     («Imprimir solo el texto», LPT1 a un archivo): se compara letra a letra;
#  D. lo que no sabe: un PDF cifrado, uno que no es PDF, uno que no existe,
#     páginas con filtro desconocido, letra ilegible, letras que no hay y
#     contenido dañado (cada una con su aviso) y un PDF cortado, que se rehace;
#  E. los PDF de verdad, en la pantalla;
#  F. curvas, figuras rellenas, texto girado, páginas giradas, formularios,
#     codificaciones, los otros filtros, PDF 1.5 y una actualización incremental.
#  G. imágenes (RGB, grises, índices, máscaras, giradas), la que no se sabe y recortes.
#  H. las JPEG: las de bloques lisos de hazpdf.py (grises, color, CMYK de
#     Adobe), la transparencia de una imagen y las que no se saben, cada una
#     con su nombre en el aviso; fotos de verdad (tests/tools/lector_fotos.py,
#     con ImageMagick: 4:2:0, 4:2:2, gris, CMYK, progresiva, grande), que se
#     comparan en la pantalla con la foto; y una página A4 escaneada a 150
#     puntos por pulgada, en gris y en color, con lo que tarda en componerse.
#  I. imprimir como se ve (ESC/P de agujas por LPT1, a un archivo): una
#     etiqueta con un código de barras (Ctrl+P) y el documento entero, con
#     una hoja A4 que se reduce; tests/tools/lector_impresora.py lo
#     descodifica y mira las barras punto por punto.
#  K. el menú del botón secundario; y sin impresora, el aviso.
# Con LECTOR_SOLO=ABCDEFGHIK (las letras que se quieran) se pasan solo esas partes
# de Hispa Ventanas; con LECTOR_SOLO=N, solo lo de fuera.
#
# Necesita qemu-system-i386, mtools, socat e ImageMagick (como las demás).
set -u
IMG=${1:-build/hispados.img}
. tests/tools/hvp_arnes.sh
HVP=$B/LECTOR.HVP
SOLO=${LECTOR_SOLO:-NABCDEFGHIK}
NASM=${NASM:-nasm} python3 tools/mkhvp.py programas/lector.asm $HVP > $T/mkhvp.txt 2>&1 || { cat $T/mkhvp.txt; ko "LECTOR.HVP no se ensambla"; exit 1; }
P=$T/pdf
python3 tests/tools/hazpdf.py $P > /dev/null || { ko "hazpdf.py no hace los PDF"; exit 1; }
R=$T/real
echo "== Lector: PDF de verdad, con lo que hay en la máquina =="
bash tests/tools/lector_deverdad.sh $R $P
reabre() { printf '[Centro]\r\nReabrir=A:\\HISPADOS\\PROGS\\LECTOR.HVP %s\r\n' "$1" > $T/CENTRO.INI
           mcopy -o -i $T/f.img $T/CENTRO.INI ::HISPADOS/CENTRO.INI; }
mete() { local f; for f in "$@"; do mcopy -o -i $T/f.img $f :: || ko "no cabe $f en el disquete de la prueba"; done; }
# (0.9.31) el disquete de la prueba se quedaba sin sitio para los PDF grandes
# (FILTROS, FOTOS, los escaneos): fuera lo que el Lector no usa (los demás
# módulos y las órdenes que no se teclean aquí), como hace Presentaciones
sitio() { local f; for f in PROGS/PINTURA.HVP PROGS/ARCHIVOS.HVP PROGS/MEZCLAD.HVP PROGS/SONIDOS.HVP PROGS/TERMINAL.HVP \
    PROGS/CUENTAS.HVP PROGS/TECLADO.HVP HSPSND.COM HSPEMM.SYS PARTIR.COM HSPRED.COM DESCARGA.COM CONSOLA.COM REVISAR.COM \
    VELOCIDA.COM HSPJOY.COM CONFRED.COM PING.COM INSTALAR.COM PROTECTO.COM HSPCD.SYS XCOPIAR.COM; do
    mdel -i $T/f.img ::HISPADOS/$f 2>/dev/null; done; }
puntos() { python3 tests/tools/lector_puntos.py "$@" || fail=1; }
pie() { chk $T/$1.ppm busca 46 551 56 559 0 6 "$2"; }           # el pie de la ventana
avisa() { chk $T/$1.ppm busca 46 106 56 114 13 8 "$2"; }         # la franja del aviso, en granate
dice() { chk $T/$1.ppm busca 120 180 680 420 0 8 "$2"; }         # un mensaje, en su diálogo
rueda() { mon "mouse_move 0 0 $1" 0.4; }                           # (-1: hacia abajo)
# como «arranca», con el ratón por la vía directa (IntelliMouse: con rueda)
arranca_rueda() { sleep 6; send 'KEYB SP' 2; send 'A:\HISPADOS\HSPRATON /DIRECTO' 3; send 'VENTANAS' 6; aparca; }

if [[ $SOLO == *N* ]]; then
echo "== Lector: el descompresor, contra zlib =="
python3 tests/tools/lector_desinfla.py ${LECTOR_AZAR:-1500} || fail=1
echo "== Lector: el núcleo, contra los PDF de hazpdf.py, estropeados y de verdad =="
python3 tests/tools/lector_pdf.py --azar ${LECTOR_ROTOS:-1500} --de-verdad $R || fail=1
echo "== Lector: el núcleo, con fotos y escaneos JPEG de verdad (ImageMagick) =="
if python3 tests/tools/lector_fotos.py haz $T/fotosn > $T/fotosn.txt 2>&1; then
  python3 tests/tools/lector_fotos.py comprueba $T/fotosn || fail=1
else
  echo "  (ImageMagick no hace las fotos: esta parte no se pasa)"; cat $T/fotosn.txt
fi
fi

if [[ $SOLO == *A* ]]; then
echo "== Lector A: una factura; ampliar y reducir; pasar de página =="
prepara $HVP; sitio
mete $P/FACTURA.PDF $P/FUENTES.PDF
reabre 'A:\FACTURA.PDF'
{ arranca
  shot a1
  clic 211 69; sleep 0.8; clic 260 239; aparca; shot a2           # Ver > Tamaño real
  clic 750 528; aparca; shot a2v; clic 734 544; aparca; shot a2h; tecla home left   # las flechas de las barras: abajo y a la derecha
  ir 400 300; mon "mouse_button 1" 0.3; ir 380 260; mon "mouse_button 0" 0.3; aparca; shot a2d; tecla home left   # se arrastra la hoja con el ratón
  tecla slash; shot a3; tecla bracket_right bracket_right; shot a4   # - y dos veces +
  clic 211 69; sleep 0.8; shot m2; clic 260 279; aparca; shot a5  # Ver > Página entera
  clic 211 69; sleep 0.8; clic 260 259; aparca                    # Ver > Ancho de página
  clic 153 90; aparca; shot a6                                    # botón: página siguiente
  ir 125 90; shot a7; clic 125 90; aparca; shot a8                # el nombre del botón, en el pie; y página anterior
  tecla end; shot a9; tecla home; shot a10
  tecla ctrl-a; sleep 1.2; escribe "fuentes.pdf"; tecla ret; sleep 2.5; shot b1
  tecla pgdn; shot b2; tecla pgdn; shot b3; tecla pgdn; sleep 0.5; shot b4      # dos pantallas y, al final, la página 2
  tecla pgup; sleep 0.5; shot b5                                  # arriba del todo: al final de la 1
  tecla ctrl-end; sleep 0.5; shot b6; tecla ctrl-home; sleep 0.5; shot b7
  tecla f5; sleep 1; shot b8; escribe "2"; tecla ret; sleep 1; shot b9
  tecla down down down; shot b10; tecla right; shot b11
  clic 83 69; sleep 0.8; shot m1; clic 150 187; sleep 1.2; shot i1; tecla ret; sleep 0.8    # Archivo > Información
  clic 243 69; sleep 0.8; clic 290 91; sleep 1.2; shot i2; tecla ret; sleep 0.8             # ? > Acerca de
  clic 83 69; sleep 0.8; clic 150 111; sleep 1; aparca; shot c1   # Archivo > Cerrar
  tecla ctrl-b; sleep 1; shot c2; tecla ret; sleep 0.5            # sin documento: lo dice
  sal; } | run 280 -vga std
chk $T/a1.ppm titulo 40 34 activa "Lector - A:\\FACTURA.PDF"
pie a1 "Página 1 de 2    86 %    A:\\FACTURA.PDF"
avisa a1 "Hay imágenes que Lector no sabe mostrar (JPEG 2000): van como recuadros grises."
# a lo ancho de la página (x = 51 + 1,1457 puntos; y = 136 + 1,1457 puntos desde arriba)
chk $T/a1.ppm rgb 300 280 CCFFFF "la banda del título (0,8 0,95 1)"
chk $T/a1.ppm rgb 60 280 FFFFFF "la hoja, blanca"
chk $T/a1.ppm px 46 300 8 "fuera de la hoja, crema"
chk $T/a1.ppm rgb 140 510 009900 "el rectángulo verde"
chk $T/a1.ppm rgb 108 510 0000FF "su borde azul"
chk $T/a1.ppm rgb 220 510 BBBBBB "el gris de CMYK"
chk $T/a1.ppm rgb 300 510 848484 "el gris medio"
chk $T/a1.ppm px 560 515 15 "el recuadro de la imagen"
puntos $T/a1.ppm 300 315 20 20 000000 20 20 "raya de arriba de la tabla: una fila de 20 puntos negros"
puntos $T/a1.ppm 500 350 20 10 000000 10 10 "raya vertical de la tabla"
puntos $T/a1.ppm 115 270 250 30 tinta 500 2500 "las letras de «FACTURA 144-2026»"
puntos $T/a1.ppm 115 350 150 16 tinta 150 900 "las letras de «Cuota anual 2026»"
puntos $T/a1.ppm 580 440 100 30 FF0000 150 900 "«Total: 286,90» y su subrayado, en rojo"
pie a2 "Página 1 de 2    100 %    A:\\FACTURA.PDF"
chk $T/a2.ppm rgb 500 305 CCFFFF "al 100 %: la banda del título"
puntos $T/a2.ppm 300 345 20 15 000000 20 20 "al 100 %: la raya de arriba de la tabla"
puntos $T/a2.ppm 576 360 15 10 000000 10 10 "al 100 %: la raya vertical"
chk $T/a2v.ppm igual 60 140 600 364 $T/a2.ppm 60 172 0     # la barra vertical: 32 puntos hacia abajo
chk $T/a2h.ppm igual 60 140 600 364 $T/a2v.ppm 92 140 0    # la horizontal: 32 hacia la derecha
chk $T/a2d.ppm igual 60 140 600 300 $T/a2.ppm 80 180 0      # arrastrada 20 a la izquierda y 40 arriba
pie a3 "Página 1 de 2    75 %    A:\\FACTURA.PDF"
pie a4 "Página 1 de 2    125 %    A:\\FACTURA.PDF"
pie a5 "Página 1 de 2    35 %    A:\\FACTURA.PDF"
chk $T/a5.ppm px 60 300 8 "página entera: a la izquierda de la hoja, crema"
chk $T/a5.ppm rgb 392 300 FFFFFF "página entera: la hoja, en el centro"
chk $T/a5.ppm px 720 300 8 "página entera: a la derecha, crema"
pie a6 "Página 2 de 2    170 %    A:\\FACTURA.PDF"
chk $T/a6.ppm nobusca 46 106 56 114 13 8 "Hay imágenes que Lector no sabe mostrar"
puntos $T/a6.ppm 80 130 300 60 tinta 300 4000 "la segunda página: «Apaisada», grande"
pie a7 "Página anterior (RePág)"
pie a8 "Página 1 de 2    86 %    A:\\FACTURA.PDF"
chk $T/a9.ppm cambia 60 140 600 380 $T/a8.ppm 5000
chk $T/a10.ppm igual 60 140 600 380 $T/a8.ppm 60 140 0
chk $T/b1.ppm titulo 40 34 activa "Lector - fuentes.pdf"
pie b1 "Página 1 de 3    86 %    fuentes.pdf"
chk $T/b2.ppm cambia 60 120 600 380 $T/b1.ppm 2000
pie b3 "Página 1 de 3    86 %    fuentes.pdf"
pie b4 "Página 2 de 3    86 %    fuentes.pdf"
pie b5 "Página 1 de 3    86 %    fuentes.pdf"
chk $T/b5.ppm igual 60 120 600 380 $T/b3.ppm 60 120 0       # abajo del todo, como antes de pasar
pie b6 "Página 3 de 3    86 %    fuentes.pdf"
pie b7 "Página 1 de 3    86 %    fuentes.pdf"
chk $T/b8.ppm busca 200 180 500 330 0 8 "Número de página, de 1 a 3:"
pie b9 "Página 2 de 3    86 %    fuentes.pdf"
chk $T/b10.ppm cambia 60 120 600 380 $T/b9.ppm 300
chk $T/b11.ppm igual 60 120 600 380 $T/b10.ppm 60 120 0     # no hay nada a la derecha: no se mueve
dice i1 "Archivo: fuentes.pdf (3 KB)"
dice i1 "Título: (no lo dice)"
dice i1 "Páginas: 3"
dice i1 "Versión de PDF: 1.4"
dice i1 "Tamaño de esta página: 210 x 297 mm"
dice i2 "El lector de documentos PDF de Hispa Ventanas:"
dice i2 "Copyright (C) 2026 Carlos Sanz y contribuidores."
dice i2 "GPL-2.0 o posterior."
chk $T/c1.ppm titulo 40 34 activa "Lector"
pie c1 "No hay ningún documento abierto."
chk $T/c1.ppm busca 150 250 450 330 7 8 "Archivo > Abrir (Ctrl+A) para leer un documento PDF"
dice c2 "Primero abra un documento: Archivo > Abrir (Ctrl+A)."
muestra a1 lector_factura; muestra a2 lector_factura_100; muestra a5 lector_pagina_entera; muestra a6 lector_apaisada
muestra b1 lector_fuentes; muestra b9 lector_fuentes_2; muestra m1 lector_menu_archivo; muestra m2 lector_menu_ver
muestra i1 lector_informacion; muestra i2 lector_acerca; muestra c1 lector_vacio
fi

if [[ $SOLO == *B* ]]; then
echo "== Lector B: abrir con el diálogo; buscar por todo el documento; la rueda =="
prepara $HVP; sitio
mete $P/LARGO.PDF
{ arranca_rueda
  shot v0
  clic 61 90; sleep 1.2; shot v1; escribe "largo.pdf"; tecla ret; sleep 3; shot v2     # botón Abrir
  clic 253 90; sleep 1; shot s0; escribe "tesoro"; tecla ret; sleep 4; aparca; shot s1  # botón Buscar
  tecla f3; sleep 6; shot s2; tecla f3; sleep 3; shot s3; tecla f3; sleep 5; shot s4
  tecla esc; shot s5
  tecla ctrl-b; sleep 1; escribe "elefante"; tecla ret; sleep 9; shot s6; tecla ret; sleep 1; shot s7
  tecla ctrl-b; sleep 1; escribe "ACORDARME"; tecla ret; sleep 1.5; shot s8; tecla f3; sleep 1; shot s9
  tecla home; shot r0; ir 400 300; rueda -1; shot r1; for i in 2 3 4 5 6 7 8 9 10 11 12; do rueda -1; done; sleep 0.5; shot r2   # doce pasos: al final de la página
  rueda -1; sleep 0.8; shot r3; rueda 1; sleep 0.8; shot r4
  sal; } | run 280 -vga std
chk $T/v0.ppm titulo 40 34 activa "Lector"
pie v0 "No hay ningún documento abierto."
chk $T/v0.ppm busca 150 250 450 330 7 8 "Archivo > Abrir (Ctrl+A) para leer un documento PDF"
chk $T/v1.ppm titulo 222 150 activa "Abrir un PDF"
chk $T/v2.ppm titulo 40 34 activa "Lector - largo.pdf"
pie v2 "Página 1 de 40    86 %    largo.pdf"
puntos $T/v2.ppm 60 120 660 400 2 0 0 "sin buscar, nada marcado (ni un punto azul petróleo)"
chk $T/s0.ppm busca 200 180 500 330 0 8 "Texto que se busca:"
pie s1 "Página 4 de 40    86 %    largo.pdf"
puntos $T/s1.ppm 60 120 660 400 2 150 900 "«tesoro», marcado en la página 4"
pie s2 "Página 27 de 40    86 %    largo.pdf"
puntos $T/s2.ppm 60 120 660 400 2 150 900 "«tesoro», marcado en la página 27"
pie s3 "Página 31 de 40    86 %    largo.pdf"
pie s4 "Página 4 de 40    86 %    largo.pdf"
puntos $T/s5.ppm 60 120 660 400 2 0 0 "con Escape, deja de estar marcado"
dice s6 "No se encuentra ese texto en el documento."
pie s7 "Página 4 de 40    86 %    largo.pdf"
puntos $T/s8.ppm 60 120 660 400 2 150 1200 "«ACORDARME» (escrito en mayúsculas), marcado"
chk $T/s9.ppm cambia 60 120 660 400 $T/s8.ppm 100           # F3: la siguiente, una línea más abajo
pie s9 "Página 4 de 40    86 %    largo.pdf"
chk $T/r1.ppm cambia 60 120 600 380 $T/r0.ppm 2000          # la rueda desplaza
pie r2 "Página 4 de 40    86 %    largo.pdf"
pie r3 "Página 5 de 40    86 %    largo.pdf"               # y, al final de la página, pasa a la siguiente
pie r4 "Página 4 de 40    86 %    largo.pdf"
muestra v1 lector_abrir; muestra v2 lector_largo; muestra s1 lector_buscar; muestra s6 lector_no_esta
fi

if [[ $SOLO == *C* ]]; then
echo "== Lector C: guardar el texto en un .TXT; imprimirlo =="
prepara $HVP; sitio
mete $P/CARTA.PDF $P/FUENTES.PDF
reabre 'A:\CARTA.PDF'
rm -f $T/prn.txt
{ arranca
  shot g0
  tecla ctrl-c; sleep 0.8; tecla ctrl-b; sleep 1; tecla ctrl-v; sleep 0.8; shot g0c; tecla esc; sleep 0.8   # copiar la página y pegarla en el cuadro de buscar
  clic 83 69; sleep 0.8; clic 150 139; sleep 1.5; shot g1; tecla ret; sleep 2.5; shot g2; tecla ret; sleep 1   # Guardar el texto de la página
  clic 83 69; sleep 0.8; clic 150 255; sleep 5; shot g3; tecla ret; sleep 1            # Imprimir solo el texto de la página
  tecla ctrl-a; sleep 1.2; escribe "fuentes.pdf"; tecla ret; sleep 2.5
  clic 153 90; sleep 0.8                                                               # a la página 2
  clic 83 69; sleep 0.8; clic 150 159; sleep 1.5; escribe "todo"; tecla ret; sleep 4; shot g4; tecla ret; sleep 1   # Guardar el texto del documento
  aparca; shot g5
  clic 83 69; sleep 0.8; clic 150 275; sleep 8; shot g6; tecla ret; sleep 1            # Imprimir solo el texto del documento
  clic 83 69; sleep 0.8; clic 150 139; sleep 1.5; escribe "fuentes.pdf"; tecla ret; sleep 1.5; shot g7; tecla ret; sleep 1   # el nombre del PDF: no
  sal; } | PARALELO=file:$T/prn.txt run 280 -vga std
puntos $T/g0c.ppm 250 292 290 18 0 400 700 "lo copiado, pegado en el cuadro de buscar (se ve el final: 35 letras)"
chk $T/g1.ppm titulo 222 150 activa "Guardar el texto como"
puntos $T/g1.ppm 243 211 80 16 12 600 1100 "el nombre propuesto (CARTA.TXT: se comprueba al guardar), elegido en añil"
dice g2 "El texto se ha guardado."
dice g3 "El texto se ha enviado a la impresora."
dice g4 "El texto se ha guardado."
pie g5 "Página 2 de 3    86 %    fuentes.pdf"              # tras guardar todo, sigue en su página
dice g6 "El texto se ha enviado a la impresora."
dice g7 "Ese es el nombre del propio documento PDF:"
mcopy -n -i $T/f.img ::CARTA.TXT $T/carta.txt 2>/dev/null
mcopy -n -i $T/f.img ::TODO.TXT $T/todo.txt 2>/dev/null
mcopy -n -i $T/f.img ::FUENTES.PDF $T/fuentes.sal 2>/dev/null
cmp -s $T/fuentes.sal $P/FUENTES.PDF && ok "FUENTES.PDF sigue como estaba" || ko "FUENTES.PDF ha cambiado en el disco"
if python3 - $T/carta.txt $T/todo.txt $T/prn.txt <<'PY'
import sys
carta = ['Albinyana, 16 de noviembre de 2026', '', 'Querida señora Martínez:',
         '¿Cómo está usted? ¡Qué alegría saber de la asociación!',
         'El nº 3 de la calle, 2ª planta. Pingüino, cigüeña, Ñandú.',
         'Son 25 EUR (veinticinco) - «gracias»...', 'ARBOL, ÉXITO, INDICE, OSCAR, UTIL']
p1 = ['Helvetica 24', '', 'Helvetica negra 18', '', 'Helvetica inclinada 12', '',
      'Times 12   Times negra   Times cursiva 9', '', 'Courier 10: 0123456789', 'Courier negra', '', 'ABCCBA']
ok = True
def mira(nombre, cond):
    global ok
    if not cond:
        ok = False
        print('  FAIL ' + nombre)
try:
    c = open(sys.argv[1], 'rb').read().decode('cp437')
    mira('CARTA.TXT: el texto de la página, línea a línea', c == ''.join(l + '\r\n' for l in carta))
    t = open(sys.argv[2], 'rb').read().decode('cp437').split('\r\n')
    mira('TODO.TXT: empieza por la página 1', t[:1 + len(p1)] == ['--- Página 1 ---'] + p1)
    mira('TODO.TXT: lleva las tres páginas', [l for l in t if l.startswith('--- ')] == ['--- Página %d ---' % i for i in (1, 2, 3)])
    mira('TODO.TXT: la página 2 y la 3', 'Hola, mundo AB03 (x) \\ final' in t and 'AB CDEF GH' in t and t[-4:] == ['--- Página 3 ---', 'Tercera página', '', ''])
    d = open(sys.argv[3], 'rb').read().decode('cp437')
    hojas = d.split('\x0c')
    mira('impresora: cuatro hojas (la carta y las tres del otro documento)', len(hojas) == 5 and hojas[4] == '')
    mira('impresora: la carta, con su margen', hojas[0] == ''.join(('    ' + l if l else '') + '\r\n' for l in carta))
    mira('impresora: la primera del documento', hojas[1] == ''.join(('    ' + l if l else '') + '\r\n' for l in p1))
    mira('impresora: la tercera', hojas[3] == '    Tercera página\r\n')
except Exception as e:
    mira('no se han podido leer los archivos: %s' % e, False)
sys.exit(0 if ok else 1)
PY
then ok "CARTA.TXT, TODO.TXT y la impresora: el texto, letra a letra"; else ko "el texto guardado o impreso no es el esperado"; fi
muestra g1 lector_guardar; muestra g0 lector_carta; muestra g0c lector_copiar
fi

if [[ $SOLO == *D* ]]; then
echo "== Lector D: lo que no sabe, lo dice =="
prepara $HVP; sitio
mete $P/CIFRADO.PDF $P/NOSABE.PDF $P/NOPDF.PDF $P/CORTADO.PDF $P/SOLOCAB.PDF $P/BUCLE.PDF $P/CIFRA15.PDF
reabre 'A:\CIFRADO.PDF'
abre() { tecla ctrl-a; sleep 1.2; escribe "$1"; tecla ret; sleep ${2:-2.5}; }
{ arranca
  shot n1; tecla ret; sleep 0.8; aparca; shot n2
  abre "cifra15.pdf"; shot n3; tecla ret; sleep 0.8
  abre "nopdf.pdf"; shot n4; tecla ret; sleep 0.8
  abre "noexiste.pdf"; shot n5; tecla ret; sleep 0.8
  abre "solocab.pdf"; shot n6; tecla ret; sleep 0.8
  abre "nosabe.pdf"; shot p1
  clic 153 90; sleep 0.8; aparca; shot p2
  tecla ctrl-b; sleep 1; escribe "lee"; tecla ret; sleep 1.5; shot p2b; tecla esc
  clic 153 90; sleep 0.8; aparca; shot p3; clic 153 90; sleep 0.8; aparca; shot p4; clic 153 90; sleep 0.8; aparca; shot p5
  clic 83 69; sleep 0.8; clic 150 139; sleep 1.2; shot p6; tecla ret; sleep 0.8           # guardar el texto de una página sin texto
  abre "cortado.pdf" 3; shot q1; tecla ctrl-end; sleep 1; shot q2
  clic 83 69; sleep 0.8; clic 150 187; sleep 1.2; shot q3; tecla ret; sleep 0.8           # Información: «se ha rehecho»
  abre "bucle.pdf"; shot q4
  sal; } | run 280 -vga std
dice n1 "El documento está cifrado (protegido con contraseña"
dice n1 "o con permisos) y Lector no sabe abrir documentos cifrados."
chk $T/n2.ppm titulo 40 34 activa "Lector"
pie n2 "No hay ningún documento abierto."
dice n3 "El documento está cifrado (protegido con contraseña"
dice n4 "Este archivo no es un documento PDF."
dice n5 "No se ha podido abrir el archivo."
dice n6 "El documento está dañado y no se puede leer:"
pie p1 "Página 1 de 5    86 %    nosabe.pdf"
avisa p1 "Aviso: página con una compresión que Lector no conoce (JBIG2Decode): no se muestra."
puntos $T/p1.ppm 60 140 660 380 tinta 0 0 "la página que no se puede mostrar, en blanco (sin basura)"
pie p2 "Página 2 de 5    86 %    nosabe.pdf"
avisa p2 "Aviso: hay texto que no se puede leer (letra con codificación propia): va en gris."
puntos $T/p2.ppm 130 190 200 24 tinta 50 900 "lo que sí se lee, se ve: «Se lee»"
puntos $T/p2.ppm 130 216 300 64 tinta 0 0 "lo ilegible no sale como letras"
puntos $T/p2.ppm 130 216 300 64 15 300 3000 "sino como bandas grises"
puntos $T/p2b.ppm 60 140 660 380 2 100 900 "y se busca en lo que sí se lee"
pie p3 "Página 3 de 5    86 %    nosabe.pdf"
avisa p3 "Aviso: hay letras que Hispa-DOS no tiene; se ven como «?»."
pie p4 "Página 4 de 5    86 %    nosabe.pdf"
avisa p4 "Aviso: la página está dañada; puede faltar una parte."
puntos $T/p4.ppm 130 190 300 24 tinta 100 1500 "de la página dañada, lo que se salva: «Hasta aqui llega»"
pie p5 "Página 5 de 5    86 %    nosabe.pdf"
avisa p5 "Aviso: la página está dañada; puede faltar una parte."
dice p6 "Esta página no tiene texto que se pueda guardar, copiar o imprimir."
pie q1 "Página 1 de 3    86 %    cortado.pdf"
chk $T/q1.ppm nobusca 46 106 56 114 13 8 "Aviso: la página está dañada; puede faltar una parte."
puntos $T/q1.ppm 130 168 300 24 tinta 100 1500 "el PDF cortado: la primera página, entera"
pie q2 "Página 3 de 3    86 %    cortado.pdf"
avisa q2 "Aviso: la página está dañada; puede faltar una parte."
dice q3 "El índice del archivo estaba dañado y se ha rehecho."
pie q4 "Página 1 de 1    84 %    bucle.pdf"                 # (su caja no vale: la de una carta, 612 x 792)
muestra n1 lector_cifrado; muestra p1 lector_filtro; muestra p2 lector_ilegible; muestra p3 lector_letras; muestra p4 lector_danada
muestra q2 lector_cortado
fi

if [[ $SOLO == *E* ]]; then
echo "== Lector E: PDF de verdad (Chrome, LibreOffice, Ghostscript, qpdf) =="
if ls $R/*.PDF > /dev/null 2>&1; then
prepara $HVP; sitio
for f in CHROME LIBREOFF GS QPDF; do [ -f $R/$f.PDF ] && mete $R/$f.PDF; done
PRIMERO=$(cd $R; ls CHROME.PDF LIBREOFF.PDF GS.PDF QPDF.PDF 2>/dev/null | head -1)
reabre "A:\\$PRIMERO"
abre() { tecla ctrl-a; sleep 1.2; escribe "$1"; tecla ret; sleep ${2:-3}; }
{ arranca
  sleep 2; shot e1
  abre "chrome.pdf" 5; shot e2; tecla ctrl-b; sleep 1; escribe "autobus"; tecla ret; sleep 2; aparca; shot e2b; tecla esc
  clic 211 69; sleep 0.8; clic 260 239; aparca; sleep 1; shot e2c
  abre "libreoff.pdf" 4; shot e3
  abre "gs.pdf"; shot e4
  abre "qpdf.pdf"; shot e5
  sal; } | run 240 -vga std
if [ -f $R/CHROME.PDF ]; then
  pie e2 "Página 1 de 1    8"                               # (86 % en A4; 84 % en papel de carta)
  chk $T/e2.ppm nobusca 46 106 56 114 13 8 "Aviso"
  puntos $T/e2.ppm 60 120 660 400 tinta 4000 40000 "Chrome: la página tiene texto"
  puntos $T/e2b.ppm 60 120 660 400 2 100 2500 "Chrome: «autobus» (sin acento) encuentra «autobús» y lo marca"
  pie e2c "Página 1 de 1    100 %    chrome.pdf"
  muestra e2 lector_chrome; muestra e2c lector_chrome_100
fi
if [ -f $R/LIBREOFF.PDF ]; then
  pie e3 "Página 1 de 1    86 %    libreoff.pdf"
  puntos $T/e3.ppm 60 120 660 400 tinta 3000 40000 "LibreOffice: la página tiene texto"
  chk $T/e3.ppm rgb 400 220 CCCCFF "LibreOffice: el recuadro relleno (una figura, no un rectángulo)"
  muestra e3 lector_libreoffice
fi
if [ -f $R/GS.PDF ]; then
  pie e4 "Página 1 de 2    8"
  puntos $T/e4.ppm 60 120 660 400 tinta 1500 40000 "Ghostscript: la página tiene texto"
  muestra e4 lector_ghostscript
fi
if [ -f $R/QPDF.PDF ]; then
  pie e5 "Página 1 de 2    86 %    qpdf.pdf"
  chk $T/e5.ppm rgb 300 280 CCFFFF "qpdf: la factura con flujos de objetos, igual que la original"
  muestra e5 lector_qpdf
fi
else
  echo "  (no hay ningún programa con que hacer PDF de verdad: esta parte no se pasa)"
fi
fi

if [[ $SOLO == *F* ]]; then
echo "== Lector F: gráficos, texto girado, formularios, codificaciones, filtros, PDF 1.5 =="
prepara $HVP; sitio
mete $P/GRAFICOS.PDF $P/CODIF.PDF $P/FILTROS.PDF $P/MODERNO.PDF $P/INCREMEN.PDF $P/ESPACIOS.PDF
reabre 'A:\GRAFICOS.PDF'
abre() { tecla ctrl-a; sleep 1.2; escribe "$1"; tecla ret; sleep ${2:-2.5}; }
{ arranca
  shot f1; clic 211 69; sleep 0.8; clic 260 279; aparca; shot f2      # Ver > Página entera
  clic 153 90; aparca; shot f3; clic 153 90; aparca; shot f4; clic 153 90; aparca; shot f5; clic 153 90; aparca; shot f6
  abre "codif.pdf"; shot h1
  abre "filtros.pdf"; shot h2
  abre "moderno.pdf"; tecla ctrl-end; sleep 0.8; shot h3
  clic 83 69; sleep 0.8; clic 150 187; sleep 1.2; shot h3i; tecla ret; sleep 0.8
  abre "incremen.pdf"; shot h4
  abre "espacios.pdf"; shot h5
  sal; } | run 240 -vga std
# GRAFICOS a lo ancho: x = 51 + 1,1457 puntos; y = 116 + 1,1457 puntos desde arriba (la hoja mide 841,89)
pie f1 "Página 1 de 5    86 %    A:\\GRAFICOS.PDF"
chk $T/f1.ppm rgb 223 245 0000FF "el triángulo relleno, por dentro (150, 730)"
chk $T/f1.ppm rgb 180 195 FFFFFF "y por fuera, a su izquierda arriba (110, 770)"
chk $T/f1.ppm rgb 270 195 FFFFFF "y a su derecha arriba (190, 770)"
puntos $T/f1.ppm 380 190 100 100 FF0000 100 400 "la curva, en rojo (una raya fina, no un relleno)"
chk $T/f1.ppm rgb 223 470 00FF00 "el rectángulo hecho con cuatro rayas, relleno"
chk $T/f1.ppm rgb 420 480 999900 "los dos rectángulos de un mismo relleno (1)"
chk $T/f1.ppm rgb 540 480 999900 "los dos rectángulos de un mismo relleno (2)"
pie f2 "Página 1 de 5    36 %    A:\\GRAFICOS.PDF"
puntos $T/f2.ppm 270 425 105 55 tinta 60 1500 "el formulario: su texto, dentro del recuadro gris"
puntos $T/f2.ppm 484 330 12 60 tinta 20 600 "el texto que va hacia arriba"
pie f3 "Página 2 de 5    7"                               # (girada: 200 x 400; cabe a un 7x %)
pie f6 "Página 5 de 5    1"                               # (290 x 180: a más de un 100 %)
pie h1 "Página 1 de 1    86 %    codif.pdf"
avisa h1 "Aviso: hay letras que Hispa-DOS no tiene; se ven como «?»."
puntos $T/h1.ppm 130 180 500 150 tinta 1200 20000 "las seis líneas de las codificaciones"
pie h2 "Página 1 de 1    86 %    filtros.pdf"
puntos $T/h2.ppm 130 170 500 250 tinta 2500 20000 "las diez líneas de los filtros"
pie h3 "Página 5 de 5    86 %    moderno.pdf"
dice h3i "Título: Titulo con ñ y \"comillas\""
dice h3i "Autor: Anónimo"
dice h3i "Versión de PDF: 1.5"
pie h4 "Página 1 de 2    86 %    incremen.pdf"
pie h5 "Página 1 de 1    86 %    espacios.pdf"
muestra f1 lector_graficos; muestra f2 lector_graficos_entera; muestra f3 lector_girada90; muestra f4 lector_girada180
muestra f5 lector_girada270; muestra f6 lector_caja; muestra h1 lector_codif; muestra h2 lector_filtros; muestra h4 lector_incremental
muestra h5 lector_espacios
fi

if [[ $SOLO == *G* ]]; then
echo "== Lector G: imágenes, máscaras y recortes =="
prepara $HVP; sitio
mete $P/IMAGENES.PDF $P/RECORTES.PDF
reabre 'A:\IMAGENES.PDF'
{ arranca
  shot j1
  tecla ctrl-a; sleep 1.2; escribe "recortes.pdf"; tecla ret; sleep 2.5; shot j2
  sal; } | run 150 -vga std
# a lo ancho, con aviso: x = 51 + 1,1457 puntos; y = 136 + 1,1457 puntos desde arriba
pie j1 "Página 1 de 1    86 %    A:\\IMAGENES.PDF"
avisa j1 "Hay imágenes que Lector no sabe mostrar (JPEG dañado): van como recuadros grises."
chk $T/j1.ppm rgb 140 250 FF0000 "la imagen RGB de 2 x 2: rojo arriba a la izquierda" rgb 210 250 00FF00 "verde a la derecha" \
  rgb 140 290 0000FF "azul abajo" rgb 210 290 FFFFFF "y blanco"
chk $T/j1.ppm rgb 280 270 000000 "grises de 2 bits: negro" rgb 310 270 585858 "gris oscuro" rgb 340 270 A5A5A5 "gris claro" rgb 370 270 FFFFFF "blanco"
chk $T/j1.ppm rgb 130 490 FFFFFF "la máscara deja ver la hoja" rgb 155 490 0000FF "y pinta con el color de relleno" rgb 130 515 0000FF "(abajo, al revés)"
chk $T/j1.ppm px 170 450 15 "la JPEG que no lo es, en su recuadro gris"
chk $T/j1.ppm rgb 310 480 0000FF "la imagen dada la vuelta: azul arriba" rgb 310 520 FF0000 "y rojo abajo"
pie j2 "Página 1 de 1    86 %    recortes.pdf"
chk $T/j2.ppm rgb 180 300 0000FF "dentro del recorte, azul" rgb 150 300 FFFFFF "fuera, a la izquierda, la hoja" \
  rgb 180 270 FFFFFF "fuera, arriba" rgb 250 385 FF0000 "el recorte de dentro, rojo" rgb 420 385 FFFFFF "que no se sale del primero"
puntos $T/j2.ppm 165 280 230 55 000000 100 3000 "el texto de dentro del recorte, en negro"
muestra j1 lector_imagenes; muestra j2 lector_recortes
fi

if [[ $SOLO == *H* ]]; then
echo "== Lector H: las JPEG: bloques lisos, transparencia y las que no se saben =="
prepara $HVP; sitio
mete $P/JPEG.PDF $P/JPEGNO.PDF $P/MASCARAS.PDF
reabre 'A:\JPEG.PDF'
abre() { tecla ctrl-a; sleep 1.2; escribe "$1"; tecla ret; sleep ${2:-2.5}; }
{ arranca
  shot k1
  abre "mascaras.pdf"; shot k2
  abre "jpegno.pdf"; shot n0
  for i in 1 2 3 4 5 6 7 8 9 10 11; do clic 153 90; aparca; shot n$i; done      # página siguiente
  sal; } | run 240 -vga std
# JPEG.PDF, a lo ancho y con aviso: x = 51 + 1,1457 puntos; y = 136 + 1,1457 puntos desde arriba
pie k1 "Página 1 de 1    86 %    A:\\JPEG.PDF"
avisa k1 "Aviso: la página está dañada; puede faltar una parte."     # (por la JPEG cortada, que enseña lo que le ha llegado)
chk $T/k1.ppm rgb 114 182 000000 "JPEG gris: el bloque negro" rgb 183 182 848484 "el gris medio" rgb 217 182 FFFFFF "y el blanco"
chk $T/k1.ppm rgb 578 201 FF0000 "JPEG en RGB (APP14 de Adobe, sin girar): rojo" rgb 612 201 00FF00 "verde" rgb 647 201 0000FF "y azul"
chk $T/k1.ppm rgb 423 281 FFFF00 "JPEG en CMYK de Adobe con su /Decode: amarillo" rgb 458 281 00FFFF "cian" rgb 492 281 FF00FF "y magenta"
# MASCARAS.PDF, sin aviso: y = 116 + 1,1457 puntos desde arriba; debajo de las imágenes hay una banda amarilla
pie k2 "Página 1 de 1    86 %    mascaras.pdf"
chk $T/k2.ppm nobusca 46 106 56 114 13 8 "Aviso"
chk $T/k2.ppm rgb 125 235 FF0000 "/SMask: donde es opaca, la imagen (rojo)" rgb 183 235 FFFF00 "donde no, lo de debajo (amarillo)" \
  rgb 125 264 0000FF "opaca por los pelos (128): azul" rgb 183 264 FFFF00 "y transparente"
chk $T/k2.ppm rgb 251 235 FF0000 "/Mask con una imagen de un bit: se ve" rgb 309 235 FFFF00 "no se ve" rgb 251 264 FFFF00 "no se ve" rgb 309 264 FFFFFF "se ve (blanco)"
chk $T/k2.ppm rgb 377 235 FF0000 "/Mask por colores: el índice 0 se pinta" rgb 435 235 FFFF00 "el 1 no" rgb 435 264 848484 "el 3 sí"
# JPEGNO.PDF: la primera se sabe; las demás, cada una con su nombre
pie n0 "Página 1 de 12    86 %    jpegno.pdf"
chk $T/n0.ppm nobusca 46 106 56 114 13 8 "Hay imágenes"
k=1
for que in "JPEG 2000" "fax CCITT" "JBIG2" "JPEG aritmético" "JPEG sin pérdidas" "JPEG jerárquico" "JPEG de 12 bits" "JPEG dañado" "JPEG dañado" "Raro"; do
  avisa n$k "Hay imágenes que Lector no sabe mostrar ($que): van como recuadros grises."
  k=$((k + 1))
done
avisa n11 "Hay imágenes que Lector no sabe mostrar: van como recuadros grises."     # (un espacio de color que no conoce: sin nombre)
chk $T/n1.ppm px 130 250 15 "la JPEG 2000, en su recuadro gris"
muestra k1 lector_jpeg; muestra k2 lector_mascaras; muestra n4 lector_jpeg_no

echo "== Lector H: fotos JPEG de verdad y páginas escaneadas =="
F=$T/fotos
if python3 tests/tools/lector_fotos.py haz $F > $T/fotos.txt 2>&1; then
prepara $HVP; sitio
mete $F/FOTOS.PDF
reabre 'A:\FOTOS.PDF'
{ arranca
  sleep 6; shot m1
  clic 211 69; sleep 0.8; clic 260 279; aparca; sleep 3; shot m2          # Ver > Página entera
  clic 211 69; sleep 0.8; clic 260 239; aparca; sleep 3; shot m3          # Ver > Tamaño real
  sal; } | run 200 -vga std
pie m1 "Página 1 de 1    86 %    A:\\FOTOS.PDF"
avisa m1 "Hay imágenes que Lector no sabe mostrar (JPEG aritmético): van como recuadros grises."
python3 tests/tools/lector_fotos.py pantalla $T/m1.ppm $F FOTOS.PDF 0 aviso || fail=1
python3 tests/tools/lector_fotos.py pantalla $T/m2.ppm $F FOTOS.PDF 0 aviso || fail=1
pie m3 "Página 1 de 1    100 %    A:\\FOTOS.PDF"
muestra m1 lector_fotos; muestra m2 lector_fotos_entera; muestra m3 lector_fotos_100
# una página A4 escaneada a 150 ppp: está en la segunda página (el archivo ya
# está en memoria); se mide desde que se pide hasta que se ve su marca negra
for cual in G C; do
  prepara $HVP; sitio
  mete $F/ESCAN$cual.PDF
  reabre "A:\\ESCAN$cual.PDF"
  { arranca
    sleep 4; shot e${cual}0
    t0=$(date +%s.%N); mon "sendkey ctrl-end" 0.05
    for i in $(seq 1 400); do
      mon "screendump $T/t.ppm" 0.12
      python3 tests/tools/lector_fotos.py oscuro $T/t.ppm 106 171 2>/dev/null && break
    done
    echo "$t0 $(date +%s.%N) $i" > $T/tiempo$cual.txt
    sleep 1; shot e${cual}1
    # y lo que tarda en repintarla al bajar un paso (32 puntos): la marca se va de (106,185)
    t0=$(date +%s.%N); mon "sendkey down" 0.05
    for i in $(seq 1 200); do
      mon "screendump $T/t.ppm" 0.12
      python3 tests/tools/lector_fotos.py oscuro $T/t.ppm 106 185 2>/dev/null || break
    done
    echo "$t0 $(date +%s.%N) $i" > $T/repinta$cual.txt
    sleep 0.5; tecla up; sleep 1
    clic 211 69; sleep 0.8; clic 260 239; aparca; sleep 3; shot e${cual}2        # Ver > Tamaño real
    sal; } | run 240 -vga std
  nom=$([ $cual = G ] && echo gris || echo color)
  pie e${cual}0 "Página 1 de 2    86 %    A:\\ESCAN$cual.PDF"
  pie e${cual}1 "Página 2 de 2    86 %    A:\\ESCAN$cual.PDF"
  chk $T/e${cual}1.ppm nobusca 46 106 56 114 13 8 "Aviso" nobusca 46 106 56 114 13 8 "Hay imágenes"
  python3 tests/tools/lector_fotos.py pantalla $T/e${cual}1.ppm $F ESCAN$cual.PDF 1 || fail=1
  puntos $T/e${cual}1.ppm 110 215 420 300 tinta 3000 60000 "el texto de la página escaneada ($nom), en la pantalla"
  seg=$(awk '{ printf "%.1f", $2 - $1 }' $T/tiempo$cual.txt)
  if awk '{ exit !($3 < 400 && $2 - $1 < 30) }' $T/tiempo$cual.txt; then ok "la página escaneada ($nom, 1240 x 1754) se compone y se pinta en $seg s"
  else ko "la página escaneada ($nom) tarda $seg s (o no ha salido)"; fi
  seg=$(awk '{ printf "%.1f", $2 - $1 }' $T/repinta$cual.txt)
  if awk '{ exit !($3 < 200 && $2 - $1 < 10) }' $T/repinta$cual.txt; then ok "y al desplazarla se repinta en $seg s"
  else ko "al desplazar la página escaneada ($nom) tarda $seg s en repintarse (o no se mueve)"; fi
  pie e${cual}2 "Página 2 de 2    100 %    A:\\ESCAN$cual.PDF"
  muestra e${cual}1 lector_escaneo_$nom; muestra e${cual}2 lector_escaneo_${nom}_100
done
else
  echo "  (ImageMagick no hace las fotos: esta parte no se pasa)"; cat $T/fotos.txt
fi
fi

if [[ $SOLO == *I* ]]; then
echo "== Lector I: imprimir como se ve (una etiqueta con código de barras; el documento entero) =="
prepara $HVP; sitio
mete $P/ETIQUETA.PDF
reabre 'A:\ETIQUETA.PDF'
rm -f $T/lpt.bin
{ arranca
  shot l1
  ir 89 90; shot l1b                                                       # el nombre del botón de la impresora
  tecla ctrl-p; sleep 8; shot l2; tecla ret; sleep 1                       # Ctrl+P: esta página, como se ve
  clic 83 69; sleep 0.8; shot l3m; clic 150 235; sleep 25; shot l3; tecla ret; sleep 1   # Archivo > Imprimir todo el documento
  aparca; shot l4
  sal; } | PARALELO=file:$T/lpt.bin run 240 -vga std
pie l1 "Página 1 de 2"
pie l1b "Imprimir esta página tal como se ve (Ctrl+P)"
dice l2 "La página se ha enviado a la impresora."
chk $T/l3m.ppm busca 55 200 420 285 0 8 "mprimir esta p" busca 55 200 420 285 0 8 "odo el documento (como se ve)" \
  busca 55 200 420 285 0 8 "Imprimir solo el te" busca 55 200 420 285 0 8 "Imprimir solo el texto del doc"
dice l3 "El documento se ha enviado a la impresora."
pie l4 "Página 1 de 2"                                     # y sigue en su página
chk $T/l4.ppm igual 60 140 600 300 $T/l1.ppm 60 140 0     # y como estaba
if [ -s $T/lpt.bin ]; then python3 tests/tools/lector_impresora.py $T/lpt.bin || fail=1
else ko "no ha salido nada por LPT1"; fi
muestra l3m lector_menu_imprimir; muestra l2 lector_impreso
fi

if [[ $SOLO == *K* ]]; then
echo "== Lector K: el botón secundario (el menú contextual) =="
prepara $HVP; sitio
mete $P/FACTURA.PDF
reabre 'A:\FACTURA.PDF'
{ arranca
  derecho 300 250; shot k1                      # sobre la página (la primera): Anterior, en gris
  tecla s; sleep 1.2; aparca; shot k2           # Página siguiente
  derecho 300 250; shot k3                      # en la última: Siguiente, en gris
  tecla t; sleep 1.2; aparca; shot k4           # Tamaño real
  tecla shift-f10; sleep 0.6; shot k5; tecla esc   # con el teclado
  derecho 300 90; sleep 0.5; shot k6            # en la barra de botones no hay menú
  tecla ctrl-p; sleep 1.5; shot k6p; tecla ret; sleep 0.8   # imprimir sin impresora (sin LPT1): lo dice
  clic 83 69; sleep 0.8; clic 150 111; sleep 1  # Archivo > Cerrar
  derecho 300 250; shot k7                      # sin documento: solo vale Abrir
  sal; } | run 200 -vga std
chk $T/k1.ppm busca 300 250 420 560 0 8 "opiar el texto de la p" busca 300 250 420 560 7 8 "Buscar si" \
  busca 300 250 420 560 7 8 "nterior" busca 300 250 420 560 0 8 "iguiente" busca 300 250 420 560 0 8 "pliar" \
  busca 300 250 420 560 0 8 "Guardar el te"
pie k2 "Página 2 de 2"
chk $T/k3.ppm busca 300 250 420 560 0 8 "nterior" busca 300 250 420 560 7 8 "iguiente"
pie k4 "Página 2 de 2    100 %"
chk $T/k5.ppm busca 40 90 200 420 0 8 "opiar el texto de la p"
chk $T/k6.ppm nobusca 280 85 440 420 0 8 "opiar el texto de la p"
dice k6p "No se ha podido imprimir."
dice k6p "está encendida, en línea y con papel."
chk $T/k7.ppm busca 300 250 420 560 7 8 "opiar el texto de la p" busca 300 250 420 560 7 8 "pliar" busca 300 250 420 580 0 8 "Abrir un d"
muestra k1 lector_contexto; muestra k7 lector_contexto_sin
fi

rm -rf $T
[ $fail -eq 0 ] && echo "LECTOR: TODO OK" || echo "LECTOR: HAY FALLOS"
exit $fail
