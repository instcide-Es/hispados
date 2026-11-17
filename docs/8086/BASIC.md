# Hispa-BASIC

El BASIC de Hispa-8086: un intérprete escrito desde cero en ensamblador del
8086, compatible con GW-BASIC 3.23 y en español. Es `BASIC.COM`.

Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.

## Qué tiene que hacer

- Ejecutar los programas de GW-BASIC tal cual: los listados de las revistas
  (texto) y, desde la 0.12, los guardados en binario (`SAVE "X"` sin `,A`).
- Decir los mensajes y los errores en español.
- Aceptar las órdenes también en español. Cada palabra española se guarda
  con el **mismo código interno** que la inglesa: el programa, por dentro, es
  de GW-BASIC. `LIST` lo enseña en inglés y `LISTAR` en español.
- Funcionar en un IBM PC con 64 K y un disquete.

## Entregas

| Versión | Qué | Estado |
|---|---|---|
| 0.11 | Lo esencial: edición de líneas, LIST/RUN/NEW/SAVE/LOAD en texto, PRINT, LET, INPUT, IF…THEN…ELSE, FOR…NEXT, WHILE…WEND, GOTO, GOSUB, ON…GOTO/GOSUB, DIM, DATA/READ/RESTORE, END/STOP/CONT, enteros (`%`), simple precisión (`!`) y cadenas (`$`), funciones de números y de cadenas, DEF FN, errores en español, palabras en español | hecha (Hispa-8086 0.11) |
| 0.12 | Formato binario de GW-BASIC, doble precisión (`#`), PRINT USING, archivos (OPEN, PRINT#, INPUT#, LINE INPUT#, CLOSE, EOF), ON ERROR / RESUME, RENUM, AUTO, DELETE, EDIT | hecha: también WRITE, LOF, LOC, NAME, FILES; EDIT deja la línea en la pantalla para corregirla con las flechas (la edición de líneas es la del sistema: ver «La línea que se teclea» en HISPA-8086.md); OPEN … FOR RANDOM, FIELD, GET y PUT, aún no |
| 0.13 | Pantalla y sonido: CLS, LOCATE, COLOR, WIDTH, SCREEN 1 y 2, PSET, LINE, CIRCLE, DRAW, PAINT, SOUND, PLAY, BEEP, INKEY$ | hecha: también PRESET y POINT; CIRCLE sin arcos ni aspecto propio; DRAW sin A, TA ni «=variable;» |

## Memoria

`BASIC.COM` es el código (un segmento). Al arrancar pide al sistema un
**segmento de datos de 64 K** (INT 21h/48h; si no lo hay, el que quede) y
ahí va todo lo del usuario, como en GW-BASIC:

```
 0      variables del intérprete que se guardan con el programa (nada aún)
 ...    el programa (líneas tokenizadas, enlazadas)
 ...    las variables simples, luego las matrices
 ...    (libre)
 ...    las cadenas, que crecen desde arriba hacia abajo
 64 K   la cima
```

Cuando las cadenas y las variables se tocan, se recogen las cadenas
muertas (compactación). Si aun así no cabe: «Memoria agotada».

## Una línea del programa

Igual que en GW-BASIC, para que la 0.12 pueda leer y escribir sus archivos
sin traducir nada:

```
 2 octetos   enlace a la línea siguiente (0 = fin del programa)
 2 octetos   número de línea
 n octetos   el texto tokenizado
 1 octeto    0
```

En memoria el enlace es la dirección dentro del segmento de datos.

### Los códigos (los de GW-BASIC)

- Sentencias y operadores: un octeto de 81h a F4h (81h END, 82h FOR,
  83h NEXT, 84h DATA, 85h INPUT, 86h DIM, 87h READ, 88h LET, 89h GOTO,
  8Ah RUN, 8Bh IF, 8Ch RESTORE, 8Dh GOSUB, 8Eh RETURN, 8Fh REM, 90h STOP,
  91h PRINT, 92h CLEAR, 93h LIST, 94h NEW, 95h ON, … A1h ELSE, … B1h WHILE,
  B2h WEND, … CCh TO, CDh THEN, CEh TAB(, CFh STEP, D1h FN, D2h SPC(,
  D3h NOT, … D9h ' , … E6h `>`, E7h `=`, E8h `<`, E9h `+`, EAh `-`,
  EBh `*`, ECh `/`, EDh `^`, EEh AND, EFh OR, F0h XOR, F1h EQV, F2h IMP,
  F3h MOD, F4h `\`).
- Funciones: dos octetos, FFh y el código (FF 81h LEFT$, 82h RIGHT$,
  83h MID$, 84h SGN, 85h INT, 86h ABS, 87h SQR, 88h RND, 89h SIN, 8Ah LOG,
  8Bh EXP, 8Ch COS, 8Dh TAN, 8Eh ATN, 8Fh FRE, … 92h LEN, 93h STR$,
  94h VAL, 95h ASC, 96h CHR$, 97h PEEK, 98h SPACE$, … 9Ah HEX$, 9Ch CINT,
  9Dh CSNG, 9Eh CDBL, 9Fh FIX). FEh y FDh, lo mismo para las demás.
- Constantes numéricas: 11h-1Bh = de 0 a 10; 0Fh n = de 11 a 255;
  1Ch nn = entero; 1Dh nnnn = simple; 1Fh nnnnnnnn = doble; 0Bh octal,
  0Ch hexadecimal; 0Eh nn = número de línea (detrás de GOTO, GOSUB, THEN,
  ELSE, RESTORE, RUN, LIST…).
- `'` se guarda como `:` REM `'` (3Ah 8Fh D9h) y ELSE como `:` ELSE.
- Lo que va entre comillas, detrás de REM y detrás de DATA se guarda tal
  cual.

La tabla exacta está en `h8086/basic/tokens.inc`; hay que comprobarla contra
archivos guardados por el GW-BASIC de verdad antes de la 0.12.

## Los números

Los de GW-BASIC, con el formato binario de Microsoft (MBF):

- Entero `%`: 16 bits con signo.
- Simple `!`: 4 octetos: 3 de mantisa (el bit alto del tercero es el
  signo; el 1 de delante no se guarda), 1 de exponente con exceso de 128
  (0 = el número 0). Valor = 0,1mmm… × 2^(e−128).
- Doble `#` (0.12): igual con 7 octetos de mantisa.

Por dentro se calcula con un acumulador más ancho (mantisa de 32 bits para
simple, de 64 para doble) y se redondea al guardar, como GW-BASIC. No usa el
coprocesador: un 8088 no lo suele tener.

PRINT escribe la simple con 7 cifras como mucho y la doble con 16, pasa a
la notación con E cuando hace falta (`1E+20`) y deja un espacio delante del
número positivo y otro detrás de todos, como GW-BASIC.

## Las palabras en español

Una lista cerrada; cada una es otra forma de escribir la inglesa:

| GW-BASIC | Español | GW-BASIC | Español |
|---|---|---|---|
| PRINT | IMPRIMIR | INPUT | PREGUNTAR |
| LET | SEA | IF / THEN / ELSE | SI / ENTONCES / SINO |
| FOR / TO / STEP / NEXT | PARA / HASTA / PASO / SIGUIENTE | GOTO | IR A (o IRA) |
| GOSUB / RETURN | LLAMAR / VOLVER | WHILE / WEND | MIENTRAS / FINMIENTRAS |
| DIM | DIMENSION | DATA / READ / RESTORE | DATOS / LEER / RELEER |
| END / STOP / CONT | FIN / PARAR / SEGUIR | REM | NOTA |
| RUN / LIST / NEW | EJECUTAR / LISTAR / NUEVO | LOAD / SAVE | CARGAR / GUARDAR |
| CLS | LIMPIAR (como en COMMAND) | ON | EN |
| AND / OR / NOT | Y / O / NO | DEF FN | DEF FN (igual) |
| SYSTEM | SALIR | RANDOMIZE | ALEATORIO |
| CLEAR | VACIAR | SWAP | CAMBIAR |
| OPEN / CLOSE | ABRIR / CERRAR | WRITE | ESCRIBIR |
| RESUME | REANUDAR | PRINT USING | IMPRIMIR USANDO |
| DELETE / RENUM / EDIT | QUITAR / RENUMERAR / EDITAR | KILL / NAME / FILES | BORRAR / RENOMBRAR / ARCHIVOS (como en COMMAND) |

`LIST` lista en inglés y `LISTAR` en español (cada vez, según con cuál se
pida).

Las funciones van con el nombre de siempre (`LEFT$`, `LEN`, `INT`…): se
leen igual en cualquier listado y no chocan con nombres de variables. Las
palabras españolas solo se reconocen enteras (seguidas de algo que no sea
letra ni cifra), para que una variable como `SIMA` o `PARAR1` siga siendo
una variable. `Y` y `O` son operadores solo entre dos expresiones
(`SI A=1 Y B=2`); una variable que se llame `Y` o `O` se escribe igual en
GW-BASIC y aquí (`Y=3`), porque detrás de una sentencia o de un operador
no puede ir un operador.

Lo mismo con las sentencias: una palabra española seguida de `=` es una
variable (`FIN=1`, `SI=2`). Las inglesas se leen como en GW-BASIC, que las
reconoce incluso pegadas a otras letras (`FORI=1TO9` es `FOR I=1 TO 9`):
así se cargan bien los listados de la época, escritos sin espacios.

## Los errores

Con el número de GW-BASIC (para ERR) y el texto en español:

| Nº | GW-BASIC | Hispa-BASIC |
|---|---|---|
| 1 | NEXT without FOR | SIGUIENTE sin PARA |
| 2 | Syntax error | Error de sintaxis |
| 3 | RETURN without GOSUB | VOLVER sin LLAMAR |
| 4 | Out of DATA | No quedan DATOS |
| 5 | Illegal function call | Llamada no válida |
| 6 | Overflow | Desbordamiento |
| 7 | Out of memory | Memoria agotada |
| 8 | Undefined line number | No existe esa línea |
| 9 | Subscript out of range | Índice fuera de la matriz |
| 10 | Duplicate Definition | Matriz ya dimensionada |
| 11 | Division by zero | División por cero |
| 12 | Illegal direct | No vale en modo directo |
| 13 | Type mismatch | Tipos que no casan |
| 14 | Out of string space | Sin sitio para cadenas |
| 15 | String too long | Cadena demasiado larga |
| 16 | String formula too complex | Expresión de cadenas muy compleja |
| 17 | Can't continue | No se puede seguir |
| 18 | Undefined user function | Función FN sin definir |
| 19 | No RESUME | Falta RESUME |
| 20 | RESUME without error | RESUME sin error |
| 22 | Missing operand | Falta un operando |
| 23 | Line buffer overflow | Línea demasiado larga |
| 26 | FOR without NEXT | PARA sin SIGUIENTE |
| 29 | WHILE without WEND | MIENTRAS sin FINMIENTRAS |
| 30 | WEND without WHILE | FINMIENTRAS sin MIENTRAS |
| 53 | File not found | No existe el archivo |
| 61 | Disk full | Disco lleno |
| 64 | Bad file name | Nombre de archivo no válido |

En un programa: «Error de sintaxis en 20».

## Diferencias conocidas con GW-BASIC

Se comprueban con PC-BASIC (que imita a GW-BASIC 3.23) en
`h8086/prueba/basic_compara.sh`. Quedan estas, todas en la última cifra de lo
que se escribe, no en los cálculos:

- Las potencias de exponente entero negativo: GW-BASIC escribe `10^-3` como
  `9.999999E-04`; Hispa-BASIC, `.001`.
- `SQR(2)` y `2^.5`: GW-BASIC, `1.414213`; Hispa-BASIC, `1.414214` (el valor
  bien redondeado es 1,41421356…).
- `RANDOMIZE n` con un número dado no da la misma serie que en GW-BASIC (sí la
  da `RND` sin `RANDOMIZE`, `RND(0)` y `RND` con un número negativo).
