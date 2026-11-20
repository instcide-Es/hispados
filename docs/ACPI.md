# ACPI: la batería de los portátiles (HSPACPI)

**Estado (18/11/2026):** hecho y probado en el equipo de compilación con las
tablas ACPI de verdad del **Toshiba NB250-10N** y del **HP Mini 5103** y un
controlador embebido simulado, y en QEMU de punta a punta (hasta la pila de la
barra). **Sin probar todavía en los equipos**: la imagen de prueba está en el
FTP, carpeta `prueba-bateria` (ver «La prueba en el equipo»).

## El problema

Hispa Ventanas pregunta la batería a la BIOS por **APM** (INT 15h AX=530Ah,
`bat_leer` en `ventanas/escritorio.inc`). Los portátiles de 2008 en adelante
ya no tienen APM —el informe de EQUIPO del HP Mini 5103 dice «APM: no hay», y
el del Toshiba NB250 igual—: la batería solo se sabe por **ACPI**, y no
leyendo un registro, sino **ejecutando un programa** que trae el propio equipo
en sus tablas (los métodos `_BST`, `_BIF`, `_STA` de la batería, escritos en
AML), que a su vez habla con el **controlador embebido**, el microcontrolador
del portátil que lleva la batería, por los puertos 62h y 66h.

## Qué se ha hecho

`HSPACPI` es un residente que lee la batería por ACPI y **contesta él a
INT 15h AX=5300h y AX=530Ah como una BIOS con APM 1.2**. Hispa Ventanas no se
ha tocado: el indicador de la barra de Hispa-486 y el de la barra de arriba de
Hispa-386 funcionan tal cual, y también cualquier programa de DOS que mire la
batería por APM.

| Pieza | Qué es | Tamaño |
|---|---|---|
| `HSPACPI.COM` (`drivers/hspacpi.asm`) | El residente de modo real y la instalación | 6 KB en disco; **1,6 KB residentes** |
| `HSPACPI.BIN` (`acpi/`) | El núcleo de 32 bits: ACPICA y lo nuestro | 146 KB en disco |
| memoria extendida | Un bloque de XMS de 3,1 MB para el núcleo | de ellos usa: 146 KB de código, 250 KB de pilas y registro y lo que pida ACPICA (Toshiba: 143 KB; HP Mini: 567 KB) |

Como `HSPWIFI`, el núcleo se carga en un bloque de memoria extendida y el
residente pasa a modo protegido con su propia tabla de segmentos para cada
llamada. Por eso **no funciona en modo V86** (con `HSPEMM`: lo dice y no se
carga) y **sí con `HSPDPMI`** (Hispa Ventanas).

### La decisión: ACPICA, no un intérprete propio

Había dos caminos: escribir un intérprete de AML pequeño solo para la batería
o traer **ACPICA**, el de Intel (el que usan Linux, FreeBSD y los demás).
Se ha traído ACPICA:

- **Riesgo.** No se puede probar en el equipo, y cada fabricante escribe su
  AML a su manera. El `_BST` del Toshiba son 30 líneas; el del HP Mini pasa
  por una docena de métodos con `Mutex`, paquetes dentro de paquetes,
  `DerefOf`/`Index`, `ToBCD`, `Concatenate`, campos indexados, regiones
  creadas dentro de un método con la dirección calculada, llamadas a la BIOS
  (SMI)… Un intérprete propio «suficiente para la batería» habría sido
  suficiente para el Toshiba y habría fallado en el siguiente equipo de una
  manera que solo se ve con el equipo delante. ACPICA lleva 25 años
  ejecutando el AML de todos los portátiles: las dos DSDT han ido a la
  primera, sin un solo aviso.
- **Tamaño.** Es grande en fuentes (4 MB, 160 archivos) pero compila a
  146 KB, y va entero en memoria extendida: en memoria convencional son
  1,6 KB. Un equipo con batería por ACPI tiene de sobra (el más pequeño que se
  conoce, 1 GB).
- **Probar sin el equipo.** ACPICA separa lo que necesita del sistema en unas
  40 funciones (`AcpiOs...`). Con dos juegos de esas funciones, el mismo
  código de 32 bits corre en el equipo y en el equipo de compilación con un
  portátil de mentira debajo (como se hizo con brcmsmac en el Wi-Fi).
- **Licencia.** ACPICA tiene licencia triple (la de Intel, BSD o GPL-2.0):
  compatible. Va aparte, en `acpi/acpica/`, sin modificar salvo dos líneas
  (ver `acpi/LEEME.md`).

### Cómo está hecho

```
 Hispa Ventanas / cualquier programa        INT 15h AX=530Ah
        │                                         ▲ contesta al momento
        ▼                                         │ con lo último leído
 ┌──────────────────── HSPACPI.COM (residente, modo real) ───────────────┐
 │  INT 15h: 5300h y 530Ah          INT 08h: cuando toca, un paso        │
 │  INT 2Fh AH=DBh (estado, registro, descargar)                         │
 └───────────────────────────────┬───────────────────────────────────────┘
                                 │ modo protegido, base = el bloque de XMS
 ┌───────────────────── HSPACPI.BIN (32 bits, en XMS) ───────────────────┐
 │ nucleo.c   las llamadas y la «tarea» (ACPICA en su propia pila)       │
 │ bateria.c  arrancar ACPICA, buscar la batería, leer, cocinar para APM │
 │ ec.c       el controlador embebido: puertos 62h/66h, _REG, avisos     │
 │ osl.c      lo que ACPICA pide al sistema; base.c memoria y registro   │
 │ maquina.c  puertos, memoria física, reloj (en las pruebas, simulados) │
 │ acpica/    ACPICA R2025_04_04: tablas, árbol de nombres, intérprete   │
 └───────────────────────────────────────────────────────────────────────┘
```

**No bloquear el equipo.** El controlador embebido es lento y los métodos
piden esperas: el `_BST` del Toshiba hace cinco `Sleep (20)`, 100 ms. ACPICA
no sabe pararse a medias de un método, así que corre en una **tarea con su
propia pila** dentro del núcleo (`acpi/nucleo/tarea.asm`): cuando un método
pide dormir, o el controlador embebido tarda más de 2 ms en contestar, el
núcleo cambia de pila y **vuelve al residente**; el equipo sigue, y en un tic
posterior del reloj (INT 08h) la tarea continúa donde se quedó. Una lectura
del Toshiba son unos 7 trocitos de pocos milisegundos repartidos en medio
segundo, con las interrupciones cerradas solo durante cada trocito. INT 15h
no espera nunca: contesta con lo último leído.

**Cada cuánto.** Una lectura cada 10 segundos (`/CADA:s`); Hispa Ventanas
mira cada 5.

**Qué se lee.** De cada batería (`PNP0C0A`): `_STA` (¿está puesta?), `_BIF`
(o `_BIX`) la primera vez, cuando el equipo avisa de que ha cambiado
(`Notify` 81h) y cada 10 minutos, y `_BST` siempre. De ahí:

- **porcentaje** = lo que queda / la capacidad llena (si no la da, la de
  fábrica); con dos baterías, la suma;
- **con cable** = no se está descargando (ver abajo por qué no `_PSR`);
- **cargando** = bit 1 de `_BST`;
- **minutos** = lo que queda × 60 / el ritmo de descarga, si el equipo lo da.

Y lo que contesta INT 15h AX=530Ah (BX=0001h, o 8001h): BH = línea de
corriente (0 sin cable, 1 con cable, FFh no se sabe), BL = estado (0 alta,
1 baja —menos del 25 %—, 2 crítica —menos del 5 % o lo dice el equipo—,
3 cargando), CH = banderas (bit 7: no hay batería), CL = porcentaje, DX =
minutos con el bit 15 puesto (FFFFh: no se sabe), SI = baterías puestas.
AX=5300h contesta «APM 1.2» (AX=0102h, BX=«PM», CX=0). Las demás funciones
53xxh van a la BIOS, que dirá que no las tiene: `APAGAR` ya apaga por ACPI
por su cuenta (probado con `HSPACPI` cargado).

**[SALUD] El desgaste.** APM no lleva nada de eso, así que el núcleo, tras
cada lectura, deja también en el residente (`ll_inicio.dir_salud`) lo que
guarda cada batería llena frente a lo que guardaba nueva: `_BIF` (o `_BIX`)
da las dos capacidades y la unidad; los **ciclos de carga** solo los da
`_BIX` (el elemento 8): si se ha leído `_BIF` y el equipo tiene también
`_BIX` (se mira con `AcpiGetHandle`, sin evaluar nada que no exista), se
evalúa una vez más para sacarlos. No se inventa: una batería sin las dos
capacidades, o con la llena más de un 5 % por encima de la de fábrica, no
cuenta («no se sabe»); si es algo mayor (pasa con baterías nuevas), se toma
como el 100 %. Con dos baterías, la salud es la de las dos juntas (si van en
la misma unidad) y los ciclos, de cada una. Lo enseña `HSPACPI /E` («Salud:
68 % (5000 de 7350 mAh de fábrica), 312 ciclos de carga») y el recuadro de
ayuda de la pila de la barra de Hispa-486 (docs/VENTANAS.md, «[AYUDA]»).
Desde esta versión (la 2) `HSPACPI.COM` y `HSPACPI.BIN` tienen que ser de
la misma: si no, «El archivo HSPACPI.BIN no vale (dañado o de otra versión)».

**Si falla algo.** Un fallo suelto de `_BST` deja el último valor; al tercero
seguido, «no se sabe» (no se inventa un nivel). Si el controlador embebido no
contesta, cada orden se intenta 3 veces con un plazo de 0,1 s, soltando el
procesador mientras espera. Un bucle de un método que no acaba se corta a los
3 segundos. Una excepción del procesador dentro del núcleo se apunta y el
núcleo se para (el residente sigue contestando «no se sabe»).

## Los dos modos

Un sistema con ACPI, al arrancar, hace mucho más que leer la batería: pasa el
equipo a «modo ACPI», ejecuta los `_INI` de todos los dispositivos, dice qué
Windows es, atiende la interrupción de ACPI… El equipo cambia entonces de
manos: deja de llevarlo la BIOS y lo lleva el sistema. Hispa-DOS **no es ese
sistema**: aquí la BIOS tiene que seguir llevando el botón de encendido, el
ventilador, el brillo y la tapa. Así que:

- **Nunca** se pasa el equipo a modo ACPI (no se escribe `ACPI_ENABLE`) ni se
  usa la interrupción de ACPI (SCI).
- **Modo mínimo (el normal).** Solo: cargar las tablas, avisar al controlador
  embebido de que se le va a leer (`_REG (3, 1)`: sin eso el Toshiba contesta
  valores de fábrica y el HP nada) y ejecutar los métodos de la batería. No se
  ejecutan los `_INI`; a `_OSI` se contesta que no somos ningún Windows y
  `\_OS` dice «Microsoft Windows» (Windows 98, el primero con ACPI). Con eso,
  en el **HP Mini 5103**, su `_REG` **no** le cambia al controlador embebido
  el tipo de sistema ni los ajustes del ventilador (lo hace con cualquier otro
  Windows): las únicas escrituras son elegir qué batería se lee. En el
  **Toshiba**, una: «región lista».
- **`/COMPLETO`.** Lo que haría un sistema con ACPI, salvo el modo ACPI:
  `\_SB._INI` y el `_INI` de cada dispositivo, `_OSI` como Windows XP SP2 (ni
  Vista ni 7: con ellos los equipos pasan el brillo y otras teclas al sistema)
  y los avisos del controlador embebido (`_Qxx`), que se miran 4 veces por
  segundo. Es el camino que los fabricantes prueban con Windows; está para el
  equipo en que el modo mínimo no baste. En el HP Mini escribe en el
  controlador embebido el tipo de sistema y los ajustes del ventilador: **hay
  que probarlo mirando el ventilador**.

Se afina con `/OSI:n` (0 ninguno, 1 XP, 2 hasta Windows 7, 3 todos),
`/EVENTOS` (atender los avisos también en el modo mínimo), `/PSR` y
`/SINREFRESCO`.

### Lo que se ha visto en cada equipo (en sus tablas)

**Toshiba NB250-10N** (DSDT de 28 KB). La batería es `\_SB.PCI0.LPC0.BAT1` y
lee directamente del controlador embebido (`EC0`, región `ERAM`): `BST0`
(C1h), `BRC0` lo que queda (C2h), `BFC0` la capacidad (CCh), `BAC0` la
corriente (D2h, en complemento a dos), `BAL0` batería puesta (9Ah bit 0),
`ADPT` cable (A3h bit 5). Todo va detrás de `ECOK ()`, que es «¿se ha
llamado ya a `_REG`?». Unidades: mAh.

**HP Mini 5103** (DSDT de 122 KB y 4 SSDT). La batería es `\_SB.BAT0`
(`BAT1` existe pero dice siempre que no está). Tres cosas que no se habrían
adivinado:

1. **Lo leído se guarda.** `_BST` devuelve lo último guardado hasta que un
   aviso del controlador embebido (`_Q03`) marca «hay que releer» llamando a
   `EC0.PWUP`. Sin interrupción de ACPI ese aviso no llega, y el nivel se
   quedaría clavado en la primera lectura. `HSPACPI` lo detecta (un método
   `PWUP` de 2 argumentos en el controlador embebido) y llama él a
   `PWUP (7, FFh)` antes de cada lectura (`/SINREFRESCO` lo quita). Probado:
   el nivel baja sin aviso y se ve.
2. **`_PSR` hace más que contestar.** Al cambiar el cable, el `_PSR` del
   adaptador apaga o enciende la **tarjeta de red por cable** (el «ahorro de
   la LAN con batería» de HP), con esperas de varios segundos. Por eso el
   cable se deduce de la batería (si no se descarga, hay cable) y **`_PSR` no
   se llama** salvo con `/PSR`.
3. **Las llamadas a la BIOS.** Varios métodos llaman a la BIOS por SMI
   (puerto B2h) y esperan a que conteste. En el equipo contestará; en las
   pruebas se simula.

## Uso

```
HSPACPI [/COMPLETO] [/CADA:s] [/V] [/REG:archivo] [/F]    cargarlo
HSPACPI /AUTO     para el AUTOEXEC.BAT
HSPACPI /E        (ya cargado) la batería, con detalle
HSPACPI /U        descargarlo
```

- `/AUTO`: solo se queda si la BIOS no da la batería por APM, hay ACPI y el
  equipo declara una batería; si no, sale callado (nivel de error 1). Es lo
  que lleva el `AUTOEXEC.BAT` de Hispa-486, antes de `VENTANAS`. En
  Hispa-386 los archivos van en `\HISPADOS` de la imagen USB; para usarlo,
  `C:\HISPADOS\HSPACPI /AUTO` en el `AUTOEXEC.BAT` (configuración «Normal»:
  sin `HSPEMM`).
- `/V` enseña el registro; `/REG:archivo` lo guarda (se añade al final).
  Cargando, se va guardando a cada paso: si algo se cuelga, queda escrito
  hasta dónde llegó. Ya cargado, `HSPACPI /REG:archivo` guarda lo apuntado
  desde la última vez.
- `/F`: cargarlo aunque la BIOS ya dé la batería por APM.
- `HSPACPI.BIN` va al lado de `HSPACPI.COM` (o en `\HISPADOS` de la unidad de
  arranque).

El **registro** (el `BATERIA.LOG` de las pruebas) lleva: las tablas
encontradas con su dirección y fabricante, el controlador embebido y sus
puertos, cada método evaluado en las primeras lecturas con lo que devuelve,
cuántos accesos al controlador embebido hizo y cuánto durmió, con `/V` cada
octeto leído o escrito en el controlador embebido (los 600 primeros), los
`Notify`, los avisos del controlador, los errores de ACPICA con sus propias
palabras y, después, una línea cada vez que cambia el nivel o el cable.

Para Hispa Ventanas no hay nada que configurar. Detalle a tener en cuenta
(viene de antes): la barra decide **al arrancar** si hay batería; si se
arranca sin ella y se pone después, la pila no sale hasta volver a entrar.

### INT 2Fh AH=DBh

| AL | Qué |
|---|---|
| 00h | ¿está? → AL = FFh, BX = 4143h («AC»), CX = versión |
| 01h | llamar al núcleo: CX = función (2 registro, 3 estado, 5 leer ya), DS:SI = petición → DX:AX |
| 02h | → ES:BX = `struct ll_apm` (lo último leído: `acpi/nucleo/llamadas.h`) |
| 03h | descargar → AL = 0 hecho, 1 hay otro residente encima |
| 04h | parar: avisa al controlador embebido (`_REG (3, 0)`) y el núcleo no se vuelve a llamar (530Ah sigue dando lo último leído) → AL = 0. Lo piden APAGAR y REINICIAR antes de vaciar los búferes y de tocar ACPI |
| 05h | [SALUD] → ES:BX = `struct ll_salud` (`acpi/nucleo/llamadas.h`): de cada batería puesta, la capacidad de fábrica (`DesignCapacity`), la de la última carga completa (`LastFullChargeCapacity`), la unidad (mAh o mWh) y los ciclos de carga (solo con `_BIX`; FFFFh si no se saben); y la salud de todas juntas (la suma de las llenas × 100 / la suma de las de fábrica; FFh si no se sabe). Desde la versión 2 (AX=DB00h → CX = 2) |

## Las pruebas

`tests/run_acpi_tests.sh` (en `make test`; 124 comprobaciones):

1. **En el equipo de compilación** (`build/acpi_sim`, 32 bits): el mismo
   núcleo con `tests/acpi/maquina_sim.c` debajo —la memoria física (las
   tablas ACPI volcadas por EQUIPO, `tests/acpi/toshiba-nb250/` y
   `tests/acpi/hp-mini-5103/`), los puertos con un controlador embebido de
   256 registros en 62h/66h y un reloj de mentira— y un escenario en texto
   (`escenario.txt`: qué hay en cada registro y qué cambia antes de cada
   lectura). Se comprueba, en los dos modos y con las dos DSDT: `_BIF` y
   `_BST` con lo esperado, porcentaje, cable, cargando y minutos; enchufar;
   quitar la batería; el HP sin aviso del controlador y con él; que en el modo
   mínimo no se toca el ventilador del HP; `/PSR`; el controlador embebido
   mudo (no se cuelga ni inventa, y se recupera); un equipo sin ACPI.
2. **En QEMU**, que tiene ACPI pero no batería: una SSDT de prueba
   (`tests/acpi/bateria-qemu.asl`, cargada con `-acpitable`) declara una
   batería y un adaptador cuyos métodos leen el nivel de dos octetos de la
   CMOS (con `Mutex`, `Sleep` y `Notify`, como los de verdad). El camino
   entero: `HSPACPI` carga el núcleo en XMS, lee las tablas de QEMU, contesta
   INT 15h y **Hispa Ventanas pinta la pila** (los mismos puntos que la prueba
   de `/BAT`), también al cambiar el nivel y el cable con Ventanas en marcha;
   `/E`, `/U`, `/AUTO`, `/COMPLETO`, el registro en un archivo, el sobremesa
   (calla y no se queda) y `APAGAR` con `HSPACPI` cargado. [SALUD] La
   batería de mentira está algo gastada (7350 mAh de fábrica, 5000 en la
   última carga completa: el 68 %; `_BIX` con 312 ciclos; un tercer octeto
   de la CMOS, 62h, cambia la salud y avisa con `Notify 81h`): `/E` enseña
   la salud, y el escritorio de Hispa-486 el recuadro de ayuda de la pila
   (sin cable, enchufada y con la batería gastada) y los del altavoz y la
   red. En el equipo de compilación, la salud del Toshiba (100 %) y la del
   HP (92 %: 4800 de 5200).

Lo que **no** se puede probar sin el equipo: el controlador embebido de
verdad (sus tiempos, y si la BIOS, que en este modo sigue hablando con él por
su cuenta, se cruza con nuestras órdenes), las llamadas a la BIOS (SMI) de los
métodos del HP, y que los valores de los registros signifiquen lo que dice la
DSDT.

Para probar con las tablas de **otro equipo**: en él, «Prueba del equipo y del
sonido» del menú de arranque (DIAGNOSTICO/EQUIPO) guarda `INFORMES\...\ACPI\`
con la DSDT y las SSDT; con `iasl -d DSDT.BIN` se ve qué registros lee su
batería, y

```
build/acpi_sim -v -n 3 -o 38 -osi 0 -t FACP.BIN -t DSDT.BIN -t FACS.BIN -t SSDT1.BIN ... -e escenario.txt
```

enseña el registro que saldría en el equipo y qué registros del controlador
embebido toca (`-o 38 -osi 0` es el modo mínimo; `-osi 1`, el completo).

## La prueba en el equipo

`tools/mkprueba_bateria.sh` hace, de la imagen USB de Hispa-486, una imagen
con la opción «PRUEBA DE LA BATERÍA» la primera del menú de arranque: carga
`HSPACPI /V /REG:C:\BATERIA.LOG`, pide enchufar y desenchufar el cargador,
abre el escritorio para ver la pila y, si el modo mínimo no va, pasa al
completo (`COMPLETO.BAT`). Todo queda en `C:\BATERIA.LOG`. Los pasos para
quien la haga, en `usb/prueba-bateria/LEEME.TXT`.

## Lo que queda

- La prueba en el HP Mini 5103 y en el Toshiba NB250, y lo que salga de ella.
- La barra de Hispa Ventanas decide una sola vez si hay batería (arriba).
- Con ACPICA dentro se puede hacer más: la tapa, el botón de encendido, la
  temperatura. No se ha hecho nada de eso.

## En Hispa-Pentium

El mismo núcleo de C (y los mismos objetos de `build/acpi/`) corre también en
Hispa-Pentium, como programa del anillo 3: `ACPI.BIN` (`acpi/pentium/`), que
además apaga el equipo (S5) y deja al núcleo reiniciar por la FADT. Lo cuenta
[PENTIUM.md](PENTIUM.md), sección 11, «Apagar de verdad y la batería». Del
código común solo cambia `nucleo.c`: `tarea_quieta()` (¿está la tarea entre
lecturas?) y, con `-DHISPA_PENTIUM`, que la petición de `LL_INICIA` está en la
memoria del propio programa.

## De quién es cada cosa

`acpi/acpica/` es ACPICA R2025_04_04 de Intel (licencia triple; aquí, bajo
GPL-2.0), ver `acpi/LEEME.md`. Lo demás (`acpi/nucleo/`,
`drivers/hspacpi.asm`, `tests/acpi/*.c`, `tests/acpi/bateria-qemu.asl`) es de
Hispa-DOS, GPL-2.0 o posterior. Las tablas de `tests/acpi/toshiba-nb250/` y
`tests/acpi/hp-mini-5103/` son los volcados de EQUIPO de esos dos portátiles
(obra de sus fabricantes), solo para las pruebas: ver `tests/acpi/LEEME.md`.
