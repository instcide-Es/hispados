#!/bin/bash
# FAT16 con grupos de 64 KB (128 sectores; los hacen NT, 2000 y XP en
# discos de 2 a 4 GB): MS-DOS 6.22 no los monta, y Hispa-DOS tampoco (antes
# los montaba con los octetos por grupo a 0: «Error de división» al leer y
# COMMAND en bucle). La misma prueba con grupos de 32 KB sí lee y escribe.
# FAT16 with 64 KB clusters is refused at mount time (like MS-DOS 6.22).
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
fail=0
Q="qemu-system-i386 -display none -serial stdio -monitor none -parallel none"
send() { sleep "${2:-0.8}"; printf '%s\r' "$1"; }
conv() { tr -d '\r' < "$1" | iconv -f CP437 -t UTF-8 > "$1.txt"; }
check() { if grep -qF -- "$2" "$1.txt"; then echo "  OK   $2"; else echo "  FAIL $2"; fail=1; fi; }
checkn() { if grep -qF -- "$2" "$1.txt"; then echo "  FAIL (no debía salir) $2"; fail=1; else echo "  OK   sin «$2»"; fi; }
show() { if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then for f in "$@"; do echo "--- $f ---"; cat "$f.txt"; done; fi; }
cp "$IMG" $T/f.img
printf 'Hola desde FAT16\r\n' > $T/hola.txt
disco() {  # nombre sectores-por-grupo
  truncate -s 600M $T/$1.img
  echo 'start=2048, type=6' | /usr/sbin/sfdisk -q $T/$1.img
  local secs=$(( $(stat -c %s $T/$1.img)/512 - 2048 ))
  mkfs.fat -F 16 -s $2 -n GRUPOS --offset 2048 $T/$1.img $((secs / 2)) > /dev/null 2>&1
  mcopy -i $T/$1.img@@1M $T/hola.txt ::HOLA.TXT
}
lanza() {  # disco registro
  { sleep 5
    send "DIR C:\\" 2
    send "TYPE C:\\HOLA.TXT" 2
    send "COPY A:\\COMMAND.COM C:\\COPIA.COM" 3
    send "ECHO SIGUE-VIVO" 1
    sleep 2
  } | timeout 40 $Q -drive file=$T/f.img,format=raw,if=floppy \
        -drive file=$T/$1.img,format=raw,if=ide -boot a -no-reboot > $T/$2 2>&1
  conv $T/$2
}
echo "== Hispa-DOS FAT16 con grupos de 64 KB (no se monta) =="
disco g64 128
lanza g64 l64
checkn $T/l64 "Error de división"
checkn $T/l64 "Reiniciando el intérprete"
checkn $T/l64 "Hola desde FAT16"
check $T/l64 "SIGUE-VIVO"
if mdir -i $T/g64.img@@1M ::COPIA.COM >/dev/null 2>&1; then echo "  FAIL se ha escrito en la FAT16 de 64 KB"; fail=1
else echo "  OK   no se ha escrito en la FAT16 de 64 KB"; fi
echo "== Hispa-DOS FAT16 con grupos de 32 KB (sí) =="
disco g32 64
lanza g32 l32
check $T/l32 "Hola desde FAT16"
check $T/l32 "SIGUE-VIVO"
checkn $T/l32 "Error de división"
if mdir -i $T/g32.img@@1M ::COPIA.COM >/dev/null 2>&1; then echo "  OK   COPIA.COM escrito en la de 32 KB"
else echo "  FAIL COPIA.COM no está en la de 32 KB"; fail=1; fi
show $T/l64 $T/l32
exit $fail
