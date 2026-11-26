#!/bin/bash
# LA TRANSICIÓN a la 0.9.31 (docs/RED.md, «La transición: de la 0.9.30 a la
# 0.9.31»): un Hispa-DOS 0.9.30 de verdad (la imagen USB publicada), con su
# ACTUALIZAR de antes (sin firma), recibe por la red la lista de la 0.9.31
# tal como irá al servidor (las dos secciones, la de 386 primero, con
# sha256=, octetos= y la firma al final) y el paquete de la 0.9.31
# (build/hispados.paq):
#   1) se instala la 0.9.30 en un disco duro vacío (PARTIR + INSTALAR desde
#      su memoria USB);
#   2) su ACTUALIZAR entiende la lista firmada (las claves nuevas no le
#      estorban), ofrece la 0.9.31 de Hispa-386 y la instala;
#   3) en el disco quedan el ACTUALIZ.COM, el KERNEL.SYS y el COMMAND.COM de
#      la 0.9.31 (octeto a octeto los de build/), y se conservan los *.INI;
#   4) arrancando ya la 0.9.31, su ACTUALIZAR NO cree la misma lista firmada
#      con la clave de PRUEBA (lleva compilada la de verdad): «La firma de
#      la lista de versiones NO es buena», y no instala nada.
#
#   uso: tests/run_transicion_0931_tests.sh [imagen USB de la 0.9.30]
#   (por omisión, la de /root/.claude/jobs/rel930/00-usb-hispa386.zip; con
#   VIEJA=ruta.zip o .img, otra: con la de Hispa-486, rel930/00-usb-hispa486.zip,
#   su ACTUALIZAR toma la sección gama=486). Ocupa ~800 MB en $TMPDIR mientras dura.
# Upgrade path test: a published 0.9.30 (unsigned updater) gets 0.9.31.
set -u
cd "$(dirname "$0")/.."
VIEJA=${1:-${VIEJA:-/root/.claude/jobs/rel930/00-usb-hispa386.zip}}
NIC=${NIC:-ne2k_pci}
[ -f "$VIEJA" ] || { echo "  se salta: falta la imagen de la 0.9.30 ($VIEJA; VIEJA=)"; exit 0; }
for f in build/hispados.paq build/ACTUALIZAR.COM build/KERNEL.SYS build/COMMAND.COM; do
  [ -f $f ] || { echo "falta $f (make paq)"; exit 1; }
done
if [ -z "${ACT_NS:-}" ] && unshare -rn true 2>/dev/null; then
  ACT_NS=1 exec unshare -rn "$0" "$@"
fi
if [ -n "${ACT_NS:-}" ]; then ip link set lo up 2>/dev/null; fi

T=$(mktemp -d)
HTTP_PID=
trap '[ -n "$HTTP_PID" ] && kill $HTTP_PID 2>/dev/null; [ -n "${GUARDAR:-}" ] && cp $T/l*.txt "$GUARDAR"/ 2>/dev/null; rm -rf $T' EXIT
export MTOOLS_SKIP_CHECK=1
OFF=32256
PORT=${PORT:-8097}
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() { if grep -qF -- "$2" "$1"; then ok "$2"; else ko "$2"; fi; }
checkn() { if grep -qF -- "$2" "$1"; then ko "(no debe salir) $2"; else ok "no sale: $2"; fi; }
send() { sleep "${2:-1.5}"; printf '%s\r' "$1"; }
key() { sleep "${2:-1}"; printf '%s' "$1"; }
txt() { tr -d '\r' < $1 | iconv -f CP437 -t UTF-8 -c > $1.txt; }
part() { awk -v a="$2" -v b="$3" 'index($0, a) {p=1; next} p && index($0, b) {exit} p' "$1"; }
HDD=$T/hdd.img
mdos() { mtype -i $HDD@@$OFF "$@" 2>/dev/null; }
igual() {   # igual RUTA_EN_C ARCHIVO_DE_BUILD
  rm -f $T/sale; mcopy -i $HDD@@$OFF "::$1" $T/sale 2>/dev/null
  if [ -f $T/sale ] && cmp -s $T/sale "$2"; then ok "C:\\$1 es el de la 0.9.31"; else ko "C:\\$1 no es el de la 0.9.31 ($2)"; fi
}

# Hispa-486 arranca el escritorio (y el Wi-Fi) desde AUTOEXEC.BAT: para
# teclear órdenes, esas líneas a REM (en la memoria y luego en el disco)
sin_escritorio() {
  mtype -i "$1" ::AUTOEXEC.BAT > $T/aut.bat 2>/dev/null || return 0
  sed -i -E 's/^([^R].*\\HISPADOS\\(VENTANAS|HSPWIFI \/AUTO|WIFI \/AUTO)\r?)$/REM \1/' $T/aut.bat
  mcopy -o -i "$1" $T/aut.bat ::AUTOEXEC.BAT
}
case "$VIEJA" in
  *.zip) unzip -q -o "$VIEJA" -d $T/z && mv $T/z/*.img $T/usb.img && rm -rf $T/z ;;
  *) cp "$VIEJA" $T/usb.img ;;
esac
[ -f $T/usb.img ] || { echo "no hay imagen en $VIEJA"; exit 1; }
mtype -i $T/usb.img@@1048576 ::HISPADOS/VERSION.TXT 2>/dev/null | head -2
tests/tools/sinmenu.sh $T/usb.img || exit 1
sin_escritorio $T/usb.img@@1048576
command dd if=/dev/zero of=$HDD bs=1M count=200 status=none

echo "== La transición: un Hispa-DOS 0.9.30 recibe la 0.9.31 firmada =="
# ------------------------------------------------------------------ 1
{
  sleep 8
  send "PARTIR" 1
  key "1" 3; key "1" 1; send "100" 2; key $'\e' 3
  key "2" 1; send "1" 2; key $'\e' 3; key $'\e' 1
  send "ECHO =INST=" 2
  send "INSTALAR" 1
  send "S" 5
  sleep 30
  send "ECHO =FIN1=" 1
} | timeout 160 qemu-system-i386 -vga std -display none -serial stdio -monitor none -no-reboot \
      -drive file=$T/usb.img,format=raw,if=ide,index=0 \
      -drive file=$HDD,format=raw,if=ide,index=1 -boot c > $T/l1 2>&1
txt $T/l1
rm -f $T/usb.img
part $T/l1.txt "=INST=" "=FIN1=" > $T/s1
check $T/s1 "Hispa-DOS instalado en la unidad C:"
rm -f $T/viejo.com; mcopy -i $HDD@@$OFF ::HISPADOS/ACTUALIZ.COM $T/viejo.com 2>/dev/null
if [ -f $T/viejo.com ] && ! cmp -s $T/viejo.com build/ACTUALIZAR.COM; then ok "el ACTUALIZ.COM instalado es el de antes (no el de la 0.9.31)"; else ko "no hay un ACTUALIZ.COM viejo en el disco"; fi

# ------------------------------------------------------------------ 2
printf 'DHCP=SI\r\nACTUALIZAR=http://10.0.2.2:%s/versiones.txt\r\n' "$PORT" > $T/RED.INI
printf 'TEMA=OSCURO\r\n' > $T/VENTANAS.INI
mcopy -o -i $HDD@@$OFF $T/RED.INI ::HISPADOS/RED.INI
mcopy -o -i $HDD@@$OFF $T/VENTANAS.INI ::HISPADOS/VENTANAS.INI
sin_escritorio $HDD@@$OFF
VER=$(sed -n 's/^%define VER_STR *"\(.*\)".*/\1/p' lib/version.inc | head -1)
[ -n "$VER" ] || VER=0.9.31
mkdir -p $T/www
cp build/hispados.paq $T/www/hispados-$VER.paq
[ -f build/hispa486.paq ] && cp build/hispa486.paq $T/www/hispa486-$VER.paq
{ printf '; Hispa-DOS - versiones publicadas (lo lee ACTUALIZAR por HTTP).\n'
  printf '[%s]\ncpu=386\nram=0\ndisco=6\nnivel=0\n' $VER
  printf 'edicion=Hispa-386 (con Hispa Ventanas)\n'
  printf 'paquete=http://10.0.2.2:%s/hispados-%s.paq\n' $PORT $VER
  printf 'nota=Novedades %s: actualizaciones firmadas.\n' $VER
  if [ -f build/hispa486.paq ]; then
    printf '[%s]\ngama=486\ncpu=386\nram=0\ndisco=8\nnivel=0\n' $VER
    printf 'edicion=Hispa-486 (escritorio con menu Inicio)\n'
    printf 'paquete=http://10.0.2.2:%s/hispa486-%s.paq\n' $PORT $VER
    printf 'nota=Hispa-486 %s: actualizaciones firmadas.\n' $VER
  fi; } > $T/www/versiones.txt
python3 tools/firma_versiones.py firma $T/www/versiones.txt --clave tests/firma/clave_prueba.key > /dev/null \
  || { echo "no se pudo firmar"; exit 1; }
grep -q '^sha256=' $T/www/versiones.txt && grep -q '^firma=ed25519 ' $T/www/versiones.txt \
  && ok "la lista lleva sha256= y la firma ($(stat -c %s $T/www/versiones.txt) octetos)" || ko "la lista no lleva sha256= o la firma"
python3 -m http.server --bind 127.0.0.1 --directory $T/www $PORT > $T/http.log 2>&1 &
HTTP_PID=$!
sleep 1

echo "-- 2) el ACTUALIZAR de la 0.9.30, con la lista firmada --"
{
  sleep 10
  send "ECHO =ACT=" 2
  send "ACTUALIZAR" 15
  send "S" 2
  sleep ${ESPERA_PAQ:-150}
  send "ECHO =FIN2=" 2
} | timeout $(( ${ESPERA_PAQ:-150} + 60 )) qemu-system-i386 -vga std -display none -serial stdio -monitor none -no-reboot \
      -drive file=$HDD,format=raw,if=ide -boot c \
      -netdev user,id=n0 -device $NIC,netdev=n0 > $T/l2 2>&1
txt $T/l2
part $T/l2.txt "=ACT=" "=FIN2=" > $T/s2
case "$VIEJA" in   # cada gama, su sección
  *486*) check $T/s2 "Hispa-486 $VER: actualizaciones firmadas." ;;
  *) check $T/s2 "Novedades $VER: actualizaciones firmadas." ;;
esac
check $T/s2 "Descargando el paquete"
check $T/s2 "Actualizado a $VER"
checkn $T/s2 "no es válido"

echo "-- 3) lo que queda en el disco --"
igual HISPADOS/ACTUALIZ.COM build/ACTUALIZAR.COM
igual KERNEL.SYS build/KERNEL.SYS
igual COMMAND.COM build/COMMAND.COM
mdos ::HISPADOS/VENTANAS.INI > $T/vini.txt
check $T/vini.txt "TEMA=OSCURO"
mdos ::HISPADOS/RED.INI > $T/rini.txt
check $T/rini.txt "ACTUALIZAR=http://10.0.2.2"
if mdos ::ACTUALIZA.PAQ | grep -q .; then ko "quedó C:\\ACTUALIZA.PAQ"; else ok "C:\\ACTUALIZA.PAQ borrado"; fi

echo "-- 4) ya en la 0.9.31: la lista firmada con la clave de prueba no vale --"
sin_escritorio $HDD@@$OFF
{
  sleep 12
  send "ECHO =VER=" 2
  send "VER" 2
  send "ECHO =ACT2=" 2
  send "ACTUALIZAR" 20
  send "S" 4
  send "ECHO =FIN4=" 2
} | timeout 90 qemu-system-i386 -vga std -display none -serial stdio -monitor none -no-reboot \
      -drive file=$HDD,format=raw,if=ide -boot c \
      -netdev user,id=n0 -device $NIC,netdev=n0 > $T/l4 2>&1
txt $T/l4
part $T/l4.txt "=VER=" "=ACT2=" > $T/s4v
part $T/l4.txt "=ACT2=" "=FIN4=" > $T/s4
check $T/s4v "$VER"
check $T/s4 "La firma de la lista de versiones NO es buena"
check $T/s4 "No se instala nada."
checkn $T/s4 "Descargando el paquete"

if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then
  for f in l1 l2 l4; do echo "--- $f ---"; cat $T/$f.txt; done
fi
exit $fail
