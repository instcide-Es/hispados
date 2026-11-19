# HispaBIOS: Hispa-DOS en equipos que solo arrancan por UEFI

**Estado (19/11/2026):** versión 0.6: el **teclado USB del firmware** ya no se
queda mudo (ver «El teclado USB del firmware se quedaba sin reloj»: el MEDION
S2217, **pendiente de probar en ese equipo** con la imagen de la carpeta
`prueba-medion` del FTP). La 0.5 trajo el **ratón** (ver «El ratón con UEFI»).
Probada en QEMU con OVMF (64 y 32 bits, i440fx y q35, hasta 8 GB, pantalla de
1920x1200, con y sin 8254 y HPET, ratón USB, teclado solo USB sin 8042). La 0.3 arranca en equipos de
verdad: ThinkPad E14 (Raptor Lake), MacBook Air de 2017 y Asus GL553V (Kaby
Lake; con sonido). En el **Monovate 14.1** (Core i3-1115G4, Tiger Lake) se
quedaba a la mitad: el reloj no andaba. La 0.4 lleva el arreglo (ver «El reloj
en los equipos con el 8254 parado») y **está pendiente de probar en ese
equipo** con la imagen de prueba del FTP, carpeta `prueba-hispabios`. El ratón
de la 0.5 está **pendiente de probar en el ThinkPad E14 y el MacBook Air de
2017** con la imagen de la carpeta `prueba-hispabios-raton`.

La parte de **32 bits** (`efi32.asm`, para firmware UEFI de 32 bits) está al
nivel de la de 64 desde el 18/11/2026 y lleva además la **pantalla girada** de
las tabletas (ver «La parte de 32 bits»). Probada en QEMU con OVMF de 32 bits
(procesador de 64 y de 32, 1 GB, 1280x800 y 800x1280 vertical); **pendiente de
probar en un equipo de verdad**: el Acer One 10 (Atom «Bay Trail»). Desde la
0.5 la de 64 bits hace también lo que estrenó la de 32: conectar las
controladoras de disco, los discos que no son el de arranque (81h...), el
equipo en el registro y la memoria extendida según el mapa.

## Qué es

Los equipos sin CSM (sin «arranque heredado») no tienen BIOS: no hay INT 13h,
INT 10h ni INT 16h, y Hispa-DOS, que es un DOS de modo real, no puede arrancar.
HispaBIOS es una aplicación UEFI (`\EFI\BOOT\BOOTX64.EFI` y `BOOTIA32.EFI` en la
memoria USB) que **hace de BIOS**: pide al firmware la memoria baja, pone en
ella una BIOS de modo real propia, baja el procesador al modo real y arranca el
sector de arranque de siempre. Cuando DOS pide algo a la BIOS (disco, teclado,
pantalla), la BIOS vuelve al modo del firmware, se lo pide a él (Block I/O,
entrada de texto, GOP) y baja otra vez. En los equipos con BIOS o CSM no
interviene: la memoria arranca por el MBR.

| Fichero | Qué es |
|---|---|
| `hispabios/efi64.asm` | La parte UEFI de 64 bits: memoria, pantalla, discos, el registro y el despachador de las peticiones |
| `hispabios/efi32.asm` | Lo mismo para firmware de 32 bits (tabletas y portátiles con Atom): arranque, memoria, registro, discos y tablas |
| `hispabios/efi32_reloj.inc`, `efi32_pant.inc`, `efi32_datos.inc` | El resto de la parte de 32 bits: el reloj y el despachador; la pantalla (escritura combinada, VBE, el giro); textos y variables |
| `hispabios/efi64_raton.inc`, `efi32_raton.inc` | El ratón (0.5): los del firmware y los ratones USB que lee HispaBIOS |
| `hispabios/efi64_tecl.inc`, `efi32_tecl.inc` | El teclado (0.6): qué teclado da el firmware y los tics de su reloj en cada viaje |
| `hispabios/bios64.asm`, `bios_int.inc` | La BIOS de modo real y los cambios de modo (con `-DARQ32`, para firmware de 32 bits) |
| `hispabios/pe.inc` | La cabecera PE/COFF, escrita a mano |
| `hispabios/hblog.asm` | `HBLOG.COM`: enseña y guarda (`\HBLOG.TXT`) el registro de la BIOS de modo real |

## Lo que hace al arrancar (efi64.asm)

1. Quita el **vigilante** del firmware (`SetWatchdogTimer(0)`), que reinicia el
   equipo a los 5 minutos de arrancar un cargador.
2. Lee el **mapa de memoria** (`GetMemoryMap`): primero pregunta el tamaño y
   pide el búfer con hueco de sobra (el mapa de un equipo actual tiene cientos
   de entradas y crece al pedir el propio búfer); respeta el tamaño de entrada
   que diga el firmware (48 octetos en OVMF, no 40). En pantalla enseña lo que
   hay por debajo de 1 MB; el mapa entero va al registro.
3. Pide al firmware, **página a página, la memoria baja** (0 a 9FFFFh,
   `AllocatePages(AllocateAddress)`). La memoria convencional de DOS llega
   hasta la primera página (de la 1 en adelante) que el firmware no da.
4. Elige **dónde va la BIOS de modo real** (60 KB seguidos): 90000h o, si el
   firmware usa esa zona (es corriente que tenga reservadas las últimas páginas
   antes de A0000h), 70000h o 60000h. Hay una copia de la BIOS ensamblada para
   cada sitio (`BIOS64.BIN`, `BIOS64_7.BIN`, `BIOS64_6.BIN`). En 80000h no puede
   ir: ahí se copia el sector de arranque de Hispa-DOS (`RELOC_SEG`), y por lo
   mismo esa página tiene que estar libre.
5. Busca la pantalla (GOP, 640x400 o más, color de 32 bits) y los discos (Block
   I/O de 512 octetos; el de arranque es el 80h, los demás 81h en adelante;
   antes pide al firmware que ponga en marcha sus controladoras de disco),
   busca el ratón (0.5), copia la BIOS y la arranca.

Desde la 0.3, además: **presta a DOS las páginas sueltas** de memoria baja que
usa el firmware, **copia las entradas de ACPI y SMBIOS** donde las ve un
programa de modo real, prepara la pantalla (**escritura combinada**, medida de
la velocidad) y el **marco de VBE**. Todo queda en el registro.

### Modo seguro

Si al arrancar se va pulsando la tecla **S**, HispaBIOS no hace nada de lo que
depende del equipo: ni el alias de escritura combinada, ni cambiar de modo de
pantalla, ni prestar páginas. Es la salida si un firmware se lleva mal con algo
de eso (sale `MODO SEGURO` y queda en el registro).

## La memoria baja: páginas prestadas

Casi todos los firmware se quedan una página suelta de la memoria baja (la del
arranque de los otros procesadores): 87000h en el ThinkPad E14, 58000h en el
MacBook Air, que dejaba a DOS con **352 KB**. Como la página 0, esas páginas
se **prestan**: en cada cambio de modo se intercambia su contenido
(`paginas_cambiar` en `bios64.asm`), el firmware ve siempre lo suyo y DOS lo
suyo. Solo las que son memoria normal del firmware según el mapa (tipos 0 a 4,
con atributos de caché), por debajo de la BIOS de modo real y como mucho
cuatro; el resto sigue cortando la memoria convencional. Con ello los dos
equipos dan 576 KB. También se presta la del sector de arranque (80000h).

## ACPI y SMBIOS

Con UEFI la RSDP de ACPI y la entrada de SMBIOS solo están en la tabla de
configuración del firmware; EQUIPO decía «ACPI: no hay» y HSPACPI (la batería)
no habría encontrado nada. HispaBIOS copia las dos entradas a una «EBDA» de
1 KB dentro de su zona (40:0Eh apunta a ella): la RSDP en +10h y la de SMBIOS
(«_SM_» o «_SM3_») en +40h. Si además F0000h es memoria libre que se puede
escribir (lo que haría un CSM) las pone también allí; en OVMF no lo es, y por
eso EQUIPO busca SMBIOS también en la EBDA. Las tablas en sí no se mueven.

## La pantalla

La pantalla de texto de DOS (80x25) se dibuja ampliada sobre el marco del
firmware (GOP); solo las celdas que cambian.

- **Escritura combinada (WC).** El firmware deja el marco sin caché (UC) y cada
  punto escrito va al bus por separado. HispaBIOS se hace unas tablas de
  páginas propias —copia del nivel 4 del firmware más una entrada con el marco
  en páginas de 2 MB de tipo WC (la entrada 5 del PAT)— y pinta por ese
  **alias**, que solo está puesto mientras dibuja. Vale igual con el marco por
  encima de 4 GB. No se tocan los MTRR ni las tablas del firmware.
- **Medida.** Al arrancar pinta la pantalla entera en negro por los dos caminos
  y apunta lo que tarda cada uno; usa el más rápido.
- **Si aun así es lenta** (más de 100 ms la pantalla entera): pasa al modo más
  grande del firmware que se pinte en ese tiempo (de 800x600 en adelante; el
  equipo lo amplía) y, si no hay, amplía el texto solo por un entero.
- Cada fila de puntos se prepara en memoria normal y va a la pantalla de una
  vez (`pintar_celda`).

### Modos gráficos: VBE

Los equipos que solo tienen UEFI no llevan VBE ni VGA: Hispa Ventanas decía «no
se ha podido poner el modo gráfico». La BIOS de HispaBIOS da **VBE 2.0**
(INT 10h 4F00h a 4F03h) con modos de **color directo de 32 bits y marco
lineal**: 640x480 (112h), 800x600 (115h) y 1024x768 (118h), los que quepan en
la pantalla. El marco es memoria normal **por debajo de 4 GB** (un programa de
32 bits no llega a una pantalla que está en 256 GB); en cada tic la parte UEFI
compara cada línea con lo ya pintado y lleva a la pantalla las que han
cambiado, ampliadas por un entero y centradas (800x600 en 1920x1200: al doble).
No hay modos de 8 bits (su paleta se pone por los puertos de la VGA, que no
existen) ni ventanas de 64 KB en A0000h. INT 10h AH = 00h vuelve al texto.

## El ratón con UEFI (0.5)

**El fallo.** En el ThinkPad E14 y el MacBook Air de 2017, arrancando por UEFI,
Hispa Ventanas no tenía ratón ni panel táctil. Con UEFI no hay emulación del
ratón PS/2 (la BIOS «heredada» la hacía con el ratón USB) y el panel táctil va
por USB (el Mac) o por I2C (el E14); HSPUSB no se carga (el USB es del
firmware) y la BIOS de HispaBIOS no daba ratón: HSPRATON decía «fallo en
C205h, AH=86h» y no había nada.

**El arreglo.** La parte UEFI busca el ratón al arrancar (`raton_preparar`, en
`efi64_raton.inc` y `efi32_raton.inc`), por este orden:

1. Pide al firmware que ponga en marcha sus **controladoras USB** (PCI clase
   0C03h, `ConnectController` recursivo): arrancando «deprisa» solo enciende
   lo necesario para arrancar.
2. El **ratón del firmware**: cada `EFI_SIMPLE_POINTER_PROTOCOL` (ratones y,
   si el firmware lo lleva, el panel táctil) y `EFI_ABSOLUTE_POINTER_PROTOCOL`
   (pantallas táctiles, tabletas). No el de la consola de entrada
   (`ConsoleInHandle`): ese reúne a los demás (en EDK II, ConSplitter) y lo
   hay aunque no haya ningún ratón.
3. Un **ratón USB que el firmware no lleva** (OVMF no lleva ratón; muchos
   firmware tampoco): cada interfaz HID de ratón (`EFI_USB_IO_PROTOCOL`, clase
   03h, protocolo 02h) que no tenga Simple Pointer ni la tenga abierta un
   controlador del firmware. Si es de arranque (subclase 01h), o acepta el
   protocolo de arranque, se le pide ese protocolo (`SET_PROTOCOL`: informes
   de botones, X, Y y rueda), `SET_IDLE` y se le pone una **transferencia de
   interrupción asíncrona**: el firmware atiende el USB durante los viajes,
   como hace con su teclado USB, y llama a `raton_cb` con cada informe.

Mientras DOS tiene el ratón activado (`T_RAT_ON`), en **cada viaje al
firmware** la parte UEFI pregunta a los ratones del firmware (`GetState`) y
suma lo que se han movido en la tabla de la BIOS (`T_RAT_DX/DY`, en cuentas
del PS/2) y los botones (`T_RAT_BOT`); `raton_cb` hace lo mismo con los
informes USB. El tic de la BIOS hace un viaje si no tenía ya que dibujar (en
los modos gráficos ya lo hace en cada tic), y el teclado hace los suyos.

La BIOS de modo real da el ratón por **INT 15h C200h a C207h**, el «ratón PS/2»
de la BIOS de siempre: tras cada viaje (en el tic y tras cada pregunta al
teclado) llama a la rutina que el programa dio con C207h con el estado, X, Y
y 0 en la pila (paquetes de 3 octetos, de -255 a 255 por eje; como mucho 4 por
vez). Es lo que usa HSPRATON por la «vía BIOS», y con él Hispa Ventanas y los
programas de INT 33h. INT 11h pone el bit 2 (hay ratón). **Sin ratón**, C2xxh
contesta «no se admite» (AH = 86h) como antes, y HSPRATON prueba el 8042
directamente (lo que ya hacía).

Las cuentas: un Simple Pointer dice sus cuentas por milímetro; hasta 8 se
pasan tal cual (lo que hace la emulación PS/2 de las BIOS con un ratón USB) y,
si son más (paneles de mucha resolución), se llevan a 8 por milímetro. Un
Absolute Pointer da la posición: se pasa a movimiento con todo el ancho (y
todo el alto) = 1024 cuentas. La rueda se lee pero no se da (los paquetes de
la BIOS son de 3 octetos).

En el **modo seguro** no se conectan las controladoras USB ni se pone el ratón
USB propio (los ratones que ya diera el firmware sí se usan). Lo que se ha
encontrado va al registro (`HISPABIOS.LOG`, apartado «Raton:») y `HBLOG` dice
de dónde sale el ratón y sus cuentas (lecturas del firmware, informes USB,
paquetes entregados).

**Lo que QEMU no puede decir** (lo dirá el registro del equipo): si el
firmware del E14 o del Mac da un Simple Pointer para su panel táctil, si el
panel del Mac aparece como interfaz USB de ratón (en los Mac, el panel
interno es USB y en su modo «de ratón» manda informes de ratón normales: es
lo que espera HispaBIOS si el firmware no lo lleva) o si el panel I2C del E14
no lo da nadie. En el E14 hay además un 8042 con puerto auxiliar que contesta
(el informe de EQUIPO): si no hay ratón del firmware, HSPRATON va al 8042
como siempre.

**No se hace**: los ratones USB que solo dan informes «de informe» (sin
protocolo de arranque) ni las tabletas USB absolutas (el `usb-tablet` de
QEMU): haría falta leer su descriptor de informe. El panel táctil por I2C o
SPI sin controlador del firmware no tiene arreglo desde aquí.

## El teclado y el reloj

- DOS mira el teclado en cada letra que escribe (Ctrl+C) y cada pregunta era
  un viaje al firmware con 1 ms de espera: era lo que más frenaba. Ahora INT
  16h AH = 01h pregunta al firmware **como mucho cada octavo de tic** (7 ms).
  Desde la 0.6, en cada viaje se le dan al firmware los tics de su reloj: sin
  ellos su teclado USB se quedaba mudo (apartado siguiente).
- El reloj de DOS es el PIT por el 8259 y LINT0; los tics que se pierden se
  recuperan con el TSC. Desde la 0.4 la BIOS **comprueba al arrancar que la
  interrupción llega** y, si no, busca otra fuente (apartado siguiente). Si
  ninguna llega, lo dice en pantalla, guarda el registro y los tics se dan al
  entrar en las interrupciones de la BIOS (`k_tics`), como en la 0.3; la
  espera de tecla no usa HLT. `HBLOG` dice si llega y de dónde.

## El teclado USB del firmware se quedaba sin reloj (0.6)

**El fallo.** En el MEDION S2217 MD99526 (2 en 1 con Atom Z3735F «Bay Trail»,
firmware Phoenix UEFI 2.31 de 32 bits, **sin 8042**: el teclado de la base es
USB, en el xHCI 8086:0F35) el teclado iba en el menú de CONFIG.SYS y dejaba
de ir después: en `C:\>`, con F8 (línea a línea) y en la pregunta S/N de
DIAGNOSTICO. El registro (`informes/Medion/HISPABIOS.LOG`, de la 0.4) acababa
bien: el 8254 cuenta y la IRQ 0 llega a DOS.

**La causa.** El teclado USB del firmware (UsbKbDxe de EDK II y los que salen
de él) no se lee al preguntarle (`ReadKeyStroke` solo mira su cola): las
teclas llegan por una transferencia de interrupción que el controlador del
xHCI repasa en un **suceso de temporizador** del firmware. Los temporizadores
del firmware solo avanzan con la **interrupción de su reloj** (el protocolo
«Timer Arch»; en el Medion, el 8254 por el 8259, vector 68h, a 18,2 Hz: el
8254 está a 65536 cuentas ya antes de HispaBIOS). Con DOS en marcha el 8254 y
el 8259 son de DOS, y el firmware solo ve un tic si el flanco del 8254 cae
justo dentro de uno de sus viajes (1 ms de espera con las interrupciones
abiertas):

- en el **menú de CONFIG.SYS** el núcleo pregunta por el teclado sin parar
  (INT 16h 01h en bucle, un viaje cada 7 ms): algunos flancos caen dentro y
  el teclado va, con algo de retraso;
- **después**, todo el que espera una tecla (el núcleo en `con_getc_bios`,
  INT 16h 00h en la BIOS) descansa con **HLT hasta el tic siguiente** y
  pregunta justo *después* de cada flanco: el firmware no ve ni uno, sus
  temporizadores no avanzan y el teclado USB se queda mudo para siempre.

La HispaBIOS de la 0.9.26 (0.1/0.2) hacía un viaje en cada pregunta, sin
medida y sin parar: el firmware se llevaba casi todos los tics, y por eso con
ella el teclado iba (y la pantalla iba a tirones: la «lentitud de la 0.2»).
Un teclado **PS/2** no depende de esto (su controlador mira el 8042 al
preguntarle), y en OVMF el reloj del firmware es el del APIC local, que no va
acompasado con el 8254 y siempre cuela algún tic: por eso no se había visto
ni en los equipos probados ni en QEMU. No es cosa del `ConnectController` de
las controladoras (el registro del fallo es de la 0.4, que no conecta el
USB), ni de TECLADO (INT 15h 87h no toca nada del firmware y la A20 está
siempre abierta: no escribe en el puerto 92h), ni de las tablas propias de la
escritura combinada.

**Se reproduce en QEMU** con el teclado solo USB (`-machine q35,i8042=off`,
`usb-kbd` en el xHCI) y `-DPRUEBA_FW_SIN_RELOJ` (el firmware no recibe
ninguna interrupción en sus viajes: el TPR del APIC local se queda en F0h):
llega a `C:\>` y el teclado no responde. Con un teclado PS/2, en las mismas
condiciones, sí.

**El arreglo** (`efi32_tecl.inc`, `efi64_tecl.inc`): en **cada viaje al
firmware** se le dan los tics de su reloj que le tocan por el tiempo pasado
(`fwt_dar`: medido con el TSC y el periodo que dice `GetTimerPeriod`; como
mucho 3 por viaje), sin esperar a su interrupción:

1. `GenerateSoftInterrupt()` de su `EFI_TIMER_ARCH_PROTOCOL` (PI, vol. 2): la
   manera prevista de darle un tic «a mano». Se prueba una vez al arrancar;
2. si no lo tiene o no lo admite, y usa el 8259 con la IRQ 0 abierta: **su
   propia rutina de la IRQ 0**, llamada como la llamaría la interrupción (por
   su IDT, vector `T_VEC0`, con el marco de pila de una interrupción).

Con eso corren sus sucesos de temporizador en cada viaje: el repaso del USB
(las teclas, y el ratón USB de la 0.5) y la repetición de teclas. La espera
de 1 ms sigue, y antes de leer la tecla se llama a `CheckEvent(WaitForKey)`,
que es como pregunta un programa de UEFI (hay firmware que atiende ahí su
teclado). En el **modo seguro** (S) no se dan tics: INT 16h pregunta siempre
al firmware y la espera de tecla no usa HLT (`T_TEC_OPC` bit 0), como la 0.2:
más lento, pero el firmware se lleva sus tics; sirve para distinguir en un
equipo de verdad si el fallo es el reloj del firmware.

**El registro.** `HISPABIOS.LOG`, apartado «Teclado:»: la consola de entrada
y si da `SimpleTextInputEx`, cada teclado del firmware (Simple Text Input: si
es USB, con su fabricante y producto), cada interfaz USB HID de teclado y
quién la lleva (el firmware, un controlador sin teclado, o nadie), el reloj
del firmware y su periodo, y cómo se le dan los tics. `HBLOG` dice las teclas
que ha dado el firmware y los tics que se le han dado. La imagen de prueba
(`tools/mkprueba_hispabios.sh ... medion`) lanza sola `TECLAPRU`
(`tests/progs/teclapru.asm`: 20 s esperando teclas con HLT, como el núcleo;
deja `\TECLAPRU.TXT`) y `HBLOG`, sin que haga falta teclear nada.

**No se hace**: leer nosotros un teclado USB que el firmware no lleve (como
se hace con el ratón): el registro lo dice («SIN controlador del firmware»)
y se hará si aparece un equipo así. Si el firmware no da Timer Arch ni usa el
8259, queda como antes (lo dice el registro).

## El reloj en los equipos con el 8254 parado (0.4)

**El fallo.** En el Monovate 14.1 (Core i3-1115G4, firmware AMI de 2024) la
0.3 llegaba a DOS pero se quedaba a la mitad, y EQUIPO decía «Velocidad: no se
ha podido medir (el reloj no avanza)». En las plataformas de Intel recientes
el firmware deja el temporizador **8254 parado** para ahorrar energía («8254
clock gating»: Windows y Linux usan el HPET, el TSC o el APIC): ni cuenta ni
da la IRQ 0. Todo lo que espera tics —el menú de arranque de 5 s, las esperas
de los controladores, HSPSND, Hispa Ventanas, la hora— se queda parado. Se
reproduce en QEMU con `-machine q35,pit=off`: la 0.3 se para tras «HSPJOY».

**El arreglo**, en dos sitios:

1. `efi64.asm`, `reloj_mirar` (antes de arrancar la BIOS):
   - mira si el 8254 **cuenta** (dos lecturas del canal 0 con 2 ms entre
     medias);
   - si no, y el puente 00:1F.0 es de Intel, le **quita el bloqueo**: registro
     ITSSPRC (desplazamiento 3300h del puerto ITSS, C4h, de la «banda lateral»
     del PCH), bit 2 = 8254CGE. Esos registros están en la memoria que dice
     SBREG_BAR, la BAR 10h del dispositivo P2SB (00:1F.1), que el firmware
     deja oculto: se muestra (bit 8 de su registro E0h), se lee la BAR y se
     vuelve a ocultar. Luego mira otra vez si cuenta. (Fuentes: coreboot,
     `soc/intel/common/block/itss` —`itss_clock_gate_8254`— y `p2sb`; Linux,
     `drivers/platform/x86/p2sb.c`; las hojas de datos de los PCH de Intel.
     Vale para los PCH de las series 100 en adelante: Skylake a Raptor Lake.)
     Es lo único que devuelve el **contador** del 8254 a los programas que lo
     leen (el canal 2: el altavoz, las medidas de velocidad): en modo real
     los puertos no se pueden emular;
   - apunta lo que hay —la tabla HPET de ACPI y sus registros, el APIC local
     (LINT0), la tabla APIC (IOAPIC, a qué entrada va la IRQ 0) y las entradas
     0 y 2 del IOAPIC— y pasa a la BIOS la dirección del HPET y del IOAPIC.
     No toca ningún registro que no esté en las tablas de páginas del firmware.
2. `bios64.asm`, `reloj_elegir` (al arrancar la BIOS de modo real): **prueba**,
   con las interrupciones abiertas, si llegan dos interrupciones en cinco tics
   (medidos con el TSC), por este orden:

   | N.º | Fuente | Camino del 8259 al procesador |
   |---|---|---|
   | 0 | La IRQ 0 tal cual: el 8254 (lo de siempre) | LINT0 del APIC local (ExtINT) |
   | 1 | Lo mismo | La entrada 0 del IOAPIC como ExtINT («virtual wire» por el IOAPIC) |
   | 2 y 3 | El **HPET** en el sitio del 8254 («legacy replacement»: el temporizador 0, periódico a 18,2 Hz, entra por la IRQ 0) | Los dos |
   | 4 y 5 | La interrupción periódica del **reloj CMOS** (IRQ 8, 64 Hz); `irq_rtc` llama a INT 08h cuando por el TSC toca un tic | Los dos |

   La fuente y el camino se ponen en 32 bits sin paginación al bajar del
   firmware (`reloj_aplicar`, `reloj_dos`); la entrada 0 del IOAPIC vuelve a
   ser la del firmware en cada viaje. El HPET solo se usa si el firmware no
   lo está usando (temporizador 0 sin interrupción y sin «legacy
   replacement»), y al dejarlo queda como estaba. En el **modo seguro** solo
   se prueban el 8254 y el reloj CMOS por LINT0 (no se tocan el PCH, el HPET
   ni el IOAPIC).

**El registro.** `HISPABIOS.LOG` lleva lo visto por `reloj_mirar` y, por la
petición `PET_REGISTRO`, lo que apunta la BIOS: cada prueba con las
interrupciones recibidas, el estado del 8259 (IRR, ISR, IMR: si la petición
está y no llega al procesador, el fallo es el camino; si no está, la fuente),
dos lecturas del contador del 8254, y cómo queda. Lo mismo sale en `HBLOG`
(`HBLOG.TXT`), que además dice si el 8254 cuenta. Si se usa algo que no sea el
8254, sale una línea en pantalla al arrancar; si **nada** llega, un aviso de
6 segundos y el registro se guarda aunque la imagen no sea la de prueba.

**Límites.** Con el HPET o el reloj CMOS la IRQ 0 llega, pero el contador del
8254 sigue parado: lo que lo lea directamente (el altavoz, algún juego que
mida con el canal 0 o el 2) no irá bien. Con el reloj CMOS, un programa que
use él mismo la IRQ 8 se queda sin ella. No se ha hecho el temporizador del
APIC local: no puede entrar por el vector 08h (los vectores por debajo de 16
no valen en el APIC) y habría que darle el fin de interrupción desde modo
real.

**La «EBDA» por debajo de 8000h.** En el Asus GL553V (la BIOS en 70000h)
EQUIPO decía «ACPI: no hay» y «SMBIOS: no hay tablas» aunque HispaBIOS las
había copiado: la «EBDA» quedaba en 78FAh y EQUIPO, APAGAR y HSPACPI
(`acpi/nucleo/maquina.c`) solo miraban la de 40:0Eh si estaba de 8000h para
arriba. Ahora, de 4000h para arriba.

## Cuando algo falla

No se queda parado: dice en pantalla **qué falta y qué hacer**, guarda el
registro en la memoria USB y espera una tecla (o 2 minutos) antes de volver al
firmware.

| Aviso | Causa | Qué hacer |
|---|---|---|
| `ESTE EQUIPO NO DEJA LIBRE LA MEMORIA BAJA QUE NECESITA HISPA-DOS` | No hay 60 KB seguidos libres en 60000h, 70000h ni 90000h, o el firmware usa la página 80000h | Activar el CSM / arranque heredado, si el equipo lo tiene |
| `ESTE EQUIPO DEJA MUY POCA MEMORIA CONVENCIONAL` | Menos de 256 KB seguidos desde abajo | Lo mismo |
| `ESTE EQUIPO NO DA UNA PANTALLA GRAFICA (GOP)...` | Sin GOP, o con un formato de color que no es de 32 bits | Lo mismo |
| `NO SE ENCUENTRA EL DISCO DE ARRANQUE` | El firmware no da la memoria USB como disco de sectores de 512 | — |
| `No se ha podido leer el mapa de memoria...` | `GetMemoryMap` falla (sale el código) | — |

### El registro de la BIOS: `HBLOG`

Mientras DOS funciona, la BIOS de modo real apunta lo que sirve para depurar
la velocidad: cuántas veces ha dibujado la pantalla y lo que ha tardado (el
último dibujo, el más lento, el total), los viajes al firmware, las preguntas
por el teclado, si llega la IRQ 0 y **cada llamada de VBE con lo que se
contestó** (también un modo de la VGA que no hay). `HBLOG` (en `\HISPADOS`) lo
enseña y lo guarda en `\HBLOG.TXT`; lo pide por INT 15h AX = 4844h.

### Apagar: INT 15h AX = 4846h y 4841h

- **4846h** («HF»): lo que el firmware tenga pendiente de escribir, a los
  discos (`FlushBlocks` de cada Block I/O) → AX = 6866h («hf»), CF = 0.
- **4841h** («HA»): apagar como un sistema UEFI: antes 4846h y luego
  `ResetSystem(EfiResetShutdown)`. Si vuelve, no ha apagado: CF = 1.

Los usa `APAGAR` (y 4846h también `REINICIAR`) después de vaciar los
búferes de DOS: con HispaBIOS se apaga con el firmware y no se toca
`SMI_CMD` ni `PM1a_CNT` (si el firmware no apaga, sigue con ACPI).

### El registro: `HISPABIOS.LOG`

Si algo falla, HispaBIOS deja en la raíz de la memoria USB el archivo
**`HISPABIOS.LOG`** (texto; DOS lo enseña como `HISPAB~1.LOG`) con:

- el fabricante y la revisión del firmware (`FirmwareVendor`,
  `FirmwareRevision`) y la versión de UEFI;
- el **mapa de memoria completo** (inicio, páginas, tipo y atributos de cada
  tramo);
- las páginas bajas que el firmware no ha dado, dónde va la BIOS y cuánta
  memoria convencional queda;
- las páginas prestadas, la pantalla (GOP) y **los modos que da el firmware**,
  **el tipo de caché del marco (MTRR y PAT), el alias de escritura combinada y
  lo que cuesta pintar la pantalla** por cada camino, el marco de VBE, las
  tablas de ACPI y SMBIOS, la memoria extendida, el vector del 8259 y los
  discos;
- todo lo que ha salido en pantalla, hasta el aviso del fallo.

Si todo va bien **no se escribe nada** en la memoria. Para un equipo que se
queda parado sin llegar a avisar está la **imagen de prueba**
(`tools/mkprueba_hispabios.sh`, ensamblada con `-DREGISTRO_PASOS`): guarda el
registro en cada paso, así que el archivo dice hasta dónde llegó.

Desde la 0.5 las dos partes escriben además el **equipo** (fabricante, modelo
y BIOS según SMBIOS), **cada disco** que da el firmware y lo que se ha hecho con
él, y el **ratón** (las controladoras USB, los ratones del firmware y los USB).

## La parte de 32 bits (`efi32.asm`): Atom «Bay Trail», Acer One 10

Muchas tabletas y 2 en 1 con Atom (Z3735F/G y parecidos) tienen un procesador
de 64 bits pero un **firmware UEFI de 32 bits** y sin CSM: solo arrancan
`\EFI\BOOT\BOOTIA32.EFI`. Esa parte hace **todo lo de la de 64** (el
vigilante, el mapa con `AllocatePool` y el tamaño de entrada del firmware, la
base de la BIOS, las páginas prestadas, los avisos, `HISPABIOS.LOG`, el modo
seguro, las entradas de ACPI y SMBIOS en la EBDA, VBE con marco lineal, la
medida de la pantalla y el modo menor si es lenta, y el reloj: `reloj_mirar`
con el 8254 —también el que «se mueve pero no cuenta»—, el bloqueo del PCH, el
HPET y los APIC). La BIOS de modo real es la misma (`bios64.asm` con `ARQ32`),
así que el teclado sin viajes continuos, `reloj_elegir`, VBE y `HBLOG` son los
mismos. Lo que cambia o es propio:

- **Las llamadas al firmware** (convención de C, argumentos en la pila) van
  todas por la macro `FW` y `fw_llamar`, que deja la **pila alineada a 16** en
  cada llamada (lo pide la especificación también en IA32, y es la lección de
  la 0.1 de 64 bits).
- **Escritura combinada.** Un firmware de 32 bits casi nunca usa paginación
  (OVMF sí), así que no hay tablas suyas donde poner un alias: HispaBIOS se
  hace unas **tablas propias** (PAE, 4 GB en páginas de 2 MB tal cual, y las
  del marco de la pantalla con el tipo WC por la entrada 5 del PAT) y las pone
  **solo mientras dibuja**, con las interrupciones cerradas; al acabar
  devuelve al firmware su CR0, CR3 y CR4 (`wc_entrar`, `wc_salir`). Mide los
  dos caminos y usa el más rápido; el registro dice cómo tiene el firmware la
  paginación. Hace falta PAE y PAT (cualquier Atom).
- **La pantalla girada.** Muchas tabletas llevan un panel **vertical**
  (800x1280) puesto de lado: el firmware da un modo de 800x1280 y todo saldría
  tumbado. Otro modo del firmware no lo arregla (el panel sigue de lado). Si
  el modo es más alto que ancho, HispaBIOS **gira la imagen 90 grados a la
  derecha** (lo que piden casi todas: es el «right side up» de la lista de
  paneles de Linux, donde está el Acer One 10 S1003): DOS ve 1280x800 y los
  modos de VBE se ofrecen según esa medida. Todo se dibuja en una **pantalla
  lógica** apaisada en memoria normal y se apunta el rectángulo que cambia;
  `girar_sucio` lo lleva a la pantalla de verdad, girado, por teselas de
  256x16 puntos compuestas en memoria y escritas por filas enteras (tiras
  seguidas: como rinde la escritura combinada). En VBE solo va el trozo de
  cada línea que ha cambiado. **Si el giro no es el bueno**: la tecla **G** al
  arrancar (junto a la S del modo seguro) o **Ctrl+Alt+G** con DOS en marcha
  (en modo texto) dan un cuarto de vuelta más, y queda **guardado en
  `\HBGIRO.CFG`** de la memoria USB (un número: 0 sin giro, 1 a la derecha,
  2 del revés, 3 a la izquierda) para los arranques siguientes. Vale también
  en una pantalla apaisada (una que esté del revés). Se acepta una pantalla de
  640x400 «de frente o de lado».
- **Los discos** (desde la 0.5, igual en `efi64.asm`). Arrancando «deprisa», el firmware solo pone en marcha el
  dispositivo del que arranca, y la eMMC no aparecería: HispaBIOS le pide que
  **conecte sus controladoras de disco** (`ConnectController` recursivo sobre
  los dispositivos PCI de clase 01h y 0805h; `discos_conectar`). El disco del
  que se arranca es el 80h (la memoria USB, o la eMMC si Hispa-DOS está en
  ella) y los demás, 81h en adelante. Una eMMC se presenta además con sus dos
  **particiones de arranque** (4 MB) como discos aparte: un disco fijo de menos
  de 32 MB que no sea el de arranque no se da a DOS. Hacen falta sectores de
  512. Cada disco va al registro con su tamaño y lo que se ha hecho con él.
- **La memoria extendida** se pide según el mapa: el tramo libre entero que
  empieza en 1 MB (64 MB como mucho; en OVMF, 7 MB en vez de 4). Desde la
  0.5, también en `efi64.asm`.
- El **modo seguro** deja además de conectar controladoras; el giro se
  mantiene (es solo dibujo).

Lo que QEMU no puede decir del Acer One 10 (lo dirá `HISPABIOS.LOG`): si el
firmware usa paginación, lo que cuesta pintar con y sin escritura combinada,
si el modo es de 800x1280 o de 1280x800 y hacia dónde hay que girar, cómo se
presenta la eMMC, y por dónde llega la IRQ 0 (en Bay Trail el 8254 anda; el
camino 8259-LINT0 es lo dudoso: `reloj_elegir` prueba los seis).

## El fallo de la 0.9.26 en un PC actual

Arrancando por UEFI en un PC actual, HispaBIOS 0.1 se quedaba en:

    HispaBIOS 0.1 (x64) - paso 2: la memoria baja
    Mapa de memoria por debajo de 1 MB (inicio, paginas, tipo):

**Causa:** la convención de llamadas de UEFI en x64 exige la **pila alineada a
16 octetos** en cada llamada al firmware. `hex64` (la rutina que escribe los
números) llamaba a `escribir` con la pila descuadrada en 8, y la primera cifra
del mapa —justo después de ese título— era la primera llamada al firmware con
la pila mal (también `memoria_ext`). El firmware de los equipos actuales está
compilado con SSE y guarda datos alineados en la pila (`movaps`): con la pila
mal, excepción y equipo parado. OVMF (el firmware de QEMU) se compila sin SSE
y el del MacBook de 2012 tampoco tropezaba: por eso no se había visto.

**Arreglo:** todas las llamadas al firmware van por la macro `FW`; `escribir`
alinea la pila ella misma, venga de donde venga, y `memoria_ext` la lleva
cuadrada. Con `-DPRUEBA_PILA` la macro comprueba la alineación en cada llamada
y avisa por el puerto serie (`PILA-MAL`): la prueba arranca hasta `C:\>` y
exige que no salga ni una vez. (Con la 0.1 instrumentada igual salen 13.)

De paso, lo que también podía parar un equipo actual y ahora no: el búfer fijo
de 16 KB para el mapa de memoria, la BIOS puesta en 90000h sin mirar si el
firmware había dado esas páginas (se escribía encima de lo suyo), una línea
por cada página no dada (160 líneas que se llevaban la pantalla; ahora, por
tramos), el vigilante de los 5 minutos y la espera muda de 60 segundos.

## La lentitud de la 0.2 («1 FPS») y Hispa Ventanas

En el ThinkPad E14 y en el MacBook Air de 2017 la 0.2 arrancaba, pero la
pantalla iba a tirones, y `VENTANAS` contestaba «no se ha podido poner el modo
gráfico». Causas y arreglos:

1. **El teclado** (medido en QEMU). DOS mira el teclado en cada letra que
   escribe; cada mirada era un viaje al firmware (dos cambios de modo, 1 ms de
   espera y lo que tarde su teclado). `DIR C:\HISPADOS` (75 líneas) en QEMU:
   **11 s con la 0.2, 1 a 2 s con la 0.3**; con la 0.3 sin la medida del
   teclado, otra vez 10 a 12 s. En el Mac, además, el firmware usa el 8259
   (vector 68h) y los tics que caían durante esos viajes se los quedaba él:
   la pantalla, que se dibuja en el tic, se refrescaba muy de tarde en tarde.
2. **La pantalla sin escritura combinada** (no se puede medir en QEMU, donde la
   pantalla es memoria normal: lo dirá el registro del equipo). Con el marco
   en UC, pintar 1920x1200 entera —cada vez que el texto sube una línea— es
   del orden de un segundo. Arreglo: el alias WC, la medida y el modo menor.
3. **Sin VBE**: Hispa Ventanas caía al modo 12h de la VGA, que tampoco hay.
   Arreglo: VBE de 32 bits con el marco por debajo de 4 GB.

Queda por saber en los equipos de verdad (lo dirán `HISPABIOS.LOG` y
`HBLOG.TXT`): lo que cuesta pintar con y sin WC y si llega la IRQ 0. El ratón,
desde la 0.5 (ver «El ratón con UEFI»).

## Pruebas

`tests/run_hispabios_tests.sh` (en `make test`; necesita los paquetes `ovmf` y
`ovmf-ia32`). Los casos de la parte de 32 bits están en
`tests/hispabios32_casos.sh` (a solas: `HB_SOLO=32 tests/run_hispabios_tests.sh`)
y los del ratón y los discos de `efi64.asm`, en `tests/hispabios_raton_casos.sh`
(`HB_SOLO=raton`). Además del arranque de siempre (64 y 32 bits, «Juegos», modo
«irreal»), ensambla variantes de `BOOTX64.EFI` con las opciones de prueba:

| Opción de `efi64.asm` | Para qué |
|---|---|
| `-DPRUEBA_PILA` | Avisa por COM1 de una llamada al firmware con la pila sin alinear |
| `-DPRUEBA_OCUPA=dir,n,...` | Ocupa antes esas páginas, como un equipo cuyo firmware las usa |
| `-DPRUEBA_SIN_GOP` | Como si no hubiera pantalla gráfica |
| `-DREGISTRO_PASOS` | Guarda `HISPABIOS.LOG` en cada paso (la imagen de prueba) |
| `-DPRUEBA_LENTA=us` | Como si pintar un millón de puntos costara eso (pantalla lenta) |
| `-DPRUEBA_SIN_WC` | Sin el alias de escritura combinada |
| `-DPRUEBA_SIN_PRESTAR` | Sin prestar a DOS las páginas bajas del firmware |
| `-DPRUEBA_SEGURO` | Como si se hubiera pulsado S (modo seguro) |
| `-DPANTALLA_NATIVA` | No cambiar de modo de pantalla aunque sea lenta |
| `-DPRUEBA_RELOJ=máscara` | Fuentes del reloj que la BIOS no prueba (bit = número de la tabla de arriba) |
| `-DPRUEBA_P2SB` | Un P2SB de mentira con el 8254 bloqueado (QEMU no tiene PCH con banda lateral) |
| `-DPRUEBA_FW_SIN_RELOJ` | El firmware no recibe ninguna interrupción en sus viajes (el caso del Medion: su reloj no anda con DOS en marcha) |
| `-DPRUEBA_FWT=n` | Los tics al firmware: 0, no se dan (el fallo); 2, por la rutina de su interrupción (en OVMF, la del temporizador del APIC local) |

`efi32.asm` tiene las mismas (menos `PRUEBA_PILA`: la pila la alinea
`fw_llamar`) y dos más: `-DPRUEBA_GIRO=n` (el giro por omisión aunque la
pantalla sea apaisada) y `-DPRUEBA_SIN_PAG` (las tablas propias se ponen y se
quitan desde un estado sin paginación ni PAE, como en un firmware que no la
usa).

Casos: equipo «actual» (q35, 8 GB) con la pila comprobada; memoria baja
ocupada (BIOS en 70000h y en 60000h, en 64 y 32 bits) hasta `C:\>` y con el
registro completo; páginas sueltas prestadas (58000h, 80000h y 87000h: 576 KB);
sin memoria baja libre y sin GOP (aviso, qué hacer y registro guardado);
pantalla de 1920x1200 (el alias WC, la velocidad de `DIR`, `HBLOG`, EQUIPO con
ACPI y SMBIOS, Hispa Ventanas en VBE con captura); pantalla lenta simulada
(modo menor del firmware) con `VBEPRU.COM` (`tests/progs/vbepru.asm`: las
llamadas de VBE de Hispa Ventanas, el marco lineal y el registro).

El reloj: equipo sin 8254 (`-machine q35,pit=off`: el HPET; con `hpet=off`,
el reloj CMOS; en 32 bits, el reloj CMOS), el P2SB de mentira (se muestra, se
quita el bit y se vuelve a ocultar), y ninguna fuente (`PRUEBA_RELOJ=15h`: se
prueban los tres caminos por el IOAPIC, que en QEMU no lleva el 8259, sale el
aviso y se guarda el registro). EQUIPO con la BIOS en 70000h y en 60000h
(ACPI y SMBIOS). Lo que QEMU no puede probar: quitar el bloqueo en un PCH de
verdad y el camino por el IOAPIC llegando de verdad.

Los casos de 32 bits: un «Acer One 10» (QEMU de 64 bits con OVMF de 32, 1 GB,
1280x800: el registro entero, las tablas propias de WC, la velocidad de `DIR`,
`HBLOG`, EQUIPO con ACPI y SMBIOS, Hispa Ventanas en VBE); la **pantalla
vertical** de 800x1280 (la captura de Hispa Ventanas, girada de vuelta, tiene
que ser la de la pantalla apaisada); el giro guardado en `HBGIRO.CFG` y
Ctrl+Alt+G (con `PRUEBA_SIN_PAG`); la memoria USB y un **segundo disco** (D:,
leer y escribir; el de 4 MB no se da); sin 8254 con el P2SB de mentira y
arrancando de un disco fijo (el HPET); el modo seguro en un procesador de 32
bits sin 8254 (el reloj CMOS); la memoria baja ocupada (70000h, 448 KB con la
página prestada), sin memoria baja (aviso y registro) y la pantalla lenta
simulada con `VBEPRU`.

El ratón (0.5): un `usb-mouse` en el xHCI de la memoria USB, movido y pulsado
desde el monitor de QEMU (`mouse_move`, `mouse_button`; en `arranca`, las
«teclas» `m:orden+con+mas`): el registro (la controladora USB conectada, el
Simple Pointer de la consola apartado, el ratón USB en marcha por
`EFI_USB_IO`), HSPRATON por la «vía BIOS», `MOUSETST` (INT 33h) viendo el
movimiento y los dos botones, y `HBLOG` con las cuentas; en 64 y en 32 bits.
**Hispa Ventanas**: el puntero se mueve (dos capturas) y un doble clic en el
grupo «Juegos» abre su ventana (la captura cambia en más de 20.000 puntos).
Sin ratón USB, C2xxh dice que no y HSPRATON no usa la BIOS; el modo seguro no
conecta controladoras. Los discos de `efi64.asm`: la memoria USB (80h), un
disco SATA de 64 MB (81h, D:, leer y escribir) y uno de 4 MB que no se da;
el equipo de SMBIOS y la memoria extendida según el mapa.

El teclado (0.6), en `tests/hispabios_teclado_casos.sh` (`HB_SOLO=teclado`):
QEMU **sin 8042** (`-machine q35,i8042=off`) con un `usb-kbd` en el xHCI y el
firmware sin interrupciones (`PRUEBA_FW_SIN_RELOJ`). Sin dar tics
(`PRUEBA_FWT=0`) llega a `C:\>` y el teclado no responde (el fallo); con
los tics por `GenerateSoftInterrupt` y por la rutina de la interrupción, en 32
bits (procesador de 64 y de 32) y en 64, DIR responde, `TECLAPRU` (la espera
con HLT) recibe las teclas, y el registro y `HBLOG` dicen el teclado USB, el
reloj del firmware y los tics dados; el modo seguro con teclado solo USB.
Lo que QEMU no puede decir: si el firmware Phoenix del Medion da Timer Arch y
admite `GenerateSoftInterrupt` (si no, queda su rutina del vector 68h).

OJO con QEMU y KVM: con el 8259 del núcleo (KVM) la IRQ 0 por LINT0 no llega
tras enmascarar y abrir LINT0, cosa que un equipo de verdad sí hace; las
pruebas van sin KVM.

## Pendiente

- Probar la 0.6 en el **MEDION S2217** (`prueba-medion` en el FTP) y ver en
  `HISPABIOS.LOG` el apartado «Teclado:» y en `HBLOG.TXT` las teclas y los
  tics. Si el teclado de algún equipo no lo lleva el firmware: leerlo por
  `EFI_USB_IO` (informes de arranque), como el ratón.
- Probar la parte de 32 bits en el **Acer One 10** con la imagen de prueba
  (`LEEME-ACER.TXT` en la carpeta `prueba-hispabios` del FTP).
- Llevar a `efi64.asm` la pantalla girada de la de 32 (hay tabletas con
  firmware de 64 bits y panel vertical). Lo demás (discos, equipo, memoria
  extendida) ya está desde la 0.5.
- Probar el **ratón** de la 0.5 en el ThinkPad E14 y el MacBook Air de 2017
  (`prueba-hispabios-raton` en el FTP) y, según diga `HISPABIOS.LOG`: si el
  panel del Mac es una interfaz USB «de informe» sin protocolo de arranque,
  leer su descriptor de informe; la rueda (paquetes de 4 octetos,
  IntelliMouse) y las tabletas USB absolutas.
- Un modo de VBE con la medida de la pantalla (1280x800): hoy Hispa Ventanas
  sale en 800x600 o 1024x768, centrada.
- Probar la 0.4 en el Monovate 14.1 (y que el ThinkPad E14, el MacBook Air y
  el Asus GL553V siguen bien) con la imagen de prueba, y ver `HISPABIOS.LOG` y
  `HBLOG.TXT`.
- **Alt y Ctrl** con las teclas de función (Alt+F4 en Hispa Ventanas): el
  teclado del firmware da el estado de las teclas, la BIOS aún no lo traduce.
- Modos VBE de 8 bits (con la paleta por INT 10h 4F09h) para juegos.
- Si en algún equipo no llega ninguna fuente por el 8259: el temporizador del
  APIC local (ver «Límites» del reloj).
