# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# pruebas_informe.sh - (INFORME-DISP) la parte 19 bis de pruebas.sh (se lee
# desde allí: usa ok, ko, mira, miracap, saca, $AQUI, $P y $G):
# «Enviar informe de los dispositivos que fallan» del Administrador de
# dispositivos (DISPOSIT.BIN, nucleo32/udisposi.cas; la ficha del SMBIOS sin
# números de serie, la 226, 8, de termoram.cas) y su receptor
# (tools/informe-disp/informe.php e informe-mover.sh). SOLO=informe, o con
# SOLO=dispositivos (detrás de la 19).
#   1. El receptor con curl (php -S en 127.0.0.1): solo POST, solo texto
#      UTF-8 (ni otro tipo, ni «<?php», ni «<script», ni binario, ni UTF-8 mal
#      hecho, ni lo que no empieza por la primera línea del informe), 413
#      con más de la cuenta, el tope por IP y el del día (429), y lo guardado
#      fuera de la web, 0600, con el nombre que pone él; el cron
#      (informe-mover.sh) lo deja en el FTP de prueba como
#      Informes-dispositivos/<AAAAMMDD-HHMMSS>-<fabricante-modelo>/INFORME.TXT
#      (de root, 0644), y no toca lo de nombre raro.
#   2. En QEMU, en la Consola, con un sistema instalado (C:) con un usuario
#      NORMAL que entra solo (Biblio2; el administrador, Carlos, con
#      contraseña), un SMBIOS con número de serie, UUID, SKU y el de la placa,
#      y un ARRANQUE.TXT y un WIFI.LOG con claves, una red, un BSSID, una MAC,
#      una IP pública, la ruta de un documento, los nombres de los usuarios y
#      del equipo, «MSDM» y HTML: DISPOSIT /ENVIAR /SI por HTTPS (el servidor
#      de prueba de «Enviar a Carlos»: tls_delante.py delante de php -S) =
#      «ENVIADO (número de informe ...)»; otra vez = el tope (429, «NO
#      ENVIADO: el servidor lo rechaza: ...»); a otro con 2 KB como mucho
#      (413). Lo que llega y lo que se enseña antes: sin ninguno de esos datos
#      (se buscan a propósito); la copia en sus Documentos
#      (\USUARIOS\BIBLIO2\DOCUMENT\INFDISP.TXT), igual que lo que ha llegado.
#   3. Sin red: «NO ENVIADO: no hay red» y la copia, igual.
#   4. En el escritorio: el botón abajo de la ventana (Mi equipo >
#      Propiedades), la vista de lo que se manda con «Enviar» y «Cancelar»,
#      Av Pág, «Cancelar» (no se manda nada), y «Enviar» sin red: «NO
#      ENVIADO» en rojo y la copia, y «Volver a la lista».
informe_disp() {
  echo "== Hispa-Pentium 0.8.12: «Enviar informe de los dispositivos que fallan» (DISPOSIT.BIN y tools/informe-disp) =="
  local I=$P/infdisp T=$AQUI/../tools/informe-disp PPH PPB PT PID_A PID_B PID_T U H f d n
  if ! command -v php > /dev/null || ! command -v curl > /dev/null || ! command -v openssl > /dev/null; then
    echo "  (sin php, curl u openssl: sin el informe de los dispositivos)"; return
  fi
  rm -rf $I; mkdir -p $I/a/www/informe $I/a/datos/registro $I/b/www/informe $I/b/datos/registro $I/ftp
  export MTOOLS_SKIP_CHECK=1
  # ---- 1. el receptor, con curl ----
  cp $T/informe.php $I/a/www/informe/enviar.php; cp $T/informe.php $I/b/www/informe/enviar.php
  libre() { python3 -I -c 'import socket; s = socket.socket(); s.bind(("127.0.0.1", 0)); print(s.getsockname()[1])'; }
  PPH=$(libre)
  INFORME_DATOS=$I/a/datos INFORME_POR_IP=3 INFORME_POR_DIA=4 php -S 127.0.0.1:$PPH -t $I/a/www > $I/a/php.log 2>&1 &
  PID_A=$!
  for _ in $(seq 50); do curl -s -o /dev/null http://127.0.0.1:$PPH/ && break; sleep 0.1; done
  U=http://127.0.0.1:$PPH/informe/enviar.php; H='Content-Type: text/plain; charset=utf-8'
  cu() { curl -s -w ' [%{http_code}]' "$@" | tr -d '\n'; echo; }   # «ERROR motivo [415]», en una línea
  printf 'Hispa-Pentium: informe de los dispositivos que fallan\r\nEquipo (SMBIOS; sin el número de serie ni el UUID):\r\n  Fabricante: TOSHIBA\r\n  Modelo: NB250 ../../../etc/x\r\n' > $I/ok.txt
  {
    cu $U
    cu -H 'Content-Type: application/x-httpd-php' --data-binary @$I/ok.txt $U
    printf '<?php system("id"); ?>\r\nHispa-Pentium: informe de los dispositivos que fallan, de mentira' | cu -H "$H" --data-binary @- $U
    (cat $I/ok.txt; printf '<script>alert(1)</script>\r\n') | cu -H "$H" --data-binary @- $U
    (cat $I/ok.txt; printf 'mal: \xff\xfe\r\n') | cu -H "$H" --data-binary @- $U
    (cat $I/ok.txt; printf 'binario: \x00\x01\r\n') | cu -H "$H" --data-binary @- $U
    printf 'Otra cosa que no es un informe de los de Hispa-Pentium, con letras de más para llegar\r\n' | cu -H "$H" --data-binary @- $U
    (cat $I/ok.txt; head -c 300000 /dev/zero | tr '\0' a) | cu -H "$H" --data-binary @- $U
    for _ in 1 2 3 4; do cu -H "$H" --data-binary @$I/ok.txt $U; done
  } > $I/resp.txt 2>&1
  f=
  for l in '^ERROR solo POST \[405\]$' '^ERROR tiene que ser texto (text/plain; charset=utf-8) \[415\]$' '^ERROR lleva «<» (ni HTML ni PHP) \[415\]$' \
           '^ERROR no es texto UTF-8 \[415\]$' '^ERROR lleva caracteres que no son de texto \[415\]$' '^ERROR no es un informe de los dispositivos de Hispa-Pentium \[415\]$' \
           '^ERROR el informe es demasiado grande (256 KB como mucho) \[413\]$' '^ERROR desde esta conexión ya se han mandado 3 informes hoy; pruebe mañana \[429\]$'; do
    grep -q -- "$l" $I/resp.txt || f="$f «$l»"
  done
  [ "$(grep -c '^OK 20[0-9]\{6\}-[0-9]\{6\}-[0-9A-F]\{4\} \[200\]$' $I/resp.txt)" = 3 ] || f="$f (no son 3 OK)"
  [ "$(grep -c '«<»' $I/resp.txt)" = 2 ] || f="$f (no son dos «<»)"
  n=$(ls $I/a/datos/informes/ 2> /dev/null | grep -c '^20[0-9]\{6\}-[0-9]\{6\}-[0-9A-F]\{4\}_TOSHIBA-NB250-etc-x\.TXT$')
  [ "$n" = 3 ] && [ "$(ls -A $I/a/datos/informes | wc -l)" = 3 ] || f="$f (lo guardado: $(ls -A $I/a/datos/informes | tr '\n' ' '))"
  [ -z "$(find $I/a/datos/informes -type f ! -perm 0600)" ] || f="$f (no todo es 0600)"
  grep -q ' 127.0.0.1 ERROR 405 solo POST$' $I/a/datos/registro/informes.log && grep -q ' 127.0.0.1 OK 20' $I/a/datos/registro/informes.log || f="$f (el registro)"
  if [ -z "$f" ]; then
    ok "informe.php: solo POST (405); solo text/plain UTF-8: otro tipo, «<?php», «<script», UTF-8 mal hecho, binario o lo que no es un informe, 415; más de 256 KB, 413; 3 por IP (429); lo guardado, fuera de la web, 0600 y con su nombre (el fabricante y el modelo, saneados: nada de «../»); y la IP y la hora en su registro"
  else ko "informe.php:$f"; sed 's/^/        /' $I/resp.txt | head -20; fi
  # el tope del día (4): como si llegaran de otra IP (sin la cuenta de esta), uno más; y luego, ninguno
  rm -f $I/a/datos/informes-cuotas/ip-*
  cu -H "$H" --data-binary @$I/ok.txt $U > $I/dia.txt 2>&1
  rm -f $I/a/datos/informes-cuotas/ip-*
  cu -H "$H" --data-binary @$I/ok.txt $U >> $I/dia.txt 2>&1
  if grep -q '^OK 20.* \[200\]$' $I/dia.txt && grep -q '^ERROR hoy ya han llegado muchos informes; pruebe mañana \[429\]$' $I/dia.txt; then ok "informe.php: y el tope del día (200; aquí, 4), venga de la IP que venga"
  else ko "informe.php: el tope del día: $(cat $I/dia.txt)"; fi
  # el cron: al FTP de prueba (lo de nombre raro, no)
  printf 'raro\r\n' > "$I/a/datos/informes/20261125-101010-ABCD_..%2F..TXT"
  printf 'raro\r\n' > "$I/a/datos/informes/nada.TXT"
  touch -d '-3 min' $I/a/datos/informes/*
  bash $T/informe-mover.sh $I/a/datos $I/ftp > $I/mover.txt 2>&1
  n=$(find $I/ftp/Informes-dispositivos -mindepth 2 -maxdepth 2 -type f -name INFORME.TXT -perm 0644 -user root | grep -c '/20[0-9]\{6\}-[0-9]\{6\}\(-[0-9A-F]\{4\}\)\?-TOSHIBA-NB250-etc-x/INFORME.TXT$')
  if [ "$n" = 4 ] && [ "$(ls $I/a/datos/informes | tr '\n' ' ')" = "20261125-101010-ABCD_..%2F..TXT nada.TXT " ] && cmp -s $I/ok.txt "$(find $I/ftp -name INFORME.TXT | head -n 1)" \
     && [ "$(grep -c 'se deja (nombre raro)' $I/mover.txt)" = 2 ]; then
    ok "informe-mover.sh: los 4 al FTP de prueba (Informes-dispositivos/<AAAAMMDD-HHMMSS>-TOSHIBA-NB250-etc-x/INFORME.TXT, de root y 0644, iguales a lo mandado); los de nombre raro se quedan donde están"
  else ko "informe-mover.sh: $n; $(cat $I/mover.txt | tr '\n' ' '); $(find $I/ftp -type f | tr '\n' ' ')"; fi
  kill $PID_A 2> /dev/null; wait $PID_A 2> /dev/null
  # ---- 2. en QEMU: por HTTPS, como un usuario normal ----
  local Y=$I/q
  mkdir -p $Y
  bash $AQUI/../acpi/pentium/hacer.sh $Y/ACPI.BIN > $Y/hacer.txt 2>&1 || ko "acpi/pentium/hacer.sh no hace ACPI.BIN: $(tail -n 1 $Y/hacer.txt)"
  bash $AQUI/nucleo32/tls/hacer.sh $Y/TLS.BIN > $Y/tls.txt 2>&1 || ko "nucleo32/tls/hacer.sh no hace TLS.BIN: $(tail -n 1 $Y/tls.txt)"
  python3 -I $AQUI/ccastel.py $AQUI/nucleo32/semilla.cas -o $Y/semilla.asm > /dev/null && nasm -f bin $Y/semilla.asm -o $Y/SEMILLA.BIN || ko "semilla.cas no compila"
  for u in udisposi:DISPOSIT uequipo:EQUIPO uordenes:ORDENES ured:RED; do
    python3 -I $AQUI/ccastel.py $AQUI/nucleo32/${u%%:*}.cas -o $Y/${u%%:*}.asm > /dev/null && nasm -f bin $Y/${u%%:*}.asm -o $Y/${u##*:}.BIN \
      || ko "${u%%:*}.cas no compila"
  done
  # el disco: un sistema instalado (\HISPA\ORDENES.BIN), con sus paquetes,
  # un usuario normal que entra solo (Biblio2) y un administrador con
  # contraseña (Carlos), sus servicios (RED.BIN y TLS.BIN, en \HISPA: los
  # de un sistema instalado salen de ahí), y unos registros con todo lo que no debe salir
  python3 - $Y <<'PY'
import sys
Y = sys.argv[1]
a = ["Hispa-Pentium: el registro del arranque (de prueba)",
     "usb: aparato 0, 0627:0001, velocidad alta, en el puerto 1 (EHCI)",
     "usb: 0627:0001 clave=hunter2",
     "usb: 0627:0001 contraseña de la red: Secreto99",
     "usb: 0627:0001 guardado en C:\\USUARIOS\\BIBLIO2\\DOCUMENT\\CARTA.TXT y en C:\\HISPA\\RED.INI",
     "usb: 0627:0001 desde 85.10.20.30 y desde 192.168.1.5",
     "usb: 0627:0001 MAC 52:54:00:AB:CD:EF",
     "usb: 0627:0001 SSID RedDeMaria",
     "usb: 0627:0001 se conecta a la red «RedDeMaria»",
     "usb: 0627:0001 la tabla MSDM en 7FFE0000",
     "usb: 0627:0001 número de serie ABC123",
     "usb: 0627:0001 lo enchufa Biblio2 en PORTATIL-DE-MARIA, y Carlos lo mira",
     "usb: 0627:0001 <script>alert(1)</script> <?php system('id'); ?>",
     "una línea que no toca a nada"]
open(Y + '/ARRANQUE.TXT', 'wb').write(('\r\n'.join(a) + '\r\n').encode('cp437'))
w = ["wifi: Hispa-Pentium WIFI.BIN (de prueba)",
     "wifi: la tarjeta 0627:0001 ve «RedDeMaria» BSSID 11:22:33:44:55:66 a -40 dBm",
     "wifi: 0627:0001 Clave de «RedDeMaria» preparada"]
open(Y + '/WIFI.LOG', 'wb').write(('\r\n'.join(w) + '\r\n').encode('cp437'))
open(Y + '/HISPA.INI', 'wb').write(b'usuario=Maria\r\nequipo=PORTATIL-DE-MARIA\r\nteclado=es\r\n')
open(Y + '/USUARIOS.TXT', 'wb').write(b'HISPA-USUARIOS 1\r\nu=1|N|0|1|BIBLIO2|Biblio2|\r\nu=2|A|0|0|CARLOS|Carlos|argon2id$12288$3$' + b'0' * 32 + b'$' + b'1' * 64 + b'\r\n')
open(Y + '/BASE.PQI', 'wb').write(b'HPQ 1\r\nnombre=base\r\nversion=0.8.12\r\nfin\r\n')
open(Y + '/EQUIPO.PQI', 'wb').write(b'HPQ 1\r\nnombre=equipo\r\nversion=0.9.6\r\nfin\r\n')
PY
  rm -f $Y/d.img
  mkfs.fat -C -F 16 -n SISTEMA $Y/d.img 16384 > /dev/null && mmd -i $Y/d.img ::HISPA ::HISPA/PAQUETES ::HISPA/RAICES ::DOCUMENT ::USUARIOS ::USUARIOS/BIBLIO2 ::USUARIOS/BIBLIO2/DOCUMENT \
    && mcopy -i $Y/d.img $Y/ARRANQUE.TXT :: && mcopy -i $Y/d.img $Y/ORDENES.BIN $Y/WIFI.LOG $Y/HISPA.INI $Y/USUARIOS.TXT ::HISPA/ \
    && mcopy -i $Y/d.img $Y/BASE.PQI $Y/EQUIPO.PQI ::HISPA/PAQUETES/ && mcopy -i $Y/d.img $Y/RED.BIN $Y/TLS.BIN ::HISPA/ || ko "no se puede hacer el disco de la prueba"
  PPH=$(libre); PPB=$(libre)
  INFORME_DATOS=$I/a/datos INFORME_POR_IP=1 php -S 127.0.0.1:$PPH -t $I/a/www > $I/a/php2.log 2>&1 &
  PID_A=$!
  INFORME_DATOS=$I/b/datos INFORME_MAX=2048 php -S 127.0.0.1:$PPB -t $I/b/www > $I/b/php.log 2>&1 &
  PID_B=$!
  rm -rf $I/a/datos/informes $I/a/datos/informes-cuotas
  python3 -I $AQUI/nucleo32/termo/pruebas/tls_delante.py $Y/tls $PPH > $Y/tls.err 2>&1 &
  PID_T=$!
  for _ in $(seq 50); do [ -s $Y/tls/puerto ] && break; sleep 0.2; done
  PT=$(cat $Y/tls/puerto 2> /dev/null)
  mcopy -i $Y/d.img $Y/tls/RAIZ.CER ::HISPA/RAICES/PRUEBA.CER 2> /dev/null || ko "sin la raíz de prueba del HTTPS: $(tail -n 2 $Y/tls.err)"
  local SM="-smbios type=1,manufacturer=Hispa-Pruebas,product=Portatil-X1,version=1.0,serial=SERIE-SECRETA-123,uuid=12345678-90ab-cdef-1234-567890abcdef,sku=SKU-SECRETO,family=Familia-X -smbios type=2,manufacturer=Placas-SA,product=PL-9,serial=PLACA-SECRETA-9,asset=INVENTARIO-7 -smbios type=0,vendor=BIOS-SA,version=2.3,date=01/02/2020"
  local M="$Y/ACPI.BIN,$Y/DISPOSIT.BIN,$Y/ORDENES.BIN,$Y/RED.BIN,$Y/TLS.BIN" UH="https://10.0.2.2:$PT/informe/enviar.php"
  cp $Y/d.img $Y/e.img
  ESCRIBE=1 DISCO=$Y/e.img MEMORIA=128 MODULOS=$M ARRANQUE="ordenes conred acpicada=1" QEMU_MAS="$SM -nic user,model=e1000 -device usb-ehci,id=e -device usb-tablet,bus=e.0" \
    $AQUI/nucleo32/probar.sh $Y/SEMILLA.BIN ":pausa 8" "m:\\disposit /enviar /si /servidor=$UH" ":hasta 60 1 disposit: informe: [EN]" \
    "m:\\disposit /enviar /si /servidor=$UH" ":hasta 40 2 disposit: informe: [EN]" \
    "m:\\disposit /enviar /si /servidor=http://10.0.2.2:$PPB/informe/enviar.php" ":hasta 40 3 disposit: informe: [EN]" ":pausa 1" > $Y/a.sal 2>&1
  tr -d '\r' < $Y/a.sal > $Y/a.txt
  S=$Y/a.txt
  mira "entra Biblio2 (un usuario normal) y DISPOSIT /ENVIAR /SI enseña lo que se va a mandar: la versión y los paquetes, el equipo (SMBIOS), el procesador, lo que falla con su ficha y sus líneas de los registros, y la lista entera" \
    "^Entra Biblio2 (sin contraseña)\.$" "^==== Lo que se va a mandar (exactamente esto) ====$" "^Hispa-Pentium: informe de los dispositivos que fallan$" "^Hispa-Pentium: ${VERSION_BASE//./\\.} (instalado en el disco)$" \
    "^Paquetes: base 0\.8\.12, equipo 0\.9\.6$" "^Equipo (SMBIOS; sin el número de serie ni el UUID):$" "^  Fabricante: Hispa-Pruebas$" "^  Modelo: Portatil-X1$" "^  Familia: Familia-X$" \
    "^  Placa: Placas-SA PL-9 " "^  BIOS: BIOS-SA, versión 2\.3, del 01/02/2020$" "^Procesador: " "^Resumen: [0-9]* dispositivos; " "^== Lo que falla: [1-9][0-9]* ==$" \
    "^-- \[!\]  QEMU USB Tablet  \[0627:0001\]$" "^       Controlador intentado: ninguno (Hispa-Pentium no tiene uno para él)$" "^       Identificador: USB 0627:0001$" \
    "^       ARRANQUE.TXT: usb: aparato 0, 0627:0001, velocidad alta" "^== Todos los dispositivos ==$" "^  \[OK\] Intel PRO/1000 MT (82540EM)  \[8086:100E; bus PCI, 00:03\.0; funciona; RED\.BIN\]$" \
    "^== Fin del informe ==$"
  mira "y lo que no debe salir, saneado: la ruta del documento, la IP pública, la MAC, la red (y el BSSID, sin sus 3 últimos octetos), los nombres de los usuarios y del equipo, y el HTML" \
    "^       ARRANQUE.TXT: usb: 0627:0001 guardado en (ruta) y en C:.HISPA.RED.INI$" "^       ARRANQUE.TXT: usb: 0627:0001 desde (IP pública) y desde 192.168.1.5$" \
    "^       ARRANQUE.TXT: usb: 0627:0001 MAC 52:54:00:xx:xx:xx$" "^       ARRANQUE.TXT: usb: 0627:0001 se conecta a la red «(quitado)»$" \
    "^       ARRANQUE.TXT: usb: 0627:0001 lo enchufa (nombre) en (nombre), y (nombre) lo mira$" "^       ARRANQUE.TXT: usb: 0627:0001 (script)alert(1)(/script) (?php system('id'); ?)$" \
    "^       WIFI.LOG: wifi: la tarjeta 0627:0001 ve «(quitado)» BSSID 11:22:33:xx:xx:xx a -40 dBm$"
  mira "por HTTPS (TLS 1.2, con la raíz de prueba), al servidor de prueba: ENVIADO con su número; la segunda vez, el tope (429); y a otro de 2 KB como mucho, demasiado grande (413): NO ENVIADO, con el motivo en llano" \
    "^disposit: informe: ENVIADO (número de informe 20[0-9]\{6\}-[0-9]\{6\}-[0-9A-F]\{4\})$" \
    "^disposit: informe: NO ENVIADO: el servidor lo rechaza: desde esta conexión ya se han mandado 1 informes hoy; pruebe mañana (429)$" \
    "^disposit: informe: NO ENVIADO: el servidor lo rechaza: el informe es demasiado grande (2 KB como mucho) (413)$" \
    "^Copia guardada en C:.USUARIOS.BIBLIO2.DOCUMENT.INFDISP.TXT (para mandarla a mano, si hace falta)\.$"
  f=$(ls $I/a/datos/informes/*.TXT 2> /dev/null | head -n 1)
  saca $Y/e.img USUARIOS/BIBLIO2/DOCUMENT/INFDISP.TXT $Y/copia.txt
  if [ -n "$f" ] && [ "$(ls $I/a/datos/informes | wc -l)" = 1 ] && [[ $(basename $f) =~ ^20[0-9]{6}-[0-9]{6}-[0-9A-F]{4}_Hispa-Pruebas-Portatil-X1\.TXT$ ]] \
     && iconv -f UTF-8 -t UTF-8 $f > /dev/null 2>&1 && head -n 1 $f | grep -q '^Hispa-Pentium: informe de los dispositivos que fallan.$' && grep -q 'Versión: 1\.0' $f \
     && [ "$(grep -c $'\r$' $f)" = "$(wc -l < $f)" ] && [ -s $Y/copia.txt ] && cmp -s $f $Y/copia.txt && grep -q 'conexión TLS: TLSv1.2' $Y/tls/tls.txt; then
    ok "lo que llega al servidor de prueba (por TLS 1.2): UTF-8 con \\r\\n, en informes/<número>_Hispa-Pruebas-Portatil-X1.TXT, y la copia de los Documentos de Biblio2, igual octeto a octeto"
  else ko "lo que llega al servidor: $(ls $I/a/datos/informes 2> /dev/null | tr '\n' ' ') $(tail -n 2 $Y/tls/tls.txt 2> /dev/null | tr '\n' ' ') (copia: $(stat -c %s $Y/copia.txt 2> /dev/null))"; fi
  # se buscan a propósito: ni en lo enseñado ni en lo que llega
  sed -n '/^==== Lo que se va a mandar/,/^==== Fin de lo que se va a mandar/p' $S > $Y/vista.txt
  f="$f $Y/vista.txt"
  local malo=
  for p in SERIE-SECRETA 'PLACA-SECRETA' 'INVENTARIO-7' 'SKU-SECRETO' '12345678-90ab' '1234567890ABCDEF' '567890abcdef' 'MSDM' 'hunter2' 'Secreto99' 'RedDeMaria' \
           'SSID RedDe' '44:55:66' 'AB:CD:EF' '85\.10\.20\.30' 'CARTA\.TXT' 'BIBLIO2\\DOCUMENT' 'Biblio2' 'Carlos' 'Maria' 'PORTATIL-DE' '<' 'número de serie ABC'; do
    if grep -q -i -- "$p" $f; then malo="$malo «$p»"; fi
  done
  if [ -z "$malo" ] && [ -s $Y/vista.txt ]; then
    ok "buscados a propósito, ni en la vista ni en lo que llega: el número de serie (del equipo y de la placa), la etiqueta de inventario, el SKU, el UUID, la MSDM, las claves, la red (SSID), el BSSID y la MAC enteros, la IP pública, el documento, los nombres de los usuarios y del equipo, «<»"
  else ko "sale lo que no debe:$malo"; fi
  # ---- 3. sin red ----
  cp $Y/d.img $Y/s.img
  ESCRIBE=1 DISCO=$Y/s.img MEMORIA=128 MODULOS=$M ARRANQUE="ordenes acpicada=1" QEMU_MAS="-device usb-ehci,id=e -device usb-tablet,bus=e.0" \
    $AQUI/nucleo32/probar.sh $Y/SEMILLA.BIN ":pausa 8" "m:\\disposit /enviar /si /servidor=$UH" ":hasta 60 1 disposit: informe: NO" ":pausa 1" > $Y/s.sal 2>&1
  tr -d '\r' < $Y/s.sal > $Y/s.txt
  S=$Y/s.txt
  mira "sin red: «NO ENVIADO: no hay red», con cómo conectar, y la copia en los Documentos" \
    "^NO ENVIADO: no hay red (enchufe el cable o conecte el Wi-Fi en Configuración > Red y Wi-Fi)$" \
    "^Copia guardada en C:.USUARIOS.BIBLIO2.DOCUMENT.INFDISP.TXT (para mandarla a mano, si hace falta)\.$" "^(M:.DISPOSIT.BIN ha acabado con el código 1)$"
  if saca $Y/s.img USUARIOS/BIBLIO2/DOCUMENT/INFDISP.TXT $Y/copia2.txt && iconv -f UTF-8 -t UTF-8 $Y/copia2.txt > /dev/null 2>&1 && grep -q '^== Fin del informe ==.$' $Y/copia2.txt \
     && ! grep -q -i 'SERIE-SECRETA\|hunter2\|RedDeMaria' $Y/copia2.txt; then ok "y la copia (UTF-8, entera) está en el disco, para mandarla a mano"
  else ko "sin red, la copia no está en los Documentos de Biblio2"; fi
  kill $PID_A $PID_B $PID_T 2> /dev/null; wait $PID_A $PID_B $PID_T 2> /dev/null
  # ---- 4. en el escritorio (sin red: nada sale de esta máquina) ----
  cp $Y/d.img $Y/w.img
  mdel -i $Y/w.img ::HISPA/USUARIOS.TXT
  CAPTURAS=$G ESCRIBE=1 SATA=$Y/w.img MODULOS=$Y/ACPI.BIN,$Y/DISPOSIT.BIN,$Y/EQUIPO.BIN ARRANQUE="escritorio unidad=M acpicada=1" \
    QEMU_MAS="-device usb-ehci,id=e -device usb-tablet,bus=e.0" $AQUI/nucleo32/probar.sh $Y/SEMILLA.BIN ":pausa 7" \
    ":clicd 63 40" ":pausa 1" ":clic 100 70" ":pausa 4" ":captura id1" ":tecla tab" ":tecla ret" ":pausa 3" ":captura id2" ":tecla pgdn" ":pausa 1" ":captura id3" \
    ":tecla esc" ":pausa 1" ":captura id4" ":clic 236 503" ":pausa 3" ":clic 84 527" ":raton 600 300" ":pausa 6" ":captura id5" ":tecla ret" ":pausa 1" ":captura id6" > $Y/w.sal 2>&1
  tr -d '\r' < $Y/w.sal > $Y/w.txt
  S=$Y/w.txt
  miracap "Configuración › Dispositivos (el Administrador): «Enviar informe de los dispositivos que fallan», abajo" id1 \
    negrita 30 15 400 40 "Administrador de dispositivos" negrita 40 480 500 520 "Enviar informe de los dispositivos que fallan" \
    negrita 40 120 400 480 "[+] Pantalla (2)" texto 380 120 620 160 "todo funciona" negrita 40 120 400 480 "[-] Teclado y ratón (3)" texto 380 120 620 480 "1 NO VA" negrita 40 120 760 480 "! QEMU USB Tablet"
  miracap "al pulsarlo (Tab, Intro): la vista, en texto, de exactamente lo que se manda, con «Enviar» y «Cancelar»" id2 \
    negrita 40 40 500 70 "Enviar informe de los dispositivos que fallan" texto 40 60 760 120 "exactamente lo que se va a mandar" \
    texto 40 110 760 140 "Hispa-Pentium: informe de los dispositivos que fallan" negrita 40 505 140 540 "Enviar" negrita 120 505 260 540 "Cancelar"
  miracap "Av Pág baja por el texto" id3 notexto 40 110 760 140 "Hispa-Pentium: informe de los dispositivos que fallan" texto 40 480 760 510 "Renglones 2"
  miracap "«Cancelar» (Esc): de vuelta a la lista, sin mandar nada" id4 texto 500 520 790 545 "No se ha enviado nada"
  miracap "«Enviar» sin red: «NO ENVIADO: no hay red» y dónde está la copia; «Volver a la lista»" id5 \
    negrita 40 40 760 70 "NO ENVIADO: no hay red" texto 40 60 760 110 "Copia guardada en C:" negrita 40 505 300 540 "Volver a la lista"
  miracap "y de vuelta a la lista" id6 texto 500 520 790 545 "Informe NO enviado"
  mira "en el escritorio: la vista, Cancelar (nada) y Enviar (sin red)" "^disposit: informe: la vista de lo que se manda ([0-9]* letras)$" \
    "^disposit: informe: cancelado (no se ha mandado nada)$" "^disposit: informe: NO ENVIADO: no hay red"
  if ! cat $Y/a.sal $Y/s.sal $Y/w.sal | grep -q "excepción\|\*\*\*"; then ok "y ni una excepción del núcleo"
  else ko "el núcleo se ha quejado:"; cat $Y/a.sal $Y/s.sal $Y/w.sal | grep "excepción\|\*\*\*" | head -3 | sed 's/^/        /'; fi
}
