/* vcard.h - Correo: las tarjetas de visita (vCard 2.1, 3.0 y 4.0: RFC 2426 y
 * RFC 6350) para la libreta de direcciones: leer (con los renglones partidos,
 * QUOTED-PRINTABLE y CHARSET de la 2.1, los \, \; \n de la 3.0 y 4.0, y
 * varios contactos en un archivo) y escribir (vCard 3.0, UTF-8, CRLF, con los
 * renglones partidos a 75 octetos sin cortar una letra).
 * C99 portable (so.h). Lo que llega es hostil: topes.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef CORREO_VCARD_H
#define CORREO_VCARD_H
#include "so.h"

#define VC_MAX_CORREOS 3
typedef struct {
	char nombre[128];		/* FN (o, si no hay, de N) */
	char correo[VC_MAX_CORREOS][128];	/* EMAIL (el preferido, el primero) */
	char telefono[48];		/* TEL (el primero) */
	char organizacion[96];		/* ORG */
	char nota[256];			/* NOTE */
	u32 usado;			/* (la libreta: las veces que se ha escrito a él) */
} Contacto;

/* los contactos de un archivo .vcf (hasta max): cuántos */
int vcard_lee(const u8 *d, u32 n, Contacto *v, int max);
/* los n contactos como .vcf: en memoria nueva */
char *vcard_escribe(const Contacto *v, int n, u32 *largo);

#endif
