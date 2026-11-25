#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo32/termo/prepara_memoria.sh CARPETA
# (TERMO) Lo que va en X:\TERMO de la memoria del Termómetro (lo llama usb.sh
# con TERMO=1): ENVIO.INI («Enviar a Carlos»: los servidores, el equipo y el
# testigo: 32 octetos al azar, en 64 cifras hexadecimales), SSH.INI (adónde
# va el botón «Terminal») y SSH\ID_ED.SSH, la llave propia de la memoria
# (Ed25519: la semilla de 32 octetos, como la de SSH /LLAVE), con su parte
# pública en SSH\ID_ED.PUB y, si se da, el CONOCIDO.TXT del equipo de destino
# (para que la primera vez no haya que fiarse de la huella).
# El testigo y la llave pública se escriben UNA vez, en TERMO_PARA (un archivo
# fuera de la memoria y del FTP: lo que hay que registrar en el servidor) y
# por la salida. Lo de cada memoria es distinto: se hace cada vez.
# Lo que se puede cambiar (el entorno):
#   TERMO_SERVIDORES  «url url» (https://; por omisión, hispados.es y actualiza.hispados.es)
#   TERMO_EQUIPO      el nombre de la carpeta en el servidor (Portatil-gamer)
#   TERMO_PRUEBA=si   deja servidores http:// (solo las pruebas)
#   TERMO_SSH_EQUIPO, TERMO_SSH_PUERTO, TERMO_SSH_USUARIO (hispados.es, 22, termo)
#   TERMO_SSH_CONOCIDO  un archivo con la llave pública del equipo (ssh-ed25519 ...)
#   TERMO_PARA        dónde dejar el testigo y la llave pública
#   TERMO_TESTIGO, TERMO_LLAVE  los de una memoria anterior (el testigo y la semilla de 32 octetos)
set -eu
D=${1:?uso: prepara_memoria.sh CARPETA}
mkdir -p "$D/SSH"
SERV=${TERMO_SERVIDORES:-https://hispados.es/termo/subir.php https://actualiza.hispados.es/termo/subir.php}   # (TERMO 1.2) hispados.es primero: actualiza no tiene HTTPS
EQ=${TERMO_EQUIPO:-Portatil-gamer}
SE=${TERMO_SSH_EQUIPO:-hispados.es}
SP=${TERMO_SSH_PUERTO:-22}
SU=${TERMO_SSH_USUARIO:-termo}
# (con TERMO_TESTIGO y TERMO_LLAVE, los de una memoria anterior: así lo ya
# registrado en el servidor sigue valiendo al rehacerla)
if [ -n "${TERMO_TESTIGO:-}" ]; then TESTIGO=$TERMO_TESTIGO; else TESTIGO=$(head -c 32 /dev/urandom | od -An -tx1 | tr -d ' \n'); fi
RES=$(printf '%s' "$TESTIGO" | sha256sum | cut -d' ' -f1)
{
  printf '; (TERMO) «Enviar a Carlos»: adónde se mandan los registros y con qué testigo.\r\n'
  printf '; No se manda nunca este archivo. Si se pierde la memoria, que se borre la línea\r\n'
  printf '; del testigo en el servidor y ya no vale.\r\n'
  for s in $SERV; do printf 'servidor=%s\r\n' "$s"; done
  printf 'equipo=%s\r\ntestigo=%s\r\n' "$EQ" "$TESTIGO"
  if [ "${TERMO_PRUEBA:-}" = si ]; then printf 'prueba=si\r\n'; fi
} > "$D/ENVIO.INI"
{
  printf '; (TERMO) el botón «Terminal»: SSH con la llave de la memoria (TERMO\\SSH\\ID_ED.SSH)\r\n'
  printf '; hacia este equipo. Se puede cambiar con el Bloc de notas de Windows.\r\n'
  printf 'equipo=%s\r\npuerto=%s\r\nusuario=%s\r\n' "$SE" "$SP" "$SU"
  if [ "${TERMO_SSH_GRABAR:-}" = si ]; then printf 'grabar=si\r\n'; fi
  if [ -n "${TERMO_SSH_VENTANA:-}" ]; then printf 'ventana=%s\r\n' "$TERMO_SSH_VENTANA"; fi
} > "$D/SSH.INI"
if [ -n "${TERMO_LLAVE:-}" ] && [ "$(stat -c %s "$TERMO_LLAVE")" = 32 ]; then cp "$TERMO_LLAVE" "$D/SSH/ID_ED.SSH"; else head -c 32 /dev/urandom > "$D/SSH/ID_ED.SSH"; fi
PUB=$(python3 -I - "$D/SSH/ID_ED.SSH" <<'PY'
import sys, struct, base64
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
from cryptography.hazmat.primitives import serialization
s = open(sys.argv[1], 'rb').read()
p = Ed25519PrivateKey.from_private_bytes(s).public_key().public_bytes(serialization.Encoding.Raw, serialization.PublicFormat.Raw)
b = struct.pack('>I', 11) + b'ssh-ed25519' + struct.pack('>I', 32) + p
print('ssh-ed25519 ' + base64.b64encode(b).decode() + ' termometro-memoria')
PY
)
printf '%s\r\n' "$PUB" > "$D/SSH/ID_ED.PUB"
if [ -n "${TERMO_SSH_CONOCIDO:-}" ] && [ -s "$TERMO_SSH_CONOCIDO" ]; then
  if [ "$SP" = 22 ]; then Q=$SE; else Q="[$SE]:$SP"; fi
  printf '%s %s\r\n' "$Q" "$(awk '{print $1" "$2; exit}' "$TERMO_SSH_CONOCIDO")" > "$D/SSH/CONOCIDO.TXT"
fi
INFO="testigo=$TESTIGO
sha256=$RES
equipo=$EQ
llave=$PUB
ssh=$SU@$SE:$SP"
if [ -n "${TERMO_PARA:-}" ]; then printf '%s\n' "$INFO" > "$TERMO_PARA"; chmod 600 "$TERMO_PARA"; fi
echo "prepara_memoria.sh: (TERMO) lo de esta memoria (apúntelo: no se vuelve a enseñar):"
printf '%s\n' "$INFO" | sed 's/^/  /'
