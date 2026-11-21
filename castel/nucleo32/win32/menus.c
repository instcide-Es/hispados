/* menus.c - WIN32.BIN: los menús de USER32. La barra de menús la pinta
 * WIN32.BIN en lo de arriba de la ventana; lo que cuelga de ella (y lo de
 * TrackPopupMenu) es el menú emergente del núcleo (la llamada 39: el mismo
 * aire que los de Hispa-Pentium), con las marcas y los submenús como un
 * menú detrás de otro.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "ventanas.h"
#include "k32.h"

#define API(t) t WINAPI
#define MF_GRAYED 1
#define MF_DISABLED 2
#define MF_BITMAP 4
#define MF_CHECKED 8
#define MF_POPUP 0x10
#define MF_BYPOSITION 0x400
#define MF_SEPARATOR 0x800
#define MF_OWNERDRAW 0x100
#define MF_DEFAULT 0x1000
#define MF_RADIO 0x200
#define ALTO_BARRA 20

Menu *menu(HMENU h)
{
	Menu *m = (Menu *)h;
	if (!m || (u32)m < 0x10000 || (u32)m >= 0x7FBF0000 || ((u32)m & 3) || m->magia != MAGIA_MENU)
		return 0;
	return m;
}
static Menu *nuevo_menu(void)
{
	Menu *m = m_cero(sizeof *m);
	m->magia = MAGIA_MENU;
	m->def = 0xFFFFFFFF;
	return m;
}
API(HMENU) CreateMenu(void) { return (HMENU)nuevo_menu(); }
API(HMENU) CreatePopupMenu(void) { return (HMENU)nuevo_menu(); }
API(BOOL) IsMenu(HMENU h) { return menu(h) != 0; }
API(BOOL) DestroyMenu(HMENU h)
{
	Menu *m = menu(h);
	i32 k;
	if (!m)
		return FALSE;
	for (k = 0; k < m->n; k++) {
		if (m->it[k].sub)
			DestroyMenu((HMENU)m->it[k].sub);
		m_soltar(m->it[k].texto);
	}
	m_soltar(m->it);
	m->magia = 0;
	m_soltar(m);
	return TRUE;
}
/* el elemento por número (MF_BYCOMMAND, también en los submenús) o por sitio */
static Item *item(Menu *m, u32 cual, u32 fl, Menu **de, i32 *sitio)
{
	i32 k;
	if (!m)
		return 0;
	if (fl & MF_BYPOSITION) {
		if (cual >= (u32)m->n)
			return 0;
		if (de) *de = m;
		if (sitio) *sitio = cual;
		return &m->it[cual];
	}
	for (k = 0; k < m->n; k++) {
		if (!m->it[k].sub && m->it[k].id == cual) {
			if (de) *de = m;
			if (sitio) *sitio = k;
			return &m->it[k];
		}
		if (m->it[k].sub) {
			Item *i = item(m->it[k].sub, cual, fl, de, sitio);
			if (i)
				return i;
			if ((u32)m->it[k].sub == cual) {
				if (de) *de = m;
				if (sitio) *sitio = k;
				return &m->it[k];
			}
		}
	}
	return 0;
}
static void pon_texto(Item *i, const char *t)
{
	m_soltar(i->texto);
	i->texto = 0;
	if (t && (u32)t >= 0x10000) {
		i->texto = m_pedir(strlen(t) + 1);
		strcpy(i->texto, t);
	}
}
static int meter(Menu *m, i32 sitio, u32 fl, u32 id, const char *t)
{
	Item *i;
	if (m->n == m->cabe) {
		m->cabe = m->cabe ? m->cabe * 2 : 8;
		m->it = m_cambiar(m->it, m->cabe * sizeof(Item));
	}
	if (sitio < 0 || sitio > m->n)
		sitio = m->n;
	memmove(&m->it[sitio + 1], &m->it[sitio], (m->n - sitio) * sizeof(Item));
	m->n++;
	i = &m->it[sitio];
	memset(i, 0, sizeof *i);
	i->fl = fl & ~MF_BYPOSITION;
	if (fl & MF_POPUP)
		i->sub = menu((HMENU)id);
	else
		i->id = id & 0xFFFF;
	if (!(fl & (MF_SEPARATOR | MF_BITMAP | MF_OWNERDRAW)))
		pon_texto(i, t);
	if (fl & MF_OWNERDRAW)
		i->dato = (u32)t;
	return 1;
}
API(BOOL) AppendMenuA(HMENU h, u32 fl, u32 id, const char *t)
{
	Menu *m = menu(h);
	if (!m)
		return FALSE;
	return meter(m, -1, fl, id, t);
}
API(BOOL) AppendMenuW(HMENU h, u32 fl, u32 id, const WCHAR *t)
{
	char b[256];
	if (t && (u32)t >= 0x10000 && !(fl & (MF_SEPARATOR | MF_BITMAP | MF_OWNERDRAW))) {
		de_w(b, t, sizeof b);
		return AppendMenuA(h, fl, id, b);
	}
	return AppendMenuA(h, fl, id, (const char *)t);
}
API(BOOL) InsertMenuA(HMENU h, u32 pos, u32 fl, u32 id, const char *t)
{
	Menu *m = menu(h), *de = m;
	i32 sitio = -1;
	if (!m)
		return FALSE;
	if (pos != 0xFFFFFFFF) {
		if (!item(m, pos, fl, &de, &sitio)) {
			de = m;
			sitio = -1;
		}
	}
	return meter(de, sitio, fl, id, t);
}
API(BOOL) ModifyMenuA(HMENU h, u32 pos, u32 fl, u32 id, const char *t)
{
	Menu *m = menu(h);
	Item *i = item(m, pos, fl, 0, 0);
	if (!i)
		return FALSE;
	i->fl = fl & ~MF_BYPOSITION;
	if (fl & MF_POPUP)
		i->sub = menu((HMENU)id);
	else {
		i->id = id & 0xFFFF;
		i->sub = 0;
	}
	if (!(fl & (MF_SEPARATOR | MF_BITMAP | MF_OWNERDRAW)))
		pon_texto(i, t);
	return TRUE;
}
static BOOL quitar(HMENU h, u32 pos, u32 fl, int destruir)
{
	Menu *m = menu(h), *de;
	i32 sitio;
	Item *i = item(m, pos, fl, &de, &sitio);
	if (!i)
		return FALSE;
	if (destruir && i->sub)
		DestroyMenu((HMENU)i->sub);
	m_soltar(i->texto);
	memmove(&de->it[sitio], &de->it[sitio + 1], (de->n - sitio - 1) * sizeof(Item));
	de->n--;
	return TRUE;
}
API(BOOL) DeleteMenu(HMENU h, u32 pos, u32 fl) { return quitar(h, pos, fl, 1); }
API(BOOL) RemoveMenu(HMENU h, u32 pos, u32 fl) { return quitar(h, pos, fl, 0); }
API(u32) CheckMenuItem(HMENU h, u32 id, u32 fl)
{
	Item *i = item(menu(h), id, fl, 0, 0);
	u32 a;
	if (!i)
		return 0xFFFFFFFF;
	a = i->fl & MF_CHECKED;
	i->fl = (i->fl & ~MF_CHECKED) | (fl & MF_CHECKED);
	return a;
}
API(BOOL) CheckMenuRadioItem(HMENU h, u32 a, u32 b, u32 c, u32 fl)
{
	u32 k;
	for (k = a; k <= b; k++) {
		Item *i = item(menu(h), k, fl, 0, 0);
		if (i) {
			i->fl &= ~MF_CHECKED;
			if (k == c)
				i->fl |= MF_CHECKED | MF_RADIO;
		}
	}
	return TRUE;
}
API(BOOL) EnableMenuItem(HMENU h, u32 id, u32 fl)
{
	Item *i = item(menu(h), id, fl, 0, 0);
	u32 a;
	if (!i)
		return 0xFFFFFFFF;
	a = i->fl & 3;
	i->fl = (i->fl & ~3) | (fl & 3);
	return a;
}
API(u32) GetMenuState(HMENU h, u32 id, u32 fl)
{
	Item *i = item(menu(h), id, fl, 0, 0);
	if (!i)
		return 0xFFFFFFFF;
	if (i->sub)
		return (i->sub->n << 8) | (i->fl & 0xFF) | MF_POPUP;
	return i->fl & 0xFFFF;
}
API(i32) GetMenuItemCount(HMENU h) { Menu *m = menu(h); return m ? m->n : -1; }
API(u32) GetMenuItemID(HMENU h, i32 p)
{
	Menu *m = menu(h);
	if (!m || p < 0 || p >= m->n)
		return 0xFFFFFFFF;
	return m->it[p].sub ? 0xFFFFFFFF : m->it[p].id;
}
API(HMENU) GetSubMenu(HMENU h, i32 p)
{
	Menu *m = menu(h);
	if (!m || p < 0 || p >= m->n)
		return 0;
	return (HMENU)m->it[p].sub;
}
API(i32) GetMenuStringA(HMENU h, u32 id, char *d, i32 n, u32 fl)
{
	Item *i = item(menu(h), id, fl, 0, 0);
	if (!i)
		return 0;
	if (!d)
		return i->texto ? strlen(i->texto) : 0;
	copia_n(d, i->texto ? i->texto : "", n);
	return strlen(d);
}
typedef struct { u32 tam, mask, tipo, estado, id; HMENU sub; HBITMAP mc, mn; u32 dato; char *texto; u32 cch; HBITMAP mapa; } MENUITEMINFOA;
API(BOOL) GetMenuItemInfoA(HMENU h, u32 cual, BOOL pos, MENUITEMINFOA *mi)
{
	Item *i = item(menu(h), cual, pos ? MF_BYPOSITION : 0, 0, 0);
	if (!i)
		return FALSE;
	if (mi->mask & 0x110) mi->tipo = (i->fl & (MF_SEPARATOR | MF_BITMAP | MF_OWNERDRAW | MF_RADIO | 0x60 | 0x4000));
	if (mi->mask & 1) mi->estado = i->fl & (3 | MF_CHECKED | MF_DEFAULT | 0x80);
	if (mi->mask & 2) mi->id = i->id;
	if (mi->mask & 4) mi->sub = (HMENU)i->sub;
	if (mi->mask & 0x20) mi->dato = i->dato;
	if ((mi->mask & 0x50) && mi->texto && mi->cch)
		copia_n(mi->texto, i->texto ? i->texto : "", mi->cch + 1);
	if (mi->mask & 0x50)
		mi->cch = i->texto ? strlen(i->texto) : 0;
	return TRUE;
}
API(BOOL) SetMenuItemInfoA(HMENU h, u32 cual, BOOL pos, const MENUITEMINFOA *mi)
{
	Item *i = item(menu(h), cual, pos ? MF_BYPOSITION : 0, 0, 0);
	if (!i)
		return FALSE;
	if (mi->mask & 0x110) i->fl = (i->fl & ~(MF_SEPARATOR | MF_BITMAP | MF_OWNERDRAW | MF_RADIO)) | (mi->tipo & (MF_SEPARATOR | MF_BITMAP | MF_OWNERDRAW | MF_RADIO));
	if (mi->mask & 1) i->fl = (i->fl & ~(3 | MF_CHECKED | MF_DEFAULT)) | (mi->estado & (3 | MF_CHECKED | MF_DEFAULT));
	if (mi->mask & 2) i->id = mi->id;
	if (mi->mask & 4) i->sub = menu(mi->sub);
	if (mi->mask & 0x20) i->dato = mi->dato;
	if ((mi->mask & 0x40) || ((mi->mask & 0x10) && !(mi->tipo & (MF_SEPARATOR | MF_BITMAP | MF_OWNERDRAW))))
		pon_texto(i, mi->texto);
	return TRUE;
}
API(BOOL) InsertMenuItemA(HMENU h, u32 cual, BOOL pos, const MENUITEMINFOA *mi)
{
	Menu *m = menu(h), *de = m;
	i32 sitio = -1;
	u32 fl = 0;
	if (!m)
		return FALSE;
	if (pos)
		sitio = cual;
	else if (item(m, cual, 0, &de, &sitio))
		;
	if (mi->mask & 0x110) fl |= mi->tipo & (MF_SEPARATOR | MF_BITMAP | MF_OWNERDRAW | MF_RADIO);
	if (mi->mask & 1) fl |= mi->estado & (3 | MF_CHECKED | MF_DEFAULT);
	if ((mi->mask & 4) && mi->sub)
		fl |= MF_POPUP;
	meter(de, sitio, fl, (mi->mask & 4) && mi->sub ? (u32)mi->sub : (mi->mask & 2) ? mi->id : 0,
		(mi->mask & 0x50) && !(fl & (MF_SEPARATOR | MF_BITMAP | MF_OWNERDRAW)) ? mi->texto : 0);
	if (mi->mask & 0x20) {
		Item *i = item(de, sitio < 0 ? (u32)de->n - 1 : (u32)sitio, MF_BYPOSITION, 0, 0);
		if (i)
			i->dato = mi->dato;
	}
	return TRUE;
}
API(BOOL) SetMenuDefaultItem(HMENU h, u32 i, u32 pos) { Menu *m = menu(h); (void)pos; if (m) m->def = i; return m != 0; }
API(u32) GetMenuDefaultItem(HMENU h, u32 pos, u32 fl) { Menu *m = menu(h); (void)pos; (void)fl; return m ? m->def : 0xFFFFFFFF; }
API(BOOL) HiliteMenuItem(HWND w, HMENU h, u32 i, u32 f) { (void)w; (void)h; (void)i; (void)f; return TRUE; }
API(BOOL) SetMenuItemBitmaps(HMENU h, u32 p, u32 f, HBITMAP a, HBITMAP b) { (void)h; (void)p; (void)f; (void)a; (void)b; return TRUE; }
API(u32) GetMenuCheckMarkDimensions(void) { return MAKELONG(13, 13); }
API(BOOL) SetMenuInfo(HMENU h, const void *i) { (void)h; (void)i; return TRUE; }
API(BOOL) GetMenuInfo(HMENU h, void *i) { (void)h; (void)i; return TRUE; }
API(HMENU) GetMenu(HWND h) { Wnd *w = wnd(h); return w && !(w->estilo & WS_CHILD) ? w->menu : 0; }
API(BOOL) SetMenu(HWND h, HMENU m)
{
	Wnd *w = wnd(h);
	RECT d;
	if (!w || (w->estilo & WS_CHILD))
		return FALSE;
	w->menu = m;
	if (menu(m))
		menu(m)->barra = 1;
	w->nc_sucio = 1;
	invalidar(w, 0, 1);
	lo_de_dentro(w, &d);
	SendMessageA(h, WM_SIZE, 0, MAKELONG(d.right, d.bottom));
	return TRUE;
}
API(BOOL) DrawMenuBar(HWND h) { Wnd *w = wnd(h); if (w) { w->nc_sucio = 1; menu_barra_pintar(w); } return w != 0; }
API(HMENU) GetSystemMenu(HWND h, BOOL r)
{
	static Menu *sistema;
	(void)h; (void)r;
	if (!sistema) {
		sistema = nuevo_menu();
		meter(sistema, -1, 0, 0xF120, "&Restaurar");
		meter(sistema, -1, 0, 0xF020, "Mi&nimizar");
		meter(sistema, -1, 0, 0xF030, "Ma&ximizar");
		meter(sistema, -1, MF_SEPARATOR, 0, 0);
		meter(sistema, -1, 0, 0xF060, "&Cerrar\tAlt+F4");
	}
	return (HMENU)sistema;
}
API(BOOL) GetMenuItemRect(HWND h, HMENU m, u32 i, RECT *r) { (void)h; (void)m; (void)i; SetRectEmpty_(r); return FALSE; }
void SetRectEmpty_(RECT *r) { r->left = r->top = r->right = r->bottom = 0; }

/* ---- de un recurso (MENU o MENUEX) ---- */
static const u8 *uno_de_recurso(Menu *m, const u8 *p, int ex)
{
	for (;;) {
		if (ex) {
			u32 tipo = *(const u32 *)p, estado = *(const u32 *)(p + 4), id = *(const u32 *)(p + 8);
			u16 fl = *(const u16 *)(p + 12);
			char t[256];
			const u16 *s = (const u16 *)(p + 14);
			u32 k;
			for (k = 0; s[k] && k < 255; k++)
				t[k] = de_uni(s[k]);
			t[k] = 0;
			p = (const u8 *)(s + k + 1);
			p = (const u8 *)(((u32)p + 3) & ~3u);
			if (fl & 1) {
				Menu *sub = nuevo_menu();
				p += 4;                /* (la ayuda) */
				p = uno_de_recurso(sub, p, 1);
				meter(m, -1, MF_POPUP | (estado & (3 | MF_CHECKED)) | (tipo & (MF_SEPARATOR | MF_RADIO)), (u32)sub, t);
			} else
				meter(m, -1, (estado & (3 | MF_CHECKED | MF_DEFAULT)) | (tipo & (MF_SEPARATOR | MF_RADIO | 0x60)), id, (tipo & MF_SEPARATOR) ? 0 : t);
			if (fl & 0x80)
				return p;
		} else {
			u16 fl = *(const u16 *)p, id = 0;
			char t[256];
			const u16 *s;
			u32 k;
			p += 2;
			if (!(fl & MF_POPUP)) {
				id = *(const u16 *)p;
				p += 2;
			}
			s = (const u16 *)p;
			for (k = 0; s[k] && k < 255; k++)
				t[k] = de_uni(s[k]);
			t[k] = 0;
			p = (const u8 *)(s + k + 1);
			if (fl & MF_POPUP) {
				Menu *sub = nuevo_menu();
				p = uno_de_recurso(sub, p, 0);
				meter(m, -1, (fl & ~0x80) | MF_POPUP, (u32)sub, t);
			} else
				meter(m, -1, (fl & ~0x80) | (!id && !t[0] ? MF_SEPARATOR : 0), id, t[0] ? t : 0);
			if (fl & 0x80)
				return p;
		}
	}
}
HMENU menu_de_recurso(const u8 *p, int ex, const u8 **fin)
{
	Menu *m = nuevo_menu();
	const u8 *f;
	(void)ex;
	if (*(const u16 *)p == 1) {
		u16 off = *(const u16 *)(p + 2);
		f = uno_de_recurso(m, p + 4 + off, 1);
	} else
		f = uno_de_recurso(m, p + 4 + *(const u16 *)(p + 2), 0);
	if (fin)
		*fin = f;
	return (HMENU)m;
}
API(HMENU) LoadMenuIndirectA(const void *p) { return menu_de_recurso(p, 0, 0); }
API(HMENU) LoadMenuIndirectW(const void *p) { return menu_de_recurso(p, 0, 0); }
API(HMENU) LoadMenuA(HINSTANCE h, const char *n)
{
	u32 tam;
	const u8 *p = recurso(h, (const char *)4, n, &tam);
	if (!p) {
		pon_error(1814);
		return 0;
	}
	return menu_de_recurso(p, 0, 0);
}
API(HMENU) LoadMenuW(HINSTANCE h, const WCHAR *n)
{
	char b[64];
	if ((u32)n < 0x10000)
		return LoadMenuA(h, (const char *)n);
	de_w(b, n, sizeof b);
	return LoadMenuA(h, b);
}

/* ---- la barra ---- */
i32 alto_menu(Wnd *w) { return w && !(w->estilo & WS_CHILD) && menu(w->menu) ? ALTO_BARRA : 0; }
static void texto_limpio(const char *t, char *d, u32 cabe, char *atajo)
{
	u32 o = 0;
	if (atajo)
		*atajo = 0;
	for (; t && *t && o + 1 < cabe; t++) {
		if (*t == '&') {
			if (t[1] == '&') { d[o++] = '&'; t++; continue; }
			if (atajo && t[1])
				*atajo = mayus(t[1]);
			continue;
		}
		if (*t == '\t')
			break;
		d[o++] = *t;
	}
	d[o] = 0;
}
static i32 ancho_item(Item *i)
{
	char t[128];
	texto_limpio(i->texto, t, sizeof t, 0);
	return strlen(t) * 8 + 16;
}
static Wnd *barra_abierta;
static i32 barra_item = -1;
void menu_barra_pintar(Wnd *w)
{
	Menu *m = menu(w->menu);
	HDC d;
	RECT r;
	POINT o, v;
	i32 x, k;
	if (!m || !ventana_visible(w))
		return;
	dentro_en_lienzo(w, &o);
	ventana_en_lienzo(w, &v);
	d = dc_de_ventana(w, 0, 0);
	r.left = o.x - v.x - 0;
	r.top = o.y - v.y - ALTO_BARRA;
	r.right = r.left + (w->lienzo.an ? w->lienzo.an : 2000);
	r.bottom = o.y - v.y;
	r.left = 0;
	FillRect(d, &r, GetSysColorBrush(30));
	{
		RECT l = { r.left, r.bottom - 1, r.right, r.bottom };
		FillRect(d, &l, GetSysColorBrush(COLOR_BTNSHADOW));
	}
	SetBkMode(d, 1);
	x = r.left + 4 - (o.x - v.x) + (o.x - v.x);
	for (k = 0; k < m->n; k++) {
		char t[128], a;
		RECT c;
		i32 an = ancho_item(&m->it[k]);
		texto_limpio(m->it[k].texto, t, sizeof t, &a);
		c.left = x; c.top = r.top + 1; c.right = x + an; c.bottom = r.bottom - 2;
		if (barra_abierta == w && barra_item == k) {
			FillRect(d, &c, GetSysColorBrush(COLOR_HIGHLIGHT));
			SetTextColor(d, GetSysColor(COLOR_HIGHLIGHTTEXT));
		} else
			SetTextColor(d, GetSysColor(m->it[k].fl & MF_GRAYED ? COLOR_GRAYTEXT : COLOR_MENUTEXT));
		TextOutA(d, x + 8, r.top + 2, t, strlen(t));
		x += an;
	}
	ensuciar(raiz_de(w), &dc(d)->tope);
	dc_soltar(d);
}

/* ---- abrir un menú (el emergente del núcleo) ---- */
static struct { Menu *m; Wnd *w; i32 x, y; int devolver, hecho; u32 res; void (*f)(Wnd *, u32); } abierto;
static u32 etiqueta_n = 0x57000;
int menu_abierto(void) { return abierto.m != 0 || abierto.f != 0; }
/* una lista de opciones (en la página del núcleo, separadas por «|») como
 * menú emergente; lo elegido, a f */
void lista_emergente(Wnd *w, i32 x, i32 y, const char *ops, void (*f)(Wnd *, u32))
{
	Wnd *raiz = raiz_de(w);
	if (!raiz || !raiz->vn)
		return;
	abierto.m = 0;
	abierto.w = w;
	abierto.f = f;
	sys_menu(raiz->vn, x, y, ops, 0, ++etiqueta_n);
}
static void abrir(Wnd *w, Menu *m, i32 x, i32 y)
{
	char op[256];
	u32 o = 0, gris = 0;
	i32 k, hay_marca = 0;
	Wnd *raiz = raiz_de(w);
	if (!raiz || !raiz->vn || !m)
		return;
	SendMessageA((HWND)w, WM_INITMENUPOPUP, (WPARAM)m, 0);
	for (k = 0; k < m->n; k++)
		if (m->it[k].fl & MF_CHECKED)
			hay_marca = 1;
	for (k = 0; k < m->n && o < sizeof op - 4; k++) {
		Item *i = &m->it[k];
		char t[128], b[160];
		u32 l;
		if (k)
			op[o++] = '|';
		if (i->fl & MF_SEPARATOR) {
			op[o++] = '-';
			continue;
		}
		texto_limpio(i->texto ? i->texto : "", t, sizeof t, 0);
		{
			/* (lo de detrás del tabulador: el atajo) */
			const char *tab = i->texto ? strchr(i->texto, '\t') : 0;
			fmt(b, sizeof b, "%s%s%s%s%s", hay_marca ? (i->fl & MF_CHECKED ? "\xFB " : "  ") : "", t, tab ? "  " : "", tab ? tab + 1 : "", i->sub ? " \x10" : "");
		}
		/* a la página del núcleo, sin «|» */
		for (l = 0; b[l] && o < sizeof op - 2; l++) {
			u8 c = (u8)b[l];
			if (c == '|')
				c = '/';
			op[o++] = (c == 0xFB || c == 0x10) && (u8)b[l] == c ? c : a437c(c);
		}
		if ((i->fl & (MF_GRAYED | MF_DISABLED)) && k < 32)
			gris |= 1u << k;
	}
	op[o] = 0;
	abierto.m = m;
	abierto.w = w;
	abierto.x = x;
	abierto.y = y;
	sys_menu(raiz->vn, x, y, op, gris, ++etiqueta_n);
}
/* ha llegado lo elegido */
void menu_suceso(Wnd *raiz, u32 op, u32 etiqueta)
{
	Menu *m = abierto.m;
	Wnd *w = abierto.w;
	(void)raiz;
	if (etiqueta == etiqueta_n && abierto.f) {
		/* (una lista: la de un COMBOBOX) */
		void (*f)(Wnd *, u32) = abierto.f;
		abierto.f = 0;
		abierto.m = 0;
		f(w, op);
		return;
	}
	if (etiqueta != etiqueta_n || !m) {
		return;
	}
	abierto.m = 0;
	if (op == 0xFFFFFFFF || op >= (u32)m->n) {
		if (barra_abierta) {
			Wnd *b = barra_abierta;
			barra_abierta = 0;
			barra_item = -1;
			menu_barra_pintar(b);
			SendMessageA((HWND)b, WM_EXITMENULOOP, 0, 0);
		}
		abierto.hecho = 1;
		abierto.res = 0;
		return;
	}
	if (m->it[op].sub) {
		abrir(w, m->it[op].sub, abierto.x + 24, abierto.y + 8 + op * 4);
		return;
	}
	if (barra_abierta) {
		Wnd *b = barra_abierta;
		barra_abierta = 0;
		barra_item = -1;
		menu_barra_pintar(b);
		SendMessageA((HWND)b, WM_EXITMENULOOP, 0, 0);
	}
	if (m->it[op].fl & (MF_GRAYED | MF_DISABLED))
		return;
	abierto.hecho = 1;
	abierto.res = m->it[op].id;
	if (abierto.devolver)
		return;
	if (m->it[op].id >= 0xF000)
		PostMessageA((HWND)w, WM_SYSCOMMAND, m->it[op].id, 0);
	else
		PostMessageA((HWND)w, WM_COMMAND, m->it[op].id, 0);
}
/* un clic en la barra (x, y en el lienzo) */
int menu_barra_clic(Wnd *w, i32 x, i32 y)
{
	Menu *m = menu(w->menu);
	i32 k, px = 4;
	POINT o;
	(void)y;
	if (!m)
		return 0;
	dentro_en_lienzo(w, &o);
	for (k = 0; k < m->n; k++) {
		i32 an = ancho_item(&m->it[k]);
		if (x >= px && x < px + an) {
			if (m->it[k].fl & (MF_GRAYED | MF_DISABLED))
				return 1;
			SendMessageA((HWND)w, WM_ENTERMENULOOP, 0, 0);
			SendMessageA((HWND)w, WM_INITMENU, (WPARAM)m, 0);
			if (m->it[k].sub) {
				barra_abierta = w;
				barra_item = k;
				menu_barra_pintar(w);
				abrir(w, m->it[k].sub, px, o.y);
			} else
				PostMessageA((HWND)w, WM_COMMAND, m->it[k].id, 0);
			return 1;
		}
		px += an;
	}
	return 0;
}
/* F10 o Alt+letra: el menú de esa letra (0: el primero) */
int menu_tecla(Wnd *w, u32 letra, int alt)
{
	Menu *m = w ? menu(w->menu) : 0;
	i32 k, px = 4;
	POINT o;
	(void)alt;
	if (!m || !m->n)
		return 0;
	dentro_en_lienzo(w, &o);
	for (k = 0; k < m->n; k++) {
		char t[128], a;
		i32 an = ancho_item(&m->it[k]);
		texto_limpio(m->it[k].texto, t, sizeof t, &a);
		if (!letra || a == (char)mayus(letra)) {
			if (m->it[k].sub) {
				SendMessageA((HWND)w, WM_INITMENU, (WPARAM)m, 0);
				barra_abierta = w;
				barra_item = k;
				menu_barra_pintar(w);
				abrir(w, m->it[k].sub, px, o.y);
			} else
				PostMessageA((HWND)w, WM_COMMAND, m->it[k].id, 0);
			return 1;
		}
		px += an;
	}
	return 0;
}
API(BOOL) TrackPopupMenuEx(HMENU h, u32 fl, i32 x, i32 y, HWND hw, void *tpm)
{
	Wnd *w = wnd(hw);
	Menu *m = menu(h);
	POINT p = { x, y };
	(void)tpm;
	if (!w || !m)
		return FALSE;
	ScreenToClient((HWND)raiz_de(w), &p);
	{
		POINT o;
		dentro_en_lienzo(raiz_de(w), &o);
		p.x += o.x;
		p.y += o.y;
	}
	abierto.devolver = (fl & 0x100) != 0;        /* TPM_RETURNCMD */
	abierto.hecho = 0;
	abrir(w, m, p.x, p.y);
	if (abierto.devolver || 1) {
		/* espera a que se elija (los mensajes siguen) */
		MSG msg;
		while (!abierto.hecho && abierto.m) {
			if (PeekMessageA(&msg, 0, 0, 0, 1)) {
				TranslateMessage(&msg);
				DispatchMessageA(&msg);
			} else
				sys_dormir(1);
		}
		if (abierto.devolver) {
			abierto.devolver = 0;
			return abierto.res;
		}
	}
	return TRUE;
}
API(BOOL) TrackPopupMenu(HMENU h, u32 fl, i32 x, i32 y, i32 r, HWND hw, const RECT *re) { (void)r; (void)re; return TrackPopupMenuEx(h, fl, x, y, hw, 0); }
API(BOOL) EndMenu(void) { abierto.m = 0; return TRUE; }
