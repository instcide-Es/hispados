# Consola remota de Hispa-DOS (CONSOLA)

`CONSOLA` deja que otra persona entre **por la red** a este equipo para
trabajar o diagnosticar, con el permiso del dueño en cada conexión. No es una
puerta trasera: la lanza el usuario **a mano**, **no arranca sola** ni se
instala por omisión (no va en `AUTOEXEC.BAT`), **avisa en la pantalla** de cada
conexión, exige **autorización humana por un código** que solo se ve en la
pantalla del equipo, y se cierra cuando el usuario quiere. Todo el código está en el proyecto (abierto): es material de
diagnóstico y de trabajo controlado por el dueño del equipo.

```
A:\>HSPRED
A:\>CONFRED
A:\>CONSOLA

Consola remota de Hispa-DOS ACTIVADA.
  IP: 10.0.2.15  Puerto: 2.323
  Abra ese puerto TCP en el router hacia este equipo para entrar desde fuera.
Esperando conexión...  (Esc para cerrar)
Código de acceso para entrar (de un solo uso; dígaselo a quien se conecta):
  Código de acceso: 537999   caduca en 9:41
```

El código está en la pantalla **desde el principio**, antes de que llegue
nadie: se le puede pasar de una vez a quien va a entrar, sin esperar a que se
conecte. La última línea se repinta en su sitio cada segundo con lo que le
queda. Cuando llega alguien:

```
[red] Conexión TCP establecida.
Conexión entrante desde 10.0.2.2: pide el código de acceso.
CONECTADO desde 10.0.2.2. Esc para cerrar la sesión.
Sesión cerrada. Esperando conexión...  (Esc para cerrar)
El código anterior ya se ha usado. Para la próxima conexión:
  Código de acceso: 806114   caduca en 10:00
```

Y lo que ve quien se conecta (con `telnet`, `nc`, PuTTY en modo «raw»...):

```
Hispa-DOS CONSOLA. Introduzca el código de acceso: 537999
Acceso concedido. Escriba SALIR para terminar.
A:\>VER

Hispa-DOS versión 0.9 (compatible con MS-DOS 6.22)

A:\>DIR
 ...
A:\>SALIR
Hasta luego.
```

## Uso

```
CONSOLA [/PUERTO:n] [/CADUCA:s]
```

1. Cargar el controlador de red y configurarla: `HSPRED` y `CONFRED` (ver
   [docs/RED.md](RED.md)). Si no, CONSOLA lo dice y sale:
   «Cargue HSPRED y configure la red (CONFRED) antes de usar CONSOLA».
2. `CONSOLA` escucha en el puerto TCP **2323** (se cambia con `/PUERTO:n`) y
   enseña la IP del equipo y el puerto, y el **código de acceso** vigente con
   lo que le queda. Se queda esperando una conexión. `/CADUCA:s`: los
   segundos que vale cada código (600, 10 minutos, si no se dice; de 5 a
   86.400).
3. **Para entrar desde dentro de la misma red** (otro PC de casa o de la
   oficina), quien se conecta usa `telnet <IP> 2323` (o `nc`, o PuTTY en modo
   «raw»/«telnet»).
   **Para entrar desde fuera de la red** (por Internet), el usuario abre en su
   **router** el puerto 2323 (TCP) hacia la IP de este equipo
   («port forwarding»/«redirección de puertos»); entonces se conecta a la IP
   pública del router.
4. El **código de 6 cifras** está en la pantalla de ESTE equipo desde que se
   lanza CONSOLA. El dueño se lo dice (por mensaje, por teléfono...) a quien
   va a entrar, cuando quiera, aunque no se haya conectado todavía. Sin ese
   código, no se entra.
5. Dentro, se escriben órdenes como en el intérprete (`DIR`, `VER`, `TYPE`,
   `COPY`, `CD`, cambiar de unidad...). `SALIR` cierra la sesión.
6. En cualquier momento, **Esc** en el teclado del equipo cierra la sesión (y,
   si no hay sesión, cierra CONSOLA y suelta el puerto).

## La seguridad por código

- Siempre hay **un código vigente de 6 cifras** (100000 a 999999: 900.000
  posibles) y se enseña **solo en la pantalla del equipo** («Código de acceso:
  123456   caduca en 9:41»). Nunca se manda por la red. El dueño decide a
  quién se lo dice.
- A quien se conecta, CONSOLA le pide: «Introduzca el código de acceso:».
  Si coincide con el vigente, entra.
- **Un solo uso.** En cuanto alguien entra, ese código deja de valer; al
  acabar la sesión sale otro en la pantalla («El código anterior ya se ha
  usado. Para la próxima conexión:»).
- **3 intentos fallidos y se cambia.** Cada código erróneo se apunta en la
  pantalla del equipo («(!) Código erróneo desde 1.2.3.4 (1 de 3 con este
  código)») y se contesta a los 2 segundos. Al tercer fallo con el mismo
  código —en una sola conexión **o sumando conexiones seguidas**— CONSOLA corta,
  enseña **otro código** y hace una **pausa** sin aceptar conexiones («Espere
  y vuelva a probar más tarde»): 30 segundos la primera vez y **el doble**
  cada vez que se vuelve a quemar un código sin que nadie entre (1 minuto, 2,
  4... hasta 16 minutos); cuando alguien entra, vuelve a 30 s. Así nadie
  puede probar de 3 en 3 contra el mismo código, ni a toda prisa contra los
  siguientes.
- **Caduca solo.** Cada código vale **10 minutos** (o lo que diga
  `/CADUCA:s`) aunque nadie se conecte; al caducar sale otro en la pantalla
  («El código anterior ha caducado. El nuevo:»). Si caduca mientras alguien
  lo está tecleando, se le dice «Código no válido» y se le da el nuevo.
- **Azar.** El código no sale del reloj sin más: CONSOLA va echando a un
  estado de 64 bits, en cada vuelta de sus bucles de espera, el contador del
  temporizador del PC (PIT, 1,19 millones de vueltas por segundo), el reloj
  de la BIOS, el contador de ciclos del procesador (`RDTSC`, si es un Pentium
  o posterior) y, en cada conexión, la IP y el puerto de quien llama; lo
  mezcla con los finalizadores de MurmurHash3 y saca el código sin sesgo
  (descarta lo que no cabe en un múltiplo entero de 900.000). Nunca repite el
  anterior.
- **Comparación sin atajos.** Se miran siempre las 6 cifras enteras (no se
  para en la primera distinta, así que el tiempo de la respuesta no dice
  cuántas se han acertado), y sin código vigente no vale nada.

**Las cuentas.** Con 3 intentos por código entre 900.000, cada código
quemado da 1 posibilidad entre 300.000. Con la pausa ya en sus 16 minutos,
quien probara sin parar quemaría unos 4 códigos por hora: haría falta del
orden de **8 años** de media para acertar (sin la pausa que crece, con 30 s
fijos, serían unos 4 meses), y todo queda apuntado en la pantalla del
equipo, con el código cambiando delante del dueño. Es el dueño quien manda: con Esc corta cuando quiere.

## Qué se puede hacer dentro (y qué no)

Esta primera versión es una **consola de órdenes**: CONSOLA ejecuta cada línea
con `%COMSPEC% /C <orden>` (el intérprete, `COMMAND.COM`), guarda su salida en
un archivo temporal (`CONSOLA.OUT`, en la raíz de la unidad de arranque) y la
manda por la conexión. Así el remoto ve lo que imprime la orden.

- Funcionan las órdenes que escriben su resultado en la pantalla: `DIR`, `VER`,
  `TYPE`, `MEM`, `SET`, `COPY`, `BUSCAR`, etc.
- `CD` (y `CHDIR`) y el cambio de unidad (`C:`, `D:`...) los maneja CONSOLA
  directamente, para mantener la carpeta y la unidad actuales entre órdenes; el
  indicador (`C:\CARPETA>`) las refleja.
- Órdenes propias de la consola: `SALIR` (o `EXIT`) cierra la sesión (no apaga
  el equipo); una línea en blanco solo repite el indicador.
- **No funcionan** los programas de **pantalla completa** ni interactivos
  (`EDITAR`, Hispa Ventanas, menús, juegos): esta versión solo captura la
  salida de texto de una orden, no redirige la pantalla y el teclado enteros.
  Eso es trabajo futuro. Tampoco se captura la salida de error (el `COMMAND.COM`
  de MS-DOS no tiene `2>&1`): va a la pantalla del equipo.

## Puerto y router

- Puerto por omisión: **2323/TCP** (`/PUERTO:n` para otro).
- Desde la **misma red**: basta con conectarse a la IP del equipo en ese
  puerto.
- Desde **fuera** (Internet): hay que abrir ese puerto en el **router** hacia
  este equipo («redirección de puertos»). CONSOLA lo recuerda en pantalla. Como
  cada conexión necesita el código que solo se ve aquí, abrir el puerto no
  abre el equipo.

## Pruebas

`tests/run_consola_tests.sh` (en `make test`) arranca Hispa-DOS en QEMU con
HSPRED y la red de usuario de QEMU, reenviando el puerto 2323 del invitado a un
puerto local (`-netdev user,hostfwd=tcp:127.0.0.1:<puerto>-:2323`), dentro de un
espacio de nombres de red propio (`unshare -rn`: no hace falta Internet ni se
toca la red del equipo). Desde `tests/consola_cliente.py` comprueba que:
el código sale en la pantalla del equipo **antes** de que llegue nadie, con
«caduca en»; que deja entrar, ejecuta `VER` y `SALIR` cierra la sesión; **un
solo uso** (al acabar sale otro y el usado ya no vale); que **3 fallos
sumando dos conexiones** cortan, cambian el código, dejan 30 s sin aceptar
conexiones (la primera pausa) y, pasados, el nuevo deja entrar; que Esc cierra CONSOLA; y la
**caducidad** con `CONSOLA /CADUCA:15`: sin que llegue nadie sale otro
código, el caducado no vale y el nuevo sí. El código mostrado se lee del puerto serie del invitado (no se añade
ningún modo especial: toda la consola del equipo sale ya por ahí en las
pruebas).

## Cómo funciona por dentro

La pila TCP/IP de [`lib/red.inc`](../lib/red.inc) ya hacía de **cliente**
(`tcp_abrir`). Para CONSOLA se le añadió el lado **servidor**:

- `tcp_escuchar BX=puerto`: apertura pasiva. Deja la conexión en estado
  **LISTEN**. Vuelve enseguida; el programa sondea con `red_sondear` y mira
  `tcp_estado` (2 = establecida).
- Al llegar un SYN a ese puerto (de cualquiera), el residente guarda el extremo,
  contesta **SYN+ACK** y pasa a **SYN-RECEIVED**; con el ACK del cliente, a
  **establecida**. A partir de ahí, `tcp_enviar`, la recepción (por la rutina
  `[t_rutina]`) y `tcp_cerrar` son iguales que en el cliente.
- **Una sola conexión a la vez**: mientras hay una (o a medio establecer), un
  SYN nuevo a ese puerto se descarta (el cliente reintenta más tarde).

CONSOLA **no se queda residente**: es un `.COM` normal que, al salir, suelta el
puerto y los tipos del packet driver y deja todo como estaba.
