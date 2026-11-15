# Consola remota de Hispa-DOS (CONSOLA)

`CONSOLA` deja que otra persona entre **por la red** a este equipo para
trabajar o diagnosticar, con el permiso del dueño en cada conexión. No es una
puerta trasera: la lanza el usuario **a mano**, **no arranca sola** ni se
instala por omisión (no va en `AUTOEXEC.BAT`), **avisa en la pantalla** de cada
conexión, exige **autorización humana por un código**, y se cierra cuando el
usuario quiere. Todo el código está en el proyecto (abierto): es material de
diagnóstico y de trabajo controlado por el dueño del equipo.

```
A:\>HSPRED
A:\>CONFRED
A:\>CONSOLA

Consola remota de Hispa-DOS ACTIVADA.
  IP: 10.0.2.15  Puerto: 2323
  Abra ese puerto TCP en el router hacia este equipo para entrar desde fuera.
Esperando conexión...  (Esc para cerrar)

Conexión entrante desde 10.0.2.2. Código de acceso: 537999 (válido 60 s). Dígaselo a quien se conecta.
CONECTADO desde 10.0.2.2. Esc para cerrar la sesión.
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
CONSOLA [/PUERTO:n]
```

1. Cargar el controlador de red y configurarla: `HSPRED` y `CONFRED` (ver
   [docs/RED.md](RED.md)). Si no, CONSOLA lo dice y sale:
   «Cargue HSPRED y configure la red (CONFRED) antes de usar CONSOLA».
2. `CONSOLA` escucha en el puerto TCP **2323** (se cambia con `/PUERTO:n`) y
   enseña la IP del equipo y el puerto. Se queda esperando una conexión.
3. **Para entrar desde dentro de la misma red** (otro PC de casa o de la
   oficina), quien se conecta usa `telnet <IP> 2323` (o `nc`, o PuTTY en modo
   «raw»/«telnet»).
   **Para entrar desde fuera de la red** (por Internet), el usuario abre en su
   **router** el puerto 2323 (TCP) hacia la IP de este equipo
   («port forwarding»/«redirección de puertos»); entonces se conecta a la IP
   pública del router.
4. En cada conexión, CONSOLA genera un **código de 6 cifras** y lo enseña en la
   pantalla de ESTE equipo. El dueño se lo dice (por teléfono, por mensaje...)
   a quien se conecta. Sin ese código, no se entra.
5. Dentro, se escriben órdenes como en el intérprete (`DIR`, `VER`, `TYPE`,
   `COPY`, `CD`, cambiar de unidad...). `SALIR` cierra la sesión.
6. En cualquier momento, **Esc** en el teclado del equipo cierra la sesión (y,
   si no hay sesión, cierra CONSOLA y suelta el puerto).

## La seguridad por código

- Por **cada** conexión que llega, CONSOLA genera un **código único de 6
  cifras**, aleatorio (sembrado con el reloj del equipo y el puerto de origen),
  **válido 60 segundos** desde que se genera.
- El código se enseña **solo en la pantalla del equipo** («Código de acceso:
  123456»). Nunca se manda por la red antes de que se introduzca. El dueño
  decide a quién se lo dice.
- A quien se conecta, CONSOLA le pide: «Introduzca el código de acceso:».
- Si el código **coincide y no ha caducado**, se concede el acceso. Si no
  coincide, caduca, o no hay un código vigente, dice «Código no válido» y, tras
  **3 intentos**, corta la conexión. **Sin un código válido y vigente, ninguna
  conexión entra.**
- Cada conexión nueva genera un código **nuevo** (no se reutiliza el anterior).

Así, aunque el puerto esté abierto en el router, nadie entra sin que el dueño,
delante del equipo, lea el código del momento y se lo dé. Es el dueño quien
manda: con Esc corta cuando quiere.

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
pide el código; que el código que se enseña en la pantalla del equipo (que sale
también por el puerto serie, como toda la consola del invitado) deja entrar y
ejecuta `VER`; que `SALIR` cierra la sesión; que una segunda conexión genera
**otro** código; y que un código erróneo se rechaza y, tras los intentos,
corta. El código mostrado se lee del puerto serie del invitado (no se añade
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
