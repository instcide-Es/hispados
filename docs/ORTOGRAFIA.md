# El corrector ortográfico

Los programas de «Productividad» (Pluma primero) comparten un corrector ortográfico de
español: dice si una palabra está bien escrita y propone correcciones. Son dos cosas:

- `programas/comun/ortografia.inc`: el código (ensamblador de 386; unos 4 K ya
  ensamblado). Se incluye en el programa, como `tipos.inc`. GPL-2.0 o posterior.
- `programas/comun/DICCIO.DAT`: el diccionario, que va al lado de los módulos
  (`\HISPADOS\PROGS`). Lo hace `tools/hazdiccio.py`. **Tiene la licencia de su origen**,
  que no es la del código (más abajo).

Si `DICCIO.DAT` falta o está roto, el corrector da todo por bueno: el programa sigue
funcionando, sin corrector.

## De dónde sale el diccionario y su licencia

Del diccionario de español de LibreOffice para Hunspell (proyecto RLA-ES, «Recursos
Lingüísticos Abiertos del Español»), tal como lo empaquetan Debian y Ubuntu en
`hunspell-es` (versión 1:24.2.1-1, del paquete fuente `libreoffice-dictionaries`):
`es_ES.dic`, 57.155 raíces con sus marcas, y `es_ES.aff`, 6.665 reglas de prefijos y
sufijos.

- Copyright: Santiago Bosio y los colaboradores de RLA-ES.
- Licencia: **GPL-3.0 o posterior, o LGPL-3.0 o posterior, o MPL-1.1 o posterior**, a
  elegir (así lo declara el paquete: `Files: dictionaries/es/*`, `License: GPL-3+ or
  LGPL-3+ or MPL-1.1+`).

`DICCIO.DAT` es ese diccionario transformado (todas sus formas, en otro formato), así
que es una obra derivada y **se distribuye con esa misma licencia triple**, como un
archivo de datos aparte: el código de Hispa-386 (GPL-2.0 o posterior) no lo lleva
dentro, lo lee al arrancar el programa. Quien lo redistribuya tiene que decir de dónde
sale y dar la manera de rehacerlo, que es este documento y `tools/hazdiccio.py`.

Además, `tools/hazdiccio.py` añade 391 palabras que al diccionario de origen le faltan
y que en una carta se echan mucho de menos (trae pocos nombres propios, unos 940):
nombres de pila, apellidos, comunidades, provincias y países, y una docena de palabras
sueltas («internet», «megabyte», «guión», «vámonos»). Esa lista (`SUPLEMENTO`, dentro de la
herramienta) es de este proyecto, GPL-2.0 o posterior. Con `--sin-suplemento` se hace
el diccionario solo con lo de origen.

## Cómo se rehace

El diccionario de origen no está en el repositorio. Para tenerlo sin instalar nada en
el sistema:

    mkdir -p /tmp/orto && cd /tmp/orto
    apt-get download hunspell-es
    dpkg -x hunspell-es_*.deb paquete
    cd -                                    # al repositorio
    python3 tools/hazdiccio.py /tmp/orto/paquete/usr/share/hunspell \
            -o programas/comun/DICCIO.DAT --medir

(si el paquete `hunspell-es` ya está instalado, la carpeta es `/usr/share/hunspell`).
Tarda unos diez segundos y da siempre el mismo archivo. Con la versión dicha:

    57155 raíces, 6665 reglas -> 656270 formas; con las 391 palabras añadidas, 656661
    autómata: 37708 nodos, 116195 arcos, 464812 octetos; la palabra más larga, 23 letras

`--formas LISTA.txt` deja además todas las formas en un texto; `--mas PALABRAS.txt`
añade las de un archivo (una por línea, UTF-8).

Las pruebas (`tests/run_ortografia_tests.sh`) rehacen el diccionario y lo comparan
con `DICCIO.DAT` si encuentran el de origen en `/usr/share/hunspell` o en la carpeta
que diga `ORTO_FUENTE`.

## Por qué así: todas las formas en un autómata

El español tiene cientos de miles de formas. Había dos caminos, y se midieron los dos:

| | (a) raíces + reglas, aplicadas al consultar | (b) todas las formas, en un autómata |
|---|---|---|
| Tamaño del archivo | unos 437 K (347 K de raíces con el principio compartido, 11 K de juegos de marcas, 78 K de reglas) | **454 K** (464.812 octetos) |
| Memoria | el archivo | el archivo |
| Consultar una palabra | probar cada sufijo y prefijo que cuadre con ella, deshacerlo, mirar la condición y buscar la raíz con esa marca; y las combinaciones de prefijo + sufijo + segundo sufijo | seguir la palabra letra a letra: unos 70 arcos |
| Propuestas | fabricar miles de candidatas y consultarlas una a una | recorrer el autómata al lado de la palabra dejando una corrección: menos de 1.000 pasos |
| Código | unas 6.700 reglas con condiciones que interpretar en ensamblador | unas 150 líneas para consultar; nada que interpretar |

Ocupan casi lo mismo, porque en español los finales se repiten tanto que el autómata
mínimo (un «DAWG»: comparte los principios de las palabras, como un árbol de letras, y
también los finales) de 656.661 palabras se queda en 116.195 arcos. Con el mismo
tamaño, el camino (b) es mucho más simple, más rápido y más fácil de comprobar (un
archivo roto no puede colgar nada), y las propuestas salen casi gratis. Se eligió (b).

`tools/hazdiccio.py` expande cada raíz como lo hace Hunspell: un sufijo de sus marcas;
un segundo sufijo si el primero trae marcas («abreviar» → «abreviación» →
«abreviaciones»), y un prefijo de sus marcas delante de cualquiera de esas formas.
Contrastado con una implementación independiente de Hunspell (spylls, en Python): de
30.000 formas al azar acepta las 30.000, y de 30.000 palabras estropeadas que el
expansor no da no acepta ninguna.

## El archivo

Los números, en little-endian. Las letras, en la página de códigos 437.

| Sitio | Qué |
|---|---|
| 0 | `"DICCIO1"`, 26 |
| 8 | `dd` cuántos arcos |
| 12 | `dd` cuántas palabras reconoce (solo se enseña) |
| 16 | `dd` suma de todos los arcos, módulo 2^32 |
| 20 | `db` largo de la palabra más larga |
| 21 | 11 octetos a cero |
| 32 | los arcos, de 4 octetos |

Cada arco: la letra (bits 0-7), «con esta letra se acaba una palabra» (bit 8), «es el
último arco de su nodo» (bit 9) y el primer arco del nodo al que lleva (bits 10-31; 0,
a ninguno). Un nodo son varios arcos seguidos, ordenados por su letra. El nodo del
principio de las palabras empieza en el arco 0. Una palabra está si, siguiéndola letra
a letra desde ahí, con la última se llega a un arco con el bit 8.

Al cargarlo se comprueba TODO: la firma, que el tamaño sea justo el de la cabecera más
los arcos (ni cortado ni con algo detrás), que cada arco lleve a un arco que existe, que
ninguna letra sea 0, que el último arco cierre su nodo y la suma. Después, consultar no
puede leer fuera del diccionario; y un tope de pasos (400.000; lo normal son menos de
1.000) corta cualquier búsqueda que no acabara en un archivo hecho con mala idea.

## Las mayúsculas y los acentos

En la página 437 hay á é í ó ú ü ñ Ñ É Ü, pero no Á, Í, Ó ni Ú. Por eso:

- los nombres propios que empiezan por una de esas se guardan sin acento («Avila»,
  «Africa», «Oscar»), que es como se pueden teclear;
- una palabra vale **tal cual** («árbol», «Madrid», «ADSL»); con **la primera en
  mayúscula** («Casa»; y «Arbol» vale por «árbol»); y **toda en mayúsculas** («CASA»,
  «MADRID»; y «ARBOL», «CAMION», «MALAGA» valen por «árbol», «camión», «Málaga»);
- la É y la Ü sí existen, así que se piden: «CAFÉ» y «CIGÜEÑA» valen; «CAFE», no;
- los nombres propios solo valen con su mayúscula: «madrid» no vale;
- lo que tiene cifras, lo de una sola letra y lo de más de 63 letras no se juzga (se da
  por bueno).

## Cómo se usa (la interfaz)

En el código del programa, `%include "ortografia.inc"`; en su `section .bss`, la macro
`ORTO_BSS` (unos 520 K: `ORT_MAX`, lo que puede ocupar el diccionario, son 512 K; se
puede definir otro tope antes del `%include`). Solo `ort_cargar` usa la API de Hispa
Ventanas (`HV dos_abrir`, `dos_leer`, `dos_cerrar`); lo demás trabaja con lo que hay en
memoria. Todas conservan los registros que no devuelven.

| Rutina | Entra | Sale |
|---|---|---|
| `ort_cargar` | ESI = ruta del módulo | `[ort_hay]` = 1 si hay diccionario. Vacía la lista del usuario |
| `ort_palabra` | ESI = palabra, ECX = largo | CF = 0 si está bien o no se juzga; CF = 1 si no se conoce |
| `ort_sugerir` | ESI = palabra, ECX = largo, EDI = dónde, EDX = cuántas a lo más (hasta 8) | EAX = cuántas ha puesto: cada una en 64 octetos y acabada en 0, la más probable primero, con las mayúsculas de la tecleada |
| `ort_propia` | ESI = palabra, ECX = largo (1 a 63, sin espacios) | CF = 0 si queda apuntada (o ya valía); CF = 1 si no cabe o no vale |
| `ort_propias_ptr` | | ESI = la lista del usuario, ECX = sus octetos |
| `ort_propias_poner` | ESI = un texto, ECX = sus octetos | la lista pasa a ser las palabras de ese texto |

**Las propuestas** son las palabras del diccionario a una corrección de la tecleada:
una letra cambiada, de más, de menos, o dos vecinas trocadas; o «ll» por «y», «k» por
«qu» y «x» por «cc» (y al revés). Los acentos no cuentan como corrección: pueden
faltar o sobrar además («tanbien» → «también»). Se ordenan por una nota (menos es
mejor): +1 por cada acento, y por la mayúscula de un nombre propio; +2 las faltas de
siempre (b/v, g/j, c/z, c/s, s/z, s/x, m/n, q/k, c/k, c/q, y/i, n/ñ, la h de más o de
menos, una letra doblada o sin doblar como r/rr, la c de «xc» y «sc», la u de «gu» y
«qu», y los grupos); +4 dos letras trocadas; +5 una letra de más o de menos; +6 una
letra por otra (y +1 a estas dos últimas si es la primera letra de la palabra).

**La lista del usuario** está en memoria (4.096 octetos) y es un texto: una palabra
por línea (CR LF). Guardarla es escribir en un archivo lo que da `ort_propias_ptr`;
leerla, pasar el archivo a `ort_propias_poner` (admite cualquier texto: palabras
separadas por espacios o saltos de línea; lo que no vale o no cabe se deja). Una
palabra apuntada en minúsculas vale también con la primera en mayúscula y toda en
mayúsculas; apuntada con mayúsculas, solo así (y toda en mayúsculas).

Pila: lo normal, menos de 1 K; lo más hondo que puede llegar (palabra de 63 letras con
un diccionario que tuviera palabras así de largas), unos 3 K.

## Lo que mide (pruebas de `tests/run_ortografia_tests.sh`)

- Archivo y memoria: 464.812 octetos. Cargarlo es leerlo y recorrerlo una vez.
- Consultar una palabra: unos 70 arcos de media. En el arnés de pruebas (un PC de hoy),
  0,12 microsegundos; en un 386 a 33 MHz se calcula (no está medido en uno) del orden de
  0,1 a 0,2 milisegundos: miles de palabras por segundo.
- Proponer: hasta unos 1.000 pasos; 30 microsegundos en el arnés; en un 386, del orden
  de decenas de milisegundos (calculado).

## Límites

- **El vocabulario es el del diccionario de origen.** Trae pocos diminutivos y
  superlativos (están «perrito», «casita», «pequeñito», «buenísimo»; no «librito»,
  «abuelita», «carísimo»), pocos nombres propios (por eso el suplemento) y poca
  palabra moderna («email» y «megabyte» se han añadido; «guasap» no está).
- **Verbos con pronombres pegados**: el `.aff` los cubre con reglas por verbo, y están
  la mayoría («dámelo», «diciéndoselo», «cuéntaselo», «avísame», «decidle»,
  «recogernos», «siéntate»), pero no todas las combinaciones de imperativo con dos
  pronombres ni las de «nos»/«os» que pierden letra: faltan, por ejemplo, «cómetelo»,
  «llévatelo», «póntelo» y «quédatelo».
- **No mira el contexto**: «tubo» y «tuvo», «haber» y «a ver», «el» y «él» valen los
  dos; una falta que da otra palabra no se ve.
- **Propuestas a una sola corrección** (más los acentos y los grupos): «haiga» no
  propone «haya», ni «nesecito» «necesito», ni «aver» «haber». Tampoco propone partir
  una palabra en dos («ala mesa»), ni las palabras del usuario. A igual nota no hay
  frecuencias de uso con que ordenar: salen por orden alfabético («espontaneo» →
  «espontaneó», «espontáneo»).
- **Mayúsculas**: «EXITO» no vale (la É existe: «ÉXITO»); una palabra con mayúsculas
  en medio solo vale si está así en el diccionario («McDonald», «pH»).
- Palabras con guion o apóstrofo: quien llama parte el texto en palabras; el corrector
  no conoce más letras que las del diccionario.
- Solo español (el de España y América juntos, como el `es_ES` de LibreOffice, que es
  el mismo archivo para todos los países).
