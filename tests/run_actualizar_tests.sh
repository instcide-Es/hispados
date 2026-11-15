#!/bin/bash
# Prueba de ACTUALIZAR (docs/RED.md): busca por HTTP si hay una versión más
# nueva del sistema. Arranca Hispa-DOS en QEMU con la red de usuario y un
# servidor HTTP propio (python3 -m http.server en 127.0.0.1, que el invitado
# ve como 10.0.2.2), y comprueba:
#   (c) SIN red (antes de HSPRED): ACTUALIZAR avisa de que cargue HSPRED.
#   (a) version=0.9.12 (igual que la instalada): «está al día».
#   (b) version=0.9.99 (mayor) con nota y descarga: «Hay una versión nueva:
#       0.9.99», la nota y la dirección de descarga.
#   (d) la dirección tomada de la clave ACTUALIZAR= de \HISPADOS\RED.INI
#       (otra versión y nota, para distinguirla de (b)).
#   (+) una dirección https:// se rechaza con su mensaje.
# Todo en un espacio de nombres de red propio (unshare -rn): no hace falta
# Internet ni se toca la red del equipo. NO se usa la red de verdad.
# ACTUALIZAR (update checker) test, with QEMU user networking + own HTTP server.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
B=build
NIC=${NIC:-ne2k_pci}
for f in HSPRED CONFRED ACTUALIZAR; do
  [ -f $B/$f.COM ] || { echo "falta $B/$f.COM (make)"; exit 1; }
done
[ -f "$IMG" ] || { echo "falta la imagen $IMG (make)"; exit 1; }

if [ -z "${ACT_NS:-}" ] && unshare -rn true 2>/dev/null; then
  ACT_NS=1 exec unshare -rn "$0" "$@"
fi
if [ -n "${ACT_NS:-}" ]; then
  ip link set lo up 2>/dev/null
fi

T=$(mktemp -d)
PORT=${PORT:-8080}
mkdir -p $T/www
# los archivos de versión que sirve el servidor HTTP
printf 'version=0.9.12\n'                                           > $T/www/aldia.txt
# el fichero en CP437 (como lo serviría un servidor real de Hispa-DOS): así
# los acentos de la nota cuadran al pasar la salida de CP437 a UTF-8
{ printf '; archivo de version de prueba\n\n'
  printf 'version=0.9.99\n'
  printf 'descarga=http://ejemplo/hispados/\n'
  printf 'nota=Batería en la barra y consola remota\n'; } | iconv -f UTF-8 -t CP437 > $T/www/nueva.txt
{ printf 'version=0.9.50\n'
  printf 'nota=Esta vino de RED.INI\n'; } | iconv -f UTF-8 -t CP437  > $T/www/redini.txt
python3 -m http.server --bind 127.0.0.1 --directory $T/www $PORT > $T/http.log 2>&1 &
HTTP_PID=$!
sleep 1
# (GUARDAR=carpeta: deja allí los registros de QEMU)
trap 'kill $HTTP_PID 2>/dev/null; [ -n "${GUARDAR:-}" ] && cp $T/*.txt "$GUARDAR"/ 2>/dev/null; rm -rf $T' EXIT

U="http://10.0.2.2:$PORT"
# RED.INI con la clave ACTUALIZAR= apuntando a nuestro servidor (prueba d)
printf 'DHCP=SI\r\nACTUALIZAR=%s/redini.txt\r\n' "$U" > $T/REDACT.INI

cp "$IMG" $T/f.img
mcopy -o -i $T/f.img $T/REDACT.INI ::REDACT.INI

# corre NOMBRE TIEMPO «s|orden»...
corre() {
  local n=$1 tmo=$2; shift 2
  { sleep 6
    for c in "$@"; do printf '%s\r' "${c#*|}"; sleep "${c%%|*}"; done
  } | timeout $tmo qemu-system-i386 -display none -serial stdio -monitor none -parallel none \
        -m 32 -drive file=$T/$n.img,format=raw,if=floppy -boot a -no-reboot \
        -netdev user,id=n0 -device $NIC,netdev=n0 > $T/$n.raw 2>&1
  tr -d '\r' < $T/$n.raw | iconv -f CP437 -t UTF-8 -c > $T/$n.txt
}

fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() { if grep -qF -- "$2" $T/$1.txt; then ok "$2"; else ko "$2"; fi; }

CMDS=('4|ACTUALIZAR'                                 # (c) sin HSPRED -> avisa
      '3|HSPRED' '4|CONFRED'
      "7|ACTUALIZAR $U/aldia.txt"                    # (a) al día
      "7|ACTUALIZAR $U/nueva.txt"                    # (b) versión nueva
      '3|ACTUALIZAR https://ejemplo.org/x.txt'       # (+) https:// rechazado
      '2|DEL A:\HISPADOS\RED.INI'
      '2|COPY A:\REDACT.INI A:\HISPADOS\RED.INI'
      "7|ACTUALIZAR"                                 # (d) URL de RED.INI
      '2|ECHO fin-de-la-prueba')

echo "== Hispa-DOS ACTUALIZAR: $NIC =="
corre f 120 "${CMDS[@]}"

# (c) sin HSPRED
check f "Cargue HSPRED y configure la red (CONFRED) antes de buscar"
# (a) al día
check f "Hispa-DOS 0.9.12 está al día."
# (b) versión nueva, con nota y descarga
check f "Hay una versión nueva: 0.9.99"
check f "Batería en la barra y consola remota"
check f "Descárgala en: http://ejemplo/hispados/"
# (+) https:// rechazado
check f "Las direcciones https:// (cifradas) todavía no funcionan"
# (d) la URL tomada de RED.INI (otra versión y nota, para distinguirla de (b))
check f "Hay una versión nueva: 0.9.50"
check f "Esta vino de RED.INI"
check f "fin-de-la-prueba"

if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/>ACTUALIZAR$/,$p' $T/f.txt | head -120; fi
exit $fail
