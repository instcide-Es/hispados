#!/bin/bash
# Prueba de INSTALAR unidad: (disco formateado sin sistema -> arranca solo y
# tiene las utilidades) y del historial de órdenes con teclas reales
# (flecha arriba, flecha abajo y F3).
# INSTALAR and command history test.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
cp "$IMG" $T/f.img
# CONFIG.SYS con la letra del disquete: INSTALAR la cambia a C:
printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nDOS=HIGH\r\n' > $T/CONFIG.SYS
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
dd if=/dev/zero of=$T/hdd.img bs=1M count=32 status=none
send() { sleep "${2:-1.5}"; printf '%s\r' "$1"; }
# 1) FDISK + FORMAT sin sistema + INSTALAR C:
{
  sleep 4
  send "FDISK /PRI" 2
  send "FORMAT C: /V:DESTINO /AUTOTEST" 4
  send "INSTALAR /?" 2
  send "INSTALAR A:" 2
  send "INSTALAR C:" 8
} | timeout 40 qemu-system-i386 -display none -serial stdio -monitor none \
      -drive file=$T/f.img,format=raw,if=floppy \
      -drive file=$T/hdd.img,format=raw,if=ide -boot a -no-reboot > $T/l1 2>&1
# 2) arrancar desde C: y usar el historial con teclas reales
qemu-system-i386 -display none -serial file:$T/l2 -monitor unix:$T/mon,server,nowait \
    -drive file=$T/hdd.img,format=raw,if=ide -boot c -no-reboot &
QP=$!
sleep 5
k() { for x in "$@"; do printf 'sendkey %s\n' "$x" | socat - UNIX-CONNECT:$T/mon >/dev/null 2>&1; sleep 0.08; done; }
k shift-e shift-c shift-o spc u n o ret; sleep 1
k shift-e shift-c shift-o spc d o s ret; sleep 1
k up up ret; sleep 1                 # ECO uno
k up down down ret; sleep 1          # línea vacía
k f3 ret; sleep 1                    # repite la última (ECO uno)
k shift-l shift-i shift-s shift-t shift-a shift-r ret; sleep 2
k shift-l shift-i shift-s shift-t shift-a shift-r spc shift-h shift-i shift-s shift-p shift-a shift-d shift-o shift-s ret; sleep 2
kill $QP 2>/dev/null; wait $QP 2>/dev/null
tr -d '\r' < $T/l1 | iconv -f CP437 -t UTF-8 > $T/l1.txt
tr -d '\r' < $T/l2 | iconv -f CP437 -t UTF-8 > $T/l2.txt
fail=0
check() {
  if grep -qF -- "$2" "$1"; then echo "  OK   $2"; else echo "  FAIL $2"; fail=1; fi
}
echo "== Hispa-DOS INSTALAR and history tests =="
check $T/l1.txt "Uso: INSTALAR unidad:"
check $T/l1.txt "No se puede instalar sobre la unidad de arranque."
check $T/l1.txt "Sistema transferido a la unidad C:"
check $T/l1.txt "Hispa-DOS instalado en la unidad C:"
check $T/l2.txt "Hispa-DOS 0.9 - núcleo compatible con MS-DOS 6.22"
check $T/l2.txt "HSPXMS 0.1: gestor XMS 3.0"
check $T/l2.txt "HISPADOS"
check $T/l2.txt "TECLADO  COM"
check $T/l2.txt "INSTALAR COM"
check $T/l2.txt "VELOCIDA COM"      # VELOCIDAD (8 letras en el disco)
check $T/l2.txt "LOADFIX  COM"
check $T/l2.txt "AUDICION COM"      # la prueba de sonido (docs/AUDICION.md)
check $T/l2.txt "ALIAS    INI"
check $T/l2.txt "CONFIG   HVP"       # [Configuración] el programa de ajustes de Hispa Ventanas
# los programas de Hispa Ventanas (.HVP) también se copian: los juegos y Pintura
hvp=0
for off in 32256 1048576; do
  MTOOLS_SKIP_CHECK=1 mdir -i $T/hdd.img@@$off ::HISPADOS/JUEGOS/CULEBRA.HVP ::HISPADOS/PROGS/PINTURA.HVP > /dev/null 2>&1 && hvp=1
done
if [ $hvp = 1 ]; then echo "  OK   \\HISPADOS\\JUEGOS y \\HISPADOS\\PROGS, con sus .HVP, en el disco instalado"; else echo "  FAIL faltan los .HVP de \\HISPADOS\\JUEGOS o \\HISPADOS\\PROGS en el disco instalado"; fail=1; fi
# y los cartuchos de los emuladores, cada uno en su carpeta (JUEGOS\NES y JUEGOS\SMS)
car=0
for off in 32256 1048576; do
  MTOOLS_SKIP_CHECK=1 mdir -i $T/hdd.img@@$off ::HISPADOS/JUEGOS/NES/HISPA1.NES ::HISPADOS/JUEGOS/SMS/BUZO.SMS > /dev/null 2>&1 && car=1
done
if [ $car = 1 ]; then echo "  OK   HISPA1.NES en \\HISPADOS\\JUEGOS\\NES y BUZO.SMS en \\HISPADOS\\JUEGOS\\SMS, en el disco instalado"; else echo "  FAIL faltan los cartuchos de JUEGOS\\NES o JUEGOS\\SMS en el disco instalado"; fail=1; fi
# el historial: "uno" debe salir tres veces (la orden, flecha arriba x2 y F3)
n=$(grep -c "^uno$" $T/l2.txt)
if [ "$n" -ge 3 ]; then echo "  OK   historial: ECO uno repetido ($n veces)"; else echo "  FAIL historial: ECO uno salió $n veces"; fail=1; fi
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/l1.txt; echo ---; cat $T/l2.txt; fi
rm -rf $T
exit $fail
