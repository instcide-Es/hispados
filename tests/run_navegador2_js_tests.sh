#!/bin/bash
# Prueba del JavaScript del Navegador completo de Hispa-486 (NAVEGA2.EXE con
# QuickJS, HSPJS.DLL; docs/486/NAVEGADOR2.md, «JavaScript»), sin Internet:
# instalado en C:\HISPADOS\INTERNET, contra tests/tools/surfjs_servidor.py
# (HTTP). Se mira en las capturas, en lo que recibe el servidor y en el
# registro de la consola de las páginas (SURFJS.LOG):
#  - un guion de hoy (clases, funciones flecha, plantillas) cambia el texto y
#    la clase de la cabecera (las cajas se rehacen: el color nuevo);
#  - setTimeout, localStorage, fetch (JSON, con la galleta de la página y una
#    cabecera propia) y console.log;
#  - un menú desplegable (position: absolute, z-index) que se pinta ENCIMA de
#    lo de después y recibe los clics; el clic en una opción, delegado en
#    document con closest() y preventDefault() (no se sigue el enlace «#»);
#  - un botón que enseña una caja (classList.toggle) y cambia su rótulo;
#  - lo escrito en un campo: el evento input;
#  - un formulario cuyo submit se para (preventDefault) y se manda con fetch
#    POST (FormData y URLSearchParams).
# (GUARDAR=carpeta deja las capturas en PNG y los registros.)
# Full browser JavaScript test (offline, local HTTP test server).
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
make -s build/NAVEGA2.EXE build/HSPJS.DLL build/HSPTLS.DLL build/WSOCK32.DLL build/MSVCRT.DLL build/HW32.COM > /dev/null 2>&1 || {
  echo "== Hispa-486: el JavaScript del Navegador completo: sin gcc de 32 bits, se salta =="; exit 0; }
. tests/tools/win32_ventanas.sh
echo "== Hispa-486: el JavaScript del Navegador completo (NAVEGA2.EXE, QuickJS) =="
P=$((20000 + $$ % 20000))
python3 tests/tools/surfjs_servidor.py $T $P > $T/srv.log 2>&1 &
SRV=$!
trap 'kill $SRV 2> /dev/null' EXIT
for i in 1 2 3 4 5 6 7 8 9 10; do grep -q listo $T/srv.log 2> /dev/null && break; sleep 0.5; done
grep -q listo $T/srv.log || { echo "  (no arranca el servidor de pruebas: se salta)"; rm -rf $T; exit 0; }
export MTOOLS_SKIP_CHECK=1
cp "$IMG" $T/f.img
printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nFILES=30\r\n' > $T/CONFIG.SYS
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
mkdir -p $T/c
mcopy -s -n -i "$IMG" ::HISPADOS $T/c/
I=$T/c/HISPADOS/INTERNET
mkdir -p $I/RECURSOS $I/FUENTES
cp $B/MSVCRT.DLL $B/WSOCK32.DLL $B/HSPTLS.DLL $B/HSPJS.DLL $B/NAVEGA2.EXE $I/
cp $B/HW32.COM $B/MSVCRT.DLL $T/c/HISPADOS/
cp w32/surf/recursos/* terceros/netsurf/netsurf/resources/{default,quirks,adblock,internal}.css $I/RECURSOS/
cp terceros/dejavu/*.TTF $I/FUENTES/
: > $I/SURF.LOG
printf 'Inicio=http://10.0.2.2:%d/\r\n' $P > $I/NAVEGA2.INI
python3 tests/tools/mkhdd.py $T/c.img 16 $T/c/HISPADOS > /dev/null
. tests/tools/surf_teclas.sh
corre() {
  rm -f $T/mon
  timeout ${TMAX:-600} qemu-system-i386 ${QEMU_PANTALLA--vga std -global VGA.xres=800 -global VGA.yres=600} -display none -serial stdio -monitor unix:$T/mon,server,nowait \
    -parallel none -m ${MEM:-32} -drive file=$T/f.img,format=raw,if=floppy,index=0 -drive file=$T/c.img,format=raw,if=ide,index=0 \
    -boot a -no-reboot -netdev user,id=n0 -device rtl8139,netdev=n0 > $T/raw 2>&1
  tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 -c > $T/log.txt
}
aparta() { ir 790 300; }
{ sleep 6; send 'A:\HISPADOS\HSPRED' 2; send 'C:\HISPADOS\VENTANAS /486' 10
  tecla ctrl-esc; sleep 1; tecla i; sleep 0.5; tecla right; sleep 1; tecla down; sleep 0.5; tecla ret
  sleep 40; aparta; shot j1                                   # la página, con su JavaScript
  clic 45 117; sleep 6; aparta; shot j2                       # «Menú»: el desplegable, encima
  clic 60 167; sleep 6; aparta; shot j3                       # «Opción dos»
  clic 135 124; sleep 6; aparta; shot j4                      # «Ver la caja»
  clic 90 290; sleep 2; escribe "hola"; sleep 6; aparta; shot j5   # el campo: input
  clic 55 410; sleep 8; aparta; shot j6                       # «Mandar»: submit con fetch
  tecla alt-f4; sleep 2; tecla alt-f4; sleep 4
  mon quit; } | corre
mcopy -n -i $T/c.img@@32256 ::HISPADOS/INTERNET/SURFJS.LOG $T/surfjs.log 2> /dev/null
mcopy -n -i $T/c.img@@32256 ::HISPADOS/INTERNET/SURF.LOG $T/surf.log 2> /dev/null
tr -d '\r' < $T/surfjs.log > $T/js.log 2> /dev/null
echo "-- la página y sus guiones --"
grep -q "pagina lista 2" $T/js.log && ok "console.log al registro (SURFJS.LOG), querySelectorAll" || ko "no está «pagina lista» en SURFJS.LOG"
grep -q "error" $T/js.log && ko "errores de JavaScript: $(grep error $T/js.log | head -2)" || ok "sin errores de JavaScript"
chk $T/j1.ppm cuenta 0 60 780 40 306767 15000 40000 "la cabecera, con la clase que pone el guion (las cajas otra vez)"
grep -q "GET /datos.json si | sesion=abc123" $T/pedidos.log && ok "fetch con su cabecera y la galleta de la página" || ko "fetch: $(grep datos $T/pedidos.log)"
echo "-- el menú desplegable (z-index) --"
chk $T/j2.ppm cuenta 14 130 200 50 ffffcf 2000 12000 "el menú, encima del bloque rojo" \
  cambia 0 60 780 200 $T/j1.ppm 1000
chk $T/j3.ppm cuenta 14 130 200 50 ffffcf 0 0 "el menú, cerrado tras elegir"
grep -c "^GET / " $T/pedidos.log | grep -qx 1 && ok "el clic en «#» no recarga la página (preventDefault)" || ko "se ha pedido otra vez la página"
echo "-- el botón, el campo y el formulario --"
chk $T/j4.ppm cuenta 0 220 780 40 309830 10000 40000 "la caja verde aparece (classList.toggle)" \
  cambia 0 60 780 400 $T/j3.ppm 1000
chk $T/j5.ppm cambia 180 275 200 30 $T/j4.ppm 100
grep -q "POST /guarda  | sesion=abc123 | a=1" $T/pedidos.log && ok "submit parado y mandado con fetch POST (a=1)" || ko "no llega el POST /guarda"
grep -q "POST /no" $T/pedidos.log && ko "el formulario se ha enviado sin JavaScript" || ok "el formulario no se envía solo"
chk $T/j6.ppm cambia 0 420 400 40 $T/j5.ppm 100
echo "-- tiempos y memoria --"
grep "^\[memoria\]" $T/js.log | head -2 | sed 's/^/  /'
grep "cajas otra vez" $T/surf.log | tail -1 | sed 's/^[0-9]* /  /'
[ -n "${GUARDAR:-}" ] && { mkdir -p "$GUARDAR"; cp $T/surf.log $T/js.log $T/pedidos.log "$GUARDAR"/; for f in $T/*.ppm; do python3 -I tests/tools/ppm2png.py $f "$GUARDAR"/$(basename ${f%.ppm}).png; done; }
fin
