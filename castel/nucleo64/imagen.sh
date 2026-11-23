#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo64/imagen.sh - Hispa-64 (docs/64/HISPA-64.md): hace el núcleo y la
# imagen de arranque por UEFI de 64 bits.
#   imagen.sh SALIDA [prueba [LÍNEA]]
# (solo los drivers de pantalla del firmware, efi_gop y efi_uga: con los de
# QEMU, como video_bochs, GRUB cambiaría de modo y no dejaría el del GOP)
# deja en la carpeta SALIDA:
#   HISPA64.BIN                     el núcleo (ccastel.py y NASM)
#   HISPA64.SIM                     los nombres de sus funciones (simbolos.py, con
#                                   el mapa de NASM): GRUB lo carga como módulo
#   hispa64.img                     el disco (GPT, una partición EFI en FAT32
#                                   con \EFI\BOOT\BOOTX64.EFI, el GRUB de
#                                   x86_64-efi con lo que usa el menú dentro,
#                                   \boot\grub\grub.cfg, \boot\HISPA64.BIN y
#                                   \boot\HISPA64.SIM)
# Con «prueba», el menú no espera, escribe por el puerto serie y el núcleo
# arranca con la línea de órdenes LÍNEA («apagar» si no se dice: acaba en
# QEMU con el código 42): lo usa pruebas.sh.
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
  boot
}
CFG
else
  cat > "$S/grub.cfg" <<'CFG'
# Hispa-64 0.2.0 (etapa 2). Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
set timeout=5
set default=0
serial --unit=0 --speed=115200
terminal_input console serial
terminal_output console serial
# la pantalla, tal cual la deja el firmware (su resolución de siempre)
set gfxpayload=keep
if cpuid -l; then
  menuentry "Hispa-64 0.2.0 (etapa 2: ACPI, APIC, HPET y PCIe)" {
    multiboot2 /boot/HISPA64.BIN
    module2 /boot/HISPA64.SIM HISPA64.SIM
    boot
  }
  menuentry "Hispa-64: sin el temporizador del APIC (si se queda parado en el tic)" {
    multiboot2 /boot/HISPA64.BIN prueba=sintic
    module2 /boot/HISPA64.SIM HISPA64.SIM
    boot
  }
  menuentry "Hispa-64: apagar el equipo por ACPI (prueba)" {
    multiboot2 /boot/HISPA64.BIN prueba=acpiapagar
    module2 /boot/HISPA64.SIM HISPA64.SIM
    boot
  }
  menuentry "Hispa-64: la prueba larga del montón (100.000 reservas)" {
    multiboot2 /boot/HISPA64.BIN prueba=monton
    module2 /boot/HISPA64.SIM HISPA64.SIM
    boot
  }
  menuentry "Hispa-64: prueba de un doble fallo (desbordar la pila, a propósito)" {
    multiboot2 /boot/HISPA64.BIN prueba=doble
    module2 /boot/HISPA64.SIM HISPA64.SIM
    boot
  }
  menuentry "Hispa-64: prueba de un fallo de página (a propósito)" {
    multiboot2 /boot/HISPA64.BIN prueba=pagina
    module2 /boot/HISPA64.SIM HISPA64.SIM
    boot
  }
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
fi
# el disco: 64 MB, GPT, la partición EFI desde el sector 2048 hasta el final
I="$S/hispa64.img"; P="$S/esp.part"
rm -f "$I" "$P"
truncate -s 64M "$I"
sgdisk -n 1:2048:0 -t 1:ef00 -c 1:HISPA64 "$I" > /dev/null
SECT=$(( $(stat -c %s "$I") / 512 - 2048 - 33 ))
mkfs.fat -F 32 -n HISPA64 -C "$P" $(( SECT / 2 )) > /dev/null
mmd -i "$P" ::EFI ::EFI/BOOT ::boot ::boot/grub
mcopy -i "$P" "$S/BOOTX64.EFI" ::EFI/BOOT/BOOTX64.EFI
mcopy -i "$P" "$S/grub.cfg" ::boot/grub/grub.cfg
mcopy -i "$P" "$S/HISPA64.BIN" ::boot/HISPA64.BIN
mcopy -i "$P" "$S/HISPA64.SIM" ::boot/HISPA64.SIM
dd if="$P" of="$I" bs=512 seek=2048 conv=notrunc status=none
rm -f "$P"
