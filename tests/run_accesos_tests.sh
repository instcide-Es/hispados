#!/bin/bash
# Prueba de Hispa Ventanas: los grupos ligados a una carpeta de juegos y las
# opciones de juego de un acceso directo (velocidad, LOADFIX y sonido).
# Arranca en QEMU desde el disquete con un disco duro que trae C:\JUEGOS,
# maneja el entorno con el ratón y el teclado del monitor de QEMU, saca
# capturas y las comprueba (tests/tools/ventanas_check.py). Los «juegos» son
# copias de tests/progs/accjuego.asm: dicen su ruta, su PSP, sus parámetros,
# la velocidad que miden con un bucle y si hay un VELOCIDAD frenando.
# QEMU va con -icount (el tiempo virtual sale de las instrucciones
# ejecutadas): las velocidades medidas no dependen de lo cargado que esté el
# equipo que hace la prueba.
#  1.ª sesión (sin CENTRO.INI; C:\JUEGOS con cinco juegos):
#  - el grupo «Juegos de DOS» sale solo, ligado a C:\JUEGOS, con un acceso
#    por juego: el programa que se llama como su carpeta (sin SETUP), el
#    único .COM, el mayor de dos .EXE, un SETUP que es lo único que hay y un
#    programa suelto;
#  - propiedades: velocidad 386 y LOADFIX en uno; se guarda en CENTRO.INI;
#  - se lanza: corre frenado, por encima de 1000h y con sus parámetros; al
#    volver, la velocidad es la normal (±10 %) y no queda VELOCIDAD;
#  - uno al 25 % que se acaba con Ctrl+C, y otro que deja un residente en la
#    INT 08h (VELOCIDAD no puede soltarla: Hispa Ventanas lo avisa): en los
#    dos la velocidad vuelve a ser la normal (±10 %);
#  - «Carpeta de juegos...» enseña la carpeta del grupo; un acceso de la
#    carpeta borrado a mano no vuelve a salir.
#  2.ª sesión (se quitan dos juegos del disco y se añade uno; antes de entrar
#  se deja puesto VELOCIDAD 25%):
#  - sale el juego nuevo, desaparece el que nadie había tocado y el que se
#    cambió a mano se queda con un aspa; el borrado sigue sin salir;
#  - un juego con velocidad: al volver, Hispa Ventanas quita el freno que
#    quedaba (la segunda red: VELOCIDAD NORMAL) y la velocidad es la normal.
#  3.ª sesión (CENTRO.INI viejo, escrito a mano, sin las claves nuevas, y sin
#  VELOCIDA.COM en el disco):
#  - los grupos y accesos de antes siguen igual y el grupo «Juegos de DOS» se
#    añade; un acceso con velocidad avisa de que falta VELOCIDA.COM; con
#    LOADFIX solo, el programa se carga por encima de 1000h.
#  4.ª sesión (con HSPEMM y una tarjeta AC'97): un acceso con «Sonido=sb»
#  carga HSPSND /SB antes del juego, que ve la variable BLASTER.
# Deja capturas en build/accesos_*.png.
# Hispa Ventanas test: game folders and per-shortcut game options (speed, LOADFIX).
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
B=$(dirname "$IMG")
[ -f "$B/ACCJUEGO.COM" ] || make -s B="$B" "$B/ACCJUEGO.COM" >/dev/null 2>&1 || { echo "falta $B/ACCJUEGO.COM (make build/ACCJUEGO.COM)"; exit 1; }
T=$(mktemp -d)
CHK="python3 tests/tools/ventanas_check.py"
ICOUNT="-icount shift=6 -rtc clock=vm"
[ "${ICOUNT_OFF:-}" = 1 ] && ICOUNT=""
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() { if grep -qF -- "$1" $T/log.txt; then ok "$1"; else ko "$1"; fi; }
checkn() { if grep -qF -- "$1" $T/log.txt; then ko "(no debía salir) $1"; else ok "sin \"$1\""; fi; }
ini() { if grep -qxF -- "$1" $T/ini.txt; then ok "CENTRO.INI: $1"; else ko "CENTRO.INI sin la línea «$1»"; fi; }
inin() { if grep -qxF -- "$1" $T/ini.txt; then ko "CENTRO.INI: sobra la línea «$1»"; else ok "CENTRO.INI sin «$1»"; fi; }
chk() { $CHK "$@" || fail=1; }
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; sleep "${2:-0.3}"; }
tecla() { local k; for k in "$@"; do mon "sendkey $k" 0.35; done; }
shot() { sleep 0.6; mon "screendump $T/$1.ppm" 0.6; }
send() { printf '%s\r' "$1"; sleep "${2:-1.5}"; }
muestra() { [ -f $T/$1.ppm ] && convert $T/$1.ppm $B/accesos_$2.png; }
RX=400; RY=300
ir() {
  local dx=$(( $1 - RX )) dy=$(( $2 - RY )) sx sy
  while [ $dx -ne 0 ] || [ $dy -ne 0 ]; do
    sx=$dx; [ $sx -gt 60 ] && sx=60; [ $sx -lt -60 ] && sx=-60
    sy=$dy; [ $sy -gt 60 ] && sy=60; [ $sy -lt -60 ] && sy=-60
    mon "mouse_move $sx $sy" 0.09
    dx=$(( dx - sx )); dy=$(( dy - sy ))
  done
  RX=$1; RY=$2; sleep 0.4
}
clic() { ir $1 $2; mon "mouse_button 1" 0.3; mon "mouse_button 0" 0.6; }
aparca() { ir 16 320; }
centro() { RX=400; RY=300; }
escribe() {
  local t="$1" i c
  for ((i=0; i<${#t}; i++)); do
    c="${t:$i:1}"
    case "$c" in
      ' ') tecla spc;;
      '/') tecla shift-7;;        # (teclado español)
      [A-Z]) tecla "shift-$(echo $c | tr A-Z a-z)";;
      *) tecla "$c";;
    esac
  done
}
# espera (hasta $2 segundos) a que el texto $1 salga $3 veces en lo que
# escribe la máquina
espera() {
  local i
  for i in $(seq 1 $(( ${2:-30} * 5 ))); do
    [ "$(tr -d '\r' < $T/raw | grep -a -c -- "$1")" -ge "${3:-1}" ] && return 0
    sleep 0.2
  done
  return 1
}
run() {
  local tmo=$1; shift
  rm -f $T/mon
  timeout $tmo qemu-system-i386 -display none -serial stdio -monitor unix:$T/mon,server,nowait \
    -parallel none -m 16 -drive file=$T/f.img,format=raw,if=floppy,index=0 \
    -drive file=$T/c.img,format=raw,if=ide -boot a -no-reboot -vga std $ICOUNT "$@" > $T/raw 2>&1
  tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 | grep -v '^audio:' > $T/log.txt
  mtype -i $T/f.img ::HISPADOS/CENTRO.INI 2>/dev/null | tr -d '\r' | iconv -f CP437 -t UTF-8 > $T/ini.txt
}
# la n-ésima línea «JUEGO» y sus campos
juego() { grep '^JUEGO PROG=' $T/log.txt | sed -n "$1p"; }
campo() { juego "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
# ¿está a/b entre lo y hi por ciento?
entre() {   # a b lo hi texto
  local a=${1:-0} b=${2:-0}
  if [ "$b" -gt 0 ] && [ $((a * 100)) -ge $((b * $3)) ] && [ $((a * 100)) -le $((b * $4)) ]; then
    ok "$5 ($a de $b: $((a * 100 / b)) %)"
  else
    ko "$5 ($a de $b; tenía que estar entre el $3 y el $4 %)"
  fi
}
es() { if [ "$(campo "$1" "$2")" = "$3" ]; then ok "$4"; else ko "$4 ($2=$(campo "$1" "$2"); línea: $(juego "$1"))"; fi; }
HD="$T/c.img@@32256"

echo "== Hispa Ventanas: el grupo de la carpeta de juegos, velocidad y LOADFIX =="
cp "$IMG" $T/f.img
mdel -i $T/f.img '::HISPADOS/JUEGOS/*.HVP'    # (sin los juegos propios ni Pintura: tienen sus pruebas)
mdel -i $T/f.img '::HISPADOS/PROGS/*.HVP'
# (DOS=HIGH: sin LOADFIX, un programa pequeño cae en los primeros 64 KB)
printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nDOS=HIGH\r\nFILES=30\r\n' > $T/CONFIG.SYS
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
# C:\JUEGOS: UNO (el que se llama como la carpeta, con un SETUP), DOS (el
# único .COM), TRES (dos .EXE: el mayor), SOLO (solo hay un SETUP) y un
# programa suelto; un LEEME y un archivo de datos no cuentan
J=$B/ACCJUEGO.COM
mkdir -p $T/hd/JUEGOS/UNO $T/hd/JUEGOS/DOS $T/hd/JUEGOS/TRES $T/hd/JUEGOS/SOLO
cp $J $T/hd/JUEGOS/UNO/UNO.EXE; cp $J $T/hd/JUEGOS/UNO/SETUP.EXE; cp $J $T/hd/JUEGOS/UNO/OTRO.EXE
cp $J $T/hd/JUEGOS/DOS/JUEGO.COM; echo datos > $T/hd/JUEGOS/DOS/DATOS.DAT
cp $J $T/hd/JUEGOS/TRES/A.EXE; { cat $J; head -c 300 /dev/zero; } > $T/hd/JUEGOS/TRES/GRANDE.EXE
cp $J $T/hd/JUEGOS/SOLO/SETUP.EXE
cp $J $T/hd/JUEGOS/SUELTO.COM; echo léame > $T/hd/JUEGOS/LEEME.TXT; cp $J $T/hd/JUEGOS/INSTALAR.EXE
python3 tests/tools/mkhdd.py $T/c.img 16 > /dev/null
# (en este orden: es el de los iconos del grupo)
for d in UNO DOS TRES SOLO; do mcopy -s -i $HD $T/hd/JUEGOS/$d ::JUEGOS/ 2>/dev/null || { mmd -i $HD ::JUEGOS; mcopy -s -i $HD $T/hd/JUEGOS/$d ::JUEGOS/; }; done
mcopy -i $HD $T/hd/JUEGOS/SUELTO.COM $T/hd/JUEGOS/LEEME.TXT $T/hd/JUEGOS/INSTALAR.EXE ::JUEGOS/
# vuelta(n): espera a que el lanzador haya cargado n veces el entorno gráfico
vuelta() { espera 'cargando el servidor DPMI' 60 $1; sleep 6; centro; aparca; }
{ espera 'A:\\>' 40
  send 'KEYB SP' 3
  send 'A:\HISPADOS\HSPRATON' 3
  send 'C:\JUEGOS\DOS\JUEGO' 4                 # (1) la velocidad sin Hispa Ventanas
  send 'VENTANAS' 3
  vuelta 1
  shot a1
  # ---- el grupo «Juegos de DOS», que ha salido solo ----
  tecla right right right ret; sleep 2
  aparca
  shot a2
  # ---- (2) «Dos», sin opciones: la velocidad normal desde un acceso ----
  tecla right ret
  espera '^JUEGO PROG' 40 2; vuelta 2
  # ---- propiedades de «Uno»: parámetros, 386 y LOADFIX ----
  tecla left
  tecla alt-a down down down ret; sleep 1.5   # Archivo > Propiedades...
  shot a3
  tecla tab tab tab tab
  escribe "uno dos"
  clic 489 393                                # opción «386»
  clic 183 444                                # casilla LOADFIX
  aparca
  shot a4
  tecla ret; sleep 1.5
  # ---- (3) se lanza: VELOCIDA 386 LOADFIX UNO.EXE uno dos ----
  tecla ret
  espera '^JUEGO PROG' 60 3; vuelta 3
  shot a5
  # ---- (4) «Dos» otra vez: la velocidad, la normal ----
  tecla right ret
  espera '^JUEGO PROG' 40 4; vuelta 4
  # ---- «Tres»: al 25 % y esperando una tecla; se acaba con Ctrl+C ----
  tecla right
  tecla alt-a down down down ret; sleep 1.5
  tecla tab tab tab tab
  escribe "/C"
  clic 283 417                                # opción «Otra:»
  clic 364 417                                # su cuadro: 25
  tecla end backspace backspace 2 5
  aparca
  shot a6
  tecla ret; sleep 1.5
  tecla ret                                   # (5)
  espera 'JUEGO: espera' 60 1; sleep 1
  tecla ctrl-c
  vuelta 5
  # ---- (6) «Dos» ----
  tecla left ret
  espera '^JUEGO PROG' 40 6; vuelta 6
  # ---- «Suelto»: como un 286, y deja un residente en la INT 08h ----
  tecla right right right
  tecla alt-a down down down ret; sleep 1.5
  tecla tab tab tab tab
  escribe "/T"
  clic 539 393                                # opción «286»
  aparca
  tecla ret; sleep 1.5
  tecla ret                                   # (7)
  espera 'JUEGO: residente' 60 1; vuelta 7
  shot a7                                     # el aviso de Hispa Ventanas
  tecla ret; sleep 1.5
  # ---- (8) «Dos» ----
  tecla left left left ret
  espera '^JUEGO PROG' 40 8; vuelta 8
  # ---- «Suelto» se borra a mano; la carpeta del grupo ----
  tecla right right right delete; sleep 1.5
  tecla ret; sleep 1.5
  shot a8
  tecla alt-a up up up ret; sleep 1.5         # Archivo > Carpeta de juegos...
  shot a9
  tecla esc; sleep 1
  # ---- salir ----
  clic 756 13; sleep 4
  send 'ECHO TRAS-SALIR' 2
  send 'C:\JUEGOS\DOS\JUEGO' 4                 # (9)
  send 'A:\HISPADOS\VELOCIDA' 5
  send 'ECHO SIGUE-VIVO' 2
  mon quit
} | run 700
# -- el grupo ha salido solo, con un acceso por juego --
chk $T/a1.ppm tam 800 600 marco 36 52 452 190 activa titulo 36 52 activa "Centro de programas" \
  texto 286 155 0 8 fina "Juegos" texto 362 155 0 8 fina "Juegos de DOS"
chk $T/a2.ppm titulo 272 212 activa "Juegos de DOS" titulo 36 52 no "Centro de programas" \
  texto 326 315 9 12 fina "Uno" texto 430 315 0 8 fina "Dos" texto 530 315 0 8 fina "Tres" texto 634 315 0 8 fina "Solo" \
  texto 314 391 0 8 fina "Suelto"
# -- propiedades: las opciones de juego --
chk $T/a3.ppm marco 158 76 484 448 activa titulo 158 76 activa "Propiedades del acceso directo" \
  texto 176 363 0 8 negrita "Opciones para juegos de DOS" texto 176 385 0 8 fina "Velocidad:" \
  px 283 393 4 "velocidad «Normal» marcada" px 489 393 9 "«386» sin marcar" px 183 444 9 "casilla LOADFIX sin marcar" \
  texto 198 461 7 8 fina "Sonido: Sound Blaster emulada (hace falta HSPEMM)"
chk $T/a4.ppm px 283 393 9 "«Normal» ya no está marcada" px 489 393 4 "«386» marcada" px 183 444 4 "casilla LOADFIX marcada"
chk $T/a6.ppm px 283 417 4 "«Otra:» marcada" px 283 393 9 "«Normal» sin marcar"
# -- los juegos --
v0=$(campo 1 VEL)
if [ "${v0:-0}" -gt 0 ]; then ok "velocidad sin freno, desde Hispa-DOS: $v0"; else ko "no hay velocidad de referencia: $(juego 1)"; fi
es 2 PROG 'C:\JUEGOS\DOS\JUEGO.COM' "(2) «Dos» desde su acceso"
entre "$(campo 2 VEL)" "$v0" 90 110 "(2) sin opciones de juego: la velocidad normal"
es 2 FRENO NO "(2) sin freno"
p2=$(campo 2 PSP)
if [ -n "$p2" ] && [ $((16#$p2)) -lt $((16#1000)) ]; then ok "(2) sin LOADFIX el juego se carga en los primeros 64 KB (PSP=$p2)"; else ko "(2) sin LOADFIX tenía que cargarse por debajo de 1000h (PSP=$p2)"; fi
es 3 PROG 'C:\JUEGOS\UNO\UNO.EXE' "(3) «Uno»: el programa que se llama como su carpeta"
if juego 3 | grep -qF 'ARGS=[ uno dos]'; then ok "(3) con sus parámetros"; else ko "(3) parámetros: $(juego 3)"; fi
es 3 FRENO SI "(3) con VELOCIDAD frenando mientras corre"
entre "$(campo 3 VEL)" "$v0" 20 70 "(3) velocidad 386: corre frenado"
p3=$(campo 3 PSP)
if [ -n "$p3" ] && [ $((16#$p3)) -ge $((16#1000)) ]; then ok "(3) con LOADFIX, por encima de 1000h (PSP=$p3)"; else ko "(3) con LOADFIX tenía que cargarse por encima de 1000h (PSP=$p3)"; fi
check "Velocidad: como un 386 a 33 MHz (medido: "
entre "$(campo 4 VEL)" "$v0" 90 110 "(4) al volver del juego frenado, la velocidad normal"
es 4 RES NO "(4) y VELOCIDAD ya no está en la memoria"
if [ "$(campo 4 PSP)" = "$p2" ]; then ok "(4) y la memoria, como antes (PSP=$p2)"; else ko "(4) memoria tras el juego: PSP=$(campo 4 PSP), antes $p2"; fi
es 5 PROG 'C:\JUEGOS\TRES\GRANDE.EXE' "(5) «Tres»: de dos .EXE, el mayor"
entre "$(campo 5 VEL)" "$v0" 17 34 "(5) velocidad 25 %: una cuarta parte"
check "JUEGO: espera"
check "^C"
checkn "JUEGO: tecla"
entre "$(campo 6 VEL)" "$v0" 90 110 "(6) tras acabar el juego con Ctrl+C, la velocidad normal"
es 6 RES NO "(6) y VELOCIDAD ya no está en la memoria"
es 7 PROG 'C:\JUEGOS\SUELTO.COM' "(7) «Suelto»: un programa suelto de la carpeta"
es 7 FRENO SI "(7) frenado"
entre "$(campo 7 VEL)" "$v0" 5 30 "(7) velocidad 286"
check "JUEGO: residente"
check "VELOCIDAD no ha podido soltar el temporizador"
chk $T/a7.ppm titulo 182 207 activa "Velocidad" \
  texto 208 248 0 8 fina "El equipo ha vuelto a su velocidad normal, pero" \
  texto 208 266 0 8 fina "VELOCIDAD no ha podido soltar el temporizador"
entre "$(campo 8 VEL)" "$v0" 90 110 "(8) con un residente del juego en la INT 08h, la velocidad normal"
es 8 FRENO NO "(8) VELOCIDAD no frena"
es 8 RES SI "(8) aunque sigue en la memoria (no puede soltar la INT 08h)"
# -- borrar a mano y la carpeta del grupo --
chk $T/a8.ppm titulo 272 212 activa "Juegos de DOS" texto 634 315 9 12 fina "Solo" px 338 370 8 "«Suelto» ya no está"
chk $T/a9.ppm titulo 170 187 activa "Carpeta de juegos" \
  texto 188 224 0 8 fina "Hispa Ventanas pone en este grupo un acceso directo"
# -- al salir --
check "TRAS-SALIR"
entre "$(campo 9 VEL)" "$v0" 90 110 "(9) al salir de Hispa Ventanas, la velocidad normal"
check "Velocidad normal (sin freno)."
check "SIGUE-VIVO"
checkn "HSPDPMI: excepción"
ini "JuegosDOS=1"; ini "Nombre=Juegos de DOS"; ini "Carpeta=C:\\JUEGOS"
ini "Programa=C:\\JUEGOS\\UNO\\UNO.EXE"; ini "Parametros=uno dos"; ini "Velocidad=386"; ini "Loadfix=1"
ini "Programa=C:\\JUEGOS\\DOS\\JUEGO.COM"; ini "Programa=C:\\JUEGOS\\TRES\\GRANDE.EXE"; ini "Velocidad=25%"
ini "Programa=C:\\JUEGOS\\SOLO\\SETUP.EXE"; ini "Programa=C:\\JUEGOS\\SUELTO.COM"; ini "Velocidad=286"
ini "Auto=1"; ini "Cambiado=1"; ini "Oculto=1"
inin "Programa=C:\\JUEGOS\\INSTALAR.EXE"; inin "Programa=C:\\JUEGOS\\UNO\\SETUP.EXE"; inin "Sonido=sb"
# los que nadie ha tocado no llevan claves nuevas más que Auto
if [ "$(sed -n '/^Nombre=Dos$/,/^$/p' $T/ini.txt | tr '\n' ' ')" = "Nombre=Dos Icono=juego Programa=C:\\JUEGOS\\DOS\\JUEGO.COM Carpeta=C:\\JUEGOS\\DOS Parametros= Volver=1 Pausa=0 Auto=1  " ]; then
  ok "CENTRO.INI: «Dos», sin tocar, solo lleva Auto=1"
else
  ko "CENTRO.INI, «Dos»: $(sed -n '/^Nombre=Dos$/,/^$/p' $T/ini.txt | tr '\n' ' ')"
fi
muestra a1 centro; muestra a2 grupo; muestra a4 propiedades; muestra a6 propiedades_pct; muestra a7 aviso_velocidad
muestra a8 borrado; muestra a9 carpeta
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/Ejecutando/,$p' $T/log.txt; cat $T/ini.txt; fi
[ -n "${GUARDAR:-}" ] && cp $T/*.ppm "$GUARDAR"/

echo "== Hispa Ventanas: juegos que entran y salen de la carpeta, y la segunda red de VELOCIDAD =="
f1=$fail
mdeltree -i $HD ::JUEGOS/SOLO ::JUEGOS/TRES
mmd -i $HD ::JUEGOS/NUEVO
mcopy -i $HD $J ::JUEGOS/NUEVO/NUEVO.EXE
RX=400; RY=300
{ espera 'A:\\>' 40
  send 'KEYB SP' 3
  send 'A:\HISPADOS\HSPRATON' 3
  send 'C:\JUEGOS\DOS\JUEGO' 4                 # (1)
  send 'A:\HISPADOS\VELOCIDA 25%' 6           # un freno puesto a mano antes de entrar
  send 'C:\JUEGOS\DOS\JUEGO' 8                 # (2)
  send 'VENTANAS' 3
  espera 'cargando el servidor DPMI' 60 1; sleep 20; centro; aparca
  shot b1
  # ---- (3) «Uno» (386 y LOADFIX): al volver, Hispa Ventanas quita el freno ----
  clic 338 292
  tecla ret
  espera '^JUEGO PROG' 90 3; vuelta 2
  # ---- (4) «Dos» ----
  clic 442 292
  tecla ret
  espera '^JUEGO PROG' 40 4; vuelta 3
  # ---- «Tres» ya no está en el disco: se cambió a mano y se queda, con el aspa ----
  clic 546 292
  aparca
  shot b2
  tecla ret; sleep 2
  shot b3
  tecla ret; sleep 1.5
  # ---- (5) el juego nuevo ----
  clic 650 292
  tecla ret
  espera '^JUEGO PROG' 40 5; vuelta 4
  clic 756 13; sleep 4
  send 'ECHO TRAS-SALIR' 2
  send 'A:\HISPADOS\VELOCIDA' 5
  send 'ECHO SIGUE-VIVO' 2
  mon quit
} | run 500
v0=$(campo 1 VEL)
entre "$(campo 2 VEL)" "$v0" 17 34 "(2) con VELOCIDAD 25% puesto a mano antes de entrar"
chk $T/b1.ppm tam 800 600 titulo 272 212 activa "Juegos de DOS" \
  texto 326 315 0 8 fina "Uno" texto 430 315 0 8 fina "Dos" texto 530 315 0 8 fina "Tres" texto 630 315 9 12 fina "Nuevo" \
  px 338 370 8 "sin segunda fila: «Solo» ha salido y «Suelto» sigue sin salir" \
  px 534 277 13 "aspa en «Tres» (su programa ya no está)" px 326 277 8 "«Uno» sin aspa"
es 3 PROG 'C:\JUEGOS\UNO\UNO.EXE' "(3) «Uno» con su velocidad"
es 3 FRENO SI "(3) frenado"
p3=$(campo 3 PSP)
if [ -n "$p3" ] && [ $((16#$p3)) -ge $((16#1000)) ]; then ok "(3) y por encima de 1000h (PSP=$p3)"; else ko "(3) LOADFIX: PSP=$p3"; fi
check "Velocidad normal. VELOCIDAD se ha quitado de la memoria."
entre "$(campo 4 VEL)" "$v0" 90 110 "(4) al volver, Hispa Ventanas ha quitado el freno que quedaba: velocidad normal"
es 4 RES NO "(4) y VELOCIDAD ya no está en la memoria"
chk $T/b3.ppm titulo 186 234 activa "Centro de programas" \
  texto 212 275 0 8 fina "No se encuentra el programa del acceso directo."
es 5 PROG 'C:\JUEGOS\NUEVO\NUEVO.EXE' "(5) el juego nuevo de la carpeta tiene su acceso"
entre "$(campo 5 VEL)" "$v0" 90 110 "(5) a la velocidad normal"
check "TRAS-SALIR"
check "Velocidad normal (sin freno)."
check "SIGUE-VIVO"
checkn "HSPDPMI: excepción"
ini "Programa=C:\\JUEGOS\\NUEVO\\NUEVO.EXE"; ini "Programa=C:\\JUEGOS\\TRES\\GRANDE.EXE"; ini "Oculto=1"
inin "Programa=C:\\JUEGOS\\SOLO\\SETUP.EXE"
if [ "$(grep -c '^Programa=C:.JUEGOS.SUELTO.COM$' $T/ini.txt)" = 1 ]; then ok "CENTRO.INI: «Suelto», borrado a mano, sigue oculto y una sola vez"; else ko "CENTRO.INI: «Suelto» $(grep -c 'SUELTO.COM' $T/ini.txt) veces"; fi
muestra b1 segunda; muestra b3 falta
if [ $fail -ne $f1 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/Ejecutando/,$p' $T/log.txt; cat $T/ini.txt; fi
[ -n "${GUARDAR:-}" ] && cp $T/*.ppm "$GUARDAR"/

echo "== Hispa Ventanas: CENTRO.INI viejo, falta VELOCIDA.COM y LOADFIX solo =="
f1=$fail
mdel -i $T/f.img ::HISPADOS/VELOCIDA.COM
printf '%s\r\n' '; de antes de las opciones de juego' '[Centro]' 'Ventana=36,52,452,190' 'Activo=0' '' \
  '[Grupo]' 'Nombre=Mis cosas' 'Ventana=100,300,460,200' 'Estado=1' 'Marcado=0' '' \
  '[Acceso]' 'Nombre=Viejo' 'Icono=juego' 'Programa=C:\JUEGOS\DOS\JUEGO.COM' 'Carpeta=C:\JUEGOS\DOS' 'Parametros=viejo' 'Volver=1' 'Pausa=0' '' \
  '[Acceso]' 'Nombre=Con freno' 'Icono=juego' 'Programa=C:\JUEGOS\UNO\UNO.EXE' 'Carpeta=' 'Parametros=' 'Volver=1' 'Pausa=0' 'velocidad = 386' '' \
  '[Acceso]' 'Nombre=Arriba' 'Icono=juego' 'Programa=C:\JUEGOS\UNO\UNO.EXE' 'Carpeta=' 'Parametros=arriba' 'Volver=1' 'Pausa=0' 'LOADFIX=1' > $T/CENTRO.INI
mcopy -o -i $T/f.img $T/CENTRO.INI ::HISPADOS/CENTRO.INI
RX=400; RY=300
{ espera 'A:\\>' 40
  send 'KEYB SP' 3
  send 'A:\HISPADOS\HSPRATON' 3
  send 'VENTANAS' 3
  vuelta 1
  shot c1
  tecla ret                                   # (1) «Viejo»
  espera '^JUEGO PROG' 40 1; vuelta 2
  tecla right ret; sleep 2                    # «Con freno»: falta VELOCIDA.COM
  shot c2
  tecla ret; sleep 1.5
  tecla right ret                             # (2) «Arriba»: LOADFIX solo
  espera '^JUEGO PROG' 40 2; vuelta 3
  clic 756 13; sleep 4
  send 'ECHO SIGUE-VIVO' 2
  mon quit
} | run 300
chk $T/c1.ppm tam 800 600 marco 100 300 460 200 activa titulo 100 300 activa "Mis cosas" \
  texto 146 403 9 12 fina "Viejo" texto 234 403 0 8 fina "Con freno" texto 350 403 0 8 fina "Arriba" \
  texto 66 155 9 7 fina "Mis cosas" texto 154 155 0 8 fina "Juegos de DOS"
es 1 PROG 'C:\JUEGOS\DOS\JUEGO.COM' "(1) el acceso de siempre funciona"
if juego 1 | grep -qF 'ARGS=[ viejo]'; then ok "(1) con sus parámetros"; else ko "(1) parámetros: $(juego 1)"; fi
es 1 RES NO "(1) sin VELOCIDAD"
chk $T/c2.ppm titulo 158 207 activa "Centro de programas" \
  texto 184 248 0 8 fina "Falta VELOCIDA.COM en la carpeta de Hispa Ventanas:"
es 2 PROG 'C:\JUEGOS\UNO\UNO.EXE' "(2) «Arriba», con LOADFIX y sin velocidad"
if juego 2 | grep -qF 'ARGS=[ arriba]'; then ok "(2) con sus parámetros"; else ko "(2) parámetros: $(juego 2)"; fi
p1=$(campo 1 PSP); p2=$(campo 2 PSP)
if [ -n "$p1" ] && [ $((16#$p1)) -lt $((16#1000)) ] && [ -n "$p2" ] && [ $((16#$p2)) -ge $((16#1000)) ]; then ok "sin LOADFIX por debajo de 1000h (PSP=$p1) y con LOADFIX por encima (PSP=$p2)"; else ko "LOADFIX: PSP=$p1 sin él y PSP=$p2 con él"; fi
es 2 RES NO "(2) sin VELOCIDAD"
if [ "$(grep -c '^JUEGO PROG' $T/log.txt)" = 2 ]; then ok "el acceso con velocidad no se ha lanzado sin VELOCIDA.COM"; else ko "juegos lanzados: $(grep -c '^JUEGO PROG' $T/log.txt)"; fi
check "SIGUE-VIVO"
checkn "HSPDPMI: excepción"
ini "Nombre=Mis cosas"; ini "Nombre=Viejo"; ini "Parametros=viejo"; ini "Velocidad=386"; ini "Loadfix=1"
ini "JuegosDOS=1"; ini "Nombre=Juegos de DOS"; ini "Carpeta=C:\\JUEGOS"
ini "Programa=C:\\JUEGOS\\NUEVO\\NUEVO.EXE"; ini "Programa=C:\\JUEGOS\\SUELTO.COM"
# los juegos que ya tenían acceso en otro grupo no se repiten
if [ "$(grep -E '^(Nombre|\[Grupo\])' $T/ini.txt | tr '\n' ' ')" = "[Grupo] Nombre=Mis cosas Nombre=Viejo Nombre=Con freno Nombre=Arriba [Grupo] Nombre=Juegos de DOS Nombre=Nuevo Nombre=Suelto " ]; then
  ok "CENTRO.INI: los grupos de antes, igual, y el de la carpeta sin repetir juegos"
else
  ko "CENTRO.INI, grupos: $(grep -E '^(Nombre|\[Grupo\])' $T/ini.txt | tr '\n' ' ')"
fi
muestra c1 ini_viejo; muestra c2 falta_velocida
if [ $fail -ne $f1 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/Ejecutando/,$p' $T/log.txt; cat $T/ini.txt; fi
[ -n "${GUARDAR:-}" ] && cp $T/*.ppm "$GUARDAR"/

echo "== Hispa Ventanas: un acceso con la Sound Blaster emulada (HSPEMM y AC'97) =="
f1=$fail
printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nDEVICE=A:\\HISPADOS\\HSPEMM.SYS\r\nFILES=30\r\n' > $T/CONFIG.SYS
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
printf '%s\r\n' '[Centro]' 'Ventana=36,52,452,190' 'Activo=0' 'JuegosDOS=1' '' \
  '[Grupo]' 'Nombre=Mis cosas' 'Ventana=100,300,460,200' 'Estado=1' 'Marcado=0' '' \
  '[Acceso]' 'Nombre=Con sonido' 'Icono=juego' 'Programa=C:\JUEGOS\DOS\JUEGO.COM' 'Sonido=SB' > $T/CENTRO.INI
mcopy -o -i $T/f.img $T/CENTRO.INI ::HISPADOS/CENTRO.INI
RX=400; RY=300
{ espera 'A:\\>' 40
  send 'KEYB SP' 3
  send 'A:\HISPADOS\HSPRATON' 3
  send 'VENTANAS' 3
  vuelta 1
  tecla alt-a down down down ret; sleep 1.5   # Archivo > Propiedades...
  shot d1
  tecla esc; sleep 1.5
  tecla ret                                   # (1) carga HSPSND /SB y el juego
  espera '^JUEGO PROG' 60 1; vuelta 2
  shot d2
  tecla ret                                   # (2) otra vez: HSPSND ya está
  espera '^JUEGO PROG' 60 2; vuelta 3
  clic 756 13; sleep 4
  send 'ECHO SIGUE-VIVO' 2
  mon quit
} | run 300 -audiodev none,id=snd0 -device AC97,audiodev=snd0
chk $T/d1.ppm marco 158 76 484 448 activa titulo 158 76 activa "Propiedades del acceso directo" \
  texto 198 461 0 8 fina "Sonido: Sound Blaster emulada (HSPSND /SB)" px 183 468 4 "casilla de la Sound Blaster emulada marcada"
check "Sound Blaster 16 emulada en 220h"
check "Variable puesta: BLASTER=A220"
es 1 BLASTER SI "(1) el juego ve la variable BLASTER que pone HSPSND /SB"
es 2 BLASTER SI "(2) y la segunda vez también"
if [ "$(grep -c 'Sound Blaster 16 emulada en 220h' $T/log.txt)" = 1 ]; then ok "HSPSND /SB se carga una sola vez"; else ko "HSPSND /SB cargado $(grep -c 'Sound Blaster 16 emulada en 220h' $T/log.txt) veces"; fi
chk $T/d2.ppm tam 800 600 marco 100 300 460 200 activa titulo 100 300 activa "Mis cosas" px 690 12 8 "altavoz encendido: HSPSND ha quedado cargado"
check "SIGUE-VIVO"
checkn "HSPDPMI: excepción"
checkn "No cabe en el entorno"
ini "Sonido=sb"
muestra d1 propiedades_sb
if [ $fail -ne $f1 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/Ejecutando/,$p' $T/log.txt; cat $T/ini.txt; fi
[ -n "${GUARDAR:-}" ] && cp $T/*.ppm "$GUARDAR"/
rm -rf $T
exit $fail
