#!/bin/bash
# Prueba del Administrador de archivos (ARCHIVOS.HVP) de Hispa Ventanas.
# Arranca en QEMU con un disquete (A:) y un disco duro (C:) hecho aquí,
# maneja el entorno con el ratón y el teclado del monitor de QEMU, mira las
# capturas (tests/tools/ventanas_check.py) y lo que queda en los discos
# (mtools y fsck.fat).
#  1.ª sesión (VESA 800x600):
#  - el acceso sale el último de «Principal» y se abre en A:\ (vista Iconos),
#    con las unidades en el árbol y el espacio libre abajo;
#  - C: desde el árbol; vista Detalles; ordenar por tamaño y al revés;
#  - carpeta nueva, copiar y pegar, arrastrar a otra carpeta (mover), cortar
#    y pegar, cambiar el nombre (F2), arrastrar una carpeta al árbol (a otra
#    unidad: copia);
#  - copiar al disquete con la ventana de progreso; «Ya existe» (No); el
#    disco lleno (Cancelar: no queda nada a medias);
#  - eliminar una carpeta con todo (pregunta); propiedades y «Solo lectura»;
#  - buscar *.TXT en C:\ y sus carpetas; abrir un texto en el Bloc de notas;
#    el menú del botón derecho; Ctrl+E;
#  - ejecutar un .COM (LANZTEST) y volver: el Administrador se abre otra vez
#    en la misma carpeta con el programa marcado;
#  - el disquete fuera: «La unidad no está preparada»;
#  - cambiar el tamaño y maximizar; el menú del escritorio lo abre;
#  - los dos discos quedan bien (fsck.fat) y ARCHIVOS.INI guarda la vista.
#  2.ª sesión (VGA de 640x480 y 16 colores): se abre maximizada con lo
#  guardado; teclado: Tab al árbol, flechas, Mayús+F10.
# Deja capturas en build/archivos_*.png.
# Hispa Ventanas file manager test.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
B=build
T=$(mktemp -d)
CHK="python3 tests/tools/ventanas_check.py"
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
chk() { $CHK "$@" || fail=1; }
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; sleep "${2:-0.3}"; }
tecla() { local k; for k in "$@"; do mon "sendkey $k" 0.3; done; }
shot() { sleep 0.4; mon "screendump $T/$1.ppm" 0.6; }
send() { printf '%s\r' "$1"; sleep "${2:-1.5}"; }
muestra() { [ -f $T/$1.ppm ] && convert $T/$1.ppm $B/archivos_$2.png; }
RX=400; RY=300
ir() {
  local dx=$(( $1 - RX )) dy=$(( $2 - RY )) sx sy
  while [ $dx -ne 0 ] || [ $dy -ne 0 ]; do
    sx=$dx; [ $sx -gt 60 ] && sx=60; [ $sx -lt -60 ] && sx=-60
    sy=$dy; [ $sy -gt 60 ] && sy=60; [ $sy -lt -60 ] && sy=-60
    mon "mouse_move $sx $sy" 0.07
    dx=$(( dx - sx )); dy=$(( dy - sy ))
  done
  RX=$1; RY=$2; sleep 0.3
}
# el ratón, a la esquina de arriba a la izquierda (tras volver de un programa)
origen() { local i; for i in $(seq 1 16); do mon "mouse_move -60 -60" 0.05; done; RX=0; RY=0; sleep 0.3; }
clic() { ir $1 $2; mon "mouse_button 1" 0.25; mon "mouse_button 0" 0.5; }
derecho() { ir $1 $2; mon "mouse_button 2" 0.25; mon "mouse_button 0" 0.5; }
doble() { ir $1 $2; mon "mouse_button 1" 0.1; mon "mouse_button 0" 0.1; mon "mouse_button 1" 0.1; mon "mouse_button 0" 0.8; }
aparca() { ir 16 470; }
# a la raíz de una unidad, escribiéndola en la dirección («c» y «:»), y el foco a la lista
unidad() { clic 300 98; tecla ctrl-a $1 shift-dot ret; sleep 2.5; clic 500 400; }
# teclea un texto: letras, cifras y punto
escribe() {
  local t="$1" i c
  for ((i=0; i<${#t}; i++)); do
    c="${t:$i:1}"
    case "$c" in
      '.') tecla dot;;
      '*') tecla kp_multiply;;
      *) tecla "$c";;
    esac
  done
}
run() {
  local tmo=$1; shift
  rm -f $T/mon
  timeout $tmo qemu-system-i386 -display none -serial stdio -monitor unix:$T/mon,server,nowait \
    -parallel none -m 16 -drive file=$T/f.img,format=raw,if=floppy,index=0 \
    -drive file=$T/c.img,format=raw,if=ide -boot a -no-reboot "$@" > $T/raw 2>&1
  tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 | grep -v '^audio:' > $T/log.txt
}
HD="$T/c.img@@$((63*512))"
# un archivo del disco duro (o del disquete) es igual que el de aquí
igual() {
  local img=$1 ruta=$2 orig=$3
  rm -rf $T/sale
  if mcopy -n -i $img ::$ruta $T/sale 2>/dev/null && cmp -s $T/sale $orig; then
    ok "$ruta, copiado bien"
  else
    ko "$ruta no está o no es igual que el original"
  fi
}
esta() { if mdir -i $1 ::$2 > /dev/null 2>&1; then ok "está $2"; else ko "falta $2"; fi; }
noesta() { if mdir -i $1 ::$2 > /dev/null 2>&1; then ko "no debía estar $2"; else ok "ya no está $2"; fi; }
fsck_bien() {
  local img=$1 nombre=$2 skip=${3:-0}
  dd if=$img of=$T/part.img bs=512 skip=$skip status=none
  if fsck.fat -n $T/part.img > $T/fsck.txt 2>&1; then
    ok "fsck.fat: $nombre sin errores"
  else
    ko "fsck.fat: $nombre: $(tail -3 $T/fsck.txt | tr '\n' ' ')"
  fi
}

[ -f $B/LANZTEST.COM ] || { echo "falta $B/LANZTEST.COM (make build/LANZTEST.COM)"; exit 1; }
echo "== Administrador de archivos: copiar, mover, arrastrar, eliminar, buscar y abrir =="
cp "$IMG" $T/f.img
printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nFILES=30\r\n' > $T/CONFIG.SYS
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
for f in f.img; do
  if mdir -i $T/$f ::HISPADOS/PROGS/ARCHIVOS.HVP > /dev/null 2>&1; then
    ok "ARCHIVOS.HVP está en \\HISPADOS\\PROGS del disquete"
  else
    ko "falta ARCHIVOS.HVP en \\HISPADOS\\PROGS del disquete"
  fi
done
if [ -f $B/hispados-usb.img ]; then
  if MTOOLS_SKIP_CHECK=1 mdir -i $B/hispados-usb.img@@1048576 ::HISPADOS/PROGS/ARCHIVOS.HVP > /dev/null 2>&1; then
    ok "y en \\HISPADOS\\PROGS de la imagen USB"
  else
    ko "falta ARCHIVOS.HVP en \\HISPADOS\\PROGS de la imagen USB"
  fi
fi
echo "  (disquete: $(mdir -i $T/f.img :: | tail -1 | sed 's/ *//'))"
python3 tests/tools/mkhdd.py $T/c.img 16 > /dev/null
mmd -i $HD ::DATOS ::DATOS/SUB ::PROG ::VIEJA ::VIEJA/DENTRO
printf 'uno\r\n' > $T/UNO.TXT; printf 'dos dos\r\n' > $T/DOS.TXT; printf 'tres tres tres\r\n' > $T/TRES.TXT
head -c 200000 /dev/urandom > $T/GRANDE.BIN
mcopy -i $HD $T/UNO.TXT $T/DOS.TXT $T/GRANDE.BIN ::DATOS/
mcopy -i $HD $T/TRES.TXT ::DATOS/SUB/
mcopy -i $HD $T/UNO.TXT ::VIEJA/A.TXT
mcopy -i $HD $T/DOS.TXT ::VIEJA/DENTRO/B.TXT
mattrib -i $HD +r ::VIEJA/DENTRO/B.TXT
mcopy -i $HD $B/LANZTEST.COM ::PROG/PRUEBA.COM
# La ventana está en (60,40) y mide 620x420: su zona empieza en (62,85); el
# árbol, en (62,113), una fila cada 18 puntos; la lista en Detalles, sus filas
# desde y=134 (el centro, 142)
FI() { echo $(( 142 + 18 * $1 )); }        # el centro de la fila N de la lista
AR() { echo $(( 122 + 18 * $1 )); }        # el centro de la fila N del árbol
LX=300                                      # (x de los nombres de la lista)
{ sleep 6
  send 'KEYB SP' 2
  send 'A:\HISPADOS\HSPRATON' 2
  send 'VENTANAS' 6
  # ---- el grupo Principal: el Administrador es el último ----
  doble 102 129; sleep 1
  aparca
  shot a0
  tecla end ret; sleep 4
  aparca
  shot a1
  # ---- C: desde el árbol (la fila 5: A: está desplegada) ----
  clic 130 $(AR 5); sleep 2
  aparca
  shot a2
  # ---- vista Detalles; DATOS; ordenar por tamaño y al revés ----
  tecla f10; sleep 0.7; tecla right right; sleep 0.7; tecla d; sleep 1
  doble $LX $(FI 0); sleep 2
  aparca
  shot a3
  clic 450 122; sleep 1
  aparca
  shot a4
  clic 450 122; sleep 1
  aparca
  shot a5
  clic 280 122; sleep 1
  # ---- carpeta nueva: COPIAS (F7, como en GESTOR) ----
  tecla f7; sleep 1.5
  shot a6
  escribe copias; tecla ret; sleep 2
  aparca
  shot a7
  # ---- copiar UNO.TXT y pegarlo en COPIAS ----
  tecla u ctrl-c c ret; sleep 2
  tecla ctrl-v; sleep 2
  aparca
  shot a8
  tecla backspace; sleep 2
  # ---- arrastrar DOS.TXT (fila 2) a COPIAS (fila 0): se mueve ----
  ir $LX $(FI 2); mon "mouse_button 1" 0.4; ir 310 $(FI 1); ir $LX $(FI 0); sleep 0.5
  shot a9
  mon "mouse_button 0" 0.6; sleep 2
  aparca
  shot a10
  # ---- cortar UNO.TXT y pegarlo en SUB ----
  tecla u ctrl-x s ret; sleep 2
  tecla ctrl-v; sleep 2
  tecla backspace; sleep 2
  # ---- cambiar el nombre: GRANDE.BIN -> ENORME.BIN ----
  tecla g f2; sleep 1
  shot a11
  escribe enorme.bin; tecla ret; sleep 2
  aparca
  shot a12
  # ---- arrastrar la carpeta SUB (fila 1) al disquete del árbol: se copia ----
  ir $LX $(FI 1); mon "mouse_button 1" 0.4; ir 200 $(FI 1); ir 130 $(AR 0); sleep 0.5
  shot a13
  mon "mouse_button 0" 0.6; sleep 4
  aparca
  # ---- copiar ENORME.BIN al disquete ----
  clic $LX $(FI 2); tecla ctrl-c
  clic 130 $(AR 0); sleep 2
  tecla ctrl-v; sleep 8
  aparca
  shot a15
  # ---- otra vez: «Ya existe» -> No ----
  tecla ctrl-v; sleep 2
  shot a16
  tecla tab ret; sleep 2
  clic 500 400                              # (el foco, a la lista)
  # ---- y en A:\DOCS: no cabe (disco lleno) -> Cancelar ----
  tecla d; tecla d; tecla ret; sleep 2
  tecla ctrl-v; sleep 10
  shot a17
  tecla esc; sleep 2
  aparca
  shot a18
  # ---- copiar A:\HISPADOS a C:\ (la ventana de progreso) ----
  tecla backspace; sleep 2
  tecla h ctrl-c
  unidad c
  tecla ctrl-v; sleep 1.5
  shot a14
  sleep 30
  # ---- eliminar C:\VIEJA, con lo que tiene dentro ----
  unidad c
  tecla v delete; sleep 1
  shot a19
  tecla ret; sleep 3
  aparca
  shot a20
  # ---- propiedades de C:\DATOS\ENORME.BIN: solo lectura ----
  tecla d ret; sleep 2
  tecla e; sleep 0.5; tecla f10; sleep 0.7; tecla p; sleep 2
  shot a21
  tecla spc ret; sleep 1
  # ---- buscar *.TXT desde C:\ ----
  tecla backspace; sleep 2
  tecla f3; sleep 1
  shot a22
  tecla ctrl-a; escribe '*.txt'; tecla ret; sleep 4
  aparca
  shot a23
  # ---- el menú del botón derecho en un resultado; Ctrl+E ----
  derecho $LX $(FI 0); sleep 1
  shot a24
  tecla esc ctrl-e; sleep 1
  aparca
  shot a25
  # ---- abrir el primero en el Bloc de notas ----
  tecla home ret; sleep 3
  shot a26
  tecla alt-f4; sleep 1
  # ---- ejecutar un .COM y volver ----
  tecla backspace; sleep 2
  tecla p ret; sleep 2
  tecla p ret; sleep 12
  tecla ret
  sleep 16; origen; aparca
  shot a27
  # ---- el disquete, fuera ----
  mon "eject -f floppy0" 1
  clic 130 $(AR 0); sleep 3
  shot a28
  tecla esc; sleep 1
  mon "change floppy0 $T/f.img raw" 1
  # ---- cambiar el tamaño y maximizar ----
  ir 678 458; mon "mouse_button 1" 0.4; ir 720 500; ir 760 540; mon "mouse_button 0" 0.6; sleep 2
  aparca
  shot a29
  doble 300 52; sleep 2
  aparca
  shot a30
  # ---- cerrar; el menú del escritorio lo abre otra vez ----
  tecla alt-f4; sleep 1
  derecho 700 560; sleep 1
  shot a31
  tecla r; sleep 4
  aparca
  shot a32
  clic 756 13; sleep 1; tecla ret; sleep 3
  send 'ECHO TRAS-SALIR'
  mon quit
} | run 420 -vga std

# -- lo que se ve --
chk $T/a0.ppm busca 200 150 650 420 0 8 "ministrador"
chk $T/a1.ppm marco 60 40 620 420 activa titulo 60 40 activa "Administrador de archivos" \
  busca 140 88 400 104 0 9 "A:\\" busca 62 113 250 132 9 7 "Disquete (A:)" busca 62 113 250 240 0 9 "Disco (C:)" \
  busca 62 113 250 240 0 9 "HISPADOS" busca 250 113 680 300 0 9 "COMMAND.COM" busca 250 113 680 300 0 9 "README.TXT" \
  busca 300 430 680 450 0 6 "Libre:" busca 62 430 300 450 0 6 "10 objetos"
chk $T/a2.ppm busca 140 88 400 104 0 9 "C:\\" busca 250 113 680 200 0 9 "DATOS" busca 250 113 680 200 0 9 "VIEJA" \
  busca 300 430 680 450 0 6 "MB"
chk $T/a3.ppm busca 140 88 400 104 0 9 "C:\\DATOS" busca 250 113 680 132 0 6 "Tipo" \
  busca 260 $(( 134 + 18 )) 400 $(( 134 + 18 )) 0 9 "DOS.TXT" busca 260 $(( 134 + 54 )) 400 $(( 134 + 54 )) 0 9 "UNO.TXT" \
  busca 400 $(( 134 + 36 )) 680 $(( 134 + 36 )) 0 9 "200.000"
chk $T/a4.ppm busca 260 $(( 134 + 18 )) 400 $(( 134 + 18 )) 0 9 "UNO.TXT" busca 260 $(( 134 + 36 )) 400 $(( 134 + 36 )) 0 9 "DOS.TXT" \
  busca 260 $(( 134 + 54 )) 400 $(( 134 + 54 )) 0 9 "GRANDE.BIN"
chk $T/a5.ppm busca 260 $(( 134 + 18 )) 400 $(( 134 + 18 )) 0 9 "GRANDE.BIN" busca 260 $(( 134 + 54 )) 400 $(( 134 + 54 )) 0 9 "UNO.TXT"
chk $T/a6.ppm busca 150 150 650 400 0 8 "Nombre de la carpeta nueva:"
chk $T/a7.ppm busca 260 134 400 134 9 12 "COPIAS"
chk $T/a8.ppm busca 140 88 400 104 0 9 "C:\\DATOS\\COPIAS" busca 260 134 400 134 9 12 "UNO.TXT"
chk $T/a9.ppm busca 62 430 400 450 0 6 "Mover a C:\\DATOS\\COPIAS" px 251 $(( 134 + 8 )) 4 "el destino, enmarcado en terracota"
chk $T/a10.ppm nobusca 250 113 680 300 0 9 "DOS.TXT" nobusca 250 113 680 300 9 12 "DOS.TXT"
chk $T/a11.ppm busca 150 150 650 400 0 8 "Nombre nuevo:"
chk $T/a12.ppm busca 250 113 680 300 9 12 "ENORME.BIN" nobusca 250 113 680 300 0 9 "GRANDE.BIN"
chk $T/a13.ppm busca 62 430 400 450 0 6 "Copiar a A:\\"
chk $T/a14.ppm busca 150 150 650 400 0 8 "Copiando " busca 150 150 650 400 0 8 "a C:\\HISPADOS" \
  busca 150 150 650 450 0 6 "Cancelar"
chk $T/a15.ppm busca 250 113 680 400 9 7 "ENORME.BIN" busca 140 88 400 104 0 9 "A:\\"
chk $T/a16.ppm busca 150 150 700 400 0 8 "Ya existe «ENORME.BIN» en la carpeta de destino." \
  busca 150 150 700 400 0 6 "Sí a todo" busca 150 150 700 400 0 6 "Cancelar"
chk $T/a17.ppm busca 100 150 700 400 0 8 "El disco está lleno." busca 100 150 700 400 0 6 "Omitir"
chk $T/a18.ppm busca 140 88 400 104 0 9 "A:\\DOCS" nobusca 250 113 680 400 0 9 "ENORME.BIN" nobusca 250 113 680 400 9 12 "ENORME.BIN"
chk $T/a19.ppm busca 100 150 700 400 0 8 "¿Quiere eliminar «VIEJA»?" busca 100 150 700 400 0 8 "Se borra también todo lo que hay dentro"
chk $T/a20.ppm nobusca 250 113 680 400 0 9 "VIEJA" nobusca 250 113 680 400 9 12 "VIEJA"
chk $T/a21.ppm busca 100 100 700 450 0 8 "ENORME.BIN" busca 100 100 700 450 0 8 "200.000 octetos" \
  busca 100 100 700 450 0 8 "Solo lectura" busca 100 100 700 450 0 8 "C:\\DATOS"
chk $T/a22.ppm busca 100 150 700 400 0 8 "Nombre (vale * y ?), aquí y dentro:"
chk $T/a23.ppm busca 140 88 600 104 0 9 "Búsqueda de *.TXT en C:\\" busca 250 113 680 400 0 9 "TRES.TXT" \
  busca 250 113 680 400 0 9 "C:\\DATOS\\SUB" busca 250 113 680 400 0 9 "C:\\DATOS\\COPIAS" busca 62 430 400 450 0 6 "4 objetos"
chk $T/a24.ppm busca 250 113 790 500 0 8 "brir" busca 250 113 790 500 0 8 "piedades" busca 250 113 790 500 0 8 "minar"
chk $T/a25.ppm busca 62 430 400 450 0 6 "4 seleccionados"
chk $T/a26.ppm titulo 150 90 activa "Bloc de notas - C:\\DATOS\\COPIAS\\DOS.TXT" busca 150 130 700 200 0 9 "os dos"
chk $T/a27.ppm titulo 60 40 activa "Administrador de archivos" busca 140 88 400 104 0 9 "C:\\PROG" \
  busca 250 113 680 400 9 12 "PRUEBA.COM"
chk $T/a28.ppm busca 100 150 790 400 0 8 "No se puede leer la unidad A:" \
  busca 100 150 790 400 0 8 "La unidad no está preparada: ¿falta el disquete o el CD?" busca 100 150 790 400 0 6 "Reintentar"
chk $T/a29.ppm busca 62 500 400 545 0 6 "seleccionado" busca 600 500 760 545 0 6 "Libre:"
chk $T/a30.ppm busca 0 520 300 599 0 6 "seleccionado" busca 500 520 799 599 0 6 "Libre:"
chk $T/a31.ppm busca 400 300 799 599 0 8 "Administrador de"
chk $T/a32.ppm busca 0 60 200 100 0 6 "Dirección" busca 0 60 790 120 0 9 "C:\\PROG"
grep -q "TRAS-SALIR" $T/log.txt && ok "Hispa Ventanas ha terminado bien" || ko "no se ha vuelto a Hispa-DOS"
grep -q "HSPDPMI: excepción" $T/log.txt && ko "HSPDPMI: excepción" || ok "sin excepciones"
# -- los discos --
igual $HD DATOS/COPIAS/UNO.TXT $T/UNO.TXT
igual $HD DATOS/COPIAS/DOS.TXT $T/DOS.TXT
noesta $HD DATOS/DOS.TXT
igual $HD DATOS/SUB/UNO.TXT $T/UNO.TXT
noesta $HD DATOS/UNO.TXT
igual $HD DATOS/ENORME.BIN $T/GRANDE.BIN
noesta $HD DATOS/GRANDE.BIN
igual $T/f.img SUB/TRES.TXT $T/TRES.TXT
esta $HD DATOS/SUB/TRES.TXT
igual $T/f.img ENORME.BIN $T/GRANDE.BIN
noesta $T/f.img DOCS/ENORME.BIN
noesta $HD VIEJA
igual $HD HISPADOS/PROGS/ARCHIVOS.HVP $B/ARCHIVOS.HVP
igual $HD HISPADOS/VENTANAS.COM $B/VENTANAS.COM
if mattrib -i $HD ::DATOS/ENORME.BIN | grep -q ' R '; then ok "ENORME.BIN, de solo lectura"; else ko "ENORME.BIN sin solo lectura: $(mattrib -i $HD ::DATOS/ENORME.BIN)"; fi
mtype -i $HD ::PROG/LANZ.TXT 2>/dev/null | tr -d '\r' > $T/lanz.txt
grep -qx 'DIR=C:\\PROG' $T/lanz.txt && ok "PRUEBA.COM ha corrido en C:\\PROG" || ko "LANZ.TXT: $(cat $T/lanz.txt | tr '\n' ' ')"
mtype -i $T/f.img ::HISPADOS/PROGS/ARCHIVOS.INI 2>/dev/null | tr -d '\r' > $T/ini.txt
for l in 'Maximizada=1' 'Vista=1' 'Carpeta=C:\PROG'; do
  grep -qxF "$l" $T/ini.txt && ok "ARCHIVOS.INI: $l" || ko "ARCHIVOS.INI sin «$l»: $(tr '\n' ' ' < $T/ini.txt)"
done
mtype -i $T/f.img ::HISPADOS/CENTRO.INI 2>/dev/null | tr -d '\r' | iconv -f CP437 -t UTF-8 > $T/centro.txt
grep -qx 'Nombre=Administrador de archivos' $T/centro.txt && ok "CENTRO.INI: el acceso directo" || ko "CENTRO.INI sin el acceso"
grep -q '^Reabrir=' $T/centro.txt && ko "CENTRO.INI: queda «Reabrir=»" || ok "CENTRO.INI: sin «Reabrir=» al acabar"
fsck_bien $T/f.img "el disquete"
fsck_bien $T/c.img "el disco duro" 63
muestra a1 iconos; muestra a3 detalles; muestra a9 arrastrar; muestra a14 progreso; muestra a16 ya_existe
muestra a17 disco_lleno; muestra a19 eliminar; muestra a21 propiedades; muestra a23 buscar; muestra a24 menu
muestra a26 bloc; muestra a27 vuelta; muestra a28 sin_disquete; muestra a30 maximizada
[ -n "${GUARDAR:-}" ] && cp $T/*.ppm $T/c.img $T/f.img "$GUARDAR"/
[ $fail -ne 0 ] && { sed -n '/VENTANAS/,$p' $T/log.txt | tail -20; }

echo "== Administrador de archivos: VGA de 640x480 y 16 colores; teclado =="
RX=320; RY=240
{ sleep 6
  send 'KEYB SP' 2
  send 'A:\HISPADOS\HSPRATON' 2
  send 'VENTANAS /VGA' 8
  derecho 600 440; sleep 1
  tecla r; sleep 4
  ir 600 470
  shot b1
  tecla tab tab tab; sleep 0.5               # (de la lista: dirección, «Subir» y el árbol)
  tecla up; sleep 2
  shot b2
  tecla shift-f10; sleep 1
  shot b3
  tecla esc; sleep 0.5
  mon quit
} | run 200 -vga std

chk $T/b1.ppm tam 640 480 busca 0 60 200 100 0 6 "Dirección" busca 0 60 640 120 0 9 "C:\\PROG" \
  busca 0 400 640 479 0 6 "Libre:"
chk $T/b2.ppm busca 0 60 640 120 0 9 "C:\\HISPADOS" busca 0 100 220 300 9 12 "HISPADOS"
chk $T/b3.ppm busca 0 100 640 479 0 8 "Nueva " busca 0 100 640 479 0 8 "piedades"
muestra b1 vga640; muestra b3 arbol_menu
[ -n "${GUARDAR:-}" ] && cp $T/b*.ppm "$GUARDAR"/
rm -rf $T
exit $fail
