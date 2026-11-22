/* letras.h - la oficina de 32 bits: las letras TrueType que comparten los
 * programas (las DejaVu de dentro y las .TTF que el usuario pone en su
 * carpeta, \DOCUMENT\LETRAS o \LETRAS), lo que mide cada letra (en unidades
 * de su letra: lo mismo en la pantalla, a cualquier zoom, que en el PDF) y
 * su dibujo con grises (stb_truetype), guardado para no rehacerlo.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef OFI_LETRAS_H
#define OFI_LETRAS_H
#include "so.h"
#include "ttf.h"

#define LETRAS_MAX 48

/* la negrita y la cursiva hechas a mano (cuando la familia no las trae) */
#define FALSA_NEGRITA 1
#define FALSA_CURSIVA 2

void letras_inicia(void);
/* añade una letra de la memoria (no se copia: tiene que durar); su número o -1 */
int letras_mete(const u8 *d, u32 n, int propia);
/* las .TTF de una carpeta; cuántas */
int letras_carpeta(const char *ruta);
/* las familias que hay (sin repetir, por orden), para la lista de la barra */
int letras_familias(const char *lista[], int max);
/* la cara para una familia y un estilo (si no está la familia, la DejaVu que
 * más se le parezca: «Times New Roman» -> DejaVu Serif...). En *falsas, lo
 * que hay que hacer a mano (FALSA_NEGRITA, FALSA_CURSIVA) */
int letra_cara(const char *familia, int negrita, int cursiva, int *falsas);
/* ¿está la familia de verdad? (o va con otra en su lugar) */
int letra_hay_familia(const char *familia);
const char *letra_familia(int cara);
ttf *letra_ttf(int cara);
/* el glifo de la letra c en la cara; si no lo tiene, en la DejaVu del mismo
 * estilo (en *cara_real, la que lo tiene) */
u32 letra_glifo(int cara, u32 c, int *cara_real);
/* lo que avanza (en unidades de la letra) */
int letra_avance(int cara, u32 glifo);
/* el avance de la letra c, en milésimas de em (con el respaldo) */
int letra_milesimas(int cara, u32 c);
u32 letra_em(int cara);

/* el dibujo de un glifo: los grises (0-255) de an x al, con su esquina en
 * (x0, y0) respecto del punto de la línea de base */
typedef struct {
	u8 *gris;
	short an, al, x0, y0;
} glifo_px;
/* tam: el tamaño en cuartos de punto de pantalla (el em); sub: un tercio
 * de punto a la derecha (0, 1, 2); falsas: FALSA_NEGRITA, FALSA_CURSIVA */
const glifo_px *letra_dibujo(int cara, u32 glifo, int tam, int sub, int falsas);
/* lo que gastan los dibujos guardados (y vaciarlos) */
u32 letras_memoria(void);
void letras_olvida(void);

/* las familias de dentro */
#define FAM_SANS "DejaVu Sans"
#define FAM_SERIF "DejaVu Serif"
#define FAM_MONO "DejaVu Sans Mono"

#endif
