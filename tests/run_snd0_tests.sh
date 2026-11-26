#!/bin/bash
# HSPSND 01h con frecuencia 0 (un WAV con la cabecera mal): vuelve con CF
# en vez de colgarse para siempre; con 8000 Hz suena. En HD Audio, AC'97 y
# Sound Blaster 16.
# HSPSND: zero sample rate.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
fail=0
for CARD in hda ac97 sb16; do
  T=$(mktemp -d)
  nasm -f bin -o $T/SND0.COM tests/progs/snd0.asm || exit 1
  cp "$IMG" $T/f.img
  mcopy -o -i $T/f.img $T/SND0.COM ::
  case $CARD in
    hda) DEV="-device intel-hda -device hda-output,audiodev=snd0" ;;
    ac97) DEV="-device AC97,audiodev=snd0" ;;
    *) DEV="-device sb16,audiodev=snd0" ;;
  esac
  { sleep 4; printf 'A:\\HISPADOS\\HSPSND\r'; sleep 3; printf 'SND0\r'; sleep 4; printf 'ECHO fin-snd0\r'; sleep 1; } |
    timeout 20 qemu-system-i386 -display none -serial stdio -monitor none \
      -audiodev wav,id=snd0,path=$T/out.wav $DEV \
      -drive file=$T/f.img,format=raw,if=floppy -boot a -no-reboot > $T/log 2>&1
  tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 -c | grep -v '^audio:' > $T/log.txt
  echo "== HSPSND con frecuencia 0 ($CARD) =="
  for want in "SND0: 0 Hz rechazado" "SND0: 8000 Hz bien" "SND0: fin" "fin-snd0"; do
    if grep -qF -- "$want" $T/log.txt; then echo "  OK   $want"; else echo "  FAIL $want"; fail=1; fi
  done
  if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log.txt; fi
  rm -rf $T
done
exit $fail
