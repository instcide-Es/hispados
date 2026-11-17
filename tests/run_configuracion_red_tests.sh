#!/bin/bash
# Prueba del apartado «Red» de Configuración (CONFIG.HVP) y de \HISPADOS\RED.INI
# (docs/RED.md). Arranca Hispa Ventanas en QEMU, abre Configuración, va al
# apartado «Red» y maneja la casilla de DHCP y los cuadros de las direcciones
# con el ratón y el teclado del monitor de QEMU; RED.INI se mira en la imagen
# con mtools. Está hecha sobre tests/run_configuracion_tests.sh (los otros
# apartados y VENTANAS.INI).
#  1.ª sesión (RED.INI de fábrica, DHCP=SI): se quita el DHCP, se ponen una IP,
#    una máscara, una puerta y un DNS a mano y se guarda; RED.INI queda con
#    DHCP=NO y esas direcciones.
#  2.ª sesión (RED.INI a mano, DHCP=NO con direcciones y una clave ACTUALIZAR=):
#    el apartado sale en modo manual; se vuelve a DHCP y se guarda; RED.INI
#    queda con DHCP=SI, sin direcciones y con ACTUALIZAR= conservada.
#  3.ª sesión (con HSPRED cargado y la IP pedida por DHCP a QEMU): el estado
#    del apartado enseña el controlador cargado y la dirección IP en marcha.
# Deja capturas en build/config_red_*.png.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
B=build
T=$(mktemp -d)
CHK="python3 tests/tools/ventanas_check.py"
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
chk() { $CHK "$@" || fail=1; }
# una línea exacta de RED.INI (en \HISPADOS de la imagen de la prueba)
red() { if grep -qxF -- "$1" $T/red.txt; then ok "RED.INI: $1"; else ko "RED.INI sin la línea «$1»"; fi; }
redn() { if grep -q -- "^$1" $T/red.txt; then ko "RED.INI: sobra «$1»"; else ok "RED.INI: sin «$1»"; fi; }
check() { if grep -qF -- "$1" $T/log.txt; then ok "$1"; else ko "$1"; fi; }
checkn() { if grep -qF -- "$1" $T/log.txt; then ko "(no debía salir) $1"; else ok "sin \"$1\""; fi; }
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; sleep "${2:-0.3}"; }
tecla() { local k; for k in "$@"; do mon "sendkey $k" 0.3; done; }
shot() { sleep 0.4; mon "screendump $T/$1.ppm" 0.6; }
send() { printf '%s\r' "$1"; sleep "${2:-1.5}"; }
muestra() { [ -f $T/$1.ppm ] && convert $T/$1.ppm $B/config_red_$2.png; }
# el ratón (igual que en run_configuracion_tests.sh): RX,RY = el puntero
RX=400; RY=300; VEL=2
ir() {
  local dx=$(( ($1 - RX) * 2 / VEL )) dy=$(( ($2 - RY) * 2 / VEL )) sx sy
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
# escribe una dirección en el cuadro de edición de (X,Y): clic, Ctrl+A y las
# cifras con los puntos (p. ej. escribe_ip X Y 192 168 1 58)
escribe_ip() {
  local x=$1 y=$2; shift 2
  clic $x $y
  tecla ctrl-a
  local primero=1 o c
  for o in "$@"; do
    [ $primero = 1 ] || tecla dot
    primero=0
    for (( i=0; i<${#o}; i++ )); do tecla "${o:$i:1}"; done
  done
}
# La ventana de Configuración en 800x600: marco (124,110), zona en (126,135).
# Los apartados: x=196, y=162+34*n; «Red» es el 7.º (n=6): y=366.
# El apartado Red (coordenadas de pantalla): casilla DHCP en ~(286,267); los
# cuadros IP/Máscara/Puerta/DNS en x~470, y 298/332/366/400; «Guardar» en
# (527,491).
run() {
  local tmo=$1; shift
  rm -f $T/mon
  timeout $tmo qemu-system-i386 -display none -serial stdio -monitor unix:$T/mon,server,nowait \
    -parallel none -m 32 -drive file=$T/f.img,format=raw,if=floppy,index=0 -boot a "$@" > $T/raw 2>&1
  tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 | grep -v '^audio:' > $T/log.txt
  mtype -i $T/f.img ::HISPADOS/RED.INI 2>/dev/null | tr -d '\r' | iconv -f CP437 -t UTF-8 > $T/red.txt
}
# abre Configuración (grupo Principal -> Configuración) y va al apartado Red
abrir_red() {
  send 'A:\HISPADOS\HSPRATON' 2
  send 'VENTANAS' 6
  doble 102 129; sleep 1          # abre el grupo Principal
  aparca
  tecla right ret; sleep 2   # Configuración (la segunda de «Principal»)
  aparca
  clic 196 366; sleep 1           # el apartado «Red»
  aparca
}
printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nFILES=30\r\n' > $T/CONFIG.SYS

echo "== Red: de DHCP a IP estática a mano, guardar y comprobar RED.INI =="
cp "$IMG" $T/f.img
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
{ sleep 6
  abrir_red
  shot a0                          # el apartado con DHCP=SI (sin cuadros)
  clic 286 267; sleep 1            # quita el DHCP: aparecen los cuadros
  aparca
  shot a1
  escribe_ip 470 298 192 168 1 58
  escribe_ip 470 332 255 255 255 0
  escribe_ip 470 366 192 168 1 1
  escribe_ip 470 400 8 8 8 8
  aparca
  shot a2
  clic 527 491; sleep 1            # «Guardar»
  aparca
  shot a3
  clic 756 13; sleep 1; tecla ret; sleep 2.5   # cerrar la ventana
  send 'ECHO SIGUE-VIVO'
  mon quit
} | run 150 -vga std -no-reboot

chk $T/a0.ppm marco 124 110 552 403 activa titulo 124 110 activa "Configuración" \
  texto 278 145 4 8 negrita "Red" texto 164 355 8 12 negrita "Red"
chk $T/a1.ppm texto 278 291 0 8 fina "Dirección IP:" texto 278 325 0 8 fina "Máscara de subred:" \
  texto 278 359 0 8 fina "Puerta de enlace:" texto 278 393 0 8 fina "Servidor DNS:"
check "Hispa Ventanas ha terminado."
check "SIGUE-VIVO"
checkn "HSPDPMI: excepción"
red "DHCP=NO"; red "IP=192.168.1.58"; red "MASCARA=255.255.255.0"; red "PUERTA=192.168.1.1"; red "DNS=8.8.8.8"
muestra a0 manual_no; muestra a1 manual_si; muestra a2 escrito; muestra a3 guardado
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/Ejecutando/,$p' $T/log.txt; cat $T/red.txt; fi
[ -n "${GUARDAR:-}" ] && cp $T/*.ppm "$GUARDAR"/

echo "== Red: RED.INI a mano (DHCP=NO), volver a DHCP y conservar ACTUALIZAR= =="
cp "$IMG" $T/f.img
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
printf '%s\r\n' '; a mano' 'DHCP=NO' 'IP=10.0.0.5' 'MASCARA=255.0.0.0' 'PUERTA=10.0.0.1' \
  'DNS=10.0.0.1' 'ACTUALIZAR=http://ejemplo/ultima.txt' > $T/RED.INI
mcopy -o -i $T/f.img $T/RED.INI ::HISPADOS/RED.INI
RX=400; RY=300
{ sleep 6
  abrir_red
  shot b0                          # sale en modo manual, con las direcciones
  clic 286 267; sleep 1            # marca el DHCP: se ocultan los cuadros
  aparca
  shot b1
  clic 527 491; sleep 1            # «Guardar»
  aparca
  shot b2
  clic 756 13; sleep 1; tecla ret; sleep 2.5
  send 'ECHO SIGUE-VIVO'
  mon quit
} | run 150 -vga std -no-reboot

chk $T/b0.ppm titulo 124 110 activa "Configuración" texto 278 145 4 8 negrita "Red" \
  texto 278 291 0 8 fina "Dirección IP:"
# con DHCP=SI los cuadros a mano desaparecen
chk $T/b1.ppm texto 278 145 4 8 negrita "Red" nobusca 270 280 460 410 0 8 "Dirección IP:"
check "Hispa Ventanas ha terminado."
check "SIGUE-VIVO"
checkn "HSPDPMI: excepción"
red "DHCP=SI"; red "ACTUALIZAR=http://ejemplo/ultima.txt"
redn "IP="; redn "MASCARA="; redn "PUERTA="; redn "DNS="
muestra b0 vuelta_manual; muestra b1 vuelta_dhcp
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/Ejecutando/,$p' $T/log.txt; cat $T/red.txt; fi
[ -n "${GUARDAR:-}" ] && cp $T/*.ppm "$GUARDAR"/

echo "== Red: con HSPRED cargado, el estado enseña la MAC y la IP en marcha =="
cp "$IMG" $T/f.img
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
RX=400; RY=300
{ sleep 6
  send 'A:\HISPADOS\HSPRED' 3      # la tarjeta rtl8139 de QEMU
  send 'A:\HISPADOS\CONFRED' 4     # pide la IP por DHCP (10.0.2.15)
  abrir_red
  shot c0
  clic 756 13; sleep 1; tecla ret; sleep 2.5
  send 'ECHO SIGUE-VIVO'
  mon quit
} | run 150 -vga std -no-reboot -netdev user,id=n0 -device rtl8139,netdev=n0

chk $T/c0.ppm texto 278 145 4 8 negrita "Red" \
  texto 278 195 0 8 fina "Controlador de red cargado (HSPRED)." \
  texto 278 213 0 8 fina "Dirección MAC: 52:54:00:12:34:56" \
  texto 278 231 0 8 fina "Dirección IP actual: 10.0.2.15"
check "Hispa Ventanas ha terminado."
check "SIGUE-VIVO"
muestra c0 estado_hspred
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/Ejecutando/,$p' $T/log.txt; fi
[ -n "${GUARDAR:-}" ] && cp $T/*.ppm "$GUARDAR"/

rm -rf $T
exit $fail
