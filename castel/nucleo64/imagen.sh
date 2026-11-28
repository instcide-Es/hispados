#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo64/imagen.sh - Hispa-64 (docs/64/HISPA-64.md): hace el núcleo y la
# imagen de arranque por UEFI de 64 bits.
#   imagen.sh SALIDA [prueba [LÍNEA]]
# [etapa 5] el disco lleva además una partición de pruebas (FAT32, «PRUEBAS»,
# con la marca HISPA64.PRB: el programa ARCHIVOS solo escribe en las que la
# tienen) y, si no es «prueba» (o con VDI=1), sale también el disco para
# VirtualBox (hispa64.vdi, con qemu-img).
# [etapa 8] el primero del menú es el escritorio (VENTANAS, la opción «escritorio»).
# [etapa 9] y dos más: «cmos=utc» y «sinred».
# [etapa 7] los programas de musl y Lua (programas/hazmusl.sh) van también en
# \boot\prog (y el cargador dinámico, ld-musl-x86_64.so.1: en Hispa-64, /lib).
# [etapa 4] con los programas de prueba (programas/hazlos.sh) en \boot\prog,
# que GRUB carga como módulos (la unidad en memoria); y, además del disco, un
# CD (hispa64-cd.iso, con grub-mkrescue: arranca por BIOS y por UEFI de 64
# bits) si no es «prueba» o si se pide con CD=1.
# (solo los drivers de pantalla del firmware, efi_gop y efi_uga: con los de
# QEMU, como video_bochs, GRUB cambiaría de modo y no dejaría el del GOP)
# deja en la carpeta SALIDA:
#   HISPA64.BIN                     el núcleo (ccastel.py y NASM)
#   HISPA64.SIM                     los nombres de sus funciones (simbolos.py, con
#                                   el mapa de NASM): GRUB lo carga como módulo
#   hispa64.img                     el disco (128 MB, GPT: la partición EFI en
#                                   FAT32 con \EFI\BOOT\BOOTX64.EFI, el GRUB de
#                                   x86_64-efi con lo que usa el menú dentro,
#                                   \boot\grub\grub.cfg, \boot\HISPA64.BIN y
#                                   \boot\HISPA64.SIM y \boot\prog\*; y la de
#                                   pruebas, FAT32, con la marca)
#   hispa64.vdi                     el mismo disco, para VirtualBox
#   hispa64-cd.iso                  el CD (BIOS y UEFI), con lo mismo
# Con «prueba», el menú no espera, escribe por el puerto serie y el núcleo
# arranca con la línea de órdenes LÍNEA («apagar» si no se dice: acaba en
# QEMU con el código 42): lo usa pruebas.sh.
# [etapa 10 h] PROGS_DE=CARPETA: los programas de la unidad, los de esa
# carpeta, sin hacerlos (con WEB=1, el disco grande del navegador).
# Nada se monta (mtools): no hace falta ser root.
set -e
AQUI=$(cd "$(dirname "$0")" && pwd)
C=$(dirname "$AQUI")
S=$1; MODO=${2:-}; LINEA=${3-apagar}
[ -n "$S" ] || { echo "uso: imagen.sh SALIDA [prueba]" >&2; exit 2; }
mkdir -p "$S"
python3 -I "$C/ccastel.py" "$AQUI/hispa64.cas" -o "$S/hispa64.asm" -I "$C/nucleo32" > /dev/null
# (el mapa de NASM, con una línea delante: los mismos octetos)
{ echo "[map symbols $S/hispa64.map]"; cat "$S/hispa64.asm"; } > "$S/hispa64m.asm"
nasm -f bin "$S/hispa64m.asm" -o "$S/HISPA64.BIN"
rm -f "$S/hispa64m.asm"
python3 -I "$AQUI/simbolos.py" "$S/hispa64.map" "$S/HISPA64.SIM"
# los programas de la unidad en memoria, y sus líneas de module2
if [ -n "${PROGS_DE:-}" ]; then
  # [etapa 10 h] los programas de esa carpeta, ya hechos (pruebas.sh: la del navegador con ventana)
  rm -rf "$S/prog" && mkdir -p "$S/prog"
  cp -l "$PROGS_DE"/* "$S/prog/" 2> /dev/null || cp "$PROGS_DE"/* "$S/prog/"
else
"$AQUI/programas/hazlos.sh" "$S/prog" > /dev/null
# [etapa 10 g] con WEB=1, también LibJS y Ladybird sin ventana (hazweb.sh: unos
# 490 MB más en la unidad; el disco, de 1,25 GB; hacen falta 3 GB de memoria)
if [ "${WEB:-0}" = 1 ]; then
  "$AQUI/programas/ladybird/hazweb.sh" "$S/prog" > /dev/null
  rm -f "$S/prog/js"
fi
fi
PROGS=$(cd "$S/prog" && ls)
MODS2=""
for f in $PROGS; do MODS2="$MODS2  module2 /boot/prog/$f $f
"; done
MODS="normal configfile part_gpt part_msdos fat multiboot2 efi_gop efi_uga video_fb gfxterm font echo test sleep cpuid serial terminal reboot halt efifwsetup ls search"
# shellcheck disable=SC2086
grub-mkimage -O x86_64-efi -o "$S/BOOTX64.EFI" -p /boot/grub $MODS
if [ "$MODO" = prueba ]; then
  cat > "$S/grub.cfg" <<CFG
set timeout=0
set default=0
serial --unit=0 --speed=115200
terminal_output serial
set gfxpayload=keep
menuentry "Hispa-64 (prueba)" {
  multiboot2 /boot/HISPA64.BIN $LINEA
  module2 /boot/HISPA64.SIM HISPA64.SIM
$MODS2  boot
}
CFG
else
  entrada() {   # TÍTULO LÍNEA
    printf '  menuentry "%s" {\n    multiboot2 /boot/HISPA64.BIN %s\n    module2 /boot/HISPA64.SIM HISPA64.SIM\n%s    boot\n  }\n' "$1" "$2" "$MODS2"
  }
  {
    cat <<'CFG'
# Hispa-64 0.11.10 (etapa 11 a). Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
set timeout=5
set default=0
serial --unit=0 --speed=115200
terminal_input console serial
terminal_output console serial
# la pantalla, tal cual la deja el firmware (su resolución de siempre)
set gfxpayload=keep
if cpuid -l; then
CFG
    entrada "Hispa-64 0.11.10 (etapa 11 a): el escritorio, con Pluma, Cuentas y Notas (lo guardado, en /casa; los demás discos, solo leer; la red, con IPv6; curl)" "escritorio"
    entrada "Hispa-64: el escritorio, guardando en los discos (SOLO en VirtualBox o en discos de prueba)" "escritorio escribir"
    entrada "Hispa-64: el intérprete de órdenes (sin el escritorio; «VENTANAS» lo abre)" ""
    entrada "Hispa-64: con escritura en los discos (SOLO en VirtualBox, en discos de prueba o en memorias USB de prueba)" "escribir"
    entrada "Hispa-64: la hora del CMOS en UTC (si la hora sale una o dos horas cambiada)" "cmos=utc"
    entrada "Hispa-64: sin la red (si se queda al poner en marcha la tarjeta de red)" "sinred"
    entrada "Hispa-64: el USB sin MSI ni MSI-X (si el teclado o el ratón USB no van)" "sinmsix sinmsi"
    entrada "Hispa-64: sin las pruebas de musl (si se queda en ellas)" "sinmusl"
    [ "${WEB:-0}" = 1 ] && entrada "Hispa-64: el escritorio con el Navegador (Ladybird con ventana; con la red)" "escritorio navegador"
    [ "${WEB:-0}" = 1 ] && entrada "Hispa-64: con las pruebas de Ladybird al arrancar (LibJS, MJS, unos 3 minutos; y el navegador sin ventana, MWEB)" "ladybird"
    entrada "Hispa-64: sin USB (si se queda al mirar el USB)" "sinusb"
    entrada "Hispa-64: sin touchpad I2C (si se queda al buscarlo)" "sini2c"
    entrada "Hispa-64: prueba del ratón, la tableta y el touchpad (10 s diciendo dónde está el puntero)" "prueba=raton"
    entrada "Hispa-64: sin PCID (si la 1 falla al cambiar de programa)" "sinpcid"
    entrada "Hispa-64: con un solo procesador (si se queda al encender los demás)" "sinsmp"
    entrada "Hispa-64: la tarea ociosa con hlt (si da problemas con mwait)" "ocioso=hlt"
    entrada "Hispa-64: prueba de un fallo en otro procesador (se paran todos y uno lo explica)" "prueba=panicoap"
    entrada "Hispa-64: sin el temporizador del APIC (si se queda parado en el tic)" "prueba=sintic"
    entrada "Hispa-64: apagar el equipo por ACPI (prueba)" "prueba=acpiapagar"
    entrada "Hispa-64: la prueba larga del montón (100.000 reservas)" "prueba=monton"
    entrada "Hispa-64: prueba de un doble fallo (desbordar la pila, a propósito)" "prueba=doble"
    entrada "Hispa-64: prueba de un fallo de página del núcleo (a propósito)" "prueba=pagina"
    cat <<'CFG'
else
  menuentry "Este procesador no es de 64 bits: Hispa-64 no puede arrancar aquí (use Hispa-Pentium)" {
    echo "Hispa-64 necesita un procesador de 64 bits (x86-64). Use Hispa-Pentium."
    sleep 30
  }
fi
menuentry "Entrar en la configuración del firmware (UEFI)" {
  fwsetup
}
menuentry "Apagar" {
  halt
}
CFG
  } > "$S/grub.cfg"
fi
# el disco: 512 MB, GPT, la partición EFI (80 MB), la de pruebas (40 MB) y
# [etapa 11 a] la casa (el resto: FAT32 con el diario en los sectores
# reservados y la marca de Casa64, hazcasa.py): /casa, de escribir de
# verdad. Con WEB=1, 1,75 GB (la EFI de 1100 MB). Las particiones se copian
# sin los ceros (dd sparse): el .img y el .vdi no crecen por lo vacío.
I="$S/hispa64.img"; P="$S/esp.part"; Q="$S/pruebas.part"; K="$S/casa.part"
rm -f "$I" "$P" "$Q" "$K"
TAM=512; ESP=80; PRB=40
[ "${WEB:-0}" = 1 ] && { TAM=1792; ESP=1100; }
truncate -s ${TAM}M "$I"
sgdisk -n 1:2048:+${ESP}M -t 1:ef00 -c 1:HISPA64 -n 2:0:+${PRB}M -t 2:0700 -c 2:PRUEBAS -n 3:0:0 -t 3:0700 -c 3:CASA "$I" > /dev/null
SECT=$(( ESP * 2048 ))
read -r I2 F2 <<< "$(sgdisk -i 2 "$I" | awk '/First sector/ {a=$3} /Last sector/ {b=$3} END {print a, b}')"
read -r I3 F3 <<< "$(sgdisk -i 3 "$I" | awk '/First sector/ {a=$3} /Last sector/ {b=$3} END {print a, b}')"
mkfs.fat -F 32 -n HISPA64 -C "$P" $(( SECT / 2 )) > /dev/null
mmd -i "$P" ::EFI ::EFI/BOOT ::boot ::boot/grub
mcopy -i "$P" "$S/BOOTX64.EFI" ::EFI/BOOT/BOOTX64.EFI
mcopy -i "$P" "$S/grub.cfg" ::boot/grub/grub.cfg
mcopy -i "$P" "$S/HISPA64.BIN" ::boot/HISPA64.BIN
mcopy -i "$P" "$S/HISPA64.SIM" ::boot/HISPA64.SIM
mmd -i "$P" ::boot/prog
for f in $PROGS; do mcopy -i "$P" "$S/prog/$f" "::boot/prog/$f"; done
dd if="$P" of="$I" bs=512 seek=2048 conv=notrunc,sparse status=none
rm -f "$P"
# [etapa 5] la de pruebas: FAT32 con cúmulos de 512 octetos, la marca y unos archivos
mkfs.fat -F 32 -s 1 -n PRUEBAS -C "$Q" $(( (F2 - I2 + 1) / 2 )) > /dev/null
printf 'Disco de pruebas de Hispa-64: se puede escribir en \xc3\xa9l.\r\n' > "$S/marca.txt"
printf 'Hola desde el anfitri\xc3\xb3n: este disco es de prueba.\n' > "$S/leeme.txt"
printf 'Un nombre largo, con acentos y e\xc3\xb1e.\n' > "$S/largo.txt"
python3 -I -c '
import sys
sys.stdout.buffer.write(bytes(((i * 131 + (i >> 8)) & 255) for i in range(300000)))' > "$S/patron.bin"
mcopy -i "$Q" "$S/marca.txt" ::HISPA64.PRB
mcopy -i "$Q" "$S/leeme.txt" ::LEEME.TXT
mcopy -i "$Q" "$S/patron.bin" ::PATRON.BIN
mmd -i "$Q" "::Carpeta con nombre largo"
mcopy -i "$Q" "$S/largo.txt" "::Carpeta con nombre largo/Canción de cuna ñandú.txt"
dd if="$Q" of="$I" bs=512 seek="$I2" conv=notrunc,sparse status=none
rm -f "$Q" "$S/marca.txt" "$S/leeme.txt" "$S/largo.txt" "$S/patron.bin"
# [etapa 11 a] la casa: FAT32 de cúmulos de 4 KB, 32.832 sectores reservados
# (el diario, del 64 al 32.831: 16 MB) y la marca en el 32
mkfs.fat -F 32 -s 8 -R 32832 -n CASA -C "$K" $(( (F3 - I3 + 1) / 2 )) > /dev/null
python3 -I "$AQUI/programas/hazcasa.py" "$K"
printf 'Esta es la casa de Hispa-64: lo que se guarda en /casa (Documentos,\r\nDescargas, lo del navegador...) queda aqu\xc3\xad, en esta partici\xc3\xb3n.\r\n' > "$S/casa.txt"
mcopy -i "$K" "$S/casa.txt" "::LEEME.TXT"
dd if="$K" of="$I" bs=512 seek="$I3" conv=notrunc,sparse status=none
rm -f "$K" "$S/casa.txt"
if [ "$MODO" != prueba ] || [ "${VDI:-0}" = 1 ]; then
  rm -f "$S/hispa64.vdi"
  qemu-img convert -f raw -O vdi "$I" "$S/hispa64.vdi"
fi
# [etapa 4] el CD: BIOS (i386-pc, El Torito) y UEFI de 64 bits, con lo mismo
if [ "$MODO" != prueba ] || [ "${CD:-0}" = 1 ]; then
  D="$S/cd"; rm -rf "$D"; mkdir -p "$D/boot/grub" "$D/boot/prog"
  cp "$S/HISPA64.BIN" "$S/HISPA64.SIM" "$D/boot/"
  cp "$S/grub.cfg" "$D/boot/grub/grub.cfg"
  for f in $PROGS; do cp "$S/prog/$f" "$D/boot/prog/"; done
  grub-mkrescue -o "$S/hispa64-cd.iso" "$D" -- -volid HISPA64 > /dev/null 2>&1
  rm -rf "$D"
fi
