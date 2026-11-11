#!/bin/bash
# Prueba del SHARE del núcleo (BIN\SHARETST): INT 2Fh 1000h, modos de
# compartición al abrir y bloqueos de zonas (INT 21h 5Ch).
# Built-in SHARE test.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
cp "$IMG" $T/f.img
{ sleep 5; printf 'BIN\\SHARETST\r'; sleep 3; } | timeout 15 qemu-system-i386 -display none -serial stdio \
  -monitor none -parallel none -drive file=$T/f.img,format=raw,if=floppy -boot a -no-reboot > $T/log 2>&1
tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 > $T/log.txt
fail=0
check() { if grep -qF -- "$1" $T/log.txt; then echo "  OK   $1"; else echo "  FAIL $1"; fail=1; fi; }
echo "== Hispa-DOS SHARE =="
check "SHARE: instalado"
check "Compartir: denegado (20h)"
check "Tras cerrar: se abre"
check "Compatibilidad: dos aperturas"
check "Bloqueo 1: concedido"
check "Bloqueo 2: choque (21h)"
check "Bloqueo 3: concedido tras desbloquear"
check "Fin de la prueba de SHARE"
if mdir -i $T/f.img ::T.DAT >/dev/null 2>&1; then echo "  FAIL T.DAT no se borró"; fail=1; else echo "  OK   T.DAT borrado"; fi
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log.txt; fi
rm -rf $T
exit $fail
