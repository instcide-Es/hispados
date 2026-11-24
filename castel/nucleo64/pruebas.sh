#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo64/pruebas.sh - las pruebas de Hispa-64 (docs/64/HISPA-64.md), etapas 0, 1, 2, 3, 4 y 5:
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
#   - [etapa 4] los programas (programas/hazlos.sh: HOLA, MEMORIA, CUENTA y
#     MALO en C con gcc -static -nostdlib, HOLACAS en Castel, SALUDO.TXT), en
#     la unidad en memoria (los módulos: -initrd o module2), por multiboot y
#     por UEFI, con 1, 2, 4 y 8 procesadores: HOLA y HOLACAS arrancan y acaban
#     bien (argv, el vector auxiliar, getpid, uname, el reloj, nanosleep, FS,
#     open/read/lseek/close, writev, SSE); MEMORIA (brk, mmap, munmap,
#     mprotect); ocho CUENTA a la vez, cada uno con su código y sus registros
#     intactos; MALO con 24 llamadas con punteros malos (todas con su error) y
#     11 formas de fallar a la vez (cada uno cerrado con su señal y explicado
#     en español); que después no queda nada (ni memoria, ni montón, ni
#     pilas); y con 2 procesadores o más, que el aviso de quitar una página de
#     un espacio va solo al que lo tiene puesto (con SMEP, SMAP y UMIP con
#     -cpu max); y el CD (grub-mkrescue) por BIOS y por UEFI.
#   - [etapa 5] los discos y los archivos, con los discos de prueba de
#     programas/hazdiscos.sh (NVMe con GPT, SATA con MBR y una lógica exFAT,
#     dos de 3 TB con una partición más allá de 2^32 sectores, uno crudo):
#     NVMe (con MSI-X y una cola por procesador) y AHCI (con MSI), y sin
#     ellas («sinmsix sinmsi», con la AHCI aparte de «pc»); las particiones;
#     el disco crudo escrito y leído a pelo y por cuatro hilos a la vez; el
#     programa ARCHIVOS (crear, escribir, leer, O_APPEND, O_TRUNC,
#     pread/pwrite, huecos, ftruncate, nombres largos con acentos, 40
#     archivos, getdents64, rename, unlink de uno abierto, las *at, chdir,
#     los errores; exFAT, de solo leer); al apagar, lo que quedaba en la
#     caché, al disco; y desde el anfitrión, fsck.fat sin quejas y lo dejado
#     comprobado con mtools; sin «escribir», ni un octeto cambiado; y por UEFI
#     desde el disco de la imagen puesto en SATA (la partición EFI a la vista,
#     un programa arrancado desde ella).
# Uso: nucleo64/pruebas.sh [CARPETA_DE_TRABAJO]   (unos 8 minutos)
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

echo "== Hispa-64, etapas 0, 1, 2, 3, 4 y 5 =="
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

# [etapa 4] los programas de la unidad en memoria
if "$AQUI/programas/hazlos.sh" "$T/prog" > "$T/prog.txt" 2>&1; then
  ok "los programas de prueba: $(tail -1 "$T/prog.txt" | cut -d: -f2) ($(du -cb "$T"/prog/* | tail -1 | cut -f1) octetos)"
else ko "no se hacen los programas de prueba: $(tail -2 "$T/prog.txt" | paste -sd ' ')"; fi
MODS="$T/HISPA64.SIM"
for f in "$T"/prog/*; do MODS="$MODS,$f"; done

# ---- por multiboot (QEMU -kernel): rápido, solo el puerto serie ----
mb1() {   # LÍNEA SALIDA MEMORIA [más opciones de QEMU]
  local l=$1 s=$2 m=$3; shift 3
  rm -f "$s"
  timeout 90 qemu-system-x86_64 -kernel "$T/HISPA64.BIN" -initrd "$MODS" -append "$l" -display none -serial file:"$s" \
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
    && ! grep -q "MAL:" <<< "$v" && grep -q "^Hispa-64 etapa 6: todo en orden.$" <<< "$v"
}
# [etapa 4] los programas, con N procesadores
e4_bien() {   # SALIDA N
  local s=$1 n=$2 v; v=$(serie "$s")
  grep -q "^Programas: syscall y sysret (los números de Linux x86-64), swapgs, una pila del núcleo por hilo$" <<< "$v" \
    && grep -q "^Unidad en memoria: 7 archivos: " <<< "$v" \
    && grep -q "^  argc = 4: HOLA uno dos tres$" <<< "$v" && grep -q "^  uname: Hispa-64 0.6.0 x86_64$" <<< "$v" \
    && grep -q "^  la unidad en memoria: SALUDO.TXT dice «Hola, soy SALUDO.TXT" <<< "$v" && grep -q "^  writev: en tres trozos$" <<< "$v" \
    && grep -q "^HOLA: todo bien (28 pruebas)$" <<< "$v" && grep -q "^Programa «HOLA uno dos tres»: acaba con el código 0, como debe" <<< "$v" \
    && grep -q "^HOLACAS: todo bien (con acentos de la página 437: «añadir», «canción»)$" <<< "$v" \
    && grep -q "^Programa «HOLACAS de Castel»: acaba con el código 0, como debe" <<< "$v" \
    && grep -q "^MEMORIA: 29 pruebas, todas bien$" <<< "$v" \
    && [ "$(grep -c "^CUENTA [1-8]: proceso [0-9]*, 0x[0-9A-F]*, [0-9]*, igual en las dos vueltas$" <<< "$v")" = 8 ] \
    && grep -q "^A la vez: 8 procesos CUENTA: los 8 acaban con su código (10 a 80), sus registros intactos; en los procesadores " <<< "$v" \
    && { [ "$n" = 1 ] || grep "^A la vez:" <<< "$v" | grep -q "procesadores [0-9]*, [0-9]"; } \
    && grep -q "^MALO punteros: 24 llamadas con punteros malos, todas con su error (-EFAULT, -EPERM...); sigue vivo$" <<< "$v" \
    && grep -q "^El programa MALO (proceso [0-9]*) ha intentado leer la dirección 0xFFFFFFFF80100000, y no es suya: es del núcleo.$" <<< "$v" \
    && grep -q "^El programa MALO (proceso [0-9]*) ha intentado escribir en la dirección 0xFFFF800000100000, y no es suya: es del núcleo.$" <<< "$v" \
    && grep -q "^El programa MALO (proceso [0-9]*) ha dividido entre cero.$" <<< "$v" \
    && grep -q "^El programa MALO (proceso [0-9]*) ha intentado ejecutar una instrucción que no existe.$" <<< "$v" \
    && grep -q "se le ha desbordado la pila).$" <<< "$v" && grep -q "que es suya, pero es de solo leer.$" <<< "$v" \
    && [ "$(grep -c "Se cierra el programa; el sistema sigue.$" <<< "$v")" = 11 ] \
    && grep -q "^Malos: 11 programas que fallan, a la vez: todos cerrados con su señal (SIGSEGV 139, SIGFPE 136, SIGILL 132)" <<< "$v" \
    && grep -q "^No se arrancan: «NOESTA»: no está en la unidad; «SALUDO.TXT»: no es un programa ELF$" <<< "$v" \
    && grep -q "^Después: 0 procesos, 0 espacios, 0 tablas de programas; 23 arrancados, 11 cerrados por un fallo" <<< "$v" \
    && grep -q "^  la memoria: la de antes; el montón: [0-9]* bloques (los de antes); las pilas: [0-9]* (las de antes)$" <<< "$v" \
    && { [ "$n" = 1 ] || grep -q "^  quitada (desde el procesador [0-9]*, sin el espacio puesto): 1 aviso por IPI; al 1 le falla la lectura: sí" <<< "$v"; } \
    && { [ "$n" -lt 3 ] || grep -q "; al 2, al volver a ponerlo, también: sí$" <<< "$v"; } \
    && ! grep -q "MAL:" <<< "$v" && grep -q "^Hispa-64 etapa 6: todo en orden.$" <<< "$v"
}
e4_dice() {   # SALIDA
  serie "$1" | grep -E '^Programa|^HOLA|^MEMORIA|^A la vez|^Malos|^Después|^  la memoria|^  quitada|MAL|parado' | cut -c1-120 | paste -sd '|'
}
e4_ok() {   # SALIDA N QUÉ
  if e4_bien "$1" "$2"; then
    ok "etapa 4 $3: HOLA, HOLACAS, MEMORIA, 8 CUENTA a la vez ($(serie "$1" | grep '^A la vez:' | grep -o 'procesadores [0-9, ]*'| sed 's/ *$//')), MALO (24 punteros malos, 11 fallos a la vez); $(serie "$1" | grep '^Después:' | cut -d';' -f2 | sed 's/^ //'); la memoria, la de antes$( [ "$2" -gt 1 ] && echo '; la TLB de un espacio: 1 aviso, solo al que lo tiene puesto')"
  else ko "etapa 4 $3 no dice lo que debe: $(e4_dice "$1")"; fi
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
if [ $c = 85 ] && hay $S "^Hispa-64 0.6.0 ─ Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.$" \
   && hay $S "^Cargador: qemu, multiboot$" && hay $S "^Memoria: 12[0-9],[0-9] MB libres en [0-9]* trozos (de 127,4 MB que da el firmware); [0-9]* KB usados por el núcleo$" \
   && hay $S "^  sin la identidad de abajo: 1 MB ya no está en las tablas$" && hay $S "los nombres de [0-9]* funciones (HISPA64.SIM)$" \
   && hay $S "^El núcleo, en la mitad alta: 0xFFFFFFFF80100000 (código, de solo leer)" && hay $S "^TSS con pilas propias (IST)" \
   && hay $S "^Páginas: de 4 KB, puesta, leída, quitada y puesta en otro marco (la TLB, al día); de 2 MB, también; de 1 GB, no (el procesador no las tiene)$" \
   && hay $S "^  el código del núcleo, sin escribir: sí; los datos, sin ejecutar: sí; la guarda de la pila, sin poner: sí$" \
   && hay $S "^Montón: 2000 reservas y liberaciones al azar .*, comprobadas; al acabar, 0 en uso y la memoria física, la de antes" \
   && hay $S "Punto de parada (int3) número 1 .*: atendido, sigue." && hay $S "^Hispa-64 etapa 6: todo en orden.$"; then
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
e4_ok $S 1 "por multiboot con 1 procesador («pc», 128 MB)"
# con «q35», 4 procesadores y un puerto PCIe con un xHCI detrás (el bus 1)
PCIQ="-machine q35 -smp 4 -device pcie-root-port,id=rp1,bus=pcie.0,chassis=1 -device qemu-xhci,bus=rp1"
# shellcheck disable=SC2086
mb1 apagar "$T/q35.txt" 256 $PCIQ; c=$?
S=$T/q35.txt
# shellcheck disable=SC2086
if [ $c = 85 ] && hay $S "^  procesadores (MADT): 4 encendidos (APIC 0, 1, 2, 3)$" && hay $S "^  MCFG: PCIe por ECAM en 0xB0000000, segmento 0, buses 0 a 255$" \
   && hay $S "^  FADT: .*8042: sí" && tics_bien $S "el HPET" && hay $S "^IOAPIC: el temporizador 0 del HPET, por la GSI 1[6-9], al vector 0x3E: 20 de 20 interrupciones (cada 10 ms)$" \
   && hay $S "^PCI (PCIe, por ECAM): [0-9]* funciones en los buses 0 a 255, 1 puentes$" && hay $S "^  01:00.0 1B36:000D bus serie: USB 3 (xHCI)" \
   && hay $S "^      BAR0 0x[0-9A-F]*, 16 KB de memoria de 64 bits$" && pci_igual $S -m 256 $PCIQ && hay $S "^Hispa-64 etapa 6: todo en orden.$"; then
  ok "etapa 2, por multiboot, «q35» con 4 procesadores: la MADT con 4, PCIe por ECAM, $(pci_nucleo $S | wc -l) funciones (el puente y el xHCI del bus 1, con su BAR de 64 bits), las mismas que dice QEMU; «$(serie $S | grep '^Tic:')»"
else ko "etapa 2 con «q35» y 4 procesadores no dice lo que debe (código $c): $(serie $S | grep -E 'MADT|^Tic|^IOAPIC:|^PCI|parado' | paste -sd ' ')"; diff <(pci_nucleo $S) <(pci_qemu -m 256 $PCIQ) | head -5 | sed 's/^/        /'; fi
if e3_bien $S 4 && hay $S "^  con INIT-SIPI-SIPI y el trampolín en 0x[0-9A-F]* (16, 32 y 64 bits)"; then
  ok "etapa 3 con 4 procesadores (q35): «$(serie $S | grep '^Procesadores:')»; los 100 hilos, el número exacto; «$(serie $S | grep '^Reparto:' | cut -d: -f2- | cut -c1-80)»; la TLB de los demás, vaciada con un IPI"
else ko "etapa 3 con 4 procesadores (q35) no dice lo que debe: $(e3_dice $S)"; fi
e4_ok $S 4 "por multiboot con 4 procesadores (q35)"
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
if e4_bien $S 8 && hay $S "^  SMEP: sí; SMAP: sí; UMIP: sí; PCID: "; then e4_ok $S 8 "con 8 procesadores y -cpu max (SMEP, SMAP y UMIP)"
else ko "etapa 4 con -cpu max no dice lo que debe: $(serie $S | grep SMEP) $(e4_dice $S)"; fi
mb1 apagar "$T/smp2.txt" 256 -machine pc -smp 2; c=$?
S=$T/smp2.txt
if [ $c = 85 ] && e3_bien $S 2 && hay $S "^  quitada sin avisar y puesta en otro marco (con 0x2222): el 1 "; then
  ok "etapa 3 con 2 procesadores («pc»): $(serie $S | grep '^  quitada sin avisar' | sed 's/.*: el 1/sin avisar, el 1/'); con aviso, le falla"
else ko "etapa 3 con 2 procesadores («pc») no dice lo que debe (código $c): $(e3_dice $S)"; fi
e4_ok $S 2 "por multiboot con 2 procesadores («pc»)"
mb1 "apagar ocioso=mwait" "$T/mwait.txt" 256 -machine q35 -smp 4 -cpu max; c=$?
S=$T/mwait.txt
if [ $c = 85 ] && e3_bien $S 4 && hay $S "la tarea ociosa, con mwait, FORZADO (ocioso=mwait)$"; then
  ok "ocioso=mwait con -cpu max y 4 procesadores: la tarea ociosa con monitor/mwait, y todo igual"
else ko "ocioso=mwait no sale (código $c): $(serie $S | grep -E '^Planificador' | cut -c1-120) $(e3_dice $S)"; fi
mb1 "apagar sinpruebas sinsmp" "$T/sinsmp.txt" 256 -machine q35 -smp 4; c=$?
if [ $c = 85 ] && hay "$T/sinsmp.txt" "^Procesadores: 1 en marcha (APIC 0) (sinsmp: los demás, apagados)$" && hay "$T/sinsmp.txt" "^Hispa-64 etapa 6: todo en orden.$"; then
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
   && hay $S "^Hispa-64 etapa 6: todo en orden.$"; then
  ok "por multiboot con 3 GB y -cpu max: «$(serie $S | grep '^Memoria:' | cut -d';' -f1)»; el mapa directo con páginas de 1 GB; una de 1 GB puesta, leída y quitada"
else ko "por multiboot con 3 GB no dice lo que debe (código $c): $(serie $S | grep -E 'Memoria:|Páginas|parado' | paste -sd ' ')"; fi
mb1 apagar "$T/mb8g.txt" 8G; c=$?
S=$T/mb8g.txt
if [ $c = 85 ] && hay $S "^Memoria: 7,9 GB libres en [0-9]* trozos (de 7,9 GB" && hay $S "en páginas de 1 GB: 0; de 2 MB: 4[0-9][0-9][0-9];" \
   && hay $S "^Hispa-64 etapa 6: todo en orden.$"; then
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

# ---- [etapa 5] los discos y los archivos ----
# los discos de prueba (programas/hazdiscos.sh), y una copia de trabajo de cada uno
DS=$T/discos
if "$AQUI/programas/hazdiscos.sh" "$DS" > "$T/discos.txt" 2>&1; then
  ok "los discos de prueba (hazdiscos.sh: GPT y MBR con una lógica, FAT32 con mkfs.fat, exFAT con hazexfat.py; 3 TB dispersos): $(tail -1 "$T/discos.txt" | cut -d: -f2)"
else ko "no se hacen los discos de prueba: $(tail -2 "$T/discos.txt" | paste -sd ' ')"; fi
copia_discos() { rm -rf "$T/w5"; mkdir -p "$T/w5"; cp --sparse=always "$DS"/*.img "$T/w5/"; }
# una partición de una imagen, suelta: IMAGEN PRIMER_SECTOR SECTORES SALIDA
saca() { dd if="$1" of="$4" bs=512 skip="$2" count="$3" status=none; }
# fsck.fat -n de una partición: 0 y sin quejas (también el resumen de libres)
fsck_bien() {   # IMAGEN PRIMER_SECTOR SECTORES
  saca "$1" "$2" "$3" "$T/fsck.part"
  local r c
  r=$(fsck.fat -n -v "$T/fsck.part" 2>&1); c=$?
  [ $c = 0 ] && ! grep -qiE 'wrong|differ|dirty|bad|lost|invalid' <<< "$r"
}
fsck_dice() { fsck.fat -n "$T/fsck.part" 2>&1 | tail -1; }
# lo que deja ARCHIVOS en un volumen (comprobado desde el anfitrión, con mtools)
lo_dejado() {   # IMAGEN PRIMER_SECTOR SECTORES
  local p="$T/dejado.part" d="::Prueba de Hispa-64"
  saca "$1" "$2" "$3" "$p"
  [ "$(mtype -i "$p" ::ULTIMO.TXT 2> /dev/null)" = "Escrito sin fsync: lo vacía el apagado." ] \
    && [ "$(mtype -i "$p" "$d/adios.txt" 2> /dev/null)" = "$(printf 'Hola, MUNDO\nsegunda línea')" ] \
    && [ "$(mtype -i "$p" "$d/Un nombre muy largo con acentos áéíóú y eñe ñ.txt" 2> /dev/null)" = "¡Hola!" ] \
    && [ "$(mtype -i "$p" "$d/sub2/Dentro/movido.txt" 2> /dev/null)" = "me han movido" ] \
    && [ "$(mdir -b -i "$p" "$d/Muchos" 2> /dev/null | grep -c 'archivo número [0-9][13579].txt$')" = 20 ] \
    && [ "$(mdir -b -i "$p" "$d/Muchos" 2> /dev/null | wc -l)" = 20 ] \
    && mcopy -n -i "$p" "$d/grande.bin" "$T/grande.bin" 2> /dev/null \
    && python3 -I -c '
import sys
d = open(sys.argv[1], "rb").read()
e = bytearray(((i * 131 + (i >> 8)) & 255) for i in range(1234567))
e[500000:500100] = b"X" * 100
sys.exit(0 if d == bytes(e) else 1)' "$T/grande.bin" \
    && [ "$(mdir -i "$p" "$d" 2> /dev/null | grep -c '^hueco *bin *3 ')" = 1 ] \
    && ! mdir -i "$p" "$d" 2> /dev/null | grep -qE 'hola *txt|uno *txt|dos *txt|abierto|desde|movido'
}
e5_bien() {   # SALIDA: lo que dice siempre una vuelta de la etapa 5 sin fallos
  local v; v=$(serie "$1")
  grep -q "^Después de los archivos: 0 abiertos, [0-9]* nodos (los de antes: las raíces), el montón como antes$" <<< "$v" \
    && ! grep -q "MAL:" <<< "$v" && grep -q "^Hispa-64 etapa 6: todo en orden.$" <<< "$v"
}
e5_dice() { serie "$1" | grep -E '^NVMe|^SATA|^  /|^ARCHIVOS|MAL|^Después de los|parado|^Al apagar' | cut -c1-140 | paste -sd '|'; }
NVME5="-drive file=$T/w5/nvme.img,if=none,id=n0,format=raw -device nvme,serial=hispa64,drive=n0"
CRUDO5="-drive file=$T/w5/crudo.img,if=none,id=n1,format=raw -device nvme,serial=crudo,drive=n1"
SATA5="-drive file=$T/w5/sata.img,if=none,id=s0,format=raw -device ide-hd,drive=s0,bus=ide.0"
# 1. q35 con 4 procesadores: NVMe (GPT), un NVMe crudo y SATA (MBR con una exFAT), escribiendo
copia_discos
# shellcheck disable=SC2086
mb1 "apagar escribir" "$T/e5.txt" 256 -machine q35 -smp 4 $NVME5 $CRUDO5 $SATA5; c=$?
S=$T/e5.txt
if [ $c = 85 ] && hay $S "^NVMe: 00:0[0-9A-F].0 1B36:0010, «QEMU NVMe Ctrl» (serie hispa64), versión 1.4$" \
   && hay $S "^  BAR 0 en 0x[0-9A-F]*; 4 colas de E/S (una por procesador) de 64 órdenes; MSI-X; hasta 128 KB por orden; con caché de escritura$" \
   && hay $S "^SATA (AHCI): 00:1F.2 8086:2922, AHCI 1.0, 6 puertos, direcciones de 64 bits; MSI; 1 disco$" \
   && hay $S "^  nvme0: «QEMU NVMe Ctrl», 112,0 MB (229376 sectores de 512 octetos), tabla GPT; se puede escribir$" \
   && hay $S "^    nvme0p1: 40,0 MB, desde el sector 2048, EFI$" && hay $S "^    nvme0p2: 70,9 MB, desde el sector 83968, datos (Windows: FAT, exFAT o NTFS)$" \
   && hay $S "^  sata0: «QEMU HARDDISK», 128,0 MB (262144 sectores de 512 octetos), tabla MBR; se puede escribir$" \
   && hay $S "^    sata0p1: 84,0 MB, desde el sector 2048, FAT32$" && hay $S "^    sata0p5: 32,0 MB, desde el sector 182272, exFAT o NTFS$" \
   && hay $S "^  nvme1: «QEMU NVMe Ctrl», 16,0 MB (32768 sectores de 512 octetos), sin tabla de particiones; se puede escribir$" \
   && hay $S "^Archivos (el VFS): 5 montajes$" && hay $S "^  /nvme0p1: FAT32 «ESP», .*; de leer y escribir$" \
   && hay $S "^  /nvme0p2: FAT32 «DATOS», 69,8 MB en 143076 cúmulos de 512 octetos, " && hay $S "^  /sata0p1: FAT32 «SATAFAT», 83,3 MB en 85332 cúmulos de 1 KB" \
   && hay $S "^  /sata0p5: exFAT «EXFATPRB», 31,9 MB en 8168 cúmulos de 4 KB; de solo leer$" \
   && hay $S "^La caché: los primeros 4 KB de los 3 discos, iguales por la caché y a pelo$" \
   && hay $S "^  escribir y leer a pelo: 1 sector, 8, 256 (128 KB), 500 (varias órdenes) y los 300 del final: iguales; más allá del final, no$" \
   && hay $S "^  4 hilos a la vez, cada uno en su procesador, 32 lecturas de 128 KB cada uno: bien; colas de NVMe usadas: 4$" \
   && hay $S "^ARCHIVOS: 3 discos de prueba (2 escritos); todo bien ([0-9]* pruebas)$" && hay $S "^Programa «ARCHIVOS auto»: acaba con el código 0, como debe" \
   && hay $S "^      Canción de cuna ñandú.txt (37 octetos)$" && hay $S "^Al apagar: [1-9][0-9]* bloques sin escribir en la caché; vaciados (sync): [1-9][0-9]* escritos$" \
   && e5_bien $S; then
  ok "etapa 5 (q35, 4 procesadores, MSI-X y MSI): NVMe con 4 colas, SATA, GPT y MBR con una lógica, 4 volúmenes (FAT32 y exFAT); «$(serie $S | grep '^  leerlo entero' | cut -d: -f2- | sed 's/^ //')»; «$(serie $S | grep '^ARCHIVOS: 3')»; al apagar, lo sin vaciar, al disco"
else ko "etapa 5 en q35 no dice lo que debe (código $c): $(e5_dice $S)"; fi
e4_ok $S 4 "con los discos (q35, 4 procesadores)"
if fsck_bien "$T/w5/nvme.img" 83968 145375 && lo_dejado "$T/w5/nvme.img" 83968 145375 \
   && fsck_bien "$T/w5/sata.img" 2048 172032 && lo_dejado "$T/w5/sata.img" 2048 172032 \
   && fsck_bien "$T/w5/nvme.img" 2048 81920; then
  ok "desde el anfitrión: fsck.fat sin quejas en los dos FAT32 escritos (y la EFI) y, con mtools, lo que ha dejado ARCHIVOS (los nombres largos con acentos, grande.bin byte a byte, los 20 de Muchos, ULTIMO.TXT, que vació el apagado)"
else ko "desde el anfitrión, los volúmenes escritos no están bien: $(fsck_dice)"; fi
# 2. 3 TB: las particiones de más allá de 2^32 sectores, por NVMe y por SATA (LBA de 48 bits)
# shellcheck disable=SC2086
mb1 "apagar escribir" "$T/e5g.txt" 512 -machine q35 -smp 2 -drive file="$T/w5/grande.img",if=none,id=n0,format=raw -device nvme,serial=grande,drive=n0 \
  -drive file="$T/w5/grande2.img",if=none,id=s0,format=raw -device ide-hd,drive=s0,bus=ide.1; c=$?
S=$T/e5g.txt
if [ $c = 85 ] && hay $S "^  nvme0: «QEMU NVMe Ctrl», 3072,0 GB (6442450944 sectores de 512 octetos), tabla GPT; se puede escribir$" \
   && hay $S "^    nvme0p2: 80,0 MB, desde el sector 4999999488, " && hay $S "^    sata0p2: 80,0 MB, desde el sector 4999999488, " \
   && hay $S "^  /nvme0p2: FAT32 «G2ALTO»" && hay $S "^  /sata0p2: FAT32 «G2ALTO»" \
   && hay $S "^ARCHIVOS: 4 discos de prueba (4 escritos); todo bien ([0-9]* pruebas)$" && e5_bien $S \
   && fsck_bien "$T/w5/grande.img" 4999999488 163840 && lo_dejado "$T/w5/grande.img" 4999999488 163840 \
   && fsck_bien "$T/w5/grande2.img" 4999999488 163840 && lo_dejado "$T/w5/grande2.img" 4999999488 163840; then
  ok "etapa 5 con dos discos de 3 TB (NVMe y SATA): las particiones desde el sector 4.999.999.488 (más allá de 2^32), escritas y leídas; fsck.fat sin quejas y lo dejado, bien"
else ko "etapa 5 con discos de 3 TB no sale (código $c): $(e5_dice $S) $(fsck_dice)"; fi
# 3. sin interrupciones (sinmsix sinmsi), «pc» con una AHCI aparte: se mira la cola
copia_discos
# shellcheck disable=SC2086
mb1 "apagar escribir sinmsix sinmsi" "$T/e5s.txt" 256 -machine pc -smp 2 $NVME5 $CRUDO5 -device ahci,id=ah \
  -drive file="$T/w5/sata.img",if=none,id=s0,format=raw -device ide-hd,drive=s0,bus=ah.2; c=$?
S=$T/e5s.txt
if [ $c = 85 ] && [ "$(serie $S | grep -c '; sin interrupciones (se mira la cola);')" = 3 ] \
   && hay $S "^ARCHIVOS: 3 discos de prueba (2 escritos); todo bien" && e5_bien $S \
   && fsck_bien "$T/w5/sata.img" 2048 172032 && lo_dejado "$T/w5/sata.img" 2048 172032; then
  ok "etapa 5 sin MSI ni MSI-X («pc», 2 procesadores, la AHCI aparte): las tres controladoras mirando la cola, y todo igual (fsck.fat, bien)"
else ko "etapa 5 sin interrupciones no sale (código $c): $(e5_dice $S)"; fi
# 4. sin «escribir»: nada cambia en los discos (ni un octeto)
copia_discos
antes=$(cat "$T"/w5/nvme.img "$T"/w5/sata.img "$T"/w5/crudo.img | md5sum)
# shellcheck disable=SC2086
mb1 "apagar" "$T/e5r.txt" 256 -machine q35 -smp 2 $NVME5 $CRUDO5 $SATA5; c=$?
S=$T/e5r.txt
if [ $c = 85 ] && [ "$(cat "$T"/w5/nvme.img "$T"/w5/sata.img "$T"/w5/crudo.img | md5sum)" = "$antes" ] \
   && [ "$(serie $S | grep -c '; solo se lee$')" = 3 ] && hay $S "^  no se escribe en él (sin «escribir» en la línea de órdenes)$" \
   && [ "$(serie $S | grep -c '^  de solo leer (EROFS): no se escribe$')" = 3 ] \
   && hay $S "^ARCHIVOS: 3 discos de prueba (0 escritos); todo bien" && ! hay $S "^Al apagar" && e5_bien $S; then
  ok "etapa 5 sin «escribir»: todo de solo leer (EROFS), y los discos, iguales octeto a octeto (md5sum)"
else ko "etapa 5 sin «escribir» no sale (código $c): $(e5_dice $S)"; fi
# 5. el núcleo de antes de la etapa 5 no escribía; el de ahora, con los discos y sin pruebas
# shellcheck disable=SC2086
mb1 "apagar sinpruebas escribir" "$T/e5n.txt" 256 -machine q35 -smp 4 $NVME5 $CRUDO5 $SATA5; c=$?
if [ $c = 85 ] && hay "$T/e5n.txt" "^Hispa-64 etapa 6: todo en orden.$" && hay "$T/e5n.txt" "^  4 hilos a la vez"; then
  ok "sinpruebas con los discos: el disco crudo sí (es del núcleo), los programas no"
else ko "sinpruebas con los discos no sale (código $c): $(e5_dice "$T/e5n.txt")"; fi

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
  if [ $c = 85 ] && grep -q "Hispa-64 0.6.0 ─ Copyright" <<< "$S" && grep -q "^Cargador: GRUB .*, multiboot2, con UEFI de 64 bits$" <<< "$S" \
     && grep -q "^Pantalla: [0-9]* x [0-9]* puntos, 32 bits por punto" <<< "$S" && grep -q "^  consola de [0-9]* x [0-9]* letras" <<< "$S" \
     && grep -q "^  la pantalla, en 0xFFFFC00000000000, con escritura combinada (PAT)$" <<< "$S" \
     && grep -q "^  módulo del cargador: HISPA64.SIM, .*: los nombres de [0-9]* funciones (HISPA64.SIM)$" <<< "$S" \
     && grep -q "^Memoria: 1[0-9][0-9],[0-9] MB libres en [0-9]* trozos" <<< "$S" && grep -q "^Montón: 2000 reservas" <<< "$S" \
     && grep -q "^ACPI: 2.0 o más, de «BOCHS »; XSDT en 0x[0-9A-F]*, [0-9]* tablas: FACP APIC HPET MCFG .*DSDT (todas con la suma bien)$" <<< "$S" \
     && grep -q "ACPI NVS" <<< "$S" && grep -q "^Hispa-64 etapa 6: todo en orden.$" <<< "$S"; then
    ok "por UEFI de 64 bits (OVMF + GRUB, multiboot2) con 128 MB: «$(grep '^Cargador' <<< "$S")»; «$(grep '^Memoria:' <<< "$S" | cut -d';' -f1)»; la pantalla del GOP con escritura combinada, el módulo HISPA64.SIM, RSDP 2.0 y «todo en orden»"
  else ko "por UEFI con 128 MB no dice lo que debe (código $c): $(grep -E 'Memoria:|pantalla, en|parado' <<< "$S" | paste -sd ' ')"; fi
  e4_ok "$T/uefi.txt" 1 "por UEFI (GRUB, module2) con 1 procesador"
  uefi "$T/uefi-max.txt" -m 3G -cpu max -smp 4,cores=4 -monitor none; c=$?
  S=$(serie "$T/uefi-max.txt")
  if [ $c = 85 ] && grep -q "páginas de 1 GB: sí" <<< "$S" && grep -q "; 4 procesadores lógicos en el paquete" <<< "$S" \
     && grep -q "^  0x00000001.*disponible$" <<< "$S" && grep -q "^Memoria: 2,9 GB libres" <<< "$S" && grep -q "en páginas de 1 GB: [1-9]" <<< "$S" \
     && grep -q "; de 1 GB, también$" <<< "$S" && grep -q "^Hispa-64 etapa 6: todo en orden.$" <<< "$S" \
     && grep -q "^  procesadores (MADT): 4 encendidos (APIC 0, 1, 2, 3)$" <<< "$S" && tics_bien "$T/uefi-max.txt" "el HPET" 50 \
     && grep -q "^IOAPIC: el temporizador 0 del HPET, por la GSI 1[6-9], al vector 0x3E: 20 de 20 interrupciones (cada 10 ms)$" <<< "$S" \
     && grep -q "^PCI (PCIe, por ECAM): [0-9]* funciones en los buses 0 a 255, 1 BAR por encima de 4 GB$" <<< "$S" \
     && grep -q "^      BAR4 0x[0-9A-F]\{9,\}, 16 KB de memoria de 64 bits (por encima de 4 GB), precargable$" <<< "$S" \
     && pci_igual "$T/uefi-max.txt" -machine q35 -m 3G -cpu max -smp 4,cores=4 -drive file="$T/uefi/hispa64.img",format=raw,if=virtio \
     && e3_bien "$T/uefi-max.txt" 4; then
    ok "por UEFI con -cpu max, 4 núcleos y 3 GB (1 por encima de 4 GB): «$(grep '^Memoria:' <<< "$S" | cut -d';' -f1)»; el mapa directo y la prueba con páginas de 1 GB; la MADT con 4; «$(grep '^Tic:' <<< "$S")»; PCIe con la BAR del disco virtio por encima de 4 GB, la misma lista que QEMU"
  else ko "por UEFI con -cpu max y 3 GB no dice lo que debe (código $c): $(grep -E 'lógicos|Memoria:|parado' <<< "$S" | paste -sd ' ') $(e3_dice "$T/uefi-max.txt")"; fi
  e4_ok "$T/uefi-max.txt" 4 "por UEFI con 4 procesadores y -cpu max"
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
  # [etapa 5] por UEFI desde el disco de la imagen puesto en SATA (q35): la
  # partición EFI y la de pruebas, a la vista; un programa arrancado desde el disco
  "$AQUI/imagen.sh" "$T/uefi5" prueba "apagar escribir dir=/sata0p1 programa=/sata0p1/boot/prog/HOLACAS" > "$T/img5.txt" 2>&1 \
    || ko "imagen.sh (etapa 5) no hace la imagen: $(tail -2 "$T/img5.txt" | paste -sd ' ')"
  rm -f "$T/uefi5.txt"; cp $OVMF_V "$T/vars5.fd"
  timeout 200 qemu-system-x86_64 -machine q35 -drive if=pflash,format=raw,readonly=on,file=$OVMF_C -drive if=pflash,format=raw,file="$T/vars5.fd" \
    -drive file="$T/uefi5/hispa64.img",if=none,id=b0,format=raw -device ide-hd,drive=b0,bus=ide.0 -m 512 -smp 4 \
    -display none -serial file:"$T/uefi5.txt" -monitor none $DBG -no-reboot -nic none > /dev/null 2>&1; c=$?
  S="$T/uefi5.txt"
  read -r i2 f2 <<< "$(sgdisk -i 2 "$T/uefi5/hispa64.img" | awk '/First sector/ {a=$3} /Last sector/ {b=$3} END {print a, b}')"
  if [ $c = 85 ] && hay $S "^Cargador: GRUB .*, multiboot2, con UEFI de 64 bits$" && hay $S "^    sata0p1: 80,0 MB, desde el sector 2048, EFI$" \
     && hay $S "^  /sata0p1: FAT32 «HISPA64», " && hay $S "^  /sata0p2: FAT32 «PRUEBAS», " \
     && hay $S "^        BOOTX64.EFI ([0-9]* octetos)$" && hay $S "^      HISPA64.BIN ([0-9]* octetos)$" \
     && hay $S "^ARCHIVOS: 1 disco de prueba (1 escrito); todo bien" \
     && hay $S "^Programa «/sata0p1/boot/prog/HOLACAS»: acaba con el código 0, como debe" && e5_bien $S \
     && fsck_bien "$T/uefi5/hispa64.img" 2048 163840 && fsck_bien "$T/uefi5/hispa64.img" "$i2" $(( f2 - i2 + 1 )) \
     && lo_dejado "$T/uefi5/hispa64.img" "$i2" $(( f2 - i2 + 1 )); then
    ok "etapa 5 por UEFI desde SATA (el disco de la imagen): la partición EFI a la vista (BOOTX64.EFI, HISPA64.BIN), HOLACAS arrancado desde ella, la de pruebas escrita; fsck.fat, bien en las dos"
  else ko "etapa 5 por UEFI desde SATA no sale (código $c): $(e5_dice $S) $(fsck_dice)"; fi
  # [etapa 4] el CD (grub-mkrescue): por BIOS (-cdrom) y por UEFI
  if command -v grub-mkrescue > /dev/null && CD=1 "$AQUI/imagen.sh" "$T/cd" prueba > "$T/cd.txt" 2>&1 && [ -s "$T/cd/hispa64-cd.iso" ]; then
    rm -f "$T/cd-bios.txt"
    timeout 150 qemu-system-x86_64 -cdrom "$T/cd/hispa64-cd.iso" -m 256 -smp 2 -display none -serial file:"$T/cd-bios.txt" -monitor none $DBG -no-reboot -nic none > /dev/null 2>&1; c=$?
    if [ $c = 85 ] && grep -q "^Cargador: GRUB .*, multiboot2, con la BIOS$" <<< "$(serie "$T/cd-bios.txt")" && e4_bien "$T/cd-bios.txt" 2; then
      ok "el CD ($(( $(stat -c %s "$T/cd/hispa64-cd.iso") / 1048576 )) MB, grub-mkrescue) por BIOS (-cdrom, 2 procesadores): «$(serie "$T/cd-bios.txt" | grep '^Cargador')», los programas y «todo en orden»"
    else ko "el CD por BIOS no arranca bien (código $c): $(serie "$T/cd-bios.txt" | grep -E '^Cargador|parado' | paste -sd ' ') $(e4_dice "$T/cd-bios.txt")"; fi
    rm -f "$T/cd-uefi.txt"; cp $OVMF_V "$T/vars3.fd"
    timeout 150 qemu-system-x86_64 -machine q35 -drive if=pflash,format=raw,readonly=on,file=$OVMF_C -drive if=pflash,format=raw,file="$T/vars3.fd" \
      -cdrom "$T/cd/hispa64-cd.iso" -m 256 -smp 4 -display none -serial file:"$T/cd-uefi.txt" -monitor none $DBG -no-reboot -nic none > /dev/null 2>&1; c=$?
    if [ $c = 85 ] && grep -q "^Cargador: GRUB .*, multiboot2, con UEFI de 64 bits$" <<< "$(serie "$T/cd-uefi.txt")" && e4_bien "$T/cd-uefi.txt" 4; then
      ok "el CD por UEFI (OVMF, q35, 4 procesadores): «$(serie "$T/cd-uefi.txt" | grep '^Cargador')», los programas y «todo en orden»"
    else ko "el CD por UEFI no arranca bien (código $c): $(serie "$T/cd-uefi.txt" | grep -E '^Cargador|parado' | paste -sd ' ') $(e4_dice "$T/cd-uefi.txt")"; fi
  else ko "no se hace el CD: $(tail -2 "$T/cd.txt" | paste -sd ' ')"; fi
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
