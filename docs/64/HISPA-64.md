# Hispa-64 — el plan

*23/11/2026. Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.*

Hispa-64 es el miembro de 64 bits de la familia Hispa-DOS: 8086 → 386 → 486 →
Pentium (gama XP, 32 bits) → **Hispa-64** (x86-64 de hoy) → Hispa-ARM. Su meta,
dicha por Carlos: **un sistema usable para una oficina de hoy en día**, en los
portátiles de ahora (UEFI, NVMe, xHCI, varios núcleos, Wi-Fi moderna), aunque
cueste un año.

Este documento es el plan: lo que se quiere, las decisiones de arquitectura con su
porqué, y las etapas, cada una con lo que se verá funcionar y cómo se prueba. Las
**etapas 0 a 9 están hechas** (secciones 6 a 15), y la **10** y la **11**, en su base (secciones 16 a 21: la 10 d, la última, con los sockets de Unix y la memoria compartida por descriptor, lo primero que pide el navegador).

---

## 1. Objetivos

1. **Hardware de hoy, con firmware de 64 bits y 4 GB o más.** UEFI de 64 bits,
   NVMe y AHCI, xHCI (USB 3), varios núcleos (también los híbridos P+E de Intel
   12.ª gen. en adelante), pantalla del GOP (aunque esté por encima de 4 GB),
   touchpads I2C-HID, Ethernet y, más tarde, Wi-Fi moderna. Sin 8254 ni PIC
   (HPET, APIC local y temporizador de ACPI).
2. **Un núcleo propio, en Castel.** Como Hispa-Pentium: el lenguaje de la casa,
   en español, compilado por los compiladores de la casa (ccastel.py, CASTEL64,
   CASTEL32). Lo que el lenguaje no sabe decir va en bloques `ensamblador`.
3. **Una capa POSIX** para traer el software de hoy sin reescribirlo: un
   navegador moderno (Ladybird o WebKit), la oficina (docx, xlsx, pdf), la
   impresión (IPP), TLS, las herramientas de siempre.
4. **Todo en español**, con los mensajes que se entienden (las excepciones del
   procesador ya se explican así desde la etapa 0).
5. **Probado en el taller de Carlos** además de en QEMU (sección 4).

Lo que **no** es objetivo: los equipos con firmware UEFI de 32 bits (Atom Bay
Trail: el Acer One 10 y la Medion S2217) ni los procesadores de 32 bits (el HP
Mini, Atom N455): se quedan con Hispa-Pentium. (Arrancar Hispa-64 desde un UEFI de
32 bits es posible —GRUB i386-efi y multiboot2, como hace Linux; el núcleo ya pasa
solo al modo largo y mira CPUID— pero tiene **prioridad baja**: quizá más adelante.)

---

## 2. Decisiones de arquitectura

### 2.1 El núcleo, en Castel 64

Castel ya genera x86-64 (castel/docs/CASTEL64.md): `nat64`, `ent64`, punteros de 8
octetos, funciones de interrupción que vuelven con `iretq`, ensamblador en línea con
r8–r15, y CASTEL64 (el compilador escrito en Castel) se compila a sí mismo hasta el
punto fijo. Para Hispa-64 hay ya un modelo propio, **`modelo hispa64`** (sección 6).

**Lo que le falta al compilador** y se hará cuando toque (cada cosa con sus
pruebas en pruebas64.sh, sin cambiar lo que sale para 16 y 32 bits):

| Falta | Para qué | Etapa |
|---|---|---|
| El **«modelo de núcleo» de direcciones** (el `-mcmodel=kernel` de gcc): el núcleo en los últimos 2 GB (`0xFFFFFFFF80000000`). Las direcciones de 32 bits con signo (`[G_x]` ya vale; `mov esi, T_3` pasa a `lea rsi, [T_3]`, en ccastel.py y Gen64; el `org` de 64 bits en ensambla.py y Montador) | el núcleo en la mitad alta | **hecho** (etapa 1; CASTEL64.md §10 bis) |
| **Llamar a C y que C llame** (System V AMD64: argumentos en RDI, RSI, RDX, RCX, R8, R9, pila alineada a 16): `externa función` con su puente | enlazar con código en C (ACPICA, musl, la oficina) | 4–7 (la etapa 2 lee ACPI sin ACPICA: sección 8) |
| **Números escritos de 64 bits** y constantes de 64 al compilar | las direcciones altas sin `nat64(x) << 32` | **hecho** (etapa 2; CASTEL64.md §10 ter: son `nat64`, sin calcular al compilar) |
| Variables **alineadas** (`alineado 4096`) | estructuras de los dispositivos | **hecho** (etapa 2, las del módulo sin valor inicial; CASTEL64.md §10 ter) |
| ~~`modelo usuario64`~~ `modelo linux64`: programas del anillo 3 con `syscall` (los de Linux corren tal cual: sección 10) | los programas de Castel en Hispa-64 | **hecho** (etapa 4) |

### 2.2 El arranque: multiboot2 de GRUB (recomendado), cargador UEFI propio más tarde

Dos caminos:

- **GRUB EFI + multiboot2** (lo que hace ya Hispa-Pentium por UEFI). GRUB
  (`BOOTX64.EFI`, hecho con `grub-mkimage` con lo que usa el menú dentro) lee el
  núcleo de la partición EFI, sale de los servicios de arranque del firmware y
  entra en el núcleo en modo protegido de 32 bits, dándole en las etiquetas de
  multiboot2 la pantalla del GOP, el mapa de memoria, la RSDP de ACPI, la línea
  de órdenes y los módulos (los archivos que se le pidan: el disco de arranque en
  memoria, más adelante).
- **Un cargador UEFI propio** (una aplicación PE32+ como HispaBIOS): más control
  (elegir el modo de pantalla, leer el EDID, guardar las tablas de UEFI que se
  quieran, quedarse con los servicios de ejecución de UEFI para la hora y las
  variables), sin GRUB por medio.

**Se recomienda multiboot2 con GRUB, ya**, por tres razones: (1) está probado en el
hardware de Carlos con Hispa-Pentium (los mismos portátiles, la misma memoria USB);
(2) el mismo núcleo arranca por UEFI de 64 bits, por la BIOS y, si algún día se
quiere, por UEFI de 32 bits; (3) es trabajo ya hecho: el esfuerzo va al núcleo.
Lo que GRUB no da (los servicios de ejecución de UEFI) se suple con ACPI y el CMOS.
El **cargador propio queda como etapa opcional** (la 13): conviene cuando haga falta
Secure Boot (con shim firmado) o los servicios de ejecución.

Detalle que importa: el núcleo se carga en 1 MB (como Hispa-Pentium). Si un equipo
tuviera ocupado ese sitio, GRUB lo dirá. Desde la etapa 1 corre en la mitad alta, y
lo único que depende de la dirección física es el arranque de 32 bits y las tablas
del núcleo (pagina64.cas): la etiqueta «relocatable» de multiboot2 (que GRUB lo cargue
donde haya sitio) pide que el arranque calcule dónde está (`call`/`pop`) y lo sume.
Se hará si algún equipo lo pide.

### 2.3 La memoria: 4 niveles de páginas y la mitad alta

| Dónde (virtual) | Qué | Desde |
|---|---|---|
| `0x0000000000000000` – `0x00007FFFFFFFFFFF` | **la mitad baja: los programas** (cada proceso la suya) | etapa 4 |
| `0xFFFF800000000000` – … | **el mapa directo**: la memoria física, tal cual, desplazada (física + `0xFFFF800000000000`); desde la etapa 1, solo la RAM, lo de ACPI y lo del cargador, sin ejecutar | etapa 0 (los 64 primeros GB, enteros); etapa 1 |
| `0xFFFFC00000000000` – … | los dispositivos, mapeados a propósito (con PAT: sin caché, o «escritura combinada» para la pantalla), cada uno en su GB | etapa 1 |
| `0xFFFFC80000000000` – … | los bloques grandes del montón (páginas propias, con una sin poner detrás) | etapa 1 |
| `0xFFFFD00000000000` – … | las pruebas de la memoria (prmem64.cas) | etapa 1 |
| `0xFFFFFFFF80000000` – `0xFFFFFFFFFFFFFFFF` | **el núcleo** (código de solo leer, datos y pila sin ejecutar, una página de guarda bajo la pila) | etapa 1 |

- **4 niveles** (PML4, PDPT, PD, PT): 48 bits de direcciones virtuales, 256 TB. La
  paginación de 5 niveles (LA57) no hace falta en portátiles.
- Páginas de 4 KB para los programas; de 2 MB y 1 GB para el mapa directo y el
  núcleo (menos tablas, menos fallos de TLB). Las de 1 GB las tiene todo procesador
  de 2010 en adelante (los del taller, todos); si no, de 2 MB.
- **NX** en los datos y la pila; **SMEP/SMAP** (que el núcleo no ejecute ni toque
  sin querer la memoria de los programas) cuando haya programas.
- La memoria física: un **mapa de bits** de marcos de 4 KB (hecho en la etapa 1: 1
  bit por marco, 32 KB por GB; con `pide_seguidos(n, alineación)` para lo que va
  seguido), y un **«buddy»** (bloques de 2^n páginas) si pedir trozos grandes
  seguidos llega a ser lento (las colas de NVMe y xHCI). El montón del núcleo, con
  casillas de tamaño fijo (32 … 2048 octetos) y, lo grande, en páginas propias.
- El núcleo deja de verse en la mitad baja cuando haya programas.

### 2.4 SMP desde el principio

Todo lo del núcleo se escribe **pensando en varios procesadores desde la etapa 1**
(aunque se arranquen en la 3): datos por procesador (con `GS` apuntando a su bloque
y `swapgs` al entrar del anillo 3), cerrojos de espera activa con orden (y sin
interrupciones mientras se tienen), contadores atómicos (`lock xadd`), y nada de
«variables globales de la tarea actual». Arrancar los demás procesadores: con la
tabla MADT de ACPI, INIT-SIPI-SIPI y un trampolín en modo real por debajo de 1 MB
(una página que se reserva del mapa de memoria). La topología, con CPUID 0xB/0x1F y,
en los híbridos, el tipo de núcleo (CPUID 0x1A: P o E).

### 2.5 El planificador

- **Hilos del núcleo y procesos** (un proceso = un espacio de direcciones + sus
  hilos). Una **cola por procesador** con prioridades (tiempo real, interactivo,
  normal, de fondo) y turnos; se roba trabajo de otra cola cuando la propia está
  vacía.
- **Expropiativo** con el temporizador del APIC local (en modo TSC-deadline si lo
  hay, calibrado con el HPET o el temporizador de ACPI: no hay 8254 en los equipos
  de ahora). Sin tic cuando el procesador está parado (`hlt` con el siguiente
  temporizador puesto: ahorra batería).
- **Híbridos**: los hilos interactivos prefieren los núcleos P; los de fondo, los E
  (Intel Thread Director más adelante, si merece).
- **Esperar sin gastar**: colas de espera y un `futex` (lo pedirán los hilos de
  POSIX).

### 2.6 Las llamadas al sistema: `syscall`/`sysret`

`syscall` es la forma rápida de x86-64 (los MSR STAR, LSTAR, FMASK; `swapgs` para
la pila del núcleo). **Los números y la forma, los de Linux x86-64** para la parte
POSIX (número en RAX; argumentos en RDI, RSI, RDX, R10, R8, R9; el error en
negativo): así la libc se porta casi sin tocarla, las herramientas de compilar
cruzado de `x86_64-linux-musl` valen tal cual y, de paso, muchos programas
estáticos de Linux correrían sin recompilar. Las llamadas propias de Hispa (la
pantalla, los sucesos de las ventanas) van en números altos, aparte. Cuidado
conocido: `sysret` con una dirección no canónica (el fallo de Intel): se comprueba
antes de volver.

### 2.7 Los ejecutables: ELF64

ELF64 (el de Linux y de casi todos): lo saca cualquier compilador de hoy, Castel lo
escribe ya a mano (`modelo linux64`) y hace falta para traer software. Al principio,
**estáticos**; luego, **dinámicos** con el cargador de musl (`ld-musl`) y bibliotecas
compartidas (las necesitará el navegador para no ocupar gigas). Los programas de
Castel (`modelo usuario64`) salen también como ELF64.

### 2.8 La capa POSIX: portar musl

**Portar musl (licencia MIT, compatible con GPL-2.0 o posterior)**, no escribir una
libc propia: musl es pequeña (~1 MB de fuentes), clara, correcta, estática y
dinámica, con hilos POSIX, y como Hispa-64 usa las llamadas de Linux (2.6), se porta
cambiando muy poco. Escribir una libc propia con la calidad que pide un navegador
son años.

El subconjunto de llamadas, por orden:

1. Archivos y carpetas: `open(at)`, `read`, `write`, `pread`, `lseek`, `close`,
   `fstat/stat`, `getdents64`, `mkdir`, `unlink`, `rename`, `dup`, `pipe`, `ioctl`
   (lo mínimo de `termios`).
2. Memoria: `mmap`, `munmap`, `mprotect`, `brk`.
3. Procesos: `fork` (copiar al escribir) o `vfork`/`posix_spawn`, `execve`, `wait4`,
   `exit_group`, `getpid`, `kill` y las señales (`rt_sigaction`, `rt_sigprocmask`,
   `rt_sigreturn`).
4. Hilos: `clone` con `CLONE_THREAD`, `futex`, `set_tid_address`, `arch_prctl`
   (FS para el TLS), `sched_yield`.
5. Tiempo: `clock_gettime` (con vDSO más tarde), `nanosleep`.
6. Esperar varias cosas: `poll`, `epoll` (lo usan los navegadores y casi todo).
7. Red: `socket`, `connect`, `bind`, `listen`, `accept`, `sendmsg`/`recvmsg`, con la
   pila TCP/IP del núcleo (etapa 9).

### 2.9 Los drivers, por prioridad

Por lo que hay en el taller y lo que pide una oficina:

1. **Pantalla del GOP** (hecha en la etapa 0) con escritura combinada (PAT).
2. **ACPI** (tablas: MADT, HPET, MCFG, FADT; y AML para apagar, la batería, las
   tapas, los botones): **portar ACPICA** (doble licencia BSD/GPLv2: compatible). Un
   intérprete de AML propio que funcione en portátiles de verdad son meses.
   **Desde la etapa 2**: las tablas las lee el núcleo él mismo (acpi64.cas, unas
   500 líneas) y ACPICA se porta cuando haga falta el AML de verdad (la batería,
   la tapa, dormir, el botón de los portátiles que lo llevan por el AML): con el
   puente con C y musl (etapas 4–7), en la 11. Ver la sección 8.
3. **APIC local, IOAPIC, HPET, temporizador de ACPI, TSC** (sin 8254 ni PIC).
4. **PCIe** con ECAM (MCFG), MSI y MSI-X; BAR de 64 bits (en 64 bits no hay que
   moverlos: se mapean donde estén, aunque sea por encima de 4 GB).
5. **NVMe** (el ThinkPad, el Monovate) y **AHCI** (el ASUS, los MacBook).
6. **xHCI** + HID (teclado, ratón) + almacenamiento masivo USB; **i8042** (teclado
   PS/2 de los portátiles); **I2C-HID** (los touchpads: el controlador I2C
   DesignWare de Intel LPSS, con lo aprendido en Hispa-Pentium).
7. **Red por cable**: Realtek RTL8111/8168 (PCIe), Intel I219 (e1000e) y el
   adaptador USB RTL8153.
8. **Sonido HDA** (Hispa-Pentium tiene uno) — los equipos con DSP (SOF) quedan para
   después.
9. **Wi-Fi moderna** (Intel AX201/AX211, Realtek 8821CE/8822CE…): es lo más caro
   (firmware cargable, 802.11ax, WPA2/WPA3); en el segundo semestre.
10. SDHCI/eMMC, lector de tarjetas, Bluetooth: después.

### 2.10 Los gráficos

**El marco del GOP**, sin cambiar de modo (la resolución que pone el firmware, que
en los portátiles es la del panel), con doble búfer en la memoria y copiando a la
pantalla solo lo que cambia, con escritura combinada. Un **compositor** sencillo
(ventanas en búferes propios, con transparencia) y el dibujo por programa con SIMD
(SSE2/AVX2, en C). Drivers de la gráfica (i915, amdgpu) para cambiar de modo y
acelerar: **no** en el primer año (son el trabajo más grande de un sistema); el
GOP basta para una oficina. Las ventanas de Hispa-Pentium (Castel, 32 bits) se
recompilan para 64 donde se pueda, y la oficina usa su `lienzo`/`ui` en C.

### 2.11 Reutilizar el C portable de Hispa-Pentium

`castel/nucleo32/oficina/comun` (Pluma, Cuentas, PDF, TTF, ZIP, XML, ortografía…)
está escrito para esto: solo cambia `so_pentium.c` por un **`so_hispa64.c`** que
pida las cosas por la capa POSIX (con `so_anfitrion.c`, el de Linux, como modelo:
casi vale tal cual). Igual con lo demás en C de Hispa-Pentium (TLS con BearSSL, el
cliente SSH, WireGuard): se compilan con el compilador cruzado `x86_64-linux-musl`
contra la musl de Hispa-64. De lo que está en Castel (los drivers xHCI, AHCI, HPET,
ACPI, PCI, la pila TCP/IP), se aprovecha **la lógica** y se adapta a 64 bits:
direcciones de 64, BAR por encima de 4 GB sin moverlos, MSI-X, varios procesadores.

### 2.12 Las aplicaciones de hoy

- **Navegador**: Ladybird (BSD-2, motor propio, en C++23) o WebKit (WPE, LGPL).
  Los dos piden lo mismo: C++ completo (libc++ o libstdc++), hilos, `mmap`,
  `epoll`, sockets, TLS, fuentes (FreeType, HarfBuzz), imágenes, ICU… Es la meta
  más dura del plan; mientras, el NetSurf con JS de Hispa-Pentium (surf-js)
  recompilado.
- **Oficina**: la de Hispa-Pentium (docx/xlsx/pdf) con `so_hispa64.c`.
- **Impresoras**: **IPP Everywhere** por la red (sin drivers: casi todas las
  impresoras de los últimos diez años), mandando PDF o PWG-raster; IPP-USB para las
  de cable.

---

## 3. Las etapas

Duraciones **honestas**, en semanas de trabajo de un agente con Carlos probando en
el taller, y contando con que en el hardware real siempre sale algo. Total hasta
«usable para una oficina» (etapa 12): **entre 12 y 15 meses**; el navegador moderno
y la Wi-Fi son lo que más puede alargarlo.

| Etapa | Qué | Duración | Acumulado |
|---|---|---|---|
| 0 | Arranque UEFI, modo largo, consola, mapa de memoria, excepciones | **hecha (23/11)** | — |
| 1 | Núcleo en la mitad alta, memoria física y virtual, montón | **hecha (23/11)** | — |
| 2 | ACPI (tablas propias), APIC, HPET, tiempo, PCIe, apagar | **hecha (23/11)** | — |
| 3 | SMP, planificador, hilos del núcleo | **hecha (23/11)** | — |
| 4 | Anillo 3, syscall/sysret, ELF64, procesos | **hecha (23/11)** | — |
| 5 | NVMe, AHCI, GPT, VFS, FAT32/exFAT | **hecha (24/11)** | — |
| 6 | xHCI, HID, i8042, I2C-HID, USB de almacenamiento; la consola y el intérprete | **hecha (24/11)** | — |
| 7 | musl, POSIX, herramientas de compilar cruzado | **hecha (24/11)** | — |
| 8 | Gráficos: compositor, ventanas, ratón, oficina | **hecha (24/11)** | — |
| 9 | Red por cable, TCP/IP, TLS; Wi-Fi en paralelo | **hecha (25/11)** (la Wi-Fi, en paralelo: 3–5 meses) | — |
| 10 | Navegador moderno (Ladybird o WebKit) | **la base, hecha (25/11)**: C++, la letra (FreeType y HarfBuzz), TCP de hoy; y lo que quedaba de la 9 (sección 16). **La base de la 10 b, hecha (25/11)**: curl 8.14.1 con BearSSL (la orden y libcurl, con «multi») e ICU 77.1 (con sus datos filtrados), readlink (sección 19). **La 10 c, hecha (26/11)**: una referencia al descriptor durante cada llamada, la causa de fondo de lo lento con varios procesadores (la TLB: la cuarentena del montón) y de los cortes del sonido, el compilador cruzado (GCC 14.3 con la libstdc++ para musl; C++ dinámico), **IPv6** (SLAAC, ND, ICMPv6, AF_INET6) y lo de la auditoría (sección 20). **La 10 d, hecha (26/11)**: los sockets de Unix (AF_UNIX, con SCM_RIGHTS y SCM_CREDENTIALS), `memfd_create` con sus sellos, un programa de C++23 que se habla con otro como los de Ladybird, el vigilante, el HPET de 32 bits y H4 y H11 de la auditoría (sección 21). Queda el navegador mismo: lo que pide, en las secciones 20 y 21 | (8–12 sem.) |
| 11 | Impresión IPP, sonido HDA, energía (suspender, batería) | **la base, hecha (25/11)**: el planificador, TCP con congestión, la INTx, el tiempo de procesador y **el sonido (HD Audio, /dev/dsp)** (sección 17). **La 11 b, hecha (25/11)**: el tic estable (la calibración del APIC), **ACPICA en el núcleo** (la batería, el cable, el EC, apagar por `_S5`, el `_CRS` de los enlaces de los `_PRT`), el brillo y HWP (sección 18). Quedan suspender, los P-states sin HWP, IPP y la mezcla del sonido | (en paralelo) |
| 12 | Instalar en el disco, actualizar, cuentas | 3–4 sem. | (en paralelo) |
| 13 | (opcional) Cargador UEFI propio, Secure Boot; UEFI de 32 bits | 3–4 sem. | — |
| 14 | (futura) Driver de NVIDIA, de Turing en adelante, con `open-gpu-kernel-modules` y el firmware GSP; la RTX 3060 del PC de Albinyana de banco de pruebas (sección 20) | sin empezar | — |

Cada etapa acaba con: sus pruebas automáticas en QEMU (`castel/nucleo64/pruebas.sh`,
que llama `pruebas64.sh`), una imagen USB en
`/root/FTP/HispaDOS/Pruebas/Pendientes/<EQUIPO>/Hispa-64-etapa-N/` con su LEEME, y
lo que diga Carlos del taller.

### Etapa 0 — arrancar y decir qué hay (hecha)

Ver la sección 6.

### Etapa 1 — la memoria (hecha)

Ver la sección 7. Lo que cambió del plan al hacerla: los números escritos de 64 bits
y las variables alineadas no han hecho falta (quedan para la etapa 2); el montón son
casillas de tamaño fijo y, lo grande, páginas propias (no un «buddy»); las pruebas de
QEMU, con 128 MB, 3 GB y 8 GB (y `-cpu qemu64`, sin páginas de 1 GB, y `-cpu max`); y
se ha añadido lo que hace falta para que un fallo «se explique con el nombre de la
función»: los nombres van en un módulo del cargador (`HISPA64.SIM`).

### Etapa 2 — ACPI, interrupciones y tiempo (hecha: sección 8)

- ~~ACPICA portada~~ las tablas leídas por el núcleo (sección 8: ACPICA, en la 11);
  MADT, HPET, MCFG, FADT.
- APIC local (x2APIC si lo hay), IOAPIC, MSI; el temporizador del APIC calibrado
  con el HPET o el de ACPI (no hay 8254); TSC invariante; la hora del CMOS.
- PCIe por ECAM: la lista de dispositivos con sus BAR (también los de más de 4 GB).
- Apagar y reiniciar por ACPI (`_S5`), el botón de encendido.
- **Se verá**: la lista de PCI (con nombres), «8 procesadores (4 P + 4 E) según la
  MADT», la hora, un reloj que avanza; pulsar el botón de encendido apaga.
- **QEMU**: q35 con OVMF (HPET, IOAPIC), `-machine q35,hpet=off` (sin HPET: el de
  ACPI), `-smp 8`. **Taller**: los tres de antes y los MacBook Air (2012 y 2017:
  el firmware de Apple tiene sus cosas con ACPI).

### Etapa 3 — varios procesadores y el planificador (hecha: sección 9)

Lo que cambió del plan al hacerla: las pruebas de QEMU, con `-smp 1, 2, 4 y 8`
(no 16: el servidor tiene 4 núcleos y con el TCG de varios hilos 16 procesadores
serían 4 veces más lentos sin probar nada nuevo); «`hlt` sin tic» queda para la
etapa 11 (el tic sigue a 100 por segundo también en reposo: es el que despierta a
los dormidos y deja robar trabajo a los ociosos); y se ha añadido lo que pedía el
encargo: sucesos, semáforos y mutex, `mwait` en la tarea ociosa donde es seguro, y
que un pánico en cualquier procesador pare a todos y lo explique uno.

- Arrancar los demás procesadores; datos por procesador; cerrojos; IPI (vaciar el
  TLB de los demás).
- Hilos del núcleo, colas por procesador, prioridades, expropiación; `hlt` sin tic.
- **Se verá**: «8 procesadores en marcha», una prueba con cien hilos que cuentan a
  la vez y dan el número exacto, la carga de cada procesador en la consola.
- **QEMU**: `-smp 1, 2, 4, 16` y `-cpu max`; KVM no hay en el VPS (todo con TCG, que
  ya prueba carreras). **Taller**: el ThinkPad (híbrido) y el ASUS (4 núcleos, 8
  hilos).

### Etapa 4 — programas: anillo 3, `syscall`, ELF64 (hecha: sección 10)

Lo que cambió del plan al hacerla: **no hace falta un `modelo usuario64`**: los
programas de `modelo linux64` (ELF estáticos de Linux x86-64) corren tal cual,
porque Hispa-64 tiene las llamadas de Linux; **copiar al escribir** llega con
`fork` (etapa 7, con musl); las páginas de los programas se ponen **al tocarlas**
(menos lo que viene del archivo); el **intérprete de órdenes** en el anillo 3 pide
leer el teclado desde un programa (etapa 6): en la 4 el núcleo arranca él los
programas de la unidad en memoria y los espera. Y se ha añadido: SSE para los
programas (sus registros, guardados al cambiar), SMEP, SMAP y UMIP, PCID, y un CD
que arranca por BIOS y por UEFI.

- Espacio de direcciones por proceso, `syscall`/`sysret`, cargar ELF64 estáticos,
  las primeras llamadas (escribir, leer, salir, memoria).
- **Se verá**: programas que se arrancan y acaban; un programa que falla se explica
  y **no** tumba el sistema.
- **QEMU** y **taller**: lo mismo, con un disco en memoria (módulos de multiboot2).

### Etapa 5 — discos y archivos (hecha: sección 11)

Lo que cambió del plan al hacerla: la caché es **de bloques** (la de páginas, para
`mmap` de archivos, con musl: etapa 7); **exFAT, solo para leer** (escribirlo, en
la etapa 6, con las memorias USB); FAT12 y FAT16 no (FAT32 sí); NVMe con **una
cola por procesador** y MSI-X, AHCI con MSI, y las dos sin ellas mirando la cola;
el VFS con un solo mutex; los discos, **de solo leer salvo con `escribir`**; los
discos de 2 TB del plan son de **3 TB** (una partición más allá de 2^32 sectores),
por NVMe y por SATA; y se ha añadido arrancar un programa desde un disco.

- NVMe (colas con MSI-X), AHCI (de Hispa-Pentium), GPT, un VFS con caché de páginas,
  FAT32 y exFAT (las memorias USB y la partición EFI); leer NTFS y ext4, más tarde.
- **Se verá**: `dir` de la memoria USB y del disco interno, copiar a 1 GB/s en NVMe.
- **QEMU**: `-device nvme`, `-device ahci`, discos de 4 GB y 2 TB (GPT). **Taller**:
  ThinkPad y Monovate (NVMe), ASUS y MacBook (AHCI). **Nunca escribir en el disco
  interno de un equipo de Carlos sin su permiso**: las primeras versiones, solo leer.

### Etapa 6 — USB y la entrada (hecha: sección 12)

Lo que cambió del plan al hacerla: se ha añadido **la entrada para los programas**
(la consola como terminal, `fork`, `execve`, `wait4`, `poll` y `select`) y **el
intérprete de órdenes en el anillo 3** (ORDENES), que pedía la etapa 4; la
**escritura de exFAT y FAT12/FAT16** (de la etapa 5); el touchpad I2C-HID **sin
ACPICA** (los PNP0C50 buscados en las tablas sin interpretar el AML; ACPICA, en la
11); las memorias USB que se enchufan en marcha se ven al arrancar otra vez (los
discos en marcha, etapa 7).

- xHCI (de Hispa-Pentium, con el BAR donde esté), HID, almacenamiento masivo, i8042
  (teclado de los portátiles, con la distribución española), I2C-HID (touchpads).
- **Se verá**: el teclado del portátil y uno USB, el touchpad, un ratón USB, una
  memoria USB.
- **QEMU**: `-device qemu-xhci`, `usb-kbd`, `usb-tablet`, `usb-storage`.
  **Taller**: todos (cada uno tiene su touchpad y su teclado).

### Etapa 7 — POSIX y musl (hecha: sección 13)

Lo que cambió del plan al hacerla: **musl no se ha tenido que tocar** (las
llamadas son las de Linux: se compila tal cual, la 1.2.6 con sus parches de
seguridad, y el «compilador cruzado» es el guion `musl-gcc` de musl, en el
árbol de construcción); la herramienta de verdad es **Lua 5.5** (MIT), sin
cambiar nada (BusyBox es GPL-2.0 *solo*, que no casa con el «o posterior» de
Hispa; toybox, SQLite y `make`, más adelante); las pruebas de libc-test, en la
8; se ha añadido **AVX** (xsave) y los **ELF de posición libre** estáticos;
`mmap` de archivos **copia** lo del archivo (la caché de páginas compartida, más
adelante); los discos que se enchufan en marcha pasan a la 8.

- musl portada, las llamadas de 2.8 (1 a 6), hilos de POSIX, `epoll`; el compilador
  cruzado `x86_64-linux-musl` (gcc o clang) para hacer programas de Hispa-64 desde el
  servidor.
- **Se verá**: BusyBox (GPL-2.0) o las herramientas de toybox, Lua, SQLite, un
  `make` corriendo en Hispa-64; las pruebas de libc-test de musl.
- **QEMU**: las pruebas de libc-test. **Taller**: la misma imagen.

### Etapa 8 — gráficos, ventanas y la oficina (hecha: sección 14)

Lo que cambió del plan al hacerla: el compositor es **un programa del anillo 3**
(VENTANAS, en C con musl), no parte del núcleo: el núcleo da la pantalla
(`/dev/fb0`, con doble búfer), el teclado y el puntero (`/dev/input/event0`)
y la memoria compartida con nombre (`/dev/shm`), como Linux; el protocolo de
las ventanas es propio, por memoria compartida y futex. La oficina de
Hispa-Pentium (Pluma 2 y Cuentas 2) se compila **tal cual** con musl y
`so_hispa64.c`. Quedan para más adelante (sección 14): el lector de PDF, la transparencia de
las ventanas, el SIMD del dibujo y el touchpad en el escritorio del taller.

- Compositor, ventanas, cursor, la letra con suavizado (TTF de la oficina), el
  escritorio de Hispa; la oficina de Hispa-Pentium con `so_hispa64.c`.
- **Se verá**: Pluma abriendo un docx, Cuentas un xlsx, el lector de PDF, a la
  resolución del panel (1920×1080 en el ThinkPad).
- **QEMU**: capturas (`screendump`) comparadas en las pruebas. **Taller**: todos.

### Etapa 9 — la red (hecha: sección 15)

Lo que cambió del plan al hacerla: la pila TCP/IP es propia, en el núcleo y en
Castel 64 (la lógica de Hispa-Pentium; sin lwIP); los nombres los resuelve musl;
la red por USB (RTL8153) queda para la etapa 10, con IPv6 y la escala de
ventana de TCP; se añadieron virtio-net y la Realtek probada en QEMU con la
RTL8139C+. Ver la sección 15.


- Realtek 8111/8168, Intel I219, USB RTL8153; TCP/IP (lo de Hispa-Pentium o lwIP,
  BSD), DHCP, DNS, `socket` POSIX; TLS con BearSSL (como en Hispa-Pentium).
- **Wi-Fi en paralelo** (3–5 meses): Intel AX201/AX211 (firmware de Intel, que se
  carga como archivo aparte), WPA2-PSK primero; lo aprendido con el AR9285 y el
  Broadcom de Hispa-Pentium sirve para el 802.11 y el WPA.
- **Se verá**: descargar por HTTPS, NTP, SSH.
- **QEMU**: `-nic user,model=e1000e`, `rtl8139`, `usb-net`. **Taller**: cable en el
  ASUS (Realtek), el ThinkPad, el adaptador USB.

### Etapa 10 — el navegador moderno (la base, hecha: sección 16)

Lo hecho el 25/11 (sección 16): primero lo que dejó la 9 (la red por USB
CDC-ECM, setitimer y alarm, la hora de NTP al CMOS y /etc/localtime, las
INTx por el `_PRT`) y, de la 10, lo de abajo de todo: **C++** (la libstdc++
del sistema con musl, con un puente: `cxx.sh`), **FreeType y HarfBuzz**
(compilados con musl, la misma letra que en Linux), **TCP de hoy** (escala
de ventana, lo desordenado guardado, SACK) y los límites de un proceso (512
hilos, 256 descriptores). Queda (la «10 b», sección 16): ICU, curl, IPv6, la
libstdc++ compilada para musl y el navegador mismo. De la 10 b, la base
(sección 19): **curl** con BearSSL (la orden CURL y libcurl) e **ICU** (con
los datos de aquí, filtrados), y readlink para que ICU sepa la zona.

- C++ completo (libc++ y libunwind, o libstdc++), FreeType, HarfBuzz, ICU, curl,
  y Ladybird (o WPE WebKit) con su ventana sobre el compositor.
- **Se verá**: el correo web, ClicGest, la web del banco.
- Es la etapa con más riesgo: puede pedir más memoria, `mmap` con más formas,
  señales finas y hilos a cientos. De ahí los 8–12 semanas.

### Etapa 11 — imprimir, sonar, ahorrar (la base y el sonido, hechos: sección 17)

Lo hecho el 25/11 (sección 17): primero lo de fondo que dejó la 10 (el
fallo del planificador, los dos intermitentes de las pruebas, TCP con
NewReno y los SACK de la otra punta, la INTx del `_PRT` para los discos y
el USB, `select` de más de 64, ITIMER_VIRTUAL y PROF), lo que encontró KVM
(STAR sin el RPL 3) y, de la 11, **el sonido**: HD Audio en el núcleo y
/dev/dsp como OSS. La «11 b» (sección 18): el tic estable, ACPICA en el
núcleo (la batería, el cable, el EC, apagar por `_S5`, los enlaces de los
`_PRT`), el brillo y HWP. Quedan IPP (mDNS), suspender y los P-states sin HWP.

- IPP Everywhere (descubrir con mDNS, mandar PDF), HDA, suspender (S3 o S0ix con
  ACPICA), la batería, el brillo, HWP/P-states.

### Etapa 12 — instalar y mantener

- Instalar en el disco (al lado de Windows, con GRUB), actualizar desde
  actualiza.hispados.es, usuarios con Argon2id (como en Hispa-Pentium).

### Etapa 13 (opcional) — cargador UEFI propio

- Una aplicación PE32+ propia (como HispaBIOS) en vez de GRUB: el modo de pantalla
  elegido por el EDID, las variables de UEFI, Secure Boot con shim. Y, si se
  quisiera, arrancar desde UEFI de 32 bits.

---

## 4. El taller de Carlos

| Equipo | Para Hispa-64 | Qué prueba sobre todo |
|---|---|---|
| **ASUS GL553VD** (Intel 7.ª gen., Kaby Lake, 4 núcleos/8 hilos, gráfica Intel + NVIDIA) | **sí** | AHCI, i8042, Ethernet Realtek, el GOP con dos gráficas |
| **ThinkPad E14 Gen 5** (Intel 13.ª gen., híbrido P+E) | **sí: el de referencia** | NVMe, híbridos, I2C-HID, sin 8254, HWP, Wi-Fi Intel |
| **Monovate 14.1** (i3-1115G4, Tiger Lake, 11.ª gen.) | **sí** | el GOP y los BAR por encima de 4 GB (y aun de 64), NVMe/eMMC, sin 8254 |
| **MacBook Air 2012** (Ivy Bridge, Mac Nº3) | sí (se arranca con la tecla Opción) | el firmware de Apple, AHCI, el teclado y el trackpad por USB interno |
| **MacBook Air 2017** (Broadwell) | sí | lo mismo, AHCI por PCIe |
| **Medion S2217 / Acer One 10** (Atom Z3735F) | **no**: UEFI de 32 bits y 2 GB | se quedan con Hispa-Pentium (2.2, etapa 13) |
| **HP Mini 5103** (Atom N455) | **no**: el procesador es de 32 bits | Hispa-Pentium |
| **VirtualBox** (Carlos) | sí, con «Habilitar EFI» | lo primero que se prueba, antes del hardware |

Las imágenes, según la norma de Carlos: `Pruebas/Pendientes/<EQUIPO>/Hispa-64-etapa-N/`
con la imagen y un LEEME.TXT.

---

## 5. Cómo se prueba

- **QEMU con OVMF** (UEFI de 64 bits, `/usr/share/OVMF/OVMF_CODE_4M.fd`, sin
  instalar nada) y con `-kernel` (multiboot, para lo rápido); la salida por el
  puerto serie (en la página 437) se compara con lo esperado, y el código con el
  que acaba por el `isa-debug-exit` (el núcleo con «apagar» en la línea de
  órdenes). La pantalla se comprueba con un pantallazo (`screendump` por el monitor
  de QEMU). Ojo con lo que QEMU no tiene igual que el hardware: no hay GOP por
  encima de 4 GB, sí hay 8254 y COM1. Por eso cada etapa va también al taller.
- `castel/nucleo64/pruebas.sh` (solo, unos 8 minutos desde la etapa 5) y `SOLO=castel64 ./pruebas.sh`
  (CASTEL64 entero, que lo llama). **Los martes y jueves de 16:30 a 21:00, nada
  pesado en el servidor** (el bingo).

---

## 6. La etapa 0, hecha (23/11/2026)

### Qué hace

`castel/nucleo64/hispa64.cas` («modelo hispa64») y sus módulos:

| Archivo | Qué |
|---|---|
| `hispa64.cas` | el núcleo: lee lo del cargador (multiboot2 o multiboot), abre la consola, lo cuenta todo, provoca excepciones si se le pide |
| `pant64.cas` | la consola en la pantalla del GOP con la letra de Hispa (`nucleo32/letra.cas`, 8×16, página 437; 2×2 en pantallas de 1400 puntos de alto o más), 16 colores, texto guardado (sube sin leer de la pantalla) |
| `excep64.cas` | la IDT con las 32 excepciones, explicadas en español (qué es, dónde, el número de error, CR2 y el porqué en un fallo de página, los registros); la int3 se atiende y se sigue |
| `cpu64.cas` | CPUID: fabricante, nombre, familia/modelo, procesadores lógicos, páginas de 1 GB, NX, x2APIC, HWP, híbrido, bits de las direcciones físicas |
| `cifras64.cas` | escribir en hexadecimal y en KB/MB/GB |
| `imagen.sh` | el núcleo y la imagen de arranque (GPT + FAT32 con `\EFI\BOOT\BOOTX64.EFI` de GRUB y su menú) |
| `pruebas.sh`, `pantallazo.py` | las pruebas (sección 5) |

El arranque es del compilador (`ARRANQUE64['hispa64']` en ccastel.py, que
`compilador/haz_apoyo.py` copia a `compilador/32/apoyo64.cas` para el compilador en
Castel): cabeceras de multiboot (para `qemu -kernel`) y multiboot2 (pide a GRUB la
pantalla gráfica), el puerto serie, **comprueba el modo largo con CPUID** (si no lo
hay, lo dice por el puerto serie y se para; el menú de GRUB lo mira antes con
`cpuid -l`), sus **propias tablas de páginas dentro del núcleo** (PML4, una PDPT y
64 PD de páginas de 2 MB: los 64 primeros GB tal cual y otra vez en la mitad alta,
desde `0xFFFF800000000000`), el modo largo, una GDT propia y `principal()`. Lo que se
escribe va al puerto serie y a la función que el núcleo pone en `C_consola` (la
consola). Una pantalla más arriba de 64 GB (los Intel de 11.ª gen. la ponen a veces
en 256 GB) se añade con páginas de 1 GB (`alcanza`, hasta 128 TB, mirando los bits
físicos del procesador).

Lo que dice (QEMU con OVMF, 3 GB y `-cpu max`, abreviado):

```
Hispa-64 0.0.1 ─ Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
Etapa 0: el núcleo mínimo, en Castel de 64 bits.

Cargador: GRUB 2.12-1ubuntu7.3, multiboot2, con UEFI de 64 bits
Procesador: QEMU TCG CPU version 2.5+
  ...; 4 procesadores lógicos en el paquete (según CPUID)
  páginas de 1 GB: sí; NX: sí; x2APIC: no; HWP: no; híbrido (núcleos P y E): no
  direcciones físicas de 40 bits
Modo largo (64 bits): sí; paginación de 4 niveles, los 64 primeros GB tal cual
  y otra vez en la mitad alta (desde 0xFFFF800000000000): comprobada
Mapa de memoria (lo que dice el firmware):
  0x0000000000000000 - 0x000000000009FFFF  640 KB  disponible
  ...
Memoria disponible: 2,9 GB en 22 trozos; la más alta acaba en 0x140000000
Pantalla: 1280 x 800 puntos, 32 bits por punto, 5120 octetos por línea, en 0x80000000
  consola de 160 x 50 letras
ACPI: la RSDP (2.0 o más) la ha copiado el cargador; la XSDT/RSDT en 0x...
Excepciones: las 32 en la IDT, explicadas en español. Una int3 de prueba:
  Punto de parada (int3) número 1 en RIP=0x...: atendido, sigue.
Hispa-64 etapa 0: todo en orden.
```

(La raya del título es la «─» de la página 437, la que hay más parecida a la «—».)

Con `prueba=pagina` en la línea de órdenes:

```
*** Hispa-64 se ha parado: excepción 14 ***
Fallo de página (#PF): una dirección que no está en las tablas de páginas, o sin permiso
Al leer de 0x0000500000000000: la página no está (desde el núcleo).
Número de error: 0x0
RIP=0x...  CS=0x8  RFLAGS=0x16  RSP=0x...
RAX=... (los 16 registros)
El procesador queda parado. Apunte lo de arriba (o hágale una foto) y reinicie el equipo.
```

Ocupa 20 KB (más unos 490 KB que empiezan a cero: las tablas, la pila y el texto de la
consola).

### Cómo se ha probado

`castel/nucleo64/pruebas.sh` (13 pruebas con CASTEL64 y ENSAMB64, unos 35 s):

- se compila igual con ccastel.py + NASM, con ensambla.py y con CASTEL64 + ENSAMB64
  (y con CASTEL32 en corre32, en `pruebas64.sh`);
- por multiboot (QEMU `-kernel`): el saludo, el modo largo, la mitad alta
  comprobada, el mapa (255,4 MB de 256), la int3, «todo en orden» con el código 42;
- las excepciones `prueba=pagina`, `gp`, `division`, `ud`: cada una explicada;
- `prueba=alto`: con páginas de 1 GB pone 1 GB en 512 GB (una PDPT nueva) y lo lee;
  sin ellas dice que no se puede;
- un procesador sin modo largo (`-cpu qemu64,-lm`): lo dice y se para;
- por **UEFI de 64 bits (OVMF) con GRUB**: la pantalla del GOP a 32 bits, la
  consola, el mapa del firmware, la RSDP 2.0; con `-cpu max`, 4 núcleos y 3 GB
  (memoria por encima de 4 GB);
- un **pantallazo** de QEMU: la letra amarilla del título, la gris del texto y la
  verde de «todo en orden» están en la pantalla.

### Lo que la etapa 0 no hace (a propósito)

- El núcleo está aún en la mitad baja (en 1 MB): la mitad alta del núcleo pide el
  cambio del compilador de la etapa 1. El mapa directo de la mitad alta sí está.
- El mapa directo cubre los 64 primeros GB **enteros** (RAM y lo que no lo es), con
  la caché que digan los MTRR del firmware: en la etapa 1, solo la RAM, y los
  dispositivos con PAT.
- Sin TSS ni pilas IST: un doble fallo por la pila reinicia el equipo (etapa 1).
- Cuántos procesadores hay de verdad lo dirá la MADT (etapa 2): ahora es lo que dice
  CPUID del paquete.
- Sin entrada (teclado): se para con todo escrito; se apaga con el botón.

---

## 7. La etapa 1, hecha (23/11/2026): la memoria

### Qué hace

Hispa-64 0.1.0. El núcleo corre **en la mitad alta**: enlazado en
`0xFFFFFFFF80100000` (el «modelo de núcleo» de direcciones del compilador,
CASTEL64.md §10 bis), cargado en 1 MB. El arranque (de 32 bits, con las direcciones
físicas) pone unas tablas provisionales (los 64 primeros GB tal cual y en el mapa
directo, y el primer GB en `0xFFFFFFFF80000000`), pasa al modo largo y salta arriba.
Desde ahí, en Castel:

| Archivo | Qué |
|---|---|
| `gdt64.cas` | la GDT del núcleo (en la mitad alta; con los segmentos del anillo 3 que querrá `syscall`) y la **TSS con tres pilas IST**: el doble fallo (IST 1), el NMI (IST 2) y el fallo de máquina (IST 3) |
| `fisica64.cas` | **la memoria física**: un mapa de bits de marcos de 4 KB hecho con el mapa del firmware (multiboot2: el de UEFI o el de la BIOS; o el de multiboot). Solo es libre lo «disponible», y aun de eso se aparta el primer MB, el núcleo, la información del cargador, sus módulos, la pantalla y el propio mapa de bits. `pide`, `devuelve`, `pide_seguidos(n, alineación)`, `suelta_seguidos`; soltar un marco libre para el núcleo |
| `pagina64.cas` | **la memoria virtual**: `mapea` (páginas de 4 KB, 2 MB y 1 GB), `desmapea` (con `invlpg`), `traduce`, `marcas_de`; NX (EFER.NXE), WP, páginas globales, la **PAT** (1: escritura combinada; 3: sin caché); `mapea_disp` para los dispositivos. Y las tablas de verdad: el núcleo con el código de solo leer y los datos sin ejecutar, la página de guarda bajo la pila sin poner, el **mapa directo solo de la RAM** (y lo de ACPI y lo del cargador) con las páginas más grandes que se pueda, la pantalla con escritura combinada en `0xFFFFC00000000000`, y **sin la identidad de abajo**; las tablas del arranque (268 KB) vuelven a la memoria libre |
| `monton64.cas` | **el montón del núcleo**: `reserva(n)`/`libera(p)` de cualquier tamaño; casillas de 32 a 2048 octetos (en páginas del mapa directo) y, lo grande, en páginas propias en `0xFFFFC80000000000` con una sin poner detrás. Cabecera con marca y sello, 8 octetos de guarda detrás, lo liberado envenenado: se para y lo dice si se escribe de más, se libera dos veces, se libera algo que no es suyo o se escribe en un bloque liberado |
| `cerrojo.cas` | cerrojos de espera activa (con las interrupciones quitadas mientras se tienen): la memoria física, las tablas y el montón ya van con cerrojo, para la etapa 3 |
| `nombre64.cas`, `simbolos.py` | **los nombres de las funciones**: `simbolos.py` saca del mapa de NASM `HISPA64.SIM` («dirección Módulo.función»), que GRUB carga como módulo (`module2`) y QEMU con `-initrd`; una excepción o una parada dice en qué función ha pasado y **quién la ha llamado** (siguiendo los RBP; una función que se llama a sí misma se cuenta, no se repite) |
| `prmem64.cas` | las pruebas de cada arranque: páginas de 4 KB (puesta, leída, quitada y puesta en otro marco: si la TLB no se vaciase, se leería el de antes), de 2 MB y de 1 GB; las banderas del núcleo; el montón con 2.000 reservas al azar; y la larga (`prueba=monton`): 100.000, con 1.000 vivos a la vez, de 1 a 20.025 octetos, comprobando que al acabar no se ha perdido ni un marco |
| `excep64.cas` | además: las puertas del doble fallo, el NMI y el #MC con su IST; «la pila del núcleo se ha desbordado» cuando el doble fallo es por eso; las paradas del propio núcleo (`detén_ini`/`detén_fin`, código 98 en QEMU); una excepción dentro de otra (en la consola) sigue solo por el puerto serie |

Lo que dice (QEMU con OVMF y 256 MB, abreviado):

```
Memoria: 248,7 MB libres en 7 trozos (de 249,7 MB que da el firmware); 380 KB usados por el núcleo
  el núcleo, 312 KB (en 0xFFFFFFFF80100000); el mapa de los marcos, 8 KB; 15 tablas de páginas
  mapa directo (desde 0xFFFF800000000000), solo de la RAM y lo de ACPI, en páginas de 1 GB: 0; de 2 MB: 122; de 4 KB: 2011; sin ejecutar (NX: sí)
  sin la identidad de abajo: 1 MB ya no está en las tablas
  la pantalla, en 0xFFFFC00000000000, con escritura combinada (PAT)
  módulo del cargador: HISPA64.SIM, 5 KB en 0x1000: los nombres de 181 funciones (HISPA64.SIM)
El núcleo, en la mitad alta: 0xFFFFFFFF80100000 (código, de solo leer), los datos sin ejecutar desde 0xFFFFFFFF80108000
  la pila, de 0xFFFFFFFF8014E000 a 0xFFFFFFFF8015E000, con una página de guarda debajo
TSS con pilas propias (IST) para el doble fallo (0xFFFFFFFF80181D20), el NMI (...) y el fallo de máquina (...)
Páginas: de 4 KB, puesta, leída, quitada y puesta en otro marco (la TLB, al día); de 2 MB, también; de 1 GB, no (el procesador no las tiene)
  el código del núcleo, sin escribir: sí; los datos, sin ejecutar: sí; la guarda de la pila, sin poner: sí
Montón: 2000 reservas y liberaciones al azar (de 1 a 20025 octetos, 59 grandes; 64 a la vez), comprobadas; al acabar, 0 en uso y la memoria física, la de antes (el montón se queda 25 páginas)
Hispa-64 etapa 1: todo en orden.
```

Con `prueba=doble` (una función que se llama a sí misma sin fin):

```
*** Hispa-64 se ha parado: excepción 8 ***
Doble fallo (#DF): ha fallado al atender otra excepción
(Se atiende con su propia pila: la IST 1 de la TSS.)
La pila del núcleo se ha desbordado: RSP=0xFFFFFFFF8014DF68 está en la página de guarda (0xFFFFFFFF8014D000 - 0xFFFFFFFF8014DFFF),
que no está en las tablas a propósito.
En la función Hispa64.hondo+0x12
RIP=...  (los registros)
Llamada desde: Hispa64.hondo+0x32 (y 39 veces más, por lo menos)
```

Y con `prueba=dosveces`:

```
*** Hispa-64 se ha parado: el montón del núcleo está estropeado ***
En el bloque 0xFFFF800000121010: se libera dos veces
Llamada desde: Monton64.estropeado+0x50 <- Monton64.libera+0x79 <- Hispa64.provoca+0x1C7 <- Hispa64.principal+0x74F <- el arranque+0x10
```

Ocupa 40 KB (más unos 470 KB que empiezan a cero; de ellos, 268 KB de las tablas del
arranque, que se devuelven).

### Cómo se ha probado

`castel/nucleo64/pruebas.sh` (24 pruebas, 25 con CASTEL64 y ENSAMB64; unos 40 s; dentro de `SOLO=castel64
./pruebas.sh`, con CASTEL64 y ENSAMB64):

- se compila igual con ccastel.py + NASM, ensambla.py, CASTEL64 + ENSAMB64 y CASTEL32
  (corre32); el «modelo de núcleo»: ninguna etiqueta en un registro de 32 bits;
- por multiboot (QEMU `-kernel`, con `HISPA64.SIM` de módulo): con **128 MB** (126,5 MB
  libres), con **3 GB** y `-cpu max` (2,9 GB; el mapa directo y la prueba con páginas
  de 1 GB) y con **8 GB** y `qemu64` (7,9 GB; sin páginas de 1 GB, con 4.094 de 2 MB);
- `prueba=monton`: 100.000 reservas y liberaciones (3.233 grandes), sin perder nada;
- las excepciones, explicadas con el nombre de la función y quién la ha llamado:
  `pagina`, `gp`, `division`, `ud`, **`doble`** (IST 1, la pila desbordada),
  `identidad` (1 MB ya no está), `codigo` (escribir en el código: sin permiso), `nx`
  (ejecutar en el montón: sin permiso); y el **NMI** que manda el monitor de QEMU
  (IST 2);
- las paradas del montón: `corrupcion` (escribir un octeto de más) y `dosveces`;
- un procesador sin modo largo;
- por **UEFI de 64 bits (OVMF) con GRUB**: con 128 MB, con 3 GB, `-cpu max` y 4
  procesadores, y con 8 GB; el módulo `HISPA64.SIM`, la pantalla con escritura
  combinada, y un pantallazo con la consola.

### Lo que queda para la etapa 2 (y más adelante)

- Las tablas de ACPI que el firmware ponga en memoria «reservada» (no en la de ACPI)
  no están en el mapa directo: se mapearán con `mapea_disp` al leerlas (etapa 2).
- Las tablas de páginas que se quedan vacías al desmapear no se devuelven aún; la
  zona de los bloques grandes del montón no se reutiliza (avanza: hay 8 TB).
- La GDT, la TSS y sus pilas IST son de un procesador: en la etapa 3, una por cada uno
  (y la PAT, NX y WP puestos en cada uno); vaciar la TLB de los demás (IPI).
- La mitad alta de la PML4 se compartirá entre los procesos (etapa 4): conviene
  tener ya hechas sus 256 PDPT para que todos vean lo mismo.
- El compilador: los números escritos de 64 bits (2.1), el puente con C.
- El montón, sin cerrojo por procesador ni cachés por procesador (cuando haya SMP, si
  hace falta por velocidad).
- El fallo de máquina (#MC) va a su IST, pero no se lee aún lo que dicen los bancos
  (MCi_STATUS): etapa 2 o 3.

---

## 8. La etapa 2, hecha (23/11/2026): ACPI, las interrupciones, el tiempo y PCIe

### Las decisiones

- **Las tablas de ACPI, leídas por el núcleo; ACPICA, más tarde.** Lo que pide
  esta etapa son tablas fijas (MADT, FADT, HPET, MCFG): unas 500 líneas de Castel
  (acpi64.cas). ACPICA (unas 100.000 líneas de C) pide el puente con C, una capa de
  servicios del sistema (`AcpiOs*`: memoria, cerrojos, semáforos, hilos, las
  interrupciones) y, en la práctica, musl o algo parecido: lo de las etapas 3 a 7.
  Lo que sí pide AML (la batería, la tapa, dormir, el botón de los portátiles que
  lo llevan por el AML, el «_PRT» de las interrupciones de PCI) llega en la 11, con
  ACPICA. Para apagar basta con `\_S5` (un `Name` con un paquete: se busca en la
  DSDT y las SSDT sin intérprete, como hacen muchos sistemas pequeños).
- **El compilador**: los números escritos de 64 bits y `alineado` (CASTEL64.md
  §10 ter), en ccastel.py y en CASTEL (los de 16, 32 y 64 bits), con la misma
  salida. El núcleo ya los usa (el mapa directo, la base del núcleo, la PAT, el
  bloque del procesador alineado a 64). El puente con C, no: aún no hace falta.
- **Sin el 8259 ni el 8254**: el 8259 se tapa entero (con sus vectores en 0x20-0x2F,
  para que una espuria no parezca una excepción); el tic es el temporizador del
  APIC local, medido contra el HPET o, si no hay, el reloj de ACPI (el 8254, su
  canal 2, solo si no hay ninguno de los dos).
- **El IOAPIC se prueba de verdad**: el temporizador 0 del HPET (o, sin HPET, el
  8254) por una GSI, 20 veces.
- **Preparado para la etapa 3**: lo de cada procesador va en un bloque (PorCpu64,
  con GS apuntando a él: `yo()` es `mov rax, [gs:0]`): su APIC, sus tics, su GDT,
  su TSS y sus IST; el APIC local se pone con una función que llamará cada uno.
- **Con cuidado con el hardware de verdad**: antes de dejar pasar la SCI se
  apagan todos los GPE y todo lo de PM1 menos el botón (sin AML no se sabe
  atender lo demás; una SCI por nivel que nadie borra lo pararía todo), y si aun
  así llegan mil que no son el botón, se tapa.

### Qué hace

Hispa-64 0.2.0. Los módulos nuevos:

| Archivo | Qué |
|---|---|
| `acpi64.cas` | la RSDP (la de GRUB por multiboot2; por multiboot, buscada en la EBDA y en 0xE0000-0xFFFFF), la XSDT o la RSDT y cada tabla con su suma; lo «reservado» con `mapea_disp` (de 64 KB en 64 KB); la MADT (procesadores con APIC local y x2APIC, los que se pueden encender, IOAPIC, cambios de las IRQ de ISA, el NMI de LINT), la FADT (reloj de ACPI de 24 o 32 bits, SCI, SMI_CMD, PM1, GPE, el siglo del CMOS, IAPC_BOOT_ARCH, el reinicio, hardware reducido, la DSDT), HPET, MCFG; `\_S5`; pasar al modo ACPI, el botón, apagar (S5) y reiniciar |
| `reloj64.cas` | el HPET (a contar, sin sustituir al 8254) o el reloj de ACPI sumado en 64 bits: `ahora_ns()`; el canal 2 del 8254 para medir 10 ms si no hay otro; la frecuencia del TSC y si es invariante; la fecha y la hora del CMOS (BCD o no, 12 o 24 h, el siglo) |
| `apic64.cas` | el 8259 tapado; el APIC local (x2APIC si lo hay, por los MSR; si no, por su memoria sin caché): SVR, LINT0 tapada, el NMI por LINT según la MADT, los errores; su temporizador medido y a 100 por segundo (vector 0x40); los IOAPIC (todo tapado; una GSI o una IRQ de ISA a un vector, con su polaridad y su disparo); lo que se atiende: el tic, el teclado PS/2 (una cola de códigos), la SCI (el botón), la espuria, una prueba |
| `pci64.cas` | la configuración por ECAM (MCFG) o por 0xCF8; todos los buses del segmento 0; quién es cada función, su clase en español, su IRQ y sus BAR con su tamaño (de E/S, de 32 o de 64 bits, precargables; las de más de 4 GB se dejan donde están) |
| `porcpu64.cas` | el bloque de cada procesador (con GS); el del que arranca, en los datos; `nueva()` para los de la etapa 3 |
| `es64.cas` | los puertos de 8, 16 y 32 bits, los MSR, el TSC, la memoria de los dispositivos con el ancho justo, `sti`/`hlt`, a·b/c con 128 bits |

Lo que dice (QEMU con OVMF, q35, 4 procesadores; abreviado):

```
ACPI: 2.0 o más, de «BOCHS »; XSDT en 0x3F77D0E8, 7 tablas: FACP APIC HPET MCFG WAET BGRT DSDT (todas con la suma bien)
  procesadores (MADT): 4 encendidos (APIC 0, 1, 2, 3)
  APIC local en 0xFEE00000; con 8259 (se tapa)
  IOAPIC 0 en 0xFEC00000, GSI 0 a 23 (24 entradas, versión 0x20)
  IRQ de ISA cambiadas: 0 -> GSI 2; 5 -> GSI 5 por nivel, activa en alto; 9 -> GSI 9 por nivel, activa en alto; ...
  FADT: reloj de ACPI en el puerto 0x608 (24 bits), la SCI en la IRQ 9, 8042: sí, botón de encendido fijo, apagar (\_S5): SLP_TYP 0
  HPET en 0xFED00000
  MCFG: PCIe por ECAM en 0xE0000000, segmento 0, buses 0 a 255
Reloj: el HPET, a 100,0 MHz, 3 temporizadores, contador de 64 bits
  TSC: 1998 MHz; invariante: no
APIC local: en xAPIC, en 0xFEE00000, el de este procesador es el 0; versión 0x14; el 8259, tapado
  su temporizador: 630929 cuentas cada 10 ms (entre 16), medido con el HPET; el tic, 100 por segundo, al vector 0x40
Tic: 100 tics en 1 s (medido con el HPET); entre dos cambios del segundo del CMOS: 99 tics
IOAPIC: el temporizador 0 del HPET, por la GSI 16, al vector 0x3E: 20 de 20 interrupciones (cada 10 ms)
Teclado PS/2: la IRQ 1, por la GSI 1, al vector 0x31
Botón de encendido: en modo ACPI; la SCI por la GSI 9; pulsarlo apaga el equipo
PCI (PCIe, por ECAM): 6 funciones en los buses 0 a 255, 1 BAR por encima de 4 GB
  00:00.0 8086:29C0 puente: del anfitrión
  00:01.0 1234:1111 pantalla: VGA
      BAR0 0x80000000, 16,0 MB de memoria, precargable
  00:02.0 1AF4:1001 almacenamiento: SCSI, IRQ 11
      BAR4 0xC000000000, 16 KB de memoria de 64 bits (por encima de 4 GB), precargable
  00:1F.2 8086:2922 almacenamiento: SATA (AHCI), IRQ 10
  ...
Hora (CMOS): 23/11/2026 17:43:04
...
Hispa-64 etapa 2: todo en orden.
(Pulse teclas para ver sus códigos; el botón de encendido apaga el equipo.)
Reloj: 23/11/2026 17:43:10 (tics: 1234)
```

En el equipo de verdad no se para: enseña la hora cada segundo y el código de cada
tecla que se pulse, y el botón de encendido apaga el equipo por ACPI. Las entradas
nuevas del menú: «sin el temporizador del APIC» (`prueba=sintic`, por si se queda
en el tic) y «apagar el equipo por ACPI» (`prueba=acpiapagar`).

### Cómo se ha probado

`castel/nucleo64/pruebas.sh`: 33 pruebas (34 con CASTEL64 y ENSAMB64), unos 3
minutos. Las de la etapa 2:

- por multiboot con `-machine pc` (la RSDP buscada en la BIOS, ACPI 1.0, RSDT, sin
  MCFG: PCI por los puertos) y con `q35` y 4 procesadores (MCFG, ECAM, un puerto
  PCIe con un xHCI detrás: el bus 1 y su BAR de 64 bits);
- sin HPET (`-machine pc,hpet=off` y `q35,hpet=off`, con 4 procesadores): el reloj
  de ACPI, y el IOAPIC con el 8254;
- por UEFI (OVMF + GRUB): q35 con 128 MB; `-cpu max`, 4 procesadores y 3 GB (la
  MADT con 4, la BAR de 64 bits del disco virtio en 0xC000000000); q35 sin HPET; y
  `-machine pc` con 2 procesadores;
- en todas: el tic, **100 en 1 s (de 75 a 110, y de 50 por UEFI con 4 procesadores: con el servidor cargado el TCG de QEMU pierde tics; sin carga, 99 o 100) y otra vez entre dos cambios del segundo
  del CMOS** (otro reloj: no es el que lo ha medido); el IOAPIC, 20 de 20 (o 17 a
  23 con el 8254); y **la lista de PCI, la misma que da QEMU** («info pci» por su
  monitor);
- el teclado (`sendkey a`, `sendkey b` por el monitor de QEMU: 0x1E 0x9E 0x30 0xB0),
  el botón de encendido (`system_powerdown`: llega por la SCI y apaga por S5; QEMU
  acaba con 0) y `prueba=acpiapagar`.

Lo que QEMU no prueba: **x2APIC** (el TCG de QEMU 8.2 no lo tiene; el camino por
los MSR solo se probará en el taller: el ThinkPad y el Monovate lo tienen) y los
procesadores híbridos.

### Lo que queda para la etapa 3 (y más adelante)

- Arrancar los demás procesadores (INIT-SIPI-SIPI con el trampolín bajo 1 MB):
  cada uno con su bloque (`PorCpu64.nueva`), su GDT, su TSS y sus pilas IST (hoy
  las de gdt64.cas son del que arranca), su `pon_lapic()` y su tic; la PAT, NX y
  WP en cada uno; vaciar la TLB de los demás (IPI); qué núcleos son P y cuáles E
  (CPUID 0x1A en cada uno).
- El tic en modo TSC-deadline (si lo hay) y sin tic cuando no hay nada que hacer.
- MSI y MSI-X (para NVMe y xHCI, etapas 5 y 6); el `_PRT` de las interrupciones de
  PCI, con ACPICA (etapa 11); las BAR de más de 4 GB, mapeadas al usarlas.
- ACPICA (el puente con C): la batería, la tapa, dormir, el botón por el AML.
- Leer los bancos del fallo de máquina (MCi_STATUS).

---

## 9. La etapa 3, hecha (23/11/2026): varios procesadores y el planificador

### Las decisiones

- **El trampolín, en una página por debajo de 1 MB, y todo relativo a ella.**
  `trampolin.asm` (231 octetos; los ensambladores de la casa no saben de 16 bits,
  así que van como `db` en `smp64.cas` y `pruebas.sh` comprueba que son los que
  saca NASM) entra en modo real, pasa a 32 bits y a 64 con `retf` (sin saltos
  lejanos que haya que cambiar: EBX lleva su dirección), con su propia GDT, su
  pila en lo alto de la página y unas **tablas de páginas del trampolín** (en el
  núcleo, por debajo de 4 GB: la identidad de los 2 primeros MB, para el momento
  de encender la paginación, y la mitad alta copiada de las de verdad). EFER.NXE
  se pone **antes** de la paginación (las tablas del núcleo llevan NX: sin él,
  serían bits reservados). La página la da `Fisica64.pide_bajo` (disponible y sin
  pisar lo del cargador; el primer MB nunca se da a nadie).
- **De uno en uno**, INIT, 10 ms, SIPI, 200 µs, SIPI (si no ha llegado) y hasta 3
  s de espera; si no llega, otro INIT (que no se quede a medias con la página) y
  se cuenta como «no contesta». Cada uno, antes de salir del trampolín, tiene ya
  su bloque de PorCpu64, su pila (la que será la de su tarea ociosa) y las tres
  pilas IST, en la **zona de las pilas** (`0xFFFFE00000000000`: sitios de 32 KB,
  la pila arriba y 16 KB sin poner debajo: quien se desborde, falla, y Excep64 lo
  dice), y su GDT y su TSS en el montón. Como mucho 64 (MAX_CPUS); los de más se
  cuentan.
- **Una cola por procesador, con robo de trabajo** (no una sola con cerrojo). Con
  una sola cola, todos los procesadores tomarían el mismo cerrojo en cada tic y en
  cada cambio de hilo: con 8 o 16, lo que más se esperaría. Con una por
  procesador, cada uno toma de la suya, y solo el que se queda sin nada roba de la
  más larga de otro, **con `intenta()`** (sin esperar: dos que roben el uno del
  otro no se cruzan). Al despertar o crear un hilo se pone en uno en reposo (en
  los híbridos, los interactivos prefieren los P y los de fondo los E) o en el de
  menos trabajo, y se le avisa con un IPI.
- **Lo delicado, como en Linux**: el cerrojo de la cola se tiene durante el
  cambio de hilo y lo suelta el que entra (`tras_cambio`); así un hilo que está en
  una cola ya ha dejado del todo su procesador. Quien despierta a otro (con `lock
  cmpxchg` de DURMIENDO a LISTO) espera a que haya dejado su procesador (`en_cpu =
  0`) antes de ponerlo en una cola; entre ponerse a dormir y cambiar, las
  interrupciones están quitadas. A los que esperan a que acabe un hilo (`une`) los
  despierta el que entra tras él: así nadie suelta su pila mientras aún corre en
  ella (y uno normal que espera a uno de fondo no se queda dando vueltas delante).
- **Leer «lo de este procesador» desde un hilo, sin interrupciones**: entre leer
  el bloque (`yo()`) y leer de él, el tic puede quitarle el procesador al hilo y
  seguir en otro; `actual()` (el hilo que corre) lo hace con ellas quitadas. Lo
  enseñó la prueba de los 100 hilos con 8 procesadores por UEFI: un mutex creía
  que lo abría otro hilo.
- **Prioridades estrictas** (tiempo real, interactivo, normal, de fondo), turnos de
  20 ms dentro de cada una, expropiación con el tic de cada procesador o con el
  aviso de otro (V_PLAN, 0xF2) cuando despierta uno de más prioridad.
- **Vaciar la TLB de los demás**: `desmapea` quita la página de las tablas y de la
  TLB propia, suelta el cerrojo y llama a `Smp64.derriba`: un IPI al vector V_TLB
  (0xF1) y espera a que **cada** procesador en marcha diga que lo ha hecho (su
  `tlb_visto`, por número de aviso); solo después se da el marco a otro. Quien
  espera el cerrojo de los avisos atiende mientras los de los demás (no se cruzan
  dos que avisan). Regla: **no se llama a `desmapea` con un cerrojo tomado** (el
  montón ya suelta el suyo antes de quitar las páginas de un bloque grande;
  `desmapea_aquí` no avisa, para lo que nadie más ha visto).
- **Los pánicos**: el primero que se para (una excepción o una parada del núcleo)
  se queda con la parada (`lock cmpxchg`), manda un **NMI** a los demás (llega
  aunque tengan las interrupciones quitadas, dando vueltas en un cerrojo), espera
  hasta 100 ms (con el TSC) a que digan que se han parado y lo explica él solo,
  diciendo en qué procesador ha pasado. Los demás, al entrar en su NMI y ver que
  ya hay uno parándose, se quedan quietos sin escribir nada.
- **La tarea ociosa**: `hlt`, o `mwait` con la pista más honda de cpuid 5 donde es
  seguro, con la misma regla que la 0.8.9 de Hispa-Pentium «(MWAIT)»: solo Intel,
  no Atom, con MONITOR/MWAIT y sus pistas, ARAT, TSC invariante y no en una
  máquina virtual; `ocioso=hlt` lo quita y `ocioso=mwait` lo fuerza (así se prueba
  en QEMU). La despierta una interrupción (su tic o el IPI de otro): entre `sti` y
  `hlt`/`mwait` no se pierde ninguna.
- **El reloj de ACPI, con cerrojo**: se va sumando en 64 bits (`pm_suma`), así que
  dos procesadores no pueden leerlo a la vez; el HPET no lo necesita.

### Qué hace

Hispa-64 0.3.0. Los módulos nuevos y lo que cambia:

| Archivo | Qué |
|---|---|
| `smp64.cas` y `trampolin.asm` | encender los de la MADT (INIT-SIPI-SIPI y el trampolín de 16 → 32 → 64 bits); en cada uno, `ap_entra`: las tablas de verdad, la PAT y las páginas globales (NX y WP, ya del trampolín), GS a su bloque, su GDT, su TSS y sus IST, la IDT, su APIC local y su tic, el tipo de núcleo (CPUID 1Ah: 0x40 de rendimiento, 0x20 eficiente) y a su tarea ociosa; `derriba` (vaciar la TLB de los demás); `para_los_demás` (el NMI de los pánicos); `lee_con_red` (leer sin pararse si la página no está: las pruebas de la TLB) |
| `hilos64.cas` | los hilos del núcleo (pila de 16 KB con guarda), el planificador (colas por procesador, robo, 4 prioridades, turnos, expropiación), `crea`, `termina`, `une`, `suéltalo` y `recoge`, `cede`, `duerme_ns`/`duerme_ms` (la lista de los dormidos, por orden), las colas de espera (`duerme_en`, `despierta_uno`, `despierta_todos`), la tarea ociosa (`hlt` o `mwait`) y lo que se cuenta (cambios, robos, el tiempo en reposo de cada uno) |
| `sinc64.cas` | sucesos (`espera`, `señala`, `rearma`), semáforos (`baja`, `sube`) y mutex (`cierra_mutex`, `abre_mutex`; abrir uno que no es suyo, o cerrar dos veces, para el núcleo y lo dice) |
| `prsmp64.cas` | las pruebas de cada arranque (abajo) y las de a mano: `prueba=panicoap`, `paradaap`, `pilahilo` |
| `cerrojo.cas` | además: `intenta`, `toma_sin`/`suelta_sin`, `suma_atómica` (`lock xadd`), `cambia_si` (`lock cmpxchg`), `pon_atómico`, `sin_ints`/`restaura` |
| `porcpu64.cas` | el bloque de cada procesador, con lo del planificador y los avisos; `cpu(n)`, `da_de_alta`; ya no importa el montón (lo usan las excepciones) |
| `gdt64.cas` | `monta(gdt, tss, ist1, ist2, ist3)`: la GDT y la TSS en cualquier sitio (las de cada procesador) |
| `pagina64.cas` | `desmapea` avisa a los demás (`desmapea_aquí`, no); la zona de las pilas (`pila_nueva`, `pila_suelta`); `prepara_este` (la PAT, CR3 y las páginas globales en otro procesador) |
| `excep64.cas` | el pánico de uno para a todos; «En el procesador N (APIC a); los otros N se han parado.»; las IST de cada procesador; los fallos de página «con red»; el desbordamiento de la pila de un hilo; quién ha llamado, también en las pilas de los hilos; `carga_idt` |
| `apic64.cas` | los IPI (`ipi`, `ipi_a_los_demás`; por el ICR del xAPIC o el MSR 830h del x2APIC) y `al_tic` (el planificador) |
| `fisica64.cas`, `reloj64.cas`, `monton64.cas` | `pide_bajo` (la página del trampolín); el cerrojo del reloj de ACPI; el montón quita las páginas de un bloque grande sin su cerrojo tomado |

Desde la etapa 3, lo que sigue en `hispa64.cas` (las pruebas a mano, el reloj y las
teclas del equipo de verdad) corre en el hilo «principal», que puede ir en
cualquier procesador; la línea del reloj dice además lo ocupado que está cada uno.
Las entradas nuevas del menú: «con un solo procesador» (`sinsmp`), «la tarea ociosa
con hlt» (`ocioso=hlt`) y «prueba de un fallo en otro procesador»
(`prueba=panicoap`).

Lo que dice (QEMU, q35, multiboot, 4 procesadores; abreviado):

```
Procesadores: 4 en marcha (APIC 0, 1, 2, 3)
  con INIT-SIPI-SIPI y el trampolín en 0x9E000 (16, 32 y 64 bits); cada uno con su GDT, su TSS y sus pilas IST, la PAT, NX y WP, su APIC local y su tic
Planificador: una cola por procesador, con robo de trabajo; 4 prioridades; turnos de 20 ms; la tarea ociosa, con hlt (sin MONITOR/MWAIT)
Hilos: 100 hilos que suman 1, 10000 veces cada uno, a la vez: con un cerrojo, 1000000 (exacto); con «lock xadd», 1000000; con un mutex (500 cada uno), 50000 (exacto); en 657 ms
  sin cerrojo (a propósito): 992798: se pierden sumas, porque corren a la vez de verdad
  al acabar, sus pilas y sus bloques, devueltos (pilas en uso: 13, las de antes)
Reparto: el trabajo de los sumadores, por procesador: 0: 26 % 1: 25 % 2: 25 % 3: 24 % (todos han trabajado)
  robos de trabajo: 55; expropiaciones: 0
Dormir: 8 hilos, de 20 a 160 ms, dos veces: ninguno antes de tiempo (0); el que más se pasa, 7,8 ms (el tic es de 10 ms); el principal, 100 ms: 106,4 ms
Suceso: 20 hilos lo esperan; antes de señalarlo, despiertos 0; después, 20
Semáforos: 4 productores y 4 consumidores, 10000 cosas por un búfer de 16: la suma, 50005000 (exacta)
Prioridades: en el procesador 3, uno normal y tres de fondo, sin parar, 300 ms: el normal hace el 100 % del trabajo
  y uno interactivo que duerme 10 ms, 10 veces, en ese mismo procesador: se pasa de media 1,7 ms (pasa por delante: 11 expropiaciones)
TLB de los demás: una página puesta en el procesador 0 y leída en el 3: lee 0x1111
  quitada sin avisar y puesta en otro marco (con 0x2222): el 3 aún lee 0x1111 (el de antes: su TLB no se ha vaciado)
  y, ya con el marco nuevo en su TLB (lee 0x2222), quitada con aviso (desmapea: 1 aviso por IPI): al 3 le falla la lectura (fallo de página en 0xFFFFD00100000000, con red), como debe
Carga (ocupado): 0: 90 % 1: 89 % 2: 89 % 3: 94 %
...
Hispa-64 etapa 3: todo en orden.
```

En un híbrido sale además una línea «híbrido: N de rendimiento (P: APIC ...) y M
eficientes (E: APIC ...)» (cada hilo de un núcleo P cuenta como uno). Con `prueba=panicoap` (un fallo de página en un hilo
fijo en el último procesador):

```
*** Hispa-64 se ha parado: excepción 14 ***
Fallo de página (#PF): una dirección que no está en las tablas de páginas, o sin permiso
En el procesador 3 (APIC 3); los otros 3 se han parado.
Al leer de 0x0000500000000000: la página no está (desde el núcleo).
En la función PrSmp64.pánico+0x20
...
Llamada desde: Hilos64.llama1+0xC <- Hilos64.arranque_hilo+0x31
```

El núcleo ocupa 88 KB.

### Cómo se ha probado

`castel/nucleo64/pruebas.sh`: 44 pruebas (45 con CASTEL64 y ENSAMB64), unos 4
minutos (dentro de `SOLO=castel64 ./pruebas.sh`, unos 7). Las de la etapa 3:

- los 231 octetos del trampolín de `smp64.cas` son los que saca NASM de
  `trampolin.asm`;
- las de cada arranque (`prsmp64.cas`, comprobadas línea a línea por `e3_bien`)
  con **1, 2, 4 y 8 procesadores**: por multiboot con 1 (`pc`), 2 (`-machine pc
  -smp 2`), 4 (q35; y sin HPET, con `pc` y con q35: el reloj de ACPI, con su
  cerrojo, en los cuatro) y 8 (`-cpu max`); por UEFI (OVMF + GRUB) con 1, 2
  (`pc`), 4 (`-cpu max`, 3 GB; y q35 sin HPET) y 8 (`-cpu max`, 8 GB). En todas:
  - **cien hilos que suman** 10.000 veces cada uno: con un cerrojo, **1.000.000
    exacto**; con `lock xadd`, 1.000.000; con un mutex (500 cada uno), **50.000
    exacto**; de 260 ms (1 procesador) a 1,1-1,6 s (8, con el TCG en los 4 núcleos
    del servidor); sin cerrojo, a propósito, se pierden entre 1.000 y 8.000 sumas
    con 2 o más (corren a la vez de verdad); al acabar, las pilas y los bloques de
    los hilos, devueltos;
  - **el reparto**: todos los procesadores trabajan (con 4, del 23 al 28 % cada
    uno; con 8, del 10 al 16 %), con entre 23 y 490 robos de trabajo;
  - **dormir**: 8 hilos de 20 a 160 ms, dos veces: ninguno antes de tiempo; el que
    más se pasa, de 1,6 a 12 ms (el tic es de 10 ms; la prueba admite 50);
  - el **suceso** (20 esperan; ninguno antes, los 20 después) y los **semáforos**
    (4 productores y 4 consumidores, 10.000 cosas por un búfer de 16: la suma,
    50.005.000, exacta);
  - **las prioridades**: en un procesador, uno normal y tres de fondo sin parar:
    el normal hace el 100 % (la prueba pide 90); uno interactivo que duerme 10
    ms, 10 veces, ahí mismo, les pasa por delante (11 expropiaciones; se pasa de
    media de 1,7 a 7 ms; la prueba admite 25);
  - **la TLB de los demás** (con 2 o más): la página puesta en el 0 se lee en el
    último; quitada **sin avisar** y puesta en otro marco, el último **aún lee el
    valor de antes** (su TLB: el TCG de QEMU la tiene como el hardware); con el
    marco nuevo en su TLB, quitada **con aviso** (`desmapea`: 1 IPI), **le falla
    la lectura** (fallo de página, con red);
- `ocioso=mwait` con `-cpu max` y 4 procesadores (la tarea ociosa con
  `monitor`/`mwait`: todo igual); `sinsmp` con 4 en la MADT (solo el que arranca);
- **los pánicos en otro procesador**, que se explican una sola vez: `panicoap` con
  4 («En el procesador 3 (APIC 3); los otros 3 se han parado.», el fallo de página
  y quién ha llamado: el hilo), `paradaap` con 2 (abrir un mutex ajeno: «el otro
  se ha parado»), `pilahilo` con 2 (el doble fallo en la IST 1 del procesador 1:
  «La pila de un hilo ... se ha desbordado»), y el **NMI del monitor de QEMU con 4
  procesadores** (llega a todos: uno lo explica, los otros 3 se paran);
- lo de las etapas 0, 1 y 2, igual (las pruebas de las excepciones, con
  `sinpruebas`).

Lo que QEMU no prueba: **x2APIC** (los IPI por el MSR 830h) y los **híbridos**
(CPUID 1Ah): en el taller (el ThinkPad). Y `mwait` de verdad (en QEMU es como
`hlt`). Los tics de la etapa 2 con el servidor cargado: con 4 procesadores y sin
HPET han salido 64 en 1 s (otra sesión con QEMU en marcha); se admite de 50 a 110,
como ya se hacía por UEFI con 4.

`SOLO=castel64` (CASTEL64 y ENSAMB64 sacan lo mismo que ccastel.py y NASM con los
módulos nuevos; CASTEL32 con corre32, también), `SOLO=castel32` y `SOLO=16`: 80 bien y 0 mal (7 min y medio), 73 y 0, y 145 y 0.

### Lo que queda para la etapa 4 (y más adelante)

- **Sin tic en reposo** (temporizador de una vez, o TSC-deadline, hasta el siguiente
  dormido): etapa 11 (la energía). Hoy dormir va de 10 en 10 ms (el tic).
- Esperas **con plazo** (un suceso o un semáforo con tiempo máximo) y despertar por
  señal: con los procesos (etapa 4).
- **Procesos**: un espacio de direcciones por proceso; el planificador ya cambia de
  pila, le faltará cambiar CR3 (y con PCID, sin vaciar la TLB) y que el aviso de
  vaciar la TLB vaya solo a los que tienen ese espacio en uso.
- La mitad alta de la PML4, compartida entre procesos (sus 256 PDPT hechas de
  antemano): hoy el trampolín copia las entradas que haya al encender.
- **x2APIC** y los **híbridos**, solo probados en el taller (el TCG de QEMU no los
  tiene); los MTRR de los demás procesadores se fían de lo que deja el firmware.
- El montón, con un solo cerrojo (cachés por procesador si hace falta); las
  prioridades son estrictas (sin «envejecer» a los de fondo); como mucho 64
  procesadores y 64 hilos esperando a que acabe uno.
- Las estadísticas (`robados`, `expropiaciones`) se suman sin `lock`: son para
  contar, no para decidir.

---

## 10. La etapa 4, hecha (23/11/2026): los programas (anillo 3, `syscall` y ELF64)

### Las decisiones

- **Los programas de Linux x86-64, tal cual.** Las llamadas al sistema tienen los
  números y la forma de Linux (2.6), y los ejecutables son ELF64 estáticos: un
  programa en C hecho con el gcc del servidor (`-static -nostdlib`) y uno de Castel
  de `modelo linux64` (CASTEL64.md §7) corren en Hispa-64 sin cambiar un octeto (y
  los dos corren igual en Linux). Por eso no hace falta un `modelo usuario64`.
- **`swapgs` en todas las entradas.** El núcleo lee lo de su procesador en `gs:[0]`;
  con programas, cualquier interrupción puede llegar en el anillo 3 con el GS que
  haya puesto el programa (con un `mov gs` basta: no se le puede impedir). Las
  funciones de interrupción de Castel no saben hacer `swapgs`, así que la IDT
  apunta a una entrada por vector (`entradas.cas`, 256, en ensamblador) que, si se
  viene del anillo 3, hace `swapgs` y pone debajo un **marco de `iretq` falso** que
  «vuelve» a `E64_salida` (que hace `swapgs` y el `iretq` de verdad); desde el
  núcleo, salta tal cual. El NMI, el doble fallo y el fallo de máquina (con su IST:
  pueden llegar entre un `swapgs` y un `sysret`) miran `GS_BASE` con `rdmsr`. Así
  no ha hecho falta tocar el compilador ni las funciones de interrupción que ya
  había: `Excep64.explica` reconoce el marco falso y busca el de verdad.
- **`syscall`/`sysret`**, con STAR (el núcleo en 0x08; los programas desde 0x10:
  datos 0x1B, código 0x23, como ya dejó la GDT de la etapa 1), LSTAR y SFMASK (IF,
  TF, DF, AC, NT fuera). La entrada guarda la pila del programa en el bloque del
  procesador, toma **la pila del núcleo del hilo** (la que pone el planificador al
  cambiar, igual que TSS.RSP0), arma el mismo marco que una interrupción y llama a
  `Sistema` **con las interrupciones puestas** (una llamada larga, como
  `nanosleep`, duerme el hilo y deja el procesador a otro). Vuelve con `sysret`, con
  las banderas limpiadas y la dirección comprobada (el fallo de `sysret` de Intel
  con una dirección no canónica no puede darse: un programa nunca tiene la última
  página de la mitad baja).
- **La mitad alta, la misma para todos**: sus 256 PDPT se hacen al arrancar, antes
  de encender los demás procesadores; cada PML4 de un programa copia esas 256
  entradas y el núcleo ya nunca añade una a la suya.
- **CR3 al cambiar de hilo, y el aviso de vaciar la TLB solo a quien lo tiene
  puesto.** El planificador llama a `Procesos.al_cambiar` cuando uno de los dos
  hilos es de un programa o lleva otro espacio: pone su CR3 (los hilos del núcleo,
  el del núcleo), TSS.RSP0, la pila de `syscall`, FS y el GS del programa, y sus
  registros x87/SSE. Quitar una página de un programa (`munmap`, `mprotect`,
  `brk` que mengua) avisa con un IPI **solo a los procesadores que tienen ese
  espacio puesto ahora** (`Smp64.derriba_espacio`); los demás, si vuelven a él,
  ven por la cuenta de avisos del espacio (`tlb_gen`) que su TLB no está al día.
  Con **PCID** (si lo hay), cada espacio lleva un número (1 a 63) y CR3 se carga
  sin vaciar la TLB cuando el procesador sabe que lo que tiene de ese número es de
  ese espacio (su id, que no se repite) y está al día. El TCG de QEMU no tiene
  PCID: ese camino se prueba en el taller, y por si acaso hay una entrada del menú
  «sin PCID» (`sinpcid`).
- **Comprobación estricta de los punteros de los programas**: el núcleo no toca
  nunca la mitad baja (con SMAP, si lo intentase, fallaría); cada octeto que lee o
  escribe de un programa lo busca en las tablas de **su** espacio (presente, con la
  marca de usuario y, para escribir, con permiso) o en una de sus regiones con
  permiso (y entonces pone la página), y lo toca **por el mapa directo**. Un
  puntero al núcleo, a nada, nulo, que da la vuelta o a algo de solo leer cuando
  se va a escribir da **-EFAULT** (los números de error, los de Linux).
- **Las páginas, al tocarlas.** Cada proceso tiene sus regiones (el programa, la
  pila de 8 MB, el montón de `brk`, lo de `mmap`), con sus permisos; un fallo de
  página de algo suyo que aún no está pone una página a cero y sigue. Solo lo que
  viene del archivo se copia al cargar. (Un programa de Castel lleva 16 MB de pila
  en lo que empieza a cero: solo se ponen las páginas que usa.)
- **Los fallos de los programas, explicados, y solo él cerrado**: «El programa MALO
  (proceso 13) ha intentado leer la dirección 0xFFFFFFFF80100000, y no es suya: es
  del núcleo.», con RIP y RSP, y el código con el que acaba es el de Linux (128 +
  SIGSEGV, SIGFPE, SIGILL...).
- **SSE para los programas** (los compiladores de hoy lo dan por hecho): CR0 y CR4
  con SSE, y el planificador guarda y pone los registros x87/SSE de cada programa
  (`fxsave64`/`fxrstor64`, 512 octetos por hilo). El núcleo no los usa. AVX
  (XSAVE) más adelante.
- **Un hilo por proceso** en esta etapa; los hilos de POSIX (`clone`), con musl.

### Qué hace

Hispa-64 0.4.0. Los módulos nuevos y lo que cambia:

| Archivo | Qué |
|---|---|
| `entradas.cas` | las 256 entradas de la IDT (con `swapgs` y el marco falso), la de `syscall` (`E64_sistema`), `a_usuario` (el primer `iretq` al anillo 3) y lo de cada procesador para los programas (`prepara_cpu`: EFER.SCE, STAR, LSTAR, SFMASK, SMEP, SMAP, UMIP, PCID, SSE) |
| `espacios.cas` | los espacios de direcciones: la mitad alta hecha de antemano, `crea_espacio`, `mapea_u`, `quita_u`, `quita_rango`, `remarca_rango` (con el aviso a quien lo tiene puesto), `activa_espacio` (CR3, con PCID), `destruye_espacio` |
| `unidad64.cas` | la unidad en memoria: los módulos del cargador por su nombre |
| `procesos.cas` | los procesos: las regiones, los archivos abiertos, el cargador de ELF64 (PT_LOAD, lo que empieza a cero, la pila de Linux con argc, argv, envp y el vector auxiliar: AT_PHDR, AT_PHENT, AT_PHNUM, AT_PAGESZ, AT_ENTRY, AT_RANDOM, AT_PLATFORM, AT_CLKTCK...), tocar la memoria de un programa, `al_cambiar`, los fallos, salir y esperar |
| `sistema.cas` | las llamadas: read, write, open, close, lseek, mmap, mprotect, munmap, brk, ioctl (-ENOTTY), writev, sched_yield, nanosleep, getpid, exit, uname, getuid y compañía, arch_prctl (FS y GS), gettid, set_tid_address, clock_gettime, exit_group, openat; las demás, -ENOSYS (y se dicen); lo que se escribe en UTF-8 pasa a la página 437 |
| `prproc64.cas` | las pruebas de cada arranque (abajo) |
| `programas/` | los programas de prueba: `hola.c`, `memoria.c`, `cuenta.c`, `malo.c` (con `sis.h`: `syscall` directo), `holacas.cas` (Castel, `modelo linux64`), `saludo.txt`; `hazlos.sh` los hace |
| `excep64.cas` | la IDT apunta a Entradas; las excepciones del anillo 3 van a `Procesos` |
| `hilos64.cas` | lo del programa en cada hilo (su espacio, su proceso, FS, GS, los registros SSE), `crea_de_programa` y la llamada a `al_cambiar` |
| `smp64.cas` | `derriba_espacio` (el aviso solo a quien tiene el espacio puesto) y `prepara_cpu` en cada procesador |
| `porcpu64.cas` | la pila de `syscall`, el espacio puesto y hasta qué aviso está al día |
| `reloj64.cas` | `lee_fecha` y `segundos_unix` (CLOCK_REALTIME) |
| `imagen.sh` | los programas en `\boot\prog` (`module2`), la entrada «sin PCID» y el CD (`hispa64-cd.iso`, `grub-mkrescue`: BIOS y UEFI) |

Lo que dice (QEMU, q35, multiboot, 4 procesadores, `-cpu max`; abreviado):

```
Programas: syscall y sysret (los números de Linux x86-64), swapgs, una pila del núcleo por hilo
  SMEP: sí; SMAP: sí; UMIP: sí; PCID: no; la mitad alta, compartida: sus 256 PDPT, hechas al arrancar (254 nuevas)
Unidad en memoria: 6 archivos: CUENTA (9 KB) HOLA (14 KB) HOLACAS (0 KB) MALO (10 KB) MEMORIA (10 KB) SALUDO.TXT (0 KB)
Hola desde el anillo 3: soy HOLA, un programa en C (ELF64 estático, sin biblioteca)
  argc = 4: HOLA uno dos tres
  el vector auxiliar: AT_PAGESZ 4096, AT_ENTRY 0x0000000000401000, AT_PHDR con 6 segmentos, AT_RANDOM sí
  ...
HOLA: todo bien (28 pruebas)
Programa «HOLA uno dos tres»: acaba con el código 0, como debe (en 37 ms)
Hola desde el anillo 3: soy HOLACAS, un programa de Castel (modelo linux64)
...
MEMORIA: 29 pruebas, todas bien
CUENTA 1: proceso 4, 0x7F03F5F9F0925A31, 12188758, igual en las dos vueltas
...
A la vez: 8 procesos CUENTA: los 8 acaban con su código (10 a 80), sus registros intactos; en los procesadores 0, 1, 2, 3; en 119 ms
MALO punteros: 24 llamadas con punteros malos, todas con su error (-EFAULT, -EPERM...); sigue vivo
MALO leer: voy a leer la memoria del núcleo
El programa MALO (proceso 13) ha intentado leer la dirección 0xFFFFFFFF80100000, y no es suya: es del núcleo.
  (en RIP=0x000000000040172D, RSP=0x00007FFFFFFFEE50) Se cierra el programa; el sistema sigue.
El programa MALO (proceso 16) ha dividido entre cero.
El programa MALO (proceso 22) ha intentado leer la dirección 0x00007FFFFF7FE398, y no es suya: está justo debajo de su pila (se le ha desbordado la pila).
...
Malos: 11 programas que fallan, a la vez: todos cerrados con su señal (SIGSEGV 139, SIGFPE 136, SIGILL 132), en 140 ms; el sistema sigue
No se arrancan: «NOESTA»: no está en la unidad; «SALUDO.TXT»: no es un programa ELF
Después: 0 procesos, 0 espacios, 0 tablas de programas; 23 arrancados, 11 cerrados por un fallo, 426 llamadas al sistema, 2509 páginas puestas al tocarlas
  la memoria: la de antes; el montón: 11 bloques (los de antes); las pilas: 13 (las de antes)
TLB de un espacio: una página de un espacio de prueba, leída desde el procesador 1 y el 2: 0x1111; el 2 deja el espacio
  quitada (desde el procesador 0, sin el espacio puesto): 1 aviso por IPI; al 1 le falla la lectura: sí; al 2, al volver a ponerlo, también: sí
Hispa-64 etapa 4: todo en orden.
```

Los once de MALO: leer el núcleo, escribir en su mapa directo, el puntero nulo,
dividir entre cero, `ud2`, `cli`, `in al, 0x60`, ejecutar en la pila (NX),
escribir en su código, desbordar la pila y saltar al núcleo; los once a la vez, y
cada uno explicado y cerrado. Las 24 llamadas de `MALO punteros`: `write` desde el
núcleo, el mapa directo, nada, el puntero nulo, más allá de la mitad baja y dando
la vuelta; `read` al núcleo, al mapa directo y a su código (de solo leer); `open`,
`clock_gettime`, `uname`, `nanosleep`, `writev` y `arch_prctl` con punteros del
núcleo; `mmap` fijo en el núcleo y en la página 0; `munmap` y `mprotect` del
núcleo; `brk` hasta el núcleo: todas dan su error y el programa sigue.

El núcleo ocupa 124 KB.

### Cómo se ha probado

`castel/nucleo64/pruebas.sh`: **53 pruebas** (54 con CASTEL64 y ENSAMB64), unos 6
minutos. Las de la etapa 4:

- los programas de prueba se hacen (`programas/hazlos.sh`: los cuatro en C con el
  gcc del servidor, `-static -nostdlib -ffreestanding`, y HOLACAS con ccastel.py y
  NASM) y van como módulos (`-initrd` con comas por multiboot; `module2` por GRUB);
- **por multiboot con 1 procesador** (`pc`, 128 MB), **2** (`pc`), **4** (q35) y
  **8** (`-cpu max`: con SMEP, SMAP y UMIP), y **por UEFI** (OVMF + GRUB) con 1 y
  con 4 (`-cpu max`, 3 GB). En todas (`e4_bien`): HOLA (28 comprobaciones propias:
  argv, la pila alineada, el vector auxiliar, getpid, set_tid_address, uname,
  nanosleep de 20 ms, CLOCK_REALTIME, un reloj que no hay, FS con arch_prctl y
  `%fs:0`, open/read/lseek/close de SALUDO.TXT, close dos veces, uno que no está,
  abrir para escribir, openat sin mirar mayúsculas, ioctl, writev, sched_yield, una
  llamada que no existe, coma flotante con SSE), HOLACAS (Castel: argv, getpid,
  uname, los acentos de la página 437), MEMORIA (29: brk que crece, mengua y no se
  pasa; mmap de 1 MB, munmap de un trozo, mmap fijo en el hueco, mprotect;
  64 MB con 64 páginas tocadas; MAP_32BIT; los errores), **ocho CUENTA a la
  vez** (cada uno hace su cuenta dos veces, de un tirón y cediendo el procesador:
  iguales, también la de coma flotante y FS; con 2 o más procesadores, en más de
  uno), **MALO punteros** (24 llamadas con punteros malos, todas con su error) y
  **los once MALO que fallan, a la vez** (cada uno explicado y cerrado con su
  señal: 139, 136, 132), lo que no se arranca, y **después, nada de más**: 0
  procesos, 0 espacios, 0 tablas de programas, y la memoria física, el montón y
  las pilas, los de antes; con 2 o más procesadores, **la TLB de un espacio**: el
  aviso de quitar la página va solo al procesador que lo tiene puesto (1 IPI), a
  ese le falla la lectura y al que lo había dejado también, al volver a él;
- **el CD** (`imagen.sh` con `CD=1`, `grub-mkrescue`): por la BIOS (`-cdrom`, 2
  procesadores) y por UEFI (OVMF, q35, 4 procesadores), con los programas y «todo
  en orden»;
- lo de las etapas 0 a 3, igual (las excepciones del núcleo, con `sinpruebas`, que
  quita también las de los programas).

`SOLO=castel64` (con CASTEL64 y ENSAMB64 el núcleo sale igual, octeto a octeto, y
CASTEL32 con corre32 también): **80 bien y 0 mal** (12 minutos, con el servidor
cargado). `SOLO=castel32` y `SOLO=16`, como antes: 73 bien y 0 mal, y 145 bien y 0 mal. Las imágenes de la carpeta del taller (el disco por UEFI con 4
procesadores y el CD por la BIOS), arrancadas en QEMU hasta «todo en orden».

En QEMU, con la pantalla gráfica (GOP o la de la BIOS), las pruebas de los
programas van de 10 a 20 veces más despacio que solo con el puerto serie (los once
avisos de MALO, 2,4 s en vez de 0,14): es la consola dibujada letra a letra y
desplazada por el TCG; en un equipo de verdad, la pantalla con escritura combinada
va mucho más deprisa.

Lo que QEMU no prueba: **PCID** (el TCG de QEMU 8.2 no lo tiene, ni con
`-cpu max,+pcid`): el camino con PCID (CR3 sin vaciar, el número de cada espacio y
hasta qué aviso llega) solo se prueba en el taller, con la entrada «sin PCID» por si
falla. Y, como antes, x2APIC y los híbridos.

### Lo que queda para la etapa 5 (y más adelante)

- **Discos y archivos** (etapa 5): NVMe, AHCI, GPT, el VFS con caché de páginas,
  FAT32 y exFAT; la unidad en memoria pasará a ser un sistema de archivos más del
  VFS (`open` y `openat` con carpetas, `stat`, `getdents64`, escribir).
- **El intérprete de órdenes en el anillo 3** pide leer el teclado desde un
  programa (`read` de la consola: hoy da 0): con la entrada (etapa 6), o antes con
  el teclado PS/2 que ya atiende el núcleo; y un `execve`/`wait4` para arrancar
  programas desde un programa.
- **musl** (etapa 7): `clone` (hilos de POSIX, varios hilos por proceso: el aviso
  de vaciar la TLB ya va a todos los que tienen el espacio puesto), `futex`,
  señales (`rt_sigaction`, `rt_sigprocmask`: hoy un fallo cierra el programa sin
  más), `fork` con **copiar al escribir**, `mmap` de archivos, `fstat`, los ELF
  dinámicos y PIE (`ld-musl`), y AVX (XSAVE).
- Esperas con plazo y despertar por señal (etapa 3, pendiente).
- Las tablas de páginas del núcleo que se quedan vacías siguen sin devolverse; las
  de los programas, sí (al acabar).
- Como mucho 48 regiones y 16 archivos abiertos por proceso; `brk` hasta 1 GB y
  `mmap` hasta 1 TB de una vez.

---

## 11. La etapa 5, hecha (24/11/2026): los discos y los archivos

### Las decisiones

- **NVMe, con una cola por procesador.** La cola de administración y, de E/S,
  una por procesador (hasta 8, o las que dé la controladora con «Set Features»):
  cada procesador manda por la suya sin esperar a los demás. Cada cola, de 64
  órdenes, con su mutex (una orden en marcha por cola: lo que hace falta mientras
  el VFS haga una cosa a la vez) y su suceso. Los datos, por DMA a memoria física
  seguida: PRP1, PRP2 o una lista de PRP (una página por cola), hasta 128 KB por
  orden (o lo que diga MDTS). «Flush» al vaciar, si tiene caché de escritura. La
  BAR 0 de 64 bits, donde esté (también por encima de 4 GB: la del ThinkPad y el
  Monovate).
- **AHCI, lo de Hispa-Pentium pasado a 64 bits.** El orden de la norma al
  preparar cada puerto (parar, sus tablas, FRE, limpiar, mirar TFD y, si el disco
  sigue ocupado, el COMRESET con FRE puesto: lo que pedía el ICH7 del Toshiba),
  pedir la controladora a la BIOS (BOHC), sin reiniciarla. READ/WRITE DMA EXT
  (LBA de 48 bits: los discos sin ella no se usan) y FLUSH CACHE EXT, un PRD de
  hasta 128 KB. Con CAP.S64A, las direcciones de 64 bits; sin él, lo que esté por
  encima de 4 GB pasa por una memoria de paso de debajo.
- **Las interrupciones por mensaje (MSI-X y MSI), con plan B.** NVMe usa MSI-X
  (la entrada 0 para todas las colas: la interrupción mira qué colas tienen
  respuesta y despierta a quien espera en ellas); AHCI, MSI. Sin ellas
  (`sinmsix`, `sinmsi`, o si el aparato no las tiene) **se mira la cola**,
  cediendo el procesador. Y con ellas, por si una se pierde: quien espera mira
  primero un momento (las órdenes rápidas no duermen), luego duerme en el suceso
  **con plazo** (20 ms: `Sinc64.espera_plazo`, nuevo, con `Hilos64.duerme_en_hasta`)
  y vuelve a mirar; a los 5 s sin respuesta, la orden falla (y se dice). Las
  patillas INTx y el `_PRT` del AML no hacen falta.
- **La caché de bloques, no una de páginas (aún).** Trozos de 4 KB de cada
  disco, una página física cada uno (el disco los deja ahí por DMA), buscados por
  (disco, bloque) en una tabla de dispersión, y el «reloj» para elegir cuál se
  va. 1/16 de la memoria libre, de 1 a 64 MB. **Escritura diferida**: escribir
  solo cambia la memoria; los bloques sucios van al disco cuando hace falta su
  sitio, al pedirlo fsync, sync o el apagado, y si no, el hilo «vaciador» los
  escribe a los 5 s. La caché de páginas (para `mmap` de archivos) llega con
  musl (etapa 7), encima de esta.
- **Un VFS sencillo y entero.** «/» es la unidad en memoria (un sistema de
  archivos más, de solo leer); cada volumen que se sabe leer se ve como
  «/nvme0p2», «/sata0p1»... (los nombres de Linux sin «n1»: «nvme0» es el espacio
  de nombres 1; el 2 sería «nvme0n2»). Las rutas se limpian antes («.», «..», «//»:
  no hay enlaces) y se busca el montaje más largo que encaje; dentro, sin mirar
  mayúsculas (como FAT y exFAT, también las de Latin-1: «CANCIÓN» = «canción»).
  Un nodo por archivo aunque se abra varias veces; lo abierto (posición y marcas)
  lo comparten los descriptores hechos con `dup`. **Un mutex para todo el VFS**
  (una operación a la vez): sencillo y seguro; los discos y la caché tienen los
  suyos. Cuando haya hilos de POSIX y un navegador, se afinará (etapa 7).
- **FAT32 entero (leer y escribir) y exFAT para leer.** FAT32: nombres largos
  (VFAT, en UTF-16; aquí UTF-8), los cortos con las minúsculas de Windows NT (si
  un nombre cabe en 8.3, corto; si no, el largo y «BASE~N» que no esté), las FAT
  todas iguales, FSInfo al día (se fía de él si está bien; si no, cuenta la FAT),
  borrar con el archivo abierto (como Linux: los cúmulos, al cerrarlo), mover
  carpetas (con su «..»). exFAT, para leer: la suma del sector de arranque y la de
  cada grupo de entradas, las cadenas de la FAT y los archivos seguidos
  («NoFatChain»), ValidDataLength. **La escritura de exFAT queda para la etapa 6**
  (el mapa de bits, las sumas y el «hash» del nombre con la tabla de mayúsculas),
  con las memorias USB, que es cuando hace falta.
- **Nada se escribe sin permiso.** Todos los discos son de solo leer salvo con
  `escribir` en la línea de órdenes (el menú lo pone como entrada aparte, «solo en
  VirtualBox o en discos de prueba»: el plan dice que nunca se escribe en el disco
  interno de un equipo del taller sin permiso). Y las pruebas solo escriben en los
  volúmenes que llevan la marca `HISPA64.PRB`, y a pelo solo en un disco que
  empieza por «HISPA64 CRUDO».
- **Los programas, también desde un disco.** `lanza` acepta una ruta
  («/sata0p1/boot/prog/HOLACAS»): el archivo se lee entero por el VFS y se carga
  igual que los de la unidad en memoria.
- **Las pilas de los hilos, de 32 KB** (eran 16): las llamadas de los archivos
  (VFS, FAT, caché, driver, esperas) son más hondas que las de la etapa 4.
- **La consola, sin quitar las interrupciones mientras se espera** (`toma_atento`):
  con la pantalla del GOP (dibujada letra a letra) y varios programas escribiendo a
  la vez, un procesador podía esperar el cerrojo más de 2 s sin atender el aviso de
  vaciar la TLB, y el núcleo se paraba («un procesador no contesta al aviso de
  vaciar su TLB»); salió en la prueba por UEFI con 4 procesadores de esta etapa. Era
  de la etapa 4.

### Qué hace

Hispa-64 0.5.0. Los módulos nuevos y lo que cambia:

| Archivo | Qué |
|---|---|
| `msi64.cas` | MSI-X (la tabla de su BAR) y MSI (la capacidad de PCI), hacia el procesador que arranca; los vectores de los aparatos, desde 0x50 |
| `nvme64.cas` | NVMe: reiniciar, la cola de administración, Identify (la controladora y los espacios de nombres), una cola de E/S por procesador, Read/Write con PRP y lista de PRP, Flush; la interrupción (MSI-X) o mirar la cola |
| `ahci64.cas` | SATA por AHCI: el traspaso de la BIOS, cada puerto como dice la norma (con el COMRESET), IDENTIFY, READ/WRITE DMA EXT, FLUSH CACHE EXT; MSI o mirar el puerto; la memoria de paso sin S64A |
| `discos64.cas` | los discos y las particiones (GPT con su CRC32 y la copia del final; MBR con las lógicas de la extendida; un volumen sin tabla), la caché de bloques con escritura diferida y el hilo vaciador |
| `nodos64.cas` | lo común: los nodos, los montajes, los errores de Linux, los nombres (UTF-8/UTF-16, sin mirar mayúsculas), la hora del DOS, el mutex del VFS |
| `fat64.cas` | FAT32: montar, buscar, listar, leer, escribir, acortar, crear, borrar, renombrar y mover, carpetas; vaciar (FSInfo) |
| `exfat64.cas` | exFAT, para leer: montar (con la suma), buscar, listar, leer |
| `vfs64.cas` | el VFS: los montajes, las rutas, lo abierto, open/read/write/lseek/getdents64/stat/mkdir/unlink/rmdir/rename/ftruncate/fsync/sync, chdir; cargar un archivo entero (para arrancar un programa de un disco) |
| `prdisc64.cas` | las pruebas de cada arranque (abajo) |
| `sistema.cas` | las llamadas nuevas: 4 stat, 5 fstat, 6 lstat, 17 pread64, 18 pwrite64, 19 readv, 21 access, 32 dup, 33 dup2, 72 fcntl, 74 fsync, 75 fdatasync, 76 truncate, 77 ftruncate, 79 getcwd, 80 chdir, 81 fchdir, 82 rename, 83 mkdir, 84 rmdir, 87 unlink, 162 sync, 217 getdents64, 258 mkdirat, 262 newfstatat, 263 unlinkat (con AT_REMOVEDIR), 264 renameat, 269 faccessat, 292 dup3, 316 renameat2 (sin marcas); open y openat ya con O_CREAT, O_EXCL, O_TRUNC, O_APPEND y O_DIRECTORY; read y write a archivos |
| `procesos.cas` | los descriptores (32) apuntan a lo abierto del VFS y se cierran al salir; la carpeta actual; arrancar un programa por su ruta |
| `sinc64.cas`, `hilos64.cas` | esperar un suceso con plazo (`espera_plazo`, `duerme_en_hasta`) |
| `cerrojo.cas` | `toma_atento` (el cerrojo de la consola) |
| `pci64.cas` | lo de cada aparato para los drivers: la clase, las BAR, la configuración, las capacidades, habilitar DMA |
| `programas/` | `archivos.c` (ARCHIVOS: las pruebas de los archivos desde el anillo 3), `hazdiscos.sh` (los discos de prueba) y `hazexfat.py` (un volumen exFAT, sin mkfs.exfat) |
| `imagen.sh` | el disco, de 128 MB con una partición de pruebas (FAT32, con la marca); el VDI para VirtualBox (qemu-img); las entradas del menú «con escritura» y «sin MSI ni MSI-X» |

Lo que dice (QEMU, q35, multiboot, 4 procesadores, con `escribir`; abreviado):

```
Discos:
NVMe: 00:02.0 1B36:0010, «QEMU NVMe Ctrl» (serie hispa64), versión 1.4
  BAR 0 en 0xFEBF0000; 4 colas de E/S (una por procesador) de 64 órdenes; MSI-X; hasta 128 KB por orden; con caché de escritura
SATA (AHCI): 00:1F.2 8086:2922, AHCI 1.0, 6 puertos, direcciones de 64 bits; MSI; 1 disco
  la caché de bloques: 4037 bloques de 4 KB (15,7 MB), con escritura diferida (a los 5 s, o al vaciar)
  nvme0: «QEMU NVMe Ctrl», 112,0 MB (229376 sectores de 512 octetos), tabla GPT; se puede escribir
    nvme0p1: 40,0 MB, desde el sector 2048, EFI
    nvme0p2: 70,9 MB, desde el sector 83968, datos (Windows: FAT, exFAT o NTFS)
  nvme1: «QEMU NVMe Ctrl», 16,0 MB (32768 sectores de 512 octetos), sin tabla de particiones; se puede escribir
  sata0: «QEMU HARDDISK», 128,0 MB (262144 sectores de 512 octetos), tabla MBR; se puede escribir
    sata0p1: 84,0 MB, desde el sector 2048, FAT32
    sata0p5: 32,0 MB, desde el sector 182272, exFAT o NTFS
Archivos (el VFS): 5 montajes
  /: la unidad en memoria (7 archivos), de solo leer
  /nvme0p1: FAT32 «ESP», 39,3 MB en 80628 cúmulos de 512 octetos, 39,3 MB libres; de leer y escribir
  /nvme0p2: FAT32 «DATOS», 69,8 MB en 143076 cúmulos de 512 octetos, 69,5 MB libres; de leer y escribir
  /sata0p1: FAT32 «SATAFAT», 83,3 MB en 85332 cúmulos de 1 KB, 83,0 MB libres; de leer y escribir
  /sata0p5: exFAT «EXFATPRB», 31,9 MB en 8168 cúmulos de 4 KB; de solo leer
...
La caché: los primeros 4 KB de los 3 discos, iguales por la caché y a pelo
Disco crudo de pruebas: nvme1 (16,0 MB)
  escribir y leer a pelo: 1 sector, 8, 256 (128 KB), 500 (varias órdenes) y los 300 del final: iguales; más allá del final, no
  leerlo entero (de 128 KB en 128 KB): 16 MB en 29 ms (573 MB/s)
  4 hilos a la vez, cada uno en su procesador, 32 lecturas de 128 KB cada uno: bien; colas de NVMe usadas: 4
ARCHIVOS: 4 volúmenes en /: /nvme0p1 /nvme0p2 /sata0p1 /sata0p5
ARCHIVOS en /nvme0p2 (un disco de prueba):
  lee lo del anfitrión: LEEME.TXT, un nombre largo con acentos y PATRON.BIN (300000 octetos), bien
  la prueba en /nvme0p2/Prueba de Hispa-64: crear, escribir, leer, O_APPEND, pwrite, 1,2 MB, huecos, ftruncate, nombres largos, 40 archivos, rename, unlink abierto, *at, chdir, los errores
ARCHIVOS en /sata0p1 (un disco de prueba): ...
ARCHIVOS en /sata0p5 (un disco de prueba):
  lee lo del anfitrión: LEEME.TXT, un nombre largo con acentos y PATRON.BIN (300000 octetos), bien
  de solo leer (EROFS): no se escribe
ARCHIVOS: 3 discos de prueba (2 escritos); todo bien (192 pruebas)
Programa «ARCHIVOS auto»: acaba con el código 0, como debe (en ... ms)
/:
  ARCHIVOS (25960 octetos)
  ...
  nvme0p2/
    HISPA64.PRB (56 octetos)
    LEEME.TXT (51 octetos)
    PATRON.BIN (300000 octetos)
    Carpeta con nombre largo/
      Canción de cuna ñandú.txt (37 octetos)
    Prueba de Hispa-64/
      grande.bin (1234567 octetos)
      ...
Después de los archivos: 0 abiertos, 5 nodos (los de antes: las raíces), el montón como antes
Hispa-64 etapa 5: todo en orden.
Al apagar: 6 bloques sin escribir en la caché; vaciados (sync): 8 escritos
```

La prueba de ARCHIVOS en cada volumen de prueba (92 comprobaciones): mkdir con
nombre largo (y otra vez: EEXIST), crear, escribir y leer, stat, O_APPEND, fstat,
pwrite (sin mover la posición), O_CREAT|O_EXCL, un archivo de 1.234.567 octetos en
trozos de 64 KB con fsync, 100 octetos cambiados en medio con lseek y leído entero,
un hueco (pwrite más allá del final: lo de en medio, a cero), ftruncate más corto y
más largo (a cero), O_TRUNC, truncate por la ruta, un nombre largo con acentos
abierto en mayúsculas («ÁÉÍÓÚ», «Ñ»), 40 archivos con nombre largo en una carpeta
(con cúmulos de 512 octetos crece 8 veces) y 20 borrados, getdents64 (con «.» y
«..»), rename en la misma carpeta, a otra, de una carpeta, de una carpeta a otra
carpeta y vuelta, dentro de sí misma (EINVAL), encima de otro, unlink de uno
abierto (se sigue leyendo), openat, newfstatat (también con AT_EMPTY_PATH),
mkdirat, renameat, unlinkat (con y sin AT_REMOVEDIR), chdir y getcwd, rutas con
«.» y «..», y los errores: ENOENT, EISDIR, ENOTDIR (O_DIRECTORY, pasar por un
archivo, rmdir de un archivo, openat desde la consola), ENOTEMPTY, EINVAL (un
nombre con «:»), ENAMETOOLONG (300 letras), EXDEV (a otro montaje), EROFS (la
unidad en memoria), EBUSY (rmdir de un montaje); dup (comparten la posición), sync,
y ULTIMO.TXT escrito sin fsync, que lo vacía el apagado.

El núcleo ocupa 188 KB (eran 124).

### Cómo se ha probado

`castel/nucleo64/pruebas.sh`: **62 pruebas, todas bien** (eran 53), unos 12 minutos
con el servidor (de ellos, unos 3 son de ensambla.py). Las de la etapa 5, con los
discos de `programas/hazdiscos.sh` (sin montar nada: sgdisk, sfdisk, mkfs.fat,
mtools y `hazexfat.py`, que hace el exFAT porque en el servidor no hay mkfs.exfat y no
se instala nada):

- **q35, 4 procesadores, con `escribir`**: un NVMe de 112 MB con GPT (la EFI y una de
  datos, FAT32 con cúmulos de 512 octetos), un NVMe crudo de 16 MB y un SATA de 128 MB
  con MBR (una FAT32 primaria y una exFAT lógica dentro de una extendida): NVMe con 4
  colas y MSI-X, AHCI con MSI, las particiones con sus tipos, los 5 montajes; la
  caché, igual que a pelo; el disco crudo escrito y leído a pelo (1, 8, 256, 500 y
  los 300 últimos sectores; más allá del final, no) y leído entero (**16 MB en 29 ms,
  unos 560 MB/s en QEMU**), y 4 hilos a la vez, cada uno en su procesador y por su
  cola (las 4 colas usadas); ARCHIVOS: **192 comprobaciones bien** en los tres
  volúmenes de prueba (la prueba entera en las dos FAT32; en la exFAT, leer lo del
  anfitrión y EROFS); «dir /» con el árbol; después, 0 abiertos, los nodos y el
  montón como antes; al apagar, los bloques sin vaciar, al disco; y la etapa 4
  entera, igual;
- **desde el anfitrión**, tras esa vuelta: `fsck.fat -n -v` sin quejas (también el
  resumen de los libres: FSInfo al día) en las dos FAT32 escritas y en la EFI, y
  con mtools lo que ha dejado ARCHIVOS: ULTIMO.TXT (escrito sin fsync: lo vació el
  apagado), adios.txt, el nombre largo con acentos, sub2/Dentro/movido.txt, los 20
  impares de Muchos (y solo ellos), grande.bin octeto a octeto (con sus 100 X),
  hueco.bin de 3 octetos, y que no queda nada de lo borrado;
- **dos discos de 3 TB** (dispersos), por NVMe y por SATA, con una partición desde el
  sector 4.999.999.488 (más allá de 2^32): la prueba entera en las cuatro (365
  comprobaciones), y fsck.fat y lo dejado, bien en las de arriba;
- **sin MSI ni MSI-X** (`sinmsix sinmsi`, `-machine pc` con la AHCI aparte): las tres
  controladoras mirando la cola, y todo igual;
- **sin `escribir`**: todo de solo leer (EROFS en las pruebas, el disco crudo sin
  tocar, nada al apagar) y los tres discos, **iguales octeto a octeto** (md5sum);
- `sinpruebas` con los discos;
- **por UEFI desde SATA** (OVMF, q35, 4 procesadores, la imagen de `imagen.sh` como
  disco SATA): la partición EFI a la vista (BOOTX64.EFI, HISPA64.BIN), HOLACAS
  arrancado desde ella (`programa=/sata0p1/boot/prog/HOLACAS`), la partición de
  pruebas escrita por ARCHIVOS, y fsck.fat bien en las dos.

`SOLO=castel64` (con CASTEL64 y ENSAMB64 el núcleo sale igual, octeto a octeto, y
sus 63 pruebas, bien): **80 bien y 0 mal** (15 minutos). Los módulos nuevos se
llaman con 8 letras como mucho (Discos64, Nodos64, PrDisc64...): CASTEL64 los busca
con los nombres del DOS. `ensambla.py` da los mismos octetos que NASM (hubo que cambiar un `si` de
`lista_apic`: con el núcleo más grande, NASM y ensambla.py elegían distinto un salto
que quedaba justo a 127 octetos; y `mfence`, que no saben ensambla.py ni ENSAMB64, va
en `db`). Las imágenes de la carpeta del taller (el VDI por UEFI en SATA, con 4
procesadores), arrancadas en QEMU hasta «todo en orden».

Lo que QEMU no prueba: los NVMe y las AHCI de verdad (los tiempos de encender un
puerto, los discos que tardan en girar, las controladoras sin S64A, los NVMe con
espacios de nombres de 4 KB, las BAR por encima de 4 GB: el código está, pero en
QEMU todo cae por debajo), los MSI-X de varios vectores (se usa uno) y la velocidad:
en el TCG de QEMU no se llega a 1 GB/s; eso, en el ThinkPad y el Monovate.

### Lo que queda para la etapa 6 (y más adelante)

- **USB de almacenamiento** (con xHCI, etapa 6): un disco más de Discos64 (la clase
  D_USB), con lo mismo encima (particiones, FAT32, exFAT); y entonces **escribir en
  exFAT** (el mapa de bits, las sumas, el «hash» del nombre) y **FAT12/FAT16** (las
  particiones EFI pequeñas y las memorias viejas).
- **Más velocidad**: leer de un tirón los cúmulos seguidos (hoy, de 4 KB en 4 KB por
  la caché), leer por delante, varias órdenes a la vez por cola (NCQ en SATA), un
  vector de MSI-X por cola, y cerrojos más finos en el VFS (con los hilos de POSIX,
  etapa 7).
- **La caché de páginas y `mmap` de archivos** (etapa 7, con musl), los permisos
  (uid, modo), los enlaces, `O_CLOEXEC` de verdad (hoy se acepta y no hace nada),
  `flock`, la hora local en FAT (hoy se escribe la del CMOS, que en QEMU es UTC).
- NTFS y ext4 para leer; TRIM; los discos que se enchufan en marcha; leer CD (ATAPI).
- La consola para escribir órdenes (el intérprete, con la entrada de la etapa 6).

---

## 12. La etapa 6, hecha (24/11/2026): el USB, la entrada y el intérprete

### Las decisiones

- **xHCI de 64 bits, desde cero pero con lo del taller.** Lo de Hispa-Pentium
  (xhci.cas y usb.cas, probados en los equipos de Carlos) pasado a 64 bits y a
  un núcleo con hilos: la BAR 0 donde esté (de 64 bits; con HCCPARAMS1.AC64 =
  0, las páginas de la controladora por debajo de 4 GB); el **traspaso de la
  BIOS** (USBLEGSUP: el semáforo del sistema, hasta 1 s para que la BIOS
  suelte, sus SMI fuera); en los PCH de Intel de las series 7 y 8, los puertos
  pasados del EHCI al xHCI (XUSB2PR y USB3_PSSEN, como Linux: aquí no hay
  EHCI); **varias controladoras** (hasta 4), cada una con su DCBAA, sus anillos
  y sus páginas de borrador; qué puertos son de USB 2 y cuáles de USB 3 (la
  capacidad «Supported Protocol»). Los de **velocidad plena** empiezan con un
  extremo 0 de 64 octetos y se les pone el que digan con **Evaluate Context**;
  tras un fallo, **Reset Endpoint** (si se ha parado) o Stop Endpoint (si se
  pasa el tiempo) y **Set TR Dequeue Pointer**; los **concentradores** (los de
  USB 1.1, 2 y 3: su descriptor 0x29 o 0x2A, SET_HUB_DEPTH, la cadena de ruta,
  el traductor de los de USB 1.x detrás de uno de alta velocidad, MTT); los de
  **USB 3** (SuperSpeed), sin reinicio si el enlace ya está entrenado (y con el
  reinicio «caliente», WPR, si no), con la ráfaga del compañero de su extremo.
- **Las interrupciones por mensaje, con plan B.** El interruptor 0 con MSI-X (o
  MSI): la interrupción recoge el anillo de sucesos (con un cerrojo: también lo
  recoge quien espera) y despierta a quien espera lo suyo (cada extremo, su
  suceso; las órdenes, el de la controladora) y al vigía. Quien espera mira
  primero un momento, luego duerme con plazo (20 ms) y vuelve a mirar; sin
  mensajes («sinmsix sinmsi», o si no los tiene) mira el anillo cediendo el
  procesador, y el vigía, cada 10 ms. Las órdenes, de una en una (un mutex y un
  contexto de entrada por controladora); las transferencias, una a la vez por
  aparato (su mutex) y a la vez entre aparatos.
- **El vigía del USB**: un hilo del núcleo que atiende lo que llega: los informes
  de los teclados, ratones y tabletas (pide otro al llegar cada uno), los
  cambios de los puertos raíz (el suceso de la controladora) y de los
  concentradores (su tubería de interrupción): **enchufar y desenchufar en
  marcha** los aparatos HID y los concentradores con lo que cuelga; y los LED
  del teclado cuando cambian. Las memorias que se enchufan en marcha se
  enumeran, pero el disco no aparece hasta el próximo arranque (Discos64 y el
  VFS aún no saben de discos nuevos en marcha: etapa 7).
- **HID por su descriptor de informe** (Hidesc64, el analizador de Hispa-Pentium
  ampliado: el teclado con sus modificadores y su arreglo de teclas o su mapa de
  bits, los LED, el ratón con rueda y «AC Pan», el puntero absoluto con su
  mínimo y su máximo, los identificadores de informe): con el protocolo de
  informes, que es el que da la rueda y la tableta; el de arranque, solo si el
  descriptor no se entiende. Las teclas, pulsadas y soltadas, a Teclas64 (el
  mapa español); el puntero, a Raton64 (relativo o absoluto, en puntos de la
  pantalla).
- **Las memorias USB, otro disco de Discos64** (la clase D_USB: «usb0»,
  «usb1»...): BOT con SCSI (INQUIRY, TEST UNIT READY con REQUEST SENSE hasta 3 s,
  READ CAPACITY 10 y 16, READ y WRITE 10 o 16, SYNCHRONIZE CACHE), los datos por
  DMA a la página de la caché, hasta 128 KB por orden en TRB que no cruzan 64 KB
  (con el «TD Size»), sectores de 512, 2048 o 4096 octetos; si se atasca,
  quitar la parada o el reinicio de la clase, y otra vez. Encima, lo mismo que
  en los otros discos: MBR o GPT, FAT y exFAT, y de solo leer salvo con
  «escribir».
- **El touchpad I2C-HID, sin ACPICA.** Hispa-64 aún no interpreta el AML, así
  que los PNP0C50 se buscan **recorriendo la DSDT y las SSDT como un árbol**
  (Aml64: Scope, Device, Name, If/Else fuera de los métodos; las expresiones se
  saltan): de cada uno, su `_HID`/`_CID` (texto o EISAID), su `_STA`, las
  plantillas de recursos con el **I2cSerialBusV2** (la dirección, la velocidad,
  7 o 10 bits y la controladora; si dice «NULL», como en el ThinkPad E14, la del
  padre), el GpioInt o Interrupt, el registro del descriptor HID que devuelve
  el `_DSM` y, cuando el `_INI` lo decide por variables del BIOS (el ASUS
  GL553VD: SYNA2393, 06CB2846, **ELAN1200**, FTE1200), **todos los
  candidatos**; la controladora, por su `_ADR` o por su nombre (I2C0 a I2C7), y
  sus SSCN/FMCN si son fijos. En el equipo se prueban los candidatos (dirección
  y registro) hasta que uno conteste con un descriptor HID de verdad (30
  octetos, versión 1.00). Las controladoras, Intel LPSS (DesignWare) con lo
  aprendido en Hispa-Pentium: **a D0 antes**, la **BAR sin poner** con dirección
  (`Pci64.asigna_bar`, como `pci_assign_unassigned_resources` de Linux), el
  **reloj por modelo** (la tabla de intel-lpss-pci). HID sobre I2C: SET_POWER,
  RESET, el descriptor de informe con Hidesc64, el touchpad en modo ratón, y se
  lee cada 10 ms (un hilo; el GpioInt aún no se usa). **Cuando llegue ACPICA**
  (etapa 11), rellenará la misma ficha evaluando de verdad `_STA`, `_INI`,
  `_CRS`, `_DSM`, SSCN/FMCN y `_PS0`, con un solo candidato; I2chid64 no
  cambia.
- **exFAT para escribir, y FAT12 y FAT16.** exFAT: el mapa de bits de los
  cúmulos, la tabla de mayúsculas (comprobada con su suma; con ella se
  comparan los nombres y se calcula el NameHash), SetChecksum rehecho en cada
  cambio, los archivos seguidos (NoFatChain) mientras se pueda y si no, a
  cadena de FAT (como Windows), ValidDataLength y DataLength, las horas con los
  10 ms y la zona, **VolumeDirty** al empezar a escribir y limpio al vaciar o
  apagar, PercentInUse; de solo leer si algo no se entiende (TexFAT, otra
  revisión). FAT12 y FAT16: la clase por el número de cúmulos (la norma), las
  entradas de 12 bits que cruzan sectores, la raíz fija (llena: ENOSPC).
- **El teclado, con el mapa español, en un solo sitio** (Teclas64): los códigos
  PS/2 (el juego 1, con 0xE0) se pasan a los usos HID de la página 7, y desde
  ahí todo va igual para el teclado del portátil y para uno USB: la ñ y la ç,
  los acentos muertos (´ ` ¨ ^, y con espacio, el acento solo), AltGr (| @ # ~ €
  ¬ [ ] { } \), Bloq Mayús, Bloq Num con el teclado numérico, Ctrl y Alt, y las
  flechas, Inicio, Fin, Supr, RePág, AvPág y F1–F12 como secuencias de VT100. La
  interrupción (o el vigía del USB) solo deja el código en una cola; lo demás,
  el hilo «teclado» (el eco escribe en la pantalla: no en una interrupción). La
  repetición del USB, aquí (a los 500 ms, cada 33 ms); los LED, al 8042 y a los
  teclados USB.
- **La consola es una terminal** (Tty64): modo canónico con eco y edición de
  línea (Retroceso borra una letra UTF-8 entera, Ctrl+U, las flechas, Inicio y
  Fin, 16 líneas de historia, Ctrl+D fin de archivo, Ctrl+C) y modo crudo
  (VMIN, VTIME); `read(0)` duerme el hilo; los `ioctl` de las terminales que
  usan musl e `isatty`; `/dev/tty` y `/dev/console`; la salida pasa de UTF-8 a
  la página 437 de la casa y entiende lo mínimo de VT100 (colores, borrar). Sin
  señales de verdad (etapa 7), **Ctrl+C cierra con 130** los programas del
  grupo de la consola al volver de su llamada al sistema.
- **Arrancar programas desde un programa, como Linux**: `fork` (y `vfork`, y
  `clone` con solo SIGCHLD) **copiando** las páginas que el padre tiene puestas
  (copiar al escribir, con musl: etapa 7), `execve` por el VFS (la unidad en
  memoria o cualquier disco), `wait4` con el estado de Linux, y los huérfanos y
  los zombis, recogidos. Con eso, **el intérprete de órdenes es un programa
  más** (ORDENES, en C, `-nostdlib`), en el anillo 3, en español.
- **El plazo del aviso de vaciar la TLB, 10 s** (eran 2): escribir en la consola
  se hace con las interrupciones quitadas, y por UEFI con «pc» y 2 procesadores,
  con el servidor cargado, la pantalla del GOP dibujada por el TCG pasó de 2 s
  una vez (MALO explicando sus fallos). En un equipo de verdad (escritura
  combinada) son milisegundos; dibujar sin quitar las interrupciones, más
  adelante (con las ventanas, etapa 8).

### Qué hace

Hispa-64 0.6.0. Los módulos nuevos y lo que cambia:

| Archivo | Qué |
|---|---|
| `xhci64.cas` | las controladoras xHCI: tomarlas (el traspaso de la BIOS, los puertos de Intel, D0 y la BAR sin poner), los anillos, el de sucesos con su interrupción (MSI-X o MSI) o mirándolo, las órdenes (Enable Slot, Address Device, Evaluate Context, Configure Endpoint, Reset Endpoint, Stop Endpoint, Set TR Dequeue Pointer, Disable Slot), los puertos raíz, las transferencias de control, a granel y de interrupción |
| `usb64.cas` | los aparatos: enumerar (con el reintento), los descriptores (también el compañero de USB 3 y los textos), los concentradores (los de USB 2 y 3), quitar; las transferencias para las clases |
| `usbhid64.cas` | los teclados, ratones y tabletas USB: por su descriptor (Hidesc64) o por el protocolo de arranque; las teclas a Teclas64, el puntero a Raton64, los LED |
| `usbalm64.cas` | las memorias USB: BOT y SCSI, para Discos64 |
| `usbvigia.cas` | el USB entero al arrancar y el hilo vigía (los informes, enchufar y desenchufar, los concentradores, los LED) |
| `raton64.cas` | el puntero: la posición en la pantalla, los botones, la rueda |
| `hidesc64.cas` | el analizador de descriptores de informe HID (y lo de HID sobre I2C) |
| `prhid64.cas` | sus pruebas de cada arranque (los descriptores de verdad de `i2chid/reales.txt`, `sint.txt` y `qemu.txt`, metidos dentro por `i2chid/hazdatos.py`); `prueba=hid`, todo lo que dice |
| `aml64.cas` | los PNP0C50 en la DSDT y las SSDT, sin interpretar el AML |
| `i2c64.cas`, `i2chid64.cas` | las controladoras I2C Intel LPSS y el HID sobre I2C de Microsoft (el touchpad, cada 10 ms) |
| `teclas64.cas`, `tty64.cas` | el teclado con el mapa español y la consola como terminal |
| `discos64.cas` | la clase D_USB |
| `exfat64.cas`, `fat64.cas`, `nodos64.cas`, `vfs64.cas` | exFAT para escribir; FAT12 y FAT16 |
| `pci64.cas` | `pon_d0`, `asigna_bar` (una BAR sin poner, con dirección), `relee_bar` |
| `procesos.cas`, `espacios.cas`, `sistema.cas` | la tabla de procesos, `fork` (`copia_espacio`), `execve`, `wait4`; las llamadas nuevas: 7 poll, 13 rt_sigaction y 14 rt_sigprocmask (lo mínimo), 23 select, 56 clone (como fork), 57 fork, 58 vfork, 59 execve, 61 wait4, 109 setpgid, 110 getppid, 111 getpgrp, 112 setsid, 121 getpgid, 124 getsid, 270 pselect6, 271 ppoll; `read`, `ioctl` y `fstat` de la consola por Tty64; O_CLOEXEC de verdad |
| `apic64.cas` | la interrupción del teclado PS/2, a Teclas64 |
| `smp64.cas` | el plazo del aviso de la TLB, 10 s |
| `hispa64.cas` | el USB antes de la caché de bloques (las memorias son discos), el I2C-HID, la consola; `prueba=raton`; «ordenes» arranca el intérprete; sin «apagar», el intérprete en la consola |
| `programas/` | `ordenes.c` (el intérprete), `hijos.c` (HIJOS: fork, execve, wait4, poll, select, la terminal), `hazusb.sh` (las memorias de prueba), `teclas.py` (un texto a teclas de `sendkey` con el mapa español), `miraexfat.py` (el comprobador de exFAT del anfitrión); `hazdiscos.sh` con FAT12, FAT16 y otra exFAT |
| `i2chid/` | los datos de prueba: descriptores de touchpads de verdad, la SSDT de prueba y las tablas del ASUS GL553VD (partidas en SSDT de menos de 64 KB por `comossdt.py`, que QEMU no admite más) |
| `imagen.sh` | el menú de la 0.6.0: «sinusb», «sini2c», «prueba=raton», el USB sin MSI |

Lo que dice (QEMU, q35, 4 procesadores, con `escribir`; abreviado):

```
xHCI: 00:02.0 1B36:000D en 0xFEBF0000
  versión 1.00, 8 puertos (4 de USB 2 y 4 de USB 3), 63 ranuras, contextos de 32 octetos, direcciones de 64 bits; MSI-X
USB: 6 aparatos (1 concentradores) en 435 ms
  1-3: 46F4:0001 «QEMU USB HARDDRIVE», USB 3.0, velocidad SuperSpeed (5 Gbit/s), ranura 1, paquete del extremo 0: 512; almacenamiento (SCSI, Bulk-Only)
  1-4: 46F4:0001 «QEMU USB HARDDRIVE», USB 3.0, velocidad SuperSpeed (5 Gbit/s), ranura 2, paquete del extremo 0: 512; almacenamiento (SCSI, Bulk-Only)
  1-5: 0409:55AA «QEMU USB Hub», USB 1.1, velocidad plena (12 Mbit/s), ranura 3, paquete del extremo 0: 8; concentrador de 8 puertos
  1-5.1: 0627:0001 «QEMU USB Keyboard», USB 2.0, velocidad plena (12 Mbit/s), ranura 4, paquete del extremo 0: 8; HID: teclado (por su descriptor, con LED)
  1-5.2: 0627:0001 «QEMU USB Tablet», USB 2.0, velocidad plena (12 Mbit/s), ranura 5, paquete del extremo 0: 8; HID: tableta (por su descriptor, de 0 a 32767)
  1-6: 0627:0001 «QEMU USB Mouse», USB 2.0, velocidad alta (480 Mbit/s), ranura 6, paquete del extremo 0: 64; HID: ratón (por su descriptor, con rueda)
  usb0: «QEMU    QEMU HARDDISK» en 1-3, 64,0 MB (131072 sectores de 512 octetos)
  usb1: «QEMU    QEMU HARDDISK» en 1-4, 48,0 MB (98304 sectores de 512 octetos)
  ...
  usb0: «QEMU    QEMU HARDDISK», 64,0 MB (131072 sectores de 512 octetos), tabla MBR; se puede escribir
    usb0p1: 63,0 MB, desde el sector 2048, FAT32
  usb1: «QEMU    QEMU HARDDISK», 48,0 MB (98304 sectores de 512 octetos), sin tabla: un volumen entero; se puede escribir
  /usb1: exFAT «USBEXFAT», 47,8 MB en 12260 cúmulos de 4 KB, 47,4 MB libres; de leer y escribir
  /usb0p1: FAT32 «USBFAT», 62,0 MB en 127006 cúmulos de 512 octetos, 61,7 MB libres; de leer y escribir
...
ARCHIVOS: 2 discos de prueba (2 escritos); todo bien (183 pruebas)
...
Hispa-64 etapa 6: todo en orden.
/> ver
...
```

Con `prueba=raton` (10 s mirando el puntero) y el monitor de QEMU:

```
Puntero: (522, 404), botones 0, rueda 0 (ratón)
Puntero: (517, 407), botones 0, rueda 1 (ratón)
Puntero: (517, 407), botones 1, rueda 1 (ratón)
Puntero: (511, 191), botones 0, rueda 0 (tableta)
USB: se ha enchufado 1-6: 0627:0001 «QEMU USB Keyboard», USB 2.0, velocidad alta (480 Mbit/s), ...
USB: se ha desenchufado 1-5.3
```

Por UEFI, OVMF deja la BAR del xHCI por encima de 4 GB («xHCI: 00:02.0 1B36:000D en
0xC000004000 (por encima de 4 GB)»), y el núcleo la usa ahí, después de que el
firmware haya usado él el teclado y la memoria.

En las tablas de verdad del ASUS GL553VD, Aml64 encuentra `\_SB.PCI0.I2C0.TPD0`
(en la I2C 00:15.0, por su `_ADR`) con sus candidatos según el BIOS (SYNA2393 en
0x20, 06CB2846, **ELAN1200 en 0x15 a 400 kHz con el descriptor en 0x0001** y
FTE1200) y `\_SB.PCI0.I2C1.TPL1` (la pantalla táctil); «nada sin entender» en sus
14 tablas, con 224 dispositivos.

### Cómo se ha probado

El núcleo ocupa 304 KB (eran 188).

`castel/nucleo64/pruebas.sh`: **96 pruebas, todas bien** (eran 62), unos 23
minutos con el servidor (de ellos, unos 8 son de `ensambla.py`, que con el núcleo
de 83.000 líneas de ensamblador va despacio: da los mismos octetos que NASM). Las
de la etapa 6:

- **el USB** (q35, 4 procesadores, con `escribir`): un concentrador de USB 1.1 con
  el teclado y la tableta detrás (velocidad plena: el extremo 0 de 8 octetos, con
  Evaluate Context), el ratón de alta velocidad y **dos memorias de USB 3**
  (SuperSpeed) hechas por `programas/hazusb.sh`: una con MBR y FAT32 y otra exFAT
  sin tabla; las seis líneas de la lista, los HID por su descriptor (el teclado con
  LED, el ratón con rueda, la tableta de 0 a 32767), los dos discos con sus
  montajes, **ARCHIVOS entero en las dos** (191 comprobaciones) y la etapa 4 igual;
  **desde el anfitrión**, la FAT32 sin quejas de fsck.fat y lo dejado por ARCHIVOS
  con mtools, y el exFAT bien con `miraexfat.py` y lo dejado;
- **dos controladoras a la vez**: el nec-usb-xhci con **MSI** (sin MSI-X) y el
  qemu-xhci **mirando la cola**, una memoria en cada una, escritas por ARCHIVOS;
- **con el monitor de QEMU** (`prueba=raton`): el ratón (lo movido, la rueda y el
  clic, exactos) y las teclas USB; la tableta detrás del concentrador, movida por
  QMP (`input-send-event`: el `mouse_move` del monitor solo mueve en relativo) al
  medio, con el botón derecho, y a la esquina; **enchufar y desenchufar en
  marcha** un teclado (su tecla llega), un ratón detrás del concentrador, y quitar
  el concentrador con lo que cuelga; y `sinusb`;
- **por UEFI**: OVMF deja la BAR del xHCI por encima de 4 GB (0xC000004000) y el
  núcleo la usa ahí, con el teclado, la tableta y la memoria que ha usado antes el
  firmware;
- **el HID y el I2C-HID** (sin controladora I2C en QEMU): los 16 descriptores de
  prueba analizados en cada arranque (los touchpads de verdad de Dell, ELAN y
  Synaptics, la pantalla táctil del XPS 9360, los de QEMU, un ratón con AC Pan y
  un teclado NKRO hechos a mano; sus informes, los descriptores HID de I2C y las
  órdenes), la SSDT de prueba (el ELAN1200 y el Synaptics de su plantilla), la del
  ThinkPad E14 con «NULL», **las tablas de verdad del ASUS GL553VD** (el ELAN1200 en
  0x15 con el descriptor en 0x0001, en la I2C0), `sini2c` y una **BAR sin poner**
  (la del aparato `edu` de QEMU puesta a 0: `asigna_bar` le da 0xFBF00000 y se lee
  bien ahí);
- **HIJOS** con 1, 2, 4 y 8 procesadores (49 comprobaciones: fork, la copia de la
  memoria, execve con argv y envp, wait4 con WNOHANG y -ECHILD, un hijo que falla
  con su señal, los huérfanos, poll y select con plazo, isatty, TCGETS/TCSETS,
  TIOCGWINSZ, O_CLOEXEC); después, «0 procesos, 0 en la tabla, 0 espacios»;
- **el intérprete con las teclas del monitor de QEMU** (343 teclas PS/2, de
  `programas/teclas.py` con el mapa español, en unos 30 s): ver, dir, cd a un disco
  FAT32, mkdir, `echo ... > archivo`, type, cp, EROFS en «/», `HOLA uno dos`,
  `CUENTA 3` (dice el código 30), una orden que no está, «canción ñandú pingüino
  àlex a@b#c», Bloq Mayús, Retroceso, las flechas y la historia, Ctrl+U, Ctrl+C (en
  el indicador y a un programa), Ctrl+D y `salir`, que apaga; desde el anfitrión,
  el archivo copiado y fsck.fat; y **lo mismo con un teclado USB** (q35 sin 8042);
- lo de las etapas 0 a 5, igual (los discos de prueba de la etapa 5 ahora llevan
  además FAT12, dos FAT16 y otra exFAT: **670 comprobaciones de ARCHIVOS** en 7
  volúmenes, fsck.fat en las FAT y miraexfat.py en las exFAT).

Con CASTEL64 y ENSAMB64, el núcleo sale igual, octeto a octeto (el ensamblador y
el binario de 311.296 octetos). La imagen de la carpeta del taller (el disco por
UEFI en SATA, con 4 procesadores y un teclado USB), arrancada en QEMU hasta
«todo en orden», con `ver` y `salir` escritos en el intérprete por el teclado USB.

Lo que QEMU no prueba: **el touchpad I2C de verdad** (QEMU no tiene controladora
DesignWare ni HID sobre I2C: las transferencias, el reloj de cada modelo, los
candidatos del ASUS, el modo ratón; el código está y lo primero que hay que mirar
en el taller es qué dice «I2C-HID:»), el traspaso de la BIOS de verdad, los puertos
de Intel de las series 7 y 8, las controladoras sin direcciones de 64 bits, los
concentradores de USB 2 con traductor (MTT) y los de USB 3 (QEMU solo tiene uno de
USB 1.1), las memorias que tardan en estar listas o de sectores de 4 KB, y los
teclados de los portátiles (cada uno con lo suyo).

### Lo que queda para la etapa 7 (y más adelante)

- **Señales de verdad** (rt_sigaction con sus funciones, kill, Ctrl+C a un
  programa que no llama al sistema), **copiar al escribir** en `fork`, `clone` con
  hilos de POSIX, `futex`: con musl.
- **Los discos que se enchufan en marcha** (una memoria USB después de arrancar:
  hoy se enumera, pero el disco aparece en el próximo arranque) y quitarlos con
  seguridad.
- **ACPICA** (etapa 11): el touchpad con su `_STA`, `_INI`, `_CRS` y `_DSM`
  evaluados (un candidato, no varios), su GpioInt (hoy se lee cada 10 ms), `_PS0`.
- La consola: un cursor visible, borrar la pantalla de verdad (hoy sube el texto),
  las líneas más anchas que la pantalla, dibujar sin quitar las interrupciones.
- Más USB: UAS, los isócronos (sonido, cámaras), varios informes pedidos a la vez,
  Bluetooth, el adaptador de red RTL8153 (etapa 9), el i8042 de los portátiles que
  tienen un «touchpad PS/2» (hoy solo el teclado).
- Los nombres de más de 255 octetos en UTF-8 (exFAT y FAT admiten 255 letras
  UTF-16).

---

## 13. La etapa 7, hecha (24/11/2026): musl y POSIX

### Las decisiones

- **musl tal cual, sin instalar nada.** La 1.2.6 (la estable de musl.libc.org,
  con su firma comprobada) está entera en `terceros/musl/`, con los tres
  parches de seguridad que manda poner su página (CVE-2026-107891 de
  `regcomp`, CVE-2026-40200 de `qsort` y CVE-2026-6042 de `iconv`) y su
  LEEME. Se compila con el gcc del sistema, para x86-64, en `build/musl-obj`
  (`make -j1`), y se instala en `build/musl`: las cabeceras, `libc.a`,
  `libc.so` (que es también el cargador dinámico) y el guion `musl-gcc` de musl,
  que llama al gcc del sistema con lo de ese árbol. Ese es el **compilador
  cruzado** de Hispa-64 desde el servidor: como las llamadas al sistema son las
  de Linux x86-64 (2.6), musl no se ha tenido que tocar ni en una línea. Lo
  hace `programas/hazmusl.sh` (solo la primera vez: unos minutos).
- **Lua 5.5.1**, la herramienta de verdad: MIT, pequeña, en C portable, y
  útil (un lenguaje para guiones). En `terceros/lua/`, sin cambiar nada, se
  compila con `make linux` y `musl-gcc` (estática) y corre desde ORDENES, con
  un guion o a mano. BusyBox no: es GPL-2.0 *solo*, y Hispa es «2.0 o
  posterior»; toybox (0BSD) es la candidata para las órdenes de siempre.
- **Los hilos, en el hilo del núcleo** (Hilos64.Hilo): su número (el tid, de
  la misma cuenta que los procesos, como en Linux), sus registros, su FS, su
  máscara de señales, sus pendientes, su pila de señales, `CLONE_CHILD_CLEARTID`
  y la lista robusta. El proceso tiene la lista de sus hilos (64 como mucho) y
  lo compartido: el espacio, los archivos, las acciones de las señales y las
  pendientes de todo el proceso. `exit` acaba el hilo; el último acaba el
  proceso; `exit_group` (o una señal que lo cierra) marca el proceso y cada
  hilo acaba al volver al anillo 3 o de su llamada (a los que duermen se les
  despierta; a los que corren en otro procesador, un aviso por IPI). `execve`
  con varios hilos deja solo el que la hace (como Linux).
- **Las señales, donde las mira Linux: al volver al anillo 3.** Al acabar
  cada llamada al sistema (`antes_de_volver`) y **al volver de cada
  interrupción** que llegó en el anillo 3: la salida común de las entradas
  (`E64_salida`) guarda los quince registros en un marco como el de `syscall` y
  llama a `al_volver`. Así un `kill` o Ctrl+C le llegan también a un programa
  que no llama nunca al sistema: en el siguiente tic (o en el aviso de otro
  procesador). El marco que se pone en la pila del programa es el de Linux
  (`rt_sigframe`: `pretcode` = `sa_restorer`, el `ucontext` con los registros
  en el orden de `mcontext_t`, la máscara de antes y el puntero a los de x87,
  SSE y AVX, y `siginfo`), alineado como lo pide el ABI; `rt_sigreturn` lo
  deshace y vuelve con `iretq` (para poner también RCX y R11), comprobando
  antes lo que podría hacer fallar al núcleo (RIP fuera de lo suyo, las
  banderas, MXCSR y la cabecera de xsave). Una pendiente de cada número (las
  de tiempo real no se encolan aún).
- **Las esperas se interrumpen.** El hilo que duerme en una espera de las que
  se pueden interrumpir (la consola, `wait4`, un futex, una tubería, dormir,
  `pause`, `sigsuspend`, `poll`) lo dice en `interrumpible`; quien manda la
  señal lo saca de ella y lo despierta. La llamada devuelve ERESTARTSYS (o
  ERESTARTNOHAND), que se vuelve EINTR si se entra en una función sin
  `SA_RESTART`, o se repite (RIP − 2 y RAX = el número) si no hay función o la
  tiene; `nanosleep` da EINTR y lo que quedaba.
- **Los fallos del procesador son señales.** Si el programa tiene una función
  para SIGSEGV, SIGFPE, SIGILL, SIGBUS o SIGTRAP (y no la bloquea), el fallo se
  le entrega al volver (con `si_addr` y `si_code`: SEGV_MAPERR o SEGV_ACCERR,
  FPE_INTDIV, ILL_ILLOPN...); si no, se explica en español y se cierra, como
  desde la etapa 4. Sin función, lo que hace cada señal es lo de Linux (cerrar,
  o nada para SIGCHLD, SIGURG, SIGWINCH y SIGCONT); sin control de trabajos aún,
  SIGSTOP y compañía no paran.
- **futex**, en 64 cubos: la clave es el espacio y la dirección
  (`FUTEX_PRIVATE_FLAG`, lo de siempre en musl) o la dirección física (entre
  procesos); lo de cada uno que espera está en su pila mientras espera, y leer
  el valor y ponerse a esperar se hace con el cerrojo del cubo: un
  `FUTEX_WAKE` no se puede perder entre medias. WAIT, WAKE, WAIT_BITSET,
  WAKE_BITSET, REQUEUE y CMP_REQUEUE, con plazo.
- **fork con copiar al escribir**: `copia_espacio` comparte las páginas, de
  solo leer y con la marca COPIAR (el bit 9 de la entrada), y una tabla de 4
  octetos por marco de la RAM dice cuántos más tienen cada uno (se pide de una
  vez al preparar los procesos: 128 KB con 128 MB). Al escribir (el fallo de
  página, o el núcleo al escribir en la memoria del programa), `rompe_copia`:
  si ya no lo tiene nadie más, el mismo marco de escribir; si no, una copia.
  Soltar una página solo devuelve el marco cuando es el último. Las de
  `MAP_SHARED` llevan la marca COMPARTIDA (el bit 10) y se comparten de
  verdad. La memoria de cada proceso lleva un **mutex** (los fallos de página
  de varios hilos a la vez, `mmap`, `munmap`, `mprotect`, `brk`, `fork`, y las
  copias del núcleo desde y hacia el programa, para que nadie quite una página
  mientras se copia).
- **`clone` con `CLONE_VM` y `CLONE_VFORK`** (el `posix_spawn` de musl, y con
  él `system` y `popen`): el hijo usa el espacio del padre («prestado») y el
  padre espera a que el hijo haga `execve` o acabe; ninguno de los dos lo
  deshace.
- **`mmap` de un archivo copia lo que tiene**, al hacer el `mmap`: con
  `MAP_PRIVATE` es justo lo que pide POSIX, y con `MAP_SHARED` de solo leer
  también, mientras nadie escriba el archivo a la vez; lo de después del final,
  a cero. `MAP_SHARED` de escribir (que lo escrito vaya al archivo) pide la
  caché de páginas compartida con el VFS: aún no (da ENODEV; EACCES si el
  archivo no está abierto para escribir).
- **ELF de posición libre y dinámicos**: un PIE va en 0x555555554000 y su
  cargador (PT_INTERP) en 0x7F0000000000 (sin direcciones al azar aún); se
  empieza en el cargador, con AT_BASE, AT_ENTRY (la del programa), AT_PHDR,
  AT_HWCAP y AT_MINSIGSTKSZ en el vector auxiliar. El cargador de musl es
  `/lib/ld-musl-x86_64.so.1`: en Hispa-64, **/lib, /usr y /bin son la unidad en
  memoria** (donde están los módulos del cargador), y ahí busca también ld-musl
  las bibliotecas (`/lib`, `/usr/lib`), que carga con `open` y `mmap`.
- **AVX con xsave**: si el procesador tiene XSAVE y AVX, OSXSAVE y XCR0 con x87,
  SSE, AVX (y los tres de AVX-512 si están); cada hilo guarda lo que diga
  CPUID 0xD (832 octetos con AVX), alineado a 64, y la señal lo lleva entero en
  su marco. Sin AVX, `fxsave` como antes.
- **Las tuberías** (Tubos64): 64 KB por tubería, PIPE_BUF (4096) de una vez,
  O_NONBLOCK, EPIPE y SIGPIPE, el fin de archivo cuando ya no hay quien
  escriba, contadas por cada descriptor de cada extremo (dup, fork).
- **epoll** (lo que usan los navegadores y casi todo, 2.8): un descriptor
  con la lista de lo que se mira (hasta 64: el descriptor, lo que se pide y el
  dato del programa); `epoll_wait` lo mira como `poll` y duerme como `poll`
  (de 10 en 10 ms si hay tuberías, o hasta que haya teclas; una señal lo
  despierta). EPOLLONESHOT de verdad; EPOLLET, como si no (avisa mientras esté
  listo: no se pierde nada, pero despierta más veces).
- **getrandom**: RDRAND (con sus reintentos), o RDSEED, o, si no hay ninguna
  (el TCG de QEMU con «pc»), lo del TSC mezclado: entonces **no** es azar de
  criptografía, y así se dice aquí.

### Qué hace

Hispa-64 0.7.0. Los módulos nuevos y lo que cambia:

| Archivo | Qué |
|---|---|
| `procesos.cas` | los hilos (`crea_hilo`, `arranca_clon`, `sal_hilo`, `sal`, `solo_este`), las señales (mandarlas a un proceso o a un hilo, `kill`, `tgkill`, Ctrl+C; sacarlas, entregarlas con su marco, lo que hacen sin función, `rt_sigreturn`; las esperas que se interrumpen), los fallos que son señales, el fallo de página con copiar al escribir y el mutex de la memoria, `fork` con copiar al escribir y con el espacio prestado (`CLONE_VFORK`), el cargador de ELF con PIE y PT_INTERP, xsave |
| `futex64.cas` | `futex`: las esperas por clave, en 64 cubos |
| `tubos64.cas` | `pipe` y `pipe2` |
| (en `sistema.cas`) | `epoll` |
| `espacios.cas` | copiar al escribir (COPIAR, COMPARTIDA, la tabla de compartidos, `rompe_copia`, `suelta_marco_u`), `mapea_u_tal` |
| `entradas.cas` | `al_volver` en la salida de las interrupciones al anillo 3; la vuelta de `syscall` por `iretq` (rt_sigreturn); XSAVE, XCR0 y lo de CPUID para AT_HWCAP |
| `hilos64.cas` | lo de POSIX de cada hilo; `interrumpe` (sacarlo de su espera, o avisar a su procesador) |
| `sistema.cas` | las llamadas nuevas (abajo), `mmap` de archivos y `MAP_SHARED`, `madvise`, las tuberías en `read`, `write`, `poll`, `select`, `fcntl`, `fstat`; Procesos.`antes_de_volver` al acabar cada llamada |
| `tty64.cas` | la espera de la consola se interrumpe con una señal (SA_RESTART) |
| `vfs64.cas`, `unidad64.cas` | /lib, /usr y /bin, la unidad; la unidad, hasta 64 archivos (y 64 módulos del cargador) |
| `prmusl64.cas` | las pruebas de cada arranque con los programas de musl (abajo); «sinmusl» las quita |
| `programas/hazmusl.sh` | musl y Lua en `build/`; los programas de `programas/musl/` |
| `programas/musl/` | `mhola.c`, `libsaludo.c`, `mhilos.c`, `msenales.c`, `mprocesos.c`, `mmapa.c`, `mbucle.c`, `prueba.lua` y `comun.h` |
| `terceros/musl/`, `terceros/lua/` | musl 1.2.6 y Lua 5.5.1, con sus licencias (MIT) y su LEEME |

Las llamadas nuevas: 13 `rt_sigaction` y 14 `rt_sigprocmask` de verdad, 15
`rt_sigreturn`, 22 `pipe`, 25 `mremap` (ENOMEM: musl copia), 28 `madvise`, 34
`pause`, 56 `clone` (hilos; `CLONE_VM` con `CLONE_VFORK`; como `fork`), 60
`exit` (el hilo), 62 `kill`, 95 `umask`, 96 `gettimeofday`, 97 `getrlimit`,
98 `getrusage`, 99 `sysinfo`, 127 `rt_sigpending`, 130 `rt_sigsuspend`, 131
`sigaltstack`, 160 `setrlimit`, 186 `gettid` (el del hilo), 200 `tkill`, 201
`time`, 202 `futex`, 203 y 204 `sched_setaffinity` y `sched_getaffinity`, 218
`set_tid_address` (del hilo), 229 `clock_getres`, 230 `clock_nanosleep`, 234
`tgkill`, 273 y 274 `set_robust_list` y `get_robust_list`, 293 `pipe2`, 302
`prlimit64`, 318 `getrandom`; 213 `epoll_create`, 291 `epoll_create1`, 233
`epoll_ctl`, 232 `epoll_wait`, 281 `epoll_pwait`; `clock_gettime` con los relojes de CPU; y 324,
334, 435 y 439 (`membarrier`, `rseq`, `clone3`, `faccessat2`) dan ENOSYS sin
decirlo (musl se las arregla sin ellas).

Lo que dice (QEMU, q35, por UEFI, 4 procesadores, `-cpu max`; abreviado: de los
dinámicos, solo lo que cambia):

```
musl: los programas de C de hoy, con la biblioteca de C de musl (estáticos, dinámicos con ld-musl y PIE)
  AVX: sí (xsave, XCR0 = 7, 832 octetos por hilo); RDRAND: sí; RDSEED: sí
MHOLA: hola desde musl (printf); estático
  uname: Hispa-64 0.7.0 x86_64 (etapa 7 (musl y POSIX))
  procesadores (sched_getaffinity): 4
  (esto va por stderr)
MHOLA: todo bien (19 pruebas)
Programa «MHOLA uno dos»: acaba con el código 0, como debe (en 747 ms)
MHOLA: hola desde musl (printf); dinámico: ld-musl en 0x7f0000000000, el programa en 0x555555554000
  libsaludo.so: hola (42)
MHOLA: todo bien (20 pruebas)
MHOLA: hola desde musl (printf); estático de posición libre, en 0x555555554000
MHOLA: todo bien (19 pruebas)
MHILOS: los hilos de POSIX de musl; estático
  8 hilos que suman 2000 veces con un mutex: 16000 (exacto); tids de 77 a 84
  un productor y 8 consumidores (pthread_cond_wait, signal y broadcast): la suma, 2001000 (exacta)
  pthread_cond_timedwait de 50 ms: ETIMEDOUT en 55 ms
  AVX: 8 hilos con ymm0 a ymm7 llenos, cediendo el procesador 40 veces: intactos
MHILOS: todo bien (26 pruebas)
MHILOS: los hilos de POSIX de musl; dinámico: ld-musl en 0x7f0000000000, el programa en 0x555555554000
  8 hilos que suman 2000 veces con un mutex: 16000 (exacto); tids de 111 a 118
  pthread_cond_timedwait de 50 ms: ETIMEDOUT en 51 ms
MHILOS: todo bien (26 pruebas)
MSENALES: las señales de verdad; estático
  raise y kill: la función de la señal, con su siginfo; la coma flotante, intacta
  sigprocmask: bloqueada no llega (sigpending la tiene); al desbloquearla, llega
  los fallos (SIGSEGV en 0x10 y en su código, SIGFPE, SIGILL), atendidos y salidos con siglongjmp
  sigaltstack y SA_ONSTACK: la función corre en la pila de las señales
  read de una tubería interrumpido: con SA_RESTART se repite; sin él, EINTR
  SIGCHLD al acabar un hijo: CLD_EXITED, con su pid y su código
  un hijo que no llama al sistema, cerrado con SIGTERM y con SIGINT
  SIG_IGN; SIGPIPE: ignorada, EPIPE; sin ignorarla, cierra el proceso
  pthread_kill (tgkill): la señal la atiende el hilo 153
MSENALES: todo bien (26 pruebas)
MSENALES: las señales de verdad; dinámico: ld-musl en 0x7f0000000000, el programa en 0x555555554000
  pthread_kill (tgkill): la señal la atiende el hilo 163
MSENALES: todo bien (26 pruebas)
MPROCESOS: fork, execve, wait, tuberías y posix_spawn; estático
  fork con 16 MB: el hijo los lee y los cambia (copiar al escribir); el padre no ve sus cambios, ni el hijo los del padre
  100 KB por una tubería (y la suma de vuelta por otra, y el fin de archivo)
  epoll de una tubería: el plazo, EPOLLIN con su dato, EPOLLONESHOT, EPOLLHUP, EPOLL_CTL_DEL
  fork, dup2 y execve de /MHOLA: «MHOLA (por una tubería): estático, argc 2»
  posix_spawn (clone con CLONE_VM y CLONE_VFORK) de /MHOLA, con su salida por una tubería; y de uno que no está, ENOENT
  (el hijo de un proceso con 2 hilos: sigue con uno)
  fork y execve con varios hilos; cinco hijos con wait
MPROCESOS: todo bien (24 pruebas)
MPROCESOS: fork, execve, wait, tuberías y posix_spawn; dinámico: ld-musl en 0x7f0000000000, el programa en 0x555555554000
MPROCESOS: todo bien (24 pruebas)
MMAPA: mmap de archivos y memoria compartida; estático
  mmap de 46464 octetos del programa: MAP_PRIVATE (de leer, con desplazamiento, de escribir sin cambiar el archivo) y MAP_SHARED de solo leer
  MAP_SHARED anónima: padre e hijo suman a la vez con atómicos: 40000 (exacto)
  munmap, mprotect (PROT_NONE y otra vez de leer) y MADV_DONTNEED, con SIGSEGV atendida; 64 MB tocados
MMAPA: todo bien (16 pruebas)
MMAPA: mmap de archivos y memoria compartida; dinámico: ld-musl en 0x7f0000000000, el programa en 0x555555554000
MMAPA: todo bien (16 pruebas)
LUA: Lua 5.5 en Hispa-64 (con musl, sin cambiar nada)
LUA: todo bien (16 pruebas); hoy es 24/11/2026
Programa «LUA /PRUEBA.LUA uno dos»: acaba con el código 0, como debe (en 1074 ms)
Después de musl: 0 procesos, 0 en la tabla, 0 espacios, 0 tablas de programas, 0 tuberías; 74 hilos creados, 34 señales entregadas, 6927 esperas de futex, 20812 páginas compartidas al bifurcar (8360 copiadas al escribir, 193 sin copiar)
  la memoria: la de antes; el montón: 16 bloques (los de antes); las pilas: 15 (las de antes)
Hispa-64 etapa 7: todo en orden.
```

### Cómo se ha probado

El núcleo ocupa 336 KB (eran 304).

`castel/nucleo64/pruebas.sh`: **101 pruebas, todas bien** (eran 96), unos 29
minutos con el servidor. Las de la etapa 7:

- **musl y Lua se compilan** la primera vez (`programas/hazmusl.sh`: `configure`
  y `make -j1` en `build/musl-obj`, unos cinco minutos; Lua, con `make linux`
  en `build/lua-obj`) y los programas de prueba van como módulos, con el
  cargador dinámico (`ld-musl-x86_64.so.1`, la `libc.so` de musl) y
  `libsaludo.so`: la unidad en memoria tiene 25 archivos;
- **por multiboot con 1 procesador** («pc», 128 MB), **4** (q35) y **8**
  (`-cpu max`: con AVX), y **por UEFI** (OVMF y GRUB) **con 1** (128 MB) **y
  con 4** (`-cpu max`, 3 GB): en todas (`e7_bien`), **MHOLA** tres veces
  (estático; dinámico, con ld-musl en 0x7F0000000000, el programa en
  0x555555554000 y `libsaludo.so` cargada por ld-musl con `open` y `mmap`; y
  estático de posición libre): printf con todos sus formatos, strtod, malloc
  de 400 KB y realloc a 16 MB (musl prueba `mremap` y copia), qsort de
  100.000, 1000 malloc y free, calloc, time, nanosleep de 20 ms, fopen y fgets,
  ENOENT, uname, getpid y gettid, sysconf (el número de procesadores, con
  `sched_getaffinity`), isatty, getrandom; **MHILOS** dos veces (estático y
  dinámico): 8 hilos que suman con un mutex (16000, exacto), cada uno con su
  TLS y su tid, pthread_join con lo que devuelven, un productor y 8
  consumidores con `pthread_cond` (la suma, exacta), semáforos y una barrera,
  un hilo suelto que acaba con `pthread_exit`, `pthread_cond_timedwait` de 50
  ms (ETIMEDOUT a su hora) y, con AVX, 8 hilos con ymm0 a ymm7 llenos que ceden
  el procesador y dan vueltas sin llamar al sistema: intactos; **MSENALES**
  dos veces: raise y kill con su siginfo (SI_TKILL, SI_USER, si_pid), la coma
  flotante intacta al volver de la señal, la máscara (bloqueada no llega,
  sigpending la tiene, al desbloquearla llega), SIGSEGV en 0x10 (SEGV_MAPERR)
  y al escribir en su código (SEGV_ACCERR), SIGFPE (FPE_INTDIV) y SIGILL
  (ILL_ILLOPN) atendidas y salidas con siglongjmp, sigaltstack con
  SA_ONSTACK, `read` de una tubería interrumpido (con SA_RESTART se repite;
  sin él, EINTR), nanosleep interrumpido (EINTR y lo que quedaba), SIGCHLD
  (CLD_EXITED, si_pid, si_status), **un hijo que solo cuenta, sin llamar al
  sistema, cerrado con SIGTERM y con SIGINT**, sigsuspend (y la máscara de
  antes al volver), SIG_IGN, SIGPIPE ignorada (EPIPE) y sin ignorar (cierra al
  hijo), pthread_kill (la atiende ese hilo); **MPROCESOS** dos veces (24 pruebas): fork con
  16 MB (el hijo los lee y los cambia; el padre no ve sus cambios, ni el hijo
  los del padre: copiar al escribir), 100 KB por una tubería y la suma de
  vuelta por otra, el fin de archivo, poll de una tubería, fork + dup2 +
  execve de /MHOLA con su salida por una tubería, **posix_spawn** (clone con
  CLONE_VM y CLONE_VFORK) con su salida y de uno que no está (ENOENT),
  pipe2 con O_CLOEXEC (cerrado en el programa nuevo), cinco hijos con wait y
  luego ECHILD, fork con dos hilos y execve con tres (los otros acaban) y epoll de una tubería (el plazo, EPOLLIN con su
  dato, EPOLLONESHOT, EPOLLHUP, EPOLL_CTL_DEL);
  **MMAPA** dos veces: mmap del propio programa (MAP_PRIVATE de leer, con
  desplazamiento, de escribir sin cambiar el archivo, MAP_SHARED de solo leer,
  EACCES al pedir MAP_SHARED de escribir con O_RDONLY), un archivo corto (lo de
  después del final, a cero), MAP_SHARED anónima entre padre e hijo (40000
  sumas con atómicos, exactas), munmap y mprotect con SIGSEGV atendida,
  MADV_DONTNEED, 64 MB tocados y soltados; **Lua** con PRUEBA.LUA (16
  pruebas: los argumentos, string.format, utf8, table.sort de 10.000, las
  clausuras, las corrutinas, pcall, gsub, las cuentas, os.time, io.open, load
  y el recolector); y **después, nada de más**: 0 procesos, 0 espacios, 0
  tablas, 0 tuberías, y la memoria, el montón y las pilas, los de antes;
- **el intérprete con las teclas del monitor de QEMU**: además de lo de la
  etapa 6, `LUA /PRUEBA.LUA uno dos`, **Lua a mano** (`LUA`, su saludo,
  `print(6 * 7, "ñandú")` da «42 ñandú», y Ctrl+D) y **Ctrl+C a MBUCLE**, que
  da vueltas sin llamar nunca al sistema: «(MBUCLE se ha cerrado por la
  señal 2, SIGINT: Ctrl+C)»;
- las vueltas que no necesitan los programas de musl (sin HPET, con 2
  procesadores, mwait, 3 y 8 GB, los discos, el USB, UEFI con 8 GB...) llevan
  «sinmusl»; lo de las etapas 0 a 6, igual (HIJOS, con `fork` ahora de copiar
  al escribir, igual de bien).

`SOLO=castel64` (con CASTEL64 y ENSAMB64 el núcleo sale igual, octeto a
octeto, y CASTEL32 con corre32 también): **80 bien y 0 mal** (36 minutos; dentro,
`nucleo64/pruebas.sh` con CASTEL64 y ENSAMB64: 102 bien y 0 mal). Al hacerlo
salió que la línea de cada módulo del cargador se cortaba a 63 octetos (con
`-initrd` y una carpeta de trabajo de ruta larga, los nombres de la unidad
quedaban en «A», «C»...): ahora, hasta 255.

Las imágenes de la carpeta del taller
(`Pruebas/Pendientes/VirtualBox/Hispa-64-etapa-7/`): el disco por UEFI en SATA
(q35, 4 procesadores, `-cpu max`), arrancado en QEMU hasta «todo en orden» y,
en el intérprete, `LUA` a mano (`print(6 * 7, "ñandú")`: «42 ñandú»), Ctrl+D,
`MBUCLE` y Ctrl+C («se ha cerrado por la señal 2») y `salir`; y el CD por la
BIOS, hasta «todo en orden».

Con varios procesadores, el TCG de QEMU hace muy caro lo que los hilos se
esperan unos a otros (cada FUTEX_WAKE que despierta a uno de otro procesador es
un IPI y un `hlt`): MHILOS tarda de 0,8 s con uno a 4 s con cuatro, y por UEFI
con la pantalla del GOP una vez ha llegado a 32 s; en un equipo de verdad son
milisegundos.

Lo que QEMU no prueba: AVX-512 (el TCG de QEMU 8.2 no lo tiene: XCR0 = 7, 832
octetos por hilo), PCID con varios hilos, y los procesadores híbridos.

### Lo que queda para la etapa 8 (y más adelante)

- **La caché de páginas compartida con el VFS**: `mmap` de archivos sin copiar
  (las páginas, al tocarlas, y las mismas para todos los que las lean),
  `MAP_SHARED` de escribir (que lo escrito vaya al archivo, con `msync`), y
  los ejecutables y las bibliotecas compartidas de verdad entre procesos (hoy
  cada uno tiene su copia, compartida solo entre un padre y sus hijos de fork).
- **Lo que le falta a POSIX**: `rt_sigtimedwait` (`sigwait`), las señales de
  tiempo real encoladas (hoy, una pendiente de cada número), `setitimer` y
  `alarm`, el control de trabajos (SIGSTOP, SIGTSTP, SIGCONT de verdad; los
  grupos en primer plano con Ctrl+Z), la lista robusta recorrida al acabar
  un hilo, `/dev/null`, `/dev/zero` y `/proc` (lo que miran muchas
  herramientas), `mremap`, `epoll` con EPOLLET de verdad (hoy avisa mientras
  esté listo) y sin mirar las tuberías de 10 en 10 ms (también `poll`), `eventfd`, `timerfd` y `signalfd`
  (los navegadores), los enlaces (`symlink`, `readlink`), los permisos y los
  usuarios.
- **La tabla de los descriptores, con su cerrojo**: hoy dos hilos que abren y
  cierran archivos a la vez pueden pisarse (musl no lo hace en sus pruebas;
  los navegadores, sí).
- **Más herramientas de verdad**: toybox (0BSD), SQLite (dominio público),
  `make`, y las pruebas de libc-test de musl en cada arranque; un
  `musl-gcc` con `-static-pie` (hoy, a mano).
- **Direcciones al azar** (ASLR) para la pila, los PIE, el cargador y mmap.
- Los **discos que se enchufan en marcha** (de la etapa 6) y quitarlos con
  seguridad.
- **Gráficos, ventanas y la oficina** (la etapa 8 del plan): el compositor, el
  ratón, la letra con suavizado y la oficina de Hispa-Pentium; con musl, las
  bibliotecas de C de siempre (FreeType, libpng, zlib) se pueden traer ya.

---

## 14. La etapa 8, hecha (24/11/2026): la pantalla para los programas, las ventanas y la oficina

### Las decisiones

- **La consola la dibuja un hilo, con las interrupciones puestas.** Escribir
  en la consola (con el cerrojo de la consola, sin interrupciones) ya solo
  cambia el texto y apunta, por cada fila, de qué casilla a qué casilla hay
  que volver a dibujar (un `nat64` por fila, que el hilo toma y deja vacío de
  una vez con `xchg`: nada se pierde); el texto es un **anillo de filas**
  (subir es mover el principio, sin copiar). El hilo «pantalla» mira cada 20
  ms si hay algo y lo dibuja con un mutex que duerme, sin ningún cerrojo de
  espera activa. Antes de que haya hilos, y cuando Hispa-64 se para, se
  dibuja al momento, como antes. Con eso, **el plazo del aviso de vaciar la
  TLB vuelve a 2 s** (en la etapa 6 se subió a 10 s porque un procesador
  podía pasarse más de 2 s dibujando con las interrupciones quitadas).
- **El cursor se ve**: una raya de dos filas de la letra debajo de su
  casilla, de la tinta con la que se escribe (se ve por la forma, no por el
  color); `ESC [ ? 25 l` y `h` lo quitan y lo ponen. **Borrar la pantalla
  es borrar** (`ESC [ 2 J`, también `0 J`, `1 J` y `K`), y `ESC [ fila ; col
  H` y las flechas de VT100 (`A`, `B`, `C`) mueven el cursor.
- **Los dispositivos, los de Linux** (`disp64.cas`), para que los programas
  gráficos no necesiten llamadas propias:
  - **`/dev/fb0`** (fbdev): `FBIOGET_VSCREENINFO`, `FBIOGET_FSCREENINFO`,
    `FBIOPUT_VSCREENINFO` (sin cambiar de modo), `FBIOPAN_DISPLAY` y `mmap`,
    con **doble búfer**: lo que da `mmap` mide el doble (`yres_virtual` = 2 ×
    `yres`): la mitad de arriba es la pantalla (con escritura combinada) y la
    de abajo un búfer en la RAM; `FBIOPAN_DISPLAY` con `yoffset = yres`
    copia el de abajo a la pantalla (el GOP no sabe cambiar de búfer: se
    copia) y **`FBIO_HISPA_COPIA` (0x46E0, propio)** copia solo un
    rectángulo (cuatro `uint32_t`: x, y, ancho, alto), lo que ha cambiado.
    El primer programa que pone la pantalla en su memoria **la tiene** (otro
    recibe EBUSY) y la consola deja de dibujar (guarda lo escrito); al
    acabar ese programa, la consola se pinta entera otra vez. Las páginas de
    la pantalla llevan en sus tablas la marca **DISPOSITIVO** (el bit 11): no
    son de la RAM y nunca se sueltan ni se cuentan; las marcas de caché (la
    PAT) se respetan al cambiar los permisos.
  - **`/dev/input/event0`** (evdev): la `struct input_event` de Linux (24
    octetos); `EV_KEY` con los códigos de Linux (`KEY_*` desde los usos de
    HID; 1 pulsada, 0 soltada, 2 repetida) y `BTN_LEFT`, `BTN_RIGHT`,
    `BTN_MIDDLE`; **`EV_ABS` con `ABS_X` y `ABS_Y` en puntos de la
    pantalla** (venga de un ratón, de una tableta o del touchpad: el núcleo
    ya lleva la posición del puntero); `EV_REL` con `REL_WHEEL`; `EV_SYN`; y
    uno propio, **`EV_LETRA` (0x1E)**: la letra ya hecha con el mapa español
    del núcleo (acentos muertos, AltGr, mayúsculas, Bloq Mayús), en Unicode,
    o el control (13 Intro, 127 Retroceso, 9 Tab, 27 Esc, Ctrl+A..Z = 1..26):
    así los programas no repiten el mapa del teclado. Cada descriptor tiene
    su cola de 1024 sucesos; `EVIOCGRAB` hace que las teclas no vayan a la
    consola; `EVIOCGVERSION`, `EVIOCGID`, `EVIOCGNAME`; `read` espera (o
    EAGAIN con O_NONBLOCK) y `poll`, `select` y `epoll` lo miran (de 10 en
    10 ms, como las tuberías). Los avisos los da el ratón con su cerrojo
    tomado: por eso no despiertan a nadie (nada de tomar el cerrojo del
    planificador desde ahí).
  - **`/dev/shm/NOMBRE`**: memoria compartida con nombre, lo que usa
    `shm_open` de musl (`open`, `ftruncate`, `mmap` con `MAP_SHARED` de leer
    y escribir, `fstat`, `unlink`), **entre procesos que no son padre e
    hijo**. El objeto guarda sus marcos (y cuenta como uno más que los
    tiene, con la tabla de «compartidos» de la etapa 7); cada `mmap` suma
    uno; `unlink` le quita el nombre y se suelta cuando ya nadie lo tiene
    abierto, y cada marco, cuando ya nadie lo tiene puesto. Hasta 64 objetos
    de 128 MB.
  - **`/dev/null` y `/dev/zero`**, que pedían muchas herramientas.
- **El servidor de ventanas, un programa** (VENTANAS, en C con musl, en el
  anillo 3), no parte del núcleo: si falla, se cierra él y vuelve la
  consola. Compone en el búfer de atrás de `/dev/fb0` y copia a la pantalla
  solo los rectángulos que cambian. **El protocolo** (`hv.h`) es por
  memoria compartida y futex entre procesos: `/dev/shm/hispa-ventanas` con
  lo que mide la pantalla, el portapapeles y 32 huecos de ventana (el
  programa toma uno con `cmpxchg`); los puntos de cada ventana, en otra
  memoria compartida del programa (`/dev/shm/hv-PID-N`) que el servidor pone
  de solo leer en la suya; lo que pide el programa (título, maximizar,
  centrar, cerrar, un menú emergente, un búfer nuevo, lo que ha pintado),
  bits que el servidor toma de una vez; los sucesos, una cola en el hueco
  con su futex. Un programa que acaba sin cerrar su ventana se ve (`kill`
  con la señal 0 da ESRCH) y se le quita. Sin tuberías ni sockets: no hacía
  falta nada más del núcleo.
- **El escritorio, al estilo del de Hispa-Pentium**: el tapiz (azul, con
  olas y «Hispa-64»), la barra de abajo con el botón **Inicio** (verde,
  redondeado), un botón por ventana (la activa, subrayada; pulsarlo la trae
  delante o la minimiza) y el reloj; el **menú Inicio** con los programas
  que hay (Pluma, Cuentas, Notas) y «Salir del escritorio» (si un programa
  pregunta si se guarda lo cambiado, no se sale); la tecla del logo o
  Ctrl+Esc lo abren. Las ventanas: título (la activa, **oscura, en negrita
  y con una raya clara debajo**; las demás, claras: se distinguen por la
  luz y la forma, no por el color, para Carlos), los botones de minimizar,
  maximizar y cerrar **dibujados** (_ □ X), arrastrarlas por el título,
  cambiarles el tamaño por la esquina de abajo a la derecha (con sus tres
  rayitas), el doble clic en el título, Alt+F4, el foco al pulsar, el doble
  y el triple clic, la rueda, los menús emergentes de los programas
  (`so_menu`), la forma del puntero (flecha o la I del texto) y el
  portapapeles. Las letras, **las DejaVu con suavizado** de la oficina
  (`comun/letras.c` y `lienzo.c`).
- **La oficina, tal cual**: `castel/nucleo32/oficina/comun/so_hispa64.c` es
  la capa «so» de Hispa-64 (la memoria y los archivos como
  `so_anfitrion.c`, con las unidades de DOS: **`C:` es el disco de
  `$HISPA_C`, o el que tenga la carpeta DOCUMENT, o el primero que no sea
  el del arranque**; la ventana, con el protocolo de VENTANAS). Con ella,
  **Pluma 2 y Cuentas 2 se compilan sin cambiar ni una línea** con
  `musl-gcc` (estáticas: 0,8 MB cada una con sus letras). Y **NOTAS**, un
  bloc de notas nuevo y sencillo con la misma capa (UTF-8, la DejaVu Sans
  Mono, Ctrl+G guarda).
- **`escritorio`** en la línea del núcleo arranca VENTANAS en vez del
  intérprete (al salir de él, el intérprete); en el menú de GRUB es la
  primera opción (y otra con `escribir`, para guardar en los discos de
  prueba). Desde el intérprete: `VENTANAS`, o `VENTANAS NOTAS PLUMA` (los
  programas, arrancados al empezar; `NOTAS=/sata1/A.TXT`, con su archivo).
- Al hacerla salió **un fallo de la etapa 7 en las tuberías**: `cierra_`
  soltaba el cerrojo para despertar a los que esperan y lo volvía a tomar
  para ver si se soltaba la tubería; dos que cerraban a la vez (el
  `execve` con O_CLOEXEC del hijo de `posix_spawn` y el `close` del padre)
  podían soltarla dos veces (el núcleo se paraba en `Monton64.libera`). Ahora
  se decide con el cerrojo tomado, y mientras se despierta cuenta como «en
  uso».
- Y otro de la etapa 1: **las zonas apartadas de la memoria física** (el
  núcleo, la información del cargador, sus módulos, la pantalla) eran 32
  como mucho y las de más se perdían sin decir nada; con los 30 módulos de
  ahora, los últimos (`libsaludo.so`) se daban como memoria libre y se
  pisaban. Ahora son 96, las seguidas se juntan en una, y si no caben se
  para diciéndolo.
- Y uno de la etapa 4, que salió al pasar la batería con el servidor
  cargado: **`p.hilo` se ponía después de encolar el primer hilo** de un
  proceso (en `lanza` y en `fork`). El hilo puede correr y acabar en otro
  procesador en cuanto está en la cola: un programa que fallaba al momento
  (MALO, con 4 u 8 procesadores) se veía como «un hilo más», se soltaba
  solo, y `espera_proceso` hacía `une` de un hilo ya soltado: se quedaba
  esperando para siempre (los procesadores, todos en `hlt`) o lo liberaba
  dos veces. Ahora `crea_de_programa` lo apunta antes de encolarlo (y
  `vivos` se suma antes). Con tres QEMU a la vez (8, 8 y 4 procesadores),
  24 arranques de 24 bien; el núcleo de la etapa 7, en las mismas, fallaba
  en la mitad.
- Y una carrera de la propia prueba de MSENALES: el hijo que debía morir
  por SIGPIPE podía escribir antes de que el padre cerrase su lado de
  leer (con 8 procesadores, pasaba); ahora se cierra antes del `fork`.

### Qué hace

Hispa-64 0.8.0. Los módulos y programas nuevos y lo que cambia:

| Archivo | Qué |
|---|---|
| `pant64.cas` | la consola: el anillo de filas, lo pendiente por fila, el hilo «pantalla», el cursor, borrar, mover el cursor, el modo gráfico |
| `disp64.cas` | `/dev/fb0` (con el búfer de atrás), `/dev/input/event0`, `/dev/shm`, `/dev/null`, `/dev/zero` |
| `sistema.cas` | los dispositivos en `open`, `read`, `write`, `close`, `dup`, `fcntl`, `fstat`, `stat`, `ftruncate`, `mmap`, `unlink`, `ioctl`, `poll`, `select`, `epoll`; uname 0.8.0 |
| `espacios.cas`, `procesos.cas` | la marca DISPOSITIVO; el núcleo no toca la pantalla de un programa por el mapa directo; al acabar un proceso, Disp64 lo sabe (la pantalla vuelve a la consola) |
| `teclas64.cas`, `raton64.cas` | los sucesos de `/dev/input/event0`; con EVIOCGRAB, las letras no van a la consola |
| `tty64.cas` | `ESC [ J`, `K`, `H`, `A`, `B`, `C`, `? 25 h/l` |
| `smp64.cas` | el aviso de la TLB, 2 s otra vez |
| `tubos64.cas` | la carrera al cerrar (arriba) |
| `hilos64.cas`, `procesos.cas` | `crea_de_programa` apunta el hilo antes de encolarlo (arriba) |
| `fisica64.cas` | las zonas apartadas, hasta 96 (arriba) |
| `hispa64.cas` | el hilo «pantalla», la opción `escritorio`, 0.8.0 |
| `programas/ventanas/` | `hv.h` (el protocolo), `hdisp.h` (fbdev y evdev para musl), `ventanas.c` (VENTANAS), `notas.c` (NOTAS) |
| `programas/hazventanas.sh` | VENTANAS, NOTAS, PLUMA y CUENTAS, con musl y la biblioteca común de la oficina (los objetos, en `build/ventanas-obj`) |
| `programas/musl/mdisp.c` | MDISP: las pruebas de los dispositivos (en cada arranque) y `MDISP sucesos N` |
| `nucleo32/oficina/comun/so_hispa64.c` | la capa «so» de la oficina para Hispa-64 |
| `pantallazo.py` | el comparador: `foto`, `cuenta` (los puntos de un color en un rectángulo), `oscuros`, `difiere` |
| `programas/teclas.py` | `{ctrl-LETRA}`, `{alt-f4}`, `{logo}`, `{ctrl-esc}` |


Lo que dice al arrancar (lo nuevo; QEMU, q35, por UEFI):

```
MDISP: los dispositivos (/dev/shm, /dev/fb0, /dev/input/event0, /dev/null, /dev/zero); estático
  /dev/shm: 256 KB compartidos con otro programa (execve), por su nombre
  /dev/fb0: 1280x800, 32 bits, 5120 octetos por línea, 8000 KB (con el búfer de atrás)
  /dev/input/event0: «Hispa-64: teclado y puntero»
MDISP: todo bien (29 pruebas)
```

Y en el escritorio (la salida de VENTANAS, por el puerto serie):

```
VENTANAS: en marcha: 1280x800, 1280 puntos por línea; el protocolo en /dev/shm/hispa-ventanas (293 KB)
VENTANAS: arranca /NOTAS (proceso 4)
VENTANAS: ventana nueva «Notas - NOTA.TXT» del proceso 4: 768x459
NOTAS: guardado (46 octetos)
VENTANAS: ventana «Notas - NOTA.TXT» movida a (240, 148)
VENTANAS: menú Inicio abierto
VENTANAS: arranca /PLUMA (proceso 5)
VENTANAS: ventana nueva «Pluma» del proceso 5: 1152x689
pluma: guardado C:\DOCUMENT\carta.PLU (PLU)
VENTANAS: Alt+F4: cerrar «carta.PLU - Pluma»
cuentas: guardado C:\DOCUMENT\libro.HOJ
VENTANAS: salir del escritorio
VENTANAS: fuera; la pantalla vuelve a la consola
```

### Cómo se ha probado

`castel/nucleo64/pruebas.sh`: **105 pruebas, todas bien** (eran 101),
unos 31 minutos con el servidor. Lo de la etapa 8:

- **MDISP en cada arranque** (con las demás de musl, por multiboot sin
  pantalla —`/dev/fb0` da ENXIO, como debe— y por UEFI con ella): `/dev/null`
  y `/dev/zero`; **`/dev/shm` entre dos programas que no son padre e hijo**
  (el otro es el mismo MDISP arrancado con `execve`, que la abre por su
  nombre, comprueba los 256 KB y escribe los suyos, que el primero ve),
  O_EXCL (EEXIST), `shm_unlink` (luego ENOENT), `ftruncate` más corto con
  ella puesta; `/dev/fb0`: los dos ioctl, `mmap` de la pantalla y del búfer
  de atrás, **otro proceso no puede tenerla a la vez (EBUSY)**, un dibujo en
  el búfer de atrás copiado con `FBIO_HISPA_COPIA` y comprobado en la
  pantalla, `FBIOPAN_DISPLAY`; `/dev/input/event0`: EVIOCGNAME,
  EVIOCGVERSION, EAGAIN, `poll` con plazo, EVIOCGRAB. Y después, **nada de
  más**: «Después de musl» comprueba también que no queda memoria
  compartida, y la memoria (con el búfer de atrás ya soltado), la de antes.
- **Por UEFI, con el teclado del monitor de QEMU y la tableta USB por QMP**
  (una sola vuelta de unos 3 minutos, con un disco FAT32 con DOCUMENT y otro
  exFAT hecho con hazexfat.py):
  - `MDISP sucesos 7`: **la a** (KEY_A pulsada, EV_LETRA 97, KEY_A soltada),
    **la tableta en (640, 400)** (EV_ABS exactos) y **la ñ** (KEY_SEMICOLON:
    el sitio de la ñ);
  - `VENTANAS NOTAS=/sata2/NOTA.TXT`: escribir «Hola, exFAT: ñandú y
    pingüino.», Intro y otra línea, Ctrl+G: **NOTA.TXT en el disco exFAT,
    comprobado con miraexfat.py** (el volumen bien y lo escrito, con ñ y ü);
    el pantallazo: el título de la activa (unos 17.000 puntos del azul
    oscuro), la letra (cientos de puntos oscuros) y el botón Inicio;
  - **arrastrar la ventana** por el título con la tableta (de (300, 33) a
    (500, 133)): «movida a (240, 148)», y en el pantallazo el título está
    donde debe (y cambian unos 340.000 puntos);
  - **el menú Inicio con el ratón** (en el pantallazo, su cabecera) y desde
    él **Pluma 2**: escribir, Ctrl+G, «carta» e Intro: **DOCUMENT/carta.PLU
    en el disco FAT32** (1758 octetos); el pantallazo: la página blanca y lo
    escrito; Alt+F4 («pluma: adiós»);
  - **Cuentas 2** desde el menú: 12, 30 y `=A1*A2+1` (en el pantallazo, las
    cifras de A1 a A3), Ctrl+G, «libro»: **DOCUMENT/libro.HOJ**; Alt+F4;
    `fsck.fat -n` sin quejas;
  - **salir del escritorio** desde el menú: la consola vuelve entera (unos
    27.000 puntos grises en el pantallazo); **`cls` la borra de verdad**
    (quedan 32 puntos: «/> » y **el cursor, una raya de 8 × 2 justo debajo
    de la casilla 3**).
- Lo de las etapas 0 a 7, igual (las cuentas de la unidad en memoria, 30
  archivos ahora; uname 0.8.0); el aviso de la TLB con su plazo de 2 s otra
  vez, sin pasarse en ninguna de las vueltas (con varios procesadores, por
  UEFI y con MALO a la vez).

`SOLO=castel64` (con CASTEL64 y ENSAMB64 el núcleo sale igual, octeto a
octeto): **80 bien y 0 mal** (36 minutos; dentro, `nucleo64/pruebas.sh` con
CASTEL64 y ENSAMB64: 106 bien y 0 mal). Una primera pasada, a la vez que
los arranques del CD y del disco de la entrega y las pruebas de otras
sesiones, dio 79 y 1: tres cosas de tiempos con el servidor muy cargado
(un «dormir» que se pasó 72 ms, una tecla del monitor perdida); repetida
sin nada más mío a la vez, todo bien.

Los pantallazos se comprueban por lo que tienen (cuántos puntos de un color
en cada sitio, cuántos oscuros, cuántos cambian de uno a otro), no punto a
punto con uno guardado: el reloj de la barra cambia, y así una mejora del
dibujo no rompe las pruebas.

Las imágenes de la carpeta del taller
(`Pruebas/Pendientes/VirtualBox/Hispa-64-etapa-8/`): el disco por UEFI (q35, 2 procesadores,
`-cpu max`, la tableta USB), arrancado en QEMU con la entrada 1 hasta el
escritorio y el menú Inicio abierto con Ctrl+Esc (en el pantallazo: Pluma,
Cuentas, Notas y «Salir del escritorio»); y el CD por la BIOS, hasta el
escritorio también («VENTANAS: en marcha: 1280x800»). En la carpeta, un
LEEME.TXT con cómo crear la máquina, qué probar y qué devolver si falla
(el puerto serie a un archivo, una foto y el VBox.log).

Con varios procesadores y el servidor cargado salió el fallo de `p.hilo`
(arriba): la primera pasada dio 101 bien y 4 mal (todo en la vuelta de 8
procesadores: el de MSENALES), la segunda 96 y 9 (dos vueltas colgadas en
MALO); reproducido con tres QEMU a la vez y un guion que, si la salida se
para 40 s, saca los registros por el monitor (los procesadores, todos en
`hlt`), y también con el núcleo de la etapa 7. Arreglado: 105 y 0.

### Lo que queda para la etapa 9 (y más adelante)

- **La red** (la etapa 9 del plan): los drivers de cable (Realtek
  8111/8168, Intel I219 y e1000e en QEMU, USB RTL8153), TCP/IP con DHCP y
  DNS, `socket`, `connect`, `bind`, `accept` de POSIX (con `poll` y
  `epoll`), y TLS con BearSSL como en Hispa-Pentium; se verá con una
  descarga por HTTPS, NTP y SSH. La Wi-Fi (Intel AX201/AX211), en paralelo.
  `getrandom` ya está; faltan `/dev/urandom`, `eventfd` y `timerfd` (los
  piden las bibliotecas de red y los navegadores).
- **Despertar en vez de mirar**: `/dev/input/event0`, `poll`, `epoll` y el
  servidor de ventanas miran de 10 en 10 ms; con la red hará falta una
  espera que despierte desde los drivers (sin el cerrojo del ratón tomado).
- **El lector de PDF y abrir docx y xlsx de verdad en el taller**: Pluma y
  Cuentas ya los leen (su código es el de Hispa-Pentium); falta probarlos
  con archivos del taller y el lector de PDF (Hispa-PDF) con su capa «so».
- **El touchpad y el ratón del taller en el escritorio**: en QEMU va la
  tableta; el touchpad I2C-HID y los ratones USB dan EV_ABS igual (el núcleo
  lleva la posición), pero falta verlo en los portátiles. El ratón PS/2
  aún no está (en VirtualBox, «Tableta USB»).
- El dibujo con **SIMD** (copiar y mezclar con SSE2/AVX2), la
  **transparencia** de las ventanas y las sombras; **cambiar el tamaño**
  también por los bordes (hoy, la esquina); más **formas del puntero**.
- Una **consola en una ventana** (un emulador de terminal sobre `/dev/pts`):
  hoy, para volver al intérprete hay que salir del escritorio.
- **La caché de páginas compartida** (de la etapa 7): `mmap` de archivos sin
  copiar y los ejecutables compartidos entre procesos (Pluma y Cuentas,
  estáticas, ocupan 0,8 MB cada una por proceso).
- Lo demás de la etapa 7 que sigue pendiente (control de trabajos,
  `/proc`, la tabla de descriptores con cerrojo, ASLR...).
- La hora: el reloj de la barra es el del núcleo, que toma el de la CMOS como
  UTC; en los equipos con Windows la CMOS va en hora local (como en
  Hispa-Pentium, habrá que decidir una zona).

---

## 15. La etapa 9, hecha (25/11/2026): la red

### Las decisiones

- **La pila TCP/IP, en el núcleo y en Castel 64**, sin lwIP: la lógica es la de
  Hispa-Pentium (`castel/nucleo32/rpila.cas` y `rtcp.cas`), escrita otra vez
  para 64 bits y para un núcleo con varios procesadores. Todo lo de los
  protocolos y los sockets se toca con **un mutex, el de la red**
  (`Redcom64.red_m`); lo mueve **un hilo del núcleo, «red»**: lo despierta
  la interrupción de la tarjeta (o mira cada 10 ms si no llega), recoge lo
  que haya llegado (de 32 en 32 tramas, soltando el cerrojo entre medias), lo
  que queda en la cola de `lo`, los plazos (ARP, DHCP, los de TCP) y el enlace
  (cada medio segundo). Lo que mandan los programas sale desde su llamada,
  con el mutex cerrado (sin copias de más: la cabecera se pone delante, en el
  mismo búfer).
- **Despertar en vez de mirar** (`sondeo64.cas`): hay un número de cambios y
  una cola de espera; quien cambia algo que se puede esperar (las tuberías,
  la consola, los sockets, eventfd, la pila al tratar lo que llega) llama a
  `avisa()`; poll, select, epoll y las esperas de los sockets toman el número
  antes de mirar y duermen solo si no ha cambiado (con el cerrojo de la cola:
  nada se pierde). Los timerfd ponen su vencimiento como plazo de la espera.
  Solo `/dev/input/event0` sigue de 10 en 10 ms (sus avisos vienen con el
  cerrojo del ratón tomado).
- **Los sockets, los de Linux** (`red64.cas`): las llamadas 41 a 55 y 288,
  con sus números y sus formas; AF_INET con SOCK_STREAM (TCP), SOCK_DGRAM
  (UDP, y los «ping sockets» de ICMP de Linux, que no piden permisos: el
  núcleo pone el identificador y la suma) y SOCK_RAW de ICMP; AF_INET6 y
  AF_UNIX dan EAFNOSUPPORT (musl lo entiende). Los nombres los resuelve
  **musl** (getaddrinfo, con UDP y, si hace falta, TCP): el núcleo da
  `/etc/resolv.conf` (los DNS de DHCP), `/etc/hosts` (localhost, el equipo y
  los de `hosts=nombre:ip,...` de la línea del núcleo), `/etc/services`,
  `/etc/hostname`, `/etc/protocols`, `/proc/net/route` y `/proc/net/dev`
  (con la forma de Linux), hechos al abrirlos.
- **TCP sin escala de ventana ni SACK** (64 KB de búfer para cada lado): con
  la red de casa sobra; la escala, SACK e IPv6 quedan para la etapa 10 (el
  navegador). Lo desordenado se tira (la otra punta lo repite). El TIME_WAIT
  dura 2 s (y las pruebas de musl esperan a que acabe: sin perder memoria).
- **El azar, con ChaCha20** (`chacha64.cas`, RFC 8439, la clave borrada en cada
  pedido): getrandom, `/dev/urandom` y `/dev/random` (que nunca esperan, como
  en Linux desde la 5.6), los números de secuencia de TCP, los puertos, los
  identificadores de DHCP. La clave se mezcla en cada pedido con RDSEED y
  RDRAND (si los hay), el TSC y el reloj, y con lo que va echando la red.
- **La hora** (`hora64.cas`): el reloj del sistema es UTC y NTP lo pone
  (clock_settime, settimeofday); **el CMOS se toma por omisión como hora
  local** (los equipos del taller tienen Windows; `cmos=utc` si va en UTC);
  la zona, `zona=Europa/Madrid` (la de por omisión; también Atlántico/
  Canarias, Europa/Lisboa, Europa/Londres, UTC, los nombres en inglés, o una
  zona de POSIX tal cual), con el cambio de horario de las reglas «M» de
  POSIX: el núcleo la usa para leer el CMOS, y los programas la reciben en
  **TZ** (`TZ=CET-1CEST,M3.5.0,M10.5.0/3`): musl hace lo demás.
- **Los drivers**, cada uno con sus anillos de descriptores en memoria física
  y su interrupción (MSI si la tarjeta la tiene; si no, la patilla INTx por
  la línea que dejó el firmware, que en QEMU llega al IOAPIC por esa misma;
  y el hilo mira cada 10 ms igual, porque VirtualBox lleva las INTx a otras
  patillas que dice el AML, y el `_PRT` aún no se lee):
  - **Intel** (`intel64.cas`): la familia 8254x («e1000»: la 82540EM de QEMU
    y la **«Intel PRO/1000 MT Desktop», la tarjeta por omisión de VirtualBox**),
    las 8257x («e1000e»: la 82574L de QEMU, con MSI) y las de los chipsets
    (ICH y PCH: 82577/8/9, I217, I218, **I219**), todas con los descriptores
    de siempre.
  - **Realtek RTL8111/8168** (`rtl64.cas`) y su familia (8101/8102/8105/8106,
    8169, 8411, 8117): la de Hispa-Pentium con lo aprendido el 24/11 en el
    ASUS GL553VD (la 8111H): en las 8168G en adelante, el FIFO compartido
    mirado en LINK_LIST_RDY, sin EEE ni ALDPS ni el paso a L2/L3, y a
    negociar otra vez; sin ASPM ni CLKREQ. QEMU no tiene la 8168, pero su
    RTL8139 en «modo C+» usa los mismos descriptores: con `realtek=ensayo` se
    lleva con este driver, y así se prueban sus anillos.
  - **virtio-net** (`virtio64.cas`), por la forma de siempre (la de
    transición, 1AF4:1000): la «paravirtualizada» de QEMU y de VirtualBox.
- Al hacerla salió **un fallo de la etapa 7**: el bloque de los registros de
  la FPU de un hilo nuevo no se ponía a cero, y `xsave` no escribe los
  octetos 528 a 575 de su cabecera (reservados, que `xrstor` quiere a 0): con
  `-cpu max` (xsave) y un bloque del montón ya usado (un socket soltado), el
  núcleo se paraba con un #GP en `pon_fpu` al cambiar a ese hilo (MRED, con
  1 procesador también). Ahora `nuevo_fpu` lo pone a cero. Y en Sondeo64
  se despierta a cada uno con el cerrojo de la cola tomado (así no puede
  acabar su hilo entre sacarlo de la cola y despertarlo).
- La **red por USB (RTL8153, CDC-ECM/NCM)** no ha entrado: pide elegir otra
  configuración del aparato (la 1 de la RTL8153 es la propia de Realtek; la
  de CDC-ECM es otra), la variante 1 de la interfaz de datos y una
  recepción a granel que no tenga el mutex del aparato mientras espera (hoy
  `u_granel` lo tiene todo el rato). Queda para la etapa 10 (sección de
  abajo).

### Qué hace

Hispa-64 0.9.0. Los módulos y programas nuevos y lo que cambia:

| Archivo | Qué |
|---|---|
| `redcom64.cas` | las interfaces (Ifz), los sockets (Sock: TCP dentro), el mutex de la red, el suceso del hilo, el orden de los octetos, la suma de IP, los anillos de los búferes |
| `ip64.cas` | Ethernet, ARP (32 vecinos, con el paquete que espera), IPv4, ICMP, UDP, DHCP (con renovación), `lo` (127.0.0.1 y la IP propia), las rutas, `ip=` (IP fija), el hilo «red» |
| `tcp64.cas` | TCP: conectar, escuchar y aceptar, la ventana, la retransmisión con su plazo medido (de 200 ms a 60 s), la rápida (tres ACK repetidos), la ventana a cero (persist), el cierre en las dos direcciones, RST; `red=traza` dice cada 2 s cómo está cada conexión |
| `red64.cas` | los sockets de POSIX, sus opciones, FIONREAD, FIONBIO y los SIOCGIF*, poll; los archivos de texto de la red |
| `sondeo64.cas` | la espera que despierta (arriba) |
| `objet64.cas` | eventfd (284, 290), timerfd (283, 286, 287), `/dev/urandom`, `/dev/random` y los textos |
| `chacha64.cas` | ChaCha20 (arriba) |
| `hora64.cas` | la hora y la zona (arriba) |
| `intel64.cas`, `rtl64.cas`, `virtio64.cas` | los drivers (arriba) |
| `prred64.cas` | las pruebas: lo que hay (las tarjetas, el azar, la hora y la zona) y, si en la unidad está `RED.PRB`, sus órdenes, mirando sus códigos; al acabar, que no quede ningún socket |
| `sistema.cas` | las llamadas nuevas (41-55, 288, 283-290, 227 clock_settime, 164 settimeofday), los sockets y los objetos en read, write, close, dup, fcntl, fstat, lseek, ioctl, poll, select y epoll; el reloj de Hora64; uname 0.9.0 |
| `procesos.cas` | TZ en el entorno de los programas; el bloque de la FPU a cero (arriba) |
| `tubos64.cas`, `tty64.cas` | avisan a Sondeo64 |
| `programas/red/` | PING, IFCONFIG, DESCARGA (HTTP y HTTPS con BearSSL), NTP, ECO y MRED (en C con musl; `hazred.sh`), y `servidores.py`: los servidores de las pruebas (HTTP, HTTPS, NTP, eco, DNS y los certificados) |

Lo que dice al arrancar (QEMU, q35, la e1000):

```
Red: 00:02.0 8086:100E Intel 82540EM (PRO/1000 MT Desktop, e1000), MAC 52:54:00:12:34:56, INTx por la GSI 11 (y mirando cada 10 ms)
Red: eth0: enlace a 1000 Mbit/s
Red: eth0: IP 10.0.2.15/24 (DHCP de 10.0.2.2, 86400 s); puerta 10.0.2.2; DNS 10.0.2.3
MRED: todo bien (123 pruebas)
Red: 1 tarjetas (y «lo», 127.0.0.1); TCP/IP en el núcleo (ARP, IPv4, ICMP, UDP, TCP, DHCP), los sockets de POSIX
  el azar: ChaCha20 con RDSEED/RDRAND; la hora: 24/11/2026 23:49:54 CET (UTC+1), zona Europa/Madrid (TZ=CET-1CEST,M3.5.0,M10.5.0/3), el CMOS en hora local
```

Y con los programas (las órdenes de `RED.PRB` de las pruebas; en el
intérprete, igual):

```
PING 10.0.2.2: 3 enviados, 3 recibidos, 0 % perdidos; tiempo mín/media/máx = 0,71/1,74/3,78 ms
DESCARGA: TLS 1.2 (suite 0xC02C), certificado de «anfitrion.prueba» bien
DESCARGA: 1048576 octetos en 0,63 s (1,6 MB/s); sha256 = ...
NTP: 10.0.2.2:55281 dice 24/11/2026 23:50:46 UTC (00:50:46 CET); el reloj iba 3600449 ms atrasado (estrato 2, ida y vuelta 2,9 ms); puesto en hora
ECO: 1000000 octetos de ida y vuelta, iguales, en 0,80 s
Después de la red: 0 sockets (15 hechos), 0 procesos; IP 4750 recibidos y 6965 enviados, 0 malos; TCP: 7 conexiones, 1 repeticiones, 6 RST; UDP 4 y 4; 0 objetos
```

### Cómo se ha probado

`castel/nucleo64/pruebas.sh`: **110 pruebas, todas bien** (eran 105), unos
55 minutos con el servidor (de ellos, unos 20 de ensambla.py, que con el
núcleo de la etapa 9 —119.600 líneas— da muchas más pasadas). Lo de la etapa 9:

- **MRED en cada arranque** (con las demás de musl): 123 pruebas de las
  llamadas por 127.0.0.1, sin tarjeta: socket, bind (y el puerto que da),
  listen, connect y accept (bloqueantes y sin esperar: EINPROGRESS, poll
  POLLOUT y SO_ERROR), getpeername, 256 KB en las dos direcciones a la vez
  (dos hilos y poll), shutdown, ECONNREFUSED, EADDRINUSE, SO_REUSEADDR con
  TIME_WAIT, UDP (sendto y recvfrom con quién lo manda, MSG_DONTWAIT,
  connect, sendmsg y recvmsg con dos trozos), epoll de uno que escucha y de
  uno conectado, las opciones, getaddrinfo de `localhost`, eventfd, timerfd
  (con intervalo, poll y gettime) y el azar (`/dev/urandom`, `/dev/random`,
  getrandom: los 256 valores, distintos cada vez).
- **La red de verdad, con la de usuario de QEMU** (slirp: el anfitrión es
  10.0.2.2) y los servidores de prueba en el anfitrión (`servidores.py`:
  HTTP, HTTPS con una autoridad de prueba hecha con openssl, NTP con la hora
  adelantada una hora, eco, y un DNS de prueba en 127.0.0.1:53), cuatro
  vueltas: **e1000** con 1 procesador y DHCP; **e1000e** (MSI) con 2,
  `cmos=utc zona=UTC`; **virtio-net** con 4, IP fija (`ip=`) y el DNS de
  prueba; y **la Realtek** (la RTL8139C+ de QEMU con `realtek=ensayo`) con 8.
  En cada una: PING al anfitrión; IFCONFIG (las dos interfaces, la puerta,
  los DNS); DESCARGA de 1 MB por HTTP, a trozos (chunked) y por HTTPS (TLS
  1.2, el certificado comprobado con la autoridad de prueba y el nombre),
  **con el sha256 igual al del anfitrión**, y sin la autoridad, rechazado
  (código 4); con DNS, por su nombre (`anfitrion.prueba`) y uno que no
  existe (código 2); NTP (lo que iba atrasado: una hora, y puesto en hora);
  ECO de 1 MB de ida y vuelta contra el anfitrión, y de servidor (el
  anfitrión le habla por `hostfwd`); y después, ni un socket ni un objeto.
  La hora: en la primera vuelta (el CMOS de QEMU en la hora local del
  anfitrión, Europa/Madrid) sale la hora local con su zona, y NTP ve solo la
  hora adelantada del servidor; con `cmos=utc zona=UTC`, igual en UTC.
- Lo de las etapas 0 a 8, igual (la unidad en memoria lleva ahora 36
  archivos; uname 0.9.0, «etapa 9 (la red)»).
- Además, a mano, contra Internet de verdad (la red de usuario de QEMU sale
  por el servidor): `DESCARGA https://hispados.es/` (TLS 1.2 con ChaCha20,
  el certificado de verdad comprobado con las anclas de siempre),
  `DESCARGA http://hispados.es/` (que redirige a HTTPS), `NTP -n`
  (pool.ntp.org, estrato 1) y `PING -c 2 hispados.es`.

`SOLO=castel64` (con CASTEL64 y ENSAMB64 el núcleo sale igual, octeto a
octeto): **80 bien y 0 mal** (unos 85 minutos; dentro, `nucleo64/pruebas.sh`
con CASTEL64 y ENSAMB64: 111 bien y 0 mal). Para que CASTEL64 los encuentre, los
módulos se llaman con 8 letras como mucho (`objet64`, `redcom64`); y el del
azar, `chacha64` (`azar64.cas` es el programa de cuentas al azar de
pruebas64.sh).

Las imágenes de la carpeta del taller
(`Pruebas/Pendientes/VirtualBox/Hispa-64-etapa-9/`): el disco (`.vdi` y
`.img`) y el CD, con LEEME.TXT y LEEME-CD.TXT: la tarjeta de red de
VirtualBox **tal como viene** (NAT, «Intel PRO/1000 MT Desktop», la
82540EM), y qué probar desde el intérprete (la entrada 3): IFCONFIG, PING,
DESCARGA por HTTP y HTTPS y NTP. El disco, arrancado en QEMU por UEFI con la
e1000: DHCP, MRED, «todo en orden» y el escritorio.

### Lo que queda para la etapa 10 (y más adelante)

- **La red por USB** (la RTL8153 en CDC-ECM o NCM, y los demás adaptadores
  CDC): elegir la configuración de CDC (la 1 de la RTL8153 es la de Realtek),
  la variante 1 de la interfaz de datos, la recepción a granel sin tener el
  mutex del aparato mientras espera (como los informes de HID: pedir y
  mirar), y QEMU `-device usb-net` para las pruebas.
- **IPv6** (la etapa 10 lo pide para el navegador): ND, SLAAC, AF_INET6 en
  los sockets; y en TCP, la escala de ventana, SACK y los búferes mayores
  (para descargar deprisa por Internet), y guardar lo que llega desordenado.
- **Leer el `_PRT` del AML** para las INTx (VirtualBox, los equipos sin MSI):
  hoy se mira cada 10 ms y va, pero con 10 ms de más.
- **Las I219 de los portátiles**: el ULP (el PHY dormido al arrancar sin
  cable) y el K1, como e1000e; probarla en el taller.
- **El cliente SSH**: el de Hispa-Pentium (`castel/nucleo32/ssh/`) depende de
  su ventana y sus rutas; hay que separar la capa del terminal (con musl y
  el terminal de la consola).
- La **Wi-Fi** (Intel AX201/AX211), en paralelo, como dice el plan.
- `/etc/localtime` (TZif) para los programas que no miran TZ, y escribir la
  hora de NTP en el CMOS (con `cmos=` sabiendo cuál es).
- setitimer (38) y alarm, que MRED usa para su vigilante (hoy -ENOSYS).

---

## 16. La etapa 10, hecha la base (25/11/2026): lo que dejó la 9, C++, la letra y TCP de hoy

### Las decisiones

- **Primero lo que dejó la 9** (sección 15, «Lo que queda»), que es de la
  base y lo pide el navegador:
  - **La red por USB, CDC-ECM** (`usbred64.cas`): `Usb64.enumera` mira, si
    la primera configuración no es de una clase que se atienda (la de
    Realtek de la RTL8153, la RNDIS de `usb-net` de QEMU), si otra es de CDC
    Ethernet (clase 2, subclase 6), y se queda con esa; de la interfaz de
    datos (0x0A) lee la **variante 1** (la 0 no tiene extremos) y del
    descriptor de Ethernet, el texto de la MAC (iMACAddress). La clase:
    SET_INTERFACE, el filtro (SET_ETHERNET_PACKET_FILTER), **recibir con un
    pedido a granel siempre en marcha, sin el mutex del aparato** (como los
    informes de HID: pedir y mirar; al acabar, el xHCI señala también el
    suceso del hilo «red», `Ext.otro_aviso`: sin mirar cada 10 ms), mandar a
    granel con un octeto de más si la trama mide un múltiplo del paquete
    (como usbnet de Linux), y los avisos de enlace. Enchufar y desenchufar
    en marcha (el vigía USB se lo pasa a la red; el aparato desenchufado se
    queda marcado, como las memorias). NCM (la subclase 0x0D) no: la RTL8153
    y los adaptadores de casa traen ECM.
  - **setitimer, getitimer y alarm** (ITIMER_REAL): cada proceso lleva su
    vencimiento y su intervalo; el hilo del núcleo «alarmas» duerme hasta el
    primero (o hasta que lo cambien) y manda SIGALRM. ITIMER_VIRTUAL y
    ITIMER_PROF (tiempo de procesador) dan EINVAL. fork no la hereda.
  - **La hora de NTP, al CMOS** (clock_settime y settimeofday), en la hora
    que lleve (la local por omisión, como Windows; UTC con `cmos=utc`);
    `cmos=quieto`: no se toca. Y **/etc/localtime**: TZif de la versión 2,
    hecho al abrirlo, con un solo cambio (en 1901) y la zona de POSIX al
    final (musl la usa pasado el último cambio); musl lo mapea con `mmap`,
    así que los textos hechos al abrirlos se pueden mapear (una copia).
  - **Las INTx por el `_PRT`** (`aml64.cas`, `prt64.cas`): Aml64, en el
    mismo recorrido de las tablas (sin interpretar el AML), apunta los Name
    con paquetes de rutas, los `_PRT` (un Name, o un Method que nombra
    paquetes o los lleva dentro: «If (PICM) Return (AR00)…» de Intel y el
    `_PRT` con los paquetes escritos del HP Mini), los `_BBN` y los `_CRS`
    fijos; se queda con el paquete que se resuelve entero (las GSI
    escritas, o los enlaces de `_CRS` fijo: los GSIA-GSIH de QEMU q35); los
    enlaces de `_CRS` hecho por un método (los LNKA del modo PIC) no se
    resuelven y entonces va la línea de siempre (y mirando cada 10 ms).
    Prt64 sabe el bus de cada dueño (el `_BBN` del puente raíz, el bus
    secundario del puente de su `_ADR`) y, si un bus no tiene `_PRT`, sube
    girando la patilla. La e1000, la Realtek y virtio-net piden la GSI así
    (`pon_intx`); con MSI o con la GSI del `_PRT`, el hilo «red» espera de 50
    en 50 ms (los plazos de TCP, ARP y DHCP), no de 10 en 10. La que tenga
    otro vector no se pisa.
- **C++ con musl, con la libstdc++ del sistema** (`programas/cxx/`): no hay
  aquí fuentes de GCC para compilar una libstdc++ para musl, pero la del
  sistema (GCC 13, `libstdc++.a`, hecha para glibc) casi vale: `cxx.sh` es
  el «compilador cruzado» de C++ (como `musl-gcc` para C): las cabeceras de
  musl y las de la libstdc++, `puente.h` delante de todo y, al enlazar
  (estático), `libpuente.a` (`puente.c`) con lo que esa libstdc++ espera de
  glibc y musl no tiene: `__GLIBC_PREREQ` (como la 2.34: todo pthread en la
  biblioteca de C, sin los débiles; pero sin las `*f128`), el `locale_t`
  de glibc (la libstdc++ mira dentro: `__ctype_b`, `tolower`, `toupper`;
  `newlocale` y `duplocale`, envueltas con `--wrap`, dan un bloque con los
  6 punteros de musl delante y las tablas de musl —las mismas que las de
  glibc— donde las busca glibc), `strerror_r` de GNU (envuelta), los
  destructores de `thread_local` (`__cxa_thread_atexit_impl`), las esperas
  con reloj de la 2.30 (`pthread_cond_clockwait`…), las `_chk` de
  `_FORTIFY_SOURCE` (comprobando), las de LFS y `arc4random`. Es un puente
  (honesto: depende de la libstdc++ del sistema); lo de fondo es compilar
  GCC para `x86_64-linux-musl` (la 10 b).
- **La letra: FreeType y HarfBuzz** (en `terceros/`, sin cambiar nada:
  FreeType 2.13.3 con la GPL versión 2 de las dos que da; HarfBuzz 10.1.0,
  Old MIT), compilados en `build/` la primera vez (FreeType con musl,
  módulo a módulo; HarfBuzz de una vez, `harfbuzz.cc`, con `cxx.sh`, con
  FreeType y los hilos de pthread). Y DejaVu Sans entera (la original, para
  GSUB, GPOS y kern: las de Hispa-Pentium están recortadas).
- **TCP de hoy** (`tcp64.cas`, lo que el navegador pide para bajar deprisa
  por Internet): la **escala de ventana** (RFC 7323: la nuestra, 3) con
  búferes de **256 KB**; **lo desordenado se guarda** en su sitio del búfer
  de recibir (detrás de lo que va en orden: 8 trozos) y se junta al llegar
  lo que faltaba; **SACK** (RFC 2018) en los ACK sin datos (con datos, la
  trama se pasaría de la MTU) si la otra punta lo ofrece. Al mandar, los
  SACK de la otra punta no se miran (se repite desde lo primero sin
  confirmar). `red=pierde` tira uno de cada 23 segmentos con datos que
  llegan, para probarlo. La red de usuario de QEMU (slirp) no sabe de escala
  ni de SACK: eso se prueba de Hispa-64 a Hispa-64 (MRED, por 127.0.0.1).
- **Los límites de un proceso**: 512 hilos (eran 64: el navegador, a
  cientos), 256 descriptores (eran 32; `select`, hasta el 63: lo demás con
  `poll`) y 1024 regiones de memoria (eran 256).

### Qué hace

Hispa-64 0.10.0. Los módulos y programas nuevos y lo que cambia:

| Archivo | Qué |
|---|---|
| `usbred64.cas` | la red por USB, CDC-ECM (arriba) |
| `prt64.cas` | las INTx por el `_PRT`: el bus de cada dueño, girar la patilla, `pon_intx`, quién tiene cada GSI |
| `aml64.cas` | los `_PRT`, `_BBN` y `_CRS` fijos, en el mismo recorrido (`busca_prt`) |
| `usb64.cas` | elegir la configuración de CDC-ECM, la variante 1 de los datos, la MAC; `texto_de` |
| `xhci64.cas` | `Ext.otro_aviso`: otro suceso que señalar al acabar una transferencia |
| `ip64.cas` | las tarjetas por USB (T_USB); el hilo «red», de 50 en 50 ms si nadie tiene que mirar |
| `tcp64.cas`, `redcom64.cas` | la escala de ventana, lo desordenado, SACK, `red=pierde` |
| `hora64.cas`, `reloj64.cas` | escribir en el CMOS (BCD o binario, 12 o 24 horas, el siglo); /etc/localtime (`tzif`) |
| `procesos.cas`, `sistema.cas` | setitimer, getitimer, alarm (el hilo «alarmas»); los límites; `mmap` de los textos |
| `intel64.cas`, `rtl64.cas`, `virtio64.cas` | la INTx con `pon_intx` |
| `prt/miraprt.cas` | MIRAPRT (modelo linux64): los `_PRT` de unas tablas de verdad, como los ve el núcleo; `prt/esperado.txt`, lo que da con las del ASUS, el Toshiba y el HP Mini |
| `programas/cxx/` | `cxx.sh`, `puente.h`, `puente.c`; MCXX (C++17) y MTEXTO (HarfBuzz y FreeType); `hazcxx.sh` |

Lo que dice al arrancar (QEMU, q35, la e1000 y un `usb-net`):

```
INTx: las rutas de ACPI (_PRT): 1 tablas resueltas, 128 entradas
Red: 00:02.0 8086:100E Intel 82540EM (PRO/1000 MT Desktop, e1000), MAC 52:54:00:12:34:56, INTx por la GSI 22 (del _PRT)
Red: USB 1-5 0525:A4A2 «RNDIS/QEMU USB Network Device», CDC-ECM (la configuración 1 de 2), MAC 40:54:00:12:34:56, a granel por los extremos 0x82 y 0x02 (de 64 octetos), con las interrupciones del xHCI
MCXX: todo bien (35 pruebas)
  hilos: 200 std::thread, thread_local (200 destruidos), condition_variable, async, promise, timed_mutex
MTEXTO: HarfBuzz 10.1.0 sobre FreeType, con /DEJAVU.TTF
  formas: 33 glifos para 42 letras; «fino», 3 glifos (sin la ligadura, 4); «AV», 2671 y 2802 (1/64 de punto, con y sin kerning)
  dibujado: 33 glifos en 640x64, 557 de avance; tinta 945961, 2504 puntos grises; suma FNV-1a 0xC23E97E8
  TCP [etapa 10]: 14 conexiones con escala de ventana, 637 trozos desordenados guardados y 25 juntados, 946 ACK con SACK (red=pierde: uno de cada 23 con datos, tirado)
  la hora, puesta 1 veces; el CMOS, escrito 1 veces: va 0 s del reloj (en hora local)
```

### Cómo se ha probado

`castel/nucleo64/pruebas.sh`: **118 pruebas** (eran 110); en la última vuelta,
117 bien y 1 mal **intermitente** (abajo); en la de antes, todas las demás bien
(los dos fallos de aquella, de las propias pruebas, arreglados). Unos 65
minutos con el servidor cargado. Lo de la etapa 10:

- **MSENALES** (38 pruebas: eran 26): alarm (al segundo, lo que quedaba,
  quitarla), setitimer con intervalo (cinco SIGALRM), getitimer, ITIMER_VIRTUAL
  (EINVAL) y que el hijo de fork no la hereda. **MRED** (126: eran 123):
  /etc/localtime es TZif 2 con la zona de TZ al final, y `localtime` sin TZ
  da lo mismo que con ella (en invierno y en verano).
- **MCXX** (35 pruebas, en cada arranque con musl): lo global y
  `thread_local` con su destructor, las excepciones (entre funciones, por
  su base, `exception_ptr`, `out_of_range`, `bad_alloc`, `throw;`), RTTI y
  `dynamic_cast`, los contenedores y algoritmos, `optional` y `variant`,
  `string_view`, los flujos (`ifstream` de /SALUDO.TXT), `regex`, **200
  `std::thread` a la vez**, `condition_variable::wait_for`, `async`,
  `promise`, `timed_mutex`, `shared_ptr` y `weak_ptr`, 20.000 `new`.
- **MTEXTO**: la ligadura «fi» (GSUB), el kerning de «AV» (GPOS) y el texto
  dibujado con suavizado; **la suma del lienzo, la misma que da el mismo
  programa en Linux** (pruebas.sh lo corre en el anfitrión y compara).
- **La red por USB**: con `usb-net` detrás del xHCI de QEMU, como las demás
  tarjetas (DHCP, PING, IFCONFIG, DESCARGA por HTTP y HTTPS con su sha256,
  NTP, ECO de ida y vuelta y de servidor), y que elige la configuración de
  ECM (la 2.ª: la 1.ª es RNDIS) con la variante 1 y la MAC; y **enchufada y
  desenchufada en marcha** (device_add y device_del por el monitor): DHCP
  le da la IP, y al quitarla, sin enlace.
- **El `_PRT`**: MIRAPRT (en Linux) con las tablas de verdad del ASUS
  GL553VD (24 tablas: AR00 de 44 entradas y las de sus puentes), el Toshiba
  NB250 (6) y el HP Mini 5103 (6, con los paquetes dentro del método), igual
  que `prt/esperado.txt`; en QEMU q35, la e1000 por la GSI del `_PRT` (los
  GSIA-GSIH), con sus interrupciones y sin mirar cada 10 ms; con «pc» (los
  LNKA), la línea y mirando.
- **La hora**: después de NTP, el CMOS va con el reloj (0 s, en hora local
  y en UTC).
- **TCP**: con la e1000e y `red=pierde` (slirp manda; Hispa-64 guarda lo
  desordenado y lo junta: las descargas, con su sha256); y de Hispa-64 a
  Hispa-64 (MRED por 127.0.0.1 con `red=pierde`): la escala de ventana por
  las dos puntas, lo desordenado, los ACK con SACK, y los 256 KB de ida y
  vuelta, iguales.
- Lo de las etapas 0 a 9, igual (la unidad en memoria lleva 39 archivos;
  uname 0.10.0, «etapa 10 (C++ y la red de hoy)»).

`SOLO=castel64`: **79 bien y 1 mal**: CASTEL64 y ENSAMB64 sacan el núcleo de
la etapa 10 igual, octeto a octeto; el «mal» es `nucleo64/pruebas.sh` dentro,
con un solo fallo intermitente (abajo).

**Dos fallos intermitentes** (con el servidor cargado; cada uno ha salido una
vez y bien en las demás vueltas, y repetidos a mano salen bien): MDISP en
el CD por BIOS («lo copiado está en la pantalla»: la consola dibuja a la vez
en la misma pantalla) y el intérprete (una tecla mandada mientras CUENTA
aún escribe sale antes de su línea). Son de la etapa 8 y la 6; quedan
apuntados abajo.

Las imágenes de la carpeta del taller
(`Pruebas/Pendientes/VirtualBox/Hispa-64-etapa-10/`): el disco (`.vdi` y
`.img`) y el CD, con LEEME.TXT: qué probar en VirtualBox (la red como
viene, ahora por la GSI del `_PRT`: sin mirar cada 10 ms; MCXX y MTEXTO en
el arranque; NTP y el CMOS).

### Lo que queda para la 10 b y la 11

- **El navegador mismo** (Ladybird o WPE WebKit) con su ventana sobre el
  compositor; antes, **ICU** (Unicode, el corte de líneas) y **curl** (con
  BearSSL), y **IPv6** (ND, SLAAC, AF_INET6).
- **La libstdc++ compilada para musl** (las fuentes de GCC: un
  `x86_64-linux-musl-g++` de verdad), para quitar el puente de `cxx.sh`; y
  los programas de C++ dinámicos (libstdc++.so).
- TCP: mirar los SACK de la otra punta al repetir (hoy, desde lo primero
  sin confirmar); SACK también en los segmentos con datos; el control de
  congestión (hoy no hay ventana de congestión: se manda lo que deje la otra
  punta).
- La red por USB: NCM (la subclase 0x0D), y la RTL8153 con su configuración
  propia (r8152) si un día hace falta; probarla en el taller con el
  adaptador de Carlos (la de la etapa 9 mataba el teclado del MacBook).
- `_PRT`: las INTx de los demás drivers (xHCI, AHCI y NVMe sin MSI) con
  `pon_intx`; los enlaces de `_CRS` hecho por un método, con ACPICA (11).
- **Un fallo del planificador** que salió al hacer las alarmas: con **un
  solo procesador**, un hilo del núcleo INTERACTIVO despertado (`señala`)
  desde la llamada al sistema de un programa le quita el procesador y el
  programa **no vuelve** de la llamada (se reproduce con alarm(0) si el
  hilo «alarmas» es INTERACTIVO: por eso es NORMAL). El hilo «red» es
  INTERACTIVO y no lo hace (sus avisos llegan con otro camino); hay que
  encontrar la causa de fondo en Hilos64 (la expropiación al volver de
  `syscall`).
- Los dos intermitentes de las pruebas (MDISP y la pantalla de la consola
  a la vez; las teclas que se adelantan en el intérprete): que MDISP pare la
  consola mientras mira, y que la prueba del intérprete espere al indicador
  antes de cada orden.
- ITIMER_VIRTUAL e ITIMER_PROF (contar el tiempo de procesador de cada
  hilo); `select` de más de 64 descriptores.
- Lo de la sección 15 que sigue: las I219 en el taller (ULP y K1), el
  cliente SSH, la Wi-Fi en paralelo.

## 17. La etapa 11, hecha la base (25/11/2026): el planificador, los intermitentes, TCP con congestión, la INTx, el tiempo de procesador y el sonido

### Las decisiones

- **Primero, lo de fondo** (lo que dejó la 10 en la sección 16):
  - **El fallo del planificador.** La causa: `despierta_todos` era
    «mientras haya alguien esperando, despierta al primero», y el que
    despertaba a uno de más prioridad le daba el procesador **en el acto**
    (en `encola`). Con un procesador, el hilo «alarmas» (INTERACTIVO)
    despertado por `alarm(0)` corría, volvía a esperar en **el mismo
    suceso** (espera en bucle) antes de que el bucle de `despierta_todos`
    diera la vuelta, y el bucle lo encontraba otra vez: sin fin, y el
    programa no volvía de la llamada (con el procesador en el núcleo, sin
    interrupciones: lo enseñó «info registers» del monitor de QEMU, en
    `despierta_uno`, `encola` y `carga_cr3`). El hilo «red» no lo hacía
    porque lo despiertan las interrupciones, que no expropian en el acto.
    El arreglo, en Hilos64, en dos partes:
    1. `despierta_todos` despierta **a los que esperan ahora**, de una vez,
       con el cerrojo de la espera tomado (y `despierta_uno` también, como
       ya hacía Sondeo64: el despertado no puede irse ni soltarse entre
       sacarlo de la cola y despertarlo); uno que vuelve a esperar espera
       al cerrojo y queda para la próxima.
    2. **Los puntos de expropiar** (`expropia_aquí`, como el
       `need_resched` de Linux): despertar a uno que pasa por delante solo
       lo apunta (`expropiar`), y se le da el procesador después, en un
       punto seguro: al soltar el cerrojo de la espera (un hilo del núcleo),
       **al acabar la llamada al sistema** (el hilo de un programa: nunca en
       medio de ella) y **al volver de una interrupción al anillo 3** (si la
       interrupción ha despertado a uno, sin esperar al tic). Con las
       interrupciones quitadas, nunca.
    Con eso, «alarmas» va como INTERACTIVO.
  - **MDISP en el CD por BIOS** («lo copiado está en la pantalla», a
    veces). La causa: el hilo «pantalla» mira `modo_gráfico` al empezar a
    dibujar, y una vuelta ya empezada (toda la pantalla tras subir una
    línea: milisegundos, o más si se le quita el procesador a medias)
    seguía pintando encima de lo del programa que acababa de tomar
    /dev/fb0. Ahora `pon_modo_gráfico(cierto)` toma el mutex del pintor:
    al volver, nadie pinta.
  - **La tecla que se adelanta en el intérprete.** La causa: la prueba
    tecleaba la orden siguiente mientras la de antes (CUENTA) aún
    escribía, y el eco de la terminal (como en Linux) la ponía en medio de
    su salida. No es un fallo del sistema sino de la prueba: `teclas.py`
    sabe ahora `{indicador}` (espera a que el intérprete dé otra vez su
    indicador, mirando lo que sale por el puerto serie), y la prueba
    teclea cada orden al ver acabada la de antes, como una persona.
- **TCP al mandar** (`tcp64.cas`): el **control de congestión NewReno**
  (RFC 5681 y 6582: 10 segmentos al empezar, RFC 6928; el arranque lento y
  la evitación de la congestión; la recuperación rápida al tercer ACK
  repetido, con la ventana a la mitad e inflada en cada ACK repetido, los
  ACK parciales y el final al confirmarse todo lo de antes de la pérdida; al
  vencer el plazo, un segmento) y **los SACK de la otra punta** (RFC 2018 y
  lo de RFC 6675: lo que dice tener se apunta, y en la recuperación se
  repiten solo los huecos, no todo desde lo primero sin confirmar). SACK
  también en los segmentos con datos (con menos datos: la MSS no cuenta
  las opciones, RFC 6691). CUBIC, no: NewReno basta para la red de casa y
  es lo que piden las RFC; se puede cambiar en `confirma` sin tocar lo demás.
- **La INTx del `_PRT` para el xHCI, la AHCI y el NVMe** sin MSI
  (`Prt64.intx_prt`): solo si el `_PRT` dice su GSI y nadie más la tiene
  (una línea que no se sabe si es la buena, por nivel, podría no apagarse
  nunca); si no, se mira la cola, como antes. Por nivel: el xHCI y la AHCI
  la apagan al borrar sus avisos; el NVMe, con INTMS al llegar e INTMC
  antes de dormir el que espera. La que llega muchas veces seguidas sin
  nada (20.000) se tapa (`intx_vacía`). Y `pon_intx` ya no pisa la GSI de
  otro vector (antes, las interrupciones del otro iban a parar al nuevo).
- **`select` y `poll` de más de 64**: `fd_set` de 1024 (FD_SETSIZE; los
  descriptores abiertos llegan a 256) y `poll` de hasta 1024 entradas,
  mirándolas de 64 en 64.
- **El tiempo de procesador**: el tic sabe si ha llegado en el anillo 3
  (la entrada lo apunta en el bloque del procesador, `desde_usuario`, y la
  vuelta lo borra) y se lo cuenta al hilo y a su proceso (en el programa o
  en el núcleo, de 10 en 10 ms). Con eso: **ITIMER_VIRTUAL** (SIGVTALRM) e
  **ITIMER_PROF** (SIGPROF), que vencen en el tic y se mandan al volver al
  programa; `getrusage` (RUSAGE_SELF, RUSAGE_CHILDREN con los hijos
  recogidos, RUSAGE_THREAD) y el de `wait4`; CLOCK_PROCESS_CPUTIME_ID y
  CLOCK_THREAD_CPUTIME_ID (antes daban el tiempo en marcha).
- **De la 11 del plan, el sonido**: **HD Audio en el núcleo** (`hda64.cas`,
  lo de Hispa-Pentium —`nucleo32/sonido/hda.c`, que viene de HSPSND de
  Hispa-DOS— en Castel: el enlace, las órdenes inmediatas o CORB y RIRB,
  las GPIO, el camino de cada pin que suena hasta un DAC por mezcladores y
  selectores, EAPD) con **/dev/dsp como OSS** (lo más sencillo de POSIX
  para los programas: `write` de muestras de 16 bits; SETFMT, SPEED,
  CHANNELS, STEREO, SYNC, RESET, GETOSPACE, GETODELAY, GETBLKSIZE,
  GETFMTS): 48 y 44,1 kHz (los que tienen todas las tarjetas, sin
  remuestrear), mono (se dobla) o estéreo; uno a la vez (EBUSY). El
  anillo de 64 KB (341 ms) va por delante de por dónde va la tarjeta, y lo
  ya tocado se pone a cero en cuanto se ve: si el programa no da más,
  suena silencio y no lo de la vuelta anterior. Sin la interrupción de la
  tarjeta (el hilo «sonido» mira cada 10 ms), como en Hispa-Pentium. El
  flujo no se para al cerrar sino tras medio segundo de silencio: lo que la
  tarjeta ya ha leído (la de QEMU, unos 30 ms) tiene que salir.
- **Lo demás de la 11 del plan** (imprimir con IPP, suspender, la batería,
  el brillo, HWP) pide ACPICA (los métodos de AML) o mDNS e IPP: queda
  para la 11 b. Y de la 10 b, el navegador, ICU, curl, IPv6 y la libstdc++
  para musl siguen allí (el plan los pone en la 10 b: no se empiezan sin
  dejar cerrado lo de antes).
- **Con KVM** (lo encontró otra sesión con el disco de la etapa 10: con KVM
  se paraba siempre tras HOLACAS con un doble fallo; sin KVM, no). La
  causa: STAR decía 0x10 para el anillo 3, sin el RPL 3; `sysret` pone en
  SS ese + 8, y en los AMD tal cual: SS quedaba 0x18 (RPL 0) en el anillo
  3, y el `iretq` de vuelta de la primera interrupción que lo guardaba (el
  fallo de página de MEMORIA en su primera escritura) daba #GP(0x18); el
  #GP se atendía como del núcleo, fallaba otra vez, y así hasta desbordar
  la pila (lo enseñaron un punto de parada de gdb en `e13` y la pila del
  hilo). El TCG de QEMU no lo mira; los Intel ponen el RPL 3 solos (en
  VirtualBox con VT-x de Intel no pasaba). Ahora STAR lleva 0x13, como
  Linux. Y la batería tiene vueltas **con KVM** (si hay /dev/kvm): `-cpu
  host` con 4 procesadores, el de QEMU por omisión con uno, y por UEFI.
  De paso: con «sinred» no se ponía ni «lo», y un `connect` a 127.0.0.1
  esperaba para siempre; ahora «sinred» deja «lo» y el hilo «red».
- **El cuadro «¿Guardar los cambios?»** de Pluma, Cuentas y Pintura (el
  código de Hispa-Pentium, que se compila igual aquí): lo alto del cuadro
  se calculaba con el alto de la letra y de los controles (U y H) antes de
  que `nuevo_cuadro` los pusiera (los argumentos se calculan antes), así
  que, si era el primer cuadro, con 0: los botones tapaban la pregunta.

### Qué hace

Hispa-64 0.11.0. Los módulos y programas nuevos y lo que cambia:

| Archivo | Qué |
|---|---|
| `hilos64.cas` | `despierta_todos` y `despierta_uno` con el cerrojo de la espera; `expropia_aquí` (los puntos de expropiar); el tiempo de procesador de cada hilo en el tic |
| `sistema.cas`, `procesos.cas` | expropiar al acabar la llamada y al volver de una interrupción al programa; «alarmas», INTERACTIVO; ITIMER_VIRTUAL y PROF, getrusage, CLOCK_*_CPUTIME_ID; `select` y `poll` de más de 64 |
| `entradas.cas`, `porcpu64.cas` | STAR con el RPL 3 (KVM en AMD); la entrada apunta si la interrupción llega del anillo 3 (`desde_usuario`) |
| `pant64.cas` | la consola deja la pantalla con el mutex del pintor |
| `tcp64.cas`, `redcom64.cas` | NewReno, los SACK de la otra punta y los huecos; SACK con datos |
| `prt64.cas`, `msi64.cas`, `xhci64.cas`, `ahci64.cas`, `nvme64.cas` | la INTx del `_PRT` (`intx_prt`, `intx_vacía`, CON_INTX); `pon_intx` no pisa la GSI de otro |
| `hda64.cas` | HD Audio (nuevo) |
| `disp64.cas` | /dev/dsp (OSS) |
| `ip64.cas`, `hispa64.cas` | «sinred» con «lo» |
| `prsmp64.cas` | «Despertar en bucle» |
| `programas/musl/msonido.c` | MSONIDO (nuevo); MSENALES con ITIMER_VIRTUAL y PROF; MPROCESOS con select y poll de 100 tuberías |
| `programas/teclas.py` | `{indicador}` y `--serie` |
| `nucleo32/oficina/{pluma,cuentas,pintura}/dialogos.c` | el cuadro «¿Guardar los cambios?» |

Lo que dice al arrancar (QEMU, q35, `intel-hda` y `hda-output`):

```
Sonido: 00:02.0 8086:2668, HD Audio (GCAP 4401), el códec 1AF4:0012 (con las órdenes inmediatas); el pin 3 (línea) desde el DAC 2; /dev/dsp
Despertar en bucle: uno interactivo que espera un suceso en bucle, señalado 1000 veces por uno normal en el procesador 0: despierta 1000 veces y pasa por delante (1002 expropiaciones), en 6 ms
  ITIMER_VIRTUAL y ITIMER_PROF: SIGVTALRM y SIGPROF al gastar el procesador (50 ms medidos con CLOCK_PROCESS_CPUTIME_ID)
  select y poll de 100 tuberías (descriptores hasta el 201)
  TCP [etapa 11]: 26 recuperaciones rápidas (NewReno), 26 acabadas; 237 ACK con SACK leídos, 0 huecos repetidos por ellos; la ventana de congestión, hasta 28 KB
MSONIDO: todo bien (15 pruebas)
Interrupciones de los discos: NVMe 62 (0 esperas sin aviso), AHCI 4412 (0 sin aviso); GSI tapadas por no apagarse: 0   (sinmsix sinmsi)
```

### Cómo se ha probado

`castel/nucleo64/pruebas.sh`: **124 pruebas** (eran 118). Tres vueltas
seguidas con el servidor cargado (otras sesiones con QEMU y KVM, carga de
4 a 6 en 4 procesadores), cada una con 122 bien y 2 mal, **y los mal,
distintos cada vez y todos del reloj del TCG de QEMU bajo carga**: con
`ocioso=mwait`, el interactivo se pasó de media 32 ms (el límite, 25); sin
HPET, 43 tics en 1 s (el mínimo, 50); por UEFI con `-cpu max`, 31 tics en 1
s. Lo demás de esos mismos arranques, bien. Y dos de las propias pruebas,
arreglados por el camino: el intérprete buscaba «a medias^C» con el `^`
sin escapar (el grep de hoy lo toma por ancla), y una conexión con
virtio-net que no se cerraba a tiempo (en la recuperación rápida no se
repetía el FIN: arreglado en `tcp64.cas`). `SOLO=castel64`: **79 bien y 1
mal** (CASTEL64 y ENSAMB64 sacan el núcleo de la 11 igual, octeto a octeto;
el mal, `nucleo64/pruebas.sh` dentro, con la conexión de virtio-net de
antes del arreglo del FIN). Unos 75 minutos cada vuelta. Lo de la
etapa 11:

- **El planificador**: «Despertar en bucle» con 1, 2, 4 y 8 procesadores
  (1000 despertares, una expropiación por señal); con el `despierta_todos`
  de antes, el sistema se queda ahí (probado a propósito: no acaba). Y
  MSENALES (alarm, setitimer) con «alarmas» INTERACTIVO y 1 procesador.
- **El intérprete**: cada orden tecleada al ver el indicador, y el
  teclado USB igual.
- **TCP**: de Hispa-64 a Hispa-64 por 127.0.0.1 con `red=pierde`: las
  recuperaciones rápidas (NewReno) y que acaban, los ACK con SACK leídos,
  la ventana de congestión, y los 256 KB de ida y vuelta, iguales; las
  descargas con pérdidas (e1000e), con su sha256.
- **La INTx**: q35 con `sinmsix sinmsi`: los dos NVMe, la AHCI y el xHCI
  por la GSI de su `_PRT`, con sus interrupciones, ARCHIVOS en los siete
  volúmenes y fsck.fat y miraexfat.py desde el anfitrión; el segundo xHCI
  de la prueba de dos controladoras, por su INTx.
- **select y poll** de 100 tuberías (MPROCESOS: 28 pruebas) e
  **ITIMER_VIRTUAL y PROF**, getrusage y CLOCK_PROCESS_CPUTIME_ID
  (MSENALES: 48), estáticos y dinámicos, con 1, 4 y 8 procesadores.
- **El sonido**: `intel-hda` con `hda-output` (2 procesadores) e
  `ich9-intel-hda` con `hda-duplex` (1): MSONIDO (15 pruebas) y lo que
  graba QEMU (`-audiodev wav`), mirado con `nucleo32/sonido/oye.py`: dos
  ráfagas, 1002 ms a 440 Hz y 501–504 ms a 1000 Hz (por los pasos por cero,
  435 y 985–995), **sin cortes**, al nivel escrito (9000).
- **Con KVM** (nuevo, si hay /dev/kvm): `-cpu host` con 4 procesadores
  (las etapas 4 y 7 enteras), el procesador por omisión con 1 y «sinred»
  (MRED por «lo»), y por UEFI con el disco de la imagen: todo en orden.
  Con el núcleo de la 10, el primero da el doble fallo de siempre.
- Lo de las etapas 0 a 10, igual (la unidad en memoria lleva 40 archivos;
  uname 0.11.0).

### Lo que queda para la 11 b (y más adelante)

- **De la 11 del plan**: imprimir (IPP Everywhere: mDNS para descubrir,
  mandar PDF), suspender (S3 o S0ix), la batería, el brillo y HWP/P-states:
  piden **ACPICA** (los métodos de AML: `_BST`, `_BCM`, `_PTS`...), que es
  lo siguiente de fondo; con ella, también los enlaces `_CRS` hechos por un
  método (los LNKA del modo PIC) para la INTx.
- **El sonido**: la mezcla de varios programas a la vez (hoy, uno con
  /dev/dsp; EBUSY para el segundo) y el volumen (en la mezcla, como en
  Hispa-Pentium), los auriculares (mirar si hay algo enchufado y callar los
  altavoces), los Cirrus de los Mac y SONIDO.INI de Hispa-Pentium; la
  entrada (el micrófono); probarlo en el taller (el Realtek del ASUS, el del
  Toshiba). Con KVM, la grabación de QEMU (`-audiodev wav`) pierde a veces
  hasta 130 ms del final de un trozo antes de cambiar la frecuencia (el
  flujo se para y se vuelve a poner): sin KVM, no; por mirar si es lo que
  guarda la tarjeta de QEMU con su temporizador (`use-timer`) o algo
  nuestro.
- **La expropiación al salir de una interrupción en el núcleo**: hoy, la
  que despierta a uno que pasa por delante mientras corre un hilo del
  núcleo le da el procesador en el tic siguiente (como mucho 10 ms); el
  aviso del otro procesador y la vuelta al anillo 3, ya en el acto.
- **El tiempo de procesador**, de 10 en 10 ms (el tic); más fino, con el
  TSC al cambiar de hilo. Y el de un fallo de página del programa cuenta
  como suyo (Linux lo cuenta del núcleo).
- **CUBIC** (si hiciera falta para enlaces largos y rápidos) y SACK al
  repetir tras vencer el plazo (hoy se olvida lo de SACK y se repite todo
  con un segmento de ventana).
- **De la 10 b**, sin empezar (el plan los pone allí): el navegador
  (Ladybird o WPE WebKit), ICU, curl, **IPv6** y la libstdc++ compilada
  para musl.
- Lo de la sección 15 que sigue: las I219 en el taller, el cliente SSH, la
  Wi-Fi en paralelo.

## 18. La etapa 11 b, hecha (25/11/2026): el tic estable, ACPICA, la batería, el brillo y HWP

### Las decisiones

- **Primero, el tic** (los mal de la 11, «sin HPET, 43 tics en 1 s (mínimo
  50)», «UEFI -cpu max, 31 tics en 1 s», «el interactivo, de media 32 ms»).
  No eran del reloj de QEMU, sino nuestros: **el temporizador del APIC se
  calibraba mal**. `mide()` ponía la cuenta y leía después la hora, y lo que
  pasara entre las dos (el TCG traduciendo el código la primera vez, o el
  anfitrión cargado sin darle su hilo al procesador) se contaba como del
  temporizador: salía cada 13,9 ms en vez de 10 (72 tics en 1 s en un
  arranque cualquiera, y menos con el servidor más cargado). Por eso nunca
  salían tics «de más», solo de menos. Ahora la cuenta del APIC, el TSC y
  la hora se leen **juntas** (la hora antes y después de la cuenta, la vez
  que menos hay entre las dos) y sobre 20 ms: 625.000 cuentas cada 10 ms
  (las de QEMU, exactas) con y sin carga. Igual la frecuencia del TSC.
- **Lo que sí es del anfitrión, contado** (no los límites subidos a ciegas):
  si el procesador está parado desde fuera (el anfitrión que no le da su
  hilo; en un equipo de verdad, el SMM), los tics que vencen mientras se
  juntan en uno (el APIC no guarda más de uno pendiente). El tic lo ve con
  el TSC: el temporizador va a su paso, así que desde el último tic que
  llegó a su hora («el ancla») tenían que haber llegado redondeo((ahora −
  ancla) / 10 ms), y los que faltan se han perdido; uno que llega tarde pero
  llega no cuenta (la primera versión contaba cada hueco de más de tic y
  medio, y un tic tarde seguido de uno a su hora salía perdido: 95 + 12 una
  vez). Va en el bloque del procesador (`perdidos`, `huecos`). La etapa 2 dice ahora «Tic: N tics en 1
  s (...), M que no han llegado (H huecos); entre dos cambios del segundo del
  CMOS: ...; el temporizador, cada 10,00 ms (contra el TSC)», y la batería
  pide: **el periodo contra el TSC de 9,80 a 10,20 ms** (lo programado de
  verdad: la relación tic/TSC, que no depende de la carga), los que llegan
  más los que no, **de 95 a 106** en 1 s (de 90 a 110 entre dos segundos
  del CMOS), y que lleguen al menos 30 (90 con KVM). El interactivo de la
  etapa 3 dice también lo que se pasa **sin esos tics** (lo que no es del
  planificador), y eso es lo que se mide (25 ms como mucho).
- **Con KVM, lo principal**: la vuelta de KVM con 4 procesadores pide ahora
  también el tic (90 o más) y la etapa 3 entera (el interactivo incluido).
- **MSENALES con KVM** («getrusage: el tiempo del programa (más de 80 ms)»,
  una vez): con KVM leer el HPET (`clock_gettime`) es salir a QEMU, y mucho
  del rato de ITIMER_PROF es del núcleo. Ahora se pide lo que de verdad se
  sabe: 50 ms del programa como poco (ITIMER_VIRTUAL) y, con lo del núcleo
  por él, 140 (50 + los 90 de ITIMER_PROF) menos lo que se va en contar de
  10 en 10 ms (110).
- **ACPICA, en el núcleo** (no como servicio del anillo 3, como ACPI.BIN de
  Hispa-Pentium). Lo piden cosas del núcleo y en el arranque: las rutas de
  las INTx antes de las tarjetas, apagar después de vaciar los discos y,
  luego, suspender; un servicio llegaría tarde y necesitaría llamadas nuevas
  para los puertos, la memoria física y la configuración PCI. Es lo que hace
  Linux. **El mismo ACPICA** (R2025_04_04, `acpi/acpica/`, bajo la GPL-2.0
  de su licencia triple) y **el mismo núcleo de C** de Hispa-DOS y
  Hispa-Pentium (`acpi/nucleo/`: la memoria, el registro, el controlador
  embebido, la batería y el cable), compilados para 64 bits (`-mcmodel=kernel
  -mno-red-zone -mgeneral-regs-only`) con lo de `acpi/hispa64/`: la entrada
  y la cabecera, la «máquina» (los puertos directos; la memoria física por
  el mapa directo o `mapea_disp`; la configuración PCI por Pci64; dormir y
  la hora, del núcleo) y las llamadas. Sale **ACPICA64.BIN** (161 KB), un
  binario plano enlazado para 0xFFFFFFFFC0000000, que va en la unidad en
  memoria: `acpica64.cas` lo copia ahí (el código de solo leer, los datos
  sin ejecutar), le da 4 MB de montón y lo llama siempre con su mutex (un
  hilo dentro cada vez: `ACPI_SINGLE_THREADED`), en la pila del hilo que
  llama; las funciones del núcleo, por `castel0`..`castel5` (los argumentos
  por la pila, como Castel). Sin el archivo, o si ACPICA no arranca, se
  sigue como antes (lo de las tablas, a ojo; «sinacpica» lo quita).
  Los cambios en `acpi/nucleo/` no cambian Hispa-DOS ni Hispa-Pentium (el
  simulador da lo mismo, octeto a octeto, con el Toshiba): los números de
  las direcciones (`ufis`, `uptr`), `achispa.h` con la rama de 64 bits,
  `bateria_arranca` y `bateria_lectura` por separado, y un gancho para los
  demás avisos (`nucleo_aviso`).
- **Lo que hace con ella**:
  - **el arranque**: las tablas, el espacio de nombres, los `_INI`, el
    controlador embebido (su `_REG`), `\_PIC(1)` (el modo APIC), a `_OSI`
    que sí a todos los Windows (como Linux);
  - **la batería y el cable**: `_STA`, `_BIF`/`_BIX` (con los ciclos),
    `_BST` y `_PSR`, con los Notify del equipo; el hilo «acpi» mira los
    avisos del controlador embebido (`_Qxx`) cada 250 ms y lee la batería
    cada 10 s o en cuanto el equipo avisa;
  - **el botón de encendido** que avisa con Notify (PNP0C0C, el de muchos
    portátiles, por el controlador embebido): como el «fijo» de la FADT,
    que sigue por la SCI;
  - **apagar de verdad**: `AcpiEnterSleepStatePrep(5)` (`\_S5` evaluado,
    `_PTS(5)`, `_SST`) y `AcpiEnterSleepState(5)`; si sigue encendido, lo de
    antes (`\_S5` buscado a ojo);
  - **el `_CRS` de los enlaces de los `_PRT`** que la 10 dejó a medias: las
    tablas que Aml64 no resuelve (los LNKA-LNKD, cuyo `_CRS` es un método
    que lee lo programado en el puente) las resuelve ACPICA
    (`AcpiGetIrqRoutingTable` y `AcpiGetCurrentResources`), con las marcas
    del `_CRS` (en «pc», por nivel y **activa en alto**), y Prt64 las usa
    como las demás;
  - **el brillo** (lo de la 11 b que «si da tiempo»): el primer dispositivo
    con `_BCM`; `_BCL` da los niveles y se pone el más cercano al pedido;
    `_BQC` lo lee.
- **HWP** (si da tiempo): si el procesador lo tiene (Intel desde Skylake),
  IA32_PM_ENABLE y, en cada procesador, IA32_HWP_REQUEST del más bajo al más
  alto, sin «deseado» y con la preferencia (EPP) a medias, como Linux: él
  elige la frecuencia y la baja en reposo. Sin HWP lo dice (los P-states de
  `_PSS`, para más adelante). En QEMU no hay HWP (ni con KVM en el EPYC del
  servidor): solo se ha probado que lo dice.
- **Las dos cosas que vio el coordinador en las capturas**: el reloj de la
  barra del escritorio marcaba la hora UTC (`gmtime_r`): ahora
  `localtime_r`, y las fechas de la oficina (`so_ahora`) también; el fondo
  dice la versión de uname en vez de «etapa 8».
- **El sonido** (la mezcla y el volumen) no ha cabido: sigue en lo que queda.

### Qué hace

Hispa-64 0.11.1.

| Archivo | Qué |
|---|---|
| `apic64.cas`, `reloj64.cas`, `porcpu64.cas` | la cuenta del APIC, el TSC y la hora leídos juntos; los tics que no llegan (`perdidos`, `huecos`); `periodo_tic_us` (contra el TSC) |
| `hispa64.cas`, `prsmp64.cas` | la etapa 2 con los que no llegan y el periodo; el interactivo sin ellos; ACPICA antes de las tarjetas; `apaga_todo`; HWP |
| `acpica64.cas` | ACPICA en el núcleo (nuevo): cargarlo, el hilo «acpi», las rutas, apagar, el brillo, `prueba=acpica` |
| `prt64.cas`, `aml64.cas` | los `_PRT` que no resuelve Aml64, por ACPICA (`ruta_disp`) |
| `acpi64.cas` | la RSDP leída, para ACPICA |
| `acpi/hispa64/` | ACPICA64.BIN (nuevo): `entrada.asm`, `maquina.c`, `servicio.c`, `hispa64.h`, `hispa64.ld`, `hacer.sh` |
| `acpi/nucleo/` | lo de 64 bits (sin cambiar Hispa-DOS ni Hispa-Pentium) |
| `tests/acpi/brillo-qemu.asl`, `haz_brillo.py` | el brillo de mentira (sin iasl: el AML, a mano) |
| `programas/ventanas/ventanas.c`, `nucleo32/oficina/comun/so_hispa64.c` | la hora local; la versión en el fondo |
| `programas/musl/msenales.c` | getrusage con lo del núcleo |

Lo que dice al arrancar (QEMU «pc», con la batería y el brillo de mentira):

```
Tic: 101 tics en 1 s (medido con el HPET), 0 que no han llegado (0 huecos); entre dos cambios del segundo del CMOS: 100 tics y 0 que no han llegado; el temporizador, cada 10,00 ms (contra el TSC)
ACPICA: ACPICA64.BIN, 161 KB (160 KB de código), en 0xFFFFFFFFC0000000 con 220 KB
Batería 1: \_SB_.HBAT
Adaptador de corriente: \_SB_.HACA
ACPICA: en marcha (55 KB de su montón; 0 avisos o errores de ACPICA); \_S5 (0, 0); el brillo, \_SB_.HLCD (50 %)
Energía: sin HWP (el procesador no deja que elija él la frecuencia): la que deja el firmware; en reposo, hlt o mwait
Prueba de ACPICA: la batería al 73 % y con el cable (CMOS 60h y 61h): 1 puesta(s), al 73 %, 3650 de 5000 mAh (de fábrica 7350), con el cable, cargando, 312 ciclos
  y al 41 % sin el cable: 1 puesta(s), al 41 %, 2050 de 5000 mAh (de fábrica 7350), sin el cable, 312 ciclos
  y quitada: 0 baterías puestas
Prueba de ACPICA: el brillo, \_SB_.HLCD, 10 niveles: a 30 % (_BCM) y leído (_BQC): 30; pedido el 77 %: 80 (el nivel de _BCL más cercano)
INTx: las rutas de ACPI (_PRT): 1 tablas resueltas (1 por ACPICA, con 128 entradas por enlaces de _CRS hecho por un método), 128 entradas
```

### Cómo se ha probado

`castel/nucleo64/pruebas.sh`: **131 pruebas** (eran 124). **Dos vueltas
seguidas, 131 bien y 0 mal cada una**, con el servidor cargado (otras
sesiones con QEMU y, a la vez, `SOLO=castel64`: carga de 3 a 5 en 4
procesadores), y `SOLO=castel64` dos vueltas seguidas a la vez, **80 bien y 0
mal** cada una (CASTEL64 y ENSAMB64 sacan el núcleo igual, octeto a octeto,
con la batería de Hispa-64 dentro). En las dos vueltas, 91 medidas del tic
cada una: de 91 a 101 tics que llegan en 1 s, con 8 y 27 que no llegaron
(en 5 y 8 arranques; 56 y 74 huecos de un tic tarde, que no cuentan), y el
temporizador siempre de 9,99 a 10,00 ms contra el TSC. La 11 dio 31 y 43
tics. Antes de esas dos vueltas, dos con mal de las propias pruebas
(«  MAL» casaba con «MALO»; la INTx de «pc» de la etapa 10 ya no es la de
antes con ACPICA) y una con el recuento de los huecos de antes (95 + 12). Lo
nuevo:

- **El tic**: en cada arranque, el periodo contra el TSC (de 9,80 a 10,20
  ms), los que llegan más los que no (de 95 a 106; entre dos segundos del
  CMOS, de 90 a 110) y que lleguen 30 o más; con KVM, 90 o más, y la etapa
  3 entera (el interactivo, de 2 a 3 ms de media). El interactivo de cada
  arranque, sin los tics perdidos (25 ms como mucho).
- **ACPICA** en «pc» y en q35 con la batería de mentira
  (`tests/acpi/bateria-qemu.aml`, la de las pruebas de Hispa-DOS: sus
  métodos leen la CMOS, que cambia la prueba) y el brillo de mentira
  (`brillo-qemu.aml`): 73 % cargando con el cable, 41 % sin él, quitada; los
  ciclos de `_BIX`; el brillo a 30 y el pedido al 77 %, al 80 de `_BCL`; 0
  errores de ACPICA. ACPICA arranca también en todos los demás arranques
  (sin batería: «sin baterías»).
- **Apagar por ACPICA** (`prueba=acpiapagar`): `_PTS(5)`, AE_OK, y QEMU
  acaba con 0; y con `sinacpica`, lo de antes.
- **Los enlaces de «pc»**: el `_PRT` con los LNKA-LNKD (su `_CRS`, un
  método), resuelto por ACPICA: 128 entradas; la e1000 por la GSI 10 u 11
  de su enlace, sin mirar cada 10 ms; los discos sin MSI ni MSI-X (los dos
  NVMe y la AHCI) por su INTx (por nivel, activa en alto: unas 100
  interrupciones del NVMe y 4.400 de la AHCI, ninguna GSI tapada), ARCHIVOS, fsck.fat y
  miraexfat.py, bien; y con `sinacpica`, mirando la cola, como antes.
- **El reloj de la barra**: con el CMOS en hora local, la hora de Madrid
  (19:47 cuando en UTC eran las 18:46) y «versión 0.11.1» en el fondo.
- **Hispa-DOS y Hispa-Pentium sin cambios**: HSPACPI.BIN y el simulador se
  compilan; el simulador con las tablas del Toshiba da lo mismo, línea a
  línea, que antes de tocar `acpi/nucleo/`.
- Lo de las etapas 0 a 11, igual (la unidad en memoria lleva 41 archivos,
  con ACPICA64.BIN; uname 0.11.1).

### Lo que queda (y más adelante)

- **Suspender** (S3 con `_PTS(3)`, el vector de despertar de la FACS y
  volver a poner todo: APIC, IOAPIC, las tarjetas; o S0ix): con ACPICA ya
  se puede empezar, pero pide guardar y restaurar cada controlador.
- **Los P-states sin HWP** (`_PSS`, `_PCT`, `_PPC` por procesador, con un
  «gobernador» que baje en reposo) y los C-states de `_CST` (hoy, hlt o
  mwait C1); el AMD CPPC.
- **Los GPE y la SCI por ACPICA** (hoy, los avisos del controlador embebido
  se miran cada 250 ms y la SCI solo atiende el botón fijo): el EC por su
  GPE, las tapas de los portátiles (PNP0C0D), los Notify de los
  dispositivos que se enchufan.
- **Los enlaces sin IRQ puesta** (su `_PRS` y `_SRS`, programarlos): hoy
  se cuentan como «sin resolver» y se mira la cola.
- **Imprimir** (IPP Everywhere: mDNS para descubrir, mandar PDF).
- **El sonido**: la mezcla de varios programas y el volumen, los
  auriculares, los Cirrus de los Mac, el micrófono (lo de la sección 17).
- Probar ACPICA en el taller: la batería y el EC del Toshiba y el HP Mini
  (las mismas tablas que ya pasan por el simulador de Hispa-DOS), el brillo
  y el botón de los portátiles.
- De la 10 b y la 11, lo que sigue allí (de la 10 b, curl e ICU, hechos:
  sección 19).

## 19. La etapa 10 b, la base hecha (25/11/2026): curl con BearSSL e ICU

### Las decisiones

- **Por qué la 10 b y no la 12**: el plan pone el navegador (la 10) antes
  de instalar y mantener (la 12), y la 11 y la 11 b se adelantaron porque
  pedían cosas del núcleo que la 10 b no necesitaba (el planificador, la
  INTx, ACPICA). De la 10 b, lo de abajo de todo, lo que cualquier
  navegador (Ladybird o WPE WebKit) pide antes de dibujar una página:
  **bajar por HTTP y HTTPS** (curl) y **el texto de hoy** (ICU). El
  navegador mismo, IPv6 y la libstdc++ compilada para musl, después (abajo).
- **curl 8.14.1, la última con BearSSL** (la 8.15 lo quitó), y no otra
  biblioteca de TLS: BearSSL ya está en `terceros/` (DESCARGA, TLS.BIN,
  HSPTLS.DLL), es MIT, pequeña y sin hilos ni estado global. Las fuentes,
  sin cambiar nada; la configuración, la que saca **su configure** para
  `x86_64-linux-musl` con la musl de Hispa-64 (`lib/curl_config.h`, con las
  opciones en `terceros/curl/LEEME.txt`: HTTP/1.1, HTTPS, FTP, FILE y
  WebSocket; IPv6 pedido; el resolutor de nombres en su hilo), y luego **un
  gcc por archivo** (`hazcurl.sh`, como FreeType): sin autotools en las
  pruebas. Lo que curl pide de BearSSL además del perfil de TLS.BIN
  (`br_ssl_client_init_full`: todas las suites de cliente, 3DES, CBC, CCM,
  el intercambio por RSA, las de AES-NI y SSE2 que elige al arrancar) va
  aparte, en `terceros/bearssl/src64` (43 archivos del mismo
  bearssl-0.6.tar.gz): el Makefile, hazred.sh y los de Hispa-Pentium siguen
  compilando `src/` solo.
- **Las autoridades**: la orden CURL lleva dentro las de siempre
  (`w32/tls/raices`, las mismas que DESCARGA y TLS.BIN) con `CURL_CA_EMBED`;
  con `--cacert`, las de ese archivo. La de libcurl por omisión es
  `/etc/ssl/certs/ca-certificates.crt`, que aún no hay (cada programa le da
  la suya: `CURLOPT_CAINFO`). BearSSL no comprueba un certificado por su IP
  (curl lo dice: «host verification of IP address is not supported»): por
  su nombre, con DNS o con `--resolve`.
- **ICU 77.1** (la de marzo de 2025, Unicode 16), lo que piden Ladybird y
  WebKit para el texto: `source/common`, `source/i18n` y `stubdata` sin
  cambiar nada, compilados con `cxx.sh` (la libstdc++ con musl) un archivo
  cada vez en `build/icu-obj/libicu.a` (unos 5 minutos la primera vez).
  **Los datos, filtrados** con la herramienta de ICU
  (`ICU_DATA_FILTER_FILE`, `terceros/icu/filtro.json`) desde sus fuentes
  (icu4c-77_1-data.zip): los idiomas de aquí (español, catalán, gallego,
  euskera) y el inglés; sin transliteración, sin los diccionarios de cortar
  el tailandés o el chino, sin RBNF ni los nombres de los caracteres; de
  las conversiones, solo windows-1252 (UTF-8, UTF-16 y Latin-1 van dentro
  de ICU): **4 MB** (`icudt77l.dat`) en vez de 31. Van aparte, no dentro
  del programa: ICU los abre de `ICU_DATA` o de la carpeta que se le diga
  (MICU, la suya). Hechos en el PC de Albinyana (las herramientas de ICU
  para el anfitrión); cómo rehacerlos, en `terceros/icu/LEEME.txt`.
- **La zona de ICU**: ICU no entiende la TZ de POSIX que da Hispa-64
  (`CET-1CEST,M3.5.0,M10.5.0/3`) y busca el nombre con `realpath` de
  `/etc/localtime`, como en Linux. Hispa-64 no tiene enlaces simbólicos:
  **`readlink` y `readlinkat`** (eran ENOSYS) dan, de `/etc/localtime`,
  `/usr/share/zoneinfo/<zona>` (Europe/Madrid, Atlantic/Canary...: el
  nombre de la IANA de la `zona=` de la línea de órdenes, `Hora64.zona_iana`),
  ese archivo se abre también (el mismo TZif) y sus carpetas, `/etc`, `/dev`
  y `/proc` «están» (EINVAL: no son enlaces, que es lo que pide el
  `realpath` de musl para seguir); lo que existe da EINVAL y lo que no,
  ENOENT. Sin eso, ICU se quedaba con «CET».
- **Comprobar con lo de fuera**: MICU, compilado igual, corre también en
  Linux (es un programa estático de Linux: pruebas.sh lo corre antes) y sus
  líneas de resultados tienen que salir **iguales** en Hispa-64, como
  MTEXTO. Lo que espera cada prueba está escrito a mano y mirado (el orden
  de la RAE, «1234» sin el punto con NumberFormatter, «juliol del 2026» en
  catalán, el punycode de «ñandú»).
- **Un fallo de fondo que sacó curl: dos hilos se llevaban el mismo
  descriptor.** Con MCURL contra Internet quedaban a veces un socket o un
  texto al acabar y una vez falló un nombre («Could not resolve hostname»).
  Con un programa de prueba (hilos que abren y cierran a la vez) el núcleo
  **se paraba** con un fallo de página en `copia_mem`, leyendo de un bloque
  del montón ya suelto. La causa: `fd_libre` (y `fd_nuevo` de Red64)
  buscaban el primer descriptor libre **sin cerrojo** y la clase se ponía
  después; dos hilos veían libre el mismo, el segundo pisaba al primero (lo
  del primero se perdía: el socket o el texto que quedaban) y, al cerrar uno,
  el otro leía de lo cerrado. curl lo hace siempre: su resolutor de nombres
  va en un hilo (musl abre allí `/etc/resolv.conf` y el socket del DNS)
  mientras el principal abre los suyos. Ahora **se apartan** con el cerrojo
  del proceso (`Procesos.aparta_fd`: la clase `F_APARTADO`, que no vale para
  nada más: EBADF), quien lo pide lo **llena** con el objeto antes que la
  clase (`llena_fd`) o lo **devuelve** si falla (`devuelve_fd`); `close`, con
  el cerrojo (dos hilos no cierran dos veces lo mismo); `dup2` sobre uno
  apartado da EBUSY (como Linux); fork no hereda los apartados. MHILOS lo
  prueba en cada arranque (8 hilos que abren y cierran 800 sockets,
  eventfd, tuberías y `/etc/hosts`, ninguno repetido; con 6.000, el núcleo
  de antes se paraba con 2 y con 4 procesadores). Lo que no se arregla aquí:
  cerrar un descriptor mientras otro hilo está **dentro** de una llamada con
  él (un `read` a medias) aún suelta el objeto debajo; Linux le pone una
  referencia durante la llamada (`fget`/`fput`), y eso queda para después
  (abajo).
- Hispa-64 **0.11.2** (uname: «etapa 10 b (curl e ICU)»).

### Qué hace

| Archivo | Qué |
|---|---|
| `terceros/curl/` | curl 8.14.1 (lib, src, include, con `lib/curl_config.h` de su configure) |
| `terceros/icu/` | ICU 77.1 (common, i18n, stubdata) y `data/icudt77l.dat`, filtrado con `filtro.json` |
| `terceros/bearssl/src64/` | lo de BearSSL 0.6 que pide curl y no está en `src/` |
| `programas/hazcurl.sh` | BearSSL entero en `build/bearssl64-obj`, libcurl en `build/curl-obj`, CURL (con las autoridades dentro) y MCURL |
| `programas/red/mcurl.c` | MCURL: las pruebas de libcurl por 127.0.0.1, con un servidor de HTTP/1.1 dentro; y `MCURL [-c CA] [-r NOMBRE:PUERTO:IP] DIRECCIÓN...`, bajar a la vez con su sha256 |
| `programas/cxx/micu.cpp` | MICU: las pruebas de ICU |
| `programas/cxx/hazcxx.sh` | `libicu.a`, MICU e `icudt77l.dat` |
| `sistema.cas`, `hora64.cas`, `red64.cas` | readlink y readlinkat; `zona_iana`, `ruta_zona`; `/usr/share/zoneinfo/<zona>` |
| `prmusl64.cas` | MICU y MCURL con los de musl, en cada arranque |
| `procesos.cas`, `sistema.cas`, `red64.cas` | los descriptores apartados con cerrojo (`aparta_fd`, `llena_fd`, `devuelve_fd`, `F_APARTADO`); `close` con cerrojo; `dup2` y EBUSY |
| `programas/musl/mhilos.c` | 8 hilos que abren y cierran descriptores a la vez |

Lo que dicen (q35, 2 procesadores, con TCG):

```
MICU: ICU 77.1 (Unicode 16.0), con musl y la libstdc++ (cxx.sh)
  a la española: Ávila avión chico cosa llama luz nube nuez Nuño ñu zapato
  a la tradicional: Ávila avión cosa chico luz llama nube nuez Nuño ñu zapato
  letras: 4 grafemas en 15 unidades de UTF-16 (una bandera, una familia con ZWJ, e + tilde y ñ)
  palabras: Qué|tal|El|navegador|de|Hispa|64|corta|3,5|palabras
  líneas: Un /texto /largo, /con /«comillas» /y /guiones-/de-/unión, /que /se /parte /(bien).
  números: 1.234.567,891; 1.234 (NumberFormat), 1234 y 12.345 (NumberFormatter); 98.765,50 €; 1.234,50 €; en inglés, 1,234,567.891
  la fecha: miércoles, 25 de noviembre de 2026, 20:30; en catalán, 15 de juliol del 2026; en gallego, 15 de xullo de 2026
MICU: la zona del sistema: Europe/Madrid (readlink de /etc/localtime: /usr/share/zoneinfo/Europe/Madrid)
  IDNA: ñandú.Español.es -> xn--and-6ma2c.xn--espaol-zwa.es -> ñandú.español.es
  los idiomas: catalán, euskera
MICU: todo bien (28 pruebas)
MCURL: libcurl libcurl/8.14.1 BearSSL, con la interfaz «multi», por 127.0.0.1
  GET de 1 MB y cinco más por la misma conexión (0 nuevas)
  el plazo de 300 ms: vence a los 305 ms
  «localhost»: por 127.0.0.1
  curl_multi: 16 de 16 bien a la vez (3081400 octetos, la mitad a trozos), en 5795 ms y 7 vueltas de curl_multi_poll
MCURL: todo bien (20 pruebas)
```

MCURL prueba: GET de 1 MB comprobado octeto a octeto y cinco más por la
misma conexión (keep-alive), a trozos, tres redirecciones y
`CURLOPT_MAXREDIRS`, 404 (y con `FAILONERROR`), POST de 40.000 octetos
devuelto, sin `Content-Length` hasta que se cierra, el plazo de 300 ms,
conexión rechazada, «localhost» (con IPv6 pedido y sin IPv6 en el núcleo:
por IPv4), 16 a la vez con `curl_multi` (8 conexiones como mucho, la mitad
a trozos), `curl_url` (la ruta sin `..`, una relativa) y `curl_easy_escape`
con UTF-8. MICU: NFC, NFD y NFKC; ß → SS, la i turca, la sigma final,
foldCase y los títulos; ordenar a la española, a la tradicional (ch y ll
aparte) y sin tildes; los grafemas, las palabras y dónde se puede cortar la
línea (UAX 14); los números y los euros; leer «3,25»; la hora de Madrid en
invierno y en verano, la fecha en español, catalán y gallego, la semana
desde el lunes; los plurales; IDNA (UTS 46) de ida y vuelta; BiDi;
las escrituras; windows-1252; las expresiones regulares de ICU
(`\p{L}`); el nombre de los idiomas.

### Cómo se ha probado

`castel/nucleo64/pruebas.sh`: **132 pruebas** (eran 131). **Dos vueltas
seguidas, 132 bien y 0 mal cada una**, con el servidor cargado por otras
sesiones (carga de 5 a 6 en 4 procesadores), y `SOLO=castel64` dos vueltas
seguidas después (CASTEL64 y ENSAMB64 sacan el núcleo igual: abajo). Antes
de esas, cinco vueltas con mal, todos de lo nuevo y arreglados: lo que
esperaban las pruebas (la memoria libre con 128 MB, de 110 MB a 100: los
programas de la unidad son ya 18,5 MB; el «=N» de RED.PRB; ORDENES, que dice
«etapa 10 b»), el euro y la Á de MICU por el puerto serie (la consola los
pasa a la página 437: ahora `\u{20AC}`), `sfence` (ensambla.py no la tiene),
MHILOS con 150 vueltas (99 s con 8 procesadores), el plazo de 20 s de MCURL
(16 a la vez llegaron a 25 s con el servidor cargado) y el sonido con cortes
dos veces de cuatro cuando MCURL iba justo antes (ahora va después de
MSONIDO, como en la 11 b). Lo nuevo:

- **MICU y MCURL en cada arranque con musl** (multiboot con 1, 4 y 8
  procesadores, «pc» y q35, 128 MB, UEFI con 1 y 4, y con KVM): MICU, 28
  pruebas, sus líneas iguales a las de Linux y la zona Europe/Madrid (de 630
  a 960 ms); MCURL, 20 pruebas: las 16 a la vez, 1,9 s con 1 procesador, 12,6
  con 4 y 18,3 con 8 (TCG: el mutex que espera al tic, abajo).
- **La red** (e1000e, 2 procesadores, IP fija y el DNS de prueba): MCURL
  baja cinco a la vez (HTTP, a trozos, 5 redirecciones, HTTPS con la
  autoridad de prueba y `-r`) con el sha256 del anfitrión (2,4 y 2,8 s); la
  orden CURL con `-L` (5 redirecciones), con `--cacert` y `--resolve`
  (`ssl_verify_result` 0), sin la autoridad de prueba (código 60), `-f`
  con un 404 (22), por su nombre con el DNS de prueba (por 10.0.2.2) y uno
  que no existe (6); ni un socket ni un objeto al acabar.
- **MHILOS**: 8 hilos que abren y cierran 800 descriptores a la vez,
  ninguno repetido, en cada arranque con musl (el núcleo de antes se paraba,
  o daba 362 repetidos: lo arreglado).
- Lo de las etapas 0 a 11 b, igual (la unidad en memoria lleva 45 archivos;
  uname 0.11.2).

`SOLO=castel64` (con CASTEL64 y ENSAMB64 el núcleo sale igual, octeto a
octeto): dos vueltas seguidas, **80 bien y 0 mal** cada una (dentro,
`nucleo64/pruebas.sh` con CASTEL64 y ENSAMB64: 133 bien y 0 mal).

El PC de Albinyana sirvió para hacer los datos de ICU (sus herramientas para
el anfitrión) y para buscar el fallo de los descriptores (varias QEMU a la
vez); `nucleo64/pruebas.sh` entero allí aún no vale: le faltan el GRUB de
x86_64-efi y su NASM saca otro trampolín (no es de Hispa-64).

Las imágenes de la carpeta del taller
(`Pruebas/Pendientes/VirtualBox/Hispa-64-etapa-10b/`): el disco (`.vdi` e
`.img`) y el CD, con LEEME.TXT: qué probar desde el intérprete (CURL, MCURL
con direcciones de verdad, MCURL y MICU solos).

Además, a mano, contra Internet de verdad (la red de usuario de QEMU sale
por el servidor): `CURL https://hispados.es/` (200, 88.935 octetos, el
certificado de verdad comprobado con las autoridades de dentro), `CURL -L
http://hispados.es/` (que redirige a HTTPS) y `MCURL https://hispados.es/
https://www.google.es/ https://curl.se/` dos veces seguidas (las tres a la
vez en 1,4 s; el sha256 de hispados.es y curl.se, el mismo que en el
anfitrión; ni un socket ni un objeto al acabar).

### Lo que queda (y más adelante)

- **IPv6** (lo siguiente de la red): ND y SLAAC (la de QEMU anuncia
  fec0::/64 y su DNS en fec0::3), ICMPv6 (el eco, los errores), y
  AF_INET6 para UDP y TCP. Hoy las direcciones son `nat32` en todo Ip64,
  Tcp64 y Red64: la forma es un tipo de 128 bits (dos `nat64`) en los
  sockets y las conexiones, con las de IPv4 como `::ffff:a.b.c.d`, para un
  solo camino; `getaddrinfo` de musl pide AAAA solo si hay una dirección
  IPv6 que no sea de enlace. curl ya va con IPv6 pedido (sin él en el
  núcleo, `socket(AF_INET6)` da EAFNOSUPPORT y curl sigue por IPv4).
- **La libstdc++ compilada para musl** y **C++ dinámico**: un
  `x86_64-linux-musl-g++` de verdad (GCC 13.3 con binutils del sistema, al
  estilo de musl-cross-make; gmp, mpfr y mpc solo para compilarlo), hecho
  una vez en el PC de Albinyana y guardado en `build/` (con las sumas de los
  archivos de GCC en un guion `hazgcc.sh`): quita el puente de `cxx.sh` y
  hace las pruebas independientes del GCC del anfitrión (el PC tiene GCC
  15 y el servidor, 13: la libstdc++ del sistema no es la misma);
  `libstdc++.so` con ld-musl para los programas grandes.
- **El navegador**: Ladybird (BSD-2) primero sin ventana (su
  `headless-browser` dibuja en un mapa de bits: se compara con un
  pantallazo), luego su ventana sobre el compositor. Pide además
  libjpeg-turbo, libpng, libwebp, woff2, simdutf, fast_float y Skia (o el
  pintor propio de antes); el JavaScript, su LibJS. WPE WebKit pediría glib,
  libsoup y EGL: más lejos.
- **curl**: `/etc/ssl/certs/ca-certificates.crt` para libcurl (hoy cada
  programa da las suyas, o `CURLOPT_CAINFO_BLOB`), HTTP/2 (nghttp2, MIT),
  `Content-Encoding` con zlib y brotli (están miniz y brotli en
  `terceros/`), los nombres con eñe (IDN) por ICU, y la lista de sufijos
  públicos (las cookies).
- **ICU**: una carpeta de datos fija (`/usr/share/icu/77.1/`), más idiomas
  si el navegador los pide (los datos enteros son 31 MB) y los diccionarios
  de cortar el chino, el japonés y el tailandés.
- **El mutex que se pasa a otro procesador espera al tic** (lo vio la
  prueba nueva de MHILOS): 8 hilos que abren y cierran a la vez sockets o
  tuberías, con 1 procesador, 1,2 ms cada uno; con 4 procesadores (TCG),
  17 ms (`socket` 21 s y `pipe` 20 s las 1.200; `eventfd`, 2 s; `getpid`,
  de 19 a 250 ms). Es lo de la sección 17: el que se despierta en otro
  procesador desde un hilo del núcleo no corre hasta su tic siguiente. Con
  muchos hilos (el navegador) se notará: despertarlo con el aviso entre
  procesadores (como al volver al anillo 3), y que el cerrojo de la red no
  sea uno solo para todo.
- **El núcleo**: una referencia al objeto mientras dura cada llamada con un
  descriptor (para que cerrarlo desde otro hilo no lo suelte debajo:
  `fget`/`fput` de Linux); `readlink` de `/proc/self/exe` (el Proceso solo
  guarda el nombre corto, no la ruta).
- De la 11 b, sin tocar: suspender (S3), los P-states sin HWP (`_PSS`), los
  GPE y la SCI por ACPICA, `_SRS`, imprimir (IPP) y la mezcla y el volumen
  del sonido (sección 18).

## 20. La etapa 10 c, hecha (26/11/2026): lo de fondo del núcleo, el compilador cruzado, IPv6 y la auditoría

### Las decisiones

- **Por qué la 10 c**: lo que la 10 b dejó para antes del navegador (sección
  19, «Lo que queda»): dos fallos de fondo del núcleo que con muchos hilos
  (el navegador) se iban a notar, el compilador de C++ propio (Ladybird pide
  GCC 14 o más, y las pruebas no pueden depender del GCC del anfitrión),
  IPv6 y, del camino, lo de la auditoría del PC de Albinyana (Claude 1) y lo
  que pidió Carlos del intérprete. Ladybird mismo, todavía no: abajo, lo que
  pide.
- **Una referencia al objeto mientras dura cada llamada** (`fget`/`fput` de
  Linux). Cada llamada que usa un descriptor en su primer argumento (read,
  write, recv, accept, ioctl, fstat, las `*at` con su carpeta...: `con_fd`
  en `sistema.cas`; y cada uno que miran poll, select y epoll) lo **toma**
  (`Procesos.toma_fd`: con el cerrojo de los descriptores, una cuenta de
  usos por número y su **generación**) y lo **deja** al acabar
  (`deja_fd`). `close` (y `dup2` encima) **vacía el número en el acto**,
  como Linux (`quita_fd`): si alguna llamada lo estaba usando, lo suyo pasa
  a la lista de **aplazados** del proceso (hasta 32 a la vez; si no cabe,
  `close` cede el procesador y vuelve a probar) y lo suelta la última que
  acaba. Antes, un `close` desde otro hilo a mitad de un `read` soltaba el
  objeto debajo. Primero se hizo de otra forma (el número «cerrado» y sin
  dar hasta que acabase la llamada) y no valía: `dup2` sobre él daba EBUSY y
  **musl repite `dup2` mientras dé EBUSY**: el programa se quedaba dando
  vueltas (lo vio la prueba nueva de MHILOS).
- **«El mutex que espera al tic» no era el mutex: era la TLB.** Midiendo (la
  espera de cada despertar, cuántos avisos de vaciar la TLB y lo que
  costaban) salió la causa de fondo: **cada página de un bloque grande del
  montón** (los de 64 KB o más: el búfer de un socket de UDP, una tubería, los
  de TCP, de 256 KB) **mandaba al liberarse su propio aviso a todos los
  procesadores y esperaba a que vaciasen su TLB**: cerrar un socket de UDP
  eran 17 avisos con su espera; con 4 procesadores y TCG, unos 3 ms cada uno.
  Ahora (`Monton64`) las páginas se quitan de aquí y sus físicas van a una
  **cuarentena** (hasta 2.048, 8 MB) que se devuelve de una vez, de 256 en
  256 con **un solo aviso** (la TLB entera, también lo global) cuando se
  llena o cuando alguien mide la memoria (`vacía_cuarentena`). Es seguro
  porque la zona de los bloques grandes no se reutiliza: una entrada vieja en
  la TLB de otro procesador solo puede llevar a una física que aún está en
  cuarentena, nunca a la de otro; y lo de la cuarentena sigue contando como
  del montón (las cuentas de «la memoria, la de antes» no cambian). Los de
  más de 512 KB, sin cuarentena, con un aviso por cada 64 páginas.
- Y dos más, del planificador, que también ayudan: **el mutex espera un poco
  sin dormir** (hasta 50 µs) si quien lo tiene está corriendo en otro
  procesador (como el «optimistic spinning» de Linux: dormirse y despertarse
  cuesta más que eso), y **uno de la misma prioridad que se despierta pasa
  por delante** del que corre en su procesador si este lleva ya 100 µs de su
  turno (con el aviso entre procesadores, `V_PLAN`; antes, solo si era de
  más prioridad, y si no esperaba al fin del turno del otro, hasta 20 ms).
- **Los cortes del sonido después de MCURL** (sección 19): la misma causa.
  MCURL deja sus conexiones en TIME_WAIT (2 s) y el hilo «red» las suelta
  mientras suena MSONIDO: 16 conexiones con sus búferes de 256 KB eran
  cientos de avisos de vaciar la TLB, cada uno con las interrupciones
  quitadas en un procesador mientras espera a los demás. Con la cuarentena
  son unos pocos (lo de cómo se ha probado, abajo).
- **El compilador cruzado, `x86_64-hispa64-linux-musl`** (con los nombres
  cortos `x86_64-hispa64-musl-gcc` y `-g++`; «hispa64-musl» a secas no lo
  admite el `config.sub` de GCC: el sistema tiene que ser `linux-musl` para
  que GCC use lo de musl). `programas/cxx/hazgcc.sh`, al estilo de
  musl-cross-make: **binutils 2.44, GCC 14.3.0** (C y C++; el 14 y no el
  13.3 que decía el plan porque Ladybird pide GCC 14 o más) y **musl 1.2.6**
  de `terceros/musl` (con sus parches) compilada con ese mismo GCC en su
  raíz; gmp 6.3.0, mpfr 4.2.2 y mpc 1.3.1 solo para compilarlo (y m4 1.4.19,
  que el PC no tiene). Los tar de GNU, con su SHA-256 en el guion (las
  firmas, comprobadas con `gnu-keyring.gpg`). Los programas del anfitrión
  van **estáticos** (corren en cualquier Linux x86-64: se hizo en el PC de
  Albinyana, con glibc 2.43, y se usa en el servidor, con la 2.39) y la
  carpeta se puede mover (todo relativo a ella). Con 12 hilos, unos 25
  minutos; deja `build/cruzado/` (253 MB) y `build/cruzado.tar.xz` (61 MB;
  `build/` no va a git: se rehace con `HILOS=12 hazgcc.sh` o se copia el tar).
- **`cxx.sh` ya no usa el GCC del anfitrión**: llama al cruzado
  (`-static` por omisión; `-dinamico` para enlazar con `libstdc++.so.6` y
  `libgcc_s.so.1`); sin él, dice cómo hacerlo (con `HISPA64_PUENTE=1`, lo de
  antes: el g++ del sistema y `puente.c`). La libstdc++ es la de GCC
  compilada para musl (el modelo de locale «generic»): sin puente para
  `newlocale` ni `strerror_r`. HarfBuzz, ICU, MCXX, MTEXTO y MICU, con él
  (lo compilado con otro compilador se rehace solo: `.compilador`).
- **C++ dinámico**: **MCXXD** (la misma prueba que MCXX) con
  `libstdc++.so.6` y `libgcc_s.so.1` en la unidad en memoria (`/lib`), por
  ld-musl.
- **IPv6, con un solo camino para las dos.** En vez de pasar a 128 bits
  todas las direcciones de Ip64, Tcp64 y Red64 (y las colas de los
  datagramas), cada dirección de IPv6 se guarda **una vez en una tabla**
  (`Redcom64.ficha_de`: 1.024, las de las interfaces fijas y las demás se
  reciclan, la más vieja, si ningún socket la usa y lleva un minuto sin
  verse) y los sockets, TCP y UDP llevan **su ficha**, un número de 32 bits
  del 240.0.0.0/8, la «clase E» que IPv4 no usa en ninguna red. Así, todo lo
  que ya sabía de direcciones (buscar la conexión, el puerto, la suma de la
  cabecera falsa, que ahora mira si es una ficha) vale para las dos sin
  tocarlo: `ip_envía` y `udp_envía` ven que «a» es una ficha y salen por
  IPv6; `entra_ip6` da a `entra_udp` y a TCP las fichas de quien manda y a
  quién. Las de IPv4, en un socket de IPv6, como **`::ffff:a.b.c.d`** solo
  en la frontera con el programa (`lee_dir`, `da_dir`).
- **Lo de IPv6**: la dirección **de enlace** (fe80:: y el identificador de la
  MAC, EUI-64) en cuanto hay enlace; **RS** (tres, de 4 en 4 s) y, con el
  **anuncio del router**, la dirección de **SLAAC** (su prefijo de /64 con la
  marca A), el router (si su vida no es 0), el MTU y el **DNS** (RDNSS, RFC
  8106: va a `/etc/resolv.conf` si la interfaz no tiene uno de IPv4); los
  **vecinos** (ND: NS a la dirección solicitada, NA con la MAC, el paquete
  que espera, tres intentos); **ICMPv6** (el eco, contestar a los ping, los
  «ping sockets» de `IPPROTO_ICMPV6`, «puerto inalcanzable» a UDP y lo que
  llega inalcanzable, a TCP y al UDP conectado); las cabeceras de extensión
  de salto a salto y de destino, saltadas. **AF_INET6** en los sockets:
  TCP, UDP y el ping; `IPV6_V6ONLY` (sin él, uno en `[::]` recibe también a
  los de IPv4, como Linux), `IPV6_UNICAST_HOPS`, `SO_DOMAIN`. Las e1000
  reciben ya toda la multidifusión (MPE: la de IPv6); las Realtek, virtio y
  las de USB ya lo hacían. «sinipv6» en la línea de órdenes lo apaga. Sin
  trozos, sin DAD (se da por buena la dirección), sin las direcciones
  temporales (RFC 8981) ni DHCPv6: abajo.
- **De la auditoría** (`auditorias/mejoras`, Claude 1): su parche, revisado
  y aplicado tal cual (H1, TCP según el RFC 5961: el límite de abajo de la
  ventana también con SYN o FIN, el RST solo justo en `rcv_nxt`, y un
  «challenge ACK» ante un SYN o un RST dentro de la ventana; H2, `sendmsg`
  sin dar la vuelta al sumar los trozos ni copiar más de lo reservado; H5,
  los LED de HID; H6, `wMaxPacketSize` 0; H7, `clone` con un TLS no
  canónico, EPERM; H8, el RIP de la señal en `al_volver_`; H9, el `mmap` de
  `/dev/fb0`), con sus pruebas nuevas en la batería (H2 en MRED6, H7 en
  MHILOS). Y de su sección 2: **la tabla de arreglos** (un `iretq` de vuelta
  al programa que falla en el anillo 0, ya con el GS del programa, cierra el
  programa con SIGSEGV en vez de parar el núcleo: Excep64 pone el GS del
  núcleo y sigue en `Entradas.E64_arreglo`; `prueba=salida` le pone a
  MBUCLE un RIP no canónico justo antes del `iretq`, y la prueba va con
  KVM: con TCG, QEMU deja pasar ese `iretq` y el fallo llega ya en el anillo
  3, por el camino de siempre; las copias hacia el programa ya
  van por el mapa directo, con el mutex de la memoria, y no pueden fallar; el
  `sysret` no canónico ya se mira antes); **el DNS** (el de musl: el id de
  cada pregunta, de `getrandom`, no de la hora, y la respuesta solo con su id
  **y su pregunta**, RFC 5452: `terceros/musl/parches-hispa/`) y **los
  puertos de salida, cada vez al azar** (RFC 6056; antes, seguidos desde uno
  al azar). TCP ya empezaba cada conexión con un número al azar entero
  (`azar32`, ChaCha20): más que lo que pide el RFC 6528.
- **ORDENES, en español** (lo pidió Carlos: «en una demostración no es
  español»): los nombres de Hispa-8086 y Hispa-386 (`listar`, `mostrar`,
  `eco` y `escribir`, `donde`, `creardir`, `borrardir`, `borrar`, `copiar`,
  `mover` y `renombrar`, `limpiar`, `ver`, `ayuda`, `salir`), en mayúsculas
  o minúsculas; los de siempre en inglés valen también, y `ayuda` los dice
  al final. Y **DEMO**, el programa de Castel de la captura de hispados.es
  (`programas/demo.cas`: saluda, 6 por 7, la fecha y la hora de Madrid con
  el cambio de horario de la UE, unos euros).
- Hispa-64 **0.11.3** (uname: «etapa 10 c (IPv6 y C++)»).

### Qué hace

| Archivo | Qué |
|---|---|
| `procesos.cas`, `sistema.cas` | `toma_fd`, `deja_fd`, `quita_fd`, los aplazados; `con_fd`; poll y epoll con cada descriptor tomado; `salida_mala`; `prueba=salida` |
| `monton64.cas` | la cuarentena de los bloques grandes (`vacía_cuarentena`) |
| `sinc64.cas`, `hilos64.cas`, `porcpu64.cas` | el mutex que espera un poco si el dueño corre; pasar por delante al despertar (`turno_tsc`, 100 µs) |
| `entradas.cas`, `excep64.cas` | la tabla de arreglos (`E64_iret1`, `E64_iret2`, `E64_arreglo`) |
| `redcom64.cas` | las fichas de IPv6 (`ficha_de`, `dir_de`, `es_ficha`, `escribe_ip6`); IPv6 en Ifz; `dom` y `v6solo` en Sock |
| `ip64.cas` | IPv6, ICMPv6, ND, SLAAC, RS y RA, RDNSS; los puertos al azar |
| `red64.cas`, `tcp64.cas` | AF_INET6, `sockaddr_in6`, ::ffff:, IPV6_V6ONLY, el ping de ICMPv6; la MSS con IPv6; el DNS de IPv6 en resolv.conf |
| `intel64.cas` | MPE (la multidifusión) |
| `programas/cxx/hazgcc.sh`, `cxx.sh`, `hazcxx.sh` | el compilador cruzado y su uso; MCXXD |
| `programas/red/mred6.c` | MRED6: IPv6 por ::1 (sin tarjeta) y, con `red NOMBRE PUERTO`, con la red |
| `programas/red/servidores.py` | `http6` (en ::1) y el AAAA de los `*6.prueba` |
| `programas/ordenes.c`, `programas/demo.cas` | las órdenes en español; DEMO |
| `terceros/musl/parches-hispa/` | el id del DNS y la pregunta |

Lo que dicen (q35, 2 procesadores, con KVM):

```
MRED6: IPv6 por ::1, sin tarjeta
  inet_pton, inet_ntop y getaddrinfo de «::1»
  TCP por [::1]:55558: connect, accept, getpeername, 200 KB de ida y vuelta en 272 ms
  los dos a la vez: uno de IPv6 en [::] recibe a 127.0.0.1 como ::ffff:127.0.0.1
  IPV6_V6ONLY: el de IPv4, rechazado
  UDP por [::1] y a ::ffff:127.0.0.1; el puerto inalcanzable (ICMPv6): ECONNREFUSED
  ping de ICMPv6 a ::1: contesta
  los errores: EAFNOSUPPORT, ENETUNREACH; sendmsg con trozos que dan la vuelta, rechazado
MRED6: todo bien (25 pruebas)
/> MBUCLE                         (con prueba=salida)
MBUCLE: doy vueltas sin llamar al sistema (Ctrl+C me para)
El programa MBUCLE (proceso 2) ha querido volver con unos registros que no valen (CS, SS o RIP de su marco): se cierra (SIGSEGV).
(MBUCLE se ha cerrado por la señal 11, SIGSEGV: una dirección que no es suya)
/> DEMO
Hola, soy un programa de Castel, el lenguaje en español de Hispa-DOS.
6 por 7 son 42.
Hoy es jueves, 26 de noviembre de 2026, y en Madrid son las 09:14 (horario de invierno).
```

### Cómo se ha probado

`castel/nucleo64/pruebas.sh`: **134 pruebas** (eran 132). **Dos vueltas
seguidas, 134 bien y 0 mal cada una** (66 y 67 minutos), con el servidor cargado por
otras sesiones. Antes, dos vueltas a medias, con lo que faltaba y arreglado:
la memoria libre con 128 MB (la unidad en memoria lleva 3,5 MB más:
desde 90 MB), la expresión de la tabla de arreglos, que con TCG el `iretq`
no falla (ahora con KVM), el `ud2` (ensambla.py no la tiene: en octetos) y
la línea del anuncio del router, mezclada con lo de otro procesador (ahora en
el resumen de la red). Y `CASTEL64` y `ENSAMB64` sacan el núcleo igual,
octeto a octeto (comprobado a mano: el ensamblador de CASTEL64, igual al de
ccastel.py, y lo de ENSAMB64, igual a lo de NASM). Lo nuevo:

- **MHILOS** (en cada arranque con musl): los 8 hilos que abren y cierran 800
  descriptores, con TCG en el servidor, de 105 a 144 ms con 1 procesador, 1,8
  s con 4 y de 1,9 a 2,5 s con 8 (la 10 b medía 17 ms por cada uno con 4:
  unos 21 s); **con KVM y 4 procesadores, de 770 ms a 41** (el núcleo de
  antes y el de ahora, el mismo MHILOS). En el PC de Albinyana (TCG, sin
  carga): con 4, de 7,5 s a 4,4; con 8, de 29 s a 12-14. Lo que costaba: de
  518 avisos de vaciar la TLB (1,5 s de espera) a 133 (0,25 s), y el
  despertar (de media 1,5 ms con TCG, 0,12 con KVM). Y cerrar una tubería
  mientras otro hilo lee de ella: EBADF para las llamadas nuevas, `dup2`
  encima vale enseguida (como Linux), el `read` de antes acaba con lo que se
  escribe después y, al acabar, la tubería se suelta (EPIPE). Y `clone` con un
  TLS no canónico: EPERM (auditoría H7).
- **El sonido después de MCURL**, otra vez en su orden: con el núcleo de
  antes, en el PC (TCG, seis a la vez), la segunda ráfaga salía cortada 3
  veces de 3 (609 a 627 ms en vez de 500, un corte); con el de ahora, 0 de 3
  (492 a 504 ms); en la batería, sin cortes.
- **MCXX y MCXXD** (el compilador cruzado; estático y dinámico): 35 pruebas
  cada uno, en cada arranque con musl; MICU, MTEXTO y MCURL, compilados con él,
  igual que antes (MICU y MTEXTO, las mismas líneas que en Linux).
- **MRED6**, 25 pruebas por ::1 en cada arranque con musl; y **IPv6 por la
  red de QEMU** (e1000e, 2 procesadores, IP fija y el DNS de prueba): la
  dirección de SLAAC (`fec0::5054:ff:fe12:3456/64`), el router (`fe80::2`;
  la red de usuario de QEMU no manda RDNSS), el AAAA de `anfitrion6.prueba`
  (fec0::2), tres pings de ICMPv6, un GET por TCP sobre IPv6 (1 MB en unos
  370 ms) y MCURL y CURL a `http://[fec0::2]:PUERTO/` con el sha256 del
  anfitrión; ni un socket al acabar.
- **ORDENES** con las órdenes en español (y DIR y echo, en inglés), `ayuda`
  y **DEMO** (6 por 7 y la hora de Madrid, la del anfitrión entre antes y
  después); y la **tabla de arreglos** con KVM (`prueba=salida`).
- Lo de las etapas 0 a 11 b y la 10 b, igual (la unidad en memoria lleva 50
  archivos; uname 0.11.3). curl_multi (16 a la vez, TCG): 2,7 s con 1
  procesador, 14,6 con 4 y 23,4 con 8 (como en la 10 b: lo de TCG, abajo).

El PC de Albinyana hizo el compilador cruzado (12 hilos, unos 25 minutos; y
otra vez la musl, con el parche del DNS), las medidas de MHILOS con 1, 4 y 8
procesadores y la prueba del sonido (los núcleos de antes y de ahora a la
vez). `nucleo64/pruebas.sh` entero allí sigue sin valer del todo: no tiene
KVM para carlos; el GRUB de x86_64-efi ya lo tiene y su NASM 3.01 se
sustituye por un 2.16.01 compilado en `/mnt/castelpc/h64/herr/nasm`.

Las imágenes de la carpeta del taller
(`Pruebas/Pendientes/VirtualBox/Hispa-64-etapa-10c/`): el disco (`.vdi` e
`.img`) y el CD, arrancados antes en QEMU (el disco por UEFI y el CD por la
BIOS: 0.11.3, la dirección de SLAAC, MRED6 y MCXX bien), con LEEME.TXT: cómo
crear la máquina en VirtualBox y qué probar (ayuda, DEMO, MCXX y MCXXD,
CURL por HTTPS, MRED6, IPv6 con la red, MHILOS).

### El navegador: lo que pide Ladybird sin ventana (el siguiente paso)

El primer paso del navegador, según el plan (sección 19), es **Ladybird sin
ventana**: su `headless-browser` (LibWeb y LibJS, sin Qt: el «chrome» de
Ladybird es aparte y en modo headless no se usa) carga una página y saca
**un PNG** (`--screenshot`) o **el texto** (`--dump-text`, `--dump-layout`),
que se comparan con lo que saca el mismo programa en Linux, como MICU y
MTEXTO. No cabía hoy; lo que hace falta, por orden:

1. **Lo del núcleo** (lo que Ladybird da por hecho de un POSIX de hoy):
   - **AF_UNIX** (`socketpair` y los de ruta), con **SCM_RIGHTS** (pasar
     descriptores): Ladybird es de varios procesos (WebContent,
     RequestServer, ImageDecoder) que se hablan por sockets de Unix y se
     pasan descriptores; hoy `socketpair` da EOPNOTSUPP. Es lo más grande.
   - **memoria compartida por descriptor**: `memfd_create` (o
     `shm_open` sobre `/dev/shm`, que ya hay) con `ftruncate` y `mmap
     MAP_SHARED` de verdad (Core::AnonymousBuffer: los mapas de bits que se
     pasan los procesos).
   - `readlink("/proc/self/exe")` (para encontrar los demás programas y sus
     recursos), `posix_spawn` con sus acciones de descriptores (ya va con
     clone y CLONE_VFORK), `pthread_setname_np`, `sched_getaffinity` con el
     número de verdad, `getrusage`, `timerfd`/`eventfd` (ya están).
   - Muchos hilos (cada proceso tiene los suyos): lo de esta etapa (el mutex,
     el despertar y la TLB) es lo que hacía falta para eso.
2. **El compilador**: GCC 14.3 con C++23 (`build/cruzado`, de esta etapa);
   CMake y Ninja en el anfitrión (para compilar: no van a Hispa-64), con un
   archivo de «toolchain» de CMake para `x86_64-hispa64-linux-musl`, en el
   PC de Albinyana (son varias horas de compilar con 12 hilos).
3. **Las bibliotecas de terceros** (todas con licencia compatible con la
   GPL-2.0; en `terceros/`, compiladas con el cruzado): **Skia** (BSD-3: lo
   que dibuja; la parte de la CPU, sin GPU ni Vulkan), libjpeg-turbo
   (BSD/IJG), libpng (libpng), libwebp (BSD-3), woff2 (MIT), simdutf
   (MIT/Apache-2.0), fast_float (MIT), SQLite (dominio público: las
   cookies y la caché), libxml2 (MIT), fontconfig (MIT, o buscar las letras
   a mano); **OpenSSL 3** (Apache-2.0: compatible con la GPL-3, **no** con
   la GPL-2.0 sola; Hispa es «GPL-2.0 o posterior», así que el conjunto
   puede ir como GPL-3, pero hay que decidirlo; si no, curl con BearSSL y
   la capa de Ladybird que usa OpenSSL para su LibCrypto hecha sobre
   BearSSL o monocypher). **Ya están**: FreeType y HarfBuzz (etapa 10), ICU
   (10 b), curl (10 b), zlib/brotli (miniz y brotli en `terceros/`).
   Sin ffmpeg (el vídeo y el audio de las páginas, más adelante), sin
   libavif ni libjxl al principio.
4. **Ladybird mismo** (BSD-2), la última versión fija que compile con GCC
   14: AK, LibCore (con el bucle de sucesos sobre `poll`), LibIPC, LibGfx
   (con Skia), LibJS, LibWeb, LibWebView y `headless-browser`; RequestServer
   (las descargas, con curl). Lo que no sea de Linux de verdad y Hispa-64 no
   tenga, con `#ifdef` en una capa propia (`Platform/Hispa64`), no
   cambiando Ladybird por dentro.
5. **La prueba**: `headless-browser --dump-text` de unas páginas de prueba
   (HTML, CSS, JavaScript, una imagen, una letra web), servidas por
   `servidores.py`, con el texto igual que en Linux; y `--screenshot`
   comparado con el de Linux (los mismos píxeles, o casi: el suavizado).

Después, su ventana sobre el compositor de la etapa 8 (con LibWebView y una
capa propia en vez de Qt) y el ratón y el teclado.

### La etapa futura: el driver de NVIDIA (Turing en adelante)

Para los equipos con una gráfica de NVIDIA de 2018 en adelante (Turing: las
RTX 20 y GTX 16; Ampere, Ada...): un driver de verdad (el modo de la
pantalla, el cursor y, más adelante, la aceleración) a partir de
**`open-gpu-kernel-modules`** de NVIDIA (el módulo de núcleo abierto, con
licencia doble MIT/GPL-2.0: la MIT nos vale), que hace casi todo hablando
con el **GSP** (el procesador que lleva la propia tarjeta) y su **firmware**
(`gsp_*.bin`, de `linux-firmware`, redistribuible tal cual). Lo nuestro
sería la capa del sistema que ese código pide (la memoria, el PCI, las
interrupciones MSI-X, los temporizadores, los cerrojos: su «os-interface»),
en un módulo propio, y cargar el firmware. **El banco de pruebas: la RTX
3060 (Ampere, GA106) del PC de Albinyana**, arrancando Hispa-64 desde una
memoria USB en ese mismo PC (con UEFI; el GOP de la tarjeta ya da la
pantalla mientras tanto). Pide antes: el cargador de módulos del núcleo (o
compilarlo dentro), y arrancar desde USB en ese PC (hoy se usa con
Windows y WSL). No se empieza todavía.

### Lo que queda (y más adelante)

- **El navegador**: lo de arriba (AF_UNIX con SCM_RIGHTS y `memfd_create`
  primero; Skia y las demás bibliotecas; Ladybird sin ventana).
- **IPv6**: los trozos (la cabecera de fragmentación: hoy se tiran), DAD (la
  detección de duplicados antes de usar la dirección), las direcciones
  temporales (RFC 8981), DHCPv6, MLD (para los conmutadores que filtran la
  multidifusión), las vidas de las direcciones y del router (hoy, para
  siempre), el MTU de la ruta (ICMPv6 «paquete demasiado grande»), y
  `::1 localhost` en `/etc/hosts` (no se ha puesto aún: los que piden
  «localhost» seguirían yendo por IPv4, pero cambiaría lo que eligen curl y
  musl); IFCONFIG, sin las de IPv6 (las dice el núcleo al tenerlas).
- **El núcleo**: `dup2` sobre un descriptor que otro hilo está **abriendo**
  da EBUSY (como Linux; musl lo repite); los aplazados, 32 por proceso; de
  la auditoría, **H3, H4 y H11 a H13** (los que piden cerrojos nuevos: hay
  que hacerlos con calma y con su prueba de carrera, como MHILOS para los
  descriptores), y la tabla de arreglos también para las copias si algún
  día dejan de ir por el mapa directo.
- **Con TCG** (QEMU sin KVM) y 4 u 8 procesadores, abrir y cerrar a la vez
  sigue costando (los avisos entre procesadores son lentos en TCG: lo que
  queda de MHILOS es eso; con KVM, 41 ms).
- El compilador cruzado: hacer también así lo de C (hoy, el gcc del
  anfitrión con las cabeceras de musl: `musl-gcc`), y que pruebas.sh lo
  rehaga solo si falta (hoy lo dice y no compila los de C++).
- De la 11 b, sin tocar: suspender (S3), los P-states sin HWP, los GPE y la
  SCI, `_SRS`, IPP y la mezcla del sonido.
- La etapa 14 (NVIDIA): sin empezar.

## 21. La etapa 10 d, hecha (26/11/2026): los sockets de Unix, la memoria compartida por descriptor y el vigilante

### Las decisiones

- **Por qué la 10 d**: es lo primero de la lista de la sección 20 («lo que
  pide Ladybird sin ventana»): Ladybird es de varios procesos (el navegador,
  WebContent, RequestServer, ImageDecoder) que se hablan por **sockets de
  Unix**, se pasan **descriptores** y comparten los mapas de bits por
  **memoria con descriptor** (Core::AnonymousBuffer: `memfd_create`,
  `ftruncate` y `mmap MAP_SHARED`).
- **Los sockets de Unix, un módulo propio** (`unix64.cas`, la clase de
  archivo abierto 17, `F_UNIX`), no dentro de Red64: no tienen nada de la
  pila de red. Sistema reparte cada llamada (41 a 55 y 288) por la clase del
  descriptor; `socket(AF_UNIX, ...)` y `socketpair` (53, antes EOPNOTSUPP) los
  hace Unix64. SOCK_STREAM, SOCK_DGRAM y SOCK_SEQPACKET, como Linux.
- **Una cola de mensajes por socket** (la del que recibe): cada envío es un
  mensaje (los de flujo, de hasta 64 KB cada uno), con quien lo manda (pid,
  uid y gid: SCM_CREDENTIALS), su nombre (para recvfrom) y los descriptores que
  viajan con él. En los de flujo se leen seguidos, pero un mensaje con
  descriptores no se junta con lo de antes y la lectura se para después de
  él, como Linux (así los descriptores llegan con su mensaje: LibIPC cuenta
  con eso). Lo que cabe en cada cola: 256 KB (contando 256 octetos por
  mensaje) y, en los de datagramas, 64 mensajes; poll da POLLOUT con la
  mitad libre (como `unix_writable` de Linux: el que espera a POLLOUT no
  vuelve para escribir un poco y quedarse otra vez sin sitio).
- **Un solo mutex para todos los sockets de Unix**: el otro extremo puede
  soltarse en cualquier momento (otro proceso lo cierra), y con el mutex
  tomado nadie se suelta mientras se mira; lo del programa se copia antes de
  tomarlo (al mandar) o después de soltarlo (al recibir), a un búfer del
  núcleo. Las esperas, en Sondeo64 (como la red): poll, select y epoll los
  ven con `listo_unix`.
- **Pasar descriptores (SCM_RIGHTS)**: al mandar, cada descriptor se toma
  (`toma_fd`, el de la 10 c) y se apunta su clase, su objeto y su
  O_NONBLOCK con una referencia más (`Procesos.otro_fd_obj`, nueva: como
  dup); al recibir, cada uno es un número nuevo del programa (con
  MSG_CMSG_CLOEXEC, FD_CLOEXEC). Si no caben en el búfer de control, los de
  más se cierran y se da MSG_CTRUNC (en CMSG_SPACE de uno caben dos, por el
  relleno hasta 8: igual que Linux). Al soltar un socket con mensajes en su
  cola, sus descriptores se cierran **sin el mutex** (cerrar uno puede soltar
  otro socket de Unix). Hasta 253 por mensaje (SCM_MAX_FD).
- **Los nombres**: bind a una ruta (relativa: desde la carpeta actual), a un
  nombre abstracto (el primer octeto, 0) o el automático (bind con solo la
  familia: «\0» y cinco cifras). **La ruta no crea un archivo en el disco**:
  es un nombre del núcleo; `stat` lo ve como S_IFSOCK, `unlink` lo quita (el
  socket sigue, con su nombre para getsockname) y se suelta al cerrarlo. Es
  lo que usan los programas (unlink antes de bind) sin tocar los discos, que
  casi siempre son de solo leer. connect a una ruta que no está: ENOENT; a un
  abstracto que no está, o a uno que no escucha: ECONNREFUSED, como Linux.
- **SO_PEERCRED** (el pid del otro al conectar o al hacer el socketpair),
  SO_PASSCRED, SO_TYPE, SO_DOMAIN, SO_PROTOCOL, SO_ERROR, SO_ACCEPTCONN,
  SO_SNDBUF y SO_RCVBUF, SO_RCVTIMEO y SO_SNDTIMEO; FIONREAD, FIONBIO,
  TIOCOUTQ; shutdown (el otro ve el final, o EPIPE); EPIPE con SIGPIPE salvo
  con MSG_NOSIGNAL; si un extremo se va con lo suyo sin leer, el otro recibe
  ECONNRESET una vez (como Linux).
- **`memfd_create` es la memoria de /dev/shm sin nombre** (Disp64, etapa 8):
  ya tenía `ftruncate`, `mmap MAP_SHARED` de verdad (las mismas físicas en
  cada proceso, con su cuenta en Espacios) y soltarse cuando nadie la tiene
  abierta ni puesta. memfd_create (319) hace una ya «borrada» (sin entrada en
  la tabla de nombres: no cuenta para sus 64). Con sus **sellos**
  (`F_ADD_SEALS`, `F_GET_SEALS`: SHRINK y GROW en ftruncate, WRITE y
  FUTURE_WRITE en un mmap de escribir; sin MFD_ALLOW_SEALING, ya sellada con
  F_SEAL_SEAL). Se pasa a otro proceso por SCM_RIGHTS como cualquier
  descriptor.
- **El cuelgue de la 0.11.3 en VirtualBox con 4 procesadores** («Despertar
  en bucle», con la BIOS y con UEFI; con 1 procesador, bien): no ha salido en
  QEMU en 290 arranques (KVM con 4 y 8 procesadores; con 4 procesadores en 1
  o 2 núcleos del anfitrión, que es lo que hace VirtualBox en un equipo
  cargado; PIIX sin HPET y sin x2APIC, como VirtualBox; y TCG). Lo que se ha
  hecho, en vez de adivinar:
  - **el vigilante** (Hilos64.vigila, desde el tic del procesador 0, que mira
    a los demás, y del 1, que mira al 0, cada medio segundo): un procesador
    en marcha cuyo tic no se mueve en 5 s se ha quedado con las
    interrupciones quitadas; se le manda un NMI y Excep64 dice, en la
    pantalla, «El vigilante: este procesador lleva 5 s sin atender su tic» y
    la función y quién la ha llamado (antes, el sistema se quedaba quieto sin
    decir nada). Uno ocioso con hilos en su cola 2 s seguidos sin cambiar de
    hilo (un aviso de replanificar perdido): se dice entre paréntesis y se le
    avisa otra vez. Y la hora del núcleo que va hacia atrás, también se dice.
    Con `prueba=vigilante` (un hilo que quita las interrupciones y da
    vueltas), en la batería;
  - **despertar con límite**: `Hilos64.despierta` esperaba sin fin a que el
    hilo despertado dejase su procesador; si en 5 s no lo deja, se para y lo
    dice (el hilo y el procesador);
  - **el HPET de 32 bits** se leía como si fuese de 64: a los 5 minutos (a
    14,3 MHz) la hora volvía atrás y los que dormían (también el hilo que
    pinta la pantalla) no despertaban. Ahora se suma como el reloj de ACPI.
    `hpet32` en la línea de órdenes lo hace pasar por ahí (y pone el
    contador a 4 s de dar la vuelta): en la batería, con 4 procesadores.
  Lo que diga en VirtualBox el vigilante (o que ya no se cuelgue) dirá la
  causa; la imagen va en `Pendientes/VirtualBox/Hispa-64-etapa-10c-arreglo/`.
- **De la auditoría (sección 20, «lo que queda»)**: **H11** (FUTEX_WAKE
  despertaba después de soltar el cerrojo del cubo: uno al que ya había
  despertado su plazo podía volver, acabar y soltarse entre medias, y
  `despierta` tocaba un hilo suelto; ahora con el cerrojo tomado, como
  Sondeo64 y REQUEUE) y **H4** (el hilo que sale, si no es el último, deja el
  espacio del programa y lo suyo del proceso antes de salir de la lista y
  con el cerrojo; antes, entre medias, el último podía destruir el espacio
  que aún tenía en su CR3 y soltar el proceso). Con su prueba de carrera
  (MUNIX). H3 ya lo arregló la 10 c (`toma_fd`); H12 y H13, abajo.
- **`/tmp`, de escribir siempre** (lo pidió Carlos: `curl -o` en la «/» daba
  «Read-only file system»): un disco en memoria (Discos64, la clase
  `D_MEM`, «tmp»: 32 MB de sectores de 512) con su FAT16 hecha al arrancar
  (8.179 cúmulos de 4 KB, 512 entradas en la raíz), que el VFS monta en
  `/tmp` como cualquier volumen y por el que pasa la caché de bloques como
  por los demás. Sus páginas, solo las que se escriben (leer lo no escrito da
  ceros); lo guardado cuenta como suyo en las cuentas de la memoria de las
  pruebas. Se pierde al apagar. `sintmp` lo quita.
- **La plantilla de los registros de los programas** (los de x87, SSE y AVX
  al empezar) se hacía con xsave sobre un bloque del montón sin poner a
  cero: xsave no escribe los octetos 528 a 575 de la cabecera, y lo que
  traía el bloque pasaba a cada programa nuevo; su xrstor daba #GP y el
  núcleo se paraba en `pon_fpu` (con `sinpruebas` y xsave: KVM con -cpu
  host, y en VirtualBox también). Ahora, a cero antes (como `nuevo_fpu`
  desde la etapa 9).
- **El menú Inicio** del escritorio: «Consola (salir a las órdenes)» (lo
  que antes decía «Salir del escritorio»: deja el intérprete). Y `ayuda` de
  ORDENES dice dónde guardar (`/tmp`, o, para que quede, con «escritura en
  los discos», la partición de pruebas del disco, `/sata0p2` en VirtualBox).
- Hispa-64 **0.11.4** (uname: «etapa 10 d (AF_UNIX y memfd)»).

### Qué hace

| Archivo | Qué |
|---|---|
| `unix64.cas` | los sockets de Unix: socket, socketpair, bind, listen, connect, accept, send/recv y sus formas, SCM_RIGHTS y SCM_CREDENTIALS, las opciones, ioctl, poll |
| `sistema.cas` | el reparto por la clase (`es_unix`), memfd_create (319), F_ADD_SEALS y F_GET_SEALS, unlink y stat de la ruta de un socket, readv/writev de una vez |
| `procesos.cas` | `otro_fd_obj`; H4 en `sal_hilo` |
| `disp64.cas` | `memfd_crea`, `sellos_shm`, los sellos en ftruncate y mmap |
| `futex64.cas` | H11 |
| `hilos64.cas`, `excep64.cas`, `porcpu64.cas` | el vigilante, despertar con límite |
| `reloj64.cas` | el HPET de 32 bits; `hpet32` |
| `prsmp64.cas` | `prueba=vigilante` |
| `discos64.cas`, `prdisc64.cas` | el disco en memoria de `/tmp` (`crea_tmp`, `mem_es`) |
| `procesos.cas` (`prepara_procesos`) | la plantilla de los registros, a cero |
| `programas/musl/munix.c` | MUNIX: 77 pruebas (también en Linux: las mismas) |
| `programas/cxx/mipc.cpp` | MIPC (C++23): dos procesos como los de Ladybird |
| `programas/ventanas/ventanas.c`, `programas/ordenes.c` | la consola en el menú Inicio; dónde guardar en `ayuda` |

Lo que dicen (q35, 4 procesadores, con KVM):

```
MUNIX: los sockets de Unix (AF_UNIX) y memfd_create; estático
  socketpair de flujo, datagramas y SOCK_SEQPACKET: MSG_PEEK, MSG_TRUNC, FIONREAD, el final, EPIPE y SIGPIPE
  O_NONBLOCK: EAGAIN al leer y con la cola llena (240 KB); poll y epoll (EPOLLIN, EPOLLRDHUP, EPOLLHUP)
  bind a una ruta (stat: S_IFSOCK, EADDRINUSE, unlink), listen, connect, accept, SO_PEERCRED; abstracto y automático; sendto y recvfrom
  SCM_RIGHTS a otro proceso (una tubería, un eventfd y un memfd hechos después del fork), MSG_CTRUNC; SCM_CREDENTIALS; memfd compartido y sus sellos; 300 pasados
  dos hilos: 10.000 mensajes de ida y vuelta en 1187 ms; 4 MB por un socket de flujo, la suma bien
  carreras: 40 rondas de futex con plazo contra FUTEX_WAKE y 30 procesos que acaban con sus hilos saliendo a la vez
  /tmp: 100 KB escritos, leídos y borrados
MUNIX: todo bien (77 pruebas)
MIPC: dos procesos que se hablan como los de Ladybird (C++23, 14.3.0)
  posix_spawn con el socket en el 3 y SOCKET_TAKEOVER; mensajes de LibIPC con SCM_RIGHTS, desde un hilo que manda y un bucle con poll
  dos mapas de 256 x 256 compartidos (memfd): 60 cuadros pintados por el otro proceso, la suma bien, en 36 ms
MIPC: todo bien (8 pruebas)
```

MIPC hace lo que hace Ladybird al arrancar WebContent: un socketpair de
flujo, `posix_spawn` del otro con su extremo en el descriptor 3
(`posix_spawn_file_actions_adddup2`) y `SOCKET_TAKEOVER=WebContent:3` en el
entorno (LibCore); los mensajes con la forma de LibIPC (el largo, el número
y lo suyo, con los descriptores por SCM_RIGHTS) mandados desde un hilo con su
cola, su mutex y su condición (IPC::TransportSocket), y leídos en un bucle
con poll del socket y de un eventfd (Core::EventLoop); dos mapas de bits de
256 x 256 hechos con memfd (Core::AnonymousBuffer), pasados una vez, en los
que el otro proceso pinta 60 cuadros por turno y dice su suma, que el
primero mira en su memoria (la misma).

### Cómo se ha probado

- MUNIX y MIPC, en Linux (las mismas 77 y 8) y en Hispa-64 en cada arranque
  con musl (`nucleo64/pruebas.sh`, etapa 7: por multiboot y por UEFI, con 1
  y 4 procesadores y -cpu max); después, ni un socket de Unix (`mal` si
  queda alguno), ni memoria compartida, ni memoria ni montón perdidos.
- `prueba=vigilante` con 4 procesadores: el NMI, «El vigilante: ...», en
  `PrSmp64.atascado` y quién la llamó; `hpet32` con 4 procesadores, el
  contador dando la vuelta a los 4 s: todo en orden y el vigilante, callado.
- El cuelgue de VirtualBox: 290 arranques sin él (arriba); las imágenes, por
  UEFI desde el .vdi y por la BIOS desde el CD, con 4 procesadores: todo en
  orden y el escritorio.
- La batería: abajo (las cifras, al acabar).

### Lo que queda (y más adelante)

- **El navegador**: lo siguiente de la lista de la sección 20 (`readlink
  /proc/self/exe` ya va; `pthread_setname_np`, `sched_getaffinity` con el
  número de verdad), CMake y Ninja con el archivo de «toolchain» para el
  compilador cruzado, las bibliotecas (Skia, libjpeg-turbo, libpng, libwebp,
  woff2, simdutf, fast_float, SQLite, libxml2) y Ladybird sin ventana.
- **Los sockets de Unix**: la ruta de bind en el disco de verdad (hoy, un
  nombre del núcleo); el recolector de ciclos (un socket que viaja en su
  propia cola no se suelta nunca: Linux tiene `unix_gc`); SO_PEERSEC y
  SCM_PIDFD (no los pide Ladybird); read y write de un memfd (solo mmap).
- **`/tmp`**: devolver las páginas de lo que se borra (hoy se quedan hasta
  apagar: FAT no dice qué cúmulos quedan libres al disco de abajo).
- **De la auditoría**: H12 (un hilo que migra a mitad de un aviso de
  vaciar la TLB) y H13 (vfork: el hijo con sus regiones y el padre con las
  suyas sobre las mismas tablas).
- **VirtualBox**: lo que diga el vigilante en el equipo de Carlos.
