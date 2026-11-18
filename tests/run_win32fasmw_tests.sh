#!/bin/bash
# Prueba de la fase 3d de Hispa-486 (docs/486/HISPA-486.md) con un programa de
# Windows de verdad: FASMW, el entorno de ventanas del flat assembler 1.73
# (licencia de fasm, de tipo BSD; lo baja tests/thirdparty/fetch_win32.sh; si
# no está, la prueba se salta). Usa casi todo lo de la fase: su editor propio
# (letras de CreateFontA, GetTextMetricsA, DrawTextA, el caret, las barras de
# desplazamiento, ToAscii), las pestañas y la barra de estado (COMCTL32.DLL
# de Hispa-486), los aceleradores, GetOpenFileNameA, su .INI
# (GetPrivateProfileStringA y WritePrivateProfileStringA) y el ensamblador en
# otro hilo (CreateThread) con su barra de progreso. Se abre HOLA486.ASM, se
# ensambla (Ctrl+F9) y el .COM que sale funciona en Hispa-DOS.
#  (GUARDAR=carpeta deja las capturas.)
# A real Win32 program (FASMW 1.73) inside Hispa Ventanas (phase 3d) test.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
DL=tests/thirdparty/dl/win32-fasmw
[ -f $DL/FASMW.EXE ] || { echo "== Hispa-486: FASMW: no está (tests/thirdparty/fetch_win32.sh), se salta =="; exit 0; }
make -s build/COMCTL32.DLL build/MSVCRT.DLL build/HW32.COM > /dev/null 2>&1 || {
  echo "== Hispa-486: FASMW: sin gcc de 32 bits, se salta =="; exit 0; }
. tests/tools/win32_ventanas.sh
echo "== Hispa-486: FASMW (flat assembler para Windows) dentro de Hispa Ventanas =="
cp $DL/FASMW.EXE build/FASMW.EXE
cp tests/progs/hola486.asm build/HOLA486.ASM
imagen FASMW.EXE COMCTL32.DLL HOLA486.ASM
rm -f build/FASMW.EXE build/HOLA486.ASM
# La ventana de FASMW sale en 96,64; su editor, de 100,108 a 462,330; sus
# pestañas, en y = 346; su barra de estado, en y = 368.
{ sleep 6; send 'VENTANAS' 9
  doble 125 408; sleep 10; aparca; shot f1
  tecla ctrl-o; sleep 2; shot f2
  tecla b i n backslash h o l a 4 8 6 dot a s m; sleep 0.5; tecla ret; sleep 3; aparca; shot f3
  tecla ctrl-end ret n o p; sleep 1.5; shot f4               # se escribe (WM_KEYDOWN + ToAscii)
  tecla ctrl-f9; sleep 8; shot f5                           # ensamblar: el hilo, la barra de progreso
  tecla ret; sleep 2; shot f6
  tecla alt-f4; sleep 3; shot f7
  mon quit; } | TMAX=240 run
echo "-- FASMW: su ventana, su menú (MENUEX), sus pestañas y su barra de estado (COMCTL32) --"
chk $T/f1.ppm titulo 96 64 activa "flat assembler 1.73.32" \
  busca 98 86 480 106 0 15 "ile" busca 98 86 480 106 0 15 "dit" busca 98 86 480 106 0 15 "ptions" \
  busca 98 340 480 364 232 249 "Untitled" busca 98 360 480 392 232 249 "1,1"
echo "-- abrir (GetOpenFileNameA), escribir y ensamblar (CreateThread) --"
chk $T/f2.ppm busca 100 100 700 400 0 8 "Nombre del archivo:"
chk $T/f3.ppm busca 98 340 480 364 232 249 "hola486.asm" busca 96 108 470 330 232 231 "org" \
  busca 96 108 470 330 232 231 "texto"
chk $T/f4.ppm busca 96 108 470 330 232 231 "nop" busca 96 360 480 392 232 249 "12,4"
chk $T/f5.ppm busca 100 80 700 400 0 8 "2 passes, 29 bytes."
echo "-- lo que queda en el disco: el .COM (que funciona) y su .INI --"
MTOOLS_SKIP_CHECK=1 mcopy -o -i $T/f.img ::BIN/HOLA486.COM $T/hola.com 2>/dev/null
if [ "$(od -An -tx1 -N9 $T/hola.com 2>/dev/null | tr -d ' \n')" = "b409ba0901cd21cd20" ] && [ $(stat -c %s $T/hola.com) = 29 ]; then
  ok "FASMW ha hecho HOLA486.COM (29 octetos, con el NOP)"; else ko "no está HOLA486.COM, o no es el que tiene que ser"; fi
if MTOOLS_SKIP_CHECK=1 mtype -i $T/f.img ::BIN/FASMW.INI 2>/dev/null | grep -q '^\[Environment\]\|^\[Options\]'; then
  ok "FASMW ha guardado su FASMW.INI (WritePrivateProfileStringA)"; else ko "no está FASMW.INI"; fi
fin
