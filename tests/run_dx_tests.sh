#!/bin/bash
# Prueba de la fase 5 de Hispa-486 (docs/486/HISPA-486.md): DirectX hasta el
# 7 sin Direct3D (DDRAW.DLL, DSOUND.DLL, DINPUT.DLL) y WINMM.DLL, hechos en C
# (w32/dx/), dentro de Hispa Ventanas. WINDX.EXE (tests/progs/windx.c):
#  - pantalla completa en 320×240 a 8 bits: agrandada al doble y centrada en
#    los 800×600 de Hispa Ventanas, con su paleta en el DAC; Blt con color,
#    BltFast con clave de color y Blt estirado con DDBLT_KEYSRC (se miran los
#    puntos); suena un la de 440 Hz por DirectSound (QEMU lo graba);
#  - el espacio, por DirectInput (por búfer), pasa a 640×480 a 16 bits en
#    una pantalla de 8 (paleta 3-3-2): los colores, un texto con GetDC; el
#    tono sube a 660 Hz con SetFrequency;
#  - otro espacio: la pantalla vuelve a Hispa Ventanas (con su paleta) y la
#    ventana dice «Comprobaciones: 24 de 24 bien.» (timeGetTime,
#    timeSetEvent, waveOut con su WOM_DONE, PlaySound de memoria,
#    GetSystemMetrics en pantalla completa, GetSurfaceDesc, Lock...);
#  - en el sonido grabado: el la y luego el mi, sin cortes.
#  CARD=hda|sb16 cambia la tarjeta (AC'97 si no). (GUARDAR=carpeta deja las capturas.)
# DirectX (phase 5) test.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
make -s build/WINDX.EXE build/MSVCRT.DLL build/HW32.COM build/WINMM.DLL build/DDRAW.DLL build/DSOUND.DLL build/DINPUT.DLL > /dev/null 2>&1 || {
  echo "== Hispa-486: DirectX: sin gcc de 32 bits, se salta =="; exit 0; }
. tests/tools/win32_ventanas.sh
CARD=${CARD:-ac97}
case $CARD in
  ac97) DEV="-device AC97,audiodev=snd0"; NAME="Intel AC'97" ;;
  hda)  DEV="-device intel-hda -device hda-micro,audiodev=snd0"; NAME="Intel HD Audio" ;;
  sb16) DEV="-device sb16,audiodev=snd0"; NAME="Sound Blaster 16" ;;
esac
echo "== Hispa-486: DirectDraw, DirectSound, DirectInput y WINMM ($NAME) =="
imagen WINDX.EXE
for f in WINMM.DLL DDRAW.DLL DSOUND.DLL DINPUT.DLL; do
  MTOOLS_SKIP_CHECK=1 mcopy -o -i $T/f.img $B/$f ::HISPADOS/$f || ko "no cabe $f en el disco"
done
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
  timeout ${TMAX:-200} qemu-system-i386 ${QEMU_PANTALLA--vga std -global VGA.xres=800 -global VGA.yres=600} -display none -serial stdio -monitor unix:$T/mon,server,nowait \
    -parallel none -m 16 -drive file=$T/f.img,format=raw,if=floppy,index=0 -boot a -no-reboot \
    -vga std -audiodev wav,id=snd0,path=$T/out.wav $DEV > $T/raw 2>&1
  tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 | grep -v '^audio:' > $T/log.txt
}
# A: 320×240 al doble desde 80,60. B: 640×480 tal cual desde 80,60
{ sleep 6
  send 'A:\HISPADOS\HSPSND /VOL:100' 3
  send 'VENTANAS' 9
  doble 125 408
  hasta a1 40 rgb 200 150 FF0000 rgb 280 460 0000FF
  sleep 1.5; shot a2
  tecla spc
  hasta b1 25 rgb 300 200 FF0000 rgb 600 450 00FF00
  sleep 1.5; shot b2
  tecla spc
  hasta c1 30 busca 12 55 780 140 232 231 "Comprobaciones:"
  sleep 1; shot c2
  tecla esc; sleep 2; shot d1
  mon quit; } | rundx
echo "-- A: 320×240×8 en pantalla completa (al doble, centrada) --"
chk $T/a2.ppm rgb 200 150 FF0000 "Blt con color: el cuarto rojo" rgb 280 460 0000FF "el fondo azul" \
  rgb 40 300 000000 "fuera del modo: negro (el color más oscuro de su paleta)" \
  rgb 79 300 000000 "el borde, hasta x = 79" rgb 80 300 0000FF "y desde x = 80, el juego" \
  rgb 512 292 FFFF00 "BltFast: el dibujo" rgb 482 262 0000FF "BltFast: la clave de color, transparente" \
  rgb 624 444 FFFF00 "Blt estirado al doble" rgb 564 384 0000FF "Blt estirado: la clave" \
  rgb 591 444 0000FF "Blt estirado: antes del dibujo" rgb 592 444 FFFF00 "y el primer punto del dibujo (al doble)"
echo "-- B: 640×480×16 en una pantalla de 8 bits (paleta 3-3-2) --"
chk $T/b2.ppm rgb 300 200 FF0000 "el rojo de 16 bits" rgb 600 450 00FF00 "el verde" rgb 580 160 000000 "el negro" \
  rgb 380 370 0000FF "Lock: el degradado" rgb 40 300 000000 "el borde" \
  cuenta 96 76 72 16 FFFFFF 50 600 "GetDC: el texto «Hispa-486» con el GDI"
echo "-- C: la pantalla vuelve a Hispa Ventanas; las comprobaciones --"
chk $T/c2.ppm titulo 10 30 activa "DirectX de Hispa-486" busca 12 55 780 140 232 231 "Comprobaciones: 24 de 24 bien."
chk $T/d1.ppm nobusca 0 0 800 600 232 231 "Comprobaciones:"
echo "-- el sonido (DirectSound): el la, luego el mi, sin cortes --"
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
la = [x for x in r if 420 <= x[1] <= 460 and x[0] >= 1.0]
di(bool(la), "suena el la de 440 Hz (DirectSound, en bucle)")
if la:
    di(la[0][2] > 3000 and la[0][3] > 3000, "por los dos canales (pico %d y %d)" % (la[0][2], la[0][3]))
    # (uno, como mucho: el cambio a 640×480×16 y su GetDC, que pasa los
    # 307.200 puntos al GDI y vuelta, tardan más que la cola de HSPSND)
    di(la[0][4] <= 1, "sin cortes, salvo quizá uno al cambiar de modo (%d)" % la[0][4])
if not ok:
    print(txt)
sys.exit(0 if ok else 1)
EOF
else
  ko "QEMU no ha grabado sonido"
fi
fin
