# Visión del proyecto

**¿Qué sistemas operativos habríamos tenido si la comunidad hispanohablante
hubiera participado en su desarrollo?**

Hispa-DOS es el primer paso de una historia alternativa de la informática:
recorrer las épocas de los sistemas operativos y crear, para cada una, el
nuestro: hecho desde cero, en español y libre. Lo podrá usar gratis cualquier
persona que tenga un equipo antiguo.

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

## Estado

- Hispa-DOS (1994): casi completo. Ver el [README](../README.md), la
  [hoja de ruta](ROADMAP.md) y el estado de [Windows 3.11](WINDOWS.md).
