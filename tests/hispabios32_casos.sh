# Pruebas de la parte de 32 bits de HispaBIOS (hispabios/efi32.asm: firmware
# UEFI de 32 bits). Lo lee tests/run_hispabios_tests.sh (que define arranca,
# hb_variante, hb_log, hb_colores, hb_reloj, ok y ko); a solas:
#     HB_SOLO=32 tests/run_hispabios_tests.sh
# El equipo que se imita es una tableta o un 2 en 1 con Atom «Bay Trail» (Acer
# One 10): procesador de 64 bits con firmware de 32 (arranca a32: QEMU de 64
# con OVMF de 32), 1 GB de memoria, pantalla de 1280x800 —o un panel vertical
# de 800x1280 puesto de lado—, la memoria USB y, además, el disco de dentro.
# Y un equipo de 32 bits de verdad (arranca 32: procesador de 32).
if [ ! -f /usr/share/OVMF/OVMF32_CODE_4M.fd ]; then
  echo "== HispaBIOS de 32 bits: (falta el paquete ovmf-ia32: se salta) =="
  return 0 2>/dev/null || exit 0
fi
HB_P1280="-vga none -device VGA,xres=1280,yres=800,vgamem_mb=32"
HB_P800V="-vga none -device VGA,xres=800,yres=1280,vgamem_mb=32"

echo "== HispaBIOS de 32 bits: como un Acer One 10 (procesador de 64 bits, firmware de 32, 1 GB, 1280x800) =="
if hb_variante acer "-DREGISTRO_PASOS" 32; then
  HB_MEM=1G HB_MAQ="-machine q35" HB_VGA="$HB_P1280" \
    arranca a32 $T/acer.img acer "d i r spc c shift-semicolon backslash h i s p a d o s ret w6 h b l o g ret w3 e q u i p o ret w25 v e n t a n a s ret w14" "cargando el servidor DPMI"
  S=$T/serie_acer.txt
  L=$(hb_log acer)
  echo "$L" | grep -q '^\[registro de HispaBIOS 0\.[0-9]* IA32\]' && echo "$L" | grep -q '^Firmware: .*EDK II.*UEFI 2\.' && ok "HISPABIOS.LOG (32 bits): el fabricante y la versión del firmware" || ko "HISPABIOS.LOG sin el firmware"
  echo "$L" | grep -q '^Mapa de memoria: [0-9]* entradas de [0-9]* octetos' && [ "$(echo "$L" | grep -c ' atr\. ')" -ge 20 ] && ok "HISPABIOS.LOG: el mapa de memoria completo ($(echo "$L" | grep -c ' atr\. ') tramos)" || ko "HISPABIOS.LOG sin el mapa completo"
  echo "$L" | grep -q 'Memoria del equipo: 102[0-9] MB' && ok "lee el mapa de memoria entero (1 GB)" || ko "no lee el mapa de 1 GB"
  echo "$L" | grep -q 'se prestan a DOS (se cambian en cada viaje): [0-9A-F]\{5\} ' && echo "$L" | grep -q 'La BIOS de modo real va en 90000h. Memoria convencional para DOS: 576 KB' && ok "presta a DOS la página suelta del firmware: 576 KB" || ko "no presta la página del firmware (o no da 576 KB)"
  grep -aq '576 KB convencional' $S && ok "DOS ve 576 KB de memoria convencional" || ko "DOS no ve 576 KB"
  echo "$L" | grep -q 'Pantalla (GOP): 1280x800' && echo "$L" | grep -q 'Giro de la imagen: ninguno; DOS ve una pantalla de 1280x800' && ok "la pantalla es de 1280x800, apaisada: sin giro" || ko "la pantalla no es de 1280x800 sin giro"
  echo "$L" | grep -q 'Modos de pantalla del firmware (GOP; \* el puesto): .*1280x800\*' && ok "HISPABIOS.LOG: los modos de pantalla del firmware y el puesto" || ko "HISPABIOS.LOG sin los modos de pantalla"
  echo "$L" | grep -q 'Cache de la pantalla: MTRR por omision [0-9A-F]*h; la pantalla, en .*; PAT [0-9A-F]\{16\}' && ok "HISPABIOS.LOG: MTRR y PAT de la pantalla" || ko "HISPABIOS.LOG sin MTRR/PAT"
  echo "$L" | grep -q 'Paginacion del firmware: CR0 [0-9A-F]\{8\}, CR3 [0-9A-F]\{8\}, CR4 [0-9A-F]\{8\} (...' && ok "HISPABIOS.LOG: cómo tiene el firmware la paginación" || ko "HISPABIOS.LOG sin la paginación del firmware"
  echo "$L" | grep -q 'Escritura combinada (WC): tablas de paginas propias (PAE, de 2 MB) en [0-9A-F]\{8\}; la pantalla, entradas [0-9]* a [0-9]*; PAT nuevo [0-9A-F]\{4\}01' && ok "tablas de páginas propias con la pantalla en escritura combinada (PAT 5 = WC)" || ko "sin las tablas de escritura combinada"
  echo "$L" | grep -q 'Pintar la pantalla entera: [0-9]* us escribiendo directamente, [0-9]* us con las tablas propias (WC). Se pinta con las tablas propias' && ok "mide lo que cuesta pintar la pantalla y pinta con las tablas propias" || ko "no mide la pantalla o no pinta con las tablas propias"
  echo "$L" | grep -q 'Texto: ampliado a toda la pantalla (1280x800)' && ok "el texto, ampliado a toda la pantalla" || ko "el texto no se amplía a 1280x800"
  echo "$L" | grep -q 'VBE: marco lineal para los programas en 00000000[0-9A-F]\{8\} ' && ok "el marco lineal de VBE" || ko "sin marco lineal de VBE"
  echo "$L" | grep -q 'Tablas del equipo: ACPI (RSDP) en [0-9A-F]* (revision [0-9]*); SMBIOS en [0-9A-F]\{8\}' && ok "HISPABIOS.LOG: las tablas de ACPI y SMBIOS del firmware" || ko "HISPABIOS.LOG sin las tablas del equipo"
  echo "$L" | grep -q '^Equipo: QEMU Standard PC .*; BIOS .*EDK II' && grep -aq '^Equipo: QEMU Standard PC' $S && ok "el fabricante y el modelo del equipo (SMBIOS), en la pantalla y en el registro" || ko "sin el modelo del equipo"
  echo "$L" | grep -q '00100000h sectores de 512 octetos (512 MB), .*EL DE ARRANQUE: disco 80h de DOS' && ok "HISPABIOS.LOG: el disco de arranque (la memoria USB), el 80h" || ko "HISPABIOS.LOG sin el disco de arranque"
  echo "$L" | grep -q 'Memoria extendida: 7[0-9]* KB' && ok "la memoria extendida, el tramo libre entero desde 1 MB (7 MB en OVMF; a potencias de dos, 4)" || ko "la memoria extendida no es el tramo entero"
  hb_reloj 'Reloj 8254 (PIT), canal 0: CUENTA' "el reloj: ve que el 8254 cuenta" "no mira si el 8254 cuenta"
  hb_reloj 'HPET: en 00000000FED00000, identificador 8086A201; capacidades' "HISPABIOS.LOG: el HPET de la tabla de ACPI" "HISPABIOS.LOG sin el HPET"
  hb_reloj 'APIC local: IA32_APIC_BASE .*LINT0 [0-9A-F]*' "HISPABIOS.LOG: el APIC local (LINT0)" "HISPABIOS.LOG sin el APIC local"
  hb_reloj 'Tabla APIC de ACPI: banderas .*IOAPIC en FEC00000 .*la IRQ 0 va a la entrada 2; el IOAPIC tiene la entrada 0 = ' "HISPABIOS.LOG: el IOAPIC y a dónde va la IRQ 0" "HISPABIOS.LOG sin el IOAPIC"
  hb_reloj 'Reloj de DOS: LLEGA. Queda: la IRQ 0 del 8254 (PIT), con el 8259 por LINT0' "la BIOS prueba el reloj y lo apunta en HISPABIOS.LOG (PET_REGISTRO)" "sin la prueba del reloj en el registro"
  awk '/HBLOG: registro de la BIOS/ { h = NR } /bytes libres/ { b = NR } END { exit !(h && b && b < h) }' $S && ok "DIR C:\\HISPADOS (75 líneas) acaba en menos de 6 s (el teclado, sin viajes continuos al firmware)" || ko "DIR C:\\HISPADOS tarda más de 6 s"
  grep -aq 'HBLOG: registro de la BIOS de HispaBIOS 0\.5' $S && ok "HBLOG enseña el registro de la BIOS" || ko "HBLOG no funciona"
  grep -aq 'tics dados sin IRQ: [0-9]* (ahora la IRQ 0 llega)' $S && ok "HBLOG: la IRQ 0 llega" || ko "HBLOG: la IRQ 0 no llega"
  grep -aq 'Pantalla: dibujada [0-9]* veces; el ultimo dibujo, [0-9]* us' $S && ok "HBLOG: las cuentas de la pantalla" || ko "HBLOG sin las cuentas de la pantalla"
  grep -aq 'ACPI: RSDP en 000[0-9A-F]*h' $S && ok "EQUIPO encuentra la RSDP de ACPI (en la EBDA de HispaBIOS)" || ko "EQUIPO no encuentra ACPI"
  grep -aq 'SMBIOS [0-9]\.[0-9]* (tablas en' $S && ok "EQUIPO encuentra SMBIOS (en la EBDA de HispaBIOS)" || ko "EQUIPO no encuentra SMBIOS"
  grep -aq 'VESA 2\.0' $S && ok "DOS ve VBE 2.0 (antes, la BIOS de 32 bits decía que no había)" || ko "DOS no ve VBE"
  grep -aq 'no se ha podido poner el modo gr' $S && ko "Hispa Ventanas no puede poner el modo gráfico" || ok "Hispa Ventanas no se queja del modo gráfico"
  if command -v convert > /dev/null; then
    c=$(hb_colores $T/pantalla_acer.ppm)
    [ "$c" -ge 8 ] && ok "Hispa Ventanas se ve en la pantalla ($c colores en la captura)" || ko "la captura no es Hispa Ventanas ($c colores)"
  fi
  n2=$(grep -ac 'BdsDxe: loading' $S)
  [ "$n2" -le 1 ] && ok "no se ha reiniciado" || ko "se ha reiniciado ($n2 arranques)"
else
  ko "no se ha podido ensamblar la variante de 32 bits"
fi

# El panel vertical (800x1280) de una tableta puesto de lado: la imagen se
# gira 90 grados a la derecha y DOS ve 1280x800. La captura de Hispa Ventanas,
# girada de vuelta, tiene que ser la misma que con la pantalla apaisada de la
# prueba anterior (salvo el reloj de la barra).
echo "== HispaBIOS de 32 bits: pantalla vertical de 800x1280 (tableta): la imagen se gira y Hispa Ventanas sale derecha =="
if hb_variante vert "-DREGISTRO_PASOS" 32; then
  HB_MEM=1G HB_MAQ="-machine q35" HB_VGA="$HB_P800V" \
    arranca a32 $T/vert.img vert "v e n t a n a s ret w14" "cargando el servidor DPMI"
  S=$T/serie_vert.txt
  L=$(hb_log vert)
  echo "$L" | grep -q 'Pantalla (GOP): 800x1280' && ok "la pantalla del firmware es de 800x1280 (vertical)" || ko "la pantalla no es de 800x1280"
  echo "$L" | grep -q 'Pantalla vertical (mas alta que ancha). Giro de la imagen: 90 grados a la derecha; DOS ve una pantalla de 1280x800' && ok "la gira 90 grados a la derecha: DOS ve 1280x800" || ko "no gira la pantalla vertical"
  grep -aq 'Giro de la imagen: 90 grados a la derecha' $S && ok "lo dice en la pantalla al arrancar (con las teclas para cambiarlo)" || ko "no dice el giro en la pantalla"
  echo "$L" | grep -q 'Texto: ampliado a toda la pantalla (1280x800)' && ok "el texto, ampliado a 1280x800 (antes del giro)" || ko "el texto no se amplía a 1280x800"
  grep -aq 'no se ha podido poner el modo gr' $S && ko "Hispa Ventanas no puede poner el modo gráfico" || ok "Hispa Ventanas no se queja del modo gráfico"
  if command -v convert > /dev/null && command -v compare > /dev/null; then
    [ "$(convert $T/pantalla_vert.ppm -format '%wx%h' info: 2>/dev/null)" = 800x1280 ] && ok "la captura es de 800x1280" || ko "la captura no es de 800x1280"
    convert $T/pantalla_vert.ppm -rotate -90 $T/vert_derecha.ppm 2>/dev/null
    c=$(hb_colores $T/vert_derecha.ppm)
    [ "$c" -ge 8 ] && ok "Hispa Ventanas se ve en la pantalla girada ($c colores)" || ko "la captura no es Hispa Ventanas ($c colores)"
    if [ -f $T/pantalla_acer.ppm ]; then
      d=$(compare -metric AE $T/pantalla_acer.ppm $T/vert_derecha.ppm null: 2>&1 | sed 's/[^0-9].*//')
      [ -n "$d" ] && [ "$d" -le 3000 ] && ok "girada de vuelta, es la misma imagen que con la pantalla apaisada ($d puntos distintos de 1.024.000: el reloj)" || ko "la imagen girada no es la de la pantalla apaisada (${d:-?} puntos distintos)"
    fi
  fi
  n2=$(grep -ac 'BdsDxe: loading' $S)
  [ "$n2" -le 1 ] && ok "no se ha reiniciado" || ko "se ha reiniciado ($n2 arranques)"
else
  ko "no se ha podido ensamblar la variante de la pantalla vertical"
fi

# El giro guardado (\HBGIRO.CFG) manda sobre el de omisión, y Ctrl+Alt+G lo
# cambia con DOS en marcha, redibuja y lo guarda. Aquí: guardado «3» (90 a la
# izquierda) en una pantalla vertical; Ctrl+Alt+G lo deja en 0 (sin giro).
# Con PRUEBA_SIN_PAG: las tablas propias de escritura combinada se ponen y se
# quitan desde un estado sin paginación ni PAE (un firmware de 32 bits de
# verdad no suele usarla; OVMF sí).
echo "== HispaBIOS de 32 bits: el giro guardado en HBGIRO.CFG y Ctrl+Alt+G =="
if hb_variante giro "-DREGISTRO_PASOS -DPRUEBA_SIN_PAG" 32; then
  printf 3 > $T/HBGIRO.CFG
  mcopy -o -i $T/giro.img@@1048576 $T/HBGIRO.CFG ::HBGIRO.CFG
  HB_MEM=1G HB_MAQ="-machine q35" HB_VGA="$HB_P800V" \
    arranca a32 $T/giro.img giro "ctrl-alt-g w3 v e r ret" "notificada: 6.22"
  S=$T/serie_giro.txt
  L=$(hb_log giro)
  echo "$L" | grep -q 'Giro de la imagen: 90 grados a la izquierda; DOS ve una pantalla de 1280x800 (giro guardado en HBGIRO.CFG)' && ok "al arrancar usa el giro guardado (90 grados a la izquierda)" || ko "no usa el giro guardado"
  [ "$(mtype -i $T/giro.img@@1048576 ::HBGIRO.CFG 2>/dev/null)" = 0 ] && ok "Ctrl+Alt+G da un cuarto de vuelta más y lo guarda (HBGIRO.CFG = 0)" || ko "Ctrl+Alt+G no guarda el giro (HBGIRO.CFG = $(mtype -i $T/giro.img@@1048576 ::HBGIRO.CFG 2>/dev/null))"
  echo "$L" | grep -q 'como en un firmware sin paginacion' && echo "$L" | grep -q 'Se pinta con las tablas propias' && ok "pinta con las tablas propias partiendo de un estado sin paginación" || ko "no pinta con las tablas propias sin paginación"
  grep -aq 'notificada: 6.22' $S && ok "y DOS sigue respondiendo (VER)" || ko "DOS no responde después de Ctrl+Alt+G"
  if command -v convert > /dev/null; then
    # sin giro, la pantalla vertical lleva el texto arriba (800x500) y lo de abajo en negro
    c=$(convert $T/pantalla_giro.ppm -crop 800x300+0+900 +repage -format %k info: 2>/dev/null)
    c2=$(convert $T/pantalla_giro.ppm -crop 800x500+0+390 +repage -format %k info: 2>/dev/null)
    [ "${c:-0}" = 1 ] && [ "${c2:-0}" -ge 2 ] && ok "la pantalla se ha redibujado sin giro (el texto, en el centro; abajo, negro)" || ko "la pantalla no se ha redibujado sin giro ($c y $c2 colores)"
  fi
else
  ko "no se ha podido ensamblar la variante del giro"
fi

# El disco de dentro (la eMMC de la tableta; aquí, un disco SATA de 64 MB con
# una partición FAT) es el segundo disco de DOS, D:. Con «arranque rápido» el
# firmware solo pone en marcha el dispositivo del que arranca: HispaBIOS le pide
# que conecte sus controladoras de disco. Un disco fijo de 4 MB (como las
# particiones de arranque de una eMMC) no se da a DOS.
echo "== HispaBIOS de 32 bits: la memoria USB (80h) y el disco de dentro (81h, D:); un disco de 4 MB no se da =="
if hb_variante discos "-DREGISTRO_PASOS" 32 && command -v sfdisk > /dev/null; then
  truncate -s 64M $T/dentro.img
  printf 'label: dos\nstart=2048, type=6, bootable\n' | sfdisk -q $T/dentro.img > /dev/null 2>&1
  mformat -i $T/dentro.img@@1048576 -v DENTRO -h 16 -s 63 -t 128 :: 2> /dev/null
  echo "archivo del disco de dentro" > $T/DENTRO.TXT
  mcopy -i $T/dentro.img@@1048576 $T/DENTRO.TXT ::
  truncate -s 4M $T/chico.img
  HB_MEM=1G HB_MAQ="-machine q35" HB_VGA="$HB_P1280" \
    HB_EXTRA="-drive file=$T/chico.img,format=raw,if=none,id=c -device ide-hd,drive=c,bus=ide.0 -drive file=$T/dentro.img,format=raw,if=none,id=e -device ide-hd,drive=e,bus=ide.1" \
    arranca a32 $T/discos.img discos "t y p e spc d shift-semicolon d e n t r o dot t x t ret w3 c o p y spc c shift-semicolon backslash c o n f i g dot s y s spc d shift-semicolon ret w4 d i r spc d shift-semicolon ret" "2 archivo"
  S=$T/serie_discos.txt
  L=$(hb_log discos)
  echo "$L" | grep -q 'Controladora de disco 8086:[0-9A-F]* (clase 0106h): se pide al firmware que la ponga en marcha. Resultado 00000000' && ok "pide al firmware que ponga en marcha la controladora SATA" || ko "no conecta la controladora de disco"
  echo "$L" | grep -q '(512 MB), fijo, EL DE ARRANQUE: disco 80h de DOS' && ok "la memoria USB de arranque es el disco 80h" || ko "el disco de arranque no es el 80h"
  echo "$L" | grep -q '00020000h sectores de 512 octetos (64 MB), fijo: disco 81h de DOS' && ok "el disco de dentro (64 MB) es el 81h" || ko "el disco de dentro no es el 81h"
  echo "$L" | grep -q '(4 MB), fijo: no se da a DOS' && ok "el disco fijo de 4 MB no se da a DOS" || ko "el disco de 4 MB se da a DOS"
  echo "$L" | grep -q 'discos: 2$' && ok "DOS tiene dos discos" || ko "DOS no tiene dos discos"
  grep -aq 'archivo del disco de dentro' $S && ok "DOS lee D: (INT 13h del disco 81h por Block I/O)" || ko "DOS no lee D:"
  grep -aq '2 archivo' $S && mdir -i $T/dentro.img@@1048576 :: 2>/dev/null | grep -qi 'CONFIG *SYS' && ok "DOS escribe en D: (el archivo copiado está en la imagen)" || ko "DOS no escribe en D:"
else
  ko "no se ha podido preparar la prueba de los discos"
fi

# Sin 8254 (pit=off), con un P2SB de mentira (el camino de quitar el bloqueo
# del 8254 en un PCH de Intel): la BIOS pone el HPET en el sitio del 8254. Y
# arrancando de un disco fijo (como Hispa-DOS instalado en la eMMC).
echo "== HispaBIOS de 32 bits sin 8254 (pit=off), arrancando de un disco fijo: el bloqueo del PCH y el HPET =="
if hb_variante sp32 "-DREGISTRO_PASOS -DPRUEBA_P2SB" 32 && HB_MEM=1G HB_MAQ="-machine q35,pit=off" HB_ARR=fijo arranca a32 $T/sp32.img sp32 "h b l o g ret w3 d i r ret" "bytes libres"; then
  S=$T/serie_sp32.txt
  L=$(hb_log sp32)
  hb_reloj 'Reloj 8254 (PIT), canal 0: NO CUENTA' "ve que el 8254 no cuenta" "no ve que el 8254 está parado"
  hb_reloj 'P2SB (00:1F.1): identificador FFFFFFFF, registro E0h FFFFFFFF; al mostrarlo, A0A08086; SBREG_BAR [0-9A-F]*; ITSSPRC (puerto C4h, 3300h) = 00000007 con el bit 2 (8254CGE) puesto: se quita y queda 00000003' "muestra el P2SB oculto (de mentira), lee SBREG_BAR y quita el bit 8254CGE" "no quita el bloqueo del 8254"
  hb_reloj '(prueba) P2SB de mentira: oculto = 1, cambios de ocultacion = 2, ITSSPRC = 00000003' "y deja el P2SB oculto otra vez" "no vuelve a ocultar el P2SB"
  hb_reloj 'el HPET en el sitio del 8254 (IRQ 0), con el 8259 por LINT0 del APIC local: interrupciones 2;.*HPET: estado 1 ' "pone el HPET en el sitio del 8254 y las interrupciones llegan" "el HPET no da la IRQ 0"
  hb_reloj 'Reloj de DOS: LLEGA. Queda: el HPET en el sitio del 8254' "HISPABIOS.LOG: el reloj queda en el HPET" "el reloj no queda en el HPET"
  hb_reloj '(512 MB), fijo, EL DE ARRANQUE: disco 80h de DOS' "el disco fijo del que se arranca es el 80h" "el disco fijo de arranque no es el 80h"
  grep -aq 'bytes libres' $S && ok "arranca hasta C:\\> (el menú de 5 s y las esperas pasan solos) y DIR responde" || ko "no llega a C:\\> sin el 8254"
  grep -aq 'tics dados sin IRQ: [0-9]* (ahora la IRQ 0 llega)' $S && ok "HBLOG: la IRQ 0 llega" || ko "HBLOG: la IRQ 0 no llega"
  grep -aq 'Temporizador 8254 (PIT): PARADO' $S && ok "HBLOG dice que el 8254 está parado" || ko "HBLOG no dice que el 8254 está parado"
else
  ko "no se ha podido ensamblar o arrancar la variante sin 8254 (32 bits)"
fi

# El modo seguro (tecla S; aquí, PRUEBA_SEGURO) en un equipo de 32 bits de
# verdad (procesador de 32) sin 8254: ni tablas de escritura combinada, ni
# páginas prestadas, ni conectar controladoras, ni HPET: el reloj CMOS.
echo "== HispaBIOS de 32 bits: el modo seguro, sin 8254 (procesador de 32 bits): el reloj CMOS =="
if hb_variante seg32 "-DREGISTRO_PASOS -DPRUEBA_SEGURO" 32 && HB_MAQ="-machine pc,pit=off" arranca 32 $T/seg32.img seg32 "d i r ret" "bytes libres"; then
  S=$T/serie_seg32.txt
  L=$(hb_log seg32)
  grep -aq 'MODO SEGURO' $S && ok "dice MODO SEGURO en la pantalla" || ko "no dice MODO SEGURO"
  echo "$L" | grep -q 'tablas de paginas propias\|se prestan a DOS\|Controladora de disco\|Bloqueo del 8254\|HPET en el sitio' && ko "el modo seguro hace algo de lo que no debe" || ok "modo seguro: sin tablas propias, sin prestar páginas, sin conectar controladoras, sin PCH ni HPET"
  hb_reloj 'Se pinta directamente' "modo seguro: pinta directamente" "modo seguro: no pinta directamente"
  hb_reloj 'Reloj: modo seguro, no se toca el PCH, el HPET ni el IOAPIC' "modo seguro: no toca el PCH, el HPET ni el IOAPIC" "el modo seguro no se nota en el reloj"
  hb_reloj 'Reloj de DOS: LLEGA. Queda: el reloj CMOS' "modo seguro sin 8254: el reloj CMOS" "modo seguro sin 8254: sin reloj"
  grep -aq 'bytes libres' $S && ok "modo seguro sin 8254: llega a C:\\> y DIR responde" || ko "modo seguro sin 8254: no llega a C:\\>"
else
  ko "no se ha podido ensamblar o arrancar la variante del modo seguro (32 bits)"
fi

# la memoria baja ocupada por el firmware: la BIOS en 70000h; la página suelta
# de debajo (58000h) se presta a DOS (antes, en 32 bits, cortaba en 352 KB)
echo "== HispaBIOS de 32 bits: el firmware usa la memoria baja (58000h, 8E000h, 9E000h-9FFFFh): la BIOS en 70000h =="
if hb_variante ocupa32 "-DREGISTRO_PASOS -DPRUEBA_OCUPA=58000h,1,8E000h,1,9E000h,2" 32 && arranca 32 $T/ocupa32.img ocupa32 "d i r ret w3 e q u i p o ret w25" "bytes libres"; then
  S=$T/serie_ocupa32.txt
  L=$(hb_log ocupa32)
  grep -aq 'La BIOS de modo real va en 70000h. Memoria convencional para DOS: 448 KB' $S && ok "elige la base 70000h y da 448 KB a DOS" || ko "no elige la base 70000h con 448 KB"
  echo "$L" | grep -q 'se prestan a DOS (se cambian en cada viaje): 58000 ' && ok "presta a DOS la página 58000h" || ko "no presta la página 58000h"
  echo "$L" | grep -q 'no da de 9E000 a 9FFFF (2 pag.)' && ok "HISPABIOS.LOG: las páginas que el firmware no da, por tramos" || ko "HISPABIOS.LOG sin las páginas no dadas"
  grep -aq '448 KB convencional' $S && ok "DOS ve 448 KB de memoria convencional" || ko "DOS no ve 448 KB"
  grep -aq 'bytes libres' $S && ok "llega a C:\\> y DIR responde" || ko "no llega a C:\\> o DIR no responde"
  grep -aq 'ACPI: RSDP en 000[0-9A-F]*h' $S && grep -aq 'SMBIOS [0-9]\.[0-9]* (tablas en' $S && ok "EQUIPO encuentra ACPI y SMBIOS con la BIOS en 70000h" || ko "EQUIPO no encuentra ACPI o SMBIOS con la BIOS en 70000h"
  n2=$(grep -ac 'BdsDxe: loading' $S)
  [ "$n2" -le 1 ] && ok "no se ha reiniciado" || ko "se ha reiniciado ($n2 arranques)"
else
  ko "no se ha podido ensamblar o arrancar la variante de la memoria baja ocupada (32 bits)"
fi

echo "== HispaBIOS de 32 bits: sin memoria baja libre: lo dice, dice qué hacer y guarda HISPABIOS.LOG =="
if hb_variante sinbase32 "-DPRUEBA_OCUPA=5F000h,1,6E000h,1,7E000h,1,8E000h,1,9E000h,2" 32 && HB_HASTA="Pulse una tecla" HB_SEG=90 arranca a32 $T/sinbase32.img sinbase32 "" "Pulse una tecla"; then
  S=$T/serie_sinbase32.txt
  grep -aq 'ESTE EQUIPO NO DEJA LIBRE LA MEMORIA BAJA' $S && ok "avisa de que no hay memoria baja libre" || ko "no avisa de la memoria baja"
  grep -aq 'arranque heredado' $S && ok "dice qué hacer (CSM / arranque heredado)" || ko "no dice qué hacer"
  grep -aq 'Se ha guardado el registro en la memoria USB: HISPABIOS.LOG' $S && ok "dice que ha guardado el registro" || ko "no dice que ha guardado el registro"
  grep -aq 'Pulse una tecla' $S && ok "espera una tecla (no se queda parado sin decir nada)" || ko "no llega a esperar la tecla"
  grep -aq 'Hispa-DOS 0\.9' $S && ko "ha arrancado DOS" || ok "DOS no arranca"
  L=$(hb_log sinbase32)
  echo "$L" | grep -q '^Firmware: ' && echo "$L" | grep -q 'no da de 9E000 a 9FFFF (2 pag.)' && echo "$L" | grep -q 'ESTE EQUIPO NO DEJA LIBRE' && ok "HISPABIOS.LOG (sin ser la imagen de prueba): firmware, páginas que no da y el fallo" || ko "HISPABIOS.LOG incompleto"
else
  ko "no se ha podido ensamblar o arrancar la variante sin memoria baja (32 bits)"
fi

# una pantalla lenta (simulada: PRUEBA_LENTA) y vertical: modo más pequeño del
# firmware; y VBEPRU (las llamadas de VBE de Hispa Ventanas, el marco lineal)
echo "== HispaBIOS de 32 bits: una pantalla lenta (simulada): modo más pequeño del firmware y VBE =="
if hb_variante lenta32 "-DREGISTRO_PASOS -DPRUEBA_LENTA=100000" 32 && nasm -f bin -o $T/VBEPRU.COM tests/progs/vbepru.asm; then
  printf 'VBEPRU\r\nHBLOG\r\n' > $T/VP.BAT
  mcopy -o -i $T/lenta32.img@@1048576 $T/VBEPRU.COM ::VBEPRU.COM
  mcopy -o -i $T/lenta32.img@@1048576 $T/VP.BAT ::VP.BAT
  HB_MEM=1G HB_MAQ="-machine q35" HB_VGA="-vga none -device VGA,xres=1920,yres=1200,vgamem_mb=32" \
    arranca a32 $T/lenta32.img lenta32 "v p ret" "fin ---"
  S=$T/serie_lenta32.txt
  L=$(hb_log lenta32)
  echo "$L" | grep -q 'Pantalla lenta: se pasa a un modo mas pequeno del firmware: 1152x864' && ok "con la pantalla lenta pasa a un modo más pequeño del firmware (1152x864)" || ko "no cambia de modo con la pantalla lenta"
  echo "$L" | grep -q 'Texto: ampliado a toda la pantalla (1152x720)' && ok "y amplía el texto a la pantalla nueva" || ko "no amplía el texto a la pantalla nueva"
  grep -aq 'VBEPRU-BIEN' $S && ok "VBE: 4F00h, 4F01h, 4F02h y 4F03h contestan lo que pide Hispa Ventanas; el marco lineal se escribe y se lee" || ko "VBE no contesta bien ($(grep -a 'VBEPRU' $S | head -1))"
  grep -aq 'INT 10h AX=4F00 BX=[0-9A-F]* CX=[0-9A-F]* -> AX=004F' $S && grep -aq 'INT 10h AX=4F02 BX=4115 CX=[0-9A-F]* -> AX=004F' $S && ok "HBLOG: las llamadas de VBE y lo contestado" || ko "HBLOG sin las llamadas de VBE"
else
  ko "no se ha podido ensamblar la variante de la pantalla lenta (32 bits)"
fi
