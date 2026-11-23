#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo32/hispac/prueba_red.sh QUÉ CARPETA
# Lo que preparan las pruebas de SSH, WireGuard y Tailscale (castel/pruebas.sh,
# parte 18; docs/PENTIUM.md §14 ter). En CARPETA deja lo de cada una:
#   sshd CARPETA PUERTO   un sshd de OpenSSH propio de la prueba, en 127.0.0.1:PUERTO,
#                         con su llave del equipo, la llave de cliente de Hispa
#                         (ID_ED.SSH, para su disco) en su authorized_keys y un
#                         ForceCommand que pinta con colores y lee una línea (no da
#                         una shell); su PID en CARPETA/sshd.pid
#   wg CARPETA DISCO      WireGuard contra el del núcleo Linux: un espacio de nombres
#                         de red propio, con wg0 (10.9.0.1) y QEMU dentro (Hispa,
#                         10.9.0.2): lo de Hispa en CARPETA/q.sal, lo de fuera en
#                         CARPETA/fuera.txt. Necesita root, ip netns y wg
#   tailscale CARPETA DISCO PUERTO  Tailscale contra Headscale (HEADSCALE=binario) con su DERP,
#                         y un tailscaled de pares, en otro espacio de nombres; lo
#                         mismo: CARPETA/q.sal y CARPETA/fuera.txt
# Los espacios de nombres y lo que se pone en marcha se quitan al acabar (por
# su PID); la red de la máquina y su Tailscale no se tocan.
set -u
AQUI=$(dirname "$(readlink -f "$0")")
N=$AQUI/..
QUE=${1:?uso: prueba_red.sh sshd|wg|tailscale CARPETA ...}
D=$(readlink -f "${2:?falta la carpeta}")
mkdir -p $D
export MTOOLS_SKIP_CHECK=1

llave_cliente() {
  # la semilla de Ed25519 de Hispa (ID_ED.SSH) y su línea de authorized_keys
  head -c 32 /dev/urandom > $D/ID_ED.SSH
  python3 -I - $D/ID_ED.SSH > $D/ak <<'PY'
import sys, struct, base64
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
from cryptography.hazmat.primitives import serialization
s = open(sys.argv[1], 'rb').read()
p = Ed25519PrivateKey.from_private_bytes(s).public_key().public_bytes(serialization.Encoding.Raw, serialization.PublicFormat.Raw)
b = struct.pack('>I', 11) + b'ssh-ed25519' + struct.pack('>I', 32) + p
print('ssh-ed25519 ' + base64.b64encode(b).decode() + ' hispa-prueba')
PY
}

case $QUE in
sshd)
  P=${3:?falta el puerto}
  rm -f $D/hk $D/hk.pub
  ssh-keygen -q -t ed25519 -N '' -f $D/hk
  llave_cliente
  cat > $D/prueba.sh <<'EOF'
#!/bin/sh
printf '\033[2J\033[H\033[1;32mHOLA DESDE OPENSSH\033[0m TERM=%s\r\n' "$TERM"
printf '\033[5;10H\033[44;37m caja azul \033[0m \342\224\200\303\261\r\n'
if [ -n "$SSH_ORIGINAL_COMMAND" ]; then echo "orden: $SSH_ORIGINAL_COMMAND"; exit 3; fi
printf '\033[7;1HEscriba algo: '
read linea
printf '\033[9;1Hleido: [%s]\r\n' "$linea"
sleep 3
EOF
  chmod +x $D/prueba.sh
  cat > $D/config <<EOF
Port $P
ListenAddress 127.0.0.1
HostKey $D/hk
PidFile $D/sshd.pid
AuthorizedKeysFile $D/ak
StrictModes no
UsePAM no
PasswordAuthentication no
KbdInteractiveAuthentication no
PermitRootLogin prohibit-password
ForceCommand $D/prueba.sh
AllowTcpForwarding no
X11Forwarding no
EOF
  setsid /usr/sbin/sshd -D -f $D/config -E $D/sshd.log < /dev/null > /dev/null 2>&1 &
  for _ in $(seq 50); do [ -s $D/sshd.pid ] && break; sleep 0.1; done
  ;;
wg)
  DISCO=${3:?falta el disco}
  ESP=hispawg$$
  umask 077
  wg genkey > $D/fuera.key; wg pubkey < $D/fuera.key > $D/fuera.pub
  wg genkey > $D/hispa.key; wg pubkey < $D/hispa.key > $D/hispa.pub
  umask 022
  ip netns add $ESP || exit 1
  trap 'ip netns del $ESP 2> /dev/null' EXIT
  ip -n $ESP link set lo up
  ip -n $ESP link add wg0 type wireguard
  ip netns exec $ESP wg set wg0 private-key $D/fuera.key listen-port 51820 peer "$(cat $D/hispa.pub)" allowed-ips 10.9.0.2/32
  ip -n $ESP addr add 10.9.0.1/24 dev wg0
  ip -n $ESP link set wg0 up
  printf '[Interface]\r\nPrivateKey = %s\r\nAddress = 10.9.0.2/24\r\n\r\n[Peer]\r\nPublicKey = %s\r\nEndpoint = 10.0.2.2:51820\r\nAllowedIPs = 10.9.0.0/24\r\nPersistentKeepalive = 25\r\n' \
    "$(cat $D/hispa.key)" "$(cat $D/fuera.pub)" > $D/WG0.CNF
  mmd -i $DISCO ::WG 2> /dev/null
  mcopy -o -i $DISCO $D/WG0.CNF ::WG/WG0.CNF
  # (ESCUDO) no a ciegas (45 s, que con la máquina cargada no llegan): hasta
  # que el WireGuard de aquí tenga el apretón de manos de Hispa-Pentium
  ( for _ in $(seq 120); do
      [ "$(ip netns exec $ESP wg show wg0 latest-handshakes | awk '{print $2}')" -gt 0 ] 2> /dev/null && break
      sleep 1
    done
    sleep 2
    echo "=== ping"; ip netns exec $ESP ping -c 3 -W 2 10.9.0.2
    echo "=== wg"; ip netns exec $ESP wg show wg0
    echo "=== ssh"
    ip netns exec $ESP timeout 60 ssh -p 22 -o StrictHostKeyChecking=no -o UserKnownHostsFile=$D/../conocidos -o UpdateHostKeys=no -o IdentitiesOnly=yes -o BatchMode=yes -i $D/../clave \
      prueba@10.9.0.2 "echo hola por wireguard" < /dev/null
    echo "código $?"
  ) > $D/fuera.txt 2>&1 &
  FUERA=$!
  # (con TMPDIR aparte: las pruebas de otras ramas usan /tmp a la vez)
  mkdir -p $D/tmp
  TMPDIR=$D/tmp ESCRIBE=1 DISCO=$DISCO ARRANQUE=ordenes MODULOS= QEMU_MAS="-nic user,model=e1000" ip netns exec $ESP $N/probar.sh "" ":pausa 3" \
    "wg /encender" ":pausa 6" "sshd /encender" ":pausa 4" "ping 10.9.0.1" ":pausa 8" "wg" ":pausa 50" > $D/q.sal 2>&1
  wait $FUERA
  ;;
tailscale)
  DISCO=${3:?falta el disco}
  PS=${4:?falta el puerto del sshd de la prueba}
  HS=${HEADSCALE:?HEADSCALE: el binario de Headscale}
  ESP=hispats$$
  PIDS=
  umask 077
  openssl req -x509 -newkey ec -pkeyopt ec_paramgen_curve:P-256 -nodes -keyout $D/ca.key -out $D/ca.pem -days 2 -subj "/CN=Prueba Hispa Tailscale" \
    -addext "basicConstraints=critical,CA:TRUE" -addext "keyUsage=critical,keyCertSign,cRLSign" 2> /dev/null
  openssl req -newkey ec -pkeyopt ec_paramgen_curve:P-256 -nodes -keyout $D/srv.key -out $D/srv.csr -subj "/CN=10.0.2.2" 2> /dev/null
  printf "subjectAltName=IP:10.0.2.2,DNS:10.0.2.2\nbasicConstraints=CA:FALSE\nextendedKeyUsage=serverAuth\n" > $D/ext.cnf
  openssl x509 -req -in $D/srv.csr -CA $D/ca.pem -CAkey $D/ca.key -CAcreateserial -out $D/srv.pem -days 2 -extfile $D/ext.cnf 2> /dev/null
  umask 022
  openssl x509 -in $D/ca.pem -outform DER -out $D/CA.CER
  mmd -i $DISCO ::RAICES 2> /dev/null
  mcopy -o -i $DISCO $D/CA.CER ::RAICES/PRUEBA.CER
  rm -f $D/db.sqlite*
  cat > $D/hs.yaml <<EOF
server_url: https://10.0.2.2:8443
listen_addr: 0.0.0.0:8443
metrics_listen_addr: 127.0.0.1:19090
grpc_listen_addr: 127.0.0.1:50443
noise:
  private_key_path: $D/noise.key
prefixes:
  v4: 100.64.0.0/10
  v6: fd7a:115c:a1e0::/48
  allocation: sequential
derp:
  server:
    enabled: true
    region_id: 999
    region_code: "hispa"
    region_name: "Relevo de prueba"
    verify_clients: true
    stun_listen_addr: "0.0.0.0:3478"
    private_key_path: $D/derp.key
    automatically_add_embedded_derp_region: true
    ipv4: 10.0.2.2
  urls: []
  paths: []
  auto_update_enabled: false
disable_check_updates: true
database:
  type: sqlite
  sqlite:
    path: $D/db.sqlite
tls_cert_path: $D/srv.pem
tls_key_path: $D/srv.key
log:
  level: info
policy:
  mode: file
  path: ""
dns:
  magic_dns: true
  base_domain: prueba.hispa
  override_local_dns: false
  nameservers:
    global: []
unix_socket: $D/hs.sock
unix_socket_permission: "0770"
EOF
  ip netns add $ESP || exit 1
  ip -n $ESP link set lo up
  ip -n $ESP addr add 10.0.2.2/32 dev lo
  limpia() {
    for p in $PIDS; do kill $p 2> /dev/null; done
    sleep 1
    ip netns del $ESP 2> /dev/null
  }
  trap limpia EXIT
  ip netns exec $ESP $HS -c $D/hs.yaml serve > $D/hs.log 2>&1 &
  PIDS="$PIDS $!"
  for _ in $(seq 50); do ip netns exec $ESP curl -s --cacert $D/ca.pem https://10.0.2.2:8443/key?v=113 > /dev/null 2>&1 && break; sleep 0.2; done
  ip netns exec $ESP $HS -c $D/hs.yaml users create prueba > /dev/null 2>&1
  # el otro equipo: un tailscaled en modo de usuario, con su estado y su socket
  mkdir -p $D/tsd
  ip netns exec $ESP env SSL_CERT_FILE=$D/ca.pem TS_NO_LOGS_NO_SUPPORT=true /usr/sbin/tailscaled --tun=userspace-networking \
    --statedir=$D/tsd --socket=$D/tsd/sock --port=41642 > $D/tsd.log 2>&1 &
  PIDS="$PIDS $!"
  sleep 3
  K=$(ip netns exec $ESP $HS -c $D/hs.yaml preauthkeys create --user 1 2>&1 | tail -1)
  TSC="tailscale --socket=$D/tsd/sock"
  ip netns exec $ESP timeout 60 $TSC up --login-server=https://10.0.2.2:8443 --authkey=$K --hostname=pares-vps > /dev/null 2>&1
  # el sshd de la prueba, dentro (el de los pares, que tailscaled lleva a 127.0.0.1:22623)
  ip netns exec $ESP /usr/sbin/sshd -D -f $D/../sshd/config -o PidFile=$D/sshd2.pid -E $D/sshd.log < /dev/null > /dev/null 2>&1 &
  PIDS="$PIDS $!"
  mkdir -p $D/tmp
  rm -rf $D/tmp/*
  touch $D/marca
  ( SAL=
    for _ in $(seq 90); do
      sleep 2
      [ -z "$SAL" ] && SAL=$(find $D/tmp -maxdepth 2 -name sal 2> /dev/null | head -1)
      [ -n "$SAL" ] && grep -aq "register/" $SAL && break
    done
    ID=$(grep -ao "register/[A-Za-z0-9_-]*" $SAL | head -1 | cut -d/ -f2)
    echo "=== alta: $ID"
    ip netns exec $ESP $HS -c $D/hs.yaml auth register --auth-id "$ID" --user prueba 2>&1 | tail -1
    sleep 50
    echo "=== estado"; ip netns exec $ESP $TSC status
    HIP=$(ip netns exec $ESP $TSC status | awk '/hispa/{print $1}' | head -1)
    echo "=== ping $HIP"; ip netns exec $ESP timeout 20 $TSC ping -c 3 $HIP
    echo "=== ssh"
    ip netns exec $ESP timeout 60 ssh -o "ProxyCommand=$TSC nc %h %p" -o StrictHostKeyChecking=no -o UserKnownHostsFile=$D/../conocidos -o UpdateHostKeys=no \
      -o IdentitiesOnly=yes -o BatchMode=yes -i $D/../clave prueba@$HIP "echo hola por tailscale" < /dev/null
    echo "código $?"
    # lo que el servidor ha guardado del equipo (la clave del disco y el relevo de casa: sin eso, los demás no lo encuentran)
    echo "=== nodos"
    ip netns exec $ESP $HS -c $D/hs.yaml nodes list -o json > $D/nodos.json 2>&1
    python3 -I -c 'import json,sys
for n in json.load(open(sys.argv[1])):
  print("nodo", n.get("given_name") or n.get("name"), "disco", "sí" if (n.get("disco_key") or "").startswith("discokey:") and n.get("disco_key")[9:].strip("0") else "no")' $D/nodos.json 2>&1
    # y cómo lo ve el otro equipo (como «tailscale debug netmap» en la red de verdad)
    ip netns exec $ESP $TSC debug netmap > $D/netmap.json 2>&1
    python3 -I -c 'import json,sys
for p in json.load(open(sys.argv[1])).get("Peers") or []:
  print("par", (p.get("Hostinfo") or {}).get("Hostname"), "disco", "sí" if p.get("DiscoKey") else "no", "derp", p.get("HomeDERP") or p.get("DERP"))' $D/netmap.json 2>&1
  ) > $D/fuera.txt 2>&1 &
  FUERA=$!
  CAPTURAS=$D TMPDIR=$D/tmp MEMORIA=64 ESCRIBE=1 DISCO=$DISCO ARRANQUE=ordenes MODULOS= QEMU_MAS="-nic user,model=e1000" ip netns exec $ESP $N/probar.sh "" ":pausa 3" \
    "sshd /encender" ":pausa 2" "tailscale /conectar https://10.0.2.2:8443" ":pausa 30" "tailscale" ":pausa 30" "tailscale" ":pausa 3" \
    "ping pares-vps" ":pausa 8" "ssh -p $PS root@pares-vps" ":pausa 15" ":escribe s" ":pausa 8" "hola tailscale" ":pausa 6" ":texto ts4" ":pausa 50" > $D/q.sal 2>&1
  wait $FUERA
  ;;
esac
