#!/bin/bash
# Prueba de Hispa-System (docs/HISPASMS.md): el emulador HISPASMS.HVP y su
# juego propio, Buzo (BUZO.SMS).
#  1. El núcleo, fuera de QEMU. Los mismos archivos del emulador se montan
#     como programa de Linux (tests/hispasms/smsnativo.asm, solo con NASM):
#      - el ensamblador de Z80 (tools/asmz80.py) con todas las instrucciones;
#      - zexdoc y zexall, las pruebas del Z80 de libre distribución (GPL 2),
#        que no van en el proyecto: las baja tests/thirdparty/fetch_sms.sh (si
#        no hay red, se saltan). Se corren como programas de CP/M;
#      - VDPTEST.SMS, TMSTEST.SMS y MAPTEST (tests/hispasms/*.s), las ROM de
#        prueba propias: imagen, interrupciones, mandos, mapeadores (el de
#        Sega y el de Codemasters) y los modos del TMS9918;
#      - Buzo con las mismas teclas que luego se pulsan en QEMU: lo que tiene
#        que verse (guardado en tests/hispasms/esperado/).
#     (En un equipo que no ejecute programas de Linux de 32 bits esta parte
#     se salta y se usa lo guardado.)
#  2. En Hispa Ventanas, en QEMU, paso a paso (/P): el acceso en «Juegos»,
#     Buzo se abre solo, se juega con el teclado (empezar, nadar, disparar,
#     la pausa de la consola, perder) y la pantalla es punto por punto la
#     esperada, al doble y a tamaño normal; menús y diálogos; Abrir ROM con
#     el diálogo de archivos (VDPTEST.SMS) y un archivo que no es una ROM; la
#     mejor puntuación queda en BUZO.SAV y vuelve al abrir otra vez; salir de
#     Hispa Ventanas con el emulador abierto deja la memoria como al entrar.
#  3. A su velocidad, con una Sound Blaster 16: Intro (la pausa de la
#     consola) empieza la partida, el arpón y la música suenan por HSPSND y
#     se sale con el juego en marcha. Y otra vez con HD Audio (48.000 Hz en
#     estéreo, otros trozos).
#  4. Con HSPJOY y un mando «pulsado» por un programa de prueba (NESJOY.COM,
#     el de Hispa-Nes, con «1»: la cruceta a la derecha y el botón 1): la
#     partida empieza sin tocar el teclado.
#  5. /T: VDPTEST.SMS dentro de HISPASMS.HVP da lo mismo que fuera, y cuántos
#     cuadros por segundo salen en QEMU con Buzo, pintando en la ventana.
#  Y a su velocidad (partes pausa, raton y carpeta):
#   - pausa: al abrirse, Buzo está en pausa (dos capturas iguales, el letrero
#     y «Pausa -» en la barra); un clic en la imagen sigue; con la partida en
#     marcha, el menú Tamaño abierto la para (capturas iguales) y la calla
#     (no entra nada en la grabación de QEMU); al cerrarlo sigue; con P y un
#     menú abierto y cerrado sigue en pausa; P otra vez, sigue;
#   - raton: con el juego en marcha, un clic en cada título de la barra
#     (Archivo, Tamaño, Sonido, Ayuda) abre su menú a la primera y un clic en
#     una orden de cada uno la hace; Abrir ROM empieza en JUEGOS\SMS;
#   - carpeta: con Carpeta=A:\ROMS en HISPASMS.INI, Abrir ROM empieza en \ROMS,
#     abre OTRA.SMS de allí y vuelve a empezar allí.
#  Los cartuchos van en \HISPADOS\JUEGOS\SMS (también VDPTEST.SMS, para Abrir ROM).
# uso: run_hispasms_tests.sh [imagen]     SOLO="nucleo pasos" para algunas
#      ACTUALIZA=1 vuelve a guardar las imágenes esperadas
# Deja capturas en build/hispasms_*.png.
# Hispa-System (Master System-compatible emulator module) tests.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
B=build
T=$(mktemp -d)
CHK="python3 tests/tools/ventanas_check.py"
HS="python3 tests/tools/hispasms_check.py"
ESP=tests/hispasms/esperado
DL=tests/thirdparty/dl/sms
NAT=$B/smsnativo
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() { if grep -qF -- "$1" $T/log.txt; then ok "$1"; else ko "$1"; fi; }
checkn() { if grep -qF -- "$1" $T/log.txt; then ko "(no debía salir) $1"; else ok "sin \"$1\""; fi; }
chk() { $CHK "$@" || fail=1; }
hs() { $HS "$@" || fail=1; }
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; sleep "${2:-0.3}"; }
tecla() { local k; for k in "$@"; do mon "sendkey $k" 0.3; done; }
# escribe TEXTO: letras, cifras y punto, con el teclado
escribe() { local i c; for ((i = 0; i < ${#1}; i++)); do c=${1:i:1}; [ "$c" = . ] && c=dot; mon "sendkey $c" 0.15; done; }
shot() { sleep 0.4; mon "screendump $T/$1.ppm" 0.6; }
send() { printf '%s\r' "$1"; sleep "${2:-1.5}"; }
muestra() { [ -f $T/$1.ppm ] && convert $T/$1.ppm $B/hispasms_$2.png; }
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
aparca() { ir 16 320; }
run() {
  local tmo=$1; shift
  rm -f $T/mon
  timeout $tmo qemu-system-i386 -display none -serial stdio -monitor unix:$T/mon,server,nowait \
    -parallel none -m 16 -drive file=$T/f.img,format=raw,if=floppy,index=0 -boot a -no-reboot "$@" > $T/raw 2>&1
  tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 | grep -v '^audio:' > $T/log.txt
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
# disco PARÁMETROS: una copia de la imagen con CONFIG.SYS, las ROM de prueba
# en la raíz y un CENTRO.INI con el grupo «Juegos» abierto abajo y el acceso
# de Hispa-System con esos parámetros
disco() {
  cp "$IMG" $T/f.img
  mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
  mcopy -o -i $T/f.img $B/VDPTEST.SMS ::VDPTEST.SMS
  # los cartuchos van en \HISPADOS\JUEGOS\SMS: allí también VDPTEST.SMS y
  # un archivo que no es una ROM; y otra carpeta, \ROMS, con OTRA.SMS
  mcopy -o -i $T/f.img $B/VDPTEST.SMS ::HISPADOS/JUEGOS/SMS/VDPTEST.SMS
  mcopy -o -i $T/f.img $T/CONFIG.SYS ::HISPADOS/JUEGOS/SMS/NOROM.TXT
  mmd -i $T/f.img ::ROMS 2> /dev/null
  mcopy -o -i $T/f.img $B/VDPTEST.SMS ::ROMS/OTRA.SMS
  # HISPASMS.INI: la última carpeta de Abrir ROM (si se da)
  if [ -n "${2:-}" ]; then
    printf 'Carpeta=%s\r\n' "$2" > $T/HISPASMS.INI
    mcopy -o -i $T/f.img $T/HISPASMS.INI ::HISPADOS/JUEGOS/HISPASMS.INI
  fi
  local nmod=$(mdir -b -i $T/f.img ::HISPADOS/JUEGOS ::HISPADOS/PROGS 2>/dev/null | grep -c '\.HVP')
  printf '%s\r\n' '[Centro]' 'Ventana=36,52,452,190' 'Activo=0' "Modulos=$nmod" '' \
    '[Grupo]' 'Nombre=Juegos' 'Ventana=300,380,452,190' 'Estado=1' '' \
    '[Acceso]' 'Nombre=Hispa-System' 'Programa=A:\HISPADOS\JUEGOS\HISPASMS.HVP' "Parametros=$1" > $T/CENTRO.INI
  mcopy -o -i $T/f.img $T/CENTRO.INI ::HISPADOS/CENTRO.INI
  RX=400; RY=300
}
entrar() {
  sleep 6
  send 'KEYB SP' 2
  send 'A:\HISPADOS\HSPRATON' 2
  local o; for o in "$@"; do send "$o" 3; done
  send 'BIN\XMSTEST' 2.5
  send 'VENTANAS' 5
  aparca
}
salir() {
  clic 756 13; sleep 1; tecla ret; sleep 2.5
  shot zz
  send 'ECHO TRAS-SALIR'
  send 'BIN\XMSTEST' 2.5
  send 'ECHO SIGUE-VIVO'
  mon quit
}
salida_bien() {
  chk $T/zz.ppm tam 720 400
  check "Hispa Ventanas ha terminado."
  check "TRAS-SALIR"
  check "SIGUE-VIVO"
  checkn "HSPDPMI: excepción"
  xms_igual
  if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/Ejecutando/,$p' $T/log.txt; fi
  [ -n "${GUARDAR:-}" ] && cp $T/*.ppm "$GUARDAR"/
  rm -f $T/*.ppm
}
# el letrero de la pausa, en medio de la imagen al doble (144,75; 512x384):
# una caja de 240x44 con «En pausa» en negrita y debajo lo que hay que hacer
CX=240; CY=237; TTX=126; TTY=180  # (el cuadro de Abrir ROM: «Carpeta: ...»; el de Teclas y mandos)
LX1=$((144 + (512 - 240) / 2 + (240 - 64) / 2)); LY1=$((75 + (384 - 44) / 2 + 5))
LX2=$((144 + (512 - 240) / 2 + 12)); LY2=$((75 + (384 - 44) / 2 + 23))
# tamano ARCHIVO: lo que mide ahora la grabación del sonido (sin salir por la tubería)
tamano() { stat -c %s $T/sms.wav > "$1" 2> /dev/null || echo 44 > "$1"; }
# el sonido de la grabación entre dos medidas: silencio_wav WAV DESDE HASTA TEXTO
# (todo por debajo de 600) o sonido_wav (algo por encima de 2.000)
trozo_wav() {
  python3 -c "
import sys, struct
d = open(sys.argv[1], 'rb').read()
a, b = int(open(sys.argv[2]).read()), int(open(sys.argv[3]).read())
a = max(44, a) & ~3; b = min(len(d), b) & ~3
s = struct.unpack('<%dh' % ((b - a) // 2), d[a:b]) if b > a else ()
print(len(s), max((abs(v) for v in s), default=0))" "$@"
}
silencio_wav() {
  local r; r=$(trozo_wav $1 $2 $3); set -- $r "$4"
  if [ "$2" -lt 600 ]; then ok "en silencio $3 ($1 muestras grabadas, la mayor $2)"; else ko "suena $3: $1 muestras, la mayor $2"; fi
}
sonido_wav() {
  local r; r=$(trozo_wav $1 $2 $3); set -- $r "$4"
  if [ "$2" -gt 2000 ]; then ok "suena $3 ($1 muestras, la mayor $2)"; else ko "no suena $3: $1 muestras, la mayor $2"; fi
}
# en_barra CAPTURA TEXTO DESCRIPCIÓN: el texto está en la barra de estado (y 460)
en_barra() {
  local x
  for x in $(seq 150 8 470); do
    if $CHK $1 texto $x 460 0 6 fina "$2" > /dev/null 2>&1; then ok "$3"; return; fi
  done
  ko "$3: no está «$2» en la barra de estado"
}
# el valor de CLAVE= en HISPASMS.LOG (sacado del disco a $T/sms.log)
del_log() { sed -n "s/^$1=//p" $T/sms.log | head -1; }

# ---- lo que tiene que verse: Buzo con las teclas de la prueba ----
T_JUEGO="z"
T_TIROS="$T_JUEGO right right right z z spc z z up up z spc z z"
T_PAUSA="$T_TIROS ret"
T_FIN="$T_PAUSA ret $(printf 'spc %.0s' $(seq 25))"
# esperada NOMBRE: el archivo con esa imagen (la recién hecha, si se ha
# podido; si no, la guardada)
esperada() { if [ -f $T/e_$1.pgm ]; then echo $T/e_$1.pgm; else echo $ESP/$1.pgm.gz; fi; }
# hace NOMBRE ROM TECLAS...: corre el núcleo y compara con la guardada
hace() {
  local nombre=$1 rom=$2 n; shift 2
  n=$($HS guion $T/guion.bin "$@")
  $NAT $rom $n $T/e_$nombre.pgm $T/guion.bin $T/e_$nombre.mem || { ko "smsnativo no ha podido con $nombre"; return; }
  guarda $nombre
  hs iguales $T/e_$nombre.pgm $ESP/$nombre.pgm.gz "$nombre: la imagen del núcleo es la guardada"
}
guarda() {
  if [ -n "${ACTUALIZA:-}" ]; then
    mkdir -p $ESP; gzip -9n < $T/e_$1.pgm > $ESP/$1.pgm.gz; echo "  (guardada $ESP/$1.pgm.gz)"
  fi
}
# prueba_rom ROM NOMBRE ESPERADO [ram]: una ROM de prueba propia
prueba_rom() {
  local r
  $NAT $1 60 $T/e_$2.pgm - $T/x.mem || { ko "smsnativo no ha podido con $1"; return; }
  r=$($HS resultado $T/x.mem ${4:-})
  if [ "$r" = "$3" ]; then ok "$(basename $1): $r"; else ko "$(basename $1): $r"; fi
}

make -s smsprogs > $T/make.txt 2>&1 || { cat $T/make.txt; ko "no se han podido hacer smsnativo y las ROM de prueba"; }
nativo=0
if [ -x $NAT ] && $NAT $B/VDPTEST.SMS 1 $T/x.pgm 2> /dev/null; then nativo=1; fi

for parte in ${SOLO:-nucleo pasos vivo mando pausa raton carpeta prueba}; do
case $parte in
nucleo)
  echo "== El núcleo de Hispa-System, fuera de QEMU: el ensamblador, el Z80, las ROM de prueba e imágenes esperadas =="
  # el ensamblador
  if python3 tools/asmz80.py tests/hispasms/asmz80_todas.s $T/todas.bin > /dev/null && \
     [ "$(xxd -p -c 16 $T/todas.bin | tr a-f A-F | sed 's/../& /g; s/ $//')" = "$(tr a-f A-F < tests/hispasms/asmz80_todas.hex | sed 's/ *$//')" ]; then
    ok "asmz80.py: las $(grep -c '^ *[a-z]' tests/hispasms/asmz80_todas.s) formas de las instrucciones del Z80 dan lo guardado"
  else
    ko "asmz80.py: asmz80_todas.s no da asmz80_todas.hex"
  fi
  if [ $nativo = 0 ]; then
    echo "  (este equipo no ejecuta programas de Linux de 32 bits: el resto de esta parte se salta)"
    continue
  fi
  # zexdoc y zexall, a la vez (unos 20 segundos cada una)
  tests/thirdparty/fetch_sms.sh > /dev/null 2>&1
  for z in zexdoc zexall; do
    [ -s $DL/$z.com ] && $NAT -c $DL/$z.com > $T/$z.txt 2>&1 &
  done
  prueba_rom $B/VDPTEST.SMS vdptest "0 VDPTEST BIEN"
  guarda vdptest
  hs iguales $T/e_vdptest.pgm $ESP/vdptest.pgm.gz "VDPTEST: la pantalla del modo 4 es la guardada"
  prueba_rom $B/TMSTEST.SMS tmstest "0 TMSTEST BIEN"
  guarda tmstest
  hs iguales $T/e_tmstest.pgm $ESP/tmstest.pgm.gz "TMSTEST: la pantalla del modo gráfico II es la guardada"
  python3 tools/asmz80.py tests/hispasms/maptest.s $T/MAPSEGA.SMS -D BANCO1=0xFFFE -D BANCO2=0xFFFF -D SUMA=0 > /dev/null
  python3 tools/asmz80.py tests/hispasms/maptest.s $T/MAPCM.SMS -D BANCO1=0x4000 -D BANCO2=0x8000 -D SUMA=0x1234 > /dev/null
  python3 tools/asmz80.py tests/hispasms/maptest.s $T/MAPNO.SMS -D BANCO1=0x4000 -D BANCO2=0x8000 -D SUMA=0 > /dev/null
  prueba_rom $T/MAPSEGA.SMS mapsega "0 MAPTEST BIEN" ram
  prueba_rom $T/MAPCM.SMS mapcm "0 MAPTEST BIEN" ram
  r=$($NAT $T/MAPNO.SMS 10 $T/x.pgm - $T/x.mem && $HS resultado $T/x.mem ram)
  case $r in "0 "*) ko "sin la cabecera de Codemasters se ha tomado por uno de Codemasters ($r)";;
             *) ok "sin su cabecera, un cartucho no se toma por uno de Codemasters ($r)";; esac
  hace b_titulo $B/BUZO.SMS
  hace b_juego $B/BUZO.SMS $T_JUEGO
  hace b_tiros $B/BUZO.SMS $T_TIROS
  hace b_pausa $B/BUZO.SMS $T_PAUSA
  hace b_fin $B/BUZO.SMS $T_FIN
  # al abrirlo otra vez con la partida guardada: la mejor en el título
  head -c 32768 < <(tail -c +8193 $T/e_b_fin.mem) > $T/b.sav
  n=$($HS guion $T/guion.bin)
  $NAT $B/BUZO.SMS $n $T/e_b_titulo2.pgm $T/guion.bin - - $T/b.sav || ko "smsnativo no ha podido con b_titulo2"
  guarda b_titulo2
  hs iguales $T/e_b_titulo2.pgm $ESP/b_titulo2.pgm.gz "b_titulo2: la imagen del núcleo es la guardada"
  if [ "$(xxd -s 0x2000 -l 6 -p $T/e_b_fin.mem)" = "42555a000010" ] && [ "$(xxd -l 1 -p $T/e_b_fin.mem)" = 02 ]; then
    ok "Buzo: al acabar la partida, la mejor puntuación (10) queda en la RAM de la pila"
  else
    ko "Buzo: la RAM de la pila al acabar: $(xxd -s 0x2000 -l 6 -p $T/e_b_fin.mem), estado $(xxd -l 1 -p $T/e_b_fin.mem)"
  fi
  # el sonido de Buzo: la música suena desde el título
  n=$($HS guion $T/guion.bin $T_TIROS)
  $NAT $B/BUZO.SMS $n $T/x.pgm $T/guion.bin - $T/son.raw
  python3 -c "
import struct, sys
d = open(sys.argv[1], 'rb').read()
s = struct.unpack('<%dh' % (len(d) // 2), d)
seg = [s[i:i + 2205] for i in range(0, len(s), 2205)]
con = sum(1 for g in seg if max(g) - min(g) > 2000)
print(len(s), con, len(seg))" $T/son.raw > $T/son.txt
  read nm con nseg < $T/son.txt
  if [ "$nm" -ge $(( n * 22050 / 60 - 400 )) ] && [ "$con" -ge $(( nseg * 3 / 4 )) ]; then
    ok "Buzo: el sonido ($nm muestras a 22.050 Hz en $n cuadros; suena en $con de $nseg décimas de segundo)"
  else
    ko "Buzo: el sonido: $nm muestras, con sonido $con de $nseg"
  fi
  wait
  for z in zexdoc zexall; do
    if [ ! -s $DL/$z.com ]; then echo "  (sin $z.com: no se ha podido descargar; se salta)"; continue; fi
    nok=$(grep -c '  OK' $T/$z.txt); nmal=$(grep -c 'ERROR' $T/$z.txt)
    if grep -q "Tests complete" $T/$z.txt && [ "$nmal" = 0 ] && [ "$nok" = 67 ]; then
      ok "$z: las 67 pruebas del Z80, bien"
    else
      ko "$z: $nok bien y $nmal mal: $(grep ERROR $T/$z.txt | head -3 | tr '\n' ' ')"
    fi
  done
  ;;

pasos)
  echo "== Hispa-System paso a paso: Buzo con el teclado, punto por punto, menús, diálogos y la partida guardada =="
  disco '/P' 'A:\NOEXISTE'                  # (una carpeta que ya no está)
  { entrar
    shot p0
    tecla ret; sleep 4                        # se abre y arranca Buzo
    aparca
    shot p1
    tecla $T_JUEGO; sleep 1
    shot p2
    tecla ${T_TIROS#$T_JUEGO }; sleep 0.6
    shot p3
    tecla ret; sleep 0.6                      # la pausa de la consola
    shot p4
    tecla p; sleep 0.6                        # la del emulador
    shot p4b
    tecla p; sleep 0.4
    tecla alt-a; sleep 0.6                    # el menú Archivo
    shot p5
    tecla esc; sleep 0.4
    tecla alt-t ret; sleep 1                  # Tamaño x1
    shot p6
    tecla alt-t down ret; sleep 1             # y otra vez x2
    shot p7
    tecla alt-y down ret; sleep 1             # Ayuda, Acerca de
    shot p8
    tecla ret; sleep 0.6
    tecla ${T_FIN#$T_PAUSA }; sleep 0.6       # sigue, y sin hacer nada, hasta perder
    shot p9
    tecla alt-f4; sleep 1.5                   # al cerrar, la partida al disco
    tecla ret; sleep 4                        # otra vez: la mejor, de BUZO.SAV
    shot p10
    tecla alt-a ret; sleep 1.2                # Archivo, Abrir ROM...
    shot p11
    escribe vdptest.sms; tecla ret; sleep 3
    shot p12
    tecla alt-a ret; sleep 1.2
    escribe norom.txt; tecla ret; sleep 1.5   # no es una ROM
    shot p13
    tecla ret; sleep 0.6
    shot p14
    salir
  } | run 300 -vga std
  mcopy -n -i $T/f.img ::HISPADOS/JUEGOS/SMS/BUZO.SAV $T/b.sav 2> /dev/null
  ini=$(mtype -i $T/f.img ::HISPADOS/JUEGOS/HISPASMS.INI 2> /dev/null | tr -d '\r')
  chk $T/p0.ppm tam 800 600 marco 300 380 452 190 activa titulo 300 380 activa "Juegos" \
    texto 318 483 9 12 fina "Hispa-System"
  chk $T/p1.ppm marco 142 30 516 449 activa titulo 142 30 activa "Hispa-System" texto 150 460 0 6 fina "BUZO.SMS"
  hs pantalla $T/p1.ppm 144 75 2 $(esperada b_titulo) "Buzo se abre solo: el título"
  hs pantalla $T/p2.ppm 144 75 2 $(esperada b_juego) "botón 1 (Z): empieza la partida"
  hs pantalla $T/p3.ppm 144 75 2 $(esperada b_tiros) "flechas y Z: el buzo nada, dispara y da a un pez"
  hs pantalla $T/p4.ppm 144 75 2 $(esperada b_pausa) "Intro: la pausa de la consola (NMI): PAUSA en el marcador"
  chk $T/p4b.ppm texto 150 460 0 6 fina "Pausa - BUZO.SMS"
  chk $T/p5.ppm texto 175 79 9 12 fina "brir ROM..."
  chk $T/p6.ppm marco 142 30 260 257 activa texto 150 268 0 6 fina "BUZO.SMS"
  hs pantalla $T/p6.ppm 144 75 1 $(esperada b_pausa) "Tamaño x1: la misma imagen, a su tamaño"
  chk $T/p7.ppm marco 142 30 516 449 activa
  hs pantalla $T/p7.ppm 144 75 2 $(esperada b_pausa) "Tamaño x2: otra vez al doble"
  chk $T/p8.ppm titulo 134 180 activa "Acerca de Hispa-System" \
    texto 160 221 0 8 fina "Hispa-System: emulador propio, compatible con los"
  hs pantalla $T/p9.ppm 144 75 2 $(esperada b_fin) "sin moverse, tres choques: fin de la partida y nuevo récord"
  if [ "$(xxd -l 6 -p $T/b.sav 2> /dev/null)" = "42555a000010" ] && [ "$(stat -c %s $T/b.sav)" = 32768 ]; then
    ok "BUZO.SAV: 32 KB con la mejor puntuación (10) al cerrar la ventana"
  else
    ko "BUZO.SAV: $(xxd -l 8 -p $T/b.sav 2>&1)"
  fi
  hs pantalla $T/p10.ppm 144 75 2 $(esperada b_titulo2) "al abrirlo otra vez, el título con la mejor puntuación (de BUZO.SAV)"
  chk $T/p11.ppm titulo 222 150 activa "Abrir ROM (.SMS)" \
    texto $CX $CY 0 8 fina "Carpeta: A:\HISPADOS\JUEGOS\SMS"
  if [ "$ini" = 'Carpeta=A:\HISPADOS\JUEGOS\SMS' ]; then
    ok "la carpeta del INI ya no estaba: Abrir ROM empieza en la de los cartuchos y apunta la nueva en HISPASMS.INI"
  else
    ko "HISPASMS.INI: «$ini»"
  fi
  chk $T/p12.ppm texto 150 460 0 6 fina "VDPTEST.SMS"
  hs pantalla $T/p12.ppm 144 75 2 $(esperada vdptest) "Abrir ROM: VDPTEST.SMS dentro de la ventana, la pantalla verde"
  chk $T/p13.ppm titulo 182 243 activa "Hispa-System" texto 208 284 0 8 fina "Este archivo no es una ROM de la consola (.SMS)."
  chk $T/p14.ppm texto 150 460 0 6 fina "VDPTEST.SMS"
  hs pantalla $T/p14.ppm 144 75 2 $(esperada vdptest) "tras el aviso sigue el cartucho que había"
  muestra p0 grupo; muestra p1 titulo; muestra p3 juego; muestra p4 pausa; muestra p5 menu; muestra p6 x1
  muestra p8 acerca; muestra p9 fin; muestra p10 mejor; muestra p11 abrir; muestra p12 vdptest; muestra p13 noesrom
  salida_bien
  ;;

vivo)
  echo "== Hispa-System a su velocidad, con una Sound Blaster 16: teclas mantenidas, sonido y salir con él abierto =="
  disco ''
  { entrar 'A:\HISPADOS\HSPSND'
    tecla ret; sleep 4
    aparca
    shot v1
    tecla ret; sleep 2                        # Intro: la pausa de la consola, que empieza
    shot v2
    tecla z; sleep 0.5; tecla z; sleep 0.5; tecla z; sleep 1.5
    tecla p; sleep 0.8                        # pausa del emulador: la imagen se queda quieta
    shot v3
    sleep 1
    shot v4
    tecla p; sleep 1
    salir                                     # con el juego en marcha
  } | run 200 -vga std -audiodev wav,id=snd0,path=$T/sms.wav -device sb16,audiodev=snd0
  check "Sound Blaster 16"
  chk $T/v1.ppm marco 142 30 516 449 activa titulo 142 30 activa "Hispa-System" px 690 12 8 "altavoz encendido: hay HSPSND"
  hs pantalla $T/v1.ppm 144 75 2 $(esperada b_titulo) "el título a 60 cuadros por segundo (el agua quieta)" 0 0 256 80
  if $HS pantalla $T/v2.ppm 144 75 2 $(esperada b_titulo) "x" 0 24 256 56 > /dev/null; then
    ko "Intro no ha empezado la partida: sigue el título"
  else
    ok "Intro (la pausa de la consola) con la tecla mantenida: empieza la partida (ya no está el título)"
  fi
  chk $T/v3.ppm texto 150 460 0 6 fina "Pausa - BUZO.SMS" igual 144 75 512 384 $T/v4.ppm 144 75 0
  python3 tests/tools/audio.py check $T/sms.wav > $T/audio.txt 2>&1
  if grep -q "tramo" $T/audio.txt; then
    ok "la música y el arpón suenan por HSPSND ($(grep -c tramo $T/audio.txt) tramos con sonido en la grabación)"
  else
    ko "no ha sonado nada: $(tr '\n' ' ' < $T/audio.txt)"
  fi
  muestra v2 vivo; muestra v3 vivo_pausa
  salida_bien
  # y con HD Audio: 48.000 Hz, 16 bits, estéreo, en trozos de 2.048 tramas
  disco ''
  { entrar 'A:\HISPADOS\HSPSND'
    tecla ret; sleep 4
    aparca
    tecla ret; sleep 2
    tecla z; sleep 0.5; tecla z; sleep 0.5; tecla z; sleep 2
    shot h1
    salir
  } | run 200 -vga std -audiodev wav,id=snd0,path=$T/hda.wav -device intel-hda -device hda-micro,audiodev=snd0
  check "Intel HD Audio"
  chk $T/h1.ppm marco 142 30 516 449 activa texto 150 460 0 6 fina "BUZO.SMS"
  python3 tests/tools/audio.py check $T/hda.wav > $T/audio.txt 2>&1
  if grep -q "tramo" $T/audio.txt; then
    ok "con HD Audio también suena ($(grep -c tramo $T/audio.txt) tramos con sonido en la grabación)"
  else
    ko "con HD Audio no ha sonado nada: $(tr '\n' ' ' < $T/audio.txt)"
  fi
  salida_bien
  ;;

mando)
  echo "== Hispa-System con un mando de HSPJOY: la partida empieza sin tocar el teclado =="
  [ -f $B/NESJOY.COM ] || make -s $B/NESJOY.COM > /dev/null 2>&1   # (el de Hispa-Nes)
  disco ''
  mcopy -o -i $T/f.img $B/NESJOY.COM ::NESJOY.COM
  { entrar 'A:\HISPADOS\HSPJOY /R' 'NESJOY 1'
    tecla ret; sleep 6                        # se abre; el mando tiene el botón 1 pulsado
    aparca
    tecla p; sleep 0.8
    shot m1
    tecla p; sleep 0.5
    salir
  } | run 150 -vga std
  check "NESJOY: mando de prueba entregado a HSPJOY."
  if $HS pantalla $T/m1.ppm 144 75 2 $(esperada b_titulo) "x" 0 24 256 56 > /dev/null; then
    ko "el botón 1 del mando no ha empezado la partida: sigue el título"
  else
    ok "el botón 1 del mando empieza la partida, sin teclado (ya no está el título)"
  fi
  hs pantalla $T/m1.ppm 144 75 2 $(esperada b_juego) "el marcador de la partida" 0 0 56 8
  chk $T/m1.ppm texto 150 460 0 6 fina "Pausa - BUZO.SMS"
  muestra m1 mando
  salida_bien
  ;;

pausa)
  echo "== Hispa-System a su velocidad: empieza en pausa, se para con los menús (callado) y sigue como estaba =="
  disco ''
  { entrar 'A:\HISPADOS\HSPSND'
    tecla ret; sleep 4                        # se abre: Buzo, en pausa
    aparca
    shot a1; sleep 1; shot a2                 # quieta, con el letrero
    clic 400 300; sleep 1                     # un clic en la imagen: a jugar
    aparca
    shot a3
    tecla ret; sleep 2                        # Intro: empieza la partida
    shot a4; sleep 0.5; shot a5               # se mueve
    tamano $T/s0
    clic 254 65; sleep 0.8                    # el menú Tamaño: se para y calla
    shot b1
    tamano $T/s1; sleep 2; tamano $T/s2
    shot b2
    tecla esc; sleep 1                        # se cierra: sigue
    shot b3; sleep 0.5; shot b4
    tecla p; sleep 0.8                        # P: la pausa, con el letrero
    shot c1
    clic 186 65; sleep 0.8                    # el menú Archivo, con la pausa
    shot c2
    tecla esc; sleep 1                        # al cerrarlo, sigue en pausa
    aparca
    shot c3; sleep 0.5; shot c4
    tecla p; sleep 1                          # P otra vez: sigue
    shot c5; sleep 0.5; shot c6
    salir
  } | run 200 -vga std -audiodev wav,id=snd0,path=$T/sms.wav -device sb16,audiodev=snd0
  chk $T/a1.ppm texto $LX1 $LY1 0 8 negrita "En pausa" texto $LX2 $LY2 0 8 fina "Pulse una tecla o haga clic" \
    texto 150 460 0 6 fina "Pausa - BUZO.SMS" igual 144 75 512 384 $T/a2.ppm 144 75 0
  hs pantalla $T/a1.ppm 144 75 2 $(esperada b_titulo) "al abrirse, en pausa: el título (sin el letrero)" 0 0 256 80
  chk $T/a3.ppm cambia 280 245 240 44 $T/a2.ppm 1000 texto 150 460 0 6 fina "BUZO.SMS  "
  chk $T/a4.ppm cambia 144 75 512 384 $T/a5.ppm 200
  chk $T/b1.ppm texto 247 99 0 8 fina "amaño x" igual 380 80 270 370 $T/b2.ppm 380 80 0
  chk $T/b3.ppm cambia 144 75 512 384 $T/b4.ppm 200
  chk $T/c1.ppm texto $LX1 $LY1 0 8 negrita "En pausa" texto 150 460 0 6 fina "Pausa - BUZO.SMS"
  chk $T/c2.ppm texto 215 79 0 8 fina "ROM..."
  chk $T/c3.ppm texto $LX1 $LY1 0 8 negrita "En pausa" texto 150 460 0 6 fina "Pausa - BUZO.SMS" \
    igual 144 75 512 384 $T/c4.ppm 144 75 0
  chk $T/c5.ppm cambia 144 75 512 384 $T/c6.ppm 200 texto 150 460 0 6 fina "BUZO.SMS  "
  silencio_wav $T/sms.wav $T/s1 $T/s2 "con el menú abierto"
  sonido_wav $T/sms.wav $T/s0 $T/s1 "jugando, antes de abrir el menú"
  muestra a1 en_pausa; muestra b1 menu_parada
  salida_bien
  ;;

raton)
  echo "== Hispa-System con el juego en marcha: cada menú de la barra con el ratón y una orden de cada uno, a la primera =="
  disco ''
  { entrar 'A:\HISPADOS\HSPSND'
    tecla ret; sleep 4
    aparca
    tecla ret; sleep 1.5                      # sigue y empieza la partida
    clic 254 65                               # Tamaño
    shot r1
    clic 262 87; sleep 1.2                    # Tamaño x1
    shot r2
    clic 254 65
    clic 262 107; sleep 1.2                   # Tamaño x2
    shot r3
    clic 318 65                               # Sonido
    shot r4
    clic 340 87; sleep 1                      # Poner o quitar el sonido
    shot r5
    clic 378 65                               # Ayuda
    shot r6
    clic 400 87; sleep 1                      # Teclas y mandos...
    shot r7
    tecla ret; sleep 0.6
    clic 186 65                               # Archivo
    shot r8
    clic 200 155; sleep 1                     # Pausa o continuar
    shot r9
    clic 186 65
    clic 200 87; sleep 1.2                    # Abrir ROM...
    shot r10
    tecla esc; sleep 0.6
    salir
  } | run 200 -vga std -audiodev wav,id=snd0,path=$T/sms.wav -device sb16,audiodev=snd0
  chk $T/r1.ppm texto 247 99 0 8 fina "amaño x"
  chk $T/r2.ppm marco 142 30 260 257 activa
  chk $T/r3.ppm marco 142 30 516 449 activa
  chk $T/r4.ppm texto 367 79 0 8 fina "quitar el "
  en_barra $T/r5.ppm "sin sonido" "Sonido: el sonido se quita (la barra dice «sin sonido»)"
  chk $T/r6.ppm texto 375 99 0 8 fina "cerca de Hispa-System..."
  chk $T/r7.ppm titulo $TTX $TTY activa "Teclas y mandos"
  chk $T/r8.ppm texto 215 79 0 8 fina "ROM..." texto 175 147 0 8 fina "ausa o continuar (P)"
  chk $T/r9.ppm texto 150 460 0 6 fina "Pausa - BUZO.SMS" texto $LX1 $LY1 0 8 negrita "En pausa"
  chk $T/r10.ppm titulo 222 150 activa "Abrir ROM (.SMS)" \
    texto $CX $CY 0 8 fina "Carpeta: A:\HISPADOS\JUEGOS\SMS"
  muestra r1 menu_tamano; muestra r4 menu_sonido; muestra r6 menu_ayuda; muestra r8 menu_archivo
  muestra r10 abrir_carpeta
  salida_bien
  ;;

carpeta)
  echo "== Hispa-System: Abrir ROM recuerda la última carpeta (HISPASMS.INI) =="
  disco '/P' 'A:\ROMS'
  { entrar
    tecla ret; sleep 4
    aparca
    tecla alt-a ret; sleep 1.2                # Abrir ROM: en \ROMS
    shot k1
    escribe otra.sms; tecla ret; sleep 3
    shot k2
    tecla alt-a ret; sleep 1.2                # y otra vez en \ROMS
    shot k3
    tecla esc; sleep 0.6
    salir
  } | run 200 -vga std
  chk $T/k1.ppm titulo 222 150 activa "Abrir ROM (.SMS)" texto $CX $CY 0 8 fina "Carpeta: A:\ROMS"
  chk $T/k2.ppm texto 150 460 0 6 fina "OTRA.SMS"
  hs pantalla $T/k2.ppm 144 75 2 $(esperada vdptest) "OTRA.SMS, de \\ROMS: la pantalla verde"
  chk $T/k3.ppm texto $CX $CY 0 8 fina "Carpeta: A:\ROMS"
  salida_bien
  ;;

prueba)
  echo "== /T: VDPTEST.SMS dentro de HISPASMS.HVP y la velocidad en QEMU =="
  disco 'A:\VDPTEST.SMS /T=60'
  { entrar
    tecla ret; sleep 5
    aparca
    shot t1
    salir
  } | run 120 -vga std
  mtype -i $T/f.img ::HISPADOS/JUEGOS/HISPASMS.LOG 2> /dev/null | tr -d '\r' > $T/sms.log
  hs pantalla $T/t1.ppm 144 75 2 $(esperada vdptest) "VDPTEST.SMS: la pantalla verde"
  if [ "$(del_log Estado)" = 0 ] && [ "$(del_log Texto)" = "VDPTEST BIEN" ]; then
    ok "HISPASMS.LOG: las pruebas propias, bien, dentro del módulo"
  else
    ko "HISPASMS.LOG: Estado=$(del_log Estado) Texto=$(del_log Texto)"
  fi
  s=$($HS suma $(esperada vdptest))
  if [ "$(del_log Imagen)" = "$s" ]; then ok "la imagen es la misma que fuera de QEMU (suma $s)"; else ko "Imagen=$(del_log Imagen) y se esperaba $s"; fi
  salida_bien
  disco '/T=600 /V'
  { entrar
    tecla ret; sleep 12
    aparca
    shot t2
    salir
  } | run 120 -vga std
  mtype -i $T/f.img ::HISPADOS/JUEGOS/HISPASMS.LOG 2> /dev/null | tr -d '\r' > $T/sms.log
  cps=$(del_log CuadrosPorSegundo)
  echo "  (velocidad en este QEMU: ${cps:-?} cuadros por segundo con Buzo en la ventana $(del_log Modo | sed 's/ventana //'), en $(del_log Milisegundos) ms los 600)"
  if [ "${cps:-0}" -ge 60 ]; then ok "Buzo, pintando cada cuadro en la ventana: 60 cuadros por segundo o más"; else ko "solo $cps cuadros por segundo"; fi
  if [ "$(del_log Modo)" = "ventana x2" ] && [ "$(del_log Mapa)" = 1 ]; then ok "HISPASMS.LOG: ventana x2, mapeador de Sega"; else ko "HISPASMS.LOG: $(tr '\n' ' ' < $T/sms.log)"; fi
  salida_bien
  ;;
esac
done
rm -rf $T
exit $fail
