#!/bin/bash
# Prueba de CONSOLA (docs/CONSOLA.md): la consola remota por red, abierta y
# con código de acceso. Arranca Hispa-DOS en QEMU con HSPRED y la red de
# usuario de QEMU, reenviando el puerto 2323 del invitado a un puerto local
#   -netdev user,hostfwd=tcp:127.0.0.1:<puerto>-:2323
# y, desde tests/consola_cliente.py, comprueba:
#   - CONSOLA escucha y, al conectar, pide un código (no entra sin él).
#   - El código que enseña en la pantalla del equipo (que sale también por el
#     puerto serie, como toda su consola) deja entrar: ejecuta VER y devuelve
#     su salida; SALIR cierra la sesión.
#   - Una 2ª conexión genera OTRO código; un código erróneo se rechaza y corta.
#   - Esc en el teclado del equipo cierra CONSOLA y suelta el puerto.
# Todo dentro de un espacio de nombres de red propio (unshare -rn): no hace
# falta Internet ni se toca la red del equipo.
# CONSOLA (remote console) test, with QEMU user networking + hostfwd.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
NIC=${NIC:-ne2k_pci}
[ -f build/CONSOLA.COM ] || { echo "falta build/CONSOLA.COM (make)"; exit 1; }
[ -f "$IMG" ] || { echo "falta la imagen $IMG (make)"; exit 1; }

# un espacio de nombres de red propio (como run_red_tests.sh)
if [ -z "${CONSOLA_NS:-}" ] && unshare -rn true 2>/dev/null; then
  CONSOLA_NS=1 exec unshare -rn "$0" "$@"
fi
if [ -n "${CONSOLA_NS:-}" ]; then
  ip link set lo up 2>/dev/null
fi

T=$(mktemp -d)
trap 'rm -rf $T' EXIT
cp "$IMG" $T/f.img
PORT=${PORT:-12323}

echo "== Hispa-DOS CONSOLA: $NIC =="
python3 tests/consola_cliente.py $T/f.img "$NIC" "$PORT"
exit $?
