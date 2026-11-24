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
# (UEFI) Desde la 0.8.9 arranca también por UEFI (x64 y, si está su GRUB,
# ia32): grub-mkrescue pone, junto al El Torito de la BIOS, otro para EFI (una
# imagen FAT con \EFI\BOOT\BOOTX64.EFI y BOOTIA32.EFI) y la tabla de la
# memoria USB (si se graba con dd); el menú, con «$grub_platform» = efi, tiene
# sus entradas de multiboot2 (nucleo32/uefi.sh). Sin grub-efi-amd64-bin ni
# grub-efi-ia32-bin (o con SIN_UEFI=1), solo BIOS, como antes.
# Necesita python3, nasm, grub-pc-bin (grub-mkrescue) y xorriso (y mtools,
# para la imagen de EFI); no hace falta ser root.
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
. $AQUI/uefi.sh                 # (UEFI)
instala_efi $W/i                # (SDHCI) y el del disco instalado
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
# (UEFI) por UEFI, solo la pantalla del firmware (GOP, o UGA en los Mac
# viejos): con all_video, en QEMU, GRUB usaría la VBE de Bochs por su cuenta
if [ "$grub_platform" = efi ]; then
  insmod efi_gop
  insmod efi_uga
else
  insmod all_video
fi
G
  entrada() {
    printf 'menuentry "%s" {\n  multiboot /%s %s\n%s' "$1" "$2" "$3" "$4"
    [ -n "${5:-}" ] && printf '  set gfxpayload=%s\n' "$5"
    printf '  boot\n}\n'
  }
  # (UEFI) por UEFI: multiboot2 y module2, con la pantalla del firmware (GOP)
  if [ -n "$EFI_PLAT" ]; then
    M2=$(a_module2 "$MODULOS")
    M2="$M2
"
    I2=$(a_module2 "$INSTALA")
    I2="$I2
"
    echo 'if [ "$grub_platform" = efi ]; then'
    menu_efi "Arrancar Hispa-Pentium 0.8 sin instalar (desde el CD, UEFI)" ESCRIT8.BIN "escritorio unidad=M" "$M2" auto
    menu_efi "Instalar Hispa-Pentium 0.8 en el disco duro (o en una tarjeta SD)" SEMILLA.BIN "instalar unidad=M" "$I2" auto
    menu_efi "Sin instalar, a la medida de la pantalla (UEFI)" ESCRIT8.BIN "escritorio unidad=M amedida" "$M2" auto
    menu_efi "Sin instalar, sin USB, discos, red ni ACPI (por si no arranca)" ESCRIT8.BIN "escritorio unidad=M sindisco sinusb sinred sinacpi" "$M2" auto
    menu_efi "El interprete de ordenes (texto, en la pantalla del firmware)" SEMILLA.BIN "ordenes unidad=M" "$M2" auto
    cat <<'G'
menuentry "Diagnostico (UEFI): la pantalla, la memoria, el PCI y ACPI (para hacer una foto)" {
  set pager=1
  echo "== Hispa-Pentium: diagnostico del arranque por UEFI =="
  echo "-- la pantalla (GOP): los modos y el de ahora (su direccion) --"
  videoinfo
  echo "-- el procesador (cpuid -l: 64 bits) --"
  if cpuid -l; then echo "64 bits: si"; else echo "64 bits: no"; fi
  echo "-- las tablas de ACPI (HPET: el reloj que sustituye al 8254) --"
  lsacpi
  echo "-- la tabla del sistema de EFI --"
  lsefisystab
  echo "-- el PCI (clase 0300: la pantalla; 0C03: USB; 0108: NVMe) --"
  lspci
  echo "-- el mapa de memoria --"
  lsmmap
  echo
  echo "Haga una foto de cada pantalla. Pulse Intro para volver."
  read nada
}
G
    cat <<'G'
menuentry "Ver los modos de video que da el firmware (GOP) de este equipo" {
  insmod videoinfo
  set pager=1
  videoinfo
  echo
  echo "Los modos de 32 bits valen. Pulse Intro para volver."
  read nada
}
menuentry "Volver a la configuracion del firmware (UEFI)" {
  fwsetup
}
else
G
  fi
  entrada "Arrancar Hispa-Pentium 0.8 sin instalar (desde el CD)" ESCRIT8.BIN "escritorio unidad=M" "$MODULOS"
  entrada "Instalar Hispa-Pentium 0.8 en el disco duro" SEMILLA.BIN "instalar unidad=M" "$INSTALA"
  entrada "Sin instalar, a la medida de la pantalla" ESCRIT8.BIN "escritorio unidad=M amedida" "$MODULOS" auto
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
  [ -n "$EFI_PLAT" ] && echo fi         # (UEFI)
} > $W/m/boot/grub/grub.cfg
# (UEFI) con GRUB EFI, grub-mkrescue hace las dos (BIOS y EFI); el núcleo
# no usa la BIOS para nada (la pantalla de texto, sin ella, la pinta él:
# textouefi.cas). Sin él, solo la BIOS, como antes
if [ -n "$EFI_PLAT" ]; then
  leeme_uefi $W/m
  # (sin -d: todas las plataformas de /usr/lib/grub, la BIOS y las de EFI)
  grub-mkrescue -o $W/cd.iso $W/m -- -volid HISPA_PENTIUM_08 > $W/mkrescue.txt 2>&1 || { cat $W/mkrescue.txt; exit 1; }
  echo "cd.sh: (UEFI) arranca también por UEFI: $EFI_PLAT"
else
grub-mkrescue -d /usr/lib/grub/i386-pc -o $W/cd.iso $W/m -- -volid HISPA_PENTIUM_08 > $W/mkrescue.txt 2>&1 || { cat $W/mkrescue.txt; exit 1; }
fi
cp $W/cd.iso "$OUT"
echo "cd.sh: $OUT, $(($(stat -c %s "$OUT") / 1048576)) MB"
