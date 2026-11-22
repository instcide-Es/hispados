# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# pruebas_iconos.sh - (ICONOS) la parte 47 de pruebas.sh (SOLO=iconos), que
# lee este archivo («.»): la función parte_iconos, con los ayudantes de
# pruebas.sh (ok, ko, mira, miracap) y sus carpetas ($AQUI, $P).
#
# Los iconos del tapiz (nucleo32/organiza.cas y escritorio.cas), en QEMU, con
# 30 iconos (los 3 de siempre, 12 accesos de paquetes con «escritorio=si» y
# 15 archivos y carpetas en \ESCRITOR). Dónde está cada uno lo dice el
# escritorio por el puerto serie («escritorio: iconos (...)») y lo mira
# nucleo32/mira_iconos.py: ninguno se sale de la pantalla ni toca la barra
# de tareas ni el dock, y no hay dos uno encima de otro.
#
#   1. A 1024 x 600 (la del HP Mini): al arrancar; «Organizar iconos» por
#      nombre, por tipo, por tamaño, por fecha y como al principio; arrastrar
#      uno encima de la barra (vuelve a la zona); sin «Alinear a la
#      cuadrícula», donde se suelta (y encima de otro, no); la organización
#      automática; el teclado (flechas, letra e Intro); y lo elegido se
#      guarda (ICONOS.INI) y vale al arrancar otra vez.
#   2. A 800 x 600: al arrancar y por nombre.
#   3. Cambiar la medida en marcha con los iconos: de 1280 x 720 (con uno
#      llevado a la derecha del todo) a 800 x 600 y luego a 1024 x 600.
#   4. A 640 x 480 con 47 iconos: las celdas, más estrechas, y en dos
#      páginas con «Más iconos» (un clic y Av Pág pasan de página).
#   5. Sin el dock (DOCK.INI con «visible=no»): los iconos bajan hasta la barra.

parte_iconos() {
echo "== Hispa-Pentium: los iconos del tapiz (la zona de trabajo, la rejilla, organizar por nombre, tipo, tamaño y fecha, arrastrar, el teclado y las páginas) (ICONOS) =="
local X=$P/iconos MI="python3 -I $AQUI/nucleo32/mira_iconos.py" i n nm f S h
rm -rf $X; mkdir -p $X/mnu $X/esc $X/mas
export MTOOLS_SKIP_CHECK=1
G=$X
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/semilla.cas -o $X/semilla.asm > /dev/null && nasm -f bin $X/semilla.asm -o $X/SEMILLA.BIN || { ko "semilla.cas no compila"; return; }
if ! $AQUI/nucleo32/disco.sh $X/d.img 16 > $X/disco.txt 2>&1; then ko "no se puede hacer el disco: $(tail -2 $X/disco.txt)"; return; fi
# 12 accesos con su icono en el tapiz y 15 cosas en \ESCRITOR (2 carpetas y
# 13 archivos, cada uno con lo que mide y su fecha)
n=0
for nm in Agenda Zapatero Lupa Calculadora Reloj Mapa Bingo Radio Notas2 Fotos2 Ajedrez Teclas; do
  n=$((n + 1)); printf 'nombre=%s\r\nprograma=\\HOLA.BIN\r\nescritorio=si\r\n' $nm > $X/mnu/A$n.MNU
done
i=0
for f in CARTA.TXT ZETA.TXT BALANCE.CSV FOTO.BMP INFORME.DOC LISTA.TXT MUSICA.WAV DIBUJO.PCX TABLA.CSV NOTA.TXT LIBRO.PDF ACTA.DOC MAPA.PNG; do
  i=$((i + 1)); head -c $((i * 1000)) /dev/zero > $X/esc/$f; touch -d "2026-01-$(printf %02d $i) 10:00" $X/esc/$f
done
cp $X/d.img $X/d30.img
mmd -i $X/d30.img ::MENU ::ESCRITOR ::ESCRITOR/CARPETA1 ::ESCRITOR/BCARPETA
mcopy -i $X/d30.img $X/mnu/*.MNU ::MENU/
mcopy -m -i $X/d30.img $X/esc/* ::ESCRITOR/
# lo que sabe la prueba de cada uno (para mirar los órdenes): nombre|familia|ext|tamaño|fecha
tam() { mdir -i $X/d30.img "::$1" 2> /dev/null | awk -v n="$2" 'toupper($1) == n { print $3; exit }'; }
{ printf 'Bloc de notas|0||%s|-\n' "$(tam BLOC.BIN BLOC)"; printf 'Consola|0||%s|-\n' "$(tam CONSOLA.BIN CONSOLA)"
  h=$(tam HOLA.BIN HOLA)
  for nm in Agenda Zapatero Lupa Calculadora Reloj Mapa Bingo Radio Notas2 Fotos2 Ajedrez Teclas; do printf '%s|0||%s|-\n' $nm $h; done
  printf 'CARPETA1|1||0|-\nBCARPETA|1||0|-\n'
  i=0
  for f in CARTA.TXT ZETA.TXT BALANCE.CSV FOTO.BMP INFORME.DOC LISTA.TXT MUSICA.WAV DIBUJO.PCX TABLA.CSV NOTA.TXT LIBRO.PDF ACTA.DOC MAPA.PNG; do
    i=$((i + 1)); printf '%s|2|%s|%s|%s\n' $f ${f#*.} $((i * 1000)) $i
  done; } > $X/datos.txt
chk() {   # lo que se prueba, la salida, el motivo, la medida y las comprobaciones de mira_iconos.py
  local que=$1 sal=$2 mot=$3 an=$4 al=$5; shift 5
  if $MI $sal "$mot" $an $al "$@" > $X/mi.txt 2>&1; then ok "$que"; else ko "$que:"; sed 's/^/        /' $X/mi.txt | head -6; fi
}
SIEMPRE="Mi equipo,Bloc de notas,Consola,Agenda,Fotos2,Ajedrez,Teclas"
# ---- 1. a 1024 x 600 ----
# El menú del tapiz: el botón secundario en (900, 100); «Organizar iconos...»,
# la tercera (y = 165); en el submenú, la opción k en y = 117 + 24 k (las de
# después de la raya, 9 más abajo: automática 246, cuadrícula 270)
cp $X/d30.img $X/c.img
CAPTURAS=$G ESCRIBE=1 DISCO=$X/c.img ARRANQUE="escritorio modo=1024x600" PANTALLA=1024x600 MODULOS= $AQUI/nucleo32/probar.sh $X/SEMILLA.BIN ":pausa 3" ":captura ic1" \
  ":clicd 900 100" ":clic 900 165" ":pausa 1" ":captura ic2" ":clic 900 117" ":pausa 1" ":captura ic3" \
  ":clicd 900 100" ":clic 900 165" ":pausa 1" ":clic 900 141" ":pausa 1" ":captura ic4" \
  ":clicd 900 100" ":clic 900 165" ":pausa 1" ":clic 900 165" ":pausa 1" \
  ":clicd 900 100" ":clic 900 165" ":pausa 1" ":clic 900 189" ":pausa 1" \
  ":clicd 900 100" ":clic 900 165" ":pausa 1" ":clic 900 213" ":pausa 1" \
  ":arrastra 64 190 500 590" ":pausa 1" ":raton 900 300" ":captura ic5" \
  ":clicd 900 100" ":clic 900 165" ":pausa 1" ":clic 900 270" ":pausa 1" \
  ":arrastra 64 114 650 300" ":pausa 1" ":arrastra 64 38 650 300" ":pausa 1" ":raton 900 300" ":captura ic6" \
  ":clicd 900 100" ":clic 900 165" ":pausa 1" ":clic 900 270" ":pausa 1" \
  ":clicd 900 100" ":clic 900 165" ":pausa 1" ":clic 900 246" ":pausa 1" \
  ":arrastra 64 190 700 400" ":pausa 1" \
  ":clicd 900 100" ":clic 900 165" ":pausa 1" ":clic 900 246" ":pausa 1" \
  ":clic 900 450" ":tecla down" ":tecla down" ":tecla c" ":tecla up" ":pausa 1" ":captura ic7" ":tecla ret" ":pausa 2" ":tecla alt-f4" ":pausa 1" \
  ":clicd 900 100" ":clic 900 165" ":pausa 1" ":clic 900 141" ":pausa 1" > $X/a.sal 2>&1
tr -d '\r' < $X/a.sal > $X/a.txt
S=$X/a.txt
chk "a 1024 x 600, al arrancar: los 30 en la zona (ni fuera, ni en la barra, ni en el dock), sin taparse y en la rejilla, en columnas de arriba abajo" \
  $S arranque 1024 600 n 30 paginas 1 rejilla orden "$SIEMPRE"
miracap "y se ven: el último de la quinta columna (ZETA.TXT), por encima del dock; la barra con «Inicio» sin tapar" ic1 tam 1024 600 texto 440 400 520 470 ZETA.TXT negrita 30 570 46 580 Inicio
miracap "el botón secundario en el tapiz: «Organizar iconos...» saca su submenú (nombre, tipo, tamaño, fecha, como al principio; la organización automática, la cuadrícula y Mi equipo el primero, con su casilla [x] o [ ])" ic2 \
  texto 700 100 1024 140 "Por nombre" texto 700 120 1024 160 "Por tipo" texto 700 150 1024 190 "Por fecha de modificación" texto 700 190 1024 230 "Como al principio" \
  texto 700 230 1024 260 "[ ] Organización automática" texto 700 255 1024 285 "[x] Alinear a la cuadrícula" texto 700 280 1024 310 "[x] Mi equipo, siempre el primero"
chk "«Por nombre»: Mi equipo el primero y los demás por su nombre (sin mirar mayúsculas)" $S organizados#1 1024 600 n 30 rejilla por nombre $X/datos.txt orden "Mi equipo,ACTA.DOC,Agenda,Ajedrez,BALANCE.CSV,BCARPETA,Bingo,Bloc de notas"
chk "«Por tipo»: los programas, las carpetas y los documentos por su extensión" $S organizados#2 1024 600 n 30 rejilla por tipo $X/datos.txt orden "Mi equipo,Agenda,Ajedrez,Bingo,Bloc de notas"
chk "«Por tamaño»: de menos a más (las carpetas, lo primero)" $S organizados#3 1024 600 n 30 rejilla por tamano $X/datos.txt orden "Mi equipo,BCARPETA,CARPETA1"
chk "«Por fecha de modificación»: lo más nuevo, primero" $S organizados#4 1024 600 n 30 rejilla por fecha $X/datos.txt
chk "«Como al principio»: como al arrancar" $S organizados#5 1024 600 n 30 rejilla orden "$SIEMPRE" en Consola 8 164
chk "Consola, arrastrada encima de la barra de tareas: vuelve a la zona (a la última fila, por encima del dock; su celda está ocupada: a la libre más cercana) y no tapa a nadie" $S soltado#1 1024 600 n 30 rejilla en Consola 568 392
miracap "y se ve encima del dock, no debajo de la barra" ic5 texto 590 428 600 440 Consola negrita 30 570 46 580 Inicio
chk "sin «Alinear a la cuadrícula», el Bloc de notas se queda donde se suelta (dentro de la zona)" $S soltado#2 1024 600 n 30 en "Bloc de notas" 594 274
chk "y Mi equipo, soltado encima de él, no lo tapa: a la celda libre más cercana" $S soltado#3 1024 600 n 30 en "Bloc de notas" 594 274 fuera "Mi equipo" 594 274
chk "con la organización automática, se ordenan solos (como al principio)" $S organizados#6 1024 600 n 30 rejilla orden "$SIEMPRE"
chk "y lo que se arrastra vuelve a su sitio en el orden" $S soltado#4 1024 600 n 30 rejilla en Consola 8 164
mira "lo elegido en el submenú, por el puerto serie (la cuadrícula: fuera y otra vez; la automática: sí y no)" \
  "escritorio: organizar iconos: orden 0, automática 0, cuadrícula 0, primero 1$" "escritorio: organizar iconos: orden 0, automática 1, cuadrícula 1, primero 1$" \
  "escritorio: organizar iconos: orden 2, automática 0, cuadrícula 1, primero 1$"
mira "el teclado en el tapiz: la flecha abajo elige el primero y el de debajo; la «c», el siguiente que empieza por ella; la flecha arriba, el de encima; e Intro lo abre" \
  "escritorio: icono elegido con el teclado: Mi equipo$" "escritorio: icono elegido con el teclado: Bloc de notas$" "escritorio: icono elegido con el teclado: Consola$" \
  "escritorio: lanzo .BLOC.BIN"
miracap "el elegido con el teclado: un recuadro alrededor del dibujo y el rótulo sobre azul con su borde blanco (no solo el color)" ic7 \
  rgb 41 88 FFFFFF 10 rgb 42 89 0B2A52 20 texto 0 120 110 140 "Bloc de notas"
f=$X/ICONOS.INI; rm -f $f; mcopy -n -i $X/c.img ::ICONOS.INI $f > /dev/null 2>&1
if [ -f $f ] && grep -q "^orden=tipo" $f && grep -q "^automatica=no" $f && grep -q "^cuadricula=si" $f && grep -q "^sitio=s:Mi equipo|8|12" $f && fsck.fat -n $X/c.img > /dev/null 2>&1; then
  ok "lo elegido y dónde está cada uno, en ICONOS.INI (orden=tipo, automatica=no, cuadricula=si, los sitios), y el disco, sano"
else ko "ICONOS.INI: $(tr -d '\r' < $f 2> /dev/null | head -5 | tr '\n' ' ')"; fi
CAPTURAS=$G DISCO=$X/c.img ARRANQUE="escritorio modo=1024x600" PANTALLA=1024x600 MODULOS= $AQUI/nucleo32/probar.sh $X/SEMILLA.BIN ":pausa 3" ":captura ic8" > $X/b.sal 2>&1
chk "al arrancar otra vez, lo guardado: por tipo, cada uno donde estaba" $X/b.sal arranque 1024 600 n 30 rejilla por tipo $X/datos.txt
if ! grep -aq "excepción\|\*\*\*" $X/a.sal $X/b.sal; then ok "ni una excepción ni una queja del núcleo"; else ko "el núcleo se ha quejado:"; grep -ah "excepción\|\*\*\*" $X/a.sal $X/b.sal | head -3 | sed 's/^/        /'; fi
# ---- 2. a 800 x 600 ----
CAPTURAS=$G DISCO=$X/d30.img ARRANQUE=escritorio MODULOS= $AQUI/nucleo32/probar.sh $X/SEMILLA.BIN ":pausa 3" ":captura ic9" \
  ":clicd 700 100" ":clic 700 165" ":pausa 1" ":clic 700 117" ":pausa 1" ":raton 790 590" ":captura ic10" > $X/c.sal 2>&1
chk "a 800 x 600, al arrancar: los 30 en la zona, en la rejilla y sin taparse" $X/c.sal arranque 800 600 n 30 paginas 1 rejilla orden "$SIEMPRE"
chk "y por nombre" $X/c.sal organizados 800 600 n 30 rejilla por nombre $X/datos.txt
# ---- 3. cambiar la medida en marcha (la VBE de Bochs), con los iconos ----
# A 1280 x 720, la Consola a la derecha del todo; Configuración (el menú del
# tapiz) > Pantalla > «800 x 600» > Probar... > Intro (Mantener); y luego
# «1024 x 600» igual
CAPTURAS=$G DISCO=$X/d30.img ARRANQUE="escritorio modo=1280x720" PANTALLA=1280x720 MODULOS= $AQUI/nucleo32/probar.sh $X/SEMILLA.BIN ":pausa 3" \
  ":arrastra 64 190 1200 300" ":pausa 1" ":clicd 1100 100" ":clic 1100 117" ":pausa 2" ":clic 576 290" ":pausa 1" ":clic 477 289" ":clic 469 501" ":pausa 2" ":tecla ret" ":pausa 1" \
  ":clic 399 229" ":clic 229 441" ":pausa 2" ":tecla ret" ":pausa 1" ":captura ic11" > $X/d.sal 2>&1
chk "a 1280 x 720: la Consola, llevada a la derecha del todo" $X/d.sal soltado 1280 720 n 30 en Consola 1128 240
chk "y a 800 x 600 en marcha: todos dentro de la pantalla nueva (la Consola, a la derecha, sin salirse) y sin taparse" $X/d.sal "pantalla#1" 800 600 n 30 rejilla en Consola 680 240
chk "y a 1024 x 600 (el HP Mini): igual" $X/d.sal "pantalla#2" 1024 600 n 30 rejilla
S=$X/d.sal
mira "las dos medidas, puestas en marcha" "escritorio: la pantalla, a 800 x 600 (en marcha)" "escritorio: la pantalla, a 1024 x 600 (en marcha)"
# ---- 4. a 640 x 480 con 47: más estrechas y en dos páginas ----
cp $X/d30.img $X/d47.img
for n in 13 14 15 16 17 18 19 20; do printf 'nombre=Extra %s\r\nprograma=\\HOLA.BIN\r\nescritorio=si\r\n' $n > $X/mas/A$n.MNU; done
for n in 1 2 3 4 5 6 7 8 9; do printf 'm\r\n' > $X/mas/MAS$n.TXT; done
mcopy -i $X/d47.img $X/mas/*.MNU ::MENU/; mcopy -i $X/d47.img $X/mas/*.TXT ::ESCRITOR/
CAPTURAS=$G DISCO=$X/d30.img ARRANQUE="escritorio modo=640x480" PANTALLA=640x480 MODULOS= $AQUI/nucleo32/probar.sh $X/SEMILLA.BIN ":pausa 3" ":captura ic12" > $X/e.sal 2>&1
chk "a 640 x 480 con 30: las celdas, más estrechas (todos en una página)" $X/e.sal arranque 640 480 n 30 paginas 1 rejilla
CAPTURAS=$G DISCO=$X/d47.img ARRANQUE="escritorio modo=640x480" PANTALLA=640x480 MODULOS= $AQUI/nucleo32/probar.sh $X/SEMILLA.BIN ":pausa 3" ":captura ic13" \
  ":clic 544 300" ":raton 620 200" ":pausa 1" ":captura ic14" ":clic 620 200" ":tecla pgdn" ":pausa 1" > $X/f.sal 2>&1
chk "a 640 x 480 con 47: en dos páginas, sin taparse, y ninguno en la barra ni en el dock (ni encima de «Más iconos»)" $X/f.sal arranque 640 480 n 47 paginas 2 rejilla
miracap "«Más (1/2)» en la última celda" ic13 texto 480 300 600 330 "Más (1/2)"
S=$X/f.sal
mira "un clic en «Más» y Av Pág pasan de página" "escritorio: iconos, la página 2 de 2$" "escritorio: iconos, la página 1 de 2$"
miracap "la segunda página: «Más (2/2)»" ic14 texto 480 300 600 330 "Más (2/2)"
# ---- 5. sin el dock ----
cp $X/d30.img $X/g.img
printf 'visible=no\r\n' > $X/DOCK.INI; mcopy -i $X/g.img $X/DOCK.INI ::DOCK.INI
CAPTURAS=$G DISCO=$X/g.img ARRANQUE="escritorio modo=1024x600" PANTALLA=1024x600 MODULOS= $AQUI/nucleo32/probar.sh $X/SEMILLA.BIN ":pausa 3" > $X/g.sal 2>&1
chk "sin el dock (DOCK.INI: visible=no), la zona llega hasta la barra (7 filas) y nadie la toca" $X/g.sal accesos 1024 600 sindock n 30 rejilla en Teclas 8 468
if ! grep -aq "excepción\|\*\*\*" $X/c.sal $X/d.sal $X/e.sal $X/f.sal $X/g.sal; then ok "en las demás sesiones, ni una excepción ni una queja del núcleo"; else ko "el núcleo se ha quejado:"; grep -ah "excepción\|\*\*\*" $X/c.sal $X/d.sal $X/e.sal $X/f.sal $X/g.sal | head -3 | sed 's/^/        /'; fi
[ -n "${CAPTURAS_A:-}" ] && { mkdir -p "$CAPTURAS_A"; cp $G/ic*.ppm "$CAPTURAS_A"/ 2> /dev/null; }
G=$P/graf
}
