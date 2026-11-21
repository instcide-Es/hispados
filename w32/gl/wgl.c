/* wgl.c - HispaGL: lo de Windows de OPENGL32.DLL. Las wgl* (contextos,
 * wglGetProcAddress, wglUseFontBitmapsA...), los formatos de punto, el
 * puente con GDI32 y USER32 (SwapBuffers, ChoosePixelFormat, SetPixelFormat,
 * DescribePixelFormat, GetPixelFormat y ChangeDisplaySettingsA son de GDI32 y
 * USER32 en Windows: en Hispa-486 las tiene Hispa Ventanas y en Hispa-Pentium
 * WIN32.BIN, y las dos le pasan la llamada a OPENGL32 por HispaGLPuente), y
 * la presentación: a la ventana con SetDIBitsToDevice (un DIB de 16 bits
 * 5-6-5 de arriba abajo) o, si el programa ha cambiado de modo
 * (ChangeDisplaySettingsA) y su ventana lo ocupa todo, a pantalla completa
 * con el DirectDraw de HW32 (DDRAW.DLL: el modo del juego, agrandado y en el
 * centro, como los juegos de DirectDraw).
 * Ajustes (variable HISPAGL, en el entorno): RAPIDO (todo al punto más
 * cercano: para los equipos viejos), EXACTO (el trilineal de verdad),
 * VENTANA (nunca a pantalla completa), TRAZA (lo que pasa, por la traza).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "hgl.h"

#define WINAPI __attribute__((stdcall))
typedef void *HANDLE;
typedef HANDLE HWND, HDC, HGLRC, HMODULE;
typedef struct { int left, top, right, bottom; } RECT;

HANDLE WINAPI GetProcessHeap(void);
void *WINAPI HeapAlloc(HANDLE, u32, u32);
int WINAPI HeapFree(HANDLE, u32, void *);
u32 WINAPI GetEnvironmentVariableA(const char *, char *, u32);
void WINAPI OutputDebugStringA(const char *);
HMODULE WINAPI LoadLibraryA(const char *);
void *WINAPI GetProcAddress(HMODULE, const char *);
HMODULE WINAPI GetModuleHandleA(const char *);
u32 WINAPI GetTickCount(void);
int WINAPI GetClientRect(HWND, RECT *);
HWND WINAPI WindowFromDC(HDC);
HDC WINAPI GetDC(HWND);
int WINAPI ReleaseDC(HWND, HDC);
int WINAPI GetSystemMetrics(int);
HWND WINAPI GetActiveWindow(void);
int WINAPI SetDIBitsToDevice(HDC, int, int, u32, u32, int, int, u32, u32, const void *, const void *, u32);
HDC WINAPI CreateCompatibleDC(HDC);
int WINAPI DeleteDC(HDC);
HANDLE WINAPI SelectObject(HDC, HANDLE);
int WINAPI DeleteObject(HANDLE);
HANDLE WINAPI CreateDIBSection(HDC, const void *, u32, void **, HANDLE, u32);
int WINAPI TextOutA(HDC, int, int, const char *, int);
int WINAPI SetTextColor(HDC, u32);
int WINAPI SetBkColor(HDC, u32);
int WINAPI SetBkMode(HDC, int);
int WINAPI GetTextExtentPoint32A(HDC, const char *, int, int *);
int WINAPI GetTextMetricsA(HDC, void *);
HANDLE WINAPI GetCurrentObject(HDC, u32);
int WINAPI HispaGLPuente(void *);

extern int hgl_calidad;

/* ---- lo de la plataforma ---- */
static int traza_si;
void *hgl_pedir(u32 n) { return HeapAlloc(GetProcessHeap(), 8 /* HEAP_ZERO_MEMORY */, n ? n : 1); }
void hgl_soltar(void *p) { if (p) HeapFree(GetProcessHeap(), 0, p); }
static int es_486(void) { u16 cs; __asm__("mov %%cs, %0" : "=r"(cs)); return cs != 0x33; }
static void serie(const char *s)
{
	OutputDebugStringA(s);
	if (es_486())        /* (en Hispa-486, como las DLL de DirectX: por COM1) */
		while (*s) { __asm__ volatile("outb %0, %1" :: "a"(*s), "Nd"((u16)0x3F8)); s++; }
}
void hgl_traza(const char *s, u32 v)
{
	char b[80];
	int k = 0, j;
	if (!traza_si) return;
	for (j = 0; "[gl] "[j]; j++) b[k++] = "[gl] "[j];
	while (*s && k < 60) b[k++] = *s++;
	b[k++] = ' ';
	for (j = 0; j < 8; j++) b[k++] = "0123456789ABCDEF"[(v >> (28 - j * 4)) & 15];
	b[k++] = '\r'; b[k++] = '\n'; b[k] = 0;
	serie(b);
}

/* ---- los formatos de punto ---- */
typedef struct {
	u16 tam, ver;
	u32 flags;
	u8 tipo, bits, rb, rs, gb, gs, bb, bs, ab, as, acb, acr, acg, acbl, aca, prof, plant, aux, capa, res;
	u32 mcapa, mvis, mdanio;
} PFD;
#define PFD_DOUBLEBUFFER 1
#define PFD_DRAW_TO_WINDOW 4
#define PFD_SUPPORT_GDI 0x10
#define PFD_SUPPORT_OPENGL 0x20
#define PFD_SWAP_COPY 0x400
#define FORMATOS 2
static void describe(int f, PFD *p)
{
	hgl_pon(p, 0, sizeof *p);
	p->tam = sizeof *p; p->ver = 1;
	p->flags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | (f == 1 ? PFD_DOUBLEBUFFER | PFD_SWAP_COPY : PFD_SUPPORT_GDI);
	p->tipo = 0;    /* PFD_TYPE_RGBA */
	p->bits = 16; p->rb = 5; p->rs = 11; p->gb = 6; p->gs = 5; p->bb = 5; p->bs = 0;
	p->prof = 16;
}

/* la ventana de cada DC con su formato (una tabla pequeña) */
#define VMAX 16
static struct { HWND v; int f; } fmts[VMAX];
static int formato_de(HWND v) { int k; for (k = 0; k < VMAX; k++) if (fmts[k].v == v) return fmts[k].f; return 0; }

int WINAPI wglChoosePixelFormat(HDC dc, const PFD *p)
{
	(void)dc;
	if (!p) return 0;
	/* el de dos búferes, salvo que se pida uno solo (o «da igual»: el de dos) */
	return (p->flags & PFD_DOUBLEBUFFER) || (p->flags & 0x40000000 /* PFD_DOUBLEBUFFER_DONTCARE */) ? 1 : 2;
}
int WINAPI wglDescribePixelFormat(HDC dc, int f, u32 n, PFD *p)
{
	(void)dc;
	if (f < 1 || f > FORMATOS) return 0;
	if (p && n >= sizeof(PFD)) describe(f, p);
	return FORMATOS;
}
int WINAPI wglSetPixelFormat(HDC dc, int f, const PFD *p)
{
	HWND v = WindowFromDC(dc);
	int k, libre = -1;
	(void)p;
	if (f < 1 || f > FORMATOS) return 0;
	for (k = 0; k < VMAX; k++) {
		if (fmts[k].v == v) { fmts[k].f = f; return 1; }
		if (!fmts[k].v && libre < 0) libre = k;
	}
	if (libre < 0) libre = 0;
	fmts[libre].v = v; fmts[libre].f = f;
	hgl_traza("SetPixelFormat", (u32)f);
	return 1;
}
int WINAPI wglGetPixelFormat(HDC dc) { return formato_de(WindowFromDC(dc)); }

/* ---- los contextos ---- */
typedef struct {
	HDC dc;
	HWND v;
} Plat;
static Ctx *actual_ctx;
static HDC actual_dc;

static void al_tamano(Ctx *C)
{
	Plat *p = C->plat;
	RECT r;
	if (!p || !p->v) return;
	if (GetClientRect(p->v, &r)) hgl_tamano(C, r.right - r.left, r.bottom - r.top);
}

HGLRC WINAPI wglCreateContext(HDC dc)
{
	Ctx *C = hgl_nuevo(NULL);
	Plat *p;
	if (!C) return NULL;
	p = hgl_pedir(sizeof(Plat));
	if (!p) { hgl_borra(C); return NULL; }
	p->dc = dc;
	p->v = WindowFromDC(dc);
	C->plat = p;
	C->dobles = formato_de(p->v) != 2;
	hgl_traza("wglCreateContext", (u32)C);
	return (HGLRC)C;
}
HGLRC WINAPI wglCreateLayerContext(HDC dc, int capa) { return capa ? NULL : wglCreateContext(dc); }
int WINAPI wglDeleteContext(HGLRC h)
{
	Ctx *C = h;
	if (!C) return 0;
	if (C == actual_ctx) { actual_ctx = NULL; actual_dc = NULL; hgl_c = NULL; }
	hgl_soltar(C->plat);
	hgl_borra(C);
	return 1;
}
int WINAPI wglMakeCurrent(HDC dc, HGLRC h)
{
	Ctx *C = h;
	if (!C) { hgl_c = actual_ctx = NULL; actual_dc = NULL; return 1; }
	if (dc) {
		Plat *p = C->plat;
		p->dc = dc;
		p->v = WindowFromDC(dc);
	}
	hgl_c = actual_ctx = C;
	actual_dc = dc;
	al_tamano(C);
	hgl_traza("wglMakeCurrent", (u32)C->an << 16 | (u32)C->al);
	return 1;
}
HGLRC WINAPI wglGetCurrentContext(void) { return actual_ctx; }
HDC WINAPI wglGetCurrentDC(void) { return actual_dc; }
int WINAPI wglShareLists(HGLRC a, HGLRC b)
{
	Ctx *A = a, *B = b;
	if (!A || !B) return 0;
	if (A->com == B->com) return 1;
	if (B->com->usos > 1) return 0;
	hgl_tex_suelta_todo(B->com);
	hgl_listas_suelta(B->com);
	hgl_soltar(B->com);
	B->com = A->com;
	A->com->usos++;
	return 1;
}
int WINAPI wglCopyContext(HGLRC a, HGLRC b, u32 m) { (void)a; (void)b; (void)m; return 0; }

/* ---- la presentación ---- */
static int modo_w, modo_h;           /* el que ha pedido el programa (ChangeDisplaySettings) */
static int solo_ventana;
/* DirectDraw (de HW32), cargado cuando hace falta */
typedef i32 (WINAPI *FDDC)(void *, void **, void *);
typedef i32 (WINAPI *M0)(void *);
typedef i32 (WINAPI *M1)(void *, u32);
typedef i32 (WINAPI *M2)(void *, u32, u32);
typedef i32 (WINAPI *M3)(void *, u32, u32, u32);
typedef i32 (WINAPI *M4)(void *, u32, u32, u32, u32);
#define COM(o, n, T) ((T)((*(void ***)(o))[n]))
typedef struct {
	u32 tam, flags, alto, ancho; i32 paso; u32 traseros, refresco, alfa, res; void *mem;
	u32 ck[8];
	u32 pf_tam, pf_flags, pf_cc, pf_bits, pf_r, pf_g, pf_b, pf_a;
	u32 caps;
} DDSD;
static void *dd, *prim;
static HWND dd_v;
static int dd_w, dd_h, dd_falla;
static void suelta_dd(void)
{
	if (prim) { COM(prim, 2, M0)(prim); prim = NULL; }
	if (dd) {
		COM(dd, 19, M0)(dd);                 /* RestoreDisplayMode */
		COM(dd, 20, M2)(dd, (u32)dd_v, 8);   /* SetCooperativeLevel(DDSCL_NORMAL) */
		COM(dd, 2, M0)(dd);
		dd = NULL;
	}
}
static int toma_dd(HWND v, int w, int h)
{
	HMODULE m;
	FDDC crea;
	DDSD d;
	if (dd && dd_v == v && dd_w == w && dd_h == h) return 1;
	suelta_dd();
	if (dd_falla) return 0;
	m = LoadLibraryA("DDRAW.DLL");
	crea = m ? (FDDC)GetProcAddress(m, "DirectDrawCreate") : NULL;
	if (!crea || crea(NULL, &dd, NULL)) { dd = NULL; dd_falla = 1; return 0; }
	if (COM(dd, 20, M2)(dd, (u32)v, 0x11 /* DDSCL_FULLSCREEN | DDSCL_EXCLUSIVE */) ||
	    COM(dd, 21, M3)(dd, (u32)w, (u32)h, 16)) { suelta_dd(); dd_falla = 1; return 0; }
	hgl_pon(&d, 0, sizeof d);
	d.tam = 108; d.flags = 1;             /* DDSD_CAPS */
	d.caps = 0x200;                       /* DDSCAPS_PRIMARYSURFACE */
	if (COM(dd, 6, M3)(dd, (u32)&d, (u32)&prim, 0)) { prim = NULL; suelta_dd(); dd_falla = 1; return 0; }
	dd_v = v; dd_w = w; dd_h = h;
	hgl_traza("pantalla completa (DirectDraw)", (u32)w << 16 | (u32)h);
	return 1;
}
static int presenta_dd(Ctx *C)
{
	DDSD d;
	int y, x;
	i32 r;
	hgl_pon(&d, 0, sizeof d);
	d.tam = 108;
	r = COM(prim, 25, M4)(prim, 0, (u32)&d, 1 /* DDLOCK_WAIT */, 0);
	if (r == (i32)0x887601C2) {           /* DDERR_SURFACELOST: otra ventana tiene la pantalla */
		COM(prim, 27, M0)(prim);
		return 1;
	}
	if (r) return 0;
	for (y = 0; y < C->al && y < (int)d.alto; y++) {
		const u16 *o = C->color + y * C->an;
		u8 *q = (u8 *)d.mem + y * d.paso;
		int n = C->an < (int)d.ancho ? C->an : (int)d.ancho;
		if (d.pf_bits == 16 && d.pf_g == 0x7E0) hgl_copia(q, o, (u32)n * 2);
		else if (d.pf_bits == 16)
			for (x = 0; x < n; x++) ((u16 *)q)[x] = (u16)(((o[x] >> 1) & 0x7FE0) | (o[x] & 0x1F));
		else if (d.pf_bits == 32)
			for (x = 0; x < n; x++) {
				u16 p = o[x];
				((u32 *)q)[x] = ((u32)((p >> 8) & 0xF8) << 16) | ((u32)((p >> 3) & 0xFC) << 8) | ((p << 3) & 0xF8);
			}
	}
	COM(prim, 32, M1)(prim, 0);           /* Unlock: se ve */
	return 1;
}
static void presenta(Ctx *C, HDC dc)
{
	Plat *p = C->plat;
	HWND v = p ? p->v : NULL;
	struct { u32 tam; i32 w, h; u16 planos, bits; u32 comp, tamimg; i32 xm, ym; u32 usados, imp; u32 masc[3]; } bmi;
	if (!C->color) return;
	if (modo_w && !solo_ventana && v && C->an >= modo_w && C->al >= modo_h && toma_dd(v, modo_w, modo_h)) {
		if (presenta_dd(C)) return;
	}
	hgl_pon(&bmi, 0, sizeof bmi);
	bmi.tam = 40; bmi.w = C->an; bmi.h = -C->al; bmi.planos = 1; bmi.bits = 16; bmi.comp = 3;   /* BI_BITFIELDS */
	bmi.masc[0] = 0xF800; bmi.masc[1] = 0x07E0; bmi.masc[2] = 0x001F;
	SetDIBitsToDevice(dc ? dc : p->dc, 0, 0, (u32)C->an, (u32)C->al, 0, 0, 0, (u32)C->al, C->color, &bmi, 0);
}

/* los cuadros por segundo, en la traza (cada 5 s) */
static u32 fps_t0, fps_n;
int WINAPI wglSwapBuffers(HDC dc)
{
	Ctx *C = actual_ctx;
	if (!C) return 0;
	presenta(C, dc);
	al_tamano(C);
	if (traza_si) {
		u32 t = GetTickCount();
		fps_n++;
		if (!fps_t0) fps_t0 = t;
		if (t - fps_t0 >= 5000) { hgl_traza("cuadros por 10 s", fps_n * 10000 / (t - fps_t0)); fps_t0 = t; fps_n = 0; }
	}
	return 1;
}
int WINAPI wglSwapLayerBuffers(HDC dc, u32 planos) { (void)planos; return wglSwapBuffers(dc); }
void APIENTRY glFlush(void)
{
	Ctx *C = hgl_c;
	/* con un solo búfer, lo dibujado se ve al acabar */
	if (C && (!C->dobles || C->colb.dibuja == GL_FRONT || C->colb.dibuja == GL_FRONT_LEFT)) presenta(C, actual_dc);
}
void APIENTRY glFinish(void) { glFlush(); }
void APIENTRY glAddSwapHintRectWIN(GLint x, GLint y, GLsizei w, GLsizei h) { (void)x; (void)y; (void)w; (void)h; }
int WINAPI wglSwapIntervalEXT(int n) { (void)n; return 1; }
int WINAPI wglGetSwapIntervalEXT(void) { return 0; }

/* ---- ChangeDisplaySettings y EnumDisplaySettings (del puente) ---- */
static int cambia_modo(const u8 *m, u32 fl)
{
	if (!m) {
		modo_w = modo_h = 0;
		suelta_dd();
		hgl_traza("ChangeDisplaySettings: vuelve", 0);
		return 0;
	}
	if (fl & 2) return 0;                 /* CDS_TEST */
	if (*(const u32 *)(m + 40) & 0x180000) {
		modo_w = (int)*(const u32 *)(m + 108);
		modo_h = (int)*(const u32 *)(m + 112);
		hgl_traza("ChangeDisplaySettings", (u32)modo_w << 16 | (u32)modo_h);
	}
	return 0;                             /* DISP_CHANGE_SUCCESSFUL */
}
static const u16 modos[][2] = { { 320, 200 }, { 320, 240 }, { 400, 300 }, { 512, 384 }, { 640, 400 }, { 640, 480 }, { 800, 600 }, { 1024, 768 } };
static int enumera_modos(u32 n, u8 *m)
{
	u32 w, h;
	if (!m) return 0;
	if (n == 0xFFFFFFFF || n == 0xFFFFFFFE) {
		w = modo_w ? (u32)modo_w : (u32)GetSystemMetrics(0);
		h = modo_h ? (u32)modo_h : (u32)GetSystemMetrics(1);
	} else if (n < sizeof modos / sizeof modos[0]) {
		w = modos[n][0]; h = modos[n][1];
	} else return 0;
	*(u16 *)(m + 36) = 156;
	*(u32 *)(m + 40) = 0x5C0000;          /* DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY */
	*(u32 *)(m + 104) = 16;
	*(u32 *)(m + 108) = w;
	*(u32 *)(m + 112) = h;
	*(u32 *)(m + 120) = 60;
	return 1;
}

/* el puente: GDI32 y USER32 llaman aquí (HispaGLPuente) */
static u32 WINAPI puente(u32 op, u32 a, u32 b, u32 c)
{
	switch (op) {
	case 1: return (u32)wglSwapBuffers((HDC)a);
	case 2: return (u32)wglChoosePixelFormat((HDC)a, (const PFD *)b);
	case 3: return (u32)wglSetPixelFormat((HDC)a, (int)b, (const PFD *)c);
	case 4: return (u32)wglDescribePixelFormat(NULL, (int)a, b, (PFD *)c);
	case 5: return (u32)wglGetPixelFormat((HDC)a);
	case 6: return (u32)cambia_modo((const u8 *)a, b);
	case 7: return (u32)enumera_modos(a, (u8 *)b);
	}
	return 0;
}

/* ---- wglGetProcAddress ---- */
static HMODULE yo;
typedef void (*PROC)(void);
PROC WINAPI wglGetProcAddress(const char *n)
{
	void *p;
	if (!n) return NULL;
	if (!yo) yo = GetModuleHandleA("OPENGL32.DLL");
	p = yo ? GetProcAddress(yo, n) : NULL;
	hgl_traza(p ? "wglGetProcAddress" : "wglGetProcAddress: no", (u32)p);
	return (PROC)p;
}

/* ---- wglUseFontBitmapsA: las letras del DC, en listas con glBitmap ---- */
int WINAPI wglUseFontBitmapsA(HDC dc, u32 primera, u32 n, u32 base)
{
	HDC m = CreateCompatibleDC(dc);
	struct { u32 tam; i32 w, h; u16 planos, bits; u32 comp, tamimg; i32 xm, ym; u32 usados, imp; } bmi;
	u32 *bits = NULL;
	HANDLE mapa, viejo;
	u8 *bm;
	u32 k;
	int alto = 16, x, y;
	struct { i32 alto, asc, desc; } tm;
	if (!m) return 0;
	hgl_pon(&tm, 0, sizeof tm);
	{
		u8 t[64];
		hgl_pon(t, 0, sizeof t);
		if (GetTextMetricsA(dc, t)) { alto = *(i32 *)t; tm.desc = *(i32 *)(t + 8); }
	}
	if (alto <= 0 || alto > 64) alto = 16;
	hgl_pon(&bmi, 0, sizeof bmi);
	bmi.tam = 40; bmi.w = 64; bmi.h = -alto; bmi.planos = 1; bmi.bits = 32;
	mapa = CreateDIBSection(m, &bmi, 0, (void **)&bits, NULL, 0);
	bm = hgl_pedir(8 * 64);
	if (!mapa || !bits || !bm) { if (mapa) DeleteObject(mapa); DeleteDC(m); hgl_soltar(bm); return 0; }
	viejo = SelectObject(m, mapa);
	{
		HANDLE letra = GetCurrentObject(dc, 6 /* OBJ_FONT */);
		if (letra) SelectObject(m, letra);
	}
	SetTextColor(m, 0xFFFFFF);
	SetBkColor(m, 0);
	SetBkMode(m, 2);
	for (k = 0; k < n; k++) {
		char ch = (char)(primera + k);
		int tam[2] = { 8, alto }, w;
		GetTextExtentPoint32A(m, &ch, 1, tam);
		w = tam[0] > 0 && tam[0] <= 64 ? tam[0] : 8;
		for (y = 0; y < alto; y++) for (x = 0; x < 64; x++) bits[y * 64 + x] = 0;
		TextOutA(m, 0, 0, &ch, 1);
		hgl_pon(bm, 0, 8 * 64);
		/* glBitmap va de abajo arriba; las filas, de 8 octetos (con el almacén
		 * de desempaquetar del programa cambiado mientras: alineación 1, fila 64) */
		for (y = 0; y < alto; y++)
			for (x = 0; x < w; x++)
				if (bits[(alto - 1 - y) * 64 + x] & 0xFFFFFF) bm[y * 8 + (x >> 3)] |= (u8)(0x80 >> (x & 7));
		if (hgl_c) {
			Almacen antes = hgl_c->des;
			hgl_pon(&hgl_c->des, 0, sizeof(Almacen));
			hgl_c->des.alin = 1; hgl_c->des.fila = 64;
			glNewList(base + k, GL_COMPILE);
			glBitmap(w, alto, 0, (float)tm.desc, (float)w, 0, bm);
			glEndList();
			hgl_c->des = antes;
		}
	}
	SelectObject(m, viejo);
	DeleteObject(mapa);
	DeleteDC(m);
	hgl_soltar(bm);
	return 1;
}
int WINAPI wglUseFontBitmapsW(HDC dc, u32 a, u32 n, u32 b) { return wglUseFontBitmapsA(dc, a, n, b); }
int WINAPI wglUseFontOutlinesA(HDC dc, u32 a, u32 n, u32 b, float d, float e, int f, void *g) { (void)dc; (void)a; (void)n; (void)b; (void)d; (void)e; (void)f; (void)g; return 0; }
int WINAPI wglUseFontOutlinesW(HDC dc, u32 a, u32 n, u32 b, float d, float e, int f, void *g) { return wglUseFontOutlinesA(dc, a, n, b, d, e, f, g); }
int WINAPI wglDescribeLayerPlane(HDC dc, int f, int c, u32 n, void *p) { (void)dc; (void)f; (void)c; (void)n; (void)p; return 0; }
int WINAPI wglSetLayerPaletteEntries(HDC dc, int c, int i, int n, const u32 *p) { (void)dc; (void)c; (void)i; (void)n; (void)p; return 0; }
int WINAPI wglGetLayerPaletteEntries(HDC dc, int c, int i, int n, u32 *p) { (void)dc; (void)c; (void)i; (void)n; (void)p; return 0; }
int WINAPI wglRealizeLayerPalette(HDC dc, int c, int r) { (void)dc; (void)c; (void)r; return 0; }

/* ---- la entrada de la DLL ---- */
int WINAPI DllMain(HANDLE h, u32 razon, void *r)
{
	(void)r;
	if (razon == 1) {                     /* DLL_PROCESS_ATTACH */
		char e[64];
		int k;
		yo = h;
		e[0] = 0;
		GetEnvironmentVariableA("HISPAGL", e, sizeof e);
		for (k = 0; e[k]; k++) if (e[k] >= 'a' && e[k] <= 'z') e[k] = (char)(e[k] - 32);
		for (k = 0; e[k]; k++) {
			if (e[k] == 'R' && e[k + 1] == 'A' && e[k + 2] == 'P') hgl_calidad = 0;
			if (e[k] == 'E' && e[k + 1] == 'X' && e[k + 2] == 'A') hgl_calidad = 2;
			if (e[k] == 'V' && e[k + 1] == 'E' && e[k + 2] == 'N') solo_ventana = 1;
			if (e[k] == 'T' && e[k + 1] == 'R' && e[k + 2] == 'A') traza_si = 1;
		}
		HispaGLPuente((void *)puente);
		hgl_traza("OPENGL32.DLL: calidad", (u32)hgl_calidad);
	}
	if (razon == 0) {                     /* DLL_PROCESS_DETACH */
		suelta_dd();
		HispaGLPuente(NULL);
	}
	return 1;
}
