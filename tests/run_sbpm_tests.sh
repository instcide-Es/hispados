#!/bin/bash
# [SB-PM] La Sound Blaster emulada (HSPSND /SB) para los programas en modo
# protegido por DPMI (como los juegos con DOS/4GW): HSPDPMI corre a sus
# clientes con IOPL 0 y el mapa de E/S de la TSS, atrapa sus IN/OUT a la SB,
# a la AdLib, al 8237 y al 8259, los hace en modo V86 (donde los lleva
# HSPEMM a HSPSND) y les entrega la IRQ 5 en su manejador de modo protegido.
# SBPM.COM (tests/progs/sbpm.asm) es SBTEST en 32 bits por DPMI; QEMU, sin
# sb16, graba lo que sale por la tarjeta y se miden los tonos.
#   hda      Intel HD Audio (hda-duplex), HSPEMM con EMS
#   ac97     Intel AC'97
#   hdalh    HD Audio con LH HSPSND /SB y LH HSPDPMI (NOEMS)
#   hdanoirq HD Audio sin IRQ (SINIRQ: como el ThinkPad A275, cuya BIOS no
#            se la da): HSPSND tira del RTC
# Y además: SBPM sobre una Sound Blaster 16 de verdad con HSPDPMI y HSPEMM
# (no se emula: IOPL 3, todo igual) y sin HSPEMM (HSPDPMI en crudo).
# Emulated Sound Blaster for DPMI (protected mode) programs.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
B=$(dirname "$IMG")
[ -f $B/SBPM.COM ] || { echo "falta $B/SBPM.COM (make emmprogs)"; exit 1; }
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() { if grep -qF -- "$1" $T/log.txt; then ok "$1"; else ko "$1"; fi; }
checkn() { if grep -qF -- "$1" $T/log.txt; then ko "(no debía salir) $1"; else ok "sin \"$1\""; fi; }
checkre() { if grep -q -- "$1" $T/log.txt; then ok "$2"; else ko "$2"; fi; }
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
  mcopy -o -i $T/f.img $B/SBPM.COM ::SBPM.COM
  mcopy -o -i $T/f.img $B/SINIRQ.COM ::SINIRQ.COM
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
comunes() {
  check "SBPM: en modo protegido de 32 bits (DPMI)"
  check "Reinicio del DSP: correcto"
  check "DSP versión 4.05"
  check "Identificación (E0h): correcta"
  check "Mezclador: IRQ y DMA 02 22, maestro correcto"
  check "IRQ (F2h): llega"
  checkre "Bloque único (14h): IRQ a los 1[0-2] tics, correcto" "bloque único (14h): la IRQ llega a su hora"
  checkre "Autoinicio (1Ch): 9 IRQ en 1[3-6] tics, correcto" "autoinicio (1Ch): 9 IRQ a su hora"
  check "La cuenta del DMA avanza: correcto"
  checkre "SB16 16 bits estéreo (B6h): 25 IRQ en \(9\|1[0-2]\) tics, correcto" "SB16 de 16 bits estéreo (B6h): 25 IRQ a su hora"
  check "AdLib (FM): detectado"
  checkre "1000 escrituras FM: \([0-9]\|1[0-8]\) tics" "1000 escrituras FM en menos de un segundo"
  check "CLI: el reloj se para"
  check "STI: el reloj sigue"
  check "PUSHFD/CLI/POPFD: el reloj sigue"
  check "Puerto 4D0h: correcto"
  check "SBPM terminado, fallos: 0"
  checkn "HSPDPMI: excepción"
  checkn "HSPEMM:"
  checkn "MAL"
}
EMM='DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nDEVICE=A:\\HISPADOS\\HSPEMM.SYS\r\nDOS=UMB\r\n'
EMMNOEMS='DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nDEVICE=A:\\HISPADOS\\HSPEMM.SYS NOEMS\r\nDOS=UMB\r\n'
HDA="-device intel-hda -device hda-duplex,audiodev=snd0"
AC97="-device AC97,audiodev=snd0"

for CARD in hda ac97 hdalh hdanoirq; do
  CFG=$EMM; DEV=$HDA; NAME="Intel HD Audio"; LOAD='A:\HISPADOS\HSPSND /SB'; DP='A:\HISPADOS\HSPDPMI'
  [ $CARD = ac97 ] && { DEV=$AC97; NAME="Intel AC'97"; }
  [ $CARD = hdalh ] && { CFG=$EMMNOEMS; LOAD='LH A:\HISPADOS\HSPSND /SB'; DP='LH A:\HISPADOS\HSPDPMI'; }
  PRE='1|REM'
  [ $CARD = hdanoirq ] && PRE='2|A:\SINIRQ'
  echo "== Hispa-DOS: Sound Blaster emulada para DPMI ($CARD: $NAME) =="
  run "$CFG" "$DEV" 64 "$PRE" "4|$LOAD" "2|$DP" '16|A:\SBPM' '2|A:\HISPADOS\HSPDPMI' '3|SONAR /TONE 440 500' '2|MEM /C'
  check "$NAME"
  check "Sound Blaster 16 emulada en 220h, IRQ 5, DMA 1 y 5 (DSP 4.05)"
  check "SBPM: IOPL 0"
  comunes
  check "HSPDPMI: ya hay un servidor DPMI instalado."
  check "Tono 440 Hz en $NAME"
  tono 500 0.6      # bloque único: 8 bits mono, 10989 Hz
  tono 808 0.83     # autoinicio: 8 bits mono, 22222 Hz, 9 bloques
  tono 1206 0.58    # SB16: 16 bits estéreo, 44100 Hz, 25 bloques
  tono 600 0.7      # FM: un operador a 600 Hz
  tono 440 0.5      # la API de HSPSND (SONAR) después
  fin
done

# ---- una Sound Blaster de verdad: HSPSND /SB no emula y HSPDPMI no atrapa ----
echo "== Hispa-DOS: SBPM con una Sound Blaster 16 de verdad (HSPEMM, sin emular) =="
run "$EMMNOEMS" "-device sb16,audiodev=snd0 -device adlib,audiodev=snd0" 50 '4|A:\HISPADOS\HSPSND /SB' '2|A:\HISPADOS\HSPDPMI' '16|A:\SBPM'
check "/SB: hay una Sound Blaster de verdad; no se emula."
check "SBPM: IOPL 3"
comunes
tono 500 0.6
tono 808 0.83
fin

echo "== Hispa-DOS: SBPM con una Sound Blaster 16 de verdad (HSPDPMI sin HSPEMM) =="
run 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\n' "-device sb16,audiodev=snd0 -device adlib,audiodev=snd0" 50 '4|A:\HISPADOS\HSPSND' '2|A:\HISPADOS\HSPDPMI' '16|A:\SBPM'
check "SBPM: IOPL 3"
comunes
tono 500 0.6
fin
exit $fail
