/* plat.c - lo que el motor de los ZIP (zip.h, las pl_) pide de los
 * archivos, con las llamadas al núcleo de Hispa-Pentium: abrir, leer,
 * escribir, carpetas, los nombres largos (llamada 28: VFAT) y los nombres
 * de 8+3 para lo que se saca (el núcleo aún no escribe nombres largos):
 * «Informe de enero.txt» queda INFORM~1.TXT (como hace Windows), sin pisar
 * lo que ya esté; uno que ya es de 8+3 (LEEME.TXT) se queda como está.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "pentium.h"
#include "zip.h"

static void limpia(const char *n, char *d, int cabe)
{
	int k = 0;

	for (; *n && k + 1 < cabe; n++)
		d[k++] = *n == '/' ? '\\' : *n;
	d[k] = 0;
}

static uint32_t abre(const char *n, uint32_t modo)
{
	char t[ZIP_MAX_RUTA];

	limpia(n, t, sizeof(t));
	return sc(SC_ABRIR, (uint32_t)t, modo, 0);
}

int pl_abrir(const char *ruta, int modo)
{
	uint32_t m = abre(ruta, modo == PL_CREAR ? PARA_CREAR : PARA_LEER);

	if (ES_ERROR(m))
		return -1;
	return (int)m;
}

int32_t pl_leer(int h, void *b, uint32_t n)
{
	uint32_t r = sc(SC_LEER, (uint32_t)h, (uint32_t)b, n);

	return ES_ERROR(r) ? -1 : (int32_t)r;
}

int32_t pl_escribir(int h, const void *b, uint32_t n)
{
	uint32_t r = sc(SC_ESCRIBIR_ARCHIVO, (uint32_t)h, (uint32_t)b, n);

	return ES_ERROR(r) ? -1 : (int32_t)r;
}

int pl_ir(int h, uint64_t pos)
{
	if (pos > 0xFFFFFFEFu)
		return -1;
	return sc(SC_MOVER, (uint32_t)h, (uint32_t)pos, 0) == (uint32_t)pos ? 0 : -1;
}

uint64_t pl_largo(int h)
{
	uint32_t r = sc(SC_LARGO, (uint32_t)h, 0, 0);

	return ES_ERROR(r) ? 0 : r;
}

int pl_cortar(int h) { return ES_ERROR(sc(SC_CORTAR, (uint32_t)h, 0, 0)) ? -1 : 0; }
void pl_cerrar(int h) { sc(SC_CERRAR, (uint32_t)h, 0, 0); }
char pl_barra(void) { return '\\'; }

int pl_que_es(const char *ruta)
{
	struct entrada e;
	uint32_t m = abre(ruta, PARA_LEER), r;

	if (ES_ERROR(m)) {
		/* «C:» y «C:\»: la raíz */
		if (ruta[0] && ruta[1] == ':' && (!ruta[2] || (ruta[2] == '\\' && !ruta[3])))
			return 2;
		return 0;
	}
	r = sc(SC_ENTRADA, m, (uint32_t)&e, 0);
	sc(SC_CERRAR, m, 0, 0);
	return ES_ERROR(r) ? 1 : 2;
}

int pl_hacer_carpeta(const char *ruta)
{
	char t[ZIP_MAX_RUTA];
	uint32_t r;

	limpia(ruta, t, sizeof(t));
	r = sc(SC_HACER_CARPETA, (uint32_t)t, 0, 0);
	if (!ES_ERROR(r))
		return 0;
	return pl_que_es(ruta) == 2 ? 0 : -1;
}

int pl_borrar(const char *ruta)
{
	char t[ZIP_MAX_RUTA];

	limpia(ruta, t, sizeof(t));
	return ES_ERROR(sc(SC_BORRAR, (uint32_t)t, 0, 0)) ? -1 : 0;
}

int pl_renombrar(const char *de, const char *a)
{
	char t[ZIP_MAX_RUTA], u[ZIP_MAX_RUTA];

	limpia(de, t, sizeof(t));
	limpia(a, u, sizeof(u));
	return ES_ERROR(sc(SC_RENOMBRAR, (uint32_t)t, (uint32_t)u, 0)) ? -1 : 0;
}

void pl_poner_fecha(const char *ruta, uint16_t fecha, uint16_t hora)
{
	(void)ruta;
	(void)fecha;
	(void)hora;		/* (el núcleo no tiene cómo: queda la de ahora) */
}

/* ---- lo que hay en una carpeta ---- */
typedef struct {
	char corto[13];
	char largo[MAX_LARGO];
	uint8_t atributos;
	uint32_t tam;
	uint16_t fecha, hora;
} pl_e;

static int lee_carpeta(const char *carpeta, pl_e **sale)
{
	uint32_t m = abre(carpeta, PARA_LEER), r;
	struct entrada e;
	char largo[MAX_LARGO];
	pl_e *l = NULL;
	int n = 0, cabe = 0;

	*sale = NULL;
	if (ES_ERROR(m))
		return -1;
	for (;;) {
		memset(largo, 0, sizeof(largo));
		r = sc(SC_ENTRADA_LARGA, m, (uint32_t)&e, (uint32_t)largo);
		if (ES_ERROR(r)) {
			sc(SC_CERRAR, m, 0, 0);
			free(l);
			return -1;
		}
		if (r == 0)
			break;
		e.nombre[12] = 0;
		if (!strcmp(e.nombre, ".") || !strcmp(e.nombre, "..") || (e.atributos & 0x08))
			continue;
		if (n == cabe) {
			pl_e *q = realloc(l, (size_t)(cabe * 2 + 16) * sizeof(pl_e));

			if (!q) {
				sc(SC_CERRAR, m, 0, 0);
				free(l);
				return -1;
			}
			l = q;
			cabe = cabe * 2 + 16;
		}
		strcpy(l[n].corto, e.nombre);
		largo[MAX_LARGO - 1] = 0;
		strcpy(l[n].largo, largo[0] ? largo : e.nombre);
		l[n].atributos = e.atributos;
		l[n].tam = e.tamano;
		l[n].fecha = e.fecha;
		l[n].hora = e.hora;
		n++;
	}
	sc(SC_CERRAR, m, 0, 0);
	*sale = l;
	return n;
}

int pl_listar(const char *carpeta, pl_cada cada, void *u)
{
	pl_e *l;
	int n = lee_carpeta(carpeta, &l), i;
	char largo[MAX_LARGO * 3];

	if (n < 0)
		return -1;
	for (i = 0; i < n; i++) {
		de_437_a_utf8((const unsigned char *)l[i].largo, (int)strlen(l[i].largo), largo, sizeof(largo));
		if (cada(u, largo, l[i].corto, (l[i].atributos & 0x10) != 0, l[i].tam, l[i].fecha, l[i].hora))
			break;
	}
	free(l);
	return 0;
}

static int igual_sin_caja(const char *a, const char *b)
{
	for (; *a && *b; a++, b++) {
		char x = *a, y = *b;

		if (x >= 'a' && x <= 'z')
			x -= 32;
		if (y >= 'a' && y <= 'z')
			y -= 32;
		if (x != y)
			return 0;
	}
	return *a == *b;
}

int pl_ficha(const char *ruta, char *largo, int cabe, int *carpeta, uint64_t *tam, uint16_t *fecha, uint16_t *hora)
{
	char t[ZIP_MAX_RUTA], padre[ZIP_MAX_RUTA], *b;
	pl_e *l;
	int n, i;

	limpia(ruta, t, sizeof(t));
	n = (int)strlen(t);
	while (n > 3 && t[n - 1] == '\\')
		t[--n] = 0;
	if (t[0] && t[1] == ':' && (!t[2] || (t[2] == '\\' && !t[3]))) {
		/* una unidad entera: se llama como su letra */
		largo[0] = t[0];
		largo[1] = 0;
		*carpeta = 1;
		*tam = 0;
		*fecha = *hora = 0;
		return 0;
	}
	b = strrchr(t, '\\');
	if (b) {
		memcpy(padre, t, (size_t)(b - t));
		padre[b - t] = 0;
		if (padre[1] == ':' && !padre[2])
			strcat(padre, "\\");
		if (!padre[0])
			strcpy(padre, "\\");
		b++;
	} else if (t[1] == ':') {
		padre[0] = t[0];
		padre[1] = ':';
		padre[2] = 0;
		b = t + 2;
	} else {
		strcpy(padre, ".");
		b = t;
	}
	n = lee_carpeta(padre, &l);
	if (n < 0)
		return -1;
	for (i = 0; i < n; i++)
		if (igual_sin_caja(l[i].corto, b) || igual_sin_caja(l[i].largo, b)) {
			de_437_a_utf8((const unsigned char *)l[i].largo, (int)strlen(l[i].largo), largo, cabe);
			*carpeta = (l[i].atributos & 0x10) != 0;
			*tam = l[i].tam;
			*fecha = l[i].fecha;
			*hora = l[i].hora;
			free(l);
			return 0;
		}
	free(l);
	return -1;
}

/* ---- los nombres de 8+3 ---- */
static const char letras_que_valen[] = "!#$%&'()-@^_`{}~";

/* una letra de la página 437, en mayúscula y que valga en la FAT (las que
 * no tienen mayúscula en la 437, sin tilde); 0 si no vale; *pierde si no
 * es la misma */
static unsigned char letra_fat(uint32_t c, int *pierde)
{
	static const struct { uint16_t u; unsigned char l; } tildes[] = {
		{ 0xF1, 0xA5 }, { 0xD1, 0xA5 }, { 0xE7, 0x80 }, { 0xC7, 0x80 }, { 0xFC, 0x9A }, { 0xDC, 0x9A },
		{ 0xE9, 0x90 }, { 0xC9, 0x90 }, { 0xE4, 0x8E }, { 0xC4, 0x8E }, { 0xE5, 0x8F }, { 0xC5, 0x8F },
		{ 0xE6, 0x92 }, { 0xC6, 0x92 }, { 0xF6, 0x99 }, { 0xD6, 0x99 },
		{ 0xE1, 'A' }, { 0xE0, 'A' }, { 0xE2, 'A' }, { 0xE3, 'A' }, { 0xC1, 'A' }, { 0xC0, 'A' }, { 0xC2, 'A' },
		{ 0xED, 'I' }, { 0xEC, 'I' }, { 0xEE, 'I' }, { 0xEF, 'I' }, { 0xCD, 'I' }, { 0xCC, 'I' },
		{ 0xF3, 'O' }, { 0xF2, 'O' }, { 0xF4, 'O' }, { 0xF5, 'O' }, { 0xD3, 'O' }, { 0xD2, 'O' }, { 0xD4, 'O' },
		{ 0xFA, 'U' }, { 0xF9, 'U' }, { 0xFB, 'U' }, { 0xDA, 'U' }, { 0xD9, 'U' },
		{ 0xE8, 'E' }, { 0xEA, 'E' }, { 0xEB, 'E' }, { 0xC8, 'E' }, { 0xCA, 'E' }, { 0xCB, 'E' },
	};
	unsigned i;

	if (c >= 'a' && c <= 'z')
		return (unsigned char)(c - 32);
	if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
		return (unsigned char)c;
	if (c < 0x80 && c && strchr(letras_que_valen, (int)c))
		return (unsigned char)c;
	for (i = 0; i < sizeof(tildes) / sizeof(tildes[0]); i++)
		if (tildes[i].u == c) {
			if (tildes[i].l < 0x80)
				*pierde = 1;	/* (sin tilde: ya no es la misma) */
			return tildes[i].l;
		}
	*pierde = 1;
	return c == ' ' ? 0 : '_';
}

static uint32_t sig_utf8(const unsigned char **pp)
{
	const unsigned char *p = *pp;
	uint32_t c = *p++;
	int n = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;

	if (n)
		c &= 0x3F >> n;
	while (n-- && (*p & 0xC0) == 0x80)
		c = c << 6 | (*p++ & 0x3F);
	*pp = p;
	return c;
}

void pl_nombre_local(const char *carpeta, const char *comp, int es_carpeta, char *sale, int cabe)
{
	char base[64], ext[8], prueba[ZIP_MAX_RUTA], nombre[16];
	const unsigned char *p = (const unsigned char *)comp, *punto = NULL, *q;
	int nb = 0, ne = 0, pierde = 0, k, puntos = 0;
	unsigned char l;

	(void)es_carpeta;
	for (q = p; *q; q++)
		if (*q == '.') {
			punto = q;
			puntos++;
		}
	if (punto == p)
		punto = NULL;	/* (un «.algo»: todo es el nombre) */
	if (puntos > 1 || (puntos == 1 && !punto))
		pierde = 1;
	while (*p && p != punto) {
		uint32_t c = sig_utf8(&p);

		if (c == '.')
			continue;
		l = letra_fat(c, &pierde);
		if (!l)
			continue;
		if (nb < 63)
			base[nb++] = (char)l;
	}
	base[nb] = 0;
	if (punto) {
		p = punto + 1;
		while (*p) {
			uint32_t c = sig_utf8(&p);

			l = letra_fat(c, &pierde);
			if (!l)
				continue;
			if (ne < 7)
				ext[ne++] = (char)l;
		}
	}
	ext[ne] = 0;
	if (nb == 0) {
		strcpy(base, "SINNOMBR");
		nb = 8;
		pierde = 1;
	}
	if (nb > 8 || ne > 3)
		pierde = 1;
	if (ne > 3) {
		ext[3] = 0;
		ne = 3;
	}
	/* los nombres que el DOS guarda para sus aparatos */
	if (!strcmp(base, "CON") || !strcmp(base, "PRN") || !strcmp(base, "AUX") || !strcmp(base, "NUL") ||
	    (nb == 4 && (!strncmp(base, "COM", 3) || !strncmp(base, "LPT", 3)) && base[3] >= '1' && base[3] <= '9'))
		pierde = 1;
	if (!pierde) {
		snprintf(sale, (size_t)cabe, "%s%s%s", base, ne ? "." : "", ext);
		return;
	}
	for (k = 1; k < 1000000; k++) {
		char cola[10];
		int nc = snprintf(cola, sizeof(cola), "~%d", k), cab = 8 - nc;

		if (cab < 1)
			break;
		memcpy(nombre, base, (size_t)(nb < cab ? nb : cab));
		snprintf(nombre + (nb < cab ? nb : cab), sizeof(nombre) - 8, "%s%s%s", cola, ne ? "." : "", ext);
		snprintf(prueba, sizeof(prueba), "%s%s%s", carpeta,
			 carpeta[0] && carpeta[strlen(carpeta) - 1] != '\\' && carpeta[strlen(carpeta) - 1] != ':' ? "\\" : "", nombre);
		if (!pl_que_es(prueba))
			break;
	}
	snprintf(sale, (size_t)cabe, "%s", nombre);
}
