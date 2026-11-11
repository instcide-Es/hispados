#!/bin/bash
# Prueba de escritura: MD, COPY, COPY CON, redirecciones, tuberías, REN, DEL, RD
# en A: (FAT12) y en C: (FAT16). Al final fsck.fat comprueba los dos discos.
# Write test on A: (FAT12) and C: (FAT16), then fsck.fat checks both disks.
set -u
SRC=${1:-build/hispados.img}
FLP=$(mktemp --suffix=.img); cp "$SRC" "$FLP"
HDD=$(mktemp --suffix=.img); dd if=/dev/zero of="$HDD" bs=1M count=64 status=none
LOG=$(mktemp); LOG2=$(mktemp)
send() { sleep "${2:-0.6}"; printf '%s\r' "$1"; }
{
  sleep 3
  send "MD PRUEBA"
  send "CD PRUEBA"
  send "COPY CON NOTA.TXT"
  send "linea uno"
  send "linea dos"
  printf '\032\r'
  send "TYPE NOTA.TXT" 1
  send "COPY NOTA.TXT COPIA.TXT" 1
  send "TYPE COPIA.TXT"
  send "DIR > LISTA.TXT" 1
  send "ECHO linea anadida >> LISTA.TXT"
  send "TYPE LISTA.TXT" 1
  send "TYPE NOTA.TXT | A:\\BIN\\UPPER" 1.5
  send "A:\\BIN\\UPPER < COPIA.TXT" 1
  send "REN *.TXT *.BAK" 1
  send "DIR" 1
  send "DEL COPIA.BAK"
  send "CD .."
  send "RD PRUEBA"
  send "DEL PRUEBA\\*.*"
  send "Y" 1
  send "RD PRUEBA" 1
  send "DIR PRUEBA" 1
  send "FDISK /PRI" 2
  send "FORMAT C: /S /AUTOTEST" 3
  send "MD C:\\DATOS" 1
  send "COPY A:\\*.* C:\\DATOS" 4
  send "COPY A:\\BIN\\*.COM C:\\DATOS" 2
  send "C:" 1
  send "CD DATOS"
  send "DIR" 2
  send "TYPE README.TXT" 1
  send "COPY KERNEL.SYS K2.SYS" 2
  send "COPY K2.SYS+KERNEL.SYS" 1
  send "ECHO fin escritura"
  sleep 2
} | timeout 75 qemu-system-i386 -display none -serial stdio -monitor none \
      -drive file="$FLP",format=raw,if=floppy \
      -drive file="$HDD",format=raw,if=ide -boot a -no-reboot > "$LOG" 2>&1
# arranque desde C: para ver que todo sigue ahí
{
  sleep 3
  send "DIR C:\\DATOS" 2
  sleep 1
} | timeout 15 qemu-system-i386 -display none -serial stdio -monitor none \
      -drive file="$HDD",format=raw,if=ide -boot c -no-reboot > "$LOG2" 2>&1
tr -d '\r' < "$LOG" | iconv -f CP437 -t UTF-8 > "$LOG.txt"; tr -d '\r' < "$LOG2" | iconv -f CP437 -t UTF-8 > "$LOG2.txt"
fail=0
check() {
  if grep -qF -- "$2" "$1"; then echo "  OK   $2"; else echo "  FAIL $2"; fail=1; fi
}
checkn() {
  if grep -qF -- "$2" "$1"; then echo "  FAIL (no debía salir) $2"; fail=1; else echo "  OK   sin \"$2\""; fi
}
echo "== HispaDOS write tests =="
check "$LOG.txt" "linea uno"
check "$LOG.txt" "linea dos"
check "$LOG.txt" "1 archivo(s) copiado(s)"
check "$LOG.txt" "NOTA     TXT"
check "$LOG.txt" "linea anadida"
check "$LOG.txt" "LINEA UNO"
check "$LOG.txt" "LINEA DOS"
check "$LOG.txt" "NOTA     BAK"
check "$LOG.txt" "LISTA    BAK"
check "$LOG.txt" "o el directorio no está vacío"
check "$LOG.txt" "¡Se borrarán todos los archivos del directorio!"
check "$LOG.txt" "No se encontró el archivo"
check "$LOG.txt" "Formato completado."
check "$LOG.txt" "archivo(s) copiado(s)"
check "$LOG.txt" " Directorio de C:\\DATOS"
check "$LOG.txt" "HELLOEXE EXE"
check "$LOG.txt" "UPPER    COM"
check "$LOG.txt" "Welcome to HispaDOS!"
check "$LOG.txt" "fin escritura"
checkn "$LOG.txt" "Acceso denegado"
check "$LOG2.txt" " Directorio de C:\\DATOS"
check "$LOG2.txt" "K2       SYS"
echo "-- comprobación con herramientas de Linux --"
if fsck.fat -n "$FLP" > "$LOG.fsck" 2>&1; then echo "  OK   fsck.fat A: (FAT12)"; else echo "  FAIL fsck.fat A:"; cat "$LOG.fsck"; fail=1; fi
PART=$(mktemp --suffix=.img)
dd if="$HDD" of="$PART" bs=512 skip=63 status=none
if fsck.fat -n "$PART" > "$LOG.fsck" 2>&1; then echo "  OK   fsck.fat C: (FAT16)"; else echo "  FAIL fsck.fat C:"; cat "$LOG.fsck"; fail=1; fi
if mtype -i "$PART" ::DATOS/README.TXT 2>/dev/null | grep -q "Welcome to HispaDOS"; then echo "  OK   mtype C:\\DATOS\\README.TXT"; else echo "  FAIL mtype"; fail=1; fi
if cmp -s <(mtype -i "$PART" ::DATOS/K2.SYS 2>/dev/null) <(cat build/KERNEL.SYS build/KERNEL.SYS); then echo "  OK   K2.SYS = KERNEL.SYS + KERNEL.SYS"; else echo "  FAIL contenido de K2.SYS"; fail=1; fi
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then echo "--- salida ---"; cat "$LOG.txt"; echo "--- arranque C: ---"; cat "$LOG2.txt"; fi
rm -f "$FLP" "$HDD" "$PART" "$LOG" "$LOG2" "$LOG.txt" "$LOG2.txt" "$LOG.fsck"
exit $fail
