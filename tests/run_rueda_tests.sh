#!/bin/bash
# [RUEDA] Prueba de la rueda del ratón, de punta a punta: el ratón PS/2 con
# rueda (IntelliMouse) de QEMU y su ratón USB (usb-mouse, que manda la rueda
# en el 4.º byte del informe), los dos en la misma máquina: el monitor de QEMU
# mueve el que se elija con «mouse_set» (mouse_move 0 0 N: la rueda; -1 hacia
# abajo). Sesiones:
#  1. DOS: HSPUSB /F y HSPRATON /DIRECTO; MOUSETST cuenta los pasos (función
#     03h, BH) primero con el ratón PS/2 y después con el USB; la función 11h
#     dice que hay rueda. 1b: el ratón USB en una UHCI. 1c: HSPRATON sin
#     opciones (como en el AUTOEXEC.BAT de las imágenes): pregunta por la
#     BIOS si el ratón PS/2 es un IntelliMouse (C202h 200, 100, 80 y C204h;
#     la SeaBIOS de QEMU contesta AAh, que no es un identificador: se prueba
#     la vía directa, que lo pregunta ella) y pasa a la vía directa: la rueda
#     del PS/2 y la del USB llegan; con /BIOS, por la BIOS y sin rueda en el
#     PS/2 (el USB sigue con la suya).
#  2. DOS: el touchpad Synaptics en modo absoluto, sin touchpad (HSPRATON
#     /DIRECTO /SYNPRUEBA; SYNTEST mete paquetes de 6 bytes por el 8042): el
#     dedo en el centro mueve el puntero, por el borde derecho da pasos de
#     rueda (y no mueve el puntero), el toque es un clic; HSPRATON /INFO lo
#     dice.
#  3. Hispa Ventanas: WINRUEDA.EXE (tests/progs/winrueda.c), un programa de
#     Windows: WM_MOUSEWHEEL a la ventana de debajo del cursor, con el delta
#     de Windows (-120 por paso hacia abajo), el punto en la pantalla, de la
#     hija a la madre por DefWindowProcA; su LISTBOX (una lista de Hispa
#     Ventanas) se desplaza tres líneas por paso; SystemParametersInfoA da 3
#     líneas. Con el ratón PS/2 y con el USB.
#  4. El Navegador (NAVEGA.EXE) con una página larga del servidor de pruebas
#     (tests/tools/nav_servidor.py, /larga): la rueda desplaza la página tres
#     filas por paso (igual que tres flechas) y, con una lista desplegable
#     abierta, sus opciones. Con el ratón PS/2 y con el USB, y con HSPRATON
#     sin opciones (el de la imagen de Hispa-486).
#  (GUARDAR=carpeta deja las capturas; SOLO=1,2,3,4 elige sesiones.)
# Mouse wheel test: PS/2 IntelliMouse and USB mouse, DOS, Win32 programs and
# the browser.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
B=build
make -s $B/MOUSETST.COM $B/SYNTEST.COM $B/HSPRATON.COM $B/HSPUSB.SYS > /dev/null 2>&1 || {
  echo "== la rueda del ratón: no se puede compilar =="; exit 1; }
CON32=1
make -s $B/WINRUEDA.EXE $B/NAVEGA.EXE $B/HSPJS.DLL $B/HSPTLS.DLL $B/WSOCK32.DLL $B/MSVCRT.DLL $B/HW32.COM > /dev/null 2>&1 || CON32=0
T=$(mktemp -d)
fail=0
export MTOOLS_SKIP_CHECK=1
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() { if grep -qF -- "$1" $T/log.txt; then ok "$1"; else ko "$1"; fi; }
nocheck() { if grep -qF -- "$1" $T/log.txt; then ko "(no debía salir) $1"; else ok "sin «$1»"; fi; }
chk() { python3 -I tests/tools/ventanas_check.py "$@" || fail=1; }
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; sleep "${2:-0.3}"; }
tecla() { local k; for k in "$@"; do mon "sendkey $k" 0.3; done; }
shot() { sleep 0.5; mon "screendump $T/$1.ppm" 0.6; }
send() { printf '%s\r' "$1"; sleep "${2:-1.5}"; }
quiere() { [ -z "${SOLO:-}" ] || [[ ",$SOLO," == *",$1,"* ]]; }
# el ratón que mueve el monitor: ps2 o usb (por su número en «info mice»)
raton() {
  local pat n
  [ "$1" = usb ] && pat="HID Mouse" || pat="PS/2 Mouse"
  n=$(printf 'info mice\n' | socat - UNIX-CONNECT:$T/mon 2> /dev/null | tr -d '\r' | grep -a "$pat" | grep -ao '#[0-9]*' | tr -d '#' | head -1)
  [ -n "$n" ] && mon "mouse_set $n" 0.5
  echo "$1 $n" >> $T/ratones
}
# N pasos de la rueda (positivo: hacia abajo, hacia el usuario)
rueda() { local i d=-1; [ $1 -lt 0 ] && d=1; for ((i=0; i<${1#-}; i++)); do mon "mouse_move 0 0 $d" 0.35; done; sleep 0.4; }
RX=400; RY=300
ir() {
  local dx=$(( $1 - RX )) dy=$(( $2 - RY )) sx sy
  while [ $dx -ne 0 ] || [ $dy -ne 0 ]; do
    sx=$dx; [ $sx -gt 60 ] && sx=60; [ $sx -lt -60 ] && sx=-60
    sy=$dy; [ $sy -gt 60 ] && sy=60; [ $sy -lt -60 ] && sy=-60
    mon "mouse_move $sx $sy" 0.08
    dx=$(( dx - sx )); dy=$(( dy - sy ))
  done
  RX=$1; RY=$2; sleep 0.3
}
clic() { ir $1 $2; mon "mouse_button 1" 0.25; mon "mouse_button 0" 0.5; }
USB="-device qemu-xhci,id=hc -device usb-mouse,bus=hc.0,id=umouse"
qemu() {   # qemu SEGUNDOS [opciones...]
  local tmo=$1; shift
  rm -f $T/mon
  timeout $tmo qemu-system-i386 ${QEMU_PANTALLA--vga std -global VGA.xres=800 -global VGA.yres=600} -display none -serial stdio -monitor unix:$T/mon,server,nowait \
    -parallel none -m 16 -drive file=$T/f.img,format=raw,if=floppy,index=0 -boot a -no-reboot "$@" > $T/raw 2>&1
  tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 -c > $T/log.txt
}
guarda() { [ -n "${GUARDAR:-}" ] && { mkdir -p "$GUARDAR"; cp $T/*.ppm "$GUARDAR"/ 2> /dev/null; cp $T/log.txt "$GUARDAR/log_$1.txt"; }; true; }
config() { printf "$1" > $T/CONFIG.SYS; mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS; }

echo "== La rueda del ratón: PS/2 (IntelliMouse), USB, touchpad, Windows y el Navegador =="

# ---- 1. DOS: PS/2 y USB ----
if quiere 1; then
  echo "-- 1. DOS: MOUSETST con la rueda del ratón PS/2 y con la del USB --"
  cp "$IMG" $T/f.img
  config 'DEVICE=A:\\HISPADOS\\HSPUSB.SYS /F\r\n'
  { sleep 9; send 'A:\HISPADOS\HSPRATON /DIRECTO' 3
    send 'BIN\MOUSETST' 2; raton ps2; rueda 3; rueda -1; tecla ret; sleep 1.5
    send 'BIN\MOUSETST' 2; raton usb; rueda -2; rueda 1; rueda -1; tecla ret; sleep 1.5
    mon quit; } | qemu 60 $USB
  check "Ratón USB preparado."
  check "Vía directa (8042, IRQ 12). Ratón PS/2 con rueda (IntelliMouse) preparado."
  check "Rueda: sí"
  n=$(grep -c "^Pasos de la rueda: 2$" $T/log.txt)
  [ "$n" -eq 1 ] && ok "PS/2: 3 pasos hacia abajo y 1 hacia arriba = 2" || ko "PS/2: pasos de la rueda ($(grep '^Pasos de la rueda' $T/log.txt | head -1))"
  n=$(grep -c "^Pasos de la rueda: -2$" $T/log.txt)
  [ "$n" -eq 1 ] && ok "USB: 3 pasos hacia arriba y 1 hacia abajo = -2" || ko "USB: pasos de la rueda ($(grep '^Pasos de la rueda' $T/log.txt | tail -1))"
  check "Rueda: -1"
  check "Rueda: 1"
  guarda dos
  if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/HSPRATON/,$p' $T/log.txt | tail -40; cat $T/ratones; fi
fi

# ---- 1b. lo mismo con un ratón USB en una UHCI (USB 1.1) ----
if quiere 1; then
  echo "-- 1b. DOS: la rueda del ratón USB en una controladora UHCI --"
  f0=$fail
  cp "$IMG" $T/f.img
  config 'DEVICE=A:\\HISPADOS\\HSPUSB.SYS /F\r\n'
  { sleep 9; send 'A:\HISPADOS\HSPRATON' 3
    send 'BIN\MOUSETST' 2; raton usb; rueda 2; rueda -1; rueda 2; tecla ret; sleep 1.5
    mon quit; } | qemu 45 -device piix3-usb-uhci,id=hc -device usb-mouse,bus=hc.0
  check "Ratón USB preparado."
  n=$(grep -c "^Pasos de la rueda: 3$" $T/log.txt)
  [ "$n" -eq 1 ] && ok "UHCI: 4 pasos hacia abajo y 1 hacia arriba = 3" || ko "UHCI: pasos de la rueda ($(grep '^Pasos de la rueda' $T/log.txt))"
  guarda uhci
  if [ $fail -ne $f0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/HSPRATON/,$p' $T/log.txt | tail -20; cat $T/ratones; fi
fi

# ---- 1c. HSPRATON sin opciones: la BIOS, y por la rueda, la vía directa ----
if quiere 1; then
  echo "-- 1c. DOS: HSPRATON sin opciones con un IntelliMouse: la vía directa, con la rueda --"
  f0=$fail
  cp "$IMG" $T/f.img
  config 'DEVICE=A:\\HISPADOS\\HSPUSB.SYS /F\r\n'
  { sleep 9; send 'A:\HISPADOS\HSPRATON' 3
    send 'BIN\MOUSETST' 2; raton ps2; rueda 3; rueda -1; tecla ret; sleep 1.5
    send 'BIN\MOUSETST' 2; raton usb; rueda -2; rueda 1; rueda -1; tecla ret; sleep 1.5
    send 'A:\HISPADOS\HSPRATON /INFO' 3
    mon quit; } | qemu 60 $USB
  check "Vía directa (8042, IRQ 12). Ratón PS/2 con rueda (IntelliMouse) preparado."
  n=$(grep -c "^Pasos de la rueda: 2$" $T/log.txt)
  [ "$n" -eq 1 ] && ok "sin opciones, PS/2: 3 pasos hacia abajo y 1 hacia arriba = 2" || ko "sin opciones, PS/2: pasos de la rueda ($(grep '^Pasos de la rueda' $T/log.txt | head -1))"
  n=$(grep -c "^Pasos de la rueda: -2$" $T/log.txt)
  [ "$n" -eq 1 ] && ok "sin opciones, USB: 3 pasos hacia arriba y 1 hacia abajo = -2" || ko "sin opciones, USB: pasos de la rueda ($(grep '^Pasos de la rueda' $T/log.txt | tail -1))"
  guarda sinopc
  if [ $fail -ne $f0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/HSPRATON/,$p' $T/log.txt | tail -40; cat $T/ratones; fi
  echo "-- 1c. DOS: HSPRATON /BIOS: sin sondear, por la BIOS (el PS/2, sin rueda) --"
  f0=$fail
  cp "$IMG" $T/f.img
  config 'DEVICE=A:\\HISPADOS\\HSPUSB.SYS /F\r\n'
  { sleep 9; send 'A:\HISPADOS\HSPRATON /BIOS' 3
    send 'BIN\MOUSETST' 2; raton ps2; rueda 3; tecla ret; sleep 1.5
    send 'BIN\MOUSETST' 2; raton usb; rueda 2; tecla ret; sleep 1.5
    mon quit; } | qemu 60 $USB
  check "Vía BIOS (INT 15h). Ratón PS/2 preparado."
  n=$(grep -c "^Pasos de la rueda: 0$" $T/log.txt)
  [ "$n" -eq 1 ] && ok "/BIOS, PS/2: la rueda no llega (0)" || ko "/BIOS, PS/2: pasos de la rueda ($(grep '^Pasos de la rueda' $T/log.txt | head -1))"
  n=$(grep -c "^Pasos de la rueda: 2$" $T/log.txt)
  [ "$n" -eq 1 ] && ok "/BIOS, USB: la rueda sí (2)" || ko "/BIOS, USB: pasos de la rueda ($(grep '^Pasos de la rueda' $T/log.txt | tail -1))"
  if [ $fail -ne $f0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/HSPRATON/,$p' $T/log.txt | tail -30; cat $T/ratones; fi
fi

# ---- 2. el touchpad Synaptics en modo absoluto (sin touchpad) ----
if quiere 2; then
  echo "-- 2. touchpad Synaptics en modo absoluto: el borde derecho es la rueda --"
  f0=$fail
  cp "$IMG" $T/f.img
  mcopy -o -i $T/f.img $B/SYNTEST.COM ::BIN/SYNTEST.COM
  { sleep 6; send 'A:\HISPADOS\HSPRATON /DIRECTO /SYNPRUEBA' 3
    send 'BIN\SYNTEST' 5
    send 'A:\HISPADOS\HSPRATON /INFO' 3
    mon quit; } | qemu 40
  check "Rueda: sí"
  if grep -qE "^Puntero: dx=[1-9][0-9]* dy=[1-9][0-9]* rueda=0$" $T/log.txt; then ok "el dedo en el centro mueve el puntero (a la derecha y abajo)"
  else ko "el dedo en el centro: $(grep '^Puntero' $T/log.txt)"; fi
  check "Borde: puntero quieto rueda=5"
  check "Borde: puntero quieto rueda=-3"
  check "Toque: botones=1 pulsaciones=1 y luego botones=0"
  check "paquetes de 6 bytes (absolutos: el borde derecho es la rueda)"
  guarda syn
  if [ $fail -ne $f0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/SYNPRUEBA/,$p' $T/log.txt | tail -30; fi
fi

# ---- 3. un programa de Windows dentro de Hispa Ventanas ----
if quiere 3 && [ $CON32 = 1 ]; then
  echo "-- 3. WINRUEDA.EXE: WM_MOUSEWHEEL en un programa de Windows --"
  f0=$fail
  cp "$IMG" $T/f.img
  config 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nDEVICE=A:\\HISPADOS\\HSPUSB.SYS /F\r\nFILES=30\r\n'
  for f in HW32.COM MSVCRT.DLL; do mcopy -o -i $T/f.img $B/$f ::HISPADOS/$f || ko "no cabe $f"; done
  mcopy -o -i $T/f.img $B/WINRUEDA.EXE ::BIN/WINRUEDA.EXE || ko "no cabe WINRUEDA.EXE"
  printf '%s\r\n' '[Centro]' 'Ventana=36,52,452,190' 'Activo=0' 'SinRepetidos=1' \
    '[Grupo]' 'Nombre=Win32' 'Ventana=60,380,600,180' 'Estado=1' \
    '[Acceso]' 'Nombre=Rueda' 'Icono=4' 'Programa=A:\BIN\WINRUEDA.EXE' \
    | iconv -f UTF-8 -t CP437 > $T/CENTRO.INI
  mcopy -o -i $T/f.img $T/CENTRO.INI ::HISPADOS/CENTRO.INI
  RX=400; RY=300
  # La ventana: en 100,100, de 480x260; su zona empieza en 102,125. La hija,
  # en 362,135 (200x80); la lista, en 362,225 (200x110).
  { sleep 9; send 'A:\HISPADOS\HSPRATON /DIRECTO' 3; send 'VENTANAS' 9
    tecla ret; sleep 6; raton ps2; ir 200 300; shot w0
    rueda 2; shot w1                                  # la madre: dos hacia abajo
    ir 450 170; rueda -1; shot w2                     # la hija, uno hacia arriba: a la madre
    ir 450 280; rueda 2; shot w3                      # la lista: seis líneas
    raton usb; ir 200 300; rueda 1; shot w4           # el ratón USB: la madre
    ir 450 280; rueda -1; shot w5                     # y la lista
    mon quit; } | qemu 120 $USB
  Z="102 125 578 358"
  chk $T/w0.ppm titulo 100 100 activa "Rueda en Win32" busca $Z 16 231 "Madre: 0, suma 0" busca $Z 16 231 "Líneas por paso: 3" \
    busca $Z 16 231 "Hija: 0" busca 362 225 562 250 0 9 "Elemento 1"
  echo "   (ratón PS/2)"
  chk $T/w1.ppm busca $Z 16 231 "Madre: 2, suma -240" busca $Z 16 231 "Punto: 200,300" busca $Z 16 231 "Teclas: 0" busca $Z 16 231 "Hija: 0"
  chk $T/w2.ppm busca $Z 16 231 "Madre: 3, suma -120" busca $Z 16 231 "Hija: 1" busca $Z 16 231 "Punto: 450,170"
  chk $T/w3.ppm busca $Z 16 231 "Madre: 3, suma -120" busca 362 225 562 250 0 9 "Elemento 7" nobusca 362 225 562 335 0 9 "Elemento 6"
  echo "   (ratón USB)"
  chk $T/w4.ppm busca $Z 16 231 "Madre: 4, suma -240" busca $Z 16 231 "Punto: 200,300"
  chk $T/w5.ppm busca $Z 16 231 "Madre: 4, suma -240" busca 362 225 562 250 0 9 "Elemento 4" nobusca 362 225 562 335 0 9 "Elemento 3"
  guarda win32
  if [ $fail -ne $f0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/VENTANAS/,$p' $T/log.txt | tail -20; cat $T/ratones; fi
fi

# ---- 4. el Navegador ----
if quiere 4 && [ $CON32 = 1 ]; then
  echo "-- 4. el Navegador: la página y la lista desplegable --"
  f0=$fail
  . tests/tools/nav_certs.sh
  if ! nav_certs $T > /dev/null 2>&1; then echo "  (sin openssl: se salta)"
  else
    P=$((20000 + $$ % 20000))
    python3 tests/tools/nav_servidor.py $T $P > $T/srv.log 2>&1 &
    SRV=$!
    trap 'kill $SRV 2> /dev/null' EXIT
    for i in 1 2 3 4 5 6 7 8 9 10; do grep -q listo $T/srv.log 2> /dev/null && break; sleep 0.5; done
    cp "$IMG" $T/f.img
    config 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nDEVICE=A:\\HISPADOS\\HSPUSB.SYS /F\r\nFILES=30\r\n'
    rm -rf $T/c; mkdir -p $T/c
    mcopy -s -n -i "$IMG" ::HISPADOS $T/c/
    mkdir -p $T/c/HISPADOS/INTERNET
    cp $B/MSVCRT.DLL $B/WSOCK32.DLL $B/HSPTLS.DLL $B/HSPJS.DLL $B/NAVEGA.EXE $T/c/HISPADOS/INTERNET/
    mkdir -p $T/c/HISPADOS/INTERNET/AUTORID; cp $T/RAIZ.CER $T/c/HISPADOS/INTERNET/AUTORID/
    cp $B/HW32.COM $B/MSVCRT.DLL $T/c/HISPADOS/
    printf 'Inicio=http://10.0.2.2:%d/larga\r\nIdioma=es-ES\r\nJavaScript=NO\r\n' $P > $T/c/HISPADOS/INTERNET/NAVEGA.INI
    python3 tests/tools/mkhdd.py $T/c.img 16 $T/c/HISPADOS > /dev/null
    RX=400; RY=300
    # La ventana va a toda la pantalla: la fila f de la página, en y = 80 + 18 f.
    PAG="0 80 780 460"
    { sleep 9; send 'A:\HISPADOS\HSPRED' 2; send 'A:\HISPADOS\HSPRATON' 3; send 'C:\HISPADOS\VENTANAS /486' 10
      tecla ctrl-esc; sleep 1; tecla i; sleep 0.5; tecla right; sleep 1; tecla ret; sleep 12
      raton ps2; ir 400 300; shot n0
      rueda 2; shot n1                                 # dos pasos: seis filas
      tecla home; sleep 1; tecla down down down down down down; sleep 1; shot n2   # lo mismo con seis flechas
      tecla home; sleep 1; clic 100 89; sleep 1.5; ir 400 300; shot l0   # la lista desplegable
      ir 100 150; rueda 1; shot l1                     # tres opciones más abajo
      tecla esc; sleep 1
      raton usb; ir 400 300; rueda 3; shot u1          # el ratón USB: nueve filas
      tecla up up up; sleep 1; shot u2                 # tres arriba: como n2 (seis)
      mon quit; } | qemu 200 -drive file=$T/c.img,format=raw,if=ide,index=0 -netdev user,id=n0 -device rtl8139,netdev=n0 $USB
    kill $SRV 2> /dev/null
    chk $T/n0.ppm busca $PAG 0 9 "Elija:" busca $PAG 0 9 "Línea larga 001."
    echo "   (ratón PS/2)"
    chk $T/n1.ppm nobusca $PAG 0 9 "Elija:" busca 0 80 300 100 0 9 "Línea larga 003." cambia $PAG $T/n0.ppm 500
    chk $T/n1.ppm igual $PAG $T/n2.ppm 0 80 0
    chk $T/l0.ppm busca 0 80 400 300 9 12 "Opción 01" busca 0 80 400 300 0 9 "Opción 10"
    chk $T/l1.ppm busca 0 80 400 300 0 9 "Opción 04" busca 0 80 400 300 0 9 "Opción 13" nobusca 0 97 400 300 0 9 "Opción 01"
    echo "   (ratón USB)"
    chk $T/u1.ppm busca 0 80 300 110 0 9 "Línea larga 005." cambia $PAG $T/n2.ppm 500
    chk $T/u2.ppm igual $PAG $T/n2.ppm 0 80 0
    guarda nav
  fi
  if [ $fail -ne $f0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/VENTANAS/,$p' $T/log.txt | tail -20; cat $T/ratones 2> /dev/null; fi
fi
[ $CON32 = 1 ] || echo "  (sin gcc de 32 bits: sin las sesiones 3 y 4)"

rm -rf $T
exit $fail
