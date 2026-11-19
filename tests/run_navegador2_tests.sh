#!/bin/bash
# Prueba del Navegador completo de Hispa-486 (NAVEGA2.EXE: NetSurf con la
# ventana de Hispa Ventanas; docs/486/NAVEGADOR2.md), sin Internet: instalado
# en C:\HISPADOS\INTERNET (lo del disquete «Internet» y lo de «Internet 2»),
# contra el servidor de pruebas del anfitrión (tests/tools/surf_servidor.py,
# por HTTPS con la raíz de pruebas de tests/tools/nav_certs.sh: RAIZ.CER junto
# al programa). Se mira en las capturas y en lo que recibe el servidor:
#  - el menú Inicio, grupo Internet: «Navegador completo» (lo crea Hispa
#    Ventanas si encuentra INTERNET\NAVEGA2.EXE) y abre el programa;
#  - una página de disco (file:) con letras de ancho variable y tablas;
#  - por HTTPS: la hoja de estilo de fuera con variables de CSS (el fondo de
#    la página y el borde de las fichas salen de var()), los botones con
#    variables por clase (.btn-peligro en granate, .btn-marca en petróleo),
#    el degradado de la cabecera (su primer color), flexbox (tres fichas en
#    una fila), PNG y JPEG; el User-Agent es «HispaSurf» (NO «HispaNavegador»:
#    a ese ClicGest le da la web ligera);
#  - un clic en un enlace (una redirección 302), Atrás, F6 y escribir una
#    dirección, un formulario GET (el campo, Intro), una página larga y Av Pág;
#  - el tiempo de carga (SURF.LOG) y la memoria (el Administrador de tareas).
# (GUARDAR=carpeta deja las capturas en PNG y el registro.)
# Full browser test (offline, against a local HTTPS test server).
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
make -s build/NAVEGA2.EXE build/HSPTLS.DLL build/WSOCK32.DLL build/MSVCRT.DLL build/HW32.COM > /dev/null 2>&1 || {
  echo "== Hispa-486: el Navegador completo: sin gcc de 32 bits, se salta =="; exit 0; }
. tests/tools/win32_ventanas.sh
. tests/tools/nav_certs.sh
echo "== Hispa-486: el Navegador completo (NAVEGA2.EXE, NetSurf) con una página moderna por HTTPS =="
if ! nav_certs $T; then echo "  (sin openssl no se pueden hacer los certificados: se salta)"; rm -rf $T; exit 0; fi
P=$((20000 + $$ % 20000))
python3 tests/tools/surf_servidor.py $T $P > $T/srv.log 2>&1 &
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
mkdir -p $I/RECURSOS $I/FUENTES $T/c/PAGINAS
cp $B/MSVCRT.DLL $B/WSOCK32.DLL $B/HSPTLS.DLL $B/NAVEGA2.EXE $T/RAIZ.CER $I/
cp $B/HW32.COM $B/MSVCRT.DLL $T/c/HISPADOS/
cp w32/surf/recursos/* terceros/netsurf/netsurf/resources/{default,quirks,adblock,internal}.css $I/RECURSOS/
cp terceros/dejavu/*.TTF $I/FUENTES/
: > $I/SURF.LOG
printf 'Inicio=https://10.0.2.2:%d/\r\n' $((P + 1)) > $I/NAVEGA2.INI
# la página de disco
cat > $T/c/PAGINAS/DISCO.HTM <<'EOF'
<!DOCTYPE html><html lang="es"><head><meta charset="utf-8"><title>De disco</title>
<style>body{font-family:sans-serif;margin:16px} h1{font-family:serif;color:#8a3b12}
td,th{border:1px solid #999;padding:3px 8px} th{background:#2f5d62;color:#fff} .mono{font-family:monospace}</style></head>
<body><h1>Letras de ancho variable</h1><p>iiiiiiiiii WWWWWWWWWW</p><p class="mono">iiiiiiiiii WWWWWWWWWW</p>
<table><tr><th>Libro</th><th>Autor</th></tr><tr><td>El Quijote</td><td>Cervantes</td></tr></table></body></html>
EOF
python3 tests/tools/mkhdd.py $T/c.img 16 $T/c/HISPADOS $T/c/PAGINAS > /dev/null
. tests/tools/surf_teclas.sh
corre() {
  rm -f $T/mon
  timeout ${TMAX:-900} qemu-system-i386 ${QEMU_PANTALLA--vga std -global VGA.xres=800 -global VGA.yres=600} -display none -serial stdio -monitor unix:$T/mon,server,nowait \
    -parallel none -m ${MEM:-32} -drive file=$T/f.img,format=raw,if=floppy,index=0 -drive file=$T/c.img,format=raw,if=ide,index=0 \
    -boot a -no-reboot -netdev user,id=n0 -device rtl8139,netdev=n0 > $T/raw 2>&1
  tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 -c > $T/log.txt
}
aparta() { ir 790 300; }
{ sleep 6; send 'A:\HISPADOS\HSPRED' 2; send 'C:\HISPADOS\VENTANAS /486' 10
  tecla ctrl-esc; sleep 1; tecla i; sleep 0.5; tecla right; sleep 1; aparta; shot i1          # Inicio > Internet
  tecla down; sleep 0.5; tecla ret; sleep 50; aparta; shot n1                                 # el Navegador completo: la página moderna
  clic 210 222; sleep 25; aparta; shot n2                                                     # «Redirección» (302)
  tecla f6; sleep 1; escribe "c:/paginas/disco.htm"; tecla ret; sleep 25; aparta; shot d1      # la de disco
  tecla backspace; sleep 25; aparta; shot n3                                                 # Atrás (Retroceso): la de la redirección
  tecla backspace; sleep 25; aparta; shot n4                                                 # y la moderna
  clic 100 268; sleep 1; escribe "hola"; tecla ret; sleep 25; aparta; shot b1                # el formulario GET
  tecla backspace; sleep 25; clic 85 222; sleep 30; aparta; shot l1                          # «Página larga»
  tecla pgdn; sleep 5; shot l2; tecla end; sleep 5; shot l3                                  # Av Pág y Fin
  tecla ctrl-esc; sleep 1; tecla a a a; sleep 0.5; tecla ret; sleep 6; aparta; shot m1       # la memoria
  tecla alt-f4; sleep 2; tecla alt-f4; sleep 4
  mon quit; } | corre
mcopy -n -i $T/c.img@@32256 ::HISPADOS/INTERNET/SURF.LOG $T/surf.log 2> /dev/null
PAG="0 57 780 550"
echo "-- el menú Inicio: «Navegador completo» --"
chk $T/i1.ppm busca 250 380 520 470 9 12 "Navegador completo"
echo "-- la página moderna, por HTTPS --"
grep -q "GET / HispaSurf/0.1 (NetSurf 3.11; Hispa-DOS 486)" $T/pedidos.log && ok "User-Agent «HispaSurf»" || ko "User-Agent: $(head -1 $T/pedidos.log)"
grep -q HispaNavegador $T/pedidos.log && ko "se presenta como HispaNavegador" || ok "no se presenta como HispaNavegador"
grep -q "GET /estilo.css" $T/pedidos.log && ok "pide la hoja de estilo de fuera" || ko "no pide /estilo.css"
grep -q "GET /verde.png" $T/pedidos.log && grep -q "GET /gris.jpg" $T/pedidos.log && ok "pide el PNG y el JPEG" || ko "no pide las imágenes"
# (los colores, ya en la paleta de Hispa Ventanas: se cuentan los puntos)
chk $T/n1.ppm cuenta 0 60 780 60 306767 3000 60000 "la cabecera (el primer color del degradado)" \
  cuenta 0 130 780 300 ffffcf 20000 400000 "el fondo de la página, de var(--fondo)" \
  cuenta 152 206 120 30 cf3030 2000 3600 "el botón granate (.btn-peligro: variables por clase)" \
  cuenta 22 206 125 30 306767 2000 3600 "el botón petróleo (.btn-marca)" \
  cuenta 0 310 780 80 cf3030 1500 20000 "el texto grande (var(--aviso))" \
  cuenta 0 120 780 60 309830 900 1300 "el PNG verde (40x30)" \
  cuenta 0 120 780 60 878787 150 400 "el JPEG gris (16x16)"
echo "-- la redirección, la página de disco, Atrás, el formulario --"
grep -q "GET /vuelve" $T/pedidos.log && grep -q "GET /llegada" $T/pedidos.log && ok "sigue la redirección 302" || ko "no sigue la redirección"
chk $T/n2.ppm cuenta $PAG 306767 200000 500000 "la página de llegada (fondo petróleo)"
chk $T/n3.ppm igual 0 57 780 490 $T/n2.ppm 0 57 3000
chk $T/d1.ppm cuenta $PAG 306767 3000 20000 "la tabla de la página de disco (la cabecera)" \
  cambia 0 57 780 490 $T/n2.ppm 10000
# las letras de ancho variable: diez «i» ocupan menos que en la monoespaciada
python3 - $T/d1.ppm <<'EOP' && ok "letras de ancho variable (las «i» de la normal, más estrechas que las de la monoespaciada)" || ko "letras de ancho variable"
import sys
d = open(sys.argv[1], 'rb').read(); t = d.split(b'\n', 3); w = int(t[1].split()[0]); px = t[3]
def fin_ies(y0, y1):
    x, hueco = 18, 0
    tinta = [any(sum(px[(y * w + xx) * 3:(y * w + xx) * 3 + 3]) < 400 for y in range(y0, y1)) for xx in range(800)]
    while x < 400 and not tinta[x]: x += 1
    while x < 400:
        hueco = hueco + 1 if not tinta[x] else 0
        if hueco > 5: return x - hueco
        x += 1
    return 999
a, b = fin_ies(136, 152), fin_ies(172, 188)
print('  (las «i»: hasta x=%d la normal, x=%d la monoespaciada)' % (a, b))
sys.exit(0 if a + 20 < b else 1)
EOP
chk $T/n4.ppm igual 0 57 780 490 $T/n1.ppm 0 57 3000
grep -q "GET /busca?q=hola" $T/pedidos.log && ok "el formulario GET (q=hola)" || ko "no llega /busca?q=hola"
echo "-- la página larga: se mueve --"
chk $T/l2.ppm cambia 0 57 780 490 $T/l1.ppm 1000
chk $T/l3.ppm cuenta $PAG cf3030 5000 60000 "Fin: la última fila (granate)"
chk $T/l1.ppm cuenta $PAG cf3030 0 0 "arriba no se ve la última fila"
echo "-- tiempos y memoria --"
grep "estado: Hecho" $T/surf.log | sed 's/^[0-9]* /  /' | head -8
[ -n "${GUARDAR:-}" ] && { mkdir -p "$GUARDAR"; cp $T/surf.log $T/pedidos.log "$GUARDAR"/; for f in $T/*.ppm; do python3 -I tests/tools/ppm2png.py $f "$GUARDAR"/$(basename ${f%.ppm}).png; done; }
fin
