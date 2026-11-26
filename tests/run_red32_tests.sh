#!/bin/bash
# Prueba de la red de los programas Hispa32 (Hispa-486, docs/RED.md), sin
# Internet: WSOCK32.DLL (la pila TCP/IP en C y Winsock 1.1) y HSPTLS.DLL (TLS
# 1.2 con comprobación del certificado), desde dos programas de consola, contra
# un servidor de pruebas en el anfitrión (tests/tools/nav_servidor.py: QEMU lo
# ve en 10.0.2.2) con certificados hechos aquí (tests/tools/nav_certs.sh); su
# raíz solo se le da al sistema en la prueba (RAIZ.CER en AUTORID\, junto al programa).
#  - REDPRUE: DHCP, ARP, una descarga de 150 KB por TCP (suma de los octetos),
#    conexión rechazada, y lo mismo sin bloqueo con select.
#  - HTTPS.EXE: redirecciones; descarga cifrada (ECDSA y RSA) con su suma; la
#    segunda vez, con el certificado recordado; guardar en un archivo; los
#    certificados malos (de otro nombre, sin autoridad conocida, caducado) y el
#    reloj del equipo mal puesto; un servidor que no existe.
#  - Con otra tarjeta (NE2000) y que al acabar el sistema sigue entero.
# (GUARDAR=carpeta deja allí lo que ha salido por la consola.)
# Hispa32 networking test (TCP/IP stack and TLS), offline.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
B=build
make -s $B/HW32.COM $B/MSVCRT.DLL $B/WSOCK32.DLL $B/HSPTLS.DLL $B/HTTPS.EXE $B/REDPRUE.EXE > /dev/null 2>&1 || {
  echo "== Hispa-486: la red de los programas Hispa32: sin gcc de 32 bits, se salta =="; exit 0; }
T=$(mktemp -d)
P=$((20000 + $$ % 20000))
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() { if grep -qF -- "$2" $T/$1.txt; then ok "$2"; else ko "$2"; fi; }
checkn() { if grep -qF -- "$2" $T/$1.txt; then ko "(no debía salir) $2"; else ok "sin «$2»"; fi; }
echo "== Hispa-486: la red de los programas Hispa32 (WSOCK32.DLL y HSPTLS.DLL) =="
. tests/tools/nav_certs.sh
if ! nav_certs $T; then echo "  (sin openssl no se pueden hacer los certificados: se salta)"; rm -rf $T; exit 0; fi
python3 tests/tools/nav_servidor.py $T $P > $T/srv.log 2>&1 &
SRV=$!
trap 'kill $SRV 2>/dev/null; [ -n "${GUARDAR:-}" ] && cp $T/*.txt $T/pedidos.log "$GUARDAR"/ 2>/dev/null; rm -rf $T' EXIT
for i in 1 2 3 4 5 6 7 8 9 10; do grep -q listo $T/srv.log 2>/dev/null && break; sleep 0.5; done
grep -q listo $T/srv.log || { echo "  (no arranca el servidor de pruebas: se salta)"; exit 0; }
SUMA=$(python3 -c "import random; random.seed(7); print(sum(bytes(random.getrandbits(8) for _ in range(150000))))")
export MTOOLS_SKIP_CHECK=1
cp "$IMG" $T/a.img
printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nFILES=30\r\n' > $T/CONFIG.SYS
mcopy -o -i $T/a.img $T/CONFIG.SYS ::CONFIG.SYS
disco_b() {
  dd if=/dev/zero of=$T/b.img bs=1474560 count=1 2> /dev/null
  mformat -i $T/b.img -f 1440 ::
  for f in HW32.COM MSVCRT.DLL WSOCK32.DLL HSPTLS.DLL HTTPS.EXE REDPRUE.EXE; do mcopy -i $T/b.img $B/$f ::; done
  mmd -i $T/b.img ::AUTORID
  mcopy -i $T/b.img $T/RAIZ.CER ::AUTORID/      # (las autoridades de más, en AUTORID\)
}
# corre NOMBRE TARJETA TIEMPO «s|orden»...
corre() {
  local n=$1 nic=$2 tmo=$3; shift 3
  { sleep 6
    for c in "$@"; do printf '%s\r' "${c#*|}"; sleep "${c%%|*}"; done
  } | timeout $tmo qemu-system-i386 -display none -serial stdio -monitor none -parallel none -m 16 \
        -drive file=$T/a.img,format=raw,if=floppy,index=0 -drive file=$T/b.img,format=raw,if=floppy,index=1 \
        -boot a -no-reboot -netdev user,id=n0 -device $nic,netdev=n0 > $T/$n.raw 2>&1
  tr -d '\r' < $T/$n.raw | iconv -f CP437 -t UTF-8 -c > $T/$n.txt
}
H='B:\HW32 B:\'
disco_b
corre uno rtl8139 150 '2|A:\HISPADOS\HSPDPMI' '2|A:\HISPADOS\HSPRED' '2|B:' \
  "9|${H}REDPRUE.EXE 10.0.2.2 $P /archivo.bin" \
  "10|${H}REDPRUE.EXE 10.0.2.2 $P /archivo.bin P" \
  "6|${H}HTTPS.EXE /N http://10.0.2.2:$P/redir" \
  "10|${H}HTTPS.EXE /V /N https://10.0.2.2:$((P+1))/archivo.bin" \
  "9|${H}HTTPS.EXE /V /N https://10.0.2.2:$((P+1))/archivo.bin" \
  '2|ECHO ---RSA' "9|${H}HTTPS.EXE /V /N https://10.0.2.2:$((P+5))/archivo.bin" \
  "12|${H}HTTPS.EXE https://10.0.2.2:$((P+1))/archivo.bin B:\\BAJADO.BIN" \
  '2|ECHO ---OTRO' "5|${H}HTTPS.EXE /N https://10.0.2.2:$((P+2))/" '1|IF ERRORLEVEL 4 ECHO NIVEL-4' \
  '2|ECHO ---SUELTO' "5|${H}HTTPS.EXE /N https://10.0.2.2:$((P+3))/" \
  '2|ECHO ---CADUCADO' "5|${H}HTTPS.EXE /N https://10.0.2.2:$((P+4))/" \
  '2|ECHO ---404' "5|${H}HTTPS.EXE /N https://10.0.2.2:$((P+1))/noesta" '1|IF ERRORLEVEL 8 ECHO NIVEL-8' \
  '2|ECHO ---NOEXISTE' "12|${H}HTTPS.EXE /N http://noexiste.invalid/" \
  '2|ECHO ---FECHA' '2|FECHA 01-01-1995' "6|${H}HTTPS.EXE /N https://10.0.2.2:$((P+1))/" \
  '3|DIR B:\HSPTLS.*' '2|ECHO SIGUE-VIVO'
echo "-- la pila TCP/IP: DHCP, ARP y una descarga de 150 KB (RTL8139) --"
check uno "IP 10.0.2.15 puerta 10.0.2.2 MAC 52:54:00:12:34:56"
check uno "Respuesta: HTTP/1.1 200 OK"
check uno "Cuerpo: 150000 octetos, suma $SUMA"
echo "-- sin bloqueo, con select; un puerto cerrado --"
check uno "Puerto cerrado: -1, error 10061"
check uno "connect sin bloqueo: -1, error 10035"
check uno "select: 1 (escribir 1, fallo 0)"
[ "$(grep -cF "Cuerpo: 150000 octetos, suma $SUMA" $T/uno.txt)" = 2 ] && ok "la descarga sin bloqueo, entera" || ko "la descarga sin bloqueo"
echo "-- HTTPS.EXE: redirecciones y descarga cifrada --"
check uno "Redirigido a http://10.0.2.2:$P/redir2"
check uno "Redirigido a http://10.0.2.2:$P/final"
check uno "Conexión segura: TLS 1.2, ECDHE-ECDSA, AES-256-GCM"
[ "$(grep -cF "Recibidos 150000 octetos, suma $SUMA." $T/uno.txt)" = 3 ] && ok "tres descargas cifradas enteras (la segunda, con el certificado recordado; la tercera, RSA)" \
  || ko "las descargas cifradas ($(grep -cF "Recibidos 150000 octetos, suma $SUMA." $T/uno.txt) de 3)"
sed -n '/---RSA/,/---OTRO/p' $T/uno.txt > $T/rsa.txt
check rsa "Conexión segura: TLS 1.2, ECDHE-RSA, ChaCha20-Poly1305"
check uno "Guardado en B:\\BAJADO.BIN (150000 octetos)."
mcopy -n -i $T/b.img ::BAJADO.BIN $T/bajado.bin 2> /dev/null
if python3 -c "
import random, sys
random.seed(7)
sys.exit(0 if open('$T/bajado.bin','rb').read() == bytes(random.getrandbits(8) for _ in range(150000)) else 1)" 2> /dev/null
then ok "el archivo guardado es el del servidor"; else ko "el archivo guardado no es el del servidor"; fi
echo "-- los certificados que no valen --"
sed -n '/---OTRO/,/---SUELTO/p' $T/uno.txt > $T/otro.txt
sed -n '/---SUELTO/,/---CADUCADO/p' $T/uno.txt > $T/suelto.txt
sed -n '/---CADUCADO/,/---404/p' $T/uno.txt > $T/caducado.txt
sed -n '/---404/,/---NOEXISTE/p' $T/uno.txt > $T/e404.txt
sed -n '/---NOEXISTE/,/---FECHA/p' $T/uno.txt > $T/noexiste.txt
sed -n '/---FECHA/,$p' $T/uno.txt > $T/fecha.txt
check otro "El certificado es de otro sitio, no del que se ha pedido."
check otro "NIVEL-4"
checkn otro "Recibidos"
check suelto "El certificado del servidor no lo firma ninguna autoridad conocida."
checkn suelto "Recibidos"
check caducado "El certificado del servidor ha caducado o aún no vale. Compruebe la fecha del equipo."
checkn caducado "Recibidos"
echo "-- una página que no existe, un servidor que no existe y el reloj mal puesto --"
check e404 "El servidor responde: HTTP/1.1 404"
check e404 "NIVEL-8"
check noexiste "No se encuentra el servidor noexiste.invalid."
check fecha "El certificado del servidor ha caducado o aún no vale. Compruebe la fecha del equipo."
check fecha "HSPTLS   SEM"
check fecha "HSPTLS   CCH"
check uno "SIGUE-VIVO"
grep -qF "GET /archivo.bin | - |  | Hispa-486 (HTTPS.EXE)" $T/pedidos.log && ok "el servidor ha visto a HTTPS.EXE" || ko "el servidor no ha visto a HTTPS.EXE"
echo "-- con una NE2000 y sin la raíz de pruebas --"
disco_b
mdel -i $T/b.img ::AUTORID/RAIZ.CER
corre dos ne2k_pci 60 '2|A:\HISPADOS\HSPDPMI' '2|A:\HISPADOS\HSPRED' \
  "9|${H}REDPRUE.EXE 10.0.2.2 $P /archivo.bin" \
  "6|${H}HTTPS.EXE /N https://10.0.2.2:$((P+1))/" \
  '2|A:\HISPADOS\HSPRED /U' '2|ECHO SIGUE-VIVO'
check dos "Cuerpo: 150000 octetos, suma $SUMA"
check dos "El certificado del servidor no lo firma ninguna autoridad conocida."
check dos "SIGUE-VIVO"
if grep -qiE "descargado|quitado|memoria" $T/dos.txt; then ok "HSPRED se descarga después (los programas soltaron la red)"; else ko "HSPRED no se descarga después"; fi
exit $fail
