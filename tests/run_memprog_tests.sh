#!/bin/bash
# Memoria convencional que queda para los programas (muchos juegos piden más
# de 500 KB libres y salen con «not enough memory» o con la pantalla en negro):
#  - el intérprete se parte en dos: quedan ~2 KB residentes junto al PSP y el
#    resto («el cuerpo») va al final de la memoria y, mientras corre un
#    programa, se guarda en memoria extendida (XMS) y se libera;
#  - INT 21h AX=5800h/5801h: estrategia de asignación (primer ajuste, mejor
#    ajuste, último ajuste), que es con lo que el cuerpo se coloca arriba;
#  - tras un programa, un residente (TSR), un intérprete anidado, un lote, un
#    FOR o un Ctrl+C todo sigue en su sitio y no se pierde memoria;
#  - sin gestor XMS el intérprete se queda entero abajo, como antes;
#  - convive con DOS=HIGH (núcleo en la HMA), HSPEMM (EMS, VCPI) y LH; con
#    tanta memoria libre los programas quedarían en los primeros 64 KB, donde
#    los .EXE con EXEPACK salen con «Packed file is corrupt»: ningún .EXE se
#    carga por debajo del segmento 1000h (EXEPILA lo comprueba);
#  - un .EXE con la pila mínima (SP = 2, MINALLOC = 0, como los lanzadores de
#    algunos juegos) arranca sin que el núcleo le toque la pila;
#  - INT 21h devuelve intactas las mitades altas de los registros de 32 bits
#    (y FS y GS), y las lecturas a saltos de los cargadores de juegos (DOS/4GW)
#    dan los datos buenos sin escribir de más, en un CD y en el disco duro.
# Programas de prueba: tests/progs/memtest.asm, exepila.asm y leetest.asm
# (make nucprogs).
# Conventional memory left for programs: COMMAND.COM resident/transient split
# (transient swapped to XMS while a program runs) and INT 21h AX=58h.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
B=build
for f in MEMTEST.COM EXEPILA.EXE LEETEST.COM; do
  [ -f $B/$f ] || { echo "falta $B/$f (make nucprogs)"; exit 1; }
done
T=$(mktemp -d)
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() { if grep -qF -- "$1" $T/log.txt; then ok "$1"; else ko "$1"; fi; }
checkn() { if grep -qF -- "$1" $T/log.txt; then ko "(no debía salir) $1"; else ok "sin \"$1\""; fi; }
cuenta() { local n; n=$(grep -cF -- "$1" $T/log.txt); if [ "$n" -eq "$2" ]; then ok "$3"; else ko "$3 (salen $n, se esperaban $2)"; fi; }
# prepara CONFIG.SYS
prepara() {
  cp "$IMG" $T/f.img
  printf "$1"'FILES=20\r\n' > $T/CONFIG.SYS
  mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
  mcopy -o -i $T/f.img $B/MEMTEST.COM ::BIN/MEMTEST.COM
  mcopy -o -i $T/f.img $B/EXEPILA.EXE ::BIN/EXEPILA.EXE
  printf '@ECHO OFF\r\nA:\\BIN\\MEMTEST\r\nECHO SIGUE-EL-LOTE %%1\r\nA:\\BIN\\HELLO lote\r\nECHO FIN-DEL-LOTE\r\n' > $T/LOTE.BAT
  mcopy -o -i $T/f.img $T/LOTE.BAT ::LOTE.BAT
}
run() {
  timeout $1 qemu-system-i386 -display none -serial stdio -monitor none -parallel none -m 16 \
    -drive file=$T/f.img,format=raw,if=floppy,index=0 -boot a -no-reboot > $T/raw 2>&1
  tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 > $T/log.txt
}
# «LIBRE nnn KB» número N (1 = el primero) del registro
libre() { grep -a "^LIBRE " $T/log.txt | sed -n "${1}p" | awk '{print $2}'; }
# bytes del «mayor programa ejecutable» del MEM número N
mayor() { grep -a "bytes del mayor programa ejecutable" $T/log.txt | sed -n "${1}p" | awk '{gsub(/\./,"",$1); print $1}'; }

echo "== Hispa-DOS: memoria para los programas (intérprete partido, AX=58h) =="
prepara 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\n'
{ sleep 6
  printf 'MEM\r'; sleep 1.5
  printf 'A:\\BIN\\MEMTEST\r'; sleep 2
  printf 'A:\\HISPADOS\\HSPRATON\r'; sleep 2.5          # un residente
  printf 'A:\\BIN\\MEMTEST\r'; sleep 2
  printf 'COMMAND /C A:\\BIN\\MEMTEST\r'; sleep 2.5     # intérprete anidado
  printf 'LOTE uno\r'; sleep 3.5
  printf 'FOR %%A IN (x y) DO A:\\BIN\\HELLO %%A\r'; sleep 3
  printf 'A:\\BIN\\HELLO | BUSCAR "Hello"\r'; sleep 3
  printf 'ECHO a medias\003'; sleep 1.5                 # Ctrl+C en el indicador
  printf 'A:\\BIN\\EXEPILA\r'; sleep 2
  printf 'A:\\BIN\\NOESTA\r'; sleep 1.5
  printf 'MEM /D\r'; sleep 2
  printf 'MEM\r'; sleep 1.5
  printf 'VER\r'; sleep 1.5
} | run 54
check "HSPXMS 0.1"
cuenta "S1 OK" 4 "AX=5800h: al empezar, primer ajuste (4 veces)"
cuenta "S2 OK" 4 "primer ajuste: el bloque va detrás del programa"
cuenta "S3 OK" 4 "último ajuste: el bloque va al final de la memoria"
cuenta "S4 OK" 4 "mejor ajuste: va al hueco más justo"
cuenta "S5 OK" 4 "vuelta al primer ajuste"
cuenta "S6 OK" 4 "una estrategia que no existe da error"
cuenta "S7 OK" 4 "último ajuste sin sitio: error 8 y BX = mayor bloque"
checkn "FALLO"
l1=$(libre 1); l2=$(libre 2); l3=$(libre 3); l4=$(libre 4)
if [ "${l1:-0}" -ge 575 ]; then ok "un programa recibe $l1 KB (el intérprete solo deja ~2 KB abajo)"; else ko "un programa recibe ${l1:-?} KB (se esperaban 575 o más)"; fi
if [ "${l2:-0}" -ge $(( ${l1:-0} - 6 )) ] && [ "${l2:-0}" -lt "${l1:-0}" ]; then ok "con un residente cargado: $l2 KB"; else ko "con un residente cargado: ${l2:-?} KB"; fi
if [ "${l3:-0}" -ge $(( ${l2:-0} - 8 )) ]; then ok "desde un intérprete anidado: $l3 KB"; else ko "desde un intérprete anidado: ${l3:-?} KB"; fi
if [ "${l4:-0}" -eq "${l2:-1}" ]; then ok "desde un lote: $l4 KB, lo mismo"; else ko "desde un lote: ${l4:-?} KB (antes $l2)"; fi
check "SIGUE-EL-LOTE uno"
check "Hello from a .COM program! Args:[ lote]"
check "FIN-DEL-LOTE"
check "Hello from a .COM program! Args:[ x]"
check "Hello from a .COM program! Args:[ y]"
cuenta "Hello from a .COM program! Args:[]" 1 "tubería: programa | filtro"
check "Comando o nombre de archivo incorrecto"
check "EXEPILA OK"
check "COMMAND   Datos"
m1=$(mayor 1); m2=$(mayor 2)
if [ "${m1:-0}" -ge $((575 * 1024)) ]; then ok "MEM cuenta como libre el cuerpo del intérprete: $m1 bytes"; else ko "MEM: mayor programa ejecutable ${m1:-?}"; fi
if [ "${m2:-0}" -ge $(( ${m1:-0} - 6 * 1024 )) ] && [ "${m2:-0}" -lt "${m1:-0}" ]; then ok "al final solo falta lo del residente: $m2 bytes"; else ko "memoria al final: ${m2:-?} (al principio ${m1:-?})"; fi
check "Versión de DOS notificada: 6.22"
checkn "Sistema detenido"
[ $fail -ne 0 ] && cat $T/log.txt

# lecturas como las de los cargadores de juegos, desde un CD y un disco duro
prepara 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nDEVICE=A:\\HISPADOS\\HSPCD.SYS\r\n'
mcopy -o -i $T/f.img $B/LEETEST.COM ::BIN/LEETEST.COM
mkdir -p $T/iso/DIR
python3 -c "
import struct, sys
open(sys.argv[1], 'wb').write(b''.join(struct.pack('<I', n) for n in range(0xAAC00 // 4)))" $T/iso/DIR/DATOS.BIN
genisoimage -quiet -V LEE -o $T/cd.iso $T/iso 2>/dev/null
dd if=/dev/zero of=$T/hd.img bs=1M count=8 status=none
printf '\x80\x01\x01\x00\x06\x0f\x3f\x0f\x3f\x00\x00\x00\xc1\x3e\x00\x00' | dd of=$T/hd.img bs=1 seek=446 conv=notrunc status=none
printf '\x55\xaa' | dd of=$T/hd.img bs=1 seek=510 conv=notrunc status=none
MTOOLS_SKIP_CHECK=1 mformat -i $T/hd.img@@32256 -T 16065 -h 16 -s 63 -H 63 -v DURO ::
MTOOLS_SKIP_CHECK=1 mcopy -i $T/hd.img@@32256 $T/iso/DIR/DATOS.BIN ::DATOS.BIN
{ sleep 7
  printf 'D:\r'; sleep 1; printf 'CD \\DIR\r'; sleep 1
  printf 'A:\\BIN\\LEETEST\r'; sleep 24
  printf 'ARBOL D:\\\r'; sleep 3                     # (comprobación de cambio de disco)
  printf 'A:\\BIN\\MEMTEST\r'; sleep 3
  printf 'C:\r'; sleep 1
  printf 'A:\\BIN\\LEETEST\r'; sleep 12
} | timeout 78 qemu-system-i386 -display none -serial stdio -monitor none -parallel none -m 16 \
    -drive file=$T/f.img,format=raw,if=floppy,index=0 -drive file=$T/hd.img,format=raw,if=ide,index=0 \
    -drive file=$T/cd.iso,media=cdrom,if=ide,index=2 -boot a -no-reboot > $T/raw 2>&1
tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 > $T/log.txt
check "Primera unidad de CD/DVD: D:"
cuenta "LEETEST OK" 2 "lecturas a saltos y registros de 32 bits intactos: CD y disco duro"
# HSPCD escribía su huella del disco fuera de sitio (en memoria de programas)
check "└───DIR"
cuenta "S7 OK" 1 "tras leer del CD y pasar ARBOL por él, la memoria sigue entera"
checkn "LEETEST FALLO"
[ $fail -ne 0 ] && cat $T/log.txt

# con el núcleo en la HMA (DOS=HIGH), HSPEMM (EMS, VCPI) y memoria superior
# (DOS=UMB, LH): el cuerpo del intérprete va y vuelve de XMS mientras corren
# clientes de EMS y de VCPI y un residente cargado arriba
if [ -f $B/EMSTEST.COM ] && [ -f $B/VCPITEST.COM ]; then
  prepara 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nDEVICE=A:\\HISPADOS\\HSPEMM.SYS\r\nDOS=HIGH,UMB\r\n'
  mcopy -o -i $T/f.img $B/EMSTEST.COM ::BIN/EMSTEST.COM
  mcopy -o -i $T/f.img $B/VCPITEST.COM ::BIN/VCPITEST.COM
  { sleep 8
    printf 'A:\\BIN\\MEMTEST\r'; sleep 2.5
    printf 'LH A:\\HISPADOS\\HSPRATON\r'; sleep 3
    printf 'A:\\BIN\\EMSTEST\r'; sleep 3
    printf 'A:\\BIN\\VCPITEST\r'; sleep 3
    printf 'COMMAND /C A:\\BIN\\MEMTEST\r'; sleep 3
    printf 'A:\\BIN\\EXEPILA\r'; sleep 2
    printf 'MEM\r'; sleep 1.5
    printf 'MEM /C\r'; sleep 2
    printf 'VER\r'; sleep 1.5
  } | run 50
  check "HSPEMM 0.1: EMS 4.0 y VCPI"
  check "Hispa-DOS está cargado en la memoria alta (HMA)"
  cuenta "S3 OK" 2 "DOS=HIGH,UMB + HSPEMM: las estrategias van igual"
  cuenta "S7 OK" 2 "DOS=HIGH,UMB + HSPEMM: MEMTEST entero, también anidado"
  checkn "FALLO"
  l1=$(libre 1)
  if [ "${l1:-0}" -ge 600 ]; then ok "con el núcleo en la HMA un programa recibe $l1 KB"; else ko "con DOS=HIGH: ${l1:-?} KB (se esperaban 600 o más)"; fi
  check "EMS test OK"
  check "VCPI test OK"
  check "EXEPILA OK"
  check "Programas en la memoria superior (UMB):"
  check "Versión de DOS notificada: 6.22"
  checkn "Sistema detenido"
  [ $fail -ne 0 ] && cat $T/log.txt
fi

# sin gestor XMS: el intérprete no se parte y todo va como antes
prepara ''
{ sleep 6
  printf 'A:\\BIN\\MEMTEST\r'; sleep 2
  printf 'LOTE dos\r'; sleep 3.5
  printf 'MEM /D\r'; sleep 2
} | run 20
cuenta "S3 OK" 2 "sin XMS: las estrategias van igual"
checkn "FALLO"
l1=$(libre 1)
if [ "${l1:-0}" -ge 530 ] && [ "${l1:-0}" -lt 575 ]; then ok "sin XMS el intérprete se queda entero abajo: $l1 KB"; else ko "sin XMS: ${l1:-?} KB"; fi
check "SIGUE-EL-LOTE dos"
check "FIN-DEL-LOTE"
checkn "COMMAND   Datos"
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log.txt; fi
rm -rf $T
exit $fail
