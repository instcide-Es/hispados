#!/bin/bash
# Prueba de HTTPS.EXE de Hispa-486 (w32/https.c, el código tal cual) en el
# anfitrión, con gcc y una red de mentira (tests/https/red_falsa.c: cada
# conexión responde con un archivo). Con AddressSanitizer si lo hay: un
# desbordamiento hace fallar la prueba.
#  - una redirección relativa larga, con un servidor largo: no cabe y no se
#    sigue (antes desbordaba dir, ruta y pet);
#  - una que sí cabe (990 letras): se sigue y se guarda;
#  - una respuesta solo con \n\n, o sin cabeceras: no escribe en la dirección 0;
#  - de https:// a http:// no se sigue, salvo con /I;
#  - «//servidor/...» y «ruta» relativa, contra la dirección de ahora.
# HTTPS.EXE host test with a fake network (redirects, overflow, downgrade).
set -u
cd "$(dirname "$0")/.."
echo "== Hispa-486: HTTPS.EXE (en el anfitrión, con una red de mentira) =="
command -v gcc > /dev/null || { echo "  (sin gcc: se salta)"; exit 0; }
T=$(mktemp -d)
trap 'rm -rf $T' EXIT
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
SAN="-fsanitize=address,undefined -fno-omit-frame-pointer"
echo 'int main(void){return 0;}' > $T/t.c
gcc $SAN $T/t.c -o $T/t 2> /dev/null || SAN=""
[ -n "$SAN" ] && echo "  (con AddressSanitizer)"
gcc -O1 -g -w $SAN -o $T/https w32/https.c tests/https/red_falsa.c || { ko "se compila"; exit 1; }
# corre NOMBRE argumentos...: con las respuestas ya puestas en $T/NOMBRE
corre() {
  local n=$1; shift
  (cd $T/$n && RED=$T/$n ASAN_OPTIONS=detect_leaks=0 timeout 20 $T/https "$@" > salida.txt 2>&1; echo $? > codigo)
}
resp() { mkdir -p $T/$1; printf '%b' "$3" > $T/$1/RESP$2; }
A1000=$(printf 'A%.0s' $(seq 1 999))
S250=$(printf 's%.0s' $(seq 1 240)).es
# 1. la redirección que no cabe
resp largo 1 "HTTP/1.1 302 Found\r\nLocation: /$A1000\r\n\r\n"
corre largo /N "http://$S250/"
c=$(cat $T/largo/codigo)
if [ "$c" = 10 ] && grep -q "demasiado larga" $T/largo/salida.txt && ! grep -q "ERROR: AddressSanitizer\|runtime error" $T/largo/salida.txt; then ok "una redirección larga con un servidor largo: no se sigue (sale con 10)"
else ko "una redirección larga (sale con $c)"; head -c 600 $T/largo/salida.txt; fi
# 2. la que cabe
A990=$(printf 'B%.0s' $(seq 1 989))
resp cabe 1 "HTTP/1.1 301 Moved\r\nLocation: /$A990\r\n\r\n"
resp cabe 2 "HTTP/1.1 200 OK\r\nContent-Length: 4\r\n\r\nhola"
corre cabe "http://x.es/" BAJADO.TXT
if [ "$(cat $T/cabe/codigo)" = 0 ] && [ "$(cat $T/cabe/BAJADO.TXT 2>/dev/null)" = hola ] && grep -q "GET /$A990 HTTP/1.0" $T/cabe/PET2; then ok "una redirección de 990 letras: se sigue y se guarda"
else ko "una redirección que cabe (sale con $(cat $T/cabe/codigo))"; head -c 600 $T/cabe/salida.txt; fi
# 3. sin \r, y sin nada
resp nl 1 "HTTP/1.1 404 No esta\n\n"
corre nl /N "http://x.es/"
if [ "$(cat $T/nl/codigo)" = 8 ] && grep -q "El servidor responde: HTTP/1.1 404 No esta" $T/nl/salida.txt; then ok "una respuesta solo con \\\\n: dice el estado (sale con 8)"
else ko "respuesta solo con \\n (sale con $(cat $T/nl/codigo))"; cat $T/nl/salida.txt; fi
resp nada 1 ""
corre nada /N "http://x.es/"
if [ "$(cat $T/nada/codigo)" = 8 ] && grep -q "no responde como se espera" $T/nada/salida.txt; then ok "sin respuesta: no escribe en la dirección 0 (sale con 8)"
else ko "sin respuesta (sale con $(cat $T/nada/codigo))"; cat $T/nada/salida.txt; fi
# 4. de https a http
resp baja 1 "HTTP/1.1 302 Found\r\nLocation: http://x.es/a.txt\r\n\r\n"
resp baja 2 "HTTP/1.1 200 OK\r\n\r\nsin cifrar"
corre baja /N "https://x.es/"
if [ "$(cat $T/baja/codigo)" = 11 ] && grep -q "de https a http" $T/baja/salida.txt && [ ! -f $T/baja/PET2 ]; then ok "de https a http: no se sigue (sale con 11)"
else ko "de https a http (sale con $(cat $T/baja/codigo))"; cat $T/baja/salida.txt; fi
resp bajai 1 "HTTP/1.1 302 Found\r\nLocation: http://x.es/a.txt\r\n\r\n"
resp bajai 2 "HTTP/1.1 200 OK\r\n\r\nsin cifrar"
corre bajai /N /I "https://x.es/"
if [ "$(cat $T/bajai/codigo)" = 0 ] && grep -q "GET /a.txt" $T/bajai/PET2; then ok "de https a http con /I: se sigue"
else ko "de https a http con /I (sale con $(cat $T/bajai/codigo))"; cat $T/bajai/salida.txt; fi
# 5. relativas
resp rel 1 "HTTP/1.1 302 Found\r\nLocation: otra.htm\r\n\r\n"
resp rel 2 "HTTP/1.1 302 Found\r\nLocation: //otro.es/b\r\n\r\n"
resp rel 3 "HTTP/1.1 200 OK\r\n\r\nya"
corre rel /N "http://x.es/dir/una.htm?q=1"
if [ "$(cat $T/rel/codigo)" = 0 ] && grep -q "GET /dir/otra.htm HTTP" $T/rel/PET2 && grep -q "Host: otro.es" $T/rel/PET3; then ok "«ruta» y «//servidor/...»: contra la dirección de ahora"
else ko "redirecciones relativas (sale con $(cat $T/rel/codigo))"; cat $T/rel/salida.txt; fi
# 6. una dirección de 1000 letras con un servidor de 255: no cabe en la petición

mkdir -p $T/largo2; corre largo2 /N "http://$S250/$(printf 'C%.0s' $(seq 1 740))"
if ! grep -q "ERROR: AddressSanitizer\|runtime error" $T/largo2/salida.txt; then ok "una dirección de 1000 letras: sin desbordar"
else ko "una dirección de 1000 letras"; head -c 600 $T/largo2/salida.txt; fi
[ $fail = 0 ] && echo "  HTTPS.EXE: todo bien" || echo "  HTTPS.EXE: HAY FALLOS"
exit $fail
