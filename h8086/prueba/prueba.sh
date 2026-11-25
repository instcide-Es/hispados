#!/bin/bash
# Prueba de los cimientos de Hispa-8086: arranca el disquete en QEMU, teclea
# órdenes y mira lo que queda escrito en la pantalla de texto.
#
# QEMU no tiene un 8086: lo que aquí se comprueba es que el sistema arranca
# y se comporta, en un 486 y en un Pentium. Que no hay ni una instrucción
# posterior al 8086 lo garantiza el ensamblador («cpu 8086» en los fuentes).
# Necesita qemu-system-i386, socat y mtools.
set -u
cd "$(dirname "$0")/.."
# (las fechas que se ponen con touch y se leen con mtools, en la misma zona
# horaria pase lo que pase en el equipo: con la de Madrid, 09:30 salía 11:30)
export TZ=UTC
T=$(mktemp -d)
fail=0
# MODO=alto ./prueba/prueba.sh pasa TODA la prueba con el núcleo en la memoria
# alta (como con DOS=ALTO en CONFIG.SYS), y MODO=8086, tratando el procesador
# como un 8086 (sin A20, HMA ni memoria extendida): se hace con copias de los
# disquetes en las que se cambia el octeto que el núcleo lleva para eso
# detrás de la marca «HCFG:»
IMG=${IMG:-build}
if [ -n "${MODO:-}" ]; then
  IMG=$T/img; mkdir -p $IMG
  python3 - $IMG "$MODO" build/hispa8086-*.img <<'PY'
import os, sys
dst, modo = sys.argv[1], sys.argv[2]
for f in sys.argv[3:]:
    d = bytearray(open(f, 'rb').read())
    i = d.index(b'HCFG:')
    d[i + 5] = {'alto': 1, '8086': 2}[modo]
    open(os.path.join(dst, os.path.basename(f)), 'wb').write(d)
PY
fi
ok() { [ -n "${SIN_QEMU:-}" ] && return; echo "  OK   $1" | tee -a $T/salida.txt; }
ko() { [ -n "${SIN_QEMU:-}" ] && return; echo "  FAIL $1" | tee -a $T/salida.txt; fail=1; }
: > $T/salida.txt
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; sleep "${2:-0.25}"; }
tecla() { local k; for k in "$@"; do mon "sendkey $k" 0.12; done; }
escribe() { teclea "$1"; tecla ret; sleep 0.8; }
teclea() {    # como escribe, pero sin el Intro del final
  local t="$1" i c
  for ((i=0; i<${#t}; i++)); do
    c="${t:$i:1}"
    case "$c" in ' ') tecla spc;; '.') tecla dot;; '*') tecla shift-8;; '<') tecla shift-comma;; '>') tecla shift-dot;; '=') tecla equal;; '/') tecla slash;; ':') tecla shift-semicolon;; '\') tecla backslash;;
      '|') tecla shift-backslash;; ';') tecla semicolon;; '$') tecla shift-4;; '"') tecla shift-apostrophe;; '%') tecla shift-5;; '(') tecla shift-9;; ')') tecla shift-0;; ',') tecla comma;; '-') tecla minus;; '+') tecla shift-equal;; '_') tecla shift-minus;; [A-Z]) tecla "shift-${c,,}";; *) tecla "$c";; esac
  done
}
# la pantalla de texto (80x25, un octeto de letra y otro de color), a texto
pantalla() {
  rm -f $T/scr.bin
  mon "pmemsave 0xb8000 4000 \"$T/scr.bin\"" 0.4
  python3 - $T/scr.bin <<'PY' > $T/$1.txt
import os, sys
d = open(sys.argv[1], 'rb').read() if os.path.exists(sys.argv[1]) else b''
for f in range(25):
    print(bytes(d[f * 160:(f + 1) * 160:2]).decode('cp437').rstrip())
PY
}
esta() { if grep -qF -- "$2" $T/$1.txt; then ok "$3«$2»"; else ko "$3falta «$2»"; fi; }
noesta() { if grep -qF -- "$2" $T/$1.txt; then ko "$3sobra «$2»"; else ok "$3no sale «$2»"; fi; }
arranca() {   # imagen del disquete, cpu y, si se quiere, imagen del disco duro
  rm -f $T/mon  # (en QMAS, lo que se le quiera añadir a QEMU: otra disquetera...)
  qemu-system-i386 -display none -monitor unix:$T/mon,server,nowait -cpu $2 -m ${QMEM:-4} \
    -drive file=$1,format=raw,if=floppy,index=0 ${3:+-drive file=$3,format=raw,if=ide,index=0} \
    -serial unix:$T/serie,server,nowait ${QMAS:-} \
    -boot a -no-reboot > /dev/null 2>&1 &
  QPID=$!
  qemu_vive
}
# espera (hasta 20 s) a que conteste el monitor de QEMU, y luego los 5 s del
# arranque. Si no contesta, un solo FAIL y lo demás de la parte (hasta el
# apaga) no cuenta: con el equipo muy cargado, un QEMU que tardaba en
# arrancar daba decenas de FAIL que parecían del sistema
qemu_vive() {
  local i
  SIN_QEMU=
  for ((i=0; i<40; i++)); do
    printf 'info status\n' | socat - UNIX-CONNECT:$T/mon 2> /dev/null | grep -q "VM status" && { sleep 5; return 0; }
    kill -0 $QPID 2> /dev/null || break
    sleep 0.5
  done
  ko "QEMU no arranca (o su monitor no contesta): lo que queda de esta parte se salta"
  SIN_QEMU=1
  return 1
}
arranca_duro() {   # solo con el disco duro, sin disquete: imagen y cpu
  rm -f $T/mon
  qemu-system-i386 -display none -monitor unix:$T/mon,server,nowait -cpu $2 -m ${QMEM:-4} \
    -drive file=$1,format=raw,if=ide,index=0 \
    -serial unix:$T/serie,server,nowait \
    -boot c -no-reboot > /dev/null 2>&1 &
  QPID=$!
  qemu_vive
}
# espera (hasta $3 segundos, 30 si no se dice) a que en la pantalla salga $2
espera() {
  local i
  for ((i=0; i<${3:-30}; i++)); do pantalla $1; grep -qF -- "$2" $T/$1.txt && return 0; sleep 1; done
  return 1
}
apaga() { mon quit 0.5; wait $QPID 2>/dev/null; SIN_QEMU=; }

# ---- las partes de la prueba: cada una arranca y apaga lo suyo. Con
# SOLO="duro medidas" ./prueba/prueba.sh se corren solo las que se digan ----
p_basico() {
echo "== Hispa-8086: arranque y órdenes (disquete de 360 K, en un 486) =="
cp $IMG/hispa8086-360.img $T/a.img
arranca $T/a.img 486
pantalla p1
esta p1 "Hispa-8086 0.13 - de la familia Hispa-DOS" ""
esta p1 "Copyright (C) 2026 Carlos Sanz y contribuidores" ""
esta p1 "Procesador: 80486 o superior" ""
esta p1 "K libres para los programas)" ""
esta p1 "puede con HISPA-386" "el consejo: "
esta p1 'A:\>' ""
esta p1 "Arranque terminado (AUTOEXEC.BAT)." "AUTOEXEC.BAT al arrancar: "
noesta p1 "ECO OFF" "«@» delante: la línea no se enseña: "
noesta p1 "REM El arranque" "con ECO OFF no se enseñan las órdenes del lote: "
escribe "limpiar"; escribe "ver"
pantalla p2
esta p2 "Hispa-8086 versión 0.13" "VER: "
escribe "limpiar"; escribe "dir *.sys"; escribe "dir h*.*"; escribe "dir c*.com"; escribe "dir"
pantalla p2
esta p2 "23 archivos, " "DIR: "
escribe "limpiar"; escribe "dir *.sys"; escribe "dir h*.*"; escribe "dir c*.com"
pantalla p2
esta p2 "NUCLEO86.SYS" "DIR: "
esta p2 "HOLA.COM             206" "DIR: "
esta p2 "COMMAND.COM" "DIR: "
esta p2 "octetos libres" "DIR: "
noesta p2 "HISPA-8086 " "DIR: (la etiqueta) "
escribe "cls"; escribe "mostrar leeme.txt"
pantalla p3
esta p3 "El sistema de la familia Hispa para los PC, XT y AT: 8086, 8088, V20," "MOSTRAR: "
esta p3 "en español." "MOSTRAR: "
escribe "limpiar"; escribe "hola uno dos"; escribe "type nada.txt"; escribe "xyz"; escribe "equipo"
pantalla p4
esta p4 "Hola desde un programa .COM. DOS 3.30" "HOLA.COM: "
esta p4 "Parametros: uno dos" "HOLA.COM: "
esta p4 "Vectores: bien" "HOLA.COM: "
esta p4 "No existe ese archivo." ""
esta p4 "Orden desconocida. Escriba AYUDA." ""
esta p4 "Procesador: 80486 o superior" "EQUIPO: "

echo "== Hispa-8086: archivos por lotes y redirección =="
escribe "limpiar"; escribe "lote uno dos"; sleep 6
pantalla pl
esta pl "Lote: empiezo LOTE [uno] [dos] [] 100%" "%0, %1, %2 y %%: "
esta pl "Parametros: desde el lote" "lanza un programa: "
esta pl "Nivel: 7 o mas" "IF ERRORLEVEL: "
esta pl "Nivel: menos de 8" "IF NOT ERRORLEVEL: "
esta pl "Existe leeme" "IF EXIST: "
esta pl "No existe nohay" "IF NOT EXIST: "
esta pl "Lote: acabo" "GOTO y hasta el final: "
noesta pl "NO DEBE SALIR" "lo que no se cumple no se ejecuta: "
noesta pl "Primera linea" "con «>», a la pantalla no sale: "
escribe "limpiar"; escribe "mostrar sale.txt"; escribe "pausa < leeme.txt"; escribe "eco tras la pausa"
escribe "eco tecleado > t.txt"; escribe "if a==a mostrar t.txt"
pantalla pr
esta pr "Primera linea" "«>»: lo escrito está en el archivo: "
esta pr "Hispa-8086 versión 0.13" "«>>»: y lo añadido detrás: "
esta pr "pausa < leeme.txt" "(la orden, como se tecleó) "
esta pr "Pulse una tecla para seguir..." "«<»: la tecla sale del archivo: "
noesta pr "Orden desconocida" "y la orden siguiente llega entera: "
esta pr "tras la pausa" ""
esta pr "if a==a mostrar t.txt" "(la orden, como se tecleó) "
if [ "$(grep -c '^tecleado' $T/pr.txt)" = 1 ]; then ok "«>» desde el teclado e IF a==a: el texto solo sale al mostrar el archivo"; else ko "«>» desde el teclado"; fi

echo "== Hispa-8086: memoria, programas .EXE y lanzar otro programa =="
escribe "limpiar"; escribe "exe"; sleep 4; escribe "equipo"
pantalla pe
esta pe "EXE: datos bien" "un .EXE con sus direcciones reubicadas: "
esta pe "EXE: llamada lejana bien" ""
esta pe "EXE: memoria bien" "pedir, soltar y encoger memoria: "
esta pe 'EXE: soy A:\EXE.EXE' "su ruta, detrás del entorno: "
esta pe "Parametros: hijo" "lanza HOLA.COM con parámetros: "
esta pe "EXE: el hijo acaba con 7" "y recoge su código de salida: "
noesta pe "EXE: MAL" ""
libres=$(grep -o "([0-9]* K libres" $T/pe.txt | tr -dc 0-9)
antes=$(grep -o "([0-9]* K libres" $T/p1.txt | tr -dc 0-9)
if [ -n "$libres" ] && [ "$libres" = "$antes" ]; then ok "al acabar, la memoria vuelve a estar libre entera ($libres K)"; else ko "memoria libre: $antes K antes, $libres K después"; fi

echo "== Hispa-8086: archivos y directorios =="
TL=$(stat -c %s build/LEEME.TXT)      # lo que ocupa el archivo con el que se juega
fila() { printf '%-14s%10d' "$1" "$2"; }
escribe "limpiar"; escribe "arch"; sleep 6
pantalla p6
esta p6 "ARCHIVOS: BIEN" "la interfaz de INT 21h, paso a paso: "
# y con las órdenes: crear un directorio, copiar dentro, entrar, renombrar,
# copiar otra vez, borrar con comodín, salir
escribe "limpiar"; escribe "creardir cartas"; escribe "copiar leeme.txt cartas"; sleep 2
escribe "cd cartas"; escribe "renombrar leeme.txt juan.txt"; escribe "copy juan.txt ana.txt"; sleep 2
escribe "copiar juan.txt pepe.doc"; sleep 2; escribe "dir"
pantalla p7
esta p7 'A:\CARTAS>' "CD: "
esta p7 "1 archivo copiado." "COPIAR: "
esta p7 "$(fila JUAN.TXT $TL)" "RENOMBRAR y DIR: "
esta p7 "$(fila ANA.TXT $TL)" "COPIAR y DIR: "
esta p7 "5 archivos, $((3 * TL)) octetos" "DIR del directorio: "
escribe "limpiar"; escribe "borrar *.txt"; sleep 2; escribe "dir"
pantalla p8
noesta p8 "JUAN.TXT" "BORRAR *.TXT: "
esta p8 "$(fila PEPE.DOC $TL)" "BORRAR *.TXT deja lo demás: "
escribe "limpiar"; escribe "cd .."; escribe "borrardir cartas"
escribe "mostrar cartas\\pepe.doc"; escribe "cd nohay"; escribe "cd"
pantalla p9
esta p9 "No se puede: acceso denegado." "BORRARDIR con algo dentro: "
esta p9 "en español." "MOSTRAR con ruta: "
esta p9 "No existe esa ruta." "CD a donde no hay: "
apaga
# lo que ha quedado en el disquete, mirado desde fuera
if python3 - $T/a.img <<'PY'
import subprocess, sys
img = sys.argv[1]
def cat(n):
    return subprocess.run(['mtype', '-i', img, '::' + n], capture_output=True).stdout
dib = bytes((i * 7 + 3) & 255 for i in range(5000))
leeme = cat('LEEME.TXT')
ok = cat('GRANDE.DAT') == dib * 4 and cat('PRUEBA.DAT') == dib[:500] and cat('SUB/TRES.TXT') == b'hola'
ok = ok and len(leeme) > 100 and cat('CARTAS/PEPE.DOC') == leeme
dentro = subprocess.run(['mdir', '-b', '-i', img, '::CARTAS'], capture_output=True).stdout.decode()
ok = ok and 'PEPE.DOC' in dentro and 'JUAN' not in dentro and 'ANA' not in dentro
sys.exit(0 if ok else 1)
PY
then ok "el disquete, visto desde fuera: cada archivo con su contenido exacto"; else ko "el disquete no ha quedado como se esperaba"; mdir -/ -i $T/a.img :: | head -30; fi
if command -v fsck.fat > /dev/null; then
  if fsck.fat -n $T/a.img > $T/fsck.txt 2>&1; then ok "fsck.fat no encuentra ningún error en el disquete"; else ko "fsck.fat encuentra errores"; cat $T/fsck.txt; fi
fi
}

p_pasar() {
echo "== Hispa-8086: PASAR, archivos por el puerto serie (XMODEM) =="
cp $IMG/hispa8086-360.img $T/a.img
arranca $T/a.img 486
# de fuera adentro: 3.000 octetos de todos los valores, con el CRC
python3 -c "import sys; sys.stdout.buffer.write(bytes((i * 13 + 5) & 255 for i in range(3000)))" > $T/ida.bin
escribe "limpiar"; escribe "pasar recibir ida.bin"
if python3 prueba/xmodem.py $T/serie enviar $T/ida.bin; then ok "el otro lado envía y PASAR RECIBIR lo da todo por bueno"; else ko "PASAR RECIBIR"; fi
sleep 2
# de dentro afuera, con el CRC y con la suma de 8 bits de los XMODEM antiguos
escribe "pasar enviar leeme.txt"
if python3 prueba/xmodem.py $T/serie recibir $T/vuelta.txt; then ok "PASAR ENVIAR y el otro lado lo recibe (con CRC)"; else ko "PASAR ENVIAR"; fi
sleep 2
escribe "pasar enviar ida.bin /v:19200"
sleep 1
if python3 prueba/xmodem.py $T/serie recibir $T/vuelta.bin suma; then ok "PASAR ENVIAR con la suma de 8 bits (y a 19.200)"; else ko "PASAR ENVIAR con suma"; fi
sleep 2
escribe "pasar"; escribe "pasar recibir x.txt /v:7"
pantalla px
esta px "Recibido: 3000 octetos." "lo recibido, con su tamaño exacto: "
esta px "Enviado: " ""
esta px "PASAR - pasa archivos por el puerto serie (XMODEM)" "sin nada, dice cómo se usa: "
esta px "Esa velocidad no vale" ""
apaga
if cmp -s <(mtype -i $T/a.img ::IDA.BIN) $T/ida.bin; then ok "lo recibido está en el disquete, octeto a octeto"; else ko "IDA.BIN no es lo enviado"; fi
if cmp -s $T/vuelta.txt build/LEEME.TXT; then ok "lo enviado llega igual (LEEME.TXT)"; else ko "LEEME.TXT no llega igual"; fi
if cmp -s $T/vuelta.bin $T/ida.bin; then ok "ida y vuelta: el archivo vuelve idéntico"; else ko "el archivo no vuelve idéntico"; fi
}

p_duro() {
echo "== Hispa-8086: el disco duro (C:, una partición FAT16 de 20 megas) =="
cp $IMG/hispa8086-360.img $T/a.img
python3 prueba/haz_duro.py $T/hd.img > /dev/null
arranca $T/a.img 486 $T/hd.img
escribe "limpiar"; escribe "dir c:"
pantalla phc
esta phc "20434944 octetos libres" "DIR C: desde A: dice lo libre de C:, no lo de A:: "
escribe "limpiar"; escribe "c:"; escribe "dir a:hola.com"
pantalla pha
noesta pha "20434944 octetos libres" "y DIR A: desde C:, lo de A:: "
esta pha "octetos libres" ""
escribe "limpiar"; escribe "dir"
pantalla ph0
esta ph0 "0 archivos, 0 octetos" "DIR del disco vacío: "
esta ph0 "20434944 octetos libres" "y su sitio libre: "
escribe "limpiar"; escribe "copiar a:leeme.txt c:"; sleep 3
escribe "creardir datos"; escribe "copy a:\\hola.com datos"; sleep 3; escribe "a:arch"; sleep 8
escribe "datos\\hola desde c"; escribe "dir"; escribe "z:"
pantalla ph
esta ph 'C:\>' "«C:» cambia de unidad: "
esta ph "ARCHIVOS: BIEN" "la interfaz de archivos, sobre FAT16: "
esta ph "Parametros: desde c" "un programa cargado del disco duro: "
esta ph "GRANDE.DAT         20000" ""
esta ph "No existe esa unidad." "una unidad que no hay: "
escribe "a:"; escribe "dir c:\\datos"
pantalla ph2
esta ph2 "HOLA.COM             206" "DIR de otra unidad, con ruta: "
esta ph2 'A:\>' "y de vuelta en A: "
apaga
dd if=$T/hd.img of=$T/part.img bs=512 skip=63 2> /dev/null
if python3 - $T/part.img build/LEEME.TXT build/HOLA.COM <<'PY'
import subprocess, sys
img = sys.argv[1]
def cat(n):
    return subprocess.run(['mtype', '-i', img, '::' + n], capture_output=True).stdout
dib = bytes((i * 7 + 3) & 255 for i in range(5000))
ok = cat('GRANDE.DAT') == dib * 4 and cat('PRUEBA.DAT') == dib[:500] and cat('SUB/TRES.TXT') == b'hola'
ok = ok and cat('LEEME.TXT') == open(sys.argv[2], 'rb').read() and cat('DATOS/HOLA.COM') == open(sys.argv[3], 'rb').read()
sys.exit(0 if ok else 1)
PY
then ok "la partición, vista desde fuera: cada archivo con su contenido exacto"; else ko "la partición no ha quedado como se esperaba"; mdir -/ -i $T/part.img :: | head -30; fi
if fsck.fat -n $T/part.img > $T/fsck.txt 2>&1; then ok "fsck.fat no encuentra ningún error en la partición"; else ko "fsck.fat encuentra errores en la partición"; cat $T/fsck.txt; fi
}

p_medidas() {
echo "== Hispa-8086: las demás medidas de disquete (720 K, 1,2 M y 1,44 M) =="
for m in 720 1200 1440; do
  cp $IMG/hispa8086-$m.img $T/b.img
  arranca $T/b.img pentium
  escribe "hola"; escribe "arch"; sleep 7; escribe "dir grande.dat"
  pantalla p5
  esta p5 "Hola desde un programa .COM. DOS 3.30" "$m K: "
  esta p5 "ARCHIVOS: BIEN" "$m K: "
  esta p5 "GRANDE.DAT         20000" "$m K: "
  apaga
  if fsck.fat -n $T/b.img > $T/fsck.txt 2>&1; then ok "$m K: fsck.fat no encuentra ningún error"; else ko "$m K: fsck.fat encuentra errores"; cat $T/fsck.txt; fi
done
# el sistema base tiene que caber en el disquete de los XT
libre=$(mdir -i $IMG/hispa8086-360.img :: | awk '/bytes free/ { gsub(/ /, "", $0); sub(/bytesfree/, "", $0); print }')
if [ "${libre:-0}" -gt 100000 ]; then ok "en el disquete de 360 K quedan $libre octetos libres"; else ko "el disquete de 360 K se queda sin sitio ($libre)"; fi
}

p_fuentes() {
echo "== Hispa-8086: solo instrucciones del 8086 =="
for f in ordenes/xcopiar; do
  if grep -q "cpu 286\|cpu 386" $f.asm; then ko "$f.asm lleva instrucciones de 286"; fi
done
if grep -q "^\s*cpu 286" *.asm *.inc ordenes/*.asm ordenes/*.inc; then ko "hay tramos de 286 (no debería: todo va por la BIOS)"; else ok "ni una instrucción de 286: la memoria extendida se maneja con la BIOS (INT 15h)"; fi
for f in arranque mbr nucleo command ordenes/pasar ordenes/formatea ordenes/sistema ordenes/partir ordenes/mas ordenes/ordenar \
         ordenes/revisar ordenes/etiqueta ordenes/atributo ordenes/arbol ordenes/xcopiar ordenes/editar ordenes/teclado ordenes/discoram; do
  if grep -q "^\s*cpu 8086" $f.asm; then ok "$f.asm se ensambla con «cpu 8086»"; else ko "$f.asm no lleva «cpu 8086»"; fi
done
# lo único de 386 es la prueba que distingue el 386 del 486, que solo corre
# cuando ya se sabe que el procesador es de 32 bits
n=$(grep -c "^\s*cpu 386" nucleo.asm)
if [ "$n" = 1 ]; then ok "un solo tramo de 386 (detrás de la prueba del 286)"; else ko "tramos de 386: $n"; fi
tam=$(stat -c %s build/NUCLEO86.SYS)
if [ $tam -lt 16384 ]; then ok "el núcleo ocupa $tam octetos"; else ko "el núcleo ha crecido a $tam octetos"; fi
tam=$(stat -c %s build/COMMAND.COM)
if [ $tam -lt 16384 ]; then ok "el intérprete ocupa $tam octetos"; else ko "el intérprete ha crecido a $tam octetos"; fi
}

p_formatea() {
echo "== Hispa-8086: FORMATEA, disquetes =="
cp $IMG/hispa8086-360.img $T/a.img
rm -f $T/b.img; truncate -s 368640 $T/b.img     # un disquete de 360 K sin nada
QMAS="-drive file=$T/b.img,format=raw,if=floppy,index=1" arranca $T/a.img 486
escribe "limpiar"; escribe "formatea"; escribe "formatea z:"; sleep 1; escribe "dir b:"; sleep 2
pantalla f0
esta f0 "FORMATEA unidad: [/S] [/V:etiqueta] [/F:medida] [/R] [/A]" "sin nada, dice cómo se usa: "
esta f0 "No existe esa unidad." ""
esta f0 "La unidad no está preparada." "el disquete nuevo, antes del formato: "
escribe "limpiar"; escribe "formatea b: /f:360"
pantalla f1
esta f1 "se va a BORRAR todo lo que haya en el disquete de la unidad B:" "avisa y pregunta: "
tecla n; sleep 1
pantalla f1
esta f1 "No se ha formateado nada." "con «n» no hace nada: "
escribe "limpiar"; escribe "formatea b: /f:360 /v:pruebas"; tecla s
if espera f2 "octetos libres" 60; then ok "FORMATEA B: acaba"; else ko "FORMATEA B: no acaba"; fi
esta f2 "Formato de 360 K." ""
esta f2 "Formato terminado." ""
esta f2 "362496 octetos en total" "lo que dice del disquete nuevo: "
esta f2 "362496 octetos libres" ""
escribe "limpiar"; escribe "copiar leeme.txt b:"; sleep 2; escribe "dir b:"
pantalla f3
esta f3 "1 archivo copiado." "el sistema usa el disquete recién formateado: "
esta f3 "1 archivos, " ""
apaga
QMAS=
if fsck.fat -n $T/b.img > $T/fsck.txt 2>&1; then ok "fsck.fat da por bueno el disquete formateado"; else ko "fsck.fat encuentra errores en el disquete formateado"; cat $T/fsck.txt; fi
if cmp -s <(mtype -i $T/b.img ::LEEME.TXT) build/LEEME.TXT; then ok "Linux (mtools) lee lo copiado al disquete nuevo"; else ko "mtools no lee el disquete nuevo"; fi
if mdir -i $T/b.img :: 2> /dev/null | grep -q "PRUEBAS"; then ok "la etiqueta (/V:) está puesta"; else ko "falta la etiqueta"; mdir -i $T/b.img :: | head -5; fi
# otra vez, sin decir la medida y rápido: conserva la que tenía y lo deja vacío;
# y un disquete sin formato no admite el rápido
truncate -s 737280 $T/c.img
QMAS="-drive file=$T/b.img,format=raw,if=floppy,index=1" arranca $T/a.img 486
escribe "limpiar"; escribe "formatea b: /r /a"
if espera f4 "octetos libres" 30; then ok "FORMATEA /R /A (rápido y sin preguntar) acaba"; else ko "FORMATEA /R /A no acaba"; fi
esta f4 "Formato de 360 K." "sin /F conserva la medida del disquete: "
esta f4 "362496 octetos libres" "y queda vacío: "
noesta f4 "pulse S" "con /A no pregunta: "
mon "change floppy1 $T/c.img raw" 2
escribe "limpiar"; escribe "formatea b: /r /a"
pantalla f5
esta f5 "El formato rápido solo vale para un disquete que ya tiene formato." ""
apaga
QMAS=
if fsck.fat -n $T/b.img > $T/fsck.txt 2>&1 && [ -z "$(mdir -b -i $T/b.img :: 2> /dev/null)" ]; then ok "tras el formato rápido, fsck.fat lo da por bueno y está vacío"; else ko "el formato rápido no deja bien el disquete"; cat $T/fsck.txt; fi

# las otras medidas; la de 1,44 M, sin decirla (la mayor de la unidad) y con /S
for m in 720 1200 1440; do
  truncate -s 0 $T/b.img; truncate -s ${m}K $T/b.img
  [ $m = 1200 ] && truncate -s 1228800 $T/b.img
  QMAS="-drive file=$T/b.img,format=raw,if=floppy,index=1" arranca $T/a.img 486
  case $m in
    720) escribe "formatea b: /f:720 /a"; total=730112;;
    1200) escribe "formatea b: /f:1200 /a"; total=1213952;;
    1440) escribe "formatea b: /a /s /v:arranque"; total=1457664;;
  esac
  if espera f6 "octetos libres" 90; then ok "$m K: FORMATEA acaba"; else ko "$m K: FORMATEA no acaba"; fi
  esta f6 "Formato de $m K." "$m K: "
  esta f6 "$total octetos en total" "$m K: "
  [ $m = 1440 ] && esta f6 "Sistema transferido." "$m K, con /S: "
  escribe "copiar hola.com b:"; sleep 2
  apaga
  QMAS=
  if fsck.fat -n $T/b.img > $T/fsck.txt 2>&1; then ok "$m K: fsck.fat da por bueno el disquete formateado"; else ko "$m K: fsck.fat encuentra errores"; cat $T/fsck.txt; fi
  if cmp -s <(mtype -i $T/b.img ::HOLA.COM) build/HOLA.COM; then ok "$m K: mtools lee lo copiado"; else ko "$m K: mtools no lee lo copiado"; fi
done
# el de 1,44 M se formateó con /S: tiene que arrancar solo
arranca $T/b.img 486
escribe "limpiar"; escribe "dir"; escribe "hola desde el nuevo"
pantalla f7
esta f7 'A:\>' "el disquete hecho con FORMATEA /S arranca: "
esta f7 "NUCLEO86.SYS" ""
esta f7 "COMMAND.COM" ""
esta f7 "Parametros: desde el nuevo" "y ejecuta programas: "
apaga

# una pista mala (la 20 de la cara 0): sus grupos quedan apuntados como
# defectuosos y el disquete sirve con lo demás. QEMU ni graba las pistas (dice
# que sí y no hace nada) ni sabe devolver un error al leer un disquete, así
# que la pista mala se le dice a FORMATEA con /M:, que solo está para esto
truncate -s 0 $T/b.img; truncate -s 368640 $T/b.img
QMAS="-drive file=$T/b.img,format=raw,if=floppy,index=1" arranca $T/a.img 486
escribe "limpiar"; escribe "formatea b: /f:360 /a /m:20"
if espera f8 "octetos libres" 60; then ok "con una pista mala, FORMATEA acaba"; else ko "con una pista mala, FORMATEA no acaba"; fi
esta f8 "5120 octetos en sectores defectuosos" "la pista mala (9 sectores, 5 grupos): "
esta f8 "357376 octetos libres" ""
apaga
QMAS=
if fsck.fat -n $T/b.img > $T/fsck.txt 2>&1; then ok "fsck.fat da por bueno el disquete con grupos defectuosos"; else ko "fsck.fat encuentra errores (disquete con pista mala)"; cat $T/fsck.txt; fi
if python3 - $T/b.img <<'PY2'
import sys
d = open(sys.argv[1], 'rb').read()
fat = d[512:512 + 2 * 512]
def ent(n):
    o = n * 3 // 2
    v = fat[o] | fat[o + 1] << 8
    return (v >> 4) if n & 1 else (v & 0xFFF)
malos = [n for n in range(2, 356) if ent(n) == 0xFF7]
sys.exit(0 if malos == [176, 177, 178, 179, 180] and d[512:1536] == d[1536:2560] else 1)
PY2
then ok "en la FAT (las dos copias) están marcados justo los grupos de esa pista"; else ko "los grupos defectuosos no son los esperados"; fi
}

# la tabla de particiones de una imagen, desde fuera: «activa tipo inicio tamaño» de la fila $2
fila_part() {
  python3 - $1 $2 <<'PY2'
import struct, sys
d = open(sys.argv[1], 'rb').read(512)
e = d[0x1BE + 16 * (int(sys.argv[2]) - 1):][:16]
print('%02x %02x %d %d %s' % (e[0], e[4], struct.unpack('<I', e[8:12])[0], struct.unpack('<I', e[12:16])[0], d[510:512].hex()))
PY2
}

p_partir() {
echo "== Hispa-8086: PARTIR, FORMATEA C: /S y arrancar del disco duro (20 megas, FAT16) =="
cp $IMG/hispa8086-360.img $T/a.img
rm -f $T/hd.img; truncate -s $((40 * 16 * 63 * 512)) $T/hd.img     # un disco nuevo, a cero (la BIOS se guarda un cilindro)
arranca $T/a.img 486 $T/hd.img
escribe "limpiar"; escribe "partir"; escribe "dir c:"
pantalla k0
esta k0 "Disco duro: 39 cilindros, 16 caras, 63 sectores por pista: 19 megas" "PARTIR enseña el disco: "
esta k0 "No hay ninguna partición." ""
esta k0 "No existe esa unidad." "sin partición no hay C:: "
escribe "limpiar"; escribe "formatea c:"; escribe "partir crear"
pantalla k1
esta k1 "El disco duro no tiene partición de DOS: créela con PARTIR." "FORMATEA C: sin partición: "
esta k1 "Se va a crear una partición de DOS de 19 megas en el disco duro." "PARTIR CREAR avisa y pregunta: "
tecla n; sleep 1; escribe "partir crear"; tecla s; sleep 2; escribe "partir"; escribe "dir c:"
pantalla k2
esta k2 "No se ha tocado nada." "con «n» no hace nada: "
esta k2 "Partición creada y activa. Ahora dele formato: FORMATEA C: /S" ""
esta k2 " 1  sí      DOS, FAT16         04h  0-38  19 megas" "la tabla, con la partición nueva: "
esta k2 "No existe esa unidad." "sin formato, todavía no hay C:: "
escribe "limpiar"; escribe "partir crear"; escribe "formatea c: /s /v:duro"
pantalla k3
esta k3 "El disco ya tiene particiones: hay que borrarlas antes" "CREAR otra vez: "
esta k3 "se va a BORRAR todo lo que hay en el disco duro C: (19 megas)." "FORMATEA C: avisa y pregunta: "
tecla s
if espera k4 "octetos libres" 60; then ok "FORMATEA C: /S acaba"; else ko "FORMATEA C: /S no acaba"; fi
esta k4 "Formato terminado." ""
esta k4 "Sistema transferido." ""
escribe "limpiar"; escribe "copiar hola.com c:"; sleep 2; escribe "copiar leeme.txt c:"; sleep 2; escribe "dir c:"
pantalla k5
esta k5 "NUCLEO86.SYS" "C: recién formateado, con el sistema: "
esta k5 "COMMAND.COM" ""
esta k5 "4 archivos, " ""
apaga
if [ "$(fila_part $T/hd.img 1)" = "80 04 63 39249 55aa" ]; then ok "la tabla, desde fuera: activa, tipo 04, del sector 63 al final"; else ko "la tabla de particiones no es la esperada: $(fila_part $T/hd.img 1)"; fi
dd if=$T/hd.img of=$T/part.img bs=512 skip=63 2> /dev/null
if fsck.fat -n $T/part.img > $T/fsck.txt 2>&1; then ok "fsck.fat da por buena la partición formateada"; else ko "fsck.fat encuentra errores en la partición"; cat $T/fsck.txt; fi
if grep -q "FAT16" <(file $T/part.img) || minfo -i $T/part.img :: 2> /dev/null | grep -q 'disk type="FAT16'; then ok "es FAT16"; else ko "no es FAT16"; fi
if cmp -s <(mtype -i $T/part.img ::NUCLEO86.SYS) <(mtype -i $IMG/hispa8086-360.img ::NUCLEO86.SYS) && cmp -s <(mtype -i $T/part.img ::COMMAND.COM) build/COMMAND.COM && cmp -s <(mtype -i $T/part.img ::LEEME.TXT) build/LEEME.TXT; then ok "mtools lee el sistema y lo copiado, idénticos"; else ko "los archivos de C: no son los esperados"; fi
if mdir -i $T/part.img :: 2> /dev/null | grep -q "DURO"; then ok "la etiqueta (/V:) está puesta"; else ko "falta la etiqueta de C:"; fi
# y ahora, sin disquete: tiene que arrancar solo
arranca_duro $T/hd.img 486
pantalla k6
esta k6 "Hispa-8086 0.13 - de la familia Hispa-DOS" "ARRANCA DEL DISCO DURO, sin disquete: "
esta k6 'C:\>' "y se queda en C:: "
escribe "limpiar"; escribe "dir"; escribe "hola desde el duro"; escribe "mostrar leeme.txt"
pantalla k7
esta k7 "4 archivos, " "DIR en C:, arrancado de él: "
esta k7 "Parametros: desde el duro" "ejecuta programas: "
esta k7 "en español." "MOSTRAR: "
escribe "limpiar"; escribe "creardir datos"; escribe "copiar leeme.txt datos"; sleep 2; escribe "dir datos"
pantalla k8
esta k8 "1 archivo copiado." "y escribe en él: "
esta k8 "LEEME.TXT" ""
apaga
dd if=$T/hd.img of=$T/part.img bs=512 skip=63 2> /dev/null
if fsck.fat -n $T/part.img > $T/fsck.txt 2>&1; then ok "después de usarlo, fsck.fat sigue sin encontrar errores"; else ko "fsck.fat encuentra errores tras usar C:"; cat $T/fsck.txt; fi
# ACTIVAR y BORRAR
arranca $T/a.img 486 $T/hd.img
escribe "limpiar"; escribe "partir activar 2"; escribe "partir activar 1 /a"; escribe "partir borrar 1"
pantalla k9
esta k9 "Esa partición no existe." "ACTIVAR una que no hay: "
esta k9 "Partición activada." ""
esta k9 "se va a BORRAR la partición 1 (19 megas) con todo lo que tiene." "BORRAR avisa y pregunta: "
tecla s; sleep 2; escribe "partir"; escribe "dir c:"
pantalla k10
esta k10 "Partición borrada." ""
esta k10 "No hay ninguna partición." ""
esta k10 "No existe esa unidad." "y C: deja de existir: "
apaga

# un disco como el de un XT: 306 cilindros, 4 caras y 17 sectores (10 megas).
# PARTIR dice lo que diga la BIOS (INT 13h/08h: la última cara, el último
# sector y el último cilindro, más uno cada uno)
rm -f $T/hd.img; truncate -s $((306 * 4 * 17 * 512)) $T/hd.img
QMAS="-drive if=none,id=xt,file=$T/hd.img,format=raw -device ide-hd,drive=xt,cyls=306,heads=4,secs=17" arranca $T/a.img 486
escribe "limpiar"; escribe "partir crear /a"; sleep 2; escribe "partir"; escribe "formatea c: /s /a"
if espera k13 "octetos libres" 60; then ok "disco de 306 cilindros, 4 caras y 17 sectores: PARTIR y FORMATEA C: /S acaban"; else ko "disco de 306:4:17: no acaban"; fi
esta k13 " 4 caras, 17 sectores por pista: 10 megas" "la geometría que da la BIOS (4 caras, 17 sectores, 10 megas): "
esta k13 "DOS, FAT12         01h" ""
apaga
QMAS=
ini=$(fila_part $T/hd.img 1 | cut -d' ' -f3); tam=$(fila_part $T/hd.img 1 | cut -d' ' -f4)
dd if=$T/hd.img of=$T/part.img bs=512 skip=$ini count=$tam 2> /dev/null
if [ "$ini" = 17 ] && [ $((tam % 68)) = 51 ] && fsck.fat -n $T/part.img > $T/fsck.txt 2>&1 && [ "$(python3 -c "import sys,struct; d=open(sys.argv[1],'rb').read(64); print(struct.unpack('<HH', d[24:28]))" $T/part.img)" = "(17, 4)" ]; then ok "la partición empieza en la segunda pista (sector 17), acaba en cilindro entero y su BPB dice 17 sectores y 4 caras"; else ko "la partición del disco 306:4:17 no es la esperada (empieza en $ini, $tam sectores)"; cat $T/fsck.txt; fi

echo "== Hispa-8086: discos duros de otras medidas (8 megas, FAT12; 64 megas, partición de 32) =="
for megas in 8 64; do
  rm -f $T/hd.img; truncate -s ${megas}M $T/hd.img
  arranca $T/a.img 486 $T/hd.img
  escribe "partir crear /a"; sleep 2; escribe "limpiar"; escribe "partir"; escribe "formatea c: /s /a"
  if espera k11 "octetos libres" 90; then ok "$megas megas: PARTIR CREAR y FORMATEA C: /S acaban"; else ko "$megas megas: no acaban"; fi
  if [ $megas = 8 ]; then
    esta k11 "DOS, FAT12         01h" "8 megas: la partición es de FAT12: "
  else
    esta k11 "DOS, FAT16         04h" "64 megas: la partición es de FAT16: "
    esta k11 "32 megas" "y no pasa de 32 megas: "
  fi
  escribe "copiar hola.com c:"; sleep 2
  apaga
  ini=$(fila_part $T/hd.img 1 | cut -d' ' -f3); tam=$(fila_part $T/hd.img 1 | cut -d' ' -f4)
  dd if=$T/hd.img of=$T/part.img bs=512 skip=$ini count=$tam 2> /dev/null
  if [ "$tam" -le 65535 ] && fsck.fat -n $T/part.img > $T/fsck.txt 2>&1; then ok "$megas megas: la partición ($tam sectores) pasa fsck.fat"; else ko "$megas megas: fsck.fat encuentra errores ($tam sectores)"; cat $T/fsck.txt; fi
  arranca_duro $T/hd.img 486
  escribe "limpiar"; escribe "hola desde $megas megas"; escribe "dir"
  pantalla k12
  esta k12 'C:\>' "$megas megas: arranca del disco duro: "
  esta k12 "Parametros: desde $megas megas" "$megas megas: "
  esta k12 "3 archivos, " "$megas megas: "
  apaga
done
}

p_sistema() {
echo "== Hispa-8086: SISTEMA (un disquete y un disco duro que ya tenían formato y archivos) =="
cp $IMG/hispa8086-360.img $T/a.img
rm -f $T/b.img; mformat -C -f 720 -v DATOS -i $T/b.img ::; mcopy -i $T/b.img build/LEEME.TXT ::CARTA.TXT
python3 prueba/haz_duro.py $T/hd.img > /dev/null
mcopy -i $T/hd.img@@$((63 * 512)) build/LEEME.TXT ::CARTA.TXT
printf '\0' | dd of=$T/hd.img bs=1 seek=$((0x1BE)) conv=notrunc 2> /dev/null      # (la partición, sin marcar como activa)
QMAS="-drive file=$T/b.img,format=raw,if=floppy,index=1" arranca $T/a.img 486 $T/hd.img
escribe "limpiar"; escribe "sistema"; escribe "sistema b:"
pantalla s0
esta s0 "SISTEMA unidad: [/A]" "sin nada, dice cómo se usa: "
esta s0 "Se va a escribir el arranque de Hispa-8086 en B: (sus archivos no se tocan)." "avisa y pregunta: "
tecla s
if espera s1 "Sistema transferido a B:." 30; then ok "SISTEMA B: acaba"; else ko "SISTEMA B: no acaba"; fi
escribe "limpiar"; escribe "sistema c: /a"
if espera s2 "Sistema transferido a C:." 30; then ok "SISTEMA C: /A acaba"; else ko "SISTEMA C: no acaba"; fi
escribe "dir c:"
pantalla s3
esta s3 "CARTA.TXT" "lo que había en C: sigue ahí: "
esta s3 "3 archivos, " ""
apaga
QMAS=
if fsck.fat -n $T/b.img > $T/fsck.txt 2>&1; then ok "fsck.fat da por bueno el disquete"; else ko "fsck.fat encuentra errores en el disquete"; cat $T/fsck.txt; fi
if cmp -s <(mtype -i $T/b.img ::CARTA.TXT) build/LEEME.TXT && mdir -i $T/b.img :: | grep -q DATOS; then ok "el disquete conserva su archivo y su etiqueta"; else ko "el disquete ha perdido algo"; fi
dd if=$T/hd.img of=$T/part.img bs=512 skip=63 2> /dev/null
if fsck.fat -n $T/part.img > $T/fsck.txt 2>&1; then ok "fsck.fat da por buena la partición"; else ko "fsck.fat encuentra errores en la partición"; cat $T/fsck.txt; fi
if [ "$(fila_part $T/hd.img 1 | cut -d' ' -f1)" = 80 ] && cmp -s -n 446 $T/hd.img build/mbr.bin; then ok "la partición ha quedado activa y el sector de particiones tiene nuestro código"; else ko "el sector de particiones no ha quedado bien"; fi
arranca $T/b.img 486
escribe "limpiar"; escribe "mostrar carta.txt"
pantalla s4
esta s4 'A:\>' "el disquete de 720 K hecho con SISTEMA arranca: "
esta s4 "en español." "y su archivo se lee: "
apaga
arranca_duro $T/hd.img 486
escribe "limpiar"; escribe "mostrar carta.txt"
pantalla s5
esta s5 'C:\>' "el disco duro hecho con SISTEMA arranca: "
esta s5 "en español." "y su archivo se lee: "
apaga
}

p_lotes() {
echo "== Hispa-8086: lotes (FOR, LLAMAR, DESPLAZAR), tuberías y filtros =="
cp $IMG/hispa8086-360.img $T/a.img
mcopy -i $T/a.img prueba/BUCLE.BAT prueba/SUB.BAT prueba/SUB2.BAT ::
arranca $T/a.img 486
escribe "limpiar"; escribe "bucle p1 p2"; sleep 8
pantalla l1
esta l1 "Elemento uno." "FOR con una lista: "
esta l1 "Elemento tres." "(la coma también separa) "
esta l1 "Archivo EXE.EXE" "FOR con comodines: "
esta l1 "Archivo HOLA.COM" ""
noesta l1 "Archivo nohay" "un patrón que no casa con nada no da vueltas: "
esta l1 "Para: b" "PARA ... EN ... HAZ, con un IF dentro: "
noesta l1 "Para: a" ""
esta l1 "Sub: recibo alfa y beta" "LLAMAR pasa los parámetros: "
esta l1 "Sub2: alfa" "un lote llamado llama a otro: "
esta l1 "Sub: acabo alfa" "y al volver sigue por donde iba: "
esta l1 "Bucle: he vuelto con p1" "de vuelta, con los parámetros de antes: "
esta l1 "Sub: acabo gamma" "CALL (el nombre clásico): "
noesta l1 "NO DEBE SALIR" ""
esta l1 "Queda p1" "DESPLAZAR: "
esta l1 "Queda p2" ""
esta l1 "Bucle: acabo" ""
noesta l1 "eco Sub" "el lote llamado hereda el ECO OFF: "
if [ "$(grep -c 'Queda' $T/l1.txt)" = 2 ] && [ "$(grep -c 'Sub2:' $T/l1.txt)" = 2 ]; then ok "cada cosa sale las veces justas"; else ko "DESPLAZAR o LLAMAR dan vueltas de más o de menos"; fi
escribe "limpiar"; escribe "for %a in (hola.com exe.exe) do dir %a"; sleep 2; escribe "sub uno dos"; sleep 2
pantalla l2
esta l2 "HOLA.COM             206" "FOR desde el teclado: "
esta l2 "EXE.EXE              464" ""
esta l2 "Sub: acabo uno" "un lote que llama a otro, lanzado desde el teclado: "
esta l2 "eco Sub: acabo uno" "(sin ECO OFF, sus líneas se enseñan) "
# tuberías y filtros
escribe "limpiar"; escribe "eco pera > f.txt"; escribe "eco Manzana >> f.txt"; escribe "eco uva >> f.txt"; escribe "eco cereza >> f.txt"
escribe "limpiar"; escribe "mostrar f.txt | ordenar"; sleep 3
pantalla l3
if [ "$(grep -E '^(cereza|Manzana|pera|uva)$' $T/l3.txt | tr '\n' ' ')" = "cereza Manzana pera uva " ]; then ok "orden | ORDENAR: las líneas salen ordenadas (sin mirar mayúsculas)"; else ko "orden | ORDENAR"; fi
escribe "limpiar"; escribe "ordenar /r < f.txt"; sleep 2
pantalla l4
if [ "$(grep -E '^(cereza|Manzana|pera|uva)$' $T/l4.txt | tr '\n' ' ')" = "uva pera Manzana cereza " ]; then ok "ORDENAR /R < archivo: al revés"; else ko "ORDENAR /R"; fi
escribe "limpiar"; escribe "sort /+2 f.txt | more"; sleep 3
pantalla l5
if [ "$(grep -E '^(cereza|Manzana|pera|uva)$' $T/l5.txt | tr '\n' ' ')" = "Manzana pera cereza uva " ]; then ok "SORT /+2 archivo | MORE: desde la segunda columna, dos programas en tubería y sus nombres clásicos"; else ko "SORT /+2 | MORE"; fi
escribe "limpiar"; escribe "ayuda > g.txt"; escribe "ayuda >> g.txt"; escribe "mostrar g.txt | ordenar | mas"; sleep 4
pantalla l6
esta l6 "-- Más --" "tres tramos, y MAS se para al llenarse la pantalla: "
noesta l6 'A:\>' ""
tecla spc; sleep 2
pantalla l7
noesta l7 "-- Más --" "con una tecla sigue hasta el final: "
esta l7 'A:\>' ""
escribe "limpiar"; escribe "dir c*.* | ordenar"; sleep 4
pantalla l8
esta l8 "COMMAND.COM" "DIR | ORDENAR: "
noesta l8 "TUBO" "los archivos temporales de la tubería no salen en la lista: "
escribe "limpiar"; escribe "dir *.\$\$\$"; escribe "format"; escribe "sys"; escribe "fdisk"
pantalla l8
esta l8 "0 archivos, 0 octetos" "los archivos temporales de la tubería se borran: "
esta l8 "FORMATEA unidad: [/S]" "FORMAT es FORMATEA: "
esta l8 "SISTEMA unidad: [/A]" "SYS es SISTEMA: "
esta l8 "No hay disco duro." "FDISK es PARTIR: "
apaga
if fsck.fat -n $T/a.img > $T/fsck.txt 2>&1; then ok "fsck.fat no encuentra ningún error en el disquete"; else ko "fsck.fat encuentra errores"; cat $T/fsck.txt; fi
}

p_config() {
echo "== Hispa-8086: CONFIG.SYS, INT 25h/26h y lo nuevo del núcleo =="
cp $IMG/hispa8086-360.img $T/a.img
mcopy -i $T/a.img build/N8.COM ::
arranca $T/a.img 486
escribe "limpiar"; escribe "n8"; sleep 3; escribe "dir n8.com"
pantalla c1
esta c1 "N8: sectores en crudo bien" "INT 25h e INT 26h: leer, escribir y releer sectores: "
esta c1 "N8: caben 08 archivos abiertos" "sin CONFIG.SYS, ocho archivos abiertos: "
esta c1 "N8.COM               283" "después de escribir en crudo, el sistema sigue leyendo el disco: "
apaga
if python3 - $T/a.img <<'PY2'
import sys
d = open(sys.argv[1], 'rb').read()
dib = bytes((0x5A + 7 * i) & 255 for i in range(1024))
sys.exit(0 if d[-1024:] == dib else 1)
PY2
then ok "lo escrito con INT 26h está en los dos últimos sectores del disquete"; else ko "INT 26h no ha escrito lo esperado"; fi
printf 'REM configuracion de prueba\r\nFILES=20\r\n\r\n  buffers = 10\r\nDEVICE=RATON.SYS\r\nLASTDRIVE=Z\r\nshell=\\COMMAND.COM /P\r\n' > $T/CONFIG.SYS
mcopy -i $T/a.img $T/CONFIG.SYS ::
arranca $T/a.img 486
pantalla c2
esta c2 "CONFIG.SYS: no se puede cargar el controlador: DEVICE=RATON.SYS" "un controlador que no está, lo dice y sigue: "
esta c2 "CONFIG.SYS: no se entiende esta línea: LASTDRIVE=Z" "lo que no entiende lo dice y sigue: "
noesta c2 "no se entiende esta línea: REM" "REM, FILES=, BUFFERS= y las líneas vacías, sin protestar: "
noesta c2 "no se entiende esta línea: FILES" ""
noesta c2 "no se entiende esta línea:   buffers" ""
esta c2 "Arranque terminado (AUTOEXEC.BAT)." "SHELL=\\COMMAND.COM /P: el intérprete arranca con AUTOEXEC.BAT: "
escribe "limpiar"; escribe "n8"; sleep 3; escribe "equipo"
pantalla c3
esta c3 "N8: caben 20 archivos abiertos" "FILES=20: "
libres=$(grep -o "([0-9]* K libres" $T/c3.txt | tr -dc 0-9)
if [ -n "$libres" ] && [ "$libres" -ge 595 ]; then ok "con FILES=20 y BUFFERS=10 quedan $libres K libres"; else ko "memoria libre con FILES=20: $libres K"; fi
apaga
printf 'FILES=999\r\nSHELL=\\NOHAY.COM /X\r\n' > $T/CONFIG.SYS
mcopy -o -i $T/a.img $T/CONFIG.SYS ::
arranca $T/a.img 486
pantalla c4
esta c4 "No se puede lanzar el intérprete de SHELL=: se usa COMMAND.COM." "un SHELL= que no existe: "
esta c4 'A:\>' "y aun así arranca: "
escribe "limpiar"; escribe "n8"; sleep 4
pantalla c5
esta c5 "N8: caben 64 archivos abiertos" "FILES=999 se queda en lo más que hay, 64: "
apaga
printf 'shell=a:\\command.com\r\n' > $T/CONFIG.SYS
mcopy -o -i $T/a.img $T/CONFIG.SYS ::
arranca $T/a.img 486
pantalla c6
esta c6 'A:\>' "SHELL= sin /P: arranca el intérprete... "
noesta c6 "Arranque terminado" "...y, como en DOS, sin /P no ejecuta AUTOEXEC.BAT: "
apaga
}

. prueba/partes_09.sh
. prueba/partes_10.sh
. prueba/partes_11.sh
. prueba/partes_pluma.sh
. prueba/partes_cuentas.sh
. prueba/partes_juegos.sh
. prueba/partes_marcianos.sh
. prueba/partes_14.sh
PARTES="basico lotes ordenes dir config alta dispositivos reserva discos fantasma editar teclado teclado9 flechas v86 v86_flechas pasar duro formatea sistema partir medidas fuentes"
PARTES="$PARTES pluma pluma_grande pluma_oficina"     # (PLUMA, del disquete de Oficina)
PARTES="$PARTES cuentas cuentas_pantalla cuentas_ordenar cuentas_grande fat_sectores"     # (CUENTAS, del disquete de Oficina)
PARTES="$PARTES derribo marcianos"     # (los juegos, del disquete de Juegos)
PARTES="$PARTES auditoria1"     # (los arreglos de la auditoría de la 0.14)
[ -n "${MAS_SH:-}" ] && . "$MAS_SH"     # (para probar a mano: más partes, de otro archivo)
for parte in ${SOLO:-$PARTES}; do p_$parte; done
n_ok=$(grep -c '^  OK' $T/salida.txt); n_fail=$(grep -c '^  FAIL' $T/salida.txt)
echo "== Resultado: $n_ok OK, $n_fail FAIL =="
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then for p in $T/*.txt; do [ $p = $T/salida.txt ] || [ $p = $T/vuelta.txt ] && continue; echo "--- $(basename $p .txt)"; cat $p; done; fi
rm -rf $T
exit $fail
