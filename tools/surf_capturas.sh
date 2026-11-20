#!/bin/bash
# Capturas del Navegador completo (NAVEGA2.EXE) en QEMU con páginas de verdad
# (por Internet, la red de QEMU), para compararlas con Chrome
# (docs/486/NAVEGADOR2.md, «Comparado con Chrome»):
#   bash tools/surf_capturas.sh CARPETA URL1 [URL2 ...]
# Abre URL1 (Inicio= de NAVEGA2.INI) y luego cada una de las otras por la
# dirección (F6); deja CARPETA/01.png, 02.png... y SURF.LOG. Cada dirección se
# escribe con el teclado de QEMU (minúsculas, cifras y / : . - ? = _).
# ESPERA=segundos por página (40), MEM=MB (32).
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
set -u
cd "$(dirname "$0")/.."
SAL=$1; shift
mkdir -p "$SAL"
make -s build/NAVEGA2.EXE build/HSPTLS.DLL build/WSOCK32.DLL build/MSVCRT.DLL build/HW32.COM build/hispados.img > /dev/null || exit 1
IMG=build/hispados.img
. tests/tools/win32_ventanas.sh
. tests/tools/surf_teclas.sh
export MTOOLS_SKIP_CHECK=1
cp "$IMG" $T/f.img
printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nFILES=30\r\n' > $T/CONFIG.SYS
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
mkdir -p $T/c
mcopy -s -n -i "$IMG" ::HISPADOS $T/c/
I=$T/c/HISPADOS/INTERNET
mkdir -p $I/RECURSOS $I/FUENTES
cp $B/MSVCRT.DLL $B/WSOCK32.DLL $B/HSPTLS.DLL $B/NAVEGA2.EXE $I/
cp $B/HW32.COM $B/MSVCRT.DLL $T/c/HISPADOS/
cp w32/surf/recursos/* terceros/netsurf/netsurf/resources/{default,quirks,adblock,internal}.css $I/RECURSOS/
cp terceros/dejavu/*.TTF $I/FUENTES/
: > $I/SURF.LOG
printf 'Inicio=%s\r\n' "$1" > $I/NAVEGA2.INI
shift
python3 tests/tools/mkhdd.py $T/c.img 16 $T/c/HISPADOS > /dev/null
ESPERA=${ESPERA:-40}
{ sleep 6; send 'A:\HISPADOS\HSPRED' 2; send 'C:\HISPADOS\VENTANAS /486' 10
  tecla ctrl-esc; sleep 1; tecla i; sleep 0.5; tecla right; sleep 1; tecla down; sleep 0.5; tecla ret
  sleep $((ESPERA + 15)); ir 790 300; shot 01
  n=1
  for u in "$@"; do
    n=$((n + 1))
    tecla f6; sleep 1; escribe "$u"; tecla ret; sleep $ESPERA; ir 790 300; shot $(printf %02d $n)
  done
  tecla alt-f4; sleep 3
  mon quit; } | {
  rm -f $T/mon
  timeout 1800 qemu-system-i386 -vga std -global VGA.xres=800 -global VGA.yres=600 -display none -serial stdio -monitor unix:$T/mon,server,nowait \
    -parallel none -m ${MEM:-32} -drive file=$T/f.img,format=raw,if=floppy,index=0 -drive file=$T/c.img,format=raw,if=ide,index=0 \
    -boot a -no-reboot -netdev user,id=n0 -device rtl8139,netdev=n0 > $T/raw 2>&1; }
mcopy -n -i $T/c.img@@32256 ::HISPADOS/INTERNET/SURF.LOG "$SAL"/SURF.LOG 2> /dev/null
mcopy -n -i $T/c.img@@32256 ::HISPADOS/INTERNET/NS.LOG "$SAL"/NS.LOG 2> /dev/null
for f in $T/[0-9]*.ppm; do python3 -I tests/tools/ppm2png.py $f "$SAL"/$(basename ${f%.ppm}).png; done
rm -rf $T
ls "$SAL"
