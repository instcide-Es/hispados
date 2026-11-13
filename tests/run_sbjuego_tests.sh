#!/bin/bash
# [SONIDO-JUEGOS] Un programa que usa la Sound Blaster directamente, como un
# juego de DOS, con HSPSND cargado como en el disquete de uso (detrás de
# HSPRATON y HSPJOY, con /VOL:80).
#
# SBJUEGO (tests/progs/sbjuego.asm) lee BLASTER, reinicia el DSP, enciende el
# altavoz, toma la IRQ y toca por DMA de ciclo único un tono de 8 bits (unos
# 440 Hz) y otro de 16 bits (882 Hz); QEMU graba lo que sale por la tarjeta y
# se mide. Antes y después HSPSND toca su propio tono (300 Hz): el juego no
# debe encontrarse la tarjeta estorbada por HSPSND, ni dejársela inservible.
# Casos: SB16 en 220h/IRQ 5/DMA 1 y 5 (la de VirtualBox), SB16 en 240h/IRQ 7/
# DMA 3 y 7, SB 2.01 (solo 8 bits) y, de referencia, sin HSPSND (con la
# variable BLASTER puesta a mano).
# A program that drives the Sound Blaster directly, like a DOS game would,
# with HSPSND resident: the recorded card output is measured.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
B=$(dirname "$IMG")
[ -f "$B/SBJUEGO.COM" ] || make -s B="$B" "$B/SBJUEGO.COM" >/dev/null 2>&1 || { echo "falta $B/SBJUEGO.COM"; exit 1; }
fail=0
for CASE in sb16 sb240 sb2 sinhspsnd; do
  T=$(mktemp -d)
  cp "$IMG" $T/f.img
  mcopy -o -i $T/f.img "$B/SBJUEGO.COM" ::SBJUEGO.COM
  DEV="-device sb16,audiodev=snd0"; OPC=""; D16=1; PROPIO=1
  HW="Sound Blaster 16 en 220h, IRQ 5, DMA 1 y 5 (DSP 4.05)"; BL="BLASTER=A220 I5 D1 H5 T6"
  case $CASE in
    sb16)   NAME="SB16 en 220h, IRQ 5, DMA 1 y 5" ;;
    sb240)  NAME="SB16 en 240h, IRQ 7, DMA 3 y 7"; DEV="-device sb16,audiodev=snd0,iobase=0x240,irq=7,dma=3,dma16=7"
            HW="Sound Blaster 16 en 240h, IRQ 7, DMA 3 y 7 (DSP 4.05)"; BL="BLASTER=A240 I7 D3 H7 T6" ;;
    sb2)    NAME="SB 2.01 (solo 8 bits)"; DEV="-device sb16,audiodev=snd0,version=0x0201"; D16=""
            HW="Sound Blaster en 220h, IRQ 5, DMA 1 (DSP 2.01)"; BL="BLASTER=A220 I5 D1 T3" ;;
    sinhspsnd) NAME="SB16 sin HSPSND (referencia)"; PROPIO="" ;;
  esac
  {
    sleep 5
    if [ -n "$PROPIO" ]; then
      printf 'A:\\HISPADOS\\HSPRATON\r'; sleep 2
      printf 'A:\\HISPADOS\\HSPJOY\r'; sleep 2
      printf 'A:\\HISPADOS\\HSPSND /VOL:80%s\r' "$OPC"; sleep 3
      printf 'PLAYWAV /TONE 300 400\r'; sleep 4
    else
      printf 'SET BLASTER=A220 I5 D1 H5 T6\r'; sleep 1
    fi
    printf 'SBJUEGO\r'; sleep 6
    printf 'IF NOT ERRORLEVEL 1 ECHO sbjuego-nivel-0\r'; sleep 1
    if [ -n "$PROPIO" ]; then
      printf 'PLAYWAV /TONE 300 400\r'; sleep 4
    fi
    printf 'SET\r'; sleep 1
  } | timeout 60 qemu-system-i386 -display none -serial stdio -monitor none \
        -audiodev wav,id=snd0,path=$T/out.wav $DEV \
        -drive file=$T/f.img,format=raw,if=floppy -boot a -no-reboot > $T/log 2>&1
  tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 | grep -v '^audio:' > $T/log.txt
  python3 tests/tools/audio.py check $T/out.wav > $T/audio.txt 2>&1
  echo "== Hispa-DOS: un juego con la Sound Blaster ($NAME) =="
  WANT=("SBJUEGO: 8 bits: OK, 4 interrupciones" "sbjuego-nivel-0" "$BL")
  [ -n "$D16" ] && WANT+=("SBJUEGO: 16 bits: OK, 8 interrupciones")
  [ -n "$PROPIO" ] && WANT+=("$HW" "Variable puesta: $BL")
  for want in "${WANT[@]}"; do
    if grep -qF -- "$want" $T/log.txt; then echo "  OK   $want"; else echo "  FAIL $want"; fail=1; fi
  done
  if [ -z "$D16" ] && grep -q "SBJUEGO: 16 bits" $T/log.txt; then echo "  FAIL no debía probar los 16 bits"; fail=1; fi
  python3 - $T/audio.txt "$PROPIO" "$D16" <<'EOF' || fail=1
import re, sys
txt = open(sys.argv[1]).read()
tr = [(float(a), float(b), float(h)) for a, b, h in re.findall(r'tramo ([\d.]+)-([\d.]+) s: (\d+) Hz', txt)]
esperado = []
if sys.argv[2]:
    esperado.append((300, 0.4, 'el tono de HSPSND antes del juego'))
esperado.append((440, 0.73, 'el tono de 8 bits del juego'))
if sys.argv[3]:
    esperado.append((882, 0.73, 'el tono de 16 bits del juego'))
if sys.argv[2]:
    esperado.append((300, 0.4, 'el tono de HSPSND después del juego'))
ok = True
i = 0
for hz, dur, que in esperado:
    while i < len(tr) and not (abs(tr[i][2] - hz) / hz < 0.05 and abs((tr[i][1] - tr[i][0]) - dur) < 0.2):
        i += 1
    if i < len(tr):
        print("  OK   suena %s (%d Hz, %.2f s)" % (que, tr[i][2], tr[i][1] - tr[i][0]))
        i += 1
    else:
        print("  FAIL no suena %s (%d Hz, %.1f s; tramos: %s)" % (que, hz, dur, tr)); ok = False
sys.exit(0 if ok else 1)
EOF
  if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log.txt; cat $T/audio.txt; fi
  rm -rf $T
done
exit $fail
