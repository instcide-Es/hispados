/* vertice.c - HispaGL: glBegin/glEnd y los vértices; la transformación, la
 * luz (8 luces, materiales, dos caras, color del material), la niebla por
 * vértice, la generación de coordenadas de textura, el recorte (contra el
 * volumen de vista y los planos del programa, con banda de guarda en x e y)
 * y el montaje de las primitivas; las matrices de vértices (glDrawArrays,
 * glDrawElements, glArrayElement, glInterleavedArrays) y glRect.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "hgl.h"

#define FUERA_BANDA 0x1000          /* más allá de la banda de guarda (o w casi 0) */
#define HAY_QUE_RECORTAR (0x0FF0 | FUERA_BANDA)  /* cerca/lejos, los del programa, la banda */
#define BANDA 4.0f

/* ---- la luz de un vértice ---- */
static void luz_cara(Ctx *C, const Material *m, const float *e, const float *n, float *col)
{
	float r = m->emi[0] + m->amb[0] * C->luz.amb[0];
	float g = m->emi[1] + m->amb[1] * C->luz.amb[1];
	float b = m->emi[2] + m->amb[2] * C->luz.amb[2];
	int k;
	for (k = 0; k < LUCES; k++) {
		const Luz *l = &C->luz.l[k];
		float L[3], d2, at = 1, nl, sr, sg, sb;
		if (!C->hab.luzi[k]) continue;
		if (l->pos[3] == 0) {
			L[0] = l->dirn[0]; L[1] = l->dirn[1]; L[2] = l->dirn[2];
		} else {
			float d, id;
			L[0] = l->pos[0] - e[0]; L[1] = l->pos[1] - e[1]; L[2] = l->pos[2] - e[2];
			d2 = L[0] * L[0] + L[1] * L[1] + L[2] * L[2];
			d = hgl_raiz(d2);
			id = d > 0 ? 1.0f / d : 0;
			L[0] *= id; L[1] *= id; L[2] *= id;
			if (l->att[1] != 0 || l->att[2] != 0 || l->att[0] != 1) {
				float q = l->att[0] + l->att[1] * d + l->att[2] * d2;
				at = q > 0 ? 1.0f / q : 1;
			}
			if (l->corte != 180) {
				float cs = -(L[0] * l->dir[0] + L[1] * l->dir[1] + L[2] * l->dir[2]);
				if (cs < l->cos_corte) continue;
				if (l->exp != 0) at *= hgl_pot(cs, l->exp);
			}
		}
		/* la ambiente */
		sr = m->amb[0] * l->amb[0]; sg = m->amb[1] * l->amb[1]; sb = m->amb[2] * l->amb[2];
		nl = n[0] * L[0] + n[1] * L[1] + n[2] * L[2];
		if (nl > 0) {
			float H[3], nh, hl;
			sr += nl * m->dif[0] * l->dif[0]; sg += nl * m->dif[1] * l->dif[1]; sb += nl * m->dif[2] * l->dif[2];
			if (m->esp[0] != 0 || m->esp[1] != 0 || m->esp[2] != 0) {
				if (C->luz.local) {
					float v = hgl_raiz(e[0] * e[0] + e[1] * e[1] + e[2] * e[2]);
					v = v > 0 ? 1.0f / v : 0;
					H[0] = L[0] - e[0] * v; H[1] = L[1] - e[1] * v; H[2] = L[2] - e[2] * v;
				} else {
					H[0] = L[0]; H[1] = L[1]; H[2] = L[2] + 1;
				}
				hl = hgl_raiz(H[0] * H[0] + H[1] * H[1] + H[2] * H[2]);
				if (hl > 0) {
					nh = (n[0] * H[0] + n[1] * H[1] + n[2] * H[2]) / hl;
					if (nh > 0) {
						float s = hgl_pot(nh, m->brillo);
						sr += s * m->esp[0] * l->esp[0]; sg += s * m->esp[1] * l->esp[1]; sb += s * m->esp[2] * l->esp[2];
					}
				}
			}
		}
		r += at * sr; g += at * sg; b += at * sb;
	}
	col[0] = hgl_lim01(r); col[1] = hgl_lim01(g); col[2] = hgl_lim01(b); col[3] = hgl_lim01(m->dif[3]);
}

void hgl_luz_vertice(Ctx *C, const float *e, const float *n, float *col, float *colb)
{
	float ep[3];
	if (e[3] != 1 && e[3] != 0) { float w = 1.0f / e[3]; ep[0] = e[0] * w; ep[1] = e[1] * w; ep[2] = e[2] * w; }
	else { ep[0] = e[0]; ep[1] = e[1]; ep[2] = e[2]; }
	luz_cara(C, &C->luz.m[0], ep, n, col);
	if (C->luz.dos_caras) {
		float nb[3] = { -n[0], -n[1], -n[2] };
		luz_cara(C, &C->luz.m[1], ep, nb, colb);
	}
}

/* ---- la transformación de un vértice ---- */
static float punto_plano(const float *p, const float *v) { return p[0] * v[0] + p[1] * v[1] + p[2] * v[2] + p[3] * v[3]; }

void hgl_transforma(Ctx *C, Vtx *v, const float *o)
{
	float n[3];
	u32 cod = 0;
	int u, k;
	hgl_mat_al_dia(C);
	mat_xf4(&C->mvp, o, v->c);
	if (C->necesita_ojo) mat_xf4(&C->mv[C->nmv], o, v->e);
	/* el color */
	if (C->hab.luz) {
		const float *a = C->cur.nor, *m = C->nor;
		n[0] = m[0] * a[0] + m[1] * a[1] + m[2] * a[2];
		n[1] = m[3] * a[0] + m[4] * a[1] + m[5] * a[2];
		n[2] = m[6] * a[0] + m[7] * a[1] + m[8] * a[2];
		if (C->hab.normaliza) {
			float l = n[0] * n[0] + n[1] * n[1] + n[2] * n[2];
			if (l > 0 && l != 1) { l = 1.0f / hgl_raiz(l); n[0] *= l; n[1] *= l; n[2] *= l; }
		}
		hgl_luz_vertice(C, v->e, n, v->col, v->colb);
	} else {
		v->col[0] = C->cur.col[0]; v->col[1] = C->cur.col[1]; v->col[2] = C->cur.col[2]; v->col[3] = C->cur.col[3];
	}
	/* las coordenadas de textura */
	for (u = 0; u < UNIDADES; u++) {
		float *t = v->t[u];
		if (!C->hab.tex2d[u] && !C->hab.tex1d[u]) continue;
		t[0] = C->cur.tc[u][0]; t[1] = C->cur.tc[u][1]; t[2] = C->cur.tc[u][2]; t[3] = C->cur.tc[u][3];
		for (k = 0; k < 4; k++) {
			const TexGen *g;
			if (!C->hab.gen[u][k]) continue;
			g = &C->tex.gen[u][k];
			if (g->modo == GL_OBJECT_LINEAR) t[k] = punto_plano(g->obj, o);
			else if (g->modo == GL_EYE_LINEAR) t[k] = punto_plano(g->ojo, v->e);
			else if (g->modo == GL_SPHERE_MAP && k < 2) {
				float ue[3], l, nd, r[3], mm, ne[3];
				const float *a = C->cur.nor, *m = C->nor;
				hgl_mat_al_dia(C);
				ne[0] = m[0] * a[0] + m[1] * a[1] + m[2] * a[2];
				ne[1] = m[3] * a[0] + m[4] * a[1] + m[5] * a[2];
				ne[2] = m[6] * a[0] + m[7] * a[1] + m[8] * a[2];
				l = hgl_raiz(ne[0] * ne[0] + ne[1] * ne[1] + ne[2] * ne[2]);
				if (l > 0) { l = 1 / l; ne[0] *= l; ne[1] *= l; ne[2] *= l; }
				l = hgl_raiz(v->e[0] * v->e[0] + v->e[1] * v->e[1] + v->e[2] * v->e[2]);
				l = l > 0 ? 1 / l : 0;
				ue[0] = v->e[0] * l; ue[1] = v->e[1] * l; ue[2] = v->e[2] * l;
				nd = ne[0] * ue[0] + ne[1] * ue[1] + ne[2] * ue[2];
				r[0] = ue[0] - 2 * ne[0] * nd; r[1] = ue[1] - 2 * ne[1] * nd; r[2] = ue[2] - 2 * ne[2] * nd;
				mm = 2 * hgl_raiz(r[0] * r[0] + r[1] * r[1] + (r[2] + 1) * (r[2] + 1));
				mm = mm > 0 ? 1 / mm : 0;
				t[k] = (k == 0 ? r[0] : r[1]) * mm + 0.5f;
			}
		}
		if (!C->tx_ident[u]) {
			float q[4];
			mat_xf4(&C->tx[u][C->ntx[u]], t, q);
			t[0] = q[0]; t[1] = q[1]; t[2] = q[2]; t[3] = q[3];
		}
	}
	/* la niebla */
	if (C->hab.niebla) {
		float z = v->e[3] != 0 && v->e[3] != 1 ? v->e[2] / v->e[3] : v->e[2], f;
		if (z < 0) z = -z;
		switch (C->nie.modo) {
		case GL_LINEAR: f = C->nie.fin != C->nie.ini ? (C->nie.fin - z) / (C->nie.fin - C->nie.ini) : 1; break;
		case GL_EXP2: f = hgl_expo(-(C->nie.dens * z) * (C->nie.dens * z)); break;
		default: f = hgl_expo(-C->nie.dens * z);
		}
		/* la lineal sin recortar: es lineal en z y se puede interpolar; se recorta en cada punto */
		v->niebla = C->nie.modo == GL_LINEAR ? (f < -8 ? -8 : f > 8 ? 8 : f) : hgl_lim01(f);
	} else v->niebla = 1;
	/* el código de recorte */
	{
		float x = v->c[0], y = v->c[1], z = v->c[2], w = v->c[3], gb = BANDA * w;
		if (x < -w) cod |= 1;
		if (x > w) cod |= 2;
		if (y < -w) cod |= 4;
		if (y > w) cod |= 8;
		if (z < -w) cod |= 16;
		if (z > w) cod |= 32;
		if (w < 1e-5f || x < -gb || x > gb || y < -gb || y > gb) cod |= FUERA_BANDA;
		for (k = 0; k < PLANOS; k++)
			if (C->hab.plano[k]) {
				const double *p = C->trf.plano[k];
				if (p[0] * v->e[0] + p[1] * v->e[1] + p[2] * v->e[2] + p[3] * v->e[3] < 0) cod |= 64 << k;
			}
	}
	v->cod = (u8)cod;     /* (los 8 de abajo) */
	v->pad = (u8)(cod >> 8);
	v->borde = C->cur.borde;
	v->proy = 0;
}
static inline u32 codigo(const Vtx *v) { return v->cod | ((u32)v->pad << 8); }

/* a la ventana: x, y (hacia abajo) y z (0..65535) */
static void proyecta(Ctx *C, Vtx *v)
{
	float iw = 1.0f / v->c[3];
	float nx = v->c[0] * iw, ny = v->c[1] * iw, nz = v->c[2] * iw;
	v->iw = iw;
	v->x = C->vis.x + (nx + 1) * 0.5f * C->vis.w;
	v->y = C->al - (C->vis.y + (ny + 1) * 0.5f * C->vis.h);
	v->z = (C->vis.cerca + (nz + 1) * 0.5f * (C->vis.lejos - C->vis.cerca)) * 65535.0f;
	if (v->z < 0) v->z = 0;
	if (v->z > 65535) v->z = 65535;
	v->proy = 1;
}

/* ---- el recorte ---- */
static void interpola(Vtx *r, const Vtx *a, const Vtx *b, float t)
{
	int k, u;
	for (k = 0; k < 4; k++) {
		r->c[k] = a->c[k] + t * (b->c[k] - a->c[k]);
		r->e[k] = a->e[k] + t * (b->e[k] - a->e[k]);
		r->col[k] = a->col[k] + t * (b->col[k] - a->col[k]);
		r->colb[k] = a->colb[k] + t * (b->colb[k] - a->colb[k]);
	}
	for (u = 0; u < UNIDADES; u++)
		for (k = 0; k < 4; k++) r->t[u][k] = a->t[u][k] + t * (b->t[u][k] - a->t[u][k]);
	r->niebla = a->niebla + t * (b->niebla - a->niebla);
	r->proy = 0;
	r->cod = 0; r->pad = 0;
}
static float distancia(Ctx *C, const Vtx *v, int p)
{
	const float *c = v->c;
	switch (p) {
	case 0: return c[0] + c[3];
	case 1: return c[3] - c[0];
	case 2: return c[1] + c[3];
	case 3: return c[3] - c[1];
	case 4: return c[2] + c[3];
	case 5: return c[3] - c[2];
	default: {
		const double *q = C->trf.plano[p - 6];
		return (float)(q[0] * v->e[0] + q[1] * v->e[1] + q[2] * v->e[2] + q[3] * v->e[3]);
	}
	}
}
#define RMAX 24
typedef struct { Vtx v[RMAX * 2]; int n; } Rec;
static Vtx *nuevo(Rec *r) { return r->n < RMAX * 2 ? &r->v[r->n++] : NULL; }

/* recorta el polígono «ent» (n vértices) contra todos los planos; deja el
 * resultado en «sal»: cuántos quedan */
static int recorta(Ctx *C, Vtx **ent, int n, Vtx **sal, Rec *r)
{
	Vtx *a1[RMAX], *a2[RMAX], **in = a1, **out = a2;
	int p, k, m;
	for (k = 0; k < n && k < RMAX; k++) in[k] = ent[k];
	n = k;
	for (p = 0; p < 6 + PLANOS; p++) {
		float d0, d1;
		if (p >= 6 && !C->hab.plano[p - 6]) continue;
		m = 0;
		for (k = 0; k < n; k++) {
			Vtx *a = in[k], *b = in[(k + 1) % n];
			d0 = distancia(C, a, p);
			d1 = distancia(C, b, p);
			if (d0 >= 0) { if (m < RMAX) out[m++] = a; }
			if ((d0 >= 0) != (d1 >= 0)) {
				Vtx *x = nuevo(r);
				float t;
				if (!x || m >= RMAX) continue;
				/* siempre desde el de dentro, para que el corte sea el mismo en los dos polígonos */
				if (d0 >= 0) { t = d0 / (d0 - d1); interpola(x, a, b, t); x->borde = a->borde; }
				else { t = d1 / (d1 - d0); interpola(x, b, a, t); x->borde = 1; }
				out[m++] = x;
			}
		}
		{ Vtx **t = in; in = out; out = t; }
		n = m;
		if (n < 3 && n != 2) break;
	}
	for (k = 0; k < n; k++) sal[k] = in[k];
	return n;
}

/* ---- dibujar ---- */
static void poligono(Ctx *C, Vtx **v, int n, const Vtx *prov)
{
	Vtx *l[RMAX], *ll;
	Rec *r = NULL;
	Rec rec;
	u32 o = 0, y = 0xFFFFFFFF;
	int k, m, frente, lado, modo;
	float area = 0;
	const float *plano = NULL;
	for (k = 0; k < n; k++) { u32 c = codigo(v[k]); o |= c; y &= c; }
	if (y & 0x0FFF) return;
	if (o & HAY_QUE_RECORTAR) {
		r = &rec;
		r->n = 0;
		m = recorta(C, v, n, l, r);
		if (m < 3) return;
	} else {
		for (k = 0; k < n && k < RMAX; k++) l[k] = v[k];
		m = k;
	}
	for (k = 0; k < m; k++) if (!l[k]->proy) proyecta(C, l[k]);
	for (k = 0; k < m; k++) {
		const Vtx *a = l[k], *b = l[(k + 1) % m];
		area += a->x * b->y - b->x * a->y;
	}
	area = -area;   /* con y hacia arriba */
	if (area == 0) return;
	frente = (C->pol.frente == GL_CCW) ? area > 0 : area < 0;
	if (C->hab.cara) {
		if (C->pol.cara == GL_FRONT_AND_BACK) return;
		if (C->pol.cara == GL_FRONT && frente) return;
		if (C->pol.cara == GL_BACK && !frente) return;
	}
	lado = (!frente && C->hab.luz && C->luz.dos_caras) ? 1 : 0;
	if (C->luz.sombreado == GL_FLAT) plano = lado ? prov->colb : prov->col;
	modo = C->pol.modo[frente ? 0 : 1];
	if (modo == GL_FILL) {
		ll = l[0];
		for (k = 1; k + 1 < m; k++) hgl_triangulo(C, ll, l[k], l[k + 1], lado, plano);
	} else if (modo == GL_LINE) {
		for (k = 0; k < m; k++)
			if (l[k]->borde) hgl_linea(C, l[k], l[(k + 1) % m], lado, plano);
	} else {
		for (k = 0; k < m; k++)
			if (l[k]->borde) hgl_punto(C, l[k], lado);
	}
}
static void triangulo(Ctx *C, Vtx *a, Vtx *b, Vtx *c, const Vtx *prov)
{
	Vtx *v[3];
	u32 ca = codigo(a), cb = codigo(b), cc = codigo(c);
	if (ca & cb & cc & 0x0FFF) return;
	/* lo más frecuente: dentro de la banda, de frente o sin descartar, relleno */
	if (!((ca | cb | cc) & HAY_QUE_RECORTAR) && C->pol.modo[0] == GL_FILL && C->pol.modo[1] == GL_FILL &&
	    !(C->hab.luz && C->luz.dos_caras)) {
		float area;
		int frente;
		if (!a->proy) proyecta(C, a);
		if (!b->proy) proyecta(C, b);
		if (!c->proy) proyecta(C, c);
		area = (b->x - a->x) * (c->y - a->y) - (c->x - a->x) * (b->y - a->y);
		area = -area;
		if (area == 0) return;
		frente = (C->pol.frente == GL_CCW) ? area > 0 : area < 0;
		if (C->hab.cara) {
			if (C->pol.cara == GL_FRONT_AND_BACK) return;
			if (C->pol.cara == GL_FRONT && frente) return;
			if (C->pol.cara == GL_BACK && !frente) return;
		}
		hgl_triangulo(C, a, b, c, 0, C->luz.sombreado == GL_FLAT ? prov->col : NULL);
		return;
	}
	v[0] = a; v[1] = b; v[2] = c;
	poligono(C, v, 3, prov);
}
static void linea(Ctx *C, Vtx *a, Vtx *b)
{
	u32 ca = codigo(a), cb = codigo(b);
	const float *plano = C->luz.sombreado == GL_FLAT ? b->col : NULL;
	if (ca & cb & 0x0FFF) return;
	if ((ca | cb) & (0x0FFF | FUERA_BANDA)) {
		/* el recorte de una línea: contra cada plano */
		Vtx x0 = *a, x1 = *b, y;
		int p;
		for (p = 0; p < 6 + PLANOS; p++) {
			float d0, d1, t;
			if (p >= 6 && !C->hab.plano[p - 6]) continue;
			d0 = distancia(C, &x0, p);
			d1 = distancia(C, &x1, p);
			if (d0 < 0 && d1 < 0) return;
			if (d0 >= 0 && d1 >= 0) continue;
			t = d0 / (d0 - d1);
			interpola(&y, &x0, &x1, t);
			if (d0 < 0) x0 = y; else x1 = y;
		}
		proyecta(C, &x0);
		proyecta(C, &x1);
		hgl_linea(C, &x0, &x1, 0, plano);
		return;
	}
	if (!a->proy) proyecta(C, a);
	if (!b->proy) proyecta(C, b);
	hgl_linea(C, a, b, 0, plano);
}
static void punto(Ctx *C, Vtx *a)
{
	if (codigo(a) & 0x0FFF) return;
	if (!a->proy) proyecta(C, a);
	hgl_punto(C, a, 0);
}

/* ---- glBegin, glVertex, glEnd ---- */
void i_empieza(u32 modo)
{
	CTX();
	if (C->prim >= 0) { hgl_error(C, GL_INVALID_OPERATION); return; }
	if (modo > GL_POLYGON) { hgl_error(C, GL_INVALID_ENUM); return; }
	C->prim = (int)modo;
	C->nv = 0;
	C->par = 0;
	C->pol_n = 0;
}

static Vtx *pol_mas(Ctx *C)
{
	if (C->pol_n >= C->pol_cap) {
		int nc = C->pol_cap ? C->pol_cap * 2 : 64;
		Vtx *n = hgl_pedir(sizeof(Vtx) * nc);
		if (!n) return NULL;
		if (C->pol_v) { hgl_copia(n, C->pol_v, sizeof(Vtx) * C->pol_n); hgl_soltar(C->pol_v); }
		C->pol_v = n;
		C->pol_cap = nc;
	}
	return &C->pol_v[C->pol_n++];
}

void i_vertice(float x, float y, float z, float w)
{
	CTX();
	float o[4] = { x, y, z, w };
	Vtx *v;
	int i = C->nv;
	switch (C->prim) {
	case GL_POINTS:
		v = &C->vb[0];
		hgl_transforma(C, v, o);
		punto(C, v);
		break;
	case GL_LINES:
		v = &C->vb[i];
		hgl_transforma(C, v, o);
		if (i == 1) { linea(C, &C->vb[0], v); C->nv = 0; } else C->nv = 1;
		break;
	case GL_LINE_STRIP:
	case GL_LINE_LOOP:
		v = &C->vb[i & 1];
		hgl_transforma(C, v, o);
		if (i == 0 && C->prim == GL_LINE_LOOP) C->primero = *v;
		if (i > 0) linea(C, &C->vb[(i - 1) & 1], v);
		C->nv = i + 1;
		break;
	case GL_TRIANGLES:
		v = &C->vb[i];
		hgl_transforma(C, v, o);
		if (i == 2) { triangulo(C, &C->vb[0], &C->vb[1], v, v); C->nv = 0; } else C->nv = i + 1;
		break;
	case GL_TRIANGLE_STRIP:
		v = &C->vb[i % 3];
		hgl_transforma(C, v, o);
		if (i >= 2) {
			Vtx *a = &C->vb[(i - 2) % 3], *b = &C->vb[(i - 1) % 3];
			if (i & 1) triangulo(C, b, a, v, v); else triangulo(C, a, b, v, v);
		}
		C->nv = i + 1;      /* (el índice crece sin parar: de él salen la ranura y la paridad) */
		break;
	case GL_TRIANGLE_FAN:
		v = &C->vb[i == 0 ? 0 : 1 + (i & 1)];
		hgl_transforma(C, v, o);
		if (i >= 2) triangulo(C, &C->vb[0], &C->vb[1 + ((i - 1) & 1)], v, v);
		C->nv = i + 1;
		if (C->nv == 4) C->nv = 2;
		break;
	case GL_QUADS:
		v = &C->vb[i];
		hgl_transforma(C, v, o);
		if (i == 3) {
			Vtx *q[4] = { &C->vb[0], &C->vb[1], &C->vb[2], &C->vb[3] };
			poligono(C, q, 4, v);
			C->nv = 0;
		} else C->nv = i + 1;
		break;
	case GL_QUAD_STRIP:
		v = &C->vb[i];
		hgl_transforma(C, v, o);
		if (i == 3) {
			Vtx *q[4] = { &C->vb[0], &C->vb[1], &C->vb[3], &C->vb[2] };
			poligono(C, q, 4, v);
			C->vb[0] = C->vb[2];
			C->vb[1] = C->vb[3];
			C->nv = 2;
		} else C->nv = i + 1;
		break;
	case GL_POLYGON:
		v = pol_mas(C);
		if (!v) { hgl_error(C, GL_OUT_OF_MEMORY); return; }
		hgl_transforma(C, v, o);
		break;
	default:
		break;    /* fuera de glBegin/glEnd: no hace nada */
	}
}
void i_acaba(void)
{
	CTX();
	if (C->prim < 0) { hgl_error(C, GL_INVALID_OPERATION); return; }
	if (C->prim == GL_LINE_LOOP && C->nv >= 2) linea(C, &C->vb[(C->nv - 1) & 1], &C->primero);
	if (C->prim == GL_POLYGON && C->pol_n >= 3) {
		Vtx *q[RMAX];
		int k, n = C->pol_n;
		if (n <= RMAX - 4) {
			for (k = 0; k < n; k++) q[k] = &C->pol_v[k];
			poligono(C, q, n, &C->pol_v[0]);
		} else {
			/* un polígono enorme: en abanico (es convexo) */
			for (k = 1; k + 1 < n; k++) {
				q[0] = &C->pol_v[0]; q[1] = &C->pol_v[k]; q[2] = &C->pol_v[k + 1];
				poligono(C, q, 3, &C->pol_v[0]);
			}
		}
	}
	C->prim = -1;
}

void i_color(float r, float g, float b, float a)
{
	CTX();
	float *c = C->cur.col;
	c[0] = r; c[1] = g; c[2] = b; c[3] = a;
	if (C->hab.colmat) {
		int k, f;
		for (f = 0; f < 2; f++) {
			Material *m = &C->luz.m[f];
			if (f == 0 && C->luz.cm_cara == GL_BACK) continue;
			if (f == 1 && C->luz.cm_cara == GL_FRONT) continue;
			for (k = 0; k < 4; k++)
				switch (C->luz.cm_modo) {
				case GL_AMBIENT: m->amb[k] = c[k]; break;
				case GL_DIFFUSE: m->dif[k] = c[k]; break;
				case GL_SPECULAR: m->esp[k] = c[k]; break;
				case GL_EMISSION: m->emi[k] = c[k]; break;
				default: m->amb[k] = m->dif[k] = c[k];
				}
		}
	}
}
void i_normal(float x, float y, float z) { CTX(); C->cur.nor[0] = x; C->cur.nor[1] = y; C->cur.nor[2] = z; }
void i_texc(u32 u, float s, float t, float r, float q)
{
	CTX();
	float *c;
	if (u >= UNIDADES) return;
	c = C->cur.tc[u];
	c[0] = s; c[1] = t; c[2] = r; c[3] = q;
}
void i_borde(u32 b) { CTX(); C->cur.borde = b != 0; }

void hgl_vertice_ini(Ctx *C) { C->prim = -1; }

/* ---- las de fuera: glBegin, glEnd, glVertex*, glColor*, glNormal*, glTexCoord* ---- */
void APIENTRY glBegin(GLenum m) { CTX(); LISTA(i_empieza, 1, m); i_empieza(m); }
void APIENTRY glEnd(void) { CTX(); LISTA0(i_acaba); i_acaba(); }

/* (todas las variantes pasan por una: la DLL queda más pequeña) */
static void __attribute__((noinline)) vert_(float x, float y, float z, float w) { CTX(); LISTA(i_vertice, 4, F(x), F(y), F(z), F(w)); i_vertice(x, y, z, w); }
#define VERT(x, y, z, w) vert_(x, y, z, w)
void APIENTRY glVertex2f(GLfloat x, GLfloat y) { VERT(x, y, 0.0f, 1.0f); }
void APIENTRY glVertex3f(GLfloat x, GLfloat y, GLfloat z) { VERT(x, y, z, 1.0f); }
void APIENTRY glVertex4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w) { VERT(x, y, z, w); }
void APIENTRY glVertex2fv(const GLfloat *v) { VERT(v[0], v[1], 0.0f, 1.0f); }
void APIENTRY glVertex3fv(const GLfloat *v) { VERT(v[0], v[1], v[2], 1.0f); }
void APIENTRY glVertex4fv(const GLfloat *v) { VERT(v[0], v[1], v[2], v[3]); }
void APIENTRY glVertex2d(GLdouble x, GLdouble y) { VERT((float)x, (float)y, 0.0f, 1.0f); }
void APIENTRY glVertex3d(GLdouble x, GLdouble y, GLdouble z) { VERT((float)x, (float)y, (float)z, 1.0f); }
void APIENTRY glVertex4d(GLdouble x, GLdouble y, GLdouble z, GLdouble w) { VERT((float)x, (float)y, (float)z, (float)w); }
void APIENTRY glVertex2dv(const GLdouble *v) { VERT((float)v[0], (float)v[1], 0.0f, 1.0f); }
void APIENTRY glVertex3dv(const GLdouble *v) { VERT((float)v[0], (float)v[1], (float)v[2], 1.0f); }
void APIENTRY glVertex4dv(const GLdouble *v) { VERT((float)v[0], (float)v[1], (float)v[2], (float)v[3]); }
void APIENTRY glVertex2i(GLint x, GLint y) { VERT((float)x, (float)y, 0.0f, 1.0f); }
void APIENTRY glVertex3i(GLint x, GLint y, GLint z) { VERT((float)x, (float)y, (float)z, 1.0f); }
void APIENTRY glVertex4i(GLint x, GLint y, GLint z, GLint w) { VERT((float)x, (float)y, (float)z, (float)w); }
void APIENTRY glVertex2iv(const GLint *v) { VERT((float)v[0], (float)v[1], 0.0f, 1.0f); }
void APIENTRY glVertex3iv(const GLint *v) { VERT((float)v[0], (float)v[1], (float)v[2], 1.0f); }
void APIENTRY glVertex4iv(const GLint *v) { VERT((float)v[0], (float)v[1], (float)v[2], (float)v[3]); }
void APIENTRY glVertex2s(GLshort x, GLshort y) { VERT((float)x, (float)y, 0.0f, 1.0f); }
void APIENTRY glVertex3s(GLshort x, GLshort y, GLshort z) { VERT((float)x, (float)y, (float)z, 1.0f); }
void APIENTRY glVertex4s(GLshort x, GLshort y, GLshort z, GLshort w) { VERT((float)x, (float)y, (float)z, (float)w); }
void APIENTRY glVertex2sv(const GLshort *v) { VERT((float)v[0], (float)v[1], 0.0f, 1.0f); }
void APIENTRY glVertex3sv(const GLshort *v) { VERT((float)v[0], (float)v[1], (float)v[2], 1.0f); }
void APIENTRY glVertex4sv(const GLshort *v) { VERT((float)v[0], (float)v[1], (float)v[2], (float)v[3]); }

static void __attribute__((noinline)) col_(float r, float g, float b, float a) { CTX(); LISTA(i_color, 4, F(r), F(g), F(b), F(a)); i_color(r, g, b, a); }
#define COL(r, g, b, a) col_(r, g, b, a)
#define UB(x) ((float)(x) * (1.0f / 255.0f))
#define B_(x) (((float)(x) * 2 + 1) * (1.0f / 255.0f))
#define US(x) ((float)(x) * (1.0f / 65535.0f))
#define S_(x) (((float)(x) * 2 + 1) * (1.0f / 65535.0f))
#define UI(x) ((float)((double)(x) * (1.0 / 4294967295.0)))
#define I_(x) ((float)(((double)(x) * 2 + 1) * (1.0 / 4294967295.0)))
void APIENTRY glColor3f(GLfloat r, GLfloat g, GLfloat b) { COL(r, g, b, 1.0f); }
void APIENTRY glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a) { COL(r, g, b, a); }
void APIENTRY glColor3fv(const GLfloat *v) { COL(v[0], v[1], v[2], 1.0f); }
void APIENTRY glColor4fv(const GLfloat *v) { COL(v[0], v[1], v[2], v[3]); }
void APIENTRY glColor3d(GLdouble r, GLdouble g, GLdouble b) { COL((float)r, (float)g, (float)b, 1.0f); }
void APIENTRY glColor4d(GLdouble r, GLdouble g, GLdouble b, GLdouble a) { COL((float)r, (float)g, (float)b, (float)a); }
void APIENTRY glColor3dv(const GLdouble *v) { COL((float)v[0], (float)v[1], (float)v[2], 1.0f); }
void APIENTRY glColor4dv(const GLdouble *v) { COL((float)v[0], (float)v[1], (float)v[2], (float)v[3]); }
void APIENTRY glColor3ub(GLubyte r, GLubyte g, GLubyte b) { COL(UB(r), UB(g), UB(b), 1.0f); }
void APIENTRY glColor4ub(GLubyte r, GLubyte g, GLubyte b, GLubyte a) { COL(UB(r), UB(g), UB(b), UB(a)); }
void APIENTRY glColor3ubv(const GLubyte *v) { COL(UB(v[0]), UB(v[1]), UB(v[2]), 1.0f); }
void APIENTRY glColor4ubv(const GLubyte *v) { COL(UB(v[0]), UB(v[1]), UB(v[2]), UB(v[3])); }
void APIENTRY glColor3b(GLbyte r, GLbyte g, GLbyte b) { COL(B_(r), B_(g), B_(b), 1.0f); }
void APIENTRY glColor4b(GLbyte r, GLbyte g, GLbyte b, GLbyte a) { COL(B_(r), B_(g), B_(b), B_(a)); }
void APIENTRY glColor3bv(const GLbyte *v) { COL(B_(v[0]), B_(v[1]), B_(v[2]), 1.0f); }
void APIENTRY glColor4bv(const GLbyte *v) { COL(B_(v[0]), B_(v[1]), B_(v[2]), B_(v[3])); }
void APIENTRY glColor3us(GLushort r, GLushort g, GLushort b) { COL(US(r), US(g), US(b), 1.0f); }
void APIENTRY glColor4us(GLushort r, GLushort g, GLushort b, GLushort a) { COL(US(r), US(g), US(b), US(a)); }
void APIENTRY glColor3usv(const GLushort *v) { COL(US(v[0]), US(v[1]), US(v[2]), 1.0f); }
void APIENTRY glColor4usv(const GLushort *v) { COL(US(v[0]), US(v[1]), US(v[2]), US(v[3])); }
void APIENTRY glColor3s(GLshort r, GLshort g, GLshort b) { COL(S_(r), S_(g), S_(b), 1.0f); }
void APIENTRY glColor4s(GLshort r, GLshort g, GLshort b, GLshort a) { COL(S_(r), S_(g), S_(b), S_(a)); }
void APIENTRY glColor3sv(const GLshort *v) { COL(S_(v[0]), S_(v[1]), S_(v[2]), 1.0f); }
void APIENTRY glColor4sv(const GLshort *v) { COL(S_(v[0]), S_(v[1]), S_(v[2]), S_(v[3])); }
void APIENTRY glColor3ui(GLuint r, GLuint g, GLuint b) { COL(UI(r), UI(g), UI(b), 1.0f); }
void APIENTRY glColor4ui(GLuint r, GLuint g, GLuint b, GLuint a) { COL(UI(r), UI(g), UI(b), UI(a)); }
void APIENTRY glColor3uiv(const GLuint *v) { COL(UI(v[0]), UI(v[1]), UI(v[2]), 1.0f); }
void APIENTRY glColor4uiv(const GLuint *v) { COL(UI(v[0]), UI(v[1]), UI(v[2]), UI(v[3])); }
void APIENTRY glColor3i(GLint r, GLint g, GLint b) { COL(I_(r), I_(g), I_(b), 1.0f); }
void APIENTRY glColor4i(GLint r, GLint g, GLint b, GLint a) { COL(I_(r), I_(g), I_(b), I_(a)); }
void APIENTRY glColor3iv(const GLint *v) { COL(I_(v[0]), I_(v[1]), I_(v[2]), 1.0f); }
void APIENTRY glColor4iv(const GLint *v) { COL(I_(v[0]), I_(v[1]), I_(v[2]), I_(v[3])); }

static void __attribute__((noinline)) nor_(float x, float y, float z) { CTX(); LISTA(i_normal, 3, F(x), F(y), F(z)); i_normal(x, y, z); }
#define NOR(x, y, z) nor_(x, y, z)
void APIENTRY glNormal3f(GLfloat x, GLfloat y, GLfloat z) { NOR(x, y, z); }
void APIENTRY glNormal3fv(const GLfloat *v) { NOR(v[0], v[1], v[2]); }
void APIENTRY glNormal3d(GLdouble x, GLdouble y, GLdouble z) { NOR((float)x, (float)y, (float)z); }
void APIENTRY glNormal3dv(const GLdouble *v) { NOR((float)v[0], (float)v[1], (float)v[2]); }
void APIENTRY glNormal3b(GLbyte x, GLbyte y, GLbyte z) { NOR(B_(x), B_(y), B_(z)); }
void APIENTRY glNormal3bv(const GLbyte *v) { NOR(B_(v[0]), B_(v[1]), B_(v[2])); }
void APIENTRY glNormal3s(GLshort x, GLshort y, GLshort z) { NOR(S_(x), S_(y), S_(z)); }
void APIENTRY glNormal3sv(const GLshort *v) { NOR(S_(v[0]), S_(v[1]), S_(v[2])); }
void APIENTRY glNormal3i(GLint x, GLint y, GLint z) { NOR(I_(x), I_(y), I_(z)); }
void APIENTRY glNormal3iv(const GLint *v) { NOR(I_(v[0]), I_(v[1]), I_(v[2])); }

static void __attribute__((noinline)) tc_(u32 u, float s, float t, float r, float q) { CTX(); LISTA(i_texc, 5, u, F(s), F(t), F(r), F(q)); i_texc(u, s, t, r, q); }
#define TC(u, s, t, r, q) tc_(u, s, t, r, q)
void APIENTRY glTexCoord1f(GLfloat s) { TC(0, s, 0.0f, 0.0f, 1.0f); }
void APIENTRY glTexCoord2f(GLfloat s, GLfloat t) { TC(0, s, t, 0.0f, 1.0f); }
void APIENTRY glTexCoord3f(GLfloat s, GLfloat t, GLfloat r) { TC(0, s, t, r, 1.0f); }
void APIENTRY glTexCoord4f(GLfloat s, GLfloat t, GLfloat r, GLfloat q) { TC(0, s, t, r, q); }
void APIENTRY glTexCoord1fv(const GLfloat *v) { TC(0, v[0], 0.0f, 0.0f, 1.0f); }
void APIENTRY glTexCoord2fv(const GLfloat *v) { TC(0, v[0], v[1], 0.0f, 1.0f); }
void APIENTRY glTexCoord3fv(const GLfloat *v) { TC(0, v[0], v[1], v[2], 1.0f); }
void APIENTRY glTexCoord4fv(const GLfloat *v) { TC(0, v[0], v[1], v[2], v[3]); }
void APIENTRY glTexCoord1d(GLdouble s) { TC(0, (float)s, 0.0f, 0.0f, 1.0f); }
void APIENTRY glTexCoord2d(GLdouble s, GLdouble t) { TC(0, (float)s, (float)t, 0.0f, 1.0f); }
void APIENTRY glTexCoord3d(GLdouble s, GLdouble t, GLdouble r) { TC(0, (float)s, (float)t, (float)r, 1.0f); }
void APIENTRY glTexCoord4d(GLdouble s, GLdouble t, GLdouble r, GLdouble q) { TC(0, (float)s, (float)t, (float)r, (float)q); }
void APIENTRY glTexCoord1dv(const GLdouble *v) { TC(0, (float)v[0], 0.0f, 0.0f, 1.0f); }
void APIENTRY glTexCoord2dv(const GLdouble *v) { TC(0, (float)v[0], (float)v[1], 0.0f, 1.0f); }
void APIENTRY glTexCoord3dv(const GLdouble *v) { TC(0, (float)v[0], (float)v[1], (float)v[2], 1.0f); }
void APIENTRY glTexCoord4dv(const GLdouble *v) { TC(0, (float)v[0], (float)v[1], (float)v[2], (float)v[3]); }
void APIENTRY glTexCoord1i(GLint s) { TC(0, (float)s, 0.0f, 0.0f, 1.0f); }
void APIENTRY glTexCoord2i(GLint s, GLint t) { TC(0, (float)s, (float)t, 0.0f, 1.0f); }
void APIENTRY glTexCoord3i(GLint s, GLint t, GLint r) { TC(0, (float)s, (float)t, (float)r, 1.0f); }
void APIENTRY glTexCoord4i(GLint s, GLint t, GLint r, GLint q) { TC(0, (float)s, (float)t, (float)r, (float)q); }
void APIENTRY glTexCoord1iv(const GLint *v) { TC(0, (float)v[0], 0.0f, 0.0f, 1.0f); }
void APIENTRY glTexCoord2iv(const GLint *v) { TC(0, (float)v[0], (float)v[1], 0.0f, 1.0f); }
void APIENTRY glTexCoord3iv(const GLint *v) { TC(0, (float)v[0], (float)v[1], (float)v[2], 1.0f); }
void APIENTRY glTexCoord4iv(const GLint *v) { TC(0, (float)v[0], (float)v[1], (float)v[2], (float)v[3]); }
void APIENTRY glTexCoord1s(GLshort s) { TC(0, (float)s, 0.0f, 0.0f, 1.0f); }
void APIENTRY glTexCoord2s(GLshort s, GLshort t) { TC(0, (float)s, (float)t, 0.0f, 1.0f); }
void APIENTRY glTexCoord3s(GLshort s, GLshort t, GLshort r) { TC(0, (float)s, (float)t, (float)r, 1.0f); }
void APIENTRY glTexCoord4s(GLshort s, GLshort t, GLshort r, GLshort q) { TC(0, (float)s, (float)t, (float)r, (float)q); }
void APIENTRY glTexCoord1sv(const GLshort *v) { TC(0, (float)v[0], 0.0f, 0.0f, 1.0f); }
void APIENTRY glTexCoord2sv(const GLshort *v) { TC(0, (float)v[0], (float)v[1], 0.0f, 1.0f); }
void APIENTRY glTexCoord3sv(const GLshort *v) { TC(0, (float)v[0], (float)v[1], (float)v[2], 1.0f); }
void APIENTRY glTexCoord4sv(const GLshort *v) { TC(0, (float)v[0], (float)v[1], (float)v[2], (float)v[3]); }

/* las varias texturas (GL_ARB_multitexture y GL_SGIS_multitexture) */
static u32 unidad(u32 t) { return t >= GL_TEXTURE0_ARB && t < GL_TEXTURE0_ARB + 32 ? t - GL_TEXTURE0_ARB : t >= GL_TEXTURE0_SGIS && t < GL_TEXTURE0_SGIS + 8 ? t - GL_TEXTURE0_SGIS : t < 8 ? t : 99; }
void APIENTRY glMultiTexCoord1fARB(GLenum u, GLfloat s) { TC(unidad(u), s, 0.0f, 0.0f, 1.0f); }
void APIENTRY glMultiTexCoord2fARB(GLenum u, GLfloat s, GLfloat t) { TC(unidad(u), s, t, 0.0f, 1.0f); }
void APIENTRY glMultiTexCoord2fvARB(GLenum u, const GLfloat *v) { TC(unidad(u), v[0], v[1], 0.0f, 1.0f); }
void APIENTRY glMultiTexCoord3fARB(GLenum u, GLfloat s, GLfloat t, GLfloat r) { TC(unidad(u), s, t, r, 1.0f); }
void APIENTRY glMultiTexCoord4fARB(GLenum u, GLfloat s, GLfloat t, GLfloat r, GLfloat q) { TC(unidad(u), s, t, r, q); }
void APIENTRY glMTexCoord2fSGIS(GLenum u, GLfloat s, GLfloat t) { TC(unidad(u), s, t, 0.0f, 1.0f); }
void APIENTRY glMTexCoord2fvSGIS(GLenum u, const GLfloat *v) { TC(unidad(u), v[0], v[1], 0.0f, 1.0f); }
static void i_activa(u32 u) { CTX(); if (u >= UNIDADES) { hgl_error(C, GL_INVALID_ENUM); return; } C->tex.activa = u; }
void APIENTRY glActiveTextureARB(GLenum t) { CTX(); u32 u = unidad(t); LISTA(i_activa, 1, u); i_activa(u); }
void APIENTRY glSelectTextureSGIS(GLenum t) { glActiveTextureARB(t); }
void APIENTRY glClientActiveTextureARB(GLenum t) { CTX(); u32 u = unidad(t); if (u >= UNIDADES) { hgl_error(C, GL_INVALID_ENUM); return; } C->arr.tex_cli = u; }

void APIENTRY glEdgeFlag(GLboolean b) { CTX(); LISTA(i_borde, 1, b); i_borde(b); }
void APIENTRY glEdgeFlagv(const GLboolean *b) { glEdgeFlag(*b); }
void APIENTRY glIndexf(GLfloat c) { CTX(); C->cur.indice = c; }
void APIENTRY glIndexd(GLdouble c) { glIndexf((float)c); }
void APIENTRY glIndexi(GLint c) { glIndexf((float)c); }
void APIENTRY glIndexs(GLshort c) { glIndexf((float)c); }
void APIENTRY glIndexub(GLubyte c) { glIndexf((float)c); }
void APIENTRY glIndexfv(const GLfloat *c) { glIndexf(c[0]); }
void APIENTRY glIndexdv(const GLdouble *c) { glIndexf((float)c[0]); }
void APIENTRY glIndexiv(const GLint *c) { glIndexf((float)c[0]); }
void APIENTRY glIndexsv(const GLshort *c) { glIndexf((float)c[0]); }
void APIENTRY glIndexubv(const GLubyte *c) { glIndexf((float)c[0]); }

/* glRect: un polígono de cuatro vértices en z = 0 */
void APIENTRY glRectf(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2)
{
	glBegin(GL_POLYGON);
	glVertex2f(x1, y1); glVertex2f(x2, y1); glVertex2f(x2, y2); glVertex2f(x1, y2);
	glEnd();
}
void APIENTRY glRectd(GLdouble a, GLdouble b, GLdouble c, GLdouble d) { glRectf((float)a, (float)b, (float)c, (float)d); }
void APIENTRY glRecti(GLint a, GLint b, GLint c, GLint d) { glRectf((float)a, (float)b, (float)c, (float)d); }
void APIENTRY glRects(GLshort a, GLshort b, GLshort c, GLshort d) { glRectf((float)a, (float)b, (float)c, (float)d); }
void APIENTRY glRectfv(const GLfloat *a, const GLfloat *b) { glRectf(a[0], a[1], b[0], b[1]); }
void APIENTRY glRectdv(const GLdouble *a, const GLdouble *b) { glRectf((float)a[0], (float)a[1], (float)b[0], (float)b[1]); }
void APIENTRY glRectiv(const GLint *a, const GLint *b) { glRectf((float)a[0], (float)a[1], (float)b[0], (float)b[1]); }
void APIENTRY glRectsv(const GLshort *a, const GLshort *b) { glRectf((float)a[0], (float)a[1], (float)b[0], (float)b[1]); }

/* ---- la luz y los materiales ---- */
static void i_luz(u32 n, u32 p, float a, float b, float c, float d)
{
	CTX();
	Luz *l;
	float v[4] = { a, b, c, d };
	if (n < GL_LIGHT0 || n >= GL_LIGHT0 + LUCES) { hgl_error(C, GL_INVALID_ENUM); return; }
	l = &C->luz.l[n - GL_LIGHT0];
	switch (p) {
	case GL_AMBIENT: l->amb[0] = a; l->amb[1] = b; l->amb[2] = c; l->amb[3] = d; break;
	case GL_DIFFUSE: l->dif[0] = a; l->dif[1] = b; l->dif[2] = c; l->dif[3] = d; break;
	case GL_SPECULAR: l->esp[0] = a; l->esp[1] = b; l->esp[2] = c; l->esp[3] = d; break;
	case GL_POSITION: {
		float q;
		mat_xf4(&C->mv[C->nmv], v, l->pos);
		q = hgl_raiz(l->pos[0] * l->pos[0] + l->pos[1] * l->pos[1] + l->pos[2] * l->pos[2]);
		q = q > 0 ? 1 / q : 0;
		l->dirn[0] = l->pos[0] * q; l->dirn[1] = l->pos[1] * q; l->dirn[2] = l->pos[2] * q;
		break;
	}
	case GL_SPOT_DIRECTION: {
		const float *m = C->mv[C->nmv].m;
		float x = m[0] * a + m[4] * b + m[8] * c, y = m[1] * a + m[5] * b + m[9] * c, z = m[2] * a + m[6] * b + m[10] * c;
		float q = hgl_raiz(x * x + y * y + z * z);
		l->dir_o[0] = x; l->dir_o[1] = y; l->dir_o[2] = z;
		if (q > 0) { q = 1 / q; x *= q; y *= q; z *= q; }
		l->dir[0] = x; l->dir[1] = y; l->dir[2] = z;
		break;
	}
	case GL_SPOT_EXPONENT: if (a < 0 || a > 128) { hgl_error(C, GL_INVALID_VALUE); return; } l->exp = a; break;
	case GL_SPOT_CUTOFF:
		if ((a < 0 || a > 90) && a != 180) { hgl_error(C, GL_INVALID_VALUE); return; }
		l->corte = a; l->cos_corte = hgl_coseno(a * 3.14159265358979f / 180.0f);
		break;
	case GL_CONSTANT_ATTENUATION: l->att[0] = a; break;
	case GL_LINEAR_ATTENUATION: l->att[1] = a; break;
	case GL_QUADRATIC_ATTENUATION: l->att[2] = a; break;
	default: hgl_error(C, GL_INVALID_ENUM);
	}
}
static int luz_n(u32 p) { return p == GL_AMBIENT || p == GL_DIFFUSE || p == GL_SPECULAR || p == GL_POSITION ? 4 : p == GL_SPOT_DIRECTION ? 3 : 1; }
void APIENTRY glLightfv(GLenum n, GLenum p, const GLfloat *v)
{
	CTX();
	float a[4] = { 0, 0, 0, 0 };
	int k, m = luz_n(p);
	for (k = 0; k < m; k++) a[k] = v[k];
	LISTA(i_luz, 6, n, p, F(a[0]), F(a[1]), F(a[2]), F(a[3]));
	i_luz(n, p, a[0], a[1], a[2], a[3]);
}
void APIENTRY glLightf(GLenum n, GLenum p, GLfloat v) { float f[4] = { v, 0, 0, 0 }; glLightfv(n, p, f); }
void APIENTRY glLighti(GLenum n, GLenum p, GLint v) { float f[4] = { (float)v, 0, 0, 0 }; glLightfv(n, p, f); }
void APIENTRY glLightiv(GLenum n, GLenum p, const GLint *v)
{
	float f[4];
	int k, m = luz_n(p);
	for (k = 0; k < m; k++)
		f[k] = (p == GL_AMBIENT || p == GL_DIFFUSE || p == GL_SPECULAR) ? (float)v[k] / 2147483647.0f : (float)v[k];
	glLightfv(n, p, f);
}
void APIENTRY glGetLightfv(GLenum n, GLenum p, GLfloat *v)
{
	CTX();
	Luz *l;
	if (n < GL_LIGHT0 || n >= GL_LIGHT0 + LUCES) { hgl_error(C, GL_INVALID_ENUM); return; }
	l = &C->luz.l[n - GL_LIGHT0];
	switch (p) {
	case GL_AMBIENT: hgl_copia(v, l->amb, 16); break;
	case GL_DIFFUSE: hgl_copia(v, l->dif, 16); break;
	case GL_SPECULAR: hgl_copia(v, l->esp, 16); break;
	case GL_POSITION: hgl_copia(v, l->pos, 16); break;
	case GL_SPOT_DIRECTION: hgl_copia(v, l->dir_o, 12); break;
	case GL_SPOT_EXPONENT: v[0] = l->exp; break;
	case GL_SPOT_CUTOFF: v[0] = l->corte; break;
	case GL_CONSTANT_ATTENUATION: v[0] = l->att[0]; break;
	case GL_LINEAR_ATTENUATION: v[0] = l->att[1]; break;
	case GL_QUADRATIC_ATTENUATION: v[0] = l->att[2]; break;
	default: hgl_error(C, GL_INVALID_ENUM);
	}
}
void APIENTRY glGetLightiv(GLenum n, GLenum p, GLint *v)
{
	float f[4] = { 0, 0, 0, 0 };
	int k, m = luz_n(p);
	glGetLightfv(n, p, f);
	for (k = 0; k < m; k++) v[k] = hgl_red(f[k]);
}
static void i_modelo(u32 p, float a, float b, float c, float d)
{
	CTX();
	switch (p) {
	case GL_LIGHT_MODEL_AMBIENT: C->luz.amb[0] = a; C->luz.amb[1] = b; C->luz.amb[2] = c; C->luz.amb[3] = d; break;
	case GL_LIGHT_MODEL_LOCAL_VIEWER: C->luz.local = a != 0; break;
	case GL_LIGHT_MODEL_TWO_SIDE: C->luz.dos_caras = a != 0; break;
	default: break;
	}
}
void APIENTRY glLightModelfv(GLenum p, const GLfloat *v)
{
	CTX();
	float a[4] = { v[0], 0, 0, 0 };
	if (p == GL_LIGHT_MODEL_AMBIENT) { a[1] = v[1]; a[2] = v[2]; a[3] = v[3]; }
	LISTA(i_modelo, 5, p, F(a[0]), F(a[1]), F(a[2]), F(a[3]));
	i_modelo(p, a[0], a[1], a[2], a[3]);
}
void APIENTRY glLightModelf(GLenum p, GLfloat v) { float f[4] = { v, 0, 0, 0 }; glLightModelfv(p, f); }
void APIENTRY glLightModeli(GLenum p, GLint v) { float f[4] = { (float)v, 0, 0, 0 }; glLightModelfv(p, f); }
void APIENTRY glLightModeliv(GLenum p, const GLint *v)
{
	float f[4];
	int k;
	for (k = 0; k < (p == GL_LIGHT_MODEL_AMBIENT ? 4 : 1); k++)
		f[k] = p == GL_LIGHT_MODEL_AMBIENT ? (float)v[k] / 2147483647.0f : (float)v[k];
	glLightModelfv(p, f);
}
static void i_material(u32 cara, u32 p, float a, float b, float c, float d)
{
	CTX();
	int f;
	for (f = 0; f < 2; f++) {
		Material *m = &C->luz.m[f];
		if (f == 0 && cara == GL_BACK) continue;
		if (f == 1 && cara == GL_FRONT) continue;
		switch (p) {
		case GL_AMBIENT: m->amb[0] = a; m->amb[1] = b; m->amb[2] = c; m->amb[3] = d; break;
		case GL_DIFFUSE: m->dif[0] = a; m->dif[1] = b; m->dif[2] = c; m->dif[3] = d; break;
		case GL_SPECULAR: m->esp[0] = a; m->esp[1] = b; m->esp[2] = c; m->esp[3] = d; break;
		case GL_EMISSION: m->emi[0] = a; m->emi[1] = b; m->emi[2] = c; m->emi[3] = d; break;
		case GL_AMBIENT_AND_DIFFUSE:
			m->amb[0] = m->dif[0] = a; m->amb[1] = m->dif[1] = b; m->amb[2] = m->dif[2] = c; m->amb[3] = m->dif[3] = d;
			break;
		case GL_SHININESS: if (a < 0 || a > 128) { hgl_error(C, GL_INVALID_VALUE); return; } m->brillo = a; break;
		case GL_COLOR_INDEXES: break;
		default: hgl_error(C, GL_INVALID_ENUM); return;
		}
	}
}
void APIENTRY glMaterialfv(GLenum cara, GLenum p, const GLfloat *v)
{
	CTX();
	float a[4] = { v[0], 0, 0, 0 };
	if (p != GL_SHININESS) { a[1] = v[1]; a[2] = v[2]; if (p != GL_COLOR_INDEXES) a[3] = v[3]; }
	LISTA(i_material, 6, cara, p, F(a[0]), F(a[1]), F(a[2]), F(a[3]));
	i_material(cara, p, a[0], a[1], a[2], a[3]);
}
void APIENTRY glMaterialf(GLenum c, GLenum p, GLfloat v) { float f[4] = { v, 0, 0, 0 }; glMaterialfv(c, p, f); }
void APIENTRY glMateriali(GLenum c, GLenum p, GLint v) { float f[4] = { (float)v, 0, 0, 0 }; glMaterialfv(c, p, f); }
void APIENTRY glMaterialiv(GLenum c, GLenum p, const GLint *v)
{
	float f[4];
	int k;
	for (k = 0; k < (p == GL_SHININESS ? 1 : 4); k++) f[k] = p == GL_SHININESS ? (float)v[k] : (float)v[k] / 2147483647.0f;
	glMaterialfv(c, p, f);
}
void APIENTRY glGetMaterialfv(GLenum cara, GLenum p, GLfloat *v)
{
	CTX();
	Material *m = &C->luz.m[cara == GL_BACK];
	switch (p) {
	case GL_AMBIENT: hgl_copia(v, m->amb, 16); break;
	case GL_DIFFUSE: hgl_copia(v, m->dif, 16); break;
	case GL_SPECULAR: hgl_copia(v, m->esp, 16); break;
	case GL_EMISSION: hgl_copia(v, m->emi, 16); break;
	case GL_SHININESS: v[0] = m->brillo; break;
	default: hgl_error(C, GL_INVALID_ENUM);
	}
}
void APIENTRY glGetMaterialiv(GLenum c, GLenum p, GLint *v)
{
	float f[4] = { 0, 0, 0, 0 };
	int k;
	glGetMaterialfv(c, p, f);
	for (k = 0; k < (p == GL_SHININESS ? 1 : 4); k++) v[k] = hgl_red(f[k]);
}
static void i_colmat(u32 cara, u32 modo)
{
	CTX();
	C->luz.cm_cara = cara; C->luz.cm_modo = modo;
	if (C->hab.colmat) i_color(C->cur.col[0], C->cur.col[1], C->cur.col[2], C->cur.col[3]);
}
void APIENTRY glColorMaterial(GLenum c, GLenum m) { CTX(); LISTA(i_colmat, 2, c, m); i_colmat(c, m); }

/* ---- las matrices de vértices ---- */
static float leer(const u8 *p, int tipo, int k, int norm)
{
	switch (tipo) {
	case GL_FLOAT: return ((const float *)p)[k];
	case GL_DOUBLE: return (float)((const double *)p)[k];
	case GL_UNSIGNED_BYTE: return norm ? UB(p[k]) : p[k];
	case GL_BYTE: return norm ? B_(((const i8 *)p)[k]) : ((const i8 *)p)[k];
	case GL_UNSIGNED_SHORT: return norm ? US(((const u16 *)p)[k]) : ((const u16 *)p)[k];
	case GL_SHORT: return norm ? S_(((const i16 *)p)[k]) : ((const i16 *)p)[k];
	case GL_UNSIGNED_INT: return norm ? UI(((const u32 *)p)[k]) : (float)((const u32 *)p)[k];
	case GL_INT: return norm ? I_(((const i32 *)p)[k]) : (float)((const i32 *)p)[k];
	}
	return 0;
}
static int tam_tipo(int t)
{
	switch (t) {
	case GL_BYTE: case GL_UNSIGNED_BYTE: return 1;
	case GL_SHORT: case GL_UNSIGNED_SHORT: return 2;
	case GL_DOUBLE: return 8;
	default: return 4;
	}
}
static const u8 *dir_de(const Arr *a, int i, int tam)
{
	int paso = a->paso ? a->paso : tam * tam_tipo(a->tipo);
	return a->p + (u32)i * (u32)paso;
}
/* un elemento: con «graba», por las de fuera (para que entre en la lista) */
void hgl_elemento(Ctx *C, int i, int graba)
{
	int u;
	if (C->arr.b.si && C->arr.b.p) {
		const u8 *p = dir_de(&C->arr.b, i, 1);
		if (graba) glEdgeFlag(*p); else i_borde(*p);
	}
	for (u = 0; u < UNIDADES; u++) {
		const Arr *a = &C->arr.t[u];
		if (a->si && a->p) {
			const u8 *p = dir_de(a, i, a->tam);
			float s = leer(p, a->tipo, 0, 0), t = a->tam > 1 ? leer(p, a->tipo, 1, 0) : 0;
			float r = a->tam > 2 ? leer(p, a->tipo, 2, 0) : 0, q = a->tam > 3 ? leer(p, a->tipo, 3, 0) : 1;
			if (graba) glMultiTexCoord4fARB(GL_TEXTURE0_ARB + u, s, t, r, q); else i_texc(u, s, t, r, q);
		}
	}
	if (C->arr.c.si && C->arr.c.p) {
		const Arr *a = &C->arr.c;
		const u8 *p = dir_de(a, i, a->tam);
		float r = leer(p, a->tipo, 0, 1), g = leer(p, a->tipo, 1, 1), b = leer(p, a->tipo, 2, 1);
		float al = a->tam > 3 ? leer(p, a->tipo, 3, 1) : 1;
		if (graba) glColor4f(r, g, b, al); else i_color(r, g, b, al);
	}
	if (C->arr.n.si && C->arr.n.p) {
		const Arr *a = &C->arr.n;
		const u8 *p = dir_de(a, i, 3);
		float x = leer(p, a->tipo, 0, 1), y = leer(p, a->tipo, 1, 1), z = leer(p, a->tipo, 2, 1);
		if (graba) glNormal3f(x, y, z); else i_normal(x, y, z);
	}
	if (C->arr.v.si && C->arr.v.p) {
		const Arr *a = &C->arr.v;
		const u8 *p = dir_de(a, i, a->tam);
		float x = leer(p, a->tipo, 0, 0), y = leer(p, a->tipo, 1, 0);
		float z = a->tam > 2 ? leer(p, a->tipo, 2, 0) : 0, w = a->tam > 3 ? leer(p, a->tipo, 3, 0) : 1;
		if (graba) glVertex4f(x, y, z, w); else i_vertice(x, y, z, w);
	}
}
void APIENTRY glArrayElement(GLint i) { CTX(); hgl_elemento(C, i, C->lista_n != 0); }
void APIENTRY glDrawArrays(GLenum m, GLint ini, GLsizei n)
{
	CTX();
	int k, g = C->lista_n != 0;
	if (n < 0) { hgl_error(C, GL_INVALID_VALUE); return; }
	if (C->prim >= 0) { hgl_error(C, GL_INVALID_OPERATION); return; }
	if (g) glBegin(m); else i_empieza(m);
	for (k = 0; k < n; k++) hgl_elemento(C, ini + k, g);
	if (g) glEnd(); else i_acaba();
}
void APIENTRY glDrawElements(GLenum m, GLsizei n, GLenum tipo, const GLvoid *ind)
{
	CTX();
	int k, g = C->lista_n != 0;
	if (n < 0) { hgl_error(C, GL_INVALID_VALUE); return; }
	if (C->prim >= 0) { hgl_error(C, GL_INVALID_OPERATION); return; }
	if (tipo != GL_UNSIGNED_BYTE && tipo != GL_UNSIGNED_SHORT && tipo != GL_UNSIGNED_INT) { hgl_error(C, GL_INVALID_ENUM); return; }
	if (g) glBegin(m); else i_empieza(m);
	for (k = 0; k < n; k++) {
		int i = tipo == GL_UNSIGNED_BYTE ? ((const u8 *)ind)[k] : tipo == GL_UNSIGNED_SHORT ? ((const u16 *)ind)[k] : (int)((const u32 *)ind)[k];
		hgl_elemento(C, i, g);
	}
	if (g) glEnd(); else i_acaba();
}
static void pon_arr(Ctx *C, Arr *a, int tam, int tipo, int paso, const void *p)
{
	if (paso < 0) { hgl_error(C, GL_INVALID_VALUE); return; }
	a->tam = tam; a->tipo = tipo; a->paso = paso; a->p = p;
}
void APIENTRY glVertexPointer(GLint t, GLenum ti, GLsizei pa, const GLvoid *p) { CTX(); if (t < 2 || t > 4) { hgl_error(C, GL_INVALID_VALUE); return; } pon_arr(C, &C->arr.v, t, ti, pa, p); }
void APIENTRY glNormalPointer(GLenum ti, GLsizei pa, const GLvoid *p) { CTX(); pon_arr(C, &C->arr.n, 3, ti, pa, p); }
void APIENTRY glColorPointer(GLint t, GLenum ti, GLsizei pa, const GLvoid *p) { CTX(); if (t < 3 || t > 4) { hgl_error(C, GL_INVALID_VALUE); return; } pon_arr(C, &C->arr.c, t, ti, pa, p); }
void APIENTRY glTexCoordPointer(GLint t, GLenum ti, GLsizei pa, const GLvoid *p) { CTX(); if (t < 1 || t > 4) { hgl_error(C, GL_INVALID_VALUE); return; } pon_arr(C, &C->arr.t[C->arr.tex_cli], t, ti, pa, p); }
void APIENTRY glEdgeFlagPointer(GLsizei pa, const GLvoid *p) { CTX(); pon_arr(C, &C->arr.b, 1, GL_UNSIGNED_BYTE, pa, p); }
void APIENTRY glIndexPointer(GLenum ti, GLsizei pa, const GLvoid *p) { CTX(); pon_arr(C, &C->arr.i, 1, ti, pa, p); }
void APIENTRY glLockArraysEXT(GLint ini, GLsizei n) { CTX(); C->bloqueados_ini = ini; C->bloqueados_n = n; }
void APIENTRY glUnlockArraysEXT(void) { CTX(); C->bloqueados_ini = C->bloqueados_n = 0; }
void APIENTRY glInterleavedArrays(GLenum f, GLsizei paso, const GLvoid *p)
{
	CTX();
	const u8 *b = p;
	int tc = 0, cc = 0, nc = 0, vc = 3, ctipo = GL_FLOAT, tam, oc = 0, on = 0, ov = 0;
	switch (f) {
	case GL_V2F: vc = 2; break;
	case GL_V3F: break;
	case GL_C4UB_V2F: cc = 4; ctipo = GL_UNSIGNED_BYTE; vc = 2; break;
	case GL_C4UB_V3F: cc = 4; ctipo = GL_UNSIGNED_BYTE; break;
	case GL_C3F_V3F: cc = 3; break;
	case GL_N3F_V3F: nc = 3; break;
	case GL_C4F_N3F_V3F: cc = 4; nc = 3; break;
	case GL_T2F_V3F: tc = 2; break;
	case GL_T4F_V4F: tc = 4; vc = 4; break;
	case GL_T2F_C4UB_V3F: tc = 2; cc = 4; ctipo = GL_UNSIGNED_BYTE; break;
	case GL_T2F_C3F_V3F: tc = 2; cc = 3; break;
	case GL_T2F_N3F_V3F: tc = 2; nc = 3; break;
	case GL_T2F_C4F_N3F_V3F: tc = 2; cc = 4; nc = 3; break;
	case GL_T4F_C4F_N3F_V4F: tc = 4; cc = 4; nc = 3; vc = 4; break;
	default: hgl_error(C, GL_INVALID_ENUM); return;
	}
	oc = tc * 4;
	on = oc + (cc ? (ctipo == GL_UNSIGNED_BYTE ? 4 : cc * 4) : 0);
	ov = on + nc * 4;
	tam = ov + vc * 4;
	if (!paso) paso = tam;
	C->arr.t[C->arr.tex_cli].si = tc != 0;
	if (tc) pon_arr(C, &C->arr.t[C->arr.tex_cli], tc, GL_FLOAT, paso, b);
	C->arr.c.si = cc != 0;
	if (cc) pon_arr(C, &C->arr.c, cc, ctipo, paso, b + oc);
	C->arr.n.si = nc != 0;
	if (nc) pon_arr(C, &C->arr.n, 3, GL_FLOAT, paso, b + on);
	C->arr.v.si = 1;
	pon_arr(C, &C->arr.v, vc, GL_FLOAT, paso, b + ov);
	C->arr.b.si = C->arr.i.si = 0;
}
