#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo32/probar.sh [NUCLEO.BIN] [ORDEN...]
# Arranca la semilla de Hispa-Pentium (un núcleo multiboot) en QEMU, le
# teclea las órdenes que se digan (por el monitor de QEMU: «sendkey», como un
# teclado de verdad) y enseña lo que escribe por el puerto serie y su código
# de salida. Sin órdenes: «ayuda» y «apagar». Una orden «:espera» se queda
# un segundo y medio sin teclear (para que corra el reloj).
# Sin NUCLEO.BIN (o si no existe), compila antes semilla.cas y los programas
# de usuario (uhola.cas, umalo.cas y ucli.cas), que se le dan como módulos de multiboot
# (-initrd). Con MODULOS="a.bin,b.bin", esos módulos (MODULOS= para ninguno).
# Con MEMORIA=n, n MB (32 si no se dice).
# Con DISCO=imagen, esa imagen como disco duro IDE (el maestro del canal
# primario; no se escribe en ella): la hace nucleo32/disco.sh, con el
# intérprete de órdenes y los programas. Con ARRANQUE="texto", lo que se le
# dice al núcleo al arrancar (ARRANQUE=ordenes: lanza el intérprete de órdenes).
# Necesita python3, nasm, qemu-system-i386 y socat.
set -u
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
[ -n "${DISCO:-}" ] && INITRD+=(-drive "file=$DISCO,format=raw,if=ide,index=0,snapshot=on")
[ -n "${ARRANQUE:-}" ] && INITRD+=(-append "$ARRANQUE")
qemu-system-i386 -kernel "$BIN" "${INITRD[@]}" -display none -monitor unix:$T/mon,server,nowait \
  -serial file:$T/sal -device isa-debug-exit,iobase=0xf4,iosize=0x04 -no-reboot -m ${MEMORIA:-32} > /dev/null 2>&1 &
QPID=$!
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; sleep "${2:-0.08}"; }
# la tecla de cada letra, como la llama «sendkey»
tecla() {
  case "$1" in
    ' ') echo spc ;; '-') echo minus ;; '.') echo dot ;; ',') echo comma ;; '_') echo shift-minus ;;
    "\\") echo backslash ;; '/') echo slash ;; '?') echo shift-slash ;; ':') echo shift-semicolon ;;
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
  for ((j = 0; j < ${#orden}; j++)); do mon "sendkey $(tecla "${orden:$j:1}")"; done
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
