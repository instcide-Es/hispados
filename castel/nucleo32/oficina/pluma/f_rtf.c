/* f_rtf.c - Pluma 2: el RTF (Rich Text Format), leer y escribir.
 *
 * Leer: un analizador de grupos con su pila de estado (con tope de
 * profundidad: lo que pasa de ahí se salta), las páginas de códigos 1252 y
 * 437 (\'hh), \uN con \ucN, las tablas de letras, colores y estilos, el
 * formato de la letra y del párrafo, las tabulaciones, las listas (las de
 * \listtable y \ls, y las de antes, \pn), las tablas (\trowd, \cellx, \cell,
 * \row: las celdas de todas las filas de una tabla se ponen en una rejilla
 * común, con las uniones a lo ancho —celdas más anchas o \clmgf/\clmrg— y
 * a lo alto —\clvmgf/\clvmrg—, el fondo y los bordes), las imágenes PNG y
 * JPEG (\pict con \pngblip o \jpegblip), la página, el encabezado y el pie
 * (con el número de página: \chpgn y los campos PAGE y NUMPAGES) y el
 * resumen (\info). Lo que no se conoce (\*\destino) se salta.
 * Escribir: lo mismo, en un RTF que abren Word y LibreOffice (solo ASCII:
 * 1252 con \'hh y \uN para lo demás; las imágenes BMP y GIF, pasadas a PNG).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "archivo.h"
#include "letras.h"
#include "maqueta.h"
#include "imagen.h"

#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#define STBIW_ASSERT(x) ((void)0)
#define STBIW_MALLOC(n) so_pide(n)
#define STBIW_REALLOC(p, n) so_repide(p, n)
#define STBIW_FREE(p) so_suelta(p)
#define STBIW_MEMMOVE memmove
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include "stb_image_write.h"
#pragma GCC diagnostic pop

#define HONDO 200		/* lo más anidado que se lee */
#define MAX_LETRAS 256
#define MAX_COLORES 256
#define MAX_ESTILOS 64
#define MAX_LISTAS 64
#define MAX_CELDAS 64		/* de una fila */
#define MAX_FILAS 4000

static const u16 t1252[32] = {
	0x20AC, 0xFFFD, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0xFFFD, 0x017D, 0xFFFD,
	0xFFFD, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0xFFFD, 0x017E, 0x0178
};

static u32 de1252(u8 c)
{
	return c >= 0x80 && c < 0xA0 ? t1252[c - 0x80] : c;
}

/* la letra en 1252 (0 si no está) */
static int a1252(u32 c)
{
	int i;

	if (c < 0x80 || (c >= 0xA0 && c <= 0xFF))
		return (int)c;
	for (i = 0; i < 32; i++)
		if (t1252[i] == c && c != 0xFFFD)
			return 0x80 + i;
	return 0;
}

/* =========================================================================
 *  Leer
 * ========================================================================= */
enum {
	D_TEXTO, D_SALTA, D_LETRAS, D_COLORES, D_ESTILOS, D_PICT, D_TITULO, D_AUTOR, D_ASUNTO, D_ENC, D_PIE,
	D_LISTAS, D_LISTA, D_NIVEL, D_OVERRIDES, D_OVERRIDE, D_FLDINST, D_FLDRSLT, D_PN
};

typedef struct {
	int dest;
	/* la letra */
	int letra, tam, negrita, cursiva, subrayado, tachado, vert, versalitas, color, resalte;
	/* el párrafo */
	fmt_parrafo p;
	int intbl, ls, ilvl, pn_tipo;
	int uc;
	int campo;		/* (en un \fldrslt) el campo ya puesto: no se copia su resultado */
} estado;

typedef struct {
	i32 x;			/* \cellx: el borde derecho */
	u8 hmgf, hmrg, vmgf, vmrg;
	u32 fondo;
} def_celda;

typedef struct {
	u32 par0;		/* el primer párrafo de sus celdas */
	u32 n;			/* cuántas celdas */
	def_celda c[MAX_CELDAS];
	int n_def;
	i32 izq;		/* \trleft */
	u8 alin;
} fila;

typedef struct {
	const u8 *s;
	u32 n, i;
	documento *d;
	estado e[HONDO];
	int h;			/* lo anidado */
	int salta_hondo;	/* > 0: grupos de más (se saltan) */
	int pagina;		/* 1252 o 437 */
	int deff;
	struct { int num; char nombre[48]; } letras[MAX_LETRAS];
	int n_letras;
	char nombre_letra[48];
	int nl_n;
	int nl_num;
	u32 colores[MAX_COLORES];
	int n_colores;
	int r, g, b, hay_rgb;
	i32 alto_sur;		/* (\u) la primera mitad de un par UTF-16 */
	struct { int num; char nombre[40]; } estilos[MAX_ESTILOS];
	int n_estilos, est_num;
	char nombre_est[40];
	int ne_n;
	/* las listas: \listid -> el tipo de cada nivel; \ls -> \listid */
	struct { i32 id; u8 tipo[9]; } listas[MAX_LISTAS];
	int n_listas, nivel_lista;
	struct { i32 ls, id; } over[MAX_LISTAS];
	int n_over;
	int listas_pn;
	i32 over_id;
	/* la imagen */
	bufer pict;
	int pict_tipo;		/* 1 png, 2 jpg, 0 otra (no se lee) */
	i32 picw, pich, picwg, pichg, escx, escy;
	int medio;		/* (medio octeto de hex pendiente) */
	/* el texto que va a parar al encabezado o al pie */
	parrafo *destino_par;	/* NULL: al documento */
	int destino_lineas;	/* (los párrafos del encabezado van en uno, con saltos) */
	/* el campo */
	bufer inst;
	int saltar_uc;
	/* la tabla de ahora */
	fila *filas;
	u32 n_filas;
	def_celda def[MAX_CELDAS];
	int n_def;
	def_celda cel;		/* las marcas de la celda que viene (antes de su \cellx) */
	i32 tr_izq;
	u8 tr_alin;
	int celdas_fila;	/* las celdas ya cerradas de la fila de ahora */
	u32 par_fila;		/* el primer párrafo de la fila de ahora */
	int en_tabla;		/* hay filas pendientes de ponerse en rejilla */
	int bordes, grosor;
	int salto_pendiente;	/* \page: el párrafo que viene empieza en otra página */
	int hay_texto;		/* el párrafo de ahora ya lleva algo */
	/* el formato de letra último (para no buscarlo con cada letra) */
	fmt_letra ultimo;
	u16 ultimo_n;
	int hay_ultimo;
} lector;

static estado *E(lector *l)
{
	return &l->e[l->h];
}

static fmt_parrafo par_base(void)
{
	fmt_parrafo p = fmt_parrafo_normal();

	p.despues = 0;
	p.antes = 0;
	p.inter = 100;
	p.estilo = 0;
	return p;
}

static void letra_por_defecto(estado *e, int deff)
{
	e->letra = deff;
	e->tam = 24;
	e->negrita = e->cursiva = e->subrayado = e->tachado = e->vert = e->versalitas = 0;
	e->color = 0;
	e->resalte = -1;
}

static const char *familia_de(lector *l, int num)
{
	int i;

	for (i = 0; i < l->n_letras; i++)
		if (l->letras[i].num == num)
			return l->letras[i].nombre;
	for (i = 0; i < l->n_letras; i++)
		if (l->letras[i].num == l->deff)
			return l->letras[i].nombre;
	return "Times New Roman";
}

static u32 color_de(lector *l, int n)
{
	if (n <= 0 || n >= l->n_colores)
		return SIN_COLOR;
	return l->colores[n];
}

static u16 fmt_de(lector *l)
{
	estado *e = E(l);
	fmt_letra f;

	memset(&f, 0, sizeof f);
	copia(f.familia, familia_de(l, e->letra), sizeof f.familia);
	if (!f.familia[0])
		copia(f.familia, "Times New Roman", sizeof f.familia);
	f.tam = (u16)(e->tam < 2 ? 2 : e->tam > 3200 ? 3200 : e->tam);
	f.negrita = (u8)e->negrita;
	f.cursiva = (u8)e->cursiva;
	f.subrayado = (u8)e->subrayado;
	f.tachado = (u8)e->tachado;
	f.vert = (u8)e->vert;
	f.versalitas = (u8)e->versalitas;
	f.color = color_de(l, e->color);
	f.resalte = e->resalte > 0 ? color_de(l, e->resalte) : SIN_COLOR;
	if (l->hay_ultimo && fmt_iguales(&f, &l->ultimo))
		return l->ultimo_n;
	l->ultimo = f;
	l->ultimo_n = doc_fmt(l->d, &f);
	l->hay_ultimo = 1;
	return l->ultimo_n;
}

/* el párrafo donde va el texto */
static parrafo *actual(lector *l)
{
	if (l->destino_par)
		return l->destino_par;
	if (!l->d->n_par && !arch_parrafo(l->d))
		return NULL;
	return &l->d->par[l->d->n_par - 1];
}

static void pon_tabla(lector *l);

/* una letra al texto */
static void letra(lector *l, u32 c)
{
	parrafo *p;
	u16 f;

	if (l->saltar_uc > 0) {
		l->saltar_uc--;
		return;
	}
	switch (E(l)->dest) {
	case D_TEXTO:
	case D_ENC:
	case D_PIE:
	case D_FLDRSLT:
		break;
	case D_LETRAS:
		if (c == ';') {
			if (l->n_letras < MAX_LETRAS) {
				u32 k = 0;

				l->letras[l->n_letras].num = l->nl_num;
				/* (sin los espacios de delante y de detrás) */
				while (k < (u32)l->nl_n && l->nombre_letra[k] == ' ')
					k++;
				copia(l->letras[l->n_letras].nombre, l->nombre_letra + k, sizeof l->letras[0].nombre);
				{
					u32 m = largo(l->letras[l->n_letras].nombre);

					while (m && l->letras[l->n_letras].nombre[m - 1] == ' ')
						l->letras[l->n_letras].nombre[--m] = 0;
				}
				l->n_letras++;
			}
			l->nl_n = 0;
			l->nombre_letra[0] = 0;
		} else if (l->nl_n < 46) {
			l->nl_n += utf8_pon(c, l->nombre_letra + l->nl_n);
			l->nombre_letra[l->nl_n] = 0;
		}
		return;
	case D_COLORES:
		if (c == ';') {
			if (l->n_colores < MAX_COLORES)
				l->colores[l->n_colores++] = l->hay_rgb ? (u32)(l->r << 16 | l->g << 8 | l->b) : SIN_COLOR;
			l->r = l->g = l->b = 0;
			l->hay_rgb = 0;
		}
		return;
	case D_ESTILOS:
		if (c == ';') {
			if (l->n_estilos < MAX_ESTILOS) {
				l->estilos[l->n_estilos].num = l->est_num;
				copia(l->estilos[l->n_estilos].nombre, l->nombre_est, sizeof l->estilos[0].nombre);
				l->n_estilos++;
			}
			l->ne_n = 0;
			l->nombre_est[0] = 0;
		} else if (l->ne_n < 36) {
			l->ne_n += utf8_pon(c, l->nombre_est + l->ne_n);
			l->nombre_est[l->ne_n] = 0;
		}
		return;
	case D_TITULO:
	case D_AUTOR:
	case D_ASUNTO: {
		char *s = E(l)->dest == D_TITULO ? l->d->titulo : E(l)->dest == D_AUTOR ? l->d->autor : l->d->asunto;
		u32 cabe = E(l)->dest == D_AUTOR ? sizeof l->d->autor : sizeof l->d->titulo, m = largo(s);

		if (m + 5 < cabe) {
			m += (u32)utf8_pon(c, s + m);
			s[m] = 0;
		}
		return;
	}
	case D_FLDINST:
		buf_u32(&l->inst, c);
		return;
	default:
		return;
	}
	if (E(l)->dest == D_FLDRSLT && E(l)->campo)
		return;
	if (c < 0x20 && c != '\t')
		return;
	/* el texto de un párrafo de fuera de la tabla: la tabla de antes se acaba */
	if (l->en_tabla && !E(l)->intbl && !l->destino_par)
		pon_tabla(l);
	p = actual(l);
	if (!p)
		return;
	f = fmt_de(l);
	if (!par_cabe(p, p->n + 1)) {
		l->d->sin_memoria = 1;
		return;
	}
	p->t[p->n] = c;
	p->f[p->n] = f;
	p->n++;
	l->hay_texto = 1;
}

/* cierra el párrafo de ahora (\par) */
static void parrafo_fin(lector *l, int con_par)
{
	estado *e = E(l);
	parrafo *p;

	if (l->destino_par) {
		/* en el encabezado o el pie: un salto de línea */
		p = l->destino_par;
		if (l->destino_lineas++ == 0)
			p->p = e->p;
		else if (con_par) {
			u16 f = fmt_de(l);

			par_inserta_uno(p, p->n, SALTO_LINEA, f);
		}
		return;
	}
	if (e->intbl && con_par) {
		/* un párrafo dentro de una celda: la celda es un párrafo, con saltos */
		p = actual(l);
		if (p)
			par_inserta_uno(p, p->n, SALTO_LINEA, fmt_de(l));
		return;
	}
	if (l->en_tabla && !e->intbl)
		pon_tabla(l);
	p = actual(l);
	if (!p)
		return;
	{
		u16 t = p->p.tabla, fi = p->p.fila, co = p->p.col;

		p->p = e->p;
		p->p.tabla = t;
		p->p.fila = fi;
		p->p.col = co;
	}
	if (l->salto_pendiente) {
		p->p.salto_antes = 1;
		l->salto_pendiente = 0;
	}
	/* la lista */
	if (e->ls > 0 || e->pn_tipo) {
		int tipo = e->pn_tipo, i;

		if (e->ls > 0) {
			i32 id = -1;

			tipo = LISTA_NUMEROS;
			for (i = 0; i < l->n_over; i++)
				if (l->over[i].ls == e->ls)
					id = l->over[i].id;
			for (i = 0; i < l->n_listas; i++)
				if (l->listas[i].id == id)
					tipo = l->listas[i].tipo[e->ilvl % 9];
		}
		if (tipo) {
			p->p.lista = (u8)tipo;
			p->p.nivel = (u8)(e->ilvl % 9);
			if (e->ls > 0)
				p->p.lista_id = (u16)e->ls;
			else {
				/* (las de antes, \pn: una lista por cada tanda seguida del mismo tipo) */
				const parrafo *q = l->d->n_par > 1 ? &l->d->par[l->d->n_par - 2] : NULL;

				if (q && q->p.lista == tipo && q->p.lista_id >= 30000)
					p->p.lista_id = q->p.lista_id;
				else
					p->p.lista_id = (u16)(30000 + (++l->listas_pn));
			}
			if (!p->p.sang_izq)
				p->p.sang_izq = 720 + 360 * p->p.nivel;
			if (!p->p.sang_primera)
				p->p.sang_primera = -360;
		}
	}
	if (!arch_parrafo(l->d))
		l->d->sin_memoria = 1;
	l->hay_texto = 0;
}

/* ---- las tablas ---- */
static int cmp_i32(const void *a, const void *b)
{
	i32 x = *(const i32 *)a, y = *(const i32 *)b;

	return x < y ? -1 : x > y;
}

static void ordena(i32 *v, int n)
{
	int i, j;

	for (i = 1; i < n; i++)
		for (j = i; j > 0 && cmp_i32(&v[j - 1], &v[j]) > 0; j--) {
			i32 t = v[j];

			v[j] = v[j - 1];
			v[j - 1] = t;
		}
}

/* \cell: la celda de ahora se cierra; viene otra */
static void celda_fin(lector *l)
{
	parrafo *p = actual(l);

	if (!p || l->destino_par)
		return;
	if (l->celdas_fila == 0)
		l->par_fila = l->d->n_par - 1;
	p->p = E(l)->p;
	p->p.tabla = 0xFFFF;	/* (aún sin tabla: pon_tabla la pone) */
	p->p.col = (u16)l->celdas_fila;
	l->celdas_fila++;
	if (!arch_parrafo(l->d))
		l->d->sin_memoria = 1;
	l->hay_texto = 0;
}

/* \row: la fila se apunta */
static void fila_fin(lector *l)
{
	fila *f;

	if (l->destino_par)
		return;
	/* (una fila sin \cell detrás de su última celda: se cierra) */
	if (l->hay_texto && E(l)->intbl)
		celda_fin(l);
	if (l->celdas_fila == 0)
		return;
	if (l->n_filas >= MAX_FILAS) {
		/* (demasiadas: lo de después, como párrafos) */
		u32 k;

		for (k = l->par_fila; k < l->d->n_par; k++)
			if (l->d->par[k].p.tabla == 0xFFFF)
				l->d->par[k].p.tabla = 0;
		l->celdas_fila = 0;
		return;
	}
	if (!(l->n_filas & 63)) {
		fila *nf = so_repide(l->filas, (l->n_filas + 64) * sizeof(fila));

		if (!nf) {
			l->d->sin_memoria = 1;
			return;
		}
		l->filas = nf;
	}
	f = &l->filas[l->n_filas++];
	memset(f, 0, sizeof *f);
	f->par0 = l->par_fila;
	f->n = (u32)l->celdas_fila;
	f->n_def = l->n_def;
	memcpy(f->c, l->def, sizeof f->c);
	f->izq = l->tr_izq;
	f->alin = l->tr_alin;
	l->celdas_fila = 0;
	l->en_tabla = 1;
}

/* las filas apuntadas, en una tabla: la rejilla de todos los bordes */
static void pon_tabla(lector *l)
{
	documento *d = l->d;
	i32 bordes[MAX_COLS + 1];
	int nb = 0, cols, t;
	u32 r, k, ini, fin, filas;
	parrafo *viejos = NULL, *nuevos;
	u32 n_viejos;

	l->en_tabla = 0;
	if (!l->n_filas)
		return;
	ini = l->filas[0].par0;
	fin = l->filas[l->n_filas - 1].par0 + l->filas[l->n_filas - 1].n;
	if (fin > d->n_par || ini >= fin)
		goto fuera;
	/* los bordes de todas las filas, sin repetir (los muy juntos, uno) */
	bordes[nb++] = l->filas[0].izq;
	for (r = 0; r < l->n_filas; r++) {
		fila *f = &l->filas[r];
		i32 x = f->izq;

		for (k = 0; k < f->n; k++) {
			int j, esta = 0;

			x = (int)k < f->n_def ? f->c[k].x : x + 1440;
			if ((int)k < f->n_def && f->c[k].hmrg)
				continue;
			for (j = 0; j < nb; j++)
				if (bordes[j] - x < 30 && x - bordes[j] < 30)
					esta = 1;
			if (!esta && nb <= MAX_COLS)
				bordes[nb++] = x;
		}
	}
	ordena(bordes, nb);
	cols = nb - 1;
	if (cols < 1)
		goto fuera;
	filas = l->n_filas;
	t = doc_nueva_tabla(d, (u16)filas, (u16)cols, 1440);
	if (t < 0)
		goto fuera;
	for (k = 0; (int)k < cols; k++)
		d->tab[t].ancho[k] = bordes[k + 1] - bordes[k] < 100 ? 100 : bordes[k + 1] - bordes[k];
	d->tab[t].bordes = (u8)(l->bordes ? BORDE_TODOS : BORDE_NO);
	if (l->grosor)
		d->tab[t].grosor = (u16)(l->grosor > 200 ? 200 : l->grosor);
	d->tab[t].alin = l->filas[0].alin;
	/* los párrafos viejos (sacados sin soltarlos) y la rejilla nueva */
	n_viejos = fin - ini;
	viejos = so_pide(n_viejos * sizeof(parrafo));
	nuevos = so_pide(filas * (u32)cols * sizeof(parrafo));
	if (!viejos || !nuevos) {
		so_suelta(viejos);
		so_suelta(nuevos);
		d->sin_memoria = 1;
		d->tab[t].viva = 0;
		goto fuera;
	}
	memcpy(viejos, d->par + ini, n_viejos * sizeof(parrafo));
	memset(nuevos, 0, filas * (u32)cols * sizeof(parrafo));
	for (k = 0; k < filas * (u32)cols; k++) {
		parrafo *q = &nuevos[k];

		q->p = par_base();
		q->p.tabla = (u16)(t + 1);
		q->p.fila = (u16)(k / (u32)cols);
		q->p.col = (u16)(k % (u32)cols);
		q->ancho_maq = -1;
		q->p.cubierta = 2;	/* (aún sin celda: luego, vacía) */
	}
	for (r = 0; r < filas; r++) {
		fila *f = &l->filas[r];
		i32 x0 = f->izq, x;
		int c0 = 0;

		for (k = 0; k < f->n; k++) {
			parrafo *o = &viejos[f->par0 - ini + k];
			int c1, j, span;
			def_celda dc;

			memset(&dc, 0, sizeof dc);
			dc.fondo = SIN_COLOR;
			if ((int)k < f->n_def)
				dc = f->c[k];
			x = (int)k < f->n_def ? dc.x : x0 + 1440;
			/* la columna donde empieza y donde acaba */
			for (j = 0; j < nb && bordes[j] < x0 - 29; j++)
				;
			c0 = j < cols ? j : cols - 1;
			for (j = c0 + 1; j < nb && bordes[j] < x - 29; j++)
				;
			c1 = j < nb ? j : nb - 1;
			span = c1 - c0 < 1 ? 1 : c1 - c0;
			x0 = x;
			if (dc.hmrg) {
				/* unida a la de su izquierda: lo suyo, a aquella */
				int a = c0 - 1;

				while (a >= 0 && nuevos[r * (u32)cols + (u32)a].p.cubierta == 1)
					a--;
				if (a >= 0) {
					parrafo *an = &nuevos[r * (u32)cols + (u32)a];

					an->p.abarca_cols = (u8)(c0 + span - a);
					if (o->n && par_cabe(an, an->n + o->n + 1)) {
						par_inserta_uno(an, an->n, SALTO_LINEA, o->f[0]);
						par_inserta(an, an->n, o->t, o->f, o->n);
					}
					for (j = c0; j < c0 + span && j < cols; j++)
						nuevos[r * (u32)cols + (u32)j].p.cubierta = 1;
					continue;
				}
			}
			if (c0 >= cols)
				continue;
			{
				parrafo *q = &nuevos[r * (u32)cols + (u32)c0];
				u16 fi = q->p.fila, co = q->p.col;

				par_suelta(q);
				*q = *o;
				memset(o, 0, sizeof *o);
				q->p.tabla = (u16)(t + 1);
				q->p.fila = fi;
				q->p.col = co;
				q->p.cubierta = 0;
				q->p.abarca_cols = (u8)(span > 1 ? span : 0);
				q->p.abarca_filas = 0;
				q->p.salto_antes = 0;
				q->p.lista = LISTA_NO;
				q->p.fondo = dc.fondo;
				q->ancho_maq = -1;
				for (j = c0 + 1; j < c0 + span && j < cols; j++)
					nuevos[r * (u32)cols + (u32)j].p.cubierta = 1;
				if (dc.vmrg && r > 0) {
					/* unida a la de arriba: se busca la que manda */
					int rr = (int)r - 1;

					while (rr >= 0 && nuevos[(u32)rr * (u32)cols + (u32)c0].p.cubierta == 1)
						rr--;
					if (rr >= 0) {
						parrafo *an = &nuevos[(u32)rr * (u32)cols + (u32)c0];

						an->p.abarca_filas = (u8)(r - (u32)rr + 1);
						if (q->n && par_cabe(an, an->n + q->n + 1)) {
							par_inserta_uno(an, an->n, SALTO_LINEA, q->f[0]);
							par_inserta(an, an->n, q->t, q->f, q->n);
						}
						par_suelta(q);
						q->p.cubierta = 1;
						q->p.abarca_cols = 0;
						for (j = c0 + 1; j < c0 + span && j < cols; j++)
							nuevos[r * (u32)cols + (u32)j].p.cubierta = 1;
					}
				}
			}
		}
	}
	/* lo que no ha tenido celda: vacío */
	for (k = 0; k < filas * (u32)cols; k++)
		if (nuevos[k].p.cubierta == 2)
			nuevos[k].p.cubierta = 0;
	/* una unión a lo ancho o a lo alto que no cuadra: fuera */
	for (k = 0; k < filas * (u32)cols; k++) {
		parrafo *q = &nuevos[k];

		if (q->p.col + (q->p.abarca_cols ? q->p.abarca_cols : 1) > cols)
			q->p.abarca_cols = (u8)(cols - q->p.col);
		if (q->p.fila + (q->p.abarca_filas ? q->p.abarca_filas : 1) > filas)
			q->p.abarca_filas = (u8)(filas - q->p.fila);
	}
	/* se cambian los viejos por la rejilla */
	for (k = 0; k < n_viejos; k++)
		par_suelta(&viejos[k]);
	memset(d->par + ini, 0, n_viejos * sizeof(parrafo));
	doc_quita_parrafos(d, ini, n_viejos);
	if (!doc_inserta_parrafos(d, ini, filas * (u32)cols)) {
		for (k = 0; k < filas * (u32)cols; k++)
			par_suelta(&nuevos[k]);
		so_suelta(nuevos);
		so_suelta(viejos);
		d->tab[t].viva = 0;
		l->n_filas = 0;
		return;
	}
	memcpy(d->par + ini, nuevos, filas * (u32)cols * sizeof(parrafo));
	so_suelta(nuevos);
	so_suelta(viejos);
	l->n_filas = 0;
	l->bordes = 0;
	l->grosor = 0;
	return;
fuera:
	/* (no se ha podido: sus celdas se quedan como párrafos) */
	for (k = 0; k < d->n_par; k++)
		if (d->par[k].p.tabla == 0xFFFF)
			d->par[k].p.tabla = 0;
	l->n_filas = 0;
}

/* ---- la imagen ---- */
static void pict_fin(lector *l)
{
	u8 *datos;
	int i;
	parrafo *p;

	if (l->pict_tipo && l->pict.n && !l->pict.mal) {
		datos = so_pide(l->pict.n + 1);
		if (datos) {
			memcpy(datos, l->pict.d, l->pict.n);
			i = doc_nueva_imagen(l->d, datos, l->pict.n);
			if (i < 0)
				so_suelta(datos);
			else {
				imagen *m = &l->d->img[i];
				i32 w = l->picwg > 0 ? l->picwg : m->ancho, h = l->pichg > 0 ? l->pichg : m->alto;

				if (l->escx > 0 && l->escx != 100)
					w = (i32)((i64)w * l->escx / 100);
				if (l->escy > 0 && l->escy != 100)
					h = (i32)((i64)h * l->escy / 100);
				if (w < 20 || w > 31680)
					w = m->ancho;
				if (h < 20 || h > 31680)
					h = m->alto;
				m->ancho = w;
				m->alto = h;
				if (l->en_tabla && !E(l)->intbl && !l->destino_par)
					pon_tabla(l);
				p = actual(l);
				if (p) {
					par_inserta_uno(p, p->n, IMAGEN_0 + (u32)i, fmt_de(l));
					l->hay_texto = 1;
				}
			}
		}
	}
	buf_suelta(&l->pict);
	l->pict_tipo = 0;
}

static int hex(u8 c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

/* ---- los grupos ---- */
static void abre_grupo(lector *l)
{
	if (l->salta_hondo || l->h + 1 >= HONDO) {
		l->salta_hondo++;
		return;
	}
	l->e[l->h + 1] = l->e[l->h];
	l->h++;
}

static void cierra_grupo(lector *l)
{
	int dest;

	if (l->salta_hondo) {
		l->salta_hondo--;
		return;
	}
	if (l->h == 0)
		return;
	dest = E(l)->dest;
	if (dest != l->e[l->h - 1].dest) {
		switch (dest) {
		case D_PICT:
			pict_fin(l);
			break;
		case D_ENC:
		case D_PIE:
			if (l->destino_par) {
				/* (el salto de línea que sobra al final, fuera) */
				parrafo *p = l->destino_par;

				while (p->n && p->t[p->n - 1] == SALTO_LINEA)
					p->n--;
			}
			l->destino_par = NULL;
			break;
		case D_FLDINST: {
			/* el campo: PAGE y NUMPAGES pasan a ser los de Pluma */
			char *s;
			u32 c = 0;

			buf_c(&l->inst, 0);
			s = l->inst.mal ? NULL : (char *)l->inst.d;
			if (s) {
				while (*s == ' ')
					s++;
				if (empieza(s, "PAGE") && (s[4] == 0 || s[4] == ' ' || s[4] == '\\'))
					c = CAMPO_PAGINA;
				else if (empieza(s, "NUMPAGES"))
					c = CAMPO_PAGINAS;
			}
			buf_suelta(&l->inst);
			if (c) {
				parrafo *p;

				l->h--;
				p = actual(l);
				if (p) {
					par_inserta_uno(p, p->n, c, fmt_de(l));
					l->hay_texto = 1;
				}
				/* el resultado (el número de ahora) no se copia */
				E(l)->campo = 1;
				return;
			}
			break;
		}
		case D_LISTA:
			l->n_listas += l->n_listas < MAX_LISTAS && l->listas[l->n_listas].id ? 1 : 0;
			break;
		case D_OVERRIDE:
			if (l->n_over < MAX_LISTAS) {
				l->over[l->n_over].id = l->over_id;
				l->over[l->n_over].ls = l->over[l->n_over].ls ? l->over[l->n_over].ls : 0;
				if (l->over[l->n_over].ls)
					l->n_over++;
			}
			break;
		}
	}
	{
		int campo = l->e[l->h - 1].campo;

		l->h--;
		/* (el \fldrslt de un campo que ya se ha puesto) */
		if (dest == D_FLDRSLT)
			E(l)->campo = 0;
		else
			E(l)->campo = campo;
	}
}

/* nombre de estilo de RTF -> estilo de Pluma */
static int estilo_de(lector *l, int num)
{
	static const struct { const char *rtf; int est; } t[] = {
		{ "heading 1", EST_T1 }, { "heading 2", EST_T2 }, { "heading 3", EST_T3 }, { "title", EST_TITULO },
		{ "subtitle", EST_SUBTITULO }, { "quote", EST_CITA }, { "header", EST_ENC }, { "footer", EST_PIE },
		{ "no spacing", EST_SIN_ESPACIO }, { "list paragraph", EST_LISTA }, { "footnote text", EST_NOTA },
	};
	int i;
	u32 k;

	for (i = 0; i < l->n_estilos; i++)
		if (l->estilos[i].num == num) {
			int e = doc_estilo(l->d, l->estilos[i].nombre);

			if (e >= 0)
				return e;
			for (k = 0; k < sizeof t / sizeof t[0]; k++)
				if (iguales_sin_caja(l->estilos[i].nombre, t[k].rtf))
					return t[k].est;
		}
	return 0;
}

static int tipo_nfc(i32 n)
{
	switch (n) {
	case 23: return LISTA_VINETAS;
	case 1: case 2: return LISTA_ROMANOS;
	case 3: case 4: return LISTA_LETRAS;
	case 255: return LISTA_NO;
	}
	return LISTA_NUMEROS;
}

static i32 limita(i64 v, i32 a, i32 b)
{
	return v < a ? a : v > b ? b : (i32)v;
}

/* una palabra de control */
static void control(lector *l, const char *w, int hay_n, i32 n)
{
	estado *e = E(l);
	fmt_parrafo *p = &e->p;
	int ignorable = 0;

	(void)ignorable;
	/* ---- los destinos ---- */
	if (iguales(w, "fonttbl")) { e->dest = D_LETRAS; return; }
	if (iguales(w, "colortbl")) { e->dest = D_COLORES; l->n_colores = 0; return; }
	if (iguales(w, "stylesheet")) { e->dest = D_ESTILOS; return; }
	if (iguales(w, "info")) { e->dest = D_SALTA; return; }
	if (iguales(w, "title")) { e->dest = D_TITULO; l->d->titulo[0] = 0; return; }
	if (iguales(w, "author")) { e->dest = D_AUTOR; l->d->autor[0] = 0; return; }
	if (iguales(w, "subject")) { e->dest = D_ASUNTO; l->d->asunto[0] = 0; return; }
	if (iguales(w, "pict")) {
		e->dest = D_PICT;
		buf_suelta(&l->pict);
		l->pict_tipo = 0;
		l->picw = l->pich = l->picwg = l->pichg = 0;
		l->escx = l->escy = 100;
		l->medio = -1;
		return;
	}
	if (iguales(w, "shppict")) return;	/* (lo de dentro se lee) */
	if (iguales(w, "nonshppict") || iguales(w, "pntext") || iguales(w, "listtext") || iguales(w, "footnote") ||
	    iguales(w, "annotation") || iguales(w, "atnid") || iguales(w, "xe") || iguales(w, "tc") || iguales(w, "bkmkstart") ||
	    iguales(w, "bkmkend") || iguales(w, "headerl") || iguales(w, "footerl") || iguales(w, "headerf") ||
	    iguales(w, "footerf") || iguales(w, "object") || iguales(w, "shp") || iguales(w, "themedata") ||
	    iguales(w, "colorschememapping") || iguales(w, "latentstyles") || iguales(w, "datastore") ||
	    iguales(w, "generator") || iguales(w, "rsidtbl") || iguales(w, "xmlnstbl") || iguales(w, "mmathPr") ||
	    iguales(w, "pgdsctbl") || iguales(w, "falt") || iguales(w, "panose") || iguales(w, "keywords") ||
	    iguales(w, "comment") || iguales(w, "operator") || iguales(w, "doccomm") || iguales(w, "creatim") ||
	    iguales(w, "revtim") || iguales(w, "printim") || iguales(w, "buptim") || iguales(w, "company") ||
	    iguales(w, "fldtype") || iguales(w, "userprops")) {
		e->dest = D_SALTA;
		return;
	}
	if (iguales(w, "header") || iguales(w, "headerr")) {
		e->dest = D_ENC;
		par_suelta(&l->d->enc);
		l->d->enc.p = l->d->est[EST_ENC].p;
		l->d->enc.ancho_maq = -1;
		l->destino_par = &l->d->enc;
		l->destino_lineas = 0;
		l->d->hay_enc = 1;
		e->intbl = 0;
		return;
	}
	if (iguales(w, "footer") || iguales(w, "footerr")) {
		e->dest = D_PIE;
		par_suelta(&l->d->pie);
		l->d->pie.p = l->d->est[EST_PIE].p;
		l->d->pie.ancho_maq = -1;
		l->destino_par = &l->d->pie;
		l->destino_lineas = 0;
		l->d->hay_pie = 1;
		e->intbl = 0;
		return;
	}
	if (iguales(w, "field")) return;
	if (iguales(w, "fldinst")) { e->dest = D_FLDINST; buf_suelta(&l->inst); return; }
	if (iguales(w, "fldrslt")) { if (e->dest != D_SALTA) e->dest = D_FLDRSLT; return; }
	if (iguales(w, "listtable")) { e->dest = D_LISTAS; return; }
	if (iguales(w, "list") && e->dest == D_LISTAS) {
		e->dest = D_LISTA;
		l->nivel_lista = 0;
		if (l->n_listas < MAX_LISTAS) {
			int k;

			l->listas[l->n_listas].id = 0;
			for (k = 0; k < 9; k++)
				l->listas[l->n_listas].tipo[k] = LISTA_NUMEROS;
		}
		return;
	}
	if (iguales(w, "listlevel") && e->dest == D_LISTA) { e->dest = D_NIVEL; return; }
	if (iguales(w, "listoverridetable")) { e->dest = D_OVERRIDES; return; }
	if (iguales(w, "listoverride") && e->dest == D_OVERRIDES) {
		e->dest = D_OVERRIDE;
		l->over_id = 0;
		if (l->n_over < MAX_LISTAS)
			l->over[l->n_over].ls = 0;
		return;
	}
	if (iguales(w, "pn")) { e->dest = D_PN; e->pn_tipo = LISTA_NUMEROS; return; }
	/* ---- dentro de las tablas de listas ---- */
	if (e->dest == D_NIVEL) {
		if (iguales(w, "levelnfc") && l->n_listas < MAX_LISTAS && l->nivel_lista < 9)
			l->listas[l->n_listas].tipo[l->nivel_lista] = (u8)tipo_nfc(n);
		return;
	}
	if (e->dest == D_LISTA) {
		if (iguales(w, "listid") && l->n_listas < MAX_LISTAS)
			l->listas[l->n_listas].id = n ? n : 1;
		return;
	}
	if (e->dest == D_OVERRIDE) {
		if (iguales(w, "listid"))
			l->over_id = n;
		else if (iguales(w, "ls") && l->n_over < MAX_LISTAS)
			l->over[l->n_over].ls = n;
		return;
	}
	if (e->dest == D_PN) {
		/* las de antes: el tipo (lo de este grupo vale para el párrafo) */
		int tipo = 0;

		if (iguales(w, "pnlvlblt")) tipo = LISTA_VINETAS;
		else if (iguales(w, "pndec")) tipo = LISTA_NUMEROS;
		else if (iguales(w, "pnucrm") || iguales(w, "pnlcrm")) tipo = LISTA_ROMANOS;
		else if (iguales(w, "pnucltr") || iguales(w, "pnlcltr")) tipo = LISTA_LETRAS;
		if (tipo) {
			e->pn_tipo = tipo;
			if (l->h > 0)
				l->e[l->h - 1].pn_tipo = tipo;
		}
		if (iguales(w, "pntxta") || iguales(w, "pntxtb"))
			e->dest = D_SALTA;
		return;
	}
	if (e->dest == D_COLORES) {
		if (iguales(w, "red")) { l->r = limita(n, 0, 255); l->hay_rgb = 1; }
		else if (iguales(w, "green")) { l->g = limita(n, 0, 255); l->hay_rgb = 1; }
		else if (iguales(w, "blue")) { l->b = limita(n, 0, 255); l->hay_rgb = 1; }
		return;
	}
	if (e->dest == D_LETRAS) {
		if (iguales(w, "f")) {
			l->nl_num = n;
			l->nl_n = 0;
			l->nombre_letra[0] = 0;
		}
		return;
	}
	if (e->dest == D_ESTILOS) {
		if (iguales(w, "s"))
			l->est_num = n;
		else if (iguales(w, "cs") || iguales(w, "ds") || iguales(w, "ts"))
			l->est_num = -1 - n;
		return;
	}
	if (e->dest == D_PICT) {
		if (iguales(w, "pngblip")) l->pict_tipo = 1;
		else if (iguales(w, "jpegblip")) l->pict_tipo = 2;
		else if (iguales(w, "wmetafile") || iguales(w, "emfblip") || iguales(w, "macpict") || iguales(w, "dibitmap") || iguales(w, "wbitmap"))
			l->pict_tipo = 0, l->pict.mal = 1;
		else if (iguales(w, "picw")) l->picw = n;
		else if (iguales(w, "pich")) l->pich = n;
		else if (iguales(w, "picwgoal")) l->picwg = limita(n, 0, 31680);
		else if (iguales(w, "pichgoal")) l->pichg = limita(n, 0, 31680);
		else if (iguales(w, "picscalex")) l->escx = limita(n, 1, 1000);
		else if (iguales(w, "picscaley")) l->escy = limita(n, 1, 1000);
		return;
	}
	if (e->dest == D_SALTA || e->dest == D_TITULO || e->dest == D_AUTOR || e->dest == D_ASUNTO || e->dest == D_LISTAS ||
	    e->dest == D_OVERRIDES || e->dest == D_FLDINST)
		goto letras_especiales;
	/* ---- el documento ---- */
	if (iguales(w, "ansicpg")) { l->pagina = (n == 437 || n == 850) ? 437 : 1252; return; }
	if (iguales(w, "pc") || iguales(w, "pca")) { l->pagina = 437; return; }
	if (iguales(w, "deff")) { l->deff = n; e->letra = n; return; }
	if (iguales(w, "paperw")) { l->d->pag_an = limita(n, 2880, 31680); return; }
	if (iguales(w, "paperh")) { l->d->pag_al = limita(n, 2880, 31680); return; }
	if (iguales(w, "margl")) { l->d->marg_izq = limita(n, 0, 8000); return; }
	if (iguales(w, "margr")) { l->d->marg_der = limita(n, 0, 8000); return; }
	if (iguales(w, "margt")) { l->d->marg_sup = limita(n, 0, 8000); return; }
	if (iguales(w, "margb")) { l->d->marg_inf = limita(n, 0, 8000); return; }
	if (iguales(w, "headery")) { l->d->dist_enc = limita(n, 0, 8000); return; }
	if (iguales(w, "footery")) { l->d->dist_pie = limita(n, 0, 8000); return; }
	if (iguales(w, "pgnstarts")) { l->d->num_inicio = (u32)limita(n, 0, 99999); return; }
	if (iguales(w, "titlepg")) { l->d->enc_no_primera = 1; return; }
	/* ---- la letra ---- */
	if (iguales(w, "plain")) { letra_por_defecto(e, l->deff); return; }
	if (iguales(w, "f")) { e->letra = n; return; }
	if (iguales(w, "fs")) { e->tam = limita(hay_n ? n : 24, 2, 3200); return; }
	if (iguales(w, "b")) { e->negrita = !hay_n || n != 0; return; }
	if (iguales(w, "i")) { e->cursiva = !hay_n || n != 0; return; }
	if (iguales(w, "ul") || iguales(w, "uld") || iguales(w, "uldb") || iguales(w, "ulw") || iguales(w, "uldash") || iguales(w, "ulth") ||
	    iguales(w, "ulwave") || iguales(w, "uldashd")) { e->subrayado = !hay_n || n != 0; return; }
	if (iguales(w, "ulnone")) { e->subrayado = 0; return; }
	if (iguales(w, "strike") || iguales(w, "striked")) { e->tachado = !hay_n || n != 0; return; }
	if (iguales(w, "super")) { e->vert = 1; return; }
	if (iguales(w, "sub")) { e->vert = 2; return; }
	if (iguales(w, "nosupersub")) { e->vert = 0; return; }
	if (iguales(w, "scaps")) { e->versalitas = !hay_n || n != 0; return; }
	if (iguales(w, "cf")) { e->color = limita(n, 0, MAX_COLORES); return; }
	if (iguales(w, "highlight") || iguales(w, "chcbpat") || iguales(w, "cb")) { e->resalte = limita(n, 0, MAX_COLORES); return; }
	if (iguales(w, "uc")) { e->uc = limita(n, 0, 10); return; }
	/* ---- el párrafo ---- */
	if (iguales(w, "pard")) {
		int intbl = 0;

		(void)intbl;
		*p = par_base();
		e->intbl = 0;
		e->ls = 0;
		e->ilvl = 0;
		e->pn_tipo = 0;
		return;
	}
	if (iguales(w, "ql")) { p->alin = AL_IZQ; return; }
	if (iguales(w, "qc")) { p->alin = AL_CENTRO; return; }
	if (iguales(w, "qr")) { p->alin = AL_DER; return; }
	if (iguales(w, "qj")) { p->alin = AL_JUST; return; }
	if (iguales(w, "li") || iguales(w, "lin")) { p->sang_izq = limita(n, -31680, 31680); return; }
	if (iguales(w, "ri") || iguales(w, "rin")) { p->sang_der = limita(n, -31680, 31680); return; }
	if (iguales(w, "fi")) { p->sang_primera = limita(n, -31680, 31680); return; }
	if (iguales(w, "sb")) { p->antes = limita(n, 0, 31680); return; }
	if (iguales(w, "sa")) { p->despues = limita(n, 0, 31680); return; }
	if (iguales(w, "sl")) {
		if (n == 0) {
			p->inter_tipo = INTER_MULTIPLE;
			p->inter = 100;
		} else if (n < 0) {
			p->inter_tipo = INTER_EXACTO;
			p->inter = limita(-(i64)n, 20, 31680);
		} else {
			p->inter_tipo = INTER_MINIMO;
			p->inter = limita(n, 20, 31680);
		}
		return;
	}
	if (iguales(w, "slmult")) {
		if (n == 1 && p->inter_tipo == INTER_MINIMO) {
			p->inter_tipo = INTER_MULTIPLE;
			p->inter = limita((i64)p->inter * 100 / 240, 50, 1000);
		}
		return;
	}
	if (iguales(w, "tqr") || iguales(w, "tqc") || iguales(w, "tqdec") || iguales(w, "tldot") || iguales(w, "tlhyph") || iguales(w, "tlul") ||
	    iguales(w, "tx")) {
		/* las tabulaciones: lo de antes de \tx es de la que viene */
		static u8 tipo, relleno;

		if (iguales(w, "tqr")) tipo = TAB_DER;
		else if (iguales(w, "tqc")) tipo = TAB_CENTRO;
		else if (iguales(w, "tqdec")) tipo = TAB_DECIMAL;
		else if (iguales(w, "tldot")) relleno = '.';
		else if (iguales(w, "tlhyph")) relleno = '-';
		else if (iguales(w, "tlul")) relleno = '_';
		else {
			if (p->n_tabs < MAX_TABS) {
				p->tabs[p->n_tabs].pos = limita(n, 0, 31680);
				p->tabs[p->n_tabs].tipo = tipo;
				p->tabs[p->n_tabs].relleno = relleno;
				p->n_tabs++;
			}
			tipo = TAB_IZQ;
			relleno = 0;
		}
		return;
	}
	if (iguales(w, "pagebb")) { p->salto_antes = 1; return; }
	if (iguales(w, "keepn")) { p->juntos = 1; return; }
	if (iguales(w, "outlinelevel")) { p->nivel_titulo = (u8)limita((i64)n + 1, 0, 9); return; }
	if (iguales(w, "s")) {
		int st = estilo_de(l, n);

		p->estilo = (u16)st;
		if (st >= EST_T1 && st <= EST_T3 && !p->nivel_titulo)
			p->nivel_titulo = (u8)(st - EST_T1 + 1);
		return;
	}
	if (iguales(w, "ls")) { e->ls = limita(n, 0, 100000); return; }
	if (iguales(w, "ilvl")) { e->ilvl = limita(n, 0, 8); return; }
	if (iguales(w, "intbl")) { e->intbl = 1; return; }
	if (iguales(w, "par")) { parrafo_fin(l, 1); return; }
	if (iguales(w, "sect")) { parrafo_fin(l, 1); return; }
	if (iguales(w, "page")) {
		if (l->hay_texto && !e->intbl)
			parrafo_fin(l, 1);
		l->salto_pendiente = 1;
		return;
	}
	/* ---- las tablas ---- */
	if (iguales(w, "trowd")) {
		/* una fila que no sigue a la de antes (otra cosa en medio): la tabla de antes se acaba */
		l->n_def = 0;
		memset(&l->cel, 0, sizeof l->cel);
		l->cel.fondo = SIN_COLOR;
		l->tr_izq = 0;
		l->tr_alin = AL_IZQ;
		return;
	}
	if (iguales(w, "trleft")) { l->tr_izq = limita(n, -31680, 31680); return; }
	if (iguales(w, "trqc")) { l->tr_alin = AL_CENTRO; return; }
	if (iguales(w, "trqr")) { l->tr_alin = AL_DER; return; }
	if (iguales(w, "clmgf")) { l->cel.hmgf = 1; return; }
	if (iguales(w, "clmrg")) { l->cel.hmrg = 1; return; }
	if (iguales(w, "clvmgf")) { l->cel.vmgf = 1; return; }
	if (iguales(w, "clvmrg")) { l->cel.vmrg = 1; return; }
	if (iguales(w, "clcbpat")) { l->cel.fondo = color_de(l, n); return; }
	if (iguales(w, "brdrs") || iguales(w, "brdrth") || iguales(w, "brdrdb") || iguales(w, "brdrdot") || iguales(w, "brdrdash")) {
		l->bordes = 1;
		return;
	}
	if (iguales(w, "brdrw")) { l->grosor = limita(n, 0, 200); return; }
	if (iguales(w, "brdrnone") || iguales(w, "brdrnil")) return;
	if (iguales(w, "cellx")) {
		if (l->n_def < MAX_CELDAS) {
			l->cel.x = limita(n, -31680, 63360);
			l->def[l->n_def++] = l->cel;
		}
		memset(&l->cel, 0, sizeof l->cel);
		l->cel.fondo = SIN_COLOR;
		return;
	}
	if (iguales(w, "cell")) { celda_fin(l); return; }
	if (iguales(w, "row")) { fila_fin(l); return; }
	if (iguales(w, "nestcell") || iguales(w, "nestrow")) { celda_fin(l); return; }
	/* ---- el número de página ---- */
	if (iguales(w, "chpgn")) {
		parrafo *q = actual(l);

		if (q && (e->dest == D_TEXTO || e->dest == D_ENC || e->dest == D_PIE))
			par_inserta_uno(q, q->n, CAMPO_PAGINA, fmt_de(l));
		return;
	}
letras_especiales:
	/* ---- las letras que se escriben con una palabra ---- */
	if (iguales(w, "line")) { if (e->dest == D_TEXTO || e->dest == D_ENC || e->dest == D_PIE || e->dest == D_FLDRSLT) letra(l, SALTO_LINEA); return; }
	if (iguales(w, "tab")) { letra(l, '\t'); return; }
	if (iguales(w, "emdash")) { letra(l, 0x2014); return; }
	if (iguales(w, "endash")) { letra(l, 0x2013); return; }
	if (iguales(w, "bullet")) { letra(l, 0x2022); return; }
	if (iguales(w, "lquote")) { letra(l, 0x2018); return; }
	if (iguales(w, "rquote")) { letra(l, 0x2019); return; }
	if (iguales(w, "ldblquote")) { letra(l, 0x201C); return; }
	if (iguales(w, "rdblquote")) { letra(l, 0x201D); return; }
	if (iguales(w, "emspace") || iguales(w, "enspace") || iguales(w, "qmspace")) { letra(l, ' '); return; }
	if (iguales(w, "u")) {
		i32 v = n;

		if (v < 0)
			v += 65536;
		if (v >= 0xD800 && v < 0xDC00) {
			/* (la primera mitad de un par: se guarda hasta la otra) */
			l->alto_sur = v;
			l->saltar_uc = e->uc;
			return;
		}
		if (v >= 0xDC00 && v < 0xE000 && l->alto_sur >= 0xD800 && l->alto_sur < 0xDC00) {
			v = 0x10000 + ((l->alto_sur - 0xD800) << 10) + (v - 0xDC00);
			l->alto_sur = 0;
		} else if (v >= 0xD800 && v < 0xE000)
			v = 0xFFFD;
		letra(l, (u32)v);
		l->saltar_uc = e->uc;
		return;
	}
}

documento *rtf_lee(const u8 *s, u32 n, char *error, u32 cabe)
{
	lector *l;
	documento *d;
	char w[33];

	if (n < 5 || memcmp(s, "{\\rtf", 5)) {
		copia(error, "No es un RTF.", cabe);
		return NULL;
	}
	l = so_pide(sizeof *l);
	if (!l)
		return NULL;
	memset(l, 0, sizeof *l);
	d = doc_nuevo();
	if (!d) {
		so_suelta(l);
		return NULL;
	}
	l->s = s;
	l->n = n;
	l->d = d;
	l->pagina = 1252;
	l->e[0].dest = D_TEXTO;
	l->e[0].p = par_base();
	l->e[0].uc = 1;
	letra_por_defecto(&l->e[0], 0);
	l->medio = -1;
	doc_quita_parrafos(d, 0, d->n_par);
	arch_parrafo(d);
	while (l->i < l->n && !d->sin_memoria) {
		u8 c = s[l->i++];

		if (c == '{') {
			abre_grupo(l);
			continue;
		}
		if (c == '}') {
			cierra_grupo(l);
			continue;
		}
		if (l->salta_hondo)
			continue;
		if (c == '\\') {
			u8 d0;

			if (l->i >= l->n)
				break;
			d0 = s[l->i];
			if ((d0 >= 'a' && d0 <= 'z') || (d0 >= 'A' && d0 <= 'Z')) {
				int k = 0, neg = 0, hay_n = 0;
				i64 v = 0;

				while (l->i < l->n && ((s[l->i] >= 'a' && s[l->i] <= 'z') || (s[l->i] >= 'A' && s[l->i] <= 'Z'))) {
					if (k < 32)
						w[k++] = (char)s[l->i];
					l->i++;
				}
				w[k] = 0;
				if (l->i < l->n && s[l->i] == '-') {
					neg = 1;
					l->i++;
				}
				while (l->i < l->n && s[l->i] >= '0' && s[l->i] <= '9') {
					if (v < 100000000000LL)
						v = v * 10 + (s[l->i] - '0');
					hay_n = 1;
					l->i++;
				}
				if (l->i < l->n && s[l->i] == ' ')
					l->i++;
				if (neg)
					v = -v;
				if (v > 0x7FFFFFFF)
					v = 0x7FFFFFFF;
				if (v < -0x7FFFFFFF)
					v = -0x7FFFFFFF;
				if (iguales(w, "bin")) {
					/* datos en crudo: se saltan */
					u32 m = v > 0 ? (u32)v : 0;

					l->i = m > l->n - l->i ? l->n : l->i + m;
					continue;
				}
				/* lo que no es un salto de las letras pendientes de \u: cuenta como una */
				if (l->saltar_uc > 0 && !iguales(w, "u")) {
					l->saltar_uc--;
					continue;
				}
				control(l, w, hay_n, (i32)v);
				continue;
			}
			l->i++;
			switch (d0) {
			case '\'': {
				int a = l->i < l->n ? hex(s[l->i]) : -1, b = l->i + 1 < l->n ? hex(s[l->i + 1]) : -1;
				u8 v;

				if (a < 0 || b < 0)
					break;
				l->i += 2;
				v = (u8)(a * 16 + b);
				letra(l, l->pagina == 437 ? de437(v) : de1252(v));
				break;
			}
			case '*':
				/* un destino que se puede saltar: si no se conoce, se salta */
				if (l->i < l->n && s[l->i] == '\\') {
					u32 j = l->i + 1, k = 0;
					char nom[33];

					while (j < l->n && ((s[j] >= 'a' && s[j] <= 'z') || (s[j] >= 'A' && s[j] <= 'Z')) && k < 32)
						nom[k++] = (char)s[j++];
					nom[k] = 0;
					if (!iguales(nom, "shppict") && !iguales(nom, "listtable") && !iguales(nom, "listoverridetable") &&
					    !iguales(nom, "fldinst") && !iguales(nom, "pn") && !iguales(nom, "pntxta") && !iguales(nom, "pntxtb"))
						E(l)->dest = D_SALTA;
				}
				break;
			case '\\': case '{': case '}':
				letra(l, d0);
				break;
			case '~':
				letra(l, 0xA0);
				break;
			case '_':
				letra(l, 0x2011);
				break;
			case '-':
				break;	/* (el guion de partir: no se ve) */
			case '\n': case '\r':
				control(l, "par", 0, 0);
				break;
			case 't':
				break;
			default:
				break;
			}
			continue;
		}
		if (c == '\r' || c == '\n')
			continue;
		if (E(l)->dest == D_PICT) {
			int v = hex(c);

			if (v < 0)
				continue;
			if (l->medio < 0)
				l->medio = v;
			else {
				if (l->pict_tipo)
					buf_c(&l->pict, (u8)(l->medio * 16 + v));
				l->medio = -1;
				if (l->pict.n > ARCH_MAX)
					l->pict.mal = 1;
			}
			continue;
		}
		if (c == '\t') {
			letra(l, '\t');
			continue;
		}
		letra(l, l->pagina == 437 ? de437(c) : de1252(c));
	}
	/* lo que queda abierto */
	if (l->h > 0 && E(l)->dest == D_PICT)
		pict_fin(l);
	l->destino_par = NULL;
	if (l->celdas_fila)
		fila_fin(l);
	if (l->en_tabla)
		pon_tabla(l);
	buf_suelta(&l->pict);
	buf_suelta(&l->inst);
	so_suelta(l->filas);
	so_suelta(l);
	/* (las celdas que se hayan quedado sin tabla) */
	{
		u32 k;

		for (k = 0; k < d->n_par; k++)
			if (d->par[k].p.tabla == 0xFFFF)
				d->par[k].p.tabla = 0;
	}
	/* el párrafo vacío del final (el que deja el último \par), fuera */
	if (d->n_par > 1 && d->par[d->n_par - 1].n == 0 && !d->par[d->n_par - 2].p.tabla)
		doc_quita_parrafos(d, d->n_par - 1, 1);
	if (d->sin_memoria) {
		doc_suelta(d);
		copia(error, "No hay memoria para abrirlo.", cabe);
		return NULL;
	}
	if (!d->n_par)
		arch_parrafo(d);
	return d;
}

/* =========================================================================
 *  Escribir
 * ========================================================================= */
/* ¿lleva la palabra p? (sin leer de más) */
static int lleva(const char *s, const char *p)
{
	u32 n = largo(p), m = largo(s), i;

	for (i = 0; i + n <= m; i++)
		if (!memcmp(s + i, p, n))
			return 1;
	return 0;
}

typedef struct {
	bufer *b;
	documento *d;
	char *letras[MAX_LETRAS];
	int n_letras;
	u32 colores[MAX_COLORES];
	int n_colores;
} escritor;

static int num_letra(escritor *w, const char *f)
{
	int i;

	for (i = 0; i < w->n_letras; i++)
		if (iguales(w->letras[i], f))
			return i;
	if (w->n_letras >= MAX_LETRAS)
		return 0;
	w->letras[w->n_letras] = (char *)f;
	return w->n_letras++;
}

static int num_color(escritor *w, u32 c)
{
	int i;

	if (c == SIN_COLOR)
		return 0;
	for (i = 1; i < w->n_colores; i++)
		if (w->colores[i] == c)
			return i;
	if (w->n_colores >= MAX_COLORES)
		return 0;
	w->colores[w->n_colores] = c;
	return w->n_colores++;
}

/* un texto suelto (el nombre de una letra, el título): ASCII con \'hh y \uN */
static void pon_txt(bufer *b, const u32 *t, u32 n)
{
	u32 k;

	for (k = 0; k < n; k++) {
		u32 c = t[k];
		int x;

		if (c == '\\' || c == '{' || c == '}') {
			buf_c(b, '\\');
			buf_c(b, (u8)c);
		} else if (c == '\t')
			buf_txt(b, "\\tab ");
		else if (c == SALTO_LINEA)
			buf_txt(b, "\\line ");
		else if (c == 0xA0)
			buf_txt(b, "\\~");
		else if (c >= 0x20 && c < 0x80)
			buf_c(b, (u8)c);
		else if (c < 0x20)
			continue;
		else if ((x = a1252(c)) != 0)
			buf_fmt(b, "\\'%02x", x);
		else if (c > 0xFFFF) {
			u32 v = c - 0x10000, a = 0xD800 + (v >> 10), z = 0xDC00 + (v & 0x3FF);

			buf_fmt(b, "\\u%d?\\u%d?", (int)a - 65536, (int)z - 65536);
		} else
			buf_fmt(b, "\\u%d?", c > 32767 ? (int)c - 65536 : (int)c);
	}
}

static void pon_txt8(bufer *b, const char *s)
{
	u32 n, *t = u32_de_utf8(s, largo(s), &n);

	if (t) {
		pon_txt(b, t, n);
		so_suelta(t);
	}
}

static void pon_fmt(escritor *w, const fmt_letra *f)
{
	bufer *b = w->b;

	buf_fmt(b, "\\plain\\f%d\\fs%u", num_letra(w, f->familia), f->tam);
	if (f->negrita)
		buf_txt(b, "\\b");
	if (f->cursiva)
		buf_txt(b, "\\i");
	if (f->subrayado)
		buf_txt(b, "\\ul");
	if (f->tachado)
		buf_txt(b, "\\strike");
	if (f->vert == 1)
		buf_txt(b, "\\super");
	if (f->vert == 2)
		buf_txt(b, "\\sub");
	if (f->versalitas)
		buf_txt(b, "\\scaps");
	if (f->color != SIN_COLOR)
		buf_fmt(b, "\\cf%d", num_color(w, f->color));
	if (f->resalte != SIN_COLOR)
		buf_fmt(b, "\\highlight%d", num_color(w, f->resalte));
	buf_c(b, ' ');
}

static void pon_imagen(escritor *w, int i)
{
	documento *d = w->d;
	imagen *m;
	const u8 *datos;
	u32 n, k;
	bufer png = { 0 };
	int es_png = 1;

	if (i < 0 || (u32)i >= d->n_img)
		return;
	m = &d->img[i];
	datos = m->datos;
	n = m->n;
	if (m->tipo == IMG_JPG)
		es_png = 0;
	else if (m->tipo != IMG_PNG) {
		/* BMP y GIF: a PNG */
		u8 *rgba;
		int j;

		if (!doc_imagen_px(d, i))
			return;
		rgba = so_pide((size_t)m->px_an * m->px_al * 4);
		if (!rgba)
			return;
		for (j = 0; j < m->px_an * m->px_al; j++) {
			u32 v = m->px[j];

			rgba[4 * j] = (u8)(v >> 16);
			rgba[4 * j + 1] = (u8)(v >> 8);
			rgba[4 * j + 2] = (u8)v;
			rgba[4 * j + 3] = (u8)(v >> 24);
		}
		{
			int sl;
			unsigned char *z = stbi_write_png_to_mem(rgba, m->px_an * 4, m->px_an, m->px_al, 4, &sl);

			so_suelta(rgba);
			if (!z)
				return;
			buf_pon(&png, z, (u32)sl);
			so_suelta(z);
		}
		datos = png.d;
		n = png.n;
	}
	buf_fmt(w->b, "{\\*\\shppict{\\pict%s\\picw%d\\pich%d\\picwgoal%d\\pichgoal%d\n", es_png ? "\\pngblip" : "\\jpegblip",
		m->px_an, m->px_al, m->ancho, m->alto);
	for (k = 0; k < n; k++) {
		buf_fmt(w->b, "%02x", datos[k]);
		if ((k & 63) == 63)
			buf_c(w->b, '\n');
	}
	buf_txt(w->b, "}}");
	buf_suelta(&png);
}

/* el texto de un párrafo, con sus cambios de formato */
static void pon_texto(escritor *w, const parrafo *p)
{
	documento *d = w->d;
	u32 k;
	int antes = -1;

	for (k = 0; k < p->n; k++) {
		u32 c = p->t[k];

		if (p->f[k] != antes && p->f[k] < d->n_fl) {
			pon_fmt(w, &d->fl[p->f[k]]);
			antes = p->f[k];
		}
		if (es_imagen(c))
			pon_imagen(w, (int)(c - IMAGEN_0));
		else if (c == CAMPO_PAGINA)
			buf_txt(w->b, "{\\field{\\*\\fldinst PAGE}{\\fldrslt 1}}");
		else if (c == CAMPO_PAGINAS)
			buf_txt(w->b, "{\\field{\\*\\fldinst NUMPAGES}{\\fldrslt 1}}");
		else if (es_objeto(c))
			continue;
		else
			pon_txt(w->b, &c, 1);
	}
}

static void pon_par(escritor *w, const fmt_parrafo *p, int en_tabla)
{
	bufer *b = w->b;
	u32 k;

	buf_txt(b, "\\pard");
	if (en_tabla)
		buf_txt(b, "\\intbl");
	if (p->estilo && p->estilo < w->d->n_est)
		buf_fmt(b, "\\s%u", p->estilo);
	switch (p->alin) {
	case AL_CENTRO: buf_txt(b, "\\qc"); break;
	case AL_DER: buf_txt(b, "\\qr"); break;
	case AL_JUST: buf_txt(b, "\\qj"); break;
	default: buf_txt(b, "\\ql"); break;
	}
	if (p->sang_izq)
		buf_fmt(b, "\\li%d", p->sang_izq);
	if (p->sang_der)
		buf_fmt(b, "\\ri%d", p->sang_der);
	if (p->sang_primera)
		buf_fmt(b, "\\fi%d", p->sang_primera);
	if (p->antes)
		buf_fmt(b, "\\sb%d", p->antes);
	if (p->despues)
		buf_fmt(b, "\\sa%d", p->despues);
	if (p->inter_tipo == INTER_EXACTO)
		buf_fmt(b, "\\sl-%d\\slmult0", p->inter);
	else if (p->inter_tipo == INTER_MINIMO)
		buf_fmt(b, "\\sl%d\\slmult0", p->inter);
	else if (p->inter && p->inter != 100)
		buf_fmt(b, "\\sl%d\\slmult1", p->inter * 240 / 100);
	for (k = 0; k < p->n_tabs && k < MAX_TABS; k++) {
		const tabulacion *t = &p->tabs[k];

		if (t->relleno == '.')
			buf_txt(b, "\\tldot");
		else if (t->relleno == '-')
			buf_txt(b, "\\tlhyph");
		else if (t->relleno == '_')
			buf_txt(b, "\\tlul");
		if (t->tipo == TAB_DER)
			buf_txt(b, "\\tqr");
		else if (t->tipo == TAB_CENTRO)
			buf_txt(b, "\\tqc");
		else if (t->tipo == TAB_DECIMAL)
			buf_txt(b, "\\tqdec");
		buf_fmt(b, "\\tx%d", t->pos);
	}
	if (p->salto_antes && !en_tabla)
		buf_txt(b, "\\pagebb");
	if (p->juntos)
		buf_txt(b, "\\keepn");
	if (p->nivel_titulo)
		buf_fmt(b, "\\outlinelevel%u", p->nivel_titulo - 1);
	buf_c(b, ' ');
}

/* la lista, a la manera de antes (\pn: la leen Word y LibreOffice) */
static void pon_lista(escritor *w, u32 i)
{
	const parrafo *p = &w->d->par[i];
	u32 m[16];

	if (!p->p.lista)
		return;
	maq_marca_lista(w->d, i, m, 16);
	buf_txt(w->b, "{\\pntext ");
	pon_txt(w->b, m, u32_largo(m));
	buf_txt(w->b, "\\tab}");
	if (p->p.lista == LISTA_VINETAS)
		buf_fmt(w->b, "{\\*\\pn\\pnlvlblt\\pnf%d\\pnindent360{\\pntxtb\\'95}}", num_letra(w, FAM_SANS));
	else
		buf_fmt(w->b, "{\\*\\pn\\pnlvlbody%s\\pnstart1\\pnindent360{\\pntxta %s}}",
			p->p.lista == LISTA_LETRAS ? "\\pnlcltr" : p->p.lista == LISTA_ROMANOS ? "\\pnlcrm" : "\\pndec",
			p->p.lista == LISTA_LETRAS ? ")" : ".");
}

/* quién tapa la celda (fila, col): la que manda, o -1 */
static int quien_tapa(documento *d, u32 ini, const tabla *tb, u32 f, u32 c)
{
	int rr;

	for (rr = (int)f; rr >= 0; rr--) {
		u32 cc;

		for (cc = 0; cc <= c; cc++) {
			const parrafo *q = &d->par[ini + (u32)rr * tb->cols + cc];
			u32 nc = q->p.abarca_cols ? q->p.abarca_cols : 1, nf = q->p.abarca_filas ? q->p.abarca_filas : 1;

			if (q->p.cubierta)
				continue;
			if (cc + nc > c && (u32)rr + nf > f && !((u32)rr == f && cc == c))
				return (int)(ini + (u32)rr * tb->cols + cc);
		}
	}
	return -1;
}

static void pon_tabla_rtf(escritor *w, u32 ini, u32 n)
{
	documento *d = w->d;
	const tabla *tb = &d->tab[d->par[ini].p.tabla - 1];
	u32 f, c;
	i32 izq = 0, total = 0;
	int bordes = tb->bordes != BORDE_NO;

	for (c = 0; c < tb->cols; c++)
		total += tb->ancho[c];
	(void)total;
	for (f = 0; f < n / tb->cols; f++) {
		i32 x = izq;
		/* la definición de la fila */
		buf_fmt(w->b, "\\trowd\\trgaph%d\\trleft%d%s", tb->margen, izq, tb->alin == AL_CENTRO ? "\\trqc" : tb->alin == AL_DER ? "\\trqr" : "");
		for (c = 0; c < tb->cols;) {
			const parrafo *q = &d->par[ini + f * tb->cols + c];
			u32 span = 1, k;

			if (q->p.cubierta) {
				int a = quien_tapa(d, ini, tb, f, c);

				if (a >= 0 && d->par[a].p.fila < f) {
					/* tapada por la de arriba: \clvmrg, tan ancha como aquella */
					const parrafo *an = &d->par[a];

					span = an->p.abarca_cols ? an->p.abarca_cols : 1;
					if (an->p.col + span <= c)
						span = 1;
					else
						span -= c - an->p.col;
					buf_txt(w->b, "\\clvmrg");
					if (an->p.fondo != SIN_COLOR)
						buf_fmt(w->b, "\\clcbpat%d", num_color(w, an->p.fondo));
				} else if (a >= 0) {
					/* (tapada por la de su izquierda: ya va en aquella) */
					c++;
					continue;
				}
			} else {
				span = q->p.abarca_cols ? q->p.abarca_cols : 1;
				if (q->p.abarca_filas > 1)
					buf_txt(w->b, "\\clvmgf");
				if (q->p.fondo != SIN_COLOR)
					buf_fmt(w->b, "\\clcbpat%d", num_color(w, q->p.fondo));
			}
			if (bordes)
				buf_fmt(w->b, "\\clbrdrt\\brdrs\\brdrw%u\\clbrdrl\\brdrs\\brdrw%u\\clbrdrb\\brdrs\\brdrw%u\\clbrdrr\\brdrs\\brdrw%u",
					tb->grosor, tb->grosor, tb->grosor, tb->grosor);
			for (k = 0; k < span && c + k < tb->cols; k++)
				x += tb->ancho[c + k];
			buf_fmt(w->b, "\\cellx%d", x);
			c += span;
		}
		buf_c(w->b, '\n');
		/* las celdas */
		for (c = 0; c < tb->cols;) {
			u32 i = ini + f * tb->cols + c;
			const parrafo *q = &d->par[i];
			u32 span = 1;

			if (q->p.cubierta) {
				int a = quien_tapa(d, ini, tb, f, c);

				if (a >= 0 && d->par[a].p.fila == f) {
					c++;
					continue;
				}
				if (a >= 0) {
					const parrafo *an = &d->par[a];

					span = an->p.abarca_cols ? an->p.abarca_cols : 1;
					span = an->p.col + span > c ? span - (c - an->p.col) : 1;
				}
				buf_txt(w->b, "\\pard\\intbl \\cell\n");
				c += span;
				continue;
			}
			span = q->p.abarca_cols ? q->p.abarca_cols : 1;
			pon_par(w, &q->p, 1);
			pon_texto(w, q);
			buf_txt(w->b, "\\cell\n");
			c += span;
		}
		buf_txt(w->b, "\\row\n");
	}
}

int rtf_escribe(documento *d, bufer *b)
{
	escritor w;
	bufer cuerpo = { 0 };
	u32 i;
	int k;

	memset(&w, 0, sizeof w);
	w.d = d;
	w.n_colores = 1;
	num_letra(&w, d->n_fl ? d->fl[0].familia : FAM_SERIF);
	/* primero el cuerpo (así se saben las letras y los colores) */
	w.b = &cuerpo;
	buf_fmt(&cuerpo, "\\paperw%d\\paperh%d\\margl%d\\margr%d\\margt%d\\margb%d\\headery%d\\footery%d", d->pag_an, d->pag_al,
		d->marg_izq, d->marg_der, d->marg_sup, d->marg_inf, d->dist_enc, d->dist_pie);
	if (d->num_inicio != 1)
		buf_fmt(&cuerpo, "\\pgnstarts%u\\pgnrestart", d->num_inicio);
	if (d->enc_no_primera)
		buf_txt(&cuerpo, "\\titlepg");
	buf_txt(&cuerpo, "\\sectd\n");
	if (d->hay_enc && d->enc.n) {
		buf_txt(&cuerpo, "{\\header ");
		pon_par(&w, &d->enc.p, 0);
		pon_texto(&w, &d->enc);
		buf_txt(&cuerpo, "\\par}\n");
	}
	if (d->hay_pie && d->pie.n) {
		buf_txt(&cuerpo, "{\\footer ");
		pon_par(&w, &d->pie.p, 0);
		pon_texto(&w, &d->pie);
		buf_txt(&cuerpo, "\\par}\n");
	}
	for (i = 0; i < d->n_par;) {
		const parrafo *p = &d->par[i];

		if (p->p.tabla && p->p.tabla <= d->n_tab) {
			u32 n;

			doc_tabla_ini(d, i, &n);
			if (n % d->tab[p->p.tabla - 1].cols == 0) {
				pon_tabla_rtf(&w, i, n);
				i += n;
				continue;
			}
		}
		pon_par(&w, &p->p, 0);
		pon_lista(&w, i);
		pon_texto(&w, p);
		/* el formato del final, antes del \par (es el de la marca de párrafo) */
		buf_txt(&cuerpo, "\\par\n");
		i++;
	}
	/* la cabecera */
	buf_txt(b, "{\\rtf1\\ansi\\ansicpg1252\\deff0\\uc1\n{\\fonttbl");
	for (k = 0; k < w.n_letras; k++) {
		const char *f = w.letras[k];
		const char *fam = lleva(f, "Mono") || lleva(f, "Courier") ? "\\fmodern" : lleva(f, "Sans") || lleva(f, "Arial") ? "\\fswiss" : "\\froman";

		buf_fmt(b, "{\\f%d%s\\fcharset0 ", k, fam);
		pon_txt8(b, f);
		buf_txt(b, ";}");
	}
	buf_txt(b, "}\n{\\colortbl;");
	for (k = 1; k < w.n_colores; k++)
		buf_fmt(b, "\\red%u\\green%u\\blue%u;", w.colores[k] >> 16 & 255, w.colores[k] >> 8 & 255, w.colores[k] & 255);
	buf_txt(b, "}\n{\\stylesheet");
	for (i = 0; i < d->n_est; i++) {
		static const char *const ingles[EST_N] = { "Normal", "Title", "heading 1", "heading 2", "heading 3", "Subtitle", "Quote",
			"List Paragraph", "header", "footer", "footnote text", "No Spacing" };

		buf_fmt(b, "{\\s%u ", i);
		/* (los de serie, con su nombre de Word, que es el que entienden todos) */
		if (i < EST_N && d->est[i].de_serie)
			buf_txt(b, ingles[i]);
		else
			pon_txt8(b, d->est[i].nombre);
		buf_txt(b, ";}");
	}
	buf_txt(b, "}\n");
	if (d->titulo[0] || d->autor[0] || d->asunto[0]) {
		buf_txt(b, "{\\info");
		if (d->titulo[0]) {
			buf_txt(b, "{\\title ");
			pon_txt8(b, d->titulo);
			buf_txt(b, "}");
		}
		if (d->autor[0]) {
			buf_txt(b, "{\\author ");
			pon_txt8(b, d->autor);
			buf_txt(b, "}");
		}
		if (d->asunto[0]) {
			buf_txt(b, "{\\subject ");
			pon_txt8(b, d->asunto);
			buf_txt(b, "}");
		}
		buf_txt(b, "}\n");
	}
	buf_pon(b, cuerpo.d, cuerpo.n);
	if (cuerpo.mal)
		b->mal = 1;
	buf_suelta(&cuerpo);
	buf_txt(b, "}\n");
	return b->mal ? -1 : 0;
}
