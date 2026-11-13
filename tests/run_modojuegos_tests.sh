#!/bin/bash
# Pruebas del menú de arranque de CONFIG.SYS y de la opción «Juegos» del
# disquete y de la memoria USB de uso (floppy/ y usb/):
#   menu      el menú del núcleo con tests/config/MENU.SYS: [menu], MENUITEM,
#             SUBMENU, MENUDEFAULT con cuenta atrás y sin ella, [common],
#             INCLUDE, la variable CONFIG, elegir por el puerto serie y con el
#             teclado (flechas), F5 y F8 dentro del menú
#   normal    el disquete de uso sin tocar nada: a los 5 s arranca «Normal»
#             (lo de siempre: modo real, sin HSPEMM)
#   memoria   el disquete de uso eligiendo «Juegos», tal cual: HMA, memoria
#             superior, EMS, BLASTER y la memoria convencional libre
#   juegos    «Juegos» con HSPUSB llevando el USB (/F) en modo V86 (HSPEMM):
#             memoria USB (leer, escribir, fsck), teclado y ratón USB y un
#             mando (tests/tools/usbmando.py), con EMS y sonido a la vez
#   umb       HSPUSB cargado en la memoria superior (DEVICEHIGH): las
#             direcciones físicas de sus anillos no son las lineales
#   scratch   HSPUSB con páginas de trabajo (XMS) en modo V86
#   pequeno   un bloque de memoria superior en el que caben los ficheros de
#             HSPUSB y HSPSND pero no lo que necesitan después (la zona de
#             trabajo, los bloques de DMA): DEVICEHIGH y LH los dejan abajo
#             sin pisar la memoria de detrás; y un controlador que no se
#             queda (no hay xHCI) no repite sus mensajes
#   usb       la imagen USB arrancando con «Juegos» (la BIOS lleva el USB)
#   usbu      la imagen USB con «Juegos» y HSPUSB /U: HSPUSB atiende en modo
#             V86 el disco del que se ha arrancado (INT 13h) y el teclado
# Sin segundo argumento se hacen todas.
# Boot menu and "Games" configuration tests (HSPUSB under the V86 monitor).
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
MODE=${2:-todo}
B=$(dirname "$IMG")
DISQ=$B/hispados-disquete.img
USBIMG=$B/hispados-usb.img
export QEMU_AUDIO_DRV=none
fail=0
T=""
QP=""
MP=""
P='A:\>'

check() {
  if grep -qF -- "$1" $T/log.txt; then echo "  OK   $1"; else echo "  FAIL $1"; cfail=1; fi
}
nocheck() {
  if grep -qF -- "$1" $T/log.txt; then echo "  FAIL no debía salir: $1"; cfail=1; else echo "  OK   (no sale) $1"; fi
}
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; cfail=1; }
veces() { grep -aoF -- "$1" $T/log 2>/dev/null | wc -l; }
espera() {  # texto [veces]
  local n=${2:-1} i=0
  while [ "$(veces "$1")" -lt "$n" ] && [ $i -lt 600 ]; do sleep 0.1; i=$((i + 1)); done
  [ $i -lt 600 ] || echo "  (no llega: $1)"
}
orden() {
  local n=$(veces "$P")
  printf '%s\r' "$1" >&3
  espera "$P" $((n + 1))
}
caso() {  # nombre imagen
  T=$(mktemp -d)
  cp "$2" $T/f.img
  cfail=0
  P='A:\>'
  echo "== Hispa-DOS modo juegos ($1) =="
}
config() {  # texto de CONFIG.SYS (printf)
  printf "$1" > $T/CONFIG.SYS
  mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
}
# arrancar sin esperar al símbolo del sistema (hay menú)
arranca() {  # [opciones de QEMU]
  rm -f $T/in $T/log
  mkfifo $T/in
  qemu-system-i386 -display none -serial stdio -monitor none -m 32 \
      -drive file=$T/f.img,format=raw,if=floppy "$@" -boot a -no-reboot < $T/in > $T/log 2>&1 &
  QP=$!
  exec 3> $T/in
}
para() {
  exec 3>&-
  kill $QP 2>/dev/null
  wait $QP $MP 2>/dev/null
  MP=""
}
fin() {
  para
  tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 -c | grep -v '^audio:\|^ALSA\|^alsa' > $T/log.txt
  "$@"
  if [ $cfail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log.txt; fi
  [ $cfail -ne 0 ] && fail=1
  rm -rf $T
}
mando() {
  USBMANDO_VE=$T/log python3 tests/tools/usbmando.py "$@" &
  MP="$MP $!"
}
mkstick() {
  dd if=/dev/zero of=$T/usb.img bs=1M count=32 status=none
  mformat -i $T/usb.img -T 65536 -h 64 -s 32 -v USBJUEGO ::
  printf 'Hola desde la memoria USB\r\n' > $T/HOLA.TXT
  mcopy -i $T/usb.img $T/HOLA.TXT ::HOLA.TXT
}
# bytes libres de la memoria convencional según la última orden MEM
libres() { grep -a "bytes libres" $T/log.txt | head -1 | tr -d ' .' | sed 's/[^0-9].*//'; }
quiere() { [ "$MODE" = todo ] || [ "$MODE" = "$1" ]; }
falta() {
  echo "== Hispa-DOS modo juegos ($1) =="
  echo "  FAIL falta $2"
  fail=1
}

# ---- menu: el menú de configuraciones del núcleo ----
if quiere menu; then
  # 1) elegir «Juegos» por el puerto serie: bloque, [common] e INCLUDE
  caso "menú: elegir por número" "$IMG"
  mcopy -o -i $T/f.img tests/config/MENU.SYS ::CONFIG.SYS
  cp $T/f.img $T/base.img
  arranca
  espera 'Elija una opci'
  printf '2\r' >&3
  espera "$P"
  orden 'SET'
  orden 'MEM'
  c_m1() {
    check "  Menú de arranque de Hispa-DOS"
    check "    1. Normal (como siempre)"
    check "    2. Juegos (EMS y memoria superior)"
    check "    3. Otras opciones"
    nocheck "No debe salir"
    check "Tiempo restante:"
    check "F5=Arranque limpio   F8=Menú paso a paso [N]"
    check "HSPEMM 0.1: EMS 4.0 y VCPI"
    check "CONFIG=JUEGOS"
    check "ANTES=si"
    check "MODO=juegos"
    check "BASE=si"
    check "FINAL=si"
    check "Hispa-DOS está cargado en la memoria alta (HMA)"
    check "bytes de memoria superior (UMB) en total"
    nocheck "Error en CONFIG.SYS"
    nocheck "Iniciando Hispa-DOS..."
  }
  fin c_m1

  # 2) sin tocar nada: la opción por omisión tras la cuenta atrás
  caso "menú: opción por omisión" "$IMG"
  mcopy -o -i $T/f.img tests/config/MENU.SYS ::CONFIG.SYS
  arranca
  espera "$P"
  orden 'SET'
  orden 'MEM'
  c_m2() {
    check "    1. Normal (como siempre)"
    check "CONFIG=NORMAL"
    check "MODO=normal"
    check "FINAL=si"
    nocheck "BASE=si"
    nocheck "HSPEMM 0.1"
    check "Hispa-DOS está cargado en la memoria alta (HMA)"
    nocheck "bytes de memoria superior (UMB) en total"
  }
  fin c_m2

  # 3) un submenú, y los números de línea de los errores
  caso "menú: submenú" "$IMG"
  mcopy -o -i $T/f.img tests/config/MENU.SYS ::CONFIG.SYS
  arranca
  espera 'Elija una opci'
  printf '3\r' >&3
  espera 'Elija una opci' 2
  printf '1\r' >&3
  espera "$P"
  orden 'SET'
  c_m3() {
    check "    1. Una rara"
    check "    2. Volver a lo normal"
    check "CONFIG=RARO"
    check "MODO=raro"
    check "BASE=si"
    check "Orden no reconocida en CONFIG.SYS"
    nocheck "MODO=normal"
    nocheck "HSPEMM 0.1"
  }
  fin c_m3

  # 4) con el teclado: flecha abajo e Intro; 5) F5; 6) F8 y el menú paso a
  #    paso (se desactiva la primera línea, SET ANTES=si); 7) flecha arriba
  for K in flechas f5 f8 arriba; do
    caso "menú: teclado, $K" "$IMG"
    mcopy -o -i $T/f.img tests/config/MENU.SYS ::CONFIG.SYS
    Q="python3 tests/tools/qmp.py $T/qmp"
    arranca -qmp unix:$T/qmp,server,nowait
    espera 'Elija una opci'
    sleep 0.5
    case $K in
      flechas) $Q key - down wait 0.3 key - ret ;;
      f5)      $Q key - f5 ;;
      f8)      $Q key - f8 wait 0.3 key - 2 wait 0.3 key - ret wait 1.5 key - spc wait 0.3 key - ret ;;
      arriba)  $Q key - down wait 0.2 key - down wait 0.2 key - down wait 0.2 key - up wait 0.2 key - up wait 0.2 key - up wait 0.2 key - ret ;;
    esac
    espera "$P"
    orden 'SET'
    c_m4() {
      case $K in
        flechas) check "CONFIG=JUEGOS"; check "MODO=juegos"; check "ANTES=si" ;;
        f5)      check "Arranque limpio: sin CONFIG.SYS ni AUTOEXEC.BAT."; nocheck "CONFIG="; nocheck "HSPXMS 0.1" ;;
        f8)      check "CONFIG=JUEGOS"; check "MODO=juegos"; check "FINAL=si"; nocheck "ANTES=si" ;;
        arriba)  check "CONFIG=NORMAL"; check "MODO=normal" ;;
      esac
    }
    fin c_m4
  done

  # 8) MENUDEFAULT=bloque,0: sin preguntar
  caso "menú: sin espera" "$IMG"
  sed 's/^MENUDEFAULT=.*/MENUDEFAULT=juegos,0/' tests/config/MENU.SYS > $T/CONFIG.SYS
  mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
  arranca
  espera "$P"
  orden 'SET'
  c_m8() {
    nocheck "Elija una opci"
    check "CONFIG=JUEGOS"
    check "MODO=juegos"
    check "HSPEMM 0.1: EMS 4.0 y VCPI"
  }
  fin c_m8

  # 9) un CONFIG.SYS sin menú sigue como siempre (F5/F8 al arrancar)
  caso "menú: CONFIG.SYS sin menú" "$IMG"
  config 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nDOS=HIGH\r\nSET SINMENU=si\r\n'
  arranca
  espera "$P"
  orden 'SET'
  c_m9() {
    check "Iniciando Hispa-DOS...  F5 = arranque limpio"
    nocheck "Elija una opci"
    check "SINMENU=si"
    nocheck "CONFIG="
  }
  fin c_m9
fi

# ---- normal: el disquete de uso, sin tocar nada ----
c_normal() {
  check "    1. Normal"
  check "    2. Juegos (memoria expandida EMS y la mayor memoria libre)"
  check "Tiempo restante:"
  check "HSPXMS 0.1: gestor XMS 3.0"
  nocheck "HSPEMM 0.1"
  check "HSPCD 0.1 - lectores de CD/DVD de Hispa-DOS"
  check "HSPUSB 0.2 - USB nativo de Hispa-DOS (UHCI, OHCI, EHCI y xHCI)"
  check "Teclado español instalado."
  check "HSPRATON 0.1 - ratón de Hispa-DOS (INT 33h)"
  check "HSPJOY 0.1 - mandos de juego de Hispa-DOS"
  check "Sound Blaster 16 en 220h, IRQ 5, DMA 1 y 5"
  check "Bienvenido a Hispa-DOS. Escriba AYUDA para ver las órdenes."
  check "CONFIG=NORMAL"
  check "BLASTER=A220 I5 D1 H5 T6"
  check "Hispa-DOS está cargado en la memoria alta (HMA)"
  nocheck "bytes de memoria superior (UMB) en total"
  nocheck "Error en CONFIG.SYS"
  nocheck "Comando o nombre de archivo incorrecto"
  check "EMS: no hay gestor (EMMXXXX0)"
  n=$(libres)
  if [ -n "$n" ] && [ "$n" -ge 590000 ]; then ok "memoria convencional libre con «Normal»: $n bytes"; else ko "memoria convencional libre con «Normal»: $n bytes (se esperaban 590000 o más)"; fi
}
if quiere normal; then
  if [ ! -f $DISQ ]; then falta normal "$DISQ (make usb)"; else
    caso normal $DISQ
    mcopy -o -i $T/f.img $B/EMSTEST.COM ::BIN/
    arranca -audiodev none,id=a0 -device sb16,audiodev=a0
    espera "$P"
    orden 'MEM'
    orden 'SET'
    orden 'BIN\EMSTEST'
    fin c_normal
  fi
fi

# ---- memoria: «Juegos» tal como viene en el disquete ----
c_memoria() {
  check "HSPEMM 0.1: EMS 4.0 y VCPI"
  check "marco de página en "
  check "HSPCD 0.1 - lectores de CD/DVD de Hispa-DOS"
  check "HSPUSB 0.2 - USB nativo de Hispa-DOS (UHCI, OHCI, EHCI y xHCI)"
  check "Teclado español instalado."
  check "HSPRATON 0.1 - ratón de Hispa-DOS (INT 33h)"
  check "Sound Blaster 16 en 220h, IRQ 5, DMA 1 y 5"
  check "Bienvenido a Hispa-DOS. Escriba AYUDA para ver las órdenes."
  check "CONFIG=JUEGOS"
  check "BLASTER=A220 I5 D1 H5 T6"
  check "Hispa-DOS está cargado en la memoria alta (HMA)"
  check "bytes de memoria superior (UMB) en total"
  check "Programas en la memoria superior (UMB):"
  check "EMS test OK"
  check "VCPI test OK"
  nocheck "Error en CONFIG.SYS"
  nocheck "Comando o nombre de archivo incorrecto"
  # el sonido sigue yendo con el búfer de DMA donde haya quedado HSPSND
  check "Tono 440 Hz en Sound Blaster 16"
  n=$(libres)
  if [ -n "$n" ] && [ "$n" -ge 595000 ]; then ok "memoria convencional libre con «Juegos»: $n bytes"; else ko "memoria convencional libre con «Juegos»: $n bytes (se esperaban 595000 o más)"; fi
}
if quiere memoria; then
  if [ ! -f $DISQ ]; then falta memoria "$DISQ (make usb)"; else
    caso memoria $DISQ
    mcopy -o -i $T/f.img $B/EMSTEST.COM $B/VCPITEST.COM ::BIN/
    arranca -audiodev none,id=a0 -device sb16,audiodev=a0
    espera 'Elija una opci'
    printf '2\r' >&3
    espera "$P"
    orden 'MEM'
    orden 'MEM /C'
    orden 'SET'
    orden 'BIN\EMSTEST'
    orden 'BIN\VCPITEST'
    orden 'SONAR /TONE 440 200'
    fin c_memoria
  fi
fi

# ---- juegos: HSPUSB lleva el USB en modo V86 ----
c_juegos() {
  check "HSPEMM 0.1: EMS 4.0 y VCPI"
  check "Controlador xHCI en "
  nocheck "No funciona"
  # (C: es la memoria USB vista por la BIOS, que ya no la atiende; D:, el CD)
  check "Disco USB: 32 MB en la unidad E:"
  check "Teclado USB preparado."
  check "Ratón USB preparado."
  check "Mando USB preparado: 045E:028E, 6 ejes y 11 botones (tipo Xbox 360)."
  check "CONFIG=JUEGOS"
  check "BLASTER=A220 I5 D1 H5 T6"
  check "Hispa-DOS está cargado en la memoria alta (HMA)"
  check "bytes de memoria superior (UMB) en total"
  check "EMS test OK"
  # la memoria USB: leer, escribir y volver a leer
  check "Hola desde la memoria USB"
  check " El volumen de la unidad E es USBJUEGO"
  if cmp -s <(mtype -i $T/usb.img ::EDITAR.COM 2>/dev/null) $B/EDIT.COM; then ok "EDITAR.COM copiado intacto a la memoria USB"; else ko "copia de EDITAR.COM a la memoria USB"; fi
  if cmp -s <(mtype -i $T/f.img ::VUELTA.COM 2>/dev/null) $B/EDIT.COM; then ok "y leído intacto de vuelta"; else ko "lectura de vuelta de la memoria USB"; fi
  if fsck.fat -n $T/usb.img > $T/fsck 2>&1; then ok "fsck.fat de la memoria USB"; else ko "fsck.fat"; cat $T/fsck; fi
  # el teclado USB
  check "A:\\>ECHO Teclado"
  if grep -q "^Teclado USB va$" $T/log.txt; then ok "lo tecleado por el teclado USB llega"; else ko "lo tecleado por el teclado USB llega"; fi
  # el ratón USB
  check "X=320 Y=100 B=0"
  if grep -qE "^X=3[3-9][0-9] Y=1[0-9][0-9] B=0" $T/log.txt; then ok "movimiento del ratón USB"; else ko "movimiento del ratón USB"; fi
  check " B=1"
  check "Pulsaciones del botón izquierdo: 1"
  if grep -q "len 4" $T/trace; then ok "informes del ratón USB por el xHCI"; else ko "informes del ratón USB por el xHCI"; fi
  # el mando
  check "Mando 1: USB tipo Xbox 360 045E:028E (6 ejes, 11 botones)"
  check "  1  USB tipo Xbox 360 045E:028E, 6 ejes y 11 botones  <- jugador 1"
  check "INT15: botones=F0 X1=193 Y1=193 X2=193 Y2=193"
  check "INT15: botones=E0 X1=193 Y1=1 X2=193 Y2=191"
  check "INT15: botones=90 X1=1 Y1=1 X2=193 Y2=191"
  check "Mando 1: tipo=2 6/11 ejes=0 0 128 127 0 255 cruz=0 botones=0006"
  # y el sonido, a la vez
  check "Tono 440 Hz en Sound Blaster 16"
  # un programa en modo protegido (HSPDPMI, cliente VCPI) con HSPUSB
  # recogiendo informes con cada tic del reloj; después el teclado sigue
  check "Con HSPEMM cargado: el modo protegido se pide por VCPI."
  check "DPMI16 terminado, fallos: 00"
  if grep -q "^Sigue el teclado$" $T/log.txt; then ok "el teclado USB sigue tras el programa en modo protegido"; else ko "el teclado USB sigue tras el programa en modo protegido"; fi
  n=$(libres)
  if [ -n "$n" ] && [ "$n" -ge 540000 ]; then ok "memoria convencional libre con «Juegos» y HSPUSB /F: $n bytes"; else ko "memoria convencional libre con «Juegos» y HSPUSB /F: $n bytes (se esperaban 540000 o más)"; fi
}
if quiere juegos; then
  if [ ! -f $DISQ ]; then falta juegos "$DISQ (make usb)"; else
    caso juegos $DISQ
    # HSPUSB con /F en la opción «Juegos» (como dice el propio CONFIG.SYS)
    mtype -i $T/f.img ::CONFIG.SYS | sed 's/^DEVICEHIGH=A:\\HISPADOS\\HSPUSB.SYS/& \/F/' > $T/CONFIG.SYS
    grep -q '^DEVICEHIGH=A:\\HISPADOS\\HSPUSB.SYS /F' $T/CONFIG.SYS || { echo "  FAIL no está la línea de HSPUSB en [juegos]"; cfail=1; }
    mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
    mcopy -o -i $T/f.img $B/EMSTEST.COM $B/JOYTEST.COM $B/DPMI16.COM ::BIN/
    printf '[mandos]\r\njuegos=no\r\n' > $T/NADA.INI
    mcopy -o -i $T/f.img $T/NADA.INI ::
    mkstick
    mando $T/redir x360 \
        espera "Mirando" wait .6 rep 0014011000000000000000000000000000000000 wait .8 \
        rep 01030e wait .2 rep 0014006000ff0080ff7f00000000000000000000 wait .8 \
        rep 0014006000ff0080ff7f00000000000000000000
    Q="python3 tests/tools/qmp.py $T/qmp"
    arranca -qmp unix:$T/qmp,server,nowait -trace enable=usb_xhci_xfer_success,file=$T/trace \
        -audiodev none,id=a0 -device sb16,audiodev=a0 \
        -vga none -device VGA,id=vga0 -device qemu-xhci,id=xhci \
        -device usb-kbd,bus=xhci.0,id=ukbd,display=vga0 -device usb-mouse,bus=xhci.0,id=umouse \
        -drive if=none,id=u,format=raw,file=$T/usb.img -device usb-storage,bus=xhci.0,drive=u \
        -chardev socket,id=rd,path=$T/redir,server=on,wait=off -device usb-redir,chardev=rd,bus=xhci.0
    espera 'Elija una opci'
    printf '2\r' >&3
    espera "$P"
    orden 'MEM'
    orden 'SET'
    orden 'BIN\EMSTEST'
    orden 'TYPE E:\HOLA.TXT'
    orden 'COPY A:\HISPADOS\EDITAR.COM E:\'
    orden 'COPY E:\EDITAR.COM A:\VUELTA.COM'
    orden 'DIR E:'
    # el teclado USB (QMP): una orden entera
    n=$(veces "$P")
    $Q type vga0 "ECHO Teclado USB va" key vga0 ret
    espera "$P" $((n + 1))
    # el ratón USB
    n=$(veces "$P")
    printf 'BIN\\MOUSETST\r' >&3
    espera 'X=320 Y=100'
    sleep 1
    $Q move - 40 0 wait 1 move - 0 24 wait 1 btn - 1 wait 1 btn - 0 wait 1 key vga0 ret
    espera "$P" $((n + 1))
    # el mando
    orden 'HSPJOY /I:A:\NADA.INI'
    orden 'MANDO'
    orden 'BIN\JOYTEST W 60'
    orden 'BIN\JOYTEST'
    orden 'SONAR /TONE 440 200'
    orden 'A:\HISPADOS\HSPDPMI'
    printf 'BIN\\DPMI16\r' >&3
    espera 'DPMI16 terminado'
    sleep 1
    n=$(veces "$P")
    $Q type vga0 "ECHO Sigue el teclado" key vga0 ret
    espera "$P" $((n + 1))
    fin c_juegos
  fi
fi

# ---- umb: HSPUSB en la memoria superior ----
c_umb() {
  check "HSPEMM 0.1: VCPI"
  check "Disco USB: 32 MB en la unidad D:"
  check "Teclado USB preparado."
  check "Hola desde la memoria USB"
  if cmp -s <(mtype -i $T/usb.img ::EDITAR.COM 2>/dev/null) $B/EDIT.COM; then ok "EDITAR.COM copiado intacto a la memoria USB"; else ko "copia de EDITAR.COM a la memoria USB"; fi
  if cmp -s <(mtype -i $T/f.img ::VUELTA.COM 2>/dev/null) $B/EDIT.COM; then ok "y leído intacto de vuelta"; else ko "lectura de vuelta de la memoria USB"; fi
  if fsck.fat -n $T/usb.img > $T/fsck 2>&1; then ok "fsck.fat de la memoria USB"; else ko "fsck.fat"; cat $T/fsck; fi
  if grep -q "^Teclado USB va$" $T/log.txt; then ok "lo tecleado por el teclado USB llega"; else ko "lo tecleado por el teclado USB llega"; fi
  # HSPUSB (unos 40 KB) está arriba: abajo queda casi todo libre
  n=$(libres)
  if [ -n "$n" ] && [ "$n" -ge 615000 ]; then ok "HSPUSB en la memoria superior: $n bytes libres abajo"; else ko "HSPUSB en la memoria superior: $n bytes libres abajo (se esperaban 615000 o más)"; fi
}
if quiere umb; then
  caso umb "$IMG"
  config 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nDEVICE=A:\\HISPADOS\\HSPEMM.SYS NOEMS\r\nDOS=HIGH,UMB\r\nDEVICEHIGH=A:\\HISPADOS\\HSPUSB.SYS /F\r\n'
  mkstick
  Q="python3 tests/tools/qmp.py $T/qmp"
  arranca -net none -qmp unix:$T/qmp,server,nowait \
      -vga none -device VGA,id=vga0 -device qemu-xhci,id=xhci \
      -device usb-kbd,bus=xhci.0,id=ukbd,display=vga0 \
      -drive if=none,id=u,format=raw,file=$T/usb.img -device usb-storage,bus=xhci.0,drive=u
  espera "$P"
  orden 'MEM'
  orden 'MEM /C'
  orden 'TYPE D:\HOLA.TXT'
  orden 'COPY A:\HISPADOS\EDITAR.COM D:\'
  orden 'COPY D:\EDITAR.COM A:\VUELTA.COM'
  n=$(veces "$P")
  $Q type vga0 "ECHO Teclado USB va" key vga0 ret
  espera "$P" $((n + 1))
  fin c_umb
fi

# ---- scratch: las páginas de trabajo del xHCI (XMS) en modo V86 ----
c_scratch() {
  check "HSPEMM 0.1: EMS 4.0 y VCPI"
  check "Disco USB: 32 MB en la unidad D:"
  check "Hola desde la memoria USB"
  if cmp -s <(mtype -i $T/usb.img ::EDITAR.COM 2>/dev/null) $B/EDIT.COM; then ok "EDITAR.COM copiado intacto a la memoria USB"; else ko "copia de EDITAR.COM a la memoria USB"; fi
  check "XMS test OK"
}
if quiere scratch; then
  caso scratch "$IMG"
  python3 tools/cp437.py drivers/hspusb.asm $T/hspusb.asm
  if nasm -f bin -i $B/gen/lib/ -i lib/ -DFORCE_SCRATCH=37 -o $T/HSPUSB.SYS $T/hspusb.asm; then
    mcopy -o -i $T/f.img $T/HSPUSB.SYS ::HISPADOS/HSPUSB.SYS
    config 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nDEVICE=A:\\HISPADOS\\HSPEMM.SYS\r\nDOS=HIGH,UMB\r\nDEVICE=A:\\HISPADOS\\HSPUSB.SYS /F\r\n'
    mkstick
    arranca -device qemu-xhci,id=xhci -drive if=none,id=u,format=raw,file=$T/usb.img -device usb-storage,bus=xhci.0,drive=u
    espera "$P"
    orden 'TYPE D:\HOLA.TXT'
    orden 'COPY A:\HISPADOS\EDITAR.COM D:\'
    orden 'A:\BIN\XMSTEST'
    fin c_scratch
  else
    echo "  FAIL no se ensambla HSPUSB con FORCE_SCRATCH"; fail=1; rm -rf $T
  fi
fi

# ---- pequeno: lo que no cabe en la memoria superior se queda abajo ----
veces_txt() { grep -cF -- "$1" $T/log.txt; }
c_pequeno() {
  check "HSPEMM 0.1: VCPI"
  n=$(veces_txt "HSPUSB 0.2 - USB nativo de Hispa-DOS (UHCI, OHCI, EHCI y xHCI)")
  if [ "$n" = 1 ]; then ok "HSPUSB se presenta una sola vez"; else ko "HSPUSB se presenta $n veces"; fi
  check "Disco USB: 32 MB en la unidad D:"
  check "Hola desde la memoria USB"
  n=$(veces_txt "HSPSND 0.1 - controlador de sonido de Hispa-DOS")
  if [ "$n" = 1 ]; then ok "HSPSND se presenta una sola vez"; else ko "HSPSND se presenta $n veces"; fi
  check "Sound Blaster 16 en 220h, IRQ 5, DMA 1 y 5"
  check "Variable puesta: BLASTER=A220 I5 D1 H5 T6"
  check "BLASTER=A220 I5 D1 H5 T6"
  check "Tono 440 Hz en Sound Blaster 16"
  n=$(grep -c "^  HSPSND " $T/log.txt)
  if [ "$n" = 1 ]; then ok "HSPSND está una sola vez en la memoria (MEM /C)"; else ko "HSPSND está $n veces en la memoria (MEM /C)"; fi
  check "Hello from a .COM program! Args:[ sigue]"
  nocheck "Sistema detenido"
  nocheck "No cabe en el entorno"
  # ¿se ha probado de verdad el caso? (depende de dónde acaben las ROM)
  if sed -n '/memoria superior (UMB):/,$p' $T/log.txt | grep -q "^  HSPSND "; then
    echo "  (aviso: HSPSND ha cabido arriba; el bloque no era pequeño en este QEMU)"
  fi
}
c_pequeno2() {
  n=$(veces_txt "No se ha encontrado ninguna controladora USB.")
  if [ "$n" = 1 ]; then ok "sin xHCI, HSPUSB con DEVICEHIGH lo dice una sola vez"; else ko "sin xHCI, HSPUSB lo dice $n veces"; fi
  # HSPRAM sí se vuelve a cargar abajo cuando arriba no le llega la memoria
  check "Disco RAM de Hispa-DOS: 64 KB en la unidad C:"
  check "Hello from a .COM program! Args:[ sigue]"
}
if quiere pequeno; then
  caso pequeno "$IMG"
  # sin EMS y con D200-DFFF excluido: queda un bloque de unos 20 KB
  CFGP='DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nDEVICE=A:\\HISPADOS\\HSPEMM.SYS NOEMS X=D200-DFFF\r\nDOS=HIGH,UMB\r\n'
  config "$CFGP"'DEVICEHIGH=A:\\HISPADOS\\HSPUSB.SYS /F\r\n'
  mkstick
  arranca -net none -audiodev none,id=a0 -device sb16,audiodev=a0 \
      -device qemu-xhci,id=xhci -drive if=none,id=u,format=raw,file=$T/usb.img -device usb-storage,bus=xhci.0,drive=u
  espera "$P"
  orden 'MEM'
  orden 'TYPE D:\HOLA.TXT'
  orden 'LH A:\HISPADOS\HSPSND /VOL:80'
  orden 'SET'
  orden 'MEM /C'
  orden 'SONAR /TONE 440 200'
  orden 'BIN\HELLO sigue'
  fin c_pequeno
  caso "pequeno, sin xHCI" "$IMG"
  config "$CFGP"'DEVICEHIGH=A:\\HISPADOS\\HSPUSB.SYS\r\nDEVICEHIGH=A:\\HISPADOS\\HSPRAM.SYS 64\r\n'
  arranca -net none
  espera "$P"
  orden 'BIN\HELLO sigue'
  fin c_pequeno2
fi

# ---- usb: la imagen USB con «Juegos» ----
c_usb() {
  check "    2. Juegos (memoria expandida EMS y la mayor memoria libre)"
  check "HSPEMM 0.1: EMS 4.0 y VCPI"
  check "HSPUSB 0.2 - USB nativo de Hispa-DOS (UHCI, OHCI, EHCI y xHCI)"
  check "HSPUSB no toma el controlador"
  check "Bienvenido a Hispa-DOS. Escriba AYUDA para ver las órdenes."
  check "CONFIG=JUEGOS"
  check "Hispa-DOS está cargado en la memoria alta (HMA)"
  check "bytes de memoria superior (UMB) en total"
  check " El volumen de la unidad C es HISPADOS"
  nocheck "Error en CONFIG.SYS"
  nocheck "Comando o nombre de archivo incorrecto"
  for f in CONFIG.SYS AUTOEXEC.BAT; do
    for img in "$DISQ" "$USBIMG@@1048576"; do
      MTOOLS_SKIP_CHECK=1 mtype -i "$img" ::$f > $T/x 2>/dev/null
      if [ $f = CONFIG.SYS ]; then
        if grep -q '^\[menu\]' $T/x && grep -q '^MENUITEM=JUEGOS,' $T/x && grep -q '^MENUDEFAULT=NORMAL,5' $T/x \
           && grep -q '^DOS=HIGH,UMB' $T/x && grep -q 'HSPEMM.SYS' $T/x; then ok "$f con menú en ${img%%@*}"; else ko "$f con menú en ${img%%@*}"; fi
      else
        if grep -q '%CONFIG%' $T/x && grep -q '^:JUEGOS' $T/x && grep -q '^LH .*HSPSND' $T/x; then ok "$f con %CONFIG% en ${img%%@*}"; else ko "$f con %CONFIG% en ${img%%@*}"; fi
      fi
    done
  done
}
if quiere usb; then
  if [ ! -f $USBIMG ] || [ ! -f $DISQ ]; then falta usb "$USBIMG o $DISQ (make usb)"; else
    T=$(mktemp -d)
    cp $USBIMG $T/usbboot.img
    cfail=0
    P='C:\>'
    echo "== Hispa-DOS modo juegos (imagen USB) =="
    rm -f $T/in; mkfifo $T/in
    qemu-system-i386 -display none -serial stdio -monitor none -m 32 \
        -device qemu-xhci,id=xhci -drive if=none,id=usbd,format=raw,file=$T/usbboot.img \
        -device usb-storage,bus=xhci.0,drive=usbd,bootindex=0 -no-reboot < $T/in > $T/log 2>&1 &
    QP=$!
    exec 3> $T/in
    espera 'Elija una opci'
    printf '2\r' >&3
    espera "$P"
    orden 'MEM'
    orden 'SET'
    orden 'DIR C:\LEEME.TXT'
    fin c_usb
  fi
fi

# ---- usbu: la imagen USB con «Juegos» y HSPUSB /U ----
c_usbu() {
  check "HSPEMM 0.1: EMS 4.0 y VCPI"
  check "Disco de arranque USB atendido por HSPUSB (INT 13h)."
  check "Teclado USB preparado."
  check "Bienvenido a Hispa-DOS. Escriba AYUDA para ver las órdenes."
  check "CONFIG=JUEGOS"
  check " Directorio de C:\\NUEVA"
  if grep -q "^Teclado USB va$" $T/log.txt; then ok "lo tecleado por el teclado USB llega"; else ko "lo tecleado por el teclado USB llega"; fi
  dd if=$T/usbboot.img of=$T/p.img bs=1M skip=1 status=none
  if fsck.fat -n $T/p.img > $T/fsck 2>&1; then ok "fsck.fat de la memoria de arranque"; else ko "fsck.fat"; cat $T/fsck; fi
  if cmp -s <(mtype -i $T/p.img ::NUEVA/EDITAR.COM 2>/dev/null) $B/EDIT.COM; then ok "EDITAR.COM copiado intacto en la memoria de arranque"; else ko "copia de EDITAR.COM en la memoria de arranque"; fi
  nocheck "Error al leer"
}
if quiere usbu; then
  if [ ! -f $USBIMG ]; then falta usbu "$USBIMG (make usb)"; else
    T=$(mktemp -d)
    cp $USBIMG $T/usbboot.img
    cfail=0
    P='C:\>'
    echo "== Hispa-DOS modo juegos (imagen USB, HSPUSB /U) =="
    MTOOLS_SKIP_CHECK=1 mtype -i $T/usbboot.img@@1048576 ::CONFIG.SYS | sed 's/^DEVICEHIGH=C:\\HISPADOS\\HSPUSB.SYS/& \/U/' > $T/CONFIG.SYS
    grep -q '^DEVICEHIGH=C:\\HISPADOS\\HSPUSB.SYS /U' $T/CONFIG.SYS || { echo "  FAIL no está la línea de HSPUSB en [juegos]"; cfail=1; }
    MTOOLS_SKIP_CHECK=1 mcopy -o -i $T/usbboot.img@@1048576 $T/CONFIG.SYS ::CONFIG.SYS
    # sin TECLADO SP: las teclas de QEMU son las del teclado americano
    MTOOLS_SKIP_CHECK=1 mtype -i $T/usbboot.img@@1048576 ::AUTOEXEC.BAT | grep -a -v 'TECLADO SP' > $T/AUTOEXEC.BAT
    MTOOLS_SKIP_CHECK=1 mcopy -o -i $T/usbboot.img@@1048576 $T/AUTOEXEC.BAT ::AUTOEXEC.BAT
    Q="python3 tests/tools/qmp.py $T/qmp"
    rm -f $T/in; mkfifo $T/in
    qemu-system-i386 -display none -serial stdio -monitor none -m 32 -qmp unix:$T/qmp,server,nowait \
        -vga none -device VGA,id=vga0 -device qemu-xhci,id=xhci \
        -device usb-kbd,bus=xhci.0,id=ukbd,display=vga0 \
        -drive if=none,id=usbd,format=raw,file=$T/usbboot.img \
        -device usb-storage,bus=xhci.0,drive=usbd,bootindex=0 -no-reboot < $T/in > $T/log 2>&1 &
    QP=$!
    exec 3> $T/in
    espera 'Elija una opci'
    printf '2\r' >&3
    espera "$P"
    orden 'SET'
    orden 'MD C:\NUEVA'
    orden 'COPY C:\HISPADOS\EDITAR.COM C:\NUEVA'
    orden 'DIR C:\NUEVA'
    n=$(veces "$P")
    $Q type vga0 "ECHO Teclado USB va" key vga0 ret
    espera "$P" $((n + 1))
    fin c_usbu
  fi
fi
exit $fail
