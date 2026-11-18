# El Navegador de Hispa-486

Un navegador web sencillo para Hispa Ventanas: páginas HTML con formularios,
por HTTP y por HTTPS. Es un programa Hispa32 (`NAVEGA.EXE`) y va, con lo que
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
- **Sin JavaScript.** Lo que se puede suplir sin él, se suple:
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
| Ratón | un clic en un enlace, un botón o un campo; la barra de la derecha desplaza (se puede arrastrar) |
| Tab | al enlace o campo siguiente (el enlace elegido sale en blanco sobre añil); Mayús+Tab, al anterior |
| Intro | abre el enlace, pulsa el botón, envía el formulario del campo; en el de un archivo, abre el diálogo |
| Espacio | marca la casilla o abre la lista; fuera de un campo, baja una pantalla |
| Flechas, Re Pág, Av Pág, Inicio, Fin | mueven la página (en un campo, el cursor) |
| Retroceso | fuera de un campo: Atrás |
| F6 o Ctrl+L | a la dirección (queda elegida entera: lo que se escriba la sustituye) |
| F5 o Ctrl+R | recargar |
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

- **El disquete «Internet»** (`make internet` → `build/internet.img`, 279 KB
  ocupados): `NAVEGA.EXE`, `HTTPS.EXE`, `WSOCK32.DLL`, `HSPTLS.DLL`,
  `MSVCRT.DLL`, `HW32.COM`, `NAVEGA.INI` y `LEEME.TXT`. Se puede usar desde el
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

Lo que se nota al usar ClicGest sin JavaScript ni sus hojas de estilo:

- El menú lateral no se ve hasta desplegar «[+] Menú principal».
- Los pasos del tutorial de la portada de la biblioteca («Ver desde el paso
  1», «Anterior», «Siguiente», «Ocultar pasos») son botones de JavaScript:
  salen en gris y no hacen nada. Los enlaces y formularios de cada paso sí.
- En «Prestar», la lista que va saliendo mientras se escribe es de
  JavaScript: aquí se escribe y se pulsa «Buscar» (o Intro), que es el
  formulario normal que la página ya trae.
- Las letras mayúsculas con acento que no están en la letra de Hispa
  Ventanas salen sin él («Ultimos»), y el euro, como «E»: la letra es la de
  la página de códigos 437, que tiene «É» y «Ñ» pero no «Á», «Í», «Ó», «Ú»,
  «À», «È», «Ò», «€» (harían falta esos glifos en `ventanas/fuente.txt`, en
  sitios de la 437 que no se usen, y cambiar `w32_1252`/`w32_437`).

### Para usar ClicGest entero (18/11/2026, la tarde)

Probado otra vez contra clicgest.es de verdad (la demostración, solo
lecturas: ni préstamos ni cambios), capturas en `docs/486/navegador/`
(desde la 22):

- **Portadas e imágenes:** el catálogo con las portadas al lado de cada
  libro (`22_catalogo_portadas.png`), la ficha con la portada grande y sus
  datos al lado (`23_ficha_portada.png`).
- **Subir la portada:** la ficha del libro, «Portada», tiene su campo de
  archivo con «Examinar...» y el diálogo de abrir (`24_portada_examinar.png`,
  `25_portada_dialogo.png`). No se ha subido nada a la demostración; la
  subida entera (el diálogo, `multipart/form-data`, y que llega el archivo
  igual) la comprueba la prueba automática.
- **Descargas:** el «Manual (PDF)» del panel (3,3 MB, con
  `Content-Disposition`) va al diálogo de guardar y luego, al disco y al
  Lector (`26_manual_guardar.png`, `27_manual_lector.png`).
- **La sesión guardada:** con «Mantener la sesión iniciada» marcada, ClicGest
  da una galleta de 180 días que ahora se guarda (`GALLETAS.TXT`); la prueba
  automática cierra el Navegador, lo abre otra vez y entra sin clave. (La
  demostración entra sin clave y no da esa galleta: se ha comprobado con el
  servidor de pruebas.)

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
- Lo que el Navegador necesitaba de Hispa Ventanas está en
  `ventanas/win32n.inc`: `SetDIBitsToDevice`, `GetKeyState`,
  `GetOpenFileNameA` y `GetSaveFileNameA` (`COMDLG32`) y `WinExec`.

Para trabajar en la maqueta sin arrancar el sistema, `html.c` y `pagina.c`
se pueden compilar en el anfitrión con un `main` que vuelque la rejilla como
texto (no dependen de Hispa Ventanas).

## Medidas

| | |
|---|---|
| `NAVEGA.EXE` | 78 KB |
| `HSPTLS.DLL` (TLS, con BearSSL y 20 autoridades raíz) | 87 KB |
| `WSOCK32.DLL` (TCP/IP y Winsock) | 23 KB |
| El disquete «Internet» entero | 279 KB de 1.440 |
| Memoria con una página de la biblioteca abierta | 1,6 MB (1.608 KB) |
| Memoria con una página de 121 KB de HTML (unas 2.170 filas) | 2,6 MB (2.632 KB); no crece más con el resto de páginas de la prueba |
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

Las cuentas del 486 salen de contar instrucciones en QEMU (`-icount`) y
multiplicaciones en el anfitrión, a 2,75 ciclos por instrucción y 40 por
multiplicación de 32×32: ver docs/RED.md. Con la conexión guardada, solo la
primera página de cada servidor paga el apretón de manos; las demás, lo que
tarde el servidor, descifrar la página y maquetarla.

## Límites

- Sin JavaScript, sin marcos (`<iframe>`), sin hojas de estilo de fuera.
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
- `tests/run_red32_tests.sh`: la pila TCP/IP y TLS desde la consola.
