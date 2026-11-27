#!/bin/bash
# Prueba de los Virtual DMA Services (VDS, INT 4Bh AX=81xxh) de HSPEMM
# (docs/HSPEMM.md, «VDS»), con VDSTEST.COM (tests/progs/vdstest.asm):
#  1. Con HSPXMS + HSPEMM y DOS=HIGH,UMB: el bit 5 de 0040:007Bh, la versión
#     (8102h), bloquear memoria convencional (física = lineal) y superior
#     (física de más arriba del primer mega), los errores 02h (cruza 64 KB),
#     0Fh (8105h), 10h (bits reservados) y 0Ch (canal 9), y 810Bh/810Ch (ZF).
#  2. Sin HSPEMM: VDSTEST dice que no hay VDS (y no se cuelga).
# Virtual DMA Services test.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
[ -f build/VDSTEST.COM ] || { echo "falta build/VDSTEST.COM (make build/VDSTEST.COM)"; exit 1; }
T=$(mktemp -d)
trap 'rm -rf $T' EXIT
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
corre() {   # NOMBRE [CONFIG.SYS]
  cp "$IMG" $T/$1.img
  mcopy -o -i $T/$1.img build/VDSTEST.COM ::VDSTEST.COM
  [ -n "${2:-}" ] && { printf '%b' "$2" > $T/CONFIG.SYS; mcopy -o -i $T/$1.img $T/CONFIG.SYS ::CONFIG.SYS; }
  { sleep 8; printf 'VDSTEST\r'; sleep 6; printf 'ECHO fin-de-la-prueba\r'; sleep 2; } | \
    timeout 40 qemu-system-i386 -display none -serial stdio -monitor none -parallel none -m 32 \
      -drive file=$T/$1.img,format=raw,if=floppy -boot a -no-reboot -nic none > $T/$1.raw 2>&1
  tr -d '\r' < $T/$1.raw | iconv -f CP437 -t UTF-8 -c > $T/$1.txt
}
echo "== Hispa-DOS: VDS (Virtual DMA Services) de HSPEMM =="
corre emm 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nDEVICE=A:\\HISPADOS\\HSPEMM.SYS\r\nDOS=HIGH,UMB\r\nFILES=30\r\n' &
P1=$!
corre sin &
P2=$!
wait $P1 $P2
for l in "el bit 5 de 0040:007Bh: BIEN" "VDS version 01.00, producto 4853" \
         "memoria convencional (fisica = lineal): BIEN" "VDS soltarlos: BIEN" \
         "error 02h y 256 octetos: BIEN" "error 0Fh: BIEN" "error 10h: BIEN" \
         "(ZF = 1): BIEN" "canal 9: error 0Ch: BIEN" "VDS: todo bien"; do
  if grep -qF -- "$l" $T/emm.txt; then ok "con HSPEMM: $l"; else ko "con HSPEMM: $l"; fi
done
if grep -q "memoria superior (fisica distinta, de mas arriba del primer mega): [0-9A-F]\{8\} BIEN" $T/emm.txt; then
  ok "con HSPEMM: la memoria superior, a su física de verdad ($(grep -o 'mega): [0-9A-F]*' $T/emm.txt | cut -d' ' -f2))"
else ko "con HSPEMM: la memoria superior"; fi
if grep -qF "VDS: no hay" $T/sin.txt && grep -qF "fin-de-la-prueba" $T/sin.txt; then ok "sin HSPEMM: dice que no hay VDS y sigue"; else ko "sin HSPEMM: no hay VDS"; fi
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then for n in emm sin; do echo "---- $n"; grep -a 'VDS' $T/$n.txt; done; fi
exit $fail
