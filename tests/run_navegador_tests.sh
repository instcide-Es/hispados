#!/bin/bash
# Prueba del Navegador de Hispa-486 (docs/486/NAVEGADOR.md), sin Internet:
# NAVEGA.EXE dentro de Hispa Ventanas (escritorio de Hispa-486, 800x600),
# instalado en C:\HISPADOS\INTERNET, contra el servidor de pruebas del
# anfitrión (tests/tools/nav_servidor.py: una «biblioteca» de mentira parecida
# a la de ClicGest, por HTTPS con certificados hechos aquí; su raíz solo se le
# da al sistema en la prueba: RAIZ.CER junto al programa). Se maneja con el
# ratón y el teclado y se miran las capturas y lo que ha recibido el servidor:
#  - el grupo «Internet» del menú Inicio aparece solo y abre el Navegador;
#  - la página de entrada por HTTPS (TLS 1.2, certificado comprobado): entrar
#    con usuario y clave (POST, galleta de sesión, redirección);
#  - la lista: una tabla, acentos, lo escondido por CSS y por «hidden» no se
#    ve, lo plegable (<details>) se despliega;
#  - prestar: buscar (GET), elegir de una lista (display:block), un <select>,
#    un área de texto con dos líneas, botones de radio y una casilla; el POST
#    que llega es el que tiene que ser (UTF-8 con %XX) y el aviso se ve;
#  - devolver: el confirm() de la página sale como un cuadro («No» no envía);
#  - Atrás y Adelante; la dirección escrita a mano; Tab e Intro;
#  - los certificados malos (de otro nombre, sin autoridad, caducado) no se
#    abren y se dice por qué; texto sin formato por RSA; Latin-1; una página
#    grande y su desplazamiento; redirecciones a http (sin cifrar); respuesta
#    sin longitud; el 404; un archivo que no es una página; javascript:;
#  - se cierra y no queda memoria.
#  (GUARDAR=carpeta deja las capturas y el registro del servidor.)
# Browser test (offline, against a local HTTPS test server).
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
make -s build/NAVEGA.EXE build/HSPTLS.DLL build/WSOCK32.DLL build/MSVCRT.DLL build/HW32.COM > /dev/null 2>&1 || {
  echo "== Hispa-486: el Navegador: sin gcc de 32 bits, se salta =="; exit 0; }
. tests/tools/win32_ventanas.sh
. tests/tools/nav_certs.sh
echo "== Hispa-486: el Navegador (NAVEGA.EXE) con una biblioteca de pruebas por HTTPS =="
if ! nav_certs $T; then echo "  (sin openssl no se pueden hacer los certificados: se salta)"; rm -rf $T; exit 0; fi
P=$((20000 + $$ % 20000))
python3 tests/tools/nav_servidor.py $T $P > $T/srv.log 2>&1 &
SRV=$!
trap 'kill $SRV 2> /dev/null' EXIT
for i in 1 2 3 4 5 6 7 8 9 10; do grep -q listo $T/srv.log 2> /dev/null && break; sleep 0.5; done
grep -q listo $T/srv.log || { echo "  (no arranca el servidor de pruebas: se salta)"; rm -rf $T; exit 0; }
export MTOOLS_SKIP_CHECK=1
cp "$IMG" $T/f.img
printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nFILES=30\r\n' > $T/CONFIG.SYS
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
# el disco duro: C:\HISPADOS (lo del disquete) y C:\HISPADOS\INTERNET (el disquete «Internet»)
mkdir -p $T/c
mcopy -s -n -i "$IMG" ::HISPADOS $T/c/
mkdir -p $T/c/HISPADOS/INTERNET
cp $B/MSVCRT.DLL $B/WSOCK32.DLL $B/HSPTLS.DLL $B/NAVEGA.EXE $T/RAIZ.CER $T/c/HISPADOS/INTERNET/
cp $B/HW32.COM $B/MSVCRT.DLL $T/c/HISPADOS/
printf 'Inicio=https://10.0.2.2:%d/\r\nIdioma=es-ES\r\n' $((P + 1)) > $T/c/HISPADOS/INTERNET/NAVEGA.INI
python3 tests/tools/mkhdd.py $T/c.img 16 $T/c/HISPADOS > /dev/null
corre() {
  rm -f $T/mon
  timeout ${TMAX:-700} qemu-system-i386 -display none -serial stdio -monitor unix:$T/mon,server,nowait \
    -parallel none -m 16 -drive file=$T/f.img,format=raw,if=floppy,index=0 -drive file=$T/c.img,format=raw,if=ide,index=0 \
    -boot a -no-reboot -netdev user,id=n0 -device rtl8139,netdev=n0 > $T/raw 2>&1
  tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 -c > $T/log.txt
}
aparta() { ir 300 520; }            # el ratón, donde no estorba
atras() { clic 40 40; sleep ${1:-5}; }
# La ventana va a toda la pantalla: la fila f de la página empieza en
# y = 80 + 18 f, y la columna c en x = 1 + 8 c. Los botones, en y = 40.
fy() { echo $((89 + 18 * $1)); }
{ sleep 6; send 'A:\HISPADOS\HSPRED' 2; send 'C:\HISPADOS\VENTANAS /486' 10
  tecla ctrl-esc; sleep 1; tecla a a a; sleep 0.5; tecla ret; sleep 5; aparta; shot m0     # la memoria antes de nada
  tecla alt-f4; sleep 1.5
  tecla ctrl-esc; sleep 1; tecla i; sleep 0.5; tecla right; sleep 1; aparta; shot i1         # Inicio > Internet
  tecla ret; sleep 10; aparta; shot n1                                                       # el Navegador: la entrada
  clic 150 $(fy 5); tecla m a r i a; tecla tab; tecla s e c r e t o; sleep 0.5; aparta; shot n2
  tecla ret; sleep 8; aparta; shot n3                                                        # dentro: la lista
  clic 20 $(fy 10); sleep 2; aparta; shot n4                                                 # «Más datos»: se despliega
  clic 100 $(fy 0); sleep 6; clic 180 $(fy 3); tecla g a r c; tecla ret; sleep 6; aparta; shot p1   # prestar: buscar
  clic 60 $(fy 7); sleep 6; aparta; shot p2                                                  # la ficha del préstamo
  clic 100 $(fy 5); sleep 1.5; aparta; shot p3                                               # la lista de libros, abierta
  clic 100 $(fy 8); sleep 1.5                                                                # «Recetas de la abuela»
  clic 60 $(fy 8); tecla h o l a spc l u i s ret d o s; sleep 0.5                            # la nota, en dos líneas
  clic 17 $(fy 12); clic 17 $(fy 14); sleep 1; aparta; shot p4                               # 15 días y avisar
  clic 200 $(fy 16); sleep 1.5; shot p5                                                      # el botón que solo va con JavaScript
  clic 40 $(fy 16); sleep 8; aparta; shot p6                                                 # «Prestar»: el aviso y la tabla
  clic 230 $(fy 0); sleep 6; aparta; shot d1                                                 # devolver
  clic 185 $(fy 4); sleep 3; aparta; shot d2                                                 # el confirm() de la página
  clic 444 313; sleep 3; aparta; shot d3                                                     # «No»: no se envía
  clic 185 $(fy 4); sleep 3; tecla ret; sleep 7; aparta; shot d4                             # «Sí»: devuelto
  atras; atras; aparta; shot h1                                                              # Atrás dos veces
  clic 130 40; sleep 5; aparta; shot h2                                                      # Adelante
  tecla f6; sleep 1; shot e0                                                                 # la dirección, elegida entera
  tecla 1 0 dot 0 dot 2 dot 2 shift-semicolon; for c in $(echo $P | sed 's/./& /g'); do tecla $c; done
  tecla slash e n l a c e s; sleep 0.5; shot e1; tecla ret; sleep 6; aparta; shot e2         # escrita a mano (http)
  clic 60 $(fy 0); sleep 7; aparta; shot c1; atras                                           # certificado de otro nombre
  clic 60 $(fy 2); sleep 7; aparta; shot c2; atras                                           # sin autoridad conocida
  clic 60 $(fy 4); sleep 7; aparta; shot c3; atras                                           # caducado
  clic 60 $(fy 6); sleep 7; aparta; shot c4; atras                                           # texto sin formato, por RSA
  clic 60 $(fy 8); sleep 6; aparta; shot c5; atras                                           # Latin-1
  clic 30 $(fy 10); sleep 9; aparta; shot g1; tecla end; sleep 2; shot g2                    # la grande: arriba y abajo
  tecla pgup; sleep 1.5; shot g3; atras
  clic 60 $(fy 12); sleep 7; aparta; shot r1; atras                                          # redirecciones
  clic 60 $(fy 14); sleep 6; aparta; shot r2; atras                                          # sin longitud
  clic 60 $(fy 16); sleep 6; aparta; shot r3; atras                                          # 404
  clic 60 $(fy 18); sleep 7; aparta; shot r4; atras                                          # un archivo
  clic 60 $(fy 20); sleep 2; aparta; shot r5                                                 # javascript:
  for i in 1 2 3 4 5 6 7 8 9 10 11; do tecla tab; done; sleep 1; shot t1                     # Tab: el último enlace...
  tecla tab; sleep 0.5; tecla tab; sleep 0.5; tecla ret; sleep 7; aparta; shot t2            # ...vuelve al primero, al segundo, Intro
  tecla ctrl-esc; sleep 1; tecla a a a; sleep 0.5; tecla ret; sleep 5; aparta; shot m2       # la memoria con el Navegador
  tecla alt-f4; sleep 1.5; tecla alt-f4; sleep 3                                             # fuera los dos
  tecla ctrl-esc; sleep 1; tecla a a a; sleep 0.5; tecla ret; sleep 5; aparta; shot m1       # y la memoria después
  mon quit; } | corre
PAG="0 78 640 548"
echo "-- el menú Inicio: el grupo «Internet» con el Navegador --"
chk $T/i1.ppm busca 20 410 60 440 9 7 "Internet" busca 270 410 320 440 9 12 "Navegador"
echo "-- la entrada, por HTTPS --"
chk $T/n1.ppm busca 96 56 130 74 0 9 "https://10.0.2.2:$((P + 1))/" busca $PAG 0 9 "Escriba su usuario y su clave." \
  busca $PAG 0 9 "Usuario" busca $PAG 0 9 "Recordarme" busca 0 80 60 300 0 6 "Entrar" \
  busca 600 548 700 570 11 15 "Conexión cifrada" busca 100 574 140 592 0 8 "Entrada - Bibl"
chk $T/n2.ppm busca 70 160 110 190 0 9 "maria" busca 50 196 100 226 0 9 "*******" nobusca $PAG 0 9 "secreto"
echo "-- dentro: la lista en una tabla; lo escondido no se ve --"
chk $T/n3.ppm busca 96 56 130 74 0 9 "https://10.0.2.2:$((P + 1))/biblioteca" busca $PAG 0 8 "El niño y la cigüeña" \
  busca $PAG 0 9 "Canción «del» mar" busca $PAG 0 9 "Lluís Martí" busca $PAG 0 8 "Pilar Esteve" busca $PAG 0 9 "En la biblioteca" \
  busca $PAG 12 9 "Prestar un libro" busca $PAG 12 9 "Devolver" nobusca $PAG 0 9 "NO-DEBE" nobusca $PAG 0 9 "Texto plegado" \
  busca $PAG 0 9 "Uno" busca $PAG 0 9 "Dos" busca 0 78 40 90 12 9 "Libros"
chk $T/n4.ppm busca $PAG 0 9 "Texto plegado: 4 libros"
echo "-- prestar: buscar, elegir, rellenar --"
chk $T/p1.ppm busca 96 56 130 74 0 9 "https://10.0.2.2:$((P + 1))/prestar?qs=garc" busca $PAG 12 9 "María García" \
  nobusca $PAG 12 9 "Pepe"
chk $T/p2.ppm busca $PAG 0 9 "Para María García" busca $PAG 0 9 "El niño y la cigüeña" busca $PAG 0 9 "15 días"
chk $T/p3.ppm busca $PAG 9 12 "El niño y la cigüeña" busca $PAG 0 9 "Canción «del» mar" busca $PAG 0 9 "Recetas de la abuela" \
  busca $PAG 0 9 "Paseos por la sierra"
chk $T/p4.ppm busca 50 160 110 190 0 9 "Recetas de la abuela" busca 0 220 60 250 0 9 "hola luis" busca 0 238 60 268 0 9 "dos" \
  px 22 $(fy 12) 0 "15 días, marcado" nopx 118 $(fy 12) 0 "30 días, sin marcar" busca 90 360 200 390 7 6 "Solo JavaScript"
chk $T/p5.ppm busca 0 548 100 570 0 15 "Este botón necesita JavaScript, que el Navegador no tiene."
chk $T/p6.ppm busca 96 56 130 74 0 9 "https://10.0.2.2:$((P + 1))/biblioteca" \
  busca $PAG 237 230 "Hecho: prestado «Recetas de la abuela» a María García (15 días, con aviso)." \
  busca $PAG 0 8 "Prestado a María García"
echo "-- devolver: el confirm() de la página es un cuadro --"
chk $T/d1.ppm busca $PAG 0 9 "Recetas de la abuela (María García)" busca $PAG 0 9 "Bien" busca 100 140 260 180 0 6 "Devuelto"
chk $T/d2.ppm busca 200 240 600 300 0 8 "¿Seguro que lo han devuelto?"
chk $T/d3.ppm nobusca 200 240 600 300 0 8 "¿Seguro que lo han devuelto?" busca $PAG 0 9 "Recetas de la abuela (María García)"
chk $T/d4.ppm busca $PAG 237 230 "Hecho: devuelto «Recetas de la abuela» (bien)." busca $PAG 0 9 "No hay libros fuera."
echo "-- Atrás, Adelante y la dirección escrita a mano --"
chk $T/h1.ppm busca 96 56 130 74 0 9 "https://10.0.2.2:$((P + 1))/biblioteca" busca $PAG 0 8 "Pilar Esteve" nobusca $PAG 0 9 "Prestado a"
chk $T/h2.ppm busca 96 56 130 74 0 9 "https://10.0.2.2:$((P + 1))/devolver" busca $PAG 0 9 "No hay libros fuera."
chk $T/e0.ppm busca 96 56 130 74 9 12 "https://10.0.2.2:$((P + 1))/devolver"
chk $T/e1.ppm busca 96 56 130 74 0 9 "10.0.2.2:$P/enlaces"
chk $T/e2.ppm busca 96 56 130 74 0 9 "http://10.0.2.2:$P/enlaces" busca $PAG 12 9 "De otro nombre" busca $PAG 12 9 "Un archivo" \
  busca 600 548 760 570 13 15 "Sin cifrar"
echo "-- los certificados que no valen no se abren --"
chk $T/c1.ppm busca $PAG 0 9 "El certificado es de otro sitio" nobusca $PAG 0 9 "Escriba su usuario"
chk $T/c2.ppm busca $PAG 0 9 "ninguna autoridad conocida." nobusca $PAG 0 9 "Escriba su usuario"
chk $T/c3.ppm busca $PAG 0 9 "El certificado del servidor ha caducado" nobusca $PAG 0 9 "Escriba su usuario"
echo "-- texto sin formato (RSA), Latin-1, una página grande --"
chk $T/c4.ppm busca $PAG 0 9 "Texto sin formato." busca $PAG 0 9 "Segunda línea: ñandú." busca 600 548 700 570 11 15 "Conexión cifrada"
chk $T/c5.ppm busca $PAG 0 9 "Año, cigüeña y camión en ISO-8859-1."
chk $T/g1.ppm busca $PAG 0 9 "Párrafo número 1 de la página grande" nobusca $PAG 0 9 "FIN-DE-LA-GRANDE"
chk $T/g2.ppm busca $PAG 0 9 "FIN-DE-LA-GRANDE" busca $PAG 0 9 "Párrafo número 400 de" cambia 784 82 12 200 $T/g1.ppm 100
chk $T/g3.ppm nobusca $PAG 0 9 "FIN-DE-LA-GRANDE" busca $PAG 0 9 "Párrafo número 39"
echo "-- redirecciones, sin longitud, 404, un archivo, javascript: --"
chk $T/r1.ppm busca 96 56 130 74 0 9 "http://10.0.2.2:$P/final" busca $PAG 0 9 "Llegada tras dos redirecciones."
chk $T/r2.ppm busca $PAG 0 9 "Hasta el cierre de la conexión."
chk $T/r3.ppm busca $PAG 0 9 "Aquí no hay nada."
chk $T/r4.ppm busca $PAG 0 9 "Esta dirección no es una página: es un archivo"
chk $T/r5.ppm busca 0 548 100 570 0 15 "Este enlace no se puede abrir aquí (no es una página web)." busca $PAG 12 9 "De otro nombre"
echo "-- Tab e Intro --"
chk $T/t1.ppm busca $PAG 9 12 "Solo JavaScript"
chk $T/t2.ppm busca $PAG 0 9 "ninguna autoridad conocida."
echo "-- lo que ha recibido el servidor --"
pedido() { if grep -qF -- "$1" $T/pedidos.log; then ok "$2"; else ko "$2 (falta: $1)"; fi; }
pedido "GET / | - |  | Navegador/0.1 (Hispa-486; Hispa32) | es-ES" "la primera petición: sin galletas, con su nombre y el idioma de NAVEGA.INI"
pedido "POST /entrar | - | ficha=abc123&usuario=maria&clave=secreto&recordar=1 |" "el POST de la entrada"
pedido "GET /biblioteca | SESION=S3S10N-de-prueba |" "la galleta de sesión vuelve (y la caducada, no)"
pedido "GET /prestar?qs=garc | SESION=S3S10N-de-prueba |" "el formulario GET"
pedido "POST /prestar | SESION=S3S10N-de-prueba | ficha=abc123&socio_id=2&marca=a%C3%B1o+%C2%AB%E2%82%AC%C2%BB&libro=3&nota=hola+luis%0D%0Ados&dias=15&avisar=si&accion=prestar |" \
  "el POST del préstamo: ocultos, lista, área de dos líneas, radio, casilla y botón; UTF-8"
pedido "POST /devolver | SESION=S3S10N-de-prueba | ficha=abc123&libro=3&estado=bien |" "el POST de la devolución"
[ "$(grep -c '^POST /devolver' $T/pedidos.log)" = 1 ] && ok "con «No», la devolución no se envió" || ko "la devolución se envió $(grep -c '^POST /devolver' $T/pedidos.log) veces"
[ "$(grep -c '^POST /prestar' $T/pedidos.log)" = 1 ] && ok "el botón de JavaScript no envió nada" || ko "el préstamo se envió $(grep -c '^POST /prestar' $T/pedidos.log) veces"
grep -q "^GET /archivo.bin" $T/pedidos.log && ok "el archivo se pidió" || ko "el archivo no se pidió"
echo "-- la memoria: con el Navegador abierto y después de cerrarlo --"
chk $T/m0.ppm busca 176 480 640 498 0 6 "4 programas en marcha"
chk $T/m2.ppm busca 176 480 640 498 0 6 "5 programas en marcha" cambia $MEMZ $T/m0.ppm 50
chk $T/m1.ppm busca 176 480 640 498 0 6 "4 programas en marcha" igual $MEMZ $T/m0.ppm ${MEMZ% * *} 0
[ -n "${GUARDAR:-}" ] && cp $T/pedidos.log $T/log.txt "$GUARDAR"/ 2> /dev/null
fin
