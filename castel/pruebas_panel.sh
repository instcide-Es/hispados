# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# pruebas_panel.sh - la parte 34 (SOLO=panel) de pruebas.sh, que lee este
# archivo («.»): (PANEL) el Panel de control (Configuración con sus iconos y
# PANEL.BIN) y (NTP) la hora por Internet (HORARED.BIN), con los ayudantes de
# pruebas.sh (ok, ko, mira, miracap, saca, sano) y sus carpetas ($AQUI, $P, $G).
#
# Los servidores NTP son de mentira (nucleo32/ntp_prueba.py, en esta
# máquina: «10.0.2.2:PUERTO» desde QEMU): dos que dicen la hora de verdad,
# uno que dice el 1/1/2001, otro que va dos días por delante y otro que no
# contesta. El reloj del PC (la CMOS de QEMU, RELOJ) empieza cinco minutos
# adelantado respecto de la hora de Madrid. Y los HTTPS de
# nucleo32/tls/servidor_https.py, para ver que TLS sigue yendo aunque NTP
# mienta, y que no se fía de un reloj de antes de TLS.HOR.
#
# Las posiciones, las del escritorio a 800 x 600: Configuración (el menú del
# tapiz) con sus iconos de 128 x 96 desde (148, 185); el Panel de control,
# de 784 x 560 en (8, 3), con las secciones a la izquierda (una cada 62
# puntos desde y = 69) y lo de la sección desde x = 236.

parte_panel() {
echo "== Hispa-Pentium 0.8.7: el Panel de control (Configuración, PANEL.BIN) y la hora por Internet (HORARED.BIN, NTP) =="
local K=$P/panel S NP S0 S1 S2 S3 S4 NPID SPID B pu hoy relojes dia
mkdir -p $K/srv $G
export MTOOLS_SKIP_CHECK=1
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/semilla.cas -o $K/semilla.asm > /dev/null && nasm -f bin $K/semilla.asm -o $K/SEMILLA.BIN || { ko "semilla.cas no compila"; return; }
bash $AQUI/../acpi/pentium/hacer.sh $K/ACPI.BIN > $K/acpi.txt 2>&1 || { ko "no se hace ACPI.BIN: $(tail -n 2 $K/acpi.txt)"; return; }
$AQUI/nucleo32/disco.sh $K/d.img 16 red tls panel > $K/disco.txt 2>&1 || { ko "disco.sh no hace el disco: $(tail -n 2 $K/disco.txt)"; return; }
ok "PANEL.BIN, HORARED.BIN, el núcleo con las llamadas 174 y 175 y ACPI.BIN con el botón de encendido compilan (PANEL.BIN: $(MTOOLS_SKIP_CHECK=1 mdir -i $K/d.img ::PANEL.BIN | awk '/^PANEL/ {print $3}') octetos; HORARED.BIN: $(MTOOLS_SKIP_CHECK=1 mdir -i $K/d.img ::HORARED.BIN | awk '/^HORARED/ {print $3}'))"
# los servidores de mentira
NP=$((21000 + $$ % 20000))
S0=10.0.2.2:$NP; S1=10.0.2.2:$((NP + 1)); S2=10.0.2.2:$((NP + 2)); S3=10.0.2.2:$((NP + 3)); S4=10.0.2.2:$((NP + 4))
python3 -I $AQUI/nucleo32/ntp_prueba.py $NP:0 $((NP + 1)):0 $((NP + 2)):2001-01-01 $((NP + 3)):+172800 $((NP + 4)):callado > $K/ntp.txt 2>&1 &
NPID=$!
python3 -I $AQUI/nucleo32/tls/servidor_https.py $K/srv > $K/srv.txt 2>&1 &
SPID=$!
for _ in $(seq 100); do [ -s $K/srv/puertos ] && grep -q escuchando $K/ntp.txt && break; sleep 0.1; done
pu() { grep "^$1=" $K/srv/puertos | cut -d= -f2; }
B=https://10.0.2.2:$(pu bueno)
mmd -i $K/d.img ::RAICES && mcopy -i $K/d.img $K/srv/RAIZ.CER ::RAICES/PRUEBA.CER
# el reloj del PC: la hora de Madrid más cinco minutos
relojes() { TZ=Europe/Madrid date -d "$1" +%Y-%m-%dT%H:%M:%S; }

# 1. HORARED en la Consola (ORDENES, con la red): una copia fuera de \HISPA
# no tiene el sello; dos servidores de acuerdo la ponen; el del 2001, no; el
# de dos días por delante, no sin el modo administrador; uno bueno y uno malo
# no se ponen de acuerdo; el callado; atrasar 3 horas no, 1 sí (y luego,
# para volver, hace falta ADMIN); y HTTPS sigue yendo
cp $K/d.img $K/c.img
RELOJ=$(relojes "+5 min") ESCRIBE=1 DISCO=$K/c.img ARRANQUE="ordenes conred" MODULOS= QEMU_MAS="-nic user,model=e1000" $AQUI/nucleo32/probar.sh "" ":pausa 3" \
  "md otra" "copy horared.bin otra" "otra\\horared /ahora $S0,$S1" ":pausa 4" "horared" \
  "horared /ahora $S0,$S1" ":pausa 4" "horared /ahora $S0,$S1" ":pausa 4" "horared" \
  "horared /ahora $S2" ":pausa 3" "horared /ahora $S3" ":pausa 3" "horared /ahora $S0,$S3" ":pausa 4" "horared /ahora $S4" ":pausa 4" \
  "horared /atrasa 3" "horared /atrasa 1" "horared /ahora $S0,$S1" ":pausa 4" \
  "admin" ":pausa 1" ":tecla s" ":pausa 1" "horared /ahora /forzar $S0,$S1" ":pausa 4" "admin no" \
  "descargar $B/ultima.txt -" ":pausa 6" "https /estado" ":pausa 2" > $K/c.sal 2>&1
tr -d '\r' < $K/c.sal > $K/c.txt
S=$K/c.txt
mira "una copia de HORARED.BIN fuera de la carpeta del sistema no tiene su sello: el núcleo no le deja poner la hora (175) ni apuntar en el Panel (174)" \
  "^permisos: acceso denegado a poner el reloj del sistema (HORARED.BIN" "^hora: el núcleo no deja poner la hora$" \
  "^permisos: acceso denegado a cambiar el ajuste 21 del Panel de control (HORARED.BIN)"
mira "HORARED sin nada: la zona (Europa/Madrid), el horario de verano (en noviembre, el de invierno) y la hora por Internet" \
  "^Ahora: [0-9/]* [0-9:]* (Europa/Madrid (península, Baleares, Ceuta y Melilla))$" "^Horario de verano: solo (ahora, el de invierno)$" \
  "^La hora por Internet: encendida$" "^La última vez: aún no se ha preguntado la hora a Internet$"
mira "HORARED /AHORA con dos servidores que dicen lo mismo: el reloj iba cinco minutos adelantado y se pone en hora (de una vez, al empezar el segundo)" \
  "^hora: 10.0.2.2 (10.0.2.2:$NP) da [0-9/]* [0-9:,]* UTC (ida y vuelta: [0-9]* ms)$" \
  "^hora: la hora de aquí: .* (el reloj del PC va adelantado 4 min [0-9]* s; contestan 2, de acuerdo 2)$\|^hora: la hora de aquí: .* (el reloj del PC va adelantado 5 min [0-9]* s; contestan 2, de acuerdo 2)$" \
  "^hora: puesta en hora: -[23][0-9][0-9] s$" "^hora: puesto en hora por Internet$" "(HORARED.BIN ha acabado con el código 2)"
mira "y otra vez: ya está en hora (un segundo arriba o abajo); el estado lo dice" "^hora: el reloj del PC está en hora$" "^hora: el reloj estaba en hora$" \
  "^La última vez: el reloj estaba en hora (el [0-9/]* [0-9:]*; el reloj iba "
mira "un servidor que dice el 1/1/2001: de antes de lo que se sabe (la fecha de HORARED.BIN): no se cree" \
  "^hora: 10.0.2.2 dice 01/01/2001 12:00:00: no puede ser" "(HORARED.BIN ha acabado con el código 7)"
mira "uno que va dos días por delante: con el reloj creíble, más de 15 minutos no se cambian solos (lo pide el Panel con el modo administrador)" \
  "^hora: el reloj del PC va atrasado 2 d 0 h: no se cambia solo" "(HORARED.BIN ha acabado con el código 8)"
mira "uno bueno y uno que miente: no dicen lo mismo y no se toca nada; y el que no contesta" \
  "^hora: los servidores no dicen lo mismo: no se cambia nada$" "(HORARED.BIN ha acabado con el código 6)" \
  "^hora: 10.0.2.2 no contesta$" "(HORARED.BIN ha acabado con el código 5)"
mira "el núcleo no deja a HORARED atrasar el reloj tres horas sin el modo administrador; una (el cambio al horario de invierno), sí" \
  "^permisos: HORARED.BIN no puede atrasar el reloj 10800 segundos sin el modo administrador" "^hora: atrasado$"
mira "con el reloj una hora atrasado, la hora de Internet ya no se pone sola; con ADMIN (lo pregunta el núcleo) y /FORZAR, sí" \
  "^hora: el reloj del PC va atrasado 1 h 0 min: no se cambia solo" "El núcleo pregunta: ORDENES.BIN" "^hora: puesta en hora: 3[56][0-9][0-9] s$"
mira "y HTTPS sigue yendo, con la hora del reloj del PC (los servidores que mienten no la han tocado)" \
  "^Hispa-Pentium 0.8: la red, cifrada.$" "^  Fecha . . . . . . . . : .* (del reloj del PC)$"
if ! grep -q "excepción\|\*\*\*" $S; then ok "ni una excepción ni una queja del núcleo"; else ko "el núcleo se ha quejado:"; grep "excepción\|\*\*\*" $S | head -4 | sed 's/^/        /'; fi

# 2. TLS.HOR: el último día con una cadena buena es el suelo. Con TLS.HOR
# dentro de diez días y el reloj del PC dentro de cinco, TLS no se fía del
# reloj (y, sin la hora de la cabecera Date, no conecta); y HORARED no cree
# a un servidor que diga hoy
cp $K/d.img $K/t.img
dia=$(python3 -c "import datetime as d; x=d.date.today()+d.timedelta(days=10); print((x-d.date(1,1,1)).days+366)")
python3 -c "import struct,sys; open(sys.argv[1],'wb').write(struct.pack('<I', int(sys.argv[2])))" $K/TLS.HOR $dia
printf 'hora=10.0.2.2:1\r\n' > $K/TLS.INI
mcopy -i $K/t.img $K/TLS.HOR $K/TLS.INI ::
RELOJ=$(relojes "+5 days") DISCO=$K/t.img ARRANQUE="ordenes conred" MODULOS= QEMU_MAS="-nic user,model=e1000" $AQUI/nucleo32/probar.sh "" ":pausa 3" \
  "descargar $B/ultima.txt -" ":pausa 6" "horared /ahora $S0,$S1" ":pausa 4" > $K/t.sal 2>&1
tr -d '\r' < $K/t.sal > $K/t.txt
S=$K/t.txt
mira "TLS.BIN no se fía de un reloj del PC de antes de TLS.HOR (aunque sea de después de su compilación): sin la cabecera Date, no conecta" \
  "tls: el reloj del PC dice [0-9/]*: es de antes del último día con una cadena buena (TLS.HOR): no se cree" \
  "^DESCARGAR: no hay conexión segura con 10.0.2.2: el reloj del equipo no está en hora"
mira "y HORARED tampoco cree a quien diga una fecha de antes de TLS.HOR" "^hora: 10.0.2.2 dice [0-9/]* [0-9:]*: no puede ser" "(HORARED.BIN ha acabado con el código 7)"

# 3. el escritorio: HORARED la pone sola al arrancar (con la red); Configuración
# con sus iconos (y el teclado); el Panel de control, sección a sección
cp $K/d.img $K/e.img
printf 'ntp=%s,%s\r\n' $S0 $S1 > $K/PANEL.INI
mcopy -i $K/e.img $K/PANEL.INI ::
mcopy -i $K/e.img $K/ACPI.BIN ::
RELOJ=$(relojes "+5 min") CAPTURAS=$G ESCRIBE=1 DISCO=$K/e.img ARRANQUE="escritorio acpicada=1" MODULOS= QEMU_MAS="-nic user,model=e1000" $AQUI/nucleo32/probar.sh "" ":pausa 9" \
  ":clicd 400 300" ":pausa 1" ":clic 410 309" ":pausa 1" ":raton 700 560" ":captura pn0" ":tecla right" ":tecla down" ":pausa 1" ":captura pn1" ":tecla up" ":tecla left" ":tecla ret" ":pausa 4" ":captura pn2" \
  ":clic 100 131" ":pausa 1" ":clic 626 111" ":pausa 1" ":tecla shift-tab" ":tecla ret" ":pausa 1" ":clic 370 177" ":pausa 1" ":clic 300 284" ":pausa 1" ":doble 560 161" ":pausa 1" \
  ":clic 248 211" ":pausa 1" ":clic 626 111" ":clicd 626 111" ":pausa 1" ":tecla shift-tab" ":tecla ret" ":pausa 1" ":clicd 248 211" ":pausa 1" ":tecla tab" ":tecla down" ":tecla down" ":pausa 1" \
  ":raton 400 450" ":captura pn3" \
  ":clic 100 193" ":pausa 1" ":tecla tab" ":tecla tab" ":tecla tab" ":tecla tab" ":tecla down" ":pausa 1" ":clic 500 335" ":tecla apostrophe" ":tecla semicolon" ":tecla shift-equal" ":pausa 1" ":captura pn4" \
  ":clic 100 193" ":tecla tab" ":tecla tab" ":tecla tab" ":tecla tab" ":tecla up" ":pausa 1" \
  ":clic 100 317" ":pausa 1" ":tecla tab" ":tecla tab" ":tecla down" ":tecla tab" ":tecla tab" ":tecla down" ":pausa 1" ":captura pn5" \
  ":clic 100 379" ":pausa 1" ":tecla tab" ":tecla down" ":tecla down" ":tecla down" ":pausa 2" ":clic 551 239" ":pausa 2" ":raton 400 450" ":captura pn6" \
  ":clic 100 69" ":pausa 1" ":raton 400 450" ":captura pn7" ":clic 728 508" ":pausa 1" ":clic 620 486" ":pausa 70" ":captura pn8" ":raton 300 300" ":pausa 1" ":captura pn9" \
  ":monitor system_powerdown" ":pausa 3" ":captura pn10" ":tecla esc" ":pausa 1" > $K/e.sal 2>&1
S=$K/e.sal
mira "al arrancar con la red, HORARED.BIN (lanzado por el escritorio, con su sello) pone el reloj en hora sola (iba cinco minutos adelantado)" \
  "hora: HORARED.BIN en marcha: la hora por Internet y el horario de verano$" "hora: el reloj del PC lleva el horario de invierno$" \
  "^hora: puesta en hora: -[23][0-9][0-9] s$"
miracap "Configuración es el Panel de control: un icono grande con su nombre por sección (cada uno con su forma), la pantalla y la memoria arriba" pn0 \
  negrita 140 85 160 100 "Configuración" negrita 190 115 200 125 "Hispa-Pentium $VERSION_BASE: Panel de control" texto 190 135 200 145 "Pantalla:  800 x 600, 32 bits por punto" \
  negrita 155 245 165 255 "Fecha y hora" negrita 300 245 310 255 "Pantalla" negrita 435 245 445 255 "Sonido" negrita 565 245 575 255 "Ratón" \
  negrita 175 341 185 351 "Teclado" negrita 285 341 295 351 "Red y Wi-Fi" negrita 430 341 440 351 "Energía" negrita 550 341 560 351 "Programas" \
  negrita 140 437 150 447 "Personalización" negrita 280 437 290 447 "Dispositivos" negrita 420 437 430 447 "Acerca de" rgb 149 230 1D4ED8 30
miracap "con el teclado: las flechas llevan el borde azul grueso de icono en icono (a la derecha y abajo: Red y Wi-Fi)" pn1 rgb 277 326 1D4ED8 30 nrgb 149 230 1D4ED8 30
mira "Intro abre la sección: Fecha y hora es el Panel de control (PANEL.BIN /FECHA)" "^escritorio: Panel de control > Fecha y hora$" "^escritorio: lanzo .PANEL.BIN" "^panel: Fecha y hora$"
miracap "Fecha y hora: la hora, la zona (Europa/Madrid), el horario de verano, la hora por Internet (puesta sola al arrancar) y ponerla a mano" pn2 \
  negrita 30 5 40 12 "Panel de control - Fecha y hora" texto15 230 120 300 150 "Europa/Madrid (península, Baleares, Ceuta y Melilla)" \
  texto15 230 160 300 190 "Cambiar solo al horario de verano y al de invierno" negrita15 240 220 300 260 "Sincronizar ahora" \
  texto15 230 245 300 260 "La última vez: puesto en hora por Internet" negrita15 240 300 300 330 "Poner en hora a mano (pide el modo administrador)"
mira "Ratón: más rápida (y con el teclado otra vez a 5), el doble clic «Lento» (de la lista), la prueba con dos clics, para zurdos (el izquierdo ya no es el principal: el derecho sí) y quitarlo (con el derecho)" \
  "^panel: el ajuste 1, a 6$" "^panel: el ajuste 1, a 5$" "^panel: el ajuste 2, a 60$" "^panel: el doble clic de prueba, bien$" \
  "^panel: el ajuste 3, a 1$" "^panel: el ajuste 3, a 0$"
[ "$(grep -c '^panel: el ajuste 1, a 6$' $S)" = 2 ] && ok "con el ratón de zurdos, el clic izquierdo no pulsa «Más rápida» y el derecho sí (dos veces a 6: antes y con el derecho)" \
  || ko "el ratón de zurdos no cambia los botones: $(grep -c '^panel: el ajuste 1, a 6$' $S) veces a 6"
miracap "Ratón: la velocidad («5 de 10», con sus casillas), el doble clic «Lento», la prueba abierta y la rueda" pn3 \
  negrita15 230 60 300 90 "Velocidad de la flecha: 5 de 10" texto15 230 160 300 190 "Lento (0,6 s)" texto15 580 140 650 170 "¡ Bien ! Abierta"
mira "la rueda: 9 renglones por paso (con el teclado, en la lista)" "^panel: el ajuste 4, a 9$"
mira "Teclado: el latinoamericano (con el teclado de QEMU: la tecla de la tilde da «{», la de la eñe, «ñ», y Mayús y la de «¡¿», «¡» y no «¿»), y otra vez el español" \
  "^panel: el ajuste 5, a 2$" "^panel: el ajuste 5, a 1$"
miracap "en el campo de prueba, con el latinoamericano: «{ñ¡»" pn4 texto15 230 318 300 330 "{ñ¡"
mira "Energía: apagar la pantalla al minuto y, con el botón de encendido, preguntar" "^panel: el ajuste 9, a 1$" "^panel: el ajuste 11, a 1$"
miracap "Energía: «Al minuto», la batería (no hay) y «Preguntar qué hacer»" pn5 texto15 230 90 300 130 "Al minuto" negrita15 230 125 300 140 "Este equipo no dice tener batería" \
  texto15 230 250 300 290 "Preguntar qué hacer"
mira "Personalización: el tema Grafito (de la lista) y el tapiz Verde: el escritorio se repinta" "^panel: el ajuste 12, a 3$" "^panel: el ajuste 13, a 2$" \
  "^escritorio: los ajustes del Panel de control (el tema 3, el tapiz 2)$"
miracap "el tema Grafito: el título de la ventana activa y la barra de tareas, en gris oscuro (con la letra blanca), y el tapiz verde detrás" pn6 \
  rgb 400 14 434A55 24 rgb 300 590 2C3138 16 texto15 230 160 300 175 "Contraste de la letra: 5,7 a 1 (pide 4,5)" negrita15 400 220 450 235 "Verde (elegido)"
miracap "la hora puesta, en Fecha y hora" pn7 texto15 230 245 300 260 "La última vez: puesto en hora por Internet"
mira "sin tocar nada un minuto, la pantalla se apaga (negra); al mover el ratón, se enciende" "^escritorio: la pantalla, apagada (tras 1 min sin tocar nada)$" \
  "^escritorio: la pantalla, encendida otra vez$"
miracap "la pantalla apagada: negra entera" pn8 rgb 10 10 000000 0 rgb 400 300 000000 0 rgb 790 590 000000 0
miracap "y encendida otra vez" pn9 nrgb 10 10 000000 4 negrita 30 570 46 580 Inicio
mira "el botón de encendido (QEMU: system_powerdown): ACPI.BIN lo ve (PWRBTN_STS) y el escritorio pregunta, como dice el Panel" \
  "^ACPI: el botón de encendido, en PM1a_EVT 600h (PWRBTN_EN puesto)$" "^ACPI: se ha pulsado el botón de encendido (1)$" \
  "^escritorio: el botón de encendido (Panel de control: 1)$"
miracap "«Apagar el equipo», por el botón de encendido" pn10 negrita 100 100 700 300 "Apagar el equipo"
if ! grep -q "excepción\|\*\*\*" $S; then ok "en el escritorio, ni una excepción ni una queja del núcleo"; else ko "el núcleo se ha quejado:"; grep "excepción\|\*\*\*" $S | head -4 | sed 's/^/        /'; fi
if saca $K/e.img PANEL.INI $K/panel.ini && tr -d '\r' < $K/panel.ini > $K/panel.txt && grep -q "^raton_velocidad=5$" $K/panel.txt && grep -q "^raton_doble=60$" $K/panel.txt && \
   grep -q "^raton_zurdo=0$" $K/panel.txt && grep -q "^raton_rueda=9$" $K/panel.txt && grep -q "^teclado=1$" $K/panel.txt && grep -q "^pantalla_apagar=1$" $K/panel.txt && \
   grep -q "^boton_encendido=1$" $K/panel.txt && grep -q "^tema=3$" $K/panel.txt && grep -q "^tapiz=2$" $K/panel.txt && grep -q "^verano_puesto=0$" $K/panel.txt && grep -q "^ntp=$S0,$S1$" $K/panel.txt; then
  ok "PANEL.INI (lo escribe el núcleo) guarda cada ajuste, y la línea «ntp=» tal cual"
else ko "PANEL.INI no guarda lo que toca:"; sed 's/^/        /' $K/panel.txt 2>&1 | head -24; fi
sano $K/e.img
if grep -q "^$K/e.img: [0-9]* files" $P/fsck.txt; then ok "y el disco queda sano"; else ko "el disco no queda sano:"; head -4 $P/fsck.txt | sed 's/^/        /'; fi

# 4. al arrancar otra vez: el tema, el tapiz y lo demás, como se dejó
RELOJ=$(relojes "+0 min") CAPTURAS=$G DISCO=$K/e.img ARRANQUE=escritorio MODULOS= $AQUI/nucleo32/probar.sh "" ":pausa 4" ":captura pn11" > $K/r.sal 2>&1
S=$K/r.sal
mira "al arrancar, el escritorio lee PANEL.INI y lo pone" "^panel: los ajustes del Panel de control (.PANEL.INI)$"
miracap "el tapiz verde y la barra de tareas del tema Grafito, desde el arranque" pn11 rgb 10 10 1F6B52 rgb 300 590 2C3138 16

# 5. la batería baja: con la batería de mentira (tests/acpi/bateria-qemu.aml) al 4 % y sin cable, el
# escritorio avisa y apaga el equipo (lo de siempre: al 5 %)
local SSDT="-acpitable file=$AQUI/../tests/acpi/bateria-qemu.aml"
cmosq() { echo ":monitor o /b 0x70 0x$1"; echo ":monitor o /b 0x71 0x$2"; }
local PUESTA
mapfile -t PUESTA < <(cmosq 60 04; cmosq 61 02; cmosq 62 50)
MODULOS=$K/ACPI.BIN ARRANQUE="escritorio acpicada=1" QEMU_MAS="-S $SSDT" $AQUI/nucleo32/probar.sh $K/SEMILLA.BIN "${PUESTA[@]}" ":monitor cont" ":pausa 15" > $K/b.sal 2>&1
S=$K/b.sal
mira "con la batería al 4 % y sin cable (el Panel apaga al 5 %), el escritorio apaga el equipo solo, en orden (ACPI S5)" \
  "^escritorio: la batería, al 4 % (el Panel de control apaga al 5 %): se apaga el equipo$" "^escritorio: apagar" "^\[código 0\]"

# 6. los temas: la letra blanca sobre el color más claro de cada degradado
# (el título, la barra, la bandeja, los botones) tiene el contraste de WCAG
# (4,5 a 1); el de siempre se queda como estaba
if python3 -I - $AQUI/nucleo32/preferencias.cas <<'PY' > $K/contraste.txt 2>&1; then ok "los cinco temas nuevos tienen la letra con contraste de 4,5 a 1 o más (WCAG): $(tr '\n' ' ' < $K/contraste.txt)"; else ko "contraste: $(cat $K/contraste.txt)"; fi
import re, sys
def L(c):
    f = lambda v: v / 12.92 if v <= 0.03928 else ((v + 0.055) / 1.055) ** 2.4
    r, g, b = [((c >> s) & 255) / 255 for s in (16, 8, 0)]
    return 0.2126 * f(r) + 0.7152 * f(g) + 0.0722 * f(b)
def cr(a, b):
    a, b = sorted((L(a), L(b)), reverse=True)
    return (a + 0.05) / (b + 0.05)
src = open(sys.argv[1], encoding='utf-8').read()
temas = re.findall(r'caso (\d): colores\(([^)]*)\)', src)
temas += [('5', m) for m in re.findall(r'\n\s+colores\((0x000000[^)]*)\)', src)]
assert len(temas) == 5, temas
mal = 0
for n, args in temas:
    c = [int(x, 16) for x in args.split(',')]
    peor = min(cr(0xFFFFFF, c[i]) for i in (0, 2, 4, 6, 8))
    print('%s: %.1f;' % (n, peor))
    mal += peor < 4.5
sys.exit(mal)
PY
kill $NPID $SPID 2> /dev/null
}
