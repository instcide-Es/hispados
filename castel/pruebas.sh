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
#      disquete de Hispa-DOS), con la biblioteca de 32 bits (biblio32);
#      cuentas al azar con las reglas de 32 bits; y la semilla de
#      Hispa-Pentium (nucleo32): IDT, reloj, teclado, excepciones, la
#      paginación con su fallo de página atendido, el montón (sin fugas, y
#      viendo lo que lo estropea), las tareas que se alternan con el reloj,
#      dormir y esperar, y los programas del anillo 3 con su llamada al
#      sistema (y los que hacen lo que no deben, que se acaban). Y con un
#      disco duro IDE de imagen (nucleo32/disco.sh; FAT12, FAT16, FAT32 y con
#      tabla de particiones): el controlador ATA, la FAT, cargar programas
#      del disco, las llamadas al sistema nuevas (teclado, archivos, lanzar y
#      esperar, memoria que crece), la pila que crece sola y el intérprete
#      de órdenes del anillo 3. Y escribiendo en el disco (en copias de esas
#      imágenes, que luego miran fsck.fat y mtools): crear, escribir, alargar,
#      cortar y borrar archivos, las carpetas, COPY, DEL, REN, MD, RD y la
#      redirección; CD y la carpeta de trabajo, PATH, los archivos .BAT, la
#      historia de la línea y Ctrl+C; el teclado español y los nombres largos.
# Necesita python3, nasm, qemu-system-i386, socat, mtools, mkfs.fat y fsck.fat. El disquete de
# Hispa-8086 se monta solo (make en h8086) si no está hecho.
#   SOLO=errores ./pruebas.sh     solo la primera parte (no arranca QEMU)
#   SOLO=32 ./pruebas.sh          la primera y la octava (lo de 32 bits)
#   SIN_ARRANQUE=1 ./pruebas.sh   sin el arranque en tres pasos
#   AZAR=3 ./pruebas.sh           además, tres programas de cuentas al azar
#                                 de 16 bits (azar.py) y otros tres de 32
#                                 (azar32.py): lo que calculan frente a lo
#                                 que tiene que dar. Unos 10 s más cada uno.
#   VER=1 ./pruebas.sh            deja lo que sale en /tmp/castel-pruebas
set -u
AQUI=$(dirname "$(readlink -f "$0")")
H=$AQUI/../h8086
# los programas de usuario de Hispa-Pentium (usuario.cas y llamadas.cas son módulos)
USU="$AQUI/nucleo32/uhola.cas $AQUI/nucleo32/umalo.cas $AQUI/nucleo32/ucli.cas $AQUI/nucleo32/ueco.cas $AQUI/nucleo32/uordenes.cas $AQUI/nucleo32/uprueba.cas $AQUI/nucleo32/uescribe.cas $AQUI/nucleo32/ubucle.cas $AQUI/nucleo32/ubloc.cas $AQUI/nucleo32/uconsola.cas"
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

# SOLO=32: solo la primera parte y la octava (los 32 bits)
if [ "${SOLO:-}" != 32 ]; then
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
for f in $AQUI/ejemplos32/*.cas $AQUI/nucleo32/semilla.cas $USU $T/ens/az32p.cas; do
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
# (en dos disquetes: en uno no caben ya todos; el segundo, lo de
# Hispa-Pentium: la semilla, que con el entorno gráfico pasa de medio mega, y
# sus programas de usuario)
NUC=" semilla $(for f in $USU; do basename $f .cas; done | tr '\n' ' ')"
mkdir $T/ens/sal
for tanda in 1 2; do
cp $H/build/hispa8086-1440.img $T/e.img
{ printf '@ECO OFF\r\n'
  for f in $T/ens/*.asm; do
    n=$(basename $f .asm)
    case "$NUC" in *" $n "*) [ $tanda = 1 ] && continue ;; *) [ $tanda = 2 ] && continue ;; esac
    N=$(echo $n | tr a-z A-Z)
    mcopy -o -i $T/e.img $f ::$N.ASM || ko "ENSAMBLA: $N.ASM no cabe en el disquete $tanda"
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
[ $hecho = 1 ] || ko "ENSAMBLA: el lote $tanda no ha llegado al final"
mcopy -n -i $T/e.img '::*.BIN' $T/ens/sal/ > /dev/null 2>&1
done
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
# dos disquetes: en A: la biblioteca en \BIB (la de 32 bits en \BIB32), los
# módulos de ejemplo en \PARTES, los de la semilla de Hispa-Pentium,
# los ejemplos, la biblioteca entera (--todo), ENSAMBLA, los errores y el
# temporal de CASTEL; en B:, lo que sale (el ensamblador de cada uno son, todos
# juntos, más de un disquete si va con los fuentes: el de ENSAMBLA solo pasa de
# 250 K)
python3 -I $AQUI/azar.py 7 45 $A/azar16.cas $A/azar16.esp
python3 -I $AQUI/azar32.py 7 28 $A/azar32.cas $A/azar32.esp
python3 -I $AQUI/azar32.py 8 40 $A/azar32p.cas $A/azar32p.esp plano
# (en dos tandas, cada una con sus disquetes: en la primera, los ejemplos, la
# biblioteca, ENSAMBLA, las cuentas al azar y los errores; en la segunda, lo
# de Hispa-Pentium, que con el entorno gráfico ya no cabe con lo demás: los
# fuentes del núcleo en A: y, en B:, el ensamblador de la semilla, de más de
# medio mega, y el de sus programas de usuario)
for tanda in 1 2; do
cp $H/build/hispa8086-1440.img $T/c.img
mmd -i $T/c.img ::BIB ::PARTES ::BIB32
for f in $AQUI/biblioteca32/*.cas; do mcopy -i $T/c.img $f ::BIB32/$(basename $f .cas | tr a-z A-Z | cut -c1-8).CAS; done
if [ $tanda = 1 ]; then
  for f in $AQUI/biblioteca/*.cas; do mcopy -i $T/c.img $f ::BIB/$(basename $f .cas | tr a-z A-Z | cut -c1-8).CAS; done
  for f in $AQUI/ejemplos/partes/*.cas; do mcopy -i $T/c.img $f ::PARTES/$(basename $f .cas | tr a-z A-Z | cut -c1-8).CAS; done
  LOTE="$(echo $AQUI/ejemplos/*.cas) $AQUI/ensamblador/ensambla.cas $T/ens/toda.cas $A/azar16.cas $A/azar32.cas $(echo $AQUI/ejemplos32/*.cas) $A/azar32p.cas"
else
  for f in $AQUI/nucleo32/*.cas; do mcopy -i $T/c.img $f ::$(basename $f .cas | tr a-z A-Z | cut -c1-8).CAS; done
  LOTE="$AQUI/nucleo32/semilla.cas $USU"
fi
mcopy -i $T/c.img $A/CASTEL.EXE ::
mformat -C -f 1440 -i $T/cb.img ::
{ printf '@ECO OFF\r\n'
  for f in $LOTE; do
    n=$(basename $f .cas); N=$(echo $n | tr a-z A-Z)
    todo=; [ $n = toda ] && todo=--todo
    mcopy -o -i $T/c.img $f ::$N.CAS
    python3 -I $AQUI/ccastel.py $todo $f -o $A/ref/$N.ASM -I $AQUI/ejemplos/partes > /dev/null 2>&1
    printf 'CASTEL %s.CAS -o B:%s.ASM -I BIB -I PARTES -I BIB32 %s > B:%s.SAL\r\n' $N $N "$todo" $N
  done
  if [ $tanda = 1 ]; then
    i=0
    for f in $AQUI/ejemplos/errores/*.cas $AQUI/ejemplos32/errores/*.cas; do
      i=$((i + 1)); N=$(printf 'E%02d' $i)
      mcopy -i $T/c.img $f ::$N.CAS
      echo "$N $(basename $f .cas)" >> $A/errores.txt
      printf 'CASTEL %s.CAS -I BIB -I PARTES > B:%s.SAL\r\n' $N $N
    done
  fi
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
[ $hecho = 1 ] || ko "CASTEL: el lote $tanda no ha llegado al final"
mcopy -n -i $T/cb.img '::*.ASM' '::*.SAL' $A/sal/ > /dev/null 2>&1
done
iguales=0
distintos=
for f in $A/ref/*.ASM; do
  N=$(basename $f .ASM)
  if cmp -s $f $A/sal/$N.ASM; then iguales=$((iguales + 1)); else distintos="$distintos $N"; fi
done
if [ -z "$distintos" ]; then ok "CASTEL: el mismo ensamblador que ccastel.py en los $iguales programas (ejemplos de 16 y de 32 bits, biblioteca entera, ENSAMBLA, la semilla con sus módulos, los programas de usuario y tres de cuentas al azar)"
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

fi   # SOLO=32

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
if grep -q "^órdenes: ayuda hora memoria mapa paginas monton tareas programas ejecutar N" $P/semilla.sal && grep -q "^no conozco «Hola»" $P/semilla.sal \
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
# la memoria, las tareas y el anillo 3 (con los programas uhola, umalo y ucli
# como módulos de multiboot)
$AQUI/nucleo32/probar.sh "" memoria paginas toca monton estropear turnos dormir tareas programas \
  "ejecutar 0" "ejecutar 1" "ejecutar 2" memoria apagar > $P/pentium.sal 2>&1
S=$P/pentium.sal
mira() {       # lo que se prueba, y las líneas que tienen que salir (expresiones de grep)
  local que=$1 l; shift
  for l in "$@"; do
    if ! grep -q -- "$l" $S; then ko "$que: falta «$l»"; sed 's/^/        /' $S | grep -v '^ *$' | head -60 | tail -20; return; fi
  done
  ok "$que"
}
mira "la memoria: el mapa de multiboot da los marcos de 4 K (mapa de bits)" "^memoria: 32 MB, [0-9]* marcos libres; paginación activa; 3 programas de usuario" \
  "^marcos: [0-9]* libres de 8160 "
mira "la paginación está encendida (CR0.PG), con la identidad para el núcleo" "^paginación: activa (CR0 = 0x8" "^  identidad: de 0 a 0x01FDFFFF, 8160 páginas en 8 tablas"
mira "un fallo de página en la zona a la demanda se atiende (un marco nuevo) y la escritura llega" \
  "^escribo en 0xD0000123 (fallos de página atendidos: 1) y leo 0xCAFE0001" "^otra vez (fallos de página atendidos: 0) y leo 0xCAFE0002"
mira "el montón: 3000 vueltas de reservar y liberar, sin fugas (ni de bloques ni de marcos)" "^montón: 3000 vueltas de reservar y liberar (y [0-9]* al final), 0 fallos" "^montón sin fugas"
mira "el montón ve lo que lo estropea: escribir de más, soltar dos veces, soltar lo que no es suyo" \
  "se ha escrito más allá de los 10 octetos del bloque" "^falso" "^lo arreglo y suelto: cierto" "ya estaba libre (¿se suelta dos veces?)" \
  "no es de un bloque" "^suelto el otro: cierto" "^el montón ha visto 3 fallos; revisado, 0"
t=$(grep "^turnos:" $S)
a=$(echo "$t" | awk '{print $5}'); b=$(echo "$t" | awk '{print $9}' | tr -d ';'); n=$(echo "$t" | awk '{print $13}')
if [ -n "$t" ] && [ "${a:-0}" -gt 0 ] && [ "${b:-0}" -gt 0 ] && [ "${n:-0}" -ge 50 ]; then ok "dos tareas que no ceden nunca se alternan con el reloj (multitarea apropiativa): $n veces"
else ko "las tareas no se alternan: «$t»"; fi
if [ "$(grep '^la dormilona' $S | awk '{print $3}' | tr -d '\n')" = 123 ] && grep -q "^la dormilona 2 se despierta: ha dormido 4[0-9] tics (pidió 40)" $S \
    && grep -q "^las tres han acabado" $S; then ok "dormir y esperar: tres dormilonas despiertan cada una a su hora, y se las espera"
else ko "las dormilonas no despiertan en orden:"; grep dormilona $S | sed 's/^/        /'; fi
mira "las tareas, con sus turnos" "^   1  consola       corriendo       0" "^   2  ociosa  " "cambios de tarea en"
m1=$(grep "^uhola.bin ha acabado" $S | awk '{print $10, $13}')
mira "un programa del anillo 3, en su espacio de direcciones, escribe por la int 0x80, duerme y acaba con su código" \
  "^ejecuto uhola.bin (la tarea [0-9]*, en el anillo 3)" "^hola desde el anillo 3: soy la tarea" "^1 + 2 + ... + 1000 = 500500" \
  "^he dormido 10 tics: cierto" "^uhola.bin ha acabado con el código 3; marcos libres"
mira "la llamada al sistema no deja escribir desde la memoria del núcleo ni desde lo que no es del programa" \
  "^el núcleo no me deja enseñar su memoria: cierto" "^ni lo que no tengo: cierto"
if [ -n "$m1" ] && [ "$(echo $m1 | awk '{print $1}')" = "$(echo $m1 | awk '{print $2}')" ]; then ok "al acabar el programa, su espacio de direcciones vuelve entero: los mismos marcos libres ($m1)"
else ko "el programa de usuario se deja marcos: «$m1»"; fi
mira "un programa que lee la memoria del núcleo: fallo de página de protección, se acaba él y el núcleo sigue" \
  "fallo de página en 0x00100000 (lectura, anillo 3, protección)" "^\*\*\* se acaba la tarea [0-9]* (umalo.bin)" "^umalo.bin ha acabado con el código 255"
mira "un programa que hace «cli» en el anillo 3: protección general, y se acaba" \
  "excepción 13: protección general (código 0)" "^ucli.bin ha acabado con el código 255" "^\[código 5\]"
if grep -q "no tenía que salir" $S; then ko "un programa del anillo 3 ha seguido después de lo que no podía hacer"; else ok "lo que no podían hacer, no lo han hecho"; fi
$AQUI/nucleo32/probar.sh "" fallo > $P/fallo.sal 2>&1
if grep -q "fallo de página en 0xDEAD0000 (lectura, núcleo, no presente)" $P/fallo.sal && grep -q "^\[código 14\]" $P/fallo.sal; then ok "un fallo de página del núcleo fuera de la zona a la demanda: lo dice con la dirección y se para (código 14)"
else ko "el fallo de página del núcleo no se dice bien"; sed 's/^/        /' $P/fallo.sal | head -6; fi

# ---- el disco, la FAT, las llamadas nuevas y el intérprete de órdenes ----
# (un disco duro IDE de imagen, hecho con mkfs.fat y mtools: nucleo32/disco.sh)
echo "== Hispa-Pentium con disco: ATA, FAT, programas del disco y el intérprete de órdenes del anillo 3 =="
hechos=1
for c in 12 16 32 mbr; do
  $AQUI/nucleo32/disco.sh $P/d$c.img $c > $P/disco$c.txt 2>&1 || { hechos=0; ko "no se puede hacer el disco de prueba ($c): $(tail -1 $P/disco$c.txt)"; }
done
if [ $hechos = 1 ]; then
# lo que tiene que sumar TEXTOS\LARGO.TXT (el mismo que escribe disco.sh)
suma=$(python3 -c "print(sum(sum(('linea %03d de un archivo de varios clusters\r\n' % i).encode()) for i in range(1, 401)))")
# 1. desde la consola del núcleo, con FAT16
DISCO=$P/d16.img $AQUI/nucleo32/probar.sh "" disco dir "dir textos" "dir nada" "lanzar hola.bin" "lanzar eco.bin uno dos tres" \
  "lanzar noprog.bin" "lanzar nada.bin" disco apagar > $P/disco.sal 2>&1
S=$P/disco.sal
mira "el controlador ATA (PIO): IDENTIFY dice qué disco hay en el canal primario, y cuántos sectores" \
  "^disco 0: QEMU HARDDISK, 16 MB, FAT16\.$" "^disco 0: QEMU HARDDISK, 32768 sectores (16 MB), LBA" "^disco 1: no hay"
mira "la FAT16 se monta y se lee su raíz (los nombres largos se saltan; queda el corto)" \
  "^sistema de archivos: FAT16, desde el sector 0, [0-9]* clusters de 2048 octetos, [0-9]* libres" \
  "^ORDENES.BIN  *[0-9]*$" "^LEEME.TXT  *118$" "^NOMBRE~1.TXT  *32$" "^TEXTOS  *<carpeta>" "^17 entradas"
mira "una carpeta dentro de otra, y una que no está" "^LARGO.TXT  *17600$" "^PIRATA.TXT  *110$" "^5 entradas" "^no puedo abrir «nada»: no existe"
m1=$(grep "^hola.bin ha acabado" $S | awk '{print $10, $13}')
mira "un programa del anillo 3 cargado del disco (no de multiboot): corre y acaba con su código" \
  "^lanzo hola.bin del disco (la tarea [0-9]*, en el anillo 3)" "^1 + 2 + ... + 1000 = 500500" "^hola.bin ha acabado con el código 3; marcos libres"
if [ -n "$m1" ] && [ "$(echo $m1 | awk '{print $1}')" = "$(echo $m1 | awk '{print $2}')" ]; then ok "al acabar el programa del disco, la memoria vuelve entera: los mismos marcos libres ($m1)"
else ko "el programa del disco se deja marcos: «$m1»"; fi
mira "un programa del disco recibe sus argumentos" "^uno dos tres$" "^eco.bin ha acabado con el código 3"
mira "lo que no es un programa, o no está, no se lanza (y se dice por qué)" "^no puedo lanzar noprog.bin: no es un programa de Hispa-Pentium" \
  "^no puedo lanzar nada.bin: no existe" "; archivos abiertos: 0$" "^\[código 5\]"
# 2. el intérprete de órdenes del anillo 3 (ORDENES.BIN, del disco)
DISCO=$P/d16.img $AQUI/nucleo32/probar.sh "" ordenes ayuda ver mem dir "dir textos" "dir nada" "type leeme.txt" "type textos\\pirata.txt" \
  "type nada.txt" "TYPE textos" "eco hola y adios" "textos\\eco2 desde una carpeta" hola malo nada noprog prueba :espera "prueba fuera" \
  "prueba pozo" :espera salir tareas memoria disco apagar > $P/ordenes.sal 2>&1
S=$P/ordenes.sal
mira "el intérprete de órdenes es un programa del anillo 3 cargado del disco, y lee el teclado por la llamada al sistema" \
  "^lanzo ORDENES.BIN del disco (la tarea [0-9]*, en el anillo 3)" "^Hispa-Pentium: intérprete de órdenes (un programa del anillo 3" \
  "^C:.>ayuda$" "^Las órdenes de Hispa-Pentium:" "^  TYPE archivo "
mira "VER y MEM" "^Hispa-Pentium 0\.3: un núcleo de 32 bits escrito en Castel" "^Memoria: 32640 K en total, [0-9]* K libres ([0-9]* marcos de 4 K)" \
  "^Este intérprete ocupa [0-9]* K. Tareas en marcha: 3\.$"
mira "DIR: la raíz y una carpeta, con sus tamaños, sus fechas y lo que queda libre" "^ Directorio de C:.$" "^LEEME    TXT  *118  [0-9/]*  [0-9:]*$" \
  "^TEXTOS  *<DIR>  " "^ *14 archivos  *[0-9]* octetos$" "^ *3 carpetas  *[0-9]* octetos libres$" "^ Directorio de C:.TEXTOS$" \
  "^LARGO    TXT  *17600  " "^ *3 archivos  *18[0-9]* octetos$" "^No puedo abrir nada: no existe\.$"
mira "TYPE: un archivo de la raíz y uno de una carpeta (abrir, leer y cerrar por la llamada al sistema)" \
  "^Este archivo está en un disco FAT y lo enseña la orden TYPE\.$" "^Con diez cañones por banda,$" "^un velero bergantín\.$" \
  "^No puedo abrir nada.txt: no existe\.$" "^textos es una carpeta"
mira "un programa se lanza por su nombre, con sus argumentos, y se le espera (también de una carpeta)" "^hola y adios$" \
  "^(ECO.BIN ha acabado con el código 3)$" "^desde una carpeta$" "^(TEXTOS.ECO2.BIN ha acabado con el código 3)$" "^(HOLA.BIN ha acabado con el código 3)$"
mira "un programa que hace lo que no debe se acaba, y el intérprete sigue; lo desconocido se dice" \
  "^\*\*\* se acaba la tarea [0-9]* (MALO.BIN)" "^(MALO.BIN ha acabado con el código 255)$" \
  "^Orden o nombre de programa desconocido: nada (escriba AYUDA)\.$" "^No puedo lanzar NOPROG.BIN: no es un programa de Hispa-Pentium\.$"
mira "la pila de un programa crece sola: el fallo de página le pone páginas según baja (y el núcleo, si va a escribir en ella)" \
  "^pila: leo 17600 octetos del disco a un arreglo local sin estrenar ([4-6] páginas más)$" \
  "^pila: 200 llamadas, una dentro de otra, con 4000 octetos cada una: 200; la pila ha crecido 19[0-9] páginas$"
mira "una pila que se desboca (una función que se llama sin fin) acaba en su tope de 4 MB: se acaba el programa, no el núcleo" \
  "^pozo: una función que se llama a sí misma sin fin\.\.\.$" "fallo de página en 0xBFC00[0-9A-F]* (escritura, anillo 3, no presente)"
mira "la memoria del programa crece y se devuelve (sbrk): páginas nuevas a cero, y vuelven" \
  "^memoria: pido 100000 octetos: 25 páginas más, a cero: cierto, y la suma de lo escrito es 312487500$" \
  "^memoria: la devuelvo: cierto, y quedan 0 páginas de más$" "^memoria: no me dan 3 GB: cierto; ni devuelvo lo que no pedí: cierto$"
mira "leer un archivo de varios clusters a trozos de 777 octetos: lo mismo que hay en el disco" \
  "^archivo: TEXTOS.LARGO.TXT mide 17600 octetos; leídos 17600 a trozos de 777, 400 líneas, suma $suma$" \
  "^archivo: al final ya no da más: cierto$" "^archivo: cerrado, ya no se deja leer: cierto$"
mira "los archivos abiertos tienen un tope, y las direcciones que no son del programa no se tocan" \
  "^abiertos: caben 16 a la vez; el siguiente: hay demasiados archivos abiertos$" "^abiertos: cerrados todos, se puede abrir otra vez: cierto$" \
  "^punteros: leer a la memoria del núcleo: cierto; a donde no tengo nada: cierto; un nombre que no es mío: cierto$" \
  "^punteros: un manejador que no existe: cierto; esperar a una tarea que no es mía: cierto$"
mira "un programa lanza otros dos a la vez y los espera" "^soy el hijo uno$" "^y yo soy el dos$" \
  "^hijos: los dos han acabado, con los códigos 4 y 5; ya no se les puede esperar: cierto$" "^hijos: los dos ruidosos han acabado, con los códigos 9 y 9$"
ra=$(grep -c "^ruido A [0-9]\{3\}: una línea entera, de principio a fin, sin letras de otro$" $S)
rb=$(grep -c "^ruido B [0-9]\{3\}: una línea entera, de principio a fin, sin letras de otro$" $S)
rm_=$(grep -a "ruido [AB]" $S | grep -vc "^ruido [AB] [0-9]\{3\}: una línea entera, de principio a fin, sin letras de otro$")
alt=$(grep "^ruido [AB] " $S | awk '{print $2}' | uniq | wc -l)
if [ "$ra" = 120 ] && [ "$rb" = 120 ] && [ "$rm_" = 0 ] && [ "$alt" -gt 2 ]; then ok "la consola con su cerrojo: dos programas que escriben a la vez (240 líneas, $alt turnos) no se mezclan ni una letra"
else ko "las líneas de dos programas que escriben a la vez se mezclan o se pierden: $ra de A, $rb de B, $rm_ rotas, $alt turnos"; fi
mira "la memoria devuelta ya no se puede tocar: fallo de página, y se acaba el programa" "^fuera: escribo en la memoria que me han dado: cierto$" \
  "fallo de página en 0x4000[0-9A-F]*000 (escritura, anillo 3, no presente)" "^(PRUEBA.BIN ha acabado con el código 255)$"
if grep -q "no tenía que salir" $S; then ko "un programa ha seguido después de lo que no podía hacer"; else ok "lo que no podían hacer (leer el núcleo, tocar lo devuelto, salir del pozo), no lo han hecho"; fi
m1=$(grep "^ORDENES.BIN ha acabado" $S | awk '{print $10, $13}')
mira "al salir, el núcleo ha recogido lo que dejó PRUEBA sin cerrar ni esperar (un archivo abierto y un programa)" \
  "^prueba: acabo con el código 7, un archivo abierto y un programa sin esperar$" "^(PRUEBA.BIN ha acabado con el código 7)$" \
  "soy el que nadie espera: he dormido y acabo solo$" \
  "^Hasta luego\.$" "^ORDENES.BIN ha acabado con el código 0; marcos libres" "; archivos abiertos: 0$" "^montón: 1 bloques ocupados con 8192 octetos"
if [ -n "$m1" ] && [ "$(echo $m1 | awk '{print $1}')" = "$(echo $m1 | awk '{print $2}')" ] && [ "$(grep -c '^ *[0-9]*  [A-Za-z.-]*  *[a-z]*  *[03]  ' $S)" = 2 ]; then
  ok "y no queda nada: los mismos marcos libres que antes de lanzar el intérprete ($m1) y solo las dos tareas del núcleo"
else ko "tras el intérprete queda memoria o tareas sin recoger: «$m1»"; grep -A6 "^> tareas" $S | sed 's/^/        /'; fi
# CLS: se mira la pantalla (lo que hay justo antes de la última orden)
VER=1 DISCO=$P/d16.img $AQUI/nucleo32/probar.sh "" ordenes dir cls apagar > $P/cls.sal 2>&1
if [ "$(sed -n '/^\[pantalla\]/,$p' $P/cls.sal | sed -n '2p')" = '|C:\>' ] && [ "$(sed -n '/^\[pantalla\]/,$p' $P/cls.sal | sed '1,2d' | grep -vc '^|$')" = 0 ]; then
  ok "CLS deja la pantalla en blanco, con la petición de órdenes arriba"
else ko "CLS no borra la pantalla:"; sed -n '/^\[pantalla\]/,$p' $P/cls.sal | head -5 | sed 's/^/        /'; fi
# 3. las otras FAT, y el disco con tabla de particiones
for c in 12 32 mbr; do
  case $c in 12) que="FAT12"; f=FAT12; s0=0 ;; 32) que="FAT32"; f=FAT32; s0=0 ;; mbr) que="un disco con tabla de particiones (la FAT16, en la primera)"; f=FAT16; s0=63 ;; esac
  DISCO=$P/d$c.img $AQUI/nucleo32/probar.sh "" disco "dir textos" "lanzar prueba.bin" :espera "lanzar textos\\eco2.bin hola" apagar > $P/fat$c.sal 2>&1
  S=$P/fat$c.sal
  mira "$que: se monta, se lee una carpeta, un archivo de varios clusters y se cargan programas" \
    "^sistema de archivos: $f, desde el sector $s0, " "^LARGO.TXT  *17600$" \
    "^archivo: TEXTOS.LARGO.TXT mide 17600 octetos; leídos 17600 a trozos de 777, 400 líneas, suma $suma$" \
    "^hijos: los dos ruidosos han acabado, con los códigos 9 y 9$" "^prueba.bin ha acabado con el código 7" "^hola$" "^eco2.bin ha acabado con el código 1"
done
# 4. el intérprete, él solo al arrancar (si el cargador dice «ordenes»); y sin disco
DISCO=$P/d16.img ARRANQUE=ordenes MODULOS= $AQUI/nucleo32/probar.sh "" ver salir apagar > $P/arranque.sal 2>&1
S=$P/arranque.sal
mira "si el cargador se lo dice (-append ordenes), el núcleo arranca con el intérprete de órdenes (y sin módulos de multiboot: todo del disco)" \
  "^memoria: 32 MB, [0-9]* marcos libres; paginación activa; 0 programas de usuario" \
  "^C:.>ver$" "^Hispa-Pentium 0\.3" "^ORDENES.BIN ha acabado con el código 0" "^\[código 5\]"
$AQUI/nucleo32/probar.sh "" disco dir ordenes apagar > $P/sindisco.sal 2>&1
S=$P/sindisco.sal
mira "sin disco, el núcleo lo dice y sigue" "^disco 0: no hay" "^sistema de archivos: no hay" "^no puedo lanzar ORDENES.BIN: no hay disco" "^\[código 5\]"

# ---- escribir en el disco: ATA, la FAT (12, 16, 32 y con particiones) y las órdenes ----
# (cada prueba escribe en una copia del disco, con ESCRIBE=1; luego la miran fsck.fat y mtools)
echo "== Hispa-Pentium escribe en el disco: ATA, FAT12/16/32, COPY, DEL, REN, MD, RD y la redirección =="
export MTOOLS_SKIP_CHECK=1
# ¿está sano el volumen de esa imagen (según fsck.fat)? Con un segundo argumento, el volumen empieza en ese sector
sano() {
  local v=$1
  if [ -n "${2:-}" ]; then v=$P/vol.img; dd if=$1 of=$v bs=512 skip=$2 status=none; fi
  fsck.fat -n $v > $P/fsck.txt 2>&1
}
# un archivo de la imagen, a un archivo de aquí (falla si no está)
saca() { rm -f $3; mcopy -n -i $1 "::$2" $3 > /dev/null 2>&1; }
for c in 16 12 32 mbr; do
  case $c in 12) que="FAT12" ;; 16) que="FAT16" ;; 32) que="FAT32" ;; mbr) que="FAT16 en una partición" ;; esac
  I=$P/w$c.img; cp $P/d$c.img $I
  M=$I; s0=; [ $c = mbr ] && { M=$I@@32256; s0=63; }
  lleno=(); [ $c = 12 ] && lleno=("escribe lleno" :espera :espera :espera)
  ESCRIBE=1 DISCO=$I $AQUI/nucleo32/probar.sh "" ordenes escribe "echo primera línea > nuevo.txt" "echo y la segunda >> nuevo.txt" "type nuevo.txt" \
    "copy textos\\largo.txt copia.txt" "md obra" "cd obra" "copy \\textos\\*.txt" "copy \\prueba.bin ." "md sub" "ren pirata.txt poema.txt" \
    "del largo.txt" "rd sub" "cd \\" "rd obra" "eco hola mundo > eco.txt" "dir obra >> eco.txt" "ren leeme.txt hola.bin" "del nada.txt" \
    "copy nada.txt x.txt" "md textos" "${lleno[@]}" salir disco apagar > $P/escribe$c.sal 2>&1
  S=$P/escribe$c.sal
  mira "$que: un programa crea un archivo, lo escribe a trozos, lo lee de vuelta, escribe en medio, lo corta, lo alarga y le añade" \
    "^escribir: un archivo nuevo, 10000 octetos a trozos de 777; mide 10000; ocupa los clusters que tocan: cierto$" \
    "^escribir: leído de vuelta: 10000 octetos, 0 distintos; no se deja escribir si se abrió para leer: cierto$" \
    "^en medio: en 5000 escribo HOLA y leo HOLA; sigue midiendo 10000$" "^cortar: en 3000: 0; mide 3000; quedan libres los clusters que sobran: cierto$" \
    "^hueco: escribo en 20000: mide 20001, y lo de en medio está a cero (0 octetos que no)$" "^añadir: tres octetos al final: mide 20004 y acaba en FIN$" \
    "^crear encima: mide 0; borrado: 0; ya no está: cierto$"
  mira "$que: lo que no se puede (abrir dos veces para escribir, borrar lo abierto, nombres que no valen...) se dice" \
    "^no se puede: abrirlo otra vez para escribir: cierto; ni para leer: cierto; ni borrarlo: cierto; ni cambiarle el nombre: cierto$" \
    "^no se puede: crear en una carpeta que no está: cierto; un nombre de más de 8+3: cierto; ni con letras raras: cierto; ni el de un dispositivo: cierto; escribir en una carpeta: cierto$" \
    "^no se puede: borrar lo que no está: cierto; borrar una carpeta como archivo: cierto; abrir para escribir lo que no está: cierto$"
  mira "$que: carpetas: hacerlas, entrar, 70 archivos (la carpeta crece), mover y cambiar nombres, vaciarlas y borrarlas; todo queda como estaba" \
    "^carpetas: hago PD: 0; otra vez no: cierto; entro: 0; estoy en .PD$" "^carpetas: 70 archivos en PD: 70 hechos; la carpeta tiene 72 entradas" \
    "^carpetas: F42.TXT dice «soy el archivo 42»; y «...PD...F07.TXT» se abre: cierto$" \
    "^mover: F00.TXT a DENTRO.CERO.TXT: 0; está allí: cierto y no aquí: cierto; la carpeta pasa a llamarse FUERA: 0 y lleva lo suyo: cierto$" \
    "^no se puede: llamarlo como otro que ya está: cierto; borrar una carpeta con algo: cierto; ni la carpeta donde estoy: cierto; ni un archivo como carpeta: cierto$" \
    "^carpetas: borrados 70 archivos; FUERA: 0; PD: 0; ya no se puede entrar: cierto$" "^escribe: todo queda como estaba (los mismos clusters libres): cierto$"
  mira "$que: las órdenes dicen lo que no puede ser (y el programa redirigido, su código, por la pantalla)" \
    "^No puedo borrar la carpeta obra: la carpeta no está vacía\.$" "^No puedo llamar hola.bin a leeme.txt: ya existe\.$" "^No puedo borrar nada.txt: no existe\.$" \
    "^No puedo abrir nada.txt: no existe\.$" "^No puedo hacer la carpeta textos: ya existe\.$" "^(ECO.BIN ha acabado con el código 2)$" \
    "^ *2 archivo(s) copiado(s)\.$" "^primera línea$" "^y la segunda$" "; archivos abiertos: 0$" "^\[código 5\]"
  if [ $c = 12 ]; then
    mira "$que: el disco se llena (se escribe hasta el último cluster), se dice, y al borrar vuelve todo" \
      "^lleno: escribo hasta que no cabe: cierto; quedan 0 clusters libres; el siguiente: no queda sitio en el disco$" \
      "^lleno: ni otro archivo con algo: no queda sitio en el disco; ni una carpeta: no queda sitio en el disco$" \
      "^lleno: lo borro: 0; vuelven a estar libres los de antes: cierto$"
  fi
  # lo que ha quedado en el disco, visto desde fuera
  if sano $I $s0; then ok "$que: después de todo eso, fsck.fat da el disco por sano ($(tail -1 $P/fsck.txt | sed 's/^[^:]*: //'))"
  else ko "$que: fsck.fat encuentra el disco mal:"; sed 's/^/        /' $P/fsck.txt | head -12; fi
  bien=1
  saca $M NUEVO.TXT $P/x.txt && printf 'primera l\xa1nea\r\ny la segunda\r\n' | cmp -s - $P/x.txt || bien=0
  saca $M COPIA.TXT $P/x.txt && saca $P/d16.img TEXTOS/LARGO.TXT $P/y.txt && cmp -s $P/x.txt $P/y.txt || bien=0
  saca $M OBRA/POEMA.TXT $P/x.txt && saca $P/d16.img TEXTOS/PIRATA.TXT $P/y.txt && cmp -s $P/x.txt $P/y.txt || bien=0
  saca $M OBRA/PRUEBA.BIN $P/x.txt && saca $P/d16.img PRUEBA.BIN $P/y.txt && cmp -s $P/x.txt $P/y.txt || bien=0
  saca $M ECO.TXT $P/x.txt && [ "$(head -1 $P/x.txt)" = $'hola mundo\r' ] && grep -a -q '^ Directorio de C:.OBRA' $P/x.txt && grep -a -q '^POEMA    TXT  *110  ' $P/x.txt \
    && ! grep -a -q -v $'\r$' $P/x.txt || bien=0
  saca $M OBRA/LARGO.TXT $P/x.txt && bien=0
  saca $M OBRA/PIRATA.TXT $P/x.txt && bien=0
  mdir -i $M ::OBRA 2> /dev/null | grep -q '^SUB ' && bien=0
  mdir -i $M :: 2> /dev/null | grep -q -i '^PD \|^P1 \|^LLENO ' && bien=0
  if [ $bien = 1 ]; then ok "$que: y mtools ve lo mismo: lo creado con «>» y «>>» (con sus finales de línea), lo copiado (igual que el original), lo renombrado y lo borrado"
  else ko "$que: lo que hay en el disco no es lo que tenía que ser:"; mdir -i $M :: ::OBRA 2>&1 | sed 's/^/        /' | head -40; fi
  # y otra vez encendido, el núcleo lee lo que escribió
  DISCO=$I $AQUI/nucleo32/probar.sh "" ordenes "type nuevo.txt" "dir obra" "type eco.txt" salir apagar > $P/relee$c.sal 2>&1
  S=$P/relee$c.sal
  mira "$que: al volver a arrancar, lo escrito sigue ahí" "^primera línea$" "^y la segunda$" "^POEMA    TXT  *110  " "^PRUEBA   BIN  *[0-9]*  " \
    "^ *2 archivos  *[0-9]* octetos$" "^hola mundo$" "^ Directorio de C:.OBRA$"
done

# ---- el intérprete: carpetas de trabajo, PATH, archivos de órdenes, la historia, Ctrl+C ----
echo "== Hispa-Pentium: CD y la carpeta de trabajo, PATH, archivos .BAT, la historia de la línea y Ctrl+C =="
I=$P/int.img; cp $P/d16.img $I
ESCRIBE=1 DISCO=$I $AQUI/nucleo32/probar.sh "" ordenes "cd textos" cd "dir" "type pirata.txt" "eco desde textos" "cd .." "cd textos\\.." cd "cd nada" "cd leeme.txt" \
  "md uno" "cd uno" "md dos" "cd dos" "echo hondo > aqui.txt" "type \\uno\\dos\\aqui.txt" "cd..\\.." "type uno\\dos\\aqui.txt" "rd uno\\dos" \
  "del uno\\dos\\aqui.txt" "rd uno\\dos" "rd uno" path "cd textos" "path ;" path "eco sin path" "eco2 con el de aquí" "path c:\\nada;c:\\" "eco con path" "cd \\" \
  "lote uno dos" :espera ":tecla spc" visto "eco primera" "eco segunda" ":tecla up up ret" ":tecla up left left left backspace x home delete z end y ret" \
  ":tecla e c o esc v e r ret" fecha salir apagar > $P/interprete.sal 2>&1
S=$P/interprete.sal
mira "CD: la carpeta de trabajo sale en la petición de órdenes, y las rutas se entienden desde ella («..» sube)" \
  "^C:.>cd textos$" "^C:.TEXTOS>cd$" "^C:.TEXTOS$" "^ Directorio de C:.TEXTOS$" "^un velero bergantín\.$" "^C:.TEXTOS>cd \.\.$" "^C:.>cd textos...$" \
  "^No puedo entrar en nada: no existe\.$" "^No puedo entrar en leeme.txt: no es una carpeta\.$"
mira "carpetas dentro de carpetas: se crean, se entra, se escribe en ellas, y no se borran con algo dentro" \
  "^C:.UNO.DOS>echo hondo > aqui.txt$" "^C:.UNO.DOS>cd.....$" "^C:.>type uno.dos.aqui.txt$" "^No puedo borrar la carpeta uno.dos: la carpeta no está vacía\.$" \
  "^C:.>rd uno$"
if [ "$(grep -c '^hondo$' $S)" = 2 ] && ! grep -q "^No puedo borrar la carpeta uno:" $S; then ok "y la carpeta de trabajo vale igual para crear que para leer (con la ruta entera se lee lo mismo)"
else ko "lo escrito en la carpeta de trabajo no se lee por su ruta entera"; fi
mira "la carpeta de trabajo la hereda el programa que se lanza, y el PATH dice dónde se buscan los programas" \
  "^PATH=C:.$" "^desde textos$" "^(C:.ECO.BIN ha acabado con el código 2)$" "^No hay PATH\.$" "^Orden o nombre de programa desconocido: eco (escriba AYUDA)\.$" \
  "^con el de aquí$" "^(ECO2.BIN ha acabado con el código 4)$" "^con path$"
mira "un archivo de órdenes (.BAT): sus líneas, ECHO, REM, PAUSE, sus argumentos (%1), programas y redirección dentro" \
  "^C:.>lote uno dos$" "^lote: empiezo con uno y dos (y el 100%)$" "^desde el lote$" "^(ECO.BIN ha acabado con el código 3)$" "^lote: una línea$" "^lote: y otra$" \
  "^Pulse una tecla para seguir \. \. \. $" "^lote: fin$"
if ! grep -q "^C:.>ECHO lote\|REM un archivo\|^C:.>PAUSE" $S && grep -q "^C:.>REM con ECHO activado se ve cada orden$" $S && grep -q "^C:.>ECO visto$" $S \
    && grep -q "^esta no se ve$" $S && ! grep -q "ECHO esta no se ve" $S; then ok "ECHO OFF y la @ callan las órdenes del archivo; sin ellos, se ven antes de hacerse"
else ko "ECHO OFF o la @ no callan lo que deben (o callan de más)"; grep -n "lote\|visto\|REM\|se ve" $S | sed 's/^/        /' | head; fi
# la historia: lo que queda en la pantalla, quitando lo que borra cada retroceso
python3 -I $AQUI/nucleo32/pantalla.py < $S > $P/historia.txt
if [ "$(grep -c '^C:.>eco primera$' $P/historia.txt)" = 2 ] && [ "$(grep -c '^primera$' $S)" = 2 ] && grep -q '^C:.>zco prixeray$' $P/historia.txt \
    && grep -q "^Orden o nombre de programa desconocido: zco " $S && grep -q '^C:.>ver$' $P/historia.txt && grep -q "^Hispa-Pentium 0\.3" $S; then
  ok "la historia de la línea: la flecha de arriba trae las órdenes de antes; se corrigen con las flechas, Retroceso, Supr, Inicio y Fin; Esc borra la línea"
else ko "la historia o la corrección de la línea no van:"; grep -n "primera\|zco\|ver" $P/historia.txt | sed 's/^/        /' | head; fi
mira "FECHA: la fecha y la hora del reloj del PC" "^Hoy es $(date +%d/%m/%Y) y son las [0-2][0-9]:[0-5][0-9]:[0-5][0-9]\.$"
if sano $I && ! mdir -i $I :: 2> /dev/null | grep -q '^UNO \|^LOTE *TXT'; then ok "y el disco queda sano, sin las carpetas ni los archivos de paso"
else ko "tras las pruebas del intérprete el disco no queda bien:"; sed 's/^/        /' $P/fsck.txt | head -8; fi
# Ctrl+C
cp $P/d16.img $I
ESCRIBE=1 DISCO=$I $AQUI/nucleo32/probar.sh "" ordenes bucle :espera ":tecla ctrl-c" "bucle duerme" ":tecla ctrl-c" "bucle tecla" ":tecla a ctrl-c" \
  sinfin :espera ":tecla ctrl-c" "bucle escribe" :espera ":tecla ctrl-c" "dir bucle.dat" ":tecla e c o ctrl-c" "echo sigo aquí" "prueba ruido Z > ruido.txt" ":tecla ctrl-c" \
  "lote a b" :espera ":tecla ctrl-c" salir tareas memoria disco "lanzar bucle.bin" :espera ":tecla ctrl-c" tareas apagar > $P/ctrlc.sal 2>&1
S=$P/ctrlc.sal
mira "Ctrl+C acaba el programa que corre: uno que da vueltas sin llamar al sistema, uno que duerme y uno que espera teclas" \
  "^bucle: doy vueltas sin fin" "^bucle: duermo sin fin" "^bucle: espero teclas sin fin" "^tecla 97$" "^C:.>bucle duerme$" "^C:.>bucle tecla$" "^C:.>sinfin$"
if [ "$(grep -c '^\^C$' $S)" -ge 7 ] && ! grep -q "ha acabado con el código 25[45])" $S; then ok "cada vez sale ^C y vuelve la petición de órdenes (sin más avisos)"
else ko "Ctrl+C no deja las cosas como debe: $(grep -c '^\^C$' $S) veces ^C"; fi
mira "Ctrl+C en un archivo de órdenes: acaba el programa y el archivo (no sigue con la línea siguiente); y en PAUSE" "^sinfin: lanzo BUCLE$" "^C:.>lote a b$" \
  "^Pulse una tecla para seguir"
if grep -q "no tenía que salir\|^lote: fin" $S; then ko "tras Ctrl+C, el archivo de órdenes ha seguido"; else ok "y lo que venía detrás en el archivo no se hace"; fi
mira "Ctrl+C sin programa en marcha: deja la línea que se estaba escribiendo, y el intérprete sigue" "^C:.>eco^C$" "^sigo aquí$"
t=$(sed -n 's/^bucle.dat es un archivo de \([0-9]*\) octetos\.$/\1/p' $S)
if [ -n "$t" ] && [ "$t" -gt 0 ] && [ $((t % 1000)) = 0 ] && sano $I && saca $I BUCLE.DAT $P/x.txt && [ "$(wc -c < $P/x.txt)" = "$t" ]; then
  ok "un programa cortado mientras escribía: su archivo queda cerrado, con lo que llevaba ($t octetos), y el disco sano"
else ko "el archivo del programa cortado no queda bien: «$t»"; sed 's/^/        /' $P/fsck.txt | head -8; fi
m1=$(grep "^ORDENES.BIN ha acabado" $S | awk '{print $10, $13}')
mira "al cortar, el núcleo lo recoge todo: ni archivos abiertos ni tareas de más; y Ctrl+C también desde la consola del núcleo" \
  "^ORDENES.BIN ha acabado con el código 0; marcos libres" "; archivos abiertos: 0$" "^montón: 1 bloques ocupados con 8192 octetos" \
  "^bucle.bin ha acabado con el código 254; marcos libres" "^\[código 5\]"
if [ -n "$m1" ] && [ "$(echo $m1 | awk '{print $1}')" = "$(echo $m1 | awk '{print $2}')" ] && [ "$(grep -c '^ *[0-9]*  [A-Za-z.-]*  *[a-z]*  *[03]  ' $S)" = 4 ]; then
  ok "y la memoria vuelve entera ($m1 marcos libres antes y después)"
else ko "tras Ctrl+C queda memoria o tareas sin recoger: «$m1»"; grep -A6 "^> tareas" $S | sed 's/^/        /'; fi

# ---- el teclado español y los nombres largos ----
echo "== Hispa-Pentium: el teclado español (acentos con tecla muerta, ñ, ç, ¿ ¡) y los nombres largos (VFAT) =="
cp $P/d16.img $I
ESCRIBE=1 DISCO=$I $AQUI/nucleo32/probar.sh "" ordenes "echo ¿Dónde está el pingüino? ¡Aquí, señor! Ñandú, cigüeña, garçon, Çédille; áéíóú àèò âêô É Ü ï > letras.txt" \
  "echo 1º 2ª [x] {y} @ # | \\ < ; : _ - + * = % & ( ) \" ' ^ \` ~ ! ? / . , \$ ÁÍÓÚ > signos.txt" "type letras.txt" teclado "teclado us" ":tecla e c h o spc bracket_left bracket_right semicolon apostrophe backslash slash ret" "teclado es" \
  ":tecla caps_lock" "echo bloqueo con ñ y ç" ":tecla caps_lock" "dir largos" "type largos\\cancion_del_pirata.txt" "type largos\\AÑO" \
  "copy largos\\cancion_del_pirata.txt cancion.txt" "cd largos" "ren cancion_del_pirata.txt pirata.txt" "del añonue~1.txt" "dir" salir teclas apagar > $P/teclado.sal 2>&1
S=$P/teclado.sal
bien=1
saca $I LETRAS.TXT $P/x.txt && printf '¿Dónde está el pingüino? ¡Aquí, señor! Ñandú, cigüeña, garçon, Çédille; áéíóú àèò âêô É Ü ï\r\n' | iconv -f UTF-8 -t CP437 | cmp -s - $P/x.txt || bien=0
saca $I SIGNOS.TXT $P/y.txt && printf '%s\r\n' "1º 2ª [x] {y} @ # | \\ < ; : _ - + * = % & ( ) \" ' ^ \` ~ ! ? / . , \$ AIOU" | iconv -f UTF-8 -t CP437 | cmp -s - $P/y.txt || bien=0
if [ $bien = 1 ]; then ok "el teclado español: ñ, ç, ¿ ¡, º ª, los acentos con su tecla muerta (´ \` ^ ¨), Alt Gr ([ ] { } @ # | \\ ~) y los signos, octeto a octeto en la página 437"
else ko "el teclado español no da las letras que debe:"; od -c $P/x.txt | head -8 | sed 's/^/        /'; od -c $P/y.txt | head -6 | sed 's/^/        /'; fi
mira "lo tecleado se ve en la pantalla; TECLADO cambia al de EE. UU. y vuelve; Bloq Mayús" "^¿Dónde está el pingüino? ¡Aquí, señor! Ñandú, cigüeña, garçon, Çédille; áéíóú àèò âêô É Ü ï$" \
  "^Teclado: español (ES)\.$" "^Teclado: de EE\. UU\. (US)\.$" "^\[\];'./$" "^BLOQUEO CON Ñ Y Ç$"
mira "los nombres largos (VFAT) se leen de las carpetas, con sus letras de fuera del ASCII, y se abre por ellos" \
  "^CANCIO~1 TXT  *57  [0-9/]*  [0-9:]*  cancion_del_pirata.txt$" "^AÑONUE~1 TXT  *18  [0-9/]*  [0-9:]*  Año nuevo.txt$" "^que es mi dios la libertad\.$" \
  "^No puedo abrir largos.AÑO: no existe\.$" "^ *1 archivo(s) copiado(s)\.$" "^PIRATA   TXT  *57  [0-9/]*  [0-9:]*$"
if sano $I && saca $I CANCION.TXT $P/x.txt && [ "$(wc -c < $P/x.txt)" = 57 ] && saca $I LARGOS/PIRATA.TXT $P/y.txt && cmp -s $P/x.txt $P/y.txt \
    && ! mdir -i $I ::LARGOS 2> /dev/null | grep -q -i 'cancion_del\|nuevo'; then
  ok "al cambiar el nombre o borrar un archivo con nombre largo, sus trozos se van con él (fsck.fat no ve nombres largos sueltos)"
else ko "los nombres largos no quedan bien al renombrar o borrar:"; sed 's/^/        /' $P/fsck.txt | head -8; mdir -i $I ::LARGOS 2>&1 | sed 's/^/        /'; fi
fi

echo "== $nok bien, $fail mal =="
exit $((fail > 0))
