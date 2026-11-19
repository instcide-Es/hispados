#!/bin/bash
# Prueba de [PANTALLA] la resolución de Hispa Ventanas y [ICONOS] el tamaño
# de los iconos (docs/VENTANAS.md, «La pantalla» y «El tamaño de los
# iconos»). QEMU con -vga std da muchos modos VESA (también 1280x800,
# 1440x900, 1920x1080...) y un EDID con la resolución de la pantalla que se
# le diga (-global VGA.xres/yres): aquí, la de un MacBook Air, 1440x900.
#  1.ª sesión (disquete, pantalla de 1440x900):
#  - sin opciones ni VENTANAS.INI, la automática es la de la pantalla;
#  - Configuración › Resolución: la lista de la tarjeta, «Automática
#    (ahora)» y la nota; «Probar» 1024x768 y 256 colores vuelve a cargar Hispa
#    Ventanas en ese modo y pregunta «¿Se ve bien?»; sin contestar, a los 15
#    segundos vuelve a 1440x900 con Configuración abierta en Resolución, y
#    VENTANAS.INI sigue sin «Modo=»;
#  - «Probar» otra vez y «Sí»: VENTANAS.INI con «Modo=1024x768x8»; al
#    salir y volver a entrar, 1024x768; con /800 manda la línea de órdenes;
#    /MODO:AUTO, la de la pantalla.
#  2.ª sesión (VENTANAS.INI a mano con un modo que este equipo no tiene y los
#    iconos grandes): la automática, sin romper; los iconos de un grupo,
#    grandes; Configuración › Resolución › «Pequeños», al momento, y se
#    guarda.
#  3.ª: sin EDID (VGA.edid=off), 800x600 como siempre; con HSPEMM (modo V86,
#    «Juegos»), 1440x900 en color verdadero.
#  4.ª (si está build/hispa486-usb.img): el escritorio de Hispa-486 a
#    1440x900 con el icono «Biblioteca» de tamaño normal y el nombre con la
#    letra normal, sin etiqueta; con los iconos grandes, también el menú
#    Inicio.
# Deja capturas en build/resol_*.png (GUARDAR=carpeta las copia también).
# Screen resolution and icon size test.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
USB486=${USB486:-build/hispa486-usb.img}
B=build
T=$(mktemp -d)
CHK="python3 tests/tools/ventanas_check.py"
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
chk() { $CHK "$@" || fail=1; }
ini() { if grep -qxF -- "$1" $T/ini.txt; then ok "VENTANAS.INI: $1"; else ko "VENTANAS.INI sin la línea «$1»"; fi; }
inin() { if grep -qi -- "^$1" $T/ini.txt; then ko "VENTANAS.INI: sobra «$1»"; else ok "VENTANAS.INI: sin «$1»"; fi; }
tam() { chk $T/$1.ppm tam $2 $3; }
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; sleep "${2:-0.3}"; }
tecla() { local k; for k in "$@"; do mon "sendkey $k" 0.3; done; }
shot() { sleep 0.4; mon "screendump $T/$1.ppm" 0.6; }
send() { printf '%s\r' "$1"; sleep "${2:-1.5}"; }
muestra() { [ -f $T/$1.ppm ] && convert $T/$1.ppm $B/resol_$2.png && { [ -z "${GUARDAR:-}" ] || cp $B/resol_$2.png "$GUARDAR/"; }; }
# el ratón: al entrar Hispa Ventanas lo pone en el centro (centro ANCHO ALTO)
RX=720; RY=450
centro() { RX=$(( $1 / 2 )); RY=$(( $2 / 2 )); }
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
PANT="-vga std -global VGA.xres=1440 -global VGA.yres=900"
run() {
  local tmo=$1; shift
  rm -f $T/mon
  timeout $tmo qemu-system-i386 -display none -serial stdio -monitor unix:$T/mon,server,nowait \
    -parallel none -m 32 "$@" > $T/raw 2>&1
  tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 > $T/log.txt
}
inid() { mtype -i $T/f.img ::HISPADOS/VENTANAS.INI 2>/dev/null | tr -d '\r' | iconv -f CP437 -t UTF-8 > $T/ini.txt; }
FLOP="-drive file=$T/f.img,format=raw,if=floppy,index=0 -boot a"
printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nFILES=30\r\n' > $T/CONFIG.SYS
# Configuración, centrada en 1440x900: su zona empieza en (447,286); el
# apartado «Resolución» (el 9.º), en (520,580); la lista, en (599,346) con
# filas de 16 (Automática, 640x480, 800x600, 1024x768...); «256» en
# (869,356) y «Probar...» en (921,451). La pregunta, centrada en 1024x768:
# «Sí» en (415,432).
abrir_res() {
  doble 102 129; sleep 1; aparca
  tecla home right ret; sleep 2
  clic 520 580; aparca
}

echo "== Resolución: la automática, «Probar», volver sola y «Sí» =="
cp "$IMG" $T/f.img
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
{ sleep 6
  send 'A:\HISPADOS\HSPRATON' 2
  send 'VENTANAS' 7
  shot a0
  abrir_res
  shot a1
  clic 650 402; clic 869 356
  clic 921 451; sleep 6
  centro 1024 768
  ir 16 300
  shot a2                                   # 1024x768 y la pregunta
  sleep 14
  centro 1440 900
  shot a3                                   # de vuelta, con Configuración
  mon "screendump $T/a3b.ppm" 0.3
  clic 650 402; clic 869 356
  clic 921 451; sleep 6
  centro 1024 768
  clic 415 432; sleep 2                     # «Sí, dejarla así»
  ir 16 300
  shot a4
  clic 980 13; sleep 1; tecla ret; sleep 3  # Salir › Volver a Hispa-DOS
  send 'VENTANAS' 7
  shot a5                                   # con «Modo=» de VENTANAS.INI
  centro 1024 768
  clic 980 13; sleep 1; tecla ret; sleep 3
  send 'VENTANAS /800' 7
  shot a6                                   # manda la línea de órdenes
  centro 800 600
  clic 756 13; sleep 1; tecla ret; sleep 3
  send 'VENTANAS /MODO:AUTO' 7
  shot a7
  centro 1440 900
  clic 1396 13; sleep 1; tecla ret; sleep 3
  send 'ECHO TRAS-SALIR' 2
  mon quit
} | run 240 $PANT $FLOP
inid
echo "-- la automática: la de la pantalla (EDID) --"
tam a0 1440 900
chk $T/a0.ppm px 1300 10 1 "la barra de arriba, de lado a lado" px 1430 450 2 "el fondo, hasta el borde"
echo "-- Configuración › Resolución --"
chk $T/a1.ppm busca 590 320 990 340 0 8 "Resolución de la pantalla:" \
  busca 597 362 840 378 0 9 "640 x 480" busca 597 394 840 410 0 9 "1024 x 768" \
  busca 590 466 990 486 0 8 "Ahora: 1440 x 900" \
  busca 590 484 990 504 0 8 "La pantalla dice que la suya es 1440 x 900." \
  busca 590 526 990 546 0 8 "Tamaño de los iconos y de su texto:"
muestra a1 apartado
echo "-- «Probar» 1024x768: otro modo y la pregunta --"
tam a2 1024 768
chk $T/a2.ppm busca 300 300 720 480 0 8 "Si no contesta, se vuelve a la de antes" \
  busca 300 300 720 480 0 6 "Sí, dejarla así" busca 300 300 720 480 0 6 "No, volver atrás"
muestra a2 pregunta
echo "-- sin contestar: a los 15 segundos, la de antes, con Configuración en Resolución --"
tam a3 1440 900
chk $T/a3.ppm busca 590 466 990 486 0 8 "Ahora: 1440 x 900" busca 590 320 990 340 0 8 "Resolución de la pantalla:"
echo "-- «Sí»: VENTANAS.INI con «Modo=» --"
tam a4 1024 768
chk $T/a4.ppm busca 0 0 1024 768 0 8 "Ahora: 1024 x 768, 256 colores (VENTANAS.INI)."
ini "Modo=1024x768x8"
ini "[Pantalla]"
tam a5 1024 768
tam a6 800 600
tam a7 1440 900
if grep -q "TRAS-SALIR" $T/log.txt; then ok "se sale a Hispa-DOS"; else ko "no ha vuelto a Hispa-DOS"; fi

echo "== Un modo que este equipo no tiene, y los iconos grandes y pequeños =="
cp "$IMG" $T/f.img
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
printf '[Pantalla]\r\nModo=1366x768x16\r\nIconos=grandes\r\n' > $T/V.INI
mcopy -o -i $T/f.img $T/V.INI ::HISPADOS/VENTANAS.INI
{ sleep 6
  send 'A:\HISPADOS\HSPRATON' 2
  send 'VENTANAS' 7
  centro 1440 900
  doble 102 129; sleep 1; aparca
  tecla home right right; sleep 1
  shot b1                                   # el grupo Principal, con los grandes
  tecla home right ret; sleep 2
  clic 520 580; aparca
  clic 607 560; sleep 1; aparca             # «Pequeños»
  shot b2
  sleep 4
  clic 1396 13; sleep 1; tecla ret; sleep 3
  send 'ECHO TRAS-SALIR' 2
  mon quit
} | run 120 $PANT $FLOP
inid
tam b1 1440 900
chk $T/b1.ppm busca 200 180 660 300 0 8 "Hispa-DOS" busca 200 180 660 300 9 12 "de tareas"
# (los grandes: el dibujo de «Hispa-DOS», de 48x48: la pantalla negra, de
# 260 a 290 y de 205 a 220; con los normales, de 32x32, no llegaría)
chk $T/b1.ppm px 260 205 0 "el dibujo grande de «Hispa-DOS»" px 292 232 0 "(abajo a la derecha: con uno de 32x32 no llegaría)"
muestra b1 iconos_grandes
chk $T/b2.ppm busca 590 466 990 486 0 8 "Ahora: 1440 x 900"
muestra b2 iconos_pequenos
ini "Iconos=pequeños"
ini "Modo=1366x768x16"

echo "== Sin EDID (800x600) y con HSPEMM (modo V86) =="
cp "$IMG" $T/f.img
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
{ sleep 6
  send 'A:\HISPADOS\VENTANAS' 7
  shot c1
  tecla alt-f4; sleep 1; tecla ret; sleep 3
  send 'ECHO TRAS-SALIR' 2
  mon quit
} | run 60 -vga std -global VGA.edid=off $FLOP
tam c1 800 600
printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nDEVICE=A:\\HISPADOS\\HSPEMM.SYS\r\nDOS=HIGH,UMB\r\nFILES=30\r\n' > $T/C2.SYS
mcopy -o -i $T/f.img $T/C2.SYS ::CONFIG.SYS
{ sleep 7
  send 'A:\HISPADOS\VENTANAS /32' 9
  shot c2
  mon quit
} | run 60 $PANT $FLOP
tam c2 1440 900
chk $T/c2.ppm px 1300 10 1 "V86: la barra de arriba" px 1430 450 2 "V86: el fondo"

if [ -f "$USB486" ]; then
  echo "== El escritorio de Hispa-486 a 1440x900 =="
  cp "$USB486" $T/u.img
  tests/tools/sinmenu.sh $T/u.img > /dev/null || ko "sinmenu"
  { sleep 24; shot d1
    mon quit; } | run 60 $PANT -no-reboot -drive file=$T/u.img,format=raw,if=ide,index=0 -boot c
  tam d1 1440 900
  # la casilla en (12,12): el dibujo (32) en (48,28) y el nombre centrado en x=64, y=70
  chk $T/d1.ppm texto 24 70 9 - fina "Biblioteca" nopx 64 44 2 "el dibujo del icono «Biblioteca»"
  muestra d1 escritorio486
  mtype -i $T/u.img@@1M ::HISPADOS/VENTANAS.INI > $T/v.ini; printf '[Pantalla]\r\nIconos=grandes\r\n' >> $T/v.ini
  mcopy -o -i $T/u.img@@1M $T/v.ini ::HISPADOS/VENTANAS.INI
  { sleep 24; tecla ctrl-esc; sleep 1.5; shot d2
    mon quit; } | run 60 $PANT -no-reboot -drive file=$T/u.img,format=raw,if=ide,index=0 -boot c
  chk $T/d2.ppm busca 0 360 300 880 0 8 "Configuración" texto 32 78 9 - fina "Biblioteca"
  muestra d2 escritorio486_grandes
fi

rm -rf $T
[ $fail -eq 0 ] && echo "== Resolución: todo bien ==" || echo "== Resolución: HAY FALLOS =="
exit $fail
