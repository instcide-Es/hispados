#!/bin/bash
# Prueba del gestor XMS HSPXMS.SYS: carga, XMSTEST (reservar, mover, bloquear,
# liberar) y segunda carga rechazada. Si están los programas de FreeDOS,
# también MEM debe ver la memoria XMS.
# XMS driver test.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
FLP=$(mktemp --suffix=.img); cp "$IMG" "$FLP"
printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nDEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\n' > build/cfg_xms_test.sys
mcopy -o -i "$FLP" build/cfg_xms_test.sys ::CONFIG.SYS
HAVEMEM=0
if [ -f tests/thirdparty/dl/mem/.ok ]; then
  mcopy -o -i "$FLP" tests/thirdparty/dl/mem/BIN/MEM.EXE ::BIN/FDMEM.EXE
  HAVEMEM=1
fi
LOG=$(mktemp)
send() { sleep "${2:-1.5}"; printf '%s\r' "$1"; }
{
  sleep 4
  send "A:\\BIN\\XMSTEST" 2
  send "A:\\BIN\\XMSTEST" 2
  [ $HAVEMEM = 1 ] && send "A:\\BIN\\FDMEM" 2
  sleep 2
} | timeout 25 qemu-system-i386 -display none -serial stdio -monitor none \
      -drive file="$FLP",format=raw,if=floppy -boot a -no-reboot > "$LOG" 2>&1
tr -d '\r' < "$LOG" | iconv -f CP437 -t UTF-8 > "$LOG.txt"
fail=0
check() {
  if grep -qF -- "$1" "$LOG.txt"; then echo "  OK   $1"; else echo "  FAIL $1"; fail=1; fi
}
echo "== Hispa-DOS XMS tests =="
check "HSPXMS 0.1: gestor XMS 3.0"
check "HSPXMS: ya hay un gestor XMS instalado"
checkn() { if grep -qF -- "$1" "$LOG.txt"; then echo "  FAIL (no debía salir) $1"; fail=1; else echo "  OK   sin \"$1\""; fi; }
checkn "Error al iniciar"
check "XMS version 0300"
# (el primer bloque de XMS es del intérprete: guarda en él su cuerpo mientras
# corre un programa; el de la prueba va detrás, pasado 1 MB + HMA)
if grep -qE "Locked at linear 0011[0-9A-F]{4}" "$LOG.txt"; then echo "  OK   Locked at linear 0011xxxx"; else echo "  FAIL Locked at linear 0011xxxx"; fail=1; fi
if [ "$(grep -c 'XMS test OK' "$LOG.txt")" -ge 2 ]; then echo "  OK   XMS test OK (dos veces)"; else echo "  FAIL XMS test OK (dos veces)"; fail=1; fi
check "XMS realloc OK"
check "XMS overlap OK"
check "XMS errors OK"
check "XMS registers OK"
[ $HAVEMEM = 1 ] && check "Extended (XMS)"
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then echo "--- salida ---"; cat "$LOG.txt"; fi
rm -f "$FLP" "$LOG" "$LOG.txt"
exit $fail
