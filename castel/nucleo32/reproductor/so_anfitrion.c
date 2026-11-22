/* so_anfitrion.c - el Reproductor en el anfitrión (Linux): lo de so.h con la
 * biblioteca de C, para las pruebas (decodificar a un .WAV y compararlo con
 * la referencia, el fuzz con ASan) y las capturas de la ventana. Lo mismo
 * valdrá para empezar en Hispa-64.
 *
 * El sonido no suena: va a un .WAV (REPRO_WAV=ruta) tan deprisa como se
 * decodifica (la «tarjeta» se lo lleva al momento). La ventana es un lienzo
 * en la memoria: REPRO_CAPTURA=ruta.ppm lo guarda al acabar, y REPRO_SUCESOS
 * da los sucesos (un archivo de renglones «clase a b c», o «espera ms»).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include "so.h"

void *so_pide(size_t n) { return malloc(n ? n : 1); }
void *so_repide(void *p, size_t n) { return realloc(p, n ? n : 1); }
void so_suelta(void *p) { free(p); }
u32 so_memoria_usada(void) { return 0; }

struct so_archivo { FILE *f; u64 largo; };

so_archivo *so_abre(const char *ruta)
{
	FILE *f = fopen(ruta, "rb");
	so_archivo *a;
	struct stat st;

	if (!f)
		return NULL;
	if (fstat(fileno(f), &st) || S_ISDIR(st.st_mode)) {
		fclose(f);
		return NULL;
	}
	a = malloc(sizeof *a);
	a->f = f;
	a->largo = (u64)st.st_size;
	return a;
}

int so_lee(so_archivo *a, void *d, u32 n)
{
	size_t k = fread(d, 1, n, a->f);

	if (!k && ferror(a->f))
		return -1;
	return (int)k;
}

int so_salta_a(so_archivo *a, u64 pos) { return fseeko(a->f, (off_t)pos, SEEK_SET) ? -1 : 0; }
u64 so_largo(so_archivo *a) { return a->largo; }
u64 so_donde(so_archivo *a) { return (u64)ftello(a->f); }
void so_cierra(so_archivo *a) { if (a) { fclose(a->f); free(a); } }

so_archivo *so_crea(const char *ruta)
{
	FILE *f = fopen(ruta, "wb");
	so_archivo *a;

	if (!f)
		return NULL;
	a = malloc(sizeof *a);
	a->f = f;
	a->largo = 0;
	return a;
}

int so_escribe(so_archivo *a, const void *d, u32 n) { return fwrite(d, 1, n, a->f) == n ? 0 : -1; }
int so_borra(const char *ruta) { return unlink(ruta); }
int so_haz_carpeta(const char *ruta) { return mkdir(ruta, 0755); }

int so_existe(const char *ruta, int *c)
{
	struct stat st;

	if (stat(ruta, &st))
		return 0;
	if (c)
		*c = S_ISDIR(st.st_mode);
	return 1;
}

u8 *so_lee_todo(const char *ruta, u32 *largo, u32 max)
{
	so_archivo *a = so_abre(ruta);
	u8 *d;

	if (!a)
		return NULL;
	if (a->largo > max) {
		so_cierra(a);
		return NULL;
	}
	d = malloc(a->largo + 1);
	*largo = (u32)fread(d, 1, a->largo, a->f);
	d[*largo] = 0;
	so_cierra(a);
	return d;
}

void *so_abre_carpeta(const char *ruta) { return opendir(ruta); }

int so_siguiente(void *c, so_entrada *e)
{
	struct dirent *de;

	while ((de = readdir(c))) {
		if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
			continue;
		memset(e, 0, sizeof *e);
		snprintf(e->nombre, sizeof e->nombre, "%s", de->d_name);
		snprintf(e->visto, sizeof e->visto, "%s", de->d_name);
		e->carpeta = de->d_type == DT_DIR;
		return 1;
	}
	return 0;
}

void so_cierra_carpeta(void *c) { closedir(c); }

int so_unidades(char *letras, char *que, int cabe)
{
	if (cabe < 1)
		return 0;
	letras[0] = '/';
	que[0] = 'C';
	return 1;
}

char so_separador(void) { return '/'; }

void so_carpeta_propia(char *d, int cabe)
{
	const char *p = getenv("REPRO_CARPETA");

	snprintf(d, (size_t)cabe, "%s/", p ? p : "/tmp");
}

void so_carpeta_musica(char *d, int cabe)
{
	const char *p = getenv("REPRO_MUSICA");

	snprintf(d, (size_t)cabe, "%s", p ? p : "/tmp");
}

void so_a_utf8(const char *s, char *d, int cabe) { snprintf(d, (size_t)cabe, "%s", s); }
void so_de_utf8(const char *s, char *d, int cabe) { snprintf(d, (size_t)cabe, "%s", s); }

static u32 ms_falsos;
static int reloj_falso = -1;

u32 so_ms(void)
{
	struct timespec t;

	if (reloj_falso < 0)
		reloj_falso = getenv("REPRO_RELOJ_FALSO") != NULL;
	if (reloj_falso)
		return ms_falsos;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return (u32)(t.tv_sec * 1000 + t.tv_nsec / 1000000);
}

void so_duerme(u32 ms)
{
	if (reloj_falso > 0)
		ms_falsos += ms;
	else
		usleep(ms * 1000);
}

u32 so_ciclos(void)
{
	struct timespec t;

	clock_gettime(CLOCK_MONOTONIC, &t);
	return (u32)(t.tv_sec * 1000000 + t.tv_nsec / 1000);
}

void so_dice(const char *t) { fputs(t, stdout); fflush(stdout); }
void so_registro(const char *t) { if (getenv("REPRO_REGISTRO")) fputs(t, stderr); }

static int n_args;
static char **v_args;
void anfitrion_argumentos(int argc, char **argv) { n_args = argc; v_args = argv; }

int so_argumentos(char *d, int cabe)
{
	int i;

	d[0] = 0;
	for (i = 1; i < n_args; i++) {
		if (i > 1)
			strncat(d, " ", (size_t)cabe - strlen(d) - 1);
		strncat(d, v_args[i], (size_t)cabe - strlen(d) - 1);
	}
	return (int)strlen(d);
}

int so_en_consola(void) { return getenv("REPRO_VENTANA") == NULL; }
int so_tecla(void) { return 0; }

/* ---- el sonido: a un .WAV ---- */
static FILE *wav;
static u64 tramas_wav;
static u32 hz_wav;

static void cabecera_wav(void)
{
	u8 c[44];
	u32 datos = (u32)(tramas_wav * 4);

	memcpy(c, "RIFF", 4);
	c[4] = (u8)(datos + 36); c[5] = (u8)((datos + 36) >> 8); c[6] = (u8)((datos + 36) >> 16); c[7] = (u8)((datos + 36) >> 24);
	memcpy(c + 8, "WAVEfmt ", 8);
	c[16] = 16; c[17] = c[18] = c[19] = 0;
	c[20] = 1; c[21] = 0; c[22] = 2; c[23] = 0;
	c[24] = (u8)hz_wav; c[25] = (u8)(hz_wav >> 8); c[26] = (u8)(hz_wav >> 16); c[27] = 0;
	c[28] = (u8)(hz_wav * 4); c[29] = (u8)(hz_wav * 4 >> 8); c[30] = (u8)(hz_wav * 4 >> 16); c[31] = 0;
	c[32] = 4; c[33] = 0; c[34] = 16; c[35] = 0;
	memcpy(c + 36, "data", 4);
	c[40] = (u8)datos; c[41] = (u8)(datos >> 8); c[42] = (u8)(datos >> 16); c[43] = (u8)(datos >> 24);
	fseek(wav, 0, SEEK_SET);
	fwrite(c, 1, 44, wav);
	fseek(wav, 0, SEEK_END);
}

u32 so_hz_tarjeta(void)
{
	const char *h = getenv("REPRO_HZ");

	return h ? (u32)atoi(h) : 48000;
}

int so_sonido_abre(u32 hz)
{
	const char *r = getenv("REPRO_WAV");

	hz_wav = hz;
	if (!r)
		return 1;
	wav = fopen(r, "wb+");
	if (!wav)
		return 0;
	cabecera_wav();
	return 1;
}

u32 so_sonido_da(const i16 *d, u32 n)
{
	if (wav)
		fwrite(d, 4, n, wav);
	tramas_wav += n;
	return n;
}

u32 so_sonido_en_cola(void) { return 0; }
u64 so_sonido_tocadas(void) { return tramas_wav; }
void so_sonido_vacia(void) {}
void so_sonido_pausa(int si) { (void)si; }
void so_sonido_volumen(int v) { (void)v; }
int so_sonido_volumen_general(int v) { return v < 0 ? 80 : v; }

void so_sonido_cierra(void)
{
	if (wav) {
		cabecera_wav();
		fclose(wav);
		wav = NULL;
	}
}

/* ---- el CD: una imagen en crudo (REPRO_CD=archivo de 2352 por sector y
 * REPRO_TOC=«lba,lba,...,fin») para probar ---- */
int so_lectores_cd(char *letras, int cabe)
{
	if (!getenv("REPRO_CD") || cabe < 1)
		return 0;
	letras[0] = 'D';
	return 1;
}

int so_cd_toc(char letra, so_toc *t)
{
	const char *p = getenv("REPRO_TOC");
	int n = 0;

	(void)letra;
	memset(t, 0, sizeof *t);
	if (!p)
		return -1;
	while (*p && n < 100) {
		t->pista[n].lba = (u32)atoi(p);
		t->pista[n].numero = (u8)(n + 1);
		n++;
		p = strchr(p, ',');
		if (!p)
			break;
		p++;
	}
	t->pistas = n - 1;
	t->primera = 1;
	return 0;
}

int so_cd_lee(char letra, u32 lba, u32 n, void *d)
{
	FILE *f = fopen(getenv("REPRO_CD"), "rb");
	size_t k;

	(void)letra;
	if (!f)
		return -1;
	fseeko(f, (off_t)lba * 2352, SEEK_SET);
	k = fread(d, 2352, n, f);
	fclose(f);
	return k == n ? 0 : -1;
}

int so_cd_expulsa(char letra) { (void)letra; return 0; }

/* ---- la ventana: un lienzo ---- */
static lienzo pantalla;
static FILE *sucesos;
static u32 espera_hasta;

int so_hay_escritorio(void) { return getenv("REPRO_VENTANA") != NULL; }

u32 so_ventana(const char *t, int an, int al)
{
	(void)t;
	pantalla.an = an;
	pantalla.al = al;
	pantalla.p = calloc((size_t)an * al, 4);
	if (getenv("REPRO_SUCESOS"))
		sucesos = fopen(getenv("REPRO_SUCESOS"), "r");
	return 1;
}

void so_titulo(u32 v, const char *t) { (void)v; (void)t; }

void so_vuelca(u32 v, const lienzo *l, int x, int y, int an, int al)
{
	int f;

	(void)v;
	if (l->an != pantalla.an || l->al != pantalla.al) {
		free(pantalla.p);
		pantalla.an = l->an;
		pantalla.al = l->al;
		pantalla.p = calloc((size_t)l->an * l->al, 4);
	}
	for (f = y; f < y + al && f < l->al; f++)
		memcpy(pantalla.p + f * l->an + x, l->p + f * l->an + x, (size_t)an * 4);
}

/* guarda lo que hay en la «ventana» (REPRO_CAPTURA y las «captura ruta» de los sucesos) */
static void captura(const char *ruta)
{
	FILE *f = fopen(ruta, "wb");
	int i;

	if (!f)
		return;
	fprintf(f, "P6\n%d %d\n255\n", pantalla.an, pantalla.al);
	for (i = 0; i < pantalla.an * pantalla.al; i++) {
		u8 c[3] = { (u8)(pantalla.p[i] >> 16), (u8)(pantalla.p[i] >> 8), (u8)pantalla.p[i] };

		fwrite(c, 1, 3, f);
	}
	fclose(f);
}

void anfitrion_captura(void)
{
	if (getenv("REPRO_CAPTURA") && pantalla.p)
		captura(getenv("REPRO_CAPTURA"));
}

int so_suceso_de(u32 v, so_suceso *s)
{
	char r[300];

	(void)v;
	if (!sucesos)
		return 0;
	if (espera_hasta && (int)(so_ms() - espera_hasta) < 0)
		return 0;
	espera_hasta = 0;
	while (fgets(r, sizeof r, sucesos)) {
		unsigned c, a, b, d;

		if (r[0] == '#' || r[0] == '\n')
			continue;
		if (!strncmp(r, "espera ", 7)) {
			espera_hasta = so_ms() + (u32)atoi(r + 7);
			return 0;
		}
		if (!strncmp(r, "captura ", 8)) {
			r[strcspn(r, "\n")] = 0;
			captura(r + 8);
			continue;
		}
		if (sscanf(r, "%u %u %u %u", &c, &a, &b, &d) >= 1) {
			s->clase = c;
			s->a = a;
			s->b = b;
			s->c = d;
			return 1;
		}
	}
	fclose(sucesos);
	sucesos = NULL;
	s->clase = EV_CERRAR;
	return 1;
}

void so_cambia_tamano(u32 v, int an, int al) { (void)v; (void)an; (void)al; }
void so_trae(u32 v) { (void)v; }
int so_menu(u32 v, int x, int y, const char *o, u32 g, u32 e) { (void)v; (void)x; (void)y; (void)o; (void)g; (void)e; return 0; }

int so_soltado(char *ruta, int cabe)
{
	const char *p = getenv("REPRO_SOLTADO");

	snprintf(ruta, (size_t)cabe, "%s", p ? p : "");
	return p != NULL;
}

void so_bandeja(u32 v, const u32 *i, const char *a, const char *o) { (void)v; (void)i; (void)a; (void)o; }
void so_al_portapapeles(const char *t) { (void)t; }
void so_lanza(const char *p, const char *a) { (void)p; (void)a; }
int so_otro_en_marcha(const char *p) { (void)p; return 0; }
int so_pedido_nuevo(char *d, int cabe) { (void)d; (void)cabe; return 0; }
