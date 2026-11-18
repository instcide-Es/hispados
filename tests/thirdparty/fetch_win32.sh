#!/bin/bash
# Descarga programas Win32 libres (de consola) para probar HW32, el cargador
# de Hispa-486, con programas reales. No se incluyen en el repositorio: se
# guardan en tests/thirdparty/dl/ (ignorado).
#   NASM 2.16.03 para Win32 (BSD de 2 cláusulas, de nasm.us; hecho con MinGW:
#   KERNEL32 + msvcrt.dll)
#   Tiny C Compiler 0.9.27 para Win32 (LGPL, de savannah.gnu.org): un
#   compilador de C que hace programas de Windows
#   [fase 3d] flat assembler 1.73.32 para Windows (licencia de fasm, de tipo
#   BSD, de flatassembler.net): FASMW.EXE, un entorno de ventanas (editor
#   propio, pestañas, barra de estado, aceleradores, diálogos comunes, un
#   .INI y el ensamblador en otro hilo) que se prueba dentro de Hispa Ventanas
# Downloads free Win32 console programs used only as a test for HW32.
set -eu
DIR=$(cd "$(dirname "$0")" && pwd)/dl
mkdir -p "$DIR/win32-nasm"
if [ ! -f "$DIR/win32-nasm/.ok" ]; then
  curl -sfL --max-time 180 -o "$DIR/win32-nasm/nasm.zip" \
    https://www.nasm.us/pub/nasm/releasebuilds/2.16.03/win32/nasm-2.16.03-win32.zip \
    && (cd "$DIR/win32-nasm" && unzip -qo nasm.zip && touch .ok) && echo "  nasm (win32)" \
    || echo "  (no se pudo bajar nasm para win32)"
fi
mkdir -p "$DIR/win32-tcc"
if [ ! -f "$DIR/win32-tcc/.ok" ]; then
  curl -sfL --max-time 180 -o "$DIR/win32-tcc/tcc.zip" \
    https://download.savannah.gnu.org/releases/tinycc/tcc-0.9.27-win32-bin.zip \
    && (cd "$DIR/win32-tcc" && unzip -qo tcc.zip && touch .ok) && echo "  tcc (win32)" \
    || echo "  (no se pudo bajar tcc para win32)"
fi
mkdir -p "$DIR/win32-fasmw"
if [ ! -f "$DIR/win32-fasmw/.ok" ]; then
  curl -sfL --max-time 180 -o "$DIR/win32-fasmw/fasmw.zip" https://flatassembler.net/fasmw17332.zip \
    && (cd "$DIR/win32-fasmw" && unzip -qo fasmw.zip && touch .ok) && echo "  fasmw (win32)" \
    || echo "  (no se pudo bajar fasmw para win32)"
fi
