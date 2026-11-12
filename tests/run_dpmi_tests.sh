#!/bin/bash
# Prueba del servidor DPMI HSPDPMI.COM con dos clientes propios (DPMI16.COM,
# de 16 bits, y DPMI32.COM, de 32): entrada en modo protegido, descriptores,
# memoria de DOS y extendida, interrupciones reflejadas, simuladas y con
# manejador del cliente, callbacks, conmutación en crudo, excepciones (con
# manejador y sin él), Ctrl+C, clientes anidados, terminación limpia (no se
# pierde memoria XMS ni de DOS) y que el reloj no se atrasa.
# Si están los programas de DJGPP (tests/thirdparty/fetch_dpmi.sh), también
# NASM, SED y LUA de FreeDOS funcionando sobre HSPDPMI.
# DPMI host test.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
B=build
for f in DPMI16.COM DPMI32.COM; do
  [ -f $B/$f ] || { echo "falta $B/$f (make nucprogs)"; exit 1; }
done
T=$(mktemp -d)
cp "$IMG" $T/f.img
printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nFILES=30\r\n' > $T/CONFIG.SYS
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
mcopy -o -i $T/f.img $B/DPMI16.COM ::BIN/DPMI16.COM
mcopy -o -i $T/f.img $B/DPMI32.COM ::BIN/DPMI32.COM
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() { if grep -qF -- "$1" $T/log.txt; then ok "$1"; else ko "$1"; fi; }
checkn() { if grep -qF -- "$1" $T/log.txt; then ko "(no debía salir) $1"; else ok "sin \"$1\""; fi; }
# run LOG TIEMPO [opciones de qemu...] -- la entrada, por stdin
run() {
  local log=$1 tmo=$2; shift 2
  timeout $tmo qemu-system-i386 -display none -serial stdio -monitor none -parallel none -m 16 \
    -drive file=$T/f.img,format=raw,if=floppy,index=0 -boot a -no-reboot "$@" > $T/raw 2>&1
  tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 > $log
}

echo "== Hispa-DOS HSPDPMI (servidor DPMI 0.9) =="
{ sleep 6
  printf 'A:\\BIN\\DPMI16\r'; sleep 2            # todavía sin servidor
  printf 'A:\\HISPADOS\\HSPDPMI\r'; sleep 2
  printf 'A:\\HISPADOS\\HSPDPMI\r'; sleep 2      # dos veces no
  printf 'A:\\BIN\\DPMI16\r'; sleep 11
  printf 'IF ERRORLEVEL 55 ECHO NIVEL-55\r'; sleep 1.5
  printf 'A:\\BIN\\DPMI32\r'; sleep 10
  printf 'IF ERRORLEVEL 56 ECHO NIVEL-56\r'; sleep 1.5
  printf 'A:\\BIN\\DPMI16 E\r'; sleep 2.5
  printf 'IF ERRORLEVEL 255 ECHO NIVEL-255\r'; sleep 1.5
  printf 'A:\\BIN\\DPMI16 D\r'; sleep 2.5
  printf 'A:\\BIN\\DPMI16 X\r'; sleep 2.5
  printf 'A:\\BIN\\DPMI16 P\r'; sleep 2.5
  printf 'A:\\BIN\\DPMI32 E\r'; sleep 2.5
  printf 'A:\\BIN\\DPMI32 U\r'; sleep 2.5
  printf 'A:\\BIN\\DPMI16 M\r'; sleep 2.5
  printf 'A:\\BIN\\DPMI16 C\r'; sleep 2.5
  printf '\003'; sleep 2
  printf 'ECHO TRAS-CTRL-C\r'; sleep 1.5
  printf 'A:\\BIN\\DPMI16 N\r'; sleep 4
  printf 'A:\\BIN\\DPMI32 N\r'; sleep 4
  printf 'A:\\BIN\\DPMI16 M\r'; sleep 2.5
  printf 'A:\\BIN\\XMSTEST\r'; sleep 2.5
  printf 'ECHO SIGUE-VIVO\r'; sleep 1.5
} | run $T/log.txt 82

check "No hay servidor DPMI"
check "HSPDPMI 0.1 - servidor DPMI 0.9 de Hispa-DOS"
check "Instalado: clientes de 16 y de 32 bits, memoria de XMS."
check "HSPDPMI: ya hay un servidor DPMI instalado."
check "DPMI16: version 005A procesador"
# cliente de 16 bits
for t in "entrada en modo protegido" "INT 2Fh 1686h" "version (0400h)" \
         "descriptores (0000h-000Ch)" "segmento a descriptor (0002h) y selector 40h" \
         "memoria de DOS (0100h-0102h) y 0300h" "memoria extendida (0500h-0503h)" \
         "vectores de modo real (0200h/0201h), 0300h y reflejo" \
         "llamadas lejanas (0301h con pila, 0302h)" \
         "vectores de modo protegido (0204h/0205h) y encadenado" \
         "callback de modo real (0303h/0304h)" \
         "INT 1Ch hacia modo protegido (desde real y desde protegido)" \
         "IRQ 0 con manejador de modo protegido" "interrupciones virtuales (0900h-0902h)" \
         "conmutacion en crudo (0305h/0306h)" "paginas y memoria fisica (0600h-0604h, 0800h)" \
         "excepcion con manejador del cliente (0202h/0203h)" \
         "A20 cerrada por el puerto 92h: se vuelve a abrir" "el reloj no se atrasa"; do
  check "$t: OK16"
done
check "Hola desde modo real (0300h, 16 bits)"
check "DPMI16 terminado, fallos: 00"
check "NIVEL-55"
# cliente de 32 bits
check "DPMI32: en modo protegido de 32 bits"
for t in "entrada, codigo y pila de 32 bits" "selector plano de 4 GB (0000h, 0008h, 0009h)" \
         "memoria extendida (0500h-0503h)" "memoria de DOS, 0300h, 0301h y reflejo" \
         "vectores de modo protegido (0204h/0205h) y encadenado" \
         "callback de modo real (0303h/0304h)" "IRQ 0 e INT 1Ch con manejadores de 32 bits" \
         "excepcion con manejador de 32 bits" "conmutacion en crudo (0305h/0306h)" \
         "el reloj no se atrasa"; do
  check "$t: OK32"
done
check "Hola desde modo real (0300h, 32 bits)"
check "DPMI32 terminado, fallos: 00"
check "NIVEL-56"
checkn "FALLO"
# excepciones sin manejador: mensaje y fin limpio
check "HSPDPMI: excepción 0Dh (fallo de protección general) en"
check "código de error 0FF4"
check "EAX=11111111 EBX=22222222"
check "EAX=AAAA5555"
check "código de error 0FEC"
check "EAX=33333333"
check "HSPDPMI: excepción 00h (división por cero) en"
check "HSPDPMI: excepción 06h (instrucción no válida) en"
n=$(grep -c "HSPDPMI: el programa se ha terminado." $T/log.txt)
if [ "$n" = 6 ]; then ok "seis programas terminados por el servidor"; else ko "programas terminados por el servidor: $n (esperados 6)"; fi
check "NIVEL-255"
check "HSPDPMI: error interno o puntero no válido en una llamada al servidor (excepción 0Dh, IP="
checkn "NO DEBIA LLEGAR AQUI"
# Ctrl+C dentro de una INT 21h reflejada
check "DPMI16: esperando Ctrl+C"
check "TRAS-CTRL-C"
# clientes anidados
check "DPMI16: el hijo ha terminado y el padre sigue: OK"
check "DPMI32: el hijo ha terminado y el padre sigue: OK"
n=$(grep -c "DPMI16: termino sin liberar nada" $T/log.txt)
if [ "$n" = 4 ]; then ok "cuatro clientes terminados sin liberar nada"; else ko "clientes terminados sin liberar: $n (esperados 4)"; fi
# no se pierde memoria: la primera y la última ejecución ven lo mismo libre
first=$(grep "^XMS libre" $T/log.txt | sed -n '2p')
last=$(grep "^XMS libre" $T/log.txt | tail -1)
if [ -n "$first" ] && [ "$first" = "$last" ]; then ok "memoria libre igual al final ($last)"; else ko "memoria libre: «$first» al principio y «$last» al final"; fi
check "XMS test OK"
check "SIGUE-VIVO"
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/DPMI16/,$p' $T/log.txt; fi

# ---- sin gestor XMS no se instala ----
printf 'FILES=20\r\n' > $T/CONFIG.SYS
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
{ sleep 6; printf 'A:\\HISPADOS\\HSPDPMI\r'; sleep 2; printf 'A:\\BIN\\DPMI16\r'; sleep 2; } | run $T/log.txt 20
check "HSPDPMI: hace falta un gestor XMS (HSPXMS.SYS en CONFIG.SYS)."
check "No hay servidor DPMI"

# ---- programas de terceros hechos con DJGPP (si se han descargado) ----
DL=tests/thirdparty/dl
if [ -f $DL/dpmi-nasm/.ok ] && [ -f $DL/dpmi-gnused/.ok ] && [ -f $DL/dpmi-lua/.ok ]; then
  echo "== HSPDPMI con programas de DJGPP (NASM, SED, LUA de FreeDOS) =="
  printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nFILES=30\r\n' > $T/CONFIG.SYS
  mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
  dd if=/dev/zero of=$T/b.img bs=1k count=1440 status=none
  mformat -i $T/b.img -f 1440 ::
  mcopy -i $T/b.img $DL/dpmi-nasm/DEVEL/NASM/NASM.EXE $DL/dpmi-gnused/BIN/SED.EXE \
        $DL/dpmi-lua/DEVEL/LUA/BIN/LUA.EXE ::
  printf 'org 100h\r\nmov ah,9\r\nmov dx,m\r\nint 21h\r\nret\r\nm db "Ensamblado por NASM sobre HSPDPMI",13,10,"$"\r\n' > $T/T.ASM
  printf 'hola mundo\r\notra linea\r\n' > $T/H.TXT
  printf 'local t = {}\nfor i = 1, 2000 do t[i] = i * i end\nlocal s = 0\nfor i = 1, #t do s = s + t[i] end\nprint("suma", s)\nprint(string.format("%%.4f", math.sqrt(2)))\nprint(string.rep("ab", 3):upper())\n' > $T/T.LUA
  # el propio HSPDPMI, ensamblado dentro de Hispa-DOS: tiene que salir igual
  python3 tools/cp437.py drivers/hspdpmi.asm $T/HSPDPMI.ASM
  nasm -f bin -o $T/ref.com $T/HSPDPMI.ASM
  # un cliente de DJGPP que lanza otro (clientes anidados)
  printf 'print(os.execute("SED s/hola/anidado/ H.TXT"))\nprint("vuelta a lua", 3*7)\n' > $T/T2.LUA
  mcopy -i $T/b.img $T/T.ASM $T/H.TXT $T/T.LUA $T/T2.LUA $T/HSPDPMI.ASM ::
  { sleep 6
    printf 'A:\\HISPADOS\\HSPDPMI\r'; sleep 2
    printf 'B:\r'; sleep 1.5
    printf 'NASM -v\r'; sleep 4
    printf 'NASM -f bin -o T.COM T.ASM\r'; sleep 5
    printf 'T\r'; sleep 2
    printf 'SED s/hola/adios/ H.TXT\r'; sleep 5
    printf 'LUA T.LUA\r'; sleep 6
    printf 'LUA T2.LUA\r'; sleep 8
    printf 'NASM -f bin -o H.COM HSPDPMI.ASM\r'; sleep 40
    printf 'A:\\BIN\\XMSTEST\r'; sleep 2.5
    printf 'ECHO SIGUE-VIVO\r'; sleep 1.5
  } | run $T/log.txt 88 -drive file=$T/b.img,format=raw,if=floppy,index=1
  check "NASM version"
  check "Ensamblado por NASM sobre HSPDPMI"
  check "adios mundo"
  check "2668667000"
  check "1.4142"
  check "ABABAB"
  check "anidado mundo"
  check "vuelta a lua"
  checkn "HSPDPMI: excepción"
  checkn "HSPDPMI: error interno"
  if mcopy -n -i $T/b.img ::H.COM $T/h.com 2>/dev/null && cmp -s $T/h.com $T/ref.com; then
    ok "HSPDPMI ensamblado con NASM (DJGPP) dentro de Hispa-DOS: idéntico"
  else
    ko "HSPDPMI ensamblado con NASM (DJGPP) dentro de Hispa-DOS"
  fi
  check "XMS test OK"
  check "SIGUE-VIVO"
  if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/HSPDPMI/,$p' $T/log.txt; fi
else
  echo "  (sin programas de DJGPP: tests/thirdparty/fetch_dpmi.sh los descarga)"
fi
rm -rf $T
exit $fail
