#!/bin/bash
# Prueba de compatibilidad con programas reales de FreeDOS (GPL), descargados
# con tests/thirdparty/fetch.sh. Si no están, la prueba se salta.
# Compatibility test with real FreeDOS programs (skipped if not downloaded).
set -u
cd "$(dirname "$0")/.."
if [ ! -f tests/thirdparty/dl/edit/.ok ]; then
  echo "== HispaDOS compatibility tests: omitida (ejecuta tests/thirdparty/fetch.sh) =="
  exit 0
fi
tests/thirdparty/mkimg.sh > /dev/null
IMG=$(mktemp --suffix=.img); cp build/compat.img "$IMG"
printf 'r\r\ng\r\nq\r\n' > build/dbg.txt
mcopy -o -i "$IMG" build/dbg.txt ::DBG.TXT
CMDS=$(mktemp)
cat > "$CMDS" <<'EOF'
A:\FD\SORT < A:\DATOS\LISTA.TXT
A:\FD\FIND /C "a" A:\DATOS\LISTA.TXT
A:\FD\TREE A:\
A:\FD\ATTRIB +R A:\DATOS\INFO.TXT
DEL A:\DATOS\INFO.TXT
A:\FD\ATTRIB -R A:\DATOS\INFO.TXT
A:\FD\MEM /C
A:\FD\DEBUG A:\FD\TREE.COM A:\ < A:\DBG.TXT
A:\FD\XCOPY A:\DATOS A:\COPIA\ /S /I
A:\FD\FC A:\DATOS\LISTA.TXT A:\COPIA\LISTA.TXT
A:\FD\DELTREE /Y A:\COPIA
A:\FD\LABEL A: PRUEBA
VOL
A:\FD\CHOICE /C:SN Elige
S
IF ERRORLEVEL 1 ECHO choice ok
TYPE A:\DATOS\LISTA.TXT | A:\FD\MORE
ECHO fin compat
EOF
LOG=$(mktemp)
N=$(wc -l < "$CMDS")
{
  sleep 4
  while IFS= read -r c; do printf '%s\r' "$c"; sleep 2.5; done < "$CMDS"
  sleep 2
} | timeout $((15 + N * 4)) qemu-system-i386 -display none -serial stdio -monitor none \
      -drive file="$IMG",format=raw,if=floppy -boot a -no-reboot > "$LOG" 2>&1
tr -d '\r' < "$LOG" | iconv -f CP437 -t UTF-8 > "$LOG.txt"
fail=0
check() {
  if grep -qF -- "$1" "$LOG.txt"; then echo "  OK   $1"; else echo "  FAIL $1"; fail=1; fi
}
checkn() {
  if grep -qF -- "$1" "$LOG.txt"; then echo "  FAIL (no debía salir) $1"; fail=1; else echo "  OK   sin \"$1\""; fi
}
echo "== HispaDOS compatibility tests (FreeDOS programs) =="
checkn "Error reading NLS"
check "alfa"
check "---------------- LISTA.TXT"
check "Directory PATH listing for Volume COMPAT"
check "[---RA] A:\\DATOS\\INFO.TXT"
check "Acceso denegado"
check "COMMAND     "
check "Largest executable program size"
check "Program terminated normally"
check "2 file(s) copied"
check "No differences"
check "Deleting \"A:\\COPIA\""
checkn "DOS error"
check " El volumen de la unidad A es PRUEBA"
check "choice ok"
check "medio"
check "fin compat"
checkn "El proceso raíz ha terminado"
if fsck.fat -n "$IMG" > "$LOG.fsck" 2>&1; then echo "  OK   fsck.fat tras los programas"; else echo "  FAIL fsck.fat"; cat "$LOG.fsck"; fail=1; fi
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then echo "--- salida ---"; cat "$LOG.txt"; fi
rm -f "$IMG" "$LOG" "$LOG.txt" "$LOG.fsck" "$CMDS"
exit $fail
