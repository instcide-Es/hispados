# Pluma, el procesador de textos de Hispa-8086

*Hispa-8086 · noviembre de 2026*
*Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.*

Pluma es el procesador de textos de Hispa-8086 (`h8086/pluma/`, `PLUMA.COM`).
Está pensado para un PC o un XT con un 8088 a 4,77 MHz, 256 K de memoria,
cualquier tarjeta de vídeo en 80 columnas (MDA, Hercules, CGA, EGA o VGA) y un
disquete de 360 K. Está escrito desde cero en ensamblador de 8086. Hispa-386
también tiene una Pluma, pero esa es gráfica, para Hispa Ventanas. Las dos
comparten el nombre y la idea: escribir sin pensar en la máquina.

Este documento cuenta tres cosas:

1. Qué había entonces.
2. Qué copiamos en espíritu y qué mejoramos, ahora que sabemos más que en la
   época.
3. Cómo está hecho.

Las fuentes están al final. Las marcamos así: **[F]** es lo que dice una
fuente; **[D]** es lo que deducimos nosotros o recordamos sin haberlo
comprobado.

## 1. Lo que había (1976-1989)

| Programa | Fecha | Máquina y memoria | Cómo se veía y qué se le criticaba |
|---|---|---|---|
| **Electric Pencil** (Michael Shrayer) | diciembre de 1976, para el Altair | 8 K, 8080 o Z80 [F] | Reajustaba el texto solo al editar, y en 1984 se le alababa el «deshacer». En el TRS-80 solo escribía mayúsculas sin una modificación del aparato. Fallows (1982) lo llamó «anticuado y tosco» [F] |
| **WordStar** (MicroPro, Rob Barnaby) | 1978 para CP/M; 3.0 para el PC en 1982; 3.3 en 1983; 4.0 en 1987 | un .COM que nunca pasaba de 64 K [F] | Los códigos de impresión (^B, ^S, ^Y) se veían en la pantalla. Las órdenes de punto (.he, .pa...) iban en el texto, y la columna 80 llevaba marcas («<» retorno duro, «P» salto de página). No reajustaba solo: había que pulsar ^B. «Famoso por lo complicado» (InfoWorld) [F] |
| **EasyWriter** (John Draper) | 1981, el que IBM vendía con el PC | — | Escrito en Forth. PC Magazine: «un limón», lento y con fallos. La 1.10 arregló casi todo [F] |
| **Volkswriter** (Lifetree) | 1982 | 64 K | Fácil de aprender. Sin encabezados ni pies, y configurar la impresora era un lío [F] |
| **MultiMate** | 1982, a imitación de los equipos Wang | hasta 128 K | Trabajaba por páginas, así que no se veía el final de una y el principio de la siguiente a la vez. Lento [F] |
| **PC-Write** (Bob Wallace) | 1983, shareware | — | Teclas como las de WordStar y texto con los códigos dentro. Archivos de 60 K como mucho, y había que guardar antes de imprimir [F] |
| **Word 1.0 para DOS** (Microsoft) | 1983 | 192 K | Usaba el modo gráfico de la CGA para enseñar la cursiva. El menú estaba en la línea de abajo y se abría con Esc. Deshacía un solo cambio [F/D] |
| **WordPerfect** (SSI) | 2.20 en 1982; 4.2 en 1986; 5.1 en 1989 | 5.1: 512 K según el fabricante, 640 K en la práctica [F] | Pantalla limpia, con el formato en códigos ocultos que se ven con *Reveal Codes*. Controladores para cientos de impresoras. La vista previa de la 5.1 no dejaba editar [F] |

### Los documentos grandes

WordStar 3 no cargaba el documento entero en la memoria. Lo que no cabía lo
dejaba en un archivo temporal `.$$$`. Al guardar, la versión de antes pasaba a
`.BAK`, de las que solo guardaba una. El límite era el disquete: «el límite
absoluto es la capacidad de un disquete» [F]. Un artículo de 1987 calcula que
hacía falta unas tres veces el tamaño del archivo: el documento, el .BAK y el
temporal [F]. Si al retroceder no cabía el temporal, salía «DISK FULL» [F].
PC-Write, en cambio, se quedaba en 60 K [F].

### Cómo se veía el formato

- **WordStar** enseñaba los códigos de impresión como «^B» (aunque cada uno
  ocupaba un solo octeto) y las órdenes de punto como líneas de texto. La
  columna 80 llevaba las marcas: «<» retorno duro, en blanco retorno blando,
  «P» salto de página, «.» después del final, «-» la línea siguiente se
  imprime encima [F].
- **WordPerfect** dejaba la pantalla limpia y guardaba el formato en códigos
  ocultos que se ven con *Reveal Codes* [F].
- **Word 1.0** pasaba al modo gráfico de la CGA para que la cursiva y la
  negrita se vieran [F].

### El formato de WordStar

| Octetos | Qué es [F] |
|---|---|
| bit alto en la última letra de cada palabra | para la microjustificación de la impresora (antes de la 5.0) |
| `0D 0A` | fin de párrafo (retorno duro) |
| `8D 0A` | retorno blando (lo puso WordStar al partir la línea) |
| `A0` | espacio blando (de justificar) |
| `02`, `13`, `19` | negrita (^B), subrayado (^S), cursiva (^Y) |
| `04`, `14`, `16`, `18` | doble golpe, superíndice, subíndice, tachado |
| `1E`/`1F` | guion blando |
| `0C` | salto de página |
| `1A` | fin del archivo |

### España, la ñ y las tildes

- Los compatibles españoles (CTE/Computec) venían con WordStar. Hubo un
  WordStar Express 1.00 en español para el Amstrad PC1512 (1987) y un
  WordPerfect 5.1 en castellano (distribuido por WordPerfect Ibérica) [F].
- En los Amstrad PCW se usaba LocoScript, con los caracteres de cada idioma
  [F].
- No hemos encontrado datos fiables de procesadores hechos en España ni de lo
  que se usaba en las academias.
- **La página de códigos 437** (la de la ROM de las tarjetas MDA y CGA) tiene
  ñ Ñ á é í ó ú ü Ü É ¿ ¡ ç Ç, pero **no Á Í Ó Ú** [F]. La 850 sí las tiene;
  la 858 es la 850 con el €.
- **Las impresoras eran el problema mayor.**
  - La Epson FX-80 no tenía ñ: con `ESC R 7` («España I») cambiaba doce
    signos ASCII por ñ Ñ ¿ ¡ ₧ ¨, pero sin vocales con tilde [F].
  - «España II» (`ESC R 11`) añade á é í ó ú; Latinoamérica (`ESC R 12`),
    también la ü [F].
  - Las Epson posteriores (LX-800, FX-850) traen la tabla PC 437 (`ESC t 1`),
    y las ESC/P 2, la 850 y la 858 (`ESC ( t`) [F].
  - La IBM Proprinter tiene la 437 con su juego de caracteres 2 (`ESC 6`) y
    no tiene cursiva [F].
  - Lo que faltaba se hacía con la letra, un retroceso (08h) y la tilde
    encima [F]. Una tesis de 1988 cuenta que muchos procesadores manejaban mal
    tildes, eñes y diéresis [F].

## 2. Qué copiamos, qué mejoramos y por qué

**Copiamos en espíritu:**

- **Los códigos de WordStar.** Negrita, subrayado y cursiva son los mismos
  octetos que en WordStar (02h, 13h, 19h). Así, un documento de WordStar
  conserva su formato al abrirlo en Pluma.
- **La columna de marcas** a la derecha («<» fin de párrafo, «P» salto de
  página) y **la regla** de arriba (`L----!----R`).
- **Las teclas de WordStar.** Las ^E ^X ^S ^D, ^K, ^Q, ^O y ^P funcionan, para
  quien las tenga en los dedos.
- **El menú de la línea de abajo, como Word 1.0.** Se abre con Esc o F10 y se
  elige con la letra.
- **Ver los códigos, como en WordPerfect.** Con Alt+V aparecen como letras al
  revés (N, S, C, M, D, J) y «¶».
- **El `.BAK` y los archivos temporales** de WordStar: el documento no tiene
  que caber en la memoria.

**Mejoramos:**

| Lo que había | Lo que hace Pluma | Por qué ahora se puede |
|---|---|---|
| WordStar 3 no reajustaba solo (^B) | Las líneas se rehacen solas al escribir y al borrar, también al justificar | Se calculan cada vez, solo las de la pantalla, y si hay teclas esperando se atienden primero: a 4,77 MHz no se nota |
| ^B en la pantalla, o pasar a modo gráfico para ver la cursiva | El formato se ve con atributos de vídeo: brillo, subrayado e inverso. **Sin colores** (Carlos es daltónico; mucha gente también) | MDA y Hercules subrayan; en color, el subrayado es inverso y la cursiva lleva fondo oscuro. La línea de arriba dice con palabras qué formato hay en el cursor |
| Un .BAK, «DISK FULL» al retroceder, tres veces el tamaño en el disco | Solo se usa el disco para lo que no cabe en la memoria. Al guardar se escribe primero `NOMBRE.$$$` y, si sale bien, el viejo pasa a `.BAK` | Un disco lleno o un disquete sacado a medias no estropean lo que había |
| Deshacer de un solo nivel (Word 1.0), o nada | Deshacer de muchos niveles (Ctrl+Z o ^U), por grupos: lo escrito de un tirón, un pegado o un reemplazar entero | Una pila de 2 K con los cambios; cuando se llena, se olvidan los más viejos |
| Buscar exacto | Buscar y reemplazar **sin mirar mayúsculas ni tildes** («cancion» encuentra «CANCIÓN»), pero **la ñ es otra letra** («ano» no es «año») | Una tabla de 256 octetos, hecha al arrancar |
| La 437 sin Á Í Ó Ú y sin € | El texto va en la **858**: todas las letras del castellano, el catalán, el gallego y el vasco, y el €. En EGA y VGA se cargan las nueve letras que faltan (Á À È Í Ï Ó Ò Ú €). En CGA y MDA, cuyas letras están grabadas en la tarjeta, se ven sin tilde (y € como ε), pero se guardan e imprimen bien | EGA y VGA dejan cambiar las letras (INT 10h, AX=1100h) |
| Escribir la Á sin tenerla en el teclado | **Alt+T**: la letra de antes del cursor pasa a la siguiente de su familia: a á à ä â, A Á À Ä Â, n ñ, c ç, ? ¿, ! ¡, < «. Alt+E es el € | La idea es la de mantener pulsada una tecla en el móvil |
| Impresoras que imprimían basura con la ñ | Una tabla por impresora: Epson FX (España I, con retroceso para las tildes), Epson moderna (la 437 con `ESC t 1`), IBM Proprinter (la 437) y solo texto. Lo que no esté, con la letra, un retroceso y la tilde encima. El € como C con = encima | Se sabe qué hace cada una |
| Vista previa sin poder volver (o no la había) | **F9**: la página tal como saldrá, con márgenes, encabezado, pie y número, y una raya entre páginas. Esc y se sigue escribiendo donde se estaba | Se compone con lo mismo que imprime |
| Formatos cerrados | `.PLU` documentado (abajo), casi texto. Exporta a **texto de DOS** (líneas como en la página), **texto de hoy** (UTF-8, un párrafo por línea) y **RTF** (Word, LibreOffice). Abre texto y documentos de WordStar | RTF es de 1987 y lo abre todo; UTF-8 es lo de hoy |
| Las órdenes de punto de WordStar (.he, .pa) en el texto | El encabezado, el pie, los márgenes y la impresora van en la cabecera del documento y se cambian con el menú Página. El salto de página es Alt+P | Menos que aprender |

Lo opcional queda fuera de PLUMA.COM. El corrector ortográfico, con su
diccionario en un disquete aparte, sería otro programa; está pendiente.

## 3. Cómo se usa

```
PLUMA [/W] [documento]
```

Sin extensión, el documento es `.PLU`. `/W` lo abre como WordStar, aunque
Pluma también lo nota solo por sus retornos blandos (8Dh 8Ah). Con la extensión
`.TXT`, Pluma guarda texto sin formato.

| Tecla | Qué hace |
|---|---|
| F1 | la ayuda (tres páginas) |
| F2 | guardar |
| F3 | buscar; F3 e Intro busca el siguiente |
| F4, F5, F6 | copiar, cortar, pegar |
| F7 | empezar a marcar un bloque; F7 otra vez lo fija |
| F8 | reemplazar (S sí, N no, T todos) |
| F9 | la vista de la página |
| Esc o F10 | el menú: Archivo, Edición, Buscar, Formato, Página, Imprimir, Ayuda, Salir |
| Alt+N, Alt+S, Alt+C | negrita, subrayado, cursiva: lo que se escriba a continuación, o el bloque marcado. Otra vez la misma, se deja |
| Alt+Q | quitar el formato del bloque |
| Alt+I, Alt+M, Alt+D, Alt+J | párrafo a la izquierda, centrado, a la derecha, justificado |
| Alt+G | sangría (5 letras más, hasta 4 pasos) |
| Alt+P | salto de página |
| Alt+T | tilde a la letra de antes del cursor |
| Alt+E | € |
| Alt+V | ver los códigos |
| Alt+X | salir |
| Ctrl+Z (o ^U) | deshacer |
| Ins | insertar o sobrescribir (el cursor se hace bloque al sobrescribir) |
| las de WordStar | ^E ^X ^S ^D ^A ^F ^R ^C ^G ^T ^Y ^V ^N ^L, ^QS ^QD ^QR ^QC ^QF ^QA ^QY ^QB ^QK, ^KB ^KK ^KC ^KV ^KY ^KH ^KS ^KD ^KX ^KQ ^KR ^KW ^KP, ^PB ^PS ^PY ^PL, ^OC ^OJ ^OD ^OL ^OR ^OG ^OP. Al pulsar ^K, ^Q, ^O o ^P, la línea de abajo dice qué sigue, como en WordStar |

La línea de arriba dice:

- el nombre del documento (con «*» si hay cambios);
- la columna;
- el formato que hay en el cursor;
- la alineación del párrafo;
- Ins o Sob;
- por dónde se va, en tanto por ciento.

**Imprimir** (menú Imprimir, o ^KP) pide dónde: `PRN` es la impresora, y si
se escribe otro nombre, se imprime en ese archivo. Con «hojas sueltas» se
para entre página y página. Esc deja de imprimir.

## 4. Cómo está hecho

### El texto, en la memoria y en el disco

PLUMA.COM vive en sus 64 K. Detrás del programa y sus búferes quedan unos
30 K para el texto. Ahí va **la parte del documento en la que se trabaja**:

- con un **hueco** en el cursor: escribir una letra es ponerla en el hueco y
  no mueve nada;
- lo de antes del cursor, al principio de esa memoria;
- lo de después, pegado al final.

Lo que no cabe va a tres archivos temporales, en trozos de 4 K:

```
   A (archivo)  |  memoria: antes [hueco] después  |  B (pila)  |  C
```

- **A** guarda, en orden, lo que hay antes de la memoria.
- **B** es una pila de trozos con lo que hay después: el de arriba es el que
  va justo detrás de la memoria.
- **C** es lo que no cupo al abrir el archivo, ya convertido. Se va leyendo
  hacia delante.

Cuando el cursor se acerca a un borde de la memoria (a menos de 4 K) y al otro
lado hay disco, se trae un trozo. Si no hay sitio, primero se lleva otro al
lado contrario. Las posiciones del documento son de 32 bits, así que el límite
es el disco, no la memoria.

**Medido en la prueba:** un texto de 99.400 octetos se abre con unos 30 K de
memoria para el texto. Se va al final, se escribe, se vuelve al principio, se
busca en medio y se guarda idéntico. Al salir, los temporales
(`~PLUMA_A.$$$`, `B` y `C`) se borran.

### Las líneas no se guardan: se calculan

Un párrafo acaba en 0Ah. Al principio puede llevar códigos de párrafo:

| Octeto | Qué es |
|---|---|
| `1Ch` | centrado |
| `1Dh` | a la derecha |
| `1Eh` | justificado |
| `14h` | un paso de sangría (5 columnas); puede repetirse |

Las líneas se sacan cada vez, partiendo por los espacios lo que no cabe en el
ancho, y solo las que se ven. La misma rutina (`mide` y `compon`, en
`pantalla.inc`) sirve para la pantalla, la vista de la página, la impresora y
el texto de DOS. Así, lo que se ve es lo que sale.

El formato de las letras se activa y se desactiva con los códigos de WordStar
(02h, 13h, 19h). En cada párrafo empieza sin nada. Al partir un párrafo con
Intro, el formato que se llevaba se cierra antes del corte y se vuelve a abrir
después, y el párrafo nuevo se queda con la alineación y la sangría del de
antes.

El cursor no se para dentro de los códigos: las flechas los pasan de largo, y
Retroceso y Supr no los borran (se borran viendo los códigos con Alt+V, o con
Alt+Q). Una pareja de códigos iguales sin nada dentro se quita al dejarla.

### El formato .PLU

```
0     8   "PLUMA86" y 1Ah (para que TYPE no enseñe más)
8     1   versión del formato: 1
9     1   margen izquierdo del papel, en letras (10)
10    1   ancho de la línea, de 20 a 78 letras (65)
11    1   sangría de la primera línea de cada párrafo (0)
12    1   líneas por página (66)
13    1   margen de arriba, en líneas (3); el encabezado va en la penúltima
14    1   margen de abajo, en líneas (3); el pie va en la segunda
15    1   número de la primera página (1)
16    1   impresora: 0 Epson FX, 1 Epson moderna, 2 IBM Proprinter, 3 texto
17    1   hojas sueltas: 1 para entre página y página
18-31     reservado (0)
32    48  el encabezado, acabado en 0 («#» es el número de página)
80    48  el pie, acabado en 0 (de fábrica, «- # -»)
128   ... el texto, en la página 858, con CR LF al final de cada párrafo,
          los códigos de formato (02h, 13h, 19h), los de párrafo (14h, 1Ch,
          1Dh, 1Eh), el tabulador (09h) y el salto de página (0Ch)
```

Sin la cabecera, es un texto casi normal. Cualquier programa que lea texto lo
abre, y quitando los códigos se queda el texto limpio.

### La pantalla

- **Fila 0:** el estado.
- **Fila 1:** la regla.
- **Filas 2 a 23:** el texto, con la columna 79 para las marcas.
- **Fila 24:** la ayuda, o el menú, o la pregunta.

Pluma escribe directamente en la memoria de vídeo. Antes de volver a pintar
mira si hay teclas esperando, y si las hay, las atiende primero. Los
atributos:

| Formato | MDA y Hercules | CGA, EGA y VGA |
|---|---|---|
| normal | 07h | 07h |
| negrita | 0Fh (brillo) | 0Fh (brillo) |
| subrayado | 01h (subrayado) | 70h (inverso) |
| cursiva | 70h (inverso) | 17h (fondo azul oscuro: se distingue por la luz) |
| bloque marcado | inverso (o normal, si ya lo era) | igual |

### Las impresoras

| | Epson FX (7 bits) | Epson moderna | IBM Proprinter | Solo texto |
|---|---|---|---|---|
| al empezar | `ESC @`, `ESC R 7` (España I) | `ESC @`, `ESC t 1` (tabla PC 437), `ESC 6` | `ESC 6` (juego 2) | — |
| negrita | `ESC E` / `ESC F` | igual | igual | — |
| subrayado | `ESC - 1` / `ESC - 0` | igual | igual | — |
| cursiva | `ESC 4` / `ESC 5` | igual | (no tiene) | — |
| ñ Ñ ¿ ¡ | `\|` `\` `]` `[` (juego español) | tal cual (437) | tal cual | tal cual |
| á é í ó ú ü ç | letra, retroceso y `'` `` ` `` `{` (diéresis del juego) `,` | tal cual | tal cual | tal cual |
| Á Í Ó Ú | letra, retroceso, `'` | igual | igual | sin tilde |
| € | `C`, retroceso, `=` | igual | igual | `E` |
| al acabar | `FF`, `ESC @` | `FF`, `ESC @` | `FF` | `FF` |

Con el juego España I, los signos `[ \ ] |` del texto salen como ¡ Ñ ¿ ñ: es
cosa de la impresora. España II (`ESC R 11`), que tiene las vocales con
tilde, no estaba en la FX-80; cabe como otra entrada de la tabla.

### Los archivos

| Archivo | Qué hay |
|---|---|
| `h8086/pluma/pluma.asm` | el principio, las teclas, moverse, escribir, el formato, los bloques |
| `h8086/pluma/texto.inc` | el texto con hueco, los trozos en disco (A, B, C), leer de principio a fin, deshacer |
| `h8086/pluma/pantalla.inc` | partir en líneas (`mide`, `compon`), pintar, la regla y el estado |
| `h8086/pluma/archivo.inc` | abrir (texto, .PLU, WordStar), guardar con .BAK, exportar a texto de DOS, UTF-8 y RTF |
| `h8086/pluma/imprime.inc` | las páginas: imprimir, la vista, contar, buscar y reemplazar |
| `h8086/pluma/dialogo.inc` | preguntar en la línea de abajo, el menú, la ayuda, lo de la página |
| `h8086/pluma/letras.inc` | la pantalla, las tablas de la 858 y las nueve letras para EGA y VGA |
| `h8086/pluma/variables.inc` | los textos y las variables |
| `h8086/prueba/partes_pluma.sh` | la prueba (`SOLO="pluma pluma_grande" ./prueba/prueba.sh`) |

**Tamaño:** PLUMA.COM ocupa unos 21 K: el programa, la ayuda y las tablas. En
memoria pide 64 K, de los que unos 30 K son para el texto. Va en el disquete
de **Oficina** (`build/oficina-360.img`), con su manual (`PLUMA.TXT`).

### La prueba

`prueba/partes_pluma.sh` arranca el sistema en QEMU, maneja PLUMA con el
teclado y mira la pantalla y los archivos desde fuera. Comprueba:

- que arranca y pinta, y el ajuste de línea;
- negrita, subrayado y cursiva con sus atributos;
- la ñ, las tildes, Á (Alt+T), € (Alt+E), ¡ ¿ ç;
- guardar y abrir el .PLU, octeto a octeto;
- exportar a UTF-8, RTF y texto de DOS;
- imprimir en un archivo con la Epson moderna y con la FX;
- centrar, justificar, deshacer, copiar y pegar, reemplazar, buscar sin
  tildes, contar, la vista de la página, la ayuda y Alt+V;
- un documento de 99.400 octetos (más que la memoria), ida y vuelta;
- un documento de WordStar;
- que no queden temporales y que `fsck.fat` no encuentre errores.

Al probarlo salió un fallo del núcleo que ya estaba arreglado en otras ramas:
`fat_trae` perdía el sector de la FAT pedido cuando antes tenía que escribir
el que estaba cambiado. Con tres archivos creciendo a la vez, la FAT se
estropeaba. Se ha traído aquí el mismo arreglo (commit 65c48081).

### Lo que falta

- **El corrector ortográfico**, como programa aparte, con un diccionario
  español comprimido en su propio disquete.
- **Epson con España II** (`ESC R 11`), y la 850/858 de las ESC/P 2
  (`ESC ( t`).
- **El número de página en la línea de arriba**, mientras se escribe. Hoy se
  ve en la vista y al contar.
- **Las órdenes de punto de WordStar** (.he, .pa...): al abrir un documento de
  WordStar se quedan como texto, para que se vean.
- **Probarlo en un PC o un XT de verdad**: el reloj de 4,77 MHz y la nieve de
  la CGA, que en QEMU no se ven.

## Fuentes

- Manual de referencia de WordStar 3.0 (MicroPro, 1981), en bitsavers:
  https://ftp.mirrorservice.org/sites/www.bitsavers.org/pdf/microPro/Wordstar_3.0/Wordstar_3.0_Reference_Manual_May81.pdf
- El formato de los archivos de WordStar: https://www.sfwriter.com/wsformat.txt
- WordStar, Electric Pencil, EasyWriter, Volkswriter, PC-Write, MultiMate,
  WordPerfect, Historia de Microsoft Word, Code page 437 y 850, LocoScript
  (Wikipedia, en inglés)
- Electric Pencil en el TRS-80: https://www.trs-80.org/electric-pencil.html
- Informe de la EPA sobre WordPerfect 5.1 (1990):
  https://mirrorservice.org/sites/www.bitsavers.org/pdf/us_epa/pc_tap/PC_TAP_Consumer_Report_%2306_Evaluation_of_WordPerfect_Version_5.1_199002.pdf
- Manual de Word 1.00 para DOS: https://mendelson.org/MS Word 1.00 for DOS Manual.pdf
- Epson ESC/P, manual de referencia (1997):
  https://files.support.epson.com/pdf/general/escp2ref.pdf
- IBM Proprinter / PPDS: https://www.ibm.com/support/pages/node/635833
- RTF 1.0 y 1.3: https://latex2rtf.sourceforge.net/RTF-Spec-1.0.txt y
  https://latex2rtf.sourceforge.net/RTF-Spec-1.3.txt
- LibreOffice, `\pc` y `\pca`:
  https://docs.libreoffice.org/writerfilter/html/rtfdispatchflag_8cxx_source.html
- Computec, el primer compatible español:
  https://parceladigital.com/articulo/computec-primer-ordenador-espanol-compatible-con-ibm-pc
- WordStar para el Amstrad PC1512: https://winworldpc.com/product/wordstar/1512
- Tesis sobre procesadores y el español (Guatemala, 1988):
  https://repositorio.uvg.edu.gt/bitstreams/112842fc-6268-484a-a9a8-65d2a9510864/download
