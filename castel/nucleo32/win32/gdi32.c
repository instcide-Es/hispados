/* gdi32.c - WIN32.BIN: GDI32. Se pinta en superficies (ventanas.h): el lienzo
 * de 32 bits de cada ventana, los mapas de bits (de 32 bits, o de 1: los
 * monocromos) y las secciones DIB con su formato (1, 4, 8, 16, 24 y 32 bits:
 * el programa escribe en sus puntos). Los DC llevan su recorte como una lista
 * de rectángulos. Las letras son la de 8x16 de Hispa-Pentium (la de Hispa
 * Ventanas), agrandada a la medida que se pide; en negrita, dos veces.
 * Lo que hace cada función: la documentación pública del API de Windows.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "ventanas.h"
#include "k32.h"

#define API(t) t WINAPI

/* ---------------------------------------------------------------------------
 *  Los puntos
 * ------------------------------------------------------------------------- */
u32 rgb(COLORREF c)
{
	if ((c >> 24) == 1) {
		/* PALETTEINDEX: los 16 de siempre */
		static const u32 p16[16] = { 0x000000, 0x800000, 0x008000, 0x808000, 0x000080, 0x800080, 0x008080, 0xC0C0C0,
			0x808080, 0xFF0000, 0x00FF00, 0xFFFF00, 0x0000FF, 0xFF00FF, 0x00FFFF, 0xFFFFFF };
		return p16[c & 15];
	}
	return ((c & 0xFF) << 16) | (c & 0xFF00) | ((c >> 16) & 0xFF);
}
static COLORREF de_rgb(u32 v) { return ((v >> 16) & 0xFF) | (v & 0xFF00) | ((v & 0xFF) << 16); }

static int bit_bajo(u32 m) { int k = 0; if (!m) return 0; while (!(m & 1)) { m >>= 1; k++; } return k; }
static int bits_de(u32 m) { int k = 0; while (m) { k += m & 1; m >>= 1; } return k; }

/* el más parecido de la paleta */
static u32 cerca(Sup *s, u32 v)
{
	static Sup *ult_s;
	static u32 ult_v, ult_r;
	u32 k, mejor = 0, d0 = 0xFFFFFFFF;
	if (s == ult_s && v == ult_v)
		return ult_r;
	for (k = 0; k < s->npal; k++) {
		i32 dr = (i32)((s->pal[k] >> 16) & 255) - (i32)((v >> 16) & 255);
		i32 dg = (i32)((s->pal[k] >> 8) & 255) - (i32)((v >> 8) & 255);
		i32 db = (i32)(s->pal[k] & 255) - (i32)(v & 255);
		u32 d = dr * dr * 3 + dg * dg * 4 + db * db * 2;
		if (d < d0) {
			d0 = d;
			mejor = k;
			if (!d)
				break;
		}
	}
	ult_s = s; ult_v = v; ult_r = mejor;
	return mejor;
}
/* 0x00RRGGBB -> el valor del punto en esa superficie */
static u32 valor_de(Sup *s, u32 v)
{
	switch (s->bpp) {
	case 32:
		if (s->mono)
			return (v & 0xFFFFFF) == 0xFFFFFF ? 0xFFFFFF : (v & 0xFFFFFF ? 0xFFFFFF : 0);
		return v & 0xFFFFFF;
	case 24:
		return v & 0xFFFFFF;
	case 16: {
		u32 r = (v >> 16) & 255, g = (v >> 8) & 255, b = v & 255;
		return ((r >> (8 - bits_de(s->mr))) << bit_bajo(s->mr)) | ((g >> (8 - bits_de(s->mg))) << bit_bajo(s->mg)) | ((b >> (8 - bits_de(s->mb))) << bit_bajo(s->mb));
	}
	default:
		return cerca(s, v);
	}
}
/* el valor del punto -> 0x00RRGGBB */
static u32 color_de(Sup *s, u32 p)
{
	switch (s->bpp) {
	case 32: case 24:
		return p & 0xFFFFFF;
	case 16: {
		int nr = bits_de(s->mr), ng = bits_de(s->mg), nb = bits_de(s->mb);
		u32 r = ((p & s->mr) >> bit_bajo(s->mr)) << (8 - nr), g = ((p & s->mg) >> bit_bajo(s->mg)) << (8 - ng), b = ((p & s->mb) >> bit_bajo(s->mb)) << (8 - nb);
		r |= r >> nr; g |= g >> ng; b |= b >> nb;
		return ((r & 255) << 16) | ((g & 255) << 8) | (b & 255);
	}
	default:
		return p < s->npal ? s->pal[p] : 0;
	}
}
static inline u8 *fila(Sup *s, i32 y) { return s->bits + y * s->paso; }
static u32 leer(Sup *s, i32 x, i32 y)
{
	u8 *f = fila(s, y);
	switch (s->bpp) {
	case 32: return ((u32 *)f)[x];
	case 24: f += x * 3; return f[0] | (f[1] << 8) | (f[2] << 16);
	case 16: return ((u16 *)f)[x];
	case 8: return f[x];
	case 4: return (f[x >> 1] >> ((x & 1) ? 0 : 4)) & 15;
	case 1: return (f[x >> 3] >> (7 - (x & 7))) & 1;
	}
	return 0;
}
static void poner(Sup *s, i32 x, i32 y, u32 v)
{
	u8 *f = fila(s, y);
	switch (s->bpp) {
	case 32: ((u32 *)f)[x] = v; break;
	case 24: f += x * 3; f[0] = v; f[1] = v >> 8; f[2] = v >> 16; break;
	case 16: ((u16 *)f)[x] = v; break;
	case 8: f[x] = v; break;
	case 4: if (x & 1) f[x >> 1] = (f[x >> 1] & 0xF0) | (v & 15); else f[x >> 1] = (f[x >> 1] & 15) | (v << 4); break;
	case 1: if (v & 1) f[x >> 3] |= 0x80 >> (x & 7); else f[x >> 3] &= ~(0x80 >> (x & 7)); break;
	}
}
static inline u32 leer_rgb(Sup *s, i32 x, i32 y) { return s->bpp == 32 ? ((u32 *)fila(s, y))[x] & 0xFFFFFF : color_de(s, leer(s, x, y)); }
static inline void poner_rgb(Sup *s, i32 x, i32 y, u32 c) { if (s->bpp == 32 && !s->mono) ((u32 *)fila(s, y))[x] = c; else poner(s, x, y, valor_de(s, c)); }
u32 a_punto(Sup *s, COLORREF c) { return valor_de(s, rgb(c)); }
COLORREF de_punto(Sup *s, u32 v) { return de_rgb(color_de(s, v)); }

/* las dos de ROP2 (sobre 0x00RRGGBB) */
static u32 rop2(int r, u32 p, u32 d)
{
	switch (r) {
	case 1: return 0;
	case 2: return ~(p | d);
	case 3: return ~p & d;
	case 4: return ~p;
	case 5: return p & ~d;
	case 6: return ~d;
	case 7: return p ^ d;
	case 8: return ~(p & d);
	case 9: return p & d;
	case 10: return ~(p ^ d);
	case 11: return d;
	case 12: return ~p | d;
	case 14: return p | ~d;
	case 15: return p | d;
	case 16: return 0xFFFFFF;
	}
	return p;
}

/* ---------------------------------------------------------------------------
 *  Las regiones (listas de rectángulos que no se pisan)
 * ------------------------------------------------------------------------- */
static int corta(RECT *a, const RECT *b)
{
	a->left = MAX(a->left, b->left);
	a->top = MAX(a->top, b->top);
	a->right = MIN(a->right, b->right);
	a->bottom = MIN(a->bottom, b->bottom);
	return a->left < a->right && a->top < a->bottom;
}
void reg_rect(Region *g, const RECT *r)
{
	g->n = 0;
	if (r->left < r->right && r->top < r->bottom) {
		g->r[0] = *r;
		g->n = 1;
	}
}
int reg_vacia(const Region *g) { return g->n == 0; }
void reg_corta(Region *g, const RECT *r)
{
	int i, n = 0;
	for (i = 0; i < g->n; i++) {
		RECT a = g->r[i];
		if (corta(&a, r))
			g->r[n++] = a;
	}
	g->n = n;
}
static void reg_pon(Region *g, i32 l, i32 t, i32 r, i32 b)
{
	if (l < r && t < b && g->n < MAX_RECTS) {
		g->r[g->n].left = l; g->r[g->n].top = t; g->r[g->n].right = r; g->r[g->n].bottom = b;
		g->n++;
	}
}
void reg_quita(Region *g, const RECT *q)
{
	Region o = *g;
	int i;
	g->n = 0;
	for (i = 0; i < o.n; i++) {
		RECT a = o.r[i], c = a;
		if (!corta(&c, q)) {
			reg_pon(g, a.left, a.top, a.right, a.bottom);
			continue;
		}
		reg_pon(g, a.left, a.top, a.right, c.top);
		reg_pon(g, a.left, c.bottom, a.right, a.bottom);
		reg_pon(g, a.left, c.top, c.left, c.bottom);
		reg_pon(g, c.right, c.top, a.right, c.bottom);
	}
}
static void reg_une(Region *g, const Region *o)
{
	int i;
	for (i = 0; i < o->n; i++) {
		reg_quita(g, &o->r[i]);
	}
	for (i = 0; i < o->n; i++)
		reg_pon(g, o->r[i].left, o->r[i].top, o->r[i].right, o->r[i].bottom);
}
static void reg_corta_reg(Region *g, const Region *o)
{
	Region res;
	int i, j;
	res.n = 0;
	for (i = 0; i < g->n; i++)
		for (j = 0; j < o->n; j++) {
			RECT a = g->r[i];
			if (corta(&a, &o->r[j]))
				reg_pon(&res, a.left, a.top, a.right, a.bottom);
		}
	*g = res;
}
static void reg_caja(const Region *g, RECT *r)
{
	int i;
	if (!g->n) {
		r->left = r->top = r->right = r->bottom = 0;
		return;
	}
	*r = g->r[0];
	for (i = 1; i < g->n; i++) {
		r->left = MIN(r->left, g->r[i].left);
		r->top = MIN(r->top, g->r[i].top);
		r->right = MAX(r->right, g->r[i].right);
		r->bottom = MAX(r->bottom, g->r[i].bottom);
	}
}
static void reg_mueve(Region *g, i32 dx, i32 dy)
{
	int i;
	for (i = 0; i < g->n; i++) {
		g->r[i].left += dx; g->r[i].right += dx;
		g->r[i].top += dy; g->r[i].bottom += dy;
	}
}

/* ---------------------------------------------------------------------------
 *  Rellenar y poner puntos, con el recorte
 * ------------------------------------------------------------------------- */
void rellenar(Sup *s, const Region *clip, RECT r, u32 color)
{
	int i;
	RECT tope = { 0, 0, s->an, s->al };
	if (!s->bits || !corta(&r, &tope))
		return;
	for (i = 0; i < clip->n; i++) {
		RECT a = r;
		i32 x, y;
		if (!corta(&a, &clip->r[i]))
			continue;
		if (s->bpp == 32) {
			u32 v = s->mono ? valor_de(s, color) : color;
			for (y = a.top; y < a.bottom; y++) {
				u32 *f = (u32 *)fila(s, y) + a.left;
				for (x = a.right - a.left; x > 0; x--)
					*f++ = v;
			}
		} else {
			u32 v = valor_de(s, color);
			for (y = a.top; y < a.bottom; y++)
				for (x = a.left; x < a.right; x++)
					poner(s, x, y, v);
		}
	}
}
void sup_rect(Sup *s, const Region *clip, i32 x, i32 y, i32 an, i32 al, COLORREF c)
{
	RECT r = { x, y, x + an, y + al };
	rellenar(s, clip, r, rgb(c));
}
static int en_clip(const Region *g, i32 x, i32 y)
{
	int i;
	for (i = 0; i < g->n; i++)
		if (x >= g->r[i].left && x < g->r[i].right && y >= g->r[i].top && y < g->r[i].bottom)
			return 1;
	return 0;
}

/* ---------------------------------------------------------------------------
 *  Los objetos
 * ------------------------------------------------------------------------- */
Gdi *gdi(HGDIOBJ h)
{
	Gdi *g = (Gdi *)h;
	if (!g || (u32)g < 0x10000 || (u32)g >= 0x7FBF0000 || g->magia != MAGIA_GDI)
		return 0;
	return g;
}
static Gdi *nuevo_gdi(int tipo)
{
	Gdi *g = m_cero(sizeof *g);
	if (g) {
		g->magia = MAGIA_GDI;
		g->tipo = tipo;
	}
	return g;
}
Dc *dc(HDC h)
{
	Dc *d = (Dc *)h;
	if (!d || (u32)d < 0x10000 || (u32)d >= 0x7FBF0000 || d->magia != MAGIA_DC)
		return 0;
	return d;
}
static Gdi stock[20];
static Gdi mapa_mono;              /* el mapa de 1x1 de los DC de memoria nuevos */
static u32 punto_mono;
static void prepara_stock(void)
{
	static int hecho;
	int k;
	static const COLORREF br[6] = { 0xFFFFFF, 0xC0C0C0, 0x808080, 0x404040, 0x000000, 0 };
	if (hecho)
		return;
	hecho = 1;
	for (k = 0; k < 20; k++) {
		stock[k].magia = MAGIA_GDI;
		stock[k].stock = 1;
	}
	for (k = 0; k < 6; k++) {
		stock[k].tipo = G_BROCHA;
		stock[k].u.brocha.color = br[k];
		stock[k].u.brocha.estilo = k == 5 ? 1 : 0;   /* NULL_BRUSH */
	}
	stock[6].tipo = G_PLUMA; stock[6].u.pluma.color = 0xFFFFFF;
	stock[7].tipo = G_PLUMA; stock[7].u.pluma.color = 0;
	stock[8].tipo = G_PLUMA; stock[8].u.pluma.estilo = 5;     /* NULL_PEN */
	for (k = 10; k <= 17; k++) {
		stock[k].tipo = G_LETRA;
		stock[k].u.letra.alto = 16;
		stock[k].u.letra.peso = 400;
		stock[k].u.letra.fija = k == 10 || k == 11 || k == 16;
		strcpy(stock[k].u.letra.cara, k == 11 || k == 10 ? "Fixedsys" : "System");
	}
	stock[15].tipo = G_PALETA;
	stock[15].u.paleta.n = 20;
	stock[18].tipo = G_BROCHA;     /* DC_BRUSH */
	stock[19].tipo = G_PLUMA;      /* DC_PEN */
	mapa_mono.magia = MAGIA_GDI;
	mapa_mono.tipo = G_MAPA;
	mapa_mono.stock = 1;
	mapa_mono.u.mapa.s.bits = (u8 *)&punto_mono;
	mapa_mono.u.mapa.s.an = mapa_mono.u.mapa.s.al = 1;
	mapa_mono.u.mapa.s.paso = 4;
	mapa_mono.u.mapa.s.bpp = 32;
	mapa_mono.u.mapa.s.mono = 1;
}
API(HGDIOBJ) GetStockObject(i32 i)
{
	prepara_stock();
	if (i < 0 || i > 19 || i == 9)
		return 0;
	return (HGDIOBJ)&stock[i];
}
API(HPEN) CreatePen(i32 e, i32 a, COLORREF c)
{
	Gdi *g = nuevo_gdi(G_PLUMA);
	g->u.pluma.estilo = e & 15;
	g->u.pluma.ancho = a < 1 ? 1 : a;
	g->u.pluma.color = c;
	return (HPEN)g;
}
API(HPEN) CreatePenIndirect(const u32 *lp) { return CreatePen(lp[0], lp[1], lp[3]); }
API(HPEN) ExtCreatePen(u32 e, u32 a, const u32 *lb, u32 n, const u32 *st) { (void)n; (void)st; return CreatePen(e & 15, a, lb[1]); }
API(HBRUSH) CreateSolidBrush(COLORREF c)
{
	Gdi *g = nuevo_gdi(G_BROCHA);
	g->u.brocha.color = c;
	return (HBRUSH)g;
}
API(HBRUSH) CreateHatchBrush(i32 t, COLORREF c)
{
	Gdi *g = nuevo_gdi(G_BROCHA);
	g->u.brocha.color = c;
	g->u.brocha.estilo = 2;
	g->u.brocha.trama = t;
	return (HBRUSH)g;
}
API(HBRUSH) CreatePatternBrush(HBITMAP b)
{
	Gdi *g = nuevo_gdi(G_BROCHA);
	g->u.brocha.estilo = 3;
	g->u.brocha.mapa = gdi(b);
	return (HBRUSH)g;
}
API(HBRUSH) CreateBrushIndirect(const u32 *lb)
{
	if (lb[0] == 1)
		return GetStockObject(5);
	if (lb[0] == 2)
		return CreateHatchBrush(lb[2], lb[1]);
	if (lb[0] == 3)
		return CreatePatternBrush((HBITMAP)lb[2]);
	return CreateSolidBrush(lb[1]);
}
API(HBRUSH) CreateDIBPatternBrushPt(const void *p, u32 u) { (void)p; (void)u; return CreateSolidBrush(0xC0C0C0); }

typedef struct {
	i32 alto, ancho, esc, ori, peso;
	u8 cursiva, subraya, tachada, juego, prec_sal, prec_rec, calidad, paso_familia;
	char cara[32];
} LOGFONTA;
API(HFONT) CreateFontIndirectA(const LOGFONTA *l)
{
	Gdi *g = nuevo_gdi(G_LETRA);
	g->u.letra.alto = l->alto;
	g->u.letra.ancho = l->ancho;
	g->u.letra.peso = l->peso ? l->peso : 400;
	g->u.letra.cursiva = l->cursiva;
	g->u.letra.subraya = l->subraya;
	g->u.letra.tachada = l->tachada;
	g->u.letra.fija = (l->paso_familia & 3) == 1 || (l->paso_familia & 0xF0) == 0x30;
	copia_n(g->u.letra.cara, l->cara, sizeof g->u.letra.cara);
	if (!stricmp_(l->cara, "Courier") || !stricmp_(l->cara, "Courier New") || !stricmp_(l->cara, "Fixedsys") || !stricmp_(l->cara, "Terminal") || !stricmp_(l->cara, "Lucida Console"))
		g->u.letra.fija = 1;
	return (HFONT)g;
}
API(HFONT) CreateFontA(i32 al, i32 an, i32 esc, i32 ori, i32 peso, u32 cur, u32 sub, u32 tach, u32 juego, u32 ps, u32 pr, u32 cal, u32 pf, const char *cara)
{
	LOGFONTA l;
	memset(&l, 0, sizeof l);
	l.alto = al; l.ancho = an; l.esc = esc; l.ori = ori; l.peso = peso;
	l.cursiva = cur; l.subraya = sub; l.tachada = tach; l.juego = juego;
	l.prec_sal = ps; l.prec_rec = pr; l.calidad = cal; l.paso_familia = pf;
	if (cara)
		copia_n(l.cara, cara, sizeof l.cara);
	return CreateFontIndirectA(&l);
}
API(HFONT) CreateFontIndirectW(const u8 *l)
{
	LOGFONTA a;
	memcpy(&a, l, 28);
	de_w(a.cara, (const WCHAR *)(l + 28), 32);
	return CreateFontIndirectA(&a);
}

/* ---- los mapas de bits ---- */
static Gdi *mapa_nuevo(i32 an, i32 al, int bpp, int mono)
{
	Gdi *g = nuevo_gdi(G_MAPA);
	Sup *s = &g->u.mapa.s;
	if (an < 1) an = 1;
	if (al < 1) al = 1;
	s->an = an;
	s->al = al;
	s->bpp = bpp;
	s->mono = mono;
	s->paso = ((an * bpp + 31) / 32) * 4;
	g->u.mapa.propio = m_cero(s->paso * al + 4);
	if (!g->u.mapa.propio) {
		m_soltar(g);
		pon_error(8);
		return 0;
	}
	s->bits = g->u.mapa.propio;
	if (mono)
		memset(s->bits, 0, s->paso * al);
	if (bpp == 16) { s->mr = 0x7C00; s->mg = 0x3E0; s->mb = 0x1F; }
	return g;
}
API(HBITMAP) CreateBitmap(i32 an, i32 al, u32 planos, u32 bpp, const void *bits)
{
	Gdi *g;
	i32 x, y;
	(void)planos;
	g = mapa_nuevo(an, al, 32, bpp == 1);
	if (!g)
		return 0;
	if (bits) {
		/* como los da Windows: filas de 16 bits, de arriba abajo */
		const u8 *b = bits;
		u32 paso = ((an * bpp + 15) / 16) * 2;
		for (y = 0; y < al; y++)
			for (x = 0; x < an; x++) {
				u32 v;
				const u8 *f = b + y * paso;
				if (bpp == 1) v = (f[x >> 3] >> (7 - (x & 7))) & 1 ? 0xFFFFFF : 0;
				else if (bpp == 8) { u8 c = f[x]; v = (c << 16) | (c << 8) | c; }
				else if (bpp == 16) { u16 c = ((const u16 *)f)[x]; v = ((c & 0x7C00) << 9) | ((c & 0x3E0) << 6) | ((c & 0x1F) << 3); }
				else if (bpp == 24) v = f[x * 3] | (f[x * 3 + 1] << 8) | (f[x * 3 + 2] << 16);
				else v = ((const u32 *)f)[x] & 0xFFFFFF;
				((u32 *)fila(&g->u.mapa.s, y))[x] = v;
			}
	}
	return (HBITMAP)g;
}
API(HBITMAP) CreateBitmapIndirect(const u32 *b) { return CreateBitmap(b[1], b[2], ((u16 *)b)[8], ((u16 *)b)[9], (void *)b[5]); }
API(HBITMAP) CreateCompatibleBitmap(HDC h, i32 an, i32 al)
{
	Dc *d = dc(h);
	int mono = d && d->memoria && d->s && d->s->mono;
	return (HBITMAP)mapa_nuevo(an, al, 32, mono);
}
API(HBITMAP) CreateDiscardableBitmap(HDC h, i32 an, i32 al) { return CreateCompatibleBitmap(h, an, al); }

typedef struct { u32 tam; i32 an, al; u16 planos, bpp; u32 comp, tamimg; i32 xppm, yppm; u32 usados, importantes; } BIH;

/* una superficie que mira unos puntos con el formato de un BITMAPINFO */
static int sup_de_bmi(Sup *s, const void *bmi_, const void *bits, u32 uso, Dc *d)
{
	const BIH *b = bmi_;
	const u32 *tabla;
	u32 n, k;
	memset(s, 0, sizeof *s);
	if (!b || b->tam < 12)
		return 0;
	if (b->tam == 12) {
		/* BITMAPCOREHEADER */
		const u16 *c = bmi_;
		s->an = c[2]; s->al = c[3]; s->bpp = c[5];
		tabla = 0;
		{
			const u8 *t = (const u8 *)bmi_ + 12;
			n = s->bpp <= 8 ? 1u << s->bpp : 0;
			s->npal = n;
			for (k = 0; k < n; k++)
				s->pal[k] = (t[k * 3 + 2] << 16) | (t[k * 3 + 1] << 8) | t[k * 3];
		}
		s->paso = ((s->an * s->bpp + 31) / 32) * 4;
		s->bits = (u8 *)bits + (s->al - 1) * s->paso;
		s->paso = -s->paso;
		s->de_abajo = 1;
		return 1;
	}
	s->an = b->an;
	s->al = b->al < 0 ? -b->al : b->al;
	s->bpp = b->bpp;
	if (s->bpp != 1 && s->bpp != 4 && s->bpp != 8 && s->bpp != 16 && s->bpp != 24 && s->bpp != 32)
		return 0;
	if (b->comp != 0 && b->comp != 3)
		return 0;        /* (RLE, no) */
	tabla = (const u32 *)((const u8 *)bmi_ + b->tam);
	if (s->bpp == 16) {
		if (b->comp == 3) { s->mr = tabla[0]; s->mg = tabla[1]; s->mb = tabla[2]; }
		else { s->mr = 0x7C00; s->mg = 0x3E0; s->mb = 0x1F; }
	}
	if (s->bpp == 32 && b->comp == 3 && tabla[0] != 0xFF0000) {
		/* (32 bits con otras máscaras: como 16, por máscaras) */
		s->mr = tabla[0]; s->mg = tabla[1]; s->mb = tabla[2];
	}
	if (s->bpp <= 8) {
		n = b->usados ? b->usados : 1u << s->bpp;
		if (n > 256)
			n = 256;
		s->npal = n;
		for (k = 0; k < n; k++) {
			if (uso == 1) {
				/* DIB_PAL_COLORS: índices de la paleta del DC */
				u16 i = ((const u16 *)tabla)[k];
				Gdi *p = d && d->paleta ? d->paleta : 0;
				s->pal[k] = p && i < p->u.paleta.n ? p->u.paleta.e[i] : rgb(0x01000000 | i);
			} else
				s->pal[k] = tabla[k] & 0xFFFFFF;
		}
	}
	s->paso = ((s->an * s->bpp + 31) / 32) * 4;
	if (b->al > 0) {
		s->bits = (u8 *)bits + (s->al - 1) * s->paso;
		s->paso = -s->paso;
		s->de_abajo = 1;
	} else
		s->bits = (u8 *)bits;
	return 1;
}
static u32 tam_bits(const BIH *b)
{
	i32 al = b->al < 0 ? -b->al : b->al;
	return ((b->an * b->bpp + 31) / 32) * 4 * al;
}
API(HBITMAP) CreateDIBSection(HDC h, const void *bmi, u32 uso, void **bits, HANDLE sec, u32 off)
{
	const BIH *b = bmi;
	Gdi *g;
	u8 *p;
	(void)sec; (void)off;
	if (!b || b->an <= 0 || !b->al)
		return 0;
	p = m_cero(tam_bits(b) + 16);
	if (!p)
		return 0;
	g = nuevo_gdi(G_MAPA);
	if (!sup_de_bmi(&g->u.mapa.s, bmi, p, uso, dc(h))) {
		m_soltar(p);
		m_soltar(g);
		return 0;
	}
	g->u.mapa.propio = p;
	g->u.mapa.seccion = 1;
	if (bits)
		*bits = p;
	return (HBITMAP)g;
}

/* ---- regiones y paletas ---- */
API(HRGN) CreateRectRgn(i32 a, i32 b, i32 c, i32 d)
{
	Gdi *g = nuevo_gdi(G_REGION);
	RECT r = { MIN(a, c), MIN(b, d), MAX(a, c), MAX(b, d) };
	reg_rect(&g->u.region, &r);
	return (HRGN)g;
}
API(HRGN) CreateRectRgnIndirect(const RECT *r) { return CreateRectRgn(r->left, r->top, r->right, r->bottom); }
API(HRGN) CreateEllipticRgn(i32 a, i32 b, i32 c, i32 d) { return CreateRectRgn(a, b, c, d); }
API(HRGN) CreateEllipticRgnIndirect(const RECT *r) { return CreateRectRgnIndirect(r); }
API(HRGN) CreateRoundRectRgn(i32 a, i32 b, i32 c, i32 d, i32 e, i32 f) { (void)e; (void)f; return CreateRectRgn(a, b, c, d); }
API(HRGN) CreatePolygonRgn(const POINT *p, i32 n, i32 m)
{
	RECT r = { 0, 0, 0, 0 };
	i32 k;
	(void)m;
	if (n > 0) {
		r.left = r.right = p[0].x; r.top = r.bottom = p[0].y;
	}
	for (k = 1; k < n; k++) {
		r.left = MIN(r.left, p[k].x); r.right = MAX(r.right, p[k].x);
		r.top = MIN(r.top, p[k].y); r.bottom = MAX(r.bottom, p[k].y);
	}
	return CreateRectRgnIndirect(&r);
}
API(BOOL) SetRectRgn(HRGN h, i32 a, i32 b, i32 c, i32 d)
{
	Gdi *g = gdi(h);
	RECT r = { MIN(a, c), MIN(b, d), MAX(a, c), MAX(b, d) };
	if (!g || g->tipo != G_REGION)
		return FALSE;
	reg_rect(&g->u.region, &r);
	return TRUE;
}
static i32 clase_reg(const Region *g) { return g->n == 0 ? 1 : g->n == 1 ? 2 : 3; }
API(i32) CombineRgn(HRGN d, HRGN a, HRGN b, i32 modo)
{
	Gdi *gd = gdi(d), *ga = gdi(a), *gb = gdi(b);
	Region r;
	if (!gd || !ga)
		return 0;
	r = ga->u.region;
	switch (modo) {
	case 1: if (gb) reg_corta_reg(&r, &gb->u.region); break;       /* RGN_AND */
	case 2: if (gb) reg_une(&r, &gb->u.region); break;             /* RGN_OR */
	case 3: if (gb) { Region x = gb->u.region; int i; Region ab = r; reg_corta_reg(&ab, &gb->u.region); reg_une(&r, &x); for (i = 0; i < ab.n; i++) reg_quita(&r, &ab.r[i]); } break;  /* XOR */
	case 4: if (gb) { int i; for (i = 0; i < gb->u.region.n; i++) reg_quita(&r, &gb->u.region.r[i]); } break;  /* DIFF */
	case 5: break;                                                    /* COPY */
	}
	gd->u.region = r;
	return clase_reg(&r);
}
API(i32) OffsetRgn(HRGN h, i32 x, i32 y) { Gdi *g = gdi(h); if (!g) return 0; reg_mueve(&g->u.region, x, y); return clase_reg(&g->u.region); }
API(i32) GetRgnBox(HRGN h, RECT *r) { Gdi *g = gdi(h); if (!g) return 0; reg_caja(&g->u.region, r); return clase_reg(&g->u.region); }
API(BOOL) PtInRegion(HRGN h, i32 x, i32 y) { Gdi *g = gdi(h); return g && en_clip(&g->u.region, x, y); }
API(BOOL) RectInRegion(HRGN h, const RECT *r)
{
	Gdi *g = gdi(h);
	int i;
	if (!g)
		return FALSE;
	for (i = 0; i < g->u.region.n; i++) {
		RECT a = *r;
		if (corta(&a, &g->u.region.r[i]))
			return TRUE;
	}
	return FALSE;
}
API(BOOL) EqualRgn(HRGN a, HRGN b) { RECT x, y; GetRgnBox(a, &x); GetRgnBox(b, &y); return !memcmp(&x, &y, sizeof x); }
API(HPALETTE) CreatePalette(const u16 *lp)
{
	Gdi *g = nuevo_gdi(G_PALETA);
	u32 k, n = lp[1];
	const u8 *e = (const u8 *)(lp + 2);
	if (n > 256)
		n = 256;
	g->u.paleta.n = n;
	for (k = 0; k < n; k++)
		g->u.paleta.e[k] = (e[k * 4] << 16) | (e[k * 4 + 1] << 8) | e[k * 4 + 2];
	return (HPALETTE)g;
}
API(HPALETTE) CreateHalftonePalette(HDC h)
{
	u16 lp[2 + 512];
	u8 *e = (u8 *)(lp + 2);
	int k;
	(void)h;
	lp[0] = 0x300; lp[1] = 216;
	for (k = 0; k < 216; k++) {
		e[k * 4] = (k / 36) * 51; e[k * 4 + 1] = (k / 6 % 6) * 51; e[k * 4 + 2] = (k % 6) * 51; e[k * 4 + 3] = 0;
	}
	return CreatePalette(lp);
}
API(HPALETTE) SelectPalette(HDC h, HPALETTE p, BOOL fondo)
{
	Dc *d = dc(h);
	Gdi *a;
	(void)fondo;
	if (!d)
		return 0;
	a = d->paleta;
	d->paleta = gdi(p);
	return a ? (HPALETTE)a : GetStockObject(15);
}
API(u32) RealizePalette(HDC h) { (void)h; return 0; }
API(u32) GetPaletteEntries(HPALETTE h, u32 a, u32 n, u8 *e)
{
	Gdi *g = gdi(h);
	u32 k;
	if (!g || g->tipo != G_PALETA)
		return 0;
	if (!e)
		return g->u.paleta.n;
	for (k = 0; k < n && a + k < g->u.paleta.n; k++) {
		u32 v = g->u.paleta.e[a + k];
		e[k * 4] = v >> 16; e[k * 4 + 1] = v >> 8; e[k * 4 + 2] = v; e[k * 4 + 3] = 0;
	}
	return k;
}
API(u32) SetPaletteEntries(HPALETTE h, u32 a, u32 n, const u8 *e)
{
	Gdi *g = gdi(h);
	u32 k;
	if (!g || g->tipo != G_PALETA)
		return 0;
	for (k = 0; k < n && a + k < 256; k++)
		g->u.paleta.e[a + k] = (e[k * 4] << 16) | (e[k * 4 + 1] << 8) | e[k * 4 + 2];
	if (a + k > g->u.paleta.n)
		g->u.paleta.n = a + k;
	return k;
}
API(BOOL) AnimatePalette(HPALETTE h, u32 a, u32 n, const u8 *e) { return SetPaletteEntries(h, a, n, e) != 0; }
API(BOOL) ResizePalette(HPALETTE h, u32 n) { Gdi *g = gdi(h); if (g && n <= 256) g->u.paleta.n = n; return TRUE; }
API(u32) GetNearestPaletteIndex(HPALETTE h, COLORREF c)
{
	Gdi *g = gdi(h);
	Sup s;
	if (!g)
		return 0;
	s.npal = g->u.paleta.n;
	memcpy(s.pal, g->u.paleta.e, 4 * s.npal);
	s.bpp = 8;
	return cerca(&s, rgb(c));
}
API(u32) GetSystemPaletteEntries(HDC h, u32 a, u32 n, u8 *e)
{
	u32 k;
	(void)h;
	if (!e)
		return 256;
	for (k = 0; k < n && a + k < 256; k++) {
		u32 i = a + k, v = i < 16 ? rgb(0x01000000 | i) : ((i & 0xE0) << 16) | ((i & 0x1C) << 11) | ((i & 3) << 6);
		e[k * 4] = v >> 16; e[k * 4 + 1] = v >> 8; e[k * 4 + 2] = v; e[k * 4 + 3] = 0;
	}
	return k;
}
API(u32) GetSystemPaletteUse(HDC h) { (void)h; return 1; }
API(u32) SetSystemPaletteUse(HDC h, u32 u) { (void)h; (void)u; return 1; }
API(COLORREF) GetNearestColor(HDC h, COLORREF c) { (void)h; return c & 0xFFFFFF; }
API(BOOL) UnrealizeObject(HGDIOBJ h) { (void)h; return TRUE; }

/* ---- seleccionar, borrar, preguntar ---- */
static void recorte_de(Dc *d);
API(HGDIOBJ) SelectObject(HDC h, HGDIOBJ o)
{
	Dc *d = dc(h);
	Gdi *g = gdi(o), *a = 0;
	if (!d || !g)
		return 0;
	switch (g->tipo) {
	case G_PLUMA: a = d->pluma; d->pluma = g; break;
	case G_BROCHA: a = d->brocha; d->brocha = g; break;
	case G_LETRA: a = d->letra; d->letra = g; break;
	case G_MAPA:
		if (!d->memoria)
			return 0;
		a = d->mapa;
		if (a)
			a->u.mapa.seleccionado = 0;
		d->mapa = g;
		g->u.mapa.seleccionado = 1;
		d->s = &g->u.mapa.s;
		d->tope.left = d->tope.top = 0;
		d->tope.right = d->s->an;
		d->tope.bottom = d->s->al;
		reg_rect(&d->clip, &d->tope);
		break;
	case G_REGION: {
		extern i32 WINAPI SelectClipRgn(HDC, HRGN);
		return (HGDIOBJ)SelectClipRgn(h, o);
	}
	case G_PALETA: a = d->paleta; d->paleta = g; break;
	}
	return (HGDIOBJ)a;
}
API(BOOL) DeleteObject(HGDIOBJ o)
{
	Gdi *g = gdi(o);
	if (!g || g->stock)
		return g != 0;
	if (g->tipo == G_MAPA) {
		if (g->u.mapa.seleccionado)
			return FALSE;
		m_soltar(g->u.mapa.propio);
	}
	g->magia = 0;
	m_soltar(g);
	return TRUE;
}
API(HGDIOBJ) GetCurrentObject(HDC h, u32 t)
{
	Dc *d = dc(h);
	if (!d)
		return 0;
	switch (t) {
	case 1: return (HGDIOBJ)d->pluma;
	case 2: return (HGDIOBJ)d->brocha;
	case 5: return (HGDIOBJ)d->paleta;
	case 6: return (HGDIOBJ)d->letra;
	case 7: return (HGDIOBJ)d->mapa;
	}
	return 0;
}
API(u32) GetObjectType(HGDIOBJ h)
{
	Gdi *g = gdi(h);
	static const u8 t[] = { 0, 1, 2, 6, 7, 8, 5, 3 };
	if (dc(h))
		return dc(h)->memoria ? 10 : 3;
	return g ? t[g->tipo] : 0;
}
API(i32) GetObjectA(HGDIOBJ h, i32 n, void *p)
{
	Gdi *g = gdi(h);
	if (!g)
		return 0;
	switch (g->tipo) {
	case G_PLUMA: {
		u32 v[4] = { g->u.pluma.estilo, g->u.pluma.ancho, 0, g->u.pluma.color };
		if (!p)
			return 16;
		memcpy(p, v, MIN(n, 16));
		return MIN(n, 16);
	}
	case G_BROCHA: {
		u32 v[3] = { g->u.brocha.estilo, g->u.brocha.color, g->u.brocha.trama };
		if (!p)
			return 12;
		memcpy(p, v, MIN(n, 12));
		return MIN(n, 12);
	}
	case G_LETRA: {
		LOGFONTA l;
		memset(&l, 0, sizeof l);
		l.alto = g->u.letra.alto; l.ancho = g->u.letra.ancho; l.peso = g->u.letra.peso;
		l.cursiva = g->u.letra.cursiva; l.subraya = g->u.letra.subraya;
		l.paso_familia = g->u.letra.fija ? 0x31 : 0x22;
		copia_n(l.cara, g->u.letra.cara, 32);
		if (!p)
			return sizeof l;
		memcpy(p, &l, MIN(n, (i32)sizeof l));
		return MIN(n, (i32)sizeof l);
	}
	case G_MAPA: {
		Sup *s = &g->u.mapa.s;
		u32 v[6];
		i32 bpp = s->mono ? 1 : s->bpp;
		v[0] = 0; v[1] = s->an; v[2] = s->al;
		v[3] = ((s->an * bpp + 15) / 16) * 2;
		if (g->u.mapa.seccion)
			v[3] = ((s->an * bpp + 31) / 32) * 4;
		((u16 *)v)[8] = 1; ((u16 *)v)[9] = bpp;
		v[5] = g->u.mapa.seccion ? (u32)g->u.mapa.propio : 0;
		if (!p)
			return g->u.mapa.seccion ? 84 : 24;
		if (n >= 84 && g->u.mapa.seccion) {
			u32 d[21];
			memset(d, 0, sizeof d);
			memcpy(d, v, 24);
			d[6] = 40; d[7] = s->an; d[8] = s->de_abajo ? s->al : -s->al;
			((u16 *)d)[18] = 1; ((u16 *)d)[19] = s->bpp;
			d[10] = s->bpp == 16 || (s->bpp == 32 && s->mr) ? 3 : 0;
			d[11] = (s->paso < 0 ? -s->paso : s->paso) * s->al;
			d[16] = s->mr; d[17] = s->mg; d[18] = s->mb;
			memcpy(p, d, 84);
			return 84;
		}
		memcpy(p, v, MIN(n, 24));
		return MIN(n, 24);
	}
	case G_PALETA:
		if (p && n >= 2)
			*(u16 *)p = g->u.paleta.n;
		return 2;
	}
	return 0;
}
API(i32) GetObjectW(HGDIOBJ h, i32 n, void *p) { return GetObjectA(h, n, p); }

/* ---------------------------------------------------------------------------
 *  Los DC
 * ------------------------------------------------------------------------- */
static void dc_inicial(Dc *d)
{
	prepara_stock();
	d->magia = MAGIA_DC;
	d->pluma = &stock[7];
	d->brocha = &stock[0];
	d->letra = &stock[13];
	d->paleta = &stock[15];
	d->tinta = 0;
	d->fondo = 0xFFFFFF;
	d->modo_fondo = 2;
	d->rop2 = 13;
	d->estirar = 1;
	d->mapeo = 1;
}
/* el recorte de un DC de ventana: lo que se ve de ella (y sin sus hijas, si hace falta) */
HDC dc_de_ventana(Wnd *w, int dentro, int clip_hijas)
{
	Dc *d = m_cero(sizeof *d);
	Wnd *r = raiz_de(w);
	POINT o;
	RECT v;
	dc_inicial(d);
	d->w = w;
	if (!r || !r->lienzo.bits || !ventana_visible(w)) {
		d->s = 0;
		return (HDC)d;
	}
	d->s = &r->lienzo;
	if (dentro)
		dentro_en_lienzo(w, &o);
	else
		ventana_en_lienzo(w, &o);
	d->origen_x = o.x;
	d->origen_y = o.y;
	visible_en_lienzo(w, &v, dentro);
	d->tope = v;
	reg_rect(&d->clip, &v);
	if (clip_hijas || (w->estilo & WS_CLIPCHILDREN)) {
		Wnd *h;
		for (h = w->hijas; h; h = h->sig)
			if ((h->estilo & WS_VISIBLE) && !h->destruida) {
				RECT hr;
				POINT ho;
				ventana_en_lienzo(h, &ho);
				hr.left = ho.x; hr.top = ho.y;
				hr.right = ho.x + h->r.right - h->r.left;
				hr.bottom = ho.y + h->r.bottom - h->r.top;
				reg_quita(&d->clip, &hr);
			}
	}
	if (w->clase && w->clase->fondo == 0 && 0) {
	}
	return (HDC)d;
}
void dc_soltar(HDC h)
{
	Dc *d = dc(h);
	if (!d)
		return;
	while (d->guardado) {
		Dc *g = d->guardado;
		d->guardado = g->guardado;
		m_soltar(g);
	}
	d->magia = 0;
	m_soltar(d);
}
API(HDC) CreateCompatibleDC(HDC h)
{
	Dc *d = m_cero(sizeof *d);
	(void)h;
	dc_inicial(d);
	d->memoria = 1;
	d->mapa = &mapa_mono;
	d->s = &mapa_mono.u.mapa.s;
	d->tope.right = d->tope.bottom = 1;
	reg_rect(&d->clip, &d->tope);
	return (HDC)d;
}
API(HDC) CreateDCA(const char *dr, const char *dev, const char *o, const void *m)
{
	(void)dr; (void)dev; (void)o; (void)m;
	return dc_de_ventana(escritorio, 1, 0);
}
API(HDC) CreateICA(const char *dr, const char *dev, const char *o, const void *m) { return CreateDCA(dr, dev, o, m); }
API(BOOL) DeleteDC(HDC h)
{
	Dc *d = dc(h);
	if (!d)
		return FALSE;
	if (d->mapa)
		d->mapa->u.mapa.seleccionado = 0;
	dc_soltar(h);
	return TRUE;
}
API(i32) SaveDC(HDC h)
{
	Dc *d = dc(h), *g;
	if (!d)
		return 0;
	g = m_pedir(sizeof *g);
	*g = *d;
	g->magia = 0;
	d->guardado = g;
	return ++d->nivel;
}
API(BOOL) RestoreDC(HDC h, i32 n)
{
	Dc *d = dc(h);
	if (!d || !d->guardado)
		return FALSE;
	if (n < 0)
		n = d->nivel + n + 1;
	while (d->guardado && d->nivel >= n) {
		Dc *g = d->guardado;
		Dc *sig = g->guardado;
		i32 nivel = d->nivel - 1;
		Gdi *mapa_ahora = d->mapa;
		*d = *g;
		d->magia = MAGIA_DC;
		d->guardado = sig;
		d->nivel = nivel;
		if (mapa_ahora != d->mapa && mapa_ahora)
			mapa_ahora->u.mapa.seleccionado = 0;
		m_soltar(g);
	}
	return TRUE;
}

/* las coordenadas lógicas -> la superficie */
static inline i32 sx(Dc *d, i32 x) { return x - d->ve.x + d->vp.x + d->origen_x; }
static inline i32 sy(Dc *d, i32 y) { return y - d->ve.y + d->vp.y + d->origen_y; }

/* el recorte de verdad (con el del programa) */
static Region clip_tmp;
static const Region *clip_ef(Dc *d)
{
	if (!d->hay_clip_app)
		return &d->clip;
	clip_tmp = d->clip;
	{
		Region a = d->clip_app;
		reg_mueve(&a, d->origen_x + d->vp.x - d->ve.x, d->origen_y + d->vp.y - d->ve.y);
		reg_corta_reg(&clip_tmp, &a);
	}
	return &clip_tmp;
}
static void recorte_de(Dc *d) { (void)d; }

API(i32) SelectClipRgn(HDC h, HRGN r)
{
	Dc *d = dc(h);
	Gdi *g = gdi(r);
	if (!d)
		return 0;
	if (!g) {
		d->hay_clip_app = 0;
		return 2;
	}
	d->hay_clip_app = 1;
	d->clip_app = g->u.region;
	return clase_reg(&d->clip_app);
}
API(i32) ExtSelectClipRgn(HDC h, HRGN r, i32 modo)
{
	Dc *d = dc(h);
	Gdi *g = gdi(r);
	if (!d)
		return 0;
	if (modo == 5 || !d->hay_clip_app)
		return SelectClipRgn(h, r);
	if (g) {
		if (modo == 1)
			reg_corta_reg(&d->clip_app, &g->u.region);
		else if (modo == 2)
			reg_une(&d->clip_app, &g->u.region);
		else if (modo == 4) {
			int i;
			for (i = 0; i < g->u.region.n; i++)
				reg_quita(&d->clip_app, &g->u.region.r[i]);
		}
	}
	return clase_reg(&d->clip_app);
}
static void clip_app_inicial(Dc *d)
{
	if (!d->hay_clip_app) {
		RECT r;
		r.left = d->tope.left - d->origen_x - d->vp.x + d->ve.x;
		r.top = d->tope.top - d->origen_y - d->vp.y + d->ve.y;
		r.right = d->tope.right - d->origen_x - d->vp.x + d->ve.x;
		r.bottom = d->tope.bottom - d->origen_y - d->vp.y + d->ve.y;
		reg_rect(&d->clip_app, &r);
		d->hay_clip_app = 1;
	}
}
API(i32) IntersectClipRect(HDC h, i32 a, i32 b, i32 c, i32 e)
{
	Dc *d = dc(h);
	RECT r = { a, b, c, e };
	if (!d)
		return 0;
	clip_app_inicial(d);
	reg_corta(&d->clip_app, &r);
	return clase_reg(&d->clip_app);
}
API(i32) ExcludeClipRect(HDC h, i32 a, i32 b, i32 c, i32 e)
{
	Dc *d = dc(h);
	RECT r = { a, b, c, e };
	if (!d)
		return 0;
	clip_app_inicial(d);
	reg_quita(&d->clip_app, &r);
	return clase_reg(&d->clip_app);
}
API(i32) OffsetClipRgn(HDC h, i32 x, i32 y) { Dc *d = dc(h); if (d && d->hay_clip_app) reg_mueve(&d->clip_app, x, y); return 2; }
API(i32) GetClipBox(HDC h, RECT *r)
{
	Dc *d = dc(h);
	const Region *c;
	if (!d)
		return 0;
	c = clip_ef(d);
	reg_caja(c, r);
	r->left -= d->origen_x + d->vp.x - d->ve.x; r->right -= d->origen_x + d->vp.x - d->ve.x;
	r->top -= d->origen_y + d->vp.y - d->ve.y; r->bottom -= d->origen_y + d->vp.y - d->ve.y;
	return clase_reg(c);
}
API(i32) GetClipRgn(HDC h, HRGN r)
{
	Dc *d = dc(h);
	Gdi *g = gdi(r);
	if (!d || !g)
		return -1;
	if (!d->hay_clip_app)
		return 0;
	g->u.region = d->clip_app;
	return 1;
}
API(BOOL) PtVisible(HDC h, i32 x, i32 y) { Dc *d = dc(h); return d && en_clip(clip_ef(d), sx(d, x), sy(d, y)); }
API(BOOL) RectVisible(HDC h, const RECT *r)
{
	Dc *d = dc(h);
	const Region *c;
	int i;
	if (!d)
		return FALSE;
	c = clip_ef(d);
	for (i = 0; i < c->n; i++) {
		RECT a = { sx(d, r->left), sy(d, r->top), sx(d, r->right), sy(d, r->bottom) };
		if (corta(&a, &c->r[i]))
			return TRUE;
	}
	return FALSE;
}

/* ---- el estado del DC ---- */
API(COLORREF) SetTextColor(HDC h, COLORREF c) { Dc *d = dc(h); COLORREF a; if (!d) return 0xFFFFFFFF; a = d->tinta; d->tinta = c; return a; }
API(COLORREF) GetTextColor(HDC h) { Dc *d = dc(h); return d ? d->tinta : 0; }
API(COLORREF) SetBkColor(HDC h, COLORREF c) { Dc *d = dc(h); COLORREF a; if (!d) return 0xFFFFFFFF; a = d->fondo; d->fondo = c; return a; }
API(COLORREF) GetBkColor(HDC h) { Dc *d = dc(h); return d ? d->fondo : 0; }
API(i32) SetBkMode(HDC h, i32 m) { Dc *d = dc(h); i32 a; if (!d) return 0; a = d->modo_fondo; d->modo_fondo = m; return a; }
API(i32) GetBkMode(HDC h) { Dc *d = dc(h); return d ? d->modo_fondo : 0; }
API(i32) SetROP2(HDC h, i32 r) { Dc *d = dc(h); i32 a; if (!d) return 0; a = d->rop2; d->rop2 = r; return a; }
API(i32) GetROP2(HDC h) { Dc *d = dc(h); return d ? d->rop2 : 0; }
API(i32) SetStretchBltMode(HDC h, i32 m) { Dc *d = dc(h); i32 a; if (!d) return 0; a = d->estirar; d->estirar = m; return a; }
API(i32) GetStretchBltMode(HDC h) { Dc *d = dc(h); return d ? d->estirar : 0; }
API(u32) SetTextAlign(HDC h, u32 a) { Dc *d = dc(h); u32 v; if (!d) return 0xFFFFFFFF; v = d->alinea; d->alinea = a; return v; }
API(u32) GetTextAlign(HDC h) { Dc *d = dc(h); return d ? d->alinea : 0; }
API(i32) SetMapMode(HDC h, i32 m) { Dc *d = dc(h); i32 a; if (!d) return 0; a = d->mapeo; d->mapeo = m; return a; }
API(i32) GetMapMode(HDC h) { Dc *d = dc(h); return d ? d->mapeo : 0; }
API(BOOL) SetViewportOrgEx(HDC h, i32 x, i32 y, POINT *p) { Dc *d = dc(h); if (!d) return FALSE; if (p) *p = d->vp; d->vp.x = x; d->vp.y = y; return TRUE; }
API(BOOL) GetViewportOrgEx(HDC h, POINT *p) { Dc *d = dc(h); if (!d) return FALSE; *p = d->vp; return TRUE; }
API(BOOL) OffsetViewportOrgEx(HDC h, i32 x, i32 y, POINT *p) { Dc *d = dc(h); if (!d) return FALSE; if (p) *p = d->vp; d->vp.x += x; d->vp.y += y; return TRUE; }
API(BOOL) SetWindowOrgEx(HDC h, i32 x, i32 y, POINT *p) { Dc *d = dc(h); if (!d) return FALSE; if (p) *p = d->ve; d->ve.x = x; d->ve.y = y; return TRUE; }
API(BOOL) GetWindowOrgEx(HDC h, POINT *p) { Dc *d = dc(h); if (!d) return FALSE; *p = d->ve; return TRUE; }
API(BOOL) OffsetWindowOrgEx(HDC h, i32 x, i32 y, POINT *p) { Dc *d = dc(h); if (!d) return FALSE; if (p) *p = d->ve; d->ve.x += x; d->ve.y += y; return TRUE; }
API(BOOL) SetWindowExtEx(HDC h, i32 x, i32 y, SIZE *s) { (void)h; (void)x; (void)y; if (s) { s->cx = 1; s->cy = 1; } return TRUE; }
API(BOOL) SetViewportExtEx(HDC h, i32 x, i32 y, SIZE *s) { (void)h; (void)x; (void)y; if (s) { s->cx = 1; s->cy = 1; } return TRUE; }
API(BOOL) GetWindowExtEx(HDC h, SIZE *s) { (void)h; s->cx = s->cy = 1; return TRUE; }
API(BOOL) GetViewportExtEx(HDC h, SIZE *s) { (void)h; s->cx = s->cy = 1; return TRUE; }
API(BOOL) SetBrushOrgEx(HDC h, i32 x, i32 y, POINT *p) { Dc *d = dc(h); if (!d) return FALSE; if (p) *p = d->bo; d->bo.x = x; d->bo.y = y; return TRUE; }
API(BOOL) GetBrushOrgEx(HDC h, POINT *p) { Dc *d = dc(h); if (!d) return FALSE; *p = d->bo; return TRUE; }
API(BOOL) LPtoDP(HDC h, POINT *p, i32 n) { Dc *d = dc(h); i32 k; if (!d) return FALSE; for (k = 0; k < n; k++) { p[k].x += d->vp.x - d->ve.x; p[k].y += d->vp.y - d->ve.y; } return TRUE; }
API(BOOL) DPtoLP(HDC h, POINT *p, i32 n) { Dc *d = dc(h); i32 k; if (!d) return FALSE; for (k = 0; k < n; k++) { p[k].x -= d->vp.x - d->ve.x; p[k].y -= d->vp.y - d->ve.y; } return TRUE; }
API(BOOL) GetCurrentPositionEx(HDC h, POINT *p) { Dc *d = dc(h); if (!d) return FALSE; *p = d->pos; return TRUE; }
API(BOOL) GdiFlush(void) { presentar_todo(); return TRUE; }
API(u32) GdiSetBatchLimit(u32 n) { (void)n; return 1; }
API(i32) SetGraphicsMode(HDC h, i32 m) { (void)h; (void)m; return 1; }
API(u32) SetLayout(HDC h, u32 l) { (void)h; (void)l; return 0; }
API(i32) GetDeviceCaps(HDC h, i32 i)
{
	Dc *d = dc(h);
	switch (i) {
	case 0: return 0x400;            /* DRIVERVERSION */
	case 2: return 1;                /* TECHNOLOGY: DT_RASDISPLAY */
	case 4: return pantalla_an * 254 / 960;   /* HORZSIZE (mm) */
	case 6: return pantalla_al * 254 / 960;
	case 8: return d && d->memoria && d->s ? d->s->an : pantalla_an;    /* HORZRES */
	case 10: return d && d->memoria && d->s ? d->s->al : pantalla_al;
	case 12: return d && d->memoria && d->s && d->s->mono ? 1 : 32;      /* BITSPIXEL */
	case 14: return 1;               /* PLANES */
	case 16: return -1;
	case 18: return 4;
	case 20: return 8;
	case 22: return 100;
	case 24: return -1;              /* NUMCOLORS */
	case 26: return 48;
	case 28: return 36;              /* CURVECAPS... */
	case 30: case 32: case 34: return 0xFF;
	case 36: return 0x7F;            /* CLIPCAPS */
	case 38: return 0x7E99;          /* RASTERCAPS: BITBLT, BITMAP64, DI_BITMAP, DIBTODEV, STRETCHBLT, STRETCHDIB */
	case 40: return 36;              /* ASPECTX */
	case 42: return 36;
	case 44: return 51;
	case 88: case 90: return 96;     /* LOGPIXELSX/Y */
	case 104: return 0;              /* SIZEPALETTE */
	case 106: return 20;
	case 108: return 24;             /* COLORRES */
	case 116: return 60;             /* VREFRESH */
	case 117: return pantalla_an;    /* DESKTOPVERTRES... */
	case 118: return pantalla_al;
	}
	return 0;
}

/* ---------------------------------------------------------------------------
 *  Dibujar
 * ------------------------------------------------------------------------- */
/* un punto con la pluma (con ROP2) */
static void punto_pluma(Dc *d, const Region *c, i32 x, i32 y, u32 col)
{
	if (!en_clip(c, x, y))
		return;
	if (x < 0 || y < 0 || x >= d->s->an || y >= d->s->al)
		return;
	if (d->rop2 == 13)
		poner_rgb(d->s, x, y, col);
	else
		poner_rgb(d->s, x, y, rop2(d->rop2, col, leer_rgb(d->s, x, y)) & 0xFFFFFF);
}
static void linea_s(Dc *d, i32 x0, i32 y0, i32 x1, i32 y1, int ultimo)
{
	Gdi *p = d->pluma;
	const Region *c;
	i32 dx, dy, sxp, syp, e, n = 0, an;
	u32 col;
	static const u8 tramas[5] = { 0xFF, 0xFC, 0xAA, 0xE4, 0xEA };
	if (!d->s || !p || p->u.pluma.estilo == 5)
		return;
	c = clip_ef(d);
	col = rgb(p->u.pluma.color);
	an = p->u.pluma.ancho;
	dx = x1 > x0 ? x1 - x0 : x0 - x1;
	dy = y1 > y0 ? y1 - y0 : y0 - y1;
	sxp = x0 < x1 ? 1 : -1;
	syp = y0 < y1 ? 1 : -1;
	e = dx - dy;
	for (;;) {
		if (x0 == x1 && y0 == y1 && !ultimo)
			break;
		if (p->u.pluma.estilo == 0 || (tramas[p->u.pluma.estilo < 5 ? p->u.pluma.estilo : 0] >> (7 - (n >> 1 & 7))) & 1) {
			if (an <= 1)
				punto_pluma(d, c, x0, y0, col);
			else {
				i32 a, b;
				for (a = -an / 2; a < an - an / 2; a++)
					for (b = -an / 2; b < an - an / 2; b++)
						punto_pluma(d, c, x0 + a, y0 + b, col);
			}
		}
		n++;
		if (x0 == x1 && y0 == y1)
			break;
		{
			i32 e2 = 2 * e;
			if (e2 > -dy) { e -= dy; x0 += sxp; }
			if (e2 < dx) { e += dx; y0 += syp; }
		}
	}
}
API(BOOL) MoveToEx(HDC h, i32 x, i32 y, POINT *p)
{
	Dc *d = dc(h);
	if (!d)
		return FALSE;
	if (p)
		*p = d->pos;
	d->pos.x = x;
	d->pos.y = y;
	return TRUE;
}
API(BOOL) LineTo(HDC h, i32 x, i32 y)
{
	Dc *d = dc(h);
	if (!d)
		return FALSE;
	linea_s(d, sx(d, d->pos.x), sy(d, d->pos.y), sx(d, x), sy(d, y), 0);
	d->pos.x = x;
	d->pos.y = y;
	ensuciar(raiz_de(d->w), &d->tope);
	return TRUE;
}
API(BOOL) Polyline(HDC h, const POINT *p, i32 n)
{
	Dc *d = dc(h);
	i32 k;
	if (!d || n < 2)
		return FALSE;
	for (k = 1; k < n; k++)
		linea_s(d, sx(d, p[k - 1].x), sy(d, p[k - 1].y), sx(d, p[k].x), sy(d, p[k].y), 0);
	ensuciar(raiz_de(d->w), &d->tope);
	return TRUE;
}
API(BOOL) PolylineTo(HDC h, const POINT *p, u32 n)
{
	Dc *d = dc(h);
	u32 k;
	if (!d)
		return FALSE;
	for (k = 0; k < n; k++)
		LineTo(h, p[k].x, p[k].y);
	return TRUE;
}
API(BOOL) PolyPolyline(HDC h, const POINT *p, const u32 *n, u32 c)
{
	u32 k;
	for (k = 0; k < c; k++) {
		Polyline(h, p, n[k]);
		p += n[k];
	}
	return TRUE;
}
/* rellenar con la brocha (en la superficie) */
static void rellenar_brocha(Dc *d, const Region *c, RECT r, Gdi *b)
{
	if (!b || b->u.brocha.estilo == 1 || !d->s)
		return;
	if (b->u.brocha.estilo == 3 && gdi(b->u.brocha.mapa)) {
		/* una trama de un mapa de bits */
		Sup *m = &b->u.brocha.mapa->u.mapa.s;
		i32 x, y;
		RECT tope = { 0, 0, d->s->an, d->s->al };
		if (!corta(&r, &tope))
			return;
		for (y = r.top; y < r.bottom; y++)
			for (x = r.left; x < r.right; x++)
				if (en_clip(c, x, y)) {
					u32 v = leer_rgb(m, ((x - d->bo.x) % m->an + m->an) % m->an, ((y - d->bo.y) % m->al + m->al) % m->al);
					if (m->mono)
						v = v ? rgb(d->fondo) : rgb(d->tinta);
					poner_rgb(d->s, x, y, v);
				}
		return;
	}
	if (d->rop2 == 13)
		rellenar(d->s, c, r, rgb(b->u.brocha.color));
	else {
		i32 x, y;
		u32 col = rgb(b->u.brocha.color);
		RECT tope = { 0, 0, d->s->an, d->s->al };
		if (!corta(&r, &tope))
			return;
		for (y = r.top; y < r.bottom; y++)
			for (x = r.left; x < r.right; x++)
				if (en_clip(c, x, y))
					poner_rgb(d->s, x, y, rop2(d->rop2, col, leer_rgb(d->s, x, y)) & 0xFFFFFF);
	}
}
API(BOOL) Rectangle(HDC h, i32 a, i32 b, i32 c, i32 e)
{
	Dc *d = dc(h);
	RECT r;
	i32 x0, y0, x1, y1;
	if (!d)
		return FALSE;
	x0 = sx(d, MIN(a, c)); y0 = sy(d, MIN(b, e)); x1 = sx(d, MAX(a, c)); y1 = sy(d, MAX(b, e));
	r.left = x0 + 1; r.top = y0 + 1; r.right = x1 - 1; r.bottom = y1 - 1;
	if (d->pluma && d->pluma->u.pluma.estilo == 5) {
		r.left--; r.top--;
	}
	rellenar_brocha(d, clip_ef(d), r, d->brocha);
	if (d->pluma && d->pluma->u.pluma.estilo != 5) {
		linea_s(d, x0, y0, x1 - 1, y0, 1);
		linea_s(d, x1 - 1, y0, x1 - 1, y1 - 1, 1);
		linea_s(d, x1 - 1, y1 - 1, x0, y1 - 1, 1);
		linea_s(d, x0, y1 - 1, x0, y0, 1);
	}
	ensuciar(raiz_de(d->w), &d->tope);
	return TRUE;
}
/* una elipse metida en el rectángulo (con relleno y borde) */
static void elipse_s(Dc *d, i32 x0, i32 y0, i32 x1, i32 y1, int rellena, int borde)
{
	i32 a = (x1 - x0) / 2, b = (y1 - y0) / 2, cx = x0 + a, cy = y0 + b, y;
	const Region *c = clip_ef(d);
	i32 ultx = a;
	if (a <= 0 || b <= 0)
		return;
	for (y = -b; y <= b; y++) {
		/* x = a * sqrt(1 - y²/b²) */
		i64 t = (i64)a * a * ((i64)b * b - (i64)y * y);
		i32 x = 0, bb = b * b;
		u32 lo = 0, hi = (u32)a + 1;
		while (lo < hi) {
			u32 m = (lo + hi + 1) / 2;
			if ((i64)m * m * bb <= t) lo = m; else hi = m - 1;
		}
		x = lo;
		if (rellena) {
			RECT r = { cx - x, cy + y, cx + x + 1, cy + y + 1 };
			rellenar_brocha(d, c, r, d->brocha);
		}
		if (borde && d->pluma && d->pluma->u.pluma.estilo != 5) {
			u32 col = rgb(d->pluma->u.pluma.color);
			i32 k;
			i32 desde = y == -b || y == b ? 0 : MIN(x, ultx);
			for (k = desde; k <= x; k++) {
				punto_pluma(d, c, cx - k, cy + y, col);
				punto_pluma(d, c, cx + k, cy + y, col);
			}
			if (y <= 0)
				ultx = x;
		}
		ultx = x;
	}
}
API(BOOL) Ellipse(HDC h, i32 a, i32 b, i32 c, i32 e)
{
	Dc *d = dc(h);
	if (!d)
		return FALSE;
	elipse_s(d, sx(d, MIN(a, c)), sy(d, MIN(b, e)), sx(d, MAX(a, c)) - 1, sy(d, MAX(b, e)) - 1, 1, 1);
	ensuciar(raiz_de(d->w), &d->tope);
	return TRUE;
}
API(BOOL) RoundRect(HDC h, i32 a, i32 b, i32 c, i32 e, i32 w, i32 al) { (void)w; (void)al; return Rectangle(h, a, b, c, e); }
API(BOOL) Pie(HDC h, i32 a, i32 b, i32 c, i32 e, i32 x1, i32 y1, i32 x2, i32 y2) { (void)x1; (void)y1; (void)x2; (void)y2; return Ellipse(h, a, b, c, e); }
API(BOOL) Chord(HDC h, i32 a, i32 b, i32 c, i32 e, i32 x1, i32 y1, i32 x2, i32 y2) { (void)x1; (void)y1; (void)x2; (void)y2; return Ellipse(h, a, b, c, e); }
API(BOOL) Arc(HDC h, i32 a, i32 b, i32 c, i32 e, i32 x1, i32 y1, i32 x2, i32 y2)
{
	Dc *d = dc(h);
	(void)x1; (void)y1; (void)x2; (void)y2;
	if (!d)
		return FALSE;
	elipse_s(d, sx(d, MIN(a, c)), sy(d, MIN(b, e)), sx(d, MAX(a, c)) - 1, sy(d, MAX(b, e)) - 1, 0, 1);
	ensuciar(raiz_de(d->w), &d->tope);
	return TRUE;
}
API(BOOL) Polygon(HDC h, const POINT *p, i32 n)
{
	Dc *d = dc(h);
	i32 y, ymin, ymax, k;
	const Region *c;
	if (!d || n < 2 || !d->s)
		return FALSE;
	c = clip_ef(d);
	ymin = ymax = p[0].y;
	for (k = 1; k < n; k++) {
		ymin = MIN(ymin, p[k].y);
		ymax = MAX(ymax, p[k].y);
	}
	/* el relleno por barrido (par-impar) */
	if (d->brocha && d->brocha->u.brocha.estilo != 1)
		for (y = ymin; y < ymax; y++) {
			i32 xs[64], m = 0, i, j;
			for (k = 0; k < n && m < 64; k++) {
				POINT a = p[k], b = p[(k + 1) % n];
				if ((a.y <= y && b.y > y) || (b.y <= y && a.y > y))
					xs[m++] = a.x + (y - a.y) * (b.x - a.x) / (b.y - a.y);
			}
			for (i = 1; i < m; i++)
				for (j = i; j > 0 && xs[j - 1] > xs[j]; j--) {
					i32 t = xs[j]; xs[j] = xs[j - 1]; xs[j - 1] = t;
				}
			for (i = 0; i + 1 < m; i += 2) {
				RECT r = { sx(d, xs[i]), sy(d, y), sx(d, xs[i + 1]), sy(d, y) + 1 };
				rellenar_brocha(d, c, r, d->brocha);
			}
		}
	for (k = 0; k < n; k++)
		linea_s(d, sx(d, p[k].x), sy(d, p[k].y), sx(d, p[(k + 1) % n].x), sy(d, p[(k + 1) % n].y), 0);
	ensuciar(raiz_de(d->w), &d->tope);
	return TRUE;
}
API(BOOL) PolyPolygon(HDC h, const POINT *p, const i32 *n, i32 c)
{
	i32 k;
	for (k = 0; k < c; k++) {
		Polygon(h, p, n[k]);
		p += n[k];
	}
	return TRUE;
}
API(COLORREF) SetPixel(HDC h, i32 x, i32 y, COLORREF c)
{
	Dc *d = dc(h);
	i32 a, b;
	if (!d || !d->s)
		return 0xFFFFFFFF;
	a = sx(d, x); b = sy(d, y);
	if (a < 0 || b < 0 || a >= d->s->an || b >= d->s->al || !en_clip(clip_ef(d), a, b))
		return 0xFFFFFFFF;
	poner_rgb(d->s, a, b, rgb(c));
	ensuciar(raiz_de(d->w), &d->tope);
	return c & 0xFFFFFF;
}
API(BOOL) SetPixelV(HDC h, i32 x, i32 y, COLORREF c) { return SetPixel(h, x, y, c) != 0xFFFFFFFF; }
API(COLORREF) GetPixel(HDC h, i32 x, i32 y)
{
	Dc *d = dc(h);
	i32 a, b;
	if (!d || !d->s)
		return 0xFFFFFFFF;
	a = sx(d, x); b = sy(d, y);
	if (a < 0 || b < 0 || a >= d->s->an || b >= d->s->al)
		return 0xFFFFFFFF;
	return de_rgb(leer_rgb(d->s, a, b));
}
/* == USER32 == */
API(i32) FillRect(HDC h, const RECT *r, HBRUSH hb)
{
	Dc *d = dc(h);
	Gdi *b;
	RECT s;
	if (!d || !r)
		return 0;
	if ((u32)hb <= 31)
		b = gdi(GetSysColorBrush((i32)hb - 1));
	else
		b = gdi(hb);
	if (!b || b->tipo != G_BROCHA)
		return 0;
	s.left = sx(d, r->left); s.top = sy(d, r->top); s.right = sx(d, r->right); s.bottom = sy(d, r->bottom);
	{
		int rop = d->rop2;
		d->rop2 = 13;
		rellenar_brocha(d, clip_ef(d), s, b);
		d->rop2 = rop;
	}
	ensuciar(raiz_de(d->w), &d->tope);
	return 1;
}
API(i32) FrameRect(HDC h, const RECT *r, HBRUSH b)
{
	RECT x;
	x = *r; x.bottom = x.top + 1; FillRect(h, &x, b);
	x = *r; x.top = x.bottom - 1; FillRect(h, &x, b);
	x = *r; x.right = x.left + 1; FillRect(h, &x, b);
	x = *r; x.left = x.right - 1; FillRect(h, &x, b);
	return 1;
}
API(BOOL) InvertRect(HDC h, const RECT *r)
{
	Dc *d = dc(h);
	i32 x, y;
	const Region *c;
	RECT s, tope;
	if (!d || !d->s)
		return FALSE;
	c = clip_ef(d);
	s.left = sx(d, r->left); s.top = sy(d, r->top); s.right = sx(d, r->right); s.bottom = sy(d, r->bottom);
	tope.left = tope.top = 0; tope.right = d->s->an; tope.bottom = d->s->al;
	if (!corta(&s, &tope))
		return TRUE;
	for (y = s.top; y < s.bottom; y++)
		for (x = s.left; x < s.right; x++)
			if (en_clip(c, x, y))
				poner_rgb(d->s, x, y, ~leer_rgb(d->s, x, y) & 0xFFFFFF);
	ensuciar(raiz_de(d->w), &d->tope);
	return TRUE;
}
API(BOOL) DrawFocusRect(HDC h, const RECT *r)
{
	Dc *d = dc(h);
	i32 x, y;
	const Region *c;
	if (!d || !d->s)
		return FALSE;
	c = clip_ef(d);
	for (x = r->left; x < r->right; x += 2) {
		i32 a = sx(d, x), t = sy(d, r->top), b = sy(d, r->bottom - 1);
		if (a >= 0 && a < d->s->an) {
			if (t >= 0 && t < d->s->al && en_clip(c, a, t)) poner_rgb(d->s, a, t, ~leer_rgb(d->s, a, t) & 0xFFFFFF);
			if (b >= 0 && b < d->s->al && b != t && en_clip(c, a, b)) poner_rgb(d->s, a, b, ~leer_rgb(d->s, a, b) & 0xFFFFFF);
		}
	}
	for (y = r->top + 2; y < r->bottom - 1; y += 2) {
		i32 l = sx(d, r->left), dd = sx(d, r->right - 1), b = sy(d, y);
		if (b >= 0 && b < d->s->al) {
			if (l >= 0 && l < d->s->an && en_clip(c, l, b)) poner_rgb(d->s, l, b, ~leer_rgb(d->s, l, b) & 0xFFFFFF);
			if (dd >= 0 && dd < d->s->an && dd != l && en_clip(c, dd, b)) poner_rgb(d->s, dd, b, ~leer_rgb(d->s, dd, b) & 0xFFFFFF);
		}
	}
	ensuciar(raiz_de(d->w), &d->tope);
	return TRUE;
}
/* == GDI32 == */
API(BOOL) FillRgn(HDC h, HRGN r, HBRUSH b)
{
	Gdi *g = gdi(r);
	int i;
	if (!g)
		return FALSE;
	for (i = 0; i < g->u.region.n; i++)
		FillRect(h, &g->u.region.r[i], b);
	return TRUE;
}
API(BOOL) PaintRgn(HDC h, HRGN r) { Dc *d = dc(h); return d && FillRgn(h, r, (HBRUSH)d->brocha); }
API(BOOL) FrameRgn(HDC h, HRGN r, HBRUSH b, i32 an, i32 al)
{
	RECT c;
	(void)an; (void)al;
	GetRgnBox(r, &c);
	return FrameRect(h, &c, b);
}
API(BOOL) InvertRgn(HDC h, HRGN r)
{
	Gdi *g = gdi(r);
	int i;
	if (!g)
		return FALSE;
	for (i = 0; i < g->u.region.n; i++)
		InvertRect(h, &g->u.region.r[i]);
	return TRUE;
}
API(BOOL) PatBlt(HDC h, i32 x, i32 y, i32 an, i32 al, u32 rop)
{
	Dc *d = dc(h);
	RECT r;
	if (!d || !d->s)
		return FALSE;
	if (an < 0) { x += an; an = -an; }
	if (al < 0) { y += al; al = -al; }
	r.left = x; r.top = y; r.right = x + an; r.bottom = y + al;
	switch (rop) {
	case 0x00F00021:                 /* PATCOPY */
		FillRect(h, &r, (HBRUSH)d->brocha);
		return TRUE;
	case 0x00550009:                 /* DSTINVERT */
		return InvertRect(h, &r);
	case 0x00000042:                 /* BLACKNESS */
		sup_rect(d->s, clip_ef(d), sx(d, x), sy(d, y), an, al, 0);
		break;
	case 0x00FF0062:                 /* WHITENESS */
		sup_rect(d->s, clip_ef(d), sx(d, x), sy(d, y), an, al, 0xFFFFFF);
		break;
	case 0x005A0049: {               /* PATINVERT */
		i32 a, b;
		u32 col = d->brocha ? rgb(d->brocha->u.brocha.color) : 0;
		const Region *c = clip_ef(d);
		for (b = sy(d, y); b < sy(d, y + al); b++)
			for (a = sx(d, x); a < sx(d, x + an); a++)
				if (a >= 0 && b >= 0 && a < d->s->an && b < d->s->al && en_clip(c, a, b))
					poner_rgb(d->s, a, b, (leer_rgb(d->s, a, b) ^ col) & 0xFFFFFF);
		break;
	}
	default:
		FillRect(h, &r, (HBRUSH)d->brocha);
	}
	ensuciar(raiz_de(d->w), &d->tope);
	return TRUE;
}

/* ---------------------------------------------------------------------------
 *  Copiar mapas de bits (BitBlt, StretchBlt, StretchDIBits...)
 * ------------------------------------------------------------------------- */
/* la ROP de tres (brocha P, origen S, destino D), bit a bit */
static u32 rop3(u32 rop, u32 p, u32 s, u32 d)
{
	u32 r = 0, k = (rop >> 16) & 0xFF;
	switch (rop) {
	case 0x00CC0020: return s;
	case 0x00EE0086: return s | d;
	case 0x008800C6: return s & d;
	case 0x00660046: return s ^ d;
	case 0x00440328: return s & ~d;
	case 0x00330008: return ~s;
	case 0x001100A6: return ~(s | d);
	case 0x00C000CA: return p & s;
	case 0x00BB0226: return ~s | d;
	case 0x00F00021: return p;
	case 0x00FB0A09: return p | ~s | d;
	case 0x005A0049: return p ^ d;
	case 0x00550009: return ~d;
	case 0x00000042: return 0;
	case 0x00FF0062: return 0xFFFFFF;
	case 0x00AA0029: return d;
	}
	if (k & 0x01) r |= ~p & ~s & ~d;
	if (k & 0x02) r |= ~p & ~s & d;
	if (k & 0x04) r |= ~p & s & ~d;
	if (k & 0x08) r |= ~p & s & d;
	if (k & 0x10) r |= p & ~s & ~d;
	if (k & 0x20) r |= p & ~s & d;
	if (k & 0x40) r |= p & s & ~d;
	if (k & 0x80) r |= p & s & d;
	return r;
}
/* copia (estirando con el punto más cercano) de os a ds: lo de destino
 * (xd, yd, wd, hd), en la superficie de destino; lo de origen, en la suya */
static void copia_sup(Dc *d, Sup *ds, const Region *clip, i32 xd, i32 yd, i32 wd, i32 hd, Sup *os, i32 xs, i32 ys, i32 ws, i32 hs, u32 rop, COLORREF tinta, COLORREF fondo, int espejo_x, int espejo_y)
{
	int i;
	u32 p = 0;
	RECT tope = { 0, 0, ds->an, ds->al };
	if (!ds->bits || !os->bits || wd <= 0 || hd <= 0 || ws <= 0 || hs <= 0)
		return;
	if (d && d->brocha)
		p = rgb(d->brocha->u.brocha.color);
	for (i = 0; i < clip->n; i++) {
		RECT a = { xd, yd, xd + wd, yd + hd };
		i32 x, y;
		if (!corta(&a, &clip->r[i]) || !corta(&a, &tope))
			continue;
		/* lo más rápido: de 32 a 32, sin estirar, copiando */
		if (rop == 0x00CC0020 && ws == wd && hs == hd && !espejo_x && !espejo_y && os->bpp == 32 && ds->bpp == 32 && !os->mono && !ds->mono) {
			for (y = a.top; y < a.bottom; y++) {
				i32 oy = ys + (y - yd);
				if (oy < 0 || oy >= os->al)
					continue;
				{
					i32 x0 = a.left, x1 = a.right, ox = xs + (x0 - xd);
					if (ox < 0) { x0 -= ox; ox = 0; }
					if (ox + (x1 - x0) > os->an) x1 = x0 + os->an - ox;
					if (x1 > x0)
						memcpy((u32 *)fila(ds, y) + x0, (u32 *)fila(os, oy) + ox, (x1 - x0) * 4);
				}
			}
			continue;
		}
		for (y = a.top; y < a.bottom; y++) {
			i32 oy = ys + (i32)((i64)(y - yd) * hs / hd);
			if (espejo_y)
				oy = ys + hs - 1 - (i32)((i64)(y - yd) * hs / hd);
			if (oy < 0 || oy >= os->al)
				continue;
			for (x = a.left; x < a.right; x++) {
				i32 ox = xs + (i32)((i64)(x - xd) * ws / wd);
				u32 s, r;
				if (espejo_x)
					ox = xs + ws - 1 - (i32)((i64)(x - xd) * ws / wd);
				if (ox < 0 || ox >= os->an)
					continue;
				s = leer_rgb(os, ox, oy);
				if (os->mono && !ds->mono)
					s = s ? rgb(fondo) : rgb(tinta);
				else if (!os->mono && ds->mono)
					s = s == rgb(fondo) ? 0xFFFFFF : 0;
				if (rop == 0x00CC0020)
					r = s;
				else
					r = rop3(rop, p, s, leer_rgb(ds, x, y));
				poner_rgb(ds, x, y, r & 0xFFFFFF);
			}
		}
	}
}
API(BOOL) StretchBlt(HDC hd, i32 xd, i32 yd, i32 wd, i32 hd_, HDC hs, i32 xs, i32 ys, i32 ws, i32 hs_, u32 rop)
{
	Dc *d = dc(hd), *s = dc(hs);
	int ex = 0, ey = 0;
	if (!d || !d->s)
		return FALSE;
	if (!s || ((rop >> 16) & 0x33) == (((rop >> 16) & 0xCC) >> 2)) {
		/* sin origen */
		return PatBlt(hd, xd, yd, wd, hd_, rop);
	}
	if (!s->s)
		return FALSE;
	if (wd < 0) { xd += wd; wd = -wd; ex = !ex; }
	if (hd_ < 0) { yd += hd_; hd_ = -hd_; ey = !ey; }
	if (ws < 0) { xs += ws; ws = -ws; ex = !ex; }
	if (hs_ < 0) { ys += hs_; hs_ = -hs_; ey = !ey; }
	copia_sup(d, d->s, clip_ef(d), sx(d, xd), sy(d, yd), wd, hd_, s->s, sx(s, xs), sy(s, ys), ws, hs_, rop, d->tinta, d->fondo, ex, ey);
	ensuciar(raiz_de(d->w), &d->tope);
	return TRUE;
}
API(BOOL) BitBlt(HDC hd, i32 xd, i32 yd, i32 w, i32 h, HDC hs, i32 xs, i32 ys, u32 rop)
{
	return StretchBlt(hd, xd, yd, w, h, hs, xs, ys, w, h, rop);
}
API(BOOL) MaskBlt(HDC hd, i32 xd, i32 yd, i32 w, i32 h, HDC hs, i32 xs, i32 ys, HBITMAP m, i32 xm, i32 ym, u32 rop)
{
	(void)m; (void)xm; (void)ym;
	return BitBlt(hd, xd, yd, w, h, hs, xs, ys, rop & 0xFFFFFF ? 0x00CC0020 : rop);
}
API(BOOL) TransparentBlt(HDC hd, i32 xd, i32 yd, i32 wd, i32 hd_, HDC hs, i32 xs, i32 ys, i32 ws, i32 hs_, u32 clave)
{
	Dc *d = dc(hd), *s = dc(hs);
	i32 x, y;
	const Region *c;
	if (!d || !s || !d->s || !s->s || wd <= 0 || hd_ <= 0)
		return FALSE;
	c = clip_ef(d);
	for (y = 0; y < hd_; y++)
		for (x = 0; x < wd; x++) {
			i32 a = sx(d, xd + x), b = sy(d, yd + y), ox = sx(s, xs + x * ws / wd), oy = sy(s, ys + y * hs_ / hd_);
			u32 v;
			if (a < 0 || b < 0 || a >= d->s->an || b >= d->s->al || ox < 0 || oy < 0 || ox >= s->s->an || oy >= s->s->al || !en_clip(c, a, b))
				continue;
			v = leer_rgb(s->s, ox, oy);
			if (v != rgb(clave))
				poner_rgb(d->s, a, b, v);
		}
	ensuciar(raiz_de(d->w), &d->tope);
	return TRUE;
}
API(BOOL) GdiTransparentBlt(HDC hd, i32 xd, i32 yd, i32 wd, i32 hd_, HDC hs, i32 xs, i32 ys, i32 ws, i32 hs_, u32 clave) { return TransparentBlt(hd, xd, yd, wd, hd_, hs, xs, ys, ws, hs_, clave); }
API(i32) StretchDIBits(HDC h, i32 xd, i32 yd, i32 wd, i32 hd, i32 xs, i32 ys, i32 ws, i32 hs, const void *bits, const void *bmi, u32 uso, u32 rop)
{
	Dc *d = dc(h);
	Sup s;
	int ex = 0, ey = 0;
	if (!d || !d->s || !bits || !sup_de_bmi(&s, bmi, bits, uso, d))
		return 0;
	if (wd < 0) { xd += wd; wd = -wd; ex = !ex; }
	if (hd < 0) { yd += hd; hd = -hd; ey = !ey; }
	if (ws < 0) { xs += ws; ws = -ws; ex = !ex; }
	if (hs < 0) { ys += hs; hs = -hs; ey = !ey; }
	/* el origen va de abajo arriba en las coordenadas del DIB: ys cuenta desde abajo */
	if (s.de_abajo)
		ys = s.al - ys - hs;
	copia_sup(d, d->s, clip_ef(d), sx(d, xd), sy(d, yd), wd, hd, &s, xs, ys, ws, hs, rop, d->tinta, d->fondo, ex, ey);
	ensuciar(raiz_de(d->w), &d->tope);
	return hs;
}
API(i32) SetDIBitsToDevice(HDC h, i32 xd, i32 yd, u32 w, u32 al, i32 xs, i32 ys, u32 ini, u32 n, const void *bits, const void *bmi, u32 uso)
{
	Dc *d = dc(h);
	Sup s;
	i32 sy0;
	if (!d || !d->s || !bits || !sup_de_bmi(&s, bmi, bits, uso, d))
		return 0;
	/* (ini, n): las filas que trae «bits», contando desde abajo */
	if (s.de_abajo) {
		s.al = n;
		s.bits = (u8 *)bits + (n - 1) * (-s.paso);
		sy0 = (i32)ini + n - ys - al;
		sy0 = n - (ys - ini) - al;
	} else {
		sy0 = ys;
	}
	copia_sup(d, d->s, clip_ef(d), sx(d, xd), sy(d, yd), w, al, &s, xs, sy0, w, al, 0x00CC0020, d->tinta, d->fondo, 0, 0);
	ensuciar(raiz_de(d->w), &d->tope);
	return n;
}
API(i32) SetDIBits(HDC h, HBITMAP hb, u32 ini, u32 n, const void *bits, const void *bmi, u32 uso)
{
	Gdi *g = gdi(hb);
	Sup s;
	if (!g || g->tipo != G_MAPA || !sup_de_bmi(&s, bmi, bits, uso, dc(h)))
		return 0;
	{
		Region c;
		RECT r = { 0, 0, g->u.mapa.s.an, g->u.mapa.s.al };
		i32 y0;
		reg_rect(&c, &r);
		if (s.de_abajo) {
			s.al = n;
			s.bits = (u8 *)bits + (n - 1) * (-s.paso);
			y0 = g->u.mapa.s.al - ini - n;
		} else
			y0 = ini;
		copia_sup(0, &g->u.mapa.s, &c, 0, y0, s.an, s.al, &s, 0, 0, s.an, s.al, 0x00CC0020, 0, 0xFFFFFF, 0, 0);
	}
	return n;
}
API(i32) GetDIBits(HDC h, HBITMAP hb, u32 ini, u32 n, void *bits, u8 *bmi, u32 uso)
{
	Gdi *g = gdi(hb);
	BIH *b = (BIH *)bmi;
	Sup *o, s;
	(void)h;
	if (!g || g->tipo != G_MAPA || !b)
		return 0;
	o = &g->u.mapa.s;
	if (b->bpp == 0 || !bits) {
		if (b->bpp == 0) {
			b->tam = 40;
			b->an = o->an;
			b->al = o->al;
			b->planos = 1;
			b->bpp = o->mono ? 1 : o->bpp;
			b->comp = 0;
			b->tamimg = ((o->an * b->bpp + 31) / 32) * 4 * o->al;
			if (b->bpp <= 8) {
				u32 k, *t = (u32 *)(bmi + 40);
				for (k = 0; k < (1u << b->bpp); k++)
					t[k] = o->mono ? (k ? 0xFFFFFF : 0) : o->pal[k];
			}
			return o->al;
		}
		if (b->bpp <= 8 && uso == 0) {
			u32 k, *t = (u32 *)(bmi + 40);
			for (k = 0; k < (1u << b->bpp); k++)
				t[k] = o->mono || b->bpp == 1 ? (k ? 0xFFFFFF : 0) : (o->npal ? o->pal[k] : rgb(0x01000000 | k));
		}
		return o->al;
	}
	if (b->bpp <= 8) {
		u32 k, *t = (u32 *)(bmi + 40);
		for (k = 0; k < (1u << b->bpp); k++)
			t[k] = b->bpp == 1 ? (k ? 0xFFFFFF : 0) : (o->npal ? o->pal[k] : rgb(0x01000000 | k));
		if (!b->usados)
			b->usados = 0;
	}
	if (!sup_de_bmi(&s, bmi, bits, uso, 0))
		return 0;
	{
		Region c;
		RECT r = { 0, 0, s.an, s.al };
		reg_rect(&c, &r);
		/* las filas de «ini» a «ini + n» (de abajo arriba) */
		if (s.de_abajo)
			copia_sup(0, &s, &c, 0, s.al - ini - n, s.an, n, o, 0, o->al - ini - n, o->an, n, 0x00CC0020, 0, 0xFFFFFF, 0, 0);
		else
			copia_sup(0, &s, &c, 0, ini, s.an, n, o, 0, ini, o->an, n, 0x00CC0020, 0, 0xFFFFFF, 0, 0);
	}
	return n;
}
API(HBITMAP) CreateDIBitmap(HDC h, const BIH *b, u32 ini, const void *bits, const void *bmi, u32 uso)
{
	HBITMAP r;
	i32 al;
	if (!b)
		return 0;
	al = b->al < 0 ? -b->al : b->al;
	r = (HBITMAP)mapa_nuevo(b->an, al, 32, b->bpp == 1 && 0);
	if (r && (ini & 4) && bits && bmi)
		SetDIBits(h, r, 0, al, bits, bmi, uso);
	return r;
}
API(u32) SetDIBColorTable(HDC h, u32 a, u32 n, const u32 *t)
{
	Dc *d = dc(h);
	u32 k;
	if (!d || !d->mapa || !d->s)
		return 0;
	for (k = 0; k < n && a + k < 256; k++)
		d->s->pal[a + k] = t[k] & 0xFFFFFF;
	if (a + k > d->s->npal)
		d->s->npal = a + k;
	return k;
}
API(u32) GetDIBColorTable(HDC h, u32 a, u32 n, u32 *t)
{
	Dc *d = dc(h);
	u32 k;
	if (!d || !d->s)
		return 0;
	for (k = 0; k < n && a + k < d->s->npal; k++)
		t[k] = d->s->pal[a + k];
	return k;
}
API(i32) GetBitmapBits(HBITMAP h, i32 n, void *d)
{
	Gdi *g = gdi(h);
	Sup *s;
	i32 y, paso;
	if (!g || g->tipo != G_MAPA)
		return 0;
	s = &g->u.mapa.s;
	paso = s->paso < 0 ? -s->paso : s->paso;
	for (y = 0; y < s->al && (y + 1) * paso <= n; y++)
		memcpy((u8 *)d + y * paso, fila(s, y), paso);
	return y * paso;
}
API(i32) SetBitmapBits(HBITMAP h, u32 n, const void *o)
{
	Gdi *g = gdi(h);
	Sup *s;
	i32 y, paso;
	if (!g || g->tipo != G_MAPA)
		return 0;
	s = &g->u.mapa.s;
	paso = s->paso < 0 ? -s->paso : s->paso;
	for (y = 0; y < s->al && (u32)((y + 1) * paso) <= n; y++)
		memcpy(fila(s, y), (const u8 *)o + y * paso, paso);
	return y * paso;
}
API(BOOL) GetBitmapDimensionEx(HBITMAP h, SIZE *s) { (void)h; s->cx = s->cy = 0; return TRUE; }

/* un BITMAP de recurso (BITMAPINFOHEADER y sus puntos) -> mapa de bits */
HBITMAP mapa_de_dib(const u8 *p, u32 tam)
{
	const BIH *b = (const BIH *)p;
	u32 tabla;
	HBITMAP h;
	(void)tam;
	if (!b)
		return 0;
	tabla = b->tam == 12 ? (b->tam + 3 * (((const u16 *)p)[5] <= 8 ? 1u << ((const u16 *)p)[5] : 0)) :
		b->tam + 4 * (b->bpp <= 8 ? (b->usados ? b->usados : 1u << b->bpp) : (b->comp == 3 ? 3 : 0));
	h = CreateDIBitmap(0, b, 4, p + tabla, p, 0);
	if (h && b->tam != 12 && b->bpp == 1) {
		/* (los de 1 bit que son blanco y negro: monocromos de verdad) */
		const u32 *t = (const u32 *)(p + b->tam);
		if ((t[0] & 0xFFFFFF) == 0 && (t[1] & 0xFFFFFF) == 0xFFFFFF)
			gdi(h)->u.mapa.s.mono = 1;
	}
	return h;
}

/* ---------------------------------------------------------------------------
 *  Las letras
 * ------------------------------------------------------------------------- */
/* cuántas veces se agranda la de 8x16 */
static int veces_de(Gdi *f)
{
	i32 a;
	if (!f || f->tipo != G_LETRA)
		return 1;
	a = f->u.letra.alto < 0 ? -f->u.letra.alto * 4 / 3 : f->u.letra.alto;
	if (a <= 24)
		return 1;
	return (a + 8) / 16;
}
i32 alto_letra(Dc *d) { return 16 * veces_de(d ? d->letra : 0); }
i32 ancho_letra(Dc *d) { return 8 * veces_de(d ? d->letra : 0); }
void texto_en(Sup *s, const Region *clip, i32 x, i32 y, const char *t, i32 n, u32 tinta, int k, int negrita)
{
	i32 i, fy, fx;
	RECT caja = { 0, 0, s->an, s->al };
	u32 v = valor_de(s, tinta);
	for (i = 0; i < n; i++, x += 8 * k) {
		const u8 *g = letra8x16 + a437c((u8)t[i]) * 16;
		RECT c = { x, y, x + 8 * k + negrita, y + 16 * k };
		int j, toca = 0;
		if (!corta(&c, &caja))
			continue;
		for (j = 0; j < clip->n; j++) {
			RECT a = c;
			if (corta(&a, &clip->r[j])) { toca = 1; break; }
		}
		if (!toca)
			continue;
		for (fy = 0; fy < 16 * k; fy++) {
			u8 b = g[fy / k];
			i32 py = y + fy;
			if (!b || py < 0 || py >= s->al)
				continue;
			for (fx = 0; fx < 8 * k; fx++)
				if (b & (0x80 >> (fx / k))) {
					i32 px = x + fx, q;
					for (q = 0; q <= negrita; q++)
						if (px + q >= 0 && px + q < s->an && en_clip(clip, px + q, py)) {
							if (s->bpp == 32 && !s->mono)
								((u32 *)fila(s, py))[px + q] = tinta;
							else
								poner(s, px + q, py, v);
						}
				}
		}
	}
}
/* el texto con lo del DC (en la superficie, desde x, y: arriba a la izquierda) */
static void texto_dc(Dc *d, i32 x, i32 y, const char *t, i32 n, const i32 *dx, const RECT *opaco)
{
	Gdi *f = d->letra;
	int k = veces_de(f), neg = f && f->tipo == G_LETRA && f->u.letra.peso >= 600;
	const Region *c = clip_ef(d);
	u32 tinta = rgb(d->tinta);
	i32 an = n * 8 * k;
	if (!d->s)
		return;
	if (d->modo_fondo == 2 && !opaco)
		sup_rect(d->s, c, x, y, an, 16 * k, d->fondo);
	if (!dx)
		texto_en(d->s, c, x, y, t, n, tinta, k, neg);
	else {
		i32 i;
		for (i = 0; i < n; i++) {
			texto_en(d->s, c, x, y, t + i, 1, tinta, k, neg);
			x += dx[i];
		}
		x -= an;
	}
	if (f && f->tipo == G_LETRA && f->u.letra.subraya)
		sup_rect(d->s, c, x, y + 14 * k, an, k, d->tinta);
	if (f && f->tipo == G_LETRA && f->u.letra.tachada)
		sup_rect(d->s, c, x, y + 8 * k, an, k, d->tinta);
}
/* dónde empieza el texto según la alineación */
static void alinear(Dc *d, i32 *x, i32 *y, i32 an)
{
	int k = veces_de(d->letra);
	u32 a = d->alinea;
	if (a & 1)                               /* TA_UPDATECP */
		*x = d->pos.x, *y = d->pos.y;
	if ((a & 6) == 6) *x -= an / 2;          /* TA_CENTER */
	else if (a & 2) *x -= an;                /* TA_RIGHT */
	if ((a & 24) == 24) *y -= 12 * k;        /* TA_BASELINE */
	else if (a & 8) *y -= 16 * k;            /* TA_BOTTOM */
}
API(BOOL) TextOutA(HDC h, i32 x, i32 y, const char *t, i32 n)
{
	Dc *d = dc(h);
	i32 an;
	if (!d || !t)
		return FALSE;
	if (n < 0)
		n = strlen(t);
	an = n * ancho_letra(d);
	alinear(d, &x, &y, an);
	texto_dc(d, sx(d, x), sy(d, y), t, n, 0, 0);
	if (d->alinea & 1)
		d->pos.x = (d->alinea & 6) == 6 ? d->pos.x : (d->alinea & 2) ? x : x + an;
	ensuciar(raiz_de(d->w), &d->tope);
	return TRUE;
}
API(BOOL) TextOutW(HDC h, i32 x, i32 y, const WCHAR *t, i32 n)
{
	char b[512];
	i32 k;
	if (n < 0)
		for (n = 0; t[n]; n++)
			;
	if (n > 511)
		n = 511;
	for (k = 0; k < n; k++)
		b[k] = de_uni(t[k]);
	return TextOutA(h, x, y, b, n);
}
API(BOOL) ExtTextOutA(HDC h, i32 x, i32 y, u32 op, const RECT *r, const char *t, u32 n, const i32 *dx)
{
	Dc *d = dc(h);
	i32 an = 0;
	u32 k;
	if (!d)
		return FALSE;
	if (!t)
		n = 0;
	if (dx)
		for (k = 0; k < n; k++)
			an += dx[k];
	else
		an = n * ancho_letra(d);
	if (r && (op & 2)) {                     /* ETO_OPAQUE */
		RECT s = { sx(d, r->left), sy(d, r->top), sx(d, r->right), sy(d, r->bottom) };
		rellenar(d->s, clip_ef(d), s, rgb(d->fondo));
	}
	alinear(d, &x, &y, an);
	if (n) {
		if (r && (op & 4)) {                 /* ETO_CLIPPED */
			Region guarda = d->clip;
			RECT s = { sx(d, r->left), sy(d, r->top), sx(d, r->right), sy(d, r->bottom) };
			reg_corta(&d->clip, &s);
			texto_dc(d, sx(d, x), sy(d, y), t, n, dx, (r && (op & 2)) ? r : 0);
			d->clip = guarda;
		} else
			texto_dc(d, sx(d, x), sy(d, y), t, n, dx, (r && (op & 2)) ? r : 0);
	}
	if (d->alinea & 1)
		d->pos.x = x + an;
	ensuciar(raiz_de(d->w), &d->tope);
	return TRUE;
}
API(BOOL) ExtTextOutW(HDC h, i32 x, i32 y, u32 op, const RECT *r, const WCHAR *t, u32 n, const i32 *dx)
{
	char b[512];
	u32 k;
	if (n > 511)
		n = 511;
	for (k = 0; k < n; k++)
		b[k] = de_uni(t[k]);
	return ExtTextOutA(h, x, y, op, r, b, n, dx);
}
API(BOOL) GetTextExtentPoint32A(HDC h, const char *t, i32 n, SIZE *s)
{
	Dc *d = dc(h);
	(void)t;
	if (n < 0)
		n = 0;
	s->cx = n * ancho_letra(d);
	s->cy = alto_letra(d);
	return TRUE;
}
API(BOOL) GetTextExtentPointA(HDC h, const char *t, i32 n, SIZE *s) { return GetTextExtentPoint32A(h, t, n, s); }
API(BOOL) GetTextExtentPoint32W(HDC h, const WCHAR *t, i32 n, SIZE *s) { (void)t; return GetTextExtentPoint32A(h, 0, n, s); }
API(BOOL) GetTextExtentExPointA(HDC h, const char *t, i32 n, i32 max, i32 *caben, i32 *dx, SIZE *s)
{
	i32 a = ancho_letra(dc(h)), k;
	(void)t;
	if (caben)
		*caben = a ? MIN(n, max / a) : n;
	if (dx)
		for (k = 0; k < n; k++)
			dx[k] = (k + 1) * a;
	return GetTextExtentPoint32A(h, t, n, s);
}
API(u32) GetTabbedTextExtentA(HDC h, const char *t, i32 n, i32 nt, const i32 *tabs)
{
	i32 a = ancho_letra(dc(h)), x = 0, k;
	(void)nt; (void)tabs;
	for (k = 0; k < n; k++)
		x = t[k] == '\t' ? (x / (8 * a) + 1) * 8 * a : x + a;
	return MAKELONG(x, alto_letra(dc(h)));
}
API(i32) TabbedTextOutA(HDC h, i32 x, i32 y, const char *t, i32 n, i32 nt, const i32 *tabs, i32 o)
{
	i32 a = ancho_letra(dc(h)), cx = 0, k, ini = 0;
	for (k = 0; k <= n; k++)
		if (k == n || t[k] == '\t') {
			TextOutA(h, x + cx, y, t + ini, k - ini);
			cx += (k - ini) * a;
			if (k < n) {
				i32 paso = nt > 0 && tabs ? tabs[0] : 8 * a;
				if (nt > 1) {
					i32 j;
					for (j = 0; j < nt && tabs[j] + o - x <= cx; j++)
						;
					cx = j < nt ? tabs[j] + o - x : (cx / paso + 1) * paso;
				} else
					cx = (cx / paso + 1) * paso;
			}
			ini = k + 1;
		}
	return MAKELONG(cx, alto_letra(dc(h)));
}
typedef struct {
	i32 alto, asc, desc, int_lead, ext_lead, ancho_medio, ancho_max, peso, sobra, aspx, aspy;
	u8 primera, ultima, def, rompe, cursiva, subraya, tachada, paso_fam, juego;
} TEXTMETRICA;
API(BOOL) GetTextMetricsA(HDC h, TEXTMETRICA *t)
{
	Dc *d = dc(h);
	int k = veces_de(d ? d->letra : 0);
	Gdi *f = d ? d->letra : 0;
	memset(t, 0, sizeof *t);
	t->alto = 16 * k;
	t->asc = 12 * k;
	t->desc = 4 * k;
	t->int_lead = 2 * k;
	t->ancho_medio = 8 * k;
	t->ancho_max = 8 * k;
	t->peso = f && f->tipo == G_LETRA ? f->u.letra.peso : 400;
	t->aspx = t->aspy = 96;
	t->primera = 0x20;
	t->ultima = 0xFF;
	t->def = '?';
	t->rompe = ' ';
	t->cursiva = f && f->tipo == G_LETRA ? f->u.letra.cursiva : 0;
	t->subraya = f && f->tipo == G_LETRA ? f->u.letra.subraya : 0;
	t->paso_fam = 0x30 | 0x06;     /* (FF_MODERN; bit 0 a cero: de paso fijo) */
	return TRUE;
}
API(BOOL) GetTextMetricsW(HDC h, u8 *t)
{
	TEXTMETRICA a;
	GetTextMetricsA(h, &a);
	memcpy(t, &a, 44);
	*(u16 *)(t + 44) = 0x20; *(u16 *)(t + 46) = 0xFF; *(u16 *)(t + 48) = '?'; *(u16 *)(t + 50) = ' ';
	t[52] = a.cursiva; t[53] = a.subraya; t[54] = a.tachada; t[55] = a.paso_fam; t[56] = 0;
	return TRUE;
}
API(BOOL) GetCharWidthA(HDC h, u32 a, u32 b, i32 *w) { u32 k; for (k = a; k <= b; k++) w[k - a] = ancho_letra(dc(h)); return TRUE; }
API(BOOL) GetCharWidth32A(HDC h, u32 a, u32 b, i32 *w) { return GetCharWidthA(h, a, b, w); }
API(BOOL) GetCharABCWidthsA(HDC h, u32 a, u32 b, i32 *w) { u32 k; for (k = a; k <= b; k++) { w[(k - a) * 3] = 0; w[(k - a) * 3 + 1] = ancho_letra(dc(h)); w[(k - a) * 3 + 2] = 0; } return TRUE; }
API(i32) GetTextFaceA(HDC h, i32 n, char *d)
{
	Dc *x = dc(h);
	const char *c = x && x->letra && x->letra->u.letra.cara[0] ? x->letra->u.letra.cara : "System";
	if (!d)
		return strlen(c) + 1;
	copia_n(d, c, n);
	return strlen(d);
}
API(u32) GetTextCharset(HDC h) { (void)h; return 0; }
API(i32) SetTextCharacterExtra(HDC h, i32 e) { (void)h; (void)e; return 0; }
API(i32) SetTextJustification(HDC h, i32 a, i32 b) { (void)h; (void)a; (void)b; return 1; }
API(u32) GetFontLanguageInfo(HDC h) { (void)h; return 0; }
typedef i32 (WINAPI *FONTPROC)(const void *, const void *, u32, LPARAM);
API(i32) EnumFontFamiliesExA(HDC h, const LOGFONTA *l, FONTPROC f, LPARAM lp, u32 fl)
{
	static const char *caras[] = { "System", "Fixedsys", "Courier New", "Arial", "MS Sans Serif" };
	u32 k;
	(void)fl;
	for (k = 0; k < 5; k++) {
		u8 elf[156 + 32];
		TEXTMETRICA tm;
		LOGFONTA *e = (LOGFONTA *)elf;
		if (l && l->cara[0] && stricmp_(l->cara, caras[k]))
			continue;
		memset(elf, 0, sizeof elf);
		e->alto = 16; e->peso = 400; e->paso_familia = k < 3 ? 0x31 : 0x22;
		strcpy(e->cara, caras[k]);
		strcpy((char *)elf + 60, caras[k]);
		GetTextMetricsA(h, &tm);
		if (!f(elf, &tm, 4, lp))
			return 0;
	}
	return 1;
}
API(i32) EnumFontFamiliesA(HDC h, const char *c, FONTPROC f, LPARAM lp)
{
	LOGFONTA l;
	memset(&l, 0, sizeof l);
	if (c)
		copia_n(l.cara, c, 32);
	return EnumFontFamiliesExA(h, &l, f, lp, 0);
}
API(i32) EnumFontsA(HDC h, const char *c, FONTPROC f, LPARAM lp) { return EnumFontFamiliesA(h, c, f, lp); }
API(i32) AddFontResourceA(const char *n) { (void)n; return 0; }
API(BOOL) RemoveFontResourceA(const char *n) { (void)n; return FALSE; }

/* ---- el borde en relieve (DrawEdge, de USER32, aquí por el dibujo) ---- */
/* == USER32 == */
static void raya(HDC h, i32 x0, i32 y0, i32 x1, i32 y1, i32 col)
{
	RECT r = { x0, y0, x1, y1 };
	FillRect(h, &r, GetSysColorBrush(col));
}
API(BOOL) DrawEdge(HDC h, RECT *r, u32 borde, u32 fl)
{
	RECT c = *r;
	int k;
	for (k = 0; k < 2; k++) {
		u32 b = k ? (borde >> 2) & 3 : borde & 3;    /* exterior, interior */
		i32 arriba, abajo;
		if (!b)
			continue;
		/* BDR_RAISEDOUTER 1, SUNKENOUTER 2, RAISEDINNER 4, SUNKENINNER 8 */
		if (k == 0) { arriba = b & 1 ? COLOR_3DLIGHT : COLOR_BTNSHADOW; abajo = b & 1 ? COLOR_3DDKSHADOW : COLOR_BTNHIGHLIGHT; }
		else { arriba = b & 1 ? COLOR_BTNHIGHLIGHT : COLOR_3DDKSHADOW; abajo = b & 1 ? COLOR_BTNSHADOW : COLOR_3DLIGHT; }
		if (fl & 2) raya(h, c.left, c.top, c.right, c.top + 1, arriba);          /* BF_TOP */
		if (fl & 1) raya(h, c.left, c.top, c.left + 1, c.bottom, arriba);        /* BF_LEFT */
		if (fl & 8) raya(h, c.left, c.bottom - 1, c.right, c.bottom, abajo);     /* BF_BOTTOM */
		if (fl & 4) raya(h, c.right - 1, c.top, c.right, c.bottom, abajo);       /* BF_RIGHT */
		if (fl & 1) c.left++;
		if (fl & 2) c.top++;
		if (fl & 4) c.right--;
		if (fl & 8) c.bottom--;
	}
	if (fl & 0x800)                                                              /* BF_MIDDLE */
		FillRect(h, &c, GetSysColorBrush(COLOR_BTNFACE));
	if (fl & 0x2000)                                                             /* BF_ADJUST */
		*r = c;
	return TRUE;
}
/* un rectángulo de botón: con su cara y su relieve */
void relleno_3d(HDC h, RECT r, int hundido)
{
	DrawEdge(h, &r, hundido ? 0x0A : 0x05, 0x80F);
}
