#!/bin/bash
# Prueba de HispaGL en el PC (docs/486/OPENGL.md): las escenas de tests/gl/
# (triángulos, luz, texturas con mipmaps y filtros, mezcla y prueba de alfa,
# niebla, listas y matrices de vértices, varias texturas, recorte, píxeles,
# trilineal y la sala del juego) se dibujan con el mismo código de
# OPENGL32.DLL (32 bits, x87, sin biblioteca de C: tests/gl/ejecuta_hgl.c) y
# se comparan con las de referencia, que son las de Mesa (llvmpipe), una
# OpenGL que no es la nuestra: tests/gl/ref/*.png. Si en el PC hay Mesa (EGL
# sin pantalla), se dibujan también con él en el momento y se comparan.
# Después mide la sala del juego (los cuadros por segundo en este PC).
#   GUARDAR=carpeta     deja las imágenes y la hoja (HispaGL | referencia | diferencia)
#   REFERENCIAS=1       vuelve a hacer tests/gl/ref/ con Mesa
# HispaGL host test (compared against Mesa).
set -u
cd "$(dirname "$0")/.."
T=$(mktemp -d)
fail=0
echo "== HispaGL en el PC: las escenas contra Mesa =="
CF="-m32 -std=gnu11 -O2 -march=i486 -mtune=pentium -ffreestanding -fno-pic -fno-stack-protector -fno-asynchronous-unwind-tables -fno-builtin -fno-tree-loop-distribute-patterns -fno-strict-aliasing -Iw32/gl -Itests/gl"
OBJ=
for f in w32/gl/mates.c w32/gl/matriz.c w32/gl/estado.c w32/gl/vertice.c w32/gl/textura.c w32/gl/listas.c w32/gl/raster.c w32/gl/pixeles.c tests/gl/escenas.c w32/gl/sala.c tests/gl/ejecuta_hgl.c; do
  o=$T/$(basename $f .c).o
  gcc $CF -c $f -o $o 2> $T/cc.txt || { cat $T/cc.txt; echo "  FAIL no compila $f"; exit 1; }
  OBJ="$OBJ $o"
done
ld -m elf_i386 -static -nostdlib -o $T/ejecuta_hgl $OBJ || { echo "  FAIL no enlaza"; exit 1; }
mkdir -p $T/hgl $T/hgl1 $T/mesa
$T/ejecuta_hgl $T/hgl 2 || { echo "  FAIL ejecuta_hgl"; exit 1; }
$T/ejecuta_hgl $T/hgl1 1 || { echo "  FAIL ejecuta_hgl (calidad normal)"; exit 1; }
REF=tests/gl/ref
MESA=0
if gcc -O2 -DHGL_ANFITRION_64 -Iw32/gl -Itests/gl tests/gl/ejecuta_mesa.c tests/gl/escenas.c w32/gl/sala.c \
     /usr/lib/x86_64-linux-gnu/libGL.so.1 -ldl -o $T/ejecuta_mesa 2> /dev/null && $T/ejecuta_mesa $T/mesa 2> /dev/null; then
  MESA=1
fi
if [ "${REFERENCIAS:-0}" = 1 ]; then
  [ $MESA = 1 ] || { echo "  FAIL REFERENCIAS=1 pide Mesa (EGL sin pantalla)"; exit 1; }
  mkdir -p $REF
  for f in $T/mesa/*.ppm; do python3 -I tests/gl/compara.py --png $f $REF/$(basename $f .ppm).png; done
  echo "  (hechas de nuevo las referencias de $REF con Mesa)"
fi
echo "-- calidad exacta (HISPAGL=EXACTO) contra las referencias guardadas ($REF, de Mesa) --"
python3 -I tests/gl/compara.py $T/hgl $REF --hoja $T/hoja.png || fail=1
echo "-- calidad normal (la de serie: los caminos rápidos de los juegos) contra las mismas --"
python3 -I tests/gl/compara.py $T/hgl1 $REF --hoja $T/hoja1.png || fail=1
if [ $MESA = 1 ]; then
  echo "-- contra Mesa, ahora --"
  python3 -I tests/gl/compara.py $T/hgl $T/mesa || fail=1
else
  echo "  --   (sin Mesa en este PC: solo las guardadas)"
fi
echo "-- la sala del juego: lo que tarda un cuadro en este PC (una sola hebra) --"
for m in "320 240 0" "320 240 1" "640 480 0" "640 480 1"; do
  set -- $m
  r=$($T/ejecuta_hgl --mide $1 $2 300 $3 $3)
  us=$(echo "$r" | awk '{print $2}')
  printf '    %s×%s, %-26s %6s µs por cuadro (%s c/s; %s triángulos y %s puntos por cuadro)\n' $1 $2 \
    "$([ $3 = 0 ] && echo 'rápida (punto más cercano)' || echo 'normal (bilineal)')" $us $(( 1000000 / (us > 0 ? us : 1) )) \
    $(echo "$r" | awk '{print $4}') $(echo "$r" | awk '{print $6}')
done
if [ -n "${GUARDAR:-}" ]; then
  mkdir -p "$GUARDAR"
  cp $T/hoja.png $T/hoja1.png "$GUARDAR"/ 2>/dev/null
  for f in $T/hgl/*.ppm; do python3 -I tests/gl/compara.py --png $f "$GUARDAR"/hgl-$(basename $f .ppm).png; done
fi
rm -rf $T
exit $fail
