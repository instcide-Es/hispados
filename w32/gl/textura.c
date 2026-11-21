/* textura.c - HispaGL: los objetos de textura (glGenTextures, glBindTexture,
 * glDeleteTextures...), la subida de las imágenes (glTexImage*,
 * glTexSubImage*, glCopyTex*) a 16 bits por punto (5-6-5 si no hay alfa,
 * 1-5-5-5 si el alfa es todo o nada, 4-4-4-4 si no), los niveles de mipmap,
 * los parámetros (filtros, repetición), glTexEnv, glTexGen, las paletas de
 * GL_EXT_paletted_texture, y lo de leer píxeles del programa (glPixelStore).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "hgl.h"

static void tex_ini(Tex *t, u32 nombre, int dim)
{
	hgl_pon(t, 0, sizeof *t);
	t->nombre = nombre;
	t->dim = dim;
	t->base = GL_RGBA;
	t->intfmt = 1;
	t->minf = GL_NEAREST_MIPMAP_LINEAR;
	t->magf = GL_LINEAR;
	t->ws = t->wt = GL_REPEAT;
	t->prioridad = 1;
}

void hgl_texturas_ini(Ctx *C)
{
	Comun *c = C->com;
	if (!c->def2d.dim) {
		tex_ini(&c->def2d, 0, 2);
		tex_ini(&c->def1d, 0, 1);
		tex_ini(&c->proxy2d, 0, 2);
		tex_ini(&c->proxy1d, 0, 1);
	}
}

Tex *hgl_tex_busca(Comun *c, u32 nombre, int crea)
{
	Tex *t = c->hash[nombre & 255];
	while (t && t->nombre != nombre) t = t->sig;
	if (!t && crea) {
		t = hgl_pedir(sizeof(Tex));
		if (!t) return NULL;
		tex_ini(t, nombre, 0);
		t->sig = c->hash[nombre & 255];
		c->hash[nombre & 255] = t;
	}
	return t;
}

static void suelta_niveles(Tex *t)
{
	int k;
	for (k = 0; k < NIVELES; k++) { hgl_soltar(t->niv[k].p); t->niv[k].p = NULL; t->niv[k].w = t->niv[k].h = 0; }
	hgl_soltar(t->paleta);
	t->paleta = NULL;
}
void hgl_tex_suelta_todo(Comun *c)
{
	int k;
	for (k = 0; k < 256; k++) {
		Tex *t = c->hash[k];
		while (t) { Tex *s = t->sig; suelta_niveles(t); hgl_soltar(t); t = s; }
		c->hash[k] = NULL;
	}
	suelta_niveles(&c->def1d);
	suelta_niveles(&c->def2d);
}

static int usa_mip(int f) { return f != GL_NEAREST && f != GL_LINEAR; }
int hgl_tex_completa(Tex *t)
{
	int k, w, h;
	if (t->estado) return t->estado == 1;
	t->estado = 2;
	if (!t->niv[0].p || !t->niv[0].w || !t->niv[0].h) return 0;
	if (!usa_mip(t->minf)) { t->nniv = 1; t->estado = 1; return 1; }
	w = t->niv[0].w; h = t->niv[0].h;
	for (k = 1; k < NIVELES; k++) {
		if (w == 1 && h == 1) break;
		w = w > 1 ? w >> 1 : 1;
		h = h > 1 ? h >> 1 : 1;
		if (!t->niv[k].p || t->niv[k].w != w || t->niv[k].h != h) return 0;
	}
	t->nniv = k;
	t->estado = 1;
	return 1;
}

Tex *hgl_tex_activa(Ctx *C, int u)
{
	Tex *t;
	if (C->hab.tex2d[u]) t = C->tex.lig2d[u] ? C->tex.lig2d[u] : &C->com->def2d;
	else if (C->hab.tex1d[u]) t = C->tex.lig1d[u] ? C->tex.lig1d[u] : &C->com->def1d;
	else return NULL;
	return hgl_tex_completa(t) ? t : NULL;
}

static Tex *ligada(Ctx *C, u32 dest)
{
	int u = C->tex.activa;
	if (dest == GL_TEXTURE_2D) return C->tex.lig2d[u] ? C->tex.lig2d[u] : &C->com->def2d;
	if (dest == GL_TEXTURE_1D) return C->tex.lig1d[u] ? C->tex.lig1d[u] : &C->com->def1d;
	if (dest == GL_PROXY_TEXTURE_2D) return &C->com->proxy2d;
	if (dest == GL_PROXY_TEXTURE_1D) return &C->com->proxy1d;
	hgl_error(C, GL_INVALID_ENUM);
	return NULL;
}

/* ---- glGenTextures, glBindTexture, glDeleteTextures ---- */
void APIENTRY glGenTextures(GLsizei n, GLuint *r)
{
	CTX();
	int k;
	if (n < 0) { hgl_error(C, GL_INVALID_VALUE); return; }
	for (k = 0; k < n; k++) {
		do C->com->sig_tex++; while (!C->com->sig_tex || hgl_tex_busca(C->com, C->com->sig_tex, 0));
		r[k] = C->com->sig_tex;
		hgl_tex_busca(C->com, r[k], 1);
	}
}
static void i_liga(u32 dest, u32 nombre)
{
	CTX();
	Tex *t = NULL;
	int u = C->tex.activa, dim = dest == GL_TEXTURE_2D ? 2 : dest == GL_TEXTURE_1D ? 1 : 0;
	if (!dim) { hgl_error(C, GL_INVALID_ENUM); return; }
	if (nombre) {
		t = hgl_tex_busca(C->com, nombre, 1);
		if (!t) { hgl_error(C, GL_OUT_OF_MEMORY); return; }
		if (!t->dim) t->dim = dim;
		else if (t->dim != dim) { hgl_error(C, GL_INVALID_OPERATION); return; }
	}
	if (dim == 2) C->tex.lig2d[u] = t; else C->tex.lig1d[u] = t;
}
void APIENTRY glBindTexture(GLenum d, GLuint n) { CTX(); LISTA(i_liga, 2, d, n); i_liga(d, n); }
void APIENTRY glDeleteTextures(GLsizei n, const GLuint *v)
{
	CTX();
	int k, u;
	for (k = 0; k < n; k++) {
		Tex **pp, *t;
		if (!v[k]) continue;
		pp = &C->com->hash[v[k] & 255];
		while (*pp && (*pp)->nombre != v[k]) pp = &(*pp)->sig;
		if (!(t = *pp)) continue;
		*pp = t->sig;
		for (u = 0; u < UNIDADES; u++) {
			if (C->tex.lig2d[u] == t) C->tex.lig2d[u] = NULL;
			if (C->tex.lig1d[u] == t) C->tex.lig1d[u] = NULL;
		}
		suelta_niveles(t);
		hgl_soltar(t);
	}
}
GLboolean APIENTRY glIsTexture(GLuint n) { CTX(0); Tex *t = n ? hgl_tex_busca(C->com, n, 0) : NULL; return t && t->dim; }
GLboolean APIENTRY glAreTexturesResident(GLsizei n, const GLuint *t, GLboolean *r) { int k; (void)t; for (k = 0; k < n; k++) r[k] = 1; return 1; }
void APIENTRY glPrioritizeTextures(GLsizei n, const GLuint *v, const GLclampf *p)
{
	CTX();
	int k;
	for (k = 0; k < n; k++) { Tex *t = hgl_tex_busca(C->com, v[k], 0); if (t) t->prioridad = hgl_lim01(p[k]); }
}

/* ---- leer los píxeles del programa a RGBA de 8 bits ---- */
static int comps(u32 f)
{
	switch (f) {
	case GL_RGB: case GL_BGR_EXT: return 3;
	case GL_RGBA: case GL_BGRA_EXT: return 4;
	case GL_LUMINANCE_ALPHA: return 2;
	case GL_RED: case GL_GREEN: case GL_BLUE: case GL_ALPHA: case GL_LUMINANCE: case GL_COLOR_INDEX:
	case GL_DEPTH_COMPONENT: case GL_STENCIL_INDEX: return 1;
	}
	return 0;
}
static int tam_t(u32 t)
{
	switch (t) {
	case GL_UNSIGNED_BYTE: case GL_BYTE: return 1;
	case GL_UNSIGNED_SHORT: case GL_SHORT: return 2;
	case GL_UNSIGNED_INT: case GL_INT: case GL_FLOAT: return 4;
	}
	return 0;
}
static int comp8(const u8 *p, u32 t, int k)
{
	switch (t) {
	case GL_UNSIGNED_BYTE: return p[k];
	case GL_BYTE: { int v = ((const i8 *)p)[k]; return v < 0 ? 0 : v * 2 + (v >> 6); }
	case GL_UNSIGNED_SHORT: return ((const u16 *)p)[k] >> 8;
	case GL_SHORT: { int v = ((const i16 *)p)[k]; return v < 0 ? 0 : v >> 7; }
	case GL_UNSIGNED_INT: return ((const u32 *)p)[k] >> 24;
	case GL_INT: { int v = ((const i32 *)p)[k]; return v < 0 ? 0 : v >> 23; }
	case GL_FLOAT: { float f = ((const float *)p)[k]; return f <= 0 ? 0 : f >= 1 ? 255 : hgl_red(f * 255); }
	}
	return 0;
}
/* w×h puntos de «p» (formato y tipo de OpenGL, con el almacén «a») a «rgba»
 * (w×h×4). GL_COLOR_INDEX pasa el índice en R (la paleta va aparte) */
void hgl_lee_pix(Ctx *C, const Almacen *a, int w, int h, u32 formato, u32 tipo, const void *p, u8 *rgba)
{
	int nc = comps(formato), tb = tam_t(tipo), x, y, fila, paso;
	const u8 *b = p;
	(void)C;
	if (!nc || !tb) { hgl_pon(rgba, 0, (u32)w * h * 4); return; }
	fila = a->fila > 0 ? a->fila : w;
	paso = fila * nc * tb;
	if (tb < a->alin) paso = (paso + a->alin - 1) / a->alin * a->alin;
	b += a->salta_f * paso + a->salta_p * nc * tb;
	for (y = 0; y < h; y++) {
		const u8 *q = b + y * paso;
		u8 *o = rgba + y * w * 4;
		for (x = 0; x < w; x++, q += nc * tb, o += 4) {
			int c0 = comp8(q, tipo, 0), c1 = nc > 1 ? comp8(q, tipo, 1) : 0, c2 = nc > 2 ? comp8(q, tipo, 2) : 0, c3 = nc > 3 ? comp8(q, tipo, 3) : 255;
			switch (formato) {
			case GL_RGB: case GL_RGBA: o[0] = c0; o[1] = c1; o[2] = c2; o[3] = c3; break;
			case GL_BGR_EXT: case GL_BGRA_EXT: o[0] = c2; o[1] = c1; o[2] = c0; o[3] = c3; break;
			case GL_LUMINANCE: o[0] = o[1] = o[2] = c0; o[3] = 255; break;
			case GL_LUMINANCE_ALPHA: o[0] = o[1] = o[2] = c0; o[3] = c1; break;
			case GL_ALPHA: o[0] = o[1] = o[2] = 0; o[3] = c0; break;
			case GL_RED: o[0] = c0; o[1] = o[2] = 0; o[3] = 255; break;
			case GL_GREEN: o[1] = c0; o[0] = o[2] = 0; o[3] = 255; break;
			case GL_BLUE: o[2] = c0; o[0] = o[1] = 0; o[3] = 255; break;
			case GL_COLOR_INDEX: o[0] = tipo == GL_UNSIGNED_BYTE ? q[0] : c0; o[1] = o[2] = 0; o[3] = 255; break;
			default: o[0] = o[1] = o[2] = 0; o[3] = 255;
			}
		}
	}
}

/* ---- el formato de dentro ---- */
static int base_de(int f)
{
	switch (f) {
	case 1: case GL_LUMINANCE: case GL_LUMINANCE4: case GL_LUMINANCE8: case GL_LUMINANCE12: case GL_LUMINANCE16: return GL_LUMINANCE;
	case 2: case GL_LUMINANCE_ALPHA: case GL_LUMINANCE4_ALPHA4: case GL_LUMINANCE6_ALPHA2: case GL_LUMINANCE8_ALPHA8:
	case GL_LUMINANCE12_ALPHA4: case GL_LUMINANCE12_ALPHA12: case GL_LUMINANCE16_ALPHA16: return GL_LUMINANCE_ALPHA;
	case 3: case GL_RGB: case GL_R3_G3_B2: case GL_RGB4: case GL_RGB5: case GL_RGB8: case GL_RGB10: case GL_RGB12: case GL_RGB16: return GL_RGB;
	case 4: case GL_RGBA: case GL_RGBA2: case GL_RGBA4: case GL_RGB5_A1: case GL_RGBA8: case GL_RGB10_A2: case GL_RGBA12: case GL_RGBA16:
	case GL_COLOR_INDEX8_EXT: case GL_COLOR_INDEX: return GL_RGBA;
	case GL_ALPHA: case GL_ALPHA4: case GL_ALPHA8: case GL_ALPHA12: case GL_ALPHA16: return GL_ALPHA;
	case GL_INTENSITY: case GL_INTENSITY4: case GL_INTENSITY8: case GL_INTENSITY12: case GL_INTENSITY16: return GL_INTENSITY;
	}
	return 0;
}
/* RGBA de 8 bits → lo que guarda la textura según su base (OpenGL toma R para L e I) */
static void a_base(u8 *o, int base)
{
	switch (base) {
	case GL_RGB: o[3] = 255; break;
	case GL_LUMINANCE: o[1] = o[2] = o[0]; o[3] = 255; break;
	case GL_LUMINANCE_ALPHA: o[1] = o[2] = o[0]; break;
	case GL_ALPHA: o[0] = o[1] = o[2] = 255; break;
	case GL_INTENSITY: o[1] = o[2] = o[3] = o[0]; break;
	}
}
static int c5(int v) { return (v * 31 + 127) / 255; }
static int c6(int v) { return (v * 63 + 127) / 255; }
static int c4(int v) { return (v * 15 + 127) / 255; }
static u16 empaca(const u8 *o, int fmt)
{
	switch (fmt) {
	case TF_565: return (u16)((c5(o[0]) << 11) | (c6(o[1]) << 5) | c5(o[2]));
	case TF_1555: return (u16)((o[3] >= 128 ? 0x8000 : 0) | (c5(o[0]) << 10) | (c5(o[1]) << 5) | c5(o[2]));
	default: return (u16)((c4(o[3]) << 12) | (c4(o[0]) << 8) | (c4(o[1]) << 4) | c4(o[2]));
	}
}
static void desempaca(u16 p, int fmt, u8 *o)
{
	int r, g, b;
	switch (fmt) {
	case TF_565: r = p >> 11; g = (p >> 5) & 63; b = p & 31;
		o[0] = (u8)((r << 3) | (r >> 2)); o[1] = (u8)((g << 2) | (g >> 4)); o[2] = (u8)((b << 3) | (b >> 2)); o[3] = 255; break;
	case TF_1555: r = (p >> 10) & 31; g = (p >> 5) & 31; b = p & 31;
		o[0] = (u8)((r << 3) | (r >> 2)); o[1] = (u8)((g << 3) | (g >> 2)); o[2] = (u8)((b << 3) | (b >> 2)); o[3] = p & 0x8000 ? 255 : 0; break;
	default:
		o[3] = (u8)((p >> 12) * 17); o[0] = (u8)(((p >> 8) & 15) * 17); o[1] = (u8)(((p >> 4) & 15) * 17); o[2] = (u8)((p & 15) * 17);
	}
}
/* el formato que pide «rgba» (n puntos) para la base: 0 565, 1 1555, 2 4444 */
static int fmt_para(const u8 *rgba, int n, int base)
{
	int k, f = TF_565;
	if (base == GL_RGB || base == GL_LUMINANCE) return TF_565;
	if (base == GL_INTENSITY || base == GL_ALPHA) {
		/* el alfa de verdad: 1555 si es todo o nada, 4444 si no */
		f = TF_1555;
		for (k = 0; k < n; k++) if (rgba[k * 4 + 3] != 0 && rgba[k * 4 + 3] != 255) return TF_4444;
		return f;
	}
	for (k = 0; k < n; k++) {
		int a = rgba[k * 4 + 3];
		if (a == 255) continue;
		if (a == 0) f = TF_1555;
		else return TF_4444;
	}
	return f;
}
static int mas_fmt(int a, int b) { return a > b ? a : b; }   /* 565 < 1555 < 4444 */
static void pasa_a(Tex *t, int fmt)
{
	int k, i;
	u8 o[4];
	if (fmt == t->fmt) return;
	for (k = 0; k < NIVELES; k++) {
		Nivel *n = &t->niv[k];
		if (!n->p) continue;
		for (i = 0; i < n->w * n->h; i++) { desempaca(n->p[i], t->fmt, o); n->p[i] = empaca(o, fmt); }
	}
	t->fmt = fmt;
}
static int log2i(int v) { int k = 0; while ((1 << k) < v) k++; return (1 << k) == v ? k : -1; }

/* la imagen ya en RGBA de 8 bits (y pasada a la base) a un nivel */
static void guarda_nivel(Ctx *C, Tex *t, int nivel, int w, int h, u8 *rgba, int intfmt)
{
	Nivel *n = &t->niv[nivel];
	int base = base_de(intfmt), k, f, vacia = 1;
	for (k = 0; k < NIVELES; k++) if (k != nivel && t->niv[k].p) vacia = 0;
	for (k = 0; k < w * h; k++) a_base(rgba + k * 4, base);
	f = fmt_para(rgba, w * h, base);
	if (vacia || t->base != base) { t->fmt = f; t->base = base; t->intfmt = intfmt; }
	else if (f > t->fmt) pasa_a(t, mas_fmt(f, t->fmt));
	if (nivel == 0) { t->intfmt = intfmt; t->base = base; }
	if (n->p && (n->w != w || n->h != h)) { hgl_soltar(n->p); n->p = NULL; }
	if (!n->p) n->p = hgl_pedir((u32)w * h * 2);
	if (!n->p) { hgl_error(C, GL_OUT_OF_MEMORY); n->w = n->h = 0; return; }
	n->w = w; n->h = h; n->lw = log2i(w); n->lh = log2i(h);
	for (k = 0; k < w * h; k++) n->p[k] = empaca(rgba + k * 4, t->fmt);
	t->estado = 0;
}

static const u8 *paleta_de(Ctx *C, Tex *t)
{
	if (C->com->hay_paleta) return C->com->paleta;
	return t->paleta;
}
static void indices_a_rgba(const u8 *pal, u8 *rgba, int n)
{
	int k;
	for (k = 0; k < n; k++) {
		int i = rgba[k * 4];
		if (pal) { rgba[k * 4] = pal[i * 4]; rgba[k * 4 + 1] = pal[i * 4 + 1]; rgba[k * 4 + 2] = pal[i * 4 + 2]; rgba[k * 4 + 3] = pal[i * 4 + 3]; }
		else { rgba[k * 4 + 1] = rgba[k * 4 + 2] = (u8)i; rgba[k * 4 + 3] = 255; }
	}
}

static void tex_imagen(Ctx *C, u32 dest, int nivel, int intfmt, int w, int h, int borde, u32 formato, u32 tipo, const void *pix, int dim)
{
	Tex *t = ligada(C, dest);
	u8 *rgba;
	if (!t) return;
	if (!base_de(intfmt)) { hgl_error(C, GL_INVALID_VALUE); return; }
	if (nivel < 0 || nivel >= NIVELES || w < 0 || h < 0 || borde < 0 || borde > 1) { hgl_error(C, GL_INVALID_VALUE); return; }
	if (pix && (!comps(formato) || !tam_t(tipo))) { hgl_error(C, GL_INVALID_ENUM); return; }
	w -= 2 * borde;
	h = dim == 2 ? h - 2 * borde : 1;
	if (w < 0 || h < 0 || (w && log2i(w) < 0) || (h && log2i(h) < 0) || w > TAM_MAX || h > TAM_MAX) {
		if (dest == GL_PROXY_TEXTURE_2D || dest == GL_PROXY_TEXTURE_1D) { t->niv[nivel].w = t->niv[nivel].h = 0; return; }
		hgl_error(C, GL_INVALID_VALUE);
		return;
	}
	if (dest == GL_PROXY_TEXTURE_2D || dest == GL_PROXY_TEXTURE_1D) {
		t->niv[nivel].w = w; t->niv[nivel].h = h; t->intfmt = intfmt;
		return;
	}
	if (!w || !h) {
		hgl_soltar(t->niv[nivel].p); t->niv[nivel].p = NULL; t->niv[nivel].w = t->niv[nivel].h = 0; t->estado = 0;
		return;
	}
	rgba = hgl_pedir((u32)(w + 2 * borde) * (h + 2 * borde) * 4);
	if (!rgba) { hgl_error(C, GL_OUT_OF_MEMORY); return; }
	if (pix) {
		int ww = w + 2 * borde, hh = dim == 2 ? h + 2 * borde : 1;
		hgl_lee_pix(C, &C->des, ww, hh, formato, tipo, pix, rgba);
		if (borde) {    /* sin el borde (no se usa: GL_CLAMP es GL_CLAMP_TO_EDGE) */
			int y;
			for (y = 0; y < h; y++) hgl_copia(rgba + y * w * 4, rgba + ((y + (dim == 2)) * ww + 1) * 4, (u32)w * 4);
		}
		if (formato == GL_COLOR_INDEX) indices_a_rgba(paleta_de(C, t), rgba, w * h);
	} else hgl_pon(rgba, 0, (u32)w * h * 4);
	guarda_nivel(C, t, nivel, w, h, rgba, intfmt);
	hgl_soltar(rgba);
}

/* en una lista: la imagen se guarda ya leída (RGBA de 8 bits) con lo demás delante */
static void i_tex_lista(const u32 *d)
{
	CTX();
	Tex *t = ligada(C, d[0]);
	int w = (int)d[3], h = (int)d[4];
	u8 *rgba;
	if (!t) return;
	if (!w || !h) { tex_imagen(C, d[0], (int)d[1], (int)d[2], w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL, (int)d[5]); return; }
	rgba = hgl_pedir((u32)w * h * 4);
	if (!rgba) { hgl_error(C, GL_OUT_OF_MEMORY); return; }
	hgl_copia(rgba, d + 6, (u32)w * h * 4);
	guarda_nivel(C, t, (int)d[1], w, h, rgba, (int)d[2]);
	hgl_soltar(rgba);
}
static int graba_imagen(Ctx *C, u32 dest, int nivel, int fi, int w, int h, int borde, u32 f, u32 tipo, const void *p, int dim)
{
	u32 *d;
	Tex *t;
	if (!C->lista_n) return 0;
	if (dest == GL_PROXY_TEXTURE_1D || dest == GL_PROXY_TEXTURE_2D) {
		hgl_error(C, GL_INVALID_OPERATION);
		return C->lista_modo == GL_COMPILE;
	}
	if (dim == 1) h = 1;
	w -= 2 * borde; if (dim == 2) h -= 2 * borde;
	if (w < 0 || h < 0 || w > TAM_MAX || h > TAM_MAX) return 0;
	d = hgl_pedir((u32)w * h * 4 + 24);
	if (!d) { hgl_error(C, GL_OUT_OF_MEMORY); return 1; }
	d[0] = dest; d[1] = nivel; d[2] = fi; d[3] = w; d[4] = h; d[5] = dim;
	if (p && w && h) {
		int ww = w + 2 * borde, hh = dim == 2 ? h + 2 * borde : 1, y;
		u8 *r = (u8 *)(d + 6), *tmp = hgl_pedir((u32)ww * hh * 4);
		if (tmp) {
			hgl_lee_pix(C, &C->des, ww, hh, f, tipo, p, tmp);
			for (y = 0; y < h; y++) hgl_copia(r + y * w * 4, tmp + ((y + (borde && dim == 2)) * ww + borde) * 4, (u32)w * 4);
			hgl_soltar(tmp);
			t = ligada(C, dest);
			if (f == GL_COLOR_INDEX && t) indices_a_rgba(paleta_de(C, t), r, w * h);
		}
	}
	hgl_graba_v(C, (void *)i_tex_lista, d, (u32)w * h + 6);
	hgl_soltar(d);
	return C->lista_modo == GL_COMPILE;
}
void APIENTRY glTexImage2D(GLenum d, GLint n, GLint fi, GLsizei w, GLsizei h, GLint b, GLenum f, GLenum t, const GLvoid *p)
{
	CTX();
	if (graba_imagen(C, d, n, fi, w, h, b, f, t, p, 2)) return;
	tex_imagen(C, d, n, fi, w, h, b, f, t, p, 2);
}
void APIENTRY glTexImage1D(GLenum d, GLint n, GLint fi, GLsizei w, GLint b, GLenum f, GLenum t, const GLvoid *p)
{
	CTX();
	if (graba_imagen(C, d, n, fi, w, 1, b, f, t, p, 1)) return;
	tex_imagen(C, d, n, fi, w, 1, b, f, t, p, 1);
}

/* un trozo de un nivel */
static void sub_rgba(Ctx *C, Tex *t, int nivel, int x0, int y0, int w, int h, u8 *rgba)
{
	Nivel *n;
	int x, y, k, f;
	if (nivel < 0 || nivel >= NIVELES || !t->niv[nivel].p) { hgl_error(C, GL_INVALID_OPERATION); return; }
	n = &t->niv[nivel];
	if (x0 < 0 || y0 < 0 || x0 + w > n->w || y0 + h > n->h) { hgl_error(C, GL_INVALID_VALUE); return; }
	for (k = 0; k < w * h; k++) a_base(rgba + k * 4, t->base);
	f = fmt_para(rgba, w * h, t->base);
	if (f > t->fmt) pasa_a(t, f);
	for (y = 0; y < h; y++)
		for (x = 0; x < w; x++) n->p[(y0 + y) * n->w + x0 + x] = empaca(rgba + (y * w + x) * 4, t->fmt);
}
static void i_sub_lista(const u32 *d)
{
	CTX();
	Tex *t = ligada(C, d[0]);
	int w = (int)d[4], h = (int)d[5];
	u8 *rgba;
	if (!t) return;
	rgba = hgl_pedir((u32)w * h * 4 + 4);
	if (!rgba) return;
	hgl_copia(rgba, d + 6, (u32)w * h * 4);
	sub_rgba(C, t, (int)d[1], (int)d[2], (int)d[3], w, h, rgba);
	hgl_soltar(rgba);
}
static void tex_sub(Ctx *C, u32 dest, int nivel, int x0, int y0, int w, int h, u32 f, u32 tipo, const void *p)
{
	Tex *t = ligada(C, dest);
	u32 *d;
	u8 *rgba;
	if (!t || w <= 0 || h <= 0) return;
	if (!comps(f) || !tam_t(tipo)) { hgl_error(C, GL_INVALID_ENUM); return; }
	d = hgl_pedir((u32)w * h * 4 + 24);
	if (!d) { hgl_error(C, GL_OUT_OF_MEMORY); return; }
	rgba = (u8 *)(d + 6);
	hgl_lee_pix(C, &C->des, w, h, f, tipo, p, rgba);
	if (f == GL_COLOR_INDEX) indices_a_rgba(paleta_de(C, t), rgba, w * h);
	if (C->lista_n) {
		d[0] = dest; d[1] = nivel; d[2] = x0; d[3] = y0; d[4] = w; d[5] = h;
		hgl_graba_v(C, (void *)i_sub_lista, d, (u32)w * h + 6);
		if (C->lista_modo == GL_COMPILE) { hgl_soltar(d); return; }
	}
	sub_rgba(C, t, nivel, x0, y0, w, h, rgba);
	hgl_soltar(d);
}
void APIENTRY glTexSubImage2D(GLenum d, GLint n, GLint x, GLint y, GLsizei w, GLsizei h, GLenum f, GLenum t, const GLvoid *p) { CTX(); tex_sub(C, d, n, x, y, w, h, f, t, p); }
void APIENTRY glTexSubImage1D(GLenum d, GLint n, GLint x, GLsizei w, GLenum f, GLenum t, const GLvoid *p) { CTX(); tex_sub(C, d, n, x, 0, w, 1, f, t, p); }

/* del búfer de color (las coordenadas de OpenGL: y hacia arriba) a RGBA */
static void lee_color(Ctx *C, int x, int y, int w, int h, u8 *rgba)
{
	int i, j;
	for (j = 0; j < h; j++) {
		int yy = C->al - 1 - (y + j);
		for (i = 0; i < w; i++) {
			int xx = x + i;
			u8 *o = rgba + (j * w + i) * 4;
			if (xx < 0 || yy < 0 || xx >= C->an || yy >= C->al || !C->color) { o[0] = o[1] = o[2] = 0; o[3] = 255; continue; }
			{
				u16 p = C->color[yy * C->an + xx];
				int r = p >> 11, g = (p >> 5) & 63, b = p & 31;
				o[0] = (u8)((r << 3) | (r >> 2)); o[1] = (u8)((g << 2) | (g >> 4)); o[2] = (u8)((b << 3) | (b >> 2)); o[3] = 255;
			}
		}
	}
}
static void i_copia_tex(u32 dest, int nivel, int fi, int x, int y, int w, int h, int dim)
{
	CTX();
	Tex *t = ligada(C, dest);
	u8 *rgba;
	if (!t) return;
	if (!base_de(fi) || w <= 0 || h <= 0 || log2i(w) < 0 || log2i(h) < 0 || w > TAM_MAX || h > TAM_MAX) { hgl_error(C, GL_INVALID_VALUE); return; }
	rgba = hgl_pedir((u32)w * h * 4);
	if (!rgba) { hgl_error(C, GL_OUT_OF_MEMORY); return; }
	lee_color(C, x, y, w, h, rgba);
	guarda_nivel(C, t, nivel, w, h, rgba, fi);
	(void)dim;
	hgl_soltar(rgba);
}
void APIENTRY glCopyTexImage2D(GLenum d, GLint n, GLenum fi, GLint x, GLint y, GLsizei w, GLsizei h, GLint b)
{
	CTX(); (void)b;
	LISTA(i_copia_tex, 8, d, n, fi, x, y, w, h, 2);
	i_copia_tex(d, n, fi, x, y, w, h, 2);
}
void APIENTRY glCopyTexImage1D(GLenum d, GLint n, GLenum fi, GLint x, GLint y, GLsizei w, GLint b)
{
	CTX(); (void)b;
	LISTA(i_copia_tex, 8, d, n, fi, x, y, w, 1, 1);
	i_copia_tex(d, n, fi, x, y, w, 1, 1);
}
static void i_copia_sub(u32 dest, int nivel, int xo, int yo, int x, int y, int w, int h)
{
	CTX();
	Tex *t = ligada(C, dest);
	u8 *rgba;
	if (!t || w <= 0 || h <= 0) return;
	rgba = hgl_pedir((u32)w * h * 4);
	if (!rgba) { hgl_error(C, GL_OUT_OF_MEMORY); return; }
	lee_color(C, x, y, w, h, rgba);
	sub_rgba(C, t, nivel, xo, yo, w, h, rgba);
	hgl_soltar(rgba);
}
void APIENTRY glCopyTexSubImage2D(GLenum d, GLint n, GLint xo, GLint yo, GLint x, GLint y, GLsizei w, GLsizei h)
{
	CTX();
	LISTA(i_copia_sub, 8, d, n, xo, yo, x, y, w, h);
	i_copia_sub(d, n, xo, yo, x, y, w, h);
}
void APIENTRY glCopyTexSubImage1D(GLenum d, GLint n, GLint xo, GLint x, GLint y, GLsizei w)
{
	CTX();
	LISTA(i_copia_sub, 8, d, n, xo, 0, x, y, w, 1);
	i_copia_sub(d, n, xo, 0, x, y, w, 1);
}

void APIENTRY glGetTexImage(GLenum d, GLint nivel, GLenum f, GLenum tipo, GLvoid *p)
{
	CTX();
	Tex *t = ligada(C, d);
	Nivel *n;
	int k, nc = comps(f);
	u8 o[4];
	if (!t || nivel < 0 || nivel >= NIVELES || !t->niv[nivel].p) return;
	if (tipo != GL_UNSIGNED_BYTE || !nc) { hgl_error(C, GL_INVALID_ENUM); return; }
	n = &t->niv[nivel];
	for (k = 0; k < n->w * n->h; k++) {
		u8 *q = (u8 *)p + k * nc;
		desempaca(n->p[k], t->fmt, o);
		switch (f) {
		case GL_RGBA: q[0] = o[0]; q[1] = o[1]; q[2] = o[2]; q[3] = o[3]; break;
		case GL_RGB: q[0] = o[0]; q[1] = o[1]; q[2] = o[2]; break;
		case GL_BGRA_EXT: q[0] = o[2]; q[1] = o[1]; q[2] = o[0]; q[3] = o[3]; break;
		case GL_BGR_EXT: q[0] = o[2]; q[1] = o[1]; q[2] = o[0]; break;
		case GL_LUMINANCE: case GL_RED: q[0] = o[0]; break;
		case GL_GREEN: q[0] = o[1]; break;
		case GL_BLUE: q[0] = o[2]; break;
		case GL_ALPHA: q[0] = o[3]; break;
		case GL_LUMINANCE_ALPHA: q[0] = o[0]; q[1] = o[3]; break;
		}
	}
}

/* ---- los parámetros ---- */
static void i_tex_param(u32 dest, u32 p, float a, float b, float c, float d)
{
	CTX();
	Tex *t = ligada(C, dest);
	int v = (int)a;
	if (!t) return;
	switch (p) {
	case GL_TEXTURE_MIN_FILTER:
		if (v != GL_NEAREST && v != GL_LINEAR && v != GL_NEAREST_MIPMAP_NEAREST && v != GL_LINEAR_MIPMAP_NEAREST &&
		    v != GL_NEAREST_MIPMAP_LINEAR && v != GL_LINEAR_MIPMAP_LINEAR) { hgl_error(C, GL_INVALID_ENUM); return; }
		t->minf = v; t->estado = 0; break;
	case GL_TEXTURE_MAG_FILTER:
		if (v != GL_NEAREST && v != GL_LINEAR) { hgl_error(C, GL_INVALID_ENUM); return; }
		t->magf = v; break;
	case GL_TEXTURE_WRAP_S: case GL_TEXTURE_WRAP_T:
		if (v != GL_REPEAT && v != GL_CLAMP && v != GL_CLAMP_TO_EDGE) { hgl_error(C, GL_INVALID_ENUM); return; }
		if (p == GL_TEXTURE_WRAP_S) t->ws = v; else t->wt = v;
		break;
	case GL_TEXTURE_BORDER_COLOR: t->borde[0] = a; t->borde[1] = b; t->borde[2] = c; t->borde[3] = d; break;
	case GL_TEXTURE_PRIORITY: t->prioridad = hgl_lim01(a); break;
	default: break;     /* (las de otras versiones: se aceptan) */
	}
}
void APIENTRY glTexParameterf(GLenum d, GLenum p, GLfloat v) { CTX(); LISTA(i_tex_param, 6, d, p, F(v), 0, 0, 0); i_tex_param(d, p, v, 0, 0, 0); }
void APIENTRY glTexParameteri(GLenum d, GLenum p, GLint v) { glTexParameterf(d, p, (float)v); }
void APIENTRY glTexParameterfv(GLenum d, GLenum p, const GLfloat *v)
{
	CTX();
	float a = v[0], b = 0, c = 0, e = 0;
	if (p == GL_TEXTURE_BORDER_COLOR) { b = v[1]; c = v[2]; e = v[3]; }
	LISTA(i_tex_param, 6, d, p, F(a), F(b), F(c), F(e));
	i_tex_param(d, p, a, b, c, e);
}
void APIENTRY glTexParameteriv(GLenum d, GLenum p, const GLint *v)
{
	float f[4] = { (float)v[0], 0, 0, 0 };
	if (p == GL_TEXTURE_BORDER_COLOR) { int k; for (k = 0; k < 4; k++) f[k] = (float)v[k] / 2147483647.0f; }
	glTexParameterfv(d, p, f);
}
void APIENTRY glGetTexParameterfv(GLenum d, GLenum p, GLfloat *v)
{
	CTX();
	Tex *t = ligada(C, d);
	if (!t) return;
	switch (p) {
	case GL_TEXTURE_MIN_FILTER: v[0] = t->minf; break;
	case GL_TEXTURE_MAG_FILTER: v[0] = t->magf; break;
	case GL_TEXTURE_WRAP_S: v[0] = t->ws; break;
	case GL_TEXTURE_WRAP_T: v[0] = t->wt; break;
	case GL_TEXTURE_BORDER_COLOR: hgl_copia(v, t->borde, 16); break;
	case GL_TEXTURE_PRIORITY: v[0] = t->prioridad; break;
	case GL_TEXTURE_RESIDENT: v[0] = 1; break;
	default: hgl_error(C, GL_INVALID_ENUM);
	}
}
void APIENTRY glGetTexParameteriv(GLenum d, GLenum p, GLint *v)
{
	float f[4] = { 0, 0, 0, 0 };
	glGetTexParameterfv(d, p, f);
	v[0] = hgl_red(f[0]);
	if (p == GL_TEXTURE_BORDER_COLOR) { int k; for (k = 0; k < 4; k++) v[k] = (int)(f[k] * 2147483647.0f); }
}
void APIENTRY glGetTexLevelParameteriv(GLenum d, GLint nivel, GLenum p, GLint *v)
{
	CTX();
	Tex *t = ligada(C, d);
	Nivel *n;
	int r = 0, g = 0, b = 0, a = 0, l = 0, i = 0;
	if (!t) return;
	if (nivel < 0 || nivel >= NIVELES) { hgl_error(C, GL_INVALID_VALUE); return; }
	n = &t->niv[nivel];
	switch (t->fmt) { case TF_565: r = b = 5; g = 6; break; case TF_1555: r = g = b = 5; a = 1; break; default: r = g = b = a = 4; }
	if (t->base == GL_LUMINANCE || t->base == GL_LUMINANCE_ALPHA) { l = r; r = g = b = 0; }
	if (t->base == GL_INTENSITY) { i = a; r = g = b = a = 0; }
	if (t->base == GL_ALPHA) r = g = b = 0;
	if (t->base == GL_RGB || t->base == GL_LUMINANCE) a = 0;
	switch (p) {
	case GL_TEXTURE_WIDTH: v[0] = n->w; break;
	case GL_TEXTURE_HEIGHT: v[0] = n->h; break;
	case GL_TEXTURE_COMPONENTS: v[0] = t->intfmt; break;
	case GL_TEXTURE_BORDER: v[0] = 0; break;
	case GL_TEXTURE_RED_SIZE: v[0] = n->w ? r : 0; break;
	case GL_TEXTURE_GREEN_SIZE: v[0] = n->w ? g : 0; break;
	case GL_TEXTURE_BLUE_SIZE: v[0] = n->w ? b : 0; break;
	case GL_TEXTURE_ALPHA_SIZE: v[0] = n->w ? a : 0; break;
	case GL_TEXTURE_LUMINANCE_SIZE: v[0] = n->w ? l : 0; break;
	case GL_TEXTURE_INTENSITY_SIZE: v[0] = n->w ? i : 0; break;
	default: hgl_error(C, GL_INVALID_ENUM);
	}
}
void APIENTRY glGetTexLevelParameterfv(GLenum d, GLint nivel, GLenum p, GLfloat *v) { GLint i = 0; glGetTexLevelParameteriv(d, nivel, p, &i); v[0] = (float)i; }

/* ---- glTexEnv ---- */
static void i_tex_env(u32 p, float a, float b, float c, float d)
{
	CTX();
	int u = C->tex.activa, v = (int)a;
	if (p == GL_TEXTURE_ENV_MODE) {
		if (v != GL_MODULATE && v != GL_DECAL && v != GL_BLEND && v != GL_REPLACE && v != GL_ADD) { hgl_error(C, GL_INVALID_ENUM); return; }
		C->tex.env[u] = v;
	} else if (p == GL_TEXTURE_ENV_COLOR) {
		C->tex.env_col[u][0] = hgl_lim01(a); C->tex.env_col[u][1] = hgl_lim01(b);
		C->tex.env_col[u][2] = hgl_lim01(c); C->tex.env_col[u][3] = hgl_lim01(d);
	}
}
void APIENTRY glTexEnvf(GLenum d, GLenum p, GLfloat v) { CTX(); (void)d; LISTA(i_tex_env, 5, p, F(v), 0, 0, 0); i_tex_env(p, v, 0, 0, 0); }
void APIENTRY glTexEnvi(GLenum d, GLenum p, GLint v) { glTexEnvf(d, p, (float)v); }
void APIENTRY glTexEnvfv(GLenum d, GLenum p, const GLfloat *v)
{
	CTX();
	float a = v[0], b = 0, c = 0, e = 0;
	(void)d;
	if (p == GL_TEXTURE_ENV_COLOR) { b = v[1]; c = v[2]; e = v[3]; }
	LISTA(i_tex_env, 5, p, F(a), F(b), F(c), F(e));
	i_tex_env(p, a, b, c, e);
}
void APIENTRY glTexEnviv(GLenum d, GLenum p, const GLint *v)
{
	float f[4] = { (float)v[0], 0, 0, 0 };
	if (p == GL_TEXTURE_ENV_COLOR) { int k; for (k = 0; k < 4; k++) f[k] = (float)v[k] / 2147483647.0f; }
	glTexEnvfv(d, p, f);
}
void APIENTRY glGetTexEnvfv(GLenum d, GLenum p, GLfloat *v)
{
	CTX();
	(void)d;
	if (p == GL_TEXTURE_ENV_MODE) v[0] = C->tex.env[C->tex.activa];
	else if (p == GL_TEXTURE_ENV_COLOR) hgl_copia(v, C->tex.env_col[C->tex.activa], 16);
	else hgl_error(C, GL_INVALID_ENUM);
}
void APIENTRY glGetTexEnviv(GLenum d, GLenum p, GLint *v)
{
	float f[4] = { 0, 0, 0, 0 };
	glGetTexEnvfv(d, p, f);
	v[0] = hgl_red(f[0]);
	if (p == GL_TEXTURE_ENV_COLOR) { int k; for (k = 0; k < 4; k++) v[k] = (int)(f[k] * 2147483647.0f); }
}

/* ---- glTexGen ---- */
static void i_tex_gen(u32 coord, u32 p, float a, float b, float c, float d)
{
	CTX();
	int k = coord == GL_S ? 0 : coord == GL_T ? 1 : coord == GL_R ? 2 : coord == GL_Q ? 3 : -1;
	TexGen *g;
	if (k < 0) { hgl_error(C, GL_INVALID_ENUM); return; }
	g = &C->tex.gen[C->tex.activa][k];
	switch (p) {
	case GL_TEXTURE_GEN_MODE:
		if ((int)a != GL_OBJECT_LINEAR && (int)a != GL_EYE_LINEAR && (int)a != GL_SPHERE_MAP) { hgl_error(C, GL_INVALID_ENUM); return; }
		g->modo = (int)a;
		break;
	case GL_OBJECT_PLANE: g->obj[0] = a; g->obj[1] = b; g->obj[2] = c; g->obj[3] = d; break;
	case GL_EYE_PLANE: {
		Mat inv;
		float v[4] = { a, b, c, d };
		int i;
		if (!mat_inversa(&inv, &C->mv[C->nmv])) mat_identidad(&inv);
		for (i = 0; i < 4; i++) g->ojo[i] = v[0] * inv.m[i * 4] + v[1] * inv.m[i * 4 + 1] + v[2] * inv.m[i * 4 + 2] + v[3] * inv.m[i * 4 + 3];
		break;
	}
	default: hgl_error(C, GL_INVALID_ENUM); return;
	}
	hgl_estado_cambia(C);
}
void APIENTRY glTexGenfv(GLenum c, GLenum p, const GLfloat *v)
{
	CTX();
	float a = v[0], b = 0, d = 0, e = 0;
	if (p != GL_TEXTURE_GEN_MODE) { b = v[1]; d = v[2]; e = v[3]; }
	LISTA(i_tex_gen, 6, c, p, F(a), F(b), F(d), F(e));
	i_tex_gen(c, p, a, b, d, e);
}
void APIENTRY glTexGenf(GLenum c, GLenum p, GLfloat v) { float f[4] = { v, 0, 0, 0 }; glTexGenfv(c, p, f); }
void APIENTRY glTexGeni(GLenum c, GLenum p, GLint v) { float f[4] = { (float)v, 0, 0, 0 }; glTexGenfv(c, p, f); }
void APIENTRY glTexGend(GLenum c, GLenum p, GLdouble v) { float f[4] = { (float)v, 0, 0, 0 }; glTexGenfv(c, p, f); }
void APIENTRY glTexGeniv(GLenum c, GLenum p, const GLint *v) { float f[4]; int k; for (k = 0; k < (p == GL_TEXTURE_GEN_MODE ? 1 : 4); k++) f[k] = (float)v[k]; glTexGenfv(c, p, f); }
void APIENTRY glTexGendv(GLenum c, GLenum p, const GLdouble *v) { float f[4]; int k; for (k = 0; k < (p == GL_TEXTURE_GEN_MODE ? 1 : 4); k++) f[k] = (float)v[k]; glTexGenfv(c, p, f); }
void APIENTRY glGetTexGenfv(GLenum c, GLenum p, GLfloat *v)
{
	CTX();
	int k = c == GL_S ? 0 : c == GL_T ? 1 : c == GL_R ? 2 : c == GL_Q ? 3 : -1;
	TexGen *g;
	if (k < 0) { hgl_error(C, GL_INVALID_ENUM); return; }
	g = &C->tex.gen[C->tex.activa][k];
	if (p == GL_TEXTURE_GEN_MODE) v[0] = g->modo;
	else if (p == GL_OBJECT_PLANE) hgl_copia(v, g->obj, 16);
	else if (p == GL_EYE_PLANE) hgl_copia(v, g->ojo, 16);
	else hgl_error(C, GL_INVALID_ENUM);
}
void APIENTRY glGetTexGeniv(GLenum c, GLenum p, GLint *v) { float f[4] = { 0, 0, 0, 0 }; int k; glGetTexGenfv(c, p, f); for (k = 0; k < (p == GL_TEXTURE_GEN_MODE ? 1 : 4); k++) v[k] = hgl_red(f[k]); }
void APIENTRY glGetTexGendv(GLenum c, GLenum p, GLdouble *v) { float f[4] = { 0, 0, 0, 0 }; int k; glGetTexGenfv(c, p, f); for (k = 0; k < (p == GL_TEXTURE_GEN_MODE ? 1 : 4); k++) v[k] = f[k]; }

/* ---- GL_EXT_paletted_texture: la paleta de la textura (o la compartida) ---- */
void APIENTRY glColorTableEXT(GLenum dest, GLenum fi, GLsizei w, GLenum f, GLenum tipo, const GLvoid *tabla)
{
	CTX();
	u8 rgba[256 * 4], *pal;
	int k;
	(void)fi;
	if (w <= 0 || w > 256) { hgl_error(C, GL_INVALID_VALUE); return; }
	hgl_pon(rgba, 0, sizeof rgba);
	{
		Almacen a = C->des;
		a.fila = 0; a.salta_f = a.salta_p = 0;
		hgl_lee_pix(C, &a, w, 1, f, tipo, tabla, rgba);
	}
	for (k = w; k < 256; k++) rgba[k * 4 + 3] = 255;
	if (dest == GL_SHARED_TEXTURE_PALETTE_EXT) { pal = C->com->paleta; C->com->hay_paleta = 1; }
	else {
		Tex *t = ligada(C, dest == GL_TEXTURE_1D ? GL_TEXTURE_1D : GL_TEXTURE_2D);
		if (!t) return;
		if (!t->paleta) t->paleta = hgl_pedir(256 * 4);
		if (!t->paleta) { hgl_error(C, GL_OUT_OF_MEMORY); return; }
		pal = t->paleta;
	}
	hgl_copia(pal, rgba, sizeof rgba);
}
