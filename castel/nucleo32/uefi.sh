#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo32/uefi.sh: (UEFI) lo del arranque por UEFI que comparten la memoria
# USB (usb.sh) y el CD (cd.sh). Se lee con «. uefi.sh» con $AQUI (la carpeta
# nucleo32). Deja:
#   $EFI_PLAT  las plataformas de GRUB EFI que hay en este equipo, de entre
#              x86_64-efi (los PC de 64 bits: BOOTX64.EFI) e i386-efi (los de
#              firmware de 32 bits, como los Bay Trail: el Acer One 10, el
#              Medion: BOOTIA32.EFI); vacío si no hay ninguna (entonces la
#              imagen arranca solo por la BIOS, como antes de la 0.8.8). Vienen
#              de los paquetes grub-efi-amd64-bin y grub-efi-ia32-bin.
# y las funciones:
#   haz_efi CARPETA   pone en CARPETA/EFI/BOOT/ el GRUB de cada plataforma,
#                     con todo lo que usa el menú dentro (no hace falta la
#                     carpeta de módulos); busca su grub.cfg en /boot/grub de
#                     la misma partición de la que lo ha cargado el firmware
#   menu_efi TITULO NUCLEO LINEA MODULOS2 GFXPAYLOAD [DESPUÉS]
#                     una entrada del menú para UEFI: multiboot2 y module2
#   a_module2 TEXTO   los «module» de $MODULOS o $INSTALA, como «module2»
#   leeme_uefi CARPETA  pone LEEME-UEFI.TXT (Secure Boot y lo demás)
#
# Con UEFI: GRUB EFI arranca el núcleo por multiboot2 (la segunda cabecera
# que pone Castel: ccastel.py, C_multiboot2) y le da la pantalla que deja el
# firmware (GOP), el mapa de memoria, la RSDP de ACPI y los módulos, y sale
# de los servicios de arranque del firmware antes de entrar. Sin firmar: con
# Secure Boot encendido el firmware no lo arranca (LEEME-UEFI.TXT).
EFI_PLAT=
for _p in x86_64-efi i386-efi; do
  [ -f /usr/lib/grub/$_p/multiboot2.mod ] && [ -f /usr/lib/grub/$_p/efi_gop.mod ] && EFI_PLAT="$EFI_PLAT $_p"
done
EFI_PLAT=${EFI_PLAT# }
[ -n "${SIN_UEFI:-}" ] && EFI_PLAT=      # (para las pruebas: la imagen de antes, solo BIOS)
EFI_MODULOS="normal configfile part_msdos part_gpt fat iso9660 multiboot2 efi_gop efi_uga video_fb videoinfo read echo test loadenv search search_fs_uuid sleep ls cat reboot halt efifwsetup lsmmap lspci lsacpi lsefisystab cpuid"

haz_efi() {
  local d=$1/EFI/BOOT p n
  mkdir -p "$d"
  for p in $EFI_PLAT; do
    case $p in x86_64-efi) n=BOOTX64.EFI ;; i386-efi) n=BOOTIA32.EFI ;; esac
    # shellcheck disable=SC2086
    grub-mkimage -O $p -o "$d/$n" -p /boot/grub $EFI_MODULOS || return 1
  done
}

a_module2() { printf '%s' "$1" | sed 's/^  module /  module2 /'; }

menu_efi() {
  printf 'menuentry "%s" {\n  multiboot2 /%s %s\n%s' "$1" "$2" "$3" "$4"
  [ -n "${5:-}" ] && printf '  set gfxpayload=%s\n' "$5"
  [ -n "${6:-}" ] && printf '%s\n' "$6"
  printf '  boot\n}\n'
}

leeme_uefi() {
  sed 's/$/\r/' "$AQUI/uefi/LEEME-UEFI.TXT" > "$1/LEEME-UEFI.TXT"
}
