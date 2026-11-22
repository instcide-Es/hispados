/* user32.c - WIN32.BIN: USER32, lo principal: las clases, las ventanas (cada
 * una de primer nivel, una de Hispa-Pentium con su lienzo: ventanas.h), los
 * mensajes y su cola, lo que llega del núcleo (el ratón, las teclas, cerrar,
 * el tamaño, el foco, los menús) hecho mensajes de Windows, el pintado
 * (WM_PAINT, BeginPaint) y llevar al núcleo lo pintado; el foco, la
 * captura, los temporizadores, el cursor de texto, el portapapeles, las
 * medidas del sistema y lo demás. Los controles (controles.c), los menús
 * (menus.c), los cuadros (dialogos.c) y DirectX (dx.c), aparte.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "ventanas.h"
#include "k32.h"

#define API(t) t WINAPI
int pantalla_an = 800, pantalla_al = 600;
Wnd *escritorio, *foco, *activa, *captura;
extern int es_ventanas;
extern void (*al_salir_user)(void);
void *recurso(HMODULE h, const char *tipo, const char *nombre, u32 *tam);
u32 WINAPI GetTickCount(void);

/* ---------------------------------------------------------------------------
 *  Los colores del sistema (los de Hispa-Pentium: el aire de su escritorio)
 * ------------------------------------------------------------------------- */
COLORREF colores_sistema[31] = {
	RGB(0xD4, 0xDC, 0xE8),   /* SCROLLBAR */
	RGB(0x15, 0x4C, 0x9C),   /* BACKGROUND (el tapiz) */
	RGB(0x3F, 0x8D, 0xEA),   /* ACTIVECAPTION */
	RGB(0x9D, 0xB7, 0xD9),   /* INACTIVECAPTION */
	RGB(0xF1, 0xF5, 0xFB),   /* MENU */
	RGB(0xFF, 0xFF, 0xFF),   /* WINDOW */
	RGB(0x30, 0x3C, 0x50),   /* WINDOWFRAME */
	RGB(0x10, 0x18, 0x28),   /* MENUTEXT */
	RGB(0x10, 0x18, 0x28),   /* WINDOWTEXT */
	RGB(0xFF, 0xFF, 0xFF),   /* CAPTIONTEXT */
	RGB(0x7F, 0xA6, 0xDC),   /* ACTIVEBORDER */
	RGB(0xB8, 0xC8, 0xE0),   /* INACTIVEBORDER */
	RGB(0xD0, 0xD8, 0xE4),   /* APPWORKSPACE */
	RGB(0x3F, 0x86, 0xE0),   /* HIGHLIGHT */
	RGB(0xFF, 0xFF, 0xFF),   /* HIGHLIGHTTEXT */
	RGB(0xE6, 0xEC, 0xF5),   /* BTNFACE */
	RGB(0x8C, 0x9C, 0xB4),   /* BTNSHADOW */
	RGB(0x88, 0x90, 0x9C),   /* GRAYTEXT */
	RGB(0x10, 0x18, 0x28),   /* BTNTEXT */
	RGB(0xE0, 0xE8, 0xF4),   /* INACTIVECAPTIONTEXT */
	RGB(0xFF, 0xFF, 0xFF),   /* BTNHIGHLIGHT */
	RGB(0x4A, 0x58, 0x70),   /* 3DDKSHADOW */
	RGB(0xF4, 0xF7, 0xFC),   /* 3DLIGHT */
	RGB(0x10, 0x18, 0x28),   /* INFOTEXT */
	RGB(0xFF, 0xF8, 0xD8),   /* INFOBK */
	RGB(0, 0, 0),
	RGB(0x00, 0x66, 0xCC),   /* HOTLIGHT */
	RGB(0x3F, 0x8D, 0xEA),
	RGB(0x9D, 0xB7, 0xD9),
	RGB(0x3F, 0x86, 0xE0),   /* MENUHILIGHT */
	RGB(0xF1, 0xF5, 0xFB),   /* MENUBAR */
};
static HBRUSH brochas_sistema[31];
API(u32) GetSysColor(i32 i) { return i >= 0 && i < 31 ? colores_sistema[i] : 0; }
API(HBRUSH) GetSysColorBrush(i32 i)
{
	if (i < 0 || i >= 31)
		return 0;
	if (!brochas_sistema[i]) {
		brochas_sistema[i] = CreateSolidBrush(colores_sistema[i]);
		((Gdi *)brochas_sistema[i])->stock = 1;
	}
	return brochas_sistema[i];
}
API(BOOL) SetSysColors(i32 n, const i32 *e, const COLORREF *c)
{
	i32 k;
	for (k = 0; k < n; k++)
		if (e[k] >= 0 && e[k] < 31) {
			colores_sistema[e[k]] = c[k];
			if (brochas_sistema[e[k]])
				((Gdi *)brochas_sistema[e[k]])->u.brocha.color = c[k];
		}
	return TRUE;
}

/* ---------------------------------------------------------------------------
 *  Las clases
 * ------------------------------------------------------------------------- */
static Clase *clases;
typedef struct { u32 estilo; WNDPROC proc; i32 extra_c, extra; HINSTANCE inst; HICON ic; HCURSOR cur; HBRUSH fondo; const char *menu, *nombre; } WNDCLASSA;
typedef struct { u32 tam, estilo; WNDPROC proc; i32 extra_c, extra; HINSTANCE inst; HICON ic; HCURSOR cur; HBRUSH fondo; const char *menu, *nombre; HICON ic_peq; } WNDCLASSEXA;
u16 WINAPI AddAtomA(const char *n);
u32 WINAPI GetAtomNameA(u16 a, char *d, i32 n);
Clase *clase_de(const char *n)
{
	Clase *c;
	char b[64];
	if ((u32)n < 0x10000) {
		u16 a = (u16)(u32)n;
		for (c = clases; c; c = c->sig)
			if (c->atomo == a)
				return c;
		if (a == 0x8002)          /* #32770 */
			n = "#32770";
		else {
			if (!GetAtomNameA(a, b, sizeof b))
				return 0;
			n = b;
		}
	}
	for (c = clases; c; c = c->sig)
		if (!stricmp_(c->nombre, n))
			return c;
	return 0;
}
static Clase *clase_nueva(const char *nombre, u32 estilo, WNDPROC proc, i32 ec, i32 e, HINSTANCE inst, HICON ic, HCURSOR cur, HBRUSH fondo, const char *menu)
{
	Clase *c;
	char n[64];
	if ((u32)nombre < 0x10000) {
		if (!GetAtomNameA((u16)(u32)nombre, n, sizeof n))
			return 0;
	} else
		copia_n(n, nombre, sizeof n);
	if ((c = clase_de(n)) && !c->propia) {
		pon_error(1410);           /* ERROR_CLASS_ALREADY_EXISTS */
		return 0;
	}
	if (!c) {
		c = m_cero(sizeof *c);
		c->sig = clases;
		clases = c;
	}
	copia_n(c->nombre, n, sizeof c->nombre);
	c->estilo = estilo;
	c->proc = proc;
	c->extra_clase = ec;
	c->extra = e;
	c->inst = inst;
	c->icono = ic;
	c->cursor = cur;
	c->fondo = fondo;
	c->datos_clase = ec > 0 ? m_cero(ec) : 0;
	if ((u32)menu >= 0x10000)
		copia_n(c->menu, menu, sizeof c->menu);
	else
		c->menu_id = (u32)menu;
	c->atomo = AddAtomA(c->nombre);
	return c;
}
API(u16) RegisterClassA(const WNDCLASSA *w)
{
	Clase *c = clase_nueva(w->nombre, w->estilo, w->proc, w->extra_c, w->extra, w->inst, w->ic, w->cur, w->fondo, w->menu);
	traza("RegisterClassA(%s) = %x", (u32)w->nombre < 0x10000 ? "#" : w->nombre, c ? c->atomo : 0);
	return c ? c->atomo : 0;
}
API(u16) RegisterClassExA(const WNDCLASSEXA *w)
{
	Clase *c = clase_nueva(w->nombre, w->estilo, w->proc, w->extra_c, w->extra, w->inst, w->ic, w->cur, w->fondo, w->menu);
	if (c)
		c->icono_peq = w->ic_peq;
	return c ? c->atomo : 0;
}
API(u16) RegisterClassW(const WNDCLASSA *w)
{
	WNDCLASSA a = *w;
	char n[64], m[64];
	if ((u32)w->nombre >= 0x10000) { de_w(n, (const WCHAR *)w->nombre, sizeof n); a.nombre = n; }
	if ((u32)w->menu >= 0x10000) { de_w(m, (const WCHAR *)w->menu, sizeof m); a.menu = m; }
	return RegisterClassA(&a);
}
API(u16) RegisterClassExW(const WNDCLASSEXA *w)
{
	WNDCLASSEXA a = *w;
	char n[64], m[64];
	if ((u32)w->nombre >= 0x10000) { de_w(n, (const WCHAR *)w->nombre, sizeof n); a.nombre = n; }
	if ((u32)w->menu >= 0x10000) { de_w(m, (const WCHAR *)w->menu, sizeof m); a.menu = m; }
	return RegisterClassExA(&a);
}
API(BOOL) UnregisterClassA(const char *n, HINSTANCE i)
{
	Clase **p, *c = clase_de(n);
	(void)i;
	if (!c || c->propia)
		return FALSE;
	for (p = &clases; *p; p = &(*p)->sig)
		if (*p == c) {
			*p = c->sig;
			m_soltar(c);
			return TRUE;
		}
	return FALSE;
}
API(BOOL) GetClassInfoA(HINSTANCE i, const char *n, WNDCLASSA *w)
{
	Clase *c = clase_de(n);
	(void)i;
	if (!c) {
		pon_error(1411);
		return FALSE;
	}
	w->estilo = c->estilo; w->proc = c->proc; w->extra_c = c->extra_clase; w->extra = c->extra;
	w->inst = c->inst; w->ic = c->icono; w->cur = c->cursor; w->fondo = c->fondo;
	w->menu = c->menu[0] ? c->menu : (const char *)c->menu_id;
	w->nombre = c->nombre;
	return TRUE;
}
API(BOOL) GetClassInfoExA(HINSTANCE i, const char *n, WNDCLASSEXA *w)
{
	WNDCLASSA a;
	if (!GetClassInfoA(i, n, &a))
		return FALSE;
	w->estilo = a.estilo; w->proc = a.proc; w->extra_c = a.extra_c; w->extra = a.extra;
	w->inst = a.inst; w->ic = a.ic; w->cur = a.cur; w->fondo = a.fondo; w->menu = a.menu; w->nombre = a.nombre;
	w->ic_peq = clase_de(n)->icono_peq;
	return TRUE;
}
void registrar_propias(void)
{
	static const struct { const char *n; WNDPROC p; u32 e; i32 x; } t[] = {
		{ "BUTTON", proc_boton, 0x8B, 0 }, { "STATIC", proc_estatico, 0x88, 0 }, { "EDIT", proc_edicion, 0x8B, 0 },
		{ "LISTBOX", proc_lista, 0x8B, 0 }, { "COMBOBOX", proc_combo, 0x8B, 0 }, { "SCROLLBAR", proc_barra, 0x8B, 0 },
		{ "#32770", DefDlgProcA, 0x88, 30 }, { "ComboLBox", proc_lista, 0x8B, 0 },
	};
	u32 k;
	for (k = 0; k < sizeof t / sizeof t[0]; k++) {
		Clase *c = clase_nueva(t[k].n, t[k].e, t[k].p, 0, t[k].x, 0, 0, 0, k == 6 ? (HBRUSH)(COLOR_BTNFACE + 1) : 0, 0);
		if (c) {
			c->propia = 1;
			if (k == 6)
				c->atomo = 0x8002;
		}
	}
}
API(i32) GetClassNameA(HWND h, char *d, i32 n)
{
	Wnd *w = wnd(h);
	if (!w || !n)
		return 0;
	copia_n(d, w->clase->nombre, n);
	return strlen(d);
}
API(u32) GetClassLongA(HWND h, i32 i)
{
	Wnd *w = wnd(h);
	if (!w)
		return 0;
	switch (i) {
	case -10: return (u32)w->clase->fondo;      /* GCL_HBRBACKGROUND */
	case -12: return (u32)w->clase->cursor;
	case -14: return (u32)w->clase->icono;
	case -34: return (u32)w->clase->icono_peq;
	case -16: return (u32)w->clase->inst;
	case -24: return (u32)w->clase->proc;
	case -26: return w->clase->estilo;
	case -32: return w->clase->atomo;
	case -20: return w->clase->extra_clase;
	case -18: return w->clase->extra;
	}
	if (i >= 0 && i + 4 <= w->clase->extra_clase)
		return *(u32 *)(w->clase->datos_clase + i);
	return 0;
}
API(u32) SetClassLongA(HWND h, i32 i, i32 v)
{
	Wnd *w = wnd(h);
	u32 a = GetClassLongA(h, i);
	if (!w)
		return 0;
	switch (i) {
	case -10: w->clase->fondo = (HBRUSH)v; break;
	case -12: w->clase->cursor = (HCURSOR)v; break;
	case -14: w->clase->icono = (HICON)v; break;
	case -34: w->clase->icono_peq = (HICON)v; break;
	case -24: w->clase->proc = (WNDPROC)v; break;
	case -26: w->clase->estilo = v; break;
	default:
		if (i >= 0 && i + 4 <= w->clase->extra_clase)
			*(u32 *)(w->clase->datos_clase + i) = v;
	}
	return a;
}
API(u16) GetClassWord(HWND h, i32 i) { return (u16)GetClassLongA(h, i); }

/* ---------------------------------------------------------------------------
 *  Las ventanas
 * ------------------------------------------------------------------------- */
Wnd *wnd(HWND h)
{
	Wnd *w = (Wnd *)h;
	if (!w || (u32)w < 0x10000 || (u32)w >= 0x7FBF0000 || ((u32)w & 3) || w->magia != MAGIA_WND)
		return 0;
	return w;
}
API(BOOL) IsWindow(HWND h) { Wnd *w = wnd(h); return w && !w->destruida; }
Wnd *raiz_de(Wnd *w)
{
	while (w && w->padre && w->padre != escritorio)
		w = w->padre;
	return w == escritorio ? 0 : w;
}
static int es_raiz(Wnd *w) { return w && w->padre == escritorio; }
static int con_titulo(Wnd *w) { return (w->estilo & WS_CAPTION) == WS_CAPTION; }

/* lo de fuera de una ventana (como lo ve Windows) */
void nc_medidas(Wnd *w, RECT *nc)
{
	nc->left = nc->top = nc->right = nc->bottom = 0;
	if (w == escritorio)
		return;
	if (es_raiz(w)) {
		if (w->nucleo_marco) {
			nc->left = nc->right = nc->bottom = BORDE_NUCLEO;
			nc->top = TITULO_NUCLEO;
		} else if (w->estilo & (WS_BORDER | WS_DLGFRAME | WS_THICKFRAME) || (w->exestilo & WS_EX_DLGMODALFRAME)) {
			i32 b = (w->estilo & WS_THICKFRAME) || (w->estilo & WS_DLGFRAME) || (w->exestilo & WS_EX_DLGMODALFRAME) ? 3 : 1;
			nc->left = nc->right = nc->top = nc->bottom = b;
		}
		nc->top += alto_menu(w);
	} else {
		i32 b = 0;
		if (w->exestilo & WS_EX_CLIENTEDGE)
			b += 2;
		else if (w->exestilo & WS_EX_STATICEDGE)
			b += 1;
		if (w->estilo & WS_DLGFRAME)
			b += 3;
		else if (w->estilo & WS_BORDER)
			b += 1;
		if (w->estilo & WS_THICKFRAME)
			b += 2;
		nc->left = nc->right = nc->top = nc->bottom = b;
	}
	if (w->estilo & WS_VSCROLL)
		nc->right += 16;
	if (w->estilo & WS_HSCROLL)
		nc->bottom += 16;
}
void lo_de_dentro(Wnd *w, RECT *r)
{
	RECT nc;
	nc_medidas(w, &nc);
	r->left = r->top = 0;
	r->right = MAX(0, (w->r.right - w->r.left) - nc.left - nc.right);
	r->bottom = MAX(0, (w->r.bottom - w->r.top) - nc.top - nc.bottom);
}
void ventana_en_lienzo(Wnd *w, POINT *p)
{
	if (!w || w == escritorio) {
		p->x = p->y = 0;
		return;
	}
	if (es_raiz(w)) {
		if (w->nucleo_marco) {
			p->x = -BORDE_NUCLEO;
			p->y = -TITULO_NUCLEO;
		} else
			p->x = p->y = 0;
		return;
	}
	dentro_en_lienzo(w->padre, p);
	p->x += w->r.left;
	p->y += w->r.top;
}
void dentro_en_lienzo(Wnd *w, POINT *p)
{
	RECT nc;
	ventana_en_lienzo(w, p);
	nc_medidas(w, &nc);
	p->x += nc.left;
	p->y += nc.top;
}
int ventana_visible(Wnd *w)
{
	for (; w && w != escritorio; w = w->padre) {
		if (!(w->estilo & WS_VISIBLE) || w->destruida)
			return 0;
		if (es_raiz(w) && (!w->vn || w->mini))
			return 0;
	}
	return 1;
}
/* lo que se ve de ella (la ventana entera o lo de dentro), en el lienzo de su raíz */
void visible_en_lienzo(Wnd *w, RECT *r, int dentro)
{
	POINT o;
	Wnd *p;
	RECT d;
	if (dentro) {
		dentro_en_lienzo(w, &o);
		lo_de_dentro(w, &d);
	} else {
		ventana_en_lienzo(w, &o);
		d.left = d.top = 0;
		d.right = w->r.right - w->r.left;
		d.bottom = w->r.bottom - w->r.top;
	}
	r->left = o.x + d.left; r->top = o.y + d.top;
	r->right = o.x + d.right; r->bottom = o.y + d.bottom;
	for (p = w->padre; p && p != escritorio; p = p->padre) {
		RECT c;
		POINT po;
		dentro_en_lienzo(p, &po);
		lo_de_dentro(p, &c);
		r->left = MAX(r->left, po.x + c.left); r->top = MAX(r->top, po.y + c.top);
		r->right = MIN(r->right, po.x + c.right); r->bottom = MIN(r->bottom, po.y + c.bottom);
	}
	{
		Wnd *raiz = raiz_de(w);
		if (raiz) {
			r->left = MAX(r->left, 0); r->top = MAX(r->top, 0);
			r->right = MIN(r->right, raiz->lienzo.an); r->bottom = MIN(r->bottom, raiz->lienzo.al);
		}
	}
	if (r->right < r->left) r->right = r->left;
	if (r->bottom < r->top) r->bottom = r->top;
}

/* ---- el lienzo y la ventana del núcleo de una raíz ---- */
static void lienzo_medida(Wnd *w, i32 an, i32 al)
{
	Sup *s = &w->lienzo;
	u8 *b;
	i32 y;
	if (an < 1) an = 1;
	if (al < 1) al = 1;
	if (s->bits && s->an == an && s->al == al)
		return;
	b = m_pedir(an * al * 4);
	if (!b)
		return;
	{
		u32 k, c = rgb(GetSysColor(COLOR_BTNFACE));
		for (k = 0; k < (u32)(an * al); k++)
			((u32 *)b)[k] = c;
	}
	if (s->bits) {
		for (y = 0; y < MIN(al, s->al); y++)
			memcpy(b + y * an * 4, s->bits + y * s->paso, MIN(an, s->an) * 4);
		m_soltar(s->bits);
	}
	s->bits = b;
	s->an = an;
	s->al = al;
	s->paso = an * 4;
	s->bpp = 32;
	w->sucio.left = w->sucio.top = 0;
	w->sucio.right = an;
	w->sucio.bottom = al;
	w->hay_sucio = 1;
}
/* lo que mide lo de dentro de la ventana del núcleo, para la ventana entera de Windows */
static void medida_nucleo(Wnd *w, i32 *an, i32 *al)
{
	*an = w->r.right - w->r.left;
	*al = w->r.bottom - w->r.top;
	if (w->nucleo_marco) {
		*an -= 2 * BORDE_NUCLEO;
		*al -= TITULO_NUCLEO + BORDE_NUCLEO;
	}
	if (*an < 1) *an = 1;
	if (*al < 1) *al = 1;
}
static void titulo_nucleo(Wnd *w)
{
	char t[64];
	if (!w->vn)
		return;
	a437(t, w->texto ? w->texto : "", sizeof t);
	sys_titulo(w->vn, t);
}
static void crear_en_nucleo(Wnd *w, int oculta);
/* (dx.c) otra vez, con esas banderas (0xFFFFFFFF: las suyas, y que se vea) */
void crear_en_nucleo_(Wnd *w, u32 marcas)
{
	extern u32 marcas_forzadas;
	marcas_forzadas = marcas;
	crear_en_nucleo(w, 0);
	marcas_forzadas = 0;
}
u32 marcas_forzadas;
static void crear_en_nucleo(Wnd *w, int oculta)
{
	i32 an, al;
	u32 marcas = oculta ? V_OCULTA : 0;
	char t[64];
	if (w->vn)
		return;
	w->nucleo_marco = con_titulo(w);
	medida_nucleo(w, &an, &al);
	if (!w->nucleo_marco)
		marcas |= V_SIN_MARCO | V_COMPLETA;
	/* (WS_EX_TOPMOST no es V_ENCIMA: esas no pueden ser la activa, como la barra de tareas) */
	if (!(w->estilo & WS_THICKFRAME) && w->nucleo_marco)
		marcas |= V_FIJA;
	if (marcas_forzadas && marcas_forzadas != 0xFFFFFFFF)
		marcas = marcas_forzadas;
	a437(t, w->texto ? w->texto : "", sizeof t);
	w->vn = sys_ventana_nueva(t, w->r.left, w->r.top, an, al, marcas);
	if (ES_ERROR(w->vn)) {
		aviso_serie("no se puede hacer la ventana del núcleo (%x)", w->vn);
		w->vn = 0;
		return;
	}
	lienzo_medida(w, an, al);
	traza("ventana del núcleo %u para %s (%dx%d)", w->vn, w->clase->nombre, an, al);
}
/* dónde está de verdad (el núcleo la mueve: se le pregunta) */
static void donde_nucleo(Wnd *w)
{
	i32 d[6];
	if (!w->vn || ES_ERROR(sys_ventana_op(4, w->vn, LIN(d))))
		return;
	if (w->nucleo_marco) {
		w->r.left = d[0] - BORDE_NUCLEO;
		w->r.top = d[1] - TITULO_NUCLEO;
		w->r.right = d[0] + d[2] + BORDE_NUCLEO;
		w->r.bottom = d[1] + d[3] + BORDE_NUCLEO;
	} else {
		w->r.left = d[0]; w->r.top = d[1];
		w->r.right = d[0] + d[2]; w->r.bottom = d[1] + d[3];
	}
	w->mini = (d[4] & V_MINIMIZADA) != 0;
	w->maxi = (d[4] & V_MAXIMIZADA) != 0;
}

/* ---- llevar lo pintado al núcleo ---- */
void ensuciar(Wnd *raiz, const RECT *r)
{
	RECT a;
	if (!raiz || !raiz->lienzo.bits)
		return;
	if (r)
		a = *r;
	else {
		a.left = a.top = 0;
		a.right = raiz->lienzo.an;
		a.bottom = raiz->lienzo.al;
	}
	a.left = MAX(a.left, 0); a.top = MAX(a.top, 0);
	a.right = MIN(a.right, raiz->lienzo.an); a.bottom = MIN(a.bottom, raiz->lienzo.al);
	if (a.left >= a.right || a.top >= a.bottom)
		return;
	if (!raiz->hay_sucio) {
		raiz->sucio = a;
		raiz->hay_sucio = 1;
	} else {
		raiz->sucio.left = MIN(raiz->sucio.left, a.left);
		raiz->sucio.top = MIN(raiz->sucio.top, a.top);
		raiz->sucio.right = MAX(raiz->sucio.right, a.right);
		raiz->sucio.bottom = MAX(raiz->sucio.bottom, a.bottom);
	}
}
static u32 *copia_mapa;
static u32 copia_tam;
void presentar(Wnd *w)
{
	RECT r;
	i32 an, al, y;
	Sup *s = &w->lienzo;
	if (!w->hay_sucio || !w->vn || !s->bits)
		return;
	{
		static u32 n, t0;
		if (traza_si && !(++n % 100)) {
			u32 t = sys_tics();
			traza("presentar: 100 en %u ms", (t - t0) * 10);
			t0 = t;
		}
	}
	caret_ensenar(0);
	r = w->sucio;
	w->hay_sucio = 0;
	an = r.right - r.left;
	al = r.bottom - r.top;
	if (an <= 0 || al <= 0)
		return;
	/* el núcleo quiere los puntos seguidos: de 2048 en 2048 de lado como mucho */
	if (an == s->an && al <= 2048 && an <= 2048) {
		sys_mapa(w->vn, 0, r.top, an, al, (u32 *)(s->bits + r.top * s->paso));
		return;
	}
	if ((u32)(an * MIN(al, 256)) > copia_tam) {
		m_soltar(copia_mapa);
		copia_tam = an * MIN(al, 256);
		copia_mapa = m_pedir(copia_tam * 4);
		if (!copia_mapa) {
			copia_tam = 0;
			return;
		}
	}
	for (y = 0; y < al; y += 256) {
		i32 n = MIN(256, al - y), k;
		for (k = 0; k < n; k++)
			memcpy(copia_mapa + k * an, s->bits + (r.top + y + k) * s->paso + r.left * 4, an * 4);
		sys_mapa(w->vn, r.left, r.top + y, an, n, copia_mapa);
	}
}
void presentar_todo(void)
{
	Wnd *w;
	if (!escritorio)
		return;
	for (w = escritorio->hijas; w; w = w->sig)
		if (w->vn && w->hay_sucio)
			presentar(w);
}

/* ---- el árbol ---- */
static void colgar(Wnd *w, Wnd *padre, int arriba)
{
	w->padre = padre;
	w->sig = 0;
	if (!padre)
		return;
	if (arriba || !padre->hijas) {
		w->sig = padre->hijas;
		padre->hijas = w;
	} else {
		Wnd *p = padre->hijas;
		while (p->sig)
			p = p->sig;
		p->sig = w;
	}
}
static void descolgar(Wnd *w)
{
	Wnd **p;
	if (!w->padre)
		return;
	for (p = &w->padre->hijas; *p; p = &(*p)->sig)
		if (*p == w) {
			*p = w->sig;
			break;
		}
	w->sig = 0;
}

void prepara_user(void);
API(HWND) CreateWindowExA(u32 ex, const char *cn, const char *nombre, u32 estilo, i32 x, i32 y, i32 an, i32 al, HWND hp, HMENU hmenu, HINSTANCE inst, void *param)
{
	Clase *c;
	Wnd *w, *padre = wnd(hp);
	struct { void *param; HINSTANCE inst; HMENU menu; HWND padre; i32 al, an, y, x; u32 estilo; const char *nombre, *clase; u32 ex; } cs;
	prepara_user();
	c = clase_de(cn);
	if (!c) {
		traza("CreateWindowExA: no hay clase %s", (u32)cn < 0x10000 ? "(átomo)" : cn);
		pon_error(1407);
		return 0;
	}
	if ((estilo & WS_CHILD) && !padre) {
		pon_error(1406);
		return 0;
	}
	w = m_cero(sizeof *w);
	w->magia = MAGIA_WND;
	w->clase = c;
	w->proc = c->proc;
	w->estilo = estilo;
	w->exestilo = ex;
	w->inst = inst;
	w->extra = c->extra > 0 ? m_cero(c->extra) : 0;
	if (nombre && (u32)nombre >= 0x10000) {
		w->texto = m_pedir(strlen(nombre) + 1);
		strcpy(w->texto, nombre);
	}
	if (estilo & WS_CHILD) {
		w->menu = hmenu;            /* el número del control */
		colgar(w, padre, !(ex & 0x00000010) /* WS_EX_NOPARENTNOTIFY */ && 0);
		if (x == (i32)0x80000000) x = y = 0;
		if (an == (i32)0x80000000) an = al = 0;
		w->r.left = x; w->r.top = y; w->r.right = x + an; w->r.bottom = y + al;
	} else {
		/* de primer nivel: la madre, si la dice, es la dueña */
		if (!(estilo & WS_POPUP))
			w->estilo |= WS_CAPTION | WS_CLIPSIBLINGS;
		w->dueno = padre ? raiz_de(padre) : 0;
		colgar(w, escritorio, 1);
		if (an == (i32)0x80000000 || an == 0) {
			an = pantalla_an * 2 / 3;
			al = pantalla_al * 2 / 3;
			if (an == (i32)0x80000000) an = 640;
		}
		if (al == (i32)0x80000000) al = pantalla_al * 2 / 3;
		if (x == (i32)0x80000000) {
			static i32 sitio;
			sitio = (sitio + 1) % 6;
			x = 60 + sitio * 28;
			y = 36 + sitio * 26;
		}
		if (an > pantalla_an) an = pantalla_an;
		if (al > pantalla_al) al = pantalla_al;
		w->r.left = x; w->r.top = y; w->r.right = x + an; w->r.bottom = y + al;
		w->nucleo_marco = con_titulo(w);
		if (hmenu)
			w->menu = hmenu;
		else if (c->menu[0] || c->menu_id) {
			extern HMENU WINAPI LoadMenuA(HINSTANCE, const char *);
			w->menu = LoadMenuA(inst, c->menu[0] ? c->menu : (const char *)c->menu_id);
		}
		if (w->menu && menu(w->menu))
			menu(w->menu)->barra = 1;
		crear_en_nucleo(w, 1);
	}
	traza("CreateWindowExA(%s, %s, %x) = %x", c->nombre, w->texto ? w->texto : "", estilo, (u32)w);
	cs.param = param; cs.inst = inst; cs.menu = hmenu; cs.padre = hp;
	cs.al = w->r.bottom - w->r.top; cs.an = w->r.right - w->r.left; cs.y = w->r.top; cs.x = w->r.left;
	cs.estilo = w->estilo; cs.nombre = nombre; cs.clase = cn; cs.ex = ex;
	if (!enviar(w, WM_NCCREATE, 0, (LPARAM)&cs)) {
		traza("WM_NCCREATE dice que no");
		DestroyWindow((HWND)w);
		return 0;
	}
	if (enviar(w, WM_CREATE, 0, (LPARAM)&cs) == -1) {
		traza("WM_CREATE dice que no");
		DestroyWindow((HWND)w);
		return 0;
	}
	if (!wnd((HWND)w))
		return 0;
	w->creada = 1;
	{
		RECT d;
		lo_de_dentro(w, &d);
		enviar(w, WM_SIZE, 0, MAKELONG(d.right, d.bottom));
		enviar(w, WM_MOVE, 0, MAKELONG(w->r.left, w->r.top));
	}
	if ((estilo & WS_CHILD) && padre && !(ex & 0x10))
		enviar(padre, WM_PARENTNOTIFY, MAKELONG(WM_CREATE, (u16)(u32)hmenu), (LPARAM)w);
	if (w->estilo & WS_VISIBLE) {
		w->estilo &= ~WS_VISIBLE;
		ShowWindow((HWND)w, (estilo & WS_MAXIMIZE) ? 3 : 1);
	}
	return (HWND)w;
}
API(HWND) CreateWindowExW(u32 ex, const WCHAR *cn, const WCHAR *nombre, u32 estilo, i32 x, i32 y, i32 an, i32 al, HWND hp, HMENU menu, HINSTANCE inst, void *param)
{
	char c[64], n[256];
	if ((u32)cn >= 0x10000) de_w(c, cn, sizeof c);
	if (nombre) de_w(n, nombre, sizeof n);
	return CreateWindowExA(ex, (u32)cn < 0x10000 ? (const char *)cn : c, nombre ? n : 0, estilo, x, y, an, al, hp, menu, inst, param);
}
static void destruir(Wnd *w)
{
	Wnd *h;
	if (!wnd((HWND)w) || w->destruida)
		return;
	w->destruida = 1;
	if (captura == w)
		captura = 0;
	enviar(w, WM_DESTROY, 0, 0);
	while ((h = w->hijas)) {
		destruir(h);
		if (w->hijas == h)
			descolgar(h);
	}
	enviar(w, WM_NCDESTROY, 0, 0);
	if (foco == w)
		foco = 0;
	if (activa == w)
		activa = 0;
	{
		extern void quitar_temporizadores(Wnd *w);
		quitar_temporizadores(w);
	}
	if (w->vn) {
		sys_cerrar_ventana(w->vn);
		w->vn = 0;
	}
	if (w->padre && !es_raiz(w) && ventana_visible(w->padre)) {
		RECT r = w->r;
		invalidar(w->padre, &r, 1);
	}
	descolgar(w);
	m_soltar(w->lienzo.bits);
	w->lienzo.bits = 0;
	m_soltar(w->texto);
	w->texto = 0;
	w->magia = 0;
	/* (la memoria de la ficha no se suelta: un programa puede preguntar aún por ella) */
}
API(BOOL) DestroyWindow(HWND h)
{
	Wnd *w = wnd(h), *d;
	if (!w || w->destruida)
		return FALSE;
	traza("DestroyWindow(%x %s)", (u32)w, w->clase->nombre);
	/* las que son suyas (de primer nivel), antes */
	if (es_raiz(w))
		for (d = escritorio->hijas; d; ) {
			Wnd *s = d->sig;
			if (d->dueno == w && !d->destruida)
				DestroyWindow((HWND)d);
			d = s;
		}
	{
		Wnd *p = w->padre;
		u32 id = (u32)w->menu;
		int hija = !es_raiz(w);
		destruir(w);
		if (hija && wnd((HWND)p) && !(w->exestilo & 0x10))
			enviar(p, WM_PARENTNOTIFY, MAKELONG(WM_DESTROY, id), (LPARAM)w);
	}
	/* que la siguiente de las suyas pase a ser la activa */
	if (!activa) {
		Wnd *o;
		for (o = escritorio->hijas; o; o = o->sig)
			if ((o->estilo & WS_VISIBLE) && o->vn && !o->destruida) {
				sys_ventana_op(3, o->vn, 0);
				break;
			}
	}
	return TRUE;
}

/* ---- invalidar y pintar ---- */
static void une(RECT *a, const RECT *b, int *hay)
{
	if (!*hay) {
		*a = *b;
		*hay = 1;
		return;
	}
	a->left = MIN(a->left, b->left); a->top = MIN(a->top, b->top);
	a->right = MAX(a->right, b->right); a->bottom = MAX(a->bottom, b->bottom);
}
void invalidar(Wnd *w, const RECT *r, int borrar)
{
	RECT d, a;
	Wnd *h;
	if (!w || w->destruida || w == escritorio)
		return;
	lo_de_dentro(w, &d);
	if (r) {
		a = *r;
		a.left = MAX(a.left, d.left); a.top = MAX(a.top, d.top);
		a.right = MIN(a.right, d.right); a.bottom = MIN(a.bottom, d.bottom);
	} else
		a = d;
	if (a.left < a.right && a.top < a.bottom) {
		une(&w->inval, &a, &w->hay_inval);
		w->borrar |= borrar;
	}
	/* las hijas que caen ahí, también (si no se recortan: sin WS_CLIPCHILDREN) */
	if (!(w->estilo & WS_CLIPCHILDREN) || !r)
		for (h = w->hijas; h; h = h->sig)
			if ((h->estilo & WS_VISIBLE) && !h->destruida) {
				RECT hr = h->r;
				RECT x = a;
				if (x.left < hr.right && hr.left < x.right && x.top < hr.bottom && hr.top < x.bottom) {
					RECT nc, hd;
					nc_medidas(h, &nc);
					x.left -= hr.left + nc.left; x.right -= hr.left + nc.left;
					x.top -= hr.top + nc.top; x.bottom -= hr.top + nc.top;
					h->nc_sucio = 1;
					lo_de_dentro(h, &hd);
					invalidar(h, r ? &x : 0, 1);
				}
			}
}
API(BOOL) InvalidateRect(HWND h, const RECT *r, BOOL borrar)
{
	Wnd *w = wnd(h);
	if (!h) {
		Wnd *o;
		for (o = escritorio->hijas; o; o = o->sig)
			invalidar(o, 0, borrar);
		return TRUE;
	}
	if (!w)
		return FALSE;
	if (!r)
		w->nc_sucio = 1;
	invalidar(w, r, borrar);
	return TRUE;
}
API(BOOL) InvalidateRgn(HWND h, HRGN g, BOOL borrar)
{
	extern i32 WINAPI GetRgnBox(HRGN, RECT *);
	RECT r;
	if (!g)
		return InvalidateRect(h, 0, borrar);
	GetRgnBox(g, &r);
	return InvalidateRect(h, &r, borrar);
}
API(BOOL) ValidateRect(HWND h, const RECT *r)
{
	Wnd *w = wnd(h);
	if (!w)
		return FALSE;
	if (!r || (r->left <= w->inval.left && r->top <= w->inval.top && r->right >= w->inval.right && r->bottom >= w->inval.bottom))
		w->hay_inval = 0;
	return TRUE;
}
API(BOOL) ValidateRgn(HWND h, HRGN g) { (void)g; return ValidateRect(h, 0); }
API(BOOL) GetUpdateRect(HWND h, RECT *r, BOOL borrar)
{
	Wnd *w = wnd(h);
	(void)borrar;
	if (!w)
		return FALSE;
	if (r) {
		if (w->hay_inval)
			*r = w->inval;
		else
			r->left = r->top = r->right = r->bottom = 0;
	}
	return w->hay_inval;
}
API(i32) GetUpdateRgn(HWND h, HRGN g, BOOL borrar)
{
	extern BOOL WINAPI SetRectRgn(HRGN, i32, i32, i32, i32);
	RECT r;
	if (!GetUpdateRect(h, &r, borrar)) {
		SetRectRgn(g, 0, 0, 0, 0);
		return 1;
	}
	SetRectRgn(g, r.left, r.top, r.right, r.bottom);
	return 2;
}
API(BOOL) RedrawWindow(HWND h, const RECT *r, HRGN g, u32 fl)
{
	Wnd *w = h ? wnd(h) : 0;
	(void)g;
	if (h && !w)
		return FALSE;
	if (!w) {
		Wnd *o;
		for (o = escritorio->hijas; o; o = o->sig)
			RedrawWindow((HWND)o, 0, 0, fl);
		return TRUE;
	}
	if (fl & 0x0001) {               /* RDW_INVALIDATE */
		if (fl & 0x0400)             /* RDW_FRAME */
			w->nc_sucio = 1;
		invalidar(w, r, (fl & 0x0004) != 0);
	}
	if (fl & 0x0008)                 /* RDW_VALIDATE */
		w->hay_inval = 0;
	if (fl & 0x0100)                 /* RDW_UPDATENOW */
		UpdateWindow(h);
	return TRUE;
}
/* la que hay que pintar (las madres antes que las hijas, y las de debajo antes) */
static Wnd *por_pintar(Wnd *w)
{
	Wnd *lista[64], *h, *r;
	int n = 0;
	if (!(w->estilo & WS_VISIBLE) || w->destruida)
		return 0;
	if (es_raiz(w) && (!w->vn || w->mini))
		return 0;
	if (w->nc_sucio && w != escritorio) {
		w->nc_sucio = 0;
		pintar_nc(w);
	}
	if (w->hay_inval && w != escritorio)
		return w;
	for (h = w->hijas; h && n < 64; h = h->sig)
		lista[n++] = h;
	while (n--)
		if ((r = por_pintar(lista[n])))
			return r;
	return 0;
}
static Wnd *alguna_por_pintar(Wnd *filtro)
{
	Wnd *w, *r;
	for (w = escritorio->hijas; w; w = w->sig) {
		if (filtro && raiz_de(filtro) != w)
			continue;
		if ((r = por_pintar(w)))
			return r;
	}
	return 0;
}
API(BOOL) UpdateWindow(HWND h)
{
	Wnd *w = wnd(h), *p;
	int vueltas = 0;
	if (!w)
		return FALSE;
	while ((p = por_pintar(w)) && vueltas++ < 200) {
		enviar(p, WM_PAINT, 0, 0);
		if (p->hay_inval && wnd((HWND)p)) {
			/* (no ha llamado a BeginPaint: se da por pintada) */
			p->hay_inval = 0;
		}
	}
	presentar(raiz_de(w) ? raiz_de(w) : w);
	return TRUE;
}
typedef PAINTSTRUCT_ PAINTSTRUCT;
API(HDC) BeginPaint(HWND h, PAINTSTRUCT *ps)
{
	Wnd *w = wnd(h);
	HDC d;
	Dc *x;
	RECT inval;
	if (!w)
		return 0;
	caret_esconder(w);
	if (w->nc_sucio) {
		w->nc_sucio = 0;
		pintar_nc(w);
	}
	if (w->hay_inval)
		inval = w->inval;
	else {
		inval.left = inval.top = inval.right = inval.bottom = 0;
	}
	w->hay_inval = 0;
	d = dc_de_ventana(w, 1, 0);
	x = dc(d);
	x->pintando = 1;
	{
		POINT o;
		RECT s;
		dentro_en_lienzo(w, &o);
		s.left = inval.left + o.x; s.top = inval.top + o.y;
		s.right = inval.right + o.x; s.bottom = inval.bottom + o.y;
		reg_corta(&x->clip, &s);
	}
	memset(ps, 0, sizeof *ps);
	ps->hdc = d;
	ps->r = inval;
	if (w->borrar) {
		w->borrar = 0;
		ps->borrar = !enviar(w, WM_ERASEBKGND, (WPARAM)d, 0);
	}
	return d;
}
API(BOOL) EndPaint(HWND h, const PAINTSTRUCT *ps)
{
	Wnd *w = wnd(h);
	if (ps && ps->hdc) {
		Dc *x = dc(ps->hdc);
		if (x && w)
			ensuciar(raiz_de(w), &x->tope);
		dc_soltar(ps->hdc);
	}
	if (w)
		caret_ensenar(w);
	return TRUE;
}
API(HDC) GetDC(HWND h)
{
	Wnd *w = h ? wnd(h) : escritorio;
	HDC d;
	if (!w)
		return 0;
	caret_esconder(w);
	d = dc_de_ventana(w, 1, 0);
	return d;
}
API(HDC) GetDCEx(HWND h, HRGN r, u32 fl) { (void)r; (void)fl; return GetDC(h); }
API(HDC) GetWindowDC(HWND h)
{
	Wnd *w = h ? wnd(h) : escritorio;
	if (!w)
		return 0;
	return dc_de_ventana(w, 0, 0);
}
API(i32) ReleaseDC(HWND h, HDC d)
{
	Dc *x = dc(d);
	Wnd *w = wnd(h);
	if (!x)
		return 0;
	if (x->w)
		ensuciar(raiz_de(x->w), &x->tope);
	dc_soltar(d);
	if (w)
		caret_ensenar(w);
	return 1;
}
API(HWND) WindowFromDC(HDC d) { Dc *x = dc(d); return x ? (HWND)x->w : 0; }
API(BOOL) LockWindowUpdate(HWND h) { (void)h; return TRUE; }

/* lo de fuera: el menú, las barras de desplazamiento y el borde */
void pintar_nc(Wnd *w)
{
	HDC d;
	RECT nc, t;
	i32 an, al;
	if (!ventana_visible(w))
		return;
	nc_medidas(w, &nc);
	an = w->r.right - w->r.left;
	al = w->r.bottom - w->r.top;
	d = dc_de_ventana(w, 0, 1);
	if (es_raiz(w)) {
		if (!w->nucleo_marco && nc.left) {
			t.left = 0; t.top = 0; t.right = an; t.bottom = al;
			DrawEdge(d, &t, nc.left >= 3 ? 0x05 : 0x0A, 0x0F);
			if (nc.left >= 3) {
				RECT u = { 2, 2, an - 2, al - 2 };
				FrameRect(d, &u, GetSysColorBrush(COLOR_BTNFACE));
			}
		}
		if (alto_menu(w))
			menu_barra_pintar(w);
	} else {
		t.left = 0; t.top = 0; t.right = an; t.bottom = al;
		if (w->exestilo & WS_EX_CLIENTEDGE)
			DrawEdge(d, &t, 0x0A, 0x200F);
		else if (w->exestilo & WS_EX_STATICEDGE)
			DrawEdge(d, &t, 0x02, 0x200F);
		if (w->estilo & WS_DLGFRAME)
			DrawEdge(d, &t, 0x05, 0x280F);
		else if (w->estilo & WS_BORDER)
			FrameRect(d, &t, GetSysColorBrush(COLOR_WINDOWFRAME));
	}
	{
		RECT dd;
		POINT o, v;
		lo_de_dentro(w, &dd);
		dentro_en_lienzo(w, &o);
		ventana_en_lienzo(w, &v);
		if (w->estilo & WS_VSCROLL) {
			RECT b = { o.x - v.x + dd.right, o.y - v.y, o.x - v.x + dd.right + 16, o.y - v.y + dd.bottom };
			barra_pintar(d, b, &w->bv, 1, IsWindowEnabled((HWND)w));
		}
		if (w->estilo & WS_HSCROLL) {
			RECT b = { o.x - v.x, o.y - v.y + dd.bottom, o.x - v.x + dd.right, o.y - v.y + dd.bottom + 16 };
			barra_pintar(d, b, &w->bh, 0, IsWindowEnabled((HWND)w));
		}
		if ((w->estilo & WS_VSCROLL) && (w->estilo & WS_HSCROLL)) {
			RECT b = { o.x - v.x + dd.right, o.y - v.y + dd.bottom, o.x - v.x + dd.right + 16, o.y - v.y + dd.bottom + 16 };
			FillRect(d, &b, GetSysColorBrush(COLOR_BTNFACE));
		}
	}
	ensuciar(raiz_de(w), &dc(d)->tope);
	dc_soltar(d);
}

/* ---------------------------------------------------------------------------
 *  Los mensajes
 * ------------------------------------------------------------------------- */
#define COLA 512
static MSG cola[COLA];
static u32 c_ent, c_sal;
static int hay_quit;
static i32 codigo_quit;
static u32 hora_msg;
static POINT pos_msg;
LRESULT enviar(Wnd *w, u32 m, WPARAM wp, LPARAM lp)
{
	if (!w || !w->proc)
		return 0;
	return w->proc((HWND)w, m, wp, lp);
}
void poner_mensaje(Wnd *w, u32 m, WPARAM wp, LPARAM lp)
{
	MSG *x;
	if (c_ent - c_sal >= COLA) {
		traza("la cola de mensajes está llena");
		return;
	}
	x = &cola[c_ent++ % COLA];
	x->hwnd = (HWND)w;
	x->message = m;
	x->wParam = wp;
	x->lParam = lp;
	x->time = GetTickCount();
	x->pt.x = raton_x();
	x->pt.y = raton_y();
}
API(LRESULT) SendMessageA(HWND h, u32 m, WPARAM wp, LPARAM lp)
{
	Wnd *w = wnd(h);
	if (h == (HWND)0xFFFF) {             /* HWND_BROADCAST */
		Wnd *o;
		for (o = escritorio->hijas; o; o = o->sig)
			enviar(o, m, wp, lp);
		return 0;
	}
	if (!w)
		return 0;
	return enviar(w, m, wp, lp);
}
API(LRESULT) SendMessageW(HWND h, u32 m, WPARAM wp, LPARAM lp) { return SendMessageA(h, m, wp, lp); }
API(LRESULT) SendMessageTimeoutA(HWND h, u32 m, WPARAM wp, LPARAM lp, u32 f, u32 t, u32 *r) { LRESULT v = SendMessageA(h, m, wp, lp); (void)f; (void)t; if (r) *r = v; return 1; }
API(BOOL) SendNotifyMessageA(HWND h, u32 m, WPARAM wp, LPARAM lp) { SendMessageA(h, m, wp, lp); return TRUE; }
API(BOOL) PostMessageA(HWND h, u32 m, WPARAM wp, LPARAM lp)
{
	Wnd *w = wnd(h);
	if (h && !w && h != (HWND)0xFFFF)
		return FALSE;
	if (h == (HWND)0xFFFF) {
		Wnd *o;
		for (o = escritorio->hijas; o; o = o->sig)
			poner_mensaje(o, m, wp, lp);
		return TRUE;
	}
	poner_mensaje(w, m, wp, lp);
	return TRUE;
}
API(BOOL) PostMessageW(HWND h, u32 m, WPARAM wp, LPARAM lp) { return PostMessageA(h, m, wp, lp); }
API(BOOL) PostThreadMessageA(u32 id, u32 m, WPARAM wp, LPARAM lp) { (void)id; poner_mensaje(0, m, wp, lp); return TRUE; }
API(void) PostQuitMessage(i32 c) { hay_quit = 1; codigo_quit = c; traza("PostQuitMessage(%d)", c); }
API(LRESULT) CallWindowProcA(WNDPROC p, HWND h, u32 m, WPARAM wp, LPARAM lp) { return p ? p(h, m, wp, lp) : 0; }
API(LRESULT) CallWindowProcW(WNDPROC p, HWND h, u32 m, WPARAM wp, LPARAM lp) { return p ? p(h, m, wp, lp) : 0; }
API(BOOL) InSendMessage(void) { return FALSE; }
API(BOOL) ReplyMessage(LRESULT r) { (void)r; return FALSE; }
API(u32) GetMessagePos(void) { return MAKELONG(pos_msg.x, pos_msg.y); }
API(i32) GetMessageTime(void) { return hora_msg; }
API(LPARAM) GetMessageExtraInfo(void) { return 0; }
API(u32) RegisterWindowMessageA(const char *n) { return 0xC000 + (AddAtomA(n) & 0x3FFF); }
API(u32) RegisterWindowMessageW(const WCHAR *n) { char b[64]; de_w(b, n, sizeof b); return RegisterWindowMessageA(b); }

/* ---- los temporizadores ---- */
#define MAX_TEMP 32
static struct { Wnd *w; HWND h; u32 id, ms, toca; void *proc; int usado, puesto; } temps[MAX_TEMP];
void quitar_temporizadores(Wnd *w)
{
	int k;
	for (k = 0; k < MAX_TEMP; k++)
		if (temps[k].usado && temps[k].w == w)
			temps[k].usado = 0;
}
API(u32) SetTimer(HWND h, u32 id, u32 ms, void *proc)
{
	int k, libre = -1;
	if (ms < 10)
		ms = 10;
	for (k = 0; k < MAX_TEMP; k++) {
		if (temps[k].usado && temps[k].h == h && h && temps[k].id == id) {
			libre = k;
			break;
		}
		if (!temps[k].usado && libre < 0)
			libre = k;
	}
	if (libre < 0)
		return 0;
	if (!h && !id)
		id = 0x7000 + libre;
	temps[libre].usado = 1;
	temps[libre].w = wnd(h);
	temps[libre].h = h;
	temps[libre].id = id;
	temps[libre].ms = ms;
	temps[libre].toca = GetTickCount() + ms;
	temps[libre].proc = proc;
	temps[libre].puesto = 0;
	return id ? id : 1;
}
API(BOOL) KillTimer(HWND h, u32 id)
{
	int k;
	for (k = 0; k < MAX_TEMP; k++)
		if (temps[k].usado && temps[k].h == h && temps[k].id == id) {
			temps[k].usado = 0;
			return TRUE;
		}
	return FALSE;
}
/* un WM_TIMER de los que tocan (como Windows: uno cada vez, si la cola está vacía) */
static int temporizador(MSG *m, HWND filtro)
{
	int k;
	u32 ahora = GetTickCount();
	for (k = 0; k < MAX_TEMP; k++)
		if (temps[k].usado && (i32)(ahora - temps[k].toca) >= 0 && (!filtro || temps[k].h == filtro)) {
			temps[k].toca = ahora + temps[k].ms;
			m->hwnd = temps[k].h;
			m->message = WM_TIMER;
			m->wParam = temps[k].id;
			m->lParam = (LPARAM)temps[k].proc;
			m->time = ahora;
			m->pt.x = raton_x();
			m->pt.y = raton_y();
			return 1;
		}
	return 0;
}
static u32 falta_temp(void)
{
	int k;
	u32 ahora = GetTickCount(), m = 0xFFFFFFFF;
	for (k = 0; k < MAX_TEMP; k++)
		if (temps[k].usado) {
			i32 d = (i32)(temps[k].toca - ahora);
			if (d < 0)
				d = 0;
			if ((u32)d < m)
				m = d;
		}
	return m;
}

/* ---------------------------------------------------------------------------
 *  Lo que llega del núcleo
 * ------------------------------------------------------------------------- */
static u32 teclas[8], teclas_antes[8];      /* los bits de las teclas (consola.cas) */
static u8 vk_pulsada[256];                  /* el estado de las teclas de Windows (GetKeyState) */
static u8 credito[256];
static int raton_px, raton_py;
static u32 botones_ahora;
int sc_es_teclado(void) { static int v = -1; if (v < 0) v = sc(31, 2, 0, 0) == 1; return v; }
int raton_x(void) { return raton_px; }
int raton_y(void) { return raton_py; }
static void raton_pantalla(void)
{
	i32 d[3];
	if (!ES_ERROR(sys_ventana_op(6, 0, LIN(d)))) {
		raton_px = d[0];
		raton_py = d[1];
	}
}
/* el código de una tecla (juego 1; desde el 128, las de 0xE0) -> su VK */
static const u8 vk_de_codigo[128] = {
	0, 0x1B, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', 0xDB, 0xDD, 0x08, 0x09,
	'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', 0xBA, 0xBB, 0x0D, 0x11, 'A', 'S',
	'D', 'F', 'G', 'H', 'J', 'K', 'L', 0xC0, 0xDE, 0xDC, 0x10, 0xBF, 'Z', 'X', 'C', 'V',
	'B', 'N', 'M', 0xBC, 0xBE, 0xBD, 0x10, 0x6A, 0x12, 0x20, 0x14, 0x70, 0x71, 0x72, 0x73, 0x74,
	0x75, 0x76, 0x77, 0x78, 0x79, 0x90, 0x91, 0x67, 0x68, 0x69, 0x6D, 0x64, 0x65, 0x66, 0x6B, 0x61,
	0x62, 0x63, 0x60, 0x6E, 0, 0, 0xE2, 0x7A, 0x7B, 0, 0, 0, 0, 0, 0, 0,
};
static u32 vk_de(u32 k)
{
	if (k < 128)
		return vk_de_codigo[k];
	switch (k - 128) {
	case 0x1C: return 0x0D;      /* Intro del teclado numérico */
	case 0x1D: return 0x11;      /* Control derecho */
	case 0x35: return 0x6F;      /* / */
	case 0x38: return 0x12;      /* Alt Gr */
	case 0x47: return 0x24;      /* Inicio */
	case 0x48: return 0x26;      /* arriba */
	case 0x49: return 0x21;      /* Re Pág */
	case 0x4B: return 0x25;
	case 0x4D: return 0x27;
	case 0x4F: return 0x23;      /* Fin */
	case 0x50: return 0x28;
	case 0x51: return 0x22;
	case 0x52: return 0x2D;      /* Insert */
	case 0x53: return 0x2E;      /* Supr */
	case 0x5B: return 0x5B;
	case 0x5C: return 0x5C;
	case 0x5D: return 0x5D;
	}
	return 0;
}
int teclas_mayus(void) { return (vk_pulsada[0x10] & 0x80) != 0; }
int teclas_control(void) { return (vk_pulsada[0x11] & 0x80) != 0; }
int teclas_alt(void) { return (vk_pulsada[0x12] & 0x80) != 0; }
static int modal_activo(void);
static Wnd *destino_teclas(void)
{
	Wnd *r = activa;
	if (foco && !foco->destruida && raiz_de(foco) == r && IsWindowEnabled((HWND)foco))
		return foco;
	return r;
}
static void tecla_msg(u32 k, int baja, int repite)
{
	u32 vk = vk_de(k), lp, m;
	Wnd *d = destino_teclas();
	if (!vk)
		return;
	if (vk == 0x10) vk_pulsada[k == 0x36 ? 0xA1 : 0xA0] = baja ? 0x80 : 0;
	if (vk == 0x11) vk_pulsada[k >= 128 ? 0xA3 : 0xA2] = baja ? 0x80 : 0;
	if (vk == 0x12) vk_pulsada[k >= 128 ? 0xA5 : 0xA4] = baja ? 0x80 : 0;
	if (baja)
		vk_pulsada[vk] = ((vk_pulsada[vk] & 1) ^ (repite ? 0 : 1)) | 0x80;
	else
		vk_pulsada[vk] &= 1;
	lp = 1 | ((k & 0x7F) << 16) | (k >= 128 ? 1u << 24 : 0);
	if (!baja)
		lp |= 0xC0000000u;
	else if (repite)
		lp |= 0x40000000u;
	if (teclas_alt() && vk != 0x12)
		lp |= 1u << 29;
	m = baja ? WM_KEYDOWN : WM_KEYUP;
	if (vk == 0x12 || vk == 0x79 || (teclas_alt() && !teclas_control()))
		m = baja ? WM_SYSKEYDOWN : WM_SYSKEYUP;
	if (d)
		poner_mensaje(d, m, vk, lp);
}
/* la llamada 181, para los dos que la usan (los mensajes y DirectInput):
 * cada uno se lleva las teclas pulsadas y soltadas entre dos miradas suyas */
static u32 ent_user[8], ent_dx[8], ultimas[8];
static int suya_ultima;
int leer_teclas(u32 *m, int de_dx)
{
	u32 x[16], k;
	int suya = sys_teclas(x) == 1;
	for (k = 0; k < 8; k++) {
		ent_user[k] |= x[8 + k];
		ent_dx[k] |= x[8 + k];
		ultimas[k] = x[k];
	}
	suya_ultima = suya;
	for (k = 0; k < 8; k++) {
		m[k] = x[k];
		m[8 + k] = de_dx ? ent_dx[k] : ent_user[k];
		if (de_dx)
			ent_dx[k] = 0;
		else
			ent_user[k] = 0;
	}
	return suya;
}
static void mira_teclas(void)
{
	u32 m[16], k;
	int suya;
	static int era_suya;
	suya = leer_teclas(m, 0) == 1;
	if (suya && !era_suya) {
		/* (acaba de ser la activa: lo pulsado antes no es para ella) */
		memset(m + 8, 0, 32);
		for (k = 0; k < 8; k++)
			m[k] &= teclas[k] | 0x00000000;
	}
	era_suya = suya;
	if (!suya) {
		/* no es la activa: las que estaban pulsadas, se sueltan */
		for (k = 0; k < 256; k++)
			if (teclas[k >> 5] & (1u << (k & 31)))
				tecla_msg(k, 0, 0);
		memset(teclas, 0, sizeof teclas);
		return;
	}
	/* primero las que bajan (también las de pulsar y soltar entre dos
	 * miradas) y luego las que suben: Alt+F4 llega como Alt, F4 */
	for (k = 0; k < 256; k++) {
		u32 b = 1u << (k & 31), ahora = m[k >> 5] & b, antes = teclas[k >> 5] & b, entremedias = m[8 + (k >> 5)] & b;
		if ((ahora && !antes) || (!ahora && !antes && entremedias)) {
			tecla_msg(k, 1, 0);
			if (credito[k] < 4)
				credito[k]++;
		}
	}
	for (k = 0; k < 256; k++) {
		u32 b = 1u << (k & 31), ahora = m[k >> 5] & b, antes = teclas[k >> 5] & b, entremedias = m[8 + (k >> 5)] & b;
		if (!ahora && (antes || entremedias))
			tecla_msg(k, 0, 0);
	}
	memcpy(teclas, m, sizeof teclas);
	(void)teclas_antes;
}
/* la ventana (y el punto, dentro de ella) que está debajo de (x, y) del lienzo de la raíz */
static Wnd *debajo(Wnd *w, i32 x, i32 y, int *dentro)
{
	Wnd *h;
	RECT v;
	visible_en_lienzo(w, &v, 0);
	if (x < v.left || y < v.top || x >= v.right || y >= v.bottom)
		return 0;
	for (h = w->hijas; h; h = h->sig)
		if ((h->estilo & WS_VISIBLE) && !h->destruida) {
			Wnd *r = debajo(h, x, y, dentro);
			if (r) {
				/* (un cuadro de grupo deja pasar el ratón a lo de debajo) */
				if (r->clase && !stricmp_(r->clase->nombre, "BUTTON") && (r->estilo & 15) == 7)
					continue;
				return r;
			}
		}
	visible_en_lienzo(w, &v, 1);
	*dentro = x >= v.left && y >= v.top && x < v.right && y < v.bottom;
	return w;
}
static Wnd *ultima_bajo;
static u32 mk_de(u32 b)
{
	u32 m = 0;
	if (b & 1) m |= 1;
	if (b & 2) m |= 2;
	if (b & 4) m |= 16;
	if (teclas_mayus()) m |= 4;
	if (teclas_control()) m |= 8;
	return m;
}
static int bloqueada(Wnd *raiz)
{
	/* un cuadro modal encima (suyo, o de cualquiera de las nuestras) */
	return raiz && raiz->modal;
}
static void suceso_raton(Wnd *raiz, Suceso *s)
{
	i32 x = (i32)s->a, y = (i32)s->b;
	u32 b = s->c & 0xFF, veces = s->c >> 16;
	int dentro = 0;
	Wnd *w;
	POINT o;
	if (raiz->nucleo_marco) {
		raton_px = raiz->r.left + BORDE_NUCLEO + x;
		raton_py = raiz->r.top + TITULO_NUCLEO + y;
	} else {
		raton_px = raiz->r.left + x;
		raton_py = raiz->r.top + y;
	}
	if (bloqueada(raiz) || menu_abierto())
		return;
	if (captura && raiz_de(captura) == raiz) {
		w = captura;
		dentro = 1;
	} else {
		w = debajo(raiz, x, y, &dentro);
		if (!w)
			return;
		while (w && !IsWindowEnabled((HWND)w) && w != raiz)
			w = 0;
		if (!w)
			return;
	}
	dentro_en_lienzo(w, &o);
	if (!dentro && !captura) {
		/* fuera de lo de dentro: el menú, las barras */
		if (s->clase == EV_BAJA && (b & ~botones_ahora & 1)) {
			RECT d;
			lo_de_dentro(w, &d);
			if (es_raiz(w) && alto_menu(w) && y < o.y && y >= o.y - alto_menu(w)) {
				menu_barra_clic(w, x, y);
				botones_ahora = b;
				return;
			}
			if ((w->estilo & WS_VSCROLL) && x >= o.x + d.right && x < o.x + d.right + 16 && y >= o.y && y < o.y + d.bottom) {
				RECT r = { o.x + d.right, o.y, o.x + d.right + 16, o.y + d.bottom };
				barra_clic(w, &w->bv, r, 1, x, y, WM_VSCROLL, (HWND)w, 0);
			} else if ((w->estilo & WS_HSCROLL) && y >= o.y + d.bottom && y < o.y + d.bottom + 16 && x >= o.x && x < o.x + d.right) {
				RECT r = { o.x, o.y + d.bottom, o.x + d.right, o.y + d.bottom + 16 };
				barra_clic(w, &w->bh, r, 0, x, y, WM_HSCROLL, (HWND)w, 0);
			}
		}
		botones_ahora = b;
		return;
	}
	{
		LPARAM lp = MAKELONG(x - o.x, y - o.y);
		if (es_del_juego(w) && es_raiz(w)) {
			i32 mx = x, my = y;             /* (JUEGOS) en el modo del juego */
			real_a_modo(&mx, &my);
			lp = MAKELONG(mx, my);
		}
		if (s->clase == EV_MUEVE) {
			poner_mensaje(w, WM_MOUSEMOVE, mk_de(b), lp);
			ultima_bajo = w;
		} else if (s->clase == EV_BAJA) {
			u32 nuevos = b & ~botones_ahora;
			int dbl = veces >= 2 && (w->clase->estilo & 8);     /* CS_DBLCLKS */
			if (raiz != activa)
				sys_ventana_op(3, raiz->vn, 0);
			if (nuevos & 1)
				poner_mensaje(w, dbl ? WM_LBUTTONDBLCLK : WM_LBUTTONDOWN, mk_de(b), lp);
			if (nuevos & 2)
				poner_mensaje(w, dbl ? WM_RBUTTONDBLCLK : WM_RBUTTONDOWN, mk_de(b), lp);
			if (nuevos & 4)
				poner_mensaje(w, WM_MBUTTONDOWN, mk_de(b), lp);
			if (!nuevos)
				poner_mensaje(w, WM_LBUTTONDOWN, mk_de(b | 1), lp);
		} else if (s->clase == EV_SUBE) {
			u32 sueltos = botones_ahora & ~b;
			if (sueltos & 1)
				poner_mensaje(w, WM_LBUTTONUP, mk_de(b), lp);
			if (sueltos & 2) {
				poner_mensaje(w, WM_RBUTTONUP, mk_de(b), lp);
				poner_mensaje(w, WM_CONTEXTMENU, (WPARAM)w, MAKELONG(raton_px, raton_py));
			}
			if (sueltos & 4)
				poner_mensaje(w, WM_MBUTTONUP, mk_de(b), lp);
			if (!sueltos && !botones_ahora)
				poner_mensaje(w, WM_LBUTTONUP, mk_de(b), lp);
		}
	}
	botones_ahora = b;
}
static void tecla_suceso(Wnd *raiz, Suceso *s)
{
	u32 c = s->a;
	Wnd *d;
	if (bloqueada(raiz) || menu_abierto())
		return;
	d = destino_teclas();
	if (!d)
		return;
	if (c >= 0x100) {
		/* una flecha, F1...: la repetición (si no ha llegado ya como pulsada) */
		u32 k = c & 0xFF, ext = (k >= 0x47 && k <= 0x53 && !(k >= 0x3B && k <= 0x44)) ? 128 : 0;
		if (k == 0x0F)
			return;               /* (Mayús+Tab: el mapa ya lo da) */
		if (k >= 0x3B && k <= 0x44)
			ext = 0;
		if (credito[k + ext])
			credito[k + ext]--;
		else if (credito[k])
			credito[k]--;
		else
			tecla_msg(k + ext, 1, 1);
		return;
	}
	if (c == '\n')
		c = '\r';
	/* las letras: su WM_CHAR (y, si se repite sin que haya llegado como
	 * pulsada, nada más: la tecla de Windows no se sabe) */
	{
		u32 ch = c < 0x20 || c == 0x7F ? c : a1252c((u8)c);
		poner_mensaje(d, teclas_alt() && !teclas_control() ? WM_SYSCHAR : WM_CHAR, ch, 1);
	}
}
static void activar(Wnd *raiz, int si)
{
	Wnd *antes = activa;
	if (si) {
		if (activa == raiz)
			return;
		if (!antes)
			enviar(raiz, WM_ACTIVATEAPP, 1, 0);
		activa = raiz;
		enviar(raiz, WM_NCACTIVATE, 1, 0);
		enviar(raiz, WM_ACTIVATE, 1, (LPARAM)antes);
		if (wnd((HWND)raiz)) {
			Wnd *f = raiz->activa_antes && wnd((HWND)raiz->activa_antes) && raiz_de(raiz->activa_antes) == raiz ? raiz->activa_antes : 0;
			if (f)
				SetFocus((HWND)f);
			else if (!foco || raiz_de(foco) != raiz)
				SetFocus((HWND)raiz);
		}
	} else {
		if (activa != raiz)
			return;
		raiz->activa_antes = foco && raiz_de(foco) == raiz ? foco : 0;
		enviar(raiz, WM_NCACTIVATE, 0, 0);
		enviar(raiz, WM_ACTIVATE, 0, 0);
		if (foco && raiz_de(foco) == raiz) {
			Wnd *f = foco;
			foco = 0;
			enviar(f, WM_KILLFOCUS, 0, 0);
		}
		activa = 0;
		enviar(raiz, WM_ACTIVATEAPP, 0, 0);
	}
}
static void suceso(Wnd *w, Suceso *s)
{
	switch (s->clase) {
	case EV_TECLA: tecla_suceso(w, s); break;
	case EV_BAJA: case EV_SUBE: case EV_MUEVE: suceso_raton(w, s); break;
	case EV_CERRAR:
		if (!bloqueada(w))
			poner_mensaje(w, WM_SYSCOMMAND, 0xF060, 0);
		break;
	case EV_TAMANO: {
		i32 an = (i32)s->a, al = (i32)s->b;
		if (an == w->lienzo.an && al == w->lienzo.al)
			break;
		lienzo_medida(w, an, al);
		donde_nucleo(w);
		{
			RECT d;
			lo_de_dentro(w, &d);
			enviar(w, WM_SIZE, w->maxi ? 2 : 0, MAKELONG(d.right, d.bottom));
		}
		w->nc_sucio = 1;
		invalidar(w, 0, 1);
		break;
	}
	case EV_FOCO: {
		/* (los sucesos de cada ventana llegan por separado: el orden entre
		 * ellas no se sabe. Se le pregunta al núcleo cuál es la activa) */
		Wnd *o, *nueva = 0;
		(void)w;
		for (o = escritorio->hijas; o; o = o->sig)
			if (o->vn) {
				i32 d[6];
				if (!ES_ERROR(sys_ventana_op(4, o->vn, LIN(d))) && d[5] == 1) {
					nueva = o;
					break;
				}
			}
		if (nueva != activa) {
			if (activa)
				activar(activa, 0);
			if (nueva)
				activar(nueva, 1);
		}
		break;
	}
	case EV_MENU:
		menu_suceso(w, s->a, s->b);
		break;
	}
}
/* mira lo que ha llegado del núcleo y lo hace mensajes */
void bomba(int esperar)
{
	Wnd *w, *sig;
	Suceso s;
	int n = 0;
	(void)esperar;
	if (!escritorio)
		return;
	mira_teclas();
	for (w = escritorio->hijas; w; w = sig) {
		sig = w->sig;
		while (w->vn && n < 64 && sys_suceso(w->vn, &s) == 1) {
			suceso(w, &s);
			n++;
			if (!wnd((HWND)w))
				break;
		}
	}
}

/* ---------------------------------------------------------------------------
 *  GetMessage y PeekMessage
 * ------------------------------------------------------------------------- */
static int casa_msg(MSG *m, HWND h, u32 a, u32 b)
{
	if (h == (HWND)-1) {
		if (m->hwnd)
			return 0;
	} else if (h && m->hwnd != h) {
		/* (las de sus hijas también, como en Windows 9x no: solo ella) */
		Wnd *x = wnd(m->hwnd);
		int es = 0;
		while (x) {
			if ((HWND)x == h) { es = 1; break; }
			x = x->padre;
		}
		if (!es)
			return 0;
	}
	if (a || b) {
		if (m->message < a || m->message > b)
			return 0;
	}
	return 1;
}
static int saca(MSG *m, HWND h, u32 a, u32 b, int quitar)
{
	u32 k;
	for (k = c_sal; k != c_ent; k++) {
		MSG *x = &cola[k % COLA];
		if (casa_msg(x, h, a, b)) {
			*m = *x;
			if (quitar) {
				/* se cierra el hueco */
				u32 j;
				for (j = k; j != c_sal; j--)
					cola[j % COLA] = cola[(j - 1) % COLA];
				c_sal++;
			}
			hora_msg = m->time;
			pos_msg = m->pt;
			{
				/* (el WM_CHAR de una tecla que ha sido un acelerador, no) */
				extern int tragar_char;
				if (tragar_char && quitar && (m->message == WM_CHAR || m->message == WM_SYSCHAR)) {
					tragar_char = 0;
					return saca(m, h, a, b, quitar);
				}
				if (tragar_char && quitar && (m->message == WM_KEYDOWN || m->message == WM_SYSKEYDOWN))
					tragar_char = 0;
			}
			if (m->hwnd && !wnd(m->hwnd)) {
				if (!quitar)
					continue;
				return saca(m, h, a, b, quitar);
			}
			return 1;
		}
	}
	return 0;
}
static void (*tic_prog)(void);
void (*tic_dx_prog)(void);           /* (dx.c: el HispaDXTic del programa) */
void dx_tic(void)
{
	static int dentro;
	if (dentro)
		return;
	dentro = 1;
	presentar_todo();
	if (tic_prog)
		tic_prog();
	if (tic_dx_prog)
		tic_dx_prog();
	dentro = 0;
}
static int algo(MSG *m, HWND h, u32 a, u32 b, int quitar)
{
	Wnd *p;
	if (saca(m, h, a, b, quitar))
		return 1;
	if ((!a && !b) || (a <= WM_PAINT && b >= WM_PAINT)) {
		if ((p = alguna_por_pintar(h == (HWND)-1 ? 0 : wnd(h)))) {
			m->hwnd = (HWND)p;
			m->message = WM_PAINT;
			m->wParam = m->lParam = 0;
			m->time = GetTickCount();
			return 1;
		}
	}
	if (hay_quit && (!a && !b)) {
		m->hwnd = 0;
		m->message = WM_QUIT;
		m->wParam = codigo_quit;
		m->lParam = 0;
		if (quitar)
			hay_quit = 0;
		return 1;
	}
	if ((!a && !b) || (a <= WM_TIMER && b >= WM_TIMER))
		if (temporizador(m, h == (HWND)-1 ? 0 : h))
			return 1;
	return 0;
}
API(BOOL) PeekMessageA(MSG *m, HWND h, u32 a, u32 b, u32 quitar)
{
	prepara_user();
	dx_tic();
	bomba(0);
	if (algo(m, h, a, b, quitar & 1))
		return TRUE;
	presentar_todo();
	return FALSE;
}
API(BOOL) PeekMessageW(MSG *m, HWND h, u32 a, u32 b, u32 q) { return PeekMessageA(m, h, a, b, q); }
API(BOOL) GetMessageA(MSG *m, HWND h, u32 a, u32 b)
{
	prepara_user();
	for (;;) {
		dx_tic();
		bomba(0);
		if (algo(m, h, a, b, 1))
			return m->message != WM_QUIT;
		presentar_todo();
		{
			u32 f = falta_temp();
			sys_dormir(f >= 20 ? 2 : 1);
		}
	}
}
API(BOOL) GetMessageW(MSG *m, HWND h, u32 a, u32 b) { return GetMessageA(m, h, a, b); }
API(BOOL) WaitMessage(void)
{
	MSG m;
	while (!PeekMessageA(&m, 0, 0, 0, 0))
		sys_dormir(1);
	return TRUE;
}
int hay_mensajes(void)             /* (JUEGOS) MsgWaitForMultipleObjects */
{
	MSG m;
	bomba(0);
	return algo(&m, 0, 0, 0, 0);
}
API(u32) GetQueueStatus(u32 f)
{
	MSG m;
	(void)f;
	bomba(0);
	return algo(&m, 0, 0, 0, 0) ? 0x00FF00FF : 0;
}
API(BOOL) GetInputState(void) { return GetQueueStatus(0) != 0; }
API(BOOL) TranslateMessage(const MSG *m) { (void)m; return FALSE; }   /* (WM_CHAR ya llega del núcleo) */
API(LRESULT) DispatchMessageA(const MSG *m)
{
	Wnd *w = wnd(m->hwnd);
	if (m->message == WM_TIMER && m->lParam) {
		typedef void (WINAPI *TP)(HWND, u32, u32, u32);
		((TP)m->lParam)(m->hwnd, WM_TIMER, m->wParam, GetTickCount());
		return 0;
	}
	if (!w)
		return 0;
	if (m->message == WM_PAINT) {
		LRESULT r = enviar(w, WM_PAINT, 0, 0);
		if (wnd((HWND)w) && w->hay_inval)
			w->hay_inval = 0;          /* (no ha llamado a BeginPaint) */
		return r;
	}
	return enviar(w, m->message, m->wParam, m->lParam);
}
API(LRESULT) DispatchMessageW(const MSG *m) { return DispatchMessageA(m); }
/* el bucle de un cuadro modal (DialogBox, MessageBox): hasta que *fin */
void bucle_modal(Wnd *w, i32 *fin)
{
	MSG m;
	while (!*fin && wnd((HWND)w)) {
		if (!GetMessageA(&m, 0, 0, 0)) {
			PostQuitMessage(m.wParam);
			break;
		}
		if (!IsDialogMessageA((HWND)w, &m)) {
			TranslateMessage(&m);
			DispatchMessageA(&m);
		}
	}
}
static int modal_activo(void) { return 0; }

/* ---------------------------------------------------------------------------
 *  DefWindowProc
 * ------------------------------------------------------------------------- */
API(LRESULT) DefWindowProcA(HWND h, u32 m, WPARAM wp, LPARAM lp)
{
	Wnd *w = wnd(h);
	if (!w)
		return 0;
	switch (m) {
	case WM_NCCREATE:
		if (lp) {
			const char *n = ((const char **)lp)[9];
			if (n && (u32)n >= 0x10000 && !w->texto) {
				w->texto = m_pedir(strlen(n) + 1);
				strcpy(w->texto, n);
			}
		}
		return 1;
	case WM_PAINT: {
		PAINTSTRUCT ps;
		BeginPaint(h, &ps);
		EndPaint(h, &ps);
		return 0;
	}
	case WM_ERASEBKGND: {
		HBRUSH b = w->clase->fondo;
		RECT r;
		if (!b)
			return 0;
		GetClientRect(h, &r);
		FillRect((HDC)wp, &r, b);
		return 1;
	}
	case WM_CLOSE:
		DestroyWindow(h);
		return 0;
	case WM_SYSCOMMAND:
		switch (wp & 0xFFF0) {
		case 0xF060: SendMessageA(h, WM_CLOSE, 0, 0); break;      /* SC_CLOSE */
		case 0xF020: ShowWindow(h, 6); break;                     /* SC_MINIMIZE */
		case 0xF030: ShowWindow(h, 3); break;                     /* SC_MAXIMIZE */
		case 0xF120: ShowWindow(h, 9); break;                     /* SC_RESTORE */
		case 0xF100:                                              /* SC_KEYMENU */
			if (w->menu)
				menu_tecla(raiz_de(w), lp, 1);
			break;
		}
		return 0;
	case WM_SETTEXT:
		m_soltar(w->texto);
		w->texto = 0;
		if (lp) {
			w->texto = m_pedir(strlen((const char *)lp) + 1);
			strcpy(w->texto, (const char *)lp);
		}
		if (es_raiz(w))
			titulo_nucleo(w);
		return 1;
	case WM_GETTEXT:
		if (!wp)
			return 0;
		copia_n((char *)lp, w->texto ? w->texto : "", wp);
		return strlen((char *)lp);
	case WM_GETTEXTLENGTH:
		return w->texto ? strlen(w->texto) : 0;
	case WM_SYSKEYDOWN:
		if (wp == 0x73 && (lp & (1 << 29))) {  /* Alt+F4 (Alt, según el mensaje) */
			PostMessageA((HWND)raiz_de(w), WM_SYSCOMMAND, 0xF060, 0);
			return 0;
		}
		if (wp == 0x79 || (wp == 0x12 && 0)) { /* F10: el menú */
			Wnd *r = raiz_de(w);
			if (r && r->menu)
				menu_tecla(r, 0, 1);
			return 0;
		}
		return 0;
	case WM_SYSCHAR: {
		Wnd *r = raiz_de(w);
		if (r && r->menu && wp != ' ')
			menu_tecla(r, wp, 1);
		return 0;
	}
	case WM_CTLCOLORMSGBOX: case WM_CTLCOLOREDIT: case WM_CTLCOLORLISTBOX: case WM_CTLCOLORBTN:
	case WM_CTLCOLORDLG: case WM_CTLCOLORSCROLLBAR: case WM_CTLCOLORSTATIC:
		SetTextColor((HDC)wp, GetSysColor(COLOR_WINDOWTEXT));
		if (m == WM_CTLCOLOREDIT || m == WM_CTLCOLORLISTBOX) {
			SetBkColor((HDC)wp, GetSysColor(COLOR_WINDOW));
			return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
		}
		SetBkColor((HDC)wp, GetSysColor(COLOR_BTNFACE));
		return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);
	case WM_SETCURSOR:
		return 0;
	case WM_MOUSEACTIVATE:
		return 1;
	case WM_GETDLGCODE:
		return 0;
	case WM_GETFONT:
		return 0;
	case WM_NCHITTEST:
		return 1;
	case WM_GETICON:
		return (LRESULT)w->clase->icono;
	case WM_VSCROLL: case WM_HSCROLL:
		return 0;
	case WM_CONTEXTMENU:
		if (!es_raiz(w) && w->padre)
			return SendMessageA((HWND)w->padre, m, wp, lp);
		return 0;
	case WM_KEYDOWN: case WM_KEYUP: case WM_CHAR:
		return 0;
	case WM_NCDESTROY:
		return 0;
	}
	return 0;
}
API(LRESULT) DefWindowProcW(HWND h, u32 m, WPARAM wp, LPARAM lp) { return DefWindowProcA(h, m, wp, lp); }

/* ---------------------------------------------------------------------------
 *  Enseñar, mover, el tamaño
 * ------------------------------------------------------------------------- */
API(BOOL) ShowWindow(HWND h, i32 c)
{
	Wnd *w = wnd(h);
	int antes;
	if (!w)
		return FALSE;
	antes = (w->estilo & WS_VISIBLE) != 0;
	traza("ShowWindow(%x, %d)", (u32)w, c);
	if (c == 0) {
		if (!antes)
			return FALSE;
		w->estilo &= ~WS_VISIBLE;
		enviar(w, WM_SHOWWINDOW, 0, 0);
		if (es_raiz(w)) {
			if (w->vn)
				sys_ventana_op(2, w->vn, 0);
		} else if (w->padre) {
			RECT r = w->r;
			invalidar(w->padre, &r, 1);
		}
		if (foco && (foco == w || IsChild_(w, foco)))
			SetFocus((HWND)w->padre != escritorio ? (HWND)w->padre : 0);
		return antes;
	}
	if (!antes)
		enviar(w, WM_SHOWWINDOW, 1, 0);
	w->estilo |= WS_VISIBLE;
	if (es_raiz(w)) {
		if (!w->vn)
			crear_en_nucleo(w, 1);
		if (!w->vn)
			return antes;
		if (!antes) {
			sys_ventana_op(2, w->vn, 1);
			w->nc_sucio = 1;
			invalidar(w, 0, 1);
		}
		switch (c) {
		case 2: case 6: case 7:
			sys_ventana_op(5, w->vn, 3);     /* PIDE_MINIMIZAR */
			break;
		case 3:
			sys_ventana_op(5, w->vn, 4);     /* PIDE_MAXIMIZAR */
			break;
		case 9:
			sys_ventana_op(5, w->vn, 1);     /* PIDE_RESTAURAR */
			break;
		}
		if (c != 4 && c != 7 && c != 8 && c != 0) {
			sys_ventana_op(3, w->vn, 0);
			if (!activa)
				activar(w, 1);
		}
		donde_nucleo(w);
	} else {
		w->nc_sucio = 1;
		invalidar(w, 0, 1);
	}
	return antes;
}
int IsChild_(Wnd *p, Wnd *h)
{
	for (h = h ? h->padre : 0; h; h = h->padre)
		if (h == p)
			return 1;
	return 0;
}
API(BOOL) ShowWindowAsync(HWND h, i32 c) { return ShowWindow(h, c); }
API(BOOL) ShowOwnedPopups(HWND h, BOOL s) { (void)h; (void)s; return TRUE; }
API(BOOL) IsWindowVisible(HWND h) { Wnd *w = wnd(h); return w && ventana_visible(w); }
API(BOOL) IsIconic(HWND h) { Wnd *w = wnd(h); if (w && w->vn) donde_nucleo(w); return w && w->mini; }
API(BOOL) IsZoomed(HWND h) { Wnd *w = wnd(h); if (w && w->vn) donde_nucleo(w); return w && w->maxi; }
API(BOOL) OpenIcon(HWND h) { return ShowWindow(h, 9); }
API(BOOL) CloseWindow(HWND h) { return ShowWindow(h, 6); }
/* mueve y cambia de tamaño (la ventana entera, en lo de su madre o en la pantalla) */
static void colocar(Wnd *w, i32 x, i32 y, i32 an, i32 al, u32 fl)
{
	RECT antes = w->r, d;
	int movida, cambia;
	if (an < 0) an = 0;
	if (al < 0) al = 0;
	if (fl & 2) { x = w->r.left; y = w->r.top; }
	if (fl & 1) { an = w->r.right - w->r.left; al = w->r.bottom - w->r.top; }
	movida = x != w->r.left || y != w->r.top;
	cambia = an != w->r.right - w->r.left || al != w->r.bottom - w->r.top;
	w->r.left = x; w->r.top = y; w->r.right = x + an; w->r.bottom = y + al;
	if (es_raiz(w)) {
		if (w->vn && (movida || cambia)) {
			i32 dan, dal;
			struct { u32 n; i32 x, y, an, al; u32 m; } pv;
			medida_nucleo(w, &dan, &dal);
			pv.n = 0;
			pv.x = x + (w->nucleo_marco ? BORDE_NUCLEO : 0);
			pv.y = y + (w->nucleo_marco ? TITULO_NUCLEO : 0);
			pv.an = dan;
			pv.al = dal;
			pv.m = 0;
			sys_ventana_op(1, w->vn, LIN(&pv));
			if (cambia)
				lienzo_medida(w, dan, dal);
		}
		if (cambia) {
			w->nc_sucio = 1;
			invalidar(w, 0, 1);
		}
	} else if (w->padre && (movida || cambia)) {
		invalidar(w->padre, &antes, 1);
		w->nc_sucio = 1;
		invalidar(w, 0, 1);
	}
	if (movida && !(fl & 0x400))
		enviar(w, WM_MOVE, 0, MAKELONG(x, y));
	if (cambia) {
		lo_de_dentro(w, &d);
		enviar(w, WM_SIZE, 0, MAKELONG(d.right, d.bottom));
	}
	if (movida || cambia) {
		struct { HWND h, despues; i32 x, y, cx, cy; u32 fl; } wp = { (HWND)w, 0, x, y, an, al, fl };
		enviar(w, WM_WINDOWPOSCHANGED, 0, (LPARAM)&wp);
	}
}
API(BOOL) MoveWindow(HWND h, i32 x, i32 y, i32 an, i32 al, BOOL pintar)
{
	Wnd *w = wnd(h);
	(void)pintar;
	if (!w)
		return FALSE;
	colocar(w, x, y, an, al, 0);
	return TRUE;
}
API(BOOL) SetWindowPos(HWND h, HWND tras, i32 x, i32 y, i32 an, i32 al, u32 fl)
{
	Wnd *w = wnd(h);
	if (!w)
		return FALSE;
	if (es_raiz(w) && w->vn)
		donde_nucleo(w);
	if (!(fl & 3) || (fl & 3) != 3)
		colocar(w, x, y, an, al, fl);
	if (!(fl & 4)) {                         /* el orden */
		if (!es_raiz(w)) {
			if (tras == 0 || tras == (HWND)-1) {       /* HWND_TOP, HWND_TOPMOST */
				descolgar(w);
				colgar(w, w->padre ? w->padre : 0, 1);
			} else if (tras == (HWND)1) {              /* HWND_BOTTOM */
				Wnd *p = w->padre;
				descolgar(w);
				colgar(w, p, 0);
			}
			invalidar(w, 0, 1);
		} else if (w->vn && !(fl & 0x10) && tras != (HWND)1)
			sys_ventana_op(3, w->vn, 0);
	}
	if (fl & 0x40)
		ShowWindow(h, (fl & 0x10) ? 8 : 5);
	if (fl & 0x80)
		ShowWindow(h, 0);
	if (fl & 0x20) {                         /* SWP_FRAMECHANGED */
		w->nc_sucio = 1;
		invalidar(w, 0, 1);
	}
	return TRUE;
}
API(HANDLE) BeginDeferWindowPos(i32 n) { (void)n; return (HANDLE)1; }
API(HANDLE) DeferWindowPos(HANDLE d, HWND h, HWND t, i32 x, i32 y, i32 an, i32 al, u32 fl) { SetWindowPos(h, t, x, y, an, al, fl); return d; }
API(BOOL) EndDeferWindowPos(HANDLE d) { (void)d; return TRUE; }
API(BOOL) BringWindowToTop(HWND h) { return SetWindowPos(h, 0, 0, 0, 0, 0, 3); }
API(BOOL) GetWindowRect(HWND h, RECT *r)
{
	Wnd *w = wnd(h);
	if (!w)
		return FALSE;
	if (w == escritorio) {
		r->left = r->top = 0;
		r->right = pantalla_an;
		r->bottom = pantalla_al;
		return TRUE;
	}
	if (es_raiz(w) && es_del_juego(w)) {
		r->left = r->top = 0;                  /* (JUEGOS) toda la pantalla del modo */
		modo_medida(&r->right, &r->bottom);
		return TRUE;
	}
	if (es_raiz(w)) {
		donde_nucleo(w);
		*r = w->r;
		return TRUE;
	}
	{
		POINT p = { w->r.left, w->r.top };
		ClientToScreen((HWND)w->padre, &p);
		r->left = p.x; r->top = p.y;
		r->right = p.x + w->r.right - w->r.left;
		r->bottom = p.y + w->r.bottom - w->r.top;
	}
	return TRUE;
}
API(BOOL) GetClientRect(HWND h, RECT *r)
{
	Wnd *w = wnd(h);
	if (!w)
		return FALSE;
	if (w == escritorio) {
		r->left = r->top = 0;
		r->right = pantalla_an;
		r->bottom = pantalla_al;
		return TRUE;
	}
	if (es_raiz(w) && es_del_juego(w)) {
		r->left = r->top = 0;                  /* (JUEGOS) */
		modo_medida(&r->right, &r->bottom);
		return TRUE;
	}
	lo_de_dentro(w, r);
	return TRUE;
}
/* dónde está lo de dentro de w en la pantalla */
static void dentro_en_pantalla(Wnd *w, POINT *p)
{
	Wnd *r = raiz_de(w);
	POINT o;
	p->x = p->y = 0;
	if (!r)
		return;
	donde_nucleo(r);
	dentro_en_lienzo(w, &o);
	p->x = r->r.left + (r->nucleo_marco ? BORDE_NUCLEO : 0) + o.x;
	p->y = r->r.top + (r->nucleo_marco ? TITULO_NUCLEO : 0) + o.y;
}
API(BOOL) ClientToScreen(HWND h, POINT *p)
{
	Wnd *w = wnd(h);
	POINT o;
	if (!w || w == escritorio)
		return w != 0;
	if (es_raiz(w) && es_del_juego(w))
		return TRUE;                            /* (JUEGOS) empieza en 0, 0 del modo */
	dentro_en_pantalla(w, &o);
	p->x += o.x;
	p->y += o.y;
	return TRUE;
}
API(BOOL) ScreenToClient(HWND h, POINT *p)
{
	Wnd *w = wnd(h);
	POINT o;
	if (!w || w == escritorio)
		return w != 0;
	if (es_raiz(w) && es_del_juego(w))
		return TRUE;                            /* (JUEGOS) */
	dentro_en_pantalla(w, &o);
	p->x -= o.x;
	p->y -= o.y;
	return TRUE;
}
API(i32) MapWindowPoints(HWND a, HWND b, POINT *p, u32 n)
{
	u32 k;
	POINT oa = { 0, 0 }, ob = { 0, 0 };
	if (a && wnd(a) != escritorio)
		ClientToScreen(a, &oa);
	if (b && wnd(b) != escritorio)
		ClientToScreen(b, &ob);
	for (k = 0; k < n; k++) {
		p[k].x += oa.x - ob.x;
		p[k].y += oa.y - ob.y;
	}
	return MAKELONG(oa.x - ob.x, oa.y - ob.y);
}
API(BOOL) AdjustWindowRectEx(RECT *r, u32 estilo, BOOL menu, u32 ex)
{
	if ((estilo & WS_CAPTION) == WS_CAPTION || !(estilo & (WS_POPUP | WS_CHILD))) {
		r->left -= BORDE_NUCLEO; r->right += BORDE_NUCLEO;
		r->top -= TITULO_NUCLEO; r->bottom += BORDE_NUCLEO;
	} else if (estilo & (WS_BORDER | WS_DLGFRAME | WS_THICKFRAME) || (ex & WS_EX_DLGMODALFRAME)) {
		i32 b = (estilo & (WS_DLGFRAME | WS_THICKFRAME)) || (ex & WS_EX_DLGMODALFRAME) ? 3 : 1;
		r->left -= b; r->right += b; r->top -= b; r->bottom += b;
	}
	if (ex & WS_EX_CLIENTEDGE) { r->left -= 2; r->right += 2; r->top -= 2; r->bottom += 2; }
	if (menu)
		r->top -= 20;
	return TRUE;
}
API(BOOL) AdjustWindowRect(RECT *r, u32 estilo, BOOL menu) { return AdjustWindowRectEx(r, estilo, menu, 0); }
typedef struct { u32 tam, fl, show; POINT min, max; RECT normal; } WINDOWPLACEMENT;
API(BOOL) GetWindowPlacement(HWND h, WINDOWPLACEMENT *p)
{
	Wnd *w = wnd(h);
	if (!w)
		return FALSE;
	GetWindowRect(h, &p->normal);
	p->fl = 0;
	p->show = w->mini ? 2 : w->maxi ? 3 : 1;
	p->min.x = p->min.y = p->max.x = p->max.y = -1;
	return TRUE;
}
API(BOOL) SetWindowPlacement(HWND h, const WINDOWPLACEMENT *p)
{
	Wnd *w = wnd(h);
	if (!w)
		return FALSE;
	colocar(w, p->normal.left, p->normal.top, p->normal.right - p->normal.left, p->normal.bottom - p->normal.top, 0);
	if (w->estilo & WS_VISIBLE || p->show)
		ShowWindow(h, p->show);
	return TRUE;
}

/* ---------------------------------------------------------------------------
 *  Lo de cada ventana: texto, largos, el árbol
 * ------------------------------------------------------------------------- */
API(i32) GetWindowTextA(HWND h, char *d, i32 n)
{
	Wnd *w = wnd(h);
	if (!w || n <= 0)
		return 0;
	d[0] = 0;
	return SendMessageA(h, WM_GETTEXT, n, (LPARAM)d);
}
API(i32) GetWindowTextW(HWND h, WCHAR *d, i32 n)
{
	char b[1024];
	i32 r = GetWindowTextA(h, b, sizeof b);
	if (n <= 0)
		return 0;
	a_w(d, b, n);
	return MIN(r, n - 1);
}
API(i32) GetWindowTextLengthA(HWND h) { return wnd(h) ? SendMessageA(h, WM_GETTEXTLENGTH, 0, 0) : 0; }
API(BOOL) SetWindowTextA(HWND h, const char *t) { return wnd(h) ? SendMessageA(h, WM_SETTEXT, 0, (LPARAM)t) != 0 : FALSE; }
API(BOOL) SetWindowTextW(HWND h, const WCHAR *t) { char b[1024]; de_w(b, t, sizeof b); return SetWindowTextA(h, b); }
API(i32) InternalGetWindowText(HWND h, WCHAR *d, i32 n) { return GetWindowTextW(h, d, n); }
static i32 *dlg_largos(Wnd *w, i32 i)
{
	/* DWL_MSGRESULT 0, DWL_DLGPROC 4, DWL_USER 8 */
	if (i == 0) return &w->dlg_res;
	if (i == 4) return (i32 *)&w->dlgproc;
	if (i == 8) return &w->dlg_usuario;
	return 0;
}
API(i32) GetWindowLongA(HWND h, i32 i)
{
	Wnd *w = wnd(h);
	if (!w)
		return 0;
	switch (i) {
	case -4: return (i32)w->proc;
	case -6: return (i32)w->inst;
	case -8: return (i32)(w->padre != escritorio ? (HWND)w->padre : (HWND)w->dueno);
	case -12: return (i32)w->menu;
	case -16: return w->estilo;
	case -20: return w->exestilo;
	case -21: return w->usuario;
	}
	if (w->es_dialogo && dlg_largos(w, i))
		return *dlg_largos(w, i);
	if (i >= 0 && i + 4 <= w->clase->extra)
		return *(i32 *)(w->extra + i);
	pon_error(1413);
	return 0;
}
API(i32) SetWindowLongA(HWND h, i32 i, i32 v)
{
	Wnd *w = wnd(h);
	i32 a;
	if (!w)
		return 0;
	a = GetWindowLongA(h, i);
	switch (i) {
	case -4: w->proc = (WNDPROC)v; return a;
	case -6: w->inst = (HINSTANCE)v; return a;
	case -8: if (!(w->estilo & WS_CHILD)) w->dueno = wnd((HWND)v); return a;
	case -12: w->menu = (HMENU)v; return a;
	case -16: {
		u32 antes = w->estilo;
		w->estilo = (u32)v;
		if ((antes ^ v) & (WS_VSCROLL | WS_HSCROLL | WS_BORDER | WS_DLGFRAME)) {
			w->nc_sucio = 1;
			invalidar(w, 0, 1);
		}
		if ((antes ^ v) & WS_DISABLED)
			invalidar(w, 0, 1);
		return a;
	}
	case -20: w->exestilo = v; return a;
	case -21: w->usuario = v; return a;
	}
	if (w->es_dialogo && dlg_largos(w, i)) {
		*dlg_largos(w, i) = v;
		return a;
	}
	if (i >= 0 && i + 4 <= w->clase->extra)
		*(i32 *)(w->extra + i) = v;
	return a;
}
API(i32) GetWindowLongW(HWND h, i32 i) { return GetWindowLongA(h, i); }
API(i32) SetWindowLongW(HWND h, i32 i, i32 v) { return SetWindowLongA(h, i, v); }
API(u16) GetWindowWord(HWND h, i32 i) { return (u16)GetWindowLongA(h, i); }
API(u16) SetWindowWord(HWND h, i32 i, u16 v) { return (u16)SetWindowLongA(h, i, v); }
API(HWND) GetParent(HWND h)
{
	Wnd *w = wnd(h);
	if (!w)
		return 0;
	if (w->estilo & WS_CHILD)
		return (HWND)w->padre;
	return (HWND)w->dueno;
}
API(HWND) GetAncestor(HWND h, u32 f)
{
	Wnd *w = wnd(h);
	if (!w)
		return 0;
	if (f == 1)
		return w->padre == escritorio ? (HWND)escritorio : (HWND)w->padre;
	return (HWND)raiz_de(w);
}
API(HWND) SetParent(HWND h, HWND p)
{
	Wnd *w = wnd(h), *np = p ? wnd(p) : escritorio, *antes;
	if (!w || !np || !(w->estilo & WS_CHILD))
		return 0;
	antes = w->padre;
	descolgar(w);
	colgar(w, np, 1);
	invalidar(np, 0, 1);
	if (antes)
		invalidar(antes, 0, 1);
	return (HWND)antes;
}
API(BOOL) IsChild(HWND p, HWND h) { return wnd(p) && wnd(h) && IsChild_(wnd(p), wnd(h)); }
API(HWND) GetDesktopWindow(void) { prepara_user(); return (HWND)escritorio; }
API(HWND) GetShellWindow(void) { return (HWND)escritorio; }
API(HWND) GetTopWindow(HWND h) { Wnd *w = h ? wnd(h) : escritorio; return w ? (HWND)w->hijas : 0; }
API(HWND) GetWindow(HWND h, u32 c)
{
	Wnd *w = wnd(h), *p;
	if (!w)
		return 0;
	switch (c) {
	case 0:                                  /* GW_HWNDFIRST */
		return w->padre ? (HWND)w->padre->hijas : 0;
	case 1:                                  /* GW_HWNDLAST */
		for (p = w->padre ? w->padre->hijas : 0; p && p->sig; p = p->sig)
			;
		return (HWND)p;
	case 2: return (HWND)w->sig;             /* GW_HWNDNEXT */
	case 3:                                  /* GW_HWNDPREV */
		for (p = w->padre ? w->padre->hijas : 0; p && p->sig != w; p = p->sig)
			;
		return (HWND)p;
	case 4: return (HWND)w->dueno;           /* GW_OWNER */
	case 5: return (HWND)w->hijas;           /* GW_CHILD */
	}
	return 0;
}
API(HWND) GetNextWindow(HWND h, u32 c) { return GetWindow(h, c); }
typedef BOOL (WINAPI *ENUMP)(HWND, LPARAM);
API(BOOL) EnumWindows(ENUMP f, LPARAM lp)
{
	Wnd *w, *sig;
	for (w = escritorio->hijas; w; w = sig) {
		sig = w->sig;
		if (!f((HWND)w, lp))
			return FALSE;
	}
	return TRUE;
}
static int enum_hijas(Wnd *p, ENUMP f, LPARAM lp)
{
	Wnd *w, *sig;
	for (w = p->hijas; w; w = sig) {
		sig = w->sig;
		if (!f((HWND)w, lp))
			return 0;
		if (wnd((HWND)w) && !enum_hijas(w, f, lp))
			return 0;
	}
	return 1;
}
API(BOOL) EnumChildWindows(HWND h, ENUMP f, LPARAM lp) { Wnd *w = h ? wnd(h) : escritorio; return w ? enum_hijas(w, f, lp) : FALSE; }
API(BOOL) EnumThreadWindows(u32 t, ENUMP f, LPARAM lp) { (void)t; return EnumWindows(f, lp); }
API(HWND) FindWindowA(const char *c, const char *n)
{
	Wnd *w;
	for (w = escritorio->hijas; w; w = w->sig) {
		if (c && (u32)c >= 0x10000 && stricmp_(w->clase->nombre, c))
			continue;
		if (c && (u32)c < 0x10000 && w->clase->atomo != (u32)c)
			continue;
		if (n && (!w->texto || strcmp(w->texto, n)))
			continue;
		return (HWND)w;
	}
	return 0;
}
API(HWND) FindWindowExA(HWND p, HWND tras, const char *c, const char *n)
{
	Wnd *w, *pp = p ? wnd(p) : escritorio;
	if (!pp)
		return 0;
	for (w = tras ? wnd(tras)->sig : pp->hijas; w; w = w->sig) {
		if (c && (u32)c >= 0x10000 && stricmp_(w->clase->nombre, c))
			continue;
		if (n && (!w->texto || strcmp(w->texto, n)))
			continue;
		return (HWND)w;
	}
	return 0;
}
API(u32) GetWindowThreadProcessId(HWND h, u32 *p) { (void)h; if (p) *p = sys_tarea(); return sys_tarea(); }
API(HWND) ChildWindowFromPointEx(HWND h, POINT p, u32 f)
{
	Wnd *w = wnd(h), *c;
	(void)f;
	if (!w)
		return 0;
	for (c = w->hijas; c; c = c->sig)
		if ((c->estilo & WS_VISIBLE) && p.x >= c->r.left && p.x < c->r.right && p.y >= c->r.top && p.y < c->r.bottom)
			return (HWND)c;
	return h;
}
API(HWND) ChildWindowFromPoint(HWND h, POINT p) { return ChildWindowFromPointEx(h, p, 0); }
API(HWND) WindowFromPoint(POINT p)
{
	Wnd *w;
	for (w = escritorio->hijas; w; w = w->sig)
		if ((w->estilo & WS_VISIBLE) && w->vn) {
			int dentro;
			Wnd *x;
			donde_nucleo(w);
			if (p.x < w->r.left || p.y < w->r.top || p.x >= w->r.right || p.y >= w->r.bottom)
				continue;
			x = debajo(w, p.x - w->r.left - (w->nucleo_marco ? BORDE_NUCLEO : 0) + (w->nucleo_marco ? -BORDE_NUCLEO + BORDE_NUCLEO : 0),
				p.y - w->r.top - (w->nucleo_marco ? TITULO_NUCLEO : 0), &dentro);
			return (HWND)(x ? x : w);
		}
	return (HWND)escritorio;
}
API(BOOL) SetPropA(HWND h, const char *n, HANDLE v)
{
	Wnd *w = wnd(h);
	Prop *p;
	if (!w)
		return FALSE;
	for (p = w->props; p; p = p->sig)
		if ((u32)n < 0x10000 ? p->atomo == (u32)n : !stricmp_(p->nombre, n)) {
			p->v = v;
			return TRUE;
		}
	p = m_cero(sizeof *p);
	if ((u32)n < 0x10000)
		p->atomo = (u16)(u32)n;
	else
		copia_n(p->nombre, n, sizeof p->nombre);
	p->v = v;
	p->sig = w->props;
	w->props = p;
	return TRUE;
}
API(HANDLE) GetPropA(HWND h, const char *n)
{
	Wnd *w = wnd(h);
	Prop *p;
	if (!w)
		return 0;
	for (p = w->props; p; p = p->sig)
		if ((u32)n < 0x10000 ? p->atomo == (u32)n : !stricmp_(p->nombre, n))
			return p->v;
	return 0;
}
API(HANDLE) RemovePropA(HWND h, const char *n)
{
	Wnd *w = wnd(h);
	Prop **p;
	if (!w)
		return 0;
	for (p = &w->props; *p; p = &(*p)->sig)
		if ((u32)n < 0x10000 ? (*p)->atomo == (u32)n : !stricmp_((*p)->nombre, n)) {
			Prop *x = *p;
			HANDLE v = x->v;
			*p = x->sig;
			m_soltar(x);
			return v;
		}
	return 0;
}

/* ---------------------------------------------------------------------------
 *  El foco, la activa, la captura, habilitar
 * ------------------------------------------------------------------------- */
API(HWND) SetFocus(HWND h)
{
	Wnd *w = wnd(h), *antes = foco;
	if (h && !w)
		return 0;
	if (w == foco)
		return (HWND)antes;
	if (w && !IsWindowEnabled(h))
		return 0;
	if (w && raiz_de(w) && raiz_de(w) != activa && raiz_de(w)->vn) {
		sys_ventana_op(3, raiz_de(w)->vn, 0);
		activar(raiz_de(w), 1);
		if (foco == w)
			return (HWND)antes;
	}
	foco = w;
	if (antes && wnd((HWND)antes))
		enviar(antes, WM_KILLFOCUS, (WPARAM)w, 0);
	if (w && foco == w)
		enviar(w, WM_SETFOCUS, (WPARAM)antes, 0);
	return (HWND)antes;
}
API(HWND) GetFocus(void) { return (HWND)foco; }
API(HWND) GetActiveWindow(void) { return (HWND)activa; }
API(HWND) SetActiveWindow(HWND h)
{
	Wnd *w = wnd(h), *a = activa;
	if (w && raiz_de(w) && raiz_de(w)->vn) {
		sys_ventana_op(3, raiz_de(w)->vn, 0);
		activar(raiz_de(w), 1);
	}
	return (HWND)a;
}
API(HWND) GetForegroundWindow(void) { return (HWND)activa; }
API(BOOL) SetForegroundWindow(HWND h) { SetActiveWindow(h); return TRUE; }
API(HWND) SetCapture(HWND h)
{
	Wnd *a = captura;
	captura = wnd(h);
	return (HWND)a;
}
API(BOOL) ReleaseCapture(void)
{
	Wnd *a = captura;
	captura = 0;
	if (a && wnd((HWND)a))
		enviar(a, WM_CAPTURECHANGED, 0, 0);
	return TRUE;
}
API(HWND) GetCapture(void) { return (HWND)captura; }
API(BOOL) EnableWindow(HWND h, BOOL e)
{
	Wnd *w = wnd(h);
	int antes;
	if (!w)
		return FALSE;
	antes = !(w->estilo & WS_DISABLED);
	if (e)
		w->estilo &= ~WS_DISABLED;
	else {
		w->estilo |= WS_DISABLED;
		if (foco == w)
			SetFocus(0);
	}
	if (antes != !!e) {
		enviar(w, WM_ENABLE, e != 0, 0);
		invalidar(w, 0, 1);
	}
	return !antes;
}
API(BOOL) IsWindowEnabled(HWND h) { Wnd *w = wnd(h); return w && !(w->estilo & WS_DISABLED); }
API(BOOL) IsWindowUnicode(HWND h) { (void)h; return FALSE; }

/* ---------------------------------------------------------------------------
 *  El teclado
 * ------------------------------------------------------------------------- */
API(i16) GetKeyState(i32 vk)
{
	if (vk == 1) return botones_ahora & 1 ? -128 : 0;
	if (vk == 2) return botones_ahora & 2 ? -128 : 0;
	if (vk == 4) return botones_ahora & 4 ? -128 : 0;
	if (vk < 0 || vk > 255)
		return 0;
	return (vk_pulsada[vk] & 0x80 ? -128 : 0) | (vk_pulsada[vk] & 1);
}
API(i16) GetAsyncKeyState(i32 vk)
{
	mira_teclas();
	if (vk == 1 || vk == 2 || vk == 4) {
		i32 d[3];
		sys_ventana_op(6, 0, LIN(d));
		return (u32)d[2] & (vk == 4 ? 4 : vk) ? (i16)0x8000 : 0;
	}
	if (vk < 0 || vk > 255)
		return 0;
	return vk_pulsada[vk] & 0x80 ? (i16)0x8000 : 0;
}
API(BOOL) GetKeyboardState(u8 *e)
{
	memcpy(e, vk_pulsada, 256);
	e[1] = botones_ahora & 1 ? 0x80 : 0;
	e[2] = botones_ahora & 2 ? 0x80 : 0;
	return TRUE;
}
API(BOOL) SetKeyboardState(const u8 *e) { memcpy(vk_pulsada, e, 256); return TRUE; }
API(u32) MapVirtualKeyA(u32 c, u32 t)
{
	u32 k;
	if (t == 0) {                         /* VK -> código */
		for (k = 1; k < 256; k++)
			if (vk_de(k) == c)
				return k & 0x7F;
		return 0;
	}
	if (t == 1 || t == 3)                 /* código -> VK */
		return vk_de(c & 0xFF);
	if (t == 2) {                         /* VK -> letra */
		if (c >= 'A' && c <= 'Z') return c;
		if (c >= '0' && c <= '9') return c;
		if (c == 0x20 || c == 0x0D || c == 0x08 || c == 0x09 || c == 0x1B) return c;
		return 0;
	}
	return 0;
}
API(u32) MapVirtualKeyExA(u32 c, u32 t, HANDLE l) { (void)l; return MapVirtualKeyA(c, t); }
API(i16) VkKeyScanA(char c)
{
	u8 u = (u8)c;
	if (u >= 'a' && u <= 'z') return mayus(u);
	if (u >= 'A' && u <= 'Z') return 0x100 | u;
	if (u >= '0' && u <= '9') return u;
	if (u == ' ' || u == '\r' || u == 8 || u == 9 || u == 27) return u;
	return -1;
}
API(i16) VkKeyScanExA(char c, HANDLE l) { (void)l; return VkKeyScanA(c); }
/* las letras de cada tecla (por su código), las del teclado del núcleo
 * (consola.cas): el español y el de EE. UU., en la página del núcleo */
static const char es_normal[59] = "\0\0331234567890'\xAD\x08\tqwertyuiop`+\r\0asdfghjkl\xA4'\xA7\0\x87zxcvbnm,.-\0*\0 ";
static const char es_mayus[59] = "\0\033!\"\xFA$%&/()=?\xA8\x08\tQWERTYUIOP^*\r\0ASDFGHJKL\xA5\"\xA6\0\x80ZXCVBNM;:_\0*\0 ";
static const char es_altgr[59] = "\0\0|@#~\0\xAA\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0[]\0\0\0\0\0\0\0\0\0\0\0\0{\\\0}";
static const char us_normal[59] = "\0\0331234567890-=\x08\tqwertyuiop[]\r\0asdfghjkl;'`\0\\zxcvbnm,./\0*\0 ";
static const char us_mayus[59] = "\0\033!@#$%^&*()_+\x08\tQWERTYUIOP{}\r\0ASDFGHJKL:\"~\0|ZXCVBNM<>?\0*\0 ";
API(i32) ToAscii(u32 vk, u32 sc, const u8 *e, u16 *d, u32 f)
{
	u32 k = sc & 0x7F, es = sc_es_teclado();
	int may = e && (e[0x10] & 0x80), ctl = e && (e[0x11] & 0x80), alt = e && (e[0x12] & 0x80), bloq = e && (e[0x14] & 1);
	u8 c = 0;
	(void)f;
	if (!k)
		k = MapVirtualKeyA(vk, 0);
	if (vk >= 0x60 && vk <= 0x69) { *d = '0' + vk - 0x60; return 1; }   /* el teclado numérico */
	if (vk == 0x6A) { *d = '*'; return 1; }
	if (vk == 0x6B) { *d = '+'; return 1; }
	if (vk == 0x6D) { *d = '-'; return 1; }
	if (vk == 0x6E) { *d = '.'; return 1; }
	if (vk == 0x6F) { *d = '/'; return 1; }
	if (k >= 59 || (sc & 0x100))
		return 0;
	if (ctl && alt && es)
		c = es_altgr[k];
	else if (ctl) {
		c = (es ? es_normal : us_normal)[k];
		if (c >= 'a' && c <= 'z') c -= 0x60;
		else if (c != 8 && c != 13 && c != 27 && c != 9) c = 0;
	} else {
		int m = may;
		c = (es ? es_normal : us_normal)[k];
		if (bloq && ((c >= 'a' && c <= 'z') || c == (char)0xA4 || c == (char)0x87))
			m = !m;
		if (m)
			c = (es ? es_mayus : us_mayus)[k];
	}
	if (!c)
		return 0;
	*d = a1252c(c);
	return 1;
}
API(i32) ToAsciiEx(u32 vk, u32 sc, const u8 *e, u16 *d, u32 f, HANDLE l) { (void)l; return ToAscii(vk, sc, e, d, f); }
API(i32) GetKeyNameTextA(i32 lp, char *d, i32 n)
{
	u32 sc = (lp >> 16) & 0xFF, vk = vk_de(sc | ((lp >> 24) & 1 ? 128 : 0));
	char t[16];
	if (vk >= 'A' && vk <= 'Z') fmt(t, sizeof t, "%c", vk);
	else if (vk >= '0' && vk <= '9') fmt(t, sizeof t, "%c", vk);
	else if (vk >= 0x70 && vk <= 0x7B) fmt(t, sizeof t, "F%u", vk - 0x6F);
	else switch (vk) {
		case 0x0D: strcpy(t, "Intro"); break;
		case 0x1B: strcpy(t, "Esc"); break;
		case 0x20: strcpy(t, "Espacio"); break;
		case 0x10: strcpy(t, "Mayús"); break;
		case 0x11: strcpy(t, "Ctrl"); break;
		case 0x12: strcpy(t, "Alt"); break;
		case 0x26: strcpy(t, "Arriba"); break;
		case 0x28: strcpy(t, "Abajo"); break;
		case 0x25: strcpy(t, "Izquierda"); break;
		case 0x27: strcpy(t, "Derecha"); break;
		default: fmt(t, sizeof t, "Tecla %u", sc);
	}
	copia_n(d, t, n);
	return strlen(d);
}
API(HANDLE) GetKeyboardLayout(u32 t) { (void)t; return (HANDLE)0x0C0A0C0A; }
API(i32) GetKeyboardType(i32 t) { return t == 0 ? 4 : t == 2 ? 12 : 0; }
API(HANDLE) LoadKeyboardLayoutA(const char *n, u32 f) { (void)n; (void)f; return (HANDLE)0x0C0A0C0A; }
API(BOOL) GetKeyboardLayoutNameA(char *d) { strcpy(d, "0000040A"); return TRUE; }
API(void) keybd_event(u8 vk, u8 sc, u32 f, u32 x) { (void)vk; (void)sc; (void)f; (void)x; }
API(void) mouse_event(u32 f, u32 x, u32 y, u32 d, u32 e) { (void)f; (void)x; (void)y; (void)d; (void)e; }

/* ---------------------------------------------------------------------------
 *  El cursor de texto (caret): se pinta invirtiendo los puntos
 * ------------------------------------------------------------------------- */
static struct { Wnd *w; i32 x, y, an, al; int escondido, visible, pintado; u32 ultimo; HBITMAP mapa; } caret;
static void caret_invertir(void)
{
	HDC d;
	RECT r;
	if (!caret.w || !wnd((HWND)caret.w))
		return;
	d = dc_de_ventana(caret.w, 1, 0);
	r.left = caret.x; r.top = caret.y; r.right = caret.x + caret.an; r.bottom = caret.y + caret.al;
	{
		extern BOOL WINAPI InvertRect(HDC, const RECT *);
		InvertRect(d, &r);
	}
	ensuciar(raiz_de(caret.w), &dc(d)->tope);
	dc_soltar(d);
	caret.pintado = !caret.pintado;
}
void caret_esconder(Wnd *w)
{
	if (caret.w && caret.pintado && (!w || raiz_de(caret.w) == raiz_de(w)))
		caret_invertir();
}
void caret_ensenar(Wnd *w)
{
	(void)w;
	if (caret.w && caret.visible && !caret.escondido && !caret.pintado && foco == caret.w) {
		u32 t = GetTickCount();
		if ((t / 530) & 1)
			return;
		caret_invertir();
	}
}
static void caret_tic(void)
{
	u32 t = GetTickCount();
	if (!caret.w || !caret.visible || caret.escondido)
		return;
	if (((t / 530) & 1) == caret.pintado || (foco != caret.w && caret.pintado))
		caret_invertir();
}
API(BOOL) CreateCaret(HWND h, HBITMAP b, i32 an, i32 al)
{
	caret_esconder(0);
	caret.w = wnd(h);
	caret.an = an > 0 ? an : 2;
	caret.al = al > 0 ? al : 16;
	caret.x = caret.y = 0;
	caret.escondido = 1;
	caret.visible = 1;
	caret.pintado = 0;
	caret.mapa = b;
	return TRUE;
}
API(BOOL) DestroyCaret(void) { caret_esconder(0); caret.w = 0; return TRUE; }
API(BOOL) SetCaretPos(i32 x, i32 y)
{
	int estaba = caret.pintado;
	if (estaba)
		caret_invertir();
	caret.x = x;
	caret.y = y;
	if (estaba)
		caret_invertir();
	return TRUE;
}
API(BOOL) GetCaretPos(POINT *p) { p->x = caret.x; p->y = caret.y; return TRUE; }
API(BOOL) ShowCaret(HWND h)
{
	(void)h;
	if (caret.escondido > 0)
		caret.escondido--;
	if (!caret.escondido)
		caret_ensenar(caret.w);
	return TRUE;
}
API(BOOL) HideCaret(HWND h)
{
	(void)h;
	caret_esconder(0);
	caret.escondido++;
	return TRUE;
}
API(u32) GetCaretBlinkTime(void) { return 530; }
API(BOOL) SetCaretBlinkTime(u32 t) { (void)t; return TRUE; }

/* ---------------------------------------------------------------------------
 *  El portapapeles (el del sistema: la llamada 40; solo texto)
 * ------------------------------------------------------------------------- */
static int pp_abierto;
static HGLOBAL pp_dato;
static HWND pp_dueno;
API(BOOL) OpenClipboard(HWND h) { pp_abierto = 1; pp_dueno = h; return TRUE; }
API(BOOL) CloseClipboard(void) { pp_abierto = 0; return TRUE; }
API(BOOL) EmptyClipboard(void) { sys_portapapeles(0, "", 0); return TRUE; }
API(HANDLE) SetClipboardData(u32 f, HANDLE h)
{
	if ((f == 1 || f == 7) && h) {          /* CF_TEXT, CF_OEMTEXT */
		const char *t = h;
		u32 n = strlen(t), k, o = 0;
		char *b = m_pedir(n + 1);
		for (k = 0; k < n; k++)
			if (t[k] != '\r')
				b[o++] = f == 1 ? a437c((u8)t[k]) : t[k];
		sys_portapapeles(0, b, o);
		m_soltar(b);
	}
	return h;
}
API(HANDLE) GetClipboardData(u32 f)
{
	u32 n, k, o = 0;
	char *b, *t;
	if (f != 1 && f != 7)
		return 0;
	n = sys_portapapeles(2, 0, 0);
	if (ES_ERROR(n) || !n)
		return 0;
	b = m_pedir(n + 1);
	n = sys_portapapeles(1, b, n);
	if (ES_ERROR(n)) {
		m_soltar(b);
		return 0;
	}
	m_soltar(pp_dato);
	pp_dato = t = m_pedir(n * 2 + 1);
	for (k = 0; k < n; k++) {
		if (b[k] == '\n' && (!k || b[k - 1] != '\r'))
			t[o++] = '\r';
		t[o++] = f == 1 ? a1252c((u8)b[k]) : b[k];
	}
	t[o] = 0;
	m_soltar(b);
	return pp_dato;
}
API(BOOL) IsClipboardFormatAvailable(u32 f)
{
	u32 n;
	if (f != 1 && f != 7)
		return FALSE;
	n = sys_portapapeles(2, 0, 0);
	return !ES_ERROR(n) && n > 0;
}
API(i32) CountClipboardFormats(void) { return IsClipboardFormatAvailable(1) ? 2 : 0; }
API(u32) EnumClipboardFormats(u32 f) { if (!IsClipboardFormatAvailable(1)) return 0; return f == 0 ? 1 : f == 1 ? 7 : 0; }
API(u32) RegisterClipboardFormatA(const char *n) { return 0xC000 + (AddAtomA(n) & 0x3FFF); }
API(HWND) GetClipboardOwner(void) { return pp_dueno; }
API(HWND) SetClipboardViewer(HWND h) { (void)h; return 0; }
API(BOOL) ChangeClipboardChain(HWND a, HWND b) { (void)a; (void)b; return TRUE; }
API(i32) GetPriorityClipboardFormat(u32 *l, i32 n) { i32 k; for (k = 0; k < n; k++) if (IsClipboardFormatAvailable(l[k])) return l[k]; return -1; }

/* ---------------------------------------------------------------------------
 *  Las medidas, el cursor del ratón y lo demás
 * ------------------------------------------------------------------------- */
API(i32) GetSystemMetrics(i32 i)
{
	if ((i == 0 || i == 1 || i == 16 || i == 78 || i == 79) && en_modo_juego()) {
		i32 an, al;                               /* (JUEGOS) en el modo del juego */
		modo_medida(&an, &al);
		return i == 1 || i == 79 ? al : an;
	}
	switch (i) {
	case 0: case 16: case 78: return pantalla_an;    /* SM_CXSCREEN, CXFULLSCREEN, CXVIRTUALSCREEN */
	case 1: case 79: return pantalla_al;
	case 17: return pantalla_al - TITULO_NUCLEO - 36;
	case 2: case 3: return 16;                        /* SM_CXVSCROLL, SM_CYHSCROLL */
	case 4: return TITULO_NUCLEO - BORDE_NUCLEO;      /* SM_CYCAPTION */
	case 5: case 6: return 1;                         /* SM_CXBORDER */
	case 7: case 8: return 3;                         /* SM_CXDLGFRAME */
	case 9: case 10: return 16;                       /* SM_CYVTHUMB, SM_CXHTHUMB */
	case 11: case 12: return 32;                      /* SM_CXICON */
	case 13: case 14: return 32;                      /* SM_CXCURSOR */
	case 15: return 20;                               /* SM_CYMENU */
	case 19: return 1;                                /* SM_MOUSEPRESENT */
	case 20: case 21: return 16;
	case 22: return 0;                                /* SM_DEBUG */
	case 23: return 0;                                /* SM_SWAPBUTTON */
	case 28: case 29: return 120;                     /* SM_CXMIN */
	case 30: case 31: return 22;                      /* SM_CXSIZE */
	case 32: case 33: return BORDE_NUCLEO;            /* SM_CXFRAME */
	case 34: return 120;
	case 35: return 60;
	case 36: case 37: return 4;                       /* SM_CXDOUBLECLK */
	case 38: case 39: return 75;                      /* SM_CXICONSPACING */
	case 42: return 0;
	case 43: return 3;                                /* SM_CMOUSEBUTTONS */
	case 44: return 0;
	case 45: case 46: return 2;                       /* SM_CXEDGE */
	case 49: case 50: return 16;                      /* SM_CXSMICON */
	case 51: return 16;
	case 52: case 53: return 18;
	case 54: case 55: return 18;                      /* SM_CXMENUSIZE */
	case 56: return 8;
	case 57: case 58: return 160;
	case 59: case 60: return pantalla_an;
	case 61: return pantalla_an;                      /* SM_CXMAXIMIZED */
	case 62: return pantalla_al - 36;
	case 63: return 3;                                /* SM_NETWORK */
	case 67: return 0;                                /* SM_CLEANBOOT */
	case 68: case 69: return 4;                       /* SM_CXDRAG */
	case 70: return 0;
	case 75: return 1;                                /* SM_MOUSEWHEELPRESENT */
	case 76: case 77: return 0;
	case 80: return 1;                                /* SM_CMONITORS */
	case 81: return 1;                                /* SM_SAMEDISPLAYFORMAT */
	}
	return 0;
}
API(BOOL) SystemParametersInfoA(u32 a, u32 b, void *c, u32 f)
{
	(void)f;
	switch (a) {
	case 0x30: {                                      /* SPI_GETWORKAREA */
		RECT *r = c;
		r->left = r->top = 0;
		r->right = pantalla_an;
		r->bottom = pantalla_al - 36;
		return TRUE;
	}
	case 0x29:                                        /* SPI_GETNONCLIENTMETRICS */
		if (c) {
			memset(c, 0, b ? b : 340);
			((u32 *)c)[0] = b ? b : 340;
			((u32 *)c)[1] = 1; ((u32 *)c)[2] = 16; ((u32 *)c)[3] = 16;
			((u32 *)c)[4] = 22; ((u32 *)c)[5] = TITULO_NUCLEO - BORDE_NUCLEO;
		}
		return TRUE;
	case 0x1F: case 0x0D: case 0x0E:                  /* SPI_GETICONTITLELOGFONT, ICONHORIZONTALSPACING... */
		if (c && a == 0x1F) memset(c, 0, 60);
		if (c && a != 0x1F) *(u32 *)c = 75;
		return TRUE;
	case 0x10: if (c) *(BOOL *)c = FALSE; return TRUE;     /* SPI_GETSCREENSAVEACTIVE */
	case 0x68: if (c) *(u32 *)c = 3; return TRUE;          /* SPI_GETWHEELSCROLLLINES */
	case 0x26: if (c) *(BOOL *)c = TRUE; return TRUE;      /* SPI_GETDRAGFULLWINDOWS */
	case 0x48: if (c) *(BOOL *)c = FALSE; return TRUE;     /* SPI_GETFONTSMOOTHING */
	case 0x100A: case 0x1012: case 0x1018: case 0x1022: case 0x1024: case 0x1026: case 0x101E:
		if (c) *(BOOL *)c = FALSE;
		return TRUE;
	case 0x01: if (c) *(BOOL *)c = TRUE; return TRUE;      /* SPI_GETBEEP */
	case 0x0A: if (c) *(u32 *)c = 1; return TRUE;          /* SPI_GETKEYBOARDSPEED */
	case 0x16: if (c) *(u32 *)c = 1; return TRUE;
	case 0x6F: if (c) *(u32 *)c = 0; return TRUE;
	}
	if (c && b)
		memset(c, 0, b < 64 ? b : 64);
	return TRUE;
}
API(BOOL) SystemParametersInfoW(u32 a, u32 b, void *c, u32 f) { return SystemParametersInfoA(a, b, c, f); }
/* (JUEGOS) el cursor de los juegos: SetCursorPos lleva la flecha de verdad
 * (los que miran con el ratón la vuelven al centro en cada vuelta), con
 * ClipCursor no sale del recuadro, y con una ventana a pantalla completa va
 * en las medidas del modo del juego */
static RECT recorte;
static int hay_recorte;
API(BOOL) GetCursorPos(POINT *p)
{
	i32 x, y;
	raton_pantalla();
	x = raton_px;
	y = raton_py;
	if (en_modo_juego())
		real_a_modo(&x, &y);
	if (hay_recorte) {
		i32 cx = x < recorte.left ? recorte.left : x >= recorte.right ? recorte.right - 1 : x;
		i32 cy = y < recorte.top ? recorte.top : y >= recorte.bottom ? recorte.bottom - 1 : y;
		x = cx;
		y = cy;
	}
	p->x = x;
	p->y = y;
	return TRUE;
}
API(BOOL) SetCursorPos(i32 x, i32 y)
{
	i32 d[2];
	if (hay_recorte) {
		if (x < recorte.left) x = recorte.left;
		if (x >= recorte.right) x = recorte.right - 1;
		if (y < recorte.top) y = recorte.top;
		if (y >= recorte.bottom) y = recorte.bottom - 1;
	}
	if (en_modo_juego())
		modo_a_real(&x, &y);
	d[0] = x;
	d[1] = y;
	if (!ES_ERROR(sys_ventana_op(8, 0, LIN(d)))) {
		raton_px = x;
		raton_py = y;
	}
	return TRUE;
}
/* lo que se ha movido el ratón de verdad, sin topar con el borde (para DirectInput) */
API(BOOL) HispaRatonMovido(i32 *dx, i32 *dy)
{
	static i32 antes[2];
	static int visto;
	i32 d[2];
	if (ES_ERROR(sys_ventana_op(9, 0, LIN(d))))
		return FALSE;
	if (!visto) {
		antes[0] = d[0];
		antes[1] = d[1];
		visto = 1;
	}
	*dx = d[0] - antes[0];
	*dy = d[1] - antes[1];
	antes[0] = d[0];
	antes[1] = d[1];
	return TRUE;
}
API(HCURSOR) SetCursor(HCURSOR c) { static HCURSOR v; HCURSOR a = v; v = c; return a; }
API(HCURSOR) GetCursor(void) { return (HCURSOR)0x7F00; }
API(i32) ShowCursor(BOOL s)
{
	static i32 n;
	i32 antes = n;
	n += s ? 1 : -1;
	if ((antes < 0) != (n < 0))
		sys_ventana_op(10, 0, n < 0 ? 1 : 0);   /* (JUEGOS) la flecha, escondida o a la vista */
	return n;
}
API(BOOL) ClipCursor(const RECT *r)
{
	hay_recorte = r != 0 && r->right > r->left && r->bottom > r->top;
	if (hay_recorte)
		recorte = *r;
	return TRUE;
}
API(BOOL) GetClipCursor(RECT *r)
{
	if (hay_recorte) {
		*r = recorte;
		return TRUE;
	}
	r->left = r->top = 0;
	r->right = pantalla_an;
	r->bottom = pantalla_al;
	if (en_modo_juego())
		modo_medida(&r->right, &r->bottom);
	return TRUE;
}
API(u32) GetDoubleClickTime(void) { return 450; }
API(BOOL) SetDoubleClickTime(u32 t) { (void)t; return TRUE; }
/* (SONIDO) el sonido del sistema: el de error con MB_ICONHAND; si no, el de aviso */
API(BOOL) MessageBeep(u32 t) { sc(193, 3, 0, (t & 0xF0) == 0x10 ? 3 : 2); return TRUE; }
API(BOOL) FlashWindow(HWND h, BOOL b) { (void)h; (void)b; return TRUE; }
API(BOOL) FlashWindowEx(void *f) { (void)f; return TRUE; }
API(HANDLE) SetWindowsHookExA(i32 i, void *f, HINSTANCE m, u32 t) { (void)i; (void)f; (void)m; (void)t; return (HANDLE)0x5555; }
API(HANDLE) SetWindowsHookA(i32 i, void *f) { (void)i; (void)f; return (HANDLE)0x5555; }
API(BOOL) UnhookWindowsHookEx(HANDLE h) { (void)h; return TRUE; }
API(BOOL) UnhookWindowsHook(i32 i, void *f) { (void)i; (void)f; return TRUE; }
API(LRESULT) CallNextHookEx(HANDLE h, i32 c, WPARAM wp, LPARAM lp) { (void)h; (void)c; (void)wp; (void)lp; return 0; }
API(BOOL) WinHelpA(HWND h, const char *f, u32 c, u32 d) { (void)h; (void)f; (void)c; (void)d; return FALSE; }
API(BOOL) DragDetect(HWND h, POINT p) { (void)h; (void)p; return FALSE; }
API(u32) GetGuiResources(HANDLE p, u32 f) { (void)p; (void)f; return 0; }
API(BOOL) IsHungAppWindow(HWND h) { (void)h; return FALSE; }
API(i32) EnumDisplaySettingsA(const char *d, u32 n, u8 *m)
{
	static const u16 modos[][3] = { { 640, 480, 32 }, { 800, 600, 32 }, { 1024, 768, 32 }, { 640, 480, 16 }, { 800, 600, 16 }, { 640, 480, 8 }, { 320, 240, 32 } };
	(void)d;
	if (n == 0xFFFFFFFF || n == 0xFFFFFFFE) {
		*(u32 *)(m + 104) = 32;
		*(u32 *)(m + 108) = pantalla_an;
		*(u32 *)(m + 112) = pantalla_al;
		*(u32 *)(m + 120) = 60;
		return TRUE;
	}
	if (n >= sizeof modos / sizeof modos[0])
		return FALSE;
	*(u16 *)(m + 36) = 156;
	*(u32 *)(m + 104) = modos[n][2];
	*(u32 *)(m + 108) = modos[n][0];
	*(u32 *)(m + 112) = modos[n][1];
	*(u32 *)(m + 120) = 60;
	*(u32 *)(m + 40) = 0x5C0000;
	return TRUE;
}
u32 gl_llama(u32 op, u32 a, u32 b, u32 c);   /* [GL] dx.c: el puente de OPENGL32.DLL (el modo del juego) */
API(i32) ChangeDisplaySettingsA(void *m, u32 f) { return (i32)gl_llama(6, (u32)m, f, 0); }
API(i32) ChangeDisplaySettingsExA(const char *d, void *m, HWND h, u32 f, void *x) { (void)d; (void)h; (void)x; return (i32)gl_llama(6, (u32)m, f, 0); }
API(HANDLE) MonitorFromWindow(HWND h, u32 f) { (void)h; (void)f; return (HANDLE)0x10001; }
API(HANDLE) MonitorFromPoint(POINT p, u32 f) { (void)p; (void)f; return (HANDLE)0x10001; }
API(HANDLE) MonitorFromRect(const RECT *r, u32 f) { (void)r; (void)f; return (HANDLE)0x10001; }
API(BOOL) GetMonitorInfoA(HANDLE m, u32 *i)
{
	(void)m;
	i[1] = 0; i[2] = 0; i[3] = pantalla_an; i[4] = pantalla_al;
	i[5] = 0; i[6] = 0; i[7] = pantalla_an; i[8] = pantalla_al - 36;
	i[9] = 1;
	return TRUE;
}

/* ---- los rectángulos ---- */
API(BOOL) SetRect(RECT *r, i32 a, i32 b, i32 c, i32 d) { r->left = a; r->top = b; r->right = c; r->bottom = d; return TRUE; }
API(BOOL) SetRectEmpty(RECT *r) { memset(r, 0, sizeof *r); return TRUE; }
API(BOOL) CopyRect(RECT *d, const RECT *s) { *d = *s; return TRUE; }
API(BOOL) InflateRect(RECT *r, i32 x, i32 y) { r->left -= x; r->right += x; r->top -= y; r->bottom += y; return TRUE; }
API(BOOL) OffsetRect(RECT *r, i32 x, i32 y) { r->left += x; r->right += x; r->top += y; r->bottom += y; return TRUE; }
API(BOOL) IsRectEmpty(const RECT *r) { return r->left >= r->right || r->top >= r->bottom; }
API(BOOL) EqualRect(const RECT *a, const RECT *b) { return !memcmp(a, b, sizeof *a); }
API(BOOL) PtInRect(const RECT *r, POINT p) { return p.x >= r->left && p.x < r->right && p.y >= r->top && p.y < r->bottom; }
API(BOOL) IntersectRect(RECT *d, const RECT *a, const RECT *b)
{
	d->left = MAX(a->left, b->left); d->top = MAX(a->top, b->top);
	d->right = MIN(a->right, b->right); d->bottom = MIN(a->bottom, b->bottom);
	if (d->left >= d->right || d->top >= d->bottom) {
		SetRectEmpty(d);
		return FALSE;
	}
	return TRUE;
}
API(BOOL) UnionRect(RECT *d, const RECT *a, const RECT *b)
{
	if (IsRectEmpty(a)) { *d = *b; return !IsRectEmpty(b); }
	if (IsRectEmpty(b)) { *d = *a; return TRUE; }
	d->left = MIN(a->left, b->left); d->top = MIN(a->top, b->top);
	d->right = MAX(a->right, b->right); d->bottom = MAX(a->bottom, b->bottom);
	return TRUE;
}
API(BOOL) SubtractRect(RECT *d, const RECT *a, const RECT *b)
{
	RECT i;
	*d = *a;
	if (!IntersectRect(&i, a, b))
		return !IsRectEmpty(d);
	if (i.top == a->top && i.bottom == a->bottom) {
		if (i.left == a->left) d->left = i.right;
		else if (i.right == a->right) d->right = i.left;
	} else if (i.left == a->left && i.right == a->right) {
		if (i.top == a->top) d->top = i.bottom;
		else if (i.bottom == a->bottom) d->bottom = i.top;
	}
	return !IsRectEmpty(d);
}

/* ---- las letras de USER32 ---- */
API(char *) CharUpperA(char *s)
{
	if ((u32)s < 0x10000)
		return (char *)(u32)mayus((u32)s);
	{
		char *p;
		for (p = s; *p; p++)
			*p = mayus(*p);
	}
	return s;
}
API(char *) CharLowerA(char *s)
{
	if ((u32)s < 0x10000)
		return (char *)(u32)minus((u32)s);
	{
		char *p;
		for (p = s; *p; p++)
			*p = minus(*p);
	}
	return s;
}
API(u32) CharUpperBuffA(char *s, u32 n) { u32 k; for (k = 0; k < n; k++) s[k] = mayus(s[k]); return n; }
API(u32) CharLowerBuffA(char *s, u32 n) { u32 k; for (k = 0; k < n; k++) s[k] = minus(s[k]); return n; }
API(char *) CharNextA(const char *s) { return (char *)(*s ? s + 1 : s); }
API(char *) CharPrevA(const char *a, const char *s) { return (char *)(s > a ? s - 1 : a); }
API(char *) CharNextExA(u16 cp, const char *s, u32 f) { (void)cp; (void)f; return CharNextA(s); }
API(BOOL) IsCharAlphaA(char c) { u8 u = c; return (u >= 'A' && u <= 'Z') || (u >= 'a' && u <= 'z') || (u >= 0xC0 && u != 0xD7 && u != 0xF7); }
API(BOOL) IsCharAlphaNumericA(char c) { return IsCharAlphaA(c) || (c >= '0' && c <= '9'); }
API(BOOL) IsCharUpperA(char c) { u8 u = c; return (u >= 'A' && u <= 'Z') || (u >= 0xC0 && u <= 0xDE && u != 0xD7); }
API(BOOL) IsCharLowerA(char c) { u8 u = c; return (u >= 'a' && u <= 'z') || (u >= 0xDF && u != 0xF7); }
API(BOOL) CharToOemA(const char *s, char *d) { a437(d, s, 0x7FFFFFFF); return TRUE; }
API(BOOL) OemToCharA(const char *s, char *d) { a1252(d, s, 0x7FFFFFFF); return TRUE; }
API(BOOL) CharToOemBuffA(const char *s, char *d, u32 n) { u32 k; for (k = 0; k < n; k++) d[k] = a437c((u8)s[k]); return TRUE; }
API(BOOL) OemToCharBuffA(const char *s, char *d, u32 n) { u32 k; for (k = 0; k < n; k++) d[k] = a1252c((u8)s[k]); return TRUE; }
API(i32) wvsprintfA(char *d, const char *f, va_list va) { return formato_win(d, 1025, f, va); }
EXPC(i32) wsprintfA(char *d, const char *f, ...)
{
	va_list va;
	i32 r;
	va_start(va, f);
	r = formato_win(d, 1025, f, va);
	va_end(va);
	return r;
}
API(i32) LoadStringA(HINSTANCE h, u32 id, char *d, i32 n)
{
	u32 tam;
	const u16 *p = recurso(h, (const char *)6, (const char *)((id >> 4) + 1), &tam);
	u32 k, i;
	if (!d || n <= 0)
		return 0;
	d[0] = 0;
	if (!p)
		return 0;
	for (k = 0; k < (id & 15); k++)
		p += *p + 1;
	for (i = 0; i < *p && (i32)i + 1 < n; i++)
		d[i] = de_uni(p[1 + i]);
	d[i] = 0;
	return i;
}
API(i32) LoadStringW(HINSTANCE h, u32 id, WCHAR *d, i32 n)
{
	char b[1024];
	i32 r = LoadStringA(h, id, b, sizeof b);
	if (n <= 0)
		return 0;
	a_w(d, b, n);
	return MIN(r, n - 1);
}

/* ---------------------------------------------------------------------------
 *  Las barras de desplazamiento de las ventanas (WS_VSCROLL, WS_HSCROLL)
 * ------------------------------------------------------------------------- */
typedef struct { u32 tam, mask; i32 min, max; u32 pag; i32 pos, track; } SCROLLINFO;
static Barra *barra_de(Wnd *w, i32 n)
{
	if (n == 1)
		return &w->bv;
	if (n == 0)
		return &w->bh;
	if (n == 2 && w->ctl)
		return (Barra *)w->ctl;    /* (el control SCROLLBAR) */
	return 0;
}
static void barra_ajusta(Barra *b)
{
	i32 max = b->max - (b->pag ? b->pag - 1 : 0);
	if (b->pos > max) b->pos = max;
	if (b->pos < b->min) b->pos = b->min;
}
API(i32) SetScrollInfo(HWND h, i32 n, const void *si_, BOOL pintar)
{
	Wnd *w = wnd(h);
	const SCROLLINFO *si = si_;
	Barra *b;
	if (!w || !(b = barra_de(w, n)))
		return 0;
	if (si->mask & 1) { b->min = si->min; b->max = si->max; }
	if (si->mask & 2) b->pag = si->pag;
	if (si->mask & 4) b->pos = si->pos;
	barra_ajusta(b);
	if (n != 2 && (si->mask & 1) && !(si->mask & 8)) {
		/* SIF_DISABLENOSCROLL no: si no hay nada que desplazar, se quita */
		u32 bit = n == 1 ? WS_VSCROLL : WS_HSCROLL;
		u32 antes = w->estilo;
		if (b->max <= b->min || (b->pag && (u32)(b->max - b->min + 1) <= (u32)b->pag))
			w->estilo &= ~bit;
		else
			w->estilo |= bit;
		if (antes != w->estilo) {
			RECT d;
			w->nc_sucio = 1;
			invalidar(w, 0, 1);
			lo_de_dentro(w, &d);
			enviar(w, WM_SIZE, 0, MAKELONG(d.right, d.bottom));
		}
	}
	if (pintar) {
		if (n == 2)
			invalidar(w, 0, 1);
		else
			w->nc_sucio = 1;
	}
	return b->pos;
}
API(BOOL) GetScrollInfo(HWND h, i32 n, void *si_)
{
	Wnd *w = wnd(h);
	SCROLLINFO *si = si_;
	Barra *b;
	if (!w || !(b = barra_de(w, n)))
		return FALSE;
	if (si->mask & 1) { si->min = b->min; si->max = b->max; }
	if (si->mask & 2) si->pag = b->pag;
	if (si->mask & 4) si->pos = b->pos;
	if (si->mask & 16) si->track = b->track;
	return TRUE;
}
API(i32) SetScrollPos(HWND h, i32 n, i32 p, BOOL pintar)
{
	SCROLLINFO si = { sizeof si, 4, 0, 0, 0, p, 0 };
	Wnd *w = wnd(h);
	Barra *b;
	i32 a;
	if (!w || !(b = barra_de(w, n)))
		return 0;
	a = b->pos;
	SetScrollInfo(h, n, &si, pintar);
	return a;
}
API(i32) GetScrollPos(HWND h, i32 n) { Wnd *w = wnd(h); Barra *b = w ? barra_de(w, n) : 0; return b ? b->pos : 0; }
API(BOOL) SetScrollRange(HWND h, i32 n, i32 a, i32 b, BOOL pintar)
{
	SCROLLINFO si = { sizeof si, 1, a, b, 0, 0, 0 };
	SetScrollInfo(h, n, &si, pintar);
	return TRUE;
}
API(BOOL) GetScrollRange(HWND h, i32 n, i32 *a, i32 *b)
{
	Wnd *w = wnd(h);
	Barra *x = w ? barra_de(w, n) : 0;
	if (!x)
		return FALSE;
	*a = x->min;
	*b = x->max;
	return TRUE;
}
API(BOOL) ShowScrollBar(HWND h, i32 n, BOOL s)
{
	Wnd *w = wnd(h);
	u32 bits = n == 1 ? WS_VSCROLL : n == 0 ? WS_HSCROLL : n == 3 ? WS_VSCROLL | WS_HSCROLL : 0, antes;
	if (!w)
		return FALSE;
	if (n == 2)
		return ShowWindow(h, s ? 5 : 0);
	antes = w->estilo;
	if (s)
		w->estilo |= bits;
	else
		w->estilo &= ~bits;
	if (antes != w->estilo) {
		RECT d;
		w->nc_sucio = 1;
		invalidar(w, 0, 1);
		lo_de_dentro(w, &d);
		enviar(w, WM_SIZE, 0, MAKELONG(d.right, d.bottom));
	}
	return TRUE;
}
API(BOOL) EnableScrollBar(HWND h, u32 n, u32 a) { (void)h; (void)n; (void)a; return TRUE; }
/* desplaza lo de dentro (los puntos del lienzo) y deja por pintar lo que queda al descubierto */
API(i32) ScrollWindowEx(HWND h, i32 dx, i32 dy, const RECT *r, const RECT *clip, HRGN g, RECT *upd, u32 fl)
{
	Wnd *w = wnd(h), *raiz;
	RECT d, a;
	POINT o;
	i32 y;
	Sup *s;
	(void)g;
	if (!w)
		return 0;
	raiz = raiz_de(w);
	lo_de_dentro(w, &d);
	a = r ? *r : d;
	if (clip) {
		a.left = MAX(a.left, clip->left); a.top = MAX(a.top, clip->top);
		a.right = MIN(a.right, clip->right); a.bottom = MIN(a.bottom, clip->bottom);
	}
	a.left = MAX(a.left, 0); a.top = MAX(a.top, 0);
	a.right = MIN(a.right, d.right); a.bottom = MIN(a.bottom, d.bottom);
	if (a.left >= a.right || a.top >= a.bottom)
		return 1;
	caret_esconder(w);
	if (raiz && raiz->lienzo.bits && ventana_visible(w) && (dx || dy)) {
		RECT v;
		i32 an, x0, x1;
		s = &raiz->lienzo;
		dentro_en_lienzo(w, &o);
		visible_en_lienzo(w, &v, 1);
		/* el rectángulo, en el lienzo, recortado a lo que se ve */
		{
			RECT L = { a.left + o.x, a.top + o.y, a.right + o.x, a.bottom + o.y };
			L.left = MAX(L.left, v.left); L.top = MAX(L.top, v.top);
			L.right = MIN(L.right, v.right); L.bottom = MIN(L.bottom, v.bottom);
			x0 = MAX(L.left, L.left + dx);
			x1 = MIN(L.right, L.right + dx);
			an = x1 - x0;
			if (an > 0) {
				if (dy <= 0) {
					for (y = L.top - MIN(dy, 0); y < L.bottom; y++)
						if (y + dy >= L.top && y + dy < L.bottom)
							memmove(s->bits + (y + dy) * s->paso + x0 * 4, s->bits + y * s->paso + (x0 - dx) * 4, an * 4);
				} else {
					for (y = L.bottom - 1; y >= L.top; y--)
						if (y + dy >= L.top && y + dy < L.bottom)
							memmove(s->bits + (y + dy) * s->paso + x0 * 4, s->bits + y * s->paso + (x0 - dx) * 4, an * 4);
				}
			}
			ensuciar(raiz, &L);
		}
	}
	/* lo que queda al descubierto */
	{
		RECT u = a;
		if (dy > 0) u.bottom = MIN(a.bottom, a.top + dy);
		else if (dy < 0) u.top = MAX(a.top, a.bottom + dy);
		if (dx > 0) u.right = MIN(a.right, a.left + dx);
		else if (dx < 0) u.left = MAX(a.left, a.right + dx);
		if (dx && dy)
			u = a;
		invalidar(w, &u, (fl & 4) != 0 || 1);
		if (upd)
			*upd = u;
	}
	/* las hijas se mueven con él (SW_SCROLLCHILDREN) */
	if (fl & 1) {
		Wnd *c;
		for (c = w->hijas; c; c = c->sig) {
			c->r.left += dx; c->r.right += dx;
			c->r.top += dy; c->r.bottom += dy;
		}
	}
	return 2;
}
API(BOOL) ScrollWindow(HWND h, i32 dx, i32 dy, const RECT *r, const RECT *c) { return ScrollWindowEx(h, dx, dy, r, c, 0, 0, 1) != 0; }
API(BOOL) ScrollDC(HDC d, i32 dx, i32 dy, const RECT *r, const RECT *c, HRGN g, RECT *u)
{
	Dc *x = dc(d);
	if (!x || !x->w)
		return FALSE;
	ScrollWindowEx((HWND)x->w, dx, dy, r, c, g, u, 0);
	return TRUE;
}

/* ---------------------------------------------------------------------------
 *  Arrancar y acabar
 * ------------------------------------------------------------------------- */
static void al_salir(void)
{
	Wnd *w;
	if (!escritorio)
		return;
	presentar_todo();
	for (w = escritorio->hijas; w; w = w->sig)
		if (w->vn) {
			sys_cerrar_ventana(w->vn);
			w->vn = 0;
		}
}
void prepara_user(void)
{
	if (escritorio)
		return;
	{
		u32 a = sys_dato(11), b = sys_dato(12);
		if (!ES_ERROR(a) && a)
			pantalla_an = a;
		if (!ES_ERROR(b) && b)
			pantalla_al = b;
	}
	escritorio = m_cero(sizeof *escritorio);
	escritorio->magia = MAGIA_WND;
	escritorio->estilo = WS_VISIBLE;
	escritorio->r.right = pantalla_an;
	escritorio->r.bottom = pantalla_al;
	registrar_propias();
	escritorio->clase = clase_de("#32770");
	escritorio->proc = DefWindowProcA;
	al_salir_user = al_salir;
	tic_prog = caret_tic;
}
/* un cuadro con el texto (los errores al cargar): en una ventana si hay
 * escritorio; si no, por la consola */
void mensaje_fatal(const char *t)
{
	char b[300];
	aviso_serie("%s", t);
	if (es_ventanas && sys_dato(13) == 1) {
		MessageBoxA(0, t, "Hispa-Pentium", 0x10);
		return;
	}
	a437(b, t, sizeof b);
	sys_escribir(b, strlen(b));
	sys_escribir("\n", 1);
}

/* ---- (HL) lo que pide un juego de 1998-2000 ---- */
/* la última ventana emergente activa de las de «h» (o ella misma) */
API(HWND) GetLastActivePopup(HWND h)
{
	Wnd *w = wnd(h), *p;
	if (!w)
		return 0;
	if (activa && activa != w && raiz_de(activa)->dueno == raiz_de(w) && ventana_visible(activa))
		return (HWND)activa;
	for (p = escritorio ? escritorio->hijas : 0; p; p = p->sig)
		if (p->dueno == w && !p->destruida && ventana_visible(p) && !(p->estilo & WS_DISABLED))
			return (HWND)p;
	return h;
}
/* quita de lo que pinta «d» lo que «h» tiene por repintar */
API(i32) ExcludeUpdateRgn(HDC d, HWND h)
{
	extern i32 WINAPI ExcludeClipRect(HDC, i32, i32, i32, i32);
	extern i32 WINAPI GetClipBox(HDC, RECT *);
	RECT r;
	if (!wnd(h))
		return 0;                       /* ERROR */
	if (GetUpdateRect(h, &r, FALSE))
		return ExcludeClipRect(d, r.left, r.top, r.right, r.bottom);
	return GetClipBox(d, &r);
}
/* los programas que lanza ya están listos (CreateProcess espera, o corren a la vez) */
API(u32) WaitForInputIdle(HANDLE p, u32 ms) { (void)p; (void)ms; return 0; }
/* apagar el equipo desde un programa de Windows: no (el que apaga es el
 * usuario, desde Inicio); se dice y se apunta */
API(BOOL) ExitWindowsEx(u32 fl, u32 r)
{
	void diag_apunta(const char *f, ...);
	(void)r;
	diag_apunta("ExitWindowsEx(%x): Hispa-Pentium no deja que un programa apague el equipo (se apaga desde Inicio)", fl);
	pon_error(1314);                    /* ERROR_PRIVILEGE_NOT_HELD */
	return FALSE;
}

/* ---- (HL) DDE: no hay con quién hablar. Se puede empezar (DdeInitialize)
 * y hacer cadenas y datos, pero conectar falla siempre, limpio
 * (DMLERR_NO_CONV_ESTABLISHED), como un Windows sin ese servidor ---- */
#define DDE_CAD 32
static char *dde_cad[DDE_CAD];
static u32 dde_error;
typedef u32 (WINAPI *DDECB)(u32, u32, HANDLE, HANDLE, HANDLE, HANDLE, u32, u32);
API(u32) DdeInitializeA(u32 *id, DDECB f, u32 cmd, u32 r) { (void)f; (void)cmd; (void)r; if (id) *id = 1; dde_error = 0; return 0; }
API(u32) DdeInitializeW(u32 *id, DDECB f, u32 cmd, u32 r) { return DdeInitializeA(id, f, cmd, r); }
API(BOOL) DdeUninitialize(u32 id) { (void)id; return TRUE; }
API(u32) DdeGetLastError(u32 id) { u32 e = dde_error; (void)id; dde_error = 0; return e; }
API(HANDLE) DdeCreateStringHandleA(u32 id, const char *s, i32 cp)
{
	u32 k;
	(void)id; (void)cp;
	for (k = 0; k < DDE_CAD; k++)
		if (dde_cad[k] && !stricmp_(dde_cad[k], s ? s : ""))
			return (HANDLE)(k + 1);
	for (k = 0; k < DDE_CAD; k++)
		if (!dde_cad[k]) {
			dde_cad[k] = m_pedir(strlen(s ? s : "") + 1);
			if (!dde_cad[k])
				break;
			strcpy(dde_cad[k], s ? s : "");
			return (HANDLE)(k + 1);
		}
	dde_error = 0x4008;                 /* DMLERR_MEMORY_ERROR */
	return 0;
}
API(HANDLE) DdeCreateStringHandleW(u32 id, const WCHAR *s, i32 cp)
{
	char b[256];
	if (s)
		de_w(b, s, sizeof b);
	return DdeCreateStringHandleA(id, s ? b : 0, cp);
}
API(BOOL) DdeFreeStringHandle(u32 id, HANDLE h) { (void)id; (void)h; return TRUE; }
API(BOOL) DdeKeepStringHandle(u32 id, HANDLE h) { (void)id; (void)h; return TRUE; }
API(u32) DdeQueryStringA(u32 id, HANDLE h, char *d, u32 n, i32 cp)
{
	u32 k = (u32)h - 1, l;
	(void)id; (void)cp;
	if (k >= DDE_CAD || !dde_cad[k])
		return 0;
	l = strlen(dde_cad[k]);
	if (d && n)
		copia_n(d, dde_cad[k], n);
	return l;
}
API(i32) DdeCmpStringHandles(HANDLE a, HANDLE b) { return a == b ? 0 : (u32)a < (u32)b ? -1 : 1; }
API(HANDLE) DdeConnect(u32 id, HANDLE serv, HANDLE tema, void *cc) { (void)id; (void)serv; (void)tema; (void)cc; dde_error = 0x400A; return 0; }
API(HANDLE) DdeConnectList(u32 id, HANDLE serv, HANDLE tema, HANDLE l, void *cc) { (void)id; (void)serv; (void)tema; (void)l; (void)cc; dde_error = 0x400A; return 0; }
API(HANDLE) DdeQueryNextServer(HANDLE l, HANDLE c) { (void)l; (void)c; return 0; }
API(HANDLE) DdeReconnect(HANDLE c) { (void)c; dde_error = 0x400A; return 0; }
API(BOOL) DdeDisconnect(HANDLE c) { (void)c; dde_error = 0x400B; return FALSE; }        /* DMLERR_NO_CONV_ESTABLISHED / INVALIDPARAMETER */
API(BOOL) DdeDisconnectList(HANDLE l) { (void)l; return FALSE; }
API(HANDLE) DdeClientTransaction(void *d, u32 n, HANDLE c, HANDLE it, u32 fmt_, u32 tipo, u32 t, u32 *r)
{
	(void)d; (void)n; (void)c; (void)it; (void)fmt_; (void)tipo; (void)t;
	if (r)
		*r = 0;
	dde_error = 0x400A;
	return 0;
}
API(BOOL) DdeAbandonTransaction(u32 id, HANDLE c, u32 t) { (void)id; (void)c; (void)t; return TRUE; }
API(u32) DdeQueryConvInfo(HANDLE c, u32 t, void *i) { (void)c; (void)t; (void)i; dde_error = 0x400B; return 0; }
API(BOOL) DdeSetUserHandle(HANDLE c, u32 i, u32 u) { (void)c; (void)i; (void)u; return FALSE; }
API(BOOL) DdeEnableCallback(u32 id, HANDLE c, u32 cmd) { (void)id; (void)c; (void)cmd; return TRUE; }
API(HANDLE) DdeNameService(u32 id, HANDLE a, HANDLE b, u32 f) { (void)id; (void)a; (void)b; (void)f; return (HANDLE)1; }
API(BOOL) DdePostAdvise(u32 id, HANDLE t, HANDLE i) { (void)id; (void)t; (void)i; return TRUE; }
API(BOOL) DdeImpersonateClient(HANDLE c) { (void)c; return FALSE; }
/* los datos: un bloque con su largo delante */
API(HANDLE) DdeCreateDataHandle(u32 id, const void *s, u32 n, u32 off, HANDLE it, u32 f, u32 cmd)
{
	u32 *b = m_cero(n + 8);
	(void)id; (void)it; (void)f; (void)cmd;
	if (!b) {
		dde_error = 0x4008;
		return 0;
	}
	b[0] = n;
	if (s)
		memcpy(b + 2, (const u8 *)s + off, n);
	return (HANDLE)b;
}
API(HANDLE) DdeAddData(HANDLE h, const void *s, u32 n, u32 off)
{
	u32 *b = h;
	if (!b)
		return 0;
	if (off + n > b[0]) {
		b = m_cambiar(b, off + n + 8);
		if (!b)
			return 0;
		memset((u8 *)(b + 2) + b[0], 0, off + n - b[0]);
		b[0] = off + n;
	}
	memcpy((u8 *)(b + 2) + off, s, n);
	return (HANDLE)b;
}
API(u32) DdeGetData(HANDLE h, void *d, u32 n, u32 off)
{
	u32 *b = h, t;
	if (!b || off > b[0])
		return 0;
	t = b[0] - off;
	if (!d)
		return t;
	if (t > n)
		t = n;
	memcpy(d, (u8 *)(b + 2) + off, t);
	return t;
}
API(void *) DdeAccessData(HANDLE h, u32 *n) { u32 *b = h; if (!b) return 0; if (n) *n = b[0]; return b + 2; }
API(BOOL) DdeUnaccessData(HANDLE h) { (void)h; return TRUE; }
API(BOOL) DdeFreeDataHandle(HANDLE h) { if (h) m_soltar(h); return TRUE; }
/* los lParam de los mensajes WM_DDE_*: los que llevan dos valores (ACK,
 * ADVISE, DATA, POKE) van en una pareja guardada aquí */
#define DDE_PAR 32
static struct { u32 uso, lo, hi; } dde_par[DDE_PAR];
static int dde_lleva_dos(u32 m) { return m == 0x3E4 || m == 0x3E2 || m == 0x3E5 || m == 0x3E7; }
static u32 *dde_de(LPARAM lp)
{
	u32 k = (u32)lp - 0xDDE00001u;
	return k < DDE_PAR && dde_par[k].uso ? &dde_par[k].uso : 0;
}
API(LPARAM) PackDDElParam(u32 m, u32 lo, u32 hi)
{
	u32 k;
	if (m == 0x3E8)                    /* WM_DDE_EXECUTE: el bloque de la orden, tal cual */
		return (LPARAM)hi;
	if (!dde_lleva_dos(m))
		return (LPARAM)((lo & 0xFFFF) | (hi << 16));
	for (k = 0; k < DDE_PAR; k++)
		if (!dde_par[k].uso) {
			dde_par[k].uso = 1;
			dde_par[k].lo = lo;
			dde_par[k].hi = hi;
			return (LPARAM)(0xDDE00001u + k);
		}
	return 0;
}
API(BOOL) UnpackDDElParam(u32 m, LPARAM lp, u32 *lo, u32 *hi)
{
	u32 *p;
	if (m == 0x3E8) {
		if (lo)
			*lo = 0;
		if (hi)
			*hi = (u32)lp;
		return TRUE;
	}
	if (!dde_lleva_dos(m)) {
		if (lo)
			*lo = (u32)lp & 0xFFFF;
		if (hi)
			*hi = (u32)lp >> 16;
		return TRUE;
	}
	p = dde_de(lp);
	if (!p) {
		if (lo)
			*lo = 0;
		if (hi)
			*hi = 0;
		return FALSE;
	}
	if (lo)
		*lo = p[1];
	if (hi)
		*hi = p[2];
	return TRUE;
}
API(BOOL) FreeDDElParam(u32 m, LPARAM lp)
{
	u32 *p = dde_lleva_dos(m) ? dde_de(lp) : 0;
	if (p)
		p[0] = 0;
	return TRUE;
}
API(LPARAM) ReuseDDElParam(LPARAM lp, u32 de, u32 a, u32 lo, u32 hi)
{
	FreeDDElParam(de, lp);
	return PackDDElParam(a, lo, hi);
}
