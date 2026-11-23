# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# pruebas_compat.sh - la parte 49 de pruebas.sh (SOLO=compat, (COMPAT) el
# Asistente de compatibilidad, castel/nucleo32/compat), que lee este archivo
# («.»): la función parte_compat, con los ayudantes de pruebas.sh (ok, ko,
# mira) y sus carpetas ($AQUI, $P, $G).
#
# NO baja nada de Microsoft ni usa ningún juego de verdad: el «juego de
# mentira» lo hace compat/pruebas/haz_juego.py (unos PE que no hacen nada),
# lo que imita a download.microsoft.com es un servidor de aquí (php -S con un
# «redistribuible» de mentira, un ejecutable con un CAB dentro que lleva otro
# CAB) y el receptor de los informes es tools/compat-subida/informe.php con
# php -S, detrás de un HTTPS de prueba (compat/pruebas/tls_delante.py, con
# su raíz en \RAICES), todo en 127.0.0.1 (desde QEMU, 10.0.2.2).
#
# 1. En Linux (con ASan y UBSan): el juego de mentira con WIN32 COMPRUEBA de
#    mentira (compat/pruebas/falso_win32.py, sus dos salidas: la de las
#    personas y la /MAQUINA), lo agrupado por DLL, el informe (sin rutas),
#    mandarlo al receptor, lo que el receptor no deja (testigo, forma, PHP
#    disfrazado, una ruta con un nombre, topes, bloqueado), el cron que lo
#    pasa al FTP y el resumen acumulado, «avisar cuando ya esté», el
#    redistribuible de mentira (bajado ya: la suma, la licencia, el CAB de
#    dentro y las DLL por su nombre de exportación), un LZX hecho aquí, y la
#    ventana con un guion (las capturas, en docs/pentium/compat).
# 2. En QEMU, la Consola, con el WIN32.BIN de verdad: COMPAT /ORDEN
#    comprueba, informe, envia (por HTTPS, TLS.BIN), componentes y pon (con
#    el modo administrador, «S»), y PAQUETES ACTUALIZA con un catálogo de
#    prueba (firmado con una clave de prueba) que trae un win32 «nuevo»: el
#    gancho de PAQUETES lanza COMPAT /REVISA y avisa de lo que ya va.
# 3. En QEMU, el escritorio a 1024 x 600: la ventana, el consentimiento, el
#    envío, los componentes y la licencia, el aviso de la bandeja tras
#    PAQUETES ACTUALIZA y la ventana de «/FALLO» (las capturas, en $G y en
#    docs/pentium/compat).
# Necesita gcc, php, openssl, curl, mtools y qemu.

parte_compat() {
echo "== (COMPAT) El Asistente de compatibilidad (COMPAT.BIN): qué le falta a un programa de Windows, el informe, el aviso y los componentes de Microsoft =="
local Y=$P/compat C=$AQUI/nucleo32/compat R=$AQUI/../tools/compat-subida AN BIN S PPH PT PID_PHP PID_TLS TG U L J W
if ! command -v php > /dev/null || ! command -v openssl > /dev/null || ! command -v curl > /dev/null; then
  echo "  (sin php, openssl o curl: sin la parte del Asistente de compatibilidad)"; return
fi
rm -rf $Y; mkdir -p $Y $G
J=$Y/j
python3 -I $C/pruebas/haz_juego.py $J
AN=$Y/compat-anfitrion
if ! bash $C/anfitrion.sh $AN asan > $Y/anf.txt 2>&1; then ko "el Asistente no se compila en Linux: $(tail -n 3 $Y/anf.txt | tr '\n' ' ')"; return; fi
BIN=$Y/COMPAT.BIN
if bash $C/hacer.sh $BIN > $Y/hacer.txt 2>&1; then ok "COMPAT.BIN se compila (-march=i586, sin CMOV: $(stat -c %s $BIN) octetos) y el Asistente en Linux, con ASan y UBSan"
else ko "COMPAT.BIN no se compila: $(tail -n 3 $Y/hacer.txt | tr '\n' ' ')"; return; fi
# --- 1. en Linux ---
W=$Y/anf
mkdir -p $W/sis/HISPA/PAQUETES $W/sis/HISPA/COMPAT $W/cap
cp -r $J/W32 $W/sis/WIN32
printf 'HPQ 1\r\nnombre=win32\r\nversion=1.2\r\nfin\r\n' > $W/sis/HISPA/PAQUETES/WIN32.PQI
printf 'HPQ 1\r\nnombre=base\r\nversion=0.8.7\r\nfin\r\n' > $W/sis/HISPA/PAQUETES/BASE.PQI
cl() { COMPAT_SISTEMA=$W/sis COMPAT_WIN32="python3 -I $C/pruebas/falso_win32.py $AQUI/nucleo32/win32 $W/sis/WIN32" ASAN_OPTIONS=detect_leaks=0 $AN "$@" 2>> $W/err.txt; }
S=$W/uno.txt
{ cl /ORDEN comprueba $J/JUEGOMEN; cl /ORDEN comprueba $J/TODOBIEN.EXE; cl /ORDEN informe $J/JUEGOMEN; cl /ORDEN lista; } > $S
mira "Linux: el juego de mentira, agrupado por DLL: lo que falta de verdad (PRUEBAC.DLL, 5) y aparte lo que no hace falta (el instalador, el servidor dedicado, Glide, A3D)" \
  "^\[FALTA\] JUEGOMEN: faltan 5 (y 5 más en partes que no hacen falta)\.$" "^  Falta la DLL entera PRUEBAC.DLL (5 funciones)$" "^    Lo pide: JUEGO.EXE$" \
  "^  SETUP.EXE: es un instalador o un desinstalador" "^  HLDS.EXE: es el servidor dedicado" "^  Falta la DLL entera GLIDE2X.DLL (2 funciones)$" \
  "^    Gráficos 3D: Glide 2, de las tarjetas 3dfx Voodoo" "^    Sonido: el sonido 3D de las tarjetas Aureal (de los 90). Es opcional" "^  Falta la DLL entera FANTASMA.DLL (2 funciones)$"
mira "Linux: el informe que se mandaría: solo los nombres (sin carpetas), los tamaños, las sumas SHA-256, lo que falta y las versiones" \
  "^HISPA-COMPAT 1$" "^hispa-pentium=0\.8\.7$" "^win32=1\.2$" "^archivos=4$" "^archivo=JUEGO.EXE	[0-9]*	[0-9a-f]\{64\}	ventanas	6	si$" \
  "^archivo=SETUP.EXE	[0-9]*	[0-9a-f]\{64\}	ventanas	2	instalador$" "^faltadll=JUEGO.EXE	PRUEBAC.DLL	5$" "^faltadll=MOTOR.DLL	GLIDE2X.DLL	2$" "^fin$"
if sed -n '/^HISPA-COMPAT 1$/,/^fin$/p' $S | grep -q "$J\|/\|\\\\"; then ko "¡el informe lleva una ruta!: $(sed -n '/^HISPA-COMPAT/,/^fin/p' $S | grep "$J\|/\|\\\\" | head -2)"
else ok "Linux: el informe no lleva ninguna ruta (ni «/» ni «\\»), solo nombres de archivo"; fi
e=$(sha256sum $J/JUEGOMEN/JUEGO.EXE | cut -c1-64)
grep -q "^archivo=JUEGO.EXE	1536	$e	" $S && ok "Linux: la suma SHA-256 del informe es la de verdad (la de sha256sum)" || ko "la suma de JUEGO.EXE no es la de sha256sum ($e)"
mira "Linux: lo comprobado queda en la carpeta del usuario (PROGRAMA.LST), con la versión de win32" "^2 programas comprobados:$" "^  JUEGOMEN  (.*)  win32 1\.2: faltan 10, 5 importantes" "^  TODOBIEN  (.*)  win32 1\.2: falta 1, 1 importantes"
# la salida /MAQUINA (la que se ha pedido a win32-hl): lo mismo, por otro camino
S=$W/maquina.txt
FALSO_MAQUINA=0 cl /ORDEN comprueba $J/JUEGOMEN > $S
mira "Linux: con la salida de texto (la de un WIN32 de antes de la 1.3, que no sabe /MAQUINA: la reserva), lo mismo" "^\[FALTA\] JUEGOMEN: faltan 5 (y 5 más en partes que no hacen falta)\.$" "^  Falta la DLL entera PRUEBAC.DLL (5 funciones)$"
# el receptor: php -S
U=$Y/php; mkdir -p $U/www/compat $U/www/download/x $U/datos/registro
cp $R/informe.php $U/www/compat/
cp $J/REDIST/PRUEBA.EXE $U/www/download/x/vcredist_x86.exe
: > $U/bloqueados.txt
PPH=$(python3 -I -c 'import socket; s = socket.socket(); s.bind(("127.0.0.1", 0)); print(s.getsockname()[1])')
COMPAT_DATOS=$U/datos COMPAT_BLOQUEADOS=$U/bloqueados.txt php -S 127.0.0.1:$PPH -t $U/www > $U/php.log 2>&1 &
PID_PHP=$!
for _ in $(seq 50); do curl -s http://127.0.0.1:$PPH/compat/informe.php > $U/listo.txt 2>&1 && break; sleep 0.2; done
printf 'servidor=http://127.0.0.1:%s/compat/informe.php\r\n' $PPH > $W/sis/HISPA/COMPAT/COMPAT.INI
S=$W/envia.txt
{ cl /ORDEN envia $J/JUEGOMEN; cl /ORDEN envia $J/JUEGOMEN /ACEPTO; } > $S
mira "Linux: «envia» sin /ACEPTO solo lo enseña; con /ACEPTO lo manda (multipart, con el testigo del equipo) y el receptor contesta OK" \
  "^(No se ha mandado nada: para mandarlo, añada /ACEPTO\.)$" "^Mandado: OK 20[0-9]\{6\}/[0-9]\{6\}-[0-9a-f]\{8\}-[0-9a-f]\{6\}\.txt$"
f=$(ls $U/datos/entrantes/*/*.txt 2> /dev/null | head -1)
if [ -n "$f" ] && cmp -s <(sed -n '/^HISPA-COMPAT 1$/,/^fin$/p' $W/envia.txt | head -n $(wc -l < $f)) $f && [ "$(stat -c %a $f)" = 600 ] \
   && [ "$(wc -c < $W/sis/USUARIOS/PRUEBA/COMPAT/TESTIGO.TXT)" = 64 ]; then
  ok "en el receptor: el informe tal cual lo enseñó el Asistente (0600, en entrantes/<día>/); el testigo, 64 cifras en la carpeta del usuario"
else ko "lo que ha llegado al receptor no es lo enseñado ($f)"; fi
TG=$(head -c 32 /dev/urandom | od -An -tx1 | tr -d ' \n')
L="http://127.0.0.1:$PPH/compat/informe.php"
printf 'HISPA-COMPAT 1\nhispa-pentium=0.8.7\nwin32=1.2\nasistente=1.0\narchivos=1\narchivo=A.EXE\t10\t-\tventanas\t1\tsi\nfalta=A.EXE\tX.DLL\tF\nfin\n' > $U/bien.txt
printf 'HISPA-COMPAT 1\nhispa-pentium=0.8.7\nwin32=1.2\nasistente=1.0\narchivos=1\narchivo=C:\\USUARIOS\\ANA\\A.EXE\t10\t-\tventanas\t1\tsi\nfin\n' > $U/ruta.txt
printf '<?php echo 1; ?>\n' > $U/php.txt
head -c 300000 /dev/zero | tr '\0' 'a' > $U/grande.txt
manda() { curl -s -w ' [%{http_code}]' --form-string testigo=$1 --form-string protocolo=1 -F "informe=@$U/$2;filename=INFORME.TXT" $L | tr -d '\n'; echo; }
{ manda $TG bien.txt; manda $TG ruta.txt; manda $TG php.txt; manda $TG grande.txt; manda xyz bien.txt; { curl -s -w ' [%{http_code}]' $L | tr -d '\n'; echo; }
  echo "$(printf '%s' $TG | sha256sum | cut -c1-64) bloqueado de prueba" >> $U/bloqueados.txt; manda $TG bien.txt
  for i in 1 2 3 4 5 6; do manda $(head -c 32 /dev/urandom | od -An -tx1 | tr -d ' \n') bien.txt; done; } > $U/resp.txt 2>&1
if grep -q '^OK .* \[200\]$' $U/resp.txt && [ "$(grep -c 'no tiene la forma de los del Asistente \[415\]' $U/resp.txt)" = 2 ] && grep -q '\[413\]' $U/resp.txt \
   && grep -q 'testigo que no vale \[403\]' $U/resp.txt && grep -q 'solo POST \[405\]' $U/resp.txt && grep -q 'este equipo no puede mandar informes \[403\]' $U/resp.txt \
   && grep -q 'muchos equipos nuevos desde aquí; mañana más \[429\]' $U/resp.txt && ! grep -q "127.0.0.1" $U/datos/registro/informes.log; then
  ok "informe.php: lo bueno, OK; una ruta con un nombre de usuario o PHP disfrazado (415), de más de 240 KB (413), sin testigo (403), un GET (405), un equipo bloqueado (403), más de 5 equipos nuevos al día desde una IP (429); y en el registro, ninguna IP"
else ko "informe.php no se porta como debe:"; sed 's/^/        /' $U/resp.txt | head -14; fi
# el cron: al FTP (aquí, una carpeta) y el resumen
mkdir -p $U/ftp
touch -d '5 minutes ago' $U/datos/entrantes/*/*.txt
sed 's/install -o root -g root -m 0644/install -m 0644/' $R/compat-mover.sh > $U/mover.sh
COMPAT_RESUMEN=$R/compat-resumen.py bash $U/mover.sh $U/datos/entrantes $U/ftp $U/datos > $U/mover.txt 2>&1
if [ "$(ls $U/ftp/*/*.txt 2> /dev/null | wc -l)" -ge 5 ] && [ -z "$(ls $U/datos/entrantes/*/*.txt 2> /dev/null)" ] && grep -q '^PRUEBAC.DLL;;1;0;1;JUEGO.EXE$' $U/ftp/resumen.csv \
   && grep -q '^X.DLL;F;1;0;4;A.EXE$' $U/ftp/resumen.csv && grep -q '^FANTASMA.DLL;;0;1;1;$' $U/ftp/resumen.csv && python3 -I -c "import json,sys; d=json.load(open(sys.argv[1])); assert d['programas'] == 2 and d['win32'] == {'1.2': 2}" $U/ftp/resumen.json; then
  ok "compat-mover.sh pasa los informes al FTP (por días) y compat-resumen.py hace el resumen acumulado (CSV y JSON): cada programa una vez, lo que no hace falta aparte"
else ko "el cron o el resumen no van:"; sed 's/^/        /' $U/mover.txt $U/ftp/resumen.csv 2> /dev/null | head -10; fi
# avisar cuando ya esté: un win32 «nuevo» con PRUEBAC.DLL (3 de sus 5)
cp $J/NUEVO/PRUEBAC.DLL $W/sis/WIN32/
printf 'HPQ 1\r\nnombre=win32\r\nversion=1.99\r\nfin\r\n' > $W/sis/HISPA/PAQUETES/WIN32.PQI
S=$W/revisa.txt
{ cl /ORDEN revisa; cl /ORDEN revisa; cl /ORDEN lista; } > $S
mira "Linux: al cambiar win32 (1.2 a 1.99, con 3 de las 5 de PRUEBAC.DLL), se vuelve a comprobar solo y se dice qué ha cambiado; la segunda vez, nada" \
  "^JUEGOMEN: ahora faltan 7 (antes 10)\.$" "^TODOBIEN: ya no falta nada; pruébelo de nuevo\.$" "^Nada nuevo (win32 1\.99)\.$" "^  TODOBIEN  (.*)  win32 1\.99: faltan 0, 0 importantes"
grep -q 'TODOBIEN: ya no falta nada' $W/sis/USUARIOS/PRUEBA/COMPAT/AVISOS.TXT && ok "Linux: los avisos quedan también en COMPAT\\AVISOS.TXT, con la fecha y la versión" || ko "AVISOS.TXT no tiene el aviso"
rm -f $W/sis/WIN32/PRUEBAC.DLL
printf 'HPQ 1\r\nnombre=win32\r\nversion=1.2\r\nfin\r\n' > $W/sis/HISPA/PAQUETES/WIN32.PQI
# el redistribuible de mentira (ya bajado: la suma, la licencia, las DLL)
e=$(sha256sum $J/REDIST/PRUEBA.EXE | cut -c1-64)
{ printf '[prueba]\nnombre=Componente de prueba (de mentira)\nurl=http://127.0.0.1:%s/download/x/vcredist_x86.exe\n' $PPH
  printf 'tam=%s\nsha256=%s\ninterior=INTERIOR.CAB\ndlls=PRUEBA1.DLL,PRUEBA2.DLL\nlicencia=LICENCIA.TXT\npara=Para las pruebas del Asistente.\n' $(stat -c %s $J/REDIST/PRUEBA.EXE) $e; } > $W/sis/HISPA/COMPAT/REDIST.LST
S=$W/redist.txt
mkdir -p $W/sacadas
{ cl /ORDEN componentes; cl /ORDEN cab $J/REDIST/PRUEBA.EXE; cl /ORDEN saca $J/REDIST/PRUEBA.EXE prueba $W/sacadas; COMPAT_PIDE_ADMIN=1 cl /ORDEN pon prueba; COMPAT_PIDE_ADMIN=0 cl /ORDEN pon prueba /ACEPTO; COMPAT_PIDE_ADMIN=1 cl /ORDEN pon prueba /ACEPTO; } > $S
mira "Linux: el redistribuible de mentira: un .EXE con un CAB (MSZIP) que lleva la licencia y otro CAB con las DLL con nombres de Windows Installer; se reconocen por su nombre de exportación" \
  "^prueba  Componente de prueba (de mentira)  \[sin poner\]$" "^carpeta 0: MSZIP (ventana 0)$" "^ *[0-9]*  \.\\\\INTERIOR\.CAB$" "^Sacadas: 2\.$"
mira "Linux: bajarlo (http de prueba: la suma comprobada), enseñar su licencia; sin /ACEPTO no se pone nada; sin el modo administrador, tampoco; con él, sus DLL en \\WIN32" \
  "^Bajado y comprobado (SHA-256 $e)" "^LICENCIA DE PRUEBA DE HISPA-DOS$" "^No se pone nada: para aceptar la licencia y poner las DLL, añada /ACEPTO\.$" \
  "^No se han puesto: Para poner las DLL en .WIN32 hace falta el modo administrador (se ha dicho que no)\.$" "^Puestas en .WIN32: 2\.$"
python3 -I - $J/REDIST/PRUEBA.EXE $W/sis/WIN32/PRUEBA1.DLL $W/sis/WIN32/PRUEBA2.DLL $W/sacadas/PRUEBA1.DLL <<'PY' > $W/redist2.txt 2>&1 && ok "las DLL puestas son las de dentro, octeto a octeto (la de 101 KB, en tres bloques MSZIP con su diccionario)" || ko "las DLL puestas no son las de dentro: $(cat $W/redist2.txt)"
import sys
d1 = open(sys.argv[2], 'rb').read(); d2 = open(sys.argv[3], 'rb').read(); s1 = open(sys.argv[4], 'rb').read()
assert d1 == s1 and len(d1) > 70000 and d1[:2] == b'MZ' and d2[:2] == b'MZ' and b'PRUEBA2.DLL' in d2, (len(d1), len(d2))
PY
# un LZX hecho aquí (compat/pruebas/haz_lzx.py: bloques literales, alineados y sin comprimir, con E8)
if python3 -I $C/pruebas/haz_lzx.py $Y/lzx > $Y/lzx.txt 2>&1; then
  { cl /ORDEN cab $Y/lzx/PRUEBA.CAB; for n in $(cat $Y/lzx/nombres.txt); do cl /ORDEN cab $Y/lzx/PRUEBA.CAB $n $Y/lzx/sale-$n; done; } > $Y/lzx2.txt
  mal=0; for n in $(cat $Y/lzx/nombres.txt); do cmp -s $Y/lzx/$n $Y/lzx/sale-$n || mal="$mal $n"; done
  if [ "$mal" = 0 ] && grep -q '^carpeta 0: LZX (ventana 1[5-9]\|^carpeta 0: LZX (ventana 2[01]' $Y/lzx2.txt; then
    ok "LZX: lo hecho aquí (literales, alineados, sin comprimir, coincidencias largas, la traducción E8, varios bloques de 32 K) sale igual octeto a octeto ($(cat $Y/lzx/nombres.txt | wc -w) archivos)"
  else ko "LZX: no sale igual:$mal ($(head -3 $Y/lzx2.txt | tr '\n' ' '))"; fi
else ko "haz_lzx.py no va: $(tail -2 $Y/lzx.txt)"; fi
if grep -q "Sanitizer\|runtime error" $W/err.txt; then ko "ASan o UBSan se han quejado: $(grep -m1 'Sanitizer\|runtime error' $W/err.txt)"; else ok "Linux: ni una queja de ASan ni de UBSan"; fi
# la ventana en Linux (con su guion): las capturas
cat > $W/guion.txt <<'GUI'
captura cpl1
tecla f5
captura cpl2
GUI
OFI_GUION=$W/guion.txt OFI_CAPTURAS=$W/cap cl $J/JUEGOMEN > $W/ventana.txt 2>&1
[ -f $W/cap/cpl1.ppm ] && ok "Linux: la ventana con su guion (captura cpl1)" || ko "Linux: la ventana no hace su captura"
kill $PID_PHP 2> /dev/null; wait $PID_PHP 2> /dev/null
# --- 2. en QEMU, la Consola, con WIN32.BIN de verdad ---
export MTOOLS_SKIP_CHECK=1
local Q=$Y/q HPQ="python3 -I $AQUI/../tools/hpq.py"
mkdir -p $Q/f/paq $Q/www/compat $Q/www/download/x $Q/datos/registro
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/semilla.cas -o $Q/semilla.asm > /dev/null && nasm -f bin $Q/semilla.asm -o $Q/SEMILLA.BIN || { ko "semilla.cas no compila"; return; }
if ! bash $AQUI/nucleo32/win32/disco.sh $Q/base.img > $Q/disco.txt 2>&1 || ! bash $AQUI/nucleo32/tls/hacer.sh $Q/TLS.BIN > $Q/tls.txt 2>&1; then
  ko "el disco de WIN32 o TLS.BIN no se hacen: $(tail -n 2 $Q/disco.txt $Q/tls.txt | tr '\n' ' ')"; return
fi
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/uhttps.cas -o $Q/uhttps.asm > /dev/null && nasm -f bin $Q/uhttps.asm -o $Q/HTTPS.BIN
# PAQUETES con la clave de prueba (como la parte de los paquetes) y el catálogo con el win32 «nuevo»
$HPQ clave $Q/prueba.key > /dev/null && $HPQ publica $Q/prueba.key $Q/clave.cas > /dev/null
cp $AQUI/nucleo32/upaquete.cas $Q/upaquete.cas
python3 -I $AQUI/ccastel.py $Q/upaquete.cas -o $Q/upaquete.asm -I $AQUI/nucleo32 > /dev/null && nasm -f bin $Q/upaquete.asm -o $Q/PAQUETES.BIN || { ko "PAQUETES (con la clave de prueba) no compila"; return; }
printf 'win32 (de prueba)\r\n' > $Q/f/LEEME12.TXT
cp $J/NUEVO/PRUEBAC.DLL $Q/f/
( cd $Q/f
  $HPQ crea W12.HPQ --nombre win32 --version 1.2 --descripcion 'win32 (de prueba)' LEEME12.TXT='\WIN32\LEEME12.TXT'
  $HPQ registro W12.HPQ WIN32.PQI
  $HPQ crea B087.HPQ --nombre base --version 0.8.7 --descripcion 'El sistema base (de prueba)' LEEME12.TXT='\HISPA\BASE.TXT'
  $HPQ registro B087.HPQ BASE.PQI
  $HPQ crea paq/WIN32.HPQ --nombre win32 --version 1.99 --descripcion 'win32 (de prueba, con PRUEBAC.DLL)' LEEME12.TXT='\WIN32\LEEME12.TXT' PRUEBAC.DLL='\WIN32\PRUEBAC.DLL'
  $HPQ catalogo paq --clave $Q/prueba.key ) > $Q/hpq.txt 2>&1 || { ko "hpq.py: $(tail -2 $Q/hpq.txt)"; return; }
mkfs.fat -F 16 -n MEMORIA -C $Q/usb.img 16384 > /dev/null && mmd -i $Q/usb.img ::PAQUETES && mcopy -i $Q/usb.img $Q/f/paq/* ::PAQUETES/
# los servidores: el receptor y lo que imita a download.microsoft.com, detrás del HTTPS de prueba
cp $R/informe.php $Q/www/compat/
cp $J/REDIST/PRUEBA.EXE $Q/www/download/x/vcredist_x86.exe
: > $Q/bloqueados.txt
PPH=$(python3 -I -c 'import socket; s = socket.socket(); s.bind(("127.0.0.1", 0)); print(s.getsockname()[1])')
COMPAT_DATOS=$Q/datos COMPAT_BLOQUEADOS=$Q/bloqueados.txt php -S 127.0.0.1:$PPH -t $Q/www > $Q/php.log 2>&1 &
PID_PHP=$!
python3 -I $C/pruebas/tls_delante.py $Q/tls $PPH > $Q/tls.err 2>&1 &
PID_TLS=$!
for _ in $(seq 50); do [ -s $Q/tls/puerto ] && break; sleep 0.2; done
PT=$(cat $Q/tls/puerto 2> /dev/null)
if [ -z "$PT" ]; then ko "el HTTPS de prueba no arranca: $(tail -n 2 $Q/tls.err)"; kill $PID_PHP $PID_TLS 2> /dev/null; return; fi
# el disco: el de WIN32 (FAT32, WIN32.BIN y \WIN32), con TLS, la raíz de prueba, el Asistente,
# \HISPA\COMPAT (la lista de prueba y adónde mandar), lo instalado (base y win32 de prueba) y el juego
e=$(sha256sum $J/REDIST/PRUEBA.EXE | cut -c1-64)
{ printf '[prueba]\r\nnombre=Componente de prueba (de mentira)\r\nurl=https://10.0.2.2:%s/download/x/vcredist_x86.exe\r\n' $PT
  printf 'tam=%s\r\nsha256=%s\r\ninterior=INTERIOR.CAB\r\ndlls=PRUEBA1.DLL,PRUEBA2.DLL\r\nlicencia=LICENCIA.TXT\r\npara=Para las pruebas del Asistente.\r\n' $(stat -c %s $J/REDIST/PRUEBA.EXE) $e; } > $Q/REDIST.LST
printf 'servidor=https://10.0.2.2:%s/compat/informe.php\r\n' $PT > $Q/COMPAT.INI
cp $Q/base.img $Q/d.img
mmd -i $Q/d.img ::RAICES ::HISPA ::HISPA/COMPAT ::HISPA/PAQUETES ::JUEGOMEN \
 && mcopy -i $Q/d.img $Q/tls/RAIZ.CER ::RAICES/PRUEBA.CER && mcopy -i $Q/d.img $Q/TLS.BIN $Q/HTTPS.BIN $BIN :: \
 && mcopy -i $Q/d.img $Q/PAQUETES.BIN ::HISPA/ && mcopy -i $Q/d.img $Q/REDIST.LST $Q/COMPAT.INI ::HISPA/COMPAT/ \
 && mcopy -i $Q/d.img $Q/f/WIN32.PQI $Q/f/BASE.PQI ::HISPA/PAQUETES/ && mcopy -i $Q/d.img $Q/f/LEEME12.TXT ::WIN32/ \
 && mcopy -i $Q/d.img $J/JUEGOMEN/* ::JUEGOMEN/ && mcopy -i $Q/d.img $J/TODOBIEN.EXE :: || { ko "no se puede preparar el disco"; kill $PID_PHP $PID_TLS; return; }
cp $Q/d.img $Q/c.img
cp $Q/d.img $Q/v.img
ESCRIBE=1 DISCO=$Q/c.img ARRANQUE=ordenes MODULOS= MEMORIA=128 \
  QEMU_MAS="-nic user,model=e1000 -device usb-ehci,id=u -drive id=mu,file=$Q/usb.img,format=raw,if=none,snapshot=on -device usb-storage,bus=u.0,drive=mu" \
  $AQUI/nucleo32/probar.sh $Q/SEMILLA.BIN ":pausa 2" \
  "compat /orden comprueba C:\\JUEGOMEN" ":hasta 90 1 apuntado en la lista" "compat /orden comprueba C:\\TODOBIEN.EXE" ":hasta 60 2 apuntado en la lista" \
  "compat /orden informe C:\\JUEGOMEN" ":hasta 60 1 ---- fin ----" "compat /orden envia C:\\JUEGOMEN /acepto" ":hasta 120 1 Mandado:\\|No se ha mandado:" \
  "compat /orden componentes" ":pausa 3" "compat /orden pon prueba /acepto" ":hasta 120 1 MODO ADMINISTRADOR" ":escribe s" ":hasta 120 1 Puestas en\\|No se han puesto" ":pausa 2" \
  "compat /orden lista" ":pausa 3" "paquetes actualiza" ":hasta 240 1 compat: aviso: TODOBIEN" ":pausa 5" "compat /orden lista" ":pausa 4" "type \\usuarios\\usuario\\compat\\avisos.txt" ":pausa 2" > $Q/consola.sal 2>&1
tr -d '\r' < $Q/consola.sal > $Q/consola.txt
S=$Q/consola.txt
mira "QEMU: COMPAT /ORDEN comprueba con el WIN32.BIN de verdad (WIN32 COMPRUEBA ... /MAQUINA, el de win32 1.3): el juego de mentira, agrupado" \
  "^\[FALTA\] JUEGOMEN: faltan 5 (y 5 más en partes que no hacen falta)\.$" "^  Falta la DLL entera PRUEBAC.DLL (5 funciones)$" "^  HLDS.EXE: es el servidor dedicado" \
  "^\[FALTA\] TODOBIEN: falta 1\.$" "^(apuntado en la lista del usuario: win32 1\.2)$"
mira "QEMU: el informe, sin rutas; y mandado por HTTPS (TLS.BIN, la raíz de prueba de \\RAICES) al receptor de prueba" \
  "^archivo=JUEGO.EXE	1536	$(sha256sum $J/JUEGOMEN/JUEGO.EXE | cut -c1-64)	ventanas	6	si$" "^Mandado: OK 20[0-9]\{6\}/"
f=$(ls $Q/datos/entrantes/*/*.txt 2> /dev/null | head -1)
if [ -n "$f" ] && grep -q '^faltadll=JUEGO.EXE	PRUEBAC.DLL	5$' $f && ! grep -q 'C:\|USUARIO\|\\' $f && grep -q 'conexión TLS: TLSv1.2' $Q/tls/tls.txt; then
  ok "en el receptor de prueba, por TLS 1.2: el informe del juego de mentira, sin ninguna ruta ni el nombre del usuario"
else ko "lo que ha llegado al receptor desde QEMU: $(head -c 300 "$f" 2> /dev/null) $(tail -n 2 $Q/tls/tls.txt 2> /dev/null | tr '\n' ' ')"; fi
mira "QEMU: los componentes de Microsoft (la lista de prueba): bajado por HTTPS de lo que imita a download.microsoft.com, la suma, la licencia, el modo administrador (lo pregunta el núcleo: «S») y sus DLL en \\WIN32" \
  "^prueba  Componente de prueba (de mentira)  \[sin poner\]$" "^Bajado y comprobado (SHA-256 $e)" "^LICENCIA DE PRUEBA DE HISPA-DOS$" "MODO ADMINISTRADOR" "^Puestas en .WIN32: 2\.$"
rm -f $Q/x.dll
if mcopy -n -i $Q/c.img ::WIN32/PRUEBA1.DLL $Q/x.dll > /dev/null 2>&1 && cmp -s $Q/x.dll $W/sacadas/PRUEBA1.DLL; then ok "en el disco, \\WIN32\\PRUEBA1.DLL es la de dentro del redistribuible de mentira, octeto a octeto"
else ko "\\WIN32\\PRUEBA1.DLL no está o no es la de dentro"; fi
mira "QEMU: PAQUETES ACTUALIZA con el catálogo de prueba (firmado) pone el win32 «nuevo» y su gancho lanza COMPAT /REVISA: vuelve a comprobar y avisa" \
  "^  instalado win32 1\.99$" "compat: revisa: win32 1\.99, 2 programas comprobados" "compat: aviso: JUEGOMEN: ahora faltan 7 (antes 10; win32 1\.99)" \
  "compat: aviso: TODOBIEN: ya no falta nada (antes 1; win32 1\.99)" "^TODOBIEN: ya no falta nada; pruébelo de nuevo\.$"
if grep -q "excepción\|\*\*\*" $S; then ko "el núcleo se ha quejado: $(grep -m1 'excepción\|\*\*\*' $S)"; else ok "QEMU (la Consola): ni una excepción ni una queja del núcleo"; fi
# --- 3. en QEMU, el escritorio a 1024 x 600: las capturas ---
local SES=(":pausa 3" ":doble 60 186" ":pausa 4" ":escribe compat C:\\JUEGOMEN" ":tecla ret" ":hasta 120 1 compat: comprobado" ":pausa 3" ":captura cp1"
  ":tecla alt-v" ":hasta 90 1 cuadro «Enviar el informe" ":pausa 2" ":captura cp2" ":tecla spc" ":pausa 1" ":tecla ret" ":hasta 120 1 compat: informe" ":pausa 2" ":captura cp3" ":tecla ret" ":pausa 1"
  ":tecla alt-p" ":hasta 30 1 cuadro «Componentes" ":pausa 2" ":captura cp4" ":tecla ret" ":hasta 120 1 cuadro «La licencia" ":pausa 2" ":captura cp5" ":tecla esc" ":pausa 2"
  ":tecla esc" ":pausa 3" ":escribe C:\\JUEGOMEN\\JUEGO.EXE" ":tecla ret" ":hasta 60 1 que necesita JUEGO.EXE" ":pausa 3" ":captura cp8" ":tecla ret" ":pausa 3"
  ":escribe compat /fallo C:\\JUEGOMEN\\JUEGO.EXE" ":tecla ret" ":hasta 120 2 compat: comprobado C:" ":pausa 3" ":captura cp7" ":tecla esc" ":pausa 3"
  ":escribe paquetes actualiza" ":tecla ret" ":hasta 240 1 compat: aviso en la bandeja" ":pausa 3" ":captura cp6")
CAPTURAS=$G PANTALLA=1024x600 ESCRIBE=1 DISCO=$Q/v.img ARRANQUE="escritorio modo=1024x600" MODULOS= MEMORIA=128 \
  QEMU_MAS="-nic user,model=e1000 -device usb-ehci,id=u -drive id=mu,file=$Q/usb.img,format=raw,if=none,snapshot=on -device usb-storage,bus=u.0,drive=mu" \
  $AQUI/nucleo32/probar.sh $Q/SEMILLA.BIN "${SES[@]}" > $Q/ventana.sal 2>&1
tr -d '\r' < $Q/ventana.sal > $Q/ventana.txt
S=$Q/ventana.txt
mira "la ventana (1024 x 600): comprobar el juego de mentira, el consentimiento y el envío, los componentes y su licencia (sin aceptarla)" \
  "compat: comprobado C:.JUEGOMEN: faltan 10 (5 importantes), 4 archivos, win32 1\.2" "compat: cuadro «Enviar el informe a Hispa-DOS»" "compat: informe mandado: OK " \
  "compat: cuadro «Componentes de Microsoft»" "compat: componente prueba: bajado y comprobado (SHA-256)" "compat: cuadro «La licencia de Microsoft»"
mira "el aviso de la bandeja tras PAQUETES ACTUALIZA (el gancho, con escritorio: la ventanita abajo a la derecha)" "compat: aviso en la bandeja: JUEGOMEN: ahora faltan 7 (antes 10)"
mira "un programa que no carga (a su MOTOR.DLL le falta GLIDE2X.DLL): COMPAT /FALLO (lo que lanza el botón de WIN32, sin decir qué) lo saca de WIN32.LOG y comprueba la carpeta" \
  "compat: fallo: JUEGO.EXE: MOTOR.DLL, que necesita GLIDE2X.DLL" "compat: comprobado C:.JUEGOMEN: faltan 10 (5 importantes)"
if grep -q "excepción\|\*\*\*" $S; then ko "el núcleo se ha quejado: $(grep -m1 'excepción\|\*\*\*' $S)"; else ok "QEMU (el escritorio): ni una excepción ni una queja del núcleo"; fi
mal=0; for c in cp1 cp2 cp3 cp4 cp5 cp6 cp7 cp8; do [ -s $G/$c.ppm ] || mal="$mal $c"; done
[ "$mal" = 0 ] && ok "las capturas de la ventana (cp1 a cp8)" || ko "faltan capturas:$mal"
if [ -n "${COMPAT_CAPTURAS:-}" ]; then
  mkdir -p "$COMPAT_CAPTURAS"
  for c in cp1:ventana cp2:consentimiento cp3:mandado cp4:componentes cp5:licencia cp6:aviso cp7:fallo cp8:win32; do
    [ -f $G/${c%%:*}.ppm ] && convert $G/${c%%:*}.ppm "$COMPAT_CAPTURAS/${c#*:}.png" 2> /dev/null
  done
  [ -f $W/cap/cpl1.ppm ] && convert $W/cap/cpl1.ppm "$COMPAT_CAPTURAS/linux.png" 2> /dev/null
fi
kill $PID_PHP $PID_TLS 2> /dev/null; wait $PID_PHP $PID_TLS 2> /dev/null
}
