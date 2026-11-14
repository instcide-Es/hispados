#!/bin/bash
# Prueba de lo que tarda un cambio de volumen (el fallo de un equipo real:
# «al cambiar el volumen en Ventanas con el ratón, la pantalla se queda
# congelada dos segundos»).
#  (a) VOLTIME (tests/progs/voltime.asm) mide 50 llamadas a la función 05h de
#      HSPSND y 50 pulsaciones de las teclas de volumen (Ctrl+Alt+flechas)
#      con HD Audio (hda-duplex y hda-output), AC'97 y Sound Blaster 16: pocos
#      milisegundos, y con HD Audio dos órdenes al códec por cambio (el
#      amplificador del DAC, izquierda y derecha) y ninguna sin respuesta.
#  (b) HD Audio con un códec que no contesta al nodo del volumen (HSPSND
#      /SIMSORDO:02, solo para pruebas; antes eran 0x100000 vueltas de espera
#      por orden, casi un segundo cada una en un equipo real): el primer
#      cambio tarda unos milisegundos, los demás nada, y el volumen sigue
#      funcionando por programa (se mide en la grabación de QEMU).
#  (c) Hispa Ventanas con ese mismo códec: se arrastra la barra del volumen
#      con el ratón y, nada más soltar, la ventanita ya dice el valor nuevo
#      (la pantalla no se ha quedado esperando a la tarjeta) y HSPSND lo tiene.
#  (d) [TOSHIBA] el Realtek ALC269 del Toshiba NB250, simulado (HSPSND
#      /SIMALC, residente; lib/alcsim.asm): su DAC tiene 87 pasos de 0,75 dB
#      (0 dB en el 87), no los 127 de otros. Al 100 %, 50 % y 10 % las órdenes
#      de ganancia (salen por COM1: «ALCSIM xxxxxxxx») van dentro de 0..87, a
#      los dos lados, y con los dB de la curva de HSPSND ((100 - %) * 15/32
#      dB, ±0,75); y en la grabación de QEMU (hda-duplex) el tono baja de
#      verdad. Con las teclas de volumen, lo mismo y sin órdenes perdidas.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
B=$(dirname "$IMG")
[ -f $B/VOLTIME.COM ] || { echo "falta $B/VOLTIME.COM (make emmprogs)"; exit 1; }
CHK="python3 tests/tools/ventanas_check.py"
T=$(mktemp -d)
trap 'rm -rf $T' EXIT
fail=0
P=${PARTES:-abcd}             # (para repetir solo una parte)
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; sleep "${2:-0.3}"; }
send() { printf '%s\r' "$1"; sleep "${2:-1.5}"; }
shot() { mon "screendump $T/$1.ppm" 0.6; }
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
run() {
  local tmo=$1; shift
  rm -f $T/mon $T/out.wav
  timeout $tmo qemu-system-i386 -display none -serial stdio -monitor unix:$T/mon,server,nowait \
    -parallel none -m 32 -drive file=$T/f.img,format=raw,if=floppy,index=0 -boot a -no-reboot \
    -audiodev wav,id=snd0,path=$T/out.wav "$@" > $T/raw 2>&1
  tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 | grep -av '^audio:' > $T/log.txt
}
nueva() {
  cp "$IMG" $T/f.img
  printf "DEVICE=A:\\\\HISPADOS\\\\HSPXMS.SYS\r\nFILES=30\r\n" > $T/CONFIG.SYS
  mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
  mcopy -o -i $T/f.img $B/VOLTIME.COM ::VOLTIME.COM
}
# mide ETIQUETA "condición en python" : la línea de VOLTIME que sigue a «ECHO
# ETIQUETA», con sus números como variables (llamadas, total_ms, media_us,
# max_us, ordenes, fallos, saltadas)
mide() {
  python3 - $T/log.txt "$1" "$2" "$3" <<'EOF' || fail=1
import re, sys
log = open(sys.argv[1], encoding='utf-8', errors='replace').read().split('\n')
et, cond, txt = sys.argv[2], sys.argv[3], sys.argv[4]
v = None
for i, l in enumerate(log):
    if l.strip() == et:
        for m in log[i + 1:]:
            if m.startswith('VOLTIME llamadas='):
                v = dict((k, int(x)) for k, x in re.findall(r'(\w+)=(\d+)', m))
                break
        break
if v is None:
    print("  FAIL %s: no sale VOLTIME" % txt); sys.exit(1)
bien = eval(cond, {}, v)
print("  %s %s (%s)" % ("OK  " if bien else "FAIL", txt,
      " ".join("%s=%d" % kv for kv in v.items())))
sys.exit(0 if bien else 1)
EOF
}

[[ $P == *a* ]] && {
echo "== (a) un cambio de volumen tarda poco (05h y teclas de volumen) =="
for CARD in hda hdaout ac97 sb16; do
  case $CARD in
    hda)    DEV="-device intel-hda -device hda-duplex,audiodev=snd0"; NAME="HD Audio (hda-duplex)"; ORD="ordenes == 2 * (llamadas + 1)" ;;
    hdaout) DEV="-device intel-hda -device hda-output,audiodev=snd0"; NAME="HD Audio (hda-output)"; ORD="ordenes == 2 * (llamadas + 1)" ;;
    ac97)   DEV="-device AC97,audiodev=snd0"; NAME="AC'97"; ORD="ordenes == 0" ;;
    sb16)   DEV="-device sb16,audiodev=snd0"; NAME="Sound Blaster 16"; ORD="ordenes == 0" ;;
  esac
  nueva
  { sleep 6
    send 'A:\HISPADOS\HSPSND' 4
    send 'ECHO uno' 0.5
    send 'VOLTIME 50' 6
    send 'ECHO dos' 0.5
    send 'VOLTIME 50 T' 6
    send 'A:\HISPADOS\HSPSND /INFO' 2
    mon quit
  } | run 40 $DEV
  mide uno "media_us < 3000 and max_us < 60000 and fallos == 0 and $ORD" "$NAME: 50 cambios con 05h"
  mide dos "media_us < 3000 and max_us < 60000 and fallos == 0 and $ORD" "$NAME: 50 teclas de volumen"
  if grep -q 'Volumen: 80 %' $T/log.txt; then ok "$NAME: el volumen, como estaba (80 %)"; else ko "$NAME: el volumen no ha vuelto al 80 %"; fi
  if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log.txt; fi
done

}
[[ $P == *b* ]] && {
echo "== (b) HD Audio: el nodo del volumen no contesta (/SIMSORDO:02) =="
nueva
{ sleep 6
  send "A:\\HISPADOS\\HSPSND /VOL:100 ${SORDO-/SIMSORDO:02}" 4
  send 'ECHO uno' 0.5
  send 'VOLTIME 50' 6
  send 'ECHO dos' 0.5
  send 'VOLTIME 50 T' 6
  send 'A:\HISPADOS\HSPSND /VOL:100' 2
  send 'SONAR /TONE 525 600' 3
  send 'A:\HISPADOS\HSPSND /VOL:30' 2
  send 'SONAR /TONE 735 600' 3
  mon quit
} | run 60 -device intel-hda -device hda-duplex,audiodev=snd0
mide uno "total_ms < 150 and fallos <= 2 and max_us < 60000" "50 cambios con un nodo que no contesta: unos milisegundos, no segundos"
mide dos "total_ms < 150 and fallos == 0" "50 teclas de volumen después: nada que esperar"
python3 - $T/out.wav <<'EOF' || fail=1
import struct, sys, wave
w = wave.open(sys.argv[1], 'rb')
ch, rate, n = w.getnchannels(), w.getframerate(), w.getnframes()
v = struct.unpack('<%dh' % (n * ch), w.readframes(n))
s = v[0::ch]
ven = rate // 20
tr = []
for i in range(0, n - ven, ven):
    a = s[i:i + ven]
    p = max(abs(x) for x in a)
    if p < 30:
        continue
    zc = sum(1 for k in range(1, len(a)) if (a[k - 1] < 0) != (a[k] < 0))
    tr.append((zc / 2 / (ven / rate), p))
# (QEMU graba solo mientras suena y los tonos salen pegados, con la
# frecuencia cambiada: se toman los dos tramos por orden, el primero el del 100 %)
g = []
for f, p in tr:
    if g and abs(f - g[-1][0]) < 0.06 * f:
        g[-1][1].append(p)
    else:
        g.append([f, [p]])
g = [sorted(ps)[len(ps) // 2] for f, ps in g if len(ps) >= 3]
a, b = (g + [0, 0])[:2]
bien = len(g) == 2 and a > 2000 and 0 < b and b * 2.5 < a
print("  %s el volumen sigue funcionando (por programa): al 30 %% más bajo que al 100 %% (picos %d y %d)" % ("OK  " if bien else "FAIL", a, b))
sys.exit(0 if bien else 1)
EOF
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log.txt; fi

}
[[ $P == *c* ]] && {
echo "== (c) Hispa Ventanas: arrastrar la barra del volumen con ese códec =="
nueva
RX=400; RY=300
{ sleep 6
  send 'KEYB SP' 2
  send 'A:\HISPADOS\HSPRATON' 2
  send 'A:\HISPADOS\HSPSND /SIMSORDO:02' 4
  send 'VENTANAS' 7
  clic 698 14; sleep 1
  # la barra: del 80 % (x = 570 + 2 * 80) al 20 % en 12 pasos
  ir 730 89
  mon "mouse_button 1" 0.1
  for i in 1 2 3 4 5 6 7 8 9 10 11 12; do mon "mouse_move -10 0" 0.07; done
  RX=610
  mon "mouse_button 0" 0.4
  shot v1
  mon "sendkey esc" 0.8
  clic 756 13; sleep 1; mon "sendkey ret" 3
  send 'A:\HISPADOS\HSPSND /INFO' 2
  mon quit
} | run 60 -vga std -device intel-hda -device hda-duplex,audiodev=snd0
if [ -f $T/v1.ppm ]; then
  $CHK $T/v1.ppm texto 568 63 0 8 fina "Volumen: 20 %" || fail=1
  convert $T/v1.ppm $B/volumen_arrastre.png 2>/dev/null; [ -n "${CAPTURAS:-}" ] && cp $B/volumen_arrastre.png "$CAPTURAS/"
else
  ko "sin captura de la ventanita del volumen"
fi
if grep -q 'Volumen: 20 %' $T/log.txt; then ok "HSPSND tiene el 20 % al acabar de arrastrar"; else ko "HSPSND no tiene el 20 %"; fi
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log.txt; fi

}
[[ $P == *d* ]] && {
echo "== (d) [TOSHIBA] HD Audio con el Realtek ALC269 del Toshiba NB250 (/SIMALC) =="
nueva
{ sleep 6
  send 'A:\HISPADOS\HSPSND /SIMALC /VOL:100' 4
  send 'ECHO cien' 0.5
  send 'SONAR /TONE 525 600' 3
  send 'A:\HISPADOS\HSPSND /VOL:50' 2
  send 'ECHO cincuenta' 0.5
  send 'SONAR /TONE 735 600' 3
  send 'A:\HISPADOS\HSPSND /VOL:10' 2
  send 'ECHO diez' 0.5
  send 'SONAR /TONE 1050 600' 3
  send 'ECHO teclas' 0.5
  send 'VOLTIME 20 T' 6
  send 'A:\HISPADOS\HSPSND /INFO' 3
  mon quit
} | run 80 -device intel-hda -device hda-duplex,audiodev=snd0
python3 - $T/log.txt $T/out.wav <<'EOF' || fail=1
import re, struct, sys, wave
log = open(sys.argv[1], encoding='utf-8', errors='replace').read().split('\n')
fail = 0
def res(bien, txt):
    global fail
    print("  %s %s" % ("OK  " if bien else "FAIL", txt))
    if not bien:
        fail = 1
# las órdenes de ganancia a la salida del DAC 02h tras cada «ECHO»
tramo, g = None, {}
for l in log:
    l = l.strip()
    if l in ('cien', 'cincuenta', 'diez', 'teclas'):
        tramo = l
        g[tramo] = []
    m = re.match(r'ALCSIM 0023([89AB])0([0-9A-F]{2})$', l)
    if m and tramo:
        g[tramo].append((int(m.group(1), 16), int(m.group(2), 16)))
todas = [v for t in g.values() for _, v in t]
res(todas and all((v & 0x7F) <= 87 for v in todas), "todas las ganancias del DAC dentro de 0..87 (%d órdenes)" % len(todas))
for t, pc in (('cien', 100), ('cincuenta', 50), ('diez', 10)):
    o = g.get(t, [])[:2]            # (las de SONAR: el volumen de ese momento)
    lados = set(k & 3 for k, _ in o)            # A: izquierdo, 9: derecho
    v = [x for _, x in o]
    ok = len(o) >= 2 and lados == {1, 2} and len(set(v)) == 1 and not (v[0] & 0x80)
    att = (87 - v[0]) * 0.75 if ok else -1
    nom = (100 - pc) * 15 / 32
    res(ok and abs(att - nom) <= 0.75, "al %d %%: paso %s a los dos lados = %.2f dB menos (la curva: %.2f dB)" % (pc, v[0] if v else '-', att, nom))
res(len(g.get('teclas', [])) >= 4, "las teclas de volumen mandan la ganancia al DAC 02h (%d órdenes)" % len(g.get('teclas', [])))
for l in log:
    if l.startswith('VOLTIME llamadas='):
        d = dict((k, int(x)) for k, x in re.findall(r'(\w+)=(\d+)', l))
        res(d.get('fallos', 1) == 0 and d.get('media_us', 99999) < 3000, "teclas: ninguna orden sin respuesta y rápidas (%s)" % l[8:])
res(any('Amplificador de volumen: nodo 02 (salida), 0 dB=57 de 57 pasos de 0,75 dB' in l for l in log), "HSPSND /INFO: el amplificador del DAC, 87 pasos de 0,75 dB")
# la grabación: los tres tonos (525, 735 y 1050 Hz), cada vez más bajos
w = wave.open(sys.argv[2], 'rb')
ch, rate, n = w.getnchannels(), w.getframerate(), w.getnframes()
s = struct.unpack('<%dh' % (n * ch), w.readframes(n))[0::ch]
ven = rate // 20
tr = []
for i in range(0, n - ven, ven):
    a = s[i:i + ven]
    p = max(abs(x) for x in a)
    if p < 20:
        continue
    zc = sum(1 for k in range(1, len(a)) if (a[k - 1] < 0) != (a[k] < 0))
    tr.append((zc / 2 / (ven / rate), p))
niv = {}
for f0 in (525, 735, 1050):
    ps = sorted(p for f, p in tr if abs(f - f0) < 0.06 * f0)
    niv[f0] = ps[len(ps) // 2] if len(ps) >= 3 else 0
a, b, c = niv[525], niv[735], niv[1050]
res(a > 2000 and 0 < b < a / 5 and 0 < c < b / 2,
    "en QEMU el tono baja de verdad: picos %d (100 %%), %d (50 %%), %d (10 %%)" % (a, b, c))
sys.exit(fail)
EOF
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log.txt; fi

}
[ $fail -eq 0 ] && echo "== volumen: todo bien ==" || echo "== volumen: HAY FALLOS =="
exit $fail
