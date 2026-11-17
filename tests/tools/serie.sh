# Esperas para las pruebas que hablan con Hispa-DOS por el puerto serie.
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
#
# En vez de «sleep N» fijos (que con el equipo cargado se quedan cortos: la
# orden siguiente se escribe antes de que acabe la anterior, o se mata a QEMU
# a media orden), se espera a que en el registro serie aparezca lo que se
# espera, con un tope generoso. Con el equipo libre, además, se tarda menos.
#
#   . tests/tools/serie.sh
#   serie_espera FICHERO TEXTO [VECES] [TOPE_s]
#        espera a que TEXTO (literal, en ASCII) salga VECES veces (1) en
#        FICHERO; devuelve 1 (y lo dice) si se agota el TOPE
#   serie_indicador FICHERO DESDE [TOPE_s] [QUIETO_s]
#        espera a que el registro haya crecido más allá del byte DESDE y acabe
#        en el indicador de órdenes («A:\>», «C:\DIR>»): la orden anterior ha
#        terminado. Si lo que hay es una pregunta (una línea a medias, sin
#        indicador) y lleva QUIETO_s segundos sin moverse, también vale: hay
#        que contestarla.
#   serie_ordenes FICHERO TOPE_s QUIETO_s orden...
#        escribe cada orden por el descriptor 9 (la entrada serie de QEMU: una
#        tubería con nombre abierta con «exec 9> tubería») cuando la anterior
#        ha terminado, y al final espera a que termine la última.
# El tope por omisión es SERIE_TOPE (120 s).
# Serial-log wait helpers for the tests (instead of fixed sleeps).
SERIE_TOPE=${SERIE_TOPE:-120}

serie_tam() { stat -c %s "$1" 2>/dev/null || echo 0; }

serie_espera() {
  local f=$1 txt=$2 veces=${3:-1} tope=${4:-$SERIE_TOPE} fin n
  fin=$(( $(date +%s) + tope ))
  while :; do
    n=$(grep -a -o -F -- "$txt" "$f" 2>/dev/null | wc -l)
    [ "$n" -ge "$veces" ] && return 0
    if [ "$(date +%s)" -ge "$fin" ]; then
      echo "  (serie: se agotaron $tope s esperando «$txt»)"
      return 1
    fi
    sleep 0.1
  done
}

serie_indicador() {
  local f=$1 desde=$2 tope=${3:-$SERIE_TOPE} quieto=${4:-5} fin tam ant=-1 visto
  fin=$(( $(date +%s) + tope )); visto=$(date +%s)
  while :; do
    tam=$(serie_tam "$f")
    if [ "$tam" -gt "$desde" ]; then
      # ¿la última línea es solo el indicador de órdenes?
      if tail -c 80 "$f" | tr '\r' '\n' | tail -n 1 | grep -a -q -E '^[A-Z]:(\\[^ >]*)?>$'; then
        return 0
      fi
      if [ "$tam" != "$ant" ]; then ant=$tam; visto=$(date +%s); fi
      # una pregunta: línea sin terminar y quieta un rato
      if [ $(( $(date +%s) - visto )) -ge "$quieto" ] && [ -n "$(tail -c 1 "$f" | tr -d '\r\n')" ]; then
        return 0
      fi
    fi
    if [ "$(date +%s)" -ge "$fin" ]; then
      echo "  (serie: se agotaron $tope s esperando el indicador de órdenes)"
      return 1
    fi
    sleep 0.1
  done
}

serie_ordenes() {
  local f=$1 tope=$2 quieto=$3 x desde=0; shift 3
  for x in "$@"; do
    serie_indicador "$f" "$desde" "$tope" "$quieto" || return 1
    desde=$(( $(serie_tam "$f") + ${#x} ))    # tiene que salir, al menos, el eco de la orden
    printf '%s\r' "$x" >&9
  done
  serie_indicador "$f" "$desde" "$tope" "$quieto"
}
