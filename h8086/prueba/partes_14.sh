# Las partes de la prueba que llegaron con la versión 0.14: los arreglos de la
# primera vuelta de la auditoría (25-11-2026). Cada comprobación dice el número
# del hallazgo («n.º 1»...). Las carga prueba.sh, que tiene las funciones.
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.

# ---- el núcleo y COMMAND (hallazgos 1, 2 y 3) ----
p_auditoria1() {
echo "== Hispa-8086: auditoría 1, el núcleo (copiar sobre sí mismo, ERRORLEVEL, «X\\.») =="
cp $IMG/hispa8086-360.img $T/a.img
printf '@eco off\r\nhola\r\nif errorlevel 10 eco MAL: 7 no es 10 o mas\r\nif errorlevel 17 eco MAL: 7 no es 17 o mas\r\nif not errorlevel 10 eco BIEN: 7 es menos de 10\r\nif errorlevel 07 eco BIEN: 07 es 7\r\n' > $T/NIVEL.BAT
mcopy -o -i $T/a.img $T/NIVEL.BAT ::
arranca $T/a.img 486
# n.º 1: crear (3Ch) un archivo abierto lo vaciaba mientras se leía
escribe "limpiar"; escribe "copiar basic.com ."; sleep 2; escribe "copiar \\hola.com hola.com"; sleep 1
pantalla u0
renglon u0 "No se puede copiar un archivo sobre sí mismo." "n.º 1: COPIAR sobre sí mismo (con textos distintos) lo dice: " 2
escribe "limpiar"; escribe "creardir g"; escribe "copiar command.com g"; sleep 3; escribe "copiar nucleo86.sys g"; sleep 3
escribe "cd g"; escribe "xcopiar *.*"; sleep 4; escribe "cd \\"
pantalla u1
renglon u1 "  no se puede copiar sobre sí mismo." "n.º 1: y XCOPIAR también: " 2
esta u1 "0 archivos copiados." "n.º 1: XCOPIAR no copia nada: "
escribe "limpiar"; escribe "dir basic.com"
pantalla u1b
esta u1b "$(printf '%-14s%10d' BASIC.COM $(stat -c %s build/BASIC.COM))" "n.º 1: BASIC.COM conserva su tamaño: "
# n.º 2: IF ERRORLEVEL de dos cifras
escribe "limpiar"; escribe "nivel"; sleep 3
pantalla u2
noesta u2 "MAL:" "n.º 2: ERRORLEVEL 10 y 17 no se cumplen con un 7: "
esta u2 "BIEN: 7 es menos de 10" "n.º 2: NOT ERRORLEVEL 10 sí: "
esta u2 "BIEN: 07 es 7" "n.º 2: y un cero delante no estorba: "
# n.º 3: «X\.» no es un nombre (RD X\. soltaba el grupo de X)
escribe "limpiar"; escribe "creardir x"; escribe "borrardir x\\."; escribe "renombrar x\\. y"; escribe "creardir x\\.."; escribe "dir x"
pantalla u3
esta u3 ".                  <DIR>" "n.º 3: tras «BORRARDIR X\\.» X sigue entera: "
noesta u3 "[Y]" ""
apaga
if cmp -s <(mtype -i $T/a.img ::BASIC.COM) build/BASIC.COM && cmp -s <(mtype -i $T/a.img ::HOLA.COM) build/HOLA.COM \
   && cmp -s <(mtype -i $T/a.img ::G/COMMAND.COM) build/COMMAND.COM && cmp -s <(mtype -i $T/a.img ::G/NUCLEO86.SYS) build/NUCLEO86.SYS; then
  ok "n.º 1: desde fuera, los cuatro archivos están intactos, octeto a octeto"; else ko "n.º 1: algún archivo ha quedado cortado"; mdir -i $T/a.img :: ::G; fi
if fsck.fat -n $T/a.img > $T/fsck.txt 2>&1 && mdir -i $T/a.img ::X > /dev/null 2>&1; then ok "n.º 1 y 3: fsck.fat no encuentra errores y X sigue ahí"; else ko "n.º 1 y 3: fsck.fat encuentra errores"; cat $T/fsck.txt; fi
}

# llena el disquete $1 con RELLENO.BIN hasta que solo queden unos $2 octetos
llena() {
  local libre=$(mdir -i $1 :: | awk '/bytes free/ { gsub(/ /, "", $0); sub(/bytesfree/, "", $0); print }')
  head -c $((libre - $2)) /dev/zero > $T/RELLENO.BIN
  mcopy -o -i $1 $T/RELLENO.BIN ::
}

# ---- n.º 5: CUENTAS guarda en un temporal; una hoja que no se ha podido abrir
# entera se queda sin nombre ----
p_auditoria_cuentas() {
echo "== Hispa-8086: auditoría 1, CUENTAS con el disco lleno y una hoja rota =="
cp $IMG/hispa8086-360.img $T/a.img
cp $IMG/oficina8086-360.img $T/b.img
local i; for ((i=1; i<=150; i++)); do printf '%d;%d,5;=A%d*B%d;texto de la fila %d\r\n' $i $((i*3)) $i $i $i; done > $T/E.CSV
mcopy -o -i $T/b.img $T/E.CSV ::
QMAS="-drive file=$T/b.img,format=raw,if=floppy,index=1" arranca $T/a.img 486
escribe "limpiar"; escribe "b:cuentas b:e.csv /g b:h.cta"; sleep 5
escribe "b:cuentas b:e.csv /g b:h.cta"; sleep 5; escribe "dir b:h.*"
pantalla k1
noesta k1 "CUENTAS: no se ha podido" "n.º 5: guardar encima de una hoja que ya existe: "
noesta k1 'H.$$$' "n.º 5: y no queda el temporal: "
apaga
mtype -i $T/b.img ::H.CTA > $T/h_bien.cta
local tam=$(stat -c %s $T/h_bien.cta)
if [ "$tam" -gt 2048 ]; then ok "n.º 5: la hoja de la prueba ocupa $tam octetos (más de dos grupos)"; else ko "n.º 5: la hoja de la prueba es muy pequeña ($tam)"; fi
head -c $((tam / 2)) $T/h_bien.cta > $T/ROTO.CTA
for ((i=1; i<=300; i++)); do printf '%d;%d,5;=A%d*B%d;otro texto, más largo, de la fila %d\r\n' $i $((i*3)) $i $i $i; done > $T/E2.CSV
mcopy -o -i $T/b.img $T/ROTO.CTA $T/E2.CSV ::
llena $T/b.img 3072
QMAS="-drive file=$T/b.img,format=raw,if=floppy,index=1" arranca $T/a.img 486
escribe "limpiar"; escribe "b:cuentas b:e2.csv /g b:h.cta"; sleep 8
pantalla k2
esta k2 "CUENTAS: no se ha podido leer o escribir el archivo." "n.º 5: con el disco lleno, guardar falla y lo dice: "
escribe "limpiar"; escribe "b:cuentas b:roto.cta"; sleep 3; tecla slash s; sleep 1
pantalla k3
esta k3 "Hay cambios sin guardar." "n.º 5: una hoja que no se ha podido abrir entera: Salir pregunta: "
tecla n; sleep 1
pantalla k4
apaga
if cmp -s <(mtype -i $T/b.img ::H.CTA) $T/h_bien.cta; then ok "n.º 5: con el disco lleno, la hoja vieja sigue entera"; else ko "n.º 5: el disco lleno ha estropeado la hoja vieja ($(mtype -i $T/b.img ::H.CTA | wc -c) octetos)"; fi
if ! mdir -i $T/b.img '::H.$$$' > /dev/null 2>&1 && fsck.fat -n $T/b.img > $T/fsck.txt 2>&1; then ok "n.º 5: no queda H.\$\$\$ y fsck.fat no encuentra errores"; else ko "n.º 5: queda el temporal o hay errores"; cat $T/fsck.txt; fi
}

# ---- n.º 6: EDITAR no guarda un archivo que no ha cabido entero, y guarda en
# un temporal ----
p_auditoria_editar() {
echo "== Hispa-8086: auditoría 1, EDITAR con un archivo que no cabe y con el disco lleno =="
cp $IMG/hispa8086-360.img $T/a.img
local i; for ((i=1; i<=1300; i++)); do printf 'linea %04d del texto grande de la prueba de EDITAR\r\n' $i; done > $T/GRANDE.TXT
head -c 3070 $T/GRANDE.TXT > $T/N.TXT
mcopy -o -i $T/a.img $T/GRANDE.TXT $T/N.TXT ::
llena $T/a.img 0
arranca $T/a.img 486
escribe "editar grande.txt"; sleep 3
pantalla e1
esta e1 "No cabe entero: se ve el principio y no se puede guardar." "n.º 6: al abrir uno que no cabe, lo dice: "
tecla spc; sleep 0.5; tecla x f2; sleep 2
pantalla e2
esta e2 "No cabe entero: se ve el principio y no se puede guardar." "n.º 6: y F2 no lo guarda: "
tecla spc; sleep 0.5; tecla esc; sleep 0.5; tecla n; sleep 1
escribe "editar n.txt"; sleep 2
teclea "0123456789"; tecla f2; sleep 3
pantalla e3
esta e3 "No se ha podido guardar (¿disco lleno o protegido?)." "n.º 6: con el disco lleno, guardar falla y lo dice: "
tecla spc; sleep 0.5; tecla esc; sleep 0.5; tecla n; sleep 1
apaga
if cmp -s <(mtype -i $T/a.img ::GRANDE.TXT) $T/GRANDE.TXT; then ok "n.º 6: el archivo grande sigue entero ($(stat -c %s $T/GRANDE.TXT) octetos)"; else ko "n.º 6: el archivo grande ha quedado en $(mtype -i $T/a.img ::GRANDE.TXT | wc -c) octetos"; fi
if cmp -s <(mtype -i $T/a.img ::N.TXT) $T/N.TXT; then ok "n.º 6: con el disco lleno, el original sigue como estaba"; else ko "n.º 6: el disco lleno ha estropeado el original ($(mtype -i $T/a.img ::N.TXT | wc -c) octetos)"; fi
if ! mdir -i $T/a.img '::N.$$$' > /dev/null 2>&1 && fsck.fat -n $T/a.img > $T/fsck.txt 2>&1; then ok "n.º 6: no queda N.\$\$\$ y fsck.fat no encuentra errores"; else ko "n.º 6: queda el temporal o hay errores"; cat $T/fsck.txt; fi
# y con sitio, guardar (por el temporal) funciona
mdel -i $T/a.img ::RELLENO.BIN
arranca $T/a.img 486
escribe "editar n.txt"; sleep 2
teclea "0123456789"; tecla f2; sleep 2; tecla esc; sleep 1
apaga
if [ "$(mtype -i $T/a.img ::N.TXT | head -c 10)" = 0123456789 ] && [ "$(mtype -i $T/a.img ::N.TXT | wc -c)" = $((3070 + 10)) ] && ! mdir -i $T/a.img '::N.$$$' > /dev/null 2>&1; then ok "n.º 6: con sitio, F2 guarda (y no queda el temporal)"; else ko "n.º 6: con sitio no ha guardado bien"; mdir -i $T/a.img ::; fi
}

# ---- n.º 7: PASAR RECIBIR ya no borra el archivo que había si no llega ----
p_auditoria_pasar() {
echo "== Hispa-8086: auditoría 1, PASAR RECIBIR sobre un archivo que ya existe =="
cp $IMG/hispa8086-360.img $T/a.img
arranca $T/a.img 486
escribe "limpiar"; escribe "pasar recibir leeme.txt"; sleep 2; tecla esc; sleep 2
pantalla s1
esta s1 "Se deja (Esc)." "n.º 7: PASAR RECIBIR LEEME.TXT, y Esc: "
python3 -c "import sys; sys.stdout.buffer.write(bytes((i * 7 + 3) & 255 for i in range(1500)))" > $T/nuevo.bin
escribe "pasar recibir hola.com"
if python3 prueba/xmodem.py $T/serie enviar $T/nuevo.bin; then ok "n.º 7: recibir encima de uno que ya existe"; else ko "n.º 7: PASAR RECIBIR encima de HOLA.COM"; fi
sleep 2
apaga
if cmp -s <(mtype -i $T/a.img ::LEEME.TXT) build/LEEME.TXT; then ok "n.º 7: tras el Esc, LEEME.TXT sigue entero"; else ko "n.º 7: tras el Esc, LEEME.TXT se ha perdido"; fi
if cmp -s <(mtype -i $T/a.img ::HOLA.COM) $T/nuevo.bin; then ok "n.º 7: y lo recibido encima ha sustituido al que había"; else ko "n.º 7: HOLA.COM no es lo recibido"; fi
if ! mdir -i $T/a.img '::*.$$$' > /dev/null 2>&1 && fsck.fat -n $T/a.img > $T/fsck.txt 2>&1; then ok "n.º 7: no quedan temporales y fsck.fat no encuentra errores"; else ko "n.º 7: quedan temporales o hay errores"; mdir -i $T/a.img ::; cat $T/fsck.txt; fi
}

# ---- n.º 8: el bloque de entorno de 4Bh, con una ruta relativa larga ----
p_auditoria_entorno() {
echo "== Hispa-8086: auditoría 1, lanzar un programa con una ruta relativa larga (4Bh) =="
cp $IMG/hispa8086-360.img $T/a.img
local d="AAAAAAAA/BBBBBBBB/CCCCCCCC/DDDDDDDD/EEEEEEEE/FFFFFFFF"
mmd -i $T/a.img ::AAAAAAAA ::AAAAAAAA/BBBBBBBB ::AAAAAAAA/BBBBBBBB/CCCCCCCC ::AAAAAAAA/BBBBBBBB/CCCCCCCC/DDDDDDDD ::AAAAAAAA/BBBBBBBB/CCCCCCCC/DDDDDDDD/EEEEEEEE ::$d
mcopy -i $T/a.img build/HOLA.COM ::$d/HOLA.COM
arranca $T/a.img 486
escribe "cd \\${d//\//\\}"; escribe "limpiar"
escribe ".\\.\\.\\.\\.\\.\\.\\.\\.\\.\\.\\.\\.\\.\\.\\.\\.\\.\\.\\.\\.\\.\\hola uno"; sleep 2
escribe ".\\.\\.\\.\\.\\.\\.\\.\\.\\.\\.\\.\\.\\.\\.\\.\\.\\.\\.\\.\\.\\.\\hola dos"; sleep 2
escribe "a:\\hola tres"; sleep 2; escribe "memoria"; sleep 1
pantalla n1
esta n1 "Parametros: uno" "n.º 8: con 9 + 53 + 52 letras de entorno: "
esta n1 "Parametros: dos" "n.º 8: y otra vez: "
esta n1 "Parametros: tres" "n.º 8: y después, otro programa: "
esta n1 "Libre para los programas: " "n.º 8: la cadena de la memoria sigue bien (MEMORIA): "
apaga
}

# ---- n.º 9: PLUMA abre un documento que no cabe en la memoria con el disco
# lleno: lo dice, y el documento se queda sin nombre ----
p_auditoria_pluma() {
echo "== Hispa-8086: auditoría 1, PLUMA con un documento grande y el disco lleno =="
cp $IMG/hispa8086-720.img $T/a.img
mcopy -i $T/a.img $IMG/PLUMA.COM ::
python3 -c "
import sys
open(sys.argv[1],'wb').write(b''.join(b'Linea %04d de un documento grande que no cabe en la memoria de Pluma.\r\n' % i for i in range(1,1401)))" $T/GRANDE.TXT
mcopy -i $T/a.img $T/GRANDE.TXT ::
llena $T/a.img 4096
arranca $T/a.img 486
escribe "pluma grande.txt"; sleep 5
pantalla q1
esta q1 "No ha cabido entero: solo se ha leído una parte." "n.º 9: lo dice: "
tecla spc; sleep 1; tecla f2; sleep 1
pantalla q2
esta q2 "Guardar como:" "n.º 9: y Guardar pide otro nombre (no pisa el bueno): "
tecla esc; sleep 0.5; tecla alt-x; sleep 1
pantalla q3
esta q3 "Hay cambios sin guardar." "n.º 9: y al salir, pregunta: "
tecla n; sleep 1
apaga
if cmp -s <(mtype -i $T/a.img ::GRANDE.TXT) $T/GRANDE.TXT; then ok "n.º 9: GRANDE.TXT sigue entero"; else ko "n.º 9: GRANDE.TXT ha cambiado"; fi
}

# ---- n.º 10: BASIC, DRAW con una escala y una distancia grandes ----
p_auditoria_basic() {
echo "== Hispa-8086: auditoría 1, Hispa-BASIC (DRAW grande) =="
cp $IMG/hispa8086-360.img $T/a.img
printf '10 SCREEN 1\r\n20 DRAW "S8U100M+10,-10"\r\n30 ON ERROR GOTO 100\r\n40 DRAW "S255U600"\r\n50 PRINT "NO DEBE SALIR"\r\n60 END\r\n100 SCREEN 0: PRINT "ERROR"; ERR; "EN"; ERL\r\n110 END\r\n' > $T/DIBUJO.BAS
mcopy -o -i $T/a.img $T/DIBUJO.BAS ::
arranca $T/a.img 486
escribe "limpiar"; escribe "basic dibujo.bas"; sleep 5
pantalla g1
esta g1 "ERROR 6 EN 40" "n.º 10: DRAW «S255U600» da «Desbordamiento» (no cuelga el equipo): "
noesta g1 "NO DEBE SALIR" ""
escribe "system"; sleep 1; escribe "eco sigue vivo"; sleep 1
pantalla g2
esta g2 "sigue vivo" "n.º 10: y el sistema sigue: "
apaga
}

# ---- n.º 11, 12 y 13: REVISAR con archivos cruzados, cadenas en círculo y
# FILEnnnn.CHK que ya existen ----
p_auditoria_revisar() {
echo "== Hispa-8086: auditoría 1, REVISAR (cruces, círculos, FILEnnnn.CHK) =="
local F="python3 prueba/fat12.py"
cp $IMG/hispa8086-360.img $T/a.img
rm -f $T/b.img; mformat -C -f 360 -i $T/b.img ::
head -c 3000 build/BASIC.COM > $T/UNO.TXT; cp $T/UNO.TXT $T/DOS.TXT; cp $T/UNO.TXT $T/TRES.TXT
printf 'viejo' > $T/FILE0000.CHK
mcopy -i $T/b.img $T/UNO.TXT $T/DOS.TXT $T/TRES.TXT $T/FILE0000.CHK ::
local g1=$($F $T/b.img grupo UNO.TXT) g3=$($F $T/b.img grupo TRES.TXT)
$F $T/b.img primero DOS.TXT $g1                        # DOS cruza con UNO desde el primer grupo
$F $T/b.img pon $((g3 + 2)) $g3                        # TRES: su último grupo vuelve al primero
local p=$($F $T/b.img libres 2); $F $T/b.img pon $p $((p + 1)); $F $T/b.img pon $((p + 1)) 0xFFF   # una cadena perdida
# en A:, un directorio cuya cadena da la vuelta sobre sí misma
mmd -i $T/a.img ::BUCLE
local gd=$($F $T/a.img grupo BUCLE); $F $T/a.img pon $gd $gd
QMAS="-drive file=$T/b.img,format=raw,if=floppy,index=1" arranca $T/a.img 486
escribe "limpiar"; escribe "revisar b: /f"; sleep 5
pantalla r1
esta r1 "DOS.TXT: cruzado con otro archivo en el grupo $g1" "n.º 11: el cruce, dicho: "
noesta r1 "DOS.TXT: el tamaño no concuerda" "n.º 11: y el cruzado no se mide (ni se recorta): "
esta r1 "TRES.TXT: su cadena de grupos está rota." "n.º 11: la cadena en círculo es una cadena rota: "
noesta r1 "TRES.TXT: cruzado" ""
esta r1 "2 cadenas perdidas guardadas en archivos FILEnnnn.CHK." "n.º 13 (la de la prueba y la que era de DOS.TXT): "
escribe "limpiar"; escribe "dir b:file*.chk"
pantalla r2
esta r2 "FILE0001.CHK" "n.º 13: las cadenas perdidas, desde FILE0001.CHK (FILE0000.CHK ya estaba): "
esta r2 "FILE0002.CHK" ""
esta r2 "3 archivos" ""
escribe "limpiar"; escribe "revisar"; sleep 6; escribe "eco sigue vivo"
pantalla r3
esta r3 "BUCLE: su cadena de grupos está rota." "n.º 12: un directorio cuya cadena da la vuelta: "
esta r3 "sigue vivo" "n.º 12: y REVISAR acaba (no se queda dando vueltas): "
apaga
if [ "$(mtype -i $T/b.img ::DOS.TXT 2> /dev/null | wc -c)" = 3000 ] && [ "$(mtype -i $T/b.img ::FILE0000.CHK)" = viejo ]; then ok "n.º 11 y 13: DOS.TXT conserva su tamaño, y FILE0000.CHK lo que tenía"; else ko "n.º 11 y 13: DOS.TXT o FILE0000.CHK han cambiado"; mdir -i $T/b.img ::; fi
if [ "$($F $T/b.img grupo TRES.TXT)" = $g3 ] && python3 - $T/b.img $g3 <<'PY'
import struct, sys
d = open(sys.argv[1], 'rb').read(); g = int(sys.argv[2])
def fat(g):
    w = struct.unpack_from('<H', d, 512 + g * 3 // 2)[0]
    return (w >> 4) if g & 1 else w & 0xFFF
n = 0
while g < 0xFF8 and n < 10: g = fat(g); n += 1
sys.exit(0 if g >= 0xFF8 and n == 3 else 1)
PY
then ok "n.º 11: con /F, la cadena de TRES.TXT queda cortada en su tercer grupo"; else ko "n.º 11: la cadena de TRES.TXT sigue en círculo"; fi
}

# ---- n.º 14 y 15: una BPB que no vale y grupos que no existen ----
p_auditoria_bpb() {
echo "== Hispa-8086: auditoría 1, discos con la BPB o los grupos estropeados =="
local F="python3 prueba/fat12.py"
cp $IMG/hispa8086-360.img $T/a.img
rm -f $T/b.img; mformat -C -f 1440 -i $T/b.img ::; mcopy -i $T/b.img build/LEEME.TXT ::
printf '\0\0' | dd of=$T/b.img bs=1 seek=$((0x18)) conv=notrunc 2> /dev/null     # 0 sectores por pista
rm -f $T/c.img; mformat -C -f 360 -i $T/c.img ::; mcopy -i $T/c.img build/LEEME.TXT ::; mcopy -i $T/c.img build/HOLA.COM ::UNO.TXT
local i; for ((i=1; i<=100; i++)); do printf 'x' > $T/R$i.TXT; done   # (las últimas, en el último sector de la raíz)
(cd $T && mcopy -i c.img R*.TXT ::); rm -f $T/R*.TXT
$F $T/c.img primero UNO.TXT 1                       # el primer grupo de UNO.TXT, el 1
QMAS="-drive file=$T/b.img,format=raw,if=floppy,index=1" arranca $T/a.img 486
escribe "limpiar"; escribe "dir b:"; sleep 3; escribe "eco sigue vivo"
pantalla x1
esta x1 "sigue vivo" "n.º 14: un disquete con 0 sectores por pista no cuelga el equipo: "
noesta x1 "LEEME.TXT" "n.º 14: y no se lee como si valiera: "
apaga
QMAS="-drive file=$T/c.img,format=raw,if=floppy,index=1" arranca $T/a.img 486
escribe "limpiar"; escribe "eco escrito en el grupo 1 >> b:uno.txt"; sleep 2; escribe "dir b:leeme.txt"; sleep 2
pantalla x2
esta x2 "LEEME.TXT" "n.º 15: escribir en un archivo cuyo primer grupo es el 1 no machaca la raíz: "
apaga
if cmp -s <(mtype -i $T/c.img ::LEEME.TXT) build/LEEME.TXT && [ "$(mdir -b -i $T/c.img :: 2> /dev/null | grep -c '/R[0-9]*.TXT')" = 100 ]; then ok "n.º 15: desde fuera, la raíz entera (102 archivos) y LEEME.TXT siguen bien"; else ko "n.º 15: la raíz se ha estropeado"; mdir -i $T/c.img ::; fi
}

# ---- n.º 16: una ruta de más de 79 letras ya no se corta en silencio ----
p_auditoria_ruta() {
echo "== Hispa-8086: auditoría 1, rutas de más de 79 letras =="
cp $IMG/hispa8086-360.img $T/a.img
local d="" l
for l in A B C D E F G H; do d="$d/$l$l$l$l$l$l$l$l"; mmd -i $T/a.img "::$d"; done
local r="${d//\//\\}\\DATOS.TXT"          # 82 letras: cortada en 79, «...\DATOS.»
arranca $T/a.img 486
escribe "limpiar"; escribe "eco hola > $r"; sleep 1
pantalla l1
esta l1 "No existe esa ruta." "n.º 16: ${#r} letras: «No existe esa ruta» (error 3), como en DOS: "
apaga
if [ -z "$(mdir -b -i $T/a.img "::$d" 2> /dev/null | grep -i datos)" ]; then ok "n.º 16: y no se ha creado «DATOS» (lo que quedaba al cortarla)"; else ko "n.º 16: se ha creado $(mdir -b -i $T/a.img "::$d" | grep -i datos)"; fi
}

# ---- n.º 17: SISTEMA en un disquete casi lleno no toca su arranque ----
p_auditoria_sistema() {
echo "== Hispa-8086: auditoría 1, SISTEMA en un disquete sin sitio =="
cp $IMG/hispa8086-360.img $T/a.img
rm -f $T/b.img; mformat -C -f 360 -i $T/b.img ::; mcopy -i $T/b.img build/LEEME.TXT ::
llena $T/b.img 4096
dd if=$T/b.img of=$T/arr0.bin bs=512 count=1 2> /dev/null
QMAS="-drive file=$T/b.img,format=raw,if=floppy,index=1" arranca $T/a.img 486
escribe "limpiar"; escribe "sistema b: /a"; sleep 10
pantalla y1
esta y1 "No se ha podido copiar el sistema: ¿está lleno el disco?" "n.º 17: no caben los archivos, y lo dice: "
apaga
if cmp -s <(dd if=$T/b.img bs=512 count=1 2> /dev/null) $T/arr0.bin; then ok "n.º 17: el sector de arranque sigue como estaba"; else ko "n.º 17: el sector de arranque ha cambiado aunque no cabía el sistema"; fi
}

# ---- n.º 18: XCOPIAR /S con el destino dentro del origen ----
p_auditoria_xcopiar() {
echo "== Hispa-8086: auditoría 1, XCOPIAR /S con el destino dentro del origen =="
cp $IMG/hispa8086-360.img $T/a.img
mmd -i $T/a.img ::DOCS ::DOCS/SUB; mcopy -i $T/a.img build/HOLA.COM ::DOCS/UNO.COM; mcopy -i $T/a.img build/HOLA.COM ::DOCS/SUB/DOS.COM
arranca $T/a.img 486
escribe "limpiar"; escribe "xcopiar docs docs\\copia /s"; sleep 8
pantalla j1
esta j1 "2 archivos copiados." "n.º 18: copia lo de DOCS (UNO.COM y SUB\\DOS.COM) y no se mete en COPIA: "
apaga
if mdir -i $T/a.img ::DOCS/COPIA/SUB/DOS.COM > /dev/null 2>&1 && ! mdir -i $T/a.img ::DOCS/COPIA/COPIA > /dev/null 2>&1 && fsck.fat -n $T/a.img > $T/fsck.txt 2>&1; then ok "n.º 18: DOCS\\COPIA tiene lo de DOCS, sin COPIA\\COPIA, y fsck.fat no encuentra errores"; else ko "n.º 18: XCOPIAR se ha metido en el destino"; mdir -/ -i $T/a.img ::DOCS | head -30; fi
}

# ---- n.º 19: CUENTAS con archivos malformados ----
p_auditoria_cuentas2() {
echo "== Hispa-8086: auditoría 1, CUENTAS con archivos malformados =="
cp $IMG/hispa8086-360.img $T/a.img
cp $IMG/oficina8086-360.img $T/b.img
printf 'HCTA\001\001\000\120' > $T/ANCHO.CTA; printf '\377\377' >> $T/ANCHO.CTA      # la columna A, de 80
printf '=SUMA(1;2;fila1\r\n7;8\r\n9;10\r\n' > $T/PAR.CSV
python3 -c "import sys; sys.stdout.write(';'.join(str(i) for i in range(1, 301)) + '\r\n')" > $T/ANCHA.CSV
mcopy -i $T/b.img $T/ANCHO.CTA $T/PAR.CSV $T/ANCHA.CSV ::
QMAS="-drive file=$T/b.img,format=raw,if=floppy,index=1" arranca $T/a.img 486
escribe "limpiar"; escribe "b:cuentas b:ancho.cta"; sleep 4; tecla slash s; sleep 2
escribe "b:cuentas b:par.csv /g b:par2.csv"; sleep 4
escribe "b:cuentas b:ancha.csv /g b:ancha2.csv"; sleep 6
escribe "eco sigue vivo"
pantalla m1
esta m1 "sigue vivo" "n.º 19: un .CTA con una columna de 80 de ancho no cuelga: "
apaga
if [ "$(mtype -i $T/b.img ::PAR2.CSV | sed -n 2p | tr -d '\r')" = "7;8" ]; then ok "n.º 19: una fórmula sin cerrar el paréntesis no se traga las líneas de detrás"; else ko "n.º 19: CSV con paréntesis sin cerrar: $(mtype -i $T/b.img ::PAR2.CSV | head -3 | tr '\r\n' '  ')"; fi
if [ "$(mtype -i $T/b.img ::ANCHA2.CSV | head -1 | tr -d '\r' | cut -d';' -f1)" = 1 ] && [ "$(mtype -i $T/b.img ::ANCHA2.CSV | head -1 | tr -d '\r' | awk -F';' '{print $256}')" = 256 ]; then ok "n.º 19: un CSV de 300 columnas: la A sigue siendo la 1 y la última, la 256"; else ko "n.º 19: CSV de 300 columnas: $(mtype -i $T/b.img ::ANCHA2.CSV | head -c 60)"; fi
}

# ---- n.º 20: SAVE de BASIC por un temporal, y PLUMA con poca memoria ----
p_auditoria_save() {
echo "== Hispa-8086: auditoría 1, SAVE de BASIC con el disco lleno o protegido, y PLUMA con poca memoria =="
cp $IMG/hispa8086-360.img $T/a.img
local i; for ((i=1; i<=40; i++)); do printf '%d PRINT "linea %d de un programa para guardar con el disco lleno"\r\n' $((i * 10)) $i; done > $T/PROG.BAS
printf '10 PRINT "solo leer"\r\n' > $T/RO.BAS
mcopy -i $T/a.img $T/PROG.BAS $T/RO.BAS ::; mattrib -i $T/a.img +r ::RO.BAS
llena $T/a.img 0
arranca $T/a.img 486
escribe "limpiar"; escribe "basic"; sleep 2
escribe "load \"prog\""; sleep 1; escribe "1000 rem una linea mas, para que no quepa"; escribe "save \"prog\",a"; sleep 2
escribe "save \"ro\""; sleep 1
pantalla v1
esta v1 "Disco lleno" "n.º 20: SAVE con el disco lleno: "
esta v1 "Permiso denegado" "n.º 20: SAVE sobre uno de solo leer: «Permiso denegado» (no «Nombre de archivo no válido»): "
escribe "system"; sleep 1
apaga
if cmp -s <(mtype -i $T/a.img ::PROG.BAS) $T/PROG.BAS && cmp -s <(mtype -i $T/a.img ::RO.BAS) $T/RO.BAS && ! mdir -i $T/a.img '::*.$$$' > /dev/null 2>&1; then ok "n.º 20: los dos .BAS siguen como estaban, sin temporales"; else ko "n.º 20: SAVE ha estropeado un .BAS o ha dejado un temporal"; mdir -i $T/a.img :: | tail -5; fi
# PLUMA con menos de 64 K libres (un disco en memoria de 512 K y tres
# intérpretes, uno dentro de otro, se comen el resto)
cp $IMG/hispa8086-360.img $T/a.img
mcopy -i $T/a.img $IMG/PLUMA.COM ::
printf 'DEVICE=\\DISCORAM.SYS 512\r\n' > $T/CONFIG.SYS
mcopy -o -i $T/a.img $T/CONFIG.SYS ::
arranca $T/a.img 486
escribe "command"; sleep 1; escribe "command"; sleep 1; escribe "command"; sleep 1; escribe "memoria"; sleep 1
escribe "limpiar"; escribe "memoria"; escribe "pluma"; sleep 3; escribe "eco sigue vivo"
pantalla v2
esta v2 "PLUMA necesita 64 K de memoria libres." "n.º 20: PLUMA con poca memoria lo dice: "
esta v2 "sigue vivo" "n.º 20: y no se cuelga: "
apaga
}
