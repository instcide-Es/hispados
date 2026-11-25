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
