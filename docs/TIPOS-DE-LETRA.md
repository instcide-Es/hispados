# Los tipos de letra de Hispa Ventanas

Los programas de «Productividad» (Pluma, y los que vengan) comparten los tipos de
letra de `\HISPADOS\PROGS`: `TIPOS.DAT` (letra normal, en casilla de 16 puntos de
pantalla) y `TIPOS12.DAT`, `TIPOS24.DAT` y `TIPOS32.DAT` (los otros tamaños). En el
repositorio están en `programas/comun/`; los hace `tools/hazfuentes.py` y los lee
`programas/comun/tipos.inc`. El tipo 0 es «Hispa», la letra de ancho fijo de Hispa
Ventanas; en los tamaños que no son el normal la hace DejaVu Sans Mono.

Cada tipo sale de una fuente libre. Lleva un nombre nuestro porque las licencias piden
que lo cambiado no use el nombre original, y conserva la licencia de su fuente de origen:

| N.º | Tipo | Sale de | Licencia | Octetos (los cuatro tamaños) |
|---|---|---|---|---|
| 1 | Roble | DejaVu Serif | licencia Bitstream Vera (libre) y dominio público | 31860 |
| 2 | Roble negra | DejaVu Serif Bold | licencia Bitstream Vera (libre) y dominio público | 34172 |
| 3 | Imprenta | Linux Libertine (fonts-linuxlibertine) | GPL-2.0 o posterior con excepción de fuentes, y OFL-1.1 | 28972 |
| 4 | Carta | Bitstream Charter (xfonts-scalable) | licencia libre de Bitstream (1989-1992) | 28464 |
| 5 | Clásica | Old Standard (fonts-oldstandard) | OFL-1.1 | 29528 |
| 6 | Elegante | GFS Didot (fonts-gfs-didot) | OFL-1.1 | 28972 |
| 7 | Pergamino | EB Garamond (fonts-ebgaramond) | OFL-1.1 | 28296 |
| 8 | Losa | Roboto Slab (fonts-roboto-slab) | Apache-2.0 | 29064 |
| 9 | Llano | DejaVu Sans | licencia Bitstream Vera (libre) y dominio público | 30812 |
| 10 | Llano negra | DejaVu Sans Bold | licencia Bitstream Vera (libre) y dominio público | 34248 |
| 11 | Claro | Open Sans (fonts-open-sans) | Apache-2.0 | 27916 |
| 12 | Pantalla | Play (fonts-play) | OFL-1.1 | 28676 |
| 13 | Técnica | Jura (fonts-jura) | OFL-1.1 | 30360 |
| 14 | Redonda | Quicksand (fonts-quicksand) | OFL-1.1 | 28612 |
| 15 | Rótulo | Linux Biolinum (fonts-linuxlibertine) | GPL-2.0 o posterior con excepción de fuentes, y OFL-1.1 | 28796 |
| 16 | Estrecha | Dosis (fonts-dosis) | OFL-1.1 | 25160 |
| 17 | Máquina | Courier Prime (fonts-courier-prime) | OFL-1.1 | 32932 |
| 18 | Fiesta | Lobster (fonts-lobster) | OFL-1.1 | 29836 |
| 19 | Manuscrita | Breip (fonts-breip) | OFL-1.0 | 32392 |

Tamaños: 9, 12, 18 y 24 puntos son los dibujados (casillas de 12, 16, 24 y 32 puntos de
pantalla); 36 y 48 son el doble de 18 y de 24.

Los documentos guardan el número del tipo: los nuevos se añaden al final de la tabla de
`tools/hazfuentes.py` y los que hay no se cambian de sitio.
