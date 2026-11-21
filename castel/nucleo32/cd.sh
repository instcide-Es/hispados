#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo32/cd.sh IMAGEN.ISO [CLAVE_PRIVADA]
# Hace el CD (o DVD) de Hispa-Pentium: una imagen ISO 9660 que arranca por la
# BIOS con El Torito «sin emulación» (la de los CD de siempre: vale grabada en
# un CD o en un DVD, en un PC de verdad, y en VirtualBox o QEMU como unidad
# de CD/DVD), con GRUB. Es también «híbrida»: grabada tal cual en una memoria
# USB (dd), arranca igual.
#
# El núcleo no sabe leer el CD (no tiene ATAPI ni ISO 9660): todo lo que hace
# falta lo carga GRUB en la memoria, como módulos, y el núcleo lo ve como la
# unidad M: (en memoria, de solo lectura). Por eso vale cualquier unidad de
# CD (IDE, SATA, USB): la lee la BIOS. El menú:
#   - Arrancar Hispa-Pentium sin instalar (la primera, por omisión): el
#     escritorio a 800 x 600, con los programas de M:; los discos se ven
#     pero no se tocan;
#   - Instalar Hispa-Pentium en el disco duro: INSTALAR, con todo lo que se
#     instala como módulos (y MEDIO.TXT, que le dice que es el CD);
#   - sin instalar, a la medida de la pantalla; sin tocar el USB, los
#     discos, la red ni ACPI; el intérprete de órdenes en modo texto; los
#     modos de vídeo de la BIOS; y arrancar del disco duro (si se ha dejado
#     el CD puesto después de instalar).
# Lleva también, en \PAQUETES, los paquetes del sistema con su catálogo (con
# CLAVE_PRIVADA, firmado: CATALOGO.FIR), por si se quieren copiar a otro sitio.
# Con GUARDA_PAQ=carpeta, deja ahí también los paquetes (para las pruebas).
# Necesita python3, nasm, grub-pc-bin (grub-mkrescue) y xorriso; no hace falta
# ser root.
set -eu
export LC_ALL=C.UTF-8
AQUI=$(dirname "$(readlink -f "$0")")
OUT=${1:?uso: cd.sh IMAGEN.ISO [CLAVE_PRIVADA]}
CLAVE=${2:-}
W=$(mktemp -d /dev/shm/pgraf-cd.XXXX)
trap 'rm -rf $W' EXIT
mkdir $W/f $W/m $W/i
MEDIO=cd
. $AQUI/sistema.sh
# lo que va en el CD: lo de M: en la raíz; lo de instalar en \INSTALA; la
# oficina; y los paquetes
cp $W/f/* $W/m/
mkdir $W/m/INSTALA
cp -r $W/i/* $W/m/INSTALA/
cp $W/of/HVPRUN.BIN $W/m/
mkdir -p $W/m/OFICINA
cp $W/of/*.HVP $W/of/*.DAT $W/m/OFICINA/
# [WIN32] las DLL de los programas de Windows y sus ejemplos
cp -r $W/w32/WIN32 $W/w32/HISPA32 $W/m/
mkdir $W/m/PAQUETES
cp $W/paq/*.HPQ $W/paq/CATALOGO.TXT $W/m/PAQUETES/
[ -n "${GUARDA_PAQ:-}" ] && { mkdir -p "$GUARDA_PAQ"; cp $W/paq/*.HPQ $W/paq/CATALOGO.TXT "$GUARDA_PAQ"/; }
[ -n "$CLAVE" ] && python3 -I $AQUI/../../tools/hpq.py firma $W/m/PAQUETES "$CLAVE" > /dev/null
mkdir -p $W/m/boot/grub
{ cat <<'G'
set timeout=10
set default=0
insmod all_video
G
  entrada() {
    printf 'menuentry "%s" {\n  multiboot /%s %s\n%s' "$1" "$2" "$3" "$4"
    [ -n "${5:-}" ] && printf '  set gfxpayload=%s\n' "$5"
    printf '  boot\n}\n'
  }
  entrada "Arrancar Hispa-Pentium 0.8 sin instalar (desde el CD)" ESCRIT8.BIN "escritorio unidad=M" "$MODULOS"
  entrada "Instalar Hispa-Pentium 0.8 en el disco duro" SEMILLA.BIN "instalar unidad=M" "$INSTALA"
  entrada "Sin instalar, a la medida de la pantalla" ESCRIT8.BIN "escritorio unidad=M" "$MODULOS" auto
  entrada "Sin instalar, sin USB, discos, red ni ACPI (por si no arranca)" ESCRIT8.BIN "escritorio unidad=M sindisco sinusb sinred sinacpi" "$MODULOS"
  entrada "El interprete de ordenes (modo texto)" SEMILLA.BIN "ordenes unidad=M" "$MODULOS"
  cat <<'G'
menuentry "Ver los modos de video que da la BIOS de este equipo" {
  insmod videoinfo
  set pager=1
  videoinfo
  echo
  echo "Los modos de 32, 24 o 16 bits (Direct color) valen. Pulse Intro para volver."
  read nada
}
menuentry "Arrancar del disco duro (si ya esta instalado)" {
  set root=(hd0)
  chainloader +1
  boot
}
G
} > $W/m/boot/grub/grub.cfg
# (solo la BIOS: el núcleo usa la pantalla de texto y la VBE, que con UEFI
# no hay, y el disco instalado arranca por la BIOS)
grub-mkrescue -d /usr/lib/grub/i386-pc -o $W/cd.iso $W/m -- -volid HISPA_PENTIUM_08 > $W/mkrescue.txt 2>&1 || { cat $W/mkrescue.txt; exit 1; }
cp $W/cd.iso "$OUT"
echo "cd.sh: $OUT, $(($(stat -c %s "$OUT") / 1048576)) MB"
