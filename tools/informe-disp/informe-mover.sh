#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# informe-mover.sh - (INFORME-DISP) los informes de los dispositivos que fallan
# que han llegado a informe.php (como el usuario hispatermo, en
# /www/hispados-subidas/informes/<AAAAMMDD-HHMMSS-XXXX>_<fabricante-modelo>.TXT)
# pasan al FTP:
#   /root/FTP/HispaDOS/Pruebas/Pendientes/Informes-dispositivos/<AAAAMMDD-HHMMSS>-<fabricante-modelo>/INFORME.TXT
# de root y 0644 (si ya hay una carpeta con ese nombre, la del número entero).
# Solo lo que lleva quieto más de 1 minuto. También borra las cuentas de los
# días pasados (informes-cuotas, de hace más de 3 días) y rota el registro
# (registro/informes.log, a partir de 4 MB: se guarda uno, .1).
# Lo pone en marcha cron (root), cada 5 min:
#   */5 * * * * root /root/hispados-tareas/informe-mover.sh >> /root/hispados-tareas/informe-mover.log 2>&1
# Con argumentos (pruebas): DATOS y DESTINO en vez de los de verdad.
set -eu
DATOS=${1:-/www/hispados-subidas}
DESTINO=${2:-/root/FTP/HispaDOS/Pruebas/Pendientes}/Informes-dispositivos
ORIGEN=$DATOS/informes
[ -d "$ORIGEN" ] || exit 0
# los nombres ya vienen hechos por informe.php; aquí se vuelven a mirar (nada
# de «..», ni enlaces, ni lo que no sea un archivo normal ni de ese patrón)
find "$ORIGEN" -mindepth 1 -maxdepth 1 -type f -mmin +1 ! -name '.*' -print0 |
while IFS= read -r -d '' f; do
  nombre=${f##*/}
  if ! [[ $nombre =~ ^(20[0-9]{6}-[0-9]{6})-([0-9A-F]{4})_([A-Za-z0-9_-]{1,40})\.TXT$ ]]; then
    echo "$(date -Is) se deja (nombre raro): $nombre"
    continue
  fi
  fecha=${BASH_REMATCH[1]}; azar=${BASH_REMATCH[2]}; equipo=${BASH_REMATCH[3]}
  d="$DESTINO/$fecha-$equipo"
  [ -e "$d" ] && d="$DESTINO/$fecha-$azar-$equipo"
  if [ -e "$d" ]; then
    echo "$(date -Is) se deja (ya está): $nombre"
    continue
  fi
  mkdir -p "$d"
  # (copiar y borrar, no mover: así queda de root y con sus permisos)
  install -o root -g root -m 0644 "$f" "$d/INFORME.TXT"
  rm -f -- "$f"
  echo "$(date -Is) ${d##*/}/INFORME.TXT"
done
# las cuentas de los días pasados, y los .tmp a medias que se hayan quedado
find "$DATOS/informes-cuotas" -mindepth 1 -maxdepth 1 -type f -mtime +3 -delete 2> /dev/null || true
find "$ORIGEN" -mindepth 1 -maxdepth 1 -type f -name '.*' -mmin +60 -delete || true
# el registro, rotado
R=$DATOS/registro/informes.log
if [ -f "$R" ] && [ "$(stat -c %s "$R")" -gt 4000000 ]; then
  mv -f -- "$R" "$R.1"
fi
exit 0
