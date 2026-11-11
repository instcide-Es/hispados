#!/bin/bash
# Prueba del gestor XMS HSPXMS.SYS: carga, XMSTEST (reservar, mover, bloquear,
# liberar) y segunda carga rechazada. Si están los programas de FreeDOS,
# también MEM debe ver la memoria XMS.
# XMS driver test.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
FLP=$(mktemp --suffix=.img); cp "$IMG" "$FLP"
printf 'DEVICE=A:\\DRIVERS\\HSPXMS.SYS\r\nDEVICE=A:\\DRIVERS\\HSPXMS.SYS\r\n' > build/cfg_xms_test.sys
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
tr -d '\r' < "$LOG" > "$LOG.txt"
fail=0
check() {
  if grep -qF -- "$1" "$LOG.txt"; then echo "  OK   $1"; else echo "  FAIL $1"; fail=1; fi
}
echo "== HispaDOS XMS tests =="
check "HSPXMS 0.1: XMS 3.0 driver"
check "HSPXMS: an XMS driver is already installed"
checkn() { if grep -qF -- "$1" "$LOG.txt"; then echo "  FAIL (no debía salir) $1"; fail=1; else echo "  OK   sin \"$1\""; fi; }
checkn "Error initializing"
check "XMS version 0300"
check "Locked at linear 00110000"
if [ "$(grep -c 'XMS test OK' "$LOG.txt")" -ge 2 ]; then echo "  OK   XMS test OK (dos veces)"; else echo "  FAIL XMS test OK (dos veces)"; fail=1; fi
[ $HAVEMEM = 1 ] && check "Extended (XMS)"
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then echo "--- salida ---"; cat "$LOG.txt"; fi
rm -f "$FLP" "$LOG" "$LOG.txt"
exit $fail
