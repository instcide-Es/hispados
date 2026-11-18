#!/bin/bash
# Prueba del icono de red de la barra de tareas de Hispa-486 y de su
# ventanita (ventanas/wifi.inc; docs/WIFI.md «En Hispa Ventanas»). En QEMU:
#  1.ª sesión, Wi-Fi con la radio simulada (HSPWIFI /SIMULA): el icono sin
#     conectar (cuatro rayitas huecas y un aspa); un clic abre la ventanita,
#     que busca sola y enseña las tres redes de la radio simulada con su
#     señal y su seguridad; «Conectar» sin contraseña la pide; con una
#     contraseña equivocada (en asteriscos) dice por qué no conecta; con la
#     buena conecta: «Conectado a «HispaPrueba»», la señal y el icono con
#     las cuatro rayitas llenas; RED.INI guarda la red y la clave preparada
#     (no la contraseña). Otra vez en Hispa Ventanas: «Desconectar»; «Conectar»
#     sin escribir nada usa la clave guardada; «Olvidar» la quita de RED.INI;
#     «Configuración de red...» abre Configuración en el apartado «Red».
#  2.ª sesión, red por cable (una e1000 con HSPRED y CONFRED): el icono de los
#     dos nodos; la ventanita con el enlace, la IP y la puerta de enlace.
#  3.ª sesión, sin red: el icono hueco con el aspa; la ventanita lo dice.
# Dos QEMU a la vez como mucho. Deja capturas en build/wifi_ventana_*.png
# (GUARDAR=carpeta: también allí).
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
B=build
for f in HSPWIFI.COM WIFI.COM HSPWIFI.BIN VENTANAS.COM CONFIG.HVP HSPRED.COM CONFRED.COM; do
  [ -f $B/$f ] || { echo "falta $B/$f (make)"; exit 1; }
done
T=$(mktemp -d)
trap 'rm -rf $T' EXIT
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() { if grep -qF -- "$2" $T/$1.txt; then ok "$2"; else ko "$2"; fi; }
checkn() { if grep -qF -- "$2" $T/$1.txt; then ko "(no debía salir) $2"; else ok "sin «$2»"; fi; }
chk() { python3 -I tests/tools/ventanas_check.py "$@" || fail=1; }
export MTOOLS_SKIP_CHECK=1

# ---- lo de cada sesión: su monitor, su ratón y su teclado (S = la sesión) ----
mon() { printf '%s\n' "$2" | socat - UNIX-CONNECT:$T/$1.mon > /dev/null 2>&1; sleep "${3:-0.3}"; }
tecla() { local s=$1 k; shift; for k in "$@"; do mon $s "sendkey $k" 0.25; done; }
escribe() {   # escribe S texto: letras, cifras y espacios
  local s=$1 t=$2 i c
  for (( i=0; i<${#t}; i++ )); do
    c=${t:i:1}; [ "$c" = " " ] && c=spc
    mon $s "sendkey $c" 0.2
  done
}
shot() { sleep 0.5; mon $1 "screendump $T/$2.ppm" 0.7; }
send() { printf '%s\r' "$1"; sleep "${2:-1.5}"; }
declare -A RX RY
ir() {   # ir S X Y (el ratón de QEMU va de 2 en 2 con HSPRATON)
  local s=$1 dx=$(( ($2 - ${RX[$1]}) )) dy=$(( ($3 - ${RY[$1]}) )) sx sy
  while [ $dx -ne 0 ] || [ $dy -ne 0 ]; do
    sx=$dx; [ $sx -gt 60 ] && sx=60; [ $sx -lt -60 ] && sx=-60
    sy=$dy; [ $sy -gt 60 ] && sy=60; [ $sy -lt -60 ] && sy=-60
    mon $s "mouse_move $sx $sy" 0.07
    dx=$(( dx - sx )); dy=$(( dy - sy ))
  done
  RX[$s]=$2; RY[$s]=$3; sleep 0.3
}
clic() { ir $1 $2 $3; mon $1 "mouse_button 1" 0.25; mon $1 "mouse_button 0" 0.5; }
salir() {   # salir S: Inicio > Salir de Hispa Ventanas > Volver a Hispa-DOS (el 3.º en Hispa-486)
  tecla $1 ctrl-esc; sleep 0.8; tecla $1 up; sleep 0.4; tecla $1 ret; sleep 2; tecla $1 down down; sleep 0.4; tecla $1 ret; sleep 4
}
qemu() {   # qemu S plazo [opciones de QEMU]
  local s=$1 tmo=$2; shift 2
  rm -f $T/$s.mon
  timeout $tmo qemu-system-i386 -display none -serial stdio -monitor unix:$T/$s.mon,server,nowait \
    -parallel none -m 32 -drive file=$T/$s.img,format=raw,if=floppy,index=0 -boot a -no-reboot "$@" > $T/$s.raw 2>&1
  tr -d '\r' < $T/$s.raw | iconv -f CP437 -t UTF-8 -c > $T/$s.txt
}
mkimg() {   # mkimg S: la imagen con sitio para el núcleo del Wi-Fi (400 KB)
  cp "$IMG" $T/$1.img
  for f in HISPA.WAV AUDICION.COM HSPSND.COM EQUIPO.COM HSPUSB.SYS; do mdel -i $T/$1.img ::HISPADOS/$f 2>/dev/null; done
  mdeltree -i $T/$1.img ::HISPADOS/JUEGOS 2>/dev/null
  printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nDOS=HIGH\r\nFILES=30\r\n' > $T/CONFIG.SYS
  mcopy -o -i $T/$1.img $T/CONFIG.SYS ::CONFIG.SYS
  mcopy -o -i $T/$1.img $B/HSPWIFI.COM $B/WIFI.COM $B/HSPWIFI.BIN ::HISPADOS/
}
# En 800x600 sin batería: el altavoz en x = 732, el icono de red en 692..708,
# y = 576..590 (la barra, de 572 a 599). La ventanita del Wi-Fi: 364x283 en
# 348,281 (su zona en 350,306); la lista en 358,350 (filas de 18 desde 352);
# la contraseña en 454,469; los botones en y = 500 (Buscar 358, Conectar
# 436, Desconectar 530, Olvidar 640) y «Configuración de red...» en 358,532.
# La de cable y la de sin red: 320 de ancho en 392 (zona en 394), encima de
# la barra.
IX=700; IY=585                  # el icono de red
FILA() { echo $(( 352 + $1 * 18 + 9 )); }
BY=512                          # los botones de abajo...
CONF_Y=544                      # ...y «Configuración de red...»

echo "== Hispa Ventanas: el icono de red y su ventanita (Wi-Fi, cable y sin red) =="
mkimg w; mkimg c; mkimg n
RX[w]=400; RY[w]=300; RX[c]=400; RY[c]=300; RX[n]=400; RY[n]=300
# ---- 1.ª: Wi-Fi (radio simulada) ----
{ sleep 7; send 'HSPWIFI /SIMULA' 6
  send 'VENTANAS /486' 12
  ir w 300 200; shot w a1                       # el icono: sin conectar
  clic w $IX $IY; sleep 5; ir w 300 200; shot w a2   # la ventanita: busca sola
  clic w 380 $(FILA 1); clic w 480 $BY; sleep 1; shot w a3   # HispaPrueba, «Conectar» sin contraseña
  escribe w "otra clave mala"; sleep 0.5; shot w a4          # en asteriscos
  tecla w ret; sleep 12; shot w a5              # la equivocada: por qué no conecta
  escribe w "clave de prueba"; tecla w ret; sleep 10; ir w 300 200; shot w a6   # conectado
  tecla w esc; sleep 1; shot w a7               # cerrada: el icono con la señal
  salir w
  send 'TYPE A:\HISPADOS\RED.INI' 3
  send 'ECHO tras-conectar' 1
  send 'VENTANAS /486' 12
  RX[w]=400; RY[w]=300                          # (Hispa Ventanas pone el ratón en medio)
  clic w $IX $IY; sleep 3; ir w 300 200; shot w b1   # otra vez: la conectada, en negrita
  clic w 580 $BY; sleep 3; ir w 300 200; shot w b2   # «Desconectar»
  clic w 380 $(FILA 1); clic w 480 $BY; sleep 10; ir w 300 200; shot w b3   # «Conectar»: la clave guardada
  clic w 670 $BY; sleep 3; ir w 300 200; shot w b4   # «Olvidar»
  clic w 450 $CONF_Y; sleep 4; ir w 300 200; shot w b5   # «Configuración de red...»
  tecla w alt-f4; sleep 1.5
  salir w
  send 'TYPE A:\HISPADOS\RED.INI' 3
  send 'ECHO fin-de-la-prueba' 1
  mon w quit; } | qemu w 220 -net none &
P1=$!
# ---- 2.ª: cable (e1000, HSPRED, CONFRED) ----
{ sleep 7; send 'HSPRED' 5
  send 'CONFRED' 6
  send 'VENTANAS /486' 12
  ir c 300 200; shot c c1
  clic c $IX $IY; sleep 2; ir c 300 200; shot c c2
  tecla c esc; sleep 1
  salir c
  send 'ECHO fin-de-la-prueba' 1
  mon c quit; } | qemu c 110 -netdev user,id=n0 -device e1000,netdev=n0 &
P2=$!
wait $P1 $P2
# ---- 3.ª: sin red ----
{ sleep 7; send 'VENTANAS /486' 12
  ir n 300 200; shot n d1
  clic n $IX $IY; sleep 2; ir n 300 200; shot n d2
  tecla n esc; sleep 1
  salir n
  send 'ECHO fin-de-la-prueba' 1
  mon n quit; } | qemu n 80 -net none

# (los colores: 0 tinta, 5 terracota clara, 6 arena, 7 arena oscura, 8 crema,
# 9 blanco, 12 añil. El icono: las rayitas en x = 691..705 con el fondo en
# y = 589, la 4.ª en 703..705; el aspa del Wi-Fi en 691,575; los nodos del
# cable en 692,576 y 700,581, y su aspa en 701,574)
L1="texto 360 312 0 8 negrita"   # la 1.ª línea de la ventanita del Wi-Fi
echo "-- Wi-Fi: el icono sin conectar y la ventanita, que busca sola --"
chk $T/a1.ppm px 703 583 6 "rayitas huecas" nopx 704 583 8 "ninguna llena" px 691 575 5 "el aspa (sin conectar)"
chk $T/a2.ppm $L1 "Wi-Fi: sin conectar" busca 355 325 370 340 0 8 "Elija una red y pulse «Conectar»." \
  texto 366 353 9 12 fina "Vecina vieja" texto 366 371 0 9 fina "HispaPrueba" texto 366 389 0 9 fina "Vecina abierta" \
  busca 590 350 600 362 9 12 "no admitida" busca 590 368 600 380 0 9 "WPA2" busca 590 386 600 398 0 9 "abierta" \
  px 571 383 0 "la señal, en rayitas" px 582 375 0 "la 4.ª rayita (señal -47 dBm)"
echo "-- Conectar: pide la contraseña, la enseña en asteriscos, dice por qué no conecta --"
chk $T/a3.ppm busca 355 325 370 340 0 8 "Escriba la contraseña de la red." texto 366 371 9 12 fina "HispaPrueba"
chk $T/a4.ppm texto 459 472 0 9 fina "**************" nopx 583 476 0 "15 asteriscos, ni uno más"
chk $T/a5.ppm $L1 "Wi-Fi: sin conectar" busca 355 325 370 340 0 8 "No conecta: la contraseña no es esa." \
  nobusca 455 470 470 480 0 9 "*"
echo "-- con la buena: conectado, la señal y el icono --"
chk $T/a6.ppm $L1 "Conectado a «HispaPrueba»" busca 355 325 370 340 0 8 "Señal -47 dBm, sin IP: falta CONFRED" \
  texto 366 371 9 12 negrita "HispaPrueba" px 704 583 8 "el icono: cuatro rayitas llenas" nopx 691 575 5 "sin el aspa"
chk $T/a7.ppm px 704 583 8 "cerrada la ventanita, el icono sigue lleno" px 692 589 8 "la 1.ª rayita"
check w "WIFI=HispaPrueba"
if grep -Eq "^WIFICLAVE=[0-9a-f]{64}$" $T/w.txt; then ok "RED.INI guarda la clave preparada (64 cifras)"; else ko "WIFICLAVE= en RED.INI"; fi
checkn w "clave de prueba"
checkn w "otra clave mala"
echo "-- otra vez: desconectar, conectar con la clave guardada, olvidar --"
chk $T/b1.ppm $L1 "Conectado a «HispaPrueba»" texto 366 371 9 12 negrita "HispaPrueba"
chk $T/b2.ppm $L1 "Wi-Fi: sin conectar" busca 355 325 370 340 0 8 "Desconectado." px 691 575 5 "el icono: el aspa"
chk $T/b3.ppm $L1 "Conectado a «HispaPrueba»" px 704 583 8 "el icono: lleno"
chk $T/b4.ppm $L1 "Wi-Fi: sin conectar" busca 355 325 370 340 0 8 "Red olvidada (quitada de RED.INI)."
chk $T/b5.ppm busca 0 0 800 560 0 8 "Estado de la red:" busca 0 0 800 560 0 8 "Wi-Fi cargado (HSPWIFI)."
n=$(sed -n '/tras-conectar/,$p' $T/w.txt | grep -c "^WIFI")
if [ "$n" -eq 0 ]; then ok "tras «Olvidar» (y cerrar Configuración), RED.INI ya no lleva la red"; else ko "RED.INI sigue con $n líneas WIFI"; fi
check w "fin-de-la-prueba"
echo "-- cable: los dos nodos; el enlace, la IP y la puerta de enlace --"
chk $T/c1.ppm px 694 578 8 "nodos llenos" nopx 701 574 5 "sin aspa"
chk $T/c2.ppm texto 404 446 0 8 negrita "Red por cable" busca 395 460 420 490 0 8 "Enlace: conectado" \
  busca 395 480 420 500 0 8 "Dirección IP: 10.0.2.15" busca 395 495 420 520 0 8 "Puerta de enlace: 10.0.2.2"
check c "fin-de-la-prueba"
echo "-- sin red: los nodos huecos con el aspa; la ventanita lo dice --"
chk $T/d1.ppm px 692 576 7 "nodos huecos" nopx 694 578 8 "sin rellenar" px 701 574 5 "el aspa"
chk $T/d2.ppm busca 395 470 420 500 0 8 "No hay controlador de red cargado:"
check n "fin-de-la-prueba"

for f in $T/*.ppm; do
  n=$(basename $f .ppm)
  if command -v convert > /dev/null; then convert $f $B/wifi_ventana_$n.png; else cp $f $B/wifi_ventana_$n.ppm; fi
  [ -n "${GUARDAR:-}" ] && cp $B/wifi_ventana_$n.* "$GUARDAR"/
done
for s in w c n; do cp $T/$s.txt $B/wifi_ventana_$s.txt 2>/dev/null; done
echo "(capturas en $B/wifi_ventana_*)"
if [ $fail -eq 0 ]; then echo "Icono de red: todas las pruebas pasan"; else echo "Icono de red: HAY FALLOS"; fi
exit $fail
