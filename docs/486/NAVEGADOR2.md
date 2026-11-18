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
| CSS | lo mínimo (de las `<style>`; clases de Bootstrap conocidas) | CSS 2.1 + flexbox + calc() de NetSurf, hojas de fuera, `@media`; variables de CSS (nuestro) |
| JavaScript | sí (QuickJS, HSPJS.DLL) | todavía no (ver «Lo que falta») |
| ClicGest | modo ligero (User-Agent `HispaNavegador`) | la web normal (User-Agent `HispaSurf`) |
| Memoria | 1-3 MB | 8-15 MB (ver «Medidas») |
| Tamaño | 300 KB | 1,07 MB + 208 KB de letras |

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

En el disquete «Internet 2» (`make internet2`: `build/internet2.img`), que
se copia **entero** en `C:\HISPADOS\INTERNET`, junto a lo del disquete
«Internet» (que trae MSVCRT.DLL, WSOCK32.DLL y HSPTLS.DLL):

| | |
|---|---|
| `NAVEGA2.EXE` | el programa (1.070 KB) |
| `NAVEGA2.INI` | sus opciones |
| `RECURSOS\` | las hojas de estilo de NetSurf (`default.css`, `quirks.css`, `adblock.css`, `internal.css`), sus textos en español (`MESSAGES`) y la página de inicio |
| `FUENTES\` | las letras: DejaVu Sans, Sans negrita, Serif, Serif negrita, Mono y Mono negrita, recortadas a las letras de Europa occidental (208 KB; `terceros/dejavu`) |
| `LEEME.TXT` | cómo se instala |

Le quedan 115 KB libres al disquete. Hispa Ventanas pone solo, al arrancar,
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
NAVEGA2.EXE a 800×600) y de NAVEGA2.EXE en QEMU, en `navegador2/`:

| Página | NAVEGA2.EXE | Chrome |
|---|---|---|
| Una página HTML+CSS del disco | `01_pagina_de_disco.png` | |
| https://hispados.es/ | `02_hispados.png` (sin las variables: `02_hispados_sin_variables.png`) | `02_hispados_chrome.png` |
| https://clicgest.es/login | `03_clicgest_login.png` | `03_clicgest_login_chrome.png` |
| demo: panel | `04_demo_panel.png` | `04_demo_panel_chrome.png` |
| demo: catálogo de la biblioteca | `05_catalogo.png` | `05_catalogo_chrome.png` |
| demo: socios | `06_socios.png` | `06_socios_chrome.png` |

(Solo lecturas en la demo: entrar por `/demo` y mirar.)

Lo que falla o falta, de más a menos visible:

1. **JavaScript**: ClicGest sin JavaScript enseña su versión para «sin
   JavaScript» (los menús desplegables abiertos, `.cg-abrir-sin-js`; la barra
   lateral como enlaces). Es lo que más cambia el aspecto.
2. **Iconos de letra** (Bootstrap Icons: `@font-face` con WOFF2): NetSurf no
   carga letras de la web; los iconos no salen (el ojo de «Ver», la lupa,
   las flechas).
3. **CSS grid** (`display: grid`): NetSurf 3.11 no lo tiene; lo de rejilla
   sale uno debajo de otro (las tarjetas grandes de la biblioteca).
4. **`gap`** en flexbox y **esquinas redondas** (`border-radius`), **sombras**
   (`box-shadow`): no los pinta (las cajas son rectas y sin sombra).
5. Los **campos de formulario** salen más altos que en Chrome (NetSurf mide
   su alto con la letra y el relleno de Bootstrap a su manera).
6. Las letras de la web (Inter, Space Grotesk...) son DejaVu: más anchas.
7. Los colores, en los 256 de Hispa Ventanas (el más cercano, sin tramado):
   los grises claros y los pasteles se parecen, pero no son.
8. SVG: libsvgtiny necesita el XML de libdom (expat), que no va aún: el
   logotipo de hispados.es (SVG) no sale.

Arreglado aquí: las variables de CSS, los degradados de fondo, las
mayúsculas de las letras con tilde y la ñ en `text-transform` (salía
«ESPAñA»), las `<style>` partidas entre dos trozos de la red, el
User-Agent que ClicGest tomaba por el del Navegador ligero.

## Medidas

En QEMU (sin KVM, en el VPS), 32 MB, 800×600, por la red de QEMU a Internet:

| Página | Tiempo (de pedirla a «Hecho») | Memoria en uso (NetSurf) |
|---|---|---|
| (ver tabla de la última medida en el informe de la sesión) | | |

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

## Lo que falta (el plan)

1. **JavaScript (fase 5).** NetSurf trae sus enlaces con JavaScript hechos
   para Duktape (`content/handlers/javascript/duktape`, generados con
   nsgenbind de los WebIDL: 220 archivos). Con QuickJS (HSPJS.DLL) habría
   que rehacer esos enlaces (el DOM de libdom hacia QuickJS): es otro
   proyecto. Lo rápido sería compilar Duktape tal cual (+700 KB y memoria);
   lo nuestro, generar los enlaces de QuickJS con un nsgenbind propio.
   Ahora: sin JavaScript (`content/handlers/javascript/none`).
2. **SVG**: el XML de libdom con expat (o un lector XML pequeño propio).
3. **Iconos de letra y `@font-face`**: cargar TTF/WOFF (WOFF2 pide Brotli).
4. **Cursiva** (inclinando las letras al pintarlas) y el «hinting».
5. **Descargas** (al Lector, como el Navegador propio: `gui_download_table`)
   e **imprimir** (la página en puntos al Lector).
6. **Grid** y `gap`: en el `layout` de NetSurf (los de NetSurf lo tienen
   pedido; el flexbox de 3.11 es reciente).
7. **Memoria** para 16 MB justos: soltar las páginas del historial, una
   caché de letras más pequeña, Bootstrap sin las reglas añadidas que no se
   usan.
8. **Tramado** de colores al pasar a la paleta (los fondos pastel).

## Pruebas

`tests/run_navegador2_tests.sh` (sin Internet: `tests/tools/surf_servidor.py`
por HTTPS con la raíz de pruebas de `tests/tools/nav_certs.sh`): el acceso
del menú Inicio, una página moderna (hoja de fuera con variables de CSS,
botones con variables por clase, degradado, flexbox, PNG y JPEG; el
User-Agent `HispaSurf`), una redirección, una página del disco, Atrás, un
formulario GET y una página larga (Av Pág, Fin), con las capturas
(`GUARDAR=carpeta`) y los tiempos de `SURF.LOG`.
