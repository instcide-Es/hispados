# Visión del proyecto

**¿Qué sistemas operativos habríamos tenido si la comunidad hispanohablante
hubiera participado en su desarrollo?**

Hispa-DOS es el primer paso de una historia alternativa de la informática:
recorrer las épocas de los sistemas operativos y crear, para cada una, el
nuestro: hecho desde cero, en español y libre. Lo podrá usar gratis cualquier
persona que tenga un equipo antiguo.

> «Vamos a hacer que muchos ordenadores puedan ser estudiados por los nietos
> de las personas que los compraron por primera vez.» — Carlos, 12/11/2026

## Épocas

| Época | Sistema | Base técnica | Equipos objetivo |
|---|---|---|---|
| 1981–1990 | **Hispa-8086** *(en marcha, versión 0.7.1: rama [`hispa-8086`](https://github.com/instcide-Es/hispados/tree/hispa-8086))* | Sistema de 16 bits en modo real para el PC, el XT y el AT: disquetes de 360 K en adelante; en un 286 aprovecha además la memoria alta. Probado con las ROM originales del IBM PC, del XT y del AT | 8086, 8088, V20, 80186, 286 |
| Principios de los 90 | **Hispa-386** *(este repositorio, versión 0.9)*, con su escritorio **Hispa Ventanas** | Compatible con MS-DOS 6.22 y con Windows 3.11; escritorio propio con ventanas, sus programas (Accesorios, Productividad, Juegos, Sonido) y emuladores de consolas | 386, 486 y posteriores |
| Mediados a finales de los 90 | **Hispa-486** *(reservado)* | DirectX para los juegos, navegador web y menú de inicio; todavía con DOS por debajo. El nombre definitivo será el del procesador mínimo con el que vaya bien de verdad | 486 / Pentium |
| Desde los 2000 | **Núcleo nuevo, sin DOS** *(nombre por decidir)* | Un sistema moderno con la tecnología de los 2000, escrito en Castel; después, 64 bits y ARM | Pentium III/4, x86-64, ARM |

**Los nombres** (Carlos, 16/11/2026). **Hispa-DOS** es la familia de los
sistemas operativos con DOS por debajo, y cada miembro se llama por el
procesador **mínimo** que pide: **Hispa-8086** (del 8086 al 286), **Hispa-386**
(386 o superior, con su escritorio **Hispa Ventanas** dentro) e **Hispa-486**
(reservado). Después vendrá un núcleo nuevo que no dependa de DOS. Todos se
actualizan entre sí, y cada uno recomienda el que le corresponde al equipo
donde arranca. Aparte va **Castel**, el lenguaje de programación de sistemas,
en español, con el que se escribirán los próximos. El conjunto se llama
**Rescoldo**; su web es [hispados.es](https://hispados.es).

Todos los sistemas llevan el nombre de la familia **Hispa**. No se usan nombres
de países ni de lugares concretos: todos los países de habla hispana tienen el
mismo valor en este proyecto.

Cada época se termina con una **versión estable** publicada en GitHub, con su
documentación y sus imágenes listas para usar, antes de empezar la siguiente.

## Principios

- **Sala limpia.** Todo el código es nuestro. La compatibilidad se consigue
  observando el comportamiento y las interfaces documentadas, nunca copiando
  código ni datos de otros sistemas. El software de terceros (Windows, Office…)
  lo aporta cada usuario y no forma parte del proyecto.
- **Libre.** GPL-2.0 o posterior.
- **Español primero**, con documentación también en inglés.
- **Para equipos reales.** Se prueba en emuladores y en máquinas antiguas de
  verdad.
- **Pruebas automáticas** en cada pieza y auditorías periódicas antes de cada
  versión estable.

## Cómo prepararemos Hispa-486

Acordado con Carlos (13/11/2026) para cuando empiece la época siguiente (lo que
entonces llamábamos «Hispa 32»; desde el 16/11/2026, Hispa-486):

- **KolibriOS** (GPL v2), como estudio de arquitectura: un sistema de 32 bits
  escrito en ensamblador, con interfaz gráfica, USB, red y sonido, que cabe
  en un disquete. Sirve para ver cómo organizar núcleo, controladores y
  ventanas en poco espacio. Su interfaz (`int 40h`) no es la de Windows, así
  que no nos dice qué funciones dar. Se estudia, no se copia.
- **Arranque UEFI nativo** (además de BIOS): Hispa-486 será el primero de la familia que arranque en PCs que solo tienen UEFI (ver docs/ROADMAP.md).
- **Windows 95**, como objetivo de comportamiento, igual que MS-DOS 6.22 lo
  es de Hispa-DOS: se observa y se prueban programas, nunca su código.
- **Documentación pública** del API Win32 (manuales y libros de la época) y
  la de los proyectos libres que ya la reimplementaron (Wine, ReactOS), para
  saber qué hace cada función.
- **Qué funciones primero**: `tools/importaciones.py` lee la tabla de
  importaciones de programas Win32 (la lista de funciones del sistema que
  cada uno usa, sin código de nadie) y ordena las funciones por cuántos
  programas las necesitan. Los programas los aporta cada usuario y no se
  suben al proyecto; solo se publica la lista resultante de nombres.
- **Puente desde Hispa-DOS**: ejecutar programas Win32 sencillos (de consola
  primero) encima de Hispa-DOS y HSPDPMI, al estilo de HX DOS Extender, para
  ir probando esas funciones antes de tener el sistema nuevo.

## Programas de oficina de cada sistema

Cada sistema tiene sus propios programas de gestión, con el nombre del
sistema detrás (Carlos, 13/11/2026): **Pluma** (procesador de textos) y
**Cuentas** (hoja de cálculo). En Hispa Ventanas son **Pluma-Ventanas** y
**Cuentas-Ventanas**; las épocas siguientes tendrán los suyos, pensados
para lo que daban aquellos equipos, y cada uno abrirá los documentos de
los anteriores.

> Nota (Carlos, 16/11/2026): los programas de oficina **dejan de ser
> accesorios** y pasan a su propio grupo, **«Productividad»** (número 4 en
> `HVP_CABECERA`): **Cuentas**, **Pluma** y, más adelante, **Facturas** (la
> economía de un hogar y de pequeños negocios). Así «Accesorios» se queda con
> las herramientas pequeñas. Desde la 0.9.18 están las tres: **Pluma**,
> reescrita como procesador de textos de verdad (letra con formato, que es lo
> que la distingue del Bloc de notas: la primera versión no pasaba de un Bloc
> con otro menú Edición); **Cuentas**, con un núcleo de cálculo nuevo (la
> primera daba resultados erróneos); y **Facturas**.

## Consolas de cada época

Idea de Carlos (13/11/2026): cada época trae emuladores de las consolas de
su tiempo, con **3 o 4 juegos hechos por nosotros** (diseño propio, GPL,
escritos para el hardware de la consola, así que funcionan también en otros
emuladores y en consolas reales). Quien conserve sus cartuchos originales
los juega desde su biblioteca personal; las ROM nunca van en el proyecto.

| Época | Emuladores |
|---|---|
| Hispa-386, con Hispa Ventanas | **Hispa-Nes**, **Hispa-System**, **Hispa-Boy** |
| Hispa-486 | **Hispa-Drive**, **Hispa-SNes**, **Hispa-BoyColor** |
| Núcleo nuevo (años 2000) | **Hispa-PSX**, **Hispa-BoyAdvance**, **Hispa-Nin64** |

- Código propio, a partir de la documentación pública del hardware (sala
  limpia, como el resto del proyecto).
- Primero, consolas que arrancan el cartucho sin BIOS del fabricante; para
  las que la necesitan (PlayStation), BIOS propia.
- Las marcas de las consolas solo se citan para decir con qué cartuchos es
  compatible cada emulador.
- Empezamos por Hispa-Nes como programa .HVP de Hispa Ventanas, con sus
  juegos (plataformas, naves, puzles), y luego Hispa-Boy.
- **Hispa-Nes ya está** (docs/HISPANES.md): `HISPANES.HVP`, con el primer
  juego propio, Cometas (naves). Faltan los otros juegos y probarlo en
  equipos de verdad.
- **Hispa-System ya está** (docs/HISPASMS.md): `HISPASMS.HVP`, compatible
  con los cartuchos de la Master System (la Game Gear, pendiente), con su
  primer juego propio, Buzo (un buzo con arpón en el fondo del mar), escrito
  en Z80 con un ensamblador propio (`tools/asmz80.py`). Faltan los otros
  juegos y probarlo en equipos de verdad. Sigue Hispa-Boy.

## Estado

- **Hispa-8086**: versión 0.7.1, en marcha (rama [`hispa-8086`](https://github.com/instcide-Es/hispados/tree/hispa-8086)).
- **Hispa-386** con Hispa Ventanas: versión 0.9 de pruebas, casi completa (la
  última, en [Releases](https://github.com/instcide-Es/hispados/releases)).
  Ver el [README](../README.md), la [hoja de ruta](ROADMAP.md) y el estado de
  [Windows 3.11](WINDOWS.md). Se puede probar en el navegador en
  [hispados.es](https://hispados.es).
- **Hispa-486**: reservado.
- **Castel**: en diseño.
