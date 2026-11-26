# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# pruebas_remota.sh - (REMOTA) la parte 58 de pruebas.sh (SOLO=remota), que lee
# este archivo («.»): la función parte_remota, con los ayudantes de
# pruebas.sh (ok, ko, mira, saca) y sus carpetas ($AQUI, $P).
#
# La consola remota «Dejar entrar a Claude» (SSHD /REMOTA, nucleo32/ssh/
# remota.c) con el cliente de verdad (tools/remota_pentium.py: OpenSSH con
# ControlMaster), por QEMU (hostfwd al 2323 del invitado), en el escritorio.
# El código se lee de las capturas de la ventana (ssh/pruebas/codigo_ppm.py:
# no sale por ningún otro sitio, tampoco por el puerto serie). Un solo QEMU:
#   1. sin la ventana abierta, el puerto está cerrado (no entra nadie);
#   2. se abre (Ejecutar: \HISPA\SSHD.BIN /REMOTA) y sale el código; uno malo
#      no entra; el bueno sí: VER, WIFI.LOG, llevar y traer 70 KB al azar
#      (iguales octeto a octeto), los registros; y el mismo código otra vez
#      ya no vale (uno por conexión);
#   3. el siguiente caduca a los 60 s: sale otro solo, el caducado no entra y
#      el nuevo sí; con alguien dentro, la ventana lo dice y la E (el botón
#      «Echar») lo echa;
#   4. tres códigos malos: otro código y una pausa de 30 s en la que no entra
#      nadie (tampoco con el de antes); pasada, el nuevo entra;
#   5. Esc cierra la ventana: el puerto se cierra otra vez;
#   6. \HISPA\REMOTA.LOG: quién entró, desde dónde, qué hizo, los fallos, el
#      echado y el cierre; y ningún código en él ni en el puerto serie.

parte_remota() {
echo "== Hispa-Pentium: la consola remota «Dejar entrar a Claude» (SSHD /REMOTA) con tools/remota_pentium.py =="
local X=$P/remota hechos=1 PS CLI S
local CL="python3 -I $AQUI/../tools/remota_pentium.py"
local LEE="python3 -I $AQUI/nucleo32/ssh/pruebas/codigo_ppm.py"
if ! command -v ssh > /dev/null; then echo "  (sin ssh de OpenSSH aquí: sin la consola remota)"; return; fi
rm -rf $X; mkdir -p $X/v $X/rd
export MTOOLS_SKIP_CHECK=1
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/semilla.cas -o $X/semilla.asm > /dev/null 2>&1 && nasm -f bin $X/semilla.asm -o $X/SEMILLA.BIN || hechos=0
$AQUI/nucleo32/disco.sh $X/vivo.img 32 red ssh > $X/disco.txt 2>&1 && mcopy -n -i $X/vivo.img '::*.BIN' $X/v/ > /dev/null 2>&1 || hechos=0
printf '[hispa]\r\nusuario=Ana\r\nequipo=PRUEBA\r\nteclado=es\r\n' > $X/HISPA.INI
printf 'wifi: un diagnostico de prueba\r\n' > $X/WIFI.LOG
printf 'arranque: de prueba\r\n' > $X/ARRANQUE.TXT
mkfs.fat -F 32 -s 1 -n HISPA -C $X/sis.img 65536 > /dev/null && mmd -i $X/sis.img ::HISPA ::HISPA/PAQUETES ::DOCUMENT \
  && mcopy -i $X/sis.img $X/v/ORDENES.BIN $X/v/RED.BIN $X/v/IPCONFIG.BIN $X/v/SSHD.BIN $X/v/ARGON2.BIN $X/v/CONSOLA.BIN \
       $X/v/EQUIPO.BIN $X/v/BLOC.BIN $X/HISPA.INI $X/WIFI.LOG ::HISPA/ \
  && mcopy -i $X/sis.img $X/ARRANQUE.TXT :: && mcopy -i $X/sis.img $X/SEMILLA.BIN ::HISPA/NUCLEO.BIN || hechos=0
if [ $hechos != 1 ]; then ko "no se han podido hacer los programas o el disco de la consola remota: $(tail -n 2 $X/disco.txt)"; return; fi
head -c 70000 /dev/urandom > $X/azar.bin
PS=$((30000 + RANDOM % 10000))
# ---- el cliente, a la vez que QEMU: lee el puerto serie (SERIE_VIVA) y las capturas ----
cat > $X/cliente.sh <<EOF
#!/bin/bash
X=$X; CL="$CL"; LEE="$LEE"; export REMOTA_DIR=$X/rd
cuenta() { tr -d '\r' < \$X/serie 2> /dev/null | grep -a -c -- "\$1"; }
espera() { for _ in \$(seq \$((\$1 * 2))); do [ "\$(cuenta "\$2")" -ge \$3 ] && return 0; sleep 0.5; done; return 1; }
codigo() { for _ in \$(seq 120); do [ -s \$X/\$1.ppm ] && sleep 1 && \$LEE \$X/\$1.ppm && return 0; sleep 0.5; done; echo ninguno; }
abre() { timeout 60 \$CL abrir 127.0.0.1 \$1 --puerto $PS --nombre prueba < /dev/null > /dev/null 2>&1; echo \$?; }
malo() { echo \$(( (10#\$1 + 314159) % 900000 + 100000 )); }
espera 120 "DHCP: IP" 1
echo "antes \$(abre 123456)"
c1=\$(codigo c1); echo "c1 \$c1"
echo "malo1 \$(abre \$(malo \$c1))"
echo "bueno1 \$(abre \$c1)"
timeout 60 \$CL orden ver > \$X/ver.txt 2>&1; echo "ver \$?"
timeout 60 \$CL leer '\\HISPA\\WIFI.LOG' > \$X/wifi.txt 2>&1; echo "wifi \$?"
timeout 90 \$CL escribir \$X/azar.bin '\\DOCUMENT\\AZAR.BIN' > /dev/null 2>&1; echo "escribir \$?"
timeout 90 \$CL leer '\\DOCUMENT\\AZAR.BIN' \$X/vuelta.bin > /dev/null 2>&1; echo "traer \$?"
cmp -s \$X/azar.bin \$X/vuelta.bin && echo "iguales"
timeout 120 \$CL registros \$X/reg > \$X/registros.txt 2>&1; echo "registros \$?"
timeout 30 \$CL leer '\\NO\\ESTA.TXT' > /dev/null 2> \$X/noesta.txt; echo "noesta \$?"
\$CL cerrar > /dev/null 2>&1
echo "otravez1 \$(abre \$c1)"
c2=\$(codigo c2); echo "c2 \$c2"
espera 120 "ha caducado" 1
c3=\$(codigo c3); echo "c3 \$c3"
echo "caducado \$(abre \$c2)"
echo "bueno3 \$(abre \$c3)"
espera 60 "ECHADO" 1
sleep 3
timeout 20 \$CL estado > /dev/null 2>&1; echo "estado_echado \$?"
\$CL cerrar > /dev/null 2>&1
c4=\$(codigo c4); echo "c4 \$c4"
for i in 1 2 3; do echo "fallo\$i \$(abre \$(malo \$c4))"; done
echo "enpausa \$(abre \$c4)"
c5=\$(codigo c5); echo "c5 \$c5"
echo "bueno5 \$(abre \$c5)"
timeout 60 \$CL orden ver > \$X/ver5.txt 2>&1; echo "ver5 \$?"
\$CL cerrar > /dev/null 2>&1
c6=\$(codigo c6); echo "c6 \$c6"
espera 60 "CERRADA" 1
sleep 2
echo "cerrada \$(abre \$c6)"
EOF
chmod +x $X/cliente.sh
rm -f $X/serie
$X/cliente.sh > $X/cliente.txt 2>&1 &
CLI=$!
QEMU_MAS="-netdev user,id=n0,hostfwd=tcp:127.0.0.1:$PS-:2323 -device e1000,netdev=n0" SERIE_VIVA=$X/serie CAPTURAS=$X ESCRIBE=1 \
  SATA=$X/sis.img MEMORIA=128 MODULOS= ARRANQUE=escritorio $AQUI/nucleo32/probar.sh $X/SEMILLA.BIN \
  ":hasta 120 1 DHCP: IP" ":pausa 25" \
  ":tecla meta_l-r" ":pausa 1" ":escribe \\HISPA\\SSHD.BIN /REMOTA" ":tecla ret" ":hasta 30 1 remota: ABIERTA" ":pausa 3" ":captura c1" \
  ":hasta 240 1 remota: SALE" ":pausa 3" ":captura c2" \
  ":hasta 90 1 ha caducado" ":pausa 3" ":captura c3" \
  ":hasta 60 2 remota: ENTRA" ":pausa 4" ":captura dentro" ":tecla e" ":hasta 20 1 ECHADO" \
  ":pausa 6" ":captura c4" \
  ":hasta 120 1 remota: tres c" ":pausa 2" ":captura pausa" ":hasta 60 1 pasada la pausa" ":pausa 3" ":captura c5" \
  ":hasta 120 3 remota: SALE" ":pausa 3" ":captura c6" ":pausa 2" ":tecla esc" ":hasta 20 1 CERRADA" ":pausa 25" > $X/q.sal 2>&1
wait $CLI
tr -d '\r' < $X/q.sal > $X/q.txt
S=$X/cliente.txt
mira "sin la ventana abierta, el puerto 2323 no deja entrar (ssh, 255)" "^antes 255$"
mira "Ejecutar: \\HISPA\\SSHD.BIN /REMOTA abre la ventana con el código (6 cifras, leído de la captura)" "^c1 [1-9][0-9]\{5\}$"
mira "un código malo no entra; el bueno sí (ssh con el código como contraseña, por la conexión maestra)" "^malo1 255$" "^bueno1 0$"
mira "dentro: una orden (VER), leer \\HISPA\\WIFI.LOG, llevar y traer 70 KB al azar (iguales), los registros" "^ver 0$" "^wifi 0$" "^escribir 0$" "^traer 0$" "^iguales$" "^registros 0$"
mira "un archivo que no está: error (1) y dice por qué" "^noesta 1$"
if grep -q "^Hispa-Pentium" $X/ver.txt && grep -q "^wifi: un diagnostico de prueba" $X/wifi.txt && grep -q "ARRANQUE.TXT" $X/registros.txt \
   && grep -q "HISPA.WIFI.LOG" $X/registros.txt && grep -q "no se puede leer" $X/noesta.txt; then
  ok "lo que sale: la versión, WIFI.LOG tal cual, ARRANQUE.TXT y WIFI.LOG en «registros» y «remota: no se puede leer»"
else ko "lo que ha salido dentro:"; head -3 $X/ver.txt $X/wifi.txt $X/registros.txt $X/noesta.txt 2> /dev/null | sed 's/^/        /'; fi
mira "uno por conexión: el mismo código otra vez ya no entra" "^otravez1 255$" "^c2 [1-9][0-9]\{5\}$"
mira "a los 60 s el código caduca solo: sale otro, el caducado no entra y el nuevo sí" "^c3 [1-9][0-9]\{5\}$" "^caducado 255$" "^bueno3 0$"
mira "con alguien dentro, la E de la ventana («Echar») lo echa: la conexión se acaba" "^estado_echado 1$"
mira "tres códigos malos: no entran, sale otro código y en la pausa no entra nadie; pasada, el nuevo sí (y dentro, VER)" \
  "^fallo1 255$" "^fallo2 255$" "^fallo3 255$" "^enpausa 255$" "^c5 [1-9][0-9]\{5\}$" "^bueno5 0$" "^ver5 0$"
mira "Esc cierra la ventana y con ella el puerto: ya no entra nadie" "^cerrada 255$"
local c; for c in c1 c2 c3 c4 c5 c6; do grep -q "^$c [1-9][0-9]\{5\}$" $S || ko "el código $c no se ha leído de la captura"; done
if [ "$(grep -o '^c[1-6] [0-9]*' $S | awk '{print $2}' | sort -u | wc -l)" = 6 ]; then ok "seis códigos, todos distintos"
else ko "los códigos se repiten: $(grep '^c[1-6] ' $S | tr '\n' ' ')"; fi
S=$X/q.txt
mira "en el registro (puerto serie): abierta, las entradas (desde 10.0.2.2), lo que se hace, los fallos, el caducado, el echado, la pausa y el cierre" \
  "remota: ABIERTA la consola remota (puerto 2323) por Ana" "remota: ENTRA claude desde 10.0.2.2" "remota:   hace: ver" "remota:   hace: lee .HISPA.WIFI.LOG" \
  "remota:   hace: escribe .DOCUMENT.AZAR.BIN" "remota: c.digo err.neo desde 10.0.2.2 (1 de 3" "remota: el c.digo ha caducado sin usarse" \
  "remota: ECHADO por el due.o del equipo" "remota: tres c.digos err.neos" "remota: pasada la pausa" "remota: CERRADA la consola remota"
if saca $X/sis.img HISPA/REMOTA.LOG $X/REMOTA.LOG; then
  S=$X/REMOTA.LOG
  mira "C:\\HISPA\\REMOTA.LOG con la fecha y la hora de cada cosa: quién entró, desde dónde, qué hizo, cuánto estuvo, los fallos y el cierre" \
    "^[0-3][0-9]/[01][0-9]/20[0-9][0-9] [0-2][0-9]:[0-5][0-9]:[0-5][0-9]  ABIERTA" "  ENTRA claude desde 10.0.2.2 (SSH-2.0-OpenSSH" "  SALE 10.0.2.2 (0 min" \
    "    hace: ver" "  ECHADO" "  CERRADA"
else ko "no está C:\\HISPA\\REMOTA.LOG"; fi
local fuga=0
for c in $(grep -o '^c[1-6] [0-9]*' $X/cliente.txt | awk '{print $2}'); do grep -aq "$c" $X/q.txt $X/REMOTA.LOG 2> /dev/null && fuga=1; done
if [ $fuga = 0 ]; then ok "ningún código sale por el puerto serie ni en REMOTA.LOG (solo en la ventana)"; else ko "un código sale por el puerto serie o en REMOTA.LOG"; fi
}
