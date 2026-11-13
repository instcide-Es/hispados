# USB en Hispa-DOS: HSPUSB.SYS

`HSPUSB.SYS` lleva el USB sin la BIOS. Desde la versión 0.2 maneja las
**cuatro clases de controladora USB** que hay en un PC, y varias a la vez:

| Clase | USB | Dónde está | Cómo se le habla |
|---|---|---|---|
| **UHCI** | 1.0 / 1.1 | Intel (PIIX3/PIIX4, ICH) y VIA: Pentium, Pentium II/III/4, y las «compañeras» de los EHCI de Intel hasta 2010 | puertos de E/S |
| **OHCI** | 1.0 / 1.1 | SiS, ALi, NEC, AMD, NVIDIA, Apple, tarjetas PCI | registros en memoria |
| **EHCI** | 2.0 | casi todos los PC de 2002 a 2014 | registros en memoria |
| **xHCI** | 3.x | los PC actuales | registros en memoria |

Encima de cualquiera de ellas funciona lo mismo:

- **memorias USB** (almacenamiento masivo, Bulk-Only + SCSI) como una unidad
  de DOS (FAT12/FAT16), con cambio de memoria en caliente;
- **teclado y ratón** USB (HID, protocolo de arranque), entregados como si
  fueran PS/2 (o a HSPRATON, o al búfer de la BIOS si no hay 8042);
- **mandos de juego** (HID genéricos y tipo Xbox 360) para HSPJOY
  ([MANDOS.md](MANDOS.md));
- **concentradores** (hubs), también encadenados;
- el disco de arranque de la BIOS con `/U`.

## Uso

```
DEVICE=C:\HISPADOS\HSPUSB.SYS [/F] [/U] [/INFO] [/V] [/?]
```

| Opción | Qué hace |
|---|---|
| (ninguna) | Seguro: si la BIOS está usando **alguna** controladora USB (es suya o la tiene en marcha), o el sistema ha arrancado de un disco USB de la BIOS, no se toca nada. |
| `/F` | Tomar el USB aunque lo use la BIOS (teclado y ratón pasan a HSPUSB). |
| `/U` | Como `/F`, también cuando se ha arrancado de una memoria USB de la BIOS: HSPUSB la reconoce y atiende él la unidad 80h (INT 13h). |
| `/INFO` | Enseña las controladoras (clase, fabricante:modelo, uso que les da la BIOS), los puertos con algo conectado de cada una y el árbol de aparatos. Espera 15 s. |
| `/V` | Mensajes detallados. |
| `/?` | Ayuda (el driver no se carga). |

Se toman **todas** las controladoras o ninguna: no tendría sentido quitarle a
la BIOS el EHCI y dejarle sus compañeras.

## Diseño: una capa común y cuatro controladoras

```
 almacenamiento   teclado/ratón   mandos   concentradores   enumeración
 ───────────────────────────────────────────────────────────────────────
   capa común de «controladora»            lib/usb_hc.inc
     control · bulk · pipe_queue / usb_poll · rp_get / rp_ack /
     port_enable · hc_time · dev_free
 ───────────────────────────────────────────────────────────────────────
   UHCI              OHCI              EHCI              xHCI
   lib/usb_uhci.inc  lib/usb_ohci.inc  lib/usb_ehci.inc  drivers/hspusb.asm
```

Lo de arriba no sabe qué hay debajo. Las operaciones (en `lib/usb_hc.inc`,
que las reparte con una tabla por clase):

| Operación | Qué hace |
|---|---|
| `control` | Transferencia de control al aparato actual (petición en `ctl_setup`, datos de entrada en `[ctl_buf]`). |
| `bulk` | Transferencia bulk de la memoria USB (entrada o salida, hasta 4 KB). |
| `pipe_queue` | Pedir el siguiente informe de una tubería de interrupción (0 teclado, 1 ratón, 2 y 3 mandos). |
| `usb_poll` | Recoger los informes que hayan llegado, de todas las controladoras (lo llama el reloj, INT 08h); cada uno va a `pipe_done` (teclado, ratón o mando). |
| `rp_get`, `rp_ack` | Estado de un puerto raíz (conectado, habilitado, ha cambiado) y darlo por visto. |
| `port_enable` | Reiniciar y habilitar un puerto raíz; da la velocidad. |
| `hc_time` | Reloj de la controladora en microtramas (para entregar a HSPJOY un informe reciente). |
| `dev_free` | Dar de baja un aparato. |

En el xHCI la propia controladora lleva las direcciones USB, los testigos de
alternancia (DATA0/DATA1) y el troceo en paquetes. En las otras tres
(«clásicas») los lleva la capa común: tabla de aparatos `dv_*` (controladora,
dirección, velocidad, paquete máximo y, para el EHCI, concentrador que le
traduce), datos de la transferencia en curso `xf_*`, y una reserva de
descriptores de transferencia que comparten todas, porque control y bulk son
**síncronos** (una transferencia cada vez en todo el driver). No se usan
interrupciones de hardware: todo se consulta.

### UHCI

Las 1024 entradas de la lista de tramas apuntan a la misma cadena de colas:
las tuberías de interrupción (una cola y un TD cada una; un informe por
trama) y, al final, la cola de control y bulk. Un TD por paquete, hasta 32
por tanda y en profundidad (varios por trama). Aparatos de velocidad plena y
baja (bit LS; el preámbulo lo pone la controladora). Puertos raíz: reinicio
de 50 ms, habilitación y velocidad. Se le quita a la BIOS con el registro
LEGSUP (C0h) y un reinicio global.

Una UHCI **sin nada conectado se queda «aparcada»**: parada y sin lista de
tramas (4 KB que no se gastan). Sus puertos se siguen viendo; si se conecta
una memoria se arranca con la lista de tramas de reserva, y si la memoria se
quita vuelve a aparcarse.

### OHCI

HCCA con las 32 entradas de interrupción apuntando a la cadena de tuberías
(un ED y dos TD que se turnan cada una), un ED en la lista de control y otro
en la de bulk, que se rellenan en cada transferencia. Un TD lleva una
transferencia entera (control son tres; bulk uno). Traspaso desde la BIOS
(SMM) con OwnershipChangeRequest, reinicio del bus y de la controladora, y
corriente a los puertos. Reinicio de puerto: tres de 10 ms seguidos.

### EHCI

Lista asíncrona con dos colas en anillo (la cabecera y la de trabajo, que se
rellena en cada transferencia de control o bulk); un qTD lleva una
transferencia entera. La lista periódica (4 KB) solo se crea cuando hay
alguna tubería de interrupción en el EHCI. Traspaso desde la BIOS con
USBLEGSUP (espacio de configuración PCI), CONFIGFLAG = 1.

**Controladoras compañeras.** Al reiniciar un puerto raíz, si el aparato no
es de alta velocidad no queda habilitado: se cede a la compañera (UHCI u
OHCI) con el bit Port Owner y aparece en el puerto de ella, donde se enumera
como cualquier otro. Por eso el orden es xHCI, EHCI y, tras una pausa, UHCI
y OHCI.

**Aparatos de USB 1.x detrás de un concentrador de USB 2.0** (traductor de
transacciones, «split»): está programado según la norma (la cola lleva la
dirección y el puerto del concentrador; en las tuberías de interrupción,
inicio en la microtrama 0 y fin en las 2 a 4), pero **no se ha podido
probar**: QEMU solo tiene un concentrador de USB 1.1.

### xHCI

Como hasta ahora (anillos, contextos, Configure Endpoint, concentradores con
su cadena de ruta y su traductor). Se lleva **un** xHCI: el primero.

## Memoria

Lo que se queda residente es **lo de `.text`** (las transferencias, los
puertos, los informes, INT 13h, el reloj y lo necesario para reconocer una
memoria USB conectada después: unos 16 KB) y, detrás, **la zona de trabajo**,
alineada a una página, con todo lo que ven las controladoras. La
**inicialización** (sección `.init`: buscar y tomar las controladoras,
ponerlas en marcha, el teclado, el ratón y los mandos, el análisis del
descriptor de informe HID, `/INFO` y los mensajes; unos 11,5 KB) va en el
fichero detrás de `.text`, pero se ejecuta en `CS:INIT_ORG` (C000h), por
encima de donde irá la zona, y **se descarta**: la zona se monta justo
encima de lo residente, donde estaba `.init` en el fichero.

| Qué | Cuánto |
|---|---|
| Búfer de datos del disco | 4 KB |
| Búferes comunes (descriptores, CBW/CSW, informes) | 640 bytes |
| xHCI (si lo hay): anillos y contexto de entrada | 6,7 KB, y 1 o 2 KB por aparato (según el tamaño de sus contextos) |
| Clásicas (si las hay): reserva de TD y colas de las tuberías | 1,2 KB |
| Cada UHCI | 16 bytes, y 4 KB (lista de tramas) si tiene algo conectado |
| Cada OHCI | 288 bytes |
| Cada EHCI | 192 bytes, y 4 KB si lleva teclado, ratón o mando |

Cuando hace falta memoria «de reserva» para lo que se conecte después (una
lista de tramas para una UHCI aparcada, o dos aparatos más en el xHCI), se
reserva al final de la inicialización, detrás de lo demás.

Mientras se inicializa hace falta memoria hasta `CS:init_end` (unos 59 KB
desde el principio del driver). Con `DEVICEHIGH` en un bloque de memoria
superior más pequeño, HSPUSB dice «no me quedo, me falta memoria» sin tocar
nada (estado de INIT con el bit 9 puesto) y el núcleo lo carga abajo.

Lo que ocupa en la memoria superior, con `DEVICEHIGH` y HSPEMM, en las pruebas
de QEMU (`PARTE=C`; memoria USB y teclado):

| Controladora | Antes (todo residente) | Ahora |
|---|---|---|
| UHCI | 38.672 | 26.384 |
| OHCI | 34.848 | 22.560 |
| EHCI | 40.960 | 28.672 |
| EHCI + 3 UHCI (ICH9) | 38.896 | 32.768 |
| xHCI | 43.008 | 31.744 |

(Antes = HSPUSB 0.2 con todo residente; HSPUSB 0.1, que solo llevaba el xHCI, ocupaba 38.912 en la misma prueba.)

Dos reglas para quien toque el código:

- Lo de `.text` no puede usar nada de `.init` después del arranque. Lo
  comprueba `tools/initcheck.py` al compilar (falla si `.text` nombra una
  etiqueta de `.init` que no esté en su lista: las que solo se usan durante
  el arranque o tras comprobar `/V` o `/INFO`, que `rescan` apaga).
- `.init` tiene que acabar por debajo de F000h (lo comprueba el ensamblador).
  Las llamadas entre `.init` y `.text` son relativas de 16 bits y dan la
  vuelta a los 64 KB; con ese margen, origen y destino «sin la vuelta» nunca
  caen en la misma página de 4 KB, que es cuando QEMU 10 se salta la vuelta
  (lo hace al encadenar bloques traducidos de la misma página) y sigue
  ejecutando en `CS:1xxxxh`. En un procesador de verdad daría igual.

## Modo V86 (HSPEMM, EMM386)

[USB-V86] El acceso a la memoria física está en muy pocas rutinas:

| Rutina | Qué hace |
|---|---|
| `mrd`, `mwr` (hspusb.asm) | Los únicos accesos a registros en memoria (xHCI, OHCI y EHCI; `hc_rd` y `hc_wr` solo suman la base). En modo real, modo «irreal»; en V86 con VCPI, INT 15h AH=87h. |
| `phys`, `lphys` (hspusb.asm) | La única traducción a dirección física (de CS:desplazamiento y de dirección lineal); en V86, por VCPI (INT 67h AX=DE06h). Nada de lo que se entrega a una controladora cruza de página. |
| `fl_seg` (usb_hc.inc) | Dirección lineal de una lista de tramas a ES:DI, para rellenarla. |
| UHCI | No usa registros en memoria: solo IN/OUT. |

## Límites

- Un solo xHCI (el primero), 8 controladoras en total, 8 aparatos a la vez
  (concentradores incluidos), 4 concentradores, una memoria USB, un teclado,
  un ratón y dos mandos.
- Teclado, ratón y mandos tienen que estar conectados al cargar el driver;
  la memoria USB se puede cambiar después (si no había ninguna al cargar, no
  hay unidad de DOS que ofrecer).
- En un EHCI sin controladoras compañeras ni traductor en el concentrador
  raíz, un aparato de USB 1.x conectado directamente no funciona (no hay
  quien lo lleve). Es raro: esos equipos traen un concentrador interno de
  USB 2.0 («rate matching hub»), y entonces va por el traductor.
- Transferencias isócronas (audio, cámaras): no.
- Memoria USB en una controladora clásica: si una orden falla por el
  transporte (un paquete perdido, un STALL, un error de fase), se hace la
  recuperación de Bulk-Only (Bulk-Only Mass Storage Reset, CLEAR_FEATURE de
  los dos endpoints, testigos a DATA0) y se repite una vez. En el xHCI no
  (el endpoint parado pediría Reset Endpoint y Set TR Dequeue Pointer).
- Aparatos de velocidad baja (bit LS de la UHCI, del ED de la OHCI y del QH
  del EHCI): hechos según la norma, **sin probar** (los teclados y ratones
  de QEMU son de velocidad plena).
- Las cuatro controladoras se sondean; no se usan sus interrupciones.

## Cómo probar

En QEMU (tiene las cuatro):

```
tests/run_usbhc_tests.sh build/hispados.img [uhci|ohci|ehci|ehcicomp|xhci|mezcla|aparcar]
```

Con cada controladora: A) una memoria en un puerto raíz (leer, escribir,
carpeta, copiar, borrar, listar, `fsck.fat`); B) teclado, ratón, memoria y
mando, con un concentrador por medio (ratón, memoria y mando detrás); C) lo
mismo que A con HSPEMM (modo V86) y HSPUSB cargado con `DEVICEHIGH`, más el
teclado USB. `PARTE=A`, `B` o `C` hacen solo una parte. `mezcla` pone las
cuatro controladoras a la vez (teclado en la UHCI, ratón en la OHCI, memoria
en el EHCI y mando en el xHCI). En `ehci` (sin compañeras) no hay
concentrador: el de QEMU es de USB 1.1 y no se puede conectar a un EHCI
solo.
`ehcicomp` es un ICH9 (EHCI y tres UHCI compañeras); `aparcar` quita y pone
la memoria entre dos UHCI.

A mano:

```
qemu-system-i386 -drive file=build/hispados.img,format=raw,if=floppy -boot a \
    -device piix3-usb-uhci,id=u -device usb-kbd,bus=u.0 -device usb-mouse,bus=u.0
    (o -device pci-ohci, -device usb-ehci, -device qemu-xhci)
```

con `DEVICE=A:\HISPADOS\HSPUSB.SYS /F /INFO` en `CONFIG.SYS`.

En un PC de verdad: `/INFO` y una foto de la pantalla dicen qué
controladoras hay, cuáles usa la BIOS y qué se ha reconocido en cada puerto.

**Sin probar en hardware real**: nada de UHCI, OHCI ni EHCI se ha probado
todavía fuera de QEMU (traspaso desde una BIOS de verdad con SMM, tiempos de
los puertos, concentradores de USB 2.0 con traductor, aparatos de velocidad
baja reales).
