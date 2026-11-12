#!/bin/bash
# Prueba de XCOPIAR (XCOPY): /S, /E, /A, /M, /D, /P, /V, /W, /Y, /-Y y
# COPYCMD, la pregunta de sobrescribir y la de archivo/directorio, comodines,
# la copia cíclica, ERRORLEVEL y un archivo mayor que el búfer. Los datos
# están en un disquete en B:; se comprueban con mtools (contenido, atributos
# y fechas) y con fsck.fat.
# XCOPY test.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
cp "$IMG" $T/f.img
dd if=/dev/zero of=$T/b.img bs=1k count=1440 status=none
mformat -i $T/b.img -f 1440 ::
mkdir -p $T/o/SUB/HONDO
printf 'archivo A\r\n' > $T/o/A.TXT
printf 'archivo B, sin atributo de archivo\r\n' > $T/o/B.TXT
printf 'solo lectura\r\n' > $T/o/RO.TXT
printf 'de 2020\r\n' > $T/o/VIEJO.TXT
printf 'oculto\r\n' > $T/o/OCULTO.TXT
head -c 100000 /dev/urandom > $T/o/GRANDE.BIN
printf 'en SUB\r\n' > $T/o/SUB/C.TXT
head -c 20000 /dev/urandom > $T/o/SUB/HONDO/D.BIN
touch -d '2020-03-15 10:20:00' $T/o/VIEJO.TXT
mmd -i $T/b.img ::ORIG ::ORIG/SUB ::ORIG/SUB/HONDO ::ORIG/VACIA
for f in A.TXT B.TXT RO.TXT VIEJO.TXT OCULTO.TXT GRANDE.BIN SUB/C.TXT SUB/HONDO/D.BIN; do
  mcopy -m -i $T/b.img $T/o/$f ::ORIG/$f
done
mattrib -i $T/b.img -a ::ORIG/B.TXT
mattrib -i $T/b.img +r ::ORIG/RO.TXT
mattrib -i $T/b.img +h ::ORIG/OCULTO.TXT
# órdenes; «@x» = una tecla sin Intro (para /W)
CMDS=("XCOPIAR B:\\ORIG B:\\S1 /S" "D"
      "XCOPY B:\\ORIG B:\\E1\\ /S /E /V"
      "IF ERRORLEVEL 1 ECHO MAL-NIVEL-BIEN"
      "XCOPIAR B:\\ORIG\\*.TXT B:\\A1\\ /A"
      "XCOPIAR B:\\ORIG\\*.TXT B:\\D1\\ /D:01-06-2025"
      "XCOPIAR B:\\ORIG\\*.TXT B:\\D2\\ /D:1/1/20"
      "XCOPIAR B:\\ORIG\\?.TXT B:\\W1\\"
      "XCOPIAR B:\\ORIG\\A.TXT B:\\P1.TXT" "A"
      "XCOPIAR B:\\ORIG\\B.TXT B:\\P1.TXT" "N"
      "SET COPYCMD=/Y"
      "XCOPIAR B:\\ORIG\\RO.TXT B:\\P1.TXT"
      "XCOPIAR B:\\ORIG\\B.TXT B:\\P1.TXT /-Y" "S"
      "SET COPYCMD="
      "XCOPIAR B:\\ORIG\\*.TXT B:\\P2\\ /P" "S" "N" "N" "N"
      "XCOPIAR B:\\ORIG\\A.TXT B:\\P2 /Y"
      "XCOPIAR B:\\ORIG\\*.TXT B:\\M1\\ /M"
      "XCOPIAR B:\\ORIG B:\\ORIG\\DENTRO /S"
      "IF ERRORLEVEL 4 ECHO NIVEL-CUATRO"
      "XCOPIAR B:\\ORIG\\*.TXT B:\\ORIG"
      "XCOPIAR B:\\NADA*.XYZ B:\\N1\\"
      "IF ERRORLEVEL 2 ECHO MAL-NIVEL-DOS"
      "IF ERRORLEVEL 1 ECHO NIVEL-UNO"
      "XCOPIAR B:\\ORIG /Q"
      "XCOPIAR B:\\ORIG\\A.TXT B:\\W2\\ /W" "@x"
      "B:" "MD \\CUR" "CD \\CUR" "XCOPIAR \\ORIG\\SUB" "A:"
      "XCOPIAR Z:\\*.* B:\\Z\\")
{ sleep 6; for c in "${CMDS[@]}"; do
    if [ "$c" = "@x" ]; then printf 'x'; else printf '%s\r' "$c"; fi; sleep 3; done; } | \
  timeout 200 qemu-system-i386 -display none -serial stdio -monitor none -parallel none \
  -drive file=$T/f.img,format=raw,if=floppy,index=0 -drive file=$T/b.img,format=raw,if=floppy,index=1 \
  -boot a -no-reboot > $T/log 2>&1
tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 > $T/log.txt
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() { if grep -qF -- "$1" $T/log.txt; then ok "$1"; else ko "$1"; fi; }
checkn() { if grep -qxF -- "$1" $T/log.txt; then ko "no debía salir: $1"; else ok "no sale: $1"; fi; }
have() { if mdir -b -i $T/b.img "::$1" >/dev/null 2>&1; then ok "existe $1"; else ko "falta $1"; fi; }
nohave() { if mdir -b -i $T/b.img "::$1" >/dev/null 2>&1; then ko "no debía existir $1"; else ok "no existe $1"; fi; }
same() { if mtype -i $T/b.img "::$1" 2>/dev/null | cmp -s - "$2"; then ok "contenido de $1"; else ko "contenido de $1"; fi; }
attr() { a=$(mattrib -i $T/b.img "::$1" 2>/dev/null | cut -c1-12 | tr -d ' '); if [ "$a" = "$2" ]; then ok "atributos de $1: «$2»"; else ko "atributos de $1: «$a» (se esperaba «$2»)"; fi; }
echo "== Hispa-DOS XCOPIAR =="
# /S: subcarpetas no vacías, sin ocultos, contenido idéntico
check "¿Es B:\\S1 un archivo o un directorio (A = archivo, D = directorio)?"
check "Leyendo los archivos de origen..."
check "B:\\ORIG\\SUB\\HONDO\\D.BIN"
check "7 archivo(s) copiado(s)"
for f in A.TXT B.TXT RO.TXT VIEJO.TXT GRANDE.BIN SUB/C.TXT SUB/HONDO/D.BIN; do same S1/$f $T/o/$f; done
nohave S1/OCULTO.TXT
nohave S1/VACIA
# /E (y /V): también la carpeta vacía
have E1/VACIA
same E1/GRANDE.BIN $T/o/GRANDE.BIN
same E1/SUB/HONDO/D.BIN $T/o/SUB/HONDO/D.BIN
checkn "MAL-NIVEL-BIEN"
# atributos y fecha de las copias
attr S1/RO.TXT A
attr S1/A.TXT A
attr ORIG/RO.TXT R
if mdir -i $T/b.img ::S1/VIEJO.TXT 2>/dev/null | grep -q '2020-03-15  10:20'; then ok "fecha y hora conservadas"; else ko "fecha y hora de VIEJO.TXT"; mdir -i $T/b.img ::S1/VIEJO.TXT; fi
# /A: solo con atributo de archivo, sin cambiarlo
have A1/A.TXT; have A1/RO.TXT; have A1/VIEJO.TXT; nohave A1/B.TXT
# /D
have D1/A.TXT; have D1/B.TXT; nohave D1/VIEJO.TXT
have D2/VIEJO.TXT
# comodines
have W1/A.TXT; have W1/B.TXT; nohave W1/RO.TXT
# pregunta de sobrescribir, /Y, /-Y y COPYCMD
check "¿Sobrescribir B:\\P1.TXT (S/N/T)?"
n=$(grep -c "¿Sobrescribir" $T/log.txt)
if [ "$n" = 2 ]; then ok "dos preguntas de sobrescribir (COPYCMD=/Y y /Y no preguntan)"; else ko "preguntas de sobrescribir: $n"; fi
same P1.TXT $T/o/B.TXT
# /P
check "B:\\ORIG\\A.TXT (S/N)?"
check "B:\\ORIG\\VIEJO.TXT (S/N)?"
have P2/A.TXT; nohave P2/B.TXT; nohave P2/RO.TXT
# /M: copia y quita el atributo de archivo del origen (/A no lo quitó)
# (RO.TXT conserva solo lectura en el origen; la copia no)
have M1/A.TXT; have M1/RO.TXT; nohave M1/B.TXT
attr ORIG/A.TXT ""
attr ORIG/VIEJO.TXT ""
attr ORIG/SUB/C.TXT A
# copia cíclica y sobre sí misma
check "No se puede ejecutar XCOPIAR de forma cíclica"
if grep -qx "NIVEL-CUATRO" $T/log.txt; then ok "ERRORLEVEL 4 tras la copia cíclica"; else ko "ERRORLEVEL 4"; fi
nohave ORIG/DENTRO
check "No se puede copiar un archivo sobre sí mismo"
# sin archivos: ERRORLEVEL 1
check "No se encontró el archivo - B:\\NADA*.XYZ"
check "0 archivo(s) copiado(s)"
if grep -qx "NIVEL-UNO" $T/log.txt; then ok "ERRORLEVEL 1 sin archivos"; else ko "ERRORLEVEL 1"; fi
checkn "MAL-NIVEL-DOS"
check "Parámetro no válido - /Q"
# /W
check "Pulse una tecla para empezar a copiar los archivos"
have W2/A.TXT
# destino por omisión: la carpeta actual
same CUR/C.TXT $T/o/SUB/C.TXT
check "Especificación de unidad no válida"
nohave Z
if fsck.fat -n $T/b.img >/dev/null 2>&1; then ok "fsck.fat"; else ko "fsck.fat"; fsck.fat -n $T/b.img; fi
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/XCOPIAR B:\\ORIG B:\\S1/,$p' $T/log.txt; fi
rm -rf $T
exit $fail
