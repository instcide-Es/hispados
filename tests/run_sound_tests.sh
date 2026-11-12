#!/bin/bash
# Prueba de sonido: QEMU graba en un .wav lo que sale por la tarjeta; HSPSND +
# PLAYWAV tocan un tono de 440 Hz y un WAV de 1000 Hz y se mide la grabación.
# Se prueba con Intel HD Audio y con Intel AC'97.
# Sound test with HD Audio and AC'97: the card output is recorded and analysed.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
fail=0
for CARD in hda ac97 sb16 sb8 sb240; do
  PRE="REM"
  T=$(mktemp -d)
  cp "$IMG" $T/f.img
  python3 tests/tools/audio.py make $T/TEST.WAV 1000 700 22050 8 1
  mcopy -o -i $T/f.img $T/TEST.WAV ::TEST.WAV
  if [ $CARD = hda ]; then
    DEV="-device intel-hda -device hda-output,audiodev=snd0"; NAME="Intel HD Audio"
  elif [ $CARD = ac97 ]; then
    DEV="-device AC97,audiodev=snd0"; NAME="Intel AC'97"
  elif [ $CARD = sb16 ]; then
    DEV="-device sb16,audiodev=snd0"; NAME="Sound Blaster 16"
  elif [ $CARD = sb8 ]; then
    # vía de 8 bits (SB 2.0/Pro): sin DMA de 16 bits
    DEV="-device sb16,audiodev=snd0"; NAME="Sound Blaster"; PRE="SET BLASTER=A220 I5 D1 H0 T4"
  else
    DEV="-device sb16,audiodev=snd0,iobase=0x240,irq=7,dma=3,dma16=7"; NAME="Sound Blaster 16"
  fi
  {
    sleep 4
    printf '%s\r' "$PRE"; sleep 1
    printf 'A:\\HISPADOS\\HSPSND\r'; sleep 3
    printf 'PLAYWAV /TONE 440 800\r'; sleep 4
    printf 'PLAYWAV TEST.WAV\r'; sleep 4
    printf 'A:\\HISPADOS\\HSPSND\r'; sleep 2
    printf 'SET\r'; sleep 1
  } | timeout 34 qemu-system-i386 -display none -serial stdio -monitor none \
        -audiodev wav,id=snd0,path=$T/out.wav $DEV \
        -drive file=$T/f.img,format=raw,if=floppy -boot a -no-reboot > $T/log 2>&1
  tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 | grep -v '^audio:' > $T/log.txt
  python3 tests/tools/audio.py check $T/out.wav > $T/audio.txt 2>&1
  echo "== Hispa-DOS sound tests ($NAME) =="
  EXTRA=""
  [ $CARD = sb16 ] && EXTRA="Sound Blaster 16 en 220h, IRQ 5, DMA 1 y 5 (DSP 4.05)|BLASTER=A220 I5 D1 H5 T6"
  [ $CARD = sb8 ] && EXTRA="Sound Blaster en 220h, IRQ 5, DMA 1 (DSP 4.05)"
  [ $CARD = sb240 ] && EXTRA="Sound Blaster 16 en 240h, IRQ 7, DMA 3 y 7 (DSP 4.05)|BLASTER=A240 I7 D3 H7 T6"
  IFS='|' read -ra XW <<< "$EXTRA"
  for want in "$NAME" "Tono 440 Hz en $NAME" "Reproduciendo TEST.WAV (22050 Hz, 8 bits, mono) en $NAME" "Ya está instalado." "${XW[@]}"; do
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

# ---- Volumen: HSPSND /VOL, SONIDO, SONAR /VOL, teclas y /INFO ----
# En cada tarjeta se tocan tonos de frecuencias distintas con volúmenes
# distintos y se compara el pico medido en la grabación. El primer tono solo
# abre la voz (QEMU no aplica el volumen puesto antes de que exista).
#   hda     códec con amplificador (volumen en el DAC)
#   hdasoft códec sin amplificador (mixer=off): volumen por programa
#   hda2    dos códecs (el de audio en la dirección 2) en un ICH9
#   ac97    maestro del AC'97
#   sb16    QEMU no aplica el mezclador de la SB16: se mira el registro
#   sb2     SB 2.0 (sin mezclador): volumen por programa
for CARD in hda hdasoft hda2 ac97 sb16 sb2; do
  T=$(mktemp -d)
  cp "$IMG" $T/f.img
  # un WAV de 8000 Hz (16 bits, estéreo): los códecs HD Audio que no admiten
  # esa frecuencia (el de QEMU tampoco) lo tocan a 48 kHz repitiendo muestras
  python3 tests/tools/audio.py make $T/T8K.WAV 300 600 8000 16 2
  mcopy -o -i $T/f.img $T/T8K.WAV ::T8K.WAV
  LOW=20; UMBRAL=1500; KEYS=""
  case $CARD in
    hda)     DEV="-device intel-hda -device hda-micro,audiodev=snd0"; NAME="Intel HD Audio"; KEYS=1 ;;
    hdasoft) DEV="-device intel-hda -device hda-output,audiodev=snd0,mixer=off"; NAME="Intel HD Audio"; UMBRAL=60 ;;
    hda2)    DEV="-device ich9-intel-hda -device hda-duplex,audiodev=snd0,cad=2"; NAME="Intel HD Audio" ;;
    ac97)    DEV="-device AC97,audiodev=snd0"; NAME="Intel AC'97"; KEYS=1 ;;
    sb16)    DEV="-device sb16,audiodev=snd0"; NAME="Sound Blaster 16" ;;
    sb2)     DEV="-device sb16,audiodev=snd0,version=0x0201"; NAME="Sound Blaster"; LOW=60; UMBRAL=300 ;;
  esac
  k() { for x in "$@"; do printf 'sendkey %s\n' "$x" | socat - UNIX-CONNECT:$T/mon >/dev/null 2>&1; sleep 0.3; done; }
  {
    sleep 4
    printf 'A:\\HISPADOS\\HSPSND /VOL:100\r'; sleep 3
    printf 'SONAR /TONE 441 300\r'; sleep 3
    printf 'A:\\HISPADOS\\HSPSND /VOL:100\r'; sleep 1.5
    printf 'SONAR /TONE 525 500\r'; sleep 3
    printf 'A:\\HISPADOS\\HSPSND /VOL:%s\r' $LOW; sleep 1.5
    printf 'SONAR /TONE 735 500\r'; sleep 3
    printf 'A:\\HISPADOS\\HSPSND /INFO\r'; sleep 2
    printf 'ECHO uno\r'; sleep 0.5
    printf 'SONIDO\r'; sleep 1
    printf 'SONIDO 60\r'; sleep 1
    printf 'SONIDO +\r'; sleep 1
    printf 'ECHO dos\r'; sleep 0.5
    printf 'SONIDO -\r'; sleep 1
    printf 'SONIDO -\r'; sleep 1
    printf 'SONIDO /M\r'; sleep 1
    printf 'SONAR /TONE 1225 500\r'; sleep 3
    printf 'SONIDO /M\r'; sleep 1
    printf 'SONAR /VOL:100 /TONE 1575 400\r'; sleep 3
    printf 'ECHO tres\r'; sleep 0.5
    printf 'SONIDO\r'; sleep 1
    printf 'SONIDO 150\r'; sleep 1
    printf 'SONIDO /?\r'; sleep 1
    # ajuste interactivo: dos veces «-» (5 % cada una) y Esc
    printf 'SONIDO /A\r'; sleep 1; printf -- '--'; sleep 0.5; printf '\033'; sleep 1
    printf 'ECHO cuatro\r'; sleep 0.5
    printf 'SONIDO\r'; sleep 1
    printf 'SONAR T8K.WAV\r'; sleep 3.5
    if [ -n "$KEYS" ]; then
      # teclas rápidas (pasan por INT 09h -> INT 15h AH=4Fh), con el teclado
      # español cargado después de HSPSND. (kbd-throttle: como un 8042 de
      # verdad, el octeto siguiente no llega hasta pasado 1 ms; sin eso, la
      # lectura doble del puerto 60h de TECLADO + BIOS se come el prefijo E0)
      printf 'TECLADO SP\r'; sleep 1.5
      k ctrl-alt-down ctrl-alt-down ctrl-alt-down
      printf 'ECHO cinco\r'; sleep 0.5
      printf 'SONIDO\r'; sleep 1
      k ctrl-alt-up
      printf 'ECHO seis\r'; sleep 0.5
      printf 'SONIDO\r'; sleep 1
      k ctrl-alt-end
      printf 'ECHO siete\r'; sleep 0.5
      printf 'SONIDO\r'; sleep 1
      k ctrl-alt-end
      # las teclas multimedia (E0 30 / E0 2E / E0 20)
      k volumeup volumedown volumedown
      printf 'ECHO ocho\r'; sleep 0.5
      printf 'SONIDO\r'; sleep 1
      k audiomute
      printf 'ECHO nueve\r'; sleep 0.5
      printf 'SONIDO\r'; sleep 1
      # el teclado sigue vivo y en español: la ñ
      k shift-r shift-e shift-m spc semicolon ret
      sleep 1
    fi
  } | timeout 80 qemu-system-i386 -display none -serial stdio -monitor unix:$T/mon,server,nowait -global i8042.kbd-throttle=on \
        -audiodev wav,id=snd0,path=$T/out.wav $DEV \
        -drive file=$T/f.img,format=raw,if=floppy -boot a -no-reboot > $T/log 2>&1
  tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 | grep -v '^audio:' > $T/log.txt
  python3 tests/tools/audio.py check $T/out.wav $UMBRAL > $T/audio.txt 2>&1
  echo "== Hispa-DOS volume tests ($CARD: $NAME) =="
  python3 tests/tools/volcheck.py $T/log.txt $T/audio.txt $CARD $LOW "$KEYS" || fail=1
  if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log.txt; cat $T/audio.txt; fi
  rm -rf $T
done

# ---- Sin HSPSND: SONIDO lo dice ----
T=$(mktemp -d)
cp "$IMG" $T/f.img
{ sleep 4; printf 'SONIDO\r'; sleep 1; printf 'SONIDO 50\r'; sleep 1; } | timeout 10 qemu-system-i386 -display none -serial stdio -monitor none \
      -drive file=$T/f.img,format=raw,if=floppy -boot a -no-reboot > $T/log 2>&1
echo "== Hispa-DOS volume tests (sin HSPSND) =="
if [ "$(tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 | grep -c 'HSPSND no está cargado')" = 2 ]; then
  echo "  OK   SONIDO avisa de que HSPSND no está cargado"
else
  echo "  FAIL SONIDO avisa de que HSPSND no está cargado"; fail=1
fi
rm -rf $T

# ---- Imagen USB (como en los portátiles sin disquetera): AUTOEXEC.BAT carga
# HSPSND /VOL:80 y SONIDO está en C:\HISPADOS ----
USBIMG=build/hispados-usb.img
echo "== Hispa-DOS volume tests (imagen USB, Intel HD Audio) =="
if [ -f $USBIMG ]; then
  T=$(mktemp -d)
  cp $USBIMG $T/usb.img
  {
    sleep 9
    printf 'SONIDO\r'; sleep 1
    printf 'SONAR /TONE 441 300\r'; sleep 3
    printf 'SONIDO 100\r'; sleep 1
    printf 'SONAR /TONE 525 500\r'; sleep 3
    printf 'HSPSND /INFO\r'; sleep 2
  } | timeout 30 qemu-system-i386 -display none -serial stdio -monitor none \
        -audiodev wav,id=snd0,path=$T/out.wav -device intel-hda -device hda-micro,audiodev=snd0 \
        -device qemu-xhci,id=xhci -drive if=none,id=usbd,format=raw,file=$T/usb.img \
        -device usb-storage,bus=xhci.0,drive=usbd,bootindex=0 -no-reboot > $T/log 2>&1
  tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 | grep -v '^audio:' > $T/log.txt
  python3 tests/tools/audio.py check $T/out.wav > $T/audio.txt 2>&1
  u=0
  for want in "Intel HD Audio en " "Volumen: 80 %  [" "Volumen: 100 %  [" "Tono 525 Hz en Intel HD Audio" \
              "Pin 03: altavoz, conector (00104010) -> 03>02" "Teclas de volumen: Ctrl+Alt+flecha"; do
    if grep -qF -- "$want" $T/log.txt; then echo "  OK   $want"; else echo "  FAIL $want"; u=1; fi
  done
  if grep -q 'tramo .*: 52[0-9] Hz' $T/audio.txt; then echo "  OK   suena el tono de 525 Hz"; else echo "  FAIL suena el tono de 525 Hz"; u=1; fi
  if [ $u -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log.txt; cat $T/audio.txt; fi
  [ $u -ne 0 ] && fail=1
  rm -rf $T
else
  echo "  (no está $USBIMG: make usb)"
fi
exit $fail
