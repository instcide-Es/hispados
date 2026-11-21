/* anfitrion.c - el motor de Comprimidos (zip.c y ordenes.c) en Linux, para
 * las pruebas: las mismas órdenes ZIP y UNZIP que en Hispa-Pentium, sobre
 * POSIX, y «fuzz ARCHIVO», que abre, lista, prueba y saca un ZIP cualquiera
 * (para el fuzz con ASan: prueba/fuzz.py). Lo hace prueba/prueba.sh.
 *
 *   anfitrion zip [-0..-9] [-d] ARCHIVO.ZIP RUTA...
 *   anfitrion unzip [-l|-t] [-o] [-P CLAVE] [-d CARPETA] ARCHIVO.ZIP [NOMBRE...]
 *   anfitrion fuzz ARCHIVO [CARPETA]
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include <utime.h>
#include <sys/stat.h>
#include "zip.h"
#include "ordenes.h"

int pl_abrir(const char *ruta, int modo)
{
	int h = open(ruta, modo == PL_CREAR ? O_RDWR | O_CREAT | O_TRUNC : O_RDONLY, 0644);
	struct stat s;

	if (h >= 0 && modo != PL_CREAR && (fstat(h, &s) || S_ISDIR(s.st_mode))) {
		close(h);
		return -1;
	}
	return h;
}
int32_t pl_leer(int h, void *b, uint32_t n) { return (int32_t)read(h, b, n); }
int32_t pl_escribir(int h, const void *b, uint32_t n) { return (int32_t)write(h, b, n); }
int pl_ir(int h, uint64_t pos) { return lseek(h, (off_t)pos, SEEK_SET) == (off_t)pos ? 0 : -1; }
uint64_t pl_largo(int h)
{
	struct stat s;

	return fstat(h, &s) ? 0 : (uint64_t)s.st_size;
}
int pl_cortar(int h) { return ftruncate(h, lseek(h, 0, SEEK_CUR)); }
void pl_cerrar(int h) { close(h); }
int pl_que_es(const char *ruta)
{
	struct stat s;

	if (lstat(ruta, &s))
		return 0;
	return S_ISDIR(s.st_mode) ? 2 : 1;
}
int pl_hacer_carpeta(const char *ruta) { return mkdir(ruta, 0755) && pl_que_es(ruta) != 2; }
int pl_borrar(const char *ruta) { return unlink(ruta); }
int pl_renombrar(const char *de, const char *a) { return rename(de, a); }
char pl_barra(void) { return '/'; }

void pl_nombre_local(const char *carpeta, const char *comp, int es_carpeta, char *sale, int cabe)
{
	(void)carpeta;
	(void)es_carpeta;
	snprintf(sale, (size_t)cabe, "%s", comp);
}

static void a_dos(time_t t, uint16_t *fecha, uint16_t *hora)
{
	struct tm *l = localtime(&t);

	if (l->tm_year < 80) {
		*fecha = (1 << 5) | 1;
		*hora = 0;
		return;
	}
	*fecha = (uint16_t)(((l->tm_year - 80) << 9) | ((l->tm_mon + 1) << 5) | l->tm_mday);
	*hora = (uint16_t)((l->tm_hour << 11) | (l->tm_min << 5) | (l->tm_sec / 2));
}

static int por_nombre(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

int pl_listar(const char *carpeta, pl_cada cada, void *u)
{
	DIR *d = opendir(carpeta);
	struct dirent *e;
	char **n = NULL, ruta[4096];
	size_t k = 0, c = 0, i;
	struct stat s;
	uint16_t f, h;

	if (!d)
		return -1;
	while ((e = readdir(d))) {
		if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
			continue;
		if (k == c) {
			c = c * 2 + 16;
			n = realloc(n, c * sizeof(char *));
		}
		n[k++] = strdup(e->d_name);
	}
	closedir(d);
	if (k)
		qsort(n, k, sizeof(char *), por_nombre);
	for (i = 0; i < k; i++) {
		snprintf(ruta, sizeof(ruta), "%s/%s", carpeta, n[i]);
		if (!lstat(ruta, &s) && !S_ISLNK(s.st_mode)) {
			a_dos(s.st_mtime, &f, &h);
			if (cada(u, n[i], n[i], S_ISDIR(s.st_mode), (uint64_t)s.st_size, f, h))
				break;
		}
	}
	for (i = 0; i < k; i++)
		free(n[i]);
	free(n);
	return 0;
}

int pl_ficha(const char *ruta, char *largo, int cabe, int *carpeta, uint64_t *tam, uint16_t *fecha, uint16_t *hora)
{
	struct stat s;
	char t[4096], *b;
	size_t l;

	if (stat(ruta, &s))
		return -1;
	snprintf(t, sizeof(t), "%s", ruta);
	l = strlen(t);
	while (l > 1 && t[l - 1] == '/')
		t[--l] = 0;
	b = strrchr(t, '/');
	snprintf(largo, (size_t)cabe, "%s", b ? b + 1 : t);
	*carpeta = S_ISDIR(s.st_mode);
	*tam = (uint64_t)s.st_size;
	a_dos(s.st_mtime, fecha, hora);
	return 0;
}

void pl_poner_fecha(const char *ruta, uint16_t fecha, uint16_t hora)
{
	struct tm l;
	struct utimbuf u;

	memset(&l, 0, sizeof(l));
	l.tm_year = (fecha >> 9) + 80;
	l.tm_mon = ((fecha >> 5) & 15) - 1;
	l.tm_mday = fecha & 31;
	l.tm_hour = hora >> 11;
	l.tm_min = (hora >> 5) & 63;
	l.tm_sec = (hora & 31) * 2;
	l.tm_isdst = -1;
	u.actime = u.modtime = mktime(&l);
	utime(ruta, &u);
}

int pl_progreso(uint64_t hecho, uint64_t total, const char *nombre) { return pl_progreso_orden(hecho, total, nombre); }
int pl_cortado(void) { return 0; }
const char *pl_pide_clave(const char *archivo) { (void)archivo; return NULL; }
void pl_temporal(const char *ruta, char *sale, int cabe) { snprintf(sale, (size_t)cabe, "%s.tmp$", ruta); }

/* de la página 437 a UTF-8, para la terminal */
void pl_escribe(const char *t)
{
	char u[8192];

	de_437_a_utf8((const unsigned char *)t, (int)strlen(t), u, sizeof(u));
	fputs(u, stdout);
}

static void borra_todo(const char *c)
{
	DIR *d = opendir(c);
	struct dirent *e;
	char r[4096];

	if (!d)
		return;
	while ((e = readdir(d))) {
		if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
			continue;
		snprintf(r, sizeof(r), "%s/%s", c, e->d_name);
		if (pl_que_es(r) == 2)
			borra_todo(r);
		else
			unlink(r);
	}
	closedir(d);
	rmdir(c);
}

int main(int argc, char **argv)
{
	/* (para probar los topes con poco) */
	if (getenv("COMPRIM_MAX_TOTAL"))
		zip_limites.max_total = strtoull(getenv("COMPRIM_MAX_TOTAL"), NULL, 10);
	if (argc >= 2 && !strcmp(argv[1], "zip"))
		return orden_zip(argc - 2, argv + 2);
	if (argc >= 2 && !strcmp(argv[1], "unzip"))
		return orden_unzip(argc - 2, argv + 2);
	if (argc >= 3 && !strcmp(argv[1], "fuzz")) {
		/* lo que haría un usuario con un ZIP cualquiera: no tiene que romperse nada */
		zarchivo z;
		zcuenta c;
		const char *dir = argc > 3 ? argv[3] : "/tmp/comprim-fuzz";
		uint32_t i;
		int r = zip_abrir(&z, argv[2]);
		uint8_t *quitar;

		printf("abrir: %s\n", zip_texto_error(r));
		if (r)
			return 0;
		for (i = 0; i < z.n; i++)
			zip_sacar(&z, i, NULL, "clave");
		borra_todo(dir);
		mkdir(dir, 0755);
		r = zip_sacar_todo(&z, dir, NULL, "", "clave", 1, &c);
		printf("sacar: %s (%u sacados, %u fallos)\n", zip_texto_error(r), c.sacados, c.fallos);
		/* y rehacerlo quitando el primero (copia los demás tal cual) */
		quitar = calloc(z.n + 1, 1);
		quitar[0] = 1;
		{
			char d[4096], t[4096];

			snprintf(d, sizeof(d), "%s/rehecho.zip", dir);
			snprintf(t, sizeof(t), "%s/rehecho.tmp", dir);
			r = zip_hacer(d, t, &z, quitar, NULL, 6);
			printf("rehacer: %s\n", zip_texto_error(r));
		}
		free(quitar);
		zip_cerrar(&z);
		borra_todo(dir);
		return 0;
	}
	fprintf(stderr, "uso: anfitrion zip|unzip ... | fuzz ARCHIVO [CARPETA]\n");
	return 2;
}
