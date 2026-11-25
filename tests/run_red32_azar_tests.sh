#!/bin/bash
# El azar y el DNS de WSOCK32.DLL (w32/wsock32.c, el código tal cual; docs/RED.md,
# «El azar de la red») en el anfitrión, con gcc (tests/red32/azar.c): ChaCha20
# contra el RFC 8439, que el generador no deja adivinar lo siguiente, el
# número inicial de TCP del RFC 6528 y que una respuesta de DNS solo vale del
# servidor al que se preguntó, desde el puerto 53 y con el número de la
# pregunta. Con AddressSanitizer si lo hay.
# WSOCK32 randomness, TCP ISN and DNS source check host test.
set -u
cd "$(dirname "$0")/.."
echo "== Hispa-486: el azar y el DNS de WSOCK32 (en el anfitrión) =="
command -v gcc > /dev/null || { echo "  (sin gcc: se salta)"; exit 0; }
T=$(mktemp -d)
trap 'rm -rf $T' EXIT
SAN="-fsanitize=address,undefined"
echo 'int main(void){return 0;}' > $T/t.c
gcc $SAN $T/t.c -o $T/t 2> /dev/null || SAN=""
gcc -O1 -g -w $SAN -o $T/azar tests/red32/azar.c || { echo "  FAIL no se compila"; exit 1; }
ASAN_OPTIONS=detect_leaks=0 $T/azar
