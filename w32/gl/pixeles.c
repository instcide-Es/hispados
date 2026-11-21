/* pixeles.c - HispaGL: glClear, glReadPixels, glDrawPixels, glCopyPixels,
 * glBitmap y glRasterPos; y los contextos (crear, soltar, el tamaño del
 * búfer).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "hgl.h"

/* ---- los contextos ---- */
Ctx *hgl_nuevo(Ctx *comparte)
{
	Ctx *C = hgl_pedir(sizeof(Ctx));
	if (!C) return NULL;
	if (comparte) { C->com = comparte->com; C->com->usos++; }
	else {
		C->com = hgl_pedir(sizeof(Comun));
		if (!C->com) { hgl_soltar(C); return NULL; }
		C->com->usos = 1;
	}
	{
		Ctx *antes = hgl_c;
		hgl_c = C;
		hgl_texturas_ini(C);
		hgl_matrices_ini(C);
		hgl_estado_ini(C);
		hgl_vertice_ini(C);
		hgl_c = antes;
	}
	C->dobles = 1;
	return C;
}

void hgl_borra(Ctx *C)
{
	if (!C) return;
	if (hgl_c == C) hgl_c = NULL;
	if (--C->com->usos <= 0) {
		hgl_tex_suelta_todo(C->com);
		hgl_listas_suelta(C->com);
		hgl_soltar(C->com);
	}
	hgl_soltar(C->color);
	hgl_soltar(C->prof);
	hgl_soltar(C->pol_v);
	hgl_soltar(C->atr);
	hgl_soltar(C->atrc);
	hgl_soltar(C->lista_cur.d);
	hgl_soltar(C);
}

/* el búfer del tamaño de la ventana: 1 si ha cambiado */
int hgl_tamano(Ctx *C, int an, int al)
{
	int primera = !C->color;
	if (an < 1) an = 1;
	if (al < 1) al = 1;
	if (an > 4096) an = 4096;
	if (al > 4096) al = 4096;
	if (C->color && an == C->an && al == C->al) return 0;
	hgl_soltar(C->color);
	hgl_soltar(C->prof);
	C->color = hgl_pedir((u32)an * al * 2);
	C->prof = hgl_pedir((u32)an * al * 2);
	if (!C->color || !C->prof) {
		hgl_soltar(C->color); hgl_soltar(C->prof);
		C->color = C->prof = NULL;
		C->an = C->al = 0;
		hgl_error(C, GL_OUT_OF_MEMORY);
		return 1;
	}
	hgl_pon(C->prof, 0xFF, (u32)an * al * 2);
	C->an = an;
	C->al = al;
	if (primera) {
		/* la primera vez, la vista y la tijera toman el tamaño de la ventana */
		C->vis.x = C->vis.y = 0; C->vis.w = an; C->vis.h = al;
		C->tij.x = C->tij.y = 0; C->tij.w = an; C->tij.h = al;
	}
	return 1;
}

/* ---- glClear ---- */
static void i_borra(u32 m)
{
	CTX();
	int x0 = 0, y0 = 0, x1 = C->an, y1 = C->al, y;
	if (!C->color) return;
	if (C->prim >= 0) { hgl_error(C, GL_INVALID_OPERATION); return; }
	if (C->hab.tijera) {
		int a = C->tij.x, b = C->tij.x + C->tij.w, c = C->al - (C->tij.y + C->tij.h), d = C->al - C->tij.y;
		if (a > x0) x0 = a;
		if (b < x1) x1 = b;
		if (c > y0) y0 = c;
		if (d < y1) y1 = d;
	}
	if (x0 >= x1 || y0 >= y1) return;
	if (m & GL_COLOR_BUFFER_BIT) {
		u16 v = hgl_565(hgl_red(C->colb.borra[0] * 255), hgl_red(C->colb.borra[1] * 255), hgl_red(C->colb.borra[2] * 255));
		u16 mk = (u16)((C->colb.mascara[0] ? 0xF800 : 0) | (C->colb.mascara[1] ? 0x07E0 : 0) | (C->colb.mascara[2] ? 0x001F : 0));
		for (y = y0; y < y1; y++) {
			u16 *p = C->color + y * C->an + x0;
			int n = x1 - x0;
			if (mk == 0xFFFF) {
				if (((u32)p & 2) && n) { *p++ = v; n--; }
				{
					u32 vv = v | ((u32)v << 16), k = (u32)n >> 1;
					u32 *q = (u32 *)p;
					while (k--) *q++ = vv;
					if (n & 1) *(u16 *)q = v;
				}
			} else while (n--) { *p = (u16)((v & mk) | (*p & ~mk)); p++; }
		}
	}
	if ((m & GL_DEPTH_BUFFER_BIT) && C->prof && C->profb.escribe) {
		u16 v = (u16)hgl_red(C->profb.borra * 65535.0f);
		for (y = y0; y < y1; y++) {
			u16 *p = C->prof + y * C->an + x0;
			int n = x1 - x0;
			while (n--) *p++ = v;
		}
	}
}
void APIENTRY glClear(GLbitfield m) { CTX(); LISTA(i_borra, 1, m); i_borra(m); }

/* ---- glRasterPos ---- */
void i_raster_pos(float x, float y, float z, float w)
{
	CTX();
	Vtx v;
	float o[4] = { x, y, z, w };
	int u;
	hgl_transforma(C, &v, o);
	if (v.cod & 0x3F) { C->cur.rvalida = 0; return; }
	{
		float iw = 1.0f / v.c[3];
		C->cur.rpos[0] = C->vis.x + (v.c[0] * iw + 1) * 0.5f * C->vis.w;
		C->cur.rpos[1] = C->vis.y + (v.c[1] * iw + 1) * 0.5f * C->vis.h;
		C->cur.rpos[2] = C->vis.cerca + (v.c[2] * iw + 1) * 0.5f * (C->vis.lejos - C->vis.cerca);
		C->cur.rpos[3] = v.c[3];
	}
	C->cur.rvalida = 1;
	C->cur.rdist = hgl_abs(v.e[2]);
	for (u = 0; u < 4; u++) C->cur.rcol[u] = v.col[u];
	for (u = 0; u < 4; u++) C->cur.rtc[u] = v.t[0][u];
}
static void __attribute__((noinline)) rp_(float x, float y, float z, float w) { CTX(); LISTA(i_raster_pos, 4, F(x), F(y), F(z), F(w)); i_raster_pos(x, y, z, w); }
#define RP(x, y, z, w) rp_(x, y, z, w)
void APIENTRY glRasterPos2f(GLfloat x, GLfloat y) { RP(x, y, 0.0f, 1.0f); }
void APIENTRY glRasterPos3f(GLfloat x, GLfloat y, GLfloat z) { RP(x, y, z, 1.0f); }
void APIENTRY glRasterPos4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w) { RP(x, y, z, w); }
void APIENTRY glRasterPos2fv(const GLfloat *v) { RP(v[0], v[1], 0.0f, 1.0f); }
void APIENTRY glRasterPos3fv(const GLfloat *v) { RP(v[0], v[1], v[2], 1.0f); }
void APIENTRY glRasterPos4fv(const GLfloat *v) { RP(v[0], v[1], v[2], v[3]); }
void APIENTRY glRasterPos2d(GLdouble x, GLdouble y) { RP((float)x, (float)y, 0.0f, 1.0f); }
void APIENTRY glRasterPos3d(GLdouble x, GLdouble y, GLdouble z) { RP((float)x, (float)y, (float)z, 1.0f); }
void APIENTRY glRasterPos4d(GLdouble x, GLdouble y, GLdouble z, GLdouble w) { RP((float)x, (float)y, (float)z, (float)w); }
void APIENTRY glRasterPos2dv(const GLdouble *v) { RP((float)v[0], (float)v[1], 0.0f, 1.0f); }
void APIENTRY glRasterPos3dv(const GLdouble *v) { RP((float)v[0], (float)v[1], (float)v[2], 1.0f); }
void APIENTRY glRasterPos4dv(const GLdouble *v) { RP((float)v[0], (float)v[1], (float)v[2], (float)v[3]); }
void APIENTRY glRasterPos2i(GLint x, GLint y) { RP((float)x, (float)y, 0.0f, 1.0f); }
void APIENTRY glRasterPos3i(GLint x, GLint y, GLint z) { RP((float)x, (float)y, (float)z, 1.0f); }
void APIENTRY glRasterPos4i(GLint x, GLint y, GLint z, GLint w) { RP((float)x, (float)y, (float)z, (float)w); }
void APIENTRY glRasterPos2iv(const GLint *v) { RP((float)v[0], (float)v[1], 0.0f, 1.0f); }
void APIENTRY glRasterPos3iv(const GLint *v) { RP((float)v[0], (float)v[1], (float)v[2], 1.0f); }
void APIENTRY glRasterPos4iv(const GLint *v) { RP((float)v[0], (float)v[1], (float)v[2], (float)v[3]); }
void APIENTRY glRasterPos2s(GLshort x, GLshort y) { RP((float)x, (float)y, 0.0f, 1.0f); }
void APIENTRY glRasterPos3s(GLshort x, GLshort y, GLshort z) { RP((float)x, (float)y, (float)z, 1.0f); }
void APIENTRY glRasterPos4s(GLshort x, GLshort y, GLshort z, GLshort w) { RP((float)x, (float)y, (float)z, (float)w); }
void APIENTRY glRasterPos2sv(const GLshort *v) { RP((float)v[0], (float)v[1], 0.0f, 1.0f); }
void APIENTRY glRasterPos3sv(const GLshort *v) { RP((float)v[0], (float)v[1], (float)v[2], 1.0f); }
void APIENTRY glRasterPos4sv(const GLshort *v) { RP((float)v[0], (float)v[1], (float)v[2], (float)v[3]); }

/* ---- glBitmap ---- */
static void i_mapa(const u32 *d)
{
	CTX();
	int w = (int)d[0], h = (int)d[1], x, y;
	float xo = hgl_uf(d[2]), yo = hgl_uf(d[3]), xm = hgl_uf(d[4]), ym = hgl_uf(d[5]);
	const u8 *b = (const u8 *)(d + 6);
	int bx, by, r, g, bl, a;
	u32 z;
	if (!C->cur.rvalida) return;
	bx = hgl_suelo(C->cur.rpos[0] - xo);
	by = hgl_suelo(C->cur.rpos[1] - yo);
	r = hgl_red(C->cur.rcol[0] * 255); g = hgl_red(C->cur.rcol[1] * 255); bl = hgl_red(C->cur.rcol[2] * 255); a = hgl_red(C->cur.rcol[3] * 255);
	z = (u32)hgl_red(C->cur.rpos[2] * 65535.0f);
	for (y = 0; y < h; y++)
		for (x = 0; x < w; x++)
			if (b[y * ((w + 7) >> 3) + (x >> 3)] & (0x80 >> (x & 7)))
				hgl_fragmento(C, bx + x, C->al - 1 - (by + y), z, r, g, bl, a);
	C->cur.rpos[0] += xm;
	C->cur.rpos[1] += ym;
}
void APIENTRY glBitmap(GLsizei w, GLsizei h, GLfloat xo, GLfloat yo, GLfloat xm, GLfloat ym, const GLubyte *bm)
{
	CTX();
	int fb = (w + 7) >> 3, y, x, paso, fila;
	u32 *d;
	u8 *o;
	if (w < 0 || h < 0) { hgl_error(C, GL_INVALID_VALUE); return; }
	d = hgl_pedir((u32)(fb * h + 3) / 4 * 4 + 28);
	if (!d) { hgl_error(C, GL_OUT_OF_MEMORY); return; }
	d[0] = w; d[1] = h; d[2] = F(xo); d[3] = F(yo); d[4] = F(xm); d[5] = F(ym);
	o = (u8 *)(d + 6);
	/* con el almacén de desempaquetar (alineación y filas) */
	fila = C->des.fila > 0 ? C->des.fila : w;
	paso = (fila + 7) >> 3;
	paso = (paso + C->des.alin - 1) / C->des.alin * C->des.alin;
	if (bm)
		for (y = 0; y < h; y++)
			for (x = 0; x < w; x++) {
				int bit = x + C->des.salta_p, v;
				const u8 *q = bm + (y + C->des.salta_f) * paso + (bit >> 3);
				v = C->des.lsb ? (*q >> (bit & 7)) & 1 : (*q >> (7 - (bit & 7))) & 1;
				if (v) o[y * fb + (x >> 3)] |= (u8)(0x80 >> (x & 7));
			}
	if (C->lista_n) {
		hgl_graba_v(C, (void *)i_mapa, d, (u32)(fb * h + 3) / 4 + 6);
		if (C->lista_modo == GL_COMPILE) { hgl_soltar(d); return; }
	}
	i_mapa(d);
	hgl_soltar(d);
}

/* ---- glDrawPixels ---- */
static void dibuja_rgba(Ctx *C, int w, int h, const u8 *rgba)
{
	int x, y, i, j;
	float zx = C->pix.zoom_x, zy = C->pix.zoom_y, bx, by;
	u32 z;
	if (!C->cur.rvalida) return;
	bx = C->cur.rpos[0]; by = C->cur.rpos[1];
	z = (u32)hgl_red(C->cur.rpos[2] * 65535.0f);
	for (y = 0; y < h; y++) {
		int ya = hgl_red(by + y * zy), yb = hgl_red(by + (y + 1) * zy);
		if (ya > yb) { int t = ya; ya = yb; yb = t; }
		for (x = 0; x < w; x++) {
			const u8 *p = rgba + (y * w + x) * 4;
			int xa = hgl_red(bx + x * zx), xb = hgl_red(bx + (x + 1) * zx), r, g, b, a;
			if (xa > xb) { int t = xa; xa = xb; xb = t; }
			r = hgl_red(p[0] * C->pix.escala[0] + C->pix.sesgo[0] * 255);
			g = hgl_red(p[1] * C->pix.escala[1] + C->pix.sesgo[1] * 255);
			b = hgl_red(p[2] * C->pix.escala[2] + C->pix.sesgo[2] * 255);
			a = hgl_red(p[3] * C->pix.escala[3] + C->pix.sesgo[3] * 255);
			for (j = ya; j < yb; j++)
				for (i = xa; i < xb; i++) hgl_fragmento(C, i, C->al - 1 - j, z, r, g, b, a);
		}
	}
}
static void i_dibuja_pix(const u32 *d)
{
	CTX();
	dibuja_rgba(C, (int)d[0], (int)d[1], (const u8 *)(d + 2));
}
void APIENTRY glDrawPixels(GLsizei w, GLsizei h, GLenum f, GLenum t, const GLvoid *p)
{
	CTX();
	u32 *d;
	if (w < 0 || h < 0) { hgl_error(C, GL_INVALID_VALUE); return; }
	if (f == GL_DEPTH_COMPONENT || f == GL_STENCIL_INDEX) return;    /* (no se hace) */
	d = hgl_pedir((u32)w * h * 4 + 8);
	if (!d) { hgl_error(C, GL_OUT_OF_MEMORY); return; }
	d[0] = w; d[1] = h;
	hgl_lee_pix(C, &C->des, w, h, f, t, p, (u8 *)(d + 2));
	if (C->lista_n) {
		hgl_graba_v(C, (void *)i_dibuja_pix, d, (u32)w * h + 2);
		if (C->lista_modo == GL_COMPILE) { hgl_soltar(d); return; }
	}
	dibuja_rgba(C, w, h, (const u8 *)(d + 2));
	hgl_soltar(d);
}

/* ---- glReadPixels: de abajo arriba, como dice OpenGL ---- */
void APIENTRY glReadPixels(GLint x, GLint y, GLsizei w, GLsizei h, GLenum f, GLenum tipo, GLvoid *pix)
{
	CTX();
	int i, j, nc, paso, fila, tb;
	u8 *b = pix;
	if (w < 0 || h < 0) { hgl_error(C, GL_INVALID_VALUE); return; }
	switch (f) {
	case GL_RGB: case GL_BGR_EXT: nc = 3; break;
	case GL_RGBA: case GL_BGRA_EXT: nc = 4; break;
	case GL_LUMINANCE_ALPHA: nc = 2; break;
	case GL_RED: case GL_GREEN: case GL_BLUE: case GL_ALPHA: case GL_LUMINANCE: case GL_DEPTH_COMPONENT: case GL_STENCIL_INDEX: nc = 1; break;
	default: hgl_error(C, GL_INVALID_ENUM); return;
	}
	tb = tipo == GL_UNSIGNED_BYTE || tipo == GL_BYTE ? 1 : tipo == GL_UNSIGNED_SHORT || tipo == GL_SHORT ? 2 : 4;
	fila = C->emp.fila > 0 ? C->emp.fila : w;
	paso = fila * nc * tb;
	if (tb < C->emp.alin) paso = (paso + C->emp.alin - 1) / C->emp.alin * C->emp.alin;
	b += C->emp.salta_f * paso + C->emp.salta_p * nc * tb;
	for (j = 0; j < h; j++) {
		int yy = C->al - 1 - (y + j);
		for (i = 0; i < w; i++) {
			int xx = x + i, k;
			float v[4] = { 0, 0, 0, 1 }, o[4];
			if (xx >= 0 && yy >= 0 && xx < C->an && yy < C->al && C->color) {
				if (f == GL_DEPTH_COMPONENT) v[0] = C->prof[yy * C->an + xx] / 65535.0f;
				else {
					u16 p = C->color[yy * C->an + xx];
					int r = p >> 11, g = (p >> 5) & 63, bb = p & 31;
					v[0] = ((r << 3) | (r >> 2)) / 255.0f; v[1] = ((g << 2) | (g >> 4)) / 255.0f; v[2] = ((bb << 3) | (bb >> 2)) / 255.0f;
				}
			}
			switch (f) {
			case GL_RGB: o[0] = v[0]; o[1] = v[1]; o[2] = v[2]; break;
			case GL_BGR_EXT: o[0] = v[2]; o[1] = v[1]; o[2] = v[0]; break;
			case GL_RGBA: o[0] = v[0]; o[1] = v[1]; o[2] = v[2]; o[3] = v[3]; break;
			case GL_BGRA_EXT: o[0] = v[2]; o[1] = v[1]; o[2] = v[0]; o[3] = v[3]; break;
			case GL_LUMINANCE_ALPHA: o[0] = hgl_lim01(v[0] + v[1] + v[2]); o[1] = v[3]; break;
			case GL_LUMINANCE: o[0] = hgl_lim01(v[0] + v[1] + v[2]); break;
			case GL_GREEN: o[0] = v[1]; break;
			case GL_BLUE: o[0] = v[2]; break;
			case GL_ALPHA: o[0] = v[3]; break;
			case GL_STENCIL_INDEX: o[0] = 0; break;
			default: o[0] = v[0];
			}
			for (k = 0; k < nc; k++) {
				u8 *q = b + j * paso + (i * nc + k) * tb;
				switch (tipo) {
				case GL_UNSIGNED_BYTE: *q = (u8)hgl_red(o[k] * 255); break;
				case GL_BYTE: *(i8 *)q = (i8)hgl_red(o[k] * 127); break;
				case GL_UNSIGNED_SHORT: *(u16 *)q = (u16)hgl_red(o[k] * 65535); break;
				case GL_SHORT: *(i16 *)q = (i16)hgl_red(o[k] * 32767); break;
				case GL_FLOAT: *(float *)q = o[k]; break;
				case GL_UNSIGNED_INT: *(u32 *)q = (u32)((double)o[k] * 4294967295.0); break;
				default: *(i32 *)q = (i32)((double)o[k] * 2147483647.0);
				}
			}
		}
	}
}

/* ---- glCopyPixels (solo el color) ---- */
static void i_copia_pix(int x, int y, int w, int h, u32 tipo)
{
	CTX();
	u8 *rgba;
	int i, j;
	if (tipo != GL_COLOR || w <= 0 || h <= 0 || !C->color) return;
	rgba = hgl_pedir((u32)w * h * 4);
	if (!rgba) { hgl_error(C, GL_OUT_OF_MEMORY); return; }
	for (j = 0; j < h; j++)
		for (i = 0; i < w; i++) {
			int xx = x + i, yy = C->al - 1 - (y + j);
			u8 *o = rgba + (j * w + i) * 4;
			o[3] = 255;
			if (xx >= 0 && yy >= 0 && xx < C->an && yy < C->al) {
				u16 p = C->color[yy * C->an + xx];
				int r = p >> 11, g = (p >> 5) & 63, b = p & 31;
				o[0] = (u8)((r << 3) | (r >> 2)); o[1] = (u8)((g << 2) | (g >> 4)); o[2] = (u8)((b << 3) | (b >> 2));
			}
		}
	dibuja_rgba(C, w, h, rgba);
	hgl_soltar(rgba);
}
void APIENTRY glCopyPixels(GLint x, GLint y, GLsizei w, GLsizei h, GLenum t) { CTX(); LISTA(i_copia_pix, 5, x, y, w, h, t); i_copia_pix(x, y, w, h, t); }
