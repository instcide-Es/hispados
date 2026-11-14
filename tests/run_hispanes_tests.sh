#!/bin/bash
# Prueba de Hispa-Nes (docs/HISPANES.md): el emulador HISPANES.HVP y su juego
# propio, Cometas (HISPA1.NES).
#  1. El núcleo, fuera de QEMU. Los mismos archivos del emulador se montan
#     como programa de Linux (tests/hispanes/nesnativo.asm, solo con NASM) y
#     corren en milisegundos:
#      - CPUTEST.NES (tests/hispanes/cputest.s), la ROM de prueba propia: 24
#        pruebas de procesador, imagen, sonido y mando;
#      - MAPTESTn.NES (tests/hispanes/maptest.s y mkmaptest.py), también
#        propias: el cambio de bancos de los cartuchos 1, 2, 3, 4, 7 y 66, y
#        el contador de líneas del 4;
#      - nestest y unas cuantas ROM de prueba de libre distribución, que no
#        van en el proyecto: las baja tests/thirdparty/fetch_nes.sh (si no
#        hay red, se saltan). Con nestest se compara cada instrucción con el
#        registro de referencia: registros, banderas y ciclos;
#      - Cometas con las mismas teclas que luego se pulsan en QEMU: lo que
#        tiene que verse (y está guardado en tests/hispanes/esperado/).
#     (En un equipo que no ejecute programas de Linux de 32 bits esta parte
#     se salta y se usa lo guardado.)
#  2. En Hispa Ventanas, en QEMU, paso a paso (/P): el acceso en «Juegos»,
#     Cometas se abre solo, se juega con el teclado (empezar, mover, disparar,
#     pausa, perder) y la pantalla es punto por punto la esperada, al doble y
#     a tamaño normal; menús y diálogos; Abrir ROM con el diálogo de archivos
#     (CPUTEST.NES: pantalla verde), un archivo que no es una ROM y un
#     cartucho que no se conoce; la mejor puntuación queda en HISPA1.SAV y
#     vuelve al abrir otra vez; salir de Hispa Ventanas con el emulador
#     abierto deja la memoria como al entrar.
#  3. A su velocidad, con una Sound Blaster 16: Start empieza la partida
#     (teclas mantenidas), los disparos suenan por HSPSND y se sale con el
#     juego en marcha.
#  4. Con HSPJOY y un mando «pulsado» por un programa de prueba (NESJOY.COM:
#     QEMU no tiene mandos que mover): Start llega del mando y la partida
#     empieza sin tocar el teclado.
#  5. /T: CPUTEST.NES dentro de HISPANES.HVP da lo mismo que fuera, y cuántos
#     cuadros por segundo salen en QEMU pintando en la ventana.
#  Y a su velocidad (partes pausa, raton y carpeta):
#   - pausa: al abrirse, Cometas está en pausa (dos capturas iguales, el letrero
#     y «Pausa -» en la barra); un clic en la imagen sigue; con la partida en
#     marcha, el menú Tamaño abierto la para (capturas iguales) y la calla
#     (no entra nada en la grabación de QEMU); al cerrarlo sigue; con P y un
#     menú abierto y cerrado sigue en pausa; P otra vez, sigue;
#   - raton: con el juego en marcha, un clic en cada título de la barra
#     (Archivo, Tamaño, Sonido, Ayuda) abre su menú a la primera y un clic en
#     una orden de cada uno la hace; Abrir ROM empieza en JUEGOS\NES;
#   - carpeta: con Carpeta=A:\ROMS en HISPANES.INI, Abrir ROM empieza en \ROMS,
#     abre OTRA.NES de allí y vuelve a empezar allí.
#  Los cartuchos van en \HISPADOS\JUEGOS\NES (también CPUTEST.NES, para Abrir ROM).
# uso: run_hispanes_tests.sh [imagen]     SOLO="nucleo pasos" para algunas
#      ACTUALIZA=1 vuelve a guardar las imágenes esperadas
# Deja capturas en build/hispanes_*.png.
# Hispa-Nes (NES-compatible emulator module) tests.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
B=build
T=$(mktemp -d)
CHK="python3 tests/tools/ventanas_check.py"
HN="python3 tests/tools/hispanes_check.py"
ESP=tests/hispanes/esperado
DL=tests/thirdparty/dl/nes
NAT=$B/nesnativo
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() { if grep -qF -- "$1" $T/log.txt; then ok "$1"; else ko "$1"; fi; }
checkn() { if grep -qF -- "$1" $T/log.txt; then ko "(no debía salir) $1"; else ok "sin \"$1\""; fi; }
chk() { $CHK "$@" || fail=1; }
hn() { $HN "$@" || fail=1; }
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; sleep "${2:-0.3}"; }
tecla() { local k; for k in "$@"; do mon "sendkey $k" 0.3; done; }
# escribe TEXTO: letras, cifras y punto, con el teclado
escribe() { local i c; for ((i = 0; i < ${#1}; i++)); do c=${1:i:1}; [ "$c" = . ] && c=dot; mon "sendkey $c" 0.15; done; }
shot() { sleep 0.4; mon "screendump $T/$1.ppm" 0.6; }
send() { printf '%s\r' "$1"; sleep "${2:-1.5}"; }
muestra() { [ -f $T/$1.ppm ] && convert $T/$1.ppm $B/hispanes_$2.png; }
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
# de Hispa-Nes con esos parámetros
disco() {
  cp "$IMG" $T/f.img
  mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
  mcopy -o -i $T/f.img $B/CPUTEST.NES ::CPUTEST.NES
  { printf 'NES\x1a\x01\x01\x30\x60'; head -c 8 /dev/zero; head -c 24576 /dev/zero; } > $T/MAPA99.NES
  mcopy -o -i $T/f.img $T/MAPA99.NES ::MAPA99.NES
  # los cartuchos van en \HISPADOS\JUEGOS\NES (allí también CPUTEST.NES) y
  # otra carpeta, \ROMS, con OTRA.NES
  mcopy -o -i $T/f.img $B/CPUTEST.NES ::HISPADOS/JUEGOS/NES/CPUTEST.NES
  mmd -i $T/f.img ::ROMS 2> /dev/null
  mcopy -o -i $T/f.img $B/CPUTEST.NES ::ROMS/OTRA.NES
  # HISPANES.INI: la última carpeta de Abrir ROM (si se da)
  if [ -n "${2:-}" ]; then
    printf 'Carpeta=%s\r\n' "$2" > $T/HISPANES.INI
    mcopy -o -i $T/f.img $T/HISPANES.INI ::HISPADOS/JUEGOS/HISPANES.INI
  fi
  local nmod=$(mdir -b -i $T/f.img ::HISPADOS/JUEGOS ::HISPADOS/PROGS 2>/dev/null | grep -c '\.HVP')
  printf '%s\r\n' '[Centro]' 'Ventana=36,52,452,190' 'Activo=0' "Modulos=$nmod" '' \
    '[Grupo]' 'Nombre=Juegos' 'Ventana=300,380,452,190' 'Estado=1' '' \
    '[Acceso]' 'Nombre=Hispa-Nes' 'Programa=A:\HISPADOS\JUEGOS\HISPANES.HVP' "Parametros=$1" > $T/CENTRO.INI
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
# el letrero de la pausa, en medio de la imagen al doble (144,75; 512x480):
# una caja de 240x44 con «En pausa» en negrita y debajo lo que hay que hacer
CX=240; CY=237; TTX=126; TTY=180  # (el cuadro de Abrir ROM: «Carpeta: ...»; el de Teclas y mandos)
LX1=$((144 + (512 - 240) / 2 + (240 - 64) / 2)); LY1=$((75 + (480 - 44) / 2 + 5))
LX2=$((144 + (512 - 240) / 2 + 12)); LY2=$((75 + (480 - 44) / 2 + 23))
# tamano ARCHIVO: lo que mide ahora la grabación del sonido (sin salir por la tubería)
tamano() { stat -c %s $T/nes.wav > "$1" 2> /dev/null || echo 44 > "$1"; }
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
# en_barra CAPTURA TEXTO DESCRIPCIÓN: el texto está en la barra de estado (y 556)
en_barra() {
  local x
  for x in $(seq 150 8 470); do
    if $CHK $1 texto $x 556 0 6 fina "$2" > /dev/null 2>&1; then ok "$3"; return; fi
  done
  ko "$3: no está «$2» en la barra de estado"
}
# el valor de CLAVE= en HISPANES.LOG (sacado del disco a $T/nes.log)
del_log() { sed -n "s/^$1=//p" $T/nes.log | head -1; }

# ---- lo que tiene que verse: Cometas con las teclas de la prueba ----
T_JUEGO="ret"
T_TIROS="$T_JUEGO right right right x x spc x x spc x x x spc x x"
T_FIN="$T_TIROS $(printf 'spc %.0s' $(seq 43))"
# esperada NOMBRE: el archivo con esa imagen (la recién hecha, si se ha
# podido; si no, la guardada)
esperada() { if [ -f $T/e_$1.pgm ]; then echo $T/e_$1.pgm; else echo $ESP/$1.pgm.gz; fi; }
# hace NOMBRE ROM TECLAS...: corre el núcleo y compara con la guardada
hace() {
  local nombre=$1 rom=$2 n; shift 2
  n=$($HN guion $T/guion.bin "$@")
  $NAT $rom $n $T/e_$nombre.pgm $T/guion.bin $T/e_$nombre.mem || { ko "nesnativo no ha podido con $nombre"; return; }
  if [ -n "${ACTUALIZA:-}" ]; then
    mkdir -p $ESP; gzip -9n < $T/e_$nombre.pgm > $ESP/$nombre.pgm.gz; echo "  (guardada $ESP/$nombre.pgm.gz)"
  fi
  hn iguales $T/e_$nombre.pgm $ESP/$nombre.pgm.gz "$nombre: la imagen del núcleo es la guardada"
}
# rom6000 ARCHIVO.mem: el resultado que deja una ROM de prueba en 6000h
rom6000() { python3 -c "
import sys
m = open(sys.argv[1], 'rb').read()[0x800:]
bien = m[1:4] == b'\xde\xb0\x61'
t = m[4:m.index(0, 4)].decode('latin-1').strip().split('\n')[-1] if bien else '(sin resultado)'
print('%d %s' % (m[0] if bien else 255, t))" "$1"; }

make -s nesprogs > $T/make.txt 2>&1 || { cat $T/make.txt; ko "no se han podido hacer nesnativo y CPUTEST.NES"; }
nativo=0
if [ -x $NAT ] && $NAT $B/CPUTEST.NES 1 $T/x.pgm 2> /dev/null; then nativo=1; fi

for parte in ${SOLO:-nucleo pasos vivo mando pausa raton carpeta prueba}; do
case $parte in
nucleo)
  echo "== El núcleo de Hispa-Nes, fuera de QEMU: ROM de prueba e imágenes esperadas =="
  if [ $nativo = 0 ]; then
    echo "  (este equipo no ejecuta programas de Linux de 32 bits: esta parte se salta)"
    continue
  fi
  $NAT $B/CPUTEST.NES 60 $T/e_cputest.pgm - $T/c.mem
  r=$(rom6000 $T/c.mem)
  if [ "$r" = "0 CPUTEST BIEN" ]; then ok "CPUTEST.NES: las 24 pruebas propias ($r)"; else ko "CPUTEST.NES: $r"; fi
  [ -n "${ACTUALIZA:-}" ] && { mkdir -p $ESP; gzip -9n < $T/e_cputest.pgm > $ESP/cputest.pgm.gz; }
  hn iguales $T/e_cputest.pgm $ESP/cputest.pgm.gz "cputest: la imagen del núcleo es la guardada"
  tests/thirdparty/fetch_nes.sh > /dev/null 2>&1
  if [ -s $DL/nestest.nes ] && [ -s $DL/nestest.log ]; then
    python3 tests/hispanes/nestest.py $B/nesnativo_t $DL/nestest.nes $DL/nestest.log || fail=1
  else
    echo "  (sin nestest.nes: no se ha podido descargar; se salta)"
  fi
  for r in official_only:5000 apu_1-len_ctr apu_2-len_table apu_3-irq_flag apu_7-dmc_basics \
           vbl_01-vbl_basics vbl_03-vbl_clear_time vbl_04-nmi_control \
           mmc3_1-clocking mmc3_2-details mmc3_3-A12_clocking mmc3_5-MMC3 oam_read; do
    n=1800; case $r in *:*) n=${r#*:}; r=${r%:*};; esac
    [ -s $DL/$r.nes ] || { echo "  (sin $r.nes: se salta)"; continue; }
    $NAT $DL/$r.nes $n $T/x.pgm - $T/x.mem
    res=$(rom6000 $T/x.mem)
    case $res in "0 "*) ok "$r.nes: $res";; *) ko "$r.nes: $res";; esac
  done
  python3 tests/hispanes/mkmaptest.py $T
  for m in 1 2 3 4 7 66; do
    $NAT $T/MAPTEST$m.NES 60 $T/x.pgm - $T/x.mem
    res=$(rom6000 $T/x.mem)
    if [ "$res" = "0 MAPTEST BIEN" ]; then ok "cartucho del tipo $m: cada banco pedido es el que se ve (MAPTEST$m.NES)"; else ko "MAPTEST$m.NES: $res"; fi
  done
  hace h1_titulo $B/HISPA1.NES
  hace h1_juego $B/HISPA1.NES $T_JUEGO
  hace h1_tiros $B/HISPA1.NES $T_TIROS
  hace h1_fin $B/HISPA1.NES $T_FIN
  if [ "$(xxd -s 0x800 -l 8 -p $T/e_h1_fin.mem)" = "484e310000000200" ]; then
    ok "Cometas: al acabar la partida, la mejor puntuación (20) queda en la RAM de la pila"
  else
    ko "Cometas: la RAM de la pila al acabar: $(xxd -s 0x800 -l 8 -p $T/e_h1_fin.mem)"
  fi
  ;;

pasos)
  echo "== Hispa-Nes paso a paso: Cometas con el teclado, punto por punto, menús, diálogos y la partida guardada =="
  disco '/P' 'A:\'                          # (Abrir ROM empieza en la raíz)
  { entrar
    shot p0
    tecla ret; sleep 4                        # se abre y arranca Cometas
    aparca
    shot p1
    tecla $T_JUEGO; sleep 1
    shot p2
    tecla ${T_TIROS#$T_JUEGO }; sleep 0.6
    shot p3
    tecla p; sleep 0.6
    shot p4
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
    tecla ${T_FIN#$T_TIROS }; sleep 0.6       # sin hacer nada, hasta perder
    shot p9
    tecla alt-f4; sleep 1.5                   # al cerrar, la partida al disco
    tecla ret; sleep 4                        # otra vez: la mejor, de HISPA1.SAV
    tecla ret; sleep 1
    shot p10
    tecla alt-a ret; sleep 1.2                # Archivo, Abrir ROM...
    shot p11
    escribe cputest.nes; tecla ret; sleep 3
    shot p12
    tecla alt-a ret; sleep 1.2
    escribe readme.txt; tecla ret; sleep 1.5  # no es una ROM
    shot p13
    tecla ret; sleep 0.6
    tecla alt-a ret; sleep 1.2
    escribe mapa99.nes; tecla ret; sleep 1.5  # un cartucho que no se conoce
    shot p14
    tecla ret; sleep 0.6
    shot p15
    salir
  } | run 300 -vga std
  mcopy -n -i $T/f.img ::HISPADOS/JUEGOS/NES/HISPA1.SAV $T/h1.sav 2> /dev/null
  chk $T/p0.ppm tam 800 600 marco 300 380 452 190 activa titulo 300 380 activa "Juegos" \
    texto 330 483 9 12 fina "Hispa-Nes" px 352 459 15 "icono de Hispa-Nes: el mando" px 363 453 13 "icono: un botón granate"
  chk $T/p1.ppm marco 142 30 516 545 activa titulo 142 30 activa "Hispa-Nes" texto 150 556 0 6 fina "HISPA1.NES"
  hn pantalla $T/p1.ppm 144 75 2 $(esperada h1_titulo) "Cometas se abre solo: la pantalla del título"
  hn pantalla $T/p2.ppm 144 75 2 $(esperada h1_juego) "Intro (Start): empieza la partida"
  hn pantalla $T/p3.ppm 144 75 2 $(esperada h1_tiros) "flechas y X: la nave se mueve, dispara y da a un cometa"
  chk $T/p4.ppm texto 150 556 0 6 fina "Pausa - HISPA1.NES"
  chk $T/p5.ppm texto 175 79 9 12 fina "brir ROM..." px 300 87 12 "la primera orden del menú Archivo, marcada"
  chk $T/p6.ppm marco 142 30 260 305 activa texto 150 316 0 6 fina "HISPA1.NES"
  hn pantalla $T/p6.ppm 144 75 1 $(esperada h1_tiros) "Tamaño x1: la misma imagen, a su tamaño"
  chk $T/p7.ppm marco 142 30 516 545 activa
  hn pantalla $T/p7.ppm 144 75 2 $(esperada h1_tiros) "Tamaño x2: otra vez al doble"
  chk $T/p8.ppm titulo 134 180 activa "Acerca de Hispa-Nes" \
    texto 160 221 0 8 fina "Hispa-Nes: emulador propio, compatible con los cartuchos"
  hn pantalla $T/p9.ppm 144 75 2 $(esperada h1_fin) "sin moverse, tres choques: fin de la partida"
  if [ "$(xxd -l 8 -p $T/h1.sav 2> /dev/null)" = "484e310000000200" ] && [ "$(stat -c %s $T/h1.sav)" = 8192 ]; then
    ok "HISPA1.SAV: 8 KB con la mejor puntuación (20) al cerrar la ventana"
  else
    ko "HISPA1.SAV: $(xxd -l 8 -p $T/h1.sav 2>&1)"
  fi
  chk $T/p10.ppm igual 496 91 80 16 $T/p9.ppm 288 91 0 texto 150 556 0 6 fina "HISPA1.NES"
  chk $T/p11.ppm titulo 222 150 activa "Abrir ROM (.NES)" texto $CX $CY 0 8 fina "Carpeta: A:\\"
  chk $T/p12.ppm texto 150 556 0 6 fina "CPUTEST.NES"
  hn pantalla $T/p12.ppm 144 75 2 $(esperada cputest) "Abrir ROM: CPUTEST.NES dentro de la ventana, pantalla verde"
  chk $T/p13.ppm titulo 174 243 activa "Hispa-Nes" texto 200 284 0 8 fina "Este archivo no es una ROM en formato iNES (.NES)."
  chk $T/p14.ppm titulo 182 234 activa "Hispa-Nes" texto 208 275 0 8 fina "Este cartucho usa un circuito (el número 99) que"
  chk $T/p15.ppm texto 150 556 0 6 fina "CPUTEST.NES"
  hn pantalla $T/p15.ppm 144 75 2 $(esperada cputest) "tras los dos avisos sigue el cartucho que había"
  muestra p0 grupo; muestra p1 titulo; muestra p3 juego; muestra p4 pausa; muestra p5 menu; muestra p6 x1
  muestra p8 acerca; muestra p9 fin; muestra p10 mejor; muestra p11 abrir; muestra p12 cputest; muestra p14 cartucho
  salida_bien
  ;;

vivo)
  echo "== Hispa-Nes a su velocidad, con una Sound Blaster 16: teclas mantenidas, sonido y salir con él abierto =="
  disco ''
  { entrar 'A:\HISPADOS\HSPSND'
    tecla ret; sleep 4
    aparca
    shot v1
    tecla ret; sleep 2                        # Start, por el mapa de teclas
    shot v2
    tecla x; sleep 0.5; tecla x; sleep 0.5; tecla x; sleep 1.5
    tecla p; sleep 0.8                        # pausa: la imagen se queda quieta
    shot v3
    sleep 1
    shot v4
    tecla p; sleep 1
    salir                                     # con el juego en marcha
  } | run 200 -vga std -audiodev wav,id=snd0,path=$T/nes.wav -device sb16,audiodev=snd0
  check "Sound Blaster 16"
  chk $T/v1.ppm marco 142 30 516 545 activa titulo 142 30 activa "Hispa-Nes" px 690 12 8 "altavoz encendido: hay HSPSND"
  hn pantalla $T/v1.ppm 144 75 2 $(esperada h1_titulo) "el título, quieto, en pausa (sin el letrero)" 0 0 256 104
  hn pantalla $T/v2.ppm 144 75 2 $(esperada h1_juego) "Start con la tecla mantenida: el marcador de la partida" 0 8 256 8
  chk $T/v3.ppm texto 150 556 0 6 fina "Pausa - HISPA1.NES" igual 144 75 512 480 $T/v4.ppm 144 75 0
  python3 tests/tools/audio.py check $T/nes.wav > $T/audio.txt 2>&1
  if grep -q "tramo" $T/audio.txt; then
    ok "los disparos suenan por HSPSND ($(grep -c tramo $T/audio.txt) tramos con sonido en la grabación)"
  else
    ko "no ha sonado nada: $(tr '\n' ' ' < $T/audio.txt)"
  fi
  muestra v2 vivo; muestra v3 vivo_pausa
  salida_bien
  ;;

mando)
  echo "== Hispa-Nes con un mando de HSPJOY: la partida empieza sin tocar el teclado =="
  disco ''
  mcopy -o -i $T/f.img $B/NESJOY.COM ::NESJOY.COM
  { entrar 'A:\HISPADOS\HSPJOY /R' 'NESJOY'
    tecla ret; sleep 6                        # se abre; el mando tiene Start pulsado
    aparca
    tecla p; sleep 0.8
    shot m1
    tecla p; sleep 0.5
    salir
  } | run 150 -vga std
  check "NESJOY: mando de prueba entregado a HSPJOY."
  hn pantalla $T/m1.ppm 144 75 2 $(esperada h1_juego) "Start del mando: el marcador de la partida, sin teclado" 0 8 256 8
  chk $T/m1.ppm texto 150 556 0 6 fina "Pausa - HISPA1.NES"
  salida_bien
  ;;

pausa)
  echo "== Hispa-Nes a su velocidad: empieza en pausa, se para con los menús (callado) y sigue como estaba =="
  disco ''
  { entrar 'A:\HISPADOS\HSPSND'
    tecla ret; sleep 4                        # se abre: Cometas, en pausa
    aparca
    shot a1; sleep 1; shot a2                 # quieta, con el letrero
    clic 400 300; sleep 1                     # un clic en la imagen: a jugar
    aparca
    shot a3
    tecla ret; sleep 2                        # Intro (Start): empieza la partida
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
  } | run 200 -vga std -audiodev wav,id=snd0,path=$T/nes.wav -device sb16,audiodev=snd0
  chk $T/a1.ppm texto $LX1 $LY1 0 8 negrita "En pausa" texto $LX2 $LY2 0 8 fina "Pulse una tecla o haga clic" \
    texto 150 556 0 6 fina "Pausa - HISPA1.NES" igual 144 75 512 480 $T/a2.ppm 144 75 0
  hn pantalla $T/a1.ppm 144 75 2 $(esperada h1_titulo) "al abrirse, en pausa: el título (sin el letrero)" 0 0 256 100
  chk $T/a3.ppm cambia 280 293 240 44 $T/a2.ppm 1000 texto 150 556 0 6 fina "HISPA1.NES  "
  chk $T/a4.ppm cambia 144 75 512 480 $T/a5.ppm 200
  chk $T/b1.ppm texto 247 99 0 8 fina "amaño x" igual 380 80 270 470 $T/b2.ppm 380 80 0
  chk $T/b3.ppm cambia 144 75 512 480 $T/b4.ppm 200
  chk $T/c1.ppm texto $LX1 $LY1 0 8 negrita "En pausa" texto 150 556 0 6 fina "Pausa - HISPA1.NES"
  chk $T/c2.ppm texto 215 79 0 8 fina "ROM..."
  chk $T/c3.ppm texto $LX1 $LY1 0 8 negrita "En pausa" texto 150 556 0 6 fina "Pausa - HISPA1.NES" \
    igual 144 75 512 480 $T/c4.ppm 144 75 0
  chk $T/c5.ppm texto 150 556 0 6 fina "HISPA1.NES  " cambia 280 293 240 44 $T/c4.ppm 1000   # (sin letrero; la partida puede haber acabado)
  silencio_wav $T/nes.wav $T/s1 $T/s2 "con el menú abierto"
  sonido_wav $T/nes.wav $T/s0 $T/s1 "jugando, antes de abrir el menú"
  muestra a1 en_pausa; muestra b1 menu_parada
  salida_bien
  ;;

raton)
  echo "== Hispa-Nes con el juego en marcha: cada menú de la barra con el ratón y una orden de cada uno, a la primera =="
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
  } | run 200 -vga std -audiodev wav,id=snd0,path=$T/nes.wav -device sb16,audiodev=snd0
  chk $T/r1.ppm texto 247 99 0 8 fina "amaño x"
  chk $T/r2.ppm marco 142 30 260 305 activa
  chk $T/r3.ppm marco 142 30 516 545 activa
  chk $T/r4.ppm texto 367 79 0 8 fina "quitar el "
  en_barra $T/r5.ppm "sin sonido" "Sonido: el sonido se quita (la barra dice «sin sonido»)"
  chk $T/r6.ppm texto 375 99 0 8 fina "cerca de Hispa-Nes..."
  chk $T/r7.ppm titulo $TTX $TTY activa "Teclas y mandos"
  chk $T/r8.ppm texto 215 79 0 8 fina "ROM..." texto 175 147 0 8 fina "ausa o continuar (P)"
  chk $T/r9.ppm texto 150 556 0 6 fina "Pausa - HISPA1.NES" texto $LX1 $LY1 0 8 negrita "En pausa"
  chk $T/r10.ppm titulo 222 150 activa "Abrir ROM (.NES)" \
    texto $CX $CY 0 8 fina "Carpeta: A:\HISPADOS\JUEGOS\NES"
  muestra r1 menu_tamano; muestra r4 menu_sonido; muestra r6 menu_ayuda; muestra r8 menu_archivo
  muestra r10 abrir_carpeta
  salida_bien
  ;;

carpeta)
  echo "== Hispa-Nes: Abrir ROM recuerda la última carpeta (HISPANES.INI) =="
  disco '/P' 'A:\ROMS'
  { entrar
    tecla ret; sleep 4
    aparca
    tecla alt-a ret; sleep 1.2                # Abrir ROM: en \ROMS
    shot k1
    escribe otra.nes; tecla ret; sleep 3
    shot k2
    tecla alt-a ret; sleep 1.2                # y otra vez en \ROMS
    shot k3
    tecla esc; sleep 0.6
    salir
  } | run 200 -vga std
  chk $T/k1.ppm titulo 222 150 activa "Abrir ROM (.NES)" texto $CX $CY 0 8 fina "Carpeta: A:\ROMS"
  chk $T/k2.ppm texto 150 556 0 6 fina "OTRA.NES"
  hn pantalla $T/k2.ppm 144 75 2 $(esperada cputest) "OTRA.NES, de \\ROMS: pantalla verde"
  chk $T/k3.ppm texto $CX $CY 0 8 fina "Carpeta: A:\ROMS"
  salida_bien
  ;;

prueba)
  echo "== /T: CPUTEST.NES dentro de HISPANES.HVP y la velocidad en QEMU =="
  disco 'A:\CPUTEST.NES /T=60'
  { entrar
    tecla ret; sleep 5
    aparca
    shot t1
    salir
  } | run 120 -vga std
  mtype -i $T/f.img ::HISPADOS/JUEGOS/HISPANES.LOG 2> /dev/null | tr -d '\r' > $T/nes.log
  hn pantalla $T/t1.ppm 144 75 2 $(esperada cputest) "CPUTEST.NES: pantalla verde"
  if [ "$(del_log Estado)" = 0 ] && [ "$(del_log Texto)" = "CPUTEST BIEN" ]; then
    ok "HISPANES.LOG: las 24 pruebas propias, bien, dentro del módulo"
  else
    ko "HISPANES.LOG: Estado=$(del_log Estado) Texto=$(del_log Texto)"
  fi
  s=$($HN suma $(esperada cputest))
  if [ "$(del_log Imagen)" = "$s" ]; then ok "la imagen es la misma que fuera de QEMU (suma $s)"; else ko "Imagen=$(del_log Imagen) y se esperaba $s"; fi
  salida_bien
  disco '/T=600 /V'
  { entrar
    tecla ret; sleep 12
    aparca
    shot t2
    salir
  } | run 120 -vga std
  mtype -i $T/f.img ::HISPADOS/JUEGOS/HISPANES.LOG 2> /dev/null | tr -d '\r' > $T/nes.log
  cps=$(del_log CuadrosPorSegundo)
  echo "  (velocidad en este QEMU: ${cps:-?} cuadros por segundo con la ventana $(del_log Modo | sed 's/ventana //'), en $(del_log Milisegundos) ms los 600)"
  if [ "${cps:-0}" -ge 60 ]; then ok "Cometas, pintando cada cuadro en la ventana: 60 cuadros por segundo o más"; else ko "solo $cps cuadros por segundo"; fi
  if [ "$(del_log Modo)" = "ventana x2" ] && [ "$(del_log Mapa)" = 0 ]; then ok "HISPANES.LOG: ventana x2, cartucho 0"; else ko "HISPANES.LOG: $(tr '\n' ' ' < $T/nes.log)"; fi
  salida_bien
  ;;
esac
done
rm -rf $T
exit $fail
