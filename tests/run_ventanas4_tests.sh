#!/bin/bash
# Prueba de Hispa Ventanas: el Centro de programas con grupos y accesos
# directos, el lanzamiento de programas de Hispa-DOS («juegos») y el volumen.
# Arranca en QEMU con una Sound Blaster 16, maneja el entorno con el ratón y
# el teclado del monitor de QEMU, saca capturas y las comprueba punto a punto
# (tests/tools/ventanas_check.py). El «juego» es tests/progs/lanztest.asm:
# apunta en LANZ.TXT y en la pantalla sus parámetros, su carpeta, la memoria
# que le han dado, si hay servidor DPMI y el volumen del mezclador.
#  1.ª sesión (sin CENTRO.INI: se crea con los grupos por omisión):
#  - crear un grupo; «Buscar programas...» en A:\JUEGOS (el programa que se
#    llama como su carpeta, el único .EXE sin contar SETUP, la pregunta
#    cuando hay varios y un .BAT); propiedades de un acceso (parámetros,
#    icono, esperar una tecla);
#  - lanzar el acceso: el programa se ejecuta en modo texto, con sus
#    parámetros y en su carpeta, sin HSPDPMI en memoria y con XMS, y al
#    terminar Hispa Ventanas vuelve con el grupo abierto y delante;
#  - mover accesos a otro grupo: arrastrando el icono y con el menú; un .BAT;
#  - el volumen: altavoz, barra (80 % al entrar, lo lee de HSPSND) al 50 %,
#    silencio; el mezclador de la tarjeta
#    lo confirma desde el «juego»;
#  - salir: CENTRO.INI en el disco con todo, la memoria como al entrar y
#    HSPDPMI (lo había cargado el lanzador) fuera de la memoria.
#  2.ª sesión (con un CENTRO.INI escrito a mano, con comentarios y espacios):
#  - las ventanas salen donde dice el archivo; MEM desde un acceso directo
#    (la memoria libre para un juego); un acceso roto avisa; un acceso sin
#    «volver» deja en Hispa-DOS.
# Deja capturas en build/ventanas4_*.png.
# Hispa Ventanas test: program groups, shortcuts, launching DOS programs, volume.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
B=build
# el «juego» de prueba no va en la imagen: lo compila «make test» (o a mano)
[ -f $B/LANZTEST.COM ] || { echo "falta $B/LANZTEST.COM (make build/LANZTEST.COM)"; exit 1; }
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
shot() { sleep 0.4; mon "screendump $T/$1.ppm" 0.6; }
send() { printf '%s\r' "$1"; sleep "${2:-1.5}"; }
muestra() { [ -f $T/$1.ppm ] && convert $T/$1.ppm $B/ventanas4_$2.png; }
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
aparca() { ir 16 320; }
# al volver de un programa el cursor está otra vez en el centro
centro() { RX=400; RY=300; }
escribe() {
  local t="$1" i c
  for ((i=0; i<${#t}; i++)); do
    c="${t:$i:1}"
    case "$c" in
      ' ') tecla spc;;
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
  tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 | grep -v '^audio:' > $T/log.txt
  mtype -i $T/f.img ::HISPADOS/CENTRO.INI 2>/dev/null | tr -d '\r' | iconv -f CP437 -t UTF-8 > $T/ini.txt
}
libres() { grep "bytes libres" $T/log.txt | sed -n "$1p" | tr -dc '0-9'; }

echo "== Hispa Ventanas: grupos, accesos directos, lanzar programas y volumen =="
cp "$IMG" $T/f.img
printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nFILES=30\r\n' > $T/CONFIG.SYS
printf '@ECHO OFF\r\nECHO BAT-EN-MARCHA\r\nA:\\JUEGOS\\PRUEBA\\PRUEBA.COM /N DESDE-BAT\r\n' > $T/JUGAR.BAT
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
mmd -i $T/f.img ::JUEGOS ::JUEGOS/PRUEBA ::JUEGOS/DOS ::JUEGOS/TRES ::JUEGOS/BAT ::JUEGOS/VACIA
mcopy -i $T/f.img $B/LANZTEST.COM ::JUEGOS/PRUEBA/PRUEBA.COM
mcopy -i $T/f.img $B/LANZTEST.COM ::JUEGOS/PRUEBA/OTRO.COM
mcopy -i $T/f.img $B/LANZTEST.COM ::JUEGOS/DOS/JUEGO.EXE
mcopy -i $T/f.img $B/LANZTEST.COM ::JUEGOS/DOS/SETUP.EXE
mcopy -i $T/f.img $B/LANZTEST.COM ::JUEGOS/DOS/AYUDA.COM
mcopy -i $T/f.img $B/LANZTEST.COM ::JUEGOS/TRES/UNO.EXE
mcopy -i $T/f.img $B/LANZTEST.COM ::JUEGOS/TRES/OTRO.EXE
mcopy -i $T/f.img $T/JUGAR.BAT ::JUEGOS/BAT/JUGAR.BAT
{ sleep 6
  send 'KEYB SP' 2
  send 'A:\HISPADOS\HSPRATON' 2
  send 'A:\HISPADOS\HSPSND' 3
  send 'BIN\XMSTEST' 2.5
  send 'MEM' 2
  send 'VENTANAS' 5
  shot g1
  # ---- un grupo nuevo ----
  tecla alt-a down down ret; sleep 1        # Archivo > Nuevo grupo...
  escribe "Pruebas"
  shot g2
  tecla ret; sleep 1
  aparca
  shot g3
  # ---- Buscar programas... ----
  tecla alt-a up up ret; sleep 1
  shot g4
  tecla ret; sleep 3                        # A:\JUEGOS, tal cual
  shot g5
  tecla down ret; sleep 2                   # de TRES, el segundo
  shot g6
  tecla ret; sleep 1
  shot g7
  # ---- propiedades de «Prueba»: parámetros, icono y esperar una tecla ----
  tecla alt-a down down down ret; sleep 1   # Archivo > Propiedades...
  tecla tab tab tab tab
  escribe "uno dos"
  clic 382 337                              # el icono de música
  clic 184 401                              # casilla «Esperar una tecla»
  aparca
  shot g8
  tecla ret; sleep 1
  shot g9
  # ---- lanzarlo ----
  tecla ret; sleep 5
  shot g10
  tecla spc; sleep 1.5                      # la tecla que espera el programa
  shot g11
  tecla spc; sleep 6                        # «Pulse una tecla para continuar...»
  centro; aparca
  shot g12
  # ---- mover: arrastrar «Dos» al icono de «Juegos» y «Tres» con el menú ----
  arrastra 442 289 310 129; aparca
  shot g13
  tecla alt-a down down down down ret; sleep 1   # Archivo > Mover a otro grupo...
  shot g14
  tecla down down ret; sleep 1
  shot g15
  # ---- un .BAT: va y vuelve solo ----
  tecla ret; sleep 9
  centro; aparca
  shot g16
  # ---- el volumen ----
  clic 698 14; sleep 0.8
  aparca
  shot g17
  clic 670 92; aparca                       # la barra, al 50 %
  shot g18
  clic 576 125; aparca                      # casilla «Silencio»
  shot g19
  clic 576 125                              # y fuera
  clic 698 14; sleep 0.8                    # se cierra con otro clic en el altavoz
  aparca
  shot g20
  # ---- el volumen, visto desde un programa: el grupo «Juegos» y «Dos» ----
  clic 200 64; tecla right right ret; sleep 1
  shot g21
  tecla ret; sleep 5
  tecla spc; sleep 6
  centro; aparca
  shot g22
  # ---- salir ----
  clic 756 13; sleep 2.5
  shot g23
  send 'ECHO TRAS-SALIR'
  send 'A:\HISPADOS\HSPDPMI /D' 2
  send 'BIN\XMSTEST' 2.5
  send 'MEM' 2
  send 'TYPE A:\JUEGOS\PRUEBA\LANZ.TXT' 2
  send 'ECHO SIGUE-VIVO'
  mon quit
} | run 200 -vga std -audiodev none,id=snd0 -device sb16,audiodev=snd0

check "Teclado español instalado."
check "Sound Blaster 16"
check "Hispa Ventanas: cargando el servidor DPMI (HSPDPMI)..."
# -- el Centro recién creado: tres grupos y el altavoz encendido --
chk $T/g1.ppm tam 800 600 cursor 400 300 marco 36 52 452 190 activa titulo 36 52 activa "Centro de programas" \
  texto 66 155 9 12 fina "Principal" texto 166 155 0 8 fina "Accesorios" texto 286 155 0 8 fina "Juegos" \
  px 100 130 6 "icono de carpeta del grupo" px 690 12 8 "altavoz encendido: hay HSPSND"
# -- nuevo grupo --
chk $T/g2.ppm marco 230 225 340 150 activa titulo 230 225 activa "Nuevo grupo" \
  texto 248 264 0 8 fina "Nombre del grupo:"
chk $T/g3.ppm marco 272 212 452 190 activa titulo 272 212 activa "Pruebas" titulo 36 52 no "Centro de programas" \
  texto 386 155 0 8 fina "Pruebas" texto 290 273 7 8 fina "Este grupo está vacío."
# -- buscar programas --
chk $T/g4.ppm marco 230 225 340 150 activa titulo 230 225 activa "Buscar programas" \
  texto 248 264 0 8 fina "Carpeta donde están los juegos:"
chk $T/g5.ppm marco 250 175 300 250 activa titulo 250 175 activa "Buscar programas" \
  texto 268 212 0 8 fina "Programa principal de TRES:" texto 274 236 9 12 fina "UNO.EXE" texto 274 252 0 9 fina "OTRO.EXE"
chk $T/g6.ppm titulo 262 243 activa "Buscar programas" texto 288 284 0 8 fina "Accesos directos añadidos: 4"
chk $T/g7.ppm marco 272 212 452 190 activa titulo 272 212 activa "Pruebas" \
  texto 314 315 9 12 fina "Prueba" texto 430 315 0 8 fina "Dos" texto 530 315 0 8 fina "Tres" texto 638 315 0 8 fina "Bat"
# -- propiedades --
chk $T/g8.ppm marco 158 132 484 336 activa titulo 158 132 activa "Propiedades del acceso directo" \
  texto 176 175 0 8 fina "Nombre:" texto 176 207 0 8 fina "Programa:" texto 176 271 0 8 fina "Parámetros:" \
  px 184 377 4 "casilla «Volver» marcada" px 184 401 4 "casilla «Esperar una tecla» marcada" \
  px 364 322 12 "marco del icono de música elegido" px 184 322 8 "el primer icono ya no está marcado"
chk $T/g9.ppm marco 272 212 452 190 activa texto 314 315 9 12 fina "Prueba" px 345 285 0 "icono de música en «Prueba»"
# -- el programa, en modo texto, y la vuelta --
chk $T/g10.ppm tam 720 400
chk $T/g11.ppm tam 720 400
check "ARGS=[ uno dos]"
check "DIR=A:\\JUEGOS\\PRUEBA"
check "DPMI=NO"
check "XMS=SI"
check "LANZTEST: en marcha; pulse una tecla."
check "Pulse una tecla para continuar..."
chk $T/g12.ppm tam 800 600 marco 272 212 452 190 activa titulo 272 212 activa "Pruebas" titulo 36 52 no "Centro de programas" \
  texto 314 315 9 12 fina "Prueba" texto 638 315 0 8 fina "Bat" px 690 12 8 "altavoz encendido"
# -- mover --
chk $T/g13.ppm texto 314 315 0 8 fina "Prueba" texto 426 315 9 12 fina "Tres" texto 534 315 0 8 fina "Bat" \
  px 650 300 8 "ya no hay cuarto icono"
chk $T/g14.ppm marco 250 175 300 250 activa titulo 250 175 activa "Mover a otro grupo" \
  texto 268 212 0 8 fina "Grupo al que se lleva:" texto 274 236 9 12 fina "Principal" texto 274 268 0 9 fina "Juegos"
chk $T/g15.ppm texto 314 315 0 8 fina "Prueba" texto 430 315 9 12 fina "Bat" px 546 300 8 "ya no hay tercer icono"
# -- el .BAT --
check "BAT-EN-MARCHA"
check "ARGS=[ /N DESDE-BAT]"
chk $T/g16.ppm tam 800 600 marco 272 212 452 190 activa titulo 272 212 activa "Pruebas" texto 430 315 9 12 fina "Bat"
# -- volumen --
chk $T/g17.ppm marco 550 34 236 116 activa titulo 550 34 activa "Volumen" \
  texto 568 63 0 8 fina "Volumen: 80 %" px 660 92 12 "barra al 80 % (lo que dice HSPSND al entrar)" px 750 92 7 "el final de la barra, sin poner"
chk $T/g18.ppm texto 568 63 0 8 fina "Volumen: 50 %" px 600 92 12 "media barra puesta" px 740 92 7 "media barra sin poner"
chk $T/g19.ppm px 576 125 4 "casilla «Silencio» marcada" px 706 15 5 "altavoz con el aspa de silencio"
chk $T/g20.ppm px 600 100 2 "la ventanita del volumen se ha cerrado" px 706 15 1 "altavoz sin aspa"
check "MIX=D0"                              # 80 %, antes de tocar el volumen
check "MIX=98"                              # y 50 % después
# -- el otro grupo --
chk $T/g21.ppm marco 248 188 452 190 activa titulo 248 188 activa "Juegos" \
  texto 302 291 9 12 fina "Dos" texto 402 291 0 8 fina "Tres"
chk $T/g22.ppm marco 248 188 452 190 activa titulo 248 188 activa "Juegos" texto 302 291 9 12 fina "Dos"
# -- salida --
chk $T/g23.ppm tam 720 400
check "Hispa Ventanas ha terminado."
check "TRAS-SALIR"
check "HSPDPMI no está en memoria."
a=$(grep "largest free KB" $T/log.txt | head -1); b=$(grep "largest free KB" $T/log.txt | tail -1)
if [ -n "$a" ] && [ "$a" = "$b" ] && [ "$(grep -c 'largest free KB' $T/log.txt)" = 2 ]; then ok "memoria extendida igual al final ($(echo $b | sed 's/.*largest/mayor bloque/'))"; else ko "memoria extendida: «$a» y «$b»"; fi
a=$(libres 1); b=$(libres 2)
if [ -n "$a" ] && [ "$a" = "$b" ]; then ok "memoria convencional igual al final ($b bytes libres)"; else ko "memoria convencional: «$a» y «$b»"; fi
kb=$(grep -m1 '^KB=' $T/log.txt | tr -dc '0-9')
if [ -n "$kb" ] && [ -n "$a" ] && [ $(( a / 1024 - kb )) -le 4 ]; then ok "el programa recibe $kb KB de los $(( a / 1024 )) KB libres (el lanzador ocupa unos 2 KB)"; else ko "memoria del programa: KB=$kb de $a bytes libres"; fi
check "XMS test OK"
check "SIGUE-VIVO"
checkn "HSPDPMI: excepción"
# -- lo que ha quedado en el disco --
if mtype -i $T/f.img ::JUEGOS/PRUEBA/LANZ.TXT 2>/dev/null | tr -d '\r' | grep -qx 'ARGS=\[ uno dos\]'; then ok "LANZ.TXT del programa, en su carpeta"; else ko "LANZ.TXT: $(mtype -i $T/f.img ::JUEGOS/PRUEBA/LANZ.TXT 2>&1 | head -3)"; fi
if mtype -i $T/f.img ::JUEGOS/BAT/LANZ.TXT 2>/dev/null | tr -d '\r' | grep -qx 'ARGS=\[ /N DESDE-BAT\]'; then ok "LANZ.TXT del .BAT, en la carpeta del .BAT"; else ko "LANZ.TXT del .BAT: $(mtype -i $T/f.img ::JUEGOS/BAT/LANZ.TXT 2>&1 | head -3)"; fi
if mtype -i $T/f.img ::JUEGOS/DOS/LANZ.TXT 2>/dev/null | tr -d '\r' | grep -qx 'DIR=A:\\JUEGOS\\DOS'; then ok "LANZ.TXT de JUEGO.EXE, en la suya"; else ko "LANZ.TXT de DOS: $(mtype -i $T/f.img ::JUEGOS/DOS/LANZ.TXT 2>&1 | head -3)"; fi
ini "[Centro]"; ini "Ventana=36,52,452,190"; ini "Volumen=50"; ini "Silencio=0"
ini "Nombre=Pruebas"; ini "Ventana=272,212,452,190"; ini "Estado=1"; ini "Marcado=1"
ini "Programa=A:\\JUEGOS\\PRUEBA\\PRUEBA.COM"; ini "Carpeta=A:\\JUEGOS\\PRUEBA"; ini "Parametros=uno dos"
ini "Icono=musica"; ini "Pausa=1"; ini "Programa=A:\\JUEGOS\\DOS\\JUEGO.EXE"; ini "Programa=A:\\JUEGOS\\TRES\\OTRO.EXE"
ini "Programa=A:\\JUEGOS\\BAT\\JUGAR.BAT"; ini "Programa=:BLOC"; ini "Programa=A:\\HISPADOS\\EDITAR.COM"
# «Dos» y «Tres» han pasado al grupo Juegos; «Prueba» y «Bat» siguen en Pruebas
if [ "$(grep -E '^(Nombre|\[Grupo\])' $T/ini.txt | tr '\n' ' ')" = "[Grupo] Nombre=Principal Nombre=Editar Nombre=Gestor Nombre=Información Nombre=Hispa-DOS [Grupo] Nombre=Accesorios Nombre=Bloc de notas Nombre=Calculadora Nombre=Reloj Nombre=Sistema [Grupo] Nombre=Juegos Nombre=Dos Nombre=Tres [Grupo] Nombre=Pruebas Nombre=Prueba Nombre=Bat " ]; then
  ok "CENTRO.INI: cada acceso, en su grupo"
else
  ko "CENTRO.INI, grupos: $(grep -E '^(Nombre|\[Grupo\])' $T/ini.txt | tr '\n' ' ')"
fi
muestra g1 centro; muestra g3 grupo_nuevo; muestra g5 buscar; muestra g7 grupo; muestra g8 propiedades
muestra g10 programa; muestra g12 vuelta; muestra g14 mover; muestra g18 volumen; muestra g19 silencio; muestra g21 juegos
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/Ejecutando/,$p' $T/log.txt; cat $T/ini.txt; fi
[ -n "${GUARDAR:-}" ] && cp $T/*.ppm "$GUARDAR"/

echo "== Hispa Ventanas: CENTRO.INI a mano, MEM desde un acceso y accesos sin vuelta =="
cp "$IMG" $T/f.img
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
mmd -i $T/f.img ::JUEGOS ::JUEGOS/PRUEBA
mcopy -i $T/f.img $B/LANZTEST.COM ::JUEGOS/PRUEBA/PRUEBA.COM
printf '%s\r\n' '; escrito a mano' '' '[centro]' '  ventana = 300, 60, 380, 170' 'Activo=0' 'Desconocida=1' '' \
  '[GRUPO]' 'Nombre = Mis juegos' 'Ventana=100,300,460,200' 'Estado=1' '' \
  '[Acceso]' 'Nombre=Memoria' 'Icono=herramienta' 'Programa=A:\COMMAND.COM' 'Parametros=/C MEM' 'Carpeta=A:\' '' \
  '[Acceso]' 'Nombre=Sin volver' 'icono=JUEGO' 'Programa=A:\JUEGOS\PRUEBA\PRUEBA.COM' 'Parametros=/N' 'Volver=0' '' \
  '[Acceso]' 'Nombre=Roto' 'Programa=A:\NOESTA.EXE' '' \
  '[Grupo]' 'Nombre=Cerrado' > $T/CENTRO.INI
mcopy -o -i $T/f.img $T/CENTRO.INI ::HISPADOS/CENTRO.INI
RX=400; RY=300
{ sleep 6
  send 'KEYB SP' 2
  send 'A:\HISPADOS\HSPDPMI' 2
  send 'A:\HISPADOS\HSPRATON' 2
  send 'MEM' 2
  send 'VENTANAS' 5
  aparca
  shot h1
  tecla ret; sleep 8                        # «Memoria»: COMMAND /C MEM, y vuelve
  centro; aparca
  shot h2
  tecla right right ret; sleep 1            # «Roto»
  shot h3
  tecla ret; sleep 0.8
  tecla left ret; sleep 6                   # «Sin volver»: acaba en Hispa-DOS
  shot h4
  send 'ECHO TRAS-JUEGO'
  send 'MEM' 2
  send 'A:\HISPADOS\HSPDPMI /D' 2
  send 'ECHO SIGUE-VIVO'
  mon quit
} | run 90 -vga std
chk $T/h1.ppm tam 800 600 marco 100 300 460 200 activa titulo 100 300 activa "Mis juegos" \
  marco 300 60 380 170 no titulo 300 60 no "Centro de programas" \
  texto 138 403 9 12 fina "Memoria" texto 230 403 0 8 fina "Sin volver" texto 358 403 0 8 fina "Roto" \
  texto 326 163 9 7 fina "Mis juegos" texto 442 163 0 8 fina "Cerrado" px 690 12 7 "altavoz gris: no hay HSPSND"
chk $T/h2.ppm tam 800 600 marco 100 300 460 200 activa titulo 100 300 activa "Mis juegos"
chk $T/h3.ppm titulo 186 234 activa "Centro de programas" \
  texto 212 275 0 8 fina "No se encuentra el programa del acceso directo."
chk $T/h4.ppm tam 720 400
n=$(grep -c "bytes libres" $T/log.txt)
a=$(libres 1); m=$(libres 2); b=$(libres 3)
if [ "$n" = 3 ] && [ -n "$m" ] && [ $(( a - m )) -gt 0 ] && [ $(( a - m )) -lt 60000 ]; then
  ok "MEM desde un acceso directo: $m bytes libres (en Hispa-DOS, antes de entrar: $a)"
else
  ko "MEM desde un acceso directo: «$a», «$m», «$b» ($n veces)"
fi
if [ -n "$a" ] && [ "$a" = "$b" ]; then ok "memoria convencional igual al final ($b bytes libres)"; else ko "memoria convencional: «$a» y «$b»"; fi
check "ARGS=[ /N]"
check "DPMI=NO"
check "TRAS-JUEGO"
check "HSPDPMI: descargado; la memoria queda libre."
check "SIGUE-VIVO"
checkn "cargando el servidor DPMI"
checkn "Hispa Ventanas ha terminado."
checkn "HSPDPMI: excepción"
ini "Nombre=Mis juegos"; ini "Ventana=300,60,380,170"; ini "Nombre=Cerrado"; ini "Icono=juego"; ini "Volver=0"
muestra h1 ini_a_mano; muestra h3 roto
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/Ejecutando/,$p' $T/log.txt; cat $T/ini.txt; fi
[ -n "${GUARDAR:-}" ] && cp $T/*.ppm "$GUARDAR"/
rm -rf $T
exit $fail
