#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo64/pruebas.sh - las pruebas de Hispa-64 (docs/64/HISPA-64.md), etapas 0, 1, 2 y 3:
#   - el núcleo (hispa64.cas) se compila igual con ccastel.py y NASM que con
#     ensambla.py (y, si se da CASTEL64=ruta y ENSAMB64=ruta, con ellos);
#   - arranca en QEMU por multiboot (-kernel, con HISPA64.SIM de módulo) con
#     128 MB, 3 GB (-cpu max: páginas de 1 GB) y 8 GB (qemu64: sin ellas), en
#     la mitad alta: la memoria libre, el mapa directo, sin la identidad, las
#     páginas de 4 KB, 2 MB y 1 GB puestas y quitadas (la TLB, al día), el
#     montón (2.000 y 100.000 reservas al azar, comprobadas, sin perder nada);
#   - las excepciones de prueba se explican en español, con el nombre de la
#     función y quién la ha llamado: el fallo de página, el doble fallo de
#     desbordar la pila (con la IST 1), el NMI (con la IST 2, mandado desde el
#     monitor de QEMU), escribir en el código, ejecutar en los datos, leer de
#     la identidad que ya no está; y las paradas del montón (escribir más allá
#     de un bloque, liberar dos veces); con un procesador sin modo largo, lo
#     dice y se para;
#   - arranca por UEFI de 64 bits (OVMF) con GRUB (BOOTX64.EFI, multiboot2 y
#     el módulo): con 128 MB, con 3 GB, -cpu max y 4 procesadores, y con 8 GB;
#     la pantalla del GOP con escritura combinada;
#   - la consola se ve en la pantalla (un pantallazo de QEMU: la letra amarilla
#     de «Memoria:», la gris del texto y la verde del final), que queda en
#     $T/pantalla.png;
#   - [etapa 2] ACPI (la RSDP de GRUB o buscada en la BIOS, la XSDT o la RSDT,
#     las sumas, la MADT, la FADT, el HPET, la MCFG), con «-machine pc» y
#     «q35», 1 y 4 procesadores, con y sin HPET («hpet=off»), por multiboot y
#     por UEFI: el reloj (el HPET o el de ACPI), el tic del APIC local (100 en
#     1 s, de 75 a 110, y entre dos cambios del segundo del CMOS, que es otro reloj),
#     el IOAPIC de verdad (el temporizador 0 del HPET, o el 8254, por una GSI),
#     el teclado PS/2 (teclas mandadas por el monitor de QEMU), el botón de
#     encendido (system_powerdown: apaga por ACPI, S5), apagar por ACPI, y la
#     lista de PCI frente a la que da QEMU («info pci»), con un puente PCIe y
#     un xHCI detrás, y una BAR de 64 bits por encima de 4 GB por UEFI.
#   - [etapa 3] el trampolín de los demás procesadores (smp64.cas) tiene los
#     octetos que saca NASM de trampolin.asm; con 1, 2, 4 y 8 procesadores
#     (-smp; con «-cpu max» también), por multiboot y por UEFI (OVMF): cuántos
#     se encienden, cien hilos que suman a la vez (con un cerrojo, «lock
#     xadd» y un mutex: el número exacto), el reparto (todos los procesadores
#     trabajan), dormir (ninguno antes de tiempo; el que más se pasa, como
#     mucho 50 ms), un suceso, semáforos (productores y consumidores: la suma
#     exacta), las prioridades (uno normal con tres de fondo: el 90 % o más
#     del trabajo es suyo; uno interactivo que duerme 10 ms les pasa por
#     delante, expropiando, y se pasa de media 25 ms como mucho), vaciar la TLB de los demás (una página quitada con
#     aviso le falla al otro; sin aviso, el otro aún la lee); la tarea ociosa
#     con mwait (ocioso=mwait, -cpu max); «sinsmp»; y los pánicos en otro
#     procesador (un fallo de página, una parada del núcleo, el desbordamiento
#     de la pila de un hilo, el NMI del monitor de QEMU con 4 procesadores):
#     se paran todos, y uno lo explica diciendo en qué procesador ha pasado.
# Uso: nucleo64/pruebas.sh [CARPETA_DE_TRABAJO]   (unos 5 minutos)
AQUI=$(cd "$(dirname "$0")" && pwd)
C=$(dirname "$AQUI")
T=${1:-/tmp/hispa64-pruebas}
mkdir -p "$T"
BIEN=0; MAL=0
ok() { echo "  bien: $*"; BIEN=$((BIEN + 1)); }
ko() { echo "  MAL:  $*"; MAL=$((MAL + 1)); }
serie() { iconv -f CP437 -t UTF-8 "$1" 2> /dev/null | tr -d '\r'; }
OVMF_C=/usr/share/OVMF/OVMF_CODE_4M.fd
OVMF_V=/usr/share/OVMF/OVMF_VARS_4M.fd
DBG="-device isa-debug-exit,iobase=0xf4,iosize=0x04"

echo "== Hispa-64, etapas 0, 1, 2 y 3 =="
# ---- compilar ----
if python3 -I "$C/ccastel.py" "$AQUI/hispa64.cas" -o "$T/hispa64.asm" -I "$C/nucleo32" > "$T/comp.txt" 2>&1 \
   && nasm -f bin "$T/hispa64.asm" -o "$T/HISPA64.BIN" 2>> "$T/comp.txt" \
   && { echo "[map symbols $T/hispa64.map]"; cat "$T/hispa64.asm"; } > "$T/hispa64m.asm" && nasm -f bin "$T/hispa64m.asm" -o "$T/HISPA64.M" \
   && cmp -s "$T/HISPA64.BIN" "$T/HISPA64.M" && python3 -I "$AQUI/simbolos.py" "$T/hispa64.map" "$T/HISPA64.SIM"; then
  ok "el núcleo se compila (ccastel.py y NASM): $(stat -c %s "$T/HISPA64.BIN") octetos, $(wc -l < "$T/hispa64.asm") líneas de ensamblador; HISPA64.SIM con $(wc -l < "$T/HISPA64.SIM") nombres"
else
  ko "el núcleo no se compila: $(head -3 "$T/comp.txt" | paste -sd ' ')"; exit 1
fi
# el «modelo de núcleo» de direcciones: ninguna etiqueta en un registro de 32
# bits en lo de 64 (a_lea_alto: «lea rsi, [T_3]»); solo en el arranque de 32
n32=$(sed -n '/^bits 64/,$p' "$T/hispa64.asm" | grep -cE '^ +mov e(ax|bx|cx|dx|si|di), [A-Z_]')
nlea=$(grep -cE '^    lea r(ax|bx|cx|dx|si|di), \[[A-Z_]' "$T/hispa64.asm")
if [ "$n32" = 0 ] && [ "$nlea" -gt 100 ] && grep -q '^org 0xFFFFFFFF80100000$' "$T/hispa64.asm"; then
  ok "el «modelo de núcleo» (en 0xFFFFFFFF80100000): $nlea direcciones con «lea r.., [etiqueta]», ninguna en un registro de 32 bits en lo de 64"
else ko "el «modelo de núcleo» no sale: $n32 etiquetas en registros de 32 bits, $nlea con lea"; fi
if python3 -I "$C/ensambla.py" "$T/hispa64.asm" -o "$T/HISPA64.PY" > /dev/null 2>&1 && cmp -s "$T/HISPA64.BIN" "$T/HISPA64.PY"; then
  ok "ensambla.py da los mismos octetos que NASM"
else ko "ensambla.py no da lo mismo que NASM"; fi
if [ -n "${CASTEL64:-}" ] && [ -n "${ENSAMB64:-}" ]; then
  if (cd "$AQUI" && "$CASTEL64" hispa64.cas -o "$T/hispa64c.asm" -I ../nucleo32 > "$T/c64.txt" 2>&1) && cmp -s "$T/hispa64.asm" "$T/hispa64c.asm" \
     && "$ENSAMB64" "$T/hispa64.asm" "$T/HISPA64.ENS" > /dev/null 2>&1 && cmp -s "$T/HISPA64.BIN" "$T/HISPA64.ENS"; then
    ok "CASTEL64 saca el mismo ensamblador y ENSAMB64 los mismos octetos"
  else ko "CASTEL64 o ENSAMB64 no dan lo mismo: $(head -2 "$T/c64.txt" | paste -sd ' ')"; fi
fi

# [etapa 3] el trampolín: los octetos de smp64.cas, los que saca NASM de trampolin.asm
if nasm -f bin "$AQUI/trampolin.asm" -o "$T/tramp.bin" 2> /dev/null \
   && [ "$(od -An -v -tx1 "$T/tramp.bin" | tr -s ' \n' ' ' | sed 's/^ //; s/ $//')" = \
        "$(sed -n '/; <trampolín>/,/; <\/trampolín>/p' "$AQUI/smp64.cas" | grep -o '0x[0-9A-F][0-9A-F]' | sed 's/0x//' | tr 'A-F' 'a-f' | paste -sd ' ')" ] \
   && grep -q "constante LARGO_TRAMPOLÍN = $(stat -c %s "$T/tramp.bin")$" "$AQUI/smp64.cas"; then
  ok "el trampolín de los demás procesadores (16, 32 y 64 bits): los $(stat -c %s "$T/tramp.bin") octetos de smp64.cas son los de trampolin.asm (NASM)"
else ko "los octetos del trampolín de smp64.cas no son los de trampolin.asm (NASM)"; fi

# ---- por multiboot (QEMU -kernel): rápido, solo el puerto serie ----
mb1() {   # LÍNEA SALIDA MEMORIA [más opciones de QEMU]
  local l=$1 s=$2 m=$3; shift 3
  rm -f "$s"
  timeout 90 qemu-system-x86_64 -kernel "$T/HISPA64.BIN" -initrd "$T/HISPA64.SIM" -append "$l" -display none -serial file:"$s" \
    -monitor none $DBG -no-reboot -nic none -m "$m" "$@" > /dev/null 2>&1
}
hay() { serie "$1" | grep -q -- "$2"; }
# [etapa 2] lo que tiene que decir de ACPI, los relojes y las interrupciones;
# entre: ¿está el número que sigue a ese texto entre MIN y MAX?
entre() {   # SALIDA TEXTO MIN MAX
  local v; v=$(serie "$1" | grep -o -- "$2[0-9]*" | head -1 | grep -o '[0-9]*$')
  [ -n "$v" ] && [ "$v" -ge "$3" ] && [ "$v" -le "$4" ]
}
# (de 75 a 110: con el servidor cargado, el TCG de QEMU pierde tics del APIC, nunca da
# de más; sin carga salen 99 o 100)
# (y de 50 a 110 por UEFI con 4 procesadores, y sin HPET con 4: el TCG reparte el servidor
# entre los cuatro; con el servidor cargado han salido 64)
tics_bien() {   # SALIDA RELOJ [MÍNIMO]: 100 tics en 1 s y entre dos segundos del CMOS
  local m=${3:-75}
  entre "$1" "^Tic: " "$m" 110 && hay "$1" "^Tic: [0-9]* tics en 1 s (medido con $2); entre dos cambios del segundo del CMOS: " \
    && entre "$1" "entre dos cambios del segundo del CMOS: " "$m" 110
}
# [etapa 3] lo de los procesadores y el planificador, con N en marcha
e3_bien() {   # SALIDA N
  local s=$1 n=$2 v; v=$(serie "$s")
  grep -q "^Procesadores: $n en marcha (APIC" <<< "$v" \
    && grep -q "^Hilos: 100 hilos que suman 1, 10000 veces cada uno, a la vez: con un cerrojo, 1000000 (exacto); con «lock xadd», 1000000; con un mutex (500 cada uno), 50000 (exacto)" <<< "$v" \
    && grep -q "^  al acabar, sus pilas y sus bloques, devueltos" <<< "$v" \
    && grep -q "^Reparto: .*(todos han trabajado)$" <<< "$v" && [ "$(grep '^Reparto:' <<< "$v" | grep -o ' %' | wc -l)" = "$n" ] \
    && grep -q "^Dormir: 8 hilos, de 20 a 160 ms, dos veces: ninguno antes de tiempo (0); el que más se pasa, " <<< "$v" \
    && entre "$s" "el que más se pasa, " 0 50 \
    && grep -q "^Suceso: 20 hilos lo esperan; antes de señalarlo, despiertos 0; después, 20$" <<< "$v" \
    && grep -q "^Semáforos: 4 productores y 4 consumidores, 10000 cosas por un búfer de 16: la suma, 50005000 (exacta)$" <<< "$v" \
    && entre "$s" "^Prioridades: en el procesador [0-9]*, uno normal y tres de fondo, sin parar, 300 ms: el normal hace el " 90 100 \
    && grep -q "^  y uno interactivo que duerme 10 ms, 10 veces, en ese mismo procesador: se pasa de media [0-9,]* ms (pasa por delante: [1-9][0-9]* expropiaciones)$" <<< "$v" \
    && entre "$s" "se pasa de media " 0 25 \
    && { [ "$n" = 1 ] || grep -q "^  y, ya con el marco nuevo en su TLB (lee 0x2222), quitada con aviso (desmapea: 1 aviso por IPI): al [0-9]* le falla la lectura (fallo de página en 0xFFFFD00100000000, con red), como debe$" <<< "$v"; } \
    && ! grep -q "MAL:" <<< "$v" && grep -q "^Hispa-64 etapa 3: todo en orden.$" <<< "$v"
}
e3_dice() {   # SALIDA: lo que dice, para cuando sale mal
  serie "$1" | grep -E '^Procesadores|^Hilos|^Reparto|^Dormir|^Suceso|^Semáforos|^Prioridades|^  y, ya|MAL:|parado' | cut -c1-110 | paste -sd '|'
}
# la lista de PCI que da QEMU (info pci, a los 3 s: con los buses de detrás de los
# puentes ya numerados por la BIOS) y la que dice el núcleo, «BB:DD.F VVVV:AAAA»
pci_qemu() {   # opciones de QEMU
  { sleep 3; printf 'info pci\nquit\n'; } | timeout 30 qemu-system-x86_64 -display none -monitor stdio -serial none -nic none "$@" 2> /dev/null | tr -d '\r' \
    | awk '/Bus .*device .*function/ { gsub(/[,:]/, " "); b = $2; d = $4; f = $6 }
           /PCI device/ { split($NF, x, ":"); printf "%02X:%02X.%d %s:%s\n", b, d, f, toupper(x[1]), toupper(x[2]) }' | sort
}
pci_nucleo() { serie "$1" | grep -E '^  [0-9A-F]{2}:[0-9A-F]{2}\.[0-7] [0-9A-F]{4}:[0-9A-F]{4} ' | cut -d' ' -f3,4 | sort; }
pci_igual() {   # SALIDA opciones de QEMU
  local s=$1; shift
  [ -n "$(pci_nucleo "$s")" ] && [ "$(pci_nucleo "$s")" = "$(pci_qemu "$@")" ]
}
mb1 apagar "$T/mb1.txt" 128; c=$?
S=$T/mb1.txt
if [ $c = 85 ] && hay $S "^Hispa-64 0.3.0 ─ Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.$" \
   && hay $S "^Cargador: qemu, multiboot$" && hay $S "^Memoria: 12[0-9],[0-9] MB libres en [0-9]* trozos (de 127,4 MB que da el firmware); [0-9]* KB usados por el núcleo$" \
   && hay $S "^  sin la identidad de abajo: 1 MB ya no está en las tablas$" && hay $S "los nombres de [0-9]* funciones (HISPA64.SIM)$" \
   && hay $S "^El núcleo, en la mitad alta: 0xFFFFFFFF80100000 (código, de solo leer)" && hay $S "^TSS con pilas propias (IST)" \
   && hay $S "^Páginas: de 4 KB, puesta, leída, quitada y puesta en otro marco (la TLB, al día); de 2 MB, también; de 1 GB, no (el procesador no las tiene)$" \
   && hay $S "^  el código del núcleo, sin escribir: sí; los datos, sin ejecutar: sí; la guarda de la pila, sin poner: sí$" \
   && hay $S "^Montón: 2000 reservas y liberaciones al azar .*, comprobadas; al acabar, 0 en uso y la memoria física, la de antes" \
   && hay $S "Punto de parada (int3) número 1 .*: atendido, sigue." && hay $S "^Hispa-64 etapa 3: todo en orden.$"; then
  ok "por multiboot con 128 MB: en la mitad alta, «$(serie $S | grep '^Memoria:')», sin la identidad, las páginas de 4 KB y 2 MB, el montón y «todo en orden» (código 42)"
else ko "por multiboot con 128 MB no dice lo que debe (código $c): $(serie $S | grep -E 'Memoria:|Páginas|Montón|parado' | paste -sd ' ')"; fi
# [etapa 2] lo mismo, con «-machine pc» (i440FX y PIIX4: sin MCFG, PCI por los puertos)
if [ $c = 85 ] && hay $S "^ACPI: 1.0, de «BOCHS » (la RSDP, buscada en la memoria de la BIOS); RSDT en 0x[0-9A-F]*, [0-9]* tablas: FACP APIC HPET .*DSDT (todas con la suma bien)$" \
   && hay $S "^  procesadores (MADT): 1 encendido (APIC 0)$" && hay $S "^  IOAPIC 0 en 0xFEC00000, GSI 0 a 23 (24 entradas, versión 0x20)$" \
   && hay $S "^  IRQ de ISA cambiadas: 0 -> GSI 2;" && hay $S "^  FADT: reloj de ACPI en el puerto 0x608 (24 bits), la SCI en la IRQ 9, .*botón de encendido fijo, apagar (._S5): SLP_TYP 0$" \
   && hay $S "^  HPET en 0xFED00000$" && hay $S "^  sin MCFG: la configuración de PCI, por los puertos 0xCF8 y 0xCFC$" \
   && hay $S "^Reloj: el HPET, a 100,0 MHz, 3 temporizadores, contador de 64 bits$" \
   && hay $S "^  su temporizador: [0-9]* cuentas cada 10 ms (entre 16), medido con el HPET; el tic, 100 por segundo, al vector 0x40$" \
   && tics_bien $S "el HPET" && hay $S "^IOAPIC: el temporizador 0 del HPET, por la GSI 2, al vector 0x3E: 20 de 20 interrupciones (cada 10 ms)$" \
   && hay $S "^Teclado PS/2: la IRQ 1, por la GSI 1, al vector 0x31$" && hay $S "^Botón de encendido: en modo ACPI; la SCI por la GSI 9; pulsarlo apaga el equipo$" \
   && hay $S "^PCI (por los puertos 0xCF8): [0-9]* funciones en los buses 0 a 255$" && hay $S "^Hora (CMOS): [0-3][0-9]/[01][0-9]/20[0-9][0-9] [0-2][0-9]:[0-5][0-9]:[0-5][0-9]$" \
   && pci_igual $S -m 128; then
  ok "etapa 2, por multiboot, «pc»: ACPI 1.0 (la RSDP buscada en la BIOS), la MADT, la FADT, el HPET; «$(serie $S | grep '^Tic:')»; el IOAPIC (el HPET por la GSI 2, 20 de 20), el teclado, el botón; $(pci_nucleo $S | wc -l) funciones de PCI, las mismas que dice QEMU"
else ko "etapa 2 por multiboot («pc») no dice lo que debe: $(serie $S | grep -E '^ACPI|^Tic|^IOAPIC:|^PCI|parado' | paste -sd ' ')"; fi
# [etapa 3] con un procesador: el planificador, los hilos, dormir, las esperas
if e3_bien $S 1; then
  ok "etapa 3 con 1 procesador: «$(serie $S | grep '^Hilos:' | cut -d: -f2 | cut -c1-60)...»; dormir, el suceso, los semáforos y las prioridades, bien"
else ko "etapa 3 con 1 procesador no dice lo que debe: $(e3_dice $S)"; fi
# con «q35», 4 procesadores y un puerto PCIe con un xHCI detrás (el bus 1)
PCIQ="-machine q35 -smp 4 -device pcie-root-port,id=rp1,bus=pcie.0,chassis=1 -device qemu-xhci,bus=rp1"
# shellcheck disable=SC2086
mb1 apagar "$T/q35.txt" 256 $PCIQ; c=$?
S=$T/q35.txt
# shellcheck disable=SC2086
if [ $c = 85 ] && hay $S "^  procesadores (MADT): 4 encendidos (APIC 0, 1, 2, 3)$" && hay $S "^  MCFG: PCIe por ECAM en 0xB0000000, segmento 0, buses 0 a 255$" \
   && hay $S "^  FADT: .*8042: sí" && tics_bien $S "el HPET" && hay $S "^IOAPIC: el temporizador 0 del HPET, por la GSI 1[6-9], al vector 0x3E: 20 de 20 interrupciones (cada 10 ms)$" \
   && hay $S "^PCI (PCIe, por ECAM): [0-9]* funciones en los buses 0 a 255, 1 puentes$" && hay $S "^  01:00.0 1B36:000D bus serie: USB 3 (xHCI)" \
   && hay $S "^      BAR0 0x[0-9A-F]*, 16 KB de memoria de 64 bits$" && pci_igual $S -m 256 $PCIQ && hay $S "^Hispa-64 etapa 3: todo en orden.$"; then
  ok "etapa 2, por multiboot, «q35» con 4 procesadores: la MADT con 4, PCIe por ECAM, $(pci_nucleo $S | wc -l) funciones (el puente y el xHCI del bus 1, con su BAR de 64 bits), las mismas que dice QEMU; «$(serie $S | grep '^Tic:')»"
else ko "etapa 2 con «q35» y 4 procesadores no dice lo que debe (código $c): $(serie $S | grep -E 'MADT|^Tic|^IOAPIC:|^PCI|parado' | paste -sd ' ')"; diff <(pci_nucleo $S) <(pci_qemu -m 256 $PCIQ) | head -5 | sed 's/^/        /'; fi
if e3_bien $S 4 && hay $S "^  con INIT-SIPI-SIPI y el trampolín en 0x[0-9A-F]* (16, 32 y 64 bits)"; then
  ok "etapa 3 con 4 procesadores (q35): «$(serie $S | grep '^Procesadores:')»; los 100 hilos, el número exacto; «$(serie $S | grep '^Reparto:' | cut -d: -f2- | cut -c1-80)»; la TLB de los demás, vaciada con un IPI"
else ko "etapa 3 con 4 procesadores (q35) no dice lo que debe: $(e3_dice $S)"; fi
# sin HPET: el reloj de ACPI y, para el IOAPIC, el 8254
for m in pc q35; do
  mb1 apagar "$T/sinhpet-$m.txt" 256 -machine $m,hpet=off -smp 4; c=$?
  S=$T/sinhpet-$m.txt
  if [ $c = 85 ] && hay $S "^  sin HPET$" && hay $S "^Reloj: el reloj de ACPI (sin HPET), a 3,579 MHz$" \
     && hay $S "^  su temporizador: [0-9]* cuentas cada 10 ms (entre 16), medido con el reloj de ACPI; el tic, 100 por segundo, al vector 0x40$" \
     && tics_bien $S "el reloj de ACPI" 50 && entre $S "^IOAPIC: el canal 0 del 8254, por la GSI 2, al vector 0x3E: " 17 23 \
     && hay $S "^  procesadores (MADT): 4 encendidos" && e3_bien $S 4; then
    ok "etapa 2 sin HPET («$m,hpet=off», 4 procesadores): el reloj de ACPI; «$(serie $S | grep '^Tic:')»; «$(serie $S | grep '^IOAPIC:' | cut -d: -f2-)»; y la etapa 3 con el reloj de ACPI (con cerrojo) en los 4"
  else ko "etapa 2 sin HPET («$m») no dice lo que debe (código $c): $(serie $S | grep -E '^Reloj|^Tic|^IOAPIC:|parado' | paste -sd ' ') $(e3_dice $S)"; fi
done
# [etapa 3] con 8 procesadores y «-cpu max»; con 2 y «-machine pc»; mwait; sinsmp
mb1 apagar "$T/smp8.txt" 512 -machine q35 -smp 8 -cpu max; c=$?
S=$T/smp8.txt
if [ $c = 85 ] && e3_bien $S 8 && hay $S "^Procesadores: 8 en marcha (APIC 0, 1, 2, 3, 4, 5, 6, 7)$"; then
  ok "etapa 3 con 8 procesadores y -cpu max: «$(serie $S | grep '^Reparto:' | cut -d: -f2- | cut -c1-110)»; «$(serie $S | grep '^Hilos:' | grep -o 'en [0-9]* ms')»; «$(serie $S | grep '^Dormir:' | grep -o 'el que más se pasa, [0-9,]* ms')»"
else ko "etapa 3 con 8 procesadores no dice lo que debe (código $c): $(e3_dice $S)"; fi
mb1 apagar "$T/smp2.txt" 256 -machine pc -smp 2; c=$?
S=$T/smp2.txt
if [ $c = 85 ] && e3_bien $S 2 && hay $S "^  quitada sin avisar y puesta en otro marco (con 0x2222): el 1 "; then
  ok "etapa 3 con 2 procesadores («pc»): $(serie $S | grep '^  quitada sin avisar' | sed 's/.*: el 1/sin avisar, el 1/'); con aviso, le falla"
else ko "etapa 3 con 2 procesadores («pc») no dice lo que debe (código $c): $(e3_dice $S)"; fi
mb1 "apagar ocioso=mwait" "$T/mwait.txt" 256 -machine q35 -smp 4 -cpu max; c=$?
S=$T/mwait.txt
if [ $c = 85 ] && e3_bien $S 4 && hay $S "la tarea ociosa, con mwait, FORZADO (ocioso=mwait)$"; then
  ok "ocioso=mwait con -cpu max y 4 procesadores: la tarea ociosa con monitor/mwait, y todo igual"
else ko "ocioso=mwait no sale (código $c): $(serie $S | grep -E '^Planificador' | cut -c1-120) $(e3_dice $S)"; fi
mb1 "apagar sinpruebas sinsmp" "$T/sinsmp.txt" 256 -machine q35 -smp 4; c=$?
if [ $c = 85 ] && hay "$T/sinsmp.txt" "^Procesadores: 1 en marcha (APIC 0) (sinsmp: los demás, apagados)$" && hay "$T/sinsmp.txt" "^Hispa-64 etapa 3: todo en orden.$"; then
  ok "sinsmp: con 4 en la MADT, solo el que arranca (los demás, apagados)"
else ko "sinsmp no sale (código $c): $(serie "$T/sinsmp.txt" | grep -E '^Procesadores|parado' | paste -sd ' ')"; fi
# el teclado (las teclas, desde el monitor de QEMU), el botón de encendido y apagar por ACPI
con_monitor() {   # PRUEBA SALIDA ESPERA ORDEN... (las órdenes, al ver ESPERA por el puerto serie)
  local p=$1 s=$2 e=$3 q c; shift 3
  rm -f "$s" "$T/mon2.sock"
  timeout 60 qemu-system-x86_64 -kernel "$T/HISPA64.BIN" -initrd "$T/HISPA64.SIM" -append "apagar sinmedir sinpruebas prueba=$p" -display none -serial file:"$s" \
    -monitor unix:"$T/mon2.sock",server,nowait $DBG -no-reboot -nic none -m 128 -machine q35 > /dev/null 2>&1 &
  q=$!
  for _ in $(seq 1 150); do grep -q "$e" "$s" 2> /dev/null && break; sleep 0.2; done
  for o in "$@"; do python3 -I "$AQUI/pantallazo.py" orden "$T/mon2.sock" "$o" 2> /dev/null; done
  wait $q; c=$?
  return $c
}
con_monitor teclado "$T/teclado.txt" "esperando teclas" "sendkey a" "sendkey b"; c=$?
if [ $c = 85 ] && [ "$(serie "$T/teclado.txt" | grep '^Tecla: ' | paste -sd ' ')" = "Tecla: código 0x1E (pulsada) Tecla: código 0x9E (soltada) Tecla: código 0x30 (pulsada) Tecla: código 0xB0 (soltada)" ]; then
  ok "prueba=teclado: las teclas «a» y «b» del monitor de QEMU llegan por el IOAPIC (la IRQ 1): 0x1E, 0x9E, 0x30, 0xB0"
else ko "prueba=teclado no recibe las teclas (código $c): $(serie "$T/teclado.txt" | grep -E 'Tecla|Teclado' | paste -sd ' ')"; fi
con_monitor boton "$T/boton.txt" "esperando el bot" system_powerdown; c=$?
if [ $c = 0 ] && hay "$T/boton.txt" "^Botón de encendido pulsado: se apaga el equipo (ACPI, S5).$" && ! hay "$T/boton.txt" "No se ha podido apagar"; then
  ok "prueba=boton: el botón de encendido (system_powerdown) llega por la SCI y apaga el equipo por ACPI (QEMU acaba con 0)"
else ko "prueba=boton no apaga (código $c): $(serie "$T/boton.txt" | tail -3 | paste -sd ' ')"; fi
mb1 "apagar sinmedir sinpruebas prueba=acpiapagar" "$T/acpiapagar.txt" 128; c=$?
if [ $c = 0 ] && hay "$T/acpiapagar.txt" "^Prueba: apagar por ACPI (S5, SLP_TYP 0)...$" && ! hay "$T/acpiapagar.txt" "No se ha podido apagar"; then
  ok "prueba=acpiapagar: apaga por ACPI (\\_S5 de la DSDT, PM1a_CNT)"
else ko "prueba=acpiapagar no apaga (código $c): $(serie "$T/acpiapagar.txt" | tail -3 | paste -sd ' ')"; fi
mb1 apagar "$T/mb3g.txt" 3G -cpu max; c=$?
S=$T/mb3g.txt
if [ $c = 85 ] && hay $S "^Memoria: 2,9 GB libres" && hay $S "en páginas de 1 GB: [1-9]" && hay $S "; de 1 GB, también$" \
   && hay $S "^Hispa-64 etapa 3: todo en orden.$"; then
  ok "por multiboot con 3 GB y -cpu max: «$(serie $S | grep '^Memoria:' | cut -d';' -f1)»; el mapa directo con páginas de 1 GB; una de 1 GB puesta, leída y quitada"
else ko "por multiboot con 3 GB no dice lo que debe (código $c): $(serie $S | grep -E 'Memoria:|Páginas|parado' | paste -sd ' ')"; fi
mb1 apagar "$T/mb8g.txt" 8G; c=$?
S=$T/mb8g.txt
if [ $c = 85 ] && hay $S "^Memoria: 7,9 GB libres en [0-9]* trozos (de 7,9 GB" && hay $S "en páginas de 1 GB: 0; de 2 MB: 4[0-9][0-9][0-9];" \
   && hay $S "^Hispa-64 etapa 3: todo en orden.$"; then
  ok "por multiboot con 8 GB (qemu64, sin páginas de 1 GB): «$(serie $S | grep '^Memoria:' | cut -d';' -f1)»; el mapa directo, con páginas de 2 MB"
else ko "por multiboot con 8 GB no dice lo que debe (código $c): $(serie $S | grep -E 'Memoria:|directo|parado' | paste -sd ' ')"; fi
mb1 "apagar sinmedir sinpruebas prueba=monton" "$T/monton.txt" 256; c=$?
if [ $c = 85 ] && hay $T/monton.txt "^Montón: 100000 reservas y liberaciones al azar (de 1 a 20025 octetos, [0-9]* grandes; 1000 a la vez), comprobadas; al acabar, 0 en uso y la memoria física, la de antes"; then
  ok "prueba=monton: $(serie $T/monton.txt | grep '^Montón: 100000' | cut -d'(' -f1)con 1000 a la vez y $(serie $T/monton.txt | grep '^Montón: 100000' | sed 's/.*octetos, \([0-9]*\) grandes.*/\1/') grandes; comprobadas, y sin perder memoria"
else ko "prueba=monton no sale (código $c): $(serie $T/monton.txt | grep -E 'Montón|parado' | paste -sd ' ')"; fi
# las excepciones: explicadas, con el nombre de la función y quién la ha llamado
for x in "pagina|14|Fallo de página (#PF)|Al leer de 0x0000500000000000: la página no está (desde el núcleo).|Llamada desde: Hispa64.provoca+" \
         "gp|13|Protección general (#GP)|Número de error: 0x0|En la función Hispa64.l64+" \
         "division|0|División por cero (#DE)|En la función Hispa64.provoca+|RIP=0x" \
         "ud|6|Instrucción no válida (#UD)|En la función Hispa64.provoca+|RIP=0x" \
         "doble|8|Doble fallo (#DF)|(Se atiende con su propia pila: la IST 1 de la TSS.)|La pila del núcleo se ha desbordado: RSP=" \
         "identidad|14|Fallo de página (#PF)|Al leer de 0x0000000000100000: la página no está (desde el núcleo).|Llamada desde: Hispa64.provoca+" \
         "codigo|14|Fallo de página (#PF)|Al escribir en 0xFFFFFFFF80100040: la página está, pero sin permiso (desde el núcleo).|En la función Hispa64.provoca+" \
         "nx|14|Fallo de página (#PF)|Al ejecutar en 0xFFFF8000|Número de error: 0x11"; do
  IFS="|" read -r p n nom det det2 <<< "$x"
  mb1 "apagar sinmedir sinpruebas prueba=$p" "$T/exc-$p.txt" 128; c=$?
  if [ $c = 199 ] && serie "$T/exc-$p.txt" | grep -q "^\*\*\* Hispa-64 se ha parado: excepción $n \*\*\*$" \
     && serie "$T/exc-$p.txt" | grep -qF "$nom" && serie "$T/exc-$p.txt" | grep -qF "$det" && serie "$T/exc-$p.txt" | grep -qF "$det2" \
     && serie "$T/exc-$p.txt" | grep -q "^R13=0x"; then
    ok "prueba=$p: la excepción $n, explicada («$nom», «$det»), con los registros"
  else ko "prueba=$p no dice la excepción $n (código $c): $(serie "$T/exc-$p.txt" | tail -12 | paste -sd ' ')"; fi
done
if serie "$T/exc-doble.txt" | grep -q "^Llamada desde: Hispa64.hondo+0x[0-9A-F]* (y [0-9]* veces más, por lo menos)$"; then
  ok "el doble fallo dice también quién ha llamado: «$(serie "$T/exc-doble.txt" | grep '^Llamada desde')»"
else ko "el doble fallo no dice quién ha llamado: $(serie "$T/exc-doble.txt" | grep Llamada)"; fi
# las paradas del montón: el código 98 (197 en QEMU)
for x in "corrupcion|se ha escrito más allá de su final (la guarda está pisada)" "dosveces|se libera dos veces"; do
  IFS="|" read -r p det <<< "$x"
  mb1 "apagar sinmedir sinpruebas prueba=$p" "$T/par-$p.txt" 128; c=$?
  if [ $c = 197 ] && hay "$T/par-$p.txt" "^\*\*\* Hispa-64 se ha parado: el montón del núcleo está estropeado \*\*\*$" \
     && hay "$T/par-$p.txt" ": $det$" && hay "$T/par-$p.txt" "^Llamada desde: Monton64.estropeado+0x[0-9A-F]* <- Monton64.libera+0x[0-9A-F]* <- Hispa64.provoca+"; then
    ok "prueba=$p: el núcleo se para y lo dice («$det»), con quién ha llamado"
  else ko "prueba=$p no para el núcleo como debe (código $c): $(serie "$T/par-$p.txt" | tail -5 | paste -sd ' ')"; fi
done
# el NMI: se lo manda el monitor de QEMU
rm -f "$T/nmi.txt" "$T/nmi.sock"
timeout 60 qemu-system-x86_64 -kernel "$T/HISPA64.BIN" -initrd "$T/HISPA64.SIM" -append "apagar sinmedir sinpruebas prueba=nmi" -display none -serial file:"$T/nmi.txt" \
  -monitor unix:"$T/nmi.sock",server,nowait $DBG -no-reboot -nic none -m 128 > /dev/null 2>&1 &
q=$!
for _ in $(seq 1 100); do grep -q "esperando un NMI" "$T/nmi.txt" 2> /dev/null && break; sleep 0.2; done
python3 -I "$AQUI/pantallazo.py" orden "$T/nmi.sock" nmi 2> /dev/null
wait $q; c=$?
if [ $c = 199 ] && hay "$T/nmi.txt" "^\*\*\* Hispa-64 se ha parado: excepción 2 \*\*\*$" && hay "$T/nmi.txt" "^(Se atiende con su propia pila: la IST 2 de la TSS.)$"; then
  ok "prueba=nmi: el NMI del monitor de QEMU se explica, atendido con su propia pila (la IST 2)"
else ko "prueba=nmi no sale (código $c): $(serie "$T/nmi.txt" | tail -4 | paste -sd ' ')"; fi
# [etapa 3] los pánicos en otro procesador: se paran todos y uno lo explica
for x in "panicoap|4|199|excepción 14|Al leer de 0x0000500000000000: la página no está (desde el núcleo).|En la función PrSmp64.pánico+" \
         "paradaap|2|197|un hilo abre un mutex que no ha cerrado él|Llamada desde: Sinc64.abre_mutex+|PrSmp64.pánico+" \
         "pilahilo|2|199|excepción 8|La pila de un hilo (o de un procesador) se ha desbordado: RSP=0xFFFFE0|(Se atiende con su propia pila: la IST 1 de la TSS.)"; do
  IFS="|" read -r p n cod que det det2 <<< "$x"
  mb1 "apagar sinmedir sinpruebas prueba=$p" "$T/smp-$p.txt" 256 -machine q35 -smp $n; c=$?
  S=$T/smp-$p.txt
  if [ $c = "$cod" ] && hay $S "^\*\*\* Hispa-64 se ha parado: $que \*\*\*$" && { hay $S "^En el procesador $((n - 1)) (APIC $((n - 1))); los otros $((n - 1)) se han parado.$" || { [ $n = 2 ] && hay $S "^En el procesador 1 (APIC 1); el otro se ha parado.$"; }; } \
     && serie $S | grep -qF "$det" && serie $S | grep -qF "$det2" && [ "$(serie $S | grep -c 'se ha parado:')" = 1 ]; then
    ok "prueba=$p con $n procesadores: «$(serie $S | grep '^En el procesador')», y lo explica uno solo («$que»)"
  else ko "prueba=$p con $n procesadores no se explica como debe (código $c): $(serie $S | tail -14 | paste -sd ' ' | cut -c1-300)"; fi
done
rm -f "$T/nmi4.txt" "$T/nmi4.sock"
timeout 60 qemu-system-x86_64 -kernel "$T/HISPA64.BIN" -initrd "$T/HISPA64.SIM" -append "apagar sinmedir sinpruebas prueba=nmi" -display none -serial file:"$T/nmi4.txt" \
  -monitor unix:"$T/nmi4.sock",server,nowait $DBG -no-reboot -nic none -m 128 -machine q35 -smp 4 > /dev/null 2>&1 &
q=$!
for _ in $(seq 1 100); do grep -q "esperando un NMI" "$T/nmi4.txt" 2> /dev/null && break; sleep 0.2; done
python3 -I "$AQUI/pantallazo.py" orden "$T/nmi4.sock" nmi 2> /dev/null
wait $q; c=$?
if [ $c = 199 ] && hay "$T/nmi4.txt" "^\*\*\* Hispa-64 se ha parado: excepción 2 \*\*\*$" && hay "$T/nmi4.txt" "^En el procesador [0-3] (APIC [0-3]); los otros 3 se han parado.$" \
   && [ "$(serie "$T/nmi4.txt" | grep -c 'se ha parado:')" = 1 ]; then
  ok "prueba=nmi con 4 procesadores (el NMI de QEMU llega a todos): «$(serie "$T/nmi4.txt" | grep '^En el procesador')», y uno solo lo explica"
else ko "prueba=nmi con 4 procesadores no sale (código $c): $(serie "$T/nmi4.txt" | tail -6 | paste -sd ' ')"; fi
# un procesador sin el modo largo (un qemu64 sin «lm»): lo dice y se para
rm -f "$T/no64.txt"
timeout 8 qemu-system-x86_64 -cpu qemu64,-lm -kernel "$T/HISPA64.BIN" -append apagar -display none -serial file:"$T/no64.txt" \
  -monitor none $DBG -no-reboot -nic none -m 256 > /dev/null 2>&1; c=$?
if [ $c = 124 ] && grep -q "Hispa-64: el procesador no es de 64 bits. Use Hispa-Pentium." "$T/no64.txt"; then
  ok "con un procesador sin modo largo lo dice por el puerto serie y se para"
else ko "sin modo largo no lo dice (código $c): $(tr -d '\r' < "$T/no64.txt" | paste -sd ' ')"; fi

# ---- por UEFI de 64 bits (OVMF), con GRUB y multiboot2 ----
if [ -f $OVMF_C ] && command -v grub-mkimage > /dev/null && command -v mcopy > /dev/null; then
  "$AQUI/imagen.sh" "$T/uefi" prueba > "$T/img.txt" 2>&1 || ko "imagen.sh no hace la imagen: $(tail -2 "$T/img.txt" | paste -sd ' ')"
  uefi() {   # SALIDA [opciones de QEMU]
    local s=$1; shift
    rm -f "$s"; cp $OVMF_V "$T/vars.fd"
    timeout 150 qemu-system-x86_64 -machine q35 -drive if=pflash,format=raw,readonly=on,file=$OVMF_C \
      -drive if=pflash,format=raw,file="$T/vars.fd" -drive file="$T/uefi/hispa64.img",format=raw,if=virtio \
      -display none -serial file:"$s" $DBG -no-reboot -nic none "$@" > /dev/null 2>&1
  }
  uefi "$T/uefi.txt" -m 128 -monitor none; c=$?
  S=$(serie "$T/uefi.txt")
  if [ $c = 85 ] && grep -q "Hispa-64 0.3.0 ─ Copyright" <<< "$S" && grep -q "^Cargador: GRUB .*, multiboot2, con UEFI de 64 bits$" <<< "$S" \
     && grep -q "^Pantalla: [0-9]* x [0-9]* puntos, 32 bits por punto" <<< "$S" && grep -q "^  consola de [0-9]* x [0-9]* letras" <<< "$S" \
     && grep -q "^  la pantalla, en 0xFFFFC00000000000, con escritura combinada (PAT)$" <<< "$S" \
     && grep -q "^  módulo del cargador: HISPA64.SIM, .*: los nombres de [0-9]* funciones (HISPA64.SIM)$" <<< "$S" \
     && grep -q "^Memoria: 1[0-9][0-9],[0-9] MB libres en [0-9]* trozos" <<< "$S" && grep -q "^Montón: 2000 reservas" <<< "$S" \
     && grep -q "^ACPI: 2.0 o más, de «BOCHS »; XSDT en 0x[0-9A-F]*, [0-9]* tablas: FACP APIC HPET MCFG .*DSDT (todas con la suma bien)$" <<< "$S" \
     && grep -q "ACPI NVS" <<< "$S" && grep -q "^Hispa-64 etapa 3: todo en orden.$" <<< "$S"; then
    ok "por UEFI de 64 bits (OVMF + GRUB, multiboot2) con 128 MB: «$(grep '^Cargador' <<< "$S")»; «$(grep '^Memoria:' <<< "$S" | cut -d';' -f1)»; la pantalla del GOP con escritura combinada, el módulo HISPA64.SIM, RSDP 2.0 y «todo en orden»"
  else ko "por UEFI con 128 MB no dice lo que debe (código $c): $(grep -E 'Memoria:|pantalla, en|parado' <<< "$S" | paste -sd ' ')"; fi
  uefi "$T/uefi-max.txt" -m 3G -cpu max -smp 4,cores=4 -monitor none; c=$?
  S=$(serie "$T/uefi-max.txt")
  if [ $c = 85 ] && grep -q "páginas de 1 GB: sí" <<< "$S" && grep -q "; 4 procesadores lógicos en el paquete" <<< "$S" \
     && grep -q "^  0x00000001.*disponible$" <<< "$S" && grep -q "^Memoria: 2,9 GB libres" <<< "$S" && grep -q "en páginas de 1 GB: [1-9]" <<< "$S" \
     && grep -q "; de 1 GB, también$" <<< "$S" && grep -q "^Hispa-64 etapa 3: todo en orden.$" <<< "$S" \
     && grep -q "^  procesadores (MADT): 4 encendidos (APIC 0, 1, 2, 3)$" <<< "$S" && tics_bien "$T/uefi-max.txt" "el HPET" 50 \
     && grep -q "^IOAPIC: el temporizador 0 del HPET, por la GSI 1[6-9], al vector 0x3E: 20 de 20 interrupciones (cada 10 ms)$" <<< "$S" \
     && grep -q "^PCI (PCIe, por ECAM): [0-9]* funciones en los buses 0 a 255, 1 BAR por encima de 4 GB$" <<< "$S" \
     && grep -q "^      BAR4 0x[0-9A-F]\{9,\}, 16 KB de memoria de 64 bits (por encima de 4 GB), precargable$" <<< "$S" \
     && pci_igual "$T/uefi-max.txt" -machine q35 -m 3G -cpu max -smp 4,cores=4 -drive file="$T/uefi/hispa64.img",format=raw,if=virtio \
     && e3_bien "$T/uefi-max.txt" 4; then
    ok "por UEFI con -cpu max, 4 núcleos y 3 GB (1 por encima de 4 GB): «$(grep '^Memoria:' <<< "$S" | cut -d';' -f1)»; el mapa directo y la prueba con páginas de 1 GB; la MADT con 4; «$(grep '^Tic:' <<< "$S")»; PCIe con la BAR del disco virtio por encima de 4 GB, la misma lista que QEMU"
  else ko "por UEFI con -cpu max y 3 GB no dice lo que debe (código $c): $(grep -E 'lógicos|Memoria:|parado' <<< "$S" | paste -sd ' ') $(e3_dice "$T/uefi-max.txt")"; fi
  uefi "$T/uefi-8g.txt" -m 8G -cpu max -smp 8 -monitor none; c=$?
  S=$(serie "$T/uefi-8g.txt")
  if [ $c = 85 ] && grep -q "^Memoria: 7,9 GB libres" <<< "$S" && e3_bien "$T/uefi-8g.txt" 8; then
    ok "por UEFI con 8 GB y 8 procesadores: «$(grep '^Memoria:' <<< "$S" | cut -d';' -f1)»; «$(grep '^Procesadores:' <<< "$S")»; la etapa 3, bien"
  else ko "por UEFI con 8 GB y 8 procesadores no dice lo que debe (código $c): $(grep -E 'Memoria:|parado' <<< "$S" | paste -sd ' ') $(e3_dice "$T/uefi-8g.txt")"; fi
  # [etapa 2] por UEFI sin HPET (q35, el reloj de ACPI) y con «pc» (i440FX: PCI por los puertos)
  uefi "$T/uefi-sinhpet.txt" -machine q35,hpet=off -m 256 -smp 4 -monitor none; c=$?
  S=$(serie "$T/uefi-sinhpet.txt")
  if [ $c = 85 ] && grep -q "^Reloj: el reloj de ACPI (sin HPET), a 3,579 MHz$" <<< "$S" && tics_bien "$T/uefi-sinhpet.txt" "el reloj de ACPI" 50 \
     && grep -q "^IOAPIC: el canal 0 del 8254, por la GSI 2" <<< "$S" && e3_bien "$T/uefi-sinhpet.txt" 4; then
    ok "por UEFI sin HPET (q35, 4 procesadores): el reloj de ACPI; «$(grep '^Tic:' <<< "$S")»"
  else ko "por UEFI sin HPET no dice lo que debe (código $c): $(grep -E '^Reloj|^Tic|^IOAPIC:|parado' <<< "$S" | paste -sd ' ')"; fi
  uefi "$T/uefi-pc.txt" -machine pc -m 256 -smp 2 -monitor none; c=$?
  S=$(serie "$T/uefi-pc.txt")
  if [ $c = 85 ] && grep -q "^  procesadores (MADT): 2 encendidos (APIC 0, 1)$" <<< "$S" && grep -q "^PCI (por los puertos 0xCF8): " <<< "$S" \
     && tics_bien "$T/uefi-pc.txt" "el HPET" && pci_igual "$T/uefi-pc.txt" -machine pc -m 256 -smp 2 -drive file="$T/uefi/hispa64.img",format=raw,if=virtio \
     && e3_bien "$T/uefi-pc.txt" 2; then
    ok "por UEFI con «-machine pc» (i440FX, 2 procesadores): la MADT con 2, PCI por los puertos, la misma lista que QEMU; «$(grep '^Tic:' <<< "$S")»"
  else ko "por UEFI con «pc» no dice lo que debe (código $c): $(grep -E 'MADT|^Tic|^PCI|parado' <<< "$S" | paste -sd ' ')"; fi
  # la pantalla: sin «apagar» se queda parado con todo escrito; un pantallazo
  "$AQUI/imagen.sh" "$T/uefi-p" prueba "" > "$T/img2.txt" 2>&1
  rm -f "$T/pant.txt" "$T/mon.sock" "$T/pantalla.ppm"; cp $OVMF_V "$T/vars2.fd"
  timeout 120 qemu-system-x86_64 -machine q35 -m 256 -drive if=pflash,format=raw,readonly=on,file=$OVMF_C \
    -drive if=pflash,format=raw,file="$T/vars2.fd" -drive file="$T/uefi-p/hispa64.img",format=raw,if=virtio \
    -display none -serial file:"$T/pant.txt" -monitor unix:"$T/mon.sock",server,nowait -no-reboot -nic none > /dev/null 2>&1 &
  q=$!
  for _ in $(seq 1 90); do grep -q "Pulse teclas para ver" "$T/pant.txt" 2> /dev/null && break; sleep 1; done
  sleep 1
  python3 -I "$AQUI/pantallazo.py" captura "$T/mon.sock" "$T/pantalla.ppm" 2> /dev/null
  wait $q 2> /dev/null
  if [ -s "$T/pantalla.ppm" ]; then
    read -r an al am gr ve <<< "$(python3 -I "$AQUI/pantallazo.py" mira "$T/pantalla.ppm" "$T/pantalla.png")"
    if [ "${am:-0}" -gt 800 ] && [ "${gr:-0}" -gt 20000 ] && [ "${ve:-0}" -gt 300 ]; then
      ok "la consola se ve en la pantalla del GOP ($an x $al), con escritura combinada: $am puntos amarillos («Memoria: ...»), $gr grises (el texto) y $ve verdes («todo en orden»); en $T/pantalla.png"
    else ko "la pantalla no tiene la consola ($an x $al: $am amarillos, $gr grises, $ve verdes; $T/pantalla.png)"; fi
  else ko "no se ha podido sacar el pantallazo"; fi
else
  echo "  (sin OVMF, grub-mkimage o mtools: no se prueba el arranque por UEFI)"
fi
echo "== Hispa-64: $BIEN bien, $MAL mal =="
[ $MAL = 0 ]
