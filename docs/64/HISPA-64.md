# Hispa-64 — el plan

*23/11/2026. Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.*

Hispa-64 es el miembro de 64 bits de la familia Hispa-DOS: 8086 → 386 → 486 →
Pentium (gama XP, 32 bits) → **Hispa-64** (x86-64 de hoy) → Hispa-ARM. Su meta,
dicha por Carlos: **un sistema usable para una oficina de hoy en día**, en los
portátiles de ahora (UEFI, NVMe, xHCI, varios núcleos, Wi-Fi moderna), aunque
cueste un año.

Este documento es el plan: lo que se quiere, las decisiones de arquitectura con su
porqué, y las etapas, cada una con lo que se verá funcionar y cómo se prueba. Las
**etapas 0, 1, 2, 3 y 4 están hechas** (secciones 6, 7, 8, 9 y 10).

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
| 5 | NVMe, AHCI, GPT, VFS, FAT32/exFAT | 5–7 sem. | 6,5 meses |
| 6 | xHCI, HID, i8042, I2C-HID, USB de almacenamiento | 5–7 sem. | 8 meses |
| 7 | musl, POSIX, herramientas de compilar cruzado | 6–8 sem. | 9,5 meses |
| 8 | Gráficos: compositor, ventanas, ratón, oficina | 5–7 sem. | 11 meses |
| 9 | Red por cable, TCP/IP, TLS; Wi-Fi en paralelo | 6–8 sem. (+ Wi-Fi 3–5 meses) | 12,5 meses |
| 10 | Navegador moderno (Ladybird o WebKit) | 8–12 sem. | 15 meses |
| 11 | Impresión IPP, sonido HDA, energía (suspender, batería) | 4–6 sem. | (en paralelo) |
| 12 | Instalar en el disco, actualizar, cuentas | 3–4 sem. | (en paralelo) |
| 13 | (opcional) Cargador UEFI propio, Secure Boot; UEFI de 32 bits | 3–4 sem. | — |

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

### Etapa 5 — discos y archivos

- NVMe (colas con MSI-X), AHCI (de Hispa-Pentium), GPT, un VFS con caché de páginas,
  FAT32 y exFAT (las memorias USB y la partición EFI); leer NTFS y ext4, más tarde.
- **Se verá**: `dir` de la memoria USB y del disco interno, copiar a 1 GB/s en NVMe.
- **QEMU**: `-device nvme`, `-device ahci`, discos de 4 GB y 2 TB (GPT). **Taller**:
  ThinkPad y Monovate (NVMe), ASUS y MacBook (AHCI). **Nunca escribir en el disco
  interno de un equipo de Carlos sin su permiso**: las primeras versiones, solo leer.

### Etapa 6 — USB y la entrada

- xHCI (de Hispa-Pentium, con el BAR donde esté), HID, almacenamiento masivo, i8042
  (teclado de los portátiles, con la distribución española), I2C-HID (touchpads).
- **Se verá**: el teclado del portátil y uno USB, el touchpad, un ratón USB, una
  memoria USB.
- **QEMU**: `-device qemu-xhci`, `usb-kbd`, `usb-tablet`, `usb-storage`.
  **Taller**: todos (cada uno tiene su touchpad y su teclado).

### Etapa 7 — POSIX y musl

- musl portada, las llamadas de 2.8 (1 a 6), hilos de POSIX, `epoll`; el compilador
  cruzado `x86_64-linux-musl` (gcc o clang) para hacer programas de Hispa-64 desde el
  servidor.
- **Se verá**: BusyBox (GPL-2.0) o las herramientas de toybox, Lua, SQLite, un
  `make` corriendo en Hispa-64; las pruebas de libc-test de musl.
- **QEMU**: las pruebas de libc-test. **Taller**: la misma imagen.

### Etapa 8 — gráficos, ventanas y la oficina

- Compositor, ventanas, cursor, la letra con suavizado (TTF de la oficina), el
  escritorio de Hispa; la oficina de Hispa-Pentium con `so_hispa64.c`.
- **Se verá**: Pluma abriendo un docx, Cuentas un xlsx, el lector de PDF, a la
  resolución del panel (1920×1080 en el ThinkPad).
- **QEMU**: capturas (`screendump`) comparadas en las pruebas. **Taller**: todos.

### Etapa 9 — la red

- Realtek 8111/8168, Intel I219, USB RTL8153; TCP/IP (lo de Hispa-Pentium o lwIP,
  BSD), DHCP, DNS, `socket` POSIX; TLS con BearSSL (como en Hispa-Pentium).
- **Wi-Fi en paralelo** (3–5 meses): Intel AX201/AX211 (firmware de Intel, que se
  carga como archivo aparte), WPA2-PSK primero; lo aprendido con el AR9285 y el
  Broadcom de Hispa-Pentium sirve para el 802.11 y el WPA.
- **Se verá**: descargar por HTTPS, NTP, SSH.
- **QEMU**: `-nic user,model=e1000e`, `rtl8139`, `usb-net`. **Taller**: cable en el
  ASUS (Realtek), el ThinkPad, el adaptador USB.

### Etapa 10 — el navegador moderno

- C++ completo (libc++ y libunwind, o libstdc++), FreeType, HarfBuzz, ICU, curl,
  y Ladybird (o WPE WebKit) con su ventana sobre el compositor.
- **Se verá**: el correo web, ClicGest, la web del banco.
- Es la etapa con más riesgo: puede pedir más memoria, `mmap` con más formas,
  señales finas y hilos a cientos. De ahí los 8–12 semanas.

### Etapa 11 — imprimir, sonar, ahorrar

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
- `castel/nucleo64/pruebas.sh` (solo, unos 6 minutos desde la etapa 4) y `SOLO=castel64 ./pruebas.sh`
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
cargado). Las imágenes de la carpeta del taller (el disco por UEFI con 4
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
