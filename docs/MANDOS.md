# Mandos de juego en Hispa-DOS

Hispa-DOS reconoce mandos de juego de varias procedencias y se los da a los
programas de DOS como si fueran el mando clásico del PC, o como teclas.

| De dónde | Qué mandos | Estado |
|---|---|---|
| USB (con `HSPUSB.SYS`, controladora xHCI) | Mandos y joysticks HID genéricos (se lee su descriptor de informe) y mandos tipo Xbox 360 con cable | Hecho; probado en QEMU con un mando simulado que lleva los descriptores de mandos reales. Falta probarlo con mandos de verdad |
| Puerto de juegos (201h; tarjeta de sonido o tarjeta suelta) | Analógicos de 2 ejes y 4 botones, o de 4 ejes (dos mandos, o uno con timón y gases) | Hecho; probado con un puerto simulado (QEMU no lo tiene). Falta probarlo en un PC con puerto de juegos |
| Puerto de impresora con un cable casero | Joystick Atari/Commodore/Amstrad/MSX de 9 patillas, mando de Mega Drive (3 botones), NES, Super Nintendo y PlayStation (digital y analógico); hasta dos de NES/SNES/PlayStation | Hecho; probado con un puerto simulado. Falta probarlo con cables de verdad |
| Puerto de juegos, mandos digitales (Gravis GamePad Pro «GrIP», Microsoft SideWinder) | | Siguiente paso (ver [más abajo](#lo-que-falta)) |
| Puerto serie (SpaceOrb, volantes) | | Diseño preparado (ver [más abajo](#puerto-serie-diseño)) |

## Las piezas

| Pieza | Qué hace |
|---|---|
| `HSPUSB.SYS` (CONFIG.SYS) | Lleva el USB. Enumera los mandos USB, analiza su descriptor de informe y, con cada informe, entrega su estado a HSPJOY. |
| `HSPJOY.COM` (AUTOEXEC.BAT) | El residente (unos 3,2 KB). Junta los mandos de todas las procedencias en una tabla, sondea el puerto de juegos y el de impresora, y los entrega: INT 15h AH=84h, interfaz propia (INT 2Fh AH=D9h) y mando a teclado. |
| `MANDO.COM` | La utilidad: lista los mandos, los enseña en vivo, enseña lo que ve un juego y calibra el del puerto de juegos. |
| `MANDO.INI` | La configuración (junto a `HSPJOY.COM`, en `\HISPADOS`). |

**Por qué este reparto.** La parte USB va dentro de HSPUSB porque solo él
puede hablar con el controlador xHCI (sus anillos, sus contextos, el modo
«irreal» o las llamadas al monitor en modo V86, el bucle de eventos del reloj): un segundo programa tendría que
repetir o compartir todo eso. El análisis del descriptor de informe (lo más
voluminoso) va en la parte de inicialización de HSPUSB, que se descarta: en
la memoria solo queda el «mapa» de cada mando (124 bytes) y la rutina que
pasa un informe al estado, alrededor de 1 KB en total. Todo lo demás (lo que
ven los juegos, las teclas, los otros puertos) va en `HSPJOY.COM`, que se
puede cargar, volver a configurar y descargar sin reiniciar, y que no se
queda en la memoria si no hay ningún mando. Los dos se hablan por una sola
llamada (INT 2Fh AX=D903h), de modo que cualquier otro driver puede aportar
mandos de la misma manera.

## Uso

```
REM CONFIG.SYS: para los mandos USB, HSPUSB tiene que llevar el USB
DEVICE=C:\HISPADOS\HSPUSB.SYS /F
REM AUTOEXEC.BAT (viene puesto en el disquete y en la memoria USB)
C:\HISPADOS\HSPJOY
```

`HSPJOY` busca los mandos al cargarse: los USB que haya enumerado HSPUSB
(tienen que estar conectados al arrancar), el del puerto de juegos y lo que
diga `MANDO.INI`. Si no encuentra ninguno no se queda en la memoria (con
`/R` sí). Ejecutado otra vez, vuelve a leer `MANDO.INI` y a buscar los mandos
sin cargar otra copia; `HSPJOY /D` lo descarga; `/I:fichero` usa otro
fichero de configuración.

| Orden | Qué hace |
|---|---|
| `MANDO` | Lista de los mandos que hay, cuál es cada jugador y si hay mando a teclado. |
| `MANDO /VER` | Ejes (0-255), cruceta y botones de cada mando, en vivo. |
| `MANDO /PROBAR` | Lo que ve un juego por INT 15h (el mando clásico), en vivo. |
| `MANDO /CALIBRAR` | Calibra el mando del puerto de juegos (centro y extremos) y lo guarda en `MANDO.INI`. |

Sin `/F` (o `/U`) HSPUSB no le quita el USB a la BIOS si esta lo está usando
(teclado USB, arranque por USB): entonces no hay mandos USB. Ver el README,
«USB propio».

## MANDO.INI

```ini
[mandos]
jugador1=auto        ; auto, ninguno, usb1, usb2, juegos1, juegos2, lpt1, lpt2
jugador2=auto
lpt=nes              ; ninguno, atari, megadrive, nes, snes, psx
puerto=1             ; 1 = LPT1, 2 = LPT2, 3 = LPT3
lpt_mandos=2         ; mandos en el cable (nes, snes, psx): 1 o 2
juegos=si            ; no = no mirar el puerto de juegos (201h)
sondeo=no            ; si = leer los mandos en cada tic aunque nadie los pida

[teclas1]            ; mando a teclado del jugador 1
arriba=ARRIBA
abajo=ABAJO
izquierda=IZQUIERDA
derecha=DERECHA
boton1=CTRL
boton2=ALT
boton3=ESPACIO
boton4=INTRO
boton10=ESC
;activo=no           ; para dejar la sección escrita sin usarla

[teclas2]            ; y del jugador 2
boton1=Q

calibrar=56,300,544,56,300,544,0,200,400,0,200,400   ; lo escribe MANDO /CALIBRAR
```

- En «auto», el jugador 1 es el primer mando que haya, por este orden: USB 1,
  USB 2, puerto de juegos A y B, impresora 1 y 2; el jugador 2, el primero
  de los demás.
- Teclas: letras y cifras, `ARRIBA ABAJO IZQUIERDA DERECHA INTRO ESC ESPACIO
  CTRL ALT MAYUS TAB RETROCESO INSERT SUPR INICIO FIN REPAG AVPAG`, `F1` a
  `F12` y `NADA`. Botones del 1 al 16. Se acepta `botón1`.
- La palanca izquierda también hace de cruceta para las teclas (por debajo de
  64 o por encima de 192).
- Una línea que no se entiende se avisa con su número y se salta.

### Mando a teclado

Las teclas se entregan como si vinieran del teclado: orden D2h del
controlador 8042, igual que hace HSPUSB con el teclado USB, así que las
reciben la BIOS, `TECLADO SP` y los juegos que leen el puerto 60h con su
propia INT 09h. Se manda la pulsación y la suelta de cada tecla, y la última
mantenida se repite (a los 0,5 s, unas 9 por segundo). En un equipo sin 8042
(los Mac) van al búfer de la BIOS (INT 16h AH=05h) y Ctrl, Alt y Mayús a los
indicadores de 0040:0017: sirven para los programas que leen por la BIOS o
por DOS, no para los que leen el puerto 60h.

Los mandos se miran 18,2 veces por segundo (el tic del reloj). Un botón que
se pulsa y se suelta entre dos tics no se pierde (queda anotado hasta el
siguiente).

## Cómo lo ven los juegos: INT 15h AH=84h

Como el mando de la BIOS de un AT:

| Llamada | Devuelve |
|---|---|
| `AH=84h DX=0` | `AL` bits 4-7 = botones A1, A2, B1, B2 como en el puerto 201h (0 = pulsado); CF = 0 |
| `AH=84h DX=1` | `AX` = X del mando A, `BX` = Y de A, `CX` = X de B, `DX` = Y de B; CF = 0 |
| otro `DX` | CF = 1, `AH` = 86h |

- El mando A es el jugador 1. Si hay jugador 2, es el mando B (sus botones 1
  y 2 son B1 y B2). Si no, B es la segunda palanca del jugador 1 (si tiene
  cuatro ejes) y B1 y B2 son sus botones 3 y 4: como un mando de 4 ejes y 4
  botones.
- Los ejes van de 1 a 383 (una vez y media el valor de 0-255, más 1; el
  centro es 193); un eje que no existe da 0, como en la BIOS. Los juegos
  calibran por su cuenta, así que solo importa que sea lineal.
- La cruceta manda sobre la palanca: si está pulsada, ese eje va al extremo.
  Así un mando digital (NES, Atari) mueve a los juegos que esperan palanca.
- Si no hay ningún mando, la llamada pasa a la BIOS, como si HSPJOY no
  estuviera.

Los juegos que leen el puerto 201h directamente (la mayoría de los de
acción) no pasan por aquí: para ellos hará falta el puerto 201h virtual de
HSPEMM (siguiente apartado). Un mando analógico de verdad en un puerto de
juegos de verdad lo siguen leyendo ellos mismos, claro.

## Interfaz propia: INT 2Fh AH=D9h

| AX | Entrada | Salida |
|---|---|---|
| D900h | | ¿Está HSPJOY? `AL` = FFh, `BX` = 484Ah («HJ»), `CX` = 0100h (versión), `ES:DI` → datos (tabla de abajo) |
| D901h | | El mando clásico, recién leído: `AL` = botones (bits 4-7 como el 201h, 0 = pulsado) y ejes que hay (bits 0-3: X1, Y1, X2, Y2), `BL` = X1, `BH` = Y1, `CL` = X2, `CH` = Y2 (0-255, 128 = centro) |
| D902h | `DL` = mando (0-5) | `AL` = FFh, `ES:DI` → su entrada de la tabla, recién leída; CF = 1 si ese número no existe |
| D903h | `DL` = mando USB (0 o 1), `ES:DI` → estado (18 bytes) | Un driver entrega el estado de un mando. `AL` = FFh. La llama HSPUSB con cada informe y cuatro veces por segundo |
| D904h | | La configuración (en los datos) ha cambiado: se sueltan las teclas y se recalcula. `AL` = FFh |

Si HSPJOY no está, `AL` vuelve sin cambiar.

### Los datos (ES:DI de AX=D900h)

Los nombres `HJD_*` y `HJS_*` están en `lib/hidjoyx.inc`.

| Desplaz. | Tamaño | Qué |
|---|---|---|
| 00h | 2 | «HJ» |
| 02h | 1 | Versión de la estructura (1) |
| 03h | 1 | Mandos de la tabla (6) |
| 04h | 1 | Bytes de cada entrada (32) |
| 06h | 2 | Desplazamiento de la tabla de mandos (en el mismo segmento) |
| 08h | 1 | **Mando clásico**: botones (bits 4-7 como el 201h, 0 = pulsado) y ejes que hay (bits 0-3) |
| 09h | 4 | **Mando clásico**: X1, Y1, X2, Y2 (0-255) |
| 0Dh | 1 | Mando que hace de jugador 1 (FFh = ninguno) |
| 0Eh | 1 | Mando que hace de jugador 2 |
| 10h | 1 | Jugador 1 pedido (FFh = automático) |
| 11h | 1 | Jugador 2 pedido (FFh = automático, FEh = ninguno) |
| 12h | 1 | Opciones: bit 0 = sondear en cada tic; bits 1 y 2 = mando a teclado de los jugadores 1 y 2 |
| 13h | 1 | Mando del cable de impresora (tipo; 0 = ninguno) |
| 14h | 2 | Puerto de impresora |
| 16h | 2 | Puerto de juegos (0 = no se mira) |
| 18h | 2 | Espera máxima del puerto de juegos (vueltas) |
| 1Ah | 24 | Calibración: mínimo, centro y máximo (palabras) de X1, Y1, X2 e Y2 |
| 32h | 40 | Teclas: 20 por jugador (arriba, abajo, izquierda, derecha, botones 1-16); código de tecla, bit 7 = prefijo E0h |
| 5Ah | 1 | Ejes del puerto de juegos que existen (bits 0-3) |
| 5Eh | 8 | Última medida del puerto de juegos (vueltas de cada eje) |

Cada entrada de la tabla de mandos (índices: 0 y 1 USB, 2 y 3 puerto de
juegos A y B, 4 y 5 impresora):

| Desplaz. | Tamaño | Qué |
|---|---|---|
| 0 | 1 | Tipo: 1 USB HID, 2 USB tipo Xbox 360, 3 puerto de juegos, 4 Atari, 5 Mega Drive, 6 NES, 7 Super Nintendo, 8 PlayStation (9 reservado: serie) |
| 1 | 1 | Bit 0: presente |
| 2 | 1 | Ejes que tiene (0-6) |
| 3 | 1 | Botones que tiene (0-16) |
| 4 | 2+2 | USB: fabricante y producto. Los demás: su puerto |
| 8 | 6 | Ejes: X, Y, X2, Y2 (segunda palanca, o timón y gases), gatillo izquierdo, gatillo derecho. 0-255; centro 128; gatillos sueltos = 0; Y crece hacia abajo |
| 14 | 1 | Cruceta: bit 0 arriba, 1 abajo, 2 izquierda, 3 derecha |
| 16 | 2 | Botones 1-16 (bit 0 = botón 1) |
| 18 | 3 | Interno (pulsaciones pendientes para las teclas) |

Los primeros 18 bytes son el «estado» que entrega un driver con AX=D903h.

### Para HSPEMM: el puerto 201h virtual

El monitor V86 puede dar a los juegos un puerto 201h hecho con estos datos
sin llamar a nadie desde su rutina de fallo:

1. Una vez (al cargarse, o la primera vez que alguien toca el 201h): `INT
   2Fh AX=D900h`; si `AL` = FFh y `BX` = 484Ah, guardar la dirección lineal
   de `ES:DI`. Poner a 1 el bit 0 del byte 12h (opciones) para que HSPJOY
   refresque en cada tic los mandos que hay que sondear (los USB llegan
   solos).
2. **Escritura en 201h**: anotar el instante (o poner a cero una cuenta de
   lecturas) y copiar los cinco bytes 08h-0Ch.
3. **Lectura de 201h**: bits 4-7 = los del byte 08h. Bits 0-3: a 1 mientras
   no haya pasado el tiempo de ese eje; si el eje no existe (su bit del byte
   08h a 0), a 1 siempre, que es lo que hace un puerto sin mando.
4. El tiempo de un eje de valor `v` (0-255): en un puerto de verdad es
   `24,2 µs + 0,011 µs × R`, con `R` de 0 a 100 kΩ, o sea de unos 24 µs a
   1,1 ms; `t = 24 + 4,3 × v` µs lo reproduce (centro ≈ 575 µs). Si el
   monitor no tiene reloj fino, sirve contar lecturas del puerto (lo que hace
   el simulador de las pruebas, `tests/progs/joysim.inc`): los juegos miden
   en vueltas de su bucle y calibran.

Con un mando de verdad en un puerto de juegos de verdad lo natural es no
virtualizar (dejar pasar el 201h); el monitor puede saberlo mirando la tabla:
tipo 3 en el jugador 1.

HSPUSB funciona también en modo V86 (con HSPEMM cargado llega a los
registros del xHCI por INT 15h AH=87h; ver [HSPEMM.md](HSPEMM.md), «HSPUSB en
modo V86»), así que los mandos USB van igual en la opción «Juegos» del menú
de arranque, lo mismo que los del puerto de impresora.

## Mandos USB

- **HID genéricos.** HSPUSB toma las interfaces HID que no son teclado ni
  ratón de arranque, pide su descriptor de informe (hasta 1 KB) y busca una
  colección de aplicación *Joystick*, *Game Pad* o *Multi-axis Controller*.
  De ahí saca dónde van X, Y, Z, Rx, Ry, Rz, Slider, Dial, la cruceta (*Hat
  switch* de 8 o 4 posiciones, o cuatro botones *D-pad*), los botones 1-16 y,
  de la página de simulación, timón, gases, acelerador y freno. Respeta los
  identificadores de informe, Push/Pop, las listas y rangos de usos, los usos
  de 4 bytes y los máximos escritos con signo. El detalle del reparto en los
  seis ejes está en `lib/hidjoyp.inc`.
- **Tipo Xbox 360 con cable** (interfaz FFh/5Dh/01h; también los muchos
  mandos de otras marcas que se presentan así, «XInput»): informe fijo de 20
  bytes. Botones: 1 A, 2 B, 3 X, 4 Y, 5 LB, 6 RB, 7 Back, 8 Start, 9 y 10 los
  pulsadores de las palancas, 11 Guía.
- Hasta **dos** mandos a la vez, también detrás de concentradores.
- **No se admiten** (todavía): los que necesitan un saludo propio antes de
  dar informes (Xbox One/Series por cable, Switch Pro por USB, el receptor
  inalámbrico de Xbox 360), conectar el mando después de arrancar, los LED y
  la vibración. Un mando que comparta dispositivo con un teclado o un ratón
  de arranque se queda como teclado o ratón.
- **Retraso.** HSPUSB recoge los informes con el tic del reloj. Tras atender
  el de un mando espera al siguiente lo que dura su intervalo (10 ms como
  mucho, medidos con el registro MFINDEX del controlador), para que el estado
  que se entrega sea de ese momento y no de hace un tic. El estado se renueva,
  pues, 18 veces por segundo.

## Puerto de juegos (201h)

Al escribir en el puerto, los bits 0-3 se ponen a 1 y cada uno baja al cabo
de un tiempo proporcional a la posición de su eje. HSPJOY cuenta las vueltas
de un bucle (con las interrupciones cortadas, unos 1,2 ms como mucho con un
mando normal; la espera máxima, unos 3,5 ms, se mide al cargarse con el
reloj) y las pasa a 0-255 con la calibración: mínimo, centro y máximo de cada
eje. Al cargarse da por bueno que la palanca está en el centro; `MANDO
/CALIBRAR` mide el centro y los extremos de verdad (con un margen del 6 % para
que se llegue seguro a 0 y a 255) y lo guarda. La calibración vale para ese
mando en ese equipo.

Un eje que no baja a tiempo no existe: con X1 e Y1 hay un mando de 4 botones;
con los cuatro, dos mandos de 2 botones (o uno de 4 ejes: el jugador 1 lo ve
entero por INT 15h).

## Puerto de impresora: los cables

> Un puerto de impresora no está protegido: un cable mal hecho puede
> estropearlo. Hágalo con el equipo apagado, repase las patillas antes de
> enchufar y no conecte ni desconecte nada con el equipo encendido. Los
> esquemas son los que circulan desde los años noventa para estos mandos
> (los mismos que usan otros sistemas), así que un cable ya hecho para ellos
> sirve.

Patillas del puerto (DB25): 1 STROBE, 2-9 datos D0-D7, 10 ACK, 11 BUSY,
12 PE, 13 SELECT, 14 AUTOFD, 15 ERROR, 16 INIT, 17 SELECT IN, 18-25 masa.

### NES y Super Nintendo (`lpt=nes`, `lpt=snes`)

| Señal del mando | NES (7 patillas) | Super Nintendo (7 patillas) | Puerto de impresora |
|---|---|---|---|
| Masa | 1 | 7 | 18 a 25 (cualquiera) |
| Reloj | 2 | 2 | 2 (D0) |
| Carga (latch) | 3 | 3 | 3 (D1) |
| Datos | 4 | 4 | 10 (ACK) el mando 1; 11 (BUSY) el mando 2 |
| +5 V | 7 | 1 | 4 a 9 (D2-D7), cada una con su diodo (1N4148, la raya hacia el mando), juntas |

La numeración del conector de Super Nintendo es la habitual (la 1 en el lado
plano: `1 [oooo|ooo) 7`). El de NES no tiene una numeración oficial: la de la
tabla es la de las hojas de patillas más corrientes; antes de conectar nada,
compruebe con un polímetro cuáles son la masa y los +5 V de su alargador.

El reloj y la carga van a los dos mandos; cada uno tiene su línea de datos.
Se lee así: un pulso en la carga y, por cada botón, se lee la línea (0 =
pulsado) y se da un pulso de reloj. NES: A, B, Select, Start, arriba, abajo,
izquierda, derecha (botones 1 A, 2 B, 3 Select, 4 Start). Super Nintendo: B,
Y, Select, Start, cruceta, A, X, L, R (botones 1 B, 2 A, 3 Y, 4 X, 5 L, 6 R,
7 Select, 8 Start).

### PlayStation (`lpt=psx`)

| Señal del mando | Mando (9 patillas) | Puerto de impresora |
|---|---|---|
| Datos | 1 | 10 (ACK) el mando 1; 11 (BUSY) el mando 2 |
| Órdenes | 2 | 2 (D0) |
| Masa | 4 | 18 a 25 |
| Alimentación (3,3-5 V) | 5 | 5 a 9 (D3-D7), cada una con su diodo, juntas |
| Selección | 6 | 3 (D1) |
| Reloj | 7 | 4 (D2) |

La línea de datos del mando es de colector abierto: si no contesta, hace
falta una resistencia de 1 a 10 kΩ entre datos (1) y alimentación (5). La
vibración (patilla 3, 7-9 V) no se conecta. Valen los mandos digitales y los
analógicos (DualShock; con el botón «Analog» encendido dan las dos palancas).
Botones: 1 Cruz, 2 Círculo, 3 Cuadrado, 4 Triángulo, 5 L1, 6 R1, 7 L2, 8 R2,
9 Select, 10 Start, 11 L3, 12 R3. El mando se da por presente solo cuando
contesta (5Ah en el tercer byte), así que se puede enchufar después.

### Joystick Atari/Commodore y Mega Drive (`lpt=atari`, `lpt=megadrive`)

Necesitan un puerto **bidireccional** (modo PS/2, EPP o ECP en la BIOS; casi
todos desde mediados de los noventa): HSPJOY pone las patillas de datos en
entrada y avisa si el puerto no lo parece.

| Señal | DB9 del mando | Puerto de impresora |
|---|---|---|
| Arriba | 1 | 2 (D0) |
| Abajo | 2 | 3 (D1) |
| Izquierda | 3 | 4 (D2) |
| Derecha | 4 | 5 (D3) |
| Botón (Atari) / B y A (Mega Drive) | 6 | 6 (D4) |
| Segundo botón / C y Start (Mega Drive) | 9 | 7 (D5) |
| Masa | 8 | 18 a 25 |
| +5 V (solo Mega Drive) | 5 | 1 (STROBE) |
| Selección (solo Mega Drive) | 7 | 14 (AUTOFD) |

Con palancas de interruptores puede hacer falta una resistencia de 10 kΩ de
la patilla 1 del puerto a cada una de las patillas 2 a 7 (pruebe antes sin
ellas). En la Mega Drive, con la selección arriba se leen B y C y con ella
abajo A y Start (botones 1 B, 2 C, 3 A, 4 Start); el mando de 6 botones va
como uno de 3. Un joystick Atari en el cable de Mega Drive también funciona.

## Puerto serie (diseño)

No está hecho. Encaja así, sin cambiar nada de lo anterior:

- **Dónde:** en `HSPJOY.COM`, como otra procedencia. La tabla tiene sitio
  para crecer (los programas leen de los datos cuántas entradas hay y cuánto
  ocupa cada una); el tipo 9 está reservado.
- **Cómo llega:** a diferencia del 201h y la impresora, el puerto serie avisa:
  una rutina de la IRQ del UART (8250/16550; IRQ 4 o 3) guarda los bytes en
  un búfer circular pequeño y un analizador por protocolo junta los paquetes
  y deja el estado en la entrada del mando, igual que AX=D903h (también anota
  las pulsaciones pendientes). No hay sondeo.
- **Configuración:** `serie=spaceorb` (o el modelo que sea) y `com=1` en
  `[mandos]`; `jugador1=serie1`.
- **Protocolos:** cada mando serie tiene el suyo (velocidad, paquetes con un
  byte de sincronía y varios de datos): hay que tomarlos de su documentación
  pública y probarlos con el aparato. Por eso no se ha escrito a ciegas.

## Lo que falta

- **Probar con hardware de verdad**: todo lo de arriba está probado en QEMU
  con dispositivos simulados (ver [Pruebas](#pruebas)).
- **Puerto 201h virtual** con HSPEMM, para los juegos que leen el puerto.
- **Mandos digitales del puerto de juegos** (Gravis GrIP, SideWinder): no usan
  los tiempos de los ejes sino paquetes de bits a varios kilohercios por las
  líneas de los botones, con tiempos que hay que medir con el contador del
  reloj y que no se pueden ajustar sin el mando delante. Sin su modo digital
  estos mandos ya funcionan como un mando analógico normal (el GamePad Pro
  tiene un interruptor; los SideWinder arrancan en analógico).
- **Puerto serie** (apartado anterior).
- USB: conectar en caliente, saludos de Xbox One/Series y Switch Pro, LED y
  vibración, más de dos mandos; mando de Mega Drive de 6 botones; más de un
  joystick Atari (interfaz de varios).
- API en Hispa Ventanas y apartado «Mando» en Configuración (los juegos
  propios pueden usar ya INT 2Fh AX=D901h/D902h).

## Pruebas

`tests/run_joy_tests.sh` (dentro de `make test`), todo en QEMU:

| Caso | Qué prueba | Con qué |
|---|---|---|
| `hid` | El analizador de descriptores (el mismo código de HSPUSB) y el paso de informe a estado | Los descriptores de informe de cinco mandos reales (DualShock 4, DualSense, Switch Pro, Luna y Stadia; `tests/progs/hidmandos.inc`), dos hechos a mano (con signo, cruceta de botones, Push/Pop, elemento largo; joystick de vuelo de 10 bits) y el formato Xbox 360; un teclado y un ratón no pasan por mandos; un descriptor cortado no cuelga |
| `usb`, `x360`, `dos` | La cadena entera: enumeración por el xHCI, descriptor de informe de 507 bytes, endpoint de interrupción, informes, INT 2Fh, INT 15h, `MANDO`, teclas | Un mando USB de mentira: el dispositivo `usb-redir` de QEMU con `tests/tools/usbmando.py` al otro lado, que se presenta con los descriptores del mando real y manda los informes que dice la prueba. Con teclado y ratón USB a la vez, y con un mando detrás de un concentrador |
| `juegos` | Medida, 2 y 4 ejes, botones, calibración, `MANDO.INI` | `HSPJOYT.COM`: el mismo `hspjoy.asm` ensamblado con `-DPRUEBA`, en el que las entradas y salidas del puerto van a un simulador (`tests/progs/joysim.inc`) |
| `impresora` | Los protocolos de NES, Super Nintendo, PlayStation, Atari y Mega Drive bit a bit, dos mandos, y el mando a teclado (el mando teclea `VER` y `DIR`) | El mismo simulador: un registro de desplazamiento, un mando de PlayStation que solo contesta si se le manda 01h, las líneas del DB9 |
| `imagen` | Que el disquete y la imagen USB llevan HSPJOY, MANDO y MANDO.INI, y que sin mandos HSPJOY no ocupa memoria | Las imágenes de distribución |

Lo que estas pruebas **no** pueden decir: que los tiempos de los cables
(pausas de microsegundos) valgan para todos los mandos, que el puerto de
juegos de una tarjeta concreta se comporte como el modelo, que la orden D2h
del 8042 llegue a los juegos en todos los equipos (en algunos con emulación
USB de la BIOS no llega), ni cómo se portan los mandos USB de verdad (los que
mandan informes sin parar, los que tardan en presentarse).
