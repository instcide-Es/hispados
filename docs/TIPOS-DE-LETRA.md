# Los tipos de letra de Hispa Ventanas

Los programas de «Productividad» (Pluma, y los que vengan) comparten los tipos de
letra del archivo `\HISPADOS\PROGS\TIPOS.DAT` (en el repositorio,
`programas/comun/TIPOS.DAT`; lo hace `tools/hazfuentes.py` y lo lee
`programas/comun/tipos.inc`). El tipo 0 es «Hispa», la letra de ancho fijo de Hispa
Ventanas, que no está en el archivo.

Cada tipo sale de una fuente libre, dibujada en una casilla de 16 puntos de alto. Lleva
un nombre nuestro porque las licencias piden que lo cambiado no use el nombre original.
Cada uno conserva la licencia de su fuente de origen:

| N.º | Tipo | Sale de | Puntos | Licencia | Octetos |
|---|---|---|---|---|---|
| 1 | Roble | DejaVu Serif | 13 | licencia Bitstream Vera (libre) y dominio público | 3914 |
| 2 | Roble negra | DejaVu Serif Bold | 13 | licencia Bitstream Vera (libre) y dominio público | 4378 |
| 3 | Imprenta | Linux Libertine (fonts-linuxlibertine) | 14 | GPL-2.0 o posterior con excepción de fuentes, y OFL-1.1 | 3532 |
| 4 | Carta | Bitstream Charter (xfonts-scalable) | 13 | licencia libre de Bitstream (1989-1992) | 3608 |
| 5 | Clásica | Old Standard (fonts-oldstandard) | 13 | OFL-1.1 | 3688 |
| 6 | Elegante | GFS Didot (fonts-gfs-didot) | 13 | OFL-1.1 | 3420 |
| 7 | Pergamino | EB Garamond (fonts-ebgaramond) | 14 | OFL-1.1 | 3528 |
| 8 | Losa | Roboto Slab (fonts-roboto-slab) | 13 | Apache-2.0 | 3498 |
| 9 | Llano | DejaVu Sans | 13 | licencia Bitstream Vera (libre) y dominio público | 3740 |
| 10 | Llano negra | DejaVu Sans Bold | 13 | licencia Bitstream Vera (libre) y dominio público | 4012 |
| 11 | Claro | Open Sans (fonts-open-sans) | 13 | Apache-2.0 | 3402 |
| 12 | Pantalla | Play (fonts-play) | 14 | OFL-1.1 | 3464 |
| 13 | Técnica | Jura (fonts-jura) | 15 | OFL-1.1 | 3626 |
| 14 | Redonda | Quicksand (fonts-quicksand) | 13 | OFL-1.1 | 3338 |
| 15 | Rótulo | Linux Biolinum (fonts-linuxlibertine) | 14 | GPL-2.0 o posterior con excepción de fuentes, y OFL-1.1 | 3484 |
| 16 | Estrecha | Dosis (fonts-dosis) | 12 | OFL-1.1 | 3160 |
| 17 | Máquina | Courier Prime (fonts-courier-prime) | 14 | OFL-1.1 | 3094 |
| 18 | Fiesta | Lobster (fonts-lobster) | 15 | OFL-1.1 | 3720 |
| 19 | Manuscrita | Breip (fonts-breip) | 18 | OFL-1.0 | 3990 |

Los documentos guardan el número del tipo: los nuevos se añaden al final de la tabla de
`tools/hazfuentes.py` y los que hay no se cambian de sitio.
