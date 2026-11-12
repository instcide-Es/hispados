#!/bin/bash
# Prueba de las opciones de DIR (LISTAR): /P /W /A /O /S /B /L /C, pegadas
# o no («Dir/w», «dir *.txt/b»), /-X para anular, DIRCMD y opción no válida.
# DIR switches test.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
cp "$IMG" $T/f.img
mk() { # nombre bytes fecha
  head -c "$2" /dev/zero | tr '\0' 'x' > "$T/$1"; touch -d "$3" "$T/$1"
}
mk A.TXT 10 "2021-03-05 10:00"
mk BB.DOC 3000 "2019-01-01 09:00"
mk C.TXT 500 "2023-07-20 18:30"
mk ZETA.BAT 1 "2020-06-15 12:00"
mk OCULTO.TXT 77 "2022-02-02 08:00"
mk UNO.TXT 123 "2022-05-05 05:05"
mk DOS.TXT 4567 "2022-06-06 06:06"
M="mmd -i $T/f.img"; C="mcopy -m -i $T/f.img"
$M ::PRUEBA ::PRUEBA/SUB ::PRUEBA/SUB/HONDO ::PRUEBA/CARPETA ::MUCHOS
for f in A.TXT BB.DOC C.TXT ZETA.BAT OCULTO.TXT; do $C $T/$f ::PRUEBA/$f; done
mattrib -i $T/f.img +h ::PRUEBA/OCULTO.TXT
$C $T/UNO.TXT ::PRUEBA/SUB/UNO.TXT
$C $T/DOS.TXT ::PRUEBA/SUB/HONDO/DOS.TXT
for i in $(seq -w 1 30); do $C $T/A.TXT ::MUCHOS/F$i.TXT; done
# rutas absolutas: así la prueba no depende de la carpeta actual
P='DIR \PRUEBA'
CMDS=("Dir/w" "$P" 'dir\PRUEBA/w/p' "$P /O-S" 'DIR /ON \PRUEBA' "$P /O" "$P /OD" "$P /O:E"
      'DIR /A:H \PRUEBA' "$P /A" "$P /A-D" "$P /S /B" "$P /S" 'dir \PRUEBA\*.txt/b' "$P /B /L"
      "SET DIRCMD=/W" "$P" "$P /-W" "SET DIRCMD=" "DIR /X" "$P /C /CH" "DIR A B"
      'DIR A:\PRUEBA /O:N /P' 'DIR \MUCHOS /P')
{ sleep 6; for c in "${CMDS[@]}"; do printf '%s\r' "$c"; sleep 1.2; done
  printf 'x'; sleep 1; printf '%s\r' "ECHO fin"; sleep 1; } | \
  timeout 90 qemu-system-i386 -display none -serial stdio -monitor none -parallel none \
  -drive file=$T/f.img,format=raw,if=floppy -boot a -no-reboot > $T/log 2>&1
tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 > $T/log.txt
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
# salida de la n-ésima vez que se tecleó «orden» (hasta el siguiente indicador)
out() { C="$1" N="${2:-1}" awk 'BEGIN { c = ENVIRON["C"]; n = ENVIRON["N"] }
  /^[A-Z]:\\[^>]*>/ { if (on) exit; p = substr($0, index($0, ">") + 1); if (p == c && ++k == n) { on = 1; next } }
  on { print }' $T/log.txt; }
# has / hasnt orden expresión [vez] descripción
has() { if out "$1" "$3" | grep -qE -- "$2"; then ok "$1: $4"; else ko "$1: $4"; fi; }
hasnt() { if out "$1" "$3" | grep -qE -- "$2"; then ko "$1: $4"; else ok "$1: $4"; fi; }
# order orden descripción expr...: cada expresión aparece en una línea posterior
order() { local c="$1" what="$2"; shift 2
  local IFS=$'\t'
  if out "$c" | PATS="$*" awk 'BEGIN { n = split(ENVIRON["PATS"], P, "\t"); i = 1 }
       i <= n && $0 ~ P[i] { i++ } END { exit !(i > n) }'; then ok "$c: $what"; else ko "$c: $what"; fi; }
echo "== Hispa-DOS DIR =="
has  "Dir/w" '^KERNEL\.SYS {5}COMMAND\.COM {4}' 1 "ancho, 15 columnas"
has  "Dir/w" '\[HISPADOS\]' 1 "carpetas entre corchetes"
hasnt "Dir/w" '<DIR>' 1 "sin <DIR>"
has  "Dir/w" '^ +[0-9]+ archivo\(s\) +[0-9.]+ bytes$' 1 "resumen"
has  "$P" '^ Directorio de A:\\PRUEBA$' 1 "cabecera"
has  "$P" '^A        TXT            10 05-03-21  10:00$' 1 "línea de archivo"
has  "$P" '^        8 archivo\(s\)          3\.511 bytes$' 1 "resumen (sin el oculto)"
hasnt "$P" 'OCULTO' 1 "sin el oculto"
has  'dir\PRUEBA/w/p' '^\[\.\] {12}\[\.\.\] {11}' 1 "/W/P pegadas: corchetes"
has  'dir\PRUEBA/w/p' '\[CARPETA\]' 1 "/W/P pegadas: [CARPETA]"
has  'dir\PRUEBA/w/p' '^        8 archivo\(s\)          3\.511 bytes$' 1 "/W/P pegadas: resumen"
hasnt 'dir\PRUEBA/w/p' 'Pulse una tecla' 1 "sin pausa (cabe)"
order "$P /O-S" "por tamaño, de mayor a menor" '^BB +DOC' '^C +TXT' '^A +TXT' '^ZETA +BAT' '<DIR>'
order 'DIR /ON \PRUEBA' "por nombre" '^A +TXT' '^BB +DOC' '^C +TXT' '^CARPETA' '^SUB' '^ZETA'
order "$P /O" "carpetas primero y por nombre" '^\. ' '^CARPETA' '^SUB' '^A +TXT' '^BB' '^C +TXT' '^ZETA'
order "$P /OD" "por fecha" '^BB +DOC' '^ZETA' '^A +TXT' '^C +TXT'
order "$P /O:E" "por extensión" '^SUB' '^ZETA +BAT' '^BB +DOC' 'TXT'
has  'DIR /A:H \PRUEBA' '^OCULTO +TXT +77 ' 1 "el oculto"
has  'DIR /A:H \PRUEBA' '^        1 archivo\(s\)' 1 "solo el oculto"
has  "$P /A" '^OCULTO +TXT' 1 "/A: todos"
has  "$P /A" '^        9 archivo\(s\)' 1 "/A: 9 entradas"
hasnt "$P /A-D" '<DIR>' 1 "sin carpetas"
has  "$P /A-D" '^        5 archivo\(s\)          3\.588 bytes$' 1 "solo archivos (con el oculto)"
has  "$P /S /B" '^A:\\PRUEBA\\SUB\\HONDO\\DOS\.TXT$' 1 "ruta completa"
has  "$P /S /B" '^A:\\PRUEBA\\SUB$' 1 "carpetas con ruta"
hasnt "$P /S /B" '(\\\.|Directorio|archivo\(s\)|OCULTO)' 1 "sin . ni cabecera ni ocultos"
has  "$P /S" '^ Directorio de A:\\PRUEBA\\SUB\\HONDO$' 1 "cabecera por carpeta"
has  "$P /S" '^DOS +TXT +4\.567 ' 1 "archivo de la subcarpeta"
has  "$P /S" '^Total de archivos listados:$' 1 "total"
has  "$P /S" '^       17 archivo\(s\)          8\.201 bytes$' 1 "totales"
has  "$P /S" 'bytes libres$' 1 "bytes libres"
order "$P /S" "subtotal por carpeta" '^ Directorio de A:.PRUEBA$' '^        8 archivo' '^ Directorio de A:.PRUEBA.SUB$' '^        4 archivo' 'HONDO$' '^        3 archivo' '^Total'
has  'dir \PRUEBA\*.txt/b' '^C\.TXT$' 1 "patrón con /b pegada"
hasnt 'dir \PRUEBA\*.txt/b' '(BB\.DOC|Directorio)' 1 "solo los .TXT"
has  "$P /B /L" '^bb\.doc$' 1 "minúsculas"
has  "$P" '\[SUB\]' 2 "DIRCMD=/W"
has  "$P /-W" '^SUB +<DIR>' 1 "/-W anula DIRCMD"
has  "DIR /X" '^Parámetro no válido - /X$' 1 "opción no válida"
hasnt "DIR /X" 'Directorio de' 1 "y no lista"
has  "$P /C /CH" '^        8 archivo\(s\)' 1 "/C y /CH aceptadas"
has  "DIR A B" '^Demasiados parámetros - B$' 1 "demasiados parámetros"
order 'DIR A:\PRUEBA /O:N /P' "ruta, /O:N y /P" '^A +TXT' '^BB' '^ZETA' 'archivo\(s\)'
has  'DIR \MUCHOS /P' '^Pulse una tecla para continuar \. \. \.' 1 "pausa"
has  'DIR \MUCHOS /P' '^\(continuando A:\\MUCHOS\)$' 1 "continúa"
has  'DIR \MUCHOS /P' '^       32 archivo\(s\)' 1 "y termina"
if fsck.fat -n $T/f.img >/dev/null 2>&1; then ok "fsck.fat"; else ko "fsck.fat"; fi
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log.txt; fi
rm -rf $T
exit $fail
