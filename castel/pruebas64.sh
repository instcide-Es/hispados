# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# pruebas64.sh - la parte 23 de pruebas.sh (no se ejecuta sola: la lee
# pruebas.sh con «.», y usa sus ok, ko, $T, $AQUI, $USU y $RED_USU).
#  23. CASTEL64 (CASTEL64.md): Castel para x86-64. En Linux, en este mismo
#      ordenador (sin emular nada): los ejemplos de 16 bits sin DOS, como
#      programas de Linux de 64 bits, escriben lo mismo; los de ejemplos64
#      (nat64, ent64, punteros de 8 octetos, ensamblador con r8-r15...) dan
#      su salida esperada; cuentas al azar con las reglas de 64 bits.
#      ensambla.py da los mismos octetos que NASM con ensamblador/formas64.asm
#      y con todo lo de 64 bits. CASTEL64 (el compilador escrito en Castel,
#      compilado para Linux x86-64) saca el mismo ensamblador que ccastel.py en
#      todos los programas de las pruebas, de 16, 32 y 64 bits (el núcleo de
#      Hispa-Pentium con sus programas, CASTEL, CASTEL32, él mismo...), y dice
#      los mismos errores; ENSAMB64 da los mismos octetos que NASM en todos; y
#      el arranque en tres pasos: ccastel.py -> CASTEL64 (paso 1) -> paso 2 ->
#      paso 3, idénticos. Y el primer ladrillo de Hispa-64 (nucleo64/hola.cas):
#      arranca en QEMU x86_64 (multiboot, y multiboot2 con GRUB si está
#      grub-mkrescue), pasa al modo largo y saluda por el puerto serie. Y el
#      núcleo de Hispa-64 (nucleo64/hispa64.cas, «modelo hispa64», en la mitad
#      alta desde la etapa 1): sus pruebas (nucleo64/pruebas.sh), por multiboot
#      y por UEFI (OVMF + GRUB), con la memoria (física, virtual, el montón).

parte23() {
echo "== CASTEL64: Castel para x86-64 (Linux de 64 bits y el primer ladrillo de Hispa-64) =="
local W=$T/c64 f n N bien mal t0 t1 e c
mkdir -p $W/ref $W/l $W/p
up() { basename $1 .cas | tr a-z A-Z | cut -c1-8; }
tiempo() { echo "$(echo "$2 - $1" | bc | sed 's/^\./0./' | cut -c1-5) s"; }
# paso 1: ccastel.py y NASM hacen CASTEL64 y ENSAMB64
t0=$(date +%s.%N)
if python3 -I $AQUI/ccastel.py $AQUI/compilador/64/linux/castel64.cas -o $W/castel64.asm -I $AQUI/compilador/64 \
     -I $AQUI/compilador/32 -I $AQUI/biblioteca64/linux -I $AQUI/compilador > /dev/null 2>&1 && nasm -f bin $W/castel64.asm -o $W/castel64 \
   && python3 -I $AQUI/ccastel.py $AQUI/ensamblador/64/linux/ensamb64.cas -o $W/ensamb64.asm -I $AQUI/biblioteca64/linux \
     -I $AQUI/ensamblador > /dev/null 2>&1 && nasm -f bin $W/ensamb64.asm -o $W/ensamb64; then
  chmod +x $W/castel64 $W/ensamb64
  ok "paso 1: ccastel.py y NASM hacen CASTEL64 ($(stat -c %s $W/castel64) octetos) y ENSAMB64 ($(stat -c %s $W/ensamb64)), programas de Linux x86-64"
else
  ko "CASTEL64 no se puede hacer con ccastel.py y NASM"; return
fi

# ---- los programas de 64 bits, ejecutados en Linux ----
# los ejemplos de 16 bits que no tocan el DOS, como programas de linux64
MISMOS="arreglos bucles cuenta estruct mezcla nivel pares pruebas refs saltos saludo textos modulos"
bien=0; mal=
for n in $MISMOS; do
  python3 - $AQUI/ejemplos/$n.cas $W/p/$n.cas <<'PY'
import sys, re
s = open(sys.argv[1], encoding='utf-8').read().split('\n')
for i, l in enumerate(s):
    if re.match(r'^módulo \w+', l):
        s.insert(i + 1, '    modelo linux64')
        break
open(sys.argv[2], 'w', encoding='utf-8').write('\n'.join(s))
PY
  if ! $AQUI/compilar.sh $W/p/$n.cas $W/p/$n -I $AQUI/ejemplos/partes > $W/comp.txt 2>&1; then mal="$mal $n(no compila)"; continue; fi
  timeout 10 $W/p/$n > $W/p/$n.raw 2>&1; c=$?
  iconv -f CP437 -t UTF-8 < $W/p/$n.raw > $W/p/$n.sal
  e=0; [ $n = nivel ] && e=7
  if cmp -s $W/p/$n.sal $AQUI/ejemplos/esperado/$n.txt && [ $c = $e ]; then bien=$((bien + 1)); else mal="$mal $n"; fi
done
if [ -z "$mal" ]; then ok "los $bien ejemplos de 16 bits sin DOS, como programas de Linux de 64 bits: escriben lo mismo (y nivel acaba con 7)"
else ko "como programas de linux64, no escriben lo mismo:$mal"; fi
# los de ejemplos64, con su salida esperada y su código de salida
for f in $AQUI/ejemplos64/*.cas; do
  n=$(basename $f .cas)
  grep -q '^ *modelo linux64' $f || continue
  if ! $AQUI/compilar.sh $f $W/p/$n -I $AQUI/biblioteca64/linux > $W/comp.txt 2>&1; then ko "$n.cas no compila: $(head -1 $W/comp.txt)"; continue; fi
  timeout 10 $W/p/$n > $W/p/$n.raw 2>&1; c=$?
  iconv -f CP437 -t UTF-8 < $W/p/$n.raw > $W/p/$n.sal
  e=$(sed -n 's/^; código de salida: //p' $f); e=${e:-0}
  if cmp -s $W/p/$n.sal $AQUI/ejemplos64/esperado/$n.txt && [ "$c" = "$e" ]; then
    ok "$n ($(stat -c %s $W/p/$n) octetos): la salida es la esperada y acaba con el código que toca ($c)"
  else
    ko "$n: la salida no es la esperada o acaba con otro código ($c y no $e; < esperada, > la que sale):"
    diff $AQUI/ejemplos64/esperado/$n.txt $W/p/$n.sal | head -8 | sed 's/^/        /'
  fi
done
# cuentas al azar con las reglas de 64 bits (lo que da Python frente a lo que sale)
python3 -I $AQUI/azar64.py 9 60 $W/azar64.cas $W/azar64.esp
if $AQUI/compilar.sh $W/azar64.cas $W/p/azar64 > $W/comp.txt 2>&1 && timeout 10 $W/p/azar64 > $W/p/azar64.sal 2>&1 \
   && cmp -s $W/azar64.esp $W/p/azar64.sal; then
  ok "cuentas al azar de 64 bits: $(wc -l < $W/azar64.esp) resultados, todos bien"
else ko "cuentas al azar de 64 bits: hay cuentas que no salen (< esperado, > lo que sale):"; diff $W/azar64.esp $W/p/azar64.sal | head -6 | sed 's/^/        /'; fi

# ---- el árbol de los fuentes, con los nombres del DOS (8+3), como en la parte 15 ----
F=$W/f
mkdir -p $F/BIB $F/BIB32 $F/PARTES $F/NUC $F/C32 $F/B32P $F/C64 $F/B64 $F/COMP $F/ENS $F/E32 $F/E64 $F/S $F/ERR $F/EJ64
for f in $AQUI/biblioteca/*.cas; do cp $f $F/BIB/$(up $f).CAS; done
for f in $AQUI/biblioteca32/*.cas; do cp $f $F/BIB32/$(up $f).CAS; done
for f in $AQUI/ejemplos/partes/*.cas; do cp $f $F/PARTES/$(up $f).CAS; done
for f in $AQUI/nucleo32/*.cas; do cp $f $F/NUC/$(up $f).CAS; done
for f in $AQUI/biblioteca32/pentium/*.cas; do cp $f $F/B32P/$(up $f).CAS; done
for f in $AQUI/biblioteca64/linux/*.cas; do cp $f $F/B64/$(up $f).CAS; done
for f in $AQUI/compilador/*.cas; do cp $f $F/COMP/$(up $f).CAS; done
for f in $AQUI/ejemplos64/*.cas; do cp $f $F/EJ64/$(up $f).CAS; done
cp $AQUI/ensamblador/montador.cas $F/ENS/MONTADOR.CAS
cp $AQUI/compilador/32/pentium/castel32.cas $F/C32/CASTEL32.CAS
cp $AQUI/compilador/32/medidas.cas $F/C32/MEDIDAS.CAS
cp $AQUI/compilador/64/linux/castel64.cas $F/C64/CASTEL64.CAS
cp $AQUI/compilador/64/medidas.cas $F/C64/MEDIDAS.CAS
for d in C32 C64; do cp $AQUI/compilador/32/gen64.cas $F/$d/GEN64.CAS; cp $AQUI/compilador/32/apoyo64.cas $F/$d/APOYO64.CAS; done
cp $AQUI/ensamblador/32/pentium/ensamb32.cas $F/E32/ENSAMB32.CAS
cp $AQUI/ensamblador/64/linux/ensamb64.cas $F/E64/ENSAMB64.CAS
python3 -I $AQUI/azar.py 7 45 $W/azar16.cas $W/azar16.esp
python3 -I $AQUI/azar32.py 7 28 $W/azar32.cas $W/azar32.esp
python3 -I $AQUI/azar32.py 8 40 $W/azar32p.cas $W/azar32p.esp plano
printf 'm\xc3\xb3dulo Toda\n    importar Textos, Archivos, Memoria, Argumentos, Lejos\n    funci\xc3\xb3n principal()\n    fin\nfin Toda.\n' > $W/toda.cas
cp $AQUI/ensamblador/montador.cas $F/MONTADOR.CAS
RAIZ="$(echo $AQUI/ejemplos/*.cas $AQUI/ejemplos32/*.cas) $AQUI/ensamblador/ensambla.cas $W/toda.cas $W/azar16.cas $W/azar32.cas $W/azar32p.cas"
for f in $RAIZ; do cp $f $F/$(up $f).CAS; done
cp $W/azar64.cas $F/EJ64/AZAR64.CAS
NUCS="$AQUI/nucleo32/semilla.cas $USU $RED_USU"
# ---- CASTEL64 frente a ccastel.py: todos los programas de las pruebas ----
t0=$(date +%s.%N)
bien=0; mal=
for f in $RAIZ; do
  N=$(up $f); todo=; [ $N = TODA ] && todo=--todo
  python3 -I $AQUI/ccastel.py $todo $f -o $W/ref/$N.ASM -I $AQUI/ejemplos/partes > /dev/null 2>&1
  (cd $F && $W/castel64 $N.CAS -o $N.ASM -I BIB -I PARTES -I BIB32 $todo > $N.SAL 2>&1)
  if cmp -s $W/ref/$N.ASM $F/$N.ASM; then bien=$((bien + 1)); else mal="$mal $N"; fi
done
for f in $NUCS; do
  N=$(up $f)
  python3 -I $AQUI/ccastel.py $f -o $W/ref/$N.ASM > /dev/null 2>&1
  (cd $F/NUC && $W/castel64 $N.CAS -o ../S/$N.ASM -I ../BIB32 > $F/S/$N.SAL 2>&1)
  if cmp -s $W/ref/$N.ASM $F/S/$N.ASM; then bien=$((bien + 1)); else mal="$mal $N"; fi
done
for f in $AQUI/ejemplos64/*.cas $W/azar64.cas; do
  N=$(up $f)
  python3 -I $AQUI/ccastel.py $f -o $W/ref/$N.ASM -I $AQUI/biblioteca64/linux > /dev/null 2>&1
  (cd $F/EJ64 && $W/castel64 $N.CAS -o ../S/$N.ASM -I ../B64 -I ../BIB > $F/S/$N.SAL 2>&1)
  if cmp -s $W/ref/$N.ASM $F/S/$N.ASM; then bien=$((bien + 1)); else mal="$mal $N"; fi
done
# y los compiladores y ensambladores: CASTEL (16), CASTEL32, ENSAMB32, CASTEL64 y ENSAMB64
python3 -I $AQUI/ccastel.py $AQUI/compilador/castel.cas -o $W/ref/CASTEL.ASM > /dev/null 2>&1
(cd $F/COMP && $W/castel64 CASTEL.CAS -o ../S/CASTEL.ASM -I ../BIB > $F/S/CASTEL.SAL 2>&1)
if cmp -s $W/ref/CASTEL.ASM $F/S/CASTEL.ASM; then bien=$((bien + 1)); else mal="$mal CASTEL"; fi
python3 -I $AQUI/ccastel.py $AQUI/compilador/32/pentium/castel32.cas -o $W/ref/CASTEL32.ASM -I $AQUI/compilador/32 \
  -I $AQUI/biblioteca32/pentium -I $AQUI/compilador -I $AQUI/nucleo32 > /dev/null 2>&1
python3 -I $AQUI/ccastel.py $AQUI/ensamblador/32/pentium/ensamb32.cas -o $W/ref/ENSAMB32.ASM -I $AQUI/biblioteca32/pentium \
  -I $AQUI/ensamblador -I $AQUI/nucleo32 > /dev/null 2>&1
cp $W/castel64.asm $W/ref/CASTEL64.ASM; cp $W/ensamb64.asm $W/ref/ENSAMB64.ASM
for x in CASTEL32:C32:"-I B32P -I COMP -I NUC -I BIB" ENSAMB32:E32:"-I B32P -I ENS -I NUC -I BIB" \
         CASTEL64:C64:"-I B64 -I COMP -I BIB" ENSAMB64:E64:"-I B64 -I ENS -I BIB"; do
  X=${x%%:*}; d=${x#*:}; i=${d#*:}; d=${d%%:*}
  (cd $F && $W/castel64 $d/$X.CAS -o S/$X.ASM $i > S/$X.SAL 2>&1)
  if cmp -s $W/ref/$X.ASM $F/S/$X.ASM; then bien=$((bien + 1)); else mal="$mal $X"; fi
done
t1=$(date +%s.%N)
if [ -z "$mal" ]; then ok "CASTEL64 saca el mismo ensamblador que ccastel.py en los $bien programas (ejemplos de 16, 32 y 64 bits, biblioteca entera, ENSAMBLA, cuentas al azar, el núcleo de Hispa-Pentium y sus programas, CASTEL, CASTEL32, ENSAMB32, CASTEL64 y ENSAMB64; $(tiempo $t0 $t1) en todo)"
else ko "CASTEL64 no saca lo mismo que ccastel.py en:$mal"; fi
# CASTEL32 (el de Hispa-Pentium, aquí con corre32) también saca 64 bits: lo mismo
# que ccastel.py en los programas de 64 bits y el núcleo de Hispa-64
if command -v gcc > /dev/null && python3 -I $AQUI/ccastel.py $AQUI/compilador/32/pentium/castel32.cas -o $W/castel32.asm \
     -I $AQUI/compilador/32 -I $AQUI/biblioteca32/pentium -I $AQUI/compilador -I $AQUI/nucleo32 > /dev/null 2>&1 \
   && nasm -f bin $W/castel32.asm -o $W/castel32.bin 2> /dev/null; then
  for f in $AQUI/nucleo64/*.cas; do cp $f $F/EJ64/$(up $f).CAS; done
  python3 -I $AQUI/ccastel.py $AQUI/nucleo64/hola.cas -o $W/ref/HOLA.ASM > /dev/null 2>&1
  python3 -I $AQUI/ccastel.py $AQUI/nucleo64/hispa64.cas -o $W/ref/HISPA64.ASM -I $AQUI/nucleo32 > /dev/null 2>&1
  bien=0; mal=
  for f in $AQUI/ejemplos64/*.cas $W/azar64.cas $AQUI/nucleo64/hola.cas $AQUI/nucleo64/hispa64.cas; do
    N=$(up $f)
    (cd $F/EJ64 && $AQUI/corre32/corre32.sh -C $F $W/castel32.bin $N.CAS -o \\S\\$N.A32 -I \\B64 -I \\BIB -I \\NUC > $F/S/$N.S32 2>&1)
    if cmp -s $W/ref/$N.ASM $F/S/$N.A32; then bien=$((bien + 1)); else mal="$mal $N"; fi
  done
  if [ -z "$mal" ]; then ok "CASTEL32 (el de Hispa-Pentium, con corre32) también saca 64 bits: lo mismo que ccastel.py en los $bien programas de 64 bits (Hispa-64 incluido)"
  else ko "CASTEL32 no saca lo mismo que ccastel.py en los de 64 bits:$mal"; fi
fi
# los errores: los mismos mensajes, con su línea y su columna
bien=0; mal=
for f in $AQUI/ejemplos/errores/*.cas $AQUI/ejemplos32/errores/*.cas $AQUI/ejemplos64/errores/*.cas; do
  [ -f $f ] || continue
  n=$(basename $f .cas)
  esperado=$(sed -n 's/^; ERROR: //p' $f | head -1)
  cp $f $F/ERR/E.CAS
  b64=; case $f in */ejemplos64/*) b64="-I ../B64" ;; esac
  dicho=$(cd $F/ERR && $W/castel64 E.CAS $b64 -I ../BIB -I ../PARTES 2>&1 | tr -d '\r' | iconv -f CP437 -t UTF-8 | head -1 | sed 's/^.*\.CAS://')
  if [ "$dicho" = "$esperado" ]; then bien=$((bien + 1)); else mal="$mal $n"; fi
done
if [ -z "$mal" ]; then ok "CASTEL64 dice los $bien errores igual que ccastel.py (con su línea y su columna)"
else ko "CASTEL64 no dice igual los errores de:$mal"; fi
# ensambla.py y ENSAMB64 frente a NASM, con todo lo anterior y las formas de 64 bits
cp $AQUI/ensamblador/formas64.asm $W/ref/FORMAS64.ASM
bien=0; mal=; bienp=0; malp=
for f in $W/ref/*.ASM; do
  N=$(basename $f .ASM)
  nasm -f bin $f -o $W/l/$N.NASM 2> /dev/null || continue
  # (ensambla.py, en Python, es lento: los de 64 bits de menos de 10.000 líneas; los grandes, ENSAMB64)
  if grep -qE '^(bits 64|cpu x64)' $f && [ $(wc -l < $f) -lt 10000 ]; then
    python3 -I $AQUI/ensambla.py $f -o $W/l/$N.PY > /dev/null 2>&1
    if cmp -s $W/l/$N.NASM $W/l/$N.PY; then bienp=$((bienp + 1)); else malp="$malp $N"; fi
  fi
  (cd $W/l && $W/ensamb64 ../ref/$N.ASM $N.BIN > $N.SAL 2>&1)
  if cmp -s $W/l/$N.NASM $W/l/$N.BIN; then bien=$((bien + 1)); else mal="$mal $N"; fi
done
if [ -z "$malp" ]; then ok "ensambla.py da los mismos octetos que NASM en los $bienp archivos de 64 bits (las formas de x86-64, los ejemplos, las cuentas al azar y ENSAMB64)"
else ko "ensambla.py no da lo mismo que NASM en:$malp"; fi
if [ -z "$mal" ]; then ok "ENSAMB64 da los mismos octetos que NASM en los $bien archivos (de 16, 32 y 64 bits)"
else ko "ENSAMB64 no da lo mismo que NASM en:$mal"; fi
# el arranque en tres pasos, en Linux
declare -A dura=()
(cd $F && for g in 2 3; do
   c=$W/castel64; e=$W/ensamb64
   [ $g = 3 ] && { c=$F/C2; e=$F/E2; }
   t0=$(date +%s.%N)
   $c C64/CASTEL64.CAS -o C$g.ASM -I B64 -I COMP -I BIB > P$g.SAL 2>&1
   t1=$(date +%s.%N)
   $e C$g.ASM C$g > R$g.SAL 2>&1
   t2=$(date +%s.%N)
   $c E64/ENSAMB64.CAS -o E$g.ASM -I B64 -I ENS -I BIB > Q$g.SAL 2>&1
   $e E$g.ASM E$g > S$g.SAL 2>&1
   chmod +x C$g E$g 2> /dev/null
   echo "$(tiempo $t0 $t1) $(tiempo $t1 $t2)" > T$g.TXT
 done)
if cmp -s $W/castel64.asm $F/C2.ASM && cmp -s $W/castel64 $F/C2 && cmp -s $W/ensamb64.asm $F/E2.ASM && cmp -s $W/ensamb64 $F/E2; then
  ok "paso 2: CASTEL64 se compila a sí mismo ($(cut -d' ' -f1-2 $F/T2.TXT)) y ENSAMB64 lo ensambla ($(cut -d' ' -f3-4 $F/T2.TXT)): lo mismo que el paso 1 con ccastel.py y NASM ($(wc -l < $F/C2.ASM) líneas, $(stat -c %s $F/C2) octetos)"
else ko "paso 2: CASTEL64 compilándose a sí mismo no da lo del paso 1: $(tr -d '\r' < $F/P2.SAL 2> /dev/null | head -1)"; fi
if [ -s $F/C3.ASM ] && cmp -s $F/C2.ASM $F/C3.ASM && cmp -s $F/C2 $F/C3 && cmp -s $F/E2 $F/E3; then
  ok "paso 3: el CASTEL64 del paso 2 se compila otra vez y sale idéntico, octeto a octeto (el punto fijo)"
else ko "paso 3: CASTEL64 no llega al punto fijo"; fi

# ---- el primer ladrillo de Hispa-64: arranca en QEMU x86_64 y saluda ----
if command -v qemu-system-x86_64 > /dev/null; then
  if python3 -I $AQUI/ccastel.py $AQUI/nucleo64/hola.cas -o $W/hola64.asm > $W/comp.txt 2>&1 && nasm -f bin $W/hola64.asm -o $W/NUCLEO64.BIN 2>> $W/comp.txt \
     && (cd $AQUI/nucleo64 && $W/castel64 hola.cas -o $W/hola64c.asm > /dev/null 2>&1) && cmp -s $W/hola64.asm $W/hola64c.asm \
     && $W/ensamb64 $W/hola64.asm $W/NUCLEO64.ENS > /dev/null 2>&1 && cmp -s $W/NUCLEO64.BIN $W/NUCLEO64.ENS; then
    rm -f $W/serie.txt
    timeout 20 qemu-system-x86_64 -kernel $W/NUCLEO64.BIN -display none -serial file:$W/serie.txt -monitor none \
      -device isa-debug-exit,iobase=0xf4,iosize=0x04 -no-reboot -nic none -m 64 > /dev/null 2>&1; c=$?
    if grep -q "Hola desde Hispa-64" $W/serie.txt 2> /dev/null && grep -q "modo largo" $W/serie.txt && [ $c = $(( (42 << 1) | 1 )) ]; then
      ok "Hispa-64 (nucleo64/hola.cas, $(stat -c %s $W/NUCLEO64.BIN) octetos, igual con CASTEL64 y ENSAMB64): arranca en QEMU x86_64 por multiboot, pasa al modo largo y dice «$(grep -a Hola $W/serie.txt | head -1 | tr -d '\r')»"
    else ko "Hispa-64 no saluda por el puerto serie (código $c): $(tr -d '\r' < $W/serie.txt 2> /dev/null | head -3 | paste -sd ' ')"; fi
    if command -v grub-mkrescue > /dev/null && [ -d /usr/lib/grub/i386-pc ]; then
      mkdir -p $W/iso/boot/grub
      cp $W/NUCLEO64.BIN $W/iso/boot/
      printf 'set timeout=0\nset default=0\nserial --unit=0 --speed=115200\nterminal_output serial\nmenuentry "Hispa-64" {\n  multiboot2 /boot/NUCLEO64.BIN\n  boot\n}\n' > $W/iso/boot/grub/grub.cfg
      if grub-mkrescue -o $W/h64.iso $W/iso > /dev/null 2>&1; then
        rm -f $W/serie2.txt
        timeout 40 qemu-system-x86_64 -cdrom $W/h64.iso -display none -serial file:$W/serie2.txt -monitor none \
          -device isa-debug-exit,iobase=0xf4,iosize=0x04 -no-reboot -nic none -m 64 > /dev/null 2>&1; c=$?
        if grep -q "Hola desde Hispa-64" $W/serie2.txt 2> /dev/null && grep -q "multiboot2" $W/serie2.txt; then
          ok "Hispa-64 arranca también con GRUB por multiboot2 (desde un CD): «$(grep -a 'cargado por' $W/serie2.txt | head -1 | tr -d '\r')»"
        else ko "Hispa-64 con GRUB (multiboot2) no saluda: $(tr -d '\r' < $W/serie2.txt 2> /dev/null | tail -3 | paste -sd ' ')"; fi
      fi
    fi
  else ko "nucleo64/hola.cas no se compila igual con ccastel.py y NASM que con CASTEL64 y ENSAMB64: $(head -2 $W/comp.txt | paste -sd ' ')"; fi
  # [HISPA-64] el núcleo de verdad, etapas 0 a 3 (docs/64/HISPA-64.md): sus pruebas,
  # con CASTEL64 y ENSAMB64 (que tienen que sacar lo mismo que ccastel.py y NASM)
  if CASTEL64=$W/castel64 ENSAMB64=$W/ensamb64 $AQUI/nucleo64/pruebas.sh $W/h64 > $W/h64.txt 2>&1; then
    ok "Hispa-64, etapas 0 a 4 (nucleo64/pruebas.sh): $(tail -1 $W/h64.txt | sed 's/^== Hispa-64: //; s/ ==$//') (en la mitad alta; multiboot con 128 MB, 3 GB y 8 GB; la memoria física, las páginas, el montón; excepciones, doble fallo y NMI con IST; UEFI con OVMF y GRUB; ACPI, el tic del APIC, el IOAPIC, el teclado, el botón y PCI con pc y q35, con y sin HPET; 1, 2, 4 y 8 procesadores, los hilos y el planificador, la TLB de los demás y los pánicos en otro procesador; los programas en el anillo 3 (ELF64, syscall, los fallos explicados) y el CD; igual con CASTEL64 y ENSAMB64)"
  else ko "Hispa-64, etapas 0 a 4 (nucleo64/pruebas.sh): $(grep -a 'MAL:' $W/h64.txt | head -3 | paste -sd ' ')"; fi
fi
}
