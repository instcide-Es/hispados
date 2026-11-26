#!/bin/bash
# Crear (3Ch, 16h) un archivo que está abierto: violación de compartición
# (20h) en vez de vaciarlo con el otro handle leyendo grupos ya libres; tras
# cerrarlo, o si solo lo tiene abierto por FCB el mismo proceso, sí se crea.
# El disco queda limpio (fsck.fat) y CA.BIN con lo último escrito.
# Creating an open file -> sharing violation.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
nasm -f bin -o $T/CREARAB.COM tests/progs/crearab.asm || exit 1
cp "$IMG" $T/f.img
mcopy -o -i $T/f.img $T/CREARAB.COM ::CREARAB.COM
{ sleep 5; printf 'CREARAB\r'; sleep 4; } | timeout 20 qemu-system-i386 -display none -serial stdio \
  -monitor none -parallel none -drive file=$T/f.img,format=raw,if=floppy -boot a -no-reboot > $T/log 2>&1
tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 > $T/log.txt
fail=0
check() { if grep -qF -- "$1" $T/log.txt; then echo "  OK   $1"; else echo "  FAIL $1"; fail=1; fi; }
echo "== Hispa-DOS crear un archivo abierto =="
check "CREAR-ABIERTO: 3Ch denegado (20h)"
check "CREAR-ABIERTO: 16h denegado"
check "CREAR-ABIERTO: el handle abierto lee sus 3000 octetos"
check "CREAR-ABIERTO: tras cerrar, 3Ch aceptado"
check "CREAR-ABIERTO: sobre un FCB propio sin cerrar, aceptado"
check "Fin de CREARAB"
n=$(mdir -i $T/f.img ::CA.BIN 2>/dev/null | grep -i "^CA *BIN" | awk '{print $3}')
if [ "$n" = "700" ]; then echo "  OK   CA.BIN con 700 octetos"; else echo "  FAIL CA.BIN mide «$n» (700)"; fail=1; fi
if /usr/sbin/fsck.fat -n $T/f.img > $T/fsck.txt 2>&1; then echo "  OK   fsck.fat limpio"
else echo "  FAIL fsck.fat"; cat $T/fsck.txt; fail=1; fi
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log.txt; fi
exit $fail
