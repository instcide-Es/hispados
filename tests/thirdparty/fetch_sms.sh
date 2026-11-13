#!/bin/bash
# Descarga las pruebas de libre distribución del Z80 para Hispa-System:
# zexdoc y zexall, de Frank D. Cringle (1994), que prueban cada instrucción
# del procesador contra lo que hace uno de verdad (zexdoc, las banderas
# documentadas; zexall, también los bits 3 y 5). Son programas de CP/M con
# licencia GPL versión 2 (con su fuente, en el mismo sitio): se podrían
# llevar en el proyecto, pero como las de Hispa-Nes se bajan para la prueba y
# se guardan en tests/thirdparty/dl/sms/ (ignorado). Si no hay red, esas
# pruebas se saltan.
# Downloads the GPL-2 Z80 instruction exercisers (zexdoc, zexall) for the tests.
set -u
DIR=$(cd "$(dirname "$0")" && pwd)/dl/sms
mkdir -p "$DIR"
baja() {                                # baja URL NOMBRE
  [ -s "$DIR/$2" ] && return 0
  curl -sfL --max-time 60 -A "Mozilla/5.0" -o "$DIR/$2.tmp" "$1" && mv "$DIR/$2.tmp" "$DIR/$2" && echo "  $2"
  rm -f "$DIR/$2.tmp"
}
BASE=https://raw.githubusercontent.com/agn453/ZEXALL/main
baja "$BASE/zexdoc.com" zexdoc.com
baja "$BASE/zexall.com" zexall.com
baja "$BASE/LICENSE" LICENSE
exit 0
