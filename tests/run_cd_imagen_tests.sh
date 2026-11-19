#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
#
# Pruebas de las imágenes de CD (make cd / make cd486; docs/CD.md) y del
# programa de instalación (INSTALAR /SETUP), en QEMU con una máquina de los
# 90 (486 o Pentium, 16 MB, sin USB):
#   cd        el ISO de Hispa-386 arrancando como CD (-boot d): el primer
#             menú (1 arrancar sin instalar, por omisión a los 10 s; 2
#             instalar), el de siempre (Normal...), HSPCD con el CD, el CD
#             con su letra, DIR del CD y Hispa Ventanas desde el CD
#   instalar  la opción 2 en un disco duro vacío: las pantallas azules
#             (bienvenida, F1, F3, el disco vacío, la carpeta, la copia con
#             barra, el final), reiniciar y arrancar del disco instalado,
#             que ya no tiene el primer menú
#   disquete  un «386» que no arranca de CD (486 con -boot a): el disquete
#             de siempre con el CD puesto; HSPCD ve el CD y la opción 2
#             instala desde él sin pedir los demás disquetes
#   disquetes la opción 2 desde el disquete de arranque, sin CD: pide
#             «Instalación 2» y «Oficina» (con el de arranque puesto dice
#             que no es ese) y, con ESC al de «Hispa-486», queda Hispa-386
#   usb       la opción 2 desde la memoria USB (disco 1) a un disco duro
#             vacío (disco 2): la memoria es la X:, se instala en la C:
#             y la memoria queda sin tocar
#   sola      la opción 2 desde la memoria USB sin más disco: no la toca
#             (ni PARTIR /BORRARTODO): «no hay disco duro donde instalar»
#   486       el CD de Hispa-486 arranca hasta el escritorio
# Sin argumento, todas. CAPTURAS=carpeta guarda las capturas de las pantallas
# (docs/cd/*.png).
# CD image and SETUP tests.
set -u
cd "$(dirname "$0")/.."
B=${B:-build}
MODO=${1:-todo}
ISO=$B/hispados-cd.iso
ISO486=$B/hispa486-cd.iso
DISQ=$B/hispados-disquete.img
CAP=${CAPTURAS:-}
export MTOOLS_SKIP_CHECK=1 QEMU_AUDIO_DRV=none
[ -f $ISO ] || { echo "falta $ISO (make cd)"; exit 1; }
T=$(mktemp -d)
QP=""
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
log() { tr -d '\r' < $T/l 2>/dev/null | iconv -f CP437 -t UTF-8 -c; }
check() { if log | grep -qF -- "$1"; then ok "$1"; else ko "$1"; fi; }
checkn() { if log | grep -qF -- "$1"; then ko "(no debe salir) $1"; else ok "no sale: $1"; fi; }
# espera N TEXTO [segundos]: hasta que TEXTO haya salido N veces
espera() { local i=0 tope=$(( ${3:-120} * 2 )); until [ "$(log | grep -cF -- "$2")" -ge $1 ] || [ $i -ge $tope ]; do sleep 0.5; i=$((i+1)); done; sleep 1; }
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; sleep 0.3; }
tecla() { local k; for k in "$@"; do mon "sendkey $k"; done; }
# captura NOMBRE: la pantalla, en $T/NOMBRE.ppm (y en $CAP/NOMBRE.png)
captura() {
  sleep 0.5; mon "screendump $T/$1.ppm"; sleep 0.5
  if [ -n "$CAP" ]; then mkdir -p "$CAP"; convert $T/$1.ppm "$CAP/$1.png" 2>/dev/null || pnmtopng $T/$1.ppm > "$CAP/$1.png"; fi
}
# ancho x alto de una captura (720x400 = modo texto)
tamano() { head -c 20 $T/$1.ppm | tr '\n' ' ' | awk '{print $2"x"$3}'; }
# el disco duro instalado (la partición: en el sector 63 o en el 2048)
hdoff() { local o; for o in 32256 1048576; do mdir -i $T/hdd.img@@$o :: > /dev/null 2>&1 && { echo $o; return; }; done; echo 32256; }
hdtiene() { if mdir -a -i $T/hdd.img@@$(hdoff) "::$1" > /dev/null 2>&1; then ok "en el disco duro: $1"; else ko "falta en el disco duro: $1"; fi; }
hdtxt() { mtype -i $T/hdd.img@@$(hdoff) "::$1" 2>/dev/null | tr -d '\r' | iconv -f CP437 -t UTF-8 -c; }
qemu_vivo() { kill -0 $QP 2>/dev/null; }

# arranca opciones-de-QEMU...: en segundo plano; la consola por el puerto
# serie (-serial pipe: FIFO .in y .out) a $T/l, y escribe le manda órdenes
arranca() {
  rm -f $T/l $T/mon $T/ser.in $T/ser.out; mkfifo $T/ser.in $T/ser.out
  qemu-system-i386 -display none -parallel none -m 16 -no-reboot -rtc base=localtime \
    -serial pipe:$T/ser -monitor unix:$T/mon,server,nowait "$@" > /dev/null 2>&1 &
  QP=$!
  cat $T/ser.out > $T/l &
  exec 7> $T/ser.in
}
escribe() { printf '%s\r' "$1" >&7; sleep "${2:-1.5}"; }
para() { exec 7>&- 2>/dev/null; [ -n "$QP" ] && kill $QP 2>/dev/null; wait $QP 2>/dev/null; QP=""; sleep 0.5; }
trap 'para; rm -rf $T' EXIT

# ---------------------------------------------------------------- cd
prueba_cd() {
  echo "== CD de Hispa-386: arrancar desde el CD (486, 16 MB) =="
  cp $ISO $T/c.iso
  arranca -cpu 486 -cdrom $T/c.iso -boot d
  espera 1 "Elija una opci" 30
  captura cd-menu
  espera 1 "Bienvenido a Hispa-DOS" 90
  check "1. Arrancar Hispa-DOS sin instalar"
  check "2. Instalar Hispa-DOS en el disco duro"
  check "1. Normal"
  check "3. Prueba del equipo y del sonido"
  check "Primera unidad de CD/DVD: C:"
  check "El CD de Hispa-DOS está en la unidad C:"
  check "HSPRATON 0.1"
  checkn "No se encuentra el CD de Hispa-DOS"
  escribe "DIR C:\\" 3
  check "HISPADOS        <DIR>"
  check "LEEMECD  TXT"
  check "ARRANQUE IMG     1.474.560"
  escribe "DIR C:\\HISPADOS\\HISPACD.INI" 3
  check "HISPACD  INI"
  escribe "SET" 2
  check "CD=C"
  check "PATH=C:\\HISPADOS;A:\\HISPADOS"
  escribe "VENTANAS" 12
  captura cd-ventanas
  local t; t=$(tamano cd-ventanas)
  if [ "$t" != "720x400" ] && [ -n "$t" ]; then ok "Hispa Ventanas arranca desde el CD (pantalla gráfica $t)"; else ko "Hispa Ventanas no sale desde el CD ($t)"; fi
  para
}

# ---------------------------------------------------------------- instalar
prueba_instalar() {
  echo "== CD de Hispa-386: el programa de instalación en un disco duro vacío (Pentium, 16 MB) =="
  cp $ISO $T/c.iso
  dd if=/dev/zero of=$T/hdd.img bs=1M count=64 status=none
  arranca -cpu pentium -drive file=$T/hdd.img,format=raw,if=ide,index=0 -cdrom $T/c.iso -boot d
  espera 1 "Elija una opci" 30
  tecla 2
  espera 1 "Bienvenido al programa de instalación" 60
  captura setup-1-bienvenida
  # [LETRAS] con disco duro, aunque esté vacío, la C: es suya y el CD la D:
  check "Primera unidad de CD/DVD: D:"
  check "El CD de Hispa-DOS está en la unidad D:"
  check "Programa de instalación de Hispa-DOS"
  check "Hispa-386"
  check "INTRO=Continuar   F1=Ayuda   F3=Salir"
  tecla f1
  espera 1 "Ayuda del programa de instalación" 10
  check "Ayuda del programa de instalación"
  tecla ret
  espera 2 "Bienvenido al programa de instalación" 10
  tecla f3
  espera 1 "Hispa-DOS no está instalado del todo" 10
  check "Hispa-DOS no está instalado del todo"
  tecla ret
  espera 3 "Bienvenido al programa de instalación" 10
  tecla ret
  espera 1 "está vacío" 60
  captura setup-2-disco
  check "El disco duro de este ordenador está vacío"
  tecla ret
  espera 1 "en esta carpeta del disco duro" 120
  check "Hispa-DOS se instalará en esta carpeta del disco duro"
  check "C:\\HISPADOS"
  check "Se copia desde la unidad D: (el CD de Hispa-DOS)"
  tecla ret
  espera 1 "Copiando: \\HISPADOS\\H" 120
  captura setup-3-copia
  espera 1 "Hispa-DOS está instalado en el disco duro" 400
  captura setup-4-fin
  check "Copiando: \\HISPADOS\\EDITAR.COM"
  check "Hispa-DOS está instalado en el disco duro"
  checkn "Inserte el disco"
  tecla ret
  local i=0; while qemu_vivo && [ $i -lt 40 ]; do sleep 0.5; i=$((i+1)); done
  if qemu_vivo; then ko "INTRO al final no reinicia"; else ok "INTRO al final reinicia el ordenador"; fi
  para
  hdtiene KERNEL.SYS
  hdtiene HISPADOS/EDITAR.COM
  hdtiene HISPADOS/HW32.COM
  hdtiene HISPADOS/PROGS/PLUMA.HVP
  hdtiene HISPADOS/JUEGOS/NES/HISPA1.NES
  hdtxt CONFIG.SYS > $T/cfg.txt
  hdtxt AUTOEXEC.BAT > $T/aut.txt
  if grep -q "^MENUITEM=NORMAL" $T/cfg.txt && ! grep -q "SOLO-MEDIO\|ARRANCAR\|INSTALAR" $T/cfg.txt; then
    ok "CONFIG.SYS del disco: el menú de siempre, sin el primero (instalar)"; else ko "CONFIG.SYS del disco con el primer menú"; fi
  if grep -q "^C:.HISPADOS.HSPRATON" $T/aut.txt && ! grep -q "SOLO-MEDIO\|INSTALAR /SETUP\|%CD%" $T/aut.txt; then
    ok "AUTOEXEC.BAT del disco: con C: y sin la opción de instalar"; else ko "AUTOEXEC.BAT del disco"; fi
  echo "== y arrancar del disco duro instalado =="
  arranca -cpu pentium -drive file=$T/hdd.img,format=raw,if=ide,index=0 -boot c
  espera 1 "Bienvenido a Hispa-DOS" 90
  check "1. Normal"
  checkn "Instalar Hispa-DOS en el disco duro"
  check "Bienvenido a Hispa-DOS"
  escribe "DIR C:\\HISPADOS\\INSTALAR.COM" 3
  check "INSTALAR COM"
  para
}

# ---------------------------------------------------------------- disquete
prueba_disquete() {
  echo "== Un «386» sin arranque de CD: el disquete de siempre y el CD (486, -boot a) =="
  [ -f $DISQ ] || { ko "falta $DISQ (make usb)"; return; }
  cp $DISQ $T/f.img
  cp $ISO $T/c.iso
  dd if=/dev/zero of=$T/hdd.img bs=1M count=64 status=none
  arranca -cpu 486 -drive file=$T/f.img,format=raw,if=floppy \
    -drive file=$T/hdd.img,format=raw,if=ide,index=0 -drive file=$T/c.iso,media=cdrom,if=ide,index=2 -boot a
  espera 1 "Elija una opci" 30
  check "2. Instalar Hispa-DOS en el disco duro"
  tecla 2
  espera 1 "Bienvenido al programa de instalación" 60
  check "Primera unidad de CD/DVD: D:"
  tecla ret
  espera 1 "está vacío" 60
  tecla ret
  espera 1 "en esta carpeta del disco duro" 120
  check "Se copia desde la unidad D: (el CD de Hispa-DOS)"
  tecla ret
  espera 1 "Hispa-DOS está instalado en el disco duro" 400
  check "Hispa-DOS está instalado en el disco duro"
  checkn "Inserte el disco rotulado"
  para
  hdtiene HISPADOS/HW32.COM                 # (en el disquete «Instalación 2»)
  hdtiene HISPADOS/PROGS/PLUMA.HVP          # (en el disquete «Oficina»)
  hdtiene HISPADOS/PROGS/TIPOS32.DAT
}

# ---------------------------------------------------------------- disquetes
prueba_disquetes() {
  echo "== El programa de instalación desde los disquetes, sin CD (486, -boot a) =="
  local f
  for f in $DISQ $B/hispados-instal2.img $B/hispados-oficina.img; do [ -f $f ] || { ko "falta $f (make usb)"; return; }; done
  cp $DISQ $T/f.img
  cp $B/hispados-instal2.img $T/i2.img
  cp $B/hispados-oficina.img $T/of.img
  dd if=/dev/zero of=$T/hdd.img bs=1M count=64 status=none
  arranca -cpu 486 -drive file=$T/f.img,format=raw,if=floppy \
    -drive file=$T/hdd.img,format=raw,if=ide,index=0 -boot a
  espera 1 "Elija una opci" 30
  tecla 2
  espera 1 "Bienvenido al programa de instalación" 60
  tecla ret
  espera 1 "está vacío" 60
  tecla ret
  espera 1 "en esta carpeta del disco duro" 120
  check "Se copia desde la unidad A:"
  tecla ret
  espera 1 "Hispa-DOS - Instalación 2" 300
  captura setup-5-disquete
  check "Inserte el disco rotulado"
  check "Hispa-DOS - Instalación 2"
  check "INTRO=Continuar   ESC=Seguir sin este disco"
  tecla ret                                  # (sigue el de arranque: no es)
  espera 1 "Ese no es el disco pedido" 20
  check "Ese no es el disco pedido"
  mon "change floppy0 $T/i2.img"; sleep 1
  tecla ret
  espera 1 "Hispa-DOS - Oficina" 120
  mon "change floppy0 $T/of.img"; sleep 1
  tecla ret
  espera 1 "Hispa-DOS - Hispa-486" 120
  check "Para quedarse con Hispa-386, pulse ESC"
  tecla esc
  espera 1 "Hispa-DOS está instalado en el disco duro" 120
  check "Hispa-DOS está instalado en el disco duro"
  para
  hdtiene KERNEL.SYS
  hdtiene HISPADOS/HW32.COM
  hdtiene HISPADOS/PROGS/TIPOS32.DAT
  hdtiene HISPADOS/PROGS/PLUMA.HVP
  if mdir -i $T/hdd.img@@$(hdoff) ::HISPADOS/EDICION.INI > /dev/null 2>&1; then ko "con ESC al de Hispa-486 queda EDICION.INI"; else ok "con ESC al disco «Hispa-486», queda Hispa-386"; fi
}

# ---------------------------------------------------------------- usb
prueba_usb() {
  echo "== El programa de instalación desde la memoria USB (el 2.º disco, vacío) =="
  local USBIMG=$B/hispados-usb.img
  [ -f $USBIMG ] || { ko "falta $USBIMG (make usb)"; return; }
  cp --sparse=always $USBIMG $T/usb.img
  dd if=/dev/zero of=$T/hdd.img bs=1M count=64 status=none
  arranca -cpu pentium -drive file=$T/usb.img,format=raw,if=ide,index=0 \
    -drive file=$T/hdd.img,format=raw,if=ide,index=1 -boot c
  espera 1 "Elija una opci" 30
  tecla 2
  espera 1 "Bienvenido al programa de instalación" 60
  tecla ret
  espera 1 "está vacío" 60
  check "El disco duro de este ordenador está vacío"
  tecla ret
  espera 1 "en esta carpeta del disco duro" 120
  check "C:\\HISPADOS"
  check "Se copia desde la unidad X:"
  tecla ret
  espera 1 "Hispa-DOS está instalado en el disco duro" 400
  check "Hispa-DOS está instalado en el disco duro"
  checkn "Inserte el disco"
  para
  hdtiene KERNEL.SYS
  hdtiene HISPADOS/PROGS/PLUMA.HVP
  hdtxt CONFIG.SYS > $T/cfg.txt
  # [LETRAS] los archivos de la memoria van sin letra (valen en el disco duro)
  if grep -q "^DEVICE=.HISPADOS.HSPXMS.SYS" $T/cfg.txt && ! grep -q "SOLO-MEDIO\|[A-Z]:.HISPADOS" $T/cfg.txt; then
    ok "CONFIG.SYS del disco: sin letra de unidad y sin el primer menú"; else ko "CONFIG.SYS del disco"; fi
  if cmp -s <(dd if=$T/usb.img bs=1M count=1 status=none) <(dd if=$B/hispados-usb.img bs=1M count=1 status=none); then
    ok "la memoria USB no se ha tocado (MBR y tabla)"; else ko "la memoria USB ha cambiado"; fi
}

# ---------------------------------------------------------------- sola
# [LETRAS] la memoria USB es el único disco (es la C:): el programa de
# instalación no la toca; dice que no hay disco duro donde instalar
prueba_sola() {
  echo "== El programa de instalación desde la memoria USB, sin disco duro: no se toca =="
  local USBIMG=$B/hispados-usb.img
  [ -f $USBIMG ] || { ko "falta $USBIMG (make usb)"; return; }
  cp --sparse=always $USBIMG $T/usb.img
  md5sum < $T/usb.img > $T/usb_antes
  arranca -cpu pentium -drive file=$T/usb.img,format=raw,if=ide,index=0 -boot c
  espera 1 "Elija una opci" 30
  tecla 2
  espera 1 "Bienvenido al programa de instalación" 60
  tecla ret
  espera 1 "ningún disco duro donde instalar" 60
  captura setup-6-sindisco
  check "No se ha encontrado ningún disco duro donde instalar Hispa-DOS."
  checkn "está vacío"
  checkn "ya tiene Hispa-DOS instalado"
  tecla ret
  espera 1 "C:\\>" 20
  escribe "PARTIR /BORRARTODO" 3
  escribe "PARTIR 1 /BORRARTODO" 3
  check "No hay ningún disco duro aparte del de arranque"
  checkn "Tabla de particiones borrada"
  para
  if md5sum < $T/usb.img | cmp -s - $T/usb_antes; then ok "la memoria USB, intacta (ni un octeto)"; else ko "la memoria USB ha cambiado"; fi
}

# ---------------------------------------------------------------- 486
prueba_486() {
  echo "== CD de Hispa-486: arranca hasta el escritorio (Pentium, 16 MB) =="
  [ -f $ISO486 ] || { echo "  (sin $ISO486: make cd486; se salta)"; return; }
  cp $ISO486 $T/c.iso
  arranca -cpu pentium -cdrom $T/c.iso -boot d
  espera 1 "Bienvenido a Hispa-DOS" 90
  sleep 15
  captura cd486-escritorio
  check "El CD de Hispa-DOS está en la unidad C:"
  local t; t=$(tamano cd486-escritorio)
  if [ "$t" != "720x400" ] && [ -n "$t" ]; then ok "el escritorio de Hispa-486 sale desde el CD ($t)"; else ko "no sale el escritorio de Hispa-486 ($t)"; fi
  para
}

case $MODO in
  cd) prueba_cd ;;
  instalar) prueba_instalar ;;
  disquete) prueba_disquete ;;
  disquetes) prueba_disquetes ;;
  usb) prueba_usb ;;
  sola) prueba_sola ;;
  486) prueba_486 ;;
  todo) prueba_cd; prueba_instalar; prueba_disquete; prueba_disquetes; prueba_usb; prueba_sola; prueba_486 ;;
  *) echo "uso: $0 [cd|instalar|disquete|disquetes|usb|sola|486|todo]"; exit 2 ;;
esac
[ $fail -ne 0 ] && [ -n "${VERBOSE:-}" ] && log
exit $fail
