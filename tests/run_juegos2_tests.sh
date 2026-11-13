#!/bin/bash
# Prueba de la segunda tanda de juegos de Hispa Ventanas (.HVP): Bloques,
# Solitario, Tragabolas y Almacén. Como tests/run_juegos_tests.sh: arranca en
# QEMU, maneja el entorno con el ratón y el teclado del monitor de QEMU, saca
# capturas y las comprueba punto a punto (tests/tools/ventanas_check.py); los
# archivos de récords se miran en la imagen con mtools.
#  - «grupo» (tests/juegos2/grupo.sh): sin CENTRO.INI, el grupo «Juegos» trae
#    un acceso por cada .HVP de \HISPADOS\JUEGOS, con su nombre y su icono;
#    cada juego nuevo se abre y se cierra; CENTRO.INI y la memoria al salir;
#  - una sesión por juego (tests/juegos2/<juego>.sh), con un CENTRO.INI hecho
#    a mano que le pasa los parámetros de prueba (/S=n semilla, /P prueba):
#    se juegan unos pasos con el teclado y el ratón, se mira la pantalla y
#    que los récords quedan en el disco.
# uso: run_juegos2_tests.sh [imagen]      SOLO="bloques almacen" para algunas
# Deja capturas en build/juegos2_*.png.
# Hispa Ventanas games test, second batch (HVP modules).
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
B=build
T=$(mktemp -d)
CHK="python3 tests/tools/ventanas_check.py"
HVP2="BLOQUES.HVP SOLITARI.HVP TRAGABOL.HVP ALMACEN.HVP"
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() { if grep -qF -- "$1" $T/log.txt; then ok "$1"; else ko "$1"; fi; }
checkn() { if grep -qF -- "$1" $T/log.txt; then ko "(no debía salir) $1"; else ok "sin \"$1\""; fi; }
ini() { if grep -qxF -- "$1" $T/ini.txt; then ok "CENTRO.INI: $1"; else ko "CENTRO.INI sin la línea «$1»"; fi; }
# rec ARCHIVO EXPRESIÓN: el archivo de \HISPADOS\JUEGOS tiene una línea así
rec() { if mtype -i $T/f.img "::HISPADOS/JUEGOS/$1" 2>/dev/null | tr -d '\r' | iconv -f CP437 -t UTF-8 | grep -qxE -- "$2"; then ok "$1: $2"; else ko "$1 sin la línea «$2»: $(mtype -i $T/f.img "::HISPADOS/JUEGOS/$1" 2>&1 | tr -d '\r' | iconv -f CP437 -t UTF-8 | tr '\n' ' ')"; fi; }
chk() { $CHK "$@" || fail=1; }
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; sleep "${2:-0.3}"; }
tecla() { local k; for k in "$@"; do mon "sendkey $k" 0.3; done; }
shot() { sleep 0.4; mon "screendump $T/$1.ppm" 0.6; }
send() { printf '%s\r' "$1"; sleep "${2:-1.5}"; }
muestra() { [ -f $T/$1.ppm ] && convert $T/$1.ppm $B/juegos2_$2.png; }
RX=400; RY=300
ir() {
  local dx=$(( $1 - RX )) dy=$(( $2 - RY )) sx sy
  while [ $dx -ne 0 ] || [ $dy -ne 0 ]; do
    sx=$dx; [ $sx -gt 60 ] && sx=60; [ $sx -lt -60 ] && sx=-60
    sy=$dy; [ $sy -gt 60 ] && sy=60; [ $sy -lt -60 ] && sy=-60
    mon "mouse_move $sx $sy" 0.07
    dx=$(( dx - sx )); dy=$(( dy - sy ))
  done
  RX=$1; RY=$2; sleep 0.3
}
clic() { ir $1 $2; mon "mouse_button 1" 0.25; mon "mouse_button 0" 0.5; }
derecho() { ir $1 $2; mon "mouse_button 2" 0.25; mon "mouse_button 0" 0.5; }
doble() { ir $1 $2; mon "mouse_button 1" 0.1; mon "mouse_button 0" 0.1; mon "mouse_button 1" 0.1; mon "mouse_button 0" 0.8; }
# arrastra X0 Y0 X1 Y1: aprieta en un sitio y suelta en otro
arrastra() { ir $1 $2; mon "mouse_button 1" 0.3; ir $3 $4; mon "mouse_button 0" 0.5; }
aparca() { ir 16 320; }
run() {
  local tmo=$1; shift
  rm -f $T/mon
  timeout $tmo qemu-system-i386 -display none -serial stdio -monitor unix:$T/mon,server,nowait \
    -parallel none -m 16 -drive file=$T/f.img,format=raw,if=floppy,index=0 -boot a -no-reboot "$@" > $T/raw 2>&1
  tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 | grep -v '^audio:' > $T/log.txt
  mtype -i $T/f.img ::HISPADOS/CENTRO.INI 2>/dev/null | tr -d '\r' | iconv -f CP437 -t UTF-8 > $T/ini.txt
}
xms_igual() {
  local a b
  a=$(grep "largest free KB" $T/log.txt | head -1); b=$(grep "largest free KB" $T/log.txt | tail -1)
  if [ -n "$a" ] && [ "$a" = "$b" ] && [ "$(grep -c 'largest free KB' $T/log.txt)" = 2 ]; then
    ok "memoria extendida igual al final ($(echo $b | sed 's/.*largest/mayor bloque/'))"
  else
    ko "memoria extendida: «$a» y «$b»"
  fi
}
printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nFILES=30\r\n' > $T/CONFIG.SYS

# ---- lo común de las sesiones ----
# disco: una copia de la imagen en $T/f.img con CONFIG.SYS (mientras se hace
# un juego, su .HVP puede no estar todavía en la imagen: se copia de build/)
disco() {
  local h
  cp "$IMG" $T/f.img
  mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
  for h in $HVP2; do
    if ! mdir -i $T/f.img ::HISPADOS/JUEGOS/$h > /dev/null 2>&1 && [ -f $B/$h ]; then
      mcopy -i $T/f.img $B/$h ::HISPADOS/JUEGOS/$h
    fi
  done
  NMOD=$(mdir -b -i $T/f.img ::HISPADOS/JUEGOS ::HISPADOS/PROGS 2>/dev/null | grep -c '\.HVP')
  RX=400; RY=300
}
# centro_ini LÍNEA...: CENTRO.INI con el grupo «Juegos» abierto abajo (en
# 300,380: los iconos, cada 76 puntos desde x=338) y los accesos que se den:
#   centro_ini '[Acceso]' 'Nombre=Bloques' 'Programa=A:\HISPADOS\JUEGOS\BLOQUES.HVP' 'Parametros=/S=7 /P'
centro_ini() {
  printf '%s\r\n' '[Centro]' 'Ventana=36,52,452,190' 'Activo=0' "Modulos=$NMOD" '' \
    '[Grupo]' 'Nombre=Juegos' 'Ventana=300,380,452,190' 'Estado=1' '' "$@" > $T/CENTRO.INI
  mcopy -o -i $T/f.img $T/CENTRO.INI ::HISPADOS/CENTRO.INI
}
# dentro de «{ ...; } | run 200 -vga std»: entrar en Hispa Ventanas y salir
entrar() {
  sleep 6
  send 'KEYB SP' 2
  send 'A:\HISPADOS\HSPRATON' 2
  send 'BIN\XMSTEST' 2.5
  send 'VENTANAS' 5
  aparca
}
salir() {
  clic 756 13; sleep 2.5
  shot zz
  send 'ECHO TRAS-SALIR'
  send 'BIN\XMSTEST' 2.5
  send 'ECHO SIGUE-VIVO'
  mon quit
}
# después de run: ha salido bien y la memoria está como al entrar
salida_bien() {
  chk $T/zz.ppm tam 720 400
  check "Hispa Ventanas ha terminado."
  check "TRAS-SALIR"
  check "SIGUE-VIVO"
  checkn "HSPDPMI: excepción"
  xms_igual
  if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/Ejecutando/,$p' $T/log.txt; cat $T/ini.txt; fi
  [ -n "${GUARDAR:-}" ] && cp $T/*.ppm "$GUARDAR"/
  rm -f $T/*.ppm
}

for parte in ${SOLO:-grupo bloques solitario tragabolas almacen}; do
  if [ -f tests/juegos2/$parte.sh ]; then
    . tests/juegos2/$parte.sh
  else
    ko "no existe tests/juegos2/$parte.sh"
  fi
done
rm -rf $T
exit $fail
