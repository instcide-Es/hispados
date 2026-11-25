# Las partes de la prueba que llegaron con la versión 0.10 (las carga
# prueba.sh): lo que el sistema aprovecha de un 286, y DIR.
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.

# (para p_alta) arranca el disquete con el CONFIG.SYS que se diga ($1, con
# «\n» entre líneas) y la memoria de $2 megas, y deja en $3 la pantalla de
# XMS, MEMORIA y la prueba de archivos
arranca_alta() {
  cp $IMG/hispa8086-360.img $T/a.img
  printf "$1" | sed 's/$/\r/' > $T/CONFIG.SYS
  mcopy -o -i $T/a.img $T/CONFIG.SYS build/XMS.COM ::
  QMEM=$2 arranca $T/a.img 486
  escribe "limpiar"; escribe "xms"; sleep 5; escribe "memoria"; escribe "arch"; sleep 7
  pantalla $3
}
libre_de() { grep -o "programas: [0-9]*" $T/$1.txt | head -1 | tr -dc 0-9; }

p_alta() {
echo "== Hispa-8086: en un 286 o más: A20, memoria extendida (XMS) y memoria alta (HMA) =="
# (con MODO=8086 el núcleo hace como si no hubiera 286: esta parte, que
# prueba justamente lo del 286 y ya prueba aparte PROCESADOR=8086, no toca)
if [ "${MODO:-}" = 8086 ]; then ok "MODO=8086: la parte del 286 no se pasa (el modo 8086 se prueba dentro de ella)"; return; fi
# 1. con DOS=BAJO: el núcleo abajo, y el gestor, solo
arranca_alta 'DOS=BAJO\n' 4 a1
esta a1 "XMS: 2.0, libres 02880 K" "con 4 megas, hay gestor de memoria extendida: "
esta a1 "A20: se abre y se cierra de verdad" "la puerta A20, comprobada cambiando 0000:0000 y mirando FFFF:0010: "
esta a1 "XMS: 256 K movidos y comparados" "mover bloques a la memoria extendida y de vuelta: "
esta a1 "XMS: crecer a 384 K, bloquear y soltar" ""
esta a1 "XMS: llenar y soltar" "pedirla toda, que no quede, soltarla y que quede la de antes: "
esta a1 "HMA: pedida y soltada" "la HMA, para el programa que la pida: "
noesta a1 "XMS: MAL" ""
esta a1 "Memoria alta (HMA), 64 K: libre" "MEMORIA: "
esta a1 "Memoria extendida: 2880 K. Libre: 2880 K" ""
esta a1 "ARCHIVOS: BIEN" "y lo de siempre, igual: "
bajo=$(libre_de a1)
apaga
# 2. DOS=ALTO: el núcleo, a la HMA
arranca_alta 'DOS=ALTO\n' 4 a2
esta a2 "A20: abierta siempre (el nucleo esta arriba)" "DOS=ALTO: A20 queda abierta: "
esta a2 "XMS: 256 K movidos y comparados" "con el núcleo arriba, el gestor sigue igual: "
esta a2 "XMS: llenar y soltar" ""
esta a2 "HMA: la tiene el sistema" "y la HMA ya no se le da a nadie: "
noesta a2 "XMS: MAL" ""
esta a2 "Memoria alta (HMA), 64 K: la usa el sistema (DOS=ALTO)" "MEMORIA: "
esta a2 "ARCHIVOS: BIEN" "la interfaz de archivos, con el núcleo en la HMA: "
alto=$(libre_de a2)
escribe "limpiar"; escribe "exe"; sleep 4; escribe "lote uno dos"; sleep 6
pantalla a3
esta a3 "EXE: el hijo acaba con 7" "programas .EXE que lanzan otros, con el núcleo arriba: "
esta a3 "Lote: acabo" "y los lotes: "
apaga
if fsck.fat -n $T/a.img > $T/fsck.txt 2>&1; then ok "fsck.fat no encuentra errores en el disquete usado con DOS=ALTO"; else ko "fsck.fat encuentra errores (DOS=ALTO)"; cat $T/fsck.txt; fi
echo "  (memoria libre para los programas, con 640 K: $bajo octetos con el núcleo abajo y $alto con DOS=ALTO: $((alto - bajo)) más)" | tee $T/ganancia.txt
if [ -n "$bajo" ] && [ -n "$alto" ] && [ $((alto - bajo)) -ge 15000 ]; then ok "DOS=ALTO deja $((alto - bajo)) octetos más de memoria convencional (de $bajo a $alto)"; else ko "DOS=ALTO no gana lo esperado (de $bajo a $alto)"; fi
# 3. como un 8086: nada de lo nuevo, aunque se pida
arranca_alta 'DOS=ALTO\nPROCESADOR=8086\n' 4 a4
esta a4 "XMS: no hay gestor de memoria extendida" "PROCESADOR=8086: sin gestor, aunque haya memoria: "
esta a4 "Sin memoria extendida (o sin un 286 que la aproveche)." "MEMORIA: "
esta a4 "ARCHIVOS: BIEN" "y todo lo demás funciona: "
como=$(libre_de a4)
mon "pmemsave 0xbc 4 \"$T/v2f.bin\"" 0.4      # el vector de INT 2Fh
apaga
if [ -n "$como" ] && [ $((como - bajo)) -ge -64 ] && [ $((como - bajo)) -le 64 ]; then ok "como un 8086, la memoria libre es la del núcleo abajo ($como octetos)"; else ko "memoria como un 8086: $como (abajo: $bajo)"; fi
if [ "$(xxd -p -s 2 -l 2 $T/v2f.bin)" != "6f00" ]; then ok "y INT 2Fh ni se toca: no apunta al núcleo"; else ko "como un 8086, INT 2Fh apunta al núcleo"; fi
# 4. con 1 mega justo no hay memoria extendida: DOS=ALTO no hace nada y arranca
arranca_alta 'DOS=ALTO\n' 1 a5
esta a5 "XMS: no hay gestor de memoria extendida" "con 1 mega (sin memoria extendida), DOS=ALTO no hace nada: "
esta a5 "ARCHIVOS: BIEN" ""
apaga
# 5. con 2 y con 16 megas
arranca_alta 'DOS=ALTO\n' 2 a6
esta a6 "XMS: llenar y soltar" "con 2 megas y DOS=ALTO: "
esta a6 "Memoria extendida: 832 K." ""
noesta a6 "XMS: MAL" ""
apaga
arranca_alta 'DOS=BAJO\n' 16 a7
esta a7 "XMS: llenar y soltar" "con 16 megas: "
esta a7 "Memoria extendida: 15168 K." ""
noesta a7 "XMS: MAL" ""
apaga
# 6. una máquina sin PCI (isapc: lo más parecido a un AT que tiene QEMU)
cp $IMG/hispa8086-360.img $T/a.img
printf 'DOS=ALTO\r\n' > $T/CONFIG.SYS
mcopy -o -i $T/a.img $T/CONFIG.SYS build/XMS.COM ::
QMAS="-M isapc" arranca $T/a.img 486
escribe "limpiar"; escribe "xms"; sleep 5; escribe "memoria"
pantalla a8
esta a8 "XMS: llenar y soltar" "en la máquina «isapc» de QEMU, con DOS=ALTO: "
esta a8 "HMA: la tiene el sistema" ""
noesta a8 "XMS: MAL" ""
apaga
QMAS=
# 7. DISCORAM en memoria extendida, con el núcleo arriba
cp $IMG/hispa8086-360.img $T/a.img
printf 'DOS=ALTO\r\nDEVICE=\\DISCORAM.SYS 1000 /E\r\n' > $T/CONFIG.SYS
mcopy -o -i $T/a.img $T/CONFIG.SYS ::
arranca $T/a.img 486
pantalla a9
esta a9 "DISCORAM: disco en memoria extendida de 1000 K en la unidad D:" "DISCORAM /E: "
escribe "limpiar"; escribe "copiar *.com d:"; sleep 9; escribe "d:"; escribe "limpiar"; escribe "hola desde la extendida"; escribe "a:revisar"; sleep 3; escribe "a:xms"; sleep 5; escribe "memoria"
pantalla a10
esta a10 "Parametros: desde la extendida" "programas cargados de un disco que está en memoria extendida: "
esta a10 "1018368 octetos en total en el disco" "REVISAR: un disco de 1.000 K: "
noesta a10 "Hay errores" ""
esta a10 "Memoria extendida: 2880 K. Libre: 1880 K" "MEMORIA: los 1.000 K del disco salen de la extendida: "
con_disco=$(libre_de a10)
apaga
if [ -n "$con_disco" ] && [ $((alto - con_disco)) -le 1024 ]; then ok "y no de la convencional: el disco de 1.000 K cuesta $((alto - con_disco)) octetos"; else ko "DISCORAM /E gasta memoria convencional: $con_disco frente a $alto"; fi
# sin memoria extendida, /E lo dice y no se instala
cp $IMG/hispa8086-360.img $T/a.img
printf 'PROCESADOR=8086\r\nDEVICE=\\DISCORAM.SYS 1000 /E\r\n' > $T/CONFIG.SYS
mcopy -o -i $T/a.img $T/CONFIG.SYS ::
arranca $T/a.img 486
pantalla a11
esta a11 "DISCORAM: no hay memoria extendida para ese disco (/E)." "DISCORAM /E como un 8086: "
esta a11 'A:\>' "y el sistema arranca: "
apaga
}

p_dir() {
echo "== Hispa-8086: DIR con fecha, /W y /P; la fecha de los archivos (INT 21h/57h) =="
cp $IMG/hispa8086-360.img $T/a.img
cp build/HOLA.COM $T/VIEJO.COM; touch -d "1987-04-02 09:30:00" $T/VIEJO.COM
mcopy -o -m -i $T/a.img $T/VIEJO.COM ::VIEJO.COM
arranca $T/a.img 486
escribe "limpiar"; escribe "fecha 5-3-1999"; escribe "hora 7:08"; escribe "creardir dd"; escribe "copiar viejo.com dd"; sleep 2; escribe "xcopiar dd ee"; sleep 3; escribe "limpiar"; escribe "dir viejo.com"; escribe "dir dd"; escribe "dir ee\\viejo.com"
pantalla i1
esta i1 "VIEJO.COM            206  02-04-1987 09:30" "DIR enseña la fecha y la hora: "
esta i1 ".                  <DIR>  05-03-1999 07:08" "la de un directorio, la del día en que se creó: "
if [ "$(grep -c 'VIEJO.COM            206  02-04-1987 09:30' $T/i1.txt)" = 3 ]; then ok "COPIAR y XCOPIAR conservan la fecha del original (INT 21h/57h)"; else ko "las copias no conservan la fecha"; fi
escribe "limpiar"; escribe "dir /w"
pantalla i2
esta i2 "NUCLEO86.SYS    COMMAND.COM     PASAR.COM       FORMATEA.COM    SISTEMA.COM" "DIR /W, cinco por línea: "
esta i2 "[DD]" "con los directorios entre corchetes: "
escribe "limpiar"; escribe "dir /p"
pantalla i3
esta i3 "Pulse una tecla para seguir..." "DIR /P se para al llenar la pantalla: "
noesta i3 "octetos libres" ""
tecla spc; sleep 1.5
pantalla i4
esta i4 "octetos libres" "y con una tecla sigue: "
apaga
if [ "$(mdir -i $T/a.img ::DD ::EE 2> /dev/null | grep -c '1987-04-02   9:30')" = 2 ] && fsck.fat -n $T/a.img > $T/fsck.txt 2>&1; then ok "desde fuera: las dos copias tienen la fecha de 1987, y fsck.fat no encuentra errores"; else ko "las fechas, vistas desde fuera, no son las esperadas"; mdir -i $T/a.img ::DD ::EE; fi
}

p_teclado9() {
echo "== Hispa-8086: TECLADO por la interrupción del teclado (INT 9), como en un PC o un XT =="
cp $IMG/hispa8086-360.img $T/a.img
arranca $T/a.img 486
escribe "limpiar"; escribe "teclado /9"; sleep 1
tecla e c o spc semicolon shift-semicolon spc apostrophe a apostrophe e spc minus shift-2 spc alt_r-2 spc equal shift-equal spc shift-7 ret; sleep 1
pantalla t9
esta t9 "ñÑ áé '\" @ ¡¿ /" "TECLADO /9 (el camino de los PC y XT; en un AT lo normal es INT 15h/4Fh): "
apaga
}

# TECLADO en el emulador v86 (el de la web del proyecto), que al leer dos
# veces el puerto del teclado da la tecla siguiente: ahí se vio el fallo de la
# 0.9. Hace falta node, playwright-core, Chrome y una copia de v86 y de la
# página; si falta algo, se dice y se salta
p_v86() {
echo "== Hispa-8086: TECLADO en el emulador v86 (opcional) =="
local pw=${PLAYWRIGHT:-/opt/herramientas/node_modules/playwright-core} cr=${CHROME:-/usr/bin/google-chrome}
local v86=${V86:-/www/hispados-web/v86} pag=${V86_PAGINA:-/root/FTP/HispaDOS/web/index.html}
local falta=""
command -v node > /dev/null || falta="$falta node"
[ -d "$pw" ] || falta="$falta $pw (PLAYWRIGHT=)"
[ -x "$cr" ] || falta="$falta $cr (CHROME=)"
[ -f "$v86/libv86.js" ] || falta="$falta $v86/libv86.js (V86=)"
[ -f "$pag" ] || falta="$falta $pag (V86_PAGINA=)"
if [ -n "$falta" ]; then echo "  (se salta: falta$falta)"; return; fi
mkdir -p $T/web/v86
cp "$pag" $T/web/index.html; cp $v86/libv86.js $v86/v86.wasm $v86/seabios.bin $v86/vgabios.bin $T/web/v86/
cp $IMG/hispa8086-1440.img $T/web/v86/hispa8086.img
printf '@ECO OFF\r\nTECLADO\r\n' > $T/AUTOEXEC.BAT
mcopy -o -i $T/web/v86/hispa8086.img $T/AUTOEXEC.BAT ::
(cd $T/web && exec python3 -m http.server ${V86_PUERTO:-8932} --bind 127.0.0.1 > /dev/null 2>&1) &
local servidor=$!
sleep 1
timeout 170 node prueba/v86_teclado.js http://127.0.0.1:${V86_PUERTO:-8932}/ "$pw" "$cr" > $T/v86.txt 2>&1
# y dos disqueteras: copiar a B: un archivo de un sector y otro de 40 K, y
# mirar el disquete de B: tal como lo deja el emulador. En B: va uno de 720 K:
# la unidad B: de v86 es de 3½, y una imagen de 360 K (5¼) la toma por un
# disquete de UNA cara y 80 pistas: con ella falla todo lo que cae en la cara 1
# (error 20h de la BIOS), y no es cosa del sistema
if grep -q "dq-archivo" $T/web/index.html; then
  cp $IMG/hispa8086-1440.img $T/web/v86/hispa8086.img
  python3 -c "import sys; sys.stdout.buffer.write(bytes((i * 31 + (i >> 8) * 7 + 5) & 255 for i in range(40000)))" > $T/GRANDE.BIN
  mcopy -o -i $T/web/v86/hispa8086.img $T/GRANDE.BIN ::
  rm -f $T/b.img; mformat -C -f 720 -i $T/b.img ::
  timeout 175 node prueba/v86_discos.js http://127.0.0.1:${V86_PUERTO:-8932}/ "$pw" "$cr" $T/b.img $T/b_v86.img "copy a:leeme.txt b:" "copy a:grande.bin b:" "dir b:" > $T/v86b.txt 2>&1
fi
kill $servidor 2> /dev/null
sed 's/^/  /' $T/v86.txt
local frase="Hola ñÑ áé ¿¡ /-():; @/"
if grep -qxF "DEPRISA: $frase" $T/v86.txt; then ok "v86, tecleando deprisa (30 ms): «$frase», sin teclas repetidas"; else ko "v86, deprisa: no sale «$frase»"; fi
if grep -qxF "DESPACIO: $frase" $T/v86.txt; then ok "v86, tecleando despacio (150 ms): «$frase»"; else ko "v86, despacio: no sale «$frase»"; fi
if [ -f $T/v86b.txt ]; then
  sed 's/^/  /' $T/v86b.txt
  if [ "$(grep -c '1 archivo copiado.' $T/v86b.txt)" -ge 2 ] && cmp -s <(mtype -i $T/b_v86.img ::LEEME.TXT) build/LEEME.TXT && cmp -s <(mtype -i $T/b_v86.img ::GRANDE.BIN) $T/GRANDE.BIN && fsck.fat -n $T/b_v86.img > $T/fsck.txt 2>&1; then ok "v86, dos disqueteras: lo copiado a B: (un sector, y 40 K) está en su disquete octeto a octeto, y fsck.fat lo da por bueno"; else ko "v86: la copia a B: no ha quedado bien"; cat $T/fsck.txt 2> /dev/null; fi
else
  echo "  (la página no tiene unidad B: la copia entre disquetes en v86 se salta)"
fi
}
