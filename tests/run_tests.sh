#!/bin/bash
# Prueba de extremo a extremo: arranca la imagen en QEMU y la maneja por COM1.
# End-to-end test: boots the image in QEMU and drives it through COM1.
set -u
IMG=${1:-build/hispados.img}
LOG=$(mktemp)
send() { sleep "${2:-0.7}"; printf '%s\r' "$1"; }
{
  sleep 3
  send "VER"
  send "DIR"
  send "TYPE README.TXT"
  send "CD DOCS"
  send "DIR"
  send "TYPE INFO.TXT"
  send "HELLOEXE"
  send "CD .."
  send "BIN\\HELLO uno dos"
  send "BIN\\HELLO.COM tres"
  send "MEM"
  send "NOEXISTE"
  send "ECHO fin de la prueba"
  sleep 1.5
} | timeout 25 qemu-system-i386 -display none -serial stdio -monitor none \
      -drive file="$IMG",format=raw,if=floppy -boot a -no-reboot > "$LOG" 2>&1
tr -d '\r' < "$LOG" > "$LOG.txt"
fail=0
check() {
  if grep -qF -- "$1" "$LOG.txt"; then echo "  OK   $1"; else echo "  FAIL $1"; fail=1; fi
}
echo "== HispaDOS tests =="
check "HispaDOS 0.2 - MS-DOS 6.22 compatible kernel"
check "HispaDOS Command Interpreter 0.2"
check "Reported DOS version: 6.22"
check " Volume in drive A is HISPADOS"
check " Directory of A:\\"
check "COMMAND  COM"
check "<DIR>"
check "bytes free"
check "This file was read through INT 21h AH=3Fh."
check " Directory of A:\\DOCS"
check "Subdirectory file OK"
check "Hello from a relocated .EXE program!"
check "Hello from a .COM program! Args:[ uno dos]"
check "Hello from a .COM program! Args:[ tres]"
check "bytes largest executable program size"
check "Bad command or file name"
check "fin de la prueba"
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then echo "--- salida / output ---"; cat "$LOG.txt"; fi
rm -f "$LOG" "$LOG.txt"
exit $fail
