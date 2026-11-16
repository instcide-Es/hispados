# Estudio: qué se usaba en los PC de 8086 a 286 y qué se echa de menos hoy

*Hispa-8086 · noviembre de 2026 · Carlos Sanz y contribuidores*

Este estudio responde a tres preguntas antes de escribir el sistema:

1. ¿Qué programas se usaron más en los PC con 8088, 8086 y 286?
2. ¿Qué necesita y qué echa de menos hoy quien conserva una de esas máquinas?
3. ¿Qué tiene que traer Hispa-8086 de serie para que el equipo vuelva a servir?

**Cómo se ha hecho y qué valor tiene.** Es una búsqueda en la web (noviembre de
2026): artículos de historia de la informática, bases de conocimiento de los
fabricantes y foros de aficionados (VOGONS, WinWorld, VCFed). **No hemos
encontrado cifras de ventas fiables por país**, y menos de España: lo que hay
son clasificaciones puntuales, cuotas de mercado de algún año y testimonios. Lo
que sigue distingue lo que dicen las fuentes de lo que deducimos nosotros, y
las conclusiones son una propuesta de trabajo, no un dato. La lista de fuentes
está al final.

## 1. Lo que más se usó

### Hoja de cálculo: la razón para comprar un PC

- **Lotus 1-2-3** fue el programa estrella del PC. Adelantó a VisiCalc y vendió
  unas 110.000 copias en sus primeros nueve meses; mucha gente compraba el
  ordenador para usarlo, y como los fabricantes de clónicos necesitaban que
  funcionase rápido, hizo de prueba de compatibilidad de hecho. En enero de
  1989 seguía el primero de una clasificación de ventas, por delante de
  WordPerfect 5 y de las Norton Utilities, y en 1990 tenía el 54 % del mercado
  de hojas de cálculo.
- Antes y alrededor: VisiCalc, MultiPlan y SuperCalc.

### Procesador de textos: la segunda razón

- **WordStar** dominó la primera mitad de los 80 (BYTE lo daba en 1983 como
  probablemente el procesador de textos más usado en el PC).
- **WordPerfect** fue el estándar de las oficinas en la segunda mitad. La
  versión 5.1 pedía, como mínimo, un 8088 y 384 K.
- En España, estos programas se difundieron por las tiendas de informática y
  los grandes almacenes: primero WordStar, luego WordPerfect, después Word.

### Base de datos

- **dBASE** (II y III) fue la primera base de datos muy extendida en el PC.
  Sobre ella se hicieron miles de programas a medida: almacén, clientes,
  facturación.

### Utilidades y gestores de archivos

- Las **Norton Utilities** aparecen terceras en la clasificación de 1989: lo
  que más se vendía, después de la hoja y el procesador, era lo que
  recuperaba archivos borrados y arreglaba discos.
- Los aficionados citan además menús y lanzadores (Direct Access) y las
  herramientas de diagnóstico.

### Lenguajes: BASIC venía con la máquina

- **GW-BASIC** (y BASICA en los IBM) se entregaba con el sistema: era el
  lenguaje que tenía todo el mundo, el de las revistas y el de los colegios.
  Microsoft publicó en 2020 su fuente original en ensamblador de 8088 (la de
  febrero de 1983) con licencia MIT, como referencia histórica.
- Hoy existe PC-BASIC, un intérprete libre (GPL 3, en Python) que busca la
  compatibilidad total con GW-BASIC 3.23, incluidos los programas guardados
  en binario y el formato de números de Microsoft.

### Entornos gráficos: llegaron con los clónicos baratos

- El **Amstrad PC1512** (1986) traía MS-DOS 3.2, DOS Plus, **GEM** con GEM
  Paint y **Locomotive BASIC 2** (que funcionaba sobre GEM). Amstrad había
  vendido unas 300.000 unidades hacia finales de 1986. Fue el primer PC de
  muchísimas casas y pequeñas empresas europeas, y con él, el primer entorno
  de ventanas.
- **GEM** funcionaba con 512 K y CGA, EGA o Hercules.
- **Windows 1.0** (1985) pedía un 8088, 256 K, dos disqueteras y una tarjeta
  gráfica; para varios programas a la vez, disco duro y 512 K.
- **GEOS/Ensemble** era ya de otra liga: 640 K y, en la práctica, un 286.

### Juegos

- No hemos encontrado una clasificación de ventas. Las recopilaciones de la
  época coinciden en los nombres (Flight Simulator, Lode Runner, las aventuras
  de Sierra, SimCity) y en el equipo: un PC o un XT, mejor a 8 MHz, con
  Hercules, CGA o EGA.

### El sistema que había debajo

- MS-DOS 3.3 ocupaba unos **54 K** de memoria; el 3.2, unos 45 K; el 3.0 y el
  3.1, unos 38 K (datos de Microsoft). En una máquina de 256 K, cada K cuenta.
- Los programas más populares del PC y el XT se saltaban a menudo el DOS para
  ir más deprisa (escribían directamente en la pantalla): un sistema para
  estas máquinas tiene que dejarles hacerlo.

## 2. Lo que se necesita y se echa de menos hoy

Quien restaura hoy un XT o un AT no tiene el problema de 1986 (qué programa
compro), sino otros:

1. **Meter y sacar archivos.** Es, con diferencia, lo que más se pregunta. Los
   ordenadores actuales no tienen puerto serie ni paralelo, los adaptadores
   USB no suelen servir, y los disquetes de 360 K no los lee casi nada. Lo que
   se usa: una tarjeta **XT-IDE** con CompactFlash (se saca la tarjeta y se lee
   en el PC moderno), la red con **mTCP** (funciona hasta en un XT) y
   programas de un solo archivo como DDLINK (por red, por el puerto paralelo o
   por el serie).
2. **Disco duro.** Las BIOS de los XT no saben de discos IDE ni de LBA: se
   depende de la BIOS universal de XT-IDE. Las unidades Zip, por ejemplo, no
   sirven en un 8088 (su controlador pide V20, 186 o más).
3. **Un sistema que quepa.** En un 8088 sin memoria extendida no vale
   cualquier intérprete de órdenes: en FreeDOS hay que elegir la variante que
   no usa XMS. El disco de arranque mínimo son cuatro archivos.
4. **Saber qué hay dentro y si funciona.** Se piden herramientas de
   diagnóstico pequeñas, que digan qué equipo es y qué falla.
5. **Probar sin el hierro.** Emuladores de XT fieles (86Box, PCem, VirtualXT)
   para preparar discos y probar antes de ir a la máquina.

Y lo que deducimos nosotros, que no sale en los foros porque allí todo está en
inglés:

6. **Nada de esto está en español.** Ni los sistemas, ni los mensajes de
   error, ni los manuales que quedan. Para quien quiere enseñar la máquina a
   sus nietos, es la primera barrera.
7. **No hay con qué empezar.** Un XT recién restaurado arranca en un `A>` y
   nada más. Los programas de la época hay que buscarlos, no son libres, y
   cada uno se maneja de una manera.

## 3. Lo que tiene que traer Hispa-8086

De lo anterior sale la lista, por orden de importancia:

| Lo que se usaba / se necesita | Lo que trae Hispa-8086 | Nombre |
|---|---|---|
| Hoja de cálculo (1-2-3) | Hoja de cálculo propia; lee y escribe .CSV | **Cuentas** |
| Procesador de textos (WordStar, WordPerfect) | Procesador de textos propio; lee y escribe .TXT | **Pluma** |
| Base de datos (dBASE) | Fichero de fichas, con búsqueda y listados | **Fichas** |
| BASIC incluido (GW-BASIC) | Intérprete propio, compatible con GW-BASIC, en español | **Hispa-BASIC** |
| Entorno (GEM, Windows 1.0) | Entorno propio que junta todo lo anterior | **Hispa Escritorio** *(nombre provisional)* |
| Gestor de archivos y utilidades (Norton) | Gestor de archivos, recuperar borrados, revisar el disco | GESTOR, RECUPERAR, REVISAR |
| Meter y sacar archivos | Transferencia por puerto serie y paralelo desde el primer día; por red después | **PASAR** |
| Diagnóstico | Qué equipo es y qué funciona | EQUIPO |
| Juegos | Tres o cuatro juegos propios, para CGA y Hercules | (como en el resto de la familia) |
| Que quepa | Núcleo pequeño, todo en ensamblador; el sistema base, en un disquete de 360 K | |
| En español | Órdenes, mensajes, ayuda y manual en español | |

Los programas llevan los mismos nombres que en Hispa Ventanas (Pluma, Cuentas)
porque son los de la familia: cada sistema tiene los suyos, hechos para lo que
da su máquina, y cada uno abre los documentos de los demás (.TXT y .CSV son el
terreno común).

## Fuentes

Ventas y cuotas:

- [Lotus 1-2-3, ventajas y desventajas (The Silicon Underground)](https://dfarq.homeip.net/advantages-disadvantages-lotus-1-2-3/)
- [Lotus, segundo editor de software del mundo en 1983 (The Silicon Underground)](https://dfarq.homeip.net/lotus-second-largest-software-publisher-in-the-world-in-1983/)
- [Tales in Tech History: Lotus Software (Silicon UK)](https://silicon.co.uk/software/office-suites/lotus-software-history-219669)
- [Lotus 1-2-3 (DOS Days)](https://dosdays.co.uk/topics/Software/lotus_123.php)
- [30 años de hojas de cálculo en el PC (eWeek)](https://www.eweek.com/de/enterprise-apps/30-years-ago-pc-spreadsheets-bring-number-crunching-to-the-masses/)
- [Software Spotlight: WordStar (WinWorld)](https://forum.winworldpc.com/discussion/comment/183243)
- [WordPerfect 5.1 para DOS (WinWorld)](https://winworldpc.com/product/wordperfect/5x-dos)
- [Los papás del Office (Libertad Digital)](https://www.libertaddigital.com/opinion/historia/los-papas-del-office-1276238303.html)
- [Programas de los primeros PC (Tecmar News)](https://scientific-solutions.com/archives/tecmar_news/pg_2.htm)

Sistema, BASIC y entornos:

- [Memoria que usa MS-DOS, artículo Q64526 de Microsoft](https://jeffpar.github.io/kbarchive/kb/064/Q64526)
- [Microsoft publica el fuente de GW-BASIC (The Register)](https://www.theregister.com/2020/05/22/gw_basic/)
- [Microsoft Open-Sources GW-BASIC (InfoQ)](https://www.infoq.com/news/2020/06/microsoft-gw-basic-open-source)
- [PC-BASIC](https://sourceforge.net/projects/pcbasic/)
- [Amstrad PC1512 y PC1640 (DOS Days)](https://dosdays.co.uk/computers/Amstrad%20PC1000/amstrad_pc1000.php)
- [PC1512 (Wikipedia)](https://en.wikipedia.org/wiki/PC1512)
- [Locomotive Software (Wikipedia)](https://en.wikipedia.org/wiki/Locomotive_Software)
- [GEM XM (Dylan Harris)](https://dylanharris.org/past/ae/2006/prose/gem/xm.html)
- [Windows 1.01 cumple 40 años (Tom's Hardware)](https://www.tomshardware.com/software/windows/windows-1-01-was-launched-40-years-ago-but-it-didnt-start-well-microsoft-began-its-graphical-os-adventures-40-years-ago)
- [GEOS en máquinas pequeñas (VOGONS)](https://www.vogons.org/viewtopic.php?p=861435)

Lo que se necesita hoy:

- [Cómo meter y sacar datos de un PC antiguo (WinWorld)](https://forum.winworldpc.com/discussion/comment/183132)
- [XTIDE Universal BIOS (James F. MacKenzie)](https://www.jamesfmackenzie.com/software/xt-ide/)
- [Transferir archivos: mTCP, DDLINK, controladores de paquetes (VOGONS)](https://www.vogons.org/viewtopic.php?p=1372451)
- [FreeDOS en un 8088 con CF-IDE (BTTR Software)](https://bttr-software.de/forum/forum_entry.php?id=21142)
- [Diagnóstico y utilidades clásicas (VOGONS)](https://vogons.org/viewtopic.php?p=1298930)
- [Juegos de DOS por años (DOS Days)](https://dosdays.co.uk/topics/dos_games_by_year.php)
- [VirtualXT](https://phix.itch.io/virtualxt)
