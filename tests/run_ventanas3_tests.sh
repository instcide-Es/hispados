#!/bin/bash
# Prueba de Hispa Ventanas, etapa 3 (ventanas, mensajes, controles y los
# primeros programas). Arranca en QEMU con el teclado español (KEYB SP),
# maneja el entorno con el ratón y el teclado del monitor de QEMU, saca
# capturas y las comprueba punto a punto (tests/tools/ventanas_check.py):
#  - Centro de programas: iconos de los grupos, menú, abrir el grupo
#    «Accesorios» y, desde él, cada programa (los grupos y los accesos
#    directos se prueban a fondo en tests/run_ventanas4_tests.sh);
#  - Bloc de notas: escribir (con ñ y acentos), menús con ratón y teclado,
#    «Guardar como» (diálogo con cuadro de texto y lista), portapapeles,
#    buscar (diálogo de pedir texto), aviso de cambios sin guardar, abrir un
#    archivo desde la lista (con su barra de desplazamiento); el archivo
#    guardado se comprueba con mtools en la imagen;
#  - ventanas: maximizar, restaurar, mover, cambiar de tamaño por la esquina,
#    minimizar a un icono y recuperar, cerrar con el botón y con Alt+F4;
#  - Calculadora: 12+30=42 con el teclado; 9x9=81, 81/4=20,25 y la división
#    entre cero con el ratón;
#  - Reloj: el temporizador (la esfera cambia sola) y el diálogo de opciones
#    con casilla y opciones;
#  - «Sistema» abierto desde el menú del Centro;
#  - diálogos de mensaje, salida ordenada a Hispa-DOS sin perder memoria, y
#    el modo VGA de 16 colores.
# Deja capturas en build/ventanas3_*.png.
# Hispa Ventanas stage 3 test: windows, messages, controls and first programs.
set -u
cd "$(dirname "$0")/.."
# EMM=1: con HSPEMM cargado (modo V86; HSPDPMI pasa a ser cliente VCPI)
EMMLINE=""; [ -n "${EMM:-}" ] && EMMLINE='DEVICE=A:\\HISPADOS\\HSPEMM.SYS\r\n'
IMG=${1:-build/hispados.img}
B=build
T=$(mktemp -d)
CHK="python3 tests/tools/ventanas_check.py"
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() { if grep -qF -- "$1" $T/log.txt; then ok "$1"; else ko "$1"; fi; }
checkn() { if grep -qF -- "$1" $T/log.txt; then ko "(no debía salir) $1"; else ok "sin \"$1\""; fi; }
chk() { $CHK "$@" || fail=1; }
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; sleep "${2:-0.3}"; }
tecla() { local k; for k in "$@"; do mon "sendkey $k" 0.3; done; }
shot() { sleep 0.4; mon "screendump $T/$1.ppm" 0.6; }
send() { printf '%s\r' "$1"; sleep "${2:-1.5}"; }
muestra() { [ -f $T/$1.ppm ] && convert $T/$1.ppm $B/ventanas3_$2.png; }
# el ratón: ir a un punto (en pasos cortos), clic y doble clic
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
aparca() { ir 16 320; }                     # el cursor, donde no tape nada
# teclea un texto con el teclado español: letras, cifras, espacio, ñ y á
escribe() {
  local t="$1" i c
  for ((i=0; i<${#t}; i++)); do
    c="${t:$i:1}"
    case "$c" in
      ' ') tecla spc;;
      'ñ') tecla semicolon;;
      'á') tecla apostrophe a;;
      [A-Z]) tecla "shift-$(echo $c | tr A-Z a-z)";;
      *) tecla "$c";;
    esac
  done
}
run() {
  local tmo=$1; shift
  rm -f $T/mon
  timeout $tmo qemu-system-i386 -display none -serial stdio -monitor unix:$T/mon,server,nowait \
    -parallel none -m 16 -drive file=$T/f.img,format=raw,if=floppy,index=0 -boot a -no-reboot "$@" > $T/raw 2>&1
  tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 > $T/log.txt
}

echo "== Hispa Ventanas, etapa 3: ventanas, controles y programas =="
cp "$IMG" $T/f.img
printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\n'"$EMMLINE"'FILES=30\r\n' > $T/CONFIG.SYS
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
{ sleep 6
  send 'KEYB SP' 2
  send 'A:\HISPADOS\HSPDPMI' 2
  send 'A:\HISPADOS\HSPRATON' 2
  send 'BIN\XMSTEST' 2.5
  send 'MEM' 2
  send 'VENTANAS' 4
  shot t1
  # ---- Bloc de notas ----
  tecla right ret; sleep 1                  # el grupo «Accesorios», con el teclado
  shot t1b
  tecla ret; sleep 1.2; aparca              # y su primer icono
  shot t2
  escribe "Hola mundo"; tecla ret; escribe "año está"
  shot t3
  clic 170 125                              # menú Archivo, con el ratón
  shot t4
  tecla down down down down
  shot t5
  tecla ret; sleep 1                        # Guardar como...
  aparca
  shot t6
  escribe "prueba"; tecla ret; sleep 1.5
  shot t7
  tecla shift-home ctrl-c end ctrl-v        # copiar la línea y pegarla detrás
  shot t8
  tecla alt-a down down ret; sleep 1.5      # Archivo > Guardar, con el teclado
  tecla alt-e up ret; sleep 1               # Edición > Buscar...
  shot t9a
  escribe "mundo"; tecla ret; sleep 1
  shot t9
  escribe "X"                               # sustituye lo seleccionado
  tecla alt-f4; sleep 1                     # cerrar con cambios: pregunta
  shot t10
  tecla tab tab                             # el foco, a «Cancelar»
  shot t10b
  tecla esc; sleep 0.8
  shot t11
  tecla alt-a down down ret; sleep 1.5      # guardar otra vez
  tecla alt-a down ret; sleep 1.2           # Archivo > Abrir...
  shot t12
  clic 551 382                              # flecha de abajo de la lista
  shot t13
  doble 300 285; sleep 1.5                  # doble clic en README.TXT
  shot t14
  # ---- maximizar, restaurar, mover, tamaño, minimizar ----
  clic 637 103
  shot t15
  clic 767 41
  shot t16
  arrastra 300 100 340 140; aparca
  shot t17
  arrastra 708 468 668 428; aparca
  shot t18
  clic 616 143
  shot t19
  clic 64 556
  shot t20
  # ---- Calculadora ----
  clic 200 64; tecla ret right ret; sleep 1.2   # Centro > Accesorios > Calculadora
  shot t21
  tecla 1 2 kp_add 3 0 kp_enter
  shot t22
  clic 532 149; clic 624 187; clic 670 187; clic 624 187; clic 647 301   # C 9 x 9 =
  shot t22b
  clic 670 149; clic 532 225; clic 647 301                               # / 4 =
  shot t23
  clic 670 149; clic 532 301; clic 647 301                               # / 0 =
  shot t24
  clic 532 149
  # ---- Reloj ----
  clic 200 64; tecla ret right ret; sleep 1.2   # Centro > Accesorios > Reloj
  shot t25
  sleep 2.2
  shot t26
  tecla alt-r ret; sleep 1                  # Reloj > Opciones...
  shot t27
  clic 297 259                              # casilla: sin aguja de segundos
  clic 297 313                              # opción: solo las cifras
  aparca
  shot t28
  tecla ret; sleep 1.2
  shot t29
  clic 747 343                              # cerrar el Reloj con su botón
  shot t30
  clic 560 72; tecla alt-f4; sleep 1        # y la Calculadora con Alt+F4
  shot t31
  tecla right; tecla alt-a ret; sleep 1.2   # «Sistema», con Archivo > Abrir
  shot t31b
  tecla alt-f4; sleep 0.8
  # ---- diálogos de mensaje y salida ----
  tecla alt-y ret; sleep 1                  # Ayuda > Acerca de...
  shot t32
  tecla ret; sleep 0.8
  clic 475 65; sleep 0.8                    # cerrar el Centro: pregunta
  shot t33
  tecla ret; sleep 2
  shot t34
  send 'ECHO TRAS-SALIR'
  # ---- VGA de 16 colores, y salir con el botón de la barra ----
  send 'VENTANAS /VGA' 4
  RX=320; RY=240
  shot v1
  clic 596 13; sleep 1; tecla ret; sleep 2
  send 'BIN\XMSTEST' 2.5
  send 'MEM' 2
  send 'ECHO SIGUE-VIVO'
  mon quit
} | run 330 -vga std

check "Teclado español instalado."
check "Instalado: clientes de 16 y de 32 bits, memoria de XMS."
# -- el Centro de programas --
chk $T/t1.ppm tam 800 600 cursor 400 300 \
  marco 36 52 452 190 activa titulo 36 52 activa "Centro de programas" \
  texto 60 79 0 15 fina "rchivo" texto 140 79 0 15 fina "uda" \
  texto 66 155 9 12 fina "Principal" texto 166 155 0 8 fina "Accesorios" \
  texto 286 155 0 8 fina "Juegos" texto 54 197 7 8 fina "Doble clic o Intro para abrir un grupo." \
  px 690 12 7 "altavoz gris: no hay HSPSND" \
  px 468 60 8 "botón de cerrar del Centro" px 475 65 13 "aspa del botón de cerrar" \
  px 300 10 1 "barra superior" texto 736 5 8 4 fina "Salir" px 491 150 0 "sombra del Centro"
# -- el grupo «Accesorios», en su ventana (alargada para que quepa la segunda
# fila: Pintura, que es un módulo .HVP y entra sola) --
chk $T/t1b.ppm marco 224 164 452 207 activa titulo 224 164 activa "Accesorios" titulo 36 52 no "Centro de programas" \
  texto 238 267 9 12 fina "Bloc de notas" texto 350 267 0 8 fina "Calculadora" \
  texto 478 267 0 8 fina "Reloj" texto 574 267 0 8 fina "Sistema" texto 166 155 9 7 fina "Acceso" \
  texto 262 343 0 8 fina "Pintura"
# -- Bloc de notas: escribir --
chk $T/t2.ppm marco 150 90 520 340 activa titulo 150 90 activa "Bloc de notas - (sin nombre)" \
  titulo 36 52 no "Centro de programas" texto 174 117 0 15 fina "rchivo" \
  px 400 300 9 "cuadro de texto en blanco" px 660 300 15 "barra de desplazamiento del texto"
chk $T/t3.ppm texto 156 138 0 9 fina "Hola mundo" texto 156 154 0 9 fina "año est" \
  px 220 160 4 "cursor de texto al final de la segunda línea"
# -- menú con el ratón y con las flechas --
chk $T/t4.ppm px 162 118 12 "título «Archivo» marcado" texto 183 139 0 8 fina "uevo" \
  texto 183 159 0 8 fina "brir..." px 300 215 8 "desplegable" px 314 200 0 "sombra del desplegable"
chk $T/t5.ppm px 165 199 12 "«Guardar como...» marcado" texto 175 199 9 12 fina "Guardar "
# -- diálogo de archivo --
chk $T/t6.ppm marco 222 150 356 300 activa titulo 222 150 activa "Guardar como" \
  titulo 150 90 no "Bloc de notas - (sin nombre)" texto 240 187 0 8 fina "Nombre del archivo:" \
  texto 240 237 0 8 fina "Carpeta: A:\\" texto 246 261 0 9 fina "KERNEL.SYS" \
  texto 328 408 0 6 fina "Aceptar" texto 416 408 0 6 fina "Cancelar"
chk $T/t7.ppm marco 150 90 520 340 activa titulo 150 90 activa "Bloc de notas - prueba.TXT"
# -- portapapeles --
chk $T/t8.ppm texto 156 154 0 9 fina "año estáaño est"
# -- buscar (diálogo de pedir texto) --
chk $T/t9a.ppm marco 230 225 340 150 activa titulo 230 225 activa "Buscar" \
  texto 248 264 0 8 fina "Texto que se busca:"
chk $T/t9.ppm texto 196 138 9 12 fina "mund" texto 156 138 0 9 fina "Hola " \
  titulo 150 90 activa "Bloc de notas - prueba.TXT"
# -- cerrar con cambios: pregunta; Tab mueve el foco; Esc cancela --
chk $T/t10.ppm marco 243 234 314 131 activa titulo 243 234 activa "Bloc de notas" \
  texto 269 275 0 8 fina "El texto ha cambiado." texto 269 293 0 8 fina "¿Quiere guardar los cambios?" \
  texto 156 138 0 9 fina "Hola X" px 274 326 4 "foco en «Sí»" px 454 326 6 "«Cancelar» sin foco"
chk $T/t10b.ppm px 454 326 4 "foco en «Cancelar» tras dos Tab" px 274 326 6 "«Sí» sin foco"
chk $T/t11.ppm marco 150 90 520 340 activa titulo 150 90 activa "Bloc de notas - prueba.TXT" \
  texto 156 138 0 9 fina "Hola " px 204 144 4 "cursor de texto tras la X"
# -- abrir: lista con barra de desplazamiento y doble clic --
chk $T/t12.ppm titulo 222 150 activa "Abrir" texto 246 261 0 9 fina "KERNEL.SYS" texto 246 277 0 9 fina "COMMAND.COM"
chk $T/t13.ppm texto 246 261 0 9 fina "COMMAND.COM" texto 246 277 0 9 fina "README.TXT"
chk $T/t14.ppm titulo 150 90 activa "Bloc de notas - README.TXT" texto 164 138 0 9 fina "elcome to Hispa-DOS!"
# -- maximizar y restaurar --
chk $T/t15.ppm marco 0 28 800 572 activa titulo 0 28 activa "Bloc de notas - README.TXT" \
  texto 14 76 0 9 fina "elcome to Hispa-DOS!" px 300 10 1 "la barra superior sigue a la vista"
chk $T/t16.ppm marco 150 90 520 340 activa titulo 36 52 no "Centro de programas" \
  px 723 471 2 "fondo, otra vez a la vista" texto 164 138 0 9 fina "elcome to Hispa-DOS!"
# -- mover y cambiar de tamaño --
chk $T/t17.ppm marco 190 130 520 340 activa titulo 190 130 activa "Bloc de notas - README.TXT" \
  px 163 263 2 "fondo donde estaba la ventana" texto 54 197 7 8 fina "Doble clic o Int" \
  texto 204 178 0 9 fina "elcome to Hispa-DOS!"
chk $T/t18.ppm marco 190 130 479 299 activa px 675 439 2 "fondo donde llegaba la ventana" \
  px 659 300 15 "la barra de desplazamiento sigue al borde"
# -- minimizar y recuperar --
chk $T/t19.ppm titulo 224 164 activa "Accesorios" titulo 36 52 no "Centro de programas" px 403 400 2 "fondo donde estaba el Bloc" \
  texto 20 578 8 2 fina "Bloc de not" px 58 554 9 "icono del Bloc minimizado"
chk $T/t20.ppm marco 190 130 479 299 activa titulo 190 130 activa "Bloc de notas - README.TXT" \
  px 67 583 2 "el icono ya no está"
# -- Calculadora --
chk $T/t21.ppm marco 500 60 204 269 activa titulo 500 60 activa "Calculadora" \
  texto 676 101 0 9 fina "0" texto 528 141 0 6 fina "C" texto 620 179 0 6 fina "9"
chk $T/t22.ppm texto 668 101 0 9 fina "42"
chk $T/t22b.ppm texto 668 101 0 9 fina "81"
chk $T/t23.ppm texto 644 101 0 9 fina "20,25"
chk $T/t24.ppm texto 644 101 0 9 fina "Error"
# -- Reloj: temporizador y opciones --
chk $T/t26.ppm marco 560 330 200 252 activa titulo 560 330 activa "Reloj" \
  px 660 383 1 "borde de la esfera" cambia 562 375 196 205 $T/t25.ppm 20
chk $T/t27.ppm titulo 268 210 activa "Opciones del reloj" px 297 259 4 "casilla marcada" \
  texto 312 251 0 8 fina "Aguja de los segundos" px 297 289 4 "opción «Esfera con agujas» elegida" \
  px 297 313 9 "opción «Solo las cifras» sin elegir"
chk $T/t28.ppm px 297 259 9 "casilla sin marcar" px 297 289 9 "opción «Esfera con agujas» sin elegir" \
  px 297 313 4 "opción «Solo las cifras» elegida"
chk $T/t29.ppm marco 560 330 200 252 activa px 660 383 8 "ya no hay esfera"
chk $T/t30.ppm px 707 471 2 "fondo donde estaba el Reloj"
chk $T/t31.ppm px 515 87 2 "fondo donde estaba la Calculadora" titulo 224 164 activa "Accesorios"
chk $T/t31b.ppm marco 300 330 400 150 activa titulo 300 330 activa "Sistema" \
  texto 316 365 0 8 fina "Pantalla: 800 x 600, 256 colores" texto 316 405 0 8 fina "Ratón: driver INT 33h (HSPRATON)"
# -- mensajes y salida --
chk $T/t32.ppm marco 190 216 420 167 activa titulo 190 216 activa "Acerca de Hispa Ventanas" \
  texto 216 257 0 8 fina "Hispa Ventanas 0.3" texto 216 293 0 8 fina "Hecho desde cero, en ensamblador y en español."
chk $T/t33.ppm marco 212 171 376 257 activa titulo 212 171 activa "Salir de Hispa Ventanas" \
  texto 238 210 0 8 negrita "¿Qué quiere hacer?" texto 350 249 0 6 fina "Volver a Hispa-DOS"
chk $T/t34.ppm tam 720 400
check "TRAS-SALIR"
# -- VGA de 16 colores --
chk $T/v1.ppm tam 640 480 cursor 320 240 marco 36 52 452 190 activa titulo 36 52 activa "Centro de programas" \
  texto 166 155 0 8 fina "Accesorios" px 600 300 8 "el grupo «Accesorios» sigue abierto"
n=$(grep -c "Hispa Ventanas ha terminado." $T/log.txt)
if [ "$n" = 2 ]; then ok "dos sesiones terminadas con su mensaje"; else ko "sesiones terminadas: $n (esperadas 2)"; fi
# -- el archivo guardado, en la imagen --
if mtype -i $T/f.img ::PRUEBA.TXT > $T/prueba.txt 2>/dev/null && \
   [ "$(od -An -tx1 $T/prueba.txt | tr -d ' \n')" = "$(printf 'Hola X\r\na\xa4o est\xa0a\xa4o est\xa0' | od -An -tx1 | tr -d ' \n')" ]; then
  ok "PRUEBA.TXT guardado en el disco: «Hola X», CR+LF, «año estáaño está» (página 437)"
else
  ko "PRUEBA.TXT: $(od -c $T/prueba.txt 2>/dev/null | head -3)"
fi
# -- no se pierde memoria --
a=$(grep "largest free KB" $T/log.txt | head -1); b=$(grep "largest free KB" $T/log.txt | tail -1)
if [ -n "$a" ] && [ "$a" = "$b" ] && [ "$(grep -c 'largest free KB' $T/log.txt)" = 2 ]; then ok "memoria extendida igual al final ($(echo $b | sed 's/.*largest/mayor bloque/'))"; else ko "memoria extendida: «$a» y «$b»"; fi
a=$(grep "bytes libres" $T/log.txt | head -1); b=$(grep "bytes libres" $T/log.txt | tail -1)
if [ -n "$a" ] && [ "$a" = "$b" ] && [ "$(grep -c 'bytes libres' $T/log.txt)" = 2 ]; then ok "memoria convencional igual al final ($(echo $b))"; else ko "memoria convencional: «$a» y «$b»"; fi
check "XMS test OK"
check "SIGUE-VIVO"
checkn "HSPDPMI: excepción"
muestra t1 centro; muestra t3 bloc; muestra t4 menu; muestra t6 guardar_como; muestra t9 buscar
muestra t10 cambios; muestra t13 abrir; muestra t15 maximizada; muestra t18 tamano; muestra t19 minimizada
muestra t23 calculadora; muestra t26 reloj; muestra t28 opciones; muestra t29 reloj_cifras
muestra t32 acerca; muestra t33 salir; muestra v1 vga16
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/Ejecutando/,$p' $T/log.txt; fi
[ -n "${GUARDAR:-}" ] && cp $T/*.ppm "$GUARDAR"/
rm -rf $T
exit $fail
