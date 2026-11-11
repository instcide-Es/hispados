#!/bin/bash
# Prueba del arranque: F5 (limpio) y F8 (menú de arranque: desmarcar una línea
# de CONFIG.SYS y AUTOEXEC.BAT), con teclas reales durante el arranque.
# Boot keys test: F5 clean boot and the F8 boot menu.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
cp "$IMG" $T/base.img
printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nDEVICE=A:\\HISPADOS\\HSPTEST.SYS\r\nFILES=30\r\n' > $T/CONFIG.SYS
mcopy -o -i $T/base.img $T/CONFIG.SYS ::CONFIG.SYS
# arranca, pulsa $1 durante el arranque y después el resto de teclas
boot() {
  local out=$1 k=$2; shift 2
  cp $T/base.img $T/f.img
  qemu-system-i386 -display none -monitor unix:$T/mon,server,nowait -parallel none \
    -serial file:$T/$out.log -drive file=$T/f.img,format=raw,if=floppy -boot a &
  local qp=$!
  sleep 0.5
  for i in $(seq 1 14); do printf 'sendkey %s\n' $k | socat - UNIX-CONNECT:$T/mon >/dev/null 2>&1; sleep 0.25; done
  sleep 1
  printf 'screendump %s\n' $T/$out.ppm | socat - UNIX-CONNECT:$T/mon >/dev/null 2>&1
  for x in "$@"; do printf 'sendkey %s\n' $x | socat - UNIX-CONNECT:$T/mon >/dev/null 2>&1; sleep 0.3; done
  sleep 4
  kill $qp 2>/dev/null; wait $qp 2>/dev/null
  tr -d '\r' < $T/$out.log | iconv -f CP437 -t UTF-8 > $T/$out.txt
}
boot f5 f5
boot f8 f8 down spc down down down spc ret
fail=0
yes() { if grep -qF -- "$2" $T/$1.txt; then echo "  OK   $1: $2"; else echo "  FAIL $1: $2"; fail=1; fi; }
no()  { if grep -qF -- "$2" $T/$1.txt; then echo "  FAIL $1: sobra $2"; fail=1; else echo "  OK   $1: sin $2"; fi; }
echo "== Hispa-DOS arranque (F5 y F8) =="
yes f5 "F5 = arranque limpio"
yes f5 "Arranque limpio: sin CONFIG.SYS ni AUTOEXEC.BAT."
no  f5 "HSPXMS 0.1"
no  f5 "Ejecutando AUTOEXEC.BAT"
yes f5 "Intérprete de órdenes de Hispa-DOS"
# F8: se desmarcan HSPTEST.SYS (2.ª línea) y AUTOEXEC.BAT (último elemento)
yes f8 "HSPXMS 0.1"
no  f8 "Controlador de prueba"
no  f8 "Ejecutando AUTOEXEC.BAT"
yes f8 "Intérprete de órdenes de Hispa-DOS"
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/f5.txt; echo ---; cat $T/f8.txt; fi
rm -rf $T
exit $fail
