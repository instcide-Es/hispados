# CUENTAS: la hoja de cálculo de Hispa-8086

*Hispa-8086 · noviembre de 2026 · Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.*

CUENTAS es la hoja de cálculo de Hispa-8086. Está pensada para un PC o un XT
con un 8088 a 4,77 MHz, 256 K de memoria y cualquier tarjeta de vídeo (MDA,
Hercules, CGA, EGA o VGA) en 80 columnas, y cabe en un disquete de 360 K.
Es un solo programa, `CUENTAS.COM`, de **16,7 K**, escrito entero en
ensamblador de 8086. Está en el disquete de Oficina, aparte del sistema.

Este documento cuenta tres cosas: qué hojas de cálculo había entre 1979 y
1987, qué hemos tomado de ellas y qué hemos hecho mejor, ahora que sabemos
más que entonces. Al final están las teclas, las funciones y el formato del
archivo.

## 1. Lo que había

Los datos vienen de los manuales escaneados de la época (bitsavers), de la
especificación de archivos de Lotus de 1984 y de artículos e historias
publicados. Cuando una cifra sale de una fuente secundaria o es dudosa, se
dice. La lista de fuentes está al final.

### VisiCalc (1979, Apple II; 1981, IBM PC)

- **Quién y cuándo**: la escribieron Dan Bricklin y Bob Frankston, de Software
  Arts, y la publicó Personal Software (luego VisiCorp). Salió para el Apple
  II el 17 de octubre de 1979, escrita en ensamblador de 6502. La versión del
  IBM PC es de 1981.
- **Memoria**: en el Apple II pedía 32 K. Intentaron que cupiera en 16 K y no
  lo consiguieron. En el PC, el manual pide al menos 64 K y una disquetera.
- **Límites**: 63 columnas (de la A a la BK) por 254 filas. Las columnas
  tenían 9 de ancho.
- **Los números**: los guardaba en decimal, con 11 cifras seguras (a veces
  12). Frankston explica que eligieron el decimal para que las cuentas
  cuadraran con las calculadoras de los contables.
- **Las fórmulas**: se calculaban **de izquierda a derecha, sin precedencia**,
  así que 9+6/3 daba 5 y no 11. Frankston lo consideró después un error. Las
  sumas se escribían `@SUM(B2...M2)`.
- **El recálculo**: iba por columnas (o por filas, con /GOR). Si una fórmula
  dependía de otra que estaba más adelante, había que pulsar `!` varias
  veces. Una referencia circular no avisaba: los valores cambiaban en cada
  pulsación.
- **Los archivos**: guardaba en su formato propio y exportaba en **DIF**.
- **Ventas**: unas 300.000 copias al principio y cerca de un millón en total.
- **Lo que se le criticaba**: la versión del PC era casi la misma que la de
  8 bits y no aprovechaba la máquina.

### SuperCalc (1981, CP/M; 1982, MS-DOS)

- La hizo Sorcim, empresa fundada en 1980. Se presentó en abril de 1981 y
  venía con el ordenador portátil Osborne 1. Pasó a MS-DOS en 1982, sin
  protección anticopia.
- Fue de las primeras en resolver las referencias circulares por iteración.
- Se vendieron más de 250.000 copias en 18 meses. Se cita que pedía 48 K y
  que la hoja era de 254 × 63, pero solo lo dice una fuente secundaria.

### Multiplan (1982, Microsoft)

- Lo escribió Doug Klunder y salió en 1982 (para PC-DOS en octubre). Se
  escribió en un p-código de C para que fuera fácil llevarlo a muchas
  máquinas: hubo un centenar de versiones.
- La hoja era de 255 filas por 63 columnas. Las referencias se escribían
  como R1C1, también las relativas (RC[-1]). Respetaba la precedencia de los
  operadores.
- Admitía referencias circulares con iteración, y el recálculo automático
  rehacía la hoja entera en cada cambio.
- Su formato de archivo era SYLK, que Microsoft nunca publicó.
- Se vendió más de un millón de copias, pero no frenó a 1-2-3.

### Lotus 1-2-3 (1983)

- Lo escribió Jonathan Sachs, en la empresa de Mitch Kapor, y salió el 26 de
  enero de 1983. La versión 1A es de abril de 1983 y la Release 2, de 1985.
- **Memoria**: pedía 192 K y dos disqueteras, y por eso no servía en el PCjr.
  El disquete del programa llevaba unos 237 K en 14 archivos.
- **Límites**: 256 columnas por 2.048 filas en la 1A, y 8.192 en la Release 2.
  Cada celda admitía 240 letras, y las columnas tenían 9 de ancho.
- **Los números**: eran de coma flotante binaria de 8 octetos, el formato del
  coprocesador 8087, con unas 15 cifras. *Esto lo deducimos nosotros*: con
  números binarios, 0,1 + 0,2 no da exactamente 0,3. En la pantalla no se
  notaba casi nunca, y no hemos encontrado que se le criticara en su época.
- **Las fórmulas**: con precedencia de operadores. Los rangos se escribían
  `B10..B17`.
- **El recálculo**: en orden «natural», es decir, primero lo que necesita cada
  fórmula. Con una referencia circular encendía el indicador **CIRC**. F9
  recalculaba.
- **Los archivos**: .WKS (y .WK1 en la Release 2), registros binarios con las
  fórmulas compiladas en notación polaca. Lotus publicó la especificación en
  1984.
- **Lo que se le criticaba**: la protección anticopia, porque había que meter
  el disquete original para arrancar. Se quitó en la versión 3.0. También el
  **año 1900 bisiesto**: el 29 de febrero de 1900, que no existió. Excel lo
  copió para ser compatible y todavía lo arrastra.
- El menú con la barra «/» lo heredó de VisiCalc.
- **Ventas**: 175.000 copias el primer año y 3,5 millones hasta 1988.

### Quattro (Borland, 1987)

- Lo escribieron Adam Bosworth, Lajos Frank y Chuck Batterman. Pedía 512 K y
  la hoja era de 256 × 8.192. No llevaba protección anticopia y leía los
  archivos de 1-2-3.
- Su manual lo explica así: «1-2-3 recalcula todas las fórmulas; Quattro,
  solo aquellas cuyos elementos han cambiado». Excel 1.0 para el Macintosh
  (1985) ya tenía un recálculo así, también de Doug Klunder.
- Lotus lo llevó a juicio por copiar sus menús, y Borland ganó.

### En España

- Lotus 1-2-3 tuvo una «Edición Española», la 2.01 de 1987, vendida con los
  Amstrad. Solo lo hemos visto en un anuncio de segunda mano.
- Se publicaron libros en castellano: de 1-2-3 (Plaza & Janés, 1986;
  Addison-Wesley Iberoamericana, 1987; McGraw-Hill, 1988) y de Multiplan
  (Osborne/McGraw-Hill, 1984).
- También circulaban los programas integrados: Symphony, Framework y Open
  Access.
- **No hemos encontrado** ninguna hoja de cálculo hecha en España en los 80, ni
  un Multiplan con las pantallas en castellano. Las versiones en castellano
  eran traducciones de las órdenes y de los manuales.

### El formato DIF

Es público. Software Arts lo publicó en 1981 (Frankston, con ideas de Mitch
Kapor y otros) para pasar datos de VisiCalc a otros programas, y fue el
primer formato común de las hojas de cálculo. Un archivo DIF es texto, con
pares de líneas:

```
TABLE          VECTORS        TUPLES         DATA
0,1            0,columnas     0,filas        0,0
""             ""             ""             ""
-1,0 / BOT     empieza una fila
0,123.5 / V    un número (o NA, ERROR, TRUE, FALSE)
1,0 / "texto"  un texto
-1,0 / EOD     fin
```

Solo lleva valores, no fórmulas, y el punto decimal.

## 2. Lo que tomamos de entonces

- **La pantalla de 1-2-3**: arriba, la celda y lo que tiene; en la segunda
  línea, los menús y las preguntas; luego la rejilla con letras y números, y
  a la derecha el modo (LISTO, MARCAR). Es lo que se usaba en todas las
  oficinas, y sigue siendo claro.
- **El menú con «/»**: cada opción se elige por su inicial, como en VisiCalc
  y en 1-2-3: `/G` guarda y `/FE` pone la celda en euros. También se abre con
  F10 y se puede recorrer con las flechas.
- **Los prefijos de los textos** de 1-2-3: `'` a la izquierda, `"` a la
  derecha, `^` en medio y `\-` repetido, para hacer rayas.
- **El texto largo** sigue por las celdas vacías de su derecha.
- **La sintaxis de entonces** también se entiende: `+A1*2`, `@SUM(A1...A10)`
  (VisiCalc), `@SUM(A1..A10,B2)` y `@AVG`, `@IF`, `@ROUND`, `@INT`,
  `@COUNT` (1-2-3).
- **F9 recalcula** y **CIRC avisa**, como en 1-2-3.
- **Un programa pequeño en ensamblador**, como VisiCalc y 1-2-3, para que
  vaya bien en la máquina de entonces.
- **DIF**, el formato de intercambio de VisiCalc, para leer y escribir.

## 3. Lo que hacemos mejor, y por qué

| Lo de entonces | Lo de CUENTAS | Por qué |
|---|---|---|
| 1-2-3: coma flotante binaria; 0,1 + 0,2 no es exactamente 0,3 | **Números exactos**: enteros de 64 bits que cuentan cienmillonésimas (8 decimales). 0,1 + 0,2 da 0,3 y la comparación `=0,1+0,2=0,3` da 1 | Para el dinero no puede haber céntimos fantasma. Sumar y restar es exacto. Multiplicar y dividir se hace con 128 bits y se redondea una sola vez. Caben unos ±92.230 millones |
| Redondeos que dependen de cómo se guarda el número | `REDONDEAR(2,675;2)` da 2,68, y `-2,675` da -2,68: la mitad se redondea alejándose del cero | Es el redondeo de los bancos y de Hacienda. En coma flotante, 2,675 es en realidad 2,67499… |
| VisiCalc: sin precedencia (9+6/3 = 5) | Precedencia de siempre: comparaciones; + y -; * y /; ^; el signo; y % detrás (21%) | Es lo que se enseña en la escuela |
| VisiCalc: recalcular a mano varias veces. 1-2-3: rehacer la hoja entera en cada cambio | **Recálculo inteligente**: cada fórmula guarda la caja de celdas que lee. Al cambiar algo, solo se recalculan las fórmulas que lo leen, y las que leen a esas, en el orden de las dependencias (por pasadas, sin recursión que agote la pila) | En un 8088 a 4,77 MHz se nota mucho. Es lo que hicieron Excel en 1985 y Quattro en 1987 |
| VisiCalc: los valores de una circular cambian en cada pulsación. 1-2-3: un indicador CIRC | Una referencia circular se **detecta** (en una pasada no se puede calcular nada de lo que queda). Esas celdas dan `#CIRC` y se avisa de dónde está: «Referencia circular en A10» | Nunca da vueltas sin fin, y se sabe qué celda mirar |
| Una hoja de tamaño fijo, que ocupa memoria aunque esté vacía (VisiCalc: 63 × 254) | **Hoja dispersa** de 256 × 9.999: solo ocupa memoria lo que tiene algo. El índice está ordenado y se busca por bisección | En 256 K caben unas 8.000 celdas con números (ver «Memoria») |
| Sin deshacer | **Ctrl+Z deshace** el último cambio, y otra vez lo rehace. Vale también para pegar, borrar un rango, ordenar o cambiar el formato | Un error con Supr o al pegar ya no se paga con el trabajo de una tarde |
| El fallo de 1900 bisiesto (1-2-3, y luego Excel) | Las fechas cuentan días desde el 30-12-1899: desde el 1-3-1900 dan el mismo número que las hojas de hoy, pero **sin el 29-2-1900**. Se escriben `24/11/2026` y se pueden sumar y restar | Las fechas son correctas, y las de hoy coinciden con las de Excel |
| En inglés, con punto decimal | **En español**: coma decimal, punto de los miles (`1.234,5` se entiende al teclearlo), funciones en español (SUMA, MEDIA, SI, REDONDEAR, CONTAR, ENTERO, HOY), mensajes y ayuda en castellano, euros (€) y pesetas (₧) | La máquina tiene que servir a quien no sabe inglés |
| El color como única señal (o nada en MDA) | La celda del cursor y lo marcado se ven en **vídeo inverso**; dentro de lo marcado, el cursor es un hueco (como en las hojas de hoy). Se ve igual en MDA, en Hercules y en blanco y negro | Carlos es daltónico, y muchos XT tienen monitor monocromo |
| 1-2-3: protección anticopia; formatos cerrados | Libre (GPL). Su formato **.CTA** está descrito aquí, y lee y escribe **.CSV** con «;» (el de las hojas de hoy en español y el de Cuentas de Hispa Ventanas) y **DIF** | Los datos de 1985 tienen que poder pasar a un ordenador de 2026, y al revés |
| Sin uso desde un lote | `CUENTAS hoja.csv /G hoja.dif` convierte y `CUENTAS hoja.cta /I PRN` imprime, sin pantalla | Para los .BAT, y para la prueba automática |
| La tecla `/` como única puerta | Ayuda completa con F1, una línea de teclas siempre a la vista, menús que se ven enteros y teclas de hoy: Mayús+flechas para marcar, F2 editar, Supr borrar, F3/F4 copiar y pegar | Se aprende sin manual |

**Lo que no hemos hecho, por ahora**: gráficos (el «2» de 1-2-3), base de
datos (el «3»), insertar y quitar filas y columnas, macros, nombres de
rangos, iteración de las circulares y leer los .WKS de 1-2-3. Cabría un
lector de .WKS: el formato está documentado.

## 4. Cómo se usa

```
CUENTAS [hoja] [/G destino] [/I destino]
```

Sin nada, empieza con una hoja vacía. Con un nombre, la abre. Con `/G`,
guarda en `destino` (en el formato de su extensión) y sale; con `/I`, la
imprime en `destino` (`PRN` es la impresora) y sale. En los dos casos, sin
pantalla.

| Tecla | |
|---|---|
| flechas, RePág, AvPág, Tab | moverse; Inicio, a A1; Fin, a la última celda con algo |
| Mayús+flechas, o F8 y flechas | marcar un rango (Esc lo deja) |
| escribir | un texto, un número (`1.234,5`, `21%`, `24/11/2026`) o una fórmula (`=…`, `+…`, `-…`, `@…`, `(…`) |
| Intro, Tab, ↑, ↓ | acaban de escribir y se mueven; Esc deja la celda como estaba |
| F2 | editar la celda |
| Supr | borrar lo marcado |
| F3, F4 | copiar lo marcado; pegarlo en el cursor (o repetirlo en todo lo marcado: así se llena una columna de fórmulas) |
| F5 | ir a una celda |
| F9 | calcularlo todo (también HOY()) |
| Ctrl+Z | deshacer; otra vez, rehacer |
| Ctrl+←, Ctrl+→ | la columna, más estrecha o más ancha |
| F1 | la ayuda |
| / o F10 | el menú: Abrir, Guardar, Nuevo, Deshacer, Formato, Columna, Ordenar, Imprimir, Salir |

**Fórmulas.** Las de hoy empiezan con `=` y usan la coma decimal y `;` entre
argumentos: `=SUMA(A1:A10;B3)*1,21`. Las clásicas empiezan con `+`, `-`, `@`
o `(` y usan el punto decimal y la `,`: `@SUM(A1..A10,B3)*1.21`. Los rangos se
escriben `A1:B9`, `A1..B9` o `A1...B9`. Una referencia se mueve al copiar
(`A1`) o se queda fija (`$A$1`; también `$A1` y `A$1`). Operadores: `+ - * /
^`, `%` detrás de un número, y `= <> < > <= >=` (dan 1 o 0).

| Función | | En inglés |
|---|---|---|
| `SUMA(…)`, `MEDIA(…)` o `PROMEDIO(…)`, `MAX(…)`, `MIN(…)`, `CONTAR(…)` | de rangos y valores sueltos; los textos y las vacías no cuentan | SUM, AVERAGE o AVG, MAX, MIN, COUNT |
| `SI(condición; si sí; si no)` | | IF |
| `REDONDEAR(x; decimales)` | la mitad, lejos del cero; decimales de -10 a 8 | ROUND |
| `ABS(x)`, `ENTERO(x)` | ENTERO redondea hacia abajo: ENTERO(-2,5) = -3 | ABS, INT |
| `HOY()` | la fecha del sistema | TODAY |

**Errores**: `#DIV/0` (dividir por cero), `#CIRC` (referencia circular),
`#DESB` (no cabe), `#REF` (una referencia que se sale de la hoja al copiar),
`#VALOR` (un argumento que no vale, como 2^0,5) y `#ERROR` (una fórmula
leída de un archivo que no se entiende). Una fórmula mal tecleada no se
acepta: se avisa arriba y el cursor queda donde falla.

**Formatos** (`/F`): General, Decimales fijos (de 0 a 8, con punto de los
miles), Euros (dos decimales y €), Pesetas (sin decimales y ₧), Tanto por
ciento (de 0 a 6 decimales) y Fecha (dd/mm/aaaa). El formato se pone a lo
marcado, también a las celdas vacías, para lo que se escriba después. Un
número que no cabe en la columna sale con almohadillas (`#####`): nunca se
enseña cortado. En General se quitan decimales hasta que quepa.

El € no está en la página de códigos 437. En EGA y VGA, CUENTAS dibuja la
letra al arrancar (en el sitio de la ε, 0EEh) y deja la de siempre al salir.
En CGA, MDA y Hercules no se puede cambiar la letra y sale una ε, que se le
parece. Al imprimir se escribe «EUR» (y «pts» para las pesetas), porque las
impresoras tampoco tienen el €.

**Ordenar** (`/O`): las filas de lo marcado, de menor a mayor o al revés, por
la columna que se diga (la del cursor, si no se dice otra). Van primero los
números y después los textos, sin distinguir mayúsculas, con las vocales
acentuadas junto a las otras y la ñ detrás de la n. Las vacías van siempre al
final. El orden es estable: las filas iguales se quedan como estaban. Las
referencias relativas de las fórmulas se mueven con su fila.

## 5. Archivos

Se elige por la extensión, al abrir y al guardar. Sin extensión, `.CTA`.

- **.CTA**, el propio: `HCTA`, la versión (1), cuántas columnas tienen otro
  ancho, y la columna y el ancho de cada una. Después, las celdas por orden:
  fila (dos octetos), columna, tipo (0 texto, 1 número, 2 fórmula) y formato.
  Un número lleva sus 8 octetos; un texto o una fórmula, el largo y las
  letras. Acaba en `FFFFh`. Un número ocupa 13 octetos y `=SUMA(A1:A9)`, 18.
  Los valores de las fórmulas no se guardan: se recalculan al abrir.
- **.CSV**: una fila por línea, con «;» entre celdas y fin de línea de DOS.
  Cada celda se escribe como se tecleó: las fórmulas con su `=`, los números
  con coma decimal, las fechas como fecha y los tantos por ciento con «%». La
  celda que lleva «;» o comillas va entre comillas. Al leer, el separador sale
  de la primera línea («;», el tabulador o «,»). Dentro de los paréntesis de
  una fórmula, el «;» no separa celdas. No guarda los formatos ni los anchos.
- **.DIF**: el de VisiCalc. Al escribir, los valores con punto decimal, los
  textos entre comillas y los errores como `ERROR`. Al leer, también entiende
  los exponentes (`1.5E+03`) y TRUE y FALSE.

## 6. Memoria y medidas

- `CUENTAS.COM`: **16.660 octetos**. En memoria, el programa con sus búferes y
  su pila ocupa unos 26 K.
- Lo demás se pide al sistema y se reparte así: una décima parte para cada
  búfer de deshacer, otra para el índice y el resto para las celdas (hasta
  256 K).
- Cuánto ocupa una celda: un número, 16 octetos (14, redondeados a grupos de
  4); un texto corto como «Luz», 12; una fórmula como `=SUMA(B2:D2)`, 32. A
  eso se suman 2 octetos del índice.
- En un PC de 256 K (unos 224 K libres para los programas), quedan unos
  195 K para la hoja. Caben unas 8.000 celdas con números, o unas 4.000
  fórmulas cortas.
- **Velocidad en un 8088** (calculada, no medida en una máquina real):
  - multiplicar, unos 7.000 ciclos de reloj (1,5 ms);
  - dividir, entre 45.000 y 90.000 ciclos (de 10 a 19 ms), porque se hace bit
    a bit con 128 bits;
  - pintar la pantalla entera, unas décimas de segundo.

  Por eso CUENTAS no pinta mientras queden teclas por leer, y al moverse
  sin marcar solo repinta las dos filas del cursor.

## 7. Cómo está hecho

| Archivo | |
|---|---|
| `h8086/programas/cuentas.asm` | el programa: memoria, pantalla, teclado, menús y órdenes |
| `h8086/programas/cuentas/numeros.inc` | los números de 64 bits: sumar, multiplicar y dividir con 128 bits, redondear, del texto al número y al revés, fechas |
| `h8086/programas/cuentas/hoja.inc` | las celdas dispersas, el índice, juntar la memoria libre y deshacer |
| `h8086/programas/cuentas/formulas.inc` | leer y calcular fórmulas, mover referencias al copiar y el recálculo |
| `h8086/programas/cuentas/ordenar.inc` | ordenar filas (por mezcla, estable) |
| `h8086/programas/cuentas/archivos.inc` | .CTA, .CSV, .DIF e imprimir |
| `h8086/programas/GASTOS.CSV` | una hoja de ejemplo, en el disquete de Oficina |
| `h8086/prueba/partes_cuentas.sh` | las pruebas (`SOLO="cuentas cuentas_pantalla cuentas_ordenar" ./prueba/prueba.sh`) |

Todo se ensambla con `cpu 8086`, así que NASM rechaza cualquier instrucción
de un procesador posterior. Las fórmulas se guardan como se teclearon (en
mayúsculas) y se leen al calcularlas. Así no hace falta un compilador ni un
descompilador, y el texto es el mismo en el .CSV. Lo que cuesta leerlas es
poco al lado de buscar las celdas.

## Fuentes

- [Manual de VisiCalc para el IBM PC, abril de 1982 (bitsavers)](https://ftp.mirrorservice.org/sites/www.bitsavers.org/pdf/visicorp/pc/60913-2210_VisiCalc_PC_Apr82.pdf)
- [Bob Frankston: Implementing VisiCalc](https://landley.net/history/mirror/apple2/implementingvisicalc.html)
- [VisiCalc (Wikipedia)](https://en.wikipedia.org/wiki/VisiCalc)
- [Data Interchange Format (Wikipedia)](https://en.wikipedia.org/wiki/Data_Interchange_Format)
- [Sorcim](https://en.wikipedia.org/wiki/Sorcim) y [SuperCalc (Wikipedia)](https://en.wikipedia.org/wiki/SuperCalc)
- [Multiplan (Wikipedia)](https://en.wikipedia.org/wiki/Multiplan) y [Multiplan Reference Guide, 1982 (bitsavers)](https://ftp.mirrorservice.org/sites/www.bitsavers.org/pdf/fortuneSystems/1000774-01_Multiplan_Reference_Guide_1982.pdf)
- [SYLK (Wikipedia)](https://en.wikipedia.org/wiki/Symbolic_Link_(SYLK))
- [Lotus 1-2-3 (Wikipedia)](https://en.wikipedia.org/wiki/Lotus_1-2-3) y [manual de 1-2-3, 1983 (bitsavers)](https://ftp.mirrorservice.org/sites/www.bitsavers.org/pdf/victor/victor9000/Lotus_123.pdf)
- [Especificación de los archivos de 1-2-3, Lotus, 1984](https://oss.sheetjs.com/notes/lotus/WSFF2.TXT)
- [Lotus 1-2-3 1A en PCjs](https://pcjs.org/software/pcx86/app/lotus/123/1a) y [Lotus News (WinWorld)](https://winworldpc.com/res/img/spotlight/lpnews.txt)
- [Microsoft: Excel cree que 1900 es bisiesto](https://learn.microsoft.com/en-us/troubleshoot/microsoft-365-apps/excel/wrongly-assumes-1900-is-leap-year)
- [Getting Started with Quattro, Borland, 1987 (bitsavers)](https://ftp.mirrorservice.org/sites/www.bitsavers.org/pdf/borland/quatro/Getting_Started_With_Quatro_1987.pdf) y [Quattro Pro (Wikipedia)](https://en.wikipedia.org/wiki/Quattro_Pro)
- [«Recalc or die»: Excel 1.0, treinta años después (GeekWire)](https://www.geekwire.com/2015/recalc-or-die-30-years-later-microsoft-excel-1-0-vets-recount-a-project-that-defied-the-odds/)
- [Lotus Symphony (Wikipedia)](https://en.wikipedia.org/wiki/Lotus_Symphony_(MS-DOS)), [Framework (Wikipedia)](https://en.wikipedia.org/wiki/Framework_(office_suite)), [Open Access (WinWorld)](https://winworldpc.com/product/open-access)
- [Lotus 1-2-3 2.01, edición española, 1987 (anuncio en todocoleccion)](https://www.todocoleccion.net/videojuegos-pc/amstrad-lotus-1-2-3-hoja-calculo-2-01-lotus-dev-1987-pc-3-1-2-ibm-pc-philips-edicion-espanola~x55147207)
