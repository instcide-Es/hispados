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

# arranca ARQ (64, 32 o a32) con la imagen IMG; espera a C:\>, escribe TECLAS y espera
# a que aparezca ESPERA en la salida (la consola de Hispa-DOS sale también por COM1).
# «a32» es un procesador de 64 bits con firmware UEFI de 32 (los Atom «Bay Trail»:
# Acer One 10); «32», procesador y firmware de 32. HB_EXTRA: más opciones de QEMU
# (otros discos); HB_ARR=fijo: la imagen es un disco fijo (SATA) y no una memoria USB.
arranca() {
  local arq=$1 img=$2 nombre=$3 teclas=$4 espera=$5
  local code vars q disco
  if [ "$arq" = 64 ]; then
    code=/usr/share/OVMF/OVMF_CODE_4M.fd; vars=/usr/share/OVMF/OVMF_VARS_4M.fd; q=qemu-system-x86_64
  else
    code=/usr/share/OVMF/OVMF32_CODE_4M.fd; vars=/usr/share/OVMF/OVMF32_VARS_4M.fd; q=qemu-system-i386
    [ "$arq" = a32 ] && q=qemu-system-x86_64
  fi
  disco="-device qemu-xhci -device usb-storage,drive=u,bootindex=1"
  [ "${HB_ARR:-}" = fijo ] && disco="-device ide-hd,drive=u,bootindex=1"
  if [ ! -f $code ]; then echo "  (falta $code: instale ovmf/ovmf-ia32; se salta $nombre)"; return 1; fi
  cp $vars $T/vars_$nombre.fd
  rm -f $T/serie_$nombre.txt $T/mon_$nombre
  $q -m ${HB_MEM:-512} ${HB_MAQ:-} ${HB_VGA:-} -smp 2 -display none -monitor unix:$T/mon_$nombre,server,nowait \
    -serial file:$T/serie_$nombre.txt \
    -drive if=pflash,format=raw,readonly=on,file=$code \
    -drive if=pflash,format=raw,file=$T/vars_$nombre.fd \
    -netdev user,id=n0 -device e1000,netdev=n0 \
    -drive file=$img,format=raw,if=none,id=u $disco ${HB_EXTRA:-} &
  local pid=$! i
  for i in $(seq 1 ${HB_SEG:-240}); do
    sleep 1
    grep -aq "${HB_HASTA:-C:\\\\>}" $T/serie_$nombre.txt 2>/dev/null && break
  done
  if grep -aq 'C:\\>' $T/serie_$nombre.txt; then
    # (una «tecla» que empieza por «w» es una espera en segundos: w8)
    for k in $teclas; do
      case $k in
        w[0-9]*) sleep ${k#w};;
        *) echo "sendkey $k" | socat - UNIX-CONNECT:$T/mon_$nombre > /dev/null; sleep 0.4;;
      esac
    done
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

# ---- ayudas de las pruebas con variantes de HispaBIOS ----
#   hb_variante nombre "opciones de nasm" [32] -> $T/nombre.img con ese
#   BOOTX64.EFI (o BOOTIA32.EFI, de efi32.asm)
hb_variante() {
  nasm -f bin -I hispabios/ -I $B/ $2 -o $T/$1.EFI hispabios/efi${3:-64}.asm || return 1
  cp --sparse=always $USB $T/$1.img
  mcopy -o -i $T/$1.img@@1048576 $T/$1.EFI ::EFI/BOOT/BOOT$( [ "${3:-64}" = 64 ] && echo X64 || echo IA32 ).EFI
}
hb_log() { mtype -i $T/$1.img@@1048576 ::HISPABIOS.LOG 2>/dev/null | tr -d '\r'; }
# hb_colores captura -> cuántos colores distintos tiene (0 si no hay ImageMagick)
hb_colores() { convert "$1" -format %k info: 2>/dev/null || echo 0; }
# hb_reloj patrón "bien" "mal": el patrón tiene que estar en $L (HISPABIOS.LOG)
hb_reloj() { echo "$L" | grep -q "$1" && ok "$2" || ko "$3"; }

# HB_SOLO=32: solo las pruebas de la parte de 32 bits (tests/hispabios32_casos.sh)
if [ "${HB_SOLO:-}" = 32 ]; then . tests/hispabios32_casos.sh; exit $fail; fi

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
# (la página 58000h suelta se PRESTA a DOS: ya no corta la memoria convencional)
for caso in "70000h 448 58000h,1,9E000h,2" "60000h 384 7E000h,1,9E000h,2"; do
  set -- $caso
  echo "== HispaBIOS: el firmware usa la memoria baja ($3): la BIOS en $1 =="
  n=ocupa${1%0000h}
  # (EQUIPO tiene que ver ACPI y SMBIOS también con la BIOS aquí: la «EBDA» de
  # HispaBIOS queda en 78FAh o 68FAh y EQUIPO, APAGAR y HSPACPI solo la miraban
  # de 8000h para arriba: en el Asus GL553V, «ACPI: no hay»)
  if hb_variante $n "-DPRUEBA_PILA -DREGISTRO_PASOS -DPRUEBA_OCUPA=$3" && HB_MEM=2G HB_MAQ="-machine q35" arranca 64 $T/$n.img $n "d i r ret w3 e q u i p o ret w25" "bytes libres"; then
    S=$T/serie_$n.txt
    grep -aq 'ACPI: RSDP en 000[0-9A-F]*h' $S && grep -aq 'SMBIOS [0-9]\.[0-9]* (tablas en' $S && ok "EQUIPO encuentra ACPI y SMBIOS con la BIOS en $1 (EBDA por debajo de 8000h)" || ko "EQUIPO no encuentra ACPI o SMBIOS con la BIOS en $1"
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

# Las páginas sueltas de memoria baja que usa el firmware (87000h en el ThinkPad
# E14, 58000h en el MacBook Air de 2017, que dejaba a DOS con 352 KB) se prestan
# a DOS: su contenido se cambia en cada viaje al firmware, como la página 0.
# También la del sector de arranque (80000h), que antes impedía arrancar.
echo "== HispaBIOS: páginas sueltas del firmware en la memoria baja (58000h, 80000h, 87000h): se prestan a DOS =="
if hb_variante presta "-DPRUEBA_PILA -DREGISTRO_PASOS -DPRUEBA_OCUPA=58000h,1,80000h,1,87000h,1,9F000h,1" && HB_MEM=2G HB_MAQ="-machine q35" arranca 64 $T/presta.img presta "d i r ret" "bytes libres"; then
  S=$T/serie_presta.txt
  grep -aq 'La BIOS de modo real va en 90000h. Memoria convencional para DOS: 576 KB' $S && ok "la BIOS en 90000h y 576 KB para DOS" || ko "no da 576 KB a DOS"
  grep -aq '576 KB convencional' $S && ok "DOS ve 576 KB de memoria convencional" || ko "DOS no ve 576 KB"
  grep -aq 'bytes libres' $S && ok "llega a C:\\> y DIR responde (con la página del sector de arranque prestada)" || ko "no llega a C:\\> o DIR no responde"
  grep -aq 'PILA-MAL' $S && ko "llamadas al firmware con la pila sin alinear" || ok "la pila, alineada"
  n2=$(grep -ac 'BdsDxe: loading' $S)
  [ "$n2" -le 1 ] && ok "no se ha reiniciado" || ko "se ha reiniciado ($n2 arranques)"
  hb_log presta | grep -q 'se prestan a DOS (se cambian en cada viaje): 58000 80000 87000' && ok "HISPABIOS.LOG: las páginas prestadas" || ko "HISPABIOS.LOG sin las páginas prestadas"
else
  ko "no se ha podido ensamblar o arrancar la variante de las páginas prestadas"
fi

echo "== HispaBIOS: sin prestar páginas, el firmware usa la del sector de arranque (80000h): lo dice y no arranca =="
if hb_variante pag80 "-DPRUEBA_PILA -DPRUEBA_SIN_PRESTAR -DPRUEBA_OCUPA=80000h,1" && HB_HASTA="Pulse una tecla" HB_SEG=90 arranca 64 $T/pag80.img pag80 "" "Pulse una tecla"; then
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

# ---- La velocidad de la pantalla, los modos gráficos (VBE) y las tablas ----
# En dos equipos de verdad (ThinkPad E14: 1920x1200 con la pantalla en 256 GB;
# MacBook Air de 2017: 1440x900) HispaBIOS 0.2 arrancaba «a 1 FPS» y Hispa
# Ventanas decía «no se ha podido poner el modo gráfico». Causas: DOS mira el
# teclado en cada letra que escribe y cada mirada era un viaje al firmware con
# 1 ms de espera; la pantalla del firmware sin escritura combinada; y no había
# VBE. Aquí, con una pantalla de 1920x1200: lo que tarda DIR C:\HISPADOS (unas
# 75 líneas: con la 0.2 tardaba 11 s en QEMU, ahora 1 o 2), el alias con
# escritura combinada (por encima de 4 GB: la entrada 255 del nivel 4), HBLOG,
# las tablas de ACPI y SMBIOS a la vista de EQUIPO, y Hispa Ventanas en un modo
# VBE de 32 bits (captura: tiene que haber colores, no solo texto).
echo "== HispaBIOS: pantalla de 1920x1200: velocidad del texto, HBLOG, EQUIPO y Hispa Ventanas (VBE) =="
if hb_variante rapida "-DPRUEBA_PILA -DREGISTRO_PASOS"; then
  HB_MEM=2G HB_MAQ="-machine q35" HB_VGA="-vga none -device VGA,xres=1920,yres=1200,vgamem_mb=32" \
    arranca 64 $T/rapida.img rapida "d i r spc c shift-semicolon backslash h i s p a d o s ret w6 h b l o g ret w3 e q u i p o ret w25 v e n t a n a s ret w12" "cargando el servidor DPMI"
  S=$T/serie_rapida.txt
  L=$(hb_log rapida)
  echo "$L" | grep -q 'Pantalla (GOP): 1920x1200' && ok "la pantalla del firmware es de 1920x1200" || ko "la pantalla no es de 1920x1200"
  echo "$L" | grep -q 'Modos de pantalla del firmware (GOP; \* el puesto): .*1920x1200\*' && ok "HISPABIOS.LOG: los modos de pantalla del firmware y el puesto" || ko "HISPABIOS.LOG sin los modos de pantalla"
  echo "$L" | grep -q 'Cache de la pantalla: MTRR por omision [0-9A-F]*h; la pantalla, en .*; PAT [0-9A-F]\{16\}' && ok "HISPABIOS.LOG: MTRR y PAT de la pantalla" || ko "HISPABIOS.LOG sin MTRR/PAT"
  echo "$L" | grep -q 'Escritura combinada (WC): alias de la pantalla en 00007F8[0-9A-F]* .*PAT nuevo [0-9A-F]\{4\}01' && ok "alias de la pantalla con escritura combinada (por encima de 4 GB, PAT 5 = WC)" || ko "sin el alias de escritura combinada"
  echo "$L" | grep -q 'Pintar la pantalla entera: [0-9]* us escribiendo directamente, [0-9]* us por el alias con WC. Se pinta por el alias' && ok "mide lo que cuesta pintar la pantalla y pinta por el alias" || ko "no mide la pantalla o no pinta por el alias"
  echo "$L" | grep -q 'Texto: ampliado a toda la pantalla (1920x1200)' && ok "el texto, ampliado a toda la pantalla" || ko "el texto no se amplía a 1920x1200"
  echo "$L" | grep -q 'VBE: marco lineal para los programas en 00000000[0-9A-F]\{8\} ' && ok "el marco lineal de VBE, por debajo de 4 GB" || ko "sin marco lineal de VBE"
  echo "$L" | grep -q 'Tablas del equipo: ACPI (RSDP) en [0-9A-F]* (revision [0-9]*); SMBIOS en [0-9A-F]\{16\}' && ok "HISPABIOS.LOG: las tablas de ACPI y SMBIOS del firmware" || ko "HISPABIOS.LOG sin las tablas del equipo"
  # la velocidad: de «DIR C:\HISPADOS» al final de su lista, por las marcas de tiempo no
  # se puede (la serie no las lleva): se cuenta que HBLOG, tecleado 6 s después, sale
  # DESPUÉS de la lista entera (con la 0.2 la lista tardaba 11 s y HBLOG no llegaba a verse)
  awk '/HBLOG: registro de la BIOS/ { h = NR } /bytes libres/ { b = NR } END { exit !(h && b && b < h) }' $S && ok "DIR C:\\HISPADOS (75 líneas) acaba en menos de 6 s (con la 0.2, 11 s)" || ko "DIR C:\\HISPADOS tarda más de 6 s"
  grep -aq 'HBLOG: registro de la BIOS de HispaBIOS 0\.4' $S && ok "HBLOG enseña el registro de la BIOS" || ko "HBLOG no funciona"
  grep -aq 'tics dados sin IRQ: [0-9]* (ahora la IRQ 0 llega)' $S && ok "HBLOG: la IRQ 0 llega" || ko "HBLOG: la IRQ 0 no llega"
  grep -aq 'Pantalla: dibujada [0-9]* veces; el ultimo dibujo, [0-9]* us' $S && ok "HBLOG: las cuentas de la pantalla" || ko "HBLOG sin las cuentas de la pantalla"
  mtype -i $T/rapida.img@@1048576 ::HBLOG.TXT 2>/dev/null | grep -q 'lo apuntado por HispaBIOS' && ok "HBLOG guarda \\HBLOG.TXT" || ko "HBLOG no guarda \\HBLOG.TXT"
  grep -aq 'ACPI: RSDP en 000[0-9A-F]*h' $S && ok "EQUIPO encuentra la RSDP de ACPI (en la EBDA de HispaBIOS)" || ko "EQUIPO no encuentra ACPI"
  grep -aq 'SMBIOS [0-9]\.[0-9]* (tablas en' $S && ok "EQUIPO encuentra SMBIOS (en la EBDA de HispaBIOS)" || ko "EQUIPO no encuentra SMBIOS"
  grep -aq 'no se ha podido poner el modo gr' $S && ko "Hispa Ventanas no puede poner el modo gráfico" || ok "Hispa Ventanas no se queja del modo gráfico"
  if command -v convert > /dev/null; then
    c=$(hb_colores $T/pantalla_rapida.ppm)
    [ "$c" -ge 8 ] && ok "Hispa Ventanas se ve en la pantalla ($c colores en la captura)" || ko "la captura no es Hispa Ventanas ($c colores)"
  fi
  grep -aq 'PILA-MAL' $S && ko "llamadas al firmware con la pila sin alinear" || ok "la pila, alineada"
  n2=$(grep -ac 'BdsDxe: loading' $S)
  [ "$n2" -le 1 ] && ok "no se ha reiniciado" || ko "se ha reiniciado ($n2 arranques)"
else
  ko "no se ha podido ensamblar la variante de la pantalla"
fi

# Una pantalla lenta de verdad no se puede tener en QEMU: PRUEBA_LENTA hace
# como si pintar un millón de puntos costara 100 ms (1920x1200: 230 ms). Y
# VBEPRU.COM (tests/progs/vbepru.asm) hace las llamadas de VBE que hace Hispa
# Ventanas, pone 800x600 de 32 bits, escribe y lee el marco lineal, pide un
# modo de la VGA que no hay (12h) y vuelve al texto; HBLOG tiene que traer
# cada llamada con lo contestado.
echo "== HispaBIOS: una pantalla lenta (simulada): modo más pequeño del firmware y registro de VBE =="
if hb_variante lenta "-DPRUEBA_PILA -DREGISTRO_PASOS -DPRUEBA_LENTA=100000"; then
  printf 'VBEPRU\r\nHBLOG\r\n' > $T/VP.BAT
  if nasm -f bin -o $T/VBEPRU.COM tests/progs/vbepru.asm; then
    mcopy -o -i $T/lenta.img@@1048576 $T/VBEPRU.COM ::VBEPRU.COM
    mcopy -o -i $T/lenta.img@@1048576 $T/VP.BAT ::VP.BAT
    HB_MEM=2G HB_MAQ="-machine q35" HB_VGA="-vga none -device VGA,xres=1920,yres=1200,vgamem_mb=32" \
      arranca 64 $T/lenta.img lenta "v p ret" "fin ---"
    S=$T/serie_lenta.txt
    L=$(hb_log lenta)
    echo "$L" | grep -q 'Pantalla lenta: se pasa a un modo mas pequeno del firmware: 1152x864' && ok "con la pantalla lenta pasa a un modo más pequeño del firmware (1152x864)" || ko "no cambia de modo con la pantalla lenta"
    echo "$L" | grep -q 'Texto: ampliado a toda la pantalla (1152x720)' && ok "y amplía el texto a la pantalla nueva" || ko "no amplía el texto a la pantalla nueva"
    grep -aq 'VBEPRU-BIEN' $S && ok "VBE: 4F00h, 4F01h, 4F02h y 4F03h contestan lo que pide Hispa Ventanas; el marco lineal se escribe y se lee" || ko "VBE no contesta bien ($(grep -a 'VBEPRU' $S | head -1))"
    grep -aq 'INT 10h AX=4F00 BX=[0-9A-F]* CX=[0-9A-F]* -> AX=004F' $S && grep -aq 'INT 10h AX=4F02 BX=4115 CX=[0-9A-F]* -> AX=004F' $S && ok "HBLOG: las llamadas de VBE y lo contestado" || ko "HBLOG sin las llamadas de VBE"
    grep -aq 'INT 10h AH=00 AL=12: ese modo de la VGA no lo hay' $S && ok "HBLOG: el modo de la VGA que no hay (12h), apuntado" || ko "HBLOG sin el modo 12h"
    grep -aq 'PILA-MAL' $S && ko "llamadas al firmware con la pila sin alinear" || ok "la pila, alineada"
  else
    ko "no se ha podido ensamblar tests/progs/vbepru.asm"
  fi
else
  ko "no se ha podido ensamblar la variante de la pantalla lenta"
fi

# ---- El reloj de DOS (IRQ 0) en los equipos con el 8254 parado ----
# En un portátil con Core de 11.ª generación (Monovate 14.1, i3-1115G4) el
# firmware deja el 8254 (PIT) parado («clock gating») y la IRQ 0 no llegaba:
# Hispa-DOS se quedaba a la mitad del arranque (EQUIPO: «el reloj no avanza»).
# HispaBIOS 0.4 mira si el 8254 cuenta, le quita el bloqueo en el PCH de Intel
# (eso no se puede probar en QEMU: no tiene P2SB; aquí se ve que lo intenta y
# lo apunta) y la BIOS PRUEBA cada fuente con las interrupciones abiertas: el
# 8254, el HPET en su sitio («legacy replacement») y el reloj CMOS (IRQ 8), con
# el 8259 por LINT0 o por la entrada 0 del IOAPIC. QEMU sin 8254: -machine
# pit=off (con la 0.3 se quedaba parado tras «HSPJOY»).
echo "== HispaBIOS: equipo sin 8254 (pit=off) con HPET: el reloj de DOS sale del HPET =="
if hb_variante sinpit "-DPRUEBA_PILA -DREGISTRO_PASOS" && HB_MEM=1G HB_MAQ="-machine q35,pit=off" arranca 64 $T/sinpit.img sinpit "h b l o g ret w3 d i r ret" "bytes libres"; then
  S=$T/serie_sinpit.txt
  L=$(hb_log sinpit)
  hb_reloj 'Reloj 8254 (PIT), canal 0: NO CUENTA' "ve que el 8254 no cuenta" "no ve que el 8254 está parado"
  hb_reloj 'Bloqueo del 8254 (clock gating) del PCH: el puente 00:1F.0 es [0-9A-F]*8086; P2SB (00:1F.1): identificador FFFFFFFF.*no aparece' "busca el P2SB del PCH para quitar el bloqueo (QEMU no lo tiene) y lo apunta" "no busca el P2SB"
  hb_reloj 'HPET: en 00000000FED00000, identificador 8086A201; capacidades' "HISPABIOS.LOG: el HPET de la tabla de ACPI" "HISPABIOS.LOG sin el HPET"
  hb_reloj 'APIC local: IA32_APIC_BASE .*LINT0 [0-9A-F]*' "HISPABIOS.LOG: el APIC local (LINT0)" "HISPABIOS.LOG sin el APIC local"
  hb_reloj 'Tabla APIC de ACPI: banderas .*IOAPIC en FEC00000 .*la IRQ 0 va a la entrada 2; el IOAPIC tiene la entrada 0 = ' "HISPABIOS.LOG: el IOAPIC y a dónde va la IRQ 0" "HISPABIOS.LOG sin el IOAPIC"
  hb_reloj 'la IRQ 0 del 8254 (PIT), con el 8259 por LINT0 del APIC local: interrupciones 0; 8259 IRR=' "la BIOS prueba el 8254 y ve que no interrumpe (con el estado del 8259)" "la BIOS no prueba el 8254"
  hb_reloj 'el HPET en el sitio del 8254 (IRQ 0), con el 8259 por LINT0 del APIC local: interrupciones 2;.*HPET: estado 1 ' "pone el HPET en el sitio del 8254 y las interrupciones llegan" "el HPET no da la IRQ 0"
  hb_reloj 'Reloj de DOS: LLEGA. Queda: el HPET en el sitio del 8254' "HISPABIOS.LOG: el reloj queda en el HPET" "el reloj no queda en el HPET"
  grep -aq 'bytes libres' $S && ok "arranca hasta C:\\> (el menú de 5 s y las esperas pasan solos) y DIR responde" || ko "no llega a C:\\> sin el 8254"
  grep -aq 'tics dados sin IRQ: [0-9]* (ahora la IRQ 0 llega)' $S && ok "HBLOG: la IRQ 0 llega" || ko "HBLOG: la IRQ 0 no llega"
  grep -aq 'Temporizador 8254 (PIT): PARADO' $S && ok "HBLOG dice que el 8254 está parado" || ko "HBLOG no dice que el 8254 está parado"
  grep -aq 'PILA-MAL' $S && ko "llamadas al firmware con la pila sin alinear" || ok "la pila, alineada"
else
  ko "no se ha podido ensamblar o arrancar la variante sin 8254"
fi

# (el camino de quitar el bloqueo, con un P2SB de mentira: oculto, se muestra,
# se lee SBREG_BAR, se quita el bit 2 de ITSSPRC y se vuelve a ocultar; y el
# modo seguro, que no toca nada de eso y se queda con el reloj CMOS)
echo "== HispaBIOS: quitar el bloqueo del 8254 (P2SB de mentira) y el modo seguro sin 8254 =="
if hb_variante p2sb "-DPRUEBA_PILA -DREGISTRO_PASOS -DPRUEBA_P2SB" && HB_MEM=1G HB_MAQ="-machine q35,pit=off" HB_HASTA="Hispa-DOS 0\.9" arranca 64 $T/p2sb.img p2sb "" "nada"; then
  L=$(hb_log p2sb)
  hb_reloj 'P2SB (00:1F.1): identificador FFFFFFFF, registro E0h FFFFFFFF; al mostrarlo, A0A08086; SBREG_BAR [0-9A-F]*; ITSSPRC (puerto C4h, 3300h) = 00000007 con el bit 2 (8254CGE) puesto: se quita y queda 00000003' "muestra el P2SB oculto, lee SBREG_BAR y quita el bit 8254CGE de ITSSPRC" "no quita el bloqueo del 8254"
  hb_reloj '(prueba) P2SB de mentira: oculto = 1, cambios de ocultacion = 2, ITSSPRC = 00000003' "y deja el P2SB oculto otra vez" "no vuelve a ocultar el P2SB"
  hb_reloj 'Reloj 8254 (PIT) despues de quitar el bloqueo: ' "vuelve a mirar si el 8254 cuenta" "no vuelve a mirar el 8254"
else
  ko "no se ha podido ensamblar o arrancar la variante del P2SB"
fi
if hb_variante segrel "-DPRUEBA_PILA -DREGISTRO_PASOS -DPRUEBA_SEGURO" && HB_MEM=1G HB_MAQ="-machine q35,pit=off" arranca 64 $T/segrel.img segrel "d i r ret" "bytes libres"; then
  S=$T/serie_segrel.txt
  L=$(hb_log segrel)
  hb_reloj 'Reloj: modo seguro, no se toca el PCH, el HPET ni el IOAPIC' "modo seguro: no toca el PCH, el HPET ni el IOAPIC" "el modo seguro no se nota en el reloj"
  echo "$L" | grep -q 'HPET en el sitio\|por la entrada 0 del IOAPIC\|Bloqueo del 8254' && ko "el modo seguro prueba el HPET, el IOAPIC o el PCH" || ok "modo seguro: no prueba el HPET ni el IOAPIC"
  hb_reloj 'Reloj de DOS: LLEGA. Queda: el reloj CMOS' "modo seguro sin 8254: el reloj CMOS" "modo seguro sin 8254: sin reloj"
  grep -aq 'bytes libres' $S && ok "modo seguro sin 8254: llega a C:\\> y DIR responde" || ko "modo seguro sin 8254: no llega a C:\\>"
else
  ko "no se ha podido ensamblar o arrancar la variante del modo seguro"
fi

echo "== HispaBIOS: equipo sin 8254 ni HPET (pit=off, hpet=off): el reloj de DOS sale del reloj CMOS (IRQ 8) =="
if hb_variante sinhpet "-DPRUEBA_PILA -DREGISTRO_PASOS" && HB_MEM=1G HB_MAQ="-machine q35,pit=off,hpet=off" arranca 64 $T/sinhpet.img sinhpet "h b l o g ret w3 d i r ret" "bytes libres"; then
  S=$T/serie_sinhpet.txt
  L=$(hb_log sinhpet)
  hb_reloj 'HPET: la tabla de ACPI no esta' "ve que no hay HPET" "no ve que falta el HPET"
  hb_reloj 'el reloj CMOS (IRQ 8, 64 Hz), con el 8259 por LINT0 del APIC local: interrupciones 2;.*CMOS: A=[0-9A-F]* B=[0-9A-F]*, IRQ 8 recibidas' "el reloj CMOS da los tics por la IRQ 8" "el reloj CMOS no da los tics"
  hb_reloj 'Reloj de DOS: LLEGA. Queda: el reloj CMOS' "HISPABIOS.LOG: el reloj queda en el reloj CMOS" "el reloj no queda en el reloj CMOS"
  grep -aq 'bytes libres' $S && ok "arranca hasta C:\\> y DIR responde" || ko "no llega a C:\\> sin 8254 ni HPET"
  grep -aq 'tics dados sin IRQ: [0-9]* (ahora la IRQ 0 llega)' $S && ok "HBLOG: los tics llegan por interrupción" || ko "HBLOG: los tics no llegan"
else
  ko "no se ha podido ensamblar o arrancar la variante sin 8254 ni HPET"
fi

# (PRUEBA_RELOJ=15h: no se prueba nada por LINT0; quedan los tres caminos por el
# IOAPIC, que en QEMU no llevan el 8259: se ve que se ponen, se quitan y no
# rompen nada, y el aviso de que no hay reloj. El registro se guarda aunque la
# imagen no sea la de prueba.)
echo "== HispaBIOS: ninguna fuente da la IRQ 0 (simulado): prueba el IOAPIC, avisa y guarda el registro =="
if hb_variante sinreloj "-DPRUEBA_PILA -DPRUEBA_RELOJ=15h" && HB_MEM=1G HB_MAQ="-machine q35" arranca 64 $T/sinreloj.img sinreloj "d i r ret" "bytes libres"; then
  S=$T/serie_sinreloj.txt
  L=$(hb_log sinreloj)
  [ "$(echo "$L" | grep -c 'con el 8259 por la entrada 0 del IOAPIC: interrupciones 0')" = 3 ] && ok "prueba el 8254, el HPET y el reloj CMOS con el 8259 por el IOAPIC" || ko "no prueba los tres por el IOAPIC"
  hb_reloj 'Reloj de DOS: NO LLEGA ninguna interrupcion' "dice que el reloj no llega y guarda HISPABIOS.LOG sin ser la imagen de prueba" "no guarda el registro cuando el reloj no llega"
  grep -aq 'bytes libres' $S && ok "sigue hasta C:\\> (el HPET y el IOAPIC quedan como estaban)" || ko "no llega a C:\\> después de probar el IOAPIC"
  grep -aq 'PILA-MAL' $S && ko "llamadas al firmware con la pila sin alinear" || ok "la pila, alineada"
else
  ko "no se ha podido ensamblar o arrancar la variante sin reloj"
fi

# ---- La parte de 32 bits (firmware UEFI de 32: Atom «Bay Trail», Acer One 10) ----
. tests/hispabios32_casos.sh

exit $fail
