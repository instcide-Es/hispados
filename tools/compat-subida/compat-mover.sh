#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# compat-mover.sh - (COMPAT) los informes del Asistente de compatibilidad que
# ha recibido informe.php (como el usuario hispacompat, en
# /www/hispados-compat-datos/entrantes/<AAAAMMDD>/) pasan al FTP:
# /root/FTP/HispaDOS/Compatibilidad/<AAAAMMDD>/, de root y 0644, y se rehace
# el resumen (compat-resumen.py: resumen.csv y resumen.json). Solo lo que
# lleva quieto más de 1 minuto. Lo pone en marcha cron (root), cada 5 min:
#   */5 * * * * root /root/hispados-tareas/compat-mover.sh >> /root/hispados-tareas/compat-mover.log 2>&1
# Y borra los topes de días pasados (cuotas/*-AAAAMMDD de hace más de 2 días).
# Con argumentos (pruebas): ORIGEN DESTINO [DATOS].
set -eu
ORIGEN=${1:-/www/hispados-compat-datos/entrantes}
DESTINO=${2:-/root/FTP/HispaDOS/Compatibilidad}
DATOS=${3:-/www/hispados-compat-datos}
AQUI=$(dirname "$(readlink -f "$0")")
[ -d "$ORIGEN" ] || exit 0
find "$ORIGEN" -mindepth 2 -maxdepth 2 -type f -mmin +1 ! -name '.*' -print0 |
while IFS= read -r -d '' f; do
  rel=${f#"$ORIGEN"/}
  dia=${rel%%/*}; nombre=${rel#*/}
  if ! [[ $dia =~ ^20[0-9]{6}$ && $nombre =~ ^[0-9]{6}-[0-9a-f]{8}-[0-9a-f]{6}\.txt$ ]]; then
    echo "$(date -Is) se deja (nombre raro): $rel"
    continue
  fi
  mkdir -p "$DESTINO/$dia"
  install -o root -g root -m 0644 "$f" "$DESTINO/$dia/$nombre"
  rm -f -- "$f"
  echo "$(date -Is) $dia/$nombre"
done
find "$ORIGEN" -mindepth 1 -maxdepth 1 -type d -empty -mmin +2 -delete || true
[ -d "$DATOS/cuotas" ] && find "$DATOS/cuotas" -maxdepth 1 -type f -mtime +2 -delete || true
# el resumen, si hay algo nuevo (o si no está)
if [ -d "$DESTINO" ] && { [ ! -f "$DESTINO/resumen.json" ] || [ -n "$(find "$DESTINO" -mindepth 2 -name '*.txt' -newer "$DESTINO/resumen.json" -print -quit)" ]; }; then
  python3 -I "${COMPAT_RESUMEN:-$AQUI/compat-resumen.py}" "$DESTINO"
fi
