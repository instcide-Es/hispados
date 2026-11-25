#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo32/disco.sh IMAGEN [12|16|32|mbr] [oficina] [red]
# Hace la imagen de un disco duro FAT para Hispa-Pentium, con el intérprete de
# órdenes (ORDENES.BIN), los programas de usuario de ejemplo (HOLA.BIN, PRUEBA.BIN,
# ECO.BIN, MALO.BIN, CLI.BIN, ESCRIBE.BIN, BUCLE.BIN), los del escritorio
# (BLOC.BIN, el Bloc de notas, CONSOLA.BIN, la Consola, y EQUIPO.BIN, Mi equipo), unos archivos de órdenes
# (LOTE.BAT, SINFIN.BAT, VISTO.BAT) y unos archivos de texto (LEEME.TXT, la
# carpeta TEXTOS y la carpeta LARGOS, con nombres largos). Sin más: FAT16 (16 MB). Con 12, FAT12 (4 MB); con 32, FAT32 (40 MB);
# con mbr, un disco con tabla de particiones y la FAT16 en su primera
# partición (desde el sector 63). Con «oficina» detrás, también HVPRUN.BIN (el
# ejecutor de los programas de Hispa Ventanas) y la carpeta OFICINA con los de
# la oficina de Hispa-386 (FACTURAS.HVP, PLUMA.HVP, CUENTAS.HVP, MIRADOR.HVP;
# (OFICINA2) DATOS, LECTOR, PUBLICA, PRESENTA y PINTURA.HVP y TIPOS12/24/32.DAT):
# nucleo32/hvprun/hacer.sh. Con «red», también el servicio de red (RED.BIN) y
# sus programas: IPCONFIG.BIN, PING.BIN y DESCARGA.BIN (la orden DESCARGAR).
# Con «tls» (y «red»), también el servicio TLS (TLS.BIN, nucleo32/tls/hacer.sh:
# necesita gcc con -m32) y HTTPS.BIN (la orden HTTPS). Con «ssh» (y «red»),
# también SSH.BIN y SSHD.BIN (el cliente y el servidor SSH), WG.BIN (WireGuard) y
# TAILSCALE.BIN: nucleo32/hispac/hacer.sh.
# Con «navegador» (y «red» y «tls»), también el Navegador (nucleo32/navega/hacer.sh)
# en \NAVEGA, su acceso en \MENU (NAVEGA.MNU, con el icono en el tapiz) y
# X87.BIN y XMM.BIN (las pruebas de que cada programa tiene su coma flotante,
# el x87 y el SSE: castel/pruebas.sh). Con 32,
# que caben (16 MB se quedan cortos). (ESCUDO) Con «escudo», el antivirus
# (nucleo32/escudo/hacer.sh): ESCUDO.BIN, \ESCUDO y su acceso en \MENU.
# (WIFI) Con «wifi» (y «red»), también el servicio Wi-Fi (WIFI.BIN,
# nucleo32/wifi/hacer.sh), la ventana del Wi-Fi (REDWIFI.BIN) y el
# microcódigo de las Broadcom (BCM43XX.FW, BCM43XXH.FW y su licencia,
# BCM43XX.TXT); con «wifisim», además un RED.INI con «WifiSimulada=SI» (la
# radio de mentira: las pruebas).
# (PANEL) Con «panel», también el Panel de control (PANEL.BIN) y la hora por
# Internet (HORARED.BIN), que el escritorio pone en marcha al arrancar.
# La imagen se le da a QEMU como disco IDE: DISCO=IMAGEN nucleo32/probar.sh ...
# Necesita python3, nasm, mkfs.fat (dosfstools) y mtools.
set -e
export LC_ALL=C.UTF-8
AQUI=$(dirname "$(readlink -f "$0")")
IMG=${1:?uso: disco.sh IMAGEN [12|16|32|mbr]}
CLASE=${2:-16}
OFICINA=
RED=
TLS=
NAVEGADOR=
ESCUDO=
SSH=
WIFI=
PANEL=
for o in "${@:3}"; do
  case $o in oficina) OFICINA=oficina ;; red) RED=red ;; tls) TLS=tls ;; navegador) NAVEGADOR=navegador ;; ssh) SSH=ssh ;; escudo) ESCUDO=escudo ;; wifi) WIFI=wifi ;; wifisim) WIFI=wifisim ;; panel) PANEL=panel ;; esac
done
T=$(mktemp -d)
trap 'rm -rf $T' EXIT
# los programas: cada uno, con su nombre en el disco
for p in uordenes:ORDENES uhola:HOLA ueco:ECO umalo:MALO ucli:CLI uprueba:PRUEBA uescribe:ESCRIBE ubucle:BUCLE ubloc:BLOC uconsola:CONSOLA uequipo:EQUIPO; do
  f=${p%%:*}; N=${p##*:}
  python3 -I $AQUI/../ccastel.py $AQUI/$f.cas -o $T/$f.asm > /dev/null
  nasm -f bin $T/$f.asm -o $T/$N.BIN
done
# los textos (en la página 437, la del PC, y con sus finales de línea de DOS)
printf 'Hispa-Pentium: un n\xa3cleo de 32 bits escrito en Castel.\r\nEste archivo est\xa0 en un disco FAT y lo ense\xa4a la orden TYPE.\r\n' > $T/LEEME.TXT
python3 - > $T/LARGO.TXT <<'PY'
import sys
for i in range(1, 401):
    sys.stdout.write('linea %03d de un archivo de varios clusters\r\n' % i)
PY
printf 'Con diez ca\xa4ones por banda,\r\nviento en popa, a toda vela,\r\nno corta el mar, sino vuela\r\nun velero bergant\xa1n.\r\n' > $T/PIRATA.TXT
printf 'esto no es un programa\r\n' > $T/NOPROG.BIN
printf 'un archivo con el nombre largo\r\n' > "$T/nombre bien largo.txt"
printf 'Que es mi barco mi tesoro,\r\nque es mi dios la libertad.\r\n' > "$T/cancion_del_pirata.txt"
printf 'Feliz a\xa4o nuevo.\r\n' > "$T/Año nuevo.txt"
# los archivos de órdenes
printf '@ECHO OFF\r\nREM un archivo de \xa2rdenes de prueba: LOTE uno dos\r\nECHO lote: empiezo con %%1 y %%2 (y el 100%%%%)\r\nECO desde el lote\r\nECHO lote: una l\xa1nea > LOTE.TXT\r\nECHO lote: y otra >> LOTE.TXT\r\nTYPE LOTE.TXT\r\nDEL LOTE.TXT\r\nECHO.\r\nPAUSE\r\nECHO lote: fin\r\n' > $T/LOTE.BAT
printf '@ECHO OFF\r\nECHO sinfin: lanzo BUCLE\r\nBUCLE\r\nECHO sinfin: esto no ten\xa1a que salir\r\n' > $T/SINFIN.BAT
printf 'REM con ECHO activado se ve cada orden\r\nECO visto\r\n@ECHO esta no se ve' > $T/VISTO.BAT
V=$T/vol.img
case $CLASE in
  12) mkfs.fat -F 12 -n HISPA -C $V 4096 > /dev/null ;;
  16|mbr) mkfs.fat -F 16 -n HISPA -C $V 16384 > /dev/null ;;
  32) mkfs.fat -F 32 -s 1 -n HISPA -C $V 40960 > /dev/null ;;
  *) echo "disco.sh: la clase es 12, 16, 32 o mbr"; exit 2 ;;
esac
export MTOOLS_SKIP_CHECK=1
mcopy -i $V $T/ORDENES.BIN $T/HOLA.BIN $T/ECO.BIN $T/MALO.BIN $T/CLI.BIN $T/PRUEBA.BIN $T/LEEME.TXT $T/NOPROG.BIN "$T/nombre bien largo.txt" ::
mmd -i $V ::TEXTOS ::VACIA ::LARGOS
mcopy -i $V $T/ESCRIBE.BIN $T/BUCLE.BIN $T/LOTE.BAT $T/SINFIN.BAT $T/VISTO.BAT $T/BLOC.BIN $T/CONSOLA.BIN $T/EQUIPO.BIN ::
mcopy -i $V "$T/cancion_del_pirata.txt" "$T/Año nuevo.txt" ::LARGOS/
mcopy -i $V $T/LARGO.TXT $T/PIRATA.TXT ::TEXTOS/
mcopy -i $V $T/ECO.BIN ::TEXTOS/ECO2.BIN
if [ "$RED" = red ]; then
  for p in ured:RED uipconf:IPCONFIG uping:PING udescarg:DESCARGA; do
    f=${p%%:*}; N=${p##*:}
    python3 -I $AQUI/../ccastel.py $AQUI/$f.cas -o $T/$f.asm > /dev/null
    nasm -f bin $T/$f.asm -o $T/$N.BIN
  done
  mcopy -i $V $T/RED.BIN $T/IPCONFIG.BIN $T/PING.BIN $T/DESCARGA.BIN ::
  # (TLS) el servicio TLS y la orden HTTPS
  if [ "$TLS" = tls ]; then
    bash $AQUI/tls/hacer.sh $T/TLS.BIN
    python3 -I $AQUI/../ccastel.py $AQUI/uhttps.cas -o $T/uhttps.asm > /dev/null
    nasm -f bin $T/uhttps.asm -o $T/HTTPS.BIN
    mcopy -i $V $T/TLS.BIN $T/HTTPS.BIN ::
  fi
  # (SSH) el cliente y el servidor SSH, WireGuard y Tailscale (nucleo32/hispac/hacer.sh)
  if [ "$SSH" = ssh ]; then
    bash $AQUI/hispac/hacer.sh $T/hc
    mcopy -i $V $T/hc/*.BIN ::
  fi
  # (WIFI) el servicio Wi-Fi, su ventana y el microcódigo de las Broadcom
  if [ -n "$WIFI" ]; then
    bash $AQUI/wifi/hacer.sh $T/WIFI.BIN
    python3 -I $AQUI/../ccastel.py $AQUI/uredwifi.cas -o $T/uredwifi.asm > /dev/null
    nasm -f bin $T/uredwifi.asm -o $T/REDWIFI.BIN
    cp $AQUI/../../wifi/firmware/bcm43xx-0.fw $T/BCM43XX.FW
    cp $AQUI/../../wifi/firmware/bcm43xx_hdr-0.fw $T/BCM43XXH.FW
    sed 's/$/\r/' $AQUI/../../wifi/firmware/LICENCE.broadcom_bcm43xx > $T/BCM43XX.TXT
    mcopy -i $V $T/WIFI.BIN $T/REDWIFI.BIN $T/BCM43XX.FW $T/BCM43XXH.FW $T/BCM43XX.TXT ::
    if [ "$WIFI" = wifisim ]; then
      printf 'WifiSimulada=SI\r\n' > $T/RED.INI
      mcopy -i $V $T/RED.INI ::
    fi
  fi
fi
# (PANEL) el Panel de control y la hora por Internet
if [ "$PANEL" = panel ]; then
  for p in upanel:PANEL uhorared:HORARED; do
    f=${p%%:*}; N=${p##*:}
    python3 -I $AQUI/../ccastel.py $AQUI/$f.cas -o $T/$f.asm > /dev/null
    nasm -f bin $T/$f.asm -o $T/$N.BIN
  done
  mcopy -i $V $T/PANEL.BIN $T/HORARED.BIN ::
fi
if [ "$OFICINA" = oficina ]; then
  bash $AQUI/hvprun/hacer.sh $T/of > /dev/null
  mcopy -i $V $T/of/HVPRUN.BIN ::
  mmd -i $V ::OFICINA
  # (OFICINA2) todos los de la oficina 1.1 (y Pintura) con sus tipos de letra
  mcopy -i $V $T/of/*.HVP $T/of/*.DAT ::OFICINA/
fi
# (NAVEGADOR) el Navegador, sin instalar: en \NAVEGA y su acceso en \MENU
if [ "$NAVEGADOR" = navegador ]; then
  mkdir -p $T/nav/NAVEGA
  NAVEGA_PROGRAMA='\NAVEGA\NAVEGA.BIN' NAVEGA_ICONO='\MENU\NAVEGA.ICP' bash $AQUI/navega/hacer.sh $T/nav/NAVEGA
  make -s -C $AQUI/../.. -f castel/nucleo32/navega/navega.mk build/navega-pentium/X87.BIN build/navega-pentium/XMM.BIN > /dev/null
  mcopy -s -i $V $T/nav/NAVEGA ::
  mmd -i $V ::MENU
  mcopy -i $V $T/nav/NAVEGA.MNU $T/nav/NAVEGA.ICP ::MENU/
  mcopy -i $V $AQUI/../../build/navega-pentium/X87.BIN $AQUI/../../build/navega-pentium/XMM.BIN ::
fi
# (ESCUDO) el antivirus, sin instalar: ESCUDO.BIN en la raíz, \ESCUDO (su LEEME;
# las firmas, en \ESCUDO\FIRMAS, las pone quien lo pruebe) y su acceso en \MENU
if [ "$ESCUDO" = escudo ]; then
  ESCUDO_PROGRAMA='\ESCUDO.BIN' ESCUDO_ICONO='\MENU\ESCUDO.ICP' bash $AQUI/escudo/hacer.sh $T/esc
  mcopy -i $V $T/esc/ESCUDO.BIN ::
  mmd -i $V ::ESCUDO ::ESCUDO/FIRMAS
  mcopy -i $V $T/esc/ESCUDO/LEEME.TXT ::ESCUDO/
  mmd -i $V ::MENU 2> /dev/null || true
  mcopy -i $V $T/esc/MENU/ESCUDO.MNU $T/esc/MENU/ESCUDO.ICP ::MENU/
fi
if [ $CLASE = mbr ]; then
  # una tabla de particiones con una sola, de FAT16 (clase 0x06), desde el sector 63
  python3 - $V $IMG <<'PY'
import sys, struct
vol = open(sys.argv[1], 'rb').read()
mbr = bytearray(512)
mbr[446:462] = struct.pack('<BBBBBBBBII', 0x80, 1, 1, 0, 0x06, 254, 63, 0, 63, len(vol) // 512)
mbr[510:512] = b'\x55\xaa'
open(sys.argv[2], 'wb').write(bytes(mbr) + bytes(62 * 512) + vol)
PY
else
  cp $V $IMG
fi
echo "disco.sh: $IMG (FAT, $CLASE), $(($(stat -c %s $IMG) / 1024)) K"
