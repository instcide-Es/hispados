#!/bin/bash
# Prueba de Hispa Ventanas (VENTANAS.COM): gráficos, ratón y ventanas, con las
# ventanas de muestra de la etapa 2 (VENTANAS /DEMO). Los programas y los
# controles de la etapa 3 se prueban en tests/run_ventanas3_tests.sh.
# Arranca en QEMU, maneja el escritorio con el ratón y el teclado del monitor
# de QEMU (mouse_move, mouse_button, sendkey), saca capturas (screendump) y
# las comprueba punto a punto con tests/tools/ventanas_check.py:
#  - VESA 800x600 de 256 colores: fondo, barra, las tres ventanas con sus
#    títulos y textos (fuente propia, con acentos), cursor, arrastre con el
#    ratón, activar con un clic, Tab, flechas, botón «Salir», Esc y Alt+F4,
#    y vuelta a Hispa-DOS en modo texto sin perder memoria;
#  - los demás formatos: VGA de 16 colores, 15/16/24/32 bits, 1024x768, 640x480;
#  - con una tarjeta Cirrus, cargando solo HSPDPMI y HSPRATON;
#  - sin XMS, sin servidor DPMI y sin ratón; y un fallo de protección estando
#    en gráficos (se vuelve a texto y se ve el mensaje del servidor).
# Deja capturas de muestra en build/ventanas_*.png.
# Hispa Ventanas test: windows, mouse dragging and video modes under QEMU.
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
chk() { $CHK "$@" || fail=1; }
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; sleep "${2:-0.3}"; }
tecla() { mon "sendkey $1" 0.8; }
shot() { mon "screendump $T/$1.ppm" 0.7; }
send() { printf '%s\r' "$1"; sleep "${2:-1.5}"; }
muestra() { [ -f $T/$1.ppm ] && convert $T/$1.ppm $B/ventanas_$2.png; }
# run TIEMPO [opciones de qemu...] -- las órdenes, por stdin
run() {
  local tmo=$1; shift
  rm -f $T/mon
  timeout $tmo qemu-system-i386 -display none -serial stdio -monitor unix:$T/mon,server,nowait \
    -parallel none -m 16 -drive file=$T/f.img,format=raw,if=floppy,index=0 -boot a -no-reboot "$@" > $T/raw 2>&1
  tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 > $T/log.txt
}
config() {
  cp "$IMG" $T/f.img
  printf "$1"'FILES=30\r\n' > $T/CONFIG.SYS
  mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
}
XMS='DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\n'
# DOSHIGH=1: las mismas pruebas con el núcleo en la HMA (DOS=HIGH)
[ -n "${DOSHIGH:-}" ] && XMS="$XMS"'DOS=HIGH\r\n'

# lo que tiene que haber nada más entrar, con una pantalla de $2 x $3
basico() {
  local c=$T/$1.ppm w=$2 h=$3
  chk $c tam $w $h \
    px 5 300 2 "fondo (mosaico, azul petróleo)" px 0 304 3 "fondo (rombo del mosaico)" \
    px 300 10 1 "barra superior" px 300 27 10 "filete ocre de la barra" \
    px $((w-70)) 7 4 "botón Salir" \
    texto 12 5 8 1 negrita "Hispa Ventanas" texto $((w-64)) 5 8 4 fina "Salir" \
    ventana 30 50 440 160 activa titulo 30 50 activa "Bienvenida" \
    px $((w-88)) 115 6 "barra de título de «Paleta y dibujo» (no activa)" \
    px $((w-88)) 300 8 "interior de «Paleta y dibujo»" px $((w-81)) 220 1 "marco de «Paleta y dibujo»" \
    ventana 60 $((h-160)) 400 150 no titulo 60 $((h-160)) no "Sistema" \
    texto 46 87 4 8 negrita "¡Bienvenido a Hispa Ventanas!" \
    texto 46 131 0 8 fina "hecho desde cero y en español." \
    texto 76 $((h-39)) 13 8 fina "áéíóú ü ñ Ñ ¿? ¡! ç Ç « » º ª" \
    px 472 100 0 "sombra de la ventana"
}

echo "== Hispa Ventanas: VESA 800x600, ratón y teclado =="
config "$XMS"
{ sleep 6
  send 'A:\HISPADOS\HSPDPMI' 2
  send 'A:\HISPADOS\HSPRATON' 2
  send 'BIN\XMSTEST' 2.5
  send 'MEM' 2
  send 'VENTANAS /DEMO' 4
  shot s1
  # el ratón, a la esquina y de ahí a la barra de título de «Bienvenida»
  mon "mouse_move -500 -400"; mon "mouse_move -500 -400"
  mon "mouse_move 125 31"; mon "mouse_move 125 31" 1
  shot s1b
  mon "mouse_button 1" 0.6
  mon "mouse_move 50 40"; mon "mouse_move 50 40" 1.2
  shot s2                                   # arrastrando: la ventana va detrás
  mon "mouse_button 0" 0.6
  mon "mouse_move 150 54"; mon "mouse_move 150 54" 0.6
  mon "mouse_button 1" 0.5; mon "mouse_button 0" 1
  shot s3                                   # clic en «Paleta y dibujo»: pasa delante
  tecla tab; shot s4                        # Tab: «Sistema»
  tecla right; tecla down; tecla tab
  shot s5                                   # flechas y otra vez Tab: «Bienvenida»
  tecla esc; sleep 1.5
  shot s6
  send 'ECHO TRAS-ESC'
  send 'VENTANAS /DEMO' 4
  mon "mouse_move 190 -141"; mon "mouse_move 190 -141" 0.8
  mon "mouse_button 1" 1
  shot s7                                   # botón «Salir» apretado
  mon "mouse_button 0" 2
  send 'ECHO TRAS-SALIR'
  send 'VENTANAS /DEMO' 4
  tecla alt-f4; sleep 1.5
  send 'ECHO TRAS-ALT-F4'
  send 'VENTANAS /DEMO /VGA' 4; shot m_vga; tecla esc; sleep 1.5
  send 'VENTANAS /DEMO /15' 4; shot m_15; tecla esc; sleep 1.5
  send 'VENTANAS /DEMO /16' 4; shot m_16; tecla esc; sleep 1.5
  send 'VENTANAS /DEMO /24' 4; shot m_24; tecla esc; sleep 1.5
  send 'VENTANAS /DEMO /32' 4; shot m_32; tecla esc; sleep 1.5
  send 'VENTANAS /DEMO /1024 /COLOR' 4; shot m_1024; tecla esc; sleep 1.5
  send 'VENTANAS /DEMO /640' 4; shot m_640; tecla esc; sleep 1.5
  send 'BIN\XMSTEST' 2.5
  send 'MEM' 2
  send 'ECHO SIGUE-VIVO'
  mon quit
} | run 150 -vga std

check "Instalado: clientes de 16 y de 32 bits, memoria de XMS."
checkn "cargando el servidor DPMI"
basico s1 800 600
chk $T/s1.ppm cursor 400 300 \
  px 445 165 8 "«Bienvenida» tapa a «Paleta y dibujo»" \
  texto 76 475 0 8 fina "Pantalla: 800 x 600, 256 colores" \
  texto 76 515 0 8 fina "Ratón: driver INT 33h (HSPRATON)"
chk $T/s1b.ppm cursor 250 62 px 400 300 2 "el cursor ya no está en el centro"
# arrastre: de (30,50) a (130,130); lo de antes, repintado
chk $T/s2.ppm ventana 130 130 440 160 activa titulo 130 130 activa "Bienvenida" \
  cursor 350 142 px 35 55 2 "fondo donde estaba la ventana" px 301 100 2 "fondo donde estaba la ventana (2)" \
  igual 130 130 440 160 $T/s1.ppm 30 50 216 \
  px 712 115 6 "«Paleta y dibujo» sigue en su sitio" ventana 60 440 400 150 no titulo 60 440 no "Sistema"
# clic: «Paleta y dibujo» delante y activa
chk $T/s3.ppm ventana 420 110 300 200 activa titulo 420 110 activa "Paleta y dibujo" \
  px 445 165 0 "«Paleta y dibujo» tapa a «Bienvenida»" titulo 130 130 no "Bienvenida" \
  cursor 650 250
chk $T/s4.ppm ventana 60 440 400 150 activa titulo 60 440 activa "Sistema" \
  titulo 420 110 no "Paleta y dibujo" px 445 165 0 "«Paleta y dibujo» sigue sobre «Bienvenida»"
chk $T/s5.ppm ventana 68 448 400 150 no titulo 68 448 no "Sistema" \
  px 63 444 2 "fondo donde estaba «Sistema»" \
  ventana 130 130 440 160 activa titulo 130 130 activa "Bienvenida" \
  px 445 165 8 "«Bienvenida» vuelve a tapar a «Paleta y dibujo»"
# vuelta a modo texto
chk $T/s6.ppm tam 720 400
n=$(grep -c "Hispa Ventanas ha terminado." $T/log.txt)
if [ "$n" = 10 ]; then ok "diez sesiones terminadas con su mensaje"; else ko "sesiones terminadas: $n (esperadas 10)"; fi
if grep -A1 -F "Hispa Ventanas ha terminado." $T/log.txt | grep -qF 'A:\>ECHO TRAS-ESC'; then ok "tras Esc, el indicador A:\\> en modo texto"; else ko "indicador A:\\> tras Esc"; fi
chk $T/s7.ppm cursor 780 18 px 730 7 10 "botón Salir apretado" texto 736 5 0 10 fina "Salir"
check "TRAS-SALIR"
check "TRAS-ALT-F4"
# los demás formatos de pantalla
basico m_vga 640 480
chk $T/m_vga.ppm texto 76 355 0 8 fina "Pantalla: 640 x 480, 16 colores" texto 76 375 0 8 fina "Vídeo: VGA (modo de reserva, sin VESA)" cursor 320 240
basico m_15 800 600
chk $T/m_15.ppm texto 76 475 0 8 fina "Pantalla: 800 x 600, 32.768 colores" cursor 400 300
basico m_16 800 600
chk $T/m_16.ppm texto 76 475 0 8 fina "Pantalla: 800 x 600, 65.536 colores"
basico m_24 800 600
chk $T/m_24.ppm texto 76 475 0 8 fina "Pantalla: 800 x 600, 16,7 millones de colores"
basico m_32 800 600
chk $T/m_32.ppm texto 76 475 0 8 fina "Pantalla: 800 x 600, 16,7 millones de colores" cursor 400 300
basico m_1024 1024 768
chk $T/m_1024.ppm texto 76 643 0 8 fina "Pantalla: 1024 x 768, 16,7 millones de colores" cursor 512 384
basico m_640 640 480
chk $T/m_640.ppm texto 76 355 0 8 fina "Pantalla: 640 x 480, 256 colores"
# no se pierde memoria ni queda nada cambiado
a=$(grep "largest free KB" $T/log.txt | head -1); b=$(grep "largest free KB" $T/log.txt | tail -1)
if [ -n "$a" ] && [ "$a" = "$b" ] && [ "$(grep -c 'largest free KB' $T/log.txt)" = 2 ]; then ok "memoria extendida igual al final ($(echo $b | sed 's/.*largest/mayor bloque/'))"; else ko "memoria extendida: «$a» y «$b»"; fi
a=$(grep "bytes libres" $T/log.txt | head -1); b=$(grep "bytes libres" $T/log.txt | tail -1)
if [ -n "$a" ] && [ "$a" = "$b" ] && [ "$(grep -c 'bytes libres' $T/log.txt)" = 2 ]; then ok "memoria convencional igual al final ($(echo $b))"; else ko "memoria convencional: «$a» y «$b»"; fi
check "XMS test OK"
check "SIGUE-VIVO"
checkn "HSPDPMI: excepción"
muestra s1 inicio; muestra s2 arrastre; muestra s3 activar; muestra m_vga vga16; muestra m_1024 1024
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/Ejecutando/,$p' $T/log.txt; fi
[ -n "${GUARDAR:-}" ] && cp $T/*.ppm "$GUARDAR"/

echo "== Hispa Ventanas: Cirrus, carga de HSPDPMI y HSPRATON, ayuda y fallo =="
config "$XMS"
{ sleep 6
  send 'VENTANAS /?' 2
  send 'VENTANAS /DEMO' 5
  shot c1
  tecla esc; sleep 1.5
  send 'VENTANAS /DEMO /VGA' 4
  shot c2
  tecla esc; sleep 1.5
  send 'VENTANAS /FALLO' 4
  shot c3
  send 'ECHO SIGUE-VIVO'
  mon quit
} | run 60 -vga cirrus
check "VENTANAS [/1024 | /800 | /640] [/8 | /15 | /16 | /24 | /32 | /COLOR] [/VGA]"
check "Hispa Ventanas: cargando el servidor DPMI (HSPDPMI)..."
check "Instalado: clientes de 16 y de 32 bits, memoria de XMS."
check "Ratón PS/2 preparado."
basico c1 800 600
chk $T/c1.ppm cursor 400 300 texto 76 515 0 8 fina "Ratón: driver INT 33h (HSPRATON)"
basico c2 640 480
chk $T/c3.ppm tam 720 400
check "HSPDPMI: excepción 0Dh (fallo de protección general) en"
check "HSPDPMI: el programa se ha terminado."
check "SIGUE-VIVO"
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/Ejecutando/,$p' $T/log.txt; fi

echo "== Hispa Ventanas: sin XMS, sin servidor DPMI y sin ratón =="
config ""
{ sleep 6; send 'VENTANAS /DEMO' 2; send 'ECHO SIGUE-VIVO'; mon quit; } | run 30 -vga std
check "Hispa Ventanas necesita memoria extendida: añada a CONFIG.SYS la línea"
check "DEVICE=A:\\HISPADOS\\HSPXMS.SYS"
check "SIGUE-VIVO"
config "$XMS"
mren -i $T/f.img ::HISPADOS/HSPDPMI.COM ::HISPADOS/OTRO.COM
mdel -i $T/f.img ::HISPADOS/HSPRATON.COM
{ sleep 6
  send 'VENTANAS /DEMO' 3
  send 'ECHO SIN-DPMI'
  send 'A:\HISPADOS\OTRO' 2
  send 'VENTANAS /DEMO' 4
  shot n1
  tecla tab; tecla left; tecla up
  shot n2
  tecla esc; sleep 1.5
  send 'ECHO SIGUE-VIVO'
  mon quit
} | run 50 -vga std
check "Hispa Ventanas necesita el servidor DPMI y no ha podido cargarlo."
check "SIN-DPMI"
basico n1 800 600
chk $T/n1.ppm px 400 300 2 "sin ratón no hay cursor" texto 76 515 0 8 fina "Ratón: no hay (cargue HSPRATON)"
chk $T/n2.ppm ventana 52 432 400 150 activa titulo 52 432 activa "Sistema" px 459 586 2 "fondo donde estaba «Sistema»"
check "Hispa Ventanas ha terminado."
check "SIGUE-VIVO"
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/Ejecutando/,$p' $T/log.txt; fi
rm -rf $T
exit $fail
