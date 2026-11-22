# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# pruebas_correo.sh - la parte 42 de pruebas.sh (SOLO=correo, Correo: el
# cliente de correo, castel/nucleo32/correo), que lee este archivo («.»): la
# función parte_correo, con los ayudantes de pruebas.sh (ok, ko, mira,
# miracap, saca) y sus carpetas ($AQUI, $P, $G).
#
# NO usa ningún servidor de correo de verdad ni ninguna cuenta real: el
# servidor es el de pruebas (correo/pruebas/servidor_correo.py: IMAP, SMTP,
# POP3 y la autoconfiguración, con su raíz de prueba, en 127.0.0.1, que QEMU
# ve como 10.0.2.2), con sus buzones de mentira (haz_buzones.py).
#
# 1. En Linux: el núcleo MIME, HTML y vCard (pruebas_mime.sh, con su fuzz),
#    el servidor (prueba_servidor.py), Correo entero con ASan contra él (IMAP
#    por TLS y por STARTTLS, POP3, SMTP, lo que no se debe dejar: sin
#    STARTTLS, certificado de otro nombre) y el fuzz del IMAP (fuzz_imap.py).
# 2. En QEMU, las órdenes (CORREO /ORDEN): lo mismo, con TLS.BIN (y su
#    STARTTLS), y que la carpeta del correo no la lee nadie más.
# 3. En QEMU, la ventana a 1024 x 600 (la del HP Mini): el asistente (los
#    servidores, de la ISPDB de prueba), la lista, un adjunto con EICAR por
#    Escudo, uno de texto abierto con el Bloc, la suplantación, el enlace
#    engañoso, las imágenes (de fuera, sin cargar; la cid:, pintada),
#    escribir con la libreta y un adjunto, responder, mover, buscar, un
#    borrador; y luego, sin red y a 800 x 600 (dos paneles), lo guardado.
# 4. Lo que tarda en un Atom N455 (QEMU -cpu n270 -icount shift=0): un
#    buzón de 10.000 mensajes.
# Las capturas, en $G (y las que van a la documentación, en docs/pentium/correo).

parte_correo() {
echo "== Correo (CORREO.BIN): IMAP, POP3 y SMTP cifrados, MIME, HTML, la ventana =="
local Y=$P/correo C=$AQUI/nucleo32/correo S SPID BIN AN
mkdir -p $Y $G
rm -rf $Y/srv $Y/d1 $Y/d2 $Y/d3 $Y/d4 $Y/d5 $Y/d6
mkdir -p $Y/srv $Y/d1 $Y/d2 $Y/d3 $Y/d4 $Y/d5 $Y/d6
# --- 1. en Linux ---
bash $C/pruebas/pruebas_mime.sh > $Y/mime.txt 2>&1
while IFS= read -r l; do case "$l" in "  OK   "*) ok "MIME: ${l#  OK   }" ;; "  FAIL "*) ko "MIME: ${l#  FAIL }" ;; esac; done < $Y/mime.txt
python3 -I $C/pruebas/fuzz.py $AQUI/../build/correo-pruebas/prueba_mime $C/pruebas/casos ${FUZZ_CORREO:-1500} 7 > $Y/fuzz.txt 2>&1
while IFS= read -r l; do case "$l" in "  OK   "*) ok "fuzz: ${l#  OK   }" ;; "  FAIL "*) ko "fuzz: ${l#  FAIL }" ;; esac; done < $Y/fuzz.txt
python3 -I $C/pruebas/prueba_servidor.py > $Y/servidor.txt 2>&1
if [ $? = 0 ]; then ok "el servidor de pruebas (IMAP, SMTP, POP3, autoconfiguración) contesta bien a imaplib, smtplib y poplib ($(grep -c '^  OK' $Y/servidor.txt) comprobaciones)"
else ko "el servidor de pruebas: $(grep '^  FAIL' $Y/servidor.txt | head -3 | tr '\n' ' ')"; fi
AN=$Y/correo-anfitrion
if ! bash $C/anfitrion.sh $AN asan > $Y/anf.txt 2>&1; then ko "Correo no se compila en Linux: $(tail -n 3 $Y/anf.txt | tr '\n' ' ')"; return; fi
BIN=$Y/CORREO.BIN
if bash $C/hacer.sh $BIN > $Y/hacer.txt 2>&1; then ok "CORREO.BIN se compila (-march=i586, sin CMOV: $(stat -c %s $BIN) octetos) y Correo en Linux, con ASan y UBSan"
else ko "CORREO.BIN no se compila: $(tail -n 3 $Y/hacer.txt | tr '\n' ' ')"; return; fi
python3 -I $C/pruebas/servidor_correo.py $Y/srv > $Y/srv.txt 2>&1 &
SPID=$!
for _ in $(seq 150); do [ -s $Y/srv/puertos ] && break; sleep 0.2; done
pu() { grep "^$1=" $Y/srv/puertos | cut -d= -f2; }
if [ ! -s $Y/srv/puertos ]; then ko "el servidor de pruebas no arranca: $(tail -n 2 $Y/srv.txt)"; kill $SPID 2> /dev/null; return; fi
c() { local d=$1; shift; CORREO_CA=$Y/srv/raiz.pem CORREO_DATOS=$Y/$d CORREO_SOLO_TLS12=1 ASAN_OPTIONS=detect_leaks=0 $AN "$@" 2>&1; }
L=127.0.0.1
{
  c d1 /ORDEN cuenta ana@prueba.test secreta1 $L $(pu imaps) tls $L $(pu smtps) tls
  c d1 /ORDEN carpetas; c d1 /ORDEN al-dia; c d1 /ORDEN lee 24; c d1 /ORDEN lee 18; c d1 /ORDEN lee 17; c d1 /ORDEN lee 16; c d1 /ORDEN lee 4
  printf 'Hola desde Linux\n' > $Y/adj.txt
  c d1 /ORDEN envia "Luis Ruiz <luis@prueba.test>" "Prueba con ñ y €" "Texto con tildes: año." $Y/adj.txt
  c d1 /ORDEN busca ñandú; c d1 /ORDEN mueve 20 Archive; c d1 /ORDEN carpetas
  c d2 /ORDEN cuenta ana@prueba.test secreta1 $L $(pu imap) starttls $L $(pu submission) starttls; c d2 /ORDEN al-dia
  c d2 /ORDEN envia luis@prueba.test "Por el 587" "con STARTTLS"
  c d3 /ORDEN cuenta ana@prueba.test secreta1 $L $(pu pop3s) tls $L $(pu smtps) tls /POP3; c d3 /ORDEN al-dia; c d3 /ORDEN al-dia
  c d6 /ORDEN cuenta ana@prueba.test secreta1 $L $(pu pop3) starttls $L $(pu smtps) tls /POP3; c d6 /ORDEN al-dia
  c d4 /ORDEN cuenta ana@prueba.test secreta1 $L $(pu imap_sin_starttls) starttls $L $(pu smtps) tls; c d4 /ORDEN al-dia
  c d5 /ORDEN cuenta ana@prueba.test secreta1 $L $(pu imaps_otro) tls $L $(pu smtps) tls; c d5 /ORDEN al-dia
  c d5 /ORDEN cuenta ana@prueba.test secreta1 $L $(pu imap_sin_starttls) sin-cifrar $L $(pu smtps) tls; c d5 /ORDEN al-dia 2
} > $Y/linux.txt
S=$Y/linux.txt
mira "Linux: la cuenta por TLS (993), sus carpetas con SPECIAL-USE y el UTF-7 de «Asociación/Cuotas»" "dentro como ana@prueba.test (IDLE sí, UIDPLUS sí, MOVE sí, SPECIAL-USE sí)" \
  "^  Asociación/Cuotas (2, 1 sin leer)$" "^Sent (2, 0 sin leer) \[enviados\]$" "^Trash (2, 0 sin leer) \[papelera\]$" "^Bandeja de entrada: 49 mensajes"
mira "Linux: los mensajes por dentro: RFC 2231, la suplantación, el enlace engañoso, el HTML sin scripts, windows-1252 con €" \
  "^Adjunto 1: Presupuesto año 2026.pdf (application/pdf" "^AVISO (grave): El nombre dice «soporte@bancosantander.es» pero el correo viene de «alerta@malo.ejemplo»" \
  "^AVISO: enlace engañoso: El enlace dice «www.banco.es» pero lleva a «malo.ejemplo»" "^-- (text/html, sin scripts) --$" "^Precio: 15 € — dijo “vale”"
mira "Linux: enviar (SMTP 465 con AUTH, un adjunto, y guardado en Enviados); buscar en el servidor «ñandú» (CHARSET UTF-8); mover a Archive" \
  "^Enviado.$" "^3 encontrados$" "^Movido a Archive.$" "^Sent (3, 0 sin leer) \[enviados\]$"
mira "Linux: STARTTLS en el 143 (IMAP) y en el 587 (SMTP): lo plano lo hace la capa del sistema, y se entra ya cifrado" \
  "imap: conectado con 127.0.0.1:$(pu imap) (TLS" "smtp: conectado con 127.0.0.1:$(pu submission) (TLS"
mira "Linux: POP3 por TLS (995) y por STLS (110): baja los 49 una vez (la segunda, nada nuevo)" "pop3: 49 nuevos de 49" "pop3: 0 nuevos de 49" \
  "pop3: conectado con 127.0.0.1:$(pu pop3) (TLS"
mira "Linux: lo que no se deja: un servidor sin STARTTLS, un certificado de otro nombre, y sin cifrar a uno que no es de casa sin «permitir»" \
  "Sin conexión: el servidor de correo no ofrece STARTTLS: la conexión iría sin cifrar" "Sin conexión: el certificado es de otro sitio" \
  "No se manda la contraseña sin cifrar a 127.0.0.1\|LOGIN SIN CIFRAR\|Sin conexión"
if grep -q 'LOGIN SIN CIFRAR' $Y/srv/registro.txt; then ko "¡una contraseña ha ido sin cifrar!: $(grep 'LOGIN SIN CIFRAR' $Y/srv/registro.txt | head -2)"
else ok "en el registro del servidor: ninguna contraseña ha llegado sin cifrar"; fi
if grep -q "Sanitizer\|runtime error" $S; then ko "ASan o UBSan se han quejado: $(grep -m1 'Sanitizer\|runtime error' $S)"; else ok "Linux: ni una queja de ASan ni de UBSan"; fi
e=$(ls $Y/srv/enviados/*.eml 2> /dev/null | head -1)
if [ -n "$e" ] && python3 -I - "$e" <<'PY'
import email, sys
from email import policy
m = email.message_from_bytes(open(sys.argv[1], 'rb').read(), policy=policy.default)
assert m['Subject'] == 'Prueba con ñ y €', m['Subject']
assert 'luis@prueba.test' in str(m['To'])
a = [p for p in m.iter_attachments()]
assert a and a[0].get_filename() == 'adj.txt' and a[0].get_content() in ('Hola desde Linux\n', b'Hola desde Linux\n')
assert 'año' in m.get_body(('plain',)).get_content()
PY
then ok "lo enviado lo lee bien el módulo email de Python (el asunto con ñ y €, el texto, el adjunto)"; else ko "lo enviado no se lee bien con Python ($e)"; fi
python3 -I $C/pruebas/fuzz_imap.py $AN ${FUZZ_IMAP:-1000} 3 > $Y/fuzzimap.txt 2>&1
while IFS= read -r l; do case "$l" in "  OK   "*) ok "${l#  OK   }" ;; "  FAIL "*) ko "${l#  FAIL }" ;; esac; done < $Y/fuzzimap.txt
cat > $Y/autocfg.xml <<'XML'
<?xml version="1.0"?>
<clientConfig version="1.1"><emailProvider id="ejemplo.es">
<incomingServer type="pop3"><hostname>pop.ejemplo.es</hostname><port>995</port><socketType>SSL</socketType><username>%EMAILLOCALPART%</username></incomingServer>
<incomingServer type="imap"><hostname>imap.ejemplo.es</hostname><port>143</port><socketType>plain</socketType><username>%EMAILADDRESS%</username></incomingServer>
<incomingServer type="imap"><hostname>imap.ejemplo.es</hostname><port>993</port><socketType>SSL</socketType><username>%EMAILADDRESS%</username></incomingServer>
<outgoingServer type="smtp"><hostname>smtp.ejemplo.es</hostname><port>587</port><socketType>STARTTLS</socketType></outgoingServer>
</emailProvider></clientConfig>
XML
S=$Y/acfg.txt; c d1 /ORDEN autocfg-xml $Y/autocfg.xml ana@ejemplo.es > $S
mira "la autoconfiguración (clientConfig 1.1): el IMAP con TLS antes que el plano, %EMAILADDRESS%, el SMTP con STARTTLS" \
  "^entrada imap imap.ejemplo.es:993 (1) usuario ana@ejemplo.es; salida smtp.ejemplo.es:587 (3)$"
# --- 2. en QEMU: las órdenes ---
export MTOOLS_SKIP_CHECK=1
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/semilla.cas -o $Y/semilla.asm > /dev/null && nasm -f bin $Y/semilla.asm -o $Y/SEMILLA.BIN || { ko "semilla.cas no compila"; kill $SPID; return; }
if ! $AQUI/nucleo32/disco.sh $Y/base.img 32 red tls escudo > $Y/disco.txt 2>&1; then ko "disco.sh: $(tail -n 2 $Y/disco.txt)"; kill $SPID; return; fi
cp $Y/base.img $Y/q.img
mmd -i $Y/q.img ::RAICES && mcopy -i $Y/q.img $Y/srv/RAIZ.CER ::RAICES/PRUEBA.CER && mcopy -i $Y/q.img $BIN ::
Q=10.0.2.2
ESCRIBE=1 DISCO=$Y/q.img ARRANQUE=ordenes MODULOS= MEMORIA=128 QEMU_MAS="-nic user,model=e1000" $AQUI/nucleo32/probar.sh $Y/SEMILLA.BIN ":pausa 2" \
  "correo /orden cuenta ana@prueba.test secreta1 $Q $(pu imaps) tls $Q $(pu smtps) tls" ":pausa 3" "correo /orden carpetas" ":pausa 12" \
  "correo /orden al-dia" ":pausa 10" "correo /orden lee 24" ":pausa 6" "correo /orden envia luis@prueba.test \"Desde QEMU\" \"Hola desde Hispa-Pentium\" leeme.txt" ":pausa 10" \
  "correo /orden cuenta ana@prueba.test secreta1 $Q $(pu imap) starttls $Q $(pu submission) starttls" ":pausa 3" "correo /orden al-dia 2" ":pausa 12" \
  "correo /orden cuenta ana@prueba.test secreta1 $Q $(pu pop3s) tls $Q $(pu smtps) tls /POP3" ":pausa 3" "correo /orden al-dia 3" ":pausa 25" \
  "correo /orden cuenta ana@prueba.test secreta1 $Q $(pu imap_sin_starttls) starttls $Q $(pu smtps) tls" ":pausa 3" "correo /orden al-dia 4" ":pausa 8" \
  "correo /orden cuenta ana@prueba.test secreta1 $Q $(pu imaps_extrano) tls $Q $(pu smtps) tls" ":pausa 3" "correo /orden al-dia 5" ":pausa 8" \
  "dir correo" ":pausa 1" "type correo\\cuentas.ini" ":pausa 1" > $Y/ordenes.sal 2>&1
tr -d '\r' < $Y/ordenes.sal > $Y/ordenes.txt
S=$Y/ordenes.txt
mira "QEMU: la cuenta por TLS (TLS.BIN, con la raíz de prueba de \\RAICES), las carpetas y lo nuevo" "correo: imap: conectado con 10.0.2.2:$(pu imaps) (TLS 1.2, ECDHE-ECDSA" \
  "^  Asociación/Cuotas (2, 1 sin leer)$" "^Bandeja de entrada: 49 mensajes"
mira "QEMU: leer uno (bajado y guardado) y enviar con un adjunto del disco" "^Adjunto 1: Presupuesto año 2026.pdf (application/pdf" "^Enviado.$"
mira "QEMU: STARTTLS en el 143 y en el 587, lo hace TLS.BIN (la orden nueva, «(CORREO)» en tls.c)" "tls: 10.0.2.2: STARTTLS (IMAP): sí" \
  "correo: imap: conectado con 10.0.2.2:$(pu imap) (TLS 1.2"
mira "QEMU: POP3 por TLS (995): los 49, a la bandeja de aquí" "correo: pop3: 49 nuevos de 49"
mira "QEMU: sin STARTTLS no se sigue (ni se manda la clave); una raíz desconocida, tampoco" \
  "Sin conexión: el servidor de correo no ofrece STARTTLS: la conexión iría sin cifrar" "Sin conexión: el certificado de 10.0.2.2 no lo firma ninguna autoridad conocida"
mira "QEMU: la carpeta del correo es un secreto de CORREO.BIN: ni DIR ni TYPE la leen (la zona protegida, permisos.cas «(CORREO)»)" \
  "acceso denegado a \\\\correo para leer (ORDENES.BIN)" "acceso denegado a \\\\correo\\\\cuentas.ini para leer (ORDENES.BIN)"
f=$(grep -l "Desde QEMU" $Y/srv/enviados/*.eml 2> /dev/null | head -1)
if [ -n "$f" ] && grep -q 'filename="leeme.txt"\|filename="LEEME.TXT"' $f; then ok "QEMU: lo enviado ha llegado al servidor, con su adjunto (LEEME.TXT)"; else ko "QEMU: lo enviado no ha llegado bien ($f)"; fi
if grep -q 'LOGIN SIN CIFRAR' $Y/srv/registro.txt; then ko "¡una contraseña ha ido sin cifrar!"; else ok "QEMU: ninguna contraseña ha llegado sin cifrar al servidor"; fi
# --- 3. en QEMU: la ventana, a 1024 x 600 ---
cp $Y/base.img $Y/v.img
mmd -i $Y/v.img ::RAICES ::CORREO && mcopy -i $Y/v.img $Y/srv/RAIZ.CER ::RAICES/PRUEBA.CER && mcopy -i $Y/v.img $BIN ::
printf 'ispdb=https://10.0.2.2:%s/v1.1/\r\n' $(pu https) > $Y/CORREO.INI
printf 'BEGIN:VCARD\r\nVERSION:3.0\r\nFN:Luis Ruiz\r\nEMAIL:luis@prueba.test\r\nEND:VCARD\r\nBEGIN:VCARD\r\nVERSION:3.0\r\nFN:Marta Gil\r\nEMAIL:marta@ejemplo.es\r\nEND:VCARD\r\n' > $Y/LIBRETA.VCF
mcopy -i $Y/v.img $Y/CORREO.INI $Y/LIBRETA.VCF ::CORREO/
busca() { echo ":tecla ctrl-f" ":tecla ctrl-a" ":escribe $1" ":tecla ret" ":pausa 6" ":clic 400 147" ":pausa 6"; }
# shellcheck disable=SC2207
SES=(":pausa 3" ":doble 60 186" ":pausa 4" ":escribe correo" ":tecla ret" ":pausa 8" ":captura co1"
 ":escribe ana@prueba.test" ":tecla tab" ":escribe Ana García" ":tecla tab" ":escribe secreta1" ":tecla ret" ":pausa 12" ":captura co2" ":tecla ret" ":pausa 25" ":captura co3"
 $(busca "Caso 23") ":captura co4" ":clic 640 164" ":pausa 2" ":clic 700 181" ":pausa 15" ":captura co5" ":tecla ret" ":pausa 4" ":captura co6" ":tecla ret" ":pausa 2"
 $(busca "Caso 20") ":clic 640 164" ":pausa 2" ":clic 700 181" ":pausa 15" ":captura co7" ":tecla alt-f4" ":pausa 3"
 $(busca "Caso 18") ":captura co8" $(busca "Caso 17") ":captura co9" ":clic 800 187" ":pausa 2" ":captura co10" ":tecla esc" ":pausa 1"
 $(busca "Caso 14") ":captura co11" $(busca "Caso 15") ":captura co12"
 ":tecla ctrl-n" ":pausa 2" ":escribe lu" ":pausa 1" ":captura co13" ":tecla ret" ":tecla tab" ":tecla tab" ":tecla tab" ":escribe Prueba desde Hispa-Pentium"
 ":tecla tab" ":escribe Hola Luis, te mando el LEEME." ":clic 294 47" ":pausa 3" ":escribe LEEME" ":pausa 1" ":tecla ret" ":pausa 2" ":captura co14" ":tecla ctrl-e" ":pausa 10" ":captura co15"
 $(busca "Caso 02") ":tecla ctrl-r" ":pausa 3" ":escribe Gracias, Marta." ":captura co16" ":tecla ctrl-e" ":pausa 10"
 $(busca "Caso 01") ":tecla ctrl-k" ":pausa 2" ":captura co17" ":tecla ret" ":pausa 6" ":captura co18"
 ":tecla ctrl-n" ":pausa 2" ":escribe marta@ejemplo.es" ":tecla tab" ":tecla tab" ":tecla tab" ":escribe Borrador de prueba" ":tecla tab" ":escribe Sin acabar"
 ":tecla ctrl-s" ":pausa 6" ":tecla esc" ":pausa 2" ":tecla ctrl-f" ":tecla ctrl-a" ":escribe ñandú" ":tecla ret" ":pausa 6" ":captura co19")
CAPTURAS=$G PANTALLA=1024x600 ESCRIBE=1 DISCO=$Y/v.img ARRANQUE="escritorio modo=1024x600" MODULOS= MEMORIA=128 QEMU_MAS="-nic user,model=e1000" \
  $AQUI/nucleo32/probar.sh $Y/SEMILLA.BIN "${SES[@]}" > $Y/ventana.sal 2>&1
tr -d '\r' < $Y/ventana.sal > $Y/ventana.txt
S=$Y/ventana.txt
mira "la ventana: sin cuentas, el asistente; los servidores, de la base de datos (la ISPDB de prueba); prueba la entrada y guarda" \
  "correo: autoconfig: ana@prueba.test: 10.0.2.2:$(pu imaps) y 10.0.2.2:$(pu smtps), de la base de datos de Mozilla (ISPDB)" \
  "correo: asistente: cuenta guardada: ana@prueba.test (IMAP 10.0.2.2:$(pu imaps), SMTP 10.0.2.2:$(pu smtps))" "correo: ventana: 1016 x 474 (tres paneles)"
miracap "el asistente (1 de 2): la dirección, el nombre, la contraseña con asteriscos, recordarla, y lo de las contraseñas de aplicación" co1 \
  negrita 200 90 820 120 "Cuenta de correo nueva (1 de 2)"
miracap "el asistente (2 de 2): lo encontrado, los servidores, sus puertos y su cifrado (con candado), y «Probar y guardar»" co2 \
  texto 210 90 820 120 "Encontrado en la base de datos de Mozilla (ISPDB)." negrita 560 480 720 520 "Probar y guardar"
miracap "tres paneles: las carpetas (los sin leer en negrita y con su número), la lista (columnas, ▲/▼, dibujos: sobre, estrella, clip) y el mensaje" co3 \
  negrita 0 70 190 90 "ana@prueba.test" negrita 230 95 600 118 "Asunto" texto 0 110 190 250 "Sent"
mira "un adjunto con EICAR («factura.com»): a Escudo antes de abrirlo; lo encuentra, va a cuarentena y no se abre" \
  "correo: adjunto C:.CORREO.TMP.FACTURA.COM: Escudo dice 1" "escudo: aviso: C:.CORREO.TMP.FACTURA.COM"
miracap "Escudo lo ha encontrado: «Adjunto peligroso», con [!] (no solo en rojo)" co6 negrita 200 150 820 400 "[!]"
mira "un adjunto de texto: Escudo no dice nada y se abre con lo suyo (Mi equipo: el Bloc de notas)" "correo: adjunto C:.CORREO.TMP.NOTA.TXT: Escudo dice 0" "correo: adjunto abierto: C:.CORREO.TMP.NOTA.TXT"
miracap "la suplantación: el recuadro con «[!] Cuidado:» y el porqué" co8 negrita 590 140 1020 200 "[!] Cuidado:"
miracap "el enlace engañoso: el aviso arriba («[!] Hay enlaces...»)" co9 negrita 590 130 1020 200 "[!] Hay enlaces"
mira "el HTML: sin scripts, las imágenes de fuera sin cargar (el píxel de rastreo), los enlaces engañosos" "leído: 16\|leído: .*sin scripts" \
  "leído: [0-9]* «Caso 14: solo HTML con imagen remota y píxel de rastreo» (HTML, 0 adjuntos, imágenes de fuera sin cargar)" "aviso: enlace engañoso: El enlace dice «www.banco.es» pero lleva a «malo.ejemplo»"
miracap "las imágenes de fuera, sin cargar, con «Mostrar imágenes»" co11 negrita 820 140 1020 200 "Mostrar imágenes"
mira "escribir: la libreta completa «lu» (Luis Ruiz), el adjunto (el cuadro de archivos, buscando al teclear), enviar y a Enviados" \
  "correo: adjuntado: C:.LEEME.TXT" "correo: enviado: «Prueba desde Hispa-Pentium» a luis@prueba.test (1 adjuntos)" "correo: guardado en Sent"
miracap "el autocompletado de la libreta, debajo de «Para»" co13 negrita 90 120 900 160 "Luis Ruiz <luis@prueba.test>"
mira "responder (Re:, con la cita y el In-Reply-To), mover a Archive y guardar un borrador en Drafts" "correo: redactar: responder" \
  "correo: enviado: «Re: Caso 02: UTF-8 quoted-printable (año, ñandú, €)»" "correo: movidos 1 a Archive" "correo: borrador guardado en Drafts"
miracap "buscar «ñandú» en el servidor: «Lo encontrado de «ñandú»: 3»" co19 negrita 186 115 590 140 "Lo encontrado de «ñandú»: 3"
f=$(grep -l "Prueba desde Hispa-Pentium" $Y/srv/enviados/*.eml 2> /dev/null | head -1)
if [ -n "$f" ] && grep -q "luis@prueba.test" $f && grep -q 'filename="LEEME.TXT"' $f; then ok "lo escrito en la ventana ha llegado: a luis@prueba.test, con LEEME.TXT"; else ko "lo de la ventana no ha llegado bien ($f)"; fi
f=$(grep -l "In-Reply-To: <caso02@ejemplo.es>" $Y/srv/enviados/*.eml 2> /dev/null | head -1)
if [ -n "$f" ] && grep -q "^> " $f; then ok "la respuesta lleva In-Reply-To y la cita con «> »"; else ko "la respuesta no lleva In-Reply-To o la cita"; fi
if grep -q "UID MOVE .* Archive\|UID MOVE .* \"Archive\"" $Y/srv/registro.txt; then ok "en el servidor: UID MOVE a Archive"; else ko "el servidor no ha visto el MOVE"; fi
if grep -q "APPEND \"Drafts\" (\\\\Seen \\\\Draft)" $Y/srv/registro.txt; then ok "en el servidor: el borrador, APPEND a Drafts con \\Draft"; else ko "el servidor no ha visto el borrador"; fi
if grep -q "excepción\|\*\*\*" $S; then ko "el núcleo se ha quejado: $(grep -m1 'excepción\|\*\*\*' $S)"; else ok "la ventana: ni una excepción ni una queja del núcleo"; fi
# luego, sin red y a 800 x 600: dos paneles, lo guardado
SES=(":pausa 3" ":doble 60 186" ":pausa 4" ":escribe correo" ":tecla ret" ":pausa 12" ":captura co20" ":tecla ctrl-f" ":tecla ctrl-a" ":escribe Caso 02" ":tecla ret" ":pausa 6"
 ":clic 400 147" ":pausa 6" ":captura co21")
CAPTURAS=$G ESCRIBE=1 DISCO=$Y/v.img ARRANQUE=escritorio MODULOS= MEMORIA=128 $AQUI/nucleo32/probar.sh $Y/SEMILLA.BIN "${SES[@]}" > $Y/sinred.sal 2>&1
tr -d '\r' < $Y/sinred.sal > $Y/sinred.txt
S=$Y/sinred.txt
mira "sin red: se ve lo guardado (la lista y lo bajado), en dos paneles (800 x 600)" "correo: ventana: [0-9]* x [0-9]* (dos paneles)" \
  "correo: buscar en lo bajado «Caso 02»: 1" "correo: leído: 2 «Caso 02"
miracap "sin red: «Sin conexión: se ve lo guardado», y el mensaje con «< Volver a la lista»" co21 negrita 180 60 400 100 "< Volver a la lista"
# --- 4. lo que tarda en un Atom N455 (-cpu n270 -icount shift=0: una instrucción, un nanosegundo) ---
cp $Y/base.img $Y/r.img
mmd -i $Y/r.img ::RAICES && mcopy -i $Y/r.img $Y/srv/RAIZ.CER ::RAICES/PRUEBA.CER && mcopy -i $Y/r.img $BIN ::
ESCRIBE=1 DISCO=$Y/r.img ARRANQUE=ordenes MODULOS= MEMORIA=128 QEMU_MAS="-nic user,model=e1000 -cpu n270 -icount shift=0" $AQUI/nucleo32/probar.sh $Y/SEMILLA.BIN ":pausa 2" \
  "correo /orden cuenta grande@prueba.test grande1 $Q $(pu imaps) tls $Q $(pu smtps) tls" ":pausa 5" ":hasta 600 1 correo: carpetas:" \
  "correo /orden al-dia" ":hasta 900 1 Bandeja de entrada: 10000 mensajes" "correo /orden al-dia" ":hasta 300 2 Bandeja de entrada: 10000 mensajes" \
  "correo /orden lista" ":pausa 60" > $Y/atom.sal 2>&1
tr -d '\r' < $Y/atom.sal > $Y/atom.txt
S=$Y/atom.txt
mira "un buzón de 10.000 mensajes, en un Atom N455 (QEMU -cpu n270 -icount): las fichas en tramos, y abrirlo otra vez desde lo guardado" \
  "correo: al día: Bandeja de entrada: 10000 mensajes (3000 nuevos, 10000 fichas traídas) en [0-9]* ms" "correo: al día: Bandeja de entrada: 10000 mensajes (0 nuevos, 0 fichas traídas) en [0-9]* ms"
grep "correo: al día: Bandeja de entrada: 10000" $S | sed 's/^/        /'
kill $SPID 2> /dev/null
wait $SPID 2> /dev/null
}
