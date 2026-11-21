/* recursos.c - WIN32.BIN: lo de USER32 que sale de los recursos y lo que se
 * pinta con ellos: los iconos (los del programa y los de serie, hechos aquí),
 * los cursores, los mapas de bits (LoadBitmap, LoadImage, también de un
 * archivo .BMP o .ICO), los aceleradores, DrawText, DrawFrameControl,
 * GrayString y DrawState.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "ventanas.h"
#include "k32.h"

#define API(t) t WINAPI
#define MAGIA_ICONO 0x49434F4E
typedef struct { u32 magia; i32 an, al; u32 *p; i32 cx, cy; int cursor; } Icono;
HBITMAP WINAPI CreateDIBSection(HDC h, const void *bmi, u32 uso, void **bits, HANDLE sec, u32 off);
COLORREF WINAPI SetPixel(HDC h, i32 x, i32 y, COLORREF c);
COLORREF WINAPI GetPixel(HDC h, i32 x, i32 y);
BOOL WINAPI TextOutA(HDC h, i32 x, i32 y, const char *t, i32 n);
i32 WINAPI SaveDC(HDC h);
BOOL WINAPI RestoreDC(HDC h, i32 n);
i32 WINAPI IntersectClipRect(HDC h, i32 a, i32 b, i32 c, i32 d);
i32 WINAPI GetObjectA(HGDIOBJ h, i32 n, void *p);
BOOL WINAPI StretchBlt(HDC hd, i32 xd, i32 yd, i32 wd, i32 hd_, HDC hs, i32 xs, i32 ys, i32 ws, i32 hs_, u32 rop);
BOOL WINAPI Ellipse(HDC, i32, i32, i32, i32);
BOOL WINAPI Polygon(HDC h, const POINT *p, i32 n);
HPEN WINAPI CreatePen(i32 e, i32 a, COLORREF c);

static Icono *icono(HICON h)
{
	Icono *i = (Icono *)h;
	if (!i || (u32)i < 0x10000 || (u32)i >= 0x7FBF0000 || ((u32)i & 3) || i->magia != MAGIA_ICONO)
		return 0;
	return i;
}
static Icono *icono_nuevo(i32 an, i32 al)
{
	Icono *i = m_cero(sizeof *i);
	i->magia = MAGIA_ICONO;
	i->an = an;
	i->al = al;
	i->p = m_cero(an * al * 4);
	return i;
}
/* una imagen de icono (BITMAPINFOHEADER, colores, XOR y AND) -> icono */
static Icono *de_imagen(const u8 *p, u32 tam)
{
	const u32 *h = (const u32 *)p;
	i32 an, al, bpp, y, x;
	u32 ncol, paso, paso_m;
	const u8 *col, *bits, *masc;
	Icono *i;
	if (tam < 40 || h[0] != 40)
		return 0;
	an = h[1];
	al = (i32)h[2] / 2;
	bpp = ((const u16 *)p)[7];
	if (an <= 0 || al <= 0 || an > 256 || al > 256)
		return 0;
	ncol = bpp <= 8 ? (h[8] ? h[8] : 1u << bpp) : 0;
	col = p + 40;
	bits = col + ncol * 4;
	paso = ((an * bpp + 31) / 32) * 4;
	masc = bits + paso * al;
	paso_m = ((an + 31) / 32) * 4;
	i = icono_nuevo(an, al);
	for (y = 0; y < al; y++)
		for (x = 0; x < an; x++) {
			const u8 *f = bits + (al - 1 - y) * paso;
			u32 v = 0, a = 0xFF000000;
			switch (bpp) {
			case 1: { u32 k = (f[x >> 3] >> (7 - (x & 7))) & 1; v = *(const u32 *)(col + k * 4); break; }
			case 4: { u32 k = (f[x >> 1] >> ((x & 1) ? 0 : 4)) & 15; v = *(const u32 *)(col + k * 4); break; }
			case 8: v = *(const u32 *)(col + f[x] * 4); break;
			case 24: v = f[x * 3] | (f[x * 3 + 1] << 8) | (f[x * 3 + 2] << 16); break;
			case 32: v = *(const u32 *)(f + x * 4); a = v & 0xFF000000; break;
			}
			if (bpp < 32 || !a) {
				const u8 *m = masc + (al - 1 - y) * paso_m;
				if ((m[x >> 3] >> (7 - (x & 7))) & 1)
					a = 0;
				else
					a = 0xFF000000;
			}
			i->p[y * an + x] = (v & 0xFFFFFF) | (a >= 0x80000000 ? 0xFF000000 : 0);
		}
	return i;
}
/* los iconos de serie, dibujados aquí */
static Icono *de_serie(u32 cual)
{
	static Icono *hechos[8];
	u32 k = cual >= 32512 && cual <= 32518 ? cual - 32512 : 0;
	i32 x, y;
	Icono *i;
	if (hechos[k])
		return hechos[k];
	i = icono_nuevo(32, 32);
	for (y = 0; y < 32; y++)
		for (x = 0; x < 32; x++) {
			i32 dx = x - 16, dy = y - 16, d2 = dx * dx + dy * dy;
			u32 v = 0;
			switch (k) {
			case 1: case 3: case 4:     /* ERROR (X), QUESTION (?), INFORMATION (i): un círculo */
				if (d2 <= 196)
					v = 0xFF000000 | (k == 1 ? 0xD8382C : 0x2C6CD8);
				if (d2 > 196 && d2 <= 225)
					v = 0xFFFFFFFF;
				break;
			case 2: {                   /* WARNING: un triángulo */
				i32 med = (y - 3) * 13 / 26;
				if (y >= 3 && y <= 29 && dx >= -med - 1 && dx <= med + 1)
					v = 0xFF000000 | 0xF0C020;
				break;
			}
			default:                    /* APPLICATION: una ventanita */
				if (x >= 3 && x <= 28 && y >= 5 && y <= 27) {
					v = 0xFFFFFFFF;
					if (y <= 9) v = 0xFF3F8DEA;
					if (x == 3 || x == 28 || y == 5 || y == 27) v = 0xFF30405A;
				}
			}
			i->p[y * 32 + x] = v;
		}
	/* lo de dentro: X, ? o i, en blanco; ! en negro */
	{
		static const char *dibujo[5][9] = {
			{ 0 },
			{ "X     X", " X   X ", "  X X  ", "   X   ", "  X X  ", " X   X ", "X     X", 0 },
			{ "   X   ", "   X   ", "   X   ", "   X   ", "   X   ", "       ", "   X   ", 0 },
			{ " XXXXX ", "X     X", "     X ", "   XX  ", "   X   ", "       ", "   X   ", 0 },
			{ "   X   ", "       ", "  XX   ", "   X   ", "   X   ", "   X   ", "  XXX  ", 0 },
		};
		if (k >= 1 && k <= 4) {
			i32 f, c;
			for (f = 0; dibujo[k][f]; f++)
				for (c = 0; c < 7; c++)
					if (dibujo[k][f][c] == 'X') {
						i32 a, b;
						for (a = 0; a < 2; a++)
							for (b = 0; b < 2; b++)
								i->p[(9 + f * 2 + a + (k == 2 ? 3 : 0)) * 32 + 9 + c * 2 + b] = k == 2 ? 0xFF000000 : 0xFFFFFFFF;
					}
		}
	}
	hechos[k] = i;
	return i;
}
static Icono *icono_de_recurso(HINSTANCE inst, const char *n, int cursor, i32 quiere)
{
	u32 tam, k, mejor = 0;
	const u8 *g = recurso(inst, (const char *)(cursor ? 12 : 14), n, &tam);
	i32 dif = 1 << 30;
	const u8 *img;
	if (!g)
		return 0;
	for (k = 0; k < ((const u16 *)g)[2]; k++) {
		const u8 *e = g + 6 + k * 14;
		i32 an = e[0] ? e[0] : 256, d = an > quiere ? an - quiere : quiere - an;
		u32 bpp = ((const u16 *)(e + 6))[0];
		d = d * 64 + (32 - (i32)(bpp > 32 ? 32 : bpp));
		if (d < dif) {
			dif = d;
			mejor = k;
		}
	}
	img = recurso(inst, (const char *)(cursor ? 1 : 3), (const char *)(u32)((const u16 *)(g + 6 + mejor * 14 + 12))[0], &tam);
	if (!img)
		return 0;
	if (cursor) {
		img += 4;                 /* (el punto caliente) */
		tam -= 4;
	}
	return de_imagen(img, tam);
}
API(HICON) LoadIconA(HINSTANCE inst, const char *n)
{
	Icono *i = 0;
	if (inst && n)
		i = icono_de_recurso(inst, n, 0, 32);
	if (!i && (!inst || ((u32)n >= 32512 && (u32)n <= 32518)))
		i = de_serie((u32)n);
	if (!i && mod_exe)
		i = icono_de_recurso((HINSTANCE)mod_exe->base, n, 0, 32);
	return (HICON)i;
}
API(HICON) LoadIconW(HINSTANCE inst, const WCHAR *n)
{
	char b[64];
	if ((u32)n < 0x10000)
		return LoadIconA(inst, (const char *)n);
	de_w(b, n, sizeof b);
	return LoadIconA(inst, b);
}
API(HCURSOR) LoadCursorA(HINSTANCE inst, const char *n)
{
	static Icono flecha;
	(void)inst; (void)n;
	flecha.magia = MAGIA_ICONO;
	flecha.cursor = 1;
	return (HCURSOR)&flecha;
}
API(HCURSOR) LoadCursorW(HINSTANCE inst, const WCHAR *n) { return LoadCursorA(inst, (const char *)n); }
API(HCURSOR) LoadCursorFromFileA(const char *n) { return LoadCursorA(0, n); }
API(BOOL) DestroyIcon(HICON h) { (void)h; return TRUE; }
API(BOOL) DestroyCursor(HCURSOR h) { (void)h; return TRUE; }
API(HICON) CopyIcon(HICON h) { return h; }
API(HICON) CreateIconIndirect(const u32 *ii)
{
	/* ICONINFO: fIcon, x, y, hbmMask, hbmColor */
	u32 bm[6];
	Icono *i;
	i32 x, y;
	HDC d;
	if (!ii[4] || !GetObjectA((HGDIOBJ)ii[4], sizeof bm, bm))
		return 0;
	i = icono_nuevo(bm[1], bm[2]);
	d = CreateCompatibleDC(0);
	SelectObject(d, (HGDIOBJ)ii[4]);
	for (y = 0; y < (i32)bm[2]; y++)
		for (x = 0; x < (i32)bm[1]; x++)
			i->p[y * bm[1] + x] = rgb(GetPixel(d, x, y)) | 0xFF000000;
	if (ii[3]) {
		u32 mm[6];
		GetObjectA((HGDIOBJ)ii[3], sizeof mm, mm);
		SelectObject(d, (HGDIOBJ)ii[3]);
		for (y = 0; y < (i32)bm[2] && y < (i32)mm[2]; y++)
			for (x = 0; x < (i32)bm[1]; x++)
				if (GetPixel(d, x, y) & 0xFFFFFF)
					i->p[y * bm[1] + x] &= 0xFFFFFF;
	}
	DeleteDC(d);
	return (HICON)i;
}
API(HICON) CreateIcon(HINSTANCE inst, i32 an, i32 al, u8 planos, u8 bpp, const u8 *y_, const u8 *x_)
{
	Icono *i = icono_nuevo(an, al);
	i32 x, y;
	u32 pm = ((an + 15) / 16) * 2;
	(void)inst; (void)planos;
	for (y = 0; y < al; y++)
		for (x = 0; x < an; x++) {
			u32 m = (y_[y * pm + (x >> 3)] >> (7 - (x & 7))) & 1;
			u32 c = bpp == 1 ? ((x_[y * pm + (x >> 3)] >> (7 - (x & 7))) & 1 ? 0xFFFFFF : 0) : 0;
			i->p[y * an + x] = m ? 0 : (0xFF000000 | c);
		}
	return (HICON)i;
}
API(BOOL) GetIconInfo(HICON h, u32 *ii)
{
	Icono *i = icono(h);
	if (!i)
		return FALSE;
	ii[0] = !i->cursor;
	ii[1] = i->an / 2;
	ii[2] = i->al / 2;
	ii[3] = 0;
	ii[4] = 0;
	return TRUE;
}
void dibuja_icono(HDC h, i32 x, i32 y, HICON ic, i32 an, i32 al)
{
	Icono *i = icono(ic);
	i32 a, b;
	if (!i || !i->p)
		return;
	if (an <= 0) an = i->an;
	if (al <= 0) al = i->al;
	for (b = 0; b < al; b++)
		for (a = 0; a < an; a++) {
			u32 v = i->p[(b * i->al / al) * i->an + a * i->an / an];
			if (v >> 24)
				SetPixel(h, x + a, y + b, ((v >> 16) & 0xFF) | (v & 0xFF00) | ((v & 0xFF) << 16));
		}
}
API(BOOL) DrawIcon(HDC h, i32 x, i32 y, HICON i) { dibuja_icono(h, x, y, i, 32, 32); return TRUE; }
API(BOOL) DrawIconEx(HDC h, i32 x, i32 y, HICON i, i32 an, i32 al, u32 paso, HBRUSH b, u32 fl)
{
	(void)paso; (void)fl;
	if (b) {
		RECT r = { x, y, x + (an ? an : 32), y + (al ? al : 32) };
		FillRect(h, &r, b);
	}
	dibuja_icono(h, x, y, i, an, al);
	return TRUE;
}
API(HICON) LookupIconIdFromDirectory(const u8 *p, BOOL icono_) { (void)icono_; return (HICON)(u32)((const u16 *)(p + 6 + 12))[0]; }
API(HICON) CreateIconFromResourceEx(const u8 *p, u32 tam, BOOL icono_, u32 v, i32 an, i32 al, u32 fl) { (void)v; (void)an; (void)al; (void)fl; return (HICON)de_imagen(icono_ ? p : p + 4, icono_ ? tam : tam - 4); }
API(HICON) CreateIconFromResource(const u8 *p, u32 tam, BOOL icono_, u32 v) { return CreateIconFromResourceEx(p, tam, icono_, v, 0, 0, 0); }

/* ---- los mapas de bits ---- */
API(HBITMAP) LoadBitmapA(HINSTANCE inst, const char *n)
{
	u32 tam;
	const u8 *p;
	if (!inst) {
		/* OBM_...: uno pequeño (una marca, una flecha) */
		HBITMAP b = CreateBitmap(16, 16, 1, 1, 0);
		return b;
	}
	p = recurso(inst, (const char *)2, n, &tam);
	if (!p) {
		pon_error(1814);
		return 0;
	}
	return mapa_de_dib(p, tam);
}
API(HBITMAP) LoadBitmapW(HINSTANCE inst, const WCHAR *n)
{
	char b[64];
	if ((u32)n < 0x10000)
		return LoadBitmapA(inst, (const char *)n);
	de_w(b, n, sizeof b);
	return LoadBitmapA(inst, b);
}
static u8 *leer_archivo(const char *n, u32 *tam)
{
	char c[260], e[260];
	u32 m, t;
	u8 *b;
	ruta_entera(n, e, sizeof e);
	a437(c, e, sizeof c);
	m = sys_abrir(c, 0);
	if (ES_ERROR(m))
		return 0;
	t = sys_largo(m);
	b = m_pedir(t + 4);
	if (sys_leer(m, b, t) != t) {
		m_soltar(b);
		sys_cerrar(m);
		return 0;
	}
	sys_cerrar(m);
	*tam = t;
	return b;
}
/* un DIB (cabecera y puntos) -> una sección DIB con su formato */
static HBITMAP seccion_de(const u8 *p)
{
	const u32 *h = (const u32 *)p;
	void *bits;
	HBITMAP b;
	u32 bpp = ((const u16 *)p)[7], ncol = bpp <= 8 ? (h[8] ? h[8] : 1u << bpp) : (h[4] == 3 ? 3 : 0);
	i32 al = (i32)h[2] < 0 ? -(i32)h[2] : (i32)h[2];
	u32 tam = ((h[1] * bpp + 31) / 32) * 4 * al;
	b = CreateDIBSection(0, p, 0, &bits, 0, 0);
	if (b && bits)
		memcpy(bits, p + h[0] + ncol * 4, tam);
	return b;
}
API(HANDLE) LoadImageA(HINSTANCE inst, const char *n, u32 tipo, i32 an, i32 al, u32 fl)
{
	traza("LoadImageA(%x, %u, %x)", (u32)n, tipo, fl);
	if (fl & 0x10) {                                      /* LR_LOADFROMFILE */
		u32 tam;
		u8 *b = leer_archivo(n, &tam);
		HANDLE r = 0;
		if (!b)
			return 0;
		if (tipo == 0 && b[0] == 'B' && b[1] == 'M')
			r = (fl & 0x2000) ? (HANDLE)seccion_de(b + 14) : (HANDLE)mapa_de_dib(b + 14, tam - 14);
		else if ((tipo == 1 || tipo == 2) && tam > 22) {
			u32 off = *(const u32 *)(b + 6 + 12), t = *(const u32 *)(b + 6 + 8);
			if (off + t <= tam)
				r = (HANDLE)de_imagen(b + off + (tipo == 2 && 0 ? 4 : 0), t);
		}
		m_soltar(b);
		return r;
	}
	if (tipo == 0) {
		if (fl & 0x2000) {
			u32 tam;
			const u8 *p = recurso(inst, (const char *)2, n, &tam);
			return p ? (HANDLE)seccion_de(p) : 0;
		}
		return (HANDLE)LoadBitmapA(inst, n);
	}
	if (tipo == 1) {
		Icono *i = inst ? icono_de_recurso(inst, n, 0, an ? an : 32) : 0;
		(void)al;
		return i ? (HANDLE)i : (HANDLE)LoadIconA(inst, n);
	}
	return (HANDLE)LoadCursorA(inst, n);
}
API(HANDLE) LoadImageW(HINSTANCE inst, const WCHAR *n, u32 tipo, i32 an, i32 al, u32 fl)
{
	char b[260];
	if ((u32)n < 0x10000)
		return LoadImageA(inst, (const char *)n, tipo, an, al, fl);
	de_w(b, n, sizeof b);
	return LoadImageA(inst, b, tipo, an, al, fl);
}
API(HANDLE) CopyImage(HANDLE h, u32 t, i32 an, i32 al, u32 fl) { (void)t; (void)an; (void)al; (void)fl; return h; }

/* ---- los aceleradores ---- */
typedef struct { u32 magia; i32 n; struct { u8 fl; u16 tecla, orden; } e[1]; } Acel;
#define MAGIA_ACEL 0x4143454C
static Acel *nuevo_acel(i32 n)
{
	Acel *a = m_cero(sizeof(Acel) + n * 8);
	a->magia = MAGIA_ACEL;
	a->n = n;
	return a;
}
API(HACCEL) LoadAcceleratorsA(HINSTANCE inst, const char *n)
{
	u32 tam, k;
	const u16 *p = recurso(inst, (const char *)9, n, &tam);
	Acel *a;
	if (!p)
		return 0;
	a = nuevo_acel(tam / 8);
	for (k = 0; k < tam / 8; k++) {
		a->e[k].fl = p[k * 4] & 0x7F;
		a->e[k].tecla = p[k * 4 + 1];
		a->e[k].orden = p[k * 4 + 2];
		if (p[k * 4] & 0x80) {
			a->n = k + 1;
			break;
		}
	}
	return (HACCEL)a;
}
API(HACCEL) LoadAcceleratorsW(HINSTANCE inst, const WCHAR *n)
{
	char b[64];
	if ((u32)n < 0x10000)
		return LoadAcceleratorsA(inst, (const char *)n);
	de_w(b, n, sizeof b);
	return LoadAcceleratorsA(inst, b);
}
API(HACCEL) CreateAcceleratorTableA(const u8 *t, i32 n)
{
	Acel *a = nuevo_acel(n);
	i32 k;
	for (k = 0; k < n; k++) {
		a->e[k].fl = t[k * 6];
		a->e[k].tecla = *(const u16 *)(t + k * 6 + 2);
		a->e[k].orden = *(const u16 *)(t + k * 6 + 4);
	}
	return (HACCEL)a;
}
API(BOOL) DestroyAcceleratorTable(HACCEL h) { Acel *a = (Acel *)h; if (a && a->magia == MAGIA_ACEL) { a->magia = 0; m_soltar(a); } return TRUE; }
API(i32) CopyAcceleratorTableA(HACCEL h, u8 *d, i32 n)
{
	Acel *a = (Acel *)h;
	i32 k;
	if (!a || a->magia != MAGIA_ACEL)
		return 0;
	if (!d)
		return a->n;
	for (k = 0; k < n && k < a->n; k++) {
		d[k * 6] = a->e[k].fl;
		*(u16 *)(d + k * 6 + 2) = a->e[k].tecla;
		*(u16 *)(d + k * 6 + 4) = a->e[k].orden;
	}
	return k;
}
int tragar_char;
API(i32) TranslateAcceleratorA(HWND h, HACCEL ha, MSG *m)
{
	Acel *a = (Acel *)ha;
	i32 k;
	if (!a || a->magia != MAGIA_ACEL || !h)
		return 0;
	if (m->message != WM_KEYDOWN && m->message != WM_SYSKEYDOWN && m->message != WM_CHAR && m->message != WM_SYSCHAR)
		return 0;
	for (k = 0; k < a->n; k++) {
		u8 fl = a->e[k].fl;
		if (fl & 1) {                                     /* FVIRTKEY */
			if (m->message != WM_KEYDOWN && m->message != WM_SYSKEYDOWN)
				continue;
			if (a->e[k].tecla != m->wParam)
				continue;
			if (!!(fl & 4) != !!teclas_mayus() || !!(fl & 8) != !!teclas_control() || !!(fl & 0x10) != !!teclas_alt())
				continue;
		} else {
			if (m->message != WM_CHAR && m->message != WM_SYSCHAR)
				continue;
			if (a->e[k].tecla != m->wParam)
				continue;
			if (!!(fl & 0x10) != (m->message == WM_SYSCHAR))
				continue;
		}
		traza("acelerador %u", a->e[k].orden);
		{
			Wnd *w = wnd(h);
			Wnd *r = w ? raiz_de(w) : 0;
			if (r && r->menu)
				SendMessageA((HWND)r, WM_INITMENU, (WPARAM)r->menu, 0);
		}
		SendMessageA(h, WM_COMMAND, MAKELONG(a->e[k].orden, 1), 0);
		if (m->message == WM_KEYDOWN || m->message == WM_SYSKEYDOWN)
			tragar_char = 1;
		return 1;
	}
	return 0;
}
/* EXPORTA TranslateAccelerator=TranslateAcceleratorA */
API(i32) TranslateAcceleratorW(HWND h, HACCEL a, MSG *m) { return TranslateAcceleratorA(h, a, m); }
API(BOOL) TranslateMDISysAccel(HWND h, MSG *m) { (void)h; (void)m; return FALSE; }

/* ---------------------------------------------------------------------------
 *  DrawText
 * ------------------------------------------------------------------------- */
#define DT_CENTER 1
#define DT_RIGHT 2
#define DT_VCENTER 4
#define DT_BOTTOM 8
#define DT_WORDBREAK 0x10
#define DT_SINGLELINE 0x20
#define DT_EXPANDTABS 0x40
#define DT_NOCLIP 0x100
#define DT_CALCRECT 0x400
#define DT_NOPREFIX 0x800
#define DT_END_ELLIPSIS 0x8000
#define DT_PATH_ELLIPSIS 0x4000
#define DT_WORD_ELLIPSIS 0x40000
API(i32) DrawTextExA(HDC h, char *t, i32 n, RECT *r, u32 f, void *p);
API(i32) DrawTextA(HDC h, const char *t, i32 n, RECT *r, u32 f)
{
	Dc *d = dc(h);
	i32 cw = ancho_letra(d), ch = alto_letra(d), ancho = r->right - r->left, max_an = 0, y, lineas = 0, k;
	char linea[1024];
	i32 sub[1024 / 8];
	const char *p, *fin;
	struct { i32 ini, n; } trozos[256];
	char todo[2048];
	i32 nt = 0, o = 0;
	if (!t)
		return 0;
	if (n < 0)
		n = strlen(t);
	if (n > 2000)
		n = 2000;
	(void)sub;
	/* sin «&» (apuntando cuál va subrayada) y con los tabuladores */
	{
		i32 i;
		for (i = 0; i < n && o < (i32)sizeof todo - 9; i++) {
			char c = t[i];
			if (c == '&' && !(f & DT_NOPREFIX)) {
				if (i + 1 < n && t[i + 1] == '&') {
					todo[o++] = '&';
					i++;
				} else
					todo[o++] = '\x01';           /* (marca: la de detrás va subrayada) */
				continue;
			}
			if (c == '\t' && (f & DT_EXPANDTABS)) {
				i32 col = 0, j;
				for (j = o - 1; j >= 0 && todo[j] != '\n'; j--)
					if (todo[j] != '\x01' && todo[j] != '\r')
						col++;
				do todo[o++] = ' '; while ((++col) % 8 && o < (i32)sizeof todo - 1);
				continue;
			}
			todo[o++] = c;
		}
		todo[o] = 0;
	}
	/* los trozos (líneas), partiendo por las palabras si hace falta */
	p = todo;
	fin = todo + o;
	while (p < fin && nt < 256) {
		const char *e = p, *corte = 0;
		i32 col = 0;
		while (e < fin && *e != '\n' && !(*e == '\r' && !(f & DT_SINGLELINE))) {
			if (*e == '\x01') { e++; continue; }
			if ((f & DT_WORDBREAK) && !(f & DT_SINGLELINE) && ancho > 0 && (col + 1) * cw > ancho && col > 0) {
				if (corte && corte > p)
					e = corte;
				break;
			}
			if (*e == ' ')
				corte = e + 1;
			col++;
			e++;
		}
		if (f & DT_SINGLELINE) {
			/* (en una línea: los saltos se ven como nada) */
			while (e < fin) e++;
		}
		trozos[nt].ini = p - todo;
		trozos[nt].n = e - p;
		nt++;
		p = e;
		if (p < fin && *p == '\r') p++;
		if (p < fin && *p == '\n') p++;
		else if ((f & DT_WORDBREAK) && p < fin && *p == ' ') {
			while (p < fin && *p == ' ')
				p++;
		}
	}
	if (!nt) {
		trozos[0].ini = 0;
		trozos[0].n = 0;
		nt = 1;
	}
	lineas = nt;
	for (k = 0; k < nt; k++) {
		i32 i, c = 0;
		for (i = 0; i < trozos[k].n; i++)
			if (todo[trozos[k].ini + i] != '\x01' && todo[trozos[k].ini + i] != '\r' && todo[trozos[k].ini + i] != '\n')
				c++;
		while (c > 0 && todo[trozos[k].ini + trozos[k].n - 1] == ' ' && (f & DT_WORDBREAK)) {
			trozos[k].n--;
			c--;
		}
		max_an = MAX(max_an, c * cw);
	}
	if (f & DT_CALCRECT) {
		r->right = r->left + ((f & DT_WORDBREAK) && ancho > 0 && max_an < ancho && !(f & DT_SINGLELINE) && 0 ? ancho : max_an);
		r->bottom = r->top + lineas * ch;
		return lineas * ch;
	}
	y = r->top;
	if (f & DT_SINGLELINE) {
		if (f & DT_VCENTER) y = r->top + (r->bottom - r->top - ch) / 2;
		else if (f & DT_BOTTOM) y = r->bottom - ch;
	}
	{
		i32 s = 0;
		if (!(f & DT_NOCLIP)) {
			s = SaveDC(h);
			IntersectClipRect(h, r->left, r->top, r->right, r->bottom);
		}
		for (k = 0; k < nt; k++, y += ch) {
			i32 i, c = 0, x, sub_x = -1;
			for (i = 0; i < trozos[k].n && c < (i32)sizeof linea - 1; i++) {
				char x0 = todo[trozos[k].ini + i];
				if (x0 == '\x01') { sub_x = c; continue; }
				if (x0 == '\r' || x0 == '\n') continue;
				linea[c++] = x0;
			}
			linea[c] = 0;
			if ((f & (DT_END_ELLIPSIS | DT_PATH_ELLIPSIS | DT_WORD_ELLIPSIS)) && ancho > 0 && c * cw > ancho && ancho / cw > 3) {
				c = ancho / cw;
				linea[c - 3] = linea[c - 2] = linea[c - 1] = '.';
				linea[c] = 0;
			}
			x = r->left;
			if (f & DT_CENTER) x = r->left + (ancho - c * cw) / 2;
			else if (f & DT_RIGHT) x = r->right - c * cw;
			TextOutA(h, x, y, linea, c);
			if (sub_x >= 0 && sub_x < c) {
				RECT u = { x + sub_x * cw, y + ch - 2, x + (sub_x + 1) * cw, y + ch - 1 };
				HBRUSH b = CreateSolidBrush(d ? d->tinta : 0);
				FillRect(h, &u, b);
				DeleteObject(b);
			}
		}
		if (s)
			RestoreDC(h, s);
	}
	return lineas * ch;
}
API(i32) DrawTextW(HDC h, const WCHAR *t, i32 n, RECT *r, u32 f)
{
	char b[2048];
	i32 k;
	if (n < 0)
		for (n = 0; t[n]; n++)
			;
	if (n > 2047)
		n = 2047;
	for (k = 0; k < n; k++)
		b[k] = de_uni(t[k]);
	return DrawTextA(h, b, n, r, f);
}
API(i32) DrawTextExA(HDC h, char *t, i32 n, RECT *r, u32 f, void *p) { (void)p; return DrawTextA(h, t, n, r, f); }
API(BOOL) GrayStringA(HDC h, HBRUSH b, void *f, LPARAM d, i32 n, i32 x, i32 y, i32 an, i32 al)
{
	COLORREF c = SetTextColor(h, GetSysColor(COLOR_GRAYTEXT));
	(void)b; (void)an; (void)al;
	if (!f)
		TextOutA(h, x, y, (const char *)d, n ? n : (i32)strlen((const char *)d));
	SetTextColor(h, c);
	return TRUE;
}
API(BOOL) DrawStateA(HDC h, HBRUSH b, void *f, LPARAM d, WPARAM wd, i32 x, i32 y, i32 cx, i32 cy, u32 fl)
{
	u32 t = fl & 15;
	COLORREF c = 0;
	(void)b; (void)f;
	if (fl & 0x20)
		c = SetTextColor(h, GetSysColor(COLOR_GRAYTEXT));
	if (t == 1 || t == 2) {
		RECT r = { x, y, x + (cx ? cx : 1000), y + (cy ? cy : 100) };
		DrawTextA(h, (const char *)d, wd ? (i32)wd : -1, &r, t == 1 ? DT_NOPREFIX : 0);
	} else if (t == 3)
		dibuja_icono(h, x, y, (HICON)d, cx, cy);
	else if (t == 4) {
		HDC m = CreateCompatibleDC(h);
		u32 bm[6];
		GetObjectA((HGDIOBJ)d, sizeof bm, bm);
		SelectObject(m, (HGDIOBJ)d);
		StretchBlt(h, x, y, cx ? cx : (i32)bm[1], cy ? cy : (i32)bm[2], m, 0, 0, bm[1], bm[2], 0x00CC0020);
		DeleteDC(m);
	}
	if (fl & 0x20)
		SetTextColor(h, c);
	return TRUE;
}
API(BOOL) DrawFrameControl(HDC h, RECT *r, u32 tipo, u32 estado)
{
	if (tipo == 4) {                                      /* DFC_BUTTON */
		u32 cual = estado & 0x1F;
		if (cual == 0x10) {                               /* DFCS_BUTTONPUSH */
			relleno_3d(h, *r, (estado & 0x200) != 0);
			return TRUE;
		}
		FillRect(h, r, GetSysColorBrush(COLOR_WINDOW));
		DrawEdge(h, r, 0x0A, 0x0F);
		if (estado & 0x400) {
			RECT c = *r;
			InflateRect_(&c, -3, -3);
			if (cual == 4) {
				HGDIOBJ vb = SelectObject(h, GetSysColorBrush(COLOR_BTNTEXT));
				Ellipse(h, c.left, c.top, c.right, c.bottom);
				SelectObject(h, vb);
			} else
				FillRect(h, &c, GetSysColorBrush(COLOR_BTNTEXT));
		}
		return TRUE;
	}
	if (tipo == 3) {                                      /* DFC_SCROLL */
		relleno_3d(h, *r, (estado & 0x200) != 0);
		return TRUE;
	}
	relleno_3d(h, *r, 0);
	return TRUE;
}
API(BOOL) DrawCaption(HWND w, HDC h, const RECT *r, u32 f) { (void)w; (void)f; FillRect(h, r, GetSysColorBrush(COLOR_ACTIVECAPTION)); return TRUE; }
API(BOOL) DrawAnimatedRects(HWND w, i32 a, const RECT *x, const RECT *y) { (void)w; (void)a; (void)x; (void)y; return TRUE; }
