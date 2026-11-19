#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo32/usb.sh IMAGEN
# Hace la imagen de una memoria USB (64 MB, FAT16, arranque BIOS con GRUB) con
# Hispa-Pentium, para probarlo en un PC de verdad: se graba tal cual en la
# memoria (dd, Rufus en modo «imagen DD», balenaEtcher...) y se arranca de ella.
# GRUB carga el núcleo por multiboot. Desde la 0.6 el núcleo lee él la
# memoria USB (xHCI y EHCI): es su unidad X:, la del sistema («unidad=X»), y
# en ella se puede guardar («escritura=usb»: solo en las memorias USB; en los
# discos de dentro del PC NO se escribe, aunque se ven: C:). Por si el USB de
# un equipo no se deja, GRUB carga también, como módulos, los programas (el
# intérprete de órdenes, el Bloc de notas, la Consola...) y un LEEME.TXT:
# hacen de unidad M:, en memoria y de solo lectura (usuario.cas), que es la
# del sistema si no hay X:. Con «registro», lo que el núcleo encuentra al
# arrancar (discos, USB) queda en X:\ARRANQUE.TXT. Las entradas del menú:
#   - el escritorio a 800x600 (el de siempre);
#   - el escritorio «a la medida de la pantalla»: GRUB pone el modo que el
#     panel dice que prefiere (lo lee de su EDID): «set gfxpayload=auto»;
#   - el escritorio a 1024x600, a 1366x768 y a 1024x768 (copias del núcleo que
#     le piden a GRUB ese modo en su cabecera: nucleo32/modo_grafico.py; si la
#     BIOS no lo da por VBE, GRUB pone el preferido o el que más se parezca);
#   - los modos de vídeo que da la BIOS de ese equipo (la orden «videoinfo»
#     de GRUB), para saber cuáles se pueden pedir;
#   - el escritorio «como en la 0.5»: sin tocar el USB ni mirar los discos
#     («sinusb sindisco»), por si con lo nuevo un equipo no arranca;
#   - el intérprete de órdenes y la consola del núcleo, en modo texto (la
#     consola enseña en la pantalla lo que encuentra: discos y USB);
#   - desde la 0.7, «Instalar Hispa-Pentium en el disco duro»: el núcleo en
#     modo texto con «instalar», que lanza INSTALAR.BIN (uinstala.cas). Todo
#     lo que se instala viene con el cargador, como módulos (la unidad M:):
#     NUCLEO.BIN (el núcleo que pide 800 x 600), los programas, el asistente
#     del primer arranque (ASISTE.BIN), los textos y el arranque del disco,
#     que se hace aquí porque el núcleo no puede: BOOT.IMG (el primer sector
#     de GRUB, para el MBR) y CORE.IMG (el resto de GRUB, con sus módulos
#     dentro, para el hueco de delante de la primera partición; busca en los
#     discos \HISPA\GRUB.CFG y sigue lo que diga). Con «instalar», el núcleo
#     no deja escribir en la memoria USB ni un sector.
# Necesita ser root (losetup, mount), python3, nasm, parted, dosfstools y
# grub-pc-bin (grub-install --target=i386-pc).
set -eu
export LC_ALL=C.UTF-8
AQUI=$(dirname "$(readlink -f "$0")")
OUT=${1:?uso: usb.sh IMAGEN}
W=$(mktemp -d /dev/shm/pgraf-usb.XXXX)
L=
trap '[ -n "$L" ] && { umount $W/m 2>/dev/null || true; losetup -d $L 2>/dev/null || true; }; rm -rf $W' EXIT
mkdir $W/f $W/m
python3 -I $AQUI/../ccastel.py $AQUI/semilla.cas -o $W/semilla.asm > /dev/null
nasm -f bin $W/semilla.asm -o $W/f/SEMILLA.BIN
python3 -I $AQUI/modo_grafico.py $W/f/SEMILLA.BIN $W/f/ESCRIT8.BIN 800 600 32 > /dev/null
python3 -I $AQUI/modo_grafico.py $W/f/SEMILLA.BIN $W/f/ESCRIT10.BIN 1024 768 32 > /dev/null
python3 -I $AQUI/modo_grafico.py $W/f/SEMILLA.BIN $W/f/ESCRIT6.BIN 1024 600 32 > /dev/null
python3 -I $AQUI/modo_grafico.py $W/f/SEMILLA.BIN $W/f/ESCRIT13.BIN 1366 768 32 > /dev/null
MODULOS=
for p in uordenes:ORDENES ubloc:BLOC uconsola:CONSOLA uequipo:EQUIPO uhola:HOLA ueco:ECO uprueba:PRUEBA ubucle:BUCLE uinstala:INSTALAR uasiste:ASISTE; do
  f=${p%%:*}; N=${p##*:}
  python3 -I $AQUI/../ccastel.py $AQUI/$f.cas -o $W/$f.asm > /dev/null
  nasm -f bin $W/$f.asm -o $W/f/$N.BIN
  # (el programa que instala y el asistente no van con las entradas de siempre)
  case $N in INSTALAR|ASISTE) continue ;; esac
  MODULOS="$MODULOS  module /$N.BIN $N.BIN
"
done
# lo que se instala en el disco: el núcleo (con el nombre que tendrá allí), los
# programas, los textos del sistema instalado y el arranque del disco
INSTALA="  module /ESCRIT8.BIN NUCLEO.BIN
  module /INSTALAR.BIN INSTALAR.BIN
  module /ASISTE.BIN ASISTE.BIN
$MODULOS  module /LEEMEDIS.TXT LEEME.TXT
  module /MANUAL.TXT MANUAL.TXT
  module /BOOT.IMG BOOT.IMG
  module /CORE.IMG CORE.IMG
"
iconv -f UTF-8 -t CP437 > $W/f/LEEMEDIS.TXT <<'L'
Hispa-Pentium 0.7, instalado en el disco duro.

El sistema está en la carpeta \HISPA de esta unidad (C:): el núcleo
(NUCLEO.BIN), los programas (ORDENES.BIN, BLOC.BIN, CONSOLA.BIN, EQUIPO.BIN...),
sus ajustes (HISPA.INI) y el menú de arranque (GRUB.CFG).
En MANUAL.TXT, en esa carpeta, hay un resumen de lo que se puede hacer.
L
iconv -f UTF-8 -t CP437 > $W/f/MANUAL.TXT <<'L'
HISPA-PENTIUM 0.7 - RESUMEN

Hispa-Pentium es un sistema de 32 bits escrito en Castel, de la familia
Hispa-DOS. Este es el sistema instalado en el disco duro.

EL ESCRITORIO
  Inicio (abajo a la izquierda) abre el menú: el Bloc de notas, la Consola,
  Mi equipo, los documentos (.TXT) de la raíz del disco, Configuración y
  Apagar el equipo. Los iconos del tapiz se abren con dos clics.
  Las ventanas se mueven por su título, cambian de tamaño por el borde y
  tienen sus botones de minimizar, maximizar y cerrar. El botón secundario
  del ratón abre un menú en casi todas partes.
  El Bloc de notas guarda con F2. Cortar, copiar y pegar: Ctrl+X, Ctrl+C y
  Ctrl+V (el portapapeles es de todo el sistema).

LA CONSOLA (el intérprete de órdenes)
  AYUDA dice las órdenes: DIR, TYPE, COPY, DEL, REN, MD, CD, RD, PATH, VER,
  MEM, FECHA, CLS... Un programa se lanza por su nombre. C:, X: cambian de
  unidad (X: es una memoria USB). Hay archivos de órdenes (.BAT).

LAS UNIDADES
  C:, D:...  los discos de dentro del equipo (con formato FAT)
  X:, Y:     las memorias USB
  M:         los archivos que trae el cargador (solo en el arranque de
             emergencia)

LA CARPETA \HISPA
  NUCLEO.BIN   el núcleo
  *.BIN        los programas
  HISPA.INI    los ajustes: usuario, equipo, teclado (es o us) y modo
  GRUB.CFG     el menú de arranque. La línea «set modo=» es la medida de la
               pantalla (por ejemplo 1024x768x32, o auto)
  ASISTE.BIN   el asistente del primer arranque: si se borra HISPA.INI,
               vuelve a salir al arrancar
  MBR.ANT      el primer sector que tenía el disco antes de instalar

EL ARRANQUE
  El menú que sale al encender tiene, además del escritorio, el intérprete
  de órdenes en modo texto y un arranque de emergencia que no toca los
  discos ni el USB.

Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
L
sed -i 's/$/\r/' $W/f/LEEMEDIS.TXT $W/f/MANUAL.TXT
# el arranque del disco: GRUB con todo lo que necesita dentro (en el disco
# instalado no hay carpeta de módulos), que busca \HISPA\GRUB.CFG
printf 'search.file /HISPA/GRUB.CFG root\nset prefix=($root)/HISPA\n' > $W/antes.cfg
grub-mkimage -O i386-pc -o $W/f/CORE.IMG -c $W/antes.cfg -p /HISPA biosdisk part_msdos part_gpt fat multiboot normal configfile \
  search_fs_file vbe vga video video_fb video_bochs video_cirrus chain echo test read boot halt reboot videoinfo ls cat
cp /usr/lib/grub/i386-pc/boot.img $W/f/BOOT.IMG
# (como hace grub-bios-setup en un disco duro: no fiarse de la unidad que
# dice la BIOS si no es un disco; el resto de GRUB, desde el sector 1)
printf '\x90\x90' | dd of=$W/f/BOOT.IMG bs=1 seek=102 conv=notrunc status=none
[ $(stat -c %s $W/f/CORE.IMG) -le 1000000 ] || { echo "usb.sh: CORE.IMG no cabe en el hueco de arranque"; exit 1; }
printf 'Hispa-Pentium 0.7: un n\xa3cleo de 32 bits escrito en Castel.\r\nEste archivo est\xa0 en la memoria USB de arranque (la unidad X:) y, por si\r\nel USB no se deja, ha venido tambi\x82n con el cargador (la unidad M:).\r\n' > $W/f/LEEME.TXT
MODULOS="$MODULOS  module /LEEME.TXT LEEME.TXT
"
IMG=$W/usb.img
truncate -s 64M $IMG
parted -s $IMG mklabel msdos mkpart primary fat16 1MiB 100% set 1 boot on
L=$(losetup -fP --show $IMG)
# (la partición tarda un momento en aparecer)
for _ in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do [ -b ${L}p1 ] && break; sleep 0.1; done
[ -b ${L}p1 ] || { partprobe $L 2> /dev/null; sleep 0.5; }
mkfs.fat -F 16 -n HISPAPENT ${L}p1 > /dev/null
mount ${L}p1 $W/m
cp $W/f/* $W/m/
mkdir -p $W/m/boot/grub
{ cat <<'G'
set timeout=8
set default=0
insmod all_video
G
  # el rótulo, el núcleo, lo que se le dice y, si hace falta, el modo que
  # se le pide a GRUB (después de «multiboot», que pone el de la cabecera)
  entrada() {
    printf 'menuentry "%s" {\n  multiboot /%s %s\n%s' "$1" "$2" "$3" "$MODULOS"
    [ -n "${4:-}" ] && printf '  set gfxpayload=%s\n' "$4"
    printf '  boot\n}\n'
  }
  N="escritura=usb unidad=X registro"
  entrada "Hispa-Pentium 0.7: el escritorio (800 x 600)" ESCRIT8.BIN "escritorio $N"
  entrada "Hispa-Pentium 0.7: el escritorio a la medida de la pantalla" ESCRIT8.BIN "escritorio $N" "auto"
  entrada "Hispa-Pentium 0.7: el escritorio (1024 x 600)" ESCRIT6.BIN "escritorio $N"
  entrada "Hispa-Pentium 0.7: el escritorio (1366 x 768)" ESCRIT13.BIN "escritorio $N"
  entrada "Hispa-Pentium 0.7: el escritorio (1024 x 768)" ESCRIT10.BIN "escritorio $N"
  entrada "Hispa-Pentium 0.7: el escritorio SIN tocar el USB ni los discos (como la 0.5)" ESCRIT8.BIN "escritorio sindisco sinusb"
  cat <<'G'
menuentry "Ver los modos de video que da la BIOS de este equipo" {
  insmod videoinfo
  set pager=1
  videoinfo
  echo
  echo "Los modos de 32, 24 o 16 bits (Direct color) valen. Pulse Intro para volver."
  read nada
}
G
  entrada "Hispa-Pentium 0.7: el interprete de ordenes (modo texto)" SEMILLA.BIN "ordenes $N"
  entrada "Hispa-Pentium 0.7: la consola del nucleo (modo texto): dice los discos y el USB que encuentra" SEMILLA.BIN "$N"
  printf 'menuentry "Instalar Hispa-Pentium 0.7 en el disco duro" {\n  multiboot /SEMILLA.BIN instalar unidad=M\n%s  boot\n}\n' "$INSTALA"
} > $W/m/boot/grub/grub.cfg
grub-install --target=i386-pc --boot-directory=$W/m/boot --modules="part_msdos fat multiboot all_video videoinfo read echo" $L > $W/grub.txt 2>&1 || { cat $W/grub.txt; exit 1; }
sync
umount $W/m
losetup -d $L
L=
cp $IMG "$OUT"
echo "usb.sh: $OUT, $(($(stat -c %s "$OUT") / 1048576)) MB"
