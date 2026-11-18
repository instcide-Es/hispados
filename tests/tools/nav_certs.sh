# Los certificados de las pruebas del Navegador y de HSPTLS (tests/run_red32_tests.sh
# y tests/run_navegador_tests.sh): una autoridad raíz de pruebas (que solo se
# le da al sistema en la prueba: RAIZ.CER junto al programa) y los
# certificados del servidor tests/tools/nav_servidor.py.
#   nav_certs CARPETA   -> 0 si se han podido hacer (hace falta openssl)
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
nav_certs() {
  local d=$1 n
  command -v openssl > /dev/null || return 1
  ( cd "$d" || exit 1
    openssl ecparam -name prime256v1 -genkey -noout -out raiz.key
    openssl req -x509 -new -key raiz.key -sha256 -days 3650 -subj "/O=Hispa-486/CN=Raiz de pruebas" \
      -addext "basicConstraints=critical,CA:TRUE" -addext "keyUsage=critical,keyCertSign,cRLSign" -out raiz.pem
    openssl x509 -in raiz.pem -outform DER -out RAIZ.CER
    hoja() {   # hoja nombre SAN [rsa]
      if [ "${3:-}" = rsa ]; then openssl genrsa -out $1.key 2048; else openssl ecparam -name prime256v1 -genkey -noout -out $1.key; fi
      openssl req -new -key $1.key -subj "/O=Hispa-486/CN=$2" -out $1.csr
      printf 'subjectAltName=DNS:%s\nbasicConstraints=CA:FALSE\nkeyUsage=digitalSignature\nextendedKeyUsage=serverAuth\n' "$2" > $1.ext
    }
    for n in "bueno 10.0.2.2" "otro otro.prueba" "rsa 10.0.2.2 rsa"; do
      set -- $n
      hoja "$@"
      openssl x509 -req -in $1.csr -CA raiz.pem -CAkey raiz.key -CAcreateserial -days 30 -sha256 -extfile $1.ext -out $1.crt
      cat $1.key $1.crt > $1.pem
    done
    # uno que no firma nadie conocido (firmado por sí mismo)
    hoja suelto 10.0.2.2
    openssl x509 -req -in suelto.csr -signkey suelto.key -days 30 -sha256 -extfile suelto.ext -out suelto.crt
    cat suelto.key suelto.crt > suelto.pem
    # y uno caducado (fechas de 2020), firmado por la raíz
    hoja caducado 10.0.2.2
    mkdir -p ca; : > ca/index.txt; echo 01 > ca/serial
    printf '%s\n' '[ca]' 'default_ca=x' '[x]' 'dir=./ca' 'database=$dir/index.txt' 'new_certs_dir=$dir' 'serial=$dir/serial' \
      'default_md=sha256' 'policy=p' 'unique_subject=no' 'x509_extensions=e' '[p]' 'organizationName=optional' 'commonName=supplied' \
      '[e]' 'subjectAltName=DNS:10.0.2.2' 'basicConstraints=CA:FALSE' > ca.cnf
    openssl ca -config ca.cnf -cert raiz.pem -keyfile raiz.key -in caducado.csr -out caducado.crt \
      -startdate 20200101000000Z -enddate 20210101000000Z -batch -notext
    cat caducado.key caducado.crt > caducado.pem
  ) > "$d/certs.log" 2>&1
  [ -s "$d/bueno.pem" ] && [ -s "$d/otro.pem" ] && [ -s "$d/suelto.pem" ] && [ -s "$d/caducado.pem" ] && [ -s "$d/rsa.pem" ] && [ -s "$d/RAIZ.CER" ]
}
