#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# pintura/pruebas/ventana.sh PINTURA CARPETA
# Pintura 2 en el anfitrión (la ventana de mentira de comun/so_anfitrion.c,
# a 1024 x 600 como el HP Mini), con ASan y UBSan: cada herramienta con un
# guion de sucesos, y lo guardado (PNG) comparado punto a punto con las
# imágenes esperadas (pruebas/esperadas; ESPERADAS=1 las vuelve a hacer,
# para mirarlas a ojo antes de subirlas). Además: deshacer y rehacer, lo
# elegido (mover, copiar, pegar, a mano alzada, transparente), el texto, las
# operaciones de Imagen y Colores, el editor de colores y el cuentagotas
# (por el nombre del color), guardar en cada formato desde el cuadro, el
# PDF, el fondo de escritorio y un lienzo de 4096 x 4096. Al final, «== N
# bien, M mal ==».
PIN=${1:?uso: ventana.sh PINTURA CARPETA}
D=${2:?uso: ventana.sh PINTURA CARPETA}
AQUI=$(dirname "$(readlink -f "$0")")
ESP=$AQUI/esperadas
rm -rf "$D"
mkdir -p "$D/cap"
bien=0
mal=0
ok() { echo "bien: $*"; bien=$((bien + 1)); }
ko() { echo "mal: $*"; mal=$((mal + 1)); }

# (las posiciones: la ventana de 1024 x 600; un lienzo de 320 x 200 cae en
# (387, 193); la paleta: el color i en (262 + (i % 14) * 22, 536 + (i / 14) * 22);
# las listas de la barra de opciones empiezan en x = 285, y = 44)
G=$D/guion.txt
p() { echo "$*" >> $G; }
nueva() { p "tecla ctrl-u"; p "escribe ${1:-320}"; p "tecla tab"; p "escribe ${2:-200}"; p "tecla ret"; }
guarda() { p "tecla f12"; p "escribe $D/$1"; p "tecla ret"; }
color() { p "clic $((262 + ($1 % 14) * 22)) $((536 + ($1 / 14) * 22))"; }
color2() { p "clicd $((262 + ($1 % 14) * 22)) $((536 + ($1 / 14) * 22))"; }
lista() { p "clic $1 44"; p "elige $2"; }
NEGRO=0 ROJO=4 AMARILLO=6 VERDE=8 AZUL=10 MORADO=12 BLANCO=14 ROSA=17 CELESTE=24
: > $G
# 1. el lápiz
nueva; color $ROJO; p "tecla l"; p "arrastra 397 203 597 303"; p "traza 397 383 447 213 497 383"
color2 $AZUL; p "arrastra 600 210 690 380 der"; guarda t_lapiz.png
# 2. el pincel redondo (8 px), con el izquierdo y con el derecho
nueva; color $AZUL; color2 $VERDE; p "tecla p"; lista 500 5; p "traza 407 213 487 293 567 223 667 373"
p "arrastra 420 380 680 230 der pasos 20"; guarda t_pincel.png
# 3. el pincel cuadrado sin suavizar, y la barra inclinada
nueva; color $ROJO; p "tecla p"; lista 360 1; lista 628 1; p "traza 407 223 507 233 567 333"
lista 360 2; lista 628 0; color $MORADO; p "traza 597 213 687 313"; guarda t_pincel2.png
# 4. el aerógrafo (rocía siempre igual: su azar empieza igual)
nueva; color $MORADO; p "tecla a"; lista 475 2; p "arrastra 420 250 650 300 pasos 10"; guarda t_aero.png
# 5. el borrador, que deja transparente (y el que pinta con el color 2)
nueva; color $NEGRO; p "tecla q"; lista 530 1; p "arrastra 397 203 697 383"
p "tecla b"; lista 505 1; p "arrastra 407 293 687 293 pasos 30"
lista 505 0; color2 $AMARILLO; p "arrastra 547 213 547 373 pasos 20"; guarda t_borra.png
# 6. el relleno (dentro de una elipse y fuera, con tolerancia)
nueva; color $NEGRO; p "tecla e"; lista 530 0; lista 345 2; p "arrastra 427 223 647 363"
p "tecla r"; color $ROJO; p "clic 537 293"; color $CELESTE; p "clic 392 198"; guarda t_relleno.png
# 7. las figuras: rectángulo con relleno, redondeado, círculo (Mayús), sin suavizar
nueva; color $NEGRO; color2 $AMARILLO; p "tecla q"; lista 530 2; lista 345 2; p "arrastra 397 203 487 273"
p "tecla o"; p "arrastra 507 203 627 273"; p "tecla e"; p "arrastra 397 293 467 343 mayus"
p "tecla q"; lista 530 0; lista 345 0; lista 718 1; color $ROJO; p "arrastra 497 293 697 383"; guarda t_figuras.png
# 8. la línea (a 45 grados con Mayús), la curva y el polígono (relleno y contorno)
nueva; color $AZUL; color2 $ROSA; p "tecla i"; lista 345 3; p "arrastra 397 203 697 263"; p "arrastra 397 383 457 300 mayus"
p "tecla u"; p "arrastra 487 373 687 373"; p "arrastra 547 373 547 283"; p "arrastra 640 373 650 280"
p "tecla g"; lista 530 2; p "clic 557 213"; p "clic 657 223"; p "clic 627 293"; p "clic 557 213"; guarda t_lineas.png
# 9. el texto (DejaVu Sans de 24 en negrita; y otra caja)
nueva; color $AZUL; p "tecla t"; p "clic 527 44"; p "clic 397 213"; p "escribe Hola, Pintura 2"; p "tecla ret"
p "escribe ñandú y 3 €"; p "clic 690 380"; p "clic 527 44"; color $ROJO; p "clic 427 313"; p "escribe ¿Qué tal?"
p "clic 690 380"; guarda t_texto.png
# 10. lo elegido: mover (deja el color 2), copiar con Control, pegar, a mano alzada, transparente
nueva; color $AZUL; color2 $BLANCO; p "tecla q"; lista 530 1; p "arrastra 407 213 477 263"; color $ROJO; p "tecla e"; p "arrastra 427 223 457 253"
p "tecla s"; p "arrastra 402 208 482 268"; p "arrastra 440 240 540 240"; p "arrastra 540 240 540 300 ctrl"
p "tecla esc"; p "tecla ctrl-e"; p "tecla ctrl-c"; p "tecla esc"
p "tecla m"; p "traza 600 210 690 220 680 280 610 270"; p "arrastra 640 245 640 330"
p "tecla s"; lista 385 1; p "arrastra 482 268 562 328"; p "arrastra 500 300 650 230"; p "tecla esc"; guarda t_seleccion.png
# 11. deshacer y rehacer (dos pasos atrás y uno adelante)
nueva; color $VERDE; p "tecla q"; lista 530 1; p "arrastra 407 213 457 263"; p "arrastra 487 213 537 263"; p "arrastra 567 213 617 263"
p "tecla ctrl-z"; p "tecla ctrl-z"; p "tecla ctrl-y"; guarda t_deshacer.png
# 12. Imagen y Colores: girar 90, voltear, invertir, grises, tamaño, girar 30, inclinar, brillo
nueva; color $ROJO; color2 $AMARILLO; p "tecla q"; lista 530 2; p "arrastra 397 203 517 253"; color $AZUL; p "tecla e"; p "arrastra 527 283 697 383"
guarda t_ops0.png
p "tecla ctrl-r"; guarda t_gira90.png
p "tecla alt-i"; p "elige 7"; guarda t_voltea.png
p "tecla ctrl-n"; guarda t_invierte.png
p "tecla alt-c"; p "elige 4"; guarda t_grises.png
p "tecla ctrl-z"; p "tecla ctrl-z"; p "tecla ctrl-z"; p "tecla ctrl-z"; guarda t_ops_deshecho.png
p "tecla ctrl-w"; p "tecla tab"; p "tecla tab"; p "escribe 50"; p "tecla ret"; guarda t_tamano.png
p "tecla ctrl-z"; p "tecla alt-i"; p "elige 6"; p "escribe 30"; p "tecla ret"; guarda t_gira30.png
p "tecla ctrl-z"; p "tecla alt-i"; p "elige 1"; p "tecla tab"; p "tecla tab"; p "escribe 20"; p "tecla ret"; guarda t_inclina.png
p "tecla ctrl-z"; p "tecla alt-c"; p "elige 5"; p "escribe 40"; p "tecla tab"; p "escribe 50"; p "tecla ret"; guarda t_brillo.png
# 13. el editor de colores (el código) y el cuentagotas, por su nombre
p "tecla alt-c"; p "elige 0"; p "tecla tab tab tab tab tab tab tab"; p "escribe #808000"; p "tecla ret"
p "tecla q"; lista 530 1; p "arrastra 600 330 690 380"; p "tecla c"; p "clic 650 360"
p "captura v1"; guarda c.png
# 14. guardar en cada formato desde el cuadro (por la extensión escrita), JPG con su calidad
nueva; color $ROJO; p "tecla p"; p "traza 407 213 687 383"; guarda f.bmp; guarda f.gif; guarda f.pcx
p "tecla f12"; p "escribe $D/f.jpg"; p "tecla ret"; p "tecla ret"
# 15. exportar a PDF, el fondo de escritorio, Propiedades
p "tecla alt-a"; p "elige 5"; p "escribe $D/f.pdf"; p "tecla ret"
p "tecla alt-a"; p "elige 8"
p "tecla alt-a"; p "elige 9"; p "captura v2"; p "tecla ret"
# 15 bis. (PINTURA32) el asa de la esquina de abajo a la derecha del lienzo
# (en (708, 394), de 7 x 7): el ratón encima (se resalta, la flecha de
# cambiar el tamaño y el aviso), arrastrarla (la medida en la barra de
# estado), lo añadido con el color 2; deshacer; y con el borrador en «Deja
# transparente», lo añadido transparente
nueva; color2 $AMARILLO; p "mueve 711 397"; p "captura v4"; p "arrastra 711 397 811 447 pasos 10"; guarda t_lienzo.png
p "tecla ctrl-z"; guarda t_lienzo2.png
p "tecla b"; lista 505 1; p "arrastra 711 397 761 397 pasos 5"; guarda t_lienzo3.png
# 16. abrir lo guardado (el GIF) y un lienzo de 4096 x 4096
p "tecla ctrl-a"; p "tecla tab"; p "escribe $D/f.gif"; p "tecla ret"
nueva 4096 4096; p "tecla p"; p "traza 450 250 650 350"; p "captura v3"
p "cerrar"; p "tecla tab"; p "tecla ret"

OFI_GUION=$G OFI_CAPTURAS=$D/cap OFI_VENTANA=1024x600 OFI_FECHA_FIJA=1 timeout 600 "$PIN" > $D/ventana.txt 2>&1
S=$D/ventana.txt
if grep -q "runtime error\|Sanitizer\|LeakSanitizer" $S; then ko "ASan o UBSan se quejan:"; grep -a "runtime error\|Sanitizer\|SUMMARY" $S | head -5
else ok "la ventana entera con su guion, sin una queja de ASan ni de UBSan"; fi
grep -q "pintura: adiós" $S && ok "se cierra (preguntando si se guardan los cambios)" || ko "no se cierra: $(tail -3 $S)"

# lo guardado, contra lo esperado
mkdir -p $ESP
for f in lapiz pincel pincel2 aero borra relleno figuras lineas texto seleccion deshacer ops0 gira90 voltea invierte grises ops_deshecho tamano gira30 inclina brillo; do
	if [ ! -f $D/t_$f.png ]; then ko "$f: no se ha guardado t_$f.png"; continue; fi
	if [ -n "${ESPERADAS:-}" ]; then cp $D/t_$f.png $ESP/$f.png; fi
	if [ ! -f $ESP/$f.png ]; then ko "$f: no hay imagen esperada ($ESP/$f.png)"; continue; fi
	r=$("$PIN" --compara $D/t_$f.png $ESP/$f.png 2>&1)
	case "$r" in *iguales*) ok "$f: igual punto a punto que la esperada (${r#*iguales })" ;; *) ko "$f: $r" ;; esac
done
# lo que no depende de una imagen esperada
i=$("$PIN" --info $D/t_borra.png 2>&1); case "$i" in *"con transparencia"*) ok "el borrador deja transparente (el PNG lleva alfa)" ;; *) ko "borrador: $i" ;; esac
i=$("$PIN" --info $D/t_gira90.png 2>&1); case "$i" in *"200 x 320"*) ok "girar 90°: de 320 x 200 a 200 x 320" ;; *) ko "girar 90: $i" ;; esac
i=$("$PIN" --info $D/t_tamano.png 2>&1); case "$i" in *"160 x 100"*) ok "cambiar el tamaño al 50 %: 160 x 100" ;; *) ko "tamaño: $i" ;; esac
i=$("$PIN" --info $D/t_grises.png 2>&1); r=$("$PIN" --compara $D/t_ops_deshecho.png $D/t_ops0.png 2>&1)
case "$r" in *iguales*) ok "deshacer cuatro operaciones de la imagen entera la deja como estaba" ;; *) ko "deshacer las operaciones: $r" ;; esac
grep -q "pintura: relleno de [1-9][0-9]* puntos" $S && ok "el relleno dice cuántos puntos: $(grep -o 'relleno de [0-9]* puntos' $S | head -1)" || ko "el relleno no rellena"
grep -q "pintura: texto puesto (27 letras)" $S && ok "el texto (con ñ, ú, € y dos líneas) queda en la imagen" || ko "el texto: $(grep 'texto puesto' $S)"
grep -q "deshecho «Rectángulo»" $S && grep -q "rehecho «Rectángulo»" $S && ok "deshacer y rehacer dicen qué" || ko "deshacer/rehacer no lo dicen"
grep -q "pintura: elegido a mano alzada" $S && grep -q "pintura: copiado" $S && ok "lo elegido: a mano alzada, mover y copiar" || ko "lo elegido"
grep -q "pintura: color 1: Verde oliva (#808000)" $S && ok "el editor de colores: el código #808000 es «Verde oliva»" || ko "el editor de colores: $(grep 'color 1' $S | tail -1)"
grep -q "Cuentagotas, color 1: Verde oliva" $S && ok "el cuentagotas dice el nombre del color y su código: $(grep -o 'Cuentagotas, color 1: [^|]*' $S | head -1)" || ko "el cuentagotas: $(grep Cuentagotas $S | head -1)"
for f in bmp gif pcx jpg; do
	[ -f $D/f.$f ] || { ko "no se ha guardado f.$f desde el cuadro"; continue; }
	i=$("$PIN" --info $D/f.$f 2>&1)
	case "$i" in *"$(echo $f | tr a-z A-Z), 320 x 200"*) ok "guardado desde el cuadro en ${f^^}: $i" ;; *) ko "f.$f: $i" ;; esac
done
r=$("$PIN" --compara $D/f.bmp $D/f.pcx 2>&1); case "$r" in *iguales*) ok "lo guardado en BMP y en PCX es lo mismo" ;; *) ko "BMP y PCX: $r" ;; esac
grep -q "pintura: guardado f.jpg (JPG" $S && grep -q "cuadro «Calidad del JPG»" $S && ok "el JPG pregunta su calidad" || ko "la calidad del JPG"
if [ -f $D/f.pdf ]; then
	if command -v qpdf > /dev/null; then
		qpdf --check $D/f.pdf > $D/qpdf.txt 2>&1 && ok "el PDF exportado: qpdf no ve errores" || ko "el PDF: $(tail -2 $D/qpdf.txt)"
	else echo "aviso: sin qpdf, el PDF no se mira por dentro"; fi
	if command -v pdfimages > /dev/null; then
		l=$(pdfimages -list $D/f.pdf 2> /dev/null | awk 'NR>2 {print $4"x"$5}')
		[ "$l" = "320x200" ] && ok "el PDF lleva la imagen entera (320 x 200)" || ko "el PDF lleva «$l»"
	fi
else ko "no está el PDF exportado"; fi
if grep -q "fondo: 1024x700" $S && [ -f $D/cap/fondo.ppm ] && grep -q "de fondo de escritorio, 1024 x 700: bien" $S; then ok "usar como fondo de escritorio: la imagen a la medida de la pantalla (1024 x 700)"
else ko "el fondo de escritorio: $(grep -i fondo $S | head -2)"; fi
# (PINTURA32) el asa de la esquina del lienzo
grep -q "pintura: aviso: Arrastre para cambiar el tamaño del lienzo (ahora, 320 x 200 píxeles)" $S && ok "el ratón sobre el asa de la esquina: la barra de estado dice «Arrastre para cambiar el tamaño del lienzo»" || ko "el aviso del asa: $(grep -a 'aviso: Arrastre' $S | head -1)"
if command -v convert > /dev/null && [ -f $D/cap/v4.ppm ]; then
	a=$(convert $D/cap/v4.ppm -format '%[hex:p{711,397}]' info: 2> /dev/null); f=$(convert $D/cap/v4.ppm -format '%[hex:p{725,411}]' info: 2> /dev/null)
	case "$a/$f" in 2F6FD0*/000000*) ok "con el ratón encima, el asa se resalta (azul) y sale la flecha de cambiar el tamaño junto a la del ratón" ;; *) ko "el asa resaltada y la flecha: $a $f" ;; esac
fi
n=$(grep -c "pintura: aviso: Tamaño del lienzo: [0-9]* x [0-9]* píxeles (suelte para aplicarlo)" $S)
[ "$n" -ge 10 ] && ok "mientras se arrastra, la barra de estado dice la medida ($n veces; la última, $(grep -o 'Tamaño del lienzo: [0-9]* x [0-9]*' $S | sed -n 11p))" || ko "la medida al arrastrar: $n"
i=$("$PIN" --info $D/t_lienzo.png 2>&1); case "$i" in *"424 x 254"*) ok "al soltar el asa se aplica: el lienzo, de 320 x 200 a 424 x 254" ;; *) ko "el lienzo arrastrado: $i" ;; esac
if command -v convert > /dev/null; then
	a=$(convert $D/t_lienzo.png -format '%[hex:p{400,240}]' info: 2> /dev/null)
	case "$a" in FFD800*) ok "lo añadido, con el color 2 (el amarillo de la paleta: $a)" ;; *) ko "lo añadido no es el color 2: $a" ;; esac
fi
i=$("$PIN" --info $D/t_lienzo2.png 2>&1); case "$i" in *"320 x 200"*) ok "y se deshace (otra vez 320 x 200)" ;; *) ko "deshacer el lienzo: $i" ;; esac
i=$("$PIN" --info $D/t_lienzo3.png 2>&1); case "$i" in *"374 x 204"*"con transparencia"*) ok "con el borrador en «Deja transparente», lo añadido es transparente ($i)" ;; *) ko "el lienzo transparente: $i" ;; esac
grep -q "cuadro «Propiedades de la imagen»" $S && ok "Propiedades de la imagen" || ko "Propiedades"
grep -q "pintura: abierto f.gif (GIF, 320 x 200)" $S && ok "abrir lo guardado (f.gif)" || ko "abrir f.gif"
grep -q "pintura: imagen nueva de 4096 x 4096" $S && ok "un lienzo de 4096 x 4096 (64 MB) y pintar en él" || ko "4096 x 4096: $(grep 'nueva\|memoria' $S | tail -2)"
# sin memoria para un lienzo tan grande: lo dice
cat > $D/g2.txt <<EOF
tecla ctrl-u
escribe 4096
tecla tab
escribe 4096
tecla ret
captura sinmem
tecla ret
cerrar
EOF
OFI_GUION=$D/g2.txt OFI_CAPTURAS=$D/cap OFI_VENTANA=1024x600 OFI_MEMORIA=48 timeout 120 "$PIN" > $D/sinmem.txt 2>&1
grep -q "No hay memoria para una imagen de 4096 x 4096" $D/sinmem.txt && ! grep -q "runtime error\|Sanitizer" $D/sinmem.txt && ok "sin memoria (48 MB) para 4096 x 4096: lo dice y sigue" || ko "sin memoria: $(grep aviso $D/sinmem.txt | tail -2)"
echo "== $bien bien, $mal mal =="
