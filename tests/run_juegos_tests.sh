#!/bin/bash
# Prueba de los juegos de Hispa Ventanas (programas aparte, .HVP): Culebra y
# Campo de minas. Arranca en QEMU, maneja el entorno con el ratón y el teclado
# del monitor de QEMU, saca capturas y las comprueba punto a punto
# (tests/tools/ventanas_check.py); los archivos de récords se miran en la
# imagen con mtools.
#  1.ª sesión (sin CENTRO.INI: se crea con los grupos por omisión):
#  - el grupo «Juegos» trae un acceso directo por cada .HVP de
#    \HISPADOS\JUEGOS, con el nombre y el icono que lleva el módulo;
#  - Culebra se abre como una ventana, se dibuja, la culebra anda sola al
#    pulsar una flecha y P la para; Campo de minas, lo mismo; al cerrarlos el
#    grupo vuelve a estar delante;
#  - salir: CENTRO.INI con los accesos y la memoria extendida como al entrar.
#  2.ª sesión (CENTRO.INI a mano, con los parámetros de prueba /S=n y /P;
#  «Modulos» cuenta también PINTURA.HVP, que está en \HISPADOS\PROGS):
#  - Culebra paso a paso con la semilla fija: llega a la fruta (10 puntos),
#    choca con el borde, sale «¡Nuevo récord!», CULEBRA.REC queda en el disco
#    y la partida siguiente enseña el récord;
#  - Campo de minas con la semilla fija: la primera losa no tiene mina y abre
#    las de alrededor; banderín con el botón derecho y con Mayús+clic;
#  - el tablero de prueba: un número, un banderín, se gana, sale el aviso con
#    el tiempo y MINAS.REC queda en el disco; F2 y el teclado;
#  - «Buscar programas...» encuentra los .HVP sueltos de A:\JUEGOS (y no el
#    archivo que se llama .HVP sin serlo); el que pide una interfaz más
#    nueva que la que hay no se carga y lo dice;
#  - dos juegos abiertos a la vez y salir con ellos abiertos: la memoria
#    queda como al entrar.
# Deja capturas en build/juegos_*.png.
# Hispa Ventanas games test (HVP modules): Culebra and Campo de minas.
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
rec() { if mtype -i $T/f.img "::HISPADOS/JUEGOS/$1" 2>/dev/null | tr -d '\r' | iconv -f CP437 -t UTF-8 | grep -qxE -- "$2"; then ok "$1: $2"; else ko "$1 sin la línea «$2»: $(mtype -i $T/f.img "::HISPADOS/JUEGOS/$1" 2>&1 | tr -d '\r' | iconv -f CP437 -t UTF-8 | tr '\n' ' ')"; fi; }
chk() { $CHK "$@" || fail=1; }
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; sleep "${2:-0.3}"; }
tecla() { local k; for k in "$@"; do mon "sendkey $k" 0.3; done; }
shot() { sleep 0.4; mon "screendump $T/$1.ppm" 0.6; }
send() { printf '%s\r' "$1"; sleep "${2:-1.5}"; }
muestra() { [ -f $T/$1.ppm ] && convert $T/$1.ppm $B/juegos_$2.png; }
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

echo "== Juegos de Hispa Ventanas: el grupo Juegos por omisión, abrir y cerrar =="
cp "$IMG" $T/f.img
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
if mdir -i $T/f.img ::HISPADOS/JUEGOS/CULEBRA.HVP ::HISPADOS/JUEGOS/MINAS.HVP > /dev/null 2>&1; then
  ok "CULEBRA.HVP y MINAS.HVP están en \\HISPADOS\\JUEGOS del disquete"
else
  ko "faltan los .HVP en \\HISPADOS\\JUEGOS del disquete"
fi
if [ -f $B/hispados-usb.img ]; then
  if MTOOLS_SKIP_CHECK=1 mdir -i $B/hispados-usb.img@@1048576 ::HISPADOS/JUEGOS/CULEBRA.HVP ::HISPADOS/JUEGOS/MINAS.HVP > /dev/null 2>&1; then
    ok "y en \\HISPADOS\\JUEGOS de la imagen USB"
  else
    ko "faltan los .HVP en \\HISPADOS\\JUEGOS de la imagen USB"
  fi
fi
{ sleep 6
  send 'KEYB SP' 2
  send 'A:\HISPADOS\HSPRATON' 2
  send 'BIN\XMSTEST' 2.5
  send 'VENTANAS' 5
  # ---- el grupo Juegos ----
  doble 310 129; sleep 1
  aparca
  shot j1
  # ---- Culebra: se abre, anda sola y se para con P ----
  tecla ret; sleep 2
  aparca
  shot j2
  tecla right; sleep 1.2
  tecla p; sleep 0.5
  shot j3
  tecla alt-j; sleep 0.5                    # el menú Juego
  shot j4
  tecla esc
  tecla alt-f4; sleep 1
  # ---- Campo de minas ----
  tecla right ret; sleep 2
  aparca
  shot j5
  tecla alt-f4; sleep 1
  shot j6
  # ---- otra vez Culebra (la memoria del módulo se suelta y se vuelve a pedir) ----
  tecla left ret; sleep 2
  tecla alt-f4; sleep 1
  tecla ret; sleep 2
  shot j7
  tecla alt-f4; sleep 1
  # ---- salir ----
  clic 756 13; sleep 2.5
  shot j8
  send 'ECHO TRAS-SALIR'
  send 'BIN\XMSTEST' 2.5
  send 'ECHO SIGUE-VIVO'
  mon quit
} | run 120 -vga std

check "Teclado español instalado."
# -- el grupo: un acceso por módulo, con su nombre y su icono --
chk $T/j1.ppm tam 800 600 marco 248 188 452 207 activa titulo 248 188 activa "Juegos" \
  texto 286 291 9 12 fina "Culebra" texto 362 291 0 8 fina "Campo de minas" \
  px 303 255 11 "icono de Culebra: el cuerpo, verde oliva" px 317 255 2 "icono de Culebra: la cabeza" \
  px 405 253 11 "icono de Campo de minas: una losa" px 426 251 10 "icono de Campo de minas: el banderín"
# -- Culebra --
chk $T/j2.ppm marco 150 70 468 377 activa titulo 150 70 activa "Culebra" \
  texto 160 123 0 8 fina "Puntos: 0" texto 288 123 0 8 fina "Nivel: 1" texto 376 123 0 8 fina "Récord: 0" \
  texto 536 123 7 8 fina "Sin muros" texto 268 296 0 8 fina "Pulse una flecha para empezar" \
  px 280 189 2 "la cabeza de la culebra" px 263 188 10 "el collar del cuerpo" px 258 183 11 "el cuerpo" \
  px 165 154 8 "casilla crema" px 181 154 15 "casilla piedra"
chk $T/j3.ppm texto 364 274 4 8 negrita "Pausa" texto 292 296 0 8 fina "P o Espacio para seguir" \
  cambia 160 181 448 16 $T/j2.ppm 100
# (el menú: sin la letra subrayada de cada orden)
chk $T/j4.ppm texto 183 119 9 12 fina "ueva partida" texto 183 187 0 8 fina "elocidad" texto 183 227 0 8 fina "elocidad" \
  px 300 127 12 "la primera orden del menú Juego, marcada"
# -- Campo de minas --
chk $T/j5.ppm marco 120 60 204 283 activa titulo 120 60 activa "Campo de minas" \
  texto 156 118 0 9 negrita "10" px 222 126 10 "el rombo de arriba: centro ocre" px 222 118 12 "el rombo: añil (jugando)" \
  px 142 161 11 "una losa tapada" px 222 241 11 "otra losa tapada" px 131 150 1 "el marco del tablero"
chk $T/j6.ppm marco 248 188 452 207 activa titulo 248 188 activa "Juegos" texto 362 291 9 12 fina "Campo de minas" \
  px 300 70 6 "donde estaba Campo de minas se ve otra vez el Centro"
chk $T/j7.ppm marco 150 70 468 377 activa titulo 150 70 activa "Culebra" texto 160 123 0 8 fina "Puntos: 0"
# -- salida --
chk $T/j8.ppm tam 720 400
check "Hispa Ventanas ha terminado."
check "TRAS-SALIR"
check "SIGUE-VIVO"
checkn "HSPDPMI: excepción"
xms_igual
ini "Modulos=7"; ini "Nombre=Juegos"; ini "Nombre=Culebra"; ini "Programa=A:\\HISPADOS\\JUEGOS\\CULEBRA.HVP"
ini "Nombre=Campo de minas"; ini "Programa=A:\\HISPADOS\\JUEGOS\\MINAS.HVP"
if [ "$(grep -E '^(Nombre|\[Grupo\])' $T/ini.txt | tr '\n' ' ')" = "[Grupo] Nombre=Principal Nombre=Editar Nombre=Gestor Nombre=Información Nombre=Hispa-DOS Nombre=Configuración [Grupo] Nombre=Accesorios Nombre=Bloc de notas Nombre=Calculadora Nombre=Reloj Nombre=Sistema Nombre=Pintura [Grupo] Nombre=Juegos Nombre=Culebra Nombre=Campo de minas Nombre=Bloques Nombre=Solitario Nombre=Tragabolas Nombre=Almacén " ]; then
  ok "CENTRO.INI: los juegos, en el grupo Juegos (y Pintura, en Accesorios)"
else
  ko "CENTRO.INI, grupos: $(grep -E '^(Nombre|\[Grupo\])' $T/ini.txt | tr '\n' ' ')"
fi
muestra j1 grupo; muestra j2 culebra; muestra j3 culebra_pausa; muestra j4 culebra_menu; muestra j5 minas
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/Ejecutando/,$p' $T/log.txt; cat $T/ini.txt; fi
[ -n "${GUARDAR:-}" ] && cp $T/*.ppm "$GUARDAR"/

echo "== Juegos de Hispa Ventanas: jugar (semilla fija), récords en el disco y dos a la vez =="
cp "$IMG" $T/f.img
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
mmd -i $T/f.img ::JUEGOS
mcopy -i $T/f.img $B/CULEBRA.HVP ::JUEGOS/OTRA.HVP
# uno que no es un módulo y otro que pide una interfaz que todavía no existe
head -c 3000 docs/VENTANAS.md > $T/ROTA.HVP
cp $B/CULEBRA.HVP $T/FUTURO.HVP
printf '\347\003' | dd of=$T/FUTURO.HVP bs=1 seek=6 conv=notrunc status=none
printf 'Del futuro\0' | dd of=$T/FUTURO.HVP bs=1 seek=32 conv=notrunc status=none
mcopy -i $T/f.img $T/ROTA.HVP $T/FUTURO.HVP ::JUEGOS/
printf '%s\r\n' '[Centro]' 'Ventana=36,52,452,190' 'Activo=0' 'Modulos=7' '' \
  '[Grupo]' 'Nombre=Juegos' 'Ventana=300,380,452,190' 'Estado=1' '' \
  '[Acceso]' 'Nombre=Culebra' 'Programa=A:\HISPADOS\JUEGOS\CULEBRA.HVP' 'Parametros=/S=7 /P' '' \
  '[Acceso]' 'Nombre=Minas 5' 'Programa=A:\HISPADOS\JUEGOS\MINAS.HVP' 'Parametros=/S=5' '' \
  '[Acceso]' 'Nombre=Minas prueba' 'Programa=A:\HISPADOS\JUEGOS\MINAS.HVP' 'Parametros=/P' > $T/CENTRO.INI
mcopy -o -i $T/f.img $T/CENTRO.INI ::HISPADOS/CENTRO.INI
RX=400; RY=300
{ sleep 6
  send 'KEYB SP' 2
  send 'A:\HISPADOS\HSPRATON' 2
  send 'BIN\XMSTEST' 2.5
  send 'VENTANAS' 5
  aparca
  shot k0
  # ---- Culebra, paso a paso: a por la fruta (está en la columna 1, fila 9) ----
  tecla ret; sleep 2
  shot k1
  tecla down down down down down down down
  shot k2
  tecla left left left left left left
  shot k3
  tecla left left; sleep 0.5                # la columna 0 y el borde
  shot k4
  tecla ret; sleep 0.5                      # otra partida
  shot k5
  tecla alt-f4; sleep 1
  # ---- Campo de minas con la semilla 5: la primera losa, banderines ----
  tecla right ret; sleep 2
  clic 222 241; aparca                      # la losa del centro
  shot m1
  derecho 142 161; aparca                   # banderín con el botón derecho
  shot m2
  ir 162 161
  mon "sendkey shift 1500" 0.3              # Mayús+clic
  mon "mouse_button 1" 0.25; mon "mouse_button 0" 1.5
  aparca
  shot m3
  derecho 162 161; aparca                   # y se quita con el derecho
  shot m4
  tecla alt-f4; sleep 1
  # ---- el tablero de prueba: se gana ----
  tecla right ret; sleep 2
  clic 162 181; aparca                      # un número
  shot n1
  derecho 142 161; aparca                   # el banderín en la mina
  shot n2
  clic 222 241; sleep 1.5; aparca           # el centro: se levanta todo lo demás
  shot n3
  tecla ret; sleep 0.8
  shot n4
  tecla f2; sleep 0.5                       # otra partida, con el teclado
  tecla b; sleep 0.5
  shot n5
  tecla alt-f4; sleep 1
  # ---- Buscar programas...: el .HVP suelto de A:\JUEGOS ----
  tecla alt-a up up ret; sleep 1
  tecla ret; sleep 3
  shot p1
  tecla ret; sleep 1
  aparca
  shot p2
  tecla right right ret; sleep 1.5          # el que pide una interfaz más nueva
  shot q1
  tecla ret; sleep 0.8
  tecla left left
  # ---- dos juegos a la vez, y salir con ellos abiertos ----
  tecla ret; sleep 2                        # Minas prueba
  clic 500 392                              # la ventana del grupo, delante
  tecla left left ret; sleep 2              # Culebra
  aparca
  shot z1
  clic 756 13; sleep 2.5
  shot z2
  send 'ECHO TRAS-SALIR'
  send 'BIN\XMSTEST' 2.5
  send 'ECHO SIGUE-VIVO'
  mon quit
} | run 200 -vga std

chk $T/k0.ppm tam 800 600 marco 300 380 452 190 activa titulo 300 380 activa "Juegos" \
  texto 338 483 9 12 fina "Culebra" texto 442 483 0 8 fina "Minas 5" texto 526 483 0 8 fina "Minas prueba"
# -- Culebra --
chk $T/k1.ppm marco 150 70 468 377 activa titulo 150 70 activa "Culebra" texto 160 123 0 8 fina "Puntos: 0" \
  px 280 189 2 "la cabeza, en su sitio" px 184 302 4 "la fruta, donde manda la semilla"
chk $T/k2.ppm px 280 301 2 "siete pasos abajo: la cabeza" px 263 300 15 "una casilla libre a su izquierda" \
  px 280 189 15 "donde empezó ya no hay culebra" px 184 302 4 "la fruta sigue ahí"
chk $T/k3.ppm texto 160 123 0 8 fina "Puntos: 10" px 184 301 2 "la cabeza, en la casilla de la fruta" \
  px 199 300 10 "el cuerpo detrás" px 247 300 10 "ha crecido: el cuerpo llega a la columna 5" px 263 300 15 "y no más allá" texto 288 123 0 8 fina "Nivel: 1"
chk $T/k4.ppm texto 328 274 4 8 negrita "¡Nuevo récord!" texto 308 296 0 8 fina "Intro: otra partida" \
  px 168 301 2 "la cabeza, en la columna 0" texto 160 123 0 8 fina "Puntos: 10"
chk $T/k5.ppm texto 160 123 0 8 fina "Puntos: 0" texto 376 123 0 8 fina "Récord: 10" \
  texto 268 296 0 8 fina "Pulse una flecha para empezar" px 280 189 2 "la cabeza, otra vez arriba"
rec CULEBRA.REC "Sin=10,0,0,0,0"; rec CULEBRA.REC "Con=0,0,0,0,0"; rec CULEBRA.REC "Velocidad=1"
# -- Campo de minas, semilla 5 --
chk $T/m1.ppm marco 120 60 204 283 activa titulo 120 60 activa "Campo de minas" texto 156 118 0 9 negrita "10" \
  px 222 241 8 "la primera losa, levantada y sin mina" texto 137 233 2 8 negrita "1" texto 157 233 12 8 negrita "2" \
  px 142 161 11 "las de la esquina siguen tapadas" px 222 118 12 "el rombo sigue añil: se está jugando"
chk $T/m2.ppm texto 156 118 0 9 negrita "9" px 141 156 10 "el banderín, en la losa" px 138 160 0 "y su palo"
chk $T/m3.ppm texto 156 118 0 9 negrita "8" px 161 156 10 "Mayús+clic: otro banderín"
chk $T/m4.ppm texto 156 118 0 9 negrita "9" px 161 156 11 "el botón derecho lo quita"
# -- el tablero de prueba --
chk $T/n1.ppm texto 156 118 0 9 negrita "2" texto 157 173 2 8 negrita "1" px 222 241 11 "lo demás sigue tapado"
chk $T/n2.ppm texto 156 118 0 9 negrita "1" px 141 156 10 "el banderín, en la mina"
chk $T/n3.ppm marco 234 234 332 131 activa titulo 234 234 activa "Campo despejado" \
  texto 260 293 0 8 fina "Es el mejor tiempo de este tablero." px 222 118 11 "el rombo, verde: se ha ganado"
chk $T/n4.ppm marco 120 60 204 283 activa texto 156 118 0 9 negrita "0" px 222 241 8 "todas levantadas" \
  px 301 316 10 "un banderín en la otra mina" px 219 128 9 "la marca blanca del rombo"
chk $T/n5.ppm texto 156 118 0 9 negrita "1" px 222 118 12 "F2: partida nueva (el rombo, añil)" \
  px 221 236 10 "B: banderín en la losa del teclado" px 142 161 11 "todo tapado otra vez"
rec MINAS.REC "Prueba=[1-9][0-9]*"; rec MINAS.REC "Pequeño=0"; rec MINAS.REC "Tablero=0"
# -- Buscar programas --
# (OTRA.HVP y FUTURO.HVP son módulos; ROTA.HVP no lo es y no entra)
chk $T/p1.ppm titulo 262 243 activa "Buscar programas" texto 288 284 0 8 fina "Accesos directos añadidos: 2"
chk $T/q1.ppm titulo 170 243 activa "Centro de programas" \
  texto 196 284 0 8 fina "Este programa necesita un Hispa Ventanas más nuevo."
chk $T/p2.ppm marco 300 380 452 190 activa texto 650 483 0 8 fina "Culebra" px 667 447 11 "con el icono del módulo"
# -- dos a la vez --
chk $T/z1.ppm marco 150 70 468 377 activa titulo 150 70 activa "Culebra" px 121 200 1 "el marco de Campo de minas, detrás" \
  texto 160 123 0 8 fina "Puntos: 0" texto 376 123 0 8 fina "Récord: 10"
chk $T/z2.ppm tam 720 400
check "Hispa Ventanas ha terminado."
check "TRAS-SALIR"
check "SIGUE-VIVO"
checkn "HSPDPMI: excepción"
xms_igual
ini "Programa=A:\\JUEGOS\\OTRA.HVP"; ini "Nombre=Del futuro"; ini "Programa=A:\\JUEGOS\\FUTURO.HVP"; ini "Parametros=/S=7 /P"; ini "Modulos=7"
muestra k3 culebra_fruta; muestra k4 culebra_record; muestra m3 minas_banderines; muestra n2 minas_prueba
muestra n3 minas_ganada; muestra z1 dos_a_la_vez
if grep -q 'ROTA.HVP' $T/ini.txt; then ko "CENTRO.INI: ROTA.HVP no es un módulo y tiene acceso directo"; else ok "CENTRO.INI: sin acceso para ROTA.HVP"; fi
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/Ejecutando/,$p' $T/log.txt; cat $T/ini.txt; fi
[ -n "${GUARDAR:-}" ] && cp $T/*.ppm "$GUARDAR"/
rm -rf $T
exit $fail
