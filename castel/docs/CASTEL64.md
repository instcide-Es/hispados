# Castel de 64 bits — x86-64, CASTEL64 y el primer ladrillo de Hispa-64

*21/11/2026. Fase 7 de CIMIENTOS.md. Define cómo genera Castel código de 64 bits
para x86-64 en modo largo, y cuenta CASTEL64: el compilador escrito en Castel,
compilado para 64 bits, que se compila a sí mismo. Lo de 16 bits (CASTEL.md) y lo
de 32 (CASTEL32.md) no cambian: ni un octeto de lo que sale para Hispa-8086 ni para
Hispa-Pentium. Es el terreno para Hispa-64.*

Regla de oro, la de siempre: **el núcleo, pequeño.** Lo que el lenguaje no sabe
decir se hace en un bloque `ensamblador`, ahora con los registros de 64 bits.

---

## 1. En resumen

- **Dos modelos nuevos**: `modelo linux64` (un programa de Linux para x86-64: un
  ELF estático que corre en cualquier Linux de 64 bits, como el del servidor) y
  `modelo núcleo64` (un núcleo de 64 bits: multiboot y multiboot2, que pasa solo al
  modo largo). `linux64` sirve para probarlo todo deprisa sin tener aún Hispa-64;
  `núcleo64`, para empezar Hispa-64.
- **Los tipos `nat64` y `ent64`**, y los punteros de 8 octetos. Las cuentas son de 32
  bits, como en el modelo plano, salvo las que llevan uno de 64 (como en C, LP64).
- **La parte de atrás de 64 bits está en los dos compiladores**: `Generador64` en
  `ccastel.py` y el módulo **`Gen64`** (`compilador/32/gen64.cas`) en el compilador
  escrito en Castel. Sacan lo mismo, letra a letra.
- **Los dos ensambladores entienden `bits 64`**: `ensambla.py` y Montador (ENSAMBLA,
  ENSAMB32, **ENSAMB64**), con los mismos octetos que NASM.
- **CASTEL64** (`compilador/64/linux/castel64.cas`) y **ENSAMB64**
  (`ensamblador/64/linux/ensamb64.cas`) son programas de Linux x86-64. CASTEL64 se
  compila a sí mismo hasta el **punto fijo** (ccastel.py → paso 1 → paso 2 → paso 3,
  idénticos, octeto a octeto) en **0,27 s**, y saca lo mismo que ccastel.py en
  **todos** los programas de las pruebas, de 16, 32 y 64 bits: es **el compilador
  cruzado de toda la familia** (Hispa-8086, Hispa-386/486, Hispa-Pentium e Hispa-64).
- **El primer ladrillo de Hispa-64** (`nucleo64/hola.cas`): arranca en QEMU x86_64 y
  con GRUB, pasa al modo largo con la paginación de cuatro niveles, atiende una
  interrupción con una función de Castel y dice «Hola desde Hispa-64» por el puerto
  serie.

---

## 2. Los modelos

```
módulo Programa
    modelo linux64        ; un programa de Linux x86-64 (ELF estático)
    ...
```

```
módulo Núcleo
    modelo núcleo64       ; un núcleo de 64 bits: multiboot (QEMU -kernel) y multiboot2 (GRUB)
    ...
```

`linux64` y `núcleo64` no son palabras reservadas (solo cuentan detrás de `modelo`).
Comparten toda la generación de 64 bits; solo cambian el arranque (§7) y adónde va
lo que se escribe: en `linux64`, a la salida de Linux (`write`); en `núcleo64`, como
en el núcleo de 32 bits, al puerto serie COM1 y a la pantalla de texto.

Lo que se escribe va, como en toda la familia, en la **página 437**: un programa de
`linux64` escribe los acentos en 437 (para verlos en un terminal de Linux,
`| iconv -f CP437`). Así el compilador saca en Linux los mismos mensajes, octeto a
octeto, que en Hispa-8086.

---

## 3. Los tipos y las cuentas

- **`nat64`** (de 0 a 18.446.744.073.709.551.615) y **`ent64`** (con signo), de 8
  octetos. Solo existen en los modelos de 64 bits; en los demás son un error («nat64
  es de 64 bits (modelo linux64 o núcleo64)»). Se convierte con `nat64(...)` y
  `ent64(...)`.
- **Un puntero ocupa 8 octetos**; un texto también. Las estructuras con punteros
  crecen en consecuencia (`tamaño(Nudo)` con un `ent64`, un `byte` y un puntero: 17).
- **Las cuentas se hacen en 32 bits salvo que haya uno de 64 por medio** (un
  `nat64`, un `ent64` o un puntero): entonces, en 64. Es la regla de C en x86-64
  (LP64: `int` sigue siendo de 32) y la del modelo plano de 32 bits: **lo escrito
  para 32 bits da lo mismo en 64**. Por eso el compilador entero, que se escribió
  para 16 y 32 bits, corre igual compilado para 64.
- Con signo si alguno de los dos lo lleva (la regla de siempre). Al pasar a 64 bits,
  uno de 32 o menos **con signo se ensancha con su signo** (`ent64(-1)` y
  `nat64(-1)` son todo unos), y uno sin signo, con ceros.
- La resta de dos punteros da un `nat64`. Desplazar uno de 64: de 0 a 63 bits.
- **Los números escritos siguen siendo de 32 bits como mucho** (y las constantes
  igual, y lo de 64 bits no se calcula al compilar): uno mayor se arma con cuentas,
  `nat64(0x12345678) << 32 ob 0x9ABCDEF0`. (Ver §11.)

```
variable grande: nat64
grande := nat64(1) << 40 + 5            ; 1099511627781
escribir(cuenta32 + cuenta32)           ; de 32 bits: da la vuelta
escribir(nat64(cuenta32) + cuenta32)    ; de 64: no
```

`ejemplos64/largo64.cas` lo prueba todo (con su salida esperada), y
`azar64.py` hace programas de cuentas al azar con estas reglas y lo que tienen que
dar, calculado en Python.

---

## 4. Cómo queda en la máquina

- **Modo largo, x86-64.** Una expresión de 32 bits o menos deja su valor en **EAX**
  con los 32 de arriba de **RAX a cero** (cada instrucción de 32 bits los deja así; al
  recortar uno de 64 a 32 se hace `mov eax, eax`); una de 64, en **RAX** entero.
  Calcular gasta RAX, RBX, RCX, RDX, RSI y RDI; se conservan RBP y RSP (y r8-r15 no
  los toca el código generado: son para los bloques `ensamblador`).
- **Las direcciones**, con los registros de 64 (`[rbp-8]`, `[rbx+rsi*8+16]`); las
  variables globales y las funciones, por su dirección: `[G_x]` (con el SIB de 32
  bits que pone NASM) o `mov ebx, G_x`. Todo el programa (código, datos, la pila y la
  memoria que pide) va **por debajo de los 2 GB**: es el «modelo pequeño» de x86-64
  (el `-mcmodel=small` de gcc), y así no hace falta ir relativo a RIP.
- **La llamada** es la de 32 bits a ocho octetos: los argumentos, de derecha a
  izquierda, **ocho octetos cada uno** (de un `ref`, la dirección); quien es llamado
  los quita (`ret n`); el resultado, en RAX. Marco con RBP: el primer parámetro en
  `[rbp+16]`, el siguiente en `[rbp+24]`...; las locales, desde `[rbp-8]`, cada una
  en un múltiplo de ocho.

  *Por qué una propia y no la System V AMD64 (la de Linux y gcc, con los seis
  primeros en registros).* Es la misma idea que en 16 y 32 bits, así que la parte de
  atrás de 64 bits es casi la de 32 cambiando el tamaño (y el compilador escrito en
  Castel lleva la misma forma en las tres); y nada de lo de Hispa necesita hoy
  enlazar con C. Cuando haga falta (para llamar a una biblioteca de C), será una
  marca en la declaración de la función de fuera (`externa`, con su puente que pase
  los argumentos a RDI, RSI, RDX, RCX, R8 y R9 y alinee la pila), sin tocar lo demás.
- **La pila**: el arranque la deja alineada a 16; el código generado no depende de
  eso (no usa SSE), y las interrupciones del modo largo la alinean solas. Lo pedirá el
  puente a C, no antes.
- **Funciones de interrupción** (`interrupción función`, en `núcleo64`): guardan los
  quince registros (RAX a R15), y su único parámetro, si lo llevan, es el número de
  error que deja el procesador, en `[rbp+120]`; vuelven con `iretq`. En la IDT de 64
  bits cada puerta son 16 octetos (`nucleo64/hola.cas` lo hace).

---

## 5. El sistema, el ensamblador en línea y la memoria

- Como en 32 bits, **no están** `interrupción()`, `acarreo()`, `reg_bx()`... (son del
  modo real): «interrupción() es del modo real (BIOS y DOS): en 64 bits no está (se
  usa un bloque «ensamblador»)». Sí están `meter_puerto` y `sacar_puerto`.
- En un bloque **`ensamblador`**, `{variable}` es `[rbp-8]` o `[G_x]` (un parámetro
  `ref`, `[rbp+16]`, donde está su dirección), `{CONSTANTE}` su número (con signo) y
  `{función}` su etiqueta. Una función que solo tiene `ensamblador` devuelve lo que
  quede en RAX:

```
público función sistema(n: nat64, a: nat64, b: nat64, c: nat64): ent64
    ensamblador
        mov rax, {n}
        mov rdi, {a}
        mov rsi, {b}
        mov rdx, {c}
        syscall
    fin
fin
```

- **La memoria «por párrafos»** (`leer_palabra(p, d)`...) sigue siendo la dirección
  `p * 16 + d`, ahora calculada en 64 bits: puede pasar de los 4 GB. Así guarda el
  compilador sus tablas también en 64 bits.

---

## 6. La biblioteca de Linux (`biblioteca64/linux`)

La misma forma de usarla que en el DOS (CASTEL.md §12), para que lo escrito para
Hispa-8086 corra igual en Linux; se pide con `-I biblioteca64/linux` **delante** de
las otras carpetas (como `biblioteca32/pentium` en 32 bits):

- **`Linux`**: `sistema(n, a, b, c)` y `sistema6(n, a, b, c, d, e, f)` (la llamada al
  sistema `n`: lo que da, o el error en negativo), los números de las llamadas que se
  usan (`LEER`, `ESCRIBIR`, `ABRIR`, `CERRAR`, `SITUAR`, `MAPEAR`, `BORRAR`,
  `LEER_ENLACE`) y `pila_inicial()` (dónde dejó Linux el número de argumentos y sus
  direcciones).
- **`Lejos`**: los bloques de memoria «lejana» se piden con `mmap` (con `MAP_32BIT`:
  por debajo de los 2 GB, como el resto del programa); su segmento es su primer
  párrafo.
- **`Archivos`**: los de siempre (`abrir`, `crear`, `leer`, `grabar`, `situar`,
  `largo`, `borrar`, `Lector`, `Escritor`...), con las llamadas de Linux. Las rutas
  como en el DOS: `\` vale como `/` y, si un archivo no está tal cual, se busca con el
  último trozo en minúsculas (`CASTEL.CAS` encuentra `castel.cas`). `error_dos` es el
  error de Linux (`ENOENT` = 2...).
- **`Argumentos`**: `cuántos_argumentos`, `argumento(n, ...)` y `ruta_del_programa`
  (de `/proc/self/exe`).
- `Textos` y `Memoria` son los de `biblioteca/`, que valen tal cual.

`ejemplos64/sistema64.cas` lo prueba (archivos, rutas del DOS, Lector, situar,
borrar, uno que no está, argumentos y la memoria lejana).

---

## 7. El arranque

### `modelo linux64` → un ELF estático (`org 0x400000`)

- Las **cabeceras ELF64 a mano** (la del archivo y un solo `PT_LOAD`, de lectura,
  escritura y ejecución: el código, los datos y, detrás, lo que empieza a cero, que
  pone a cero Linux). No hace falta enlazador.
- El arranque guarda dónde dejó Linux la pila (`C_pila_linux`, para Argumentos), pone
  **su propia pila** (16 MB dentro de lo que empieza a cero: así todo va por debajo
  de los 2 GB) y llama a `principal()`. `escribir` va por `write` (la llamada 1, hasta
  que vaya todo) y `salir` por `exit_group` (la 231).

### `modelo núcleo64` → un núcleo de 64 bits (`org 0x100000`)

- Lleva **dos cabeceras**: la de **multiboot** (la que entiende `qemu-system-x86_64
  -kernel`, con la «a.out kludge», sin ELF) y la de **multiboot2** (GRUB:
  `multiboot2 /boot/NUCLEO64.BIN`), con la dirección de carga y la de entrada.
- Los dos dejan el procesador en modo protegido de 32 bits. El arranque (en `bits
  32`) apunta la magia y la información del cargador (`C_mb_magia`, `C_mb_info`),
  pone a cero lo que empieza a cero, monta la **paginación de cuatro niveles** (PML4,
  PDPT y cuatro PD con páginas de 2 MB: los 4 primeros GB tal cual, en seis páginas
  desde `0x70000`), enciende PAE, carga CR3, pone `EFER.LME` y la paginación, y salta
  con una **GDT de 64 bits** (código `0x08` con L=1, datos `0x10`) al modo largo.
- Ya en 64 bits: los segmentos, la pila (64 K), la pantalla en blanco, el puerto
  serie COM1 a 115200 y `principal()`. `salir` lo dice por el puerto `0xF4` (el
  `isa-debug-exit` de QEMU) y para el procesador.

---

## 8. El ensamblador de 64 bits

`ensambla.py` y Montador (ENSAMBLA por dentro) entienden, con `bits 64`, lo que saca
el compilador y lo corriente de un bloque `ensamblador`, con **los mismos octetos que
NASM** (`ensamblador/formas64.asm` tiene una muestra de cada forma):

- Los registros `rax`...`rdi` y `r8`...`r15` (y sus partes `r8d`, `r8w`, `r8b`...),
  `qword`, el prefijo **REX** (W, R, X, B) donde hace falta, la memoria con los
  registros de 64 (`[r12]` y `[r13]` como NASM; con los de 32, el prefijo 67h) y la
  dirección fija con SIB (`[0x1000]`: sin él sería relativa a RIP).
- `movsxd`, `cdqe`, `cqo`, `iretq`, `syscall`, `sysret`, `rdmsr`, `wrmsr`, `cpuid`,
  `rdtsc`, `stosq`, `movsq`, `lodsq`..., `push`/`pop` de 64, `dq` y `resq`.
- `mov` de un número a un registro de 64, como NASM: el de 32 bits si vale (pone a
  cero los de arriba), el de 32 con su signo (`C7h`) si es negativo y cabe, o el de 64
  entero; con una etiqueta, el de 64 entero (por eso el compilador escribe `mov ebx,
  G_x`).
- Las cuentas, en `bits 64`, son de 64 bits (salvo multiplicar, dividir y el resto,
  que se hacen con los 32 de abajo). En Montador cada cuenta lleva su palabra alta
  aparte (`e_alto`).
- `bits 32` y `bits 64` en el mismo archivo (el arranque de `núcleo64`).

---

## 9. CASTEL64

### Los mismos fuentes

Como CASTEL32 (CASTEL32.md §11), **no hay un compilador de 64 bits aparte**: son los
módulos de `compilador/` (y `ensamblador/montador.cas`) tal cual; lo que cambia está
en módulos pequeños que el programa importa antes que nada:

| | 16 bits | Hispa-Pentium | HW32 | **Linux x86-64** |
|---|---|---|---|---|
| el programa | `compilador/castel.cas` | `compilador/32/pentium/castel32.cas` | `compilador/32/hw32/castelw.cas` | **`compilador/64/linux/castel64.cas`** (`modelo linux64`) |
| su ensamblador | `ensamblador/ensambla.cas` | `ensamblador/32/pentium/ensamb32.cas` | `ensamblador/32/hw32/ensambw.cas` | **`ensamblador/64/linux/ensamb64.cas`** |
| las medidas | `compilador/medidas.cas` | `compilador/32/medidas.cas` | la misma | **`compilador/64/medidas.cas`** |
| la parte de atrás de 64 bits | `compilador/gen64.cas` (vacía) | `compilador/32/gen64.cas` | la misma | la misma |
| el sistema | `biblioteca/` | `biblioteca32/pentium/` | `biblioteca32/hw32/` | **`biblioteca64/linux/`** |
| lo que sale | CASTEL.EXE, ENSAMBLA.COM | CASTEL32.BIN, ENSAMB32.BIN | CASTELW.EXE, ENSAMBW.EXE | **castel64, ensamb64** |

```
python3 -I ccastel.py compilador/64/linux/castel64.cas -o castel64.asm \
        -I compilador/64 -I compilador/32 -I biblioteca64/linux -I compilador
nasm -f bin castel64.asm -o castel64 && chmod +x castel64
./castel64 PROGRAMA.CAS -o PROGRAMA.ASM -I BIB [--todo] [--gasto]
```

(Como los demás, busca los módulos con sus nombres del DOS: `importar Argumentos`
busca `ARGUMENT.CAS`; las pruebas usan un árbol con esos nombres.)

### Gen64, el de verdad y el vacío

La parte de atrás de 64 bits del compilador escrito en Castel es el módulo **`Gen64`**,
al lado de las de 16 y 32 (que están en `Expr` y `Sent`). Las expresiones, los
lugares, las condiciones y las llamadas están enteros en él (no se apoyan en `Expr`:
`Expr` los llama a ellos cuando el programa es de 64 bits); las sentencias y la
función entera, en `Sent`; el programa entero, en `Programa`; las rutinas de apoyo y
el arranque, en `Apoyo64` (`compilador/32/apoyo64.cas`, que escribe
`compilador/haz_apoyo.py` con el texto de `ccastel.py`).

Hay **dos `Gen64`**: el de verdad (`compilador/32/gen64.cas`), que importan antes que
nada CASTEL32, CASTELW y CASTEL64, y uno **vacío** (`compilador/gen64.cas`), con lo
mismo público y sin hacer nada, que es el que encuentra el CASTEL de 16 bits. El de
16 bits no saca 64 bits («este CASTEL, el de 16 bits, no saca 64 bits: use CASTEL32
o CASTEL64»): su segmento de datos (64 K, con la pila dentro) no tiene sitio para los
textos de 64 bits; crece solo 165 octetos. **CASTEL32 y CASTELW sí sacan 64 bits**:
el compilador de Hispa-Pentium ya puede compilar para Hispa-64.

### Las cifras

| | |
|---|---|
| CASTEL64 / ENSAMB64 | 172.240 / 29.968 octetos (45.412 y 9.133 líneas de ensamblador) |
| CASTEL64 compilándose a sí mismo, en Linux (el servidor) | 0,2 s (0,21 a 0,27) |
| ... y ENSAMB64 ensamblándolo | 0,15 s |
| CASTEL64 con los 63 programas de las pruebas (de 16, 32 y 64 bits: el núcleo de Hispa-Pentium y sus programas, CASTEL, CASTEL32, ENSAMB32, CASTEL64...) | 26 s en total |
| CASTEL32.BIN (con Gen64) | 166.560 octetos (eran 134.808) |

### El arranque y el punto fijo

```
ccastel.py + NASM  ->  castel64, ensamb64                      (paso 1)
castel64 se compila a sí mismo, ensamb64 lo ensambla  ->  C2    (paso 2)
C2 se compila a sí mismo  ->  C3                                (paso 3)
```

El paso 2 da el mismo ensamblador que el paso 1 (el de ccastel.py) y el mismo binario
que NASM; el paso 3, el mismo que el 2, octeto a octeto. Y CASTEL64 saca lo mismo que
ccastel.py en todos los programas de las pruebas, de 16, 32 y 64 bits, y dice los
mismos errores: es el compilador cruzado de toda la familia.

---

## 10. Hispa-64: el primer ladrillo

`nucleo64/hola.cas` es un núcleo de `modelo núcleo64`:

```
qemu-system-x86_64 -kernel NUCLEO64.BIN -serial stdio -display none \
    -device isa-debug-exit,iobase=0xf4,iosize=0x04
Hola desde Hispa-64
cargado por multiboot
en el modo largo (64 bits)
un puntero ocupa 8 octetos; 2^40 = 1099511627776
int3 atendida 2 veces; vuelve a cierto
```

Mira la magia del cargador (multiboot o multiboot2), comprueba con `EFER.LMA` que
de verdad va en el modo largo, pone una IDT de 64 bits con una función de
interrupción de Castel para la `int3` (y se la dispara dos veces), escribe en la
pantalla de texto con un puntero y acaba con el código 42. Con GRUB (un CD hecho con
`grub-mkrescue`, `multiboot2 /boot/NUCLEO64.BIN`) dice «cargado por multiboot2».
Ocupa 1.432 octetos.

---

## 10 bis. `modelo hispa64`: el núcleo de verdad (23/11/2026)

El núcleo de Hispa-64 (`nucleo64/hispa64.cas`, docs/64/HISPA-64.md) usa su propio
modelo, **`modelo hispa64`**, que es `núcleo64` con otro arranque
(`ARRANQUE64['hispa64']` y `APOYO_HISPA64` en ccastel.py; `Apoyo64` lo copia con
`haz_apoyo.py`, y Gen64 lo elige con `es_hispa64`):

- La cabecera de multiboot2 **pide a GRUB la pantalla gráfica** (la etiqueta 5: la
  que deje el firmware, el GOP con UEFI). Sigue la de multiboot (QEMU `-kernel`).
- **Mira el modo largo con CPUID** (80000001h, EDX bit 29) antes de usarlo; si no
  lo hay, lo dice por el puerto serie y se para.
- **Las tablas de páginas van dentro del núcleo** (`C_tablas`, en lo que empieza a
  cero, alineadas a 4 KB; no en 0x70000, que con UEFI puede ser del firmware): los
  64 primeros GB tal cual y otra vez en la mitad alta (`0xFFFF800000000000`), con
  páginas de 2 MB.
- No hay pantalla de texto: lo que se escribe va al puerto serie (sin esperar si no
  está) y a la función que el núcleo ponga en **`C_consola`** (una función de Castel
  de un parámetro, que recibe la letra; la consola del GOP).
- El texto del arranque sale entero en una línea `db` del compilador escrito en
  Castel: no debe pasar de unos 2.900 octetos (ENSAMB64 no lee líneas de más de
  12.000 letras). Desde la etapa 1 va en dos trozos (`'hispa64'` y `'hispa64_2'`;
  `l64_arranque2()` en Gen64).

### El «modelo de núcleo» de direcciones (etapa 1, 23/11/2026)

Desde la etapa 1 de Hispa-64, `modelo hispa64` enlaza el núcleo **en la mitad alta**,
en los últimos 2 GB del espacio de direcciones (el `-mcmodel=kernel` de gcc):
`org 0xFFFFFFFF80100000`, cargado en 1 MB (física = virtual − `0xFFFFFFFF80000000`).
Toda etiqueta, vista como número de 32 bits **con signo**, es su dirección de verdad;
así que:

- `[G_x]`, `[rsi+G_x]`, `push T_3`, `mov qword [x], T_3`, `call`/`jmp` y `dq G_x` valen
  tal cual (el procesador alarga los 32 bits con su signo, o son relativos, o de 64).
- Lo único que no vale es **una etiqueta en un registro de 32 bits** (`mov esi, T_3`,
  que la alargaría con ceros): el compilador la cambia por **`lea rsi, [T_3]`**
  (`a_lea_alto` en ccastel.py, en `Generador64.em`; `a_lea_alto` en `salida.cas`, en
  `cruda()`; los dos solo con `es_hispa64`, y no en las líneas de un bloque
  `ensamblador`, que van tal cual: ahí, `mov rax, ETIQUETA`, con los 64 bits).
- El arranque, de 32 bits, usa la física: `X-0xFFFFFFFF80000000`; salta a la mitad
  alta con `mov rax, C_alto` / `jmp rax`.
- Detrás del código, `align 4096` y `C_datos:` (los datos, en otra página: el núcleo
  pone el código de solo leer y los datos sin ejecutar). `C_consola` pasa a lo que
  empieza a cero (en la página del código no se puede escribir).
- Los ensambladores: el `org` se cuenta en 64 bits aunque vaya detrás de `bits 32`
  (ensambla.py y Montador); Montador guarda su palabra alta (`pc_alto`) y se la pone
  a todas las etiquetas en `bits 64` (sus etiquetas son de 32 bits).

Lo que sale para los demás modelos (16, 32 y 64 bits) no cambia ni un octeto.

## 10 ter. Números de 64 bits y variables alineadas (etapa 2 de Hispa-64, 23/11/2026)

Lo que pedía la etapa 2 de Hispa-64 (las direcciones de ACPI, de los BAR de PCIe y
de la mitad alta), en los dos compiladores (ccastel.py y el escrito en Castel:
CASTEL64, CASTEL32 y el de 16 bits) con la misma salida:

- **Números escritos de hasta 64 bits** (`0xFFFF800000000000`,
  `18446744073709551615`), solo en los modelos de 64 bits: desde que el léxico
  ha visto `modelo linux64`, `núcleo64` o `hispa64` (en el programa, que se lee
  antes que sus módulos). Antes, o en los demás modelos, siguen diciendo «el
  número no cabe en 32 bits» donde siempre; de más de 64, «el número no cabe en
  64 bits». Un número así **es un `nat64`** (se pone con `mov rax, n`, con su
  signo: los que pasan de 2^63, en negativo) y **no se calcula con él al
  compilar** (como `nat64(x)`): vale en las cuentas, las comparaciones, los
  argumentos, `devolver`, el valor inicial de una global (`dq`) y `{X}` en un
  bloque `ensamblador`. Una `constante X = 0x...` con uno de ellos es una
  constante de 64 bits (en las tablas de CASTEL, `S_NUM` con la marca `SM_64` y
  los 32 bits de arriba en `c` y `d`; en el árbol, un `K_NUM` con su parte alta en
  otro `K_NUM`, en `c`); `X + 1` en otra constante, no (no se calcula).
  En CASTEL, la pieza del léxico guarda los 32 bits de arriba (`Pieza.alto`; en
  CASTEL32 y CASTEL64 la ventana de piezas pasa de 16 a 32 octetos por pieza:
  `PÁRRAFOS_PIEZA` de Medidas; el de 16 bits, que no saca 64 bits, sigue con 16:
  en sus 640 K no caben 16 K más).
- **`alineado N`** detrás del tipo de una variable del módulo, sin valor inicial
  (`variable tabla: arreglo [512] de nat64 alineado 4096`), N una potencia de 2
  de 1 a 4096, en los modelos de 64 bits: sale `alignb N` delante de su etiqueta
  en lo que empieza a cero, que ensambla.py y ENSAMB64 (Montador: la marca
  `R_ALINEA_B` en su resumen) saben ya como NASM. Los errores: ««alineado» es de
  los modelos de 64 bits», «solo se alinean las variables del módulo», ««alineado»
  lleva una potencia de 2, de 1 a 4096» y «una variable alineada no lleva valor
  inicial».

Las pruebas: `ejemplos64/numeros64.cas` (con su salida esperada) y seis errores
nuevos en `ejemplos64/errores/`; lo de los demás modelos no cambia ni un octeto.

**Llamar a C** (System V AMD64) no se ha hecho aún: la etapa 2 de Hispa-64 lee las
tablas de ACPI ella misma, sin ACPICA (docs/64/HISPA-64.md, sección 8). Hará falta
para ACPICA (el AML: la batería, las tapas, dormir) y para musl.

## 11. Probar y comprobar

`pruebas64.sh` es la parte 23 de `pruebas.sh` (`SOLO=castel64 ./pruebas.sh`, unos 4
minutos; sin emular nada salvo Hispa-64):

- los ejemplos de 16 bits sin DOS, como programas de `linux64`, escriben lo mismo;
  `ejemplos64/` dan su salida esperada y su código; cuentas al azar de 64 bits;
- CASTEL64 frente a ccastel.py en todos los programas de las pruebas (y los errores,
  con `ejemplos64/errores`), y CASTEL32 (con corre32) en los de 64 bits; `ensambla.py`
  y ENSAMB64 frente a NASM (el de aquí, 2.16, y el del PC de Albinyana, 3.01: dan lo
  mismo con lo que saca el compilador; en `formas64.asm`, NASM 3 cambia `movsxd rax,
  eax` por `cdqe` y acorta los `near` sin `strict`, así que ahí no están);
- el arranque en tres pasos;
- Hispa-64 en QEMU por multiboot y, si está `grub-mkrescue`, por multiboot2 con GRUB.

Las partes de 16 bits (`SOLO=16`) y de CASTEL32 (`SOLO=castel32`) siguen dando lo
mismo: lo que sale para Hispa-8086 e Hispa-Pentium no ha cambiado.

---

## 12. Lo que queda para más adelante

- **Referencias de 32 bits en las tablas del compilador.** Como en CASTEL32, un nudo,
  un nombre o un texto es un párrafo de 16 octetos **dentro de su tabla**, con una
  referencia de 16 bits: cada tabla llega a 64 K párrafos (casi 1 MB). CASTEL64 tiene
  las mismas medidas que CASTEL32 (el núcleo de Hispa-Pentium, el mayor programa, usa
  el 16 % del árbol). Quitar ese tope es un cambio de las estructuras de todo el
  compilador: un tipo `Ref` en Medidas (`nat16` en 16 y 32 bits, `nat32` en 64) en
  vez de `nat16` en cada referencia (unas 400 de las 850 del compilador), y nudos,
  tipos y fichas de 32 octetos con campos de 32 bits en 64 (con `Ref = nat16`, el
  CASTEL de 16 y el de 32 saldrían iguales). Se hará cuando un programa lo pida.
- **Constantes de 64 bits que se calculen al compilar** (`X + 1` con una de 64): los
  números escritos de 64 bits ya están (§10 ter), pero no se calcula con ellos.
- **Variables alineadas con valor inicial**, o locales (§10 ter: solo las del módulo
  sin él).
- **Llamar a C** (System V AMD64): `externa función`, con su puente (§4).
- ~~Los programas del anillo 3 de Hispa-64 (un `modelo usuario64`)~~: **no hace
  falta** (etapa 4 de Hispa-64, 23/11/2026, docs/64/HISPA-64.md §10): Hispa-64 tiene
  las llamadas al sistema de Linux x86-64, así que un programa de `modelo linux64`
  corre tal cual en su anillo 3 (`nucleo64/programas/holacas.cas`). Lo que queda
  del núcleo de verdad (discos, la entrada, musl...), en las etapas siguientes.
- Ir relativo a RIP (programas que se carguen en cualquier sitio, PIE).
