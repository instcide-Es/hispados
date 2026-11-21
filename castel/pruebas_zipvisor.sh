# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# pruebas_zipvisor.sh - las partes 27 (SOLO=zip, Comprimidos) y 28
# (SOLO=mirador, el Mirador) de pruebas.sh, que lee este archivo («.»): las
# funciones parte_zip y parte_mirador, con los ayudantes de pruebas.sh (ok,
# ko, mira, miracap, saca, sano) y sus carpetas ($AQUI, $P, $G).
# Las posiciones, las del escritorio a 800 x 600: Mi equipo en (60, 40), la
# unidad C: en (330, 160) y, en ella, los iconos de cinco en cinco desde
# (260, 160), 96 puntos a lo ancho y 78 a lo alto.

# un ZIP de lo de Linux, con una carpeta, un nombre largo con eñe, uno de
# azar y uno de texto; y uno con contraseña (ZipCrypto) y uno malo («..»)
zip_de_prueba() {
  local d=$1
  rm -rf $d/src && mkdir -p $d/src/sub
  seq 1 3000 > $d/src/NUMEROS.TXT
  printf 'hola desde Linux\r\n' > "$d/src/sub/Informe largo ñandú.txt"
  head -c 5000 /dev/urandom > $d/src/AZAR.BIN
  rm -f $d/LINUX.ZIP $d/ZC.ZIP
  (cd $d/src && zip -qr ../LINUX.ZIP . && zip -qr -P secreta ../ZC.ZIP .)
  python3 -I $AQUI/nucleo32/comprim/prueba/haz_casos.py $d/casos > /dev/null 2>&1
  cp $d/casos/subir.zip $d/MALO.ZIP
}

parte_zip() {
echo "== Hispa-Pentium: Comprimidos (COMPRIM.BIN, ZIP y UNZIP) =="
local Y=$P/zip S
mkdir -p $Y $G
# 1. el motor en el anfitrión (con el zip y el unzip de Linux, los casos
# malos y el fuzz con ASan): prueba/prueba.sh
if command -v zip > /dev/null && command -v unzip > /dev/null; then
  SIN_GRANDE=${SIN_GRANDE:-} bash $AQUI/nucleo32/comprim/prueba/prueba.sh ${FUZZ_ZIP:-1500} > $Y/anfitrion.txt 2>&1
  while IFS= read -r l; do
    case "$l" in "  OK   "*) ok "anfitrión: ${l#  OK   }" ;; "  FAIL "*) ko "anfitrión: ${l#  FAIL }" ;; esac
  done < $Y/anfitrion.txt
else ko "faltan zip y unzip en el anfitrión"; fi
# 2. en Hispa-Pentium
if ! bash $AQUI/nucleo32/comprim/hacer.sh $Y/BIN > $Y/hacer.txt 2>&1; then ko "no se puede hacer COMPRIM.BIN (¿gcc con -m32?): $(tail -n 2 $Y/hacer.txt)"; return; fi
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/semilla.cas -o $Y/semilla.asm > /dev/null && nasm -f bin $Y/semilla.asm -o $Y/SEMILLA.BIN || { ko "semilla.cas no compila"; return; }
$AQUI/nucleo32/disco.sh $Y/base.img 32 > $Y/disco.txt 2>&1 || { ko "disco.sh no hace el disco: $(tail -n 2 $Y/disco.txt)"; return; }
zip_de_prueba $Y
export MTOOLS_SKIP_CHECK=1
cp $Y/base.img $Y/c.img
mcopy -i $Y/c.img $Y/BIN/COMPRIM.BIN $Y/BIN/ZIP.BIN $Y/BIN/UNZIP.BIN $Y/LINUX.ZIP $Y/ZC.ZIP $Y/MALO.ZIP ::
# 2a. las órdenes, en el intérprete
ESCRIBE=1 DISCO=$Y/c.img ARRANQUE=ordenes MODULOS= MEMORIA=64 $AQUI/nucleo32/probar.sh $Y/SEMILLA.BIN "unzip -l linux.zip" ":pausa 2" \
  "unzip -d sale linux.zip" ":pausa 3" "dir sale\\sub" "zip nuevo.zip sale" ":pausa 3" "unzip -t nuevo.zip" ":pausa 2" \
  "unzip -t zc.zip" ":pausa 1" ":escribe mala" ":tecla ret" ":pausa 3" "unzip -q -P secreta -d conclave zc.zip" ":pausa 4" \
  "unzip -d malo malo.zip" ":pausa 2" "zip -d nuevo.zip SALE/AZAR.BIN" ":pausa 3" "unzip -l nuevo.zip" ":pausa 2" > $Y/ordenes.sal 2>&1
tr -d '\r' < $Y/ordenes.sal > $Y/ordenes.txt
S=$Y/ordenes.txt
mira "UNZIP -l: lo que lleva un ZIP del zip de Linux, con el nombre largo y la eñe" "^      Tamaño  Comprimido  Fecha      Hora   Nombre$" \
  "^ *18  .*  sub/Informe largo ñandú.txt$" "^ *13893  *[0-9]*  .*  NUMEROS.TXT$" "  4 archivos$"
mira "UNZIP -d SALE: lo saca, y el nombre largo queda de 8+3 (INFORM~1.TXT)" "^Sacados 4 de linux.zip$" "^INFORM~1 TXT  *18 "
mira "ZIP: mete una carpeta con lo de dentro, y UNZIP -t lo prueba (CRC)" "^  metiendo: SALE/SUB/INFORM~1.TXT$" "^No hay errores en nuevo.zip$"
mira "UNZIP con contraseña (ZipCrypto): la pregunta si no se da (con asteriscos) y dice si no es esa" "^Contraseña de zc.zip: \*\*\*\*$" \
  "probando: NUMEROS.TXT  *hace falta la contraseña (o no es esa)$"
mira "un ZIP con «../fuera.txt»: lo bueno sale y lo peligroso no" "^  NO SE SACA: ../fuera.txt$" "^unzip: 1 no se han podido sacar (el último, ../fuera.txt: nombre peligroso"
mira "ZIP -d: quitar uno" "^  quitando: SALE/AZAR.BIN$"
rm -rf $Y/out && mkdir -p $Y/out
mcopy -s -n -i $Y/c.img ::SALE ::CONCLAVE ::NUEVO.ZIP $Y/out/ > /dev/null 2>&1
if cmp -s $Y/out/SALE/NUMEROS.TXT $Y/src/NUMEROS.TXT && cmp -s $Y/out/SALE/AZAR.BIN $Y/src/AZAR.BIN && cmp -s "$Y/out/SALE/SUB/INFORM~1.TXT" "$Y/src/sub/Informe largo ñandú.txt" \
   && cmp -s $Y/out/CONCLAVE/NUMEROS.TXT $Y/src/NUMEROS.TXT; then ok "lo sacado en Hispa-Pentium (sin clave y con ella) es igual octeto a octeto que lo de Linux"
else ko "lo sacado no es igual:"; ls -R $Y/out | head -8 | sed 's/^/        /'; fi
if unzip -tq $Y/out/NUEVO.ZIP > /dev/null 2>&1 && (cd $Y/out && unzip -qq -o NUEVO.ZIP -d z && cmp -s z/SALE/NUMEROS.TXT ../src/NUMEROS.TXT); then
  ok "el ZIP hecho en Hispa-Pentium lo da por bueno el unzip de Linux, y saca lo mismo"
else ko "el ZIP de Hispa-Pentium no lo lee bien Linux: $(unzip -tq $Y/out/NUEVO.ZIP 2>&1 | tail -1)"; fi
unzip -l $Y/out/NUEVO.ZIP 2>/dev/null | grep -q "SALE/AZAR.BIN" && ko "ZIP -d no lo ha quitado" || ok "ZIP -d: ya no está en el ZIP (lo mira el unzip de Linux)"
if sano $Y/c.img; then ok "y el disco, sano (fsck.fat)"; else ko "el disco ha quedado mal:"; sed 's/^/        /' $P/fsck.txt | head -4; fi
# 2b. la ventana, desde el explorador: LINUX.ZIP (dos clics), una carpeta
# del ZIP, Extraer todo, Quitar, Añadir, Propiedades, arrastrar uno al
# tapiz, otro ZIP (con contraseña), y en el explorador «Comprimir en ZIP»
cp $Y/base.img $Y/g.img
mmd -i $Y/g.img ::CARTAS
printf 'Querida Ana:\r\n' > $Y/ANA.TXT; printf 'Querido Juan:\r\n' > $Y/JUAN.TXT
mcopy -i $Y/g.img $Y/ANA.TXT $Y/JUAN.TXT ::CARTAS/
mcopy -i $Y/g.img $Y/BIN/COMPRIM.BIN $Y/LINUX.ZIP $Y/ZC.ZIP ::
SES=(":pausa 2" ":doble 60 40" ":pausa 3" ":doble 330 160" ":pausa 2" ":doble 644 316" ":pausa 3" ":raton 790 590" ":captura z1"
 ":doble 100 181" ":pausa 1" ":raton 790 590" ":captura z2" ":tecla backspace" ":pausa 1"
 ":clic 108 94" ":pausa 1" ":captura z3" ":tecla ret" ":pausa 3" ":captura z4" ":tecla esc" ":pausa 1"
 ":clic 120 201" ":clic 490 94" ":pausa 1" ":captura z5" ":tecla ret" ":pausa 3"
 ":clic 395 94" ":pausa 1" ":escribe C:\\LEEME.TXT" ":tecla ret" ":pausa 3" ":raton 790 590" ":captura z6"
 ":clicd 120 201" ":pausa 1" ":captura z7" ":tecla up" ":tecla ret" ":pausa 2" ":captura z8" ":tecla ret" ":pausa 1"
 ":arrastra 120 221 100 545" ":pausa 3"
 ":clicd 400 400" ":pausa 1" ":captura z9" ":clic 440 493" ":pausa 1" ":escribe C:\\ZC.ZIP" ":tecla ret" ":pausa 3"
 ":clic 108 94" ":pausa 1" ":tecla ret" ":pausa 2" ":captura z10" ":escribe secreta" ":tecla ret" ":pausa 4" ":captura z11" ":tecla esc" ":pausa 1"
 ":clic 750 61" ":pausa 2" ":clicd 260 160" ":pausa 1" ":captura z12" ":clic 300 400" ":pausa 5" ":raton 790 590" ":captura z13")
CAPTURAS=$G ESCRIBE=1 DISCO=$Y/g.img ARRANQUE=escritorio MODULOS= MEMORIA=96 $AQUI/nucleo32/probar.sh $Y/SEMILLA.BIN "${SES[@]}" > $Y/ventana.sal 2>&1
S=$Y/ventana.sal
mira "dos clics en LINUX.ZIP, en el explorador, lo abren en Comprimidos (la clase ZIP, del paquete)" "^comprim: abierto C:.LINUX.ZIP (4 archivos, 0 peligrosos, 0 con clave)$"
miracap "la ventana: la barra de herramientas, la ruta del ZIP, las columnas y las carpetas delante" z1 negrita 40 40 300 70 "LINUX.ZIP - Comprimidos" \
  negrita 40 80 200 110 "Extraer todo" texto 40 120 300 150 'C:\LINUX.ZIP' negrita 60 150 200 170 Nombre texto 60 170 200 195 sub texto 60 190 200 215 AZAR.BIN \
  texto 400 170 520 195 "1 dentro"
miracap "dos clics en una carpeta del ZIP: se entra (el nombre largo con su eñe, de la página 437)" z2 texto 40 120 400 150 'C:\LINUX.ZIP\sub' texto 60 170 400 195 "Informe largo ñandú.txt"
miracap "«Extraer todo»: pregunta dónde (al lado del ZIP, con su nombre)" z3 negrita 140 220 400 245 "Extraer todo" texto 160 280 400 305 'C:\LINUX'
miracap "y lo saca: lo dice y ofrece abrir la carpeta" z4 texto 160 250 500 275 'Sacados 4 a C:\LINUX.' texto 160 280 500 310 "¿Abrir la carpeta?"
mira "Extraer todo: los 4 a C:\\LINUX" "^comprim: sacados 4 a C:.LINUX (0 ya estaban, 0 fallos: bien)$"
miracap "«Quitar»: pregunta antes" z5 negrita 160 220 400 245 Quitar texto 160 250 600 290 "¿Quitar del ZIP lo elegido"
miracap "«Añadir...»: C:\\LEEME.TXT, dentro (y AZAR.BIN, quitado)" z6 texto 60 190 300 215 LEEME.TXT notexto 40 170 760 260 AZAR.BIN texto 40 490 300 510 "Añadidos 1."
miracap "el botón secundario en uno: Abrir, Extraer..., Quitar del ZIP, Propiedades" z7 texto 100 200 300 260 Abrir texto 100 230 300 300 "Extraer..." texto 100 260 300 360 Propiedades
miracap "Propiedades: lo que mide, lo comprimido, cómo y su CRC" z8 negrita 160 200 400 230 Propiedades texto 160 240 600 300 "Guardado con" texto 160 280 600 330 "CRC-32:"
mira "arrastrar NUMEROS.TXT al tapiz: se saca al lado del ZIP y el escritorio le pone su acceso" "^comprim: sacados 1 a C:\\\\ (0 ya estaban" \
  "^escritorio: un acceso nuevo en el tapiz: C:.NUMEROS.TXT"
miracap "el botón secundario en lo vacío: Extraer todo, Añadir, Nuevo ZIP, Abrir otro ZIP" z9 texto 400 400 600 520 "Abrir otro ZIP..."
miracap "otro ZIP con contraseña: «Extraer todo» la pide (con asteriscos)" z10 negrita 140 220 400 250 "Contraseña" texto 160 250 600 300 "Escriba su contraseña"
mira "con la contraseña buena (ZipCrypto), lo saca" "^comprim: abierto C:.ZC.ZIP (4 archivos, 0 peligrosos, 3 con clave)$" "^comprim: sacados 4 a C:.ZC (0 ya estaban, 0 fallos: bien)$"
miracap "el explorador: «Comprimir en ZIP» en el menú del botón secundario" z12 texto 260 160 500 420 "Comprimir en ZIP"
mira "«Comprimir en ZIP» en la carpeta CARTAS: CARTAS.ZIP, al lado" "^comprim: hecho C:.CARTAS.ZIP con 3 archivos y carpetas: bien$"
miracap "y el explorador lo ve en cuanto acaba (vuelve a mirar la carpeta)" z13 texto 200 150 780 480 CARTAS.ZIP
rm -rf $Y/out2 && mkdir -p $Y/out2
mcopy -s -n -i $Y/g.img ::LINUX ::LINUX.ZIP ::CARTAS.ZIP ::ZC ::NUMEROS.TXT ::LEEME.TXT $Y/out2/ > /dev/null 2>&1
if cmp -s $Y/out2/LINUX/NUMEROS.TXT $Y/src/NUMEROS.TXT && cmp -s "$Y/out2/LINUX/SUB/INFORM~1.TXT" "$Y/src/sub/Informe largo ñandú.txt" && cmp -s $Y/out2/ZC/AZAR.BIN $Y/src/AZAR.BIN \
   && cmp -s $Y/out2/NUMEROS.TXT $Y/src/NUMEROS.TXT; then ok "en el disco: lo sacado (Extraer todo, con contraseña y arrastrado) igual octeto a octeto"
else ko "lo sacado desde la ventana no es igual:"; ls -R $Y/out2 | head -10 | sed 's/^/        /'; fi
if unzip -tq $Y/out2/LINUX.ZIP > /dev/null 2>&1 && unzip -l $Y/out2/LINUX.ZIP | grep -q " LEEME.TXT$" && ! unzip -l $Y/out2/LINUX.ZIP | grep -q AZAR.BIN \
   && [ "$(unzip -p $Y/out2/LINUX.ZIP LEEME.TXT | md5sum)" = "$(md5sum < $Y/out2/LEEME.TXT)" ]; then ok "LINUX.ZIP, rehecho (sin AZAR.BIN, con LEEME.TXT): lo da por bueno el unzip de Linux"
else ko "LINUX.ZIP rehecho no está bien: $(unzip -l $Y/out2/LINUX.ZIP 2>&1 | tail -3)"; fi
if unzip -tq $Y/out2/CARTAS.ZIP > /dev/null 2>&1 && [ "$(unzip -p $Y/out2/CARTAS.ZIP CARTAS/ANA.TXT)" = "$(cat $Y/ANA.TXT)" ]; then ok "CARTAS.ZIP (del explorador): bueno para Linux, con CARTAS/ANA.TXT y CARTAS/JUAN.TXT"
else ko "CARTAS.ZIP no está bien: $(unzip -l $Y/out2/CARTAS.ZIP 2>&1 | tail -4)"; fi
if sano $Y/g.img; then ok "y el disco, sano (fsck.fat)"; else ko "el disco ha quedado mal:"; sed 's/^/        /' $P/fsck.txt | head -4; fi
if ! cat $Y/ordenes.sal $Y/ventana.sal | grep -q "excepción\|^\*\*\*"; then ok "ni una excepción ni una queja del núcleo"
else ko "el núcleo se ha quejado:"; cat $Y/ordenes.sal $Y/ventana.sal | grep "excepción\|^\*\*\*" | head -4 | sed 's/^/        /'; fi
}

parte_mirador() {
echo "== Hispa-Pentium: el Mirador (MIRADOR.BIN) y una imagen de tapiz (la llamada 172) =="
local Y=$P/mira S
mkdir -p $Y $G
if ! bash $AQUI/nucleo32/mirador/hacer.sh $Y/BIN > $Y/hacer.txt 2>&1; then ko "no se puede hacer MIRADOR.BIN (¿gcc con -m32?): $(tail -n 2 $Y/hacer.txt)"; return; fi
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/semilla.cas -o $Y/semilla.asm > /dev/null && nasm -f bin $Y/semilla.asm -o $Y/SEMILLA.BIN || { ko "semilla.cas no compila"; return; }
$AQUI/nucleo32/disco.sh $Y/base.img 32 > $Y/disco.txt 2>&1 || { ko "disco.sh no hace el disco: $(tail -n 2 $Y/disco.txt)"; return; }
mkdir -p $Y/fotos
python3 -I $AQUI/nucleo32/mirador/prueba/haz_imagenes.py $Y/fotos || { ko "no se pueden hacer las imágenes de prueba"; return; }
export MTOOLS_SKIP_CHECK=1
cp $Y/base.img $Y/m.img
mcopy -i $Y/m.img $Y/BIN/MIRADOR.BIN ::
mmd -i $Y/m.img ::FOTOS
mcopy -i $Y/m.img $Y/fotos/* ::FOTOS/
SES=(":pausa 2" ":doble 60 40" ":pausa 3" ":doble 330 160" ":pausa 2" ":doble 260 160" ":pausa 2" ":doble 452 160" ":pausa 3" ":raton 790 590" ":captura v1"
 ":tecla right" ":pausa 2" ":captura v2" ":raton 400 300" ":monitor mouse_move 0 0 -1" ":pausa 1" ":monitor mouse_move 0 0 -1" ":pausa 2" ":raton 790 590" ":captura v3"
 ":tecla right" ":pausa 2" ":captura v4" ":tecla home" ":pausa 1" ":captura v5" ":escribe r" ":pausa 1" ":captura v6"
 ":escribe f" ":pausa 3" ":captura v7" ":tecla f5" ":pausa 6" ":captura v8" ":tecla esc" ":pausa 1" ":tecla esc" ":pausa 1"
 ":tecla end" ":pausa 1" ":clicd 300 300" ":pausa 1" ":captura v9" ":clic 330 374" ":pausa 4" ":raton 790 590" ":captura v10")
CAPTURAS=$G ESCRIBE=1 DISCO=$Y/m.img ARRANQUE=escritorio MODULOS= MEMORIA=128 $AQUI/nucleo32/probar.sh $Y/SEMILLA.BIN "${SES[@]}" > $Y/mirador.sal 2>&1
S=$Y/mirador.sal
mira "dos clics en FLOR.PNG, en el explorador, la abren en el Mirador (la clase PNG, del paquete)" "^mirador: abierta C:.FOTOS.FLOR.PNG (160 x 120, 1 cuadros)$"
miracap "la ventana: la barra (anterior, siguiente, acercar, Ajustar, Girar, Diapositivas, Completa, Más...), la imagen en el centro y su estado" v1 \
  negrita 40 30 300 60 "FLOR.PNG - Mirador" negrita 180 70 280 95 Ajustar negrita 400 70 540 95 Diapositivas rgb 400 300 FF7A33 rgb 330 250 2F74D0 \
  rgb 200 300 2A2D31 texto 30 500 400 525 "FLOR.PNG   160 x 120   100 %   3 de 6   PNG"
mira "las flechas: las demás de la carpeta, por orden" "^mirador: abierta C:.FOTOS.GRANDE.PNG (1200 x 900, 1 cuadros)$" "^mirador: abierta C:.FOTOS.ICONO.ICO (32 x 32, 1 cuadros)$"
miracap "una grande (1200 x 900): ajustada a la ventana (alejada con el bilineal, de su mitad)" v2 texto 30 500 400 525 "GRANDE.PNG   1200 x 900   4" rgb 300 200 4F3E80 30 rgb 500 400 AFBE80 30
miracap "la rueda del ratón la aleja donde está la flecha" v3 texto 30 500 400 525 "GRANDE.PNG   1200 x 900   2" rgb 120 300 2A2D31
miracap "un ICO (el de 32 x 32 con alfa, el más grande de los dos): lo transparente, sobre cuadros" v4 texto 30 500 400 525 "ICONO.ICO   32 x 32" rgb 400 302 00A000 30
mira "un GIF animado: sus cuatro cuadros; y girado" "^mirador: abierta C:.FOTOS.BOLA.GIF (64 x 64, 4 cuadros)$" "^mirador: girada a la derecha (64 x 64)$"
miracap "el GIF: «animada: 4 cuadros»" v5 texto 30 500 600 525 "animada: 4 cuadros"
miracap "girado a la derecha: la franja de abajo queda a la izquierda" v6 rgb 370 300 1E78DC 30 rgb 425 300 FFFFFF 10
mira "pantalla completa y diapositivas (F5): pasa sola a la siguiente" "^mirador: pantalla completa, 800 x 600$" "^mirador: diapositivas en marcha$" "^mirador: fuera de la pantalla completa$"
miracap "a pantalla completa: sin marco y tapando la barra de tareas" v7 rgb 10 10 2A2D31 rgb 400 590 2A2D31 notexto 0 560 200 600 Inicio
miracap "el menú de Más... (y del botón secundario): girar, fondo de escritorio, abrir, propiedades" v9 texto 300 300 600 420 "Usar como fondo de escritorio"
mira "«Usar como fondo de escritorio»: la imagen a la medida de la pantalla, al núcleo (la llamada 172)" "^mirador: de fondo de escritorio, 800 x 600: bien$" \
  "^escritorio: el tapiz, ahora una imagen (FONDO.HPM)$"
miracap "el tapiz, ya la imagen (PAISAJE.JPG: de amarillo arriba a rojo abajo; abajo a la izquierda, rojo)" v10 rgb 20 540 BB331D 30
saca $Y/m.img FONDO.HPM $Y/fondo.hpm
if [ -f $Y/fondo.hpm ] && [ "$(stat -c %s $Y/fondo.hpm)" = $((12 + 800 * 600 * 4)) ] && [ "$(head -c 4 $Y/fondo.hpm)" = HPMI ]; then ok "FONDO.HPM, en la raíz (sin instalar): «HPMI», 800 x 600 y sus puntos"
else ko "no está FONDO.HPM, o no es lo que tiene que ser"; fi
# y al arrancar otra vez, el tapiz sigue siendo la imagen
CAPTURAS=$G ESCRIBE=1 DISCO=$Y/m.img ARRANQUE=escritorio MODULOS= MEMORIA=64 $AQUI/nucleo32/probar.sh $Y/SEMILLA.BIN ":pausa 3" ":raton 600 300" ":captura v11" > $Y/otra.sal 2>&1
miracap "al arrancar otra vez, el tapiz sigue siendo la imagen (amarillo arriba, rojo abajo)" v11 rgb 300 30 FCD204 30 rgb 100 540 BB331D 30
if ! cat $Y/mirador.sal $Y/otra.sal | grep -q "excepción\|^\*\*\*"; then ok "ni una excepción ni una queja del núcleo"
else ko "el núcleo se ha quejado:"; cat $Y/mirador.sal $Y/otra.sal | grep "excepción\|^\*\*\*" | head -4 | sed 's/^/        /'; fi
}
