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
   (`tests/run_pintura_tests.sh`). Los programas de oficina (nombres de
   Carlos) van en su propio grupo, **«Productividad»** (el 4 de
   `HVP_CABECERA`, desde la 0.9.18): **Pluma-Ventanas**, el procesador de
   textos (letra con formato; `tests/run_pluma_tests.sh`), **Cuentas-Ventanas**,
   la hoja de cálculo (fórmulas con SUMA, PROMEDIO, MAX, MIN, CONTAR, REDONDEAR,
   ABS y %; el CSV entrecomilla las celdas que llevan «;»;
   `tests/run_cuentas_tests.sh`), y **Facturas**, la economía de una casa y de
   un pequeño negocio (emitidas y recibidas, líneas, IVA y retención, resumen
   por trimestres; `tests/run_facturas_tests.sh`). El Administrador de archivos ya está
   ([ARCHIVOS], `programas/archivos.asm`, ver «Administrador de archivos»). La
   segunda tanda de juegos (Bloques, Solitario, Tragabolas y Almacén) ya
   está (`tests/run_juegos2_tests.sh`). Pendiente (se
   hará según haga falta en las etapas 4 y 5): avisar de los
   textos sin guardar al salir con el botón «Salir»; órdenes
   de menú desactivadas o con marca; tabuladores y ajuste de línea en el
   cuadro de texto; varias copias del mismo programa a la vez; elegir el
   «solo el contorno» (`AJ_CONTORNO`) desde Configuración. Hecho después
   ([TAMAÑO], «Cambiar el tamaño y desplazarse»): bordes que se agarran con
   holgura, cursor que cambia de forma, contorno en los equipos lentos,
   barras de desplazamiento vertical y horizontal como control (`c_barra`) y
   rueda del ratón.
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

Hispa Ventanas es `VENTANAS.COM` (unos 70 KB) y los programas aparte que
carga (módulos .HVP, más abajo), todo hecho en NASM. Arranca en modo real, comprueba lo que necesita, elige el modo
de vídeo y entra en modo protegido como **cliente DPMI de 32 bits**. Los
programas de siempre (Centro, Bloc de notas…) van dentro, cada uno en su
archivo y como una tarea; los nuevos (los juegos y Pintura) van aparte, en
archivos .HVP que se cargan como una tarea más.

`VENTANAS.COM` se ejecuta dos veces: la primera copia es el **lanzador**
(unos 2,5 KB con su entorno; si hay memoria superior libre, se muda a ella
y no gasta nada de la convencional) y ejecuta la segunda, que es el
entorno gráfico (ver «Programas de Hispa-DOS desde Hispa Ventanas»).

Lo que usa la parte de modo real (el código de arranque, el búfer de paso
de DOS, las colas del ratón y del teclado) tiene que estar en los primeros
64 KB del programa, y un .COM de verdad no pasa de 64 KB. Por eso
`VENTANAS.COM` **tiene formato .EXE** (cabecera «MZ») aunque se llame .COM:
el núcleo mira la cabecera, no el nombre, y carga la imagen entera, mida lo
que mida. La cabecera (32 octetos, la escribe el propio `ventanas.asm`) no
lleva reubicaciones y pide CS = SS = PSP (FFF0h: el segmento de carga menos
10h) e IP = 100h: el programa arranca exactamente como un .COM (ORG 100h,
DS = ES = CS = PSP). Un .EXE así el núcleo lo carga donde cargaría un .COM,
sin el relleno hasta 1000h que pone a los demás .EXE (el lanzador se queda
residente: ese relleno sería memoria perdida para los juegos).

El archivo tiene dos partes:

- la **parte baja**, hasta los 64 KB: todo el código de 16 bits, el código
  de 32 bits de siempre, los textos y las variables que usa el modo real
  (`bss.inc`, que van en el archivo como ceros). Si se sale de los 64 KB,
  la compilación falla (`-w+error=number-overflow`). Queda poco más de 1 KB
  (1.268 octetos, medido el 18/11/2026): lo nuevo va en la parte alta;
- la **parte alta** (`alto_ini`, detrás de la baja): los iconos, la fuente
  y el código de 32 bits que no cabe abajo (`accesos.inc`). El modo
  protegido llega a ella con punteros de 32 bits; desde el código de
  16 bits no se puede nombrar nada de ahí (da el mismo error al compilar).
  No tiene límite: lo nuevo que solo sea de 32 bits va ahí.

Detrás del archivo, en memoria, siguen la pila y las variables que solo usa
el modo protegido. Los iconos se guardan a 4 bits por punto
(`tools/icopack.py` los empaqueta al compilar; en los fuentes siguen en
texto).

### Cómo se arranca y se usa

```
DEVICE=A:\HISPADOS\HSPXMS.SYS        (en CONFIG.SYS)
KEYB SP                              (en AUTOEXEC.BAT, para el teclado español)
VENTANAS [/1024 | /800 | /640] [/8 | /15 | /16 | /24 | /32 | /COLOR] [/VGA] [/DEMO]
         [/CONTORNO] [/486]
```

- Hace falta HSPXMS.SYS (si no está, lo dice y termina).
- Si no hay servidor DPMI, carga HSPDPMI.COM de su misma carpeta y al salir
  lo quita de la memoria (si ya estaba cargado, lo deja); si no lo
  encuentra, pide que se ejecute antes.
- Si no hay driver de ratón (INT 33h), carga HSPRATON.COM de su misma carpeta
  (queda residente); sin ratón funciona igual con el teclado.
- Sin opciones: VESA 800×600 de 256 colores; si la tarjeta no tiene VESA 2.0
  con marco lineal, VGA de 640×480 y 16 colores.
- `/486` pone el **escritorio de Hispa-486**: la barra de tareas abajo y el
  menú Inicio, sin la barra de arriba y con el Centro de programas cerrado
  (ver «El escritorio de Hispa-486»; también con `Escritorio=486` en
  VENTANAS.INI). Sin ella, todo lo que sigue es el escritorio de siempre.
- Al entrar se abre el **Centro de programas** con sus grupos (Principal,
  Accesorios, Juegos): doble clic en un icono (o flechas e Intro) abre el
  grupo en su ventana y, dentro, el programa. `/DEMO` pone solo las ventanas
  de muestra de la etapa 2.
- El altavoz de la barra de arriba abre el volumen; a su derecha, la hora
  (HH:MM) y, en los portátiles, un indicador de batería (APM) con el nivel y
  si está cargando.
- Ventanas: se arrastran por la barra de título; los botones de la derecha
  minimizan (a un icono abajo; un clic lo recupera), maximizan o restauran
  (también doble clic en el título) y cierran; se cambia el tamaño tirando de
  un borde o de una esquina (ver «Cambiar el tamaño y desplazarse»).
- Teclado: Alt+F4 cierra la ventana activa, Ctrl+Tab (o Alt+Tab) pasa a otra,
  F10 o Alt+letra abre un menú (flechas, Intro, Esc), Tab y Mayús+Tab pasan
  de un control a otro, Intro y Esc aceptan y cancelan un diálogo. En los
  cuadros de texto: flechas, Inicio, Fin, Re Pág, Av Pág, Ctrl+Inicio,
  Ctrl+Fin, Mayús con cualquiera de ellas para seleccionar, Ctrl+C / Ctrl+X /
  Ctrl+V (o Ctrl+Ins, Mayús+Supr, Mayús+Ins) y Ctrl+A.
- Para salir: el botón «Salir» de la barra de arriba, Alt+F4 en el
  escritorio (sin ninguna ventana activa), cerrar el Centro de programas o su
  «Archivo > Salir a Hispa-DOS». Todos abren el mismo cuadro, «¿Qué quiere
  hacer?», con tres botones grandes con su dibujo: **Volver a Hispa-DOS**
  (Intro), **Reiniciar el equipo** y **Apagar el equipo**, y Cancelar (Esc);
  las flechas pasan de uno a otro (`ventanas/apagado.inc`). Los grupos
  abiertos y el sitio de cada ventana se guardan en CENTRO.INI y la próxima
  vez salen igual; VENTANAS.INI ya lo escribe Configuración cuando cambia.
  Reiniciar y apagar van como un juego (ver «Programas de Hispa-DOS desde
  Hispa Ventanas»): Hispa Ventanas guarda CENTRO.INI, deja al lanzador el
  encargo de ejecutar `REINICIA.COM` o `APAGAR.COM` de su carpeta, sin
  «volver», vuelve a modo texto y termina; el lanzador quita HSPDPMI de la
  memoria y ejecuta el programa, que escribe en el disco lo pendiente y
  reinicia o apaga (si el Bloc de notas tiene cambios sin guardar, pregunta
  antes). Si falta el programa, lo dice y no sale.
- Al salir deja el modo de texto, el ratón y la memoria como estaban. Mientras
  está el entorno gráfico, un error crítico de disco (INT 24h) hace fallar la
  llamada sin preguntar en la pantalla; no cambia ningún otro vector de
  interrupción. Si el programa falla estando en gráficos, vuelve antes a modo
  texto para que se lea el mensaje de HSPDPMI.

### Cambiar el tamaño y desplazarse

[TAMAÑO] (`ventanas/tamano.inc`, en la parte alta). Pedido de Carlos,
probándolo en un ThinkPad con touchpad.

- **Bordes y esquinas.** Las ventanas con `E_TAMANO` (las de los grupos y el
  Centro, el Bloc de notas, el Reloj, Pintura, Hispa-Nes, Hispa-System, el
  Solitario; los diálogos no)
  se agarran por cualquiera de los cuatro bordes y las cuatro esquinas, con
  holgura para un touchpad: 5 puntos por dentro del borde y 4 por fuera
  (`AGARRE`, `AGARRE_FUERA`); a menos de 20 puntos de una esquina
  (`AGARRE_ESQ`) se tira en diagonal. Una ventana maximizada no se agarra.
- **El cursor cambia de forma** encima de un borde: doble flecha ↔ en los
  lados, ↕ arriba y abajo y en diagonal en las esquinas (dibujos propios de
  16×16, en `tamano.inc`; la punta es el centro). Un programa puede pedir
  una forma para su zona con `cur_forma`.
- **Tamaño mínimo.** Al empezar a tirar se pregunta a la ventana
  `WM_GETMINMAXINFO`: devuelve en EAX el ancho mínimo + el alto mínimo×65536
  (0: los normales, 140×70). Los grupos piden 150×110 (una columna y la
  barra), el Bloc 220×130, Pintura 320×260 y el Solitario 444×415 (su
  escala pequeña).
- **El programa se entera**: `WM_SIZE` en cada paso (EDX=ancho + alto×65536
  de la zona), para recolocar lo de dentro, y, al soltar,
  `WM_EXITSIZEMOVE` (ECX=1 si ha cambiado el tamaño, 0 si solo se movió).
- **Solo el contorno.** En un equipo lento repintar a cada paso parpadea y
  se queda atrás: entonces, al mover o cambiar el tamaño, solo se dibuja un
  contorno (tres filetes, tinta, ocre y tinta, puestos en la superficie
  guardando lo que tapan) y la ventana cambia una vez, al soltar. Lo decide
  `AJ_CONTORNO` (`aj_leer`/`aj_poner`): 0 automático (lo normal: la primera
  vez que se arrastra se mide cuántas veces se lleva la pantalla entera en
  dos tics de reloj; menos de 4, es lenta), 1 siempre el contorno (también
  `VENTANAS /CONTORNO`, que el lanzador conserva) y 2 nunca.
- **Barras de desplazamiento** (`c_barra`, vertical o, con `BDS_HORIZ`,
  horizontal): flechas en las puntas, pulgar proporcional (lo que se ve
  respecto a todo) que se arrastra, clic en la pista para avanzar una página,
  flechas y pista que repiten mientras se tienen apretadas y la rueda del
  ratón encima. Avisan a su madre con `WM_VSCROLL` / `WM_HSCROLL` (ECX=la
  posición nueva, EDX=la barra). Ver «Controles».
  - En las ventanas de los **grupos** y en el **Centro**, cuando los iconos no
    caben sale la barra vertical a la derecha (y se quita cuando vuelven a
    caber); los iconos se colocan en las columnas que quedan. Se desplaza
    con la barra, con la rueda (media fila por paso), y con el teclado:
    flechas, **Re Pág / Av Pág** (una página de filas), **Inicio** y **Fin**
    (el primero y el último); lo marcado siempre queda a la vista.
  - El **Bloc de notas** tiene, además de la vertical del cuadro de texto,
    una horizontal debajo para las líneas largas.
  - Las listas (`c_lista`) y el cuadro de texto de varias líneas se
    desplazan también con la rueda (tres filas por paso).
- **La rueda del ratón** llega como `WM_MOUSEWHEEL` (ECX=pasos, positivo
  hacia abajo; EDX=x,y en la zona) a la ventana que hay debajo del cursor;
  la que no la quiere la pasa a su madre (`ven_defecto`). Hispa Ventanas la
  pide con la interfaz de CuteMouse (el bit 7 de la máscara de sucesos, con
  los pasos en BH; [RUEDA] siempre, porque de un ratón USB solo se sabe que
  tiene rueda cuando se mueve: a un driver sin rueda ese bit no le dice
  nada). La da HSPRATON (ver su cabecera) con un ratón IntelliMouse (PS/2
  con rueda, por la vía directa), con un ratón USB con rueda (por HSPUSB:
  [USB.md](USB.md)) y con el borde derecho de un touchpad Synaptics (los
  netbooks: modo absoluto; `/SINBORDE` lo quita); la dan también otros
  drivers como CuteMouse. Un touchpad sin nada de esto se desplaza con la
  barra o el teclado.
- [RUEDA] A los **programas de Windows** (Hispa-486) les llega como el
  `WM_MOUSEWHEEL` de Windows (docs/486/HISPA-486.md); el **Navegador**
  desplaza la página tres filas por paso y, con una lista desplegable
  abierta, sus opciones. El Administrador de archivos (árbol y lista) y el
  Centro de programas ya la usaban; el menú Inicio de Hispa-486 no la
  necesita (no se desplaza: lo que no cabe va en columnas y «Más...»).
  Prueba: `tests/run_rueda_tests.sh` (con el ratón PS/2 y con el USB).
- **Se recuerda.** El sitio y el tamaño de las ventanas de los grupos y del
  Centro van a CENTRO.INI (`Ventana=`) al cerrarlas y al salir. Un grupo al
  que el usuario ha dado el tamaño con el ratón lleva `Medida=1` y ya no se
  alarga solo al abrirse cuando no caben sus iconos (sale la barra).

### El Centro de programas: grupos y accesos directos

La ventana del Centro tiene un icono por **grupo**. Cada grupo se abre en su
propia ventana (se mueve, se cambia de tamaño, se minimiza) con un icono por
**acceso directo**. Por omisión hay tres grupos: «Principal» (los programas
de Hispa-DOS que haya en la carpeta de VENTANAS.COM: Editar, Gestor,
Información, el intérprete de órdenes y, el último, Configuración, que es un
módulo .HVP que está en `\HISPADOS`: ver «Configuración y VENTANAS.INI»), «Accesorios» (los programas
internos: Bloc de notas, Calculadora, Reloj y Sistema, y Pintura, que es un
módulo .HVP) y «Juegos» (los juegos propios). [MEZCLADOR] El grupo **«Sonido»**
(el Reproductor de sonido y el Mezclador, módulos .HVP con el grupo 2 en la
cabecera) se crea solo la primera vez que aparecen; en un CENTRO.INI de antes,
el Reproductor se queda en «Accesorios» y el Mezclador va a «Sonido». Los módulos .HVP están en dos
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

[TAREAS] El **Administrador de tareas** (`TAREAS.HVP`) sale en
**«Principal»** (el 3 de su cabecera), con Configuración y el Administrador
de archivos: es una herramienta del sistema, no un accesorio. Pedido de
Carlos: «Administrador de tareas en Principal y en botón secundario en el
escritorio». Un módulo tiene un solo acceso (al buscar módulos, el que ya
tiene acceso en cualquier grupo no recibe otro), y desde la 0.9.22
«Principal» no repite iconos (`SinRepetidos`), así que no está en los dos
grupos: en la 0.9.22 salía en «Accesorios», y ahora, **una sola vez** (la
clave `TareasPrincipal=1` de `[Centro]` lo recuerda; `pr_tareas` en
`config.inc`), el acceso que esté en «Accesorios» pasa a «Principal» al
arrancar. Si estaba en otro grupo, se queda donde está; y si después
alguien lo lleva a otro grupo («Mover a otro grupo...»), también. La regla
de `SinRepetidos` no lo toca: solo quita de «Principal» EDITAR.COM,
GESTOR.COM e INFO.COM. Además se abre con **Ctrl+Alt+Supr** y con el botón
derecho en el fondo del escritorio (ver «Botón derecho»).

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
| Carpeta de juegos... | liga el grupo marcado a una carpeta | liga este grupo |
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
carpeta y, si falta, el nombre. Debajo están las **opciones para juegos de
DOS** (velocidad, LOADFIX y sonido: ver «Opciones de juego»).

**Buscar programas...** pide una carpeta (por omisión `\JUEGOS` de la unidad
de Hispa Ventanas) y crea un acceso directo, con el icono de juego y el
nombre de la carpeta, al programa principal de cada una de sus subcarpetas
(hasta 64). La regla, por este orden:

1. el .EXE, .COM o .BAT que se llama como la subcarpeta (`DOOM\DOOM.EXE`);
2. si no, el único .EXE; si no hay ninguno, el único .COM; si tampoco, el
   único .BAT. No cuentan los que empiezan por SETUP, INSTALL, INSTALAR,
   CONFIG, SETSOUND, SNDSETUP, UNINSTAL, README, LEEME, DOS4GW, CWSDPMI,
   DOS32A, PMODEW o UNIVBE (salvo que la subcarpeta no tenga más que un
   programa y sea uno de esos: entonces, ese);
3. si quedan varios de la misma clase, pregunta con una lista («Cancelar»
   salta esa carpeta).

Además añade un acceso por cada programa de Hispa Ventanas (.HVP) que haya
en la carpeta misma, con el nombre que lleva el módulo (un archivo que se
llame .HVP sin serlo no cuenta).

No repite un programa que ya tenga acceso directo, y al final dice cuántos
ha añadido.

#### Grupos ligados a una carpeta de juegos

Un grupo se puede **ligar a una carpeta** (`C:\JUEGOS`): Hispa Ventanas le
pone solo un acceso directo por cada juego que haya en ella, sin tener que
buscar nada. Se hace con **Archivo > Carpeta de juegos...**, que pide la
carpeta (propone `\JUEGOS` del disco duro en que está Hispa Ventanas, o
`C:\JUEGOS`; «Examinar...» abre el diálogo de archivos en modo de elegir
carpeta: se entra en ella con doble clic y se acepta). Con la carpeta en
blanco el grupo deja de estar ligado (sus accesos se quedan).

La primera vez que Hispa Ventanas encuentra esa carpeta de juegos (`\JUEGOS`
de su unidad si es un disco duro, o `C:\JUEGOS`) crea él mismo el grupo
**«Juegos de DOS»** ligado a ella, también en un CENTRO.INI de antes. Solo
una vez (`JuegosDOS=1` en CENTRO.INI): si el grupo se borra o se le quita la
carpeta, no vuelve.

**Al arrancar** (también al volver de un juego) **y al abrir el grupo**, el
grupo se pone al día con su carpeta:

- cada **subcarpeta** sin acceso directo recibe uno a su programa principal,
  con las reglas de «Buscar programas...» pero sin preguntar: si quedan
  varios programas de la misma clase, se elige el de mayor tamaño (se puede
  cambiar luego en «Propiedades»). Si ya hay un acceso, en cualquier grupo,
  a algún programa de esa subcarpeta, el juego se da por puesto;
- cada **programa suelto** (.EXE, .COM o .BAT de la carpeta misma, sin
  contar los de instalar y configurar) recibe también el suyo;
- los accesos cuyo programa **ya no está** se quitan... si los puso Hispa
  Ventanas y nadie los ha tocado. Uno que se cambió en «Propiedades» (por
  ejemplo, se le puso la velocidad) no se quita: se queda con un **aspa**
  granate sobre el icono, y la pierde si el programa vuelve. Los accesos
  creados a mano no se miran;
- un acceso de la carpeta que se **borra a mano** no vuelve a salir mientras
  su juego siga ahí (se guarda oculto); aceptar otra vez «Carpeta de
  juegos...» en el grupo los recupera todos;
- si la carpeta no se puede leer (un disco que no está puesto), no se toca
  nada.

Hasta 64 subcarpetas y 32 programas sueltos por carpeta.

#### Opciones de juego

En las propiedades de un acceso directo a un programa de Hispa-DOS:

- **Velocidad**: Normal, Pentium, 486, 386, 286, XT u «Otra» (un tanto por
  ciento de la velocidad del equipo, del 1 al 100). Si no es «Normal», el
  programa se abre con `VELOCIDAD nivel programa`, que frena el equipo solo
  mientras corre (ver «Juegos antiguos en equipos rápidos» en el README).
- **LOADFIX**: lo abre con `LOADFIX`, por encima de los primeros 64 KB de la
  memoria (para los «Packed file corrupt»).
- **Sonido: Sound Blaster emulada**: antes del programa se carga
  `HSPSND /SB` (docs/HSPSB.md) si no hay ya un HSPSND en la memoria, y queda
  cargado. Solo se puede marcar si HSPEMM está cargado (hace falta para
  emular la tarjeta); si no, la casilla sale en gris y lo dice.

Cómo se lanza, en «Programas de Hispa-DOS desde Hispa Ventanas».

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
SinRepetidos=1
TareasPrincipal=1

[Grupo]
Nombre=Juegos
Ventana=248,188,452,190
Estado=1
Marcado=0
Medida=1

[Acceso]
Nombre=Doom
Icono=juego
Programa=C:\JUEGOS\DOOM\DOOM.EXE
Carpeta=C:\JUEGOS\DOOM
Parametros=
Volver=1
Pausa=0
```

`Medida=1` ([TAMAÑO]) solo se escribe si el usuario ha cambiado el tamaño
del grupo con el ratón: entonces no se alarga solo al abrirse aunque no
quepan sus iconos.

Un grupo ligado a una carpeta de juegos y un acceso con opciones de juego
llevan además estas claves (**solo se escriben cuando hacen falta**: un
CENTRO.INI sin juegos queda como siempre, y los de antes siguen valiendo
porque las claves que faltan valen lo normal):

```
[Centro]
JuegosDOS=1

[Grupo]
Nombre=Juegos de DOS
Carpeta=C:\JUEGOS

[Acceso]
Nombre=Doom
Programa=C:\JUEGOS\DOOM\DOOM.EXE
Velocidad=486
Loadfix=1
Sonido=sb
Auto=1
Cambiado=1
```

- Las líneas son `clave=valor`; no importan las mayúsculas de las claves ni
  de los nombres de sección, ni los espacios alrededor. Las líneas que
  empiezan por `;`, las vacías y las claves desconocidas se saltan.
- `[Centro]`: `Ventana` = x, y, ancho y alto de la ventana del Centro;
  `Volumen` = 0-100 y `Silencio` = 0/1 (no están hasta que se toca el
  volumen); `Activo` = el grupo cuya ventana estaba delante (0 el primero;
  -1, la del Centro); `Modulos` = cuántos .HVP había en las carpetas `JUEGOS`
  y `PROGS` la última vez que se miró (no está si no hay ninguno);
  `SinRepetidos` = 1 cuando ya se quitaron de «Principal» Editar, Gestor e
  Información (0.9.22) y `TareasPrincipal` = 1 cuando el Administrador de
  tareas ya pasó de «Accesorios» a «Principal»: las dos cosas se hacen una
  sola vez (si falta la clave, se hace al arrancar).
- `[Grupo]`: empieza un grupo. `Nombre` (27 letras); `Ventana`, la de su
  ventana; `Estado` = 0 cerrado, 1 abierto, 2 minimizado, 3 maximizado;
  `Marcado` = el icono que tiene marcado (0 el primero).
- `[Acceso]`: un acceso directo del último `[Grupo]`. `Nombre` (31 letras);
  `Icono` (uno de los nombres de la lista de arriba, sin acentos: `musica`,
  `ordenes`; si falta, `programa`); `Programa` (79 letras); `Carpeta` (67);
  `Parametros` (71); `Volver` = 0/1 (si falta, 1); `Pausa` = 0/1 (0).
- Juegos: en `[Centro]`, `JuegosDOS` = 1 cuando ya se creó (o se ligó a
  mano) un grupo de juegos de DOS: no se vuelve a crear solo. En `[Grupo]`,
  `Carpeta` = la carpeta de juegos a la que está ligado (67 letras; si
  falta, a ninguna). En `[Acceso]`: `Velocidad` = `Normal`, `Pentium`,
  `486`, `386`, `286`, `XT` o un tanto por ciento (`50%`; si falta,
  Normal); `Loadfix` = 0/1 (0); `Sonido` = `sb` para la Sound Blaster
  emulada (si falta, el sonido normal); `Auto` = 1 si lo puso Hispa Ventanas
  desde la carpeta de su grupo; `Cambiado` = 1 si después se cambió en
  «Propiedades» (ya no se quita solo); `Oculto` = 1 si se borró a mano (no
  se enseña ni vuelve a salir mientras el juego siga en la carpeta).

### Programas de Hispa-DOS desde Hispa Ventanas (juegos)

Los programas internos y los módulos .HVP son ventanas. Un acceso directo a
un programa de Hispa-DOS lo ejecuta **fuera** de Hispa Ventanas, con la pantalla
en modo texto y toda la memoria convencional para él, y después vuelve:

1. `VENTANAS.COM` se ejecuta dos veces. La primera copia es el **lanzador**:
   suelta toda su memoria menos los primeros 2 KB (PSP incluido); si hay
   memoria superior (`DOS=UMB`) donde quepan, se muda a ella con su entorno
   (copia el PSP, se hace dueño de los bloques nuevos, INT 21h AH=50h y suelta
   los de abajo): así un juego lanzado desde Hispa Ventanas tiene la misma
   memoria convencional que desde la línea de órdenes
   (`tests/run_memjuegos_tests.sh`). Después carga
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

Con **opciones de juego** (velocidad o LOADFIX), lo que el lanzador ejecuta
es `VELOCIDA.COM` (o `LOADFIX.COM`), de la carpeta de Hispa Ventanas, con el
juego en los parámetros:

    C:\HISPADOS\VELOCIDA.COM  386 C:\HISPADOS\LOADFIX.COM DOOM.EXE parámetros

(el juego va solo con su nombre si está en la carpeta de trabajo; un .BAT
lo ejecuta VELOCIDAD con `%COMSPEC% /C`). La orden entera no puede pasar de
126 letras: si no cabe, Hispa Ventanas lo dice en vez de lanzarla. Si falta
`VELOCIDA.COM` (o `LOADFIX.COM`) en la carpeta de Hispa Ventanas, avisa y no
lanza el juego. El juego recibe la misma memoria menos lo que ocupan
VELOCIDAD (unos 8 KB mientras corre) y LOADFIX (unos 3 KB, más lo que rellena
por debajo de 1000h).

**Al cerrar el juego el equipo vuelve a su velocidad normal.** `VELOCIDAD
nivel programa` ya quita el freno cuando el programa acaba, también si
acaba con Ctrl+C, con un error crítico (Abortar) o con un código de error.
Además el lanzador hace de **segunda red**: después de un juego abierto con
velocidad busca el residente de VELOCIDAD en la memoria (por su firma en la
cadena de bloques, como hace el propio VELOCIDAD) y, si aún frena o tiene
las cachés desactivadas, ejecuta `VELOCIDAD NORMAL`. Eso incluye un freno
que el usuario hubiera puesto a mano antes de entrar en Hispa Ventanas: tras
un juego con velocidad, el equipo queda siempre al 100 %. Lo que no tiene
arreglo se avisa al volver, en una ventana:

- si el juego deja un residente que toma la INT 08h, VELOCIDAD quita el
  freno (el equipo va a su velocidad normal) pero no puede soltar el
  temporizador ni salir de la memoria: se queda, sin frenar, hasta reiniciar;
- si VELOCIDAD siguiera frenando (no se ha podido ejecutar `VELOCIDAD
  NORMAL`), se dice cómo quitarlo a mano.

Con **Sound Blaster emulada**, el lanzador ejecuta antes `HSPSND.COM /SB`.
HSPSND apunta la variable `BLASTER` en el entorno de quien lo ejecuta, que
es el lanzador (de él la hereda el juego): antes le hace sitio agrandando
su entorno 64 octetos.

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

[MEZCLADOR] Debajo de la casilla «Silencio», el botón **«Mezclador...»** cierra
la ventanita y abre el Mezclador (`MEZCLAD.HVP`, docs/MEZCLADOR.md) en la vista
Completa; un **doble clic en el altavoz** hace lo mismo. Con HSPSND de la
versión 4 el volumen es la fuente «General» de su mezclador y se guarda,
con lo demás del mezclador, en `\HISPADOS\SONIDO.INI` (lo escribe
Configuración en cuanto cambia); CENTRO.INI lo sigue apuntando como antes.

El altavoz de la barra superior (a la izquierda de «Salir») abre y cierra una
ventanita con una barra deslizante de 0 a 100 % (con el ratón o con las
flechas izquierda y derecha) y la casilla «Silencio». El volumen se le da al
driver HSPSND por su interfaz (INT 2Fh AX=D700h, función 04h, AL=0..31),
llamándolo en modo real. HSPSND no tiene función para leer el volumen, así
que el último valor se guarda en CENTRO.INI y se vuelve a poner al entrar.
Si HSPSND no está cargado, o no ha encontrado tarjeta, el altavoz sale gris
y la ventanita lo dice. Con el volumen en silencio, el altavoz lleva un aspa.

**La campanita.** Al cambiar el volumen (al soltar la barra, con las flechas
o al quitar el silencio; también en Configuración › Sonido y en el
Reproductor de sonido) suena una campanita corta con el volumen nuevo, para
oír cómo queda. En silencio no suena, ni mientras un programa está sonando
(Hispa-Nes, el Reproductor: ya se oye el volumen). Es `\HISPADOS\CAMPANA.WAV`
(0,3 s a 11.025 Hz, 8 bits, mono: 3 KB; la hace `tools/mksample.py` con
cuatro parciales de campana y caída exponencial), que Hispa Ventanas lee la
primera vez que hace falta: así no ocupa sitio en VENTANAS.COM (si el archivo
no está, no suena y ya). La ventana nunca espera al sonido: `snd_campana`
solo la prepara y `cam_bomba`, desde el bucle del sistema, le va dando a
HSPSND trozos de la medida exacta que la tarjeta toca de una vez (2.048
muestras a 48.000 Hz con HD Audio y AC'97, repitiendo muestras; 1.024 o
4.096 a 11.025 Hz con las Sound Blaster), y solo los que caben en su cola en
ese momento: se lo pregunta con la **función 08h** de HSPSND (versión 3 de
su interfaz: devuelve en AL cuántos trozos caben sin esperar; con un HSPSND
anterior no hay campanita). Detrás van unos trozos de silencio y, cuando
HSPSND dice que la cola está vacía, se le manda parar (función 03h): HD
Audio da vueltas a sus cuatro trozos y, si no, repetiría el final sin
parar. Si ya estaba sonando, se corta y empieza otra vez; si un programa
empieza a sonar, se deja. Las teclas de volumen de HSPSND (Ctrl+Alt+flechas)
no pasan por Hispa Ventanas: con ellas no suena.

### La batería y la hora

[BATERÍA] [RELOJ] En la barra superior, **entre el altavoz y «Salir»**, van un
indicador de batería y la hora (`bat_*`, `bar_*`, `rel_*` en `escritorio.inc`).
El orden, de izquierda a derecha, es **[altavoz] [batería] [HH:MM] [Salir]**;
el altavoz se corre a la izquierda para dejarles sitio (todo cuadra también a
640x480).

La **hora** se enseña siempre, en formato de 24 horas («23:58»), en crema y con
la fuente de la barra. Se lee de DOS con `INT 21h` función 2Ch (como el Reloj,
`reloj.inc`) y se guarda en `bar_h`/`bar_m`.

El **indicador de batería** es una pila horizontal: el cuerpo con marco crema y
un borne a la derecha, con un relleno proporcional de izquierda a derecha
—**verde** por encima del 50 %, **ocre** del 20 al 50 % y **rojo** por debajo
del 20 %— y el tanto por ciento al lado («85 %»). Si está **enchufada o
cargando**, lleva un pequeño rayo blanco encima. Solo sale si hay una batería;
en un **sobremesa** (sin APM) se ve únicamente la hora (una batería ahí no
sirve de nada, un reloj sí).

El nivel de batería se lee por **APM** (como en los 90), llamando a la BIOS en
modo real (`rm_int`): primero `INT 15h AX=5300h` comprueba que hay APM
(devuelve BX = «PM»); luego `INT 15h AX=530Ah BX=0001h` da el estado —la línea
de CA (BH), si la batería carga (BL=03h), si no hay batería (CH bit 7: un
equipo de sobremesa) y el tanto por ciento (CL)—. Si no hay APM, el nivel es
desconocido o no se puede llamar a la BIOS en modo real (V86 sin VCPI), se
toma como «sin batería» y no se dibuja la pila.

[ACPI] Los portátiles de 2008 en adelante no tienen APM (el HP Mini 5103, el
Toshiba NB250): en ellos la batería la lee **`HSPACPI`** por ACPI y contesta
él a esas mismas llamadas de APM, así que aquí no cambia nada
([ACPI.md](ACPI.md)).

Un **temporizador del escritorio** de cinco segundos (número 1, puesto en
`WM_CREATE`) refresca la hora —y la batería, si la hay— y, comparando una
«firma» de lo que se ve (hora, nivel y estado), solo repinta la zona de la
derecha cuando algo cambia (al pasar el minuto, sobre todo). En un sobremesa no
se gasta APM en cada vuelta: solo se vuelve a leer la batería si la había. En
la demo (`/DEMO`) no aparece ni batería ni hora, y la barra queda como antes.

Como QEMU no simula una batería APM, para las pruebas y las capturas hay una
opción oculta de depuración, **`/BATnn[C]`**, que fija el nivel sin tocar la
BIOS: `/BAT85` lo pone al 85 %, y una `C` al final («`/BAT100C`») lo marca
enchufado/cargando. La usa `tests/run_ventanas_bateria_tests.sh`.

### El escritorio de Hispa-486: barra de tareas y menú Inicio

[BARRA] (`ventanas/barra.inc`, en la parte alta; fase 4 de
docs/486/HISPA-486.md). Pedido de Carlos, probando la demo de Hispa-486: «en
486 se echa de menos la barra de inicio abajo», «todas las aplicaciones en el
menú de inicio para que quede el escritorio despejado» y «en 386 no hace
falta poner ese menú de inicio, porque así respeta la estética de Win 3.11».
Por eso Hispa Ventanas tiene **dos escritorios**:

- el de **Hispa-386**, el de siempre: la barra arriba, el Centro de
  programas abierto y los iconos de lo minimizado abajo. No ha cambiado nada
  y no tiene ninguna opción nueva en Configuración;
- el de **Hispa-486**: una **barra de tareas abajo** con el botón
  **«Inicio»**, un botón por ventana y, a la derecha, la red, el volumen, la
  batería y la hora; el escritorio **empieza despejado** (el Centro de
  programas no se abre: todos los programas están en el menú Inicio).

Es una función equivalente a la de los sistemas de mediados de los 90 con el
aspecto de Hispa Ventanas (marrón con filete ocre, terracota, el rombo): no
copia el dibujo de ninguno.

**Cómo se pide.** No es un botón de Configuración: es un ajuste de la
instalación de Hispa-486. Vale cualquiera de los dos:

- la opción **`/486`** de la línea de órdenes (`VENTANAS /486`; se puede
  juntar con las demás: `VENTANAS /486 /640`);
- la clave **`Escritorio=486`** en `VENTANAS.INI` (sección `[Escritorio]`,
  aunque las claves no miran la sección), que es lo que pone la imagen o la
  instalación de Hispa-486. La lee el propio VENTANAS.COM al arrancar
  (`bt_ini_leer`, antes de crear nada); Configuración no la enseña ni la
  cambia, pero la **conserva** cuando guarda el archivo. `Escritorio=386` (o
  no poner la clave) es el escritorio de siempre.

No se cambia en marcha: vale para toda la sesión.

**La barra de tareas** (28 puntos, como la de arriba; la misma en 800×600,
640×480 y en la VGA de 16 colores):

- **«Inicio»**, a la izquierda (un clic, **Ctrl+Esc** o la **tecla de
  Windows** abren y cierran el menú; el botón se aclara mientras está
  abierto).
- **Un botón por ventana principal**, con su icono (a mitad de tamaño) y su
  título recortado a lo que quepa; se reparten el sitio (160 puntos cada uno
  como mucho; si hay muchas, se estrechan hasta quedarse en el icono). Un
  clic **trae la ventana delante**; si ya está delante, **la minimiza**; si
  está minimizada, **la recupera**. La de delante va en crema con una raya
  terracota; las demás, en arena; las minimizadas, en arena oscura. Tienen
  botón las ventanas que se pueden minimizar y, de un programa que no tiene
  ninguna así (uno de Windows que solo saca un cuadro, por ejemplo), la
  primera suya; los cuadros de diálogo de un programa con ventana no salen
  (el botón de su dueña los trae delante). Los programas de Windows
  (`win32*.inc`) salen como cualquier otro: son ventanas del mismo gestor.
- **La bandeja**, a la derecha: el icono de red (un clic, la ventanita de
  la red: ver abajo), el altavoz (un clic, la ventanita del volumen, que
  aquí sale encima de la barra; doble clic, el Mezclador), la batería si la
  hay y la hora. Son los mismos dibujos y las mismas rutinas de la barra de
  arriba (`vol_pintar_icono`, `bat_pintar_icono`, `rel_pintar`), corridos
  al sitio que deja «Salir»; el de red es propio de esta barra
  (`wf_pintar_icono`).
- **[WIFI] El icono de red** dice por la forma (no solo por el color) si no
  hay red (nodos huecos y un aspa), si hay cable con o sin enlace (nodos
  llenos; con aspa, sin enlace) o Wi-Fi (cuatro rayitas: llenas, las de la
  señal; huecas y con aspa, sin conectar). Un clic abre encima de la barra
  una ventanita: con HSPWIFI, el estado, la lista de redes con su señal y su
  seguridad, la contraseña (en asteriscos) y «Buscar», «Conectar»,
  «Desconectar» y «Olvidar»; con HSPRED, el enlace, la IP y la puerta de
  enlace; y siempre «Configuración de red...» (Configuración › Red). Todo
  en `ventanas/wifi.inc` (parte alta: 6.784 octetos; la parte baja no cambia) y `bsswifi.inc`; los
  detalles, en docs/WIFI.md («En Hispa Ventanas»). En Hispa-386 no cambia
  nada.
- **Botón derecho** en la barra: «Ventanas en cascada», «Ventanas en
  mosaico», «Administrador de tareas...» y «Configuración...». En el botón
  de una ventana: el menú de esa ventana (Restaurar, Minimizar, Maximizar,
  Cerrar).
- Las ventanas minimizadas **no van como icono al escritorio**: se quedan en
  su botón.

**La barra de arriba no está** en este escritorio (es lo más limpio: dos
barras para lo mismo estorban, y en 640×480 se comerían 56 puntos). Lo que
tenía se reparte: el nombre «Hispa Ventanas», en la cabecera del menú
Inicio; «Salir», dentro del menú; el volumen, la red, la batería y la hora,
en la bandeja. El fondo ocupa toda la pantalla.

**El menú Inicio** (`ini_*`): arriba, una cabecera marrón con el rombo y
«Hispa Ventanas»; debajo, **un grupo del Centro de programas por línea**
(con su flecha: cada uno abre un **submenú con sus programas**, con el icono
de cada acceso; el de un .HVP, el suyo); una raya; **Configuración**,
**Administrador de archivos**, **Administrador de tareas** y **Centro de
programas**; otra raya; y **«Salir de Hispa Ventanas...»** (el cuadro de
siempre: volver a Hispa-DOS, reiniciar o apagar). Se lee de los grupos y los
accesos de CENTRO.INI cada vez que se abre: lo que se añade o se cambia en
el Centro sale en el menú sin hacer nada más.

- Con el **ratón**: un clic en un grupo abre su submenú; también al parar el
  ratón encima un cuarto de segundo (así se puede ir en diagonal al submenú
  sin que se abra el de al lado). Un clic en un programa o en una orden la
  hace; un clic fuera cierra el menú (y el clic sigue su camino).
- Con el **teclado**: Ctrl+Esc lo abre con la primera línea marcada; flechas
  arriba y abajo; derecha o Intro entran en el submenú; izquierda o Esc
  salen de él; Intro abre el programa; la primera letra salta a la orden
  siguiente que empieza por ella; Esc (o Ctrl+Esc otra vez, o Alt+F4) cierra.
- Un grupo vacío dice «(vacío)». Si un grupo tiene más programas de los que
  caben en la pantalla en varias columnas (solo pasa con decenas de juegos en
  640×480), la última línea es «Más...», que abre la ventana del grupo.
- El **Centro de programas** sigue existiendo, oculto: es el dueño de los
  grupos y de los avisos. «Centro de programas» (en el menú Inicio y en el
  menú del escritorio) lo enseña para crear grupos y accesos; **cerrarlo lo
  vuelve a esconder** (no sale de Hispa Ventanas: para eso está «Salir»).
  Los grupos que quedaran abiertos no se vuelven a abrir al entrar.

**El área de trabajo** es la pantalla menos la barra (`area_y0`, `area_y1`:
en Hispa-386, de debajo de la barra de arriba al final de la pantalla; en
Hispa-486, de arriba del todo a la barra de tareas). La usan maximizar
(`bt_area`), arrastrar y cambiar el tamaño (`ges_acotar`), la cascada y el
mosaico, el centrado de los cuadros de diálogo y `bt_colocar`, que sube lo
justo la ventana que nacería detrás de la barra.

**Cómo está hecho.** Todo está en `barra.inc` (parte alta) y sus variables
en `bssbarra.inc`; en los demás archivos solo hay ganchos de una o dos
líneas, marcados con [BARRA] (en la parte baja son 132 octetos, contando la
opción `/486` y su línea de ayuda; `barra.inc` ocupa unos 6 KB de la alta):

| Gancho | Dónde | Qué hace |
|---|---|---|
| `bt_iniciar` | `principal` | pone el área de trabajo; lee `Escritorio=486`; en Hispa-486 crea la barra y le cambia el procedimiento al escritorio (`bt_esc_proc`: el fondo entero, sin barra arriba ni iconos) |
| `bt_vigilar` | bucle del sistema, una vez por vuelta | la lista de ventanas con botón y la «firma» de lo que enseña cada una (estado, si está delante, título, icono) y de la bandeja: si algo cambia, repinta la barra. También hace lo que se ha elegido en el menú y mira la tecla de Windows |
| `bt_orden` | `orden_poner` | la ventana con título que pasa delante se queda justo debajo de la barra (la barra está siempre encima de las ventanas; los menús y lo emergente, encima de ella) |
| `bt_area` | `ven_mostrar` (maximizar) | el área de trabajo |
| `bt_colocar` | `ven_crear` | que una ventana no nazca detrás de la barra |
| `bt_centro_ini`, `bt_centro_cerrar` | `centro_tarea`, `vista_proc` | el Centro, oculto al entrar y al cerrarlo |
| `bt_tecla` | `ctx_tecla` (de `ges_tecla`) | Ctrl+Esc y, con el menú abierto, todas las teclas |
| `bt_clic_fuera` | `ctx_ctrl_clic` (de `eventos`) | un clic fuera del menú lo cierra |
| `wf_vigilar` | `bt_vigilar` | [WIFI] mira la red cada segundo (cada 3 tics con la ventanita abierta o algo en marcha) y lleva a trozos buscar, preparar la clave y conectar |
| `wf_pintar_icono`, `wf_en_icono`, `wf_alternar` | `bt_pintar`, `bt_proc` | [WIFI] el icono de red y su ventanita |

La barra es una ventana de primer nivel sin título, de la tarea del sistema;
el menú y el submenú, dos ventanas emergentes con su propia clase
(`c_inicio`), no los desplegables de `menus.inc` (aquellos no tienen iconos
ni submenús). **El gestor no avisa de nada a la barra**: es ella la que mira
(`bt_vigilar`), así que cualquier ventana —de un módulo, de un programa de
Windows, de dentro— sale sola. Lo elegido en el menú no se hace dentro del
procedimiento de la ventana del menú (que hay que destruir), sino en la
vuelta siguiente (`ini_pendiente`); un programa se abre con `acc_lanzar`
—lo mismo que un doble clic en su icono— en una tarea propia
(`bt_lz_tarea`), porque lee del disco y puede sacar un aviso.

Límites: 32 ventanas con botón (las de primer nivel que admite el gestor);
el escritorio no se cambia sin reiniciar Hispa Ventanas; el escritorio de
Hispa-486 todavía no tiene iconos propios ni «Mi PC» (lo siguiente de la
fase 4).

### La imagen de fondo del escritorio

[FONDO] (`ventanas/fondo.inc`; pedido de Carlos, 18/11/2026: «igual ya tenemos
imagen para el fondo de escritorio de 486»). Detrás de las ventanas, en vez
del dibujo de 16×16, puede ir **una imagen**. Vale para los dos escritorios
(el de Hispa-386 y el de Hispa-486) y es opcional: **sin la clave `Fondo=` en
VENTANAS.INI no hay imagen** y todo es como antes (Hispa-386, de serie); la
imagen de Hispa-486 la trae puesta (docs/486/HISPA-486.md).

Cinco formas de ponerla (`Ajuste=`):

| Forma | Qué hace |
|---|---|
| `ajustada` (la de serie) | a la pantalla, **conservando la proporción**: llena el ancho o el alto y queda en el centro; lo que sobra se rellena con el **color del escritorio** (el del fondo del dibujo elegido en Configuración › Pantalla: el que más sale en el mosaico). Una imagen pequeña se amplía; una grande se reduce |
| `centrada` | a su tamaño, en el centro, con el color del escritorio alrededor; si es mayor que la pantalla, se ve su parte central |
| `mosaico` | a su tamaño, repetida desde la esquina de arriba a la izquierda |
| `estirada` | a **toda la pantalla, aunque se deforme**: el ancho y el alto se escalan cada uno por su lado (pedido de Carlos, 18/11/2026) |
| `rellenar` | a **toda la pantalla sin deformar**: se escala hasta llenarla y lo que sobra (por los lados, o por arriba y abajo) queda fuera, a partes iguales |

**Dónde se ve.** En todos los modos de VESA (640×480, 800×600 y 1024×768, de
256 colores o de color verdadero: la superficie de dibujo es siempre de un
octeto por punto con la paleta de Hispa Ventanas). En la **VGA de 16
colores no hay imagen**: queda el dibujo; la clave no se pierde (sale otra
vez al entrar con VESA). Con `VENTANAS /DEMO`, tampoco.

**El archivo.** Un **BMP sin comprimir, de 8 bits (con su paleta) o de 24**,
de 4.096 puntos de lado como mucho. Hispa Ventanas no descodifica aquí nada
más (VENTANAS.COM no carga con JPEG ni con Deflate): los demás formatos se
convierten antes, con Mirador o con `tools/mkfondo.py` (abajo). Al leerlo,
cada color pasa a la paleta de Hispa Ventanas:

- un color del BMP que es **exactamente uno de los 256** de la paleta (los 16
  propios, el cubo de 6×6×6 o los 24 grises) se queda tal cual. Un BMP ya
  preparado (los de `mkfondo.py` y los que guarda Mirador «con los 256
  colores de Hispa Ventanas») **se lee y se pinta sin convertir nada**: lo
  más rápido al arrancar;
- los demás van al cubo de 6×6×6 con un **difuminado ordenado** (matriz de
  4×4), y los grises (rojo = verde = azul), a la escala de 24 grises. Así
  vale cualquier BMP de 24 bits o con otra paleta, aunque queda mejor uno
  preparado con `mkfondo.py` (Floyd-Steinberg).

**Escalar.** Al ampliar, cada punto se repite (no se difumina otra vez). Al
reducir, cada punto de la pantalla es la **media** de los del original que
le caen encima (en rojo, verde y azul), que vuelve a la paleta con el mismo
difuminado ordenado: una imagen preparada para 800×600 se ve bien también en
640×480.

**Memoria.** La imagen lista (ya escalada o recortada: nunca más que la
pantalla) queda en **un bloque de memoria extendida** (`mem_bloque`, la
función 0501h del DPMI), un octeto por punto: 480.000 octetos (469 KB) una
de 800×600 y 307.200 (300 KB) una de 640×480; nada en la memoria
convencional. Mientras se lee hay otro bloque con el original (ancho × alto
octetos, más una fila del archivo), que se suelta al acabar; si la imagen
ya tiene el tamaño con el que se pone (la de Hispa-486 en 800×600), el
original pasa a ser el definitivo y no hay segundo bloque. Si no hay memoria
(o el archivo no está o no vale), queda el dibujo y la clave se conserva.
VENTANAS.COM crece 2,7 KB (de 130.736 a 133.488 octetos: el código, en la
parte alta; en los 64 KB de la parte de modo real no hay nada nuevo: los dos
escritorios llaman a `fon_pintar` donde llamaban a `dc_mosaico`) y medio KB
de variables; lo que solo hace falta al cargar (VENTANAS.INI, la paleta del
BMP) usa el búfer con el que `barra.inc` lee VENTANAS.INI al arrancar.
CONFIG.HVP crece 2,2 KB y MIRADOR.HVP, 0,6 KB.

**Pintar.** `fon_pintar` (lo llaman `esc_proc` y `bt_esc_proc` al pintar el
escritorio) copia del bloque a la superficie **solo en los rectángulos del
escritorio que están a la vista** (`dc_cada`, como las demás primitivas):
al mover una ventana se repinta lo que destapa, tan deprisa como el dibujo
de antes (un `rep movsb` por fila), y sin parpadeo, porque la pantalla solo
recibe la superficie ya compuesta. Con mosaico solo se copian las piezas
que tocan el rectángulo. La barra de tareas de Hispa-486 y la barra de
arriba de Hispa-386 se pintan encima, como siempre (tapan esa franja de la
imagen: la imagen se coloca respecto a la pantalla entera).

**Las claves** (en VENTANAS.INI, sección `[Escritorio]`; las lee VENTANAS.COM
al arrancar, antes de pintar nada: `fon_iniciar`):

```
[Escritorio]
Fondo=FONDO.BMP
Ajuste=ajustada
```

- `Fondo`: la ruta del BMP. **Sin unidad ni `\` delante es de la carpeta de
  VENTANAS.COM** (`FONDO.BMP` es `C:\HISPADOS\FONDO.BMP`: la misma
  VENTANAS.INI vale en la memoria USB y en el disco duro); con ellas, la que
  diga (`C:\FOTOS\MAR.BMP`). Hasta 79 letras. Sin la clave (o vacía), no hay
  imagen.
- `Ajuste`: `ajustada`, `centrada`, `mosaico`, `estirada` o `rellenar` (o
  0, 1, 2, 3, 4; solo se mira la primera letra). Sin ella, `ajustada`.

**Cómo se cambia.**

- **Configuración › Escritorio** (el octavo apartado): la imagen puesta,
  **«Elegir imagen...»** (el diálogo de abrir de Hispa Ventanas: un BMP),
  **«Sin fondo»** y las cinco formas. Se aplica al momento; si el archivo no
  vale (no es un BMP de 8 o 24 bits sin comprimir, no se puede abrir o no
  hay memoria) lo dice y **se queda la que había**. En la VGA de 16 colores
  lo explica («no se ve la imagen: queda el dibujo») y la elección queda
  apuntada. «De fábrica» no toca la imagen.
- **Mirador › Imagen › «Poner como fondo de escritorio»**: la imagen que se
  está viendo (de cualquier formato de los que lee: JPEG, PNG, GIF, PCX,
  TGA...; como se ve: girada, recortada, retocada) se guarda como
  `MIFONDO.BMP` en la carpeta de VENTANAS.COM (un BMP de 256 colores con la
  paleta de Hispa Ventanas, con el paso a 256 colores que tenga puesto
  Mirador: difusión de error de serie) y pasa a ser el fondo, con la forma
  que hubiera.
- A mano, en VENTANAS.INI, con Hispa Ventanas cerrado.

**Quién hace qué.** La imagen la lleva VENTANAS.COM; CONFIG.HVP no lee esas
dos claves: al guardar VENTANAS.INI le pregunta a Hispa Ventanas lo que hay
puesto y lo escribe (si no hay `Escritorio=`, pone también la sección). Los
programas usan una entrada nueva de la interfaz, la 97, **`esc_imagen`**:

| EAX | Qué hace |
|---|---|
| 0 | lo que hay: → EAX=forma (0 ajustada, 1 centrada, 2 mosaico, 3 estirada, 4 rellenar), ESI=la ruta (vacía: sin imagen), ECX=0 sin imagen, 1 a la vista, 2 apuntada pero no se ve (16 colores, o no se pudo cargar), EDX=un número que cambia cada vez que se pone otra |
| 1 | ESI=ruta (0 o vacía: sin fondo), EDX=forma: la pone y repinta el escritorio → EAX=0 y CF=0; o CF=1 y EAX=1 (no se puede abrir o leer), 2 (no es un BMP de 8 o 24 bits sin comprimir), 3 (no hay memoria: en los tres casos sigue la que había) o 4 (VGA de 16 colores: queda apuntada) |
| 2 | EDI=búfer de 80: la carpeta de VENTANAS.COM, con su `\` al final (donde Mirador deja `MIFONDO.BMP`) |

Configuración mira ese número una vez por segundo: si otro programa
(Mirador) ha cambiado el fondo, lo apunta en VENTANAS.INI a los tres
segundos, como cualquier otro cambio.

**`tools/mkfondo.py`** prepara un fondo fuera de Hispa-DOS:

```
python3 tools/mkfondo.py FOTO.JPG FONDO.BMP [--tam 800x600]
                         [--modo recortar|ajustar|estirar] [--sin-difuminar]
```

Reduce la foto al tamaño de la pantalla (`recortar`, lo normal: llena la
pantalla y deja fuera lo que sobra por los lados; `ajustar`: entera, con su
proporción, y el BMP sale más corto por un lado; `estirar`: deformada) y la
pasa a los **240 colores fijos** de la paleta (del 16 al 255: no usa los 16
propios) con difuminado de Floyd-Steinberg en zigzag. Escribe un BMP de
8 bits con la paleta de Hispa Ventanas (la lee de `ventanas/datos.inc`).
Lee PPM y BMP de 24 bits por sí mismo; JPEG, PNG y demás, con Pillow si
está o, si no, con ImageMagick, ffmpeg o djpeg; reducir, difuminar y
escribir el BMP es suyo (Python sin bibliotecas).

Límites: una sola imagen para todas las resoluciones (se escala al
arrancar); los nombres de los
iconos de lo minimizado (Hispa-386) y el menú del botón derecho se pintan
igual sobre la imagen. Pruebas: `tests/run_fondo_tests.sh`.

### ASOCIA.INI: qué programa abre cada tipo de archivo

`\HISPADOS\ASOCIA.INI` (0.9.22) dice con qué programa se abre cada
extensión al hacer doble clic (o Intro) en el Administrador de archivos.
Una línea por extensión, `EXT=programa`, donde el programa va con su ruta
desde `\HISPADOS` (`PROGS\PLUMA.HVP`), con la ruta entera
(`C:\JUEGOS\X.HVP`) o es `:BLOC` (el Bloc de notas, que va dentro de Hispa
Ventanas); `EXT=-` o no tener línea: ninguno. Las líneas que empiezan por
`;` o `[` no cuentan. Si no existe, se escribe con los de serie la primera
vez que hace falta:

| Extensiones | Programa |
|---|---|
| TXT | Bloc de notas |
| PLU | Pluma |
| HOJ, CSV | Cuentas |
| BDD | Datos |
| PDF | Lector |
| PUB | Publicaciones |
| PRE | Presentaciones |
| BMP, PCX, GIF, PNG, TGA, ICO, JPG, JPEG, PBM, PGM, PPM | Mirador |
| WAV | Sonidos |

Lo cambian «Abrir con…» del Administrador de archivos (menú Archivo y el del
botón derecho; con «¿Usar siempre este programa para los .XXX?») y
Configuración › Sistema › «Tipos de archivo...». Lo que no tiene programa
pregunta «Abrir con…». Los programas, los .COM/.EXE/.BAT, las carpetas y los
módulos .HVP no pasan por aquí. «Editar» con un BMP lo abre en Pintura (abrir
lo lleva a Mirador). Lo común está en `programas/comun/asocia_ini.inc`.

### Configuración y VENTANAS.INI

**Configuración** (`programas/config.asm`, `CONFIG.HVP`, en `\HISPADOS`, al
lado de VENTANAS.COM; acceso directo en el grupo «Principal») es el programa
de los ajustes. Es un módulo .HVP con diseño propio: los apartados a la
izquierda (un rombo y su nombre; el elegido, en añil) y, a la derecha, el
apartado elegido. Abajo, lo que pasa con los cambios y los botones «De
fábrica» (pregunta antes), «Guardar» y «Cerrar».

| Apartado | Qué tiene |
|---|---|
| Pantalla | Tema (ocho combinaciones hechas: Hispa, Noche, Mar, Campo, Vino, Duna, Pizarra, Ladrillo), dibujo del fondo (diez: rombos, liso, puntos, rayas, cuadros, ladrillos, olas, damero, trama, zigzag; se ven con los colores puestos), color del fondo y color del dibujo (los 16 de la paleta), y el salvapantallas: casilla, minutos (1 a 60), dibujo («Letrero de Hispa» o «Lluvia verde») y «Probar», que enseña el elegido |
| Ratón | Velocidad del puntero (de x0,5 a x3), tiempo máximo del doble clic (165 a 990 ms, con una zona para probarlo) e intercambiar los botones (para zurdos); [CONTEXTO] «Ctrl+clic como botón derecho» (para los Mac y los touchpads de un botón; activado por omisión) |
| Teclado | Espera antes de repetir (250 a 1000 ms) y velocidad de repetición (2 a 30 por segundo), con un cuadro para probarlas; distribución española o estadounidense, si TECLADO (KEYB) está cargado |
| Sonido | Volumen y silencio: los mismos del altavoz de la barra (si no hay HSPSND, lo dice); [MEZCLADOR] el botón «Mezclador...», la tarjeta que lleva HSPSND y si está la «Sound Blaster para juegos» (`HSPSND /SB`: sí, no o «es la propia tarjeta»), con lo que hay que hacer para ponerla o quitarla (no se puede sin volver a cargar HSPSND, así que lo explica). Además, Configuración guarda cada segundo el mezclador de HSPSND en `\HISPADOS\SONIDO.INI` si ha cambiado (docs/MEZCLADOR.md) |
| Fecha y hora | La fecha con todas las letras y la hora, al segundo; seis cuadros (día, mes, año, hora, minuto, segundo) y «Poner fecha y hora» |
| Sistema | Versión de Hispa Ventanas y de Hispa-DOS, procesador, memoria convencional, extendida y libre, pantalla, vídeo, ratón, sonido, teclado y país. Abajo, **«Tipos de archivo...»** ([ASOCIA], 0.9.22): las extensiones con el programa que las abre (de `\HISPADOS\ASOCIA.INI`), más «Añadir un tipo...» y «Volver a los de serie»; al elegir una, la lista de programas (o «Ninguno», que la quita) |
| Red | La red local del equipo (como las «Propiedades de TCP/IP» de Windows). Una casilla **«Obtener la dirección automáticamente (DHCP)»**; al quitarla, cuatro cuadros para la **IP**, la **máscara de subred**, la **puerta de enlace** y el **servidor DNS** a mano (cada uno `a.b.c.d`, con octetos de 0 a 255; con DHCP puesto, no salen). Arriba, el estado en marcha si HSPRED está cargado: la MAC y la IP que tiene. Se guarda en `\HISPADOS\RED.INI` (no en VENTANAS.INI); los cambios se aplican al reiniciar o al recargar HSPRED (`CONFRED /RENOVAR`). Ver docs/RED.md |
| Escritorio | [FONDO] La imagen de fondo del escritorio: la que hay puesta, **«Elegir imagen...»** (un BMP, con el diálogo de abrir), **«Sin fondo»** y cómo se pone: ajustada a la pantalla (con su proporción), centrada, en mosaico, estirada (toda la pantalla, aunque se deforme) o rellenando (toda la pantalla, recortando lo que sobra). Ver «La imagen de fondo del escritorio» |

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
   pantalla con uno de sus dibujos: el **letrero** («Hispa Ventanas» y la
   hora, que se pasea sobre negro) o la **lluvia verde** (columnas de
   letras que caen a distintas velocidades, con la cabeza casi blanca y la
   estela que se apaga de verde claro a verde oscuro; las letras de la
   estela cambian y las columnas vuelven a empezar arriba). La lluvia usa
   los verdes del cubo de la paleta con VESA (256 colores o color
   verdadero) y, en la VGA de 16 colores, los que hay (turquesa, verde
   oliva y petróleo). Se repinta solo lo que cambia, una vez por tic de
   reloj. Para añadir otro dibujo: `SALVA_*` y `N_SALVA`, su nombre en
   `sal_nombres`, su opción en `pl_pantalla` y lo suyo en `sal_proc`.
   Se quita con el ratón o con una tecla (que no llega a la ventana de
   debajo). (En el modo texto de Hispa-DOS la lluvia la pone `PROTECTOR`,
   ver README.md; no salta dentro de Hispa Ventanas, que va en modo
   gráfico.)

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
SalvaDibujo=letrero

[Raton]
Velocidad=2
DobleClic=440
Zurdo=0
CtrlClic=1

[Teclado]
Espera=500
Repeticion=20
Distribucion=SP

[Escritorio]
Escritorio=486
Fondo=FONDO.BMP
Ajuste=ajustada
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
  `Salvapantallas` = 0/1, `Minutos` = 1-60 y `SalvaDibujo` = `letrero`
  o `lluvia` (o su número, 0 o 1; no se llama `Dibujo` porque ese es el
  del fondo y las claves no miran la sección).
- `Velocidad`: del ratón, en mitades (1 = x0,5; 2 = normal; 6 = x3).
  `DobleClic`: milisegundos como mucho entre los dos clics (165-990; se
  redondea a tics de reloj, de 55 ms). `Zurdo` = 0/1. `CtrlClic` = 1 (lo
  normal: Ctrl+clic izquierdo hace de botón derecho) o 0.
- `Espera` (250, 500, 750 o 1000 ms) y `Repeticion` (0-31; 31, la más
  rápida: 30 por segundo) se dan a la BIOS (INT 16h, función 03h);
  `Distribucion` = `SP` o `US`, al residente TECLADO. Las tres **solo están
  si se han tocado**: sin ellas, Hispa Ventanas no cambia nada del teclado.
- [BARRA] `Escritorio` = `486` (la barra de tareas y el menú Inicio de
  Hispa-486) o `386` (el de siempre). **Solo está en las instalaciones de
  Hispa-486**: la pone la instalación, la lee VENTANAS.COM al arrancar y
  Configuración no la enseña; si está, la conserva al guardar, y si no,
  no escribe ni la sección (ver «El escritorio de Hispa-486»).
- [FONDO] `Fondo` = la imagen de fondo del escritorio (un BMP; sin unidad
  ni `\` delante, de la carpeta de VENTANAS.COM) y `Ajuste` = `ajustada`,
  `centrada`, `mosaico`, `estirada` o `rellenar`. Las lee VENTANAS.COM al arrancar y se cambian en
  Configuración › Escritorio (o desde Mirador); Configuración las escribe
  con lo que le diga Hispa Ventanas. Sin `Fondo`, no hay imagen y no se
  escribe ninguna de las dos (ver «La imagen de fondo del escritorio»).

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
  win32.inc      [Hispa-486] programas de Windows de ventanas (parte alta):
                 HW32.COM como módulo (su KERNEL32 y sus DLL), el despacho
                 y la llamada de vuelta
  win32u.inc     [Hispa-486] su USER32 y su GDI32 (parte alta)
  win32c.inc     [Hispa-486] sus controles de serie: BUTTON, EDIT, STATIC,
                 LISTBOX y COMBOBOX sobre los de controles.inc (parte alta)
  win32m.inc     [Hispa-486] sus menús (sobre menus.inc), sus recursos
                 (.rsrc) y sus diálogos de plantilla (parte alta)
  ajustes.inc    los ajustes que cambia Configuración (CONFIG.HVP)
  centro.inc bloc.inc calc.inc reloj.inc    los programas
  accesos.inc    grupos ligados a una carpeta de juegos y opciones de
                 juego de un acceso (va en la parte alta del archivo)
  barra.inc      [BARRA] el escritorio de Hispa-486: barra de tareas y menú
                 Inicio (parte alta)
  fondo.inc      [FONDO] la imagen de fondo del escritorio (BMP): leerla,
                 escalarla y pintarla; la entrada esc_imagen
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

**Las letras que la 437 no tiene.** Para escribir bien español, catalán,
valenciano, gallego, portugués y francés, Ventanas pone 26 letras más en
códigos de la 437 que nunca pintaba (recuadros que mezclan línea sencilla y
doble, dos medios bloques y unos signos griegos y de fórmulas sin dibujo). La
lista está en un solo sitio, `tools/hispa437.py`, y la usan la fuente
(`tools/mkfuente.py`), los tipos de la oficina (`tools/hazfuentes.py`), las
tablas de la página 1252 de los programas Hispa32 (`tools/mk1252.py`:
`w32_1252` y `w32_437`), el paso de los fuentes a la 437
(`tools/cp437.py --ventanas`, para Ventanas y los .HVP) y la letra de
HispaBIOS (`tools/mkfuentebios.py`, que los deja como en la 437):

| Código | Letra | En la 437 | | Código | Letra | En la 437 |
|---|---|---|---|---|---|---|
| B5h | À | ╡ | | D2h | Ò | ╥ |
| B6h | Á | ╢ | | D3h | Ó | ╙ |
| B7h | Â | ╖ | | D4h | Ô | ╘ |
| B8h | Ã | ╕ | | D5h | Õ | ╒ |
| BDh | È | ╜ | | D6h | Ù | ╓ |
| BEh | Ê | ╛ | | D7h | Ú | ╫ |
| C6h | Ë | ╞ | | D8h | Û | ╪ |
| C7h | Ì | ╟ | | DDh | ã | ▌ |
| CFh | Í | ╧ | | DEh | õ | ▐ |
| D0h | Î | ╨ | | E2h | Œ | Γ |
| D1h | Ï | ╤ | | E5h | œ | σ |
| E7h | © | τ | | F4h | € | ⌠ |
| E8h | ® | Φ | | F5h | … | ⌡ |

Los recuadros de siempre (sencillos y dobles enteros), las tramas y los
bloques enteros siguen como estaban. Es **solo de Ventanas**: el modo texto
(la VGA y la letra de HispaBIOS) sigue con la 437 de siempre. Por eso un texto
guardado desde Ventanas (Bloc de notas, Pluma...) con «Á» lleva B6h, que en
un programa de modo texto (EDIT, TYPE) se ve «╢»; y un .TXT de DOS con esos
recuadros mezclados se ve en Ventanas con estas letras. Lo que ya estaba
guardado no cambia (antes esas letras no se podían escribir). Los códigos no
se cambian de sitio: hay textos guardados con ellos.

Con el teclado español (KEYB SP), Ventanas pide a KEYB al arrancar (INT 2Fh
AX=AD82h BL=1; BL=0 al salir) que las mayúsculas con acento (´ ` ^ ¨ y la
vocal: Á Í Ó Ú À È Ì Ò Ù Â Ê Î Ô Û Ë Ï) y el euro (AltGr+E) salgan con estos
códigos; fuera de Ventanas, como siempre (´ y A = A). Lector (PDF) sigue
pasando el texto a la 437 de siempre (€ = «EUR»), porque guarda .TXT para DOS.
Con estas letras los tipos de la oficina crecieron y `TIPOS32.DAT` ya no cabe
en el disquete «Oficina»: va en el de «Instalación 2», en su
`\HISPADOS\PROGS`, que INSTALAR también copia.

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

### Botón derecho

[CONTEXTO] (`ventanas/contexto.inc`, en la parte alta). Pedido de Carlos:
«No tenemos botón secundario en Ventanas». Los menús contextuales llegaron
a mediados de los 90; aquí tienen el aspecto de los menús de siempre.

- **El menú emergente** es el mismo desplegable de los menús de la barra
  (`menus.inc`), abierto donde está el ratón. Se maneja igual: flechas,
  Intro, Esc, la **letra subrayada** (que ahora vale también en los
  desplegables de la barra) y el ratón; un clic fuera lo cierra. Si no cabe
  a la derecha o debajo del punto, sale al otro lado (se ajusta a los
  bordes de la pantalla). Las órdenes que no se pueden hacer salen en gris
  y no se marcan.
- **Cuándo sale**: al soltar el botón derecho sobre una ventana (que se
  activa al apretarlo). Con el botón apretado se puede ir hasta una orden y
  soltar encima. Sin ratón, **Mayús+F10** o la **tecla de menú** (la de la
  lista, junto a Ctrl derecho: Hispa Ventanas la mira en `tec_mapa`, porque
  la BIOS no la da como tecla) lo abren en la ventana que tiene el foco.
- **Mac y touchpads de un botón**: **Ctrl+clic izquierdo** hace de clic
  derecho en todo Hispa Ventanas (también para los módulos: lo cambia antes
  de repartir, como los botones de los zurdos). Se quita en Configuración >
  Ratón, «Ctrl+clic como botón derecho» (`CtrlClic=` en `[Raton]` de
  VENTANAS.INI; activado por omisión; `AJ_CTRL_DER`).

| Dónde | Menú |
|---|---|
| Un icono del Centro (un grupo) | Abrir, Propiedades..., Cambiar nombre..., Eliminar (pregunta antes) |
| Un icono de un grupo (un acceso) | Abrir, Propiedades... (el diálogo del acceso), Cambiar nombre..., Mover a otro grupo..., Eliminar (pregunta antes). El icono queda marcado |
| El fondo de un grupo | Nuevo acceso directo..., Buscar programas..., Organizar iconos (por orden alfabético; se guarda en CENTRO.INI) |
| El fondo del Centro | Nuevo grupo..., Buscar programas... |
| La barra de título (o sus botones) de cualquier ventana | Restaurar, Minimizar, Maximizar y Cerrar: las que tiene la ventana; en gris la que no toca (Restaurar si no está maximizada ni minimizada...) |
| El icono de una ventana minimizada (abajo) | el mismo de la barra de título |
| El escritorio (el fondo) | Configuración..., Fondo del escritorio... (Configuración en el apartado Pantalla), Mezclador...; Centro de programas (lo trae delante; si está minimizado, lo restaura), [ARCHIVOS] Administrador de archivos..., [TAREAS] **Administrador de tareas...** (lo abre o, si ya está abierto, lo trae delante; a diferencia de Ctrl+Alt+Supr, no para a nadie ni ofrece reiniciar); Ventanas en cascada, Ventanas en mosaico; Salir de Hispa Ventanas... (el mismo cuadro del botón «Salir»: volver a Hispa-DOS, reiniciar o apagar) |
| Un cuadro de texto (Bloc de notas, campos de los diálogos) | Cortar, Copiar, Pegar, Borrar, Seleccionar todo (con sus teclas al lado; en gris lo que no se puede: sin selección, con el portapapeles vacío o sin texto). Deshacer no está: los cuadros de texto no lo tienen |
| Pintura | el botón derecho sigue siendo el color secundario: no hay menú con el ratón; con Mayús+F10, Deshacer, Rehacer, Cortar, Copiar, Pegar, Borrar, Seleccionar todo e Intercambiar los colores |
| Mezclador | Vista sencilla / completa (en gris la que se ve), Avanzado..., Valores de fábrica... |
| Reproductor de sonido | Sonido de prueba, Tono 440 Hz, Abrir WAV..., Parar (en gris si no suena), Mezclador... |
| La familia de oficina (Pluma, Cuentas, Facturas, Datos, Lector, Publicaciones, Presentaciones, Mirador) | el de lo que hay bajo el puntero: el texto, la celda, la factura, el registro, la página, el objeto, la imagen (ver «El botón secundario» en `OFICINA.md`) |
| Las demás ventanas (los juegos, la Calculadora...) | con el ratón, nada (el botón derecho es suyo: el Campo de minas pone banderas); con el teclado, el de la barra de título |

**Ventanas en cascada** pone las ventanas visibles en escalera, de 26 en 26
puntos, sin cambiar cuál está delante (las que cambian de tamaño, dentro de
la pantalla); **en mosaico** reparte la pantalla en una cuadrícula (tantas
columnas como la raíz del número de ventanas, por arriba): las que cambian
de tamaño ocupan su casilla (sin bajar de su tamaño mínimo) y las demás
solo se ponen en su sitio. Las maximizadas se restauran antes; las
minimizadas y las ocultas no se tocan.

**Cómo funciona** (y cómo lo usa un programa):

1. El botón derecho llega a la ventana como `WM_RBUTTONDOWN` y
   `WM_RBUTTONUP` (como antes). Si la ventana manda `WM_RBUTTONUP` a
   `ven_defecto` (lo normal: no lo atiende), `ven_defecto` le manda
   **`WM_CONTEXTMENU`** (0x007B): ECX = la ventana donde se pidió, EDX =
   x + y×65536 **de la pantalla**. Con el teclado, `WM_CONTEXTMENU` va a la
   ventana con el foco con **EDX = -1**.
2. Una hija que no lo atiende lo pasa a su madre (`ven_defecto`), con el
   mismo ECX: el Mezclador recibe el de sus columnas. Una ventana de primer
   nivel que no lo atiende no hace nada con el ratón y, con el teclado,
   abre el menú de su barra de título.
3. Quien lo atiende abre su menú con **`ctx_menu`** (EBP = la ventana a la
   que irá la orden, ESI = las órdenes con el formato de un desplegable de
   la barra: `dw número`, `db "&Texto", 0`, ..., `dw 0` + `db 0` para una
   raya y `dw 0xFFFF` al final; EAX = x, EBX = y de la pantalla, o **EAX =
   -1**: donde se pidió, el ratón o, con el teclado, la esquina de la zona
   de la ventana; ECX = las órdenes en gris, un bit por orden contando las
   rayas). No espera: la orden elegida llega después como **`WM_COMMAND`**
   con ECX = su número, igual que las de la barra (así un menú contextual
   puede usar los mismos números que el menú de la ventana). Con Esc o un
   clic fuera no llega nada.

Las órdenes del menú de la barra de título (números `SC_*`) y las del
escritorio, de los cuadros de texto y de los grupos las atiende Hispa
Ventanas; un programa no las ve.

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
| `WM_GETMINMAXINFO` | [TAMAÑO] antes de cambiar el tamaño con el ratón | → EAX=ancho mínimo + alto mínimo×65536 (0: 140×70) |
| `WM_EXITSIZEMOVE` | [TAMAÑO] se suelta la ventana que se movía o cambiaba de tamaño con el ratón | ECX=1 si ha cambiado el tamaño |
| `WM_ENTERMENULOOP`, `WM_EXITMENULOOP` | se abre y se cierra un menú de la ventana (de la barra o contextual) | |
| `WM_VSCROLL`, `WM_HSCROLL` | [TAMAÑO] se ha movido una barra de desplazamiento hija | ECX=posición nueva, EDX=la barra |
| `WM_MOUSEWHEEL` | [TAMAÑO] la rueda del ratón, sobre la zona (la que no lo quiere lo pasa a su madre) | ECX=pasos (positivo: hacia abajo), EDX=x + y×65536 |
| `WM_SETFOCUS`, `WM_KILLFOCUS` | toma o pierde el foco del teclado | |
| `WM_KEYDOWN` | tecla que no escribe (flechas, Intro, Tab, Esc, Supr, F1…) | ECX=tecla (`TC_*`; Ctrl+letra = `TC_CTRL`+1…26), EDX=mayúsculas (`TM_*`) |
| `WM_CHAR` | carácter (página 437, con el teclado de KEYB) | ECX=carácter |
| `WM_MOUSEMOVE`, `WM_LBUTTONDOWN`, `WM_LBUTTONUP`, `WM_LBUTTONDBLCLK` | ratón sobre la zona de la ventana (o capturado) | EDX=x + y×65536 (con signo, en la zona), ECX=mayúsculas |
| `WM_RBUTTONDOWN`, `WM_RBUTTONUP` | se aprieta el botón derecho sobre la zona de la ventana (desde [CONTEXTO], la activa) y se suelta; `ven_defecto` convierte `WM_RBUTTONUP` en `WM_CONTEXTMENU`. Mientras está apretado, el ratón es de esa ventana (recibe los `WM_MOUSEMOVE` aunque salga de ella), como con el izquierdo | lo mismo |
| `WM_CONTEXTMENU` | [CONTEXTO] se pide el menú contextual (botón derecho que no atiende la ventana, Mayús+F10, tecla de menú); la hija que no lo quiere lo pasa a su madre | ECX=la ventana donde se pidió, EDX=x + y×65536 de la pantalla (-1: con el teclado) |
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
| `c_barra` | [TAMAÑO] barra de desplazamiento (en `tamano.inc`), vertical o con `BDS_HORIZ` horizontal, de `BD_AN` (16) de grueso; la posición va de 0 al máximo | `BDM_PONER` (ECX=máximo, EDX=página: lo que se ve), `BDM_POS` (ECX=posición, sin aviso), `BDM_LEER` (→ EAX), `BDM_MOVER` (ECX=`BD_LIN_ARR`, `BD_LIN_ABA`, `BD_PAG_ARR`, `BD_PAG_ABA`, `BD_INICIO`, `BD_FIN`: como si se pulsara, con aviso: para las teclas), `BDM_LINEA` (ECX=lo que avanza una flecha o un paso de la rueda; 1); avisa con `WM_VSCROLL` / `WM_HSCROLL` |

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
Cuando se abre un menú suyo (de la barra o contextual) recibe
`WM_ENTERMENULOOP`, y cuando se cierra, `WM_EXITMENULOOP` (antes que el
`WM_COMMAND` de la orden elegida, si se ha elegido alguna). El desplegable
es una ventana del sistema y la tarea de la ventana sigue a lo suyo: un
programa que trabaja todo el rato (un emulador) se para con el primero y
sigue con el segundo. Los dos van a la cola, como `WM_COMMAND`.

Con el ratón, un clic en cualquier título de la barra abre su desplegable
(hasta noviembre de 2026 solo el primero respondía bien: `menu_saltar`
estropeaba la x del clic).

### Diálogos (`dialogos.inc`)

| Función | Registros |
|---|---|
| `dlg_mensaje` | EBP=dueña ESI=texto EDI=título EAX=`DM_ACEPTAR`, `DM_ACEPTAR_CANCELAR`, `DM_SI_NO_CANCELAR` o `DM_SI_NO` (solo «Sí» y «No»: Esc y el botón de cerrar dan `ID_CANCELAR`) → EAX=`ID_ACEPTAR`, `ID_CANCELAR`, `ID_SI`, `ID_NO` |
| `dlg_pedir` | EBP=dueña ESI=pregunta EDI=título EBX=búfer ECX=tamaño → EAX=1 si se acepta |
| `dlg_archivo` | EBP=dueña EDI=título EBX=búfer del nombre → EAX=1; lista la carpeta actual y deja entrar en las carpetas |
| `dlg_lista` | (en `centro.inc`; para los módulos, la entrada 96 de la interfaz, desde la 0.9.22) EBP=dueña EDI=título ESI=pregunta EBX=textos, uno cada EDX octetos, ECX=cuántos → EAX=el elegido (-1: ninguno) |
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
- **Reproductor de sonido** (`programas/sonidos.asm`, `SONIDOS.HVP`, en
  «Accesorios»): para saber enseguida si un equipo suena (ver «Reproductor
  de sonido», más abajo).
- **Hispa-Nes** (`programas/hispanes.asm`, `HISPANES.HVP`, en «Juegos»): el
  emulador compatible con los cartuchos de la NES, con su juego Cometas
  (`HISPA1.NES`). Tiene su documento: [HISPANES.md](HISPANES.md).
- **Hispa-System** (`programas/hispasms.asm`, `HISPASMS.HVP`, en «Juegos»):
  el emulador compatible con los cartuchos de la Master System, con su juego
  Buzo (`BUZO.SMS`). Tiene su documento: [HISPASMS.md](HISPASMS.md).

## Programas aparte: los módulos .HVP

`VENTANAS.COM` no puede crecer mucho más (lo que usa el modo real tiene que
caber en 64 KB), así que los programas nuevos van **aparte**, cada uno en su
archivo `.HVP` («programa de Hispa Ventanas»). Los primeros son los juegos,
`CULEBRA.HVP` y `MINAS.HVP` (y la segunda tanda: `BLOQUES.HVP`,
`SOLITARI.HVP`, `TRAGABOL.HVP` y `ALMACEN.HVP`), en `\HISPADOS\JUEGOS`, y
Pintura, `PINTURA.HVP`, y el Reproductor de sonido, `SONIDOS.HVP`, en
`\HISPADOS\PROGS`.

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
| 28 | 4 | el grupo del Centro al que va su acceso directo: 0 «Juegos», 1 «Accesorios», 2 «Sonido» ([MEZCLADOR]) |
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

### Programas de Windows de 32 bits (Hispa-486)

Un `.EXE` de Win32 **de ventanas** (formato PE, subsistema 2) también se abre
como una ventana, no saliendo a Hispa-DOS: `es_hvp` dice que sí tanto para un
.HVP como para un PE de ventanas (`es_pe_gui` mira la cabecera del archivo),
y `hvp_abrir` pasa los segundos a `pe_abrir` (`ventanas/win32.inc`, en la
parte alta). Es la fase 3 de Hispa-486; el detalle está en
`docs/486/HISPA-486.md` («Cómo está hecho (fase 3a)», «(fase 3b)» y «(fase
3c)»). Lo que importa aquí:

- El programa corre como **una tarea más**, pero con **sus propios
  selectores** (código, datos y pila, FS) y **su propia pila**, dentro de un
  bloque de DPMI. Las fichas están en `pe_tabla` (`PE_MAX` = 4), aparte de
  `hvp_tabla`: para el Administrador de tareas es una tarea sin módulo (sale
  con el título de su ventana; se puede terminar, pero no arrebatarle el
  control con INT 3).
- Sus llamadas a Windows entran por `pe_despacho` (un salto lejano desde un
  trampolín), que pone los segmentos de Hispa Ventanas y pasa a la pila de
  Hispa Ventanas de la tarea antes de llamar a la función. **`tar_ceder` no
  ha cambiado**: solo se cede el turno dentro de esas funciones, ya con los
  segmentos y la pila de siempre, y mientras tanto no queda cargado ningún
  selector del programa (FS y GS incluidos).
- Las funciones (`w32_tabla`) reciben EBP = los argumentos y usan la memoria
  del programa sumando `PE_DELTA` a sus punteros. `MessageBoxA` es
  `dlg_mensaje`.
- `hvp_limpiar` llama a `pe_limpiar`, que suelta los selectores y el bloque
  de los programas que han acabado (o que ha terminado el Administrador de
  tareas).
- (Fase 3b, `ventanas/win32u.inc`.) **Sus ventanas son ventanas de Hispa
  Ventanas** con el procedimiento `w32_ven_proc` y lo de Windows en `V_D0` a
  `V_D6` (el programa, su `WndProc`, su clase, su estilo, su bloque con el
  título...). El marco y el reparto del ratón y del teclado son los de
  siempre; `GetMessageA` es `msg_mirar` más la traducción de los mensajes a
  los de Windows, y cede el turno con `tar_ceder`.
- **`pe_llamar`** es el camino contrario a `pe_despacho`: de Hispa Ventanas
  al `WndProc` del programa, en la pila del programa, y de vuelta por un
  trampolín (`pe_volver`). Solo se usa **en la tarea del programa**. Como
  `msg_enviar` llama al procedimiento de una ventana desde la tarea que sea
  (el sistema, al arrastrarla o al cambiar la activa), `w32_ven_proc` deja lo
  que le llega desde otra tarea en la cola de la suya, con la marca
  `W32_DIFERIDO` en `M_MSG`, y se entrega cuando el programa mira su cola.
  (`M_MSG` puede llevar también `W32_CRUDO`: un mensaje de `PostMessageA`,
  que ya es de Windows. Las dos marcas son bits altos que ningún mensaje
  usa; `msg_purgar` y `msg_despachar` no las miran.)
- **El dibujo** va por las `dc_*`: cada función de GDI abre el contexto de la
  ventana, dibuja y lo cierra, porque el contexto de Hispa Ventanas es uno
  solo y no vale de un turno a otro. Los colores RGB pasan al cubo de 6×6×6
  y a los grises de la paleta.
- La memoria que pide del montón (el bloque del programa y el de cada
  ventana) queda **apuntada a su tarea**, como la de los módulos
  (`tar_w_pedir`): si se termina a la fuerza, `tar_soltar_de` la suelta.

- (Fase 3c.) **KERNEL32 y las DLL son cosa de HW32**: `pe_abrir` carga
  `HW32.COM` como módulo del programa (`pe_modulo`: una copia en memoria
  extendida, con sus dos selectores) y le pide que cargue el programa con sus
  DLL. Las llamadas a KERNEL32 van a esa copia sin pasar por Hispa Ventanas;
  HW32 pide lo del entorno (USER32, GDI32, `Sleep`, salir, los avisos) por
  tres entradas lejanas: `pe_resolver`, `pe_salir_mod` y `pe_falta_mod`. Ojo
  en ellas: llegan con la pila de HW32 o la del programa (nada de `[EBP]` ni
  `[ESP]` hasta cambiar de pila). `pe_limpiar` llama a `mod_fin` de HW32, que
  suelta todo lo del programa. Sin `HW32.COM` en el disco, los programas de
  Windows de ventanas no se abren (y lo dicen).
- (Fase 3c, `ventanas/win32c.inc`.) **Sus controles son los de
  `controles.inc`**: un BUTTON es un `c_boton`, etc., con `V_PROC` =
  `w32_ctl_proc` (que llama al procedimiento de la clase y añade `EN_CHANGE`)
  y su texto en un bloque del montón. Como `V_D0` a `V_D7` son de la clase,
  el control es del programa de su tarea. El aviso a la madre (`avisar`:
  `WM_COMMAND` con el control en EDX) llega al programa con el HWND del
  control en `lParam`. El COMBOBOX es una clase nueva, `c_w32combo`, que abre
  un `ctx_menu`.
- (Fase 3c, `ventanas/win32m.inc`.) **Su barra de menús es la de
  `menus.inc`**: de las fichas de sus HMENU se hace la tabla de `V_MENU` (en
  un bloque del montón; en `V_D7`, el HMENU). Lo único que se ha tocado fuera
  es `menu_abrir`, que llama a `w32_menu_abierto` para saber qué órdenes van
  en gris (`menu_gris` y `ctx_gris`, que mira `ctx_es_gris`). **Sus diálogos
  de plantilla** son ventanas de Windows con la marca `W32V_DIALOGO` y los
  controles que diga la plantilla de sus recursos; el modal tiene su propio
  bucle (`msg_tomar` y `msg_despachar`), como `dlg_modal`.

Un `.EXE` de Win32 **de consola** sale a Hispa-DOS como los demás programas,
pero `acc_lanzar` lo marca (`pe_marca_consola`: bit 4 de `lz_band`) y el
lanzador lo ejecuta con `COMSPEC /C` y sin quitar HSPDPMI de la memoria:
COMMAND.COM lo pasa a HW32, que necesita el servidor DPMI.

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

Añadidas para Hispa-Nes (76 entradas: cuatro más, de la 72 a la 75):

| Entradas | Para qué |
|---|---|
| `ceder` | da el turno a los demás programas y al sistema (ratón, teclas, pantalla) y vuelve en cuanto le toca otra vez, sin esperar a un mensaje. Para un programa que tiene algo que hacer todo el rato (un emulador): su bucle mira los mensajes con `msg_mirar`, hace un poco de lo suyo y llama a `ceder`. Mientras alguien la usa, Hispa Ventanas no duerme entre interrupciones: cuando no haya nada que hacer hay que volver a `msg_tomar` |
| `tec_mapa` | (datos) 128 octetos, uno por tecla (su código de teclado): 1 mientras está pulsada. Con `WM_KEYDOWN` no se sabe cuándo se suelta una tecla; aquí sí. Hispa Ventanas lo apunta en modo real (INT 15h, AH=4Fh: el aviso de la BIOS por cada código que llega del teclado, que también reciben las teclas que mandan HSPUSB y HSPJOY). Las teclas con prefijo (las flechas) usan el mismo sitio que las del teclado numérico. Vale para todas las ventanas: quien lo lee mira antes si tiene el foco (`WM_SETFOCUS`, `WM_KILLFOCUS`) |
| `snd_pcm` (ESI=muestras ECX=octetos, 2.048 como mucho, EDX=Hz, AL=formato: bit 0, 16 bits; bit 1, estéreo → CF=1 si no hay sonido) | pone un trozo de sonido en la cola de HSPSND (su función 01h), que lo toca a continuación de los anteriores. HSPSND tiene sitio para cuatro trozos y, si no cabe, la llamada espera: quien no pueda esperar tiene que llevar la cuenta de lo que ha mandado (ver docs/HISPANES.md) |
| `dos_dir_actual` (EDI=búfer de 80) | la carpeta de trabajo, con su unidad (`A:\CARPETA`): para hacer una ruta completa con el nombre que devuelve `dlg_archivo` |

Añadidas para el Reproductor de sonido (80 entradas: cuatro más, de la 76 a
la 79):

| Entradas | Para qué |
|---|---|
| `snd_info` (→ EAX=tarjeta, ESI=su nombre, EDX=versión de la interfaz de HSPSND) | la tarjeta que lleva HSPSND (su función 00h): 0 ninguna, 1 Sound Blaster de 8 bits, 2 Sound Blaster 16, 3 AC'97, 4 HD Audio; -1 si HSPSND no está cargado (y ESI y EDX no se tocan) |
| `snd_pcm2` (como `snd_pcm`, con ECX de hasta 8.192 octetos) | un trozo grande para la cola de HSPSND: lo que HD Audio y AC'97 tocan de una vez (2.048 tramas de 16 bits en estéreo). Como `snd_pcm`, espera si no cabe: para no esperar, trozos de la medida de la tarjeta (2.048 tramas a 48.000 Hz con HD Audio y AC'97, 1.024 con Sound Blaster 16, 4.096 muestras de 8 bits y un canal con las anteriores) y preguntar antes si cabe (`snd_orden` con AH=08h; ver `programas/sonidos.asm`) |
| `snd_orden` (AH=función de HSPSND, AL=dato → EAX; CF=1 si no hay sonido) | las demás funciones de HSPSND: 03h parar, 02h esperar a que acabe, 06h leer el volumen y, con la versión 3 de su interfaz, 08h: cuántos trozos caben en la cola sin esperar (AL, de 0 a 4). El volumen y el silencio se cambian con `aj_poner`, que además los apunta y repinta el altavoz |
| `snd_campana` | toca la campanita del volumen (ver «El volumen»); no espera |

Añadidas para cambiar el tamaño y desplazarse ([TAMAÑO]; 82 entradas: dos
más, la 80 y la 81):

| Entradas | Para qué |
|---|---|
| `c_barra` | (datos) la clase de la barra de desplazamiento (ver «Controles»): `HV_c_barra` en la plantilla y `hvl_clases` |
| `cur_forma` (EBP=ventana, EAX=`CUR_FLECHA`, `CUR_HORIZ`, `CUR_VERT`, `CUR_DIAG1` o `CUR_DIAG2`) | la forma del cursor mientras está sobre la zona de esa ventana (hasta que se pida otra o se destruya la ventana) |

Los mensajes nuevos (`WM_GETMINMAXINFO`, `WM_EXITSIZEMOVE`, `WM_VSCROLL`,
`WM_HSCROLL`, `WM_MOUSEWHEEL`, `BDM_*`) no gastan entradas: un módulo que no
los conoce los manda a `ven_defecto`, que devuelve 0 (los tamaños mínimos
normales) o pasa la rueda a la madre.

Añadidas para el Mezclador ([MEZCLADOR]; 84 entradas: dos más, la 82 y la 83):

| Entradas | Para qué |
|---|---|
| `snd_mezcla` (EAX, EBX, ECX, EDX → los mismos; CF=1 si no hay sonido o HSPSND dice que no) | las funciones 09h-0Fh de HSPSND, el mezclador por fuentes (docs/MEZCLADOR.md). Con 0Bh (poner una fuente) y 0Eh (valores de fábrica), Hispa Ventanas vuelve a leer el volumen de General y repinta el altavoz |
| `mez_abrir` (EAX=1: en la vista Completa) | abre el Mezclador (`PROGS\MEZCLAD.HVP` al lado de VENTANAS.COM) o trae su ventana delante |

Añadida para el botón derecho ([CONTEXTO]; 85 entradas: una más, la 84):

| Entradas | Para qué |
|---|---|
| `ctx_menu` (EBP=dueña ESI=órdenes EAX=x EBX=y de la pantalla, EAX=-1: donde se pidió; ECX=las que van en gris, un bit por orden) | abre un menú contextual (ver «Botón derecho»); la orden elegida llega a la dueña como `WM_COMMAND` (ECX=número) |

Añadida para los emuladores ([EMULADORES]; 86 entradas: una más, la 85):

| Entradas | Para qué |
|---|---|
| `dos_ir_carpeta` (ESI=ruta, con unidad o sin ella → CF=1 si no se puede, y entonces no cambia nada) | la carpeta de trabajo (y la unidad, si la ruta la lleva) pasa a ser esa: así `dlg_archivo` empieza donde se quiera. Quien la cambia la deja luego como estaba (`dos_dir_actual` antes): es de todos los programas |

Los mensajes `WM_ENTERMENULOOP` y `WM_EXITMENULOOP` (un menú de la ventana se
abre y se cierra) tampoco gastan entrada: quien no los conoce los manda a
`ven_defecto`, que no hace nada.

El mensaje `WM_CONTEXTMENU` no gasta entrada: un módulo que no lo conoce lo
manda a `ven_defecto` (a la madre o, con el teclado, el menú de la ventana).
`AJ_CTRL_DER` (15) se lee y se cambia con `aj_leer` y `aj_poner`.

`ventanas/hvpmez.inc` (como `hvplib.inc`, dentro del módulo) tiene lo común
del mezclador para los módulos: leerlo todo, poner una fuente o un ajuste y
guardar la sección `[Mezclador]` de `\HISPADOS\SONIDO.INI`.

Añadidas para el Administrador de archivos ([ARCHIVOS]; 90 entradas: cuatro
más, de la 86 a la 89, detrás de `dos_ir_carpeta`, la 85, que viene de los
emuladores; `ventanas/archapi.inc`, en la parte alta):

| Entradas | Para qué |
|---|---|
| `rm_interrup` (ESI=bloque de registros `RL_*` de `defs.inc`: `RL_EAX`, `RL_EBX`, `RL_ECX`, `RL_EDX`, `RL_ESI`, `RL_EDI` y el octeto `RL_INT`) | una interrupción de modo real (INT 21h, INT 11h...) con esos registros y DS=ES=Hispa Ventanas: los punteros van dentro de `dos_buf`. A la vuelta el bloque trae los registros que deja la interrupción y `RL_BAND` sus banderas; CF=1 si deja el acarreo (el error de DOS, en `RL_EAX`). Con ella el módulo hace lo que no tiene `dos.inc`: atributos (43h), cambiar de nombre y mover (56h), carpetas (39h, 3Ah), buscar con su propia DTA (1Ah, 4Eh, 4Fh), espacio libre (36h), fecha de un archivo (57h), unidades (4408h) |
| `dos_buf` | (datos) el búfer de paso de DOS (2.048 octetos, en memoria convencional): un puntero a él vale igual en modo real |
| `dos_crit` (→ EAX) | el último error crítico de disco (INT 24h) desde la última vez que se preguntó: el código que da DOS a INT 24h (0 protegido contra escritura, 2 unidad no preparada...) o -1 si no ha habido. El manejador de INT 24h del lanzador sigue haciendo fallar la llamada sin preguntar, pero ahora apunta el error |
| `prog_abrir` (EBP=ventana ESI=programa EDI=parámetros (0: ninguno) EDX=carpeta de trabajo (0: la del programa) ECX=banderas: 1 volver a Hispa Ventanas, 2 esperar una tecla; EBX=qué abrir al volver o 0) | abre un programa como un acceso directo: un .COM, .EXE o .BAT (Hispa Ventanas sale a Hispa-DOS por el lanzador y vuelve), un .HVP (con los parámetros) o un interno (`:BLOC`). EBX es «ruta del .HVP y parámetros»: si se sale a Hispa-DOS y se vuelve, se guarda en CENTRO.INI (`Reabrir=` en `[Centro]`) y al volver se abre ese módulo (y la clave se borra) |

Además, un acceso directo con parámetros a un programa que **ya está
abierto** le manda el mensaje `WM_HV_ABRIR` (`WM_USER`+200, EDX=los
parámetros) en vez de no hacer nada: el Bloc de notas (`:BLOC`, que ahora
abre el archivo de los parámetros también al empezar), Pintura y el
Reproductor de sonido (que ahora también acepta un WAV en los parámetros y
lo toca) abren ese archivo; los demás lo pasan a `ven_defecto`, que no hace
nada. El grupo 3 de la cabecera de un módulo es «Principal».

Añadida para la imagen de fondo del escritorio ([FONDO]; 98 entradas: la
97, detrás de `dlg_lista`, la 96; `ventanas/fondo.inc`, en la parte alta):

| Entrada | Para qué |
|---|---|
| `esc_imagen` (EAX=0: lo que hay puesto → EAX=forma, ESI=ruta, ECX=estado, EDX=número de cambio; EAX=1, ESI=ruta, EDX=forma: la pone → CF=1 y EAX=el error; EAX=2, EDI=búfer: la carpeta de VENTANAS.COM) | la imagen de fondo del escritorio: la usan Configuración (apartado Escritorio y al guardar VENTANAS.INI) y Mirador («Poner como fondo de escritorio»). Los registros, en «La imagen de fondo del escritorio» |

Un módulo compilado con menos entradas en su cabecera (63, 66, 72, 76, 80, 82, 84, 85 u 86) sigue
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

Un programa grande puede partirse en varios archivos: los `%include` que no
son de `ventanas/` se buscan en una carpeta con el nombre del fuente, a su
lado (`programas/hispanes.asm` y `programas/hispanes/*.inc`).

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
- **Solitario** (`juegos/solitario.asm`, `SOLITARI.HVP`): el solitario
  de columnas («Klondike») con dos barajas dibujadas aquí, a elegir en
  «Juego > Baraja francesa / Baraja española» (la elegida lleva «»» en el
  menú, se recuerda en `SOLITARI.REC` y al cambiarla empieza otra partida).
  Por omisión, la **baraja francesa de 52 cartas** (corazones y diamantes
  en rojo, picas y tréboles en negro; A, 2 a 10, J, Q y K): siete columnas
  de 1 a 7 cartas, tapadas menos la de encima, y las otras 24 en el mazo.
  La **española de 40** (oros y copas, claros, con el número en terracota;
  espadas y bastos, oscuros, en tinta; as, 2 a 7, sota 10, caballo 11 y
  rey 12): seis columnas (21 cartas, 19 en el mazo). El mazo se pasa de una
  en una y se le da la vuelta las veces que se quiera. En las columnas se
  baja de una en una, del rey al as, alternando los colores (rojo y negro;
  claros y oscuros); a un hueco solo va un rey. Arriba, cuatro montones de
  salida, uno por palo, del as al rey: se gana con todas. Las cartas
  (72×100, sobre tapete azul petróleo) imitan las de verdad con diseño
  propio: blancas, esquinas redondeadas y filete gris; el índice (número o
  letra en negrita y la pinta pequeña) arriba a la izquierda y, girado,
  abajo a la derecha; las pintas en su sitio de siempre, las de la mitad de
  abajo del revés; el as, con una pinta grande; las figuras, en un
  recuadro (las francesas, de dos cabezas; las españolas, de cuerpo entero
  con su palo debajo); el dorso, añil con margen blanco, celosía y un
  rombo ocre. El rojo y el negro de la francesa son los puros del cubo de
  la paleta de 256 colores (en la de 16, terracota y tinta). **La ventana
  es como las demás** ([TAMAÑO]): se cambia de tamaño por los bordes y las
  esquinas (lo más pequeño, 444×415) y se maximiza, y las cartas tienen tres
  escalas: grande (72×100), mediana (64×88, pintas de 9 y figuras a 1,5) y
  pequeña (56×76, pintas de 7 y figuras a su tamaño); se usa la mayor con
  la que caben las siete columnas a lo ancho y, a lo alto, una columna de
  seis tapadas y trece destapadas que asomen 13 puntos. Si una columna no
  cabe, sus cartas se juntan. Al abrirla, el sitio y el tamaño guardados
  (`Ventana=x,y,ancho,alto` en `SOLITARI.REC`, al soltarla tras moverla o
  cambiarla y al cerrarla; si estaba maximizada, los de antes) si caben en
  la pantalla; si no, la mayor escala que quepa, arriba y centrada (en
  800×600, 588×567 con la grande; en 640×480, 516×450 con la mediana). Ratón: arrastrar una carta o una
  escalera (si la jugada no vale, vuelve a su sitio), clic en el mazo,
  doble clic o botón derecho para subir una carta. Teclado: F2 nueva,
  Retroceso o Ctrl+Z deshacer (512 pasos), Espacio sacar carta, S subir las
  que se puedan. A la vista, movimientos, tiempo (corre desde el primer
  movimiento; no, minimizada) y cartas del mazo. `SOLITARI.REC`:
  `Baraja=francesa` (o `española`), y los récords de cada baraja: los de la
  española con las claves de antes, `Jugadas=`, `Ganadas=`, `Tiempo=` (el
  mejor, en segundos) y `Movimientos=` (los menos), y los de la francesa
  con `Francesa jugadas=`, `Francesa ganadas=`, `Francesa tiempo=` y
  `Francesa movimientos=`.
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
Solitario, `/S=n` (reparto fijo) y `/P` (partida casi acabada, con la
baraja elegida: faltan cuatro cartas); Tragabolas, `/S=n`, `/P` (todo avanza un paso con cada
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

### Reproductor de sonido

`programas/sonidos.asm`, `SONIDOS.HVP` (en `\HISPADOS\PROGS`; acceso en
el grupo «Sonido»; en un CENTRO.INI de antes, en «Accesorios»). Para probar el sonido de un equipo y saber enseguida si
suena. Diseño, textos y sonidos propios.

- Arriba dice **qué tarjeta** lleva HSPSND y cómo suena con ella, o avisa:
  «HSPSND no está cargado» (con lo que hay que hacer: «Cargue HSPSND antes
  de abrir Hispa Ventanas...») o «HSPSND no ha encontrado ninguna tarjeta de
  sonido».
- **Sonido de prueba**: toca `\HISPADOS\HISPA.WAV` (si no está ahí, lo busca
  al lado del módulo), una melodía de 4,6 s hecha por `tools/mksample.py`
  (un arpegio de do mayor, una frase y un acorde; 11.025 Hz, 16 bits,
  mono, para que quepa holgado en el disquete; 101 KB). **Izquierda** y **Derecha**:
  el mismo sonido por un solo altavoz (con una Sound Blaster de 8 bits, que
  es mono, lo dice y suena por los dos). La primera vez se lee del disco
  («Cargando... 40 %», sin parar la ventana) y se queda en memoria.
- **Tono 440 Hz**: dos segundos de un «la» que se hace en el momento (no
  necesita ningún archivo: suena aunque falte HISPA.WAV).
- **Abrir WAV...**: cualquier WAV PCM de 8 o 16 bits, mono o estéreo, de
  8.000 a 48.000 Hz (se salta las partes que no son `fmt ` ni `data`). Si no
  es un WAV, no es de esos formatos o no hay memoria para él, lo dice en la
  barra de estado.
- **Parar**, el **volumen** (un deslizador y la casilla «Silencio»: son los
  de Hispa Ventanas, `aj_leer` y `aj_poner`; al soltarlo suena la campanita
  si no está sonando otra cosa) y, abajo, una barra con por dónde va y la
  **barra de estado**: «Sonando... sonido de prueba (22050 Hz, 16 bits,
  estéreo)», «Terminado.», «Parado.» o el error.

**Sin esperar al sonido.** HSPSND toca trozos uno detrás de otro (tiene
sitio para cuatro) y hace esperar a quien le manda otro si no cabe; además,
cada trozo que no llena lo que la tarjeta toca de una vez se completa con
silencio. Por eso el Reproductor tiene el sonido entero en memoria (un
`mem_bloque`) y lo manda con `snd_pcm2` en trozos **de la medida exacta y en
el formato de la tarjeta**: 2.048 tramas de 16 bits en estéreo a 48.000 Hz
con HD Audio y AC'97 (el paso a 48.000 Hz se hace aquí, interpolando entre
muestras), 1.024 tramas con una Sound Blaster 16 (a la frecuencia del sonido,
44.100 Hz como mucho) y 4.096 muestras de 8 bits y un canal con las
anteriores (22.050 Hz como mucho). Antes de cada trozo pregunta a HSPSND si
cabe (función 08h) y, si no, lo deja para la vuelta siguiente: nunca espera
y la cola va llena. Detrás del sonido manda un tercio de segundo de silencio
y, cuando ha sonado todo, le dice a HSPSND que pare (HD Audio, si no,
repetiría sus últimos cuatro trozos sin parar). (Con un HSPSND anterior, sin esa función, lleva la
cuenta con el reloj —el contador 8254, como Hispa-Nes— de cuánto queda en
la cola y solo manda otro cuando quedan tres trozos o menos.) Con `HSPSND
/SB` (la Sound Blaster emulada) la interfaz de HSPSND espera siempre a que
suene cada trozo: suena bien, pero la ventana va a tirones. Las variables
del módulo están a más de una página de su código: se escriben en cada
trama y, pegadas al código, el procesador (y QEMU más) lo toma por código
que se cambia a sí mismo y va lentísimo. Mientras
suena o carga, su bucle es `msg_mirar`, un paso y `ceder`; parado,
`msg_tomar`.

Pendiente: una lista de los últimos archivos, repetir, y los WAV de 24 bits
o comprimidos.

### Administrador de archivos

[ARCHIVOS] `programas/archivos.asm` (y `programas/archivos/*.inc`),
`ARCHIVOS.HVP` (unos 50 KB, 12 de ellos los iconos), en `\HISPADOS\PROGS`;
su acceso sale en «Principal» (el 3 de la cabecera), y también se abre desde
el menú del botón derecho del escritorio («Administrador de archivos...»).
Pedido de Carlos: «En ventanas, ¿tenemos explorador de archivos?».

- **La ventana** se cambia de tamaño y se maximiza (mínimo 380×260), y
  recuerda en `ARCHIVOS.INI` (al lado del módulo) su sitio, su tamaño, si
  estaba maximizada, la vista, el orden, el ancho del árbol y la carpeta.
  Arriba, la barra de menús (Archivo, Edición, Ver, Ayuda) y la de
  dirección: la ruta, que se puede escribir (Intro va, Esc la deja como
  estaba), y el botón «Subir». Abajo, la barra de estado: cuántos objetos
  (o cuántos elegidos) y lo que ocupan, y el espacio libre de la unidad.
- **A la izquierda, el árbol**: las unidades (A:; B: si la BIOS dice que hay
  dos disqueteras; de C: a Z: las que da DOS con 4408h: «Disco» las fijas y
  «Extraíble» las demás, que pasan a llamarse «CD» cuando se ve que son de
  ISO 9660) y sus carpetas. El cuadradito (o doble clic, Intro, + y -, las
  flechas izquierda y derecha) despliega o pliega; un clic en una carpeta
  la enseña en la lista. El separador entre los dos paneles se arrastra.
- **A la derecha, la lista**: vista **Iconos** (iconos propios de 32×32:
  carpeta, programa, por lotes, texto, imagen, sonido, cartucho, módulo y
  desconocido) o **Detalles** (iconos de 16×16; nombre, tamaño, tipo y
  fecha con la hora si cabe; un clic en la cabecera ordena por esa columna
  y otro, al revés). Las carpetas, siempre delante. Barras de
  desplazamiento y rueda en los dos paneles.
- **Elegir**: clic; Mayús+clic o Mayús+flechas, un trozo; Ctrl+Espacio, uno
  más o uno menos; Ctrl+clic también, si en Configuración > Ratón está
  quitado «Ctrl+clic como botón derecho» (si no, Ctrl+clic es el menú,
  como en todo Hispa Ventanas); Ctrl+E (como en el Windows en español) o
  Ctrl+A (como en los cuadros de texto y Pintura), todo; Edición > Invertir.
- **Teclado**: flechas, Inicio, Fin, Re Pág, Av Pág, Intro (abrir),
  Retroceso (subir), Tab (árbol, lista, dirección, «Subir»), una letra (al
  siguiente que empieza por ella), F2 cambiar el nombre, F3 (o Ctrl+B)
  buscar, F7 carpeta nueva (como en GESTOR), F5 actualizar, Supr eliminar (y Ctrl+Retroceso en los Mac, que
  TECLADO ya da como Supr), Ctrl+C, Ctrl+X, Ctrl+V (y Ctrl+Ins, Mayús+Ins),
  Esc deja de arrastrar. Mayús+F10 o la tecla de menú, el menú contextual.
- **Copiar, cortar y pegar** (el portapapeles de archivos es del
  Administrador: hasta 512); lo cortado sale más claro hasta que se pega.
  Pegar en la lista va a la carpeta que se ve; en el árbol, a la carpeta
  elegida. Pegar en la misma carpeta hace una copia con otro nombre
  (`NOMBR~1.TXT`).
- **Arrastrar y soltar**, con el botón izquierdo, de la lista a una carpeta
  de la lista o del árbol, y del árbol (una carpeta) a otra del árbol o a la
  lista (la carpeta que se ve). El destino sale enmarcado en terracota y la
  barra de estado dice «Mover a ...» o «Copiar a ...»: en la misma unidad se
  mueve, a otra se copia; con Ctrl apretado al soltar, se copia siempre.
- **Mover** en la misma unidad cambia de sitio cada archivo (56h, sin copiar
  los datos); una carpeta se crea en el destino, se le pasa lo de dentro y
  se quita la de antes. A otra unidad se copia y, si todo ha ido bien, se
  borra el origen (si algo se ha omitido, el origen se queda entero).
- **Copiar** guarda la fecha y los atributos. La **ventana de progreso**
  dice qué se copia y adónde, con una barra (por octetos, contados antes) y
  «3 de 10 archivos», y se cancela con su botón, Esc o su aspa (lo que se
  estaba copiando a medias se borra). Mientras está, la ventana del
  Administrador no atiende. Si el destino ya existe pregunta **Sí, No, Sí a
  todo o Cancelar**. Un error de disco se dice con palabras («El disco está
  protegido contra escritura», «La unidad no está preparada: ¿falta el
  disquete o el CD?», «El disco está lleno», un archivo de solo lectura...)
  con **Reintentar, Omitir o Cancelar**.
- **Eliminar** (Supr) pregunta antes («¿Quiere eliminar «X»?» o «estos N
  elementos», y avisa de que se borra lo de dentro de las carpetas); borra
  también los de solo lectura, ocultos y de sistema. No hay papelera.
- **Cambiar nombre** (F2) y **Nueva carpeta** piden el nombre (propone
  NUEVA, NUEVA2...) y comprueban que vale en DOS (8.3, sin espacios ni
  signos que no valen; lo pasa a mayúsculas, también la ñ y la ç).
- **Propiedades**: nombre, tipo, carpeta, tamaño (las carpetas, con lo que
  tienen dentro: archivos y carpetas), fecha y los atributos **Solo
  lectura, Oculto, Sistema y Archivo**, que se cambian (con varios
  elegidos, solo los que se tocan). De una unidad: el espacio total, libre
  y ocupado.
- **Buscar** (F3): un nombre con * y ? (en cualquier sitio: `*NOTA*.T?T`), en
  la carpeta que se ve y todas las de dentro, con la ventana de progreso;
  los resultados salen en la lista (en Detalles, la columna «Carpeta» en vez
  de «Tipo»; la dirección dice «Búsqueda de ... en ...») y con ellos se
  puede hacer todo lo demás. «Subir» vuelve a la carpeta; F5 busca otra vez.
- **Abrir** (doble clic, Intro): una carpeta entra en ella; un .COM, .EXE o
  .BAT se ejecuta con `prog_abrir`, como un acceso directo (un .BAT espera
  una tecla al acabar); antes se guarda `ARCHIVOS.INI` y, al volver, el
  Administrador se abre otra vez solo (`/R`: con lo que se ejecutó
  marcado). Un texto (TXT, INI, LOG, CFG, ASM, INC, DIZ, NFO, LST, CSV, ME,
  1ST, MD) va al Bloc de notas; un .BMP a Pintura; un .WAV al Reproductor
  de sonido; un .NES a Hispa-Nes y un .SMS a Hispa-System (los buscan en
  `\HISPADOS\PROGS` y `\HISPADOS\JUEGOS`); un .HVP se abre. Lo demás
  pregunta si se quiere ver en el Bloc de notas. «Editar» abre cualquier
  archivo en el Bloc de notas (también un .BAT).
- **Botón derecho** (y Mayús+F10, y Ctrl+clic): en un archivo o carpeta,
  Abrir, Editar, Cortar, Copiar, Pegar, Eliminar, Cambiar nombre y
  Propiedades; en el fondo de la lista, las vistas, los órdenes, Pegar,
  Nueva carpeta, Seleccionar todo y Actualizar; en el árbol, Desplegar o
  plegar, Nueva carpeta, Cortar, Copiar, Pegar, Eliminar, Cambiar nombre y
  Propiedades (en una unidad, lo que no se puede, en gris).
- Funciona en VESA y en la VGA de 16 colores (solo usa los 16 colores
  propios), también en 640×480.

Límites: 1.000 objetos por carpeta (lo demás no se ve), 600 filas en el
árbol, 512 elementos en una operación, 32 carpetas una dentro de otra al
recorrer, rutas de 66 letras (la de DOS) y, para abrir un archivo con otro
programa, 71 letras de parámetros (los de un acceso directo). Pendiente:
renombrar en el sitio (hoy es un diálogo), seleccionar arrastrando un
recuadro, desplazar solo al arrastrar cerca del borde, la etiqueta del
disco, una papelera y abrir un archivo en un programa que ya está abierto
con otro (Hispa-Nes y Hispa-System solo se traen delante).

### Mezclador

`programas/mezclad.asm`, `MEZCLAD.HVP` (en `\HISPADOS\PROGS`; acceso en el
grupo «Sonido»). El volumen de cada fuente (General, Sonidos, Música, CD,
Línea, Micrófono, Altavoz del PC) con el mezclador por fuentes de HSPSND:
columnas con deslizador vertical y balance, casillas con texto («Silencio»,
«Sonar», «Grabar»), graves y agudos si la tarjeta los tiene y una frase con
lo que falta; vistas Sencilla y Completa, Avanzado y el recorrido «Cómo se
usa». Se guarda solo en `\HISPADOS\SONIDO.INI`. Todo, en docs/MEZCLADOR.md.
También se abre con doble clic en el altavoz de la barra, con el botón
«Mezclador...» de su ventanita y con el de Configuración > Sonido.

## Pruebas

- `tests/run_fondo_tests.sh` ([FONDO]): la imagen de fondo del escritorio.
  La prueba hace sus imágenes (con `tools/mkfondo.py` las de 8 bits, y una
  de 24 bits) y compara zonas enteras de la pantalla con el BMP
  (`ventanas_check.py`: `bmp`, `bmpmos` y, para lo reducido, `bmpmedia`, el
  color medio; `trama`, el dibujo de siempre). Con el escritorio de
  Hispa-486 en 800×600 y `Fondo=`: la imagen ajustada (ampliada dos veces y
  media) detrás de la barra de tareas; el Bloc de notas se abre, se arrastra
  (donde estaba vuelve a verse el fondo, punto por punto), se maximiza (no
  tapa la barra) y se cierra; Configuración › Escritorio: «En mosaico»,
  «Centrada» (con el color del escritorio alrededor), «Sin fondo» (vuelve
  el dibujo), «Elegir imagen...» con el diálogo de abrir (un BMP de 24 bits:
  los colores de la paleta, exactos, y uno que no está, difuminado), un
  archivo que no es una imagen (lo dice y sigue la que había), «Ajustada»,
  «Estirada» y «Rellenar»;
  al salir, VENTANAS.INI con `Fondo=`, `Ajuste=` y `Escritorio=486`.
  Segunda sesión, 640×480 y el escritorio de Hispa-386 (`Fondo = ...` y
  `AJUSTE=Ajustada`, con espacios y mayúsculas): una imagen de 1000×60
  reducida a 640×38, en el centro. Tercera, VGA de 16 colores: el dibujo
  de siempre, Configuración lo explica y al guardar no se pierde la clave.
  Cuarta, sin VENTANAS.INI (sin imagen: el dibujo): Mirador, Imagen › «Poner
  como fondo de escritorio»: deja `MIFONDO.BMP`, el fondo cambia y
  Configuración lo apunta (con su sección `[Escritorio]`, sin
  `Escritorio=`). La imagen de Hispa-486 de verdad (`FONDO.BMP`, 800×600)
  se mira en `tests/run_imagen486_tests.sh`.
- `tests/run_barra_tests.sh` ([BARRA]): el escritorio de Hispa-486. Con
  `VENTANAS /486` en 800×600: el escritorio despejado con la barra de tareas
  abajo; el menú Inicio con el ratón (los grupos, las órdenes fijas, el
  submenú al parar el ratón) y con el teclado (Ctrl+Esc, flechas, Intro,
  Esc); la Calculadora y el Bloc de notas abiertos desde él, con sus botones;
  un clic en el botón trae delante, minimiza (al botón, sin icono en el
  escritorio) y recupera; maximizar no tapa la barra; el botón derecho en la
  barra (cascada) y en un botón; el Centro desde Inicio, que al cerrarse se
  esconde; y «Salir». Segunda sesión en VGA de 640×480 y 16 colores con
  `Escritorio=486` en VENTANAS.INI: el menú cabe, un programa de Windows
  (WINCALC.EXE, si hay gcc de 32 bits) sale en la barra y se minimiza desde
  su botón, y Configuración guarda VENTANAS.INI conservando la clave.
  Tercera, sin el ajuste: el escritorio de Hispa-386 de siempre (y Ctrl+Esc
  no hace nada). `GUARDAR=carpeta` deja las capturas.
- `tests/run_win32crt_tests.sh`, `run_win32ctl_tests.sh`,
  `run_win32menu_tests.sh` y `run_win32calc_tests.sh` ([Hispa-486], fase 3c):
  un Win32 de ventanas en C con MSVCRT.DLL y una DLL propia (KERNEL32 es el
  de HW32, cargado como módulo; al cerrar no queda memoria; la DLL que falta;
  sin `HW32.COM`); los controles de serie con el ratón y el teclado; los
  menús, los recursos y los diálogos de plantilla; y la calculadora
  (WINCALC.EXE), que lo junta todo. Lo común (la imagen con `HW32.COM` y
  `MSVCRT.DLL` en `\HISPADOS`, el grupo «Win32», el ratón y el teclado de
  QEMU) está en `tests/tools/win32_ventanas.sh`.
- `tests/run_win32hola_tests.sh` ([Hispa-486], fase 3b): el «Hola, mundo»
  de ventana de Win32 (`tests/progs/winhola.c`): la ventana con su título, su
  texto y sus dibujos; los clics y las teclas; `MessageBoxA` con dueña;
  moverla y cambiarle el tamaño; cerrarla con la X sin dejar memoria; dos
  copias a la vez y el Administrador de tareas.
- `tests/run_win32gui_tests.sh` ([Hispa-486], fase 3a): programas Win32 de
  ventanas dentro de Hispa Ventanas, desde un grupo del Centro hecho por la
  prueba. WINGUI.EXE: su `MessageBoxA` sale como cuadro con «Sí» y «No», Esc
  no lo cierra, y el segundo cuadro dice lo contestado, la línea de órdenes
  y «Todo bien.»; WINFALTA.EXE: «Hispa-486: falta la función
  CreateWindowExA (USER32.dll)» y sigue todo; con uno preguntando se abren
  el Bloc de notas y otro WINGUI; el Administrador de tareas lo lista, lo
  termina a la fuerza y la memoria libre queda como al principio (se compara
  la zona de la memoria de su ventana). Segunda sesión: HOLAC.EXE (de
  consola) sale a Hispa-DOS, lo ejecuta HW32 y se vuelve a Hispa Ventanas.
  `GUARDAR=carpeta` deja las capturas.

- `tests/run_archivos_tests.sh` ([ARCHIVOS]): el Administrador de archivos
  con un disquete y un disco duro hecho por la prueba: abrir, navegar por
  el árbol, vista Detalles y orden, carpeta nueva, copiar y pegar,
  arrastrar a otra carpeta (mover) y una carpeta al árbol (a otra unidad:
  copia), cortar y pegar, cambiar el nombre, la ventana de progreso, «Ya
  existe», el disco lleno, eliminar con confirmación, propiedades (solo
  lectura), buscar, abrir un texto en el Bloc de notas, el menú contextual,
  Ctrl+E, ejecutar un .COM y volver, el disquete fuera, cambiar el tamaño,
  maximizar y el menú del escritorio; los discos se miran con mtools y
  `fsck.fat`. Segunda sesión en VGA de 640×480 con el teclado. Capturas en
  `build/archivos_*.png`.

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
- `tests/run_apagar_tests.sh` (con APAGAR y REINICIAR): el cuadro «Salir»
  (captura), Esc, Alt+F4 en el escritorio, las flechas, «Volver a
  Hispa-DOS», y «Apagar el equipo» (-M pc y -M q35) y «Reiniciar el equipo»
  (-M q35), que terminan QEMU con CENTRO.INI ya guardado. Capturas en
  `build/apagar_*.png`.
- `tests/run_tamano_tests.sh` ([TAMAÑO]): con HSPRATON por la vía directa
  (rueda) y un CENTRO.INI con un grupo de 14 accesos: el grupo sale
  alargado y con la barra; el cursor cambia de forma en un borde (agarrado
  por fuera) y en una esquina; se arrastran el borde derecho y la esquina de
  arriba a la izquierda (tamaño nuevo, iconos en dos columnas); se desplaza
  arrastrando el pulgar (se ve el icono que estaba oculto), con Inicio, la
  rueda, Fin y Re Pág; CENTRO.INI guarda el tamaño y `Medida=1`. Segunda
  sesión con `/CONTORNO`: el grupo sale con el tamaño de antes, al tirar de
  un borde solo se mueve el contorno y la ventana cambia al soltar; en el
  Bloc de notas, una línea larga mueve la barra horizontal, Inicio la
  devuelve y sus flechas desplazan el texto. Capturas en
  `build/tamano_*.png`.
- `tests/run_principal_tests.sh`: el grupo «Principal». Con un CENTRO.INI
  de antes de la 0.9.22 se quitan Editar, Gestor e Información (y solo de
  «Principal»; `SinRepetidos=1`), y al volver a Hispa-DOS sale INFO; de
  fábrica, «Principal» trae Hispa-DOS, Configuración, el Administrador de
  tareas y el de archivos, y el de tareas no está en «Accesorios»; con un
  CENTRO.INI de la 0.9.22 (el Administrador de tareas en «Accesorios»)
  pasa a «Principal» (`TareasPrincipal=1`) y, llevado otra vez a
  «Accesorios» a mano, se queda; y, con capturas, el icono en la ventana
  de «Principal», el menú del botón derecho en el fondo del escritorio,
  «Administrador de tareas...» (que lo abre), «Centro de programas» (que
  lo trae delante) y «Salir de Hispa Ventanas...» (el cuadro de salir).
  `GUARDAR=carpeta` deja las capturas.
- `tests/run_contexto_tests.sh` ([CONTEXTO]): el botón derecho. En un
  icono de un grupo, su menú; «Propiedades» abre el diálogo del acceso;
  «Cambiar nombre» (con la letra subrayada) y «Eliminar» (con las flechas e
  Intro; pregunta antes); en el fondo del grupo, «Organizar iconos» (y
  CENTRO.INI con el orden nuevo); Ctrl+clic izquierdo igual que el derecho
  (por QMP, con Ctrl apretado); Esc y un clic fuera lo cierran; Mayús+F10 y
  la tecla de menú; en la barra de título, Maximizar y, con Ctrl+clic,
  Restaurar; en un icono del Centro; en el Bloc de notas, Seleccionar todo,
  Copiar y Pegar (con lo demás en gris); en el escritorio, cerca de la
  esquina (el menú sale hacia arriba y a la izquierda), «Fondo del
  escritorio...» (Configuración en Pantalla), cascada, mosaico y el menú de
  una ventana minimizada. Segunda sesión con `CtrlClic=0`: Ctrl+clic es un
  clic normal; en Pintura el derecho pinta y Mayús+F10 da su menú; los
  menús del Mezclador y del Reproductor; la casilla de Configuración >
  Ratón (que lo vuelve a poner y se guarda). Capturas en
  `build/contexto_*.png`.
- `tests/run_accesos_tests.sh`: los grupos ligados a una carpeta de juegos
  y las opciones de juego, con un disco duro que trae `C:\JUEGOS` y QEMU en
  modo `-icount` (las velocidades medidas no dependen de la carga del
  equipo). Los «juegos» son copias de `tests/progs/accjuego.asm`, que dice
  su ruta, su PSP, sus parámetros, la velocidad que mide con un bucle y si
  hay un VELOCIDAD frenando. Primera sesión, sin CENTRO.INI: el grupo
  «Juegos de DOS» sale solo con un acceso por juego (el que se llama como
  su carpeta, el único .COM, el mayor de dos .EXE, un SETUP que es lo único
  que hay y un programa suelto); se pone velocidad 386 y LOADFIX a uno y se
  lanza (corre frenado y por encima de 1000h; al volver, la velocidad es la
  normal ±10 % y VELOCIDAD no queda en memoria); otro al 25 % se acaba con
  Ctrl+C y otro deja un residente en la INT 08h (Hispa Ventanas avisa): en
  los dos la velocidad vuelve a ser la normal ±10 %; se borra un acceso a
  mano y se mira «Carpeta de juegos...». Segunda sesión, con dos juegos
  menos y uno más en el disco y un `VELOCIDAD 25%` puesto a mano antes de
  entrar: sale el nuevo, desaparece el que nadie tocó, el cambiado a mano
  se queda con el aspa, el borrado no vuelve, y tras un juego con velocidad
  el lanzador quita el freno que quedaba. Tercera sesión, con un CENTRO.INI
  de antes y sin VELOCIDA.COM: los grupos de antes siguen igual, se añade
  «Juegos de DOS» sin repetir juegos, un acceso con velocidad avisa de que
  falta VELOCIDA.COM y uno con LOADFIX solo se carga por encima de 1000h.
  Capturas en `build/accesos_*.png`.
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
  Solitario, con la francesa, reparte, pasa cartas, arrastra (también
  rojo sobre rojo y una dama a un hueco, que no valen; un rey a un hueco),
  deshace, sube un as, gana la partida de prueba y deja `SOLITARI.REC`, y
  con la española, elegida en el menú y recordada, gana la suya; se le
  cambia el tamaño con el ratón (de la grande a la mediana y a la pequeña,
  siguiendo la partida hasta ganarla), se recuerda y se maximiza, y en
  640×480 se abre entera y se gana; Tragabolas
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
- `tests/run_hispanes_tests.sh`: Hispa-Nes (ver [HISPANES.md](HISPANES.md)):
  el núcleo fuera de QEMU con ROM de prueba, Cometas paso a paso comparando
  la pantalla punto por punto, a su velocidad con sonido y la velocidad.
  Capturas de muestra en `build/hispanes_*.png`.
- `tests/run_hispasms_tests.sh`: Hispa-System (ver [HISPASMS.md](HISPASMS.md)):
  el núcleo fuera de QEMU (zexdoc, zexall y las ROM de prueba propias), Buzo
  paso a paso comparando la pantalla punto por punto, a su velocidad con
  sonido, con un mando y la velocidad. Capturas de muestra en
  `build/hispasms_*.png`.
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
- `tests/run_sonidos_tests.sh`: el Reproductor de sonido y la campanita del
  volumen, con AC'97, HD Audio y Sound Blaster 16. QEMU graba lo que sale
  por la tarjeta y se mide cada sonido (`tests/tools/audio.py rafagas`): el
  tono (2 s a 440 Hz), el sonido de prueba (4,6 s, empieza a 523 Hz, igual
  por los dos canales), «Izquierda» y «Derecha» (todo por un canal),
  «Parar», un WAV de 8.000 Hz y 8 bits, uno que no se admite, y las
  campanitas al cambiar el volumen desde el Reproductor, desde la barra del
  altavoz y desde Configuración › Sonido (más fuertes al 100 % que al 20 %;
  ninguna en silencio). Y sin HSPSND: el aviso. Capturas de muestra en
  `build/sonidos_*.png`. De cada sonido se cuentan además los cortes
  (silencios dentro del sonido: la tarjeta se ha quedado sin datos): el
  tono y los sonidos de prueba no pueden tener ninguno. Y el guion no lleva
  esperas fijas: después de cada pulsación espera a ver su efecto en la
  pantalla («Sonando...», «Terminado.», el título de la ventana), para que
  valga igual con el equipo cargado (la batería entera).
- Programas de prueba de Windows 16 bits propios (NASM + nuestra
  herramienta de cabeceras NE) para cada función de la API, con su
  resultado escrito en un archivo (etapa 4).
- Las aplicaciones de terceros (Windows 3.11, Word, Excel) solo en local, con
  las copias de cada uno; nunca en el repositorio.
