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
