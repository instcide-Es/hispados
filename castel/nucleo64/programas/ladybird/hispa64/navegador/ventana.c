/* Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * hispa64/navegador/ventana.c - Hispa-64, etapa 10 h (docs/64/HISPA-64.md,
 * sección 25): ventana.h, sobre la capa «so» de la oficina para Hispa-64
 * (so_hispa64.c, que va aquí dentro, con su main con otro nombre: el de
 * NAVEGADOR es el de LibMain de Ladybird) y el lienzo de la oficina.
 * [etapa 10 i] (sección 26) y los menús (so_menu) y los cuadros de diálogo de
 * la oficina (ui.c): los avisos de las páginas (alert, confirm, prompt,
 * onbeforeunload) y el selector de archivos para subir uno. */
#define main so_hispa64_main_sin_usar
#include "so_hispa64.c"
#undef main
#include <dirent.h>
#include <sys/stat.h>
#include "lienzo.h"
#include "letras.h"
#include "texto.h"
#include "ui.h"
#include "ventana.h"

/* (so_hispa64.c llama a ofi_principal desde su main, que aquí no se usa) */
int ofi_principal(const char *args)
{
	(void)args;
	return 0;
}

int nv_hay_escritorio(void)
{
	return conecta();
}

void nv_pantalla(int *an, int *al)
{
	so_pantalla(an, al);
}

int nv_abre(const char *titulo, int an, int al)
{
	return so_ventana(titulo, an, al);
}

void nv_titulo(int v, const char *t)
{
	so_titulo(v, t);
}

void nv_maximiza(int v)
{
	so_maximiza(v);
}

void nv_cierra(int v)
{
	so_cierra_ventana(v);
}

void nv_mapa(int v, int x, int y, int an, int al, const uint32_t *px, int paso)
{
	so_mapa(v, x, y, an, al, px, paso);
}

int nv_suceso_de(int v, nv_suceso *s)
{
	return so_lee_suceso(v, (so_suceso *)s, 0);
}

/* (de otro hilo: solo mira la cola y el futex, no toca el búfer) */
int nv_espera(int v, int ms)
{
	hv_ventana *h;
	u32 visto;

	if (v < 1 || v > MAX_V || !V[v].h)
		return 0;
	h = V[v].h;
	visto = h->ev_aviso;
	__sync_synchronize();
	if (h->ev_toma != h->ev_pon)
		return 1;
	if (ctl->servidor > 0 && kill(ctl->servidor, 0) && errno == ESRCH)
		return 1;
	hv_espera(&h->ev_aviso, visto, ms);
	return h->ev_toma != h->ev_pon;
}

void nv_puntero(int v, int forma)
{
	so_puntero(v, forma);
}

int nv_control(void)
{
	return so_control();
}

void nv_al_portapapeles(const char *t, uint32_t n)
{
	so_al_portapapeles(t, n);
}

char *nv_del_portapapeles(uint32_t *n)
{
	u32 k = 0;
	char *d = so_del_portapapeles(&k);
	char *r;

	if (!d)
		return NULL;
	r = malloc(k + 1);
	if (r) {
		memcpy(r, d, k);
		r[k] = 0;
		*n = k;
	}
	so_suelta(d);
	return r;
}

static int cara_n = -1, cara_b = -1;

void nv_letras(void)
{
	letras_inicia();
	cara_n = letra_cara(FAM_SANS, 0, 0, NULL);
	cara_b = letra_cara(FAM_SANS, 1, 0, NULL);
}

int nv_texto(uint32_t *px, int an, int al, int paso, int x, int y, const char *t, uint32_t c, int negrita, int tam4)
{
	lienzo l;

	memset(&l, 0, sizeof l);
	l.p = px;
	l.an = an;
	l.al = al;
	l.paso = paso;
	lz_sin_recorte(&l);
	return lz_texto(&l, negrita ? cara_b : cara_n, tam4, 0, x, y, t, c);
}

int nv_ancho(const char *t, int negrita, int tam4)
{
	return lz_ancho(negrita ? cara_b : cara_n, tam4, 0, t);
}

/* ---- [etapa 10 i] los menús ---- */
void nv_menu(int v, int x, int y, const char *opciones, uint32_t grises, uint32_t etiqueta)
{
	so_menu(v, x, y, opciones, grises, etiqueta);
}

/* ---- [etapa 10 i] los cuadros de diálogo ---- */
#define B_ACEPTAR 1
#define B_CANCELAR 2
#define ID_CAMPO 10
#define ID_SUBIR 20
#define ID_LISTA 21
#define ID_NOMBRE 22
#define MAX_ENT 600

static ui_control C_[16];
static int nC_;
static ui_cuadro Q_;
static int clase_ = -1;			/* -1: ninguno abierto */
static int resultado_ = -1;
static char titulo_[256];
static char texto_[4096];		/* el mensaje, ya partido en renglones */
static char valor_[1024];
/* el selector de archivos: la carpeta, lo que hay en ella y la lista */
static char carpeta_[1024] = "/";
static char rot_carpeta_[1100];
static char *ent_[MAX_ENT];
static int ent_dir_[MAX_ENT];
static int n_ent_;
static char *ops_;

static ui_control *control_(int tipo, int id, int x, int y, int an, const char *rot)
{
	ui_control *c;

	if (nC_ >= (int)(sizeof C_ / sizeof C_[0]))
		nC_ = (int)(sizeof C_ / sizeof C_[0]) - 1;
	c = &C_[nC_++];
	memset(c, 0, sizeof *c);
	c->tipo = tipo;
	c->id = id;
	c->x = x;
	c->y = y;
	c->an = an;
	c->rotulo = rot;
	return c;
}

/* el texto, en renglones que quepan en «ancho» (por los espacios; los \n se
 * respetan); cuántos renglones */
static int parte_(const char *t, int ancho, char *d, int cabe)
{
	int n = 1, k = 0;
	char linea[1200];
	int ln = 0;
	const char *p = t;

	d[0] = 0;
	while (*p && k < cabe - 8) {
		/* la palabra siguiente (con lo que la separa) */
		const char *q = p;
		char pal[600];
		int lp = 0;

		if (*q == '\n') {
			linea[ln] = 0;
			k += snprintf(d + k, (size_t)(cabe - k), "%s\n", linea);
			ln = 0;
			n++;
			p++;
			continue;
		}
		while (*q && *q != ' ' && *q != '\n' && lp < (int)sizeof pal - 2)
			pal[lp++] = *q++;
		while (*q == ' ')
			q++;
		pal[lp] = 0;
		if (ln > 0) {
			char prueba[1800];

			snprintf(prueba, sizeof prueba, "%s %s", linea, pal);
			if (lz_ui_ancho(prueba) > ancho) {
				linea[ln] = 0;
				k += snprintf(d + k, (size_t)(cabe - k), "%s\n", linea);
				ln = 0;
				n++;
			} else {
				linea[ln++] = ' ';
			}
		}
		if (ln + lp < (int)sizeof linea - 2) {
			memcpy(linea + ln, pal, (size_t)lp);
			ln += lp;
		}
		p = q;
		if (n > 40)
			break;
	}
	linea[ln] = 0;
	snprintf(d + k, (size_t)(cabe - k), "%s", linea);
	return n;
}

static void abre_q_(void)
{
	int i;

	Q_.c = C_;
	Q_.n = nC_;
	Q_.cerrado = -9999;
	Q_.pulsado = -1;
	Q_.menu_de = -1;
	Q_.arrastra = -1;
	Q_.foco = 0;
	for (i = 0; i < Q_.n; i++)
		if (!C_[i].apagado && C_[i].tipo != UC_ROTULO && C_[i].tipo != UC_TEXTO && C_[i].tipo != UC_MUESTRA) {
			Q_.foco = i;
			break;
		}
	for (i = 0; i < Q_.n; i++)
		if (C_[i].tipo == UC_CAMPO) {
			Q_.foco = i;
			C_[i].ancla = 0;
			C_[i].cursor = (int)utf8_cuenta(C_[i].valor, largo(C_[i].valor));
		}
}

static int compara_ent_(const void *a, const void *b)
{
	int i = *(const int *)a, j = *(const int *)b;

	if (ent_dir_[i] != ent_dir_[j])
		return ent_dir_[j] - ent_dir_[i];
	return strcmp(ent_[i], ent_[j]);
}

/* lo que hay en carpeta_ (las carpetas primero), en ent_ y en ops_ */
static void lee_carpeta_(void)
{
	DIR *d;
	struct dirent *e;
	int i, orden[MAX_ENT];
	char *nombres[MAX_ENT];
	int dirs[MAX_ENT];
	size_t k = 0, cabe;

	for (i = 0; i < n_ent_; i++)
		free(ent_[i]);
	n_ent_ = 0;
	d = opendir(carpeta_);
	if (d) {
		while ((e = readdir(d)) && n_ent_ < MAX_ENT) {
			char r[2100];
			struct stat st;

			if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
				continue;
			snprintf(r, sizeof r, "%s%s%s", carpeta_, carpeta_[strlen(carpeta_) - 1] == '/' ? "" : "/", e->d_name);
			ent_dir_[n_ent_] = stat(r, &st) == 0 && S_ISDIR(st.st_mode);
			ent_[n_ent_] = strdup(e->d_name);
			if (ent_[n_ent_])
				n_ent_++;
		}
		closedir(d);
	}
	for (i = 0; i < n_ent_; i++)
		orden[i] = i;
	qsort(orden, (size_t)n_ent_, sizeof orden[0], compara_ent_);
	for (i = 0; i < n_ent_; i++) {
		nombres[i] = ent_[orden[i]];
		dirs[i] = ent_dir_[orden[i]];
	}
	for (i = 0; i < n_ent_; i++) {
		ent_[i] = nombres[i];
		ent_dir_[i] = dirs[i];
	}
	free(ops_);
	cabe = 16;
	for (i = 0; i < n_ent_; i++)
		cabe += strlen(ent_[i]) + 8;
	ops_ = malloc(cabe);
	if (!ops_)
		return;
	ops_[0] = 0;
	for (i = 0; i < n_ent_; i++) {
		const char *s;

		if (i)
			ops_[k++] = '|';
		for (s = ent_[i]; *s; s++)
			ops_[k++] = *s == '|' ? '!' : *s;	/* («|» separa las opciones) */
		if (ent_dir_[i])
			ops_[k++] = '/';
	}
	ops_[k] = 0;
	if (!n_ent_)
		snprintf(ops_, cabe, "(vacía)");
	snprintf(rot_carpeta_, sizeof rot_carpeta_, "Carpeta: %s", carpeta_);
}

static void cuadro_archivo_(void)
{
	int an = 600, H = ui_alto_control(), U = lz_ui_alto(), lh = U * 12 + 8, al;
	ui_control *c;

	nC_ = 0;
	lee_carpeta_();
	al = 12 + H + 8 + lh + 10 + H + 16 + H + 12;
	Q_.titulo = titulo_;
	Q_.an = an;
	Q_.al = al;
	control_(UC_ROTULO, 0, 14, 12, an - 150, rot_carpeta_);
	control_(UC_BOTON, ID_SUBIR, an - 130, 12, 116, "&Subir");
	c = control_(UC_LISTA, ID_LISTA, 14, 12 + H + 8, an - 28, NULL);
	c->al = lh;
	c->opciones = ops_ ? ops_ : "";
	c->elegida = n_ent_ ? 0 : -1;
	control_(UC_ROTULO, 0, 14, 12 + H + 8 + lh + 10, 0, "&Nombre:");
	c = control_(UC_CAMPO, ID_NOMBRE, 100, 12 + H + 8 + lh + 10, an - 114, NULL);
	c->valor[0] = 0;
	c = control_(UC_BOTON, B_ACEPTAR, an - 2 * 120 - 24, al - H - 12, 120, "&Abrir");
	c->principal = 1;
	c = control_(UC_BOTON, B_CANCELAR, an - 120 - 12, al - H - 12, 120, "Cancelar");
	c->cancela = 1;
	abre_q_();
	/* (el foco, en la lista) */
	Q_.foco = 2;
}

void nv_cuadro_abre(int clase, const char *titulo, const char *mensaje, const char *valor, const char *carpeta)
{
	int an = 500, H, U, n, al, ytexto;
	ui_control *c;

	H = ui_alto_control();
	U = lz_ui_alto();
	clase_ = clase;
	resultado_ = -1;
	memset(&Q_, 0, sizeof Q_);
	snprintf(titulo_, sizeof titulo_, "%s", titulo ? titulo : "");
	snprintf(valor_, sizeof valor_, "%s", valor ? valor : "");
	if (clase == NV_C_ARCHIVO) {
		if (carpeta && carpeta[0])
			snprintf(carpeta_, sizeof carpeta_, "%s", carpeta);
		cuadro_archivo_();
		return;
	}
	n = parte_(mensaje ? mensaje : "", an - 40, texto_, sizeof texto_);
	if (n > 18)
		n = 18;
	nC_ = 0;
	ytexto = 16;
	al = ytexto + n * U + 14 + (clase == NV_C_PREGUNTA ? H + 12 : 0) + H + 14;
	Q_.titulo = titulo_;
	Q_.an = an;
	Q_.al = al;
	c = control_(UC_TEXTO, 0, 20, ytexto, an - 40, texto_);
	c->al = n * U;
	if (clase == NV_C_PREGUNTA) {
		c = control_(UC_CAMPO, ID_CAMPO, 20, ytexto + n * U + 12, an - 40, NULL);
		snprintf(c->valor, sizeof c->valor, "%s", valor_);
	}
	if (clase == NV_C_AVISO) {
		c = control_(UC_BOTON, B_ACEPTAR, an - 120 - 14, al - H - 12, 120, "Aceptar");
		c->principal = 1;
	} else {
		c = control_(UC_BOTON, B_ACEPTAR, an - 2 * 130 - 26, al - H - 12, 130, clase == NV_C_SALIR ? "&Salir" : "Aceptar");
		c->principal = 1;
		c = control_(UC_BOTON, B_CANCELAR, an - 130 - 14, al - H - 12, 130, clase == NV_C_SALIR ? "&Quedarse" : "Cancelar");
		c->cancela = 1;
	}
	abre_q_();
}

int nv_cuadro_abierto(void)
{
	return clase_ >= 0;
}

/* el selector de archivos: lo elegido (una carpeta: se entra; un archivo: listo) */
static int archivo_elegido_(int id)
{
	ui_control *l = ui_control_id(&Q_, ID_LISTA), *nom = ui_control_id(&Q_, ID_NOMBRE);
	char ruta[2100];
	const char *n = nom ? nom->valor : "";
	struct stat st;

	if (id == ID_SUBIR) {
		char *b = strrchr(carpeta_, '/');

		if (b && b != carpeta_)
			*b = 0;
		else
			snprintf(carpeta_, sizeof carpeta_, "/");
		cuadro_archivo_();
		return 0;
	}
	if (!n[0] && l && l->elegida >= 0 && l->elegida < n_ent_)
		n = ent_[l->elegida];
	if (!n[0]) {
		cuadro_archivo_();
		return 0;
	}
	if (n[0] == '/')
		snprintf(ruta, sizeof ruta, "%s", n);
	else
		snprintf(ruta, sizeof ruta, "%s%s%s", carpeta_, carpeta_[strlen(carpeta_) - 1] == '/' ? "" : "/", n);
	if (stat(ruta, &st) == 0 && S_ISDIR(st.st_mode)) {
		snprintf(carpeta_, sizeof carpeta_, "%s", ruta);
		cuadro_archivo_();
		return 0;
	}
	if (stat(ruta, &st) != 0) {
		/* (no está: otra vez, con lo escrito) */
		cuadro_archivo_();
		return 0;
	}
	snprintf(valor_, sizeof valor_, "%s", ruta);
	return 1;
}

int nv_cuadro_suceso(int v, const nv_suceso *s, int dy, int an, int al)
{
	so_suceso e;
	int r;

	(void)an;
	(void)al;
	if (clase_ < 0)
		return 0;
	memcpy(&e, s, sizeof e);
	e.y -= dy;
	r = ui_cuadro_suceso(&Q_, v, &e);
	if (Q_.cerrado != -9999 && resultado_ < 0) {
		int id = Q_.cerrado;

		if (clase_ == NV_C_ARCHIVO) {
			if (id == B_ACEPTAR || id == ID_SUBIR) {
				if (archivo_elegido_(id))
					resultado_ = 1;
				return 1;
			}
			resultado_ = 0;
			return 1;
		}
		if (id == B_ACEPTAR) {
			ui_control *c = ui_control_id(&Q_, ID_CAMPO);

			if (c)
				snprintf(valor_, sizeof valor_, "%s", c->valor);
			resultado_ = 1;
		} else {
			resultado_ = clase_ == NV_C_AVISO ? 1 : 0;
		}
		return 1;
	}
	return r;
}

void nv_cuadro_pinta(uint32_t *px, int an, int al)
{
	lienzo l;

	if (clase_ < 0)
		return;
	memset(&l, 0, sizeof l);
	l.p = px;
	l.an = an;
	l.al = al;
	l.paso = an;
	lz_sin_recorte(&l);
	ui_cuadro_pinta(&l, &Q_);
}

int nv_cuadro_cerrado(char *valor, int cabe)
{
	int r = resultado_;

	if (clase_ < 0)
		return 0;
	if (r < 0)
		return -1;
	if (valor && cabe > 0)
		snprintf(valor, (size_t)cabe, "%s", valor_);
	clase_ = -1;
	resultado_ = -1;
	return r;
}
