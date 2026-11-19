# Wi-Fi en Hispa-486

Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.

Red inalámbrica con **WPA2-Personal** para Hispa-486, empezando por la tarjeta
**Broadcom BCM43224** (PCI `14E4:4353`) de los HP Mini 5103 y del MacBook Air
de 2012. El controlador se presenta como un *packet driver* Ethernet, igual
que `HSPRED`: `CONFRED`, `PING`, `DESCARGAR`, `ACTUALIZAR`, `CONSOLA` y lo
demás funcionan sin cambios.

> **Estado (18/11/2026): sin probar en la tarjeta de verdad.** Todo lo que no
> es la tarjeta está probado (ver «Pruebas»): la criptografía con los vectores
> de las normas, la gestión 802.11 y WPA2 contra un punto de acceso de
> mentira, el residente y el paso a modo protegido en QEMU, y el controlador
> de la Broadcom contra un modelo de la tarjeta. Falta lo único que no se
> puede simular: que la radio de verdad arranque y emita. La imagen de prueba
> para el HP Mini deja un registro detallado para eso (ver «Prueba en el
> equipo de verdad»).

```
C:\>HSPWIFI
HSPWIFI 0.1: controlador Wi-Fi de Hispa-486 (packet driver)
Tarjeta Wi-Fi Broadcom BCM43224, MAC 00:26:82:xx:xx:xx, packet driver en INT 60h
Para elegir la red: WIFI REDES y WIFI CONECTAR nombre

C:\>WIFI REDES
Buscando redes...

N.º  Red                              Canal  Señal         Seguridad
  1  Biblioteca                         6  ████ -47 dBm  WPA2
  2  Vecina abierta                    11  ██░░ -71 dBm  abierta

Para conectar: WIFI CONECTAR número  (o el nombre)

C:\>WIFI CONECTAR 1
Contraseña de la red: ***************
Preparando la clave.......
Conectando con «Biblioteca»...
Conectado a «Biblioteca» (señal -47 dBm).
La red queda guardada en RED.INI. Ahora: CONFRED

C:\>CONFRED
  Tarjeta . . . . . . . : Wi-Fi Broadcom BCM43224 (INT 60h)
  Dirección IP  . . . . : 192.168.1.37
  ...
```

## Uso

1. `HSPXMS.SYS` en `CONFIG.SYS` (ya está) y **sin HSPEMM** (configuración
   «Normal»): ver «Límites».
2. `HSPWIFI` (a mano o en `AUTOEXEC.BAT`). Busca la tarjeta, carga su núcleo
   y el microcódigo y enciende la radio (un par de segundos).
3. `WIFI REDES` y `WIFI CONECTAR red` la primera vez. La red y su clave
   quedan en `RED.INI`; desde entonces basta `WIFI /AUTO` en `AUTOEXEC.BAT`
   (las dos líneas vienen con `REM` en el `AUTOEXEC.BAT` que se instala).
4. `CONFRED` (DHCP) y lo demás, como con cable.

| Orden | Para qué |
|---|---|
| `HSPWIFI [/V] [/PANTALLA] [/REG:archivo] [/INT:xx] [/JUNTO]` | El controlador. Si `HSPRED` está cargado sin enlace (sin cable), lo descarga y sigue; con enlace no se carga (salvo `/JUNTO`). `/V` enseña el registro de la puesta en marcha; `/PANTALLA` lo escribe además directo en la pantalla mientras trabaja (se ve aunque el equipo se quede colgado); `/REG:archivo` lo guarda en un archivo (se añade al final, y `WIFI` va añadiendo lo nuevo cada vez que se usa). `/INT:xx`: el vector del packet driver. `/JUNTO`: cargarse al lado de `HSPRED` (para depurar el Wi-Fi por la consola remota de la red por cable: las órdenes de red siguen yendo por el cable y `HSPWIFI` deja `INT 2Fh AH=DAh` a `HSPRED`). `/U`: descargarlo. `/SIMULA`: una radio de mentira con una red «HispaPrueba» (para las pruebas). |
| `HSPWIFI /AUTO` | Para el `AUTOEXEC.BAT` (el de Hispa-486 la lleva: docs/486/HISPA-486.md, «La red al arrancar»). Solo se carga si hay una tarjeta admitida en el PCI (lo pregunta a la BIOS, sin cargar el núcleo) y no se ha quedado la red por cable; si no, sale en silencio con nivel de error 1. Desde la 0.9.28 **no hace falta una red guardada** (así, sin cable, la ventanita de Hispa Ventanas puede buscar las redes) y respeta `Preferir=` de `RED.INI` (docs/RED.md, «Cable o Wi-Fi»): con `CABLE` no quita `HSPRED`; con `WIFI` lo quita aunque tenga enlace; con `AUTO`, si `HSPRED` está cargado y su tarjeta no tiene enlace (3 segundos de espera), lo descarga y se carga; con enlace, o si no se sabe, se queda el cable. Si no puede quitarlo (otro residente encima), lo dice y cómo arreglarlo. |
| `WIFI` | Cómo está la conexión: red, canal, señal, punto de acceso, paquetes, velocidad y, si no hay conexión, por qué. |
| `WIFI REDES` | Busca (unos 2 segundos) y enseña las redes, con su señal y su seguridad. |
| `WIFI CONECTAR red [clave]` | Conecta. «red» es el nombre (entre comillas si lleva espacios) o su número en la lista de `WIFI REDES` (lo más cómodo si el nombre lleva eñes o símbolos). Si la red pide contraseña y no se da, la pregunta **sin enseñarla** (asteriscos). Al conectar la guarda en `RED.INI`. |
| `WIFI DESCONECTAR` | Corta la conexión (hasta el próximo `WIFI /AUTO` o `CONECTAR`). |
| `WIFI OLVIDAR` | La corta y quita la red de `RED.INI`. |
| `WIFI /AUTO` | Conecta con la red guardada. Sin red guardada o sin `HSPWIFI` no dice nada (para `AUTOEXEC.BAT`). Si no conecta a la primera, el controlador lo sigue intentando solo. |
| `WIFI REGISTRO [archivo]` | Guarda el registro del controlador, con un resumen del estado (si no se dice el archivo: el de `/REG:` o `\WIFI.LOG` de la unidad de arranque). |

### RED.INI

Dos claves más en `\HISPADOS\RED.INI` (las escribe `WIFI CONECTAR`; `INSTALAR`
y `ACTUALIZAR` conservan el archivo):

| Clave | Valor |
|---|---|
| `WIFI=` | el nombre de la red |
| `WIFICLAVE=` | 64 cifras hexadecimales: la clave **ya preparada** (la PMK de WPA2: PBKDF2 de la contraseña y el nombre de la red). No se guarda la contraseña, ni se enseña nunca. Sin esta línea, la red es abierta. |

Guardar la clave preparada sirve para dos cosas: la contraseña no queda
escrita en el disco (la PMK solo vale para esa red; ojo: quien la copie puede
entrar en esa red, así que `RED.INI` sigue siendo un archivo que cuidar) y no
hay que recalcularla en cada arranque (8.192 HMAC-SHA1: medio segundo en un
Atom, bastante más en un 486). `WIFI CONECTAR red clave` admite también esas
64 cifras en vez de la contraseña.

### En Hispa Ventanas (Hispa-486): el icono de red y su ventanita

En el escritorio de Hispa-486 (`VENTANAS /486`), el **icono de red** de la
barra de tareas (a la izquierda del altavoz) dice cómo está la red **por la
forma**, no solo por el color:

| Icono | Qué quiere decir |
|---|---|
| dos nodos huecos y un aspa | sin red: no hay ningún controlador cargado |
| dos nodos llenos y un aspa | cable (HSPRED) sin enlace |
| dos nodos llenos unidos | cable (HSPRED u otro *packet driver*) |
| cuatro rayitas de alto creciente | Wi-Fi conectado: llenas, las de la señal (de 1 a 4, como en `WIFI REDES`); huecas, las demás |
| las cuatro rayitas huecas | Wi-Fi conectando |
| las cuatro huecas y un aspa | Wi-Fi sin conectar |

**Un clic** en el icono abre (o cierra) una ventanita encima de la barra,
como la del volumen:

- **Con HSPWIFI**: arriba, cómo está la conexión («Conectado a «red»» con la
  señal en rayitas, «Conectando con...», «Buscando redes...», «Wi-Fi: sin
  conectar») y debajo la señal en dBm y la IP, o por qué no conecta, o lo
  que toca hacer. Después, **la lista de redes** (la de más señal arriba;
  la conectada, en negrita) con su señal en rayitas y su seguridad
  («WPA2», «abierta» o «no admitida»), que se maneja con el ratón o con
  las flechas (doble clic o Intro: conectar). Al abrirla, si no se conoce
  ninguna red, busca sola. El cuadro **«Contraseña»** enseña asteriscos.
  Botones: **«Buscar»**, **«Conectar»** (si la red es WPA2 y no se escribe
  contraseña, usa la de `RED.INI` si es esa red; si no, la pide),
  **«Desconectar»**, **«Olvidar»** (desconecta y quita la red de
  `RED.INI`) y **«Configuración de red...»**.
- **Con HSPRED**: el enlace (conectado, «no hay cable enchufado» o «no se
  sabe»), la dirección IP y la puerta de enlace (o «todavía sin dirección»).
  Sin cable enchufado y con tarjeta Wi-Fi: «Para usar el Wi-Fi, elija
  «Wi-Fi».».
- **Sin red**: «No hay ninguna red en marcha.» y, si el equipo tiene una
  tarjeta Wi-Fi de las admitidas, «Para usar el Wi-Fi, elija «Wi-Fi».» (si
  no, «Enchufe el cable y reinicie el equipo.»).
- Siempre, **«Usar: Automático / Cable / Wi-Fi»** (desde la 0.9.28; docs/
  RED.md, «Cable o Wi-Fi»): marcada la de `Preferir=` de `RED.INI`. Al
  elegir otra la guarda y, si para usarla hace falta otro controlador
  (HSPRED y HSPWIFI solo se ponen al arrancar), pregunta «Hace falta
  reiniciar para usar el Wi-Fi. ¿Reiniciar ahora?» (o «...el cable»):
  **Sí** reinicia como «Reiniciar» del cuadro «Salir»; **No** deja el aviso
  «Cambiará la próxima vez que arranque.». Si no hace falta, lo dice («Ya
  se está usando el Wi-Fi.», «Guardado: al arrancar elegirá solo.»); sin
  tarjeta Wi-Fi, «Este equipo no tiene Wi-Fi.».
- Y **«Configuración de red...»**: abre Configuración en el apartado «Red».

Lo que hace es lo mismo que la orden `WIFI`, por la misma interfaz del
residente (`INT 2Fh AX=DC01h` con las funciones de
`wifi/nucleo/llamadas.h`: buscar, lista, conectar, desconectar, estado y
preparar la clave por trozos); **no ha hecho falta ninguna llamada nueva en
HSPWIFI**. Al conectar guarda en `RED.INI` `WIFI=` y `WIFICLAVE=` (la clave
preparada, nunca la contraseña) y borra la configuración IP de la red
anterior, como `WIFI CONECTAR`; la dirección IP se pide luego con `CONFRED`
(la segunda línea lo recuerda: «sin IP: falta CONFRED»). La contraseña
escrita se borra de la memoria en cuanto se prepara la clave (o al cerrar
la ventanita). Lo que tarda (buscar, unos 2 s; preparar la clave, 8.192
HMAC-SHA1; conectar) no para el escritorio: se lleva a trozos en cada vuelta
de la barra; si se cierra la ventanita conectando, la conexión sigue y se
guarda igual. Ver `ventanas/wifi.inc` y docs/VENTANAS.md.

Configuración (`CONFIG.HVP`, que está en marcha todo el rato) vuelve a leer
`WIFI=` y `WIFICLAVE=` del disco justo antes de escribir `RED.INI`: así no
deshace lo que haya cambiado la ventanita o la orden `WIFI` mientras tanto.

En Hispa-386 (sin barra de tareas) no cambia nada: el icono de la barra de
arriba sigue siendo el de siempre y no se pulsa.

### Lo que admite

- Redes **abiertas** y con **WPA2-Personal (clave compartida) con AES/CCMP**:
  lo que trae cualquier router desde 2006.
- 2,4 GHz, canales 1 a 13 (el 12 y el 13, solo tras oír una baliza), 802.11b/g
  (hasta 54 Mbit/s). Sin 802.11n ni 5 GHz por ahora (la tarjeta los tiene; el
  controlador de la radio también: falta la parte de gestión).
- **No**: WEP, WPA antigua (TKIP), WPA2 en modo mixto con la clave de grupo
  TKIP, WPA2-Empresa (802.1X), WPA3 solo. `WIFI REDES` lo dice en cada red y
  `WIFI CONECTAR` explica por qué no.
- Reconexión: si el punto de acceso nos echa o deja de oírse 8 segundos, busca
  y vuelve a conectar solo (cada 5 segundos), con claves nuevas. `WIFI`
  cuenta las reconexiones.

## Decisiones

### El controlador de la radio: brcmsmac de Linux, tal cual

La BCM43224 es una tarjeta *softMAC*: la tarjeta lleva la radio y un
procesador pequeño con su microcódigo para lo urgente (acuses, tiempos), y
todo lo demás (poner en marcha el PHY de 802.11n, calibrar la radio, las
colas de DMA) lo hace el controlador. Solo esa parte son unas 60.000 líneas
(la puesta en marcha del PHY N, 28.000, con 10.000 de tablas).

Hay dos controladores libres:

| | `brcmsmac` | `b43` |
|---|---|---|
| De quién | Broadcom (2010), mantenido en Linux | ingeniería inversa (comunidad) |
| Licencia | **ISC** (compatible con la GPL) | GPL-2.0 |
| BCM43224 | sí: es una de las tres tarjetas para las que se escribió | sí, parcial (PHY N rev. 6), menos probado |
| Microcódigo | `brcm/bcm43xx-0.fw` de **linux-firmware**: se puede repartir | sacado del controlador de Windows con `b43-fwcutter`: **no se puede repartir** |

Se usa **brcmsmac**. Y no reescrito: sus fuentes de Linux 5.10 van **sin
tocar** en `wifi/linux/` y se compilan contra una capa propia que les da lo
que esperan del núcleo de Linux (`wifi/compat/`). Reescribir 60.000 líneas de
calibración de radio sin poder probarlas en la tarjeta no tenía sentido; así,
el código que toca la tarjeta es el mismo que lleva años funcionando en
Linux, y lo nuestro (lo que puede fallar) es una capa pequeña y conocida.

Con brcmsmac hace falta también **bcma**, el bus interno de estos chips
(leer los núcleos del chip, el ChipCommon, el núcleo PCIe y la SPROM con la
MAC y las potencias de fábrica). Sus fuentes (`drivers/bcma` de Linux) son
**GPL-2.0** (el núcleo de Linux es «solo versión 2»).

> **Para Carlos — licencias.** Hispa-DOS es «GPL-2.0 o posterior». Lo de
> `wifi/linux/` conserva sus licencias: brcmsmac y brcmutil, ISC (sin
> problema); bcma, GPL-2.0 **sin** «o posterior». Consecuencia: `HSPWIFI.BIN`
> (el binario que los junta) se reparte bajo GPL-2.0, que es compatible con
> cómo se reparte el resto; lo único que se pierde es poder pasar *ese
> binario* a GPL-3.0 en el futuro. Si eso importara, bcma son unas 2.500
> líneas que se pueden reescribir (lo que usa esta tarjeta es mucho menos).
> Es la primera vez que entra código de otros en el proyecto: está aparte, en
> `wifi/linux/`, con su procedencia en `wifi/LEEME.md`.

### El microcódigo

Dos archivos de Broadcom, de [linux-firmware](https://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git)
(`brcm/bcm43xx-0.fw`, 96.224 octetos, y `brcm/bcm43xx_hdr-0.fw`, 180; versión
610.812), con su licencia `LICENCE.broadcom_bcm43xx`: permite **usarlos y
repartirlos completos y sin modificar**, con una copia de la licencia, para
usarlos con chips de Broadcom; no permite modificarlos ni desensamblarlos. No
es software libre, así que **no se mezcla con el código**: van como archivos
aparte (`\HISPADOS\BCM43XX.FW` y `BCM43XXH.FW`, con la licencia en
`BCM43XX.TXT`) que `HSPWIFI` carga al arrancar, igual que hace Linux. En el
repositorio están en `wifi/firmware/`. Es «mera agregación»: no afecta a la
licencia del resto.

### La idea de cargar el controlador de Windows XP (capa NDIS)

Idea de Carlos (18/11/2026): hacer como `ndiswrapper` en Linux y cargar el
`.sys` de Windows XP de la tarjeta, que ya lleva dentro el microcódigo y la
calibración. Se valoró y **se deja para más adelante**; para el HP Mini se
sigue con brcmsmac. Por qué:

| | brcmsmac + capa propia (lo hecho) | `.sys` de XP + capa NDIS |
|---|---|---|
| Lo que hay que escribir | unas 70 funciones de Linux (memoria, tiempo, PCI, DMA, colas) y 1.500 líneas de gestión | NDIS 5.1 y el trozo de `ntoskrnl`/HAL que usa el controlador: más de 200 funciones, con IRQL, DPC, hilos, elementos de trabajo y sus rarezas (`ndiswrapper` son más de 30.000 líneas) |
| Si falla en el equipo | hay fuentes: el registro dice qué registro o qué paso | código cerrado de 1,5 MB: un cuelgue mudo |
| Probarlo sin la tarjeta | modelo de la tarjeta (hecho) | la capa, sí, con un controlador de XP de una tarjeta que QEMU emule; lo que usa el de Broadcom, no |
| Microcódigo | se reparte con el sistema | lo pone el usuario (bajar e instalar el controlador de XP) |
| Otras tarjetas | cada una, su controlador de Linux | cualquiera con controlador de XP |

Lo que decide es **depurar a distancia**: el equipo se prueba por un registro
que hay que subir a mano, y con código cerrado cada fallo es a ciegas. La
capa NDIS tiene sentido después, como proyecto propio de Hispa-486 (que ya
carga programas de Windows), para las tarjetas sin controlador libre.

## Cómo está hecho

```
 CONFRED, PING, DESCARGAR, CONSOLA, navegador...   (lib/red.inc, sin cambios)
        │  packet driver (INT 60h), INT 2Fh AH=DAh
 ┌──────┴───────────────────────────┐   WIFI.COM ── INT 2Fh AH=DCh
 │ HSPWIFI.COM (modo real, 4,6 KB)  │◄──────────────┘
 └──────┬───────────────────────────┘
        │  paso a modo protegido de 32 bits (tabla de segmentos propia)
 ┌──────┴────────────────────────────────────────────────────────────┐
 │ HSPWIFI.BIN (32 bits, en memoria extendida)                       │
 │   nucleo.c      las llamadas del residente, cola de recepción     │
 │   w80211.c      búsqueda, autenticación, asociación, Ethernet ↔   │
 │                 802.11 (LLC/SNAP), reconexión                     │
 │   wpa.c         PBKDF2, acuerdo de 4 pasos, clave de grupo, CCMP  │
 │   radio_brcm.c  bus bcma, lo que hacía mac80211, velocidad        │
 │   compat/       lo que brcmsmac espera de Linux                   │
 │   linux/        brcmsmac, brcmutil (ISC) y bcma (GPL-2.0)         │
 └──────┬────────────────────────────────────────────────────────────┘
        │  registros en memoria (BAR0) y DMA
     BCM43224 (con BCM43XX.FW cargado en su procesador)
```

### El residente (drivers/hspwifi.asm)

Lo que queda en la memoria convencional son **4,6 KB** (código, el
búfer de una trama y la pila). Hace de *packet driver* (las mismas funciones
que `HSPRED`, copiadas de él) y contesta a `INT 2Fh AH=DAh` como `HSPRED`
(configuración IP compartida, enlace, descarga, datos de `/INFO` con la MAC
en el mismo sitio: Configuración de Hispa Ventanas la lee igual). Por eso
**no puede estar cargado a la vez que HSPRED** (lo dice), salvo con `/JUNTO`.

Para cada llamada al núcleo (`nuc`): cierra las interrupciones, abre la línea
A20, guarda la pila y las tablas (`SGDT`/`SIDT`), pone su tabla de segmentos
y entra en modo protegido; llama al núcleo con una llamada lejana de 32 bits
y vuelve a modo real restaurándolo todo. Los segmentos de 32 bits tienen la
**base en el principio del bloque** de memoria extendida: el núcleo está
enlazado para la dirección 0 y no hay que reubicarlo (una dirección física
*F* es, para el núcleo, el puntero *F − base*; da la vuelta por 2³² y cae en
su sitio). Sin paginación. La base se alinea a 64 KB dentro del bloque: el
controlador alinea sus anillos de DMA (a 4 y 8 KB) por el puntero y la
tarjeta los quiere alineados por la dirección física, así que las dos tienen
que coincidir en los bits bajos.

La tarjeta **no usa su interrupción** (se apaga en el PCI, bit 10 de la
orden): compartir una línea de IRQ con el USB sin controlar quién la atiende
puede colgar el equipo. El residente mira la tarjeta en cada tic del reloj
(INT 08h, 18 veces por segundo) y cada vez que un programa consulta el
teclado (INT 16h, AH=01h u 11h: las esperas de `lib/red.inc` lo hacen en
cada vuelta), como `HSPUSB` con la red por USB.

`INT 2Fh AH=DCh` (para `WIFI` y lo que venga): AL=00h, ¿está? (AL = FFh, BX
= 5746h «WF»); AL=01h, llamar al núcleo (CX = función, DS:SI = petición);
AL=02h, datos del residente (archivo del registro y opciones); AL=03h,
descargarlo. Las funciones
están en `wifi/nucleo/llamadas.h`: buscar, lista de redes, conectar,
desconectar, estado, registro, preparar la clave por trozos.

### El núcleo (wifi/)

- `wifi/nucleo/w80211.c`: lo que en Linux hace mac80211, reducido a una
  estación de una red de casa. Búsqueda activa (sondeos) en los canales 1 a
  11 y pasiva en el 12 y el 13, 130 ms por canal; tabla de 24 redes; elige el
  punto de acceso de más señal; autenticación abierta y asociación con
  reintentos; datos con cabecera LLC/SNAP; duplicados; trama vacía cada 20 s;
  avisa al punto de acceso (ahorro de energía) si busca estando conectado.
- `wifi/nucleo/wpa.c`: SHA-1, HMAC, PBKDF2 (por trozos, para no parar el
  equipo), PRF, AES-128 (tablas calculadas al arrancar), envoltorio de claves
  (RFC 3394), CCMP **por programa** (brcmsmac no usa el cifrado de la
  tarjeta; en un Atom, menos de 1 ms por trama) y el suplicante: acuerdo de 4
  pasos, clave de grupo y su renovación, contadores de repetición, números de
  paquete (un paso 3 repetido no vuelve a poner las claves: sin el fallo
  «KRACK»).
- `wifi/nucleo/radio_brcm.c`: une brcmsmac con lo anterior: el arranque del
  bus bcma (lo de `drivers/bcma/main.c`), las funciones de mac80211 que el
  controlador llama (alta, tramas recibidas, estado de las enviadas, colas) y
  una elección de velocidad sencilla (una escalera con las velocidades del
  punto de acceso: sube tras 12 acuses seguidos, baja tras 2 fallos).
- `wifi/compat/`: `hispa_linux.h` y `hispa_80211.h` (tipos, listas, sk_buff,
  cerrojos que no hacen nada, las definiciones de mac80211 que usa el
  controlador) y `base.c` (memoria, textos, registro, tiempo por el contador
  TSC medido con el temporizador 8254, trabajos aplazados, PCI por sus
  puertos, DMA).
- `wifi/nucleo/sim.c`: una radio de mentira con su punto de acceso (WPA2,
  DHCP, ARP y eco): es `HSPWIFI /SIMULA` y la base de las pruebas.
- `wifi/nucleo/entrada.asm` y `nucleo.ld`: la entrada (cabecera `HWF1`, pila
  de 64 KB, tabla de excepciones) y el enlazado a un binario plano.

La puesta en marcha va en **tres pasos**, y tras cada uno el registro se
escribe en el archivo de `/REG:` (si el siguiente se cuelga, queda escrito
hasta dónde se llegó): 1, buscar la tarjeta en el PCI (solo mira); 2,
despertarla, leer sus núcleos y su SPROM y montar el controlador; 3, cargar
el microcódigo y encender la radio.

Si dentro del núcleo salta una **excepción del procesador**, no se cuelga el
equipo: se apunta en el registro (vector, EIP), el Wi-Fi queda parado y lo
demás sigue.

### Memoria

| | |
|---|---|
| Convencional (residente) | 4.708 octetos (4,6 KB) |
| Extendida (XMS) | un bloque de **2 MB** (más 64 KB para alinear): el núcleo, 632 KB (404 KB de archivo y el resto datos: registro de 96 KB, cola de recepción, pila de 64 KB); el microcódigo, 96 KB; y el montón, del que el controlador usa unos 225 KB |
| En el disco | `HSPWIFI.COM` 8 KB, `HSPWIFI.BIN` 404 KB, `WIFI.COM` 5,5 KB, microcódigo 97 KB |

Necesita un procesador con contador TSC (cualquiera con ranura PCIe lo
tiene).

## Límites

- **Sin probar en la tarjeta** (arriba).
- **No funciona con HSPEMM** (modo V86): el residente tiene que pasar a modo
  protegido por su cuenta. Lo dice y no se carga. Con HSPDPMI (Hispa
  Ventanas, programas de Windows) sí: probado en QEMU.
- **No a la vez que HSPRED**: cable y Wi-Fi a la vez, no (`HSPWIFI` descarga `HSPRED` si su tarjeta no tiene enlace; con `/JUNTO` se
  cargan los dos, pero la red va solo por el cable: es para depurar).
- Sin interrupción: entre tic y tic (55 ms) la tarjeta guarda hasta 256
  tramas, pero la respuesta de un programa que no consulte el teclado espera
  al tic. Para navegar y descargar va de sobra.
- Mientras el núcleo trabaja, las interrupciones están cerradas: al cargar,
  uno o dos segundos (la puesta en marcha de la radio); después, ratos de
  microsegundos.
- Solo 802.11b/g, sin ahorro de energía, sin 802.11n (agregación) ni 5 GHz.
- El nombre de la red se escribe tal cual: con eñes o acentos (UTF-8 en el
  router) no coincide con lo que se teclea; para esas, el número de la lista.
- La ventanita de Hispa Ventanas no pide la dirección IP: tras conectar,
  `CONFRED` (o `WIFI /AUTO` y `CONFRED` en `AUTOEXEC.BAT`). Enseña las 6
  primeras redes a la vista (16 en la lista, con las flechas); las redes
  ocultas salen sin nombre.

## Pruebas

`tests/run_wifi_tests.sh` (en `make test`), todo sin la tarjeta:

1. **Las normas** (`build/prueba_wifi`, en el anfitrión): SHA-1 (FIPS 180),
   HMAC-SHA1 (RFC 2202), PBKDF2 (RFC 6070), la PMK (IEEE 802.11i, H.4.3:
   «password»/«IEEE» y los demás), PRF (H.6.3), la PTK (comprobada aparte con
   Python), AES-128 (FIPS 197), envoltorio de claves (RFC 3394) y **CCMP con
   el vector de prueba 1 de IEEE 802.11-2007 (H.8.3.1)**, octeto a octeto.
2. **La gestión** contra el punto de acceso de mentira (mismo programa):
   búsqueda, acuerdo de 4 pasos, ARP (clave de pareja), DHCP (clave de
   grupo), renovación de la clave de grupo, buscar estando conectado,
   expulsión y reconexión, pérdida de la red y vuelta, tramas perdidas,
   contraseña equivocada, red que no existe, red WPA (TKIP), red abierta.
3. **El controlador de la Broadcom contra un modelo de la tarjeta**
   (`build/chip_sim`, en el anfitrión; el modelo, `tests/wifi/tarjeta_sim.c`):
   PCI (la despierta de D3, apaga la interrupción), el bus bcma con su EROM y
   una SPROM de revisión 8, el núcleo 802.11 con el **microcódigo de verdad**,
   los anillos de DMA. Montaje, encendido, búsqueda, WPA2, 300 ecos de ida y
   vuelta, acuses y subida de velocidad, renovación de clave, parada, sin
   pérdidas de memoria. El modelo dice lo que el controlador *espera* de la
   tarjeta: prueba nuestro código, no la tarjeta.
4. **En QEMU, con la radio simulada** (`HSPWIFI /SIMULA`): el residente, el
   paso a modo protegido, el packet driver y la orden `WIFI` enteros, con
   `CONFRED` (DHCP) y `PING` de verdad por encima; la clave preguntada sin
   enseñarla; `RED.INI` sin la contraseña; `/AUTO`, `OLVIDAR`, `/U`.
5. **En QEMU, el núcleo de verdad con el modelo de la tarjeta dentro**
   (`build/HSPWSIM.BIN`) y el microcódigo cargado del disco: lo mismo, por el
   camino completo del controlador, y el registro en el disco.
6. Con HSPEMM: lo dice y no se carga. Con HSPRED: tampoco, salvo con
   `/JUNTO` (y entonces la red sigue por el cable).

`tests/run_wifi_ventana_tests.sh` (en `make test`): el icono y la ventanita
de Hispa Ventanas en QEMU, con `HSPWIFI /SIMULA` (sin conectar, la lista,
la contraseña pedida y en asteriscos, la equivocada, conectado y el icono
lleno, `RED.INI` sin la contraseña, desconectar, conectar con la clave
guardada, olvidar, «Configuración de red...»), con una e1000 y HSPRED
(enlace, IP, puerta de enlace) y sin red.

Fallos que sacaron estas pruebas (y que en el equipo habrían sido un cuelgue
sin explicación): un puntero nulo al encender la radio (el canal tiene que
estar puesto antes), las velocidades básicas dadas a destiempo, una petición
de envío que pisaba la dirección de entrada del núcleo, las trampas de
excepción mal alineadas, lecturas de puertos que el compilador sacaba de los
bucles y, el más serio, **los anillos de DMA mal alineados** (la base del
núcleo no estaba alineada y el controlador alineaba por el puntero: en la
tarjeta de verdad no habría funcionado nada, y además se escribía fuera de
la memoria pedida). Desde entonces el montón lleva marcas y se repasa: si
algo escribe fuera de su sitio, el registro lo dice.

## Prueba en el equipo de verdad

`tools/mkprueba_wifi.sh carpeta` hace, a partir de `build/hispados-usb.img`,
la imagen USB de la prueba (`usb/prueba-wifi/`: `PRUWIFI.BAT`, `PASO2.BAT`,
`WIFIUSB.BAT` y el `LEEME.TXT` para quien la hace). Está en el FTP, en
`HispaDOS/prueba-wifi/`. Arranca con un menú cuya primera opción carga
`HSPWIFI /PANTALLA /REG:C:\WIFI.LOG` y busca redes; se conecta a mano (`WIFI
CONECTAR n`) y `PASO2` hace `CONFRED`, `PING`, una descarga y `WIFI
REGISTRO`. Todo queda en `C:\WIFI.LOG` (en el pendrive) para subirlo. La
segunda opción del menú es para tener consola remota por un adaptador USB de
red (HSPUSB `/F /RED`, `HSPRED /USB`, `CONSOLA`, y el Wi-Fi con `/JUNTO`). El registro lleva: las tarjetas PCI de red, la
tarjeta (BAR0, IRQ, subsistema), el chip y sus núcleos con sus revisiones,
la SPROM (revisión, placa, país, MAC), el PHY y la radio, cada aviso del
controlador de Linux, las redes vistas con su señal, la autenticación, la
asociación, los 4 pasos, y un resumen (paquetes, acuses, velocidad, memoria).

Qué mirar primero si falla, por orden:

| En el registro | Qué significa |
|---|---|
| «no hay ninguna tarjeta Wi-Fi Broadcom» y la lista de `PCI ...` | la tarjeta es otra (apuntar su `fabricante:modelo`) o está apagada en la BIOS |
| «la BIOS no ha dado a la tarjeta una dirección de memoria» | BAR0 vacío o por encima de 4 GB |
| se queda tras «Tarjeta Wi-Fi: ...» | no se leen los núcleos (ventana del BAR0): lo primero que puede ser distinto del modelo |
| «la radio está APAGADA por el interruptor» | el botón del Wi-Fi del equipo |
| `EXCEPCIÓN n ... EIP xxxxxxxx` | fallo nuestro: con el EIP y `build/wifi/nucleo.elf` se saca la línea |
| enciende pero `WIFI REDES` no ve nada | la radio no recibe: PHY, antenas (SPROM), canal |
| ve redes pero «no contesta a la autenticación» | no emite, o emite mal (potencia, velocidad de gestión) |
| asocia pero «el acuerdo de claves no termina» | la contraseña, o las tramas de datos (cifrado, acuses) |
| conecta pero `CONFRED` no consigue dirección | la clave de grupo o la recepción de difusión |

## Lo que sigue

- **Probarlo en el HP Mini** y corregir con el registro.
- Que la ventanita de la red pida también la dirección IP (DHCP) al
  conectar, sin `CONFRED`.
- 802.11n (MCS, agregación) y 5 GHz: el controlador de la radio ya los lleva.
- La clave de grupo TKIP (routers en modo mixto WPA/WPA2).
- Usar la interrupción de la tarjeta cuando se sepa que su línea es solo suya.
- Otras tarjetas: BCM4313 (falta incluir su PHY LCN, que está en Linux),
  Realtek RTL8822BE (Lenovo A275: `rtw88`, con microcódigo repartible),
  Atheros (`ath9k`, sin microcódigo). La gestión, WPA2 y el residente valen
  para todas: solo cambia la «radio».
- La capa NDIS para controladores de Windows XP (ver «Decisiones»).
