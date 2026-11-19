#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo32/disco.sh IMAGEN [12|16|32|mbr]
# Hace la imagen de un disco duro FAT para Hispa-Pentium, con el intérprete de
# órdenes (ORDENES.BIN), los programas de usuario de ejemplo (HOLA.BIN, PRUEBA.BIN,
# ECO.BIN, MALO.BIN, CLI.BIN, ESCRIBE.BIN, BUCLE.BIN), los del escritorio
# (BLOC.BIN, el Bloc de notas, y CONSOLA.BIN, la Consola), unos archivos de órdenes
# (LOTE.BAT, SINFIN.BAT, VISTO.BAT) y unos archivos de texto (LEEME.TXT, la
# carpeta TEXTOS y la carpeta LARGOS, con nombres largos). Sin más: FAT16 (16 MB). Con 12, FAT12 (4 MB); con 32, FAT32 (40 MB);
# con mbr, un disco con tabla de particiones y la FAT16 en su primera
# partición (desde el sector 63).
# La imagen se le da a QEMU como disco IDE: DISCO=IMAGEN nucleo32/probar.sh ...
# Necesita python3, nasm, mkfs.fat (dosfstools) y mtools.
set -e
export LC_ALL=C.UTF-8
AQUI=$(dirname "$(readlink -f "$0")")
IMG=${1:?uso: disco.sh IMAGEN [12|16|32|mbr]}
CLASE=${2:-16}
T=$(mktemp -d)
trap 'rm -rf $T' EXIT
# los programas: cada uno, con su nombre en el disco
for p in uordenes:ORDENES uhola:HOLA ueco:ECO umalo:MALO ucli:CLI uprueba:PRUEBA uescribe:ESCRIBE ubucle:BUCLE ubloc:BLOC uconsola:CONSOLA; do
  f=${p%%:*}; N=${p##*:}
  python3 -I $AQUI/../ccastel.py $AQUI/$f.cas -o $T/$f.asm > /dev/null
  nasm -f bin $T/$f.asm -o $T/$N.BIN
done
# los textos (en la página 437, la del PC, y con sus finales de línea de DOS)
printf 'Hispa-Pentium: un n\xa3cleo de 32 bits escrito en Castel.\r\nEste archivo est\xa0 en un disco FAT y lo ense\xa4a la orden TYPE.\r\n' > $T/LEEME.TXT
python3 - > $T/LARGO.TXT <<'PY'
import sys
for i in range(1, 401):
    sys.stdout.write('linea %03d de un archivo de varios clusters\r\n' % i)
PY
printf 'Con diez ca\xa4ones por banda,\r\nviento en popa, a toda vela,\r\nno corta el mar, sino vuela\r\nun velero bergant\xa1n.\r\n' > $T/PIRATA.TXT
printf 'esto no es un programa\r\n' > $T/NOPROG.BIN
printf 'un archivo con el nombre largo\r\n' > "$T/nombre bien largo.txt"
printf 'Que es mi barco mi tesoro,\r\nque es mi dios la libertad.\r\n' > "$T/cancion_del_pirata.txt"
printf 'Feliz a\xa4o nuevo.\r\n' > "$T/Año nuevo.txt"
# los archivos de órdenes
printf '@ECHO OFF\r\nREM un archivo de \xa2rdenes de prueba: LOTE uno dos\r\nECHO lote: empiezo con %%1 y %%2 (y el 100%%%%)\r\nECO desde el lote\r\nECHO lote: una l\xa1nea > LOTE.TXT\r\nECHO lote: y otra >> LOTE.TXT\r\nTYPE LOTE.TXT\r\nDEL LOTE.TXT\r\nECHO.\r\nPAUSE\r\nECHO lote: fin\r\n' > $T/LOTE.BAT
printf '@ECHO OFF\r\nECHO sinfin: lanzo BUCLE\r\nBUCLE\r\nECHO sinfin: esto no ten\xa1a que salir\r\n' > $T/SINFIN.BAT
printf 'REM con ECHO activado se ve cada orden\r\nECO visto\r\n@ECHO esta no se ve' > $T/VISTO.BAT
V=$T/vol.img
case $CLASE in
  12) mkfs.fat -F 12 -n HISPA -C $V 4096 > /dev/null ;;
  16|mbr) mkfs.fat -F 16 -n HISPA -C $V 16384 > /dev/null ;;
  32) mkfs.fat -F 32 -s 1 -n HISPA -C $V 40960 > /dev/null ;;
  *) echo "disco.sh: la clase es 12, 16, 32 o mbr"; exit 2 ;;
esac
export MTOOLS_SKIP_CHECK=1
mcopy -i $V $T/ORDENES.BIN $T/HOLA.BIN $T/ECO.BIN $T/MALO.BIN $T/CLI.BIN $T/PRUEBA.BIN $T/LEEME.TXT $T/NOPROG.BIN "$T/nombre bien largo.txt" ::
mmd -i $V ::TEXTOS ::VACIA ::LARGOS
mcopy -i $V $T/ESCRIBE.BIN $T/BUCLE.BIN $T/LOTE.BAT $T/SINFIN.BAT $T/VISTO.BAT $T/BLOC.BIN $T/CONSOLA.BIN ::
mcopy -i $V "$T/cancion_del_pirata.txt" "$T/Año nuevo.txt" ::LARGOS/
mcopy -i $V $T/LARGO.TXT $T/PIRATA.TXT ::TEXTOS/
mcopy -i $V $T/ECO.BIN ::TEXTOS/ECO2.BIN
if [ $CLASE = mbr ]; then
  # una tabla de particiones con una sola, de FAT16 (clase 0x06), desde el sector 63
  python3 - $V $IMG <<'PY'
import sys, struct
vol = open(sys.argv[1], 'rb').read()
mbr = bytearray(512)
mbr[446:462] = struct.pack('<BBBBBBBBII', 0x80, 1, 1, 0, 0x06, 254, 63, 0, 63, len(vol) // 512)
mbr[510:512] = b'\x55\xaa'
open(sys.argv[2], 'wb').write(bytes(mbr) + bytes(62 * 512) + vol)
PY
else
  cp $V $IMG
fi
echo "disco.sh: $IMG (FAT, $CLASE), $(($(stat -c %s $IMG) / 1024)) K"
