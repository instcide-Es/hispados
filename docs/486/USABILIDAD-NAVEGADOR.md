# Prueba de uso: la biblioteca de ClicGest con el Navegador de Hispa-486

Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.

**18/11/2026.** Carlos: «Todavía no puedo presentarlo porque no se puede
trabajar en la página web con lo que tenemos. Hay que seguir mejorando para
que una persona pueda usar dicho navegador». Esta es la prueba de uso que
dice **qué** impide trabajar: las nueve tareas de un voluntario de biblioteca
(mayor, no informático), hechas de principio a fin con el ratón y el teclado
dentro de QEMU, con captura de cada pantalla. Solo análisis: no se ha tocado
código del Navegador ni de ClicGest.

Capturas: `docs/486/usabilidad/` (las `chrome_*.png` son las mismas pantallas
en Chrome, para comparar).

## Cómo se ha probado

- `build/hispa486-usb.img` (copia), QEMU con una e1000, 32 MB, 800×600.
  Navegador 0.2 (`HispaNavegador/0.2`), sin JavaScript (la rama de QuickJS no
  está aquí). ClicGest de verdad, cuenta de demostración (tenant 1), modo
  ligero.
- Dos pasadas: una a la velocidad del anfitrión (recorrido y capturas) y otra
  con `-icount shift=5,align=on`, que deja el procesador en **31 millones de
  instrucciones por segundo en tiempo real** (un 486DX4-100, más o menos; un
  486DX2-66 son unos 24: multiplicar por 1,3; un 486DX-33, por 2,6). El disco
  y la red no se frenan: los tiempos son los del procesador.
- La página visible son **97 columnas × 26 filas**. Todo lo que sigue sale de
  esa cuenta: lo que no cabe en 26 filas, no se ve.
- Lo que se ha hecho en la demostración (se restaura a las 0:00): dos
  préstamos y sus devoluciones, alta de «Don Quijote de la Mancha» (nº 13)
  con su portada, la etiqueta del nº 11 marcada como impresa, y el nº 12 dado
  por perdido sin querer (ver G1) y recuperado después.
- Sin probar: nadie iba tarde en la demostración (no se puede fabricar un
  retraso); un equipo de verdad; lector de códigos de barras; la entrada con
  correo y clave (solo se ha visto la pantalla).

## Resumen

**Lo que funciona:** se llega, se busca, se presta, se devuelve, se da de
alta un libro por ISBN (Open Library contesta), se sube la portada, se baja
el recibo y se ve en el Lector, el historial se busca por persona. Los
acentos y la ñ se escriben bien, las listas desplegables y los «[+]» van, el
aviso «Hecho: …» se ve, la tabla del historial cabe entera.

**Por qué aun así no se puede trabajar:** casi todo lo importante cae
**debajo de la pantalla**, lo que se pulsa **no parece un botón**, cada
página tarda **unos 5 segundos**, lo que se teclea **no va a ningún campo**
si antes no se pulsa en él, **no se puede imprimir** un recibo ni una
etiqueta, y hay un fallo serio: **«Dar por perdido» se ejecuta sin
preguntar**.

### Las diez cosas que más estorban

| # | Qué pasa | De quién | Arreglo |
|---|---|---|---|
| 1 | «Dar por perdido» (y borrar, retirar…) se hacen **sin preguntar** | los dos | Navegador: leer `data-confirmar`; ClicGest: no saltarse su pregunta |
| 2 | Tras «Buscar», el resultado queda **debajo de la pantalla**: parece que no ha pasado nada | ClicGest (y Navegador) | resultados pegados al campo; menú en una línea; anclas `#` |
| 3 | **5 s por página** en un 486; el catálogo, **63 s** hasta la última portada | los dos | ClicGest: sin `<script>` ni CSS de más, miniaturas; Navegador: medir dónde se va |
| 4 | **No se imprime**: el Lector solo manda texto a LPT1; y antes, 3 o 4 cuadros | Hispa-486 (y ClicGest) | imprimir la página como imagen; abrir sin preguntar; recibo en texto |
| 5 | Lo que se teclea se pierde: el campo **no tiene el foco** (`autofocus`); con teclado, **30 Tab** | Navegador | `autofocus`; Tab empieza en el contenido |
| 6 | «Prestar» y «Devolver» son **dos enlaces pequeños** entre 70; todos los botones iguales | ClicGest (y Navegador) | botones de verdad, arriba; el peligroso, distinto |
| 7 | El menú y los párrafos de ayuda se comen **6 + 10 de 26 filas** en cada página | ClicGest | menú plegado; ayuda en un «[+]» |
| 8 | **«Atrás» vuelve arriba del todo**, no a donde se estaba: el mismo clic da otro enlace | Navegador | recordar la fila; mismo sitio que al entrar |
| 9 | Los campos **cortan lo escrito por delante** («015» por 2015, «uel de Cervantes») | Navegador | enseñar el principio; campos más anchos |
| 10 | Llegar: menú de arranque de DOS, Inicio › Internet › Navegador, página propia, enlace | Hispa-486 | un icono «Biblioteca» en el escritorio que abra ClicGest |

## Las tareas, una a una

Clics y teclas contados con el ratón (lo que haría un voluntario). «Cargas»
son páginas pedidas al servidor: cada una, unos 5 s en un 486 (ver Tiempos).

| Tarea | Pantallas | Clics | Tecleado | Av Pág | Cargas | ¿Se puede? |
|---|---|---|---|---|---|---|
| 1. Llegar a la biblioteca | 5 | 5 | — | 0 | 2 | sí, con rodeo |
| 2. Buscar un libro y ver su ficha | 3 | 3 | título + Intro | 1 | 3 | sí |
| 3. Prestar (tres pasos) | 6 | 6 | 2 búsquedas | 3 | 6 | sí, a ciegas en el paso 2 |
| 4. Devolver | 2 | 2 | — | 1 | 2 | sí |
| 5. Quién tiene libros / va tarde | 1 | 1-2 | — | 1-2 | 0 | sí |
| 6. Alta de un libro (con portada) | 3 + 2 | 5 + 4 | ISBN; ruta de la foto | 2 | 3 + 1 | sí; la foto, difícil |
| 7. Imprimir recibo o etiqueta | 4-5 cuadros | 4-5 | — | 0 | 1 | **no se imprime** |
| 8. Buscar a un lector y su historial | 2 | 2 | nombre + Intro | 0 | 2 | sí, por «Historial» |
| 9. Salir | 3 | 5 | — | 0 | 1 | sí, con dos tropiezos |

### 1. Llegar a la biblioteca desde el arranque

`00a_menu_arranque` → `00_escritorio` → `01_menu_inicio` → `02_menu_internet`
→ `03_navegador_inicio` → `04_fav_biblioteca_login` → `06_panel` →
`07_biblioteca`.

- Al encender sale el **menú de arranque de Hispa-DOS** (Normal, Juegos,
  Prueba del equipo) 5 segundos. A un voluntario no le dice nada.
- El escritorio está **vacío**: hay que saber que es Inicio › Internet ›
  Navegador (tres clics), y luego elegir un enlace de la página del
  Navegador.
- «ClicGest: la biblioteca» lleva a la entrada (`/login`). El logotipo llega
  después que el texto y **empuja todo dos filas** (`04` y `05`): el clic que
  iba a «Probar la demo» cayó en otra línea.
- En la entrada, «Mantener la sesión iniciada» viene **marcada**, y debajo
  dice «No en equipos compartidos». El de la biblioteca es compartido.
- Ya dentro, la biblioteca es un enlace más del menú.

### 2. Buscar un libro por título y ver su ficha

`09_catalogo`, `11_buscar_tren_escrito`, `12_buscar_tren_resultado`,
`13_ficha_libro`, `13b_ficha_libro_2`.

- Va bien: «[Catálogo]», clic en el campo, «tren», Intro, y el libro sale en
  la fila 20 (a la vista, por poco).
- Hay **dos botones**, «Buscar» y «Filtrar», uno debajo de otro. No se sabe
  cuál es.
- En el catálogo toda la tarjeta es un enlace: **todo sale subrayado**
  (título, autor, estado, categoría).
- La ficha se lee bien (portada a la izquierda, datos al lado). No tiene
  «Prestar este libro»: hay que salir a Biblioteca › Prestar y buscar primero
  a la persona.
- La caja del buscador de la portada de la biblioteca («Buscar una persona o
  un libro») **no busca en el catálogo**: solo filtra lo que está fuera
  (`60`, `61`). Quien busque ahí un libro leerá «Nada… casa con…».

### 3. Prestar un libro (los tres pasos)

`20_prestar_1` … `28_prestado`. En Chrome: `chrome_prestar`.

- **Paso 1.** El campo no tiene el foco: se escribió «mar» sin pulsar antes y
  **no fue a ninguna parte** (la página pide `autofocus`; el Navegador no lo
  atiende). Tras buscar, la primera persona sale en **la última fila** de la
  pantalla (`21`); las demás, debajo.
- **Paso 2.** Se escribe el título, Intro… y **la pantalla queda igual**
  (`24`): el libro encontrado está en la fila 28, fuera. Hay que adivinar que
  toca bajar (`25`). Es lo peor del recorrido.
- **Paso 3.** El botón «Prestar» también está debajo (`26`, `27`).
- El final sí se ve: «Hecho: Prestado … Tiene que devolverlo el 18/12/2026»,
  con «Anular: me he equivocado» e «Imprimir el recibo» (`28`).
- Los pasos salen como «1. 1 ¿A quién? (estás aquí)»: el número, dos veces.
- En Chrome, al entrar ya está el foco en el campo y sale la lista de las 63
  personas: un clic. Aquí, hasta buscar no sale nadie.

### 4. Devolver

`40_devolver` … `43_devuelto`. En Chrome: `chrome_devolver`.

- «Devolver» y, bajando una pantalla, la lista «Fuera ahora» con su botón
  «Devuelto · Nº 11». Un clic y «Hecho: Devuelto… Va a su sitio: Estantería
  B · balda 3», con «Deshacer». Es la tarea que mejor va.
- «Devuelto», «Renovar» y «Dar por perdido» son **tres botones iguales**, uno
  debajo de otro (`41`). Y el último no pregunta (G1).
- Ocho líneas de texto sobre lectores de códigos y Pinakes van **antes** de
  la lista.

### 5. Quién tiene libros y quién va tarde

`33_biblioteca_con_prestamo_2`, `34`, `35_pendientes_abierto_2`, `88_comprobar`.

- En la portada, bajando: «[+] Nadie va tarde», «Esta semana…», «Este mes…»,
  «Este año: 1 persona · 1 libro». Se abren en la misma página y dentro está
  la persona, el libro, la fecha y «Devuelto». Se entiende.
- «[+]» y «[-]» son la única señal de que se pueden abrir.
- Al abrir «Más de la biblioteca», un botón sale partido en dos líneas:
  «[» al final de una y «Reseñas]» en la siguiente (`36`).

### 6. Dar de alta un libro (con portada)

`50_alta_libro` … `55`, `82_portada_dialogo` … `84_portada_subida`.

- Catálogo › «[+] Añadir un libro» › ISBN › «Buscar por ISBN»: Open Library
  contesta y rellena. «Dar de alta» y sale la ficha. Bien.
- Los campos **cortan por delante** lo que no cabe: el autor se ve «uel de
  Cervantes Saavedra», la editorial «emia Española / Alfaguara» y el año
  **«015»** (`51`, `52`). Parece que el dato está mal.
- Los campos miden unas 26 letras en una página de 97.
- La portada: Ficha › Fin › «[+] Portada» › «Examinar...». El cuadro abre en
  **`C:\`**, con `COMMAND.COM`, `AUTOEXEC.BAT` y `CONFIG.SYS`, ocho líneas a
  la vista, sin filtro de imágenes (`82`). Escribiendo la ruta, sube bien
  (6 s) y la portada sale en la ficha y en el catálogo.
- Falta saber cómo llega una foto a ese equipo (no hay cámara ni móvil).
- Un libro sin portada sale descolocado en el catálogo: el punto en una
  línea y el título en la siguiente, sin sangría.

### 7. Imprimir o guardar el recibo y la etiqueta

`14_imprimir_etiqueta` … `17_lector_imprimir`, `29_recibo` … `31`,
`80_recibo_ya_existe`.

- «Imprimir etiqueta» no imprime: sale **«Guardar la descarga»**, con
  `ETIQUETA.PDF` y la lista de `C:\` (`14`). Aceptar → «Guardado… ¿Abrirlo
  ahora con el Lector?» (`15`) → el Lector (`16`).
- La segunda vez hay un cuadro más: «Ya hay un archivo `C:\RECIBO_P.PDF`.
  ¿Sustituirlo?» (`80`). Son **cuatro cuadros** por recibo.
- En el Lector, el recibo se ve muy bien (`31`). Pero el botón de la
  impresora dice «Imprimir el **texto** de esta página» y va al puerto
  paralelo, LPT1 (`17`). Una etiqueta es un código de barras: **no sale**.
  (En la prueba no había impresora: QEMU iba sin puerto paralelo.)
- La etiqueta queda «impresa en ClicGest» aunque solo se haya bajado.

### 8. Buscar a un lector y ver su historial

`62_historial`, `63_historial_antonia`, `65_lectores`, `67_ficha_socio`.

- «[Historial]» › nombre › Intro: tabla de cinco columnas que cabe entera,
  con filas alternas. Es la pantalla que mejor se ve.
- La ficha del socio no dice nada de la biblioteca. Y abre con un botón
  vacío, «[]» (un icono sin texto).

### 9. Salir

`70_salir`, `71_salir_ventanas`.

- «Salir» está al final de la tercera línea del menú. Cierra la sesión sin
  decirlo y deja al voluntario en **la página comercial** de ClicGest
  («Empezar gratis», «Ver demo en vivo»).
- Inicio › «Salir de Hispa Ventanas...»: el botón que viene elegido es
  **«Volver a Hispa-DOS»**. Intro deja al voluntario delante de `C:\>`.

## Lo que estorba, por gravedad

### Grave: impide trabajar o hace daño

**G1. Las acciones que no tienen vuelta se hacen sin preguntar.** (`89`, `90`)
Se pulsó «Dar por perdido» para ver la pregunta y **no hubo pregunta**: el
libro quedó retirado. ClicGest escribe
`onsubmit="return confirm(this.dataset.confirmar)"`; el Navegador solo
entiende `confirm('texto')` (`pag_confirma`, en `pagina.c`: si lo que sigue
no es una comilla, devuelve que no hay pregunta); y ClicGest, que tiene su
pregunta sin JavaScript, **se la salta para el Navegador** porque cree que ya
pregunta él (`Confirmar.php`, `esNavegadorSencillo`). Les pasa a todos los
formularios de `Core\Confirmar`: dar por perdido, borrar o retirar un
ejemplar, quitar una petición, borrar una categoría o una suspensión.
- *Navegador:* si en `confirm(` no hay un texto entre comillas, usar el
  atributo `data-confirmar` del formulario (o el campo oculto `_cg_pregunta`);
  si tampoco, preguntar «¿Seguro?».
- *ClicGest:* no saltarse `Confirmar::interceptar()` para el Navegador hasta
  que su versión lo arregle (por el número del User-Agent). Es el arreglo
  rápido: una línea.

**G2. Lo que se busca queda debajo de la pantalla.** (`21`, `24`/`25`, `26`,
`60`/`61`, `40`) De 26 filas, 6 son el menú, 3 el título y «[Biblioteca]», 4
los pasos, y luego van el campo, el botón y un párrafo de ayuda. El resultado
empieza en la fila 25 o más allá. En el paso 2 de prestar y en el buscador de
la portada, tras Intro **la pantalla no cambia**.
- *ClicGest (modo ligero):* poner los resultados **justo debajo del campo**,
  antes de la ayuda; la ayuda («Con un lector de códigos…», «¿No es socio…?»)
  en un `<details>` o al final; los pasos en una línea («Paso 2 de 3: ¿Qué
  libro?»); y el menú en una (ver M1). Con eso el resultado sale en la fila
  8-10.
- *Navegador:* atender las anclas (`#resultados`: hoy se quitan de la
  dirección y un `href="#x"` solo sube arriba) y, al llegar la página de un
  formulario, empezar por el primer campo con `autofocus`. Así ClicGest puede
  mandar a `…?q=tren#resultados`.

**G3. Cada página tarda unos 5 segundos.** (ver Tiempos) En el anfitrión el
servidor contesta en 0,05 s; a velocidad de 486 la barra de estado se queda
4,5 s en «Recibiendo la página… (3 K)». Prestar un libro son 6 páginas:
medio minuto mirando. Y el catálogo tarda **63 s** en acabar de traer las 11
portadas (unos 5 s cada una).
- *ClicGest:* la página de «Prestar» pesa 33 KB, y de ellos **13,8 KB son
  `<script>` y 14,3 KB `<style>`**: el 85 % es para tirar. En modo ligero, no
  mandar los `<script>` y mandar una hoja mínima (las cuatro reglas que el
  Navegador entiende). Las portadas del catálogo son JPEG de **600×900 (35
  KB)** para un hueco de 56×80: servir miniaturas (`?w=56`, 2-3 KB).
- *Navegador:* medir con `-icount` dónde se van los 140 millones de
  instrucciones por página (descifrar, leer el HTML, maquetar o pintar);
  saltar `<script>` y `<style>` largos sin recorrerlos letra a letra; pintar
  lo que ya ha llegado. Y que se note que está trabajando (M8).

**G4. No se puede imprimir.** (`14`-`17`, `80`) Cuatro cuadros para ver un
recibo, y al final el Lector solo imprime texto por LPT1.
- *Hispa-486 (Lector):* imprimir la página **como imagen** (lo que ya
  dibuja en pantalla) a una impresora de matriz o láser corriente (ESC/P,
  PCL), y decir qué impresora hay.
- *Navegador:* un PDF pulsado desde un botón «Imprimir…» o «Recibo» se abre
  **derecho en el Lector** (guardado en una carpeta temporal, sin preguntar
  el nombre ni si se sustituye). «Guardar» queda para quien lo pida. Y que
  `Descargas=` venga puesto a una carpeta propia (`C:\DOCUMEN`), no a `C:\`.
- *ClicGest:* para el modo ligero, el recibo también como **página** (HTML
  sencillo, 40 columnas), que el Navegador pueda imprimir como texto; y no
  marcar la etiqueta como impresa por haberla bajado.
- Mientras tanto: decirlo. Las etiquetas se imprimen desde otro equipo.

**G5. Lo que se teclea no va a ningún campo.** (`20`, `72_tab_27`) El campo
de buscar pide `autofocus` y no lo recibe: el voluntario escribe el nombre
mirando al teclado y no sale nada. Con el teclado solo, hay que pulsar **Tab
30 veces** para llegar al campo (el menú va antes). Y la página promete «con
un lector de códigos, apunta al QR: se elige solo»: sin foco, tampoco.
- *Navegador:* atender `autofocus` (foco y página movida hasta el campo); si
  se teclea una letra sin foco y la página tiene un solo campo de texto a la
  vista, ponerla en él; Tab empieza en el primer enlace **del contenido**, no
  del menú.
- *ClicGest:* el menú, después del contenido en el HTML, o plegado (M1).

**G6. Lo que hay que pulsar no parece un botón.** (`07`, `08`, `41`;
`chrome_biblioteca`) En Chrome, «Prestar» es un botón verde de media
pantalla y «Devolver» otro al lado. Aquí son **dos palabras subrayadas** en
las filas 21 y 22, debajo de un tutorial de 10 filas que ya está hecho, y
hay 70 enlaces más. Tres formas distintas para lo mismo: enlace subrayado,
`[Enlace con corchetes]` y botón gris. Y los botones grises son todos
iguales, sea «Devuelto» o «Dar por perdido».
- *ClicGest (modo ligero):* «Prestar» y «Devolver» arriba, lo primero de la
  página, como `class="btn"` (el Navegador ya los pinta con fondo); el
  tutorial hecho, plegado en una línea.
- *Navegador:* un `.btn` grande (`btn-lg`, `btn-primary`, `btn-success`) como
  botón de verdad, con marco y relieve, igual que los de formulario; el
  `btn-danger` u `btn-outline-danger`, en granate.

### Medio: se trabaja, pero cuesta o confunde

**M1. El menú ocupa 6 de 26 filas en todas las páginas.** Con «Ver la
versión normal» sale en **una** («[+] Menú principal (30 enlaces)», `74`):
la versión adaptada deja menos sitio que la que no lo está. *ClicGest:* en
modo ligero, una línea: «Biblioteca › Prestar · [+] Menú · Salir».

**M2. «Atrás» vuelve al principio del todo.** (`09x_ajustes_por_error`, `10`)
Al entrar, la página empieza en el menú (fila 4); al volver con «Atrás»,
empieza en el aviso naranja (fila 0), cuatro filas más arriba, y no donde se
estaba. El mismo clic cayó dos veces en «Ajustes» en vez de en «[Catálogo]».
Escribir la dirección a mano (redirigida de http a https) hace lo mismo.
*Navegador:* guardar la fila de cada página del historial y volver a ella; y
tras una redirección, empezar por el contenido igual.

**M3. La página salta cuando llega una imagen sin medidas.** (`04`, `05`)
*ClicGest:* `width` y `height` en el logotipo. *Navegador:* si la imagen
llega y lo de debajo ya se ha visto, no mover lo que hay bajo el ratón (o
mover la página lo mismo).

**M4. Los campos enseñan el final de lo escrito, no el principio.** (`51`,
`52`) *Navegador:* un campo sin el foco se ve desde la primera letra; un
campo de 4 letras con 4 letras escritas las enseña todas. Y el ancho por
defecto de un campo de texto, 40 letras, no 26 (sobran 70 columnas).

**M5. La rueda del ratón no desplaza.** En la imagen, `AUTOEXEC.BAT` carga
`HSPRATON` sin `/DIRECTO`, y sin él no hay rueda (lo dice
`tests/run_lector_tests.sh`). *Hispa-486:* probarlo en un equipo de verdad y,
si pasa igual, cargarlo con la rueda.

**M6. Las teclas pulsadas mientras carga se pierden.** Av Pág justo después
de Intro no hace nada cuando llega la página. Con 5 s por página, pasa
siempre. *Navegador:* guardar Av Pág, flechas y Tab y aplicarlos a la página
nueva.

**M7. No se nota que está cargando.** En 5 s solo cambia una línea pequeña
abajo. El voluntario vuelve a pulsar. *Navegador:* el cursor de espera (el
reloj) y el botón pulsado, hundido, hasta que llegue la página.

**M8. Textos que hablan de lo que aquí no hay.** «Con un lector de códigos,
apunta al QR…» (tres párrafos largos), «Se abre el PDF en otra pestaña»,
«También está en la app», «JPG, PNG o WebP». *ClicGest:* fuera en modo
ligero, o plegados.

**M9. Detalles de ClicGest en modo ligero.**
- «1. 1 ¿A quién?»: el número dos veces (`<ol>` y el círculo del paso).
- «Buscar» y «Filtrar» en el catálogo: dejar uno.
- El texto de ejemplo del campo repite el rótulo y sale cortado («Título,
  autor, ISBN o nº de et»): quitarlo en modo ligero.
- «[]» en la ficha del socio: un enlace de icono sin texto.
- «Hecho: Open Library no tenía nada que añadir»: no es un «Hecho».
- El rótulo del correo va al lado del campo y el de la clave, encima.
- «Salir» lleva a la página comercial: mejor a la entrada, con «Has salido».
- «Mantener la sesión iniciada», sin marcar para el Navegador.

**M10. Detalles del Navegador.**
- Un `[botón]` no se parte entre dos líneas (`36`).
- Un elemento de lista sin imagen no deja el punto solo en su línea (`85`).
- En una tarjeta que es toda un enlace, subrayar solo el título.
- El cuadro de abrir y de guardar: más alto, empezando en la carpeta de
  documentos, sin los archivos del sistema, y solo imágenes cuando el campo
  lo pida (`accept`).

**M11. Llegar y salir del equipo.** *Hispa-486:* un icono «Biblioteca» en el
escritorio (o que el Navegador se abra solo al arrancar, para un equipo que
solo hace eso), con `Inicio=https://clicgest.es/biblioteca`; el menú de
arranque sin espera en ese equipo; y en «Salir de Hispa Ventanas», elegido
«Apagar el equipo».

### Leve

- El título de la entrada dice «Iniciar sesion», sin acento (ClicGest).
- La barra de desplazamiento no tiene flechas: solo la pastilla y el carril
  (Navegador). Un clic en el carril sí baja.
- Los campos de una ficha no se alinean entre sí (cada uno empieza donde
  acaba su rótulo). Con rótulo encima del campo quedaría parejo (ClicGest).

## Tiempos

A 31 millones de instrucciones por segundo (486DX4-100 aproximado). Para un
486DX2-66, por 1,3. Son tiempos desde el clic hasta ver la página.

| Qué | Anfitrión | A velocidad de 486 |
|---|---|---|
| Del arranque al escritorio (5 s son el menú; sin la BIOS) | unos 15 s | 30 s |
| Abrir el Navegador y llegar al panel de la demostración | sin medir | 6,5 s |
| Una página de la biblioteca (33-55 KB) | 0,3-1,5 s | 4,5-5,5 s |
| La misma, recargada dos veces seguidas (conexión abierta) | — | 4,9 s y 5,0 s |
| El catálogo: texto | 1 s | 1-5 s |
| El catálogo: hasta la última de 11 portadas, la primera vez | sin medir (varios segundos) | **63 s** |
| El catálogo otra vez (portadas ya vistas) | — | 10 s |
| Subir una portada de 20 KB y ver la ficha | 2 s | 6 s |
| Prestar un libro entero (6 páginas), solo esperas | unos 6 s | **unos 30 s** |

El apretón de manos de TLS no se ha notado porque el certificado estaba
recordado; la primera vez de cada semana son unos 15 s más en un DX2-66
(NAVEGADOR.md, «Medidas»). El servidor contesta en 0,05-0,07 s: el tiempo es
del equipo.

## Comparación con Chrome

| Pantalla | En Chrome (`chrome_*.png`) | En el Navegador | Qué se pierde |
|---|---|---|---|
| Biblioteca | «Prestar» y «Devolver», dos botones de media pantalla, en la 2.ª franja | dos enlaces en las filas 21-22 (`07`) | saber por dónde empezar |
| Prestar, paso 1 | foco en el campo y la lista de las 63 personas ya abierta | campo sin foco, nadie en la lista hasta buscar, resultado en la última fila (`20`, `21`) | un clic se vuelve clic + teclear + Intro + bajar + clic |
| Catálogo | tarjetas con el nº grande en un recuadro, chapas de color («Prestado», «Letra grande») | lista subrayada, estado en verde (`85`) | el nº de etiqueta deja de destacar; poco más: se lee bien |
| Devolver | lo mismo, con los tres botones de distinto color | tres botones grises iguales (`41`) | distinguir «Devuelto» de «Dar por perdido» |

## Orden propuesto

1. **G1** hoy mismo en ClicGest (una línea) y luego en el Navegador.
2. **G2 + M1 + G6** juntos, en ClicGest: una plantilla de modo ligero con el
   menú en una línea, los botones arriba y los resultados pegados al campo.
   Es lo que más cambia lo que ve Carlos.
3. **G5 + M2 + M4 + M6 + M7** en el Navegador: foco, «Atrás», campos, teclas
   y señal de carga. Son arreglos pequeños y cada uno quita un tropiezo.
4. **G3**: quitar `<script>` y CSS en ClicGest y miniaturas; medir el
   Navegador.
5. **G4** (imprimir) es el más largo: decidir antes con qué impresora.
6. **M11**: el icono en el escritorio, para la presentación.
