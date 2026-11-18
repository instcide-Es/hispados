#!/bin/bash
# Prueba del juego de ejemplo de la fase 5 de Hispa-486: «Naves»
# (w32/naves/naves.c, NAVES.EXE), un matamarcianos Hispa32 con DirectDraw
# (320×240×8 en pantalla completa, Flip, BltFast con clave de color, GetDC),
# DirectSound (música en bucle y efectos), DirectInput (teclado) y WINMM.
# Se abre desde un acceso del Centro con /PRUEBA (la oleada quieta):
#  - la portada: el título (GDI sobre la superficie), la nave, al doble y
#    centrado en los 800×600 de Hispa Ventanas; la música suena;
#  - espacio: la partida (24 naves enemigas, la nuestra abajo, PUNTOS 0);
#  - espacio otra vez: un disparo derriba la enemiga de encima (PUNTOS 10,
#    la explosión), y suenan el disparo y la explosión sobre la música;
#  - flecha a la izquierda (DirectInput: estado inmediato): la nave se mueve;
#  - Esc: el juego acaba y la pantalla vuelve a Hispa Ventanas.
#  En el sonido grabado (AC'97): la música seguida, sin cortes, y más fuerte
#  donde suenan los efectos.
#  (GUARDAR=carpeta deja las capturas.)
# Hispa32 «Naves» (DirectX sample game) test.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
make -s build/NAVES.EXE build/MSVCRT.DLL build/HW32.COM build/WINMM.DLL build/DDRAW.DLL build/DSOUND.DLL build/DINPUT.DLL > /dev/null 2>&1 || {
  echo "== Hispa-486: Naves: sin gcc de 32 bits, se salta =="; exit 0; }
. tests/tools/win32_ventanas.sh
echo "== Hispa-486: Naves, el juego de ejemplo de Hispa32 (DirectX) =="
imagen NAVES.EXE
for f in WINMM.DLL DDRAW.DLL DSOUND.DLL DINPUT.DLL; do
  MTOOLS_SKIP_CHECK=1 mcopy -o -i $T/f.img $B/$f ::HISPADOS/$f || ko "no cabe $f en el disco"
done
printf '%s\r\n' '[Centro]' 'Ventana=36,52,452,190' 'Activo=0' 'SinRepetidos=1' 'TareasPrincipal=1' \
  '[Grupo]' 'Nombre=Hispa32' 'Ventana=60,330,600,200' 'Estado=1' \
  '[Acceso]' 'Nombre=Naves' 'Icono=4' 'Programa=A:\BIN\NAVES.EXE' 'Parametros=/PRUEBA' \
  | iconv -f UTF-8 -t CP437 > $T/CENTRO.INI
MTOOLS_SKIP_CHECK=1 mcopy -o -i $T/f.img $T/CENTRO.INI ::HISPADOS/CENTRO.INI
CHK="python3 -I tests/tools/ventanas_check.py"
hasta() {
  local nom=$1 fin=$(( SECONDS + $2 )); shift 2
  while :; do
    printf 'screendump %s\n' "$T/$nom.ppm" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1
    sleep 0.3
    $CHK $T/$nom.ppm "$@" > /dev/null 2>&1 && return 0
    [ $SECONDS -ge $fin ] && return 1
  done
}
rundx() {
  rm -f $T/mon $T/out.wav
  timeout ${TMAX:-200} qemu-system-i386 -display none -serial stdio -monitor unix:$T/mon,server,nowait \
    -parallel none -m 16 -drive file=$T/f.img,format=raw,if=floppy,index=0 -boot a -no-reboot \
    -vga std -audiodev wav,id=snd0,path=$T/out.wav -device AC97,audiodev=snd0 > $T/raw 2>&1
  tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 | grep -v '^audio:' > $T/log.txt
}
# El juego (320×240) sale al doble desde 80,60: un punto suyo (x, y) está en
# la pantalla en (80 + 2x, 60 + 2y). Colores de su paleta, tal y como los da
# el DAC de 6 bits: amarillo FFE728, verde 28DF3C, blanco FFFFFF
{ sleep 6
  send 'A:\HISPADOS\HSPSND /VOL:100' 3
  send 'VENTANAS' 9
  doble 125 408
  hasta p1 40 cuenta 300 190 200 40 FFE728 200 5000
  sleep 2; shot p2
  tecla spc
  hasta j1 20 cuenta 160 100 40 30 28DF3C 50 1000
  sleep 1; shot j2
  tecla spc; sleep 0.25; shot j3
  sleep 2; shot j4
  mon "sendkey left 1500" 2; sleep 0.5; shot j5
  tecla esc
  hasta f1 20 cuenta 0 0 800 600 28DF3C 0 0
  sleep 1; shot f2
  mon quit; } | rundx
echo "-- la portada --"
chk $T/p2.ppm cuenta 300 190 200 40 FFE728 200 5000 "el título «N A V E S» (GDI en la superficie, al doble)" \
  cuenta 140 270 520 60 F7E7C8 200 8000 "«Un juego de Hispa32» y las teclas" \
  cuenta 384 440 32 32 FFFFFF 20 600 "la nave" rgb 40 300 000000 "el borde negro"
echo "-- la partida --"
chk $T/j2.ppm cuenta 160 100 40 30 28DF3C 50 1000 "la primera nave enemiga (40,24 del juego)" \
  cuenta 580 100 40 30 28DF3C 50 1000 "la última de la fila de arriba (250,24)" \
  cuenta 384 492 32 32 FFFFFF 20 600 "nuestra nave, abajo en el centro" \
  cuenta 88 68 70 14 F7E7C8 20 400 "PUNTOS 0"
chk $T/j4.ppm cuenta 400 186 32 20 28DF3C 0 0 "el disparo ha derribado la enemiga de encima (la del medio, abajo)" \
  cuenta 400 146 32 20 28DF3C 50 1000 "y la de encima de esa sigue" \
  cuenta 88 68 80 14 F7E7C8 20 400 "PUNTOS 10"
chk $T/j5.ppm cuenta 384 492 32 32 FFFFFF 0 0 "la nave se ha movido a la izquierda (DirectInput)" \
  cuenta 80 480 300 60 FFFFFF 20 600 "y está más a la izquierda"
echo "-- Esc: la pantalla vuelve a Hispa Ventanas --"
chk $T/f2.ppm titulo 36 52 inactiva "Centro de programas"
echo "-- el sonido: la música seguida y los efectos --"
if [ -s $T/out.wav ]; then
  python3 tests/tools/audio.py rafagas $T/out.wav 300 20 > $T/audio.txt 2>&1
  python3 - $T/audio.txt <<'EOF' || fail=1
import re, sys
txt = open(sys.argv[1], encoding='utf-8').read()
r = [tuple(float(x) for x in m) for m in re.findall(r'dura ([\d.]+), empieza a (\d+) Hz, pico I (\d+) D (\d+), cortes (\d+)', txt)]
ok = True
def di(c, t):
    global ok
    print(("  OK   " if c else "  FAIL ") + t)
    ok = ok and c
m = [x for x in r if x[0] >= 4.0]
di(bool(m), "la música suena seguida (en bucle, más de 4 s)")
if m:
    di(m[0][4] == 0, "sin cortes (%d)" % m[0][4])
    di(m[0][2] > 4000, "con los efectos encima (pico %d; la música sola no pasa de unos 2.000)" % m[0][2])
if not ok:
    print(txt)
sys.exit(0 if ok else 1)
EOF
else
  ko "QEMU no ha grabado sonido"
fi
fin
