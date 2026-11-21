# El ensamblador propio de Castel

*17/11/2026. Segunda parte de la Fase 4 (CIMIENTOS.md): para compilar dentro de Hispa-8086
hace falta un ensamblador que no sea NASM.*

El compilador de Castel saca texto en ensamblador con la sintaxis de NASM. Este ensamblador
entiende **la parte de NASM que el compilador usa** (y lo corriente de un bloque
`ensamblador` escrito a mano) y da **el mismo archivo que NASM, octeto a octeto**. Hay dos,
que tienen que dar siempre lo mismo:

| | Qué es |
|---|---|
| `ensambla.py` | La referencia, en Python: `python3 -I ensambla.py ENTRADA.asm [-o SALIDA]` |
| `ensamblador/ensambla.cas` | **ENSAMBLA**, el de verdad: el anterior pasado a Castel, función por función (por dentro, el módulo `Montador`, `montador.cas`). Corre en Hispa-8086: `ENSAMBLA ENTRADA.ASM [SALIDA]` (sin SALIDA, `ENTRADA.COM`, o `.EXE` si el programa es de «modelo grande») |
| `ensamblador/32/pentium/ensamb32.cas`, `ensamblador/32/hw32/ensambw.cas` | **ENSAMB32** y **ENSAMBW**: el mismo Montador compilado para 32 bits, para Hispa-Pentium y para HW32 (CASTEL32.md §11). Ensamblan el núcleo de Hispa-Pentium en unos 3 s dentro de él |
| `ensamblador/64/linux/ensamb64.cas` | **ENSAMB64**: el mismo Montador compilado para Linux x86-64 (CASTEL64.md §9). Ensambla CASTEL64 entero en 0,15 s |

`pruebas.sh` ensambla con NASM, con `ensambla.py` y con ENSAMBLA (dentro de Hispa-8086) los
ejemplos, la biblioteca entera, `ensamblador/formas.asm` (una muestra de cada forma de
instrucción) y el propio ENSAMBLA, y compara los archivos.

## Lo que entiende

- **Instrucciones del 8086**: `mov`; las cuentas `add or adc sbb and sub xor cmp` y `test`;
  `inc dec neg not mul imul div idiv`; los desplazamientos `shl sal shr sar rol ror rcl rcr`
  (de 1 o de `cl`); `push pop` (registros, segmentos, memoria), `pushf popf`; `xchg`, `lea`;
  `jmp`, `call` (cercanos, por registro o memoria, y lejanos `seg:desp`), los saltos
  condicionales con todos sus nombres, `loop loope loopne jcxz`; `ret`/`retn`, `retf` (con o
  sin número), `iret`, `int`; `in`, `out`; las de cadenas (`lodsb stosw movsb...`) con
  `rep repe repne`; y las sueltas (`cld std cli sti clc stc cmc cwd cbw nop hlt lahf sahf
  xlatb into`).
- **Operandos**: registros de 8 y 16 bits y de segmento; memoria `[base+índice+n]` con las
  combinaciones del 8086, `[etiqueta+n]`, con `byte`/`word` delante cuando hace falta y
  prefijo de segmento (`[es:bx]`); números en decimal, `0x1F`, `1Fh` y `'A'`.
- **Cuentas** con números y etiquetas: `+ - * / % << >> & | ^ ~` y paréntesis, en 32 bits.
- **Etiquetas**: `nombre:` (los dos puntos son obligatorios) y locales `.nombre:`, que son
  de la última etiqueta no local.
- **x86-64** (`bits 64`, desde el 21/11/2026; CASTEL64.md §8): los registros de 64 bits
  y `r8`-`r15`, `qword`, el prefijo REX, `movsxd cdqe cqo iretq syscall rdmsr wrmsr
  cpuid stosq movsq`..., `dq resq`, y las cuentas en 64 bits; `ensamblador/formas64.asm`
  tiene una muestra de cada forma.
- **Órdenes**: `db dw dd` (números, cuentas y textos entre comillas), `resb resw resd`,
  `times N ...`, `org`, `absolute` (lo que sigue no va al archivo), `cpu`, `bits`, y lo que
  usa el modelo grande: `section NOMBRE vstart=N align=N` y `align N, db 0`.

## Las mismas decisiones que NASM

- **La forma más corta de cada instrucción**: desplazamientos de 8 bits cuando caben (y
  ninguno si es 0, salvo con `bp`); `83h` con un octeto para sumar o comparar con un número
  pequeño; las formas propias de `ax`/`al` (`A0h`–`A3h`, `05h`, `3Dh`, `A8h`...); `inc`/`dec`/
  `push`/`pop`/`xchg ax` de un octeto.
- **Los saltos**: se pasa por el archivo las veces que haga falta. En cada pasada un salto
  va corto si llega (127 octetos) con las etiquetas donde están ahora: las de atrás, donde han
  quedado en esta pasada; las de delante, donde quedaron en la anterior (en la primera aún no
  se saben y se supone que llega). Cuando ninguna etiqueta cambia de sitio, se ha acabado. Un
  `jmp` que no llega son 3 octetos; un salto condicional que no llega, en el 8086, es el
  contrario por encima de un `jmp` (5 octetos).

## Lo que no hace (y cómo lo dice)

No es NASM entero: no hay macros, ni `equ`, ni `%define`, ni instrucciones del 286 en
adelante, ni más formatos que el archivo tal cual. Una cuenta con etiquetas ocupa siempre 16
bits (NASM, si la cuenta es una diferencia pequeña, a veces la encoge: el compilador no
genera ese caso). Lo que no entiende lo dice con el archivo y la línea:
`PROG.ASM: error: línea 120: instrucción que no entiendo: enter`.

## ENSAMBLA por dentro

Es un `.COM` de unos 17 K. El archivo no se guarda en memoria: se lee entero en la primera
pasada y en la última. Las pasadas de en medio (el compilador entero necesita 35 para que
todos sus saltos se asienten) solo mueven etiquetas y alargan saltos, así que se hacen sobre
un **resumen** que deja la primera en memoria lejana: tramos de tamaño fijo, etiquetas y
saltos. Sale lo mismo que leyendo el archivo cada vez, y es decenas de veces más rápido. (Si
algún salto no va a una etiqueta sola, o el resumen no cabe, las hace todas sobre el archivo.)

Las etiquetas van a **memoria lejana** (módulo `Lejos` de la biblioteca): las `L_n` y `T_n`,
que son casi todas, en dos tablas por su número; las demás, con su nombre, en una tabla de
dispersión. Así ensambla programas mucho mayores que él: el compilador entero, de 37.000
líneas y 95 K.
