#!/bin/bash
# Prueba de HW32 (Hispa-486, fase 1): programas Win32 (PE) de consola sobre
# HSPDPMI. PEHOLA (cabecera PE a mano): salida, línea de órdenes y código de
# salida; PEFALTA: una función que no hay se dice por su nombre; HOLAC (en C,
# con gcc -m32 y tools/elf2pe.py, si hay gcc de 32 bits): cálculo, montón de
# 400 K y llamadas de C. Varias veces seguidas: no se pierde memoria XMS.
# Fase 2 (también en C): K32PRUE repasa KERNEL32 de consola (archivos,
# búsquedas, directorios, hora, montón, VirtualAlloc, entorno, TLS,
# GetProcAddress y textos) con los resultados de Windows 98; K32CON pinta en
# la pantalla (colores, cursor, rellenar) y se mira su memoria.
# Win32 PE loader test.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
B=build
for f in HW32.COM PEHOLA.EXE PEFALTA.EXE; do
  [ -f $B/$f ] || { echo "falta $B/$f (make nucprogs)"; exit 1; }
done
HAYC=0
make -s $B/HOLAC.EXE $B/K32PRUE.EXE $B/K32CON.EXE >/dev/null 2>&1 && HAYC=1
T=$(mktemp -d)
cp "$IMG" $T/f.img
printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nFILES=30\r\n' > $T/CONFIG.SYS
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
for f in HW32.COM PEHOLA.EXE PEFALTA.EXE; do mcopy -o -i $T/f.img $B/$f ::BIN/$f; done
[ $HAYC = 1 ] && for f in HOLAC.EXE K32PRUE.EXE K32CON.EXE; do mcopy -o -i $T/f.img $B/$f ::BIN/$f; done
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
    printf 'A:\\BIN\\HW32 A:\\BIN\\K32PRUE.EXE\r'; sleep 14
    printf 'IF ERRORLEVEL 3 ECHO NIVEL-3\r'; sleep 1
  fi
  for i in 1 2 3; do printf 'A:\\BIN\\HW32 A:\\BIN\\PEHOLA.EXE\r'; sleep 2.5; done
  printf 'A:\\BIN\\XMSTEST\r'; sleep 2.5
  printf 'ECHO SIGUE-VIVO\r'; sleep 1.5
} | timeout 110 qemu-system-i386 -display none -serial stdio -monitor none -parallel none -m 16 \
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
  # KERNEL32 (fase 2): lo que da Windows 98
  while IFS= read -r l; do check "$l"; done <<'FIN'
versionex: 4.10 plataforma 1 build 1998
version: C0000A04
crear: si
escritos: 22
atributos: 00000020
tamano: 22
mover: 6
leer3: uno
desde_fin: 19
leidos_fin: 3 00000073
leidos_eof: 0
abrir_nohay: si error 2
crear_nuevo_existe: si error 80
cortado: 5
copiar: 1
copiar_existe: 0
renombrar: 1
ya_no_esta: FFFFFFFF error 2
fin_busqueda: 18
prueba 5 movido 105
fecha_archivo_ok: si
buscar_nada: si error 2
mkdir: 1
cd: 1
dir_actual: A:\SUBDIR
necesita: 10
rmdir: 1
completo: 13 A:\PRUEBA.TXT PRUEBA.TXT
tipo_a: 2
libre: 1 512
temporal: si si
borrar: 1 1
borrar_otra_vez: 0 error 2
ft_1970: 019DB1DED53E8000
ida_y_vuelta: 2026-11-17 12:34:56.789 semana 2
bisiesto: 2-29 semana 2
dos: 0000285D 0000BF7D
dos_vuelta: 0
comparar: -1 1
ahora_ok: si
reusa: si
realloc: conservado 5000 0
grande: si 3
20000_vueltas: si
20_grandes: si
local: si si
virtual: si 1 1
memoria: 32 si si
comspec: si
comspec_corto: si
sin_variable: 0 error 203
tls: si 1234
interlocked: 6 6 10 20
kernel32: BFF70000
getprocaddress: si 6
otra_vez_igual: si
no_existe: si error 127
user32: si error 126
modulo_propio: 00400000
textos: Hola, mundo 11 0 -1
anchos: 4 4 98 4 abc
FIN K32PRUE
NIVEL-3
FIN
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
cp $T/log.txt $T/log1.txt

# ---- la consola en la pantalla (K32CON), desde AUTOEXEC.BAT ----
if [ $HAYC = 1 ]; then
  echo "-- K32CON: la pantalla --"
  printf '@ECHO OFF\r\nA:\\HISPADOS\\HSPDPMI > NUL\r\nCLS\r\nA:\\BIN\\HW32 A:\\BIN\\K32CON.EXE\r\n' > $T/AUTOEXEC.BAT
  mcopy -o -i $T/f.img $T/AUTOEXEC.BAT ::AUTOEXEC.BAT
  qemu-system-i386 -display none -monitor unix:$T/mon,server,nowait -m 16 \
    -drive file=$T/f.img,format=raw,if=floppy,index=0 -boot a -no-reboot >/dev/null 2>&1 &
  QP=$!
  sleep 14
  printf 'pmemsave 0xb8000 4000 "%s"\n' "$T/pant.bin" | socat - UNIX-CONNECT:$T/mon >/dev/null 2>&1
  sleep 1
  printf 'quit\n' | socat - UNIX-CONNECT:$T/mon >/dev/null 2>&1
  wait $QP 2>/dev/null
  if [ -f $T/pant.bin ]; then
    python3 - $T/pant.bin > $T/pant.txt <<'PY'
import sys
d = open(sys.argv[1], 'rb').read()
def txt(f, c, n): return bytes(d[(f*80+c)*2:(f*80+c+n)*2:2]).decode('cp437')
def col(f, c): return d[(f*80+c)*2+1]
print('fila12:', txt(12, 10, 8), 'color %02X %02X' % (col(12, 10), col(12, 17)), 'despues %02X' % col(12, 18))
print('fila3:', txt(3, 70, 5), 'color %02X %02X' % (col(3, 70), col(3, 74)), 'despues %02X' % col(3, 75))
print('fila20:', txt(20, 40, 9))
print('fila14:', txt(14, 0, 50).rstrip())
PY
    cat $T/pant.txt | sed 's/^/     /'
    cp $T/pant.txt $T/log.txt
    check "fila12: AMARILLO color 1E 1E despues 07"
    check "fila3: ##### color 4F 4F despues 07"
    check "fila20: HISPA-486"
    check "fila14: CON: 80x25 cursor 18,12 tipo 2 escritos 9"
  else
    ko "no se ha podido leer la pantalla"
  fi
  cp $T/log1.txt $T/log.txt
fi
if [ $fail = 0 ]; then echo "Resultado: todo OK"; else echo "Resultado: FALLOS"; cat $T/log.txt | tail -40; fi
rm -rf $T
exit $fail
