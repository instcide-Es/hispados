#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo64/programas/hazusb.sh - Hispa-64, etapa 6 (docs/64/HISPA-64.md,
# sección 12): las memorias USB de prueba (sin montar nada: sfdisk,
# mkfs.fat, mtools y hazexfat.py). Como los volúmenes de hazdiscos.sh:
# HISPA64.PRB (la marca: ARCHIVOS solo escribe en los que la tienen),
# LEEME.TXT, PATRON.BIN (300000 octetos) y un nombre largo con acentos.
#   hazusb.sh SALIDA
# deja en SALIDA:
#   usbfat.img    64 MB, MBR con una partición FAT32 (tipo 0x0C, cúmulos de 512
#                 octetos), como viene una memoria de las tiendas
#   usbexfat.img  48 MB sin tabla («superfloppy»): un volumen exFAT entero
#   usbfat.part   la partición FAT32 suelta (para fsck.fat después)
set -e
AQUI=$(cd "$(dirname "$0")" && pwd)
S=$1
[ -n "$S" ] || { echo "uso: hazusb.sh SALIDA" >&2; exit 2; }
mkdir -p "$S"
cd "$S"
rm -f usbfat.img usbexfat.img usbfat.part
python3 -I -c '
import sys
sys.stdout.buffer.write(bytes(((i * 131 + (i >> 8)) & 255) for i in range(300000)))' > patron.bin
printf 'Hola desde el anfitri\xc3\xb3n: este disco es de prueba.\n' > leeme.txt
printf 'Un nombre largo, con acentos y e\xc3\xb1e.\n' > largo.txt
printf 'Disco de pruebas de Hispa-64: se puede escribir en \xc3\xa9l.\n' > marca.txt
# --- usbfat.img: MBR, una FAT32 desde el sector 2048 ---
truncate -s 64M usbfat.img
printf 'label: dos\nstart=2048, type=c\n' | sfdisk -q usbfat.img
truncate -s $(( (131072 - 2048) * 512 )) usbfat.part
mkfs.fat -F 32 -s 1 -n USBFAT usbfat.part > /dev/null
mcopy -i usbfat.part marca.txt ::HISPA64.PRB
mcopy -i usbfat.part leeme.txt ::LEEME.TXT
mcopy -i usbfat.part patron.bin ::PATRON.BIN
mmd -i usbfat.part "::Carpeta con nombre largo"
mcopy -i usbfat.part largo.txt "::Carpeta con nombre largo/Canción de cuna ñandú.txt"
dd if=usbfat.part of=usbfat.img bs=512 seek=2048 conv=notrunc status=none
# --- usbexfat.img: exFAT sin tabla ---
python3 -I "$AQUI/hazexfat.py" usbexfat.img $(( 48 * 1024 * 1024 )) 0 USBEXFAT HISPA64.PRB=marca.txt LEEME.TXT=leeme.txt \
  PATRON.BIN=patron.bin "Carpeta con nombre largo/Canción de cuna ñandú.txt=largo.txt" > /dev/null
echo "memorias: usbfat.img usbexfat.img"
