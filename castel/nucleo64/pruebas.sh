#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo64/pruebas.sh - las pruebas de Hispa-64 (docs/64/HISPA-64.md), etapa 0:
#   - el núcleo (hispa64.cas) se compila igual con ccastel.py y NASM que con
#     ensambla.py (y, si se da CASTEL64=ruta y ENSAMB64=ruta, con ellos);
#   - arranca en QEMU por multiboot (-kernel) y dice lo que tiene que decir por
#     el puerto serie; las excepciones de prueba se explican en español; con un
#     procesador sin modo largo, lo dice y se para;
#   - arranca por UEFI de 64 bits (OVMF) con GRUB (BOOTX64.EFI, multiboot2):
#     con la pantalla del GOP, el mapa del firmware, la RSDP; con -cpu max y 4
#     procesadores; con memoria por encima de 4 GB;
#   - la consola se ve en la pantalla (un pantallazo de QEMU: la letra amarilla
#     del título y la gris del texto), que queda en $T/pantalla.png.
# Uso: nucleo64/pruebas.sh [CARPETA_DE_TRABAJO]   (unos 2 minutos)
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

echo "== Hispa-64, etapa 0 =="
# ---- compilar ----
if python3 -I "$C/ccastel.py" "$AQUI/hispa64.cas" -o "$T/hispa64.asm" -I "$C/nucleo32" > "$T/comp.txt" 2>&1 \
   && nasm -f bin "$T/hispa64.asm" -o "$T/HISPA64.BIN" 2>> "$T/comp.txt"; then
  ok "el núcleo se compila (ccastel.py y NASM): $(stat -c %s "$T/HISPA64.BIN") octetos, $(wc -l < "$T/hispa64.asm") líneas de ensamblador"
else
  ko "el núcleo no se compila: $(head -3 "$T/comp.txt" | paste -sd ' ')"; exit 1
fi
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
mb1() {   # LÍNEA SALIDA [más opciones de QEMU]
  local l=$1 s=$2; shift 2
  rm -f "$s"
  timeout 30 qemu-system-x86_64 -kernel "$T/HISPA64.BIN" -append "$l" -display none -serial file:"$s" -monitor none \
    $DBG -no-reboot -nic none -m 256 "$@" > /dev/null 2>&1
}
mb1 apagar "$T/mb1.txt"; c=$?
if [ $c = 85 ] && serie "$T/mb1.txt" | grep -q "^Hispa-64 0.0.1 ─ Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.$" \
   && serie "$T/mb1.txt" | grep -q "^Cargador: qemu, multiboot$" && serie "$T/mb1.txt" | grep -q "^Modo largo (64 bits): sí" \
   && serie "$T/mb1.txt" | grep -q "mitad alta (desde 0xFFFF800000000000): comprobada" \
   && serie "$T/mb1.txt" | grep -q "^Memoria disponible: 255,4 MB" \
   && serie "$T/mb1.txt" | grep -q "Punto de parada (int3) número 1 .*: atendido, sigue." \
   && serie "$T/mb1.txt" | grep -q "^Hispa-64 etapa 0: todo en orden.$"; then
  ok "por multiboot (QEMU -kernel): el saludo, modo largo, la mitad alta comprobada, 255,4 MB de 256, la int3 atendida y «todo en orden» (código 42)"
else ko "por multiboot no dice lo que debe (código $c): $(serie "$T/mb1.txt" | tail -3 | paste -sd ' ')"; fi
for x in "pagina:14:Fallo de página (#PF):Al leer de 0x0000500000000000: la página no está (desde el núcleo)." \
         "gp:13:Protección general (#GP):Número de error: 0x0" \
         "division:0:División por cero (#DE):RIP=0x" \
         "ud:6:Instrucción no válida (#UD):RIP=0x"; do
  IFS=: read -r p n nom det <<< "$x"
  mb1 "apagar prueba=$p" "$T/exc-$p.txt"; c=$?
  if [ $c = 199 ] && serie "$T/exc-$p.txt" | grep -q "^\*\*\* Hispa-64 se ha parado: excepción $n \*\*\*$" \
     && serie "$T/exc-$p.txt" | grep -qF "$nom" && serie "$T/exc-$p.txt" | grep -qF "$det" \
     && serie "$T/exc-$p.txt" | grep -q "^R13=0x"; then
    ok "prueba=$p: la excepción $n, explicada («$nom», «$det»), con los registros"
  else ko "prueba=$p no dice la excepción $n (código $c): $(serie "$T/exc-$p.txt" | tail -4 | paste -sd ' ')"; fi
done
# lo de más de 64 GB (una pantalla muy arriba): con páginas de 1 GB, y sin ellas no
mb1 "apagar prueba=alto" "$T/alto.txt" -cpu max; c=$?
mb1 "apagar prueba=alto" "$T/alto2.txt"; c2=$?
if [ $c = 85 ] && serie "$T/alto.txt" | grep -q "en 0x0000008000000000 (512 GB) y leer de allí: 0x[0-9A-F]* (leído sin fallo de página)$" \
   && [ $c2 = 85 ] && serie "$T/alto2.txt" | grep -q "no se puede (sin páginas de 1 GB, o más allá de lo que direcciona)$"; then
  ok "prueba=alto: con páginas de 1 GB (-cpu max) pone 1 GB en 512 GB (una PDPT nueva) y lo lee; sin ellas (qemu64) dice que no se puede"
else ko "prueba=alto no va (códigos $c y $c2): $(serie "$T/alto.txt" | grep Prueba)"; fi
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
  uefi "$T/uefi.txt" -m 512 -monitor none; c=$?
  S=$(serie "$T/uefi.txt")
  if [ $c = 85 ] && grep -q "Hispa-64 0.0.1 ─ Copyright" <<< "$S" && grep -q "^Cargador: GRUB .*, multiboot2, con UEFI de 64 bits$" <<< "$S" \
     && grep -q "^Pantalla: [0-9]* x [0-9]* puntos, 32 bits por punto" <<< "$S" && grep -q "^  consola de [0-9]* x [0-9]* letras" <<< "$S" \
     && grep -q "^ACPI: la RSDP (2.0 o más)" <<< "$S" && grep -q "ACPI NVS" <<< "$S" && grep -q "^Hispa-64 etapa 0: todo en orden.$" <<< "$S"; then
    ok "por UEFI de 64 bits (OVMF + GRUB, multiboot2): «$(grep '^Cargador' <<< "$S")»; «$(grep '^Pantalla' <<< "$S" | cut -c1-60)»; consola, mapa del firmware, RSDP 2.0 y «todo en orden»"
  else ko "por UEFI no dice lo que debe (código $c): $(tail -4 <<< "$S" | paste -sd ' ')"; fi
  uefi "$T/uefi-max.txt" -m 3G -cpu max -smp 4,cores=4 -monitor none; c=$?
  S=$(serie "$T/uefi-max.txt")
  if [ $c = 85 ] && grep -q "páginas de 1 GB: sí" <<< "$S" && grep -q "; 4 procesadores lógicos en el paquete" <<< "$S" \
     && grep -q "^  0x00000001.*disponible$" <<< "$S" && grep -q "^Hispa-64 etapa 0: todo en orden.$" <<< "$S"; then
    ok "con -cpu max, 4 núcleos y 3 GB (1 por encima de 4 GB): páginas de 1 GB, 4 procesadores lógicos, memoria disponible por encima de 4 GB; $(grep '^Memoria disponible' <<< "$S")"
  else ko "con -cpu max y 3 GB no dice lo que debe (código $c): $(grep -E 'lógicos|Memoria disp' <<< "$S" | paste -sd ' ')"; fi
  # la pantalla: sin «apagar» se queda parado con todo escrito; un pantallazo
  "$AQUI/imagen.sh" "$T/uefi-p" prueba "" > "$T/img2.txt" 2>&1
  rm -f "$T/pant.txt" "$T/mon.sock" "$T/pantalla.ppm"; cp $OVMF_V "$T/vars2.fd"
  timeout 120 qemu-system-x86_64 -machine q35 -m 512 -drive if=pflash,format=raw,readonly=on,file=$OVMF_C \
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
      ok "la consola se ve en la pantalla del GOP ($an x $al): $am puntos amarillos (el título), $gr grises (el texto) y $ve verdes («todo en orden»); en $T/pantalla.png"
    else ko "la pantalla no tiene la consola ($an x $al: $am amarillos, $gr grises, $ve verdes; $T/pantalla.png)"; fi
  else ko "no se ha podido sacar el pantallazo"; fi
else
  echo "  (sin OVMF, grub-mkimage o mtools: no se prueba el arranque por UEFI)"
fi
echo "== Hispa-64: $BIEN bien, $MAL mal =="
[ $MAL = 0 ]
