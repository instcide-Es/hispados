#!/bin/bash
# Prueba de EDIT (editor a pantalla completa) y GESTOR (gestor de archivos)
# con teclas reales por el monitor de QEMU; comprueba los ficheros del disco.
# EDIT and GESTOR test: real keystrokes, then the disk contents are checked.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
cp "$IMG" $T/f.img
printf 'linea uno\r\nlinea dos\r\n' > $T/NOTA.TXT
printf 'para renombrar\r\n' > $T/VIEJO.TXT
mcopy -o -i $T/f.img $T/NOTA.TXT ::NOTA.TXT
mcopy -o -i $T/f.img $T/VIEJO.TXT ::VIEJO.TXT
printf '%s\n' \
  "type EDIT NOTA.TXT" "key ret" "wait 2" "shot edit1" \
  "key ctrl-end" "type linea tres" "key ret" "key f2" "wait 1" "shot edit2" \
  "key esc" "wait 1" \
  "type EDIT NUEVO.TXT" "key ret" "wait 2" "type Hola Hispa-DOS" "key esc" \
  "wait 1" "shot edit3" "type S" "wait 2" \
  "type GESTOR" "key ret" "wait 2" "shot gestor1" \
  "key f7" "wait 1" "type CARPETA" "key ret" "wait 2" "key ret" "wait 2" \
  "type V" "wait 1" "key f6" "wait 1" "type NUEVONOM.TXT" "key ret" "wait 2" "shot gestor2" "key ret" "wait 2" \
  "shot gestor3" "key esc" "wait 2" > $T/guion
tests/tools/screen.sh $T/f.img $T/o $T/guion
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
echo "== Hispa-DOS EDIT y GESTOR =="
D=$T/o_disk.img
mtype -i $D ::NOTA.TXT > $T/nota 2>/dev/null
printf 'linea uno\r\nlinea dos\r\nlinea tres\r\n' > $T/nota_ok
if cmp -s $T/nota $T/nota_ok; then ok "EDIT añade una línea y guarda"; else ko "EDIT guardar"; od -c $T/nota | head -5; fi
if mtype -i $D ::NUEVO.TXT 2>/dev/null | grep -q "^Hola Hispa-DOS"; then ok "EDIT crea un archivo nuevo (Esc + S)"; else ko "EDIT archivo nuevo"; fi
if mdir -i $D ::CARPETA >/dev/null 2>&1; then ok "GESTOR crea una carpeta (F7)"; else ko "GESTOR F7"; fi
if mtype -i $D ::NUEVONOM.TXT 2>/dev/null | grep -q "para renombrar"; then ok "GESTOR renombra (F6)"; else ko "GESTOR F6"; fi
if [ -n "${KEEP:-}" ]; then mkdir -p "$KEEP"; cp $T/o_*.png "$KEEP"/; fi
rm -rf $T
exit $fail
