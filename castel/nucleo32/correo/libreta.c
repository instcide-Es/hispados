/* libreta.c - Correo: la libreta de direcciones (LIBRETA.VCF, vCard 3.0, en
 * la carpeta del correo): la de los contactos que se apuntan a mano o al
 * escribir a alguien, con las veces que se les ha escrito (para el
 * autocompletado: primero los más usados); importar y exportar .vcf.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include <stdio.h>
#include "correo.h"

Contacto *libreta;
int n_libreta;

static void ruta(char *d, u32 cabe)
{
	snprintf(d, cabe, "%s%cLIBRETA.VCF", dir_datos, so_barra());
}

static int pide_sitio(void)
{
	if (!libreta) {
		libreta = so_pide(MAX_CONTACTOS * sizeof(Contacto));
		if (!libreta)
			return -1;
		n_libreta = 0;
	}
	return 0;
}

/* las veces de cada uno, en NOTE no: en un X- propio que vcard.c no ve; se
 * guardan en LIBRETA.USO (una línea por contacto: veces|dirección) */
static void lee_usos(void)
{
	char r[260];
	u8 *d;
	u32 n, i = 0;

	snprintf(r, sizeof r, "%s%cLIBRETA.USO", dir_datos, so_barra());
	d = so_lee_archivo(r, &n, 1u << 20);
	if (!d)
		return;
	while (i < n) {
		u32 j = i, v = 0, k;
		char dir[128];
		int c;

		while (j < n && d[j] != '\n')
			j++;
		for (k = i; k < j && d[k] >= '0' && d[k] <= '9' && v < 1000000; k++)
			v = v * 10 + (u32)(d[k] - '0');
		if (k < j && d[k] == '|') {
			copia_utf8(dir, sizeof dir, (const char *)d + k + 1, j - k - 1 - (j > k + 1 && d[j - 1] == '\r'));
			for (c = 0; c < n_libreta; c++)
				if (!strcmp(libreta[c].correo[0], dir))
					libreta[c].usado = v;
		}
		i = j + 1;
	}
	so_suelta(d);
}

int libreta_lee(void)
{
	char r[260];
	u8 *d;
	u32 n;

	if (pide_sitio())
		return -1;
	n_libreta = 0;
	ruta(r, sizeof r);
	d = so_lee_archivo(r, &n, 8u << 20);
	if (!d)
		return 0;
	n_libreta = vcard_lee(d, n, libreta, MAX_CONTACTOS);
	so_suelta(d);
	if (n_libreta < 0)
		n_libreta = 0;
	lee_usos();
	return n_libreta;
}

int libreta_guarda(void)
{
	char r[260], t[200];
	char *d;
	u32 n;
	void *f;
	int mal, i;

	if (!libreta)
		return 0;
	ruta(r, sizeof r);
	d = vcard_escribe(libreta, n_libreta, &n);
	if (!d)
		return -1;
	f = so_abre(r, SO_CREAR);
	if (!f) {
		so_suelta(d);
		return -1;
	}
	mal = so_escribe(f, d, n);
	so_suelta(d);
	if (so_cierra(f) || mal)
		return -1;
	snprintf(r, sizeof r, "%s%cLIBRETA.USO", dir_datos, so_barra());
	f = so_abre(r, SO_CREAR);
	if (!f)
		return -1;
	for (i = 0; i < n_libreta; i++)
		if (libreta[i].usado) {
			snprintf(t, sizeof t, "%u|%s\r\n", (unsigned)libreta[i].usado, libreta[i].correo[0]);
			mal |= so_escribe(f, t, (u32)strlen(t));
		}
	return so_cierra(f) || mal ? -1 : 0;
}

static int igual_sin_caja(const char *a, const char *b)
{
	for (; *a && *b; a++, b++) {
		char x = *a >= 'A' && *a <= 'Z' ? *a + 32 : *a, y = *b >= 'A' && *b <= 'Z' ? *b + 32 : *b;

		if (x != y)
			return 0;
	}
	return *a == *b;
}

void libreta_usa(const char *nombre, const char *dir)
{
	int i, j;

	if (!dir || !dir[0] || pide_sitio())
		return;
	for (i = 0; i < n_libreta; i++)
		for (j = 0; j < VC_MAX_CORREOS; j++)
			if (libreta[i].correo[j][0] && igual_sin_caja(libreta[i].correo[j], dir)) {
				libreta[i].usado++;
				if (!libreta[i].nombre[0] && nombre && nombre[0])
					copia_utf8(libreta[i].nombre, sizeof libreta[i].nombre, nombre, (u32)strlen(nombre));
				return;
			}
	if (n_libreta >= MAX_CONTACTOS)
		return;
	memset(&libreta[n_libreta], 0, sizeof libreta[0]);
	copia_utf8(libreta[n_libreta].nombre, sizeof libreta[0].nombre, nombre ? nombre : "", nombre ? (u32)strlen(nombre) : 0);
	copia_utf8(libreta[n_libreta].correo[0], sizeof libreta[0].correo[0], dir, (u32)strlen(dir));
	libreta[n_libreta].usado = 1;
	n_libreta++;
}

/* ¿empieza alguna palabra de «t» por «p»? (los dos ya sin tildes ni mayúsculas) */
static int alguna_empieza(const char *t, const char *p)
{
	u32 n = (u32)strlen(p);
	const char *q = t;

	if (!n)
		return 1;
	while (*q) {
		if (!strncmp(q, p, n))
			return 1;
		while (*q && *q != ' ' && *q != '.' && *q != '@' && *q != '-' && *q != '_')
			q++;
		while (*q == ' ' || *q == '.' || *q == '@' || *q == '-' || *q == '_')
			q++;
	}
	return 0;
}

int libreta_completa(const char *t, int *v, int max)
{
	char *p, *a, *b;
	u32 pn, an, bn;
	int i, k = 0, j, x;

	if (!libreta || !t[0] || max <= 0)
		return 0;
	p = utf8_plano(t, (u32)strlen(t), &pn);
	if (!p)
		return 0;
	for (i = 0; i < n_libreta; i++) {
		int casa;

		a = utf8_plano(libreta[i].nombre, (u32)strlen(libreta[i].nombre), &an);
		b = utf8_plano(libreta[i].correo[0], (u32)strlen(libreta[i].correo[0]), &bn);
		casa = (a && alguna_empieza(a, p)) || (b && alguna_empieza(b, p));
		so_suelta(a);
		so_suelta(b);
		if (!casa)
			continue;
		/* por las veces usado (de más a menos): se mete en su sitio */
		if (k == max && libreta[v[k - 1]].usado >= libreta[i].usado)
			continue;
		if (k < max)
			k++;
		for (j = k - 1; j > 0 && libreta[v[j - 1]].usado < libreta[i].usado; j--)
			v[j] = v[j - 1];
		v[j] = i;
		x = j;
		(void)x;
	}
	so_suelta(p);
	return k;
}

int libreta_importa(const char *ruta_vcf)
{
	u8 *d;
	u32 n;
	Contacto *v;
	int k, i, j, nuevos = 0;

	if (pide_sitio())
		return -1;
	d = so_lee_archivo(ruta_vcf, &n, 8u << 20);
	if (!d)
		return -1;
	v = so_pide(MAX_CONTACTOS * sizeof(Contacto));
	if (!v) {
		so_suelta(d);
		return -1;
	}
	k = vcard_lee(d, n, v, MAX_CONTACTOS);
	so_suelta(d);
	for (i = 0; i < k && n_libreta < MAX_CONTACTOS; i++) {
		for (j = 0; j < n_libreta; j++)
			if (v[i].correo[0][0] && igual_sin_caja(libreta[j].correo[0], v[i].correo[0]))
				break;
		if (j < n_libreta) {
			u32 u = libreta[j].usado;

			libreta[j] = v[i];
			libreta[j].usado = u;
		} else {
			libreta[n_libreta++] = v[i];
			nuevos++;
		}
	}
	so_suelta(v);
	libreta_guarda();
	return nuevos;
}

int libreta_exporta(const char *ruta_vcf)
{
	char *d;
	u32 n;
	void *f;
	int mal;

	d = vcard_escribe(libreta, n_libreta, &n);
	if (!d)
		return -1;
	f = so_abre(ruta_vcf, SO_CREAR);
	if (!f) {
		so_suelta(d);
		return -1;
	}
	mal = so_escribe(f, d, n);
	so_suelta(d);
	return so_cierra(f) || mal ? -1 : n_libreta;
}
