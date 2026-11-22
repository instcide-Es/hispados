# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# pruebas_oficina.sh - (OFICINA2) la parte 40 de pruebas.sh (SOLO=oficina): la
# oficina 1.1 entera en HVPRUN de Hispa-Pentium (Datos, el Lector,
# Publicaciones, Presentaciones y Pintura, con Facturas, Pluma, Cuentas y
# Mirador), que pruebas.sh lee («.»): la función parte_oficina, con sus
# ayudantes (ok, ko, mira, miracap, saca, sano) y sus carpetas ($AQUI, $P, $G).
#
# Cada programa en QEMU: abrir, escribir (con eñe, tildes y el euro),
# guardar, cerrar y volver a abrir lo guardado (y mirarlo en el disco desde
# fuera); el menú Inicio (Oficina y Multimedia, sin dos Miradores), las
# clases de Mi equipo (.BDD, .PUB, .PRE, .PDF y «Editar con Pintura» en los
# BMP y PCX), la rueda, Alt+letra, el portapapeles con el Bloc de notas,
# maximizar, una ruta larga (C:\USUARIOS\...\PROGRAMA, 57 letras) con su
# título acortado, la carpeta de los documentos (\DOCUMENT) y la zona
# protegida (\HISPA: no se guarda ahí). Las muestras (imágenes BMP y PCX y un
# PDF) se hacen aquí: nucleo32/hvprun/muestras/haz_imagenes.py y
# nucleo32/pdf/pruebas/haz_pdf.py. Las capturas buenas, en docs/pentium/oficina/.
#
# Posiciones (800 x 600): Inicio (40, 585); Todos los programas (120, 503);
# sus carpetas Multimedia (300, 453) y Oficina (300, 479); en Oficina, de
# 26 en 26 desde (540, 366): Cuentas, Datos, Facturas, Lector de PDF,
# Mirador, Pluma, Presentaciones y Publicaciones; en Multimedia, Pintura (540,
# 453). Ejecutar... (300, 404).

# abre «Todos los programas > CARPETA > el de Y» (carpeta: 479 Oficina, 453 Multimedia)
# (en el arreglo M)
of_menu() { M=(":clic 40 585" ":pausa 1" ":clic 120 503" ":pausa 1" ":clic 300 $1" ":pausa 1" ":clic 540 $2" ":pausa 5"); }

parte_oficina() {
echo "== Hispa-Pentium: la oficina 1.1 en HVPRUN (Datos, Lector, Publicaciones, Presentaciones y Pintura) =="
local Y=$P/oficina S D
mkdir -p $Y $G
export MTOOLS_SKIP_CHECK=1
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/semilla.cas -o $Y/semilla.asm > /dev/null && nasm -f bin $Y/semilla.asm -o $Y/SEMILLA.BIN || { ko "semilla.cas no compila"; return; }
$AQUI/nucleo32/disco.sh $Y/of.img 16 oficina > $Y/disco.txt 2>&1 || { ko "disco.sh no hace el disco con la oficina: $(tail -n 2 $Y/disco.txt)"; return; }
python3 -I $AQUI/nucleo32/hvprun/muestras/haz_imagenes.py $Y/img > /dev/null || { ko "no se pueden hacer las imágenes de muestra"; return; }
python3 -I $AQUI/nucleo32/pdf/pruebas/haz_pdf.py $Y/MUESTRA.PDF A 3
local L=USUARIOS/CARLOSSA/DOCUMENT/ASOCIACI/FIESTAS/2026/PROGRAMA d p=
mmd -i $Y/of.img ::DOCUMENT ::HISPA ::IMAGENES
for d in $(echo $L | tr / ' '); do p=${p:+$p/}$d; mmd -i $Y/of.img ::$p; done
mcopy -i $Y/of.img $Y/img/FOTO.PCX $Y/img/COLOR.PCX $Y/img/FOTO.BMP ::
mcopy -i $Y/of.img $Y/img/EGA.PCX ::IMAGENES/
mcopy -i $Y/of.img $Y/MUESTRA.PDF ::DOCUMENT/
if mdir -i $Y/of.img ::OFICINA/DATOS.HVP > /dev/null 2>&1 && mdir -i $Y/of.img ::OFICINA/PINTURA.HVP > /dev/null 2>&1 && mdir -i $Y/of.img ::OFICINA/TIPOS32.DAT > /dev/null 2>&1; then
  ok "el disco con la oficina 1.1: los nueve .HVP (con Datos, LECTOR, PUBLICA, PRESENTA y PINTURA) y los tipos de letra en todos sus tamaños (TIPOS12/24/32.DAT)"
else ko "al disco le faltan programas de la oficina 1.1 o los tipos de letra"; mdir -i $Y/of.img ::OFICINA | sed 's/^/        /'; fi
corre() {      # corre NOMBRE ÓRDENES...: una sesión en el escritorio, sobre el disco (escribiendo en él)
  local n=$1; shift
  CAPTURAS=$G ESCRIBE=1 DISCO=$Y/of.img ARRANQUE=escritorio MODULOS= MEMORIA=64 $AQUI/nucleo32/probar.sh $Y/SEMILLA.BIN ":pausa 2" "$@" > $Y/$n.sal 2>&1
  S=$Y/$n.sal
}

# ---- 1. el menú Inicio: Todos los programas > Oficina y > Multimedia
corre menu ":clic 40 585" ":pausa 1" ":clic 120 503" ":pausa 1" ":clic 300 479" ":pausa 1" ":captura ofi1" \
  ":tecla esc" ":tecla esc" ":tecla esc" ":pausa 1" ":clic 40 585" ":pausa 1" ":clic 120 503" ":pausa 1" ":clic 300 453" ":pausa 1" ":captura ofi2"
miracap "Todos los programas > Oficina: los de la oficina 1.1 (Datos, Lector de PDF, Presentaciones y Publicaciones) con los de siempre, cada uno con su icono" ofi1 \
  texto 480 356 520 376 Cuentas texto 480 382 520 402 Datos texto 480 434 520 454 "Lector de PDF" texto 480 460 520 480 Mirador \
  texto 480 512 520 532 Presentaciones texto 480 538 520 558 Publicaciones nrgb 474 398 F7F7F7 nrgb 474 450 F7F7F7
miracap "Todos los programas > Multimedia: Pintura (el paquete «pintura»)" ofi2 texto 480 444 520 464 Pintura

# ---- 2. Datos: una tabla de socios, un registro con eñe, tildes y euro; copiar
# al Bloc de notas; y al volver a abrirlo, sigue (DATOS.BDD, al lado del programa)
of_menu 479 392
corre datos "${M[@]}" ":captura ofd1" ":clic 580 82" ":pausa 2" ":clic 346 408" ":pausa 2" ":tecla insert" ":pausa 2" \
  ":escribe Begoña" ":tecla tab" ":escribe Núñez Ávila" ":tecla tab" ":escribe 12345678Z" ":tecla tab" ":tecla tab" ":escribe Local 3 (12 €)" \
  ":tecla tab" ":escribe Peñíscola" ":pausa 1" ":clic 364 412" ":pausa 2" ":captura ofd2" ":tecla ctrl-c" ":pausa 1" ":clic 771 34" ":pausa 3" \
  ":doble 64 110" ":pausa 3" ":tecla ctrl-v" ":pausa 1" ":captura ofd3" ":clic 40 585" ":pausa 1" ":clic 120 503" ":pausa 1" ":clic 300 479" ":pausa 1" \
  ":clic 540 392" ":pausa 5" ":captura ofd4"
mira "el menú lanza Datos con HVPRUN" "escritorio: lanzo .HVPRUN.BIN"
miracap "Datos (DATOS.HVP) en su ventana: la base de siempre, DATOS.BDD, aún sin tablas" ofd1 negrita 20 20 300 40 "Datos - DATOS.BDD" texto 100 150 700 200 "Esta base todav"
miracap "una tabla de Socios con un registro tecleado: eñe, tildes y el euro (Alt Gr+E)" ofd2 texto 10 115 160 135 "Begoña" texto 150 115 330 135 "Núñez" \
  texto 505 115 700 135 "Local 3 (12" texto 10 525 200 545 "1 registro"
miracap "Ctrl+C en Datos y Ctrl+V en el Bloc de notas: el registro pasa por el portapapeles del sistema" ofd3 texto 60 120 700 400 "Begoña"
miracap "y al volver a abrir Datos, la tabla y el registro siguen" ofd4 texto 10 115 160 135 "Begoña" texto 10 525 200 545 "1 registro"
saca $Y/of.img OFICINA/DATOS.BDD $Y/datos.bdd; saca $Y/of.img OFICINA/DATOS.BDJ $Y/datos.bdj
if cat $Y/datos.bdd $Y/datos.bdj 2> /dev/null | LC_ALL=C grep -qa "$(printf 'Pe\xa4\xa1scola')"; then ok "en el disco: OFICINA\\DATOS.BDD (y su diario, .BDJ) con «Peñíscola» (eñe y tilde en la página de Ventanas)"
else ko "en el disco no está lo guardado por Datos"; fi

EJ=(":clic 40 585" ":pausa 1" ":clic 300 404" ":pausa 2")        # Inicio > Ejecutar... (uno por sesión:
                                                                 # recuerda la orden de antes)

# ---- 3. el Lector: un PDF desde Mi equipo (la clase PDF, sin Hispa-PDF), la
# rueda, la página siguiente, Alt+A y «Guardar el texto de la página» (en la
# carpeta del PDF), y el texto al Bloc de notas por el portapapeles
corre lector "${EJ[@]}" ":escribe \\EQUIPO.BIN /ABRIR \\DOCUMENT\\MUESTRA.PDF" ":tecla ret" ":pausa 6" ":raton 400 300" ":captura ofl1" \
  ":monitor mouse_move 0 0 -1" ":pausa 1" ":monitor mouse_move 0 0 -1" ":pausa 1" ":monitor mouse_move 0 0 -1" ":pausa 2" ":captura ofl2" \
  ":clic 120 82" ":pausa 3" ":captura ofl3" ":tecla alt-a" ":pausa 1" ":captura ofl4" ":escribe g" ":pausa 2" ":escribe pagina2" ":tecla ret" ":pausa 3" \
  ":tecla ret" ":pausa 1" ":tecla ctrl-c" ":pausa 1" ":clic 774 34" ":pausa 3" ":doble 64 110" ":pausa 3" ":tecla ctrl-v" ":pausa 1" ":captura ofl5"
mira "Mi equipo abre el .PDF con el Lector (HVPRUN), que no está Hispa-PDF" "escritorio: lanzo .EQUIPO.BIN"
miracap "el Lector (LECTOR.HVP) con el PDF: su título y «Página 1 de 3»" ofl1 negrita 20 20 400 45 "Lector - \\DOCUMENT\\MUESTRA.PDF" texto 10 522 200 542 "Página 1 de 3"
miracap "la rueda del ratón baja la página (HVPRUN le da a Ventanas el suceso de la rueda)" ofl2 distinto 100 110 600 300 $G/ofl1.ppm texto 10 522 200 542 "Página 1 de 3"
miracap "la página siguiente (su botón): la 2" ofl3 texto 10 522 200 542 "Página 2 de 3"
miracap "Alt+A abre el menú Archivo (Alt+letra llega como en DOS)" ofl4 texto 10 60 400 300 "el texto de la p"
miracap "Ctrl+C copia el texto de la página y el Bloc de notas lo pega" ofl5 texto 60 120 700 400 "Documento A, página 2"
saca $Y/of.img DOCUMENT/PAGINA2.TXT $Y/pagina2.txt
if LC_ALL=C grep -qa "$(printf 'Documento A, p\xa0gina 2')" $Y/pagina2.txt 2> /dev/null; then ok "«Guardar el texto de la página»: DOCUMENT\\PAGINA2.TXT, en la carpeta del PDF, con su tilde"
else ko "no está DOCUMENT\\PAGINA2.TXT con el texto de la página 2"; fi

# ---- 4. Publicaciones: una caja de texto (36 puntos), maximizar, guardar en
# los documentos (\DOCUMENT, la carpeta de trabajo) y abrirla desde Mi equipo
of_menu 479 548
corre publica "${M[@]}" ":captura ofp0" ":clic 343 80" ":arrastra 290 170 500 250" ":pausa 1" ":escribe Gran cartel: ñ, á y 3 €" ":tecla esc" ":pausa 1" \
  ":triple 171 108" ":escribe 36" ":tecla ret" ":pausa 2" ":captura ofp1" ":clic 747 32" ":pausa 3" ":captura ofp2" ":clic 757 13" ":pausa 3" \
  ":tecla ctrl-g" ":pausa 2" ":captura ofp3" ":escribe cartel" ":tecla ret" ":pausa 3" ":captura ofp4" ":clic 774 32" ":pausa 3" \
  "${EJ[@]}" ":escribe \\EQUIPO.BIN /ABRIR \\DOCUMENT\\CARTEL.PUB" ":tecla ret" ":pausa 6" ":captura ofp5"
miracap "Publicaciones (PUBLICA.HVP): una caja de texto con eñe, tilde y euro" ofp1 negrita 20 20 400 45 "Publicaciones - (sin nombre)" distinto 280 160 230 100 $G/ofp0.ppm
miracap "maximizada: Publicaciones se recoloca en la pantalla entera (su barra de estado, abajo)" ofp2 texto 10 540 200 562 "Página 1 de 1" notexto 10 520 200 538 "Página 1 de 1"
miracap "Guardar: el cuadro empieza en los documentos (C:\\DOCUMENT), no en \\HISPA" ofp3 texto 230 230 560 250 "Carpeta: C:\\DOCUMENT"
miracap "guardada: CARTEL.PUB" ofp4 negrita 20 20 400 45 "Publicaciones - cartel.PUB"
miracap "Mi equipo abre el .PUB con Publicaciones: lo guardado" ofp5 negrita 20 20 500 45 "Publicaciones - \\DOCUMENT\\CARTEL.PUB" distinto 280 160 230 100 $G/ofp0.ppm
saca $Y/of.img DOCUMENT/CARTEL.PUB $Y/cartel.pub
if LC_ALL=C grep -qa "$(printf 'Gran cartel: \xa4, \xa0 y 3 \xf4')" $Y/cartel.pub 2> /dev/null && [ "$(head -c 7 $Y/cartel.pub)" = PUBLICA ]; then
  ok "en el disco: DOCUMENT\\CARTEL.PUB (PUBLICA), con «Gran cartel: ñ, á y 3 €» (el euro, F4h)"
else ko "DOCUMENT\\CARTEL.PUB no está o no lleva el texto"; fi

# ---- 5. Presentaciones: el título, guardar en \HISPA (la zona protegida: no
# se puede, y lo dice) y en los documentos
of_menu 479 522
corre presenta "${M[@]}" ":captura ofr0" ":clic 460 240" ":pausa 1" ":escribe Fiesta de la Peña: 5 €" ":tecla esc" ":pausa 1" ":captura ofr1" \
  ":tecla ctrl-g" ":pausa 2" ":escribe \\HISPA\\FIESTA" ":tecla ret" ":pausa 3" ":captura ofr2" ":tecla ret" ":pausa 1" \
  ":tecla ctrl-g" ":pausa 2" ":escribe fiesta" ":tecla ret" ":pausa 3" ":captura ofr3" ":clic 774 32" ":pausa 3"
miracap "Presentaciones (PRESENTA.HVP): el título tecleado, con los tipos de letra de la oficina (48 puntos)" ofr1 negrita 20 20 400 45 "Presentaciones - (sin nombre)" \
  distinto 290 225 340 40 $G/ofr0.ppm texto 10 525 200 545 "Diapositiva 1 de 1"
mira "guardar en \\HISPA: el núcleo no deja (la zona protegida)" "permisos: acceso denegado a .HISPA.FIESTA"
miracap "y Presentaciones lo dice" ofr2 texto 180 260 640 310 "No se ha podido guardar la presentaci"
miracap "en los documentos, sí: FIESTA.PRE" ofr3 negrita 20 20 400 45 "Presentaciones - fiesta.PRE"
saca $Y/of.img DOCUMENT/FIESTA.PRE $Y/fiesta.pre
if LC_ALL=C grep -qa "$(printf 'Fiesta de la Pe\xa4a: 5 \xf4')" $Y/fiesta.pre 2> /dev/null && ! mdir -i $Y/of.img ::HISPA/FIESTA.PRE > /dev/null 2>&1 && ! mdir -i $Y/of.img ::HISPA/FIESTA.TMP > /dev/null 2>&1; then
  ok "en el disco: DOCUMENT\\FIESTA.PRE con el título (eñe y euro); en \\HISPA, nada"
else ko "FIESTA.PRE no está bien en DOCUMENT, o algo ha quedado en \\HISPA"; mdir -i $Y/of.img ::HISPA | sed 's/^/        /'; fi

# ---- 6. una ruta larga: C:\USUARIOS\CARLOSSA\DOCUMENT\ASOCIACI\FIESTAS\2026\PROGRAMA
# (57 letras): Mi equipo abre FIESTA.PRE de ahí; el título, acortado (el
# núcleo no pone uno de 64 letras o más); se cambia, se guarda y «Guardar
# como» va a esa misma carpeta (la del documento)
mcopy -i $Y/of.img $Y/fiesta.pre ::$L/FIESTA.PRE
corre larga "${EJ[@]}" ":escribe \\EQUIPO.BIN /ABRIR \\USUARIOS\\CARLOSSA\\DOCUMENT\\ASOCIACI\\FIESTAS\\2026\\PROGRAMA" ":tecla ret" ":pausa 4" ":captura ofg1" \
  ":doble 260 160" ":pausa 6" ":captura ofg2" ":clic 460 297" ":pausa 1" ":escribe Programa de 2026" ":tecla esc" ":pausa 1" ":tecla ctrl-g" ":pausa 3" \
  ":tecla alt-a" ":pausa 1" ":escribe o" ":pausa 2" ":captura ofg3" ":escribe copia" ":tecla ret" ":pausa 3" ":captura ofg4" ":clic 774 32" ":pausa 3"
miracap "Mi equipo en la carpeta larga (57 letras), con FIESTA.PRE" ofg1 negrita 20 5 700 30 "C:\\USUARIOS\\CARLOSSA\\DOCUMENT\\ASOCIACI\\FIESTAS\\2026\\PROGRAMA"
miracap "Presentaciones la abre de ahí: el título, acortado para que quepa («...» y el final de la ruta)" ofg2 negrita 20 20 600 45 "Presentaciones - ...\\ASOCIACI\\FIESTAS\\2026\\PROGRAMA\\FIESTA.PRE"
miracap "Guardar como empieza en la carpeta del documento" ofg3 texto 230 230 580 250 "Carpeta: C:\\USUARIOS\\CARLOSSA\\DOCUMENT"
saca $Y/of.img $L/FIESTA.PRE $Y/fiesta2.pre; saca $Y/of.img $L/COPIA.PRE $Y/copia.pre
if LC_ALL=C grep -qa "Programa de 2026" $Y/fiesta2.pre 2> /dev/null && LC_ALL=C grep -qa "Programa de 2026" $Y/copia.pre 2> /dev/null; then
  ok "en el disco, en la carpeta larga: FIESTA.PRE cambiada (Guardar) y COPIA.PRE (Guardar como), las dos con lo nuevo"
else ko "en la carpeta larga no están FIESTA.PRE cambiada y COPIA.PRE"; mdir -i $Y/of.img ::$L | sed 's/^/        /'; fi

# ---- 7. Pintura: un rectángulo, guardarlo (DIBUJO.BMP) y abrirlo otra vez;
# «Editar con Pintura» de un PCX de 16 colores (EGA.PCX) en Mi equipo, que se
# guarda al lado en BMP; y los PCX de 256 colores y de color verdadero
of_menu 453 453
corre pintura "${M[@]}" ":captura ofn1" ":escribe r" ":arrastra 150 180 300 260" ":pausa 1" ":tecla ctrl-g" ":pausa 2" ":escribe dibujo" ":tecla ret" ":pausa 3" \
  ":captura ofn2" ":clic 685 73" ":pausa 3" "${EJ[@]}" ":escribe \\EQUIPO.BIN /ABRIR \\DOCUMENT\\DIBUJO.BMP" ":tecla ret" ":pausa 6" ":captura ofn3"
# (otra sesión: «Ejecutar...» recuerda lo de antes)
corre editar "${EJ[@]}" ":escribe \\EQUIPO.BIN /ABRIR \\IMAGENES" ":tecla ret" ":pausa 4" ":clicd 260 160" ":pausa 1" ":captura ofn4" ":clic 330 428" ":pausa 5" ":captura ofn5" \
  ":tecla ctrl-g" ":pausa 3"
miracap "Pintura (PINTURA.HVP), del menú Multimedia" ofn1 negrita 100 60 400 85 "Pintura - (sin nombre)" texto 100 480 400 500 "480 x 320"
miracap "un rectángulo, guardado en los documentos: DIBUJO.BMP" ofn2 negrita 100 60 400 85 "Pintura - dibujo.BMP" distinto 150 180 150 80 $G/ofn1.ppm
miracap "abierto otra vez desde Mi equipo (la clase BMP: el Mirador), con el rectángulo" ofn3 negrita 0 20 400 45 "Mirador - DIBUJO.BMP"
miracap "Mi equipo: el menú del botón secundario de un PCX lleva «Editar con Pintura»" ofn4 texto 260 400 460 440 "Editar con Pintura"
miracap "«Editar con Pintura» de EGA.PCX (cuatro planos, 16 colores): Pintura lo lee; se guardará como EGA.BMP" ofn5 negrita 100 60 400 85 "Pintura - \\IMAGENES\\EGA.BMP" \
  rgb 118 180 FF5555 40 rgb 142 180 55FF55 40 rgb 166 180 5555FF 40 rgb 190 180 FFFF55 40 texto 100 480 400 500 "96 x 64"
saca $Y/of.img DOCUMENT/DIBUJO.BMP $Y/dibujo.bmp; saca $Y/of.img IMAGENES/EGA.BMP $Y/ega.bmp
if [ "$(head -c 2 $Y/dibujo.bmp 2> /dev/null)" = BM ] && [ "$(head -c 2 $Y/ega.bmp 2> /dev/null)" = BM ] && cmp -s $Y/img/EGA.PCX <(mtype -i $Y/of.img ::IMAGENES/EGA.PCX 2> /dev/null); then
  ok "en el disco: DOCUMENT\\DIBUJO.BMP y EGA.BMP (al lado del PCX, que no se toca)"
else ko "no están DIBUJO.BMP y EGA.BMP, o EGA.PCX ha cambiado"; fi
for f in FOTO COLOR; do
  corre pcx$f "${EJ[@]}" ":escribe \\HVPRUN.BIN \\OFICINA\\PINTURA.HVP \\$f.PCX" ":tecla ret" ":pausa 6" ":captura ofx$f"
done
miracap "un PCX de 256 colores (con la paleta del final, RLE) en Pintura" ofxFOTO negrita 100 60 400 85 "Pintura - \\FOTO.BMP" texto 100 480 400 500 "160 x 100" \
  rgb 110 150 000000 70 rgb 262 150 FF3CC8 80 rgb 110 244 00FF3C 90
miracap "un PCX de color verdadero (tres planos) en Pintura" ofxCOLOR negrita 100 60 400 85 "Pintura - \\COLOR.BMP" texto 100 480 400 500 "160 x 100" \
  rgb 110 150 000000 70 rgb 262 150 FF3CC8 80 rgb 110 244 00FF3C 90

# ---- 8. con el Mirador de Pentium (el paquete «mirador»: su acceso), el de
# la oficina no sale en el menú (no dos Miradores)
mmd -i $Y/of.img ::MENU 2> /dev/null
printf 'nombre=Mirador\r\nprograma=\\MIRADOR.BIN\r\ntipo=Multimedia\r\n' > $Y/MIRADOR.MNU
mcopy -o -i $Y/of.img $Y/MIRADOR.MNU ::MENU/
corre mirador ":clic 40 585" ":pausa 1" ":captura ofm1" ":clic 120 503" ":pausa 1" ":clic 300 479" ":pausa 1" ":captura ofm2" \
  ":tecla esc" ":tecla esc" ":tecla esc" ":pausa 1" ":clic 40 585" ":pausa 1" ":clic 120 503" ":pausa 1" ":clic 300 453" ":pausa 1" ":captura ofm3"
miracap "con el Mirador de Pentium, en Todos los programas > Oficina ya no está el de la oficina" ofm2 texto 480 300 700 600 "Publicaciones" notexto 480 300 700 600 "Mirador"
miracap "y en Multimedia, el de Pentium y Pintura" ofm3 texto 480 400 700 600 "Mirador" texto 480 400 700 600 "Pintura"
if ! cat $Y/*.sal | grep -q "excepción\|^\*\*\*\|HVPRUN: "; then ok "ni una excepción, ni una queja del núcleo, ni un aviso de HVPRUN"
else ko "el núcleo o HVPRUN se han quejado:"; cat $Y/*.sal | grep "excepción\|^\*\*\*\|HVPRUN: " | head -4 | sed 's/^/        /'; fi
if sano $Y/of.img; then ok "y el disco queda sano (fsck.fat)"; else ko "el disco no queda sano:"; sed 's/^/        /' $P/fsck.txt | head -4; fi
# las capturas, para docs/pentium/oficina/ (con OFICINA_CAPTURAS=carpeta)
if [ -n "${OFICINA_CAPTURAS:-}" ]; then
  mkdir -p "$OFICINA_CAPTURAS"
  for c in ofi1 ofd2 ofd4 ofl1 ofl5 ofp2 ofp5 ofr1 ofr2 ofg2 ofn5 ofxFOTO ofm2; do [ -f $G/$c.ppm ] && pnmtopng $G/$c.ppm > "$OFICINA_CAPTURAS/$c.png" 2> /dev/null; done
fi
# y los documentos que han hecho (para nucleo32/hvprun/muestras/, con OFICINA_MUESTRAS=carpeta)
if [ -n "${OFICINA_MUESTRAS:-}" ]; then
  mkdir -p "$OFICINA_MUESTRAS"
  cp $Y/cartel.pub "$OFICINA_MUESTRAS/CARTEL.PUB" 2> /dev/null; cp $Y/fiesta2.pre "$OFICINA_MUESTRAS/FIESTA.PRE" 2> /dev/null
  cp $Y/datos.bdd "$OFICINA_MUESTRAS/DATOS.BDD" 2> /dev/null; cp $Y/datos.bdj "$OFICINA_MUESTRAS/DATOS.BDJ" 2> /dev/null
  cp $Y/dibujo.bmp "$OFICINA_MUESTRAS/DIBUJO.BMP" 2> /dev/null; cp $Y/pagina2.txt "$OFICINA_MUESTRAS/PAGINA2.TXT" 2> /dev/null
fi
}
