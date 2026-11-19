#!/bin/bash
# Prueba de la imagen de fondo del escritorio de Hispa Ventanas
# (ventanas/fondo.inc; docs/VENTANAS.md, «La imagen de fondo»): las claves
# «Fondo=» y «Ajuste=» de VENTANAS.INI, el apartado «Escritorio» de
# Configuración y «Poner como fondo de escritorio» de Mirador.
# Las imágenes de la prueba se hacen aquí: PRUEBA.BMP (320x240) y ANCHA.BMP
# (1000x60), de 8 bits con los colores de Hispa Ventanas (tools/mkfondo.py, a
# partir de un dibujo de colores), y TESELA.BMP (48x32, de 24 bits: cuadros
# con colores que están en la paleta y una franja con uno que no).
#  1.ª sesión, 800x600, el escritorio de Hispa-486 con «Fondo=PRUEBA.BMP»
#     (ajustada: a 800x600, cada punto dos veces y media): el fondo pintado
#     (se comparan zonas con el BMP), la barra de tareas encima; una ventana
#     (el Bloc de notas) que se abre, se arrastra (donde estaba vuelve a
#     verse el fondo), se maximiza (no tapa la barra) y se cierra.
#     Configuración › Escritorio: «En mosaico», «Centrada» (con el color del
#     escritorio alrededor), «Sin fondo» (vuelve el dibujo), «Elegir
#     imagen...» (TESELA.BMP, con el diálogo de abrir), un archivo que no es
#     una imagen (lo dice y se queda la que había), «Ajustada» (ampliada),
#     «Estirada» (toda la pantalla, deformada) y «Rellenar» (recortando).
#     Al salir, VENTANAS.INI lleva «Fondo=» y «Ajuste=», y sigue «Escritorio=486».
#  2.ª sesión, 640x480 en 256 colores, el escritorio de Hispa-386 con
#     «Fondo = ANCHA.BMP» (ajustada: reducida a 640x38, en el centro): el
#     color medio de cada zona es el del BMP; arriba, la barra de siempre.
#  3.ª sesión, VGA de 16 colores con «Fondo=» puesto: no hay imagen (el
#     dibujo de siempre), Configuración lo explica y al guardar no se pierde.
#  4.ª sesión, 800x600 sin VENTANAS.INI (Hispa-386 de serie: sin imagen):
#     Mirador con TESELA.BMP, «Imagen › Poner como fondo de escritorio»: deja
#     \HISPADOS\MIFONDO.BMP, el fondo cambia y Configuración lo apunta.
#  (GUARDAR=carpeta deja las capturas.)
# Desktop wallpaper test (VENTANAS.INI keys, Settings, Mirador).
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
export MTOOLS_SKIP_CHECK=1
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() { if grep -qF -- "$1" $T/log.txt; then ok "$1"; else ko "$1"; fi; }
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; sleep "${2:-0.3}"; }
tecla() { local k; for k in "$@"; do mon "sendkey $k" 0.3; done; }
shot() { sleep 0.4; mon "screendump $T/$1.ppm" 0.6; }
send() { printf '%s\r' "$1"; sleep "${2:-1.5}"; }
RX=400; RY=300
ir() {
  local dx=$(( $1 - RX )) dy=$(( $2 - RY )) sx sy
  while [ $dx -ne 0 ] || [ $dy -ne 0 ]; do
    sx=$dx; [ $sx -gt 60 ] && sx=60; [ $sx -lt -60 ] && sx=-60
    sy=$dy; [ $sy -gt 60 ] && sy=60; [ $sy -lt -60 ] && sy=-60
    mon "mouse_move $sx $sy" 0.07
    dx=$(( dx - sx )); dy=$(( dy - sy ))
  done
  RX=$1; RY=$2; sleep 0.3
}
clic() { ir $1 $2; mon "mouse_button 1" 0.25; mon "mouse_button 0" 0.5; }
doble() { ir $1 $2; mon "mouse_button 1" 0.12; mon "mouse_button 0" 0.12; mon "mouse_button 1" 0.12; mon "mouse_button 0" 0.6; }
chk() { python3 -I tests/tools/ventanas_check.py "$@" || fail=1; }
run() {
  rm -f $T/mon
  timeout ${TMAX:-170} qemu-system-i386 ${QEMU_PANTALLA--vga std -global VGA.xres=800 -global VGA.yres=600} -display none -serial stdio -monitor unix:$T/mon,server,nowait \
    -parallel none -m 16 -drive file=$T/f.img,format=raw,if=floppy,index=0 -boot a -no-reboot > $T/raw 2>&1
  tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 > $T/log.txt
}
ini() { mtype -i $T/f.img ::HISPADOS/VENTANAS.INI 2>/dev/null | tr -d '\r' | iconv -f CP437 -t UTF-8 > $T/ven.txt; }
tiene() { if grep -qix -- "$1" $T/ven.txt; then ok "VENTANAS.INI: $1"; else ko "VENTANAS.INI sin «$1»"; fi; }
notiene() { if grep -qi -- "^$1" $T/ven.txt; then ko "VENTANAS.INI: sobra «$1»"; else ok "VENTANAS.INI: sin «$1»"; fi; }
pon_ini() { printf '%s\r\n' "$@" > $T/VENTANAS.INI; mcopy -o -i $T/f.img $T/VENTANAS.INI ::HISPADOS/VENTANAS.INI; }
guarda() { [ -n "${GUARDAR:-}" ] && for f in $T/$1.ppm; do cp $f "$GUARDAR"/fondo_$(basename $f); done 2>/dev/null; }

echo "== Hispa Ventanas: la imagen de fondo del escritorio =="
# ---- las imágenes de la prueba ----
python3 -I - $T <<'EOF' || { ko "no se han podido hacer las imágenes de la prueba"; exit 1; }
import struct, sys
t = sys.argv[1]
# un dibujo de colores: degradados, un disco y unas barras (320x240)
w, h = 320, 240
d = bytearray()
for y in range(h):
    for x in range(w):
        r, g, b = x * 255 // (w - 1), y * 255 // (h - 1), 255 - (x + y) * 255 // (w + h - 2)
        if (x - 100) ** 2 + (y - 90) ** 2 < 50 ** 2:
            r, g, b = 250, 200, 40
        if 200 <= x < 300 and 150 <= y < 220:
            r, g, b = ((x - 200) // 20 * 50, 255 - (x - 200) // 20 * 50, (y - 150) * 3)
        d += bytes((r, g, b))
open(t + '/dibujo.ppm', 'wb').write(b'P6\n%d %d\n255\n' % (w, h) + bytes(d))
# TESELA.BMP: 48x32 de 24 bits; a la izquierda (32x32), cuadros de 8x8 con
# colores del cubo y grises de la paleta; a la derecha, uno que no está
w, h = 48, 32
filas = []
for y in range(h):
    f = bytearray()
    for x in range(w):
        if x < 32:
            n = (x // 8) + 4 * (y // 8)
            c = ((n % 6) * 51, (n * 5 % 6) * 51, (n * 7 % 6) * 51) if n % 3 else ((n * 11) % 253 // 11 * 11,) * 3
        else:
            c = (100, 150, 200)
        f += bytes((c[2], c[1], c[0]))
    filas.append(bytes(f))
datos = b''.join(reversed(filas))
cab = struct.pack('<2sIHHI', b'BM', 54 + len(datos), 0, 0, 54)
cab += struct.pack('<IiiHHIIiiII', 40, w, h, 1, 24, 0, len(datos), 2835, 2835, 0, 0)
open(t + '/TESELA.BMP', 'wb').write(cab + datos)
EOF
python3 tools/mkfondo.py $T/dibujo.ppm $T/PRUEBA.BMP --tam 320x240 --modo estirar > /dev/null || { ko "tools/mkfondo.py"; exit 1; }
python3 tools/mkfondo.py $T/dibujo.ppm $T/ANCHA.BMP --tam 1000x60 --modo estirar > /dev/null || { ko "tools/mkfondo.py"; exit 1; }
# mkfondo: solo los colores fijos (16 a 255) y la paleta de Hispa Ventanas
if python3 -I - $T/PRUEBA.BMP <<'EOF'
import sys
d = open(sys.argv[1], 'rb').read()
assert d[:2] == b'BM' and d[28] == 8 and len(d) == 54 + 1024 + 320 * 240
assert min(d[54 + 1024:]) >= 16
assert d[54 + 16 * 4:54 + 18 * 4] == bytes((0, 0, 0, 0, 51, 0, 0, 0)) and d[54 + 255 * 4:54 + 256 * 4] == bytes((253, 253, 253, 0))
EOF
then ok "tools/mkfondo.py: un BMP de 8 bits con la paleta de Hispa Ventanas y solo los colores fijos (16-255)"
else ko "tools/mkfondo.py: el BMP no es el que se espera"; fi
cp "$IMG" $T/f.img
printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nFILES=30\r\n' > $T/CONFIG.SYS
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
mdel -i $T/f.img '::HISPADOS/PROGS/TIPOS*.DAT' 2>/dev/null     # (sitio para las imágenes)
mcopy -o -i $T/f.img $T/PRUEBA.BMP ::HISPADOS/PRUEBA.BMP || ko "PRUEBA.BMP no cabe en el disquete de la prueba"
mcopy -o -i $T/f.img $T/ANCHA.BMP ::HISPADOS/ANCHA.BMP || ko "ANCHA.BMP no cabe en el disquete de la prueba"
mcopy -o -i $T/f.img $T/TESELA.BMP ::TESELA.BMP || ko "TESELA.BMP no cabe en el disquete de la prueba"
cp $T/f.img $T/base.img
P=$T/PRUEBA.BMP; A=$T/ANCHA.BMP; TE=$T/TESELA.BMP

# ---- 1.ª: 800x600, el escritorio de Hispa-486, «Fondo=PRUEBA.BMP» ----
# La barra de tareas: de y = 572 a 599. El Bloc de notas nace en 150,90.
# Configuración: en 124,112 (su zona, en 126,137): el apartado «Escritorio»
# (el octavo), en 196,400; «Elegir imagen...», en 353,293; «Sin fondo», en
# 493,293; las opciones (ajustada, centrada, mosaico, estirada, rellenar), en
# 286,339 / 359 / 379 / 399 / 419 (encima, [ESTILO] el estilo del escritorio).
pon_ini '; prueba del fondo' '[Escritorio]' 'Escritorio=486' 'Fondo=PRUEBA.BMP'
config() { tecla ctrl-esc; sleep 0.8; tecla c; sleep 0.4; tecla ret; sleep 3; }
{ sleep 6; send 'VENTANAS' 11
  ir 300 200; shot a1                               # el fondo, ajustado a la pantalla
  tecla ctrl-esc; sleep 1; tecla down right; sleep 0.6; tecla ret; sleep 3; ir 700 500; shot a2   # el Bloc de notas
  ir 300 100; mon "mouse_button 1" 0.3; ir 400 200; ir 500 300; mon "mouse_button 0" 0.8; ir 700 500; shot a3   # arrastrado
  doble 500 302; sleep 1; ir 700 500; shot a4       # maximizado
  doble 400 12; sleep 1; tecla alt-f4; sleep 1.5; ir 700 500; shot a5   # como estaba, y cerrado
  config; clic 196 400; sleep 1; ir 700 540; shot b1    # Configuración › Escritorio
  clic 286 379; sleep 2; ir 700 540; shot b2        # «En mosaico»
  clic 286 359; sleep 2; tecla esc; sleep 1.5; ir 700 540; shot b3    # «Centrada» (y fuera Configuración)
  config; clic 493 293; sleep 2; ir 700 540; shot b4    # «Sin fondo»
  clic 353 293; sleep 2; shot b5                    # «Elegir imagen...»: el diálogo
  tecla t e s e l a dot b m p; sleep 0.4; tecla ret; sleep 3; ir 700 540; shot b6
  tecla esc; sleep 1.5; shot b7                     # TESELA.BMP, centrada
  config; clic 353 293; sleep 2; tecla c o n f i g dot s y s; sleep 0.4; tecla ret; sleep 2.5; ir 700 540; shot b8   # no es una imagen
  tecla ret; sleep 1; shot b0; clic 286 339; sleep 2; tecla esc; sleep 1.5; ir 700 540; shot b9    # «Ajustada»
  config; clic 286 399; sleep 2; ir 700 540; shot e0; tecla esc; sleep 1.5; shot e1    # «Estirada»
  config; clic 286 419; sleep 2; tecla esc; sleep 1.5; ir 700 540; shot e2    # «Rellenar»
  tecla ctrl-esc; sleep 0.8; tecla up; sleep 0.4; tecla ret; sleep 2; tecla down; tecla down; tecla ret; sleep 4   # (en 486, «Apagar» es lo elegido: dos abajo, «Volver a Hispa-DOS»)
  send 'ECHO TRAS-SALIR' 2
  mon quit; } | TMAX=270 run
echo "-- el fondo, ajustado a la pantalla, detrás de la barra de tareas --"
chk $T/a1.ppm tam 800 600 bmp $P 0 0 800 600 60 40 200 120 bmp $P 0 0 800 600 520 330 220 200 \
  bmp $P 0 0 800 600 0 0 800 4 bmp $P 0 0 800 600 0 560 800 12 \
  px 400 572 10 "el filete de la barra de tareas" px 84 590 1 "la barra de tareas" texto 26 578 8 4 negrita "Inicio"
echo "-- una ventana encima: abrirla, arrastrarla, maximizarla y cerrarla --"
chk $T/a2.ppm titulo 150 90 activa "Bloc de notas - (sin nombre)" bmp $P 0 0 800 600 20 100 120 300 \
  bmp $P 0 0 800 600 150 40 400 46
chk $T/a3.ppm titulo 350 290 activa "Bloc de notas - (sin nombre)" bmp $P 0 0 800 600 150 90 190 190 \
  bmp $P 0 0 800 600 20 100 120 300
chk $T/a4.ppm marco 0 0 800 572 activa titulo 0 0 activa "Bloc de notas - (sin nombre)" \
  px 400 572 10 "el filete de la barra de tareas" px 84 590 1 "la barra de tareas"
chk $T/a5.ppm bmp $P 0 0 800 600 100 60 600 400 bmp $P 0 0 800 600 0 480 800 92 texto 26 578 8 4 negrita "Inicio"
echo "-- Configuración › Escritorio --"
chk $T/b1.ppm titulo 124 110 activa "Configuración" texto 278 145 4 8 negrita "Escritorio" \
  busca 270 172 290 190 0 8 "Estilo del escritorio:" busca 290 212 320 232 0 8 "Barra de tareas con menú Inicio" \
  busca 270 236 290 254 0 8 "Imagen de fondo del escritorio:" texto 278 259 0 8 negrita "PRUEBA.BMP" \
  busca 290 326 320 346 0 8 "Ajustada a la pantalla (con su proporción)" busca 290 346 320 366 0 8 "Centrada (a su tamaño)" \
  busca 290 366 320 386 0 8 "En mosaico (repetida)" busca 290 386 320 406 0 8 "Estirada a toda la pantalla (se deforma)" \
  busca 290 406 320 426 0 8 "Rellenar la pantalla (recorta lo que sobra)" busca 270 419 290 439 0 8 "Vale un BMP sin comprimir, de 8 o de 24 bits."
chk $T/b2.ppm bmpmos $P 690 20 100 500 bmpmos $P 10 20 100 500 bmpmos $P 130 520 500 50
chk $T/b3.ppm bmp $P 240 180 320 240 240 180 320 240 px 100 100 2 "el color del escritorio, a la izquierda" \
  px 700 300 2 "el color del escritorio, a la derecha" px 400 100 2 "arriba" px 400 500 2 "y abajo" \
  nobusca 100 100 400 140 8 4 "Configuración"
chk $T/b4.ppm texto 278 259 0 8 negrita "(ninguna: se ve el dibujo de Pantalla)" \
  trama 690 20 100 500 trama 10 20 100 500
chk $T/b5.ppm busca 230 165 250 185 0 8 "Nombre del archivo:"
chk $T/b6.ppm texto 278 259 0 8 negrita "A:\\tesela.bmp"
chk $T/b7.ppm bmp $TE 376 284 48 32 376 284 32 32 bmpmedia $TE 376 284 48 32 408 284 16 32 12 \
  px 300 284 2 "el color del escritorio alrededor"
chk $T/b8.ppm busca 100 150 700 450 0 8 "La imagen tiene que ser un BMP sin comprimir, de 8 o de"
chk $T/b0.ppm texto 278 259 0 8 negrita "A:\\tesela.bmp"
chk $T/b9.ppm bmp $TE 0 33 800 533 20 60 480 440 bmpmedia $TE 0 33 800 533 560 100 200 400 12 \
  px 400 10 2 "el color del escritorio, arriba" px 400 568 2 "y abajo"
echo "-- «Estirada» (toda la pantalla, deformada) y «Rellenar» (toda la pantalla, recortando) --"
# TESELA.BMP es de 48x32. Estirada: a 800x600. Rellenar: sus 42 columnas del
# centro (de la 3 a la 44), a 800x600: cada cuadro de 8x8 queda de 152x150
chk $T/e0.ppm busca 270 419 290 439 0 8 "Vale un BMP sin comprimir, de 8 o de 24 bits."
chk $T/e1.ppm bmp $TE 0 0 800 600 20 20 500 540 bmpmedia $TE 0 0 800 600 560 20 220 540 12 bmp $TE 0 0 800 600 0 0 530 8
chk $T/e2.ppm bmp $TE -57 0 914 600 110 20 120 110 bmp $TE -57 0 914 600 260 170 120 110 bmp $TE -57 0 914 600 420 320 110 110 \
  bmp $TE -57 0 914 600 2 2 80 140 bmpmedia $TE -57 0 914 600 580 20 200 540 12
check "Hispa Ventanas ha terminado."
if sed -n '/Hispa Ventanas ha terminado/,$p' $T/log.txt | grep -qF 'TRAS-SALIR'; then ok "después, el intérprete de órdenes sigue"; else ko "sin el intérprete tras salir"; fi
ini
tiene 'Escritorio=486'; tiene 'Fondo=A:\\tesela.bmp'; tiene 'Ajuste=rellenar'
guarda '[abe][0-9]'

# ---- 2.ª: 640x480 en 256 colores, el escritorio de Hispa-386 ----
# ANCHA.BMP (1000x60) ajustada: 640x38, en 0,221. El Centro de programas
# tapa la parte izquierda; a la derecha se ve.
cp $T/base.img $T/f.img
pon_ini '[Pantalla]' 'Dibujo=rombos' '' '[Escritorio]' 'Fondo = ANCHA.BMP' 'AJUSTE=Ajustada'
RX=320; RY=240
{ sleep 6; send 'VENTANAS /640' 11
  ir 600 440; shot c1
  clic 596 13; sleep 1; tecla ret; sleep 4
  send 'ECHO TRAS-SALIR' 2
  mon quit; } | TMAX=120 run
echo "-- 640x480, el escritorio de Hispa-386: la imagen reducida, en el centro --"
chk $T/c1.ppm tam 640 480 px 320 10 1 "la barra de arriba" px 320 27 10 "y su filete" \
  bmpmedia $A 0 221 640 38 500 223 64 34 10 bmpmedia $A 0 221 640 38 564 223 64 34 10 \
  bmpmedia $A 0 221 640 38 500 223 20 34 14 \
  px 600 215 2 "el color del escritorio, encima de la imagen" px 600 265 2 "y debajo"
check "Hispa Ventanas ha terminado."
guarda 'c[0-9]'

# ---- 3.ª: VGA de 16 colores, con «Fondo=» puesto ----
# Configuración: en 44,50 (su zona, en 46,75): «Escritorio», en 116,338;
# «Centrada», en 206,297.
cp $T/base.img $T/f.img
pon_ini '[Escritorio]' 'Escritorio=486' 'Fondo=PRUEBA.BMP' 'Ajuste=mosaico'
RX=320; RY=240
{ sleep 6; send 'VENTANAS /VGA' 11
  ir 300 460; shot v1
  config; clic 116 338; sleep 1; ir 620 20; shot v2
  clic 206 297; sleep 1.5; tecla esc; sleep 2; ir 300 460; shot v3
  tecla ctrl-esc; sleep 0.8; tecla up; sleep 0.4; tecla ret; sleep 2; tecla down; tecla down; tecla ret; sleep 4
  send 'ECHO TRAS-SALIR' 2
  mon quit; } | TMAX=150 run
echo "-- VGA de 16 colores: no hay imagen, queda el dibujo; la clave no se pierde --"
chk $T/v1.ppm tam 640 480 px 320 452 10 "el filete de la barra de tareas" texto 26 458 8 4 negrita "Inicio" \
  trama 0 0 640 440
chk $T/v2.ppm texto 198 198 0 8 negrita "PRUEBA.BMP" busca 180 354 220 384 0 8 "En la VGA de 16 colores no se ve la imagen:"
chk $T/v3.ppm trama 0 0 640 440
check "Hispa Ventanas ha terminado."
ini
tiene 'Escritorio=486'; tiene 'Fondo=PRUEBA.BMP'; tiene 'Ajuste=centrada'
guarda 'v[0-9]'

# ---- 4.ª: 800x600, sin VENTANAS.INI; Mirador pone el fondo ----
# Mirador nace en 40,34 (720x540). TESELA.BMP ajustada: 800x533, en 0,33.
cp $T/base.img $T/f.img
mdel -i $T/f.img ::HISPADOS/VENTANAS.INI 2>/dev/null
mdel -i $T/f.img ::HISPADOS/PRUEBA.BMP ::HISPADOS/ANCHA.BMP 2>/dev/null
printf '[Centro]\r\nReabrir=A:\\HISPADOS\\PROGS\\MIRADOR.HVP A:\\TESELA.BMP\r\n' > $T/CENTRO.INI
mcopy -o -i $T/f.img $T/CENTRO.INI ::HISPADOS/CENTRO.INI
RX=400; RY=300
{ sleep 6; send 'VENTANAS' 12
  ir 600 300; shot m1                               # Mirador, con la imagen; detrás, el dibujo
  tecla alt-i; sleep 1; shot m2                     # el menú Imagen
  tecla f; sleep 4; shot m3                         # «Poner como fondo de escritorio»
  tecla ret; sleep 1; tecla alt-f4; sleep 5.5; ir 780 300; shot m4    # sin Mirador: el fondo
  clic 756 13; sleep 1; tecla ret; sleep 4
  send 'ECHO TRAS-SALIR' 2
  mon quit; } | TMAX=150 run
echo "-- Mirador: «Poner como fondo de escritorio» --"
mcopy -n -i $T/f.img ::HISPADOS/MIFONDO.BMP $T/MIFONDO.BMP 2>/dev/null
if [ -s $T/MIFONDO.BMP ]; then ok "Mirador ha dejado \\HISPADOS\\MIFONDO.BMP"; else ko "falta \\HISPADOS\\MIFONDO.BMP"; cp $TE $T/MIFONDO.BMP; fi
chk $T/m1.ppm titulo 40 34 activa "Mirador - TESELA.BMP" trama 768 40 32 480
chk $T/m2.ppm busca 150 60 400 330 0 8 "Poner como "
chk $T/m3.ppm busca 100 150 700 450 0 8 "La imagen se ha puesto de fondo del escritorio."
chk $T/m4.ppm bmp $T/MIFONDO.BMP 0 33 800 533 500 300 30 250 bmp $T/MIFONDO.BMP 0 33 800 533 560 300 230 250 \
  bmp $TE 0 33 800 533 500 300 30 250 px 400 10 1 "la barra de arriba"
check "Hispa Ventanas ha terminado."
ini
tiene '\[Escritorio\]'; tiene 'Fondo=MIFONDO.BMP'; tiene 'Ajuste=ajustada'; notiene 'Escritorio='
guarda 'm[0-9]'

rm -rf $T
if [ $fail = 0 ]; then echo "TODOS LOS TESTS OK"; else echo "HAY FALLOS"; fi
exit $fail
