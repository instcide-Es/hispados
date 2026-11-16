#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# probar.sh PROGRAMA.COM [NOMBRE_EN_DOS]
# Ejecuta un .COM dentro de Hispa-DOS (imagen de disquete) por QEMU y ensena su
# salida (capturada por el puerto serie). Necesita qemu-system-i386, mtools y la
# imagen build/hispados-disquete.img de este repositorio (make usb).
set -u
COM=$1
NOM=${2:-$(basename "${COM%.*}" | tr 'a-z' 'A-Z')}
# Ruta a la imagen de Hispa-DOS (ajustar si el repo esta en otro sitio).
AQUI=$(dirname "$(readlink -f "$0")")
IMG=${HISPADOS_IMG:-$AQUI/../build/hispados-disquete.img}   # la de este repositorio (make usb)
[ -f "$IMG" ] || { echo "No encuentro la imagen de Hispa-DOS: $IMG"; echo "Pon la ruta en HISPADOS_IMG."; exit 2; }
T=$(mktemp -d)
cp "$IMG" "$T/f.img"
mcopy -o -i "$T/f.img" "$COM" "::$NOM.COM"
{ sleep 7
  printf '%s\r' "$NOM"; sleep 3
  printf '%s\r' "ECHO [fin]"; sleep 1.5
} | timeout 90 qemu-system-i386 -display none -serial stdio -monitor none -parallel none \
    -drive file="$T/f.img",format=raw,if=floppy -boot a -no-reboot > "$T/log" 2>&1
tr -d '\r' < "$T/log" | iconv -f CP437 -t UTF-8 2>/dev/null > "$T/log.txt"
awk -v n="$NOM" 'index($0,n){v=1} v' "$T/log.txt" | sed '/\[fin\]/q'
rm -rf "$T"
