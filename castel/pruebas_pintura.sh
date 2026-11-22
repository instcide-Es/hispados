# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# pruebas_pintura.sh - la parte 45 (SOLO=pintura, PINTURA32) de pruebas.sh,
# que lee este archivo («.»): la función parte_pintura, Pintura 2
# (PINTURA.BIN, el programa de dibujo de 32 bits en C portable:
# castel/nucleo32/oficina/pintura), con los ayudantes de pruebas.sh (ok, ko,
# saca) y sus carpetas ($AQUI, $P, $G).
#   1. en el anfitrión, con ASan y UBSan: los formatos (pintura/pruebas/
#      formatos.sh: ida y vuelta de PNG, BMP, PCX y GIF idénticos y JPG con
#      su PSNR, lo de ImageMagick leído igual, el fuzz de los siete lectores
#      con FUZZ_PINTURA vueltas, las medidas enormes) y la ventana con
#      guiones (pintura/pruebas/ventana.sh: cada herramienta comparada punto
#      a punto con su imagen esperada, deshacer, lo elegido, el texto, las
#      operaciones, los colores por su nombre, guardar en cada formato, el
#      PDF, el fondo, 4096 x 4096 y sin memoria);
#   2. en QEMU (el escritorio a 1024 x 600, como el HP Mini; la Consola en
#      (60, 186)): PINTURA, pintar con el pincel y una elipse, escribir un
#      texto, guardar DIBUJO.PNG, «Usar como fondo de escritorio» (la
#      llamada 172: FONDO.HPM), cerrar y volver a abrir DIBUJO.PNG; lo que
#      deja en el disco, mirado desde fuera;
#   3. lo que tarda el pincel en un Atom (QEMU -cpu n270 -icount shift=1:
#      unos 500 millones de instrucciones por segundo, menos que un N455) con
#      un lienzo de 1600 x 1200 en la pantalla de 1024 x 600
#      (PINTURA --mide 1600 1200 200).
# Con PINTURA_CAPTURAS=carpeta, las capturas (en .png) van también allí
# (las de docs/pentium/pintura/).

parte_pintura() {
echo "== Pintura 2 (PINTURA.BIN: el programa de dibujo de 32 bits, en C portable) =="
local V=$P/pintura PI=$AQUI/nucleo32/oficina/pintura S r t
local HPI=$V/pintura-anfitrion
mkdir -p $V/a $V/q $G
export MTOOLS_SKIP_CHECK=1
if ! bash $PI/anfitrion.sh $HPI asan > $V/anf.txt 2>&1 || ! bash $PI/hacer.sh $V/PINTURA.BIN > $V/hacer.txt 2>&1; then
  ko "no se puede compilar Pintura 2: $(tail -n 3 $V/anf.txt $V/hacer.txt)"
  return
fi
ok "PINTURA.BIN se compila (-march=i586, sin CMOV: $(stat -c %s $V/PINTURA.BIN) octetos) y Pintura 2 en el anfitrión, con ASan y UBSan"
# ---- 1. en el anfitrión ---- (PINTURA_SOLO_QEMU=1: sin esta, ni la 3)
if [ -z "${PINTURA_SOLO_QEMU:-}" ]; then
pi_partes() {     # el nombre, la orden; cuenta sus «bien»/«mal»
  local que=$1 sal=$V/$2.txt; shift 2
  "$@" > $sal 2>&1
  r=$(grep -a "bien, .* mal" $sal | tail -1)
  case "$r" in *" 0 mal"*) ok "$que: ${r#*== }" ;; *) ko "$que: ${r:-sin resumen}"; grep -a -i "^mal:\|runtime error\|Sanitizer" $sal | head -5 | sed 's/^/        /' ;; esac
}
pi_partes "anfitrión: los formatos (ida y vuelta, ImageMagick, el fuzz de PNG, JPG, BMP, GIF, PCX, ICO y TGA, medidas enormes)" formatos \
  env FUZZ=${FUZZ_PINTURA:-300} bash $PI/pruebas/formatos.sh $V/tf
pi_partes "anfitrión: la ventana con guiones (cada herramienta contra su imagen esperada, deshacer, selección, texto, Imagen, Colores, PDF, fondo)" ventana \
  bash $PI/pruebas/ventana.sh $HPI $V/tv
fi
# ---- 2. en QEMU ----
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/semilla.cas -o $V/semilla.asm > /dev/null && nasm -f bin $V/semilla.asm -o $V/SEMILLA.BIN || { ko "semilla.cas no compila"; return; }
bash $AQUI/nucleo32/disco.sh $V/v.img 32 > $V/disco.txt 2>&1 || { ko "disco.sh: $(tail -n 2 $V/disco.txt)"; return; }
mcopy -i $V/v.img $V/PINTURA.BIN ::
# (una copia para «Editar con Pintura», con una imagen y sin lo de después)
cp $V/v.img $V/e.img
[ -f $V/tv/t_figuras.png ] && mcopy -i $V/e.img $V/tv/t_figuras.png ::FIGURAS.PNG || mcopy -i $V/e.img $PI/pruebas/esperadas/figuras.png ::FIGURAS.PNG
CAPTURAS=$G ESCRIBE=1 DISCO=$V/v.img ARRANQUE="escritorio modo=1024x600" PANTALLA=1024x600 MODULOS= MEMORIA=128 $AQUI/nucleo32/probar.sh $V/SEMILLA.BIN ":pausa 3" ":doble 60 186" ":pausa 4" \
  ":escribe PINTURA" ":tecla ret" ":hasta 180 1 pintura: lista" ":pausa 3" ":captura pi1" \
  ":clic 354 497" ":arrastra 200 160 600 380" ":pausa 1" ":clic 486 497" ":tecla e" ":arrastra 650 150 900 400" ":pausa 1" \
  ":tecla t" ":clic 280 230" ":escribe Hispa-Pentium" ":clic 800 440" ":pausa 2" ":captura pi2" \
  ":tecla ctrl-g" ":pausa 2" ":escribe DIBUJO.PNG" ":tecla ret" ":hasta 60 1 pintura: guardado" ":pausa 1" \
  ":clic 40 41" ":pausa 1" ":captura pi3" ":clic 130 234" ":hasta 60 1 pintura: de fondo" ":pausa 2" \
  ":clic 1011 13" ":hasta 30 1 pintura: adiós" ":pausa 3" ":captura pi4" \
  ":escribe PINTURA DIBUJO.PNG" ":tecla ret" ":hasta 180 1 pintura: abierto" ":pausa 3" ":captura pi5" \
  ":clic 639 509" ":pausa 2" ":captura pi6" ":tecla esc" ":pausa 1" > $V/qemu.sal 2>&1
S=$V/qemu.sal
mira_pi() { if grep -a -q -- "$2" $S; then ok "$1"; else ko "$1: falta «$2»"; grep -a "pintura" $S | grep -v aviso | tail -6 | sed 's/^/        /'; fi; }
mira_pi "en QEMU: Pintura 2 abre su ventana en el escritorio de 1024 x 600" "pintura: lista"
mira_pi "en QEMU: el pincel y una elipse" "pintura: Elipse de"
mira_pi "en QEMU: un texto con la TrueType" "pintura: texto puesto (13 letras)"
mira_pi "en QEMU: guardado como DIBUJO.PNG" "pintura: guardado DIBUJO.PNG (PNG, 800 x 600"
mira_pi "en QEMU: «Usar como fondo de escritorio» (la llamada 172), a la medida de la pantalla" "pintura: de fondo de escritorio, 1024 x 600: bien"
mira_pi "en QEMU: cerrar y volver a abrir DIBUJO.PNG" "pintura: abierto DIBUJO.PNG (PNG, 800 x 600)"
mira_pi "en QEMU: el editor de colores" "cuadro «Editar colores»"
if ! grep -a -q "excepción\|\*\*\*" $S; then ok "en QEMU: ni una excepción ni una queja del núcleo"
else ko "el núcleo se ha quejado:"; grep -a "excepción\|\*\*\*" $S | head -4 | sed 's/^/        /'; fi
if saca $V/v.img DIBUJO.PNG $V/a/dibujo.png; then
  t=$($HPI --info $V/a/dibujo.png 2>&1)
  case "$t" in *"PNG, 800 x 600, opaca"*) ok "DIBUJO.PNG (hecho en QEMU), mirado desde fuera: $t" ;; *) ko "DIBUJO.PNG: «$t»" ;; esac
  # (el texto azul y el trazo rojo están en él: sus colores)
  if command -v convert > /dev/null; then
    n=$(convert $V/a/dibujo.png -format %c histogram:info:- 2> /dev/null | grep -ci "#E02020\|#2050E0")
    [ "$n" -ge 2 ] && ok "DIBUJO.PNG lleva el rojo y el azul de la paleta (#E02020 y #2050E0)" || ko "DIBUJO.PNG: los colores ($n)"
  fi
else ko "no está DIBUJO.PNG en el disco de QEMU"; fi
if saca $V/v.img FONDO.HPM $V/a/fondo.hpm; then
  t=$(python3 -I -c "import struct,sys; d=open(sys.argv[1],'rb').read(); m,a,b=struct.unpack('<4sII',d[:12]); print(m.decode(), a, b, len(d) == 12 + a * b * 4)" $V/a/fondo.hpm)
  [ "$t" = "HPMI 1024 600 True" ] && ok "FONDO.HPM (lo escribe el núcleo): la imagen de 1024 x 600 entera" || ko "FONDO.HPM: $t"
else ko "no está FONDO.HPM (el fondo de escritorio) en el disco"; fi
if [ -n "${PINTURA_CAPTURAS:-}" ]; then
  mkdir -p "$PINTURA_CAPTURAS"
  for c in pi1 pi2 pi3 pi4 pi5 pi6; do [ -f $G/$c.ppm ] && convert $G/$c.ppm "$PINTURA_CAPTURAS/$c.png" 2> /dev/null; done
  for c in v1 v2 v3; do [ -f $V/tv/cap/$c.ppm ] && convert $V/tv/cap/$c.ppm "$PINTURA_CAPTURAS/h$c.png" 2> /dev/null; done
fi
# «Editar con Pintura» en Mi equipo (el botón secundario en FIGURAS.PNG; la última opción)
CAPTURAS=$G DISCO=$V/e.img ARRANQUE="escritorio modo=1024x600" PANTALLA=1024x600 MODULOS= MEMORIA=128 $AQUI/nucleo32/probar.sh $V/SEMILLA.BIN ":pausa 3" \
  ":doble 64 30" ":pausa 5" ":doble 400 155" ":pausa 3" ":clicd 708 232" ":pausa 2" ":captura pi7" ":tecla up" ":tecla ret" \
  ":hasta 120 1 pintura: abierto" ":pausa 2" > $V/editar.sal 2>&1
grep -a -q "pintura: abierto FIGURAS.PNG (PNG, 320 x 200)" $V/editar.sal && ok "en QEMU: Mi equipo, «Editar con Pintura» en una imagen la abre en Pintura 2" \
  || ko "en QEMU: «Editar con Pintura»: $(grep -a pintura $V/editar.sal | tail -2)"
[ -n "${PINTURA_CAPTURAS:-}" ] && [ -f $G/pi7.ppm ] && convert $G/pi7.ppm "$PINTURA_CAPTURAS/pi7.png" 2> /dev/null
# ---- 3. lo que tarda en un Atom ----
[ -n "${PINTURA_SOLO_QEMU:-}" ] && return
QEMU_MAS="-cpu n270 -icount shift=1" ESCRIBE=1 DISCO=$V/v.img ARRANQUE="escritorio modo=1024x600" PANTALLA=1024x600 MODULOS= MEMORIA=256 \
  $AQUI/nucleo32/probar.sh $V/SEMILLA.BIN ":pausa 3" ":doble 60 186" ":pausa 4" \
  ":escribe PINTURA --mide 1600 1200 200" ":tecla ret" ":hasta 900 1 mide: memoria" ":pausa 2" > $V/mide.sal 2>&1
grep -a "pintura: mide" $V/mide.sal | sed 's/^/        /'
t=$(grep -a "ms por paso" $V/mide.sal | head -1)
ms=$(echo "$t" | sed -n 's/.*: \([0-9]*\) ms por paso.*/\1/p')
peor=$(echo "$t" | sed -n 's/.*el peor, \([0-9]*\) ms.*/\1/p')
if [ -n "$ms" ] && [ "$ms" -le 20 ] && [ "${peor:-999}" -le 60 ]; then ok "en un Atom (QEMU -cpu n270 -icount), lienzo de 1600 x 1200 a 1024 x 600: el pincel, $ms ms por paso (el peor, $peor ms)"
else ko "en un Atom: «$t»"; fi
}
