#!/bin/bash
# Prueba de la fase 3c de Hispa-486 (docs/486/HISPA-486.md), parte 1: un
# programa de ventanas hecho en C con MSVCRT.DLL dentro de Hispa Ventanas.
# HW32.COM es el módulo del programa: le da el KERNEL32 de los de consola y
# carga sus DLL. WINCRT.EXE (tests/progs/wincrt.c) prueba cada cosa y escribe
# una línea por cada una. Se mira en capturas:
#  - el montón y VirtualAlloc; los archivos y las búsquedas; la hora, el
#    entorno y TLS; GetModuleHandle, LoadLibrary y GetProcAddress (de
#    KERNEL32, de USER32 y de una DLL); MSVCRT (sprintf con coma, qsort,
#    malloc, archivos con búfer; lo que va a la consola no sale); la DLL de al
#    lado del programa (DLLPRUE.DLL: su función, su dato y su DllMain) y la de
#    la carpeta de HW32 (MSVCRT.DLL);
#  - Sleep deja el turno y vuelve; un clic deja un archivo abierto;
#  - al cerrar la ventana no queda memoria (ni la del programa, ni la de sus
#    DLL, ni la del montón), tampoco si acaba con exit() de MSVCRT dejando
#    3 MB sin soltar;
#  - WINSINDL.EXE importa de una DLL que no está: no se abre y dice cuál;
#  - sin HW32.COM, un programa de Windows no se abre y se dice por qué.
#  (GUARDAR=carpeta deja las capturas.)
# Win32 GUI program in C with MSVCRT.DLL inside Hispa Ventanas (phase 3c) test.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
B=build
make -s $B/WINCRT.EXE $B/WINSINDL.EXE $B/DLLPRUE.DLL $B/MSVCRT.DLL $B/HW32.COM > /dev/null 2>&1 || {
  echo "== Hispa-486: Win32 de ventanas con MSVCRT: sin gcc de 32 bits, se salta =="; exit 0; }
T=$(mktemp -d)
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; sleep "${2:-0.3}"; }
tecla() { local k; for k in "$@"; do mon "sendkey $k" 0.3; done; }
shot() { sleep 0.4; mon "screendump $T/$1.ppm" 0.6; }
send() { printf '%s\r' "$1"; sleep "${2:-1.5}"; }
RX=400; RY=300
ir() {
  local dx=$(( $1 - RX )) dy=$(( $2 - RY )) sx sy
  while [ $dx -ne 0 ] || [ $dy -ne 0 ]; do
    sx=$dx; [ $sx -gt 60 ] && sx=60; [ $sx -lt -60 ] && sx=-60
    sy=$dy; [ $sy -gt 60 ] && sy=60; [ $sy -lt -60 ] && sy=-60
    mon "mouse_move $sx $sy" 0.07
    dx=$(( dx - sx )); dy=$(( dy - sy ))
  done
  RX=$1; RY=$2; sleep 0.3
}
clic() { ir $1 $2; mon "mouse_button 1" 0.25; mon "mouse_button 0" 0.5; }
chk() { python3 -I tests/tools/ventanas_check.py "$@" || fail=1; }
run() {
  rm -f $T/mon
  timeout ${TMAX:-170} qemu-system-i386 ${QEMU_PANTALLA--vga std -global VGA.xres=800 -global VGA.yres=600} -display none -serial stdio -monitor unix:$T/mon,server,nowait \
    -parallel none -m 16 -drive file=$T/f.img,format=raw,if=floppy,index=0 -boot a -no-reboot > $T/raw 2>&1
  tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 > $T/log.txt
}

echo "== Hispa-486: un Win32 de ventanas en C con MSVCRT.DLL dentro de Hispa Ventanas =="
cp "$IMG" $T/f.img
printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nFILES=30\r\n' > $T/CONFIG.SYS
MTOOLS_SKIP_CHECK=1 mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
MTOOLS_SKIP_CHECK=1 mdel -i $T/f.img ::HISPADOS/HISPA.WAV 2>/dev/null   # (sitio: aquí no suena)
for f in WINCRT.EXE WINSINDL.EXE DLLPRUE.DLL; do
  MTOOLS_SKIP_CHECK=1 mcopy -o -i $T/f.img $B/$f ::BIN/$f || ko "no cabe $f en el disco"
done
cp $T/f.img $T/sinhw.img
for f in HW32.COM MSVCRT.DLL; do
  MTOOLS_SKIP_CHECK=1 mcopy -o -i $T/f.img $B/$f ::HISPADOS/$f || ko "no cabe $f en el disco"
done
# un grupo «Win32», abierto, con los accesos de la prueba (tres iconos en
# fila: sus nombres, en x = 125, 229 y 333 de la pantalla, y = 371)
printf '%s\r\n' '[Centro]' 'Ventana=36,52,452,190' 'Activo=0' 'SinRepetidos=1' 'TareasPrincipal=1' \
  '[Grupo]' 'Nombre=Win32' 'Ventana=60,330,600,200' 'Estado=1' \
  '[Acceso]' 'Nombre=Crt' 'Icono=4' 'Programa=A:\BIN\WINCRT.EXE' \
  '[Acceso]' 'Nombre=SinDll' 'Icono=4' 'Programa=A:\BIN\WINSINDL.EXE' \
  '[Acceso]' 'Nombre=Tareas' 'Icono=2' 'Programa=A:\HISPADOS\PROGS\TAREAS.HVP' \
  | iconv -f UTF-8 -t CP437 > $T/CENTRO.INI
MTOOLS_SKIP_CHECK=1 mcopy -o -i $T/f.img $T/CENTRO.INI ::HISPADOS/CENTRO.INI
MTOOLS_SKIP_CHECK=1 mcopy -o -i $T/sinhw.img $T/CENTRO.INI ::HISPADOS/CENTRO.INI

aparca() { ir 16 590; }
MEMZ="180 418 450 54"      # la zona de la memoria en la ventana del Administrador de tareas
N=16; B_=231               # negro y blanco del cubo de colores (el texto y el fondo de la ventana)
Z="45 68 560 300"          # donde escribe WINCRT (su ventana, en 40,40; su zona, en 42,65)

{ sleep 6; send 'VENTANAS' 9
  tecla right right ret; sleep 5; aparca; shot m0; tecla alt-f4; sleep 1.5   # la memoria antes de nada
  tecla left left ret; sleep 6; aparca; shot r1                            # WINCRT
  clic 300 200; aparca; shot r2                                              # un clic: deja un archivo abierto
  tecla s; sleep 3; shot r3                                                  # Sleep(1500)
  clic 548 51; sleep 2; aparca; shot x1                                      # la X
  tecla right right ret; sleep 5; aparca; shot m1; tecla alt-f4; sleep 1.5   # la memoria, después
  tecla left ret; sleep 4; aparca; shot e1; tecla ret; sleep 1.5             # la DLL que falta
  tecla left ret; sleep 6; tecla n; sleep 1; tecla x; sleep 2.5; shot x2     # otra vez: 3 MB sin soltar y exit()
  tecla right right ret; sleep 5; aparca; shot m2
  mon quit; } | TMAX=200 run
chk $T/m0.ppm titulo 170 80 activa "Administrador de tareas" busca 176 418 640 436 0 8 "Libre:" \
  busca 176 480 640 498 0 6 "4 programas en marcha"
echo "-- WINCRT.EXE: KERNEL32 (el de HW32), las DLL y MSVCRT --"
chk $T/r1.ppm titulo 40 40 activa "Win32 con MSVCRT" busca $Z $N $B_ "Memoria: bien" \
  busca $Z $N $B_ "Archivos: bien (486 escribe)" busca $Z $N $B_ "Hora, entorno y TLS: bien" \
  busca $Z $N $B_ "GetProcAddress: bien" busca $Z $N $B_ "MSVCRT: 123459  6.28 x" \
  busca $Z $N $B_ "MSVCRT, archivos: bien" busca $Z $N $B_ "DLLPRUE: Doble(20) = 41, contador = 42" \
  busca $Z $N $B_ "A:\\BIN\\WINCRT.EXE|A:\\BIN\\WINCRT.EXE" busca $Z $N $B_ "Clics: 0" \
  nobusca 0 0 790 590 $N $B_ "MAL"
chk $T/r2.ppm busca $Z $N $B_ "Clics: 1"
chk $T/r3.ppm busca $Z $N $B_ "Clics: 101"
echo "-- al cerrar no queda memoria; la DLL que falta --"
chk $T/x1.ppm titulo 60 330 activa "Win32" nobusca 0 30 790 320 $N $B_ "Clics: 101"
chk $T/m1.ppm titulo 170 80 activa "Administrador de tareas" busca 176 480 640 498 0 6 "4 programas en marcha" \
  igual $MEMZ $T/m0.ppm ${MEMZ% * *} 0
chk $T/e1.ppm busca 150 250 650 330 0 8 "Hispa-486: falta la DLL NOESTA.DLL" nobusca 0 30 790 320 $N $B_ "Memoria: bien"
chk $T/x2.ppm titulo 60 330 activa "Win32" nobusca 0 30 790 320 $N $B_ "Memoria: bien"
chk $T/m2.ppm titulo 170 80 activa "Administrador de tareas" busca 176 480 640 498 0 6 "4 programas en marcha" \
  igual $MEMZ $T/m0.ppm ${MEMZ% * *} 0
[ -n "${GUARDAR:-}" ] && cp $T/*.ppm "$GUARDAR"/ 2>/dev/null
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/VENTANAS/,$p' $T/log.txt | tail -20; fi

echo "-- sin HW32.COM --"
f1=$fail
cp $T/sinhw.img $T/f.img
RX=400; RY=300
{ sleep 6; send 'VENTANAS' 9
  tecla ret; sleep 4; aparca; shot s1; tecla ret; sleep 1.5; shot s2
  mon quit; } | TMAX=60 run
chk $T/s1.ppm busca 100 250 700 330 0 8 "Falta HW32.COM: hace falta para los programas de Windows."
chk $T/s2.ppm titulo 60 330 activa "Win32" nobusca 100 250 700 330 0 8 "Falta HW32.COM: hace falta para los programas de Windows."
[ -n "${GUARDAR:-}" ] && { for f in $T/s?.ppm; do cp $f "$GUARDAR"/; done; } 2>/dev/null
if [ $fail -ne $f1 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/VENTANAS/,$p' $T/log.txt | tail -20; fi
rm -rf $T
exit $fail
