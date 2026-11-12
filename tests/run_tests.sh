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
  send "BIN\\LOLTEST"
  send "dir.com"
  send "ECHO fin de la prueba"
  sleep 1.5
} | timeout 25 qemu-system-i386 -display none -serial stdio -monitor none \
      -drive file="$IMG",format=raw,if=floppy -boot a -no-reboot > "$LOG" 2>&1
tr -d '\r' < "$LOG" | iconv -f CP437 -t UTF-8 > "$LOG.txt"
fail=0
check() {
  if grep -qF -- "$1" "$LOG.txt"; then echo "  OK   $1"; else echo "  FAIL $1"; fail=1; fi
}
echo "== Hispa-DOS tests =="
check "Hispa-DOS 0.9 - núcleo compatible con MS-DOS 6.22"
check "Intérprete de órdenes de Hispa-DOS 0.9"
check "Versión de DOS notificada: 6.22"
check " El volumen de la unidad A es HISPADOS"
check " Directorio de A:\\"
check "COMMAND  COM"
check "<DIR>"
check "bytes libres"
check "This file was read through INT 21h AH=3Fh."
check " Directorio de A:\\DOCS"
check "Subdirectory file OK"
check "Hello from a relocated .EXE program!"
check "Hello from a .COM program! Args:[ uno dos]"
check "Hello from a .COM program! Args:[ tres]"
check "bytes del mayor programa ejecutable"
check "Comando o nombre de archivo incorrecto"
check "CDS A: A:\\ flags=4000"
check "DPB drive A: spc=0001 bps=0200 maxclus=0B20"
check "fin de la prueba"
# dir.com = DIR .COM = DIR *.COM (solo los .COM: en la raíz, COMMAND.COM)
if sed -n "/>dir.com/,/>ECHO fin/p" "$LOG.txt" | grep -q "        1 archivo(s)" && ! sed -n "/>dir.com/,/>ECHO fin/p" "$LOG.txt" | grep -q "README"; then echo "  OK   dir.com = DIR *.COM"; else echo "  FAIL dir.com"; fail=1; fi
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then echo "--- salida / output ---"; cat "$LOG.txt"; fi
rm -f "$LOG" "$LOG.txt"
exit $fail
