/* glu.c - GLU32.DLL de HispaGL: lo de la biblioteca de utilidades de OpenGL
 * (GLU 1.2) que usan los programas de la época: las cámaras (gluPerspective,
 * gluLookAt, gluOrtho2D, gluPickMatrix), proyectar (gluProject,
 * gluUnProject), las imágenes (gluScaleImage, gluBuild1DMipmaps,
 * gluBuild2DMipmaps), las cuádricas (esfera, cilindro, disco), y los textos
 * (gluErrorString, gluGetString). El teselador y las NURBS: que existan (no
 * dibujan). Hecho a partir de la especificación pública de GLU 1.2.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "GL/gl.h"

#define WINAPI __attribute__((stdcall))
typedef unsigned int u32;
typedef unsigned char u8;
#define NULL ((void *)0)
void *WINAPI GetProcessHeap(void);
void *WINAPI HeapAlloc(void *, u32, u32);
int WINAPI HeapFree(void *, u32, void *);
static void *pedir(u32 n) { return HeapAlloc(GetProcessHeap(), 8, n ? n : 1); }
static void soltar(void *p) { if (p) HeapFree(GetProcessHeap(), 0, p); }
void *memset(void *d, int v, u32 n);
void *memcpy(void *d, const void *s, u32 n);
void *memset(void *d, int v, u32 n) { u8 *p = d; while (n--) *p++ = (u8)v; return d; }
void *memcpy(void *d, const void *s, u32 n) { u8 *a = d; const u8 *b = s; while (n--) *a++ = *b++; return d; }

static double raiz(double x) { double r; if (x <= 0) return 0; __asm__("fsqrt" : "=t"(r) : "0"(x)); return r; }
static double seno(double x) { double r; __asm__("fsin" : "=t"(r) : "0"(x)); return r; }
static double coseno(double x) { double r; __asm__("fcos" : "=t"(r) : "0"(x)); return r; }
#define PI 3.14159265358979323846

#define GLU_INVALID_ENUM 100900
#define GLU_INVALID_VALUE 100901
#define GLU_OUT_OF_MEMORY 100902
#define GLU_VERSION 100800
#define GLU_EXTENSIONS 100801
#define GLU_SMOOTH 100000
#define GLU_FLAT 100001
#define GLU_NONE 100002
#define GLU_POINT 100010
#define GLU_LINE 100011
#define GLU_FILL 100012
#define GLU_SILHOUETTE 100013
#define GLU_OUTSIDE 100020
#define GLU_INSIDE 100021

/* ---- las cámaras ---- */
void WINAPI gluPerspective(GLdouble fovy, GLdouble asp, GLdouble n, GLdouble f)
{
	double t = n * seno(fovy * PI / 360) / coseno(fovy * PI / 360);
	glFrustum(-t * asp, t * asp, -t, t, n, f);
}
void WINAPI gluOrtho2D(GLdouble l, GLdouble r, GLdouble b, GLdouble t) { glOrtho(l, r, b, t, -1, 1); }
static void normaliza(double *v) { double l = raiz(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); if (l > 0) { v[0] /= l; v[1] /= l; v[2] /= l; } }
static void cruz(const double *a, const double *b, double *r) { r[0] = a[1] * b[2] - a[2] * b[1]; r[1] = a[2] * b[0] - a[0] * b[2]; r[2] = a[0] * b[1] - a[1] * b[0]; }
void WINAPI gluLookAt(GLdouble ex, GLdouble ey, GLdouble ez, GLdouble cx, GLdouble cy, GLdouble cz, GLdouble ux, GLdouble uy, GLdouble uz)
{
	double f[3] = { cx - ex, cy - ey, cz - ez }, u[3] = { ux, uy, uz }, s[3], v[3];
	GLdouble m[16];
	normaliza(f);
	cruz(f, u, s);
	normaliza(s);
	cruz(s, f, v);
	m[0] = s[0]; m[4] = s[1]; m[8] = s[2]; m[12] = 0;
	m[1] = v[0]; m[5] = v[1]; m[9] = v[2]; m[13] = 0;
	m[2] = -f[0]; m[6] = -f[1]; m[10] = -f[2]; m[14] = 0;
	m[3] = 0; m[7] = 0; m[11] = 0; m[15] = 1;
	glMultMatrixd(m);
	glTranslated(-ex, -ey, -ez);
}
void WINAPI gluPickMatrix(GLdouble x, GLdouble y, GLdouble w, GLdouble h, GLint *vista)
{
	if (w <= 0 || h <= 0) return;
	glTranslated((vista[2] - 2 * (x - vista[0])) / w, (vista[3] - 2 * (y - vista[1])) / h, 0);
	glScaled(vista[2] / w, vista[3] / h, 1);
}
static void xf(const GLdouble *m, const double *i, double *o)
{
	int k;
	for (k = 0; k < 4; k++) o[k] = m[k] * i[0] + m[4 + k] * i[1] + m[8 + k] * i[2] + m[12 + k] * i[3];
}
int WINAPI gluProject(GLdouble ox, GLdouble oy, GLdouble oz, const GLdouble *mv, const GLdouble *pr, const GLint *vista, GLdouble *wx, GLdouble *wy, GLdouble *wz)
{
	double a[4] = { ox, oy, oz, 1 }, b[4], c[4];
	xf(mv, a, b);
	xf(pr, b, c);
	if (c[3] == 0) return 0;
	c[0] /= c[3]; c[1] /= c[3]; c[2] /= c[3];
	*wx = vista[0] + (1 + c[0]) * vista[2] / 2;
	*wy = vista[1] + (1 + c[1]) * vista[3] / 2;
	*wz = (1 + c[2]) / 2;
	return 1;
}
static int invierte(const double *a, double *r)
{
	double m[4][8];
	int i, j, k;
	for (i = 0; i < 4; i++)
		for (j = 0; j < 4; j++) { m[i][j] = a[j * 4 + i]; m[i][j + 4] = i == j; }
	for (k = 0; k < 4; k++) {
		int p = k;
		double mx = m[k][k] < 0 ? -m[k][k] : m[k][k], d;
		for (i = k + 1; i < 4; i++) { double v = m[i][k] < 0 ? -m[i][k] : m[i][k]; if (v > mx) { mx = v; p = i; } }
		if (mx < 1e-30) return 0;
		for (j = 0; j < 8; j++) { double t = m[k][j]; m[k][j] = m[p][j]; m[p][j] = t; }
		d = 1 / m[k][k];
		for (j = 0; j < 8; j++) m[k][j] *= d;
		for (i = 0; i < 4; i++)
			if (i != k) { double f = m[i][k]; for (j = 0; j < 8; j++) m[i][j] -= f * m[k][j]; }
	}
	for (i = 0; i < 4; i++) for (j = 0; j < 4; j++) r[j * 4 + i] = m[i][j + 4];
	return 1;
}
int WINAPI gluUnProject(GLdouble wx, GLdouble wy, GLdouble wz, const GLdouble *mv, const GLdouble *pr, const GLint *vista, GLdouble *ox, GLdouble *oy, GLdouble *oz)
{
	double m[16], inv[16], a[4], b[4];
	int i, j, k;
	for (i = 0; i < 4; i++)
		for (j = 0; j < 4; j++) {
			double s = 0;
			for (k = 0; k < 4; k++) s += pr[k * 4 + i] * mv[j * 4 + k];
			m[j * 4 + i] = s;
		}
	if (!invierte(m, inv)) return 0;
	a[0] = (wx - vista[0]) * 2 / vista[2] - 1;
	a[1] = (wy - vista[1]) * 2 / vista[3] - 1;
	a[2] = wz * 2 - 1;
	a[3] = 1;
	xf(inv, a, b);
	if (b[3] == 0) return 0;
	*ox = b[0] / b[3]; *oy = b[1] / b[3]; *oz = b[2] / b[3];
	return 1;
}

/* ---- las imágenes ---- */
static int comps(GLenum f)
{
	switch (f) {
	case GL_RGB: case GL_BGR_EXT: return 3;
	case GL_RGBA: case GL_BGRA_EXT: return 4;
	case GL_LUMINANCE_ALPHA: return 2;
	case GL_LUMINANCE: case GL_ALPHA: case GL_RED: case GL_GREEN: case GL_BLUE: case GL_COLOR_INDEX: case GL_INTENSITY: return 1;
	}
	return 0;
}
/* la media de lo que cae en cada punto (o el más cercano al agrandar) */
static void escala(int nc, int wi, int hi, const u8 *a, int wo, int ho, u8 *b)
{
	int x, y, k;
	for (y = 0; y < ho; y++)
		for (x = 0; x < wo; x++) {
			int x0 = x * wi / wo, x1 = (x + 1) * wi / wo, y0 = y * hi / ho, y1 = (y + 1) * hi / ho, i, j;
			if (x1 <= x0) x1 = x0 + 1;
			if (y1 <= y0) y1 = y0 + 1;
			for (k = 0; k < nc; k++) {
				u32 s = 0, n = 0;
				for (j = y0; j < y1; j++) for (i = x0; i < x1; i++) { s += a[(j * wi + i) * nc + k]; n++; }
				b[(y * wo + x) * nc + k] = (u8)((s + n / 2) / n);
			}
		}
}
int WINAPI gluScaleImage(GLenum f, GLint wi, GLint hi, GLenum ti, const void *a, GLint wo, GLint ho, GLenum to, void *b)
{
	int nc = comps(f);
	if (!nc) return GLU_INVALID_ENUM;
	if (ti != GL_UNSIGNED_BYTE || to != GL_UNSIGNED_BYTE) return GLU_INVALID_ENUM;
	if (wi <= 0 || hi <= 0 || wo <= 0 || ho <= 0) return GLU_INVALID_VALUE;
	escala(nc, wi, hi, a, wo, ho, b);
	return 0;
}
static int pot2(int v) { int p = 1; while (p * 2 <= v) p *= 2; if (p < v && v - p > p * 2 - v) p *= 2; return p; }
int WINAPI gluBuild2DMipmaps(GLenum dest, GLint fi, GLint w, GLint h, GLenum f, GLenum t, const void *datos)
{
	int nc = comps(f), nw, nh, nivel = 0, maxt = 256;
	u8 *a, *b;
	if (!nc || t != GL_UNSIGNED_BYTE) return GLU_INVALID_ENUM;
	if (w <= 0 || h <= 0) return GLU_INVALID_VALUE;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxt);
	nw = pot2(w); nh = pot2(h);
	if (nw > maxt) nw = maxt;
	if (nh > maxt) nh = maxt;
	a = pedir((u32)nw * nh * nc);
	b = pedir((u32)nw * nh * nc);
	if (!a || !b) { soltar(a); soltar(b); return GLU_OUT_OF_MEMORY; }
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	if (nw == w && nh == h) memcpy(a, datos, (u32)w * h * nc);
	else escala(nc, w, h, datos, nw, nh, a);
	for (;;) {
		glTexImage2D(dest, nivel, fi, nw, nh, 0, f, GL_UNSIGNED_BYTE, a);
		if (nw == 1 && nh == 1) break;
		{
			int mw = nw > 1 ? nw / 2 : 1, mh = nh > 1 ? nh / 2 : 1;
			u8 *t2;
			escala(nc, nw, nh, a, mw, mh, b);
			t2 = a; a = b; b = t2;
			nw = mw; nh = mh;
		}
		nivel++;
	}
	soltar(a);
	soltar(b);
	return 0;
}
int WINAPI gluBuild1DMipmaps(GLenum dest, GLint fi, GLint w, GLenum f, GLenum t, const void *datos)
{
	(void)dest;
	return gluBuild2DMipmaps(GL_TEXTURE_2D, fi, w, 1, f, t, datos);
}

/* ---- los textos ---- */
const GLubyte *WINAPI gluErrorString(GLenum e)
{
	switch (e) {
	case GL_NO_ERROR: return (const GLubyte *)"no error";
	case GL_INVALID_ENUM: return (const GLubyte *)"invalid enumerant";
	case GL_INVALID_VALUE: return (const GLubyte *)"invalid value";
	case GL_INVALID_OPERATION: return (const GLubyte *)"invalid operation";
	case GL_STACK_OVERFLOW: return (const GLubyte *)"stack overflow";
	case GL_STACK_UNDERFLOW: return (const GLubyte *)"stack underflow";
	case GL_OUT_OF_MEMORY: case GLU_OUT_OF_MEMORY: return (const GLubyte *)"out of memory";
	case GLU_INVALID_ENUM: return (const GLubyte *)"invalid enumerant";
	case GLU_INVALID_VALUE: return (const GLubyte *)"invalid value";
	}
	return (const GLubyte *)"";
}
const GLubyte *WINAPI gluGetString(GLenum n)
{
	if (n == GLU_VERSION) return (const GLubyte *)"1.2 HispaGL";
	if (n == GLU_EXTENSIONS) return (const GLubyte *)"";
	return NULL;
}

/* ---- las cuádricas ---- */
typedef struct { int normales, textura, estilo, dentro; } Cuadrica;
void *WINAPI gluNewQuadric(void)
{
	Cuadrica *q = pedir(sizeof(Cuadrica));
	if (q) { q->normales = GLU_SMOOTH; q->estilo = GLU_FILL; q->dentro = 0; }
	return q;
}
void WINAPI gluDeleteQuadric(void *q) { soltar(q); }
void WINAPI gluQuadricNormals(void *q, GLenum n) { if (q) ((Cuadrica *)q)->normales = (int)n; }
void WINAPI gluQuadricTexture(void *q, GLboolean t) { if (q) ((Cuadrica *)q)->textura = t; }
void WINAPI gluQuadricDrawStyle(void *q, GLenum e) { if (q) ((Cuadrica *)q)->estilo = (int)e; }
void WINAPI gluQuadricOrientation(void *q, GLenum o) { if (q) ((Cuadrica *)q)->dentro = o == GLU_INSIDE; }
void WINAPI gluQuadricCallback(void *q, GLenum w, void (*f)(void)) { (void)q; (void)w; (void)f; }
static GLenum prim(const Cuadrica *q, GLenum relleno) { return q->estilo == GLU_POINT ? GL_POINTS : q->estilo == GLU_LINE || q->estilo == GLU_SILHOUETTE ? GL_LINE_LOOP : relleno; }

void WINAPI gluSphere(void *qq, GLdouble r, GLint cortes, GLint pilas)
{
	Cuadrica *q = qq;
	int i, j;
	double s = q && q->dentro ? -1 : 1;
	if (!q || cortes < 3 || pilas < 2) return;
	for (j = 0; j < pilas; j++) {
		double a0 = PI * j / pilas, a1 = PI * (j + 1) / pilas;
		if (q->estilo == GLU_FILL) glBegin(GL_QUAD_STRIP);
		for (i = 0; i <= cortes; i++) {
			double b = 2 * PI * (i == cortes ? 0 : i) / cortes;
			double x0 = -seno(b) * seno(a0), y0 = coseno(b) * seno(a0), z0 = coseno(a0);
			double x1 = -seno(b) * seno(a1), y1 = coseno(b) * seno(a1), z1 = coseno(a1);
			if (q->estilo != GLU_FILL) glBegin(GL_LINES);
			if (q->normales != GLU_NONE) glNormal3d(x0 * s, y0 * s, z0 * s);
			if (q->textura) glTexCoord2d(1 - (double)i / cortes, 1 - (double)j / pilas);
			glVertex3d(x0 * r, y0 * r, z0 * r);
			if (q->normales != GLU_NONE) glNormal3d(x1 * s, y1 * s, z1 * s);
			if (q->textura) glTexCoord2d(1 - (double)i / cortes, 1 - (double)(j + 1) / pilas);
			glVertex3d(x1 * r, y1 * r, z1 * r);
			if (q->estilo != GLU_FILL) glEnd();
		}
		if (q->estilo == GLU_FILL) glEnd();
	}
}
void WINAPI gluCylinder(void *qq, GLdouble rb, GLdouble rt, GLdouble h, GLint cortes, GLint pilas)
{
	Cuadrica *q = qq;
	int i, j;
	double s = q && q->dentro ? -1 : 1, dz = rb - rt, nz = h != 0 ? dz / h : 0, ln;
	if (!q || cortes < 3 || pilas < 1) return;
	ln = raiz(1 + nz * nz);
	for (j = 0; j < pilas; j++) {
		double z0 = h * j / pilas, z1 = h * (j + 1) / pilas, r0 = rb - dz * j / pilas, r1 = rb - dz * (j + 1) / pilas;
		glBegin(prim(q, GL_QUAD_STRIP));
		for (i = 0; i <= cortes; i++) {
			double a = 2 * PI * (i == cortes ? 0 : i) / cortes, x = seno(a), y = coseno(a);
			if (q->normales != GLU_NONE) glNormal3d(x * s / ln, y * s / ln, nz * s / ln);
			if (q->textura) glTexCoord2d((double)i / cortes, (double)j / pilas);
			glVertex3d(x * r0, y * r0, z0);
			if (q->textura) glTexCoord2d((double)i / cortes, (double)(j + 1) / pilas);
			glVertex3d(x * r1, y * r1, z1);
		}
		glEnd();
	}
}
void WINAPI gluPartialDisk(void *qq, GLdouble ri, GLdouble ro, GLint cortes, GLint anillos, GLdouble ini, GLdouble barrido)
{
	Cuadrica *q = qq;
	int i, j;
	if (!q || cortes < 3 || anillos < 1) return;
	if (q->normales != GLU_NONE) glNormal3d(0, 0, q->dentro ? -1 : 1);
	for (j = 0; j < anillos; j++) {
		double r0 = ri + (ro - ri) * j / anillos, r1 = ri + (ro - ri) * (j + 1) / anillos;
		glBegin(prim(q, GL_QUAD_STRIP));
		for (i = 0; i <= cortes; i++) {
			double a = (ini + barrido * i / cortes) * PI / 180, x = seno(a), y = coseno(a);
			if (q->textura) glTexCoord2d(0.5 + x * r1 / ro / 2, 0.5 + y * r1 / ro / 2);
			glVertex3d(x * r1, y * r1, 0);
			if (q->textura) glTexCoord2d(0.5 + x * r0 / ro / 2, 0.5 + y * r0 / ro / 2);
			glVertex3d(x * r0, y * r0, 0);
		}
		glEnd();
	}
}
void WINAPI gluDisk(void *q, GLdouble ri, GLdouble ro, GLint cortes, GLint anillos) { gluPartialDisk(q, ri, ro, cortes, anillos, 0, 360); }

/* ---- el teselador y las NURBS: que estén ---- */
void *WINAPI gluNewTess(void) { return NULL; }
void WINAPI gluDeleteTess(void *t) { (void)t; }
void WINAPI gluTessCallback(void *t, GLenum w, void (*f)(void)) { (void)t; (void)w; (void)f; }
void WINAPI gluTessBeginPolygon(void *t, void *d) { (void)t; (void)d; }
void WINAPI gluTessEndPolygon(void *t) { (void)t; }
void WINAPI gluTessBeginContour(void *t) { (void)t; }
void WINAPI gluTessEndContour(void *t) { (void)t; }
void WINAPI gluTessVertex(void *t, GLdouble *c, void *d) { (void)t; (void)c; (void)d; }
void WINAPI gluTessProperty(void *t, GLenum w, GLdouble v) { (void)t; (void)w; (void)v; }
void WINAPI gluTessNormal(void *t, GLdouble x, GLdouble y, GLdouble z) { (void)t; (void)x; (void)y; (void)z; }
void WINAPI gluBeginPolygon(void *t) { (void)t; }
void WINAPI gluEndPolygon(void *t) { (void)t; }
void WINAPI gluNextContour(void *t, GLenum w) { (void)t; (void)w; }
void *WINAPI gluNewNurbsRenderer(void) { return NULL; }
void WINAPI gluDeleteNurbsRenderer(void *n) { (void)n; }
void WINAPI gluBeginSurface(void *n) { (void)n; }
void WINAPI gluEndSurface(void *n) { (void)n; }
void WINAPI gluBeginCurve(void *n) { (void)n; }
void WINAPI gluEndCurve(void *n) { (void)n; }
void WINAPI gluNurbsProperty(void *n, GLenum p, GLfloat v) { (void)n; (void)p; (void)v; }
void WINAPI gluNurbsSurface(void *n, GLint a, GLfloat *b, GLint c, GLfloat *d, GLint e, GLint f, GLfloat *g, GLint h, GLint i, GLenum t) { (void)n; (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; (void)g; (void)h; (void)i; (void)t; }
void WINAPI gluNurbsCurve(void *n, GLint a, GLfloat *b, GLint c, GLfloat *d, GLint e, GLenum t) { (void)n; (void)a; (void)b; (void)c; (void)d; (void)e; (void)t; }
