#!/bin/bash
# Prueba de sonido: QEMU graba en un .wav lo que sale por la tarjeta; HSPSND +
# PLAYWAV tocan un tono de 440 Hz y un WAV de 1000 Hz y se mide la grabación.
# Se prueba con Intel HD Audio y con Intel AC'97.
# Sound test with HD Audio and AC'97: the card output is recorded and analysed.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
fail=0
for CARD in hda ac97; do
  T=$(mktemp -d)
  cp "$IMG" $T/f.img
  python3 tests/tools/audio.py make $T/TEST.WAV 1000 700 22050 8 1
  mcopy -o -i $T/f.img $T/TEST.WAV ::TEST.WAV
  if [ $CARD = hda ]; then
    DEV="-device intel-hda -device hda-output,audiodev=snd0"; NAME="Intel HD Audio"
  else
    DEV="-device AC97,audiodev=snd0"; NAME="Intel AC'97"
  fi
  {
    sleep 4
    printf 'A:\\HISPADOS\\HSPSND\r'; sleep 3
    printf 'PLAYWAV /TONE 440 800\r'; sleep 4
    printf 'PLAYWAV TEST.WAV\r'; sleep 4
    printf 'A:\\HISPADOS\\HSPSND\r'; sleep 2
  } | timeout 30 qemu-system-i386 -display none -serial stdio -monitor none \
        -audiodev wav,id=snd0,path=$T/out.wav $DEV \
        -drive file=$T/f.img,format=raw,if=floppy -boot a -no-reboot > $T/log 2>&1
  tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 | grep -v '^audio:' > $T/log.txt
  python3 tests/tools/audio.py check $T/out.wav > $T/audio.txt 2>&1
  echo "== Hispa-DOS sound tests ($NAME) =="
  for want in "$NAME" "Tono 440 Hz en $NAME" "Reproduciendo TEST.WAV (22050 Hz, 8 bits, mono) en $NAME" "Ya está instalado."; do
    if grep -qF -- "$want" $T/log.txt; then echo "  OK   $want"; else echo "  FAIL $want"; fail=1; fi
  done
  python3 - $T/audio.txt <<'EOF' || fail=1
import re, sys
txt = open(sys.argv[1]).read()
tr = [(float(a), float(b), float(h)) for a, b, h in re.findall(r'tramo ([\d.]+)-([\d.]+) s: (\d+) Hz', txt)]
ok = True
for want, dur in ((440, 0.8), (1000, 0.7)):
    m = [t for t in tr if abs(t[2] - want) / want < 0.05]
    if m and abs((m[0][1] - m[0][0]) - dur) < 0.2:
        print("  OK   suena %d Hz durante %.2f s" % (want, m[0][1] - m[0][0]))
    else:
        print("  FAIL no suena %d Hz unos %.1f s (tramos: %s)" % (want, dur, tr)); ok = False
sys.exit(0 if ok else 1)
EOF
  if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log.txt; cat $T/audio.txt; fi
  rm -rf $T
done
exit $fail
