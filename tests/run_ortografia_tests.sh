#!/bin/bash
# Prueba del corrector ortográfico (programas/comun/ortografia.inc y
# DICCIO.DAT), fuera de Hispa Ventanas: no necesita QEMU ni la imagen.
#
# tests/tools/ortografia_prueba.py ensambla la pieza tal cual dentro de un
# programa de Linux de 32 bits y la compara con un modelo hecho en Python:
# todas las formas del diccionario (más de 650.000), decenas de miles de
# palabras que no deben valer, un texto de verdad, las propuestas para una
# lista de faltas y para miles de palabras estropeadas, las palabras del
# usuario, archivos rotos y lo que tarda. Al final dice «N OK / M FAIL».
#
# Antes mira que la pieza se ensambla también como parte de un módulo .HVP
# (con tools/mkhvp.py, como la usará Pluma) y que tools/hazdiccio.py lee lo
# que escribe.
#
# Si está el diccionario de origen (el paquete hunspell-es; ORTO_FUENTE dice
# la carpeta de es_ES.aff y es_ES.dic, por defecto /usr/share/hunspell), se
# rehace entero y se compara con DICCIO.DAT. Cómo conseguirlo, en
# docs/ORTOGRAFIA.md.
#
# uso: tests/run_ortografia_tests.sh [CUÁNTAS_AL_AZAR]     (por defecto, 60000)
# Necesita nasm, ld (binutils, con -m elf_i386) y python3.
set -u
T=$(mktemp -d)
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }

echo "== Ortografía: la pieza, dentro de un módulo =="
{
  echo '%include "hvp.inc"'
  echo 'HVP_CABECERA "Prueba de ortografia", entrada, 1'
  for i in $(seq 32); do echo '        db "................................"'; done
  cat <<'ASM'
HVP_CODIGO
entrada:
        call ort_cargar
        mov esi, palabra
        mov ecx, 4
        call ort_palabra
        mov edi, propuestas
        mov edx, ORT_SUG
        call ort_sugerir
        call ort_propia
        call ort_propias_ptr
        call ort_propias_poner
        ret
palabra db "casa"
%include "ortografia.inc"
%include "hvplib.inc"
        section .bss
propuestas  resb ORT_SUG * 64
        ORTO_BSS
HVP_FIN
ASM
} > $T/ortop.asm
if NASM=${NASM:-nasm} python3 tools/mkhvp.py $T/ortop.asm $T/ORTOP.HVP > $T/mkhvp.txt 2>&1; then
  ok "se ensambla como parte de un módulo .HVP ($(sed -n 's/.*: //p' $T/mkhvp.txt | head -1))"
else
  ko "no se ensambla dentro de un módulo .HVP"; cat $T/mkhvp.txt
fi

echo "== Ortografía: contra el modelo =="
python3 tests/tools/ortografia_prueba.py ${1:-60000} | tee $T/prueba.txt
[ "${PIPESTATUS[0]}" -eq 0 ] || fail=1
cuenta=$(tail -1 $T/prueba.txt)
case "$cuenta" in
  *" OK / "*" FAIL") n_ok=${cuenta%% OK*}; n_ko=${cuenta##*/ }; n_ko=${n_ko% FAIL};;
  *) n_ok=0; n_ko=1; fail=1;;
esac
[ $fail -ne 0 ] && [ "$n_ko" -eq 0 ] && n_ko=1
[ $fail -eq 0 ] && n_ok=$(( n_ok + 1 ))
echo "== Ortografía: $n_ok OK / $n_ko FAIL =="
rm -rf $T
exit $fail
