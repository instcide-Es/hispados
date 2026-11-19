# Casos de HispaBIOS: el ratón con UEFI (0.5) y los discos de efi64.asm.
# (Lo incluye tests/run_hispabios_tests.sh, con sus ayudas: arranca, hb_variante,
# hb_log, ok, ko. A solas: HB_SOLO=raton tests/run_hispabios_tests.sh.)
#
# Arrancando por UEFI no hay ratón PS/2: en el ThinkPad E14 y en el MacBook Air
# de 2017 Hispa Ventanas se quedaba sin ratón. HispaBIOS 0.5 busca el ratón del
# firmware (Simple Pointer, Absolute Pointer) y, si el firmware no lleva el
# ratón USB (OVMF no lo lleva: es lo que se prueba aquí), lo lee ella por
# EFI_USB_IO; la BIOS lo da por INT 15h C2xxh y HSPRATON lo coge por la «vía
# BIOS». QEMU: un usb-mouse en el mismo xHCI que la memoria USB; el monitor lo
# mueve (mouse_move) y pulsa (mouse_button): es el ratón «actual» de QEMU por
# ser el último que se ha puesto (el PS/2 de QEMU no se mueve).

echo "== HispaBIOS: ratón USB (usb-mouse en el xHCI) por INT 15h C2xxh: HSPRATON, MOUSETST y HBLOG =="
if hb_variante rat64 "-DPRUEBA_PILA -DREGISTRO_PASOS"; then
  mcopy -o -i $T/rat64.img@@1048576 $B/MOUSETST.COM ::MOUSETST.COM
  HB_EXTRA="-device usb-mouse" arranca 64 $T/rat64.img rat64 \
    "m o u s e t s t ret w3 m:mouse_move+20+10 w1 m:mouse_move+20+10 w1 m:mouse_button+1 w1 m:mouse_button+0 w1 m:mouse_button+2 w1 m:mouse_button+0 w1 spc w2 h b l o g ret" "fin ---"
  S=$T/serie_rat64.txt
  L=$(hb_log rat64)
  echo "$L" | grep -q 'Controladora USB [0-9A-F]*:[0-9A-F]* (clase 0C03h): se pide al firmware que la ponga en marcha. Resultado 00000000' && ok "pide al firmware que ponga en marcha la controladora USB" || ko "no conecta la controladora USB"
  echo "$L" | grep -q '(el de la consola, que reune a los demas, no se usa)' && ok "no usa el Simple Pointer de la consola (el que los reúne, que existe siempre)" || ko "no aparta el Simple Pointer de la consola"
  echo "$L" | grep -q 'USB: interfaz HID de raton n.0, subclase 01h: la lleva HispaBIOS; punto final 8[0-9A-F]h, informes de [3-8] octetos .*SET_PROTOCOL (arranque): 00000000, SET_IDLE: [0-9A-F]*, transferencia asincrona: 00000000' && ok "HISPABIOS.LOG: el ratón USB de arranque, en marcha por EFI_USB_IO (transferencia asíncrona)" || ko "HISPABIOS.LOG sin el ratón USB"
  grep -aq 'a BIOS (INT 15h). Rat' $S && ok "HSPRATON coge el ratón por la BIOS (INT 15h C2xxh)" || ko "HSPRATON no coge el ratón por la BIOS"
  grep -aq 'X=340 Y=105 B=0' $S && grep -aq 'X=360 Y=110 B=0' $S && ok "MOUSETST ve moverse el ratón USB (INT 33h: 320,100 -> 340,105 -> 360,110)" || ko "MOUSETST no ve moverse el ratón USB ($(grep -a 'X=' $S | tr -d '\r' | tr '\n' ' '))"
  grep -aq 'X=360 Y=110 B=1' $S && grep -aq 'X=360 Y=110 B=2' $S && ok "MOUSETST ve los botones izquierdo y derecho" || ko "MOUSETST no ve los botones"
  grep -aq 'Raton (INT 15h C2xxh): USB (lo lee HispaBIOS); activado; lecturas del firmware: 0; informes USB: [1-9][0-9]*; paquetes al programa: [1-9][0-9]*' $S && ok "HBLOG: el ratón USB, activado, con sus informes y paquetes" || ko "HBLOG sin el ratón ($(grep -a 'Raton (INT' $S | tr -d '\r'))"
  grep -aq 'PILA-MAL' $S && ko "llamadas al firmware con la pila sin alinear ($(grep -ac 'PILA-MAL' $S))" || ok "la pila, alineada (también en las llamadas del ratón)"
  n2=$(grep -ac 'BdsDxe: loading' $S)
  [ "$n2" -le 1 ] && ok "no se ha reiniciado" || ko "se ha reiniciado ($n2 arranques)"
else
  ko "no se ha podido ensamblar la variante del ratón"
fi

# Hispa Ventanas (800x600 en la pantalla de 1280x800 del firmware): el puntero
# se mueve con el ratón USB (dos capturas distintas) y un doble clic en el
# grupo «Juegos» del Centro de programas abre su ventana (la captura cambia
# mucho). El puntero empieza en el centro: primero se lleva a la esquina.
echo "== HispaBIOS: Hispa Ventanas con el ratón USB: el puntero se mueve y el doble clic abre un grupo =="
if hb_variante ratven "-DPRUEBA_PILA"; then
  esq=""; for i in $(seq 1 16); do esq="$esq m:mouse_move+-60+-60"; done
  ir=""; for i in $(seq 1 10); do ir="$ir m:mouse_move+31+13"; done
  HB_MEM=2G HB_MAQ="-machine q35" HB_EXTRA="-device usb-mouse" arranca 64 $T/ratven.img ratven \
    "v e n t a n a s ret w15 m:screendump+$T/ratven1.ppm w1 $esq w1 $ir w2 m:screendump+$T/ratven2.ppm w1 m:mouse_button+1 m:mouse_button+0 m:mouse_button+1 m:mouse_button+0 w4" "nada"
  S=$T/serie_ratven.txt
  grep -aq 'no se ha podido poner el modo gr' $S && ko "Hispa Ventanas no puede poner el modo gráfico" || ok "Hispa Ventanas arranca en VBE"
  if command -v compare > /dev/null && [ -f $T/ratven1.ppm ] && [ -f $T/ratven2.ppm ] && [ -f $T/pantalla_ratven.ppm ]; then
    d1=$(compare -metric AE $T/ratven1.ppm $T/ratven2.ppm null: 2>&1 | cut -d' ' -f1)
    d2=$(compare -metric AE $T/ratven2.ppm $T/pantalla_ratven.ppm null: 2>&1 | cut -d' ' -f1)
    [ "${d1%.*}" -ge 40 ] 2>/dev/null && ok "el puntero de Hispa Ventanas se mueve con el ratón USB ($d1 puntos distintos)" || ko "el puntero no se mueve ($d1 puntos distintos)"
    [ "${d2%.*}" -ge 20000 ] 2>/dev/null && ok "el doble clic en «Juegos» abre su ventana ($d2 puntos distintos)" || ko "el doble clic no abre la ventana de «Juegos» ($d2 puntos distintos)"
  else
    echo "  (sin ImageMagick o sin capturas: no se comparan)"
  fi
  grep -aq 'PILA-MAL' $S && ko "llamadas al firmware con la pila sin alinear" || ok "la pila, alineada"
else
  ko "no se ha podido ensamblar la variante de Hispa Ventanas con ratón"
fi

# Lo mismo con el firmware de 32 bits (procesador de 64: los Atom «Bay Trail»)
echo "== HispaBIOS de 32 bits: ratón USB por INT 15h C2xxh (MOUSETST) =="
if hb_variante rat32 "-DREGISTRO_PASOS" 32; then
  mcopy -o -i $T/rat32.img@@1048576 $B/MOUSETST.COM ::MOUSETST.COM
  HB_EXTRA="-device usb-mouse" arranca a32 $T/rat32.img rat32 \
    "m o u s e t s t ret w3 m:mouse_move+20+10 w1 m:mouse_move+20+10 w1 m:mouse_button+1 w1 m:mouse_button+0 w1 spc w2 h b l o g ret" "fin ---"
  S=$T/serie_rat32.txt
  L=$(hb_log rat32)
  echo "$L" | grep -q 'Controladora USB [0-9A-F]*:[0-9A-F]* (clase 0C03h): se pide al firmware que la ponga en marcha. Resultado 00000000' && ok "32 bits: pide al firmware que ponga en marcha la controladora USB" || ko "32 bits: no conecta la controladora USB"
  echo "$L" | grep -q 'USB: interfaz HID de raton n.0, subclase 01h: la lleva HispaBIOS; .*transferencia asincrona: 00000000' && ok "32 bits: el ratón USB, en marcha por EFI_USB_IO" || ko "32 bits: HISPABIOS.LOG sin el ratón USB"
  grep -aq 'a BIOS (INT 15h). Rat' $S && ok "32 bits: HSPRATON coge el ratón por la BIOS" || ko "32 bits: HSPRATON no coge el ratón por la BIOS"
  grep -aq 'X=340 Y=105 B=0' $S && grep -aq 'X=360 Y=110 B=1' $S && ok "32 bits: MOUSETST ve moverse el ratón y el botón" || ko "32 bits: MOUSETST no ve el ratón ($(grep -a 'X=' $S | tr -d '\r' | tr '\n' ' '))"
  grep -aq 'Raton (INT 15h C2xxh): USB (lo lee HispaBIOS); activado; .*informes USB: [1-9]' $S && ok "32 bits: HBLOG con el ratón USB" || ko "32 bits: HBLOG sin el ratón"
else
  ko "no se ha podido ensamblar la variante de 32 bits del ratón"
fi

# Sin ratón USB (lo de siempre en QEMU: solo el PS/2 de su 8042): INT 15h C2xxh
# dice que no hay y HSPRATON va directo al 8042, como antes de la 0.5.
echo "== HispaBIOS: sin ratón USB, INT 15h C2xxh dice que no y HSPRATON usa el 8042 =="
if hb_variante sinrat "-DREGISTRO_PASOS"; then
  arranca 64 $T/sinrat.img sinrat "h b l o g ret" "fin ---"
  S=$T/serie_sinrat.txt
  hb_log sinrat | grep -q 'No hay raton: ni el firmware da uno ni hay un raton USB de arranque' && ok "HISPABIOS.LOG: no hay ratón" || ko "HISPABIOS.LOG no dice que no hay ratón"
  grep -aq 'a BIOS (INT 15h). Rat' $S && ko "HSPRATON coge un ratón de la BIOS que no hay" || ok "HSPRATON no usa la BIOS (no hay ratón en INT 15h C2xxh)"
  grep -aq 'Raton (INT 15h C2xxh): no hay' $S && ok "HBLOG: no hay ratón" || ko "HBLOG no dice que no hay ratón"
else
  ko "no se ha podido ensamblar la variante sin ratón"
fi

# Los discos en efi64.asm (lo que ya hacía la de 32): las controladoras de disco
# se ponen en marcha, la memoria USB es el 80h, un disco SATA de 64 MB el 81h
# (D:, se lee y se escribe) y un disco fijo de 4 MB no se da a DOS.
echo "== HispaBIOS (x64): la memoria USB (80h) y el disco de dentro (81h, D:); un disco de 4 MB no se da =="
if hb_variante discos64 "-DPRUEBA_PILA -DREGISTRO_PASOS" && command -v sfdisk > /dev/null; then
  truncate -s 64M $T/dentro64.img
  printf 'label: dos\nstart=2048, type=6, bootable\n' | sfdisk -q $T/dentro64.img > /dev/null 2>&1
  mformat -i $T/dentro64.img@@1048576 -v DENTRO -h 16 -s 63 -t 128 :: 2> /dev/null
  echo "archivo del disco de dentro" > $T/DENTRO.TXT
  mcopy -i $T/dentro64.img@@1048576 $T/DENTRO.TXT ::
  truncate -s 4M $T/chico64.img
  HB_MEM=1G HB_MAQ="-machine q35" \
    HB_EXTRA="-drive file=$T/chico64.img,format=raw,if=none,id=c -device ide-hd,drive=c,bus=ide.0 -drive file=$T/dentro64.img,format=raw,if=none,id=e -device ide-hd,drive=e,bus=ide.1" \
    arranca 64 $T/discos64.img discos64 "t y p e spc d shift-semicolon d e n t r o dot t x t ret w3 c o p y spc c shift-semicolon backslash c o n f i g dot s y s spc d shift-semicolon ret w4 d i r spc d shift-semicolon ret" "2 archivo"
  S=$T/serie_discos64.txt
  L=$(hb_log discos64)
  echo "$L" | grep -q 'Controladora de disco 8086:[0-9A-F]* (clase 0106h): se pide al firmware que la ponga en marcha. Resultado 00000000' && ok "pide al firmware que ponga en marcha la controladora SATA" || ko "no conecta la controladora de disco"
  echo "$L" | grep -q '(512 MB), fijo, EL DE ARRANQUE: disco 80h de DOS' && ok "la memoria USB de arranque es el disco 80h" || ko "el disco de arranque no es el 80h"
  echo "$L" | grep -q '00020000h sectores de 512 octetos (64 MB), fijo: disco 81h de DOS' && ok "el disco de dentro (64 MB) es el 81h" || ko "el disco de dentro no es el 81h"
  echo "$L" | grep -q '(4 MB), fijo: no se da a DOS' && ok "el disco fijo de 4 MB no se da a DOS" || ko "el disco de 4 MB se da a DOS"
  echo "$L" | grep -q 'discos: 2$' && ok "DOS tiene dos discos" || ko "DOS no tiene dos discos"
  echo "$L" | grep -q '^Equipo: QEMU .*; BIOS .*EDK II' && ok "HISPABIOS.LOG: el equipo y su BIOS (SMBIOS)" || ko "HISPABIOS.LOG sin el equipo"
  echo "$L" | grep -q 'Memoria extendida: [0-9]* KB' && ! echo "$L" | grep -q 'Memoria extendida: 4096 KB' && ok "la memoria extendida, según el mapa ($(echo "$L" | grep -o 'Memoria extendida: [0-9]* KB'); antes, 4096 KB)" || ko "la memoria extendida no sale del mapa"
  grep -aq 'archivo del disco de dentro' $S && ok "DOS lee D: (INT 13h del disco 81h por Block I/O)" || ko "DOS no lee D:"
  grep -aq '2 archivo' $S && mdir -i $T/dentro64.img@@1048576 :: 2>/dev/null | grep -qi 'CONFIG *SYS' && ok "DOS escribe en D: (el archivo copiado está en la imagen)" || ko "DOS no escribe en D:"
  grep -aq 'PILA-MAL' $S && ko "llamadas al firmware con la pila sin alinear" || ok "la pila, alineada"
else
  ko "no se ha podido preparar la prueba de los discos de efi64"
fi
