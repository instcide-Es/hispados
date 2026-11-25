#!/bin/bash
# HSPDPMI: 0300h-0302h con la pila del servidor (SS:SP = 0) y más palabras
# de las que caben -> 8021h (antes se pisaban la tabla de callbacks y los
# vectores guardados); un callback de un cliente que ya ha salido vuelve sin
# más; y un cliente muerto en mitad de su manejador de la IRQ 0 no deja el
# 8259 sin su EOI (el reloj sigue).
# HSPDPMI limits and dead-client leftovers (DPMILIM.COM).
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
fail=0
nasm -f bin -o $T/DPMILIM.COM tests/progs/dpmilim.asm || exit 1
cp "$IMG" $T/f.img
printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nFILES=30\r\n' > $T/CONFIG.SYS
mcopy -o -i $T/f.img $T/CONFIG.SYS $T/DPMILIM.COM ::
{ sleep 6
  for c in 'A:\HISPADOS\HSPDPMI' 'DPMILIM S' 'DPMILIM B' 'DPMILIM L' 'DPMILIM T' 'DPMILIM I' 'DPMILIM T' 'ECHO fin-dpmilim'; do
    printf '%s\r' "$c"; sleep 3
  done; sleep 2
} | timeout 50 qemu-system-i386 -display none -serial stdio -monitor none -parallel none -m 16 \
      -drive file=$T/f.img,format=raw,if=floppy,index=0 -boot a -no-reboot > $T/raw 2>&1
tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 -c > $T/log.txt
check() { if grep -qF -- "$1" $T/log.txt; then echo "  OK   $1"; else echo "  FAIL $1"; fail=1; fi; }
checkn() { if grep -qF -- "$1" $T/log.txt; then echo "  FAIL (no debía salir) $1"; fail=1; else echo "  OK   sin «$1»"; fi; }
echo "== HSPDPMI: límites y restos de un cliente muerto =="
check "DPMILIM: 0300h CX=0700h -> 8021h"
check "DPMILIM: 0300h CX=4 bien"
check "DPMILIM: callback dejado"
check "DPMILIM: callback muerto, vuelto"
check "DPMILIM: INT 08h enganchada"
n=$(grep -cF "DPMILIM: reloj vivo" $T/log.txt)
if [ "$n" = 2 ]; then echo "  OK   el reloj sigue vivo (antes y después de la muerte en la IRQ 0)"
else echo "  FAIL el reloj: $n veces vivo (2)"; fail=1; fi
checkn "DPMILIM: reloj MUERTO"
check "fin-dpmilim"
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log.txt; fi
exit $fail
