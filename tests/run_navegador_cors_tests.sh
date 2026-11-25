#!/bin/bash
# [CORS] Las reglas del origen de los dos Navegadores de Hispa-486 (NAVEGA.EXE
# y NAVEGA2.EXE; docs/486/NAVEGADOR.md, «Lo que puede pedir una página»), en
# el anfitrión y sin QEMU, con el código de verdad:
#  1. w32/navega/cors.h con gcc (tests/cors/prueba.c): el origen de una
#     dirección, SameSite, las cabeceras que puede mandar una página (sin las
#     prohibidas; a otro origen, solo las simples y GET, HEAD o POST), si
#     puede leer la respuesta (Access-Control-Allow-Origin y -Credentials) y
#     las cabeceras que ve (nunca Set-Cookie).
#  2. w32/navega/dom.js con node (tests/navjs/origen.js): localStorage no le
#     da ningún origen a C (cambiar URL.prototype.origin no sirve) y fetch y
#     XMLHttpRequest dicen qué galletas piden.
# Lo mismo con el Navegador entero, en el anfitrión con gcc -m32 (la página
# /js/cors): tests/run_navegador_js_tests.sh.
# Browser origin rules (CORS, SameSite, localStorage origin) host test.
set -u
cd "$(dirname "$0")/.."
echo "== Hispa-486: el origen en los Navegadores (CORS, SameSite, localStorage) =="
fail=0
T=$(mktemp -d)
trap 'rm -rf $T' EXIT
if command -v gcc > /dev/null && gcc -Wall -Wextra -Werror -O1 -o $T/cors tests/cors/prueba.c; then
  echo "-- cors.h (gcc) --"
  $T/cors || fail=1
else echo "  FAIL cors.h no se compila"; fail=1; fi
if command -v node > /dev/null; then
  echo "-- dom.js (node) --"
  node tests/navjs/origen.js w32/navega/dom.js || fail=1
else echo "  (sin node: lo de dom.js se salta)"; fi
[ $fail = 0 ] && echo "  el origen: todo bien" || echo "  el origen: HAY FALLOS"
exit $fail
