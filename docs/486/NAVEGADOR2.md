# El Navegador completo de Hispa-486 (NetSurf)

`NAVEGA2.EXE`: un segundo navegador para Hispa-486, para ver **la web entera**
(hojas de estilo de fuera, flexbox, letras de ancho variable, imágenes en
color, colores de verdad) como en un navegador de los grandes. Es
[NetSurf](https://www.netsurf-browser.org/) 3.11, el navegador libre en C,
hecho nuestro: la ventana, la barra, la línea de estado y el dibujo son de
Hispa Ventanas (programa Hispa32 con GDI), en español, con **nuestra red**
(WSOCK32.DLL y HSPTLS.DLL, por el `http.c` del Navegador propio) en vez de
libcurl y OpenSSL.
Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior (lo
nuestro); NetSurf, GPL-2.0, y sus bibliotecas, MIT: el programa entero va con
la GPL-2.0.

Nació de una decisión de Carlos (18/11/2026): «La B con NetSurf». El
Navegador propio ([NAVEGADOR.md](NAVEGADOR.md)) dibuja la página como una
rejilla de letras de 8×16: va muy bien para el modo ligero de ClicGest, pero
no puede mostrar la web de hoy. Los dos conviven: el ligero (`NAVEGA.EXE`,
disquete «Internet») para la biblioteca en un 486 lento, el completo
(`NAVEGA2.EXE`, disquete «Internet 2») para lo demás.

| | Navegador (NAVEGA.EXE) | Navegador completo (NAVEGA2.EXE) |
|---|---|---|
| Página | rejilla de letras 8×16, 256 colores de Hispa | puntos (32 bits, pasados a la paleta), letras DejaVu de ancho variable |
| CSS | lo mínimo (de las `<style>`; clases de Bootstrap conocidas) | CSS 2.1 + flexbox + calc() de NetSurf, hojas de fuera, `@media`; variables de CSS, `gap`, grid, esquinas redondas, sombras, `@font-face` (nuestro) |
| JavaScript | sí (QuickJS, HSPJS.DLL) | sí (QuickJS, la misma HSPJS.DLL, sobre el DOM de libdom: ver «JavaScript») |
| ClicGest | modo ligero (User-Agent `HispaNavegador`) | la web normal (User-Agent `HispaSurf`) |
| Memoria | 1-3 MB | 8-15 MB (ver «Medidas») |
| Tamaño | 300 KB | 1,27 MB + 208 KB de letras (dos disquetes; el JavaScript, en HSPJS.DLL del disquete «Internet») |

## Cómo se maneja

La ventana: **< >** (atrás, adelante), **Recargar** (mientras carga,
**Parar**, en ocre), **Inicio** y la dirección con **Ir**; abajo, la línea de
estado (lo que hace NetSurf, el enlace de debajo del ratón; «Conexión
cifrada» en verde con https; «Espere, por favor» en ocre mientras carga, y el
cursor de reloj). La barra de la derecha desplaza (la pista, una pantalla; el
pulgar se arrastra).

| | |
|---|---|
| Ratón | clic en enlaces, botones y campos; la rueda mueve la página (o la caja con desplazamiento que hay debajo); Mayús+rueda, a los lados |
| F6 o Ctrl+L | a la dirección (elegida entera: lo que se escriba la sustituye); Intro va; Esc la deja como estaba |
| F5 o Ctrl+R | recargar |
| Esc | parar la carga |
| Flechas, Re Pág, Av Pág, Inicio, Fin | mueven la página (en un campo, el cursor) |
| Espacio | fuera de un campo, baja una pantalla (Mayús: sube) |
| Retroceso | fuera de un campo: atrás |
| Tab, Mayús+Tab | en los formularios, al campo siguiente o al anterior (NetSurf) |
| Ctrl+A, Ctrl+C, Ctrl+V, Ctrl+X | en los campos (un portapapeles del programa) |

Una dirección sin `http://` se pide por HTTP; `C:\CARPETA\PAGINA.HTM` abre
la página del disco (`file:///C:/CARPETA/PAGINA.HTM`). Si se le pasa una
dirección al abrirlo, abre esa.

`NAVEGA2.INI`, junto al programa (las líneas son opcionales):

```
Inicio=https://clicgest.es/login   ; la página de arranque y del botón Inicio
Idioma=es                          ; el idioma que se pide (Accept-Language)
```

Sin `Inicio=`, arranca en `resource:inicio.htm` (`RECURSOS\INICIO.HTM`: los
enlaces a hispados.es, clicgest.es y C:\, y cómo se maneja). Las galletas que
dicen hasta cuándo valen se guardan al cerrar en `SURFGAL.TXT` (el mismo
formato que las del Navegador propio).

## Dónde está y qué necesita

En los disquetes «Internet 2» y «Internet 3» (`make internet2`:
`build/internet2.img` y `build/internet3.img`), que se copian **enteros** en
`C:\HISPADOS\INTERNET`, junto a lo del disquete «Internet» (que trae
MSVCRT.DLL, WSOCK32.DLL, HSPTLS.DLL y HSPJS.DLL, el JavaScript). Desde el 20/11/2026 son dos (con las
letras de la web ya no cabía en uno): «Internet 2», el programa y `RECURSOS`;
«Internet 3», las letras (`FUENTES`).

| | |
|---|---|
| `NAVEGA2.EXE` | el programa (1.270 KB; con el JavaScript, el preludio `dom.js` + `surf.js` va dentro como texto: 75 KB) |
| `NAVEGA2.INI` | sus opciones |
| `RECURSOS\` | las hojas de estilo de NetSurf (`default.css`, `quirks.css`, `adblock.css`, `internal.css`), sus textos en español (`MESSAGES`), la página de inicio y `BROTLI.DIC` (el diccionario de Brotli para las letras WOFF2, comprimido: 58 KB) |
| `FUENTES\` (disquete «Internet 3») | las letras: DejaVu Sans, Sans negrita, Serif, Serif negrita, Mono y Mono negrita, recortadas a las letras de Europa occidental (208 KB; `terceros/dejavu`) |
| `LEEME.TXT` | cómo se instala |

Le quedan 56 KB libres a «Internet 2» (con el JavaScript) y 1,2 MB a «Internet 3». Hispa Ventanas pone solo, al arrancar,
el acceso **«Navegador completo»** en el grupo **Internet** del menú Inicio
(si encuentra `INTERNET\NAVEGA2.EXE`; una vez: la clave `Internet2` de
`[Centro]` en CENTRO.INI; si se borra, no vuelve).

Necesita un 486 (mejor DX2 o más) y **16 MB de memoria como poco; con 32 MB
va holgado** (ver «Medidas»). Si existe `SURF.LOG` junto al programa, apunta
ahí lo que pide, los tiempos y la memoria, y NetSurf su registro en `NS.LOG`
(con `make SURF_REGISTRO=INFO`, todo lo que hace).

## Cómo está hecho

```
terceros/netsurf/   NetSurf 3.11 y sus bibliotecas (libcss, libdom, hubbub,
                    libparserutils, libwapcaplet, libnsutils, libnsfb,
                    libnspsl, libnsbmp, libnsgif, libsvgtiny), con lo
                    generado ya hecho; LEEME.txt (procedencia, sha256,
                    licencias y los cambios marcados [HISPA])
terceros/stb/       stb_truetype (las letras) y stb_image (PNG y JPEG)
terceros/dejavu/    las letras, recortadas
w32/surf/           lo nuestro:
  surf.c            el frontal: la ventana, la barra, el ratón y el teclado,
                    las tablas de NetSurf (ventana, varios, portapapeles), el
                    bucle de mensajes con el planificador de NetSurf
  letra.c           las letras con stb_truetype (medir, partir líneas,
                    pintar con sus grises) y una memoria de 1.024 letras ya
                    pintadas (256 KB como mucho)
  red.c, red_http.c http y https: un «fetcher» de NetSurf sobre el http.c
                    del Navegador (una petición cada vez, por la conexión que
                    se guarda abierta; multipart/form-data)
  imagen.c          PNG y JPEG con stb_image (los GIF, BMP e ICO, con
                    libnsgif y libnsbmp de NetSurf)
  cssvar.c          las variables de CSS (ver abajo) y los degradados
  webletra.c        las letras de la web (@font-face): las lee, las pide
  woff.c            WOFF y WOFF2 a TTF (terceros/brotli: el decodificador
                    de Brotli de Google, MIT)
  redondo.c         esquinas redondas, sombras y fondos con transparencia,
                    en el búfer de la página
  anfitrion.c,      el mismo NetSurf para Linux, sin ventana: pinta una
  anfitrion_rh.c    página en un PPM (`make surfppm`; la red con curl); para
                    probar el pintado en segundos (ver «Pruebas»)
  archivo.c         las rutas de DOS (C:\...) <-> file:///C:/...
  memoria.c         malloc/free propios (ver «Medidas»)
  apoyo.c, inc/     la biblioteca de C que falta en MSVCRT.DLL (POSIX,
                    iconv, la hora...) y sus cabeceras
  ns/               plot.c, bitmap.c, schedule.c: del frontal framebuffer
                    de NetSurf (GPL-2.0), con los cambios mínimos
  surf.mk           cómo se compila (lo incluye el Makefile)
```

- **Sin la biblioteca del sistema.** Todo se compila con `gcc -m32
  -ffreestanding -nostdinc` contra `w32/surf/inc` (las estructuras de
  MSVCRT.DLL: `FILE`, `struct stat`, `time_t` de 32 bits) y se enlaza con
  `tools/elf2pe.py` (600 objetos, un minuto en el VPS sin `-j`). Lo que no
  tiene MSVCRT lo pone `apoyo.c`. Ojo con gcc: `(float)floor(x)` lo cambia
  por `floorf(x)` (que se llamaba a sí misma: el primer cuelgue); y en
  Hispa-486 no corren los constructores de C (`__attribute__((constructor))`:
  la superficie de libnsfb se registra a mano).
- **El dibujo.** NetSurf pinta la página en un búfer de 32 bits de libnsfb
  (los dibujos del frontal framebuffer: rectángulos, líneas, imágenes
  escaladas y en mosaico, letras con sus grises); `surf.c` junta lo que hay
  que repintar y lo copia a la ventana con `SetDIBitsToDevice` (Hispa
  Ventanas lo pasa a su paleta de 256 colores, el más cercano).
- **Las letras.** NetSurf necesita letras de ancho variable: FreeType era
  demasiado; `stb_truetype` (un solo archivo, dominio público) lee los TTF y
  pinta cada letra con 256 grises. Sin «hinting»: a 12-16 puntos se leen bien
  (ver las capturas). No hay cursiva de verdad (se usa la normal).
- **La red.** El User-Agent es `HispaSurf/0.1 (NetSurf 3.11; Hispa-DOS 486)`:
  ClicGest da su **modo ligero** a todo lo que lleve `HispaNavegador`,
  `(Hispa-486` o `Hispa32` (`src/Core/Dispositivo.php`, `UA_LIGERO`), así
  que este no lleva ninguno de los tres (con el primero que se probó,
  «(Hispa-486; NetSurf)», ClicGest daba la versión ligera). Las páginas HTML
  se dan a NetSurf enteras, no a trozos: con los trozos de la red, una
  `<style>` partida se perdía.

### Las variables de CSS

libcss (0.9.2, la de NetSurf 3.11) no entiende `var()`: Bootstrap 5 y la web
de Hispa-DOS ponen así casi todos sus colores, y sin ellas salían en blanco y
negro (`navegador2/02_hispados_sin_variables.png` contra
`02_hispados.png`). `cssvar.c` las cambia en el texto de cada hoja antes de
que la lea libcss (`css.c` y `select.c` de NetSurf, marcado `[HISPA]`):

- las de `:root`, `html`, `body`, `*` valen para todo, también para las
  hojas que vienen después y para los atributos `style` (se guardan de una
  página a otra: las `<style>` de la página llegan antes que las hojas de
  fuera que las definen);
- `var(--x)` se cambia por la de la misma regla, si la pone; si no, la de
  todo; si no, la de reserva (`var(--x, 10px)`); una que queda vacía no se
  pone;
- si otra regla pone la variable (`.btn-primary { --bs-btn-bg: #0d6efd }`
  y `.btn { background-color: var(--bs-btn-bg) }`), se añade una regla para
  los que llevan las dos y para los de dentro (`.btn.btn-primary,
  .btn-primary .btn { background-color: #0d6efd }`), hasta 24 por regla y no
  de las de `:hover`, `:focus`... (memoria). No es la cascada de verdad,
  pero da casi siempre lo mismo;
- los **degradados** (`linear-gradient`, `radial-gradient`) libcss los tira
  con la declaración entera (y el fondo oscuro de una página se quedaba
  blanco): `background: radial-gradient(...), #0b0a07` pasa a
  `background: #0b0a07`; si solo hay degradado, su primer color.

Bootstrap 5.3 (233 KB) sale en 439 KB; en el VPS, 0,1 s.

## Comparado con Chrome

Capturas de Chrome (Playwright, 780×493: la misma zona de página que
NAVEGA2.EXE a 800×600) y de NAVEGA2.EXE en QEMU, en `navegador2/`. Las del
18/11 (`01`-`09`) son de antes del 20/11; las `1x_..._despues.png`, de
después del pintado nuevo (`tools/surf_capturas.sh`):

| Página | NAVEGA2.EXE (18/11) | NAVEGA2.EXE (20/11) | Chrome |
|---|---|---|---|
| Una página HTML+CSS del disco | `01_pagina_de_disco.png` | | |
| https://hispados.es/ | `02_hispados.png` (sin las variables: `02_hispados_sin_variables.png`) | | `02_hispados_chrome.png` |
| https://clicgest.es/ | | `10_clicgest_inicio_despues.png` | `10_clicgest_inicio_chrome.png` |
| https://clicgest.es/login | `03_clicgest_login.png` | `11_login_despues.png` | `03_clicgest_login_chrome.png` |
| demo: panel | `04_demo_panel.png` | `12_panel_despues.png` | `04_demo_panel_chrome.png` |
| demo: la biblioteca | | `13_biblioteca_despues.png` | `13_biblioteca_chrome.png` |
| demo: catálogo de la biblioteca | `05_catalogo.png` | `14_catalogo_despues.png` | `05_catalogo_chrome.png` |
| demo: socios | `06_socios.png` | | `06_socios_chrome.png` |

(Solo lecturas en la demo: entrar por `/demo` y mirar.)

### Lo que se arregló el 20/11/2026 (el pintado)

Carlos (20/11): «Mejor dejar la web como tiene que estar y hacer compatible
el explorador web» (veía «espacios raros» y le costaba encontrar «Entrar» en
ClicGest). Sin tocar ClicGest:

- **96 puntos por pulgada**, como Chrome. NetSurf usaba 90: 16px de CSS eran
  15 puntos en las cajas y 17 en las letras (`letra.c` ya contaba con 96):
  todo salía un poco más estrecho y las letras más grandes.
- **Letras de la web** (`@font-face`, `webletra.c`): TTF, OTF, WOFF (zlib) y
  WOFF2 (Brotli, `woff.c`; el diccionario, `RECURSOS\BROTLI.DIC`, se lee
  solo mientras se descomprime). Se piden las familias que usa alguna regla
  (`font-family`) y, con `unicode-range`, solo si tienen la «A»; la página
  espera a que lleguen (como `font-display: block`), así se maqueta una vez.
  Si la hoja da a elegir, TTF/OTF y WOFF antes que WOFF2 (en un 486 Brotli
  tarda más y gasta más memoria). **Bootstrap Icons** sale (el ojo, la lupa,
  las flechas...) y la letra de ClicGest, **Plus Jakarta Sans** (variable:
  stb_truetype pinta su peso normal; la negrita se hace engordando 1-3 puntos
  la letra). Hasta 16 letras y 2,5 MB entre todas; se guardan de una página a
  otra.
- **`::before` y `::after` con texto** (NetSurf solo hacía bloques vacíos):
  los iconos de letra son eso (`.bi::before { content: "\f341" }`).
- **Línea de base**: el texto y los inline-block (iconos, botones) a la
  misma línea de base, con `vertical-align: -.125em`; la base sale de las
  medidas de la letra (NetSurf la ponía a 3/4 de la línea: los iconos salían
  cortados por arriba). El espacio detrás de un inline-block («`<i></i> Ver`»).
- **`gap`** (`row-gap`, `column-gap`, `grid-gap`) en flexbox y grid, y
  **`justify-content`** (NetSurf 3.11 no lo hacía: `space-between`, `center`...).
- **CSS grid** (`layout_grid.c`): `grid-template-columns` con longitudes,
  `fr`, `auto`, `minmax()`, `repeat(N | auto-fill | auto-fit, ...)`;
  `grid-column` (`N`, `N / M`, `1 / -1`, `span N`); `align-items`. Las filas,
  del alto de lo que llevan. No: `grid-template-areas`, `grid-row`,
  `grid-template-rows`.
- **Esquinas redondas** (`border-radius`, suavizadas), **sombras**
  (`box-shadow`, la primera, difuminada) y **fondos y bordes con
  transparencia** (`rgba()`): `redondo.c`, en el búfer de la página.
- **Campos y botones como en Chrome**: `min-height` y `max-height` con
  `box-sizing: border-box` (los campos de 48 de ClicGest medían 66); el texto
  de los botones en el centro; el texto de ayuda (`placeholder`) en gris; las
  casillas con estilo (Bootstrap), azules con su marca al estar marcadas.
- **Flexbox**: los elementos en línea, los campos y los absolutos dentro de un
  flex son bloques (si no, el campo de `.input-group` no se estiraba y
  `.visually-hidden` ocupaba sitio); los absolutos no entran en el reparto;
  el margen de la caja siguiente ya no atraviesa una caja flex; `inline-flex`
  da alto a su línea; con `min-height`, se vuelve a centrar.
- **CSS**: `font-weight: bolder` (el `<strong>` de Bootstrap salía normal);
  `:focus-within` y `:focus-visible` (sin entenderlas se perdía la regla de
  `.visually-hidden`); texto con degradado (`background-clip: text`: «todo en
  uno»), los brillos desenfocados (`filter: blur()`, se esconden) y los
  degradados casi transparentes (sin manchas de color); una imagen con su
  alto y `width: auto` (el logotipo) guarda la proporción.

### Lo que se arregló el 21/11/2026 (con el Navegador de Hispa-Pentium)

El Navegador de Hispa-Pentium ([PENTIUM.md](../PENTIUM.md), sección 14 ter) es
este mismo NetSurf con otra ventana; lo arreglado allí vale aquí:

- **`vh` y `vw`** estaban cambiados en libcss (`100vh` era el ancho): la
  tarjeta de entrar de ClicGest salía muy abajo (`03_clicgest_login.png`);
  ahora, arriba, como en Chrome.
- **`clamp()`, `min()`, `max()` y `env()`** (cssvar.c): los títulos grandes de
  hispados.es y sus márgenes (`max(20px, env(safe-area-inset-left, 0px))`).
- **`opacity: 0`** no se pinta (los botones de Google con su capa encima).
- **`document.implementation.createHTMLDocument()`** (dom.js) da un documento
  aparte: con el de la página, jQuery se llevaba el cuerpo entero al arrancar
  (Wikipedia en blanco).
- Un campo que JavaScript hace de texto tras hacer su caja tiene su textarea.
- **`position: fixed` y `sticky`** (21/11 por la tarde, navega 0.2 de
  Hispa-Pentium): lo fijo, respecto a la ventana (la barra lateral de
  ClicGest ya no se va con la página), al pintar y al pulsar; lo sticky, pegado
  arriba; y `vh`/`vw` sin redondear (100vh se quedaba corto).

### Lo que queda distinto, de más a menos visible

1. ~~**JavaScript**~~: hecho el 20/11 (ver «JavaScript»): ClicGest ya no
   enseña su versión «sin JavaScript».
2. **SVG**: el botón de la hamburguesa (`navbar-toggler-icon`), las flechas
   de los desplegables (`form-select`) y la marca de las casillas de
   Bootstrap son SVG; sin SVG no salen (la de las casillas se pinta aquí).
3. **`<details>`**: NetSurf lo enseña siempre abierto (y sin JavaScript no
   se podría abrir): el formulario «Añadir un libro» del catálogo sale entero.
4. **Las letras**: Plus Jakarta Sans variable en su peso normal (la negrita,
   engordada); sin `letter-spacing` (NetSurf no lo hace): los títulos un poco
   más anchos; los emojis (👋, 🟡) no salen.
5. **`position: fixed`** (el botón del menú lateral del móvil), `opacity`,
   `transform` (salvo esconder lo de fuera de la pantalla) y las animaciones.
6. Los colores, en los 256 de Hispa Ventanas (el más cercano, sin tramado):
   los grises claros, los pasteles y las sombras suaves se parecen, pero no
   son.
7. Algún caso de texto que salta a la línea siguiente en vez de partirse
   (NetSurf parte las cajas de texto al maquetar y, si se maqueta otra vez
   más ancho, no las junta).

## JavaScript

Desde el 20/11/2026 (decisión de Carlos: «mejor dejar la web como tiene que
estar y hacer compatible el explorador web»), el Navegador completo ejecuta
el JavaScript de las páginas: ClicGest se usa como en Chrome, **sin cambiar
ClicGest** (su web normal, la de un ordenador).

### Por qué QuickJS y no Duktape

NetSurf trae sus enlaces hechos para **Duktape**, pero Duktape se queda en
ES5: no entiende `class`, las funciones flecha, `const`/`let` ni las
plantillas, y Bootstrap 5.3 (`bootstrap.bundle.min.js`) y el JavaScript de
ClicGest (más de 5.000 funciones flecha) son ES2015 y más: con Duktape no se
habría ejecutado ni una línea de ClicGest. **QuickJS** (ES2020) ya estaba
hecho y probado en Hispa-486 (HSPJS.DLL, la del Navegador propio, en el
disquete «Internet»): no ocupa sitio de más en los disquetes del Navegador
completo. Lo que faltaba era unirlo al árbol de libdom; se ha hecho como en
el Navegador propio: el DOM y lo de window los hace el **preludio** en
JavaScript (`w32/navega/dom.js`, el mismo archivo, sin tocar), sobre unas 70
funciones de C (`w32/surf/guion.c`, el objeto `N`) que esta vez trabajan con
libdom. `w32/surf/surf.js` cambia lo que en NetSurf se sabe de verdad: las
medidas de las cajas.

### Qué hay (probado en ClicGest)

- Todo lo del preludio del Navegador propio (ver NAVEGADOR.md,
  «JavaScript»): `document` y los nodos, `querySelector(All)`, `matches`,
  `closest` (los selectores de `w32/navega/dom.c`, pasados a libdom:
  `w32/surf/selector.c`), `classList`, `dataset`, `style`, `innerHTML` (con
  el lector de fragmentos de hubbub; dentro de una tabla o de un `<select>`,
  en el suyo), eventos con captura y burbuja, `setTimeout`/`setInterval`,
  promesas, `fetch` y `XMLHttpRequest` (con las galletas, la misma conexión
  que las páginas, las redirecciones; a otro origen, con CORS y SameSite como
  el Navegador propio: NAVEGADOR.md, «Lo que puede pedir una página»; lo
  decide `w32/surf/guion.c` con `w32/navega/cors.h`), `localStorage` (en
  `NAVEGA2.ALM`, por el origen que saca C de la dirección),
  `document.cookie`, `alert`/`confirm` (los cuadros de Hispa Ventanas),
  `console` (a `SURFJS.LOG`).
- **Las medidas de verdad** (`surf.js`): `getBoundingClientRect`,
  `offsetWidth/Height/Top/Left/Parent`, `clientWidth`, `scrollHeight`,
  `getComputedStyle` (de la caja de NetSurf: `display`, `position`, rellenos,
  márgenes, bordes, colores, `z-index`; las transiciones, 0 s),
  `scrollY`/`scrollTo`. Si el árbol ha cambiado en ese mismo turno, las cajas
  se rehacen antes de medir (como hacen los navegadores grandes).
- **Las cajas otra vez** (`w32/surf/rehaz.c`): NetSurf 3.11 maqueta una vez
  («NS layout is static»). Tras cada guion, evento o temporizador que cambia
  el árbol, se rehacen las cajas de la página entera desde libdom (con los
  estilos otra vez) y se maqueta, sin perder el sitio, lo escrito en los
  campos (los controles se guardan en su nodo), el campo del cursor ni las
  imágenes ya bajadas (el objeto se vuelve a usar).
- **Lo que va por encima**: un elemento con `position: absolute` o `fixed` y
  `z-index` mayor que 0 (los menús desplegables de Bootstrap) se pinta al
  final y recibe los clics antes que lo que tiene debajo (`redraw.c` e
  `interaction.c`, marcado `[HISPA]`). Popper (que coloca los menús con
  `transform: translate()`, que NetSurf no tiene) se cambia por `left`/`top`.
- **Los eventos de la persona**: `mousedown`, `mouseup` y `click` (al
  elemento de dentro: el `<a>`, no el `<li>` de alrededor; si la página dice
  `preventDefault()`, no se sigue el enlace ni se envía el formulario);
  `input` al escribir en un campo, `change` en las casillas, `keydown` (al
  campo del cursor), `focus`/`blur`, `submit` (con `preventDefault()`, lo
  manda la página con `fetch`), `DOMContentLoaded` y `load`.
- **El «Ver» de la contraseña**: cambiar `type` de `password` a `text`
  rehace el campo con su texto.

En ClicGest (la demostración, 20/11/2026, QEMU): el «Ver» de la contraseña
del inicio de sesión (`navegador2/20_js_login_ver.png`), los botones
`.js-only`, «Herramientas» de socios (el menú de Bootstrap, encima de la
lista: `21_js_socios_herramientas.png`), el catálogo de la biblioteca y su
buscador (`22_js_biblioteca_buscar.png`); la página de la prueba, con su menú
(`23_js_prueba_menu.png`); sin errores de JavaScript en `SURFJS.LOG` en las páginas
probadas (panel, catálogo, socios).

Lo que no hay o es distinto:

- Los **globos de ayuda** de Bootstrap (`tooltip`): `matchMedia('(hover:
  none)')` dice que no hay ratón que pase por encima (como en el Navegador
  propio) y ClicGest no los arranca (ni baja Bootstrap para ellos).
- Las **ventanas** de Bootstrap (`modal`, `offcanvas`: «Columnas...» de
  socios) se abren, pero NetSurf no tiene `opacity`, `transform` ni
  `position: fixed` de verdad: salen arriba de la página o cortadas por la
  izquierda.
- `<details>`, `mouseover`, arrastrar, `canvas`, los `iframe` con JavaScript.

### Cómo está hecho

- `w32/surf/guion.c`: la interfaz de NetSurf para un motor de JavaScript
  (`javascript/js.h`, en vez de `javascript/none`), con QuickJS de
  HSPJS.DLL: un motor por página (se suelta entero al irse; sin páginas, la
  memoria de JavaScript vuelve toda al sistema), un objeto por nodo de
  libdom, el objeto `N`, los turnos con su tope de tiempo (15 s), los
  temporizadores con el planificador de NetSurf, lo compilado guardado
  (Bootstrap y `mobile.js` se compilan una vez), lo que no se puede hacer en
  medio de NetSurf (ir a otra página, enviar un formulario) desde el
  planificador, y nada que toque el árbol mientras NetSurf hace las cajas a
  trozos.
- `w32/surf/red.c`: `fetch` y `XMLHttpRequest` van a la misma cola que lo de
  NetSurf (una petición cada vez, por la conexión guardada).
- Ganchos `[HISPA]` en NetSurf: `html.c` (`DOMContentLoaded`),
  `interaction.c` (clics, teclas, foco; lo que va por encima),
  `redraw.c` (pintarlo al final), `form.c` (`submit`), `forms.c` (los
  controles sin formulario, guardados), `box_textarea.c` (el campo que ya
  había se queda; `input`), `box_construct.c` (las cajas de una vez),
  `object.c` (las imágenes sin caja), `css/select.c` (olvidar lo de libcss de
  un nodo); en libdom, `element.c` (cambiar `class` cambia la lista de
  clases: antes no) y `event.c` (sin `time()` en cada evento: preguntaba a
  DOS y hacía cada `insertBefore` seis veces más lento).
- `make SURF_PERFIL=-DPERFIL`: cuántas veces y cuántos ciclos lleva cada
  función de `N` (en `SURFJS.LOG`).

### Medidas del JavaScript

En QEMU (sin KVM, en el VPS; un 486 DX2-66 es 4-6 veces más lento), 32 MB:

| | |
|---|---|
| El preludio (dom.js + surf.js), por página | ~0,2 s; 590 KB de JavaScript en uso |
| Bootstrap 5.3 (78 KB) la primera vez / ya compilado | 0,11-0,22 s / 0,06 s |
| JavaScript de una página de ClicGest (panel, catálogo) | 0,7-1,4 MB en uso |
| Socios (298 KB de HTML, Bootstrap, 54 filas) | 2,7 MB en uso; su guion de 22 KB, 0,33 s (2,6 s antes de quitar `time()` de libdom) |
| Rehacer las cajas tras un clic | panel 0,2 s; catálogo 0,4 s; socios 0,9-1,2 s |

El JavaScript gasta en su propio montón (el de HSPJS.DLL), aparte de la
memoria de NetSurf: con 32 MB va holgado; con 16 MB, las páginas sencillas y
el panel caben (1-2 MB más que sin JavaScript), la lista de socios no (ya no
cabía sin JavaScript).

## Medidas

En QEMU (sin KVM, en el VPS), 32 MB, 800×600, por la red de QEMU a Internet:

| Página (por orden, en la misma ventana) | Tiempo (de pedirla a «Hecho») | Memoria en uso (`memoria.c`, KB) | Pedida al sistema (KB) |
|---|---|---|---|
| Página HTML+CSS del disco | 0,3-0,7 s | 2.000 | 4.600 |
| https://clicgest.es/login (Bootstrap 5, primera vez) | 3,7 s | 5.350 (lo más: 7.590) | 7.680 |
| demo: panel (`/demo` → `/dashboard`) | 4,2 s | 6.660 (8.760) | 9.730 |
| demo: catálogo de la biblioteca | 5,4 s | 7.490 (9.600) | 9.730 |
| demo: socios (54 filas en una tabla) | 22,1 s | 10.960 (13.060) | 13.820 |
| https://hispados.es/ | 4,6 s | 10.850 | 13.820 |

Con el Administrador de tareas (`navegador2/07_memoria.png`): 30.120 KB
libres antes de abrirlo, 13.392 KB tras esas cuatro páginas de ClicGest: el
Navegador completo gasta ~16,7 MB con todo (el programa, el montón, la pila,
el búfer de la página, las letras). Con 16 MB de memoria (14 MB libres en
Hispa Ventanas) entran las páginas sencillas, hispados.es y ClicGest hasta el
catálogo (probado en QEMU con `-m 16`: el entrar, el panel y el catálogo van,
con 3 MB libres tras el panel: `navegador2/09_memoria_16mb.png`); la lista de
socios (54 filas) **no cabe con 16 MB**: se queda en «Pidiendo» sin avisar.
Con 24-32 MB va todo. Ver «Lo que falta».

- **Tamaño:** NAVEGA2.EXE 1.096 KB (código 1,06 MB: libcss 330 KB,
  NetSurf 420 KB, libdom+hubbub 200 KB...); con las letras y los recursos,
  1,36 MB (cabe en un disquete).
- **Memoria:** el montón de HW32 redondea a potencias de dos (de 33 a 64
  octetos gasta 64) y NetSurf pide miles de trozos pequeños: con él, tras
  seis páginas de ClicGest el Administrador de tareas daba 21,8 MB menos;
  con `memoria.c` (trozos de 8 en 8 con listas por tamaño, bloques de 1 MB
  con VirtualAlloc que se devuelven si quedan vacíos) y la caché de NetSurf
  en 1 MB, 14,7 MB. Lo que más gasta es Bootstrap ya leído por libcss
  (sus 4.700 reglas con los selectores), el DOM y las cajas de la página y
  el búfer de la página (800×500×4 = 1,5 MB).
- **Pentium y 486:** QEMU sin KVM en el VPS va, para este código, como un
  Pentium de 200-300 MHz. Un 486 DX2-66 es 4-6 veces más lento: una página
  de ClicGest de 3-4 s en QEMU serían unos 15-25 s en un 486 (la primera
  vez: Bootstrap se lee una sola vez y las siguientes páginas son la mitad);
  en un Pentium 133, 6-10 s. El apretón de manos de TLS (HSPTLS, con la
  conexión que se guarda) va aparte, como en el Navegador propio.

### Medidas con el pintado nuevo (20/11/2026)

En QEMU (32 MB, 800×600), por orden en la misma ventana
(`tools/surf_capturas.sh`):

| Página | Tiempo | Memoria en uso (KB) | Lo más (KB) |
|---|---|---|---|
| https://clicgest.es/ (la primera: Bootstrap, las dos letras de la web, una página de 20.000 puntos de alto) | 14,3 s | 9.001 | 11.238 |
| /login | 1,0 s | 9.219 | 11.325 |
| /demo → panel | 3,4 s | 7.656 | 11.325 |
| la biblioteca | 3,4 s | 8.270 | 11.325 |
| el catálogo | 4,6 s | 8.720 | 11.325 |

Las letras de la web se quedan en memoria: Bootstrap Icons (WOFF, 176 KB
bajado) son 478 KB en TTF; Plus Jakarta Sans (WOFF2, 27 KB), 61 KB (una
sola para todos los pesos). Al decodificar un WOFF2 grande (Brotli) el pico
es de ~1,3 MB más, un momento. El programa crece 72 KB (Brotli y WOFF 40 KB;
grid, letras, esquinas y lo demás, 32 KB). Con 16 MB sigue justo: ver abajo.

## Lo que falta (el plan)

1. ~~**JavaScript (fase 5)**~~: hecho el 20/11/2026 con QuickJS (ver
   «JavaScript»). Queda: `opacity` y `position: fixed` de verdad (las
   ventanas de Bootstrap), rehacer solo la parte de las cajas que cambia (en
   un 486, las páginas grandes tardan en cada clic), los eventos del ratón
   que pasa por encima (`mouseover`: los globos de ayuda).
2. **SVG**: el XML de libdom con expat (o un lector XML pequeño propio).
3. ~~Iconos de letra y `@font-face`~~: hecho el 20/11 (ver «Comparado con
   Chrome»). Queda: los pesos de las letras variables (`gvar`), `<details>`.
4. **Cursiva** (inclinando las letras al pintarlas) y el «hinting».
5. **Descargas** (al Lector, como el Navegador propio: `gui_download_table`)
   e **imprimir** (la página en puntos al Lector).
6. ~~Grid y `gap`~~: hecho el 20/11 (`layout_grid.c`). Queda: las áreas
   con nombre, `grid-row`, `grid-template-rows`.
7. **Memoria** para 16 MB justos: que una página que no cabe dé un aviso
   (ahora se queda en «Pidiendo»), soltar las páginas del historial, una
   caché de letras más pequeña, Bootstrap sin las reglas añadidas que no se
   usan, el búfer de la página en 8 bits.
8. **Tramado** de colores al pasar a la paleta (los fondos pastel).

## Pruebas

`tests/run_navegador2_tests.sh` (en `make test`; 25 comprobaciones, unos 6
minutos; sin Internet: `tests/tools/surf_servidor.py`
por HTTPS con la raíz de pruebas de `tests/tools/nav_certs.sh`): el acceso
del menú Inicio, una página moderna (hoja de fuera con variables de CSS,
botones con variables por clase, degradado, flexbox, PNG y JPEG; el
User-Agent `HispaSurf`), una redirección, una página del disco, Atrás, un
formulario GET y una página larga (Av Pág, Fin), con las capturas
(`GUARDAR=carpeta`) y los tiempos de `SURF.LOG`.

`tests/run_navegador2_js_tests.sh` (en `make test`; 14 comprobaciones,
unos 3 minutos; sin Internet: `tests/tools/surfjs_servidor.py` por HTTP): un
guion de hoy que cambia la cabecera (las cajas otra vez), `setTimeout`,
`localStorage`, `fetch` con su galleta y una cabecera, un menú desplegable
con `z-index` que se pinta encima y recibe el clic, un clic delegado con
`closest()` y `preventDefault()`, `classList.toggle`, el evento `input` y un
formulario parado en `submit` que se manda con `fetch` POST.

`tests/run_surf_pintado_tests.sh` (en `make test`; en el anfitrión, sin QEMU
ni Internet, unos segundos tras compilar `surfppm`): pinta
`tests/surf/pagina.html` con el NetSurf de Linux (`w32/surf/anfitrion.c`) y
mira los puntos: un icono de letra de prueba (`tests/surf/icono.*`, hecho
con `tests/surf/genera_icono.py`) en TTF, WOFF y WOFF2 con `::before`; grid
(`repeat(3, 1fr)` con `gap`, `auto-fill` con `minmax()`, `grid-column: 1 /
-1`); `justify-content: space-between`; esquinas redondas; sombra; un campo
con `min-height: 48px` y `border-box` (48 de alto) y su `placeholder`.
`tests/run_surf_woff_tests.sh`: el decodificador WOFF/WOFF2 glifo a glifo
contra el TTF de origen, y con archivos rotos.

Para comparar con Chrome a mano: `make surfppm` y
`build/anf/surfppm [-entera] -o pagina.ppm https://...` (las galletas en
`/tmp/surfppm.galletas`: abrir antes `https://clicgest.es/demo` para entrar
en la demo; `SURF_CAJAS=1` saca el árbol de cajas), y
`tools/surf_capturas.sh CARPETA URL...` para las capturas en QEMU.
