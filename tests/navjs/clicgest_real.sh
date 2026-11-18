#!/bin/bash
# [JS] El Navegador de Hispa-486 con JavaScript contra clicgest.es DE VERDAD
# (necesita Internet; no es parte de «make test»). Solo mira: entra en la
# demostración (sin clave), pasa a la versión normal (la de ordenador; al
# Navegador, ClicGest le da la ligera), despliega el menú lateral, repasa el
# tutorial de la biblioteca, escribe en el buscador de «Prestar» y mueve las
# flechas (sin elegir a nadie ni prestar nada), cambia de pestaña en una
# salida y recorre las páginas de la biblioteca. Deja las capturas (js_*.ppm)
# y NAVEGA.LOG en la carpeta que se le diga. Las posiciones de los clics son
# las de la demostración del 18/11/2026.
#    tests/navjs/clicgest_real.sh CARPETA [build/hispados.img]
# (ICOUNT="-icount shift=0" cuenta instrucciones: los ms de las líneas
# [memoria] de NAVEGA.LOG son entonces millones de instrucciones.)
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
set -u
cd "$(dirname "$0")/../.."
SAL=$(realpath -m "${1:?carpeta}")
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
aparta() { ir 300 540; }
foto() { sleep 0.4; mon "screendump $SAL/js_$1.ppm" 0.8; }
# escribe un texto, letra a letra
escribe() {
  local t=$1 i c
  for ((i = 0; i < ${#t}; i++)); do
    c=${t:i:1}
    case $c in
      ' ') tecla spc;; /) tecla slash;; .) tecla dot;; -) tecla minus;; :) tecla shift-semicolon;;
      '?') tecla shift-slash;; =) tecla equal;; '&') tecla shift-7;; [A-Z]) tecla shift-${c,,};; *) tecla $c;;
    esac
  done
}
dir() { tecla f6; sleep 1; escribe "$1"; tecla ret; sleep ${2:-60}; }
{ sleep 6; send 'A:\HISPADOS\HSPRED' 2; send 'C:\HISPADOS\VENTANAS /486' 12
  tecla ctrl-esc; sleep 1; tecla i; sleep 0.5; tecla right; sleep 1; tecla ret; sleep 90; aparta; foto 01_panel_ligero
  dir "https://clicgest.es/vista?modo=normal&volver=/biblioteca" 90; tecla home; sleep 2; aparta; foto 02_biblioteca
  clic 40 88; sleep 8; aparta; foto 03_menu_abierto                   # «Menú» (el ☰): el menú lateral
  clic 40 88; sleep 8; aparta
  clic 90 340; sleep 8; aparta; foto 04_tutorial_paso1                # «Ver desde el paso 1»
  clic 205 520; sleep 8; aparta; foto 05_tutorial_paso2               # «Siguiente»
  dir https://clicgest.es/biblioteca/prestar 60; aparta; foto 06_prestar
  clic 130 466; sleep 8; escribe "socia 01"; sleep 12; aparta; foto 07_prestar_sugiere
  tecla down; sleep 6; tecla down; sleep 6; foto 08_prestar_flechas   # (sin Intro: no se elige a nadie)
  tecla esc; sleep 3
  for p in catalogo devolver lectores historial ajustes comprobar etiquetas por-atender resenas sugerencias suspensiones; do
    dir https://clicgest.es/biblioteca/$p 60; aparta; foto 1_$p
  done
  dir https://clicgest.es/eventos/1 150; aparta
  tecla pgdn; sleep 4; tecla pgdn; sleep 4; tecla pgdn; sleep 4; tecla pgdn; sleep 4; tecla pgup; sleep 6; foto 20_salida_pestanas
  clic 80 88; sleep 10; aparta; foto 21_salida_pagado_todo           # la pestaña «Pagado todo»
  mon quit; } | { rm -f $T/mon; timeout 3000 qemu-system-i386 ${ICOUNT:-} -display none -serial stdio -monitor unix:$T/mon,server,nowait \
    -parallel none -m 16 -drive file=$T/f.img,format=raw,if=floppy,index=0 -drive file=$T/c.img,format=raw,if=ide,index=0 \
    -boot a -no-reboot -netdev user,id=n0 -device rtl8139,netdev=n0 > $T/raw 2>&1; }
mcopy -n -i $T/c.img@@32256 ::HISPADOS/INTERNET/NAVEGA.LOG "$SAL/NAVEGA.LOG"
echo "Capturas y NAVEGA.LOG en $SAL"
grep -c '^\[error\]' "$SAL/NAVEGA.LOG" | sed 's/^/errores de JavaScript en el registro: /'
rm -rf $T
