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
#  - imágenes (PNG con sus medidas, JPEG reducido por la hoja de estilo y
#    dentro de un enlace, GIF sin medidas, un icono en la línea, data:, una
#    rota y un SVG: su texto alternativo); Mayús+Tab;
#  - subir un archivo (multipart/form-data): required y pattern lo paran; el
#    diálogo de abrir elige C:\FOTOS\TAPA.PNG; llega entero y se ve;
#  - una descarga (un PDF con Content-Disposition): sin preguntar nada, a
#    C:\HISPADOS\INTERNET\DESCARGA (la segunda vez, con otro nombre) y abierta
#    con el Lector; <meta http-equiv=refresh>;
#  - lo de la prueba de uso (docs/486/USABILIDAD-NAVEGADOR.md; /uso del
#    servidor): autofocus (se escribe sin pulsar en el campo), la marca de la
#    dirección (#resultados) al llegar y en un enlace de la misma página (y
#    Atrás sin pedirla otra vez), los campos que se ven desde la primera
#    letra, los botones (.btn) con su marco y el peligroso (btn-danger) en
#    granate y con «!», sin partirse; confirm(this.dataset.confirmar) sin
#    JavaScript; «Atrás» vuelve a la fila donde se estaba; el Tab empieza tras
#    el menú; una letra sin foco va al único campo; lo tecleado mientras llega
#    una página (y la línea de estado en ocre, «Espere»); una imagen sin
#    medidas que llega tarde (9 s) no mueve lo que se mira; imprimir la página
#    (botón «Imprimir» y Ctrl+P: lo que llega a LPT1, QEMU -parallel file);
#  - las galletas de «recordarme» se guardan al cerrar (GALLETAS.TXT, sin las
#    de sesión ni las caducadas) y, al abrirlo otra vez, se entra sin clave;
#  - se cierra y no queda memoria.
#  (GUARDAR=carpeta deja las capturas y el registro del servidor; MEDIR=1
#  además captura el Administrador de tareas con la página de la biblioteca
#  abierta, mb, y con /grande?n=720 —121 KB de HTML—, mg; ver NAVEGADOR.md.)
# (Con JavaScript=NO en NAVEGA.INI: así se prueba el Navegador sin guiones;
# el JavaScript lo prueba tests/run_navegador_js_tests.sh.)
# Browser test (offline, against a local HTTPS test server).
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
make -s build/NAVEGA.EXE build/HSPJS.DLL build/HSPTLS.DLL build/WSOCK32.DLL build/MSVCRT.DLL build/HW32.COM build/LECTOR.HVP > /dev/null 2>&1 || {
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
cp $B/MSVCRT.DLL $B/WSOCK32.DLL $B/HSPTLS.DLL $B/HSPJS.DLL $B/NAVEGA.EXE $T/RAIZ.CER $T/c/HISPADOS/INTERNET/
cp $B/HW32.COM $B/MSVCRT.DLL $T/c/HISPADOS/
mkdir -p $T/c/HISPADOS/PROGS $T/c/FOTOS
cp $B/LECTOR.HVP $T/c/HISPADOS/PROGS/
# la foto que se sube: un PNG naranja de 64x48 (el mismo escritor que el servidor)
python3 -c "
import sys; sys.argv = ['', '$T', '0']
exec(open('tests/tools/nav_servidor.py').read().split('class Pedido')[0])
open('$T/c/FOTOS/TAPA.PNG', 'wb').write(png(64, 48, (255, 153, 0)))"
printf 'Inicio=https://10.0.2.2:%d/\r\nIdioma=es-ES\r\nJavaScript=NO\r\n' $((P + 1)) > $T/c/HISPADOS/INTERNET/NAVEGA.INI
python3 tests/tools/mkhdd.py $T/c.img 16 $T/c/HISPADOS $T/c/FOTOS > /dev/null
corre() {
  rm -f $T/mon
  timeout ${TMAX:-950} qemu-system-i386 ${QEMU_PANTALLA--vga std -global VGA.xres=800 -global VGA.yres=600} -display none -serial stdio -monitor unix:$T/mon,server,nowait \
    -parallel file:$T/lpt.bin -m 16 -drive file=$T/f.img,format=raw,if=floppy,index=0 -drive file=$T/c.img,format=raw,if=ide,index=0 \
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
  if [ -n "${MEDIR:-}" ]; then                                                               # MEDIR=1: la memoria con la biblioteca
    tecla ctrl-esc; sleep 1; tecla a a a; sleep 0.5; tecla ret; sleep 5; aparta; shot mb; tecla alt-f4; sleep 1.5
    tecla f6; sleep 1; tecla 1 0 dot 0 dot 2 dot 2 shift-semicolon; for c in $(echo $P | sed 's/./& /g'); do tecla $c; done
    tecla slash g r a n d e shift-slash n equal 7 2 0 ret; sleep 20; aparta; shot mg0     # y con 121 KB de HTML
    tecla ctrl-esc; sleep 1; tecla a a a; sleep 0.5; tecla ret; sleep 5; aparta; shot mg; tecla alt-f4; sleep 1.5
    atras 8
  fi
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
  clic 60 $(fy 18); sleep 7; aparta; shot r4; tecla esc; sleep 2; shot r4b                   # un archivo: guardarlo (no)
  clic 60 $(fy 20); sleep 2; aparta; shot r5                                                 # javascript:
  for i in 1 2 3 4 5 6 7 8 9 10 11; do tecla tab; done; sleep 1; shot t1                     # Tab: el último enlace...
  tecla shift-tab; sleep 1; shot t3; tecla tab; sleep 0.5                                    # (Mayús+Tab: el de antes)
  tecla tab; sleep 0.5; tecla tab; sleep 0.5; tecla ret; sleep 7; aparta; shot t2            # ...vuelve al primero, al segundo, Intro
  dir_https() { tecla f6; sleep 1; tecla h t t p s shift-semicolon slash slash 1 0 dot 0 dot 2 dot 2 shift-semicolon
    for c in $(echo $((P + 1)) | sed 's/./& /g'); do tecla $c; done; tecla slash; tecla "$@"; tecla ret; }
  dir_https i m a g e n e s; sleep 25; aparta; shot im1                                     # las imágenes
  clic 30 $(fy 8); sleep 6; aparta; shot im2; atras 10; aparta; shot im3                   # la portada es un enlace; y al volver, de lo ya visto
  dir_https p o r t a d a; sleep 8; aparta; shot u0                                         # subir: el formulario
  clic 40 $(fy 7); sleep 3; shot u1; tecla ret; sleep 2                                     # sin archivo: required
  clic 340 $(fy 3); sleep 3; aparta; shot u2                                                        # «Examinar...»: el diálogo de abrir
  tecla shift-c shift-semicolon backslash shift-f shift-o shift-t shift-o shift-s backslash shift-t shift-a shift-p shift-a dot shift-p shift-n shift-g
  sleep 0.5; tecla ret; sleep 3; aparta; shot u3
  clic 150 $(fy 5); tecla a b; clic 40 $(fy 7); sleep 3; shot u4; tecla ret; sleep 2        # «ab»: el pattern lo para
  clic 150 $(fy 5); tecla end c; clic 40 $(fy 7); sleep 15; aparta; shot u5                 # «abc»: se sube y se ve
  dir_https r e c i b o dot p d f; sleep 16; shot s3                                        # el recibo: derecho al Lector
  tecla alt-f4; sleep 3; aparta
  dir_https r e c i b o dot p d f; sleep 16; shot s4                                        # otra vez: con otro nombre, sin preguntar
  tecla alt-f4; sleep 3; aparta
  dir_https r e f r e s c a; sleep 10; aparta; shot f1                                      # <meta http-equiv=refresh>
  # ---- la prueba de uso ----
  dir_https u s o; sleep 9; shot w0                                                         # autofocus, campos, botones
  tecla t r e n; sleep 0.5; shot w1; tecla ret; sleep 9; shot w2                            # sin pulsar en el campo; a #resultados
  aparta; tecla pgdn; sleep 1; shot a1                                                       # un poco más abajo...
  tecla tab; sleep 1; tecla ret; sleep 7; atras 9; aparta; shot a2                          # ...a otra página y Atrás: ahí mismo
  tecla home; sleep 1.5; shot w3
  clic 300 $(fy 12); sleep 3; aparta; shot w5                          # «Ir a los resultados» (la misma página)
  atras 3; aparta; shot w6                                                                  # Atrás: arriba, sin pedirla otra vez
  clic 60 $(fy 14); sleep 3; shot w7; tecla esc; sleep 2                  # «Perdido de verdad»: la pregunta; Esc, no
  clic 60 $(fy 14); sleep 3; tecla ret; sleep 7; aparta; shot w8          # Intro: sí
  dir_https u s o slash t a b; sleep 8; aparta; tecla tab; sleep 1; shot x1                 # el Tab empieza tras el menú
  tecla f5; sleep 8; aparta; tecla h o l a; sleep 1; shot x2                                # una letra sin foco: al único campo
  tecla tab; sleep 0.5; tecla ret; sleep 0.5; tecla a b c ret; shot x3; sleep 14; aparta; shot x4   # tecleado mientras llega
  dir_https u s o slash s a l t a; sleep 4; ir 320 500; aparta; shot y0; sleep 14; shot y1               # la imagen que llega tarde
  tecla home; sleep 1.5; shot y2
  dir_https u s o slash r e c i b o; sleep 8; aparta; clic 385 40; sleep 6; shot z1           # «Imprimir»
  tecla ctrl-p; sleep 6; shot z2                                                             # y Ctrl+P
  tecla ctrl-esc; sleep 1; tecla a a a; sleep 0.5; tecla ret; sleep 5; aparta; shot m2       # la memoria con el Navegador
  tecla alt-f4; sleep 1.5; tecla alt-f4; sleep 4                                             # fuera los dos (las galletas, al disco)
  tecla ctrl-esc; sleep 1; tecla i; sleep 0.5; tecla right; sleep 1; tecla ret; sleep 12; aparta; shot k1   # otra vez: dentro sin clave
  tecla alt-f4; sleep 4
  tecla ctrl-esc; sleep 1; tecla a a a; sleep 0.5; tecla ret; sleep 5; aparta; shot m1       # y la memoria después
  mon quit; } | corre
mcopy -n -i $T/c.img@@32256 ::HISPADOS/INTERNET/GALLETAS.TXT $T/galletas.txt 2> /dev/null
mcopy -n -i $T/c.img@@32256 ::HISPADOS/INTERNET/DESCARGA/RECIBO20.PDF $T/recibo.pdf 2> /dev/null
mcopy -n -i $T/c.img@@32256 ::HISPADOS/INTERNET/DESCARGA/RECIBO01.PDF $T/recibo1.pdf 2> /dev/null
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
chk $T/p5.ppm busca 0 548 100 570 0 15 "Este botón necesita JavaScript, que está apagado (NAVEGA.INI)."
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
chk $T/r1.ppm busca 96 56 130 74 0 9 "http://10.0.2.2:$P/final" busca $PAG 0 9 "Llegada tras dos redirecciones." \
  busca $PAG 0 9 "Últimos 33,00 € - Àngel, Òscar, Íñigo, Ïsona, Ëlia" \
  busca $PAG 0 9 "São João: Ã Õ Â Ê Ô Û Î È Ì Ù Ú Ó - Œuvre, cœur © ® €…"     # [GLIFOS] Á, €... (tools/hispa437.py)
chk $T/r2.ppm busca $PAG 0 9 "Hasta el cierre de la conexión."
chk $T/r3.ppm busca $PAG 0 9 "Aquí no hay nada."
chk $T/r4.ppm titulo 222 136 activa "Guardar la descarga" busca 0 548 600 570 0 15 "Descarga: ARCHIVO.BIN (147 K). Elija dónde guardarla."
chk $T/r4b.ppm busca 0 548 100 570 0 15 "No se ha guardado la descarga." busca $PAG 12 9 "De otro nombre"
chk $T/r5.ppm busca 0 548 100 570 0 15 "Este enlace no se puede abrir aquí (no es una página web)." busca $PAG 12 9 "De otro nombre"
echo "-- Tab, Mayús+Tab e Intro --"
chk $T/t1.ppm busca $PAG 9 12 "Solo JavaScript"
chk $T/t3.ppm busca $PAG 9 12 "Un archivo" nobusca $PAG 9 12 "Solo JavaScript"
chk $T/t2.ppm busca $PAG 0 9 "ninguna autoridad conocida."
echo "-- lo que ha recibido el servidor --"
pedido() { if grep -qF -- "$1" $T/pedidos.log; then ok "$2"; else ko "$2 (falta: $1)"; fi; }
pedido "GET / | - |  | HispaNavegador/0.3 (Hispa-486) | es-ES | text/html" "la primera petición: sin galletas, con su nombre y el idioma de NAVEGA.INI"
pedido "POST /entrar | - | ficha=abc123&usuario=maria&clave=secreto&recordar=1 |" "el POST de la entrada"
pedido "GET /biblioteca | SESION=S3S10N-de-prueba; RECUERDA=R3CU3RDA; PREF=azul |" "la galleta de sesión vuelve (y la caducada, no)"
pedido "GET /prestar?qs=garc | SESION=S3S10N-de-prueba; RECUERDA=R3CU3RDA; PREF=azul |" "el formulario GET"
pedido "POST /prestar | SESION=S3S10N-de-prueba; RECUERDA=R3CU3RDA; PREF=azul | ficha=abc123&socio_id=2&marca=a%C3%B1o+%C2%AB%E2%82%AC%C2%BB&libro=3&nota=hola+luis%0D%0Ados&dias=15&avisar=si&accion=prestar |" \
  "el POST del préstamo: ocultos, lista, área de dos líneas, radio, casilla y botón; UTF-8"
pedido "POST /devolver | SESION=S3S10N-de-prueba; RECUERDA=R3CU3RDA; PREF=azul | ficha=abc123&libro=3&estado=bien&_cg_sin_js=1 |" "el POST de la devolución (con JavaScript=NO, el <noscript> del formulario cuenta)"
[ "$(grep -c '^POST /devolver' $T/pedidos.log)" = 1 ] && ok "con «No», la devolución no se envió" || ko "la devolución se envió $(grep -c '^POST /devolver' $T/pedidos.log) veces"
[ "$(grep -c '^POST /prestar' $T/pedidos.log)" = 1 ] && ok "el botón de JavaScript no envió nada" || ko "el préstamo se envió $(grep -c '^POST /prestar' $T/pedidos.log) veces"
grep -q "^GET /archivo.bin" $T/pedidos.log && ok "el archivo se pidió" || ko "el archivo no se pidió"
echo "-- la memoria: con el Navegador abierto y después de cerrarlo --"
chk $T/m0.ppm busca 176 480 640 498 0 6 "4 programas en marcha"
chk $T/m2.ppm busca 176 480 640 498 0 6 "5 programas en marcha" cambia $MEMZ $T/m0.ppm 50
chk $T/m1.ppm busca 176 480 640 498 0 6 "4 programas en marcha" igual $MEMZ $T/m0.ppm ${MEMZ% * *} 0
echo "-- imágenes --"
# (los colores, del cubo de la paleta: rojo 196, verde 34, amarillo 226,
# azul 21, magenta 201, cian 51, naranja 214)
IMGS="px 50 168 196 logo-png-rojo px 100 182 196 logo-96x36 nopx 110 168 196 logo-acaba px 35 228 34 jpeg-verde px 35 270 226 jpeg-amarillo
  nopx 70 270 226 jpeg-56-de-ancho px 30 340 21 gif-azul px 60 392 201 icono-png nopx 66 392 201 icono-transparente px 197 393 51 data-png"
chk $T/im1.ppm $IMGS busca $PAG 0 9 "Una portada con enlace" busca $PAG 0 9 "sin medidas" busca $PAG 7 9 "[ROTA]" busca $PAG 7 9 "[SVG]" \
  busca $PAG 0 9 "FIN-IMAGENES" nobusca $PAG 7 9 "[Logo]" nobusca $PAG 7 9 "[Azul]"
chk $T/im2.ppm busca $PAG 0 9 "Llegada tras dos redirecciones."
chk $T/im3.ppm $IMGS
[ "$(grep -c '^GET /tapa.jpg' $T/pedidos.log)" = 1 ] && ok "al volver, las imágenes salen de lo ya visto (no se piden otra vez)" || ko "la portada se pidió $(grep -c '^GET /tapa.jpg' $T/pedidos.log) veces"
pedido "GET /logo.png | SESION=S3S10N-de-prueba; RECUERDA=R3CU3RDA; PREF=azul |  | HispaNavegador/0.3 (Hispa-486) | es-ES | image/png" "las imágenes se piden con las galletas y diciendo que son imágenes"
grep -q "^GET /data:\|^GET /dentro" $T/pedidos.log && ko "la imagen data: se pidió al servidor" || ok "la imagen data: no se pide (va en la página)"
echo "-- subir un archivo (multipart/form-data) --"
chk $T/u0.ppm busca $PAG 7 9 "(ningún archivo)"
chk $T/u1.ppm busca 150 150 650 450 0 8 "Falta elegir el archivo de «Portada»."
chk $T/u2.ppm titulo 222 136 activa "Elegir el archivo que se sube" busca 0 0 800 600 0 8 "Nombre del archivo:"
chk $T/u3.ppm busca $PAG 0 9 "C:\\FOTOS\\TAPA.PNG"
chk $T/u4.ppm busca 150 150 650 450 0 8 "No tiene la forma que se pide «Título»." busca 150 150 650 450 0 8 "Solo letras, tres o más."
TAM=$(stat -c %s $T/c/FOTOS/TAPA.PNG); SHA=$(sha1sum $T/c/FOTOS/TAPA.PNG | cut -c1-8)
chk $T/u5.ppm busca $PAG 237 230 "Subida: TAPA.PNG ($TAM octetos, image/png)." px 40 195 214 "la-subida-se-ve"
pedido "POST /portada | SESION=S3S10N-de-prueba; RECUERDA=R3CU3RDA; PREF=azul | [multipart] ficha=abc123&portada=TAPA.PNG:image/png:$TAM:$SHA&titulo=abc |" \
  "el POST multipart: el campo oculto, el archivo entero (tamaño y SHA-1) y el texto"
[ "$(grep -c '^POST /portada' $T/pedidos.log)" = 1 ] && ok "required y pattern pararon los otros envíos" || ko "la portada se envió $(grep -c '^POST /portada' $T/pedidos.log) veces"
pedido "GET /subida.img | SESION=" "la imagen subida se pidió para verla"
echo "-- una descarga: sin preguntar, a DESCARGA y abierta con el Lector --"
chk $T/s3.ppm titulo 40 32 activa "Lector - C:\\HISPADOS\\INTERNET\\DESCARGA\\RECIBO20.PDF"
chk $T/s4.ppm titulo 40 32 activa "Lector - C:\\HISPADOS\\INTERNET\\DESCARGA\\RECIBO01.PDF"
python3 -c "
import sys; sys.argv = ['', '$T', '0']
exec(open('tests/tools/nav_servidor.py').read().split('class Pedido')[0])
sys.exit(0 if open('$T/recibo.pdf', 'rb').read() == PDF and open('$T/recibo1.pdf', 'rb').read() == PDF else 1)" 2> /dev/null && ok "DESCARGA\\RECIBO20.PDF y RECIBO01.PDF son el PDF que dio el servidor" || ko "DESCARGA\\RECIBO20.PDF o RECIBO01.PDF no están o no son el del servidor"
echo "-- <meta http-equiv=refresh> --"
chk $T/f1.ppm busca 96 56 130 74 0 9 "https://10.0.2.2:$((P + 1))/final" busca $PAG 0 9 "Llegada tras dos redirecciones."
grep -A1 "^GET /refresca" $T/pedidos.log | tail -1 | grep -q "^GET /final" && ok "el refresco pidió /final" || ko "el refresco no pidió /final"
echo "-- las galletas de «recordarme», guardadas al cerrar --"
if [ -f $T/galletas.txt ]; then
  grep -q "RECUERDA	R3CU3RDA" $T/galletas.txt && grep -q "PREF	azul" $T/galletas.txt && ok "GALLETAS.TXT tiene las que caducan (Max-Age y Expires)" || ko "GALLETAS.TXT: faltan RECUERDA o PREF"
  grep -q "SESION\|caduca" $T/galletas.txt && ko "GALLETAS.TXT tiene la de sesión o la caducada" || ok "GALLETAS.TXT no tiene la de sesión ni la caducada"
else ko "no se ha escrito GALLETAS.TXT"; fi
pedido "GET / | RECUERDA=R3CU3RDA; PREF=azul |" "al abrirlo otra vez: con las galletas guardadas (sin la de sesión)"
grep -q "caduca=1" $T/pedidos.log && ko "se envió la galleta caducada (Expires de 2015)" || ok "la galleta caducada (Expires de 2015) no se envió nunca"
chk $T/k1.ppm busca 96 56 130 74 0 9 "https://10.0.2.2:$((P + 1))/biblioteca" busca $PAG 0 8 "Pilar Esteve"
echo "-- la prueba de uso: autofocus, marcas, campos y botones --"
chk $T/w0.ppm busca $PAG 0 9 "Real Academia Española / Alfaguara" busca $PAG 0 9 "2015" \
  busca $PAG 0 6 "Reseñas de los lectores" busca $PAG 0 6 "Prestar" busca $PAG 13 6 "! Dar por perdido" busca $PAG 13 6 "! Perdido de verdad"
chk $T/w1.ppm busca $PAG 0 9 "tren"
pedido "GET /uso?q=tren |" "autofocus: lo escrito sin pulsar en el campo se ha enviado"
chk $T/w2.ppm busca 96 56 130 74 0 9 "https://10.0.2.2:$((P + 1))/uso?q=tren#resultados" busca 0 78 640 135 0 9 "RESULTADO: tren" \
  nobusca $PAG 0 9 "Prueba de uso"
chk $T/a2.ppm igual 0 80 640 460 $T/a1.ppm 0 80 0
chk $T/w5.ppm busca 0 78 640 135 0 9 "RESULTADO: tren" nobusca $PAG 0 9 "Prueba de uso"
chk $T/w6.ppm busca $PAG 0 6 "Buscar" busca $PAG 0 9 "Relleno 01" nobusca $PAG 0 9 "RESULTADO"
[ "$(grep -c '^GET /uso?q=tren |' $T/pedidos.log)" = 2 ] && ok "la marca de la misma página y Atrás no piden la página otra vez" || ko "GET /uso?q=tren se pidió $(grep -c '^GET /uso?q=tren |' $T/pedidos.log) veces (2: al buscar y al volver de /final)"
chk $T/w7.ppm busca 150 150 650 450 0 8 "¿Seguro que se ha perdido? No tiene vuelta."
chk $T/w8.ppm busca $PAG 0 9 "DADO-POR-PERDIDO"
[ "$(grep -c '^POST /uso/perdido' $T/pedidos.log)" = 1 ] && ok "confirm(this.dataset.confirmar): con Esc no se envió; con Intro, sí" || ko "POST /uso/perdido: $(grep -c '^POST /uso/perdido' $T/pedidos.log) veces"
echo "-- el Tab, las letras sin foco y lo tecleado mientras llega una página --"
chk $T/x1.ppm busca $PAG 9 12 "Primero del contenido" nobusca $PAG 9 12 "Menú 1"
chk $T/x2.ppm busca $PAG 0 9 "hola"
chk $T/x3.ppm busca 600 548 800 570 0 10 "Espere, por favor"
chk $T/x4.ppm busca $PAG 0 9 "COD: abc"
pedido "GET /uso/lenta2?cod=abc |" "lo tecleado mientras llegaba (abc e Intro) fue al campo con autofocus"
echo "-- una imagen sin medidas que llega tarde no mueve lo que se mira --"
chk $T/y0.ppm busca 0 112 640 135 12 9 "Enlace que no se mueve"
chk $T/y1.ppm busca 0 112 640 135 12 9 "Enlace que no se mueve" nobusca $PAG 0 9 "Arriba del todo."
chk $T/y2.ppm busca 0 78 640 100 0 9 "Arriba del todo." nobusca 0 112 640 135 12 9 "Enlace que no se mueve" busca $PAG 12 9 "Enlace que no se mueve"
echo "-- imprimir la página (LPT1) --"
chk $T/z1.ppm busca 0 548 300 570 0 15 "Página impresa."
python3 - "$T/lpt.bin" << 'FIN_PY' && ok "lo impreso: ESC @, ESC t 1, el texto en la 437 (con lo de los campos) y el salto de hoja, dos veces" || ko "lo impreso no es lo que se esperaba"
import sys
d = open(sys.argv[1], 'rb').read()
ok = d.count(b'\x1b@\x1bt\x01\x1b2') == 2 and d.count('Recibo de préstamo'.encode('cp437')) == 2 \
    and d.count('María García'.encode('cp437')) == 2 and d.count(b'[con prisa') == 2 and d.count(b'FIN-DEL-RECIBO') == 2 \
    and d.endswith(b'\x0c') and b'\x1bE' in d
sys.exit(0 if ok else 1)
FIN_PY
[ -n "${GUARDAR:-}" ] && cp $T/pedidos.log $T/log.txt $T/galletas.txt $T/lpt.bin "$GUARDAR"/ 2> /dev/null
fin
