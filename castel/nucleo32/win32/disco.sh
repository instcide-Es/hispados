#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo32/win32/disco.sh IMAGEN
# El disco de las pruebas de los programas de Windows (castel/pruebas.sh,
# «Los programas de Windows»): el de nucleo32/disco.sh (FAT32, con el
# intérprete de órdenes, la Consola, el Bloc y Mi equipo) y, además,
# WIN32.BIN, la carpeta \WIN32 (las DLL), \HISPA32 (los programas de
# ejemplo de Hispa-486: CALC32, HOLA32, CTL32, MENU32, AVISO32, NAVES),
# \PRUEBAS (los de las pruebas de HW32: PEHOLA, HOLAC, K32PRUE, USADLL,
# REDPRUE...), el servicio de red (RED.BIN) y,
# si se han bajado (tests/thirdparty/fetch_win32.sh), NASM, TCC y el flat
# assembler (FASM, FASMW y su ejemplo DDRAW).
set -e
export LC_ALL=C.UTF-8 MTOOLS_SKIP_CHECK=1
AQUI=$(dirname "$(readlink -f "$0")")
RAIZ=$(readlink -f "$AQUI/../../..")
IMG=$(readlink -f "${1:?uso: disco.sh IMAGEN}")
T=$(mktemp -d)
trap 'rm -rf $T' EXIT
bash $AQUI/../disco.sh $T/base.img 32 red > /dev/null
bash $AQUI/hacer.sh $T/WIN32.BIN $T/w32
cd "$RAIZ"
make -s build/GLPRUEBA.EXE build/ENGRANA.EXE build/PEHOLA.EXE build/HOLAC.EXE build/K32PRUE.EXE build/USADLL.EXE build/DLLPRUE.DLL build/REDPRUE.EXE > /dev/null
V=$T/base.img
mcopy -i $V $T/WIN32.BIN ::
mmd -i $V ::WIN32 ::HISPA32 ::PRUEBAS
mcopy -i $V $T/w32/* ::WIN32/
for p in WINCALC:CALC32 WINHOLA:HOLA32 WINCTL:CTL32 WINMENU:MENU32 WINGUI:AVISO32 NAVES:NAVES GLPRUEBA:GLPRUEBA ENGRANA:ENGRANA; do   # [GL] los de HispaGL
  mcopy -i $V build/${p%%:*}.EXE ::HISPA32/${p##*:}.EXE
done
mcopy -i $V build/PEHOLA.EXE build/HOLAC.EXE build/K32PRUE.EXE build/USADLL.EXE build/DLLPRUE.DLL build/REDPRUE.EXE ::PRUEBAS/
D=tests/thirdparty/dl
# [GL] wglgears (los engranajes de los demos de Mesa, MIT), si se ha bajado (tests/thirdparty/fetch_gl.sh)
if [ -f $D/gl/wglgears.c ] && make -s build/WGLGEARS.EXE > /dev/null 2>&1; then
  mcopy -i $V build/WGLGEARS.EXE ::PRUEBAS/
fi
if [ -f $D/win32-nasm/nasm-2.16.03/nasm.exe ]; then
  mmd -i $V ::NASM
  mcopy -i $V $D/win32-nasm/nasm-2.16.03/nasm.exe ::NASM/NASM.EXE
  printf 'section .text\r\nglobal _start\r\n_start:\r\n  mov eax, 1\r\n  ret\r\n' > $T/HOLA.ASM
  mcopy -i $V $T/HOLA.ASM ::NASM/
fi
if [ -f $D/win32-tcc/tcc/tcc.exe ]; then
  mmd -i $V ::TCC
  mcopy -s -i $V $D/win32-tcc/tcc/tcc.exe $D/win32-tcc/tcc/libtcc.dll $D/win32-tcc/tcc/include $D/win32-tcc/tcc/lib ::TCC/
  printf '#include <stdio.h>\r\nint main(void)\r\n{\r\n    printf("Hola desde TCC en Hispa-Pentium\\n");\r\n    return 7;\r\n}\r\n' > $T/HOLA.C
  mcopy -i $V $T/HOLA.C ::TCC/
fi
if [ -f $D/win32-fasmw/FASMW.EXE ]; then
  mmd -i $V ::FASM ::FASM/DDRAW
  mcopy -s -i $V $D/win32-fasmw/FASM.EXE $D/win32-fasmw/FASMW.EXE $D/win32-fasmw/INCLUDE ::FASM/
  mcopy -i $V $D/win32-fasmw/EXAMPLES/DDRAW/DDRAW.ASM $D/win32-fasmw/EXAMPLES/DDRAW/DDRAW.INC $D/win32-fasmw/EXAMPLES/DDRAW/GIF87A.INC $D/win32-fasmw/EXAMPLES/DDRAW/DDRAW.GIF ::FASM/DDRAW/
  mcopy -i $V $D/win32-fasmw/EXAMPLES/HELLO/HELLO.ASM ::FASM/ 2> /dev/null || true
fi
cp $V "$IMG"
echo "win32/disco.sh: $IMG, $(($(stat -c %s "$IMG") / 1024)) K"
