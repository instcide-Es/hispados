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
  $q -m 512 -smp 2 -display none -monitor unix:$T/mon_$nombre,server,nowait \
    -serial file:$T/serie_$nombre.txt \
    -drive if=pflash,format=raw,readonly=on,file=$code \
    -drive if=pflash,format=raw,file=$T/vars_$nombre.fd \
    -netdev user,id=n0 -device e1000,netdev=n0 \
    -drive file=$img,format=raw,if=none,id=u -device qemu-xhci -device usb-storage,drive=u,bootindex=1 &
  local pid=$! i
  for i in $(seq 1 240); do
    sleep 1
    grep -aq 'C:\\>' $T/serie_$nombre.txt 2>/dev/null && break
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

exit $fail
