#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo32/probar.sh [NUCLEO.BIN] [ORDEN...]
# Arranca la semilla de Hispa-Pentium (un núcleo multiboot) en QEMU, le
# teclea las órdenes que se digan (por el monitor de QEMU: «sendkey», como un
# teclado de verdad) y enseña lo que escribe por el puerto serie y su código
# de salida. Sin órdenes: «ayuda» y «apagar». Una orden «:espera» se queda
# un segundo y medio sin teclear (para que corra el reloj); «:tecla up ret» o
# «:tecla ctrl-c» pulsa esas teclas tal cual (con los nombres de «sendkey»),
# sin el Intro del final. El teclado es el español: cada letra de una orden se
# teclea con la tecla (o las teclas: á es el acento y la a) que la da en él.
# Sin NUCLEO.BIN (o si no existe), compila antes semilla.cas y los programas
# de usuario (uhola.cas, umalo.cas y ucli.cas), que se le dan como módulos de multiboot
# (-initrd). Con MODULOS="a.bin,b.bin", esos módulos (MODULOS= para ninguno).
# Con MEMORIA=n, n MB (32 si no se dice). Con RELOJ=2001-01-01 (TLS), el reloj del PC
# empieza en esa fecha (si no, en la de aquí: «-rtc base=localtime»).
# Para el escritorio (ARRANQUE=escritorio, o la orden «escritorio»), el ratón y
# las capturas: «:raton X Y» lleva la flecha a ese punto de la pantalla;
# «:clic X Y» pulsa allí el botón izquierdo; «:doble X Y», dos veces;
# «:triple X Y», tres; «:clicd X Y», el botón derecho (el secundario);
# «:mayusclic X Y», el izquierdo con Mayús pulsada; «:ctrlclic X Y», con Control;
# «:arrastra X1 Y1 X2 Y2» lo lleva pulsado de un sitio a otro; «:escribe
# texto» lo teclea sin Intro; «:pausa S» espera S segundos; «:monitor ORDEN»
# le da esa orden al monitor de QEMU (device_add, device_del...: enchufar y
# desenchufar en marcha); «:captura NOMBRE»
# deja la pantalla en $CAPTURAS/NOMBRE.ppm (CAPTURAS: una carpeta; /tmp si no
# se dice). La flecha empieza en el centro (PANTALLA=800x600 si no se dice).
# «:hasta S N texto» espera a que el puerto serie diga N veces «texto» (con
# RELOJ_A=archivo, apunta la hora: para medir); «:reloj NOMBRE», la hora ya.
# «:monitor ORDEN» le da esa orden al monitor de QEMU (como «o /b 0x70 0x60»:
# escribir en un puerto; con QEMU_MAS="-S", antes de arrancar, y «cont»).
# (PANEL) «:esquina» lleva la flecha arriba a la izquierda y cuenta desde ahí.
# «:texto NOMBRE» deja la pantalla de texto (sus 25 líneas) en
# $CAPTURAS/NOMBRE.txt (para el programa que instala, que la pinta entera).
# Con DISCO=imagen, esa imagen como disco duro IDE (el maestro del canal
# primario): la hace nucleo32/disco.sh, con el intérprete de órdenes y los
# programas. Lo que se escriba en el disco no llega a la imagen (se queda en
# una copia de QEMU), salvo con ESCRIBE=1. Con ARRANQUE="texto", lo que se le
# dice al núcleo al arrancar (ARRANQUE=ordenes: lanza el intérprete de órdenes).
# Necesita python3, nasm, qemu-system-i386 y socat.
set -u
export LC_ALL=C.UTF-8
AQUI=$(dirname "$(readlink -f "$0")")
T=$(mktemp -d)
QPID=
trap '[ -n "$QPID" ] && kill $QPID 2>/dev/null; rm -rf $T' EXIT
BIN=${1:-}
[ $# -gt 0 ] && shift
if [ -z "$BIN" ] || [ ! -f "$BIN" ]; then
  python3 -I $AQUI/../ccastel.py $AQUI/semilla.cas -o $T/semilla.asm > /dev/null || exit 1
  nasm -f bin $T/semilla.asm -o $T/SEMILLA.BIN || exit 1
  BIN=$T/SEMILLA.BIN
  if [ -z "${MODULOS+x}" ]; then
    MODULOS=
    for u in uhola umalo ucli; do
      python3 -I $AQUI/../ccastel.py $AQUI/$u.cas -o $T/$u.asm > /dev/null || exit 1
      nasm -f bin $T/$u.asm -o $T/$u.bin || exit 1
      MODULOS=$MODULOS${MODULOS:+,}$T/$u.bin
    done
  fi
fi
INITRD=()
[ -n "${MODULOS:-}" ] && INITRD=(-initrd "$MODULOS")
[ $# -gt 0 ] || set -- ayuda apagar
INSTANTANEA=on; [ -n "${ESCRIBE:-}" ] && INSTANTANEA=off
[ -n "${DISCO:-}" ] && INITRD+=(-drive "file=$DISCO,format=raw,if=ide,index=0,snapshot=$INSTANTANEA")
# con un disco (una imagen), se deja escribir en él; con SOLO_LECTURA=1, no
# (como en un PC de verdad, donde hay que pedirlo: «escritura»)
[ -n "${DISCO:-}${SATA:-}" ] && [ -z "${SOLO_LECTURA:-}" ] && ARRANQUE="${ARRANQUE:+$ARRANQUE }escritura"
[ -n "${ARRANQUE:-}" ] && INITRD+=(-append "$ARRANQUE")
[ -n "${VGA:-}" ] && INITRD+=(-vga "$VGA")        # (VGA=none: un PC sin tarjeta gráfica que valga)
# con SATA=imagen, esa imagen como disco SATA de una controladora AHCI (con
# ESCRIBE=1, lo que se escriba llega a ella); con MAQUINA=..., esa máquina de
# QEMU (MAQUINA=pc,i8042=off: un PC sin teclado ni ratón PS/2, como un Mac);
# con QEMU_MAS="...", lo que se quiera añadir (controladoras y aparatos USB, la
# tarjeta de red)
[ -n "${SATA:-}" ] && INITRD+=(-device ahci,id=sata -drive "id=dsata,file=$SATA,format=raw,if=none,snapshot=$INSTANTANEA" -device ide-hd,drive=dsata,bus=sata.0)
[ -n "${MAQUINA:-}" ] && INITRD+=(-machine "$MAQUINA")
# shellcheck disable=SC2206
[ -n "${QEMU_MAS:-}" ] && INITRD+=($QEMU_MAS)
# sin tarjeta de red, salvo que se dé una en QEMU_MAS («-nic user,model=e1000»,
# «-netdev user,id=n0 -device rtl8139,netdev=n0»): QEMU pone una e1000 si no
# se dice nada, y con ella el escritorio pondría en marcha el servicio de red
case " ${QEMU_MAS:-} " in *" -nic "*|*" -netdev "*) ;; *) INITRD+=(-nic none) ;; esac
qemu-system-i386 -kernel "$BIN" "${INITRD[@]}" -display none -monitor unix:$T/mon,server,nowait \
  -serial file:$T/sal -device isa-debug-exit,iobase=0xf4,iosize=0x04 -no-reboot -rtc base=${RELOJ:-localtime} -m ${MEMORIA:-32} > /dev/null 2>&1 &
QPID=$!
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; sleep "${2:-0.08}"; }
# las teclas de cada letra en el teclado español, como las llama «sendkey»
# (alt_r es Alt Gr; los acentos son teclas muertas: primero el acento)
tecla() {
  case "$1" in
    ' ') echo spc ;; '-') echo slash ;; '_') echo shift-slash ;; '.') echo dot ;; ':') echo shift-dot ;;
    ',') echo comma ;; ';') echo shift-comma ;; "\\") echo alt_r-grave_accent ;; '/') echo shift-7 ;;
    '?') echo shift-minus ;; "'") echo minus ;; '¡') echo equal ;; '¿') echo shift-equal ;;
    '<') echo less ;; '>') echo shift-less ;; '*') echo shift-bracket_right ;; '+') echo bracket_right ;;
    '!') echo shift-1 ;; '"') echo shift-2 ;; '$') echo shift-4 ;; '%') echo shift-5 ;; '&') echo shift-6 ;;
    '(') echo shift-8 ;; ')') echo shift-9 ;; '=') echo shift-0 ;;
    '|') echo alt_r-1 ;; '@') echo alt_r-2 ;; '#') echo alt_r-3 ;; '~') echo alt_r-4 ;;
    '[') echo alt_r-bracket_left ;; ']') echo alt_r-bracket_right ;; '{') echo alt_r-apostrophe ;; '}') echo alt_r-backslash ;;
    'ñ') echo semicolon ;; 'Ñ') echo shift-semicolon ;; 'ç') echo backslash ;; 'Ç') echo shift-backslash ;;
    'º') echo grave_accent ;; 'ª') echo shift-grave_accent ;;
    '€') echo alt_r-e ;;                           # (OFICINA2) Alt Gr+E
    'á') echo apostrophe a ;; 'é') echo apostrophe e ;; 'í') echo apostrophe i ;; 'ó') echo apostrophe o ;; 'ú') echo apostrophe u ;;
    'Á') echo apostrophe shift-a ;; 'É') echo apostrophe shift-e ;; 'Í') echo apostrophe shift-i ;; 'Ó') echo apostrophe shift-o ;; 'Ú') echo apostrophe shift-u ;;
    'à') echo bracket_left a ;; 'è') echo bracket_left e ;; 'ò') echo bracket_left o ;;
    'â') echo shift-bracket_left a ;; 'ê') echo shift-bracket_left e ;; 'ô') echo shift-bracket_left o ;;
    'ü') echo shift-apostrophe u ;; 'Ü') echo shift-apostrophe shift-u ;; 'ï') echo shift-apostrophe i ;;
    '^') echo shift-bracket_left spc ;; '`') echo bracket_left spc ;;
    [A-Z]) echo "shift-$(echo $1 | tr A-Z a-z)" ;;
    *) echo "$1" ;;
  esac
}
for ((i = 0; i < 40; i++)); do [ -S $T/mon ] && break; sleep 0.1; done
sleep 1.5
pantalla() {
  mon "pmemsave 0xb8000 4000 \"$T/pantalla.bin\"" 0.3
  python3 - $T/pantalla.bin <<'PY' > $T/pantalla.txt
import sys
d = open(sys.argv[1], 'rb').read()
for f in range(25):
    print('|' + bytes(d[f * 160:(f + 1) * 160:2]).decode('cp437').rstrip())
PY
}
# el ratón: la flecha, de donde está (RX, RY) a un punto, a pasos cortos
PANTALLA=${PANTALLA:-800x600}
RX=$((${PANTALLA%x*} / 2)); RY=$((${PANTALLA#*x} / 2))
ir() {
  local dx=$(($1 - RX)) dy=$(($2 - RY)) sx sy
  while [ $dx -ne 0 ] || [ $dy -ne 0 ]; do
    sx=$dx; [ $sx -gt 50 ] && sx=50; [ $sx -lt -50 ] && sx=-50
    sy=$dy; [ $sy -gt 50 ] && sy=50; [ $sy -lt -50 ] && sy=-50
    mon "mouse_move $sx $sy" 0.05
    dx=$((dx - sx)); dy=$((dy - sy))
  done
  RX=$1; RY=$2; sleep 0.25
}
n=0
ORDENES=("$@")
for orden in "${ORDENES[@]}"; do
  n=$((n + 1))
  # la pantalla, por si se quiere mirar (VER=1): antes de la última orden
  [ -n "${VER:-}" ] && [ $n = ${#ORDENES[@]} ] && pantalla
  if [ "$orden" = ":espera" ]; then sleep 1.5; continue; fi
  if [ "${orden:0:7}" = ":tecla " ]; then
    for k in ${orden:7}; do mon "sendkey $k"; done
    sleep 0.3; continue
  fi
  case "$orden" in
    ":pausa "*) sleep ${orden:7}; continue ;;
    # (RÁPIDO) «:hasta S N texto»: espera (hasta S segundos) a que el puerto serie
    # haya dicho N veces «texto»; y con RELOJ_A=archivo, apunta allí la hora
    # (en segundos, con decimales) al acabar de esperar: lo que tarda algo
    ":hasta "*) set -- ${orden:7}; s=$1; c=$2; shift 2
      for ((i = 0; i < s * 4; i++)); do [ "$(tr -d '\r' < $T/sal | grep -a -c -- "$*")" -ge $c ] && break; sleep 0.25; done
      [ -n "${RELOJ_A:-}" ] && echo "$* $c $(date +%s.%N)" >> "$RELOJ_A"
      continue ;;
    ":reloj "*) [ -n "${RELOJ_A:-}" ] && echo "${orden:7} $(date +%s.%N)" >> "$RELOJ_A"; continue ;;
    ":monitor "*) mon "${orden:9}" 0.3; continue ;;
    ":raton "*) set -- ${orden:7}; ir $1 $2; continue ;;
    # (PANEL) «:esquina»: la flecha, arriba a la izquierda del todo (el núcleo
    # la para en el borde), y se cuenta desde ahí: tras cambiar la velocidad del
    # ratón en el Panel de control, lo que se mueve ya no es punto a punto
    ":esquina") for _ in $(seq 40); do mon "mouse_move -60 -60" 0.03; done; RX=0; RY=0; sleep 0.3; continue ;;
    ":clic "*) set -- ${orden:6}; ir $1 $2; mon "mouse_button 1" 0.2; mon "mouse_button 0" 0.4; continue ;;
    ":doble "*) set -- ${orden:7}; ir $1 $2; mon "mouse_button 1" 0.1; mon "mouse_button 0" 0.1; mon "mouse_button 1" 0.1; mon "mouse_button 0" 0.5; continue ;;
    ":triple "*) set -- ${orden:8}; ir $1 $2; for _ in 1 2; do mon "mouse_button 1" 0.1; mon "mouse_button 0" 0.1; done; mon "mouse_button 1" 0.1; mon "mouse_button 0" 0.5; continue ;;
    ":clicd "*) set -- ${orden:7}; ir $1 $2; mon "mouse_button 2" 0.2; mon "mouse_button 0" 0.4; continue ;;
    ":mayusclic "*) set -- ${orden:11}; ir $1 $2; mon "sendkey shift 900" 0.3; mon "mouse_button 1" 0.2; mon "mouse_button 0" 0.9; continue ;;
    ":ctrlclic "*) set -- ${orden:10}; ir $1 $2; mon "sendkey ctrl 900" 0.3; mon "mouse_button 1" 0.2; mon "mouse_button 0" 0.9; continue ;;
    ":arrastra "*) set -- ${orden:10}; ir $1 $2; mon "mouse_button 1" 0.25; ir $3 $4; mon "mouse_button 0" 0.4; continue ;;
    ":captura "*) sleep 0.4; mon "screendump ${CAPTURAS:-/tmp}/${orden:9}.ppm" 0.5; continue ;;
    ":texto "*) sleep 0.4; pantalla; cp $T/pantalla.txt "${CAPTURAS:-/tmp}/${orden:7}.txt"; continue ;;
    ":escribe "*)
      t=${orden:9}
      for ((j = 0; j < ${#t}; j++)); do for k in $(tecla "${t:$j:1}"); do mon "sendkey $k"; done; done
      sleep 0.3; continue ;;
  esac
  for ((j = 0; j < ${#orden}; j++)); do for k in $(tecla "${orden:$j:1}"); do mon "sendkey $k"; done; done
  mon "sendkey ret" 0.4
done
for ((i = 0; i < 50; i++)); do kill -0 $QPID 2>/dev/null || break; sleep 0.1; done
if kill -0 $QPID 2>/dev/null; then
  mon quit
  wait $QPID 2>/dev/null
  r=no
else
  wait $QPID
  r=$?
fi
QPID=
tr -d '\r' < $T/sal | iconv -f CP437 -t UTF-8
if [ "$r" = no ]; then echo "[sigue encendido]"; else echo "[código $(((r - 1) / 2))]"; fi
[ -n "${VER:-}" ] && [ -f $T/pantalla.txt ] && { echo "[pantalla]"; cat $T/pantalla.txt; }
