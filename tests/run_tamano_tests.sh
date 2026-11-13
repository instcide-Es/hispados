#!/bin/bash
# [TAMAÑO] Prueba de Hispa Ventanas: cambiar el tamaño de las ventanas con el
# ratón y las barras de desplazamiento. Arranca en QEMU con HSPRATON por la
# vía directa (IntelliMouse: con rueda), un CENTRO.INI con un grupo de 14
# accesos directos, y maneja el entorno con el ratón y el teclado del monitor
# de QEMU (capturas comprobadas con tests/tools/ventanas_check.py):
#  1.ª sesión:
#  - el grupo se alarga hasta abajo y, como no caben los iconos, sale la
#    barra vertical;
#  - el cursor cambia de forma en un borde (doble flecha ↔, agarrando por
#    fuera de la ventana) y en una esquina (en diagonal);
#  - arrastrar el borde derecho y la esquina de arriba a la izquierda: el
#    tamaño nuevo, y los iconos en dos columnas;
#  - desplazar arrastrando el pulgar de la barra (se ve el icono que estaba
#    oculto), con Inicio, con la rueda del ratón y con Fin y RePág;
#  - al salir, CENTRO.INI guarda el tamaño y «Medida=1».
#  2.ª sesión (VENTANAS /CONTORNO):
#  - el grupo sale con el tamaño que se le dio (no se alarga otra vez);
#  - al arrastrar un borde solo se mueve el contorno; al soltar, la ventana
#    cambia; y se recuerda.
# Deja capturas en build/tamano_*.png (y en capturas/ si existe).
# Hispa Ventanas test: resizing windows with the mouse, scroll bars, wheel.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
B=build
T=$(mktemp -d)
CHK="python3 tests/tools/ventanas_check.py"
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() { if grep -qF -- "$1" $T/log.txt; then ok "$1"; else ko "$1"; fi; }
checkn() { if grep -qF -- "$1" $T/log.txt; then ko "(no debía salir) $1"; else ok "sin \"$1\""; fi; }
ini() { if grep -qxF -- "$1" $T/ini.txt; then ok "CENTRO.INI: $1"; else ko "CENTRO.INI sin la línea «$1»"; fi; }
chk() { $CHK "$@" || fail=1; }
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; sleep "${2:-0.3}"; }
tecla() { local k; for k in "$@"; do mon "sendkey $k" 0.3; done; }
shot() { sleep 0.5; mon "screendump $T/$1.ppm" 0.6; }
send() { printf '%s\r' "$1"; sleep "${2:-1.5}"; }
muestra() {
  [ -f $T/$1.ppm ] || return
  convert $T/$1.ppm $B/tamano_$2.png
  [ -d capturas ] && cp $B/tamano_$2.png capturas/
}
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
doble() { ir $1 $2; mon "mouse_button 1" 0.1; mon "mouse_button 0" 0.1; mon "mouse_button 1" 0.1; mon "mouse_button 0" 0.8; }
arrastra() { ir $1 $2; mon "mouse_button 1" 0.4; ir $3 $4; mon "mouse_button 0" 0.6; }
aparca() { ir 600 100; }
rueda() { local i; for ((i=0; i<$2; i++)); do mon "mouse_move 0 0 $1" 0.3; done; sleep 0.3; }
run() {
  local tmo=$1; shift
  rm -f $T/mon
  timeout $tmo qemu-system-i386 -display none -serial stdio -monitor unix:$T/mon,server,nowait \
    -parallel none -m 16 -drive file=$T/f.img,format=raw,if=floppy,index=0 -boot a -no-reboot "$@" > $T/raw 2>&1
  tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 > $T/log.txt
  mtype -i $T/f.img ::HISPADOS/CENTRO.INI 2>/dev/null | tr -d '\r' | iconv -f CP437 -t UTF-8 > $T/ini.txt
}

echo "== Hispa Ventanas: cambiar el tamaño, barras de desplazamiento y rueda =="
cp "$IMG" $T/f.img
mdel -i $T/f.img '::HISPADOS/JUEGOS/*.HVP'    # (sin módulos: no entran en el grupo)
mdel -i $T/f.img '::HISPADOS/PROGS/*.HVP'
printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nFILES=30\r\n' > $T/CONFIG.SYS
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
{
  printf '%s\r\n' '[Centro]' 'Ventana=36,52,452,190' 'Activo=0' 'Modulos=0' '' \
    '[Grupo]' 'Nombre=Muchos' 'Ventana=100,300,460,200' 'Estado=1' 'Marcado=0'
  printf '%s\r\n' '' '[Acceso]' 'Nombre=Acceso 01' 'Programa=:BLOC'
  for i in 02 03 04 05 06 07 08 09 10 11 12 13 14; do
    printf '%s\r\n' '' '[Acceso]' "Nombre=Acceso $i" 'Programa=A:\COMMAND.COM'
  done
} > $T/CENTRO.INI
mcopy -o -i $T/f.img $T/CENTRO.INI ::HISPADOS/CENTRO.INI
{ sleep 6
  send 'KEYB SP' 2
  send 'A:\HISPADOS\HSPDPMI' 2
  send 'A:\HISPADOS\HSPRATON /DIRECTO' 3
  send 'VENTANAS' 5
  shot a1
  # ---- el cursor en el borde derecho (agarrando por fuera) y arrastrarlo ----
  ir 561 450
  shot a2
  arrastra 561 450 351 450
  aparca
  shot a3
  # ---- la esquina de arriba a la izquierda ----
  ir 98 298
  shot a4
  arrastra 98 298 78 238
  aparca
  shot a5
  # ---- desplazar: el pulgar, Inicio, la rueda, Fin y RePág ----
  arrastra 341 380 341 560
  aparca
  shot a6
  clic 132 560                              # (un icono, para el teclado)
  tecla home
  aparca
  shot a7
  ir 150 420
  rueda -1 3
  aparca
  shot a8
  tecla end
  shot a9
  tecla pgup
  shot a10
  # ---- salir: CENTRO.INI ----
  clic 756 13; sleep 1; tecla ret; sleep 2.5
  # ---- 2.ª sesión: el tamaño se recuerda; solo el contorno ----
  send 'VENTANAS /CONTORNO' 5
  RX=400; RY=300
  shot b1
  ir 353 450
  mon "mouse_button 1" 0.4
  ir 453 450
  shot b2
  mon "mouse_button 0" 0.8
  aparca
  shot b3
  # ---- el Bloc de notas: la barra horizontal ----
  doble 144 300; sleep 1.5
  for i in 1 2 3; do tecla a b c d e f g h i j k l m n o p q r s t u v w x y z; done
  sleep 0.8
  aparca
  shot c1
  tecla home; sleep 0.8
  shot c2
  for i in 1 2 3 4 5; do clic 660 420; done
  aparca
  shot c3
  clic 756 13; sleep 1; tecla ret; sleep 2.5
  send 'ECHO SIGUE-VIVO'
  mon quit
} | run 240

check "Teclado español instalado."
check "Ratón PS/2 con rueda (IntelliMouse)"
# -- el grupo, alargado hasta abajo, con la barra (aún hay sitio para 4 columnas) --
chk $T/a1.ppm tam 800 600 marco 100 300 460 300 activa titulo 100 300 activa "Muchos" \
  px 545 348 6 "flecha de arriba de la barra" px 549 353 0 "triángulo de la flecha de arriba" \
  px 544 588 6 "flecha de abajo de la barra" px 544 420 6 "el pulgar" px 545 570 15 "la pista, debajo del pulgar" \
  texto 234 403 0 8 fina "Acceso 02" texto 442 403 0 8 fina "Acceso 04"
# -- el cursor en el borde (agarrado por fuera): doble flecha izquierda-derecha --
chk $T/a2.ppm px 561 450 10 "cursor ↔: el centro, ocre" px 554 450 0 "cursor ↔: la punta izquierda" \
  px 568 450 0 "cursor ↔: la punta derecha" px 558 446 0 "cursor ↔: el filo de la cabeza"
# -- borde derecho arrastrado: 252 de ancho, dos columnas y la barra a la derecha --
chk $T/a3.ppm marco 100 300 252 300 activa px 400 450 2 "fondo donde llegaba la ventana" \
  px 336 348 6 "la barra, en el borde nuevo" \
  texto 234 403 0 8 fina "Acceso 02" texto 130 479 0 8 fina "Acceso 03"
# -- la esquina de arriba a la izquierda: en diagonal --
chk $T/a4.ppm px 98 298 10 "cursor en diagonal: el centro, ocre" px 94 293 0 "cursor en diagonal: arriba a la izquierda" \
  px 101 302 10 "cursor en diagonal: abajo a la derecha, ocre"
# -- esquina arrastrada: 78,238 de 274x362; los últimos iconos, ocultos --
chk $T/a5.ppm marco 78 238 274 362 activa titulo 78 238 activa "Muchos" \
  texto 108 417 0 8 fina "Acceso 03" px 336 380 6 "el pulgar, arriba" px 336 500 15 "la pista, abajo" \
  cursor 600 100
# -- el pulgar, arrastrado abajo del todo: se ve «Acceso 14» --
chk $T/a6.ppm texto 212 572 0 8 fina "Acceso 14" texto 108 572 0 8 fina "Acceso 13" \
  px 336 330 15 "la pista, arriba" px 336 500 6 "el pulgar, abajo"
# -- Inicio: arriba otra vez --
chk $T/a7.ppm texto 108 417 0 8 fina "Acceso 03" px 336 380 6 "el pulgar, arriba (Inicio)" \
  px 336 540 15 "la pista, abajo (Inicio)"
# -- la rueda, tres pasos hacia abajo: 114 puntos --
chk $T/a8.ppm texto 108 303 0 8 fina "Acceso 03" px 336 320 15 "la pista, arriba (rueda)" \
  px 336 500 6 "el pulgar, a media barra (rueda)" cambia 82 285 250 315 $T/a7.ppm 500
# -- Fin: el último, marcado y a la vista --
chk $T/a9.ppm texto 108 580 0 8 fina "Acceso 13" px 336 500 6 "el pulgar, abajo (Fin)"
# -- RePág: una página arriba --
chk $T/a10.ppm cambia 82 285 250 315 $T/a9.ppm 500 px 336 330 15 "la pista, arriba (RePág)" \
  px 336 400 6 "el pulgar sube (RePág)"
# -- CENTRO.INI de la 2.ª sesión (lo último que se escribió) --
ini "Nombre=Muchos"; ini "Ventana=78,238,376,362"; ini "Medida=1"
# -- 2.ª sesión: el tamaño de antes, sin alargarse --
chk $T/b1.ppm marco 78 238 274 362 activa titulo 78 238 activa "Muchos" texto 108 417 0 8 fina "Acceso 03"
# -- solo el contorno: la ventana no cambia hasta soltar --
chk $T/b2.ppm titulo 78 238 activa "Muchos" px 351 300 1 "el borde de la ventana, donde estaba" px 452 300 10 "contorno: la línea ocre, a la derecha" \
  px 453 300 0 "contorno: el filo de fuera" px 451 300 0 "contorno: el filo de dentro" \
  px 300 239 10 "contorno: la línea ocre, arriba" px 400 350 2 "el fondo, dentro del contorno"
chk $T/b3.ppm marco 78 238 376 362 activa px 452 300 1 "el borde nuevo, sin contorno"
# -- el Bloc de notas: la barra horizontal sigue al texto y lo desplaza --
chk $T/c1.ppm titulo 150 90 activa "Bloc de notas - (sin nombre)" texto 156 138 0 9 fina "stuvwxyz" \
  px 640 420 6 "barra horizontal: el pulgar, a la derecha" px 200 420 15 "barra horizontal: la pista, a la izquierda" \
  px 655 415 6 "barra horizontal: la flecha de la derecha"
chk $T/c2.ppm texto 164 138 0 9 fina "bcdefghi" px 200 420 6 "Inicio: el pulgar, a la izquierda" \
  px 640 420 15 "Inicio: la pista, a la derecha"
chk $T/c3.ppm texto 156 138 0 9 fina "klmnopqr" px 200 420 15 "cinco flechas: el pulgar se ha movido"
check "SIGUE-VIVO"
checkn "HSPDPMI: excepción"
n=$(grep -c "Hispa Ventanas ha terminado." $T/log.txt)
if [ "$n" = 2 ]; then ok "dos sesiones terminadas con su mensaje"; else ko "sesiones terminadas: $n (esperadas 2)"; fi
muestra a1 grupo_barra; muestra a2 cursor_borde; muestra a3 borde_derecho; muestra a4 cursor_esquina
muestra a5 esquina; muestra a6 pulgar_abajo; muestra a7 inicio; muestra a8 rueda; muestra a9 fin
muestra b1 recordado; muestra b2 contorno; muestra b3 contorno_soltado
muestra c1 bloc_horizontal; muestra c3 bloc_desplazado
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/Ejecutando/,$p' $T/log.txt | tail -40; fi
[ -n "${GUARDAR:-}" ] && cp $T/*.ppm "$GUARDAR"/
rm -rf $T
exit $fail
