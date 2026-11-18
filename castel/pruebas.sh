#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# pruebas.sh - las pruebas de Castel. Cuatro partes:
#   1. Los errores: cada ejemplos/errores/*.cas tiene que dar, al compilar, el
#      mensaje (con su línea y su columna) que lleva escrito en «; ERROR: ».
#   2. Los ejemplos: se compilan todos, se ejecutan dentro de Hispa-8086 (en
#      QEMU) con la salida mandada a un archivo, y cada una se compara con
#      ejemplos/esperado/NOMBRE.txt. Entre ellos, los de la Fase 4: los
#      números de 32 bits (largos), la biblioteca escrita en Castel (biblio,
#      ficheros) y los programas en varios archivos (modulos, con los módulos
#      de ejemplos/partes).
#   3. MAS: la orden MAS escrita en Castel (ejemplos/mas.cas) frente a la de
#      ensamblador (h8086/ordenes/mas.asm), con las mismas entradas y las
#      mismas teclas: archivo, «<», tubería, archivo que no está, pausas,
#      Esc. Tienen que escribir lo mismo, octeto a octeto, en la salida y en
#      la pantalla.
#   4. CONTAR y NUMERA (ejemplos/contar.cas y numera.cas): los programas de
#      verdad de la Fase 4, con archivos, «<», tubería, uno que no está y uno
#      de más de 65535 octetos; lo que escriben se compara con lo que sale de
#      hacer la misma cuenta en Python.
#   5. El ensamblador propio: ensambla.py (la referencia, en Python) y ENSAMBLA
#      (ensamblador/ensambla.cas, escrito en Castel, corriendo en Hispa-8086)
#      ensamblan todos los ejemplos, la biblioteca entera, las formas de
#      ensamblador/formas.asm y el propio ENSAMBLA, y cada archivo tiene que
#      salir igual, octeto a octeto, que el de NASM.
#   6. El compilador escrito en Castel (compilador/castel.cas -> CASTEL.EXE),
#      dentro de Hispa-8086: saca el mismo ensamblador que ccastel.py para
#      todos los ejemplos, la biblioteca entera y ENSAMBLA, y dice los mismos
#      errores. Y EL ARRANQUE EN TRES PASOS: (1) ccastel.py compila el
#      compilador; (2) ese CASTEL.EXE, dentro de Hispa-8086, se compila a sí
#      mismo y ENSAMBLA lo ensambla; (3) el resultado se compila otra vez.
#      Los pasos 2 y 3 tienen que dar el mismo ensamblador y el mismo .EXE
#      (que son, además, los del paso 1 con NASM).
#   8. Castel de 32 bits (CASTEL32.md): los ejemplos de 16 bits que no tocan
#      el DOS, compilados como núcleo de 32 bits, escriben lo mismo; los de
#      ejemplos32 (núcleos en QEMU y programas Hispa32 con HW32, si está el
#      disquete de Hispa-DOS); cuentas al azar con las reglas de 32 bits; y la
#      semilla de Hispa-Pentium (nucleo32): IDT, reloj, teclado, excepciones.
# Necesita python3, nasm, qemu-system-i386, socat y mtools. El disquete de
# Hispa-8086 se monta solo (make en h8086) si no está hecho.
#   SOLO=errores ./pruebas.sh     solo la primera parte (no arranca QEMU)
#   SIN_ARRANQUE=1 ./pruebas.sh   sin el arranque en tres pasos
#   AZAR=3 ./pruebas.sh           además, tres programas de cuentas al azar
#                                 de 16 bits (azar.py) y otros tres de 32
#                                 (azar32.py): lo que calculan frente a lo
#                                 que tiene que dar. Unos 10 s más cada uno.
#   VER=1 ./pruebas.sh            deja lo que sale en /tmp/castel-pruebas
set -u
AQUI=$(dirname "$(readlink -f "$0")")
H=$AQUI/../h8086
T=$(mktemp -d)
QPID=
acaba() { [ -n "$QPID" ] && kill $QPID 2>/dev/null; [ -n "${VER:-}" ] && { rm -rf /tmp/castel-pruebas; cp -r $T /tmp/castel-pruebas; }; rm -rf $T; }
trap acaba EXIT
fail=0
nok=0
ok() { echo "  OK   $1"; nok=$((nok + 1)); }
ko() { echo "  FAIL $1"; fail=$((fail + 1)); }

# ---------------------------------------------------------------- 1. errores
echo "== Castel: los errores se dicen en español, con línea y columna =="
for f in $AQUI/ejemplos/errores/*.cas $AQUI/ejemplos32/errores/*.cas; do
  n=$(basename $f .cas)
  esperado=$(sed -n 's/^; ERROR: //p' $f | head -1)
  if python3 -I $AQUI/ccastel.py $f -o $T/err.asm -I $AQUI/ejemplos/partes > $T/err.txt 2>&1; then
    ko "$n: compila, y tenía que dar un error"
    continue
  fi
  dicho=$(head -1 $T/err.txt | sed 's/^.*\.cas://')
  if [ "$dicho" = "$esperado" ]; then ok "$n: $dicho"; else ko "$n: dice «$dicho» y se esperaba «$esperado»"; fi
done
if [ "${SOLO:-}" = errores ]; then
  echo "== $nok bien, $fail mal =="
  exit $((fail > 0))
fi

# -------------------------------------------------------- 2. compilar todo
echo "== Castel: los ejemplos compilan =="
EJEMPLOS=
for f in $AQUI/ejemplos/*.cas; do
  n=$(basename $f .cas)
  N=$(echo $n | tr a-z A-Z)
  [ $n = mas ] && N=MASC               # MAS.COM es la de ensamblador
  X=COM
  grep -q '^ *modelo grande' $f && X=EXE   # el «modelo grande» da un .EXE
  if $AQUI/compilar.sh $f $T/$N.$X -I $AQUI/ejemplos/partes > $T/comp.txt 2>&1; then
    ok "$n.cas -> $N.$X ($(stat -c %s $T/$N.$X) octetos)"
    # MAS, CONTAR y NUMERA llevan argumentos y sus pruebas aparte
    [ $n = mas ] || [ $n = contar ] || [ $n = numera ] || EJEMPLOS="$EJEMPLOS $n"
  else
    ko "$n.cas no compila: $(head -1 $T/comp.txt)"
  fi
done

if [ -f $T/modulos.asm ] && ! grep -q "F_Figuras__sin_usar" $T/modulos.asm && grep -q "F_Figuras__cuadrado" $T/modulos.asm; then
  ok "de un módulo solo va al programa lo que se usa"
else ko "de un módulo solo va al programa lo que se usa"; fi

# el disquete: el de Hispa-8086 con los programas, los textos y un AUTOEXEC
# que lo ejecuta todo con la salida en archivos .SAL
[ -f $H/build/hispa8086-1440.img ] || make -C $H > /dev/null 2>&1
[ -f $H/build/hispa8086-1440.img ] || { echo "No hay disquete de Hispa-8086 (make en h8086)"; exit 2; }
cp $H/build/hispa8086-1440.img $T/a.img
python3 - $T $EJEMPLOS <<'PY'
import sys
t = sys.argv[1]
# CORTO.TXT: cabe en una pantalla. Tabuladores, acentos (página 437), una
# línea sin retorno de carro y otra sin fin de línea al final
corto = ("Primera línea\r\n\tcon un tabulador\r\nuno\tdos\ttres\tcuatro\r\n"
         "1234567\ta\r\n12345678\tb\r\nsolo salto de línea\nñandú, pingüino, ¿qué?\r\n"
         "sin fin de línea").encode('cp437')
open(t + '/CORTO.TXT', 'wb').write(corto)
# LARGO.TXT: tres pausas con 24 líneas de pantalla. Líneas de justo 80 letras
# y de más, tabuladores que llegan al borde, más de un trozo de 512 octetos
# y, al final, el fin de archivo de los textos (1Ah) con algo detrás
ln = []
for i in range(1, 76):
    if i == 7:
        ln.append("x" * 80)
    elif i == 13:
        ln.append("".join(str(j % 10) for j in range(100)))
    elif i == 19:
        ln.append("a\tb\t\tc" + "\t" * 9 + "tras el borde")
    elif i == 31:
        ln.append("y" * 79)
    elif i == 40:
        ln.append("z" * 73 + "\ttab al borde")
    elif i == 45:
        ln.append("")
    else:
        ln.append(f"Línea {i:02d} de la prueba de MAS:\t{'=' * (i % 30)}")
largo = ("\r\n".join(ln) + "\r\nfin del texto\r\n").encode('cp437') + b"\x1aNO DEBE SALIR\r\n"
open(t + '/LARGO.TXT', 'wb').write(largo)
# GRANDE.TXT: más de 65535 octetos, para las cuentas de 32 bits de CONTAR
grande = "".join(f"renglón {i} del archivo grande, que pasa de 65535 octetos\r\n"
                 for i in range(1, 1501)).encode('cp437')
open(t + '/GRANDE.TXT', 'wb').write(grande)
# lo que tiene que decir CONTAR de cada uno (y el total): líneas, palabras y
# octetos, en columnas de 8
def cuenta(d):
    return (d.count(b'\n'), len(d.split()), len(d))
def renglon(c, nombre):
    return "%8d%8d%8d" % c + (" " + nombre if nombre else "") + "\n"
cl, cc, cg = cuenta(largo), cuenta(corto), cuenta(grande)
assert cg[2] > 65535
open(t + '/contar1.esp', 'w').write(
    renglon(cl, 'LARGO.TXT') + renglon(cc, 'CORTO.TXT') + "CONTAR: no puedo abrir NOHAY.TXT\n"
    + renglon(cg, 'GRANDE.TXT') + renglon(tuple(a + b + c for a, b, c in zip(cl, cc, cg)), 'total'))
open(t + '/contar2.esp', 'w').write(renglon(cc, 'CORTO.TXT'))
open(t + '/contar3.esp', 'w').write(renglon(cc, ''))
# (por la tubería llega lo que escribe TYPE, que acaba siempre con un fin de línea suyo)
open(t + '/contar4.esp', 'w').write(renglon(cuenta(grande + b'\r\n'), ''))
# y lo que tiene que escribir NUMERA: cada línea con su número delante, hasta
# el fin de archivo de los textos (1Ah)
def numera(d):
    d = d.split(b'\x1a')[0]
    sal = b''
    for i, ln in enumerate(d.splitlines(keepends=True)):
        sal += b"%5d: " % (i + 1) + ln
    return sal
assert b'\r' not in corto.replace(b'\r\n', b'') and b'\r' not in largo.replace(b'\r\n', b'')
open(t + '/numera1.esp', 'wb').write(numera(largo))
open(t + '/numera2.esp', 'wb').write(numera(corto))
# cuántas pausas tiene que haber (la misma cuenta que hace MAS)
col = lin = pausas = 0
for c in largo:
    if c == 0x1A:
        break
    nueva = False
    if c == 9:
        while True:
            col += 1
            if col >= 80:
                col = 0
                nueva = True
                break
            if col % 8 == 0:
                break
    elif c == 10:
        nueva = True
    elif c == 13:
        col = 0
    elif c >= 32:
        col += 1
        if col >= 80:
            col = 0
            nueva = True
    if nueva:
        lin += 1
        if lin >= 24:
            lin = 0
            pausas += 1
assert pausas == 3, pausas
lote = ["@ECO OFF"]
for n in sys.argv[2:]:
    N = n.upper()
    lote.append(f"{N} > {N}.SAL")
    if N == "NIVEL":
        lote += ["IF ERRORLEVEL 7 ECO siete > NIV7.SAL", "IF ERRORLEVEL 8 ECO ocho > NIV8.SAL"]
lote += [
    "CONTAR LARGO.TXT CORTO.TXT NOHAY.TXT GRANDE.TXT > CONTAR1.SAL",
    "IF ERRORLEVEL 1 ECO nivel 1 > CONTAR1N.SAL",
    "CONTAR CORTO.TXT > CONTAR2.SAL",
    "IF ERRORLEVEL 1 ECO nivel 1 > CONTAR2N.SAL",
    "CONTAR < CORTO.TXT > CONTAR3.SAL",
    "TYPE GRANDE.TXT | CONTAR > CONTAR4.SAL",
    "NUMERA LARGO.TXT > NUMERA1.SAL",
    "NUMERA < CORTO.TXT > NUMERA2.SAL",
    "NUMERA NOHAY.TXT > NUMERA3.SAL",
    "IF ERRORLEVEL 1 ECO nivel 1 > NUMERA3N.SAL",
]
for prog, l in (("MAS", "M"), ("MASC", "C")):
    lote += [
        "LIMPIAR",
        f"{prog} CORTO.TXT > {l}1.SAL",
        f"{prog} < CORTO.TXT > {l}2.SAL",
        f"TYPE CORTO.TXT | {prog} > {l}3.SAL",
        f"{prog} NOHAY.TXT > {l}4.SAL",
        f"IF ERRORLEVEL 1 ECO nivel 1 > {l}4N.SAL",
        f"{prog}    CORTO.TXT   > {l}5.SAL",
        f"{prog} LARGO.TXT > {l}6.SAL",           # pausas 1 a 3
        "LIMPIAR",
        f"{prog} LARGO.TXT",                      # pausas 4 a 6, en la pantalla
        "LIMPIAR",
        f"TYPE LARGO.TXT | {prog}",               # pausa 7 y pausa 8: Esc
        f"IF ERRORLEVEL 1 ECO nivel 1 > {l}7N.SAL",
        f"{prog} CORTO.TXT",
        f"{prog} < LARGO.TXT > {l}8.SAL",         # pausas 9 a 11
        f"{prog} NOHAY.TXT",
        f"{prog} LARGO.TXT > {l}9.SAL",           # pausas 12 a 14
        "LIMPIAR",
        f"{prog} LARGO.TXT",                      # pausa 15: Esc
        f"IF ERRORLEVEL 1 ECO nivel 1 > {l}10N.SAL",
    ]
lote += ["LIMPIAR", "ECO [castel-fin]"]
open(t + '/AUTOEXEC.BAT', 'wb').write(("\r\n".join(lote) + "\r\n").encode('cp437'))
PY
[ $? = 0 ] || { echo "No se han podido preparar los textos de prueba"; exit 2; }
mcopy -o -i $T/a.img $T/*.COM $T/*.EXE $T/CORTO.TXT $T/LARGO.TXT $T/GRANDE.TXT $T/AUTOEXEC.BAT ::
POR_PROG=15         # pausas de cada una de las dos MAS
ESC="8 15"          # las que se contestan con Esc

# ------------------------------------------------------------ QEMU
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; sleep "${2:-0.25}"; }
# la pantalla de texto (80x25, un octeto de letra y otro de color), a texto
pantalla() {
  mon "pmemsave 0xb8000 4000 \"$T/scr.bin\"" 0.3
  python3 - $T/scr.bin <<'PY' > $T/$1.txt
import sys
d = open(sys.argv[1], 'rb').read()
for f in range(25):
    print(bytes(d[f * 160:(f + 1) * 160:2]).decode('cp437').rstrip())
PY
}
echo "== Castel: los ejemplos, dentro de Hispa-8086 (QEMU) =="
qemu-system-i386 -display none -monitor unix:$T/mon,server,nowait -cpu 486 -m 4 \
  -drive file=$T/a.img,format=raw,if=floppy,index=0 -boot a -no-reboot > /dev/null 2>&1 &
QPID=$!
sleep 4
np=0
hecho=0
for ((i = 0; i < 400; i++)); do
  pantalla p
  if grep -qF '[castel-fin]' $T/p.txt; then hecho=1; break; fi
  if grep -qF -- '-- Más --' $T/p.txt; then
    np=$((np + 1))
    cp $T/p.txt $T/pausa_$np.txt
    tecla=spc
    for e in $ESC; do
      if [ $np = $e ] || [ $np = $((e + POR_PROG)) ]; then tecla=esc; fi
    done
    mon "sendkey $tecla" 0.6
  else
    sleep 0.4
  fi
done
mon quit 0.5
wait $QPID 2>/dev/null
QPID=
if [ $hecho = 0 ]; then
  ko "el lote no ha llegado al final (pantalla en $T/p.txt):"
  sed 's/^/        /' $T/p.txt | grep -v '^ *$' | tail -8
fi
mkdir $T/sal
mcopy -n -i $T/a.img '::*.SAL' $T/sal/ > /dev/null 2>&1

for n in $EJEMPLOS; do
  N=$(echo $n | tr a-z A-Z)
  if [ ! -f $T/sal/$N.SAL ]; then ko "$n: no ha dejado salida"; continue; fi
  tr -d '\r' < $T/sal/$N.SAL | iconv -f CP437 -t UTF-8 > $T/sal/$n.txt
  if cmp -s $T/sal/$n.txt $AQUI/ejemplos/esperado/$n.txt; then
    ok "$n: la salida es la esperada"
  else
    ko "$n: la salida no es la esperada (< esperada, > la que sale):"
    diff $AQUI/ejemplos/esperado/$n.txt $T/sal/$n.txt | sed 's/^/        /' | head -12
  fi
done

if [ -s $T/sal/NIV7.SAL ] && [ ! -s $T/sal/NIV8.SAL ]; then ok "nivel: lo que devuelve principal() es el nivel de error (7)"
else ko "nivel: el programa no ha acabado con el nivel de error 7"; fi

# ---------------------------------------------------------------- 3. MAS
echo "== MAS en Castel frente a MAS en ensamblador =="
echo "  (MAS.COM en ensamblador: $(stat -c %s $H/build/MAS.COM) octetos; en Castel: $(stat -c %s $T/MASC.COM))"
compara() {    # número de la salida y lo que se prueba
  if [ ! -s $T/sal/M$1.SAL ]; then ko "$2: la de ensamblador no ha escrito nada"
  elif cmp -s $T/sal/M$1.SAL $T/sal/C$1.SAL; then ok "$2: la misma salida ($(stat -c %s $T/sal/C$1.SAL) octetos)"
  else ko "$2: la salida es distinta"; cmp $T/sal/M$1.SAL $T/sal/C$1.SAL 2>&1 | sed 's/^/        /'; fi
}
compara 1 "MAS archivo"
compara 2 "MAS < archivo"
compara 3 "orden | MAS"
compara 4 "MAS con un archivo que no está («No existe ese archivo.»)"
compara 4N "y acaba con el nivel de error 1"
compara 5 "MAS con espacios antes y después del nombre"
compara 6 "MAS archivo largo > salida (tres pausas)"
compara 8 "MAS < archivo largo > salida"
compara 9 "MAS archivo largo > salida, tras el aviso de que no está"
if [ -s $T/sal/M10N.SAL ] || [ -s $T/sal/C10N.SAL ]; then ko "tras Esc, el nivel de error es 0"; else ok "tras Esc, las dos acaban con el nivel de error 0"; fi
if [ -s $T/sal/M7N.SAL ] && [ -s $T/sal/C7N.SAL ]; then ok "tras Esc en una tubería, el mismo nivel de error (1 o más)"
elif [ -s $T/sal/M7N.SAL ] || [ -s $T/sal/C7N.SAL ]; then ko "tras Esc en una tubería, el nivel de error es distinto"
else ok "tras Esc en una tubería, el mismo nivel de error (0)"; fi
if tr -d '\r' < $T/sal/C6.SAL | iconv -f CP437 -t UTF-8 | grep -q "NO DEBE SALIR"; then ko "lo que hay tras el 1Ah no se escribe"
else ok "lo que hay tras el fin de archivo (1Ah) no se escribe"; fi
if tr -d '\r' < $T/sal/C6.SAL | iconv -f CP437 -t UTF-8 | grep -q -- "-- Más --"; then ko "el aviso no va a la salida"
else ok "el aviso «-- Más --» no acaba en el archivo de salida"; fi
if [ $np = $((2 * POR_PROG)) ]; then ok "las mismas pausas: $POR_PROG cada una"; else ko "pausas: $np, y tenían que ser $((2 * POR_PROG))"; fi
iguales=1
for ((i = 1; i <= POR_PROG; i++)); do
  if ! cmp -s $T/pausa_$i.txt $T/pausa_$((i + POR_PROG)).txt; then
    iguales=0
    ko "la pantalla en la pausa $i es distinta:"
    diff $T/pausa_$i.txt $T/pausa_$((i + POR_PROG)).txt 2>&1 | sed 's/^/        /' | head -8
  fi
done
[ $iguales = 1 ] && ok "la pantalla es la misma en las $POR_PROG pausas (también tras Esc)"
if [ -f $T/pausa_4.txt ] && grep -q "Línea 1[0-9] de la prueba" $T/pausa_4.txt && grep -qF -- "-- Más --" $T/pausa_4.txt; then
  ok "en la pantalla: el texto y, debajo, «-- Más --»"
else ko "en la pantalla no está el texto con el aviso"; fi

# ---------------------------------------------------------------- 4. CONTAR
echo "== CONTAR y NUMERA: programas con módulos, biblioteca y 32 bits =="
contar() {     # número de la salida, la esperada y lo que se prueba
  if [ ! -f $T/sal/CONTAR$1.SAL ]; then ko "$3: no ha dejado salida"; return; fi
  tr -d '\r' < $T/sal/CONTAR$1.SAL | iconv -f CP437 -t UTF-8 > $T/sal/contar$1.txt
  if cmp -s $T/sal/contar$1.txt $T/contar$2.esp; then ok "$3"
  else ko "$3 (< esperado, > lo que sale):"; diff $T/contar$2.esp $T/sal/contar$1.txt | sed 's/^/        /' | head -8; fi
}
contar 1 1 "CONTAR de cuatro archivos (uno no está; otro pasa de 65535 octetos) y el total"
if [ -s $T/sal/CONTAR1N.SAL ]; then ok "y acaba con el nivel de error 1"; else ko "CONTAR no ha acabado con el nivel de error 1"; fi
contar 2 2 "CONTAR de un archivo"
if [ -s $T/sal/CONTAR2N.SAL ]; then ko "CONTAR de un archivo que está acaba con nivel de error"; else ok "y acaba con el nivel de error 0"; fi
contar 3 3 "CONTAR < archivo"
contar 4 4 "orden | CONTAR (con el archivo de más de 65535 octetos)"
numera() {     # número de la salida y lo que se prueba
  if [ ! -f $T/sal/NUMERA$1.SAL ]; then ko "$2: no ha dejado salida"
  elif cmp -s $T/sal/NUMERA$1.SAL $T/numera$1.esp; then ok "$2 ($(stat -c %s $T/sal/NUMERA$1.SAL) octetos, los mismos)"
  else ko "$2: no sale lo esperado"; cmp $T/numera$1.esp $T/sal/NUMERA$1.SAL 2>&1 | sed 's/^/        /'; fi
}
numera 1 "NUMERA de un archivo de 76 líneas (para en el 1Ah)"
numera 2 "NUMERA < archivo"
if [ -s $T/sal/NUMERA3N.SAL ] && grep -q "no puedo abrir NOHAY.TXT" $T/sal/NUMERA3.SAL; then ok "NUMERA de uno que no está: lo dice y acaba con el nivel de error 1"
else ko "NUMERA de uno que no está: no lo dice o no acaba con el nivel de error 1"; fi

# ---------------------------------------------------- 5. el ensamblador propio
echo "== El ensamblador propio: los mismos octetos que NASM =="
mkdir $T/ens $T/ens/nasm
cp $T/*.asm $T/ens/
cp $AQUI/ensamblador/formas.asm $T/ens/formas.asm
# y los de 32 bits: las formas de 32, los ejemplos de ejemplos32, la semilla
# de Hispa-Pentium y unas cuentas al azar (CASTEL32.md)
cp $AQUI/ensamblador/formas32.asm $T/ens/formas32.asm
python3 -I $AQUI/azar32.py 9 150 $T/ens/az32p.cas $T/ens/az32p.esp plano
for f in $AQUI/ejemplos32/*.cas $AQUI/nucleo32/semilla.cas $T/ens/az32p.cas; do
  python3 -I $AQUI/ccastel.py $f -o $T/ens/$(basename $f .cas).asm > /dev/null 2>&1 || ko "$(basename $f) no compila"
done
rm -f $T/ens/az32p.cas $T/ens/az32p.esp
# la biblioteca entera (también lo que nadie usa) y el propio ensamblador
printf 'm\xc3\xb3dulo Toda\n    importar Textos, Archivos, Memoria, Argumentos, Lejos\n    funci\xc3\xb3n principal()\n    fin\nfin Toda.\n' > $T/ens/toda.cas
python3 -I $AQUI/ccastel.py --todo $T/ens/toda.cas -o $T/ens/toda.asm > /dev/null 2>&1 || ko "la biblioteca entera no compila"
python3 -I $AQUI/ccastel.py $AQUI/ensamblador/ensambla.cas -o $T/ens/ensambla.asm > /dev/null 2>&1 || ko "ensambla.cas no compila"
iguales=0
distintos=
for f in $T/ens/*.asm; do
  n=$(basename $f .asm)
  nasm -f bin $f -o $T/ens/nasm/$n.bin 2> /dev/null
  python3 -I $AQUI/ensambla.py $f -o $T/ens/$n.py.bin 2> /dev/null
  if cmp -s $T/ens/nasm/$n.bin $T/ens/$n.py.bin; then iguales=$((iguales + 1)); else distintos="$distintos $n"; fi
done
if [ -z "$distintos" ]; then ok "ensambla.py: los $iguales archivos (ejemplos de 16 y de 32 bits, biblioteca entera, formas y ENSAMBLA), iguales que con NASM"
else ko "ensambla.py no da lo mismo que NASM en:$distintos"; fi
# ENSAMBLA, dentro de Hispa-8086: los ensambla todos (también a sí mismo)
cp $H/build/hispa8086-1440.img $T/e.img
{ printf '@ECO OFF\r\n'
  for f in $T/ens/*.asm; do
    N=$(basename $f .asm | tr a-z A-Z)
    mcopy -o -i $T/e.img $f ::$N.ASM
    printf 'ENSAMBLA %s.ASM %s.BIN > %s.SAL\r\n' $N $N $N
  done
  printf 'ECO [castel-fin]\r\n'; } > $T/AUTOEXEC.BAT
mcopy -o -i $T/e.img $T/ens/nasm/ensambla.bin ::ENSAMBLA.COM
mcopy -o -i $T/e.img $T/AUTOEXEC.BAT ::
qemu-system-i386 -display none -monitor unix:$T/mon,server,nowait -cpu 486 -m 4 \
  -drive file=$T/e.img,format=raw,if=floppy,index=0 -boot a -no-reboot > /dev/null 2>&1 &
QPID=$!
sleep 4
hecho=0
for ((i = 0; i < 300; i++)); do
  pantalla p
  if grep -qF '[castel-fin]' $T/p.txt; then hecho=1; break; fi
  sleep 1
done
mon quit 0.5
wait $QPID 2>/dev/null
QPID=
[ $hecho = 1 ] || ko "ENSAMBLA: el lote no ha llegado al final"
mkdir $T/ens/sal
mcopy -n -i $T/e.img '::*.BIN' $T/ens/sal/ > /dev/null 2>&1
iguales=0
distintos=
for f in $T/ens/*.asm; do
  n=$(basename $f .asm)
  N=$(echo $n | tr a-z A-Z)
  if cmp -s $T/ens/nasm/$n.bin $T/ens/sal/$N.BIN; then iguales=$((iguales + 1)); else distintos="$distintos $n"; fi
done
if [ -z "$distintos" ]; then ok "ENSAMBLA (en Castel, dentro de Hispa-8086): los $iguales archivos, iguales que con NASM (él mismo incluido)"
else ko "ENSAMBLA no da lo mismo que NASM en:$distintos"; fi

# ------------------------------------------------ 6. el compilador en Castel
echo "== El compilador escrito en Castel, dentro de Hispa-8086 =="
A=$T/aut
mkdir $A $A/ref $A/sal
# paso 1: ccastel.py compila el compilador, y NASM lo ensambla
if python3 -I $AQUI/ccastel.py $AQUI/compilador/castel.cas -o $A/c1.asm > $T/comp.txt 2>&1 && nasm -f bin $A/c1.asm -o $A/CASTEL.EXE 2>> $T/comp.txt; then
  ok "paso 1: ccastel.py compila el compilador (castel.cas -> CASTEL.EXE, $(stat -c %s $A/CASTEL.EXE) octetos)"
else ko "el compilador en Castel no compila: $(head -1 $T/comp.txt)"; fi
# dos disquetes: en A: la biblioteca en \BIB, los módulos de ejemplo en \PARTES,
# los ejemplos, la biblioteca entera (--todo), ENSAMBLA, los errores y el
# temporal de CASTEL; en B:, lo que sale (el ensamblador de cada uno son, todos
# juntos, más de un disquete si va con los fuentes: el de ENSAMBLA solo pasa de
# 250 K)
cp $H/build/hispa8086-1440.img $T/c.img
mmd -i $T/c.img ::BIB ::PARTES
for f in $AQUI/biblioteca/*.cas; do mcopy -i $T/c.img $f ::BIB/$(basename $f .cas | tr a-z A-Z | cut -c1-8).CAS; done
for f in $AQUI/ejemplos/partes/*.cas; do mcopy -i $T/c.img $f ::PARTES/$(basename $f .cas | tr a-z A-Z | cut -c1-8).CAS; done
mcopy -i $T/c.img $A/CASTEL.EXE ::
mformat -C -f 1440 -i $T/cb.img ::
{ printf '@ECO OFF\r\n'
  python3 -I $AQUI/azar.py 7 45 $A/azar16.cas $A/azar16.esp
  python3 -I $AQUI/azar32.py 7 28 $A/azar32.cas $A/azar32.esp
  python3 -I $AQUI/azar32.py 8 40 $A/azar32p.cas $A/azar32p.esp plano
  for f in $AQUI/ejemplos/*.cas $AQUI/ensamblador/ensambla.cas $T/ens/toda.cas $A/azar16.cas $A/azar32.cas \
           $AQUI/ejemplos32/*.cas $AQUI/nucleo32/semilla.cas $A/azar32p.cas; do
    n=$(basename $f .cas); N=$(echo $n | tr a-z A-Z)
    todo=; [ $n = toda ] && todo=--todo
    mcopy -i $T/c.img $f ::$N.CAS
    python3 -I $AQUI/ccastel.py $todo $f -o $A/ref/$N.ASM -I $AQUI/ejemplos/partes > /dev/null 2>&1
    printf 'CASTEL %s.CAS -o B:%s.ASM -I BIB -I PARTES %s > B:%s.SAL\r\n' $N $N "$todo" $N
  done
  i=0
  for f in $AQUI/ejemplos/errores/*.cas $AQUI/ejemplos32/errores/*.cas; do
    i=$((i + 1)); N=$(printf 'E%02d' $i)
    mcopy -i $T/c.img $f ::$N.CAS
    echo "$N $(basename $f .cas)" >> $A/errores.txt
    printf 'CASTEL %s.CAS -I BIB -I PARTES > B:%s.SAL\r\n' $N $N
  done
  printf 'ECO [castel-fin]\r\n'; } > $T/AUTOEXEC.BAT
mcopy -o -i $T/c.img $T/AUTOEXEC.BAT ::
qemu-system-i386 -display none -monitor unix:$T/mon,server,nowait -cpu 486 -m 4 \
  -drive file=$T/c.img,format=raw,if=floppy,index=0 -drive file=$T/cb.img,format=raw,if=floppy,index=1 \
  -boot a -no-reboot > /dev/null 2>&1 &
QPID=$!
sleep 4
hecho=0
for ((i = 0; i < 300; i++)); do
  pantalla p
  if grep -qF '[castel-fin]' $T/p.txt; then hecho=1; break; fi
  sleep 1
done
mon quit 0.5
wait $QPID 2>/dev/null
QPID=
[ $hecho = 1 ] || ko "CASTEL: el lote de los ejemplos no ha llegado al final"
mcopy -n -i $T/cb.img '::*.ASM' '::*.SAL' $A/sal/ > /dev/null 2>&1
iguales=0
distintos=
for f in $A/ref/*.ASM; do
  N=$(basename $f .ASM)
  if cmp -s $f $A/sal/$N.ASM; then iguales=$((iguales + 1)); else distintos="$distintos $N"; fi
done
if [ -z "$distintos" ]; then ok "CASTEL: el mismo ensamblador que ccastel.py en los $iguales programas (ejemplos de 16 y de 32 bits, biblioteca entera, ENSAMBLA, la semilla y tres de cuentas al azar)"
else ko "CASTEL no saca el mismo ensamblador que ccastel.py en:$distintos"; fi
iguales=0
distintos=
while read N n; do
  esperado=$(sed -n 's/^; ERROR: //p' $AQUI/ejemplos/errores/$n.cas $AQUI/ejemplos32/errores/$n.cas 2> /dev/null | head -1)
  dicho=$(tr -d '\r' < $A/sal/$N.SAL 2> /dev/null | iconv -f CP437 -t UTF-8 | head -1 | sed 's/^.*\.CAS://')
  if [ "$dicho" = "$esperado" ]; then iguales=$((iguales + 1)); else distintos="$distintos $n"; fi
done < $A/errores.txt
if [ -z "$distintos" ]; then ok "CASTEL: los $iguales errores, dichos igual que ccastel.py (con su línea y su columna)"
else ko "CASTEL no dice igual que ccastel.py los errores de:$distintos"; fi

# El arranque en tres pasos. Cada paso, en un Hispa-8086 con dos disquetes: en
# A: el sistema, el compilador que toca, ENSAMBLA y los fuentes (y ahí deja
# CASTEL su archivo temporal); en B:, lo que sale (el ensamblador del
# compilador son 700 K: no cabe todo en uno). Compilar y ensamblar van en dos
# arranques seguidos con el mismo B: (en Hispa-8086, después de que CASTEL
# compile, leer archivos grandes en ese mismo arranque va decenas de veces
# más despacio: el ensamblado tardaría cuatro minutos en vez de tres segundos).
arranca_ab() {     # las órdenes del AUTOEXEC (con A: en $T/d.img y B: en $T/b.img)
  printf '@ECO OFF\r\n%s\r\nECO [castel-fin]\r\n' "$1" > $T/AUTOEXEC.BAT
  mcopy -o -i $T/d.img $T/AUTOEXEC.BAT ::
  qemu-system-i386 -display none -monitor unix:$T/mon,server,nowait -cpu 486 -m 4 \
    -drive file=$T/d.img,format=raw,if=floppy,index=0 -drive file=$T/b.img,format=raw,if=floppy,index=1 \
    -boot a -no-reboot > /dev/null 2>&1 &
  QPID=$!
  sleep 4
  hecho=0
  for ((i = 0; i < 600; i++)); do
    pantalla p
    if grep -qF '[castel-fin]' $T/p.txt; then hecho=1; break; fi
    sleep 1
  done
  mon quit 0.5
  wait $QPID 2>/dev/null
  QPID=
  [ $hecho = 1 ]
}
paso() {       # número del paso, el compilador (un .EXE) y cómo se llama en el disquete
  cp $H/build/hispa8086-1440.img $T/d.img
  mmd -i $T/d.img ::BIB
  mcopy -i $T/d.img $2 ::$3.EXE
  mcopy -i $T/d.img $T/ens/nasm/ensambla.bin ::ENSAMBLA.COM
  for f in $AQUI/compilador/*.cas; do mcopy -i $T/d.img $f ::$(basename $f .cas | tr a-z A-Z | cut -c1-8).CAS; done
  for f in $AQUI/biblioteca/*.cas; do mcopy -i $T/d.img $f ::BIB/$(basename $f .cas | tr a-z A-Z | cut -c1-8).CAS; done
  rm -f $T/b.img; mformat -C -f 1440 -i $T/b.img ::
  arranca_ab "$3 CASTEL.CAS -o B:C$1.ASM -I BIB > B:P$1.SAL" || ko "el paso $1 del arranque (compilar) no ha llegado al final"
  arranca_ab "ENSAMBLA B:C$1.ASM B:C$1.EXE > B:E$1.SAL" || ko "el paso $1 del arranque (ensamblar) no ha llegado al final"
  mcopy -n -i $T/b.img "::C$1.*" '::*.SAL' $A/arr/ > /dev/null 2>&1
}
if [ -z "${SIN_ARRANQUE:-}" ]; then
  echo "== El arranque en tres pasos: el compilador se compila a sí mismo =="
  mkdir $A/arr
  paso 2 $A/CASTEL.EXE CASTEL
  if [ -s $A/arr/C2.EXE ]; then paso 3 $A/arr/C2.EXE C2; else ko "el paso 2 no ha dejado C2.EXE: $(tr -d '\r' < $A/arr/P2.SAL 2> /dev/null | iconv -f CP437 -t UTF-8 | head -1)"; fi
  if [ -s $A/arr/C2.ASM ] && cmp -s $A/c1.asm $A/arr/C2.ASM; then ok "paso 2: CASTEL se compila a sí mismo dentro de Hispa-8086: el mismo ensamblador que el paso 1 ($(wc -l < $A/arr/C2.ASM) líneas)"
  else ko "paso 2: el ensamblador no es el del paso 1: $(tr -d '\r' < $A/arr/P2.SAL 2> /dev/null | iconv -f CP437 -t UTF-8 | head -1)"; fi
  if [ -s $A/arr/C2.EXE ] && cmp -s $A/CASTEL.EXE $A/arr/C2.EXE; then ok "paso 2: ENSAMBLA lo ensambla: el mismo CASTEL.EXE que el paso 1 con NASM ($(stat -c %s $A/arr/C2.EXE) octetos)"
  else ko "paso 2: el .EXE no es el del paso 1: $(tr -d '\r' < $A/arr/E2.SAL 2> /dev/null | iconv -f CP437 -t UTF-8 | head -1)"; fi
  if [ -s $A/arr/C3.ASM ] && cmp -s $A/arr/C2.ASM $A/arr/C3.ASM; then ok "paso 3: el resultado se compila otra vez: el mismo ensamblador que el paso 2"
  else ko "paso 3: el ensamblador no es el del paso 2: $(tr -d '\r' < $A/arr/P3.SAL 2> /dev/null | iconv -f CP437 -t UTF-8 | head -1)"; fi
  if [ -s $A/arr/C3.EXE ] && cmp -s $A/arr/C2.EXE $A/arr/C3.EXE; then ok "paso 3: y el mismo .EXE: el compilador se sostiene solo"
  else ko "paso 3: el .EXE no es el del paso 2: $(tr -d '\r' < $A/arr/E3.SAL 2> /dev/null | iconv -f CP437 -t UTF-8 | head -1)"; fi
fi

# ------------------------------------------------- 7. cuentas al azar (AZAR=n)
if [ -n "${AZAR:-}" ]; then
  echo "== Castel: cuentas al azar =="
  for ((s = 1; s <= AZAR; s++)); do
    python3 -I $AQUI/azar32.py $s 120 $T/azar32_$s.cas $T/azar32_$s.esp
    if ! $AQUI/compilar.sh $T/azar32_$s.cas $T/AZ32_$s.COM > $T/comp.txt 2>&1; then
      ko "azar de 32 bits $s: no compila: $(head -1 $T/comp.txt)"
      continue
    fi
    $AQUI/probar.sh $T/AZ32_$s.COM > $T/azar32_$s.sal 2>&1
    if cmp -s $T/azar32_$s.esp $T/azar32_$s.sal; then ok "azar de 32 bits $s: $(wc -l < $T/azar32_$s.esp) resultados, todos bien"
    else ko "azar de 32 bits $s: hay cuentas que no salen (< esperado, > lo que sale):"; diff $T/azar32_$s.esp $T/azar32_$s.sal | head -6 | sed 's/^/        /'; fi
  done
  for ((s = 1; s <= AZAR; s++)); do
    python3 -I $AQUI/azar.py $s 250 $T/azar$s.cas $T/azar$s.esp
    if ! $AQUI/compilar.sh $T/azar$s.cas $T/AZAR$s.COM > $T/comp.txt 2>&1; then
      ko "azar $s: no compila: $(head -1 $T/comp.txt)"
      continue
    fi
    $AQUI/probar.sh $T/AZAR$s.COM > $T/azar$s.sal 2>&1
    if cmp -s $T/azar$s.esp $T/azar$s.sal; then ok "azar $s: $(wc -l < $T/azar$s.esp) resultados, todos bien"
    else ko "azar $s: hay cuentas que no salen (< esperado, > lo que sale):"; diff $T/azar$s.esp $T/azar$s.sal | head -6 | sed 's/^/        /'; fi
  done
fi

# ------------------------------------------------- 8. Castel de 32 bits
echo "== Castel de 32 bits: «modelo núcleo» (QEMU -kernel) y «modelo hispa32» (HW32) =="
P=$T/p32
mkdir $P
# los ejemplos de 16 bits que no tocan el DOS, compilados como núcleo de 32
# bits: tienen que escribir lo mismo
MISMOS="arreglos bucles cuenta estruct mezcla nivel pares pruebas refs saltos saludo textos modulos"
iguales=0
distintos=
for n in $MISMOS; do
  python3 - $AQUI/ejemplos/$n.cas $P/$n.cas <<'PY'
import sys, re
s = open(sys.argv[1], encoding='utf-8').read().split('\n')
for i, l in enumerate(s):
    if re.match(r'^módulo \w+', l):
        s.insert(i + 1, '    modelo núcleo')
        break
open(sys.argv[2], 'w', encoding='utf-8').write('\n'.join(s))
PY
  if ! $AQUI/compilar.sh $P/$n.cas $P/$n.bin -I $AQUI/ejemplos/partes > $T/comp.txt 2>&1; then
    distintos="$distintos $n(no compila)"; continue
  fi
  $AQUI/probar32.sh $P/$n.bin > $P/$n.sal 2>&1
  cod=$(tail -1 $P/$n.sal); sed -i '$d' $P/$n.sal
  esperado_c="[código 0]"; [ $n = nivel ] && esperado_c="[código 7]"
  if cmp -s $P/$n.sal $AQUI/ejemplos/esperado/$n.txt && [ "$cod" = "$esperado_c" ]; then iguales=$((iguales + 1))
  else distintos="$distintos $n"; fi
done
if [ -z "$distintos" ]; then ok "los $iguales ejemplos de 16 bits sin DOS, como núcleos de 32 bits: escriben lo mismo (y nivel acaba con 7)"
else ko "como núcleos de 32 bits, no escriben lo mismo:$distintos"; fi
# los de ejemplos32: los núcleos, en QEMU; los de hispa32, con HW32 (si está el
# disquete de Hispa-DOS: make y make nucprogs en la raíz)
for f in $AQUI/ejemplos32/*.cas; do
  n=$(basename $f .cas)
  if grep -q '^ *modelo hispa32' $f; then X=EXE; else X=BIN; fi
  if ! $AQUI/compilar.sh $f $P/$n.$X > $T/comp.txt 2>&1; then ko "$n.cas no compila: $(head -1 $T/comp.txt)"; continue; fi
  if [ $X = BIN ]; then
    $AQUI/probar32.sh $P/$n.$X > $P/$n.sal 2>&1
    cod=$(tail -1 $P/$n.sal); sed -i '$d' $P/$n.sal
  elif [ -f $AQUI/../build/hispados.img ] && [ -f $AQUI/../build/HW32.COM ]; then
    NIVEL=42 $AQUI/probar32.sh $P/$n.$X > $P/$n.log 2>&1
    sed '/^A:\\>/,$d' $P/$n.log | sed '/^$/d' > $P/$n.sal
    cod="[código ?]"
    grep -qx '\[nivel-42\]' $P/$n.log && ! grep -qx '\[nivel-43\]' $P/$n.log && cod="[código 42]"
  else
    echo "  (sin el disquete de Hispa-DOS no se prueba $n.EXE: make y make nucprogs en la raíz)"
    continue
  fi
  esp_cod="[código $(sed -n 's/^; código de salida: //p' $f)]"
  if cmp -s $P/$n.sal $AQUI/ejemplos32/esperado/$n.txt && [ "$cod" = "$esp_cod" ]; then
    ok "$n.$X ($(stat -c %s $P/$n.$X) octetos): la salida es la esperada y acaba con el código que toca ($cod)"
  else
    ko "$n.$X: la salida no es la esperada o acaba con otro código ($cod y no $esp_cod; < esperada, > la que sale):"
    diff $AQUI/ejemplos32/esperado/$n.txt $P/$n.sal | sed 's/^/        /' | head -12
  fi
done
# cuentas al azar, con las reglas de 32 bits
for s in 1 2; do
  python3 -I $AQUI/azar32.py $s 150 $P/az$s.cas $P/az$s.esp plano
  $AQUI/compilar.sh $P/az$s.cas $P/az$s.bin > /dev/null 2>&1
  $AQUI/probar32.sh $P/az$s.bin > $P/az$s.sal 2>&1
  sed -i '$d' $P/az$s.sal
  if cmp -s $P/az$s.esp $P/az$s.sal; then ok "azar de 32 bits en el modelo plano $s: $(wc -l < $P/az$s.esp) resultados, todos bien"
  else ko "azar plano $s: hay cuentas que no salen"; diff $P/az$s.esp $P/az$s.sal | head -6 | sed 's/^/        /'; fi
done
# la semilla de Hispa-Pentium: la IDT, el reloj, el teclado y las excepciones
$AQUI/nucleo32/probar.sh "" ayuda hora :espera hora Hola teclas apagar > $P/semilla.sal 2>&1
if grep -q "^órdenes: ayuda hora memoria teclas cero apagar" $P/semilla.sal && grep -q "^no conozco «Hola»" $P/semilla.sal \
    && grep -q "^\[código 5\]" $P/semilla.sal; then ok "la semilla de Hispa-Pentium arranca, lee el teclado y entiende sus órdenes"
else ko "la semilla de Hispa-Pentium no hace lo que debe:"; sed 's/^/        /' $P/semilla.sal | head -12; fi
s1=$(grep "^llevo" $P/semilla.sal | head -1 | awk '{print $2 * 100 + $5}')
s2=$(grep "^llevo" $P/semilla.sal | tail -1 | awk '{print $2 * 100 + $5}')
if [ -n "$s1" ] && [ -n "$s2" ] && [ $((s2 - s1)) -ge 100 ]; then ok "el reloj (IRQ 0) corre: $s1 y luego $s2 centésimas"
else ko "el reloj no corre: «$s1» y «$s2»"; fi
if grep -q "^28 teclas pulsadas" $P/semilla.sal; then ok "el teclado (IRQ 1) cuenta las teclas, mayúsculas incluidas"
else ko "el teclado no cuenta bien las teclas: $(grep teclas $P/semilla.sal | tail -1)"; fi
$AQUI/nucleo32/probar.sh "" cero > $P/cero.sal 2>&1
if grep -q "excepción 0: división por cero" $P/cero.sal && grep -q "^\[código 9\]" $P/cero.sal; then ok "una división por cero llega a su excepción (la 0, por la IDT)"
else ko "la división por cero no llega a la excepción"; sed 's/^/        /' $P/cero.sal | head -6; fi

echo "== $nok bien, $fail mal =="
exit $((fail > 0))
