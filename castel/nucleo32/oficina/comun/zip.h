/* zip.h - la oficina de 32 bits: los ZIP (los .ODT, .DOCX, .ODS, .XLSX son
 * un ZIP con XML dentro), con miniz (terceros/miniz, MIT). Se leen desde la
 * memoria sin fiarse: cada archivo de dentro, hasta ZIP_MAX_ARCHIVO
 * descomprimido; entre todos los sacados, hasta ZIP_MAX_TOTAL; hasta
 * ZIP_MAX_ENTRADAS entradas. Se escriben a un bufer (texto.h).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef OFI_ZIP_H
#define OFI_ZIP_H
#include "so.h"
#include "texto.h"

#define ZIP_MAX_ARCHIVO (64u * 1024 * 1024)
#define ZIP_MAX_TOTAL (256u * 1024 * 1024)
#define ZIP_MAX_ENTRADAS 20000

typedef struct zip_lector zip_lector;
typedef struct zip_escritor zip_escritor;

/* abre el ZIP (d, n: tiene que durar mientras esté abierto); NULL si no lo es */
zip_lector *zip_abre(const u8 *d, u32 n);
void zip_cierra(zip_lector *z);
int zip_cuantos(zip_lector *z);
/* el nombre de la entrada i (hasta 511 letras; "" si no vale) */
const char *zip_nombre(zip_lector *z, int i);
/* la entrada de ese nombre (sin mirar mayúsculas; sin «/» ni «./» delante); -1 */
int zip_busca(zip_lector *z, const char *nombre);
/* lo de dentro, descomprimido (de so_pide, con un 0 detrás); NULL si no
 * está, está roto o pasa de los topes. En *n, lo que mide */
u8 *zip_saca(zip_lector *z, const char *nombre, u32 *n);
u8 *zip_saca_i(zip_lector *z, int i, u32 *n);

/* escribir: uno nuevo, sus archivos (comprimidos o, con «guardar», tal
 * cual: el «mimetype» del ODT) y acabarlo en b (y se suelta). 0 si va bien */
zip_escritor *zip_nuevo(void);
int zip_mete(zip_escritor *z, const char *nombre, const void *d, u32 n, int guardar);
int zip_acaba(zip_escritor *z, bufer *b);
void zip_deja(zip_escritor *z);		/* (soltarlo sin acabar) */

#endif
