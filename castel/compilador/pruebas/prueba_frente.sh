#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# prueba_frente.sh - la prueba del frente del compilador de Castel escrito en
# Castel (compilador/lexico.cas y sintactico.cas): VOLCAR.EXE (volcar.cas),
# dentro de Hispa-8086, carga cada programa y vuelca su árbol; volcar.py hace
# lo mismo con las clases de ccastel.py. Para cada programa:
#   - si ccastel.py lo lee bien, los dos árboles tienen que ser iguales;
#   - si da un error (del léxico, de la gramática o del cargador), VOLCAR
#     tiene que dar el mismo, con su línea y su columna.
# Los programas: los ejemplos, los de ejemplos/errores, la biblioteca entera,
# los casos de compilador/pruebas/casos (lo que los ejemplos no tocan: letras
# de fuera del ASCII, escapes, bloques de ensamblador, cada error del léxico y
# de la gramática), el ensamblador (ensamblador/ensambla.cas) y el propio frente (volcar.cas con
# base.cas, lexico.cas y sintactico.cas).
# Necesita python3, nasm, qemu-system-i386, socat y mtools. Como mucho arranca
# un QEMU a la vez, y espera si ya hay dos en marcha.
#   VER=1 ./prueba_frente.sh      deja lo que sale en /tmp/castel-frente
set -u
AQUI=$(dirname "$(readlink -f "$0")")
COMP=$AQUI/..
CASTEL=$COMP/..
H=$CASTEL/../h8086
T=$(mktemp -d)
QPID=
acaba() { [ -n "$QPID" ] && kill $QPID 2>/dev/null; [ -n "${VER:-}" ] && { rm -rf /tmp/castel-frente; cp -r $T /tmp/castel-frente; }; rm -rf $T; }
trap acaba EXIT
fail=0
nok=0
ok() { nok=$((nok + 1)); [ -n "${VER:-}" ] && echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=$((fail + 1)); }

[ -f $H/build/hispa8086-1440.img ] || make -C $H > /dev/null 2>&1
[ -f $H/build/hispa8086-1440.img ] || { echo "No hay disquete de Hispa-8086 (make en h8086)"; exit 2; }
python3 -I $CASTEL/ccastel.py $AQUI/volcar.cas -o $T/volcar.asm -I $COMP > $T/comp.txt 2>&1 \
  && nasm -f bin $T/volcar.asm -o $T/VOLCAR.EXE || { echo "volcar.cas no compila:"; cat $T/comp.txt; exit 2; }
printf 'm\xc3\xb3dulo Toda\n    importar Textos, Archivos, Memoria, Argumentos, Lejos\n    funci\xc3\xb3n principal()\n    fin\nfin Toda.\n' > $T/toda.cas

mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; sleep 0.3; }

# tanda NOMBRE SEGUNDOS ARCHIVO...: vuelca esos programas en un arranque de
# Hispa-8086 y compara cada uno con lo que da volcar.py
tanda() {
  local nombre=$1 max=$2 i=0 f n
  shift 2
  cp $H/build/hispa8086-1440.img $T/a.img
  # los módulos, con su nombre de DOS: las 8 primeras letras, en mayúsculas
  for f in $CASTEL/biblioteca/*.cas $CASTEL/ejemplos/partes/*.cas $COMP/*.cas; do
    n=$(basename $f .cas | tr a-z A-Z | cut -c1-8)
    mcopy -o -i $T/a.img $f ::$n.CAS
  done
  printf '@ECO OFF\r\n' > $T/AUTOEXEC.BAT
  rm -rf $T/$nombre; mkdir $T/$nombre
  for f in "$@"; do
    i=$((i + 1))
    mcopy -o -i $T/a.img $f ::P$i.CAS
    printf 'VOLCAR P%d.CAS S%d.TXT > E%d.SAL\r\n' $i $i $i >> $T/AUTOEXEC.BAT
    python3 -I $AQUI/volcar.py $f -I $CASTEL/ejemplos/partes -I $COMP > $T/$nombre/$i.esp 2>&1
    echo $? > $T/$nombre/$i.cod
  done
  printf 'ECO [castel-fin]\r\n' >> $T/AUTOEXEC.BAT
  mcopy -o -i $T/a.img $T/VOLCAR.EXE $T/AUTOEXEC.BAT ::
  while [ $(pgrep -c qemu-system) -ge 2 ]; do sleep 2; done
  qemu-system-i386 -display none -monitor unix:$T/mon,server,nowait -cpu 486 -m 4 \
    -drive file=$T/a.img,format=raw,if=floppy,index=0 -boot a -no-reboot > /dev/null 2>&1 &
  QPID=$!
  sleep 3
  local hecho=0 ini=$(date +%s)
  while [ $(( $(date +%s) - ini )) -lt $max ]; do
    mon "pmemsave 0xb8000 4000 \"$T/scr.bin\""
    if [ -f $T/scr.bin ] && python3 -c "
import sys
d = open(sys.argv[1], 'rb').read()
sys.exit(0 if b'[castel-fin]' in d[::2] else 1)" $T/scr.bin; then hecho=1; break; fi
    sleep 1
  done
  mon quit
  wait $QPID 2>/dev/null
  QPID=
  [ $hecho = 1 ] || ko "$nombre: el lote no ha llegado al final en $max s"
  mcopy -n -i $T/a.img '::S*.TXT' '::E*.SAL' $T/$nombre/ > /dev/null 2>&1
  i=0
  for f in "$@"; do
    i=$((i + 1))
    n=$(basename $f)
    if [ "$(cat $T/$nombre/$i.cod)" = 0 ]; then
      if [ ! -f $T/$nombre/S$i.TXT ]; then
        ko "$n: no ha dejado el árbol: $(tr -d '\r' < $T/$nombre/E$i.SAL 2>/dev/null | iconv -f CP437 -t UTF-8 | head -1)"
      elif tr -d '\r' < $T/$nombre/S$i.TXT | cmp -s - $T/$nombre/$i.esp; then
        ok "$n: el mismo árbol ($(stat -c %s $T/$nombre/$i.esp) octetos)"
      else
        ko "$n: el árbol es distinto: $(tr -d '\r' < $T/$nombre/S$i.TXT | cmp - $T/$nombre/$i.esp 2>&1 | head -1)"
      fi
    else
      # (una letra que no está en la página 437 no se puede enseñar en Hispa-8086: sale «?»)
      esperado=$(head -1 $T/$nombre/$i.esp | sed -e 's/^.*\.cas://' -e 's/la letra «.» no está en la página 437/la letra «?» no está en la página 437/')
      dicho=$(tr -d '\r' < $T/$nombre/E$i.SAL 2>/dev/null | iconv -f CP437 -t UTF-8 | head -1 | sed 's/^.*\.CAS://')
      if [ "$dicho" = "$esperado" ]; then ok "$n: $dicho"; else ko "$n: dice «$dicho» y se esperaba «$esperado»"; fi
    fi
  done
}

echo "== El frente en Castel (léxico, gramática y cargador) frente al de ccastel.py =="
tanda ejemplos 240 $CASTEL/ejemplos/*.cas
tanda errores 240 $CASTEL/ejemplos/errores/*.cas
tanda casos 240 $AQUI/casos/*.cas
tanda mayores 400 $T/toda.cas $CASTEL/ensamblador/ensambla.cas
tanda frente 600 $AQUI/volcar.cas
echo "== $nok bien, $fail mal =="
exit $((fail > 0))
