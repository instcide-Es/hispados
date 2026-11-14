#!/bin/bash
# Prueba de AYUDA: lista, «AYUDA orden», «orden /?» (internas, pegado y en
# programas externos por ALIAS.INI).
# HELP test.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
cp "$IMG" $T/f.img
CMDS=("AYUDA" "AYUDA DIR" "copy /?" "cd/?" "AYUDA ETIQUETA" "AYUDA XCOPY" "REVISAR /?" "AYUDA SHIFT" "Versi\xa2n" "/ver" "/?" "kernel.sys" "readme.txt" "ECHO fin")
{ sleep 6; for c in "${CMDS[@]}"; do printf '%b\r' "$c"; sleep 2; done; } | \
  timeout 60 qemu-system-i386 -display none -serial stdio -monitor none -parallel none \
  -drive file=$T/f.img,format=raw,if=floppy -boot a -no-reboot > $T/log 2>&1
tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 > $T/log.txt
fail=0
check() { if grep -qF -- "$1" $T/log.txt; then echo "  OK   $1"; else echo "  FAIL $1"; fail=1; fi; }
echo "== Hispa-DOS AYUDA =="
check "Lista de órdenes internas"
check "AYUDA orden (u «orden /?») explica cada orden."
check "LISTAR (DIR) [unidad:][ruta][archivo] [/P] [/W]"
check "COPIAR (COPY) origen [destino]"
check "CD ..   sube a la carpeta anterior"
check "ETIQUETA [unidad:][etiqueta]"
check "Uso: XCOPIAR origen [destino]"
check "Desplaza los parámetros de un archivo por lotes"
check "Tab: completar nombres de archivos y carpetas"
check "solo salen programas:"
n=$(grep -c "Hispa-DOS versión 0.9 (compatible con MS-DOS 6.22)" $T/log.txt)
if [ "$n" -ge 2 ]; then echo "  OK   Versión (con tilde) y /ver"; else echo "  FAIL Versión y /ver ($n)"; fail=1; fi
n=$(grep -c "Lista de órdenes internas" $T/log.txt)
if [ "$n" -ge 2 ]; then echo "  OK   /? solo muestra la ayuda"; else echo "  FAIL /? solo ($n)"; fail=1; fi
n=$(grep -c "Comando o nombre de archivo incorrecto" $T/log.txt)
if [ "$n" -ge 2 ] && [ "$(grep -c 'núcleo compatible con MS-DOS' $T/log.txt)" -le 1 ]; then echo "  OK   kernel.sys y readme.txt no se ejecutan"; else echo "  FAIL kernel.sys o readme.txt se ejecutaron ($n)"; fail=1; fi
check "fin"
if grep -qF "Parámetro no válido" $T/log.txt; then echo "  FAIL algún /? dio «Parámetro no válido»"; fail=1; else echo "  OK   ningún «Parámetro no válido»"; fi
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log.txt; fi
rm -rf $T
exit $fail
