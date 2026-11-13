#!/bin/bash
# Pruebas de los mandos de juego (docs/MANDOS.md): HSPUSB (mandos USB),
# HSPJOY (INT 15h 84h, interfaz INT 2Fh D9h, mando a teclado) y MANDO.
#   hid        el analizador de descriptores de informe HID (el de HSPUSB) con
#              los descriptores de cinco mandos reales y dos hechos a mano, y
#              el formato de los mandos tipo Xbox 360 (HIDTEST)
#   juegos     puerto de juegos 201h (simulado: QEMU no lo tiene): 2 y 4
#              ejes, botones, calibración con MANDO /CALIBRAR y MANDO.INI
#   impresora  cable del puerto de impresora (simulado): NES, Super Nintendo,
#              Atari, Mega Drive y PlayStation, y mando a teclado
#   usb        un mando USB de verdad para HSPUSB: el dispositivo usb-redir de
#              QEMU con tests/tools/usbmando.py haciendo de DualShock 4 (con
#              su descriptor real), junto a un teclado y un ratón USB; INT
#              15h, MANDO /VER y las teclas del mando
#   x360       lo mismo con un mando tipo Xbox 360 (045E:028E) y /INFO
#   dos        dos mandos a la vez (uno detrás de un concentrador): jugadores
#              1 y 2
#   imagen     el disquete de distribución: HSPJOY en AUTOEXEC.BAT, MANDO y
#              MANDO.INI
# Sin segundo argumento se hacen todas.
# Game controller tests (USB pads through usb-redir, simulated ports).
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
MODE=${2:-todo}
B=$(dirname "$IMG")
fail=0
T=""
QP=""
MP=""

check() {
  if grep -qF -- "$1" $T/log.txt; then echo "  OK   $1"; else echo "  FAIL $1"; cfail=1; fi
}
nocheck() {
  if grep -qF -- "$1" $T/log.txt; then echo "  FAIL no debía salir: $1"; cfail=1; else echo "  OK   (no sale) $1"; fi
}
# cuántas veces sale un texto en la consola del invitado hasta ahora
veces() { grep -aoF -- "$1" $T/log 2>/dev/null | wc -l; }
# esperar a que un texto salga N veces (60 s como mucho)
espera() {  # texto [veces]
  local n=${2:-1} i=0
  while [ "$(veces "$1")" -lt "$n" ] && [ $i -lt 600 ]; do sleep 0.1; i=$((i + 1)); done
  [ $i -lt 600 ] || echo "  (no llega: $1)"
}
# escribir una orden y esperar a que vuelva el símbolo del sistema
orden() {
  local n=$(veces 'A:\>')
  printf '%s\r' "$1" >&3
  espera 'A:\>' $((n + 1))
}
tecla() { printf '%s' "$1" >&3; }
caso() {  # nombre [ficheros a la raíz...]: la imagen con los programas de prueba
  T=$(mktemp -d)
  cp "$IMG" $T/f.img
  mcopy -o -i $T/f.img $B/HIDTEST.COM $B/JOYTEST.COM $B/HSPJOYT.COM ::BIN/
  cfail=0
  echo "== Hispa-DOS mandos de juego ($1) =="
}
# un fichero de configuración en la raíz del disquete (CR LF)
ini() {  # nombre líneas...
  local f=$T/$1; shift
  printf '%s\r\n' "$@" > $f
  mcopy -o -i $T/f.img $f ::
}
arranca() {  # [opciones de QEMU]: la consola por el puerto serie
  mkfifo $T/in
  qemu-system-i386 -display none -serial stdio -monitor none \
      -drive file=$T/f.img,format=raw,if=floppy "$@" -boot a -no-reboot < $T/in > $T/log 2>&1 &
  QP=$!
  exec 3> $T/in
  espera 'A:\>'
}
mando() {  # socket perfil órdenes...: el mando USB de mentira
  USBMANDO_VE=$T/log python3 tests/tools/usbmando.py "$@" &
  MP="$MP $!"
}
fin() {
  exec 3>&-
  kill $QP 2>/dev/null
  wait $QP $MP 2>/dev/null
  MP=""
  tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 > $T/log.txt
  "$@"
  if [ $cfail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log.txt; fi
  [ $cfail -ne 0 ] && fail=1
  rm -rf $T
}
quiere() { [ "$MODE" = todo ] || [ "$MODE" = "$1" ]; }

# ---- hid: el analizador con mandos reales ----
c_hid() {
  check "DualShock 4: id=1 ejes=6 botones=14"
  check "  X =8/8 Y =16/8 X2=24/8 Y2=32/8 L =64/8 R =72/8 hat=40/4 b=44 45 46 47 48 49 50 51 52 53 54 55 56 57 - -"
  check "  estado: 0 128 128 255 64 255 cruz=4 botones=0012"
  check "DualSense: id=1 ejes=6 botones=15"
  check "  X =8/8 Y =16/8 X2=24/8 Y2=32/8 L =40/8 R =48/8 hat=64/4 b=68 69 70 71 72 73 74 75 76 77 78 79 80 81 82 -"
  check "  estado: 128 0 128 128 0 255 cruz=0 botones=0001"
  check "Switch Pro: id=48 ejes=4 botones=16"
  check "  X =24/16 Y =40/16 X2=56/16 Y2=72/16 hat=88/4 b=8 9 10 11 12 13 14 15 16 17 18 19 20 21 92 93"
  check "  estado: 0 255 128 128 0 0 cruz=8 botones=0005"
  check "Luna: id=1 ejes=6 botones=12"
  check "  X =32/8 Y =40/8 X2=48/8 Y2=56/8 L =64/8 R =72/8 hat=24/8 b=8 9 10 11 12 13 14 15 16 17 18 19 - - - -"
  check "  estado: 255 128 128 128 16 32 cruz=1 botones=0803"
  check "Stadia: id=3 ejes=6 botones=15"
  check "  X =32/8 Y =40/8 X2=48/8 Y2=56/8 L =64/8 R =72/8 hat=8/4 b=30 29 - 28 27 - 26 25 - - 22 21 20 24 23 -"
  check "  estado: 0 255 128 128 0 255 cruz=0 botones=0001"
  check "Generico: id=no ejes=2 botones=8"
  check "  X =0/8s Y =8/8s dpad=24 b=16 17 18 19 20 21 22 23 - - - - - - - -"
  check "  estado: 0 255 128 128 0 0 cruz=9 botones=0082"
  check "Vuelo: id=no ejes=4 botones=4"
  check "  X =0/10 Y =10/10 X2=24/8 Y2=32/8 hat=20/4 b=40 41 42 43 - - - - - - - - - - - -"
  check "  estado: 255 0 0 255 0 0 cruz=2 botones=0004"
  check "Xbox 360: id=0 ejes=6 botones=11"
  check "  X =48/16s Y =64/16si X2=80/16s Y2=96/16si L =32/8 R =40/8 dpad=16 b=28 29 30 31 24 25 21 20 22 23 26 - - - - -"
  check "  estado: 0 0 128 127 128 0 cruz=1 botones=0021"
  check "Xbox 360: informe de LED descartado"
  check "  estado: 0 0 128 127 128 0 cruz=0 botones=0080"
  check "Teclado: no es un mando"
  check "Raton: no es un mando"
  check "Descriptor cortado: sin cuelgue"
}
if quiere hid; then
  caso hid
  arranca
  orden 'BIN\HIDTEST'
  fin c_hid
fi

# ---- juegos: el puerto 201h (simulado) ----
c_juegos() {
  # sin mandos no se queda residente; con /R sí
  check "HSPJOY 0.1 - mandos de juego de Hispa-DOS (PRUEBA: puertos simulados)"
  check "No se ha encontrado ningún mando de juego."
  check "INT15: sin mando (CF)"
  check "HSPJOY no está cargado. Cárguelo antes con la orden HSPJOY"
  check "HSPJOY queda cargado (/R)."
  check "  (no hay ningún mando)"
  # dos ejes y cuatro botones, centrado al cargar
  check "HSPJOY ya estaba cargado: configuración actualizada."
  check "Mando 3: puerto de juegos (2 ejes, 4 botones)"
  check "INT15: botones=F0 X1=193 Y1=193 X2=0 Y2=0"
  check "Clasico: botones=F3 X1=128 Y1=128 X2=128 Y2=128"
  check "Registros conservados"
  check "  3  puerto de juegos, 2 ejes y 4 botones  <- jugador 1"
  # palanca a la izquierda y abajo (el eje Y no contesta: tope), botón 2
  check "INT15: botones=D0 X1=8 Y1=383 X2=0 Y2=0"
  check "Mando 3: tipo=3 2/4 ejes=5 255 128 128 0 0 cruz=0 botones=0002"
  # cuatro ejes: dos mandos de dos botones
  check "Mando 3: puerto de juegos (2 ejes, 2 botones)"
  check "Mando 4: puerto de juegos (2 ejes, 2 botones)"
  check "Mando 3: tipo=3 2/2 ejes=128 128 128 128 0 0 cruz=0 botones=0001"
  check "Mando 4: tipo=3 2/2 ejes=128 128 128 128 0 0 cruz=0 botones=0002"
  check "INT15: botones=F0 X1=383 Y1=1 X2=229 Y2=193"
  check "Mando 4: tipo=3 2/2 ejes=152 128 128 128 0 0 cruz=0 botones=0000"
  check "  4  puerto de juegos, 2 ejes y 2 botones  <- jugador 2"
  # MANDO /CALIBRAR con la palanca dando vueltas (40 a 560, centro 300):
  # mínimo y máximo con su margen, guardados en MANDO.INI sin perder lo demás
  check "1. Suelte la palanca, que quede en el centro, y pulse un botón del"
  check "Calibrado."
  check "Guardado en A:\\HISPADOS\\MANDO.INI"
  check "; MANDO.INI de Hispa-DOS: los mandos de juego (lo lee HSPJOY al cargarse)"
  # el mínimo y el máximo dependen de cuántas vueltas de la palanca simulada
  # alcanza a ver la calibración (según lo que tarden las teclas): margen
  c=$(grep -a -m1 '^calibrar=' $T/log.txt | cut -d= -f2)
  IFS=, read -r a b c2 d e f _ <<< "$c"
  if [ -n "$a" ] && [ "$a" -le 90 ] && [ "$b" = 300 ] && [ "$c2" -ge 510 ] && [ "$d" -le 90 ] && [ "$e" = 300 ] && [ "$f" -ge 510 ]; then
    echo "  OK   calibrar=$c (mínimo <= 90, centro 300, máximo >= 510)"
  else echo "  FAIL calibrar=$c"; cfail=1; fi
  check "Mando 3: tipo=3 2/4 ejes=0 255 128 128 0 0 cruz=0 botones=0000"
  # con la calibración nueva, el eje Y en 360 da unos 191 (según el máximo visto)
  if grep -a -q -E '^Mando 3: tipo=3 2/4 ejes=128 1(8[5-9]|9[0-9]) 128 128 0 0 cruz=0 botones=0000' $T/log.txt; then
    echo "  OK   Mando 3: con la calibración nueva, Y de 185 a 199"
  else echo "  FAIL Mando 3: con la calibración nueva (Y fuera de 185-199)"; cfail=1; fi
  check "Calibración cancelada."
  n=$(grep -c "^calibrar=" $T/log.txt)
  if [ "$n" = 2 ]; then echo "  OK   una sola línea calibrar= en MANDO.INI"; else echo "  FAIL líneas calibrar= vistas: $n (2 = una por cada MOSTRAR)"; cfail=1; fi
  # MANDO /PROBAR y /VER
  check "Juego: X=  1 Y=383 Z=  0 R=  0  botones=...."
  check "Mando 3 X=  0 Y=255  cruceta=....  botones=...."
  # descargar
  check "HSPJOY descargado."
  check "HSPJOY no está cargado."
  n=$(grep -c "INT15: sin mando (CF)" $T/log.txt)
  if [ "$n" -ge 3 ]; then echo "  OK   tras descargarlo INT 15h vuelve a la BIOS"; else echo "  FAIL tras descargarlo INT 15h vuelve a la BIOS ($n)"; cfail=1; fi
}
if quiere juegos; then
  caso juegos
  arranca
  orden 'BIN\HSPJOYT'
  orden 'BIN\JOYTEST'
  orden 'MANDO'
  orden 'BIN\HSPJOYT /R'
  orden 'MANDO'
  orden 'BIN\JOYTEST S 0 2C 01 2C 01'
  orden 'BIN\HSPJOYT'
  orden 'BIN\JOYTEST'
  orden 'BIN\JOYTEST R'
  orden 'MANDO'
  orden 'BIN\JOYTEST S 0 30 00 33 02 FF FF FF FF D0'
  orden 'BIN\JOYTEST S 0 2C 01 2C 01 2C 01 90 01 60'
  orden 'BIN\HSPJOYT'
  orden 'BIN\JOYTEST'
  orden 'MANDO'
  orden 'BIN\JOYTEST S 0 FF FF 25 00 5E 01 90 01 F0'
  # calibrar: de vuelta a dos ejes; el simulador mueve la palanca a los 5 s
  orden 'BIN\JOYTEST S 0 2C 01 2C 01 FF FF FF FF F0'
  orden 'BIN\HSPJOYT'
  orden 'BIN\JOYTEST S 2B 01'
  printf 'MANDO /CALIBRAR\r' >&3
  espera 'pulse un bot'
  sleep 1.5
  tecla ' '
  espera 'Lleve la palanca'
  sleep 9.5
  n=$(veces 'A:\>')
  tecla ' '
  espera 'A:\>' $((n + 1))
  orden 'MOSTRAR A:\HISPADOS\MANDO.INI'
  orden 'BIN\HSPJOYT /I:A:\HISPADOS\MANDO.INI'
  orden 'BIN\JOYTEST S 0 38 00 20 02'
  orden 'BIN\JOYTEST S 0 2C 01 A6 01'
  # otra vez, y se deja con Esc: la calibración no cambia
  printf 'MANDO /CALIBRAR\r' >&3
  espera 'pulse un bot' 3
  sleep 1
  n=$(veces 'A:\>')
  printf '\033' >&3
  espera 'A:\>' $((n + 1))
  orden 'MOSTRAR A:\HISPADOS\MANDO.INI'
  orden 'BIN\JOYTEST S 0 38 00 20 02'
  printf 'MANDO /PROBAR\r' >&3
  espera 'Juego:'
  sleep 0.5
  n=$(veces 'A:\>')
  tecla ' '
  espera 'A:\>' $((n + 1))
  printf 'MANDO /VER\r' >&3
  espera 'Mando 3 X='
  sleep 0.5
  n=$(veces 'A:\>')
  tecla ' '
  espera 'A:\>' $((n + 1))
  orden 'BIN\HSPJOYT /D'
  orden 'BIN\JOYTEST'
  orden 'MANDO'
  orden 'BIN\HSPJOYT /D'
  fin c_juegos
fi

# ---- impresora: los mandos del cable (simulados) ----
c_impresora() {
  # NES: dos mandos; el 1 con A, Start y derecha, el 2 con B y arriba
  check "Mando 5: NES (impresora) (0 ejes, 4 botones)"
  check "Mando 6: NES (impresora) (0 ejes, 4 botones)"
  check "INT15: botones=60 X1=383 Y1=193 X2=193 Y2=1"
  check "Mando 5: tipo=6 0/4 ejes=128 128 128 128 0 0 cruz=8 botones=0009"
  check "Mando 6: tipo=6 0/4 ejes=128 128 128 128 0 0 cruz=1 botones=0002"
  check "  5  NES (impresora), 0 ejes y 4 botones  <- jugador 1"
  # sin mando en el cable (la línea a 0): nada pulsado
  check "Mando 5: tipo=6 0/4 ejes=128 128 128 128 0 0 cruz=0 botones=0000"
  # Super Nintendo: B, izquierda, X y R
  check "Mando 5: Super Nintendo (impresora) (0 ejes, 8 botones)"
  check "Mando 5: tipo=7 0/8 ejes=128 128 128 128 0 0 cruz=4 botones=0029"
  check "INT15: botones=60 X1=1 Y1=193 X2=0 Y2=0"
  # Atari: arriba, izquierda y el botón
  check "Mando 5: joystick Atari/Commodore (impresora) (0 ejes, 2 botones)"
  check "Mando 5: tipo=4 0/2 ejes=128 128 128 128 0 0 cruz=5 botones=0001"
  # Mega Drive: abajo, izquierda, C, A y Start; y un joystick Atari en ese
  # cable (no baja izquierda y derecha con la selección): solo sus botones
  check "MANDO.INI: no se entiende la línea 4"
  check "Mando 5: Mega Drive (impresora) (0 ejes, 4 botones)"
  check "Mando 5: tipo=5 0/4 ejes=128 128 128 128 0 0 cruz=6 botones=000E"
  check "Mando 5: tipo=5 0/4 ejes=128 128 128 128 0 0 cruz=6 botones=0002"
  # PlayStation: no hay mando hasta que contesta 5Ah; las órdenes 01h 42h
  check "PSX ordenes=01 42"
  check "Mando 5: tipo=8 0/12 ejes=128 128 128 128 0 0 cruz=0 botones=0000"
  check "Mando 5: tipo=8 4/12 ejes=48 240 16 32 0 0 cruz=1 botones=0001"
  check "Mando 6: tipo=8 0/12 ejes=128 128 128 128 0 0 cruz=4 botones=0040"
  check "INT15: botones=E0 X1=73 Y1=1 X2=1 Y2=193"
  check "Mando 5: tipo=8 0/12 ejes=128 128 128 128 0 0 cruz=0 botones=0100"
  # mando a teclado: el mando 1 teclea VER e Intro; el 2, DIR e Intro
  check "Mando a teclado activado (MANDO.INI)."
  check "Mando a teclado: jugador 1 jugador 2"
  check "Hispa-DOS versión 0.9 (compatible con MS-DOS 6.22)"
  check " Directorio de A:\\"
  nocheck "A:\\>VER"
  nocheck "A:\\>DIR"
}
if quiere impresora; then
  caso impresora
  ini NES.INI '; dos mandos de NES' '[mandos]' 'lpt=nes' 'lpt_mandos=2' 'juegos=no'
  ini SNES.INI '[mandos]' '  lpt = snes' 'juegos=no'
  ini ATARI.INI '[mandos]' 'lpt=atari' 'juegos=no'
  ini MEGA.INI '[mandos]' 'lpt=megadrive' 'juegos=no' 'cosa=rara'
  ini PSX.INI '[mandos]' 'lpt=psx' 'lpt_mandos=2' 'juegos=no'
  ini TECLAS.INI '[mandos]' 'lpt=nes' 'lpt_mandos=2' 'juegos=no' '[teclas1]' 'boton1=V' 'boton2=e' \
      'boton3 = R' 'boton4=INTRO' 'arriba=ARRIBA' '[teclas2]' 'boton1=D' 'boton2=I' 'boton3=R' 'boton4=intro'
  arranca
  orden 'BIN\HSPJOYT /I:A:\NES.INI'
  orden 'BIN\JOYTEST S 9 06 89 00 12 00'
  orden 'MANDO'
  orden 'BIN\JOYTEST S 9 06 FF 00 00 00'
  orden 'BIN\HSPJOYT /I:A:\SNES.INI'
  orden 'BIN\JOYTEST S 9 07 41 0A'
  orden 'BIN\HSPJOYT /I:A:\ATARI.INI'
  orden 'BIN\JOYTEST S 9 04 00 00 00 00 15'
  orden 'BIN\HSPJOYT /I:A:\MEGA.INI'
  orden 'BIN\JOYTEST S 9 05 00 00 00 00 26 30'
  orden 'BIN\JOYTEST S 9 04 00 00 00 00 26 30'
  orden 'BIN\JOYTEST S 9 08'
  orden 'BIN\HSPJOYT /I:A:\PSX.INI'
  orden 'BIN\JOYTEST'
  orden 'BIN\JOYTEST S 10 FF 73 5A EF BF 10 20 30 F0 FF 41 5A 7F FE'
  orden 'BIN\JOYTEST S 10 FF 41 5A FE FF 80 80 80 80 FF FF FF FF FF'
  # las teclas
  orden 'BIN\JOYTEST S 9 06 00 00 00 00'
  orden 'BIN\HSPJOYT /I:A:\TECLAS.INI'
  orden 'MANDO'
  n=$(veces 'A:\>')
  printf 'BIN\\JOYTEST P A 01 02 04 08\r' >&3
  espera 'A:\>' $((n + 2))
  n=$(veces 'A:\>')
  printf 'BIN\\JOYTEST P C 01 02 04 08\r' >&3
  espera 'A:\>' $((n + 2))
  fin c_impresora
fi

# informes del DualShock 4 (USB): 01, X, Y, X der., Y der., cruceta y botones...
DS=0180808080
# ---- usb: un DualShock 4 con teclado y ratón USB ----
c_usb() {
  check "Teclado USB preparado."
  check "Ratón USB preparado."
  check "Mando USB preparado: 054C:09CC, 6 ejes y 14 botones."
  check "Mando 1: USB 054C:09CC (6 ejes, 14 botones)"
  check "Mando a teclado activado (MANDO.INI)."
  check "  1  USB 054C:09CC, 6 ejes y 14 botones  <- jugador 1"
  check "Registros conservados"
  # el mando teclea VER e Intro (cruceta arriba, Cuadrado, Cruz, Círculo)
  check "Hispa-DOS versión 0.9 (compatible con MS-DOS 6.22)"
  nocheck "A:\\>VER"
  # y el teclado USB sigue yendo
  check "A:\\>ECHO Teclado"
  # sin teclas: lo que ve un juego con cada informe
  check "HSPJOY ya estaba cargado: configuración actualizada."
  check "INT15: botones=F0 X1=193 Y1=193 X2=193 Y2=193"
  check "INT15: botones=D0 X1=1 Y1=193 X2=193 Y2=193"
  check "INT15: botones=E0 X1=193 Y1=383 X2=193 Y2=193"
  check "INT15: botones=30 X1=193 Y1=193 X2=383 Y2=1"
  check "Mando 1: tipo=1 6/14 ejes=128 128 255 0 64 255 cruz=0 botones=000C"
  # MANDO /VER
  check "Mando 1 X=  0 Y=128 Z=128 R=255 L= 64 D=255  cruceta=..I.  botones=.2..5........."
  # el descriptor de informe (507 bytes) se ha pedido entero
  if grep -q "control 81 06 2200 0003 507 -> 507" $T/mlog; then echo "  OK   descriptor de informe HID de 507 bytes leído"; else echo "  FAIL descriptor de informe HID de 507 bytes leído"; cfail=1; fi
}
if quiere usb; then
  caso usb
  printf 'DEVICE=A:\\HISPADOS\\HSPUSB.SYS /F\r\n' > $T/CONFIG.SYS
  mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
  ini TECLAS.INI '[mandos]' 'juegos=no' '[teclas1]' 'arriba=V' 'boton1=e' 'boton2 = R' 'boton3=INTRO' 'abajo=ABAJO'
  ini NADA.INI '[mandos]' 'juegos=no'
  USBMANDO_LOG=$T/mlog mando $T/redir ds4 \
      espera "Registros conservados" wait 1.5 \
      rep ${DS}000000 wait .3 rep ${DS}080000 wait .3 rep ${DS}180000 wait .3 rep ${DS}080000 wait .3 \
      rep ${DS}280000 wait .3 rep ${DS}080000 wait .3 rep ${DS}480000 wait .3 rep ${DS}080000 \
      espera "Mirando" wait .6 rep 0100808080280000 wait .8 rep 0180808080140000 wait .8 \
      rep 018080ff00c800004000 wait .8 rep 018080ff00c8000040ff \
      espera "Una tecla para terminar" wait .6 rep 01008080ff26010040ff
  Q="python3 tests/tools/qmp.py $T/qmp"
  arranca -qmp unix:$T/qmp,server,nowait -vga none -device VGA,id=vga0 -device qemu-xhci,id=xhci \
      -device usb-kbd,bus=xhci.0,id=ukbd,display=vga0 -device usb-mouse,bus=xhci.0,id=umouse \
      -chardev socket,id=rd,path=$T/redir,server=on,wait=off -device usb-redir,chardev=rd,bus=xhci.0
  orden 'HSPJOY /I:A:\TECLAS.INI'
  orden 'MANDO'
  n=$(veces 'A:\>')
  printf 'BIN\\JOYTEST R\r' >&3
  espera 'A:\>' $((n + 2))            # (el segundo, tras el VER que teclea el mando)
  sleep 0.5
  n=$(veces 'A:\>')
  $Q type vga0 "ECHO Teclado" key vga0 ret
  espera 'A:\>' $((n + 1))
  orden 'HSPJOY /I:A:\NADA.INI'
  orden 'BIN\JOYTEST W 80'
  orden 'BIN\JOYTEST'
  printf 'MANDO /VER\r' >&3
  espera 'cruceta=..I.'
  sleep 0.5
  n=$(veces 'A:\>')
  tecla ' '
  espera 'A:\>' $((n + 1))
  fin c_usb
fi

# ---- x360: un mando tipo Xbox 360 ----
c_x360() {
  check "FF/5D/01"
  check "-> mando de juego"
  check "Mando USB preparado: 045E:028E, 6 ejes y 11 botones (tipo Xbox 360)."
  check "Mando 1: USB tipo Xbox 360 045E:028E (6 ejes, 11 botones)"
  check "  1  USB tipo Xbox 360 045E:028E, 6 ejes y 11 botones  <- jugador 1"
  check "INT15: botones=F0 X1=193 Y1=193 X2=193 Y2=193"
  # cruceta arriba y A
  check "INT15: botones=E0 X1=193 Y1=1 X2=193 Y2=191"
  # un informe que no es de entrada (01h: los LED) no cambia nada
  # palanca izquierda a la izquierda y arriba del todo, B y X, gatillo derecho
  check "INT15: botones=90 X1=1 Y1=1 X2=193 Y2=191"
  check "Mando 1: tipo=2 6/11 ejes=0 0 128 127 0 255 cruz=0 botones=0006"
}
if quiere x360; then
  caso x360
  printf 'DEVICE=A:\\HISPADOS\\HSPUSB.SYS /F /INFO\r\n' > $T/CONFIG.SYS
  mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
  ini NADA.INI '[mandos]' 'juegos=no'
  mando $T/redir x360 \
      espera "Mirando" wait .6 rep 0014011000000000000000000000000000000000 wait .8 \
      rep 01030e wait .2 rep 0014006000ff0080ff7f00000000000000000000 wait .8 \
      rep 0014006000ff0080ff7f00000000000000000000
  arranca -device qemu-xhci,id=xhci \
      -chardev socket,id=rd,path=$T/redir,server=on,wait=off -device usb-redir,chardev=rd,bus=xhci.0
  orden 'HSPJOY /I:A:\NADA.INI'
  orden 'MANDO'
  orden 'BIN\JOYTEST W 60'
  orden 'BIN\JOYTEST'
  fin c_x360
fi

# ---- dos: dos mandos, uno detrás de un concentrador ----
c_dos() {
  check "Mando USB preparado: 054C:09CC, 6 ejes y 14 botones."
  check "Mando USB preparado: 045E:028E, 6 ejes y 11 botones (tipo Xbox 360)."
  check "  1  USB 054C:09CC, 6 ejes y 14 botones  <- jugador 1"
  check "  2  USB tipo Xbox 360 045E:028E, 6 ejes y 11 botones  <- jugador 2"
  check "INT15: botones=F0 X1=193 Y1=193 X2=193 Y2=193"
  # mando 1: izquierda y Cruz (botón 2); después el 2: abajo y A (botón 1)
  check "INT15: botones=D0 X1=1 Y1=193 X2=193 Y2=193"
  check "INT15: botones=90 X1=1 Y1=193 X2=193 Y2=383"
  check "Mando 1: tipo=1 6/14 ejes=0 128 128 128 0 0 cruz=0 botones=0002"
  check "Mando 2: tipo=2 6/11 ejes=128 127 128 127 0 0 cruz=2 botones=0001"
  # jugador1=usb2: se cambian (el 2 automático es el otro)
  check "  2  USB tipo Xbox 360 045E:028E, 6 ejes y 11 botones  <- jugador 1"
  check "  1  USB 054C:09CC, 6 ejes y 14 botones  <- jugador 2"
  check "INT15: botones=60 X1=193 Y1=383 X2=1 Y2=193"
  # y con jugador2=ninguno, la segunda palanca y los botones 3 y 4 del 1
  if grep -A1 -xF "  1  USB 054C:09CC, 6 ejes y 14 botones" $T/log.txt | grep -qF "<- jugador 1"; then
    echo "  OK   jugador2=ninguno: el otro mando se queda sin jugador"
  else echo "  FAIL jugador2=ninguno: el otro mando se queda sin jugador"; cfail=1; fi
  check "INT15: botones=E0 X1=193 Y1=383 X2=193 Y2=191"
}
if quiere dos; then
  caso dos
  printf 'DEVICE=A:\\HISPADOS\\HSPUSB.SYS /F\r\n' > $T/CONFIG.SYS
  mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
  ini NADA.INI '[mandos]' 'juegos=no'
  ini CAMBIO.INI '[mandos]' 'juegos=no' 'jugador1=usb2'
  ini SOLO.INI '[mandos]' 'juegos=no' 'jugador1=usb2' 'jugador2=ninguno'
  mando $T/redir ds4 espera "Mirando" wait .6 rep 0100808080280000
  mando $T/redir2 x360 espera "Mirando" wait 1.6 rep 0014021000000000000000000000000000000000
  arranca -device qemu-xhci,id=xhci \
      -chardev socket,id=rd,path=$T/redir,server=on,wait=off -device usb-redir,chardev=rd,bus=xhci.0,port=1 \
      -device usb-hub,bus=xhci.0,port=2,id=hub1 \
      -chardev socket,id=rd2,path=$T/redir2,server=on,wait=off -device usb-redir,chardev=rd2,bus=xhci.0,port=2.1
  orden 'HSPJOY /I:A:\NADA.INI'
  orden 'MANDO'
  orden 'BIN\JOYTEST W 70'
  orden 'BIN\JOYTEST'
  orden 'HSPJOY /I:A:\CAMBIO.INI'
  orden 'MANDO'
  orden 'BIN\JOYTEST'
  orden 'HSPJOY /I:A:\SOLO.INI'
  orden 'MANDO'
  orden 'BIN\JOYTEST'
  fin c_dos
fi

# ---- imagen: el disquete de distribución ----
c_imagen() {
  check "HSPJOY 0.1 - mandos de juego de Hispa-DOS"
  check "No se ha encontrado ningún mando de juego."
  check "HSPJOY no está cargado. Cárguelo antes con la orden HSPJOY"
  check "  MANDO /CALIBRAR   calibra el mando del puerto de juegos y lo guarda"
  check "Uso: HSPJOY [/R] [/D] [/I:fichero]"
  check "; MANDO.INI de Hispa-DOS: los mandos de juego (lo lee HSPJOY al cargarse)"
  check "; y a cada botón, una tecla. Teclas: letras y cifras, ARRIBA ABAJO IZQUIERDA"
  check "HSPJOY queda cargado (/R)."
  n=$(grep -c "^  HSPJOY " $T/log.txt)
  if [ "$n" = 1 ]; then echo "  OK   HSPJOY solo ocupa memoria con /R (MEM /C)"; else echo "  FAIL HSPJOY solo ocupa memoria con /R (MEM /C): $n"; cfail=1; fi
  check "HSPJOY descargado."
  check "REVISAR (CHKDSK), MANDO (mandos de juego),"
  if [ "$(grep -c 'HSPJOY  COM' $B/hispados-usb.img)" -ge 1 ] && [ "$(grep -c 'MANDO   INI' $B/hispados-usb.img)" -ge 1 ] \
     && [ "$(grep -c 'MANDO   COM' $B/hispados-usb.img)" -ge 1 ]; then
    echo "  OK   HSPJOY.COM, MANDO.COM y MANDO.INI en la imagen USB"
  else echo "  FAIL HSPJOY.COM, MANDO.COM y MANDO.INI en la imagen USB"; cfail=1; fi
  if mtype -i $B/hispados-disquete.img ::AUTOEXEC.BAT | grep -q 'A:\\HISPADOS\\HSPJOY'; then
    echo "  OK   HSPJOY en el AUTOEXEC.BAT del disquete"
  else echo "  FAIL HSPJOY en el AUTOEXEC.BAT del disquete"; cfail=1; fi
}
if quiere imagen; then
  if [ ! -f $B/hispados-disquete.img ] || [ ! -f $B/hispados-usb.img ]; then
    echo "== Hispa-DOS mandos de juego (imagen) =="
    echo "  FAIL faltan las imágenes de distribución (make usb)"
    fail=1
  else
    T=$(mktemp -d)
    cp $B/hispados-disquete.img $T/f.img
    # (el menú de arranque de CONFIG.SYS, sin espera: opción «Normal»)
    tests/tools/sinmenu.sh $T/f.img
    cfail=0
    echo "== Hispa-DOS mandos de juego (imagen) =="
    arranca
    orden 'MEM /C'
    orden 'MANDO'
    orden 'MANDO /?'
    orden 'HSPJOY /?'
    orden 'MOSTRAR A:\HISPADOS\MANDO.INI'
    orden 'HSPJOY /R'
    orden 'MEM /C'
    orden 'HSPJOY /D'
    orden 'AYUDA'
    fin c_imagen
  fi
fi
exit $fail
