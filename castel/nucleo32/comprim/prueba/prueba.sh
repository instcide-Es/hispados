#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo32/comprim/prueba/prueba.sh [VUELTAS_DE_FUZZ]
# Las pruebas en el anfitrión del motor de Comprimidos (zip.c, con ASan y
# UBSan: anfitrion.sh): lo que hace el zip de Linux lo saca el nuestro, y lo
# que hace el nuestro lo saca el unzip de Linux, comparado octeto a octeto
# (diff -r); ZipCrypto (zip -P), el AES de WinZip, ZIP64, los nombres en
# UTF-8 y en la página 437, añadir y quitar; los ZIP malos (rutas con «..»,
# absolutas, demasiado hondas, la bomba solapada, el tope de lo
# descomprimido, el CRC, lo que miente) y el fuzz (fuzz.py). Dice OK o FAIL
# de cada cosa; acaba con el número de fallos. Necesita zip, unzip y
# python3 con «cryptography» (para el AES).
set -u
AQUI=$(dirname "$(readlink -f "$0")")
W=$(mktemp -d)
trap 'rm -rf $W' EXIT
fail=0; nok=0
ok() { echo "  OK   $1"; nok=$((nok + 1)); }
ko() { echo "  FAIL $1"; fail=$((fail + 1)); }
A=$W/anfitrion
export ASAN_OPTIONS=detect_leaks=1
bash $AQUI/../anfitrion.sh $A 1 > $W/hacer.txt 2>&1 || { echo "  FAIL no compila: $(tail -3 $W/hacer.txt)"; exit 1; }
cd $W
# lo de siempre: carpetas (una vacía), un archivo vacío, uno de azar (no se
# comprime), texto (sí) y nombres con letras de aquí
mkdir -p src/sub/hondo src/vacia
head -c 300000 /dev/urandom > src/azar.bin
seq 1 60000 > src/numeros.txt
printf 'hola\r\n' > "src/sub/Informe largo ñandú.txt"
: > src/cero.txt
cp src/numeros.txt src/sub/hondo/otra.txt
# 1. el zip de Linux, sacado con el nuestro
(cd src && zip -qr ../linux.zip .)
$A unzip -q -d sale1 linux.zip > s1.txt 2>&1
if [ $? = 0 ] && diff -r src sale1 > /dev/null; then ok "un ZIP del zip de Linux, sacado con el nuestro: igual octeto a octeto (con la carpeta vacía y el archivo vacío)"
else ko "el ZIP de Linux no sale igual:"; diff -r src sale1 | head -5; cat s1.txt | head -5; fi
# 2. el nuestro, sacado con el unzip de Linux
$A zip -q nuestro.zip src > s2.txt 2>&1
if [ $? = 0 ] && unzip -qq -d sale2 nuestro.zip && diff -r src sale2/src > /dev/null; then ok "un ZIP del nuestro, sacado con el unzip de Linux: igual octeto a octeto (los nombres en UTF-8 también)"
else ko "nuestro ZIP no lo saca bien el unzip de Linux:"; diff -r src sale2/src 2>&1 | head -5; cat s2.txt | head; fi
if unzip -tq nuestro.zip | grep -q "^No errors detected"; then ok "unzip -t de Linux: sin errores"; else ko "unzip -t de Linux da errores"; unzip -t nuestro.zip | tail -3; fi
if zipinfo nuestro.zip | grep -q "defN.*src/numeros.txt" && zipinfo nuestro.zip | grep -q "stor.*src/azar.bin"; then ok "el texto va comprimido (deflate) y lo de azar, guardado (no se gana nada)"
else ko "los métodos no son los esperados:"; zipinfo nuestro.zip | head; fi
# y otra vez, con -0 y -9
$A zip -q -0 guardado.zip src > /dev/null && $A zip -q -9 mucho.zip src > /dev/null
if unzip -tq guardado.zip > /dev/null && unzip -tq mucho.zip > /dev/null && ! zipinfo guardado.zip | grep -q defN \
   && [ "$(stat -c %s mucho.zip)" -le "$(stat -c %s nuestro.zip)" ]; then ok "-0 (solo guardar) y -9 (lo más): los dos los lee Linux, y -9 no ocupa más"
else ko "-0 o -9 no van bien"; fi
# 3. ZipCrypto
(cd src && zip -qr -P "secreta ñ" ../zc.zip .)
$A unzip -q -P "secreta ñ" -d sale3 zc.zip > /dev/null 2>&1
if diff -r src sale3 > /dev/null; then ok "ZipCrypto (zip -P de Linux): con la contraseña, igual octeto a octeto"; else ko "ZipCrypto no sale bien"; fi
$A unzip -q -P mala -d sale3b zc.zip > s3.txt 2>&1
r=$?
if [ $r = 82 ] && grep -q "contraseña" s3.txt && [ -z "$(find sale3b -type f 2>/dev/null)" ]; then ok "con la contraseña mala: lo dice, y no deja nada a medias"
else ko "con la contraseña mala no se porta bien (código $r):"; head -3 s3.txt; find sale3b -type f | head -3; fi
$A unzip -q -d sale3c zc.zip > s3c.txt 2>&1
[ $? = 82 ] && grep -q "hace falta la contraseña" s3c.txt && ok "sin contraseña: «hace falta la contraseña»" || ko "sin contraseña: $(head -1 s3c.txt)"
# 4. los casos hechos a mano
python3 -I $AQUI/haz_casos.py casos > /dev/null 2>&1 || ko "haz_casos.py no va (¿python3-cryptography?)"
for c in aes256 aes128 aes192g; do
  rm -rf sa; $A unzip -q -P "clave buena" -d sa casos/$c.zip > /dev/null 2>&1
  [ "$(md5sum < sa/quijote.txt 2>/dev/null)" = "$(python3 -c "import sys;sys.stdout.buffer.write(b'En un lugar de la Mancha, de cuyo nombre no quiero acordarme.\n'*400)" | md5sum)" ] && aes=si || aes=no
  [ $aes = si ] && ok "AES de WinZip ($c): sale bien" || ko "AES de WinZip ($c) no sale"
done
$A unzip -t -P "otra" casos/aes256.zip > s4.txt 2>&1; grep -q "quijote.txt.*contraseña" s4.txt && ok "AES con la contraseña mala: lo dice (su verificador)" || ko "AES con la contraseña mala: $(tail -1 s4.txt)"
$A unzip -t -P "clave buena" casos/aestocado.zip > s4.txt 2>&1; grep -q "no dan su CRC" s4.txt && ok "AES con los datos tocados: lo descubre su firma (HMAC-SHA1)" || ko "AES tocado: $(tail -2 s4.txt)"
$A unzip -l casos/cp437.zip > s5.txt; grep -q "CAÑON/NIÑO Ü.TXT" s5.txt && ok "nombres en la página 437 (de MS-DOS, sin la marca de UTF-8): se leen bien" || ko "la página 437: $(grep TXT s5.txt)"
$A unzip -l casos/utf8.zip > s5.txt; grep -q "Canción de cuna/Año 2026 E.txt" s5.txt && ok "nombres en UTF-8 (con la marca): se leen bien (el € sale como E en la página 437)" || ko "UTF-8: $(grep txt s5.txt)"
$A unzip -q -d sa64 casos/zip64.zip > /dev/null 2>&1 && [ "$(stat -c %s sa64/grande.txt)" = "$(python3 -c "print(len(b'En un lugar de la Mancha, de cuyo nombre no quiero acordarme.\n'*400))")" ] \
  && ok "ZIP64 (el de zipfile, a la fuerza): se lee y se saca" || ko "ZIP64 no va"
# un ZIP64 de los nuestros: más de 65535 miembros no caben, pero sí lo
# grande; con un archivo de 4 GB y pico (disperso) que comprime en poco
if [ -z "${SIN_GRANDE:-}" ]; then
  mkdir -p g && truncate -s 4300M g/grande.bin && echo fin >> g/grande.bin
  if $A zip -q -1 g64.zip g > /dev/null 2>&1 && unzip -tq g64.zip | grep -q "^No errors" && zipinfo g64.zip | grep -q "4508876804"; then
    ok "un ZIP64 de los nuestros (un archivo de 4,2 GB, disperso): lo comprueba unzip -t de Linux"
  else ko "el ZIP64 nuestro no va: $(unzip -tq g64.zip 2>&1 | tail -1)"; fi
  rm -rf g g64.zip
fi
# 5. añadir y quitar
cp nuestro.zip mod.zip
echo nuevo > nuevo.txt
$A zip -q mod.zip nuevo.txt > /dev/null && $A zip -q -d mod.zip "src/sub/*" > /dev/null
if unzip -tq mod.zip > /dev/null && unzip -l mod.zip | grep -q " nuevo.txt" && ! unzip -l mod.zip | grep -q "src/sub/" && unzip -l mod.zip | grep -q "src/numeros.txt"; then
  ok "añadir (nuevo.txt) y quitar (src/sub/*): unzip -t lo da por bueno y está lo que tiene que estar"
else ko "añadir y quitar no va:"; unzip -l mod.zip | tail -8; fi
echo otro > nuevo.txt; $A zip -q mod.zip nuevo.txt > /dev/null
[ "$(unzip -p mod.zip nuevo.txt)" = otro ] && [ "$(unzip -l mod.zip | grep -c ' nuevo.txt')" = 1 ] && ok "volver a meter uno que ya está: se pone al día (no sale dos veces)" || ko "poner al día: $(unzip -l mod.zip | grep nuevo)"
cp casos/aes256.zip aesmod.zip; $A zip -q aesmod.zip nuevo.txt > /dev/null
rm -rf sa; $A unzip -q -P "clave buena" -d sa aesmod.zip > /dev/null 2>&1; [ -s sa/quijote.txt ] && [ "$(cat sa/nuevo.txt)" = otro ] \
  && ok "añadir a un ZIP con AES: lo cifrado se copia tal cual (sin la contraseña) y se sigue leyendo" || ko "añadir a uno con AES lo estropea"
# 6. los malos
for c in subir absoluto hondo; do
  rm -rf mal; mkdir mal; (cd mal && $A unzip -q ../casos/$c.zip > ../s6.txt 2>&1)
  if [ -f mal/bueno.txt ] && [ ! -e fuera.txt ] && [ ! -e /etc/fuera.txt ] && [ "$(find mal -name 'fuera.txt' -o -name x.txt | wc -l)" = 0 ] && grep -q "no se han podido sacar" s6.txt; then
    ok "$c.zip: lo bueno sale y lo peligroso no (ni fuera de la carpeta)"
  else ko "$c.zip:"; find mal | head; head -3 s6.txt; fi
done
$A unzip -l casos/solapado.zip > s7.txt 2>&1; grep -q "pisan" s7.txt && ok "la bomba solapada (dos entradas sobre los mismos datos): no se abre" || ko "la bomba solapada: $(head -1 s7.txt)"
rm -rf sa; COMPRIM_MAX_TOTAL=1048576 $A unzip -q -d sa casos/ceros.zip > s8.txt 2>&1
[ $? = 80 ] && grep -q "bomba" s8.txt && [ ! -e sa/ceros.bin ] && ok "el tope de lo descomprimido (aquí, 1 MB): 8 MB de ceros no se sacan" || ko "el tope: $(cat s8.txt | head -2)"
rm -rf sa; $A unzip -q -d sa casos/miente.zip > /dev/null 2>&1; [ ! -e sa/corto.txt ] && ok "uno que da más de lo que dice: se para en lo que dice y no se queda" || ko "el que miente se ha sacado"
rm -rf sa; $A unzip -q -d sa casos/crc.zip > s9.txt 2>&1; grep -q "CRC" s9.txt && [ ! -e sa/crc.txt ] && ok "el CRC cambiado: se dice y no se queda" || ko "el CRC: $(head -1 s9.txt)"
# 7. el fuzz
cp linux.zip zc.zip nuestro.zip mod.zip casos/
rm -f casos/ceros.zip
if python3 -I $AQUI/fuzz.py $A casos ${1:-1500} > fz.txt 2>&1; then ok "el fuzz: $(tail -1 fz.txt) (con ASan y UBSan)"
else ko "el fuzz:"; tail -12 fz.txt; fi
echo "prueba.sh: $nok bien, $fail mal"
exit $fail
