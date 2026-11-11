#!/bin/bash
# Prueba de las órdenes esenciales: ARBOL (TREE), ATRIBUTO (ATTRIB),
# BUSCAR (FIND), ORDENAR (SORT), PAGINAR (MORE), ELEGIR (CHOICE) y
# MOVER (MOVE), con su nombre en español o el clásico.
# Essential commands test.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
cp "$IMG" $T/f.img
send() { sleep "${2:-1.2}"; printf '%s\r' "$1"; }
{
  sleep 5
  send "ARBOL A:\\"
  send "TREE /A"
  send "ARBOL BAT /F"
  send "ATRIBUTO README.TXT"
  send "ATTRIB +R +H README.TXT"
  send "ATTRIB README.TXT"
  send "ATRIBUTO -R -H README.TXT"
  send "ATTRIB /S *.BAT"
  send "BUSCAR \"Hispa\" README.TXT"
  send "FIND /C /I \"hispa\" README.TXT"
  send "FIND /V \"Welcome\" README.TXT"
  send "TYPE README.TXT | FIND /N \"to\""
  send "ECHO pera> F.TXT"
  send "ECHO manzana>> F.TXT"
  send "ECHO zanahoria>> F.TXT"
  send "ORDENAR < F.TXT"
  send "SORT /R F.TXT"
  send "TYPE F.TXT | MORE"
  send "PAGINAR F.TXT"
  send "ELEGIR /C:ABC Opcion"
  send "B" 1
  send "IF ERRORLEVEL 2 ECHO eligio-la-segunda"
  send "CHOICE /T:N,1 Sigo"
  sleep 2
  send "IF ERRORLEVEL 2 ECHO por-omision-N"
  send "MD CAJA"
  send "MOVER F.TXT CAJA"
  send "MOVE CAJA\\F.TXT G.TXT"
  send "MOVE CAJA SACO"
  send "DIR SACO"
  send "TYPE G.TXT"
  send "MOVER *.TXT NOEXISTE.TXT"
  send "ECHO fin"
  sleep 1
} | timeout 75 qemu-system-i386 -display none -serial stdio -monitor none -parallel none \
  -drive file=$T/f.img,format=raw,if=floppy -boot a -no-reboot > $T/log 2>&1
tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 > $T/log.txt
fail=0
check() { if grep -qF -- "$1" $T/log.txt; then echo "  OK   $1"; else echo "  FAIL $1"; fail=1; fi; }
echo "== Hispa-DOS órdenes esenciales =="
check "Listado de carpetas del volumen HISPADOS"
check "El número de serie del volumen es"
check "├───BAT"
check "├───HISPADOS"
check "└───BIN"
check "+---BAT"
check "\\---BIN"
check "PRUEBA.BAT"
check "A            A:\\README.TXT"
check "A     HR     A:\\README.TXT"
check "A:\\BAT\\PRUEBA.BAT"
check "---------- README.TXT"
check "Welcome to Hispa-DOS!"
check "---------- README.TXT: 1"
check "[1]Welcome to Hispa-DOS!"
# ORDENAR: manzana, pera, zanahoria; /R al revés
if sed -n '/>ORDENAR < F.TXT/,/>SORT/p' $T/log.txt | tr '\n' ' ' | grep -q "manzana *pera *zanahoria"; then echo "  OK   ORDENAR"; else echo "  FAIL ORDENAR"; fail=1; fi
if sed -n '/>SORT \/R/,/>TYPE F.TXT/p' $T/log.txt | tr '\n' ' ' | grep -q "zanahoria *pera *manzana"; then echo "  OK   SORT /R"; else echo "  FAIL SORT /R"; fail=1; fi
if sed -n '/>PAGINAR F.TXT/,/>ELEGIR/p' $T/log.txt | grep -q "zanahoria"; then echo "  OK   PAGINAR / MORE"; else echo "  FAIL PAGINAR"; fail=1; fi
check "Opcion[A,B,C]?B"
check "eligio-la-segunda"
check "Sigo[S,N]?N"
check "por-omision-N"
check "F.TXT => CAJA\\F.TXT [correcto]"
check "CAJA\\F.TXT => G.TXT [correcto]"
check "CAJA => SACO [correcto]"
check " Directorio de A:\\SACO"
check "Para mover varios archivos, el destino tiene que ser una carpeta."
if mtype -i $T/f.img ::G.TXT 2>/dev/null | grep -q pera; then echo "  OK   G.TXT movido"; else echo "  FAIL G.TXT"; fail=1; fi
if mattrib -i $T/f.img ::README.TXT 2>/dev/null | grep -q "^  A"; then echo "  OK   README.TXT sin R ni H"; else echo "  FAIL atributos finales: $(mattrib -i $T/f.img ::README.TXT)"; fail=1; fi
if fsck.fat -n $T/f.img >/dev/null 2>&1; then echo "  OK   fsck.fat"; else echo "  FAIL fsck.fat"; fail=1; fi
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log.txt; fi
rm -rf $T
exit $fail
