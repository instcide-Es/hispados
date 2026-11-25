#!/bin/bash
# HSPEMM: límites de las zonas del programa. 57h con una zona convencional
# que se sale del primer mega (desde o hacia FFFF:0010) -> A2h; 4Dh, 4Eh,
# 4Fh, 54h, DE08h y DE01h con ES:DI al final de FFFF:FFFF -> A3h, como MS
# EMM386, en vez de escribir sobre el directorio de páginas, la IDT o la GDT
# del monitor. Las llamadas buenas siguen yendo, y el equipo sigue vivo.
# HSPEMM bounds checks (EMMLIM.COM).
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
fail=0
nasm -f bin -o $T/EMMLIM.COM tests/progs/emmlim.asm || exit 1
cp "$IMG" $T/f.img
printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nDEVICE=A:\\HISPADOS\\HSPEMM.SYS\r\n' > $T/CONFIG.SYS
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
mcopy -o -i $T/f.img $T/EMMLIM.COM ::EMMLIM.COM
{ sleep 6; printf 'EMMLIM\r'; sleep 3; printf 'MEM\r'; sleep 2; printf 'ECHO tras-emmlim\r'; sleep 2; } |
  timeout 25 qemu-system-i386 -display none -serial stdio -monitor none -parallel none -m 32 -net none \
    -drive file=$T/f.img,format=raw,if=floppy -boot a -no-reboot > $T/raw 2>&1
tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 -c > $T/log.txt
check() { if grep -qF -- "$1" $T/log.txt; then echo "  OK   $1"; else echo "  FAIL $1"; fail=1; fi; }
echo "== HSPEMM: límites de las zonas del programa =="
check "HSPEMM 0.1: EMS 4.0 y VCPI, "
check "EMMLIM 0=00"
check "EMMLIM A=A2"
check "EMMLIM B=A2"
check "EMMLIM 1=00"
check "EMMLIM C=A3"
check "EMMLIM D=A3"
check "EMMLIM E=A3"
check "EMMLIM F=A3"
check "EMMLIM G=A3"
check "EMMLIM H=A3"
check "EMMLIM: sigue vivo"
check "tras-emmlim"
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log.txt; fi
exit $fail
