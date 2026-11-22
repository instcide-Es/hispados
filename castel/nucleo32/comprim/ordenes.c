/* ordenes.c - las órdenes ZIP y UNZIP de Comprimidos (en la Consola y en
 * el intérprete de órdenes: ZIP.BIN y UNZIP.BIN lanzan COMPRIM.BIN con
 * /ZIP o /UNZIP delante), al estilo de las de Info-ZIP:
 *
 *   ZIP [-0..-9] [-q] ARCHIVO.ZIP RUTA...      mete (o pone al día) archivos y carpetas
 *   ZIP -d [-q] ARCHIVO.ZIP NOMBRE...          quita esos (vale * y ?)
 *   UNZIP [-o] [-q] [-P CLAVE] [-d CARPETA] ARCHIVO.ZIP [NOMBRE...]
 *   UNZIP -l ARCHIVO.ZIP                       lo que lleva
 *   UNZIP -t [-P CLAVE] ARCHIVO.ZIP            probarlo (CRC) sin sacar nada
 *
 * Es lo mismo en Hispa-Pentium (comprim.c) y en el anfitrión (anfitrion.c,
 * las pruebas). Los textos, en la página 437.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "zip.h"
#include "ordenes.h"

static int callado;

static void di(const char *f, ...)
{
	char t[1200];
	va_list a;

	va_start(a, f);
	vsnprintf(t, sizeof(t), f, a);
	va_end(a);
	pl_escribe(t);
}

static const char *en437(const char *u)
{
	static char t[4][600];
	static int k;

	k = (k + 1) & 3;
	utf8_a_437(u, t[k], sizeof(t[k]));
	return t[k];
}

/* ¿casa el nombre con el patrón? (* y ?, sin mirar mayúsculas) */
static int casa(const char *p, const char *n)
{
	unsigned char a, b;

	if (!*p)
		return !*n;
	if (*p == '*') {
		for (;; n++) {
			if (casa(p + 1, n))
				return 1;
			if (!*n)
				return 0;
		}
	}
	if (!*n)
		return 0;
	a = (unsigned char)*p;
	b = (unsigned char)*n;
	if (a >= 'a' && a <= 'z')
		a -= 32;
	if (b >= 'a' && b <= 'z')
		b -= 32;
	if (a != '?' && a != b)
		return 0;
	return casa(p + 1, n + 1);
}

/* «casa» con lo que haya en la línea (en la página 437) o con su carpeta */
static int lo_piden(const zmiembro *m, char **nombres, int n)
{
	char t[600];
	int i;
	size_t l;

	if (n == 0)
		return 1;
	utf8_a_437(m->nombre, t, sizeof(t));
	for (i = 0; i < n; i++) {
		if (casa(nombres[i], t))
			return 1;
		l = strlen(nombres[i]);
		if (l && !strncmp(nombres[i], t, l) && (t[l] == '/' || nombres[i][l - 1] == '/'))
			return 1;
	}
	return 0;
}

static void fecha_de(uint16_t f, uint16_t h, char *t)
{
	snprintf(t, 20, "%02u/%02u/%04u %02u:%02u", f & 31, (f >> 5) & 15, 1980 + (f >> 9), h >> 11, (h >> 5) & 63);
}

static int uso_zip(void)
{
	pl_escribe("Uso: ZIP [-0..-9] [-q] ARCHIVO.ZIP RUTA...   mete archivos y carpetas (con lo de dentro)\r\n"
		   "     ZIP -d ARCHIVO.ZIP NOMBRE...            los quita (vale * y ?)\r\n"
		   "     -0 solo guarda; -9 comprime lo m\xA0s; -q sin decir cada uno.\r\n");
	return 2;
}

static int uso_unzip(void)
{
	pl_escribe("Uso: UNZIP [-o] [-q] [-P CLAVE] [-d CARPETA] ARCHIVO.ZIP [NOMBRE...]   lo saca\r\n"
		   "     UNZIP -l ARCHIVO.ZIP     lo que lleva\r\n"
		   "     UNZIP -t ARCHIVO.ZIP     lo prueba (sin sacar nada)\r\n"
		   "     -o reemplaza lo que ya est\xA0; -P la contrase\xA4" "a (ZipCrypto o AES).\r\n");
	return 2;
}

int pl_progreso_orden(uint64_t hecho, uint64_t total, const char *nombre)
{
	(void)hecho;
	(void)total;
	(void)nombre;
	return pl_cortado();
}

int orden_zip(int argc, char **argv)
{
	int i, nivel = 6, quitar = 0, r, h;
	const char *archivo = NULL;
	char tmp[ZIP_MAX_RUTA];
	char **rutas;
	int n_rutas = 0;
	zarchivo z;
	zfuentes l;
	uint8_t *fuera = NULL;
	int hay = 0;

	callado = 0;
	rutas = calloc((size_t)argc + 1, sizeof(char *));
	if (!rutas)
		return 1;
	for (i = 0; i < argc; i++) {
		if (argv[i][0] == '-' && argv[i][1] && !archivo) {
			if (argv[i][1] >= '0' && argv[i][1] <= '9')
				nivel = argv[i][1] - '0';
			else if (argv[i][1] == 'd' || argv[i][1] == 'D')
				quitar = 1;
			else if (argv[i][1] == 'q' || argv[i][1] == 'Q')
				callado = 1;
			else
				return uso_zip();
		} else if (!archivo)
			archivo = argv[i];
		else
			rutas[n_rutas++] = argv[i];
	}
	if (!archivo || !n_rutas)
		return uso_zip();
	pl_temporal(archivo, tmp, sizeof(tmp));
	memset(&z, 0, sizeof(z));
	z.h = -1;
	memset(&l, 0, sizeof(l));
	h = pl_que_es(archivo);
	if (h == 2) {
		di("zip: %s es una carpeta\r\n", archivo);
		return 1;
	}
	if (h == 1) {
		r = zip_abrir(&z, archivo);
		if (r) {
			di("zip: %s: %s\r\n", archivo, zip_texto_error(r));
			return 1;
		}
		hay = 1;
	} else if (quitar) {
		di("zip: no est\xA0 %s\r\n", archivo);
		return 1;
	}
	if (quitar) {
		int cuantos = 0;

		fuera = calloc(z.n + 1, 1);
		if (!fuera)
			return 1;
		for (i = 0; i < (int)z.n; i++)
			if (lo_piden(&z.m[i], rutas, n_rutas)) {
				fuera[i] = 1;
				cuantos++;
				if (!callado)
					di("  quitando: %s\r\n", en437(z.m[i].nombre));
			}
		if (!cuantos) {
			pl_escribe("zip: no hay nada con ese nombre\r\n");
			zip_cerrar(&z);
			return 12;
		}
		r = zip_hacer(archivo, tmp, &z, fuera, NULL, nivel);
	} else {
		zip_excluir[0] = archivo;
		zip_excluir[1] = tmp;
		for (i = 0; i < n_rutas; i++) {
			r = zip_juntar(&l, rutas[i], NULL);
			if (r) {
				di("zip: %s: %s\r\n", rutas[i], zip_texto_error(r));
				zip_suelta_fuentes(&l);
				if (hay)
					zip_cerrar(&z);
				return 1;
			}
		}
		if (!callado)
			for (i = 0; i < (int)l.n; i++) {
				uint32_t k;
				int ya = 0;

				for (k = 0; hay && k < z.n; k++)
					if (!strcmp(z.m[k].nombre, l.f[i].nombre))
						ya = 1;
				di("  %s: %s\r\n", ya ? "poniendo al d\xA1" "a" : "metiendo", en437(l.f[i].nombre));
			}
		r = zip_hacer(archivo, tmp, hay ? &z : NULL, NULL, &l, nivel);
		zip_suelta_fuentes(&l);
	}
	free(fuera);
	free(rutas);
	if (hay)
		zip_cerrar(&z);
	if (r) {
		di("zip: %s\r\n", zip_texto_error(r));
		return 1;
	}
	return 0;
}

int orden_unzip(int argc, char **argv)
{
	int i, lista = 0, prueba = 0, reemplazar = 0, r, n_nombres = 0, malos = 0;
	const char *archivo = NULL, *carpeta = ".", *clave = NULL;
	char **nombres;
	zarchivo z;
	uint8_t *elegidos;
	char t[40];

	callado = 0;
	nombres = calloc((size_t)argc + 1, sizeof(char *));
	if (!nombres)
		return 1;
	for (i = 0; i < argc; i++) {
		if (argv[i][0] == '-' && argv[i][1] && argv[i][2] == 0) {
			switch (argv[i][1]) {
			case 'l': case 'L': lista = 1; break;
			case 't': case 'T': prueba = 1; break;
			case 'o': case 'O': reemplazar = 1; break;
			case 'q': case 'Q': callado = 1; break;
			case 'P': case 'p':
				if (++i >= argc)
					return uso_unzip();
				clave = argv[i];
				break;
			case 'd': case 'D':
				if (++i >= argc)
					return uso_unzip();
				carpeta = argv[i];
				break;
			default:
				return uso_unzip();
			}
		} else if (!archivo)
			archivo = argv[i];
		else
			nombres[n_nombres++] = argv[i];
	}
	if (!archivo)
		return uso_unzip();
	r = zip_abrir(&z, archivo);
	if (r) {
		di("unzip: %s: %s\r\n", archivo, zip_texto_error(r));
		free(nombres);
		return 9;
	}
	elegidos = calloc(z.n + 1, 1);
	if (!elegidos)
		return 1;
	for (i = 0; i < (int)z.n; i++)
		elegidos[i] = (uint8_t)lo_piden(&z.m[i], nombres, n_nombres);
	if (lista) {
		uint64_t tt = 0, tc = 0;
		uint32_t n = 0;

		di("Archivo: %s%s\r\n", archivo, z.zip64 ? " (ZIP64)" : "");
		pl_escribe("      Tama\xA4o  Comprimido  Fecha      Hora   Nombre\r\n"
			   "------------  ----------  ---------- -----  ------\r\n");
		for (i = 0; i < (int)z.n; i++) {
			zmiembro *m = &z.m[i];

			if (!elegidos[i])
				continue;
			fecha_de(m->fecha, m->hora, t);
			di("%12llu  %10llu  %s  %s%s%s\r\n", (unsigned long long)m->tam, (unsigned long long)m->comp, t, en437(m->nombre),
			   m->cifrado ? (m->cifrado == ZC_AES ? "  [AES]" : "  [con clave]") : "", m->peligro ? "  [PELIGROSO: no se saca]" : "");
			tt += m->tam;
			tc += m->comp;
			n++;
		}
		di("------------  ----------                    ------\r\n%12llu  %10llu                    %u archivos\r\n",
		   (unsigned long long)tt, (unsigned long long)tc, n);
		zip_cerrar(&z);
		free(elegidos);
		free(nombres);
		return 0;
	}
	if (zip_pide_clave(&z, elegidos) && !clave) {
		clave = pl_pide_clave(archivo);
		if (!clave) {
			pl_escribe("unzip: hace falta la contrase\xA4" "a (-P CLAVE)\r\n");
			zip_cerrar(&z);
			free(elegidos);
			free(nombres);
			return 82;
		}
	}
	if (prueba) {
		for (i = 0; i < (int)z.n; i++) {
			if (!elegidos[i] || z.m[i].carpeta)
				continue;
			r = zip_sacar(&z, (uint32_t)i, NULL, clave);
			if (r)
				malos++;
			if (r || !callado)
				di("  probando: %-40s %s\r\n", en437(z.m[i].nombre), r ? zip_texto_error(r) : "bien");
		}
		if (malos)
			di("Hay %d con errores en %s\r\n", malos, archivo);
		else
			di("No hay errores en %s\r\n", archivo);
		zip_cerrar(&z);
		free(elegidos);
		free(nombres);
		return malos ? 2 : 0;
	}
	{
		zcuenta c;

		if (!callado)
			for (i = 0; i < (int)z.n; i++)
				if (elegidos[i])
					di("  %s: %s\r\n", z.m[i].peligro ? "NO SE SACA" : "sacando", en437(z.m[i].nombre));
		r = zip_sacar_todo(&z, carpeta, elegidos, "", clave, reemplazar, &c);
		if (c.saltados)
			di("unzip: %u ya estaban y no se han tocado (-o para reemplazarlos)\r\n", c.saltados);
		if (r && !c.fallos)
			di("unzip: %s: %s\r\n", archivo, zip_texto_error(r));
		if (c.fallos)
			di("unzip: %u no se han podido sacar (el \xA3ltimo, %s: %s)\r\n", c.fallos, en437(c.ultimo), zip_texto_error(c.ultimo_error));
		if (!callado || c.fallos)
			di("Sacados %u de %s\r\n", c.sacados, archivo);
		zip_cerrar(&z);
		free(elegidos);
		free(nombres);
		return r ? (r == ZIP_E_CLAVE ? 82 : r == ZIP_E_BOMBA ? 80 : 50) : 0;
	}
}
