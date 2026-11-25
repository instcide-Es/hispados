# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# pruebas_rtl.sh - (RTL8723BS) la parte 55 de pruebas.sh (SOLO=rtl), que lee
# este archivo («.»): la función parte_rtl, con los ayudantes de pruebas.sh
# (ok, ko, mira, saca) y sus carpetas ($AQUI, $P, $G).
#
# El Wi-Fi de las tabletas Bay Trail: la Realtek RTL8723BS por SDIO
# (wifi/nucleo/sdio.c, radio_rtl.c, radio_rtl_pc.c; acpi/pentium/sdio.c).
# QEMU no tiene SDIO ni esta tarjeta: un modelo (tests/wifi/rtl_sim.c) de la
# controladora SDHCI, la tarjeta SDIO y el chip, con el punto de acceso de
# mentira detrás:
#   1. en el anfitrión (build/rtl_prueba): el SDIO (CMD5, CMD3, CMD7, el CCCR,
#      el CIS, 4 bits, 50 MHz, la función 1), el EFUSE, el firmware (uno de
#      mentira; con RTL_FW=rtl8723bs_nic.bin, también el de verdad), las
#      tablas, la antena, buscar redes, WPA2, ARP, DHCP y las renovaciones; y
#      el chip apagado (no contesta a CMD5) y un archivo que no es el firmware;
#   2. en QEMU, WIFISIM.BIN con «chiprtl» (el modelo dentro): RED.BIN lo pone
#      en marcha, el firmware sale de \FIRMWARE (con su nombre largo), busca,
#      conecta con WPA2, DHCP y cinco PING;
#   3. sin el firmware: lo dice (dónde ha buscado, de dónde se baja) y deja
#      el DIAGNÓSTICO en WIFI.LOG;
#   4. ACPI.BIN con una SSDT como la de una tableta (tests/acpi/hazsdhci.py
#      «sdio»: la SDIO INT33BB, su hijo OBDA8723, la controladora GPIO SUS
#      INT33FC y los _PS0 que mueven la patilla 26 por una región
#      GeneralPurposeIo): atiende esas regiones y aprende en \SDIO.TXT la
#      controladora, su «BAR1», el chip y las escrituras en la patilla (con
#      la espera de 20 ms); y WIFI.BIN las lee en el arranque siguiente.
parte_rtl() {
echo "== Hispa-Pentium: el Wi-Fi por SDIO de las tabletas Bay Trail (Realtek RTL8723BS, contra un modelo) (RTL8723BS) =="
local X=$P/rtl S U k
rm -rf $X; mkdir -p $X
export MTOOLS_SKIP_CHECK=1
# ---- 1. en el anfitrión ----
if make -s -C $AQUI/.. build/rtl_prueba > $X/make.txt 2>&1; then
  if $AQUI/../build/rtl_prueba > $X/anf.txt 2>&1 && grep -q ", 0 fallos$" $X/anf.txt; then ok "(RTL8723BS) en el anfitrión, el SDIO y el controlador contra el modelo: $(tail -1 $X/anf.txt)"
  else ko "(RTL8723BS) build/rtl_prueba falla:"; grep "FALLA\|MODELO" $X/anf.txt | head -8 | sed 's/^/        /'; fi
  if [ -n "${RTL_FW:-}" ]; then
    if $AQUI/../build/rtl_prueba "$RTL_FW" > $X/anf2.txt 2>&1 && grep -q ", 0 fallos$" $X/anf2.txt; then ok "(RTL8723BS) y con el firmware de verdad ($RTL_FW): $(tail -1 $X/anf2.txt)"
    else ko "(RTL8723BS) con el firmware de verdad falla:"; grep "FALLA\|MODELO" $X/anf2.txt | head -8 | sed 's/^/        /'; fi
  fi
else ko "(RTL8723BS) no se hace build/rtl_prueba: $(tail -3 $X/make.txt)"; fi
# ---- 2. WIFISIM.BIN con «chiprtl», en QEMU ----
if ! $AQUI/nucleo32/disco.sh $X/d.img 16 red wifisim > $X/disco.txt 2>&1 || ! bash $AQUI/nucleo32/wifi/hacer.sh $X/WIFISIM.BIN chiprtl > $X/hacer.txt 2>&1; then
  ko "(RTL8723BS) no se pueden hacer el disco o WIFISIM.BIN: $(tail -2 $X/disco.txt) $(tail -2 $X/hacer.txt)"
  return
fi
# un firmware de mentira, con la cabecera de la 8723B (5301h, versión 35)
python3 -I -c '
import sys
b = bytearray((k * 131 + (k >> 7)) & 255 for k in range(32108))
b[0:32] = bytes(32); b[0] = 1; b[1] = 0x53; b[2] = 0x10; b[4] = 0x23
open(sys.argv[1], "wb").write(b)' $X/rtl8723bs_nic.bin
printf 'WifiSimulada=CHIP\r\n' > $X/RED.INI
cp $X/d.img $X/k.img
mcopy -o -i $X/k.img $X/WIFISIM.BIN ::WIFI.BIN && mcopy -o -i $X/k.img $X/RED.INI :: && mmd -i $X/k.img ::FIRMWARE && mcopy -i $X/k.img $X/rtl8723bs_nic.bin ::FIRMWARE/
MEMORIA=64 DISCO=$X/k.img ARRANQUE=ordenes MODULOS= $AQUI/nucleo32/probar.sh "" "redwifi /estado" ":pausa 8" "redwifi /conectar HispaPrueba" "clave de prueba" ":pausa 2" \
  "ping -n 5 192.168.77.1" ":pausa 5" > $X/k.sal 2>&1
tr -d '\r' < $X/k.sal > $X/k.txt
S=$X/k.txt
mira "(RTL8723BS) WIFISIM.BIN: el SDIO y la RTL8723BS en el anillo 3 (la controladora, el modelo); el firmware, de \\FIRMWARE con su nombre largo" \
  "^wifi: Se usa la radio de las Realtek RTL8723BS por SDIO" "^wifi: Firmware de la Realtek: .FIRMWARE.rtl8723bs_nic\.bin, 32108 octetos (versión 35\.0)$" \
  "^wifi: sdio: RCA 0001; CCCR 3, SDIO 4, SD 3; capacidades 13; fabricante 024C, tarjeta B723" "^wifi: sdio: lista: 4 bits, 50000 kHz (alta velocidad)" \
  "^wifi: rtl: MAC 40:9F:38:8B:23:01; cristal 22" "^wifi: rtl: firmware 35\.0 (firma 5301, 32108 octetos) en marcha" "^wifi: PASO 3 de 3 hecho: radio en marcha" \
  "^red: Wi-Fi Realtek RTL8723BS (SDIO)$"
mira "(RTL8723BS) y con ella: buscar, WPA2, DHCP y cinco PING sin perder ninguno" "^wifi:   \.\.\.:00:01 canal  6  -46 dBm WPA2" "^wifi: 802\.11: CONECTADO a «HispaPrueba»$" \
  "red: DHCP: IP 192\.168\.77\.10" "^    Paquetes: enviados = 5, recibidos = 5, perdidos = 0"
if grep -q "MODELO:" $S; then ko "(RTL8723BS) el modelo ha visto algo mal:"; grep "MODELO:" $S | head -5 | sed 's/^/        /'; else ok "(RTL8723BS) y el modelo no ha visto nada mal (ni una queja)"; fi
# ---- 3. sin el firmware ----
cp $X/d.img $X/f.img
mcopy -o -i $X/f.img $X/WIFISIM.BIN ::WIFI.BIN && mcopy -o -i $X/f.img $X/RED.INI ::
MEMORIA=64 ESCRIBE=1 DISCO=$X/f.img ARRANQUE=ordenes MODULOS= $AQUI/nucleo32/probar.sh "" ":pausa 3" "ipconfig" ":pausa 8" "redwifi /estado" ":pausa 2" > $X/f.sal 2>&1
tr -d '\r' < $X/f.sal > $X/f.txt
S=$X/f.txt
mira "(RTL8723BS) sin rtl8723bs_nic.bin: lo dice, dónde lo ha buscado y de dónde se baja; y el DIAGNÓSTICO, en WIFI.LOG" \
  "^wifi: ERROR: FALTA EL FIRMWARE de la Realtek RTL8723BS (rtl8723bs_nic\.bin" "^wifi: ERROR:   Se ha buscado rtl8723bs_nic\.bin (y cualquier \.BIN con su cabecera) en: X:.FIRMWARE .FIRMWARE .HISPA.FIRMWARE C:.FIRMWARE C:.HISPA.FIRMWARE$" \
  "^wifi: ERROR:   Póngalo en X:.FIRMWARE.rtl8723bs_nic\.bin de la memoria USB" "linux-firmware\.git/tree/rtlwifi" \
  "^wifi: ==== DIAGN.STICO: la tarjeta está, pero no se ha podido poner en marcha ====$" "^wifi: Tarjeta: Realtek RTL8723BS (SDIO); controlador: Realtek"
# ---- 4. ACPI.BIN aprende la SDIO y cómo se enciende el chip; WIFI.BIN lo lee ----
if ! bash $AQUI/../acpi/pentium/hacer.sh $X/ACPI.BIN > $X/acpi.txt 2>&1 || ! bash $AQUI/nucleo32/wifi/hacer.sh $X/WIFI.BIN > $X/hacer2.txt 2>&1; then
  ko "(RTL8723BS) no se hacen ACPI.BIN o WIFI.BIN: $(tail -1 $X/acpi.txt) $(tail -1 $X/hacer2.txt)"
  return
fi
python3 -I $AQUI/../tests/acpi/hazsdhci.py FEBF1000 $X/sdio.aml sdio
$AQUI/nucleo32/disco.sh $X/u.img 32 red > $X/disco32.txt 2>&1
mcopy -o -i $X/u.img $X/ACPI.BIN ::ACPI.BIN && mcopy -o -i $X/u.img $X/WIFI.BIN ::WIFI.BIN && mcopy -o -i $X/u.img $X/RED.INI :: \
  && mmd -i $X/u.img ::FIRMWARE && mcopy -i $X/u.img $X/rtl8723bs_nic.bin ::FIRMWARE/
U="-device qemu-xhci,id=u -drive id=mu,file=$X/u.img,format=raw,if=none -device usb-storage,bus=u.0,drive=mu"
ESCRIBE=1 MODULOS= ARRANQUE="ordenes escritura=usb unidad=X registro" QEMU_MAS="$U -acpitable file=$X/sdio.aml" $AQUI/nucleo32/probar.sh "" \
  ":pausa 12" salir apagar > $X/aprende.sal 2>&1
tr -d '\r' < $X/aprende.sal > $X/aprende.txt
S=$X/aprende.txt
mira "(RTL8723BS) ACPI.BIN: la SDIO (INT33BB), su _PS0 y el de su hijo (OBDA8723), con la patilla de la controladora GPIO SUS (INT33FC _UID 3) por la región GeneralPurposeIo" \
  "^ACPI: SDIO: controladora GPIO ._SB.GPO2: SUS, registros en FED0E000h$" "^ACPI: SDIO: ._SB.GPO2 patilla 26 (FED0E168h) := 0$" "^ACPI: SDIO: ._SB.GPO2 patilla 26 (FED0E168h) := 1$" \
  "^ACPI: SDIO: _PS0 de ._SB_\.SDHC: hecho$" "^ACPI: SDIO: hijo ._SB_\.SDHC\.RTLW OBDA8723 (dirección 1)$" "^ACPI: SDIO: _PS0 de ._SB_\.SDHC\.RTLW: hecho$" \
  "^ACPI: SDIO: lo aprendido, en .SDIO\.TXT ([0-9]* octetos: la controladora en FEB00000h, BAR1 FEB01000h, OBDA8723, 3 escrituras en las patillas)$"
saca $X/u.img SDIO.TXT $X/sdio.txt
tr -d '\r' < $X/sdio.txt > $X/sdio2.txt 2> /dev/null
k=$(sed -n 's/^espera=\([0-9]*\)$/\1/p' $X/sdio2.txt | head -1)
if grep -q "^equipo=[0-9A-F]\{8\} [0-9A-F]\{8\}$" $X/sdio2.txt && grep -q "^sdio=FEB00000 0$" $X/sdio2.txt && grep -q "^sdio1=FEB01000$" $X/sdio2.txt \
  && grep -q "^hijo=OBDA8723$" $X/sdio2.txt && [ "$(grep -c "^gpio=FED0E168 [0-9A-F]\{8\} 32$" $X/sdio2.txt)" = 3 ] && [ "${k:-0}" -ge 18 ] && [ "${k:-0}" -le 60 ]; then
  ok "(RTL8723BS) \\SDIO.TXT: el equipo, la controladora y su BAR1, el chip y las tres escrituras en la patilla, con la espera de 20 ms del _PS0 ($k ms)"
else ko "(RTL8723BS) \\SDIO.TXT no es como debe:"; sed 's/^/        /' $X/sdio2.txt 2> /dev/null | head -12; mdir -i $X/u.img :: 2>&1 | grep -i "txt" | sed 's/^/        /'; fi
if ! grep -q "excepción\|\*\*\*\|ACPI Error\|ACPI Exception" $S; then ok "(RTL8723BS) ni una excepción del núcleo ni un error de ACPICA"
else ko "(RTL8723BS) hay excepciones o errores de ACPICA:"; grep "excepción\|\*\*\*\|ACPI Error\|ACPI Exception" $S | head -4 | sed 's/^/        /'; fi
# el arranque siguiente, sin ACPI: WIFI.BIN (el de verdad; RED.BIN lo pone en
# marcha con WifiSimulada=CHIP) usa \SDIO.TXT: la controladora a D0 por su
# BAR1, las escrituras repetidas; en QEMU no hay nada en FEB00000
ESCRIBE=1 MODULOS= ARRANQUE="ordenes escritura=usb unidad=X registro sinacpi" QEMU_MAS="$U" $AQUI/nucleo32/probar.sh "" "ipconfig" ":pausa 10" salir apagar > $X/usa.sal 2>&1
tr -d '\r' < $X/usa.sal > $X/usa.txt
S=$X/usa.txt
mira "(RTL8723BS) y en el arranque siguiente, sin ACPI: WIFI.BIN lee \\SDIO.TXT (de este equipo), repite las escrituras y va a la controladora aprendida" \
  "^wifi: rtl: .SDIO\.TXT (lo aprendido de ACPI en este equipo): la controladora SDIO en FEB00000, el chip: OBDA8723; 3 escrituras para encenderla (y el chip)$" \
  "^wifi: rtl: FED0E168 := [0-9A-F]\{8\}" "^wifi: rtl: la controladora SDIO, en FEB00000 (de ACPI (.SDIO\.TXT))"
}
