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
   `tests/run_ventanas3_tests.sh` y `tests/run_ventanas4_tests.sh`. Los
   programas nuevos van aparte, en módulos .HVP: los primeros, los juegos
   Culebra y Campo de minas (ver «Programas aparte: los módulos .HVP»;
   `tests/run_juegos_tests.sh`), y después Pintura, el programa de dibujo
   (`tests/run_pintura_tests.sh`). Próximos programas propios (nombres de
   Carlos): **Pluma-Ventanas**, el procesador de textos (letra con formato,
   párrafos, imprimir), y **Cuentas-Ventanas**, la hoja de cálculo (fórmulas, formatos de
   número, gráficos sencillos); además el Administrador de archivos. La
   segunda tanda de juegos (Bloques, Solitario, Tragabolas y Almacén) ya
   está (`tests/run_juegos2_tests.sh`). Pendiente (se
   hará según haga falta en las etapas 4 y 5): avisar de los
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

Hispa Ventanas es `VENTANAS.COM` (unos 58 KB) y los programas aparte que
carga (módulos .HVP, más abajo), todo hecho en NASM. Arranca en modo real, comprueba lo que necesita, elige el modo
de vídeo y entra en modo protegido como **cliente DPMI de 32 bits**. Los
programas de siempre (Centro, Bloc de notas…) van dentro, cada uno en su
archivo y como una tarea; los nuevos (los juegos y Pintura) van aparte, en
archivos .HVP que se cargan como una tarea más.

`VENTANAS.COM` se ejecuta dos veces: la primera copia es el **lanzador**
(2 KB que se quedan en memoria convencional) y ejecuta la segunda, que es el
entorno gráfico (ver «Programas de Hispa-DOS desde Hispa Ventanas»).

Un .COM no pasa de 64 KB y lo que usa la parte de modo real (el código de
arranque, el búfer de paso de DOS, las colas del ratón y del teclado) tiene
que estar en los primeros 64 KB del programa. Por eso los iconos se guardan
a 4 bits por punto (`tools/icopack.py` los empaqueta al compilar; en los
fuentes siguen en texto) y la pila y las variables que solo usa el modo
protegido van detrás, donde llegan los punteros de 32 bits. Si lo de modo
real se sale de los 64 KB, la compilación falla. Quedan unos 2 KB de
margen: por eso lo nuevo va en módulos .HVP, que no gastan de ahí.

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
Información, el intérprete de órdenes y, el último, Configuración, que es un
módulo .HVP que está en `\HISPADOS`: ver «Configuración y VENTANAS.INI»), «Accesorios» (los programas
internos: Bloc de notas, Calculadora, Reloj y Sistema, y Pintura, que es un
módulo .HVP) y «Juegos» (los juegos propios). Los módulos .HVP están en dos
carpetas al lado de VENTANAS.COM: los juegos en `\HISPADOS\JUEGOS` y los
demás programas en `\HISPADOS\PROGS`.

Los programas nuevos entran solos: al arrancar, si los .HVP de esas dos
carpetas no son los mismos que la última vez (se cuenta cuántos hay entre
las dos, `Modulos` en CENTRO.INI), los que no tengan acceso directo se
añaden a su grupo, con el nombre que lleva el módulo. El grupo lo dice el
propio módulo en su cabecera (ver «El formato»): «Juegos» o «Accesorios»
(si no lo hay, se crea); la carpeta solo sirve para tenerlos ordenados. No
se toca nada más de un CENTRO.INI que ya exista (al que se hizo antes de que
existiera Pintura se le añade su acceso en «Accesorios»), y un acceso que
se borre a mano no vuelve mientras no cambie el número de módulos.

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
ruta, un programa de Hispa Ventanas —un módulo .HVP, que se abre como una
ventana y sale con su propio icono— o un programa interno: `:BLOC`,
`:CALC`, `:RELOJ`, `:SISTEMA`);
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

Además añade un acceso por cada programa de Hispa Ventanas (.HVP) que haya
en la carpeta misma, con el nombre que lleva el módulo (un archivo que se
llame .HVP sin serlo no cuenta).

No repite un programa que ya tenga acceso directo, y al final dice cuántos
ha añadido.

Límites: 12 grupos, 96 accesos directos en total; el nombre de un acceso se
ve hasta 14 letras debajo del icono; una ventana de grupo no tiene barra de
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
Modulos=7

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
  -1, la del Centro); `Modulos` = cuántos .HVP había en las carpetas `JUEGOS`
  y `PROGS` la última vez que se miró (no está si no hay ninguno).
- `[Grupo]`: empieza un grupo. `Nombre` (27 letras); `Ventana`, la de su
  ventana; `Estado` = 0 cerrado, 1 abierto, 2 minimizado, 3 maximizado;
  `Marcado` = el icono que tiene marcado (0 el primero).
- `[Acceso]`: un acceso directo del último `[Grupo]`. `Nombre` (31 letras);
  `Icono` (uno de los nombres de la lista de arriba, sin acentos: `musica`,
  `ordenes`; si falta, `programa`); `Programa` (79 letras); `Carpeta` (67);
  `Parametros` (71); `Volver` = 0/1 (si falta, 1); `Pausa` = 0/1 (0).

### Programas de Hispa-DOS desde Hispa Ventanas (juegos)

Los programas internos y los módulos .HVP son ventanas. Un acceso directo a
un programa de Hispa-DOS lo ejecuta **fuera** de Hispa Ventanas, con la pantalla
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

### Configuración y VENTANAS.INI

**Configuración** (`programas/config.asm`, `CONFIG.HVP`, en `\HISPADOS`, al
lado de VENTANAS.COM; acceso directo en el grupo «Principal») es el programa
de los ajustes. Es un módulo .HVP con diseño propio: los apartados a la
izquierda (un rombo y su nombre; el elegido, en añil) y, a la derecha, el
apartado elegido. Abajo, lo que pasa con los cambios y los botones «De
fábrica» (pregunta antes), «Guardar» y «Cerrar».

| Apartado | Qué tiene |
|---|---|
| Pantalla | Tema (ocho combinaciones hechas: Hispa, Noche, Mar, Campo, Vino, Duna, Pizarra, Ladrillo), dibujo del fondo (diez: rombos, liso, puntos, rayas, cuadros, ladrillos, olas, damero, trama, zigzag; se ven con los colores puestos), color del fondo y color del dibujo (los 16 de la paleta), y el salvapantallas: casilla, minutos (1 a 60) y «Probar» |
| Ratón | Velocidad del puntero (de x0,5 a x3), tiempo máximo del doble clic (165 a 990 ms, con una zona para probarlo) e intercambiar los botones (para zurdos) |
| Teclado | Espera antes de repetir (250 a 1000 ms) y velocidad de repetición (2 a 30 por segundo), con un cuadro para probarlas; distribución española o estadounidense, si TECLADO (KEYB) está cargado |
| Sonido | Volumen y silencio: los mismos del altavoz de la barra (si no hay HSPSND, lo dice) |
| Fecha y hora | La fecha con todas las letras y la hora, al segundo; seis cuadros (día, mes, año, hora, minuto, segundo) y «Poner fecha y hora» |
| Sistema | Versión de Hispa Ventanas y de Hispa-DOS, procesador, memoria convencional, extendida y libre, pantalla, vídeo, ratón, sonido, teclado y país |

Todo se aplica **al momento**. Lo de Pantalla, Ratón y Teclado se guarda en
`VENTANAS.INI`: con «Guardar», al cerrar la ventana y, solo, unos tres
segundos después del último cambio (así no se pierde si se sale de Hispa
Ventanas con la ventana abierta). El volumen no va ahí: lo sigue llevando
Hispa Ventanas (ver «El volumen»). La fecha y la hora se le dan a Hispa-DOS
(INT 21h, funciones 2Bh y 2Dh), que las pone también en el reloj del equipo.
Hispa-DOS solo tiene un país (España, página 437), así que no hay nada que
elegir de idioma ni de país: sale en «Sistema».

**Cómo se reparte el trabajo.** En VENTANAS.COM apenas queda sitio, así que
dentro solo está lo mínimo (`ventanas/ajustes.inc`, unos 400 octetos): los
valores y cómo se aplican (la velocidad y los botones del ratón y el doble
clic, en `eventos`; el fondo, en el escritorio). Leer y escribir
VENTANAS.INI, los dibujos del fondo y el salvapantallas los hace CONFIG.HVP:

1. Al arrancar, el Centro de programas carga `CONFIG.HVP` de su carpeta con
   el parámetro `/A` (`aj_iniciar`). El módulo lee VENTANAS.INI, aplica los
   ajustes por la interfaz (`aj_poner`, `esc_fondo`) y **se queda en
   marcha con la ventana oculta**. Si CONFIG.HVP no está, todo queda como
   viene de fábrica.
2. Abrir el acceso directo «Configuración» enseña esa ventana (un módulo ya
   abierto no se carga otra vez: `hvp_abrir` trae su ventana delante, y
   ahora también si está oculta). Cerrarla la vuelve a ocultar.
3. Mientras está en marcha mira una vez por segundo cuánto hace del último
   movimiento del ratón, clic o tecla (`AJ_ULT_ACT`); pasado el tiempo
   puesto, abre el **salvapantallas**: una ventana que ocupa toda la
   pantalla, en negro, con un letrero («Hispa Ventanas» y la hora) que se
   pasea. Se quita con el ratón o con una tecla (que no llega a la ventana
   de debajo).

Ocupa una de las 6 plazas de módulos, una tarea y 6 ventanas (con la
ventana oculta, los controles del apartado no existen).

`VENTANAS.INI` está en la carpeta de VENTANAS.COM (`C:\HISPADOS`). Es texto
(página 437, líneas con CR+LF), con el mismo aire que CENTRO.INI; se puede
cambiar a mano con Hispa Ventanas cerrado. Si no existe, vale lo de fábrica,
y no se crea hasta que se cambia algo.

```
; Hispa Ventanas - ajustes del programa Configuración (CONFIG.HVP).
; Se puede cambiar a mano con Hispa Ventanas cerrado (docs/VENTANAS.md).

[Pantalla]
Dibujo=rombos
ColorFondo=2
ColorDibujo=3
Salvapantallas=0
Minutos=5

[Raton]
Velocidad=2
DobleClic=440
Zurdo=0

[Teclado]
Espera=500
Repeticion=20
Distribucion=SP
```

- Líneas `clave=valor`; no importan las mayúsculas de las claves ni los
  espacios alrededor del `=`. Las líneas que empiezan por `;`, las vacías,
  los nombres de sección (están para quien lo lee: cada clave tiene un
  nombre distinto), las claves desconocidas y los valores fuera de sus
  límites se saltan (queda el valor de fábrica).
- `Dibujo`: `rombos`, `liso`, `puntos`, `rayas`, `cuadros`, `ladrillos`,
  `olas`, `damero`, `trama` o `zigzag` (o su número, de 0 a 9).
  `ColorFondo` y `ColorDibujo`: de 0 a 15, los colores de la paleta
  (0 tinta, 1 marrón, 2 azul petróleo, 3 petróleo claro, 4 terracota,
  5 terracota claro, 6 arena, 7 arena oscura, 8 crema, 9 blanco, 10 ocre,
  11 verde oliva, 12 añil, 13 granate, 14 turquesa, 15 piedra).
  `Salvapantallas` = 0/1 y `Minutos` = 1-60.
- `Velocidad`: del ratón, en mitades (1 = x0,5; 2 = normal; 6 = x3).
  `DobleClic`: milisegundos como mucho entre los dos clics (165-990; se
  redondea a tics de reloj, de 55 ms). `Zurdo` = 0/1.
- `Espera` (250, 500, 750 o 1000 ms) y `Repeticion` (0-31; 31, la más
  rápida: 30 por segundo) se dan a la BIOS (INT 16h, función 03h);
  `Distribucion` = `SP` o `US`, al residente TECLADO. Las tres **solo están
  si se han tocado**: sin ellas, Hispa Ventanas no cambia nada del teclado.

### Memoria y modo del procesador

El núcleo gráfico es de 32 bits (386 o superior, que es lo que exige HSPDPMI):
un segmento de código de 32 bits y un único segmento de datos y pila, los dos
de 4 GB y con la base del programa. Así un puntero «cercano» llega a toda la
memoria: *puntero = dirección lineal − base* (y el código de un módulo .HVP,
que se carga en memoria extendida, se ejecuta con el mismo segmento). La
superficie de dibujo y el
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
  (módulos .HVP: juegos/*.asm y programas/*.asm, con hvp.inc y hvplib.inc)
  api.inc modulos.inc    la interfaz y el cargador de los módulos
  ajustes.inc    los ajustes que cambia Configuración (CONFIG.HVP)
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
cambio de los botones izquierdo y derecho; `rm_raton` solo apunta el suceso (botones y
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
| `WM_RBUTTONDOWN`, `WM_RBUTTONUP` | se aprieta el botón derecho sobre la zona de la ventana (no la activa) y se suelta. Mientras está apretado, el ratón es de esa ventana (recibe los `WM_MOUSEMOVE` aunque salga de ella), como con el izquierdo | lo mismo |
| `WM_COMMAND` | orden de menú o aviso de un control | menú: ECX=número. Control: ECX=id + aviso×65536, EDX=el control |
| `WM_TIMER` | vence un temporizador | ECX=su número |
| `WM_QUIT` | lo pone `msg_salir`: `msg_tomar` devuelve 0 | |
| `WM_SETTEXT`, `WM_GETTEXT`, `WM_CUT`, `WM_COPY`, `WM_PASTE` | cuadros de texto | ver controles |
| `WM_USER`+n | propios de cada control (`BM_*`, `EM_*`, `LM_*`, `DM_INICIO`) | |

Las teclas que no quiere una ventana hija suben a su madre (así Intro y Esc
llegan al diálogo); Tab lo atiende `ven_defecto` pasando el foco a la hermana
siguiente con `E_TABULA`. Mientras un botón está apretado, el ratón es de la
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
| `dc_mosaico`, `dc_copiar_clave`, `dc_copiar` | como las de `gdi.inc` |

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

Límites de hoy: 160 ventanas (contando controles), 32 de primer nivel, 12
tareas, 16 temporizadores, una copia de cada programa a la vez, 6 módulos
.HVP abiertos.

Un programa nuevo ya no hace falta meterlo en VENTANAS.COM: se hace igual,
pero como módulo .HVP (ver «Programas aparte: los módulos .HVP»).

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
- **Culebra**, **Campo de minas**, **Bloques**, **Solitario**,
  **Tragabolas** y **Almacén**: los juegos, en módulos .HVP (ver «Los
  juegos», más abajo).
- **Configuración** (`programas/config.asm`, `CONFIG.HVP`): los ajustes y
  el salvapantallas (ver «Configuración y VENTANAS.INI»).
- **Pintura**: el programa de dibujo, también un módulo .HVP (ver
  «Pintura», más abajo).

## Programas aparte: los módulos .HVP

`VENTANAS.COM` no puede crecer mucho más (lo que usa el modo real tiene que
caber en 64 KB), así que los programas nuevos van **aparte**, cada uno en su
archivo `.HVP` («programa de Hispa Ventanas»). Los primeros son los juegos,
`CULEBRA.HVP` y `MINAS.HVP` (y la segunda tanda: `BLOQUES.HVP`,
`SOLITARI.HVP`, `TRAGABOL.HVP` y `ALMACEN.HVP`), en `\HISPADOS\JUEGOS`, y
Pintura, `PINTURA.HVP`, en `\HISPADOS\PROGS`.

Un acceso directo cuyo programa acaba en `.HVP` **se abre como una ventana**
(no se sale a Hispa-DOS como con un .COM o un .EXE): Hispa Ventanas carga el
archivo en su memoria y lo pone en marcha como una tarea más, con multitarea
cooperativa, igual que el Bloc de notas o la Calculadora. Al cerrar su
ventana, el programa termina y su memoria se suelta. Si ya está abierto,
abrir otra vez el acceso trae su ventana delante (una copia de cada módulo a
la vez). En el Centro, el acceso sale con el **icono que lleva el módulo**
dentro.

### El formato

Un .HVP es un **binario plano de 32 bits**: una cabecera, el icono, un hueco
para la tabla de la interfaz, el código y los datos; y detrás, la tabla de
reubicaciones.

| Sitio | Tamaño | Qué es |
|---|---|---|
| 0 | 4 | la firma `HVP1` |
| 4 | 2 | versión del formato (1) |
| 6 | 2 | cuántas entradas de la interfaz conoce el módulo |
| 8 | 4 | tamaño del archivo |
| 12 | 4 | memoria que necesita (imagen y variables sin valor inicial) |
| 16 | 4 | punto de entrada (desde el principio del módulo) |
| 20 | 4 | tamaño de la imagen: ahí empiezan las reubicaciones |
| 24 | 4 | cuántas reubicaciones hay |
| 28 | 4 | el grupo del Centro al que va su acceso directo: 0 «Juegos», 1 «Accesorios» |
| 32 | 32 | el nombre del programa, acabado en 0 (página 437) |
| 64 | 512 | el icono: 32×32 a 4 bits por punto, como los de `iconos.inc` |
| 576 | 4 × entradas | la tabla de la interfaz (en el archivo, a cero) |
| … | | código y datos |
| imagen | 4 × reubicaciones | las reubicaciones: el sitio de cada puntero de 32 bits |

El cargador (`hvp_abrir`, en `ventanas/modulos.inc`):

1. lee la cabecera y comprueba la firma y que el módulo no pida más entradas
   de la interfaz de las que hay (si no, avisa: «Este programa necesita un
   Hispa Ventanas más nuevo»);
2. pide al servidor DPMI un bloque de memoria extendida del tamaño que dice
   la cabecera (función 0501h) y lee en él el archivo entero;
3. **reubica**: el módulo está ensamblado para la dirección 0, así que a cada
   puntero de la tabla le suma la dirección donde ha quedado (un puntero
   «cercano», como todos los de Hispa Ventanas: el segmento de código y el de
   datos llegan a los 4 GB con la base del programa);
4. pone a cero lo que no viene en el archivo (las variables de `.bss`) y
   **copia la tabla de la interfaz** en el sitio 576: las direcciones de las
   funciones de Hispa Ventanas que el módulo conoce, en el orden de
   `ventanas/api.inc`;
5. crea la tarea. La entrada del módulo recibe **EBX** = su base (la
   cabecera), **ESI** = su ruta completa (`C:\HISPADOS\JUEGOS\CULEBRA.HVP`),
   **EDI** = los parámetros del acceso directo y **ECX** = las entradas de
   la interfaz que tiene este Hispa Ventanas. Cuando la entrada vuelve
   (`ret`), se destruyen sus ventanas y se suelta la memoria (0502h).

Un módulo corre dentro de Hispa Ventanas, sin protección: como cualquier
programa interno, un error suyo es un error del entorno.

### La interfaz y sus versiones

La interfaz es la lista de `ventanas/api.inc`: una línea por función, **en
orden**, y solo se añade al final. Nunca se quita una entrada, se cambia de
sitio ni cambia lo que hace. Así:

- un módulo ya compilado funciona con cualquier Hispa Ventanas posterior
  (usa las primeras entradas, que siguen siendo las mismas);
- un módulo hecho para una interfaz más nueva que el Hispa Ventanas que hay
  no se carga, y lo dice;
- un módulo que quiera funcionar con varias versiones puede mirar ECX al
  entrar y no usar lo que falte.

El módulo llama con `HV nombre` (`call` por la tabla; `HVJ nombre` para
saltar), con **los mismos registros que dentro de VENTANAS.COM** (ver «La
interfaz de la etapa 3»): EBP = la ventana, EAX = el mensaje o el resultado,
ECX y EDX los parámetros; las funciones conservan los registros salvo EAX
cuando devuelven algo. Las constantes (mensajes `WM_*`, estilos `E_*`, la
ficha `V_*`, las teclas `TC_*`, los colores `C_*`) son las de
`ventanas/defs.inc`. Versión 1 (63 entradas, de la 0 a la 62):

| Entradas | Para qué |
|---|---|
| `ven_crear`, `ven_lista`, `ven_destruir`, `ven_mover`, `ven_mostrar`, `ven_activar`, `ven_enfocar`, `ven_capturar`, `ven_soltar`, `ven_invalidar`, `ven_invalidar_rect`, `ven_texto`, `ven_cli`, `ven_hija_id`, `ven_defecto` | ventanas (la clase y la plantilla están en el módulo; el menú se pone en `V_MENU` y el icono de la ventana minimizada en `V_ICONO`) |
| `msg_tomar`, `msg_mirar`, `msg_despachar`, `msg_poner`, `msg_enviar`, `msg_salir`, `app_bucle` | mensajes; `app_bucle` es el bucle normal de un programa: toma y despacha hasta `WM_QUIT` |
| `tmp_poner`, `tmp_quitar`, `tics` | temporizadores (`WM_TIMER`) y el reloj de la BIOS (18,2 tics por segundo) |
| `mem_pedir`, `mem_soltar` | memoria del montón de Hispa Ventanas |
| `dc_pintar`, `dc_abrir`, `dc_cerrar`, `dc_cortar`, `dc_color` (EAX=color), `dc_negrita` (EAX=0/1), `dc_rect`, `dc_marco`, `dc_linea`, `dc_punto`, `dc_texto`, `dc_texto_n`, `dc_texto_m`, `dc_icono`, `dc_rombo`, `dc_copiar_clave`, `dc_mosaico` | dibujo en la ventana (las macros `COLOR c` y `NEGRITA n` de `hvp.inc` llaman a `dc_color` y `dc_negrita` conservando EAX) |
| `dlg_mensaje`, `dlg_pedir`, `dlg_modal` | diálogos: mensaje, pedir un texto y diálogo a medida |
| `dos_abrir`, `dos_crear`, `dos_cerrar`, `dos_leer`, `dos_escribir`, `dos_puntero`, `dos_borrar` | archivos (para guardar los récords) |
| `azar` (EAX=n → EAX de 0 a n-1), `azar_semilla` (EAX) | números aleatorios |
| `tec_estado` (→ EAX = mayúsculas, Ctrl y Alt: `TM_*`) | teclado (las teclas llegan como `WM_KEYDOWN` y `WM_CHAR`) |
| `pantalla` (→ EAX=ancho, EBX=alto) | tamaño de la pantalla |
| `c_boton`, `c_estatico`, `c_edit`, `c_lista`, `c_dialogo` | (datos, no funciones) las clases de los controles, para las plantillas del módulo: se pone `HV_c_boton` como clase y `hvl_clases` lo cambia por la dirección |

Añadidas para Configuración (66 entradas; los módulos de 63 siguen
funcionando igual):

| Entradas | Para qué |
|---|---|
| `aj_leer` (EAX=número del ajuste → EAX=su valor) | leer un ajuste o un dato del entorno. Los números (`AJ_*`, en `defs.inc`): `AJ_RAT_VEL` (velocidad del ratón en mitades; 2 = normal), `AJ_RAT_DOBLE` (tics de reloj del doble clic; 8), `AJ_RAT_ZURDO` (0/1), `AJ_VOLUMEN` (0-100; -1 si no se sabe), `AJ_SILENCIO` (0/1), `AJ_ULT_ACT` (los tics del último ratón o teclado); solo para leer: `AJ_VID_BITS`, `AJ_VID_TIPO` (1 VESA, 0 VGA), `AJ_HAY_RATON`, `AJ_HAY_SONIDO` y, como punteros a un texto, `AJ_TXT_PANTALLA`, `AJ_TXT_VIDEO`, `AJ_TXT_DPMI` y `AJ_TXT_VERSION`. Con un número que no existe devuelve 0 |
| `aj_poner` (EAX=número, EDX=valor) | cambiar uno de los seis primeros: vale desde ese momento (el volumen y el silencio se le dan a HSPSND y se repinta el altavoz). No comprueba los límites: es cosa de quien llama |
| `esc_fondo` (ESI=mosaico de 16×16: 256 octetos, un color por punto) | cambiar el fondo del escritorio, que se repinta |

Añadidas para Pintura (72 entradas: seis más, de la 66 a la 71):

| Entradas | Para qué |
|---|---|
| `dc_copiar` (EAX=x EBX=y ECX=ancho EDX=alto ESI=mapa) | dibuja en la ventana un mapa de bits de un octeto por punto (índices de la paleta), `ancho` × `alto`, sin color transparente. Con `dc_cortar` antes, de un mapa grande solo se copia lo que cae dentro: así se enseña un trozo de un lienzo desplazado |
| `dlg_archivo` (EBP=dueña EDI=título EBX=búfer de 80 con el nombre → EAX=1 si se acepta) | el diálogo de elegir un archivo (el de Abrir y Guardar como del Bloc de notas) |
| `mem_bloque` (ECX=octetos → EAX=puntero, EDX=mango; CF=1 si no hay), `mem_bloque_soltar` (EDX=mango) | un bloque de memoria extendida propio (función 0501h del DPMI), para lo que no cabe en el montón de 640 KB: el lienzo y los pasos de deshacer. No viene a cero. El módulo lo suelta antes de acabar (al salir de Hispa Ventanas lo suelta el servidor DPMI) |
| `pal_rgb` | (datos) la paleta: 256 colores de cuatro octetos (rojo, verde, azul, 0): los 16 propios, el cubo de 6×6×6 y los 24 grises |
| `fuente8x16` | (datos) la fuente: 256 caracteres de 16 octetos, una fila por octeto, el bit de más peso a la izquierda |

Un módulo compilado con menos entradas en su cabecera (63 o 66) sigue
cargándose igual; uno que pida más de las que tiene un Hispa Ventanas
anterior no se carga y lo dice.

El ratón llega como siempre (`WM_LBUTTONDOWN`, `WM_MOUSEMOVE`…, con las
mayúsculas en ECX) y también el **botón derecho**: `WM_RBUTTONDOWN` y, desde
que existe Pintura, `WM_RBUTTONUP`; mientras está apretado, la ventana recibe los
movimientos del ratón aunque salga de ella (como con el izquierdo), para
poder dibujar arrastrando con cualquiera de los dos botones. Con los
botones intercambiados (`AJ_RAT_ZURDO`), el cambio se hace antes de
repartir: los programas no se enteran.

`ventanas/hvplib.inc` son rutinas que se ensamblan **dentro** del módulo
(no gastan sitio en VENTANAS.COM): `hvl_cad` y `hvl_num` (componer textos),
`hvl_lee_num`, `hvl_opcion` (buscar `/X` en los parámetros), `hvl_ruta` (la
ruta del módulo con otra extensión: el archivo de datos), `hvl_leer` y
`hvl_escribir` (un archivo entero), `hvl_clave` (buscar `Clave=` en un
texto) y `hvl_clases`.

### Cómo se hace un programa

El fuente va en `juegos/` o en `programas/` (o donde se quiera) e incluye
`hvp.inc`. El más corto, completo:

```nasm
%include "hvp.inc"
HVP_CABECERA "Hola", entrada            ; nombre (31 letras) y entrada (y,
                                        ; si va a «Accesorios», un 1 detrás)
        db "................................"   ; el icono: 32 filas de 32
        ...                                     ; («.» transparente, 0-9 a-f
        db "................................"   ; los colores; el 3 no vale)
HVP_CODIGO

c_hola      dd hola_proc
            db C_CREMA, 0, 0, 0
hola_plant:
        PLANTILLA c_hola, t_hola, E_NORMAL, 100, 100, 300, 160, 0
t_hola      db "Hola", 0
t_hola_txt  db "¡Hola desde un módulo!", 0

entrada:                                ; EBX=base ESI=ruta EDI=parámetros
        mov esi, hola_plant
        xor ebp, ebp
        HV ven_crear
        HV app_bucle
        ret                             ; fin del programa

hola_proc:                              ; EBP=ventana EAX=mensaje ECX EDX
        cmp eax, WM_PAINT
        je .pintar
        cmp eax, WM_CREATE
        je .crear
        cmp eax, WM_DESTROY
        je .destruir
        HVJ ven_defecto
.crear:
        mov dword [ebp+V_ICONO], hvp_icono      ; el icono de la cabecera
        jmp .hecho
.destruir:
        HV msg_salir                    ; acaba app_bucle
        jmp .hecho
.pintar:
        HV dc_pintar
        COLOR C_TINTA
        mov eax, 20
        mov ebx, 20
        mov esi, t_hola_txt
        HV dc_texto
        HV dc_cerrar
.hecho:
        xor eax, eax
        ret

%include "hvplib.inc"                   ; (si se usan sus rutinas)
        section .bss                    ; variables sin valor inicial: a cero
veces       resd 1
HVP_FIN
```

Las direcciones son absolutas, como en cualquier programa (`mov esi,
hola_plant`): `tools/mkhvp.py` ensambla el fuente dos veces, con dos
direcciones de carga, y de la diferencia saca la tabla de reubicaciones;
también pasa los textos a la página 437, empaqueta el icono y rellena la
cabecera. En el Makefile es una regla de dos líneas (como `CULEBRA.HVP`) y
el nombre del archivo en `HVPS` (un juego: va a `\HISPADOS\JUEGOS`) o en
`HVPA` (un accesorio, con el 1 en `HVP_CABECERA`: va a `\HISPADOS\PROGS`):
con eso está en el disquete y en la imagen USB, `INSTALAR` lo copia al disco
duro y, la próxima vez que arranque Hispa Ventanas, aparece su acceso en el
grupo «Juegos» o en «Accesorios». Un módulo del sistema va en `HVPSIS` (y en
`HVPSIS` de `tools/mkusb.py`): se copia a `\HISPADOS`, al lado de
VENTANAS.COM, como `CONFIG.HVP`; su acceso directo se pone en
`def_principal` (`config.inc`) o lo añade el usuario («Buscar programas...»
en `C:\HISPADOS` lo encuentra).

A mano:

```
python3 tools/mkhvp.py juegos/hola.asm build/HOLA.HVP
```

Reglas para un módulo: lo largo se parte con un temporizador (no se cede el
turno más que en `msg_tomar`); los textos que se le dan a un diálogo o a una
ventana tienen que seguir existiendo mientras se usan (un búfer para pintar
y otro para los diálogos); los archivos de datos, al lado del módulo
(`hvl_ruta`), y se escriben cuando cambian, no al salir.

### Los juegos

- **Culebra** (`juegos/culebra.asm`, `CULEBRA.HVP`): la culebra anda sola y
  se lleva con las flechas hasta la fruta; cada fruta la alarga dos casillas
  y cada cinco va más deprisa (niveles). Acaba al tocar el borde, un muro o
  su cuerpo. P o Espacio: pausa (también al minimizar). Intro: otra partida.
  Menú Juego: muros (obstáculos) sí o no, tres velocidades y las mejores
  puntuaciones (cinco sin muros y cinco con muros). Se guardan, con la
  velocidad y los muros elegidos, en `CULEBRA.REC` (texto: `Sin=10,0,0,0,0`).
- **Campo de minas** (`juegos/minas.asm`, `MINAS.HVP`): tableros Pequeño
  (9×9, 10 minas), Mediano (16×16, 40), Grande (30×16, 99) y Personalizado
  (hasta 30×24, según la pantalla). La primera losa nunca tiene mina (ni las
  de alrededor, si caben). Clic: levantar; botón derecho, Mayús+clic o la
  tecla B: banderín; doble clic en un número: levantar las de alrededor.
  Teclado: flechas, Intro o Espacio y F2 (otra partida). Arriba, los
  banderines que faltan, el tiempo y un rombo (añil mientras se juega, verde
  con una marca al ganar, granate con un aspa al perder) que con un clic
  empieza otra partida. Los mejores tiempos, el tablero elegido y el
  personalizado se guardan en `MINAS.REC` (texto: `Pequeño=47`).

Los dos aceptan en los parámetros del acceso directo `/S=n` (semilla fija
de los números aleatorios) y `/P` (Culebra: paso a paso, avanza una casilla
con cada flecha; Campo de minas: un tablero de 9×9 con dos minas en dos
esquinas, cuyo tiempo se guarda aparte, como `Prueba=`). Son para las
pruebas automáticas.

**Segunda tanda** (también en `\HISPADOS\JUEGOS`; todos con diseño propio,
menú Juego —partida nueva, récords, salir— y Ayuda, y solo con las 72
entradas de la interfaz que ya había):

- **Bloques** (`juegos/bloques.asm`, `BLOQUES.HVP`): piezas que caen en un
  pozo de 11×18 y se encajan; las filas completas desaparecen. Once piezas
  propias: dos de tres casillas (Barrita, Codo), siete de cuatro (Losa,
  Viga, Gancho, Garfio, Peldaño, Escalón, Pico) y dos de cinco (Cruz, Cazo),
  dibujadas como azulejos con un rombo; un recuadro marca dónde caería la
  pieza. Puntos: uno por casilla encajada y por fila bajada al soltar de
  golpe; 20, 50, 90 o 140 por quitar 1, 2, 3 o 4 filas a la vez, por el
  nivel. Sube un nivel cada 8 filas (hasta el 12) y cae más deprisa; desde
  el nivel 5, cada 10 piezas sube del fondo una **fila de piedra** con un
  hueco. Flechas (mover, arriba girar, abajo bajar), Espacio suelta, P
  pausa (y sola al minimizar), F2 otra partida; clic gira y el botón
  derecho suelta. Menú: empezar en el nivel 1, 4 o 7. Las cinco mejores
  puntuaciones y el nivel inicial, en `BLOQUES.REC` (`Puntos=194,0,0,0,0`).
- **Solitario** (`juegos/solitario.asm`, `SOLITARI.HVP`): un solitario de
  columnas de la familia del «Klondike» con **baraja española de 40
  cartas** dibujada aquí (oros, copas, espadas y bastos; as, 2 a 7, sota
  10, caballo 11 y rey 12), sobre tapete azul petróleo. Seis columnas de 1
  a 6 cartas, tapadas menos la de encima; las otras 19 son el mazo, que se
  pasa de una en una y se le da la vuelta las veces que se quiera. En las
  columnas se baja de una en una (rey, caballo, sota, 7… as) alternando
  palos claros (oros y copas) y oscuros (espadas y bastos); a un hueco solo
  va un rey. Arriba, cuatro montones de salida, uno por palo, del as al
  rey: se gana con las 40. Ratón: arrastrar una carta o una escalera (si la
  jugada no vale, vuelve a su sitio), clic en el mazo, doble clic o botón
  derecho para subir una carta. Teclado: F2 nueva, Retroceso o Ctrl+Z
  deshacer (512 pasos), Espacio sacar carta, S subir las que se puedan. A
  la vista, movimientos, tiempo (corre desde el primer movimiento; no,
  minimizada) y cartas del mazo. `SOLITARI.REC`: `Jugadas=`, `Ganadas=`,
  `Tiempo=` (el mejor, en segundos) y `Movimientos=` (los menos).
- **Tragabolas** (`juegos/tragabolas.asm`, `TRAGABOL.HVP`): el Tragón, una
  bola azul petróleo, recoge las bolitas de un jardín de setos (25×17
  casillas) esquivando a tres erizos: el granate va a por él, el añil anda
  al azar y el marrón le sale al paso. Tres jardines propios que se turnan;
  cada vuelta va más deprisa. Cuatro **campanas** por jardín: al coger una,
  los erizos se duermen un rato (grises) y no pinchan. Tres vidas. Puntos:
  10 por bolita, 50 por campana y 100 × el jardín al dejarlo limpio.
  Flechas (la pedida se recuerda hasta que se puede girar), P o Espacio
  pausa (y sola al minimizar), F2 partida nueva. Las cinco mejores
  puntuaciones, en `TRAGABOL.REC` (`Puntos=40,0,0,0,0`).
- **Almacén** (`juegos/almacen.asm`, `ALMACEN.HVP`): el mozo empuja las
  cajas (de una en una; no puede tirar) hasta las marcas del suelo, entre
  estanterías. **24 niveles propios** de hasta 16×11 casillas, de 1 a 6
  cajas; se puede ir al primero, a los ya resueltos y al siguiente de uno
  resuelto. Flechas; Retroceso, U o Ctrl+Z deshacen (todo el nivel); R o F2
  empiezan el nivel otra vez; Av Pág y Re Pág cambian de nivel; un clic en
  la fila o la columna del mozo da un paso hacia allí. Arriba, nivel,
  movimientos, empujones y la mejor marca. `ALMACEN.REC`: `Nivel=` (donde
  se quedó), `Mejor=` y `Empujones=` (24 valores; 0, sin resolver).
  `tools/almacen_niveles.py` lee los niveles del fuente, comprueba que
  están bien formados y **resuelve cada uno** (unos 3,5 minutos los 24;
  `-n N` uno solo, `-s` escribe la solución).

Parámetros de prueba: Bloques, `/S=n` y `/P` (la pieza no cae sola);
Solitario, `/S=n` (reparto fijo) y `/P` (partida casi acabada: faltan
cuatro cartas); Tragabolas, `/S=n`, `/P` (todo avanza un paso con cada
flecha) y `/N=n` (jardín inicial); Almacén, `/N=n` (empieza en el nivel n,
con todos abiertos).

### Pintura

`programas/pintura.asm`, `PINTURA.HVP` (19 KB), en `\HISPADOS\PROGS`; su
acceso sale en «Accesorios». Es el programa de dibujo, con diseño propio:

- **Arriba, la barra de herramientas**: lápiz, pincel, goma, línea,
  rectángulo, elipse, relleno, cuentagotas, texto y selección (teclas L, P,
  G, N, R, E, B, C, T y S); al lado, los cuatro **grosores** (1, 2, 4 y 8
  puntos; teclas + y -) y las tres **formas** del rectángulo y la elipse:
  contorno, lleno y contorno con fondo.
- **A la derecha, los colores**: dos rombos, el primario delante y el
  secundario detrás (un clic en ellos los intercambia), y la paleta: los 16
  colores de Hispa Ventanas y 40 más. El botón izquierdo elige el primario y
  el derecho el secundario. Colores › Todos los colores abre una ventana con
  los 256.
- **El botón izquierdo dibuja con el primario y el derecho con el
  secundario** (en las formas con fondo, el otro color es el fondo). La goma
  pone el secundario; lo que deja una selección al moverla o borrarla,
  también. El cuentagotas toma el color del lienzo para el botón con que se
  pulsa. El texto se pide en un diálogo y se escribe con la fuente de Hispa
  Ventanas (al doble y al triple con los grosores 4 y 8).
- **Selección**: se marca un recuadro; arrastrándolo se mueve; Ctrl+X,
  Ctrl+C y Ctrl+V cortan, copian y pegan (lo pegado queda flotando para
  colocarlo); Supr borra; Esc, otra herramienta o un clic fuera lo dejan
  donde está; Ctrl+A lo selecciona todo. El portapapeles de imágenes es de
  Pintura (el de Hispa Ventanas solo lleva texto).
- **Deshacer y rehacer** (Ctrl+Z, Ctrl+Y): hasta 8 pasos; menos si el lienzo
  es grande (cada paso guarda el lienzo entero y no se pasa de 2,5 MB: con
  640×480 son 6 y con 1024×768, 1).
- **El lienzo**: de 8×8 a 1024×768 puntos (480×320 al empezar; Archivo ›
  Nuevo pide el tamaño), un octeto por punto con los 256 colores de Hispa
  Ventanas. Si no cabe en la ventana salen **barras de desplazamiento**
  propias (se arrastra el tirador o se pulsa en la pista; también las
  flechas, Re Pág y Av Pág).
- **Archivo**: Nuevo, Abrir, Guardar (Ctrl+G) y Guardar como, en **BMP**. Lee
  los BMP de 1, 4, 8 y 24 bits sin compresión (también con la primera fila
  arriba); cada color pasa al más parecido de la paleta: uno de los 16
  propios si es exacto y, si no, el del cubo o el gris más cercano. Guarda
  en el modo de color de Hispa Ventanas: BMP de 8 bits con su paleta de 256
  colores (lo que se guarda y se vuelve a abrir queda igual). Sin extensión,
  guarda como `.BMP`. Pregunta antes de perder cambios (al cerrar, con
  Nuevo y con Abrir).
- En los **parámetros** del acceso directo se puede poner un BMP, que se
  abre al entrar.
- Abajo, la **barra de estado**: la herramienta, el tamaño del lienzo y el
  punto donde está el ratón.

Memoria: un bloque de memoria extendida (`mem_bloque`) con el lienzo, la
zona flotante (lo que se mueve o se pega; también es la pila del relleno) y
los pasos de deshacer; otro, del tamaño justo, para lo copiado. El relleno
va por tramos de fila, sin recursión.

Pendiente: ampliar la vista (lupa), voltear y girar, líneas a 45° con
Mayús, elegir la fuente del texto y avisar de los cambios sin guardar al
salir con el botón «Salir» de la barra de arriba (como el Bloc de notas).

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
- `tests/run_juegos_tests.sh`: los módulos .HVP y los dos juegos. Primera
  sesión, sin CENTRO.INI: el grupo «Juegos» sale con un acceso por módulo
  (nombre e icono propios), cada juego se abre como ventana, se dibuja, la
  culebra anda y se para con P, se cierran, se vuelve a abrir y al salir la
  memoria extendida queda como al entrar. Segunda sesión, con los
  parámetros de prueba: Culebra paso a paso con la semilla fija llega a la
  fruta, choca con el borde y deja el récord en `CULEBRA.REC` (se mira con
  mtools); Campo de minas abre la primera losa, pone banderines con el botón
  derecho y con Mayús+clic, gana el tablero de prueba y deja el tiempo en
  `MINAS.REC`; «Buscar programas...» añade los .HVP sueltos y no el archivo
  que no es un módulo; el módulo que pide una interfaz más nueva no se
  carga; dos juegos abiertos a la vez y salir con ellos abiertos. Capturas
  de muestra en `build/juegos_*.png`.
- `tests/run_juegos2_tests.sh`: la segunda tanda de juegos, por partes
  (`tests/juegos2/*.sh`; `SOLO="bloques almacen"` pasa solo algunas).
  «grupo»: sin CENTRO.INI, el grupo «Juegos» trae los seis accesos en dos
  filas (la ventana se alarga sola), cada juego nuevo se abre y se cierra, y
  caben en el disquete. Después, una sesión por juego con sus parámetros de
  prueba: Bloques completa una fila, llena el pozo y deja el récord;
  Solitario pasa cartas, arrastra (también una jugada que no vale),
  deshace, gana la partida de prueba y deja `SOLITARI.REC`; Tragabolas
  recoge bolitas, lo para un seto, lo pinchan hasta acabar, coge una campana
  y deja el récord; Almacén empuja, deshace, resuelve el nivel 1, pasa al 2
  y lo recuerda al volver a abrir. Los récords se miran con mtools.
  Capturas de muestra en `build/juegos2_*.png`.
- `tests/run_configuracion_tests.sh`: Configuración y VENTANAS.INI. Primera
  sesión, sin VENTANAS.INI: el acceso directo en «Principal», la ventana y
  sus apartados; un tema, un dibujo y un color cambian el escritorio al
  momento y se guardan solos; salvapantallas con «Probar» (y la tecla que
  lo quita no llega a la ventana de debajo); doble clic más largo y zona
  de prueba; fecha y hora cambiadas (y, al salir, FECHA y HORA de
  Hispa-DOS dan las nuevas, también tras reiniciar el equipo); Sistema;
  teclado estadounidense y español con la tecla de la ñ; ocultar y volver
  a enseñar; al salir, VENTANAS.INI se mira con mtools y la memoria
  extendida queda como al entrar. Segunda sesión, con el mismo disco: el
  fondo guardado sale al arrancar y el doble clic lento vale. Tercera, con
  un VENTANAS.INI hecho a mano (mayúsculas, espacios, una clave
  desconocida, un valor imposible): fondo, ratón el doble de rápido,
  botones intercambiados y teclado estadounidense al arrancar, y «De
  fábrica». Cuarta: en VGA de 640×480 y 16 colores. Capturas de muestra en
  `build/config_*.png`. (`tests/run_config_tests.sh` es la de CONFIG.SYS.)
- `tests/run_pintura_tests.sh`: Pintura. Primera sesión, sin CENTRO.INI: su
  acceso sale en «Accesorios»; se abre, se dibuja con el ratón con cada
  herramienta y con los dos botones, se eligen colores, grosores y formas,
  se rellena, se deshace y se rehace, se escribe un texto, se mueve, se
  copia y se pega una selección; se guarda y el BMP del disco se saca con
  mtools y se mira octeto a octeto (`tests/tools/pintura_bmp.py`: cabecera,
  paleta y el color de cada punto dibujado); cerrar con cambios pregunta;
  salir con Pintura abierta deja la memoria como al entrar. Segunda sesión:
  abre BMP de 1, 4, 8 y 24 bits hechos por la prueba (el nombre va en los
  parámetros del acceso) y comprueba los colores en la pantalla; guarda el
  de 24 bits y compara todos sus puntos; un archivo que no es un BMP, uno
  comprimido, uno demasiado grande y uno que no existe; lienzo nuevo de otro
  tamaño; y una imagen que no cabe: barras, flechas, Av Pág, arrastrar el
  tirador, dibujar con el lienzo desplazado y guardar. Capturas de muestra
  en `build/pintura_*.png`.
- Programas de prueba de Windows 16 bits propios (NASM + nuestra
  herramienta de cabeceras NE) para cada función de la API, con su
  resultado escrito en un archivo (etapa 4).
- Las aplicaciones de terceros (Windows 3.11, Word, Excel) solo en local, con
  las copias de cada uno; nunca en el repositorio.
