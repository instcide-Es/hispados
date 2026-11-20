# La cura de adelgazamiento (docs/ADELGAZAR-8086.md), las carga prueba.sh: lo
# que el núcleo y el intérprete dejan en memoria mientras corre un programa,
# y la parte transitoria del intérprete, que un programa puede pisar y se
# vuelve a leer de COMMAND.COM (y, si no está en el disquete, se pide).
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.

p_adelgaza() {
echo "== Hispa-8086: lo que se queda en memoria, y la parte transitoria del intérprete =="
cp $IMG/hispa8086-360.img $T/a.img
mcopy -i $T/a.img build/MIDE.COM build/PISA.COM ::
arranca $T/a.img 486
escribe "limpiar"; escribe "mide"
pantalla m1
local n=($(grep -o "MIDE: [0-9]* [0-9]* [0-9]*" $T/m1.txt | cut -d' ' -f2-))
# delante del intérprete, la tabla de vectores y lo de la BIOS (1.792
# octetos) y el núcleo; entre el intérprete y el programa, lo que deja el
# intérprete (con su entorno y el del programa)
local nucleo=$(( ${n[0]:-99999} - 1792 )) inter=$(( ${n[1]:-99999} - ${n[0]:-0} ))
if [ $nucleo -lt 20000 ]; then ok "el núcleo deja en memoria $nucleo octetos (sin el arranque)"; else ko "el núcleo ocupa $nucleo octetos (${n[*]})"; fi
if [ $inter -lt 5000 ]; then ok "mientras corre un programa, del intérprete quedan $inter octetos (con los entornos)"; else ko "el intérprete deja $inter octetos (${n[*]})"; fi
escribe "limpiar"; escribe "pisa"; sleep 2; escribe "ver"
pantalla m2
esta m2 "PISA: memoria pisada" ""
esta m2 "Hispa-8086 versión 0.12" "la parte transitoria, pisada por un programa, se vuelve a leer de COMMAND.COM: "
apaga
# sin COMMAND.COM en el disquete, se pide; con el bueno puesto (otro
# disquete: otro número de serie), sigue
cp $IMG/hispa8086-360.img $T/a.img
cp $IMG/hispa8086-360.img $T/b.img
printf '\x12\x34\x56\x78' | dd of=$T/b.img bs=1 seek=39 conv=notrunc 2> /dev/null
mcopy -i $T/a.img build/PISA.COM ::
arranca $T/a.img 486
escribe "limpiar"; escribe "renombrar command.com otro.com"; escribe "pisa"; sleep 3
pantalla m3
esta m3 "Ponga el disquete con COMMAND.COM y pulse una tecla..." "sin COMMAND.COM: "
mon "change floppy0 $T/b.img raw" 1
tecla spc; sleep 3; escribe "ver"
pantalla m4
esta m4 "Hispa-8086 versión 0.12" "puesto el disquete con COMMAND.COM, sigue: "
apaga
}
