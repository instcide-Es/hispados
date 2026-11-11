#!/bin/bash
# Prueba de sonido: QEMU con una tarjeta AC'97 que graba su salida en un
# .wav; HSPSND + PLAYWAV tocan un tono de 440 Hz y un WAV de 1000 Hz, y se
# comprueba la grabación (que suenen los dos tramos con su frecuencia).
# Sound test: AC'97 output is recorded and analysed.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
cp "$IMG" $T/f.img
python3 tests/tools/audio.py make $T/TEST.WAV 1000 700 22050 8 1
mcopy -o -i $T/f.img $T/TEST.WAV ::TEST.WAV
send() { sleep "${2:-1.5}"; printf '%s\r' "$1"; }
{
  sleep 4
  send "A:\\DRIVERS\\HSPSND" 2
  send "PLAYWAV /TONE 440 800" 3
  send "PLAYWAV TEST.WAV" 3
  send "A:\\DRIVERS\\HSPSND" 2
  sleep 1
} | timeout 30 qemu-system-i386 -display none -serial stdio -monitor none \
      -audiodev wav,id=snd0,path=$T/out.wav -device AC97,audiodev=snd0 \
      -drive file=$T/f.img,format=raw,if=floppy -boot a -no-reboot > $T/log 2>&1
tr -d '\r' < $T/log > $T/log.txt
python3 tests/tools/audio.py check $T/out.wav > $T/audio.txt 2>&1
fail=0
check() {
  if grep -qF -- "$2" "$1"; then echo "  OK   $2"; else echo "  FAIL $2"; fail=1; fi
}
echo "== HispaDOS sound tests (AC'97) =="
check $T/log.txt "Intel AC'97 audio at I/O"
check $T/log.txt "Tone 440 Hz on Intel AC'97"
check $T/log.txt "Playing TEST.WAV (22050 Hz, 8-bit mono) on Intel AC'97"
check $T/log.txt "Already installed."
# frecuencias medidas en la grabación (con un margen del 5 %)
python3 - $T/audio.txt <<'EOF' || fail=1
import re, sys
txt = open(sys.argv[1]).read()
hz = [float(m) for m in re.findall(r'tramo [\d.]+-[\d.]+ s: (\d+) Hz', txt)]
ok = True
for want in (440, 1000):
    if any(abs(h - want) / want < 0.05 for h in hz):
        print("  OK   suena %d Hz en la grabación" % want)
    else:
        print("  FAIL no suena %d Hz (medido: %s)" % (want, hz)); ok = False
sys.exit(0 if ok else 1)
EOF
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then echo "--- salida ---"; cat $T/log.txt; cat $T/audio.txt; fi
rm -rf $T
exit $fail
