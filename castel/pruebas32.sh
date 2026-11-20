# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# pruebas32.sh - la parte 15 de pruebas.sh (no se ejecuta sola: la lee
# pruebas.sh con «.», y usa sus ok, ko, $T, $AQUI, $USU y $RED_USU).
#  15. CASTEL32: el compilador escrito en Castel, compilado para 32 bits
#      (CASTEL32.md §11). Con corre32 (en Linux, deprisa): CASTEL32 saca el
#      mismo ensamblador que ccastel.py en todos los programas de las pruebas
#      (y los mismos errores), y ENSAMB32 los mismos octetos que NASM; el
#      arranque en tres pasos en 32 bits. DENTRO DE HISPA-PENTIUM (QEMU, con un
#      disco FAT32): CASTEL32 compila el núcleo entero y todos sus programas, y
#      ENSAMB32 los ensambla, igual que ccastel.py y NASM; y CASTEL32 se compila
#      a sí mismo dos veces y sale idéntico (el punto fijo), con lo que dice
#      --gasto (las tablas, lejos del tope). Y EN HISPA-386/486 (Hispa-DOS con
#      HW32, si está el disquete: make y make nucprogs en la raíz): CASTELW
#      se compila a sí mismo, también hasta el punto fijo.

parte15() {
echo "== CASTEL32: el compilador en Castel, de 32 bits (Hispa-Pentium, HW32 y corre32) =="
local W=$T/c32 R=$AQUI/corre32/corre32.sh f n N bien mal e t0 t1
mkdir -p $W/ref $W/l
up() { basename $1 .cas | tr a-z A-Z | cut -c1-8; }
# paso 1: ccastel.py y NASM hacen los cuatro programas
comp32() {   # programa, salida (sin extensión) y las carpetas de -I
  local p=$1 s=$2; shift 2
  local args=() c
  for c in "$@"; do args+=(-I $AQUI/$c); done
  python3 -I $AQUI/ccastel.py $AQUI/$p -o $s.asm "${args[@]}" > /dev/null 2>&1 && nasm -f bin $s.asm -o $s.bin 2> /dev/null
}
if comp32 compilador/32/pentium/castel32.cas $W/castel32 compilador/32 biblioteca32/pentium compilador nucleo32 \
  && comp32 ensamblador/32/pentium/ensamb32.cas $W/ensamb32 biblioteca32/pentium ensamblador nucleo32 \
  && comp32 compilador/32/hw32/castelw.cas $W/castelw compilador/32 biblioteca32/hw32 compilador \
  && comp32 ensamblador/32/hw32/ensambw.cas $W/ensambw biblioteca32/hw32 ensamblador; then
  ok "paso 1: ccastel.py y NASM hacen CASTEL32.BIN ($(stat -c %s $W/castel32.bin) octetos) y ENSAMB32.BIN ($(stat -c %s $W/ensamb32.bin)) para Hispa-Pentium, y CASTELW.EXE ($(stat -c %s $W/castelw.bin)) y ENSAMBW.EXE ($(stat -c %s $W/ensambw.bin)) para HW32"
else
  ko "CASTEL32 no se puede hacer con ccastel.py y NASM"; return
fi

# ---- el árbol de los fuentes, con los nombres del DOS (8+3) ----
F=$W/f
mkdir -p $F/BIB $F/BIB32 $F/PARTES $F/NUC $F/C32 $F/B32P $F/COMP $F/ENS $F/E32 $F/S $F/ERR
for f in $AQUI/biblioteca/*.cas; do cp $f $F/BIB/$(up $f).CAS; done
for f in $AQUI/biblioteca32/*.cas; do cp $f $F/BIB32/$(up $f).CAS; done
for f in $AQUI/ejemplos/partes/*.cas; do cp $f $F/PARTES/$(up $f).CAS; done
for f in $AQUI/nucleo32/*.cas; do cp $f $F/NUC/$(up $f).CAS; done
for f in $AQUI/biblioteca32/pentium/*.cas; do cp $f $F/B32P/$(up $f).CAS; done
for f in $AQUI/compilador/*.cas; do cp $f $F/COMP/$(up $f).CAS; done
cp $AQUI/ensamblador/montador.cas $F/ENS/MONTADOR.CAS
cp $AQUI/compilador/32/pentium/castel32.cas $F/C32/CASTEL32.CAS
cp $AQUI/compilador/32/medidas.cas $F/C32/MEDIDAS.CAS
cp $AQUI/ensamblador/32/pentium/ensamb32.cas $F/E32/ENSAMB32.CAS

# ---- con corre32, en Linux: todos los programas de las pruebas ----
# (los de la raíz se compilan con -I BIB -I PARTES -I BIB32; los del núcleo,
# desde NUC, con -I \BIB32; CASTEL.CAS, el de 16 bits, desde COMP)
python3 -I $AQUI/azar.py 7 45 $W/azar16.cas $W/azar16.esp
python3 -I $AQUI/azar32.py 7 28 $W/azar32.cas $W/azar32.esp
python3 -I $AQUI/azar32.py 8 40 $W/azar32p.cas $W/azar32p.esp plano
printf 'm\xc3\xb3dulo Toda\n    importar Textos, Archivos, Memoria, Argumentos, Lejos\n    funci\xc3\xb3n principal()\n    fin\nfin Toda.\n' > $W/toda.cas
cp $AQUI/ensamblador/montador.cas $F/MONTADOR.CAS
RAIZ="$(echo $AQUI/ejemplos/*.cas $AQUI/ejemplos32/*.cas) $AQUI/ensamblador/ensambla.cas $W/toda.cas $W/azar16.cas $W/azar32.cas $W/azar32p.cas"
for f in $RAIZ; do cp $f $F/$(up $f).CAS; done
NUCS="$AQUI/nucleo32/semilla.cas $USU $RED_USU"
bien=0; mal=
for f in $RAIZ; do
  N=$(up $f); todo=; [ $N = TODA ] && todo=--todo
  python3 -I $AQUI/ccastel.py $todo $f -o $W/ref/$N.ASM -I $AQUI/ejemplos/partes > /dev/null 2>&1
  (cd $F && $R $W/castel32.bin $N.CAS -o $N.ASM -I BIB -I PARTES -I BIB32 $todo > $N.SAL 2>&1)
  if cmp -s $W/ref/$N.ASM $F/$N.ASM; then bien=$((bien + 1)); else mal="$mal $N"; fi
done
for f in $NUCS; do
  N=$(up $f)
  python3 -I $AQUI/ccastel.py $f -o $W/ref/$N.ASM > /dev/null 2>&1
  (cd $F/NUC && $R -C $F $W/castel32.bin $N.CAS -o \\S\\$N.ASM -I \\BIB32 > $F/S/$N.SAL 2>&1)
  if cmp -s $W/ref/$N.ASM $F/S/$N.ASM; then bien=$((bien + 1)); else mal="$mal $N"; fi
done
# y los cinco compiladores: el de 16 bits y los cuatro de 32
python3 -I $AQUI/ccastel.py $AQUI/compilador/castel.cas -o $W/ref/CASTEL.ASM > /dev/null 2>&1
(cd $F/COMP && $R -C $F $W/castel32.bin CASTEL.CAS -o \\S\\CASTEL.ASM -I \\BIB > $F/S/CASTEL.SAL 2>&1)
if cmp -s $W/ref/CASTEL.ASM $F/S/CASTEL.ASM; then bien=$((bien + 1)); else mal="$mal CASTEL"; fi
for x in castel32 ensamb32; do
  X=$(echo $x | tr a-z A-Z); d=C32; i="-I \\B32P -I \\COMP -I \\NUC -I \\BIB"
  [ $x = ensamb32 ] && { d=E32; i="-I \\B32P -I \\ENS -I \\NUC -I \\BIB"; }
  (cd $F && $R $W/castel32.bin $d\\$X.CAS -o \\S\\$X.ASM $i > $F/S/$X.SAL 2>&1)
  if cmp -s $W/$x.asm $F/S/$X.ASM; then bien=$((bien + 1)); else mal="$mal $X"; fi
done
if [ -z "$mal" ]; then ok "corre32: CASTEL32 saca el mismo ensamblador que ccastel.py en los $bien programas (ejemplos de 16 y de 32 bits, biblioteca entera, ENSAMBLA, cuentas al azar, el núcleo de Hispa-Pentium y todos sus programas, CASTEL de 16 bits, CASTEL32 y ENSAMB32)"
else ko "corre32: CASTEL32 no saca lo mismo que ccastel.py en:$mal"; fi
# los errores: los mismos mensajes, con su línea y su columna
bien=0; mal=
for f in $AQUI/ejemplos/errores/*.cas $AQUI/ejemplos32/errores/*.cas; do
  n=$(basename $f .cas)
  esperado=$(sed -n 's/^; ERROR: //p' $f | head -1)
  cp $f $F/ERR/E.CAS
  dicho=$(cd $F/ERR && $R -C $F $W/castel32.bin E.CAS -I \\BIB -I \\PARTES 2>&1 | tr -d '\r' | iconv -f CP437 -t UTF-8 | head -1 | sed 's/^.*\.CAS://')
  if [ "$dicho" = "$esperado" ]; then bien=$((bien + 1)); else mal="$mal $n"; fi
done
if [ -z "$mal" ]; then ok "corre32: CASTEL32 dice los $bien errores igual que ccastel.py (con su línea y su columna)"
else ko "corre32: CASTEL32 no dice igual los errores de:$mal"; fi
# ENSAMB32 frente a NASM, con todo lo anterior
bien=0; mal=
for f in $W/ref/*.ASM; do
  N=$(basename $f .ASM)
  nasm -f bin $f -o $W/l/$N.NASM 2> /dev/null || continue
  (cd $W/l && $R -C $W $W/ensamb32.bin ..\\ref\\$N.ASM $N.BIN > $N.SAL 2>&1)
  if cmp -s $W/l/$N.NASM $W/l/$N.BIN; then bien=$((bien + 1)); else mal="$mal $N"; fi
done
if [ -z "$mal" ]; then ok "corre32: ENSAMB32 da los mismos octetos que NASM en los $bien archivos (el núcleo de Hispa-Pentium incluido)"
else ko "corre32: ENSAMB32 no da lo mismo que NASM en:$mal"; fi
# el arranque en tres pasos, en Linux
(cd $F && for g in 2 3; do
   p=$((g - 1)); c=$W/castel32.bin; e=$W/ensamb32.bin
   [ $g = 3 ] && { c=$F/C2.BIN; e=$F/E2.BIN; }
   $R $c C32\\CASTEL32.CAS -o C$g.ASM -I B32P -I COMP -I NUC -I BIB > P$g.SAL 2>&1
   $R $c E32\\ENSAMB32.CAS -o E$g.ASM -I B32P -I ENS -I NUC -I BIB > Q$g.SAL 2>&1
   $R $e C$g.ASM C$g.BIN > R$g.SAL 2>&1
   $R $e E$g.ASM E$g.BIN > S$g.SAL 2>&1
 done)
if cmp -s $W/castel32.asm $F/C2.ASM && cmp -s $W/castel32.bin $F/C2.BIN && cmp -s $F/C2.ASM $F/C3.ASM && cmp -s $F/C2.BIN $F/C3.BIN \
   && cmp -s $W/ensamb32.bin $F/E2.BIN && cmp -s $F/E2.BIN $F/E3.BIN; then
  ok "corre32: el arranque en tres pasos de CASTEL32 y ENSAMB32 (paso 2 = paso 1 con ccastel.py y NASM; paso 3 = paso 2)"
else ko "corre32: el arranque en tres pasos de CASTEL32 no llega al punto fijo"; fi
rm -f $F/*.ASM $F/*.SAL $F/C?.* $F/E?.* $F/S/*

# ---- dentro de Hispa-Pentium (QEMU), con un disco FAT32 ----
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/semilla.cas -o $W/semilla.asm > /dev/null 2>&1 && nasm -f bin $W/semilla.asm -o $W/SEMILLA.BIN
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/uordenes.cas -o $W/ordenes.asm > /dev/null 2>&1 && nasm -f bin $W/ordenes.asm -o $F/ORDENES.BIN
cp $W/castel32.bin $F/CASTEL32.BIN; cp $W/ensamb32.bin $F/ENSAMB32.BIN
rm -f $F/*.CAS $F/MONTADOR.CAS
{ printf '@ECHO OFF\r\nECHO [empieza]\r\nCD NUC\r\n'
  for f in $NUCS; do N=$(up $f)
    g=; [ $N = SEMILLA ] && g=--gasto
    printf '\\CASTEL32 %s.CAS -o \\S\\%s.ASM -I \\BIB32 %s > \\S\\%s.SAL\r\n' $N $N "$g" $N
  done
  printf 'CD \\\r\nECHO [compilado]\r\n'
  for f in $NUCS; do N=$(up $f); printf 'ENSAMB32 S\\%s.ASM S\\%s.BIN > S\\%s.ESA\r\n' $N $N $N; done
  printf 'ECHO [ensamblado]\r\n'
  printf 'CASTEL32 C32\\CASTEL32.CAS -o C2.ASM -I B32P -I COMP -I NUC -I BIB --gasto > P2.SAL\r\n'
  printf 'ECHO [paso2-compilado]\r\n'
  printf 'ENSAMB32 C2.ASM C2.BIN > R2.SAL\r\nCASTEL32 E32\\ENSAMB32.CAS -o E2.ASM -I B32P -I ENS -I NUC -I BIB > Q2.SAL\r\nENSAMB32 E2.ASM E2.BIN > S2.SAL\r\n'
  printf 'ECHO [paso2]\r\n'
  printf 'C2 C32\\CASTEL32.CAS -o C3.ASM -I B32P -I COMP -I NUC -I BIB > P3.SAL\r\nE2 C3.ASM C3.BIN > R3.SAL\r\n'
  printf 'ECHO [paso3]\r\n'; } > $F/TODO.BAT
rm -f $W/disco.img
mkfs.fat -F 32 -s 1 -n CASTEL32 -C $W/disco.img 98304 > /dev/null 2>&1
(cd $F && MTOOLS_SKIP_CHECK=1 mcopy -s -i $W/disco.img * ::) || { ko "no se puede hacer el disco de CASTEL32"; return; }
rm -f $W/sal $W/mon
qemu-system-i386 -kernel $W/SEMILLA.BIN -append "ordenes escritura" -drive file=$W/disco.img,format=raw,if=ide,index=0 \
  -display none -monitor unix:$W/mon,server,nowait -serial file:$W/sal -device isa-debug-exit,iobase=0xf4,iosize=0x04 \
  -no-reboot -nic none -m 128 > /dev/null 2>&1 &
QPID=$!
monp() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$W/mon > /dev/null 2>&1; sleep 0.08; }
for ((i = 0; i < 150; i++)); do grep -q 'C:.>' $W/sal 2> /dev/null && break; sleep 0.2; done
sleep 0.5
for k in shift-t shift-o shift-d shift-o ret; do monp "sendkey $k"; done
declare -A cuando=()
t0=$(date +%s.%N)
for ((i = 0; i < 3000; i++)); do
  for m in compilado ensamblado paso2-compilado paso2 paso3; do
    [ -z "${cuando[$m]:-}" ] && grep -q "^\[$m\]" $W/sal 2> /dev/null && cuando[$m]=$(date +%s.%N)
  done
  [ -n "${cuando[paso3]:-}" ] && break
  kill -0 $QPID 2> /dev/null || break
  sleep 0.2
done
sleep 1
monp quit
wait $QPID 2> /dev/null
QPID=
seg() { [ -n "${cuando[$2]:-}" ] && echo "$(echo "${cuando[$2]} - $1" | bc | sed 's/^\./0./' | cut -c1-5) s" || echo "?"; }
[ -n "${cuando[paso3]:-}" ] || ko "CASTEL32 dentro de Hispa-Pentium: el lote no ha llegado al final ($(tr -d '\r' < $W/sal | tail -1))"
mkdir -p $W/den
MTOOLS_SKIP_CHECK=1 mcopy -s -n -i $W/disco.img ::S ::C2.ASM ::C2.BIN ::C3.ASM ::C3.BIN ::E2.BIN ::P2.SAL $W/den/ > /dev/null 2>&1
bien=0; mal=
for f in $NUCS; do
  N=$(up $f)
  nasm -f bin $W/ref/$N.ASM -o $W/den/$N.NASM 2> /dev/null
  if cmp -s $W/ref/$N.ASM $W/den/S/$N.ASM && cmp -s $W/den/$N.NASM $W/den/S/$N.BIN; then bien=$((bien + 1)); else mal="$mal $N"; fi
done
if [ -z "$mal" ]; then ok "dentro de Hispa-Pentium: CASTEL32 compila el núcleo entero y sus $((bien - 1)) programas ($(seg $t0 compilado)) y ENSAMB32 los ensambla ($(seg ${cuando[compilado]:-0} ensamblado)): el mismo ensamblador que ccastel.py y los mismos octetos que NASM (NUCLEO: $(stat -c %s $W/den/S/SEMILLA.BIN 2> /dev/null) octetos)"
else ko "dentro de Hispa-Pentium, CASTEL32/ENSAMB32 no dan lo mismo en:$mal"; fi
# el gasto del núcleo: lejos del tope
tr -d '\r' < $W/den/S/SEMILLA.SAL 2> /dev/null | iconv -f CP437 -t UTF-8 > $W/gasto.txt
if grep -q "árbol (lo más) " $W/gasto.txt && awk '/ de [0-9]+ \([0-9]+ %\)/ { p = $(NF-1); sub(/\(/, "", p); if (p + 0 >= 50) m = 1 } END { exit m }' $W/gasto.txt; then
  ok "con CASTEL32 las tablas del núcleo, lejos del tope: $(grep -E 'nombres|árbol|declarados' $W/gasto.txt | sed 's/^ *//; s/  */ /g' | paste -sd ';' | sed 's/;/; /g')"
else ko "CASTEL32 --gasto con el núcleo: alguna tabla pasa de la mitad, o no lo dice: $(head -3 $W/gasto.txt | paste -sd ' ')"; fi
if cmp -s $W/castel32.asm $W/den/C2.ASM && cmp -s $W/castel32.bin $W/den/C2.BIN && cmp -s $W/ensamb32.bin $W/den/E2.BIN; then
  ok "dentro de Hispa-Pentium, paso 2: CASTEL32 se compila a sí mismo ($(seg ${cuando[ensamblado]:-0} paso2-compilado)) y ENSAMB32 lo ensambla: lo mismo que el paso 1 ($(wc -l < $W/den/C2.ASM) líneas, $(stat -c %s $W/den/C2.BIN) octetos)"
else ko "dentro de Hispa-Pentium, el paso 2 no da lo del paso 1: $(tr -d '\r' < $W/den/P2.SAL 2> /dev/null | head -1)"; fi
if [ -s $W/den/C3.ASM ] && cmp -s $W/den/C2.ASM $W/den/C3.ASM && cmp -s $W/den/C2.BIN $W/den/C3.BIN; then
  ok "dentro de Hispa-Pentium, paso 3: el CASTEL32 compilado por sí mismo se compila otra vez y sale idéntico (el punto fijo, $(seg ${cuando[paso2]:-0} paso3))"
else ko "dentro de Hispa-Pentium, el paso 3 no es igual al 2"; fi

# ---- en Hispa-386/486: CASTELW con HW32, encima de Hispa-DOS ----
local B=$AQUI/../build
if [ -f $B/hispados.img ] && [ -f $B/HW32.COM ]; then
  local DW=$W/dos
  mkdir -p $DW
  dos_lote() {   # el disquete B: ($DW/b.img) y las órdenes, una por línea
    cp $B/hispados.img $DW/f.img
    printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nFILES=30\r\n' > $DW/CONFIG.SYS
    MTOOLS_SKIP_CHECK=1 mcopy -o -i $DW/f.img $DW/CONFIG.SYS ::CONFIG.SYS
    MTOOLS_SKIP_CHECK=1 mcopy -o -i $DW/f.img $B/HW32.COM ::BIN/HW32.COM
    rm -f $DW/raw $DW/sock
    qemu-system-i386 -display none -chardev socket,id=s,path=$DW/sock,server=on,wait=off,logfile=$DW/raw -serial chardev:s \
      -monitor none -parallel none -m 64 -drive file=$DW/f.img,format=raw,if=floppy,index=0 \
      -drive file=$DW/b.img,format=raw,if=floppy,index=1 -boot a -no-reboot > /dev/null 2>&1 &
    QPID=$!
    for ((i = 0; i < 50; i++)); do [ -S $DW/sock ] && break; sleep 0.1; done
    exec 3> >(socat - UNIX-CONNECT:$DW/sock > /dev/null 2>&1)
    for ((i = 0; i < 300; i++)); do grep -aq 'A:.>' $DW/raw 2> /dev/null && break; sleep 0.2; done
    printf 'A:\\HISPADOS\\HSPDPMI\r' >&3; sleep 2
    printf 'B:\r' >&3
    for ((i = 0; i < 50; i++)); do grep -aq 'B:.>' $DW/raw 2> /dev/null && break; sleep 0.2; done
    local l antes hecho
    while read -r l; do
      antes=$(grep -ao 'B:.>' $DW/raw | wc -l)
      printf '%s\r' "$l" >&3
      hecho=0
      for ((i = 0; i < 3000; i++)); do [ "$(grep -ao 'B:.>' $DW/raw | wc -l)" -gt "$antes" ] && { hecho=1; break; }; sleep 0.2; done
      [ $hecho = 1 ] || break
    done
    exec 3>&-
    kill $QPID 2> /dev/null; wait $QPID 2> /dev/null
    QPID=
  }
  hazb() {   # el disquete B: con los fuentes de CASTELW y el compilador que toca
    rm -f $DW/b.img
    MTOOLS_SKIP_CHECK=1 mformat -C -f 2880 -i $DW/b.img ::
    MTOOLS_SKIP_CHECK=1 mmd -i $DW/b.img ::BIB ::COMP ::B32W ::C32 ::ENS ::E32
    for f in $AQUI/biblioteca/*.cas; do MTOOLS_SKIP_CHECK=1 mcopy -i $DW/b.img $f ::BIB/$(up $f).CAS; done
    for f in $AQUI/compilador/*.cas; do MTOOLS_SKIP_CHECK=1 mcopy -i $DW/b.img $f ::COMP/$(up $f).CAS; done
    for f in $AQUI/biblioteca32/hw32/*.cas; do MTOOLS_SKIP_CHECK=1 mcopy -i $DW/b.img $f ::B32W/$(up $f).CAS; done
    MTOOLS_SKIP_CHECK=1 mcopy -i $DW/b.img $AQUI/compilador/32/hw32/castelw.cas ::C32/CASTELW.CAS
    MTOOLS_SKIP_CHECK=1 mcopy -i $DW/b.img $AQUI/compilador/32/medidas.cas ::C32/MEDIDAS.CAS
    MTOOLS_SKIP_CHECK=1 mcopy -i $DW/b.img $AQUI/ensamblador/montador.cas ::ENS/MONTADOR.CAS
    MTOOLS_SKIP_CHECK=1 mcopy -i $DW/b.img $AQUI/ensamblador/32/hw32/ensambw.cas ::E32/ENSAMBW.CAS
    MTOOLS_SKIP_CHECK=1 mcopy -i $DW/b.img $1 ::CW.EXE
    MTOOLS_SKIP_CHECK=1 mcopy -i $DW/b.img $2 ::EW.EXE
  }
  for g in 2 3; do
    if [ $g = 2 ]; then hazb $W/castelw.bin $W/ensambw.bin; else hazb $DW/C2.EXE $DW/E2.EXE; fi
    t0=$(date +%s.%N)
    dos_lote <<X
A:\\BIN\\HW32 CW.EXE C32\\CASTELW.CAS -o C$g.ASM -I B32W -I COMP -I BIB --gasto > P$g.SAL
A:\\BIN\\HW32 EW.EXE C$g.ASM C$g.EXE > R$g.SAL
DEL C$g.ASM
A:\\BIN\\HW32 CW.EXE E32\\ENSAMBW.CAS -o E$g.ASM -I B32W -I ENS -I BIB > Q$g.SAL
A:\\BIN\\HW32 EW.EXE E$g.ASM E$g.EXE > S$g.SAL
X
    t1=$(date +%s.%N)
    eval "tdos$g=$(echo "$t1 - $t0" | bc | cut -c1-5)"
    MTOOLS_SKIP_CHECK=1 mcopy -n -i $DW/b.img ::C$g.EXE ::E$g.EXE ::E$g.ASM ::P$g.SAL $DW/ > /dev/null 2>&1
  done
  if cmp -s $W/castelw.bin $DW/C2.EXE && cmp -s $W/ensambw.asm $DW/E2.ASM && cmp -s $W/ensambw.bin $DW/E2.EXE; then
    ok "en Hispa-DOS con HW32 (Hispa-386/486), paso 2: CASTELW se compila a sí mismo y ENSAMBW lo ensambla: el mismo CASTELW.EXE que el paso 1 ($(stat -c %s $DW/C2.EXE) octetos, ${tdos2:-?} s con ENSAMBW incluido)"
  else ko "en Hispa-DOS con HW32, el paso 2 de CASTELW no da lo del paso 1: $(tr -d '\r' < $DW/P2.SAL 2> /dev/null | head -1)"; fi
  if [ -s $DW/C3.EXE ] && cmp -s $DW/C2.EXE $DW/C3.EXE && cmp -s $DW/E2.EXE $DW/E3.EXE; then
    ok "en Hispa-DOS con HW32, paso 3: el CASTELW del paso 2 se compila otra vez y sale idéntico (el punto fijo)"
  else ko "en Hispa-DOS con HW32, el paso 3 de CASTELW no es igual al 2"; fi
else
  echo "  (sin el disquete de Hispa-DOS no se prueba CASTELW con HW32: make y make nucprogs en la raíz)"
fi
}
