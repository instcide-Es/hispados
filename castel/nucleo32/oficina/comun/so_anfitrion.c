/* so_anfitrion.c - la oficina de 32 bits en Linux (para las pruebas, con
 * ASan y UBSan; y como modelo para Hispa-64): lo de so.h con la biblioteca
 * de C de siempre. La ventana es de mentira: un lienzo en la memoria que se
 * maneja con un guion de sucesos (OFI_GUION=archivo; una orden por línea) y
 * deja capturas .ppm (en OFI_CAPTURAS, una carpeta):
 *
 *   escribe texto            las letras, una a una
 *   tecla ret|tab|esc|retro|supr|arriba|abajo|izq|der|inicio|fin|repag|avpag|f1..f12
 *                            (con «mayus-» o «ctrl-» delante; «ctrl-n» es Ctrl+N)
 *   clic X Y | doble X Y | triple X Y | clicd X Y | mayusclic X Y | ctrlclic X Y
 *   arrastra X1 Y1 X2 Y2 [der] [ctrl] [mayus] [pasos N]
 *                            pulsado de un sitio a otro (el izquierdo, o el
 *                            derecho con «der»; en 8 pasos, o en N)
 *   traza X1 Y1 X2 Y2 ... [der]   pulsado por esos puntos (de uno a otro, en 4 pasos)
 *   mueve X Y                el ratón, sin pulsar
 *   rueda N X Y [ctrl]       N pasos (hacia arriba, positivos)
 *   elige N                  la opción N del último menú pedido (-1: ninguna)
 *   tamano AN AL             la ventana cambia de tamaño
 *   captura NOMBRE           la ventana, a OFI_CAPTURAS/NOMBRE.ppm
 *   marca TEXTO              lo dice (para saber por dónde va)
 *   cerrar                   el botón de cerrar
 *
 * Al acabarse el guion, cerrar (tres veces lo más; luego se acaba).
 * OFI_MEMORIA=n (MB): lo más que se deja pedir. OFI_83=1: como Hispa-Pentium,
 * sin nombres largos al crear.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include "so.h"
#include "texto.h"

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

/* ---- los archivos: las barras de DOS valen como las de Linux ---- */
static void ruta_linux(const char *r, char *d)
{
	u32 i;

	copia(d, r, 1024);
	for (i = 0; d[i]; i++)
		if (d[i] == '\\')
			d[i] = '/';
}

u8 *so_lee_archivo(const char *ruta, u32 *largo_, u32 max, const char **error)
{
	char r[1024];
	FILE *f;
	long n;
	u8 *d;

	ruta_linux(ruta, r);
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

	ruta_linux(ruta, r);
	if (!so_nombres_largos()) {
		/* (como Hispa-Pentium: 8+3) */
		const char *n = sin_carpeta(r), *p = strchr(n, '.');
		u32 l = largo(n);

		if ((p ? (u32)(p - n) : l) > 8 || (p && largo(p + 1) > 3) || (p && strchr(p + 1, '.'))) {
			if (error)
				*error = "el nombre no vale (8+3)";
			return NULL;
		}
	}
	f = fopen(r, "wb");
	if (!f && error)
		*error = "no se puede crear";
	return f;
}

int so_escribe(void *f, const void *d, u32 n)
{
	return n && fwrite(d, 1, n, f) != n ? -1 : 0;
}

int so_cierra(void *f)
{
	return fclose(f) ? -1 : 0;
}

int so_borra(const char *ruta)
{
	char r[1024];

	ruta_linux(ruta, r);
	return remove(r) ? -1 : 0;
}

int so_renombra(const char *de, const char *a)
{
	char r[1024], s[1024];

	ruta_linux(de, r);
	ruta_linux(a, s);
	return rename(r, s) ? -1 : 0;
}

int so_existe(const char *ruta)
{
	char r[1024];
	struct stat st;

	ruta_linux(ruta, r);
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
	ruta_linux(ruta, c->ruta);
	if (!c->ruta[0])
		copia(c->ruta, ".", sizeof c->ruta);
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
		fmt(r, sizeof r, "%s/%s", c->ruta, de->d_name);
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
	const char *e = getenv("OFI_83");

	return !(e && *e == '1');
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
	(void)t;
}

void so_ahora(int *ano, int *mes, int *dia, int *hora, int *min, int *seg)
{
	time_t t = getenv("OFI_FECHA_FIJA") ? (time_t)1795339200 : time(NULL);	/* (22/11/2026 a mediodía, para las pruebas) */
	struct tm m;

	gmtime_r(&t, &m);
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

/* ---- la ventana de mentira ---- */
static u32 *lienzo;
static int lan, lal;
static FILE *guion;
static int cerrados;
static u32 etiqueta_menu;
static int hay_menu;
static int mods_ahora;
/* los sucesos de una orden del guion, en cola */
static so_suceso cola[512];
static int c_ini, c_fin;
static char *papel;
static u32 n_papel;

int so_ventana(const char *titulo, int an, int al)
{
	const char *t = getenv("OFI_VENTANA");

	if (t) {
		an = atoi(t);
		t = strchr(t, 'x');
		if (t)
			al = atoi(t + 1);
	}
	if (an < 200)
		an = 200;
	if (al < 140)
		al = 140;
	lan = an;
	lal = al;
	free(lienzo);
	lienzo = calloc((size_t)an * al, 4);
	if (!lienzo)
		return 0;
	if (getenv("OFI_GUION"))
		guion = fopen(getenv("OFI_GUION"), "r");
	fprintf(stderr, "ventana: «%s» %dx%d\n", titulo, an, al);
	return 1;
}

void so_titulo(int v, const char *t)
{
	(void)v;
	fprintf(stderr, "título: %s\n", t);
}

static void mete(so_suceso s);

void so_maximiza(int v)
{
	so_suceso s;

	(void)v;
	memset(&s, 0, sizeof s);
	s.clase = SO_TAMANO;
	s.x = lan;
	s.y = lal;
	mete(s);
}
void so_centra(int v) { (void)v; }
void so_cierra_ventana(int v) { (void)v; }

void so_pantalla(int *an, int *al)
{
	*an = 1024;
	*al = 700;
}

void so_mapa(int v, int x, int y, int an, int al, const u32 *px, int paso)
{
	int f, c;

	(void)v;
	for (f = 0; f < al; f++) {
		if (y + f < 0 || y + f >= lal)
			continue;
		for (c = 0; c < an; c++)
			if (x + c >= 0 && x + c < lan)
				lienzo[(y + f) * lan + x + c] = px[f * paso + c] & 0xFFFFFF;
	}
}

static void captura(const char *nombre)
{
	char r[1024];
	const char *d = getenv("OFI_CAPTURAS");
	FILE *f;
	int i;

	fmt(r, sizeof r, "%s/%s.ppm", d ? d : "/tmp", nombre);
	f = fopen(r, "wb");
	if (!f)
		return;
	fprintf(f, "P6\n%d %d\n255\n", lan, lal);
	for (i = 0; i < lan * lal; i++) {
		u8 p[3] = { (u8)(lienzo[i] >> 16), (u8)(lienzo[i] >> 8), (u8)lienzo[i] };

		fwrite(p, 1, 3, f);
	}
	fclose(f);
	fprintf(stderr, "captura: %s\n", r);
}

static void mete(so_suceso s)
{
	if ((c_fin + 1) % 512 == c_ini)
		return;
	cola[c_fin] = s;
	c_fin = (c_fin + 1) % 512;
}

static void mete_tecla(u32 t, int mods)
{
	so_suceso s;

	memset(&s, 0, sizeof s);
	s.clase = SO_TECLA;
	s.tecla = t;
	s.mods = mods;
	mete(s);
}

static void mete_raton(int clase, int x, int y, int botones, int mods, int veces)
{
	so_suceso s;

	memset(&s, 0, sizeof s);
	s.clase = clase;
	s.x = x;
	s.y = y;
	s.botones = botones;
	s.mods = mods;
	s.veces = veces;
	mete(s);
}

static u32 nombre_tecla(const char *n, int *mods)
{
	static const struct { const char *n; u32 t; } tt[] = {
		{ "ret", SO_INTRO }, { "tab", SO_TAB }, { "esc", SO_ESC }, { "retro", SO_RETROCESO },
		{ "supr", SO_T_SUPR }, { "arriba", SO_T_ARRIBA }, { "abajo", SO_T_ABAJO }, { "izq", SO_T_IZQUIERDA },
		{ "der", SO_T_DERECHA }, { "inicio", SO_T_INICIO }, { "fin", SO_T_FIN }, { "repag", SO_T_RE_PAG },
		{ "avpag", SO_T_AV_PAG }, { "ins", SO_T_INSERTAR }, { "espacio", ' ' },
	};
	u32 i;

	*mods = 0;
	for (;;) {
		if (empieza(n, "mayus-")) {
			*mods |= SO_CON_MAYUS;
			n += 6;
		} else if (empieza(n, "ctrl-")) {
			*mods |= SO_CON_CONTROL;
			n += 5;
		} else if (empieza(n, "alt-")) {
			n += 4;
			return SO_T_ALT + (u32)(n[0] - 'a');
		} else
			break;
	}
	for (i = 0; i < sizeof tt / sizeof tt[0]; i++)
		if (iguales(n, tt[i].n))
			return tt[i].t;
	if (n[0] == 'f' && n[1] >= '1' && n[1] <= '9')
		return SO_T_F1 + (u32)(atoi(n + 1) - 1);
	if (n[0] >= 'a' && n[0] <= 'z' && !n[1] && (*mods & SO_CON_CONTROL))
		return (u32)(n[0] - 'a' + 1);
	if (n[0] && !n[1])
		return (u8)n[0];
	return 0;
}

/* la orden siguiente del guion, a la cola; 0 si se ha acabado */
static int lee_guion(void)
{
	char l[2048];
	int x, y, x2, y2, n;

	while (guion && fgets(l, sizeof l, guion)) {
		char *p = l + strlen(l);

		while (p > l && (p[-1] == '\n' || p[-1] == '\r'))
			*--p = 0;
		if (!l[0] || l[0] == '#')
			continue;
		if (empieza(l, "escribe ")) {
			u32 i = 8, k = largo(l), c;

			while (i < k) {
				i += (u32)utf8_lee((u8 *)l + i, k - i, &c);
				mete_tecla(c, 0);
			}
			return 1;
		}
		if (empieza(l, "tecla ")) {
			char *t = strtok(l + 6, " ");

			while (t) {
				int m;
				u32 k = nombre_tecla(t, &m);

				if (k)
					mete_tecla(k, m);
				t = strtok(NULL, " ");
			}
			return 1;
		}
		if (sscanf(l, "clic %d %d", &x, &y) == 2 || sscanf(l, "mayusclic %d %d", &x, &y) == 2 ||
		    sscanf(l, "ctrlclic %d %d", &x, &y) == 2) {
			int m = l[0] == 'm' ? SO_CON_MAYUS : l[0] == 'c' && l[1] == 't' ? SO_CON_CONTROL : 0;

			mete_raton(SO_MUEVE, x, y, 0, m, 1);
			mete_raton(SO_BAJA, x, y, SO_BOTON_IZQ, m, 1);
			mete_raton(SO_SUBE, x, y, 0, m, 1);
			return 1;
		}
		if (sscanf(l, "doble %d %d", &x, &y) == 2 || sscanf(l, "triple %d %d", &x, &y) == 2) {
			int v = l[0] == 'd' ? 2 : 3, i;

			for (i = 1; i <= v; i++) {
				mete_raton(SO_BAJA, x, y, SO_BOTON_IZQ, 0, i);
				mete_raton(SO_SUBE, x, y, 0, 0, i);
			}
			return 1;
		}
		if (sscanf(l, "clicd %d %d", &x, &y) == 2) {
			mete_raton(SO_BAJA, x, y, SO_BOTON_DER, 0, 1);
			mete_raton(SO_SUBE, x, y, 0, 0, 1);
			return 1;
		}
		if (sscanf(l, "mueve %d %d", &x, &y) == 2) {
			mete_raton(SO_MUEVE, x, y, 0, 0, 1);
			return 1;
		}
		if (sscanf(l, "arrastra %d %d %d %d", &x, &y, &x2, &y2) == 4) {
			int i, b = strstr(l, " der") ? SO_BOTON_DER : SO_BOTON_IZQ, pasos = 8;
			int m = (strstr(l, " ctrl") ? SO_CON_CONTROL : 0) | (strstr(l, " mayus") ? SO_CON_MAYUS : 0);
			const char *ps = strstr(l, " pasos ");

			if (ps)
				pasos = atoi(ps + 7);
			if (pasos < 1 || pasos > 400)
				pasos = 8;
			mete_raton(SO_MUEVE, x, y, 0, m, 1);
			mete_raton(SO_BAJA, x, y, b, m, 1);
			for (i = 1; i <= pasos; i++)
				mete_raton(SO_MUEVE, x + (x2 - x) * i / pasos, y + (y2 - y) * i / pasos, b, m, 1);
			mete_raton(SO_SUBE, x2, y2, 0, m, 1);
			return 1;
		}
		if (empieza(l, "traza ")) {
			int px[128], k = 0, i, j, b = strstr(l, " der") ? SO_BOTON_DER : SO_BOTON_IZQ;
			char *t = strtok(l + 6, " ");

			while (t && k < 128) {
				if ((t[0] >= '0' && t[0] <= '9') || t[0] == '-')
					px[k++] = atoi(t);
				t = strtok(NULL, " ");
			}
			k &= ~1;
			if (k < 2)
				continue;
			mete_raton(SO_MUEVE, px[0], px[1], 0, 0, 1);
			mete_raton(SO_BAJA, px[0], px[1], b, 0, 1);
			for (i = 2; i < k; i += 2)
				for (j = 1; j <= 4; j++)
					mete_raton(SO_MUEVE, px[i - 2] + (px[i] - px[i - 2]) * j / 4, px[i - 1] + (px[i + 1] - px[i - 1]) * j / 4, b, 0, 1);
			mete_raton(SO_SUBE, px[k - 2], px[k - 1], 0, 0, 1);
			return 1;
		}
		if (sscanf(l, "rueda %d %d %d", &n, &x, &y) == 3) {
			so_suceso s;

			memset(&s, 0, sizeof s);
			s.clase = SO_RUEDA;
			s.x = x;
			s.y = y;
			s.pasos = n;
			s.mods = strstr(l, "ctrl") ? SO_CON_CONTROL : 0;
			mete(s);
			return 1;
		}
		if (sscanf(l, "elige %d", &n) == 1) {
			so_suceso s;

			memset(&s, 0, sizeof s);
			s.clase = SO_MENU;
			s.opcion = n;
			s.etiqueta = etiqueta_menu;
			if (!hay_menu)
				fprintf(stderr, "guion: «elige» sin menú abierto\n");
			hay_menu = 0;
			mete(s);
			return 1;
		}
		if (sscanf(l, "tamano %d %d", &x, &y) == 2) {
			so_suceso s;
			u32 *nl = calloc((size_t)x * y, 4);

			if (!nl)
				continue;
			free(lienzo);
			lienzo = nl;
			lan = x;
			lal = y;
			memset(&s, 0, sizeof s);
			s.clase = SO_TAMANO;
			s.x = x;
			s.y = y;
			mete(s);
			return 1;
		}
		if (empieza(l, "captura ")) {
			captura(l + 8);
			continue;
		}
		if (empieza(l, "marca ")) {
			fprintf(stderr, "marca: %s\n", l + 6);
			continue;
		}
		if (iguales(l, "cerrar")) {
			so_suceso s;

			memset(&s, 0, sizeof s);
			s.clase = SO_CERRAR;
			mete(s);
			return 1;
		}
		fprintf(stderr, "guion: no sé qué es «%s»\n", l);
	}
	return 0;
}

int so_lee_suceso(int v, so_suceso *s, int espera)
{
	(void)v;
	memset(s, 0, sizeof *s);
	/* sin esperar y sin nada en la cola: nada (así el programa pinta antes
	 * de la orden siguiente del guion, que puede ser una captura) */
	if (c_ini == c_fin && !espera)
		return 0;
	if (c_ini == c_fin && !lee_guion()) {
		if (!espera)
			return 0;
		if (++cerrados > 3) {
			fprintf(stderr, "guion: acabado y la ventana no se cierra\n");
			exit(3);
		}
		s->clase = SO_CERRAR;
		return 1;
	}
	if (c_ini == c_fin)
		return 0;
	*s = cola[c_ini];
	c_ini = (c_ini + 1) % 512;
	if (s->clase == SO_TECLA || s->clase == SO_BAJA)
		mods_ahora = s->mods;
	return 1;
}

void so_menu(int v, int x, int y, const char *opciones, u32 grises, u32 etiqueta)
{
	(void)v;
	(void)grises;
	fprintf(stderr, "menú (%d, %d): %s\n", x, y, opciones);
	etiqueta_menu = etiqueta & ~0x40000000u;	/* (como el núcleo: sin la marca de las flechas) */
	hay_menu = 1;
}

int so_control(void)
{
	return (mods_ahora & SO_CON_CONTROL) != 0;
}

void so_al_portapapeles(const char *t, u32 n)
{
	free(papel);
	papel = malloc(n + 1);
	if (!papel)
		return;
	memcpy(papel, t, n);
	papel[n] = 0;
	n_papel = n;
}

char *so_del_portapapeles(u32 *n)
{
	char *d;

	*n = 0;
	if (!papel)
		return NULL;
	d = duplica_n(papel, n_papel);
	if (d)
		*n = n_papel;
	return d;
}

void so_pantalla_entera(int *an, int *al)
{
	so_pantalla(an, al);
}

int so_fondo_escritorio(const u32 *px, int an, int al)
{
	char r[1024];
	const char *d = getenv("OFI_CAPTURAS");
	FILE *f;
	int i;

	if (an < 1 || al < 1 || an > 4096 || al > 4096)
		return -1;
	fmt(r, sizeof r, "%s/fondo.ppm", d ? d : "/tmp");
	f = fopen(r, "wb");
	if (!f)
		return -1;
	fprintf(f, "P6\n%d %d\n255\n", an, al);
	for (i = 0; i < an * al; i++) {
		u8 p[3] = { (u8)(px[i] >> 16), (u8)(px[i] >> 8), (u8)px[i] };

		fwrite(p, 1, 3, f);
	}
	fclose(f);
	fprintf(stderr, "fondo: %dx%d en %s\n", an, al, r);
	return 0;
}

int so_lanza(const char *programa, const char *args)
{
	fprintf(stderr, "lanza: %s %s\n", programa, args ? args : "");
	return 0;
}

static char nombre_programa[1024];

void so_programa(char *d, u32 cabe)
{
	copia(d, nombre_programa, cabe);
}

int main(int argc, char **argv)
{
	int i, r;
	const char *m = getenv("OFI_MEMORIA");

	if (m)
		tope = (size_t)atoi(m) << 20;
	copia(nombre_programa, argv[0], sizeof nombre_programa);
	args_linea[0] = 0;
	for (i = 1; i < argc; i++) {
		/* (con comillas si llevan espacios) */
		int esp = strchr(argv[i], ' ') != NULL;

		if (i > 1)
			anade(args_linea, " ", sizeof args_linea);
		if (esp)
			anade(args_linea, "\"", sizeof args_linea);
		anade(args_linea, argv[i], sizeof args_linea);
		if (esp)
			anade(args_linea, "\"", sizeof args_linea);
	}
	r = ofi_principal(args_linea);
	free(lienzo);
	free(papel);
	if (guion)
		fclose(guion);
	return r;
}
