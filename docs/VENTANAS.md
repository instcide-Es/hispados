# Hispa Ventanas — plan

Segunda época de la serie Hispa (ver [VISION.md](VISION.md)): un **entorno
gráfico propio** que funciona sobre Hispa-DOS, como si lo hubiéramos hecho a
principios de los 90, y que **ejecuta las aplicaciones de Windows 3.x**
(Word 6, Excel 5 y similares que aporte cada usuario).

Reglas de siempre: sala limpia (nada de código, gráficos, fuentes ni textos de
Microsoft), GPL, español primero, pruebas automáticas y equipos reales
(386/486 con 4–16 MB, y también PCs modernos con BIOS o CSM).

## Decisiones de base

| Tema | Decisión | Por qué |
|---|---|---|
| Lenguaje | Ensamblador (NASM), como Hispa-DOS | Ocupa mucho menos y va más rápido en los 386/486 a los que está destinado (decisión de Carlos, 12/11/2026) |
| Modo del procesador | Modo protegido sobre el servidor DPMI propio (HSPDPMI) y HSPXMS: el núcleo de Hispa Ventanas es un cliente de 32 bits (memoria plana, dibujo rápido); las aplicaciones de Windows 3.x seguirán en sus segmentos de 16 bits | Dibujo y memoria sencillos y rápidos en 386/486, y compatibilidad con las aplicaciones de la época |
| Aspecto | Diseño propio: colores, iconos, fuentes de mapa de bits y nombres en español hechos aquí | Que se reconozca como nuestro, no como una copia |
| Compatibilidad | API de Windows 3.1 documentada (KERNEL, USER, GDI, KEYBOARD, MOUSE, DISPLAY, COMMDLG, SHELL) implementada desde cero | Así las aplicaciones de la época funcionan sin cambios |
| Programas propios | Hechos como aplicaciones de Windows 16 bits normales, en NASM, con una herramienta propia que escribe la cabecera NE | Cada programa propio prueba también la compatibilidad |

## Etapas

Cada etapa termina con algo que se ve y se puede probar en QEMU y en un PC real.

1. **Servidor DPMI propio (HSPDPMI).** Modo protegido de 16/32 bits sobre
   HSPXMS: descriptores, memoria, interrupciones reflejadas a DOS.
   *Se ve:* programas DPMI de terceros (por ejemplo de FreeDOS o DJGPP)
   funcionando sin CWSDPMI ni el DOSX de Windows.
   *Estado:* hecho (`drivers/hspdpmi.asm`, `HSPDPMI.COM` en `\HISPADOS`; se
   carga a mano o desde AUTOEXEC.BAT, después de HSPXMS.SYS). El cliente corre
   en anillo 3 sin paginación y las interrupciones y llamadas a DOS/BIOS se
   reflejan volviendo a modo real de verdad. Servicios de INT 31h: 0000h-000Dh,
   0100h-0102h, 0200h-0205h, 0300h-0306h, 0400h, 0500h-0503h, 0600h-0604h,
   0702h/0703h, 0800h/0801h y 0900h-0902h. Pruebas: `tests/run_dpmi_tests.sh`
   (clientes propios de 16 y 32 bits y, si se descargan con
   `tests/thirdparty/fetch_dpmi.sh`, NASM, SED y LUA de DJGPP). Pendiente: la
   traducción de punteros de la API de DOS (INT 21h con DS:DX desde modo
   protegido; hoy los clientes usan 0300h con memoria de DOS, como DJGPP),
   VCPI (para convivir con un EMM386) y memoria virtual.
2. **Gráficos y ratón.** Driver de pantalla VGA 640×480 16 colores y VESA
   (800×600 y 1024×768, 256 colores y color verdadero); primitivas de dibujo,
   fuentes propias de mapa de bits, cursor del ratón (HSPRATON).
   *Se ve:* un escritorio con fondo, cursor y ventanas que se pueden arrastrar.
   *Estado:* hecho (`ventanas/`, `VENTANAS.COM` en `\HISPADOS`; se arranca
   con `VENTANAS`, ver «Arquitectura»). VESA con marco lineal en 640×480,
   800×600 (lo normal) y 1024×768, de 256 colores o de 15, 16, 24 y 32 bits,
   y VGA de 640×480 y 16 colores cuando no hay VESA (o con `/VGA`). Fuente
   propia de 8×16 (`ventanas/fuente.txt`), cursor por software, tres ventanas
   de ejemplo que se activan con un clic y se arrastran por la barra de
   título, Tab, flechas, botón «Salir», Esc y Alt+F4. Pruebas:
   `tests/run_ventanas_tests.sh`. Pendiente: VESA sin marco lineal (por
   bancos), el ratón por interrupciones en vez de por sondeo y arrastrar solo
   el contorno en los equipos lentos.
3. **Ventanas y mensajes (USER).** Clases de ventana, cola de mensajes,
   multitarea cooperativa, menús, botones, cuadros de texto, listas, cuadros de
   diálogo, portapapeles, temporizadores.
   *Se ve:* nuestros primeros programas: Centro de programas (lanzador),
   Bloc de notas, Calculadora, Reloj y un juego propio.
4. **Cargador de ejecutables de Windows (NE) y KERNEL.** Módulos, DLL,
   recursos (iconos, menús, diálogos, cadenas), memoria global y local,
   segmentos móviles y descartables, archivos .INI (perfiles).
   *Se ve:* programas de Windows 3.1 sencillos (los nuestros de prueba)
   funcionando en Hispa Ventanas.
5. **Compatibilidad ampliada.** GDI completo (mapas de bits, paletas,
   regiones, metarchivos, impresoras), cuadros de diálogo comunes (Abrir,
   Guardar, Imprimir, Fuente, Color), SHELL, DDE, OLE 1 básico, ayuda.
   *Se ve:* Word 6 y Excel 5 (copias del usuario) abriendo, editando y
   guardando documentos.
6. **Sesiones DOS en ventana y modo 386.** Máquinas virtuales 8086 (V86) para
   ejecutar programas de DOS en ventana o a pantalla completa, junto con el
   EMM386 propio de Hispa-DOS (UMB, EMS) y la Sound Blaster emulada en tarjetas
   modernas.
   *Se ve:* varias sesiones de DOS a la vez, cada una en su ventana.

Al final de la etapa 5 sale la primera versión estable de Hispa Ventanas.

## Arquitectura

Hispa Ventanas es hoy un solo programa, `VENTANAS.COM` (unos 13 KB), hecho
entero en NASM. Arranca en modo real, comprueba lo que necesita, elige el modo
de vídeo y entra en modo protegido como **cliente DPMI de 32 bits**.

### Cómo se arranca

```
DEVICE=A:\HISPADOS\HSPXMS.SYS        (en CONFIG.SYS)
VENTANAS [/1024 | /800 | /640] [/8 | /15 | /16 | /24 | /32 | /COLOR] [/VGA]
```

- Hace falta HSPXMS.SYS (si no está, lo dice y termina).
- Si no hay servidor DPMI, carga HSPDPMI.COM de su misma carpeta (queda
  residente); si no lo encuentra, pide que se ejecute antes.
- Si no hay driver de ratón (INT 33h), carga HSPRATON.COM de su misma carpeta;
  sin ratón funciona igual con el teclado.
- Sin opciones: VESA 800×600 de 256 colores; si la tarjeta no tiene VESA 2.0
  con marco lineal, VGA de 640×480 y 16 colores.
- Dentro: clic en una ventana para pasarla delante, arrastrar por la barra de
  título, Tab (siguiente ventana), flechas (mover la activa), Esc, Alt+F4 o
  el botón «Salir» para volver a Hispa-DOS.
- Al salir deja el modo de texto, el ratón y la memoria como estaban. No
  cambia ningún vector de interrupción. Si el programa falla estando en
  gráficos, vuelve antes a modo texto para que se lea el mensaje de HSPDPMI.

### Memoria y modo del procesador

El núcleo gráfico es de 32 bits (386 o superior, que es lo que exige HSPDPMI):
un segmento de código de 32 bits y un único segmento de datos y pila de 4 GB,
los dos con la base del programa. Así un puntero «cercano» llega a toda la
memoria: *puntero = dirección lineal − base*. La superficie de dibujo (memoria
extendida, función 0501h) y el marco lineal de la tarjeta (0800h) se usan con
punteros normales, sin cambiar de segmento.

Las aplicaciones de Windows 3.x de las etapas 4 y 5 seguirán siendo de 16
bits: tendrán sus propios segmentos de 16 bits en la misma LDT y entrarán en
este núcleo por puertas de llamada. Lo que es de 16 bits es la API, no el
motor.

Las llamadas a DOS y a la BIOS se hacen por el servidor DPMI: `INT 10h`,
`INT 16h` e `INT 33h` con registros se reflejan solas; lo que necesita un
búfer (VBE 4F00h/4F01h) se hace antes de entrar, en modo real.

### Capas

Cada capa está en su archivo de `ventanas/` y solo usa la de debajo por las
funciones que se listan (la lista completa, con sus registros, está en la
cabecera de cada archivo). Todas conservan los registros.

```
  escritorio.inc   fondo, barra superior y ventanas de ejemplo     (etapa 2)
        |          -> en la etapa 3: las aplicaciones
  gestor.inc       ventanas, orden Z, zona sucia, arrastre, teclas  (USER)
        |
  gdi.inc          dibujo con recorte sobre la superficie           (GDI)
        |
  video.inc        driver: modo, paleta, superficie -> pantalla     (DISPLAY)
  video16.inc      (parte de modo real: elegir el modo)
  raton.inc        ratón por INT 33h y cursor por software          (MOUSE)
```

**Driver de pantalla** (`video16.inc`, `video.inc`)

| Función | Qué hace |
|---|---|
| `vid_detectar` | (modo real) elige el modo: VESA con marco lineal a la resolución pedida, 800×600 o VGA de 16 colores |
| `vid_abrir` | pone el modo y la paleta; si VESA falla, pasa a VGA |
| `vid_presentar` | lleva un rectángulo de la superficie a la pantalla (EAX=x0 EBX=y0 ECX=x1 EDX=y1) |
| `vid_cerrar` | vuelve al modo de texto anterior |
| `vid_ancho`, `vid_alto`, `vid_bits` | el modo puesto |

Las capas de arriba no saben qué formato tiene la pantalla. Dibujan en la
**superficie**: un byte por punto, con índices de la paleta de Hispa Ventanas
(16 colores propios, un cubo de 6×6×6 y 24 grises). El driver tiene un
«presentador» por formato: 8 bits (copia), 15/16, 24 y 32 bits (por una tabla
con cada color ya en el formato de la pantalla, hecha con las máscaras que da
VBE) y VGA de cuatro planos. Añadir un formato (VESA por bancos, otra
tarjeta) es añadir un presentador.

**Dibujo** (`gdi.inc`)

Estado: `gdi_color`, `gdi_negrita` y el rectángulo de recorte (`rec_*`); en
la etapa 3 pasará a ser el contexto de dispositivo de cada ventana.

| Función | Registros |
|---|---|
| `gdi_fijar_recorte`, `gdi_cortar`, `gdi_recorte_total` | recorte: fijarlo, estrecharlo, quitarlo |
| `gdi_punto` | EAX=x EBX=y |
| `gdi_linea` | EAX=x0 EBX=y0 ECX=x1 EDX=y1 |
| `gdi_rect`, `gdi_marco` | EAX=x EBX=y ECX=ancho EDX=alto (lleno / borde) |
| `gdi_mosaico` | lo mismo y ESI=mosaico de 16×16 |
| `gdi_copiar`, `gdi_copiar_clave` | bloque de memoria a la superficie (ESI=mapa; con «clave», un color transparente) |
| `gdi_leer` | bloque de la superficie a memoria (EDI=mapa) |
| `gdi_texto`, `gdi_ancho_texto` | EAX=x EBX=y ESI=cadena (página 437) |

La **fuente** «Hispa 8×16» se diseña en texto (`ventanas/fuente.txt`: una
rejilla de `#` y `.` por carácter, y las letras acentuadas compuestas de letra
más acento) y `tools/mkfuente.py` la convierte en la tabla que se ensambla.
Tiene ASCII completo y las letras acentuadas y los signos de la página 437
que usan el español, el catalán, el gallego y el francés. La negrita la hace
`gdi_texto`. `make` deja una vista en `build/gen/ventanas/fuente.ppm`.

**Ratón y cursor** (`raton.inc`)

En cada vuelta del bucle, una rutina de modo real (`rm_sondeo`, llamada con
la función 0301h) espera a la siguiente interrupción con `HLT` si no hay nada
pendiente, y recoge de una vez el movimiento (INT 33h, función 0Bh), los
botones (03h) y una tecla (INT 16h). La posición la lleva Hispa Ventanas en
puntos de pantalla, así que no depende de que el driver de ratón conozca el
modo gráfico. El cursor se dibuja en la superficie guardando lo que tapa
(`cur_mostrar`) y se quita reponiéndolo (`cur_ocultar`).

**Gestor de ventanas** (`gestor.inc`)

| Función | Qué hace |
|---|---|
| `ges_crear` | EAX=x EBX=y ECX=ancho EDX=alto ESI=título EDI=pintor: ventana nueva, delante |
| `ges_invalidar` | marca un rectángulo para repintar |
| `ges_actualizar` | pinta lo marcado y mueve el cursor (una vez por vuelta) |
| `ges_raton_mueve`, `ges_raton_pulsa`, `ges_raton_suelta`, `ges_tecla` | lo que llega del ratón y del teclado |

Cada ventana es una ficha (posición, tamaño, título y **pintor**) y hay una
lista con el orden, de la del fondo a la de delante (la activa). El pintor es
la semilla de `WM_PAINT`: recibe el rectángulo interior de su ventana con el
recorte ya puesto.

Para que no parpadee, **nada se dibuja directamente en la pantalla**: quien
cambia algo marca la zona; `ges_actualizar` quita el cursor de la superficie,
pinta solo esa zona de atrás adelante (fondo, ventanas en su orden), vuelve a
poner el cursor y manda al driver la zona ya terminada.

### Aspecto

Propio, sin parecidos: fondo de mosaico (un azulejo de rombos) en azul
petróleo; barra superior marrón con un filete ocre; ventanas de color crema
con marco marrón y sombra dura; barra de título terracota con un rombo ocre y
el texto en negrita si es la activa, y arena si no lo es; cursor ocre con
borde de tinta. La paleta y los dibujos están en `ventanas/datos.inc`.

## Pruebas

- Capturas de pantalla automáticas en QEMU (tests/tools/screen.sh) comparadas
  con lo esperado, y órdenes de prueba por el puerto serie.
- Programas de prueba de Windows 16 bits propios (NASM + nuestra
  herramienta de cabeceras NE) para cada función de la API, con su
  resultado escrito en un archivo.
- Las aplicaciones de terceros (Windows 3.11, Word, Excel) solo en local, con
  las copias de cada uno; nunca en el repositorio.
