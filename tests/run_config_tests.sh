#!/bin/bash
# Prueba de CONFIG.SYS y drivers: DEVICE= (carácter y bloque), SET, INSTALL,
# SHELL, LASTDRIVE, errores de línea y driver inexistente.
# CONFIG.SYS and device driver test.
set -u
IMG=${1:-build/hispados.img}
FLP=$(mktemp --suffix=.img); cp "$IMG" "$FLP"
mcopy -o -i "$FLP" tests/config/CONFIG.SYS ::CONFIG.SYS
LOG=$(mktemp)
send() { sleep "${2:-0.7}"; printf '%s\r' "$1"; }
{
  sleep 4
  send "SET"
  send "TYPE HSPTEST\$"
  send "ECHO hola driver > HSPTEST\$"
  send "TYPE HSPTEST\$"
  send "C:"
  send "DIR" 1
  send "COPY A:\\README.TXT C:\\" 1
  send "TYPE C:\\README.TXT"
  send "MD C:\\TMP"
  send "COPY A:\\HELLOEXE.EXE C:\\TMP" 1
  send "C:\\TMP\\HELLOEXE"
  send "A:"
  send "MEM" 1
  sleep 1
} | timeout 30 qemu-system-i386 -display none -serial stdio -monitor none \
      -drive file="$FLP",format=raw,if=floppy -boot a -no-reboot > "$LOG" 2>&1
tr -d '\r' < "$LOG" > "$LOG.txt"
fail=0
check() {
  if grep -qF -- "$1" "$LOG.txt"; then echo "  OK   $1"; else echo "  FAIL $1"; fail=1; fi
}
echo "== HispaDOS CONFIG.SYS tests =="
check "HSPTEST.SYS: test device installed"
check "HispaDOS RAM disk: 128 KB as drive C:"
check "Bad or missing A:\\NOEXISTE.SYS"
check "Error in CONFIG.SYS line 8"
check "Unrecognized command in CONFIG.SYS"
check "Error in CONFIG.SYS line 9"
check "Hello from a .COM program! Args:[ install]"
check "CONFIGVAR=desde-config"
check "COMSPEC=A:\\COMMAND.COM"
check "HispaDOS test driver OK"
check "hola driver"
check " Volume in drive C is HSPRAMDISK"
check "1 file(s) copied"
check "Welcome to HispaDOS!"
check "Hello from a relocated .EXE program!"
check "bytes largest executable program size"
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then echo "--- salida ---"; cat "$LOG.txt"; fi
rm -f "$FLP" "$LOG" "$LOG.txt"
exit $fail
