#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# probar32.sh PROGRAMA [argumentos...]
# Ejecuta un programa Castel de 32 bits y enseña lo que escribe y su código
# de salida (en la última línea, «[código N]»):
#   - un núcleo (NOMBRE.BIN, de «modelo núcleo»): QEMU lo arranca con
#     -kernel (multiboot); lo que escribe sale por el puerto serie, y el
#     código, por el dispositivo isa-debug-exit (puerto 0xF4);
#   - un programa Hispa32 (NOMBRE.EXE, de «modelo hispa32»): se copia al
#     disquete de Hispa-DOS (../build/hispados.img, o HISPADOS_IMG=ruta) y
#     se lanza con HW32 (y HSPDPMI), tecleando por el puerto serie.
# Necesita qemu-system-i386 y, para los .EXE, mtools y el disquete hecho
# (make y make nucprogs en la raíz del proyecto).
set -u
P=${1:-}
[ -f "$P" ] || { echo "uso: probar32.sh PROGRAMA.BIN|PROGRAMA.EXE [argumentos...]"; exit 2; }
shift
AQUI=$(dirname "$(readlink -f "$0")")
T=$(mktemp -d)
trap 'rm -rf $T' EXIT
EXT=$(echo "${P##*.}" | tr 'a-z' 'A-Z')
if [ "$EXT" = BIN ]; then
  timeout ${ESPERA:-20} qemu-system-i386 -kernel "$P" -display none -monitor none -serial file:$T/sal \
    -device isa-debug-exit,iobase=0xf4,iosize=0x04 -no-reboot -m 32 > /dev/null 2>&1
  r=$?
  tr -d '\r' < $T/sal | iconv -f CP437 -t UTF-8
  # isa-debug-exit acaba con (código * 2) + 1
  if [ $((r % 2)) = 1 ]; then echo "[código $(((r - 1) / 2))]"; else echo "[no ha acabado: $r]"; fi
  exit 0
fi
IMG=${HISPADOS_IMG:-$AQUI/../build/hispados.img}
B=$(dirname "$IMG")
[ -f "$IMG" ] && [ -f $B/HW32.COM ] || { echo "falta el disquete de Hispa-DOS o HW32.COM (make y make nucprogs)"; exit 2; }
NOM=$(basename "${P%.*}" | tr 'a-z' 'A-Z' | cut -c1-8)
cp "$IMG" $T/f.img
printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nFILES=30\r\n' > $T/CONFIG.SYS
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
mcopy -o -i $T/f.img $B/HW32.COM ::BIN/HW32.COM
mcopy -o -i $T/f.img "$P" ::BIN/$NOM.EXE
{ sleep 6
  printf 'A:\\HISPADOS\\HSPDPMI\r'; sleep 2
  printf 'A:\\BIN\\HW32 A:\\BIN\\%s.EXE %s\r' $NOM "$*"; sleep ${ESPERA:-5}
  printf 'ECHO [fin-programa]\r'; sleep 1
} | timeout 60 qemu-system-i386 -display none -serial stdio -monitor none -parallel none -m 16 \
    -drive file=$T/f.img,format=raw,if=floppy,index=0 -boot a -no-reboot > $T/raw 2>&1
tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 > $T/log
awk -v n="HW32 A:\\\\BIN\\\\$NOM.EXE" 'v && index($0, "ECHO [fin-programa]") { exit } v { print } index($0, n) { v = 1 }' $T/log
