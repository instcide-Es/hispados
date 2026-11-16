# Castel — compilador mínimo (Fase 1, etapa 0)

**Castel** es el lenguaje de sistemas en español (familia Pascal/Oberon) para escribir
los sistemas operativos de Rescoldo. Esta carpeta es el **primer compilador** (etapa 0):
escrito en Python en el PC, lee el subconjunto mínimo de Castel y **saca ensamblador para
NASM**, que se ensambla a un `.COM` de DOS de 16 bits. Es el espíritu de CIMIENTOS.md:
algo vivo pronto, afinar sobre vivo.

Definición del lenguaje: [docs/CASTEL.md](docs/CASTEL.md). Estrategia y fases: [docs/CIMIENTOS.md](docs/CIMIENTOS.md).

## Qué hay aquí

- `ccastel.py` — el compilador (léxico UTF-8 con palabras clave acentuadas →
  análisis sintáctico descendente → generación de NASM de 16 bits para `.COM`).
- `compilar.sh ENTRADA.cas` — compila a `.asm` y ensambla a `NOMBRE.COM`.
- `probar.sh PROGRAMA.COM` — lo ejecuta dentro de Hispa-DOS (QEMU) y enseña su salida.
- `ejemplos/` — `pares.cas` (el diana de CASTEL.md §7), `saludo.cas`, `pruebas.cas`.

## Probado y funcionando (16/11/2026)

Los tres ejemplos compilan, ensamblan y **corren en Hispa-DOS** con la salida correcta:

```
$ ./compilar.sh ejemplos/pares.cas
$ ./probar.sh ejemplos/PARES.COM
1 impar
2 par
...
20 par
```

`pruebas.cas` → `suma 1..5 = 15` y `1 3 7 9`.

## Qué entiende este primer compilador (CASTEL.md §8)

Módulo con `principal`; `constante` y `variable` (numéricas y de texto); tipos
`nat8/nat16/ent16/byte/bool`; funciones con y sin retorno (parámetros y locales en marco
de pila `bp`); `:=`; `si`/`sino`/`fin` (y `sino si` encadenado); `mientras`;
`para`/`desde`/`hasta`; `romper`/`seguir`; `devolver`; `ensamblador`/`fin` (pasa el
ensamblador tal cual); aritmética `+ - * div mod`; comparaciones `= <> < <= > >=`;
lógicas `y o no`; llamadas; funciones de bajo nivel `escribir`, `meter_puerto`,
`sacar_puerto`, `leer_byte`, `poner_byte`; comentarios `;` y `(* *)`.

Detalles de generación: `.COM` con `org 0x100`, código `cpu 8086` (compatible amplio),
valores de 16 bits en AX, llamada estilo cdecl (el que llama limpia la pila), `escribir`
distingue texto (cadena 0-terminada, int 21h/02h) de número (decimal con signo).

## Lo que NO entra todavía (por fases, CASTEL.md §8)

`nat32/ent32`, punteros y `dirección`/`valor`, arreglos, estructuras, `tipo` propios,
`según/caso`, `paso` en el `para`, operadores de bits, parámetros por referencia, módulos
públicos, textos como tipo manejable. Y las comprobaciones semánticas son mínimas (detecta
nombres no declarados; no hay chequeo fuerte de tipos todavía).

## Siguientes hitos (CIMIENTOS.md)

- **Fase 2:** reescribir **una orden pequeña de Hispa-DOS** en Castel y que funcione igual
  (la prueba de fuego de que sirve para el sistema real).
- **Fase 4:** autohospedaje (reescribir el compilador en Castel).

Decisiones por confirmar con Carlos: seguir en Python o pasar el compilador a C; qué orden
de Hispa-DOS es la primera en migrarse.
