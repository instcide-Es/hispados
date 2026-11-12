#!/bin/bash
# HSPSND con Sound Blaster cargado en 16 direcciones distintas (un disco RAM de
# 16 a 31 KB delante lo desplaza de KB en KB). Los bloques de DMA se alinean a
# 16 KB a partir del final de la parte residente, así que en algunas
# direcciones caen encima del código de instalación: no debe pisarse mientras
# aún se ejecuta. También: DSP 1.05 (sin autoinicio) y 2.01 con la IRQ 7
# sin variable BLASTER (la IRQ se pregunta a la tarjeta).
# Sound Blaster driver loaded at 16 different addresses; old DSPs; IRQ probe.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
fail=0
T=$(mktemp -d)
echo "== Hispa-DOS HSPSND: Sound Blaster en 16 direcciones de carga =="
bad=""
for KB in $(seq 16 31); do
  cp "$IMG" $T/f.img
  printf 'DEVICE=A:\\HISPADOS\\HSPRAM.SYS %d\r\n' $KB > $T/CONFIG.SYS
  mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
  {
    sleep 4
    printf 'A:\\HISPADOS\\HSPSND\r'; sleep 2
    printf 'ECHO sigue-vivo-%d\r' $KB; sleep 1
    printf 'SET\r'; sleep 1
  } | timeout 12 qemu-system-i386 -display none -serial stdio -monitor none \
        -audiodev none,id=snd0 -device sb16,audiodev=snd0 \
        -drive file=$T/f.img,format=raw,if=floppy -boot a -no-reboot > $T/log 2>&1
  tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 > $T/log.txt
  if grep -q "^sigue-vivo-$KB" $T/log.txt && grep -qF "BLASTER=A220 I5 D1 H5 T6" $T/log.txt \
     && [ "$(grep -c 'Variable puesta: BLASTER=A220 I5 D1 H5 T6' $T/log.txt)" = 1 ]; then :; else
    bad="$bad $KB"; cp $T/log.txt $T/bad_$KB.txt
  fi
done
if [ -z "$bad" ]; then echo "  OK   las 16 direcciones"; else
  echo "  FAIL con el disco RAM de:$bad KB"; fail=1
  for k in $bad; do echo "--- $k KB ---"; tail -12 $T/bad_$k.txt; done
fi

for CASE in dsp105 dsp201; do
  cp "$IMG" $T/f.img
  python3 tests/tools/audio.py make $T/TEST.WAV 1000 700 22050 8 1
  mcopy -o -i $T/f.img $T/TEST.WAV ::TEST.WAV
  if [ $CASE = dsp105 ]; then VER=261; WANT="Sound Blaster en 220h, IRQ 7, DMA 1 (DSP 1.05)"; BL="BLASTER=A220 I7 D1 T1"
  else VER=513; WANT="Sound Blaster en 220h, IRQ 7, DMA 1 (DSP 2.01)"; BL="BLASTER=A220 I7 D1 T3"; fi
  {
    sleep 4
    printf 'A:\\HISPADOS\\HSPSND\r'; sleep 3
    printf 'PLAYWAV /TONE 440 800\r'; sleep 4
    printf 'PLAYWAV TEST.WAV\r'; sleep 4
    printf 'SET\r'; sleep 1
  } | timeout 30 qemu-system-i386 -display none -serial stdio -monitor none \
        -audiodev wav,id=snd0,path=$T/out.wav -device sb16,audiodev=snd0,version=$VER,irq=7 \
        -drive file=$T/f.img,format=raw,if=floppy -boot a -no-reboot > $T/log 2>&1
  tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 | grep -v '^audio:' > $T/log.txt
  python3 tests/tools/audio.py check $T/out.wav > $T/audio.txt 2>&1
  echo "== Hispa-DOS HSPSND: $CASE, IRQ 7 sin BLASTER =="
  for want in "$WANT" "$BL" "Tono 440 Hz en Sound Blaster"; do
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
done
rm -rf $T
exit $fail
