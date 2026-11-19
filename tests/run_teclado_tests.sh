#!/bin/bash
# Prueba de Teclado (TECLADO.HVP), la herramienta de taller del teclado.
#
# Abre Hispa Ventanas, entra en el grupo «Accesorios» y abre el Teclado (el
# septimo icono: Bloc, Calculadora, Reloj, Sistema, Pintura,
# Terminal, Teclado). Comprueba que:
#  - la ventana «Teclado» se abre (título en 40,36);
#  - el teclado dibujado está pintado (la tecla «A», apagada, en color crema);
#  - al inyectar un código de rastreo con el monitor de QEMU (sendkey), el
#    accesorio lo recibe EN CRUDO (WM_TECLA_CRUDA) y lo enseña: tras pulsar la
#    tecla «a» (make 1E) el ORIGEN del remapeo pasa a 1E; tras una flecha
#    (extendida, E0), la ÚLTIMA tecla lleva el prefijo E0.
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
chk() { $CHK "$@" || fail=1; }
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; sleep "${2:-0.3}"; }
tecla() { local k; for k in "$@"; do mon "sendkey $k" 0.3; done; }
shot() { sleep 0.4; mon "screendump $T/$1.ppm" 0.6; }
send() { printf '%s\r' "$1"; sleep "${2:-1.5}"; }
muestra() { [ -f $T/$1.ppm ] && convert $T/$1.ppm $B/teclado_$2.png; }
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
run() {
  local tmo=$1; shift
  rm -f $T/mon
  timeout $tmo qemu-system-i386 ${QEMU_PANTALLA--vga std -global VGA.xres=800 -global VGA.yres=600} -display none -serial stdio -monitor unix:$T/mon,server,nowait \
    -parallel none -m 16 -drive file=$T/f.img,format=raw,if=floppy,index=0 -boot a -no-reboot "$@" > $T/raw 2>&1
  tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 | grep -v '^audio:' > $T/log.txt
  mtype -i $T/f.img ::HISPADOS/CENTRO.INI 2>/dev/null | tr -d '\r' | iconv -f CP437 -t UTF-8 > $T/ini.txt
}

echo "== Teclado: abrir la herramienta y recibir el código crudo de teclado =="
cp "$IMG" $T/f.img
printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nFILES=30\r\n' > $T/CONFIG.SYS
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS

if mdir -i "$IMG" ::HISPADOS/PROGS/TECLADO.HVP > /dev/null 2>&1; then
  ok "TECLADO.HVP está en \\HISPADOS\\PROGS"
else
  ko "falta TECLADO.HVP en \\HISPADOS\\PROGS"
fi

{ sleep 6
  send 'KEYB SP' 2
  send 'A:\HISPADOS\HSPRATON' 2
  send 'VENTANAS' 5
  doble 206 129; sleep 1                # abrir el grupo «Accesorios»
  aparca
  # el septimo icono (Teclado): seis «right» desde el primero (Bloc de notas)
  tecla right right right right right right ret; sleep 2.5
  aparca
  shot k1
  tecla a; sleep 0.6                    # la tecla «a»: make 1E
  aparca
  shot k2
  tecla up; sleep 0.6                   # una flecha: tecla extendida (E0)
  aparca
  shot k3
  clic 756 13; sleep 1; tecla ret; sleep 2.5    # salir de Hispa Ventanas
  send 'ECHO SIGUE-VIVO'
  mon quit
} | run 200 -vga std

# -- la ventana del Teclado, abierta en (40,36), con el teclado dibujado --
chk $T/k1.ppm titulo 40 36 activa "Teclado"
# -- una tecla dibujada, apagada (color crema, C_CREMA=8); la cara de la tecla
#    «A» (fila de casa) cae en pantalla sobre x108..136, y194..212; (130,196)
#    es cara crema, esquivando el glifo oscuro de la letra, centrado --
chk $T/k1.ppm px 130 196 8 "la tecla A dibujada, apagada (crema)"
# -- tras pulsar «a» (make 1E), el ORIGEN del remapeo pasa a 1E --
chk $T/k2.ppm busca 44 248 440 276 0 6 "1E"
# -- tras una flecha (extendida), el ORIGEN lleva el prefijo E0 (texto fino) --
chk $T/k3.ppm busca 44 248 440 276 0 6 "E0"
# -- Hispa-DOS sigue vivo tras salir --
if grep -qF -- "SIGUE-VIVO" $T/log.txt; then ok "Hispa-DOS sigue vivo tras salir"; else ko "no vuelve a Hispa-DOS"; fi

muestra k1 abierto; muestra k2 tecla_a; muestra k3 flecha

if [ $fail -eq 0 ]; then echo "== Teclado: TODO BIEN =="; else echo "== Teclado: HAY FALLOS =="; fi
exit $fail
