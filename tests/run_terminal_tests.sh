#!/bin/bash
# Prueba de Terminal (TERMINAL.HVP), el símbolo del sistema de Hispa Ventanas.
#
# Abre Hispa Ventanas, entra en el grupo «Accesorios» y abre el Terminal (el
# sexto icono: Bloc, Calculadora, Reloj, Sistema, Pintura,
# Terminal). Comprueba que:
#  - la ventana «Terminal» se abre (título en 100,60);
#  - VER escribe la versión del sistema en el área de texto;
#  - ECHO escribe su argumento;
#  - al salir, Hispa-DOS sigue vivo.
#
# Necesita qemu-system-i386, mtools, socat e ImageMagick (como las demás).
set -u
IMG=${1:-build/hispados.img}
B=build
T=$(mktemp -d)
CHK="python3 tests/tools/ventanas_check.py"
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() { if grep -qF -- "$1" $T/log.txt; then ok "$1"; else ko "$1"; fi; }
chk() { $CHK "$@" || fail=1; }
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; sleep "${2:-0.3}"; }
tecla() { local k; for k in "$@"; do mon "sendkey $k" 0.3; done; }
shot() { sleep 0.4; mon "screendump $T/$1.ppm" 0.6; }
send() { printf '%s\r' "$1"; sleep "${2:-1.5}"; }
muestra() { [ -f $T/$1.ppm ] && convert $T/$1.ppm $B/terminal_$2.png; }
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
doble() { ir $1 $2; mon "mouse_button 1" 0.1; mon "mouse_button 0" 0.1; mon "mouse_button 1" 0.1; mon "mouse_button 0" 0.8; }
aparca() { ir 16 320; }
# teclea un texto: letras, cifras, espacio y punto
escribe() {
  local t="$1" i c
  for ((i=0; i<${#t}; i++)); do
    c="${t:$i:1}"
    case "$c" in
      ' ') tecla spc;;
      '.') tecla dot;;
      [A-Z]) tecla "shift-$(echo $c | tr A-Z a-z)";;
      *) tecla "$c";;
    esac
  done
}
run() {
  local tmo=$1; shift
  rm -f $T/mon
  timeout $tmo qemu-system-i386 -display none -serial stdio -monitor unix:$T/mon,server,nowait \
    -parallel none -m 16 -drive file=$T/f.img,format=raw,if=floppy,index=0 -boot a -no-reboot "$@" > $T/raw 2>&1
  tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 | grep -v '^audio:' > $T/log.txt
  mtype -i $T/f.img ::HISPADOS/CENTRO.INI 2>/dev/null | tr -d '\r' | iconv -f CP437 -t UTF-8 > $T/ini.txt
}

echo "== Terminal: abrir el símbolo del sistema y ejecutar VER y ECHO =="
cp "$IMG" $T/f.img
printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nFILES=30\r\n' > $T/CONFIG.SYS
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS

if MTOOLS_SKIP_CHECK=1 mdir -i $T/f.img ::HISPADOS/PROGS/TERMINAL.HVP > /dev/null 2>&1; then
  ok "TERMINAL.HVP está en \\HISPADOS\\PROGS"
else
  ko "falta TERMINAL.HVP en \\HISPADOS\\PROGS"
fi

{ sleep 6
  send 'KEYB SP' 2
  send 'A:\HISPADOS\HSPRATON' 2
  send 'VENTANAS' 5
  doble 206 129; sleep 1                # abrir el grupo «Accesorios»
  aparca
  # el sexto icono (Terminal): Bloc, Calculadora, Reloj, Sistema, Pintura,
  # Terminal (Cuentas esta en Productividad) -> cinco «right» desde el primero
  tecla right right right right right ret; sleep 2.5
  aparca
  shot t1
  escribe "ver"; tecla ret; sleep 1    # la versión del sistema
  aparca
  shot t2
  escribe "echo hola terminal"; tecla ret; sleep 1
  aparca
  shot t3
  clic 756 13; sleep 1; tecla ret; sleep 2.5    # salir de Hispa Ventanas
  send 'ECHO TRAS-SALIR'
  send 'ECHO SIGUE-VIVO'
  mon quit
} | run 200 -vga std

# -- la ventana del Terminal, abierta en (100,60) --
chk $T/t1.ppm titulo 100 60 activa "Terminal"
# -- VER: la versión, escrita en el área de texto (claro sobre negro) --
chk $T/t2.ppm busca 106 105 656 414 15 0 "Hispa-DOS 0.9.19"
# -- ECHO: su argumento --
chk $T/t3.ppm busca 106 105 656 414 15 0 "hola terminal"
# -- Hispa-DOS sigue vivo tras salir --
check "Hispa Ventanas ha terminado."
check "TRAS-SALIR"
check "SIGUE-VIVO"
# -- al salir, el CENTRO.INI cuenta 15 módulos y el Terminal en Accesorios --
if grep -qxF -- "Modulos=17" $T/ini.txt; then ok "CENTRO.INI: Modulos=17"; else ko "CENTRO.INI sin Modulos=17"; fi
if grep -qF -- "Nombre=Terminal" $T/ini.txt; then ok "CENTRO.INI: Nombre=Terminal"; else ko "CENTRO.INI sin Nombre=Terminal"; fi

muestra t1 abierto; muestra t2 ver; muestra t3 echo
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/Ejecutando/,$p' $T/log.txt; cat $T/ini.txt; fi
rm -rf $T
exit $fail
