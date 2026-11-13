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
| 1981–1984 | *(nombre por decidir)* | Sistema para el 8086/8088 del PC original y el XT: modo real, disquetes, 64–640 KB | 8086/8088 |
| 1984–1990 | *(nombre por decidir)* | Sistema para el 286 (PC AT): modo real y primeros pasos en modo protegido | 286, 1–4 MB |
| 1994 | **Hispa-DOS** | Modo real, compatible con MS-DOS 6.22 y con Windows 3.11 | 386/486, 4–16 MB |
| Principios de los 90 | **Hispa Ventanas**: entorno gráfico propio sobre Hispa-DOS | Ventanas, multitarea cooperativa, aplicaciones de Windows 3.x | 386/486 |
| Mediados de los 90 | **Hispa 32**: sistema de 32 bits | Multitarea real, nombres largos, Plug and Play | Pentium, 8–32 MB |
| Finales de los 90 | **Hispa Red**: sistema maduro | Red e Internet, USB, multimedia | Pentium II/III |
| Principios de los 2000 | **Hispa XXI** | Sistema de 32 bits moderno | Pentium III/4 |
| Desde mediados de los 2000 | **Hispa x64** | 64 bits, para los PC actuales | x86-64 |
| La era ARM | **Hispa ARM**: el último de la serie | Procesadores ARM | Raspberry Pi, portátiles y equipos ARM |

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

## Cómo prepararemos Hispa 32

Acordado con Carlos (13/11/2026) para cuando empiece la época de 32 bits:

- **KolibriOS** (GPL v2), como estudio de arquitectura: un sistema de 32 bits
  escrito en ensamblador, con interfaz gráfica, USB, red y sonido, que cabe
  en un disquete. Sirve para ver cómo organizar núcleo, controladores y
  ventanas en poco espacio. Su interfaz (`int 40h`) no es la de Windows, así
  que no nos dice qué funciones dar. Se estudia, no se copia.
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

## Consolas de cada época

Idea de Carlos (13/11/2026): cada época trae emuladores de las consolas de
su tiempo, con **3 o 4 juegos hechos por nosotros** (diseño propio, GPL,
escritos para el hardware de la consola, así que funcionan también en otros
emuladores y en consolas reales). Quien conserve sus cartuchos originales
los juega desde su biblioteca personal; las ROM nunca van en el proyecto.

| Época | Emuladores |
|---|---|
| Hispa-DOS / Hispa Ventanas (486) | **Hispa-Nes**, **Hispa-System**, **Hispa-Boy** |
| Hispa 32 (Pentium) | **Hispa-Drive**, **Hispa-SNes**, **Hispa-BoyColor** |
| Hispa XXI | **Hispa-PSX**, **Hispa-BoyAdvance**, **Hispa-Nin64** |

- Código propio, a partir de la documentación pública del hardware (sala
  limpia, como el resto del proyecto).
- Primero, consolas que arrancan el cartucho sin BIOS del fabricante; para
  las que la necesitan (PlayStation), BIOS propia.
- Las marcas de las consolas solo se citan para decir con qué cartuchos es
  compatible cada emulador.
- Empezamos por Hispa-Nes como programa .HVP de Hispa Ventanas, con sus
  juegos (plataformas, naves, puzles), y luego Hispa-Boy.

## Estado

- Hispa-DOS (1994): casi completo. Ver el [README](../README.md), la
  [hoja de ruta](ROADMAP.md) y el estado de [Windows 3.11](WINDOWS.md).
