#!/bin/bash
# INSTALADOR POR RED de verdad: ACTUALIZAR baja un paquete por HTTP y lo instala
# EN EL SITIO, conservando los datos y ajustes del usuario.
#   1) Se instala Hispa-DOS en un disco duro vacío (PARTIR + INSTALAR desde el
#      USB), igual que run_actualizar_disco. Queda un disco arrancable.
#   2) Se "ensucia" el disco con datos y ajustes del usuario: un RED.INI propio
#      (DHCP=SI y ACTUALIZAR= apuntando a NUESTRO servidor), un VENTANAS.INI y se
#      pone un VENTANAS.COM "viejo" para ver que la actualización lo sustituye.
#   3) Se arranca SOLO el disco, con tarjeta de red, y se ejecuta ACTUALIZAR (sin
#      argumentos: usa la URL de RED.INI). El servidor sirve un versiones.txt con
#      una versión nueva (0.9.17) y su paquete .PAQ (generado con
#      tools/paquete.py). Se contesta que SÍ. ACTUALIZAR baja el paquete, lo
#      desempaqueta en C:\ACTUALIZA y lo aplica en el sitio.
#   4) Se comprueba, con mtools, que:
#      - un fichero del sistema cambió a la versión nueva (\HISPADOS\VENTANAS.COM
#        es el del paquete, ya NO el "viejo"),
#      - un fichero NUEVO del paquete está (\HISPADOS\NUEVO.TXT),
#      - un *.INI del usuario se CONSERVÓ (\HISPADOS\VENTANAS.INI),
#      - el CONFIG.SYS del usuario quedó como CONFIG.ANT,
#      - el temporal (C:\ACTUALIZA.PAQ y C:\ACTUALIZA) se borró.
# Net-installer end-to-end test (download + unpack + apply in place).
set -u
cd "$(dirname "$0")/.."
USBIMG=${1:-build/hispados-usb.img}
NIC=${NIC:-ne2k_pci}
[ -f "$USBIMG" ] || { echo "falta $USBIMG (make usb)"; exit 1; }
[ -f build/VENTANAS.COM ] || { echo "falta build/VENTANAS.COM (make)"; exit 1; }

# red propia: no toca la red de verdad (el invitado ve 127.0.0.1 como 10.0.2.2)
if [ -z "${ACT_NS:-}" ] && unshare -rn true 2>/dev/null; then
  ACT_NS=1 exec unshare -rn "$0" "$@"
fi
if [ -n "${ACT_NS:-}" ]; then ip link set lo up 2>/dev/null; fi

T=$(mktemp -d)
export MTOOLS_SKIP_CHECK=1
OFF=32256                 # la primaria empieza en el sector 63
PORT=${PORT:-8096}
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

cp "$USBIMG" $T/usb.img
tests/tools/sinmenu.sh $T/usb.img || exit 1
command dd if=/dev/zero of=$HDD bs=1M count=200 status=none

echo "== ACTUALIZAR instalador por red (bajar e instalar en el sitio) =="

# ------------------------------------------------------------------ 1
# instalación nueva en el disco duro (desde el USB)
{
  sleep 8
  send "PARTIR" 1
  key "1" 3             # crear...
  key "1" 1             # ...una primaria
  send "100" 2          # 100 MB
  key $'\e' 3
  key "2" 1             # activar
  send "1" 2
  key $'\e' 3
  key $'\e' 1           # salir
  send "ECHO =INST=" 2
  send "INSTALAR" 1
  send "S" 5            # dar formato e instalar
  sleep 28
  send "ECHO =FIN1=" 1
} | timeout 150 qemu-system-i386 -display none -serial stdio -monitor none -no-reboot \
      -drive file=$T/usb.img,format=raw,if=ide,index=0 \
      -drive file=$HDD,format=raw,if=ide,index=1 -boot c > $T/l1 2>&1
txt $T/l1
part $T/l1.txt "=INST=" "=FIN1=" > $T/s1
check $T/s1 "Hispa-DOS instalado en la unidad D:"
if mdos ::HISPADOS/VENTANAS.COM > /dev/null; then ok "el disco quedó con \\HISPADOS instalado"; else ko "no se instaló Hispa-DOS en el disco"; fi

# ------------------------------------------------------------------ 2
# datos y ajustes del usuario + VENTANAS.COM "viejo"
echo "-- 2) datos y ajustes del usuario --"
printf 'DHCP=SI\r\nACTUALIZAR=http://10.0.2.2:%s/versiones.txt\r\n' "$PORT" > $T/RED.INI
printf 'TEMA=OSCURO\r\nFONDO=MIFOTO.BMP\r\n' > $T/VENTANAS.INI
printf 'VIEJO-NO-VALIDO\r\n' > $T/VIEJO.BIN
mcopy -o -i $HDD@@$OFF $T/RED.INI ::HISPADOS/RED.INI
mcopy -o -i $HDD@@$OFF $T/VENTANAS.INI ::HISPADOS/VENTANAS.INI
mcopy -o -i $HDD@@$OFF $T/VIEJO.BIN ::HISPADOS/VENTANAS.COM
if mdos ::HISPADOS/VENTANAS.COM | grep -qF "VIEJO"; then ok "VENTANAS.COM puesto 'viejo'"; else ko "no se pudo poner el VENTANAS.COM viejo"; fi

# ------------------------------------------------------------------ 2b
# el servidor HTTP con versiones.txt y el paquete .PAQ (tools/paquete.py)
mkdir -p $T/www $T/paqsrc/HISPADOS
cp build/VENTANAS.COM $T/paqsrc/HISPADOS/VENTANAS.COM
printf 'VERSION-NUEVA-OK\r\n' > $T/paqsrc/HISPADOS/NUEVO.TXT
printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nDOS=HIGH\r\n' > $T/paqsrc/CONFIG.SYS
printf '@ECHO Hispa-DOS actualizado por red\r\n' > $T/paqsrc/AUTOEXEC.BAT
python3 -I tools/paquete.py $T/paqsrc $T/www/hispados-0.9.17.paq || { echo "paquete.py falló"; exit 1; }
{ printf '[0.9.17]\n'
  printf 'cpu=386\n'
  printf 'disco=1\n'
  printf 'nivel=0\n'
  printf 'edicion=Hispa-DOS con Hispa Ventanas\n'
  printf 'paquete=http://10.0.2.2:%s/hispados-0.9.17.paq\n' "$PORT"
  printf 'nota=Version de prueba por red\n'; } > $T/www/versiones.txt
python3 -m http.server --bind 127.0.0.1 --directory $T/www $PORT > $T/http.log 2>&1 &
HTTP_PID=$!
sleep 1
trap 'kill $HTTP_PID 2>/dev/null; [ -n "${GUARDAR:-}" ] && cp $T/l*.txt "$GUARDAR"/ 2>/dev/null; rm -rf $T' EXIT

# ------------------------------------------------------------------ 3
# arrancar el disco con red y ACTUALIZAR (sin argumentos: usa RED.INI), decir SÍ
echo "-- 3) ACTUALIZAR por red, contestando que SÍ --"
{
  sleep 10
  send "ECHO =ACT=" 2
  send "ACTUALIZAR" 11       # carga HSPRED, DHCP, baja versiones.txt, pregunta
  send "S" 20               # SÍ: baja el paquete, desempaqueta y aplica
  send "ECHO =FIN3=" 2
} | timeout 150 qemu-system-i386 -display none -serial stdio -monitor none -no-reboot \
      -drive file=$HDD,format=raw,if=ide -boot c \
      -netdev user,id=n0 -device $NIC,netdev=n0 > $T/l3 2>&1
txt $T/l3
part $T/l3.txt "=ACT=" "=FIN3=" > $T/s3
check $T/s3 "Tu equipo admite la edición"
check $T/s3 "Version de prueba por red"
check $T/s3 "Descargando el paquete"
check $T/s3 "Aplicando la actualización"
check $T/s3 "Actualizado a 0.9.17"

# ------------------------------------------------------------------ 4
echo "-- 4) comprobaciones en el disco --"
# (1) un fichero del sistema cambió al del paquete
mcopy -i $HDD@@$OFF ::HISPADOS/VENTANAS.COM $T/vent.out 2>/dev/null
if [ -f $T/vent.out ] && cmp -s $T/vent.out build/VENTANAS.COM; then ok "\\HISPADOS\\VENTANAS.COM es el del paquete (actualizado)"; else ko "VENTANAS.COM no coincide con el del paquete"; fi
if mdos ::HISPADOS/VENTANAS.COM | grep -qF "VIEJO"; then ko "VENTANAS.COM sigue siendo el viejo"; else ok "VENTANAS.COM ya no es el viejo"; fi
# fichero NUEVO del paquete
mdos ::HISPADOS/NUEVO.TXT > $T/nuevo.txt
check $T/nuevo.txt "VERSION-NUEVA-OK"
# (2) el *.INI del usuario se conservó
mdos ::HISPADOS/VENTANAS.INI > $T/vini.txt
check $T/vini.txt "TEMA=OSCURO"
# (3) el CONFIG.SYS del usuario quedó como CONFIG.ANT
if mdos ::CONFIG.ANT | grep -q .; then ok "CONFIG.ANT (copia del CONFIG.SYS anterior)"; else ko "falta CONFIG.ANT"; fi
# el temporal se borró
if mdos ::ACTUALIZA.PAQ | grep -q .; then ko "quedó C:\\ACTUALIZA.PAQ sin borrar"; else ok "C:\\ACTUALIZA.PAQ borrado"; fi
if mdir -i $HDD@@$OFF ::ACTUALIZA >/dev/null 2>&1; then ko "quedó la carpeta C:\\ACTUALIZA sin borrar"; else ok "carpeta C:\\ACTUALIZA borrada"; fi

if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then
  echo "--- instalación ---"; cat $T/l1.txt
  echo "--- actualización por red ---"; cat $T/l3.txt
fi
exit $fail
