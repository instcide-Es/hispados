/* so_hispa64.c - la oficina (Pluma 2, Cuentas 2...) en Hispa-64: lo de so.h
 * con la capa POSIX de Hispa-64 (musl) y las ventanas de su servidor
 * (VENTANAS: castel/nucleo64/programas/ventanas/hv.h; docs/64/HISPA-64.md,
 * sección 14). Lo de los archivos y la memoria es como so_anfitrion.c (el
 * de Linux), con las unidades de DOS: «C:\DOCUMENT\x.plu» es
 * /DISCO/DOCUMENT/x.plu, con DISCO el de $HISPA_C o, si no se dice, uno de
 * los discos de «/» (sata1, nvme0p2...): el que tenga la carpeta DOCUMENT,
 * o el primero que no sea el del arranque (disco_c). La ventana es un hueco del
 * servidor y sus puntos, una memoria compartida (/dev/shm/hv-PID-N).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <signal.h>
#include <spawn.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include "so.h"
#include "texto.h"
#include "hv.h"

extern char **environ;
static size_t tope, usado;

void *so_pide(size_t n)
{
	size_t *p;

	if (tope && usado + n > tope)
		return NULL;
	p = malloc(n + 16);
	if (!p)
		return NULL;
	p[0] = n;
	usado += n;
	return (u8 *)p + 16;
}

void so_suelta(void *q)
{
	size_t *p;

	if (!q)
		return;
	p = (size_t *)((u8 *)q - 16);
	usado -= p[0];
	free(p);
}

void *so_repide(void *q, size_t n)
{
	size_t *p, antes;

	if (!q)
		return so_pide(n);
	p = (size_t *)((u8 *)q - 16);
	antes = p[0];
	if (tope && usado - antes + n > tope)
		return NULL;
	p = realloc(p, n + 16);
	if (!p)
		return NULL;
	usado = usado - antes + n;
	p[0] = n;
	return (u8 *)p + 16;
}

u32 so_memoria_usada(void)
{
	return (u32)usado;
}

/* ---- las rutas: «C:\x\y» -> /DISCO/x/y ---- */
static char disco[256];

/* el disco de «C:»: $HISPA_C; si no, el primero de «/» con una carpeta
 * DOCUMENT; si no, el primero que no es el del arranque (el que tiene
 * \boot\HISPA64.BIN o \EFI\BOOT); si no, el primero */
static void disco_mira(const char *r, int *nivel, char *d)
{
	char t[400];
	struct stat st;
	int n = 1;

	fmt(t, sizeof t, "%s/DOCUMENT", r);
	if (!stat(t, &st) && S_ISDIR(st.st_mode))
		n = 3;
	else {
		fmt(t, sizeof t, "%s/boot/HISPA64.BIN", r);
		if (stat(t, &st)) {
			fmt(t, sizeof t, "%s/EFI/BOOT", r);
			if (stat(t, &st))
				n = 2;
		}
	}
	if (n > *nivel) {
		*nivel = n;
		copia(d, r, 256);
	}
}

static const char *disco_c(void)
{
	DIR *d;
	struct dirent *e;
	int nivel = 0;

	if (disco[0])
		return disco;
	if (getenv("HISPA_C") && getenv("HISPA_C")[0]) {
		copia(disco, getenv("HISPA_C"), sizeof disco);
		return disco;
	}
	d = opendir("/");
	if (d) {
		while ((e = readdir(d))) {
			char r[300];
			struct stat st;

			if (e->d_name[0] == '.' || !strcmp(e->d_name, "lib") || !strcmp(e->d_name, "usr") || !strcmp(e->d_name, "bin"))
				continue;
			fmt(r, sizeof r, "/%s", e->d_name);
			if (stat(r, &st) || !S_ISDIR(st.st_mode))
				continue;
			disco_mira(r, &nivel, disco);
		}
		closedir(d);
	}
	if (!disco[0])
		copia(disco, "/", sizeof disco);
	return disco;
}

static void ruta_hispa(const char *r, char *d)
{
	u32 i;

	if (((r[0] >= 'A' && r[0] <= 'Z') || (r[0] >= 'a' && r[0] <= 'z')) && r[1] == ':') {
		copia(d, disco_c(), 1024);
		if (r[2] != '\\' && r[2] != '/')
			anade(d, "/", 1024);
		anade(d, r + 2, 1024);
	} else {
		copia(d, r, 1024);
	}
	for (i = 0; d[i]; i++)
		if (d[i] == '\\')
			d[i] = '/';
	/* (sin barra al final, salvo la raíz) */
	i = largo(d);
	while (i > 1 && d[i - 1] == '/')
		d[--i] = 0;
}

u8 *so_lee_archivo(const char *ruta, u32 *largo_, u32 max, const char **error)
{
	char r[1024];
	FILE *f;
	long n;
	u8 *d;

	ruta_hispa(ruta, r);
	f = fopen(r, "rb");
	if (!f) {
		if (error)
			*error = "no está";
		return NULL;
	}
	fseek(f, 0, SEEK_END);
	n = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (n < 0 || (u32)n > max) {
		fclose(f);
		if (error)
			*error = n < 0 ? "no se puede leer" : "es demasiado grande";
		return NULL;
	}
	d = so_pide((size_t)n + 1);
	if (!d) {
		fclose(f);
		if (error)
			*error = "no hay memoria para leerlo";
		return NULL;
	}
	if (n && fread(d, 1, (size_t)n, f) != (size_t)n) {
		fclose(f);
		so_suelta(d);
		if (error)
			*error = "no se puede leer";
		return NULL;
	}
	fclose(f);
	d[n] = 0;
	*largo_ = (u32)n;
	return d;
}

void *so_crea(const char *ruta, const char **error)
{
	char r[1024];
	FILE *f;

	ruta_hispa(ruta, r);
	f = fopen(r, "wb");
	if (!f && error)
		*error = errno == EROFS ? "el disco es de solo leer" : errno == ENOSPC ? "el disco está lleno" : "no se puede crear";
	return f;
}

int so_escribe(void *f, const void *d, u32 n)
{
	return n && fwrite(d, 1, n, f) != n ? -1 : 0;
}

int so_cierra(void *f)
{
	int r = fflush(f) ? -1 : 0;

	if (fsync(fileno(f)))
		r = r ? r : 0;
	return fclose(f) ? -1 : r;
}

int so_borra(const char *ruta)
{
	char r[1024];

	ruta_hispa(ruta, r);
	return remove(r) ? -1 : 0;
}

int so_renombra(const char *de, const char *a)
{
	char r[1024], s[1024];

	ruta_hispa(de, r);
	ruta_hispa(a, s);
	return rename(r, s) ? -1 : 0;
}

int so_existe(const char *ruta)
{
	char r[1024];
	struct stat st;

	ruta_hispa(ruta, r);
	if (stat(r, &st))
		return 0;
	return S_ISDIR(st.st_mode) ? 2 : 1;
}

typedef struct {
	DIR *d;
	char ruta[1024];
} hcarpeta;

void *so_abre_carpeta(const char *ruta)
{
	hcarpeta *c = so_pide(sizeof *c);

	if (!c)
		return NULL;
	ruta_hispa(ruta, c->ruta);
	if (!c->ruta[0])
		copia(c->ruta, "/", sizeof c->ruta);
	c->d = opendir(c->ruta);
	if (!c->d) {
		so_suelta(c);
		return NULL;
	}
	return c;
}

int so_lee_carpeta(void *cc, so_entrada *e)
{
	hcarpeta *c = cc;
	struct dirent *de;

	while ((de = readdir(c->d))) {
		char r[1300];
		struct stat st;

		if (de->d_name[0] == '.')
			continue;
		fmt(r, sizeof r, "%s/%s", strcmp(c->ruta, "/") ? c->ruta : "", de->d_name);
		if (stat(r, &st))
			continue;
		copia(e->nombre, de->d_name, sizeof e->nombre);
		copia(e->corto, de->d_name, sizeof e->corto);
		e->tamano = (u32)st.st_size;
		e->carpeta = S_ISDIR(st.st_mode);
		return 1;
	}
	return 0;
}

void so_cierra_carpeta(void *cc)
{
	hcarpeta *c = cc;

	if (!c)
		return;
	closedir(c->d);
	so_suelta(c);
}

int so_nombres_largos(void)
{
	return 1;
}

char so_unidad_sistema(void)
{
	return 'C';
}

/* ---- lo demás ---- */
void so_dice(const char *t)
{
	fputs(t, stderr);
}

static char args_linea[4096];

int so_argumentos(char *d, int cabe)
{
	copia(d, args_linea, (u32)cabe);
	return (int)largo(d);
}

u32 so_tics(void)
{
	struct timespec t;

	clock_gettime(CLOCK_MONOTONIC, &t);
	return (u32)(t.tv_sec * 1000 + t.tv_nsec / 1000000);
}

u32 so_hz(void)
{
	return 1000;
}

void so_duerme(u32 t)
{
	struct timespec d;

	d.tv_sec = t / 1000;
	d.tv_nsec = (long)(t % 1000) * 1000000;
	nanosleep(&d, NULL);
}

void so_ahora(int *ano, int *mes, int *dia, int *hora, int *min, int *seg)
{
	time_t t = time(NULL);
	struct tm m;

	localtime_r(&t, &m);	/* la hora local (la zona, de TZ y /etc/localtime), la que ve quien la usa */
	*ano = m.tm_year + 1900;
	*mes = m.tm_mon + 1;
	*dia = m.tm_mday;
	*hora = m.tm_hour;
	*min = m.tm_min;
	*seg = m.tm_sec;
}

double so_raiz(double x) { return sqrt(x); }
double so_suelo(double x) { return floor(x); }
double so_techo(double x) { return ceil(x); }
double so_abs(double x) { return fabs(x); }

/* ---- las ventanas (el servidor VENTANAS) ---- */
#define MAX_V 8
static hv_control *ctl;
static struct {
	hv_ventana *h;		/* su hueco */
	u32 *px;		/* sus puntos (la memoria compartida) */
	int an, al;
	size_t largo;
	char buf[48];
} V[MAX_V + 1];
static int n_bufs;

static int conecta(void)
{
	int fd;

	if (ctl)
		return 1;
	fd = shm_open(HV_NOMBRE, O_RDWR, 0);
	if (fd < 0)
		return 0;
	ctl = mmap(0, sizeof(hv_control), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	close(fd);
	if (ctl == MAP_FAILED || ctl->magia != HV_MAGIA) {
		ctl = NULL;
		return 0;
	}
	return 1;
}

static void avisa(void)
{
	__sync_fetch_and_add(&ctl->aviso, 1);
	hv_despierta(&ctl->aviso);
}

static void pide(int v, u32 p)
{
	if (v < 1 || v > MAX_V || !V[v].h)
		return;
	__sync_fetch_and_or(&V[v].h->pide, p);
	avisa();
}

/* un búfer de an x al para la ventana v (el de antes, fuera) */
static int pon_bufer(int v, int an, int al)
{
	char nombre[48];
	size_t l = (size_t)an * al * 4;
	int fd;
	u32 *p;

	fmt(nombre, sizeof nombre, "/hv-%d-%d", (int)getpid(), ++n_bufs);
	fd = shm_open(nombre, O_CREAT | O_EXCL | O_RDWR, 0600);
	if (fd < 0)
		return 0;
	if (ftruncate(fd, (off_t)l)) {
		close(fd);
		shm_unlink(nombre);
		return 0;
	}
	p = mmap(0, l, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	close(fd);
	if (p == MAP_FAILED) {
		shm_unlink(nombre);
		return 0;
	}
	/* (lo de dentro, del color de las ventanas mientras el programa pinta) */
	{
		size_t i;

		for (i = 0; i < (size_t)an * al; i++)
			p[i] = 0xF4F4F4;
	}
	if (V[v].px) {
		munmap(V[v].px, V[v].largo);
		shm_unlink(V[v].buf);
	}
	V[v].px = p;
	V[v].an = an;
	V[v].al = al;
	V[v].largo = l;
	copia(V[v].buf, nombre, sizeof V[v].buf);
	copia(V[v].h->buf, nombre, sizeof V[v].h->buf);
	V[v].h->an = an;
	V[v].h->al = al;
	__sync_fetch_and_add(&V[v].h->gen, 1);
	return 1;
}

int so_ventana(const char *titulo, int an, int al)
{
	int v, i;

	if (!conecta()) {
		fputs("(no está el servidor de ventanas: arránquelo con VENTANAS)\n", stderr);
		return 0;
	}
	for (v = 1; v <= MAX_V && V[v].h; v++)
		;
	if (v > MAX_V)
		return 0;
	if (an < 120)
		an = 120;
	if (al < 80)
		al = 80;
	if (an > ctl->libre_an)
		an = ctl->libre_an;
	if (al > ctl->libre_al - 30)
		al = ctl->libre_al - 30;
	for (i = 0; i < HV_MAX; i++)
		if (__sync_bool_compare_and_swap(&ctl->v[i].estado, HV_LIBRE, HV_TOMADA))
			break;
	if (i >= HV_MAX)
		return 0;
	V[v].h = &ctl->v[i];
	memset((void *)&V[v].h->pide, 0, sizeof(hv_ventana) - offsetof(hv_ventana, pide));
	V[v].h->pid = getpid();
	copia(V[v].h->titulo, titulo, sizeof V[v].h->titulo);
	if (!pon_bufer(v, an, al)) {
		V[v].h->estado = HV_LIBRE;
		V[v].h = NULL;
		return 0;
	}
	V[v].h->sx0 = 0;
	V[v].h->sy0 = 0;
	V[v].h->sx1 = an;
	V[v].h->sy1 = al;
	__sync_synchronize();
	V[v].h->estado = HV_LISTA;
	avisa();
	return v;
}

void so_titulo(int v, const char *t)
{
	if (v < 1 || v > MAX_V || !V[v].h)
		return;
	copia(V[v].h->titulo, t, sizeof V[v].h->titulo);
	pide(v, HV_P_TITULO);
}

void so_maximiza(int v)
{
	pide(v, HV_P_MAXIMIZA);
}

void so_centra(int v)
{
	pide(v, HV_P_CENTRA);
}

void so_cierra_ventana(int v)
{
	if (v < 1 || v > MAX_V || !V[v].h)
		return;
	pide(v, HV_P_CIERRA);
	if (V[v].px) {
		munmap(V[v].px, V[v].largo);
		shm_unlink(V[v].buf);
	}
	memset(&V[v], 0, sizeof V[v]);
}

void so_pantalla(int *an, int *al)
{
	if (conecta()) {
		*an = ctl->libre_an;
		*al = ctl->libre_al;
	} else {
		*an = 1024;
		*al = 700;
	}
}

void so_pantalla_entera(int *an, int *al)
{
	if (conecta()) {
		*an = ctl->pant_an;
		*al = ctl->pant_al;
	} else {
		*an = 1024;
		*al = 768;
	}
}

void so_mapa(int v, int x, int y, int an, int al, const u32 *px, int paso)
{
	hv_ventana *h;
	int f;

	if (v < 1 || v > MAX_V || !V[v].h || !V[v].px)
		return;
	h = V[v].h;
	/* (recortado al búfer) */
	if (x < 0) {
		px -= x;
		an += x;
		x = 0;
	}
	if (y < 0) {
		px -= (long)y * paso;
		al += y;
		y = 0;
	}
	if (x + an > V[v].an)
		an = V[v].an - x;
	if (y + al > V[v].al)
		al = V[v].al - y;
	if (an <= 0 || al <= 0)
		return;
	for (f = 0; f < al; f++)
		memcpy(V[v].px + (size_t)(y + f) * V[v].an + x, px + (size_t)f * paso, (size_t)an * 4);
	hv_toma(&h->s_cerrojo);
	if (h->sx1 <= h->sx0 || h->sy1 <= h->sy0) {
		h->sx0 = x;
		h->sy0 = y;
		h->sx1 = x + an;
		h->sy1 = y + al;
	} else {
		if (x < h->sx0) h->sx0 = x;
		if (y < h->sy0) h->sy0 = y;
		if (x + an > h->sx1) h->sx1 = x + an;
		if (y + al > h->sy1) h->sy1 = y + al;
	}
	hv_suelta(&h->s_cerrojo);
	pide(v, HV_P_SUCIO);
}

int so_lee_suceso(int v, so_suceso *s, int espera)
{
	hv_ventana *h;
	u32 visto;

	memset(s, 0, sizeof *s);
	if (v < 1 || v > MAX_V || !V[v].h)
		return 0;
	h = V[v].h;
	for (;;) {
		visto = h->ev_aviso;
		if (h->ev_toma != h->ev_pon)
			break;
		if (!espera)
			return 0;
		/* (si el servidor ya no está, se cierra) */
		if (ctl->servidor > 0 && kill(ctl->servidor, 0) && errno == ESRCH) {
			s->clase = SO_SALE;
			return 1;
		}
		hv_espera(&h->ev_aviso, visto, 1000);
	}
	__sync_synchronize();
	memcpy(s, (const void *)&h->ev[h->ev_toma % HV_COLA], sizeof *s);
	__sync_synchronize();
	h->ev_toma++;
	/* el tamaño nuevo: un búfer de ese tamaño antes de que el programa pinte */
	if (s->clase == SO_TAMANO && (s->x != V[v].an || s->y != V[v].al) && s->x > 0 && s->y > 0) {
		if (pon_bufer(v, s->x, s->y))
			pide(v, HV_P_BUFER);
	}
	return 1;
}

void so_menu(int v, int x, int y, const char *opciones, u32 grises, u32 etiqueta)
{
	hv_ventana *h;

	if (v < 1 || v > MAX_V || !V[v].h)
		return;
	h = V[v].h;
	h->menu_x = x;
	h->menu_y = y;
	h->menu_grises = grises;
	h->menu_etiqueta = etiqueta;
	copia(h->menu, opciones, sizeof h->menu);
	pide(v, HV_P_MENU);
}

int so_control(void)
{
	return ctl ? (int)ctl->control : 0;
}

void so_al_portapapeles(const char *t, u32 n)
{
	if (!conecta())
		return;
	if (n >= HV_PAPEL)
		n = HV_PAPEL - 1;
	hv_toma(&ctl->p_cerrojo);
	memcpy(ctl->papel, t, n);
	ctl->papel[n] = 0;
	ctl->p_n = n;
	hv_suelta(&ctl->p_cerrojo);
}

char *so_del_portapapeles(u32 *n)
{
	char *d;

	if (!conecta() || !ctl->p_n)
		return NULL;
	hv_toma(&ctl->p_cerrojo);
	d = so_pide(ctl->p_n + 1);
	if (d) {
		memcpy(d, ctl->papel, ctl->p_n);
		d[ctl->p_n] = 0;
		*n = ctl->p_n;
	}
	hv_suelta(&ctl->p_cerrojo);
	return d;
}

int so_fondo_escritorio(const u32 *px, int an, int al)
{
	(void)px;
	(void)an;
	(void)al;
	return -1;
}

void so_puntero(int v, int forma)
{
	if (v < 1 || v > MAX_V || !V[v].h || V[v].h->forma == forma)
		return;
	V[v].h->forma = forma;
	pide(v, HV_P_PUNTERO);
}

void so_puntero_zona(int v, int x, int y, int an, int al, int forma)
{
	hv_ventana *h;

	if (v < 1 || v > MAX_V || !V[v].h)
		return;
	h = V[v].h;
	if (an <= 0) {
		h->n_zonas = 0;
	} else if (h->n_zonas < 16) {
		h->zona[h->n_zonas][0] = x;
		h->zona[h->n_zonas][1] = y;
		h->zona[h->n_zonas][2] = an;
		h->zona[h->n_zonas][3] = al;
		h->zona[h->n_zonas][4] = forma;
		h->n_zonas++;
	}
	pide(v, HV_P_PUNTERO);
}

/* el programa (PLUMA.BIN, \HISPA\PLUMA.BIN, /PLUMA...): /PLUMA */
static void ruta_programa(const char *p, char *d, u32 cabe)
{
	const char *n = p, *q;
	u32 i = 1;

	for (q = p; *q; q++)
		if (*q == '\\' || *q == '/')
			n = q + 1;
	d[0] = '/';
	while (*n && *n != '.' && i + 1 < cabe) {
		d[i++] = (*n >= 'a' && *n <= 'z') ? (char)(*n - 32) : *n;
		n++;
	}
	d[i] = 0;
}

int so_lanza(const char *programa, const char *args)
{
	char r[300], a[1024], *argv[16];
	int n = 0;
	char *s;
	pid_t h;

	ruta_programa(programa, r, sizeof r);
	copia(a, args ? args : "", sizeof a);
	argv[n++] = r;
	for (s = strtok(a, " "); s && n < 15; s = strtok(NULL, " "))
		argv[n++] = s;
	argv[n] = NULL;
	if (posix_spawn(&h, r, NULL, NULL, argv, environ))
		return -1;
	return 0;
}

static char nombre_programa[300];

void so_programa(char *d, u32 cabe)
{
	copia(d, nombre_programa, cabe);
}

static void al_salir(void)
{
	int v;

	for (v = 1; v <= MAX_V; v++)
		if (V[v].h)
			so_cierra_ventana(v);
}

int main(int argc, char **argv)
{
	int i, r;

	/* (los hijos que lance, sin zombis) */
	signal(SIGCHLD, SIG_IGN);
	copia(nombre_programa, argv[0], sizeof nombre_programa);
	args_linea[0] = 0;
	for (i = 1; i < argc; i++) {
		int esp = strchr(argv[i], ' ') != NULL;

		if (i > 1)
			anade(args_linea, " ", sizeof args_linea);
		if (esp)
			anade(args_linea, "\"", sizeof args_linea);
		anade(args_linea, argv[i], sizeof args_linea);
		if (esp)
			anade(args_linea, "\"", sizeof args_linea);
	}
	atexit(al_salir);
	r = ofi_principal(args_linea);
	return r;
}
