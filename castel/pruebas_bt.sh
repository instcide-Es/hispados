# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# pruebas_bt.sh - (BLUETOOTH) la parte 55 de pruebas.sh (SOLO=bt), que lee
# este archivo («.»): la función parte_bt, con los ayudantes de pruebas.sh
# (ok, ko, mira) y sus carpetas ($AQUI, $P).
#
# El Bluetooth (castel/nucleo32/usbbt.cas, bt/, usbhid.cas). QEMU ya no
# tiene «-device usb-bt-dongle» (lo quitó en la 5.0, con todo su
# Bluetooth), así que:
#   1. en Linux, build/bt_prueba: la pila de BT.BIN contra el modelo de
#      adaptador y aparatos (tests/bt/modelo.c);
#   2. en QEMU, un adaptador USB de mentira por usb-redir
#      (build/usbredir_bt, con el mismo modelo detrás): el núcleo lo
#      enumera (E0/01/01) y lo da a BT.BIN, que lo pone en marcha (reset,
#      dirección...), busca, empareja el teclado (el código, tecleado por
#      «la persona» del modelo) y el ratón (Just Works), y lo que teclea el
#      teclado llega a la consola (ORDENES lo ejecuta); BT.INI, BT.LOG sin
#      claves; al arrancar otra vez, el teclado se conecta solo; y el Intel
#      0AAA con su firmware de mentira por el extremo a granel.
parte_bt() {
echo "== Hispa-Pentium: el Bluetooth (HCI por USB, SSP, L2CAP, SDP, HID; BT.BIN) (BLUETOOTH) =="
local X=$P/bt R=$AQUI/.. S PU QM
rm -rf $X; mkdir -p $X/fw
# ---- 1. la pila en el anfitrión ----
if make -s -C $R build/bt_prueba build/usbredir_bt > $X/make.txt 2>&1; then
  if $R/build/bt_prueba $X/fw > $X/anf.txt 2>&1 && grep -q ", 0 fallos" $X/anf.txt; then ok "en el anfitrión, la pila contra el modelo: $(tail -1 $X/anf.txt)"
  else ko "build/bt_prueba falla:"; grep "MAL\|FALLO" $X/anf.txt | head -8 | sed 's/^/        /'; tail -1 $X/anf.txt | sed 's/^/        /'; fi
else ko "no se hacen build/bt_prueba y build/usbredir_bt: $(tail -2 $X/make.txt)"; return; fi
# ---- 2. en QEMU (el núcleo, una vez; con BT_NUCLEO=archivo, ese) ----
local K=${BT_NUCLEO:-}
if [ -z "$K" ]; then
  if ! { python3 -I $AQUI/ccastel.py $AQUI/nucleo32/semilla.cas -o $X/s.asm > /dev/null && nasm -f bin $X/s.asm -o $X/SEMILLA.BIN; }; then ko "no se hace el núcleo"; return; fi
  K=$X/SEMILLA.BIN
fi
if ! $AQUI/nucleo32/disco.sh $X/d.img 16 bt > $X/disco.txt 2>&1; then ko "no se hace el disco con BT.BIN: $(tail -2 $X/disco.txt)"; return; fi
PU=$((47000 + RANDOM % 2000))
QM="-device qemu-xhci,id=x -chardev socket,id=bt,host=127.0.0.1,port=$PU -device usb-redir,chardev=bt,bus=x.0"
# (a) emparejar el teclado y el ratón; el teclado teclea «hola» e Intro
$R/build/usbredir_bt $PU --estado $X/estado --teclea 'hola\n' 2> $X/redir1.txt &
sleep 0.5
QEMU_MAS="$QM" MEMORIA=64 ESCRIBE=1 DISCO=$X/d.img ARRANQUE=ordenes MODULOS= $AQUI/nucleo32/probar.sh $K "bt" ":pausa 3" "bt /buscar" \
  "bt /emparejar DE:AD:BA:BE:10:01" ":pausa 12" "bt /emparejar DE:AD:BA:BE:11:02" ":pausa 8" "bt" ":pausa 2" "type bt.log" ":pausa 2" "type bt.ini" ":pausa 1" > $X/q1.sal 2>&1
wait
S=$X/q1.sal
mira "QEMU, usb-redir: el núcleo ve el adaptador Bluetooth (E0/01/01) y BT.BIN lo pone en marcha (reset, versión, dirección)" \
  "^bt: el aparato [0-9]* (0A12:0001) es un adaptador Bluetooth: lo lleva BT.BIN" "^bt: Adaptador: CSR 0A12:0001" \
  "^bt: Versión: Bluetooth 4.2" "^bt: Dirección: xx:xx:xx:xx:3F:21$" "^bt: Encendido: los emparejados se pueden conectar"
mira "BT /BUSCAR: el teclado (con su nombre por EIR), el ratón (su nombre, preguntado) y el ratón BLE" \
  "Teclado de prueba (teclado), DE:AD:BA:BE:10:01" "Ratón de prueba (ratón), DE:AD:BA:BE:11:02" "Ratón BLE (ratón), .*BLE (HID): emparejarlo llega en la etapa 2 b"
mira "emparejar el teclado: el código, a teclear en él (sin escribirlo en el registro), y luego SDP y HID con el de arranque" \
  "^Escriba [0-9]\{6\} en el teclado «Teclado de prueba» y pulse Intro" "^Emparejado con Teclado de prueba" \
  "^bt: HID listo: xx:xx:xx:xx:10:01 (Teclado de prueba), con el protocolo de arranque" "^bt: un teclado Bluetooth (ranura 0), con el protocolo de arranque"
mira "lo que teclea el teclado Bluetooth llega a la consola: «hola» e Intro, y ORDENES lanza HOLA.BIN" "^hola desde el anillo 3"
mira "el ratón: Just Works, su descriptor por SDP y el protocolo de informes (hiddesc.cas, como los USB)" \
  "^Emparejado con Ratón de prueba" "^bt: un ratón Bluetooth (ranura 1), con el protocolo de informes (su descriptor)" "^usb: informe de un ratón (5 octetos): 02 00 0A 05 00"
mira "BT: los dos, emparejados y conectados" "Teclado de prueba (teclado), DE:AD:BA:BE:10:01: emparejado, conectado" "Ratón de prueba (ratón), DE:AD:BA:BE:11:02: emparejado, conectado"
if grep -q "Escriba [0-9]\{6\}" $S && ! sed -n '/type bt.log/,$p' $S | grep -q "$(grep -o 'Escriba [0-9]\{6\}' $S | head -1 | cut -c9-)"; then ok "BT.LOG: ni el código de emparejar ni las direcciones enteras"
else ko "BT.LOG lleva el código de emparejar"; fi
MTOOLS_SKIP_CHECK=1 mcopy -n -i $X/d.img ::BT.INI $X/BT.INI > /dev/null 2>&1
if grep -q "^Aparato=0110BEBAADDE," $X/BT.INI && grep -q "^Aparato=0211BEBAADDE," $X/BT.INI && grep -q "^; Teclado de prueba (teclado)" $X/BT.INI && grep -q "^Encendido=SI" $X/BT.INI; then
  ok "BT.INI: el teclado y el ratón, con su nombre a la vista y la clave cifrada"
else ko "BT.INI no tiene los emparejados"; fi
mira "BT.INI es un secreto de BT.BIN: ORDENES no lo puede leer" "^permisos: acceso denegado a .bt.ini para leer (ORDENES.BIN)"
if grep -q "modelo: FALLO" $X/redir1.txt; then ko "el modelo ha visto cosas mal hechas: $(grep -m3 'modelo: FALLO' $X/redir1.txt | tr '\n' ' ')"
else ok "el adaptador de mentira no ha visto nada mal hecho ($(tail -1 $X/redir1.txt | sed 's/^usbredir_bt: //'))"; fi
# (b) arrancar otra vez: el teclado se despierta y se conecta solo, y teclea
$R/build/usbredir_bt $PU --estado $X/estado --teclea 'hola\n' 2> $X/redir2.txt &
sleep 0.5
QEMU_MAS="$QM" MEMORIA=64 ESCRIBE=1 DISCO=$X/d.img ARRANQUE=ordenes MODULOS= $AQUI/nucleo32/probar.sh $K "bt" ":pausa 14" "bt" ":pausa 1" > $X/q2.sal 2>&1
wait
S=$X/q2.sal
mira "al arrancar otra vez: BT.INI leído, los emparejados se reconectan solos (con la clave guardada, sin SDP) y el teclado teclea" \
  "^bt: BT.INI: 2 emparejados; encendido" "^bt: Reconectando con xx:xx:xx:xx:10:01" "^bt: Cifrado con xx:xx:xx:xx:10:01" \
  "^bt: HID listo: xx:xx:xx:xx:10:01 (Teclado de prueba), con el protocolo de arranque" "^hola desde el anillo 3"
mira "y el ratón" "^bt: Reconectando con xx:xx:xx:xx:11:02" "Ratón de prueba (ratón), DE:AD:BA:BE:11:02: emparejado, conectado"
if grep -q "modelo: FALLO" $X/redir2.txt; then ko "el modelo ha visto cosas mal hechas: $(grep -m3 'modelo: FALLO' $X/redir2.txt | tr '\n' ' ')"; else ok "al reconectar, el modelo no ha visto nada mal hecho"; fi
# (d) el escritorio: con un adaptador, el icono «Bluetooth» del Panel de
# control; su ventana: buscar, elegir el teclado, «Emparejar» (el código en
# grande) y emparejado
$AQUI/nucleo32/disco.sh $X/g.img 16 bt > /dev/null 2>&1
rm -f $X/estado_g
$R/build/usbredir_bt $PU --estado $X/estado_g --teclea 'hola' 2> $X/redir4.txt &
sleep 0.5
QEMU_MAS="$QM" CAPTURAS=$X MEMORIA=64 ESCRIBE=1 DISCO=$X/g.img ARRANQUE=escritorio MODULOS= $AQUI/nucleo32/probar.sh $K ":pausa 9" ":clicd 400 300" ":pausa 1" ":clic 410 309" ":pausa 2" \
  ":captura bt1" ":clic 590 370" ":pausa 4" ":clic 328 139" ":pausa 14" ":captura bt2" ":clic 300 185" ":pausa 1" ":clic 174 497" ":pausa 2" ":captura bt3" ":pausa 10" ":captura bt4" > $X/q4.sal 2>&1
wait
S=$X/q4.sal
mira "el escritorio: con un adaptador, Configuración tiene «Bluetooth», que abre su ventana (BT /VENTANA)" "^escritorio: Panel de control > Bluetooth$" "^escritorio: lanzo .BT.BIN"
mira "la ventana: «Buscar aparatos», el teclado elegido, «Emparejar»: el código (tecleado por «la persona»), emparejado y conectado" \
  "^bt: Buscando aparatos" "^bt: Emparejar con xx:xx:xx:xx:10:01 (Teclado de prueba)" "^bt: SSP: el código, a teclear en xx:xx:xx:xx:10:01" "^bt: HID listo: xx:xx:xx:xx:10:01"
# (c) el Intel 0AAA en su cargador, con un firmware de mentira (lo hace bt_prueba en $X/fw: se rehace)
if :; then
  python3 -I - $X/fw <<'EOF'
import sys, os
d = sys.argv[1]
b = bytearray((i * 7) & 255 for i in range(644))
b += bytes([0x0E, 0xFC, 7, 0x00, 0x5A, 0xA5, 0x00, 1, 2, 3, 0x02, 0xFC, 2, 0, 0, 0x02, 0xFC, 2, 0, 0])
for k in range(120):
    pl = 20 + k % 200
    b += bytes([0x8E, 0xFC, pl]) + bytes((k + i) & 255 for i in range(pl))
    m = (4 - (3 + pl + 3) % 4) % 4
    b += bytes([0x02, 0xFC, m]) + bytes(m)
os.makedirs(d + '/FIRMWARE', exist_ok=True)
open(d + '/FIRMWARE/ibt-17-16-1.sfi', 'wb').write(b)
open(d + '/ibt-17-16-1.sfi', 'wb').write(b)
EOF
fi
cp $X/d.img $X/i.img
MTOOLS_SKIP_CHECK=1 mmd -i $X/i.img ::FIRMWARE 2> /dev/null; MTOOLS_SKIP_CHECK=1 mcopy -o -i $X/i.img $X/fw/FIRMWARE/ibt-17-16-1.sfi ::FIRMWARE/
$R/build/usbredir_bt $PU --clase intel-cargador --fw $X/fw 2> $X/redir3.txt &
sleep 0.5
QEMU_MAS="$QM" MEMORIA=64 DISCO=$X/i.img ARRANQUE=ordenes MODULOS= $AQUI/nucleo32/probar.sh $K "bt" ":hasta 240 1 del fabricante 2 (Intel)" ":pausa 2" "bt" ":pausa 1" > $X/q3.sal 2>&1
wait
S=$X/q3.sal
mira "Intel 8087:0AAA en su cargador: su firmware (ibt-17-16-1.sfi, de \\FIRMWARE) por el extremo a granel, arranca y luego el reset de siempre" \
  "bt: Intel 0AAA: plataforma 37, familia 11 JfP" "bt: Firmware ibt-17-16-1.sfi: [0-9]* octetos" "bt: Intel: el firmware se ha cargado" \
  "bt: Intel: ha arrancado su firmware" "bt: Versión: Bluetooth 4.2" "Bluetooth: Encendido (Intel 8087:0AAA (Wireless 9460/9560)"
mira "mientras carga el firmware, BT dice que está ocupado (no que no hay servicio)" "^Bluetooth: ocupado, poniendo en marcha el adaptador"
if grep -q "modelo: FALLO" $X/redir3.txt; then ko "Intel: el modelo ha visto cosas mal hechas: $(grep -m3 'modelo: FALLO' $X/redir3.txt | tr '\n' ' ')"; else ok "Intel: el modelo no ha visto nada mal hecho"; fi
}
