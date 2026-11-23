# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# pruebas_termo.sh - la parte 46 (SOLO=termo: el Termómetro, TERMO.BIN,
# nucleo32/termo/, y su memoria USB: «TERMO=1 usb.sh») de pruebas.sh, que lee
# este archivo («.»): la función parte_termo, con los ayudantes de
# pruebas.sh (ok, ko, mira) y sus carpetas ($AQUI, $P).
#  0. En el anfitrión: el veredicto (veredicto.c) con los registros de los
#     equipos de verdad (termo/pruebas/*-real.log: el HP Mini 5103 con una
#     zona clavada, el Toshiba NB250 con el sensor clavado en 6 °C, el
#     ThinkPad A275 con 114,3 °C de un registro equivocado), la TjMax de los
#     Intel, el terminal de SSH.BIN (ssh/vt.c) con lo que pintan colores.sh,
#     htop, vim y less de verdad, y el receptor de «Enviar a Carlos»
#     (tools/termo-subida/subir.php con «php -S», y termo-mover.sh).
# QEMU no da temperaturas del procesador: la zona térmica de mentira de
# tests/acpi/termo-qemu.py (-acpitable) lee sus grados de la CMOS (63h), que
# se cambia por el monitor. Se arranca la memoria de verdad (GRUB, xHCI):
#  1. -cpu Haswell: el Termómetro se abre solo; dice que el procesador no da
#     temperatura; la zona y el ventilador de ACPI y el SMBIOS; «Calentar»
#     con la zona a 91 °C (de 100): la parada de seguridad, el enfriamiento y
#     el resumen (.TXT); el CSV y el .LOG con sus sucesos; el registro cada
#     2 s por el reloj; lo ocupado en reposo; el ratón que pasa no repinta;
#     y una zona que lee de un controlador embebido que no contesta (EC0 de
#     la misma SSDT): el porqué en el .LOG y sus escrituras al EC, no hechas.
#  2. Calentando, la máquina se mata sin apagar (kill -9): la FAT, sana
#     (fsck.fat) y el .LOG y el .CSV con sus últimas líneas de la carga.
#  3. Otra vez, con KVM y el procesador del anfitrión si hay /dev/kvm (si no,
#     -cpu phenom): el aviso de que la vez anterior se apagó durante la
#     prueba, con su temperatura; y el MSR que no está (la excepción 13 la
#     recoge el núcleo: el rdmsr «seguro»).
#  4. «Enviar a Carlos» (la tecla E): sin red lo dice; con red, por HTTPS (una
#     raíz de prueba en \RAICES; tls_delante.py delante de subir.php) llega
#     lo de \TERMO y el \ARRANQUE.TXT, con el testigo de la memoria.
#  5. «Terminal» (la tecla T): SSH.BIN, con la llave de la memoria, contra un
#     sshd propio de la prueba (puerto alto, 127.0.0.1, su ForceCommand:
#     termo/pruebas/sesion_ssh.sh): xterm-256color, colores de 24 bits,
#     htop, vim y less a pantalla completa (capturas), las teclas en crudo y
#     el tamaño nuevo al cambiar la ventana (window-change: la ventana
#     empieza de 640 x 380 y se agranda con Alt+Espacio, «Tamaño» y las flechas).
#  6. -smp 4 (con y sin KVM): la carga en los demás procesadores; y sin
#     ninguna NVIDIA, que lo dice sin colgarse.
#  7. Por UEFI (OVMF, q35, -smp 2): el Termómetro, las zonas y la carga.
#  8. «Rendimiento» (R): las medidas en RENDIMIE.TXT, y que Esc la para.
# Necesita ser root (usb.sh), qemu-system-x86_64, socat, mtools y fsck.fat;
# para la 0, gcc; para la 4, php y openssl; para la 5, sshd (OpenSSH).

# termo_qemu IMAGEN NOMBRE SEGUNDOS "CPU" "t:orden del monitor"...
# (TQ_RED: la red de QEMU; sin red si no se dice. TQ_FIRM: el firmware, OVMF para UEFI)
termo_qemu() {
  local img=$1 n=$2 seg=$3 cpu=$4 t=0 o w q; shift 4
  rm -f $Y/$n.mon
  # shellcheck disable=SC2086
  qemu-system-x86_64 $cpu ${TQ_FIRM:-} ${TQ_RED:--nic none} -m 512 -display none -monitor unix:$Y/$n.mon,server,nowait -serial file:$Y/$n.sal -no-reboot \
    -acpitable file=$Y/termo.aml -device qemu-xhci -drive file=$img,format=raw,if=none,id=u -device usb-storage,drive=u,bootindex=1 > /dev/null 2>&1 &
  q=$!
  QPID=$q
  sleep 1
  printf 'o /b 0x70 0x63\no /b 0x71 45\n' | socat - UNIX-CONNECT:$Y/$n.mon > /dev/null 2>&1
  for o in "$@"; do
    w=${o%%:*}
    while [ $t -lt $w ]; do sleep 1; t=$((t + 1)); done
    printf '%s\n' "${o#*:}" | socat - UNIX-CONNECT:$Y/$n.mon > /dev/null 2>&1
  done
  while [ $t -lt $seg ]; do sleep 1; t=$((t + 1)); done
  kill -9 $q 2> /dev/null
  wait $q 2> /dev/null
  QPID=
  tr -d '\r' < $Y/$n.sal | iconv -f CP437 -t UTF-8 > $Y/$n.txt 2> /dev/null || tr -d '\r' < $Y/$n.sal > $Y/$n.txt
  S=$Y/$n.txt
}

# (TERMO) 0. lo del anfitrión
termo_anfitrion() {
  local T=$AQUI/nucleo32/termo B=$Y/anf r f
  mkdir -p $B
  if ! gcc -O1 -Wall -I$T $T/pruebas/lee_log.c $T/veredicto.c -o $B/lee_log 2> $B/gcc.txt || ! gcc -O1 -Wall -I$T $T/pruebas/tjmax.c $T/veredicto.c -o $B/tjmax 2>> $B/gcc.txt; then
    ko "veredicto.c no compila en el anfitrión: $(head -n 2 $B/gcc.txt)"; return
  fi
  $B/lee_log $T/pruebas/hpmini-5103-real.log > $B/hpmini.txt
  if grep -q 'Medido con «Nucleo 0»: subió de 25,0 °C a 38,0 °C (+13 °C)' $B/hpmini.txt && grep -q 'No cambia en toda la prueba (fija, no cuenta): ACPI \\_TZ_.FDTZ (50,0 °C)' $B/hpmini.txt \
     && grep -q 'no volvió a la de antes de la prueba (25,0 °C)' $B/hpmini.txt && ! grep -q '55,0' $B/hpmini.txt && ! grep -qi 'polvo' $B/hpmini.txt; then
    ok "el registro de verdad del HP Mini 5103: se mide con el procesador (de 25 a 38 °C, +13), la zona FDTZ clavada en 50 °C sale como fija y no cuenta, nada de «volvió a 55 °C»"
  else ko "el veredicto del HP Mini 5103: $(head -c 300 $B/hpmini.txt)"; fi
  $B/lee_log $T/pruebas/thinkpad-a275-real.log > $B/thinkpad.txt
  if grep -q 'No se puede juzgar: ningún sensor da una temperatura creíble' $B/thinkpad.txt && ! grep -q 'PELIGRO' $B/thinkpad.txt; then
    ok "el registro del ThinkPad A275 (114,3 °C fijos de 90): «no se puede juzgar», nunca PELIGRO"
  else ko "el veredicto del ThinkPad A275: $(head -c 300 $B/thinkpad.txt)"; fi
  $B/lee_log $T/pruebas/toshiba-nb250-real.log > $B/toshiba.txt
  if grep -q 'este equipo no da la temperatura del procesador' $B/toshiba.txt; then
    ok "el registro del Toshiba NB250 (el sensor clavado en 6 °C, también en carga): «este equipo no da la temperatura del procesador»"
  else ko "el veredicto del Toshiba NB250: $(head -c 300 $B/toshiba.txt)"; fi
  $B/lee_log $T/pruebas/toshiba-nb250-lento.log > $B/toshiba2.txt
  if grep -q 'medida aproximada: la cifra no es de fiar, la subida sí): subió unos 7 °C (de 16,0 °C' $B/toshiba2.txt && grep -q 'Al parar se enfría bien' $B/toshiba2.txt \
     && ! grep -q 'No se puede juzgar\|fuera de lo posible' $B/toshiba2.txt; then
    ok "el Toshiba NB250 con el sensor lento (16 °C fijos 150 s y luego a 23): «subió unos 7 °C (medida aproximada); se enfría bien», no «no se puede juzgar»"
  else ko "el veredicto del Toshiba con el sensor lento: $(head -c 300 $B/toshiba2.txt)"; fi
  if $B/tjmax > $B/tjmax.txt; then ok "la TjMax de los Intel que no la dicen: $(grep -c '^bien' $B/tjmax.txt) procesadores bien (el Atom N455 del HP Mini y del Toshiba, 100 °C; los N270, 90)"
  else ko "la TjMax: $(grep '^mal' $B/tjmax.txt | head -n 2)"; fi
  # el terminal de SSH.BIN
  python3 -I $AQUI/nucleo32/juegos/datos386.py $B/datos386.h $B/letra.h
  if gcc -O1 -Wall -Wextra -Wno-unused-parameter -I$B -I$AQUI/nucleo32/ssh $AQUI/nucleo32/ssh/vt.c $AQUI/nucleo32/ssh/pruebas/vt_prueba.c -o $B/vt_prueba 2> $B/gccvt.txt; then
    python3 -I $AQUI/nucleo32/ssh/pruebas/graba.py 100 30 $B/colores.bin 5 bash $AQUI/nucleo32/ssh/pruebas/colores.sh
    $B/vt_prueba 100 30 $B/colores.bin $B/colores.ppm 0 0 2 2 0 6 > $B/colores.txt
    if grep -q '^fila 02: ╭──────────────────────────────────────────╮' $B/colores.txt && grep -q '^fila 06: ⏺ Leyendo termo.c' $B/colores.txt \
       && grep -q '^fila 11: ✻ Pensando… (esc para parar)' $B/colores.txt && grep -q '^fila 13: │ ❯' $B/colores.txt \
       && grep -q '^celda 0,0: U+0020 letra [0-9A-F]* fondo FF5000' $B/colores.txt && grep -q '^celda 2,2: U+2500 letra D77757' $B/colores.txt \
       && grep -q 'resp: \\e\[8;30;100t\\e\[17;1R\\e\]11;rgb:1515/1818/1d1d\\e\\\\e\[?2026;2\$y\\e\[?62;22c' $B/colores.txt \
       && grep -q 'pegado 1; sincro 0' $B/colores.txt; then
      ok "el terminal (vt.c) con una interfaz como la de Claude Code: 24 bits (FF5000), cajas redondas, ⏺ ⎿ ✻ ❯, la línea del «pensando» repintada en su sitio, la actualización sincronizada (2026) y sus preguntas (tamaño, cursor, fondo, DECRQM, DA)"
    else ko "el terminal con colores.sh:"; sed 's/^/        /' $B/colores.txt | head -20; fi
    for r in htop vim less; do
      command -v $r > /dev/null || continue
      case $r in
      htop) python3 -I $AQUI/nucleo32/ssh/pruebas/graba.py 100 30 $B/htop.bin 4 htop -d 10 ;;
      vim) python3 -I $AQUI/nucleo32/ssh/pruebas/graba.py 100 30 $B/vim.bin 4 vim -n -u NONE -N -c 'syntax on' -c 'set number' $AQUI/nucleo32/ssh/vt.h ;;
      less) python3 -I $AQUI/nucleo32/ssh/pruebas/graba.py 100 30 $B/less.bin 3 less $AQUI/nucleo32/ssh/vt.h ;;
      esac
      $B/vt_prueba 100 30 $B/$r.bin $B/$r.ppm > $B/$r.txt
    done
    if [ -s $B/htop.txt ] && grep -q '^fila 29: F1Help  F2Setup' $B/htop.txt && grep -q 'alterna 1' $B/htop.txt && grep -q 'Load average' $B/htop.txt; then
      ok "htop de verdad (100 x 30) en el terminal: los medidores, la lista y la barra de abajo, en la pantalla alternativa"
    elif [ -s $B/htop.txt ]; then ko "htop en el terminal: $(grep '^fila 0[0-2]\|^fila 29' $B/htop.txt | tr '\n' ' ')"; fi
    if [ -s $B/vim.txt ] && grep -q '^fila 00:   1 /\* vt.h' $B/vim.txt && grep -q 'alterna 1' $B/vim.txt; then
      ok "vim de verdad: los números de línea y los colores de la sintaxis, en la pantalla alternativa"
    elif [ -s $B/vim.txt ]; then ko "vim en el terminal: $(head -n 2 $B/vim.txt | tr '\n' ' ')"; fi
    if [ -s $B/less.txt ] && grep -q '^fila 00: /\* vt.h' $B/less.txt; then ok "less de verdad: el archivo, desde arriba"
    elif [ -s $B/less.txt ]; then ko "less en el terminal: $(head -n 2 $B/less.txt | tr '\n' ' ')"; fi
    [ -n "${CAPTURAS_A:-}" ] && { mkdir -p "$CAPTURAS_A"; cp $B/*.ppm "$CAPTURAS_A"/ 2> /dev/null; }
  else ko "vt.c no compila en el anfitrión: $(head -n 2 $B/gccvt.txt)"; fi
  # el receptor de «Enviar a Carlos»
  if command -v php > /dev/null && command -v curl > /dev/null; then
    local W=$B/php TG U L PPID_PHP
    rm -rf $W; mkdir -p $W/www/termo $W/datos/registro $W/ftp
    cp $AQUI/../tools/termo-subida/subir.php $W/www/termo/
    TG=$(head -c 32 /dev/urandom | od -An -tx1 | tr -d ' \n')
    echo "$(printf '%s' $TG | sha256sum | cut -d' ' -f1) Portatil-gamer prueba" > $W/testigos.txt
    local PPH=$(python3 -I -c 'import socket; s = socket.socket(); s.bind(("127.0.0.1", 0)); print(s.getsockname()[1])')
    TERMO_TESTIGOS=$W/testigos.txt TERMO_DATOS=$W/datos php -S 127.0.0.1:$PPH -t $W/www > $W/php.log 2>&1 &
    PPID_PHP=$!
    sleep 1
    printf 'a;b\r\n1;2\r\n' > $W/11221912.CSV; head -c 5000000 /dev/zero > $W/GRANDE.CSV; printf '<?php echo 1;' > $W/MALO.TXT
    U=http://127.0.0.1:$PPH/termo/subir.php; L=20261122-201500-ABCD
    {
      curl -s -w ' [%{http_code}]\n' --form-string testigo=$TG --form-string equipo=otro --form-string lote=$L -F archivo=@$W/11221912.CSV $U
      curl -s -w ' [%{http_code}]\n' --form-string testigo=$TG --form-string lote=$L -F "archivo=@$W/11221912.CSV;filename=../../x.php" $U
      curl -s -w ' [%{http_code}]\n' --form-string testigo=$(printf '%064d' 0) --form-string lote=$L -F archivo=@$W/11221912.CSV $U
      curl -s -w ' [%{http_code}]\n' --form-string testigo=$TG --form-string lote=$L -F archivo=@$W/GRANDE.CSV $U
      curl -s -w ' [%{http_code}]\n' --form-string testigo=$TG --form-string lote=$L -F archivo=@$W/MALO.TXT $U
      curl -s -w ' [%{http_code}]\n' --form-string testigo=$TG --form-string lote=../x -F archivo=@$W/11221912.CSV $U
    } | tr -d '\n' | sed 's/\]/]\n/g' > $W/resp.txt
    kill $PPID_PHP 2> /dev/null; wait $PPID_PHP 2> /dev/null
    if grep -q '^OK 11221912.CSV 10 \[200\]' $W/resp.txt && grep -q '^ERROR nombre o tipo que no vale.*\[415\]' $W/resp.txt && grep -q '^ERROR testigo desconocido o revocado \[403\]' $W/resp.txt \
       && grep -q '\[413\]' $W/resp.txt && grep -q '^ERROR no es un texto del Termómetro \[415\]' $W/resp.txt && grep -q '^ERROR lote que no vale \[400\]' $W/resp.txt \
       && [ -s $W/datos/entrantes/Portatil-gamer/$L/11221912.CSV ] && [ "$(stat -c %a $W/datos/entrantes/Portatil-gamer/$L/11221912.CSV)" = 600 ] \
       && [ "$(find $W/datos/entrantes -type f | wc -l)" = 1 ] && grep -q '127.0.0.1 Portatil-gamer 11221912.CSV OK' $W/datos/registro/subidas.log; then
      ok "subir.php: con el testigo, a entrantes/<equipo del testigo>/<lote>/ (0600); sin él (403), con un nombre o un tipo que no (415), de más de 4 MB (413), PHP disfrazado (415) o un lote raro (400), nada; y la IP y la hora en el registro"
    else ko "subir.php:"; sed 's/^/        /' $W/resp.txt; fi
    # termo-mover.sh: lo quieto, al FTP (de prueba)
    touch -d '-5 min' $W/datos/entrantes/Portatil-gamer/$L/11221912.CSV
    bash $AQUI/../tools/termo-subida/termo-mover.sh $W/datos/entrantes $W/ftp > $W/mover.txt 2>&1
    if [ -s $W/ftp/Portatil-gamer/Termometro/subidas/$L/11221912.CSV ] && [ ! -e $W/datos/entrantes/Portatil-gamer/$L/11221912.CSV ] \
       && [ "$(stat -c '%U %a' $W/ftp/Portatil-gamer/Termometro/subidas/$L/11221912.CSV)" = "root 644" ]; then
      ok "termo-mover.sh: lo que lleva quieto más de 2 min pasa a Pendientes/<equipo>/Termometro/subidas/<lote>/, de root y 0644"
    else ko "termo-mover.sh: $(cat $W/mover.txt | head -n 3)"; fi
  else echo "  (sin php o sin curl: sin el receptor de «Enviar a Carlos»)"; fi
}

parte_termo() {
echo "== (TERMO) Hispa-Pentium: el Termómetro (TERMO.BIN) y su memoria USB =="
local Y=$P/termo S C="o /b 0x70 0x63" L N
mkdir -p $Y
export MTOOLS_SKIP_CHECK=1
termo_anfitrion
if [ "$(id -u)" != 0 ] || ! command -v qemu-system-x86_64 > /dev/null; then echo "  (sin ser root o sin qemu-system-x86_64: sin el Termómetro en QEMU)"; return; fi
python3 -I $AQUI/../tests/acpi/termo-qemu.py $Y/termo.aml
if TERMO=1 USB_MB=32 TERMO_PARA=$Y/para.txt bash $AQUI/nucleo32/usb.sh $Y/termo.img > $Y/usb.txt 2>&1 && mtype -i $Y/termo.img@@1M ::boot/grub/grub.cfg | grep -q "escritorio .*termo"; then
  ok "usb.sh con TERMO=1: la memoria de 32 MB con TERMO.BIN y el menú que arranca el escritorio con «termo»"
else ko "usb.sh con TERMO=1: $(tail -n 2 $Y/usb.txt)"; return; fi
if mtype -i $Y/termo.img@@1M ::TERMO/ENVIO.INI | grep -q "^testigo=$(sed -n 's/^testigo=//p' $Y/para.txt)" && mtype -i $Y/termo.img@@1M ::TERMO/SSH.INI | grep -q '^equipo=' \
   && [ "$(mdir -b -i $Y/termo.img@@1M ::TERMO/SSH 2> /dev/null | grep -c 'ID_ED')" = 2 ] && grep -q '^llave=ssh-ed25519 ' $Y/para.txt \
   && grep -q "^sha256=$(sed -n 's/^testigo=//p' $Y/para.txt | tr -d '\n' | sha256sum | cut -d' ' -f1)$" $Y/para.txt; then
  ok "la memoria lleva su testigo (ENVIO.INI) y su llave propia (TERMO\\SSH\\ID_ED.SSH y .PUB, SSH.INI); el testigo, su SHA-256 y la llave pública, una vez, aparte"
else ko "lo de la memoria (ENVIO.INI, SSH.INI, la llave): $(cat $Y/para.txt 2> /dev/null | head -n 2)"; fi
# 1. la parada de seguridad; el ratón que pasa (sin repintar), lo ocupado, cada 2 s
termo_qemu $Y/termo.img t1 150 "-cpu Haswell" "38:mouse_move 40 30" "39:mouse_move -20 25" "40:mouse_move 35 -10" "41:mouse_move -40 -30" "42:sendkey f12" \
  "44:sendkey 1" "46:sendkey c" "58:$C" "58:o /b 0x71 91" "70:$C" "70:o /b 0x71 50"
mira "se abre solo al arrancar; el procesador emulado no da temperatura y se dice; la zona de ACPI, su ventilador y el SMBIOS" \
  "escritorio: lanzo .TERMO.BIN" "termo: procesador «Intel Core Processor (Haswell)»" \
  "termo: suceso: sensor «Zona ACPI ._TZ_.TZ01»: 45,0 °C (límite 100,0 °C)" "termo: suceso: ventilador 1: 2400 r.p.m." "equipo «QEMU "
mira "(EC) la zona TZ02 lee del controlador embebido (que en QEMU no contesta): el porqué, en el .LOG; lo que su _TMP quiere escribir en el EC, no se escribe" \
  "termo: suceso: sensor «Zona ACPI ._TZ_.TZ02»: sin dato (--): _TMP falla: AE_TIME" "EC: (TERMO) el AML quiere escribir 01h en EC.11h.: no se escribe (solo lectura)" \
  "termo: suceso: ACPI: el controlador embebido (EC), al leer las zonas: [0-9]* lecturas, [1-9][0-9]* sin respuesta a tiempo, [1-9][0-9]* escrituras del AML no hechas"
mira "Calentar (5 min): la tarea hija carga; a 91 °C de 100, la parada de seguridad; luego el enfriamiento y el resumen" \
  "termo: calentar 300 s (la carga en una tarea hija)" "termo: carga en marcha" "termo: fin de la carga a los [0-9]* s (PARADA DE SEGURIDAD); pico 91,0 °C" \
  "termo: carga parada" "termo: resumen: PARADA DE SEGURIDAD" "PELIGRO: se ha parado por seguridad" "termo: suceso: informe para el técnico: X:.TERMO.INFORME.TXT y INFORME.BMP"
if grep -q 'termo: estado fase=reposo .* sin_motivo=0$' $S && ! grep -q 'sin_motivo=[1-9]' $S; then
  ok "(PARPADEO) el ratón que solo pasa por encima del Termómetro no hace pintar nada (sin_motivo=0 en cada estado) y lo pintado se sube por trozos ($(sed -n 's/.* subidos=\([0-9]*\).*/\1/p' $S | tail -n 1) puntos en total)"
else ko "(PARPADEO) se pinta sin motivo: $(grep -o 'pintados=.*' $S | tail -n 1)"; fi
L=$(mdir -b -i $Y/termo.img@@1M ::TERMO 2> /dev/null | tr -d ' ')
if echo "$L" | grep -q 'LOG$' && echo "$L" | grep -q 'CSV$' && echo "$L" | grep -q 'TXT$' \
   && mtype -i $Y/termo.img@@1M $(echo "$L" | grep 'CSV$') | grep -q '^[0-9/]*;[0-9:]*;[0-9]*;carga;91,0;' \
   && mtype -i $Y/termo.img@@1M $(echo "$L" | grep 'LOG$') | grep -q 'SUCESO ACABA la carga .*PARADA DE SEGURIDAD' \
   && mtype -i $Y/termo.img@@1M $(echo "$L" | grep 'LOG$') | grep -q ' ARRANQUE '; then
  ok "en la memoria, \\TERMO: el CSV (91,0 en la carga), el .LOG (el arranque, los sucesos) y el resumen .TXT"
else ko "\\TERMO de la memoria: $(echo $L)"; fi
# cada 2 s por el reloj (el HP Mini apuntaba a veces cada 4 s)
N=$(mtype -i $Y/termo.img@@1M $(echo "$L" | grep 'CSV$') 2> /dev/null | awk -F';' 'NR > 1 && $3 ~ /^[0-9]+$/ { if (a != "") { d = $3 - a; if (d > 3) m++; n++ } a = $3 } END { printf "%d %d", n, m + 0 }')
if [ "${N% *}" -ge 40 ] && [ "${N#* }" -le 1 ]; then ok "el registro, cada 2 s por el reloj: ${N% *} líneas y ${N#* } huecos de más de 3 s (con la carga y la memoria USB escribiendo)"
else ko "el registro no va cada 2 s: líneas y huecos ${N:-?}"; fi
N=$(mtype -i $Y/termo.img@@1M $(echo "$L" | grep 'LOG$') 2> /dev/null | sed -n 's/.*DATOS fase=reposo.*ocupado=\([0-9]*\)%.*/\1/p' | sort -n | head -n 5 | tail -n 1)
if [ -n "$N" ] && [ "$N" -le 15 ]; then ok "lo ocupado del procesador en reposo, en el .LOG (ocupado=...): $N % (el núcleo descansa con hlt; el Termómetro duerme entre lecturas)"
else ko "lo ocupado en reposo: ${N:-no sale en el .LOG} %"; fi
# 2. el apagón, calentando
for f in $(mdir -b -i $Y/termo.img@@1M ::TERMO 2> /dev/null | grep -E '\.(LOG|CSV|TXT)$'); do mdel -i $Y/termo.img@@1M "$f" > /dev/null 2>&1; done
termo_qemu $Y/termo.img t2 70 "-cpu Haswell" "46:sendkey c" "56:$C" "56:o /b 0x71 80"
dd if=$Y/termo.img of=$Y/part.img bs=1M skip=1 status=none
L=$(mdir -b -i $Y/part.img ::TERMO 2> /dev/null | tr -d ' ')
if fsck.fat -n $Y/part.img > $Y/fsck.txt 2>&1 && mtype -i $Y/part.img $(echo "$L" | grep 'LOG$') | tail -n 1 | grep -q 'DATOS fase=carga max=80,0' \
   && mtype -i $Y/part.img $(echo "$L" | grep 'CSV$') | tail -n 1 | grep -q ';carga;80,0;'; then
  ok "el equipo se apaga de golpe calentando: la FAT sana y el .LOG y el .CSV acaban en la carga, a 80 °C"
else ko "el apagón: $(tail -n 2 $Y/fsck.txt | tr '\n' ' ')"; fi
# 3. la vez siguiente
local cpu="-cpu phenom"
[ -w /dev/kvm ] && cpu="-enable-kvm -cpu host"
termo_qemu $Y/termo.img t3 70 "$cpu"
mira "al arrancar otra vez: «la última vez se apagó durante la prueba, a 80 °C»; el MSR que no está, recogido ($cpu)" \
  "termo: aviso: La última vez .*durante la PRUEBA DE ESFUERZO (calentando), a las [0-9:]*, con el más caliente a 80,0 °C" \
  "termo: MSR 19Ch sin sensor: "
[ -w /dev/kvm ] && mira "con KVM: el MSR 19Ch, que este procesador no tiene, da la excepción 13 y el núcleo la recoge" "termo: MSR 19Ch sin sensor: no lo tiene (lectura recogida por el núcleo)"
# 6. (TERMO) los demás procesadores: -smp 4 (como el MSI GT80: 4 núcleos), sin y con KVM
mdel -i $Y/termo.img@@1M ::TERMO/INFORME.TXT > /dev/null 2>&1
termo_qemu $Y/termo.img t4 140 "-smp 4 -cpu Haswell" "44:sendkey 1" "46:sendkey c" "60:$C" "60:o /b 0x71 91" "72:$C" "72:o /b 0x71 50" "100:screendump $Y/smp.ppm"
mira "-smp 4 (sin KVM): al calentar, el núcleo despierta a los otros 3 procesadores, cargan (sus vueltas suben) y se duermen otra vez al parar" \
  "termo: suceso: la carga, en 4 procesadores (este y 3 más" "termo: los demás procesadores, al parar: 3, vueltas de carga: [1-9][0-9]* [1-9][0-9]* [1-9]" \
  "termo: suceso: informe para el técnico"
if mtype -i $Y/termo.img@@1M ::TERMO/INFORME.TXT 2> /dev/null | grep -q 'en 4 procesadores (hilos)' && mtype -i $Y/termo.img@@1M ::TERMO/INFORME.TXT | grep -q '^FRENADOS DEL PROCESADOR' \
   && mtype -i $Y/termo.img@@1M ::TERMO/INFORME.TXT | grep -q '^RESULTADO'; then
  ok "INFORME.TXT para el técnico: el equipo, la carga en 4 procesadores, cada sensor, los frenados y el resultado"
else ko "INFORME.TXT: $(mtype -i $Y/termo.img@@1M ::TERMO/INFORME.TXT 2> /dev/null | head -n 12 | tr '\n' ' ')"; fi
if [ -w /dev/kvm ]; then
  termo_qemu $Y/termo.img t5 120 "-enable-kvm -cpu host -smp 4" "48:sendkey 1" "50:sendkey c" "78:$C" "78:o /b 0x71 91"
  mira "-smp 4 con KVM: lo mismo (los 3, a cargar y luego, dormidos)" "termo: suceso: la carga, en 4 procesadores (este y 3 más" \
    "termo: los demás procesadores, al parar: 3, vueltas de carga: [1-9][0-9]* [1-9][0-9]* [1-9]"
fi
termo_qemu $Y/termo.img t6 70 "-cpu Haswell" "44:sendkey 1" "46:sendkey c" "56:$C" "56:o /b 0x71 91"
mira "con un solo procesador: lo dice y carga solo este" "termo: suceso: la carga, en este procesador solo: los demás no (ningún otro procesador ha contestado"
mira "(NVIDIA) sin gráficas NVIDIA (QEMU): lo dice, sin colgarse" "termo: suceso: gráficas NVIDIA Maxwell: ninguna en el PCI"
# 8. (TERMO) «Rendimiento» (la tecla R): las medidas, en RENDIMIE.TXT; y Esc la para
local cpum="-smp 2 -cpu Haswell"
[ -w /dev/kvm ] && cpum="-enable-kvm -cpu host -smp 2"
termo_qemu $Y/termo.img t9 170 "$cpum" "46:sendkey ret" "48:sendkey r"
mira "«Rendimiento»: un núcleo (SHA-256, criba, matrices), todos los procesadores, la memoria, la memoria USB y pintar; en RENDIMIE.TXT" \
  "termo: suceso: EMPIEZA la medida del rendimiento" "termo: rendimiento: 3/7 Todos los procesadores" "termo: rendimiento: 5/7 El disco" "termo: rendimiento: Hecho: SHA-256 [0-9]*,[0-9] MB/s"
if mtype -i $Y/termo.img@@1M ::TERMO/RENDIMIE.TXT 2> /dev/null | grep -q '^MEDIDA;' && mtype -i $Y/termo.img@@1M ::TERMO/RENDIMIE.TXT | grep -q 'Todos los procesadores (2): [0-9]*,[0-9] vueltas/s, [0-9]*,[0-9] veces' \
   && mtype -i $Y/termo.img@@1M ::TERMO/RENDIMIE.TXT | grep -q 'sin aceleración: mide la CPU pintando' && ! mdir -i $Y/termo.img@@1M ::TERMO/RENDIM.DAT > /dev/null 2>&1; then
  ok "RENDIMIE.TXT: la línea para la tabla, los dos procesadores, «sin aceleración» y el archivo de prueba de la memoria USB, borrado"
else ko "RENDIMIE.TXT: $(mtype -i $Y/termo.img@@1M ::TERMO/RENDIMIE.TXT 2> /dev/null | head -n 6 | tr '\n' ' ')"; fi
termo_qemu $Y/termo.img t10 75 "$cpum" "46:sendkey ret" "48:sendkey r" "56:sendkey esc"
mira "«Rendimiento» se para con Esc" "termo: rendimiento: Medida cancelada (Esc)"
# 7. (UEFI) la misma memoria por UEFI (OVMF, q35): el Termómetro, las zonas de
# la SSDT de prueba (ACPI.BIN con la RSDP que le pasa el núcleo) y la carga
termo_uefi
# 4. «Enviar a Carlos»
termo_enviar
# 5. «Terminal»
termo_terminal
[ -n "${CAPTURAS_A:-}" ] && { mkdir -p "$CAPTURAS_A"; cp $Y/ssh/*.ppm $Y/envio/*.ppm "$CAPTURAS_A"/ 2> /dev/null; }
}

# (TERMO) 4. «Enviar a Carlos»: sin red y con red (HTTPS de prueba)
termo_enviar() {
  local E=$Y/envio PPH PT PID_PHP PID_TLS TG cpu="-cpu Haswell"
  if ! command -v php > /dev/null || ! command -v openssl > /dev/null; then echo "  (sin php u openssl: sin «Enviar a Carlos» en QEMU)"; return; fi
  [ -w /dev/kvm ] && cpu="-enable-kvm -cpu host"
  rm -rf $E; mkdir -p $E/www/termo $E/datos/registro
  cp $AQUI/../tools/termo-subida/subir.php $E/www/termo/
  PPH=$(python3 -I -c 'import socket; s = socket.socket(); s.bind(("127.0.0.1", 0)); print(s.getsockname()[1])')
  python3 -I $AQUI/nucleo32/termo/pruebas/tls_delante.py $E/tls $PPH > $E/tls.err 2>&1 &
  PID_TLS=$!
  for _ in $(seq 50); do [ -s $E/tls/puerto ] && break; sleep 0.2; done
  PT=$(cat $E/tls/puerto 2> /dev/null)
  if [ -z "$PT" ] || ! TERMO=1 USB_MB=32 TERMO_PARA=$E/para.txt TERMO_SERVIDORES="https://10.0.2.2:$PT/termo/subir.php" TERMO_EQUIPO=Portatil-gamer \
       bash $AQUI/nucleo32/usb.sh $E/termo.img > $E/usb.txt 2>&1; then
    ko "«Enviar a Carlos»: no se puede preparar (el HTTPS de prueba o la memoria): $(tail -n 1 $E/usb.txt $E/tls.err 2> /dev/null | tr '\n' ' ')"
    kill $PID_TLS 2> /dev/null; return
  fi
  mmd -i $E/termo.img@@1M ::RAICES 2> /dev/null; mcopy -i $E/termo.img@@1M $E/tls/RAIZ.CER ::RAICES/PRUEBA.CER
  TG=$(sed -n 's/^testigo=//p' $E/para.txt)
  echo "$(sed -n 's/^sha256=//p' $E/para.txt) Portatil-gamer memoria de prueba" > $E/testigos.txt
  TERMO_TESTIGOS=$E/testigos.txt TERMO_DATOS=$E/datos php -S 127.0.0.1:$PPH -t $E/www > $E/php.log 2>&1 &
  PID_PHP=$!
  # sin red: lo dice (y cómo conectar el Wi-Fi)
  cp $E/termo.img $E/sin.img
  TQ_RED="-nic none" termo_qemu $E/sin.img e1 60 "$cpu" "45:sendkey e" "50:screendump $E/e1.ppm"
  mira "«Enviar a Carlos» sin red: lo dice, y cómo conectar el cable o el Wi-Fi" "termo: suceso: Enviar a Carlos: No hay red: enchufe el cable o conecte el Wi-Fi"
  # con red: por HTTPS
  TQ_RED="-nic user,model=e1000" termo_qemu $E/termo.img e2 130 "$cpu" "50:sendkey e" "120:screendump $E/e2.ppm"
  mira "«Enviar a Carlos» con red (cable): TERMO.BIN /ENVIAR sube por HTTPS cada archivo y acaba con «Hecho»" \
    "termo: suceso: Enviar a Carlos: se manda X:.TERMO" "termo: envío: Enviando [0-9]* archivos a 10.0.2.2" "termo: envío: Hecho: [0-9]* archivos enviados a Carlos"
  local D
  D=$(ls -d $E/datos/entrantes/Portatil-gamer/20* 2> /dev/null | head -n 1)
  if [ -n "$D" ] && ls $D | grep -q 'CSV$' && ls $D | grep -q 'LOG$' && [ -s $D/ARRANQUE.TXT ] && ! ls $D | grep -qi 'INI\|EST\|HEC' \
     && grep -q 'conexión TLS: TLSv1.2' $E/tls/tls.txt && grep -q ' Portatil-gamer .*CSV OK' $E/datos/registro/subidas.log; then
    ok "en el servidor de prueba, por TLS 1.2 y con el testigo de la memoria: el .CSV, el .LOG y el ARRANQUE.TXT en entrantes/Portatil-gamer/$(basename $D)/ (ni ENVIO.INI ni lo demás)"
  else ko "lo que llega al servidor de prueba: $(ls $D 2> /dev/null | tr '\n' ' ') $(tail -n 2 $E/tls/tls.txt 2> /dev/null | tr '\n' ' ')"; fi
  kill $PID_PHP $PID_TLS 2> /dev/null; wait $PID_PHP $PID_TLS 2> /dev/null
}

# (TERMO) 5. «Terminal»: SSH.BIN contra un sshd propio (puerto alto, solo 127.0.0.1, ForceCommand)
termo_terminal() {
  local H=$Y/ssh PS cpu="-cpu Haswell" R
  if ! [ -x /usr/sbin/sshd ]; then echo "  (sin sshd: sin «Terminal» en QEMU)"; return; fi
  [ -w /dev/kvm ] && cpu="-enable-kvm -cpu host"
  rm -rf $H; mkdir -p $H
  R=$H/resultado.txt
  PS=$(python3 -I -c 'import socket; s = socket.socket(); s.bind(("127.0.0.1", 0)); print(s.getsockname()[1])')
  ssh-keygen -q -t ed25519 -N '' -f $H/hk
  if ! TERMO=1 USB_MB=32 TERMO_PARA=$H/para.txt TERMO_SSH_EQUIPO=10.0.2.2 TERMO_SSH_PUERTO=$PS TERMO_SSH_USUARIO=root TERMO_SSH_CONOCIDO=$H/hk.pub TERMO_SSH_GRABAR=si TERMO_SSH_VENTANA=640x380 \
       bash $AQUI/nucleo32/usb.sh $H/termo.img > $H/usb.txt 2>&1; then
    ko "«Terminal»: no se puede hacer la memoria: $(tail -n 1 $H/usb.txt)"; return
  fi
  # la llave de la memoria, solo para la orden de la prueba (restrict, pty)
  echo "restrict,pty,command=\"$AQUI/nucleo32/termo/pruebas/sesion_ssh.sh $R $AQUI/nucleo32/ssh/pruebas\" $(sed -n 's/^llave=//p' $H/para.txt)" > $H/ak
  cat > $H/config <<EOF
Port $PS
ListenAddress 127.0.0.1
HostKey $H/hk
PidFile $H/sshd.pid
AuthorizedKeysFile $H/ak
StrictModes no
UsePAM no
PasswordAuthentication no
KbdInteractiveAuthentication no
PermitRootLogin forced-commands-only
AcceptEnv COLORTERM LANG
AllowTcpForwarding no
X11Forwarding no
PermitTunnel no
EOF
  /usr/sbin/sshd -f $H/config -E $H/sshd.log
  for _ in $(seq 30); do [ -s $H/sshd.pid ] && break; sleep 0.2; done
  cat > $H/guion.txt <<EOF
serie termo: en marcha 120
pausa 4
tecla t
serie ssh: (TERMO) ventana de 60
serie ssh: dentro, con la llave propia 60
archivo $R FASE colores 30
pausa 5
captura vt1
archivo $R FASE htop 30
pausa 4
captura vt2
archivo $R FASE vim 30
pausa 4
captura vt3
archivo $R FASE less 30
pausa 3
captura vt4
archivo $R FASE teclas 30
pausa 2
tecla up down esc ctrl-c shift-tab a ret
pausa 1
captura vt5
archivo $R teclas= 20
archivo $R FASE encoge 20
pausa 1
tecla alt-spc
pausa 1
tecla t ret
pausa 1
tecla right right right right right down down down ret
pausa 3
captura vt6
archivo $R FASE fin 30
pausa 3
EOF
  python3 -I $AQUI/nucleo32/termo/pruebas/conduce.py $H $H/guion.txt -- qemu-system-x86_64 $cpu -nic user,model=e1000 -m 512 -display none -no-reboot \
    -acpitable file=$Y/termo.aml -device qemu-xhci -drive file=$H/termo.img,format=raw,if=none,id=u -device usb-storage,drive=u,bootindex=1 > $H/conduce.err 2>&1
  [ -s $H/sshd.pid ] && kill "$(cat $H/sshd.pid)" 2> /dev/null
  mcopy -n -i $H/termo.img@@1M ::TERMO/SSH/GRABA.BIN $H/graba.bin 2> /dev/null
  tr -d '\r' < $H/serie.txt | iconv -f CP437 -t UTF-8 > $H/serie8.txt 2> /dev/null
  S=$H/serie8.txt
  mira "«Terminal» (la tecla T): SSH.BIN abre su ventana con el terminal nuevo y entra con la llave de la memoria (TERMO\\SSH), sin preguntar la huella (CONOCIDO.TXT)" \
    "termo: suceso: Terminal: SSH a root@10.0.2.2" "ssh: (TERMO) ventana de [0-9]* x [0-9]*: el terminal, de [0-9]* x [0-9]* (xterm-256color)" "ssh: dentro, con la llave propia$"
  local C0
  C0=$(sed -n 's/.*el terminal, de \([0-9]*\) x \([0-9]*\).*/\2 \1/p' $S | head -n 1)
  if grep -q '^TERM=xterm-256color COLORTERM=truecolor LANG=C.UTF-8 colores=256' $R 2> /dev/null && grep -q "^tamano=$C0$" $R; then
    ok "al otro lado: TERM=xterm-256color, COLORTERM=truecolor, LANG=C.UTF-8, 256 colores y el tamaño de la ventana ($C0)"
  else ko "lo que ve el otro lado: $(head -n 2 $R 2> /dev/null | tr '\n' ' ') (la ventana: $C0)"; fi
  if grep -q '^respuestas= *033 \[ 8 ; .* t 033 \[ .* R 033 \] 1 1 ; r g b : 1 5 1 5 / 1 8 1 8 / 1 d 1 d 033 \\ 033 \[ ? 2 0 2 6 ; 2 \$ y 033 \[ ? 6 2 ; 2 2' $R 2> /dev/null; then
    ok "las respuestas del terminal llegan al programa del otro lado (lo que pregunta Ink y compañía): el tamaño, el cursor, el color de fondo (OSC 11), el 2026 (DECRQM) y DA1"
  else ko "las respuestas del terminal: $(grep '^respuestas=' $R 2> /dev/null)"; fi
  if grep -q '^teclas= *1b 5b 41 1b 5b 42 1b 03 1b 5b 5a 61 0d' $R 2> /dev/null; then
    ok "las teclas, en crudo, como un xterm: arriba, abajo, Esc, Ctrl+C, Mayús+Tab (ESC [ Z), una letra e Intro"
  else ko "las teclas: $(grep '^teclas=' $R 2> /dev/null)"; fi
  if grep -q '^winch=' $R 2> /dev/null && [ "$(sed -n 's/^tamano_final=//p' $R)" != "$C0" ] && grep -q 'ssh: (TERMO) window-change: ' $S; then
    ok "cambiar la ventana de tamaño (Alt+Espacio, «Tamaño», flechas): SIGWINCH al otro lado con el tamaño nuevo ($(sed -n 's/^tamano_final=//p' $R), antes $C0): window-change"
  else ko "el tamaño al encoger: $(grep 'winch\|tamano_final' $R 2> /dev/null | tr '\n' ' '); $(tail -n 2 $H/conduce.txt | tr '\n' ' ')"; fi
  # las capturas: en la de colores, la franja de 24 bits (muchos colores distintos) y el naranja de las cajas
  if [ -s $H/vt1.ppm ] && python3 -I - $H/vt1.ppm <<'PY'
import sys
d = open(sys.argv[1], 'rb').read().split(b'\n', 3)
an, al = map(int, d[1].split()); p = d[3]
colores = set(p[i:i + 3] for i in range(0, len(p) - 2, 3))
naranja = bytes.fromhex('d77757') in colores
sys.exit(0 if len(colores) >= 100 and naranja else 1)
PY
  then ok "la captura de la ventana con colores.sh: la franja de 24 bits (más de 100 colores distintos) y las cajas en su naranja (D77757)"
  else ko "la captura de colores.sh (vt1.ppm) no tiene los colores de 24 bits"; fi
  for c in vt2 vt3 vt4; do [ -s $H/$c.ppm ] || ko "falta la captura $c (htop, vim o less)"; done
  [ -s $H/vt2.ppm ] && [ -s $H/vt3.ppm ] && [ -s $H/vt4.ppm ] && ok "htop, vim y less a pantalla completa por SSH, en la ventana (capturas vt2, vt3 y vt4)"
  if ! grep -q "excepción\|\*\*\*" $S; then ok "en la sesión, ni una excepción ni una queja del núcleo"; else ko "el núcleo se ha quejado: $(grep "excepción\|\*\*\*" $S | head -n 2)"; fi
}

# (TERMO) 7. por UEFI (OVMF), si lo hay
termo_uefi() {
  local OV=/usr/share/OVMF C="o /b 0x70 0x63"
  if [ ! -f $OV/OVMF_CODE_4M.fd ] || ! mdir -i $Y/termo.img@@1M ::EFI/BOOT/BOOTX64.EFI > /dev/null 2>&1; then
    echo "  (sin OVMF o sin GRUB EFI en la memoria: sin el Termómetro por UEFI)"; return
  fi
  cp $OV/OVMF_VARS_4M.fd $Y/vars64.fd
  TQ_FIRM="-machine q35 -fw_cfg name=opt/ovmf/X-PciMmio64Mb,string=0 -drive if=pflash,format=raw,readonly=on,file=$OV/OVMF_CODE_4M.fd -drive if=pflash,format=raw,file=$Y/vars64.fd" \
    termo_qemu $Y/termo.img t8 110 "-smp 2 -cpu Haswell" "60:sendkey 1" "62:sendkey c" "76:$C" "76:o /b 0x71 91"
  mira "por UEFI (OVMF): GRUB EFI arranca la primera entrada, la del Termómetro, que se abre solo; ACPI.BIN, con la RSDP que le pasa el núcleo, ve la zona de la SSDT de prueba" \
    "escritorio: lanzo .TERMO.BIN" "termo: suceso: sensor «Zona ACPI ._TZ_.TZ01»: 45,0 °C (límite 100,0 °C)" "termo: suceso: ACPI (ACPI.BIN): 2 zonas térmicas"
  mira "por UEFI, la carga en los 2 procesadores (el trampolín en la memoria baja libre del mapa de UEFI) y la parada de seguridad" \
    "termo: suceso: la carga, en 2 procesadores (este y 1 más" "termo: fin de la carga a los [0-9]* s (PARADA DE SEGURIDAD)"
  if ! grep -q "excepción\|\*\*\*" $S; then ok "por UEFI, ni una excepción ni una queja del núcleo"; else ko "por UEFI, el núcleo se queja: $(grep "excepción\|\*\*\*" $S | head -n 2)"; fi
}
