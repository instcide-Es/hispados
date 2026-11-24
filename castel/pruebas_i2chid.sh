# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# pruebas_i2chid.sh - (I2CHID) la parte 52 de pruebas.sh (SOLO=i2chid), que
# lee este archivo («.»): la función parte_i2chid, con los ayudantes de
# pruebas.sh (ok, ko, mira) y sus carpetas ($AQUI, $P).
#
# Los touchpads HID sobre I2C (nucleo32/i2c.cas, i2chid.cas, hiddesc.cas y
# acpi/pentium/i2chid.c). QEMU no tiene ni controladora I2C DesignWare ni
# HID sobre I2C, así que:
#   1. en Linux, con corre32: HIDPRUEB.BIN (hiddesc.cas) con descriptores de
#      informe de touchpads de verdad (ELAN, Synaptics, Dell; de las pruebas de
#      Linux: nucleo32/i2chid/reales.txt) y con informes, descriptores HID y
#      órdenes hechos a mano (nucleo32/i2chid/sint.txt);
#   2. en Linux, con build/acpi_sim (ACPICA, como en ACPI.BIN): la SSDT de
#      prueba (tests/acpi/i2chid-qemu.aml, de tests/acpi/hazi2chid.py) sobre
#      las tablas del Toshiba, y las tablas de verdad del ASUS GL553VD
#      (tests/acpi/asus-gl553vd/) con las variables de su BIOS puestas a mano
#      (qué touchpad lleva: SDS0 de la GNVS y los bits ELAN/AWTP del CMOS);
#   3. en QEMU: sin controladora I2C, el arranque lo dice y no falla; con la
#      SSDT de prueba, ACPI.BIN halla los dos touchpads y se los da al núcleo
#      (la llamada 159), que dice que no tiene controladora; las zonas PCI
#      sin poner o por encima de los 4 GB («pci=cero» y «pci=arriba», en las
#      del USB: como las de las I2C del Monovate y del E14); la SSDT con
#      «NULL» del E14; (0.8.10) con «i2csim» (dos controladoras de mentira)
#      y la SSDT «monovate», el touchpad de 2Ch se arranca una sola vez y en
#      la del bus perdido no se prueba más; y con «sini2c», ni se buscan.
parte_i2chid() {
echo "== Hispa-Pentium: los touchpads HID sobre I2C (Intel LPSS DesignWare, ACPI PNP0C50, HID sobre I2C de Microsoft) (I2CHID) =="
local X=$P/i2chid l S R=$AQUI/.. TB=$AQUI/../tests/acpi/toshiba-nb250 A=$AQUI/../tests/acpi/asus-gl553vd
rm -rf $X; mkdir -p $X
# ---- 1. hiddesc.cas, con corre32 ----
if python3 -I $AQUI/ccastel.py $AQUI/nucleo32/uhidprue.cas -o $X/h.asm > /dev/null && nasm -f bin $X/h.asm -o $X/HIDPRUEB.BIN; then
  cp $AQUI/nucleo32/i2chid/reales.txt $AQUI/nucleo32/i2chid/sint.txt $X/
  $AQUI/corre32/corre32.sh -C $X $X/HIDPRUEB.BIN reales.txt 2>&1 | iconv -f cp437 -t utf-8 > $X/reales.sal
  $AQUI/corre32/corre32.sh -C $X $X/HIDPRUEB.BIN sint.txt 2>&1 | iconv -f cp437 -t utf-8 > $X/sint.sal
  # en orden: lo que tiene que salir de cada descriptor (y de cada línea de sint.txt)
  en_orden() {      # archivo, lo que se prueba, y las líneas, en su orden
    local f=$1 que=$2 n=0 l; shift 2
    python3 -I - "$f" "$@" <<'EOF' > $X/orden.txt
import sys
l = open(sys.argv[1], encoding='utf-8').read().split('\n')
i = 0
for q in sys.argv[2:]:
    while i < len(l) and l[i] != q:
        i += 1
    if i == len(l):
        print('falta: ' + q)
        sys.exit(1)
    i += 1
EOF
    if [ $? = 0 ]; then ok "$que"; else ko "$que: $(cat $X/orden.txt)"; fi
  }
  en_orden $X/reales.sal "Dell XPS 9360 (Synaptics, 665 octetos): el ratón es el informe 2 (dos botones, X e Y de 8 bits) y el modo de entrada, el informe 4; es un touchpad de precisión" \
    "descriptor: 665 octetos, 13 informes" "ratón: informe 2, 2 botones en el bit 0, X en el 8, Y en el 16, de 8 bits; rueda en el 0, de 0 bits; 3 octetos" \
    "modo: informe 4, en el bit 0, de 8 bits; el informe, 1 octetos" "digitalizador: 5"
  en_orden $X/reales.sal "ELAN 04F3:300B: el ratón con rueda (el informe 1, 8 octetos) y el modo de entrada de dos octetos (el informe 3)" \
    "descriptor: 361 octetos, 11 informes" "ratón: informe 1, 2 botones en el bit 0, X en el 8, Y en el 16, de 8 bits; rueda en el 24, de 8 bits; 8 octetos" \
    "modo: informe 3, en el bit 0, de 8 bits; el informe, 2 octetos"
  en_orden $X/reales.sal "ELAN 04F3:3045 (otro ELAN de 2017)" "descriptor: 356 octetos, 10 informes" \
    "ratón: informe 1, 2 botones en el bit 0, X en el 8, Y en el 16, de 8 bits; rueda en el 24, de 8 bits; 8 octetos" "modo: informe 3, en el bit 0, de 8 bits; el informe, 2 octetos"
  en_orden $X/reales.sal "ELAN 04F3:313A (HID sobre I2C): tres botones, y el modo de entrada de 16 bits; de sus dos ratones, el primero" \
    "descriptor: 464 octetos, 14 informes" "ratón: informe 1, 3 botones en el bit 0, X en el 8, Y en el 16, de 8 bits; rueda en el 0, de 0 bits; 8 octetos" \
    "modo: informe 3, en el bit 0, de 16 bits; el informe, 2 octetos"
  en_orden $X/reales.sal "Synaptics 06CB:CE08 (HID sobre I2C, 751 octetos): el ratón, el informe 2; el modo, el 4" \
    "descriptor: 751 octetos, 15 informes" "ratón: informe 2, 2 botones en el bit 0, X en el 8, Y en el 16, de 8 bits; rueda en el 0, de 0 bits; 3 octetos" \
    "modo: informe 4, en el bit 0, de 8 bits; el informe, 1 octetos"
  en_orden $X/reales.sal "Dell 06CB:75DB (521 octetos)" "descriptor: 521 octetos, 13 informes" \
    "ratón: informe 2, 2 botones en el bit 0, X en el 8, Y en el 16, de 8 bits; rueda en el 0, de 0 bits; 3 octetos"
  en_orden $X/reales.sal "la pantalla táctil ELAN del XPS 9360 (945 octetos, con Push y Pop): ni ratón ni modo; es una pantalla táctil (Touch Screen, 4)" \
    "descriptor: 945 octetos, 6 informes" "ratón: no" "modo: no" "digitalizador: 4"
  en_orden $X/sint.sal "los informes del ELAN: lo movido (con signo), los botones y la rueda; otro identificador o corto, no es del ratón" \
    "informe: dx=5 dy=-5 botones=1 rueda=1" "informe: dx=-128 dy=127 botones=2 rueda=-1" "informe: no es del ratón" "informe: no es del ratón"
  en_orden $X/sint.sal "el descriptor HID de 30 octetos: bien (04F3:300B, sus registros) y los que no valen (la versión, todo FFh, corto, el largo, sin descriptor de informe)" \
    "hid: bien: 04F3:300B, informe de 361 octetos en 0002h, entrada en 0003h (34), órdenes en 0005h, datos en 0006h" \
    "hid: mal: no es la versión 1.00" "hid: mal: todo FFh o todo 0 (no contesta un HID)" "hid: mal: no llegan los 30 octetos" \
    "hid: mal: no dice que mide 30 octetos" "hid: mal: sus registros no tienen sentido"
  en_orden $X/sint.sal "el modo de entrada a 0 (ratón): la orden SET_REPORT de la característica 3, el registro de datos, el largo (5) y los dos octetos a 0" \
    "modo 0: 05 00 33 03 06 00 05 00 03 00 00"
  en_orden $X/sint.sal "las órdenes: SET_POWER, RESET, SET_REPORT con un identificador de 15 o más (el octeto de más) y GET_REPORT" \
    "orden: 05 00 00 08" "orden: 05 00 00 01" "orden: 05 00 3F 03 14" "orden: 20 00 34 02"
  en_orden $X/sint.sal "el Dell: sus informes y su modo de entrada (informe 4, un octeto)" \
    "informe: dx=-1 dy=1 botones=2 rueda=0" "informe: dx=0 dy=0 botones=3 rueda=0" "informe: no es del ratón" \
    "hid: bien: 046D:B000, informe de 665 octetos en 0020h, entrada en 0021h (64), órdenes en 0023h, datos en 0024h" \
    "modo 0: 23 00 34 03 24 00 04 00 04 00"
  en_orden $X/sint.sal "un descriptor que se acaba a medias no rompe nada (y no da ratón)" "descriptor: 13 octetos, 1 informes, mal hecho" "ratón: no"
  en_orden $X/sint.sal "un ratón USB sin identificador, con X e Y de 12 bits por separado, cinco botones y la rueda (lo que también usa usbhid.cas)" \
    "ratón: informe 0, 5 botones en el bit 0, X en el 8, Y en el 20, de 12 bits; rueda en el 32, de 8 bits; 5 octetos" \
    "informe: dx=-1 dy=0 botones=17 rueda=-1"
else
  ko "uhidprue.cas no compila"
fi

# ---- 2. lo de ACPI, con acpi_sim ----
if (cd $R && make -s build/acpi_sim) > $X/make.txt 2>&1; then
  tabla() { for f in "$@"; do printf -- '-t %s ' "$f"; done; }
  python3 -I $R/tests/acpi/hazi2chid.py $X/i2chid.aml
  if cmp -s $X/i2chid.aml $R/tests/acpi/i2chid-qemu.aml; then ok "tests/acpi/hazi2chid.py hace la misma SSDT que tests/acpi/i2chid-qemu.aml"
  else ko "tests/acpi/i2chid-qemu.aml no es la que hace hazi2chid.py (vuelva a hacerla)"; fi
  timeout 120 $R/build/acpi_sim -i2chid -n 1 -o 38 -osi 0 $(tabla $TB/*.BIN) -t $R/tests/acpi/i2chid-qemu.aml -e $TB/escenario.txt > $X/sim.txt 2>&1
  S=$X/sim.txt
  mira "con la SSDT de prueba: el ELAN presente, su I2cSerialBus del _CRS (15h, 400 kHz, \\_SB.PCI0.I2C1), su GpioInt, el _DSM (0001h) y SSCN/FMCN de la controladora (_ADR 00150001: 00:15.1)" \
    "^I2CHID 1: ._SB_.PCI0.I2C1.TPD0 hid=ELAN1200 cid=PNP0C50 sta=F esclavo=15 velocidad=400000 diez=0 fuente=._SB.PCI0.I2C1 adr=00150001 pci=00A900 registro=1 gpio=55/100 ss=528/640/30 fm=128/160/30 origen=0$"
  mira "el Synaptics que su _STA da por ausente se apunta igual; su _CRS solo trae la interrupción (como las DSDT de Intel sin Windows 8) y la dirección sale de su ResourceTemplate (2Ch, 100 kHz); _DSM 0020h" \
    "^I2CHID 2: ._SB_.PCI0.I2C1.TPD1 hid=SYNA2393 cid=PNP0C50 sta=0 esclavo=2C velocidad=100000 diez=0 fuente=._SB.PCI0.I2C1 adr=00150001 pci=00A900 registro=20 gpio=-1/0 ss=528/640/30 fm=128/160/30 origen=1$" \
    "^ACPI: I2C-HID: 2 dispositivos HID sobre I2C (PNP0C50)"
  if ! grep -q "ACPI Error\|ACPI Exception" $S; then ok "sin un error de ACPICA"; else ko "errores de ACPICA: $(grep -m2 'ACPI Error\|ACPI Exception' $S)"; fi
  # la del ThinkPad E14 Gen 5 (Raptor Lake): «NULL» en el I2cSerialBus de la
  # plantilla (el _CRS solo trae la interrupción): la controladora, su padre
  python3 -I $R/tests/acpi/hazi2chid.py $X/i2cnull.aml null
  timeout 120 $R/build/acpi_sim -i2chid -n 1 -o 38 -osi 0 $(tabla $TB/*.BIN) -t $X/i2cnull.aml -e $TB/escenario.txt > $X/simnull.txt 2>&1
  S=$X/simnull.txt
  mira "como el ThinkPad E14 Gen 5: su I2cSerialBus dice «NULL» (no existe): la controladora es la de su padre, \\_SB.PCI0.I2C0 (su _ADR: 00:15.0, y su FMCN); sin _ADR, por el nombre del padre (I2C3: 15.3)" \
    "^ACPI: I2C-HID: ._SB_.PCI0.I2C0.TPD0 ELAN06C6 (_CID PNP0C50), _STA Fh: I2cSerialBus 15h a 400000 Hz por NULL, de su ResourceTemplate (no del _CRS) (no existe: la de su padre, ._SB_.PCI0.I2C0, _ADR 00150000h)" \
    "^I2CHID 1: ._SB_.PCI0.I2C0.TPD0 hid=ELAN06C6 cid=PNP0C50 sta=F esclavo=15 velocidad=400000 diez=0 fuente=NULL adr=00150000 pci=00A800 registro=1 gpio=-1/0 ss=0/0/0 fm=100/214/28 origen=5$" \
    "^I2CHID 2: ._SB_.PCI0.I2C3.TPD3 hid=SYNA3602 .*fuente=NULL adr=FFFFFFFF pci=00AB00 .*origen=6$"
  timeout 120 $R/build/acpi_sim -i2chid -n 1 -o 38 -osi 0 $(tabla $TB/*.BIN) -e $TB/escenario.txt > $X/sim0.txt 2>&1
  S=$X/sim0.txt
  mira "el Toshiba NB250 (de 2010) no tiene ninguno" "^ACPI: I2C-HID: 0 dispositivos HID sobre I2C (PNP0C50)"
  # el ASUS GL553VD: SDS0 (GNVS 7AF05000h + 37Dh) dice qué touchpad se ha
  # elegido (5: el que haya; 1 y 2: Synaptics), SMD0 (PNVA 7AF0A018h + 115h)
  # = 2, la controladora en el PCI (su _ADR); los bits ELAN y AWTP del CMOS
  # (índice D2h por 72h/73h), qué ha visto la BIOS
  if [ -f $A/DSDT.BIN ]; then
    asus() {      # etiqueta SDS0 lo-que-da-el-CMOS
      printf 'es 73 %s\n' $3 > $X/$1.esc
      timeout 120 $R/build/acpi_sim -i2chid -n 1 -o 38 -osi 0 -m 7AF0537D $2 -m 7AF0A12D 2 $(tabla $A/FACP.BIN $A/DSDT.BIN $A/FACS.BIN $A/SSDT*.BIN) -e $X/$1.esc > $X/$1.txt 2>&1
      S=$X/$1.txt
    }
    asus elan 5 04
    mira "ASUS GL553VD (sus tablas de verdad), con un ELAN: el _INI de \\_SB.PCI0.I2C0.TPD0 lo hace ELAN1200 en 15h a 400 kHz, descriptor en 0001h; la dirección, de SBFB (su _CRS, sin Windows 8, solo da la interrupción); 00:15.0" \
      "^I2CHID 1: ._SB_.PCI0.I2C0.TPD0 hid=ELAN1200 cid=PNP0C50 sta=F esclavo=15 velocidad=400000 diez=0 fuente=._SB.PCI0.I2C0 adr=00150000 pci=00A800 registro=1 .*origen=1$"
    asus fte 5 08
    mira "ASUS GL553VD con un FocalTech: FTE1200, también en 15h y 0001h" \
      "^I2CHID 1: ._SB_.PCI0.I2C0.TPD0 hid=FTE1200 .*esclavo=15 velocidad=400000 .*registro=1 "
    asus syna 1 00
    mira "ASUS GL553VD con el Synaptics elegido: SYNA2393 en 20h, descriptor en 0020h" \
      "^I2CHID 1: ._SB_.PCI0.I2C0.TPD0 hid=SYNA2393 .*esclavo=20 velocidad=400000 .*registro=20 "
    asus nada 0 00
    mira "ASUS GL553VD sin touchpad elegido: _STA 0, y se apunta igual (con la dirección de su plantilla: 20h)" \
      "^I2CHID 1: ._SB_.PCI0.I2C0.TPD0 hid=XXXX0000 cid=PNP0C50 sta=0 esclavo=20 " "^I2CHID 2: ._SB_.PCI0.I2C1.TPL1 "
  else
    echo "  (sin tests/acpi/asus-gl553vd: sin las tablas del ASUS)"
  fi
else
  ko "no se hace build/acpi_sim: $(tail -2 $X/make.txt)"
fi

# ---- 3. en QEMU ----
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/semilla.cas -o $X/semilla.asm > /dev/null && nasm -f bin $X/semilla.asm -o $X/SEMILLA.BIN || { ko "semilla.cas no compila"; return; }
if ! bash $R/acpi/pentium/hacer.sh $X/ACPI.BIN > $X/hacer.txt 2>&1; then ko "acpi/pentium/hacer.sh no hace ACPI.BIN: $(tail -n 1 $X/hacer.txt)"; return; fi
MODULOS=$X/ACPI.BIN ARRANQUE="registro" $AQUI/nucleo32/probar.sh $X/SEMILLA.BIN ":pausa 6" "usb" ":pausa 1" > $X/q0.sal 2>&1
S=$X/q0.sal
mira "QEMU (sin controladora I2C): el arranque dice «no hay controladora I2C» y sigue; ACPI.BIN no halla ningún PNP0C50 y el núcleo lo dice" \
  "^i2c: no hay controladora I2C (Intel LPSS)$" "^ACPI: I2C-HID: 0 dispositivos HID sobre I2C (PNP0C50)" \
  "i2c-hid: ACPI no dice ningún dispositivo HID sobre I2C (PNP0C50)" "^Escriba «ayuda»."
if ! grep -q "excepción\|\*\*\*" $S; then ok "sin una excepción del núcleo"; else ko "hay excepciones: $(grep -m2 'excepción\|\*\*\*' $S)"; fi
MODULOS=$X/ACPI.BIN ARRANQUE="registro" QEMU_MAS="-acpitable file=$R/tests/acpi/i2chid-qemu.aml" $AQUI/nucleo32/probar.sh $X/SEMILLA.BIN ":pausa 6" > $X/q1.sal 2>&1
S=$X/q1.sal
mira "QEMU con la SSDT de prueba: ACPI.BIN halla los dos touchpads y se los da al núcleo (la llamada 159), que dice que no tiene controladora I2C en marcha" \
  "^ACPI: I2C-HID: ._SB_.PCI0.I2C1.TPD0 ELAN1200 (_CID PNP0C50), _STA Fh: I2cSerialBus 15h a 400000 Hz por ._SB.PCI0.I2C1 (_ADR 00150001h), GpioInt 55" \
  "^ACPI: I2C-HID: 2 dispositivos HID sobre I2C (PNP0C50)" \
  "i2c-hid: ACPI dice 2 dispositivos HID sobre I2C, pero no hay controladora I2C en marcha$"
if ! grep -q "excepción\|\*\*\*\|ACPI Error" $S; then ok "sin una excepción del núcleo ni un error de ACPICA"; else ko "hay excepciones: $(grep -m2 'excepción\|\*\*\*\|ACPI Error' $S)"; fi
# las zonas PCI sin poner (a 0) o por encima de los 4 GB: así dejan los
# firmware de Tiger y Raptor Lake (el Monovate 14.1, el ThinkPad E14 Gen 5)
# las de las I2C; QEMU no tiene I2C, así que con «pci=cero» el núcleo pone a
# 0 la zona 0 de las controladoras USB (la del xHCI es de 64 bits; la del
# EHCI, de 32) y con «pci=arriba» la del xHCI en lo alto (1 0000 0000h más):
# pci_memoria tiene que darles dirección o bajarla, y el USB tiene que ir
MODULOS=$X/ACPI.BIN ARRANQUE="registro pci=cero" QEMU_MAS="-device qemu-xhci,id=u -device usb-kbd,bus=u.0 -device usb-mouse,bus=u.0 -device usb-ehci,id=e -device usb-kbd,bus=e.0" \
  $AQUI/nucleo32/probar.sh $X/SEMILLA.BIN ":pausa 6" > $X/q3.sal 2>&1
S=$X/q3.sal
mira "QEMU con las zonas del xHCI (64 bits) y del EHCI (32) sin poner («pci=cero»): pci_memoria les da dirección debajo de los 4 GB, lo apunta (con lo que decía el registro), y van los teclados y el ratón" \
  "^pci: la zona del 00:0.\.0 (registro 10h: 0000000[04]h, alta 0h) estaba sin poner: puesta en [0-9A-F]*h ([0-9]* K); puestas, 2$" \
  "^usb: controladora xHCI 1B36:000D" "^usb: controladora EHCI 8086:24CD" "^usb: 2 teclados, 1 ratón, 0 memorias\.$"
if ! grep -q "excepción\|\*\*\*\|no se ha podido\|no se le ha podido" $S; then ok "sin una excepción ni una zona sin poner"; else ko "hay quejas: $(grep -m2 'excepción\|\*\*\*\|no se ha podido\|no se le ha podido' $S)"; fi
MODULOS=$X/ACPI.BIN ARRANQUE="registro pci=arriba" QEMU_MAS="-device qemu-xhci,id=u -device usb-kbd,bus=u.0 -device usb-mouse,bus=u.0" \
  $AQUI/nucleo32/probar.sh $X/SEMILLA.BIN ":pausa 6" > $X/q4.sal 2>&1
S=$X/q4.sal
mira "QEMU por la BIOS con la zona del xHCI por encima de los 4 GB («pci=arriba»): la mueve debajo y va el USB" \
  "^pci: la zona de memoria del 00:0.\.0 estaba por encima de los 4 GB (1[0-9A-F]*h): movida a [0-9A-F]*h (16 K); movidas, 1$" "^usb: 1 teclado, 1 ratón, 0 memorias\.$"
MODULOS=$X/ACPI.BIN ARRANQUE="registro" QEMU_MAS="-acpitable file=$X/i2cnull.aml" $AQUI/nucleo32/probar.sh $X/SEMILLA.BIN ":pausa 6" > $X/q5.sal 2>&1
S=$X/q5.sal
mira "QEMU con la SSDT como la del ThinkPad E14 («NULL»): ACPI.BIN da la controladora del padre (00:15.0) y el núcleo dice que no está en marcha" \
  "^ACPI: I2C-HID: ._SB_.PCI0.I2C0.TPD0 ELAN06C6 .*no existe: la de su padre, ._SB_.PCI0.I2C0, _ADR 00150000h" \
  "i2c-hid: ACPI dice 2 dispositivos HID sobre I2C, pero no hay controladora I2C en marcha$"
# (0.8.10) el Monovate 14.1 de verdad: el SYNA3602 de la I2C0 (_STA Fh, 2Ch)
# contestaba bien, pero TPD0 y TPL1 (_STA 0, en la misma) hallaban con las
# direcciones de reserva el mismo en 2Ch y lo arrancaban otra vez (tres
# RESET, tres lectores quitándose los informes: la flecha no se movía); en
# la I2C1, todos daban «otro mandaba en el bus». Con «i2csim», i2c.cas pone
# dos controladoras de mentira (00:15.0 con un touchpad en 2Ch y 00:15.1 con
# el bus perdido) y la SSDT «monovate» de hazi2chid.py
python3 -I $R/tests/acpi/hazi2chid.py $X/i2cmono.aml monovate
MODULOS=$X/ACPI.BIN ARRANQUE="registro i2csim" QEMU_MAS="-acpitable file=$X/i2cmono.aml" $AQUI/nucleo32/probar.sh $X/SEMILLA.BIN ":pausa 8" > $X/q6.sal 2>&1
S=$X/q6.sal
mira "como el Monovate («i2csim» y la SSDT «monovate»): las dos controladoras de mentira, ACPI.BIN da los cinco y el SYNA3602 de 2Ch se pone en marcha" \
  "^i2c: controladora 00:15.0 simulada («i2csim»): un touchpad HID en 2Ch" "^ACPI: I2C-HID: 5 dispositivos HID sobre I2C (PNP0C50)" \
  "^i2c-hid:   el descriptor HID (2Ch, registro 0020h): bien: 06CB:7E7E" "^i2c-hid:   en marcha"
l=$(grep -c "^i2c-hid:   en marcha" $S); n=$(grep -c "^i2c-hid:   RESET: contestado" $S)
if [ "$l" = 1 ] && [ "$n" = 1 ]; then ok "y se arranca UNA sola vez (un RESET, un lector), no una por cada candidato de la I2C0"
else ko "el touchpad de 2Ch se arranca $l veces ($n RESET)"; fi
l=$(grep -c "^i2c-hid:   (ya hay un touchpad en marcha en esta controladora: sin las direcciones de reserva)" $S)
if [ "$l" = 2 ] && [ "$(grep -c "^i2c-hid:   el descriptor HID (2Ch" $S)" = 1 ]; then ok "TPD0 y TPL1 (_STA 0, en la misma I2C0) no prueban las direcciones de reserva"
else ko "los de _STA 0 prueban las de reserva ($l): $(grep -m3 'descriptor HID' $S)"; fi
l=$(grep -c "^i2c-hid:   otro mandaba en el bus: en esta controladora no se prueba nada más" $S)
if [ "$l" = 1 ] && grep -q "): en su controladora otro mandaba en el bus; no se prueba$" $S && [ "$(grep -c 'descriptor HID .*otro mandaba' $S)" = 1 ]; then
  ok "en la I2C1, tras el primer «otro mandaba en el bus», ni una prueba más (dicho una vez)"
else ko "en la I2C1 se sigue probando: $(grep -c 'otro mandaba' $S) veces «otro mandaba»"; fi
if ! grep -q "excepción\|\*\*\*\|ACPI Error" $S; then ok "sin una excepción del núcleo ni un error de ACPICA"; else ko "hay excepciones: $(grep -m2 'excepción\|\*\*\*\|ACPI Error' $S)"; fi
MODULOS=$X/ACPI.BIN ARRANQUE="registro sini2c" QEMU_MAS="-acpitable file=$R/tests/acpi/i2chid-qemu.aml" $AQUI/nucleo32/probar.sh $X/SEMILLA.BIN ":pausa 6" > $X/q2.sal 2>&1
S=$X/q2.sal
if grep -q "^acpi: el servicio de ACPI (ACPI.BIN) en marcha" $S && ! grep -q "^i2c\|I2C-HID" $S; then ok "con «sini2c» en la línea del cargador, ni el núcleo ni ACPI.BIN buscan nada de I2C"
else ko "con «sini2c» se ha buscado algo: $(grep -m3 '^i2c\|I2C-HID\|^acpi:' $S)"; fi
}
