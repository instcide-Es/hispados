# El Navegador de Hispa-486

Un navegador web sencillo para Hispa Ventanas: páginas HTML con formularios y
JavaScript, por HTTP y por HTTPS. Es un programa Hispa32 (`NAVEGA.EXE`) y va, con lo que
necesita, en el segundo disquete de Hispa-486: «Internet».
Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.

Nació de un pedido de Carlos (18/11/2026): «Si le ponemos un explorador web
al 486 ya lo podemos usar para la biblio». Y el mismo día, otro: «Haz el
navegador compatible con ClicGest.es»: galletas que se guardan, imágenes,
subir y bajar archivos (ver «Para usar ClicGest entero», más abajo). La «biblio» es la biblioteca de la
asociación dentro de ClicGest: páginas que se hacen en el servidor, casi sin
JavaScript. Con el Navegador se entra en ClicGest y se usa la biblioteca
entera: catálogo, prestar, devolver, lectores, historial (ver «Con ClicGest»).

## Qué hace

- **Barra:** Atrás, Adelante, Recargar, Inicio y la dirección (con «Ir»).
  Abajo, la línea de estado: lo que está haciendo, la dirección del enlace que
  hay bajo el ratón y si la conexión va cifrada («Conexión cifrada», en
  verde) o no («Sin cifrar», en granate).
- **HTTP/1.1 y HTTPS** (TLS 1.2 con comprobación del certificado: ver
  docs/RED.md, «La red de los programas Hispa32»). Redirecciones (hasta 10),
  respuestas a trozos (`chunked`), con longitud o hasta el cierre; la
  conexión se guarda abierta para la petición siguiente (el apretón de manos
  de TLS es lo caro en un 486).
- **Galletas (cookies):** con `Path`, `Domain`, `Secure`, `HttpOnly`,
  `SameSite`, `Max-Age` y `Expires` (la fecha entera, en los tres formatos
  de HTTP). Las de sesión, en memoria hasta que se cierra el Navegador; las
  que dicen hasta cuándo valen (como la de «Mantener la sesión iniciada» de
  ClicGest, 180 días) se guardan al cerrarlo, y cuando cambian, en
  `GALLETAS.TXT` junto al programa: al abrirlo otra vez se sigue dentro.
  `SameSite`: lo que pide una página de otro sitio no lleva las `Strict`, ni
  las `Lax` si es un POST o una imagen. Hasta 64 galletas.
- **Formularios:** GET y POST (`application/x-www-form-urlencoded`, en UTF-8);
  campos de texto (text, search, email, number, date, tel, url), claves,
  ocultos, casillas, botones de radio, listas (`<select>`, con su lista
  desplegable), áreas de texto, botones de enviar y de restablecer;
  `required` (también en casillas, listas, botones de radio y archivos),
  `pattern` (las expresiones regulares corrientes), `type=email`,
  `minlength`, `maxlength`, `readonly`, `disabled`, `placeholder`,
  `formaction`, `formmethod`, `formenctype`, `novalidate`. Lo que falta o no
  vale se dice en un cuadro («Falta rellenar «Email».», «No tiene la forma
  que se pide «Título».» con el `title` del campo) y el foco va al campo.
  Intro en un campo envía el formulario.
- **Subir archivos:** `<input type=file>` es un campo con la ruta y un botón
  «Examinar...» que abre el diálogo de abrir de Hispa Ventanas (o se escribe
  la ruta). El formulario con `enctype="multipart/form-data"` se envía como
  tal (RFC 7578), con el tipo del archivo por su extensión; hasta 2 MB.
- **Descargas:** lo que no es una página (un PDF, un recibo, un adjunto con
  `Content-Disposition`) se guarda en el disco: en cuanto llegan las
  cabeceras sale el diálogo de guardar, con el nombre que propone el
  servidor en 8+3 (`Recibo 2026-11.pdf` → `RECIBO20.PDF`), y lo demás va
  derecho al archivo (no a la memoria: hasta 64 MB). Al acabar, si hay un
  programa para ese tipo (`\HISPADOS\ASOCIA.INI`, como el Administrador de
  archivos; o el Lector para los PDF y Mirador para las imágenes), se ofrece
  abrirlo con él; desde el Lector se imprime.
- **Imágenes:** PNG, GIF, JPEG (también progresivo), BMP e ICO, y las de
  `data:`. Se bajan después de la página, una a una y por la misma
  conexión, y se ven en la rejilla de letras: una imagen ocupa un bloque de
  letras y filas, del tamaño que diga la página (`width` y `height`, o la
  hoja de estilo en `px`) o el suyo, reducido si hace falta (como mucho
  360×270 puntos): la media de los puntos (un JPEG grande se descodifica ya
  a 1/2, 1/4 u 1/8) y los 256 colores de Hispa Ventanas repartiendo el error.
  Una imagen que es lo primero de un bloque con más cosas (la portada de una
  ficha) va a la izquierda y el texto a su lado. Lo ya visto se recuerda
  (320 KB): el logotipo no se baja en cada página. Si no se puede ver (un
  SVG, una rota), su texto alternativo. Esc deja de bajarlas.
- **HTML tolerante** (etiquetas sin cerrar o mal anidadas): títulos,
  párrafos, negrita, subrayado, tachado, listas, definiciones, citas, `<pre>`,
  enlaces, tablas (anchos según el contenido, filas alternas sombreadas;
  si no caben, un espacio entre columnas en vez de dos), `<details>` (se
  pliega y se despliega), reglas, entidades, `<meta http-equiv=refresh>`.
  UTF-8 (y Latin-1) a la letra de Hispa Ventanas.
- **CSS, lo mínimo**, para que la página se lea aunque no se parezca al
  diseño: de los `<style>` de la página y de los atributos `style`,
  `display: none` (y `block`, `flex`...: lo que hace línea aparte),
  `visibility: hidden`, `color` y `background-color` (si quedan legibles),
  `font-weight`, `text-align`, `text-decoration`, con selectores sencillos
  (etiqueta, `.clase`, `#id`) y `@media` según el ancho de la ventana. El
  atributo `hidden`. Las hojas de estilo de fuera (`<link>`) no se bajan; de
  las de Bootstrap, que son las de ClicGest, se sabe de memoria qué clases
  esconden cosas (`d-none`, `collapse`, `modal`, `dropdown-menu`...) y unas
  pocas más (`text-center`, `fw-bold`, `text-danger`, `alert`, `badge`,
  `btn`).
- **JavaScript** (ES2020, con QuickJS: ver «JavaScript», más abajo). Se
  apaga con `JavaScript=NO` en NAVEGA.INI; entonces, lo que se puede suplir
  sin él, se suple:
  - `onsubmit="return confirm('...')"` (y en `onclick`): antes de enviar sale
    la pregunta en un cuadro de Hispa Ventanas (Sí o No);
  - `<select onchange="this.form.submit()">`: al elegir, se envía;
  - `<noscript>` se enseña; `<script>` no se enseña ni se ejecuta;
  - un botón `type="button"` (solo hace algo con JavaScript) sale en gris y,
    si se pulsa, lo dice; si no tiene rótulo, no sale.
- **Menús largos, plegados.** Un `<nav>` o `<aside>` con más de 8 enlaces
  (el menú lateral de ClicGest tiene 30) sale en una línea, «[+] Menú
  principal (30 enlaces)», que se despliega al pulsarla. Y si la página dice
  cuál es su parte principal (`<main>`, `role="main"`, la clase `main`), al
  abrirla se empieza por ahí: lo de antes queda arriba, a un paso.

## Cómo se maneja

| | |
|---|---|
| Ratón | un clic en un enlace, un botón o un campo; la barra de la derecha desplaza (se puede arrastrar); la rueda desplaza la página (3 filas por paso) y, con una lista abierta, sus opciones |
| Tab | al enlace o campo siguiente (el enlace elegido sale en blanco sobre añil); Mayús+Tab, al anterior |
| Intro | abre el enlace, pulsa el botón, envía el formulario del campo; en el de un archivo, abre el diálogo |
| Espacio | marca la casilla o abre la lista; fuera de un campo, baja una pantalla |
| Flechas, Re Pág, Av Pág, Inicio, Fin | mueven la página (en un campo, el cursor) |
| Retroceso | fuera de un campo: Atrás |
| F6 o Ctrl+L | a la dirección (queda elegida entera: lo que se escriba la sustituye) |
| F5 o Ctrl+R | recargar |
| (con JavaScript) | las teclas y los clics van primero a la página: en el buscador de «Prestar», las flechas eligen y Intro abre |
| Esc | para la carga; en la dirección, la deja como estaba |

Una dirección sin `http://` delante se pide por HTTP (si el sitio es de
HTTPS, él mismo redirige).

`NAVEGA.INI`, junto al programa (todas las líneas son opcionales):

```
Inicio=https://clicgest.es/biblioteca     ; la página de arranque y del botón Inicio
Idioma=es                                 ; el idioma que se pide (Accept-Language): es, ca, en
AlContenido=NO                            ; no empezar por la parte principal de la página
Favorito=La biblioteca|https://clicgest.es/biblioteca    ; enlaces de la página de inicio
Descargas=C:\DOCUMEN                     ; la carpeta que propone para guardar lo que se baja (si no: C:\)
Galletas=NO                               ; no guardar las galletas al cerrar (un equipo de todos)
JavaScript=NO                             ; sin JavaScript (por omisión, con)
TiempoJS=15                               ; segundos que puede tardar un guion o un evento (15)
MemoriaJS=7168                            ; KB que puede usar el JavaScript de una página (7168)
```

Sin `Inicio=`, arranca en una página propia (`about:inicio`) con los
favoritos y cómo se maneja. Si se le pasa una dirección al abrirlo, abre esa.

`GALLETAS.TXT` (junto al programa) lo escribe el Navegador: borrarlo es
cerrar todas las sesiones guardadas en ese equipo.

### Cómo se presenta (para que ClicGest le dé una versión adaptada)

Cada petición lleva estas cabeceras (y nada más):

```
GET /biblioteca/catalogo HTTP/1.1
Host: clicgest.es
User-Agent: HispaNavegador/0.2 (Hispa-486)
Accept: text/html,text/plain;q=0.9,*/*;q=0.5
Accept-Language: es
Connection: keep-alive
Cookie: PHPSESSID=...                       (si hay galletas para esa dirección)
```

Las imágenes piden `Accept: image/png,image/gif,image/jpeg,image/bmp,image/x-icon;q=0.9,*/*;q=0.1`;
un POST añade `Content-Type` (`application/x-www-form-urlencoded`, o
`multipart/form-data; boundary=----HispaNavegador...`) y `Content-Length`.
No manda `Referer`, `Origin`, `Accept-Encoding` (no entiende gzip) ni
`Upgrade-Insecure-Requests`. Para reconocerlo basta con que el User-Agent
empiece por `HispaNavegador/`; la versión es la de `NAV_VERSION` en
`w32/navega/nav.h` (el número que sale en la página de inicio).

## Dónde está y qué necesita

- **El disquete «Internet»** (`make internet` → `build/internet.img`, 1.318 KB
  ocupados de 1.440; con las imágenes, las descargas y el JavaScript caben
  todavía en uno): `NAVEGA.EXE`, `HSPJS.DLL` (el JavaScript), `HTTPS.EXE`,
  `WSOCK32.DLL`, `HSPTLS.DLL`, `MSVCRT.DLL`, `HW32.COM`, `NAVEGA.INI` y
  `LEEME.TXT`. Si el Navegador crece más, el JavaScript irá en un segundo
  disquete («Internet 2»: `HSPJS.DLL` y `JS.EXE`), que se copia a la misma
  carpeta del disco duro. Se puede usar desde el
  disquete (abriendo `B:\NAVEGA.EXE` desde el Administrador de archivos) o,
  lo normal, copiado al disco duro: `XCOPIAR B:\*.* C:\HISPADOS\INTERNET\`.
- **En el menú Inicio:** si al arrancar Hispa Ventanas `NAVEGA.EXE` está en
  la carpeta `INTERNET` de la de Hispa Ventanas (`\HISPADOS\INTERNET`), se
  crea, una sola vez, el grupo **«Internet»** con el acceso **«Navegador»**
  (clave `Internet=1` de `[Centro]`, en CENTRO.INI). Sale en el Centro de
  programas y, en el escritorio de Hispa-486, en Inicio › Internet.
- **Antes de entrar en Hispa Ventanas:** `HSPXMS` en CONFIG.SYS y `HSPRED`
  cargado (el controlador de la tarjeta de red: docs/RED.md). Si no hay red,
  el Navegador lo dice en la propia página.
- **Para abrir lo que se descarga:** el Lector (los PDF) va en el disquete
  «Oficina» y Mirador (las imágenes) en el primero; si no están, lo bajado
  se guarda igual y se dice dónde.
- **El reloj en hora.** Un certificado solo vale entre dos fechas: con el
  reloj del equipo en 1980 (la pila gastada) ninguna página https se abre, y
  el aviso lo dice («Compruebe la fecha del equipo»).
- **Memoria:** ver «Medidas». Corre en un equipo de 8 MB.

## Con ClicGest

Probado el 18/11/2026 contra clicgest.es de verdad (cuenta de demostración,
tenant 1), desde QEMU:

- `https://clicgest.es/demo` entra en la demostración sin clave (dos
  redirecciones y la galleta `PHPSESSID`) y llega al panel.
- **Biblioteca:** la portada, el catálogo (con sus listas de categoría y
  orden), prestar en sus tres pasos (buscar a la persona, buscar el libro,
  confirmar con su nota: POST y redirección, con el aviso «Hecho»), devolver
  (con «Cómo vuelve» elegido de la lista), el historial y los lectores. Se
  hizo un préstamo entero y su devolución; las capturas, en
  `docs/486/navegador/`.
- **La parte pública** (`/b/{asociación}`): el escaparate y el catálogo con
  su búsqueda se leen bien (no llevan JavaScript). La de la demostración no
  está publicada (`/b/demo` da 404: falta activarla en los ajustes de la
  biblioteca de la demo).
- La entrada normal (`/login`: correo, clave y la casilla) es un formulario
  corriente, con su campo oculto `_csrf`: funciona igual. No hay captcha ni
  comprobación por JavaScript.

ClicGest reconoce al Navegador por su nombre (User-Agent) y le da su **modo
ligero** (un menú corto arriba, sin el armazón del teléfono): ver
docs/navegador_ligero.md en ClicGest. Desde «Ver la versión normal» se pasa a
la de siempre, la de un ordenador, que es la que tiene JavaScript de verdad.

Con JavaScript (18/11/2026, contra clicgest.es de verdad, desde QEMU; capturas
en `docs/486/navegador/js_*.png`):

- **El menú lateral:** el botón «Menú» (el ☰ de ClicGest, con su nombre: el
  dibujo no está en la letra de Hispa Ventanas) lo despliega y lo vuelve a
  plegar; «[+] Menú principal» sigue sirviendo igual.
- **El tutorial de la biblioteca:** «Ver desde el paso 1», «Anterior»,
  «Siguiente», «Cerrar el repaso» y «Ocultar pasos» (que se recuerda, en
  localStorage).
- **El buscador de «Prestar»:** al escribir sale la lista de personas que
  casan («4 coinciden...»), se acota con cada letra, las flechas marcan una
  fila (en ocre) e Intro, o un clic en la fila, la elige. (Sin elegir a
  nadie: en la prueba no se prestó nada.)
- **Las pestañas** de una salida («Falta pagar», «Pagado todo»,
  «Asientos»...): son las de Bootstrap, con su JavaScript de verdad
  (bootstrap.bundle.min.js, 80 KB).
- Ninguna página de la biblioteca deja errores de JavaScript en NAVEGA.LOG
  (portada, prestar, catálogo, devolver, lectores, historial, ajustes,
  comprobar, etiquetas, por atender, reseñas, sugerencias y suspensiones).

Lo que se sigue notando (con y sin JavaScript):

- Sin las hojas de estilo de fuera, la página se ve en una columna, como
  siempre; lo que esconde o enseña ClicGest con sus clases propias (y no con
  las de Bootstrap o con `hidden`) puede salir de más.
- Las letras mayúsculas con acento que no están en la letra de Hispa
  Ventanas salen sin él («Ultimos»), y el euro, como «E»: la letra es la de
  la página de códigos 437, que tiene «É» y «Ñ» pero no «Á», «Í», «Ó», «Ú»,
  «À», «È», «Ò», «€» (harían falta esos glifos en `ventanas/fuente.txt`, en
  sitios de la 437 que no se usen, y cambiar `w32_1252`/`w32_437`).

- Sin JavaScript (`JavaScript=NO`): el menú lateral, plegado en «[+] Menú
  principal»; los botones del tutorial, en gris; en «Prestar», se escribe y
  se pulsa «Buscar».

### Para usar ClicGest entero (18/11/2026, la tarde)

Probado otra vez contra clicgest.es de verdad (la demostración, solo
lecturas: ni préstamos ni cambios), capturas en `docs/486/navegador/`
(desde la 22):

- **Se reconoce:** ClicGest ya le da su versión adaptada por el User-Agent
  (el menú en dos líneas arriba, «Ver la versión normal»):
  `23_catalogo_menu_adaptado.png`, `31_panel.png`.
- **Portadas e imágenes:** el catálogo con las portadas (JPEG de 600×900,
  56×80 en la página) al lado de cada libro (`22_catalogo_portadas.png`), la
  ficha con la portada grande y sus datos al lado (`24_ficha_portada.png`).
  Con el catálogo y sus 10 portadas abierto, el Navegador gasta lo mismo que
  con la biblioteca de la prueba: 1,7 MB (`32_memoria_catalogo.png`: 13.740 K
  libres antes, 12.036 K con él).
- **Subir la portada:** la ficha del libro, «Portada», tiene su campo de
  archivo con «Examinar...» y el diálogo de abrir (`25_portada_examinar.png`,
  `26_portada_dialogo.png`); «Subir» sin archivo no envía nada y lo dice
  (`27_portada_falta_archivo.png`). No se ha subido nada a la demostración;
  la subida entera (el diálogo, `multipart/form-data`, y que llega el
  archivo igual) la comprueba la prueba automática.
- **Descargas:** el «Manual (PDF)» del panel (3,3 MB, con
  `Content-Disposition`) va al diálogo de guardar (`28_manual_guardar.png`),
  al disco sin pasar por la memoria (`29_manual_guardado.png`) y al Lector,
  con sus 653 páginas (`30_manual_lector.png`).
- **La sesión guardada:** con «Mantener la sesión iniciada» marcada, ClicGest
  da una galleta de 180 días que ahora se guarda (`GALLETAS.TXT`); la prueba
  automática cierra el Navegador, lo abre otra vez y entra sin clave. (La
  demostración entra sin clave y no da esa galleta: se ha comprobado con el
  servidor de pruebas.)
- **Con JavaScript:** el mismo User-Agent va en las peticiones de `fetch` y
  `XMLHttpRequest`, y `navigator.userAgent` dice lo mismo
  (`HispaNavegador/0.2 (Hispa-486)`); `document.cookie` lee y escribe las
  mismas galletas, las que se guardan en `GALLETAS.TXT` también, pero no ve
  las `HttpOnly` ni las puede pisar; `fetch` sigue SameSite como lo demás
  (a otro sitio solo van las `SameSite=None`). Si un guion cambia el `src`
  de un `<img>`, se baja la imagen nueva. Las imágenes y las peticiones de
  JavaScript usan la misma conexión: primero lo de JavaScript, y una imagen
  que ya se está bajando acaba antes.

## JavaScript

El motor es **QuickJS** (Fabrice Bellard y Charlie Gordon, licencia MIT,
versión 2026-06-04: `terceros/quickjs`, con su LEEME): JavaScript moderno
(ES2020 y más: funciones flecha, clases, `const`/`let`, plantillas,
`async`/`await`, Map, Set, Proxy, expresiones regulares de Unicode, BigInt,
JSON), que es lo que usa ClicGest (5.400 funciones flecha, `fetch`, clases).
Duktape, la otra opción pequeña, se queda en ES5. Va en una DLL aparte,
**HSPJS.DLL**, que también usa **JS.EXE**, la consola para probarlo:

```
JS [/M] [/T:segundos] archivo.js     (/M: memoria y tiempo al acabar)
JS /E "print(2 ** 10)"
```

### Qué hay

- **document y los nodos:** `getElementById`, `querySelector(All)`,
  `matches` y `closest` con los selectores de CSS corrientes (etiqueta,
  `#id`, `.clase`, `[atributo=valor]` y los `^= $= *= ~= |=`, los
  combinadores ` > + ~`, listas con comas, `:not() :is() :has()
  :first-child :last-child :nth-child() :checked :disabled :scope`...),
  `createElement`, `textContent`, `innerHTML` y `outerHTML` (se leen con el
  mismo lector de HTML de la página), `insertAdjacentHTML`, `appendChild`,
  `insertBefore`, `append`, `prepend`, `before`, `after`, `remove`,
  `replaceChildren`, `cloneNode`, `children`, `parentElement`, `dataset`,
  `classList`, `style` (se escribe en el atributo `style`; la maqueta mira
  `display`, `visibility`, colores, negrita y alineación), `hidden`,
  atributos, `DocumentFragment`, `<template>`, `<details open>`, `<dialog>`
  (`showModal`, `close`).
- **Formularios:** `value`, `checked`, `selected`, `selectedIndex`,
  `disabled`, `options`, `elements`, `form`, `submit()`,
  `requestSubmit()`, `reset()`, `FormData`, `checkValidity()`.
- **Eventos:** `addEventListener` (con captura y burbuja, `once`, `signal`),
  los `onclick="..."` del HTML y las propiedades `onclick = ...`;
  `preventDefault`, `stopPropagation`, `CustomEvent` y `dispatchEvent`. Los
  que manda el Navegador: `click` (y `mousedown`, `mouseup`), `input`,
  `change`, `submit`, `keydown` (con `key` y `code`: flechas, Intro, Esc,
  Tab, letras...), `focus`, `blur`, `focusin`, `focusout`, `toggle`,
  `DOMContentLoaded`, `load`, `pageshow`, `readystatechange`, `resize`.
  Lo que tiene un clic de la página (una fila de una lista, un `<li>` cuyo
  contenedor escucha los clics) se puede pulsar, como un enlace.
- **window:** `setTimeout`, `setInterval`, `requestAnimationFrame`,
  `queueMicrotask`; `alert` y `confirm` (con los cuadros de Hispa Ventanas);
  `prompt` (Hispa Ventanas no tiene un cuadro para escribir: pregunta si
  vale la respuesta que trae la página); `location` (y sus partes, `assign`,
  `replace`, `reload`), `history` (`back`, `pushState`...), `navigator`,
  `matchMedia` (una pantalla de ordenador, sin ratón que pase por encima: sin
  globos de ayuda, ni animaciones), `getComputedStyle` (lo justo:
  `display`), `localStorage` (en `NAVEGA.ALM`, junto al programa, por sitio)
  y `sessionStorage` (mientras el Navegador está abierto), `document.cookie`
  (las galletas de http.c), `URL`, `URLSearchParams`, `Headers`, `Blob`,
  `TextEncoder`, `btoa`/`atob`, `crypto.getRandomValues`, `performance.now`,
  `Intl.NumberFormat` y `DateTimeFormat` y `toLocaleString` en español
  («1.234,50 €», «18 de noviembre de 2026»), `MutationObserver` e
  `IntersectionObserver` (de mentira: no avisan / avisan una vez).
- **La red:** `fetch` y `XMLHttpRequest` (GET y POST, y otros métodos;
  cabeceras, JSON, `FormData` y `URLSearchParams`, que van como un
  formulario corriente), por http.c, con las galletas y siguiendo las
  redirecciones. Una petición a la vez, y solo cuando el Navegador no está
  cargando una página.
- **console:** `log`, `warn`, `error`... van a **NAVEGA.LOG**, junto al
  programa, con los errores que nadie recoge (con su pila: archivo, línea y
  columna) y las promesas rechazadas sin `catch`. Cada página empieza con
  «== dirección» y acaba con una línea «[memoria]». El registro empieza de
  nuevo cada vez que se abre el Navegador.

### Cómo va

- Por cada página que tiene algún `<script>` (o algún `onclick="..."`), un
  motor nuevo (`JSRuntime` y `JSContext`): primero el **preludio**
  (`w32/navega/dom.js`, el DOM y lo de window, hecho en JavaScript sobre lo
  que da el C), luego sus guiones en orden: los de dentro, los de fuera (se
  piden a la red sin parar la ventana; la página ya se ve mientras), los
  `defer` y `async`, y después `DOMContentLoaded` y `load`. Los
  `<noscript>` no se enseñan. Al irse de la página, el motor se suelta entero
  y su memoria vuelve al sistema.
- Lo compilado se guarda mientras el Navegador está abierto (450 KB como
  mucho): el preludio, una vez; los guiones de fuera, por su dirección (con su
  `?v=` de versión, como los pone ClicGest: no se vuelven a pedir); los de
  dentro, por su contenido. Así, en ClicGest, `mobile.js` y los guiones del
  menú de cada página no se piden ni se compilan más que la primera vez.
- Tras cada guion, evento o temporizador, si el árbol ha cambiado, los
  controles del formulario se rehacen (lo escrito se queda) y la página se
  vuelve a maquetar sin perder el sitio ni el foco.
- **El tope:** un guion o un evento que tarde más de `TiempoJS` segundos (15)
  se para (en NAVEGA.LOG: «un guion ha tardado más de 15 s y se ha parado»;
  abajo, en la línea de estado, también) y el Navegador sigue. Y el
  JavaScript de una página no puede usar más de `MemoriaJS` KB (7 MB).

### Cómo está hecho

- `terceros/quickjs/`: QuickJS tal cual, menos una línea (sin `Atomics`,
  que piden hilos). `w32/js/hspjs.c`: lo que QuickJS pide a la biblioteca de
  C y MSVCRT.DLL no tiene: la memoria (un repartidor propio, en zonas de
  256 KB pedidas con VirtualAlloc; el montón de HW32 da potencias de dos y
  desperdiciaba la mitad), las divisiones de 64 bits, la salida y la hora.
  `w32/js/mates.c`: las cuentas de Math con el x87 (fsin, fyl2x, f2xm1,
  fpatan...); la DLL pone el x87 a 53 bits, como Windows, para que las sumas
  redondeen como manda JavaScript. `w32/js/inc/`: las cabeceras mínimas.
- `w32/navega/dom.c`: el árbol desde JavaScript (crear, mover, quitar,
  atributos, `innerHTML` con html.c, sacar el HTML y el texto) y los
  selectores de CSS. `w32/navega/script.c`: la unión: un objeto por nodo
  (siempre el mismo), los guiones, los turnos (y sus promesas), los
  temporizadores (uno de la ventana mientras haya alguno), la cola de la red,
  el almacén, el registro. `w32/navega/dom.js`: el preludio (va dentro de
  NAVEGA.EXE como texto; se compila al abrir el Navegador).
- En los archivos de antes, solo ganchos pequeños marcados `[JS]`: en
  navega.c (los eventos, el foco, el temporizador, el bucle de mensajes, lo
  que JavaScript pide a la ventana), en pagina.c (rehacer los controles, el
  rótulo de un botón sin texto, lo que se pulsa con JavaScript, la fila
  elegida de una lista), en html.c (el código de los `<script>`, que antes
  se tiraba) y en http.c (las cabeceras de `fetch` y `document.cookie`).
- Los textos de la página van en la página 1252 (como siempre); hacia
  JavaScript, en UTF-8. El código de los `<script>` se guarda tal cual.

## Cómo está hecho

En `w32/navega/` (C, `gcc -m32` sin biblioteca y `tools/elf2pe.py --gui`;
5.500 líneas con la red y TLS):

- `html.c`: del HTML al árbol del documento. Lector tolerante (lo que se
  cierra solo: `<li>`, `<p>`, `<td>`, `<option>`...; cierres que no casan),
  entidades, UTF-8 a la página 1252 (la de los programas Hispa32; lo que no
  existe ahí, a lo más parecido; los emoji, fuera). Todo lo de un documento
  vive en una memoria a trozos que se suelta de una vez.
- `pagina.c`: el estilo (el CSS mínimo), los controles de formulario, qué se
  envía, y la maqueta. Como la letra de Hispa Ventanas es fija (8×16), **la
  página es una rejilla de letras**, como en un terminal: cada fila, sus
  letras y un atributo por letra (negrita, subrayado, color, fondo, enlace,
  control). Las tablas miden primero cada columna (lo más ancho y lo que no
  se puede partir) y reparten el ancho; si no cabe, la página es más ancha
  que la ventana y se desplaza de lado.
- `http.c`: direcciones (absolutas a partir de relativas), galletas y la
  petición como una máquina de estados que nunca espera: la ventana sigue
  atendiendo (Esc para la carga). Una conexión guardada por servidor.
- `navega.c`: la ventana. Todo se dibuja con `FillRect` y `TextOutA` (la
  barra, los campos, las listas, la barra de desplazamiento), con los colores
  de la paleta de Hispa Ventanas (`PALETTEINDEX`, que Hispa Ventanas entiende
  desde ahora en los colores de GDI), y las imágenes con `SetDIBitsToDevice`
  (8 bits, colores de la paleta). El historial guarda 40 direcciones y
  por dónde se iba en cada una; volver atrás pide la página otra vez. Aquí
  también: la cola de las imágenes (una petición aparte, después de la
  página; lo ya visto), las descargas (el diálogo de guardar al llegar las
  cabeceras; el cuerpo, al archivo) y la validación de los formularios.
- `imagen.asm`: los formatos de imagen de Mirador **tal cual**
  (`programas/mirador/nucleo.inc`, un `.inc` por formato, y
  `programas/comun/desinfla_t.inc` y `jpeg.inc`), ensamblados con NASM en un
  objeto de 32 bits que se enlaza con lo demás, y un puente para llamarlos
  desde C. Ni una línea copiada: si Mirador mejora un formato, el Navegador
  también. `imagen.c`: del archivo a lo que se ve (reducir, a la paleta).
- Lo que el Navegador necesitaba de Hispa Ventanas: `SetDIBitsToDevice`,
  `GetKeyState`, `GetOpenFileNameA` y `GetSaveFileNameA` (`COMDLG32`), que
  ahora son los de la fase 3d (`ventanas/win32b.inc` y `win32x.inc`,
  docs/486/HISPA-486.md), y `WinExec` (`ventanas/win32n.inc`).

Para trabajar en la maqueta sin arrancar el sistema, `html.c` y `pagina.c`
se pueden compilar en el anfitrión con un `main` que vuelque la rejilla como
texto (no dependen de Hispa Ventanas).

## Medidas

| | |
|---|---|
| `NAVEGA.EXE` | 275 KB (130 KB sin JavaScript; 78 antes de lo de ClicGest entero: 23 KB son los formatos de Mirador; el preludio, `dom.js`, va dentro: 70 KB de texto) |
| `HSPJS.DLL` (QuickJS, con -O2: con -Os serían 513 KB, pero 1,8 veces más lento) | 851 KB |
| `JS.EXE` | 7 KB |
| `HSPTLS.DLL` (TLS, con BearSSL y 20 autoridades raíz) | 87 KB |
| `WSOCK32.DLL` (TCP/IP y Winsock) | 23 KB |
| El disquete «Internet» entero | 1.318 KB de 1.440 (con HSPJS.DLL; sin ella, 486) |
| JavaScript de una página de la biblioteca de ClicGest | 670-720 KB (1,2 MB como mucho); la de una salida (`/eventos/1`, 351 KB de HTML y Bootstrap), 1,9 MB. Se devuelve todo al dejar la página |
| El motor recién hecho, sin página (JS.EXE) | 163 KB |
| Compilar el preludio (`dom.js`, 82 KB), una vez al abrir el Navegador | 56 millones de instrucciones: unos 2,3 s en un 486DX2-66 |
| Compilar `mobile.js` de ClicGest (36 KB), la primera vez | 10 millones: 0,4 s (luego, guardado) |
| Compilar Bootstrap (`bootstrap.bundle.min.js`, 80 KB) | 73 millones: 3 s |
| Los guiones de una página de la biblioteca, ya compilados (desde que llega la página hasta `load`) | hasta 60 millones: 2,5 s como mucho |
| La de una salida (`/eventos/1`: 351 KB de HTML, Bootstrap y sus pestañas) | 380 millones: unos 16 s |
| Filtrar 20.000 nombres como el buscador de «Prestar» | 220 millones: 9 s (la demostración tiene 60 personas: un instante) |
| Un bucle de un millón de vueltas (`x += i % 7`) | 380 millones: 16 s |
| Memoria con una página de la biblioteca abierta (sin JavaScript) | 1,7 MB (1.704 KB; 1.608 antes de las imágenes) |
| La misma, con el catálogo de ClicGest y sus 10 portadas | 1,7 MB (1.704 KB) |
| Memoria con una página de 121 KB de HTML (unas 2.170 filas) | 2,7 MB (2.728 KB); no crece más con el resto de páginas de la prueba |
| Lo que se recuerda de las imágenes ya vistas | 320 KB como mucho |
| Memoria convencional | 16 KB (el búfer de DOS de HW32) + 19 KB (las tramas de red) |
| Apretón de manos de TLS con clicgest.es, la primera vez | 216 millones de instrucciones: unos 15 s en un 486DX2-66 |
| El mismo, con el certificado recordado (una semana) | 22 millones: 1,4 s en un 486DX2-66 |

La memoria se mide con el Administrador de tareas de Hispa Ventanas (lo
«Libre» antes de abrir el Navegador menos lo «Libre» con la página abierta,
en un equipo de 16 MB): `MEDIR=1 GUARDAR=carpeta tests/run_navegador_tests.sh`
deja las capturas `mb` (la biblioteca) y `mg` (`/grande?n=720`, 121 KB). El
montón del Navegador no devuelve memoria al sistema mientras está abierto (la
reutiliza de una página a otra); al cerrarlo vuelve toda. Hispa Ventanas y el
Navegador con la biblioteca abierta ocupan unos 2,9 MB de memoria extendida.

Lo del JavaScript se mide con `-icount shift=0` (una instrucción, un
nanosegundo del reloj del equipo): con JS.EXE y `tests/navjs/clicgest_real.sh`
con `ICOUNT="-icount shift=0"` (las líneas `[memoria]` de NAVEGA.LOG, con la
resolución del reloj de DOS: 55 ms, o sea 55 millones).

Las cuentas del 486 salen de contar instrucciones en QEMU (`-icount`) y
multiplicaciones en el anfitrión, a 2,75 ciclos por instrucción y 40 por
multiplicación de 32×32: ver docs/RED.md. Con la conexión guardada, solo la
primera página de cada servidor paga el apretón de manos; las demás, lo que
tarde el servidor, descifrar la página y maquetarla.

## Límites

- JavaScript: sin `prompt()` de verdad (pregunta si vale la respuesta que
  trae la página), sin hojas de estilo calculadas (`getComputedStyle` sabe
  si algo se ve, y poco más), sin medidas (`getBoundingClientRect`,
  `offsetWidth`: de mentira, 0 si no se ve), sin `<canvas>`, sin
  `Worker`, sin módulos (`import`) en las páginas, sin subir archivos con
  `fetch` y sin `Atomics`. `MutationObserver` no avisa. Una petición de red a
  la vez. Los guiones de otros sitios se cargan igual (no hay CORS).
- Sin marcos (`<iframe>`), sin hojas de estilo de fuera.
  Sin SVG ni WebP (sale su texto alternativo). Las imágenes no se amplían ni
  se ven a más de 360×270 puntos; una imagen en medio de una línea de texto
  más alta que una fila pasa a ser un bloque aparte.
- Una sola letra (fija): no hay cursiva ni tamaños; los títulos van en
  negrita y color. Las columnas de una página (rejillas de CSS, `float`) se
  ven una debajo de otra.
- No hay «recordar la clave» del propio Navegador (lo hace la galleta que da
  el sitio). Las galletas se guardan sin cifrar en `GALLETAS.TXT`.
- Una petición a la vez y una conexión guardada; páginas de hasta 4 MB
  (las descargas, hasta 64 MB, al disco); se suben archivos de hasta 2 MB.
- El reloj del equipo se toma como hora universal para las fechas de las
  galletas (una o dos horas de diferencia, sin importancia).
- TLS: solo 1.2 (no 1.3) y solo intercambio ECDHE; sin revocación; ver
  docs/RED.md.

## Pruebas

- `tests/run_navegador_tests.sh`: el Navegador entero dentro de Hispa
  Ventanas, sin Internet, contra `tests/tools/nav_servidor.py` (una
  biblioteca de mentira parecida a la de ClicGest, por HTTPS con
  certificados hechos en la propia prueba): el menú Inicio, entrar, la tabla,
  prestar, devolver con su pregunta, Atrás y Adelante, la dirección a mano,
  los certificados malos, texto, Latin-1, una página grande, redirecciones y
  la memoria al cerrar; y, para ClicGest: las imágenes (PNG con medidas,
  JPEG reducido por la hoja de estilo y dentro de un enlace, GIF sin
  medidas, un icono en la línea, `data:`, una rota, un SVG; y que al volver
  salen de lo ya visto), Mayús+Tab, subir un archivo (`required` y `pattern`
  lo paran; el diálogo de abrir; el servidor comprueba el tamaño y el SHA-1
  de lo que llega), una descarga (el diálogo de guardar, el archivo en el
  disco igual que el del servidor, y abrirlo con el Lector), el refresco de
  `<meta>`, y las galletas de «recordarme»: se cierra el Navegador, se mira
  `GALLETAS.TXT` (sin las de sesión ni las caducadas) y al abrirlo otra vez
  se entra sin clave. Mira las capturas y lo que recibe el servidor.
  Va con `JavaScript=NO`: el Navegador sin guiones.
- `tests/run_navegador_js_tests.sh`: el JavaScript, contra las páginas
  `/js/` del mismo servidor (`tests/tools/nav_js.py`): un buscador que sugiere
  al escribir (fetch y JSON, flechas, Intro y clic en una sugerencia),
  pestañas y `<details>`, confirm y alert, onsubmit, localStorage (en
  NAVEGA.ALM), setTimeout, XMLHttpRequest, DOMContentLoaded, un error que va
  a NAVEGA.LOG, las galletas desde JavaScript (`document.cookie` sin las
  `HttpOnly`, que tampoco se pueden pisar; `fetch` con las galletas y el
  User-Agent del Navegador), un bucle sin fin que se corta (`TiempoJS=3`), y con
  `JavaScript=NO` el `<noscript>`. Primero en el anfitrión, si hay `gcc -m32`
  con su biblioteca (`tests/navjs/`: html.c, pagina.c, http.c, dom.c y
  script.c tal cual, con QuickJS para Linux, la página vuelta a texto y unas
  órdenes: `clic`, `escribe`, `tecla`...), y luego en Hispa Ventanas.
- `tests/run_js_tests.sh`: HSPJS.DLL con JS.EXE: las pruebas de QuickJS (el
  lenguaje, clausuras, bucles, lo de serie y BigInt), lo que usa ClicGest y
  las cuentas de Math, una excepción, el tope de tiempo.
- `tests/navjs/clicgest_real.sh CARPETA`: contra clicgest.es de verdad (con
  Internet; no va en `make test`): la demostración, el menú, el tutorial,
  «Prestar» (sin prestar nada), las páginas de la biblioteca y las pestañas
  de una salida; deja las capturas y NAVEGA.LOG.
- `tests/run_red32_tests.sh`: la pila TCP/IP y TLS desde la consola.
