#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo64/pruebas.sh - las pruebas de Hispa-64 (docs/64/HISPA-64.md), etapas 0 y 1:
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
#     $T/pantalla.png.
# Uso: nucleo64/pruebas.sh [CARPETA_DE_TRABAJO]   (unos 40 segundos)
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

echo "== Hispa-64, etapas 0 y 1 =="
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

# ---- por multiboot (QEMU -kernel): rápido, solo el puerto serie ----
mb1() {   # LÍNEA SALIDA MEMORIA [más opciones de QEMU]
  local l=$1 s=$2 m=$3; shift 3
  rm -f "$s"
  timeout 60 qemu-system-x86_64 -kernel "$T/HISPA64.BIN" -initrd "$T/HISPA64.SIM" -append "$l" -display none -serial file:"$s" \
    -monitor none $DBG -no-reboot -nic none -m "$m" "$@" > /dev/null 2>&1
}
hay() { serie "$1" | grep -q -- "$2"; }
mb1 apagar "$T/mb1.txt" 128; c=$?
S=$T/mb1.txt
if [ $c = 85 ] && hay $S "^Hispa-64 0.1.0 ─ Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.$" \
   && hay $S "^Cargador: qemu, multiboot$" && hay $S "^Memoria: 12[0-9],[0-9] MB libres en [0-9]* trozos (de 127,4 MB que da el firmware); [0-9]* KB usados por el núcleo$" \
   && hay $S "^  sin la identidad de abajo: 1 MB ya no está en las tablas$" && hay $S "los nombres de [0-9]* funciones (HISPA64.SIM)$" \
   && hay $S "^El núcleo, en la mitad alta: 0xFFFFFFFF80100000 (código, de solo leer)" && hay $S "^TSS con pilas propias (IST)" \
   && hay $S "^Páginas: de 4 KB, puesta, leída, quitada y puesta en otro marco (la TLB, al día); de 2 MB, también; de 1 GB, no (el procesador no las tiene)$" \
   && hay $S "^  el código del núcleo, sin escribir: sí; los datos, sin ejecutar: sí; la guarda de la pila, sin poner: sí$" \
   && hay $S "^Montón: 2000 reservas y liberaciones al azar .*, comprobadas; al acabar, 0 en uso y la memoria física, la de antes" \
   && hay $S "Punto de parada (int3) número 1 .*: atendido, sigue." && hay $S "^Hispa-64 etapa 1: todo en orden.$"; then
  ok "por multiboot con 128 MB: en la mitad alta, «$(serie $S | grep '^Memoria:')», sin la identidad, las páginas de 4 KB y 2 MB, el montón y «todo en orden» (código 42)"
else ko "por multiboot con 128 MB no dice lo que debe (código $c): $(serie $S | grep -E 'Memoria:|Páginas|Montón|parado' | paste -sd ' ')"; fi
mb1 apagar "$T/mb3g.txt" 3G -cpu max; c=$?
S=$T/mb3g.txt
if [ $c = 85 ] && hay $S "^Memoria: 2,9 GB libres" && hay $S "en páginas de 1 GB: [1-9]" && hay $S "; de 1 GB, también$" \
   && hay $S "^Hispa-64 etapa 1: todo en orden.$"; then
  ok "por multiboot con 3 GB y -cpu max: «$(serie $S | grep '^Memoria:' | cut -d';' -f1)»; el mapa directo con páginas de 1 GB; una de 1 GB puesta, leída y quitada"
else ko "por multiboot con 3 GB no dice lo que debe (código $c): $(serie $S | grep -E 'Memoria:|Páginas|parado' | paste -sd ' ')"; fi
mb1 apagar "$T/mb8g.txt" 8G; c=$?
S=$T/mb8g.txt
if [ $c = 85 ] && hay $S "^Memoria: 7,9 GB libres en [0-9]* trozos (de 7,9 GB" && hay $S "en páginas de 1 GB: 0; de 2 MB: 4[0-9][0-9][0-9];" \
   && hay $S "^Hispa-64 etapa 1: todo en orden.$"; then
  ok "por multiboot con 8 GB (qemu64, sin páginas de 1 GB): «$(serie $S | grep '^Memoria:' | cut -d';' -f1)»; el mapa directo, con páginas de 2 MB"
else ko "por multiboot con 8 GB no dice lo que debe (código $c): $(serie $S | grep -E 'Memoria:|directo|parado' | paste -sd ' ')"; fi
mb1 "apagar prueba=monton" "$T/monton.txt" 256; c=$?
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
  mb1 "apagar prueba=$p" "$T/exc-$p.txt" 128; c=$?
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
  mb1 "apagar prueba=$p" "$T/par-$p.txt" 128; c=$?
  if [ $c = 197 ] && hay "$T/par-$p.txt" "^\*\*\* Hispa-64 se ha parado: el montón del núcleo está estropeado \*\*\*$" \
     && hay "$T/par-$p.txt" ": $det$" && hay "$T/par-$p.txt" "^Llamada desde: Monton64.estropeado+0x[0-9A-F]* <- Monton64.libera+0x[0-9A-F]* <- Hispa64.provoca+"; then
    ok "prueba=$p: el núcleo se para y lo dice («$det»), con quién ha llamado"
  else ko "prueba=$p no para el núcleo como debe (código $c): $(serie "$T/par-$p.txt" | tail -5 | paste -sd ' ')"; fi
done
# el NMI: se lo manda el monitor de QEMU
rm -f "$T/nmi.txt" "$T/nmi.sock"
timeout 60 qemu-system-x86_64 -kernel "$T/HISPA64.BIN" -initrd "$T/HISPA64.SIM" -append "apagar prueba=nmi" -display none -serial file:"$T/nmi.txt" \
  -monitor unix:"$T/nmi.sock",server,nowait $DBG -no-reboot -nic none -m 128 > /dev/null 2>&1 &
q=$!
for _ in $(seq 1 100); do grep -q "esperando un NMI" "$T/nmi.txt" 2> /dev/null && break; sleep 0.2; done
python3 -I "$AQUI/pantallazo.py" orden "$T/nmi.sock" nmi 2> /dev/null
wait $q; c=$?
if [ $c = 199 ] && hay "$T/nmi.txt" "^\*\*\* Hispa-64 se ha parado: excepción 2 \*\*\*$" && hay "$T/nmi.txt" "^(Se atiende con su propia pila: la IST 2 de la TSS.)$"; then
  ok "prueba=nmi: el NMI del monitor de QEMU se explica, atendido con su propia pila (la IST 2)"
else ko "prueba=nmi no sale (código $c): $(serie "$T/nmi.txt" | tail -4 | paste -sd ' ')"; fi
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
    timeout 90 qemu-system-x86_64 -machine q35 -drive if=pflash,format=raw,readonly=on,file=$OVMF_C \
      -drive if=pflash,format=raw,file="$T/vars.fd" -drive file="$T/uefi/hispa64.img",format=raw,if=virtio \
      -display none -serial file:"$s" $DBG -no-reboot -nic none "$@" > /dev/null 2>&1
  }
  uefi "$T/uefi.txt" -m 128 -monitor none; c=$?
  S=$(serie "$T/uefi.txt")
  if [ $c = 85 ] && grep -q "Hispa-64 0.1.0 ─ Copyright" <<< "$S" && grep -q "^Cargador: GRUB .*, multiboot2, con UEFI de 64 bits$" <<< "$S" \
     && grep -q "^Pantalla: [0-9]* x [0-9]* puntos, 32 bits por punto" <<< "$S" && grep -q "^  consola de [0-9]* x [0-9]* letras" <<< "$S" \
     && grep -q "^  la pantalla, en 0xFFFFC00000000000, con escritura combinada (PAT)$" <<< "$S" \
     && grep -q "^  módulo del cargador: HISPA64.SIM, .*: los nombres de [0-9]* funciones (HISPA64.SIM)$" <<< "$S" \
     && grep -q "^Memoria: 1[0-9][0-9],[0-9] MB libres en [0-9]* trozos" <<< "$S" && grep -q "^Montón: 2000 reservas" <<< "$S" \
     && grep -q "^ACPI: la RSDP (2.0 o más)" <<< "$S" && grep -q "ACPI NVS" <<< "$S" && grep -q "^Hispa-64 etapa 1: todo en orden.$" <<< "$S"; then
    ok "por UEFI de 64 bits (OVMF + GRUB, multiboot2) con 128 MB: «$(grep '^Cargador' <<< "$S")»; «$(grep '^Memoria:' <<< "$S" | cut -d';' -f1)»; la pantalla del GOP con escritura combinada, el módulo HISPA64.SIM, RSDP 2.0 y «todo en orden»"
  else ko "por UEFI con 128 MB no dice lo que debe (código $c): $(grep -E 'Memoria:|pantalla, en|parado' <<< "$S" | paste -sd ' ')"; fi
  uefi "$T/uefi-max.txt" -m 3G -cpu max -smp 4,cores=4 -monitor none; c=$?
  S=$(serie "$T/uefi-max.txt")
  if [ $c = 85 ] && grep -q "páginas de 1 GB: sí" <<< "$S" && grep -q "; 4 procesadores lógicos en el paquete" <<< "$S" \
     && grep -q "^  0x00000001.*disponible$" <<< "$S" && grep -q "^Memoria: 2,9 GB libres" <<< "$S" && grep -q "en páginas de 1 GB: [1-9]" <<< "$S" \
     && grep -q "; de 1 GB, también$" <<< "$S" && grep -q "^Hispa-64 etapa 1: todo en orden.$" <<< "$S"; then
    ok "por UEFI con -cpu max, 4 núcleos y 3 GB (1 por encima de 4 GB): «$(grep '^Memoria:' <<< "$S" | cut -d';' -f1)»; el mapa directo y la prueba con páginas de 1 GB"
  else ko "por UEFI con -cpu max y 3 GB no dice lo que debe (código $c): $(grep -E 'lógicos|Memoria:|parado' <<< "$S" | paste -sd ' ')"; fi
  uefi "$T/uefi-8g.txt" -m 8G -cpu max -monitor none; c=$?
  S=$(serie "$T/uefi-8g.txt")
  if [ $c = 85 ] && grep -q "^Memoria: 7,9 GB libres" <<< "$S" && grep -q "^Hispa-64 etapa 1: todo en orden.$" <<< "$S"; then
    ok "por UEFI con 8 GB: «$(grep '^Memoria:' <<< "$S" | cut -d';' -f1)»"
  else ko "por UEFI con 8 GB no dice lo que debe (código $c): $(grep -E 'Memoria:|parado' <<< "$S" | paste -sd ' ')"; fi
  # la pantalla: sin «apagar» se queda parado con todo escrito; un pantallazo
  "$AQUI/imagen.sh" "$T/uefi-p" prueba "" > "$T/img2.txt" 2>&1
  rm -f "$T/pant.txt" "$T/mon.sock" "$T/pantalla.ppm"; cp $OVMF_V "$T/vars2.fd"
  timeout 120 qemu-system-x86_64 -machine q35 -m 256 -drive if=pflash,format=raw,readonly=on,file=$OVMF_C \
    -drive if=pflash,format=raw,file="$T/vars2.fd" -drive file="$T/uefi-p/hispa64.img",format=raw,if=virtio \
    -display none -serial file:"$T/pant.txt" -monitor unix:"$T/mon.sock",server,nowait -no-reboot -nic none > /dev/null 2>&1 &
  q=$!
  for _ in $(seq 1 90); do grep -q "Ya puede apagar" "$T/pant.txt" 2> /dev/null && break; sleep 1; done
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
