#!/bin/bash
# Prueba del arranque: F5 (limpio) y F8 (menú de arranque: desmarcar una línea
# de CONFIG.SYS y AUTOEXEC.BAT; AUTOEXEC.BAT paso a paso, línea a línea con
# [S/N] y Esc para el resto), con teclas reales durante el arranque.
# Boot keys test: F5 clean boot and the F8 boot menu.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
cp "$IMG" $T/base.img
printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nDEVICE=A:\\HISPADOS\\HSPTEST.SYS\r\nFILES=30\r\n' > $T/CONFIG.SYS
mcopy -o -i $T/base.img $T/CONFIG.SYS ::CONFIG.SYS
# AUTOEXEC.BAT con órdenes que se ven (TYPE) para el paso a paso
printf '@ECHO OFF\r\nREM paso a paso\r\nECHO Ejecutando AUTOEXEC.BAT\r\nTYPE A:\\UNO.TXT\r\nTYPE A:\\DOS.TXT\r\nTYPE A:\\TRES.TXT\r\nTYPE A:\\CUATRO.TXT\r\nECHO fin del AUTOEXEC\r\n' > $T/AUTOEXEC.BAT
mcopy -o -i $T/base.img $T/AUTOEXEC.BAT ::AUTOEXEC.BAT
for n in uno dos tres cuatro; do
  printf 'contenido-%s\r\n' $n > $T/$n.txt
  mcopy -o -i $T/base.img $T/$n.txt ::$(echo $n | tr a-z A-Z).TXT
done
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
# AUTOEXEC.BAT paso a paso (Espacio dos veces: «?»): UNO sí, DOS no, Esc
boot f8p f8 down down down spc spc ret s n esc
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
# F8 paso a paso: ECHO y REM sin preguntar; DOS.TXT no; Esc: el resto sin preguntar
yes f8p "HSPTEST"
yes f8p "Ejecutando AUTOEXEC.BAT"
no  f8p "REM paso a paso [S/N]"
no  f8p "ECHO Ejecutando AUTOEXEC.BAT [S/N]"
yes f8p "TYPE A:\\UNO.TXT [S/N]? S"
yes f8p "contenido-uno"
yes f8p "TYPE A:\\DOS.TXT [S/N]? N"
no  f8p "contenido-dos"
yes f8p "TYPE A:\\TRES.TXT [S/N]?"
yes f8p "contenido-tres"
no  f8p "CUATRO.TXT [S/N]"
yes f8p "contenido-cuatro"
yes f8p "fin del AUTOEXEC"
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/f5.txt; echo ---; cat $T/f8.txt; echo ---; cat $T/f8p.txt; fi
rm -rf $T
exit $fail
