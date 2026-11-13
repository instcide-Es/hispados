#!/bin/bash
# Prueba de Configuración (CONFIG.HVP), el programa de ajustes de Hispa
# Ventanas, y de VENTANAS.INI. Arranca en QEMU, maneja el entorno con el
# ratón y el teclado del monitor de QEMU, saca capturas y las comprueba punto
# a punto (tests/tools/ventanas_check.py); VENTANAS.INI se mira en la imagen
# con mtools. (tests/run_config_tests.sh es otra cosa: la de CONFIG.SYS.)
#  1.ª sesión (sin VENTANAS.INI: todo de fábrica):
#  - CONFIG.HVP está en \HISPADOS del disquete y de la imagen USB, y el grupo
#    «Principal» trae su acceso directo, «Configuración», con su icono;
#  - se abre como una ventana con los apartados a la izquierda;
#  - Pantalla: un tema, un dibujo y un color cambian el fondo del escritorio
#    al momento; a los tres segundos se guarda solo en VENTANAS.INI;
#  - el salvapantallas: casilla, minutos y «Probar» (pantalla en negro, se
#    quita con una tecla, que no llega a la ventana de debajo);
#  - Ratón: tiempo del doble clic con el deslizador y la zona de prueba;
#    «Guardar»;
#  - Fecha y hora: enseña la del reloj del equipo, se cambia y, al salir,
#    FECHA y HORA de Hispa-DOS dan la nueva (también tras reiniciar);
#  - Sistema: versión, memoria, pantalla;
#  - Teclado: con TECLADO residente se pasa a estadounidense y a española
#    (la tecla de la ñ escribe «;» o «ñ»), y la velocidad de repetición;
#  - Esc oculta la ventana (se queda en marcha), el acceso la enseña otra vez;
#  - salir con ella abierta: VENTANAS.INI en el disco con lo elegido y la
#    memoria extendida como al entrar.
#  2.ª sesión (el mismo disco: «reiniciar Hispa Ventanas», y con una Sound
#    Blaster 16 y HSPSND): el fondo guardado sale al arrancar, sin abrir nada,
#    y el doble clic lento vale; Sonido: volumen y silencio (los guarda Hispa
#    Ventanas en CENTRO.INI).
#  3.ª sesión (VENTANAS.INI a mano, con mayúsculas cambiadas, espacios, una
#    clave desconocida y un valor imposible): fondo, ratón el doble de rápido,
#    botones intercambiados y teclado estadounidense al arrancar; «De
#    fábrica» lo deja todo como viene y así se guarda.
#  4.ª sesión: VGA de 640x480 y 16 colores (VENTANAS /VGA): la ventana cabe y
#    el fondo guardado se ve.
# Deja capturas en build/config_*.png.
# Hispa Ventanas settings program (CONFIG.HVP) and VENTANAS.INI test.
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
checke() { if grep -qE -- "$1" $T/log.txt; then ok "$2"; else ko "$2"; fi; }
ini() { if grep -qxF -- "$1" $T/ini.txt; then ok "VENTANAS.INI: $1"; else ko "VENTANAS.INI sin la línea «$1»"; fi; }
inin() { if grep -q -- "^$1" $T/ini.txt; then ko "VENTANAS.INI: sobra «$1»"; else ok "VENTANAS.INI: sin «$1»"; fi; }
chk() { $CHK "$@" || fail=1; }
# memoria extendida que se ve: la de la máquina (16 MB; otra con QEMU_MAS='-m N'
# en tools/pruebas_pc.sh) menos el primer mega y 128 KB
MEGAS=$(echo " ${QEMU_MAS:-} " | sed -n 's/.* -m \([0-9]*\)[ M].*/\1/p'); MEGAS=${MEGAS:-16}
MEMEXT=$(printf "%d" $((MEGAS * 1024 - 1152)) | sed ':a;s/\B[0-9]\{3\}\>/.&/;ta')
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; sleep "${2:-0.3}"; }
tecla() { local k; for k in "$@"; do mon "sendkey $k" 0.3; done; }
shot() { sleep 0.4; mon "screendump $T/$1.ppm" 0.6; }
send() { printf '%s\r' "$1"; sleep "${2:-1.5}"; }
muestra() { [ -f $T/$1.ppm ] && convert $T/$1.ppm $B/config_$2.png; }
# el ratón: RX, RY = dónde está el puntero; VEL = la velocidad puesta, en
# mitades (2: un punto por cada paso del ratón)
RX=400; RY=300; VEL=2
ir() {
  local dx=$(( ($1 - RX) * 2 / VEL )) dy=$(( ($2 - RY) * 2 / VEL )) sx sy
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
aparca() { ir 16 320; }
run() {
  local tmo=$1; shift
  rm -f $T/mon
  timeout $tmo qemu-system-i386 -display none -serial stdio -monitor unix:$T/mon,server,nowait \
    -parallel none -m 16 -drive file=$T/f.img,format=raw,if=floppy,index=0 -boot a "$@" > $T/raw 2>&1
  tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 | grep -v '^audio:' > $T/log.txt
  mtype -i $T/f.img ::HISPADOS/VENTANAS.INI 2>/dev/null | tr -d '\r' | iconv -f CP437 -t UTF-8 > $T/ini.txt
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
# La ventana de Configuración en 800x600: marco en (124,110), de 552x403; su
# zona empieza en (126,135). Los apartados: x=196, y=162+34*n.

echo "== Configuración: de fábrica, cambiar ajustes, guardar, fecha y hora =="
cp "$IMG" $T/f.img
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
if mdir -i $T/f.img ::HISPADOS/CONFIG.HVP > /dev/null 2>&1; then
  ok "CONFIG.HVP está en \\HISPADOS del disquete"
else
  ko "falta CONFIG.HVP en \\HISPADOS del disquete"
fi
if mdir -i $T/f.img ::HISPADOS/JUEGOS/CONFIG.HVP > /dev/null 2>&1; then ko "CONFIG.HVP no es un juego: sobra en \\HISPADOS\\JUEGOS"; fi
if [ -f $B/hispados-usb.img ]; then
  if MTOOLS_SKIP_CHECK=1 mdir -i $B/hispados-usb.img@@1048576 ::HISPADOS/CONFIG.HVP > /dev/null 2>&1 &&
     ! MTOOLS_SKIP_CHECK=1 mdir -i $B/hispados-usb.img@@1048576 ::HISPADOS/JUEGOS/CONFIG.HVP > /dev/null 2>&1; then
    ok "y en \\HISPADOS de la imagen USB (de ahí lo copia INSTALAR)"
  else
    ko "CONFIG.HVP no está en \\HISPADOS de la imagen USB (o está en JUEGOS)"
  fi
fi
{ sleep 6
  send 'KEYB SP' 2
  send 'A:\HISPADOS\HSPRATON' 2
  send 'BIN\XMSTEST' 2.5
  send 'VENTANAS' 6
  shot s0
  # ---- el grupo Principal y su acceso directo ----
  doble 102 129; sleep 1
  aparca
  shot s1
  tecla right right right right ret; sleep 2
  aparca
  shot s2
  # ---- Pantalla: tema «Mar», dibujo «damero», fondo granate ----
  clic 318 237; aparca
  shot s3
  clic 544 258; clic 575 321
  shot s4                                   # (enseguida: todavía sin guardar)
  aparca
  sleep 4                                   # se guarda solo
  shot s5
  # ---- salvapantallas: casilla, minutos y «Probar» ----
  clic 286 407; clic 482 407; aparca
  shot s6
  clic 318 439; sleep 2
  shot s7
  tecla spc; sleep 1
  shot s8
  # ---- Ratón: el doble clic más largo, la zona de prueba y «Guardar» ----
  clic 196 196
  clic 508 271
  doble 526 316; aparca
  shot s9
  clic 527 491; aparca
  shot s10
  # ---- Fecha y hora ----
  clic 196 298; aparca; sleep 1.2
  shot s11
  clic 338 268; tecla ctrl-a 2 4 tab ctrl-a 1 2 tab ctrl-a 2 0 3 0 tab
  tecla ctrl-a 7 tab ctrl-a 8 tab ctrl-a 9 tab ret; sleep 1.5
  shot s12
  # ---- Sistema ----
  clic 196 332; aparca
  shot s13
  # ---- Teclado: estadounidense y española con la tecla de la ñ ----
  clic 196 230; aparca
  shot s14
  clic 400 330; tecla semicolon
  clic 426 393; clic 400 330; tecla semicolon
  clic 508 271; aparca
  shot s15
  clic 286 393; clic 400 330; tecla semicolon tab; aparca
  shot s16
  # ---- Esc la oculta; el acceso directo la enseña otra vez ----
  tecla esc; sleep 1.5
  shot s17
  tecla ret; sleep 1.5
  shot s18
  # ---- salir con ella abierta ----
  clic 756 13; sleep 1; tecla ret; sleep 2.5
  shot s19
  send 'ECHO TRAS-SALIR'
  send 'BIN\XMSTEST' 2.5
  send 'FECHA'
  send ''
  send 'HORA'
  send ''
  send 'ECHO REINICIO'
  mon system_reset 9
  send 'FECHA'
  send ''
  send 'ECHO SIGUE-VIVO'
  mon quit
} | run 200 -vga std -rtc base=2026-03-10T10:20:30

check "Teclado español instalado."
chk $T/s0.ppm tam 800 600 cursor 400 300 px 723 471 2 "el fondo de fábrica" \
  titulo 36 52 activa "Centro de programas"
# -- el acceso directo, el último de «Principal» --
chk $T/s1.ppm marco 200 140 452 260 activa titulo 200 140 activa "Principal" texto 542 243 0 8 fina "Hispa-DOS" \
  texto 214 319 0 8 fina "Configuración" px 265 285 8 "icono de Configuración: el panel" px 270 291 12 "icono de Configuración: un rombo añil"
# -- la ventana --
chk $T/s2.ppm marco 124 110 552 403 activa titulo 124 110 activa "Configuración" \
  texto 164 151 8 12 negrita "Pantalla" texto 164 185 0 8 fina "Ratón" texto 164 219 0 8 fina "Teclado" \
  texto 164 253 0 8 fina "Sonido" texto 164 287 0 8 fina "Fecha y hora" texto 164 321 0 8 fina "Sistema" \
  texto 278 145 4 8 negrita "Pantalla" texto 278 175 0 8 fina "Tema:" texto 424 175 0 8 fina "Dibujo del fondo: rombos" \
  texto 278 289 0 8 fina "Color del fondo: azul petróleo" texto 278 339 0 8 fina "Color del dibujo: petróleo claro" \
  texto 300 399 0 8 fina "Salvapantallas tras" texto 584 399 0 8 fina "5 minutos" \
  texto 134 483 0 8 fina "Se aplica al momento." px 333 321 2 "color del fondo elegido: azul petróleo" \
  px 289 321 0 "la muestra de la tinta" px 575 321 13 "la muestra del granate"
# -- el tema, el dibujo y el color cambian el escritorio al momento --
chk $T/s3.ppm texto 424 175 0 8 fina "Dibujo del fondo: olas" texto 278 289 0 8 fina "Color del fondo: añil" \
  px 720 475 14 "escritorio: una ola turquesa" px 720 473 12 "escritorio: fondo añil" px 60 402 12 "y a la izquierda"
chk $T/s4.ppm texto 424 175 0 8 fina "Dibujo del fondo: damero" texto 278 289 0 8 fina "Color del fondo: granate" \
  px 724 476 13 "escritorio: un cuadro granate" px 724 468 14 "escritorio: un cuadro turquesa" \
  texto 134 483 0 8 fina "Hay cambios sin guardar."
chk $T/s5.ppm texto 134 483 0 8 fina "Guardado en VENTANAS.INI."
# -- salvapantallas --
chk $T/s6.ppm texto 584 399 0 8 fina "10 minutos" px 286 407 4 "la casilla del salvapantallas, marcada"
chk $T/s7.ppm tam 800 600 px 5 5 0 "salvapantallas: la pantalla, en negro" px 400 14 0 "la barra de arriba, tapada" \
  px 795 595 0 "hasta la esquina" cambia 0 0 800 600 $T/s6.ppm 300000
chk $T/s8.ppm marco 124 110 552 403 activa titulo 124 110 activa "Configuración" px 5 5 1 "la barra de arriba, otra vez" \
  texto 584 399 0 8 fina "10 minutos" px 286 407 4 "la tecla no ha llegado a la ventana (la casilla sigue marcada)"
# -- Ratón --
chk $T/s9.ppm texto 164 185 8 12 negrita "Ratón" texto 278 145 4 8 negrita "Ratón" texto 278 175 0 8 fina "Velocidad del puntero:" \
  texto 584 199 0 8 fina "x1" texto 584 263 0 8 fina "990 ms" px 526 306 12 "el doble clic en la zona de prueba: rombo añil" \
  texto 300 359 0 8 fina "Intercambiar los botones (para zurdos)"
chk $T/s10.ppm texto 134 483 0 8 fina "Guardado en VENTANAS.INI."
# -- Fecha y hora --
chk $T/s11.ppm texto 278 175 0 8 negrita "martes, 10 de marzo de 2026" texto 278 261 0 8 fina "Día:" texto 322 260 0 9 fina "10" \
  texto 418 260 0 9 fina "3" texto 514 260 0 9 fina "2026" texto 278 323 0 8 fina "Hora:" texto 330 322 0 9 fina "10"
chk $T/s12.ppm texto 278 175 0 8 negrita "martes, 24 de diciembre de 2030" texto 278 197 0 8 negrita "07:08:" \
  texto 458 365 0 8 fina "Fecha y hora cambiadas."
# -- Sistema --
chk $T/s13.ppm texto 278 145 4 8 negrita "Sistema" texto 278 175 0 8 fina "Hispa Ventanas 0.3" \
  texto 278 193 0 8 fina "Sistema: Hispa-DOS (compatible con MS-DOS 6.22)" texto 278 211 0 8 fina "Procesador:" \
  texto 278 247 0 8 fina "Memoria convencional: 639 KB" texto 278 265 0 8 fina "Memoria extendida: $MEMEXT KB" \
  texto 278 301 0 8 fina "Pantalla: 800 x 600, 256 colores" texto 278 337 0 8 fina "Ratón: driver INT 33h (HSPRATON)" \
  texto 278 373 0 8 fina "Teclado: español (TECLADO)" texto 278 391 0 8 fina "País: España (página de códigos 437)"
# -- Teclado --
chk $T/s14.ppm texto 278 145 4 8 negrita "Teclado" texto 584 199 0 8 fina "500 ms" texto 584 263 0 8 fina "10,9 por s" \
  texto 300 385 0 8 fina "Española" px 286 393 4 "«Española», marcada" px 426 393 9 "«Estadounidense», sin marcar"
chk $T/s15.ppm texto 282 322 0 9 fina "ñ;" px 426 393 4 "«Estadounidense», marcada" px 286 393 9 "«Española», ya no" \
  texto 584 263 0 8 fina "30,0 por s"
chk $T/s16.ppm texto 282 322 0 9 fina "ñ;ñ" px 286 393 4 "«Española», otra vez"
# -- ocultar y enseñar --
chk $T/s17.ppm titulo 200 140 activa "Principal" texto 214 319 9 12 fina "Configuración" px 660 300 13 "Configuración ya no está a la vista"
chk $T/s18.ppm marco 124 110 552 403 activa titulo 124 110 activa "Configuración" texto 278 145 4 8 negrita "Teclado" \
  texto 134 483 0 8 fina "Se aplica al momento." texto 584 263 0 8 fina "30,0 por s"
# -- salida --
chk $T/s19.ppm tam 720 400
check "Hispa Ventanas ha terminado."
check "TRAS-SALIR"
checkn "HSPDPMI: excepción"
xms_igual
checke "La fecha actual es Mar 24-12-2030" "al salir, FECHA de Hispa-DOS da la fecha puesta (Mar 24-12-2030)"
checke "La hora actual es  7:0[89]:[0-9][0-9]" "y HORA, la hora puesta (7:08)"
if sed -n '/REINICIO/,$p' $T/log.txt | grep -q "La fecha actual es Mar 24-12-2030"; then
  ok "tras reiniciar el equipo, el reloj de tiempo real da la fecha puesta"
else
  ko "tras reiniciar el equipo, la fecha puesta"
fi
check "SIGUE-VIVO"
ini "[Pantalla]"; ini "Dibujo=damero"; ini "ColorFondo=13"; ini "ColorDibujo=14"; ini "Salvapantallas=1"; ini "Minutos=10"
ini "[Raton]"; ini "Velocidad=2"; ini "DobleClic=990"; ini "Zurdo=0"
ini "[Teclado]"; ini "Espera=500"; ini "Repeticion=31"; ini "Distribucion=SP"
muestra s1 grupo; muestra s2 pantalla; muestra s4 fondo; muestra s7 salvapantallas; muestra s9 raton
muestra s12 fecha; muestra s13 sistema; muestra s15 teclado
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/Ejecutando/,$p' $T/log.txt; cat $T/ini.txt; fi
[ -n "${GUARDAR:-}" ] && cp $T/*.ppm "$GUARDAR"/

echo "== Configuración: al reiniciar Hispa Ventanas se aplica lo guardado =="
# (el mismo disco, con el VENTANAS.INI y el CENTRO.INI que ha dejado la 1.ª sesión)
RX=400; RY=300
{ sleep 6
  send 'KEYB SP' 2
  send 'A:\HISPADOS\HSPRATON' 2
  send 'A:\HISPADOS\HSPSND' 3
  send 'VENTANAS' 6
  aparca
  shot r0
  # ---- Sonido: el volumen de HSPSND (80 % al entrar) al 50 %, y silencio ----
  doble 266 290; sleep 2                    # Configuración, en el grupo Principal (que está detrás)
  clic 196 264; aparca
  shot r2
  clic 428 207; clic 286 245; aparca
  shot r3
  tecla esc; sleep 1
  # ---- doble clic lento (más de medio segundo entre los dos) en «Accesorios» ----
  ir 196 126
  mon "mouse_button 1" 0.1; mon "mouse_button 0" 0.55; mon "mouse_button 1" 0.1; mon "mouse_button 0" 1.2
  aparca
  shot r1
  clic 756 13; sleep 1; tecla ret; sleep 2.5
  send 'ECHO SIGUE-VIVO'
  mon quit
} | run 120 -vga std -no-reboot -audiodev none,id=snd0 -device sb16,audiodev=snd0

chk $T/r0.ppm tam 800 600 px 724 476 13 "al arrancar, el fondo guardado: un cuadro granate" px 724 468 14 "y uno turquesa" \
  px 60 588 14 "en todo el escritorio" px 300 500 13 "sin la ventana de Configuración"
chk $T/r2.ppm titulo 124 110 activa "Configuración" texto 164 253 8 12 negrita "Sonido" texto 278 145 4 8 negrita "Sonido" \
  texto 278 175 0 8 fina "Volumen:" texto 586 199 0 8 fina "80 %" texto 300 237 0 8 fina "Silencio" px 286 245 9 "sin silencio"
chk $T/r3.ppm texto 586 199 0 8 fina "50 %" px 286 245 4 "«Silencio», marcada"
chk $T/r1.ppm titulo 224 164 activa "Accesorios"
cen() { if mtype -i $T/f.img ::HISPADOS/CENTRO.INI 2>/dev/null | tr -d '\r' | grep -qxF -- "$1"; then ok "CENTRO.INI: $1"; else ko "CENTRO.INI sin la línea «$1»"; fi; }
cen "Volumen=50"; cen "Silencio=1"
check "Hispa Ventanas ha terminado."
check "SIGUE-VIVO"
checkn "HSPDPMI: excepción"
ini "Dibujo=damero"; ini "DobleClic=990"
muestra r0 al_arrancar
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/Ejecutando/,$p' $T/log.txt; cat $T/ini.txt; fi
[ -n "${GUARDAR:-}" ] && cp $T/*.ppm "$GUARDAR"/

echo "== Configuración: VENTANAS.INI a mano, botones intercambiados y «De fábrica» =="
cp "$IMG" $T/f.img
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
printf '%s\r\n' '; a mano' '[pantalla]' 'DIBUJO = Zigzag' 'colorfondo=11' 'ColorDibujo = 10' 'Salvapantallas=1' 'Minutos=99' \
  'Inventada=7' '' '[Raton]' 'Velocidad=4' 'DobleClic=440' 'Zurdo=1' '' '[Teclado]' 'Distribucion=us' > $T/VENTANAS.INI
mcopy -o -i $T/f.img $T/VENTANAS.INI ::HISPADOS/VENTANAS.INI
RX=400; RY=300; VEL=4
{ sleep 6
  send 'KEYB SP' 2
  send 'A:\HISPADOS\HSPRATON' 2
  send 'VENTANAS' 6
  mon "mouse_move 50 40" 0.5                # el ratón va el doble de rápido
  shot m0
  RX=500; RY=380
  clic 206 129; aparca                      # el botón izquierdo ya no elige
  shot m1
  derecho 206 129; aparca                   # el derecho, sí
  shot m2
  tecla left ret; sleep 1                   # «Principal» y, dentro, Configuración
  tecla right right right right ret; sleep 2
  derecho 196 230; aparca                   # Teclado
  shot m3
  derecho 430 491; sleep 1                  # «De fábrica»
  shot m4
  tecla ret; sleep 1.5
  VEL=2
  for i in 1 2 3 4 5 6; do mon "mouse_move -200 -200" 0.1; done; RX=0; RY=0   # (a la esquina, para saber dónde está)
  aparca
  shot m5
  clic 196 196; aparca                      # Ratón (ya con el botón izquierdo)
  shot m6
  clic 527 491; aparca
  shot m7
  clic 756 13; sleep 1; tecla ret; sleep 2.5
  send 'ECHO SIGUE-VIVO'
  mon quit
} | run 120 -vga std -no-reboot

chk $T/m0.ppm tam 800 600 cursor 500 380 px 721 464 10 "al arrancar, el fondo del VENTANAS.INI hecho a mano: zigzag ocre" \
  px 724 464 11 "sobre verde oliva" texto 66 155 9 12 fina "Principal"
chk $T/m1.ppm texto 66 155 9 12 fina "Principal" texto 166 155 0 8 fina "Accesorios"
chk $T/m2.ppm texto 166 155 9 12 fina "Accesorios" texto 66 155 0 8 fina "Principal"
chk $T/m3.ppm titulo 124 110 activa "Configuración" texto 278 145 4 8 negrita "Teclado" \
  px 426 393 4 "teclado estadounidense, puesto al arrancar" px 286 393 9 "«Española», sin marcar"
chk $T/m4.ppm texto 208 284 0 8 fina "¿Poner todos los ajustes como vienen de fábrica?"
chk $T/m5.ppm cursor 16 320 px 723 471 2 "«De fábrica»: el fondo de siempre" \
  texto 278 145 4 8 negrita "Teclado" px 426 393 4 "(el teclado se queda como estaba)"
chk $T/m6.ppm texto 278 145 4 8 negrita "Ratón" texto 584 199 0 8 fina "x1" texto 584 263 0 8 fina "440 ms" px 286 367 9 "botones sin intercambiar"
chk $T/m7.ppm texto 134 483 0 8 fina "Guardado en VENTANAS.INI."
check "Hispa Ventanas ha terminado."
check "SIGUE-VIVO"
checkn "HSPDPMI: excepción"
ini "Dibujo=rombos"; ini "ColorFondo=2"; ini "ColorDibujo=3"; ini "Salvapantallas=0"; ini "Minutos=5"
ini "Velocidad=2"; ini "DobleClic=440"; ini "Zurdo=0"
inin "Espera="; inin "Repeticion="; inin "Distribucion="; inin "Inventada"
muestra m0 ini_a_mano; muestra m4 de_fabrica
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/Ejecutando/,$p' $T/log.txt; cat $T/ini.txt; fi
[ -n "${GUARDAR:-}" ] && cp $T/*.ppm "$GUARDAR"/

echo "== Configuración: en VGA de 640x480 y 16 colores =="
cp "$IMG" $T/f.img
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
printf '%s\r\n' '[Pantalla]' 'Dibujo=ladrillos' 'ColorFondo=4' 'ColorDibujo=8' > $T/VENTANAS.INI
mcopy -o -i $T/f.img $T/VENTANAS.INI ::HISPADOS/VENTANAS.INI
RX=320; RY=240; VEL=2
{ sleep 6
  send 'KEYB SP' 2
  send 'A:\HISPADOS\HSPRATON' 2
  send 'VENTANAS /VGA' 7
  tecla ret; sleep 1.5
  tecla right right right right ret; sleep 2.5
  ir 16 320
  shot v0
  tecla esc; sleep 1
  clic 596 13; sleep 1; tecla ret; sleep 2.5
  send 'ECHO SIGUE-VIVO'
  mon quit
} | run 90 -vga std -no-reboot

# (640x480: la ventana, en (44,50); el fondo, ladrillos terracota con la junta crema)
chk $T/v0.ppm tam 640 480 marco 44 50 552 403 activa titulo 44 50 activa "Configuración" \
  texto 198 85 4 8 negrita "Pantalla" texto 344 115 0 8 fina "Dibujo del fondo: ladrillos" \
  px 20 464 8 "la junta de los ladrillos, crema" px 20 468 4 "un ladrillo terracota"
check "Hispa Ventanas ha terminado."
check "SIGUE-VIVO"
checkn "HSPDPMI: excepción"
muestra v0 vga
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/Ejecutando/,$p' $T/log.txt; fi
[ -n "${GUARDAR:-}" ] && cp $T/*.ppm "$GUARDAR"/
rm -rf $T
exit $fail
