#!/bin/bash
# Prueba del Reproductor de sonido (SONIDOS.HVP) y de la campanita del volumen
# de Hispa Ventanas. QEMU graba en un .wav lo que sale por la tarjeta
# (-audiodev wav, como run_sound_tests.sh) y se mide cada sonido de la
# grabación (tests/tools/audio.py rafagas): lo que dura, la frecuencia con la
# que empieza y el pico de cada canal. El entorno se maneja con el ratón y el
# teclado del monitor de QEMU y las capturas se miran punto a punto
# (tests/tools/ventanas_check.py).
#  Con Intel AC'97, con Intel HD Audio y con una Sound Blaster 16:
#  - SONIDOS.HVP está en \HISPADOS\PROGS, y HISPA.WAV y CAMPANA.WAV en
#    \HISPADOS (disquete e imagen USB); su acceso sale en «Accesorios»;
#  - la ventana dice qué tarjeta lleva HSPSND;
#  - «Tono 440 Hz»: dos segundos a 440 Hz por los dos canales;
#  - «Sonido de prueba»: HISPA.WAV entero (4,6 s), que empieza con un do de
#    523 Hz, por los dos canales; la barra de estado dice «Sonando...» y, al
#    acabar, «Terminado.»;
#  - «Izquierda» y «Derecha»: el mismo sonido por un solo canal;
#  - «Parar» lo corta;
#  - «Abrir WAV...»: un WAV de 8 bits, mono, a 8.000 Hz suena a su frecuencia
#    y lo que dura; uno que no es PCM se rechaza y lo dice;
#  - el volumen: al soltar el deslizador del Reproductor, la barra del
#    altavoz de Hispa Ventanas y el deslizador de Configuración > Sonido suena
#    la campanita, más fuerte al 100 % que al 20 % (con la Sound Blaster 16
#    solo se mira que suena: QEMU no aplica su mezclador a la grabación); en
#    silencio no suena.
#  Sin HSPSND: la ventana lo avisa («Cargue HSPSND antes de abrir Hispa
#  Ventanas») y los botones no hacen nada más que decirlo.
#  CARDS="ac97 hda sb16" elige las tarjetas; CARDS=sb8 prueba además la vía de
#  las Sound Blaster de 8 bits (un canal: «Izquierda» y «Derecha» lo dicen y
#  suenan por los dos lados); no va en la lista por omisión.
# Deja capturas en build/sonidos_*.png.
# Sound player (SONIDOS.HVP) and volume chime test.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
B=build
T=$(mktemp -d)
CHK="python3 tests/tools/ventanas_check.py"
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() { if grep -qF -- "$1" $T/log.txt; then ok "$1"; else ko "$1"; fi; }
checkn() { if grep -qF -- "$1" $T/log.txt; then ko "(no debía salir) $1"; else ok "sin \"$1\""; fi; }
ini() { if grep -qxF -- "$1" $T/ini.txt; then ok "CENTRO.INI: $1"; else ko "CENTRO.INI sin la línea «$1»"; fi; }
chk() { $CHK "$@" || fail=1; }
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; sleep "${2:-0.3}"; }
tecla() { local k; for k in "$@"; do mon "sendkey $k" 0.3; done; }
shot() { sleep 0.4; mon "screendump $T/$1.ppm" 0.6; }
send() { printf '%s\r' "$1"; sleep "${2:-1.5}"; }
muestra() { [ -f $T/$1.ppm ] && convert $T/$1.ppm $B/sonidos_$2.png; }
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
doble() { ir $1 $2; mon "mouse_button 1" 0.1; mon "mouse_button 0" 0.1; mon "mouse_button 1" 0.1; mon "mouse_button 0" 0.8; }
aparca() { ir 16 320; }
escribe() {
  local t="$1" i c
  for ((i=0; i<${#t}; i++)); do
    c="${t:$i:1}"
    case "$c" in
      ' ') tecla spc;;
      '.') tecla dot;;
      [A-Z]) tecla "shift-$(echo $c | tr A-Z a-z)";;
      *) tecla "$c";;
    esac
  done
}
run() {
  local tmo=$1; shift
  rm -f $T/mon $T/out.wav
  timeout $tmo qemu-system-i386 -display none -serial stdio -monitor unix:$T/mon,server,nowait \
    -parallel none -m 16 -drive file=$T/f.img,format=raw,if=floppy,index=0 -boot a -no-reboot "$@" > $T/raw 2>&1
  tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 | grep -v '^audio:' > $T/log.txt
  mtype -i $T/f.img ::HISPADOS/CENTRO.INI 2>/dev/null | tr -d '\r' | iconv -f CP437 -t UTF-8 > $T/ini.txt
}
# La ventana del Reproductor sale centrada: en 800x600, su zona empieza en
# (160,160). El centro de cada botón, en la pantalla:
PRUEBA="400 258"; IZQ="284 302"; DER="516 302"; TONO="284 340"; ABRIR="516 340"; PARAR="400 378"
VOLX() { echo $(( 258 + 2 * $1 )); }        # el deslizador: 2 puntos por cada 1 %
VOLY=420
MUDO="544 421"
EST="texto 174 461 0 9 fina"                # la barra de estado
printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nFILES=30\r\n' > $T/CONFIG.SYS

echo "== Reproductor de sonido: los archivos del disco =="
for f in PROGS/SONIDOS.HVP HISPA.WAV CAMPANA.WAV; do
  if mdir -i "$IMG" ::HISPADOS/$f > /dev/null 2>&1; then ok "$f está en \\HISPADOS del disquete"; else ko "falta $f en \\HISPADOS del disquete"; fi
  if [ -f $B/hispados-usb.img ]; then
    if MTOOLS_SKIP_CHECK=1 mdir -i $B/hispados-usb.img@@1048576 ::HISPADOS/$f > /dev/null 2>&1; then
      ok "y en la imagen USB"
    else
      ko "falta $f en \\HISPADOS de la imagen USB"
    fi
  fi
done
python3 - $B/HISPA.WAV $B/CAMPANA.WAV <<'EOF' || fail=1
import sys, wave
bien = True
for ruta, quiero in ((sys.argv[1], (1, 2, 22050)), (sys.argv[2], (1, 1, 11025))):
    w = wave.open(ruta, 'rb')
    es = (w.getnchannels(), w.getsampwidth(), w.getframerate())
    dura = w.getnframes() / w.getframerate()
    lim = (4.0, 6.0) if quiero[1] == 2 else (0.25, 0.35)
    if es == quiero and lim[0] <= dura <= lim[1]:
        print("  OK   %s: %d canales, %d bits, %d Hz, %.2f s" % (ruta, es[0], es[1] * 8, es[2], dura))
    else:
        print("  FAIL %s: %s, %.2f s" % (ruta, es, dura)); bien = False
sys.exit(0 if bien else 1)
EOF
if [ "$(stat -c %s $B/CAMPANA.WAV)" -le 4096 ]; then ok "CAMPANA.WAV cabe en lo que lee Hispa Ventanas ($(stat -c %s $B/CAMPANA.WAV) octetos)"; else ko "CAMPANA.WAV pasa de 4096 octetos"; fi

for CARD in ${CARDS:-ac97 hda sb16}; do
  PRE="REM"; LADO="solo por la izquierda"; UMB=120
  case $CARD in
    ac97) DEV="-device AC97,audiodev=snd0"; NAME="Intel AC'97" ;;
    hda)  DEV="-device intel-hda -device hda-micro,audiodev=snd0"; NAME="Intel HD Audio" ;;
    sb16) DEV="-device sb16,audiodev=snd0"; NAME="Sound Blaster 16" ;;
    # (la vía de 8 bits y un canal: no va en la lista; CARDS=sb8 para probarla)
    sb8)  DEV="-device sb16,audiodev=snd0"; NAME="Sound Blaster"; PRE="SET BLASTER=A220 I5 D1 H0 T4"; UMB=400 ;;
  esac
  echo "== Reproductor de sonido con $NAME: tono, sonido de prueba, izquierda, derecha, parar, WAV y campanita =="
  cp "$IMG" $T/f.img
  mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
  python3 tests/tools/audio.py make $T/OCHO.WAV 300 1500 8000 8 1
  mcopy -o -i $T/f.img $T/OCHO.WAV ::OCHO.WAV
  # un WAV que no es PCM (formato 3: coma flotante)
  python3 - $T/OCHO.WAV $T/RARO.WAV <<'EOF'
import sys
d = bytearray(open(sys.argv[1], 'rb').read())
d[20] = 3
open(sys.argv[2], 'wb').write(d)
EOF
  mcopy -o -i $T/f.img $T/RARO.WAV ::RARO.WAV
  RX=400; RY=300
  { sleep 6
    send 'KEYB SP' 2
    send 'A:\HISPADOS\HSPRATON' 2
    send "$PRE" 1
    send 'A:\HISPADOS\HSPSND /VOL:100' 3
    send 'VENTANAS' 6
    # ---- el grupo Accesorios: el Reproductor es el sexto ----
    doble 206 129; sleep 1
    aparca
    shot a0
    tecla right right right right right ret; sleep 3
    aparca
    shot a1
    # ---- tono, sonido de prueba, izquierda y derecha ----
    clic $TONO; sleep 0.8
    shot a2
    sleep 3.5
    clic $PRUEBA; sleep 1.2
    aparca
    shot a3
    sleep 6
    shot a4
    clic $IZQ; sleep 1
    shot a5
    sleep 6
    clic $DER; sleep 7
    # ---- parar a medias ----
    clic $PRUEBA; sleep 1.2
    clic $PARAR; sleep 1.5
    aparca
    shot a6
    # ---- Abrir WAV: uno de 8.000 Hz y uno que no se admite ----
    clic $ABRIR; sleep 1.5
    shot a7d
    escribe "OCHO.WAV"; tecla ret; sleep 0.6
    shot a7
    sleep 3
    clic $ABRIR; sleep 1.5
    escribe "RARO.WAV"; tecla ret; sleep 2
    aparca
    shot a8
    # ---- el volumen del Reproductor: al 20 % y al 100 %, y silencio ----
    clic $(VOLX 20) $VOLY; sleep 2
    aparca
    shot a9
    clic $(VOLX 100) $VOLY; sleep 2
    clic $MUDO; sleep 1.5
    clic $(VOLX 60) $VOLY; sleep 2
    aparca
    shot a10
    clic $MUDO; sleep 2                     # (al quitar el silencio también suena: al 60 %)
    clic $(VOLX 100) $VOLY; sleep 2
    tecla esc; sleep 1.5
    # ---- la barra del altavoz de Hispa Ventanas ----
    clic 698 14; sleep 1
    clic 610 92; sleep 2
    clic 770 92; sleep 2
    aparca
    shot a11
    clic 698 14; sleep 1
    # ---- Configuración > Sonido ----
    doble 102 129; sleep 1.5
    tecla right right right right ret; sleep 2.5
    clic 196 264; sleep 0.5
    clic 344 207; sleep 2
    clic 568 207; sleep 2
    aparca
    shot a12
    tecla esc; sleep 1
    clic 756 13; sleep 1; tecla ret; sleep 2.5
    send 'ECHO SIGUE-VIVO'
    mon quit
  } | run 170 -vga std -audiodev wav,id=snd0,path=$T/out.wav $DEV
  python3 tests/tools/audio.py rafagas $T/out.wav $UMB 20 > $T/audio.txt 2>&1
  chk $T/a0.ppm titulo 224 164 activa "Accesorios"
  chk $T/a1.ppm tam 800 600 marco 158 135 484 353 activa titulo 158 135 activa "Reproductor de sonido" \
    texto 174 168 0 8 negrita "Tarjeta: $NAME" $EST "Listo." \
    texto 174 412 0 8 fina "Volumen:" texto 474 412 0 8 fina "100 %"
  chk $T/a2.ppm $EST "Sonando... tono de 440 Hz (2 segundos)"
  chk $T/a3.ppm $EST "Sonando... sonido de prueba (22050 Hz, 16 bits, mono)" px 180 446 12 "la barra de por dónde va, empezada" px 600 446 9 "y sin acabar"
  chk $T/a4.ppm $EST "Terminado." px 180 446 9 "la barra de por dónde va, vacía"
  [ $CARD = sb8 ] && LADO="la tarjeta es mono: igual por los dos lados"
  chk $T/a5.ppm $EST "Sonando... $LADO"
  chk $T/a6.ppm $EST "Parado."
  chk $T/a7.ppm $EST "Sonando... OCHO.WAV (8000 Hz, 8 bits, mono)"
  chk $T/a8.ppm $EST "No se admite: solo WAV PCM de 8 o 16 bits, 8000-48000 Hz"
  chk $T/a9.ppm texto 474 412 0 8 fina "20 %"
  chk $T/a10.ppm texto 474 412 0 8 fina "60 %" px 544 421 4 "«Silencio», marcada"
  chk $T/a11.ppm titulo 550 34 activa "Volumen" texto 568 63 0 8 fina "Volumen: 100 %"
  chk $T/a12.ppm titulo 124 110 activa "Configuración" texto 586 199 0 8 fina "100 %"
  python3 tests/tools/audio.py check $T/out.wav > $T/tramos.txt 2>&1
  python3 - $T/audio.txt $CARD $T/tramos.txt <<'EOF' || fail=1
import re, sys
txt = open(sys.argv[1], encoding='utf-8').read()
card = sys.argv[2]
r = [tuple(float(x) for x in m) for m in
     re.findall(r'ráfaga ([\d.]+)-([\d.]+) s: dura ([\d.]+), empieza a (\d+) Hz, pico I (\d+) D (\d+)', txt)]
tr = [tuple(float(x) for x in m) for m in
      re.findall(r'tramo ([\d.]+)-([\d.]+) s: (\d+) Hz', open(sys.argv[3], encoding='utf-8').read())]
bien = True
def mira(cond, que):
    global bien
    print("  %s %s" % ("OK  " if cond else "FAIL", que))
    bien = bien and bool(cond)
def n(i):
    return r[i] if i < len(r) else (0, 0, 0, 0, 0, 0)
mono = card == 'sb8'                    # (un canal: sale igual por los dos lados)
t = n(0)
mira(abs(t[2] - 2.0) < 0.25 and abs(t[3] - 440) < 15 and t[4] > 8000 and t[5] > 8000,
     "Tono 440 Hz: %.2f s a %d Hz por los dos canales (picos %d y %d)" % (t[2], t[3], t[4], t[5]))
t = n(1)
mira(abs(t[2] - 4.65) < 0.35 and abs(t[3] - 523) < 25 and t[4] > 8000 and abs(t[4] - t[5]) < t[4] * 0.05,
     "Sonido de prueba: %.2f s, empieza a %d Hz, igual por los dos canales (picos %d y %d)" % (t[2], t[3], t[4], t[5]))
t = n(2)
if mono:
    mira(abs(t[2] - 4.65) < 0.35 and t[4] > 4000 and abs(t[4] - t[5]) < t[4] * 0.05,
         "Izquierda con una tarjeta mono: %.2f s, por los dos canales (picos %d y %d)" % (t[2], t[4], t[5]))
else:
    mira(abs(t[2] - 4.65) < 0.35 and abs(t[3] - 523) < 25 and t[4] > 8000 and t[5] < t[4] * 0.02,
         "Izquierda: %.2f s, casi todo por el canal izquierdo (picos %d y %d)" % (t[2], t[4], t[5]))
t = n(3)
if mono:
    mira(abs(t[2] - 4.65) < 0.35 and t[4] > 4000 and abs(t[4] - t[5]) < t[4] * 0.05,
         "Derecha con una tarjeta mono: %.2f s, por los dos canales (picos %d y %d)" % (t[2], t[4], t[5]))
else:
    mira(abs(t[2] - 4.65) < 0.35 and abs(t[3] - 523) < 25 and t[5] > 8000 and t[4] < t[5] * 0.02,
         "Derecha: %.2f s, casi todo por el canal derecho (picos %d y %d)" % (t[2], t[4], t[5]))
# «Parar» y el WAV de 8.000 Hz (300 Hz durante 1,5 s). Con algunas tarjetas,
# al parar, QEMU deja de grabar hasta el sonido siguiente y los dos salen
# pegados: se separan por el tramo de 300 Hz
t = n(4)
ocho = [x for x in tr if abs(x[2] - 300) < 12 and x[1] - x[0] > 1.0]
sig = 5
if ocho and t[0] <= ocho[0][0] < t[1]:
    parar = ocho[0][0] - t[0]
else:
    parar = t[2]
    sig = 6
mira(0.6 < parar < 3.6 and abs(t[3] - 523) < 25, "Parar corta el sonido de prueba (a los %.2f s)" % parar)
mira(ocho and abs(ocho[0][1] - ocho[0][0] - 1.5) < 0.25,
     "un WAV de 8000 Hz, 8 bits y mono suena a 300 Hz durante 1,5 s (%s)" % ", ".join("%.2f s a %d Hz" % (x[1] - x[0], x[2]) for x in ocho))
# las campanitas: Reproductor 20 y 100, (silencio: nada), al quitarlo 60 y
# 100; barra 20 y 100; Configuración 20 y 100
c = [n(i) for i in range(sig, sig + 8)]
mira(len(r) == sig + 8, "ocho campanitas, ninguna en silencio (%d sonidos después del WAV)" % (len(r) - sig))
mira(all(0.15 < x[2] < 0.6 for x in c), "de unos 0,3 s (%s)" % ", ".join("%.2f" % x[2] for x in c))
for nombre, bajo, alto in (("el Reproductor (20 % y 100 %)", 0, 1), ("el Reproductor al quitar el silencio (60 %) y al 100 %", 2, 3),
                           ("la barra del altavoz (20 % y 100 %)", 4, 5), ("Configuración (20 % y 100 %)", 6, 7)):
    a, b = max(c[bajo][4], c[bajo][5]), max(c[alto][4], c[alto][5])
    if card in ('sb16', 'sb8'):
        mira(a > 1000 and b > 1000, "campanita desde %s (picos %d y %d; QEMU no aplica el mezclador de la SB16)" % (nombre, a, b))
    else:
        mira(a > 150 and b > a * 1.2, "campanita desde %s: más fuerte con más volumen (picos %d y %d)" % (nombre, a, b))
sys.exit(0 if bien else 1)
EOF
  ini "Nombre=Reproductor de sonido"; ini "Programa=A:\\HISPADOS\\PROGS\\SONIDOS.HVP"; ini "Volumen=100"; ini "Silencio=0"
  check "Hispa Ventanas ha terminado."
  check "SIGUE-VIVO"
  checkn "HSPDPMI: excepción"
  if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/audio.txt; fi
  if [ $CARD = ac97 ]; then
    muestra a1 ventana; muestra a3 sonando; muestra a8 rechazado; muestra a10 silencio
  fi
  [ -n "${GUARDA:-}" ] && { mkdir -p $GUARDA/$CARD; cp $T/*.ppm $T/audio.txt $T/log.txt $T/out.wav $GUARDA/$CARD/ 2>/dev/null; }
done

echo "== Reproductor de sonido sin HSPSND: lo avisa =="
cp "$IMG" $T/f.img
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
RX=400; RY=300
{ sleep 6
  send 'KEYB SP' 2
  send 'A:\HISPADOS\HSPRATON' 2
  send 'VENTANAS' 6
  doble 206 129; sleep 1
  tecla right right right right right ret; sleep 3
  aparca
  shot n1
  clic $PRUEBA; sleep 1
  aparca
  shot n2
  clic $TONO; sleep 1
  clic $(VOLX 50) $VOLY; sleep 1
  tecla esc; sleep 1
  clic 756 13; sleep 1; tecla ret; sleep 2.5
  send 'ECHO SIGUE-VIVO'
  mon quit
} | run 60 -vga std -audiodev wav,id=snd0,path=$T/out.wav -device AC97,audiodev=snd0
chk $T/n1.ppm titulo 158 135 activa "Reproductor de sonido" texto 174 168 0 8 negrita "HSPSND no está cargado: no hay sonido." \
  texto 174 188 0 8 fina "Cargue HSPSND antes de abrir Hispa Ventanas: salga,"
chk $T/n2.ppm $EST "No hay sonido: HSPSND no está cargado."
python3 tests/tools/audio.py rafagas $T/out.wav 120 > $T/audio.txt 2>&1
if grep -q "ráfaga" $T/audio.txt; then ko "sin HSPSND no tenía que sonar nada"; cat $T/audio.txt; else ok "no suena nada"; fi
check "Hispa Ventanas ha terminado."
check "SIGUE-VIVO"
checkn "HSPDPMI: excepción"
muestra n1 sin_hspsnd
[ -n "${GUARDA:-}" ] && { mkdir -p $GUARDA/no; cp $T/*.ppm $T/log.txt $GUARDA/no/ 2>/dev/null; }
rm -rf $T
exit $fail
