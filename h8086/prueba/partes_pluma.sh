# Las partes de la prueba de PLUMA, el procesador de textos (las carga
# prueba.sh). Se copia PLUMA.COM al disquete del sistema y se maneja con el
# teclado, como lo haría una persona; lo que se guarda, se exporta y se
# imprime se mira luego desde fuera.
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.

# escribe un texto en PLUMA (sin Intro al final)
pl_texto() {
  local t="$1" i c
  for ((i=0; i<${#t}; i++)); do
    c="${t:$i:1}"
    case "$c" in ' ') tecla spc;; '.') tecla dot;; ',') tecla comma;; '-') tecla minus;; [A-Z]) tecla "shift-${c,,}";; *) tecla "$c";; esac
  done
}
# el atributo de la pantalla en la fila $1, columna $2
pl_atributo() {
  mon "pmemsave 0xb8000 4000 \"$T/scr.bin\"" 0.4
  python3 -c "import sys; d=open(sys.argv[1],'rb').read(); print('%02X %02X' % (d[(int(sys.argv[2])*80+int(sys.argv[3]))*2], d[(int(sys.argv[2])*80+int(sys.argv[3]))*2+1]))" $T/scr.bin $1 $2
}
# compara el archivo $1 del disquete con lo que escribe en Python $2
pl_igual() {
  mtype -i $T/a.img ::$1 > $T/pl_sal.bin 2>/dev/null
  if python3 - $T/pl_sal.bin "$2" <<'PY'
import sys
d = open(sys.argv[1], 'rb').read()
e = eval(sys.argv[2])
if d != e:
    print('    es:      ', d[:200])
    print('    esperado:', e[:200])
    sys.exit(1)
PY
  then ok "$3"; else ko "$3"; fi
}
# ¿está la secuencia de octetos (en Python) $2 en el archivo $1?
pl_tiene() {
  mtype -i $T/a.img ::$1 > $T/pl_sal.bin 2>/dev/null
  if python3 -c "import sys; sys.exit(0 if eval(sys.argv[2]) in open(sys.argv[1],'rb').read() else 1)" $T/pl_sal.bin "$2"
  then ok "$3"; else ko "$3"; fi
}

p_pluma() {
echo "== PLUMA: escribir, el formato, guardar, la ñ y las tildes, exportar e imprimir =="
cp $IMG/hispa8086-360.img $T/a.img
mcopy -i $T/a.img build/PLUMA.COM ::
arranca $T/a.img 486
escribe "teclado"; escribe "pluma carta"; sleep 2
pantalla u1
esta u1 "Pluma  CARTA.PLU" "arranca y pinta: la línea de estado: "
esta u1 "L---!----!----!" "la regla, como la de WordStar: "
esta u1 "Documento nuevo." ""
# un párrafo largo: las líneas se parten solas, por los espacios
pl_texto "En un lugar de la Mancha, de cuyo nombre no quiero acordarme, no ha mucho tiempo que vivia un hidalgo de los de lanza en astillero."
tecla ret
pl_texto "Uno "; tecla alt-n; pl_texto "dos"; tecla alt-n; pl_texto " tres "; tecla alt-s; pl_texto "sub"; tecla alt-s; pl_texto " "; tecla alt-c; pl_texto "cur"; tecla alt-c
tecla ret; sleep 1
pantalla u2
esta u2 "En un lugar de la Mancha, de cuyo nombre no quiero acordarme, no" "ajuste de línea: la primera, hasta 65 letras: "
esta u2 "ha mucho tiempo que vivia un hidalgo de los de lanza en" "y lo demás en la siguiente, sin pulsar Intro: "
if grep -q "^astillero\.  *<$" $T/u2.txt; then ok "el fin de párrafo, marcado con «<» a la derecha (como WordStar)"; else ko "falta la marca de fin de párrafo"; fi
esta u2 "Uno dos tres sub cur" "los códigos de formato no se ven ni ocupan sitio: "
a=$(pl_atributo 5 4); b=$(pl_atributo 5 0); c=$(pl_atributo 5 13); d=$(pl_atributo 5 17)
if [ "${a#* }" = 0F ] && [ "${b#* }" = 07 ]; then ok "negrita con brillo en la pantalla ($a frente a $b)"; else ko "negrita: $a, normal: $b"; fi
if [ "${c#* }" = 70 ] && [ "${d#* }" = 17 ]; then ok "subrayado (inverso en color) y cursiva (fondo oscuro), sin colores de por medio ($c, $d)"; else ko "subrayado $c, cursiva $d"; fi
# la ñ, las tildes (TECLADO), la Á con Alt+T y el euro con Alt+E
pl_texto "Ni"; tecla semicolon; pl_texto "o "; tecla apostrophe a; pl_texto " A"; tecla alt-t; tecla spc alt-e spc shift-1 alt-t spc shift-minus alt-t c alt-t
sleep 1
pantalla u3
esta u3 "Niño á" "la ñ y las tildes, en la pantalla: "
esta u3 "F1 Ayuda  F2 Guardar  F3 Buscar" "la línea de ayuda: "
mon "pmemsave 0xb8000 4000 \"$T/scr.bin\"" 0.4
if python3 -c "import sys; d=open(sys.argv[1],'rb').read()[6*160:7*160:2]; sys.exit(0 if b'Ni\xa4o \xa0 \xb5 \xd5 \xad \xa8\x87' in d else 1)" $T/scr.bin; then ok "Á y € (que la 437 no tiene) se ven con sus letras en VGA; ¡ ¿ y ç con Alt+T"; else ko "Á, €, ¡, ¿, ç en la pantalla"; fi
tecla f2; sleep 2
pantalla u4
esta u4 "Guardado." "F2 guarda: "
noesta u4 "CARTA.PLU*" "y ya no hay cambios: "
mtype -i $T/a.img ::CARTA.PLU > $T/carta.plu
if python3 - $T/carta.plu <<'PY'
import sys
d = open(sys.argv[1], 'rb').read()
cab, cuerpo = d[:128], d[128:]
assert cab[:8] == b'PLUMA86\x1a' and cab[9:13] == bytes([10, 65, 0, 66]), cab[:16]
assert cab[80:86] == b'- # -\x00'
e = (b'En un lugar de la Mancha, de cuyo nombre no quiero acordarme, no ha mucho tiempo que vivia un hidalgo de los de lanza en astillero.\r\n'
     b'Uno \x02dos\x02 tres \x13sub\x13 \x19cur\x19\r\n'
     b'Ni\xa4o \xa0 \xb5 \xd5 \xad \xa8\x87')
assert cuerpo == e, cuerpo
PY
then ok "el .PLU: la cabecera (márgenes, pie «- # -») y el texto en la 858, con CR LF y los códigos de WordStar"; else ko "el .PLU guardado no es el esperado"; fi
# exportar: texto de hoy (UTF-8), RTF y texto de DOS
tecla f10 a e h ret; sleep 2
tecla f10 a e r ret; sleep 2
tecla f10 a e t; for i in 1 2 3 4 5 6 7 8 9 10 11; do tecla backspace; done; pl_texto "carta.dos"; tecla ret; sleep 2
pl_igual CARTA.TXT "'En un lugar de la Mancha, de cuyo nombre no quiero acordarme, no ha mucho tiempo que vivia un hidalgo de los de lanza en astillero.\r\nUno dos tres sub cur\r\nNiño á Á € ¡ ¿ç'.encode('utf-8')" "texto de hoy: UTF-8, un párrafo por línea, con ñ, Á y €"
mtype -i $T/a.img ::CARTA.RTF > $T/carta.rtf
if python3 - $T/carta.rtf <<'PY2'
import sys
d = open(sys.argv[1], 'rb').read()
for t in [rb'{\rtf1\ansi\ansicpg1252', rb"Ni\'f1o \'e1 \'c1 \'80 \'a1 \'bf\'e7", rb'\b dos\b0 ',
          rb'\ul sub\ul0 ', rb'\i cur\i0 ', rb'\margl1440', rb'{\footer', rb'\fldinst PAGE']:
    if t not in d:
        print('    falta', t); sys.exit(1)
sys.exit(0 if d.rstrip().endswith(b'}') else 1)
PY2
then ok "RTF: página 1252 (ñ, á, Á y € como \\'hh), negrita, subrayado, cursiva, márgenes y el pie con su número"; else ko "el RTF no es el esperado"; fi
pl_igual CARTA.DOS "b'En un lugar de la Mancha, de cuyo nombre no quiero acordarme, no\r\nha mucho tiempo que vivia un hidalgo de los de lanza en\r\nastillero.\r\nUno dos tres sub cur\r\nNi\xa4o \xa0 A \xee \xad \xa8\x87\r\n'.replace(b'\xee', b'E')" "texto de DOS: las líneas como en la página, en la 437 (sin la Á ni el €)"
# imprimir en un archivo: Epson moderna (la tabla PC 437) y Epson FX (juego España I)
tecla f10 i i i m p dot p r n ret; sleep 3
tecla f10 p i f; tecla f10 i i i m p f x dot p r n ret; sleep 3
pl_tiene IMP.PRN "b'\x1b@\x1bt\x01\x1b6'" "Epson con la tabla PC 437: ESC @, ESC t 1, ESC 6"
pl_tiene IMP.PRN "b'          En un lugar de la Mancha'" "el margen izquierdo del papel (10): "
pl_tiene IMP.PRN "b'Uno \x1bEdos\x1bF tres \x1b-\x01sub\x1b-\x00 \x1b4cur\x1b5'" "negrita ESC E/F, subrayado ESC - 1/0, cursiva ESC 4/5"
pl_tiene IMP.PRN "b'Ni\xa4o \xa0 A\x08\x27 C\x08= \xad \xa8\x87'" "la ñ y la á tal cual; Á con retroceso y tilde; € como C y = encima"
pl_tiene IMP.PRN "b'- 1 -\r\n'" "el pie con el número de página: "
pl_tiene IMP.PRN "b'\x0c\x1b@'" "y al final, la hoja fuera (FF) y la impresora como estaba"
pl_tiene IMPFX.PRN "b'\x1b@\x1bR\x07'" "Epson FX: el juego de caracteres España I (ESC R 7)"
pl_tiene IMPFX.PRN "b'Ni|o a\x08\x27 A\x08\x27 C\x08= [ ]c\x08,'" "en la FX: ñ es |, ¡ [, ¿ ], y las tildes encima con retroceso"
tecla alt-x; sleep 1
apaga
arranca $T/a.img 486
escribe "pluma carta"; sleep 2
pantalla u5
esta u5 "Uno dos tres sub cur" "abrir lo guardado: "
noesta u5 "CARTA.PLU*" "recién abierto, sin cambios: "
a=$(pl_atributo 5 4)
if [ "${a#* }" = 0F ]; then ok "la negrita sigue en su sitio"; else ko "la negrita al abrir: $a"; fi
# más: deshacer, bloques, alineación, reemplazar, contar, la vista, la ayuda
tecla ctrl-end ret
pl_texto "Titulo"; tecla alt-m ret
pl_texto "Este parrafo va justificado para ver como se reparten los espacios entre las palabras de cada linea del texto."
tecla alt-j ret alt-i; sleep 1
pantalla u6
esta u6 "                             Titulo" "Alt+M: centrado en el ancho de la línea: "
esta u6 "Este  parrafo  va  justificado  para  ver  como  se  reparten los" "Alt+J: justificado, con los espacios repartidos: "
pl_texto "Linea para deshacer"; tecla ctrl-z; sleep 1
pantalla u7
noesta u7 "Linea para deshacer" "Ctrl+Z deshace lo escrito: "
pl_texto "Rojo verde rojo"; tecla home f7 end f4 f6; sleep 1
pantalla u8
esta u8 "Rojo verde rojoRojo verde rojo" "F7 marcar, F4 copiar, F6 pegar: "
tecla f8; pl_texto "rojo"; tecla ret; pl_texto "azul"; tecla ret t; sleep 2
pantalla u9
esta u9 "4 cambios." "F8 reemplazar (sin mirar mayúsculas), T todos: "
esta u9 "azul verde azulazul verde azul" ""
tecla f3; pl_texto "o a a"; tecla ret; sleep 1
pantalla u10
noesta u10 "No está." "F3 busca sin mirar tildes ni mayúsculas: «o a a» encuentra «o á Á»: "
esta u10 "Col 4 " ""
tecla f3; pl_texto "nino"; tecla ret; sleep 1
pantalla u10
esta u10 "No está." "pero la ñ es otra letra: «nino» no es «niño»: "
tecla f10 e l; sleep 2
pantalla u11
esta u11 "61 palabras, 256 letras, 6 párrafos y 1 página." "contar palabras, letras, párrafos y páginas: "
tecla spc f9; sleep 2
pantalla u12
esta u12 "vista de la página" "F9, la vista de la página: "
esta u12 "          En un lugar de la Mancha" "con el margen del papel: "
esta u12 "                                       Titulo" "el título, centrado: "
tecla pgdn up up up up up; sleep 2
pantalla u13
esta u13 "- 1 -" "AvPág: el pie de la página 1, con su número: "
esta u13 "fin de la página 1" "y la raya entre páginas: "
tecla esc f1; sleep 1
pantalla u14
esta u14 "Ayuda (1 de 3)" "F1, la ayuda: "
tecla esc alt-v; sleep 1
pantalla u15
if grep -q "MTitulo" $T/u15.txt; then ok "Alt+V enseña los códigos (como Reveal Codes de WordPerfect)"; else ko "Alt+V no enseña los códigos"; fi
tecla alt-v alt-x n; sleep 1
apaga
for f in pluma/pluma.asm; do
  if grep -q "^\s*cpu 8086" $f; then ok "$f se ensambla con «cpu 8086»"; else ko "$f no lleva «cpu 8086»"; fi
done
if grep -q "^\s*cpu [123]86" pluma/*.asm pluma/*.inc; then ko "PLUMA lleva instrucciones posteriores al 8086"; else ok "PLUMA: ni una instrucción posterior al 8086"; fi
tam=$(stat -c %s build/PLUMA.COM)
if [ $tam -lt 24576 ]; then ok "PLUMA.COM ocupa $tam octetos"; else ko "PLUMA.COM ha crecido a $tam octetos"; fi
if fsck.fat -n $T/a.img > $T/fsck.txt 2>&1; then ok "fsck.fat no encuentra ningún error en el disquete"; else ko "fsck.fat encuentra errores"; cat $T/fsck.txt; fi
}

p_pluma_grande() {
echo "== PLUMA: un documento mayor que la memoria, y uno de WordStar =="
cp $IMG/hispa8086-720.img $T/a.img
mcopy -i $T/a.img build/PLUMA.COM ::
python3 -c "
import sys
open(sys.argv[1],'wb').write(b''.join(b'Linea %04d de un documento grande que no cabe en la memoria de Pluma.\r\n' % i for i in range(1,1401)))" $T/GRANDE.TXT
python3 -c "
import sys
def ws(t):
    o = bytearray()
    for w in t.split(b' '):
        o += w[:-1] + bytes([w[-1] | 0x80]) + b' '
    return bytes(o[:-1])
d = ws(b'Carta de') + b' \xa0' + ws(b'prueba que') + b'\x8d\x8a' + ws(b'sigue en otra linea') + b'\r\n\x02' + ws(b'Negrita') + b'\x02 fin.\r\n\x1a'
open(sys.argv[1], 'wb').write(d)" $T/CARTA.WS
mcopy -i $T/a.img $T/GRANDE.TXT $T/CARTA.WS ::
arranca $T/a.img 486
escribe "pluma grande.txt"; sleep 4
pantalla w1
esta w1 "Linea 0001 de un documento grande" "abre un texto de 99.400 octetos (en la memoria caben unos 30 K): "
tecla ctrl-end; sleep 4
pantalla w2
esta w2 "Linea 1400 de un documento grande" "Ctrl+Fin: hasta el final (lo de antes va al disco, a trozos): "
esta w2 "100%" ""
pl_texto "FIN"
tecla ctrl-home; sleep 4
pl_texto "INICIO "
pantalla w3
esta w3 "INICIO Linea 0001" "Ctrl+Inicio: vuelta al principio, y se escribe: "
tecla f3; pl_texto "linea 0700"; tecla ret; sleep 4
pantalla w4
esta w4 "Linea 0700 de un documento grande" "F3 encuentra lo que está en el disco, en medio: "
tecla f2; sleep 8
pantalla w5
esta w5 "Guardado." "y se guarda: "
tecla alt-x; sleep 2
escribe "pluma carta.ws"; sleep 3
pantalla w6
esta w6 "Carta de prueba que sigue en otra linea" "WordStar: sin el bit alto, el retorno blando es un espacio: "
esta w6 "Negrita fin." ""
esta w6 "Abierto de WordStar: se guardará como .PLU." ""
a=$(pl_atributo 3 0)
if [ "${a#* }" = 0F ]; then ok "WordStar: su ^B es la negrita de Pluma"; else ko "la negrita de WordStar: $a"; fi
tecla f2; sleep 2; tecla alt-x; sleep 1
apaga
mtype -i $T/a.img ::GRANDE.TXT > $T/grande.txt
if python3 - $T/grande.txt <<'PY'
import sys
d = open(sys.argv[1], 'rb').read()
e = b''.join(b'Linea %04d de un documento grande que no cabe en la memoria de Pluma.\r\n' % i for i in range(1, 1401))
sys.exit(0 if d == b'INICIO ' + e + b'FIN' else 1)
PY
then ok "lo guardado es exactamente el documento, con lo escrito al principio y al final"; else ko "el documento grande no se ha guardado bien"; fi
if [ "$(mtype -i $T/a.img ::GRANDE.BAK | md5sum)" = "$(md5sum < $T/GRANDE.TXT)" ]; then ok "el de antes se queda como GRANDE.BAK"; else ko "GRANDE.BAK no es el de antes"; fi
mtype -i $T/a.img ::CARTA.PLU | tail -c +129 > $T/carta.cuerpo
if [ "$(xxd -p $T/carta.cuerpo | tr -d '\n')" = "$(printf 'Carta de prueba que sigue en otra linea\r\n\002Negrita\002 fin.\r\n' | xxd -p | tr -d '\n')" ]; then ok "y se guarda en .PLU"; else ko "CARTA.PLU: $(xxd -p $T/carta.cuerpo)"; fi
if mdir -i $T/a.img :: | grep -q "~PLUMA"; then ko "quedan archivos temporales"; else ok "al salir no quedan archivos temporales (~PLUMA_A, B y C)"; fi
if fsck.fat -n $T/a.img > $T/fsck.txt 2>&1; then ok "fsck.fat no encuentra ningún error en el disquete"; else ko "fsck.fat encuentra errores"; cat $T/fsck.txt; fi
}

p_pluma_oficina() {
echo "== PLUMA: desde el disquete de Oficina, en B: =="
cp $IMG/hispa8086-360.img $T/a.img
cp build/oficina8086-360.img $T/b.img
QMAS="-drive file=$T/b.img,format=raw,if=floppy,index=1" arranca $T/a.img 486
escribe "b:pluma b:pluma.txt"; sleep 3
pantalla o1
esta o1 "PLUMA, el procesador de textos de Hispa-8086" "el disquete de Oficina: PLUMA abre su manual: "
esta o1 "Pluma  B:PLUMA.TXT" ""
esta o1 "solas. Intro acaba el párrafo." "con sus tildes: "
tecla alt-x; sleep 1
apaga
QMAS=
if fsck.fat -n $T/b.img > $T/fsck.txt 2>&1; then ok "fsck.fat no encuentra ningún error en el disquete de Oficina"; else ko "fsck.fat encuentra errores en el de Oficina"; cat $T/fsck.txt; fi
}
