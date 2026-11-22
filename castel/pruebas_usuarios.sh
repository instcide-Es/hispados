# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# pruebas_usuarios.sh - (USUARIOS) la parte 38 de pruebas.sh (SOLO=usuarios),
# que lee este archivo («.»): la función parte_usuarios, con los ayudantes de
# pruebas.sh (ok, ko, mira, miracap, saca, sano) y sus carpetas ($AQUI, $P).
#
#   0. En Linux: ARGON2 (argon2.c, con ASan y UBSan): el vector de la RFC
#      9106, la página 437 y UTF-8, y 2.000 pedidos al azar; tools/hpq.py:
#      la serie del catálogo (sube al firmar; --serie).
#   1. Un sistema de antes (HISPA.INI, sin USUARIOS.TXT): su usuario entra
#      solo y es administrador; se pone su contraseña, se dan de alta Bea
#      (con contraseña) y Biblioteca (sin ella, entra sola); sus carpetas;
#      PAQUETES por Internet: el catálogo firmado con la serie 100 se usa y
#      se apunta, el de la serie 50 no. Desde fuera: USUARIOS.TXT con
#      Argon2id (el mismo resumen que en Linux), ni rastro de las contraseñas.
#   2. El arranque siguiente: entra sola Biblioteca (normal): no lee ni
#      escribe la carpeta de Ana, ni lanza sus programas, ni lee USUARIOS.TXT
#      ni AZAR.SEM ni el disco en crudo, ni pone la hora, ni el modo
#      administrador, ni instala; SALIR, y la entrada en texto: Ana con una
#      contraseña mala y con la buena; el modo administrador con SU
#      contraseña. El azar del núcleo no se repite de un arranque a otro.
#   3. Con el escritorio: la pantalla de entrada (Ctrl+Alt+Supr y el logo no
#      la saltan), la contraseña mala, la buena, ADMIN en la Consola (la
#      ventanita del núcleo pide la contraseña), cerrar la sesión y entrar
#      como Bea («Hola, Bea»).
#   4. SSH con los usuarios (OpenSSH de aquí, por QEMU): Bea con su
#      contraseña entra y es Bea; con otra, no; Biblioteca (sin contraseña),
#      no; Ana con la llave de su carpeta; SSHD /ENCENDER, solo un administrador.

parte_usuarios() {
echo "== Hispa-Pentium: los usuarios (la entrada, Argon2id, las carpetas), la serie del catálogo, el azar del núcleo y SSH con los usuarios =="
local X=$P/usuarios HC=$AQUI/nucleo32/hispac S hechos=1 sal1 sal2 a1 a2 PS SPID FUERA
local HPQ="python3 -I $AQUI/../tools/hpq.py"
rm -rf $X; mkdir -p $X/v $X/raiz/pentium/nuevo $X/raiz/pentium/viejo $X/f $X/srv
export MTOOLS_SKIP_CHECK=1
G=$X
# ---- 0. en Linux ----
{ echo '#include <stdint.h>'; echo 'typedef uint32_t u32; typedef uint16_t u16; typedef uint8_t u8;'
  sed -n '/^static const u16 alta437/,/^};/p' $HC/hispa.c; sed -n '/^int h_utf8_de_437/,/^}/p' $HC/hispa.c; } > $X/utf437.c
if gcc -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined -DARGON2_LINUX -I$HC -I$AQUI/../terceros/monocypher \
     $HC/argon2_prueba.c $HC/argon2.c $AQUI/../terceros/monocypher/monocypher.c $X/utf437.c -o $X/argon2_prueba > $X/gcc.txt 2>&1; then
  if ASAN_OPTIONS=detect_leaks=0 $X/argon2_prueba 2000 > $X/a2.txt 2>&1; then
    ok "ARGON2 en Linux (ASan y UBSan): el vector de la RFC 9106 (Argon2id), la misma contraseña en la página 437 y en UTF-8, y $(tail -n 1 $X/a2.txt | sed 's/^argon2_prueba: //')"
  else ko "ARGON2 en Linux:"; tail -n 5 $X/a2.txt | sed 's/^/        /'; fi
else ko "argon2_prueba no compila: $(grep -m 3 error $X/gcc.txt)"; hechos=0; fi
# hpq.py: la serie del catálogo
$HPQ clave $X/prueba.key > /dev/null && $HPQ publica $X/prueba.key $X/clave.cas > /dev/null || hechos=0
printf 'una nota del paquete\r\n' > $X/f/NOTA.TXT
( cd $X/f && $HPQ crea $X/raiz/pentium/nuevo/NOTA.HPQ --nombre nota --version 1.0 --descripcion 'Una nota' NOTA.TXT='\HISPA\NOTA.TXT' ) > $X/hpq.txt 2>&1 || hechos=0
cp $X/raiz/pentium/nuevo/NOTA.HPQ $X/raiz/pentium/viejo/
$HPQ catalogo $X/raiz/pentium/viejo --clave $X/prueba.key > /dev/null 2>&1 && $HPQ firma $X/raiz/pentium/viejo $X/prueba.key > /dev/null && a1=$(sed -n 's/^serie=\([0-9]*\).*/\1/p' $X/raiz/pentium/viejo/CATALOGO.TXT)
$HPQ firma $X/raiz/pentium/viejo $X/prueba.key > /dev/null && a2=$(sed -n 's/^serie=\([0-9]*\).*/\1/p' $X/raiz/pentium/viejo/CATALOGO.TXT)
if [ -n "${a1:-}" ] && [ -n "${a2:-}" ] && [ "$a2" -gt "$a1" ] && [ "$a1" -ge 1795000000 ] && [ "$(sed -n 2p $X/raiz/pentium/viejo/CATALOGO.TXT | tr -d '\r')" = "serie=$a2" ] \
   && $HPQ comprueba $X/raiz/pentium/viejo $X/clave.cas > /dev/null; then
  ok "hpq.py firma: «serie=N» en la segunda línea del catálogo, dentro de lo firmado, y sube cada vez ($a1, $a2: como poco, los segundos desde 1970)"
else ko "hpq.py firma: la serie no sube ($a1, $a2) o la firma no vale"; fi
$HPQ catalogo $X/raiz/pentium/nuevo --clave $X/prueba.key --serie 100 > /dev/null && $HPQ firma $X/raiz/pentium/viejo $X/prueba.key --serie 50 > /dev/null || hechos=0
# ---- los programas y los discos ----
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/semilla.cas -o $X/semilla.asm > /dev/null 2>&1 && nasm -f bin $X/semilla.asm -o $X/SEMILLA.BIN || hechos=0
cp $AQUI/nucleo32/upaquete.cas $X/upaquete.cas
python3 -I $AQUI/ccastel.py $X/upaquete.cas -o $X/upaquete.asm -I $AQUI/nucleo32 > /dev/null 2>&1 && nasm -f bin $X/upaquete.asm -o $X/PAQUETES.BIN || hechos=0
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/ucuentas.cas -o $X/ucuentas.asm > /dev/null 2>&1 && nasm -f bin $X/ucuentas.asm -o $X/USUARIOS.BIN || hechos=0
# el «espía»: lo que un programa de otro usuario no debe poder
cat > $X/uespia.cas <<'CAS'
módulo UEspia
    modelo usuario
    importar Llamadas, Textos32, Formato
    variable pedido: arreglo [3] de nat32
    variable sector: arreglo [512] de byte
    variable a: arreglo [32] de byte
    variable hx: arreglo [80] de byte
    función sc(n: nat32, a1: nat32, a2: nat32, a3: nat32): nat32
        ensamblador
            mov eax, {n}
            mov ebx, {a1}
            mov ecx, {a2}
            mov edx, {a3}
            int 0x80
        fin
    fin
    función dice(qué: puntero a byte, r: nat32)
        si es_error(r) entonces escribir("espia: ", qué, ": no (", texto_error(r), ")\n") sino escribir("espia: ", qué, ": SE PUEDE\n") fin
    fin
    función abre(qué: puntero a byte, r: nat32)
        dice(qué, r)
        si no es_error(r) entonces cerrar(r) fin
    fin
    función principal(): byte
        variable i: nat32
        variable cifras: puntero a byte
        escribir("espia: soy el usuario ", mi_usuario(), "; administrador: ")
        si soy_administrador() entonces escribir("sí\n") sino escribir("no\n") fin
        abre("leer el secreto de Ana", abrir("\\USUARIOS\\ANA\\DOCUMENT\\SECRETO.TXT"))
        abre("ver la carpeta de Ana", abrir("\\USUARIOS\\ANA"))
        abre("escribir en la carpeta de Ana", crear("\\USUARIOS\\ANA\\DOCUMENT\\MALO.TXT"))
        dice("borrar el secreto de Ana", borrar("\\USUARIOS\\ANA\\DOCUMENT\\SECRETO.TXT"))
        dice("llevarse el secreto de Ana", renombrar("\\USUARIOS\\ANA\\DOCUMENT\\SECRETO.TXT", "\\DOCUMENT\\S.TXT"))
        dice("hacer una carpeta en \\USUARIOS", crear_carpeta("\\USUARIOS\\OTRO"))
        dice("lanzar un programa de la carpeta de Ana", lanzar("\\USUARIOS\\ANA\\HOLA.BIN", nulo))
        abre("leer USUARIOS.TXT", abrir("\\HISPA\\USUARIOS.TXT"))
        abre("leer AZAR.SEM", abrir("\\HISPA\\AZAR.SEM"))
        abre("escribir en su carpeta", crear("\\USUARIOS\\BIBLIOTE\\DOCUMENT\\MIO.TXT"))
        pedido[0] := 0
        pedido[1] := 1
        pedido[2] := nat32(dirección(sector[0]))
        dice("leer el disco en crudo (la 64)", sc(64, 0, nat32(dirección(pedido[0])), 0))
        dice("poner el reloj (la 67)", sc(67, (20 << 9) ob (1 << 5) ob 1, 0, 0))
        dice("el modo administrador (la 157)", sc(157, 0, 0, 0))
        dice("crear un usuario (la 201)", sc(201, 0, nat32(dirección(sector[0])), 0))
        dice("la contraseña de otro (la 202)", sc(202, 0, nat32(dirección(sector[0])), 0))
        dice("el pedido de ARGON2 (la 209)", sc(209, 0, nat32(dirección(sector[0])), 512))
        si azar_del_sistema(a, 32) = 32 entonces
            cifras := "0123456789abcdef"
            para i desde 0 hasta 31 hacer
                hx[i * 2] := cifras[a[i] >> 4]
                hx[i * 2 + 1] := cifras[a[i] yb 15]
            fin
            hx[64] := 0
            escribir("espia: azar ", hx, "\n")
        fin
        devolver 0
    fin
fin UEspia.
CAS
python3 -I $AQUI/ccastel.py $X/uespia.cas -o $X/uespia.asm -I $AQUI/nucleo32 > /dev/null 2>&1 && nasm -f bin $X/uespia.asm -o $X/ESPIA.BIN || hechos=0
$AQUI/nucleo32/disco.sh $X/vivo.img 32 red ssh > $X/disco.txt 2>&1 && mcopy -n -i $X/vivo.img '::*.BIN' $X/v/ > /dev/null 2>&1 || hechos=0
printf '[hispa]\r\nusuario=Ana\r\nequipo=PRUEBA\r\nteclado=es\r\n' > $X/HISPA.INI
mkfs.fat -F 32 -s 1 -n HISPA -C $X/sis.img 65536 > /dev/null && mmd -i $X/sis.img ::HISPA ::HISPA/PAQUETES ::DOCUMENT \
  && mcopy -i $X/sis.img $X/v/ORDENES.BIN $X/v/RED.BIN $X/v/IPCONFIG.BIN $X/v/DESCARGA.BIN $X/v/SSHD.BIN $X/v/ARGON2.BIN $X/v/HOLA.BIN \
       $X/v/BLOC.BIN $X/v/CONSOLA.BIN $X/v/EQUIPO.BIN $X/PAQUETES.BIN $X/USUARIOS.BIN $X/HISPA.INI ::HISPA/ \
  && mcopy -i $X/sis.img $X/ESPIA.BIN :: && mcopy -i $X/sis.img $X/SEMILLA.BIN ::HISPA/NUCLEO.BIN || hechos=0
if [ $hechos != 1 ]; then ko "no se han podido hacer los programas o los discos de las pruebas de los usuarios: $(tail -n 2 $X/disco.txt) $(tail -n 2 $X/hpq.txt)"; return; fi
ok "el disco de las pruebas: un sistema de antes (HISPA.INI con «usuario=Ana», sin USUARIOS.TXT) con USUARIOS.BIN, ARGON2.BIN, PAQUETES (la clave de prueba), la red y SSHD"
python3 -I $AQUI/nucleo32/servidor_prueba.py $X/srv $X/raiz > $X/srv.txt 2>&1 &
SPID=$!
for _ in $(seq 100); do [ -s $X/srv/puerto ] && break; sleep 0.1; done
local U="http://10.0.2.2:$(cat $X/srv/puerto 2> /dev/null)/pentium"
# ---- 1. el sistema de antes: sus usuarios nuevos ----
cp $X/sis.img $X/s1.img
ESCRIBE=1 SATA=$X/s1.img MEMORIA=128 MODULOS= ARRANQUE=ordenes QEMU_MAS="-nic user,model=e1000" \
  $AQUI/nucleo32/probar.sh $X/SEMILLA.BIN ":pausa 3" "usuarios" ":pausa 1" "usuarios /clave Ana" ":pausa 1" "uno2tres" "uno2tres" ":pausa 12" \
  "usuarios /nuevo Bea" ":pausa 1" "secreta" "secreta" ":pausa 12" "usuarios /nuevo Biblioteca /solo" ":pausa 1" "" ":pausa 4" \
  "echo secreto de Ana > \\usuarios\\ana\\document\\secreto.txt" "copy \\hispa\\hola.bin \\usuarios\\ana\\hola.bin" "dir \\usuarios" ":pausa 1" \
  "ipconfig" ":pausa 8" "paquetes catalogo /desde=$U/nuevo/" ":pausa 8" "paquetes catalogo /desde=$U/viejo/" ":pausa 8" > $X/t1.sal 2>&1
tr -d '\r' < $X/t1.sal > $X/t1.txt
S=$X/t1.txt
mira "un sistema de antes: su usuario (el de HISPA.INI) es administrador, sin contraseña, y entra solo, como siempre" \
  "^cuentas: el usuario de HISPA.INI (Ana), administrador, sin contraseña$" "^Entra Ana (sin contraseña)\.$" "^  Ana  administrador, sin contraseña, entra solo  (usted)$"
mira "USUARIOS /CLAVE: su contraseña (resumida con Argon2id por ARGON2.BIN, 12 MB y 3 pasadas); entonces recibe su carpeta" \
  "^cuentas: Argon2id (12288 KB, 3 pasadas) en [0-9]* tics$" "^cuentas: contraseña de Ana cambiada (por USUARIOS.BIN)$" "^  Ana  administrador, con contraseña, carpeta .USUARIOS.ANA  (usted)$"
mira "USUARIOS /NUEVO: Bea (normal, con contraseña) y Biblioteca (normal, sin contraseña, entra sola), cada una con su carpeta" \
  "^cuentas: USUARIOS.BIN da de alta a Bea$" "^  Bea  normal, con contraseña, carpeta .USUARIOS.BEA$" "^  Biblioteca  normal, sin contraseña, entra solo, carpeta .USUARIOS.BIBLIOTE$" \
  "^ANA  *<DIR>" "^BEA  *<DIR>" "^BIBLIOTE  *<DIR>"
mira "PAQUETES por Internet: el catálogo firmado con la serie 100 se usa (y se apunta); el de la serie 50, firmado igual, no (la vuelta atrás)" \
  "^La firma del catálogo es buena" "^La serie del catálogo: 100\.$" \
  "^paquetes: el catálogo de Internet es más viejo (serie 50) que el último que se ha usado (serie 100): no se usa$"
if sano $X/s1.img && saca $X/s1.img HISPA/USUARIOS.TXT $X/U1.TXT && [ "$(grep -c '^u=.*|argon2id\$12288\$3\$[0-9a-f]\{32\}\$[0-9a-f]\{64\}' $X/U1.TXT)" = 2 ] \
   && grep -q '^u=3|N|0|1|BIBLIOTE|Biblioteca|' $X/U1.TXT && python3 -c "import sys; d=open(sys.argv[1],'rb').read(); sys.exit(1 if b'uno2tres' in d or b'secreta' in d else 0)" $X/s1.img; then
  ok "desde fuera: el disco sano; USUARIOS.TXT con Ana y Bea resumidas con Argon2id (su sal y su resumen) y Biblioteca sin contraseña; ni rastro de las contraseñas en todo el disco"
else ko "USUARIOS.TXT o el disco no están como tienen que estar:"; sed 's/^/        /' $X/U1.TXT 2> /dev/null | head -5; fi
local lin; lin=$(grep '^u=1|' $X/U1.TXT | tr -d '\r' | sed 's/^.*|//')
local salA; salA=$(echo "$lin" | cut -d'$' -f4)
if [ -n "$salA" ] && [ "$($X/argon2_prueba --resume uno2tres $salA 12288 3)" = "$lin" ]; then ok "el resumen de Ana que ha hecho ARGON2.BIN en QEMU es el mismo que da Argon2id de Monocypher en Linux con su sal"
else ko "el resumen de Ana no es el de Linux: «$lin»"; fi
if saca $X/s1.img HISPA/PAQUETES/SERIE.TXT $X/serie.txt && [ "$(tr -d '\r\n' < $X/serie.txt)" = 100 ] && saca $X/s1.img HISPA/AZAR.SEM $X/azar1.sem && [ "$(stat -c %s $X/azar1.sem)" = 32 ] \
   && saca $X/s1.img USUARIOS/ANA/DOCUMENT/SECRETO.TXT $X/sec.txt && mdir -i $X/s1.img ::USUARIOS/BEA/AJUSTES > /dev/null 2>&1; then
  ok "y PAQUETES\\SERIE.TXT con la 100, la semilla del azar (AZAR.SEM, 32 octetos) y las carpetas (DOCUMENT, ESCRITOR, AJUSTES)"
else ko "falta SERIE.TXT (100), AZAR.SEM o las carpetas de los usuarios"; fi
# ---- 2. el arranque siguiente: entra sola Biblioteca ----
cp $X/s1.img $X/s2.img
ESCRIBE=1 SATA=$X/s2.img MEMORIA=128 MODULOS= ARRANQUE=ordenes \
  $AQUI/nucleo32/probar.sh $X/SEMILLA.BIN ":pausa 3" "espia" ":pausa 3" "admin" ":pausa 1" "paquetes instala nota" ":pausa 2" "sshd /encender" ":pausa 2" \
  "salir" ":pausa 2" "Ana" ":pausa 1" "mal" ":pausa 6" "Ana" ":pausa 1" "uno2tres" ":pausa 6" "ordenes" ":pausa 2" \
  "type \\usuarios\\ana\\document\\secreto.txt" ":pausa 1" "admin" ":pausa 1" "uno2tres" ":pausa 6" "type \\hispa\\usuarios.txt" ":pausa 1" "admin no" ":pausa 1" > $X/t2.sal 2>&1
tr -d '\r' < $X/t2.sal > $X/t2.txt
S=$X/t2.txt
mira "al arrancar entra sola Biblioteca (normal, sin contraseña)" "^cuentas: 3 usuarios (USUARIOS.TXT)$" "^Entra Biblioteca (sin contraseña)\.$" "^espia: soy el usuario 3; administrador: no$"
mira "Biblioteca no lee ni ve la carpeta de Ana, ni escribe en ella, ni borra ni se lleva lo suyo, ni hace carpetas en \\USUARIOS, ni lanza un programa de Ana: acceso denegado" \
  "^espia: leer el secreto de Ana: no (acceso denegado" "^espia: ver la carpeta de Ana: no (acceso denegado" "^espia: escribir en la carpeta de Ana: no (acceso denegado" \
  "^espia: borrar el secreto de Ana: no (acceso denegado" "^espia: llevarse el secreto de Ana: no (acceso denegado" "^espia: hacer una carpeta en .USUARIOS: no (acceso denegado" \
  "^espia: lanzar un programa de la carpeta de Ana: no (acceso denegado" "^espia: escribir en su carpeta: SE PUEDE$"
mira "ni lee USUARIOS.TXT ni AZAR.SEM, ni el disco en crudo, ni pone la hora, ni el modo administrador, ni las llamadas de USUARIOS.BIN, SSHD y ARGON2.BIN" \
  "^espia: leer USUARIOS.TXT: no (acceso denegado" "^espia: leer AZAR.SEM: no (acceso denegado" "^espia: leer el disco en crudo (la 64): no (acceso denegado" \
  "^espia: poner el reloj (la 67): no (acceso denegado" "^espia: el modo administrador (la 157): no (acceso denegado" "^espia: crear un usuario (la 201): no (" \
  "^espia: la contraseña de otro (la 202): no (" "^espia: el pedido de ARGON2 (la 209): no ("
mira "y desde ORDENES: ADMIN, PAQUETES INSTALA y SSHD /ENCENDER, solo un administrador" \
  "^Modo administrador: solo para un usuario administrador" "^paquetes: solo un administrador puede instalar" "^Eso solo lo puede hacer un administrador\.$"
mira "SALIR: sale; la entrada en texto (Usuario, Contraseña con asteriscos): con una contraseña mala, no; con la buena, Ana entra (y la consola del núcleo, para una administradora)" \
  "^cuentas: sale el usuario 3" "^Usuario: Ana$" "^La contraseña no es esa\.$" "^cuentas: entra Ana (el usuario 1)$" "^> ordenes$" "^secreto de Ana$"
mira "ADMIN: el núcleo pide la contraseña de Ana (no un Sí); con ella, el modo administrador lee USUARIOS.TXT" \
  "^Contraseña de Ana (Esc: no): \*\*\*\*\*\*\*\*$" "^permisos: modo administrador para ORDENES.BIN" "^u=2|N|0|0|BEA|Bea|argon2id"
sal1=$(sed -n 's/^espia: azar //p' $X/t2.txt)
if saca $X/s2.img HISPA/AZAR.SEM $X/azar2.sem && ! cmp -s $X/azar1.sem $X/azar2.sem; then ok "AZAR.SEM (la semilla del pozo del azar) cambia en cada arranque"
else ko "AZAR.SEM no cambia de un arranque a otro"; fi
if saca $X/s2.img USUARIOS/BIBLIOTE/DOCUMENT/MIO.TXT $X/m.txt && ! mdir -i $X/s2.img ::USUARIOS/ANA/DOCUMENT/MALO.TXT > /dev/null 2>&1 \
   && saca $X/s2.img USUARIOS/ANA/DOCUMENT/SECRETO.TXT $X/sec2.txt && ! mdir -i $X/s2.img ::USUARIOS/OTRO > /dev/null 2>&1 && sano $X/s2.img; then
  ok "y en el disco: lo de Biblioteca en su carpeta; el secreto de Ana, en su sitio; nada suyo en la de Ana; ni \\USUARIOS\\OTRO"
else ko "el disco no queda como tiene que quedar tras el arranque de Biblioteca"; fi
# otro arranque del mismo disco (con la misma semilla que tendrá el de SSH: el
# azar no debe salir igual aunque AZAR.SEM sea la misma)
cp $X/s2.img $X/s3.img
ESCRIBE=1 SATA=$X/s3.img MEMORIA=128 MODULOS= ARRANQUE=ordenes $AQUI/nucleo32/probar.sh $X/SEMILLA.BIN ":pausa 3" "espia" ":pausa 3" > $X/t3.sal 2>&1
local sal3; sal3=$(tr -d '\r' < $X/t3.sal | sed -n 's/^espia: azar //p')
# ---- 3. con el escritorio: la pantalla de entrada ----
cp $X/s2.img $X/e.img
saca $X/e.img HISPA/USUARIOS.TXT $X/U2.TXT && sed 's/|1|BIBLIOTE/|0|BIBLIOTE/' $X/U2.TXT > $X/U3.TXT && mcopy -o -i $X/e.img $X/U3.TXT ::HISPA/USUARIOS.TXT
CAPTURAS=$X ESCRIBE=1 SATA=$X/e.img MEMORIA=128 MODULOS= ARRANQUE=escritorio \
  $AQUI/nucleo32/probar.sh $X/SEMILLA.BIN ":pausa 6" ":tecla ctrl-alt-delete" ":tecla meta_l" ":pausa 1" ":captura ue1" ":escribe nopuede" ":tecla ret" ":pausa 5" ":captura ue2" \
  ":escribe uno2tres" ":tecla ret" ":pausa 6" ":doble 60 186" ":pausa 4" "admin" ":pausa 2" ":captura ue5" ":escribe uno2tres" ":tecla ret" ":pausa 5" \
  ":tecla meta_l-l" ":pausa 6" ":captura ue3" ":tecla right" ":escribe secreta" ":tecla ret" ":pausa 6" ":tecla meta_l" ":pausa 2" ":captura ue4" > $X/e.sal 2>&1
tr -d '\r' < $X/e.sal > $X/e.txt
S=$X/e.txt
miracap "con el escritorio, la pantalla de entrada: los tres usuarios, su tipo, la contraseña de la elegida, Entrar, Reiniciar y Apagar (Ctrl+Alt+Supr y el logo no la saltan)" ue1 \
  negrita 300 70 500 110 "Elija su usuario" texto 150 200 650 280 "Administrador" texto 150 200 650 280 "Biblioteca" negrita 150 290 600 320 "Contraseña de Ana:" \
  negrita 450 320 650 360 "Entrar" negrita 650 540 800 590 "Apagar"
miracap "con la contraseña mala: «La contraseña no es esa», y no entra" ue2 negrita 150 350 650 400 "La contraseña no es esa"
mira "con la buena entra Ana; cerrar la sesión (logo+L) vuelve a la pantalla; Bea, con la suya, entra" \
  "^entrada: la pantalla de entrada$" "^entrada: contraseña que no vale$" "^cuentas: entra Ana (el usuario 1)$" "^cuentas: sale el usuario 1" "^cuentas: entra Bea (el usuario 2)$"
miracap "ADMIN en la Consola de Ana: la ventanita del núcleo pide SU contraseña (no un «Sí»)" ue5 \
  negrita 190 230 420 260 "ORDENES.BIN pide el modo administrador" texto 190 280 420 300 "Contraseña de Ana:" negrita 380 360 470 400 "Aceptar"
mira "con la contraseña de Ana, ORDENES (en su Consola) tiene el modo administrador" "^permisos: modo administrador para ORDENES.BIN"
miracap "tras cerrar la sesión, otra vez la pantalla de entrada" ue3 negrita 300 70 500 110 "Elija su usuario"
miracap "y el menú Inicio saluda a quien ha entrado: «Hola, Bea»" ue4 negrita 0 90 400 130 "Hola, Bea"
if ! grep -q "Administrador de tareas\|excepción\|^\*\*\* " $S; then ok "sin el Administrador de tareas detrás de la pantalla de entrada, y sin quejas del núcleo"
else ko "el escritorio:"; grep "Administrador de tareas\|excepción\|^\*\*\* " $S | head -3 | sed 's/^/        /'; fi
# ---- 4. SSH con los usuarios ----
if command -v ssh > /dev/null && command -v ssh-keygen > /dev/null; then
  ssh-keygen -q -t ed25519 -N '' -f $X/clave -C ana
  cp $X/clave.pub $X/AUTORIZA.TXT
  cp $X/s2.img $X/h.img && mcopy -o -i $X/h.img $X/AUTORIZA.TXT ::USUARIOS/ANA/AJUSTES/AUTORIZA.TXT
  printf '#!/bin/sh\necho secreta\n' > $X/bea.sh; printf '#!/bin/sh\necho otra\n' > $X/mala.sh; printf '#!/bin/sh\necho\n' > $X/vacia.sh; chmod +x $X/bea.sh $X/mala.sh $X/vacia.sh
  PS=$((30000 + RANDOM % 10000))
  local O="-p $PS -o StrictHostKeyChecking=no -o UserKnownHostsFile=$X/conocidos -o UpdateHostKeys=no -o ConnectTimeout=30 -o NumberOfPasswordPrompts=1"
  ( sleep 70
    # shellcheck disable=SC2086
    SSH_ASKPASS=$X/bea.sh SSH_ASKPASS_REQUIRE=force DISPLAY=:0 setsid timeout 60 ssh $O -o PreferredAuthentications=password -o PubkeyAuthentication=no Bea@127.0.0.1 \
      'usuarios /lista' < /dev/null > $X/s_bea.txt 2>&1; echo "código $?" >> $X/s_bea.txt
    # shellcheck disable=SC2086
    SSH_ASKPASS=$X/bea.sh SSH_ASKPASS_REQUIRE=force DISPLAY=:0 setsid timeout 60 ssh $O -o PreferredAuthentications=password -o PubkeyAuthentication=no Bea@127.0.0.1 \
      'type \usuarios\ana\document\secreto.txt' < /dev/null > $X/s_bea2.txt 2>&1; echo "código $?" >> $X/s_bea2.txt
    # shellcheck disable=SC2086
    SSH_ASKPASS=$X/mala.sh SSH_ASKPASS_REQUIRE=force DISPLAY=:0 setsid timeout 60 ssh $O -o PreferredAuthentications=password -o PubkeyAuthentication=no Bea@127.0.0.1 \
      'ver' < /dev/null > $X/s_mala.txt 2>&1; echo "código $?" >> $X/s_mala.txt
    # shellcheck disable=SC2086
    SSH_ASKPASS=$X/vacia.sh SSH_ASKPASS_REQUIRE=force DISPLAY=:0 setsid timeout 60 ssh $O -o PreferredAuthentications=password -o PubkeyAuthentication=no Biblioteca@127.0.0.1 \
      'ver' < /dev/null > $X/s_bib.txt 2>&1; echo "código $?" >> $X/s_bib.txt
    # shellcheck disable=SC2086
    timeout 60 ssh $O -o IdentitiesOnly=yes -o BatchMode=yes -i $X/clave Ana@127.0.0.1 'type \usuarios\ana\document\secreto.txt' < /dev/null > $X/s_ana.txt 2>&1; echo "código $?" >> $X/s_ana.txt
  ) &
  FUERA=$!
  ESCRIBE=1 SATA=$X/h.img MEMORIA=128 MODULOS= ARRANQUE=ordenes QEMU_MAS="-netdev user,id=n0,hostfwd=tcp:127.0.0.1:$PS-:22 -device e1000,netdev=n0" \
    $AQUI/nucleo32/probar.sh $X/SEMILLA.BIN ":pausa 3" "espia" ":pausa 3" "salir" ":pausa 2" "Ana" ":pausa 1" "uno2tres" ":pausa 6" "ordenes" ":pausa 2" "sshd /encender" ":pausa 150" > $X/h.sal 2>&1
  wait $FUERA
  tr -d '\r' < $X/h.sal > $X/h.txt
  S=$X/h.txt
  mira "SSHD encendido por una administradora (Ana), con los usuarios" "^Servidor SSH encendido (puerto 22)" "red: tcp: escuchando en el puerto 22$"
  sal2=$(sed -n 's/^espia: azar //p' $X/h.txt)
  if [ ${#sal1} = 64 ] && [ ${#sal2} = 64 ] && [ ${#sal3} = 64 ] && [ "$sal1" != "$sal2" ] && [ "$sal3" != "$sal2" ] && [ "$sal1" != "$sal3" ]; then
    ok "el azar del núcleo (la llamada 210) no se repite: tres arranques, tres distintos (${sal1:0:12}..., ${sal3:0:12}..., ${sal2:0:12}...), también dos con la misma AZAR.SEM"
  else ko "el azar se repite o no sale: «$sal1», «$sal3» y «$sal2»"; fi
  mira "SSHD: Bea entra con su contraseña (la comprueba el núcleo: Argon2id con UTF-8) y Ana con la llave de su carpeta; la contraseña mala y Biblioteca (sin contraseña), no" \
    "^cuentas: SSH: entra Bea (con su contraseña)$" "^sshd: entra Bea con la contraseña$" "^sshd: contraseña que no vale (Bea)$" "^sshd: contraseña que no vale (Biblioteca)$" \
    "^cuentas: SSH: entra Ana (con su llave)$" "^sshd: entra Ana con su llave$"
  S=$X/s_bea.txt
  mira "por SSH, el intérprete es de Bea («(usted)» en su renglón)" "^  Bea  normal, con contraseña, carpeta .USUARIOS.BEA  (usted)$" "^código 0$"
  S=$X/s_bea2.txt
  mira "y Bea, por SSH, no lee la carpeta de Ana" "acceso denegado"
  if grep -q "^código 255$" $X/s_mala.txt && grep -q "^código 255$" $X/s_bib.txt; then ok "con la contraseña mala, o como Biblioteca sin contraseña, ssh no entra (código 255)"
  else ko "ssh ha entrado con lo que no vale: $(tail -n 1 $X/s_mala.txt) / $(tail -n 1 $X/s_bib.txt)"; fi
  S=$X/s_ana.txt
  mira "Ana, con la llave de su \\USUARIOS\\ANA\\AJUSTES\\AUTORIZA.TXT, entra y lee lo suyo" "^secreto de Ana$" "^código 0$"
else echo "  (sin ssh de OpenSSH aquí: sin la prueba de SSH con los usuarios)"; fi
kill $SPID 2> /dev/null
# (las quejas del núcleo empiezan por «*** »; los asteriscos de las contraseñas, no)
if ! cat $X/t1.txt $X/t2.txt $X/t3.sal $X/e.txt | grep -q "excepción\|^\*\*\* "; then ok "en esos arranques, ni una excepción ni una queja del núcleo"
else ko "el núcleo se ha quejado:"; cat $X/t1.txt $X/t2.txt $X/t3.sal $X/e.txt | grep "excepción\|^\*\*\* " | head -3 | sed 's/^/        /'; fi
}
