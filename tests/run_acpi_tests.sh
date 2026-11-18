#!/bin/bash
# [ACPI] Prueba de la batería por ACPI (HSPACPI, docs/ACPI.md).
#
# 1) En el equipo de compilación (build/acpi_sim): el mismo núcleo de 32 bits
#    que HSPACPI.BIN (ACPICA + acpi/nucleo/) con las tablas ACPI de verdad del
#    Toshiba NB250-10N y del HP Mini 5103 (tests/acpi/) y un controlador
#    embebido simulado. Se comprueba, en el modo mínimo y en el completo:
#     - que _BIF y _BST dan lo que hay en los registros del controlador y de
#       ahí salen el porcentaje, el cable, «cargando» y los minutos;
#     - HP: que un cambio SIN aviso del controlador se ve (PWUP), y un aviso
#       (_Q03) se atiende en el modo completo;
#     - que quitar la batería se nota; que un controlador que no contesta no
#       cuelga nada ni inventa un nivel, y que al volver se recupera;
#     - /PSR en el HP (su _PSR toca la red por cable: por eso no se usa);
#     - un equipo sin ACPI.
# 2) En QEMU (que tiene ACPI pero no batería): con una batería de mentira en
#    una SSDT (tests/acpi/bateria-qemu.asl, -acpitable) que lee el nivel de la
#    CMOS, el camino entero: HSPACPI.COM carga el núcleo en memoria extendida,
#    lee, contesta INT 15h AX=530Ah y Hispa Ventanas pinta la pila en la barra
#    (los mismos puntos que tests/run_ventanas_bateria_tests.sh), también al
#    cambiar el nivel y enchufar con Ventanas en marcha; /E, /U, /AUTO y el
#    registro en un archivo. Y en un QEMU sin batería: /AUTO calla y no se
#    queda. Y que APAGAR sigue apagando con HSPACPI cargado.
# Deja capturas en build/acpi_*.png.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
B=build
T=$(mktemp -d)
CHK="python3 tests/tools/ventanas_check.py"
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
chk() { $CHK "$@" || fail=1; }

echo "== ACPI: el núcleo con las tablas del Toshiba NB250 y del HP Mini 5103 (controlador embebido simulado) =="
tablas() { for f in tests/acpi/$1/*.BIN; do printf -- '-t %s ' "$f"; done; }
# sim ETIQUETA EQUIPO ESCENARIO LECTURAS [opciones de acpi_sim]
sim() {
  local e=$1 m=$2 esc=$3 n=$4; shift 4
  timeout 120 $B/acpi_sim -n $n "$@" $(tablas $m) -e tests/acpi/$m/$esc > $T/$e.txt 2>&1
  echo $? > $T/$e.rc
}
# linea ETIQUETA "texto exacto de una línea" "qué se comprueba"
linea() { if grep -qxF -- "$2" $T/$1.txt; then ok "$3"; else ko "$3 (falta: $2)"; fi; }
tiene() { if grep -qF -- "$2" $T/$1.txt; then ok "$3"; else ko "$3 (falta: $2)"; fi; }
notiene() { if grep -qF -- "$2" $T/$1.txt; then ko "$3 (sobra: $2)"; else ok "$3"; fi; }
MIN="-o 38 -osi 0"          # el modo mínimo (lo que pide HSPACPI sin opciones)
COM="-osi 1"                # /COMPLETO

for modo in minimo completo; do
  [ $modo = minimo ] && O=$MIN || O=$COM
  sim t_$modo toshiba-nb250 escenario.txt 4 $O
  tiene t_$modo "ACPI: DSDT 0x" "Toshiba ($modo): carga su DSDT"
  tiene t_$modo "EC: \\_SB_.PCI0.LPC0.EC0_, datos en 62h, órdenes y estado en 66h" "Toshiba ($modo): encuentra el controlador embebido y sus puertos (_CRS)"
  tiene t_$modo "EC: _REG (3, 1): AE_OK" "Toshiba ($modo): _REG (3, 1)"
  tiene t_$modo '._BIF -> Package(13){1h, 1130h, 1130h, 1h, 2A30h,' "Toshiba ($modo): _BIF con lo del controlador (4400 mAh, 10800 mV)"
  tiene t_$modo '._BST -> Package(4){1h, 5DBh, C60h, 2CECh}' "Toshiba ($modo): _BST (descargando, 1499 mA, 3168 mAh, 11500 mV)"
  linea t_$modo "APM 1: fase=1 ca=00 estado=00 banderas=01 pct=72 min=126 nbat=1" "Toshiba ($modo): 72 %, sin cable, quedan 126 min"
  linea t_$modo "APM 3: fase=1 ca=01 estado=03 banderas=09 pct=75 min=0 nbat=1" "Toshiba ($modo): enchufado: 75 %, con cable, cargando"
  linea t_$modo "APM 4: fase=1 ca=01 estado=FF banderas=80 pct=255 min=0 nbat=0" "Toshiba ($modo): sin batería: no hay pila que pintar"
  tiene t_$modo "modelo=[] tipo=[Li-Ion] marca=[TOSHIBA]" "Toshiba ($modo): tipo y marca"
  tiene t_$modo "ACPICA: 0 avisos o errores" "Toshiba ($modo): ACPICA no se queja de nada"

  sim h_$modo hp-mini-5103 escenario.txt 5 $O
  tiene h_$modo "ACPI: DSDT 0x" "HP Mini ($modo): carga su DSDT (122 KB) y las SSDT"
  tiene h_$modo "EC: \\_SB_.PCI0.LPCB.EC0_, datos en 62h, órdenes y estado en 66h" "HP Mini ($modo): el controlador embebido"
  tiene h_$modo "EC: _REG (3, 1): AE_OK" "HP Mini ($modo): _REG (3, 1)"
  tiene h_$modo "Batería 1: \\_SB_.BAT0" "HP Mini ($modo): la batería \\_SB.BAT0"
  tiene h_$modo "Equipo de HP: antes de cada lectura se llama a" "HP Mini ($modo): se le pide releer (PWUP)"
  tiene h_$modo '._BIF -> Package(13){1h, 1450h, 12C0h, 1h, 2A30h,' "HP Mini ($modo): _BIF (5200 de fábrica, 4800 llena)"
  linea h_$modo "APM 1: fase=1 ca=00 estado=00 banderas=01 pct=50 min=120 nbat=1" "HP Mini ($modo): 50 %, sin cable, quedan 120 min"
  linea h_$modo "APM 3: fase=1 ca=00 estado=00 banderas=01 pct=45 min=108 nbat=1" "HP Mini ($modo): baja al 45 % sin aviso del controlador y se ve"
  linea h_$modo "APM 4: fase=1 ca=01 estado=03 banderas=09 pct=47 min=0 nbat=1" "HP Mini ($modo): enchufado: con cable, cargando"
  linea h_$modo "APM 5: fase=1 ca=01 estado=03 banderas=09 pct=56 min=0 nbat=1" "HP Mini ($modo): 56 %"
  tiene h_$modo "modelo=[Primary] tipo=[LIon] marca=[Hewlett-Packard]" "HP Mini ($modo): modelo, tipo y marca"
  tiene h_$modo "ACPICA: 0 avisos o errores" "HP Mini ($modo): ACPICA no se queja de nada"
done
# lo que distingue los dos modos en el HP: en el mínimo no se le cambia el
# tipo de sistema al controlador embebido (registro E3h) ni se toca el
# ventilador (D5h/D6h/D9h/DAh); en el completo sí, y se atienden los avisos
sim h_min_v hp-mini-5103 escenario.txt 2 -v $MIN
notiene h_min_v "EC[E3h] <-" "HP Mini (mínimo): no se escribe el tipo de sistema en el controlador embebido"
notiene h_min_v "EC[D5h] <-" "HP Mini (mínimo): no se toca el ventilador"
sim h_com_v hp-mini-5103 escenario.txt 2 -v $COM
tiene h_com_v "EC[E3h] <-" "HP Mini (completo): se escribe el tipo de sistema, como un sistema con ACPI"
notiene h_minimo "EC: aviso 03h" "HP Mini (mínimo): los avisos del controlador no se atienden"
tiene h_completo "EC: aviso 03h, _Q03: AE_OK" "HP Mini (completo): el aviso 03h ejecuta _Q03"
# /PSR en el HP: _PSR, al cambiar el cable, se pone con la tarjeta de red (y
# aquí, sin ella, se cansa de esperar): el nivel y el cable salen igual
sim h_psr hp-mini-5103 escenario.txt 4 -o 54 -osi 0
linea h_psr "APM 4: fase=1 ca=01 estado=03 banderas=09 pct=47 min=0 nbat=1" "HP Mini con /PSR: aunque _PSR falle, el cable sale de la batería"
tiene h_psr "Aborting method \\_SB.AC._PSR" "HP Mini con /PSR: el fallo de _PSR queda en el registro"
# el controlador embebido deja de contestar
sim t_mudo toshiba-nb250 mudo.txt 6 $MIN
[ "$(cat $T/t_mudo.rc)" = 0 ] && ok "Toshiba, controlador mudo: no se cuelga" || ko "Toshiba, controlador mudo: no se cuelga"
linea t_mudo "APM 1: fase=1 ca=00 estado=00 banderas=01 pct=72 min=126 nbat=1" "Toshiba, controlador mudo: antes, 72 %"
tiene t_mudo "EC: sin respuesta a la orden 80h" "Toshiba, controlador mudo: se apunta en el registro"
linea t_mudo "APM 4: fase=1 ca=FF estado=FF banderas=FF pct=255 min=0 nbat=1" "Toshiba, controlador mudo: al tercer fallo, nivel desconocido (no se inventa)"
linea t_mudo "APM 6: fase=1 ca=00 estado=00 banderas=01 pct=72 min=126 nbat=1" "Toshiba, controlador mudo: cuando vuelve, se recupera"
# un equipo sin ACPI
timeout 20 $B/acpi_sim > $T/nada.txt 2>&1
tiene nada "ACPI: el equipo no tiene ACPI" "sin ACPI: lo dice"
tiene nada "APM: fase=3" "sin ACPI: no hay batería (fase de error)"
[ -n "${VERBOSE:-}" ] && cat $T/t_minimo.txt $T/h_minimo.txt

echo "== ACPI: HSPACPI en QEMU con una batería de mentira (SSDT) hasta la pila de la barra =="
check() { if grep -qF -- "$1" $T/log.txt; then ok "$1"; else ko "$1"; fi; }
checkn() { if grep -qF -- "$1" $T/log.txt; then ko "(no debe salir) $1"; else ok "no sale: $1"; fi; }
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; sleep "${2:-0.3}"; }
shot() { sleep 0.4; mon "screendump $T/$1.ppm" 0.6; }
send() { printf '%s\r' "$1"; sleep "${2:-1.5}"; }
# un octeto de la CMOS desde fuera (con Hispa Ventanas en marcha no hay consola)
cmos() { mon "o /b 0x70 0x$1" 0.1; mon "o /b 0x71 0x$2" 0.2; }
muestra() { [ -f $T/$1.ppm ] && convert $T/$1.ppm $B/acpi_$2.png; }
# run "opciones de QEMU" 'órdenes'
run() {
  cp "$IMG" $T/f.img
  printf "DEVICE=A:\\\\HISPADOS\\\\HSPXMS.SYS\r\nFILES=30\r\n" > $T/CONFIG.SYS
  mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
  mcopy -o -i $T/f.img $B/HSPACPI.COM $B/HSPACPI.BIN $B/CMOSW.COM ::HISPADOS/
  rm -f $T/mon
  { sleep 6
    eval "$2"
    mon quit
  } | timeout ${3:-120} qemu-system-i386 -display none -serial stdio -monitor unix:$T/mon,server,nowait \
      -parallel none -m 32 -rtc base=2026-11-15T23:58:04 $1 \
      -drive file=$T/f.img,format=raw,if=floppy,index=0 -boot a -no-reboot \
      -audiodev none,id=snd0 -device sb16,audiodev=snd0 > $T/raw 2>&1
  tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 | grep -av '^audio:' > $T/log.txt
}
SSDT="-acpitable file=tests/acpi/bateria-qemu.aml"
# -- 1: consola. 85 % sin cable; /E; el registro en un archivo; enchufar; /U --
run "$SSDT" '
  send "A:\\HISPADOS\\CMOSW 60 55" 1
  send "A:\\HISPADOS\\CMOSW 61 02" 1
  send "A:\\HISPADOS\\HSPACPI /AUTO /REG:A:\\BATERIA.LOG" 6
  send "A:\\HISPADOS\\HSPACPI /E" 3
  send "A:\\HISPADOS\\CMOSW 61 03" 1
  send "A:\\HISPADOS\\CMOSW 60 3C" 1
  send "A:\\HISPADOS\\HSPACPI /E" 3
  send "A:\\HISPADOS\\HSPACPI" 3
  send "A:\\HISPADOS\\HSPACPI /U" 2
  send "A:\\HISPADOS\\HSPACPI /E" 2
  send "A:\\HISPADOS\\HSPACPI /COMPLETO /V" 7
  send "A:\\HISPADOS\\HSPACPI /U" 2
'
check "HSPACPI 0.1: la batería del portátil por ACPI"
check "Batería: 85 %, sin cable, quedan 255 min"
check '  \_SB_.HBAT: HISPA-PRUEBA LION Hispa-DOS 4250 de 5000 mAh (de fábrica, 5000), 11000 mV'
check "Batería: 60 %, con cable, cargando"
check "HSPACPI descargado."
check "HSPACPI no está cargado."
check "ACPI: se ejecutan los _INI del equipo..."
check "como Windows XP SP2"
check "Lectura 1: 60 % (3000 de 5000 mAh), con cable, cargando"
mcopy -n -i $T/f.img ::BATERIA.LOG $T/BATERIA.LOG 2> /dev/null
iconv -f CP437 -t UTF-8 $T/BATERIA.LOG > $T/bat.txt 2> /dev/null
for l in "ACPI: RSDP en 000F" "ACPI: DSDT 0x" "BATPRUEB" "ACPI: modo mínimo: no se ejecutan los _INI del equipo" \
         "Batería 1: \\_SB_.HBAT" "Adaptador de corriente: \\_SB_.HACA" \
         '\_SB_.HBAT._BST -> Package(4){1h, 3E8h, 109Ah, 2AF8h}' "Notify (\\_SB_.HBAT, 80h)" \
         "Lectura 1: 85 % (4250 de 5000 mAh), sin cable, descargando, quedan 255 min"; do
  if grep -qF -- "$l" $T/bat.txt; then ok "BATERIA.LOG: $l"; else ko "BATERIA.LOG: $l"; fi
done
[ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ] && { sed -n '/Ejecutando/,$p' $T/log.txt; cat $T/bat.txt 2> /dev/null; }

# -- 2: Hispa Ventanas. La pila sale por HSPACPI (sin /BAT) y cambia sola,
#       con Ventanas en marcha (lectura cada 2 s; la barra mira cada 5) --
run "$SSDT" '
  send "A:\\HISPADOS\\CMOSW 60 55" 1
  send "A:\\HISPADOS\\CMOSW 61 02" 1
  send "A:\\HISPADOS\\HSPACPI /CADA:2" 6
  send "A:\\HISPADOS\\HSPDPMI" 2
  send "A:\\HISPADOS\\HSPRATON" 2
  send "A:\\HISPADOS\\HSPSND" 3
  send "VENTANAS" 8
  shot w1
  cmos 60 0F
  sleep 9
  shot w2
  cmos 60 64
  cmos 61 03
  sleep 9
  shot w3
'
check "Batería: 85 %, sin cable, quedan 255 min"
chk $T/w1.ppm tam 800 600 \
  px 582 12 8 "altavoz encendido (crema), corrido a la izquierda" \
  px 608 12 8 "marco crema de la pila (lado izquierdo)" \
  px 614 12 11 "relleno verde al 85 % (leído por ACPI)" \
  texto 636 5 8 1 fina "85%" \
  texto 676 5 8 1 fina "23:58" \
  texto 736 5 8 4 fina "Salir"
muestra w1 85
chk $T/w2.ppm px 610 12 4 "relleno rojo (terracota) al 15 %" px 608 12 8 "marco crema" \
  texto 636 5 8 1 fina "15%"
muestra w2 15
chk $T/w3.ppm px 614 12 11 "relleno verde al 100 %" px 618 13 9 "rayo blanco (con cable)" \
  texto 636 5 8 1 fina "100%"
muestra w3 100c
[ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ] && sed -n '/Ejecutando/,$p' $T/log.txt

# -- 3: un QEMU sin batería (el sobremesa): /AUTO calla y no se queda --
run "" '
  send "A:\\HISPADOS\\HSPACPI /AUTO" 5
  send "A:\\HISPADOS\\HSPACPI /E" 2
  send "A:\\HISPADOS\\HSPACPI" 5
' 60
n=$(grep -cF "HSPACPI 0.1: la batería del portátil por ACPI" $T/log.txt)
[ "$n" = 2 ] && ok "sin batería, HSPACPI /AUTO no dice nada" || ko "sin batería, HSPACPI /AUTO no dice nada ($n títulos, deben ser 2)"
check "HSPACPI no está cargado."
check "Este equipo no declara ninguna batería por ACPI: HSPACPI no se queda."
[ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ] && sed -n '/Ejecutando/,$p' $T/log.txt

# -- 4: con HSPACPI cargado (contesta «hay APM» a INT 15h AX=5300h), APAGAR
#       sigue apagando por ACPI por su cuenta --
run "$SSDT" '
  send "A:\\HISPADOS\\CMOSW 61 02" 1
  send "A:\\HISPADOS\\HSPACPI" 6
  send "APAGAR" 6
' 60
check "Batería: 0 %, sin cable"
grep -q "terminating on signal" $T/raw && ko "APAGAR con HSPACPI cargado apaga el equipo" || ok "APAGAR con HSPACPI cargado apaga el equipo"
[ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ] && sed -n '/Ejecutando/,$p' $T/log.txt
rm -rf $T
exit $fail
