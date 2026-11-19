#!/bin/bash
# [JS] Prueba del JavaScript del Navegador de Hispa-486 (docs/486/NAVEGADOR.md,
# «JavaScript»), sin Internet, contra las páginas /js/ del servidor de pruebas
# (tests/tools/nav_servidor.py y tests/tools/nav_js.py):
#  1. En el anfitrión (si hay gcc -m32 con su biblioteca: tests/navjs): el DOM
#     y los guiones tal cual, con órdenes (tests/navjs/*.txt) y la página
#     vuelta a texto: el buscador que sugiere al escribir (fetch, JSON,
#     flechas, Intro, clic en una sugerencia), pestañas, <details>, confirm y
#     alert, onsubmit, localStorage, setTimeout, XMLHttpRequest,
#     DOMContentLoaded, un error al registro y un bucle sin fin cortado; y
#     lo de la maqueta de la prueba de uso (/uso: autofocus, dónde empieza el
#     Tab, la marca #resultados, los botones .btn y btn-danger, los campos
#     desde la primera letra y la pregunta de confirm(this.dataset...)).
#  2. En Hispa Ventanas (QEMU): NAVEGA.EXE con HSPJS.DLL por HTTPS: lo
#     mismo con el ratón y el teclado, mirando las capturas, el registro
#     (NAVEGA.LOG) y lo que recibe el servidor; el tope de tiempo (TiempoJS=3);
#     y con JavaScript=NO, que sale el <noscript>. Que se cierra y no queda
#     memoria.
# (GUARDAR=carpeta deja las capturas, el registro y lo del anfitrión.)
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
make -s build/NAVEGA.EXE build/HSPJS.DLL build/HSPTLS.DLL build/WSOCK32.DLL build/MSVCRT.DLL build/HW32.COM > /dev/null 2>&1 || {
  echo "== Hispa-486: el JavaScript del Navegador: sin gcc de 32 bits, se salta =="; exit 0; }
. tests/tools/win32_ventanas.sh
. tests/tools/nav_certs.sh
echo "== Hispa-486: el JavaScript del Navegador (NAVEGA.EXE con HSPJS.DLL) =="
if ! nav_certs $T; then echo "  (sin openssl no se pueden hacer los certificados: se salta)"; rm -rf $T; exit 0; fi
P=$((20000 + $$ % 20000))
python3 tests/tools/nav_servidor.py $T $P > $T/srv.log 2>&1 &
SRV=$!
trap 'kill $SRV 2> /dev/null' EXIT
for i in 1 2 3 4 5 6 7 8 9 10; do grep -q listo $T/srv.log 2> /dev/null && break; sleep 0.5; done
grep -q listo $T/srv.log || { echo "  (no arranca el servidor de pruebas: se salta)"; rm -rf $T; exit 0; }

# ---- 1. en el anfitrión ----
tiene() { if grep -qF -- "$2" $T/an_$1.txt; then ok "$1: $2"; else ko "$1: $2"; fi; }
notiene() { if grep -qF -- "$2" $T/an_$1.txt; then ko "$1: (no debía) $2"; else ok "$1: sin «$2»"; fi; }
if tests/navjs/hazlo.sh > $T/hazlo.log 2>&1; then
  echo "-- en el anfitrión (tests/navjs) --"
  mkdir -p $T/an
  an() { local d=$PWD; (cd $T/an && rm -f NAVEGA.LOG && timeout 90 $d/build/navjs/anfitrion "http://127.0.0.1:$P$2" "$d/tests/navjs/$3.txt" > $T/an_$1.txt 2>&1); }
  an raiz /js/ raiz
  tiene raiz "Con JavaScript: 2-4-6."
  notiene raiz "SIN-JAVASCRIPT"
  an busca /js/buscador buscador
  tiene busca " · Socia 017 Prueba (nº 3)"
  tiene busca "3 coinciden."
  tiene busca "IR: http://127.0.0.1:$P/js/ficha?n=Socia%20010%20Prueba"
  tiene busca "[log] buscador listo"
  an busca2 /js/buscador buscador2
  tiene busca2 " · María García (nº 1)"
  tiene busca2 "IR: http://127.0.0.1:$P/js/ficha?n=Mar%C3%ADa%20Garc%C3%ADa"
  an pest /js/pestanas pestanas
  tiene pest "PANEL-DEVOLVER: nada."
  tiene pest "DETALLE-ABIERTO"
  tiene pest "[log] selector 2 true"
  an cuad /js/cuadros cuadros
  tiene cuad "Visitas: 1"
  tiene cuad "Temporizador: hecho"
  tiene cuad "XHR: 3 socias"
  tiene cuad "DOM: cargado"
  tiene cuad "CONFIRM: ¿Borrar de verdad?"
  tiene cuad "ALERT: Aviso de la página"
  tiene cuad "(submit impedido)"
  tiene cuad "ENVIA: POST http://127.0.0.1:$P/js/envio dato=uno"
  tiene cuad "CONFIRM: ¿Enviar el formulario?"
  notiene cuad "_cg_sin_js"
  tiene cuad "[error] ReferenceError: 'noExiste' is not defined"
  an cuad2 /js/cuadros raiz
  tiene cuad2 "Visitas: 2"
  an gal /js/galletas galletas
  tiene gal "COOKIE: vista=1"
  notiene gal "COOKIE: vista=1; sesion"
  tiene gal "ECO galletas=[vista=1; sesion=S1; js=1] agente=[HispaNavegador/"
  notiene gal "oculta"
  tiene gal "[log] HispaNavegador/"
  an uso /uso uso
  tiene uso "AUTOFOCO: q"
  tiene uso "(sin texto)"
  notiene uso "MARCA resultados: fila -1"
  tiene uso "PREGUNTA: ¿Seguro que se ha perdido? No tiene vuelta."
  tiene uso "[! Dar por perdido]"
  tiene uso "[Prestar]"
  tiene uso "<! Perdido de verdad>"
  tiene uso "[Real Academia Española / Alfaguara"
  tiene uso "[Reseñas de los lectores]"
  an usotab /uso/tab uso_tab
  tiene usotab "Primero del contenido"
  tiene usotab "AUTOFOCO: nadie"
  an bucle /js/bucle bucle
  tiene bucle "Sigue: sí"
  tiene bucle "un guion ha tardado mas de"
else
  echo "-- (sin gcc -m32 con su biblioteca: lo del anfitrión se salta) --"
fi

# ---- 2. en Hispa Ventanas ----
export MTOOLS_SKIP_CHECK=1
cp "$IMG" $T/f.img
printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nFILES=30\r\n' > $T/CONFIG.SYS
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
mkdir -p $T/c
mcopy -s -n -i "$IMG" ::HISPADOS $T/c/
mkdir -p $T/c/HISPADOS/INTERNET
cp $B/MSVCRT.DLL $B/WSOCK32.DLL $B/HSPTLS.DLL $B/HSPJS.DLL $B/NAVEGA.EXE $T/RAIZ.CER $T/c/HISPADOS/INTERNET/
cp $B/HW32.COM $B/MSVCRT.DLL $T/c/HISPADOS/
printf 'Inicio=https://10.0.2.2:%d/js/\r\nTiempoJS=3\r\n' $((P + 1)) > $T/c/HISPADOS/INTERNET/NAVEGA.INI
python3 tests/tools/mkhdd.py $T/c.img 16 $T/c/HISPADOS > /dev/null
corre() {
  rm -f $T/mon
  timeout ${TMAX:-700} qemu-system-i386 ${QEMU_PANTALLA--vga std -global VGA.xres=800 -global VGA.yres=600} -display none -serial stdio -monitor unix:$T/mon,server,nowait \
    -parallel none -m 16 -drive file=$T/f.img,format=raw,if=floppy,index=0 -drive file=$T/c.img,format=raw,if=ide,index=0 \
    -boot a -no-reboot -netdev user,id=n0 -device rtl8139,netdev=n0 > $T/raw 2>&1
  tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 -c > $T/log.txt
}
aparta() { ir 300 520; }
fy() { echo $((89 + 18 * $1)); }
# escribe en la dirección (F6) una ruta del servidor bueno y la abre
dir() {
  local c
  tecla f6; sleep 1
  tecla h t t p s shift-semicolon slash slash 1 0 dot 0 dot 2 dot 2 shift-semicolon
  for c in $(echo $((P + 1)) | sed 's/./& /g'); do tecla $c; done
  for c in $(echo "$1" | sed 's/./& /g'); do case $c in /) tecla slash;; ?) tecla $c;; esac; done
  tecla ret; sleep ${2:-8}
}
{ sleep 6; send 'A:\HISPADOS\HSPRED' 2; send 'C:\HISPADOS\VENTANAS /486' 10
  tecla ctrl-esc; sleep 1; tecla a a a; sleep 0.5; tecla ret; sleep 5; aparta; shot m0     # la memoria antes de nada
  tecla alt-f4; sleep 1.5
  tecla ctrl-esc; sleep 1; tecla i; sleep 0.5; tecla right; sleep 1; tecla ret; sleep 14; aparta; shot j1   # el índice
  dir /js/buscador 9; aparta; shot b0                                                        # el buscador
  clic 120 $(fy 3); sleep 1; tecla s o c; sleep 8; aparta; shot b1                           # escribir: sugiere
  tecla down; sleep 2; tecla down; sleep 3; aparta; shot b2                                  # flechas
  tecla ret; sleep 8; aparta; shot b3                                                        # Intro: a la ficha
  dir /js/buscador 9; clic 120 $(fy 3); sleep 1; tecla m a r; sleep 8; aparta; shot b4
  clic 60 $(fy 5); sleep 8; aparta; shot b5                                                  # pulsar la sugerencia
  dir /js/pestanas 9; aparta; shot p0                                                        # pestañas
  clic 40 $(fy 4); sleep 3; aparta; shot p1                                                  # «A devolver»
  clic 40 $(fy 11); sleep 3; aparta; shot p2                                                 # «Abrir el detalle»
  dir /js/cuadros 10; aparta; shot q0                                                        # cuadros, temporizador, XHR
  clic 40 $(fy 11); sleep 3; aparta; shot q1                                                 # «Borrar»: confirm
  tecla ret; sleep 3; aparta; shot q2                                                        # «Sí»
  clic 130 $(fy 11); sleep 3; aparta; shot q3                                                # «Avisar»: alert
  tecla ret; sleep 2
  dir /js/bucle 16; aparta; shot z1                                                          # el bucle sin fin, cortado
  tecla f5; sleep 9; aparta; shot z2                                                         # el Navegador sigue
  tecla ctrl-esc; sleep 1; tecla a a a; sleep 0.5; tecla ret; sleep 5; aparta; shot m2       # la memoria con el Navegador
  tecla alt-f4; sleep 1.5; tecla alt-f4; sleep 10
  tecla ctrl-esc; sleep 1; tecla a a a; sleep 0.5; tecla ret; sleep 5; aparta; shot m1       # y después
  mon quit; } | corre
PAG="0 78 640 548"
echo "-- en Hispa Ventanas: el índice, por HTTPS --"
chk $T/j1.ppm busca $PAG 0 9 "Con JavaScript: 2-4-6." nobusca $PAG 0 9 "SIN-JAVASCRIPT" nobusca $PAG 0 9 "Sin JavaScript."
echo "-- el buscador: sugiere al escribir (fetch), flechas, Intro y clic --"
chk $T/b0.ppm busca $PAG 0 9 "Escriba para buscar."
chk $T/b1.ppm busca $PAG 12 9 "Socia 017 Prueba" busca $PAG 0 9 "3 coinciden."
chk $T/b3.ppm busca $PAG 0 9 "FICHA DE Socia 010 Prueba"
chk $T/b4.ppm busca $PAG 12 9 "María García" busca $PAG 0 9 "1 coinciden."
chk $T/b5.ppm busca $PAG 0 9 "FICHA DE María García"
echo "-- pestañas y detalles --"
chk $T/p0.ppm busca $PAG 0 9 "PANEL-ASIENTOS" nobusca $PAG 0 9 "PANEL-DEVOLVER"
chk $T/p1.ppm busca $PAG 0 9 "PANEL-DEVOLVER" nobusca $PAG 0 9 "PANEL-ASIENTOS"
chk $T/p2.ppm busca $PAG 0 9 "DETALLE-ABIERTO"
echo "-- confirm, alert, setTimeout, XMLHttpRequest, localStorage --"
chk $T/q0.ppm busca $PAG 0 9 "Visitas: 1" busca $PAG 0 9 "Temporizador: hecho" busca $PAG 0 9 "XHR: 3 socias" busca $PAG 0 9 "DOM: cargado"
chk $T/q1.ppm busca 200 240 600 300 0 8 "¿Borrar de verdad?"
chk $T/q2.ppm busca $PAG 0 9 "Sí, borrado."
chk $T/q3.ppm busca 200 240 600 300 0 8 "Aviso de la página"
echo "-- un bucle sin fin: se corta y el Navegador sigue --"
chk $T/z1.ppm busca $PAG 0 9 "Sigue: sí" busca 0 548 100 570 0 15 "Un guion de la página tardaba demasiado"
chk $T/z2.ppm busca $PAG 0 9 "Sigue: sí"
echo "-- el registro (NAVEGA.LOG) y lo que ha recibido el servidor --"
mcopy -n -i $T/c.img@@32256 ::HISPADOS/INTERNET/NAVEGA.LOG $T/NAVEGA.LOG 2> /dev/null
mcopy -n -i $T/c.img@@32256 ::HISPADOS/INTERNET/NAVEGA.ALM $T/NAVEGA.ALM 2> /dev/null
reg() { if grep -qF -- "$1" $T/NAVEGA.LOG 2> /dev/null; then ok "registro: $1"; else ko "registro: $1"; fi; }
reg "[log] buscador listo"
reg "[error] ReferenceError: 'noExiste' is not defined"
reg "un guion ha tardado mas de 3 s y se ha parado"
grep -qF '"visitas":"1"' $T/NAVEGA.ALM 2> /dev/null && ok "localStorage, en NAVEGA.ALM" || ko "localStorage, en NAVEGA.ALM"
pedido() { if grep -qF -- "$1" $T/pedidos.log; then ok "$2"; else ko "$2 (falta: $1)"; fi; }
pedido "GET /js/api?q=soc |" "fetch al escribir"
pedido "GET /js/buscador.js |" "el guion de fuera"
pedido "GET /js/api?q=socia |" "XMLHttpRequest"
echo "-- la memoria: con el Navegador abierto y después de cerrarlo --"
chk $T/m2.ppm busca 176 480 640 498 0 6 "5 programas en marcha" cambia $MEMZ $T/m0.ppm 50
chk $T/m1.ppm busca 176 480 640 498 0 6 "4 programas en marcha" igual $MEMZ $T/m0.ppm ${MEMZ% * *} 0

# ---- con JavaScript=NO ----
echo "-- con JavaScript=NO: el <noscript> y nada de guiones --"
printf 'Inicio=https://10.0.2.2:%d/js/\r\nJavaScript=NO\r\n' $((P + 1)) | mcopy -o -i $T/c.img@@32256 - ::HISPADOS/INTERNET/NAVEGA.INI
{ sleep 6; send 'A:\HISPADOS\HSPRED' 2; send 'C:\HISPADOS\VENTANAS /486' 10
  tecla ctrl-esc; sleep 1; tecla i; sleep 0.5; tecla right; sleep 1; tecla ret; sleep 12; aparta; shot s1
  mon quit; } | TMAX=120 corre
chk $T/s1.ppm busca $PAG 0 9 "Sin JavaScript." busca $PAG 0 9 "SIN-JAVASCRIPT" nobusca $PAG 0 9 "Con JavaScript"
[ -n "${GUARDAR:-}" ] && cp $T/pedidos.log $T/log.txt $T/NAVEGA.LOG $T/an_*.txt "$GUARDAR"/ 2> /dev/null
fin
