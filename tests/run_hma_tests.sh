#!/bin/bash
# Prueba de DOS=HIGH: el código del núcleo en la HMA.
#  1) DOS=LOW con HSPXMS: memoria convencional libre de referencia
#  2) DOS=HIGH,UMB con HSPXMS: MEM lo dice, hay unos 30 KB más de memoria
#     convencional, INT 21h AX=3306h e INT 2Fh AX=4A01h/4A02h contestan, y
#     todo sigue funcionando (ficheros, EXEC, lotes, XMS) aunque un programa
#     cierre la línea A20 a propósito (HMATEST /A y /C)
#  3) DOS=HIGH sin gestor XMS: aviso y el núcleo se queda abajo
#  4) DOS=HIGH con la HMA ya en uso (HMAUSO.SYS): aviso y abajo
#  5) DOS=HIGH con un gestor XMS que da la HMA y dice que A20 está abierta,
#     pero no lo está (HMAUSO.SYS /M): el núcleo lo comprueba y no se mueve
# Los programas están en tests/progs/hmatest.asm y hmauso.asm (make nucprogs).
# DOS=HIGH test: kernel code in the HMA.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
B=build
for f in HMATEST.COM HMAUSO.SYS; do
  [ -f $B/$f ] || make -s nucprogs
done
T=$(mktemp -d)
cp "$IMG" $T/base.img
mcopy -o -i $T/base.img $B/HMATEST.COM ::BIN/HMATEST.COM
mcopy -o -i $T/base.img $B/HMAUSO.SYS ::BIN/HMAUSO.SYS
printf '@ECHO OFF\r\nA:\\BIN\\HMATEST /C\r\nECHO lote-uno\r\nA:\\BIN\\HELLO lote\r\nA:\\BIN\\HMATEST /C\r\nCOPY A:\\DOCS\\INFO.TXT A:\\HMACOPIA.TXT\r\nA:\\BIN\\HMATEST /C\r\nTYPE A:\\HMACOPIA.TXT\r\nDEL A:\\HMACOPIA.TXT\r\nIF NOT EXIST A:\\HMACOPIA.TXT ECHO lote-fin\r\n' > $T/HMA.BAT
mcopy -o -i $T/base.img $T/HMA.BAT ::BAT/HMA.BAT

# arrancar con el CONFIG.SYS $2 y teclear las órdenes restantes -> $T/$1.txt
run() {
  local name=$1 cfg=$2; shift 2
  cp $T/base.img $T/f.img
  printf "$cfg" > $T/CONFIG.SYS
  mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
  { sleep 4
    for c in "$@"; do sleep 1.5; printf '%s\r' "$c"; done
    sleep 3
  } | timeout $((9 + 2 * $#)) qemu-system-i386 -display none -serial stdio -monitor none \
        -drive file=$T/f.img,format=raw,if=floppy -boot a -no-reboot > $T/$name.log 2>&1
  tr -d '\r' < $T/$name.log | iconv -f CP437 -t UTF-8 > $T/$name.txt
}
fail=0
OUT=
check()  { if grep -qF -- "$1" $OUT; then echo "  OK   $1"; else echo "  FAIL $1"; fail=1; fi; }
checkn() { if grep -qF -- "$1" $OUT; then echo "  FAIL (no debía salir) $1"; fail=1; else echo "  OK   sin \"$1\""; fi; }
num()    { grep -m1 "^$1=" $2 | sed 's/^[A-Z]*=\([0-9]*\).*/\1/'; }

echo "== Hispa-DOS DOS=HIGH: referencia con DOS=LOW =="
run low 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nDOS=LOW\r\n' "MEM" "A:\\BIN\\HMATEST" "A:\\BIN\\HMATEST /A"
OUT=$T/low.txt
check "HSPXMS 0.1: gestor XMS 3.0"
check "bytes del mayor programa ejecutable"
checkn "cargado en la memoria alta"
check "DOSHMA=0"
check "HMALIBRE=0 en FFFF:FFFF"
check "HMATEST A20 OK: 12 llamadas con A20 cerrada"
LOW=$(num LIBRE $OUT)

echo "== Hispa-DOS DOS=HIGH: el núcleo en la HMA =="
run high 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\ndos=high,umb\r\nFILES=30\r\n' \
    "MEM" "A:\\BIN\\HMATEST" "A:\\BIN\\HMATEST /A" "A:\\BIN\\HMATEST /C" \
    "DIR A:\\DOCS" "HELLOEXE" "A:\\BIN\\HMATEST /C" "CALL A:\\BAT\\HMA.BAT" \
    "A:\\BIN\\HMATEST /C" "A:\\BIN\\XMSTEST" "A:\\BIN\\HMATEST /C" "MEM /D" "A:\\BIN\\HMATEST" "VER"
OUT=$T/high.txt
check "HSPXMS 0.1: gestor XMS 3.0"
checkn "se carga en la memoria convencional"
check "Hispa-DOS está cargado en la memoria alta (HMA)"
check "DOSHMA=1"
check "HMA reserva OK"
check "A20=abierta"
check "Hello from a .COM program! Args:[ cerrada]"
check "HMATEST A20 OK: 12 llamadas con A20 cerrada"
if [ "$(grep -c '^A20=cerrada' $OUT)" -ge 7 ]; then echo "  OK   A20 cerrada a propósito (7 veces)"; else echo "  FAIL A20 cerrada a propósito (7 veces)"; fail=1; fi
check "INFO     TXT"
check "Hello from a relocated .EXE program!"
check "lote-uno"
check "Hello from a .COM program! Args:[ lote]"
check "Subdirectory file OK"
check "lote-fin"
check "XMS test OK"
check "XMS registers OK"
check "Núcleo de Hispa-DOS"
checkn "HMATEST FALLO"
checkn "no se cierra"
# la segunda vez, la HMA libre es menor (la reserva de la primera se queda)
H1=$(grep -m1 '^HMALIBRE=' $OUT | sed 's/HMALIBRE=\([0-9]*\).*/\1/')
H2=$(grep '^HMALIBRE=' $OUT | tail -1 | sed 's/HMALIBRE=\([0-9]*\).*/\1/')
if [ -n "$H1" ] && [ -n "$H2" ] && [ "$H1" -gt 16000 ] && [ $((H1 - H2)) -eq 112 ]; then
  echo "  OK   HMA libre para otros: $H1 bytes (y $H2 tras reservar 112)"
else echo "  FAIL HMA libre para otros ($H1, $H2)"; fail=1; fi
HIGH=$(num LIBRE $OUT)
GAIN=$(( ${HIGH:-0} - ${LOW:-0} ))
if [ -n "$LOW" ] && [ -n "$HIGH" ] && [ $GAIN -ge 30000 ]; then
  echo "  OK   memoria convencional libre: $LOW con DOS=LOW, $HIGH con DOS=HIGH (+$GAIN bytes)"
else echo "  FAIL memoria convencional libre: $LOW con DOS=LOW, $HIGH con DOS=HIGH (+$GAIN bytes)"; fail=1; fi

echo "== Hispa-DOS DOS=HIGH sin gestor XMS =="
run noxms 'DOS=HIGH\r\n' "MEM" "A:\\BIN\\HMATEST" "A:\\BIN\\HELLO abajo" "CALL A:\\BAT\\PRUEBA.BAT"
OUT=$T/noxms.txt
check "No hay gestor de memoria extendida (XMS): Hispa-DOS se carga en la memoria convencional."
checkn "cargado en la memoria alta"
check "DOSHMA=0"
check "HMALIBRE=0 en FFFF:FFFF"
check "Hello from a .COM program! Args:[ abajo]"
check "100% hecho"
NOX=$(num LIBRE $OUT)
# sin HMA el código ocupa como con DOS=LOW o algo menos (se sueltan los datos
# de arranque, unos 5 KB; y aquí no está HSPXMS, otros 2 KB)
D=$(( ${NOX:-0} - ${LOW:-0} ))
if [ -n "$NOX" ] && [ $D -ge 0 ] && [ $D -le 9000 ]; then echo "  OK   sin HMA no se pierde memoria ($NOX bytes libres)"
else echo "  FAIL sin HMA no se pierde memoria ($NOX frente a $LOW)"; fail=1; fi

echo "== Hispa-DOS DOS=HIGH con la HMA en uso =="
run uso 'DEVICE=A:\\BIN\\HMAUSO.SYS\r\nDOS=HIGH\r\n' "MEM" "A:\\BIN\\HMATEST" "A:\\BIN\\HELLO abajo" "HELLOEXE"
OUT=$T/uso.txt
check "HMAUSO: gestor XMS de prueba con la HMA en uso"
check "La memoria alta (HMA) no está disponible: Hispa-DOS se carga en la memoria convencional."
checkn "cargado en la memoria alta"
check "DOSHMA=0"
check "Hello from a .COM program! Args:[ abajo]"
check "Hello from a relocated .EXE program!"

echo "== Hispa-DOS DOS=HIGH con la línea A20 cerrada de verdad =="
run a20 'DEVICE=A:\\BIN\\HMAUSO.SYS /M\r\nDOS=HIGH\r\n' "MEM" "A:\\BIN\\HMATEST" "A:\\BIN\\HELLO abajo" "HELLOEXE" "CALL A:\\BAT\\PRUEBA.BAT"
OUT=$T/a20.txt
check "HMAUSO /M: da la HMA con la linea A20 cerrada"
check "La memoria alta (HMA) no está disponible: Hispa-DOS se carga en la memoria convencional."
checkn "cargado en la memoria alta"
check "DOSHMA=0"
check "Hello from a .COM program! Args:[ abajo]"
check "Hello from a relocated .EXE program!"
check "100% hecho"

if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then
  for f in low high noxms uso a20; do echo "--- $f ---"; cat $T/$f.txt; done
fi
rm -rf $T
exit $fail
