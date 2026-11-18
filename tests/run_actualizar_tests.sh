#!/bin/bash
# Prueba de ACTUALIZAR (docs/RED.md): busca por HTTP si hay una versión más
# nueva o una edición más rica del sistema. Arranca Hispa-DOS en QEMU con la red
# de usuario y un servidor HTTP propio (python3 -m http.server en 127.0.0.1, que
# el invitado ve como 10.0.2.2), que sirve ficheros versiones.txt. Comprueba:
#   (c) SIN HSPRED a mano: ACTUALIZAR carga HSPRED solo (hay tarjeta) y, tras
#       hacer DHCP, consulta el servidor; NO pide «Cargue HSPRED».
#   (a) [0.9.26] (igual que la instalada, sin más rica): «está al día».
#   (b) [0.9.99] (más nueva) con nota y descarga (sin paquete): «Hay una versión
#       nueva: 0.9.99», la nota y la dirección de descarga.
#   (e) recomendación por EDICIÓN: a igualdad de versión pero con nivel más alto
#       (edición más rica) y requisitos que el equipo cumple, RECOMIENDA pasarse
#       a esa edición y pregunta; contestamos N y queda en «solo mirar».
#   (f) límite por hardware: una edición que pide 586 y mucha RAM no se puede;
#       dice hasta dónde llega el equipo y por qué la otra no.
#   (g) [HISPA-486] las gamas: una sección con «gama=486» no es para este
#       Hispa-386 (ni se ofrece ni se avisa); la de su gama, sí.
#   (d) la dirección tomada de la clave ACTUALIZAR= de \HISPADOS\RED.INI.
#   (+) una dirección https:// se rechaza con su mensaje.
# Todo en un espacio de nombres de red propio (unshare -rn): no usa Internet.
# ACTUALIZAR (net installer) test, with QEMU user networking + own HTTP server.
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
# --- los ficheros versiones.txt que sirve el servidor HTTP (en CP437, como un
#     servidor real de Hispa-DOS, para que los acentos cuadren) ---
printf '[0.9.26]\ncpu=386\n'                                        > $T/www/aldia.txt
{ printf '; lista de versiones de prueba\n\n'
  printf '[0.9.99]\n'
  printf 'nota=Batería en la barra y consola remota\n'
  printf 'descarga=http://ejemplo/hispados/\n'; } | iconv -f UTF-8 -t CP437 > $T/www/nueva.txt
{ printf '[0.9.26]\n'
  printf 'cpu=386\n'
  printf 'disco=0\n'
  printf 'nivel=1\n'
  printf 'edicion=Hispa-DOS con Hispa Ventanas (grafico)\n'
  printf 'paquete=http://10.0.2.2:%s/noexiste.paq\n' "$PORT"
  printf 'nota=Da el salto al entorno grafico\n'; } | iconv -f UTF-8 -t CP437 > $T/www/recom.txt
{ printf '[0.9.26]\n'
  printf 'cpu=386\n'
  printf 'nivel=0\n'
  printf 'edicion=Hispa-DOS con Hispa Ventanas\n\n'
  printf '[1.0.0]\n'
  printf 'cpu=586\n'
  printf 'ram=2000000\n'
  printf 'nivel=2\n'
  printf 'edicion=Edicion Total\n'
  printf 'nota=Nueva edicion grafica superior\n'; } | iconv -f UTF-8 -t CP437 > $T/www/limite.txt
{ printf '[0.9.98]\n'
  printf 'gama=486\n'
  printf 'cpu=386\n'
  printf 'nota=Solo para Hispa-486\n\n'
  printf '[0.9.97]\n'
  printf 'cpu=386\n'
  printf 'nota=La de Hispa-386\n'; } | iconv -f UTF-8 -t CP437 > $T/www/gamas.txt
{ printf '[0.9.50]\n'
  printf 'nota=Esta vino de RED.INI\n'; } | iconv -f UTF-8 -t CP437  > $T/www/redini.txt
python3 -m http.server --bind 127.0.0.1 --directory $T/www $PORT > $T/http.log 2>&1 &
HTTP_PID=$!
sleep 1
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

CMDS=("8|ACTUALIZAR $U/aldia.txt"                    # (c) sin HSPRED a mano -> lo carga solo
      '3|HSPRED' '4|CONFRED'
      "7|ACTUALIZAR $U/aldia.txt"                    # (a) al día
      "7|ACTUALIZAR $U/nueva.txt"                    # (b) versión nueva (avisa)
      "7|ACTUALIZAR $U/recom.txt" '2|N'              # (e) recomienda edición más rica; N
      "7|ACTUALIZAR $U/limite.txt"                   # (f) límite por hardware
      "7|ACTUALIZAR $U/gamas.txt"                    # (g) las gamas
      '3|ACTUALIZAR https://ejemplo.org/x.txt'       # (+) https:// rechazado
      '2|DEL A:\HISPADOS\RED.INI'
      '2|COPY A:\REDACT.INI A:\HISPADOS\RED.INI'
      "7|ACTUALIZAR"                                 # (d) URL de RED.INI
      '2|ECHO fin-de-la-prueba')

echo "== Hispa-DOS ACTUALIZAR: $NIC =="
corre f 150 "${CMDS[@]}"

# (c) sin HSPRED a mano: ACTUALIZAR lo carga solo y NO pide cargarlo
check f "Cargando el controlador de red (HSPRED)"
if grep -qF -- "Cargue HSPRED y configure la red (CONFRED) antes de buscar" $T/f.txt; then
  ko "no debe pedir «Cargue HSPRED» (hay tarjeta: lo carga solo)"
else
  ok "no pide «Cargue HSPRED» (lo carga solo)"
fi
# (a) al día
check f "Hispa-DOS 0.9.26 está al día."
# (b) versión nueva, con nota y descarga
check f "Hay una versión nueva: 0.9.99"
check f "Batería en la barra y consola remota"
check f "Descárgala en: http://ejemplo/hispados/"
# (e) recomendación por edición más rica; contestamos N
check f "Tu equipo admite la edición"
check f "Hispa-DOS con Hispa Ventanas (grafico)"
check f "¿Descargar e instalar la 0.9.26?"
check f "Da el salto al entorno grafico"
check f "De acuerdo: solo mirar"
# (f) límite por hardware
check f "Tu equipo llega hasta la 0.9.26"
check f "Edicion Total"
check f "Pentium o superior"
# (g) las gamas: la de Hispa-486 no se mira; la de Hispa-386, sí
check f "Hay una versión nueva: 0.9.97"
check f "La de Hispa-386"
if grep -qF -- "Solo para Hispa-486" $T/f.txt || grep -qF -- "0.9.98" $T/f.txt; then
  ko "una sección de gama=486 no es para Hispa-386"; else ok "la sección de gama=486 no se mira en Hispa-386"; fi
if grep -qF -- "(Hispa-486)" $T/f.txt; then ko "Hispa-386 dice que es Hispa-486"; else ok "lo instalado no dice «(Hispa-486)»"; fi
# (+) https:// rechazado
check f "Las direcciones https:// (cifradas) todavía no funcionan"
# (d) la URL tomada de RED.INI
check f "Hay una versión nueva: 0.9.50"
check f "Esta vino de RED.INI"
check f "fin-de-la-prueba"

if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/>ACTUALIZAR$/,$p' $T/f.txt | head -160; fi
exit $fail
