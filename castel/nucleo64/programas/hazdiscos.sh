#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo64/programas/hazdiscos.sh - Hispa-64, etapa 5 (docs/64/HISPA-64.md,
# sección 11): los discos de prueba (sin montar nada: sgdisk, sfdisk,
# mkfs.fat, mtools y hazexfat.py). Cada volumen de prueba lleva HISPA64.PRB
# (la marca: el programa ARCHIVOS solo escribe en los que la tienen),
# LEEME.TXT, PATRON.BIN (300000 octetos) y un nombre largo con acentos.
#   hazdiscos.sh SALIDA
# deja en SALIDA:
#   nvme.img    112 MB, GPT: p1 «EFI» (FAT32, sin marca), p2 «datos» (FAT32,
#               cúmulos de 512 octetos: las carpetas crecen enseguida)
#   sata.img    128 MB, MBR: p1 FAT32 (tipo 0x0C, cúmulos de 1 KB), una
#               extendida con una lógica (p5) exFAT (tipo 0x07, hecha con
#               hazexfat.py: de solo leer)
#   grande.img  3 TB (disperso), GPT: p1 FAT32 al principio y p2 FAT32 más
#               allá de los 2 TB (sector 4.999.999.488 > 2^32)
#   grande2.img otro igual (para SATA: LBA de 48 bits)
#   crudo.img   16 MB sin tabla, con «HISPA64 CRUDO» en el sector 0: el
#               núcleo escribe y lee en él a pelo (prueba_discos)
#   *.part      cada partición suelta, para mirarla después con fsck.fat
#   patron.bin  el patrón (para comparar)
set -e
AQUI=$(cd "$(dirname "$0")" && pwd)
S=$1
[ -n "$S" ] || { echo "uso: hazdiscos.sh SALIDA" >&2; exit 2; }
mkdir -p "$S"
cd "$S"
rm -f ./*.img ./*.part
# el patrón: el octeto i vale (i * 131 + i / 256) mod 256
python3 -I -c '
import sys
sys.stdout.buffer.write(bytes(((i * 131 + (i >> 8)) & 255) for i in range(300000)))' > patron.bin
printf 'Hola desde el anfitri\xc3\xb3n: este disco es de prueba.\n' > leeme.txt
printf 'Un nombre largo, con acentos y e\xc3\xb1e.\n' > largo.txt
printf 'Disco de pruebas de Hispa-64: se puede escribir en \xc3\xa9l.\n' > marca.txt
# un volumen FAT32 de prueba en el archivo $1 (de $2 sectores, $3 sectores por cúmulo, etiqueta $4)
fat_prueba() {
  truncate -s $(( $2 * 512 )) "$1"
  mkfs.fat -F 32 -s "$3" -n "$4" "$1" > /dev/null
  mcopy -i "$1" marca.txt ::HISPA64.PRB
  mcopy -i "$1" leeme.txt ::LEEME.TXT
  mcopy -i "$1" patron.bin ::PATRON.BIN
  mmd -i "$1" "::Carpeta con nombre largo"
  mcopy -i "$1" largo.txt "::Carpeta con nombre largo/Canción de cuna ñandú.txt"
}
# --- nvme.img: GPT ---
truncate -s 112M nvme.img
sgdisk -n 1:2048:+40M -t 1:ef00 -c 1:EFI -n 2:0:0 -t 2:0700 -c 2:DATOS nvme.img > /dev/null
read -r i2 f2 <<< "$(sgdisk -i 2 nvme.img | awk '/First sector/ {a=$3} /Last sector/ {b=$3} END {print a, b}')"
truncate -s 40M nvme1.part
mkfs.fat -F 32 -s 1 -n ESP nvme1.part > /dev/null
mmd -i nvme1.part ::EFI ::EFI/BOOT
printf 'no es un cargador\n' > falso.efi
mcopy -i nvme1.part falso.efi ::EFI/BOOT/BOOTX64.EFI
dd if=nvme1.part of=nvme.img bs=512 seek=2048 conv=notrunc status=none
fat_prueba nvme2.part $(( f2 - i2 + 1 )) 1 DATOS
dd if=nvme2.part of=nvme.img bs=512 seek="$i2" conv=notrunc status=none
# --- sata.img: MBR con una extendida ---
truncate -s 128M sata.img
printf 'label: dos\nstart=2048, size=172032, type=c\nstart=180224, type=5\nstart=182272, size=65536, type=7\n' | sfdisk -q sata.img
fat_prueba sata1.part 172032 2 SATAFAT
dd if=sata1.part of=sata.img bs=512 seek=2048 conv=notrunc status=none
cp patron.bin patron.frag
python3 -I "$AQUI/hazexfat.py" sata5.part $(( 65536 * 512 )) 182272 EXFATPRB HISPA64.PRB=marca.txt LEEME.TXT=leeme.txt \
  PATRON.BIN.frag=patron.bin "Carpeta con nombre largo/Canción de cuna ñandú.txt=largo.txt" "Otra carpeta/más hondo/seguido.bin=patron.bin" > /dev/null
dd if=sata5.part of=sata.img bs=512 seek=182272 conv=notrunc status=none
# --- grande.img y grande2.img: 3 TB, GPT, una partición más allá de 2^32 sectores ---
for g in grande grande2; do
  truncate -s 3T $g.img
  sgdisk -n 1:2048:+80M -t 1:0700 -n 2:4999999488:+80M -t 2:0700 $g.img > /dev/null
  fat_prueba ${g}1.part 163840 2 G1
  fat_prueba ${g}2.part 163840 2 G2ALTO
  dd if=${g}1.part of=$g.img bs=512 seek=2048 conv=notrunc,sparse status=none
  dd if=${g}2.part of=$g.img bs=512 seek=4999999488 conv=notrunc,sparse status=none
done
# --- crudo.img ---
truncate -s 16M crudo.img
printf 'HISPA64 CRUDO: disco de pruebas, se escribe a pelo.' | dd of=crudo.img conv=notrunc status=none
rm -f falso.efi patron.frag
echo "discos: nvme.img sata.img grande.img grande2.img crudo.img"
