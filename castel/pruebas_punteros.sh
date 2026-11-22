# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# pruebas_punteros.sh - (PUNTEROS) la parte 51 de pruebas.sh (SOLO=punteros),
# que lee este archivo («.»): la función parte_punteros, con los ayudantes de
# pruebas.sh (ok, ko, mira, miracap) y sus carpetas ($AQUI, $P, $G).
#
# Los punteros del ratón (nucleo32/cursores.cas, ventanas.cas, apuntero.cas
# y el Panel de control > Ratón > Puntero, panelpun.cas), en QEMU. El
# puntero es lo que cambia entre dos capturas de la misma pantalla, una con
# él y otra con él lejos: nucleo32/mira_punteros.py saca de ahí su máscara y
# sus colores y los compara.
#
#   1. A 800 x 600, lo de siempre: la flecha de antes punto a punto; sobre el
#      texto del Bloc de notas, la «I» (una zona que pide el Bloc); en los
#      bordes de su ventana, las cuatro de cambiar el tamaño (las pone el
#      núcleo solo); al abrir un programa, la flecha con la espera.
#   2. A 1280 x 720, PUNTEST.BIN (upuntest.cas: 13 columnas, cada una con su
#      forma, arriba sobre claro y abajo sobre oscuro) con nueve PUNTERO.INI:
#      las 13 formas, distintas, a 1x y a 3x (3 veces más altas, nítidas y
#      no ampliadas a bloques); Moderna, Gruesa, Retro (sin suavizar,
#      invertido: el contrario del fondo), Mano (la del enlace, distinta) y
#      Mano en diagonal; los colores con su borde, visibles en claro y en
#      oscuro; para zurdos, la flecha y la mano en espejo y la «I» y la cruz
#      iguales; los anillos de Control y el rastro; y sin restos después de
#      moverlo deprisa por encima de las zonas y los bordes (punto a punto).
#   3. El Panel de control > Ratón > Puntero, con el teclado: los once
#      colores uno a uno (el puntero de verdad, de ese color, sobre claro y
#      sobre oscuro), los cuatro tamaños, para zurdos; se pone al momento,
#      se guarda en PUNTERO.INI y vale al arrancar otra vez.

parte_punteros() {
echo "== Hispa-Pentium: los punteros del ratón (formas, tamaños, colores, para zurdos, localizar, el rastro; el Panel > Ratón > Puntero) (PUNTEROS) =="
local X=$P/punteros MP="python3 -I $AQUI/nucleo32/mira_punteros.py" S k c n r f ord args
rm -rf $X; mkdir -p $X/c
export MTOOLS_SKIP_CHECK=1
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/semilla.cas -o $X/semilla.asm > /dev/null && nasm -f bin $X/semilla.asm -o $X/SEMILLA.BIN || { ko "semilla.cas no compila"; return; }
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/upuntest.cas -o $X/upuntest.asm > /dev/null && nasm -f bin $X/upuntest.asm -o $X/PUNTEST.BIN || { ko "upuntest.cas no compila"; return; }
if ! $AQUI/nucleo32/disco.sh $X/d.img 16 panel > $X/disco.txt 2>&1; then ko "no se puede hacer el disco: $(tail -2 $X/disco.txt)"; return; fi
mcopy -i $X/d.img $X/PUNTEST.BIN ::
ok "el núcleo con cursores.cas y la llamada 177, PANEL.BIN con «Puntero» y PUNTEST.BIN compilan"
G=$X/c
mp() {   # lo que se prueba y la orden de mira_punteros.py
  local que=$1; shift
  if $MP "$@" > $X/mp.txt 2>&1; then ok "$que ($(head -1 $X/mp.txt))"; else ko "$que: $(head -3 $X/mp.txt | tr '\n' ' ')"; fi
}
quejas() { if ! grep -aq "excepción\|\*\*\*" "$@"; then ok "$(basename $1 .sal) y las demás de la tanda: ni una excepción ni una queja del núcleo"; else ko "el núcleo se ha quejado:"; grep -ah "excepción\|\*\*\*" "$@" | head -3 | sed 's/^/        /'; fi; }

# ---- 1. a 800 x 600: la flecha de siempre, el Bloc, los bordes, la espera ----
# el Bloc es la primera ventana: en (88, 62), de 508 de ancho (sus bordes: x
# 88 y 595, y = 62 arriba)
CAPTURAS=$G DISCO=$X/d.img ARRANQUE=escritorio MODULOS= $AQUI/nucleo32/probar.sh $X/SEMILLA.BIN ":pausa 3" ":raton 400 300" ":captura a1" \
  ":doble 64 110" ":pausa 3" ":raton 300 250" ":pausa 1" ":captura a2" ":raton 700 520" ":pausa 1" ":captura a0" \
  ":raton 89 250" ":pausa 1" ":captura a3" ":raton 300 63" ":pausa 1" ":captura a4" ":raton 89 63" ":pausa 1" ":captura a5" ":raton 594 63" ":pausa 1" ":captura a6" \
  ":raton 594 250" ":pausa 1" ":raton 700 520" ":pausa 1" > $X/a.sal 2>&1
tr -d '\r' < $X/a.sal > $X/a.txt
S=$X/a.txt
miracap "la flecha de siempre (la «Clásica» a 1x), punto a punto donde está el ratón" a1 rgb 400 300 10233F rgb 402 304 FFFFFF rgb 401 312 FFFFFF nrgb 398 300 10233F
mira "al abrir el Bloc de notas, la flecha con la espera hasta que hace su ventana; luego, la flecha" "^puntero: forma 6 (la flecha con la espera)$" "^puntero: forma 0 (la flecha)$"
mira "sobre el texto del Bloc, la «I» (la zona que pide el Bloc: el núcleo la pone sin esperarle)" "^puntero: forma 7 (el texto)$"
mp "y se ve la «I» (lo que cambia con el ratón encima del texto)" forma $G/a2.ppm $G/a0.ppm 300 250 20 20
mira "en los bordes de la ventana, la flecha doble que toca (la pone el núcleo): de lado a lado, de arriba abajo y las dos diagonales" \
  "^puntero: forma 1 (cambiar el tamaño, de lado a lado)$" "^puntero: forma 2 (cambiar el tamaño, de arriba abajo)$" \
  "^puntero: forma 3 (cambiar el tamaño, en diagonal .)$" "^puntero: forma 4 (cambiar el tamaño, en diagonal /)$"
mp "la de lado a lado se ve (la de siempre)" forma $G/a3.ppm $G/a0.ppm 89 250 14 30
mp "y las otras tres son distintas entre sí y de ella" distintas 14 $G/a3.ppm $G/a0.ppm 89 250 $G/a4.ppm $G/a0.ppm 300 63 $G/a5.ppm $G/a0.ppm 89 63 $G/a6.ppm $G/a0.ppm 594 63
quejas $X/a.sal

# ---- 2. a 1280 x 720: PUNTEST.BIN con varios PUNTERO.INI ----
# la columna k: el punto activo en x = 144 + 80 k (la ventana, en (100, 10)); arriba (claro) y = 98,
# abajo (oscuro) y = 238. «sin»: el ratón lejos, en (1250, 600)
cx() { echo $((144 + 80 * $1)); }
sesion() {   # nombre, juego, tamaño, color, rastro, localizar, zurdo, las formas («todas») y lo que se teclea al final
  local nom=$1 fs=$8 ord k
  printf 'juego=%s\r\ntamano=%s\r\ncolor=%s\r\nrastro=%s\r\nlocalizar=%s\r\nzurdo=%s\r\n' $2 $3 $4 $5 $6 $7 > $X/PUNTERO.INI
  cp $X/d.img $X/$nom.img
  mcopy -o -i $X/$nom.img $X/PUNTERO.INI ::
  [ "$fs" = todas ] && fs="0 1 2 3 4 5 6 7 8 9 10 11 12"
  ord=()
  for k in $fs; do ord+=(":raton $(cx $k) 98" ":pausa 0.3" ":captura ${nom}_${k}c" ":raton $(cx $k) 238" ":pausa 0.3" ":captura ${nom}_${k}o"); done
  CAPTURAS=$G DISCO=$X/$nom.img ARRANQUE="escritorio modo=1280x720" PANTALLA=1280x720 MODULOS= $AQUI/nucleo32/probar.sh $X/SEMILLA.BIN ":pausa 3" \
    ":tecla meta_l-r" ":pausa 1" ":escribe \\PUNTEST.BIN" ":tecla ret" ":pausa 3" ":raton 1250 600" ":pausa 0.5" ":captura ${nom}_sin" "${ord[@]}" "${@:9}" > $X/$nom.sal 2>&1
  tr -d '\r' < $X/$nom.sal > $X/$nom.txt
}
sesion s1 0 0 0 0 0 0 todas ":raton 1250 600" ":pausa 0.5" ":captura s1_r0" \
  ":raton 150 60" ":raton 700 250" ":raton 103 200" ":raton 400 40" ":raton 1142 300" ":raton 200 336" ":raton 900 120" ":raton 500 250" ":raton 1250 600" ":pausa 1" ":captura s1_r1"
S=$X/s1.txt
mira "PUNTEST: 13 zonas en su ventana; una forma que no hay y una ventana que no es suya, no valen" "^puntest: una forma que no hay y una ventana que no es suya: no valen$" "^puntest: lista"
if grep -q "zona .* no vale" $S; then ko "PUNTEST: alguna zona no vale: $(grep 'no vale' $S | head -2)"; fi
mira "por encima de las 13 columnas, las 13 formas (la flecha, las cuatro de cambiar el tamaño, la espera, la flecha con la espera, el texto, la mano, mover, la cruz, prohibido y la ayuda)" \
  "^puntero: forma 5 (la espera)$" "^puntero: forma 8 (la mano)$" "^puntero: forma 9 (mover)$" "^puntero: forma 10 (la cruz)$" "^puntero: forma 11 (prohibido)$" "^puntero: forma 12 (la ayuda)$"
args=()
for k in 0 1 2 3 4 5 6 7 8 9 10 11 12; do args+=($G/s1_${k}c.ppm $G/s1_sin.ppm $(cx $k) 98); done
mp "Clásica a 1x: las 13, distintas entre sí (sobre claro)" distintas 34 "${args[@]}"
for k in 0 7 8; do mp "Clásica a 1x, la forma $k: se ve sobre claro y sobre oscuro" visible $G/s1_${k}o.ppm $G/s1_sin.ppm $(cx $k) 238 34; done
mp "Clásica a 1x: blanca con su borde de siempre" color $G/s1_0c.ppm $G/s1_sin.ppm $(cx 0) 98 30 FFFFFF
mp "sin restos: después de moverlo deprisa por las zonas y los bordes (cambiando de forma), la pantalla, igual punto a punto (salvo el reloj)" restos $G/s1_r0.ppm $G/s1_r1.ppm 1100 690 1280 720
sesion s2 0 3 0 0 0 0 todas ":raton 1250 600" ":pausa 0.5" ":captura s2_r0" \
  ":raton 150 60" ":raton 700 250" ":raton 103 200" ":raton 400 40" ":raton 1142 300" ":raton 200 336" ":raton 900 120" ":raton 500 250" ":raton 1250 600" ":pausa 1" ":captura s2_r1"
args=()
for k in 0 1 2 3 4 5 6 7 8 9 10 11 12; do args+=($G/s2_${k}c.ppm $G/s2_sin.ppm $(cx $k) 98); done
mp "Clásica a 3x (enorme): las 13, distintas entre sí" distintas 90 "${args[@]}"
mp "la flecha a 3x, tres veces más alta que a 1x" escala $G/s1_0c.ppm $G/s1_sin.ppm $(cx 0) 98 $G/s2_0c.ppm $G/s2_sin.ppm $(cx 0) 98 90 2.6 3.4
mp "la «I» a 3x, tres veces más alta" escala $G/s1_7c.ppm $G/s1_sin.ppm $(cx 7) 98 $G/s2_7c.ppm $G/s2_sin.ppm $(cx 7) 98 90 2.6 3.4
mp "la flecha a 3x, nítida: con medios tonos y no ampliada a bloques" nitido $G/s2_0c.ppm $G/s2_sin.ppm $(cx 0) 98 90 suave
mp "la mano a 3x, nítida" nitido $G/s2_8c.ppm $G/s2_sin.ppm $(cx 8) 98 90 suave
mp "sin restos con el puntero enorme (lo de debajo, repuesto entero)" restos $G/s2_r0.ppm $G/s2_r1.ppm 1100 690 1280 720
sesion s3 1 2 1 0 0 0 "0 5 7 8"
mp "Moderna a 2x, negra con borde blanco" color $G/s3_0c.ppm $G/s3_sin.ppm $(cx 0) 98 60 101010
mp "y sobre oscuro se ve (el borde blanco)" visible $G/s3_0o.ppm $G/s3_sin.ppm $(cx 0) 238 60
mp "Moderna: su flecha no es la Clásica" distintas 60 $G/s3_0c.ppm $G/s3_sin.ppm $(cx 0) 98 $G/s1_0c.ppm $G/s1_sin.ppm $(cx 0) 98
sesion s4 2 1 3 0 0 0 "0 7 8"
mp "Gruesa a 1,5x, amarilla con borde negro" color $G/s4_0c.ppm $G/s4_sin.ppm $(cx 0) 98 50 FFE600
mp "Gruesa a 1,5x: más alta que la Clásica a 1x" escala $G/s1_0c.ppm $G/s1_sin.ppm $(cx 0) 98 $G/s4_0c.ppm $G/s4_sin.ppm $(cx 0) 98 60 1.6 2.2
sesion s5 3 2 2 0 0 0 "0 7"
mp "Retro a 2x: sin suavizar (los puntos, enteros, como en 1990)" nitido $G/s5_0c.ppm $G/s5_sin.ppm $(cx 0) 98 60 duro
mp "invertido sobre claro: el relleno, oscuro" invertido $G/s5_0c.ppm $G/s5_sin.ppm $(cx 0) 98 60
mp "invertido sobre oscuro: el relleno, claro" invertido $G/s5_0o.ppm $G/s5_sin.ppm $(cx 0) 238 60
sesion s6 4 2 4 0 1 0 "0 8" ":raton 600 120" ":pausa 0.5" ":captura s6_l0" ":tecla ctrl" ":captura s6_l1" ":pausa 1"
S=$X/s6.txt
mp "Mano: la flecha es una mano con el dedo hacia arriba, verde lima con borde negro" color $G/s6_0c.ppm $G/s6_sin.ppm $(cx 0) 98 60 7CFC00
mp "y la del enlace es otra (con su destello): se nota al pasar por un enlace" distintas 60 $G/s6_0c.ppm $G/s6_sin.ppm $(cx 0) 98 $G/s6_8c.ppm $G/s6_sin.ppm $(cx 8) 98
mira "localizar: Control pulsado y suelto, solo" "^puntero: localizado (Control)$"
mp "los anillos que se cierran alrededor del puntero" anillos $G/s6_l1.ppm $G/s6_l0.ppm 600 120
sesion s7 5 3 5 1 0 0 "0 6 7 8 10 12" ":raton 1250 600" ":pausa 0.5" ":captura s7_r0" ":raton 900 300" ":raton 300 120" ":raton 1250 600" ":pausa 1" ":captura s7_r1"
mp "Mano en diagonal a 3x, roja con borde blanco" color $G/s7_0c.ppm $G/s7_sin.ppm $(cx 0) 98 90 E01B1B
mp "la mano en diagonal no es la de arriba" distintas 90 $G/s7_0c.ppm $G/s7_sin.ppm $(cx 0) 98 $G/s6_0c.ppm $G/s6_sin.ppm $(cx 0) 98
S=$X/s7.txt
mira "el rastro: al moverlo, copias más transparentes por donde ha pasado" "^puntero: rastro, con [1-9] copias$"
mp "y luego se van sin dejar restos (con el puntero enorme)" restos $G/s7_r0.ppm $G/s7_r1.ppm 1100 690 1280 720
sesion s8 5 3 5 0 0 1 "0 6 7 8 10 12"
mp "para zurdos: la mano en diagonal, en espejo (apunta arriba a la derecha)" espejo $G/s7_0c.ppm $G/s7_sin.ppm $(cx 0) 98 $G/s8_0c.ppm $G/s8_sin.ppm $(cx 0) 98 90
mp "y la mano del enlace, también" espejo $G/s7_8c.ppm $G/s7_sin.ppm $(cx 8) 98 $G/s8_8c.ppm $G/s8_sin.ppm $(cx 8) 98 90
mp "la «I» no cambia (es simétrica)" igual $G/s7_7c.ppm $G/s7_sin.ppm $(cx 7) 98 $G/s8_7c.ppm $G/s8_sin.ppm $(cx 7) 98 90
mp "ni la cruz" igual $G/s7_10c.ppm $G/s7_sin.ppm $(cx 10) 98 $G/s8_10c.ppm $G/s8_sin.ppm $(cx 10) 98 90
mp "la ayuda y la flecha con la espera, al otro lado (su «?» y su espera, sin espejo)" distintas 90 $G/s7_12c.ppm $G/s7_sin.ppm $(cx 12) 98 $G/s8_12c.ppm $G/s8_sin.ppm $(cx 12) 98
sesion s9 0 0 0 0 0 1 "0 1 7"
mp "para zurdos, la flecha Clásica de siempre en espejo" espejo $G/s1_0c.ppm $G/s1_sin.ppm $(cx 0) 98 $G/s9_0c.ppm $G/s9_sin.ppm $(cx 0) 98 30
mp "la de cambiar el tamaño no cambia" igual $G/s1_1c.ppm $G/s1_sin.ppm $(cx 1) 98 $G/s9_1c.ppm $G/s9_sin.ppm $(cx 1) 98 30
S=$X/s9.txt
mira "PUNTERO.INI se lee al arrancar" "^puntero: juego 0, tamaño 0, color 0, rastro 0, localizar 0, zurdo 1$"
quejas $X/s1.sal $X/s2.sal $X/s3.sal $X/s4.sal $X/s5.sal $X/s6.sal $X/s7.sal $X/s8.sal $X/s9.sal

# ---- 3. el Panel de control > Ratón > Puntero, con el teclado ----
# El Panel, de 784 x 560 en (8, 3): el foco empieza en «Forma»; Tab, al
# Tamaño; otra vez, al Color; las flechas cambian lo elegido sin abrir la
# lista. El puntero, en un claro (la columna de las secciones, en (60, 470))
# y en un oscuro (la vista previa, en (330, 470)); cada uno es el «sin» del
# otro (lo demás no cambia)
ord=()
for c in 1 2 3 4 5 6 7 8 9 10; do ord+=(":tecla down" ":pausa 0.6" ":raton 60 470" ":pausa 0.3" ":captura p${c}c" ":raton 330 470" ":pausa 0.3" ":captura p${c}o"); done
cp $X/d.img $X/p.img
CAPTURAS=$G ESCRIBE=1 DISCO=$X/p.img ARRANQUE=escritorio MODULOS= $AQUI/nucleo32/probar.sh $X/SEMILLA.BIN ":pausa 3" \
  ":tecla meta_l-r" ":pausa 1" ":escribe \\PANEL.BIN /PUNTERO" ":tecla ret" ":pausa 4" ":raton 60 470" ":pausa 0.3" ":captura p0c" ":raton 330 470" ":pausa 0.3" ":captura p0o" \
  ":tecla tab" ":tecla tab" "${ord[@]}" ":tecla shift-tab" ":tecla down" ":pausa 0.5" ":tecla down" ":pausa 0.5" ":tecla down" ":pausa 0.6" ":raton 60 470" ":pausa 0.3" ":captura pt3" \
  ":tecla tab" ":tecla tab" ":tecla tab" ":tecla tab" ":tecla spc" ":pausa 1" ":raton 790 590" ":pausa 0.3" ":captura pz" > $X/p.sal 2>&1
tr -d '\r' < $X/p.sal > $X/p.txt
S=$X/p.txt
mira "el Panel abre «Ratón > Puntero»" "^panel: Ratón > Puntero$"
mira "cada color, al momento (lo pone el núcleo: el Panel tiene su sello)" "^puntero: juego 0, tamaño 0, color 1, rastro 0, localizar 0, zurdo 0 (del Panel)$" \
  "^puntero: juego 0, tamaño 0, color 10, rastro 0, localizar 0, zurdo 0 (del Panel)$"
local col=(FFFFFF 101010 - FFE600 7CFC00 E01B1B 1E5AE6 FF8C00 FF5FC0 8A2BE2 00D8FF)
local nm=("Blanco" "Negro" "Invertido" "Amarillo" "Verde lima" "Rojo" "Azul" "Naranja" "Rosa" "Morado" "Cian")
for c in 0 1 2 3 4 5 6 7 8 9 10; do
  if [ $c = 2 ]; then
    mp "el Panel, color «Invertido»: sobre claro, oscuro" invertido $G/p2c.ppm $G/p2o.ppm 60 470 30
    mp "y sobre oscuro, claro" invertido $G/p2o.ppm $G/p2c.ppm 330 470 30
  else
    mp "el Panel, color «${nm[$c]}»: el puntero de ese color y con borde" color $G/p${c}c.ppm $G/p${c}o.ppm 60 470 30 ${col[$c]}
    mp "y se ve sobre oscuro" visible $G/p${c}o.ppm $G/p${c}c.ppm 330 470 30
  fi
done
mira "el tamaño, al momento" "^puntero: juego 0, tamaño 3, color 10, rastro 0, localizar 0, zurdo 0 (del Panel)$"
mp "el tamaño «Enorme (3x)»: el puntero, tres veces más alto" escala $G/p10c.ppm $G/p10o.ppm 60 470 $G/pt3.ppm $G/p10o.ppm 60 470 90 2.6 3.4
mira "«Para zurdos: en espejo», con el teclado" "^puntero: juego 0, tamaño 3, color 10, rastro 0, localizar 0, zurdo 1 (del Panel)$"
f=$X/PUNTERO.INI; rm -f $f; mcopy -n -i $X/p.img ::PUNTERO.INI $f > /dev/null 2>&1
if [ -f $f ] && grep -q "^juego=0" $f && grep -q "^tamano=3" $f && grep -q "^color=10" $f && grep -q "^zurdo=1" $f && fsck.fat -n $X/p.img > /dev/null 2>&1; then
  ok "lo elegido, guardado en PUNTERO.INI (lo escribe el núcleo), y el disco, sano"
else ko "PUNTERO.INI: $(tr -d '\r' < $f 2> /dev/null | tr '\n' ' ')"; fi
CAPTURAS=$G DISCO=$X/p.img ARRANQUE=escritorio MODULOS= $AQUI/nucleo32/probar.sh $X/SEMILLA.BIN ":pausa 3" > $X/q.sal 2>&1
tr -d '\r' < $X/q.sal > $X/q.txt
S=$X/q.txt
mira "y al arrancar otra vez, lo guardado" "^puntero: juego 0, tamaño 3, color 10, rastro 0, localizar 0, zurdo 1$"
quejas $X/p.sal $X/q.sal
[ -n "${CAPTURAS_A:-}" ] && { mkdir -p "$CAPTURAS_A"; cp $G/*.ppm "$CAPTURAS_A"/ 2> /dev/null; }
G=$P/graf
}
