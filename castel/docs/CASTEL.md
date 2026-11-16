# Castel — definición del lenguaje (Fase 0)

*Castel: un corte de «Castellano». Un lenguaje de sistemas en español, de la familia Pascal/Oberon,
para escribir sistemas operativos. Este documento es el plano que implementará el primer
compilador. 15/11/2026 — borrador para discutir y afinar.*

Regla de oro: **esto es el subconjunto MÍNIMO para arrancar.** Todo lo marcado con
*(más adelante)* NO va en el primer compilador; se añade cuando el lenguaje ya se sostiene.
Cuanto más pequeño el núcleo, antes se autohospeda.

---

## 1. Forma de un programa

Un fichero fuente es un **módulo**. Un módulo tiene, en este orden: declaraciones
(constantes, tipos, variables y funciones) y, opcionalmente, un cuerpo de arranque.

```
módulo Saludo

    constante LIMITE = 10

    función principal()
        variable i: ent16
        para i desde 1 hasta LIMITE hacer
            escribir(i)
        fin
    fin

fin Saludo.
```

- `módulo <Nombre>` abre; `fin <Nombre>.` cierra (el nombre tiene que coincidir; el punto
  final marca fin de fichero, como en Pascal/Oberon).
- La función `principal()` es por donde empieza a ejecutarse un programa suelto (una orden
  del sistema). Un módulo que solo aporta funciones a otros puede no tenerla.

---

## 2. Cómo se escribe (léxico)

- **Mayúsculas y minúsculas importan**: `Dato` y `dato` son distintos. Las palabras clave
  van en minúscula.
- **Comentarios**:
  - De línea: `;` hasta el fin de línea (igual que en ensamblador — así es familiar).
  - De bloque: `(* ... *)`, y se pueden anidar.
- **Identificadores**: letra (incluidas vocales acentuadas y `ñ`) o `_`, seguida de letras,
  dígitos o `_`. Ejemplos válidos: `tamaño`, `n_filas`, `PantallaTexto`.
- **Números**:
  - Decimal: `0`, `42`, `1990`.
  - Hexadecimal: `0xB800`, `0x20` (para direcciones y puertos; cómodo para hardware).
  - Carácter: `'A'` es el número de su código (65).
- **Textos**: `"entre comillas dobles"`. Dentro, `\n` fin de línea, `\t` tabulador,
  `\"` comilla, `\\` barra, `\0` cero. Un texto es una secuencia de bytes terminada en 0.
- **Separación**: una **sentencia por línea** (el fin de línea la termina). No hace falta
  `;` al final. Los bloques se cierran con `fin`.

---

## 3. Tipos

### Del primer compilador (los básicos)
| Tipo    | Qué es                                  |
|---------|------------------------------------------|
| `nat8`  | natural (sin signo) de 8 bits (0..255)   |
| `nat16` | natural de 16 bits (0..65535)            |
| `ent16` | entero con signo de 16 bits              |
| `byte`  | = `nat8`                                 |
| `bool`  | `cierto` o `falso`                       |

En modo real (Hispa-DOS) la palabra de la máquina es de 16 bits: por eso `nat16`/`ent16`
son los de a diario. `nat8`/`byte` para memoria y hardware.

### Más adelante
- `nat32` / `ent32` (32 bits, para el SO de los 90).
- `puntero a <tipo>` y `dirección` (direcciones de memoria).
- `arreglo [N] de <tipo>` (vectores).
- `estructura campo: tipo ... fin` (registros).
- `tipo <Nombre> = <definición>` (nombrar tipos propios).

---

## 4. Declaraciones

```
constante <NOMBRE> = <número o texto>          ; un valor fijo con nombre
variable  <nombre>: <tipo>                      ; una variable
variable  a, b, c: ent16                        ; varias del mismo tipo

función <nombre>(<parámetros>): <tipo>          ; función que devuelve un valor
    ...
    devolver <expresión>
fin

función <nombre>(<parámetros>)                  ; procedimiento (no devuelve nada)
    ...
fin
```

- Parámetros: `nombre: tipo`, separados por comas. `función max(a: ent16, b: ent16): ent16`.
- *(Más adelante)* parámetros por referencia (`ref`), para que la función pueda cambiar lo
  que le pasan.
- `público` antes de una declaración la hace visible a otros módulos *(más adelante, con el
  sistema de módulos)*.

---

## 5. Sentencias

### Asignación
```
i := 0
pantalla := 0xB800
```
El símbolo es `:=` (dos puntos e igual). `=` es solo comparar, nunca asignar (esto evita el
error clásico de C de confundir `=` con `==`).

### Decisión
```
si <condición> entonces
    ...
sino si <condición> entonces
    ...
sino
    ...
fin
```

### Bucles
```
mientras <condición> hacer
    ...
fin

para <variable> desde <inicio> hasta <fin> hacer      ; de uno en uno
    ...
fin

para <variable> desde <inicio> hasta <fin> paso <n> hacer   ; (más adelante) de n en n
    ...
fin
```
`romper` sale del bucle; `seguir` salta a la siguiente vuelta.

### Elegir entre casos *(más adelante)*
```
según <expresión>
    caso 1: ...
    caso 2, 3: ...
    si no: ...
fin
```

### Devolver y llamar
```
devolver <expresión>        ; en una función con tipo
devolver                    ; en un procedimiento (termina antes)
escribir(i)                 ; una llamada como sentencia
```

### Bajar al metal: ensamblador en línea
```
ensamblador
    cli
    mov al, 0x20
    out 0x20, al
    sti
fin
```
Dentro de `ensamblador ... fin` va ensamblador tal cual (lo que ya sabemos de NASM). Es la
válvula de escape: **lo que el lenguaje todavía no sepa expresar, se hace a mano aquí.** Así
Hispa-DOS se puede migrar trozo a trozo. *(Más adelante: nombrar variables Castel dentro
del bloque para pasarle datos.)*

---

## 6. Expresiones

- **Aritméticas**: `+  -  *  div  mod` (`div` = división entera, `mod` = resto).
- **Comparaciones**: `=  <>  <  <=  >  >=` (dan `bool`).
- **Lógicas**: `y`  `o`  `no` (sobre `bool`).
- **De bits** *(más adelante)*: `yb` `ob` `xob` (and/or/xor de bits), `<<` `>>`.
- **Unarias**: `-x` (negativo), `no c` (negar un bool).
- **Agrupar**: con paréntesis `( ... )`.
- **Llamada**: `nombre(arg1, arg2)` también es una expresión (si la función devuelve valor).

Precedencia (de más fuerte a más floja): unario → `* div mod` → `+ -` → comparaciones →
`no` → `y` → `o`.

### Memoria y hardware (funciones propias del lenguaje)
Para el primer compilador, el acceso de bajo nivel va por funciones «mágicas» que el
compilador conoce:
```
meter_puerto(puerto)        ; lee un byte de un puerto (IN) -> nat8
sacar_puerto(puerto, dato)  ; escribe un byte en un puerto (OUT)
leer_byte(seg, desp)        ; lee memoria en seg:desp -> nat8   (más adelante: punteros)
poner_byte(seg, desp, dato) ; escribe memoria en seg:desp
escribir(x)                 ; muestra un número o un texto por pantalla (DOS)
```
Cuando lleguen los **punteros**, `leer_byte`/`poner_byte` darán paso a `valor(p)` y
`p := dirección(seg, desp)`, que es más limpio. De momento, con estas funciones basta para
escribir cosas reales.

---

## 7. Un ejemplo completo y realista

Una orden que imprime los números del 1 al N que le pasan, y dice si cada uno es par:

```
módulo Pares

    función es_par(n: ent16): bool
        devolver (n mod 2) = 0
    fin

    función principal()
        variable i: ent16
        para i desde 1 hasta 20 hacer
            escribir(i)
            si es_par(i) entonces
                escribir(" par")
            sino
                escribir(" impar")
            fin
        fin
    fin

fin Pares.
```

Esto es exactamente lo que el **primer compilador** tiene que saber compilar: módulo,
funciones con y sin valor de retorno, `variable`, `para`, `si/sino`, aritmética,
comparación, llamadas y `escribir`. Con eso ya podemos reescribir una orden pequeña de
Hispa-DOS como prueba de fuego.

---

## 8. El subconjunto del primer compilador (resumen)

**Entra:** módulo con `principal`; `constante` y `variable`; tipos `nat8/nat16/ent16/byte/
bool`; funciones con y sin retorno; `:=`; `si/sino/fin`; `mientras/fin`; `para/desde/hasta/
hacer/fin`; `romper`/`seguir`; `devolver`; `ensamblador/fin`; aritmética (`+ - * div mod`),
comparaciones, lógicas (`y o no`); llamadas; las funciones de bajo nivel (`meter_puerto`,
`sacar_puerto`, `leer_byte`, `poner_byte`, `escribir`); comentarios `;` y `(* *)`.

**No entra todavía (se añade por fases):** `nat32/ent32`, punteros y `dirección`/`valor`,
arreglos, estructuras, `tipo` propios, `según/caso`, `paso` en el `para`, operadores de bits,
parámetros por referencia, módulos públicos, textos como tipo manejable.

---

## 9. Lo que falta decidir para construir el compilador

(Las dejé en CIMIENTOS.md; mi recomendación por defecto, para no pararnos:)
1. **Cómo genera el código la etapa 0**: *sacar ensamblador y rematar con NASM* (lo que
   dominamos) — recomendado — frente a escribir el binario nosotros.
2. **En qué lenguaje escribimos ese primer compilador**: *C* (rápido, binarios pequeños), o
   un prototipo en Python primero para ir aún más deprisa.
3. **Qué orden de Hispa-DOS reescribimos** como primer hito real: la más pequeña y autónoma.

Si te parece, tiro con los valores recomendados (NASM + C) y, en cuanto digas, monto el
primer compilador que compile el ejemplo de la sección 7.
```
```
