#!/bin/bash
# HSPTLS.DLL (w32/tls/hsptls.c, el código tal cual, con BearSSL) en el
# anfitrión, con gcc y un KERNEL32 y un WSOCK32 de mentira (tests/tls/):
#  - la semilla (HSPTLS.SEM) se guarda junto al programa o, si ahí no se
#    puede escribir (un disquete protegido), en C:\; sin TSC ni semilla,
#    «poco azar», también en TlsInfo (lo enseña HTTPS /V);
#  - las autoridades de más: solo los *.CER de AUTORID\ (uno suelto junto al
#    programa, el que deja HTTPS.EXE al bajarlo, no vale);
#  - TlsEscribir no se queda colgado si el servidor no lee;
#  - la memoria de certificados no da por buena una cadena caducada y guarda
#    los usos de la clave de verdad.
# HSPTLS host test (seed file, extra anchors folder, write timeout, cache).
set -u
cd "$(dirname "$0")/.."
echo "== Hispa-486: HSPTLS (en el anfitrión) =="
command -v gcc > /dev/null || { echo "  (sin gcc: se salta)"; exit 0; }
command -v openssl > /dev/null || { echo "  (sin openssl: se salta)"; exit 0; }
T=$(mktemp -d)
trap 'rm -rf $T' EXIT
fail=0
mkdir -p $T/o
BF="-DBR_64=0 -DBR_INT128=0 -DBR_UMUL128=0 -DBR_SSE2=0 -DBR_AES_X86NI=0 -DBR_RDRAND=0 -DBR_POWER8=0 -DBR_USE_URANDOM=0 -DBR_USE_GETENTROPY=0 -DBR_USE_UNIX_TIME=0"
for f in terceros/bearssl/src/*.c; do
  gcc -O1 -w $BF -Iterceros/bearssl/inc -Iterceros/bearssl/src -c $f -o $T/o/$(basename $f .c).o || { echo "  FAIL BearSSL no se compila"; exit 1; }
done
gcc -O1 -g -w $BF -Iterceros/bearssl/inc -Iterceros/bearssl/src -Iw32/tls -o $T/tls tests/tls/anfitrion.c $T/o/*.o || { echo "  FAIL no se compila"; exit 1; }
# los certificados: una raíz y la hoja de x.es (3 días)
C=$T/c
mkdir -p $C
openssl ecparam -name prime256v1 -genkey -noout -out $C/raiz.key 2> /dev/null
openssl req -x509 -new -key $C/raiz.key -subj /CN=Raiz-pruebas -days 30 -out $C/raiz.pem \
  -addext basicConstraints=critical,CA:TRUE -addext keyUsage=critical,keyCertSign 2> /dev/null
openssl ecparam -name prime256v1 -genkey -noout -out $C/hoja.key 2> /dev/null
openssl req -new -key $C/hoja.key -subj /CN=x.es -out $C/hoja.csr 2> /dev/null
printf 'subjectAltName=DNS:x.es\nkeyUsage=critical,digitalSignature\n' > $C/ext
openssl x509 -req -in $C/hoja.csr -CA $C/raiz.pem -CAkey $C/raiz.key -CAcreateserial -days 3 -extfile $C/ext -out $C/hoja.pem 2> /dev/null
R=$T/disco
mkdir -p $R/PROG
openssl x509 -in $C/raiz.pem -outform DER -out $R/PROG/RAIZ.CER
openssl x509 -in $C/hoja.pem -outform DER -out $R/HOJA.DER
corre() { RAIZ=$R timeout 60 $T/tls "$@" || fail=1; }
corre semilla
corre anclas
mkdir -p $R/PROG/AUTORID
mv $R/PROG/RAIZ.CER $R/PROG/AUTORID/RAIZ.CER
corre anclas autorid
corre escribe
corre memoria
[ $fail = 0 ] && echo "  HSPTLS: todo bien" || echo "  HSPTLS: HAY FALLOS"
exit $fail
