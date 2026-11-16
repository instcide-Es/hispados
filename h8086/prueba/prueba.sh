#!/bin/bash
# Prueba de los cimientos de Hispa-8086: arranca el disquete en QEMU, teclea
# órdenes y mira lo que queda escrito en la pantalla de texto.
#
# QEMU no tiene un 8086: lo que aquí se comprueba es que el sistema arranca
# y se comporta, en un 486 y en un Pentium. Que no hay ni una instrucción
# posterior al 8086 lo garantiza el ensamblador («cpu 8086» en los fuentes).
# Necesita qemu-system-i386, socat y mtools.
set -u
cd "$(dirname "$0")/.."
T=$(mktemp -d)
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; sleep "${2:-0.25}"; }
tecla() { local k; for k in "$@"; do mon "sendkey $k" 0.12; done; }
escribe() {
  local t="$1" i c
  for ((i=0; i<${#t}; i++)); do
    c="${t:$i:1}"
    case "$c" in ' ') tecla spc;; '.') tecla dot;; *) tecla "$c";; esac
  done
  tecla ret; sleep 0.8
}
# la pantalla de texto (80x25, un octeto de letra y otro de color), a texto
pantalla() {
  mon "pmemsave 0xb8000 4000 \"$T/scr.bin\"" 0.4
  python3 - $T/scr.bin <<'PY' > $T/$1.txt
import sys
d = open(sys.argv[1], 'rb').read()
for f in range(25):
    print(bytes(d[f * 160:(f + 1) * 160:2]).decode('cp437').rstrip())
PY
}
esta() { if grep -qF -- "$2" $T/$1.txt; then ok "$3«$2»"; else ko "$3falta «$2»"; fi; }
noesta() { if grep -qF -- "$2" $T/$1.txt; then ko "$3sobra «$2»"; else ok "$3no sale «$2»"; fi; }
arranca() {   # imagen, cpu, memoria en K
  rm -f $T/mon
  qemu-system-i386 -display none -monitor unix:$T/mon,server,nowait -cpu $2 -m 4 \
    -drive file=$1,format=raw,if=floppy,index=0 -boot a -no-reboot > /dev/null 2>&1 &
  QPID=$!
  sleep 5
}
apaga() { mon quit 0.5; wait $QPID 2>/dev/null; }

echo "== Hispa-8086: arranque y órdenes (disquete de 360 K, en un 486) =="
cp build/hispa8086-360.img $T/a.img
arranca $T/a.img 486
pantalla p1
esta p1 "Hispa-8086 0.1 - de la familia Rescoldo" ""
esta p1 "Copyright (C) 2026 Carlos Sanz y contribuidores" ""
esta p1 "Procesador: 80486 o superior" ""
esta p1 "K libres para los programas)" ""
esta p1 "puede con HISPA-DOS" "el consejo: "
esta p1 "A>" ""
tecla ctrl-l 2>/dev/null
escribe "borrar"; escribe "ver"; escribe "dir"
pantalla p2
esta p2 "Hispa-8086 versión 0.1 (cimientos)" "VER: "
esta p2 "NUCLEO86 SYS" "DIR: "
esta p2 "HOLA     COM       206" "DIR: "
esta p2 "LEEME    TXT" "DIR: "
esta p2 "3 archivos, " "DIR: "
noesta p2 "HISPA-8086 " "DIR: (la etiqueta) "
escribe "borrar"; escribe "tipo leeme.txt"
pantalla p3
esta p3 "El sistema de la familia Hispa para los PC, XT y AT: 8086, 8088, V20," "TIPO: "
esta p3 "en español." "TIPO: "
escribe "borrar"; escribe "hola uno dos"; escribe "tipo nada.txt"; escribe "xyz"; escribe "equipo"
pantalla p4
esta p4 "Hola desde un programa .COM. DOS 3.30" "HOLA.COM: "
esta p4 "Parametros: uno dos" "HOLA.COM: "
esta p4 "Vectores: bien" "HOLA.COM: "
esta p4 "No existe ese archivo." ""
esta p4 "Orden desconocida. Escriba AYUDA." ""
esta p4 "Procesador: 80486 o superior" "EQUIPO: "
apaga

echo "== Hispa-8086: las demás medidas de disquete (720 K, 1,2 M y 1,44 M) =="
for m in 720 1200 1440; do
  cp build/hispa8086-$m.img $T/b.img
  arranca $T/b.img pentium
  escribe "hola"; escribe "dir"
  pantalla p5
  esta p5 "Hispa-8086 0.1" "$m K: "
  esta p5 "Hola desde un programa .COM. DOS 3.30" "$m K: "
  esta p5 "3 archivos, " "$m K: "
  apaga
done
# el sistema base tiene que caber en el disquete de los XT
libre=$(mdir -i build/hispa8086-360.img :: | awk '/bytes free/ { gsub(/ /, "", $0); sub(/bytesfree/, "", $0); print }')
if [ "${libre:-0}" -gt 100000 ]; then ok "en el disquete de 360 K quedan $libre octetos libres"; else ko "el disquete de 360 K se queda sin sitio ($libre)"; fi

echo "== Hispa-8086: solo instrucciones del 8086 =="
for f in arranque nucleo; do
  if grep -q "^\s*cpu 8086" $f.asm; then ok "$f.asm se ensambla con «cpu 8086»"; else ko "$f.asm no lleva «cpu 8086»"; fi
done
# lo único de 386 es la prueba que distingue el 386 del 486, que solo corre
# cuando ya se sabe que el procesador es de 32 bits
n=$(grep -c "^\s*cpu 386" nucleo.asm)
if [ "$n" = 1 ]; then ok "un solo tramo de 386 (detrás de la prueba del 286)"; else ko "tramos de 386: $n"; fi
tam=$(stat -c %s build/NUCLEO86.SYS)
if [ $tam -lt 8192 ]; then ok "el núcleo ocupa $tam octetos"; else ko "el núcleo ha crecido a $tam octetos"; fi

if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then for p in p1 p2 p3 p4 p5; do echo "--- $p"; cat $T/$p.txt 2>/dev/null; done; fi
rm -rf $T
exit $fail
