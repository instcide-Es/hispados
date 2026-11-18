#!/bin/bash
# [JS] El Navegador de Hispa-486 con JavaScript contra clicgest.es DE VERDAD
# (necesita Internet; no es parte de «make test»). Solo mira: entra en la
# demostración (sin clave), abre la biblioteca, despliega el menú lateral,
# repasa el tutorial, escribe en el buscador de «Prestar» (sin elegir a
# nadie ni prestar nada), cambia de pestaña en una salida y recorre las
# páginas de la biblioteca. Deja las capturas y NAVEGA.LOG en la carpeta
# que se le diga.
#    tests/navjs/clicgest_real.sh CARPETA [build/hispados.img]
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
set -u
cd "$(dirname "$0")/../.."
SAL=${1:?carpeta}
IMG=${2:-build/hispados.img}
mkdir -p "$SAL"
make -s build/NAVEGA.EXE build/HSPJS.DLL build/HSPTLS.DLL build/WSOCK32.DLL build/MSVCRT.DLL build/HW32.COM || exit 1
. tests/tools/win32_ventanas.sh
export MTOOLS_SKIP_CHECK=1
cp "$IMG" $T/f.img
printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nFILES=30\r\n' > $T/CONFIG.SYS
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
mkdir -p $T/c
mcopy -s -n -i "$IMG" ::HISPADOS $T/c/
mkdir -p $T/c/HISPADOS/INTERNET
cp $B/MSVCRT.DLL $B/WSOCK32.DLL $B/HSPTLS.DLL $B/HSPJS.DLL $B/NAVEGA.EXE $T/c/HISPADOS/INTERNET/
cp $B/HW32.COM $B/MSVCRT.DLL $T/c/HISPADOS/
printf 'Inicio=https://clicgest.es/demo\r\n' > $T/c/HISPADOS/INTERNET/NAVEGA.INI
python3 tests/tools/mkhdd.py $T/c.img 16 $T/c/HISPADOS > /dev/null
aparta() { ir 300 520; }
fy() { echo $((89 + 18 * $1)); }
dir() {
  local c
  tecla f6; sleep 1
  tecla h t t p s shift-semicolon slash slash c l i c g e s t dot e s
  for c in $(echo "$1" | sed 's/./& /g'); do case $c in /) tecla slash;; -) tecla minus;; ?) tecla $c;; esac; done
  tecla ret; sleep ${2:-20}
}
{ sleep 6; send 'A:\HISPADOS\HSPRED' 2; send 'C:\HISPADOS\VENTANAS /486' 10
  tecla ctrl-esc; sleep 1; tecla i; sleep 0.5; tecla right; sleep 1; tecla ret; sleep 45; aparta; shot r01_panel
  dir /biblioteca 25; aparta; shot r02_biblioteca
  tecla home; sleep 2; shot r03_arriba
  clic 30 $(fy 0); sleep 6; aparta; shot r04_menu_abierto                 # «Menú» (☰): el menú lateral
  clic 30 $(fy 0); sleep 6; aparta; shot r05_menu_cerrado
  dir /biblioteca/prestar 25; aparta; shot r06_prestar
  for k in 1 2 3; do tecla tab; sleep 1; done; shot r07_tab
  mon "screendump $T/x.ppm"; sleep 1
  { } ; mon quit; } | true
echo "(esqueleto: ver el guion de abajo)"
