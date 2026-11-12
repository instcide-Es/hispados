#!/bin/bash
# Fecha y hora de verdad: INT 21h 2Bh/2Dh (FECHA y HORA del intérprete)
# cambian la fecha del núcleo, los tics de la BIOS y el reloj de tiempo real
# (tras reiniciar sigue la fecha puesta); al pasar la medianoche la fecha
# avanza; las fechas imposibles se rechazan; los archivos nuevos llevan la
# fecha y la hora puestas.
# Real date and time: 2Bh/2Dh set the kernel date, the BIOS ticks and the RTC.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
cp "$IMG" $T/f.img
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; }
# guion: una orden por línea; "@N" espera N segundos; "!orden" va al monitor
{ sleep 5
  while IFS= read -r c; do
    case "$c" in
      @*) sleep "${c#@}";;
      !*) mon "${c#!}";;
      *) printf '%s\r' "$c";;
    esac
    sleep 0.7
  done <<'EOF'
ECHO uno
FECHA
HORA
FECHA 30-02-2024
FECHA 29-02-2023
FECHA 31-04-2026
FECHA 01-01-1979
HORA 24:00
HORA 12:60
ECHO dos
FECHA 29-02-2024
FECHA
HORA 9:08:07
HORA
ECHO hola > RELOJ.TXT
DIR RELOJ.TXT
ECHO tres
FECHA 31-12-2027
HORA 23:59:57
@5
FECHA
HORA
ECHO cuatro
HORA 23:59:58
@3
HORA 23:59:58
@3
FECHA
ECHO cinco
FECHA 15-08-2031
HORA 6:07:08
!system_reset
@7
FECHA
HORA
ECHO fin
EOF
  sleep 2
  mon quit
} | timeout 90 qemu-system-i386 -display none -serial stdio -monitor unix:$T/mon,server,nowait -parallel none \
      -rtc base=2026-03-10T10:20:30 -drive file=$T/f.img,format=raw,if=floppy -boot a > $T/raw 2>&1
tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 > $T/log
sec() { sed -n "/^$1\$/,/^$2\$/p" $T/log; }
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
has() { if sec "$1" "$2" | grep -qE -- "$3"; then ok "$4"; else ko "$4"; fi; }
echo "== Hispa-DOS: fecha y hora (2Ah-2Dh, reloj de tiempo real, medianoche) =="
has uno dos "La fecha actual es Mar 10-03-2026" "al arrancar, la fecha del reloj de tiempo real (Mar 10-03-2026)"
has uno dos "La hora actual es 10:2[0-9]:[0-9][0-9]" "y su hora (10:2x)"
n=$(sec uno dos | grep -c "Fecha no válida")
if [ "$n" = 4 ]; then ok "fechas imposibles rechazadas (30-02-2024, 29-02-2023, 31-04-2026, 1979)"; else ko "fechas imposibles rechazadas: $n de 4"; fi
n=$(sec uno dos | grep -c "Hora no válida")
if [ "$n" = 2 ]; then ok "horas imposibles rechazadas (24:00, 12:60)"; else ko "horas imposibles rechazadas: $n de 2"; fi
has dos tres "La fecha actual es Jue 29-02-2024" "FECHA 29-02-2024 -> Jue 29-02-2024"
has dos tres "La hora actual es  9:08:(0[7-9]|10)" "HORA 9:08:07 -> 9:08:07"
has dos tres "RELOJ +TXT.* 29-02-24 +9:08" "el archivo creado después lleva esa fecha y esa hora"
has tres cuatro "La fecha actual es Sáb 01-01-2028" "medianoche: del 31-12-2027 23:59:57 al Sáb 01-01-2028"
has tres cuatro "La hora actual es  0:00:0[0-9]" "y la hora vuelve a 0:00"
has cuatro cinco "La fecha actual es Lun 03-01-2028" "dos medianoches más (hora puesta dos veces a 23:59:58): Lun 03-01-2028"
sec cinco fin | sed -n '/system_reset/,$p;/Hispa-DOS 0/,$p' > $T/tras
if grep -q "La fecha actual es Vie 15-08-2031" $T/tras; then ok "tras reiniciar, el reloj de tiempo real da la fecha puesta (Vie 15-08-2031)"; else ko "tras reiniciar, la fecha puesta (Vie 15-08-2031)"; fi
if grep -qE "La hora actual es  6:07:[1-5][0-9]" $T/tras; then ok "y la hora puesta sigue corriendo (6:07)"; else ko "y la hora puesta (6:07)"; fi
check=$(grep -c "^fin$" $T/log)
[ "$check" -ge 1 ] && ok "fin" || ko "fin"
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log; fi
rm -rf $T
[ $fail = 0 ] && echo "TODO OK" || echo "HAY FALLOS"
exit $fail
