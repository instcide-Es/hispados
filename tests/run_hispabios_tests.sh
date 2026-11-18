#!/bin/bash
# Pruebas de HispaBIOS: la imagen USB de Hispa-DOS arranca en un PC que solo
# tiene UEFI (QEMU con OVMF sin CSM, de 64 y de 32 bits) por \EFI\BOOT, llega
# a C:\>, el teclado y el disco funcionan, y la opción «Juegos» no reinicia
# el equipo (HSPEMM no se instala con HispaBIOS). El equipo lleva una tarjeta
# de red e1000 (la que QEMU pone por omisión): HSPRED (a mano: ya no va en
# AUTOEXEC.BAT) la tiene que cargar.
# uso: run_hispabios_tests.sh [imagen_usb]     (necesita los paquetes ovmf y ovmf-ia32)
cd "$(dirname "$0")/.."
B=build
USB=${1:-$B/hispados-usb.img}
[ "$USB" = "$B/hispados.img" ] && USB=$B/hispados-usb.img   # (make test pasa la del disquete)
T=$(mktemp -d)
trap 'kill $(jobs -p) 2>/dev/null; rm -rf $T' EXIT
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
export MTOOLS_SKIP_CHECK=1

if [ ! -f "$USB" ]; then echo "  (no hay $USB: haga «make usb»; se salta)"; exit 0; fi

# arranca ARQ (64 o 32) con la imagen IMG; espera a C:\>, escribe TECLAS y espera
# a que aparezca ESPERA en la salida (la consola de Hispa-DOS sale también por COM1)
arranca() {
  local arq=$1 img=$2 nombre=$3 teclas=$4 espera=$5
  local code vars q
  if [ "$arq" = 64 ]; then
    code=/usr/share/OVMF/OVMF_CODE_4M.fd; vars=/usr/share/OVMF/OVMF_VARS_4M.fd; q=qemu-system-x86_64
  else
    code=/usr/share/OVMF/OVMF32_CODE_4M.fd; vars=/usr/share/OVMF/OVMF32_VARS_4M.fd; q=qemu-system-i386
  fi
  if [ ! -f $code ]; then echo "  (falta $code: instale ovmf/ovmf-ia32; se salta $nombre)"; return 1; fi
  cp $vars $T/vars_$nombre.fd
  rm -f $T/serie_$nombre.txt $T/mon_$nombre
  $q -m ${HB_MEM:-512} ${HB_MAQ:-} -smp 2 -display none -monitor unix:$T/mon_$nombre,server,nowait \
    -serial file:$T/serie_$nombre.txt \
    -drive if=pflash,format=raw,readonly=on,file=$code \
    -drive if=pflash,format=raw,file=$T/vars_$nombre.fd \
    -netdev user,id=n0 -device e1000,netdev=n0 \
    -drive file=$img,format=raw,if=none,id=u -device qemu-xhci -device usb-storage,drive=u,bootindex=1 &
  local pid=$! i
  for i in $(seq 1 ${HB_SEG:-240}); do
    sleep 1
    grep -aq "${HB_HASTA:-C:\\\\>}" $T/serie_$nombre.txt 2>/dev/null && break
  done
  if grep -aq 'C:\\>' $T/serie_$nombre.txt; then
    for k in $teclas; do echo "sendkey $k" | socat - UNIX-CONNECT:$T/mon_$nombre > /dev/null; sleep 0.4; done
    for i in $(seq 1 60); do
      sleep 1
      grep -aq "$espera" $T/serie_$nombre.txt && break
    done
  fi
  echo "screendump $T/pantalla_$nombre.ppm" | socat - UNIX-CONNECT:$T/mon_$nombre > /dev/null 2>&1
  sleep 1
  echo quit | socat - UNIX-CONNECT:$T/mon_$nombre > /dev/null 2>&1
  wait $pid 2>/dev/null
  return 0
}

for arq in 64 32; do
  echo "== HispaBIOS: arranque por UEFI de $arq bits (sin CSM) =="
  cp --sparse=always $USB $T/u$arq.img
  if mdir -i $T/u$arq.img@@1048576 ::EFI/BOOT 2>/dev/null | grep -q "BOOT$( [ $arq = 64 ] && echo X64 || echo IA32 )"; then
    ok "la imagen USB lleva \\EFI\\BOOT\\BOOT$( [ $arq = 64 ] && echo X64 || echo IA32 ).EFI"
  else
    ko "falta \\EFI\\BOOT en la imagen USB"; continue
  fi
  arranca $arq $T/u$arq.img n$arq "h s p r e d ret d i r ret" "bytes libres" || continue
  S=$T/serie_n$arq.txt
  grep -aq 'Hispa-DOS 0.9' $S && ok "arranca el núcleo de Hispa-DOS" || ko "no arranca el núcleo"
  grep -aq 'C:\\>' $S && ok "llega a C:\\>" || ko "no llega a C:\\>"
  grep -aq 'bytes libres' $S && ok "DIR responde al teclado (disco y teclado por el firmware)" || ko "DIR no responde"
  grep -aqE 'EFI +<DIR>' $S && ok "DIR ve la carpeta EFI de la memoria" || ko "DIR no ve la carpeta EFI"
  n=$(grep -ac 'BdsDxe: loading' $S)
  [ "$n" -le 1 ] && ok "no se ha reiniciado" || ko "se ha reiniciado ($n arranques)"
  grep -aq 'los lleva el firmware' $S && ok "HSPCD deja la controladora SATA al firmware" || ko "HSPCD no avisa de la SATA"
  grep -aq 'HSPUSB no se carga' $S && ok "HSPUSB deja el USB al firmware" || echo "  (HSPUSB no se carga en esta configuración)"
  # (HSPRED, tecleado antes de DIR, con la e1000 de QEMU, que solo da sus registros
  # en memoria: usa el modo «irreal». Sin la base de su GDT, con HispaBIOS el
  # equipo se reiniciaba aquí una y otra vez: era el fallo de esta prueba.)
  grep -aq 'Tarjeta Intel PRO/1000 (e1000), MAC 52:54:00:12:34:56' $S && ok "HSPRED carga la tarjeta de red (e1000, modo «irreal»)" || ko "HSPRED no carga la tarjeta de red"
done

echo "== HispaBIOS: la opción «Juegos» (HSPEMM no se instala) =="
cp --sparse=always $USB $T/j.img
mcopy -o -i $T/j.img@@1048576 ::CONFIG.SYS $T/cfg.sys
sed -i 's/^MENUDEFAULT=NORMAL,[0-9]*/MENUDEFAULT=JUEGOS,1/I' $T/cfg.sys
mcopy -o -i $T/j.img@@1048576 $T/cfg.sys ::CONFIG.SYS
if arranca 64 $T/j.img juegos "d i r ret" "bytes libres"; then
  S=$T/serie_juegos.txt
  grep -aq 'HSPEMM: equipo UEFI' $S && ok "HSPEMM dice que no se instala (equipo UEFI)" || ko "HSPEMM no avisa"
  grep -aq 'C:\\>' $S && ok "«Juegos» llega a C:\\>" || ko "«Juegos» no llega a C:\\>"
  n=$(grep -ac 'BdsDxe: loading' $S)
  [ "$n" -le 1 ] && ok "no se ha reiniciado" || ko "se ha reiniciado ($n arranques)"
fi

# El modo «irreal» (FS con límite de 4 GB en modo real: HSPRED lo usa para los
# registros de las tarjetas Intel, y muchos juegos) tiene que sobrevivir a los
# viajes al firmware (el tic del reloj que dibuja la pantalla, el disco, el
# teclado): HispaBIOS vuelve al modo real con los segmentos de datos a 4 GB.
# IRREAL.COM (tests/progs/irreal.asm) pone FS, escribe puntos, deja pasar tics
# y lee por encima de 1 MB. OJO: QEMU sin KVM no comprueba los límites de los
# segmentos, así que aquí solo se ve que el programa acaba y no hay reinicio;
# la comprobación de verdad es con KVM o en un PC real.
echo "== HispaBIOS: el modo «irreal» sobrevive a los viajes al firmware =="
if nasm -f bin -o $T/IRREAL.COM tests/progs/irreal.asm; then
  cp --sparse=always $USB $T/i.img
  mcopy -o -i $T/i.img@@1048576 $T/IRREAL.COM ::IRREAL.COM
  if arranca 64 $T/i.img irreal "i r r e a l ret" "IRREAL-BIEN"; then
    S=$T/serie_irreal.txt
    grep -aq 'IRREAL-BIEN' $S && ok "IRREAL acaba: leer con FS por encima de 1 MB tras los tics" || ko "IRREAL no acaba (¿FS perdió el límite?)"
    n=$(grep -ac 'BdsDxe: loading' $S)
    [ "$n" -le 1 ] && ok "no se ha reiniciado" || ko "se ha reiniciado ($n arranques)"
  fi
else
  ko "no se ha podido ensamblar tests/progs/irreal.asm"
fi

# ---- El arranque en un equipo «actual» y cuando algo falla ----
# (El fallo de la 0.9.26 en un PC de verdad: HispaBIOS se quedaba parado tras
# «Mapa de memoria por debajo de 1 MB», en la primera llamada al firmware con
# la pila sin alinear a 16 octetos; OVMF lo perdona y los firmware actuales,
# que usan SSE, no.) BOOTX64.EFI se ensambla aquí con las opciones de prueba
# de hispabios/efi64.asm: PRUEBA_PILA avisa por COM1 («PILA-MAL») de una
# llamada al firmware con la pila mal; PRUEBA_OCUPA ocupa antes páginas bajas
# (como un equipo cuyo firmware las usa); REGISTRO_PASOS guarda HISPABIOS.LOG
# aunque todo vaya bien (la imagen de prueba: tools/mkprueba_hispabios.sh).
#   hb_variante nombre "opciones de nasm" [32] -> $T/nombre.img con ese
#   BOOTX64.EFI (o BOOTIA32.EFI, de efi32.asm, que solo tiene PRUEBA_OCUPA)
hb_variante() {
  nasm -f bin -I hispabios/ -I $B/ $2 -o $T/$1.EFI hispabios/efi${3:-64}.asm || return 1
  cp --sparse=always $USB $T/$1.img
  mcopy -o -i $T/$1.img@@1048576 $T/$1.EFI ::EFI/BOOT/BOOT$( [ "${3:-64}" = 64 ] && echo X64 || echo IA32 ).EFI
}
hb_log() { mtype -i $T/$1.img@@1048576 ::HISPABIOS.LOG 2>/dev/null | tr -d '\r'; }

echo "== HispaBIOS: equipo «actual» (q35, 8 GB) y la pila alineada en las llamadas al firmware =="
if hb_variante pila "-DPRUEBA_PILA" && HB_MEM=8G HB_MAQ="-machine q35" arranca 64 $T/pila.img pila "d i r ret" "bytes libres"; then
  S=$T/serie_pila.txt
  grep -aq 'HispaBIOS 0\.' $S && ok "HispaBIOS arranca" || ko "HispaBIOS no arranca"
  grep -aq 'Memoria del equipo: 819[0-9] MB' $S && ok "lee el mapa de memoria entero (8 GB)" || ko "no lee el mapa de memoria de 8 GB"
  grep -aq 'La BIOS de modo real va en 90000h' $S && ok "la BIOS de modo real, en 90000h" || ko "la BIOS de modo real no va en 90000h"
  grep -aq 'bytes libres' $S && ok "llega a C:\\> y DIR responde" || ko "no llega a C:\\> o DIR no responde"
  grep -aq 'PILA-MAL' $S && ko "hay llamadas al firmware con la pila sin alinear ($(grep -ac 'PILA-MAL' $S))" || ok "todas las llamadas al firmware, con la pila alineada a 16"
  [ -z "$(hb_log pila)" ] && ok "sin fallos no se escribe HISPABIOS.LOG en la memoria" || ko "se ha escrito HISPABIOS.LOG sin fallos"
else
  ko "no se ha podido ensamblar o arrancar la variante PRUEBA_PILA"
fi

# la memoria baja ocupada por el firmware: la BIOS de modo real en otro sitio
# (90000h, 70000h o 60000h; en 80000h no, que ahí se copia el sector de
# arranque de Hispa-DOS: con la BIOS en 80000h no arrancaba)
for caso in "70000h 352 58000h,1,9E000h,2" "60000h 384 7E000h,1,9E000h,2"; do
  set -- $caso
  echo "== HispaBIOS: el firmware usa la memoria baja ($3): la BIOS en $1 =="
  n=ocupa${1%0000h}
  if hb_variante $n "-DPRUEBA_PILA -DREGISTRO_PASOS -DPRUEBA_OCUPA=$3" && HB_MEM=2G HB_MAQ="-machine q35" arranca 64 $T/$n.img $n "d i r ret" "bytes libres"; then
    S=$T/serie_$n.txt
    grep -aq "La BIOS de modo real va en $1. Memoria convencional para DOS: $2 KB" $S && ok "elige la base $1 y da $2 KB a DOS" || ko "no elige la base $1 con $2 KB"
    grep -aq 'bytes libres' $S && ok "llega a C:\\> y DIR responde" || ko "no llega a C:\\> o DIR no responde"
    grep -aq "$2 KB convencional" $S && ok "DOS ve $2 KB de memoria convencional" || ko "DOS no ve $2 KB"
    grep -aq 'PILA-MAL' $S && ko "llamadas al firmware con la pila sin alinear" || ok "la pila, alineada"
    n2=$(grep -ac 'BdsDxe: loading' $S)
    [ "$n2" -le 1 ] && ok "no se ha reiniciado" || ko "se ha reiniciado ($n2 arranques)"
    L=$(hb_log $n)
    echo "$L" | grep -q '^Firmware: .*EDK II.*UEFI 2\.' && ok "HISPABIOS.LOG: el fabricante y la versión del firmware" || ko "HISPABIOS.LOG sin el firmware"
    echo "$L" | grep -q '^Mapa de memoria: [0-9]* entradas de [0-9]* octetos' && [ "$(echo "$L" | grep -c ' atr\. ')" -ge 20 ] && ok "HISPABIOS.LOG: el mapa de memoria completo" || ko "HISPABIOS.LOG sin el mapa completo"
    echo "$L" | grep -q 'Arrancando la BIOS de modo real' && echo "$L" | grep -q 'Memoria extendida: .*discos: 1' && ok "HISPABIOS.LOG: los pasos hasta arrancar la BIOS" || ko "HISPABIOS.LOG sin los pasos"
  else
    ko "no se ha podido ensamblar o arrancar la variante $n"
  fi
done

echo "== HispaBIOS de 32 bits: el firmware usa la memoria baja (58000h, 8E000h, 9E000h-9FFFFh): la BIOS en 70000h =="
if hb_variante ocupa32 "-DPRUEBA_OCUPA=58000h,1,8E000h,1,9E000h,2" 32 && arranca 32 $T/ocupa32.img ocupa32 "d i r ret" "bytes libres"; then
  S=$T/serie_ocupa32.txt
  grep -aq 'La BIOS de modo real va en 00070000' $S && ok "elige la base 70000h" || ko "no elige la base 70000h"
  grep -aq '352 KB convencional' $S && ok "DOS ve 352 KB de memoria convencional" || ko "DOS no ve 352 KB"
  grep -aq 'bytes libres' $S && ok "llega a C:\\> y DIR responde" || ko "no llega a C:\\> o DIR no responde"
  n2=$(grep -ac 'BdsDxe: loading' $S)
  [ "$n2" -le 1 ] && ok "no se ha reiniciado" || ko "se ha reiniciado ($n2 arranques)"
else
  echo "  (sin OVMF de 32 bits: se salta)"
fi

echo "== HispaBIOS: el firmware usa la página del sector de arranque (80000h): lo dice y no arranca =="
if hb_variante pag80 "-DPRUEBA_PILA -DPRUEBA_OCUPA=80000h,1" && HB_HASTA="Pulse una tecla" HB_SEG=90 arranca 64 $T/pag80.img pag80 "" "Pulse una tecla"; then
  S=$T/serie_pag80.txt
  grep -aq 'El firmware usa la pagina 80000h' $S && grep -aq 'ESTE EQUIPO NO DEJA LIBRE LA MEMORIA BAJA' $S && ok "avisa y no arranca DOS encima" || ko "no avisa de la página 80000h"
  grep -aq 'Hispa-DOS 0\.9' $S && ko "ha arrancado DOS" || ok "DOS no arranca"
else
  ko "no se ha podido ensamblar o arrancar la variante de la página 80000h"
fi

echo "== HispaBIOS: sin memoria baja libre: lo dice, dice qué hacer y guarda HISPABIOS.LOG =="
if hb_variante sinbase "-DPRUEBA_PILA -DPRUEBA_OCUPA=5F000h,1,6E000h,1,7E000h,1,8E000h,1,9E000h,2" && HB_MEM=8G HB_MAQ="-machine q35" HB_HASTA="Pulse una tecla" HB_SEG=90 arranca 64 $T/sinbase.img sinbase "" "Pulse una tecla"; then
  S=$T/serie_sinbase.txt
  grep -aq 'ESTE EQUIPO NO DEJA LIBRE LA MEMORIA BAJA' $S && ok "avisa de que no hay memoria baja libre" || ko "no avisa de la memoria baja"
  grep -aq 'arranque heredado' $S && ok "dice qué hacer (CSM / arranque heredado)" || ko "no dice qué hacer"
  grep -aq 'Se ha guardado el registro en la memoria USB: HISPABIOS.LOG' $S && ok "dice que ha guardado el registro" || ko "no dice que ha guardado el registro"
  grep -aq 'Pulse una tecla' $S && ok "espera una tecla (no se queda parado sin decir nada)" || ko "no llega a esperar la tecla"
  grep -aq 'PILA-MAL' $S && ko "llamadas al firmware con la pila sin alinear" || ok "la pila, alineada"
  L=$(hb_log sinbase)
  echo "$L" | grep -q '^Firmware: ' && echo "$L" | grep -q 'no da de 9E000 a 9FFFF (2 pag.)' && echo "$L" | grep -q 'ESTE EQUIPO NO DEJA LIBRE' && ok "HISPABIOS.LOG: firmware, páginas que no da y el fallo" || ko "HISPABIOS.LOG incompleto"
  [ "$(echo "$L" | grep -c ' atr\. ')" -ge 100 ] && ok "HISPABIOS.LOG: el mapa de memoria completo ($(echo "$L" | grep -c ' atr\. ') tramos)" || ko "HISPABIOS.LOG sin el mapa completo"
else
  ko "no se ha podido ensamblar o arrancar la variante sin memoria baja"
fi

echo "== HispaBIOS: sin pantalla gráfica (GOP): lo dice y guarda HISPABIOS.LOG =="
if hb_variante singop "-DPRUEBA_PILA -DPRUEBA_SIN_GOP" && HB_HASTA="Pulse una tecla" HB_SEG=90 arranca 64 $T/singop.img singop "" "Pulse una tecla"; then
  S=$T/serie_singop.txt
  grep -aq 'ESTE EQUIPO NO DA UNA PANTALLA GRAFICA' $S && ok "avisa de que no hay pantalla gráfica" || ko "no avisa de la pantalla"
  hb_log singop | grep -q 'ESTE EQUIPO NO DA UNA PANTALLA GRAFICA' && ok "HISPABIOS.LOG guardado con el fallo" || ko "HISPABIOS.LOG sin el fallo"
else
  ko "no se ha podido ensamblar o arrancar la variante sin GOP"
fi

exit $fail
