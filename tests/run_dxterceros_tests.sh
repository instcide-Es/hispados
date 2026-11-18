#!/bin/bash
# Prueba de la fase 5 de Hispa-486 (docs/486/HISPA-486.md) con programas de
# DirectX de otros, libres (los baja tests/thirdparty/fetch_dx.sh; si no
# están, cada parte se salta). Cada uno, en un disquete B: aparte:
#  1. El ejemplo de DirectDraw del flat assembler (EXAMPLES/DDRAW, licencia
#     de fasm, tipo BSD): FASM.EXE lo ensambla DENTRO de Hispa-DOS (HW32) y
#     DDRAW.EXE corre en pantalla completa: 640×480 a 8 bits con su paleta,
#     un GIF cargado con Lock, BltFast con clave de color y Flip; Esc acaba y
#     devuelve la pantalla.
#  2. TinyPTC 0.8 (dominio público / LGPL): dos .EXE comprimidos con UPX que
#     cargan DDRAW.DLL con LoadLibraryA. fullscreen.exe: 320×200 en 32 bits
#     (convertidos a la pantalla de 8 bits); windowed.exe: en una ventana,
#     con un recortador y Blt a la primaria.
#  3. FCE Ultra 0.98.8 (GPL), un emulador de NES de MinGW con DirectDraw 7,
#     DirectInput 7, DirectSound, OLE32 y menús, con el juego de NES propio
#     de Hispa-DOS (HISPA1.NES, «Cometas»).
#  (GUARDAR=carpeta deja las capturas.)
# Third-party free DirectX programs (phase 5) test.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
DL=tests/thirdparty/dl
make -s build/MSVCRT.DLL build/HW32.COM build/WINMM.DLL build/DDRAW.DLL build/DSOUND.DLL build/DINPUT.DLL build/OLE32.DLL build/WINDX.EXE > /dev/null 2>&1 || {
  echo "== Hispa-486: DirectX de otros: sin gcc de 32 bits, se salta =="; exit 0; }
. tests/tools/win32_ventanas.sh
CHK="python3 -I tests/tools/ventanas_check.py"
hasta() {
  local nom=$1 fin=$(( SECONDS + $2 )); shift 2
  while :; do
    printf 'screendump %s\n' "$T/$nom.ppm" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1
    sleep 0.3
    $CHK $T/$nom.ppm "$@" > /dev/null 2>&1 && return 0
    [ $SECONDS -ge $fin ] && return 1
  done
}
# prepara $T/f.img (con las DLL y un grupo «DirectX» con los accesos que se
# digan: «programa|parámetros|carpeta» ...) y $T/b.img (vacío, para lo que
# se ponga)
prepara() {
  local a
  imagen WINDX.EXE
  MTOOLS_SKIP_CHECK=1 mdel -i $T/f.img ::BIN/WINDX.EXE
  for f in WINMM.DLL DDRAW.DLL DSOUND.DLL DINPUT.DLL OLE32.DLL; do
    MTOOLS_SKIP_CHECK=1 mcopy -o -i $T/f.img $B/$f ::HISPADOS/$f || ko "no cabe $f en el disco"
  done
  local ini=('[Centro]' 'Ventana=36,52,452,190' 'Activo=0' 'SinRepetidos=1' 'TareasPrincipal=1' \
    '[Grupo]' 'Nombre=DirectX' 'Ventana=60,330,600,200' 'Estado=1')
  local p q c
  for a in "$@"; do IFS='|' read -r p q c <<< "$a"; ini+=('[Acceso]' 'Nombre=P' 'Icono=4' "Programa=$p" "Parametros=$q" "Carpeta=$c"); done
  printf '%s\r\n' "${ini[@]}" > $T/CENTRO.INI
  MTOOLS_SKIP_CHECK=1 mcopy -o -i $T/f.img $T/CENTRO.INI ::HISPADOS/CENTRO.INI
  rm -f $T/b.img
  mformat -C -f 1440 -v DX -i $T/b.img ::
}
rundx() {
  rm -f $T/mon $T/out.wav
  timeout ${TMAX:-200} qemu-system-i386 -display none -serial stdio -monitor unix:$T/mon,server,nowait \
    -parallel none -m 32 -drive file=$T/f.img,format=raw,if=floppy,index=0 \
    -drive file=$T/b.img,format=raw,if=floppy,index=1 -boot a -no-reboot \
    -vga std -audiodev wav,id=snd0,path=$T/out.wav -device AC97,audiodev=snd0 > $T/raw 2>&1
  tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 | grep -v '^audio:' > $T/log.txt
}
guarda() { [ -n "${GUARDAR:-}" ] && for f in "$@"; do cp $T/$f.ppm "$GUARDAR"/$1-$f.ppm 2>/dev/null; done; true; }
check() { if grep -qF -- "$1" $T/log.txt; then ok "$1"; else ko "$1"; fi; }

# ---- 1. el ejemplo de DirectDraw del flat assembler ----
F=$DL/win32-fasmw
if [ -f $F/FASM.EXE ] && [ -f $F/EXAMPLES/DDRAW/DDRAW.ASM ]; then
  echo "== DirectX de otros: el ejemplo DDRAW del flat assembler (ensamblado en Hispa-DOS) =="
  prepara 'B:\DDRAW\DDRAW.EXE||B:\DDRAW'
  mcopy -i $T/b.img $F/FASM.EXE ::
  mcopy -s -i $T/b.img $F/INCLUDE ::
  mmd -i $T/b.img ::DDRAW
  mcopy -i $T/b.img $F/EXAMPLES/DDRAW/DDRAW.ASM $F/EXAMPLES/DDRAW/DDRAW.INC $F/EXAMPLES/DDRAW/GIF87A.INC $F/EXAMPLES/DDRAW/DDRAW.GIF ::DDRAW/
  { sleep 6
    send 'A:\HISPADOS\HSPDPMI' 3
    send 'SET INCLUDE=B:\INCLUDE' 1
    send 'B:' 1; send 'CD \DDRAW' 1
    send 'B:\FASM DDRAW.ASM' 12
    send 'A:' 1
    send 'VENTANAS' 9
    doble 125 408
    hasta f1 40 rgb 40 300 000000 cuenta 80 300 640 240 202088 5000 200000
    sleep 3; shot f2
    tecla esc
    hasta f3 20 titulo 36 52 inactiva "Centro de programas"
    sleep 1; shot f4
    mon quit; } | rundx
  check "3 passes"
  if MTOOLS_SKIP_CHECK=1 mdir -i $T/b.img ::DDRAW/DDRAW.EXE > /dev/null 2>&1; then ok "FASM.EXE ha hecho DDRAW.EXE en Hispa-DOS"; else ko "no está DDRAW.EXE"; fi
  chk $T/f2.ppm rgb 40 300 000000 "pantalla completa: el borde, negro" \
    cuenta 80 300 640 240 202088 5000 200000 "el suelo de cuadros azules del GIF (640×480, tal cual)" \
    cuenta 80 300 640 240 282848 5000 200000 "y los cuadros oscuros"
  chk $T/f4.ppm titulo 36 52 inactiva "Centro de programas"
  guarda fasm f2 f4
else
  echo "== DirectX de otros: el ejemplo DDRAW del flat assembler: no está (tests/thirdparty/fetch_win32.sh), se salta =="
fi

# ---- 2. TinyPTC ----
P=$DL/dx-tinyptc/0.8/windows
if [ -f $P/fullscreen.exe ]; then
  echo "== DirectX de otros: TinyPTC (UPX, LoadLibraryA, 32 bits, pantalla completa y ventana) =="
  prepara 'B:\PTCFULL.EXE||B:\' 'B:\PTCVEN.EXE||B:\'
  mcopy -i $T/b.img $P/fullscreen.exe ::PTCFULL.EXE
  mcopy -i $T/b.img $P/windowed.exe ::PTCVEN.EXE
  { sleep 6
    send 'VENTANAS' 9
    doble 125 408
    hasta p1 40 rgb 40 300 000000 cuenta 80 100 640 400 FFFFFF 5000 200000
    sleep 2; shot p2
    tecla esc
    hasta p3 20 titulo 36 52 inactiva "Centro de programas"
    doble 225 408
    hasta p4 40 titulo 238 186 activa "test"
    sleep 4; shot p5
    mon quit; } | rundx
  chk $T/p2.ppm rgb 40 300 000000 "fullscreen.exe: el borde" \
    cuenta 80 100 640 400 FFFFFF 5000 200000 "el ruido de 320×200 en 32 bits (al doble): blanco" \
    cuenta 80 100 640 400 000000 5000 200000 "y negro" rgb 40 50 000000 "arriba, el borde (200 de alto: 100..500)"
  chk $T/p3.ppm titulo 36 52 inactiva "Centro de programas"
  chk $T/p5.ppm titulo 238 186 activa "test" \
    cuenta 242 212 316 196 202020 1000 30000 "windowed.exe: el ruido en la ventana (Blt a la primaria con su recortador): gris oscuro" \
    cuenta 242 212 316 196 A7A7A7 1000 30000 "y gris claro (los grises de Hispa Ventanas: StretchDIBits de 32 bits)"
  guarda ptc p2 p5
else
  echo "== DirectX de otros: TinyPTC: no está (tests/thirdparty/fetch_dx.sh), se salta =="
fi

# ---- 3. FCE Ultra con el juego de NES propio ----
E=$DL/dx-fceu
if [ -f $E/fceu.exe ] && make -s build/HISPA1.NES > /dev/null 2>&1; then
  echo "== DirectX de otros: FCE Ultra 0.98.8 (DirectDraw 7, DirectInput 7, DirectSound) con «Cometas» =="
  prepara 'B:\FCEU.EXE|HISPA1.NES|B:\'
  mcopy -i $T/b.img $E/fceu.exe ::FCEU.EXE
  mcopy -i $T/b.img $B/HISPA1.NES ::HISPA1.NES
  { sleep 6
    send 'A:\HISPADOS\HSPSND /VOL:100' 3
    send 'VENTANAS' 9
    doble 125 408
    hasta e1 60 titulo 48 52 activa "FCE Ultra" cuenta 52 98 254 222 30CF30 100 20000
    sleep 3; shot e2
    mon quit; } | TMAX=240 rundx
  chk $T/e2.ppm titulo 48 52 activa "FCE Ultra" \
    busca 50 76 310 96 0 15 "ile" busca 50 76 310 96 0 15 "onfig" \
    cuenta 52 98 254 222 30CF30 100 20000 "«Cometas» en su ventana (el texto verde de la portada)" \
    cuenta 52 98 254 222 FFFFFF 100 20000 "y las estrellas"
  guarda fceu e2
else
  echo "== DirectX de otros: FCE Ultra: no está (tests/thirdparty/fetch_dx.sh), se salta =="
fi
fin
