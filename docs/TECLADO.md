# El teclado en Hispa-DOS (`TECLADO` / `KEYB`)

`TECLADO` (su nombre clásico es `KEYB`, en `\HISPADOS\TECLADO.COM`) es el
residente que pone la distribución española del teclado, las teclas muertas
(acentos `´ \` ^ ¨`), los atajos de los portátiles de Apple y el **remapeo de
teclas** para teclados raros.

Se engancha al aviso de teclado de la BIOS (INT 15h, AH=4Fh: la BIOS lo llama
desde INT 09h con cada código de rastreo, al pulsar y al soltar). Puede cambiar
ese código antes de que la BIOS lo procese.

## Uso

```
TECLADO [SP|ES|US] [/MAC|/NOMAC]
```

| Orden | Qué hace |
|---|---|
| `TECLADO SP` o `TECLADO ES` | Teclado español (ñ, ç, ¡, ¿, º, ª, acentos, AltGr). |
| `TECLADO US` | Teclado estadounidense (el de la BIOS). |
| `TECLADO` | Enseña el teclado activo. |
| `TECLADO ... /MAC` | Teclas que faltan en los portátiles de Apple (se activan solas si el equipo es de Apple). |
| `TECLADO ... /NOMAC` | Las quita. |

Al cargarse, `TECLADO` lee también `\HISPADOS\TECLAS.INI` (ver abajo). Para
averiguar el código de rastreo de una tecla, use **`TECLA`**: la pulsa y enseña
su código en hexadecimal (y si lleva el prefijo `E0` de las teclas extendidas).

## Remapeo de teclas: `TECLAS.INI`

Algunos teclados de portátil mandan, en alguna tecla, un código que no es el
habitual (por ejemplo, un `Ctrl` izquierdo que no manda `1D`). `TECLAS.INI`
arregla eso: convierte el código que manda una tecla en el de otra.

Es **por equipo** (vive en esa máquina, en `\HISPADOS\TECLAS.INI`) y, al ser un
`*.INI`, el instalador por red lo **conserva** en cada actualización. En el
disco viene un `TECLAS.INI` de ejemplo con todo comentado, así que de fábrica no
cambia el teclado de nadie.

`TECLADO` lo lee al cargarse (al arrancar, desde `AUTOEXEC.BAT`). **Después de
cambiarlo, reinicie** para que lo vuelva a leer.

### Formato

Una línea por remapeo:

```
origen=destino
```

* `origen` y `destino` son **códigos de rastreo «make»** en **HEXADECIMAL**.
* `origen` es la tecla tal como la manda el teclado; `destino`, lo que quiere
  que haga.
* `origen` lleva **2 cifras** (p. ej. `5B`) o, para una tecla **extendida**
  (su código va con el prefijo `E0`), **4 cifras** con el `E0` delante
  (p. ej. `E05B`). `destino` lleva **2 cifras**.
* Las líneas en blanco y las que empiezan por `;` o `#` se ignoran.
* Caben hasta **32** remapeos.

Ejemplos:

```ini
; La tecla cuyo make es 5B actúa como Ctrl izquierdo:
5B=1D

; Igual, pero la tecla es extendida (su código es E0 5B, p. ej. la tecla
; Windows izquierda de muchos portátiles):
E05B=1D
```

Códigos `make` útiles para el destino:

| Código | Tecla | Código | Tecla |
|---|---|---|---|
| `1D` | Ctrl | `3A` | Bloq Mayús |
| `2A` | Mayús izquierda | `01` | Esc |
| `36` | Mayús derecha | `0E` | Retroceso |
| `38` | Alt | `0F` | Tab |
| `39` | Espacio | `1C` | Intro |

Para que el destino sea una **tecla modificadora de verdad** (Ctrl `1D`,
Mayús `2A`/`36`, Alt `38`) y la BIOS actualice sus banderas, use su código
`make`: la BIOS procesa el código de destino y, por ejemplo, una tecla rara
pasa a ser un `Ctrl` auténtico.

### Cómo se aplica

El remapeo se hace en la **entrada** del gancho de la BIOS, **antes** del
procesado propio de `TECLADO` (la distribución española, las teclas muertas y
los atajos de Apple). Así, cuando el destino es una modificadora, la BIOS ve el
código cambiado y actualiza sus banderas de Ctrl/Mayús/Alt; el remapeo a `Ctrl`
funciona de verdad.

* **Al pulsar** (make): si el código entrante coincide con un `origen`, se
  sustituye por el `destino`.
* **Al soltar** (break, que es `make | 80h`): se sustituye igual, conservando el
  bit de soltar (`destino | 80h`).
* **Teclas extendidas** (`E0 xx`): llegan como dos avisos seguidos (primero el
  `E0`, luego el código). El `E0` se deja pasar y el remapeo se aplica al
  segundo código, comparándolo contra los `origen` marcados como extendidos.

### Nota sobre las teclas extendidas

Para un `origen` **normal** (sin `E0`) el remapeo es completo: la BIOS procesa
el destino tal cual. Para un `origen` **extendido** (`E0 xx`), el prefijo `E0`
ya se le ha entregado a la BIOS cuando llega el código, así que el destino se
procesa **manteniendo el `E0`** (como `E0 destino`). Por eso, para teclas
extendidas conviene que el destino sea una **modificadora** (Ctrl, Alt): la
BIOS trata `E0 1D` como Ctrl derecho y `E0 38` como Alt derecho, que ponen las
mismas banderas que los izquierdos, de modo que «actuar como Ctrl» sigue
funcionando. Un destino que sea una letra conviene remaparlo desde un `origen`
normal.

### La ventana «Teclado»

El formato se ha dejado sencillo y bien documentado a propósito: una futura
ventana «Teclado» (un accesorio gráfico) **escribirá** este `TECLAS.INI`. Basta
con escribir líneas `origen=destino` en hexadecimal; no hace falta nada más.
