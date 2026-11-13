#!/bin/bash
# Prueba de Pintura (PINTURA.HVP), el programa de dibujo de Hispa Ventanas.
# Arranca en QEMU, maneja el entorno con el ratón y el teclado del monitor de
# QEMU, saca capturas y las comprueba punto a punto
# (tests/tools/ventanas_check.py); los BMP que guarda se sacan de la imagen
# con mtools y se miran octeto a octeto (tests/tools/pintura_bmp.py).
#  1.ª sesión (sin CENTRO.INI: se crea con los grupos por omisión):
#  - PINTURA.HVP está en \HISPADOS\PROGS del disquete y de la imagen USB, y
#    su acceso sale en el grupo «Accesorios» con su nombre y su icono;
#  - se abre como una ventana: barra de herramientas, paleta, lienzo en
#    blanco de 480x320 y barra de estado;
#  - lápiz con el botón izquierdo; colores primario y secundario con cada
#    botón; rectángulo con fondo y elipse con el botón derecho; relleno,
#    deshacer y rehacer; línea, pincel, goma, texto (con su diálogo),
#    cuentagotas con los dos botones; selección: marcar, mover, copiar, pegar
#    y dejar con Esc;
#  - Guardar pide el nombre (sin extensión: .BMP) y el archivo del disco es
#    un BMP de 8 bits con la paleta de Hispa Ventanas y cada punto donde se
#    dibujó;
#  - cerrar sin cambios no pregunta; con cambios, sí («No» cierra);
#  - salir con Pintura abierta: la memoria extendida queda como al entrar.
#  2.ª sesión (CENTRO.INI a mano; el BMP que se abre va en los parámetros):
#  - abre BMP de 1, 4, 8 y 24 bits (y uno con la primera fila arriba): cada
#    color sale el más parecido de la paleta;
#  - «Guardar como» del de 24 bits: el archivo tiene cada punto pasado a la
#    paleta;
#  - Archivo > Abrir con un archivo que no es un BMP, uno comprimido y uno
#    demasiado grande: lo dice y sigue como estaba;
#  - Archivo > Nuevo con otro tamaño;
#  - una imagen que no cabe: barras de desplazamiento, flechas, Av Pág,
#    arrastrar el tirador, dibujar con el lienzo desplazado y guardar.
# Deja capturas en build/pintura_*.png.
# Hispa Ventanas paint program test.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
B=build
T=$(mktemp -d)
CHK="python3 tests/tools/ventanas_check.py"
BMP="python3 tests/tools/pintura_bmp.py"
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() { if grep -qF -- "$1" $T/log.txt; then ok "$1"; else ko "$1"; fi; }
checkn() { if grep -qF -- "$1" $T/log.txt; then ko "(no debía salir) $1"; else ok "sin \"$1\""; fi; }
ini() { if grep -qxF -- "$1" $T/ini.txt; then ok "CENTRO.INI: $1"; else ko "CENTRO.INI sin la línea «$1»"; fi; }
chk() { $CHK "$@" || fail=1; }
mira() { $BMP mira "$@" || fail=1; }
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; sleep "${2:-0.3}"; }
tecla() { local k; for k in "$@"; do mon "sendkey $k" 0.3; done; }
shot() { sleep 0.4; mon "screendump $T/$1.ppm" 0.6; }
send() { printf '%s\r' "$1"; sleep "${2:-1.5}"; }
muestra() { [ -f $T/$1.ppm ] && convert $T/$1.ppm $B/pintura_$2.png; }
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
# arrastra BOTON X0 Y0 X1 Y1 (botón: 1 izquierdo, 2 derecho)
arrastra() { ir $2 $3; mon "mouse_button $1" 0.4; ir $4 $5; mon "mouse_button 0" 0.6; }
aparca() { ir 16 320; }
# teclea un texto: letras, cifras, espacio y punto
escribe() {
  local t="$1" i c
  for ((i=0; i<${#t}; i++)); do
    c="${t:$i:1}"
    case "$c" in
      ' ') tecla spc;;
      '.') tecla dot;;
      [A-Z]) tecla "shift-$(echo $c | tr A-Z a-z)";;
      *) tecla "$c";;
    esac
  done
}
# La ventana de Pintura está en (100,60) y mide 600x440: su zona empieza en
# (102,105) y el lienzo, sin desplazar, en (108,145). Estas funciones pasan
# un punto del lienzo a la pantalla
X() { echo $(( 108 + $1 )); }
Y() { echo $(( 145 + $1 )); }
# el centro del botón de una herramienta (0-9), de un grosor (0-3), de una
# forma (0-2) y de un color de la paleta (0-55), en la pantalla
HX() { echo $(( 120 + 30 * $1 )); }
GX() { echo $(( 425 + 24 * $1 )); }
FX() { echo $(( 531 + 28 * $1 )); }
PX() { echo $(( 635 + 17 * ($1 % 4) )); }
PY() { echo $(( 197 + 17 * ($1 / 4) )); }
BY=122                                      # la fila de los botones
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

echo "== Pintura: el grupo Accesorios, dibujar con cada herramienta, deshacer y guardar un BMP =="
cp "$IMG" $T/f.img
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
if mdir -i $T/f.img ::HISPADOS/PROGS/PINTURA.HVP > /dev/null 2>&1; then
  ok "PINTURA.HVP está en \\HISPADOS\\PROGS del disquete"
else
  ko "falta PINTURA.HVP en \\HISPADOS\\PROGS del disquete"
fi
if [ -f $B/hispados-usb.img ]; then
  if MTOOLS_SKIP_CHECK=1 mdir -i $B/hispados-usb.img@@1048576 ::HISPADOS/PROGS/PINTURA.HVP > /dev/null 2>&1; then
    ok "y en \\HISPADOS\\PROGS de la imagen USB"
  else
    ko "falta PINTURA.HVP en \\HISPADOS\\PROGS de la imagen USB"
  fi
fi
{ sleep 6
  send 'KEYB SP' 2
  send 'A:\HISPADOS\HSPRATON' 2
  send 'BIN\XMSTEST' 2.5
  send 'VENTANAS' 5
  # ---- el grupo Accesorios: Pintura es el quinto ----
  doble 206 129; sleep 1
  aparca
  shot a1
  tecla right right right right ret; sleep 2.5
  aparca
  shot a2
  # ---- lápiz: una raya con el botón izquierdo ----
  arrastra 1 $(X 20) $(Y 20) $(X 100) $(Y 20)
  # ---- colores: terracota con el izquierdo, añil con el derecho ----
  clic $(PX 4) $(PY 4)
  derecho $(PX 12) $(PY 12)
  aparca
  shot a3
  # ---- rectángulo con fondo, de grosor 4 ----
  tecla r
  clic $(GX 2) $BY
  clic $(FX 2) $BY
  arrastra 1 $(X 150) $(Y 50) $(X 250) $(Y 120)
  # ---- elipse con el botón derecho: los colores, al revés ----
  tecla e
  arrastra 2 $(X 300) $(Y 50) $(X 420) $(Y 130)
  aparca
  shot a4
  # ---- relleno en ocre, deshacer y rehacer ----
  clic $(PX 10) $(PY 10)
  tecla b
  clic $(X 10) $(Y 300)
  aparca
  shot a5
  tecla ctrl-z; sleep 0.5
  shot a6
  tecla ctrl-y; sleep 0.5
  shot a7
  tecla ctrl-z; sleep 0.5
  # ---- línea, pincel y goma ----
  tecla n
  arrastra 1 $(X 20) $(Y 250) $(X 120) $(Y 290)
  tecla p
  arrastra 1 $(X 300) $(Y 200) $(X 400) $(Y 200)
  tecla g
  clic $(X 350) $(Y 200)
  # ---- texto (con el grosor fino) ----
  tecla slash slash t                       # (la tecla del guion: dos grosores menos)
  clic $(X 20) $(Y 150); sleep 1
  shot a8
  escribe "Hola 26"; tecla ret; sleep 1
  aparca
  shot a9
  # ---- cuentagotas: cada botón, su color ----
  tecla c
  clic $(X 200) $(Y 85)
  derecho $(X 60) $(Y 20)
  aparca
  shot a10
  # ---- selección: marcar, mover, copiar, pegar y dejar ----
  tecla s
  arrastra 1 $(X 140) $(Y 40) $(X 260) $(Y 130)
  aparca
  shot a11
  arrastra 1 $(X 200) $(Y 85) $(X 200) $(Y 185)
  aparca
  shot a12
  tecla ctrl-c ctrl-v; sleep 0.5
  shot a13
  tecla esc; sleep 0.5
  shot a14
  # ---- guardar: pide el nombre ----
  tecla ctrl-g; sleep 1.2
  shot a15
  escribe "dibujo"; tecla ret; sleep 8
  aparca
  shot a16
  tecla alt-f4; sleep 1                     # sin cambios: se cierra sin preguntar
  shot a17
  # ---- otra vez: con cambios, pregunta ----
  tecla ret; sleep 2.5
  arrastra 1 $(X 30) $(Y 30) $(X 90) $(Y 30)
  tecla alt-f4; sleep 1
  shot a18
  tecla tab spc; sleep 1                    # «No»
  shot a19
  # ---- y salir de Hispa Ventanas con Pintura abierta ----
  tecla ret; sleep 2.5
  arrastra 1 $(X 30) $(Y 30) $(X 90) $(Y 30)
  tecla alt-c ret; sleep 1                  # (y con la ventana de todos los colores)
  clic 264 185; derecho 344 185             # el quinto y el decimotercero de los propios
  aparca
  shot a20
  clic 756 13; sleep 1; tecla ret; sleep 2.5
  shot a21
  send 'ECHO TRAS-SALIR'
  send 'BIN\XMSTEST' 2.5
  send 'ECHO SIGUE-VIVO'
  mon quit
} | run 240 -vga std

check "Teclado español instalado."
# -- el grupo: Pintura, con su nombre y su icono --
chk $T/a1.ppm tam 800 600 marco 224 164 452 207 activa titulo 224 164 activa "Accesorios" \
  texto 262 343 0 8 fina "Pintura" px 284 304 1 "icono de Pintura: el marco de la hoja" \
  px 281 317 12 "icono de Pintura: el recuadro añil"
# -- la ventana recién abierta --
chk $T/a2.ppm marco 100 60 600 440 activa titulo 100 60 activa "Pintura - (sin nombre)" \
  texto 110 480 0 15 fina "Lápiz   480 x 320" px $(X 5) $(Y 5) 9 "el lienzo, en blanco" \
  px 107 144 0 "el filete del lienzo" px 104 141 7 "el fondo de detrás del lienzo" \
  px 650 161 0 "color primario: tinta" px 680 167 9 "color secundario: blanco" \
  px $(PX 4) $(PY 4) 4 "la paleta: terracota" px $(PX 14) $(PY 14) 14 "la paleta: turquesa" \
  px 106 108 4 "el lápiz, elegido (recuadro terracota)" px 136 108 7 "el pincel, sin elegir"
# -- lápiz y colores --
chk $T/a3.ppm px $(X 60) $(Y 20) 0 "la raya del lápiz" px $(X 60) $(Y 22) 9 "y debajo, blanco" \
  px $(X 19) $(Y 20) 9 "la raya empieza donde se apretó" px $(X 100) $(Y 20) 0 "y acaba donde se soltó" \
  px 650 161 4 "primario: terracota (clic izquierdo en la paleta)" \
  px 680 167 12 "secundario: añil (clic derecho)"
# -- rectángulo y elipse --
chk $T/a4.ppm px $(X 151) $(Y 51) 4 "rectángulo: el contorno, con el primario" \
  px $(X 153) $(Y 85) 4 "de 4 puntos de grueso" px $(X 154) $(Y 85) 12 "y el fondo, con el secundario" \
  px $(X 200) $(Y 85) 12 "el fondo del rectángulo" px $(X 145) $(Y 85) 9 "fuera, blanco" \
  px $(X 250) $(Y 120) 4 "la esquina de abajo" px $(X 251) $(Y 121) 9 "y ni un punto más" \
  px $(X 360) $(Y 90) 4 "elipse con el botón derecho: fondo con el primario" \
  px $(X 301) $(Y 90) 12 "y contorno con el secundario" px $(X 360) $(Y 51) 12 "el contorno, arriba" \
  px $(X 303) $(Y 53) 9 "la esquina de su caja, en blanco" \
  texto 110 480 0 15 fina "Elipse   480 x 320" px 574 108 4 "la forma «con fondo», elegida" \
  px 462 108 4 "el grosor 4, elegido"
# -- relleno, deshacer y rehacer --
chk $T/a5.ppm px $(X 10) $(Y 300) 10 "relleno: el fondo, en ocre" px $(X 470) $(Y 5) 10 "hasta la otra punta" \
  px $(X 200) $(Y 85) 12 "sin entrar en el rectángulo" px $(X 360) $(Y 90) 4 "ni en la elipse" \
  px $(X 60) $(Y 20) 0 "ni tapar la raya"
chk $T/a6.ppm px $(X 10) $(Y 300) 9 "Ctrl+Z: el relleno, deshecho" px $(X 200) $(Y 85) 12 "y lo demás sigue"
chk $T/a7.ppm px $(X 10) $(Y 300) 10 "Ctrl+Y: rehecho"
# -- línea, pincel, goma y texto --
chk $T/a8.ppm titulo 230 225 activa "Texto" texto 248 264 0 8 fina "Texto que se escribe en el lienzo:"
chk $T/a9.ppm px $(X 10) $(Y 300) 9 "otra vez deshecho" \
  px $(X 70) $(Y 270) 10 "la línea, en ocre" px $(X 70) $(Y 264) 9 "y solo la línea" \
  px $(X 330) $(Y 200) 10 "el pincel" px $(X 330) $(Y 201) 10 "de 4 puntos" px $(X 330) $(Y 202) 9 "justos" \
  px $(X 350) $(Y 200) 12 "la goma: un cuadrado con el secundario" px $(X 345) $(Y 195) 12 "su esquina" \
  px $(X 344) $(Y 194) 9 "y fuera, nada" \
  texto $(X 20) $(Y 150) 10 9 fina "Hola 26" texto 110 480 0 15 fina "Texto   480 x 320" \
  px 414 108 4 "el grosor fino, elegido con la tecla -" px 462 108 7 "y el de 4, ya no"
# -- cuentagotas --
chk $T/a10.ppm px 650 161 12 "cuentagotas: primario, el añil del rectángulo" \
  px 680 167 0 "y con el derecho, secundario: la tinta de la raya"
# -- selección --
chk $T/a11.ppm px $(X 140) $(Y 40) 0 "el recuadro de la selección: una raya" px $(X 145) $(Y 40) 9 "y un hueco" \
  px $(X 260) $(Y 130) 0 "hasta la otra esquina" texto 110 480 0 15 fina "Selección   480 x 320"
chk $T/a12.ppm px $(X 200) $(Y 185) 12 "el rectángulo, movido 100 puntos abajo" \
  px $(X 151) $(Y 151) 4 "con su contorno" px $(X 200) $(Y 85) 0 "donde estaba queda el secundario" \
  px $(X 139) $(Y 85) 9 "solo en lo que se marcó" px $(X 360) $(Y 90) 4 "la elipse, en su sitio"
chk $T/a13.ppm px $(X 12) $(Y 12) 4 "pegado en la esquina: el contorno" px $(X 60) $(Y 45) 12 "y el fondo" \
  px $(X 0) $(Y 0) 0 "con su recuadro" px $(X 200) $(Y 185) 12 "el que se movió se ha quedado"
chk $T/a14.ppm px $(X 0) $(Y 0) 9 "Esc: se deja y se va el recuadro" px $(X 12) $(Y 12) 4 "lo pegado sigue"
# -- guardar --
chk $T/a15.ppm titulo 222 150 activa "Guardar como"
chk $T/a16.ppm marco 100 60 600 440 activa titulo 100 60 activa "Pintura - dibujo.BMP"
mcopy -n -i $T/f.img ::DIBUJO.BMP $T/DIBUJO.BMP 2>/dev/null
mira $T/DIBUJO.BMP tam 480 320 \
  px 12 12 4 "el rectángulo pegado: contorno" px 60 45 12 "y fondo" \
  px 200 85 0 "el hueco del que se movió" px 200 185 12 "el rectángulo movido" px 151 151 4 "su contorno" \
  px 360 90 4 "la elipse" px 301 90 12 "su contorno" px 70 270 10 "la línea" \
  px 330 200 10 "el pincel" px 350 200 12 "la goma" px 470 310 9 "el fondo" px 479 319 9 "la última esquina" \
  px 0 319 9 "la esquina de abajo a la izquierda" px 115 20 9 "junto a lo pegado"
# -- cerrar --
chk $T/a17.ppm titulo 224 164 activa "Accesorios" px 400 300 8 "Pintura se ha cerrado sin preguntar"
chk $T/a18.ppm titulo 230 234 activa "Pintura" texto 256 275 0 8 fina "La imagen tiene cambios sin guardar." \
  texto 256 293 0 8 fina "¿Quiere guardarlos?"
chk $T/a19.ppm titulo 224 164 activa "Accesorios" px 400 300 8 "«No»: se cierra sin guardar"
chk $T/a20.ppm titulo 214 150 activa "Todos los colores" px 224 185 0 "el primero: tinta" \
  px 374 185 15 "el decimosexto: piedra" px 574 281 9 "el último del cubo: blanco" \
  px 650 161 4 "un clic en ella: primario, terracota" px 680 167 12 "y con el derecho: secundario, añil"
# -- salida --
chk $T/a21.ppm tam 720 400
check "Hispa Ventanas ha terminado."
check "TRAS-SALIR"
check "SIGUE-VIVO"
checkn "HSPDPMI: excepción"
xms_igual
ini "Modulos=9"; ini "Nombre=Pintura"; ini "Programa=A:\\HISPADOS\\PROGS\\PINTURA.HVP"
if [ "$(grep -E '^(Nombre|\[Grupo\])' $T/ini.txt | tr '\n' ' ')" = "[Grupo] Nombre=Principal Nombre=Editar Nombre=Gestor Nombre=Información Nombre=Hispa-DOS Nombre=Configuración [Grupo] Nombre=Accesorios Nombre=Bloc de notas Nombre=Calculadora Nombre=Reloj Nombre=Sistema Nombre=Pintura Nombre=Reproductor de sonido [Grupo] Nombre=Juegos Nombre=Culebra Nombre=Campo de minas Nombre=Bloques Nombre=Solitario Nombre=Tragabolas Nombre=Almacén Nombre=Hispa-Nes " ]; then
  ok "CENTRO.INI: Pintura, en el grupo Accesorios, y los juegos en el suyo"
else
  ko "CENTRO.INI, grupos: $(grep -E '^(Nombre|\[Grupo\])' $T/ini.txt | tr '\n' ' ')"
fi
muestra a1 grupo; muestra a2 abierta; muestra a4 formas; muestra a5 relleno; muestra a8 texto
muestra a9 herramientas; muestra a11 seleccion; muestra a13 pegado; muestra a15 guardar_como
muestra a16 guardado; muestra a18 cambios; muestra a20 colores
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/Ejecutando/,$p' $T/log.txt; cat $T/ini.txt; fi
[ -n "${GUARDAR:-}" ] && cp $T/*.ppm $T/*.BMP "$GUARDAR"/

echo "== Pintura: abrir BMP de 1, 4, 8 y 24 bits, lienzo nuevo y una imagen que no cabe =="
cp "$IMG" $T/f.img
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
mkdir -p $T/bmp
$BMP crea $T/bmp
mcopy -i $T/f.img $T/bmp/*.BMP ::
printf '%s\r\n' '[Centro]' 'Ventana=36,52,452,190' 'Activo=0' 'Modulos=9' '' \
  '[Grupo]' 'Nombre=Imagenes' 'Ventana=120,420,560,150' 'Estado=1' '' \
  '[Acceso]' 'Nombre=Uno' 'Programa=A:\HISPADOS\PROGS\PINTURA.HVP' 'Parametros=A:\T1.BMP' '' \
  '[Acceso]' 'Nombre=Cuatro' 'Programa=A:\HISPADOS\PROGS\PINTURA.HVP' 'Parametros=A:\T4.BMP' '' \
  '[Acceso]' 'Nombre=Ocho' 'Programa=A:\HISPADOS\PROGS\PINTURA.HVP' 'Parametros=A:\T8.BMP' '' \
  '[Acceso]' 'Nombre=Veinticuatro' 'Programa=A:\HISPADOS\PROGS\PINTURA.HVP' 'Parametros=A:\T24.BMP' '' \
  '[Acceso]' 'Nombre=Arriba' 'Programa=A:\HISPADOS\PROGS\PINTURA.HVP' 'Parametros=A:\TD24.BMP' '' \
  '[Acceso]' 'Nombre=Grande' 'Programa=A:\HISPADOS\PROGS\PINTURA.HVP' 'Parametros=A:\GRANDE.BMP' > $T/CENTRO.INI
mcopy -o -i $T/f.img $T/CENTRO.INI ::HISPADOS/CENTRO.INI
RX=400; RY=300
{ sleep 6
  send 'KEYB SP' 2
  send 'A:\HISPADOS\HSPRATON' 2
  send 'BIN\XMSTEST' 2.5
  send 'VENTANAS' 5
  aparca
  # ---- 1 bit ----
  tecla ret; sleep 3
  shot b1
  tecla alt-f4; sleep 1
  # ---- 4 bits ----
  tecla right ret; sleep 3
  shot b2
  tecla alt-f4; sleep 1
  # ---- 8 bits ----
  tecla right ret; sleep 3
  shot b3
  tecla alt-f4; sleep 1
  # ---- 24 bits, y Guardar como ----
  tecla right ret; sleep 3
  shot b4
  tecla alt-a down down down ret; sleep 1.2
  escribe "copia"; tecla ret; sleep 4
  shot b5
  tecla alt-f4; sleep 1
  # ---- 24 bits con la primera fila arriba ----
  tecla right ret; sleep 3
  shot b6
  # ---- Abrir: lo que no se puede abrir ----
  tecla alt-a down ret; sleep 1.2
  escribe "roto.bmp"; tecla ret; sleep 1.5
  shot b7
  tecla ret; sleep 0.8
  tecla alt-a down ret; sleep 1.2
  escribe "rle.bmp"; tecla ret; sleep 1.5
  shot b8
  tecla ret; sleep 0.8
  tecla alt-a down ret; sleep 1.2
  escribe "enorme.bmp"; tecla ret; sleep 1.5
  shot b9
  tecla ret; sleep 0.8
  tecla alt-a down ret; sleep 1.2
  escribe "nohay.bmp"; tecla ret; sleep 1.5
  shot b10
  tecla ret; sleep 0.8
  shot b11
  # ---- Abrir uno bueno desde el diálogo ----
  tecla alt-a down ret; sleep 1.2
  escribe "t4.bmp"; tecla ret; sleep 2.5
  shot b12
  # ---- Nuevo, con otro tamaño ----
  tecla alt-a ret; sleep 1.2
  shot b13
  escribe "200x100"; tecla ret; sleep 1.5
  shot b14
  tecla alt-f4; sleep 1
  # ---- una imagen que no cabe ----
  tecla right ret; sleep 20
  shot c1
  tecla right right right; sleep 0.5
  shot c2
  tecla pgdn; sleep 0.5
  shot c3
  arrastra 1 241 471 391 471; aparca       # el tirador de la barra de abajo
  shot c4
  arrastra 1 300 200 360 200; aparca       # el lápiz, con el lienzo desplazado
  shot c5
  tecla alt-a down down down ret; sleep 1.2
  escribe "g2"; tecla ret; sleep 25
  shot c6
  clic 756 13; sleep 1; tecla ret; sleep 2.5
  shot c7
  send 'ECHO TRAS-SALIR'
  send 'BIN\XMSTEST' 2.5
  send 'ECHO SIGUE-VIVO'
  mon quit
} | run 300 -vga std

# el color con el que tiene que salir el punto (x, y) de una imagen de prueba
c() { $BMP color $1 $2 $3; }
chk $T/b1.ppm marco 100 60 600 440 activa titulo 100 60 activa "Pintura - A:\\T1.BMP" \
  texto 110 480 0 15 fina "Lápiz   70 x 40" \
  px $(X 0) $(Y 0) 12 "1 bit: el primer cuadro, añil" px $(X 8) $(Y 0) 10 "el segundo, ocre" \
  px $(X 7) $(Y 7) 12 "la esquina del primero" px $(X 0) $(Y 8) 10 "debajo, ocre" \
  px $(X 69) $(Y 39) 12 "el último punto" px $(X 70) $(Y 39) 0 "y ahí acaba el lienzo (su filete)" \
  px $(X 69) $(Y 0) 12 "la última columna, arriba"
chk $T/b2.ppm titulo 100 60 activa "Pintura - A:\\T4.BMP" texto 110 480 0 15 fina "Lápiz   66 x 30" \
  px $(X 0) $(Y 0) 15 "4 bits: arriba, los colores al revés" px $(X 62) $(Y 0) 0 "hasta la tinta" \
  px $(X 0) $(Y 6) 0 "debajo, del derecho" px $(X 17) $(Y 10) 4 "terracota" px $(X 41) $(Y 20) 10 "ocre" \
  px $(X 65) $(Y 29) 0 "el último punto (el color 16 vuelve a ser el primero)" px $(X 61) $(Y 29) 15 "y el de antes, piedra"
chk $T/b3.ppm titulo 100 60 activa "Pintura - A:\\T8.BMP" texto 110 480 0 15 fina "Lápiz   83 x 37" \
  px $(X 40) $(Y 2) 11 "8 bits: la franja de arriba, verde oliva" \
  px $(X 3) $(Y 10) 4 "un color propio: terracota" px $(X 11) $(Y 10) 12 "añil" \
  rgb $(X 19) $(Y 10) $(c T8 19 10) "rojo del cubo" rgb $(X 27) $(Y 10) $(c T8 27 10) "un gris de la escala" \
  rgb $(X 35) $(Y 10) $(c T8 35 10) "casi rojo: el rojo del cubo" rgb $(X 43) $(Y 10) $(c T8 43 10) "gris 100: el 99 de la escala" \
  rgb $(X 51) $(Y 20) $(c T8 51 20) "blanco" rgb $(X 59) $(Y 20) $(c T8 59 20) "negro" \
  rgb $(X 67) $(Y 30) $(c T8 67 30) "azul claro: el más parecido del cubo" rgb $(X 75) $(Y 36) $(c T8 75 36) "un color justo del cubo" \
  rgb $(X 82) $(Y 36) $(c T8 82 36) "el último punto"
chk $T/b4.ppm titulo 100 60 activa "Pintura - A:\\T24.BMP" texto 110 480 0 15 fina "Lápiz   61 x 48" \
  px $(X 30) $(Y 2) 11 "24 bits: la franja de arriba" px $(X 3) $(Y 10) 4 "terracota" \
  rgb $(X 35) $(Y 10) $(c T24 35 10) "casi rojo" rgb $(X 43) $(Y 30) $(c T24 43 30) "gris" \
  rgb $(X 5) $(Y 44) $(c T24 5 44) "el degradado, a la izquierda" rgb $(X 30) $(Y 42) $(c T24 30 42) "en medio" \
  rgb $(X 60) $(Y 47) $(c T24 60 47) "y el último punto"
chk $T/b5.ppm titulo 100 60 activa "Pintura - copia.BMP"
mcopy -n -i $T/f.img ::COPIA.BMP $T/COPIA.BMP 2>/dev/null
mira $T/COPIA.BMP tam 61 48 igual $T/bmp/T24.BMP
chk $T/b6.ppm titulo 100 60 activa "Pintura - A:\\TD24.BMP" texto 110 480 0 15 fina "Lápiz   32 x 20" \
  px $(X 5) $(Y 2) 4 "la primera fila, arriba: terracota" px $(X 5) $(Y 9) 4 "hasta la fila 9" \
  px $(X 5) $(Y 10) 14 "y debajo, turquesa" px $(X 31) $(Y 19) 14 "hasta el final"
# -- lo que no se abre --
chk $T/b7.ppm titulo 166 234 activa "Pintura" \
  texto 192 275 0 8 fina "El archivo no es una imagen BMP de 1, 4, 8 o 24 bits" texto 192 293 0 8 fina "sin compresión."
chk $T/b8.ppm titulo 166 234 activa "Pintura" \
  texto 192 275 0 8 fina "El archivo no es una imagen BMP de 1, 4, 8 o 24 bits"
chk $T/b9.ppm titulo 198 234 activa "Pintura" \
  texto 224 275 0 8 fina "La imagen es demasiado grande: Pintura llega" texto 224 293 0 8 fina "hasta 1024 x 768 puntos."
chk $T/b10.ppm titulo 242 243 activa "Pintura" texto 268 284 0 8 fina "No se ha podido abrir el archivo."
chk $T/b11.ppm titulo 100 60 activa "Pintura - A:\\TD24.BMP" px $(X 5) $(Y 2) 4 "y la imagen sigue como estaba"
chk $T/b12.ppm titulo 100 60 activa "Pintura - t4.bmp" texto 110 480 0 15 fina "Lápiz   66 x 30" \
  px $(X 17) $(Y 10) 4 "abierta desde el diálogo"
# -- lienzo nuevo --
chk $T/b13.ppm titulo 230 225 activa "Lienzo nuevo" texto 248 264 0 8 fina "Ancho x alto (hasta 1024 x 768):"
chk $T/b14.ppm titulo 100 60 activa "Pintura - (sin nombre)" texto 110 480 0 15 fina "Lápiz   200 x 100" \
  px $(X 17) $(Y 10) 9 "en blanco" px $(X 199) $(Y 99) 9 "hasta su esquina" px $(X 200) $(Y 100) 0 "y su filete"
# -- la imagen que no cabe (600x400; se ven 506x325) --
chk $T/c1.ppm titulo 100 60 activa "Pintura - A:\\GRANDE.BMP" texto 110 480 0 15 fina "Lápiz   600 x 400" \
  px $(X 10) $(Y 10) 0 "el primer cuadro: tinta" px $(X 60) $(Y 10) 1 "el segundo: marrón" px $(X 10) $(Y 60) 3 "el de debajo" \
  px 300 471 6 "la barra de abajo: el tirador" px 580 471 15 "y la pista" \
  px 615 200 6 "la barra de la derecha: el tirador" px 615 440 15 "y la pista"
chk $T/c2.ppm px $(X 10) $(Y 10) 1 "tres flechas a la derecha: 48 puntos" px $(X 1) $(Y 10) 0 "justos" \
  px $(X 2) $(Y 10) 1 "(el cuadro marrón empieza en el 50)"
chk $T/c3.ppm px $(X 10) $(Y 10) 4 "Av Pág: hasta abajo (87 puntos)" px $(X 10) $(Y 12) 4 "(la fila 99)" \
  px $(X 10) $(Y 13) 7 "y el cuadro siguiente" px 615 200 15 "el tirador de la derecha ha bajado" px 615 440 6 "hasta el fondo"
chk $T/c4.ppm px $(X 10) $(Y 10) 5 "arrastrar el tirador: hasta la derecha (106 puntos)" \
  px 580 471 6 "el tirador, a la derecha" px 110 471 15 "y la pista, a la izquierda" \
  px $(X 434) $(Y 253) 12 "un cuadro del final de la imagen" px $(X 494) $(Y 250) 0 "su filete" \
  px $(X 497) $(Y 250) 7 "y el margen"
chk $T/c5.ppm px 330 200 0 "el lápiz, con el lienzo desplazado" px 330 198 12 "sobre el cuadro añil"
chk $T/c6.ppm titulo 100 60 activa "Pintura - g2.BMP"
mcopy -n -i $T/f.img ::G2.BMP $T/G2.BMP 2>/dev/null
mira $T/G2.BMP tam 600 400 px 328 142 0 "la raya, donde toca del lienzo" px 298 142 0 "donde empezó" \
  px 358 142 0 "donde acabó" px 359 142 13 "y ni un punto más" px 328 140 12 "encima, el cuadro añil" \
  px 10 10 0 "el primer cuadro" px 60 10 1 "el segundo" px 599 399 0 "el último punto" px 0 399 5 "abajo a la izquierda"
chk $T/c7.ppm tam 720 400
check "Hispa Ventanas ha terminado."
check "TRAS-SALIR"
check "SIGUE-VIVO"
checkn "HSPDPMI: excepción"
xms_igual
muestra b1 bmp_1bit; muestra b2 bmp_4bits; muestra b3 bmp_8bits; muestra b4 bmp_24bits; muestra b7 no_es_bmp
muestra b13 nuevo; muestra c1 barras; muestra c5 desplazado
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/Ejecutando/,$p' $T/log.txt; cat $T/ini.txt; fi
[ -n "${GUARDAR:-}" ] && cp $T/*.ppm $T/*.BMP "$GUARDAR"/
rm -rf $T
exit $fail
