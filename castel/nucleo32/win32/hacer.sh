#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo32/win32/hacer.sh SALIDA [CARPETA_DLL [CARPETA_EJEMPLOS [CARPETA_MENU]]]
# Hace WIN32.BIN, el que corre los programas de Windows de 32 bits en
# Hispa-Pentium (docs/pentium/WIN32.md): el cargador de PE y KERNEL32,
# USER32, GDI32, COMDLG32, ADVAPI32, SHELL32 y WSOCK32 en C, enlazado para
# la dirección 0 de los segmentos de Win32 (base 0x40000000). Con
# CARPETA_DLL, deja ahí además lo de la carpeta \WIN32: las DLL de Hispa-486
# que valen tal cual (MSVCRT, UCRT, COMCTL32, DDRAW, DSOUND, DINPUT, OLE32;
# WINMM, la de Pentium: su hora sale de GetTickCount). Con
# CARPETA_EJEMPLOS, los programas de ejemplo de \HISPA32 (CALC32, HOLA32,
# CTL32, MENU32, AVISO32 y NAVES, con su icono NAVES.ICP); con
# CARPETA_MENU, el acceso de Naves para el menú Inicio (NAVES.MNU). Los
# objetos, en build/win32-pentium/. Necesita gcc (con -m32), nasm, ld,
# objcopy, make y python3. Sin «make -j».
set -e
AQUI=$(dirname "$(readlink -f "$0")")
RAIZ=$(readlink -f "$AQUI/../../..")
SAL=$(readlink -m "${1:?uso: hacer.sh SALIDA [CARPETA_DLL [CARPETA_EJEMPLOS [CARPETA_MENU]]]}")
mkdir -p "$(dirname "$SAL")"
CAR=${2:+$(readlink -m "$2")}
EJE=${3:+$(readlink -m "$3")}
MNU=${4:+$(readlink -m "$4")}
cd "$RAIZ"
O=build/win32-pentium
mkdir -p $O
BF="-m32 -std=gnu11 -march=i586 -O2 -ffreestanding -fno-pic -fno-stack-protector -fno-asynchronous-unwind-tables
    -fno-builtin -fno-tree-loop-distribute-patterns -ffunction-sections -fdata-sections -fno-strict-aliasing
    -fexec-charset=CP1252 -Wall -Wno-unused -Wno-unused-function -Werror=implicit-function-declaration -Werror=int-conversion -I$O -I$AQUI"
FUENTES="sys libc monton cargador principal kernel32 archivos user32 controles menus dialogos recursos gdi32 dx comdlg32 otros red"
python3 -I $AQUI/hazdatos.py $O/datos.h
LISTA=
for f in $FUENTES; do [ -f $AQUI/$f.c ] && LISTA="$LISTA $AQUI/$f.c"; done
python3 -I $AQUI/hazexp.py $O/exportan.c $LISTA
OBJ=
for f in $LISTA $O/exportan.c; do
  o=$O/$(basename $f .c).o
  # shellcheck disable=SC2086
  gcc $BF -c $f -o $o
  OBJ="$OBJ $o"
done
nasm -f elf32 $AQUI/entrada.asm -o $O/entrada.o
# shellcheck disable=SC2086
ld -m elf_i386 --no-warn-rwx-segments -z noexecstack --gc-sections -T $AQUI/pentium.ld -o $O/win32.elf $O/entrada.o $OBJ
objcopy -O binary $O/win32.elf "$SAL"
if [ -n "$CAR" ]; then
  mkdir -p "$CAR"
  make -s build/MSVCRT.DLL build/UCRT.DLL build/COMCTL32.DLL build/DDRAW.DLL build/DSOUND.DLL build/DINPUT.DLL build/OLE32.DLL \
    build/WINCALC.EXE build/WINHOLA.EXE build/WINCTL.EXE build/WINMENU.EXE build/WINGUI.EXE build/NAVES.EXE > /dev/null
  # WINMM.DLL de Pentium: la misma, con la hora de GetTickCount (no hay tics de la BIOS)
  gcc -m32 -std=gnu11 -O2 -ffreestanding -fno-pic -fno-stack-protector -fno-asynchronous-unwind-tables -fno-builtin \
    -fno-tree-loop-distribute-patterns -DHISPA_PENTIUM -c w32/dx/winmm.c -o $O/winmm.o
  python3 tools/elf2pe.py --dll --exp "$(nm -g --defined-only $O/winmm.o | awk '$2=="T"{print $3}' | grep -v '^mem' | paste -sd,)" -t ventanas/win32.inc -o "$CAR/WINMM.DLL" $O/winmm.o
  for d in MSVCRT UCRT COMCTL32 DDRAW DSOUND DINPUT OLE32; do cp build/$d.DLL "$CAR/"; done
fi
if [ -n "$EJE" ]; then
  mkdir -p "$EJE"
  make -s build/WINCALC.EXE build/WINHOLA.EXE build/WINCTL.EXE build/WINMENU.EXE build/WINGUI.EXE build/NAVES.EXE > /dev/null
  for p in WINCALC:CALC32 WINHOLA:HOLA32 WINCTL:CTL32 WINMENU:MENU32 WINGUI:AVISO32 NAVES:NAVES; do
    cp build/${p%%:*}.EXE "$EJE/${p##*:}.EXE"
  done
  python3 -I $AQUI/hazicono.py "$EJE/NAVES.ICP"
fi
if [ -n "$MNU" ]; then
  mkdir -p "$MNU"
  printf 'nombre=Naves\r\nprograma=\\HISPA32\\NAVES.EXE\r\nicono=\\HISPA32\\NAVES.ICP\r\nescritorio=no\r\n' > "$MNU/NAVES.MNU"
fi
