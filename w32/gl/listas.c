/* listas.c - HispaGL: las listas de visualización (glNewList, glEndList,
 * glCallList, glCallLists, glGenLists, glDeleteLists, glIsList, glListBase).
 * Una lista es una tira de llamadas a las funciones de dentro: la función, el
 * número de argumentos (de 32 bits, tal cual: los float van con sus bits) y
 * los argumentos; o la función y un bloque (una matriz, una imagen), que se
 * le pasa por su dirección. Se compila lo que dice la especificación: lo que
 * cambia el estado del servidor; lo del cliente (glPixelStore, los punteros
 * de las matrices de vértices...) se hace en el momento.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "hgl.h"
#include <stdarg.h>

#define BLOQUE 0x80000000u

static int mete(Lista *l, u32 n)
{
	if (l->n + n > l->cap) {
		u32 nc = l->cap ? l->cap * 2 : 256, *d;
		while (nc < l->n + n) nc *= 2;
		d = hgl_pedir(nc * 4);
		if (!d) return 0;
		if (l->d) { hgl_copia(d, l->d, l->n * 4); hgl_soltar(l->d); }
		l->d = d;
		l->cap = nc;
	}
	return 1;
}

void hgl_graba(Ctx *C, void *fn, int n, ...)
{
	Lista *l = &C->lista_cur;
	va_list ap;
	int k;
	if (!mete(l, (u32)n + 2)) { hgl_error(C, GL_OUT_OF_MEMORY); return; }
	l->d[l->n++] = (u32)fn;
	l->d[l->n++] = (u32)n;
	va_start(ap, n);
	for (k = 0; k < n; k++) l->d[l->n++] = va_arg(ap, u32);
	va_end(ap);
}

void hgl_graba_v(Ctx *C, void *fn, const void *p, u32 palabras)
{
	Lista *l = &C->lista_cur;
	if (!mete(l, palabras + 2)) { hgl_error(C, GL_OUT_OF_MEMORY); return; }
	l->d[l->n++] = (u32)fn;
	l->d[l->n++] = BLOQUE | palabras;
	hgl_copia(l->d + l->n, p, palabras * 4);
	l->n += palabras;
}

typedef void (*F0)(void);
typedef void (*F1)(u32);
typedef void (*F2)(u32, u32);
typedef void (*F3)(u32, u32, u32);
typedef void (*F4)(u32, u32, u32, u32);
typedef void (*F5)(u32, u32, u32, u32, u32);
typedef void (*F6)(u32, u32, u32, u32, u32, u32);
typedef void (*F7)(u32, u32, u32, u32, u32, u32, u32);
typedef void (*F8)(u32, u32, u32, u32, u32, u32, u32, u32);
typedef void (*FP)(const void *);

static void ejecuta(Ctx *C, const Lista *l)
{
	const u32 *d = l->d, *f = l->d + l->n;
	while (d < f) {
		void *fn = (void *)d[0];
		u32 n = d[1];
		const u32 *a = d + 2;
		if (n & BLOQUE) {
			((FP)fn)(a);
			d = a + (n & ~BLOQUE);
			continue;
		}
		switch (n) {
		case 0: ((F0)fn)(); break;
		case 1: ((F1)fn)(a[0]); break;
		case 2: ((F2)fn)(a[0], a[1]); break;
		case 3: ((F3)fn)(a[0], a[1], a[2]); break;
		case 4: ((F4)fn)(a[0], a[1], a[2], a[3]); break;
		case 5: ((F5)fn)(a[0], a[1], a[2], a[3], a[4]); break;
		case 6: ((F6)fn)(a[0], a[1], a[2], a[3], a[4], a[5]); break;
		case 7: ((F7)fn)(a[0], a[1], a[2], a[3], a[4], a[5], a[6]); break;
		default: ((F8)fn)(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7]); break;
		}
		d = a + n;
		if (hgl_c != C) return;   /* (por si la lista cambia de contexto: no debería) */
	}
}

void hgl_ejecuta_lista(Ctx *C, u32 n)
{
	Comun *c = C->com;
	if (!n || n >= c->nlistas || !c->listas[n].d) return;
	if (C->lista_prof >= 64) return;
	C->lista_prof++;
	ejecuta(C, &c->listas[n]);
	C->lista_prof--;
}

void hgl_listas_suelta(Comun *c)
{
	u32 k;
	for (k = 0; k < c->nlistas; k++) hgl_soltar(c->listas[k].d);
	hgl_soltar(c->listas);
	c->listas = NULL;
	c->nlistas = 0;
}

static int sitio(Comun *c, u32 n)
{
	if (n < c->nlistas) return 1;
	{
		u32 nn = c->nlistas ? c->nlistas : 64;
		Lista *l;
		while (nn <= n) nn *= 2;
		l = hgl_pedir(nn * sizeof(Lista));
		if (!l) return 0;
		if (c->listas) { hgl_copia(l, c->listas, c->nlistas * sizeof(Lista)); hgl_soltar(c->listas); }
		c->listas = l;
		c->nlistas = nn;
	}
	return 1;
}

/* las listas «dadas» (glGenLists) sin nada dentro llevan d = NULL y cap = 1 */
GLuint APIENTRY glGenLists(GLsizei r)
{
	CTX(0);
	Comun *c = C->com;
	u32 ini = 1, k;
	if (r <= 0) { if (r < 0) hgl_error(C, GL_INVALID_VALUE); return 0; }
	for (;;) {
		for (k = 0; k < (u32)r; k++)
			if (ini + k < c->nlistas && (c->listas[ini + k].d || c->listas[ini + k].cap)) break;
		if (k == (u32)r) break;
		ini += k + 1;
	}
	if (!sitio(c, ini + r)) { hgl_error(C, GL_OUT_OF_MEMORY); return 0; }
	for (k = 0; k < (u32)r; k++) c->listas[ini + k].cap = 1;
	return ini;
}
GLboolean APIENTRY glIsList(GLuint n)
{
	CTX(0);
	return n && n < C->com->nlistas && (C->com->listas[n].d || C->com->listas[n].cap);
}
void APIENTRY glDeleteLists(GLuint n, GLsizei r)
{
	CTX();
	u32 k;
	if (r < 0) { hgl_error(C, GL_INVALID_VALUE); return; }
	for (k = n; k < n + (u32)r; k++)
		if (k && k < C->com->nlistas) {
			hgl_soltar(C->com->listas[k].d);
			C->com->listas[k].d = NULL;
			C->com->listas[k].n = C->com->listas[k].cap = 0;
		}
}
void APIENTRY glNewList(GLuint n, GLenum modo)
{
	CTX();
	if (!n) { hgl_error(C, GL_INVALID_VALUE); return; }
	if (modo != GL_COMPILE && modo != GL_COMPILE_AND_EXECUTE) { hgl_error(C, GL_INVALID_ENUM); return; }
	if (C->lista_n || C->prim >= 0) { hgl_error(C, GL_INVALID_OPERATION); return; }
	C->lista_n = n;
	C->lista_modo = modo;
	C->lista_cur.d = NULL;
	C->lista_cur.n = C->lista_cur.cap = 0;
}
void APIENTRY glEndList(void)
{
	CTX();
	Comun *c = C->com;
	u32 n = C->lista_n;
	if (!n) { hgl_error(C, GL_INVALID_OPERATION); return; }
	C->lista_n = 0;
	if (!sitio(c, n)) { hgl_soltar(C->lista_cur.d); hgl_error(C, GL_OUT_OF_MEMORY); return; }
	hgl_soltar(c->listas[n].d);
	c->listas[n] = C->lista_cur;
	if (!c->listas[n].cap) c->listas[n].cap = 1;
	C->lista_cur.d = NULL;
	C->lista_cur.n = C->lista_cur.cap = 0;
}
static void i_llama(u32 n) { CTX(); hgl_ejecuta_lista(C, n); }
void APIENTRY glCallList(GLuint n) { CTX(); LISTA(i_llama, 1, n); hgl_ejecuta_lista(C, n); }
static void i_base(u32 b) { CTX(); C->lista_base = b; }
void APIENTRY glListBase(GLuint b) { CTX(); LISTA(i_base, 1, b); i_base(b); }
void APIENTRY glCallLists(GLsizei n, GLenum tipo, const GLvoid *l)
{
	CTX();
	const u8 *p = l;
	int k;
	for (k = 0; k < n; k++) {
		u32 v;
		switch (tipo) {
		case GL_BYTE: v = (u32)(int)((const i8 *)p)[k]; break;
		case GL_UNSIGNED_BYTE: v = p[k]; break;
		case GL_SHORT: v = (u32)(int)((const i16 *)p)[k]; break;
		case GL_UNSIGNED_SHORT: v = ((const u16 *)p)[k]; break;
		case GL_INT: case GL_UNSIGNED_INT: v = ((const u32 *)p)[k]; break;
		case GL_FLOAT: v = (u32)(int)((const float *)p)[k]; break;
		case GL_2_BYTES: v = p[k * 2] * 256u + p[k * 2 + 1]; break;
		case GL_3_BYTES: v = (p[k * 3] * 256u + p[k * 3 + 1]) * 256u + p[k * 3 + 2]; break;
		case GL_4_BYTES: v = ((p[k * 4] * 256u + p[k * 4 + 1]) * 256u + p[k * 4 + 2]) * 256u + p[k * 4 + 3]; break;
		default: hgl_error(C, GL_INVALID_ENUM); return;
		}
		v += C->lista_base;
		glCallList(v);
	}
}
