#!/bin/bash
# [0.9.31] Guardar sin perder nada (auditoría de Hispa-386, 25-11-2026: el
# crítico 2 y los altos 6 y 7, y el medio 22), en QEMU con Hispa Ventanas:
#  - Pluma: escribir y pulsar «Salir» > «Volver a Hispa-DOS»: pregunta
#    «¿Quiere guardarlos?» (WM_QUERYENDSESSION); «Cancelar» se queda en Hispa
#    Ventanas, «No» sale a Hispa-DOS;
#  - Pluma: «Guardar como» con el nombre de otro archivo que ya está: pregunta
#    si se reemplaza; «No» vuelve al cuadro y el archivo sigue igual; «Sí» lo
#    reemplaza; tras guardar no quedan ni el .$$$ ni la .BAK;
#  - Pluma: en un disquete casi lleno, Guardar falla («No se ha podido
#    guardar») y el archivo de antes está entero (antes quedaba vacío o a
#    medias), sin temporal;
#  - Pluma: un .TXT de más de 30.000 letras se abre recortado, lo dice, y
#    Ctrl+G pide un nombre nuevo (no lo escribe recortado encima);
#  - Cuentas: un .CSV de 1.100 filas, lo mismo;
#  - Facturas: un FACTURAS.DAT que no vale lo dice al abrir, no lo da por
#    «sin facturas» y no guarda encima (el archivo queda igual);
#  - CENTRO.INI que falta (se fue la luz al guardar): se lee CENTRO.BAK.
# Deja capturas en build/guardar_*.png. Necesita qemu-system-i386, mtools,
# socat e ImageMagick (como las demás).
set -u
IMG=${1:-build/hispados.img}
. tests/tools/hvp_arnes.sh
for m in PLUMA CUENTAS FACTURAS; do
  [ -f $B/$m.HVP ] || NASM=${NASM:-nasm} python3 tools/mkhvp.py programas/$(echo $m | tr A-Z a-z).asm $B/$m.HVP > /dev/null
done
reabre() { printf '[Centro]\r\nReabrir=A:\\HISPADOS\\PROGS\\%s\r\n' "$1" > $T/CENTRO.INI
           mcopy -o -i $T/f.img $T/CENTRO.INI ::HISPADOS/CENTRO.INI; }
suma() { mtype -i $T/f.img "$1" 2>/dev/null | md5sum | cut -c1-32; }
esta() { mdir -b -i $T/f.img "$1" > /dev/null 2>&1; }
noesta() { if esta "$1"; then ko "$2: queda $1"; else ok "$2: no queda $1"; fi; }
# el sitio libre del disquete, en octetos
libre() { mdir -i $T/f.img :: 2>/dev/null | sed -n 's/^ *\([0-9 ]*[0-9]\) bytes free.*/\1/p' | tr -d ' '; }

# SOLO="salir csv": solo esas partes; GUARDAR=carpeta: las capturas, allí
parte() { [ -z "${SOLO:-}" ] || [[ " $SOLO " == *" $1 "* ]]; }

if parte salir; then
echo "== Pluma: salir con cambios sin guardar (WM_QUERYENDSESSION) =="
prepara $B/PLUMA.HVP
{ arranca
  escribe "Hola"; aparca
  clic 756 13; sleep 1.5; tecla ret; sleep 2.5; shot q1   # Salir > Volver a Hispa-DOS
  tecla esc; sleep 1.5; aparca; shot q2                    # Cancelar: se queda
  clic 756 13; sleep 1.5; tecla ret; sleep 2.5
  clic 399 335; sleep 6                                    # No: sale
  sal; } | run 150 -vga std
chk $T/q1.ppm titulo 218 234 activa "Pluma" busca 220 260 580 320 0 8 "El documento tiene cambios sin guardar." \
  busca 220 260 580 320 0 8 "¿Quiere guardarlos?"
chk $T/q2.ppm titulo 70 40 activa "Pluma - (sin nombre)" nobusca 220 260 580 320 0 8 "¿Quiere guardarlos?"
if grep -q '^A:\\>$' <(tail -3 $T/log.txt); then ok "«No»: sale a Hispa-DOS"; else ko "«No»: no ha salido a Hispa-DOS"; tail -5 $T/log.txt; fi
muestra q1 guardar_salir
fi

if parte reemplazar; then
echo "== Pluma: Guardar como encima de otro archivo; el disquete casi lleno =="
prepara $B/PLUMA.HVP
printf 'No me borres.\r\n' > $T/existe.txt
mcopy -o -i $T/f.img $T/existe.txt ::EXISTE.TXT
s0=$(suma ::EXISTE.TXT)
{ arranca
  escribe "Hola"; aparca
  tecla ctrl-g; sleep 1.2; escribe "existe.txt"; tecla ret; sleep 2; shot r1   # ya está: ¿reemplazarlo?
  tecla tab ret; sleep 2; shot r2                                             # No: otra vez el cuadro
  tecla esc; sleep 1
  sal; } | run 120 -vga std
chk $T/r1.ppm busca 150 230 650 340 0 8 "Ya hay un archivo con ese nombre:" busca 150 230 650 340 0 8 "existe.txt"
chk $T/r2.ppm busca 150 150 650 260 0 8 "Nombre del archivo:"
if [ "$(suma ::EXISTE.TXT)" = "$s0" ]; then ok "«No»: EXISTE.TXT sigue igual"; else ko "«No»: EXISTE.TXT ha cambiado"; fi
muestra r1 guardar_reemplazar
{ arranca
  escribe "Hola"; aparca
  tecla ctrl-g; sleep 1.2; escribe "existe.txt"; tecla ret; sleep 2
  tecla ret; sleep 3; aparca; shot r3                                         # Sí: se reemplaza
  sal; } | run 120 -vga std
chk $T/r3.ppm titulo 70 40 activa "Pluma - existe.txt"
if mtype -i $T/f.img ::EXISTE.TXT | tr -d '\r' | grep -qx 'Hola'; then ok "«Sí»: EXISTE.TXT reemplazado"; else ko "«Sí»: EXISTE.TXT no es el nuevo"; fi
noesta ::EXISTE.\$\$\$ "guardar"; noesta ::EXISTE.BAK "guardar"
# casi lleno: un texto de 6 K y sitio solo para 2 K más (cabría encima, pero
# no a la vez que el de antes): no se guarda, y el de antes queda entero
python3 -c "
import sys
sys.stdout.write(''.join('Linea %04d del texto que no se tiene que perder.\r\n' % i for i in range(120)))" > $T/lleno.txt
mcopy -o -i $T/f.img $T/lleno.txt ::LLENO.TXT
reabre 'PLUMA.HVP A:\LLENO.TXT'
l=$(libre); head -c $(( l - 2048 )) /dev/zero > $T/relleno.bin; mcopy -o -i $T/f.img $T/relleno.bin ::RELLENO.BIN
s1=$(suma ::LLENO.TXT)
{ arranca
  escribe "X"; aparca
  tecla ctrl-g; sleep 4; shot l1; tecla ret; sleep 1
  sal; } | run 120 -vga std
chk $T/l1.ppm busca 150 230 650 340 0 8 "No se ha podido guardar el documento."
if [ "$(suma ::LLENO.TXT)" = "$s1" ]; then ok "disquete lleno: LLENO.TXT entero, como estaba"; else ko "disquete lleno: LLENO.TXT ha cambiado"; fi
noesta ::LLENO.\$\$\$ "disquete lleno"
muestra l1 guardar_lleno
fi

if parte largo; then
echo "== Pluma: un .TXT de más de 30.000 letras =="
prepara $B/PLUMA.HVP
python3 -c "
import sys
sys.stdout.write(''.join('Fila %05d con letras de relleno para pasar de las treinta mil.\r\n' % i for i in range(520)))" > $T/largo.txt
mcopy -o -i $T/f.img $T/largo.txt ::LARGO.TXT
reabre 'PLUMA.HVP A:\LARGO.TXT'
s2=$(suma ::LARGO.TXT)
{ arranca
  shot g1; tecla ret; sleep 1.5
  tecla ctrl-g; sleep 1.5; shot g2; tecla esc; sleep 1
  sal; } | run 120 -vga std
chk $T/g1.ppm busca 150 200 650 360 0 8 "se han abierto solo las primeras 30.000 letras." \
  busca 150 200 650 360 0 8 "al guardarlo se pedirá uno nuevo."
chk $T/g2.ppm busca 150 150 650 260 0 8 "Nombre del archivo:"
if [ "$(suma ::LARGO.TXT)" = "$s2" ]; then ok "LARGO.TXT entero (no se ha guardado recortado)"; else ko "LARGO.TXT ha cambiado"; fi
muestra g1 guardar_largo
fi

if parte csv; then
echo "== Cuentas: un .CSV de 1.100 filas =="
prepara $B/CUENTAS.HVP
python3 -c "
import sys
sys.stdout.write(''.join('%d;fila %d\r\n' % (i, i) for i in range(1, 1101)))" > $T/grande.csv
mcopy -o -i $T/f.img $T/grande.csv ::GRANDE.CSV
reabre 'CUENTAS.HVP A:\GRANDE.CSV'
s3=$(suma ::GRANDE.CSV)
{ arranca
  shot c1; tecla ret; sleep 1.5
  tecla ctrl-g; sleep 1.5; shot c2; tecla esc; sleep 1
  sal; } | run 120 -vga std
chk $T/c1.ppm busca 120 200 680 380 0 8 "El archivo es demasiado grande para Cuentas (como mucho," \
  busca 120 200 680 380 0 8 "al guardarla se pedirá uno nuevo."
chk $T/c2.ppm busca 150 150 650 260 0 8 "Nombre del archivo:"
if [ "$(suma ::GRANDE.CSV)" = "$s3" ]; then ok "GRANDE.CSV entero (no se ha guardado recortado)"; else ko "GRANDE.CSV ha cambiado"; fi
muestra c1 guardar_csv
fi

if parte facturas; then
echo "== Facturas: FACTURAS.DAT dañado =="
prepara $B/FACTURAS.HVP
# la cabecera dice 5 facturas, pero el archivo se acaba antes
python3 - $T/fac.dat <<'PY'
import sys,struct
# «HFAC», versión 1 y, en CAB_N (8), 5 facturas; pero solo 600 octetos
d=b'HFAC'+struct.pack('<II',1,5)+bytes(588)
open(sys.argv[1],'wb').write(d)
PY
mcopy -o -i $T/f.img $T/fac.dat ::HISPADOS/PROGS/FACTURAS.DAT
s4=$(suma ::HISPADOS/PROGS/FACTURAS.DAT)
{ arranca
  shot f1; tecla ret; sleep 1.2
  tecla insert; sleep 1.2; tecla tab tab; escribe "Ana Soler"; tecla ret; sleep 2.5; shot f2
  sal; } | run 150 -vga std
chk $T/f1.ppm busca 100 200 700 400 0 8 "El archivo de las facturas (FACTURAS.DAT) está dañado:"
chk $T/f2.ppm busca 100 200 700 400 0 8 "No se guarda: FACTURAS.DAT está dañado"
if [ "$(suma ::HISPADOS/PROGS/FACTURAS.DAT)" = "$s4" ]; then ok "FACTURAS.DAT dañado: sigue igual"; else ko "FACTURAS.DAT dañado: se ha escrito encima"; fi
muestra f1 guardar_facturas
fi

if parte centro; then
echo "== CENTRO.INI que falta: CENTRO.BAK =="
prepara $B/FACTURAS.HVP
mren -i $T/f.img ::HISPADOS/CENTRO.INI ::HISPADOS/CENTRO.BAK
{ arranca
  shot b1
  sal; } | run 90 -vga std
chk $T/b1.ppm titulo 50 36 activa "Facturas"
muestra b1 guardar_centro_bak
fi

[ -n "${GUARDAR:-}" ] && cp $T/*.ppm "$GUARDAR"/ 2>/dev/null
rm -rf $T
exit $fail
