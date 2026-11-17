#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# probar.sh PROGRAMA.COM [argumentos...]
# Ejecuta un .COM dentro de Hispa-8086 (en QEMU) y enseña lo que escribe.
# El programa se copia al disquete de Hispa-8086 (h8086/build, que se monta
# solo si falta) y se lanza desde el AUTOEXEC.BAT con la salida mandada a un
# archivo, que luego se saca del disquete. Vale para programas que no esperan
# teclas; para lo demás, pruebas.sh (que sí las manda).
# Necesita qemu-system-i386, socat y mtools.
#
# Con HISPADOS_IMG=ruta/hispados-disquete.img lo ejecuta en Hispa-386 (la
# imagen de «make usb»), tecleando por el puerto serie, como al principio.
set -u
COM=${1:-}
[ -f "$COM" ] || { echo "uso: probar.sh PROGRAMA.COM [argumentos...]"; exit 2; }
shift
NOM=$(basename "${COM%.*}" | tr 'a-z' 'A-Z')
AQUI=$(dirname "$(readlink -f "$0")")
T=$(mktemp -d)
QPID=
trap '[ -n "$QPID" ] && kill $QPID 2>/dev/null; rm -rf $T' EXIT

if [ -n "${HISPADOS_IMG:-}" ]; then
  [ -f "$HISPADOS_IMG" ] || { echo "No encuentro la imagen de Hispa-386: $HISPADOS_IMG"; exit 2; }
  cp "$HISPADOS_IMG" "$T/f.img"
  mcopy -o -i "$T/f.img" "$COM" "::$NOM.COM"
  { sleep 7
    printf '%s\r' "$NOM $*"; sleep 3
    printf '%s\r' "ECHO [fin]"; sleep 1.5
  } | timeout 90 qemu-system-i386 -display none -serial stdio -monitor none -parallel none \
      -drive file="$T/f.img",format=raw,if=floppy -boot a -no-reboot > "$T/log" 2>&1
  tr -d '\r' < "$T/log" | iconv -f CP437 -t UTF-8 2>/dev/null > "$T/log.txt"
  awk -v n="$NOM" 'index($0,n){v=1} v' "$T/log.txt" | sed '/\[fin\]/q'
  exit 0
fi

H=$AQUI/../h8086
[ -f $H/build/hispa8086-1440.img ] || make -C $H > /dev/null 2>&1
[ -f $H/build/hispa8086-1440.img ] || { echo "No hay disquete de Hispa-8086 (make en h8086)"; exit 2; }
cp $H/build/hispa8086-1440.img $T/a.img
printf '@ECO OFF\r\n%s %s > SALIDA.TXT\r\nECO [castel-fin]\r\n' "$NOM" "$*" > $T/AUTOEXEC.BAT
mcopy -o -i $T/a.img "$COM" "::$NOM.COM"
mcopy -o -i $T/a.img $T/AUTOEXEC.BAT ::
qemu-system-i386 -display none -monitor unix:$T/mon,server,nowait -cpu 486 -m 4 \
  -drive file=$T/a.img,format=raw,if=floppy,index=0 -boot a -no-reboot > /dev/null 2>&1 &
QPID=$!
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; sleep 0.3; }
sleep 3
hecho=0
for ((i = 0; i < 60; i++)); do
  mon "pmemsave 0xb8000 4000 \"$T/scr.bin\""
  if [ -f $T/scr.bin ] && python3 -c "
import sys
d = open(sys.argv[1], 'rb').read()
sys.exit(0 if b'[castel-fin]' in d[::2] else 1)" $T/scr.bin; then hecho=1; break; fi
  sleep 0.5
done
mon quit
wait $QPID 2>/dev/null
QPID=
mcopy -n -i $T/a.img ::SALIDA.TXT $T/SALIDA.TXT > /dev/null 2>&1
[ -f $T/SALIDA.TXT ] && tr -d '\r' < $T/SALIDA.TXT | iconv -f CP437 -t UTF-8
if [ $hecho = 0 ]; then echo "(el programa no ha acabado: ¿espera una tecla?)"; exit 1; fi
