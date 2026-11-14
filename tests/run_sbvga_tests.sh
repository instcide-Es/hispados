#!/bin/bash
# [SB-VGA] Modo X (320x200 sin encadenar, como Doom) desde un programa en modo
# protegido por DPMI mientras suena la Sound Blaster emulada (HSPSND /SB +
# HSPDPMI + HSPEMM): SBVGA.COM (tests/progs/sbvga.asm) pinta columnas con la
# máscara del secuenciador (3C4h/3C5h), lee los cuatro planos (3CEh/3CFh) y
# comprueba que no se mezclan, con la IRQ 5 en autoinicio, la FM y la IRQ 0
# enganchadas en modo protegido; y que tras PUSHFD / CLI / OUT (o IN) a un
# puerto atrapado / POPFD las interrupciones vuelven a estar abiertas (antes
# HSPDPMI, que va paso a paso tras un CLI emulado, no veía venir ese POPFD:
# el reloj se paraba y Doom se colgaba al rato en el HP Mini 5103), y que con
# un manejador de la IRQ 5 lento (abre las interrupciones y espera un tic)
# HSPSND no le entrega la siguiente hasta que vuelve (se anidaban sin fin:
# «Transfer stack overflow» de DOS/4GW, «pila de modo real agotada»).
#   hda       Intel HD Audio, -vga std
#   hdanoirq  HD Audio sin IRQ (SINIRQ): HSPSND tira del RTC
#   cirrus    HD Audio con -vga cirrus
#   lh        LH HSPSND /SB y LH HSPDPMI /SB (como la opción «Juegos»)
#   lenta     HD Audio sin IRQ con la CPU frenada (-icount), como un Atom
# Y de referencia, una sb16 de verdad (IOPL 3: no se atrapa nada).
# Mode X drawing from a DPMI client while the emulated Sound Blaster plays.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
B=$(dirname "$IMG")
[ -f $B/SBVGA.COM ] || { echo "falta $B/SBVGA.COM (make emmprogs)"; exit 1; }
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() { if grep -qF -- "$1" $T/log.txt; then ok "$1"; else ko "$1"; fi; }
checkn() { if grep -qF -- "$1" $T/log.txt; then ko "(no debía salir) $1"; else ok "sin \"$1\""; fi; }
checkre() { if grep -q -- "$1" $T/log.txt; then ok "$2"; else ko "$2"; fi; }
# run "CONFIG.SYS" "DISPOSITIVOS" TIEMPO orden...   (cada orden: «segundos|texto»)
run() {
  local cfg=$1 dev=$2 tmo=$3; shift 3
  T=$(mktemp -d)
  cp "$IMG" $T/f.img
  printf "$cfg" > $T/CONFIG.SYS
  mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
  mcopy -o -i $T/f.img $B/SBVGA.COM ::SBVGA.COM
  mcopy -o -i $T/f.img $B/SINIRQ.COM ::SINIRQ.COM
  { sleep 6
    for c in "$@"; do printf '%s\r' "${c#*|}"; sleep "${c%%|*}"; done
  } | timeout $tmo qemu-system-i386 -display none -serial stdio -monitor none -parallel none \
        -m 32 -net none -audiodev none,id=snd0 $dev \
        -drive file=$T/f.img,format=raw,if=floppy,index=0 -boot a -no-reboot > $T/raw 2>&1
  tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 | grep -v '^audio:' > $T/log.txt
}
fin() {
  if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log.txt; fi
  rm -rf $T
}
comunes() {
  check "SBVGA: en modo protegido de 32 bits (DPMI)"
  check "Modo X (320x200 sin encadenar) con la SB y la FM sonando"
  check "Planos de la VGA: sin mezclar, correcto"
  checkre "Reloj durante el dibujo: [0-9]* tics, correcto" "el reloj sigue mientras se dibuja"
  checkre "IRQ 0 en el manejador de modo protegido: [0-9]*, correcto" "IRQ 0 en el manejador de modo protegido"
  checkre "IRQ 5 de la SB durante el dibujo: [0-9]*, correcto" "IRQ 5 de la SB durante el dibujo"
  checkre "PUSHFD/CLI/OUT/POPFD: [0-9]* vueltas, interrupciones cerradas después: 0, correcto" \
          "PUSHFD/CLI/OUT|IN/POPFD: las interrupciones vuelven a abrirse"
  check "PUSHFD/CLI/OUT/POPFD en el manejador de la IRQ 5, cerradas después: 0, correcto"
  check "SBVGA terminado, fallos: 0"
  checkn "HSPDPMI: excepción"
  checkn "HSPDPMI: pila"
  checkn "HSPDPMI: error interno"
  checkn "HSPEMM:"
  checkn "MAL"
}
EMM='DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nDEVICE=A:\\HISPADOS\\HSPEMM.SYS\r\nDOS=UMB\r\n'
EMMNOEMS='DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nDEVICE=A:\\HISPADOS\\HSPEMM.SYS NOEMS\r\nDOS=UMB\r\n'
HDA="-device intel-hda -device hda-duplex,audiodev=snd0"

for CASO in ${CASOS:-hda hdanoirq cirrus lh lenta}; do
  CFG=$EMM; DEV="$HDA -vga std"; LOAD='A:\HISPADOS\HSPSND /SB'; DP='A:\HISPADOS\HSPDPMI'
  PRE='1|REM'; TMO=70; ESPERA=30
  [ $CASO = hdanoirq ] && PRE='2|A:\SINIRQ'
  [ $CASO = cirrus ] && DEV="$HDA -vga cirrus"
  [ $CASO = lh ] && { CFG=$EMMNOEMS; LOAD='LH A:\HISPADOS\HSPSND /SB'; DP='LH A:\HISPADOS\HSPDPMI /SB'; }
  [ $CASO = lenta ] && { PRE='2|A:\SINIRQ'; DEV="$HDA -vga std -icount shift=5"; TMO=150; ESPERA=100; }
  echo "== Hispa-DOS: modo X con la Sound Blaster emulada ($CASO) =="
  run "$CFG" "$DEV" $TMO "$PRE" "4|$LOAD" "2|$DP" "$ESPERA|A:\\SBVGA"
  check "Sound Blaster 16 emulada en 220h, IRQ 5, DMA 1 y 5 (DSP 4.05)"
  check "SBVGA: IOPL 0"
  checkre "IRQ 5 con un manejador lento: anidadas como mucho 1$" "IRQ 5 con un manejador lento: no se anida (como mucho 1)"
  comunes
  fin
done

# ---- una Sound Blaster de verdad: HSPDPMI no atrapa (IOPL 3) ----
echo "== Hispa-DOS: modo X con una Sound Blaster 16 de verdad (HSPEMM, sin emular) =="
run "$EMMNOEMS" "-device sb16,audiodev=snd0 -device adlib,audiodev=snd0 -vga std" 60 '4|A:\HISPADOS\HSPSND /SB' '2|A:\HISPADOS\HSPDPMI' '25|A:\SBVGA'
check "SBVGA: IOPL 3"
checkre "IRQ 5 con un manejador lento: anidadas como mucho [1-9]" "IRQ 5 con un manejador lento (la de verdad sí se anida)"
comunes
fin
exit $fail
