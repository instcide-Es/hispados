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
# Con MEMORIA=n, n MB (32 si no se dice).
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
[ -n "${ARRANQUE:-}" ] && INITRD+=(-append "$ARRANQUE")
qemu-system-i386 -kernel "$BIN" "${INITRD[@]}" -display none -monitor unix:$T/mon,server,nowait \
  -serial file:$T/sal -device isa-debug-exit,iobase=0xf4,iosize=0x04 -no-reboot -rtc base=localtime -m ${MEMORIA:-32} > /dev/null 2>&1 &
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
n=0
for orden in "$@"; do
  n=$((n + 1))
  # la pantalla, por si se quiere mirar (VER=1): antes de la última orden
  [ -n "${VER:-}" ] && [ $n = $# ] && pantalla
  if [ "$orden" = ":espera" ]; then sleep 1.5; continue; fi
  if [ "${orden:0:7}" = ":tecla " ]; then
    for k in ${orden:7}; do mon "sendkey $k"; done
    sleep 0.3; continue
  fi
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
