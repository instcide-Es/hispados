# La familia de oficina de Hispa-386

Hispa-386 lleva de serie lo imprescindible para la oficina, en el grupo
«Productividad» de Hispa Ventanas. Todo es código abierto (GPL-2.0 o posterior)
y está escrito en ensamblador, como el resto del sistema.

| Programa | Para qué | Estado |
|---|---|---|
| **Pluma** | procesador de textos | en uso |
| **Cuentas** | hoja de cálculo (rangos, formatos, funciones, gráficos) | en uso |
| **Facturas** | facturas emitidas y recibidas | en uso |
| **Publicaciones** | panfletos, carteles, tarjetas, folletos | en uso |
| **Presentaciones** | diapositivas, temas, pase a pantalla completa | en uso |
| **Lector** | leer documentos PDF (texto, rayas y recuadros) | en uso |
| **Datos** | base de datos sencilla: tablas, fichas, filtros, listados | en uso |
| **Correo** | correo sencillo | en proyecto (le falta red cifrada en el sistema) |

## Una sola familia: el aspecto

El usuario tiene que sentir que todo es un paquete. Por eso lo que se ve y se
maneja igual no se escribe en cada programa: está en `programas/comun/` y
todos lo incluyen.

- **`barra.inc`**: la barra de botones, el pie y los iconos.
  - Botones de 22 puntos de alto sobre arena; el botón «puesto», en petróleo.
  - Las mismas órdenes llevan el mismo icono en todos los programas (Nuevo,
    Abrir, Guardar, Imprimir, Cortar, Copiar, Pegar, Deshacer, Rehacer...).
    Los iconos son nuestros, de 16x16; los dibuja `tools/iconos.py`.
  - Las órdenes propias de un programa, sin icono evidente, van con texto.
  - Al pasar el ratón por un botón, el pie dice lo que hace.
  - En los programas de hoja (Pluma, Publicaciones, Presentaciones) las filas
    de botones van centradas, como la hoja, sea cual sea el tamaño de la
    ventana (maximizada también): al cambiar de tamaño (`WM_SIZE`) el programa
    llama a `br_centra` con la x del centro y sus filas, y `br_fila` y
    `br_boton_en` cuentan con lo que se han corrido (`[br_x0]`). El programa
    que no la llama deja sus botones a la izquierda, como dice su tabla.
- **`ortografia.inc`** y `DICCIO.DAT`: el corrector de español (ver `ORTOGRAFIA.md`):
  dice si una palabra está bien, propone correcciones y lleva las palabras propias
  del usuario (`PROPIAS.TXT`).
- **`lamina.inc`** y **`lamina_ed.inc`**: la lámina con objetos (cajas de texto,
  formas, imágenes) y su editor; los comparten Publicaciones y Presentaciones.
- **`desinfla.inc`**: el descompresor Deflate/zlib (lo usa el Lector).
- **`tipos.inc`** y `TIPOS*.DAT`: los tipos de letra (veinte, en seis tamaños;
  ver `TIPOS-DE-LETRA.md`). Un documento hecho en un programa se ve con la
  misma letra en otro.

Y unas reglas, que no son código:

- **Colores**: los de la paleta de Hispa Ventanas (`ventanas/defs.inc`). Papel
  blanco (`C_BLANCO`) sobre fondo crema; barras en arena; lo elegido, en
  petróleo con letra clara; avisos y negativos, en granate.
- **Menús**, en este orden y con estos nombres cuando existan: Archivo,
  Edición, Ver, Insertar, Formato, Herramientas, «?». En Archivo: Nuevo,
  Abrir, Guardar, Guardar como, Imprimir, Salir.
- **Atajos**, los mismos en todos: Ctrl+U nuevo, Ctrl+A abrir, Ctrl+G guardar,
  Ctrl+P imprimir, Ctrl+Z deshacer, Ctrl+Y rehacer, Ctrl+X/C/V cortar, copiar
  y pegar, Ctrl+E seleccionar todo, Ctrl+B buscar, Ctrl+N negrita, Ctrl+K
  cursiva, Ctrl+S subrayado. (Ctrl+H y Ctrl+M no valen: para el teclado son
  Retroceso e Intro.)
- **Textos**: en español llano, sin jerga; los errores dicen qué ha pasado y
  qué se puede hacer. «Acerca de» lleva el nombre, una frase y la línea
  `Copyright (C) 2026 Carlos Sanz y contribuidores.` / `GPL-2.0 o posterior.`
- **Diálogos**: Aceptar a la izquierda de Cancelar; Aceptar es el botón por
  defecto (Intro) y Escape cancela.
- **Números y fechas**, a la española: coma decimal, punto de millares,
  día/mes/año.

## El botón secundario

Pedido de Carlos: «Botón secundario en toda la suite Office». En todos los
programas de la familia, el botón derecho del ratón (o Ctrl+clic, o Mayús+F10
y la tecla de menú con el teclado) abre un menú contextual en el sitio del
puntero con las órdenes que tocan para lo que hay debajo, como en Windows y
como en el Administrador de archivos. Las reglas:

- **Primero se coloca**: al apretar el botón sobre algo que no está elegido
  (un sitio del texto, una celda, una factura, un registro, un objeto), se
  elige eso, como con un clic; si ya estaba dentro de lo elegido, lo elegido
  no cambia (para cortarlo, copiarlo o darle formato entero).
- **Las mismas órdenes**: cada línea del menú es una orden de la barra de
  menús, con su mismo número, su mismo texto y su misma tecla al lado. Un menú
  contextual no hace nada que no se pueda hacer por la barra.
- **En gris lo que ahora no se puede** (Pegar sin nada que pegar, Cortar sin
  nada elegido, la página anterior en la primera...).
- **En la barra de botones y en el pie no hay menú.** Con el teclado, el menú
  es el de lo elegido y sale junto a ello cuando se puede.

| Programa | Dónde | Menú |
|---|---|---|
| Pluma | el texto | Deshacer, Repetir, Cortar, Copiar, Pegar, Borrar, Seleccionar todo, Fuentes, Párrafo, Estilo del párrafo, Viñetas, Numeración, Ortografía |
| Cuentas | una celda o un rango | Cortar, Copiar, Pegar, Borrar, insertar y eliminar filas y columnas, Autosuma, ordenar, Formato de celdas |
| | el rótulo de una columna o de una fila (se elige entera) | Cortar, Copiar, Pegar, Borrar, insertar y eliminar, su ancho o su alto, Formato de celdas |
| | la pestaña de una hoja (se pasa a ella) | Hoja nueva, Eliminar hoja, Nombre, moverla a la izquierda o a la derecha |
| Facturas | una factura de la lista | Abrir, Datos, Pagada, Duplicar, Borrar, Guardar como texto, Imprimir, Nueva |
| | la lista, donde no hay facturas | Nueva, emitidas, recibidas, Resumen, Exportar |
| | la factura abierta (la línea queda elegida) | Datos, Pagada, añadir, cambiar y quitar línea, texto, imprimir, volver a la lista |
| Datos | un registro (en la rejilla o en la ficha) | Cambiar, Duplicar, Borrar, Copiar, Pegar en el campo, Añadir, Buscar, Filtrar, Quitar el filtro, Resumen del campo |
| | el encabezado de una columna | ordenar, filtrar, resumen y las órdenes del campo (añadir, cambiar, quitar, mover) |
| | la tabla, donde no hay registros | Añadir, Elegir todos, Buscar, Filtrar, las vistas, Elegir tabla, Añadir un campo |
| | sin tablas | Nueva base, Abrir base |
| Lector | la página | Copiar el texto, Buscar, Buscar siguiente, página anterior y siguiente, Ir a la página, ampliar, reducir, tamaño real, ancho de página, página entera, guardar e imprimir el texto, Información, Abrir |
| Mirador | la imagen | anterior, siguiente, ampliar, reducir, ajustar, tamaño real, pantalla completa, girar, elegir una zona, recortar, deshacer, copiar el nombre, guardar como BMP, información, miniaturas |
| | una miniatura (queda elegida) | Abrir, Volver a la imagen |
| Publicaciones y Presentaciones | un objeto (o varios elegidos) | Cortar, Copiar, Pegar, Borrar, Duplicar, Escribir en la caja, Traer al frente, Enviar al fondo, Agrupar, Desagrupar, Girar, tipo, tamaño y color de la letra, colores del relleno y del borde, grosor |
| | escribiendo en una caja (el cursor va al punto) | Pegar, negrita, cursiva, subrayado, alineación, tipo, tamaño y color de la letra, viñetas, numeración |
| | la página, donde no hay objetos | Deshacer, Pegar, Elegir todo, las herramientas (caja de texto, rectángulo, elipse, línea, imagen) y lo de la página o la diapositiva |
| Presentaciones | una diapositiva de la tira o del clasificador (se pasa a ella) | nueva, duplicar, quitar, subir, bajar, diseño, transición, empezar desde ella |

**Cómo está hecho.** Lo común está en `programas/comun/contexto.inc`:
`ctx_abre` recibe el menú (el formato de un desplegable, con los números de
las órdenes de la barra) y una tabla del programa con las órdenes que pueden
ir en gris y la rutina que lo dice (CF=1: ahora no); con eso llama a
`ctx_menu` de Hispa Ventanas, y la orden elegida llega a la ventana como
`WM_COMMAND`, igual que si se hubiera elegido arriba. `ctx_porta` dice si hay
texto que pegar. Cada programa atiende `WM_RBUTTONDOWN` (se coloca y apunta
en `ctx_donde` qué había debajo) y `WM_CONTEXTMENU` (abre el menú que toca;
con EDX = -1 viene del teclado). En Publicaciones y Presentaciones eso está
en el editor de láminas (`lamina_ed.inc`: `raton_der`, `menu_ctx`); el
programa solo pone el menú de su página (`ctx_fondo`), otro suyo si lo tiene
(`ctx_prog`) y su tabla de gris (`ctx_gris_prog`, con la macro `CTX_GRIS_ED`).
Las pruebas de cada programa tienen su parte del botón secundario (`derecho X
Y` en `tests/tools/hvp_arnes.sh`).

## Cómo está hecho un programa

Cada programa es un módulo `.HVP` (`ventanas/hvp.inc` explica la cabecera y la
API; `ventanas/hvplib.inc`, las ayudas). Se ensambla con

    NASM=nasm python3 tools/mkhvp.py programas/pluma.asm build/PLUMA.HVP

`mkhvp.py` pasa el fuente a la página 437 y busca los `%include` propios en
`programas/<nombre>/`, después en `programas/comun/` y después en `lib/`.
`programas/pluma.asm` es el modelo más completo (menús, barra de dos filas,
diálogos, portapapeles, deshacer, impresión, tipos de letra);
`programas/facturas.asm`, el de un programa de datos con listas.

## Cómo se prueba

Cada programa tiene su prueba automática, `tests/run_<nombre>_tests.sh`, sobre
el arnés `tests/tools/hvp_arnes.sh`: arranca Hispa Ventanas en QEMU con el
módulo abierto, teclea y pulsa como una persona, saca capturas y comprueba lo
que se ve (`tests/tools/ventanas_check.py`) y lo que queda guardado en el
disco. Una función nueva no está hecha hasta que su prueba pasa.

    bash tests/run_pluma_tests.sh        # necesita build/hispados.img
