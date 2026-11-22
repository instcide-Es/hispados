# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# pruebas_cortes.sh - la parte 37 (SOLO=cortes) de pruebas.sh, que lee este
# archivo («.»): (CORTES) los cortes de la red, con los ayudantes de
# pruebas.sh (ok, ko, mira, miracap, saca, sano) y sus carpetas ($AQUI, $P, $G).
#
# Lo que vio Carlos en el HP Mini 5103 con el Wi-Fi (la 0.8.6 y la 0.8.7):
# si se corta a mitad de bajar una actualización, la descarga se queda
# esperando para siempre (aunque el Wi-Fi vuelva), «Cancelar» no la para y,
# al reiniciar desde el menú Inicio, el sistema espera a que se cierre
# «Programas y actualizaciones» (colgada) y no reinicia. Aquí, en QEMU:
#
#  1. DESCARGAR contra nucleo32/servidor_cortes.py (que admite Range, como
#     el nginx de actualiza.hispados.es): el servidor que cierra a mitad, el
#     que corta con un RST, el que no deja seguir (otra vez desde el
#     principio), el que se calla sin cerrar (30 s sin octetos: cortada) y
#     el enlace que se cae en plena descarga (el monitor de QEMU: set_link):
#     espera a la red y sigue desde donde iba; lo bajado, igual octeto a
#     octeto.
#  2. TCP: una conexión viva sin datos sigue (los «keepalive» tienen
#     respuesta); con el camino muerto sin que se caiga el enlace (un
#     filter-buffer de QEMU que se queda las tramas), cortada a los 35 s.
#  3. PAQUETES INSTALA un .HPQ de 3 MB de Internet con el enlace caído a
#     mitad: sigue, comprueba el SHA-256 del catálogo e instala.
#  4. La ventana «Programas y actualizaciones»: «Cancelar» (Esc) con la
#     descarga parada (el servidor callado) corta en menos de 2 s, la
#     ventana responde y no queda nada a medias; y «Reiniciar» del menú
#     Inicio en plena descarga parada: la ventana cancela, se cierra y el
#     equipo reinicia.
#  5. Un programa colgado (no lee lo que le llega) al reiniciar: «Estos
#     programas no se han cerrado», con «Forzar el cierre» y «Cancelar»;
#     Cancelar no reinicia; si no se elige nada, a los 15 s se fuerza; y
#     el botón «Forzar el cierre» acaba con él y reinicia.
#  6. El Wi-Fi (la radio simulada): el punto de acceso desaparece y vuelve
#     con otra IP (REDWIFI /SIMCORTE): RED.BIN lo sabe (sin enlace, DHCP otra
#     vez, la IP nueva) y lo nuevo (PING) va; y la IP que cambia sin que
#     se vaya el enlace (IPCONFIG /RENOVAR): «la red ha cambiado».

parte_cortes() {
echo "== (CORTES) Los cortes de la red: DESCARGAR sigue donde iba, TCP no se queda colgado, Cancelar al momento, Forzar el cierre al apagar, el Wi-Fi que vuelve =="
local K=$P/cortes HPQ SPID U hechos t0 t1 r ms
mkdir -p $K/srv $K/raiz $K/f $G
export MTOOLS_SKIP_CHECK=1
HPQ="python3 -I $AQUI/../tools/hpq.py"
hechos=1
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/semilla.cas -o $K/semilla.asm > /dev/null 2>&1 && nasm -f bin $K/semilla.asm -o $K/SEMILLA.BIN || { ko "semilla.cas no compila"; return; }
$AQUI/nucleo32/disco.sh $K/d.img 16 red > $K/disco.txt 2>&1 || { ko "disco.sh no hace el disco: $(tail -n 2 $K/disco.txt)"; return; }
# los programas de las pruebas: uno que espera mucho en una conexión TCP
# (TCPESP IP PUERTO CAMINO) y uno con ventana que no lee lo que le llega
cat > $K/utcpesp.cas <<'CAS'
módulo UTcpesp
    modelo usuario
    importar Llamadas, Redes, Textos32, Formato
    variable b: arreglo [8192] de byte
    variable a: arreglo [300] de byte
    variable w: arreglo [200] de byte
    función principal(): byte
        variable ip, puerto, s, r, total, t0: nat32
        argumentos(a, 300)
        si no palabra(a, 0, w, 200) o no texto_a_ip(w, ip) entonces devolver 2 fin
        si no palabra(a, 1, w, 200) o no texto_a_natural(w, puerto) entonces devolver 2 fin
        si no palabra(a, 2, w, 200) entonces devolver 2 fin
        s := conectar(ip, puerto, ESPERA_NORMAL)
        si es_error_red(s) entonces
            imprimir1("tcpespera: %s\n", nat32(texto_error_red(s)))
            devolver 1
        fin
        enviar_texto(s, "GET ")
        enviar_texto(s, w)
        enviar_texto(s, " HTTP/1.1\r\nHost: prueba\r\nConnection: close\r\n\r\n")
        total := 0
        t0 := tics()
        mientras cierto hacer
            r := recibir(s, b, 8192, 6000)
            si r = 0 entonces
                escribir("tcpespera: el servidor ha cerrado\n")
                romper
            fin
            si r = TIEMPO entonces
                imprimir1("tcpespera: nada en 60 s, y la conexión sigue (%u octetos)\n", total)
            sino si es_error_red(r) entonces
                imprimir2("tcpespera: %s (%u octetos)\n", nat32(texto_error_red(r)), total)
                romper
            sino
                total := total + r
            fin
        fin
        cerrar_conexión(s)
        devolver 0
    fin
fin UTcpesp.
CAS
cat > $K/ucuelga.cas <<'CAS'
módulo UCuelga
    modelo usuario
    importar Llamadas
    función principal(): byte
        variable v: nat32
        v := ventana("Programa colgado", 320, 120)
        texto(v, 12, 12, "No lee lo que le llega.", 0)
        escribir("cuelga: en marcha (no lee sus sucesos)\n")
        mientras cierto hacer dormir(100) fin
        devolver 0
    fin
fin UCuelga.
CAS
for p in utcpesp:TCPESP ucuelga:CUELGA; do
  python3 -I $AQUI/ccastel.py $K/${p%%:*}.cas -o $K/${p%%:*}.asm -I $AQUI/nucleo32 > $K/${p%%:*}.txt 2>&1 && nasm -f bin $K/${p%%:*}.asm -o $K/f/${p##*:}.BIN || { hechos=0; tail -2 $K/${p%%:*}.txt; }
done
mcopy -i $K/d.img $K/f/TCPESP.BIN :: || hechos=0
# el servidor que corta (con la raíz de los paquetes, para las partes 3 y 4)
arranca_servidor() {
  [ -n "${SPID:-}" ] && { kill $SPID 2> /dev/null; wait $SPID 2> /dev/null; }
  rm -f $K/srv/puerto $K/srv/pedidos $K/srv/servidos
  python3 -I $AQUI/nucleo32/servidor_cortes.py $K/srv $K/raiz > $K/srv.txt 2>&1 &
  SPID=$!
  for _ in $(seq 100); do [ -s $K/srv/puerto ] && break; sleep 0.1; done
  U=http://10.0.2.2:$(cat $K/srv/puerto 2> /dev/null)
}
arranca_servidor
NIC="-netdev user,id=n0 -device e1000,netdev=n0,id=nic0"

# 1. DESCARGAR, con cada clase de corte
cp $K/d.img $K/d1.img
ESCRIBE=1 DISCO=$K/d1.img ARRANQUE=ordenes MODULOS= QEMU_MAS="$NIC" $AQUI/nucleo32/probar.sh $K/SEMILLA.BIN ":pausa 2" \
  "descargar $U/cierra/grande.bin cierra.bin" ":pausa 10" "descargar $U/rst/grande.bin rst.bin" ":pausa 10" \
  "descargar $U/norango/grande.bin norango.bin" ":pausa 12" "descargar $U/calla/grande.bin calla.bin" ":pausa 45" \
  "descargar $U/lento/grande.bin lento.bin" ":pausa 7" ":monitor set_link nic0 off" ":pausa 15" ":monitor set_link nic0 on" ":pausa 35" \
  "ipconfig /todo" ":pausa 2" > $K/d1.sal 2>&1
tr -d '\r' < $K/d1.sal > $K/d1.txt
S=$K/d1.txt
mira "el servidor cierra a mitad (menos de lo que dijo): DESCARGAR pide el resto (Range) y acaba" \
  "^DESCARGAR: la conexión se ha cerrado antes de acabar (a medias: [0-9]* octetos)\.$" "^Hecho: cierra\.bin, 3000000 octetos"
mira "corta con un RST: lo mismo, desde el octeto en que iba" "^Hecho: rst\.bin, 3000000 octetos"
mira "el servidor no deja seguir (contesta 200 con todo): se empieza de nuevo y acaba" \
  "^DESCARGAR: el servidor no deja seguir desde donde iba (o el archivo ha cambiado): se empieza de nuevo\.$" "^Hecho: norango\.bin, 3000000 octetos"
mira "el servidor se calla sin cerrar: a los 30 s sin un octeto, cortada; y sigue desde donde iba" \
  "^DESCARGAR: se ha cortado la conexión (30 s sin que llegue nada) (a medias: 1500000 octetos)\.$" "^Se sigue desde el octeto 1500000 (de 3000000)" "^Hecho: calla\.bin, 3000000 octetos"
mira "el enlace se cae en plena descarga (set_link off): sin enlace, se espera a la red; al volver (DHCP otra vez), sigue desde donde iba" \
  "red: enlace: no" "la red no tiene enlace (¿está enchufado el cable? ¿se ha desconectado el Wi-Fi?) (a medias: [0-9]* octetos)\.$" \
  "^DESCARGAR: sin red: se espera a que vuelva para seguir (hasta 2 minutos)\.\.\.$" "^DESCARGAR: la red ha vuelto: se sigue\.$" "^Hecho: lento\.bin, 3000000 octetos"
if grep -q '^/rst/grande.bin bytes=[0-9]*-$' $K/srv/pedidos && grep -q '^/calla/grande.bin bytes=1500000-$' $K/srv/pedidos && grep -q '^/lento/grande.bin bytes=[0-9]*-$' $K/srv/pedidos \
  && grep -q '^/norango/grande.bin bytes=' $K/srv/pedidos && [ "$(grep -c '^/cierra/grande.bin' $K/srv/pedidos)" = 2 ]; then
  ok "al servidor, el resto se le pide con «Range: bytes=N-» (y «If-Range»): una vez por corte"
else ko "lo que se le pide al servidor no es lo que toca:"; sed 's/^/        /' $K/srv/pedidos; fi
iguales=1
for f in CIERRA RST NORANGO CALLA LENTO; do saca $K/d1.img $f.BIN $K/x.bin && cmp -s $K/x.bin $K/srv/grande.bin || { iguales=0; echo "        $f.BIN no es igual"; }; done
sano $K/d1.img
if [ $iguales = 1 ] && grep -q "^$K/d1.img: [0-9]* files" $P/fsck.txt; then ok "los cinco archivos, en el disco, iguales octeto a octeto a los del servidor (3 MB cada uno), y el disco sano"
else ko "lo bajado tras los cortes no es igual, o el disco no queda sano"; fi

# 2. TCP: viva sin datos (los keepalive tienen respuesta) y, con el camino
# muerto (el enlace sigue), cortada a los 35 s. calla manda la mitad (la
# primera vez de cada archivo grande: el servidor, otra vez desde cero)
arranca_servidor
cp $K/d.img $K/d2.img
DISCO=$K/d2.img ARRANQUE=ordenes MODULOS= QEMU_MAS="$NIC" $AQUI/nucleo32/probar.sh $K/SEMILLA.BIN ":pausa 2" "ipconfig" ":pausa 1" \
  "tcpesp 10.0.2.2 ${U##*:} /calla/grande.bin" ":pausa 75" ":monitor object_add filter-buffer,id=f0,netdev=n0,queue=all,interval=600000000" ":pausa 70" "ipconfig /todo" ":pausa 2" > $K/d2.sal 2>&1
tr -d '\r' < $K/d2.sal > $K/d2.txt
S=$K/d2.txt
mira "una conexión viva sin datos sigue (los keepalive tienen respuesta); con el camino muerto (sin caerse el enlace), cortada a los 35 s, y el programa lo sabe" \
  "^tcpespera: nada en 60 s, y la conexión sigue (15[0-9]* octetos)$" "^red: tcp: la conexión con el puerto [0-9]* no contesta desde hace 3[5-9] s: cortada$" \
  "^tcpespera: la conexión se ha cortado (15[0-9]* octetos)$"

# 3. PAQUETES INSTALA un paquete de 3 MB de Internet con el enlace caído a mitad
# (PAQUETES con la clave de prueba, como en la parte 12)
$HPQ clave $K/prueba.key > /dev/null 2>&1; $HPQ publica $K/prueba.key $K/clave.cas > /dev/null || hechos=0
cp $AQUI/nucleo32/upaquete.cas $K/upaquete.cas
for p in uordenes:ORDENES upaquete:PAQUETES uprogram:PROGRAMA udescarg:DESCARGA ured:RED; do
  fu=$AQUI/nucleo32/${p%%:*}.cas; [ ${p%%:*} = upaquete ] && fu=$K/upaquete.cas
  python3 -I $AQUI/ccastel.py $fu -o $K/${p%%:*}.asm -I $AQUI/nucleo32 > /dev/null 2>&1 && nasm -f bin $K/${p%%:*}.asm -o $K/f/${p##*:}.BIN || hechos=0
done
head -c 3000000 /dev/urandom > $K/f/GRANDE.DAT; head -c 2500000 /dev/urandom > $K/f/GRANDE2.DAT
( cd $K/f
  $HPQ crea BASE.HPQ --nombre base --version 0.8.7 --tipo sistema --descripcion 'El sistema base (de prueba)' ORDENES.BIN='\HISPA\ORDENES.BIN' PAQUETES.BIN='\HISPA\PAQUETES.BIN' \
    PROGRAMA.BIN='\HISPA\PROGRAMA.BIN' DESCARGA.BIN='\HISPA\DESCARGA.BIN' RED.BIN='\HISPA\RED.BIN'
  $HPQ registro BASE.HPQ BASE.PQI
  $HPQ crea $K/raiz/GRANDE.HPQ --nombre grande --version 1.0 --tipo multimedia --descripcion 'Un paquete grande (3 MB)' GRANDE.DAT='\DATOS\GRANDE.DAT'
  $HPQ crea $K/raiz/GRANDE2.HPQ --nombre grande2 --version 1.0 --tipo herramientas --descripcion 'Otro grande' GRANDE2.DAT='\DATOS\GRANDE2.DAT'
  $HPQ catalogo $K/raiz --clave $K/prueba.key ) > $K/hpq.txt 2>&1 || hechos=0
mkfs.fat -F 32 -s 1 -n HISPA -C $K/sis.img 65536 > /dev/null && mmd -i $K/sis.img ::HISPA ::HISPA/PAQUETES \
  && mcopy -i $K/sis.img $K/f/ORDENES.BIN $K/f/PAQUETES.BIN $K/f/PROGRAMA.BIN $K/f/DESCARGA.BIN $K/f/RED.BIN ::HISPA/ && mcopy -i $K/sis.img $K/f/BASE.PQI ::HISPA/PAQUETES/ \
  && mcopy -i $K/sis.img $K/f/CUELGA.BIN :: || hechos=0
[ $hechos = 1 ] || ko "no se han podido hacer los programas, los paquetes o el disco de las pruebas: $(tail -n 2 $K/hpq.txt)"
arranca_servidor
cp $K/sis.img $K/s3.img
ESCRIBE=1 SATA=$K/s3.img MEMORIA=64 MODULOS= ARRANQUE=ordenes QEMU_MAS="$NIC" $AQUI/nucleo32/probar.sh $K/SEMILLA.BIN ":pausa 2" \
  "paquetes instala grande /desde=$U/lento/pentium/" ":pausa 9" ":monitor set_link nic0 off" ":pausa 12" ":monitor set_link nic0 on" ":pausa 40" "paquetes lista" ":pausa 2" > $K/s3.sal 2>&1
tr -d '\r' < $K/s3.sal > $K/s3.txt
S=$K/s3.txt
mira "PAQUETES INSTALA de Internet con el enlace caído a mitad del .HPQ: DESCARGA espera a la red, sigue desde donde iba, y PAQUETES comprueba las sumas e instala" \
  "^DESCARGAR: sin red: se espera a que vuelva para seguir (hasta 2 minutos)\.\.\.$" "^Se sigue desde el octeto [0-9]* (de 3000[0-9]*)" \
  "^Hecho: C:.HISPA.PAQUETES.BAJADAS.GRANDE\.HPQ, 3000[0-9]* octetos" "^  instalado grande 1\.0$" "^  grande    1\.0 "
if saca $K/s3.img DATOS/GRANDE.DAT $K/x.bin && cmp -s $K/x.bin $K/f/GRANDE.DAT && ! mdir -/ -b -i $K/s3.img ::HISPA/PAQUETES/BAJADAS 2> /dev/null | grep -q HPQ && sano $K/s3.img; then
  ok "en el disco: \\DATOS\\GRANDE.DAT igual al del paquete, BAJADAS sin restos y el disco sano"
else ko "lo instalado tras el corte no queda bien:"; mdir -/ -b -i $K/s3.img :: | sed 's/^/        /' | head -10; fi

# 4. la ventana: «Cancelar» con la descarga parada (el servidor callado:
# la mitad y nada) y «Reiniciar» en plena descarga parada
printf '[hispa]\r\nusuario=Ana\r\nequipo=PRUEBA\r\nteclado=es\r\nmodo=800x600\r\n' > $K/HISPA.INI
arranca_servidor
printf '%s/calla/pentium/\r\n' "$U" > $K/ORIGEN.TXT
cp $K/sis.img $K/s4.img
mcopy -i $K/s4.img $K/HISPA.INI ::HISPA/ && mcopy -i $K/s4.img $K/ORIGEN.TXT ::HISPA/PAQUETES/
rm -f $K/relojes.txt
CAPTURAS=$G RELOJ_A=$K/relojes.txt ESCRIBE=1 SATA=$K/s4.img MEMORIA=64 MODULOS= ARRANQUE=escritorio QEMU_MAS="$NIC" $AQUI/nucleo32/probar.sh $K/SEMILLA.BIN ":pausa 4" \
  ":clic 40 585" ":pausa 1" ":clic 330 308" ":pausa 6" ":tecla ctrl-tab" ":pausa 1" ":captura k1" ":tecla down" ":tecla ret" ":pausa 12" ":captura k2" \
  ":reloj cancela" ":tecla esc" ":pausa 3" ":captura k3" ":tecla down" ":pausa 1" ":captura k4" ":clic 40 585" ":pausa 1" ":clic 216 537" ":pausa 6" > $K/s4.sal 2>&1
S=$K/s4.sal
t0=$(awk '/^cancela / {print $2}' $K/relojes.txt)
t1=$(awk '/el cliente ha cerrado/ {print $NF; exit}' $K/srv/servidos)
ms=$(python3 -I -c "import sys; print(int((float(sys.argv[2]) - float(sys.argv[1])) * 1000))" "${t0:-0}" "${t1:-0}" 2> /dev/null)
if [ -n "$t0" ] && [ -n "$t1" ] && [ "${ms:-99999}" -ge 0 ] && [ "${ms:-99999}" -lt 2000 ]; then
  ok "«Cancelar» (Esc) con la descarga parada (el servidor callado): la conexión se corta en $ms ms (menos de 2 s)"
else ko "«Cancelar» no corta la descarga parada a tiempo (pulsado: ${t0:-?}; el servidor ve el cierre: ${t1:-nunca})"; sed 's/^/        /' $K/srv/servidos; fi
miracap "«Sin instalar»: los dos paquetes grandes del servidor" k1 negrita 370 120 530 140 "Sin instalar (2)" negrita 40 170 200 230 grande
miracap "instalando: «Bajando ... GRANDE.HPQ» se queda en la mitad (el servidor callado), con «Cancelar»" k2 negrita 150 250 650 300 "Bajando" negrita 150 250 650 300 "GRANDE.HPQ:" texto 150 300 650 345 "Cancelar"
miracap "tras «Cancelar», la ventana dice que se ha cancelado y que todo queda como estaba" k3 texto 0 530 800 560 "cancelado: lo tra" negrita 370 120 530 140 "Sin instalar (2)"
miracap "y responde: se mueve por la lista" k4 distinto 12 190 770 50 $G/k3.ppm
mira "y, del menú Inicio, «Reiniciar» reinicia (la ventana se cierra sola)" "^escritorio: reiniciar$"
if ! grep -q "no se cierran" $S && ! mcopy -n -i $K/s4.img ::HISPA/PAQUETES/GRANDE.PQI $K/x.pqi > /dev/null 2>&1 && ! mdir -/ -b -i $K/s4.img ::HISPA/PAQUETES/BAJADAS 2> /dev/null | grep -q HPQ \
  && ! mdir -/ -b -i $K/s4.img ::HISPA/PAQUETES 2> /dev/null | grep -q 'DIARIO\|CANCELA\|AVANCE' && sano $K/s4.img; then
  ok "en el disco, todo como estaba: grande sin instalar, nada a medias en BAJADAS, sin diario ni CANCELA.TXT, y el disco sano"
else ko "tras «Cancelar» el disco no queda como estaba:"; mdir -/ -b -i $K/s4.img ::HISPA/PAQUETES | sed 's/^/        /' | head -10; fi
# «Reiniciar» con la descarga parada (el servidor, otra vez desde cero)
arranca_servidor
printf '%s/calla/pentium/\r\n' "$U" > $K/ORIGEN.TXT
cp $K/sis.img $K/s5.img
mcopy -i $K/s5.img $K/HISPA.INI ::HISPA/ && mcopy -i $K/s5.img $K/ORIGEN.TXT ::HISPA/PAQUETES/
rm -f $K/relojes.txt
CAPTURAS=$G RELOJ_A=$K/relojes.txt ESCRIBE=1 SATA=$K/s5.img MEMORIA=64 MODULOS= ARRANQUE=escritorio QEMU_MAS="$NIC" $AQUI/nucleo32/probar.sh $K/SEMILLA.BIN ":pausa 4" \
  ":clic 40 585" ":pausa 1" ":clic 330 308" ":pausa 6" ":tecla ctrl-tab" ":pausa 1" ":tecla down" ":tecla ret" ":pausa 12" \
  ":clic 40 585" ":pausa 1" ":reloj reinicia" ":clic 216 537" ":hasta 20 1 escritorio: reiniciar" ":reloj reiniciado" ":pausa 1" > $K/s5.sal 2>&1
S=$K/s5.sal
t0=$(awk '/^reinicia / {print $2}' $K/relojes.txt); t1=$(awk '/^reiniciado / {print $2}' $K/relojes.txt)
ms=$(python3 -I -c "import sys; print(int((float(sys.argv[2]) - float(sys.argv[1])) * 1000))" "${t0:-0}" "${t1:-0}" 2> /dev/null)
if grep -q "^escritorio: reiniciar$" $S && ! grep -q "no se cierran" $S && [ "${ms:-99999}" -lt 8000 ]; then
  ok "«Reiniciar» del menú Inicio en plena descarga parada: la ventana cancela, se cierra sola y el equipo reinicia ($ms ms)"
else ko "«Reiniciar» con la descarga parada no reinicia solo (${ms:-?} ms):"; grep '^escritorio\|^programa' $S | tail -n 6 | sed 's/^/        /'; fi
if ! mcopy -n -i $K/s5.img ::HISPA/PAQUETES/GRANDE.PQI $K/x.pqi > /dev/null 2>&1 && ! mdir -/ -b -i $K/s5.img ::HISPA/PAQUETES/BAJADAS 2> /dev/null | grep -q HPQ && sano $K/s5.img; then
  ok "y en el disco, todo como estaba (nada instalado ni a medias; el disco sano)"
else ko "tras reiniciar en plena descarga, el disco no queda como estaba"; fi

# 5. un programa colgado al reiniciar: «Forzar el cierre»
cp $K/sis.img $K/s6.img
mcopy -i $K/s6.img $K/HISPA.INI ::HISPA/
CAPTURAS=$G ESCRIBE=1 SATA=$K/s6.img MEMORIA=64 MODULOS= ARRANQUE=escritorio $AQUI/nucleo32/probar.sh $K/SEMILLA.BIN ":pausa 4" \
  ":tecla meta_l-r" ":pausa 1" ":escribe cuelga" ":tecla ret" ":pausa 2" ":clic 40 585" ":pausa 1" ":clic 216 537" ":pausa 5" ":captura k5" ":tecla esc" ":pausa 2" ":captura k6" \
  ":clic 40 585" ":pausa 1" ":clic 216 537" ":hasta 30 1 nadie ha elegido" ":pausa 2" > $K/s6.sal 2>&1
S=$K/s6.sal
mira "un programa colgado (no lee lo que le llega) al reiniciar: «Estos programas no se han cerrado»; Cancelar no reinicia; sin elegir nada, a los 15 s se fuerza el cierre y se reinicia" \
  "^cuelga: en marcha" "^escritorio: hay programas que no se cierran: «Forzar el cierre»$" "^escritorio: no se apaga (Cancelar)$" \
  "^escritorio: nadie ha elegido: se fuerza el cierre$" "^escritorio: se fuerza el cierre de la tarea [0-9]* (Programa colgado)$" "^escritorio: reiniciar$"
miracap "el cuadro: el programa, que no responde, «Forzar el cierre» y «Cancelar», y lo que queda para forzarlo" k5 negrita 0 0 800 600 "Estos programas no se han cerrado:" \
  texto 0 0 800 600 "Programa colgado (no responde)" negrita 0 0 800 600 "Forzar el cierre" negrita 0 0 800 600 "Cancelar" texto 0 0 800 600 "se forzará dentro de 1"
miracap "con «Cancelar», el cuadro se va y el programa sigue ahí" k6 notexto 0 0 800 600 "Estos programas no se han cerrado:"
cp $K/sis.img $K/s7.img
mcopy -i $K/s7.img $K/HISPA.INI ::HISPA/
python3 -I $AQUI/nucleo32/mira_captura.py -d $G/k5.ppm negrita 0 0 800 600 "Forzar el cierre" > $K/donde.txt 2>&1
r=$(grep -o '([0-9]*, [0-9]*)' $K/donde.txt | head -1 | tr -d '(),')
[ -n "$r" ] || r="214 371"
ESCRIBE=1 SATA=$K/s7.img MEMORIA=64 MODULOS= ARRANQUE=escritorio $AQUI/nucleo32/probar.sh $K/SEMILLA.BIN ":pausa 4" \
  ":tecla meta_l-r" ":pausa 1" ":escribe cuelga" ":tecla ret" ":pausa 2" ":clic 40 585" ":pausa 1" ":clic 216 537" ":pausa 5" ":clic $(( ${r% *} + 20 )) $(( ${r#* } + 6 ))" ":hasta 10 1 escritorio: reiniciar" > $K/s7.sal 2>&1
S=$K/s7.sal
mira "y con el botón «Forzar el cierre»: se acaba el programa colgado y se reinicia (los discos, asentados antes)" "^escritorio: se fuerza el cierre de la tarea [0-9]* (Programa colgado)$" "^escritorio: reiniciar$"
if ! grep -q "nadie ha elegido" $S && sano $K/s7.img; then ok "sin esperar a los 15 s, y el disco sano"; else ko "«Forzar el cierre» no ha ido por el botón, o el disco no queda sano"; fi

# 6. el Wi-Fi (la radio simulada): el punto de acceso se va y vuelve con
# otra IP; y la IP que cambia sin que se vaya el enlace
$AQUI/nucleo32/disco.sh $K/w.img 16 red wifisim > $K/discow.txt 2>&1 || ko "disco.sh no hace el disco del Wi-Fi: $(tail -n 2 $K/discow.txt)"
MEMORIA=64 DISCO=$K/w.img ARRANQUE=ordenes MODULOS= $AQUI/nucleo32/probar.sh $K/SEMILLA.BIN "redwifi /estado" ":pausa 6" "redwifi /redes" ":pausa 2" "redwifi /conectar HispaPrueba" "clave de prueba" ":pausa 4" \
  "ping -n 1 192.168.77.1" ":pausa 2" "redwifi /simcorte 12 ip11" ":pausa 30" "ipconfig" ":pausa 1" "ping -n 2 192.168.77.1" ":pausa 3" \
  "redwifi /simcorte 0 ip10" "ipconfig /renovar" ":pausa 4" "ipconfig" ":pausa 1" "ping -n 1 192.168.77.1" ":pausa 2" > $K/w.sal 2>&1
tr -d '\r' < $K/w.sal > $K/w.txt
S=$K/w.txt
mira "el punto de acceso desaparece: WIFI.BIN lo nota (8 s sin oírlo) y RED.BIN se queda sin enlace (lo que hubiera, cortado)" \
  "^wifi: .*SIM: el punto de acceso desaparece 12 s; DHCP da la 192\.168\.77\.11$" "wifi: .*802\.11: se ha perdido la red (no se oye al punto de acceso)$" "red: enlace: no (el Wi-Fi no está conectado)$"
mira "al volver, WIFI.BIN se conecta solo y se lo dice a RED.BIN: DHCP otra vez, con la IP, la puerta y el DNS nuevos; y lo nuevo (PING) va" \
  "^red: DHCP: IP 192\.168\.77\.11, máscara 255\.255\.255\.0, puerta 192\.168\.77\.1, DNS 192\.168\.77\.1$" "^  Dirección IP  . . . . : 192\.168\.77\.11$" \
  "^    Paquetes: enviados = 2, recibidos = 2, perdidos = 0 (0% perdidos)$"
mira "la IP cambia sin que se vaya el enlace (IPCONFIG /RENOVAR, y DHCP da otra): RED.BIN lo ve y corta lo de antes; lo nuevo va" \
  "^red: DHCP: la red ha cambiado (otra IP u otra puerta de enlace): las conexiones de antes se cortan$" "^red: DHCP: IP 192\.168\.77\.10," "^  Dirección IP  . . . . : 192\.168\.77\.10$"
# (la contraseña del Wi-Fi sale con asteriscos: «\*\*\*» solo al principio de la línea)
if ! grep -q "excepción\|^\*\*\*" $K/d1.sal $K/d2.sal $K/s3.sal $K/s4.sal $K/s5.sal $K/s6.sal $K/s7.sal $K/w.sal; then ok "ni una excepción del núcleo en toda la parte"
else ko "hay excepciones:"; grep -h "excepción\|^\*\*\*" $K/*.sal | head -3 | sed 's/^/        /'; fi
[ -n "${GUARDA_CAPTURAS:-}" ] && mkdir -p "$GUARDA_CAPTURAS" && cp $G/k[1-6].ppm "$GUARDA_CAPTURAS/"
kill $SPID 2> /dev/null; wait $SPID 2> /dev/null
SPID=
}
