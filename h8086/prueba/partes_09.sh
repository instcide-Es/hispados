# Las partes de la prueba que llegaron con la versión 0.10 (las carga
# prueba.sh, que tiene las funciones: arranca, escribe, pantalla, esta...).
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.

p_ordenes() {
echo "== Hispa-8086: las órdenes nuevas del intérprete =="
cp $IMG/hispa8086-360.img $T/a.img
mcopy -i $T/a.img prueba/SIIR.BAT ::
: > $T/lpt.txt
QMAS="-parallel file:$T/lpt.txt" arranca $T/a.img 486
escribe "limpiar"; escribe "fecha 5-3-1999"; escribe "hora 7:08:09"; escribe "fecha"; tecla ret; sleep 0.5; escribe "hora"; tecla ret; sleep 0.5; escribe "fecha 40-1-2000"
pantalla o1
esta o1 "Fecha: 05-03-1999" "FECHA la pone y la dice: "
esta o1 "Hora: 07:08:" "HORA la pone y la dice: "
esta o1 "Fecha nueva (dd-mm-aaaa), o Intro:" "sin nada, pregunta: "
esta o1 "Esa fecha no vale." ""
escribe "limpiar"; escribe "vol"; escribe "verificar"; escribe "verificar on"; escribe "verificar"; escribe "memoria"; escribe "pon uno=dos"; escribe "pon"
pantalla o2
esta o2 "El disco se llama HISPA-8086" "VOL: "
esta o2 "VERIFICAR está quitado (OFF)." "VERIFICAR: "
esta o2 "VERIFICAR está puesto (ON)." ""
esta o2 "Memoria: 639 K. Libre para los programas: " "MEMORIA: "
esta o2 'PATH=A:\' "PON enseña las variables: "
esta o2 "UNO=dos" "y pone una nueva: "
escribe "limpiar"; escribe "creardir d1"; escribe "copiar *.bat d1"; sleep 5; escribe "renombrar d1\\*.bat *.txt"; sleep 1; escribe "dir d1"
pantalla o3
esta o3 "3 archivos copiados." "COPIAR con comodines a un directorio (con VERIFICAR ON): "
esta o3 "AUTOEXEC.TXT         124" "RENOMBRAR con comodines: "
esta o3 "LOTE.TXT             462" ""
noesta o3 ".BAT   " ""
escribe "limpiar"; escribe "mover d1\\lote.txt ."; escribe "dir lote.txt"; escribe "dir d1\\lote.txt"
pantalla o4
esta o4 "1 archivo movido." "MOVER a otro directorio: "
esta o4 "LOTE.TXT             462" "está en el nuevo... "
esta o4 "0 archivos, 0 octetos" "...y ya no en el viejo: "
escribe "limpiar"; escribe "copiar leeme.txt leeme.txt"; escribe "copiar d1\\*.txt *.b"; sleep 3; escribe "dir *.b"
pantalla o4
esta o4 "No se puede copiar un archivo sobre sí mismo." ""
esta o4 "AUTOEXEC.B           124" "COPIAR *.TXT *.B (el destino, con comodines): "
escribe "limpiar"; escribe "copiar con nota.txt"; escribe "linea uno"; escribe "linea dos"; tecla ctrl-z ret; sleep 1.5; escribe "mostrar nota.txt"; escribe "copiar nota.txt con"
pantalla o5
if [ "$(grep -c '^linea uno' $T/o5.txt)" = 3 ] && [ "$(grep -c '^linea dos' $T/o5.txt)" = 3 ]; then ok "COPIAR CON archivo (hasta Ctrl+Z) y COPIAR archivo CON"; else ko "COPIAR CON / COPIAR a CON"; fi
escribe "limpiar"; escribe "copiar leeme.txt prn"; sleep 1; escribe "dir hola.com > lpt1"; escribe "eco esto no sale > nul"; escribe "dir nul"
pantalla o6
esta o6 "1 archivo copiado." "COPIAR archivo PRN: "
if grep -q "^esto no sale" $T/o6.txt; then ko "> NUL no se lo traga"; else ok "> NUL se lo traga"; fi
esta o6 "0 archivos, 0 octetos" "y no queda ningún archivo NUL: "
escribe "limpiar"; escribe "cd d1"; escribe "hola por la ruta"; escribe "ruta ;"; escribe "ruta"; escribe "hola"; escribe "ruta a:\\"; escribe "ruta"; escribe "indicador \$n\$g "; escribe "indicador"
pantalla o7
esta o7 "Parametros: por la ruta" "un programa de otro directorio, por la variable PATH: "
esta o7 "No hay ruta de búsqueda." "RUTA ; la quita: "
esta o7 "Orden desconocida." "y entonces no se encuentra: "
esta o7 'A:\' "RUTA la pone y la dice: "
esta o7 "A> indicador" "INDICADOR \$N\$G cambia el indicador: "
escribe "limpiar"; escribe "borrar *.*"; tecla n; sleep 1; escribe "dir"; escribe "borrar *.*"; tecla s; sleep 2; escribe "dir"
pantalla o8
esta o8 "Se van a borrar TODOS los archivos. ¿Seguro? (S/N)" "BORRAR *.* pregunta: "
esta o8 "4 archivos, " "con «n» no borra: "
esta o8 "2 archivos, 0 octetos" "con «s», sí (quedan «.» y «..»): "
escribe "limpiar"; escribe "cd \\"; escribe "siir uno"; sleep 4
pantalla o9
esta o9 "Si: no existe" "SI NO EXISTE: "
esta o9 "Si: nivel 7" "SI NIVEL: "
esta o9 "Si: fin" "IR A, y SI a==b: "
noesta o9 "NO DEBE SALIR" ""
apaga
QMAS=
if head -c 15 $T/lpt.txt | grep -qa "Hispa-8086 0.12" && grep -qa "HOLA.COM             206" $T/lpt.txt; then ok "lo mandado a PRN y a LPT1 ha salido por el puerto paralelo"; else ko "PRN: no ha salido lo esperado por el puerto paralelo"; fi
if fsck.fat -n $T/a.img > $T/fsck.txt 2>&1; then ok "fsck.fat no encuentra ningún error en el disquete"; else ko "fsck.fat encuentra errores"; cat $T/fsck.txt; fi
}

p_dispositivos() {
echo "== Hispa-8086: DEVICE= (un disco en memoria y un dispositivo de carácter) =="
cp $IMG/hispa8086-360.img $T/a.img
printf 'DEVICE=\\DISCORAM.SYS 100\r\nDEVICE=\\SALUDO.SYS\r\n' > $T/CONFIG.SYS
mcopy -i $T/a.img $T/CONFIG.SYS build/SALUDO.SYS ::
arranca $T/a.img 486
pantalla v1
esta v1 "DISCORAM: disco en memoria de 100 K en la unidad D:" "DEVICE=DISCORAM.SYS, al arrancar: "
escribe "limpiar"; escribe "mostrar saludo"; escribe "eco hola > saludo"; escribe "mostrar saludo.txt"
pantalla v2
esta v2 "Saludos del controlador: 00 octetos recibidos." "un dispositivo de carácter cargado con DEVICE=, leído como un archivo: "
esta v2 "Saludos del controlador: 07 octetos recibidos." "y escrito con «>»: "
escribe "limpiar"; escribe "copiar *.com d:"; sleep 9; escribe "d:"; escribe "creardir sub"; escribe "copiar a:leeme.txt sub"; sleep 2; escribe "hola desde la memoria"; escribe "revisar"; sleep 3
pantalla v3
esta v3 'D:\>' "la unidad D: existe: "
esta v3 "Parametros: desde la memoria" "y de ella se cargan programas: "
esta v3 "99328 octetos en total en el disco" "REVISAR la repasa: "
esta v3 "512 octetos en 1 directorios" ""
noesta v3 "Hay errores" ""
escribe "limpiar"; escribe "a:"; escribe "dir d:\\sub"; escribe "equipo"
pantalla v4
esta v4 "LEEME.TXT" "un directorio en D:, visto desde A:: "
libres=$(grep -o "([0-9]* K libres" $T/v4.txt | tr -dc 0-9)
if [ -n "$libres" ] && [ "$libres" -ge 480 ] && [ "$libres" -le 535 ]; then ok "el disco de 100 K se le quita a la memoria: quedan $libres K"; else ko "memoria libre con DISCORAM: $libres K"; fi
apaga
}

p_reserva() {
echo "== Hispa-8086: la reserva de sectores (BUFFERS=) =="
for n in 0 4 20; do
  cp $IMG/hispa8086-360.img $T/a.img
  printf 'BUFFERS=%d\r\n' $n > $T/CONFIG.SYS
  [ $n = 4 ] && : > $T/CONFIG.SYS        # (4 es lo que hay sin decir nada)
  mcopy -i $T/a.img $T/CONFIG.SYS build/CUENTA.COM ::
  arranca $T/a.img 486
  escribe "limpiar"; escribe "cuenta > c1.txt"; escribe "dir"; escribe "dir"; escribe "copiar leeme.txt x.txt"; sleep 2; escribe "mostrar x.txt"
  escribe "limpiar"; escribe "cuenta"; escribe "mostrar c1.txt"
  pantalla r$n
  apaga
  # la primera línea CUENTA es la de después; la segunda (el archivo), la de antes
  set -- $(grep "^CUENTA" $T/r$n.txt | head -1 | tr -dc '0-9 '); ld=$((10#${1:-0})); ed=$((10#${2:-0}))
  set -- $(grep "^CUENTA" $T/r$n.txt | tail -1 | tr -dc '0-9 '); la=$((10#${1:-0})); ea=$((10#${2:-0}))
  eval "lect_$n=$((ld - la)); escr_$n=$((ed - ea))"
  if fsck.fat -n $T/a.img > $T/fsck.txt 2>&1 && cmp -s <(mtype -i $T/a.img ::X.TXT) build/LEEME.TXT; then ok "BUFFERS=$n: lo copiado está bien y fsck.fat no encuentra errores"; else ko "BUFFERS=$n: el disquete no ha quedado bien"; fi
done
echo "  (DIR dos veces, COPIAR y MOSTRAR: sin reserva, $lect_0 sectores leídos y $escr_0 escritos; con 4, $lect_4 y $escr_4; con 20, $lect_20 y $escr_20)" | tee $T/medida.txt
if [ "$lect_4" -gt 0 ] && [ "$lect_4" -lt "$lect_0" ]; then ok "con la reserva de 4 sectores se lee menos del disquete ($lect_4 sectores, frente a $lect_0 sin ella)"; else ko "la reserva no ahorra lecturas ($lect_4 frente a $lect_0)"; fi
if [ "$lect_20" -le "$lect_4" ]; then ok "y con 20, menos todavía ($lect_20)"; else ko "con 20 sectores se lee más que con 4 ($lect_20 frente a $lect_4)"; fi
if [ "$escr_4" = "$escr_0" ] && [ "$escr_20" = "$escr_0" ]; then ok "escribir no se retrasa: los mismos sectores escritos con reserva y sin ella ($escr_0)"; else ko "con la reserva cambia lo que se escribe ($escr_0, $escr_4, $escr_20)"; fi
}

p_discos() {
echo "== Hispa-8086: REVISAR, ETIQUETA, ATRIBUTO, XCOPIAR y ARBOL =="
cp $IMG/hispa8086-360.img $T/a.img
rm -f $T/b.img; mformat -C -f 360 -i $T/b.img ::; mcopy -i $T/b.img build/LEEME.TXT build/HOLA.COM ::; mmd -i $T/b.img ::SUB ::SUB/DENTRO; mcopy -i $T/b.img build/HOLA.COM ::SUB/UNO.COM; mcopy -i $T/b.img build/LEEME.TXT ::SUB/DENTRO/DOS.TXT
# se estropea a propósito: una cadena de tres grupos que no es de nadie, y un
# archivo que dice medir más de lo que tienen sus grupos
python3 - $T/b.img <<'PY2'
import struct, sys
d = bytearray(open(sys.argv[1], 'rb').read())
def pon(n, v):
    for fat in (512, 512 + 2 * 512):
        o = fat + n * 3 // 2
        w = d[o] | d[o + 1] << 8
        w = (w & 0x000F) | (v << 4) if n & 1 else (w & 0xF000) | v
        d[o], d[o + 1] = w & 255, w >> 8
pon(200, 201); pon(201, 202); pon(202, 0xFFF)
raiz = 512 * 5
for i in range(112):
    e = raiz + 32 * i
    if d[e:e + 11] == b'LEEME   TXT':
        d[e + 28:e + 32] = struct.pack('<I', 5000)
open(sys.argv[1], 'wb').write(d)
PY2
QMAS="-drive file=$T/b.img,format=raw,if=floppy,index=1" arranca $T/a.img 486
escribe "limpiar"; escribe "revisar b:"; sleep 3
pantalla d1
esta d1 "LEEME.TXT: el tamaño no concuerda con sus grupos." "REVISAR ve el tamaño que no cuadra: "
esta d1 "3 grupos perdidos en 1 cadenas." "y la cadena perdida: "
esta d1 "362496 octetos en total en el disco" ""
esta d1 "2048 octetos en 2 directorios" ""
esta d1 "octetos de memoria" "dice la memoria: "
esta d1 "Hay errores. REVISAR /F los corrige." ""
escribe "limpiar"; escribe "chkdsk b: /f"; sleep 4; escribe "dir b:"
pantalla d2
esta d2 "1 cadenas perdidas guardadas en archivos FILEnnnn.CHK." "REVISAR /F (y CHKDSK, su nombre clásico): "
esta d2 "FILE0000.CHK        3072" "la cadena perdida, en un archivo: "
esta d2 "LEEME.TXT           1024" "y el tamaño, corregido: "
escribe "limpiar"; escribe "revisar b:"; sleep 3
pantalla d3
noesta d3 "Hay errores" "después de corregir, REVISAR no encuentra nada: "
noesta d3 "perdidos" ""
esta d3 "octetos en 5 archivos" ""
escribe "limpiar"; escribe "etiqueta b:"; escribe "etiqueta b: pruebas9"; escribe "vol b:"; escribe "label b:"; escribe "etiqueta b: /b"; escribe "vol b:"
pantalla d4
esta d4 "El disco no tiene etiqueta." "ETIQUETA, en un disco sin nombre: "
esta d4 "El disco se llama PRUEBAS9" "ETIQUETA lo pone, y VOL y LABEL lo dicen: "
if [ "$(grep -c 'El disco se llama PRUEBAS9' $T/d4.txt)" = 2 ] && [ "$(grep -c 'El disco no tiene etiqueta.' $T/d4.txt)" = 2 ]; then ok "ETIQUETA /B lo quita"; else ko "ETIQUETA /B"; fi
escribe "limpiar"; escribe "etiqueta b: datos"; escribe "atributo +r +h b:hola.com"; escribe "atributo b:*.*"; escribe "borrar b:hola.com"; escribe "dir b:hola.com"; escribe "attrib -h b:hola.com"; escribe "dir b:hola.com"
pantalla d5
esta d5 "A HR  b:HOLA.COM" "ATRIBUTO +R +H, y los enseña: "
esta d5 "A     b:LEEME.TXT" ""
esta d5 "No se puede: acceso denegado." "el de solo leer no se borra: "
esta d5 "0 archivos, 0 octetos" "el oculto no sale en DIR: "
esta d5 "HOLA.COM             206" "ATTRIB -H: vuelve a salir: "
escribe "limpiar"; escribe "xcopiar b:\\sub copia /s"; sleep 6; escribe "arbol copia /a"; sleep 1; escribe "tree b:"; sleep 1
pantalla d6
esta d6 'B:\SUB\DENTRO\DOS.TXT' "XCOPIAR /S baja a los subdirectorios: "
esta d6 "2 archivos copiados." ""
esta d6 "  [DENTRO]" "ARBOL /A: "
esta d6 "    DOS.TXT" ""
esta d6 "    [DENTRO]" "TREE B: (el árbol entero): "
esta d6 "2 directorios" ""
apaga
QMAS=
if fsck.fat -n $T/b.img > $T/fsck.txt 2>&1; then ok "fsck.fat da por bueno el disquete corregido por REVISAR /F"; else ko "fsck.fat encuentra errores tras REVISAR /F"; cat $T/fsck.txt; fi
if mdir -i $T/b.img :: | grep -q "DATOS" && [ "$(mtype -i $T/b.img ::FILE0000.CHK | wc -c)" = 3072 ] && [ "$(mattrib -i $T/b.img ::HOLA.COM | cut -c1-8 | tr -d ' ')" = "AR" ]; then ok "desde fuera: la etiqueta, el archivo recuperado y los atributos"; else ko "desde fuera no se ve lo esperado"; mdir -i $T/b.img ::; mattrib -i $T/b.img ::HOLA.COM; fi
if fsck.fat -n $T/a.img > $T/fsck.txt 2>&1 && cmp -s <(mtype -i $T/a.img ::COPIA/DENTRO/DOS.TXT) build/LEEME.TXT && cmp -s <(mtype -i $T/a.img ::COPIA/UNO.COM) build/HOLA.COM; then ok "lo copiado por XCOPIAR está entero"; else ko "XCOPIAR no ha copiado bien"; fi
# dos archivos cruzados (comparten un grupo): lo dice y no lo toca
rm -f $T/b.img; mformat -C -f 360 -i $T/b.img ::; mcopy -i $T/b.img build/LEEME.TXT ::UNO.TXT; mcopy -i $T/b.img build/LEEME.TXT ::DOS.TXT
python3 - $T/b.img <<'PY2'
import sys
d = bytearray(open(sys.argv[1], 'rb').read())
raiz = 512 * 5
for i in range(112):
    e = raiz + 32 * i
    if d[e:e + 11] == b'DOS     TXT':
        d[e + 26:e + 28] = b'\x02\x00'
open(sys.argv[1], 'wb').write(d)
PY2
QMAS="-drive file=$T/b.img,format=raw,if=floppy,index=1" arranca $T/a.img 486
escribe "limpiar"; escribe "revisar b:"; sleep 3
pantalla d7
esta d7 "DOS.TXT: cruzado con otro archivo en el grupo 2" "REVISAR ve dos archivos cruzados: "
esta d7 "1 grupos perdidos en 1 cadenas." "y el grupo que ha quedado suelto: "
esta d7 "Los archivos cruzados no: cópielos a otro sitio y borre los dos." ""
apaga
QMAS=
}

# (para p_fantasma) cada vez que el sistema pida un disquete, se le pone el
# de esa unidad. $1: la pantalla; $2: lo que tiene que acabar saliendo
pon_el_que_pida() {
  local i u
  for ((i=0; i<12; i++)); do
    pantalla $1
    grep -qF -- "$2" $T/$1.txt && return 0
    u=$(grep -o "Ponga el disquete de la unidad [AB]" $T/$1.txt | tail -1 | tr -dc AB | tail -c 1)
    if [ -n "$u" ] && [ "$u" != "$puesto" ]; then
      if [ $u = A ]; then mon "change floppy0 $T/a.img raw" 1.5; else mon "change floppy0 $T/b.img raw" 1.5; fi
      puesto=$u; tecla spc; sleep 2
    else sleep 1; fi
  done
  return 1
}

p_fantasma() {
echo "== Hispa-8086: una sola disquetera (B: es la misma) y los disquetes del PC original =="
cp $IMG/hispa8086-360.img $T/a.img
rm -f $T/b.img; mformat -C -f 360 -i $T/b.img ::
arranca $T/a.img 486
puesto=A
escribe "limpiar"; escribe "dir b:"
if pon_el_que_pida g1 "0 archivos, 0 octetos"; then ok "DIR B: pide el disquete de B: y lo lee"; else ko "DIR B: con una sola disquetera"; fi
esta g1 "Ponga el disquete de la unidad B: y pulse una tecla..." ""
escribe "limpiar"; escribe "copiar a:leeme.txt b:"
if pon_el_que_pida g2 "1 archivo copiado."; then ok "COPIAR de A: a B: con una sola disquetera, cambiando de disquete"; else ko "COPIAR de A: a B: con una sola disquetera"; fi
escribe "limpiar"; escribe "dir b:"; sleep 1
if pon_el_que_pida g3 "1 archivos, "; then ok "y el archivo está en B:"; else ko "el archivo no está en B:"; fi
escribe "limpiar"; escribe "dir a:hola.com"
if pon_el_que_pida g4 "HOLA.COM             206"; then ok "de vuelta a A:, pide su disquete"; else ko "de vuelta a A:"; fi
apaga
if fsck.fat -n $T/b.img > $T/fsck.txt 2>&1 && cmp -s <(mtype -i $T/b.img ::LEEME.TXT) build/LEEME.TXT; then ok "el disquete de B:, desde fuera: el archivo entero y sin errores"; else ko "el disquete de B: no ha quedado bien"; cat $T/fsck.txt; fi
if fsck.fat -n $T/a.img > $T/fsck.txt 2>&1; then ok "y el de A:, sin errores"; else ko "el disquete de A: tiene errores"; cat $T/fsck.txt; fi
# 160, 180 y 320 K: FORMATEA los hace, y se leen también sin medidas en el
# primer sector (como los dejaba DOS 1), por el primer octeto de la FAT
for m in 160 180 320; do
  truncate -s 0 $T/b.img; truncate -s ${m}K $T/b.img
  QMAS="-drive file=$T/b.img,format=raw,if=floppy,index=1" arranca $T/a.img 486
  escribe "formatea b: /f:$m /a"
  if espera g5 "octetos libres" 60; then ok "$m K: FORMATEA acaba"; else ko "$m K: FORMATEA no acaba"; fi
  esta g5 "Formato de $m K." "$m K: "
  escribe "copiar leeme.txt b:"; sleep 2
  apaga
  QMAS=
  if fsck.fat -n $T/b.img > $T/fsck.txt 2>&1 && cmp -s <(mtype -i $T/b.img ::LEEME.TXT) build/LEEME.TXT; then ok "$m K: fsck.fat lo da por bueno y mtools lee lo copiado"; else ko "$m K: el disquete no ha quedado bien"; cat $T/fsck.txt; fi
  # sin medidas: el primer sector, a cero
  dd if=/dev/zero of=$T/b.img bs=512 count=1 conv=notrunc 2> /dev/null
  QMAS="-drive file=$T/b.img,format=raw,if=floppy,index=1" arranca $T/a.img 486
  escribe "limpiar"; escribe "dir b:"; escribe "copiar b:leeme.txt b:otro.txt"; sleep 2; escribe "mostrar b:otro.txt"
  pantalla g6
  esta g6 "LEEME.TXT" "$m K, sin medidas en el primer sector: DIR lo lee: "
  esta g6 "en español." "$m K, sin medidas: se escribe y se lee: "
  apaga
  QMAS=
done
}

p_editar() {
echo "== Hispa-8086: EDITAR =="
cp $IMG/hispa8086-360.img $T/a.img
arranca $T/a.img 486
escribe "limpiar"; escribe "editar"
pantalla e0
esta e0 "EDITAR archivo" "sin nombre, dice cómo se usa: "
escribe "editar nota.txt"; sleep 1
pantalla e0
esta e0 " F2 Guardar  F3 Buscar  F4 Copiar  F5 Cortar  F6 Pegar  F10 Salir   1:1 INS" "la línea de abajo: "
tecla u n o ret d o s ret t r e s; tecla up up end x f5 down f6 home insert shift-u; sleep 0.5
pantalla e1
esta e1 "Unox" "escribir, cortar una línea (F5), pegarla (F6) y sobrescribir: "
esta e1 "2:2 SOB*" "la línea, la columna, el modo y la marca de cambios: "
tecla f3 t r e s ret; sleep 0.5
# (la revisión en PCjs esperaba aquí «2:1»; pero «tres» es la tercera línea
# del texto, dos, Unox y tres: lo que sale, «3:1», es lo bueno, y tras juntar
# las líneas con Retroceso, «2:5»)
pantalla e1b
esta e1b " 3:1 SOB*" "F3 lleva el cursor a lo encontrado, y la línea de abajo lo dice: "
tecla backspace; sleep 0.5
pantalla e2
esta e2 "Unoxtres" "buscar (F3) y juntar dos líneas con Retroceso: "
esta e2 " 2:5 SOB*" "y la línea de abajo, al día: "
tecla f10; sleep 0.5
pantalla e3
esta e3 "Hay cambios sin guardar. ¿Guardarlos?" "al salir con cambios, pregunta: "
tecla s; sleep 1.5; escribe "mostrar nota.txt"; escribe "edit leeme.txt"; sleep 1
pantalla e4
esta e4 "El sistema de la familia Hispa para los PC, XT y AT: 8086, 8088, V20," "EDIT (el nombre clásico) abre un archivo que ya existe: "
tecla esc; sleep 1; escribe "ver"
pantalla e5
esta e5 "Hispa-8086 versión 0.12" "Esc sale sin preguntar si no hay cambios: "
apaga
if [ "$(mtype -i $T/a.img ::NOTA.TXT | xxd -p)" = "$(printf 'dos\r\nUnoxtres' | xxd -p)" ]; then ok "lo guardado (F2) es exactamente lo escrito, con sus retornos"; else ko "NOTA.TXT no es lo esperado: $(mtype -i $T/a.img ::NOTA.TXT | xxd -p)"; fi
if fsck.fat -n $T/a.img > $T/fsck.txt 2>&1; then ok "fsck.fat no encuentra ningún error en el disquete"; else ko "fsck.fat encuentra errores"; cat $T/fsck.txt; fi
}

p_teclado() {
echo "== Hispa-8086: TECLADO (el teclado español, residente) =="
cp $IMG/hispa8086-360.img $T/a.img
arranca $T/a.img 486
escribe "limpiar"; escribe "teclado /n"; escribe "memoria"; escribe "teclado"; sleep 1
tecla e c o spc semicolon shift-semicolon spc apostrophe a apostrophe e spc minus shift-2 spc alt_r-2 spc equal shift-equal spc shift-apostrophe u spc shift-comma shift-minus spc apostrophe spc backslash shift-backslash ret; sleep 1
tecla m e m o r i a ret; sleep 1; tecla k e y b spc shift-7 n ret; sleep 1; escribe "eco ;:"
pantalla t1
esta t1 "TECLADO no está en la memoria." "TECLADO /N sin haberlo puesto: "
esta t1 "Teclado español." ""
esta t1 "ñÑ áé '\" @ ¡¿ ü ;? 'çÇ" "la eñe, los acentos, AltGr, ¡ ¿ y la cedilla: "
esta t1 "Teclado normal (el de la BIOS)." "KEYB /N (el nombre clásico, tecleado en español): "
if [ "$(grep -c '^;:' $T/t1.txt)" = 1 ]; then ok "con /N las teclas vuelven a ser las de la BIOS"; else ko "TECLADO /N no devuelve el teclado normal"; fi
a=$(grep -o "programas: [0-9]*" $T/t1.txt | head -1 | tr -dc 0-9); b=$(grep -o "programas: [0-9]*" $T/t1.txt | tail -1 | tr -dc 0-9)
if [ -n "$a" ] && [ -n "$b" ] && [ $((a - b)) -gt 256 ] && [ $((a - b)) -le 1024 ]; then ok "se queda residente (INT 21h/31h) y ocupa $((a - b)) octetos"; else ko "memoria con TECLADO: de $a a $b octetos"; fi
apaga
}
