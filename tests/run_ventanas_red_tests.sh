#!/bin/bash
# Icono de RED en la barra de arriba de Hispa Ventanas (a la izquierda del
# altavoz): dos nodos unidos, en crema si hay un controlador de red (packet
# driver, p. ej. HSPRED) cargado, y en arena oscura si no hay. La detección
# mira la firma "PKT DRVR" de los vectores INT 60h..80h.
# QEMU no da batería (APM), así que el altavoz se pega a la hora y el icono de
# red queda en ~x=624; se comprueba el color del nodo en (626,4).
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
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; sleep "${2:-0.3}"; }
shot() { sleep 0.4; mon "screendump $T/$1.ppm" 0.6; }
send() { printf '%s\r' "$1"; sleep "${2:-1.5}"; }
muestra() { [ -f $T/$1.ppm ] && convert $T/$1.ppm $B/ventanas_red_$2.png; }

# run ETIQUETA CONRED(0/1): arranca Hispa Ventanas; si CONRED=1, carga HSPRED
# (con una tarjeta RTL8139 de QEMU) antes de VENTANAS.
run() {
  cp "$IMG" $T/f.img
  printf "DEVICE=A:\\\\HISPADOS\\\\HSPXMS.SYS\r\nFILES=30\r\n" > $T/CONFIG.SYS
  mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
  rm -f $T/mon
  local red=""
  [ "$2" = 1 ] && red="-netdev user,id=n0 -device rtl8139,netdev=n0"
  { sleep 6
    send 'A:\HISPADOS\HSPDPMI' 2
    send 'A:\HISPADOS\HSPRATON' 2
    [ "$2" = 1 ] && send 'A:\HISPADOS\HSPRED' 3
    send 'VENTANAS' 7
    shot "$1"
    mon quit
  } | timeout 60 qemu-system-i386 -display none -serial stdio -monitor unix:$T/mon,server,nowait \
      -parallel none -m 32 -rtc base=2026-11-15T23:58:04 \
      -drive file=$T/f.img,format=raw,if=floppy,index=0 -boot a -no-reboot $red > $T/raw 2>&1
  tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 -c > $T/$1.log
}

echo "== Hispa Ventanas: icono de red en la barra =="
# -- sin controlador de red: el icono, apagado (arena oscura, color 7) --
run r0 0
chk $T/r0.ppm px 626 4 7 "sin red: el icono de red, apagado (arena oscura)" \
  texto 676 5 8 1 fina "23:58" px 650 12 7 "el altavoz (sin HSPSND, apagado) pegado a la hora"
muestra r0 sinred
# -- con HSPRED cargado: el icono, encendido (crema, color 8) --
run r1 1
if grep -qiE "HSPRED .*packet driver|controlador de red" $T/r1.log; then
  ok "HSPRED se cargó (hay packet driver)"
else
  ko "HSPRED no se cargó en QEMU (no se puede probar el icono encendido)"
fi
chk $T/r1.ppm px 626 4 8 "con red: el icono de red, encendido (crema)" \
  px 650 12 7 "el altavoz sigue en su sitio"
muestra r1 conred

[ -n "${CAPTURAS:-}" ] && { for n in r0 r1; do [ -f $T/$n.ppm ] && convert $T/$n.ppm "$CAPTURAS/ventanas_red_$n.png"; done; }
rm -rf $T
exit $fail
