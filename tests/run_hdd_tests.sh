#!/bin/bash
# Prueba del disco duro: FDISK + FORMAT /S desde el disquete y arranque desde C:.
# Hard disk test: FDISK + FORMAT /S from the floppy, then boot from C:.
set -u
IMG=${1:-build/hispados.img}
HDD=$(mktemp --suffix=.img)
LOG=$(mktemp)
LOG2=$(mktemp)
dd if=/dev/zero of="$HDD" bs=1M count=64 status=none
send() { sleep "${2:-0.8}"; printf '%s\r' "$1"; }

# Fase 1: arrancar del disquete, particionar y formatear
{
  sleep 3
  send "FDISK /STATUS" 1.5
  send "FDISK /PRI" 2
  send "FDISK /STATUS" 2
  send "FORMAT C: /S /V:DISCODURO /AUTOTEST" 3
  send "C:" 3
  send "DIR" 2
  send "VOL" 1
  send "DIR A:\\DOCS" 1.5
  send "A:" 1
  send "FDISK 2 /STATUS" 1
  sleep 2
} | timeout 40 qemu-system-i386 -display none -serial stdio -monitor none \
      -drive file="$IMG",format=raw,if=floppy \
      -drive file="$HDD",format=raw,if=ide -boot a -no-reboot > "$LOG" 2>&1

# Fase 2: arrancar solo del disco duro
{
  sleep 3
  send "VER"
  send "DIR"
  send "MEM"
  sleep 1.5
} | timeout 20 qemu-system-i386 -display none -serial stdio -monitor none \
      -drive file="$HDD",format=raw,if=ide -boot c -no-reboot > "$LOG2" 2>&1

tr -d '\r' < "$LOG" | iconv -f CP437 -t UTF-8 > "$LOG.txt"
tr -d '\r' < "$LOG2" | iconv -f CP437 -t UTF-8 > "$LOG2.txt"
fail=0
check() {
  if grep -qF -- "$2" "$1"; then echo "  OK   $2"; else echo "  FAIL $2"; fail=1; fi
}
echo "== Hispa-DOS hard disk tests =="
check "$LOG.txt" "No hay particiones definidas."
check "$LOG.txt" "Partición DOS primaria creada."
check "$LOG.txt" "PRI DOS  FAT16"
check "$LOG.txt" "C:"
check "$LOG.txt" "Formato completado."
check "$LOG.txt" "Sistema transferido"
check "$LOG.txt" "bytes disponibles en el disco"
check "$LOG.txt" "C:\\>"
check "$LOG.txt" " El volumen de la unidad C es DISCODURO"
check "$LOG.txt" "COMMAND  COM"
check "$LOG.txt" " Directorio de A:\\DOCS"
check "$LOG.txt" "Número de disco duro no válido."
echo "-- arranque desde C: / boot from C: --"
check "$LOG2.txt" "Hispa-DOS 0.2 - núcleo compatible con MS-DOS 6.22"
check "$LOG2.txt" "Versión de DOS notificada: 6.22"
check "$LOG2.txt" " Directorio de C:\\"
check "$LOG2.txt" "bytes del mayor programa ejecutable"
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then
  echo "--- fase 1 ---"; cat "$LOG.txt"; echo "--- fase 2 ---"; cat "$LOG2.txt"
fi
rm -f "$HDD" "$LOG" "$LOG2" "$LOG.txt" "$LOG2.txt"
exit $fail
