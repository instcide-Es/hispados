#!/bin/bash
# Prueba del mezclador por fuentes: HSPSND (funciones 09h-0Fh, versión 4), el
# Mezclador de Hispa Ventanas (MEZCLAD.HVP), Configuración > Sonido y el
# guardado en \HISPADOS\SONIDO.INI (docs/MEZCLADOR.md).
#  (a) Sound Blaster 16: en Hispa Ventanas se abre el Mezclador con doble clic
#      en el altavoz de la barra y se mueven deslizadores y casillas con el
#      teclado y con el ratón; luego MEZTEST (tests/progs/meztest.asm) lee los
#      registros 30h-47h del CT1745 y se comparan con lo que debe salir de
#      cada fuente (la misma cuenta que HSPSND: curva en dB, balance, casillas
#      de 3Ch-3Eh, ganancias, ajuste del micrófono y tonos). También con las
#      órdenes de MEZTEST (la interfaz, sin ventanas).
#  (b) Intel HD Audio y AC'97: los volúmenes por fuente se miden en la
#      grabación de QEMU: Sonidos al 30 % suena más bajo que al 100 %; con el
#      balance a la izquierda, el canal derecho calla; Sonidos y General en
#      silencio no suenan. Con la Sound Blaster emulada (HSPSND /SB sobre
#      HSPEMM), la Música en silencio deja sin sonar la FM de SBTEST y al
#      quitarlo suena.
#  (c) Lo del Mezclador queda en SONIDO.INI ([Mezclador]) y, al arrancar otra
#      vez, HSPSND lo pone (MEZTEST lo lee, y los registros del CT1745); con
#      /VOL:n manda la orden.
#  (d) La SB emulada: lo que un juego escribe en su mezclador (MEZTEST W..,
#      SBTEST) se queda en el mezclador emulado (MEZTEST P) y no cambia lo del
#      usuario (MEZTEST), que sigue mandando.
#  Y Configuración > Sonido: la tarjeta, «Sound Blaster para juegos» y el
#  botón «Mezclador...». Capturas en build/mezclador_*.png (con CAPTURAS=dir,
#  también ahí).
# Per-source mixer test (HSPSND API 09h-0Fh, MEZCLAD.HVP, SONIDO.INI).
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
B=$(dirname "$IMG")
for f in MEZTEST.COM SBTEST.COM; do
  [ -f $B/$f ] || { echo "falta $B/$f (make emmprogs)"; exit 1; }
done
CHK="python3 tests/tools/ventanas_check.py"
T=$(mktemp -d)
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() { if grep -qF -- "$1" $T/log.txt; then ok "$1"; else ko "$1"; fi; }
chk() { $CHK "$@" || fail=1; }
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; sleep "${2:-0.3}"; }
tecla() { local k; for k in "$@"; do mon "sendkey $k" 0.3; done; }
shot() { sleep 0.4; mon "screendump $T/$1.ppm" 0.6; }
send() { printf '%s\r' "$1"; sleep "${2:-1.5}"; }
muestra() { [ -f $T/$1.ppm ] && { convert $T/$1.ppm $B/mezclador_$2.png; [ -n "${CAPTURAS:-}" ] && mkdir -p "$CAPTURAS" && cp $B/mezclador_$2.png "$CAPTURAS/"; }; }
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
run() {
  local tmo=$1; shift
  rm -f $T/mon $T/out.wav
  timeout $tmo qemu-system-i386 -display none -serial stdio -monitor unix:$T/mon,server,nowait \
    -parallel none -m 32 -drive file=$T/f.img,format=raw,if=floppy,index=0 -boot a -no-reboot \
    -audiodev wav,id=snd0,path=$T/out.wav "$@" > $T/raw 2>&1
  tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 | grep -v '^audio:' > $T/log.txt
  mtype -i $T/f.img ::HISPADOS/SONIDO.INI 2>/dev/null | tr -d '\r' | iconv -f CP437 -t UTF-8 > $T/ini.txt
}
nueva() {
  cp "$IMG" $T/f.img
  mcopy -o -i $T/f.img $B/MEZTEST.COM ::MEZTEST.COM
  mcopy -o -i $T/f.img $B/SBTEST.COM ::SBTEST.COM
  printf "${1:-DEVICE=A:\\\\HISPADOS\\\\HSPXMS.SYS\\r\\nFILES=30\\r\\n}" > $T/CONFIG.SYS
  mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
}
# ct ETIQUETA: el último «CT ...» del registro (desde «ECHO ETIQUETA») tiene que
# ser lo que sale de las fuentes y ajustes que dice MEZTEST justo después
ct() {
  python3 - $T/log.txt "$1" "${2:-}" <<'EOF' || fail=1
import re, sys
log = open(sys.argv[1], encoding='utf-8', errors='replace').read()
i = log.find('ECHO ' + sys.argv[2])
if i < 0:
    print("  FAIL no está la parte «%s»" % sys.argv[2]); sys.exit(1)
log = log[i:]
m = re.search(r'^CT((?: [0-9A-F]{2}=[0-9A-F]{2})+)\s*$', log, re.M)
f = re.findall(r'^F(\d) vol=(\d+) cas=(\d+) bal=(\d+)', log, re.M)[:7]
a = re.search(r'^AJ 0=(\d+) 1=(\d+) 2=(\d+) 3=(\d+) 4=(\d+) 5=(\d+)', log, re.M)
if not (m and len(f) == 7 and a):
    print("  FAIL «%s»: no están los registros o el estado" % sys.argv[2]); sys.exit(1)
reg = {int(k, 16): int(v, 16) for k, v in re.findall(r'([0-9A-F]{2})=([0-9A-F]{2})', m.group(1))}
vol = {int(n): (int(v), int(c), int(b)) for n, v, c, b in f}
aj = [int(x) for x in a.groups()]
def q(src, lado):
    v, c, b = vol[src]
    if c & 1 or v == 0:
        return None
    if src < 5:
        if lado == 0 and b > 50:
            v = v * (100 - b) // 50
        if lado == 1 and b < 50:
            v = v * b // 50
    if v == 0:
        return None
    return (100 - v) * 15 // 8
def ct5(x):
    return 0 if x is None else max(0, 31 - (x + 4) // 8) << 3
quiero = {}
for s in range(5):
    quiero[0x30 + 2 * s] = ct5(q(s, 0))
    quiero[0x31 + 2 * s] = ct5(q(s, 1))
quiero[0x3A] = ct5(q(5, 0))
quiero[0x3B] = 0 if q(6, 0) is None else ((vol[6][0] * 3 + 50) // 100) << 6
def cas(s):
    return 0 if vol[s][1] & 1 else vol[s][1]
quiero[0x3C] = (1 if cas(5) & 2 else 0) | (6 if cas(3) & 2 else 0) | (0x18 if cas(4) & 2 else 0)
quiero[0x3D] = (1 if cas(5) & 4 else 0) | (4 if cas(3) & 4 else 0) | (0x10 if cas(4) & 4 else 0) | (0x40 if cas(2) & 4 else 0)
quiero[0x3E] = (1 if cas(5) & 4 else 0) | (2 if cas(3) & 4 else 0) | (8 if cas(4) & 4 else 0) | (0x20 if cas(2) & 4 else 0)
quiero[0x3F] = quiero[0x40] = aj[2] << 6
quiero[0x41] = quiero[0x42] = aj[3] << 6
quiero[0x43] = 0 if aj[4] else 1
quiero[0x44] = quiero[0x45] = ((aj[1] * 15 + 50) // 100) << 4
quiero[0x46] = quiero[0x47] = ((aj[0] * 15 + 50) // 100) << 4
mal = ["%02X=%02X (debía ser %02X)" % (k, reg.get(k, -1), v) for k, v in sorted(quiero.items()) if reg.get(k) != v]
if mal:
    print("  FAIL «%s»: registros del CT1745 que no cuadran: %s" % (sys.argv[2], ", ".join(mal))); sys.exit(1)
print("  OK   «%s»: los 24 registros del CT1745 (30h-47h) son los de las fuentes y ajustes%s" % (sys.argv[2], sys.argv[3]))
EOF
}
# fuente ETIQUETA "F1 vol=70 cas=0 bal=60" ...: el estado que dice MEZTEST después de la etiqueta
estado() {
  local et=$1; shift
  local parte; parte=$(awk -v e="ECHO $et" 'index($0, e) {f=1} f' $T/log.txt)
  for w in "$@"; do
    if grep -qF -- "$w" <<< "$parte"; then ok "$et: $w"; else ko "$et: $w"; fi
  done
}
ini() { if grep -qxF -- "$1" $T/ini.txt; then ok "SONIDO.INI: $1"; else ko "SONIDO.INI sin la línea «$1»"; fi; }

echo "== Mezclador: los archivos del disco =="
for f in PROGS/MEZCLAD.HVP PROGS/SONIDOS.HVP; do
  if mdir -i "$IMG" ::HISPADOS/$f > /dev/null 2>&1; then ok "$f está en \\HISPADOS del disquete"; else ko "falta $f"; fi
  if [ -f $B/hispados-usb.img ]; then
    if MTOOLS_SKIP_CHECK=1 mdir -i $B/hispados-usb.img@@1048576 ::HISPADOS/$f > /dev/null 2>&1; then ok "y en la imagen USB"; else ko "falta $f en la imagen USB"; fi
  fi
done
python3 - $B/MEZCLAD.HVP $B/SONIDOS.HVP <<'EOF' || fail=1
import struct, sys
bien = True
for r in sys.argv[1:]:
    d = open(r, 'rb').read(64)
    g = struct.unpack_from('<I', d, 28)[0]
    n = d[32:64].split(b'\0')[0].decode('cp437')
    print("  %s %s: «%s», grupo %d (Sonido)" % ("OK  " if g == 2 else "FAIL", r, n, g))
    bien = bien and g == 2
sys.exit(0 if bien else 1)
EOF

echo "== (a) Sound Blaster 16: el Mezclador mueve los registros del CT1745 =="
nueva
RX=400; RY=300
{ sleep 6
  send 'KEYB SP' 2
  send 'A:\HISPADOS\HSPRATON' 2
  send 'A:\HISPADOS\HSPSND' 3
  send 'ECHO uno' 0.5
  send 'MEZTEST C' 2
  send 'VENTANAS' 7
  # el volumen de la barra: el botón «Mezclador...»
  clic 698 14; sleep 1
  aparca
  shot v1
  clic 698 14; sleep 1
  # doble clic en el altavoz: el Mezclador, en la vista Completa
  doble 698 14; sleep 3.5
  aparca
  shot m1
  # con el teclado: General -15; Sonidos -20 y el balance a la derecha;
  # Música en silencio (M); CD a grabar; Línea a sonar
  tecla down down down; sleep 0.3
  tecla tab tab down down down down right right; sleep 0.3
  tecla tab tab m; sleep 0.3
  tecla tab tab tab tab tab tab spc; sleep 0.3
  tecla tab tab tab spc; sleep 0.5
  # con el ratón: el Altavoz del PC, un clic a un cuarto de la guía (75 %)
  clic 666 208; sleep 1
  aparca
  shot m2
  # Avanzado: la ganancia de entrada a x2 y sin el ajuste automático
  tecla alt-v; sleep 0.5; tecla down down ret; sleep 2
  aparca
  shot m3
  tecla spc; sleep 0.5
  tecla tab tab spc; sleep 0.5
  shot m4
  tecla esc; sleep 1
  # Cómo se usa: el primer paso y el del balance
  tecla alt-y; sleep 0.5; tecla ret; sleep 2
  aparca
  shot t1
  tecla ret ret ret; sleep 1.5
  shot t4
  tecla esc; sleep 1
  # la vista sencilla y otra vez la completa
  tecla alt-v; sleep 0.5; tecla ret; sleep 2
  aparca
  shot s1
  tecla alt-v; sleep 0.5; tecla down ret; sleep 2
  sleep 2
  tecla esc; sleep 1.5
  # Configuración > Sonido
  doble 102 129; sleep 1.5
  tecla right right right right ret; sleep 2.5
  clic 196 264; sleep 1.5
  aparca
  shot c1
  tecla esc; sleep 1
  clic 756 13; sleep 1; tecla ret; sleep 3
  send 'ECHO dos' 0.5
  send 'MEZTEST C' 2
  send 'ECHO tres' 0.5
  send 'MEZTEST F C' 2
  send 'ECHO cuatro' 0.5
  send 'MEZTEST V0:33 B0:90 V3:0 G4:1 S5:1 V6:10 A0:0 A1:100 A3:3 A4:1 C' 2
  send 'ECHO cinco' 0.5
  send 'A:\HISPADOS\HSPSND /INFO' 2
  mon quit
} | run 200 -vga std -device sb16,audiodev=snd0
cp $T/ini.txt $T/ini_a.txt
ct uno " (de fábrica)"
estado uno "MEZ versión 4 tarjeta 2 fuentes=7F tarjeta=7F ajustes=00BF grabar=3C sonar=38" \
  "F0 vol=80 cas=0 bal=50" "F1 vol=90 cas=0 bal=50" "F2 vol=85 cas=0 bal=50" "F3 vol=85 cas=2 bal=50" \
  "F4 vol=85 cas=0 bal=50" "F5 vol=85 cas=4 bal=50" "F6 vol=50 cas=0 bal=50" "AJ 0=50 1=50 2=0 3=0 4=1 5=0"
ct dos " (lo movido en el Mezclador)"
estado dos "F0 vol=65 cas=0 bal=50" "F1 vol=70 cas=0 bal=60" "F2 vol=85 cas=1 bal=50" "F3 vol=85 cas=6 bal=50" \
  "F4 vol=85 cas=2 bal=50" "F5 vol=85 cas=4 bal=50" "F6 vol=75 cas=0 bal=50" "AJ 0=50 1=50 2=1 3=0 4=0 5=0" \
  "CT 30=B8 31=B8 32=A8 33=C0 34=00 35=00 36=D8 37=D8 38=D8 39=D8 3A=D8 3B=80 3C=1E 3D=05 3E=03 3F=40 40=40"
ct tres " (valores de fábrica, 0Eh)"
estado tres "F0 vol=80 cas=0 bal=50" "F2 vol=85 cas=0 bal=50" "AJ 0=50 1=50 2=0 3=0 4=1 5=0"
ct cuatro " (órdenes de MEZTEST)"
estado cuatro "F0 vol=33 cas=0 bal=90" "F3 vol=0 cas=2 bal=50" "F4 vol=85 cas=4 bal=50" "F5 vol=85 cas=6 bal=50" "AJ 0=0 1=100 2=0 3=3 4=1 5=0"
estado cinco "Mezclador: General 33 %, Sonidos 90 %, Música 85 %, CD 0 % suena," \
  "Línea 85 % graba," "Micrófono 85 % suena graba, Altavoz 10 %"
chk $T/v1.ppm titulo 550 34 activa "Volumen" texto 568 63 0 8 fina "Volumen: 80 %" texto 621 147 0 6 fina "Mezclador..."
chk $T/m1.ppm titulo 86 81 activa "Mezclador" texto 110 136 0 8 negrita "General" texto 394 136 0 8 negrita "CD" \
  texto 96 446 0 8 fina "Esta tarjeta lo tiene todo: volumen, balance y casillas por fuente."
chk $T/m2.ppm texto 122 156 0 8 fina "65 %" texto 210 156 0 8 fina "70 %" texto 650 156 0 8 fina "75 %" \
  texto 298 156 7 8 fina "85 %" px 276 346 4 "«Silencio» de Música, marcada"
chk $T/m3.ppm titulo 126 141 activa "Avanzado" texto 198 181 0 6 fina "Ganancia de entrada: x1"
chk $T/m4.ppm texto 198 181 0 6 fina "Ganancia de entrada: x2"
chk $T/t1.ppm texto 402 146 0 8 negrita "Paso 1 de 8: Qué es"
chk $T/t4.ppm texto 402 146 0 8 negrita "Paso 4 de 8: El balance" px 97 306 4 "el marco del recorrido, en el balance de General"
chk $T/s1.ppm texto 192 138 0 8 fina "Vista sencilla: solo el"
chk $T/c1.ppm titulo 124 110 activa "Configuración" texto 278 313 0 8 negrita "Tarjeta: Sound Blaster 16" \
  texto 278 337 0 8 negrita "Sound Blaster para juegos: es la propia tarjeta" texto 534 237 0 6 fina "Mezclador..."
muestra v1 volumen_barra; muestra m1 sb16; muestra m2 sb16_movido; muestra m4 avanzado; muestra t1 como_se_usa
muestra t4 como_se_usa_balance; muestra s1 sencilla; muestra c1 configuracion_sonido

echo "== (c) SONIDO.INI: se guarda solo y HSPSND lo pone al arrancar otra vez =="
cp $T/ini_a.txt $T/ini.txt
for l in "[Mezclador]" "General=65,50" "Sonidos=70,60" "Musica=85,50" "Altavoz=75,50" "Silencio=Musica" \
         "Sonar=CD,Linea" "Grabar=CD,Microfono" "GananciaEntrada=1" "AjusteMicrofono=0" "Vista=completa"; do ini "$l"; done
{ sleep 6
  send 'A:\HISPADOS\HSPSND' 3
  send 'ECHO seis' 0.5
  send 'MEZTEST C' 2
  send 'ECHO siete' 0.5
  send 'A:\HISPADOS\HSPSND /INFO' 2
  mon quit
} | run 30 -device sb16,audiodev=snd0
ct seis " (al arrancar, de SONIDO.INI)"
estado seis "F0 vol=65 cas=0 bal=50" "F1 vol=70 cas=0 bal=60" "F2 vol=85 cas=1 bal=50" "F3 vol=85 cas=6 bal=50" \
  "F6 vol=75 cas=0 bal=50" "AJ 0=50 1=50 2=1 3=0 4=0 5=0" "cambios=0"
estado siete "Volumen: 65 %" "Mezclador: General 65 %, Sonidos 70 %, Música 85 % (silencio), CD 85 % suena graba"
# /VOL:n manda sobre General; y AUDICION puede haber puesto sus secciones antes
python3 - $T/f.img <<'EOF'
import subprocess, sys
ini = subprocess.run(['mtype', '-i', sys.argv[1], '::HISPADOS/SONIDO.INI'], capture_output=True).stdout
nuevo = b'; prueba\r\n[HDA 8086:2668 1AF4:1100 1AF4:0022 1AF4:0022]\r\nEstado=SUENA\r\nRuta=03h(0) 02h\r\n\r\n' + ini
open('/dev/stdout', 'wb')
p = sys.argv[1] + '.ini'
open(p, 'wb').write(nuevo)
subprocess.run(['mcopy', '-o', '-i', sys.argv[1], p, '::HISPADOS/SONIDO.INI'])
EOF
{ sleep 6
  send 'A:\HISPADOS\HSPSND /VOL:40' 3
  send 'ECHO ocho' 0.5
  send 'MEZTEST' 2
  mon quit
} | run 25 -device sb16,audiodev=snd0
estado ocho "F0 vol=40 cas=0 bal=50" "F1 vol=70 cas=0 bal=60" "F3 vol=85 cas=6 bal=50"
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log.txt; cat $T/ini_a.txt; fi

for CARD in hda ac97; do
  case $CARD in
    hda)  DEV="-device intel-hda -device hda-duplex,audiodev=snd0"; NAME="Intel HD Audio"; FU="fuentes=03 tarjeta=01" ;;
    ac97) DEV="-device AC97,audiodev=snd0"; NAME="Intel AC'97"; FU="fuentes=03 tarjeta=03" ;;
  esac
  echo "== (b) $NAME: el volumen de cada fuente, medido en la grabación =="
  nueva
  { sleep 6
    send 'A:\HISPADOS\HSPSND /VOL:100' 3
    send 'ECHO uno' 0.5
    send 'MEZTEST' 2
    send 'SONAR /TONE 441 300' 3
    send 'MEZTEST V1:100' 2
    send 'SONAR /TONE 525 500' 3
    send 'MEZTEST V1:30' 2
    send 'SONAR /TONE 735 500' 3
    send 'MEZTEST V1:100 B1:0' 2
    send 'SONAR /TONE 980 500' 3
    send 'MEZTEST B1:50 M1:1' 2
    send 'SONAR /TONE 1225 500' 3
    send 'MEZTEST M1:0 M0:1' 2
    send 'SONAR /TONE 1400 500' 3
    send 'MEZTEST M0:0' 2
    send 'SONAR /TONE 1575 500' 3
    mon quit
  } | run 70 $DEV
  estado uno "MEZ versión 4 tarjeta" "$FU"
  # (QEMU solo graba mientras suena: con HD Audio los tonos salen pegados.
  # Se miden por tramos de la misma frecuencia, con el pico de cada canal)
  python3 - $T/out.wav $CARD > $T/audio.txt <<'EOF' || fail=1
import struct, sys, wave
w = wave.open(sys.argv[1], 'rb')
ch, rate, n = w.getnchannels(), w.getframerate(), w.getnframes()
v = struct.unpack('<%dh' % (n * ch), w.readframes(n))
izq, der = v[0::ch], (v[1::ch] if ch > 1 else v[0::ch])
ven = rate // 20
tr = []
for i in range(0, n - ven, ven):
    a, b = izq[i:i + ven], der[i:i + ven]
    pi, pd = max(abs(x) for x in a), max(abs(x) for x in b)
    s = a if pi >= pd else b
    if max(pi, pd) < 60:
        tr.append(None)
        continue
    zc = sum(1 for k in range(1, len(s)) if (s[k - 1] < 0) != (s[k] < 0))
    tr.append((zc / 2 / (ven / rate), pi, pd))
g = []
for t in tr:
    if t and g and abs(t[0] - g[-1][0]) < 0.06 * t[0]:
        g[-1][1].append(t)
    elif t:
        g.append([t[0], [t]])
# cada tramo: su frecuencia, lo que dura y la mediana de los picos de cada
# canal (en el paso de un tono al siguiente, una ventana puede llevar lo
# del anterior)
r = []
for f, ts in g:
    if len(ts) >= 4:                            # 0,2 s o más
        pi = sorted(x[1] for x in ts)[len(ts) // 2]
        pd = sorted(x[2] for x in ts)[len(ts) // 2]
        r.append([f, len(ts), pi, pd])
for x in r:
    print("tramo: %.0f Hz, %.2f s, pico I %d D %d" % (x[0], x[1] / 20, x[2], x[3]))
def de(hz):
    m = [(0, 0, 0, x[2], x[3]) for x in r if abs(x[0] - hz) < hz * 0.07]
    return m[0] if m else None
bien = True
def mira(c, t):
    global bien
    print("  %s %s" % ("OK  " if c else "FAIL", t)); bien = bien and bool(c)
a, b = de(525), de(735)
pa = max(a[3], a[4]) if a else 0
pb = max(b[3], b[4]) if b else 0
mira(pa > 2000 and pb * 2.5 < pa, "Sonidos al 30 %% suena más bajo que al 100 %% (picos %d y %d)" % (pa, pb))
c = de(980)
mira(c and c[3] > 2000 and c[4] * 10 < c[3], "balance de Sonidos a la izquierda: el derecho calla (picos I %d D %d)" % (c[3], c[4]) if c else "balance: no suena el tono de 980 Hz")
mira(de(1225) is None, "Sonidos en silencio: no suena (1225 Hz)")
mira(de(1400) is None, "General en silencio: no suena (1400 Hz)")
d = de(1575)
mira(d and max(d[3], d[4]) > 2000, "al quitar el silencio, suena otra vez (1575 Hz)")
sys.exit(0 if bien else 1)
EOF
  grep -v '^tramo' $T/audio.txt
  if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log.txt; cat $T/audio.txt; fi

  echo "== (b) y (d) $NAME con la Sound Blaster emulada: la Música (FM) y lo que escribe el juego =="
  nueva 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nDEVICE=A:\\HISPADOS\\HSPEMM.SYS\r\nDOS=UMB\r\n'
  { sleep 6
    send 'A:\HISPADOS\HSPSND /SB' 4
    send 'ECHO uno' 0.5
    send 'MEZTEST M2:1 W30:F8 W31:F8 W34:F8 W35:F8 W22:FF' 2.5
    send 'A:\SBTEST' 12
    send 'ECHO dos' 0.5
    send 'MEZTEST W30:00 W31:00 P' 2.5
    send 'ECHO tres' 0.5
    send 'MEZTEST M2:0 W30:F8 W31:F8' 2.5
    send 'A:\SBTEST' 12
    mon quit
  } | run 70 $DEV
  estado uno "fuentes=07" "F0 vol=80 cas=0 bal=50" "F2 vol=85 cas=1 bal=50"
  estado dos "SBP 22=00" "30=00 31=00" "34=F8 35=F8" "F0 vol=80 cas=0 bal=50" "F1 vol=90 cas=0 bal=50" "F2 vol=85 cas=1 bal=50"
  estado tres "F2 vol=85 cas=0 bal=50"
  python3 tests/tools/audio.py check $T/out.wav > $T/audio.txt 2>&1
  python3 - $T/audio.txt <<'EOF' || fail=1
import re, sys
tr = [(float(a), float(b), float(h), int(p)) for a, b, h, p in
      re.findall(r'tramo ([\d.]+)-([\d.]+) s: (\d+) Hz, pico (\d+)', open(sys.argv[1]).read())]
d = [t for t in tr if abs(t[2] - 498) < 20 and t[1] - t[0] > 0.3]
fm = [t for t in tr if abs(t[2] - 600) < 20 and t[1] - t[0] > 0.3]
bien = True
def mira(c, t):
    global bien
    print("  %s %s" % ("OK  " if c else "FAIL", t)); bien = bien and bool(c)
mira(len(d) == 2, "las dos veces suena el DSP de SBTEST (500 Hz: %d)" % len(d))
if len(d) == 2:
    mira(not [t for t in fm if t[0] < d[1][0]], "con la Música en silencio, la FM de SBTEST no suena (aunque el juego la ponga al máximo)")
    mira([t for t in fm if t[0] > d[1][0]], "sin el silencio, la FM suena")
sys.exit(0 if bien else 1)
EOF
  if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log.txt; cat $T/audio.txt; fi
done

echo "== El Mezclador con Intel HD Audio: lo que no tiene, dicho =="
nueva
RX=400; RY=300
{ sleep 6
  send 'KEYB SP' 2
  send 'A:\HISPADOS\HSPRATON' 2
  send 'A:\HISPADOS\HSPSND' 3
  send 'VENTANAS' 7
  doble 414 129; sleep 1.5
  aparca
  shot g1
  tecla right ret; sleep 3
  aparca
  shot h1
  tecla esc; sleep 1
  clic 756 13; sleep 1; tecla ret; sleep 3
  mon quit
} | run 80 -vga std -device intel-hda -device hda-duplex,audiodev=snd0
chk $T/g1.ppm marco 272 212 452 190 activa titulo 272 212 activa "Sonido"
chk $T/h1.ppm texto 130 174 0 8 negrita "General" texto 218 174 0 8 negrita "Sonidos" \
  texto 116 408 0 8 fina "CD, Línea y Micrófono no salen: la tarjeta no tiene esas entradas." \
  texto 116 426 0 8 fina "Música (FM) solo sale con la Sound Blaster para juegos (HSPSND /SB)." \
  texto 116 444 0 8 fina "Graves y agudos no salen:"
muestra g1 grupo_sonido; muestra h1 hda
if grep -qF 'Nombre=Mezclador' <(mtype -i $T/f.img ::HISPADOS/CENTRO.INI 2>/dev/null | tr -d '\r'); then
  ok "CENTRO.INI: el Mezclador, en el grupo Sonido"
else
  ko "CENTRO.INI: el Mezclador, en el grupo Sonido"
fi
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log.txt; fi
rm -rf $T
exit $fail
