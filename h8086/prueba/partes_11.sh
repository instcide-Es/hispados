# Las teclas de cursor (las carga prueba.sh): se pulsan las flechas, Inicio,
# Fin, Supr e Insert en todos los sitios donde tienen que servir (la línea de
# órdenes, BASIC, INKEY$ y EDITAR) y se mira el efecto. Con las teclas grises
# (el bloque aparte del teclado de 101 teclas, que llegan con el prefijo E0h)
# y con las del teclado numérico (sin BloqNum: las únicas del de 83 teclas);
# sin TECLADO, con TECLADO (INT 15h/4Fh) y con TECLADO /9 (INT 9).
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.

# las teclas de cada juego: L R (izquierda, derecha), U D (arriba, abajo),
# H E (Inicio, Fin), S (Supr), I (Insert)
juego() {
  if [ "$1" = grises ]; then L=left R=right U=up D=down H=home E=end S=delete I=insert
  else L=kp_4 R=kp_6 U=kp_8 D=kp_2 H=kp_7 E=kp_1 S=kp_decimal I=kp_0; fi
}
# hay en la pantalla $1 exactamente $3 renglones que son $2 (uno, si no se dice)
renglon() {
  local n=$(grep -cxF -- "$2" $T/$1.txt)
  if [ "$n" = "${4:-1}" ]; then ok "$3«$2»"; else ko "$3«$2» sale $n veces y no ${4:-1}"; fi
}

# ---- la línea de órdenes ($1: nombre para las pantallas, $2: lo que se dice) ----
flechas_orden() {
escribe "limpiar"
teclea "eco acd"; tecla $L $L b ret; sleep 0.4
teclea "co hola"; tecla $H e $E s ret; sleep 0.4
teclea "eco muxndo"; tecla $L $L $L $L $S ret; sleep 0.4
teclea "eco gaxto"; tecla $L $L backspace ret; sleep 0.4
teclea "eco pera"; tecla $L $L $L $I a ret; sleep 0.4
teclea "eco sl"; tecla $H $R $R $R $R $R a ret; sleep 0.6
pantalla $1a
renglon $1a "abcd" "$2: izquierda, y la letra se intercala: "
renglon $1a "holas" "$2: Inicio y Fin: "
renglon $1a "mundo" "$2: Supr borra la letra del cursor: "
renglon $1a "gato" "$2: Retroceso en medio de la línea: "
renglon $1a "para" "$2: Insert, y se escribe encima: "
renglon $1a "sal" "$2: derecha (y la línea siguiente vuelve a intercalar): "
renglon $1a 'A:\>eco sal' "$2: en la pantalla queda la línea corregida: "
escribe "limpiar"; escribe "eco uno"; escribe "eco dos"
tecla $U $U ret; sleep 0.6
tecla $U $U $U $D ret; sleep 0.6
teclea "eco mal"; tecla esc; escribe "eco bien"
teclea "eco cua"; tecla f3 ret; sleep 0.6
pantalla $1b
renglon $1b "uno" "$2: arriba dos veces trae la orden de antes de la última: " 2
renglon $1b "dos" "$2: arriba tres veces y abajo una: " 2
renglon $1b "bien" "$2: Esc borra lo tecleado: "
noesta $1b "mal" "$2: Esc: "
renglon $1b "cuan" "$2: F3 trae lo que falta de la última línea: "
}
# una línea más larga que la pantalla, con el indicador en el último renglón
# (al pasar de renglón la pantalla sube): el cursor va de un renglón a otro
flechas_larga() {
local p=1234567890abcdefghij1234567890klmnopqrst1234567890uvwxyzabcd1234567890efghij
local q="${p:0:1}${p:2}"      # (lo que queda: se le quita la segunda letra)
escribe "limpiar"; escribe "dir"; sleep 1
teclea "co $p"; tecla $H e $E z $H $R $R $R $R $R $S $E; sleep 0.5
pantalla $1l
renglon $1l "A:\\>eco ${q:0:72}" "$2: una línea de dos renglones, corregida en el primero desde el segundo: "
renglon $1l "${q:72}z" "$2: y su segundo renglón: "
tecla ret; sleep 0.8
pantalla $1m
renglon $1m "${q}z" "$2: la línea larga llega entera a la orden: "
}
# ---- BASIC: las líneas del programa, EDIT e INKEY$ ----
flechas_basic() {
escribe "limpiar"; escribe "basic"; sleep 2
teclea "10 print 125"; tecla $L; teclea "34"; tecla ret; sleep 0.4
escribe "20 print 77"
tecla $U $U $H $S $S; teclea "30"; tecla ret; sleep 0.4
escribe "edit 20"; tecla backspace 8 ret; sleep 0.4
escribe "list"; escribe "run"
pantalla $1c
renglon $1c "10 PRINT 12345" "$2: BASIC, izquierda en una línea del programa: "
renglon $1c "30 PRINT 12345" "$2: BASIC, arriba, Inicio y Supr (una línea hecha de otra): "
renglon $1c "20 PRINT 78" "$2: BASIC, EDIT deja la línea para corregirla: " 2
renglon $1c " 12345" "$2: BASIC, el programa corregido funciona: " 2
renglon $1c " 78" "$2: BASIC, y la línea de EDIT: "
escribe "system"; sleep 1; escribe "limpiar"; escribe "basic tecla.bas"; sleep 2
tecla $L $R $U $D $H $E $S $I a z; sleep 1.5
pantalla $1d
local c
for c in 75 77 72 80 71 79 83 82; do renglon $1d "EXT $c" "$2: INKEY\$ da las teclas de cursor en dos letras (0 y el código): "; done
renglon $1d "LETRA a" "$2: INKEY\$, y detrás las letras siguen bien: "
renglon $1d "LETRA z" "$2: INKEY\$: "
}
# ---- EDITAR ----
flechas_editar() {
escribe "limpiar"; escribe "editar f.txt"; sleep 1
tecla a b c ret d e f; tecla $U $H x $D $E y $L $L $S; sleep 0.5
pantalla $1e
renglon $1e "xabc" "$2: EDITAR, arriba e Inicio: "
renglon $1e "dey" "$2: EDITAR, abajo, Fin, izquierda y Supr: "
tecla $I; sleep 0.4
pantalla $1e
esta $1e "SOB" "$2: EDITAR, Insert: "
tecla f10; sleep 0.5; tecla n; sleep 1
}

p_flechas() {
echo "== Hispa-8086: las teclas de cursor (línea de órdenes, BASIC, INKEY\$ y EDITAR) =="
local modo j n=0 que
printf '10 A$=INKEY$:IF A$="" THEN 10\r\n20 IF LEN(A$)=2 THEN PRINT "EXT";ASC(MID$(A$,2)) ELSE PRINT "LETRA ";A$\r\n30 IF A$<>"z" THEN 10\r\n40 SYSTEM\r\n' > $T/TECLA.BAS
for modo in "" "teclado" "teclado /9"; do
  cp $IMG/hispa8086-360.img $T/a.img
  mcopy -o -i $T/a.img $T/TECLA.BAS ::
  arranca $T/a.img 486
  if [ -n "$modo" ]; then escribe "$modo"; sleep 1; fi
  for j in grises numerico; do
    juego $j; n=$((n + 1))
    que="${modo:-sin TECLADO}, "; [ $j = grises ] && que="${que}teclas grises" || que="${que}teclado numérico"
    flechas_orden f$n "$que"
    [ $n = 1 ] || [ $n = 4 ] && flechas_larga f$n "$que"
    [ $n = 1 ] || [ $n = 4 ] || [ $n = 5 ] && flechas_basic f$n "$que"
    flechas_editar f$n "$que"
  done
  escribe "limpiar"; escribe "ver"
  pantalla f${n}z
  esta f${n}z "Hispa-8086 versión" "${modo:-sin TECLADO}: y al acabar el sistema sigue en pie: "
  apaga
done
}

# Lo mismo en el emulador v86 (el de la web del proyecto), con las teclas de
# un teclado de ahora y TECLADO cargado, que es donde se vio que las flechas
# no hacían nada. Hace falta node, playwright-core, Chrome y una copia de v86
# y de la página; si falta algo, se dice y se salta. (Con V86_IMG=imagen se
# prueba otro disquete: el publicado, por ejemplo)
p_v86_flechas() {
echo "== Hispa-8086: las teclas de cursor en el emulador v86 (opcional) =="
local pw=${PLAYWRIGHT:-/opt/herramientas/node_modules/playwright-core} cr=${CHROME:-/usr/bin/google-chrome}
local v86=${V86:-/www/hispados-web/v86} pag=${V86_PAGINA:-/root/FTP/HispaDOS/web/index.html}
if ! command -v node > /dev/null || [ ! -d "$pw" ] || [ ! -x "$cr" ] || [ ! -f "$v86/libv86.js" ] || [ ! -f "$pag" ]; then
  echo "  (no hay node, playwright-core, Chrome o v86 en esta máquina: esta parte se salta)"; return
fi
mkdir -p $T/webf/v86
cp "$pag" $T/webf/index.html; cp $v86/libv86.js $v86/v86.wasm $v86/seabios.bin $v86/vgabios.bin $T/webf/v86/
cp ${V86_IMG:-$IMG/hispa8086-1440.img} $T/webf/v86/hispa8086.img
printf '@ECO OFF\r\nTECLADO\r\n' > $T/AUTOEXEC.BAT
mcopy -o -i $T/webf/v86/hispa8086.img $T/AUTOEXEC.BAT ::
(cd $T/webf && exec python3 -m http.server $((${V86_PUERTO:-8932} + 1)) --bind 127.0.0.1 > /dev/null 2>&1) &
local servidor=$!
sleep 1
timeout 240 node prueba/v86_flechas.js http://127.0.0.1:$((${V86_PUERTO:-8932} + 1))/ "$pw" "$cr" > $T/v86f.txt 2>&1
kill $servidor 2> /dev/null
local que r
for que in grises numerico; do
  for r in abcd holas mundo sal uno dos xabc dey; do
    if grep -qxF "SI $que: $r" $T/v86f.txt; then ok "v86, teclas ${que}: «$r»"; else ko "v86, teclas ${que}: no sale «$r»"; fi
  done
done
}
