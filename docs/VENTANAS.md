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
   `tests/thirdparty/fetch_dpmi.sh`, NASM, SED y LUA de DJGPP). `HSPDPMI /D`
   lo quita de la memoria cuando no tiene clientes (lo usa el lanzador de
   Hispa Ventanas antes de ejecutar un juego; ver «Programas de Hispa-DOS»).
   Pendiente: la
   traducción de punteros de la API de DOS (INT 21h con DS:DX desde modo
   protegido; hoy los clientes usan 0300h con memoria de DOS, como DJGPP y
   como Hispa Ventanas, ver «Archivos»), VCPI (para convivir con un EMM386) y
   memoria virtual.
2. **Gráficos y ratón.** Driver de pantalla VGA 640×480 16 colores y VESA
   (800×600 y 1024×768, 256 colores y color verdadero); primitivas de dibujo,
   fuentes propias de mapa de bits, cursor del ratón (HSPRATON).
   *Se ve:* un escritorio con fondo, cursor y ventanas que se pueden arrastrar.
   *Estado:* hecho (`ventanas/`, `VENTANAS.COM` en `\HISPADOS`). VESA con
   marco lineal en 640×480, 800×600 (lo normal) y 1024×768, de 256 colores o
   de 15, 16, 24 y 32 bits, y VGA de 640×480 y 16 colores cuando no hay VESA
   (o con `/VGA`). Fuente propia de 8×16 (`ventanas/fuente.txt`) y cursor por
   software. Las tres ventanas de muestra de esta etapa siguen en
   `VENTANAS /DEMO`. Pruebas: `tests/run_ventanas_tests.sh`. Pendiente: VESA
   sin marco lineal (por bancos).
3. **Ventanas y mensajes (USER).** Clases de ventana, cola de mensajes,
   multitarea cooperativa, menús, botones, cuadros de texto, listas, cuadros de
   diálogo, portapapeles, temporizadores.
   *Se ve:* nuestros primeros programas: Centro de programas (lanzador),
   Bloc de notas, Calculadora y Reloj.
   *Estado:* hecho (ver «Arquitectura» y «La interfaz de la etapa 3»).
   Ventanas con procedimiento y mensajes (`WM_CREATE`, `WM_PAINT`,
   `WM_MOUSEMOVE`, `WM_LBUTTONDOWN`…), una tarea por programa con su cola y su
   bucle `msg_tomar`/`msg_despachar`, captura del ratón y foco del teclado,
   ratón por interrupciones, zona no cliente (título con cerrar, maximizar o
   restaurar y minimizar a un icono; cambio de tamaño por los bordes),
   regiones de recorte por ventana, ventanas hijas y controles (botón,
   casilla, opción, texto, cuadro de texto de una y de varias líneas con
   selección y portapapeles, lista con barra de desplazamiento), menús
   desplegables, diálogos modales (mensaje, pedir un texto, elegir un
   archivo), temporizadores, teclado español de KEYB con acentos. Programas:
   Centro de programas (con grupos y accesos directos, también a programas
   de Hispa-DOS: juegos), Bloc de notas (abre y guarda archivos),
   Calculadora, Reloj y Sistema, y el volumen en la barra superior. Pruebas:
   `tests/run_ventanas3_tests.sh` y `tests/run_ventanas4_tests.sh`. Pendiente (se
   hará según haga falta en las etapas 4 y 5): un juego propio; avisar de los
   textos sin guardar al salir con el botón «Salir»; arrastrar solo el
   contorno en los equipos lentos y cambiar el cursor en los bordes; órdenes
   de menú desactivadas o con marca; barra de desplazamiento horizontal,
   tabuladores y ajuste de línea en el cuadro de texto; varias copias del
   mismo programa a la vez.
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

Hispa Ventanas es hoy un solo programa, `VENTANAS.COM` (unos 56 KB), hecho
entero en NASM. Arranca en modo real, comprueba lo que necesita, elige el modo
de vídeo y entra en modo protegido como **cliente DPMI de 32 bits**. Los
programas (Centro, Bloc de notas…) van dentro, cada uno en su archivo y como
una tarea; en la etapa 4 saldrán a ejecutables propios.

`VENTANAS.COM` se ejecuta dos veces: la primera copia es el **lanzador**
(2 KB que se quedan en memoria convencional) y ejecuta la segunda, que es el
entorno gráfico (ver «Programas de Hispa-DOS desde Hispa Ventanas»).

Un .COM no pasa de 64 KB y lo que usa la parte de modo real (el código de
arranque, el búfer de paso de DOS, las colas del ratón y del teclado) tiene
que estar en los primeros 64 KB del programa. Por eso los iconos se guardan
a 4 bits por punto (`tools/icopack.py` los empaqueta al compilar; en los
fuentes siguen en texto) y la pila y las variables que solo usa el modo
protegido van detrás, donde llegan los punteros de 32 bits. Si lo de modo
real se sale de los 64 KB, la compilación falla. Quedan unos 4 KB de margen:
los programas saldrán a ejecutables propios en la etapa 4.

### Cómo se arranca y se usa

```
DEVICE=A:\HISPADOS\HSPXMS.SYS        (en CONFIG.SYS)
KEYB SP                              (en AUTOEXEC.BAT, para el teclado español)
VENTANAS [/1024 | /800 | /640] [/8 | /15 | /16 | /24 | /32 | /COLOR] [/VGA] [/DEMO]
```

- Hace falta HSPXMS.SYS (si no está, lo dice y termina).
- Si no hay servidor DPMI, carga HSPDPMI.COM de su misma carpeta y al salir
  lo quita de la memoria (si ya estaba cargado, lo deja); si no lo
  encuentra, pide que se ejecute antes.
- Si no hay driver de ratón (INT 33h), carga HSPRATON.COM de su misma carpeta
  (queda residente); sin ratón funciona igual con el teclado.
- Sin opciones: VESA 800×600 de 256 colores; si la tarjeta no tiene VESA 2.0
  con marco lineal, VGA de 640×480 y 16 colores.
- Al entrar se abre el **Centro de programas** con sus grupos (Principal,
  Accesorios, Juegos): doble clic en un icono (o flechas e Intro) abre el
  grupo en su ventana y, dentro, el programa. `/DEMO` pone solo las ventanas
  de muestra de la etapa 2.
- El altavoz de la barra de arriba abre el volumen.
- Ventanas: se arrastran por la barra de título; los botones de la derecha
  minimizan (a un icono abajo; un clic lo recupera), maximizan o restauran
  (también doble clic en el título) y cierran; se cambia el tamaño tirando de
  un borde o de una esquina.
- Teclado: Alt+F4 cierra la ventana activa, Ctrl+Tab (o Alt+Tab) pasa a otra,
  F10 o Alt+letra abre un menú (flechas, Intro, Esc), Tab y Mayús+Tab pasan
  de un control a otro, Intro y Esc aceptan y cancelan un diálogo. En los
  cuadros de texto: flechas, Inicio, Fin, Re Pág, Av Pág, Ctrl+Inicio,
  Ctrl+Fin, Mayús con cualquiera de ellas para seleccionar, Ctrl+C / Ctrl+X /
  Ctrl+V (o Ctrl+Ins, Mayús+Supr, Mayús+Ins) y Ctrl+A.
- Para volver a Hispa-DOS: el botón «Salir» de la barra de arriba, o cerrar
  el Centro de programas (pregunta antes). Los grupos abiertos y el sitio de
  cada ventana se guardan en CENTRO.INI y la próxima vez salen igual.
- Al salir deja el modo de texto, el ratón y la memoria como estaban. Mientras
  está el entorno gráfico, un error crítico de disco (INT 24h) hace fallar la
  llamada sin preguntar en la pantalla; no cambia ningún otro vector de
  interrupción. Si el programa falla estando en gráficos, vuelve antes a modo
  texto para que se lea el mensaje de HSPDPMI.

### El Centro de programas: grupos y accesos directos

La ventana del Centro tiene un icono por **grupo**. Cada grupo se abre en su
propia ventana (se mueve, se cambia de tamaño, se minimiza) con un icono por
**acceso directo**. Por omisión hay tres grupos: «Principal» (los programas
de Hispa-DOS que haya en la carpeta de VENTANAS.COM: Editar, Gestor,
Información, y el intérprete de órdenes), «Accesorios» (los programas
internos: Bloc de notas, Calculadora, Reloj y Sistema) y «Juegos» (vacío).

El menú **Archivo** es el mismo en el Centro y en los grupos y actúa sobre el
icono marcado de la ventana que lo tiene:

| Orden | En el Centro (un grupo marcado) | En un grupo (un acceso marcado) |
|---|---|---|
| Abrir (Intro, doble clic) | abre el grupo | abre el programa |
| Nuevo acceso directo... | en el grupo marcado | en este grupo |
| Nuevo grupo... | pide el nombre y lo abre | lo mismo |
| Propiedades... | cambia el nombre del grupo | el diálogo del acceso |
| Mover a otro grupo... | — | lista de grupos |
| Borrar (Supr) | el grupo con sus accesos (pregunta) | el acceso (pregunta) |
| Buscar programas... | añade al grupo marcado | añade a este grupo |

Un acceso directo también se lleva a otro grupo **arrastrando su icono** a la
ventana de ese grupo o al icono del grupo en el Centro.

Un **acceso directo** tiene: nombre; programa (un .COM, .EXE o .BAT con su
ruta, o un programa interno: `:BLOC`, `:CALC`, `:RELOJ`, `:SISTEMA`);
carpeta de trabajo (si se deja vacía, la del programa); parámetros; icono
(se elige con un clic en la fila de dibujos: programa, juego, disco,
documento, herramienta, música, órdenes, bloc, calculadora, reloj, sistema,
carpeta; todos dibujados aquí, en `ventanas/iconos.inc` e `iconos2.inc`, con
`tools/mkiconos.py` de plantilla); «Volver a Hispa Ventanas al terminar»
(si no, al acabar el programa se queda en Hispa-DOS) y «Esperar una tecla al
terminar» (para leer lo que deja escrito un programa que acaba enseguida).
«Examinar...» abre el diálogo de elegir archivo y rellena el programa, la
carpeta y, si falta, el nombre.

**Buscar programas...** pide una carpeta (por omisión `\JUEGOS` de la unidad
de Hispa Ventanas) y crea un acceso directo, con el icono de juego y el
nombre de la carpeta, al programa principal de cada una de sus subcarpetas
(hasta 64). La regla, por este orden:

1. el .EXE, .COM o .BAT que se llama como la subcarpeta (`DOOM\DOOM.EXE`);
2. si no, el único .EXE; si no hay ninguno, el único .COM; si tampoco, el
   único .BAT. No cuentan los que empiezan por SETUP, INSTALL, INSTALAR,
   CONFIG, SETSOUND, SNDSETUP, UNINSTAL, README, LEEME, DOS4GW, CWSDPMI,
   DOS32A, PMODEW o UNIVBE;
3. si quedan varios de la misma clase, pregunta con una lista («Cancelar»
   salta esa carpeta).

No repite un programa que ya tenga acceso directo, y al final dice cuántos
ha añadido.

Límites: 12 grupos, 96 accesos directos en total; el nombre de un acceso se
ve hasta 13 letras debajo del icono; una ventana de grupo no tiene barra de
desplazamiento (se agranda).

### CENTRO.INI

Los grupos, los accesos directos, el sitio de las ventanas y el volumen se
guardan en `CENTRO.INI`, en la carpeta de VENTANAS.COM (`C:\HISPADOS`). Es
texto (página 437, líneas con CR+LF) con un formato propio parecido a un
.INI; se puede cambiar a mano con Hispa Ventanas cerrado. Si no existe (o no
tiene ningún grupo) se crea con los grupos por omisión. Se escribe cuando
cambia algo: al crear, cambiar, mover o borrar, al cerrar el volumen, al
abrir un programa de Hispa-DOS y al salir.

```
; un comentario
[Centro]
Ventana=36,52,452,190
Volumen=80
Silencio=0
Activo=0

[Grupo]
Nombre=Juegos
Ventana=248,188,452,190
Estado=1
Marcado=0

[Acceso]
Nombre=Doom
Icono=juego
Programa=C:\JUEGOS\DOOM\DOOM.EXE
Carpeta=C:\JUEGOS\DOOM
Parametros=
Volver=1
Pausa=0
```

- Las líneas son `clave=valor`; no importan las mayúsculas de las claves ni
  de los nombres de sección, ni los espacios alrededor. Las líneas que
  empiezan por `;`, las vacías y las claves desconocidas se saltan.
- `[Centro]`: `Ventana` = x, y, ancho y alto de la ventana del Centro;
  `Volumen` = 0-100 y `Silencio` = 0/1 (no están hasta que se toca el
  volumen); `Activo` = el grupo cuya ventana estaba delante (0 el primero;
  -1, la del Centro).
- `[Grupo]`: empieza un grupo. `Nombre` (27 letras); `Ventana`, la de su
  ventana; `Estado` = 0 cerrado, 1 abierto, 2 minimizado, 3 maximizado;
  `Marcado` = el icono que tiene marcado (0 el primero).
- `[Acceso]`: un acceso directo del último `[Grupo]`. `Nombre` (31 letras);
  `Icono` (uno de los nombres de la lista de arriba, sin acentos: `musica`,
  `ordenes`; si falta, `programa`); `Programa` (79 letras); `Carpeta` (67);
  `Parametros` (71); `Volver` = 0/1 (si falta, 1); `Pausa` = 0/1 (0).

### Programas de Hispa-DOS desde Hispa Ventanas (juegos)

Los programas internos siguen siendo ventanas. Un acceso directo a un
programa de Hispa-DOS lo ejecuta **fuera** de Hispa Ventanas, con la pantalla
en modo texto y toda la memoria convencional para él, y después vuelve:

1. `VENTANAS.COM` se ejecuta dos veces. La primera copia es el **lanzador**:
   suelta toda su memoria menos los primeros 2 KB (PSP incluido), carga
   HSPRATON y HSPDPMI si faltan (por ese orden, para que HSPDPMI quede el
   último) y ejecuta otra vez VENTANAS.COM con la opción interna `/@`: esa
   segunda copia es el entorno gráfico.
2. Al abrir el acceso, el entorno gráfico comprueba que el programa existe
   (y avisa si el Bloc de notas tiene cambios sin guardar), guarda en
   CENTRO.INI los grupos abiertos, el sitio de cada ventana y cuál está
   delante, deja el **encargo** (programa, parámetros, carpeta y las dos
   casillas) en la memoria del lanzador, vuelve a modo texto y termina con
   el código FEh.
3. El lanzador quita **HSPDPMI** de la memoria (su interfaz propia,
   INT 2Fh AX=D600h BX=4844h DL=1: restaura el vector y entrega el segmento
   que hay que liberar; si HSPDPMI ya estaba cargado antes de entrar en
   Hispa Ventanas no lo descarga, lo desactiva con DL=3, para no dejarlo
   luego en otro sitio de la memoria), cambia a la unidad y la carpeta de
   trabajo y ejecuta
   el programa (función 4B00h) con sus parámetros; un .BAT, con
   `%COMSPEC% /C`. El programa encuentra un DOS limpio: sin servidor DPMI
   (los juegos con DOS4GW, CWSDPMI u otro extensor usan el suyo), con la
   memoria extendida de XMS entera, la variable BLASTER y el driver de ratón
   como estaban, y HSPSND sin tocar.
4. Cuando el programa acaba, el lanzador espera una tecla si se pidió, pone
   el modo de texto si el programa dejó otro, vuelve a la unidad y la
   carpeta de antes y, si el acceso dice «volver», carga otra vez HSPDPMI y
   el entorno gráfico, que se reconstruye desde CENTRO.INI: mismas
   ventanas, mismo sitio, misma ventana delante. Si no, termina y queda
   Hispa-DOS.

Por qué así y no de otra manera: un `VENTANAS.BAT` depende de que nadie lo
toque y deja al intérprete de órdenes en medio; ejecutar el programa desde
dentro (EXEC por la función 0300h) lo dejaría con el servidor DPMI y la
memoria del entorno gráfico cargados. Con el lanzador no hace falta ningún
archivo temporal, funciona desde cualquier carpeta o intérprete y el programa
solo pierde 2 KB de memoria convencional.

Memoria medida en QEMU (16 MB, HSPXMS.SYS y KEYB SP, sin más residentes;
12/11/2026):

| | Bytes libres |
|---|---|
| En Hispa-DOS, antes de entrar (MEM) | 549.920 |
| Un programa abierto desde un acceso directo (su bloque de memoria) | unos 544.800 (532 KB) |
| `A:\COMMAND.COM /C MEM` desde un acceso directo (con ese segundo COMMAND.COM, de 46 KB, cargado) | 498.592 |
| En Hispa-DOS, al salir (queda HSPRATON, 2.400 bytes) | 547.504 |

Es decir: el programa recibe toda la memoria libre de Hispa-DOS menos unos
5 KB (2 KB del lanzador, 2,4 KB de HSPRATON si no estaba cargado y la copia
del entorno). Lo que hoy limita esa cifra no es Hispa Ventanas sino lo que
ocupan el núcleo (57 KB) y COMMAND.COM (46 KB) en memoria convencional.
`tests/run_ventanas4_tests.sh` comprueba en cada pasada que el programa
recibe la memoria libre menos 4 KB como mucho.

Si HSPDPMI se cargó a mano antes de entrar (en AUTOEXEC.BAT), no se
descarga: queda **desactivado** (no responde a 1687h, pero sigue ocupando
sus 17 KB) mientras corre el programa y se activa de nuevo al volver. Para
que los juegos tengan esos 17 KB, lo mejor es no cargarlo y dejar que lo
haga Hispa Ventanas, que lo pone el último y lo quita de verdad. Lo mismo
pasa con `HSPDPMI /D` a mano si después de HSPDPMI se cargó otro residente
que también usa INT 2Fh: no se puede sacar de la cadena y se desactiva.

### El volumen

El altavoz de la barra superior (a la izquierda de «Salir») abre y cierra una
ventanita con una barra deslizante de 0 a 100 % (con el ratón o con las
flechas izquierda y derecha) y la casilla «Silencio». El volumen se le da al
driver HSPSND por su interfaz (INT 2Fh AX=D700h, función 04h, AL=0..31),
llamándolo en modo real. HSPSND no tiene función para leer el volumen, así
que el último valor se guarda en CENTRO.INI y se vuelve a poner al entrar.
Si HSPSND no está cargado, o no ha encontrado tarjeta, el altavoz sale gris
y la ventanita lo dice. Con el volumen en silencio, el altavoz lleva un aspa.

### Memoria y modo del procesador

El núcleo gráfico es de 32 bits (386 o superior, que es lo que exige HSPDPMI):
un segmento de código de 32 bits y un único segmento de datos y pila de 4 GB,
los dos con la base del programa. Así un puntero «cercano» llega a toda la
memoria: *puntero = dirección lineal − base*. La superficie de dibujo y el
«montón» de donde salen las fichas de las ventanas, las pilas de las tareas y
los búferes de los controles (memoria extendida, función 0501h), el marco
lineal de la tarjeta (0800h) y los datos de la BIOS (el reloj y el estado de
las teclas de mayúsculas) se usan con punteros normales, sin cambiar de
segmento.

Las aplicaciones de Windows 3.x de las etapas 4 y 5 seguirán siendo de 16
bits: tendrán sus propios segmentos de 16 bits en la misma LDT y entrarán en
este núcleo por puertas de llamada. Lo que es de 16 bits es la API, no el
motor.

Las llamadas a DOS y a la BIOS se hacen por el servidor DPMI: `INT 10h`,
`INT 16h`, `INT 21h` e `INT 33h` con registros se reflejan solas; lo que
necesita un búfer va por la función 0300h (ver «Archivos») o se hace antes de
entrar, en modo real (VBE 4F00h/4F01h).

### Capas

Cada capa está en su archivo de `ventanas/` y solo usa las de debajo por las
funciones que se listan (la lista completa, con sus registros, está en la
cabecera de cada archivo). Todas conservan los registros, salvo EAX cuando
devuelven algo.

```
  centro.inc bloc.inc calc.inc reloj.inc    los programas
  config.inc     CENTRO.INI: grupos, accesos directos, ventanas
  volumen.inc    el altavoz de la barra y su ventanita (HSPSND)
        |
  dialogos.inc   mensaje, pedir texto, elegir archivo (modales)
  controles.inc  botón, casilla, opción, texto, cuadros de texto, lista
  menus.inc      barra de menús y desplegables
  dos.inc        archivos (INT 21h desde modo protegido)
        |
  escritorio.inc fondo, barra superior, iconos de lo minimizado
  gestor.inc     ventanas, orden, activa, foco, captura, zona no
        |        cliente, reparto del ratón y del teclado          (USER)
  nucleo.inc     memoria, tareas, colas de mensajes, temporizadores
        |
  dc.inc         regiones de recorte y contexto de dispositivo
  gdi.inc        dibujo con recorte sobre la superficie            (GDI)
        |
  video.inc      driver: modo, paleta, superficie -> pantalla      (DISPLAY)
  video16.inc    (parte de modo real: elegir el modo)
  raton.inc      cursor por software y avisos del driver de ratón  (MOUSE)
  defs.inc       mensajes, estilos y fichas (las constantes de la interfaz)
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

**Dibujo** (`gdi.inc`): primitivas sobre la superficie con un rectángulo de
recorte: `gdi_punto`, `gdi_linea`, `gdi_rect`, `gdi_marco`, `gdi_mosaico`,
`gdi_copiar`, `gdi_copiar_clave`, `gdi_leer`, `gdi_texto`. Los programas no
las llaman: usan las `dc_*` de su ventana (más abajo).

La **fuente** «Hispa 8×16» se diseña en texto (`ventanas/fuente.txt`: una
rejilla de `#` y `.` por carácter, y las letras acentuadas compuestas de letra
más acento) y `tools/mkfuente.py` la convierte en la tabla que se ensambla.
Tiene ASCII completo y las letras acentuadas y los signos de la página 437
que usan el español, el catalán, el gallego y el francés. La negrita la hace
`gdi_texto`. `make` deja una vista en `build/gen/ventanas/fuente.ppm`.

Los **iconos** (32×32) también están en texto, en `ventanas/iconos.inc` y
`ventanas/iconos2.inc`: un carácter por punto, `.` transparente y `0`-`9`,
`a`-`f` los 16 colores. La plantilla la dibuja `tools/mkiconos.py`; luego se
retocan a mano. Al compilar, `tools/icopack.py` los deja a 4 bits por punto
(el transparente ocupa el sitio del color 3, que por eso no se usa en los
iconos).

**Ratón y teclado** (`raton.inc`, `ventanas.asm`)

La entrada llega **por interrupciones**. Al arrancar, Hispa Ventanas pide al
driver de ratón (INT 33h, función 0Ch) que llame a `rm_raton` —una rutina de
modo real que está dentro de VENTANAS.COM— en cada movimiento y en cada
cambio del botón izquierdo; `rm_raton` solo apunta el suceso (botones y
contadores de movimiento) en una cola de 64. Las teclas las encola la BIOS
(IRQ 1), ya traducidas por KEYB: de ahí salen la ñ y los acentos. El bucle
del sistema, cuando no hay nada que hacer, duerme con `HLT` en modo real
(`rm_sondeo`, función 0301h) hasta la siguiente interrupción; al despertar
vacía la cola del ratón y la de teclas (INT 16h) y lo reparte. La posición
del cursor la lleva Hispa Ventanas en puntos de pantalla, así que no depende
de que el driver de ratón conozca el modo gráfico. Al salir (también si el
programa falla) se retira el aviso.

No se usa la función 0303h (retrollamadas de modo real a modo protegido): la
rutina de modo real y la cola están en la memoria convencional del propio
programa, que se ve desde los dos modos, y así cada suceso del ratón no
cuesta dos cambios de modo.

**Regiones y contexto de dispositivo** (`dc.inc`)

Cada ventana solo dibuja en su **región visible**: su zona menos lo que tapan
las ventanas que tiene encima (y sus sombras), como lista de rectángulos. Así
no se repinta lo tapado y una ventana de detrás puede pintar sin estropear
las de delante. El contexto de dispositivo es esa región más el origen de la
zona de la ventana: las funciones `dc_*` reciben coordenadas de la ventana y
repiten la primitiva en cada rectángulo visible.

**Cómo se pinta, sin parpadeo.** Nadie dibuja en la pantalla. Quien cambia
algo marca la zona (`ven_invalidar`); cada tarea, cuando no tiene otros
mensajes, recibe `WM_PAINT` para sus ventanas marcadas y las pinta en la
superficie; y una vez por vuelta, cuando ya no queda nada por pintar, el
sistema repone el cursor y lleva a la pantalla solo el rectángulo que ha
cambiado (`ges_presentar`). El marco de las ventanas (zona no cliente) lo
pinta el propio gestor.

**Tareas** (`nucleo.inc`): multitarea cooperativa. Cada programa es una tarea
con su pila (8 KB) y su cola de mensajes (48). Solo se cambia de tarea dentro
de `msg_tomar`, cuando la que corre no tiene mensajes, nada que pintar ni
temporizadores vencidos. La tarea 0 es el sistema: recoge la entrada, la
reparte, atiende el escritorio y los menús y presenta la pantalla. Un diálogo
modal es un bucle de mensajes dentro de la tarea que lo abre: las demás
siguen funcionando.

### Archivos: INT 21h desde modo protegido

Las funciones de archivo de DOS quieren punteros de modo real. Había dos
caminos: que HSPDPMI traduzca los punteros de INT 21h (3Ch, 3Dh, 3Fh, 40h…),
o usar la función 0300h con memoria convencional. **Se ha elegido lo
segundo**, dentro de Hispa Ventanas (`dos.inc`), porque:

- no hace falta pedir memoria de DOS: VENTANAS.COM es un .COM y su segmento
  de datos de modo protegido tiene la base del programa, así que un búfer de
  paso de 2 KB dentro del propio programa (`dos_buf`) se ve a la vez desde
  modo protegido (como puntero) y desde modo real (segmento del programa);
- HSPDPMI no cambia: sigue pasando sus pruebas con los clientes de terceros,
  que ya usan 0300h (DJGPP) y no ganan nada con la traducción;
- la traducción en el servidor hará falta de verdad en la etapa 4 (las
  aplicaciones de Windows de 16 bits llaman a INT 21h con punteros de modo
  protegido), y se hará entonces, en KERNEL, con las reglas de Windows.

`dos.inc` da: `dos_abrir`, `dos_crear`, `dos_cerrar`, `dos_leer`,
`dos_escribir` (en trozos de 2 KB), `dos_puntero`, `dos_borrar`,
`dos_cambiar_dir`, `dos_crear_dir`, `dos_dir_actual`, `dos_primero` y
`dos_siguiente` (funciones 3Dh, 3Ch, 3Eh, 3Fh, 40h, 42h, 41h, 3Bh, 39h,
19h+47h, 4Eh y 4Fh), todas con CF=1 y el código de DOS en EAX si hay error.

### Aspecto

Propio, sin parecidos: fondo de mosaico (un azulejo de rombos) en azul
petróleo; barra superior marrón con un filete ocre; ventanas de color crema
con marco marrón y sombra dura; barra de título terracota con un rombo ocre y
el texto en negrita si es la activa, y arena si no lo es; botones del título
de color crema con su dibujo (raya, recuadro, aspa); menús sobre piedra con
la orden marcada en añil; botones de color arena; casillas cuadradas y
opciones en forma de rombo, con la marca terracota; cursor de texto
terracota; cursor del ratón ocre con borde de tinta. La paleta y los dibujos
están en `ventanas/datos.inc` y `ventanas/iconos.inc`.

## La interfaz de la etapa 3

Es interna (ensamblador, registros): la base sobre la que la etapa 4 pondrá
la API de Windows de 16 bits. Los **mensajes** llevan los nombres y los
números documentados de Windows (es interfaz, no código); lo demás tiene
nombres propios en español. Todo está en `ventanas/defs.inc`.

### Convenio de registros

| Registro | Uso |
|---|---|
| EBP | la ventana (puntero a su ficha; es lo que en Windows sería el HWND) |
| EAX | el mensaje; a la vuelta, el resultado |
| ECX | wParam |
| EDX | lParam |

Un **procedimiento de ventana** recibe eso y puede cambiar todos los
registros; devuelve el resultado en EAX. Lo que no atiende lo pasa con
`jmp ven_defecto`. Las funciones del sistema conservan los registros (salvo
EAX si devuelven algo). Los rectángulos van siempre en EAX=x, EBX=y,
ECX=ancho, EDX=alto.

### Mensajes

| Mensaje | Cuándo | ECX, EDX |
|---|---|---|
| `WM_CREATE` | al crear la ventana, antes de mostrarla | EDX=plantilla |
| `WM_DESTROY` | al destruirla (las hijas, antes) | |
| `WM_CLOSE` | botón de cerrar, Alt+F4 (por defecto: se destruye) | |
| `WM_PAINT` | hay algo que repintar | |
| `WM_SIZE`, `WM_MOVE` | ha cambiado de tamaño / de sitio | `WM_SIZE`: EDX=ancho + alto×65536 de la zona |
| `WM_SETFOCUS`, `WM_KILLFOCUS` | toma o pierde el foco del teclado | |
| `WM_KEYDOWN` | tecla que no escribe (flechas, Intro, Tab, Esc, Supr, F1…) | ECX=tecla (`TC_*`; Ctrl+letra = `TC_CTRL`+1…26), EDX=mayúsculas (`TM_*`) |
| `WM_CHAR` | carácter (página 437, con el teclado de KEYB) | ECX=carácter |
| `WM_MOUSEMOVE`, `WM_LBUTTONDOWN`, `WM_LBUTTONUP`, `WM_LBUTTONDBLCLK` | ratón sobre la zona de la ventana (o capturado) | EDX=x + y×65536 (con signo, en la zona), ECX=mayúsculas |
| `WM_COMMAND` | orden de menú o aviso de un control | menú: ECX=número. Control: ECX=id + aviso×65536, EDX=el control |
| `WM_TIMER` | vence un temporizador | ECX=su número |
| `WM_QUIT` | lo pone `msg_salir`: `msg_tomar` devuelve 0 | |
| `WM_SETTEXT`, `WM_GETTEXT`, `WM_CUT`, `WM_COPY`, `WM_PASTE` | cuadros de texto | ver controles |
| `WM_USER`+n | propios de cada control (`BM_*`, `EM_*`, `LM_*`, `DM_INICIO`) | |

Las teclas que no quiere una ventana hija suben a su madre (así Intro y Esc
llegan al diálogo); Tab lo atiende `ven_defecto` pasando el foco a la hermana
siguiente con `E_TABULA`. Mientras el botón está apretado, el ratón es de la
ventana donde se apretó (captura), hasta que se suelta.

### Ventanas

La **ficha** (`V_*`, 128 octetos): clase, procedimiento, madre, tarea, estilo
(`E_*`), estado (`S_*`), rectángulo en la pantalla, texto, id, menú, icono,
zona por repintar y ocho palabras para la clase (`V_D0`…`V_D7`).

Una **clase** es `dd procedimiento` y `db color de fondo` (o `SIN_FONDO`),
`banderas` (`CB_FOCO`: toma el foco con un clic), `0, 0`.

Una ventana se crea con una **plantilla**:
`PLANTILLA clase, texto, estilo, x, y, ancho, alto, id` (las hijas, en
coordenadas de la zona de su madre).

| Estilo | Significado |
|---|---|
| `E_HIJA` | ventana hija (control) |
| `E_TITULO`, `E_CERRAR`, `E_MAXIM`, `E_MINIM`, `E_TAMANO` | marco con título; cada botón; bordes que cambian el tamaño (`E_NORMAL` = todos, `E_DIALOGO` = título y cerrar) |
| `E_EMERGENTE` | sin título, con un filete (los menús) |
| `E_TABULA` | el foco para en ella con Tab |
| `E_OCULTA` | se crea sin mostrar |
| `E_SINFOCO` | no toma el foco aunque se haga clic |

| Función | Registros |
|---|---|
| `ven_crear` | ESI=plantilla EBP=madre (0: ninguna) → EAX=ventana |
| `ven_lista` | ESI=plantillas seguidas acabadas en `dd 0`, EBP=madre |
| `ven_destruir` | EBP |
| `ven_mover` | EBP, EAX=x EBX=y ECX=ancho EDX=alto |
| `ven_mostrar` | EBP, EAX=0 ocultar, 1 mostrar, 2 minimizar, 3 maximizar, 4 restaurar |
| `ven_activar`, `ven_enfocar` | EBP: delante y activa / con el foco del teclado |
| `ven_capturar`, `ven_soltar` | EBP: captura del ratón |
| `ven_invalidar`, `ven_invalidar_rect` | EBP (y el rectángulo): a repintar |
| `ven_texto` | EBP, ESI=texto (se guarda el puntero) |
| `ven_cli` | EBP → EAX=x EBX=y (pantalla) ECX=ancho EDX=alto de su zona |
| `ven_hija_id` | EBP=madre ECX=id → EAX=la hija |
| `ven_defecto` | el procedimiento por defecto |

| Mensajes y tareas | Registros |
|---|---|
| `msg_tomar` | EDI=mensaje (16 octetos: ventana, mensaje, wParam, lParam): espera uno → EAX=0 si es `WM_QUIT` |
| `msg_mirar` | lo mismo sin esperar; CF=1 si no hay |
| `msg_despachar` | EDI=mensaje: lo entrega a su ventana |
| `msg_poner` | EBP, EAX, ECX, EDX: a la cola de la tarea de la ventana |
| `msg_enviar` | lo mismo, llamando ya al procedimiento → EAX |
| `msg_salir` | pone `WM_QUIT` en la cola de la tarea que corre |
| `tar_crear` | EAX=entrada → EAX=tarea; al volver la entrada, la tarea acaba y se destruyen sus ventanas |
| `tmp_poner`, `tmp_quitar` | EBP=ventana ECX=número (EDX=milisegundos) |
| `mem_pedir`, `mem_soltar` | ECX=octetos → EAX=puntero (a cero) / EAX=puntero |

| Dibujo (dentro de `WM_PAINT`) | Registros |
|---|---|
| `dc_pintar` | EBP: abre el contexto de lo que hay que repintar y lo borra con el fondo de la clase |
| `dc_abrir` | EBP: contexto de toda la zona (para dibujar fuera de `WM_PAINT`) |
| `dc_cerrar` | |
| `dc_cortar` | EAX, EBX, ECX, EDX: estrecha el contexto a un rectángulo |
| `COLOR c` | (macro) el color; `gdi_negrita` = 1 para negrita |
| `dc_rect`, `dc_marco` | EAX=x EBX=y ECX=ancho EDX=alto |
| `dc_linea` | EAX=x0 EBX=y0 ECX=x1 EDX=y1 |
| `dc_punto` | EAX=x EBX=y |
| `dc_texto`, `dc_texto_m`, `dc_texto_n` | EAX=x EBX=y ESI=texto (con `&` de menú / de ECX caracteres) |
| `dc_icono` | EAX=x EBX=y ESI=icono |
| `dc_rombo` | EAX=x EBX=y del centro, ECX=radio |
| `dc_mosaico`, `dc_copiar_clave` | como las de `gdi.inc` |

### Controles (`controles.inc`)

Son clases de ventana hija. Avisan a su madre con `WM_COMMAND`.

| Clase | Qué es | Mensajes y avisos |
|---|---|---|
| `c_boton` | botón; con `BS_CASILLA`, casilla; con `BS_OPCION`, opción (una sola de las de su madre); `BS_DEFECTO`, con doble marco | clic, Espacio o Intro → `WM_COMMAND` con su id. `BM_MARCA`, `BM_PONER_MARCA` |
| `c_estatico` | texto fijo (el carácter 10 cambia de línea); `SS_DERECHA`, `SS_HUNDIDO`, `SS_NEGRITA` | |
| `c_edit` | cuadro de texto de una línea (255 caracteres); con `ES_MULTI`, de varias (60.000) con barra de desplazamiento | `WM_SETTEXT` (EDX=texto), `WM_GETTEXT` (ECX=tamaño, EDX=búfer), `WM_CUT`, `WM_COPY`, `WM_PASTE`, `EM_SEL_TODO`, `EM_MODIFICADO`, `EM_PONER_MODIF`, `EM_LARGO`; `ed_poner`, `ed_datos`, `ed_seleccionar` |
| `c_lista` | lista de textos (200 de 39 letras) con barra de desplazamiento | `LM_ANADIR` (EDX=texto), `LM_VACIAR`, `LM_SELEC`, `LM_PONER_SELEC`, `LM_TEXTO`, `LM_CUENTA`; avisos `LN_SELEC` y `LN_DOBLE` |

El **portapapeles** guarda texto (hasta 64 KB): `pp_poner` (ESI, ECX) y
`pp_buf`, `pp_len`. Dentro, el texto de varias líneas lleva solo LF; el Bloc
de notas pone CR+LF al guardar y los quita al abrir.

### Menús (`menus.inc`)

La barra es una tabla a la que apunta `V_MENU` (se pone en `WM_CREATE`):

```
menu_mio:
        db "&Archivo", 0            ; título («&»: la letra de Alt+letra)
        dw 101                      ;   número de la orden
        db "&Nuevo", 0              ;   su texto
        dw 0                        ;   número 0: una raya
        db 0
        dw 0xFFFF                   ; fin del desplegable
        db "A&yuda", 0
        ...
        db 0                        ; fin de la barra
```

Al elegir una orden, la ventana recibe `WM_COMMAND` con ECX=número.

### Diálogos (`dialogos.inc`)

| Función | Registros |
|---|---|
| `dlg_mensaje` | EBP=dueña ESI=texto EDI=título EAX=`DM_ACEPTAR`, `DM_ACEPTAR_CANCELAR` o `DM_SI_NO_CANCELAR` → EAX=`ID_ACEPTAR`, `ID_CANCELAR`, `ID_SI`, `ID_NO` |
| `dlg_pedir` | EBP=dueña ESI=pregunta EDI=título EBX=búfer ECX=tamaño → EAX=1 si se acepta |
| `dlg_archivo` | EBP=dueña EDI=título EBX=búfer del nombre → EAX=1; lista la carpeta actual y deja entrar en las carpetas |
| `dlg_lista` | (en `centro.inc`) EBP=dueña EDI=título ESI=pregunta EBX=textos, uno cada EDX octetos, ECX=cuántos → EAX=el elegido (-1: ninguno) |
| `dlg_modal` | EBP=dueña ESI=plantilla del diálogo EDI=plantillas de los controles EBX=gancho EDX=parámetro → EAX=id del botón que lo cierra |

En `dlg_modal`, cualquier botón con id menor que 16 cierra el diálogo y su id
es la respuesta. El **gancho** recibe todos los mensajes del diálogo antes
que el procedimiento normal (y `DM_INICIO` cuando ya están los controles) y
devuelve EAX distinto de 0 si ya lo ha atendido: ahí se rellenan los
controles al empezar y se leen al aceptar (ver `ro_gancho` en `reloj.inc`).

### Cómo se escribe un programa

Un programa es un archivo `.inc` con una clase, una plantilla, la entrada de
la tarea y el procedimiento de su ventana. El más corto, completo:

```nasm
c_hola      dd hola_proc
            db C_CREMA, 0, 0, 0             ; se borra con crema
hola_plant:
        PLANTILLA c_hola, t_hola, E_NORMAL, 100, 100, 300, 160, 0
t_hola      db "Hola", 0
t_hola_txt  db "¡Hola, Hispa Ventanas!", 0

hola_tarea:                                 ; la entrada de la tarea
        mov esi, hola_plant
        xor ebp, ebp
        call ven_crear
        mov [hola_ven], eax                 ; (una dd en bss3.inc)
        jmp app_bucle                       ; msg_tomar / msg_despachar

hola_proc:                                  ; EBP=ventana EAX=mensaje ECX EDX
        cmp eax, WM_PAINT
        je .pintar
        cmp eax, WM_DESTROY
        jne ven_defecto
        mov dword [hola_ven], 0
        call msg_salir                      ; acaba el bucle y la tarea
        xor eax, eax
        ret
.pintar:
        call dc_pintar
        COLOR C_TINTA
        mov eax, 20
        mov ebx, 20
        mov esi, t_hola_txt
        call dc_texto
        call dc_cerrar
        xor eax, eax
        ret
```

Para que se pueda abrir desde el Centro de programas se añade una línea a
`int_tabla` en `centro.inc` (su nombre interno, como `:HOLA`, la entrada y
la variable de su ventana) y se sube `INT_N`; con un acceso directo cuyo
programa sea `:HOLA` ya se abre (para que salga en «Accesorios» en una
instalación nueva, otra línea en `def_accesorios`, en `config.inc`); el
`%include` va en `ventanas.asm`. Los controles se crean en
`WM_CREATE` con `ven_lista` y una lista de plantillas (ver `calc_hijos`); el
menú, poniendo `V_MENU`; los temporizadores, con `tmp_poner` (ver
`reloj.inc`). Mientras un programa hace algo largo no cede el turno: lo
largo se parte con un temporizador.

Límites de hoy: 160 ventanas (contando controles), 32 de primer nivel, 8
tareas, 16 temporizadores, una copia de cada programa a la vez.

### Los programas

- **Centro de programas** (`centro.inc`, `config.inc`): los grupos, cada uno
  en su ventana, y sus accesos directos; menú Archivo y Ayuda. Cerrarlo es
  salir de Hispa Ventanas.
- **Bloc de notas** (`bloc.inc`): texto de hasta 60.000 caracteres. Archivo:
  Nuevo, Abrir, Guardar, Guardar como, Salir. Edición: Cortar, Copiar, Pegar,
  Seleccionar todo, Buscar. Pregunta antes de perder cambios. Sin extensión,
  guarda como `.TXT`.
- **Calculadora** (`calc.inc`): cuatro operaciones con coma decimal, en
  coma fija de 64 bits con seis decimales (no necesita coprocesador), con el
  ratón o el teclado.
- **Reloj** (`reloj.inc`): esfera con agujas que se adapta al tamaño de la
  ventana y hora en cifras; opciones (sin segundero, solo cifras).
- **Sistema** (`centro.inc`): pantalla, vídeo, ratón y memoria que ha
  encontrado.

## Pruebas

- `tests/run_ventanas_tests.sh`: gráficos y modos de pantalla con las
  ventanas de muestra (`VENTANAS /DEMO`).
- `tests/run_ventanas3_tests.sh`: la etapa 3 entera en QEMU con el teclado
  español: abre cada programa desde el Centro, escribe en el Bloc de notas
  (con ñ y acentos), guarda y comprueba el archivo en la imagen con mtools,
  usa menús, diálogos, portapapeles, lista y barras, mueve, cambia de tamaño,
  maximiza, minimiza y cierra ventanas, calcula 12+30=42 y comprueba el
  temporizador del Reloj. Las capturas se comparan punto a punto con
  `tests/tools/ventanas_check.py` (que conoce la paleta y la fuente) y se
  dejan de muestra en `build/ventanas3_*.png`.
- `tests/run_ventanas4_tests.sh`: el Centro con grupos y el lanzador, con
  una Sound Blaster 16: crea un grupo, busca programas en `A:\JUEGOS` (las
  tres reglas y un .BAT), cambia las propiedades de un acceso, lo lanza (el
  programa de prueba `tests/progs/lanztest.asm` apunta sus parámetros, su
  carpeta, su memoria, si hay DPMI y el volumen del mezclador), comprueba
  que Hispa Ventanas vuelve con el grupo abierto y delante, mueve accesos
  arrastrando y por el menú, cambia el volumen y el silencio, y al salir
  mira CENTRO.INI, la memoria y que HSPDPMI ya no está. Una segunda sesión
  arranca con un CENTRO.INI escrito a mano, mide la memoria con MEM desde
  un acceso directo y prueba un acceso roto y otro sin «volver». Capturas
  de muestra en `build/ventanas4_*.png`.
- Programas de prueba de Windows 16 bits propios (NASM + nuestra
  herramienta de cabeceras NE) para cada función de la API, con su
  resultado escrito en un archivo (etapa 4).
- Las aplicaciones de terceros (Windows 3.11, Word, Excel) solo en local, con
  las copias de cada uno; nunca en el repositorio.
