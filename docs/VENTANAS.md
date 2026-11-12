# Hispa Ventanas — plan

Segunda época de la serie Hispa (ver [VISION.md](VISION.md)): un **entorno
gráfico propio** que funciona sobre Hispa-DOS, como si lo hubiéramos hecho a
principios de los 90, y que **ejecuta las aplicaciones de Windows 3.x**
(Word 6, Excel 5 y similares que aporte cada usuario).

Reglas de siempre: sala limpia (nada de código, gráficos, fuentes ni textos de
Microsoft), GPL, español primero, pruebas automáticas y equipos reales
(386/486 con 4–16 MB, y también PCs modernos con BIOS o CSM).

## Decisiones de base

| Tema | Decisión | Por qué |
|---|---|---|
| Lenguaje | Ensamblador (NASM), como Hispa-DOS | Ocupa mucho menos y va más rápido en los 386/486 a los que está destinado (decisión de Carlos, 12/11/2026) |
| Modo del procesador | Modo protegido de 16 bits (como el «modo estándar»), con un servidor DPMI propio sobre HSPXMS | Es lo que esperan las aplicaciones de Windows 3.x y funciona desde el 286/386 |
| Aspecto | Diseño propio: colores, iconos, fuentes de mapa de bits y nombres en español hechos aquí | Que se reconozca como nuestro, no como una copia |
| Compatibilidad | API de Windows 3.1 documentada (KERNEL, USER, GDI, KEYBOARD, MOUSE, DISPLAY, COMMDLG, SHELL) implementada desde cero | Así las aplicaciones de la época funcionan sin cambios |
| Programas propios | Hechos como aplicaciones de Windows 16 bits normales, en NASM, con una herramienta propia que escribe la cabecera NE | Cada programa propio prueba también la compatibilidad |

## Etapas

Cada etapa termina con algo que se ve y se puede probar en QEMU y en un PC real.

1. **Servidor DPMI propio (HSPDPMI).** Modo protegido de 16/32 bits sobre
   HSPXMS: descriptores, memoria, interrupciones reflejadas a DOS.
   *Se ve:* programas DPMI de terceros (por ejemplo de FreeDOS o DJGPP)
   funcionando sin CWSDPMI ni el DOSX de Windows.
   *Estado:* hecho (`drivers/hspdpmi.asm`, `HSPDPMI.COM` en `\HISPADOS`; se
   carga a mano o desde AUTOEXEC.BAT, después de HSPXMS.SYS). El cliente corre
   en anillo 3 sin paginación y las interrupciones y llamadas a DOS/BIOS se
   reflejan volviendo a modo real de verdad. Servicios de INT 31h: 0000h-000Dh,
   0100h-0102h, 0200h-0205h, 0300h-0306h, 0400h, 0500h-0503h, 0600h-0604h,
   0702h/0703h, 0800h/0801h y 0900h-0902h. Pruebas: `tests/run_dpmi_tests.sh`
   (clientes propios de 16 y 32 bits y, si se descargan con
   `tests/thirdparty/fetch_dpmi.sh`, NASM, SED y LUA de DJGPP). Pendiente: la
   traducción de punteros de la API de DOS (INT 21h con DS:DX desde modo
   protegido; hoy los clientes usan 0300h con memoria de DOS, como DJGPP),
   VCPI (para convivir con un EMM386) y memoria virtual.
2. **Gráficos y ratón.** Driver de pantalla VGA 640×480 16 colores y VESA
   (800×600 y 1024×768, 256 colores y color verdadero); primitivas de dibujo,
   fuentes propias de mapa de bits, cursor del ratón (HSPRATON).
   *Se ve:* un escritorio con fondo, cursor y ventanas que se pueden arrastrar.
3. **Ventanas y mensajes (USER).** Clases de ventana, cola de mensajes,
   multitarea cooperativa, menús, botones, cuadros de texto, listas, cuadros de
   diálogo, portapapeles, temporizadores.
   *Se ve:* nuestros primeros programas: Centro de programas (lanzador),
   Bloc de notas, Calculadora, Reloj y un juego propio.
4. **Cargador de ejecutables de Windows (NE) y KERNEL.** Módulos, DLL,
   recursos (iconos, menús, diálogos, cadenas), memoria global y local,
   segmentos móviles y descartables, archivos .INI (perfiles).
   *Se ve:* programas de Windows 3.1 sencillos (los nuestros de prueba)
   funcionando en Hispa Ventanas.
5. **Compatibilidad ampliada.** GDI completo (mapas de bits, paletas,
   regiones, metarchivos, impresoras), cuadros de diálogo comunes (Abrir,
   Guardar, Imprimir, Fuente, Color), SHELL, DDE, OLE 1 básico, ayuda.
   *Se ve:* Word 6 y Excel 5 (copias del usuario) abriendo, editando y
   guardando documentos.
6. **Sesiones DOS en ventana y modo 386.** Máquinas virtuales 8086 (V86) para
   ejecutar programas de DOS en ventana o a pantalla completa, junto con el
   EMM386 propio de Hispa-DOS (UMB, EMS) y la Sound Blaster emulada en tarjetas
   modernas.
   *Se ve:* varias sesiones de DOS a la vez, cada una en su ventana.

Al final de la etapa 5 sale la primera versión estable de Hispa Ventanas.

## Pruebas

- Capturas de pantalla automáticas en QEMU (tests/tools/screen.sh) comparadas
  con lo esperado, y órdenes de prueba por el puerto serie.
- Programas de prueba de Windows 16 bits propios (NASM + nuestra
  herramienta de cabeceras NE) para cada función de la API, con su
  resultado escrito en un archivo.
- Las aplicaciones de terceros (Windows 3.11, Word, Excel) solo en local, con
  las copias de cada uno; nunca en el repositorio.
