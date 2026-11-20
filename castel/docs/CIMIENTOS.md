# Cimientos de un lenguaje de programación en español para hacer sistemas operativos

*Documento para pensar y discutir — no es código ni compromiso. 15/11/2026.*

Carlos quiere que los próximos sistemas operativos (los de mediados y finales de
los 90, más avanzados que Hispa-DOS) **no se escriban en ensamblador**, sino en un
**lenguaje propio, en español, hecho exclusivamente para escribir esos sistemas**.
Esto es la fase de pensar los cimientos: para qué tiene que servir, qué forma darle
y cómo se arranca desde cero. No tocamos nada del sistema actual.

---

## 1. Para qué tiene que servir (y para qué NO)

**Sí:**
- Escribir el núcleo, los controladores y las utilidades de un SO (lo que hoy hacemos
  en ensamblador NASM en Hispa-DOS).
- Acceso directo al hardware: memoria por dirección, puertos, interrupciones,
  registros. Tiene que poder hacer todo lo que hace el ensamblador.
- Correr **sin runtime pesado**: nada de recolector de basura, ni máquina virtual, ni
  biblioteca enorme por debajo. Lo que no escribas tú, no está.
- Generar código nativo para PC: primero 16 bits (modo real, como Hispa-DOS) y luego
  32 bits (modo protegido, para el SO de los 90).

**No:**
- No es un lenguaje para enseñar a programar ni para hacer programas de usuario cómodos
  (eso ya lo cubren otros). Es una **herramienta de taller** para hacer SO.
- No buscamos competir con C/Rust ni ser «el español que reemplaza al inglés». Buscamos
  **una herramienta nuestra, pequeña y entendible, en nuestro idioma**, que podamos
  dominar entera.

### Por qué tiene sentido hacerlo nosotros
Miré lo que hay. Los lenguajes de programación en español que existen —Latino, Pauscal,
LPP y los de trabajos universitarios— son **todos educativos o interpretados**: ninguno
sirve para escribir un núcleo, un controlador o algo de bajo nivel. O sea, **el hueco de
un lenguaje en español DE SISTEMAS está vacío de verdad.** No vamos a reinventar algo que
ya exista; vamos a un sitio donde no hay nada.

---

## 2. La decisión de fondo: en qué «familia» apoyarnos

Para un primer lenguaje que uno mismo tiene que compilar, conviene una gramática **simple
y regular**. Hay dos familias clásicas:

- **Familia C** (C, y luego Rust): potentísima, pero su gramática es enrevesada de
  analizar (declaraciones, punteros, precedencias). Cara de compilar bien a la primera.
- **Familia Pascal / Oberon / Modula** (la de Niklaus Wirth): gramática limpia, fácil de
  analizar, y —esto es clave— **con ella se han escrito sistemas operativos enteros**
  (Oberon es un SO escrito en el lenguaje Oberon). Tiene punteros, estructuras, acceso a
  memoria y ensamblador en línea, sin runtime pesado.

**Mi recomendación: apoyarnos en la familia Pascal/Oberon**, con palabras en español.
Es la que mejor equilibra «puedo escribir un SO con esto» y «puedo compilarlo yo mismo sin
volverme loco». Es, históricamente, el camino probado para lo que queremos.

---

## 3. Qué pinta tendría (para hacerlo tangible)

Esto son **bocetos para discutir**, no la sintaxis definitiva. La idea es que se lea casi
como instrucciones en español, pero con la precisión de un lenguaje de sistemas.

### Tipos pensados para el hardware
Nombres que dicen el tamaño y el signo sin ambigüedad:

```
  nat8   nat16   nat32      (naturales: sin signo, 8/16/32 bits)
  ent8   ent16   ent32      (enteros: con signo)
  byte                      (= nat8)
  bool                      (cierto / falso)
  puntero a <tipo>          (una dirección a algo de ese tipo)
  dirección                 (una dirección cruda, sin tipo)
```

### Una función y lo básico

```
  función sumar(a: ent16, b: ent16): ent16
      devolver a + b
  fin

  función principal()
      variable i: ent16
      para i desde 0 hasta 9 hacer
          escribir(i)
      fin
      si sumar(2, 3) = 5 entonces
          escribir("cuadra")
      sino
          escribir("algo va mal")
      fin
  fin
```

Palabras clave en español, todas: `función`, `devolver`, `variable`, `constante`,
`si`/`entonces`/`sino`/`fin`, `mientras`/`hacer`, `para`/`desde`/`hasta`,
`estructura`, `tipo`, `según` (para elegir entre casos), `romper`, `seguir`,
`público`, `externo`.

### Lo que lo hace «de sistemas»: memoria y hardware

```
  ; leer y escribir una dirección concreta de memoria
  variable p: puntero a nat16
  p := dirección(0xB800, 0x0000)      ; la memoria de la pantalla en modo texto
  valor(p) := 0x0741                  ; poner una 'A' blanca sobre negro

  ; puertos de entrada/salida
  sacar_puerto(0x60, dato)
  dato := meter_puerto(0x60)

  ; y, cuando haga falta bajar al metal del todo, ensamblador en línea:
  ensamblador
      cli
      mov al, 0x20
      out 0x20, al
      sti
  fin
```

La clave de esto último: **el lenguaje nunca te deja tirado**. Si algo no se puede
expresar, abres un bloque `ensamblador` y lo haces a mano, igual que hoy. Así podemos
migrar Hispa-DOS poco a poco, no de golpe.

---

## 4. El arranque (bootstrap): cómo se hace un compilador desde la nada

Esto es el «huevo y la gallina»: para compilar el lenguaje hace falta un compilador, y el
compilador habría que escribirlo... ¿en qué? El camino probado (lo usan de GCC a Rust) es
por **etapas**:

- **Etapa 0 — El primer compilador, escrito en el PC.** Lo escribimos aquí, en el
  ordenador de desarrollo, en un lenguaje que ya tenemos (yo propondría **C**, por rapidez
  y porque genera binarios pequeños; Python valdría para un primer prototipo aún más
  rápido de hacer). Este compilador coge texto del lenguaje nuevo y **saca código máquina
  de PC**. No tiene que ser bonito ni rápido: solo correcto. Entiende un **subconjunto
  pequeño** del lenguaje (lo justo para arrancar).

- **Etapa 1 — Probarlo de verdad.** Con ese compilador reescribimos **una utilidad real y
  pequeña de Hispa-DOS** (por ejemplo una orden sencilla) en el lenguaje nuevo, la
  compilamos y comprobamos que funciona exactamente igual que la de ensamblador. Ese es el
  primer hito: *«una pieza del sistema actual, hecha en el lenguaje nuevo, funcionando».*

- **Etapa 2 — Autohospedaje.** Reescribimos **el propio compilador en el lenguaje nuevo**.
  Cuando el compilador se puede compilar a sí mismo (y el resultado vuelve a compilarse
  igual), el lenguaje «se sostiene solo» y ya no dependemos del de la etapa 0. A partir de
  ahí, el lenguaje crece escrito en sí mismo.

Dos reglas de oro que aprendí mirando otros bootstraps:
1. **Mantener el lenguaje pequeño al principio.** Todo lo que metas, el primer compilador
   tiene que saber compilarlo. Cuanto más pequeño el núcleo, antes se autohospeda. Se crece
   después.
2. **La eficiencia no es el objetivo al principio.** El primer compilador puede generar
   código torpe; da igual. Primero que ande; afinar es luego.

### ¿Generamos código nosotros o nos apoyamos en NASM?
Dos opciones para la «etapa 0», a decidir:
- **(A) Apoyarnos en NASM como respaldo:** nuestro compilador saca **texto en ensamblador**
  y NASM (que ya usamos y dominamos) lo convierte en binario. *Mucho más fácil y rápido de
  arrancar*; aprovechamos todo lo que ya sabemos de NASM y de Hispa-DOS.
- **(B) Generar el código máquina nosotros:** el compilador escribe directamente los bytes
  del binario (`.COM`/`.OBJ`). Más puro e independiente, pero mucho más trabajo.

**Mi recomendación: empezar por (A)** —sacar ensamblador y que NASM remate— para tener algo
vivo pronto, y dejar (B) como meta a largo plazo si queremos independencia total. Es el
mismo espíritu con el que hemos hecho Hispa-DOS: algo que funcione ya, y afinar sobre vivo.

---

## 5. Las piezas del compilador (para cuando toque)

Un compilador, por dentro, son cuatro pasos. En español, para que cuadre con el proyecto:
1. **El lector (análisis léxico):** parte el texto en palabras, números, símbolos.
2. **La gramática (análisis sintáctico):** monta con esas palabras el «árbol» del programa.
3. **Las comprobaciones (análisis semántico):** que los tipos cuadren, que las variables
   existan, que no sumes una dirección con un texto, etc.
4. **La generación:** del árbol, saca el código (ensamblador, en la opción A).

Para una familia Pascal/Oberon, los pasos 1 y 2 son **sencillos y predecibles** — por eso
la elijo: hace el primer compilador abordable.

---

## 6. Camino por fases (propuesta)

- **Fase 0 — Diseño (esto):** acordar la familia (Pascal/Oberon), los tipos, las palabras
  clave en español y el alcance del primer subconjunto. Un documento de «cómo se escribe».
- **Fase 1 — Compilador mínimo (etapa 0) en el PC:** lee el subconjunto pequeño, saca
  ensamblador, NASM remata. Objetivo: compilar un «hola» y una función con `si`/`para`.
  *(Hecha el 16/11/2026: `castel/ccastel.py`, en Python.)*
- **Fase 2 — Primer hito real:** reescribir **una orden pequeña de Hispa-DOS** en el
  lenguaje nuevo y que funcione igual. Prueba de fuego de que sirve para el sistema real.
  *(Hecha el 17/11/2026: la orden MAS de Hispa-8086, `castel/ejemplos/mas.cas`, comprobada
  contra la de ensamblador por `castel/pruebas.sh`.)*
- **Fase 3 — Crecer el lenguaje:** estructuras, punteros con todas las de la ley, bloques
  `ensamblador`, módulos. Reescribir más piezas de Hispa-DOS.
  *(Hecha el 17/11/2026, antes que la 2 porque MAS la necesitaba: arreglos, estructuras,
  punteros, `ref`, `paso`, `según`, bits, `interrupción`. Los módulos y `nat32` llegaron con
  la Fase 4; quedan los punteros lejanos: CASTEL.md §13.)*
- **Fase 4 — Autohospedaje:** reescribir el compilador en el propio lenguaje.
  *(Primera parte, hecha el 17/11/2026: lo que el compilador necesita para poder escribirse
  en Castel. Números de 32 bits (`nat32`, `ent32`), programas en varios archivos (`importar`
  y `público`) y una biblioteca escrita en Castel: textos, archivos del DOS, un montón de
  memoria y los argumentos; con un programa de verdad hecho con todo ello, la orden CONTAR.
  Segunda parte, hecha el 17/11/2026: el **ensamblador propio** (`ensambla.py` y ENSAMBLA,
  escrito en Castel: el mismo archivo que NASM), el **«modelo grande»** (programas `.EXE` de
  varios segmentos, para lo que no cabe en 64 K), **el compilador escrito en Castel**
  (`castel/compilador/`: el mismo ensamblador que `ccastel.py`, letra a letra) y **el
  arranque en tres pasos**: dentro de Hispa-8086 el compilador se compila a sí mismo y el
  resultado, compilado otra vez, sale idéntico. Castel se sostiene solo. De las dos opciones
  del apartado 4, hemos acabado con las dos: el compilador sigue sacando texto (A), y el
  ensamblador es nuestro (B).)*
- **Fase 5 — Saltar a 32 bits:** con el lenguaje ya sólido, empezar el SO de los 90.
  *(Hecha desde el 18/11/2026: CASTEL32.md, y Hispa-Pentium escrito entero en Castel.)*
- **Fase 6 — El compilador, también de 32 bits (CASTEL32):** *(hecha el 20/11/2026,
  CASTEL32.md §11).* Castel nació para el 386 en adelante, pero su compilador
  escrito en Castel seguía siendo un programa de 16 bits para Hispa-8086 (por cómo
  se llegó: el autohospedaje fue antes que los 32 bits), y con el núcleo de
  Hispa-Pentium 0.8 sus tablas ya no daban más de sí en 640 K. **Decisión:** el
  compilador que manda es **CASTEL32**, los mismos fuentes compilados para 32 bits
  con la memoria plana: en Hispa-Pentium es un programa del anillo 3 (CASTEL32.BIN,
  con ENSAMB32.BIN) y en Hispa-386/486 un programa PE para HW32 (CASTELW.EXE, con
  ENSAMBW.EXE). Hispa-Pentium se compila a sí mismo dentro (el núcleo entero sale
  idéntico) y CASTEL32 llega al punto fijo en tres pasos dentro de Hispa-Pentium y
  dentro de Hispa-DOS. El CASTEL de 16 bits sigue (se compila a sí mismo en
  Hispa-8086 y compila lo de 16 bits), pero **ningún límite del núcleo depende ya
  de él**. Y el compilador queda preparado para más partes de atrás: la de x86-64
  para Hispa-64 es la siguiente (CASTEL32.md §12).

Cada fase es un hito pequeño y comprobable, al estilo de cómo venimos haciendo Hispa-DOS.

---

## 7. Decisiones abiertas (para hablarlo)

1. **La familia:** ¿te convence Pascal/Oberon con palabras en español, o prefieres algo
   más parecido a C?
2. **La etapa 0:** ¿sacar ensamblador y rematar con NASM (mi recomendación), o generar el
   binario nosotros desde el principio?
3. **El lenguaje de la etapa 0:** C (rápido, binarios pequeños) o un prototipo en Python
   primero para ir aún más deprisa.
4. **El primer hito:** ¿qué orden de Hispa-DOS reescribimos como prueba? Conviene la más
   pequeña y autónoma posible.
5. **El nombre.** Lo eliges tú. Algunas ideas para arrancar la conversación: **Castel**
   (de «castellano» + núcleo), **Cimiento**, **Taller**, **Raíz**, **Solera**. O el que te
   guste; esto es tuyo.

---

## 8. Resumen en tres frases

- Hay sitio real: **ningún lenguaje en español sirve hoy para hacer sistemas**; el nuestro
  sería el primero.
- El camino está trillado: **lenguaje pequeño de la familia Pascal/Oberon, en español**,
  con ensamblador en línea para no quedarnos tirados; compilador mínimo primero en el PC
  (sacando ensamblador para NASM), luego autohospedado.
- El primer paso tangible no es «hacer un SO», es **reescribir una orden de Hispa-DOS en el
  lenguaje nuevo y verla funcionar.** Desde ahí, todo lo demás.
