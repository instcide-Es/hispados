#!/bin/bash
# Prueba de HispaGL en Hispa-486 (OPENGL32.DLL, GLU32.DLL; docs/486/OPENGL.md):
# los programas de ejemplo dentro de Hispa Ventanas, en QEMU.
#  - GLPRUEBA /PRUEBA en una ventana: el puente de GDI32 (ChoosePixelFormat,
#    SetPixelFormat, SwapBuffers), wglCreateContext, wglMakeCurrent,
#    wglUseFontBitmapsA, GLU32 (gluPerspective, gluSphere, gluBuild2DMipmaps):
#    el suelo con textura y niebla, la caja, la esfera roja con brillo, los
#    cristales, la estrella amarilla y el texto;
#  - GLPRUEBA /COMPLETA /PRUEBA: ChangeDisplaySettingsA a 320×240 y la
#    pantalla completa por DirectDraw (al doble y en el centro: 80,60);
#  - ENGRANA: los engranajes, y los cuadros por segundo (por la traza de
#    HispaGL: SET HISPAGL=TRAZA);
#  - GLPRUEBA /SALA: la sala de medir (multitextura, como un juego de 1997),
#    y sus cuadros por segundo;
#  - si se ha bajado (tests/thirdparty/fetch_gl.sh), WGLGEARS: los engranajes
#    de los demos de Mesa (MIT), un programa de otros, con WGL.
# Los colores se miran por clases (rojo, amarillo...: tests/gl/compara.py
# --cuenta), que la pantalla de Hispa Ventanas puede ser de 8 bits.
# (GUARDAR=carpeta deja las capturas.)
# HispaGL (OpenGL) on Hispa-486 test.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
make -s build/GLPRUEBA.EXE build/ENGRANA.EXE build/OPENGL32.DLL build/GLU32.DLL build/MSVCRT.DLL build/HW32.COM build/WINMM.DLL build/DDRAW.DLL > /dev/null 2>&1 || {
  echo "== Hispa-486: HispaGL: sin gcc de 32 bits, se salta =="; exit 0; }
. tests/tools/win32_ventanas.sh
echo "== Hispa-486: HispaGL (OpenGL 1.1 por programa: OPENGL32.DLL) =="
HAY_WG=0
[ -f tests/thirdparty/dl/gl/wglgears.c ] && make -s build/WGLGEARS.EXE > /dev/null 2>&1 && HAY_WG=1
if [ $HAY_WG = 1 ]; then imagen GLPRUEBA.EXE ENGRANA.EXE WGLGEARS.EXE; else imagen GLPRUEBA.EXE ENGRANA.EXE; fi
# (sitio: los juegos de Hispa Ventanas no se usan aquí)
MTOOLS_SKIP_CHECK=1 mdeltree -i $T/f.img ::HISPADOS/JUEGOS > /dev/null 2>&1
for f in OPENGL32.DLL GLU32.DLL WINMM.DLL DDRAW.DLL; do
  MTOOLS_SKIP_CHECK=1 mcopy -o -i $T/f.img $B/$f ::HISPADOS/$f || ko "no cabe $f en el disco"
done
printf '%s\r\n' '[Centro]' 'Ventana=36,52,452,190' 'Activo=0' 'SinRepetidos=1' 'TareasPrincipal=1' \
  '[Grupo]' 'Nombre=HispaGL' 'Ventana=60,330,600,200' 'Estado=1' \
  '[Acceso]' 'Nombre=Ventana' 'Icono=4' 'Programa=A:\BIN\GLPRUEBA.EXE' 'Parametros=/PRUEBA' \
  '[Acceso]' 'Nombre=Completa' 'Icono=4' 'Programa=A:\BIN\GLPRUEBA.EXE' 'Parametros=/COMPLETA /PRUEBA' \
  '[Acceso]' 'Nombre=Engranajes' 'Icono=4' 'Programa=A:\BIN\ENGRANA.EXE' \
  '[Acceso]' 'Nombre=Sala' 'Icono=4' 'Programa=A:\BIN\GLPRUEBA.EXE' 'Parametros=/SALA /ANCHO 320 /ALTO 240' \
  '[Acceso]' 'Nombre=WglGears' 'Icono=4' 'Programa=A:\BIN\WGLGEARS.EXE' \
  | iconv -f UTF-8 -t CP437 > $T/CENTRO.INI
MTOOLS_SKIP_CHECK=1 mcopy -o -i $T/f.img $T/CENTRO.INI ::HISPADOS/CENTRO.INI
CUENTA="python3 -I tests/gl/compara.py --cuenta"
cuenta() { $CUENTA "$@" || fail=1; }
hasta() {
  local nom=$1 fin=$(( SECONDS + $2 )); shift 2
  while :; do
    printf 'screendump %s\n' "$T/$nom.ppm" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1
    sleep 0.5
    $CUENTA $T/$nom.ppm "$@" x > /dev/null 2>&1 && return 0
    [ $SECONDS -ge $fin ] && return 1
  done
}
# los accesos del grupo «HispaGL» (abierto en 60,330): los iconos en fila,
# cada 104 puntos (125, 229, 333, 437)
{ sleep 6
  send 'SET HISPAGL=TRAZA' 2
  send 'VENTANAS' 9
  doble 125 408
  # la ventana de GLPRUEBA (en 40,30; dentro, 400×300): la esfera roja
  hasta v1 90 44 56 396 294 rojo 300 99999
  sleep 3; shot v2
  tecla esc
  sleep 3; shot v3
  doble 229 408
  hasta c1 90 80 60 640 480 rojo 600 99999
  sleep 3; shot c2
  tecla esc
  sleep 4; shot c3
  doble 333 408
  sleep 45; shot e1
  sleep 10; shot e2
  tecla esc
  sleep 3; shot e3
  doble 437 408
  hasta s1 90 44 56 320 240 ladrillo 15000 99999
  sleep 40; shot s2
  tecla esc
  sleep 3
  if [ $HAY_WG = 1 ]; then
    doble 541 408
    sleep 45; shot w1
    tecla esc
    sleep 3; shot w2
  fi
  mon quit; } | TMAX=480 run
grep -a '\[gl\]' $T/log.txt | sed 's/^/    /' | head -40
echo "-- en una ventana --"
cuenta $T/v2.ppm 44 56 396 294 rojo 300 6000 "la esfera roja (luz, brillo; gluSphere de GLU32)"
cuenta $T/v2.ppm 44 56 396 294 amarillo 300 6000 "la estrella amarilla (prueba de alfa) y el texto (wglUseFontBitmapsA)"
cuenta $T/v2.ppm 44 56 396 294 baldosa 15000 90000 "el suelo de baldosas (textura con mipmaps de gluBuild2DMipmaps, niebla)"
cuenta $T/v2.ppm 44 56 396 294 verde 1000 20000 "el cristal verde (mezcla con alfa)"
chk $T/v3.ppm titulo 36 52 inactiva "Centro de programas"
echo "-- a pantalla completa (ChangeDisplaySettingsA + DirectDraw) --"
cuenta $T/c2.ppm 80 60 640 480 rojo 1000 20000 "la esfera roja, al doble"
cuenta $T/c2.ppm 80 60 640 480 baldosa 60000 250000 "el suelo, al doble"
cuenta $T/c2.ppm 0 0 800 60 negro 40000 48000 "el borde negro de arriba (320×240 al doble, en el centro)"
chk $T/c3.ppm titulo 36 52 inactiva "Centro de programas"
echo "-- los engranajes --"
cuenta $T/e1.ppm 64 64 300 300 rojo 8000 40000 "el engranaje rojo (listas, luz, sombreado plano y suave)"
cuenta $T/e1.ppm 64 64 300 300 verde 2000 20000 "el verde"
cuenta $T/e1.ppm 64 64 300 300 azul 2000 20000 "el azul"
cps() {   # los «cuadros por 10 s» de la traza de un programa: el último que dio (la traza va en orden)
  awk -v p="$1" '/wglCreateContext/{n++} n==p && /cuadros por 10 s/{v=$NF} END{print v}' $T/log.txt
}
CPS=$(cps 3)
if [ -n "$CPS" ]; then
  ok "ENGRANA: $(( 16#$CPS / 10 )),$(( 16#$CPS % 10 )) cuadros por segundo en QEMU (300×300, 16 bits)"
else
  ko "ENGRANA: la traza no dice los cuadros por segundo"
fi
echo "-- la sala de medir (GLPRUEBA /SALA, 320×240) --"
cuenta $T/s2.ppm 44 56 320 240 ladrillo 15000 77000 "las paredes de ladrillo (multitextura: la pared por su mapa de luz)"
CPS=$(cps 4)
if [ -n "$CPS" ]; then
  ok "la sala: $(( 16#$CPS / 10 )),$(( 16#$CPS % 10 )) cuadros por segundo en QEMU (320×240, calidad normal)"
else
  ko "la sala: la traza no dice los cuadros por segundo"
fi
if [ $HAY_WG = 1 ]; then
  echo "-- un programa de otros: wglgears (demos de Mesa, MIT; WGL sin GLUT) --"
  cuenta $T/w1.ppm 0 0 800 600 verde 2000 40000 "WGLGEARS: el engranaje verde"
  cuenta $T/w1.ppm 0 0 800 600 azul 2000 40000 "y el azul"
  CPS=$(cps 5)
  if [ -n "$CPS" ]; then ok "WGLGEARS: $(( 16#$CPS / 10 )),$(( 16#$CPS % 10 )) cuadros por segundo en QEMU"
  else ko "WGLGEARS: la traza no dice los cuadros por segundo"; fi
  chk $T/w2.ppm titulo 36 52 inactiva "Centro de programas"
else
  echo "  --   (sin wglgears: tests/thirdparty/fetch_gl.sh)"
fi
fin
