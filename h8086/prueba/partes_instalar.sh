# Hispa-8086 0.14: el teclado español al arrancar, INSTALAR y MENU (las
# carga prueba.sh). Estas partes usan los disquetes de verdad ($IMG_REAL), con
# TECLADO en su AUTOEXEC.BAT: lo que se teclea son letras, S, N, Esc e Intro,
# que están en el mismo sitio en los dos teclados, salvo lo que se prueba.
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.

# la tecla de la ñ (la del «;» americano) y la del «;» español (Mayúsculas y
# la coma): en español, «ñ;»
tecla_ene() { teclea "eco "; tecla semicolon shift-comma ret; sleep 0.8; }

p_espanol() {
echo "== Hispa-8086: el disquete arranca con el teclado español; TECLADO EE.UU. lo quita =="
cp $IMG_REAL/hispa8086-360.img $T/a.img
arranca $T/a.img 486
pantalla t0
esta t0 "Teclado español." "AUTOEXEC.BAT pone el teclado español al arrancar: "
escribe "limpiar"; tecla_ene
pantalla t1
renglon t1 "ñ;" "sin hacer nada, la ñ y el «;» salen en su sitio: "
escribe "limpiar"; escribe "teclado ee.uu."; tecla_ene
pantalla t2
esta t2 "Teclado normal (el de la BIOS)." "TECLADO EE.UU. vuelve al americano: "
renglon t2 'A:\>eco ;<' "y entonces esas teclas dan «;» y «<»: "
apaga
}

# el disco ($1) con Hispa-8086 instalado: cada archivo, como en los disquetes
duro_part() { dd if=$1 of=$T/part.img bs=512 skip=63 2> /dev/null; }

p_instalar() {
echo "== Hispa-8086: INSTALAR en un disco duro nuevo, con la Oficina, los Juegos y el menú =="
cp $IMG_REAL/hispa8086-360.img $T/a.img
cp $IMG_REAL/oficina8086-360.img $T/o.img
cp $IMG_REAL/juegos8086-360.img $T/j.img
rm -f $T/hd.img; truncate -s $((40 * 16 * 63 * 512)) $T/hd.img
arranca $T/a.img 486 $T/hd.img
escribe "limpiar"; escribe "instalar"; sleep 2
pantalla n0
esta n0 "INSTALAR: Hispa-8086 en el disco duro." "INSTALAR se presenta: "
esta n0 "se borrará TODO lo que tenga el disco duro." "avisa de que borra el disco y pregunta: "
tecla s
if espera n1 "¿Instalar también la Oficina (Pluma y Cuentas)? (S/N)" 150; then ok "crea la partición, le da formato y copia el sistema"; else ko "INSTALAR no llega a preguntar por la Oficina"; fi
esta n1 "C:\\TECLADO.COM" "copia las órdenes a C:: "
tecla s; sleep 1
pantalla n2
esta n2 "Ponga en A: el disquete de OFICINA y pulse una tecla" "pide el disquete de Oficina: "
tecla spc; sleep 2
pantalla n3
esta n3 "No es ese disquete." "con el del sistema puesto, lo dice y lo vuelve a pedir: "
mon "change floppy0 $T/o.img raw" 1; sleep 3; tecla spc
if espera n4 "¿Instalar también los Juegos (Derribo y Marcianada)? (S/N)" 90; then ok "copia la Oficina"; else ko "INSTALAR no acaba de copiar la Oficina"; fi
esta n4 "C:\\OFICINA\\PLUMA.COM" ""
tecla s; sleep 1; mon "change floppy0 $T/j.img raw" 1; sleep 3; tecla spc
if espera n5 "¿Quiere que al arrancar salga el menú de las aplicaciones? (S/N)" 90; then ok "copia los Juegos"; else ko "INSTALAR no acaba de copiar los Juegos"; fi
tecla s
if espera n6 "Hispa-8086 está instalado en el disco duro." 30; then ok "y acaba diciéndolo"; else ko "INSTALAR no acaba"; fi
apaga
duro_part $T/hd.img
if fsck.fat -n $T/part.img > $T/fsck.txt 2>&1 && cmp -s <(mtype -i $T/part.img ::OFICINA/PLUMA.COM) <(mtype -i $T/o.img ::PLUMA.COM) \
   && cmp -s <(mtype -i $T/part.img ::JUEGOS/MARCIANA.COM) <(mtype -i $T/j.img ::MARCIANA.COM) && cmp -s <(mtype -i $T/part.img ::MENU.COM) build/MENU.COM; then
  ok "desde fuera: C:, sin errores, con el sistema, C:\\OFICINA y C:\\JUEGOS"; else ko "C: no ha quedado como se esperaba"; cat $T/fsck.txt; mdir -/ -i $T/part.img :: | head -40; fi
if mtype -i $T/part.img ::AUTOEXEC.BAT | grep -q '^TECLADO' && mtype -i $T/part.img ::AUTOEXEC.BAT | grep -q '^MENU'; then ok "C:\\AUTOEXEC.BAT pone el teclado español y el menú"; else ko "C:\\AUTOEXEC.BAT: $(mtype -i $T/part.img ::AUTOEXEC.BAT | tr '\r\n' '  ')"; fi
cp $T/hd.img $T/hd_instalado.img
# arrancar de C:: el menú, con lo que hay instalado
arranca_duro $T/hd.img 486
if espera m0 "Hispa-8086: las aplicaciones" 30; then ok "arranca de C: y sale el menú"; else ko "al arrancar de C: no sale el menú"; fi
esta m0 "Pluma, el procesador de textos" "el menú tiene lo instalado: "
esta m0 "Marcianada (juego)" ""
esta m0 "7  Volver a la línea de órdenes" ""
tecla 2
if espera m1 "octetos libres" 15; then ok "con el 2, BASIC"; else ko "el menú no lanza BASIC"; fi
escribe "system"
if espera m2 "Hispa-8086: las aplicaciones" 15; then ok "al salir de BASIC, vuelve el menú"; else ko "al salir del programa no vuelve el menú"; fi
tecla down ret
if espera m3 "Pluma  (sin nombre)" 15; then ok "con las flechas (desde BASIC, una abajo) e Intro, PLUMA (desde C:\\OFICINA)"; else ko "el menú no lanza PLUMA"; fi
tecla alt-x; sleep 1
if espera m4 "Hispa-8086: las aplicaciones" 15; then ok "y al salir de PLUMA, otra vez el menú"; else ko "tras PLUMA no vuelve el menú"; fi
tecla 7; sleep 1
escribe "limpiar"; tecla_ene
pantalla m5
renglon m5 "ñ;" "«Volver»: a la línea de órdenes, y en C: el teclado es el español: "
escribe "dir"
pantalla m6
esta m6 'C:\>' "en C:\\: "
apaga
}

p_instalar2() {
echo "== Hispa-8086: INSTALAR en un disco que ya tiene C:, sin Oficina, sin Juegos y sin menú =="
[ -f $T/hd_instalado.img ] || { ko "hace falta antes la parte instalar"; return; }
cp $IMG_REAL/hispa8086-360.img $T/a.img
cp $T/hd_instalado.img $T/hd.img
arranca $T/a.img 486 $T/hd.img
escribe "limpiar"; escribe "instalar"; sleep 2
pantalla p0
esta p0 "El disco duro ya tiene la unidad C:. ¿Darle formato antes de instalar?" "con C: ya hecha, pregunta si formatear: "
tecla s
if espera p1 "¿Instalar también la Oficina (Pluma y Cuentas)? (S/N)" 150; then ok "formatea y copia"; else ko "INSTALAR no llega a la Oficina"; fi
tecla n; sleep 1; tecla n; sleep 1; tecla n
if espera p2 "Hispa-8086 está instalado en el disco duro." 30; then ok "sin Oficina, sin Juegos y sin menú, acaba"; else ko "INSTALAR no acaba"; fi
apaga
duro_part $T/hd.img
if fsck.fat -n $T/part.img > $T/fsck.txt 2>&1 && ! mdir -i $T/part.img ::OFICINA > /dev/null 2>&1 && ! mdir -i $T/part.img ::JUEGOS > /dev/null 2>&1 && ! mtype -i $T/part.img ::AUTOEXEC.BAT | grep -q '^MENU'; then ok "desde fuera: C: formateada otra vez, sin Oficina, Juegos ni menú"; else ko "C: no ha quedado como se esperaba"; mdir -i $T/part.img ::; fi
arranca_duro $T/hd.img 486
pantalla q0
esta q0 "Teclado español." "arranca de C: con el teclado español: "
esta q0 'C:\>' "y sin menú, en la línea de órdenes: "
apaga
}
