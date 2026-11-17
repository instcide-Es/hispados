#!/bin/bash
# Prueba de HW32 (Hispa-486, fase 1): programas Win32 (PE) de consola sobre
# HSPDPMI. PEHOLA (cabecera PE a mano): salida, línea de órdenes y código de
# salida; PEFALTA: una función que no hay se dice por su nombre; HOLAC (en C,
# con gcc -m32 y tools/elf2pe.py, si hay gcc de 32 bits): cálculo, montón de
# 400 K y llamadas de C. Varias veces seguidas: no se pierde memoria XMS.
# Win32 PE loader test.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
B=build
for f in HW32.COM PEHOLA.EXE PEFALTA.EXE; do
  [ -f $B/$f ] || { echo "falta $B/$f (make nucprogs)"; exit 1; }
done
HAYC=0
make -s $B/HOLAC.EXE >/dev/null 2>&1 && HAYC=1
T=$(mktemp -d)
cp "$IMG" $T/f.img
printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nFILES=30\r\n' > $T/CONFIG.SYS
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
for f in HW32.COM PEHOLA.EXE PEFALTA.EXE; do mcopy -o -i $T/f.img $B/$f ::BIN/$f; done
[ $HAYC = 1 ] && mcopy -o -i $T/f.img $B/HOLAC.EXE ::BIN/HOLAC.EXE
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() { if grep -qF -- "$1" $T/log.txt; then ok "$1"; else ko "$1"; fi; }
checkn() { if grep -qxF -- "$1" $T/log.txt; then ko "(no debía salir) $1"; else ok "sin \"$1\""; fi; }

echo "== Hispa-486: HW32 (programas Win32 de consola) =="
{ sleep 6
  printf 'A:\\BIN\\HW32 A:\\BIN\\PEHOLA.EXE\r'; sleep 2      # sin DPMI
  printf 'A:\\HISPADOS\\HSPDPMI\r'; sleep 2
  printf 'A:\\BIN\\HW32\r'; sleep 2
  printf 'A:\\BIN\\HW32 A:\\NOESTA.EXE\r'; sleep 2
  printf 'A:\\BIN\\HW32 A:\\BIN\\XMSTEST.COM\r'; sleep 2  # no es PE
  printf 'A:\\BIN\\XMSTEST\r'; sleep 2.5
  printf 'A:\\BIN\\HW32 A:\\BIN\\PEHOLA.EXE uno dos\r'; sleep 3
  printf 'IF ERRORLEVEL 42 ECHO NIVEL-42\r'; sleep 1
  printf 'IF ERRORLEVEL 43 ECHO NIVEL-43\r'; sleep 1
  printf 'A:\\BIN\\HW32 A:\\BIN\\PEFALTA.EXE\r'; sleep 3
  printf 'IF ERRORLEVEL 255 ECHO NIVEL-255\r'; sleep 1
  if [ $HAYC = 1 ]; then
    printf 'A:\\BIN\\HW32 A:\\BIN\\HOLAC.EXE tres\r'; sleep 5
    printf 'IF ERRORLEVEL 7 ECHO NIVEL-7\r'; sleep 1
    printf 'IF ERRORLEVEL 8 ECHO NIVEL-8\r'; sleep 1
  fi
  for i in 1 2 3; do printf 'A:\\BIN\\HW32 A:\\BIN\\PEHOLA.EXE\r'; sleep 2.5; done
  printf 'A:\\BIN\\XMSTEST\r'; sleep 2.5
  printf 'ECHO SIGUE-VIVO\r'; sleep 1.5
} | timeout 90 qemu-system-i386 -display none -serial stdio -monitor none -parallel none -m 16 \
    -drive file=$T/f.img,format=raw,if=floppy,index=0 -boot a -no-reboot > $T/raw 2>&1
tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 > $T/log.txt

check "HW32: hace falta HSPDPMI (cliente DPMI de 32 bits)."
check "HW32 PROGRAMA.EXE [parametros]"
check "HW32: no existe el programa."
check "HW32: no es un programa Win32 (PE de 32 bits)."
check "Hola desde Win32"
check "Linea: A:\\BIN\\PEHOLA.EXE uno dos"
check "NIVEL-42"
checkn "NIVEL-43"
check "Hispa-486: falta la funcion ExisteSoloEnPruebas"
check "NIVEL-255"
if [ $HAYC = 1 ]; then
  check "Hola desde C (gcc) en Hispa-486"
  check "Linea: A:\\BIN\\HOLAC.EXE tres"
  check "Primo 100: 541"
  check "Suma: 704982704"
  check "NIVEL-7"
  checkn "NIVEL-8"
else
  echo "  (sin gcc de 32 bits: HOLAC no se prueba)"
fi
n=$(grep -c "^Hola desde Win32" $T/log.txt)
if [ "$n" = 5 ]; then ok "cinco veces PEHOLA/PEFALTA"; else ko "PEHOLA/PEFALTA: $n veces (esperadas 5)"; fi
first=$(grep "largest free KB" $T/log.txt | head -1)
last=$(grep "largest free KB" $T/log.txt | tail -1)
if [ -n "$first" ] && [ "$first" = "$last" ]; then ok "no se pierde memoria XMS ($first)"; else ko "memoria XMS: «$first» y «$last»"; fi
checkn "HSPDPMI: excepción"
check "SIGUE-VIVO"
if [ $fail = 0 ]; then echo "Resultado: todo OK"; else echo "Resultado: FALLOS"; cat $T/log.txt | tail -40; fi
rm -rf $T
exit $fail
