#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo64/pruebas.sh - las pruebas de Hispa-64 (docs/64/HISPA-64.md), etapas 0 a 10:
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
#     programas/hazdiscos.sh (NVMe con GPT, SATA con MBR: FAT32, FAT12 y cuatro
#     lógicas: exFAT, FAT16 de tipo 0x0E, FAT16 de tipo 0x04 y otra exFAT con
#     cúmulos de 512 octetos,
#     dos de 3 TB con una partición más allá de 2^32 sectores, uno crudo):
#     NVMe (con MSI-X y una cola por procesador) y AHCI (con MSI), y sin
#     ellas («sinmsix sinmsi», con la AHCI aparte de «pc»); las particiones;
#     el disco crudo escrito y leído a pelo y por cuatro hilos a la vez; el
#     programa ARCHIVOS (crear, escribir, leer, O_APPEND, O_TRUNC,
#     pread/pwrite, huecos, ftruncate, nombres largos con acentos, 40
#     archivos, getdents64, rename, unlink de uno abierto, las *at, chdir,
#     los errores, un nombre de 255 letras; en FAT32, FAT16, FAT12 y exFAT;
#     la raíz fija de la FAT12, llena hasta ENOSPC); al apagar, lo que
#     quedaba en la caché, al disco; y desde el anfitrión, fsck.fat sin quejas
#     y lo dejado comprobado con mtools (FAT32, FAT16 y FAT12) y con
#     miraexfat.py (exFAT: el arranque y su suma, VolumeDirty limpio, las
#     sumas y el NameHash de cada grupo, las cadenas y el mapa de bits, y lo
#     dejado); sin «escribir», ni un octeto cambiado; y por UEFI
#     desde el disco de la imagen puesto en SATA (la partición EFI a la vista,
#     un programa arrancado desde ella).
#   - [etapa 7] musl (programas/hazmusl.sh: musl y Lua, compilados en build/
#     la primera vez): MHOLA (estático, dinámico con ld-musl y libsaludo.so, y
#     estático de posición libre), MHILOS (8 hilos, mutex, condiciones, TLS,
#     semáforos, AVX), MSENALES (las señales de verdad: sigaction, la máscara,
#     los fallos atendidos, sigaltstack, SA_RESTART, SIGCHLD, kill a un hijo
#     que no llama al sistema), MPROCESOS (fork con copiar al escribir,
#     execve, tuberías, posix_spawn), MMAPA (mmap de archivos, MAP_SHARED
#     anónima) y Lua, por multiboot con 1, 4 y 8 procesadores (-cpu max: AVX)
#     y por UEFI con 1 y 4; después, nada de más; y en el intérprete, Lua y
#     Ctrl+C a un programa que no llama al sistema (MBUCLE). Las vueltas que no
#     las necesitan llevan «sinmusl».
#   - [etapa 8] MDISP en cada arranque (/dev/null, /dev/zero, /dev/shm entre
#     dos programas, /dev/fb0 con su doble búfer, /dev/input/event0); y por
#     UEFI: los sucesos del teclado y la tableta (MDISP sucesos), VENTANAS con
#     NOTAS (guarda en exFAT), arrastrar una ventana, el menú Inicio, Pluma 2 y
#     Cuentas 2 (guardan en FAT32), Alt+F4, salir del escritorio, el cursor y
#     cls; los pantallazos, comprobados con pantallazo.py (cuenta, oscuros,
#     difiere).
#   - [etapa 9] MRED en cada arranque (los sockets por 127.0.0.1, eventfd,
#     timerfd, /dev/urandom); y la red de verdad con la de usuario de QEMU:
#     e1000, e1000e, virtio-net y la Realtek (con la RTL8139C+, «realtek=
#     ensayo»), con 1, 2, 4 y 8 procesadores: DHCP o IP fija, PING, IFCONFIG,
#     DESCARGA por HTTP y HTTPS (sha256), DNS, NTP (la hora puesta) y ECO de ida
#     y vuelta, contra los servidores de prueba del anfitrión; la zona horaria.
#   - [etapa 10] MSENALES con alarm y setitimer; MRED con /etc/localtime; MCXX
#     (C++ con la libstdc++ y musl: cxx.sh) y MTEXTO (HarfBuzz y FreeType: lo
#     mismo que en Linux) en cada arranque con musl; la red por USB (usb-net,
#     CDC-ECM), también enchufada y desenchufada en marcha; el CMOS puesto en
#     hora por NTP; las INTx por el _PRT (MIRAPRT con las tablas del ASUS, el
#     Toshiba y el HP Mini; q35 y pc); TCP con pérdidas (red=pierde): lo
#     desordenado, la escala de ventana y SACK, de Hispa-64 a Hispa-64.
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

echo "== Hispa-64, etapas 0 a 10 =="
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
# [etapa 10] lo que dibuja MTEXTO (HarfBuzz y FreeType) en Linux: en Hispa-64, lo mismo
MTX=$(cd "$T/prog" && ./MTEXTO DEJAVU.TTF 2> /dev/null | grep '^  dibujado: ')

# ---- por multiboot (QEMU -kernel): rápido, solo el puerto serie ----
mb1() {   # LÍNEA SALIDA MEMORIA [más opciones de QEMU]
  local l=$1 s=$2 m=$3; shift 3
  rm -f "$s"
  timeout 150 qemu-system-x86_64 -kernel "$T/HISPA64.BIN" -initrd "$MODS" -append "$l" -display none -serial file:"$s" \
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
    && ! grep -q "MAL:" <<< "$v" && grep -q "^Hispa-64 etapa 10: todo en orden.$" <<< "$v"
}
# [etapa 4] los programas, con N procesadores
e4_bien() {   # SALIDA N
  local s=$1 n=$2 v; v=$(serie "$s")
  grep -q "^Programas: syscall y sysret (los números de Linux x86-64), swapgs, una pila del núcleo por hilo$" <<< "$v" \
    && grep -q "^Unidad en memoria: 39 archivos: " <<< "$v" \
    && grep -q "^  argc = 4: HOLA uno dos tres$" <<< "$v" && grep -q "^  uname: Hispa-64 0.10.0 x86_64$" <<< "$v" \
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
    && [ "$(sed -n '1,/^Después: /p' <<< "$v" | grep -c "Se cierra el programa; el sistema sigue.$")" = 11 ] \
    && grep -q "^Malos: 11 programas que fallan, a la vez: todos cerrados con su señal (SIGSEGV 139, SIGFPE 136, SIGILL 132)" <<< "$v" \
    && grep -q "^No se arrancan: «NOESTA»: no está en la unidad; «SALUDO.TXT»: no es un programa ELF$" <<< "$v" \
    && grep -q "^Después: 0 procesos, 0 espacios, 0 tablas de programas; 23 arrancados, 11 cerrados por un fallo" <<< "$v" \
    && grep -q "^  la memoria: la de antes; el montón: [0-9]* bloques (los de antes); las pilas: [0-9]* (las de antes)$" <<< "$v" \
    && { [ "$n" = 1 ] || grep -q "^  quitada (desde el procesador [0-9]*, sin el espacio puesto): 1 aviso por IPI; al 1 le falla la lectura: sí" <<< "$v"; } \
    && { [ "$n" -lt 3 ] || grep -q "; al 2, al volver a ponerlo, también: sí$" <<< "$v"; } \
    && ! grep -q "MAL:" <<< "$v" && grep -q "^Hispa-64 etapa 10: todo en orden.$" <<< "$v"
}
e4_dice() {   # SALIDA
  serie "$1" | grep -E '^Programa|^HOLA|^MEMORIA|^A la vez|^Malos|^Después|^  la memoria|^  quitada|MAL|parado' | cut -c1-120 | paste -sd '|'
}
e4_ok() {   # SALIDA N QUÉ
  if e4_bien "$1" "$2"; then
    ok "etapa 4 $3: HOLA, HOLACAS, MEMORIA, 8 CUENTA a la vez ($(serie "$1" | grep '^A la vez:' | grep -o 'procesadores [0-9, ]*'| sed 's/ *$//')), MALO (24 punteros malos, 11 fallos a la vez); $(serie "$1" | grep '^Después:' | cut -d';' -f2 | sed 's/^ //'); la memoria, la de antes$( [ "$2" -gt 1 ] && echo '; la TLB de un espacio: 1 aviso, solo al que lo tiene puesto')"
  else ko "etapa 4 $3 no dice lo que debe: $(e4_dice "$1")"; fi
}
# [etapa 7] los programas de musl (estáticos, dinámicos con ld-musl, PIE) y
# Lua, con N procesadores (y «avx»: con -cpu max, los registros de AVX)
e7_bien() {   # SALIDA N [avx]
  local s=$1 n=$2 v d; v=$(serie "$s")
  d=$(sed -n '/^Después de musl: /{n;p}' <<< "$v")
  grep -q "^musl: los programas de C de hoy, con la biblioteca de C de musl (estáticos, dinámicos con ld-musl y PIE)$" <<< "$v" \
    && grep -q "^MHOLA: hola desde musl (printf); estático$" <<< "$v" \
    && grep -q "^MHOLA: hola desde musl (printf); dinámico: ld-musl en 0x7f0000000000, el programa en 0x555555554000$" <<< "$v" \
    && grep -q "^MHOLA: hola desde musl (printf); estático de posición libre, en 0x555555554000$" <<< "$v" \
    && grep -q "^  libsaludo.so: hola (42)$" <<< "$v" && grep -q "^  uname: Hispa-64 0.10.0 x86_64 (etapa 10 (C++ y la red de hoy))$" <<< "$v" \
    && grep -q "^  procesadores (sched_getaffinity): $n$" <<< "$v" \
    && [ "$(grep -c "^MHOLA: todo bien (" <<< "$v")" = 3 ] && [ "$(grep -c "^MHILOS: todo bien (" <<< "$v")" = 2 ] \
    && [ "$(grep -c "^  8 hilos que suman 2000 veces con un mutex: 16000 (exacto)" <<< "$v")" = 2 ] \
    && [ "$(grep -c "^  un productor y 8 consumidores (pthread_cond_wait, signal y broadcast): la suma, 2001000 (exacta)$" <<< "$v")" = 2 ] \
    && [ "$(grep -c "^MSENALES: todo bien (38 pruebas)$" <<< "$v")" = 2 ] \
    && [ "$(grep -c "^  un hijo que no llama al sistema, cerrado con SIGTERM y con SIGINT$" <<< "$v")" = 2 ] \
    && [ "$(grep -c "^MPROCESOS: todo bien (24 pruebas)$" <<< "$v")" = 2 ] \
    && [ "$(grep -c "^  posix_spawn (clone con CLONE_VM y CLONE_VFORK) de /MHOLA, con su salida por una tubería; y de uno que no está, ENOENT$" <<< "$v")" = 2 ] \
    && [ "$(grep -c "^  epoll de una tubería: el plazo, EPOLLIN con su dato, EPOLLONESHOT, EPOLLHUP, EPOLL_CTL_DEL$" <<< "$v")" = 2 ] \
    && [ "$(grep -c "^MMAPA: todo bien (16 pruebas)$" <<< "$v")" = 2 ] \
    && grep -q "^LUA: todo bien (16 pruebas); hoy es " <<< "$v" \
    && grep -q "^MDISP: todo bien (" <<< "$v" && grep -q "^Programa «MDISP»: acaba con el código 0, como debe" <<< "$v" \
    && grep -q "^MRED: todo bien (126 pruebas)$" <<< "$v" && grep -q "^Programa «MRED»: acaba con el código 0, como debe" <<< "$v" \
    && grep -q "^MCXX: todo bien (35 pruebas)$" <<< "$v" && grep -q "^Programa «MCXX»: acaba con el código 0, como debe" <<< "$v" \
    && grep -qE "^  TCP \[etapa 10\]: [1-9][0-9]* conexiones con escala de ventana" <<< "$v" \
    && grep -q "^MTEXTO: todo bien (6 pruebas)$" <<< "$v" && [ -n "$MTX" ] && grep -qxF "$MTX" <<< "$v" \
    && grep -q "^  hilos: 200 std::thread, thread_local (200 destruidos), condition_variable, async, promise, timed_mutex$" <<< "$v" \
    && [ "$(grep -cE "^Programa «(MHOLA|MHOLAD|MHOLAP) uno dos»: acaba con el código 0, como debe|^Programa «(MHILOS|MHILOSD|MSENALES|MSENALESD|MPROCESOS|MPROCESOSD|MMAPA|MMAPAD)»: acaba con el código 0, como debe|^Programa «LUA /PRUEBA.LUA uno dos»: acaba con el código 0, como debe" <<< "$v")" = 12 ] \
    && grep -q "^Después de musl: 0 procesos, 0 en la tabla, 0 espacios, 0 tablas de programas, 0 tuberías; " <<< "$v" \
    && grep -q "^  la memoria: la de antes; el montón: [0-9]* bloques (los de antes); las pilas: [0-9]* (las de antes)$" <<< "$d" \
    && { [ "${3:-}" != avx ] || { grep -q "^  AVX: sí (xsave, XCR0 = " <<< "$v" && [ "$(grep -c "^  AVX: 8 hilos con ymm0 a ymm7 llenos, cediendo el procesador 40 veces: intactos$" <<< "$v")" = 2 ]; }; } \
    && ! grep -q "MAL:" <<< "$v" && grep -q "^Hispa-64 etapa 10: todo en orden.$" <<< "$v"
}
e7_ok() {   # SALIDA N QUÉ [avx]
  local v; v=$(serie "$1")
  if e7_bien "$1" "$2" "${4:-}"; then
    ok "etapa 7 $3: musl (MHOLA estático, dinámico con ld-musl y libsaludo.so, y PIE; MHILOS, MSENALES, MPROCESOS y MMAPA, estáticos y dinámicos), Lua y [etapa 10] C++ (MCXX: 200 hilos, excepciones, flujos, regex) y la letra (MTEXTO: HarfBuzz y FreeType, lo mismo que en Linux), todo bien; $(grep '^Después de musl:' <<< "$v" | cut -d';' -f2 | sed 's/^ //')$( [ "${4:-}" = avx ] && echo '; los registros de AVX de 8 hilos, intactos')"
  else ko "etapa 7 $3 no dice lo que debe: $(grep -E '^Programa «[ML]|^M[A-Z]*: [0-9]* mal|^LUA:|^Después de musl|^  la memoria|MAL|parado' <<< "$v" | cut -c1-110 | paste -sd '|' | cut -c1-1500)"; fi
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
if [ $c = 85 ] && hay $S "^Hispa-64 0.10.0 ─ Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.$" \
   && hay $S "^Cargador: qemu, multiboot$" && hay $S "^Memoria: 1[12][0-9],[0-9] MB libres en [0-9]* trozos (de 127,4 MB que da el firmware); [0-9,]* [KM]B usados por el núcleo$" \
   && hay $S "^  sin la identidad de abajo: 1 MB ya no está en las tablas$" && hay $S "los nombres de [0-9]* funciones (HISPA64.SIM)$" \
   && hay $S "^El núcleo, en la mitad alta: 0xFFFFFFFF80100000 (código, de solo leer)" && hay $S "^TSS con pilas propias (IST)" \
   && hay $S "^Páginas: de 4 KB, puesta, leída, quitada y puesta en otro marco (la TLB, al día); de 2 MB, también; de 1 GB, no (el procesador no las tiene)$" \
   && hay $S "^  el código del núcleo, sin escribir: sí; los datos, sin ejecutar: sí; la guarda de la pila, sin poner: sí$" \
   && hay $S "^Montón: 2000 reservas y liberaciones al azar .*, comprobadas; al acabar, 0 en uso y la memoria física, la de antes" \
   && hay $S "Punto de parada (int3) número 1 .*: atendido, sigue." && hay $S "^Hispa-64 etapa 10: todo en orden.$"; then
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
e7_ok $S 1 "por multiboot con 1 procesador («pc», 128 MB)"
# con «q35», 4 procesadores y un puerto PCIe con un xHCI detrás (el bus 1)
PCIQ="-machine q35 -smp 4 -device pcie-root-port,id=rp1,bus=pcie.0,chassis=1 -device qemu-xhci,bus=rp1"
# shellcheck disable=SC2086
mb1 apagar "$T/q35.txt" 256 $PCIQ; c=$?
S=$T/q35.txt
# shellcheck disable=SC2086
if [ $c = 85 ] && hay $S "^  procesadores (MADT): 4 encendidos (APIC 0, 1, 2, 3)$" && hay $S "^  MCFG: PCIe por ECAM en 0xB0000000, segmento 0, buses 0 a 255$" \
   && hay $S "^  FADT: .*8042: sí" && tics_bien $S "el HPET" && hay $S "^IOAPIC: el temporizador 0 del HPET, por la GSI 1[6-9], al vector 0x3E: 20 de 20 interrupciones (cada 10 ms)$" \
   && hay $S "^PCI (PCIe, por ECAM): [0-9]* funciones en los buses 0 a 255, 1 puentes$" && hay $S "^  01:00.0 1B36:000D bus serie: USB 3 (xHCI)" \
   && hay $S "^      BAR0 0x[0-9A-F]*, 16 KB de memoria de 64 bits$" && pci_igual $S -m 256 $PCIQ && hay $S "^Hispa-64 etapa 10: todo en orden.$"; then
  ok "etapa 2, por multiboot, «q35» con 4 procesadores: la MADT con 4, PCIe por ECAM, $(pci_nucleo $S | wc -l) funciones (el puente y el xHCI del bus 1, con su BAR de 64 bits), las mismas que dice QEMU; «$(serie $S | grep '^Tic:')»"
else ko "etapa 2 con «q35» y 4 procesadores no dice lo que debe (código $c): $(serie $S | grep -E 'MADT|^Tic|^IOAPIC:|^PCI|parado' | paste -sd ' ')"; diff <(pci_nucleo $S) <(pci_qemu -m 256 $PCIQ) | head -5 | sed 's/^/        /'; fi
if e3_bien $S 4 && hay $S "^  con INIT-SIPI-SIPI y el trampolín en 0x[0-9A-F]* (16, 32 y 64 bits)"; then
  ok "etapa 3 con 4 procesadores (q35): «$(serie $S | grep '^Procesadores:')»; los 100 hilos, el número exacto; «$(serie $S | grep '^Reparto:' | cut -d: -f2- | cut -c1-80)»; la TLB de los demás, vaciada con un IPI"
else ko "etapa 3 con 4 procesadores (q35) no dice lo que debe: $(e3_dice $S)"; fi
e4_ok $S 4 "por multiboot con 4 procesadores (q35)"
e7_ok $S 4 "por multiboot con 4 procesadores (q35)"
# sin HPET: el reloj de ACPI y, para el IOAPIC, el 8254
for m in pc q35; do
  mb1 "apagar sinmusl" "$T/sinhpet-$m.txt" 256 -machine $m,hpet=off -smp 4; c=$?
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
e7_ok $S 8 "con 8 procesadores y -cpu max (AVX con xsave)" avx
mb1 "apagar sinmusl" "$T/smp2.txt" 256 -machine pc -smp 2; c=$?
S=$T/smp2.txt
if [ $c = 85 ] && e3_bien $S 2 && hay $S "^  quitada sin avisar y puesta en otro marco (con 0x2222): el 1 "; then
  ok "etapa 3 con 2 procesadores («pc»): $(serie $S | grep '^  quitada sin avisar' | sed 's/.*: el 1/sin avisar, el 1/'); con aviso, le falla"
else ko "etapa 3 con 2 procesadores («pc») no dice lo que debe (código $c): $(e3_dice $S)"; fi
e4_ok $S 2 "por multiboot con 2 procesadores («pc»)"
mb1 "apagar ocioso=mwait sinmusl" "$T/mwait.txt" 256 -machine q35 -smp 4 -cpu max; c=$?
S=$T/mwait.txt
if [ $c = 85 ] && e3_bien $S 4 && hay $S "la tarea ociosa, con mwait, FORZADO (ocioso=mwait)$"; then
  ok "ocioso=mwait con -cpu max y 4 procesadores: la tarea ociosa con monitor/mwait, y todo igual"
else ko "ocioso=mwait no sale (código $c): $(serie $S | grep -E '^Planificador' | cut -c1-120) $(e3_dice $S)"; fi
mb1 "apagar sinpruebas sinsmp" "$T/sinsmp.txt" 256 -machine q35 -smp 4; c=$?
if [ $c = 85 ] && hay "$T/sinsmp.txt" "^Procesadores: 1 en marcha (APIC 0) (sinsmp: los demás, apagados)$" && hay "$T/sinsmp.txt" "^Hispa-64 etapa 10: todo en orden.$"; then
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
mb1 "apagar sinmusl" "$T/mb3g.txt" 3G -cpu max; c=$?
S=$T/mb3g.txt
if [ $c = 85 ] && hay $S "^Memoria: 2,9 GB libres" && hay $S "en páginas de 1 GB: [1-9]" && hay $S "; de 1 GB, también$" \
   && hay $S "^Hispa-64 etapa 10: todo en orden.$"; then
  ok "por multiboot con 3 GB y -cpu max: «$(serie $S | grep '^Memoria:' | cut -d';' -f1)»; el mapa directo con páginas de 1 GB; una de 1 GB puesta, leída y quitada"
else ko "por multiboot con 3 GB no dice lo que debe (código $c): $(serie $S | grep -E 'Memoria:|Páginas|parado' | paste -sd ' ')"; fi
mb1 "apagar sinmusl" "$T/mb8g.txt" 8G; c=$?
S=$T/mb8g.txt
if [ $c = 85 ] && hay $S "^Memoria: 7,9 GB libres en [0-9]* trozos (de 7,9 GB" && hay $S "en páginas de 1 GB: 0; de 2 MB: 4[0-9][0-9][0-9];" \
   && hay $S "^Hispa-64 etapa 10: todo en orden.$"; then
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
  ok "los discos de prueba (hazdiscos.sh: GPT y MBR con lógicas, FAT32, FAT16 y FAT12 con mkfs.fat, exFAT con hazexfat.py; 3 TB dispersos): $(tail -1 "$T/discos.txt" | cut -d: -f2)"
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
# lo mismo en un exFAT (etapa 6), con miraexfat.py: el volumen bien (sin
# VolumeDirty: lo ha limpiado el apagado) y lo que ha dejado ARCHIVOS
MIRAEX="python3 -I $AQUI/programas/miraexfat.py"
exfat_bien() {   # IMAGEN PRIMER_SECTOR SECTORES (lo que dice, a miraexfat.txt)
  saca "$1" "$2" "$3" "$T/exfat.part"
  $MIRAEX "$T/exfat.part" >> "$T/miraexfat.txt" 2>&1
}
exfat_dice() { grep -m 3 MAL "$T/miraexfat.txt" | paste -sd ' '; }
lo_dejado_ex() {   # IMAGEN PRIMER_SECTOR SECTORES
  local p="$T/dejado.part" d="Prueba de Hispa-64"
  saca "$1" "$2" "$3" "$p"
  [ "$($MIRAEX "$p" tipo ULTIMO.TXT 2> /dev/null)" = "Escrito sin fsync: lo vacía el apagado." ] \
    && [ "$($MIRAEX "$p" tipo "$d/adios.txt" 2> /dev/null)" = "$(printf 'Hola, MUNDO\nsegunda línea')" ] \
    && [ "$($MIRAEX "$p" tipo "$d/UN NOMBRE MUY LARGO CON ACENTOS ÁÉÍÓÚ Y EÑE Ñ.TXT" 2> /dev/null)" = "¡Hola!" ] \
    && [ "$($MIRAEX "$p" tipo "$d/sub2/Dentro/movido.txt" 2> /dev/null)" = "me han movido" ] \
    && [ "$($MIRAEX "$p" dir "$d/Muchos" 2> /dev/null | grep -c '^archivo número [0-9][13579].txt 22$')" = 20 ] \
    && [ "$($MIRAEX "$p" dir "$d/Muchos" 2> /dev/null | wc -l)" = 20 ] \
    && $MIRAEX "$p" tipo "$d/grande.bin" > "$T/grande.bin" 2> /dev/null \
    && python3 -I -c '
import sys
d = open(sys.argv[1], "rb").read()
e = bytearray(((i * 131 + (i >> 8)) & 255) for i in range(1234567))
e[500000:500100] = b"X" * 100
sys.exit(0 if d == bytes(e) else 1)' "$T/grande.bin" \
    && [ "$($MIRAEX "$p" dir "$d" 2> /dev/null | grep -c '^hueco.bin 3$')" = 1 ] \
    && ! $MIRAEX "$p" dir "$d" 2> /dev/null | grep -qE '^(hola|uno|dos|abierto|desde|movido)' \
    && [ "$($MIRAEX "$p" dir / 2> /dev/null | grep -c '^RAIZ')" = 0 ]
}
# la SATA de prueba: FAT32 (p1), FAT12 (p2), exFAT (p5 y p8), FAT16 (p6 y p7): las escritas, bien
sata_bien() {   # IMAGEN
  rm -f "$T/miraexfat.txt"
  fsck_bien "$1" 2048 172032 && lo_dejado "$1" 2048 172032 \
    && fsck_bien "$1" 174080 8192 && lo_dejado "$1" 174080 8192 \
    && fsck_bien "$1" 251904 65536 && lo_dejado "$1" 251904 65536 \
    && fsck_bien "$1" 319488 16384 && lo_dejado "$1" 319488 16384 \
    && exfat_bien "$1" 184320 65536 && lo_dejado_ex "$1" 184320 65536 \
    && exfat_bien "$1" 337920 32768 && lo_dejado_ex "$1" 337920 32768
}
e5_bien() {   # SALIDA: lo que dice siempre una vuelta de la etapa 5 sin fallos
  local v; v=$(serie "$1")
  grep -q "^Después de los archivos: 0 abiertos, [0-9]* nodos (los de antes: las raíces), el montón como antes$" <<< "$v" \
    && ! grep -q "MAL:" <<< "$v" && grep -q "^Hispa-64 etapa 10: todo en orden.$" <<< "$v"
}
e5_dice() { serie "$1" | grep -E '^NVMe|^SATA|^  /|^ARCHIVOS|MAL|^Después de los|parado|^Al apagar' | cut -c1-140 | paste -sd '|'; }
NVME5="-drive file=$T/w5/nvme.img,if=none,id=n0,format=raw -device nvme,serial=hispa64,drive=n0"
CRUDO5="-drive file=$T/w5/crudo.img,if=none,id=n1,format=raw -device nvme,serial=crudo,drive=n1"
SATA5="-drive file=$T/w5/sata.img,if=none,id=s0,format=raw -device ide-hd,drive=s0,bus=ide.0"
# 1. q35 con 4 procesadores: NVMe (GPT), un NVMe crudo y SATA (MBR con una exFAT), escribiendo
copia_discos
# shellcheck disable=SC2086
mb1 "apagar escribir sinmusl" "$T/e5.txt" 256 -machine q35 -smp 4 $NVME5 $CRUDO5 $SATA5; c=$?
S=$T/e5.txt
if [ $c = 85 ] && hay $S "^NVMe: 00:0[0-9A-F].0 1B36:0010, «QEMU NVMe Ctrl» (serie hispa64), versión 1.4$" \
   && hay $S "^  BAR 0 en 0x[0-9A-F]*; 4 colas de E/S (una por procesador) de 64 órdenes; MSI-X; hasta 128 KB por orden; con caché de escritura$" \
   && hay $S "^SATA (AHCI): 00:1F.2 8086:2922, AHCI 1.0, 6 puertos, direcciones de 64 bits; MSI; 1 disco$" \
   && hay $S "^  nvme0: «QEMU NVMe Ctrl», 112,0 MB (229376 sectores de 512 octetos), tabla GPT; se puede escribir$" \
   && hay $S "^    nvme0p1: 40,0 MB, desde el sector 2048, EFI$" && hay $S "^    nvme0p2: 70,9 MB, desde el sector 83968, datos (Windows: FAT, exFAT o NTFS)$" \
   && hay $S "^  sata0: «QEMU HARDDISK», 184,0 MB (376832 sectores de 512 octetos), tabla MBR; se puede escribir$" \
   && hay $S "^    sata0p1: 84,0 MB, desde el sector 2048, FAT32$" && hay $S "^    sata0p2: 4,0 MB, desde el sector 174080, FAT12$" \
   && hay $S "^    sata0p5: 32,0 MB, desde el sector 184320, exFAT o NTFS$" && hay $S "^    sata0p6: 32,0 MB, desde el sector 251904, FAT16$" \
   && hay $S "^    sata0p7: 8,0 MB, desde el sector 319488, FAT16$" && hay $S "^    sata0p8: 16,0 MB, desde el sector 337920, exFAT o NTFS$" \
   && hay $S "^  nvme1: «QEMU NVMe Ctrl», 16,0 MB (32768 sectores de 512 octetos), sin tabla de particiones; se puede escribir$" \
   && hay $S "^Archivos (el VFS): 9 montajes$" && hay $S "^  /nvme0p1: FAT32 «ESP», .*; de leer y escribir$" \
   && hay $S "^  /nvme0p2: FAT32 «DATOS», 69,8 MB en 143076 cúmulos de 512 octetos, " && hay $S "^  /sata0p1: FAT32 «SATAFAT», 83,3 MB en 85332 cúmulos de 1 KB" \
   && hay $S "^  /sata0p5: exFAT «EXFATPRB», 31,9 MB en 8168 cúmulos de 4 KB, 31,1 MB libres; de leer y escribir$" \
   && hay $S "^  /sata0p2: FAT12 «FAT12PRB», 3,9 MB en 4081 cúmulos de 1 KB, .* libres; de leer y escribir$" \
   && hay $S "^  /sata0p6: FAT16 «FAT16PRB», 31,8 MB en 32623 cúmulos de 1 KB, .* libres; de leer y escribir$" \
   && hay $S "^  /sata0p7: FAT16 «FAT16CHICA», 7,9 MB en 16223 cúmulos de 512 octetos, .* libres; de leer y escribir$" \
   && hay $S "^  /sata0p8: exFAT «EXFAT512», 15,8 MB en 32384 cúmulos de 512 octetos, 15,5 MB libres; de leer y escribir$" \
   && hay $S "^La caché: los primeros 4 KB de los 3 discos, iguales por la caché y a pelo$" \
   && hay $S "^  escribir y leer a pelo: 1 sector, 8, 256 (128 KB), 500 (varias órdenes) y los 300 del final: iguales; más allá del final, no$" \
   && hay $S "^  4 hilos a la vez, cada uno en su procesador, 32 lecturas de 128 KB cada uno: bien; colas de NVMe usadas: 4$" \
   && hay $S "^ARCHIVOS: 7 discos de prueba (7 escritos); todo bien ([0-9]* pruebas)$" && hay $S "^Programa «ARCHIVOS auto»: acaba con el código 0, como debe" \
   && hay $S "^  la raíz fija: [1-9][0-9]* archivos hasta ENOSPC, y borrados$" \
   && hay $S "^      Canción de cuna ñandú.txt (37 octetos)$" && hay $S "^Al apagar: [1-9][0-9]* bloques sin escribir en la caché; vaciados (sync): [1-9][0-9]* escritos$" \
   && e5_bien $S; then
  ok "etapa 5 (q35, 4 procesadores, MSI-X y MSI): NVMe con 4 colas, SATA, GPT y MBR con lógicas, 8 volúmenes (FAT32, FAT16, FAT12 y exFAT, todos escritos); «$(serie $S | grep '^  leerlo entero' | cut -d: -f2- | sed 's/^ //')»; «$(serie $S | grep '^ARCHIVOS: 3')»; al apagar, lo sin vaciar, al disco"
else ko "etapa 5 en q35 no dice lo que debe (código $c): $(e5_dice $S)"; fi
e4_ok $S 4 "con los discos (q35, 4 procesadores)"
if fsck_bien "$T/w5/nvme.img" 83968 145375 && lo_dejado "$T/w5/nvme.img" 83968 145375 \
   && fsck_bien "$T/w5/nvme.img" 2048 81920 && sata_bien "$T/w5/sata.img"; then
  ok "desde el anfitrión: fsck.fat sin quejas en los dos FAT32, la FAT12 y las dos FAT16 escritas (y la EFI) y, con mtools, lo que ha dejado ARCHIVOS (los nombres largos con acentos, grande.bin byte a byte, los 20 de Muchos, ULTIMO.TXT, que vació el apagado; nada de la raíz llena de la FAT12)"
  ok "desde el anfitrión, las dos exFAT escritas, con miraexfat.py: $(cut -d: -f2- "$T/miraexfat.txt" | sed 's/^ //' | paste -sd ';' | sed 's/;/; y /'); y lo que ha dejado ARCHIVOS, leído por él"
else ko "desde el anfitrión, los volúmenes escritos no están bien: $(fsck_dice) $(exfat_dice)"; fi
# 2. 3 TB: las particiones de más allá de 2^32 sectores, por NVMe y por SATA (LBA de 48 bits)
# shellcheck disable=SC2086
mb1 "apagar escribir sinmusl" "$T/e5g.txt" 512 -machine q35 -smp 2 -drive file="$T/w5/grande.img",if=none,id=n0,format=raw -device nvme,serial=grande,drive=n0 \
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
mb1 "apagar escribir sinmsix sinmsi sinmusl" "$T/e5s.txt" 256 -machine pc -smp 2 $NVME5 $CRUDO5 -device ahci,id=ah \
  -drive file="$T/w5/sata.img",if=none,id=s0,format=raw -device ide-hd,drive=s0,bus=ah.2; c=$?
S=$T/e5s.txt
if [ $c = 85 ] && [ "$(serie $S | grep -c '; sin interrupciones (se mira la cola);')" = 3 ] \
   && hay $S "^ARCHIVOS: 7 discos de prueba (7 escritos); todo bien" && e5_bien $S \
   && sata_bien "$T/w5/sata.img"; then
  ok "etapa 5 sin MSI ni MSI-X («pc», 2 procesadores, la AHCI aparte): las tres controladoras mirando la cola, y todo igual (fsck.fat y miraexfat.py, bien)"
else ko "etapa 5 sin interrupciones no sale (código $c): $(e5_dice $S) $(fsck_dice) $(exfat_dice)"; fi
# 3 b. [etapa 11] sin MSI ni MSI-X en q35: la INTx de cada una por la GSI del _PRT
# (los GSIA-GSIH), con sus interrupciones (por nivel: se apagan), y el xHCI igual
copia_discos
# shellcheck disable=SC2086
mb1 "apagar escribir sinmsix sinmsi sinmusl" "$T/e5x.txt" 256 -machine q35 -smp 2 $NVME5 $CRUDO5 $SATA5 -device qemu-xhci,id=xhci -device usb-kbd,bus=xhci.0; c=$?
S=$T/e5x.txt
if [ $c = 85 ] && [ "$(serie $S | grep -cE '^  BAR 0 en .*; INTx por la GSI [0-9]* \(del _PRT\);|^SATA \(AHCI\): .*; INTx por la GSI [0-9]* \(del _PRT\); 1 disco$|^  versión .*; INTx por la GSI [0-9]* \(del _PRT\)$')" = 4 ] \
   && hay $S "^ARCHIVOS: 7 discos de prueba (7 escritos); todo bien" && e5_bien $S \
   && entre $S "^Interrupciones de los discos: NVMe " 1 999999 && entre $S ", AHCI " 1 9999999 && hay $S "; GSI tapadas por no apagarse: 0$" \
   && hay $S "«QEMU USB Keyboard»" && sata_bien "$T/w5/sata.img"; then
  ok "etapa 11, sin MSI ni MSI-X en q35: NVMe (2), AHCI y xHCI por la INTx de su GSI del _PRT; «$(serie $S | grep '^Interrupciones de los discos' | cut -d: -f2- | sed 's/^ //')»; ARCHIVOS, fsck.fat y miraexfat.py, bien"
else ko "etapa 11, la INTx de los discos y el xHCI no sale (código $c): $(serie $S | grep -E 'INTx por|^Interrupciones|^ARCHIVOS|MAL|parado' | cut -c1-120 | paste -sd '|') $(fsck_dice) $(exfat_dice)"; fi
# 4. sin «escribir»: nada cambia en los discos (ni un octeto)
copia_discos
antes=$(cat "$T"/w5/nvme.img "$T"/w5/sata.img "$T"/w5/crudo.img | md5sum)
# shellcheck disable=SC2086
mb1 "apagar sinmusl" "$T/e5r.txt" 256 -machine q35 -smp 2 $NVME5 $CRUDO5 $SATA5; c=$?
S=$T/e5r.txt
if [ $c = 85 ] && [ "$(cat "$T"/w5/nvme.img "$T"/w5/sata.img "$T"/w5/crudo.img | md5sum)" = "$antes" ] \
   && [ "$(serie $S | grep -c '; solo se lee$')" = 3 ] && hay $S "^  no se escribe en él (sin «escribir» en la línea de órdenes)$" \
   && [ "$(serie $S | grep -c '^  de solo leer (EROFS): no se escribe$')" = 7 ] \
   && hay $S "^ARCHIVOS: 7 discos de prueba (0 escritos); todo bien" && ! hay $S "^Al apagar" && e5_bien $S; then
  ok "etapa 5 sin «escribir»: todo de solo leer (EROFS), y los discos, iguales octeto a octeto (md5sum)"
else ko "etapa 5 sin «escribir» no sale (código $c): $(e5_dice $S)"; fi
# 5. el núcleo de antes de la etapa 5 no escribía; el de ahora, con los discos y sin pruebas
# shellcheck disable=SC2086
mb1 "apagar sinpruebas escribir" "$T/e5n.txt" 256 -machine q35 -smp 4 $NVME5 $CRUDO5 $SATA5; c=$?
if [ $c = 85 ] && hay "$T/e5n.txt" "^Hispa-64 etapa 10: todo en orden.$" && hay "$T/e5n.txt" "^  4 hilos a la vez"; then
  ok "sinpruebas con los discos: el disco crudo sí (es del núcleo), los programas no"
else ko "sinpruebas con los discos no sale (código $c): $(e5_dice "$T/e5n.txt")"; fi

# ---- [etapa 6] el USB: xHCI, concentradores, HID y memorias ----
"$AQUI/programas/hazusb.sh" "$T/usbd" > "$T/usbd.txt" 2>&1 || ko "hazusb.sh no hace las memorias: $(tail -2 "$T/usbd.txt" | paste -sd ' ')"
copia_usb() { rm -rf "$T/w6"; mkdir -p "$T/w6"; cp "$T/usbd/usbfat.img" "$T/usbd/usbexfat.img" "$T/w6/"; }
# los aparatos de las pruebas: un concentrador de USB 1.1 (velocidad plena) con
# el teclado y la tableta detrás, el ratón (de alta velocidad), y las dos
# memorias (de USB 3: SuperSpeed)
USBD="-device qemu-xhci,id=xhci -device usb-hub,bus=xhci.0,port=1 -device usb-kbd,bus=xhci.0,port=1.1 -device usb-tablet,bus=xhci.0,port=1.2 \
 -device usb-mouse,bus=xhci.0,port=2"
memorias() { echo "-drive if=none,id=u1,format=raw,file=$T/w6/usbfat.img -device usb-storage,bus=xhci.0,drive=u1 -drive if=none,id=u2,format=raw,file=$T/w6/usbexfat.img -device usb-storage,bus=xhci.0,drive=u2"; }
e6_dice() { serie "$1" | grep -E '^xHCI|^  versión|^USB|^  [0-9]-|^  usb|^  /usb|^ARCHIVOS|MAL|parado' | cut -c1-150 | paste -sd '|'; }
copia_usb
# shellcheck disable=SC2046,SC2086
mb1 "apagar escribir sinmusl" "$T/e6.txt" 512 -machine q35 -smp 4 $USBD $(memorias); c=$?
S=$T/e6.txt
# shellcheck disable=SC2086
if [ $c = 85 ] && hay $S "^xHCI: 00:0[0-9A-F].0 1B36:000D en 0x[0-9A-F]*$" \
   && hay $S "^  versión 1.00, 8 puertos (4 de USB 2 y 4 de USB 3), 63 ranuras, contextos de 32 octetos, direcciones de 64 bits; MSI-X$" \
   && hay $S "^USB: 6 aparatos (1 concentradores) en [0-9]* ms$" \
   && hay $S "^  1-[0-9]: 0409:55AA «QEMU USB Hub», USB 1.1, velocidad plena (12 Mbit/s), ranura [0-9]*, paquete del extremo 0: 8; concentrador de 8 puertos$" \
   && hay $S "^  1-[0-9].1: 0627:0001 «QEMU USB Keyboard», USB 2.0, velocidad plena (12 Mbit/s), ranura [0-9]*, paquete del extremo 0: 8; HID: teclado (por su descriptor, con LED)$" \
   && hay $S "^  1-[0-9].2: 0627:0001 «QEMU USB Tablet», .*; HID: tableta (por su descriptor, de 0 a 32767)$" \
   && hay $S "«QEMU USB Mouse», USB 2.0, velocidad alta (480 Mbit/s), ranura [0-9]*, paquete del extremo 0: 64; HID: ratón (por su descriptor, con rueda)$" \
   && [ "$(serie $S | grep -c '«QEMU USB HARDDRIVE», USB 3.0, velocidad SuperSpeed (5 Gbit/s), ranura [0-9]*, paquete del extremo 0: 512; almacenamiento (SCSI, Bulk-Only)$')" = 2 ] \
   && hay $S "^  usb0: «QEMU    QEMU HARDDISK», 64,0 MB (131072 sectores de 512 octetos), tabla MBR; se puede escribir$" \
   && hay $S "^    usb0p1: 63,0 MB, desde el sector 2048, FAT32$" \
   && hay $S "^  usb1: «QEMU    QEMU HARDDISK», 48,0 MB (98304 sectores de 512 octetos), sin tabla: un volumen entero; se puede escribir$" \
   && hay $S "^  /usb0p1: FAT32 «USBFAT», .*; de leer y escribir$" && hay $S "^  /usb1: exFAT «USBEXFAT», .*; de leer y escribir$" \
   && hay $S "^ARCHIVOS: 2 discos de prueba (2 escritos); todo bien ([0-9]* pruebas)$" && e4_bien $S 4; then
  ok "etapa 6, el USB (q35, 4 procesadores): xHCI con MSI-X, «$(serie $S | grep '^USB:' | cut -d' ' -f2-5)»: el concentrador de USB 1.1 con el teclado y la tableta de velocidad plena (extremo 0 de 8: Evaluate Context), el ratón de alta con rueda, dos memorias SuperSpeed; ARCHIVOS en las dos («$(serie $S | grep '^ARCHIVOS: 2 discos' | cut -d';' -f2 | sed 's/^ //')»); la etapa 4, igual"
else ko "etapa 6, el USB, no dice lo que debe (código $c): $(e6_dice $S)"; fi
saca "$T/w6/usbfat.img" 2048 129024 "$T/usbfat.part"
if fsck_bien "$T/w6/usbfat.img" 2048 129024 && lo_dejado "$T/w6/usbfat.img" 2048 129024; then
  ok "etapa 6, desde el anfitrión: la FAT32 de la memoria USB, sin quejas de fsck.fat, y lo que ha dejado ARCHIVOS, comprobado con mtools"
else ko "etapa 6: la FAT32 de la memoria USB no está bien: $(fsck_dice)"; fi
rm -f "$T/miraexfat.txt"
if exfat_bien "$T/w6/usbexfat.img" 0 98304 && ! grep -q MAL "$T/miraexfat.txt" && lo_dejado_ex "$T/w6/usbexfat.img" 0 98304; then
  ok "etapa 6, desde el anfitrión: el exFAT de la otra memoria, bien (miraexfat.py: $(tail -1 "$T/miraexfat.txt" | cut -c1-80)), y lo que ha dejado ARCHIVOS"
else ko "etapa 6: el exFAT de la memoria USB no está bien: $(exfat_dice)"; fi
# sin MSI-X: el nec-usb-xhci con MSI y el qemu-xhci mirando la cola, a la vez (dos controladoras)
copia_usb
# shellcheck disable=SC2086
mb1 "apagar escribir sinmsix sinmusl" "$T/e6b.txt" 256 -machine q35 -smp 2 -device nec-usb-xhci,id=xhci,msix=off -device usb-kbd,bus=xhci.0 \
  -drive if=none,id=u1,format=raw,file=$T/w6/usbfat.img -device usb-storage,bus=xhci.0,drive=u1 \
  -device qemu-xhci,id=x2 -drive if=none,id=u2,format=raw,file=$T/w6/usbexfat.img -device usb-storage,bus=x2.0,drive=u2; c=$?
S=$T/e6b.txt
if [ $c = 85 ] && hay $S "^xHCI: 00:0[0-9A-F].0 1033:0194 en " && hay $S "^  versión 1.00, 8 puertos .*; MSI$" \
   && hay $S "^  versión 1.00, 8 puertos .*; sin interrupciones (se mira la cola)$" && hay $S "^USB: 3 aparatos (0 concentradores)" \
   && hay $S "^  2-[0-9]: 46F4:0001 «QEMU USB HARDDRIVE»" && hay $S "^ARCHIVOS: 2 discos de prueba (2 escritos); todo bien ([0-9]* pruebas)$" && e4_bien $S 2; then
  ok "etapa 6, dos controladoras: el nec-usb-xhci con MSI y el qemu-xhci sin MSI-X (se mira la cola); las dos memorias, escritas por ARCHIVOS"
else ko "etapa 6 con dos controladoras no dice lo que debe (código $c): $(e6_dice $S)"; fi
# el puntero, el teclado y enchufar en marcha: con el monitor de QEMU (y la
# tableta, por QMP: «mouse_move» del monitor solo mueve en relativo)
abs_qmp() {   # SOCKET X Y (de 0 a 32767)
  python3 -I - "$@" <<'PY' 2> /dev/null
import json, socket, sys, time
s = socket.socket(socket.AF_UNIX); s.connect(sys.argv[1]); f = s.makefile('rw')
f.readline(); f.write('{"execute": "qmp_capabilities"}\n'); f.flush(); f.readline()
ev = [{"type": "abs", "data": {"axis": "x", "value": int(sys.argv[2])}}, {"type": "abs", "data": {"axis": "y", "value": int(sys.argv[3])}}]
f.write(json.dumps({"execute": "input-send-event", "arguments": {"events": ev}}) + '\n'); f.flush(); f.readline()
PY
}
con_usb() {   # SALIDA ÓRDENES -- opciones de QEMU (las órdenes, separadas por «;», al ver «Puntero: 10 s»)
  local s=$1 o=$2 q c; shift 3
  rm -f "$s" "$T/mon6.sock"
  rm -f "$T/qmp6.sock"
  timeout 90 qemu-system-x86_64 -kernel "$T/HISPA64.BIN" -initrd "$T/HISPA64.SIM" -append "apagar sinpruebas prueba=raton" -display none -serial file:"$s" \
    -monitor unix:"$T/mon6.sock",server,nowait -qmp unix:"$T/qmp6.sock",server,nowait $DBG -no-reboot -nic none -m 256 -machine q35 -device qemu-xhci,id=xhci "$@" > /dev/null 2>&1 &
  q=$!
  for _ in $(seq 1 200); do grep -q "^Puntero: 10 s" "$s" 2> /dev/null && break; sleep 0.2; done
  sleep 0.5
  IFS=';' read -ra OS <<< "$o"
  for x in "${OS[@]}"; do
    if [ "${x%% *}" = abs ]; then abs_qmp "$T/qmp6.sock" ${x#abs }; else python3 -I "$AQUI/pantallazo.py" orden "$T/mon6.sock" "$x" 2> /dev/null; fi
    sleep 0.6
  done
  wait $q; c=$?
  return $c
}
con_usb "$T/e6r.txt" "mouse_move 10 20;mouse_move -5 3 1;mouse_button 1;mouse_button 0;sendkey a;sendkey shift-b" -- -device usb-mouse,bus=xhci.0 -device usb-kbd,bus=xhci.0; c=$?
# (la Mayúscula y la b pueden llegar en el mismo informe: la línea de la Mayúscula, no siempre; las cuenta el resumen)
P=$(serie "$T/e6r.txt" | grep -E '^Puntero: \(|^Tecla USB' | grep -v '^Tecla USB: uso 0xE1$' | paste -sd '|')
if [ $c = 85 ] && [ "$P" = "Puntero: (522, 404), botones 0, rueda 0 (ratón)|Puntero: (517, 407), botones 0, rueda 1 (ratón)|Puntero: (517, 407), botones 1, rueda 1 (ratón)|Puntero: (517, 407), botones 0, rueda 1 (ratón)|Tecla USB: uso 0x4|Tecla USB: uso 0x5" ] \
   && hay "$T/e6r.txt" ", 1 clics; teclas USB: 3, LED puestos: "; then
  ok "etapa 6, el ratón USB (mouse_move y mouse_button del monitor): lo movido, la rueda y el clic, exactos; y las teclas del teclado USB (a, Mayúsculas, b)"
else ko "etapa 6, el ratón y el teclado USB no dan lo que deben (código $c): $P"; fi
con_usb "$T/e6t.txt" "abs 16384 8192;mouse_button 2;mouse_button 0;abs 32767 32767" -- -device usb-hub,bus=xhci.0,port=1 -device usb-tablet,bus=xhci.0,port=1.1; c=$?
P=$(serie "$T/e6t.txt" | grep -E '^Puntero: \(' | paste -sd '|')
if [ $c = 85 ] && grep -q "^Puntero: (51[0-2], 19[0-2]), botones 0, rueda 0 (tableta)|Puntero: (51[0-2], 19[0-2]), botones 2, rueda 0 (tableta)|Puntero: (51[0-2], 19[0-2]), botones 0, rueda 0 (tableta)|Puntero: (1023, 767), botones 0, rueda 0 (tableta)$" <<< "$P"; then
  ok "etapa 6, la tableta USB detrás del concentrador (posición absoluta de 0 a 32767): al medio, el botón derecho y a la esquina: «$(cut -d'|' -f1 <<< "$P")»"
else ko "etapa 6, la tableta USB no da lo que debe (código $c): $P"; fi
con_usb "$T/e6c.txt" "device_add usb-kbd,bus=xhci.0,id=k9;sendkey c;device_del k9;device_add usb-mouse,bus=xhci.0,port=1.3,id=m9;mouse_move 3 4;device_del h1" -- -device usb-hub,bus=xhci.0,port=1,id=h1; c=$?
S=$T/e6c.txt
if [ $c = 85 ] && hay $S "^USB: se ha enchufado 1-[0-9]: 0627:0001 «QEMU USB Keyboard», .*HID: teclado" && hay $S "^Tecla USB: uso 0x6$" \
   && hay $S "^USB: se ha enchufado 1-[0-9].3: 0627:0001 «QEMU USB Mouse», USB 2.0, velocidad plena .*HID: ratón (por su descriptor, con rueda)$" \
   && hay $S "^Puntero: (515, 388), botones 0, rueda 0 (ratón)$" && [ "$(serie $S | grep -c '^USB: se ha desenchufado ')" = 3 ] && hay $S "^Hispa-64 etapa 10: todo en orden.$"; then
  ok "etapa 6, enchufar y desenchufar en marcha: un teclado (su tecla llega), un ratón detrás del concentrador, y quitar el concentrador con lo que cuelga (3 desenchufados)"
else ko "etapa 6, en marcha, no sale (código $c): $(serie $S | grep -E '^USB:|^Tecla|^Puntero' | paste -sd '|')"; fi
# sinusb: no se toca
mb1 "apagar sinpruebas sinusb" "$T/e6n.txt" 256 -machine q35 -device qemu-xhci,id=xhci -device usb-kbd,bus=xhci.0; c=$?
if [ $c = 85 ] && ! hay "$T/e6n.txt" "^xHCI:" && hay "$T/e6n.txt" "^Hispa-64 etapa 10: todo en orden.$"; then
  ok "etapa 6, sinusb: el xHCI, sin tocar"
else ko "etapa 6, sinusb, no sale (código $c)"; fi

# ---- [etapa 6] HID: los descriptores (Hidesc64) e I2C-HID (Aml64, I2c64, I2chid64) ----
# QEMU no tiene ni controladora I2C DesignWare ni HID sobre I2C, así que: los
# descriptores de informe (touchpads de verdad, los aparatos USB de QEMU) se
# analizan en el arranque (prhid64.cas, con «prueba=hid» uno a uno); los
# PNP0C50 se buscan en tablas de prueba dadas con -acpitable (la SSDT de
# prueba de Hispa-Pentium, una como la del ThinkPad E14 con «NULL», y las
# tablas de verdad del ASUS GL553VD: su DSDT, partida en SSDT de 60 KB por
# comossdt.py); sin controladora, el arranque lo dice y no falla; «sini2c»; y
# dar dirección a una BAR sin poner (la del aparato edu de QEMU, «prueba=barcero»).
I2=$AQUI/i2chid
hid_orden() {   # SALIDA QUÉ LÍNEAS...: ¿salen esas líneas, en ese orden? (las de «prueba=hid», sin los dos espacios)
  local s=$1 q=$2; shift 2
  serie "$s" | sed -n 's/^  //p' > "$T/hid.lin"
  if python3 -I - "$T/hid.lin" "$@" <<'PY' > "$T/hid.orden"; then ok "etapa 6, HID: $q"; else ko "etapa 6, HID: $q: $(cat "$T/hid.orden")"; fi
import sys
l = open(sys.argv[1], encoding='utf-8').read().split('\n')
i = 0
for q in sys.argv[2:]:
    while i < len(l) and l[i] != q:
        i += 1
    if i == len(l):
        print('falta: ' + q)
        sys.exit(1)
    i += 1
PY
}
if python3 -I "$I2/hazdatos.py" --mira; then ok "etapa 6, HID: los guiones de i2chid/ (reales.txt, sint.txt, qemu.txt) están al día en prhid64.cas (hazdatos.py)"
else ko "etapa 6, HID: prhid64.cas no lleva los guiones de ahora (vuelva a hacerlo con i2chid/hazdatos.py)"; fi
python3 -I "$I2/hazi2chid.py" "$T/i2chid.aml" && python3 -I "$I2/hazi2chid.py" "$T/i2cnull.aml" null
if cmp -s "$T/i2chid.aml" "$I2/i2chid-qemu.aml"; then ok "etapa 6, I2C-HID: hazi2chid.py hace la misma SSDT de prueba que i2chid/i2chid-qemu.aml"
else ko "etapa 6, I2C-HID: i2chid/i2chid-qemu.aml no es la que hace hazi2chid.py"; fi
mb1 "apagar sinpruebas prueba=hid" "$T/e6h.txt" 128 -machine q35; c=$?
S=$T/e6h.txt
if [ $c = 85 ] && hay $S "^HID: 16 descriptores de prueba (touchpads de verdad y los aparatos USB de QEMU): 11 ratones, 1 punteros absolutos, 2 teclados (2 con LED), 8 con modo de entrada, 1 mal hechos; 13 informes leídos, 7 descriptores HID de I2C y 6 órdenes$" \
   && hay $S "^i2c: no hay controladora I2C (Intel LPSS)$" && hay $S "^i2c-hid: ACPI no dice ningún dispositivo HID sobre I2C (PNP0C50)$" \
   && hay $S "^I2C-HID: en las tablas de ACPI (buscado sin AML, en 1 tablas: [0-9]* dispositivos): 0 dispositivos HID sobre I2C (PNP0C50)$" && hay $S "^Hispa-64 etapa 10: todo en orden.$"; then
  ok "etapa 6, HID: los 16 descriptores de prueba, analizados en el arranque; sin controladora I2C ni PNP0C50 (QEMU), lo dice y sigue"
else ko "etapa 6, HID con prueba=hid no sale (código $c): $(serie $S | grep -E '^HID|^I2C|^i2c|MAL|parado' | paste -sd '|')"; fi
hid_orden $S "Dell XPS 9360 (Synaptics, 665 octetos): el ratón es el informe 2 y el modo de entrada, el 4; touchpad de precisión" \
  "descriptor: 665 octetos, 13 informes" "ratón: informe 2, 2 botones en el bit 0, X en el 8, Y en el 16, de 8 bits; rueda en el 0, de 0 bits; 3 octetos" \
  "modo: informe 4, en el bit 0, de 8 bits; el informe, 1 octetos" "digitalizador: 5"
hid_orden $S "ELAN 04F3:300B y 04F3:3045: el ratón con rueda y AC Pan (el informe 1, 8 octetos) y el modo de dos octetos (el 3)" \
  "descriptor: 361 octetos, 11 informes" "ratón: informe 1, 2 botones en el bit 0, X en el 8, Y en el 16, de 8 bits; rueda en el 24, de 8 bits; 8 octetos" \
  "pan: en el bit 32, de 8 bits" "modo: informe 3, en el bit 0, de 8 bits; el informe, 2 octetos" "descriptor: 356 octetos, 10 informes" \
  "ratón: informe 1, 2 botones en el bit 0, X en el 8, Y en el 16, de 8 bits; rueda en el 24, de 8 bits; 8 octetos" "modo: informe 3, en el bit 0, de 8 bits; el informe, 2 octetos"
hid_orden $S "ELAN 04F3:313A (tres botones, el modo de 16 bits), Synaptics 06CB:CE08 (751 octetos) y Dell 06CB:75DB (521)" \
  "descriptor: 464 octetos, 14 informes" "ratón: informe 1, 3 botones en el bit 0, X en el 8, Y en el 16, de 8 bits; rueda en el 0, de 0 bits; 8 octetos" \
  "modo: informe 3, en el bit 0, de 16 bits; el informe, 2 octetos" "descriptor: 751 octetos, 15 informes" \
  "ratón: informe 2, 2 botones en el bit 0, X en el 8, Y en el 16, de 8 bits; rueda en el 0, de 0 bits; 3 octetos" "modo: informe 4, en el bit 0, de 8 bits; el informe, 1 octetos" \
  "descriptor: 521 octetos, 13 informes" "ratón: informe 2, 2 botones en el bit 0, X en el 8, Y en el 16, de 8 bits; rueda en el 0, de 0 bits; 3 octetos"
hid_orden $S "la pantalla táctil ELAN del XPS 9360 (945 octetos, con Push y Pop): ni ratón, ni puntero absoluto (son dedos), ni modo; Touch Screen" \
  "descriptor: 945 octetos, 6 informes" "ratón: no" "absoluto: no" "teclado: no" "modo: no" "digitalizador: 4"
hid_orden $S "los informes del ELAN (con signo, botones, rueda; otro identificador o corto, no), sus descriptores HID de 30 octetos (bien y los que no valen), el modo a 0 y las órdenes" \
  "informe: dx=5 dy=-5 botones=1 rueda=1" "informe: dx=-128 dy=127 botones=2 rueda=-1" "informe: no es del ratón" "informe: no es del ratón" \
  "hid: bien: 04F3:300B, informe de 361 octetos en 0002h, entrada en 0003h (34), órdenes en 0005h, datos en 0006h" "modo 0: 05 00 33 03 06 00 05 00 03 00 00" \
  "hid: mal: no es la versión 1.00" "hid: mal: todo FFh o todo 0 (no contesta un HID)" "hid: mal: no llegan los 30 octetos" \
  "hid: mal: no dice que mide 30 octetos" "hid: mal: sus registros no tienen sentido" \
  "orden: 05 00 00 08" "orden: 05 00 00 01" "orden: 05 00 3F 03 14" "orden: 20 00 34 02"
hid_orden $S "el Dell a mano (sus informes, su descriptor HID, su modo de un octeto), uno que se acaba a medias y un ratón USB de 12 bits y 5 botones" \
  "informe: dx=-1 dy=1 botones=2 rueda=0" "informe: dx=0 dy=0 botones=3 rueda=0" "informe: no es del ratón" \
  "hid: bien: 046D:B000, informe de 665 octetos en 0020h, entrada en 0021h (64), órdenes en 0023h, datos en 0024h" "modo 0: 23 00 34 03 24 00 04 00 04 00" \
  "descriptor: 13 octetos, 1 informes, mal hecho" "ratón: no" \
  "ratón: informe 0, 5 botones en el bit 0, X en el 8, Y en el 20, de 12 bits; rueda en el 32, de 8 bits; 5 octetos" "informe: dx=-1 dy=0 botones=17 rueda=-1"
hid_orden $S "usb-mouse de QEMU (52 octetos): 5 botones, X, Y y la rueda de 8 bits, 4 octetos; sus informes" \
  "descriptor: 52 octetos, 1 informes" "ratón: informe 0, 5 botones en el bit 0, X en el 8, Y en el 16, de 8 bits; rueda en el 24, de 8 bits; 4 octetos" \
  "absoluto: no" "teclado: no" "informe: dx=5 dy=-5 botones=1 rueda=1" "informe: dx=-128 dy=127 botones=18 rueda=-1"
hid_orden $S "usb-tablet de QEMU (74 octetos): no es un ratón; el puntero absoluto, X e Y de 16 bits de 0 a 32767, la rueda, 6 octetos; sus informes" \
  "descriptor: 74 octetos, 1 informes" "ratón: no" \
  "absoluto: informe 0, 3 botones en el bit 0, X en el 8 (16 bits, de 0 a 32767), Y en el 24 (16 bits, de 0 a 32767); rueda en el 40, de 8 bits; 6 octetos" \
  "absoluto: x=16383 y=32767 botones=1 rueda=1" "absoluto: x=0 y=0 botones=6 rueda=-1" "absoluto: no es del puntero"
hid_orden $S "usb-kbd de QEMU (63 octetos): los 8 modificadores, 6 teclas de 8 bits en el bit 16 (de 0 a 255: 25 FF sin signo), 5 LED; un modificador en el arreglo, el rollover, corto; los LED" \
  "descriptor: 63 octetos, 1 informes" "teclado: informe 0, 8 modificadores en el bit 0, 6 teclas de 8 bits en el bit 16 (usos desde 00h, de 0 a 255); 8 octetos" \
  "leds: informe 0, 5 en el bit 0; 1 octetos" "teclado: modificadores 02h, teclas 04 05" "teclado: modificadores 02h, teclas 1D" \
  "teclado: no es del teclado (o demasiadas teclas a la vez)" "teclado: no es del teclado (o demasiadas teclas a la vez)" "leds 3: 03" "leds 7: 07"
hid_orden $S "un ratón con AC Pan (la rueda de lado) y un teclado NKRO con identificador (un mapa de 56 teclas) y sus LED" \
  "descriptor: 61 octetos, 1 informes" "pan: en el bit 32, de 8 bits" "informe: dx=1 dy=2 botones=4 rueda=-1" "pan=-2" \
  "descriptor: 47 octetos, 1 informes" "teclado: informe 1, 8 modificadores en el bit 0, un mapa de 56 teclas en el bit 8 (desde 04h); 8 octetos" \
  "leds: informe 1, 3 en el bit 0; 1 octetos" "teclado: modificadores 02h, teclas 04 07" "teclado: no es del teclado (o demasiadas teclas a la vez)" "leds 2: 01 02"
# la SSDT de prueba (dos touchpads, uno «ausente» con el _CRS de quien no es Windows 8)
mb1 "apagar sinpruebas" "$T/e6i.txt" 256 -machine q35 -acpitable file="$I2/i2chid-qemu.aml"; c=$?
S=$T/e6i.txt
if [ $c = 85 ] && hay $S "^I2C-HID: en las tablas de ACPI (buscado sin AML, en 2 tablas: [0-9]* dispositivos): 2 dispositivos HID sobre I2C (PNP0C50)$" \
   && hay $S "^  ._SB.PCI0.I2C1.TPD0: ELAN1200 (_CID PNP0C50), _STA 0Fh$" \
   && hay $S "^    I2cSerialBus 15h a 400 kHz por ._SB.PCI0.I2C1; la controladora: ._SB.PCI0.I2C1, _ADR 00150001h: 00:15.1$" \
   && hay $S "^    GpioInt 55; _DSM: el descriptor HID en 0001h (con el GUID de HID sobre I2C); SSCN 528/640/30; FMCN 128/160/30$" \
   && hay $S "^  ._SB.PCI0.I2C1.TPD1: SYNA2393 (_CID PNP0C50), _STA 00h$" \
   && hay $S "^    I2cSerialBus 2Ch a 100 kHz por ._SB.PCI0.I2C1, de su plantilla SBFB (su _CRS no es fijo); la controladora: ._SB.PCI0.I2C1, _ADR 00150001h: 00:15.1$" \
   && hay $S "^    Interrupt 16; _DSM: el descriptor HID en 0020h; SSCN 528/640/30; FMCN 128/160/30$" \
   && hay $S "^i2c: no hay controladora I2C (Intel LPSS)$" && hay $S "^i2c-hid: ACPI dice 2 dispositivos HID sobre I2C, pero no hay controladora I2C en marcha$" \
   && hay $S "^HID: 16 descriptores de prueba " && hay $S "^Hispa-64 etapa 10: todo en orden.$"; then
  ok "etapa 6, I2C-HID con la SSDT de prueba (sin AML): el ELAN1200 (15h, 400 kHz, I2C1 = 00:15.1, GpioInt 55, _DSM 0001h, SSCN/FMCN) y el Synaptics de su plantilla SBFB (2Ch, 100 kHz, _DSM 0020h); sin controladora, lo dice"
else ko "etapa 6, I2C-HID con la SSDT de prueba no sale (código $c): $(serie $S | grep -E '^I2C|^  ._SB|^    |^i2c|MAL|parado' | paste -sd '|')"; fi
# como la del ThinkPad E14 Gen 5 (Raptor Lake): «NULL» en el I2cSerialBus: la controladora, su padre
mb1 "apagar sinpruebas" "$T/e6n2.txt" 256 -machine pc -acpitable file="$T/i2cnull.aml"; c=$?
S=$T/e6n2.txt
if [ $c = 85 ] && hay $S "^    I2cSerialBus 15h a 400 kHz por NULL, de su plantilla SBFB (su _CRS no es fijo); la controladora: ._SB.PCI0.I2C0 (su padre), _ADR 00150000h: 00:15.0$" \
   && hay $S "^    Interrupt 16; _DSM: el descriptor HID en 0001h; FMCN 100/214/28$" \
   && hay $S "^    I2cSerialBus 2Ch a 400 kHz por NULL, de su plantilla SBFB (su _CRS no es fijo); la controladora: ._SB.PCI0.I2C3 (su padre), por su nombre: 00:15.3$" \
   && hay $S "^i2c-hid: ACPI dice 2 dispositivos HID sobre I2C, pero no hay controladora I2C en marcha$" && hay $S "^Hispa-64 etapa 10: todo en orden.$"; then
  ok "etapa 6, I2C-HID como el ThinkPad E14 («NULL»): la controladora, la de su padre (I2C0 por su _ADR, 00:15.0; I2C3 por su nombre, 00:15.3)"
else ko "etapa 6, I2C-HID con «NULL» no sale (código $c): $(serie $S | grep -E '^I2C|^  ._SB|^    |^i2c|MAL|parado' | paste -sd '|')"; fi
# las tablas de verdad del ASUS GL553VD (su DSDT, partida en SSDT de 60 KB, y sus 10 SSDT)
rm -f "$T"/asusd*.aml
if python3 -I "$I2/comossdt.py" "$I2/asus-gl553vd/DSDT.BIN" "$T/asusd"; then
  AT=""
  for f in "$T"/asusd*.aml "$I2"/asus-gl553vd/SSDT*.BIN; do AT="$AT -acpitable file=$f"; done
  # shellcheck disable=SC2086
  mb1 "apagar sinpruebas" "$T/e6a.txt" 256 -machine q35 $AT; c=$?
  S=$T/e6a.txt
  if [ $c = 85 ] && hay $S "^I2C-HID: en las tablas de ACPI (buscado sin AML, en 16 tablas: [0-9]* dispositivos): 2 dispositivos HID sobre I2C (PNP0C50)$" \
     && hay $S "^  ._SB.PCI0.I2C0.TPD0: XXXX0000 (_CID PNP0C50), _STA: lo decide un método$" \
     && hay $S "^    I2cSerialBus 20h a 400 kHz por ._SB.PCI0.I2C0, de su plantilla SBFB (su _CRS no es fijo); la controladora: ._SB.PCI0.I2C0, _ADR 00150000h: 00:15.0$" \
     && hay $S "_DSM: el descriptor HID, en HID2 (lo pone su _INI) (con el GUID de HID sobre I2C)$" \
     && hay $S "^    según el BIOS, su _INI lo hace: SYNA2393 (descriptor en 0020h); 06CB2846 (descriptor en 0020h); ELAN1200 (15h a 400 kHz, descriptor en 0001h); FTE1200 (15h a 400 kHz, descriptor en 0001h)$" \
     && hay $S "^  ._SB.PCI0.I2C1.TPL1: XXXX0000 (_CID PNP0C50)" && hay $S "^    según el BIOS, su _INI lo hace: ATML3432 (4Ch a 400 kHz, descriptor en 0000h); " \
     && hay $S "^i2c-hid: ACPI dice 2 dispositivos HID sobre I2C, pero no hay controladora I2C en marcha$" && ! hay $S "trozos sin entender" && hay $S "^Hispa-64 etapa 10: todo en orden.$"; then
    ok "etapa 6, I2C-HID con las tablas de verdad del ASUS GL553VD (sin AML, nada sin entender): TPD0 en I2C0 (00:15.0), su plantilla (20h, 400 kHz) y lo que su _INI hace según el BIOS: el ELAN1200 en 15h con el descriptor en 0001h (y SYNA2393, 06CB2846, FTE1200); TPL1 (la pantalla táctil) en I2C1"
  else ko "etapa 6, I2C-HID con las tablas del ASUS no sale (código $c): $(serie $S | grep -E '^I2C|^  ._SB|^    |^i2c|MAL|parado' | cut -c1-160 | paste -sd '|')"; fi
else ko "etapa 6, I2C-HID: comossdt.py no parte la DSDT del ASUS"; fi
# «sini2c»: ni se busca nada de I2C (las pruebas de los descriptores, sí)
mb1 "apagar sinpruebas sini2c" "$T/e6s.txt" 256 -machine q35 -acpitable file="$I2/i2chid-qemu.aml"; c=$?
S=$T/e6s.txt
if [ $c = 85 ] && hay $S "^(sini2c: ni las controladoras I2C ni los touchpads HID sobre I2C)$" && ! hay $S "^I2C-HID\|^i2c" && hay $S "^HID: 16 descriptores" && hay $S "^Hispa-64 etapa 10: todo en orden.$"; then
  ok "etapa 6, sini2c: ni las controladoras I2C ni los PNP0C50"
else ko "etapa 6, sini2c no sale (código $c): $(serie $S | grep -E '^I2C|^i2c|^.sini2c|parado' | paste -sd '|')"; fi
# una BAR sin poner (como las I2C de Tiger y Raptor Lake): la del edu de QEMU, a 0; Pci64.asigna_bar le da otra
mb1 "apagar sinpruebas prueba=barcero" "$T/e6b.txt" 256 -machine q35 -device edu; c=$?
S=$T/e6b.txt
if [ $c = 85 ] && hay $S "^i2c: (prueba=barcero) la zona 0 de 00:0[0-9A-F].0 1234:11E8 (estaba en 0x[0-9A-F]*), a 0: puesta en 0x[0-9A-F]* (1,0 MB); el aparato dice 0x[0-9A-F]*; se lee bien: sí$" \
   && hay $S "^Hispa-64 etapa 10: todo en orden.$"; then
  ok "etapa 6, una BAR sin poner (la del edu de QEMU, a 0): $(serie $S | grep -o 'puesta en 0x[0-9A-F]*'), libre, y el aparato se lee bien en ella"
else ko "etapa 6, prueba=barcero no sale (código $c): $(serie $S | grep -E 'barcero|parado' | paste -sd '|')"; fi

# ---- por UEFI de 64 bits (OVMF), con GRUB y multiboot2 ----
# ---- [etapa 6] la consola y el intérprete ----
# HIJOS, en cada arranque de los de arriba (fork, execve, wait4, los huérfanos y
# los zombis; la consola como terminal; poll y select con plazo); y el
# intérprete ORDENES con las teclas del monitor de QEMU (sendkey), escritas
# con el mapa español por programas/teclas.py: las órdenes, un archivo escrito
# en un disco, un programa con sus argumentos y su código, uno que no está, los
# acentos con las teclas muertas, la ñ y AltGr, Retroceso, las flechas, la
# historia, Ctrl+U, Ctrl+C (al programa y en el indicador), Ctrl+D, y «salir»,
# que apaga. Para repetirlo con otro teclado (USB): con_interprete con sus
# opciones de QEMU.
e6c_bien() {   # SALIDA
  local v; v=$(serie "$1")
  grep -q "^Consola: una terminal (modo canónico con eco y edición de línea, y crudo), " <<< "$v" \
    && grep -q "^HIJOS: todo bien ([0-9]* pruebas)$" <<< "$v" && grep -q "^Programa «HIJOS»: acaba con el código 0, como debe" <<< "$v" \
    && grep -q "^  el hijo cerrado por su señal: estado 11 (SIGSEGV)$" <<< "$v" \
    && grep -q "^  execve de HIJOS en un hijo: sus argumentos y su entorno, lo de O_CLOEXEC cerrado: 42$" <<< "$v" \
    && grep -q "^Después de los hijos: 0 procesos, 0 en la tabla, 0 espacios, 0 tablas de programas; 4[0-9] fork, 1 execve, 4[0-9] recogidos por wait4, 2 huérfanos$" <<< "$v" \
    && ! grep -q "MAL:" <<< "$v" && grep -q "^Hispa-64 etapa 10: todo en orden.$" <<< "$v"
}
for x in mb1:1 q35:4 smp8:8 smp2:2; do
  f=$T/${x%%:*}.txt
  if e6c_bien "$f"; then
    ok "etapa 6, HIJOS con ${x##*:} procesador(es) (${x%%:*}): $(serie "$f" | grep '^HIJOS: todo bien' | sed 's/^HIJOS: //'); «$(serie "$f" | grep '^Después de los hijos:' | cut -d';' -f2 | sed 's/^ //')»"
  else ko "etapa 6, HIJOS (${x%%:*}) no dice lo que debe: $(serie "$f" | grep -E '^HIJOS|^Programa «HIJOS|^Después de los hijos|MAL|parado' | cut -c1-120 | paste -sd '|')"; fi
done
manda_teclas() {   # MONITOR.sock TEXTO... (con el mapa español; {intro} {retro} {izq} {arriba} {ctrl-c} {pausa}...)
  python3 -I "$AQUI/programas/teclas.py" "$@"
}
con_interprete() {   # SALIDA TEXTO [opciones de QEMU]: «apagar sinpruebas escribir ordenes»; las teclas, al ver el indicador
  local s=$1 x=$2 q c; shift 2
  rm -f "$s" "$T/mon7.sock"
  timeout 180 qemu-system-x86_64 -kernel "$T/HISPA64.BIN" -initrd "$MODS" -append "apagar sinpruebas escribir ordenes" -display none -serial file:"$s" \
    -monitor unix:"$T/mon7.sock",server,nowait $DBG -no-reboot -nic none -m 256 -machine q35 -smp 4 "$@" > /dev/null 2>&1 &
  q=$!
  for _ in $(seq 1 300); do grep -q "^/> " "$s" 2> /dev/null && break; sleep 0.2; done
  manda_teclas --serie "$s" "$T/mon7.sock" "$x"
  wait $q; c=$?
  return $c
}
mkdir -p "$T/w7"; rm -f "$T/w7/fat.img"
truncate -s 48M "$T/w7/fat.img" && mkfs.fat -F 32 -n ESCRIBIR "$T/w7/fat.img" > /dev/null
# ([etapa 11] {indicador}: la orden siguiente, cuando el intérprete ha vuelto a dar
# su indicador: lo tecleado antes salía, con el eco, en medio de lo que aún escribía
# la de antes, a veces)
TECLAS7='ver{intro}{indicador}dir /{intro}{indicador}cd /sata0{intro}{indicador}mkdir prueba{intro}{indicador}cd prueba{intro}{indicador}echo hola desde el teclado > saludo.txt{intro}{indicador}'
TECLAS7+='type saludo.txt{intro}{indicador}cp saludo.txt copia.txt{intro}{indicador}dir{intro}{indicador}mkdir /nada{intro}{indicador}cd /{intro}{indicador}HOLA uno dos{intro}{indicador}CUENTA 3{intro}{indicador}'
TECLAS7+='noexiste{intro}{indicador}echo canción ñandú pingüino àlex a@b#c{intro}{indicador}echo holx{retro}a{intro}{indicador}echo mudo{izq}{izq}n{intro}{indicador}{arriba}{intro}{indicador}'
TECLAS7+='basura{ctrl-u}echo limpio{intro}{indicador}echo Hola {bloqmayus}abcñ{bloqmayus}def{intro}{indicador}a medias{ctrl-c}{indicador}'
TECLAS7+='HIJOS lee{intro}{pausa}una línea{intro}{pausa}{ctrl-c}{indicador}'
TECLAS7+='HIJOS lee{intro}{pausa}otra{intro}{ctrl-d}{indicador}'
# [etapa 7] Lua (el de verdad, con musl) con un guion y a mano; MBUCLE, que no llama al sistema, parado con Ctrl+C
TECLAS7+='LUA /PRUEBA.LUA uno dos{intro}{indicador}LUA{intro}{pausa}{pausa}print(6 * 7, "ñandú"){intro}{pausa}{ctrl-d}{indicador}'
TECLAS7+='MBUCLE{intro}{pausa}{pausa}{pausa}{ctrl-c}{indicador}salir{intro}'
con_interprete "$T/e7.txt" "$TECLAS7" -drive file="$T/w7/fat.img",if=none,id=s0,format=raw -device ide-hd,drive=s0,bus=ide.0; c=$?
v=$(serie "$T/e7.txt")
if [ $c = 85 ] && grep -q "^ORDENES 1.0, el intérprete de órdenes de Hispa-64 (en Hispa-64 0.10.0, etapa 10 " <<< "$v" \
   && grep -q "^ *[0-9.]*  HOLA$" <<< "$v" && grep -q "^         <DIR>  sata0/$" <<< "$v" && grep -q "^  39 archivos (" <<< "$v" \
   && [ "$(grep -c "^hola desde el teclado$" <<< "$v")" = 1 ] && grep -q "^            22  copia.txt$" <<< "$v" \
   && grep -q "^/sata0/prueba> " <<< "$v" \
   && grep -q "^mkdir: «/nada»: es de solo leer .*(EROFS, error 30 de Linux)$" <<< "$v" \
   && grep -q "^  argc = 3: HOLA uno dos$" <<< "$v" && grep -q "^HOLA: todo bien (28 pruebas)$" <<< "$v" && ! grep -q "^(HOLA " <<< "$v" \
   && grep -q "^(CUENTA ha acabado con el código 30)$" <<< "$v" \
   && grep -q "^No hay ninguna orden ni ningún programa que se llame «noexiste»" <<< "$v" \
   && grep -q "^canción ñandú pingüino àlex a@b#c$" <<< "$v" \
   && grep -q "^hola$" <<< "$v" && [ "$(grep -c "^mundo$" <<< "$v")" = 2 ] && grep -q "^limpio$" <<< "$v" && grep -q "^Hola ABCÑdef$" <<< "$v" \
   && grep -q "a medias^C$" <<< "$v" \
   && grep -q "^  leído: una línea$" <<< "$v" && grep -q "^(HIJOS se ha cerrado por la señal 2, SIGINT: Ctrl+C)$" <<< "$v" \
   && grep -q "^  leído: otra$" <<< "$v" && grep -q "^HIJOS lee: fin del archivo$" <<< "$v" \
   && grep -q "^LUA: todo bien (16 pruebas); hoy es " <<< "$v" && grep -q "^Lua 5.5.1  Copyright (C) 1994-2026 Lua.org, PUC-Rio$" <<< "$v" \
   && grep -q "^42[[:space:]]\+ñandú$" <<< "$v" && grep -q "^MBUCLE: doy vueltas sin llamar al sistema" <<< "$v" \
   && grep -q "^(MBUCLE se ha cerrado por la señal 2, SIGINT: Ctrl+C)$" <<< "$v" \
   && grep -q "^El intérprete de órdenes ha acabado (código 0).$" <<< "$v" \
   && [ "$(mtype -i "$T/w7/fat.img" ::prueba/copia.txt 2> /dev/null)" = "hola desde el teclado" ] \
   && fsck.fat -n "$T/w7/fat.img" > "$T/w7/fsck.txt" 2>&1; then
  ok "etapa 6, el intérprete ORDENES con las teclas del monitor de QEMU (PS/2, mapa español, $(manda_teclas -n "$TECLAS7" | wc -w) teclas): ver, dir, cd, mkdir, echo > archivo, type, cp (en el disco: fsck.fat bien), EROFS, HOLA uno dos, CUENTA 3 (código 30), una que no está, «canción ñandú pingüino àlex a@b#c», Bloq Mayús, Retroceso, las flechas, la historia, Ctrl+U, Ctrl+C (en el indicador y a HIJOS: 130), Ctrl+D; [etapa 7] Lua con un guion y a mano (print(6 * 7)), y Ctrl+C a MBUCLE, que no llama al sistema; salir"
else ko "etapa 6, el intérprete no hace lo que debe (código $c): $(sed -n '/^ORDENES, el/,$p' <<< "$v" | tr -d '\b' | cut -c1-80 | paste -sd '|' | cut -c1-1500)"; fi
# lo mismo por un teclado USB (sin 8042: todo llega por tecla_hid)
TECLAS7U='echo canción ñandú a@b{intro}{indicador}echo holx{retro}a{intro}{indicador}echo Hola {bloqmayus}abc{bloqmayus}d{intro}{indicador}HOLA uno{intro}{indicador}salir{intro}'
con_interprete "$T/e7u.txt" "$TECLAS7U" -machine q35,i8042=off -device qemu-xhci,id=xhci -device usb-kbd,bus=xhci.0; c=$?
v=$(serie "$T/e7u.txt")
if [ $c = 85 ] && grep -q "^Teclado PS/2: no hay 8042" <<< "$v" && grep -q "^canción ñandú a@b$" <<< "$v" && grep -q "^hola$" <<< "$v" \
   && grep -q "^Hola ABCd$" <<< "$v" && grep -q "^  argc = 2: HOLA uno$" <<< "$v" && grep -q "^El intérprete de órdenes ha acabado (código 0).$" <<< "$v"; then
  ok "etapa 6, el intérprete con un teclado USB (sin 8042): los acentos, la ñ, AltGr, Retroceso, Bloq Mayús, HOLA uno y salir"
else ko "etapa 6, el intérprete con el teclado USB no hace lo que debe (código $c): $(sed -n '/^ORDENES, el/,$p' <<< "$v" | tr -d '\b' | cut -c1-80 | paste -sd '|' | cut -c1-800)"; fi
if [ -f $OVMF_C ] && command -v grub-mkimage > /dev/null && command -v mcopy > /dev/null; then
  "$AQUI/imagen.sh" "$T/uefi" prueba > "$T/img.txt" 2>&1 || ko "imagen.sh no hace la imagen: $(tail -2 "$T/img.txt" | paste -sd ' ')"
  # [etapa 7] (y otra sin las pruebas de musl, para las vueltas que no las necesitan)
  "$AQUI/imagen.sh" "$T/uefi-sm" prueba "apagar sinmusl" > "$T/img-sm.txt" 2>&1 || ko "imagen.sh no hace la imagen sin musl: $(tail -2 "$T/img-sm.txt" | paste -sd ' ')"
  IMG="$T/uefi/hispa64.img"
  uefi() {   # SALIDA [opciones de QEMU]
    local s=$1; shift
    rm -f "$s"; cp $OVMF_V "$T/vars.fd"
    timeout 200 qemu-system-x86_64 -machine q35 -drive if=pflash,format=raw,readonly=on,file=$OVMF_C \
      -drive if=pflash,format=raw,file="$T/vars.fd" -drive file="$IMG",format=raw,if=virtio \
      -display none -serial file:"$s" $DBG -no-reboot -nic none "$@" > /dev/null 2>&1
  }
  uefi "$T/uefi.txt" -m 128 -monitor none; c=$?
  S=$(serie "$T/uefi.txt")
  if [ $c = 85 ] && grep -q "Hispa-64 0.10.0 ─ Copyright" <<< "$S" && grep -q "^Cargador: GRUB .*, multiboot2, con UEFI de 64 bits$" <<< "$S" \
     && grep -q "^Pantalla: [0-9]* x [0-9]* puntos, 32 bits por punto" <<< "$S" && grep -q "^  consola de [0-9]* x [0-9]* letras" <<< "$S" \
     && grep -q "^  la pantalla, en 0xFFFFC00000000000, con escritura combinada (PAT)$" <<< "$S" \
     && grep -q "^  módulo del cargador: HISPA64.SIM, .*: los nombres de [0-9]* funciones (HISPA64.SIM)$" <<< "$S" \
     && grep -q "^Memoria: 1[0-9][0-9],[0-9] MB libres en [0-9]* trozos" <<< "$S" && grep -q "^Montón: 2000 reservas" <<< "$S" \
     && grep -q "^ACPI: 2.0 o más, de «BOCHS »; XSDT en 0x[0-9A-F]*, [0-9]* tablas: FACP APIC HPET MCFG .*DSDT (todas con la suma bien)$" <<< "$S" \
     && grep -q "ACPI NVS" <<< "$S" && grep -q "^Hispa-64 etapa 10: todo en orden.$" <<< "$S"; then
    ok "por UEFI de 64 bits (OVMF + GRUB, multiboot2) con 128 MB: «$(grep '^Cargador' <<< "$S")»; «$(grep '^Memoria:' <<< "$S" | cut -d';' -f1)»; la pantalla del GOP con escritura combinada, el módulo HISPA64.SIM, RSDP 2.0 y «todo en orden»"
  else ko "por UEFI con 128 MB no dice lo que debe (código $c): $(grep -E 'Memoria:|pantalla, en|parado' <<< "$S" | paste -sd ' ')"; fi
  e7_ok "$T/uefi.txt" 1 "por UEFI (OVMF y GRUB) con 1 procesador y 128 MB"
  # [etapa 6] el USB por UEFI: OVMF deja la BAR del xHCI por encima de 4 GB, y ha usado él antes el teclado y la memoria
  copia_usb
  IMG="$T/uefi-sm/hispa64.img"
  uefi "$T/uefi6.txt" -m 512 -smp 2 -monitor none -device qemu-xhci,id=xhci -device usb-kbd,bus=xhci.0 -device usb-tablet,bus=xhci.0 \
    -drive if=none,id=u1,format=raw,file="$T/w6/usbfat.img" -device usb-storage,bus=xhci.0,drive=u1; c=$?
  S=$(serie "$T/uefi6.txt")
  if [ $c = 85 ] && grep -q "^xHCI: 00:0[0-9A-F].0 1B36:000D en 0x[0-9A-F]\{9,\} (por encima de 4 GB)$" <<< "$S" && grep -q "^USB: 3 aparatos (0 concentradores)" <<< "$S" \
     && grep -q "HID: teclado (por su descriptor, con LED)$" <<< "$S" && grep -q "HID: tableta (por su descriptor, de 0 a 32767)$" <<< "$S" \
     && grep -q "^  /usb0p1: FAT32 «USBFAT», .*; de solo leer$" <<< "$S" && grep -q "^Hispa-64 etapa 10: todo en orden.$" <<< "$S"; then
    ok "por UEFI, el USB después de OVMF: «$(grep '^xHCI:' <<< "$S" | cut -d' ' -f4-)», el teclado, la tableta y la memoria (de solo leer)"
  else ko "por UEFI, el USB no sale (código $c): $(grep -E '^xHCI|^USB|^  /usb|parado' <<< "$S" | paste -sd '|')"; fi
  e4_ok "$T/uefi.txt" 1 "por UEFI (GRUB, module2) con 1 procesador"
  IMG="$T/uefi/hispa64.img"
  uefi "$T/uefi-max.txt" -m 3G -cpu max -smp 4,cores=4 -monitor none; c=$?
  S=$(serie "$T/uefi-max.txt")
  if [ $c = 85 ] && grep -q "páginas de 1 GB: sí" <<< "$S" && grep -q "; 4 procesadores lógicos en el paquete" <<< "$S" \
     && grep -q "^  0x00000001.*disponible$" <<< "$S" && grep -q "^Memoria: 2,9 GB libres" <<< "$S" && grep -q "en páginas de 1 GB: [1-9]" <<< "$S" \
     && grep -q "; de 1 GB, también$" <<< "$S" && grep -q "^Hispa-64 etapa 10: todo en orden.$" <<< "$S" \
     && grep -q "^  procesadores (MADT): 4 encendidos (APIC 0, 1, 2, 3)$" <<< "$S" && tics_bien "$T/uefi-max.txt" "el HPET" 50 \
     && grep -q "^IOAPIC: el temporizador 0 del HPET, por la GSI 1[6-9], al vector 0x3E: 20 de 20 interrupciones (cada 10 ms)$" <<< "$S" \
     && grep -q "^PCI (PCIe, por ECAM): [0-9]* funciones en los buses 0 a 255, 1 BAR por encima de 4 GB$" <<< "$S" \
     && grep -q "^      BAR4 0x[0-9A-F]\{9,\}, 16 KB de memoria de 64 bits (por encima de 4 GB), precargable$" <<< "$S" \
     && pci_igual "$T/uefi-max.txt" -machine q35 -m 3G -cpu max -smp 4,cores=4 -drive file="$T/uefi/hispa64.img",format=raw,if=virtio \
     && e3_bien "$T/uefi-max.txt" 4; then
    ok "por UEFI con -cpu max, 4 núcleos y 3 GB (1 por encima de 4 GB): «$(grep '^Memoria:' <<< "$S" | cut -d';' -f1)»; el mapa directo y la prueba con páginas de 1 GB; la MADT con 4; «$(grep '^Tic:' <<< "$S")»; PCIe con la BAR del disco virtio por encima de 4 GB, la misma lista que QEMU"
  else ko "por UEFI con -cpu max y 3 GB no dice lo que debe (código $c): $(grep -E 'lógicos|Memoria:|parado' <<< "$S" | paste -sd ' ') $(e3_dice "$T/uefi-max.txt")"; fi
  e4_ok "$T/uefi-max.txt" 4 "por UEFI con 4 procesadores y -cpu max"
  e7_ok "$T/uefi-max.txt" 4 "por UEFI con 4 procesadores y -cpu max (AVX con xsave)" avx
  IMG="$T/uefi-sm/hispa64.img"
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
  "$AQUI/imagen.sh" "$T/uefi-p" prueba "sinmusl" > "$T/img2.txt" 2>&1
  rm -f "$T/pant.txt" "$T/mon.sock" "$T/pantalla.ppm"; cp $OVMF_V "$T/vars2.fd"
  timeout 120 qemu-system-x86_64 -machine q35 -m 256 -drive if=pflash,format=raw,readonly=on,file=$OVMF_C \
    -drive if=pflash,format=raw,file="$T/vars2.fd" -drive file="$T/uefi-p/hispa64.img",format=raw,if=virtio \
    -display none -serial file:"$T/pant.txt" -monitor unix:"$T/mon.sock",server,nowait -no-reboot -nic none > /dev/null 2>&1 &
  q=$!
  for _ in $(seq 1 90); do grep -q "^/> " "$T/pant.txt" 2> /dev/null && break; sleep 1; done
  sleep 1
  python3 -I "$AQUI/pantallazo.py" captura "$T/mon.sock" "$T/pantalla.ppm" 2> /dev/null
  wait $q 2> /dev/null
  if [ -s "$T/pantalla.ppm" ]; then
    read -r an al am gr ve <<< "$(python3 -I "$AQUI/pantallazo.py" mira "$T/pantalla.ppm" "$T/pantalla.png")"
    if [ "${am:-0}" -gt 800 ] && [ "${gr:-0}" -gt 20000 ] && [ "${ve:-0}" -gt 300 ]; then
      ok "la consola se ve en la pantalla del GOP ($an x $al), con escritura combinada: $am puntos amarillos («Memoria: ...»), $gr grises (el texto) y $ve verdes («todo en orden»); en $T/pantalla.png"
    else ko "la pantalla no tiene la consola ($an x $al: $am amarillos, $gr grises, $ve verdes; $T/pantalla.png)"; fi
  else ko "no se ha podido sacar el pantallazo"; fi
  # [etapa 8] la pantalla para los programas, las ventanas y la oficina (por
  # UEFI: sin el GOP no hay /dev/fb0): MDISP con los sucesos del teclado y de
  # la tableta; VENTANAS con NOTAS (guarda en un disco exFAT), arrastrar una
  # ventana, el menú Inicio, Pluma 2 (guarda en FAT32), Cuentas 2 (una
  # fórmula, guarda), Alt+F4, salir del escritorio (la consola vuelve, con
  # el cursor) y cls (que borra de verdad). Los pantallazos, comprobados con
  # pantallazo.py (los puntos de cada color en cada sitio); las teclas, por
  # el monitor (teclas.py); el ratón, la tableta USB por QMP.
  PZ="python3 -I $AQUI/pantallazo.py"
  "$AQUI/imagen.sh" "$T/uefi-e8" prueba "apagar sinpruebas escribir ordenes" > "$T/img-e8.txt" 2>&1 || ko "imagen.sh no hace la imagen de la etapa 8: $(tail -2 "$T/img-e8.txt" | paste -sd ' ')"
  rm -f "$T/e8fat.img" "$T/e8exfat.img"
  truncate -s 48M "$T/e8fat.img" && mkfs.fat -F 32 -n DATOS "$T/e8fat.img" > /dev/null && mmd -i "$T/e8fat.img" ::/DOCUMENT
  echo "volumen exFAT de la etapa 8" > "$T/e8leeme.txt"
  python3 -I "$AQUI/programas/hazexfat.py" "$T/e8exfat.img" $(( 32 * 1048576 )) 0 EXFAT8 LEEME.TXT="$T/e8leeme.txt" > /dev/null
  S8="$T/e8.txt"; M8="$T/mon8.sock"; Q8="$T/qmp8.sock"
  rm -f "$S8" "$M8" "$Q8" "$T"/e8-*.ppm; cp $OVMF_V "$T/vars8.fd"
  timeout 400 qemu-system-x86_64 -machine q35 -m 1024 -smp 2 -drive if=pflash,format=raw,readonly=on,file=$OVMF_C \
    -drive if=pflash,format=raw,file="$T/vars8.fd" -drive file="$T/uefi-e8/hispa64.img",format=raw,if=none,id=d0 -device ide-hd,drive=d0,bus=ide.0 \
    -drive file="$T/e8fat.img",format=raw,if=none,id=d1 -device ide-hd,drive=d1,bus=ide.1 \
    -drive file="$T/e8exfat.img",format=raw,if=none,id=d2 -device ide-hd,drive=d2,bus=ide.2 \
    -device qemu-xhci,id=xhci -device usb-tablet,bus=xhci.0 -display none -serial file:"$S8" \
    -monitor unix:"$M8",server,nowait -qmp unix:"$Q8",server,nowait $DBG -no-reboot -nic none > /dev/null 2>&1 &
  q=$!
  espera8() {   # PATRÓN SEGUNDOS: hasta que la salida lo tenga (o se acabe el plazo)
    local i
    for i in $(seq 1 $(( $2 * 5 ))); do serie "$S8" | grep -q "$1" && return 0; sleep 0.2; done
    return 1
  }
  teclas8() { python3 -I "$AQUI/programas/teclas.py" "$M8" "$@" 2> /dev/null; sleep 0.3; }
  raton8() {   # X Y [clic]: la tableta en el punto (X, Y) de la pantalla de 1280 x 800, y un clic
    python3 -I - "$Q8" "$@" <<'PY' 2> /dev/null
import json, socket, sys, time
s = socket.socket(socket.AF_UNIX); s.connect(sys.argv[1]); f = s.makefile('rw')
f.readline(); f.write('{"execute": "qmp_capabilities"}\n'); f.flush(); f.readline()
def manda(ev):
    f.write(json.dumps({"execute": "input-send-event", "arguments": {"events": ev}}) + '\n'); f.flush(); f.readline()
manda([{"type": "abs", "data": {"axis": "x", "value": (int(sys.argv[2]) * 32767 + 1278) // 1279}},
       {"type": "abs", "data": {"axis": "y", "value": (int(sys.argv[3]) * 32767 + 798) // 799}}])
if len(sys.argv) > 4:
    time.sleep(0.15)
    for d in (True, False):
        manda([{"type": "btn", "data": {"button": "left", "down": d}}]); time.sleep(0.15)
PY
    sleep 0.4
  }
  boton8() {   # 1 o 0: el izquierdo, pulsado o soltado
    python3 -I - "$Q8" "$1" <<'PY' 2> /dev/null
import json, socket, sys
s = socket.socket(socket.AF_UNIX); s.connect(sys.argv[1]); f = s.makefile('rw')
f.readline(); f.write('{"execute": "qmp_capabilities"}\n'); f.flush(); f.readline()
f.write(json.dumps({"execute": "input-send-event", "arguments": {"events": [{"type": "btn", "data": {"button": "left", "down": sys.argv[2] == '1'}}]}}) + '\n'); f.flush(); f.readline()
PY
    sleep 0.3
  }
  foto8() { $PZ foto "$M8" "$T/e8-$1.ppm" 2> /dev/null; }
  bien8=1
  paso8() { if ! espera8 "$1" "$2"; then bien8=0; echo "  (etapa 8: no llega «$1»)"; fi; }
  paso8 "^/> " 150
  # /dev/input/event0 con un programa: MDISP lee 6 sucesos (la a, la tableta, la ñ)
  teclas8 'MDISP sucesos 7{intro}'; paso8 "^MDISP: esperando 7 sucesos" 20
  teclas8 'a'; raton8 640 400; teclas8 'ñ'; paso8 "^MDISP: 7 sucesos" 20; teclas8 '{retro}'
  # (si llega antes el soltar del Intro de la orden, se salta: lo de la a en adelante)
  EV8=$(serie "$S8" | grep '^suceso: ' | sed '/^suceso: tecla 30 1$/,$!d' | head -6 | paste -sd '|')
  # el escritorio con NOTAS, que escribe en el disco exFAT
  teclas8 'VENTANAS NOTAS=/sata2/NOTA.TXT{intro}'; paso8 "ventana nueva «Notas" 60
  sleep 2
  teclas8 'Hola, exFAT: ñandú y pingüino.{intro}Otra línea.{ctrl-g}'; paso8 "^NOTAS: guardado (" 20
  sleep 1; foto8 notas
  # arrastrar la ventana por su título: de (300, 33) a (500, 133)
  raton8 300 33; boton8 1; raton8 400 83; raton8 500 133; boton8 0; paso8 "movida a (240, 148)" 10
  sleep 1; foto8 movida
  teclas8 '{esc}'; paso8 "ventana fuera «Notas" 15
  # el menú Inicio con el ratón, y Pluma desde él
  raton8 50 783 clic; paso8 "menú Inicio abierto" 10
  sleep 1; foto8 inicio
  raton8 100 615 clic; paso8 "ventana nueva «Pluma»" 60; paso8 "^pluma: lista" 30
  sleep 2
  teclas8 'Carta de prueba: ñandú y pingüino.{ctrl-g}'; paso8 "^pluma: cuadro «Guardar como»" 20
  sleep 1; teclas8 'carta{intro}'; paso8 "^pluma: guardado C:.DOCUMENT.carta.PLU" 20
  sleep 1; foto8 pluma
  teclas8 '{alt-f4}'; paso8 "^pluma: adiós" 20
  # Cuentas, con una fórmula
  raton8 50 783 clic; sleep 0.5; raton8 100 655 clic; paso8 "ventana nueva «Cuentas»" 60; paso8 "^cuentas: lista" 30
  sleep 2
  teclas8 '12{intro}30{intro}=A1*A2+1{intro}'; sleep 2; foto8 cuentas
  teclas8 '{ctrl-g}'; paso8 "^cuentas: cuadro «Guardar como»" 20
  sleep 1; teclas8 'libro{intro}'; paso8 "^cuentas: guardado " 20
  teclas8 '{alt-f4}'; paso8 "ventana fuera «libro" 20
  # salir del escritorio: la consola otra vez; cls la borra y el cursor se ve
  raton8 50 783 clic; sleep 0.5; raton8 100 735 clic; paso8 "fuera; la pantalla vuelve a la consola" 20
  paso8 "^/> " 10; sleep 1; foto8 consola
  teclas8 'cls{intro}'; sleep 1.5; foto8 cls
  teclas8 'salir{intro}'
  wait $q; c=$?
  V8=$(serie "$S8")
  # lo que se ve, en los pantallazos
  r() { $PZ "$@" 2> /dev/null || echo 0; }
  n_tit=$(r cuenta "$T/e8-notas.ppm" 40 21 768 24 1F3F6E)          # el título de la activa (oscuro)
  n_txt=$(r oscuros "$T/e8-notas.ppm" 48 52 400 40)                 # el texto escrito
  n_ini=$(r cuenta "$T/e8-notas.ppm" 4 770 92 26 2E7D32 6)          # el botón Inicio
  n_mov=$(r cuenta "$T/e8-movida.ppm" 240 121 768 24 1F3F6E)        # el título, donde se ha llevado
  n_dif=$(r difiere "$T/e8-notas.ppm" "$T/e8-movida.ppm" 0 0 1280 766)
  n_men=$(r cuenta "$T/e8-inicio.ppm" 5 551 298 40 2E7D32 6)        # la cabecera del menú Inicio
  n_plu=$(r cuenta "$T/e8-pluma.ppm" 240 170 780 500 FFFFFF)        # la página de Pluma
  n_plt=$(r oscuros "$T/e8-pluma.ppm" 320 255 600 30)              # lo escrito en ella
  n_cue=$(r oscuros "$T/e8-cuentas.ppm" 56 186 86 58)               # las cifras de A1 a A3
  n_con=$(r cuenta "$T/e8-consola.ppm" 0 0 1280 800 AAAAAA)         # la consola, otra vez
  n_cls=$(r cuenta "$T/e8-cls.ppm" 0 0 1280 800 AAAAAA)             # tras cls: «/> » y el cursor
  n_cur=$(r cuenta "$T/e8-cls.ppm" 24 14 8 2 AAAAAA)                # el cursor: una raya debajo de la casilla 3 de la primera fila
  if [ $c = 85 ] && [ $bien8 = 1 ] \
     && [ "$EV8" = "suceso: tecla 30 1|suceso: letra 97|suceso: tecla 30 0|suceso: posición x 640|suceso: posición y 400|suceso: tecla 39 1" ]; then
    ok "etapa 8, /dev/input/event0 (MDISP, con EVIOCGRAB): la a (KEY_A pulsada, EV_LETRA 97, soltada), la tableta en (640, 400) y la ñ"
  else ko "etapa 8, /dev/input/event0 no da lo que debe (código $c): $EV8"; fi
  if [ $bien8 = 1 ] && [ "$n_tit" -gt 8000 ] && [ "$n_txt" -gt 300 ] && [ "$n_ini" -gt 1500 ] && [ "$n_mov" -gt 8000 ] && [ "$n_dif" -gt 50000 ] && [ "$n_men" -gt 8000 ] \
     && [ "$(python3 -I "$AQUI/programas/miraexfat.py" "$T/e8exfat.img" tipo NOTA.TXT 2> /dev/null)" = "$(printf 'Hola, exFAT: ñandú y pingüino.\nOtra línea.')" ] \
     && python3 -I "$AQUI/programas/miraexfat.py" "$T/e8exfat.img" > /dev/null 2>&1; then
    ok "etapa 8, VENTANAS y NOTAS: la ventana (título de la activa: $n_tit puntos oscuros; $n_txt de letra), la barra con Inicio ($n_ini), arrastrada por el título a (240, 148) ($n_dif puntos cambian), el menú Inicio con el ratón; NOTA.TXT guardado en el disco exFAT (miraexfat: bien, lo escrito con ñ y ü)"
  else ko "etapa 8, VENTANAS y NOTAS no hacen lo que deben: título $n_tit, letra $n_txt, Inicio $n_ini, movida $n_mov/$n_dif, menú $n_men; $(grep -E '^VENTANAS|^NOTAS|parado' <<< "$V8" | tail -6 | paste -sd '|')"; fi
  if [ $bien8 = 1 ] && [ "$n_plu" -gt 150000 ] && [ "$n_plt" -gt 400 ] && [ "$n_cue" -gt 60 ] && mdir -i "$T/e8fat.img" ::/DOCUMENT/carta.PLU > /dev/null 2>&1 \
     && [ "$(mtype -i "$T/e8fat.img" ::/DOCUMENT/carta.PLU | head -c 4 | od -An -c | tr -d ' ')" != "" ] \
     && mdir -b -i "$T/e8fat.img" ::/DOCUMENT 2> /dev/null | grep -qi 'libro' && fsck.fat -n "$T/e8fat.img" > /dev/null 2>&1; then
    ok "etapa 8, la oficina de Hispa-Pentium con so_hispa64.c: Pluma 2 (desde el menú Inicio; escrito y guardado en FAT32: DOCUMENT/carta.PLU, $(mdir -i "$T/e8fat.img" ::/DOCUMENT/carta.PLU 2> /dev/null | grep -i carta | awk '{print $3}') octetos), Cuentas 2 (12, 30 y =A1*A2+1; guardado: $(mdir -b -i "$T/e8fat.img" ::/DOCUMENT | grep -i libro | sed 's|.*/||')), Alt+F4; fsck.fat sin quejas"
  else ko "etapa 8, Pluma o Cuentas no hacen lo que deben: página $n_plu, letra $n_plt, celdas $n_cue; $(grep -E '^pluma|^cuentas|parado' <<< "$V8" | tail -6 | paste -sd '|'); $(mdir -b -i "$T/e8fat.img" ::/DOCUMENT 2>&1 | paste -sd ' ')"; fi
  if [ $bien8 = 1 ] && [ "$n_con" -gt 20000 ] && [ "$n_cls" -lt 400 ] && [ "$n_cls" -gt 20 ] && [ "$n_cur" = 16 ] \
     && grep -q "^VENTANAS: fuera; la pantalla vuelve a la consola$" <<< "$V8"; then
    ok "etapa 8, salir del escritorio: la consola vuelve entera ($n_con puntos grises); cls la borra de verdad (quedan $n_cls: «/> » y el cursor, una raya de 8 x 2 debajo de la casilla)"
  else ko "etapa 8, la consola al salir del escritorio no está bien: $n_con grises, tras cls $n_cls, el cursor $n_cur"; fi
else
  echo "  (sin OVMF, grub-mkimage o mtools: no se prueba el arranque por UEFI)"
fi

# ---- [etapa 9] la red: e1000, e1000e, virtio-net y la Realtek (con la
# RTL8139C+ de QEMU, «realtek=ensayo»), por la red de usuario de QEMU (slirp:
# el anfitrión es 10.0.2.2), con 1, 2, 4 y 8 procesadores; los servidores de
# prueba (programas/red/servidores.py) en el anfitrión: HTTP, HTTPS (con una
# autoridad de prueba), NTP (una hora adelantada), eco y, si se puede, DNS
# (en 127.0.0.1:53, con IP fija); el eco al revés (el servidor en Hispa-64,
# por hostfwd); y RED.PRB, las órdenes que corre el núcleo (prred64.cas) ----
R9=$T/red9; mkdir -p "$R9"; SRV=$AQUI/programas/red/servidores.py; PIDS9=""
head -c 1048576 /dev/urandom > "$R9/a.bin"; SHA9=$(sha256sum "$R9/a.bin" | cut -c1-64)
rm -rf "$R9/certs"; python3 -I "$SRV" certs "$R9/certs" anfitrion.prueba > /dev/null 2>&1; cp "$R9/certs/ca.pem" "$R9/CA.PEM" 2> /dev/null
srv9() {   # VARIABLE MODO ARGUMENTOS...: arranca un servidor y deja su puerto en VARIABLE
  local v=$1 k=0; shift
  (cd "$R9" && exec python3 -I "$SRV" "$@") > "$R9/srv_$1.txt" 2>&1 & PIDS9="$PIDS9 $!"
  until grep -q '^listo' "$R9/srv_$1.txt" 2> /dev/null || [ $k -gt 50 ]; do sleep 0.2; k=$((k + 1)); done
  eval "$v=$(grep -o '^listo [0-9]*' "$R9/srv_$1.txt" | grep -o '[0-9]*$')"
}
PH9=""; PS9=""; PN9=""; PE9=""; PD9=""
srv9 PH9 http 0 a.bin; srv9 PS9 https 0 a.bin certs/servidor.pem certs/servidor.key; srv9 PN9 ntp 0 3600; srv9 PE9 eco 0
srv9 PD9 dns 53 10.0.2.2
red9() {   # SALIDA NIC CPUS LÍNEA [dns]: arranca con RED.PRB y el cliente del eco
  local s=$1 nic=$2 n=$3 l=$4 m="RED.PRB,CA.PEM"
  {
    echo "PING -c 3 -i 200 10.0.2.2"
    echo "IFCONFIG"
    echo "DESCARGA http://10.0.2.2:$PH9/a.bin"
    echo "DESCARGA http://10.0.2.2:$PH9/trozos"
    echo "DESCARGA -c /CA.PEM -n anfitrion.prueba https://10.0.2.2:$PS9/a.bin"
    echo "=4 DESCARGA https://10.0.2.2:$PS9/a.bin"
    if [ "${5:-}" = dns ]; then
      echo "DESCARGA -o /dev/null -c /CA.PEM https://anfitrion.prueba:$PS9/a.bin"
      echo "PING -c 1 anfitrion.prueba"
      echo "=2 DESCARGA http://noexiste.zz/"
    fi
    echo "NTP -p $PN9 10.0.2.2"
    echo "ECO cliente 10.0.2.2 $PE9 1000000"
    echo "ECO servidor 7000 1"
  } > "$R9/RED.PRB"
  for f in "$T"/prog/*; do m="$m,$f"; done
  # [etapa 10] la red por USB: usb-net (CDC-ECM) detrás de un xHCI
  local nicq="-nic user,model=$nic,hostfwd=tcp:127.0.0.1:$((17000 + n))-:7000"
  [ "$nic" = usb-net ] && nicq="-device qemu-xhci,id=xhci -netdev user,id=u0,hostfwd=tcp:127.0.0.1:$((17000 + n))-:7000 -device usb-net,netdev=u0,bus=xhci.0"
  rm -f "$s" "$s.eco"
  ( for i in $(seq 150); do python3 -I -c '
import socket, sys
s = socket.create_connection(("127.0.0.1", int(sys.argv[1])), timeout=30); d = bytes(range(256)) * 400; s.sendall(d); s.shutdown(1); r = b""
while True:
    x = s.recv(65536)
    if not x: break
    r += x
sys.exit(0 if r == d else 1)' "$((17000 + n))" 2> /dev/null && { echo bien > "$s.eco"; break; }; sleep 1; done ) &
  local cli=$!
  (cd "$R9" && timeout 300 qemu-system-x86_64 -kernel "$T/HISPA64.BIN" -initrd "$m" -append "apagar sinmusl $l" -display none -serial file:"$s" \
    -monitor none $DBG -no-reboot -m 512 -machine q35 -smp "$n" -rtc base="${RTC9:-localtime}" \
    $nicq > /dev/null 2>&1); local c=$?
  kill $cli 2> /dev/null; wait $cli 2> /dev/null
  return $c
}
red9_bien() {   # SALIDA [dns]
  local s=$1 v; v=$(serie "$s")
  [ -n "$PH9" ] && [ -n "$PS9" ] && [ -n "$PN9" ] && [ -n "$PE9" ] \
    && grep -q "^  eth0: .*, IP 10.0.2.15/24; " <<< "$v" && grep -q "^PING 10.0.2.2: 3 enviados, 3 recibidos, 0 % perdidos" <<< "$v" \
    && grep -q "^IFCONFIG: 2 interfaces$" <<< "$v" && grep -q "^puerta de enlace: 10.0.2.2 (por eth0)$" <<< "$v" \
    && [ "$(grep -c "^DESCARGA: 1048576 octetos en .*; sha256 = $SHA9$" <<< "$v")" -ge 3 ] \
    && grep -q "^DESCARGA: TLS 1.2 (suite 0x[0-9A-F]*), certificado de «anfitrion.prueba» bien$" <<< "$v" \
    && grep -q "certificado no válido: es de otro nombre" <<< "$v" \
    && { [ "${2:-}" != dns ] || { grep -q "^PING anfitrion.prueba (10.0.2.2): 56 octetos de datos" <<< "$v" && grep -q "^PING 10.0.2.2: 1 enviados, 1 recibidos" <<< "$v" && grep -q "^Programa «=2 DESCARGA\|^Programa «DESCARGA http://noexiste.zz/»: acaba con el código 2, como debe" <<< "$v" && [ "$(grep -c "^DESCARGA: 1048576 octetos en .*; sha256 = $SHA9$" <<< "$v")" -ge 4 ]; }; } \
    && grep -q "^NTP: 10.0.2.2:$PN9 dice .*; el reloj iba 36[0-9][0-9][0-9][0-9][0-9] ms atrasado .*; puesto en hora$" <<< "$v" \
    && grep -qE "^  la hora, puesta [0-9]+ veces; el CMOS, escrito [0-9]+ veces: va (-1|0|1) s del reloj \(en (hora local|UTC)\)$" <<< "$v" \
    && grep -q "^ECO: 1000000 octetos de ida y vuelta, iguales" <<< "$v" && grep -q "^ECO: servidor: 1 conexiones, 102400 octetos devueltos$" <<< "$v" \
    && [ -f "$s.eco" ] \
    && ! grep -q "^Programa .*: acaba con el código [0-9]*$" <<< "$v" \
    && grep -q "^Después de la red: 0 sockets (.*0 objetos$" <<< "$v" \
    && ! grep -q "MAL:" <<< "$v" && grep -q "^Hispa-64 etapa 10: todo en orden.$" <<< "$v"
}
red9_dice() { serie "$1" | grep -E '^Red|^PING [0-9a-z.]*:|^DESCARGA: [0-9c]|^NTP:|^ECO: [0-9s]|^Programa .*código [0-9]*$|MAL|parado|^Después de la red' | cut -c1-140 | paste -sd '|' | cut -c1-1800; }
red9_ok() {   # SALIDA QUÉ [dns]
  local v; v=$(serie "$1")
  if red9_bien "$1" "${3:-}"; then
    ok "etapa 9, $2: $(grep '^Red: [0-9]' <<< "$v" | head -1 | sed 's/^Red: //; s/ (y «lo».*//'), $(grep -m1 '^Red: 00' <<< "$v" | sed 's/^Red: [0-9A-F:.]* //' | cut -d, -f1); DHCP o IP fija, PING al anfitrión ($(grep '^PING 10.0.2.2:' <<< "$v" | grep -o 'máx = [0-9,/]*')), IFCONFIG, DESCARGA por HTTP, a trozos y por HTTPS (sha256 bien; sin la autoridad, rechazado)$( [ "${3:-}" = dns ] && echo ', por su nombre con DNS (y uno que no existe: código 2)'), NTP ($(grep -o 'iba [0-9]* ms atrasado' <<< "$v"): puesto en hora), ECO de ida y vuelta (1 MB) y de servidor; sin sockets al acabar"
  else ko "etapa 9, $2 no hace lo que debe: $(red9_dice "$1")"; fi
}
if [ -n "$PH9" ] && [ -n "$PS9" ] && [ -f "$R9/CA.PEM" ]; then
  red9 "$T/e9a.txt" e1000 1 ""
  red9_ok "$T/e9a.txt" "e1000 (la de VirtualBox), 1 procesador, DHCP"
  RTC9=utc red9 "$T/e9b.txt" e1000e 2 "cmos=utc zona=UTC red=pierde"
  red9_ok "$T/e9b.txt" "e1000e (82574L, con MSI), 2 procesadores, [etapa 10] tirando uno de cada 23 segmentos que llegan (red=pierde)"
  if serie "$T/e9b.txt" | grep -qE "^  TCP \[etapa 10\], después de la red: [0-9]+ conexiones con escala de ventana, [1-9][0-9]* trozos desordenados guardados y [1-9][0-9]* juntados"; then
    ok "etapa 10, TCP: lo desordenado (red=pierde), guardado en su sitio y juntado al llegar lo que faltaba: $(serie "$T/e9b.txt" | grep -o '[0-9]* trozos desordenados guardados y [0-9]* juntados' | tail -1), y las descargas, con su sha256"
  else ko "etapa 10, TCP no guarda lo desordenado: $(serie "$T/e9b.txt" | grep '^  TCP \[')"; fi
  if [ -n "$PD9" ]; then
    red9 "$T/e9c.txt" virtio-net-pci 4 "ip=10.0.2.15/24,10.0.2.2,10.0.2.2 sindhcp" dns
    red9_ok "$T/e9c.txt" "virtio-net, 4 procesadores, IP fija y DNS (el de prueba)" dns
  else
    red9 "$T/e9c.txt" virtio-net-pci 4 ""
    red9_ok "$T/e9c.txt" "virtio-net, 4 procesadores (sin el DNS de prueba: el puerto 53 no se deja)"
  fi
  red9 "$T/e9d.txt" rtl8139 8 "realtek=ensayo"
  red9_ok "$T/e9d.txt" "la Realtek RTL8168 (con la RTL8139C+ de QEMU, «realtek=ensayo»), 8 procesadores"
  # [etapa 10] la red por USB: CDC-ECM (la segunda configuración de usb-net; la primera, RNDIS)
  red9 "$T/e10u.txt" usb-net 2 ""
  red9_ok "$T/e10u.txt" "[etapa 10] la red por USB (usb-net de QEMU, CDC-ECM, detrás del xHCI), 2 procesadores"
  v=$(serie "$T/e10u.txt")
  if grep -q "^Red: USB 1-[0-9]* 0525:A4A2 «RNDIS/QEMU USB Network Device», CDC-ECM (la configuración 1 de 2), MAC 40:54:00:12:34:56, a granel por los extremos 0x82 y 0x02 (de 64 octetos), con las interrupciones del xHCI$" <<< "$v"; then
    ok "etapa 10, la red por USB: la configuración de CDC-ECM (la 2.ª de las dos: la 1.ª es RNDIS), la variante 1 de los datos, la MAC de su iMACAddress, recibir sin el mutex del aparato (el xHCI despierta al hilo «red»)"
  else ko "etapa 10, la red por USB no dice lo que debe: $(grep -E '^Red: USB|^USB|^  1-' <<< "$v" | paste -sd '|')"; fi
  # la zona: la hora local con cambio de horario, y el CMOS en UTC o en hora local
  v=$(serie "$T/e9b.txt"); w=$(serie "$T/e9a.txt")
  if grep -q "zona UTC (TZ=UTC0), el CMOS en UTC$" <<< "$v" && grep -q "zona Europa/Madrid (TZ=CET-1CEST,M3.5.0,M10.5.0/3), el CMOS en hora local$" <<< "$w" \
     && grep -q "la hora: [0-9/]* [0-9:]* CE[S]*T (UTC+[12])" <<< "$w"; then
    ok "etapa 9, la hora: $(grep -o 'la hora: .*' <<< "$w" | cut -d, -f1) (Europa/Madrid, con el CMOS en hora local), y en UTC con «cmos=utc zona=UTC»"
  else ko "etapa 9, la hora y la zona no salen como deben: $(grep -o 'la hora: .*' <<< "$w") | $(grep -o 'la hora: .*' <<< "$v")"; fi
else
  ko "etapa 9: no se arrancan los servidores de prueba (servidores.py): $(cat "$R9"/srv_*.txt 2> /dev/null | head -3 | paste -sd ' ')"
fi
# shellcheck disable=SC2086
kill $PIDS9 2> /dev/null

# ---- [etapa 10] las INTx por el _PRT (Aml64 y Prt64): MIRAPRT (modelo
# linux64) con las tablas de verdad del ASUS, el Toshiba y el HP Mini (lo
# que sale, igual que prt/esperado.txt); en QEMU q35, la e1000 por la GSI
# del _PRT (los GSIA-GSIH de _CRS fijo) sin mirar cada 10 ms; con «pc» (los
# LNKA de _CRS hecho por un método), la línea y mirando ----
if python3 -I "$C/ccastel.py" "$AQUI/prt/miraprt.cas" -o "$T/miraprt.asm" -I "$C/biblioteca64/linux" -I "$C/biblioteca" -I "$AQUI" > /dev/null 2>&1 \
   && nasm -f bin "$T/miraprt.asm" -o "$T/miraprt" && chmod +x "$T/miraprt"; then
  for e in "$AQUI/i2chid/asus-gl553vd" "$AQUI/../../tests/acpi/toshiba-nb250" "$AQUI/../../tests/acpi/hp-mini-5103"; do
    echo "== $(basename "$e")"; "$T/miraprt" "$e/DSDT.BIN" "$e"/SSDT*.BIN | iconv -f CP437 -t UTF-8
  done > "$T/miraprt.txt" 2>&1
  if cmp -s "$T/miraprt.txt" "$AQUI/prt/esperado.txt"; then
    ok "etapa 10, el _PRT de las tablas de verdad (MIRAPRT, en Linux): $(grep '^MIRAPRT' "$T/miraprt.txt" | cut -d, -f1,3 | sed 's/MIRAPRT: //' | paste -sd ';' | sed 's/;/; /g') (ASUS, Toshiba y HP Mini: con la GSI escrita, por nombre y dentro del método)"
  else ko "etapa 10, MIRAPRT no da lo de prt/esperado.txt: $(diff "$T/miraprt.txt" "$AQUI/prt/esperado.txt" | head -4 | paste -sd '|')"; fi
else ko "etapa 10, no se compila MIRAPRT (prt/miraprt.cas)"; fi
v=$(serie "$T/e9a.txt")
if grep -q "^INTx: las rutas de ACPI (_PRT): 1 tablas resueltas, 128 entradas$" <<< "$v" && grep -q "^Red: 00:02.0 8086:100E .*, INTx por la GSI 2[0-3] (del _PRT)$" <<< "$v" \
   && grep -qE "^  eth0: .*; [0-9]+ tramas recibidas y [0-9]+ enviadas, [1-9][0-9]+ interrupciones$" <<< "$v" && ! grep -q "mirando cada 10 ms" <<< "$v"; then
  ok "etapa 10, la INTx por el _PRT en QEMU q35: $(grep -o 'INTx por la GSI [0-9]* (del _PRT)' <<< "$v"), $(grep '^  eth0:' <<< "$v" | tail -1 | grep -o '[0-9]* interrupciones'), sin mirar cada 10 ms"
else ko "etapa 10, la e1000 de q35 no va por la GSI del _PRT: $(grep -E '^INTx|^Red: 00|^  eth0' <<< "$v" | paste -sd '|')"; fi
mb1 "apagar sinmusl sinpruebas" "$T/e10pc.txt" 256 -machine pc -nic user,model=e1000; c=$?
v=$(serie "$T/e10pc.txt")
if [ $c = 85 ] && grep -q "^INTx: las rutas de ACPI (_PRT): 0 tablas resueltas (y 1 sin resolver: sus enlaces los hace un método), 0 entradas$" <<< "$v" \
   && grep -q "^Red: 00:03.0 8086:100E .*, INTx por la GSI 1[01] (y mirando cada 10 ms)$" <<< "$v" && grep -q "^Hispa-64 etapa 10: todo en orden.$" <<< "$v"; then
  ok "etapa 10, con «pc» (los enlaces LNKA, de _CRS hecho por un método): la línea de la configuración, y mirando cada 10 ms"
else ko "etapa 10, con «pc» la INTx no sale como debe (código $c): $(grep -E '^INTx|^Red: 00|parado' <<< "$v" | paste -sd '|')"; fi
# [etapa 10] TCP entre dos de Hispa-64 (MRED, por 127.0.0.1): la escala de
# ventana y SACK por las dos puntas; con «red=pierde», lo desordenado se
# guarda, los ACK llevan SACK, y los 256 KB de ida y vuelta salen iguales
mb1 "apagar red=pierde" "$T/e10p.txt" 512 -machine q35 -smp 2; c=$?
v=$(serie "$T/e10p.txt")
if [ $c = 85 ] && grep -q "^MRED: todo bien (126 pruebas)$" <<< "$v" && grep -q "^Programa «MRED»: acaba con el código 0, como debe" <<< "$v" \
   && grep -qE "^  TCP \[etapa 10\]: [1-9][0-9]* conexiones con escala de ventana, [1-9][0-9]* trozos desordenados guardados y [1-9][0-9]* juntados, [1-9][0-9]* ACK con SACK \(red=pierde" <<< "$v" && grep -q "^Hispa-64 etapa 10: todo en orden.$" <<< "$v"; then
  ok "etapa 10, TCP de Hispa-64 a Hispa-64 (MRED por 127.0.0.1, tirando uno de cada 23): $(grep -o '[0-9]* conexiones con escala de ventana, .* ACK con SACK' <<< "$v"); MRED, todo bien"
else ko "etapa 10, TCP con pérdidas por 127.0.0.1 no sale (código $c): $(grep -E '^MRED|^  TCP \[|MAL|parado' <<< "$v" | head -5 | paste -sd '|')"; fi

# [etapa 10] la red por USB enchufada en marcha (device_add, por el monitor de
# QEMU): el vigía USB la enumera, la red la toma y DHCP le da la IP; al
# desenchufarla (device_del), sin enlace
rm -f "$T/e10hp.txt" "$T/mon10.sock"
timeout 120 qemu-system-x86_64 -kernel "$T/HISPA64.BIN" -initrd "$T/HISPA64.SIM" -append "sinmusl sinpruebas" -display none -serial file:"$T/e10hp.txt" \
  -monitor unix:"$T/mon10.sock",server,nowait $DBG -no-reboot -m 256 -machine q35 -device qemu-xhci,id=xhci -netdev user,id=u0 > /dev/null 2>&1 &
q=$!
for _ in $(seq 1 300); do grep -q "todo en orden" "$T/e10hp.txt" 2> /dev/null && break; sleep 0.2; done
python3 -I "$AQUI/pantallazo.py" orden "$T/mon10.sock" "device_add usb-net,netdev=u0,bus=xhci.0,id=un" 2> /dev/null
for _ in $(seq 1 100); do serie "$T/e10hp.txt" | grep -q "^Red: eth0: IP 10.0.2.15/24 (DHCP" && break; sleep 0.2; done
python3 -I "$AQUI/pantallazo.py" orden "$T/mon10.sock" "device_del un" 2> /dev/null
for _ in $(seq 1 50); do serie "$T/e10hp.txt" | grep -q "^Red: eth0: sin enlace" && break; sleep 0.2; done
python3 -I "$AQUI/pantallazo.py" orden "$T/mon10.sock" "quit" 2> /dev/null
wait $q 2> /dev/null
v=$(serie "$T/e10hp.txt" | tr -s ' ' | sed 's/Reloj: [0-9/]* [0-9:]* (tics: [0-9]*; ocupados: [0-9]*%) //g')
if grep -q "USB: se ha enchufado 1-[0-9]*: 0525:A4A2 «RNDIS/QEMU USB Network Device»" <<< "$v" && grep -q "^Red: USB 1-[0-9]* 0525:A4A2 .*, CDC-ECM (la configuración 1 de 2)" <<< "$v" \
   && grep -q "^Red: eth0: IP 10.0.2.15/24 (DHCP de 10.0.2.2" <<< "$v" && grep -q "USB: se ha desenchufado 1-" <<< "$v" && grep -q "^Red: eth0: sin enlace" <<< "$v" && ! grep -q "parado" <<< "$v"; then
  ok "etapa 10, la red por USB enchufada en marcha (device_add): enumerada, a la red y con IP por DHCP; desenchufada (device_del): sin enlace"
else ko "etapa 10, la red por USB en marcha no sale como debe: $(grep -E 'USB: se|^Red|parado' <<< "$v" | cut -c1-120 | paste -sd '|')"; fi
echo "== Hispa-64: $BIEN bien, $MAL mal =="
[ $MAL = 0 ]
