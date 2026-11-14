#!/bin/bash
# [CONTEXTO] Prueba de Hispa Ventanas: el botón derecho y los menús
# contextuales. Arranca en QEMU con HSPRATON, un CENTRO.INI con un grupo de
# tres accesos directos, y maneja el entorno con el ratón y el teclado del
# monitor de QEMU (y, para tener Ctrl apretado mientras se hace clic, por
# QMP: tests/tools/qmp.py). Capturas comprobadas con
# tests/tools/ventanas_check.py:
#  - clic derecho en un icono de un grupo: su menú (Abrir, Propiedades,
#    Cambiar nombre, Mover a otro grupo, Eliminar); «Propiedades» abre el
#    diálogo del acceso; «Cambiar nombre» (con su letra) lo cambia;
#    «Eliminar» (con las flechas e Intro) pregunta antes;
#  - en el fondo del grupo: Nuevo acceso directo, Buscar programas,
#    Organizar iconos (los ordena por el nombre y se guarda en CENTRO.INI);
#  - Ctrl+clic izquierdo hace lo mismo que el derecho; Esc cierra el menú;
#    un clic fuera también;
#  - Mayús+F10: el menú del icono marcado, sin ratón; la tecla de menú;
#  - en la barra de título: Restaurar, Minimizar, Maximizar y Cerrar
#    (Maximizar la maximiza; Restaurar, con Ctrl+clic, la deja como estaba);
#  - en un icono del Centro (un grupo);
#  - en el Bloc de notas: Seleccionar todo, Copiar y Pegar (con lo que no se
#    puede hacer en gris);
#  - en el escritorio, cerca de la esquina (el menú se ajusta al borde):
#    «Fondo del escritorio...» abre Configuración en Pantalla; «Ventanas en
#    cascada»; y el menú de una ventana minimizada (su icono de abajo).
# Deja capturas en build/contexto_*.png (y en capturas/ si existe).
# Hispa Ventanas test: right mouse button and context menus.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
B=build
T=$(mktemp -d)
CHK="python3 tests/tools/ventanas_check.py"
Q="python3 tests/tools/qmp.py $T/qmp"
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() { if grep -qF -- "$1" $T/log.txt; then ok "$1"; else ko "$1"; fi; }
checkn() { if grep -qF -- "$1" $T/log.txt; then ko "(no debía salir) $1"; else ok "sin \"$1\""; fi; }
ini() { if grep -qxF -- "$1" $T/ini.txt; then ok "CENTRO.INI: $1"; else ko "CENTRO.INI sin la línea «$1»"; fi; }
inin() { if grep -qF -- "$1" $T/ini.txt; then ko "CENTRO.INI no debía tener «$1»"; else ok "CENTRO.INI sin «$1»"; fi; }
chk() { $CHK "$@" || fail=1; }
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; sleep "${2:-0.3}"; }
tecla() { local k; for k in "$@"; do mon "sendkey $k" 0.3; done; }
shot() { sleep 0.5; mon "screendump $T/$1.ppm" 0.6; }
send() { printf '%s\r' "$1"; sleep "${2:-1.5}"; }
muestra() {
  [ -f $T/$1.ppm ] || return
  convert $T/$1.ppm $B/contexto_$2.png
  [ -d capturas ] && cp $B/contexto_$2.png capturas/
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
derecho() { ir $1 $2; mon "mouse_button 2" 0.25; mon "mouse_button 0" 0.6; }
# Ctrl apretado (por QMP) mientras se hace clic con el izquierdo
ctrlclic() { ir $1 $2; $Q down - ctrl wait 0.3; mon "mouse_button 1" 0.25; mon "mouse_button 0" 0.3; $Q up - ctrl; sleep 0.5; }
aparca() { ir 760 100; }
run() {
  local tmo=$1; shift
  rm -f $T/mon $T/qmp
  timeout $tmo qemu-system-i386 -display none -serial stdio -monitor unix:$T/mon,server,nowait \
    -qmp unix:$T/qmp,server,nowait \
    -parallel none -m 16 -drive file=$T/f.img,format=raw,if=floppy,index=0 -boot a -no-reboot "$@" > $T/raw 2>&1
  tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 > $T/log.txt
  mtype -i $T/f.img ::HISPADOS/CENTRO.INI 2>/dev/null | tr -d '\r' | iconv -f CP437 -t UTF-8 > $T/ini.txt
}

echo "== Hispa Ventanas: el botón derecho y los menús contextuales =="
cp "$IMG" $T/f.img
NMOD=$(( $(mdir -b -i $T/f.img '::HISPADOS/JUEGOS/*.HVP' 2>/dev/null | wc -l) + $(mdir -b -i $T/f.img '::HISPADOS/PROGS/*.HVP' 2>/dev/null | wc -l) ))
printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nFILES=30\r\n' > $T/CONFIG.SYS
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
{
  printf '%s\r\n' '[Centro]' 'Ventana=36,52,452,190' 'Activo=0' "Modulos=$NMOD" '' \
    '[Grupo]' 'Nombre=Pruebas' 'Ventana=100,300,460,200' 'Estado=1' 'Marcado=0' '' \
    '[Acceso]' 'Nombre=Zeta' 'Programa=A:\COMMAND.COM' '' \
    '[Acceso]' 'Nombre=Bloc de notas' 'Programa=:BLOC' '' \
    '[Acceso]' 'Nombre=Alfa' 'Programa=A:\COMMAND.COM'
} > $T/CENTRO.INI
mcopy -o -i $T/f.img $T/CENTRO.INI ::HISPADOS/CENTRO.INI
{ sleep 6
  send 'KEYB SP' 2
  send 'A:\HISPADOS\HSPDPMI' 2
  send 'A:\HISPADOS\HSPRATON' 3
  send "VENTANAS" 6
  shot a0
  # ---- un icono del grupo: su menú; Propiedades ----
  derecho 270 377
  shot a1
  clic 300 412; sleep 1                     # «Propiedades...»
  shot a2
  tecla esc; sleep 1
  # ---- Cambiar nombre (con la letra subrayada) ----
  derecho 166 377
  tecla n; sleep 1
  shot a3
  tecla ctrl-a; sleep 0.3
  tecla shift-o m e g a ret; sleep 1.2
  aparca
  shot a4
  # ---- Eliminar, con las flechas e Intro: pregunta antes ----
  derecho 374 377
  tecla up ret; sleep 1
  shot a5
  tecla ret; sleep 1
  aparca
  shot a6
  # ---- el fondo del grupo: Organizar iconos ----
  derecho 480 460
  shot a7
  tecla o; sleep 1
  aparca
  shot a8
  # ---- Ctrl+clic izquierdo = derecho; Esc cierra ----
  ctrlclic 270 377
  shot b1
  tecla esc; sleep 0.6
  aparca
  shot b2
  # ---- Mayús+F10 (el icono marcado) y un clic fuera ----
  tecla shift-f10; sleep 0.8
  shot b3
  clic 700 200; sleep 0.6
  shot b4
  # ---- la tecla de menú ----
  tecla compose; sleep 0.8
  shot b5
  tecla esc; sleep 0.6
  # ---- la barra de título: Maximizar; Restaurar con Ctrl+clic ----
  derecho 300 311
  shot c1
  tecla x; sleep 1                          # «Maximizar»
  aparca
  shot c2
  ctrlclic 300 40
  shot c3
  tecla r; sleep 1                          # «Restaurar»
  aparca
  shot c4
  # ---- un icono del Centro (un grupo) ----
  derecho 90 135
  shot d1
  tecla esc; sleep 0.6
  # ---- el Bloc de notas: Seleccionar todo, Copiar, Pegar ----
  doble 166 377; sleep 2
  for k in h o l a spc m u n d o; do tecla $k; done; sleep 0.5
  derecho 400 250
  shot e1
  tecla s; sleep 0.6                        # Seleccionar todo
  derecho 400 250
  shot e2
  tecla c; sleep 0.6                        # Copiar
  tecla end; sleep 0.3
  derecho 400 250
  tecla p; sleep 0.8                        # Pegar
  aparca
  shot e3
  # ---- el escritorio, cerca de la esquina: el menú se ajusta ----
  derecho 700 560
  shot f1
  tecla f; sleep 2.5                        # «Fondo del escritorio...»
  aparca
  shot f2
  tecla esc; sleep 1
  derecho 700 560
  tecla a; sleep 1.5                        # «Ventanas en cascada»
  aparca
  shot f3
  # ---- el menú de una ventana minimizada (su icono, abajo) ----
  derecho 300 70
  tecla n; sleep 1                          # el grupo: «Minimizar»
  derecho 64 570
  shot f4
  tecla r; sleep 1                          # «Restaurar»
  aparca
  shot f5
  # ---- Ventanas en mosaico ----
  derecho 700 560
  tecla o; sleep 1.5
  aparca
  shot f6
  clic 756 13; sleep 1; tecla ret; sleep 2.5
  send 'ECHO SIGUE-VIVO'
  mon quit
} | run 300

# -- un icono del grupo: su menú, junto al punto; el icono, marcado --
chk $T/a1.ppm texto 295 382 0 8 fina "brir" texto 295 410 0 8 fina "ropiedades..." \
  texto 287 430 0 8 fina "Cambiar " texto 295 450 0 8 fina "over a otro grupo..." \
  texto 295 478 0 8 fina "liminar" px 270 440 1 "el filete del menú"
chk $T/a2.ppm texto 186 81 8 4 negrita "Propiedades del acceso directo" texto 288 119 0 9 fina "loc de notas"
chk $T/a3.ppm texto 248 264 0 8 fina "Nombre nuevo del acceso directo:"
chk $T/a4.ppm texto 146 403 9 12 fina "Omega" px 270 440 8 "el menú, cerrado"
chk $T/a5.ppm texto 356 284 0 8 fina "acceso directo"
chk $T/a6.ppm px 374 377 8 "«Alfa», borrado"
# -- el fondo del grupo --
chk $T/a7.ppm texto 497 465 0 8 fina "Nuevo " texto 505 485 0 8 fina "uscar programas..." \
  texto 505 513 0 8 fina "rganizar iconos"
# -- Organizar iconos: por el nombre --
chk $T/a8.ppm texto 114 403 9 12 fina "Bloc de notas" texto 250 403 0 8 fina "Omega"
# -- Ctrl+clic izquierdo: el mismo menú; Esc lo cierra --
chk $T/b1.ppm texto 295 410 0 8 fina "ropiedades..." px 270 440 1 "el filete del menú (Ctrl+clic)"
chk $T/b2.ppm px 270 440 8 "Esc: el menú, cerrado"
# -- Mayús+F10: en el icono marcado; un clic fuera lo cierra; la tecla de menú --
chk $T/b3.ppm texto 295 422 0 8 fina "ropiedades..."
chk $T/b4.ppm px 270 450 8 "un clic fuera: el menú, cerrado"
chk $T/b5.ppm texto 295 422 0 8 fina "ropiedades..."
# -- la barra de título: Restaurar en gris; Maximizar --
chk $T/c1.ppm texto 325 316 7 8 fina "estaurar" texto 317 336 0 8 fina "Mi" texto 317 356 0 8 fina "Ma" \
  texto 325 384 0 8 fina "errar"
chk $T/c2.ppm marco 0 28 800 572 activa titulo 0 28 activa "Pruebas"
chk $T/c3.ppm texto 325 45 0 8 fina "estaurar" texto 317 85 7 8 fina "Ma"
chk $T/c4.ppm marco 100 300 460 200 activa titulo 100 300 activa "Pruebas"
# -- un icono del Centro --
chk $T/d1.ppm texto 115 168 0 8 fina "ropiedades..." texto 115 216 0 8 fina "liminar"
# -- el Bloc de notas: en gris lo que no se puede; Seleccionar todo, Copiar, Pegar --
chk $T/e1.ppm texto 156 138 0 9 fina "hola mund" texto 417 255 7 8 fina "Cor" texto 425 275 7 8 fina "opiar" \
  texto 425 295 7 8 fina "egar" texto 425 343 0 8 fina "eleccionar todo"
chk $T/e2.ppm texto 156 138 9 12 fina "hola mund" texto 417 255 0 8 fina "Cor" texto 425 275 0 8 fina "opiar" \
  texto 425 295 7 8 fina "egar"
chk $T/e3.ppm texto 156 138 0 9 fina "hola mundohola mund"
# -- el escritorio, en la esquina: el menú, arriba y a la izquierda del punto --
chk $T/f1.ppm texto 507 451 0 8 fina "onfiguración..." texto 507 471 0 8 fina "ondo del escritorio..." \
  texto 507 491 0 8 fina "ezclador..." texto 499 519 0 8 fina "Ventanas en c" texto 499 539 0 8 fina "Ventanas en m" \
  px 482 500 1 "el filete del menú, a la izquierda"
chk $T/f2.ppm titulo 124 110 activa "Configuración" texto 278 145 4 8 negrita "Pantalla"
# -- cascada --
chk $T/f3.ppm titulo 8 36 no "Centro de programas" titulo 34 62 no "Pruebas" titulo 60 88 activa "Bloc de notas - (sin nombre)"
# -- una ventana minimizada (el grupo): su menú, con Minimizar en gris; Restaurar --
chk $T/f4.ppm texto 89 481 0 8 fina "estaurar" texto 81 501 7 8 fina "Mi" texto 81 521 0 8 fina "Ma"
chk $T/f5.ppm marco 34 62 460 200 activa titulo 34 62 activa "Pruebas"
# -- mosaico: tres ventanas en una cuadrícula de 2x2 --
chk $T/f6.ppm marco 0 28 400 286 no marco 400 28 400 286 no marco 0 314 400 286 activa titulo 0 314 activa "Pruebas"
check "SIGUE-VIVO"
checkn "HSPDPMI: excepción"
ini "Nombre=Omega"; inin "Nombre=Alfa"; inin "Nombre=Zeta"
if [ "$(grep -n '^Nombre=' $T/ini.txt | sed -n '2,3p' | cut -d= -f2 | tr '\n' ,)" = "Bloc de notas,Omega," ]; then
  ok "CENTRO.INI: los accesos, en orden alfabético"; else ko "CENTRO.INI: los accesos no están en orden alfabético"; fi
for s in a1 a2 a7 b3 c1 e2 f1 f4 f6; do muestra $s $s; done
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/Ejecutando/,$p' $T/log.txt; cat $T/ini.txt; fi
[ -n "${GUARDAR:-}" ] && cp $T/*.ppm "$GUARDAR"/


# ---- 2.ª sesión: los módulos (Pintura, Mezclador, Reproductor) y Ctrl+clic quitado ----
cp "$IMG" $T/f.img
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
{
  printf '%s\r\n' '[Centro]' 'Ventana=36,52,452,190' 'Activo=0' "Modulos=$NMOD" '' \
    '[Grupo]' 'Nombre=Programas' 'Ventana=100,300,460,200' 'Estado=1' 'Marcado=2' '' \
    '[Acceso]' 'Nombre=Pintura' 'Programa=A:\HISPADOS\PROGS\PINTURA.HVP' '' \
    '[Acceso]' 'Nombre=Mezclador' 'Programa=A:\HISPADOS\PROGS\MEZCLAD.HVP' '' \
    '[Acceso]' 'Nombre=Sonidos' 'Programa=A:\HISPADOS\PROGS\SONIDOS.HVP'
} > $T/CENTRO.INI
mcopy -o -i $T/f.img $T/CENTRO.INI ::HISPADOS/CENTRO.INI
printf '%s\r\n' '[Raton]' 'CtrlClic=0' > $T/VENTANAS.INI
mcopy -o -i $T/f.img $T/VENTANAS.INI ::HISPADOS/VENTANAS.INI
RX=400; RY=300
{ sleep 6
  send 'KEYB SP' 2
  send 'A:\HISPADOS\HSPDPMI' 2
  send 'A:\HISPADOS\HSPRATON' 3
  send 'VENTANAS' 7
  # ---- CtrlClic=0: Ctrl+clic es un clic normal (marca, sin menú) ----
  ctrlclic 166 377
  aparca
  shot g1
  # ---- Pintura: Mayús+F10, su menú; el derecho pinta (sin menú) ----
  doble 166 377; sleep 2.5
  tecla shift-f10; sleep 0.8
  shot g2
  tecla i; sleep 0.8                        # «Intercambiar primario y secundario»
  ir 300 200; mon "mouse_button 2" 0.3; ir 400 260; mon "mouse_button 0" 0.8
  aparca
  shot g3
  tecla alt-f4; sleep 1.5
  clic 370 336; sleep 1.5                   # «No» (sin guardar)
  # ---- el Mezclador y el Reproductor de sonido: su menú ----
  doble 270 377; sleep 2.5
  derecho 400 320
  shot g4
  tecla esc; sleep 0.5
  tecla alt-f4; sleep 1.5
  doble 374 377; sleep 2.5
  shot g7
  derecho 400 200
  shot g5
  tecla esc; sleep 0.5
  tecla alt-f4; sleep 1.5
  # ---- Configuración > Ratón: la casilla de Ctrl+clic ----
  derecho 700 560
  tecla c; sleep 2.5
  clic 196 196; sleep 1                     # Ratón
  shot g6
  clic 286 431; sleep 1                     # «Ctrl+clic como botón derecho»
  shot g7
  tecla esc; sleep 1.5                      # (se guarda al cerrar)
  # ---- ya con Ctrl+clic: el menú del icono ----
  ctrlclic 270 377
  shot g8
  tecla esc; sleep 0.5
  clic 756 13; sleep 1; tecla ret; sleep 2.5
  send 'ECHO SIGUE-VIVO'
  mon quit
} | run 200
mtype -i $T/f.img ::HISPADOS/VENTANAS.INI 2>/dev/null | tr -d '\r' > $T/vini.txt

# -- CtrlClic=0: Ctrl+clic es un clic normal --
chk $T/g1.ppm texto 138 403 9 12 fina "Pintura" px 166 450 8 "sin menú"
# -- Pintura: Mayús+F10 da su menú; el derecho pinta con el secundario (negro tras intercambiar) --
chk $T/g2.ppm texto 135 118 0 8 fina "eshacer"
chk $T/g3.ppm px 330 230 0 "la raya del botón derecho" px 380 260 0 "la raya, en horizontal" px 150 200 9 "sin menú (el lienzo, blanco)"
# -- el Mezclador y el Reproductor --
chk $T/g4.ppm texto 417 325 0 8 fina "Vista " texto 417 345 7 8 fina "Vista " texto 425 373 0 8 fina "vanzado..."
chk $T/g5.ppm texto 417 205 0 8 fina "Sonido de " texto 417 273 7 8 fina "Pa" texto 425 301 0 8 fina "ezclador..."
# -- Configuración > Ratón: la casilla, sin marcar (CtrlClic=0) y marcada --
chk $T/g6.ppm texto 278 145 4 8 negrita "Ratón" texto 300 423 0 8 fina "Ctrl+clic como botón derecho" \
  px 286 431 9 "la casilla, sin marcar"
chk $T/g7.ppm cambia 278 421 20 20 $T/g6.ppm 8
# -- y ya hace de botón derecho --
chk $T/g8.ppm texto 295 410 0 8 fina "ropiedades..."
check "SIGUE-VIVO"
checkn "HSPDPMI: excepción"
if grep -qx 'CtrlClic=1' $T/vini.txt; then ok "VENTANAS.INI: CtrlClic=1"; else ko "VENTANAS.INI sin «CtrlClic=1»"; fi
for s in g2 g3 g4 g5 g6 g8; do muestra $s $s; done
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/Ejecutando/,$p' $T/log.txt; cat $T/vini.txt; fi
[ -n "${GUARDAR:-}" ] && cp $T/*.ppm "$GUARDAR"/
rm -rf $T
exit $fail
