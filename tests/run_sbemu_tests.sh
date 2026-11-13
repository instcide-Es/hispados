#!/bin/bash
# Prueba de la Sound Blaster emulada (HSPSND /SB sobre HSPEMM): un PC sin
# Sound Blaster, solo con Intel HD Audio o con AC'97. SBTEST (tests/progs/)
# hace lo que un juego: reinicia el DSP, pide la versión, programa el 8237 y
# reproduce por DMA un bloque único (14h), en autoinicio (1Ch) y en 16 bits
# estéreo (B6h), contando las IRQ; detecta el AdLib y toca una nota FM. QEMU
# graba en un .wav lo que sale por la tarjeta de verdad y se miden los tonos.
#   hda      Intel HD Audio (hda-duplex), HSPEMM con EMS
#   ac97     Intel AC'97
#   hdalh    HD Audio con LH HSPSND /SB (el anillo en la memoria superior:
#            las direcciones físicas no son las lineales)
# Y además: HD Audio sin /SB bajo HSPEMM (antes no funcionaba en modo V86),
# /SB sin HSPEMM (lo dice y sigue sin emular) y /SB con una Sound Blaster de
# verdad (no emula). Y «Windows» en modo 386 mejorado (EMMMISC W): apaga el
# modo V86, la emulación se para, y al volver arranca otra vez.
# Emulated Sound Blaster test (HSPSND /SB over HSPEMM, HD Audio and AC'97).
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
B=$(dirname "$IMG")
[ -f $B/SBTEST.COM ] || { echo "falta $B/SBTEST.COM (make emmprogs)"; exit 1; }
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() { if grep -qF -- "$1" $T/log.txt; then ok "$1"; else ko "$1"; fi; }
checkn() { if grep -qF -- "$1" $T/log.txt; then ko "(no debía salir) $1"; else ok "sin \"$1\""; fi; }
# tono HZ SEGUNDOS: hay un tramo de esa frecuencia (5 %) y esa duración (0,2 s)
tono() {
  python3 - $T/audio.txt "$1" "$2" <<'EOF' || fail=1
import re, sys
txt = open(sys.argv[1]).read()
want, dur = float(sys.argv[2]), float(sys.argv[3])
tr = [(float(a), float(b), float(h)) for a, b, h in re.findall(r'tramo ([\d.]+)-([\d.]+) s: (\d+) Hz', txt)]
m = [t for t in tr if abs(t[2] - want) / want < 0.05 and abs((t[1] - t[0]) - dur) < 0.2]
if m:
    print("  OK   suena %d Hz durante %.2f s" % (want, m[0][1] - m[0][0]))
else:
    print("  FAIL no suena %d Hz unos %.1f s (tramos: %s)" % (want, dur, tr)); sys.exit(1)
EOF
}
# run "CONFIG.SYS" "DISPOSITIVOS" TIEMPO orden...   (cada orden: «segundos|texto»)
run() {
  local cfg=$1 dev=$2 tmo=$3; shift 3
  T=$(mktemp -d)
  cp "$IMG" $T/f.img
  printf "$cfg" > $T/CONFIG.SYS
  mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
  mcopy -o -i $T/f.img $B/SBTEST.COM ::SBTEST.COM
  for f in ${EXTRA:-}; do mcopy -o -i $T/f.img $f ::; done
  { sleep 6
    for c in "$@"; do printf '%s\r' "${c#*|}"; sleep "${c%%|*}"; done
  } | timeout $tmo qemu-system-i386 -display none -serial stdio -monitor none -parallel none \
        -m 32 -net none -audiodev wav,id=snd0,path=$T/out.wav $dev \
        -drive file=$T/f.img,format=raw,if=floppy,index=0 -boot a -no-reboot > $T/raw 2>&1
  tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 | grep -v '^audio:' > $T/log.txt
  python3 tests/tools/audio.py check $T/out.wav > $T/audio.txt 2>&1
}
fin() {
  if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log.txt; cat $T/audio.txt; fi
  rm -rf $T
}
EMM='DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nDEVICE=A:\\HISPADOS\\HSPEMM.SYS\r\nDOS=UMB\r\n'
EMMNOEMS='DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nDEVICE=A:\\HISPADOS\\HSPEMM.SYS NOEMS\r\nDOS=UMB\r\n'
HDA="-device intel-hda -device hda-duplex,audiodev=snd0"
AC97="-device AC97,audiodev=snd0"

for CARD in hda ac97 hdalh; do
  CFG=$EMM; DEV=$HDA; NAME="Intel HD Audio"; LOAD='A:\HISPADOS\HSPSND /SB'
  [ $CARD = ac97 ] && { DEV=$AC97; NAME="Intel AC'97"; }
  [ $CARD = hdalh ] && { CFG=$EMMNOEMS; LOAD='LH A:\HISPADOS\HSPSND /SB'; }
  echo "== Hispa-DOS: Sound Blaster emulada ($CARD: $NAME) =="
  run "$CFG" "$DEV" 45 "4|$LOAD" '1|SET' '9|A:\SBTEST' '3|SONAR /TONE 440 500' '2|A:\HISPADOS\HSPSND' '2|MEM /C'
  check "$NAME"
  check "Sound Blaster 16 emulada en 220h, IRQ 5, DMA 1 y 5 (DSP 4.05)"
  check "BLASTER=A220 I5 D1 H5 T6"
  check "SBTEST: tarjeta en 220h, IRQ 5, DMA 1 y 5"
  check "Reinicio del DSP: correcto"
  check "DSP versión 4.05"
  check "Identificación (E0h): correcta"
  check "Mezclador: IRQ y DMA 02 22, maestro correcto"
  check "IRQ (F2h): llega"
  if grep -q "Bloque único (14h): IRQ a los 1[0-2] tics, correcto" $T/log.txt; then ok "bloque único (14h): la IRQ llega a su hora"; else ko "bloque único (14h): la IRQ llega a su hora"; fi
  if grep -q "Autoinicio (1Ch): 9 IRQ en 1[3-6] tics, correcto" $T/log.txt; then ok "autoinicio (1Ch): 9 IRQ a su hora"; else ko "autoinicio (1Ch): 9 IRQ a su hora"; fi
  check "La cuenta del DMA avanza: correcto"
  if grep -q "SB16 16 bits estéreo (B6h): 25 IRQ en \(9\|1[0-2]\) tics, correcto" $T/log.txt; then ok "SB16 de 16 bits estéreo (B6h): 25 IRQ a su hora"; else ko "SB16 de 16 bits estéreo (B6h): 25 IRQ a su hora"; fi
  check "AdLib (FM): detectado"
  check "Tono 440 Hz en $NAME"
  check "Ya está instalado."
  checkn "HSPEMM:"
  if [ $CARD = hdalh ]; then
    if awk '/Programas en la memoria superior/{u=1} u && /HSPSND/{f=1} END{exit !f}' $T/log.txt; then
      ok "HSPSND cargado en la memoria superior"
    else ko "HSPSND cargado en la memoria superior"; fi
  fi
  tono 500 0.6      # bloque único: 8 bits mono, 10989 Hz
  tono 808 0.83     # autoinicio: 8 bits mono, 22222 Hz, 9 bloques
  tono 1206 0.58    # SB16: 16 bits estéreo, 44100 Hz, 25 bloques
  tono 600 0.7      # FM: un operador a 600 Hz
  tono 440 0.5      # la API de HSPSND (SONAR) sobre el mismo anillo
  fin
done

# ---- HD Audio bajo HSPEMM sin /SB: la API normal, por la ventana ----
echo "== Hispa-DOS: HD Audio en modo V86 (HSPEMM), sin /SB =="
run "$EMM" "$HDA" 30 '4|A:\HISPADOS\HSPSND' '4|SONAR /TONE 440 800' '2|A:\HISPADOS\HSPSND /INFO' '1|SET'
check "Intel HD Audio en "
check "Tono 440 Hz en Intel HD Audio"
check "Códec "
checkn "BLASTER="
checkn "emulada"
tono 440 0.8
fin

# ---- /SB sin HSPEMM: lo dice y funciona sin emular ----
echo "== Hispa-DOS: HSPSND /SB sin HSPEMM =="
run 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\n' "$HDA" 24 '4|A:\HISPADOS\HSPSND /SB' '4|SONAR /TONE 440 800' '1|SET'
check "/SB: hace falta HSPEMM.SYS (en CONFIG.SYS) para emular la Sound Blaster."
check "Intel HD Audio en "
checkn "BLASTER="
tono 440 0.8
fin

# ---- /SB con una Sound Blaster de verdad: no se emula ----
echo "== Hispa-DOS: HSPSND /SB con una Sound Blaster 16 de verdad =="
run "$EMMNOEMS" "-device sb16,audiodev=snd0" 28 '4|A:\HISPADOS\HSPSND /SB' '8|A:\SBTEST'
check "/SB: hay una Sound Blaster de verdad; no se emula."
check "Sound Blaster 16 en 220h, IRQ 5, DMA 1 y 5 (DSP 4.05)"
checkn "emulada"
check "Reinicio del DSP: correcto"
check "IRQ (F2h): llega"
tono 500 0.6
tono 808 0.83
fin

# ---- «Windows» en modo 386 mejorado apaga el modo V86 (INT 2Fh AX=1605h): la
# emulación se para (sin monitor no hay puertos ni registros del HD Audio) y
# al volver (1606h) arranca otra vez. EMMMISC W hace de Windows ----
echo "== Hispa-DOS: HSPSND /SB y Windows 386 mejorado (EMMMISC W) =="
if [ -f $B/EMMMISC.COM ]; then
  EXTRA=$B/EMMMISC.COM
  run 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nDEVICE=A:\\HISPADOS\\HSPEMM.SYS NOEMS\r\n' "$HDA" 36 '4|A:\HISPADOS\HSPSND /SB' '3|A:\EMMMISC W' '3|SONAR /TONE 440 500' '9|A:\SBTEST'
  EXTRA=
  check "Sound Blaster 16 emulada en 220h, IRQ 5, DMA 1 y 5 (DSP 4.05)"
  check "Windows: modo V86 apagado y encendido OK"
  check "Reinicio del DSP: correcto"
  check "IRQ (F2h): llega"
  check "La cuenta del DMA avanza: correcto"
  check "AdLib (FM): detectado"
  checkn "HSPEMM:"
  tono 440 0.5
  tono 500 0.6
  tono 808 0.83
  tono 1206 0.58
  tono 600 0.7
  fin
else
  echo "  (falta $B/EMMMISC.COM: make emmprogs)"
fi
exit $fail
