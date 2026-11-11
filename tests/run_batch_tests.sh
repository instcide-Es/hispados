#!/bin/bash
# Prueba de ficheros por lotes y del entorno: AUTOEXEC.BAT, parámetros, IF,
# GOTO, FOR, SHIFT, CALL, SET, PATH, PROMPT y COMMAND /C.
# Batch file and environment test.
set -u
IMG=${1:-build/hispados.img}
FLP=$(mktemp --suffix=.img); cp "$IMG" "$FLP"
LOG=$(mktemp)
send() { sleep "${2:-0.7}"; printf '%s\r' "$1"; }
{
  sleep 3.5
  send "SET"
  send "A:\\BAT\\PRUEBA uno dos" 3
  send "PATH"
  send "PATH A:\\BIN;A:\\"
  send "UPPER < README.TXT" 1
  send "PROMPT [\$N] \$P\$G"
  send "ECHO"
  send "COMMAND /C ECHO desde barra C" 1.5
  send "EXIT"
  send "ECHO sigo vivo"
  send "SET HISPA="
  send "SET" 1
  sleep 1
} | timeout 30 qemu-system-i386 -display none -serial stdio -monitor none \
      -drive file="$FLP",format=raw,if=floppy -boot a -no-reboot > "$LOG" 2>&1
tr -d '\r' < "$LOG" > "$LOG.txt"
fail=0
check() {
  if grep -qF -- "$1" "$LOG.txt"; then echo "  OK   $1"; else echo "  FAIL $1"; fail=1; fi
}
checkn() {
  if grep -qF -- "$1" "$LOG.txt"; then echo "  FAIL (no debía salir) $1"; fail=1; else echo "  OK   sin \"$1\""; fi
}
echo "== HispaDOS batch tests =="
check "Ejecutando AUTOEXEC.BAT"
check "HISPA=funciona"
check "COMSPEC=A:\\COMMAND.COM"
check "Param 1=uno 2=dos"
check "Variable=funciona"
check "IF cadena OK"
check "IF NOT OK"
check "IF EXIST OK"
check "IF NOT EXIST OK"
check "Hello from a .COM program! Args:[ x]"
check "ERRORLEVEL 7 OK"
checkn "ERRORLEVEL 8 MAL"
checkn "esto no debe salir"
check "GOTO OK"
check "FOR uno"
check "FOR tres"
check "fichero A:\\DOCS\\INFO.TXT"
check "tras SHIFT dos"
check "OTRA recibe desde-call"
check "vuelta de CALL"
check "100% hecho"
checkn "@ECHO OFF"
check "PATH=A:\\"
check "WELCOME TO HISPADOS!"
check "[A] A:\\>"
check "ECHO is on"
check "desde barra C"
check "sigo vivo"
# tras SET HISPA= la variable ya no debe aparecer en el último SET
if sed -n '/SET HISPA=$/,$p' "$LOG.txt" | tail -n +2 | grep -q "HISPA="; then echo "  FAIL SET HISPA= no la borró"; fail=1; else echo "  OK   SET HISPA= la borra"; fi
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then echo "--- salida ---"; cat "$LOG.txt"; fi
rm -f "$FLP" "$LOG" "$LOG.txt"
exit $fail
