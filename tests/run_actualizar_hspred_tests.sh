#!/bin/bash
# Prueba de ACTUALIZAR cargando HSPRED solo (como Windows al conectarse).
# Si HSPRED no está cargado, ACTUALIZAR lo ejecuta (\HISPADOS\HSPRED.COM de la
# unidad de arranque) y reintenta. Dos escenarios, en sendos QEMU:
#   (1) CON tarjeta de red: SIN cargar HSPRED a mano, ACTUALIZAR lo carga solo,
#       hace DHCP y consulta el servidor local -> «está al día», y NO aparece el
#       aviso «Cargue HSPRED».
#   (2) SIN tarjeta de red: ACTUALIZAR intenta cargar HSPRED, pero como no hay
#       tarjeta HSPRED no se queda; entonces sí sale el aviso de que no hay red.
# Todo en un espacio de nombres de red propio (unshare -rn): no toca la red real.
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
printf '[0.9.29]\ncpu=386\n' > $T/www/aldia.txt
python3 -m http.server --bind 127.0.0.1 --directory $T/www $PORT > $T/http.log 2>&1 &
HTTP_PID=$!
sleep 1
trap 'kill $HTTP_PID 2>/dev/null; [ -n "${GUARDAR:-}" ] && cp $T/*.txt "$GUARDAR"/ 2>/dev/null; rm -rf $T' EXIT

U="http://10.0.2.2:$PORT"

# corre NOMBRE TIEMPO RED «s|orden»...  (RED = 1 con tarjeta, 0 sin tarjeta)
corre() {
  local n=$1 tmo=$2 red=$3; shift 3
  local netargs=()
  if [ "$red" = 1 ]; then
    netargs=(-netdev user,id=n0 -device $NIC,netdev=n0)
  else
    netargs=(-net none)     # sin tarjeta: QEMU, si no, añade una e1000 por omisión
  fi
  cp "$IMG" $T/$n.img
  { sleep 6
    for c in "$@"; do printf '%s\r' "${c#*|}"; sleep "${c%%|*}"; done
  } | timeout $tmo qemu-system-i386 -display none -serial stdio -monitor none -parallel none \
        -m 32 -drive file=$T/$n.img,format=raw,if=floppy -boot a -no-reboot \
        "${netargs[@]}" > $T/$n.raw 2>&1
  tr -d '\r' < $T/$n.raw | iconv -f CP437 -t UTF-8 -c > $T/$n.txt
}

fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check()  { if grep -qF -- "$2" $T/$1.txt; then ok "$2"; else ko "$2"; fi; }
nocheck(){ if grep -qF -- "$2" $T/$1.txt; then ko "NO debía aparecer: $2"; else ok "ausente (bien): $2"; fi; }

# (1) con tarjeta: ACTUALIZAR carga HSPRED solo y consulta el servidor
echo "== ACTUALIZAR carga HSPRED solo (con tarjeta: $NIC) =="
corre con 120 1 "9|ACTUALIZAR $U/aldia.txt" '2|ECHO fin-con'
check   con "Cargando el controlador de red (HSPRED)"
check   con "Hispa-DOS 0.9.29 está al día."
nocheck con "Cargue HSPRED y configure la red (CONFRED) antes de buscar"
check   con "fin-con"

# (2) sin tarjeta: tras intentar cargar HSPRED, el aviso de siempre
echo "== ACTUALIZAR sin tarjeta de red: avisa =="
corre sin 90 0 "9|ACTUALIZAR $U/aldia.txt" '2|ECHO fin-sin'
check sin "Cargando el controlador de red (HSPRED)"
check sin "Cargue HSPRED y configure la red (CONFRED) antes de buscar"
check sin "fin-sin"

if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then
  echo "--- con ---"; sed -n '/>ACTUALIZAR/,$p' $T/con.txt | head -80
  echo "--- sin ---"; sed -n '/>ACTUALIZAR/,$p' $T/sin.txt | head -80
fi
exit $fail
