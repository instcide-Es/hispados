/* orto.h - la oficina de 32 bits: el corrector ortográfico de español que
 * comparten los programas (Pluma 2, y luego Cuentas 2...). Es el de
 * Hispa-386 (programas/comun/ortografia.inc) pasado a C portable, con el
 * mismo diccionario: DICCIO.DAT (un autómata con todas las formas, en la
 * página 437 de Hispa; de dónde sale, su licencia y sus reglas, en
 * docs/ORTOGRAFIA.md). Las palabras entran y salen en UTF-32 (el texto de
 * los documentos). Un solo diccionario por programa (estado de este módulo).
 *
 *   - Una palabra vale tal cual, con la primera en mayúscula o toda en
 *     mayúsculas (sin las tildes que la 437 no tiene: «ARBOL» vale por
 *     «árbol»); los nombres propios, solo con su mayúscula. Lo que lleva
 *     cifras, lo de una letra y lo de más de 63 no se juzga (vale).
 *   - Las propuestas: las palabras del diccionario a una corrección (una
 *     letra cambiada, de más, de menos, dos vecinas trocadas, ll/y, k/qu,
 *     x/cc), con los acentos de regalo; por la misma nota que en Hispa-386.
 *   - Sin diccionario (no está, o está roto), todo vale.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef OFI_ORTO_H
#define OFI_ORTO_H
#include "so.h"

#define ORTO_LARGO 63		/* la palabra más larga que se mira */
#define ORTO_SUG 8		/* las propuestas, a lo más */

/* el diccionario: los datos (de so_pide) pasan a ser del corrector (si no
 * vale, los suelta). 1 si es un diccionario entero y sano */
int orto_carga(u8 *datos, u32 n);
/* lo lee de un archivo (\HISPA\PLUMA\DICCIO.DAT...); 1 si vale */
int orto_carga_archivo(const char *ruta);
void orto_suelta(void);
int orto_hay(void);

/* ¿está bien escrita? (1 sí, o no se juzga; 0 no se conoce) */
int orto_bien(const u32 *p, u32 n);
/* las propuestas (hasta max, ORTO_SUG a lo más): cada una acabada en 0, la
 * más probable primero, con las mayúsculas de la tecleada. Da cuántas */
int orto_sugiere(const u32 *p, u32 n, u32 sug[][ORTO_LARGO + 1], int max);
/* lo más que ha andado la última búsqueda de propuestas (para medir) */
u32 orto_pasos(void);

/* las palabras del usuario: añadir una (0 si queda apuntada o ya valía; -1
 * si no vale o no cabe); la lista como texto UTF-8, una por línea (de
 * so_pide; en *n, su largo); y ponerla de un texto (palabras separadas por
 * espacios o saltos de línea: lo que no vale se deja). Apuntada en
 * minúsculas vale también con la primera en mayúscula y toda en mayúsculas */
int orto_propia(const u32 *p, u32 n);
char *orto_propias(u32 *n);
void orto_propias_poner(const char *t, u32 n);
/* «omitir todas»: esta palabra vale mientras dure el programa */
int orto_omite(const u32 *p, u32 n);
void orto_olvida_omitidas(void);

/* ---- partir el texto en palabras ---- */
/* ¿es de las que forman palabras? (letras, también con tilde, y cifras; el
 * guion y el apóstrofo parten: el diccionario no los tiene) */
int orto_de_palabra(u32 c);
/* la palabra que lleva (o que acaba en) la posición o: de *ini a *fin; 0 si no hay */
int orto_palabra_en(const u32 *t, u32 n, u32 o, u32 *ini, u32 *fin);
/* las faltas del texto: en marcas (n octetos), 1 donde empieza una palabra
 * mal escrita y 2 en sus demás letras (0 lo demás). Da cuántas hay. La
 * palabra que acaba justo en «no_hasta» (la que se está escribiendo, con el
 * cursor detrás) no se marca; con no_hasta = 0xFFFFFFFF, todas */
u32 orto_faltas(const u32 *t, u32 n, u8 *marcas, u32 no_hasta);

#endif
