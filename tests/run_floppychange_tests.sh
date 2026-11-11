#!/bin/bash
# Prueba del cambio de disquete sin reiniciar, con distintas densidades.
# Floppy change test (several densities).
#
# Como en un PC de verdad, los disquetes de 3,5" y los de 5,25" no caben en
# la misma disquetera, así que hay dos pasadas (dos arranques de QEMU):
#   1) B: disquetera de 3,5" 2,88 MB (lee 2,88/1,44/720):
#        1,44 -> 720 -> 2,88 -> 1,44, con escrituras en el de 720 y el de 2,88;
#      A: el de arranque se cambia por uno de datos (720 KB) y se vuelve a poner;
#      B: dos disquetes distintos con el mismo número de serie (mformat -N).
#   2) B: disquetera de 5,25" 1,2 MB (lee 1,2 MB y 360 KB):
#        1,2 -> 360 -> 1,2, con escrituras en los dos.
# Después, con mtools y fsck.fat: cada escritura está en su disquete, los
# demás no han cambiado y todos los sistemas de ficheros están bien.
#
# Variables: VERBOSE=1 (mostrar la consola), SLP (segundos por orden, 3),
# KEEP=1 (no borrar los temporales), FDTYPE35/FDTYPE525 (tipo de disquetera
# B en QEMU para cada pasada: 288 y 120). Con FDTYPE35=288 FDTYPE525=288 se
# reproduce lo que pasa al meter un 5,25" en una disquetera de 3,5".
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
cp "$IMG" $T/f.img
cp $T/f.img $T/f.orig
SLP=${SLP:-3}
FDTYPE35=${FDTYPE35:-288}
FDTYPE525=${FDTYPE525:-120}

# disquete de datos: nombre tamaño(KB) etiqueta fichero texto
mkfl() {
  dd if=/dev/zero of=$T/$1.img bs=1k count=$2 status=none
  mformat -i $T/$1.img -f $2 -v $3 ${SERIE:+-N $SERIE} ::
  printf '%s\r\n' "$5" > $T/$4
  mcopy -i $T/$1.img $T/$4 ::$4
  cp $T/$1.img $T/$1.orig
}
mkfl d1440 1440 ALTA144  F1440.TXT "Disquete de 1440 KB"
mkfl d720  720  DOBLE720 F720.TXT  "Disquete de 720 KB"
mkfl d2880 2880 EXTRA288 F2880.TXT "Disquete de 2880 KB"
mkfl d1200 1200 ALTA1200 F1200.TXT "Disquete de 1200 KB"
mkfl d360  360  DOBLE360 F360.TXT  "Disquete de 360 KB"
mkfl dA    720  DATOSA   X.TXT     "Datos en la unidad A"
# dos disquetes distintos con el MISMO número de serie; el primero con SUB\
SERIE=12345678 mkfl s1 1440 MISMO1 S1.TXT "Primero con la misma serie"
SERIE=12345678 mkfl s2 1440 MISMO2 S2.TXT "Segundo con la misma serie"
printf 'En la subcarpeta\r\n' > $T/SUB1.TXT
mmd -i $T/s1.img ::SUB; mcopy -i $T/s1.img $T/SUB1.TXT ::SUB/SUB1.TXT; cp $T/s1.img $T/s1.orig

mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; }
typ() { python3 - "$1" <<'EOF' | socat - UNIX-CONNECT:$T/mon >/dev/null 2>&1
import sys
m = {' ': 'spc', '.': 'dot', ':': 'shift-semicolon', '\\': 'backslash'}
for ch in sys.argv[1]:
    k = m.get(ch, ch.lower())
    if ch.isupper(): k = 'shift-' + k
    print('sendkey ' + k)
print('sendkey ret')
EOF
}
cmd() { typ "$1"; sleep $SLP; }
# cambiar disquete: $1 = 0 (A:, id fa) o 1 (B:, id fb), $2 = imagen
chg() { local id=fa; [ "$1" = 1 ] && id=fb; mon "change $id $T/$2.img raw"; sleep 2; }
# arrancar QEMU: $1 = log, $2 = disquete inicial de B, $3 = tipo de disquetera B
start() {
  rm -f $T/mon
  qemu-system-i386 -display none -serial file:$T/$1 -monitor unix:$T/mon,server,nowait -parallel none \
    -drive file=$T/f.img,format=raw,if=none,id=fa -drive file=$T/$2.img,format=raw,if=none,id=fb \
    -device floppy,unit=0,drive=fa,drive-type=144 -device floppy,unit=1,drive=fb,drive-type=$3 \
    -boot a -no-reboot ${QTRACE:-} &
  QP=$!
  sleep 7
}
stop() { kill $QP 2>/dev/null; wait $QP 2>/dev/null; }

# --- pasada 1: 3,5" en B y cambio de A ---
start log1 d1440 $FDTYPE35
cmd "DIR B:"
cmd "TYPE B:F1440.TXT"
chg 1 d720
cmd "DIR B:"
cmd "TYPE B:F720.TXT"
cmd "MD B:\\SIETE20"
chg 1 d2880
cmd "DIR B:"
cmd "TYPE B:F2880.TXT"
cmd "COPIAR A:\\HISPADOS\\EDITAR.COM B:"
cmd "MD B:\\NUEVA"
chg 1 d1440
cmd "DIR B:"
cmd "TYPE B:F1440.TXT"
chg 1 d720
cmd "DIR B:"
chg 0 dA
cmd "DIR A:"
cmd "TYPE A:X.TXT"
chg 0 f
cmd "DIR A:\\HISPADOS"
cmd "TYPE A:\\HISPADOS\\ALIAS.INI"
# misma serie: el contenido nuevo tiene que verse igual (cachés vaciadas)
chg 1 s1
cmd "DIR B:\\"
cmd "TYPE B:S1.TXT"
cmd "CD B:\\SUB"
cmd "DIR B:"
chg 1 s2
cmd "DIR B:\\"
cmd "TYPE B:\\S2.TXT"
cmd "TYPE B:\\S1.TXT"
cmd "DIR B:"
cmd "MD B:\\ENS2"
chg 1 s1
cmd "DIR B:\\"
stop

# --- pasada 2: 5,25" en B ---
start log2 d1200 $FDTYPE525
cmd "DIR B:"
cmd "TYPE B:F1200.TXT"
cmd "MD B:\\NUEVA"
cmd "COPIAR A:\\HISPADOS\\EDITAR.COM B:"
chg 1 d360
cmd "DIR B:"
cmd "TYPE B:F360.TXT"
cmd "MD B:\\TRES60"
cmd "COPIAR B:F360.TXT B:COPIA.TXT"
chg 1 d1200
cmd "DIR B:"
cmd "TYPE B:F1200.TXT"
stop

for n in 1 2; do tr -d '\r' < $T/log$n | iconv -f CP437 -t UTF-8 > $T/log$n.txt; done

fail=0
ok()  { echo "  OK   $1"; }
bad() { echo "  FAIL $1"; fail=1; }
# $1 = log, $2 = texto, $3 = veces mínimas (1)
check() {
  local n; n=$(grep -cF -- "$2" $T/$1.txt)
  if [ "$n" -ge "${3:-1}" ]; then ok "$2 (x$n)"; else bad "$2 (x$n, se esperaban ${3:-1})"; fi
}
# el texto $3 aparece en la salida de la orden $2 (hasta el siguiente indicador)
after() {
  if C="A:\\>$2" X="$3" awk 'index($0,"A:\\>")==1{on=($0==ENVIRON["C"])} on && index($0,ENVIRON["X"]){f=1} END{exit !f}' $T/$1.txt
  then ok "«$2» -> $3"; else bad "«$2» -> $3"; fi
}

echo "== Hispa-DOS cambio de disquete =="
echo "-- consola, pasada 1 (B de 3,5\", tipo $FDTYPE35) --"
check log1 "El volumen de la unidad B es ALTA144" 2
after log1 "TYPE B:F1440.TXT" "Disquete de 1440 KB"
check log1 "El volumen de la unidad B es DOBLE720" 2
after log1 "TYPE B:F720.TXT" "Disquete de 720 KB"
check log1 "SIETE20         <DIR>"
check log1 "El volumen de la unidad B es EXTRA288"
after log1 "TYPE B:F2880.TXT" "Disquete de 2880 KB"
check log1 "El volumen de la unidad A es DATOSA"
after log1 "TYPE A:X.TXT" "Datos en la unidad A"
check log1 "El volumen de la unidad A es HISPADOS"
check log1 "INSTALAR COM"
after log1 "TYPE A:\\HISPADOS\\ALIAS.INI" "EDIT=EDITAR"
check log1 "El volumen de la unidad B es MISMO1" 2
after log1 "TYPE B:S1.TXT" "Primero con la misma serie"
check log1 "SUB1     TXT"
check log1 "El volumen de la unidad B es MISMO2"
after log1 "TYPE B:\\S2.TXT" "Segundo con la misma serie"
after log1 "TYPE B:\\S1.TXT" "No se encontró el archivo"
echo "-- consola, pasada 2 (B de 5,25\", tipo $FDTYPE525) --"
check log2 "El volumen de la unidad B es ALTA1200" 2
after log2 "TYPE B:F1200.TXT" "Disquete de 1200 KB"
check log2 "El volumen de la unidad B es DOBLE360"
after log2 "TYPE B:F360.TXT" "Disquete de 360 KB"
check log2 "NUEVA           <DIR>"

echo "-- disquetes --"
for d in d1440 d720 d2880 d1200 d360 dA f s1 s2; do
  if fsck.fat -n $T/$d.img > $T/fsck_$d.txt 2>&1; then ok "fsck.fat -n $d"; else bad "fsck.fat -n $d"; sed 's/^/        /' $T/fsck_$d.txt; fi
done
for d in d1440 dA f s1; do
  if cmp -s $T/$d.img $T/$d.orig; then ok "$d sin cambios"; else bad "$d ha cambiado"; fi
done
mtools_ls() { mdir -b -i $T/$1.img ::/ 2>/dev/null | sed 's|^::/||' | sort | tr '\n' ' '; }
exp_d720="F720.TXT SIETE20/ "
exp_d2880="EDITAR.COM F2880.TXT NUEVA/ "
exp_d1200="EDITAR.COM F1200.TXT NUEVA/ "
exp_d360="COPIA.TXT F360.TXT TRES60/ "
exp_s2="ENS2/ S2.TXT "
for d in d720 d2880 d1200 d360 s2; do
  e=exp_$d; got=$(mtools_ls $d)
  if [ "$got" = "${!e}" ]; then ok "$d raíz: $got"; else bad "$d raíz: «$got» (se esperaba «${!e}»)"; fi
done
mcopy -n -i $T/f.orig ::HISPADOS/EDITAR.COM $T/EDITAR.ref
for d in d2880 d1200; do
  rm -f $T/EDITAR.got
  if mcopy -n -i $T/$d.img ::EDITAR.COM $T/EDITAR.got 2>/dev/null && cmp -s $T/EDITAR.got $T/EDITAR.ref; then
    ok "$d: EDITAR.COM idéntico al original"
  else bad "$d: EDITAR.COM distinto o ilegible"; fi
done
for spec in "d720:F720.TXT:Disquete de 720 KB" "d2880:F2880.TXT:Disquete de 2880 KB" \
            "d1200:F1200.TXT:Disquete de 1200 KB" "d360:F360.TXT:Disquete de 360 KB" \
            "d360:COPIA.TXT:Disquete de 360 KB"; do
  IFS=: read -r d f txt <<< "$spec"
  if [ "$(mtype -i $T/$d.img ::$f 2>/dev/null | tr -d '\r\n')" = "$txt" ]; then ok "$d: $f correcto"; else bad "$d: $f alterado o ausente"; fi
done

if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then
  echo "-- consola, pasada 1 --"; cat $T/log1.txt
  echo "-- consola, pasada 2 --"; cat $T/log2.txt
fi
if [ -n "${KEEP:-}" ]; then echo "(temporales en $T)"; else rm -rf $T; fi
exit $fail
