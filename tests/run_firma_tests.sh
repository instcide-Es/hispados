#!/bin/bash
# La firma de las actualizaciones (0.9.31, docs/RED.md «La firma de
# versiones.txt»):
#   1) utils/firma/cripto16.c en el anfitrión (gcc): SHA-256 (los bordes de
#      los bloques, por trozos) y Ed25519 (RFC 8032 y firmas de Python, buenas
#      y estropeadas: un bit del mensaje, de R, de S, otra clave, sin la
#      etiqueta, S+L) contra hashlib y «cryptography» (tests/firma/vectores.py);
#   2) lo mismo, compilado con gcc -m16 como en ACTUALIZ.COM, en QEMU
#      (build/FIRMATST.COM, en el disquete: modo real);
#   3) tools/firma_versiones.py: firma, comprueba, pone sha256=/octetos=, no
#      vale con otra clave ni cambiada, y no deja firmar una lista que no cabe;
#   4) ACTUALIZ.COM lleva la clave de verdad (la de Hispa-Pentium) y no la de
#      prueba; ACTPRUEB.COM, la de prueba; y los dos caben en 64 KB.
# Signed updates test.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
T=$(mktemp -d)
trap 'rm -rf $T' EXIT
for f in build/FIRMATST.COM build/ACTUALIZAR.COM build/ACTPRUEB.COM "$IMG"; do
  [ -f "$f" ] || { echo "falta $f (make $f)"; exit 1; }
done
echo "== La firma de las actualizaciones =="

echo "-- 1) las cuentas en el anfitrión --"
gcc -O2 -Wall -Werror -o $T/prueba tests/firma/prueba_cripto.c || { ko "no compila prueba_cripto.c"; exit 1; }
python3 tests/firma/vectores.py > $T/vec.txt
$T/prueba < $T/vec.txt > $T/r1.txt
n=$(wc -l < $T/vec.txt)
if grep -q "^cripto16 (anfitrión): $n OK, 0 FAIL" $T/r1.txt; then ok "$n casos de SHA-256 y Ed25519 (anfitrión)"; else ko "anfitrión: $(tail -1 $T/r1.txt)"; grep FAIL $T/r1.txt | head; fi

echo "-- 2) lo de ACTUALIZ.COM (gcc -m16), en QEMU --"
cp "$IMG" $T/f.img
mcopy -o -i $T/f.img build/FIRMATST.COM ::FIRMATST.COM
{ sleep 6; printf 'FIRMATST\r'; sleep 20; printf 'ECHO =FIN=\r'; sleep 2; } | timeout 60 qemu-system-i386 -display none -serial stdio \
    -monitor none -m 32 -drive file=$T/f.img,format=raw,if=floppy -boot a -no-reboot > $T/q.raw 2>&1
tr -d '\r' < $T/q.raw | iconv -f CP437 -t UTF-8 -c > $T/q.txt
for c in "SHA-256 abc: BIEN" "firma RFC 8032: BIEN" "firma cambiada: BIEN"; do
  if grep -qF "$c" $T/q.txt; then ok "$c (modo real)"; else ko "$c (modo real)"; fi
done
grep -q "MAL" $T/q.txt && ko "FIRMATST dice MAL"

echo "-- 3) tools/firma_versiones.py --"
K=tests/firma/clave_prueba.key
mkdir -p $T/w
printf 'contenido del paquete\n' > $T/w/hispados-9.9.9.paq
printf '[9.9.9]\r\ncpu=386\r\npaquete=http://x/hispados-9.9.9.paq\r\nsha256=viejo\r\nnota=Prueba\r\nfirma=ed25519 00\r\n' > $T/w/versiones.txt
if python3 tools/firma_versiones.py firma $T/w/versiones.txt --clave $K > $T/f.txt; then ok "firma"; else ko "firma"; fi
h=$(sha256sum $T/w/hispados-9.9.9.paq | cut -d' ' -f1)
grep -q "^sha256=$h"$'\r'"$" $T/w/versiones.txt && ok "pone sha256= del paquete (y quita el viejo)" || ko "sha256="
grep -q "^octetos=22"$'\r'"$" $T/w/versiones.txt && ok "pone octetos=" || ko "octetos="
[ "$(grep -c '^firma=' $T/w/versiones.txt)" = 1 ] && [ "$(tail -1 $T/w/versiones.txt | cut -c1-14)" = "firma=ed25519 " ] && ok "una sola firma, al final" || ko "la línea de la firma"
grep -q "^sha256=viejo" $T/w/versiones.txt && ko "quedó el sha256= viejo"
pub=$(python3 -c "
from cryptography.hazmat.primitives.asymmetric import ed25519
from cryptography.hazmat.primitives import serialization
k=ed25519.Ed25519PrivateKey.from_private_bytes(bytes.fromhex(open('$K').read().strip()))
print(k.public_key().public_bytes(serialization.Encoding.Raw, serialization.PublicFormat.Raw).hex())")
python3 tools/firma_versiones.py comprueba $T/w/versiones.txt --publica tests/firma/clave_prueba.inc --paquetes $T/w > $T/c.txt \
  && grep -q "cuadra" $T/c.txt && ok "comprueba (la clave del .inc) y el paquete cuadra" || { ko "comprueba"; cat $T/c.txt; }
python3 tools/firma_versiones.py comprueba $T/w/versiones.txt --publica $pub > /dev/null && ok "comprueba (la clave en hex)" || ko "comprueba en hex"
python3 tools/firma_versiones.py comprueba $T/w/versiones.txt > /dev/null && ko "vale con la clave de verdad (no debe)" || ok "con la clave de verdad, no vale"
sed 's/nota=Prueba/nota=Otra/' $T/w/versiones.txt > $T/w/cambiada.txt
python3 tools/firma_versiones.py comprueba $T/w/cambiada.txt --publica $pub > /dev/null && ko "vale cambiada" || ok "cambiada, no vale"
printf 'x' >> $T/w/hispados-9.9.9.paq
python3 tools/firma_versiones.py comprueba $T/w/versiones.txt --publica $pub --paquetes $T/w > $T/c2.txt
grep -q "NO CUADRA" $T/c2.txt && ok "un paquete cambiado no cuadra" || ko "el paquete cambiado cuadra"
{ printf '[9.9.8]\n'; for i in $(seq 1 60); do printf 'nota=%070d\n' $i; done; } > $T/w/larga.txt
python3 tools/firma_versiones.py firma $T/w/larga.txt --clave $K > /dev/null 2>&1 && ko "firma una lista de más de 4095 octetos" || ok "no firma una lista que ACTUALIZAR no lee entera"
printf '[9.9.7]\npaquete=http://x/noesta.paq\n' > $T/w/falta.txt
python3 tools/firma_versiones.py firma $T/w/falta.txt --clave $K > /dev/null 2>&1 && ko "firma sin el paquete" || ok "no firma si falta el paquete"
# la firma de firma_versiones es la que comprueba cripto16 (la etiqueta)
python3 - "$K" $T/w/versiones.txt > $T/v2.txt <<'EOP'
import sys
from cryptography.hazmat.primitives.asymmetric import ed25519
from cryptography.hazmat.primitives import serialization
k = ed25519.Ed25519PrivateKey.from_private_bytes(bytes.fromhex(open(sys.argv[1]).read().strip()))
pub = k.public_key().public_bytes(serialization.Encoding.Raw, serialization.PublicFormat.Raw)
d = open(sys.argv[2], 'rb').read()
p = d.rfind(b'firma=')
f = bytes.fromhex(d[p + 14:p + 14 + 128].decode())
print('ed', pub.hex(), f.hex(), b'HISPADOS-VERSIONES 1\n'.hex(), d[:p].hex(), 1)
print('ed', pub.hex(), f.hex(), '-', d[:p].hex(), 0)
EOP
$T/prueba < $T/v2.txt | grep -q "2 OK, 0 FAIL" && ok "cripto16 acepta la firma del guion (con la etiqueta) y sin ella no" || ko "cripto16 y el guion no casan"

echo "-- 4) las claves de cada programa --"
real=$(python3 -c "
import re
t=open('lib/clave_firma.inc').read().split('clave_act:')[1]
print(bytes(int(x,16) for x in re.findall(r'0x([0-9A-F]{2})',t)[:32]).hex())")
cas=$(python3 -c "
import re
t=open('castel/nucleo32/clave.cas',encoding='utf-8').read().split('pública')[-1]
print(bytes(int(x,16) for x in re.findall(r'0x([0-9A-Fa-f]{2})',t)[:32]).hex())")
[ "$real" = "$cas" ] && ok "lib/clave_firma.inc = la de Hispa-Pentium (clave.cas)" || ko "lib/clave_firma.inc no es la de clave.cas"
if python3 -c "import sys; d=open('build/ACTUALIZAR.COM','rb').read(); sys.exit(0 if bytes.fromhex('$real') in d and bytes.fromhex('$pub') not in d else 1)"; then
  ok "ACTUALIZ.COM lleva la clave de verdad y no la de prueba"; else ko "ACTUALIZ.COM: la clave"; fi
if python3 -c "import sys; d=open('build/ACTPRUEB.COM','rb').read(); sys.exit(0 if bytes.fromhex('$pub') in d and bytes.fromhex('$real') not in d else 1)"; then
  ok "ACTPRUEB.COM lleva la de prueba"; else ko "ACTPRUEB.COM: la clave"; fi
python3 tools/tam_com.py build/ACTUALIZAR.COM > /dev/null && ok "ACTUALIZ.COM cabe en 64 KB" || ko "ACTUALIZ.COM no cabe"
for img in build/hispados.img build/hispados-usb.img@@1M build/hispa486-usb.img@@1M; do
  f=${img%@@*}; [ -f "$f" ] || continue
  MTOOLS_SKIP_CHECK=1 mcopy -o -i "$img" ::HISPADOS/ACTUALIZ.COM $T/a.com 2>/dev/null || continue
  if python3 -c "import sys; d=open('$T/a.com','rb').read(); sys.exit(0 if bytes.fromhex('$pub') not in d else 1)"; then
    ok "$(basename $f): su ACTUALIZ.COM no lleva la clave de prueba"; else ko "$(basename $f) lleva la clave de PRUEBA"; fi
done
exit $fail
