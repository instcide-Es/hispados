#!/bin/bash
# Prueba de AYUDA: el índice de temas, «AYUDA tema» (sin mirar mayúsculas),
# «AYUDA TODO» con su pausa por pantalla (y sin pausa si va a un archivo),
# «AYUDA orden», «orden /?» (internas, pegado y en programas externos por ALIAS.INI).
# HELP test.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
cp "$IMG" $T/f.img
CMDS=("AYUDA" "AYUDA archivos" "AYUDA Red" "AYUDA TECLAS" "AYUDA TODO" " " " " " " " " "AYUDA TODO > T.TXT" "MOSTRAR T.TXT" "AYUDA DIR" "copy /?" "cd/?" "AYUDA ETIQUETA" "AYUDA XCOPY" "REVISAR /?" "AYUDA SHIFT" "Versi\xa2n" "/ver" "/?" "kernel.sys" "readme.txt" "AYUDA IPCONFIG" "DESCARGAR /?" "PING /?" "ECHO fin")
{ sleep 6; for c in "${CMDS[@]}"; do printf '%b\r' "$c"; sleep 2; done; } | \
  timeout 60 qemu-system-i386 -display none -serial stdio -monitor none -parallel none \
  -drive file=$T/f.img,format=raw,if=floppy -boot a -no-reboot > $T/log 2>&1
tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 > $T/log.txt
fail=0
check() { if grep -qF -- "$1" $T/log.txt; then echo "  OK   $1"; else echo "  FAIL $1"; fail=1; fi; }
echo "== Hispa-DOS AYUDA =="
check "AYUDA de Hispa-DOS. Escriba AYUDA y un tema:"
check "  AYUDA TODO        todo seguido, parando en cada pantalla"
check "O AYUDA y una orden, por ejemplo AYUDA COPIAR (lo mismo que COPIAR /?)."
check "  BORRARARBOL / DELTREE   borra una carpeta con todo lo que tiene"
check "  CONFRED / IPCONFIG      ver y cambiar la dirección de red"
check "TECLAS (al escribir órdenes)"
check "LISTAR (DIR) [unidad:][ruta][archivo] [/P] [/W]"
check "COPIAR (COPY) origen [destino]"
check "CD ..   sube a la carpeta anterior"
check "ETIQUETA [unidad:][etiqueta]"
check "Uso: XCOPIAR origen [destino]"
check "Desplaza los parámetros de un archivo por lotes"
check "Tab: completar nombres de archivos y carpetas"
check "solo salen programas:"
check "CONFRED (IPCONFIG) [/RENOVAR | /LIBERAR]"
check "DESCARGAR [http://]servidor[:puerto]/ruta [archivo]"
check "PING nombre|dirección [/N:n] [/T]"
n=$(grep -c "Hispa-DOS versión 0.9.28 (compatible con MS-DOS 6.22)" $T/log.txt)
if [ "$n" -ge 2 ]; then echo "  OK   Versión (con tilde) y /ver"; else echo "  FAIL Versión y /ver ($n)"; fail=1; fi
n=$(grep -c "AYUDA de Hispa-DOS. Escriba AYUDA y un tema:" $T/log.txt)
if [ "$n" -ge 2 ]; then echo "  OK   /? solo muestra la ayuda"; else echo "  FAIL /? solo ($n)"; fail=1; fi
# AYUDA TODO en la pantalla: los siete temas y una pausa en cada pantalla (85
# líneas: 3 pausas); a un archivo, sin pausas (si parase, se comería las órdenes
# siguientes): los temas salen otra vez con MOSTRAR T.TXT
n=$(grep -c "Pulse una tecla para continuar" $T/log.txt)
if [ "$n" = 3 ]; then echo "  OK   AYUDA TODO para 3 veces en la pantalla"; else echo "  FAIL AYUDA TODO: $n pausas (eran 3)"; fail=1; fi
n=0; for t in "ARCHIVOS Y CARPETAS" "DISCOS (nombre" "EL ORDENADOR" "LA RED" "EL SONIDO" "ARCHIVOS POR LOTES" "TECLAS (al escribir"; do [ "$(grep -c "^$t" $T/log.txt)" -ge 2 ] && n=$((n+1)); done
if [ "$n" = 7 ]; then echo "  OK   AYUDA TODO: los 7 temas, en la pantalla y en T.TXT"; else echo "  FAIL AYUDA TODO: $n temas de 7"; fail=1; fi
n=$(grep -c "Comando o nombre de archivo incorrecto" $T/log.txt)
if [ "$n" -ge 2 ] && [ "$(grep -c 'núcleo compatible con MS-DOS' $T/log.txt)" -le 1 ]; then echo "  OK   kernel.sys y readme.txt no se ejecutan"; else echo "  FAIL kernel.sys o readme.txt se ejecutaron ($n)"; fail=1; fi
check "fin"
if grep -qF "Parámetro no válido" $T/log.txt; then echo "  FAIL algún /? dio «Parámetro no válido»"; fail=1; else echo "  OK   ningún «Parámetro no válido»"; fi
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log.txt; fi
rm -rf $T
exit $fail
