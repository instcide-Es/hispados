/* pinta.c - Hispa-PDF: pintar las páginas (para las miniaturas, para verlas
 * y para «PDF IMAGENES») y recorrer su contenido (también para sacar el
 * texto: texto.c). Un pintor sencillo pero entero: caminos rellenos y
 * trazados (con suavizado: 4 subfilas por fila y lo que cubre cada punto a
 * lo ancho), recortes, imágenes (las JPEG con stb_image; las de Flate y
 * demás de 1 a 16 bits, en gris, RGB, CMYK, indexadas, máscaras y
 * transparencias), degradados (axiales y radiales), las letras (las que
 * trae el PDF si son TrueType u OpenType; si no, DejaVu con el ancho de la
 * suya) y las Type3. Lo que no se sabe pintar (JPX, JBIG2, CCITT, los
 * patrones de mosaico) sale en gris, sin pararse.
 * Todo con tope: lo anidado (q, los formularios), las órdenes de una
 * página y lo que mide una imagen.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "fuente.h"
#include "pinta.h"

#define SS 4			/* las subfilas de cada fila */
#define MAX_PILA 48		/* operandos */
#define MAX_Q 64		/* q dentro de q */
#define MAX_FORMAS 10		/* formularios y Type3, uno dentro de otro */
#define MAX_ORDENES 4000000	/* las órdenes de una página (con lo de dentro) */
u32 hpdf_max_pixeles = 24u * 1024 * 1024;	/* lo que mide una imagen que se pinta */

/* ---- las matrices ---- */
static pmat mul(pmat a, pmat b)		/* primero a, luego b */
{
	pmat r;

	r.a = a.a * b.a + a.b * b.c;
	r.b = a.a * b.b + a.b * b.d;
	r.c = a.c * b.a + a.d * b.c;
	r.d = a.c * b.b + a.d * b.d;
	r.e = a.e * b.a + a.f * b.c + b.e;
	r.f = a.e * b.b + a.f * b.d + b.f;
	return r;
}

static void aplica(const pmat *m, double x, double y, double *sx, double *sy)
{
	*sx = m->a * x + m->c * y + m->e;
	*sy = m->b * x + m->d * y + m->f;
}

static int inversa(pmat m, pmat *r)
{
	double det = m.a * m.d - m.b * m.c;

	if (det > -1e-12 && det < 1e-12)
		return 0;
	r->a = m.d / det;
	r->b = -m.b / det;
	r->c = -m.c / det;
	r->d = m.a / det;
	r->e = -(m.e * r->a + m.f * r->c);
	r->f = -(m.e * r->b + m.f * r->d);
	return 1;
}

static pmat matriz(double a, double b, double c, double d, double e, double f)
{
	pmat m;

	m.a = a; m.b = b; m.c = c; m.d = d; m.e = e; m.f = f;
	return m;
}

/* ---- el camino (ya en puntos de la imagen) ---- */
static void cam_vacia(pcamino *c)
{
	c->n = 0;
	c->n_sub = 0;
	c->abierto = 0;
}

static void cam_punto(pcamino *c, double x, double y, int nuevo)
{
	if (c->n >= c->cap) {
		if (c->cap >= 4u * 1024 * 1024)
			return;
		c->cap = c->cap ? c->cap * 2 : 256;
		c->x = repide(c->x, c->cap * sizeof(double));
		c->y = repide(c->y, c->cap * sizeof(double));
		c->ini = repide(c->ini, c->cap);
	}
	if (x != x || y != y)
		x = y = 0;
	if (x > 1e6) x = 1e6;
	if (x < -1e6) x = -1e6;
	if (y > 1e6) y = 1e6;
	if (y < -1e6) y = -1e6;
	c->x[c->n] = x;
	c->y[c->n] = y;
	c->ini[c->n] = (u8)nuevo;	/* 1: empieza un trozo; 2: y está cerrado (lo pone cierra) */
	c->n++;
}

static void cam_cierra(pcamino *c)
{
	int i;

	/* el primero del trozo, marcado de cerrado */
	for (i = (int)c->n - 1; i >= 0; i--)
		if (c->ini[i] & 1) {
			c->ini[i] |= 2;
			break;
		}
}

/* una curva de Bézier, en rayas (cuantas hagan falta para lo que mide) */
static void cam_curva(pcamino *c, double x0, double y0, double x1, double y1, double x2, double y2, double x3, double y3)
{
	double l = so_abs(x1 - x0) + so_abs(y1 - y0) + so_abs(x2 - x1) + so_abs(y2 - y1) + so_abs(x3 - x2) + so_abs(y3 - y2);
	int n = (int)(so_raiz(l) * 1.5) + 2, i;

	if (n > 64)
		n = 64;
	for (i = 1; i <= n; i++) {
		double t = (double)i / n, u = 1 - t;
		double a = u * u * u, b = 3 * u * u * t, cc = 3 * u * t * t, d = t * t * t;

		cam_punto(c, a * x0 + b * x1 + cc * x2 + d * x3, a * y0 + b * y1 + cc * y2 + d * y3, 0);
	}
}

/* ---- rellenar: las aristas, por subfilas (con la lista de las activas), y
 * lo que cubre cada punto a lo ancho ---- */
typedef struct {
	double x0, y0, x1, y1, dxdy;
	int dir;
} parista;

static int por_y(const void *a, const void *b)
{
	const parista *p = a, *q = b;

	return p->y0 < q->y0 ? -1 : p->y0 > q->y0;
}

/* ordenar (Shell, con los huecos de Ciura): no hay qsort en el anillo 3 */
static void ordena(void *v, u32 n, u32 t, int (*cmp)(const void *, const void *))
{
	static const u32 huecos[] = {1, 4, 10, 23, 57, 132, 301, 701, 1750, 4375, 10937, 27343, 68359, 170898, 427245};
	u8 *a = v, tmp[64];
	int h;

	for (h = 14; h >= 0; h--) {
		u32 hueco = huecos[h], i, j;

		if (hueco >= n)
			continue;
		for (i = hueco; i < n; i++) {
			memcpy(tmp, a + i * t, t);
			for (j = i; j >= hueco && cmp(a + (j - hueco) * t, tmp) > 0; j -= hueco)
				memcpy(a + j * t, a + (j - hueco) * t, t);
			memcpy(a + j * t, tmp, t);
		}
	}
}

typedef void (*pfila)(void *u, int y, int x0, int x1, const u8 *cob);

/* lo que cubre el camino c en cada punto (dentro del rectángulo r): llama
 * a «fila» con cada fila que tenga algo */
static void cubre(pcamino *c, prect r, int par_impar, pfila fila, void *u)
{
	parista *ar;
	u32 na = 0, i, *act, nact = 0, sig = 0;
	double ymin = 1e9, ymax = -1e9, xmin = 1e9, xmax = -1e9;
	int y0, y1, x0, x1, y;
	double *cruces;
	int *dirs;
	float *acu;
	u8 *cob;

	if (c->n < 2)
		return;
	ar = pide(c->n * sizeof(parista) + sizeof(parista));
	for (i = 0; i < c->n;) {
		u32 j = i + 1, a;

		while (j < c->n && !(c->ini[j] & 1))
			j++;
		for (a = i; a < j; a++) {
			u32 b = a + 1 < j ? a + 1 : i;
			double xa = c->x[a], ya = c->y[a], xb = c->x[b], yb = c->y[b];
			parista *e;

			if (xa < xmin) xmin = xa;
			if (xa > xmax) xmax = xa;
			if (ya < ymin) ymin = ya;
			if (ya > ymax) ymax = ya;
			if (ya == yb)
				continue;
			e = &ar[na++];
			if (ya < yb) {
				e->x0 = xa; e->y0 = ya; e->x1 = xb; e->y1 = yb; e->dir = 1;
			} else {
				e->x0 = xb; e->y0 = yb; e->x1 = xa; e->y1 = ya; e->dir = -1;
			}
			e->dxdy = (e->x1 - e->x0) / (e->y1 - e->y0);
		}
		i = j;
	}
	y0 = (int)so_suelo(ymin);
	y1 = (int)so_techo(ymax);
	x0 = (int)so_suelo(xmin);
	x1 = (int)so_techo(xmax) + 1;
	if (y0 < r.y0) y0 = r.y0;
	if (y1 > r.y1) y1 = r.y1;
	if (x0 < r.x0) x0 = r.x0;
	if (x1 > r.x1) x1 = r.x1;
	if (na == 0 || y0 >= y1 || x0 >= x1) {
		so_suelta(ar);
		return;
	}
	ordena(ar, na, sizeof(parista), por_y);
	act = pide(na * sizeof(u32));
	cruces = pide(na * sizeof(double));
	dirs = pide(na * sizeof(int));
	acu = pide((size_t)(x1 - x0 + 2) * sizeof(float));
	cob = pide((size_t)(x1 - x0 + 2));
	/* (las que acaban antes de la primera fila, fuera ya) */
	while (sig < na && ar[sig].y0 < y0 && 0)
		sig++;
	for (y = y0; y < y1; y++) {
		int s, hay = 0;

		memset(acu, 0, (size_t)(x1 - x0 + 2) * sizeof(float));
		for (s = 0; s < SS; s++) {
			double sy = y + (s + 0.5) / SS;
			u32 nc = 0, k, q;
			int w = 0;

			/* las que empiezan, dentro; las que acabaron, fuera */
			while (sig < na && ar[sig].y0 <= sy)
				act[nact++] = sig++;
			for (k = 0, q = 0; k < nact; k++)
				if (ar[act[k]].y1 > sy)
					act[q++] = act[k];
			nact = q;
			for (k = 0; k < nact; k++) {
				parista *e = &ar[act[k]];
				double x = e->x0 + (sy - e->y0) * e->dxdy;
				u32 m = nc;

				while (m > 0 && cruces[m - 1] > x) {
					cruces[m] = cruces[m - 1];
					dirs[m] = dirs[m - 1];
					m--;
				}
				cruces[m] = x;
				dirs[m] = e->dir;
				nc++;
			}
			for (k = 0; k + 1 < nc; k++) {
				double a, b;
				int ia, ib;

				w += dirs[k];
				if (par_impar ? !(w & 1) : w == 0)
					continue;
				a = cruces[k] - x0;
				b = cruces[k + 1] - x0;
				if (b <= 0 || a >= x1 - x0 || b <= a)
					continue;
				if (a < 0)
					a = 0;
				if (b > x1 - x0)
					b = x1 - x0;
				ia = (int)a;
				ib = (int)b;
				if (ia == ib)
					acu[ia] += (float)((b - a) / SS);
				else {
					int t;

					acu[ia] += (float)((ia + 1 - a) / SS);
					for (t = ia + 1; t < ib; t++)
						acu[t] += 1.0f / SS;
					if (ib < x1 - x0)
						acu[ib] += (float)((b - ib) / SS);
				}
				hay = 1;
			}
		}
		if (hay) {
			int q, a = -1, b = -1;

			for (q = 0; q < x1 - x0; q++) {
				float v = acu[q];

				cob[q] = (u8)(v >= 0.999f ? 255 : v <= 0 ? 0 : (int)(v * 255 + 0.5f));
				if (cob[q]) {
					if (a < 0)
						a = q;
					b = q;
				}
			}
			if (a >= 0)
				fila(u, y, x0 + a, x0 + b + 1, cob + a);
		}
		if (sig >= na && nact == 0)
			break;
	}
	so_suelta(cob);
	so_suelta(acu);
	so_suelta(dirs);
	so_suelta(cruces);
	so_suelta(act);
	so_suelta(ar);
}

/* ---- las funciones (tipos 0, 2, 3 y 4) ---- */
static int funcion(ppintor *p, pobj *fn, const double *ent, int ne, double *sal, int max, int hondo);

/* la calculadora de PostScript (tipo 4): sin bucles, con su pila con tope */
typedef struct { const u8 *p, *fin; } pps;

static int ps_ejecuta(const u8 *p, const u8 *fin, double *pila, int *n, int hondo)
{
	plex l;
	ptok t;

	if (hondo > 16)
		return 0;
	l.ini = l.p = p;
	l.fin = fin;
	for (;;) {
		lex_token(&l, &t);
		if (t.t == T_FIN || t.t == T_CIERRA_L)
			return 1;
		if (t.t == T_ENT || t.t == T_REAL) {
			if (*n >= 100)
				return 0;
			pila[(*n)++] = t.t == T_ENT ? t.i : t.r;
			continue;
		}
		if (t.t == T_ABRE_L) {
			/* uno o dos bloques y luego if o ifelse */
			const u8 *b1 = l.p, *e1, *b2 = NULL, *e2 = NULL;
			int prof = 1;
			ptok u;

			while (prof && lex_token(&l, &u) != T_FIN) {
				if (u.t == T_ABRE_L) prof++;
				if (u.t == T_CIERRA_L) prof--;
			}
			e1 = l.p;
			{
				const u8 *antes = l.p;

				lex_token(&l, &u);
				if (u.t == T_ABRE_L) {
					b2 = l.p;
					prof = 1;
					while (prof && lex_token(&l, &u) != T_FIN) {
						if (u.t == T_ABRE_L) prof++;
						if (u.t == T_CIERRA_L) prof--;
					}
					e2 = l.p;
					lex_token(&l, &u);
				}
				if (tok_es(&u, "if")) {
					if (*n < 1)
						return 0;
					if (pila[--(*n)] != 0 && !ps_ejecuta(b1, e1, pila, n, hondo + 1))
						return 0;
				} else if (tok_es(&u, "ifelse") && b2) {
					if (*n < 1)
						return 0;
					{
						int cierto = pila[--(*n)] != 0;

						if (!ps_ejecuta(cierto ? b1 : b2, cierto ? e1 : e2, pila, n, hondo + 1))
							return 0;
					}
				} else
					l.p = antes;
			}
			continue;
		}
		if (t.t != T_PALABRA)
			continue;
#define UNO(e) do { if (*n < 1) return 0; { double a = pila[*n - 1]; pila[*n - 1] = (e); (void)a; } } while (0)
#define DOS(e) do { if (*n < 2) return 0; { double a = pila[*n - 2], b = pila[*n - 1]; (*n)--; pila[*n - 1] = (e); (void)a; (void)b; } } while (0)
		if (tok_es(&t, "add")) DOS(a + b);
		else if (tok_es(&t, "sub")) DOS(a - b);
		else if (tok_es(&t, "mul")) DOS(a * b);
		else if (tok_es(&t, "div")) DOS(b != 0 ? a / b : 0);
		else if (tok_es(&t, "idiv")) DOS((i32)b != 0 ? (double)((i32)a / (i32)b) : 0);
		else if (tok_es(&t, "mod")) DOS((i32)b != 0 ? (double)((i32)a % (i32)b) : 0);
		else if (tok_es(&t, "neg")) UNO(-a);
		else if (tok_es(&t, "abs")) UNO(a < 0 ? -a : a);
		else if (tok_es(&t, "ceiling")) UNO(so_techo(a));
		else if (tok_es(&t, "floor")) UNO(so_suelo(a));
		else if (tok_es(&t, "round")) UNO(so_suelo(a + 0.5));
		else if (tok_es(&t, "truncate")) UNO(a < 0 ? so_techo(a) : so_suelo(a));
		else if (tok_es(&t, "sqrt")) UNO(a > 0 ? so_raiz(a) : 0);
		else if (tok_es(&t, "sin")) UNO(so_sen(a * 3.14159265358979 / 180));
		else if (tok_es(&t, "cos")) UNO(so_cos(a * 3.14159265358979 / 180));
		else if (tok_es(&t, "atan")) DOS(so_atan2(a, b) * 180 / 3.14159265358979 + (so_atan2(a, b) < 0 ? 360 : 0));
		else if (tok_es(&t, "exp")) DOS(so_pot(a, b));
		else if (tok_es(&t, "ln")) UNO(a > 0 ? so_ln(a) : 0);
		else if (tok_es(&t, "log")) UNO(a > 0 ? so_ln(a) / 2.302585092994046 : 0);
		else if (tok_es(&t, "cvi")) UNO((double)(i32)a);
		else if (tok_es(&t, "cvr")) UNO(a);
		else if (tok_es(&t, "eq")) DOS(a == b);
		else if (tok_es(&t, "ne")) DOS(a != b);
		else if (tok_es(&t, "gt")) DOS(a > b);
		else if (tok_es(&t, "ge")) DOS(a >= b);
		else if (tok_es(&t, "lt")) DOS(a < b);
		else if (tok_es(&t, "le")) DOS(a <= b);
		else if (tok_es(&t, "and")) DOS((double)((i32)a & (i32)b));
		else if (tok_es(&t, "or")) DOS((double)((i32)a | (i32)b));
		else if (tok_es(&t, "xor")) DOS((double)((i32)a ^ (i32)b));
		else if (tok_es(&t, "not")) UNO(a == 0 || a == 1 ? (double)(a == 0) : (double)~(i32)a);
		else if (tok_es(&t, "bitshift")) DOS((i32)b >= 0 ? (double)((i32)a << ((i32)b & 31)) : (double)((i32)a >> ((-(i32)b) & 31)));
		else if (tok_es(&t, "true")) { if (*n >= 100) return 0; pila[(*n)++] = 1; }
		else if (tok_es(&t, "false")) { if (*n >= 100) return 0; pila[(*n)++] = 0; }
		else if (tok_es(&t, "pop")) { if (*n < 1) return 0; (*n)--; }
		else if (tok_es(&t, "dup")) { if (*n < 1 || *n >= 100) return 0; pila[*n] = pila[*n - 1]; (*n)++; }
		else if (tok_es(&t, "exch")) { double x; if (*n < 2) return 0; x = pila[*n - 1]; pila[*n - 1] = pila[*n - 2]; pila[*n - 2] = x; }
		else if (tok_es(&t, "copy")) {
			int k, m;
			if (*n < 1) return 0;
			m = (int)pila[--(*n)];
			if (m < 0 || m > *n || *n + m > 100) return 0;
			for (k = 0; k < m; k++) pila[*n + k] = pila[*n - m + k];
			*n += m;
		} else if (tok_es(&t, "index")) {
			int m;
			if (*n < 1) return 0;
			m = (int)pila[*n - 1];
			if (m < 0 || m + 1 >= *n) return 0;
			pila[*n - 1] = pila[*n - 2 - m];
		} else if (tok_es(&t, "roll")) {
			int m, j, k;
			if (*n < 2) return 0;
			j = (int)pila[--(*n)];
			m = (int)pila[--(*n)];
			if (m < 0 || m > *n) return 0;
			if (m > 0) {
				j %= m;
				if (j < 0) j += m;
				for (k = 0; k < j; k++) {
					double x = pila[*n - 1];
					int q;
					for (q = *n - 1; q > *n - m; q--) pila[q] = pila[q - 1];
					pila[*n - m] = x;
				}
			}
		}
#undef UNO
#undef DOS
	}
}

static int funcion(ppintor *p, pobj *fn, const double *ent, int ne, double *sal, int max, int hondo)
{
	pdoc *d = p->d;
	pobj *dom, *rango, *dic;
	int tipo, k;
	double x[8];

	fn = resuelve(d, fn);
	if (hondo > 8 || ne > 8)
		return 0;
	if (fn->t == O_ARREGLO) {
		/* un arreglo de funciones: una salida cada una */
		int n = 0;
		u32 i;

		for (i = 0; i < fn->n && n < max; i++) {
			double s1[8];
			int m = funcion(p, fn->u.v[i], ent, ne, s1, 8, hondo + 1);

			if (m > 0)
				sal[n++] = s1[0];
		}
		return n;
	}
	dic = o_dic_de(fn);
	if (!dic)
		return 0;
	tipo = (int)o_num(dic_r(d, dic, "FunctionType"), -1);
	dom = dic_r(d, dic, "Domain");
	rango = dic_r(d, dic, "Range");
	for (k = 0; k < ne; k++) {
		x[k] = ent[k];
		if (dom && dom->t == O_ARREGLO && (u32)(2 * k + 1) < dom->n) {
			double a = o_num(resuelve(d, dom->u.v[2 * k]), 0), b = o_num(resuelve(d, dom->u.v[2 * k + 1]), 1);

			if (x[k] < a) x[k] = a;
			if (x[k] > b) x[k] = b;
		}
	}
	if (tipo == 2) {
		pobj *c0 = dic_r(d, dic, "C0"), *c1 = dic_r(d, dic, "C1");
		double e = o_num(dic_r(d, dic, "N"), 1), t = so_pot(x[0] < 0 ? 0 : x[0], e);
		int n = c0 && c0->t == O_ARREGLO ? (int)c0->n : c1 && c1->t == O_ARREGLO ? (int)c1->n : 1, i;

		if (n > max)
			n = max;
		for (i = 0; i < n; i++) {
			double a = c0 && c0->t == O_ARREGLO && (u32)i < c0->n ? o_num(resuelve(d, c0->u.v[i]), 0) : 0;
			double b = c1 && c1->t == O_ARREGLO && (u32)i < c1->n ? o_num(resuelve(d, c1->u.v[i]), 1) : 1;

			sal[i] = a + t * (b - a);
		}
		return n;
	}
	if (tipo == 3) {
		pobj *fs = dic_r(d, dic, "Functions"), *lim = dic_r(d, dic, "Bounds"), *en = dic_r(d, dic, "Encode");
		double a = dom && dom->t == O_ARREGLO && dom->n >= 2 ? o_num(resuelve(d, dom->u.v[0]), 0) : 0;
		double b = dom && dom->t == O_ARREGLO && dom->n >= 2 ? o_num(resuelve(d, dom->u.v[1]), 1) : 1;
		u32 i = 0, nf = fs && fs->t == O_ARREGLO ? fs->n : 0;
		double lo = a, hi = b, t;

		if (!nf)
			return 0;
		while (lim && lim->t == O_ARREGLO && i < lim->n && i + 1 < nf && x[0] >= o_num(resuelve(d, lim->u.v[i]), 0))
			i++;
		if (lim && lim->t == O_ARREGLO) {
			if (i > 0 && i - 1 < lim->n)
				lo = o_num(resuelve(d, lim->u.v[i - 1]), a);
			if (i < lim->n)
				hi = o_num(resuelve(d, lim->u.v[i]), b);
		}
		t = x[0];
		if (en && en->t == O_ARREGLO && 2 * i + 1 < en->n) {
			double e0 = o_num(resuelve(d, en->u.v[2 * i]), 0), e1 = o_num(resuelve(d, en->u.v[2 * i + 1]), 1);

			t = hi > lo ? e0 + (x[0] - lo) * (e1 - e0) / (hi - lo) : e0;
		}
		return funcion(p, fs->u.v[i], &t, 1, sal, max, hondo + 1);
	}
	if (tipo == 0 && fn->t == O_FLUJO) {
		/* muestreada: solo de una entrada (lo de los degradados y las tintas) */
		pobj *tam = dic_r(d, dic, "Size"), *en = dic_r(d, dic, "Encode"), *de = dic_r(d, dic, "Decode");
		int bps = (int)o_num(dic_r(d, dic, "BitsPerSample"), 8), nout, i;
		u32 n, m, idx;
		u8 *datos;
		double e0 = 0, e1, t;

		if (ne != 1 || !tam || tam->t != O_ARREGLO || !rango || rango->t != O_ARREGLO)
			return 0;
		m = (u32)o_num(resuelve(d, tam->u.v[0]), 0);
		nout = (int)rango->n / 2;
		if (m < 1 || m > 65536 || nout < 1 || nout > 8 || (bps != 8 && bps != 16 && bps != 1 && bps != 2 && bps != 4))
			return 0;
		e1 = m - 1;
		if (en && en->t == O_ARREGLO && en->n >= 2) {
			e0 = o_num(resuelve(d, en->u.v[0]), 0);
			e1 = o_num(resuelve(d, en->u.v[1]), m - 1);
		}
		{
			double a = dom && dom->t == O_ARREGLO && dom->n >= 2 ? o_num(resuelve(d, dom->u.v[0]), 0) : 0;
			double b = dom && dom->t == O_ARREGLO && dom->n >= 2 ? o_num(resuelve(d, dom->u.v[1]), 1) : 1;

			t = b > a ? e0 + (x[0] - a) * (e1 - e0) / (b - a) : e0;
		}
		if (t < 0) t = 0;
		if (t > m - 1) t = m - 1;
		idx = (u32)(t + 0.5);
		datos = flujo_datos(d, fn, &n, 0, NULL);
		if (!datos)
			return 0;
		for (i = 0; i < nout && i < max; i++) {
			u32 bit = (idx * (u32)nout + (u32)i) * (u32)bps, v = 0;
			double mx = (double)((1u << bps) - 1), r0, r1;

			if ((bit + (u32)bps + 7) / 8 > n)
				break;
			if (bps == 8)
				v = datos[bit / 8];
			else if (bps == 16)
				v = (u32)datos[bit / 8] << 8 | datos[bit / 8 + 1];
			else
				v = (datos[bit / 8] >> (8 - bps - bit % 8)) & ((1u << bps) - 1);
			r0 = o_num(resuelve(d, rango->u.v[2 * i]), 0);
			r1 = o_num(resuelve(d, rango->u.v[2 * i + 1]), 1);
			if (de && de->t == O_ARREGLO && (u32)(2 * i + 1) < de->n) {
				r0 = o_num(resuelve(d, de->u.v[2 * i]), r0);
				r1 = o_num(resuelve(d, de->u.v[2 * i + 1]), r1);
			}
			sal[i] = r0 + v * (r1 - r0) / mx;
		}
		so_suelta(datos);
		return i;
	}
	if (tipo == 4 && fn->t == O_FLUJO) {
		u32 n;
		u8 *datos = flujo_datos(d, fn, &n, 0, NULL);
		double pila[104];
		int np = 0, i, nout;
		const u8 *q;

		if (!datos)
			return 0;
		for (i = 0; i < ne; i++)
			pila[np++] = x[i];
		q = datos;
		while (q < datos + n && *q != '{')
			q++;
		if (q < datos + n)
			q++;
		if (!ps_ejecuta(q, datos + n, pila, &np, 0)) {
			so_suelta(datos);
			return 0;
		}
		so_suelta(datos);
		nout = rango && rango->t == O_ARREGLO ? (int)rango->n / 2 : np;
		if (nout > np)
			nout = np;
		if (nout > max)
			nout = max;
		for (i = 0; i < nout; i++)
			sal[i] = pila[np - nout + i];
		return nout;
	}
	return 0;
}

/* ---- los colores ---- */
static u8 a8(double v)
{
	if (v != v || v <= 0)
		return 0;
	if (v >= 1)
		return 255;
	return (u8)(v * 255 + 0.5);
}

static u32 rgb(double r, double g, double b)
{
	return (u32)a8(r) << 16 | (u32)a8(g) << 8 | a8(b);
}

static u32 de_cmyk(double c, double m, double y, double k)
{
	return rgb((1 - c) * (1 - k), (1 - m) * (1 - k), (1 - y) * (1 - k));
}

/* cuántos números lleva un color de ese espacio */
static int componentes(ppintor *p, pobj *cs, int hondo)
{
	pdoc *d = p->d;

	cs = resuelve(d, cs);
	if (hondo > 8)
		return 1;
	if (o_es_nombre(cs, "DeviceRGB") || o_es_nombre(cs, "RGB") || o_es_nombre(cs, "CalRGB") || o_es_nombre(cs, "Lab"))
		return 3;
	if (o_es_nombre(cs, "DeviceCMYK") || o_es_nombre(cs, "CMYK"))
		return 4;
	if (cs->t == O_ARREGLO && cs->n >= 1) {
		pobj *k = resuelve(d, cs->u.v[0]);

		if (o_es_nombre(k, "ICCBased") && cs->n >= 2) {
			int n = (int)o_num(dic_r(d, resuelve(d, cs->u.v[1]), "N"), 3);

			return n >= 1 && n <= 4 ? n : 3;
		}
		if (o_es_nombre(k, "CalRGB") || o_es_nombre(k, "Lab"))
			return 3;
		if (o_es_nombre(k, "DeviceN") && cs->n >= 2) {
			pobj *nn = resuelve(d, cs->u.v[1]);

			return nn->t == O_ARREGLO && nn->n >= 1 && nn->n <= 8 ? (int)nn->n : 1;
		}
		if (o_es_nombre(k, "Pattern") && cs->n >= 2)
			return componentes(p, cs->u.v[1], hondo + 1);
	}
	return 1;
}

static u32 color_de(ppintor *p, pobj *cs, const double *c, int n, int hondo)
{
	pdoc *d = p->d;

	cs = resuelve(d, cs);
	if (hondo > 8)
		return 0;
	if (cs->t == O_NOMBRE) {
		if (o_es_nombre(cs, "DeviceRGB") || o_es_nombre(cs, "RGB") || o_es_nombre(cs, "CalRGB"))
			return n >= 3 ? rgb(c[0], c[1], c[2]) : 0;
		if (o_es_nombre(cs, "DeviceCMYK") || o_es_nombre(cs, "CMYK"))
			return n >= 4 ? de_cmyk(c[0], c[1], c[2], c[3]) : 0;
		return n >= 1 ? rgb(c[0], c[0], c[0]) : 0;
	}
	if (cs->t == O_ARREGLO && cs->n >= 1) {
		pobj *k = resuelve(d, cs->u.v[0]);

		if (o_es_nombre(k, "ICCBased") && cs->n >= 2) {
			int m = componentes(p, cs, hondo + 1);

			if (m == 4 && n >= 4)
				return de_cmyk(c[0], c[1], c[2], c[3]);
			if (m == 3 && n >= 3)
				return rgb(c[0], c[1], c[2]);
			return n >= 1 ? rgb(c[0], c[0], c[0]) : 0;
		}
		if (o_es_nombre(k, "CalRGB"))
			return n >= 3 ? rgb(c[0], c[1], c[2]) : 0;
		if (o_es_nombre(k, "CalGray"))
			return n >= 1 ? rgb(c[0], c[0], c[0]) : 0;
		if (o_es_nombre(k, "Lab") && n >= 3) {
			/* (Lab a RGB, con el blanco D65 y sin más) */
			double L = c[0], a = c[1], b = c[2], fy = (L + 16) / 116, fx = fy + a / 500, fz = fy - b / 200;
			double X = fx * fx * fx > 0.008856 ? fx * fx * fx : (fx - 16.0 / 116) / 7.787;
			double Y = fy * fy * fy > 0.008856 ? fy * fy * fy : (fy - 16.0 / 116) / 7.787;
			double Z = fz * fz * fz > 0.008856 ? fz * fz * fz : (fz - 16.0 / 116) / 7.787;

			X *= 0.9505;
			Z *= 1.089;
			return rgb(3.2406 * X - 1.5372 * Y - 0.4986 * Z, -0.9689 * X + 1.8758 * Y + 0.0415 * Z,
				   0.0557 * X - 0.2040 * Y + 1.0570 * Z);
		}
		if (o_es_nombre(k, "Indexed") || o_es_nombre(k, "I")) {
			pobj *base, *tabla;
			int nb, hival, i;
			u32 idx;
			double v[8];

			if (cs->n < 4 || n < 1)
				return 0;
			base = cs->u.v[1];
			hival = (int)o_num(resuelve(d, cs->u.v[2]), 0);
			tabla = resuelve(d, cs->u.v[3]);
			nb = componentes(p, base, hondo + 1);
			idx = c[0] < 0 ? 0 : (u32)c[0];
			if ((int)idx > hival)
				idx = (u32)hival;
			if (tabla->t == O_CADENA) {
				if ((idx + 1) * (u32)nb > tabla->n)
					return 0;
				for (i = 0; i < nb; i++)
					v[i] = tabla->u.s[idx * (u32)nb + (u32)i] / 255.0;
			} else if (tabla->t == O_FLUJO) {
				u32 m;
				u8 *t = flujo_datos(d, tabla, &m, 0, NULL);

				if (!t)
					return 0;
				for (i = 0; i < nb; i++)
					v[i] = (idx + 1) * (u32)nb <= m ? t[idx * (u32)nb + (u32)i] / 255.0 : 0;
				so_suelta(t);
			} else
				return 0;
			return color_de(p, base, v, nb, hondo + 1);
		}
		if ((o_es_nombre(k, "Separation") || o_es_nombre(k, "DeviceN")) && cs->n >= 4) {
			double v[8];
			int m;

			if (o_es_nombre(k, "Separation") && o_es_nombre(resuelve(d, cs->u.v[1]), "None"))
				return 0xFFFFFF;
			m = funcion(p, cs->u.v[3], c, n, v, 8, 0);
			if (m <= 0) {
				/* (sin poder hacer la función: la tinta, en gris) */
				return n >= 1 ? rgb(1 - c[0], 1 - c[0], 1 - c[0]) : 0;
			}
			return color_de(p, cs->u.v[2], v, m, hondo + 1);
		}
	}
	return n >= 3 ? rgb(c[0], c[1], c[2]) : n >= 1 ? rgb(c[0], c[0], c[0]) : 0;
}

/* ---- poner los puntos ---- */
static pgestado *G(ppintor *p)
{
	return &p->g[p->ng];
}

static void mezcla(u32 *px, u32 c, u32 a)
{
	u32 o = *px, r, g, b;

	if (a >= 255) {
		*px = c;
		return;
	}
	r = ((c >> 16 & 255) * a + (o >> 16 & 255) * (255 - a) + 127) / 255;
	g = ((c >> 8 & 255) * a + (o >> 8 & 255) * (255 - a) + 127) / 255;
	b = ((c & 255) * a + (o & 255) * (255 - a) + 127) / 255;
	*px = r << 16 | g << 8 | b;
}

/* el color de un degradado en (x, y) de la imagen: 0xAARRGGBB (A = 0: nada) */
typedef struct {
	int tipo;		/* 2 axial, 3 radial */
	double c[6];
	double t0, t1;
	int ext0, ext1;
	u32 tabla[256];
	pmat inv;
} pdegradado;

static void prepara_degradado(ppintor *p, pobj *sh, pmat a_imagen, pdegradado *dg)
{
	pdoc *d = p->d;
	pobj *coords = dic_r(d, sh, "Coords"), *dom = dic_r(d, sh, "Domain"), *ext = dic_r(d, sh, "Extend");
	pobj *fn = o_dic_clave(o_dic_de(sh), "Function"), *cs = dic_r(d, sh, "ColorSpace");
	int i, nc = componentes(p, cs, 0);

	memset(dg, 0, sizeof *dg);
	dg->tipo = (int)o_num(dic_r(d, sh, "ShadingType"), 0);
	dg->t1 = 1;
	if (coords && coords->t == O_ARREGLO)
		for (i = 0; i < 6 && (u32)i < coords->n; i++)
			dg->c[i] = o_num(resuelve(d, coords->u.v[i]), 0);
	if (dom && dom->t == O_ARREGLO && dom->n >= 2) {
		dg->t0 = o_num(resuelve(d, dom->u.v[0]), 0);
		dg->t1 = o_num(resuelve(d, dom->u.v[1]), 1);
	}
	if (ext && ext->t == O_ARREGLO && ext->n >= 2) {
		pobj *e0 = resuelve(d, ext->u.v[0]), *e1 = resuelve(d, ext->u.v[1]);

		dg->ext0 = e0->t == O_BOOL && e0->u.b;
		dg->ext1 = e1->t == O_BOOL && e1->u.b;
	}
	for (i = 0; i < 256; i++) {
		double t = dg->t0 + (dg->t1 - dg->t0) * i / 255.0, v[8];
		int m = fn ? funcion(p, fn, &t, 1, v, 8, 0) : 0;

		if (m < nc) {
			int k;

			for (k = m; k < nc; k++)
				v[k] = 0.5;
		}
		dg->tabla[i] = color_de(p, cs, v, nc, 0);
	}
	if (!inversa(a_imagen, &dg->inv))
		dg->tipo = 0;
}

static u32 color_degradado(pdegradado *dg, double x, double y, int *hay)
{
	double sx, sy, s;

	aplica(&dg->inv, x, y, &sx, &sy);
	*hay = 1;
	if (dg->tipo == 2) {
		double dx = dg->c[2] - dg->c[0], dy = dg->c[3] - dg->c[1], l = dx * dx + dy * dy;

		s = l > 0 ? ((sx - dg->c[0]) * dx + (sy - dg->c[1]) * dy) / l : 0;
	} else if (dg->tipo == 3) {
		double cx = dg->c[3] - dg->c[0], cy = dg->c[4] - dg->c[1], dr = dg->c[5] - dg->c[2];
		double px = sx - dg->c[0], py = sy - dg->c[1], r0 = dg->c[2];
		double a = cx * cx + cy * cy - dr * dr, b = px * cx + py * cy + r0 * dr, c = px * px + py * py - r0 * r0;

		if (a > -1e-9 && a < 1e-9) {
			if (b > -1e-12 && b < 1e-12) {
				*hay = 0;
				return 0;
			}
			s = c / (2 * b);
		} else {
			double disc = b * b - a * c;

			if (disc < 0) {
				*hay = 0;
				return 0;
			}
			disc = so_raiz(disc);
			s = (b + disc) / a;
			if (r0 + s * dr < 0 || (s > 1 && !dg->ext1)) {
				double s2 = (b - disc) / a;

				if (r0 + s2 * dr >= 0 && (s2 <= 1 || dg->ext1))
					s = s2;
			}
		}
	} else {
		*hay = 0;
		return 0;
	}
	if (s < 0) {
		if (!dg->ext0) {
			*hay = 0;
			return 0;
		}
		s = 0;
	}
	if (s > 1) {
		if (!dg->ext1) {
			*hay = 0;
			return 0;
		}
		s = 1;
	}
	return dg->tabla[(int)(s * 255 + 0.5)];
}

/* lo que pinta una fila: el color (o el degradado) por la cobertura, el
 * recorte y la transparencia */
typedef struct {
	ppintor *p;
	u32 color;
	u32 alfa;		/* 0 a 255 */
	pdegradado *dg;
	precorte *rec;
	u8 *mascara;		/* (para hacer un recorte nuevo) */
	precorte *viejo;
} pfila_ctx;

static void fila_pinta(void *u, int y, int x0, int x1, const u8 *cob)
{
	pfila_ctx *f = u;
	ppintor *p = f->p;
	u32 *fila = p->px + (size_t)y * p->an;
	const u8 *m = f->rec && f->rec->m ? f->rec->m + (size_t)y * p->an : NULL;
	int x;

	for (x = x0; x < x1; x++) {
		u32 a = cob[x - x0];

		if (m)
			a = a * m[x] / 255;
		if (!a)
			continue;
		a = a * f->alfa / 255;
		if (f->dg) {
			int hay;
			u32 c = color_degradado(f->dg, x + 0.5, y + 0.5, &hay);

			if (hay)
				mezcla(fila + x, c, a);
		} else
			mezcla(fila + x, f->color, a);
	}
}

static void fila_recorte(void *u, int y, int x0, int x1, const u8 *cob)
{
	pfila_ctx *f = u;
	ppintor *p = f->p;
	u8 *dst = f->mascara + (size_t)y * p->an;
	const u8 *antes = f->viejo && f->viejo->m ? f->viejo->m + (size_t)y * p->an : NULL;
	int x;

	for (x = x0; x < x1; x++)
		dst[x] = (u8)(antes ? (u32)cob[x - x0] * antes[x] / 255 : cob[x - x0]);
}

static void recorte_suelta(precorte *r)
{
	if (r && --r->usos == 0) {
		so_suelta(r->m);
		so_suelta(r);
	}
}

/* ¿el camino es un rectángulo derecho? (así el recorte no lleva máscara) */
static int es_rectangulo(pcamino *c, prect *r)
{
	double x0, x1, y0, y1;
	u32 i, n = c->n;

	if (n == 5 && c->x[4] == c->x[0] && c->y[4] == c->y[0])
		n = 4;
	if (n != 4)
		return 0;
	for (i = 1; i < c->n; i++)
		if (c->ini[i] & 1)
			return 0;
	x0 = x1 = c->x[0];
	y0 = y1 = c->y[0];
	for (i = 1; i < 4; i++) {
		if (c->x[i] < x0) x0 = c->x[i];
		if (c->x[i] > x1) x1 = c->x[i];
		if (c->y[i] < y0) y0 = c->y[i];
		if (c->y[i] > y1) y1 = c->y[i];
	}
	for (i = 0; i < 4; i++) {
		int en_x = so_abs(c->x[i] - x0) < 0.01 || so_abs(c->x[i] - x1) < 0.01;
		int en_y = so_abs(c->y[i] - y0) < 0.01 || so_abs(c->y[i] - y1) < 0.01;

		if (!en_x || !en_y)
			return 0;
	}
	r->x0 = (int)so_suelo(x0 + 0.5);
	r->y0 = (int)so_suelo(y0 + 0.5);
	r->x1 = (int)so_suelo(x1 + 0.5);
	r->y1 = (int)so_suelo(y1 + 0.5);
	if (r->x1 <= r->x0) r->x1 = r->x0 + 1;
	if (r->y1 <= r->y0) r->y1 = r->y0 + 1;
	return 1;
}

static prect cruza(prect a, prect b)
{
	prect r;

	r.x0 = a.x0 > b.x0 ? a.x0 : b.x0;
	r.y0 = a.y0 > b.y0 ? a.y0 : b.y0;
	r.x1 = a.x1 < b.x1 ? a.x1 : b.x1;
	r.y1 = a.y1 < b.y1 ? a.y1 : b.y1;
	if (r.x1 < r.x0) r.x1 = r.x0;
	if (r.y1 < r.y0) r.y1 = r.y0;
	return r;
}

/* el recorte nuevo: el de antes y el camino c */
static void recorta(ppintor *p, pcamino *c, int par_impar)
{
	pgestado *g = G(p);
	precorte *nuevo = pide(sizeof(precorte)), *viejo = g->rec;
	prect r;

	memset(nuevo, 0, sizeof *nuevo);
	nuevo->usos = 1;
	if (es_rectangulo(c, &r)) {
		nuevo->r = cruza(viejo->r, r);
		if (viejo->m) {
			nuevo->m = pide((size_t)p->an * p->al);
			memcpy(nuevo->m, viejo->m, (size_t)p->an * p->al);
		}
	} else {
		pfila_ctx f;
		double x0 = 1e9, y0 = 1e9, x1 = -1e9, y1 = -1e9;
		u32 i;

		for (i = 0; i < c->n; i++) {
			if (c->x[i] < x0) x0 = c->x[i];
			if (c->x[i] > x1) x1 = c->x[i];
			if (c->y[i] < y0) y0 = c->y[i];
			if (c->y[i] > y1) y1 = c->y[i];
		}
		r.x0 = (int)so_suelo(x0);
		r.y0 = (int)so_suelo(y0);
		r.x1 = (int)so_techo(x1) + 1;
		r.y1 = (int)so_techo(y1) + 1;
		nuevo->r = cruza(viejo->r, r);
		nuevo->m = pide((size_t)p->an * p->al);
		memset(nuevo->m, 0, (size_t)p->an * p->al);
		memset(&f, 0, sizeof f);
		f.p = p;
		f.mascara = nuevo->m;
		f.viejo = viejo;
		if (c->n >= 3)
			cubre(c, nuevo->r, par_impar, fila_recorte, &f);
	}
	recorte_suelta(viejo);
	g->rec = nuevo;
}

/* rellenar el camino c con la pintura de relleno (o la de trazo) */
static void pinta_camino(ppintor *p, pcamino *c, int par_impar, int de_trazo)
{
	pgestado *g = G(p);
	pfila_ctx f;
	pdegradado dg;
	pobj *pat = de_trazo ? g->pat_trazo : g->pat_relleno;

	if (!p->px || c->n < 2)
		return;
	memset(&f, 0, sizeof f);
	f.p = p;
	f.rec = g->rec;
	f.color = de_trazo ? g->trazo : g->relleno;
	f.alfa = a8(de_trazo ? g->alfa_trazo : g->alfa_relleno);
	if (pat) {
		pobj *sh = dic_r(p->d, pat, "Shading");
		int tipo = (int)o_num(dic_r(p->d, pat, "PatternType"), 1);

		if (tipo == 2 && sh) {
			pobj *m = dic_r(p->d, pat, "Matrix");
			pmat pm = matriz(1, 0, 0, 1, 0, 0);

			if (m && m->t == O_ARREGLO && m->n >= 6)
				pm = matriz(o_num(resuelve(p->d, m->u.v[0]), 1), o_num(resuelve(p->d, m->u.v[1]), 0),
					    o_num(resuelve(p->d, m->u.v[2]), 0), o_num(resuelve(p->d, m->u.v[3]), 1),
					    o_num(resuelve(p->d, m->u.v[4]), 0), o_num(resuelve(p->d, m->u.v[5]), 0));
			prepara_degradado(p, sh, mul(pm, p->base), &dg);
			if (dg.tipo == 2 || dg.tipo == 3)
				f.dg = &dg;
			else
				f.color = dg.tabla[128];
		} else
			f.color = 0xC8C8C8;	/* (un mosaico: en gris) */
	}
	if (f.alfa == 0)
		return;
	cubre(c, g->rec->r, par_impar, fila_pinta, &f);
}

/* ---- trazar: cada raya, un rectángulo; cada esquina, un círculo o un
 * triángulo; todo del mismo sentido y relleno junto (distinto de cero) ---- */
static void poligono(pcamino *t, const double *x, const double *y, int n)
{
	double area = 0;
	int i;

	for (i = 0; i < n; i++) {
		int j = (i + 1) % n;

		area += x[i] * y[j] - x[j] * y[i];
	}
	if (area > 0)
		for (i = 0; i < n; i++)
			cam_punto(t, x[i], y[i], i == 0);
	else
		for (i = n - 1; i >= 0; i--)
			cam_punto(t, x[i], y[i], i == n - 1);
}

static void circulo(pcamino *t, double cx, double cy, double r)
{
	double x[16], y[16];
	int i, n = r < 2 ? 8 : 16;
	static const double cs[16][2] = {{1, 0}, {0.92388, 0.38268}, {0.70711, 0.70711}, {0.38268, 0.92388}, {0, 1},
					 {-0.38268, 0.92388}, {-0.70711, 0.70711}, {-0.92388, 0.38268}, {-1, 0},
					 {-0.92388, -0.38268}, {-0.70711, -0.70711}, {-0.38268, -0.92388}, {0, -1},
					 {0.38268, -0.92388}, {0.70711, -0.70711}, {0.92388, -0.38268}};

	for (i = 0; i < n; i++) {
		x[i] = cx + r * cs[i * 16 / n][0];
		y[i] = cy + r * cs[i * 16 / n][1];
	}
	poligono(t, x, y, n);
}

static void raya_gruesa(pcamino *t, double ax, double ay, double bx, double by, double hw, int tapa_ini, int tapa_fin, int tapa)
{
	double dx = bx - ax, dy = by - ay, l = so_raiz(dx * dx + dy * dy), nx, ny, x[4], y[4];

	if (l < 1e-9)
		return;
	dx /= l;
	dy /= l;
	nx = -dy * hw;
	ny = dx * hw;
	if (tapa == 2) {
		if (tapa_ini) {
			ax -= dx * hw;
			ay -= dy * hw;
		}
		if (tapa_fin) {
			bx += dx * hw;
			by += dy * hw;
		}
	}
	x[0] = ax + nx; y[0] = ay + ny;
	x[1] = bx + nx; y[1] = by + ny;
	x[2] = bx - nx; y[2] = by - ny;
	x[3] = ax - nx; y[3] = ay - ny;
	poligono(t, x, y, 4);
}

static void esquina(pcamino *t, double ax, double ay, double bx, double by, double cx, double cy, double hw, int junta)
{
	double d1x = bx - ax, d1y = by - ay, d2x = cx - bx, d2y = cy - by;
	double l1 = so_raiz(d1x * d1x + d1y * d1y), l2 = so_raiz(d2x * d2x + d2y * d2y), x[3], y[3];

	if (l1 < 1e-9 || l2 < 1e-9)
		return;
	if (junta == 1 || hw < 1.2) {
		circulo(t, bx, by, hw);
		return;
	}
	d1x /= l1; d1y /= l1; d2x /= l2; d2y /= l2;
	/* las dos esquinas de fuera, y el bisel entre ellas (y, para el inglete, la punta) */
	{
		double cr = d1x * d2y - d1y * d2x, s = cr > 0 ? -1 : 1;
		double p1x = bx - d1y * hw * s, p1y = by + d1x * hw * s, p2x = bx - d2y * hw * s, p2y = by + d2x * hw * s;

		x[0] = bx; y[0] = by;
		x[1] = p1x; y[1] = p1y;
		x[2] = p2x; y[2] = p2y;
		poligono(t, x, y, 3);
		if (junta == 0) {
			/* el inglete: la punta, si no pasa de 4 veces el grosor */
			double mx = d1x - d2x, my = d1y - d2y, ml = so_raiz(mx * mx + my * my);
			double cosm = d1x * d2x + d1y * d2y, largo;

			if (ml > 1e-9 && cosm > -0.98) {
				largo = hw / so_raiz((1 + cosm) / 2 + 1e-12);
				if (largo < 4 * hw) {
					double q[4], r[4];

					mx /= ml;
					my /= ml;
					q[0] = bx; r[0] = by;
					q[1] = p1x; r[1] = p1y;
					q[2] = bx + mx * largo; r[2] = by + my * largo;
					q[3] = p2x; r[3] = p2y;
					poligono(t, q, r, 4);
				}
			}
		}
	}
}

/* el trazo del camino c (ya en la imagen), con los guiones */
static void traza(ppintor *p, pcamino *c, double hw)
{
	pgestado *g = G(p);
	pcamino *t = &p->trazo;
	double esc = so_raiz(so_abs(g->ctm.a * g->ctm.d - g->ctm.b * g->ctm.c));
	u32 i;

	cam_vacia(t);
	for (i = 0; i < c->n;) {
		u32 j = i + 1, a, nseg;
		int cerrado = (c->ini[i] & 2) != 0;

		while (j < c->n && !(c->ini[j] & 1))
			j++;
		nseg = j - i;
		if (nseg == 1) {
			/* un punto suelto: con tapa redonda o cuadrada, se ve */
			if (g->tapa == 1)
				circulo(t, c->x[i], c->y[i], hw);
			i = j;
			continue;
		}
		if (g->n_guion > 0) {
			/* los guiones: a trozos de la raya */
			double total = 0, pos;
			int k = 0, encendido = 1;
			u32 m;

			for (m = 0; m < (u32)g->n_guion; m++)
				total += g->guion[m];
			if (total * esc < 0.5) {
				goto sin_guiones;
			}
			pos = so_resto(g->fase * esc, total * esc);
			while (pos > g->guion[k] * esc) {
				pos -= g->guion[k] * esc;
				k = (k + 1) % g->n_guion;
				encendido = !encendido;
			}
			pos = g->guion[k] * esc - pos;
			for (a = i; a + 1 < j + (cerrado ? 1 : 0); a++) {
				u32 b = a + 1 < j ? a + 1 : i;
				double ax = c->x[a], ay = c->y[a], bx = c->x[b], by = c->y[b];
				double dx = bx - ax, dy = by - ay, l = so_raiz(dx * dx + dy * dy), hecho = 0;
				int vueltas = 0;

				while (hecho < l && vueltas++ < 100000) {
					double paso = pos < l - hecho ? pos : l - hecho;

					if (encendido && paso > 0)
						raya_gruesa(t, ax + dx * hecho / l, ay + dy * hecho / l, ax + dx * (hecho + paso) / l,
							    ay + dy * (hecho + paso) / l, hw, 1, 1, g->tapa);
					hecho += paso;
					pos -= paso;
					if (pos <= 1e-9) {
						k = (k + 1) % g->n_guion;
						encendido = !encendido;
						pos = g->guion[k] * esc;
						if (pos < 0.01)
							pos = 0.01;
					}
				}
			}
			i = j;
			continue;
		}
sin_guiones:
		for (a = i; a + 1 < j; a++)
			raya_gruesa(t, c->x[a], c->y[a], c->x[a + 1], c->y[a + 1], hw, a == i && !cerrado, a + 2 == j && !cerrado, g->tapa);
		if (cerrado)
			raya_gruesa(t, c->x[j - 1], c->y[j - 1], c->x[i], c->y[i], hw, 0, 0, g->tapa);
		for (a = i + 1; a + 1 < j; a++)
			esquina(t, c->x[a - 1], c->y[a - 1], c->x[a], c->y[a], c->x[a + 1], c->y[a + 1], hw, g->junta);
		if (cerrado && j - i >= 3) {
			esquina(t, c->x[j - 2], c->y[j - 2], c->x[j - 1], c->y[j - 1], c->x[i], c->y[i], hw, g->junta);
			esquina(t, c->x[j - 1], c->y[j - 1], c->x[i], c->y[i], c->x[i + 1], c->y[i + 1], hw, g->junta);
		} else if (!cerrado && g->tapa == 1) {
			circulo(t, c->x[i], c->y[i], hw);
			circulo(t, c->x[j - 1], c->y[j - 1], hw);
		}
		i = j;
	}
	pinta_camino(p, t, 0, 1);
}

static double medio_grosor(ppintor *p)
{
	pgestado *g = G(p);
	double esc = so_raiz(so_abs(g->ctm.a * g->ctm.d - g->ctm.b * g->ctm.c)), w = g->grosor * esc;

	if (w < 0.7)
		w = 0.7;	/* (lo más fino, que se vea) */
	return w / 2;
}

/* ---- las imágenes ---- */
static pobj *clave2(pdoc *d, pobj *dic, const char *larga, const char *corta)
{
	pobj *v = dic_r(d, dic, larga);

	return v ? v : corta ? dic_r(d, dic, corta) : NULL;
}

/* los nombres cortos de las imágenes en línea */
static pobj *cs_largo(pdoc *d, pobj *cs)
{
	if (o_es_nombre(cs, "G"))
		return o_nombre(d, "DeviceGray");
	if (o_es_nombre(cs, "RGB"))
		return o_nombre(d, "DeviceRGB");
	if (o_es_nombre(cs, "CMYK"))
		return o_nombre(d, "DeviceCMYK");
	return cs;
}

/* una muestra de «bpc» bits del punto x de la fila */
static u32 muestra(const u8 *fila, u32 k, int bpc)
{
	switch (bpc) {
	case 8: return fila[k];
	case 16: return fila[2 * k];	/* (lo alto basta) */
	case 1: return (fila[k >> 3] >> (7 - (k & 7))) & 1;
	case 2: return (fila[k >> 2] >> (6 - 2 * (k & 3))) & 3;
	case 4: return (fila[k >> 1] >> (4 - 4 * (k & 1))) & 15;
	}
	return 0;
}

static u32 *decodifica(ppintor *p, pobj *f, pobj *recursos, int *an, int *al, u32 relleno, int hondo, int solo_gris);

/* la imagen f a 0xAARRGGBB (de pide); NULL si no se sabe (o es muy grande) */
static u32 *decodifica(ppintor *p, pobj *f, pobj *recursos, int *an, int *al, u32 relleno, int hondo, int solo_gris)
{
	pdoc *d = p->d;
	pobj *dic = o_dic_de(f);
	int w = (int)o_num(clave2(d, dic, "Width", "W"), 0), h = (int)o_num(clave2(d, dic, "Height", "H"), 0);
	pobj *im = clave2(d, dic, "ImageMask", "IM"), *cs, *dec = clave2(d, dic, "Decode", "D");
	int mascara = im && im->t == O_BOOL && im->u.b, bpc, nc, x, y;
	u32 n, *r, fila;
	u8 *datos;
	const char *queda;
	double dmin[8], dmax[8];

	(void)recursos;
	if (hondo > 2 || w <= 0 || h <= 0 || w > 65535 || h > 65535 || (u64)w * h > hpdf_max_pixeles)
		return NULL;
	*an = w;
	*al = h;
	bpc = mascara ? 1 : (int)o_num(clave2(d, dic, "BitsPerComponent", "BPC"), 8);
	cs = mascara ? NULL : cs_largo(d, clave2(d, dic, "ColorSpace", "CS"));
	if (cs && cs->t == O_NOMBRE && recursos) {
		/* un nombre de los recursos */
		pobj *csr = dic_r(d, dic_r(d, recursos, "ColorSpace"), (const char *)cs->u.s);

		if (csr)
			cs = csr;
	}
	if (solo_gris)
		cs = NULL;
	nc = mascara || !cs ? 1 : componentes(p, cs, 0);
	datos = flujo_datos(d, f, &n, 1, &queda);
	if (!datos)
		return NULL;
	r = pide((size_t)w * h * 4);
	if (queda) {
		if (!strcmp(queda, "DCTDecode") || !strcmp(queda, "DCT")) {
			int jw, jh, jc;
			u8 *j = decodifica_imagen(datos, n, &jw, &jh, &jc, 3);
			size_t i, t = (size_t)w * h;

			so_suelta(datos);
			if (!j || jw != w || jh != h) {
				/* (si no mide lo que dice, se pinta lo que mide) */
				if (j && (u64)jw * jh <= hpdf_max_pixeles) {
					so_suelta(r);
					r = pide((size_t)jw * jh * 4);
					*an = w = jw;
					*al = h = jh;
					t = (size_t)w * h;
				} else {
					if (j)
						suelta_stbi(j);
					so_suelta(r);
					return NULL;
				}
			}
			for (i = 0; i < t; i++)
				r[i] = 0xFF000000u | (u32)j[3 * i] << 16 | (u32)j[3 * i + 1] << 8 | j[3 * i + 2];
			if (cs && componentes(p, cs, 0) == 4) {
				/* (las CMYK de Adobe van invertidas: stb_image ya las da en RGB) */
			}
			suelta_stbi(j);
			if (solo_gris)
				for (i = 0; i < t; i++) {
					u32 c = r[i];
					r[i] = 0xFF000000u | (((c >> 16 & 255) * 30 + (c >> 8 & 255) * 59 + (c & 255) * 11) / 100);
				}
			goto mascaras;
		}
		/* JPX, JBIG2, CCITT: no */
		so_suelta(datos);
		so_suelta(r);
		return NULL;
	}
	if (bpc != 1 && bpc != 2 && bpc != 4 && bpc != 8 && bpc != 16)
		bpc = 8;
	fila = ((u32)w * (u32)nc * (u32)bpc + 7) / 8;
	/* lo del Decode */
	{
		int k, mx = (1 << (bpc > 8 ? 8 : bpc)) - 1;
		int indexada = cs && cs->t == O_ARREGLO && cs->n && (o_es_nombre(resuelve(d, cs->u.v[0]), "Indexed") || o_es_nombre(resuelve(d, cs->u.v[0]), "I"));

		for (k = 0; k < nc && k < 8; k++) {
			dmin[k] = 0;
			dmax[k] = indexada ? mx : (cs && o_es_nombre(resuelve(d, cs->t == O_ARREGLO && cs->n ? cs->u.v[0] : cs), "Lab") ? (k == 0 ? 100 : 0) : 1);
			if (dec && dec->t == O_ARREGLO && (u32)(2 * k + 1) < dec->n) {
				dmin[k] = o_num(resuelve(d, dec->u.v[2 * k]), dmin[k]);
				dmax[k] = o_num(resuelve(d, dec->u.v[2 * k + 1]), dmax[k]);
			}
		}
	}
	if (mascara) {
		int pinta0 = !(dmin[0] > dmax[0]);	/* [0 1]: el 0 pinta */

		for (y = 0; y < h; y++) {
			const u8 *fl = (u32)(y + 1) * fila <= n ? datos + (u32)y * fila : NULL;

			for (x = 0; x < w; x++) {
				u32 v = fl ? muestra(fl, (u32)x, 1) : 1;

				r[(size_t)y * w + x] = (v == 0) == pinta0 ? (0xFF000000u | relleno) : 0;
			}
		}
		so_suelta(datos);
		return r;
	}
	{
		/* con una tabla de los colores, si caben (8 bits de una muestra o menos) */
		int con_tabla = nc == 1 && bpc <= 8, mx = (1 << (bpc > 8 ? 8 : bpc)) - 1, k;
		u32 tabla[256];
		int simple_rgb = !dec && bpc == 8 && cs && nc == 3 && (o_es_nombre(cs, "DeviceRGB") || (cs->t == O_ARREGLO && o_es_nombre(resuelve(d, cs->u.v[0]), "ICCBased")));
		int simple_cmyk = !dec && bpc == 8 && nc == 4 && cs && (o_es_nombre(cs, "DeviceCMYK") || (cs->t == O_ARREGLO && o_es_nombre(resuelve(d, cs->u.v[0]), "ICCBased")));

		if (con_tabla)
			for (k = 0; k <= mx; k++) {
				double v = dmin[0] + k * (dmax[0] - dmin[0]) / mx;

				tabla[k] = cs ? color_de(p, cs, &v, 1, 0) : rgb(v, v, v);
			}
		for (y = 0; y < h; y++) {
			const u8 *fl = (u32)(y + 1) * fila <= n ? datos + (u32)y * fila : NULL;
			u32 *s = r + (size_t)y * w;

			if (!fl) {
				for (x = 0; x < w; x++)
					s[x] = 0xFF808080u;
				continue;
			}
			if (con_tabla) {
				for (x = 0; x < w; x++)
					s[x] = 0xFF000000u | tabla[muestra(fl, (u32)x, bpc)];
			} else if (simple_rgb) {
				for (x = 0; x < w; x++)
					s[x] = 0xFF000000u | (u32)fl[3 * x] << 16 | (u32)fl[3 * x + 1] << 8 | fl[3 * x + 2];
			} else if (simple_cmyk) {
				for (x = 0; x < w; x++) {
					const u8 *q = fl + 4 * x;
					u32 kk = 255 - q[3];

					s[x] = 0xFF000000u | ((255 - q[0]) * kk / 255) << 16 | ((255 - q[1]) * kk / 255) << 8 | ((255 - q[2]) * kk / 255);
				}
			} else {
				for (x = 0; x < w; x++) {
					double v[8];

					for (k = 0; k < nc && k < 8; k++) {
						u32 m = muestra(fl, (u32)(x * nc + k), bpc);

						v[k] = dmin[k] + m * (dmax[k] - dmin[k]) / (bpc == 16 ? 255 : mx);
					}
					s[x] = 0xFF000000u | color_de(p, cs, v, nc, 0);
				}
			}
		}
		/* el color clave (/Mask con un arreglo): esos, transparentes */
		{
			pobj *mk = dic_r(d, dic, "Mask");

			if (mk && mk->t == O_ARREGLO && mk->n >= (u32)(2 * nc) && bpc == 8) {
				for (y = 0; y < h; y++) {
					const u8 *fl = (u32)(y + 1) * fila <= n ? datos + (u32)y * fila : NULL;

					if (!fl)
						continue;
					for (x = 0; x < w; x++) {
						int dentro = 1;

						for (k = 0; k < nc; k++) {
							double v = fl[x * nc + k];

							if (v < o_num(resuelve(d, mk->u.v[2 * k]), 0) || v > o_num(resuelve(d, mk->u.v[2 * k + 1]), 0))
								dentro = 0;
						}
						if (dentro)
							r[(size_t)y * w + x] &= 0x00FFFFFF;
					}
				}
			}
		}
	}
	so_suelta(datos);
mascaras:
	/* la transparencia: /SMask (gris) o /Mask (una máscara de 1 bit) */
	{
		pobj *sm = dic_r(d, dic, "SMask"), *mk = dic_r(d, dic, "Mask");
		int mw, mh, es_sm = sm && sm->t == O_FLUJO;
		u32 *m = NULL;

		if (es_sm)
			m = decodifica(p, sm, NULL, &mw, &mh, 0xFFFFFF, hondo + 1, 1);
		else if (mk && mk->t == O_FLUJO)
			m = decodifica(p, mk, NULL, &mw, &mh, 0x000000, hondo + 1, 0);
		if (m) {
			for (y = 0; y < h; y++) {
				int my = (int)((i64)y * mh / h);

				for (x = 0; x < w; x++) {
					int mx2 = (int)((i64)x * mw / w);
					u32 v = m[(size_t)my * mw + mx2], a;

					if (es_sm)
						a = v & 255;	/* (el gris es la opacidad) */
					else
						a = v >> 24;	/* (lo que pinta la máscara, se ve) */
					r[(size_t)y * w + x] = (r[(size_t)y * w + x] & 0x00FFFFFF) | (a * (r[(size_t)y * w + x] >> 24) / 255) << 24;
				}
			}
			so_suelta(m);
		}
	}
	return r;
}

u32 *imagen_de(pdoc *d, pobj *f, int *an, int *al, u32 relleno, int hondo)
{
	ppintor p;

	memset(&p, 0, sizeof p);
	p.d = d;
	return decodifica(&p, f, NULL, an, al, relleno, hondo, 0);
}

/* poner la imagen (el cuadrado unidad, con la CTM) */
static void pon_imagen(ppintor *p, const u32 *img, int w, int h)
{
	pgestado *g = G(p);
	pmat inv;
	double xs[4], ys[4], x0 = 1e9, y0 = 1e9, x1 = -1e9, y1 = -1e9, esc;
	int i, x, y, X0, Y0, X1, Y1, sub;
	const u8 *m = g->rec->m;
	u32 alfa = a8(g->alfa_relleno);

	if (!p->px || !inversa(g->ctm, &inv))
		return;
	aplica(&g->ctm, 0, 0, &xs[0], &ys[0]);
	aplica(&g->ctm, 1, 0, &xs[1], &ys[1]);
	aplica(&g->ctm, 1, 1, &xs[2], &ys[2]);
	aplica(&g->ctm, 0, 1, &xs[3], &ys[3]);
	for (i = 0; i < 4; i++) {
		if (xs[i] < x0) x0 = xs[i];
		if (xs[i] > x1) x1 = xs[i];
		if (ys[i] < y0) y0 = ys[i];
		if (ys[i] > y1) y1 = ys[i];
	}
	X0 = (int)so_suelo(x0);
	Y0 = (int)so_suelo(y0);
	X1 = (int)so_techo(x1);
	Y1 = (int)so_techo(y1);
	if (X0 < g->rec->r.x0) X0 = g->rec->r.x0;
	if (Y0 < g->rec->r.y0) Y0 = g->rec->r.y0;
	if (X1 > g->rec->r.x1) X1 = g->rec->r.x1;
	if (Y1 > g->rec->r.y1) Y1 = g->rec->r.y1;
	if (X0 >= X1 || Y0 >= Y1)
		return;
	/* (si la imagen tiene muchos más puntos que lo que ocupa, 2 x 2 muestras) */
	esc = (double)w * h / ((x1 - x0) * (y1 - y0) + 1);
	sub = esc > 2.5 ? 2 : 1;
	for (y = Y0; y < Y1; y++) {
		u32 *fila = p->px + (size_t)y * p->an;

		for (x = X0; x < X1; x++) {
			u32 r = 0, gg = 0, b = 0, a = 0;
			int sx, sy, cuenta = 0;

			for (sy = 0; sy < sub; sy++)
				for (sx = 0; sx < sub; sx++) {
					double u, v;
					int ix, iy;
					u32 c;

					aplica(&inv, x + (sx + 0.5) / sub, y + (sy + 0.5) / sub, &u, &v);
					cuenta++;
					if (u < 0 || u >= 1 || v <= 0 || v > 1)
						continue;
					ix = (int)(u * w);
					iy = (int)((1 - v) * h);
					if (ix >= w) ix = w - 1;
					if (iy >= h) iy = h - 1;
					if (iy < 0) iy = 0;
					c = img[(size_t)iy * w + ix];
					a += c >> 24;
					r += (c >> 16 & 255) * (c >> 24);
					gg += (c >> 8 & 255) * (c >> 24);
					b += (c & 255) * (c >> 24);
				}
			if (!a)
				continue;
			{
				u32 c = (r / a) << 16 | (gg / a) << 8 | (b / a), aa = a / (u32)cuenta;

				if (m)
					aa = aa * m[(size_t)y * p->an + x] / 255;
				aa = aa * alfa / 255;
				mezcla(fila + x, c, aa);
			}
		}
	}
}

static void imagen_gris(ppintor *p)
{
	/* lo que no se sabe pintar: un recuadro gris claro */
	pgestado *g = G(p);
	pcamino *c = &p->camt;
	double x, y;
	u32 color = g->relleno;
	double alfa = g->alfa_relleno;

	cam_vacia(c);
	aplica(&g->ctm, 0, 0, &x, &y);
	cam_punto(c, x, y, 1);
	aplica(&g->ctm, 1, 0, &x, &y);
	cam_punto(c, x, y, 0);
	aplica(&g->ctm, 1, 1, &x, &y);
	cam_punto(c, x, y, 0);
	aplica(&g->ctm, 0, 1, &x, &y);
	cam_punto(c, x, y, 0);
	g->relleno = 0xD8D8D8;
	g->alfa_relleno = 1;
	pinta_camino(p, c, 0, 0);
	g->relleno = color;
	g->alfa_relleno = alfa;
	cam_vacia(c);
}

static void dibuja_imagen(ppintor *p, pobj *f, pobj *recursos)
{
	int w, h;
	u32 *img;

	if (!p->px)
		return;
	img = decodifica(p, f, recursos, &w, &h, G(p)->relleno, 0, 0);
	if (!img) {
		imagen_gris(p);
		return;
	}
	pon_imagen(p, img, w, h);
	so_suelta(img);
}

/* un sombreado (sh): todo lo de dentro del recorte */
static void sombrea(ppintor *p, pobj *sh)
{
	pgestado *g = G(p);
	pdegradado dg;
	int x, y;
	prect r = g->rec->r;
	const u8 *m = g->rec->m;

	if (!p->px || !sh)
		return;
	prepara_degradado(p, sh, g->ctm, &dg);
	if (dg.tipo != 2 && dg.tipo != 3) {
		/* (de otra clase: su color de en medio, en lo que diga su /BBox, o todo) */
		return;
	}
	for (y = r.y0; y < r.y1; y++)
		for (x = r.x0; x < r.x1; x++) {
			int hay;
			u32 c = color_degradado(&dg, x + 0.5, y + 0.5, &hay), a = 255;

			if (!hay)
				continue;
			if (m)
				a = m[(size_t)y * p->an + x];
			a = a * a8(g->alfa_relleno) / 255;
			if (a)
				mezcla(p->px + (size_t)y * p->an + x, c, a);
		}
}

/* ---- el texto ---- */
static void t_mueve(void *u, double x, double y)
{
	ppintor *p = u;
	double dx, dy;

	aplica(&p->tm_letra, x, y, &dx, &dy);
	cam_punto(&p->camt, dx, dy, 1);
}

static void t_linea(void *u, double x, double y)
{
	ppintor *p = u;
	double dx, dy;

	aplica(&p->tm_letra, x, y, &dx, &dy);
	cam_punto(&p->camt, dx, dy, 0);
	p->lx = x;
	p->ly = y;
}

static void t_curva(void *u, double x1, double y1, double x2, double y2, double x3, double y3)
{
	ppintor *p = u;
	double a, b, c, d, e, f, g, h;
	pcamino *k = &p->camt;

	if (k->n == 0)
		return;
	a = k->x[k->n - 1];
	b = k->y[k->n - 1];
	aplica(&p->tm_letra, x1, y1, &c, &d);
	aplica(&p->tm_letra, x2, y2, &e, &f);
	aplica(&p->tm_letra, x3, y3, &g, &h);
	cam_curva(k, a, b, c, d, e, f, g, h);
}

static void t_cierra(void *u)
{
	(void)u;
}

static void ejecuta(ppintor *p, const u8 *datos, u32 n, pobj *recursos, int hondo);

static int empuja(ppintor *p)
{
	if (p->ng + 1 >= MAX_Q_PILA)
		return 0;
	p->g[p->ng + 1] = p->g[p->ng];
	p->g[p->ng + 1].rec->usos++;
	p->ng++;
	return 1;
}

static void saca(ppintor *p)
{
	if (p->ng <= p->ng_base)
		return;
	recorte_suelta(p->g[p->ng].rec);
	p->ng--;
}

static void muestra_texto(ppintor *p, const u8 *s, u32 n, pobj *recursos, int hondo)
{
	pgestado *g = G(p);
	pfuente *f = g->fuente;
	u32 i = 0;
	int pinta = p->px && g->modo != 3 && g->modo != 7;

	cam_vacia(&p->camt);
	while (i < n) {
		u32 codigo;
		int k = fuente_codigo(f, s + i, n - i, &codigo), un;
		double w0 = fuente_ancho(f, codigo) / 1000, tx;
		pmat trm = mul(matriz(g->tam * g->esc_h, 0, 0, g->tam, 0, g->subida), mul(p->tm, g->ctm));
		u32 uni[8];

		if (k <= 0)
			break;
		if (p->letra) {
			double x, y, x2, y2, ex, ey, l, tam;

			un = fuente_unicode(f, codigo, uni, 8);
			aplica(&trm, 0, 0, &x, &y);
			aplica(&trm, w0, 0, &x2, &y2);
			ex = x2 - x;
			ey = y2 - y;
			l = so_raiz(ex * ex + ey * ey);
			{
				double ux, uy;

				aplica(&trm, 0, 1, &ux, &uy);
				tam = so_raiz((ux - x) * (ux - x) + (uy - y) * (uy - y));
			}
			p->letra(p->u, f, codigo, uni, un, x, y, l, tam, l > 0 ? ex / l : 1, l > 0 ? ey / l : 0);
		}
		if (pinta && f) {
			if (f->tipo3) {
				const char *nombre = codigo < 256 ? f->nombres[codigo] : NULL;
				pobj *proc = nombre && f->procs ? dic_r(p->d, f->procs, nombre) : NULL;

				if (proc && proc->t == O_FLUJO && hondo < MAX_FORMAS && empuja(p)) {
					u32 m;
					u8 *c = flujo_datos(p->d, proc, &m, 0, NULL);
					pgestado *g2 = G(p);

					g2->ctm = mul(matriz(f->fm[0], f->fm[1], f->fm[2], f->fm[3], f->fm[4], f->fm[5]), trm);
					if (c) {
						pmat tm = p->tm, tlm = p->tlm;
						int base = p->ng_base;

						p->ng_base = p->ng;
						ejecuta(p, c, m, f->recursos3 ? f->recursos3 : recursos, hondo + 1);
						while (p->ng > p->ng_base)
							saca(p);
						p->ng_base = base;
						p->tm = tm;
						p->tlm = tlm;
						so_suelta(c);
					}
					saca(p);
					g = G(p);
				}
			} else {
				ptrazo t;
				double eh;

				t.u = p;
				t.mueve = t_mueve;
				t.linea = t_linea;
				t.curva = t_curva;
				t.cierra = t_cierra;
				p->tm_letra = trm;
				/* (la DejaVu, con la escala a lo ancho de la suya: se rehace) */
				{
					u32 antes = p->camt.n;

					if (fuente_dibujo(f, codigo, &t, &eh) && eh != 1) {
						p->camt.n = antes;
						p->tm_letra = mul(matriz(eh, 0, 0, 1, 0, 0), trm);
						fuente_dibujo(f, codigo, &t, &eh);
					}
				}
			}
		}
		tx = (w0 * g->tam + g->carac + (k == 1 && s[i] == ' ' ? g->palabra : 0)) * g->esc_h;
		p->tm = mul(matriz(1, 0, 0, 1, tx, 0), p->tm);
		i += (u32)k;
	}
	if (pinta && p->camt.n) {
		if (g->modo == 0 || g->modo == 2 || g->modo == 4 || g->modo == 6)
			pinta_camino(p, &p->camt, 0, 0);
		if (g->modo == 1 || g->modo == 2 || g->modo == 5 || g->modo == 6) {
			u32 q;

			for (q = 0; q < p->camt.n; q++)
				if (p->camt.ini[q] & 1)
					p->camt.ini[q] |= 2;
			traza(p, &p->camt, medio_grosor(p));
		}
	}
	cam_vacia(&p->camt);
}

/* ---- el contenido: las órdenes ---- */
static double num(pobj **op, int n, int k)
{
	return k < n ? o_num(op[k], 0) : 0;
}

static void pon_color(ppintor *p, int trazo, pobj *cs, pobj **op, int n)
{
	pgestado *g = G(p);
	double c[8];
	int k, m = componentes(p, cs, 0);
	pobj *pat = NULL;

	if (n > 0 && op[n - 1]->t == O_NOMBRE) {
		/* un patrón */
		pobj *res = p->recursos ? dic_r(p->d, p->recursos, "Pattern") : NULL;

		pat = res ? dic_r(p->d, res, (const char *)op[n - 1]->u.s) : NULL;
		n--;
	}
	for (k = 0; k < 8; k++)
		c[k] = k < n ? o_num(op[k], 0) : 0;
	if (trazo) {
		g->pat_trazo = pat;
		if (!pat || n)
			g->trazo = color_de(p, cs, c, n < m ? n : m, 0);
	} else {
		g->pat_relleno = pat;
		if (!pat || n)
			g->relleno = color_de(p, cs, c, n < m ? n : m, 0);
	}
}

/* el espacio de color de ese nombre (lo que se guarda en el estado no
 * puede ser de la memoria de los operandos, que se vacía) */
static pobj *espacio(ppintor *p, pobj *nombre)
{
	pobj *r;

	if (!nombre || nombre->t != O_NOMBRE)
		return p->cs_dev[0];
	if (o_es_nombre(nombre, "DeviceGray") || o_es_nombre(nombre, "G") || o_es_nombre(nombre, "CalGray"))
		return p->cs_dev[0];
	if (o_es_nombre(nombre, "DeviceRGB") || o_es_nombre(nombre, "RGB") || o_es_nombre(nombre, "CalRGB"))
		return p->cs_dev[1];
	if (o_es_nombre(nombre, "DeviceCMYK") || o_es_nombre(nombre, "CMYK"))
		return p->cs_dev[2];
	if (o_es_nombre(nombre, "Pattern"))
		return p->cs_dev[3];
	r = p->recursos ? dic_r(p->d, dic_r(p->d, p->recursos, "ColorSpace"), (const char *)nombre->u.s) : NULL;
	return r ? r : p->cs_dev[0];
}

static pobj *por_numero(ppintor *p, int n)
{
	return p->cs_dev[n >= 4 ? 2 : n >= 3 ? 1 : 0];
}

static void gs(ppintor *p, pobj *nombre)
{
	pdoc *d = p->d;
	pgestado *g = G(p);
	pobj *e = p->recursos && nombre && nombre->t == O_NOMBRE ? dic_r(d, dic_r(d, p->recursos, "ExtGState"), (const char *)nombre->u.s) : NULL, *v;

	if (!e)
		return;
	if ((v = dic_r(d, e, "LW")))
		g->grosor = o_num(v, 1);
	if ((v = dic_r(d, e, "LC")))
		g->tapa = (int)o_num(v, 0);
	if ((v = dic_r(d, e, "LJ")))
		g->junta = (int)o_num(v, 0);
	if ((v = dic_r(d, e, "CA")))
		g->alfa_trazo = o_num(v, 1);
	if ((v = dic_r(d, e, "ca")))
		g->alfa_relleno = o_num(v, 1);
	if ((v = dic_r(d, e, "D")) && v->t == O_ARREGLO && v->n >= 1) {
		pobj *a = resuelve(d, v->u.v[0]);
		u32 k;

		g->n_guion = 0;
		if (a->t == O_ARREGLO)
			for (k = 0; k < a->n && k < 16; k++)
				g->guion[g->n_guion++] = o_num(resuelve(d, a->u.v[k]), 0);
		g->fase = v->n > 1 ? o_num(resuelve(d, v->u.v[1]), 0) : 0;
	}
	if ((v = dic_r(d, e, "Font")) && v->t == O_ARREGLO && v->n >= 2) {
		g->fuente = fuente_de(d, resuelve(d, v->u.v[0]));
		g->tam = o_num(resuelve(d, v->u.v[1]), 12);
	}
}

static void forma(ppintor *p, pobj *f, pobj *recursos, int hondo)
{
	pdoc *d = p->d;
	pobj *m = dic_r(d, f, "Matrix"), *bb = dic_r(d, f, "BBox"), *res = dic_r(d, f, "Resources");
	pgestado *g;
	u32 n;
	u8 *c;
	pmat tm = p->tm, tlm = p->tlm;
	int base = p->ng_base;

	if (hondo >= MAX_FORMAS || !empuja(p))
		return;
	g = G(p);
	if (m && m->t == O_ARREGLO && m->n >= 6)
		g->ctm = mul(matriz(o_num(resuelve(d, m->u.v[0]), 1), o_num(resuelve(d, m->u.v[1]), 0), o_num(resuelve(d, m->u.v[2]), 0),
				    o_num(resuelve(d, m->u.v[3]), 1), o_num(resuelve(d, m->u.v[4]), 0), o_num(resuelve(d, m->u.v[5]), 0)),
			     g->ctm);
	if (bb && bb->t == O_ARREGLO && bb->n >= 4 && p->px) {
		double x0 = o_num(resuelve(d, bb->u.v[0]), 0), y0 = o_num(resuelve(d, bb->u.v[1]), 0);
		double x1 = o_num(resuelve(d, bb->u.v[2]), 0), y1 = o_num(resuelve(d, bb->u.v[3]), 0), x, y;
		pcamino *c2 = &p->camt;

		cam_vacia(c2);
		aplica(&g->ctm, x0, y0, &x, &y);
		cam_punto(c2, x, y, 1);
		aplica(&g->ctm, x1, y0, &x, &y);
		cam_punto(c2, x, y, 0);
		aplica(&g->ctm, x1, y1, &x, &y);
		cam_punto(c2, x, y, 0);
		aplica(&g->ctm, x0, y1, &x, &y);
		cam_punto(c2, x, y, 0);
		recorta(p, c2, 0);
		cam_vacia(c2);
	}
	c = flujo_datos(d, f, &n, 0, NULL);
	if (c) {
		p->ng_base = p->ng;
		ejecuta(p, c, n, res ? res : recursos, hondo + 1);
		while (p->ng > p->ng_base)
			saca(p);
		p->ng_base = base;
		so_suelta(c);
	}
	saca(p);
	p->tm = tm;
	p->tlm = tlm;
}

/* una imagen en línea: BI ... ID datos EI */
static const u8 *imagen_en_linea(ppintor *p, plex *l, pobj *recursos)
{
	pobj *dic = o_dicc(p->tmp), *f;
	ptok t;
	const u8 *ini, *fin;
	int vueltas = 0;

	for (;;) {
		pobj *k, *v;

		if (lex_token(l, &t) == T_FIN || ++vueltas > 200)
			return l->fin;
		if (t.t == T_PALABRA && tok_es(&t, "ID"))
			break;
		k = lee_desde_token(p->tmp, l, &t, 0);
		v = lee_objeto(p->tmp, l, 0);
		if (k && k->t == O_NOMBRE && v) {
			static const char *cortas[][2] = {{"W", "Width"}, {"H", "Height"}, {"BPC", "BitsPerComponent"}, {"CS", "ColorSpace"},
							  {"F", "Filter"}, {"DP", "DecodeParms"}, {"IM", "ImageMask"}, {"D", "Decode"},
							  {"I", "Interpolate"}};
			const char *nombre = (const char *)k->u.s;
			u32 q;

			for (q = 0; q < sizeof cortas / sizeof cortas[0]; q++)
				if (!strcmp(nombre, cortas[q][0]))
					nombre = cortas[q][1];
			if (o_es_nombre(v, "AHx")) v = o_nombre(p->tmp, "ASCIIHexDecode");
			else if (o_es_nombre(v, "A85")) v = o_nombre(p->tmp, "ASCII85Decode");
			else if (o_es_nombre(v, "Fl")) v = o_nombre(p->tmp, "FlateDecode");
			else if (o_es_nombre(v, "RL")) v = o_nombre(p->tmp, "RunLengthDecode");
			else if (o_es_nombre(v, "DCT")) v = o_nombre(p->tmp, "DCTDecode");
			o_pon(p->tmp, dic, nombre, v);
		}
	}
	ini = l->p;
	if (ini < l->fin && es_blanco(*ini))
		ini++;
	/* lo que mide, si no lleva filtro; si no, hasta « EI » */
	fin = NULL;
	if (!o_dic_clave(dic, "Filter")) {
		int w = (int)o_num(o_dic_clave(dic, "Width"), 0), h = (int)o_num(o_dic_clave(dic, "Height"), 0);
		pobj *im = o_dic_clave(dic, "ImageMask");
		int bpc = im && im->t == O_BOOL && im->u.b ? 1 : (int)o_num(o_dic_clave(dic, "BitsPerComponent"), 8);
		int nc = im && im->t == O_BOOL && im->u.b ? 1 : componentes(p, espacio(p, o_dic_clave(dic, "ColorSpace")), 0);
		u64 largo = (u64)((u64)w * nc * bpc + 7) / 8 * (u64)h;

		if (w > 0 && h > 0 && largo <= (u64)(l->fin - ini)) {
			const u8 *q = ini + largo;

			while (q < l->fin && es_blanco(*q))
				q++;
			if (q + 2 <= l->fin && q[0] == 'E' && q[1] == 'I')
				fin = ini + largo;
		}
	}
	if (!fin) {
		const u8 *q;

		for (q = ini; q + 2 <= l->fin; q++)
			if (q[0] == 'E' && q[1] == 'I' && (q == ini || es_blanco(q[-1])) && (q + 2 == l->fin || es_blanco(q[2]) || es_delim(q[2])))
				break;
		fin = q + 2 <= l->fin ? q : l->fin;
	}
	{
		pobj *cs = o_dic_clave(dic, "ColorSpace");

		if (cs && cs->t == O_NOMBRE)
			o_pon(p->tmp, dic, "ColorSpace", espacio(p, cs));
	}
	f = o_nuevo(p->tmp, O_FLUJO);
	f->u.f.dic = dic;
	f->u.f.datos = ini;
	f->u.f.largo = (u32)(fin - ini);
	f->propio = 1;
	if (p->px)
		dibuja_imagen(p, f, recursos);
	l->p = fin;
	{
		ptok e;
		const u8 *antes = l->p;

		if (lex_token(l, &e) != T_PALABRA || !tok_es(&e, "EI"))
			l->p = antes;
	}
	return l->p;
}

static void pinta_y_acaba(ppintor *p, int rellena, int par_impar, int traza_)
{
	if (rellena)
		pinta_camino(p, &p->cam, par_impar, 0);
	if (traza_)
		traza(p, &p->cam, medio_grosor(p));
	if (p->recorte_pendiente && p->px)
		recorta(p, &p->cam, p->recorte_pendiente == 2);
	p->recorte_pendiente = 0;
	cam_vacia(&p->cam);
}

#define OP(s) (t.n == sizeof(s) - 1 && memcmp(t.p, s, sizeof(s) - 1) == 0)

static void ejecuta(ppintor *p, const u8 *datos, u32 n, pobj *recursos, int hondo)
{
	pdoc *d = p->d;
	plex l;
	ptok t;
	pobj *op[MAX_PILA];
	int nop = 0;
	pobj *res_antes = p->recursos;

	p->recursos = recursos;
	l.ini = l.p = datos;
	l.fin = datos + n;
	for (;;) {
		pgestado *g;

		lex_token(&l, &t);
		if (t.t == T_FIN)
			break;
		if (t.t != T_PALABRA) {
			pobj *o;

			if (t.t == T_CIERRA_A || t.t == T_CIERRA_D || t.t == T_MALO || t.t == T_ABRE_L || t.t == T_CIERRA_L)
				continue;
			o = lee_desde_token(p->tmp, &l, &t, 0);
			if (!o)
				continue;
			if (nop >= MAX_PILA) {
				memmove(op, op + 1, (MAX_PILA - 1) * sizeof(pobj *));
				nop--;
			}
			op[nop++] = o;
			continue;
		}
		if (++p->ordenes > MAX_ORDENES)
			break;
		g = G(p);
		/* el camino */
		if (OP("m")) {
			double x, y;

			p->cx = num(op, nop, 0);
			p->cy = num(op, nop, 1);
			p->ix = p->cx;
			p->iy = p->cy;
			aplica(&g->ctm, p->cx, p->cy, &x, &y);
			cam_punto(&p->cam, x, y, 1);
		} else if (OP("l")) {
			double x, y;

			p->cx = num(op, nop, 0);
			p->cy = num(op, nop, 1);
			aplica(&g->ctm, p->cx, p->cy, &x, &y);
			cam_punto(&p->cam, x, y, p->cam.n == 0);
		} else if (OP("c") || OP("v") || OP("y")) {
			double x0, y0, x1, y1, x2, y2, x3, y3, a1, b1, a2, b2, a3, b3;

			if (OP("c")) {
				a1 = num(op, nop, 0); b1 = num(op, nop, 1); a2 = num(op, nop, 2); b2 = num(op, nop, 3);
				a3 = num(op, nop, 4); b3 = num(op, nop, 5);
			} else if (OP("v")) {
				a1 = p->cx; b1 = p->cy; a2 = num(op, nop, 0); b2 = num(op, nop, 1);
				a3 = num(op, nop, 2); b3 = num(op, nop, 3);
			} else {
				a1 = num(op, nop, 0); b1 = num(op, nop, 1); a2 = a3 = num(op, nop, 2); b2 = b3 = num(op, nop, 3);
			}
			aplica(&g->ctm, p->cx, p->cy, &x0, &y0);
			aplica(&g->ctm, a1, b1, &x1, &y1);
			aplica(&g->ctm, a2, b2, &x2, &y2);
			aplica(&g->ctm, a3, b3, &x3, &y3);
			if (p->cam.n == 0)
				cam_punto(&p->cam, x0, y0, 1);
			cam_curva(&p->cam, x0, y0, x1, y1, x2, y2, x3, y3);
			p->cx = a3;
			p->cy = b3;
		} else if (OP("h")) {
			cam_cierra(&p->cam);
			p->cx = p->ix;
			p->cy = p->iy;
		} else if (OP("re")) {
			double x = num(op, nop, 0), y = num(op, nop, 1), w = num(op, nop, 2), h = num(op, nop, 3), a, b;

			aplica(&g->ctm, x, y, &a, &b);
			cam_punto(&p->cam, a, b, 1);
			aplica(&g->ctm, x + w, y, &a, &b);
			cam_punto(&p->cam, a, b, 0);
			aplica(&g->ctm, x + w, y + h, &a, &b);
			cam_punto(&p->cam, a, b, 0);
			aplica(&g->ctm, x, y + h, &a, &b);
			cam_punto(&p->cam, a, b, 0);
			cam_cierra(&p->cam);
			p->cx = p->ix = x;
			p->cy = p->iy = y;
		} else if (OP("S")) {
			pinta_y_acaba(p, 0, 0, 1);
		} else if (OP("s")) {
			cam_cierra(&p->cam);
			pinta_y_acaba(p, 0, 0, 1);
		} else if (OP("f") || OP("F")) {
			pinta_y_acaba(p, 1, 0, 0);
		} else if (OP("f*")) {
			pinta_y_acaba(p, 1, 1, 0);
		} else if (OP("B")) {
			pinta_y_acaba(p, 1, 0, 1);
		} else if (OP("B*")) {
			pinta_y_acaba(p, 1, 1, 1);
		} else if (OP("b")) {
			cam_cierra(&p->cam);
			pinta_y_acaba(p, 1, 0, 1);
		} else if (OP("b*")) {
			cam_cierra(&p->cam);
			pinta_y_acaba(p, 1, 1, 1);
		} else if (OP("n")) {
			pinta_y_acaba(p, 0, 0, 0);
		} else if (OP("W")) {
			p->recorte_pendiente = 1;
		} else if (OP("W*")) {
			p->recorte_pendiente = 2;
		/* el estado */
		} else if (OP("q")) {
			empuja(p);
		} else if (OP("Q")) {
			saca(p);
		} else if (OP("cm")) {
			if (nop >= 6)
				g->ctm = mul(matriz(num(op, nop, 0), num(op, nop, 1), num(op, nop, 2), num(op, nop, 3), num(op, nop, 4),
						    num(op, nop, 5)), g->ctm);
		} else if (OP("w")) {
			g->grosor = num(op, nop, 0);
		} else if (OP("J")) {
			g->tapa = (int)num(op, nop, 0);
		} else if (OP("j")) {
			g->junta = (int)num(op, nop, 0);
		} else if (OP("M")) {
			g->inglete = num(op, nop, 0);
		} else if (OP("d")) {
			g->n_guion = 0;
			if (nop >= 1 && op[0]->t == O_ARREGLO) {
				u32 k;
				double total = 0;

				for (k = 0; k < op[0]->n && k < 16; k++) {
					g->guion[g->n_guion] = o_num(op[0]->u.v[k], 0);
					if (g->guion[g->n_guion] < 0)
						g->guion[g->n_guion] = 0;
					total += g->guion[g->n_guion++];
				}
				if (total <= 0)
					g->n_guion = 0;
			}
			g->fase = num(op, nop, 1);
		} else if (OP("gs")) {
			if (nop >= 1)
				gs(p, op[0]);
		/* los colores */
		} else if (OP("g")) {
			double v = num(op, nop, 0);

			g->cs_relleno = NULL;
			g->pat_relleno = NULL;
			g->relleno = rgb(v, v, v);
		} else if (OP("G")) {
			double v = num(op, nop, 0);

			g->cs_trazo = NULL;
			g->pat_trazo = NULL;
			g->trazo = rgb(v, v, v);
		} else if (OP("rg")) {
			g->cs_relleno = NULL;
			g->pat_relleno = NULL;
			g->relleno = rgb(num(op, nop, 0), num(op, nop, 1), num(op, nop, 2));
		} else if (OP("RG")) {
			g->cs_trazo = NULL;
			g->pat_trazo = NULL;
			g->trazo = rgb(num(op, nop, 0), num(op, nop, 1), num(op, nop, 2));
		} else if (OP("k")) {
			g->cs_relleno = NULL;
			g->pat_relleno = NULL;
			g->relleno = de_cmyk(num(op, nop, 0), num(op, nop, 1), num(op, nop, 2), num(op, nop, 3));
		} else if (OP("K")) {
			g->cs_trazo = NULL;
			g->pat_trazo = NULL;
			g->trazo = de_cmyk(num(op, nop, 0), num(op, nop, 1), num(op, nop, 2), num(op, nop, 3));
		} else if (OP("cs") || OP("CS")) {
			pobj *cs = espacio(p, nop ? op[0] : NULL);
			int tr = OP("CS");
			double cero[4] = {0, 0, 0, 1};
			int m = componentes(p, cs, 0);
			u32 c = o_es_nombre(cs, "DeviceCMYK") ? 0 : color_de(p, cs, cero, m == 4 ? 4 : m, 0);

			if (o_es_nombre(cs, "Pattern"))
				c = 0;
			if (tr) {
				g->cs_trazo = cs;
				g->pat_trazo = NULL;
				g->trazo = c;
			} else {
				g->cs_relleno = cs;
				g->pat_relleno = NULL;
				g->relleno = c;
			}
		} else if (OP("sc") || OP("scn")) {
			pon_color(p, 0, g->cs_relleno ? g->cs_relleno : por_numero(p, nop), op, nop);
		} else if (OP("SC") || OP("SCN")) {
			pon_color(p, 1, g->cs_trazo ? g->cs_trazo : por_numero(p, nop), op, nop);
		} else if (OP("sh")) {
			pobj *sh = nop && op[0]->t == O_NOMBRE && recursos ? dic_r(d, dic_r(d, recursos, "Shading"), (const char *)op[0]->u.s) : NULL;

			sombrea(p, sh);
		/* el texto */
		} else if (OP("BT")) {
			p->tm = p->tlm = matriz(1, 0, 0, 1, 0, 0);
		} else if (OP("ET")) {
		} else if (OP("Tc")) {
			g->carac = num(op, nop, 0);
		} else if (OP("Tw")) {
			g->palabra = num(op, nop, 0);
		} else if (OP("Tz")) {
			g->esc_h = num(op, nop, 0) / 100;
		} else if (OP("TL")) {
			g->interl = num(op, nop, 0);
		} else if (OP("Ts")) {
			g->subida = num(op, nop, 0);
		} else if (OP("Tr")) {
			g->modo = (int)num(op, nop, 0);
		} else if (OP("Tf")) {
			if (nop >= 2) {
				pobj *fd = recursos && op[0]->t == O_NOMBRE ? dic_r(d, dic_r(d, recursos, "Font"), (const char *)op[0]->u.s) : NULL;

				g->fuente = fuente_de(d, fd);
				g->tam = num(op, nop, 1);
			}
		} else if (OP("Td") || OP("TD")) {
			if (OP("TD"))
				g->interl = -num(op, nop, 1);
			p->tlm = mul(matriz(1, 0, 0, 1, num(op, nop, 0), num(op, nop, 1)), p->tlm);
			p->tm = p->tlm;
		} else if (OP("Tm")) {
			if (nop >= 6)
				p->tm = p->tlm = matriz(num(op, nop, 0), num(op, nop, 1), num(op, nop, 2), num(op, nop, 3), num(op, nop, 4), num(op, nop, 5));
		} else if (OP("T*")) {
			p->tlm = mul(matriz(1, 0, 0, 1, 0, -g->interl), p->tlm);
			p->tm = p->tlm;
		} else if (OP("Tj") || OP("'") || OP("\"")) {
			pobj *s = NULL;

			if (OP("\"") && nop >= 3) {
				g->palabra = num(op, nop, 0);
				g->carac = num(op, nop, 1);
				s = op[2];
			} else if (nop >= 1)
				s = op[nop - 1];
			if (!OP("Tj")) {
				p->tlm = mul(matriz(1, 0, 0, 1, 0, -g->interl), p->tlm);
				p->tm = p->tlm;
			}
			if (s && s->t == O_CADENA)
				muestra_texto(p, s->u.s, s->n, recursos, hondo);
		} else if (OP("TJ")) {
			if (nop >= 1 && op[nop - 1]->t == O_ARREGLO) {
				pobj *a = op[nop - 1];
				u32 k;

				for (k = 0; k < a->n; k++) {
					pobj *x = a->u.v[k];

					if (x->t == O_CADENA)
						muestra_texto(p, x->u.s, x->n, recursos, hondo);
					else if (o_es_num(x)) {
						double tx = -o_num(x, 0) / 1000 * g->tam * g->esc_h;

						if (p->letra && o_num(x, 0) < -180)
							p->letra(p->u, g->fuente, 0xFFFFFFFF, NULL, 0, 0, 0, 0, 0, 0, 0);	/* (un hueco: un espacio) */
						p->tm = mul(matriz(1, 0, 0, 1, tx, 0), p->tm);
					}
				}
			}
		/* lo de fuera */
		} else if (OP("Do")) {
			pobj *x = nop && op[0]->t == O_NOMBRE && recursos ? dic_r(d, dic_r(d, recursos, "XObject"), (const char *)op[0]->u.s) : NULL;

			if (x && x->t == O_FLUJO) {
				pobj *st = dic_r(d, x, "Subtype");

				if (o_es_nombre(st, "Image"))
					dibuja_imagen(p, x, recursos);
				else if (o_es_nombre(st, "Form"))
					forma(p, x, recursos, hondo);
			}
		} else if (OP("BI")) {
			imagen_en_linea(p, &l, recursos);
		}
		/* (BMC, BDC, EMC, MP, DP, BX, EX, d0, d1, ri, i: nada que pintar) */
		nop = 0;
		/* la memoria de los operandos, de vez en cuando, a cero */
		if (hondo == 0 && (p->ordenes & 1023) == 0 && p->tmp->bloques) {
			ar_suelta(p->tmp);
			p->tmp->nulo = o_nuevo(p->tmp, O_NULO);
		}
	}
	p->recursos = res_antes;
}
#undef OP

/* ---- las anotaciones: su aspecto (/AP /N), en su sitio ---- */
static void anotaciones(ppintor *p, u32 i)
{
	pdoc *d = p->d;
	pobj *an = dic_r(d, d->paginas[i], "Annots");
	u32 k;

	if (!an || an->t != O_ARREGLO)
		return;
	for (k = 0; k < an->n && k < 2000; k++) {
		pobj *a = resuelve(d, an->u.v[k]), *ap, *n, *r, *bb, *m;
		int flags;
		double R[4], B[4], M[6] = {1, 0, 0, 1, 0, 0};
		u32 q;

		if (a->t != O_DICC)
			continue;
		flags = (int)o_num(dic_r(d, a, "F"), 0);
		if (flags & (2 | 32))	/* oculta, o que no se ve */
			continue;
		if (o_es_nombre(dic_r(d, a, "Subtype"), "Popup"))
			continue;
		ap = dic_r(d, a, "AP");
		n = ap ? dic_r(d, ap, "N") : NULL;
		if (n && n->t == O_DICC) {
			pobj *as = dic_r(d, a, "AS");

			n = as && as->t == O_NOMBRE ? dic_r(d, n, (const char *)as->u.s) : NULL;
		}
		if (!n || n->t != O_FLUJO)
			continue;
		r = dic_r(d, a, "Rect");
		bb = dic_r(d, n, "BBox");
		m = dic_r(d, n, "Matrix");
		if (!r || r->t != O_ARREGLO || r->n < 4 || !bb || bb->t != O_ARREGLO || bb->n < 4)
			continue;
		for (q = 0; q < 4; q++) {
			R[q] = o_num(resuelve(d, r->u.v[q]), 0);
			B[q] = o_num(resuelve(d, bb->u.v[q]), 0);
		}
		if (m && m->t == O_ARREGLO && m->n >= 6)
			for (q = 0; q < 6; q++)
				M[q] = o_num(resuelve(d, m->u.v[q]), M[q]);
		{
			/* la caja, con su matriz, a lo que mide el Rect */
			pmat fm = matriz(M[0], M[1], M[2], M[3], M[4], M[5]);
			double xs[4], ys[4], x0 = 1e9, y0 = 1e9, x1 = -1e9, y1 = -1e9, rx0, ry0, rx1, ry1;
			int j;

			aplica(&fm, B[0], B[1], &xs[0], &ys[0]);
			aplica(&fm, B[2], B[1], &xs[1], &ys[1]);
			aplica(&fm, B[2], B[3], &xs[2], &ys[2]);
			aplica(&fm, B[0], B[3], &xs[3], &ys[3]);
			for (j = 0; j < 4; j++) {
				if (xs[j] < x0) x0 = xs[j];
				if (xs[j] > x1) x1 = xs[j];
				if (ys[j] < y0) y0 = ys[j];
				if (ys[j] > y1) y1 = ys[j];
			}
			rx0 = R[0] < R[2] ? R[0] : R[2];
			rx1 = R[0] < R[2] ? R[2] : R[0];
			ry0 = R[1] < R[3] ? R[1] : R[3];
			ry1 = R[1] < R[3] ? R[3] : R[1];
			if (x1 - x0 < 1e-6 || y1 - y0 < 1e-6)
				continue;
			if (!empuja(p))
				break;
			G(p)->ctm = mul(matriz((rx1 - rx0) / (x1 - x0), 0, 0, (ry1 - ry0) / (y1 - y0), rx0 - x0 * (rx1 - rx0) / (x1 - x0),
					       ry0 - y0 * (ry1 - ry0) / (y1 - y0)), G(p)->ctm);
			forma(p, n, dic_r(d, d->paginas[i], "Resources"), 1);
			saca(p);
		}
	}
}

/* lo de la página que se va a recorrer: su contenido, entero */
static u8 *contenido(pdoc *d, u32 i, u32 *n)
{
	pobj *c = dic_r(d, d->paginas[i], "Contents");
	u8 *todo = NULL;
	u32 total = 0, k;

	*n = 0;
	if (!c)
		return NULL;
	if (c->t == O_FLUJO) {
		todo = flujo_datos(d, c, n, 0, NULL);
		return todo;
	}
	if (c->t != O_ARREGLO)
		return NULL;
	for (k = 0; k < c->n && k < 10000; k++) {
		pobj *f = resuelve(d, c->u.v[k]);
		u32 m;
		u8 *x;

		if (f->t != O_FLUJO)
			continue;
		x = flujo_datos(d, f, &m, 0, NULL);
		if (!x)
			continue;
		if ((u64)total + m + 2 > MAX_FLUJO) {
			so_suelta(x);
			break;
		}
		todo = repide(todo, total + m + 2);
		memcpy(todo + total, x, m);
		total += m;
		todo[total++] = '\n';
		so_suelta(x);
	}
	*n = total;
	return todo;
}

static void prepara(ppintor *p, pdoc *d, pmat base, int an, int al)
{
	pgestado *g;

	memset(p, 0, sizeof *p);
	p->d = d;
	p->an = an;
	p->al = al;
	p->base = base;
	p->tmp = pdf_vacio();
	p->cs_dev[0] = o_nombre(d, "DeviceGray");
	p->cs_dev[1] = o_nombre(d, "DeviceRGB");
	p->cs_dev[2] = o_nombre(d, "DeviceCMYK");
	p->cs_dev[3] = o_nombre(d, "Pattern");
	g = &p->g[0];
	g->ctm = base;
	g->alfa_relleno = g->alfa_trazo = 1;
	g->grosor = 1;
	g->inglete = 10;
	g->esc_h = 1;
	g->rec = pide(sizeof(precorte));
	memset(g->rec, 0, sizeof(precorte));
	g->rec->usos = 1;
	g->rec->r.x1 = an;
	g->rec->r.y1 = al;
}

static void acaba(ppintor *p)
{
	while (p->ng > 0)
		saca(p);
	p->ng_base = -1;
	recorte_suelta(p->g[0].rec);
	so_suelta(p->cam.x); so_suelta(p->cam.y); so_suelta(p->cam.ini);
	so_suelta(p->camt.x); so_suelta(p->camt.y); so_suelta(p->camt.ini);
	so_suelta(p->trazo.x); so_suelta(p->trazo.y); so_suelta(p->trazo.ini);
	pdf_cierra(p->tmp);
}

pimagen *pinta_pagina(pdoc *d, u32 i, double ppp, int lado)
{
	ppintor *p;
	pimagen *im;
	double c[4], w, h, s;
	int giro, an, al;
	pmat base;
	u32 n;
	u8 *datos;
	size_t k;

	if (i >= d->n_paginas)
		return NULL;
	pagina_caja(d, i, c);
	giro = pagina_giro(d, i);
	w = c[2] - c[0];
	h = c[3] - c[1];
	if (giro == 90 || giro == 270) {
		double t = w;
		w = h;
		h = t;
	}
	if (lado > 0)
		s = lado / (w > h ? w : h);
	else
		s = ppp / 72;
	an = (int)(w * s + 0.5);
	al = (int)(h * s + 0.5);
	if (an < 1) an = 1;
	if (al < 1) al = 1;
	if ((u64)an * al > 64u * 1024 * 1024)
		return NULL;
	switch (giro) {
	case 90: base = matriz(0, s, s, 0, -c[1] * s, -c[0] * s); break;
	case 180: base = matriz(-s, 0, 0, s, c[2] * s, -c[1] * s); break;
	case 270: base = matriz(0, -s, -s, 0, c[3] * s, c[2] * s); break;
	default: base = matriz(s, 0, 0, -s, -c[0] * s, c[3] * s); break;
	}
	im = pide(sizeof(pimagen));
	im->an = an;
	im->al = al;
	im->px = pide((size_t)an * al * 4);
	for (k = 0; k < (size_t)an * al; k++)
		im->px[k] = 0xFFFFFF;
	p = pide(sizeof(ppintor));
	prepara(p, d, base, an, al);
	p->px = im->px;
	datos = contenido(d, i, &n);
	if (datos) {
		ejecuta(p, datos, n, pagina_heredada(d, i, "Resources"), 0);
		so_suelta(datos);
	}
	while (p->ng > 0)
		saca(p);
	anotaciones(p, i);
	acaba(p);
	so_suelta(p);
	return im;
}

void imagen_suelta(pimagen *im)
{
	if (!im)
		return;
	so_suelta(im->px);
	so_suelta(im);
}

/* recorrer la página sin pintar: cada letra, a «letra» (en puntos, con la y hacia abajo) */
void recorre_texto(pdoc *d, u32 i, pletra_fn letra, void *u)
{
	ppintor *p;
	double c[4];
	u32 n;
	u8 *datos;

	if (i >= d->n_paginas)
		return;
	pagina_caja(d, i, c);
	p = pide(sizeof(ppintor));
	prepara(p, d, matriz(1, 0, 0, -1, -c[0], c[3]), (int)(c[2] - c[0]) + 1, (int)(c[3] - c[1]) + 1);
	p->letra = letra;
	p->u = u;
	datos = contenido(d, i, &n);
	if (datos) {
		ejecuta(p, datos, n, pagina_heredada(d, i, "Resources"), 0);
		so_suelta(datos);
	}
	acaba(p);
	so_suelta(p);
}
