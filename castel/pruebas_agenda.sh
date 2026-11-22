# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# pruebas_agenda.sh - (AGENDA) la parte 43 de pruebas.sh (SOLO=agenda), que
# lee este archivo («.»): la función parte_agenda, con los ayudantes de
# pruebas.sh (ok, ko, mira, miracap) y sus carpetas ($AQUI, $P, $G).
#   En el anfitrión (corre32, deprisa): las repeticiones frente a
#   python-dateutil (si está; si no, los casos escritos a mano), el cambio de
#   hora (Europe/Madrid: la hora que no existe, la que pasa dos veces, los
#   avisos de esa noche), un .ICS de Google (zonas, UTC, RRULE, EXDATE,
#   RECURRENCE-ID, VALARM, VTODO, cumpleaños de antes de 1970, renglones
#   doblados, UTF-8) importado y exportado, ida y vuelta igual; los
#   cumpleaños de una vCard; los festivos (2026 y 2027 del BOE y el DOGC,
#   Albinyana); y el fuzz del lector .ICS (FUZZ_AGENDA vueltas; 400 de serie).
#   En QEMU (el escritorio a 800 x 600): el vigilante arranca con el
#   escritorio; la Agenda desde su icono; importar el .ICS (la repetición
#   semanal, en el mes); una cita nueva con su aviso y su repetición; el
#   aviso, con el reloj del PC puesto justo antes (su ventana, Posponer 5 min);
#   la semana, los próximos y las tareas; el Reloj abre la Agenda con dos
#   clics en un día; el reloj de la bandeja dice la próxima cita; y lo que
#   queda en el disco (AGENDA.ICS, VISTO.TXT con lo pospuesto, PROXIMA.TXT).
#   Las capturas, a docs/pentium/agenda/ con CAPTURAS_DOCS=1.

parte_agenda() {
echo "== Hispa-Pentium: la Agenda (AGENDA.BIN) =="
local Y=$P/agenda S R C h
rm -rf $Y; mkdir -p $Y/c $G
h=$AQUI/nucleo32/agenda/pruebas
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/uagenda.cas -o $Y/a.asm > /dev/null && nasm -f bin $Y/a.asm -o $Y/AGENDA.BIN || { ko "uagenda.cas no compila"; return; }
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/ureloj.cas -o $Y/r.asm > /dev/null && nasm -f bin $Y/r.asm -o $Y/RELOJ.BIN || { ko "ureloj.cas (con festivos.cas) no compila"; return; }
C=$AQUI/corre32/corre32.sh
R="$C -C $Y/c $Y/AGENDA.BIN /CARPETA=\\AG"
a437() { python3 -I -c "import sys; sys.path.insert(0, '$AQUI/../tools'); import hispa437; sys.stdout.write(''.join(hispa437.letra(b) for b in sys.stdin.buffer.read() if b != 13))"; }
# 1. las repeticiones
if python3 -I $h/repite.py $Y/AGENDA.BIN $C ${REPITE_AGENDA:-1500} > $Y/repite.txt 2>&1; then
  ok "las repeticiones (RRULE): $(tail -n 1 $Y/repite.txt) — diaria, semanal con días y WKST, mensual por el día (y el último) o «el segundo martes», anual, BYSETPOS, BYYEARDAY, hasta un día o N veces, y saltando sin contar"
else ko "las repeticiones:"; head -12 $Y/repite.txt | sed 's/^/        /'; fi
SIN_DATEUTIL=1 python3 -I $h/repite.py $Y/AGENDA.BIN $C > $Y/repite2.txt 2>&1 && ok "y los casos escritos a mano (sin dateutil): $(tail -n 1 $Y/repite2.txt)" || { ko "los casos escritos a mano:"; head -8 $Y/repite2.txt | sed 's/^/        /'; }
# 2. el cambio de hora
{ for a in "/UTC 29/03/2026 01:59" "/UTC 29/03/2026 02:30" "/UTC 29/03/2026 03:00" "/UTC 25/10/2026 02:30" "/UTC 25/10/2026 03:00" "/MADRID 25/10/2026 00:30" "/MADRID 25/10/2026 01:30" "/MADRID 29/03/2026 01:00"; do echo "$a"; $R $a; done; } > $Y/hora.txt 2>&1
S=$Y/hora.txt
mira "la hora de Madrid y UTC: el invierno (+1), el verano (+2), las 2:30 que no existen (= las 3:30) y las 2:30 que pasan dos veces (la primera)" \
  "^agenda: 29/03/2026 00:59$" "^agenda: 29/03/2026 01:30$" "^agenda: 29/03/2026 01:00$" "^agenda: 25/10/2026 00:30$" "^agenda: 25/10/2026 02:00$" "^agenda: 29/03/2026 03:00$"
grep -c '^agenda: 25/10/2026 02:30$' $Y/hora.txt | grep -q '^2$' && ok "y de UTC a Madrid, las dos 2:30 de octubre (00:30 y 01:30 UTC)" || ko "las dos 2:30 de octubre: $(cat $Y/hora.txt | tr '\n' ' ')"
$R /NUEVA 29/03/2026 03:00 60 60 - Cambio de hora > /dev/null 2>&1
$R /NUEVA 23/03/2026 10:00 60 0 'FREQ=WEEKLY;COUNT=3' Los lunes a las diez > /dev/null 2>&1
$R /AVISOS 28/03/2026 23:00 29/03/2026 05:00 2>&1 | a437 > $Y/avisos.txt
$R /LISTA 23/03/2026 06/04/2026 2>&1 | a437 >> $Y/avisos.txt
S=$Y/avisos.txt
mira "los avisos la noche del cambio (una hora antes de las 3:00 de verano: la 1:00 de invierno) y una cita semanal que sigue a las 10:00 después del cambio" \
  "^agenda: aviso 29/03/2026 01:00 Cambio de hora" "^agenda: 23/03/2026 10:00 60 Los lunes a las diez" "^agenda: 30/03/2026 10:00 60 Los lunes a las diez" "^agenda: 06/04/2026 10:00 60 Los lunes"
# 3. el .ICS de Google: importar, la lista, exportar y otra vez (ida y vuelta)
cp $h/google.ics $Y/c/G.ICS; cp $h/libreta.vcf $Y/c/L.VCF
R="$C -C $Y/c $Y/AGENDA.BIN /CARPETA=\\G1"
{ $R /IMPORTA G.ICS; $R /LISTA 01/11/2026 31/12/2026; $R /LISTA 29/03/2026; $R /LISTA 15/03/2027; $R /TAREAS; $R /AVISOS 01/12/2026 00:00 02/12/2026 00:00; } 2>&1 | a437 > $Y/ics.txt
S=$Y/ics.txt
mira "un .ICS de Google: la cita semanal (COUNT, EXDATE y la vez cambiada a las 12, RECURRENCE-ID), la de UTC, la de Nueva York (su VTIMEZONE), la de tres días, la hora que no existe, el cumpleaños de 1952, la tarea, el aviso (VALARM) y el UTF-8 (\\, \\n, € y la ç catalana)" \
  "^agenda: importadas 8, cambiadas 0, saltadas 0" "^agenda: 03/11/2026 10:00 90 Taller de informàtica, 1r grup | Casal d'Avis, Albinyana | Asociación | 30$" \
  "^agenda: 24/11/2026 12:00 90 Taller de informàtica (cambiado a las 12)" "^agenda: 29/12/2026 10:00 90 Taller" "^agenda: 01/12/2026 17:30 60 Médico de cabecera | CAP El Vendrell" \
  "^agenda: 10/12/2026 15:00 60 Videollamada con Nueva York" "^agenda: 08/12/2026 todo 4320 Excursión a Montserrat" "^agenda: 29/03/2026 03:30 60 A una hora que no existe" \
  "^agenda: 15/03/2027 todo 1440 Cumpleaños de Pepa |  | Cumpleaños" "^agenda: tarea \[ \] 30/11/2026 Pagar la cuota" "^agenda: aviso 01/12/2026 09:30 Taller" "^agenda: aviso 01/12/2026 16:30 Médico"
grep -q "^agenda: 17/11/2026" $Y/ics.txt && ko "el 17/11 (EXDATE) sale" || ok "el 17/11 (EXDATE) y el 24/11 a las 10 (la vez cambiada) no salen"
$R /EXPORTA E1.ICS > /dev/null 2>&1
R="$C -C $Y/c $Y/AGENDA.BIN /CARPETA=\\G2"
$R /IMPORTA E1.ICS > /dev/null 2>&1; $R /EXPORTA E2.ICS > /dev/null 2>&1
if diff <(grep -v '^DTSTAMP' $Y/c/E1.ICS) <(grep -v '^DTSTAMP' $Y/c/E2.ICS) > $Y/diff.txt && [ -s $Y/c/E1.ICS ]; then ok "ida y vuelta: lo exportado, importado y exportado otra vez, sale igual ($(grep -c BEGIN:VEVENT $Y/c/E1.ICS) VEVENT, $(grep -c BEGIN:VTODO $Y/c/E1.ICS) VTODO)"
else ko "ida y vuelta: no sale igual"; head -10 $Y/diff.txt | sed 's/^/        /'; fi
if python3 -I - $Y/c/E1.ICS > $Y/rfc.txt 2>&1 <<'PY'
import sys
d = open(sys.argv[1], 'rb').read()
assert d.startswith(b'BEGIN:VCALENDAR\r\n') and d.endswith(b'END:VCALENDAR\r\n'), 'principio o final'
for l in d.split(b'\r\n'):
    assert len(l) <= 75, 'renglón de %d octetos' % len(l)
    assert b'\n' not in l and b'\r' not in l
d.decode('utf-8')
assert b'TZID:Europe/Madrid' in d and b'RRULE:FREQ=WEEKLY;COUNT=10;BYDAY=TU' in d and b'EXDATE;TZID=Europe/Madrid:20261117T100000' in d
assert b'DTSTART;VALUE=DATE:19520315' in d and b'TRIGGER:-PT30M' in d and b'SUMMARY:Taller de inform\xc3\xa0tica\\, 1r grup' in d
print('bien')
PY
then ok "lo exportado cumple RFC 5545: CR LF, renglones de 75 octetos como mucho, UTF-8, la VTIMEZONE de Madrid, RRULE, EXDATE con su zona, VALARM y el cumpleaños con su año"
else ko "lo exportado: $(tail -n 1 $Y/rfc.txt)"; fi
if python3 -c "import icalendar" 2> /dev/null; then
  python3 -c "import icalendar,sys; c=icalendar.Calendar.from_ical(open('$Y/c/E1.ICS','rb').read()); print(len(c.walk('VEVENT')))" > $Y/ical.txt 2>&1 && ok "y python-icalendar lo lee ($(cat $Y/ical.txt) VEVENT)" || ko "python-icalendar no lo lee: $(tail -n 1 $Y/ical.txt)"
fi
R="$C -C $Y/c $Y/AGENDA.BIN /CARPETA=\\G3"
{ $R /IMPORTA L.VCF; $R /IMPORTA L.VCF; $R /LISTA 16/07/2027; $R /LISTA 29/02/2028; $R /LISTA 03/05/2027; } 2>&1 | a437 > $Y/vcf.txt
S=$Y/vcf.txt
mira "los cumpleaños de una libreta (vCard: 1948-07-16, --0229 y 20150503), una vez (importar dos veces no los duplica)" \
  "^agenda: importadas 3, cambiadas 0" "^agenda: importadas 0, cambiadas 3" "^agenda: 16/07/2027 todo 1440 Cumpleaños de Josep Ferré" "^agenda: 29/02/2028 todo 1440 Cumpleaños de Maria Àngels" "^agenda: 03/05/2027 todo 1440 Cumpleaños de Nieta Laia"
# 4. los festivos: la misma tabla en el Reloj y en la Agenda
{ $C $Y/RELOJ.BIN /FESTIVOS 2026; $C $Y/RELOJ.BIN /FESTIVOS 2027; $R /FESTIVOS 2026 0; $R /FESTIVOS 2026 13; } 2>&1 | a437 > $Y/fest.txt
S=$Y/fest.txt
mira "los festivos (festivos.cas, la misma tabla para el Reloj y la Agenda): Cataluña 2026 y 2027 como el DOGC (Lunes de Pascua, Sant Joan, la Diada, Sant Esteve; sin el 26/12 de 2027, domingo), los locales de Albinyana (16/7 y 24/8) y otra comunidad (Madrid: Jueves Santo y el 2 de mayo)" \
  "^reloj: Lunes, 6 de abril de 2026: Lunes de Pascua$" "^reloj: Miércoles, 24 de junio de 2026: Sant Joan$" "^reloj: Viernes, 11 de septiembre de 2026: Diada Nacional de Catalunya$" \
  "^reloj: Sábado, 26 de diciembre de 2026: Sant Esteve$" "^reloj: Jueves, 16 de julio de 2026: Mare de Déu del Carme (fiesta local)$" "^reloj: Lunes, 24 de agosto de 2026: Sant Bartomeu (fiesta local)$" \
  "^reloj: Lunes, 29 de marzo de 2027: Lunes de Pascua$" "^reloj: Viernes, 26 de marzo de 2027: Viernes Santo$" "^agenda: festivo 02/04/2026 Jueves Santo$" "^agenda: festivo 02/05/2026 Fiesta de la Comunidad de Madrid$" "^agenda: festivo 02/11/2026 Lunes siguiente a Todos los Santos$"
grep -q "26 de diciembre de 2027" $Y/fest.txt && ko "sale el 26/12/2027 (domingo: no es festivo en Cataluña ese año)" || ok "y el 26/12/2027 no sale (en domingo: el DOGC no lo pone)"
grep -q "^agenda: festivo 02/04/2026" <($R /FESTIVOS 2026 0 2>&1) && ko "con solo los nacionales sale el Jueves Santo" || ok "con «solo los nacionales», sin los de la comunidad"
# 5. el fuzz del lector .ICS
if python3 -I $h/fuzz.py $Y/AGENDA.BIN $C ${FUZZ_AGENDA:-400} > $Y/fuzz.txt 2>&1; then ok "el fuzz del lector .ICS: $(tail -n 1 $Y/fuzz.txt)"
else ko "el fuzz del lector .ICS:"; tail -n 6 $Y/fuzz.txt | sed 's/^/        /'; fi
# 6. en QEMU
parte_agenda_qemu
}

# la parte de QEMU: el escritorio con la Agenda y el Reloj en la raíz del disco
# (sin instalar: el escritorio lanza \AGENDA.BIN /VIGILAR igual) y CITAS.ICS
# en \DOCUMENT; el reloj del PC, el martes 24/11/2026 a las 9:23:30
parte_agenda_qemu() {
local Y=$P/agenda S Q=$P/agenda/q
export MTOOLS_SKIP_CHECK=1
mkdir -p $Q/menu
if ! $AQUI/nucleo32/disco.sh $Q/d.img 32 > $Q/disco.txt 2>&1; then ko "no se puede hacer el disco de prueba: $(tail -n 2 $Q/disco.txt)"; return; fi
python3 -I $AQUI/nucleo32/accesorios.py $Q/acc --raiz --escritorio && cp $Q/acc/RELOJ.MNU $Q/acc/RELOJ.ICP $Q/menu/
python3 -I $AQUI/nucleo32/agenda/icono.py $Q/menu/AGENDA.ICP
printf 'nombre=Agenda\r\nprograma=\\AGENDA.BIN\r\nicono=\\MENU\\AGENDA.ICP\r\ntipo=Oficina\r\nescritorio=si\r\n' > $Q/menu/AGENDA.MNU
mmd -i $Q/d.img ::MENU ::DOCUMENT && mcopy -i $Q/d.img $Q/menu/* ::MENU/ && mcopy -i $Q/d.img $Y/AGENDA.BIN $Y/RELOJ.BIN :: \
  && mcopy -i $Q/d.img $AQUI/nucleo32/agenda/pruebas/google.ics ::DOCUMENT/CITAS.ICS || { ko "no se puede preparar el disco"; return; }
# el icono de la Agenda en (63, 258) y el del Reloj en (63, 334); el menú
# Archivo en (60, 45) e «Importar .ICS» en (100, 158)
CAPTURAS=$G ESCRIBE=1 DISCO=$Q/d.img ARRANQUE=escritorio MODULOS= RELOJ=2026-11-24T09:23:30 $AQUI/nucleo32/probar.sh "" \
  ":pausa 4" ":captura ag0" ":doble 63 258" ":pausa 4" ":captura ag1" \
  ":clic 60 45" ":pausa 1" ":clic 100 158" ":pausa 1" ":captura ag2" ":tecla ret" ":pausa 2" ":captura ag3" \
  ":tecla ctrl-n" ":pausa 1" ":escribe Reunión de la junta" ":tecla tab" ":escribe Casal d'Avis" ":tecla tab" ":tecla tab" \
  ":tecla backspace backspace backspace backspace backspace" ":escribe 09:40" ":tecla tab" ":tecla tab" ":tecla tab" ":tecla tab" ":tecla tab" \
  ":tecla down" ":tecla down" ":pausa 1" ":captura ag4" ":tecla tab" ":tecla ret" ":pausa 2" ":captura ag5" \
  ":hasta 150 1 AVISO Reuni" ":pausa 3" ":captura ag6" ":tecla shift-tab" ":tecla shift-tab" ":tecla shift-tab" ":tecla ret" ":pausa 2" \
  ":clic 400 16" ":tecla 2" ":pausa 1" ":captura ag7" ":tecla 4" ":pausa 1" ":captura ag8" ":tecla 5" ":pausa 1" ":captura ag9" \
  ":tecla 3" ":pausa 1" ":captura ag10" ":clic 778 16" ":pausa 2" \
  ":doble 63 334" ":pausa 4" ":captura ag11" ":doble ${AG_DIA_X:-570} ${AG_DIA_Y:-385}" ":pausa 4" ":captura ag12" \
  ":raton 780 588" ":pausa 2" ":raton 781 588" ":pausa 2" ":captura ag13" > $Q/e.sal 2>&1
tr -d '\r' < $Q/e.sal > $Q/e.txt
S=$Q/e.txt
mira "el escritorio lanza el vigilante de los avisos (AGENDA /VIGILAR) y el icono abre la Agenda" "agenda: el vigilante de los avisos, en marcha" "escritorio: lanzo .AGENDA.BIN (la tarea [0-9]*)" "^agenda: Agenda 1.0"
miracap "la Agenda: el mes de noviembre de lunes a domingo, hoy con HOY, el mini calendario y la leyenda de las categorías (símbolo y nombre)" ag1 \
  negrita15 230 100 500 130 "Noviembre de 2026" negrita 300 395 400 425 HOY texto 0 310 120 340 Asociaci texto 100 290 230 320 Trabajo
mira "importar el .ICS (Archivo > Importar): 8 citas" "^agenda: importadas 8, cambiadas 0, saltadas 0"
miracap "la cita semanal importada, los martes del mes (con el símbolo de su categoría) y la cambiada a las 12" ag3 \
  texto 315 230 395 260 Taller texto 315 290 395 320 Taller texto 315 420 395 450 Taller texto 235 480 315 510 "Pag" negrita 235 460 400 490 "+1"
miracap "la ficha de una cita nueva: título, lugar, el día, la hora, el aviso y «Cada semana» con sus días" ag4 \
  texto15 30 90 400 130 "Reuni" texto15 400 90 780 130 "15 minutos antes" texto15 400 220 780 260 "Cada semana"
mira "la cita se guarda y el vigilante vuelve a leer la agenda (AGENDA.GEN)" "^agenda: guardada Reunión de la junta" "^agenda: vuelvo a leer la agenda"
mira "el aviso salta a su hora (15 minutos antes de las 9:40, con la Agenda abierta o cerrada) y se pospone 5 minutos" "^agenda: AVISO Reunión de la junta" "^agenda: pospuesto 5 minutos: Reunión de la junta"
miracap "la ventanita del aviso: la palabra AVISO, la cita, su hora y su lugar, y Posponer 5 min, 10 min, 1 hora, Hecho y Abrir la Agenda" ag6 \
  negrita15 80 90 720 130 "AVISO" negrita2 80 130 720 180 "Reuni" texto15 80 210 720 260 "Casal" negrita15 80 400 720 500 "Hecho" negrita15 80 400 720 500 "Posponer 5 min"
miracap "la semana (de lunes a domingo, hoy con HOY)" ag7 negrita15 230 100 780 130 "Semana del 23 al 29" negrita 300 150 420 200 HOY texto 300 170 420 260 Taller
miracap "los próximos 60 días (con los festivos)" ag8 texto15 0 100 790 520 "Festivo" negrita15 0 100 790 520 "Reuni"
miracap "las tareas, con su casilla" ag9 negrita15 0 100 790 520 "Pagar la cuota"
mira "el Reloj: dos clics en un día del calendario abren la Agenda en ese día" "^reloj: abro la Agenda /DIA 26/11/2026" "escritorio\|^agenda: Agenda 1.0"
miracap "la Agenda abierta desde el Reloj, en la vista del día" ag12 negrita15 230 90 790 140 "26 de noviembre de 2026"
miracap "el reloj de la bandeja, con el ratón encima: la próxima cita" ag13 texto 200 480 800 570 "xima cita: hoy" texto 200 480 800 570 "Reuni"
if ! grep -q "excepción\|\*\*\*" $Q/e.sal; then ok "y ni una excepción del núcleo"
else ko "el núcleo se ha quejado:"; grep "excepción\|\*\*\*" $Q/e.sal | head -3 | sed 's/^/        /'; fi
# lo que queda en el disco
rm -f $Q/AGENDA.ICS $Q/VISTO.TXT $Q/PROXIMA.TXT
mcopy -n -i $Q/d.img ::DOCUMENT/AGENDA/AGENDA.ICS ::DOCUMENT/AGENDA/VISTO.TXT ::DOCUMENT/AGENDA/PROXIMA.TXT $Q/ > /dev/null 2>&1
if grep -q "SUMMARY:Reuni" $Q/AGENDA.ICS 2> /dev/null && grep -q "RRULE:FREQ=WEEKLY;BYDAY=TU" $Q/AGENDA.ICS && grep -q "^pospuesto=" $Q/VISTO.TXT 2> /dev/null && [ -s $Q/PROXIMA.TXT ]; then
  ok "en el disco: \\DOCUMENT\\AGENDA\\AGENDA.ICS (con la cita nueva y su RRULE), VISTO.TXT (con lo pospuesto) y PROXIMA.TXT (para la bandeja)"
else ko "en el disco falta algo: $(ls $Q/*.ICS $Q/*.TXT 2>&1 | tr '\n' ' ')"; fi
fsck.fat -n $Q/d.img > $Q/fsck.txt 2>&1 && ok "y el disco, sano (fsck.fat)" || ko "el disco: $(tail -n 2 $Q/fsck.txt)"
# otra vez, con el equipo «apagado» hasta el martes siguiente por la tarde:
# lo que se pasó (los avisos de la Reunión, el Taller y el Médico del 1/12),
# en su lista; y lo pospuesto, que salta
CAPTURAS=$G ESCRIBE=1 DISCO=$Q/d.img ARRANQUE=escritorio MODULOS= RELOJ=2026-12-01T19:00:00 $AQUI/nucleo32/probar.sh "" \
  ":pausa 6" ":captura ag14" > $Q/e2.sal 2>&1
tr -d '\r' < $Q/e2.sal > $Q/e2.txt
S=$Q/e2.txt
mira "al arrancar más tarde: los avisos que se pasaron con el equipo apagado, en una lista (y lo pospuesto, que salta)" \
  "agenda: con el equipo apagado se pasaron 3 avisos" "agenda: AVISO pospuesto Reunión de la junta"
miracap "la lista de lo que se pasó con el equipo apagado" ag14 negrita15 0 0 800 600 "mientras el equipo estaba apagado" texto15 0 0 800 600 "Taller" texto15 0 0 800 600 "dico de cabecera"
# las capturas, a docs/pentium/agenda/ (con CAPTURAS_DOCS=1)
if [ -n "${CAPTURAS_DOCS:-}" ] && command -v pnmtopng > /dev/null; then
  mkdir -p $AQUI/../docs/pentium/agenda
  for c in ag1 ag3 ag4 ag6 ag7 ag8 ag9 ag11 ag12 ag13 ag14; do [ -f $G/$c.ppm ] && pnmtopng $G/$c.ppm > $AQUI/../docs/pentium/agenda/$c.png 2> /dev/null; done
fi
}
