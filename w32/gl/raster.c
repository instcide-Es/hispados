/* raster.c - HispaGL: el rasterizado por programa. Triángulos por filas con
 * la regla de arriba a la izquierda (el centro del punto), los atributos por
 * sus gradientes (el plano de cada uno: se calcula al principio de cada fila
 * y se avanza en coma fija), las coordenadas de textura con corrección de
 * perspectiva cada 16 puntos (una división por tramo, como los motores por
 * programa de la época) y el nivel de mipmap por tramo. Tres caminos:
 *  - tramo_color: sin textura, sin mezcla ni niebla (Gouraud o plano);
 *  - tramo_tex: una textura 5-6-5, punto más cercano, GL_REPEAT, MODULATE o
 *    REPLACE, sin mezcla, prueba de alfa ni niebla (lo más frecuente en un
 *    juego: las paredes y los modelos);
 *  - tramo_todo: lo demás, punto a punto con todo lo de OpenGL 1.1: dos
 *    unidades de textura con sus modos, filtro bilineal y trilineal, niebla,
 *    prueba de alfa, profundidad con cualquier función, mezcla con todos los
 *    factores, máscara de color y punteado.
 * Líneas y puntos van por hgl_fragmento (punto a punto). El búfer de color es
 * de 5-6-5 y el de profundidad de 16 bits; la fila 0 es la de arriba.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "hgl.h"

#define SEG 16

int hgl_calidad = 1;   /* 0 rápida (todo al punto más cercano), 1 normal (bilineal, sin trilineal), 2 exacta */

/* ---- la caja donde se puede pintar (búfer ∩ vista ∩ tijera) ---- */
typedef struct { int x0, y0, x1, y1; } Caja;
static void caja(Ctx *C, Caja *k)
{
	int x0 = C->vis.x, x1 = C->vis.x + C->vis.w, y0 = C->al - (C->vis.y + C->vis.h), y1 = C->al - C->vis.y;
	if (C->hab.tijera) {
		int a = C->tij.x, b = C->tij.x + C->tij.w, c = C->al - (C->tij.y + C->tij.h), d = C->al - C->tij.y;
		if (a > x0) x0 = a;
		if (b < x1) x1 = b;
		if (c > y0) y0 = c;
		if (d < y1) y1 = d;
	}
	if (x0 < 0) x0 = 0;
	if (y0 < 0) y0 = 0;
	if (x1 > C->an) x1 = C->an;
	if (y1 > C->al) y1 = C->al;
	k->x0 = x0; k->y0 = y0; k->x1 = x1; k->y1 = y1;
}

/* ---- las texturas: leer un punto ---- */
static inline u32 deco(u16 p, int fmt)   /* → 0xAARRGGBB */
{
	u32 r, g, b, a;
	switch (fmt) {
	case TF_565:
		r = p >> 11; g = (p >> 5) & 63; b = p & 31;
		return 0xFF000000u | (((r << 3) | (r >> 2)) << 16) | (((g << 2) | (g >> 4)) << 8) | ((b << 3) | (b >> 2));
	case TF_1555:
		r = (p >> 10) & 31; g = (p >> 5) & 31; b = p & 31;
		return (p & 0x8000 ? 0xFF000000u : 0) | (((r << 3) | (r >> 2)) << 16) | (((g << 3) | (g >> 2)) << 8) | ((b << 3) | (b >> 2));
	default:
		a = p >> 12; r = (p >> 8) & 15; g = (p >> 4) & 15; b = p & 15;
		return ((a * 17) << 24) | ((r * 17) << 16) | ((g * 17) << 8) | (b * 17);
	}
}
typedef struct {
	const Tex *t;
	int repite_s, repite_t;          /* GL_REPEAT o no (los demás: hasta el borde) */
	int fmag, fmin;                  /* 0 punto, 1 bilineal */
	int mip;                         /* 0 sin mipmaps, 1 nivel más cercano, 2 entre dos niveles */
	int modo;                        /* M_* */
	int env[4];                      /* el color de glTexEnv, 0..255 */
	float W, H;
} UT;
enum { M_MOD, M_REP_RGB, M_REP_RGBA, M_REP_A, M_DECAL, M_BLEND, M_BLEND_I, M_ADD, M_ADD_I };

static inline u32 lee(const Nivel *n, int fmt, int x, int y, int rs, int rt)
{
	if (rs) x &= n->w - 1; else x = x < 0 ? 0 : x >= n->w ? n->w - 1 : x;
	if (rt) y &= n->h - 1; else y = y < 0 ? 0 : y >= n->h ? n->h - 1 : y;
	return deco(n->p[(y << n->lw) + x], fmt);
}
static inline u32 mezcla4(u32 a, u32 b, u32 c, u32 d, int fx, int fy)  /* fx, fy: 0..256 */
{
	u32 r = 0;
	int k;
	for (k = 0; k < 32; k += 8) {
		int va = (a >> k) & 255, vb = (b >> k) & 255, vc = (c >> k) & 255, vd = (d >> k) & 255;
		int ab = va * 256 + (vb - va) * fx, cd = vc * 256 + (vd - vc) * fx;
		int v = (ab * 256 + (cd - ab) * fy) >> 16;
		r |= (u32)v << k;
	}
	return r;
}
/* s, t: en puntos del nivel 0, 16.16; nivel: el de mipmap; fil: 0 punto, 1 bilineal */
static inline u32 muestra(const UT *u, i32 s, i32 t, int nivel, int fil)
{
	const Nivel *n = &u->t->niv[nivel];
	int fmt = u->t->fmt;
	if (nivel) { s >>= nivel; t >>= nivel; }
	if (!fil) return lee(n, fmt, s >> 16, t >> 16, u->repite_s, u->repite_t);
	s -= 0x8000; t -= 0x8000;
	{
		int x = s >> 16, y = t >> 16, fx = (s >> 8) & 255, fy = (t >> 8) & 255;
		u32 a = lee(n, fmt, x, y, u->repite_s, u->repite_t), b = lee(n, fmt, x + 1, y, u->repite_s, u->repite_t);
		u32 c = lee(n, fmt, x, y + 1, u->repite_s, u->repite_t), d = lee(n, fmt, x + 1, y + 1, u->repite_s, u->repite_t);
		return mezcla4(a, b, c, d, fx, fy);
	}
}
/* el nivel (y la fracción, 0..256, para el trilineal) de rho² en puntos² */
static inline int nivel_de(float rho2, int nniv, int *frac)
{
	union { float f; u32 u; } x;
	int e, l;
	x.f = rho2;
	e = (int)((x.u >> 23) & 255) - 127;      /* suelo de log2(rho²) */
	if (frac) {
		/* lambda = log2(rho²)/2 ≈ (e + m)/2; el nivel de abajo y la fracción */
		int m = (int)((x.u >> 15) & 255);     /* la mantisa, 0..255 ≈ log2(1+m) */
		int lam = (e * 256 + m) / 2;          /* lambda en 1/256 */
		if (lam <= 0) { *frac = 0; return 0; }
		l = lam >> 8;
		*frac = lam & 255;
		if (l >= nniv - 1) { *frac = 0; return nniv - 1; }
		return l;
	}
	l = (e + 1) >> 1;
	if (l < 0) l = 0;
	if (l >= nniv) l = nniv - 1;
	return l;
}

static inline int mul8(int a, int b) { return (a * (b + 1)) >> 8; }
static inline int div255(int x) { return (x + 1 + (x >> 8)) >> 8; }
static inline int lim8(int v) { return v < 0 ? 0 : v > 255 ? 255 : v; }

/* aplica el modo de la unidad: f = el fragmento, t = el punto de la textura (0xAARRGGBB) */
static inline void entorno(const UT *u, int *c, u32 t)
{
	int tr = (t >> 16) & 255, tg = (t >> 8) & 255, tb = t & 255, ta = t >> 24;
	switch (u->modo) {
	case M_MOD: c[0] = mul8(c[0], tr); c[1] = mul8(c[1], tg); c[2] = mul8(c[2], tb); c[3] = mul8(c[3], ta); break;
	case M_REP_RGB: c[0] = tr; c[1] = tg; c[2] = tb; break;
	case M_REP_RGBA: c[0] = tr; c[1] = tg; c[2] = tb; c[3] = ta; break;
	case M_REP_A: c[3] = ta; break;
	case M_DECAL:
		c[0] += ((tr - c[0]) * (ta + (ta >> 7))) >> 8;
		c[1] += ((tg - c[1]) * (ta + (ta >> 7))) >> 8;
		c[2] += ((tb - c[2]) * (ta + (ta >> 7))) >> 8;
		break;
	case M_BLEND: case M_BLEND_I:
		c[0] = div255(c[0] * (255 - tr) + u->env[0] * tr);
		c[1] = div255(c[1] * (255 - tg) + u->env[1] * tg);
		c[2] = div255(c[2] * (255 - tb) + u->env[2] * tb);
		if (u->modo == M_BLEND_I) c[3] = div255(c[3] * (255 - ta) + u->env[3] * ta);
		else c[3] = mul8(c[3], ta);
		break;
	case M_ADD: c[0] = lim8(c[0] + tr); c[1] = lim8(c[1] + tg); c[2] = lim8(c[2] + tb); c[3] = mul8(c[3], ta); break;
	case M_ADD_I: c[0] = lim8(c[0] + tr); c[1] = lim8(c[1] + tg); c[2] = lim8(c[2] + tb); c[3] = lim8(c[3] + ta); break;
	}
}
static int modo_de(int env, int base)
{
	switch (env) {
	case GL_REPLACE:
		if (base == GL_RGB || base == GL_LUMINANCE) return M_REP_RGB;
		if (base == GL_ALPHA) return M_REP_A;
		return M_REP_RGBA;
	case GL_DECAL:
		if (base == GL_RGB || base == GL_LUMINANCE) return M_REP_RGB;
		if (base == GL_RGBA || base == GL_LUMINANCE_ALPHA) return M_DECAL;
		return M_MOD;
	case GL_BLEND: return base == GL_INTENSITY ? M_BLEND_I : base == GL_ALPHA ? M_MOD : M_BLEND;
	case GL_ADD: return base == GL_INTENSITY ? M_ADD_I : base == GL_ALPHA ? M_MOD : M_ADD;
	default: return M_MOD;
	}
}

/* ---- lo que se prepara por triángulo ---- */
typedef struct {
	Ctx *C;
	int nu;                          /* unidades con textura (0, 1 o 2) */
	UT u[UNIDADES];
	int ui[UNIDADES];                /* de qué unidad de OpenGL es cada una */
	/* los gradientes: d/dx, d/dy y la base (el valor en el punto 0,0) */
	float zx, zy, z0;
	float cx[4], cy[4], c0[4];       /* r, g, b, a: 0..255 */
	float fx, fy, f0;                /* niebla 0..255 */
	float sx[UNIDADES], sy[UNIDADES], s0[UNIDADES];   /* s/w, t/w, q/w */
	float tx[UNIDADES], ty[UNIDADES], t0[UNIDADES];
	float qx[UNIDADES], qy[UNIDADES], q0[UNIDADES];
	int liso;                        /* el color es el mismo en todo el triángulo */
	int col_k[4];                    /* (liso) el color, 0..255 */
	int persp;                       /* hay que dividir por w (texturas) */
	float wx, wy, w0;                /* el plano de 1/w */
	int pcol;                        /* el color y la niebla, con perspectiva (sus planos son c/w) */
	int seg;
	/* lo de los fragmentos */
	int prof, prof_f, escribe_z, zk;
	int alfa_f, alfa_ref;
	int mezcla, m_or, m_de, mtipo;
	int niebla, nie_c[3];
	u16 masc;
	int punteo;
	int gris;
} Tri;

enum { MZ_NADA, MZ_ALFA, MZ_SUMA, MZ_MULT, MZ_ALFA_SUMA, MZ_OTRA };

static Tri T;

static inline int pasa_z(int f, u32 z, u32 zb)
{
	switch (f) {
	case GL_NEVER: return 0;
	case GL_LESS: return z < zb;
	case GL_EQUAL: return z == zb;
	case GL_LEQUAL: return z <= zb;
	case GL_GREATER: return z > zb;
	case GL_NOTEQUAL: return z != zb;
	case GL_GEQUAL: return z >= zb;
	default: return 1;
	}
}
static inline int pasa_a(int f, int a, int r)
{
	switch (f) {
	case GL_NEVER: return 0;
	case GL_LESS: return a < r;
	case GL_EQUAL: return a == r;
	case GL_LEQUAL: return a <= r;
	case GL_GREATER: return a > r;
	case GL_NOTEQUAL: return a != r;
	case GL_GEQUAL: return a >= r;
	default: return 1;
	}
}
static inline int factor(int f, int c, int ca, int sc, int sa, int dc, int k)
{
	(void)k;
	switch (f) {
	case GL_ZERO: return 0;
	case GL_ONE: return 255;
	case GL_SRC_COLOR: return sc;
	case GL_ONE_MINUS_SRC_COLOR: return 255 - sc;
	case GL_DST_COLOR: return dc;
	case GL_ONE_MINUS_DST_COLOR: return 255 - dc;
	case GL_SRC_ALPHA: return sa;
	case GL_ONE_MINUS_SRC_ALPHA: return 255 - sa;
	case GL_DST_ALPHA: return 255;
	case GL_ONE_MINUS_DST_ALPHA: return 0;
	case GL_SRC_ALPHA_SATURATE: return ca ? 255 : 0;
	}
	(void)c;
	return 255;
}
/* escribe el color c (0..255) en el punto p, con la mezcla de T */
static inline void escribe(u16 *p, const int *c)
{
	int r = c[0], g = c[1], b = c[2];
	if (T.mtipo != MZ_NADA) {
		u16 d = *p;
		int dr = ((d >> 8) & 0xF8) | (d >> 13), dg = ((d >> 3) & 0xFC) | ((d >> 9) & 3), db = ((d << 3) & 0xF8) | ((d >> 2) & 7);
		int a = c[3];
		switch (T.mtipo) {
		case MZ_ALFA: r = dr + mul8(r - dr, a); g = dg + mul8(g - dg, a); b = db + mul8(b - db, a); break;
		case MZ_SUMA: r = lim8(r + dr); g = lim8(g + dg); b = lim8(b + db); break;
		case MZ_MULT: r = mul8(dr, r); g = mul8(dg, g); b = mul8(db, b); break;
		case MZ_ALFA_SUMA: r = lim8(dr + mul8(r, a)); g = lim8(dg + mul8(g, a)); b = lim8(db + mul8(b, a)); break;
		default: {
			int s0 = factor(T.m_or, 0, 0, r, a, dr, 0), s1 = factor(T.m_or, 0, 0, g, a, dg, 1), s2 = factor(T.m_or, 0, 0, b, a, db, 2);
			int d0 = factor(T.m_de, 0, 0, r, a, dr, 0), d1 = factor(T.m_de, 0, 0, g, a, dg, 1), d2 = factor(T.m_de, 0, 0, b, a, db, 2);
			if (T.m_or == GL_SRC_ALPHA_SATURATE) s0 = s1 = s2 = 0;   /* min(As, 1 − Ad) con Ad = 1 */
			r = lim8(div255(r * s0 + dr * d0));
			g = lim8(div255(g * s1 + dg * d1));
			b = lim8(div255(b * s2 + db * d2));
		}
		}
	}
	{
		u16 v = hgl_565(r, g, b);
		if (T.masc != 0xFFFF) v = (u16)((v & T.masc) | (*p & ~T.masc));
		*p = v;
	}
}

/* el color y la niebla de un segmento de m puntos que empieza en (fx, fy):
 * el valor del principio y lo que avanza por punto, en 8.16 */
typedef struct { i32 c[4], dc[4], f, df; } Seg;
static inline i32 lim16(float v) { return v <= 0 ? 0 : v >= 255.0f ? 255 << 16 : hgl_red(v * 65536.0f); }
static void seg_color(Seg *g, float fx, float fy, int m)
{
	int k;
	if (T.liso) {
		for (k = 0; k < 4; k++) { g->c[k] = T.col_k[k] << 16; g->dc[k] = 0; }
	} else if (!T.pcol) {
		for (k = 0; k < 4; k++) {
			float a = T.c0[k] + T.cx[k] * fx + T.cy[k] * fy;
			g->c[k] = lim16(a);
			g->dc[k] = (lim16(a + T.cx[k] * m) - g->c[k]) / m;
		}
	} else {
		float W0 = T.w0 + T.wx * fx + T.wy * fy, W1 = W0 + T.wx * m, i0, i1;
		if (W1 <= 0) W1 = W0;
		i0 = 1.0f / W0; i1 = 1.0f / W1;
		for (k = 0; k < 4; k++) {
			float a = T.c0[k] + T.cx[k] * fx + T.cy[k] * fy;
			g->c[k] = lim16(a * i0);
			g->dc[k] = (lim16((a + T.cx[k] * m) * i1) - g->c[k]) / m;
		}
	}
	if (T.niebla) {
		float a = T.f0 + T.fx * fx + T.fy * fy;
		if (T.pcol) {
			float W0 = T.w0 + T.wx * fx + T.wy * fy, W1 = W0 + T.wx * m;
			if (W1 <= 0) W1 = W0;
			g->f = lim16(a / W0);
			g->df = (lim16((a + T.fx * m) / W1) - g->f) / m;
		} else {
			g->f = lim16(a);
			g->df = (lim16(a + T.fx * m) - g->f) / m;
		}
	}
}

/* ---- el camino general: punto a punto ---- */
static void tramo_todo(u16 *pc, u16 *pz, int x, int y, int n, float fx, float fy)
{
	Ctx *C = T.C;
	u32 z = (u32)hgl_red((T.z0 + T.zx * fx + T.zy * fy) * 32768.0f), dz = (u32)hgl_red(T.zx * 32768.0f);
	float S[UNIDADES], Tt[UNIDADES], Q[UNIDADES];
	int k, u;
	for (u = 0; u < T.nu; u++) {
		S[u] = T.s0[u] + T.sx[u] * fx + T.sy[u] * fy;
		Tt[u] = T.t0[u] + T.tx[u] * fx + T.ty[u] * fy;
		Q[u] = T.q0[u] + T.qx[u] * fx + T.qy[u] * fy;
	}
	while (n > 0) {
		int m = n < T.seg ? n : T.seg, i;
		i32 s[UNIDADES], t[UNIDADES], ds[UNIDADES], dt[UNIDADES];
		int nv[UNIDADES], nv2[UNIDADES], fr[UNIDADES], fil[UNIDADES];
		Seg g;
		seg_color(&g, fx, fy, m);
		fx += m;
		for (u = 0; u < T.nu; u++) {
			const UT *U = &T.u[u];
			float q0 = 1.0f / Q[u], s0 = S[u] * q0 * U->W, t0 = Tt[u] * q0 * U->H, s1, t1;
			float S1 = S[u] + T.sx[u] * m, T1 = Tt[u] + T.tx[u] * m, Q1 = Q[u] + T.qx[u] * m, q1;
			float off_s = 0, off_t = 0;
			if (Q1 <= 0) Q1 = Q[u];
			q1 = 1.0f / Q1;
			s1 = S1 * q1 * U->W; t1 = T1 * q1 * U->H;
			/* para que la coma fija no se salga: se quita un múltiplo del tamaño */
			if (U->repite_s && (s0 > 16384 || s0 < -16384)) off_s = (float)hgl_suelo(s0 / U->W) * U->W;
			if (U->repite_t && (t0 > 16384 || t0 < -16384)) off_t = (float)hgl_suelo(t0 / U->H) * U->H;
			if (!U->repite_s) { if (s0 < -16384) s0 = -16384; if (s0 > 16384) s0 = 16384; if (s1 < -16384) s1 = -16384; if (s1 > 16384) s1 = 16384; }
			if (!U->repite_t) { if (t0 < -16384) t0 = -16384; if (t0 > 16384) t0 = 16384; if (t1 < -16384) t1 = -16384; if (t1 > 16384) t1 = 16384; }
			s[u] = hgl_red((s0 - off_s) * 65536.0f);
			t[u] = hgl_red((t0 - off_t) * 65536.0f);
			ds[u] = hgl_red((s1 - s0) * 65536.0f / m);
			dt[u] = hgl_red((t1 - t0) * 65536.0f / m);
			nv[u] = 0; nv2[u] = 0; fr[u] = 0;
			fil[u] = U->fmag;
			if (U->mip || U->fmin != U->fmag) {
				/* rho²: lo que avanza la textura por punto, en x y en y */
				float dsx = (s1 - s0) / m, dtx = (t1 - t0) / m;
				float dsy = (T.sy[u] - S[u] * q0 * T.qy[u]) * q0 * U->W, dty = (T.ty[u] - Tt[u] * q0 * T.qy[u]) * q0 * U->H;
				float rx = dsx * dsx + dtx * dtx, ry = dsy * dsy + dty * dty, r2 = rx > ry ? rx : ry;
				if (r2 > 1.0f) {
					fil[u] = U->fmin;
					if (U->mip) {
						nv[u] = nivel_de(r2, U->t->nniv, U->mip == 2 ? &fr[u] : NULL);
						nv2[u] = nv[u] + 1 < U->t->nniv ? nv[u] + 1 : nv[u];
					}
				}
			}
			S[u] = S1; Tt[u] = T1; Q[u] = Q1;
		}
		for (i = 0; i < m; i++, x++, pc++, pz++) {
			u32 zz = z >> 15;
			int col[4];
			if (T.prof && !pasa_z(T.prof_f, zz, *pz)) goto sig;
			if (T.punteo) {
				int gy = (C->al - 1 - y) & 31, gx = x & 31;
				if (!(C->punteo[gy * 4 + (gx >> 3)] & (0x80 >> (gx & 7)))) goto sig;
			}
			col[0] = g.c[0] >> 16; col[1] = g.c[1] >> 16; col[2] = g.c[2] >> 16; col[3] = g.c[3] >> 16;
			for (k = 0; k < 4; k++) col[k] = col[k] < 0 ? 0 : col[k] > 255 ? 255 : col[k];
			for (u = 0; u < T.nu; u++) {
				u32 tx = muestra(&T.u[u], s[u], t[u], nv[u], fil[u]);
				if (fr[u]) {
					u32 t2 = muestra(&T.u[u], s[u], t[u], nv2[u], fil[u]);
					tx = mezcla4(tx, t2, tx, t2, fr[u], 0);
				}
				entorno(&T.u[u], col, tx);
			}
			if (T.niebla) {
				int ff = g.f >> 16;
				ff = ff < 0 ? 0 : ff > 255 ? 255 : ff;
				col[0] = div255(col[0] * ff + T.nie_c[0] * (255 - ff));
				col[1] = div255(col[1] * ff + T.nie_c[1] * (255 - ff));
				col[2] = div255(col[2] * ff + T.nie_c[2] * (255 - ff));
			}
			if (T.alfa_f != GL_ALWAYS && !pasa_a(T.alfa_f, col[3], T.alfa_ref)) goto sig;
			if (T.escribe_z) *pz = (u16)zz;
			escribe(pc, col);
			C->n_pix++;
		sig:
			z += dz;
			for (k = 0; k < 4; k++) g.c[k] += g.dc[k];
			g.f += g.df;
			for (u = 0; u < T.nu; u++) { s[u] += ds[u]; t[u] += dt[u]; }
		}
		n -= m;
	}
}

/* ---- sin textura, sin mezcla ni niebla ---- */
static void tramo_color(u16 *pc, u16 *pz, int x, int y, int n, float fx, float fy)
{
	u32 z = (u32)hgl_red((T.z0 + T.zx * fx + T.zy * fy) * 32768.0f), dz = (u32)hgl_red(T.zx * 32768.0f);
	int zk = T.zk, ez = T.escribe_z;
	(void)x; (void)y;
	T.C->n_pix += n;
	if (T.liso) {
		u16 v = hgl_565(T.col_k[0], T.col_k[1], T.col_k[2]);
		if (zk < -65536 && !ez) { while (n--) *pc++ = v; return; }
		while (n--) {
			int zz = (int)(z >> 15);
			if (zz <= (int)*pz - zk) { *pc = v; if (ez) *pz = (u16)zz; }
			pc++; pz++; z += dz;
		}
		return;
	}
	while (n > 0) {
		int m = n < T.seg ? n : T.seg;
		Seg g;
		i32 r, gg, b, dr, dg, db;
		seg_color(&g, fx, fy, m);
		fx += m;
		n -= m;
		r = g.c[0]; gg = g.c[1]; b = g.c[2]; dr = g.dc[0]; dg = g.dc[1]; db = g.dc[2];
		while (m--) {
			int zz = (int)(z >> 15);
			if (zz <= (int)*pz - zk) {
				*pc = (u16)(((r >> 8) & 0xF800) | ((gg >> 13) & 0x07E0) | ((u32)b >> 19));
				if (ez) *pz = (u16)zz;
			}
			pc++; pz++; z += dz; r += dr; gg += dg; b += db;
		}
	}
}

/* ---- una textura 5-6-5, punto más cercano, GL_REPEAT ---- */
static void tramo_tex(u16 *pc, u16 *pz, int x, int y, int n, float fx, float fy)
{
	const UT *U = &T.u[0];
	u32 z = (u32)hgl_red((T.z0 + T.zx * fx + T.zy * fy) * 32768.0f), dz = (u32)hgl_red(T.zx * 32768.0f);
	float S = T.s0[0] + T.sx[0] * fx + T.sy[0] * fy, Tt = T.t0[0] + T.tx[0] * fx + T.ty[0] * fy, Q = T.q0[0] + T.qx[0] * fx + T.qy[0] * fy;
	float q0 = 1.0f / Q;
	int zk = T.zk, ez = T.escribe_z, blanco = T.liso && T.col_k[0] == 255 && T.col_k[1] == 255 && T.col_k[2] == 255;
	int reemplaza = U->modo == M_REP_RGB || U->modo == M_REP_RGBA || blanco;
	(void)x; (void)y;
	T.C->n_pix += n;
	while (n > 0) {
		int m = n < T.seg ? n : T.seg, nivel = 0, sh, wm, hm, lw;
		float S1 = S + T.sx[0] * m, T1 = Tt + T.tx[0] * m, Q1 = Q + T.qx[0] * m, q1, s0, t0, s1, t1, os, ot;
		const Nivel *nv;
		const u16 *tex;
		i32 s, t, ds, dt, r = 0, g = 0, b = 0, dr = 0, dg = 0, db = 0;
		if (Q1 <= 0) Q1 = Q;
		q1 = 1.0f / Q1;
		s0 = S * q0 * U->W; t0 = Tt * q0 * U->H;
		s1 = S1 * q1 * U->W; t1 = T1 * q1 * U->H;
		if (U->mip) {
			float dsx = (s1 - s0) / m, dtx = (t1 - t0) / m;
			float dsy = (T.sy[0] - S * q0 * T.qy[0]) * q0 * U->W, dty = (T.ty[0] - Tt * q0 * T.qy[0]) * q0 * U->H;
			float rx = dsx * dsx + dtx * dtx, ry = dsy * dsy + dty * dty, r2 = rx > ry ? rx : ry;
			if (r2 > 1.0f) nivel = nivel_de(r2, U->t->nniv, NULL);
		}
		if (!reemplaza) {
			Seg g2;
			seg_color(&g2, fx, fy, m);
			r = g2.c[0]; g = g2.c[1]; b = g2.c[2]; dr = g2.dc[0]; dg = g2.dc[1]; db = g2.dc[2];
		}
		fx += m;
		nv = &U->t->niv[nivel];
		tex = nv->p;
		lw = nv->lw; wm = nv->w - 1; hm = nv->h - 1;
		sh = 16 - lw;
		/* en puntos de este nivel; sin salirse de la coma fija */
		{
			float k = 1.0f / (float)(1 << nivel);
			s0 *= k; s1 *= k; t0 *= k; t1 *= k;
		}
		os = (float)(hgl_suelo(s0) & ~wm); ot = (float)(hgl_suelo(t0) & ~hm);
		s = hgl_red((s0 - os) * 65536.0f); t = hgl_red((t0 - ot) * 65536.0f);
		ds = hgl_red((s1 - s0) * (65536.0f / m)); dt = hgl_red((t1 - t0) * (65536.0f / m));
		hm <<= lw;
		n -= m;
		if (reemplaza) {
			while (m--) {
				int zz = (int)(z >> 15);
				if (zz <= (int)*pz - zk) {
					*pc = tex[((t >> sh) & hm) | ((s >> 16) & wm)];
					if (ez) *pz = (u16)zz;
				}
				pc++; pz++; z += dz; s += ds; t += dt;
			}
		} else if (T.gris) {
			/* el mismo valor en r, g y b: una sola multiplicación (5-6-5 abierto en 32 bits) */
			while (m--) {
				int zz = (int)(z >> 15);
				if (zz <= (int)*pz - zk) {
					u32 p = tex[((t >> sh) & hm) | ((s >> 16) & wm)];
					u32 v = (p | (p << 16)) & 0x07E0F81Fu, k = ((u32)r * 33) >> 24;   /* 0..32 */
					v = ((v * k) >> 5) & 0x07E0F81Fu;
					*pc = (u16)(v | (v >> 16));
					if (ez) *pz = (u16)zz;
				}
				pc++; pz++; z += dz; s += ds; t += dt; r += dr;
			}
		} else {
			while (m--) {
				int zz = (int)(z >> 15);
				if (zz <= (int)*pz - zk) {
					u32 p = tex[((t >> sh) & hm) | ((s >> 16) & wm)];
					u32 rr = ((p >> 11) * ((u32)(r >> 16) + 1)) >> 8, gg = (((p >> 5) & 63) * ((u32)(g >> 16) + 1)) >> 8, bb = ((p & 31) * ((u32)(b >> 16) + 1)) >> 8;
					*pc = (u16)((rr << 11) | (gg << 5) | bb);
					if (ez) *pz = (u16)zz;
				}
				pc++; pz++; z += dz; s += ds; t += dt; r += dr; g += dg; b += db;
			}
		}
		S = S1; Tt = T1; Q = Q1; q0 = q1;
	}
}

typedef void (*FTramo)(u16 *, u16 *, int, int, int, float, float);

/* el plano de un atributo: d/dx, d/dy y su valor en (0, 0) del búfer (centros de los puntos) */
static void plano(float a0, float a1, float a2, const float *X, const float *Y, float ia, float *dx, float *dy, float *b0)
{
	float d1 = a1 - a0, d2 = a2 - a0;
	float gx = (d1 * (Y[2] - Y[0]) - d2 * (Y[1] - Y[0])) * ia;
	float gy = (d2 * (X[1] - X[0]) - d1 * (X[2] - X[0])) * ia;
	*dx = gx; *dy = gy;
	*b0 = a0 - gx * (X[0] - 0.5f) - gy * (Y[0] - 0.5f);
}

void hgl_triangulo(Ctx *C, const Vtx *a, const Vtx *b, const Vtx *c, int lado, const float *col_plano)
{
	const Vtx *v[3] = { a, b, c };
	float X[3], Y[3], area, ia;
	int k, u, yi, y0, y1, top, mid, bot;
	Caja K;
	FTramo fn;
	if (!C->color) return;
	X[0] = a->x; X[1] = b->x; X[2] = c->x;
	Y[0] = a->y; Y[1] = b->y; Y[2] = c->y;
	area = (X[1] - X[0]) * (Y[2] - Y[0]) - (X[2] - X[0]) * (Y[1] - Y[0]);
	if (area == 0 || area != area) return;
	ia = 1.0f / area;
	caja(C, &K);
	if (K.x0 >= K.x1 || K.y0 >= K.y1) return;
	C->n_tri++;
	T.C = C;
	/* profundidad */
	plano(a->z, b->z, c->z, X, Y, ia, &T.zx, &T.zy, &T.z0);
	if (C->hab.desplaza_r && (C->pol.desp_f != 0 || C->pol.desp_u != 0)) {
		float m = hgl_abs(T.zx) > hgl_abs(T.zy) ? hgl_abs(T.zx) : hgl_abs(T.zy);
		T.z0 += C->pol.desp_f * m + C->pol.desp_u;
	}
	/* el plano de 1/w: si cambia, el color, la niebla y las texturas van con perspectiva */
	plano(a->iw, b->iw, c->iw, X, Y, ia, &T.wx, &T.wy, &T.w0);
	{
		float mn = a->iw, mx = a->iw;
		mn = b->iw < mn ? b->iw : mn; mx = b->iw > mx ? b->iw : mx;
		mn = c->iw < mn ? c->iw : mn; mx = c->iw > mx ? c->iw : mx;
		T.pcol = mx - mn > mx * 0.002f;
	}
	/* el color */
	if (col_plano) {
		T.liso = 1;
		for (k = 0; k < 4; k++) T.col_k[k] = hgl_red(hgl_lim01(col_plano[k]) * 255.0f);
	} else {
		const float *ca = lado ? a->colb : a->col, *cb = lado ? b->colb : b->col, *cc = lado ? c->colb : c->col;
		T.liso = ca[0] == cb[0] && ca[0] == cc[0] && ca[1] == cb[1] && ca[1] == cc[1] && ca[2] == cb[2] && ca[2] == cc[2] && ca[3] == cb[3] && ca[3] == cc[3];
		if (T.liso) for (k = 0; k < 4; k++) T.col_k[k] = hgl_red(hgl_lim01(ca[k]) * 255.0f);
		else {
			float wa = T.pcol ? a->iw : 1, wb = T.pcol ? b->iw : 1, wc = T.pcol ? c->iw : 1;
			for (k = 0; k < 4; k++)
				plano(hgl_lim01(ca[k]) * 255.0f * wa, hgl_lim01(cb[k]) * 255.0f * wb, hgl_lim01(cc[k]) * 255.0f * wc, X, Y, ia, &T.cx[k], &T.cy[k], &T.c0[k]);
		}
		T.gris = !T.liso && ca[0] == ca[1] && ca[1] == ca[2] && cb[0] == cb[1] && cb[1] == cb[2] && cc[0] == cc[1] && cc[1] == cc[2];
	}
	if (T.liso) T.gris = T.col_k[0] == T.col_k[1] && T.col_k[1] == T.col_k[2];
	/* las texturas */
	T.nu = 0;
	T.persp = 0;
	for (u = 0; u < UNIDADES; u++) {
		Tex *t = hgl_tex_activa(C, u);
		UT *U;
		float q[3];
		if (!t) continue;
		U = &T.u[T.nu];
		T.ui[T.nu] = u;
		U->t = t;
		U->W = (float)t->niv[0].w; U->H = (float)t->niv[0].h;
		U->repite_s = t->ws == GL_REPEAT; U->repite_t = t->wt == GL_REPEAT;
		U->fmag = t->magf == GL_LINEAR;
		U->fmin = t->minf == GL_LINEAR || t->minf == GL_LINEAR_MIPMAP_NEAREST || t->minf == GL_LINEAR_MIPMAP_LINEAR;
		U->mip = t->nniv > 1 && (t->minf != GL_NEAREST && t->minf != GL_LINEAR) ?
			((t->minf == GL_NEAREST_MIPMAP_LINEAR || t->minf == GL_LINEAR_MIPMAP_LINEAR) && hgl_calidad >= 2 ? 2 : 1) : 0;
		if (hgl_calidad == 0) { U->fmag = U->fmin = 0; if (U->mip == 2) U->mip = 1; }
		U->modo = modo_de(C->tex.env[u], t->base);
		for (k = 0; k < 4; k++) U->env[k] = hgl_red(C->tex.env_col[u][k] * 255.0f);
		for (k = 0; k < 3; k++) q[k] = v[k]->t[u][3] * v[k]->iw;
		plano(v[0]->t[u][0] * v[0]->iw, v[1]->t[u][0] * v[1]->iw, v[2]->t[u][0] * v[2]->iw, X, Y, ia, &T.sx[T.nu], &T.sy[T.nu], &T.s0[T.nu]);
		plano(v[0]->t[u][1] * v[0]->iw, v[1]->t[u][1] * v[1]->iw, v[2]->t[u][1] * v[2]->iw, X, Y, ia, &T.tx[T.nu], &T.ty[T.nu], &T.t0[T.nu]);
		plano(q[0], q[1], q[2], X, Y, ia, &T.qx[T.nu], &T.qy[T.nu], &T.q0[T.nu]);
		{
			float mn = q[0], mx = q[0];
			for (k = 1; k < 3; k++) { if (q[k] < mn) mn = q[k]; if (q[k] > mx) mx = q[k]; }
			if (mx - mn > mx * 0.002f) T.persp = 1;
		}
		T.nu++;
	}
	T.seg = T.persp || T.pcol ? (C->sug[0] == GL_FASTEST ? SEG * 2 : SEG) : 4096;
	if (T.seg <= SEG * 2 && C->sug[0] != GL_FASTEST) {
		/* las paredes muy de lado: tramos más cortos si 1/w cambia mucho en uno */
		float mn = a->iw < b->iw ? a->iw : b->iw, d = hgl_abs(T.wx) * T.seg;
		mn = c->iw < mn ? c->iw : mn;
		if (d > mn * 0.5f) T.seg = 4;
		else if (d > mn * 0.25f) T.seg = 8;
	}
	/* la niebla */
	T.niebla = C->hab.niebla;
	if (T.niebla) {
		if (a->niebla >= 1 && b->niebla >= 1 && c->niebla >= 1) T.niebla = 0;
		else {
			float wa = T.pcol ? a->iw : 1, wb = T.pcol ? b->iw : 1, wc = T.pcol ? c->iw : 1;
			plano(a->niebla * 255.0f * wa, b->niebla * 255.0f * wb, c->niebla * 255.0f * wc, X, Y, ia, &T.fx, &T.fy, &T.f0);
			for (k = 0; k < 3; k++) T.nie_c[k] = hgl_red(C->nie.col[k] * 255.0f);
		}
	}
	/* lo de los fragmentos */
	T.prof = C->hab.prof;
	T.prof_f = C->profb.f;
	T.escribe_z = T.prof && C->profb.escribe;
	T.zk = !T.prof ? -100000 : T.prof_f == GL_LESS ? 1 : T.prof_f == GL_LEQUAL ? 0 : T.prof_f == GL_ALWAYS ? -100000 : 99;
	T.alfa_f = C->hab.alfa ? C->colb.alfa_f : GL_ALWAYS;
	T.alfa_ref = hgl_red(C->colb.alfa_ref * 255.0f);
	T.mezcla = C->hab.mezcla;
	T.m_or = C->colb.m_or; T.m_de = C->colb.m_de;
	if (!T.mezcla || (T.m_or == GL_ONE && T.m_de == GL_ZERO)) T.mtipo = MZ_NADA;
	else if (T.m_or == GL_SRC_ALPHA && T.m_de == GL_ONE_MINUS_SRC_ALPHA) T.mtipo = MZ_ALFA;
	else if (T.m_or == GL_ONE && T.m_de == GL_ONE) T.mtipo = MZ_SUMA;
	else if ((T.m_or == GL_ZERO && T.m_de == GL_SRC_COLOR) || (T.m_or == GL_DST_COLOR && T.m_de == GL_ZERO)) T.mtipo = MZ_MULT;
	else if (T.m_or == GL_SRC_ALPHA && T.m_de == GL_ONE) T.mtipo = MZ_ALFA_SUMA;
	else T.mtipo = MZ_OTRA;
	T.masc = (u16)((C->colb.mascara[0] ? 0xF800 : 0) | (C->colb.mascara[1] ? 0x07E0 : 0) | (C->colb.mascara[2] ? 0x001F : 0));
	if (!C->colb.mascara[0] && !C->colb.mascara[1] && !C->colb.mascara[2]) T.masc = 0;
	T.punteo = C->hab.punteado_p;
	/* el camino */
	{
		int simple = T.mtipo == MZ_NADA && T.alfa_f == GL_ALWAYS && !T.niebla && T.zk != 99 && T.masc == 0xFFFF && !T.punteo;
		if (simple && T.nu == 0) fn = tramo_color;
		else if (simple && T.nu == 1 && T.u[0].t->fmt == TF_565 && T.u[0].repite_s && T.u[0].repite_t &&
			 !T.u[0].fmag && !T.u[0].fmin && T.u[0].mip != 2 &&
			 (T.u[0].modo == M_MOD || T.u[0].modo == M_REP_RGB || T.u[0].modo == M_REP_RGBA))
			fn = tramo_tex;
		else fn = tramo_todo;
	}
	/* las filas: el de arriba, el del medio y el de abajo */
	top = 0; mid = 1; bot = 2;
	if (Y[top] > Y[mid]) { k = top; top = mid; mid = k; }
	if (Y[mid] > Y[bot]) { k = mid; mid = bot; bot = k; }
	if (Y[top] > Y[mid]) { k = top; top = mid; mid = k; }
	y0 = hgl_techo(Y[top] - 0.5f);
	y1 = hgl_techo(Y[bot] - 0.5f);
	if (y0 < K.y0) y0 = K.y0;
	if (y1 > K.y1) y1 = K.y1;
	{
		float dxl = (Y[bot] != Y[top]) ? (X[bot] - X[top]) / (Y[bot] - Y[top]) : 0;
		float dx1 = (Y[mid] != Y[top]) ? (X[mid] - X[top]) / (Y[mid] - Y[top]) : 0;
		float dx2 = (Y[bot] != Y[mid]) ? (X[bot] - X[mid]) / (Y[bot] - Y[mid]) : 0;
		for (yi = y0; yi < y1; yi++) {
			float yc = yi + 0.5f, xa, xb, xl, xr;
			int x0, x1;
			xa = X[top] + (yc - Y[top]) * dxl;
			if (yc < Y[mid]) xb = X[top] + (yc - Y[top]) * dx1;
			else xb = X[mid] + (yc - Y[mid]) * dx2;
			if (xa < xb) { xl = xa; xr = xb; } else { xl = xb; xr = xa; }
			x0 = hgl_techo(xl - 0.5f);
			x1 = hgl_techo(xr - 0.5f);
			if (x0 < K.x0) x0 = K.x0;
			if (x1 > K.x1) x1 = K.x1;
			if (x0 >= x1) continue;
			fn(C->color + yi * C->an + x0, C->prof ? C->prof + yi * C->an + x0 : NULL, x0, yi, x1 - x0, (float)x0, (float)yi);
		}
	}
}

/* ---- un fragmento suelto (líneas, puntos, glDrawPixels, glBitmap) ---- */
void hgl_fragmento(Ctx *C, int x, int y, u32 z, int r, int g, int b, int a)
{
	Caja K;
	int c[4];
	u16 *pz;
	caja(C, &K);
	if (x < K.x0 || y < K.y0 || x >= K.x1 || y >= K.y1 || !C->color) return;
	T.C = C;
	pz = C->prof ? C->prof + y * C->an + x : NULL;
	if (C->hab.prof && pz && !pasa_z(C->profb.f, z, *pz)) return;
	c[0] = lim8(r); c[1] = lim8(g); c[2] = lim8(b); c[3] = lim8(a);
	if (C->hab.alfa && !pasa_a(C->colb.alfa_f, c[3], hgl_red(C->colb.alfa_ref * 255.0f))) return;
	if (C->hab.prof && pz && C->profb.escribe) *pz = (u16)z;
	T.m_or = C->colb.m_or; T.m_de = C->colb.m_de;
	T.mtipo = !C->hab.mezcla || (T.m_or == GL_ONE && T.m_de == GL_ZERO) ? MZ_NADA : MZ_OTRA;
	T.masc = (u16)((C->colb.mascara[0] ? 0xF800 : 0) | (C->colb.mascara[1] ? 0x07E0 : 0) | (C->colb.mascara[2] ? 0x001F : 0));
	escribe(C->color + y * C->an + x, c);
}

static void color_de(Ctx *C, const Vtx *v, int lado, const float *plano, float *c)
{
	const float *s = plano ? plano : lado ? v->colb : v->col;
	int k;
	for (k = 0; k < 4; k++) c[k] = hgl_lim01(s[k]) * 255.0f;
	if (C->hab.niebla) {
		float f = hgl_lim01(v->niebla);
		for (k = 0; k < 3; k++) c[k] = c[k] * f + C->nie.col[k] * 255.0f * (1 - f);
	}
}
/* el color de una textura en un vértice (sin perspectiva: líneas y puntos) */
static void textura_en(Ctx *C, const Vtx *v, float *c)
{
	int u;
	for (u = 0; u < UNIDADES; u++) {
		Tex *t = hgl_tex_activa(C, u);
		UT U;
		int col[4], k;
		float q;
		if (!t) continue;
		hgl_pon(&U, 0, sizeof U);
		U.t = t; U.repite_s = t->ws == GL_REPEAT; U.repite_t = t->wt == GL_REPEAT;
		U.modo = modo_de(C->tex.env[u], t->base);
		for (k = 0; k < 4; k++) U.env[k] = hgl_red(C->tex.env_col[u][k] * 255.0f);
		q = v->t[u][3] != 0 ? 1.0f / v->t[u][3] : 1;
		for (k = 0; k < 4; k++) col[k] = hgl_red(c[k]);
		entorno(&U, col, muestra(&U, hgl_red(v->t[u][0] * q * t->niv[0].w * 65536.0f), hgl_red(v->t[u][1] * q * t->niv[0].h * 65536.0f), 0, 0));
		for (k = 0; k < 4; k++) c[k] = (float)col[k];
	}
}

/* las líneas: un punto por columna (o por fila), en su centro, sin el último
 * (la regla del rombo de OpenGL, aproximada) */
void hgl_linea(Ctx *C, const Vtx *a, const Vtx *b, int lado, const float *plano)
{
	float ca[4], cb[4], dx = b->x - a->x, dy = b->y - a->y;
	int k, w = (int)(C->linea + 0.5f), i0, i1, i, pat = 0, xmayor = hgl_abs(dx) >= hgl_abs(dy);
	if (w < 1) w = 1;
	color_de(C, a, lado, plano, ca);
	color_de(C, b, lado, plano, cb);
	if (C->hab.tex2d[0] || C->hab.tex1d[0]) { textura_en(C, a, ca); textura_en(C, b, cb); }
	if (xmayor) { if (dx == 0) return; i0 = hgl_techo((dx > 0 ? a->x : b->x) - 0.5f); i1 = hgl_techo((dx > 0 ? b->x : a->x) - 0.5f); }
	else { i0 = hgl_techo((dy > 0 ? a->y : b->y) - 0.5f); i1 = hgl_techo((dy > 0 ? b->y : a->y) - 0.5f); }
	for (i = i0; i < i1; i++) {
		float c = i + 0.5f, t = xmayor ? (c - a->x) / dx : (c - a->y) / dy, o, col[4];
		int p, j, dibuja = 1;
		if (t < 0) t = 0;
		if (t > 1) t = 1;
		o = xmayor ? a->y + t * dy : a->x + t * dx;
		p = hgl_suelo(o);
		for (k = 0; k < 4; k++) col[k] = ca[k] + t * (cb[k] - ca[k]);
		if (C->hab.punteado_l) { dibuja = (C->punteado_p >> ((pat / C->punteado_f) & 15)) & 1; pat++; }
		if (!dibuja) continue;
		for (j = -(w - 1) / 2; j <= w / 2; j++) {
			u32 z = (u32)hgl_red(a->z + t * (b->z - a->z));
			if (xmayor) hgl_fragmento(C, i, p + j, z, hgl_red(col[0]), hgl_red(col[1]), hgl_red(col[2]), hgl_red(col[3]));
			else hgl_fragmento(C, p + j, i, z, hgl_red(col[0]), hgl_red(col[1]), hgl_red(col[2]), hgl_red(col[3]));
		}
	}
}

void hgl_punto(Ctx *C, const Vtx *a, int lado)
{
	float c[4];
	int s = (int)(C->punto + 0.5f), x0, y0, i, j;
	if (s < 1) s = 1;
	color_de(C, a, lado, NULL, c);
	if (C->hab.tex2d[0] || C->hab.tex1d[0]) textura_en(C, a, c);
	x0 = hgl_suelo(a->x - (s - 1) * 0.5f);
	y0 = hgl_suelo(a->y - (s - 1) * 0.5f);
	for (j = 0; j < s; j++)
		for (i = 0; i < s; i++)
			hgl_fragmento(C, x0 + i, y0 + j, (u32)hgl_red(a->z), hgl_red(c[0]), hgl_red(c[1]), hgl_red(c[2]), hgl_red(c[3]));
}

void hgl_raster_ini(void) {}
