#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# termo-mover.sh - (TERMO) lo que ha llegado por «Enviar a Carlos» (subir.php,
# como el usuario hispatermo, en /www/hispados-subidas/entrantes/<EQUIPO>/<lote>/)
# pasa al FTP: /root/FTP/HispaDOS/Pruebas/Pendientes/<EQUIPO>/Termometro/subidas/<lote>/,
# de root y 0644. Solo lo que lleva quieto más de 2 minutos (un lote a medias
# se acaba de pasar la vez siguiente). Lo pone en marcha cron (root), cada 5 min:
#   */5 * * * * root /root/hispados-tareas/termo-mover.sh >> /root/hispados-tareas/termo-mover.log 2>&1
# Con un argumento (pruebas): ORIGEN y DESTINO en vez de los de verdad.
set -eu
ORIGEN=${1:-/www/hispados-subidas/entrantes}
DESTINO=${2:-/root/FTP/HispaDOS/Pruebas/Pendientes}
[ -d "$ORIGEN" ] || exit 0
# los nombres ya vienen saneados por subir.php; aquí se vuelven a mirar (nada
# de «..», ni enlaces, ni lo que no sea un archivo normal)
find "$ORIGEN" -mindepth 3 -maxdepth 3 -type f -mmin +2 ! -name '.*' -print0 |
while IFS= read -r -d '' f; do
  rel=${f#"$ORIGEN"/}
  equipo=${rel%%/*}; resto=${rel#*/}; lote=${resto%%/*}; nombre=${resto#*/}
  if ! [[ $equipo =~ ^[A-Za-z0-9_-]{1,40}$ && $lote =~ ^20[0-9]{6}-[0-9]{6}-[0-9A-F]{4}$ && $nombre =~ ^[A-Z0-9_-]{1,8}\.(CSV|LOG|TXT|BMP)$ ]]; then
    echo "$(date -Is) se deja (nombre raro): $rel"
    continue
  fi
  d="$DESTINO/$equipo/Termometro/subidas/$lote"
  mkdir -p "$d"
  # (copiar y borrar, no mover: así queda de root y con sus permisos)
  install -o root -g root -m 0644 "$f" "$d/$nombre"
  rm -f -- "$f"
  echo "$(date -Is) $equipo/$lote/$nombre"
done
# las carpetas de los lotes ya vacías (y las de equipo vacías)
find "$ORIGEN" -mindepth 1 -maxdepth 2 -type d -empty -mmin +2 -delete || true
