/* controles.c - WIN32.BIN: los controles de serie de USER32: BUTTON (botón,
 * casilla, botón de opción, cuadro de grupo, el que pinta el programa),
 * STATIC, EDIT (de una línea y de varias), LISTBOX, COMBOBOX (la lista
 * desplegable es el menú emergente del núcleo) y SCROLLBAR, y las barras de
 * desplazamiento de las ventanas. Con los mensajes y los avisos de Windows.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "ventanas.h"
#include "k32.h"

#define API(t) t WINAPI
static void notificar(Wnd *w, u32 codigo)
{
	if (w->padre && w->padre != escritorio)
		SendMessageA((HWND)w->padre, WM_COMMAND, MAKELONG((u32)w->menu, codigo), (LPARAM)w);
}
typedef struct { u32 tipo, id, item, accion, estado; HWND h; HDC dc; RECT r; u32 dato; } DRAWITEMSTRUCT;
/* la brocha y los colores que da la madre (WM_CTLCOLOR...) */
static HBRUSH colores(Wnd *w, HDC d, u32 m)
{
	HBRUSH b = 0;
	if (w->padre && w->padre != escritorio)
		b = (HBRUSH)SendMessageA((HWND)w->padre, m, (WPARAM)d, (LPARAM)w);
	if (!b)
		b = (HBRUSH)DefWindowProcA((HWND)w, m, (WPARAM)d, (LPARAM)w);
	return b;
}
static HFONT letra_de(Wnd *w) { return (HFONT)GetPropA((HWND)w, "#letra"); }
static HDC dc_control(Wnd *w, PAINTSTRUCT_ *ps);

/* ---------------------------------------------------------------------------
 *  Las barras de desplazamiento
 * ------------------------------------------------------------------------- */
static void flecha(HDC d, RECT r, int dir, int activa)
{
	i32 cx = (r.left + r.right) / 2, cy = (r.top + r.bottom) / 2, k;
	HBRUSH b = GetSysColorBrush(activa ? COLOR_BTNTEXT : COLOR_GRAYTEXT);
	for (k = 0; k < 4; k++) {
		RECT l;
		switch (dir) {
		case 0: l.left = cx - k; l.right = cx + k + 1; l.top = cy - 2 + k; l.bottom = l.top + 1; break;     /* arriba */
		case 1: l.left = cx - k; l.right = cx + k + 1; l.top = cy + 1 - k; l.bottom = l.top + 1; break;     /* abajo */
		case 2: l.top = cy - k; l.bottom = cy + k + 1; l.left = cx - 2 + k; l.right = l.left + 1; break;    /* izquierda */
		default: l.top = cy - k; l.bottom = cy + k + 1; l.left = cx + 1 - k; l.right = l.left + 1; break;  /* derecha */
		}
		FillRect(d, &l, b);
	}
}
/* dónde va el botón que se arrastra */
static void pulgar(RECT r, Barra *b, int vertical, i32 *ini, i32 *lar)
{
	i32 largo = vertical ? r.bottom - r.top : r.right - r.left, zona = largo - 32, rango = b->max - b->min + 1, t;
	if (zona < 8 || rango <= 0) {
		*ini = *lar = 0;
		return;
	}
	t = b->pag ? (i32)((i64)zona * b->pag / rango) : 16;
	if (t < 12) t = 12;
	if (t > zona) t = zona;
	{
		i32 recorrido = rango - (b->pag ? (i32)b->pag : 1);
		*ini = 16 + (recorrido > 0 ? (i32)((i64)(zona - t) * (b->pos - b->min) / recorrido) : 0);
	}
	*lar = t;
}
void barra_pintar(HDC d, RECT r, Barra *b, int vertical, int activa)
{
	RECT a, z;
	i32 ini, lar;
	FillRect(d, &r, GetSysColorBrush(COLOR_SCROLLBAR));
	a = r;
	if (vertical) a.bottom = a.top + 16; else a.right = a.left + 16;
	relleno_3d(d, a, 0);
	flecha(d, a, vertical ? 0 : 2, activa);
	a = r;
	if (vertical) a.top = a.bottom - 16; else a.left = a.right - 16;
	relleno_3d(d, a, 0);
	flecha(d, a, vertical ? 1 : 3, activa);
	pulgar(r, b, vertical, &ini, &lar);
	if (lar && activa && b->max > b->min) {
		z = r;
		if (vertical) { z.top = r.top + ini; z.bottom = z.top + lar; }
		else { z.left = r.left + ini; z.right = z.left + lar; }
		relleno_3d(d, z, 0);
	}
}
static void mandar(u32 msg, HWND a_quien, HWND ctl, u32 codigo, i32 pos)
{
	SendMessageA(a_quien, msg, MAKELONG(codigo, pos), (LPARAM)ctl);
}
/* un clic en la barra (en el lienzo; r también en el lienzo): sigue al
 * ratón (mirando los sucesos del núcleo) hasta que se suelta */
int barra_clic(Wnd *w, Barra *b, RECT r, int vertical, i32 x, i32 y, u32 msg, HWND a_quien, HWND ctl)
{
	Wnd *raiz = raiz_de(w);
	i32 p = vertical ? y - r.top : x - r.left, largo = vertical ? r.bottom - r.top : r.right - r.left, ini, lar, codigo;
	u32 hasta;
	int arrastra = 0;
	i32 desde = 0, pos0 = b->pos;
	pulgar(r, b, vertical, &ini, &lar);
	if (p < 16) codigo = 0;                       /* SB_LINEUP */
	else if (p >= largo - 16) codigo = 1;         /* SB_LINEDOWN */
	else if (lar && p < ini) codigo = 2;          /* SB_PAGEUP */
	else if (lar && p >= ini + lar) codigo = 3;   /* SB_PAGEDOWN */
	else { codigo = 5; arrastra = 1; desde = p; }
	if (!arrastra)
		mandar(msg, a_quien, ctl, codigo, 0);
	hasta = GetTickCount_() + 350;
	for (;;) {
		Suceso s;
		int fin = 0;
		while (raiz->vn && sys_suceso(raiz->vn, &s) == 1) {
			if (s.clase == EV_SUBE && !(s.c & 1)) {
				fin = 1;
				break;
			}
			if (s.clase == EV_MUEVE && arrastra) {
				i32 q = (vertical ? (i32)s.b - r.top : (i32)s.a - r.left) - desde;
				i32 zona = largo - 32 - lar, rango = b->max - b->min + 1 - (b->pag ? (i32)b->pag : 1);
				i32 np = pos0 + (zona > 0 ? (i32)((i64)q * rango / zona) : 0);
				if (np < b->min) np = b->min;
				if (np > b->min + rango) np = b->min + rango;
				b->track = np;
				mandar(msg, a_quien, ctl, 5, np);       /* SB_THUMBTRACK */
				presentar_todo();
			}
		}
		if (fin || !raiz->vn)
			break;
		if (!arrastra && GetTickCount_() >= hasta) {
			mandar(msg, a_quien, ctl, codigo, 0);
			hasta = GetTickCount_() + 60;
		}
		presentar_todo();
		sys_dormir(1);
	}
	if (arrastra)
		mandar(msg, a_quien, ctl, 4, b->track);         /* SB_THUMBPOSITION */
	mandar(msg, a_quien, ctl, 8, 0);                    /* SB_ENDSCROLL */
	return 1;
}

/* ---------------------------------------------------------------------------
 *  BUTTON
 * ------------------------------------------------------------------------- */
typedef struct { int marca, pulsado, raton, foco; HANDLE imagen; } Boton;
static Boton *boton(Wnd *w)
{
	if (!w->ctl)
		w->ctl = m_cero(sizeof(Boton));
	return w->ctl;
}
static u32 tipo_boton(Wnd *w) { return w->estilo & 15; }
static void pinta_boton(Wnd *w, HDC d)
{
	Boton *b = boton(w);
	RECT r;
	u32 t = tipo_boton(w);
	char txt[256];
	HBRUSH fondo;
	GetClientRect((HWND)w, &r);
	GetWindowTextA((HWND)w, txt, sizeof txt);
	if (letra_de(w))
		SelectObject(d, letra_de(w));
	SetBkMode(d, 1);
	if (t == 0xB) {                                       /* BS_OWNERDRAW */
		DRAWITEMSTRUCT di;
		memset(&di, 0, sizeof di);
		di.tipo = 4;
		di.id = (u32)w->menu;
		di.accion = 1;
		di.estado = (b->pulsado ? 1 : 0) | (IsWindowEnabled((HWND)w) ? 0 : 4) | (b->foco ? 0x10 : 0);
		di.h = (HWND)w;
		di.dc = d;
		di.r = r;
		SendMessageA((HWND)w->padre, WM_DRAWITEM, (WPARAM)w->menu, (LPARAM)&di);
		return;
	}
	fondo = colores(w, d, t == 7 || t == 2 || t == 3 || t == 4 || t == 9 || t == 5 || t == 6 ? WM_CTLCOLORSTATIC : WM_CTLCOLORBTN);
	if (t == 7) {                                         /* BS_GROUPBOX */
		RECT m = r;
		SIZE s;
		GetTextExtentPoint32A(d, txt, strlen(txt), &s);
		m.top += 8;
		DrawEdge(d, &m, 0x06, 0x0F);
		if (txt[0]) {
			RECT f = { 8, 0, 8 + s.cx + 8, 16 };
			FillRect(d, &f, fondo);
			SetTextColor(d, GetSysColor(IsWindowEnabled((HWND)w) ? COLOR_BTNTEXT : COLOR_GRAYTEXT));
			{
				RECT tr = { 12, 0, 12 + s.cx, 16 };
				DrawTextA(d, txt, -1, &tr, 0x24);
			}
		}
		return;
	}
	if (t == 0 || t == 1 || t == 0xA || (w->estilo & 0x1000)) {   /* botón (o BS_PUSHLIKE) */
		int hundido = b->pulsado || ((w->estilo & 0x1000) && b->marca);
		FillRect(d, &r, GetSysColorBrush(COLOR_BTNFACE));
		{
			RECT m = r;
			if (t == 1 || (b->foco && t != 0xA)) {
				FrameRect(d, &m, GetSysColorBrush(COLOR_WINDOWFRAME));
				InflateRect_(&m, -1, -1);
			}
			DrawEdge(d, &m, hundido ? 0x0A : 0x05, 0x0F);
		}
		SetTextColor(d, GetSysColor(IsWindowEnabled((HWND)w) ? COLOR_BTNTEXT : COLOR_GRAYTEXT));
		{
			RECT tr = r;
			if (hundido) { tr.left += 2; tr.top += 2; }
			if (b->imagen && (w->estilo & 0x40))
				dibuja_icono(d, (r.right - 32) / 2, (r.bottom - 32) / 2, b->imagen, 32, 32);
			else
				DrawTextA(d, txt, -1, &tr, 0x25);      /* DT_CENTER | DT_VCENTER | DT_SINGLELINE */
		}
		if (b->foco) {
			RECT f = r;
			InflateRect_(&f, -4, -4);
			DrawFocusRect(d, &f);
		}
		return;
	}
	/* casillas y botones de opción */
	FillRect(d, &r, fondo);
	{
		i32 cy = (r.bottom - r.top - 13) / 2, tx;
		RECT c = { 0, cy, 13, cy + 13 };
		int izq = (w->estilo & 0x20) != 0;
		if (izq) { c.left = r.right - 13; c.right = r.right; }
		if (t == 4 || t == 9) {
			/* el redondo */
			HBRUSH bl = GetSysColorBrush(COLOR_WINDOW);
			HPEN pl = CreatePen(0, 1, GetSysColor(COLOR_BTNSHADOW));
			HGDIOBJ vp = SelectObject(d, pl), vb = SelectObject(d, bl);
			extern BOOL WINAPI Ellipse(HDC, i32, i32, i32, i32);
			Ellipse(d, c.left, c.top, c.right, c.bottom);
			if (b->marca) {
				HBRUSH n = GetSysColorBrush(COLOR_BTNTEXT);
				SelectObject(d, n);
				Ellipse(d, c.left + 4, c.top + 4, c.right - 4, c.bottom - 4);
			}
			SelectObject(d, vp);
			SelectObject(d, vb);
			DeleteObject(pl);
		} else {
			FillRect(d, &c, GetSysColorBrush(COLOR_WINDOW));
			DrawEdge(d, &c, 0x0A, 0x0F);
			if (b->marca) {
				i32 k;
				HBRUSH n = GetSysColorBrush(b->marca == 2 ? COLOR_GRAYTEXT : COLOR_BTNTEXT);
				for (k = 0; k < 7; k++) {
					i32 yy = k < 3 ? c.top + 5 + k : c.top + 7 + 2 - (k - 3);
					RECT p = { c.left + 3 + k, yy, c.left + 4 + k, yy + 3 };
					FillRect(d, &p, n);
				}
			}
		}
		SetTextColor(d, GetSysColor(IsWindowEnabled((HWND)w) ? COLOR_BTNTEXT : COLOR_GRAYTEXT));
		tx = izq ? 0 : 18;
		{
			RECT tr = { tx, 0, izq ? r.right - 18 : r.right, r.bottom };
			DrawTextA(d, txt, -1, &tr, 0x24 | (izq ? 2 : 0));
			if (b->foco) {
				SIZE s;
				RECT f;
				GetTextExtentPoint32A(d, txt, strlen(txt), &s);
				f.left = tx - 1; f.top = (r.bottom - 16) / 2; f.right = tx + s.cx + 1; f.bottom = f.top + 16;
				DrawFocusRect(d, &f);
			}
		}
	}
}
static void radio_grupo(Wnd *w)
{
	/* los de su grupo: de la anterior con WS_GROUP a la siguiente */
	Wnd *p = w->padre, *x, *ini = p->hijas;
	for (x = p->hijas; x && x != w; x = x->sig)
		if (x->estilo & WS_GROUP)
			ini = x;
	for (x = ini; x; x = x->sig) {
		if (x != ini && (x->estilo & WS_GROUP))
			break;
		if (x != w && x->clase == w->clase && tipo_boton(x) == 9 && boton(x)->marca) {
			boton(x)->marca = 0;
			InvalidateRect((HWND)x, 0, TRUE);
		}
	}
}
static void clic_boton(Wnd *w)
{
	Boton *b = boton(w);
	u32 t = tipo_boton(w);
	if (t == 3) b->marca = !b->marca;
	else if (t == 6) b->marca = (b->marca + 1) % 3;
	else if (t == 9) {
		if (!b->marca) {
			b->marca = 1;
			radio_grupo(w);
		}
	}
	InvalidateRect((HWND)w, 0, TRUE);
	notificar(w, 0);                                      /* BN_CLICKED */
}
API(LRESULT) proc_boton(HWND h, u32 m, WPARAM wp, LPARAM lp)
{
	Wnd *w = wnd(h);
	Boton *b;
	if (!w)
		return 0;
	b = boton(w);
	switch (m) {
	case WM_PAINT: {
		PAINTSTRUCT_ ps;
		HDC d = dc_control(w, &ps);
		pinta_boton(w, d);
		EndPaint(h, (void *)&ps);
		return 0;
	}
	case WM_ERASEBKGND:
		return 1;
	case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK:
		if (tipo_boton(w) == 7)
			return 0;
		b->pulsado = 1;
		b->raton = 1;
		SetCapture(h);
		SetFocus(h);
		InvalidateRect(h, 0, TRUE);
		return 0;
	case WM_MOUSEMOVE:
		if (b->raton) {
			RECT r;
			POINT p = { (i16)LOWORD(lp), (i16)HIWORD(lp) };
			int dentro;
			GetClientRect(h, &r);
			dentro = p.x >= 0 && p.y >= 0 && p.x < r.right && p.y < r.bottom;
			if (dentro != b->pulsado) {
				b->pulsado = dentro;
				InvalidateRect(h, 0, TRUE);
			}
		}
		return 0;
	case WM_LBUTTONUP:
		if (b->raton) {
			int dentro = b->pulsado;
			b->raton = 0;
			b->pulsado = 0;
			ReleaseCapture();
			InvalidateRect(h, 0, TRUE);
			if (dentro)
				clic_boton(w);
		}
		return 0;
	case WM_KEYDOWN:
		if (wp == ' ' && !b->pulsado) {
			b->pulsado = 1;
			InvalidateRect(h, 0, TRUE);
			return 0;
		}
		/* (como en Hispa Ventanas: lo que el botón no quiere, a su madre si no es un cuadro) */
		if (w->padre && w->padre != escritorio && !w->padre->es_dialogo)
			return SendMessageA((HWND)w->padre, m, wp, lp);
		return 0;
	case WM_KEYUP:
		if (wp == ' ' && b->pulsado) {
			b->pulsado = 0;
			InvalidateRect(h, 0, TRUE);
			clic_boton(w);
		}
		return 0;
	case WM_CHAR:
		if (wp == '\r' && (tipo_boton(w) == 0 || tipo_boton(w) == 1) && w->padre && !w->padre->es_dialogo) {
			clic_boton(w);                                /* (Intro, como en Hispa Ventanas: lo pulsa) */
			return 0;
		}
		if (wp != ' ' && w->padre && w->padre != escritorio && !w->padre->es_dialogo)
			return SendMessageA((HWND)w->padre, m, wp, lp);
		return 0;
	case WM_SETFOCUS:
		b->foco = 1;
		InvalidateRect(h, 0, TRUE);
		if (tipo_boton(w) == 9 && !b->marca && 0)
			clic_boton(w);
		return 0;
	case WM_KILLFOCUS:
		b->foco = 0;
		if (b->raton) {
			b->raton = 0;
			b->pulsado = 0;
			ReleaseCapture();
		}
		InvalidateRect(h, 0, TRUE);
		return 0;
	case WM_ENABLE:
		InvalidateRect(h, 0, TRUE);
		return 0;
	case WM_GETDLGCODE: {
		u32 t = tipo_boton(w);
		if (t == 1) return 0x2010;                        /* DLGC_BUTTON | DLGC_DEFPUSHBUTTON */
		if (t == 0) return 0x2020;                        /* DLGC_BUTTON | DLGC_UNDEFPUSHBUTTON */
		if (t == 4 || t == 9) return 0x2040;              /* DLGC_RADIOBUTTON */
		if (t == 7) return 0x2000 | 0x100;                /* DLGC_STATIC */
		return 0x2000;
	}
	case 0xF0: return b->marca;                           /* BM_GETCHECK */
	case 0xF1:                                            /* BM_SETCHECK */
		if (b->marca != (int)wp) {
			b->marca = wp;
			InvalidateRect(h, 0, TRUE);
		}
		return 0;
	case 0xF2: return b->marca | (b->pulsado ? 4 : 0) | (b->foco ? 8 : 0);
	case 0xF3: b->pulsado = wp != 0; InvalidateRect(h, 0, TRUE); return 0;
	case 0xF4:                                            /* BM_SETSTYLE */
		w->estilo = (w->estilo & ~0xFFFF) | (wp & 0xFFFF);
		if (lp)
			InvalidateRect(h, 0, TRUE);
		return 0;
	case 0xF5: clic_boton(w); return 0;                   /* BM_CLICK */
	case 0xF6: return (LRESULT)b->imagen;
	case 0xF7: { HANDLE a = b->imagen; b->imagen = (HANDLE)lp; InvalidateRect(h, 0, TRUE); return (LRESULT)a; }
	case WM_SETTEXT: {
		LRESULT r = DefWindowProcA(h, m, wp, lp);
		InvalidateRect(h, 0, TRUE);
		return r;
	}
	case WM_SETFONT:
		SetPropA(h, "#letra", (HANDLE)wp);
		if (lp)
			InvalidateRect(h, 0, TRUE);
		return 0;
	case WM_GETFONT:
		return (LRESULT)letra_de(w);
	}
	return DefWindowProcA(h, m, wp, lp);
}

/* ---------------------------------------------------------------------------
 *  STATIC
 * ------------------------------------------------------------------------- */
typedef struct { HANDLE imagen; } Estatico;
API(LRESULT) proc_estatico(HWND h, u32 m, WPARAM wp, LPARAM lp)
{
	Wnd *w = wnd(h);
	Estatico *e;
	if (!w)
		return 0;
	if (!w->ctl)
		w->ctl = m_cero(sizeof(Estatico));
	e = w->ctl;
	switch (m) {
	case WM_CREATE:
		if ((w->estilo & 0x1F) == 3 && w->texto && w->texto[0]) {
			extern HICON WINAPI LoadIconA(HINSTANCE, const char *);
			const char *n = w->texto;
			e->imagen = LoadIconA(w->inst, n[0] == '#' ? n : n);
		}
		return 0;
	case WM_PAINT: {
		PAINTSTRUCT_ ps;
		HDC d = dc_control(w, &ps);
		RECT r;
		u32 t = w->estilo & 0x1F;
		char txt[1024];
		HBRUSH b;
		GetClientRect(h, &r);
		if (letra_de(w))
			SelectObject(d, letra_de(w));
		b = colores(w, d, WM_CTLCOLORSTATIC);
		switch (t) {
		case 4: FillRect(d, &r, GetSysColorBrush(COLOR_WINDOWFRAME)); break;
		case 5: FillRect(d, &r, GetSysColorBrush(COLOR_BTNSHADOW)); break;
		case 6: FillRect(d, &r, GetSysColorBrush(COLOR_BTNHIGHLIGHT)); break;
		case 7: FrameRect(d, &r, GetSysColorBrush(COLOR_WINDOWFRAME)); break;
		case 8: FrameRect(d, &r, GetSysColorBrush(COLOR_BTNSHADOW)); break;
		case 9: FrameRect(d, &r, GetSysColorBrush(COLOR_BTNHIGHLIGHT)); break;
		case 0x10: { RECT l = { 0, 0, r.right, 2 }; DrawEdge(d, &l, 0x06, 0x0F); break; }
		case 0x11: { RECT l = { 0, 0, 2, r.bottom }; DrawEdge(d, &l, 0x06, 0x0F); break; }
		case 0x12: DrawEdge(d, &r, 0x06, 0x0F); break;
		case 3: case 0xE:
			FillRect(d, &r, b);
			if (e->imagen) {
				if (t == 3)
					dibuja_icono(d, (w->estilo & 0x200) ? (r.right - 32) / 2 : 0, (w->estilo & 0x200) ? (r.bottom - 32) / 2 : 0, e->imagen, 32, 32);
				else {
					HDC md = CreateCompatibleDC(d);
					extern i32 WINAPI GetObjectA(HGDIOBJ, i32, void *);
					extern BOOL WINAPI BitBlt(HDC, i32, i32, i32, i32, HDC, i32, i32, u32);
					extern BOOL WINAPI DeleteDC(HDC);
					extern HDC WINAPI CreateCompatibleDC(HDC);
					u32 bm[6];
					GetObjectA(e->imagen, sizeof bm, bm);
					SelectObject(md, e->imagen);
					BitBlt(d, 0, 0, bm[1], bm[2], md, 0, 0, 0x00CC0020);
					DeleteDC(md);
				}
			}
			break;
		case 0xD: {
			DRAWITEMSTRUCT di;
			memset(&di, 0, sizeof di);
			di.tipo = 5; di.id = (u32)w->menu; di.accion = 1; di.h = h; di.dc = d; di.r = r;
			SendMessageA((HWND)w->padre, WM_DRAWITEM, (WPARAM)w->menu, (LPARAM)&di);
			break;
		}
		default: {
			u32 f = 0;
			FillRect(d, &r, b);
			GetWindowTextA(h, txt, sizeof txt);
			SetBkMode(d, 1);
			if (!IsWindowEnabled(h))
				SetTextColor(d, GetSysColor(COLOR_GRAYTEXT));
			if (t == 1) f |= 1;
			else if (t == 2) f |= 2;
			if (t == 0 || t == 1 || t == 2) f |= 0x10;   /* DT_WORDBREAK */
			if (t == 0xB || t == 0xC) f |= 0x20;          /* (una línea) */
			if (w->estilo & 0x80) f |= 0x800;             /* SS_NOPREFIX */
			if (w->estilo & 0x200) f |= 4 | 0x20;         /* SS_CENTERIMAGE: centrado en alto */
			f |= 0x40;                                    /* DT_EXPANDTABS */
			DrawTextA(d, txt, -1, &r, f);
		}
		}
		if (w->estilo & 0x1000)
			DrawEdge(d, &r, 0x02, 0x0F);
		EndPaint(h, (void *)&ps);
		return 0;
	}
	case WM_ERASEBKGND:
		return 1;
	case WM_SETTEXT: {
		LRESULT r = DefWindowProcA(h, m, wp, lp);
		InvalidateRect(h, 0, TRUE);
		return r;
	}
	case WM_LBUTTONDOWN:
		if (w->estilo & 0x100)
			notificar(w, 0);                              /* STN_CLICKED */
		return 0;
	case WM_LBUTTONDBLCLK:
		if (w->estilo & 0x100)
			notificar(w, 1);
		return 0;
	case WM_NCHITTEST:
		return (w->estilo & 0x100) ? 1 : -1;
	case 0x170: case 0x172: {                             /* STM_SETICON, STM_SETIMAGE */
		HANDLE a = e->imagen;
		e->imagen = m == 0x170 ? (HANDLE)wp : (HANDLE)lp;
		InvalidateRect(h, 0, TRUE);
		return (LRESULT)a;
	}
	case 0x171: case 0x173:
		return (LRESULT)e->imagen;
	case WM_SETFONT:
		SetPropA(h, "#letra", (HANDLE)wp);
		if (lp)
			InvalidateRect(h, 0, TRUE);
		return 0;
	case WM_GETFONT:
		return (LRESULT)letra_de(w);
	case WM_ENABLE:
		InvalidateRect(h, 0, TRUE);
		return 0;
	case WM_GETDLGCODE:
		return 0x100;                                     /* DLGC_STATIC */
	}
	return DefWindowProcA(h, m, wp, lp);
}

/* el DC para pintar un control: BeginPaint (o el que da WM_PRINTCLIENT) */
static HDC dc_control(Wnd *w, PAINTSTRUCT_ *ps)
{
	return BeginPaint((HWND)w, (void *)ps);
}

/* ---------------------------------------------------------------------------
 *  EDIT
 * ------------------------------------------------------------------------- */
typedef struct {
	char *t;
	u32 n, cabe, limite;
	u32 ini, fin;               /* la selección: de ini a fin (fin: donde está el cursor) */
	i32 linea0, col0;           /* lo primero que se ve */
	int foco, raton, cambiado, solo_leer;
	char clave;
	char *deshacer;
	u32 margen_i, margen_d;
} Edit;
#define ES_CENTER 1
#define ES_RIGHT 2
#define ES_MULTILINE 4
#define ES_UPPERCASE 8
#define ES_LOWERCASE 0x10
#define ES_PASSWORD 0x20
#define ES_AUTOVSCROLL 0x40
#define ES_AUTOHSCROLL 0x80
#define ES_NOHIDESEL 0x100
#define ES_READONLY 0x800
#define ES_WANTRETURN 0x1000
#define ES_NUMBER 0x2000
static Edit *edit(Wnd *w)
{
	if (!w->ctl) {
		Edit *e = m_cero(sizeof(Edit));
		e->cabe = 64;
		e->t = m_cero(e->cabe);
		e->limite = (w->estilo & ES_MULTILINE) ? 0x7FFFFFFE : 30000;
		e->solo_leer = (w->estilo & ES_READONLY) != 0;
		if (w->estilo & ES_PASSWORD)
			e->clave = '*';
		w->ctl = e;
	}
	return w->ctl;
}
static int multi(Wnd *w) { return (w->estilo & ES_MULTILINE) != 0; }
static i32 ancho_c(Wnd *w) { HDC d = GetDC((HWND)w); i32 a; if (letra_de(w)) SelectObject(d, letra_de(w)); a = ancho_letra(dc(d)); ReleaseDC((HWND)w, d); return a ? a : 8; }
static i32 alto_c(Wnd *w) { HDC d = GetDC((HWND)w); i32 a; if (letra_de(w)) SelectObject(d, letra_de(w)); a = alto_letra(dc(d)); ReleaseDC((HWND)w, d); return a ? a : 16; }
/* la línea y la columna de una posición */
static void lin_col(Edit *e, u32 p, i32 *lin, i32 *col)
{
	u32 k, l = 0, c = 0;
	for (k = 0; k < p && k < e->n; k++) {
		if (e->t[k] == '\n') { l++; c = 0; }
		else if (e->t[k] != '\r') c++;
	}
	*lin = l;
	*col = c;
}
static u32 inicio_linea(Edit *e, i32 lin)
{
	u32 k = 0;
	while (lin > 0 && k < e->n) {
		if (e->t[k] == '\n')
			lin--;
		k++;
	}
	return k;
}
static u32 largo_linea(Edit *e, u32 ini)
{
	u32 k = ini;
	while (k < e->n && e->t[k] != '\r' && e->t[k] != '\n')
		k++;
	return k - ini;
}
static i32 lineas(Edit *e) { i32 n = 1; u32 k; for (k = 0; k < e->n; k++) if (e->t[k] == '\n') n++; return n; }
static u32 pos_de(Edit *e, i32 lin, i32 col)
{
	u32 i = inicio_linea(e, lin), l = largo_linea(e, i);
	if (col < 0) col = 0;
	return i + MIN((u32)col, l);
}
static void edit_rect(Wnd *w, RECT *r)
{
	Edit *e = edit(w);
	GetClientRect((HWND)w, r);
	r->left += e->margen_i + (multi(w) ? 2 : 2);
	r->right -= e->margen_d + 2;
	if (!multi(w)) {
		i32 a = alto_c(w);
		r->top = (r->bottom - a) / 2;
		if (r->top < 0) r->top = 0;
		r->bottom = r->top + a;
	} else
		r->top += 1;
}
static void ver_cursor(Wnd *w)
{
	Edit *e = edit(w);
	RECT r;
	i32 lin, col, cols, filas, a = ancho_c(w), al = alto_c(w);
	edit_rect(w, &r);
	lin_col(e, e->fin, &lin, &col);
	cols = (r.right - r.left) / a;
	filas = (r.bottom - r.top) / al;
	if (cols < 1) cols = 1;
	if (filas < 1) filas = 1;
	if (col < e->col0) e->col0 = col;
	if (col >= e->col0 + cols) e->col0 = col - cols + 1;
	if (multi(w)) {
		if (lin < e->linea0) e->linea0 = lin;
		if (lin >= e->linea0 + filas) e->linea0 = lin - filas + 1;
	}
	if (e->foco) {
		SetCaretPos(r.left + (col - e->col0) * a, r.top + (lin - e->linea0) * al);
	}
	if (multi(w) && (w->estilo & WS_VSCROLL)) {
		struct { u32 tam, mask; i32 min, max; u32 pag; i32 pos, track; } si = { 28, 7, 0, lineas(e) - 1, filas, e->linea0, 0 };
		SetScrollInfo((HWND)w, 1, &si, TRUE);
	}
}
static void edit_pinta(Wnd *w, HDC d)
{
	Edit *e = edit(w);
	RECT r, c;
	HBRUSH b;
	i32 a = ancho_c(w), al = alto_c(w), lin, filas;
	u32 s0 = MIN(e->ini, e->fin), s1 = MAX(e->ini, e->fin);
	GetClientRect((HWND)w, &c);
	if (letra_de(w))
		SelectObject(d, letra_de(w));
	b = colores(w, d, e->solo_leer || !IsWindowEnabled((HWND)w) ? WM_CTLCOLORSTATIC : WM_CTLCOLOREDIT);
	FillRect(d, &c, b);
	edit_rect(w, &r);
	filas = multi(w) ? (c.bottom - r.top + al - 1) / al : 1;
	SetBkMode(d, 2);
	for (lin = 0; lin < filas; lin++) {
		u32 i = multi(w) ? inicio_linea(e, e->linea0 + lin) : 0, l, k;
		i32 x;
		if (multi(w) && (i >= e->n && e->linea0 + lin > 0) && (lineas(e) <= e->linea0 + lin))
			break;
		l = largo_linea(e, i);
		x = r.left;
		if (!multi(w) && (w->estilo & (ES_CENTER | ES_RIGHT)) && l * a < (u32)(r.right - r.left)) {
			i32 sobra = (r.right - r.left) - l * a;
			x += (w->estilo & ES_CENTER) ? sobra / 2 : sobra;
		}
		for (k = e->col0; k < l; k++) {
			u32 p = i + k;
			char ch = e->clave ? e->clave : e->t[p];
			int sel = p >= s0 && p < s1 && (e->foco || (w->estilo & ES_NOHIDESEL));
			if (x >= c.right)
				break;
			SetTextColor(d, sel ? GetSysColor(COLOR_HIGHLIGHTTEXT) : IsWindowEnabled((HWND)w) ? GetTextColor(d) : GetSysColor(COLOR_GRAYTEXT));
			if (!sel && IsWindowEnabled((HWND)w))
				SetTextColor(d, GetSysColor(COLOR_WINDOWTEXT));
			SetBkColor(d, sel ? GetSysColor(COLOR_HIGHLIGHT) : (e->solo_leer || !IsWindowEnabled((HWND)w)) ? GetSysColor(COLOR_BTNFACE) : GetSysColor(COLOR_WINDOW));
			TextOutA(d, x, r.top + lin * al, ch == '\t' ? " " : &ch, 1);
			x += a;
		}
		if (!multi(w))
			break;
	}
}
static void cambio(Wnd *w)
{
	Edit *e = edit(w);
	e->cambiado = 1;
	ver_cursor(w);
	InvalidateRect((HWND)w, 0, TRUE);
	notificar(w, 0x400);                                  /* EN_UPDATE */
	notificar(w, 0x300);                                  /* EN_CHANGE */
}
static void guardar_deshacer(Edit *e)
{
	m_soltar(e->deshacer);
	e->deshacer = m_pedir(e->n + 1);
	memcpy(e->deshacer, e->t, e->n);
	e->deshacer[e->n] = 0;
}
/* cambia la selección por «s» (n octetos) */
static void reemplazar(Wnd *w, const char *s, u32 n, int notifica)
{
	Edit *e = edit(w);
	u32 s0 = MIN(e->ini, e->fin), s1 = MAX(e->ini, e->fin), k, o = 0;
	char *t = m_pedir(n * 2 + 2);
	/* los finales de línea: «\r\n» en el multilínea; sin ellos en el de una */
	for (k = 0; k < n; k++) {
		char c = s[k];
		if (!multi(w) && (c == '\r' || c == '\n'))
			break;
		if (multi(w) && c == '\n' && (!k || s[k - 1] != '\r'))
			t[o++] = '\r';
		if (w->estilo & ES_UPPERCASE) c = mayus(c);
		if (w->estilo & ES_LOWERCASE) c = minus(c);
		if ((w->estilo & ES_NUMBER) && (c < '0' || c > '9'))
			continue;
		t[o++] = c;
	}
	if (e->n - (s1 - s0) + o > e->limite) {
		o = e->limite > e->n - (s1 - s0) ? e->limite - (e->n - (s1 - s0)) : 0;
		notificar(w, 0x501);                              /* EN_MAXTEXT */
	}
	if (e->n - (s1 - s0) + o + 1 > e->cabe) {
		e->cabe = (e->n - (s1 - s0) + o + 1) * 2;
		e->t = m_cambiar(e->t, e->cabe);
	}
	memmove(e->t + s0 + o, e->t + s1, e->n - s1);
	memcpy(e->t + s0, t, o);
	e->n = e->n - (s1 - s0) + o;
	e->t[e->n] = 0;
	e->ini = e->fin = s0 + o;
	m_soltar(t);
	if (notifica)
		cambio(w);
	else {
		ver_cursor(w);
		InvalidateRect((HWND)w, 0, TRUE);
	}
}
static void poner_texto(Wnd *w, const char *s)
{
	Edit *e = edit(w);
	e->ini = 0;
	e->fin = e->n;
	e->linea0 = e->col0 = 0;
	reemplazar(w, s ? s : "", s ? strlen(s) : 0, 0);
	e->ini = e->fin = 0;
	e->cambiado = 0;
	ver_cursor(w);
}
static void copiar_sel(Wnd *w, int cortar)
{
	Edit *e = edit(w);
	u32 s0 = MIN(e->ini, e->fin), s1 = MAX(e->ini, e->fin);
	extern BOOL WINAPI OpenClipboard(HWND);
	extern BOOL WINAPI EmptyClipboard(void);
	extern HANDLE WINAPI SetClipboardData(u32, HANDLE);
	extern BOOL WINAPI CloseClipboard(void);
	char *b;
	if (s1 <= s0 || e->clave)
		return;
	b = m_pedir(s1 - s0 + 1);
	memcpy(b, e->t + s0, s1 - s0);
	b[s1 - s0] = 0;
	OpenClipboard((HWND)w);
	EmptyClipboard();
	SetClipboardData(1, b);
	CloseClipboard();
	m_soltar(b);
	if (cortar && !e->solo_leer) {
		guardar_deshacer(e);
		reemplazar(w, "", 0, 1);
	}
}
static void pegar(Wnd *w)
{
	extern BOOL WINAPI OpenClipboard(HWND);
	extern HANDLE WINAPI GetClipboardData(u32);
	extern BOOL WINAPI CloseClipboard(void);
	const char *t;
	if (edit(w)->solo_leer)
		return;
	OpenClipboard((HWND)w);
	t = GetClipboardData(1);
	if (t) {
		guardar_deshacer(edit(w));
		reemplazar(w, t, strlen(t), 1);
	}
	CloseClipboard();
}
static u32 pos_raton(Wnd *w, i32 x, i32 y)
{
	Edit *e = edit(w);
	RECT r;
	i32 a = ancho_c(w), al = alto_c(w), lin = 0, col;
	edit_rect(w, &r);
	if (multi(w))
		lin = e->linea0 + (y - r.top) / al;
	if (lin < 0) lin = 0;
	if (lin >= lineas(e)) lin = lineas(e) - 1;
	col = e->col0 + (x - r.left + a / 2) / a;
	return pos_de(e, lin, col);
}
static void mover(Wnd *w, u32 p, int mayus_)
{
	Edit *e = edit(w);
	e->fin = p;
	if (!mayus_)
		e->ini = p;
	ver_cursor(w);
	InvalidateRect((HWND)w, 0, TRUE);
}
API(LRESULT) proc_edicion(HWND h, u32 m, WPARAM wp, LPARAM lp)
{
	Wnd *w = wnd(h);
	Edit *e;
	if (!w)
		return 0;
	e = edit(w);
	switch (m) {
	case WM_CREATE:
		if (w->texto) {
			char *t = w->texto;
			w->texto = 0;
			poner_texto(w, t);
			m_soltar(t);
		}
		return 0;
	case WM_PAINT: {
		PAINTSTRUCT_ ps;
		HDC d = dc_control(w, &ps);
		edit_pinta(w, d);
		EndPaint(h, (void *)&ps);
		return 0;
	}
	case WM_ERASEBKGND:
		return 1;
	case WM_SETTEXT:
		poner_texto(w, (const char *)lp);
		notificar(w, 0x300);
		return 1;
	case WM_GETTEXT:
		if (!wp)
			return 0;
		copia_n((char *)lp, e->t, wp);
		return strlen((char *)lp);
	case WM_GETTEXTLENGTH:
		return e->n;
	case WM_SETFOCUS:
		e->foco = 1;
		CreateCaret(h, 0, 2, alto_c(w));
		ver_cursor(w);
		ShowCaret(h);
		if (!multi(w) && 0) {
			e->ini = 0;
			e->fin = e->n;
		}
		InvalidateRect(h, 0, TRUE);
		notificar(w, 0x100);                              /* EN_SETFOCUS */
		return 0;
	case WM_KILLFOCUS:
		e->foco = 0;
		DestroyCaret();
		InvalidateRect(h, 0, TRUE);
		notificar(w, 0x200);
		return 0;
	case WM_LBUTTONDOWN:
		SetFocus(h);
		mover(w, pos_raton(w, (i16)LOWORD(lp), (i16)HIWORD(lp)), (wp & 4) != 0);
		e->raton = 1;
		SetCapture(h);
		return 0;
	case WM_LBUTTONDBLCLK: {
		u32 a = e->fin, b = e->fin;
		while (a > 0 && e->t[a - 1] > ' ') a--;
		while (b < e->n && e->t[b] > ' ') b++;
		e->ini = a;
		e->fin = b;
		ver_cursor(w);
		InvalidateRect(h, 0, TRUE);
		return 0;
	}
	case WM_MOUSEMOVE:
		if (e->raton) {
			u32 p = pos_raton(w, (i16)LOWORD(lp), (i16)HIWORD(lp));
			if (p != e->fin)
				mover(w, p, 1);
		}
		return 0;
	case WM_LBUTTONUP:
		if (e->raton) {
			e->raton = 0;
			ReleaseCapture();
		}
		return 0;
	case WM_KEYDOWN: {
		int may = teclas_mayus(), ctl = teclas_control();
		i32 lin, col;
		lin_col(e, e->fin, &lin, &col);
		switch (wp) {
		case 0x25: mover(w, ctl ? (e->fin ? e->fin - 1 : 0) : (e->fin > 0 ? e->fin - 1 - (e->fin >= 2 && e->t[e->fin - 1] == '\n' && e->t[e->fin - 2] == '\r') : 0), may); return 0;
		case 0x27: mover(w, e->fin < e->n ? e->fin + 1 + (e->t[e->fin] == '\r' && e->fin + 1 < e->n && e->t[e->fin + 1] == '\n') : e->n, may); return 0;
		case 0x26: if (multi(w) && lin > 0) mover(w, pos_de(e, lin - 1, col), may); return 0;
		case 0x28: if (multi(w) && lin + 1 < lineas(e)) mover(w, pos_de(e, lin + 1, col), may); return 0;
		case 0x24: mover(w, ctl || !multi(w) ? 0 : inicio_linea(e, lin), may); return 0;
		case 0x23: mover(w, ctl || !multi(w) ? e->n : inicio_linea(e, lin) + largo_linea(e, inicio_linea(e, lin)), may); return 0;
		case 0x21: if (multi(w)) mover(w, pos_de(e, MAX(0, lin - 10), col), may); return 0;
		case 0x22: if (multi(w)) mover(w, pos_de(e, MIN(lineas(e) - 1, lin + 10), col), may); return 0;
		case 0x2E:                                         /* Supr */
			if (e->solo_leer)
				return 0;
			if (may) { copiar_sel(w, 1); return 0; }
			if (e->ini == e->fin && e->fin < e->n) {
				e->fin++;
				if (e->t[e->fin - 1] == '\r' && e->fin < e->n && e->t[e->fin] == '\n')
					e->fin++;
			}
			guardar_deshacer(e);
			reemplazar(w, "", 0, 1);
			return 0;
		case 0x2D:                                         /* Insert */
			if (ctl) copiar_sel(w, 0);
			else if (may) pegar(w);
			return 0;
		}
		return 0;
	}
	case WM_CHAR: {
		u8 c = (u8)wp;
		if (c == 1) {                                     /* Ctrl+A */
			e->ini = 0;
			e->fin = e->n;
			ver_cursor(w);
			InvalidateRect(h, 0, TRUE);
			return 0;
		}
		if (c == 3) { copiar_sel(w, 0); return 0; }
		if (c == 24) { copiar_sel(w, 1); return 0; }
		if (c == 22) { pegar(w); return 0; }
		if (c == 26) {                                    /* Ctrl+Z */
			if (e->deshacer) {
				char *d = e->deshacer;
				e->deshacer = 0;
				e->ini = 0;
				e->fin = e->n;
				reemplazar(w, d, strlen(d), 1);
				m_soltar(d);
			}
			return 0;
		}
		if (e->solo_leer)
			return 0;
		if (c == 8) {
			if (e->ini == e->fin && e->fin > 0) {
				e->ini = e->fin - 1;
				if (e->ini > 0 && e->t[e->ini] == '\n' && e->t[e->ini - 1] == '\r')
					e->ini--;
			}
			guardar_deshacer(e);
			reemplazar(w, "", 0, 1);
			return 0;
		}
		if (c == '\r') {
			if (multi(w) && ((w->estilo & ES_WANTRETURN) || !GetParent(h) || !wnd(GetParent(h))->es_dialogo)) {
				guardar_deshacer(e);
				reemplazar(w, "\r\n", 2, 1);
			}
			return 0;
		}
		if (c == '\t' && !multi(w))
			return 0;
		if (c < 32 && c != '\t')
			return 0;
		guardar_deshacer(e);
		reemplazar(w, (const char *)&c, 1, 1);
		return 0;
	}
	case WM_VSCROLL: {
		i32 n = lineas(e), filas;
		RECT r;
		edit_rect(w, &r);
		filas = (r.bottom - r.top) / alto_c(w);
		switch (LOWORD(wp)) {
		case 0: e->linea0--; break;
		case 1: e->linea0++; break;
		case 2: e->linea0 -= filas; break;
		case 3: e->linea0 += filas; break;
		case 4: case 5: e->linea0 = (i16)HIWORD(wp); break;
		}
		if (e->linea0 > n - filas) e->linea0 = n - filas;
		if (e->linea0 < 0) e->linea0 = 0;
		{
			struct { u32 tam, mask; i32 min, max; u32 pag; i32 pos, track; } si = { 28, 7, 0, n - 1, filas, e->linea0, 0 };
			SetScrollInfo(h, 1, &si, TRUE);
		}
		InvalidateRect(h, 0, TRUE);
		return 0;
	}
	case WM_GETDLGCODE: {
		LRESULT r = 0x80 | 1 | 8;                          /* DLGC_WANTCHARS | WANTARROWS | HASSETSEL */
		if (multi(w)) {
			MSG *x = (MSG *)lp;
			r |= 4;                                        /* DLGC_WANTALLKEYS (menos Esc y Tab) */
			if (x && x->message == WM_KEYDOWN && (x->wParam == 0x1B || x->wParam == 9))
				r &= ~4;
			if (x && x->message == WM_KEYDOWN && x->wParam == 0x0D && !(w->estilo & ES_WANTRETURN))
				r &= ~4;
		}
		return r;
	}
	case WM_CUT: copiar_sel(w, 1); return 0;
	case WM_COPY: copiar_sel(w, 0); return 0;
	case WM_PASTE: pegar(w); return 0;
	case WM_CLEAR: guardar_deshacer(e); reemplazar(w, "", 0, 1); return 0;
	case WM_UNDO: case 0xC7: proc_edicion(h, WM_CHAR, 26, 0); return 1;
	case 0xC6: return e->deshacer != 0;                   /* EM_CANUNDO */
	case 0xCD: m_soltar(e->deshacer); e->deshacer = 0; return 0;   /* EM_EMPTYUNDOBUFFER */
	case 0xB0:                                            /* EM_GETSEL */
		if (wp) *(u32 *)wp = MIN(e->ini, e->fin);
		if (lp) *(u32 *)lp = MAX(e->ini, e->fin);
		return MAKELONG(MIN(e->ini, e->fin), MAX(e->ini, e->fin));
	case 0xB1: {                                          /* EM_SETSEL */
		i32 a = (i32)wp, b = (i32)lp;
		if (a < 0) { e->ini = e->fin; ver_cursor(w); InvalidateRect(h, 0, TRUE); return 0; }
		if (b < 0 || (u32)b > e->n) b = e->n;
		if ((u32)a > e->n) a = e->n;
		e->ini = a;
		e->fin = b;
		ver_cursor(w);
		InvalidateRect(h, 0, TRUE);
		return 0;
	}
	case 0xB2: case 0xB3: edit_rect(w, (RECT *)lp); return 0;   /* EM_GETRECT */
	case 0xB5:                                            /* EM_SCROLL */
		proc_edicion(h, WM_VSCROLL, wp, 0);
		return 0;
	case 0xB6:                                            /* EM_LINESCROLL */
		e->col0 += (i32)wp;
		if (e->col0 < 0) e->col0 = 0;
		e->linea0 += (i32)lp;
		if (e->linea0 < 0) e->linea0 = 0;
		InvalidateRect(h, 0, TRUE);
		return 1;
	case 0xB7: ver_cursor(w); InvalidateRect(h, 0, TRUE); return 1;   /* EM_SCROLLCARET */
	case 0xB8: return e->cambiado;                        /* EM_GETMODIFY */
	case 0xB9: e->cambiado = wp != 0; return 0;
	case 0xBA: return lineas(e);                          /* EM_GETLINECOUNT */
	case 0xBB: {                                          /* EM_LINEINDEX */
		i32 lin, col;
		if ((i32)wp < 0)
			lin_col(e, e->fin, &lin, &col);
		else
			lin = wp;
		if (lin >= lineas(e))
			return -1;
		return inicio_linea(e, lin);
	}
	case 0xC1: {                                          /* EM_LINELENGTH */
		i32 lin, col;
		if ((i32)wp < 0)
			return largo_linea(e, inicio_linea(e, (lin_col(e, e->fin, &lin, &col), lin)));
		lin_col(e, wp, &lin, &col);
		return largo_linea(e, inicio_linea(e, lin));
	}
	case 0xC2:                                            /* EM_REPLACESEL */
		if (wp)
			guardar_deshacer(e);
		reemplazar(w, (const char *)lp, strlen((const char *)lp), 1);
		return 0;
	case 0xC4: {                                          /* EM_GETLINE */
		u32 i, l, cabe = *(u16 *)lp;
		if ((i32)wp >= lineas(e))
			return 0;
		i = inicio_linea(e, wp);
		l = MIN(largo_linea(e, i), cabe);
		memcpy((void *)lp, e->t + i, l);
		return l;
	}
	case 0xC5:                                            /* EM_LIMITTEXT */
		e->limite = wp ? wp : 0x7FFFFFFE;
		return 0;
	case 0xD5: return e->limite;                          /* EM_GETLIMITTEXT */
	case 0xC9: {                                          /* EM_LINEFROMCHAR */
		i32 lin, col;
		lin_col(e, (i32)wp < 0 ? e->fin : wp, &lin, &col);
		return lin;
	}
	case 0xCE: return multi(w) ? e->linea0 : e->col0;     /* EM_GETFIRSTVISIBLELINE */
	case 0xCF:                                            /* EM_SETREADONLY */
		e->solo_leer = wp != 0;
		if (wp) w->estilo |= ES_READONLY; else w->estilo &= ~ES_READONLY;
		InvalidateRect(h, 0, TRUE);
		return 1;
	case 0xCC: e->clave = (char)wp; InvalidateRect(h, 0, TRUE); return 0;   /* EM_SETPASSWORDCHAR */
	case 0xD2: return e->clave;
	case 0xD3:                                            /* EM_SETMARGINS */
		if (wp & 1) e->margen_i = LOWORD(lp);
		if (wp & 2) e->margen_d = HIWORD(lp);
		InvalidateRect(h, 0, TRUE);
		return 0;
	case 0xD4: return MAKELONG(e->margen_i, e->margen_d);
	case 0xD6: {                                          /* EM_POSFROMCHAR */
		i32 lin, col;
		RECT r;
		edit_rect(w, &r);
		lin_col(e, wp, &lin, &col);
		return MAKELONG(r.left + (col - e->col0) * ancho_c(w), r.top + (lin - e->linea0) * alto_c(w));
	}
	case 0xD7: return pos_raton(w, (i16)LOWORD(lp), (i16)HIWORD(lp));   /* EM_CHARFROMPOS */
	case 0xBD: case 0xBC: return 0;                       /* EM_GETHANDLE, EM_SETHANDLE */
	case 0xC8: return 0;                                  /* EM_FMTLINES */
	case 0xCB: return 1;                                  /* EM_SETTABSTOPS */
	case WM_SETFONT:
		SetPropA(h, "#letra", (HANDLE)wp);
		if (lp)
			InvalidateRect(h, 0, TRUE);
		return 0;
	case WM_GETFONT:
		return (LRESULT)letra_de(w);
	case WM_ENABLE:
		InvalidateRect(h, 0, TRUE);
		return 0;
	case WM_NCDESTROY:
		if (w->ctl) {
			m_soltar(e->t);
			m_soltar(e->deshacer);
			m_soltar(e);
			w->ctl = 0;
		}
		return 0;
	}
	return DefWindowProcA(h, m, wp, lp);
}

/* ---------------------------------------------------------------------------
 *  LISTBOX (y la lista de COMBOBOX)
 * ------------------------------------------------------------------------- */
typedef struct { char *t; u32 dato; int sel; } LItem;
typedef struct { LItem *it; i32 n, cabe, cur, arriba, ancla, alto, raton, foco; } Lista;
#define LBS_NOTIFY 1
#define LBS_SORT 2
#define LBS_MULTIPLESEL 8
#define LBS_OWNERDRAWFIXED 0x10
#define LBS_OWNERDRAWVARIABLE 0x20
#define LBS_HASSTRINGS 0x40
#define LBS_EXTENDEDSEL 0x800
static Lista *lista(Wnd *w)
{
	if (!w->ctl) {
		Lista *l = m_cero(sizeof(Lista));
		l->cur = -1;
		l->alto = 16;
		w->ctl = l;
	}
	return w->ctl;
}
static int ld_propia(Wnd *w) { return (w->estilo & (LBS_OWNERDRAWFIXED | LBS_OWNERDRAWVARIABLE)) && !(w->estilo & LBS_HASSTRINGS); }
static i32 l_meter(Wnd *w, Lista *l, i32 p, const char *t, int ordenar)
{
	i32 k;
	if (l->n == l->cabe) {
		l->cabe = l->cabe ? l->cabe * 2 : 16;
		l->it = m_cambiar(l->it, l->cabe * sizeof(LItem));
	}
	if (ordenar && !ld_propia(w)) {
		for (p = 0; p < l->n; p++)
			if (stricmp_(l->it[p].t ? l->it[p].t : "", t ? t : "") > 0)
				break;
	} else if (p < 0 || p > l->n)
		p = l->n;
	memmove(&l->it[p + 1], &l->it[p], (l->n - p) * sizeof(LItem));
	l->n++;
	memset(&l->it[p], 0, sizeof(LItem));
	if (ld_propia(w))
		l->it[p].dato = (u32)t;
	else if (t) {
		l->it[p].t = m_pedir(strlen(t) + 1);
		strcpy(l->it[p].t, t);
	}
	if (l->cur >= p)
		l->cur++;
	(void)k;
	return p;
}
static i32 l_visibles(Wnd *w, Lista *l)
{
	RECT r;
	GetClientRect((HWND)w, &r);
	return MAX(1, r.bottom / (l->alto ? l->alto : 16));
}
static void l_barra(Wnd *w, Lista *l)
{
	if (w->estilo & WS_VSCROLL) {
		struct { u32 tam, mask; i32 min, max; u32 pag; i32 pos, track; } si = { 28, 7 | 8, 0, l->n - 1, l_visibles(w, l), l->arriba, 0 };
		SetScrollInfo((HWND)w, 1, &si, TRUE);
	}
}
static void l_ver(Wnd *w, Lista *l)
{
	i32 v = l_visibles(w, l);
	if (l->cur >= 0) {
		if (l->cur < l->arriba) l->arriba = l->cur;
		if (l->cur >= l->arriba + v) l->arriba = l->cur - v + 1;
	}
	if (l->arriba > l->n - v) l->arriba = l->n - v;
	if (l->arriba < 0) l->arriba = 0;
	l_barra(w, l);
	InvalidateRect((HWND)w, 0, TRUE);
}
static void l_pinta(Wnd *w, HDC d)
{
	Lista *l = lista(w);
	RECT r;
	HBRUSH b;
	i32 k;
	GetClientRect((HWND)w, &r);
	if (letra_de(w))
		SelectObject(d, letra_de(w));
	b = colores(w, d, WM_CTLCOLORLISTBOX);
	FillRect(d, &r, b);
	for (k = l->arriba; k < l->n; k++) {
		RECT c = { 0, (k - l->arriba) * l->alto, r.right, (k - l->arriba + 1) * l->alto };
		int sel = (w->estilo & (LBS_MULTIPLESEL | LBS_EXTENDEDSEL)) ? l->it[k].sel : k == l->cur;
		if (c.top >= r.bottom)
			break;
		if (w->estilo & (LBS_OWNERDRAWFIXED | LBS_OWNERDRAWVARIABLE)) {
			DRAWITEMSTRUCT di;
			memset(&di, 0, sizeof di);
			di.tipo = 2; di.id = (u32)w->menu; di.item = k; di.accion = 1;
			di.estado = (sel ? 1 : 0) | (l->foco && k == l->cur ? 0x10 : 0);
			di.h = (HWND)w; di.dc = d; di.r = c; di.dato = l->it[k].dato;
			SendMessageA((HWND)w->padre, WM_DRAWITEM, (WPARAM)w->menu, (LPARAM)&di);
			continue;
		}
		if (sel) {
			FillRect(d, &c, GetSysColorBrush(COLOR_HIGHLIGHT));
			SetTextColor(d, GetSysColor(COLOR_HIGHLIGHTTEXT));
		} else
			SetTextColor(d, GetSysColor(IsWindowEnabled((HWND)w) ? COLOR_WINDOWTEXT : COLOR_GRAYTEXT));
		SetBkMode(d, 1);
		{
			RECT tr = c;
			tr.left += 2;
			DrawTextA(d, l->it[k].t ? l->it[k].t : "", -1, &tr, 0x24 | 0x40 | 0x800);
		}
		if (l->foco && k == l->cur)
			DrawFocusRect(d, &c);
	}
}
static void l_elige(Wnd *w, Lista *l, i32 k, int avisar)
{
	if (k < 0 || k >= l->n)
		return;
	if (!(w->estilo & (LBS_MULTIPLESEL | LBS_EXTENDEDSEL))) {
		if (l->cur == k)
			return;
	} else {
		i32 j;
		if (!(w->estilo & LBS_MULTIPLESEL) && !teclas_control())
			for (j = 0; j < l->n; j++)
				l->it[j].sel = 0;
		l->it[k].sel = (w->estilo & LBS_MULTIPLESEL) || teclas_control() ? !l->it[k].sel : 1;
	}
	l->cur = k;
	l_ver(w, l);
	if (avisar && (w->estilo & LBS_NOTIFY))
		notificar(w, 1);                                  /* LBN_SELCHANGE */
}
static i32 l_busca(Lista *l, i32 desde, const char *t, int exacta)
{
	i32 k, n = l->n;
	u32 lt = strlen(t);
	for (k = 0; k < n; k++) {
		i32 i = (desde + 1 + k) % (n ? n : 1);
		if (!l->it[i].t)
			continue;
		if (exacta ? !stricmp_(l->it[i].t, t) : !strnicmp_(l->it[i].t, t, lt))
			return i;
	}
	return -1;
}
static void l_vaciar(Lista *l)
{
	i32 k;
	for (k = 0; k < l->n; k++)
		m_soltar(l->it[k].t);
	l->n = 0;
	l->cur = -1;
	l->arriba = 0;
}
typedef struct { u32 at; u32 f[6]; u32 alto, bajo, r0, r1; char nombre[260]; char corto[14]; } FINDA;
HANDLE WINAPI FindFirstFileA(const char *, FINDA *);
BOOL WINAPI FindNextFileA(HANDLE, FINDA *);
BOOL WINAPI FindClose(HANDLE);
static i32 l_dir(Wnd *w, Lista *l, u32 at, const char *patron, int combo)
{
	FINDA f;
	HANDLE h = FindFirstFileA(patron, &f);
	i32 ult = -1;
	(void)combo;
	if (h != INVALID_HANDLE_VALUE) {
		do {
			char t[280];
			int carpeta = (f.at & 0x10) != 0;
			if (carpeta && !(at & 0x10))
				continue;
			if (!carpeta && (at & 0x8000))
				continue;
			if (carpeta) {
				if (!strcmp(f.nombre, "."))
					continue;
				fmt(t, sizeof t, "[%s]", f.nombre);
			} else
				copia_n(t, f.nombre, sizeof t);
			ult = l_meter(w, l, -1, t, 1);
		} while (FindNextFileA(h, &f));
		FindClose(h);
	}
	if (at & 0x4000) {                                    /* DDL_DRIVES */
		extern u32 WINAPI GetLogicalDrives(void);
		u32 m = GetLogicalDrives(), k;
		for (k = 0; k < 26; k++)
			if (m & (1u << k)) {
				char t[8];
				fmt(t, sizeof t, "[-%c-]", 'a' + k);
				ult = l_meter(w, l, -1, t, 0);
			}
	}
	return ult;
}
/* lo común de LB_* y CB_* (los de la lista) */
static LRESULT l_mensaje(Wnd *w, Lista *l, u32 m, WPARAM wp, LPARAM lp, int *hecho)
{
	*hecho = 1;
	switch (m) {
	case 0x180: { i32 p = l_meter(w, l, -1, (const char *)lp, (w->estilo & LBS_SORT) != 0); l_barra(w, l); InvalidateRect((HWND)w, 0, TRUE); return p; }
	case 0x181: { i32 p = l_meter(w, l, (i32)wp, (const char *)lp, 0); l_barra(w, l); InvalidateRect((HWND)w, 0, TRUE); return p; }
	case 0x182:                                           /* LB_DELETESTRING */
		if ((i32)wp < 0 || (i32)wp >= l->n)
			return -1;
		m_soltar(l->it[wp].t);
		memmove(&l->it[wp], &l->it[wp + 1], (l->n - wp - 1) * sizeof(LItem));
		l->n--;
		if (l->cur == (i32)wp) l->cur = -1;
		else if (l->cur > (i32)wp) l->cur--;
		l_ver(w, l);
		return l->n;
	case 0x184: l_vaciar(l); l_barra(w, l); InvalidateRect((HWND)w, 0, TRUE); return 0;   /* LB_RESETCONTENT */
	case 0x185:                                           /* LB_SETSEL */
		if ((i32)lp == -1) { i32 k; for (k = 0; k < l->n; k++) l->it[k].sel = wp != 0; }
		else if ((i32)lp >= 0 && (i32)lp < l->n) { l->it[lp].sel = wp != 0; if (wp) l->cur = lp; }
		InvalidateRect((HWND)w, 0, TRUE);
		return 0;
	case 0x186:                                           /* LB_SETCURSEL */
		if ((i32)wp < 0 || (i32)wp >= l->n) { l->cur = -1; InvalidateRect((HWND)w, 0, TRUE); return (i32)wp == -1 ? 0 : -1; }
		l->cur = wp;
		if (w->estilo & (LBS_MULTIPLESEL | LBS_EXTENDEDSEL)) { i32 k; for (k = 0; k < l->n; k++) l->it[k].sel = k == (i32)wp; }
		l_ver(w, l);
		return wp;
	case 0x187: return (i32)wp >= 0 && (i32)wp < l->n ? ((w->estilo & (LBS_MULTIPLESEL | LBS_EXTENDEDSEL)) ? l->it[wp].sel : (i32)wp == l->cur) : -1;
	case 0x188: return l->cur;                            /* LB_GETCURSEL */
	case 0x189:                                           /* LB_GETTEXT */
		if ((i32)wp < 0 || (i32)wp >= l->n)
			return -1;
		if (ld_propia(w)) { *(u32 *)lp = l->it[wp].dato; return 4; }
		strcpy((char *)lp, l->it[wp].t ? l->it[wp].t : "");
		return strlen((char *)lp);
	case 0x18A: return (i32)wp >= 0 && (i32)wp < l->n ? (ld_propia(w) ? 4 : (i32)strlen(l->it[wp].t ? l->it[wp].t : "")) : -1;
	case 0x18B: return l->n;                              /* LB_GETCOUNT */
	case 0x18C: {                                         /* LB_SELECTSTRING */
		i32 k = l_busca(l, (i32)wp, (const char *)lp, 0);
		if (k >= 0) { l->cur = k; l_ver(w, l); }
		return k;
	}
	case 0x18D: { i32 r = l_dir(w, l, wp, (const char *)lp, 0); l_barra(w, l); InvalidateRect((HWND)w, 0, TRUE); return r; }   /* LB_DIR */
	case 0x18E: return l->arriba;                         /* LB_GETTOPINDEX */
	case 0x18F: return l_busca(l, (i32)wp, (const char *)lp, 0);
	case 0x1A2: return l_busca(l, (i32)wp, (const char *)lp, 1);
	case 0x190: { i32 k, n = 0; for (k = 0; k < l->n; k++) n += l->it[k].sel; return (w->estilo & (LBS_MULTIPLESEL | LBS_EXTENDEDSEL)) ? n : -1; }
	case 0x191: { i32 k, n = 0; for (k = 0; k < l->n && n < (i32)wp; k++) if (l->it[k].sel) ((i32 *)lp)[n++] = k; return n; }
	case 0x197: l->arriba = wp; l_ver(w, l); return 0;    /* LB_SETTOPINDEX */
	case 0x198: {                                         /* LB_GETITEMRECT */
		RECT *r = (RECT *)lp, c;
		GetClientRect((HWND)w, &c);
		r->left = 0; r->right = c.right;
		r->top = ((i32)wp - l->arriba) * l->alto; r->bottom = r->top + l->alto;
		return 1;
	}
	case 0x199: return (i32)wp >= 0 && (i32)wp < l->n ? (LRESULT)l->it[wp].dato : -1;
	case 0x19A: if ((i32)wp >= 0 && (i32)wp < l->n) { l->it[wp].dato = lp; return 0; } return -1;
	case 0x19B: case 0x183: {                             /* LB_SELITEMRANGE(EX) */
		i32 a = m == 0x19B ? LOWORD(lp) : (i32)wp, b = m == 0x19B ? HIWORD(lp) : (i32)lp, k;
		for (k = MIN(a, b); k <= MAX(a, b) && k < l->n; k++)
			if (k >= 0) l->it[k].sel = m == 0x19B ? wp != 0 : 1;
		InvalidateRect((HWND)w, 0, TRUE);
		return 0;
	}
	case 0x19C: l->ancla = wp; return 0;
	case 0x19D: return l->ancla;
	case 0x19E: l->cur = wp; l_ver(w, l); return 0;
	case 0x19F: return l->cur;
	case 0x1A0: if (LOWORD(lp)) l->alto = LOWORD(lp); InvalidateRect((HWND)w, 0, TRUE); return 0;   /* LB_SETITEMHEIGHT */
	case 0x1A1: return l->alto;
	case 0x1A8: return l->n;                              /* LB_INITSTORAGE */
	case 0x1A9: {                                         /* LB_ITEMFROMPOINT */
		i32 k = l->arriba + (i16)HIWORD(lp) / l->alto;
		return k >= 0 && k < l->n ? k : MAKELONG(l->n ? l->n - 1 : 0, 1);
	}
	case 0x192: case 0x194: case 0x195: return 1;
	case 0x193: return 0;
	}
	*hecho = 0;
	return 0;
}
API(LRESULT) proc_lista(HWND h, u32 m, WPARAM wp, LPARAM lp)
{
	Wnd *w = wnd(h);
	Lista *l;
	int hecho;
	LRESULT r;
	if (!w)
		return 0;
	l = lista(w);
	r = l_mensaje(w, l, m, wp, lp, &hecho);
	if (hecho)
		return r;
	switch (m) {
	case WM_CREATE:
		if (w->estilo & (LBS_OWNERDRAWFIXED | LBS_OWNERDRAWVARIABLE)) {
			struct { u32 tipo, id, item, an, al; u32 dato; } mi = { 2, (u32)w->menu, 0, 0, 16, 0 };
			SendMessageA((HWND)w->padre, WM_MEASUREITEM, (WPARAM)w->menu, (LPARAM)&mi);
			if (mi.al)
				l->alto = mi.al;
		}
		return 0;
	case WM_PAINT: {
		PAINTSTRUCT_ ps;
		HDC d = dc_control(w, &ps);
		l_pinta(w, d);
		EndPaint(h, (void *)&ps);
		return 0;
	}
	case WM_ERASEBKGND:
		return 1;
	case WM_SIZE:
		l_ver(w, l);
		return 0;
	case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK: {
		i32 k = l->arriba + (i16)HIWORD(lp) / l->alto;
		SetFocus(h);
		if (k >= 0 && k < l->n)
			l_elige(w, l, k, 1);
		if (m == WM_LBUTTONDBLCLK && (w->estilo & LBS_NOTIFY))
			notificar(w, 2);                              /* LBN_DBLCLK */
		l->raton = 1;
		SetCapture(h);
		return 0;
	}
	case WM_MOUSEMOVE:
		if (l->raton && !(w->estilo & (LBS_MULTIPLESEL | LBS_EXTENDEDSEL))) {
			i32 k = l->arriba + (i16)HIWORD(lp) / l->alto;
			if (k >= 0 && k < l->n && k != l->cur)
				l_elige(w, l, k, 1);
		}
		return 0;
	case WM_LBUTTONUP:
		if (l->raton) {
			l->raton = 0;
			ReleaseCapture();
		}
		return 0;
	case WM_KEYDOWN: {
		i32 k = l->cur, v = l_visibles(w, l);
		switch (wp) {
		case 0x26: k--; break;
		case 0x28: k++; break;
		case 0x21: k -= v; break;
		case 0x22: k += v; break;
		case 0x24: k = 0; break;
		case 0x23: k = l->n - 1; break;
		case ' ':
			if (w->estilo & LBS_MULTIPLESEL) { l->it[l->cur].sel = !l->it[l->cur].sel; InvalidateRect(h, 0, TRUE); notificar(w, 1); }
			return 0;
		default: return 0;
		}
		if (k < 0) k = 0;
		if (k >= l->n) k = l->n - 1;
		l_elige(w, l, k, 1);
		return 0;
	}
	case WM_CHAR: {
		char t[2] = { (char)wp, 0 };
		i32 k = l_busca(l, l->cur, t, 0);
		if (k >= 0 && wp >= 32)
			l_elige(w, l, k, 1);
		return 0;
	}
	case WM_VSCROLL: {
		i32 v = l_visibles(w, l);
		switch (LOWORD(wp)) {
		case 0: l->arriba--; break;
		case 1: l->arriba++; break;
		case 2: l->arriba -= v; break;
		case 3: l->arriba += v; break;
		case 4: case 5: l->arriba = (i16)HIWORD(wp); break;
		}
		if (l->arriba > l->n - v) l->arriba = l->n - v;
		if (l->arriba < 0) l->arriba = 0;
		l_barra(w, l);
		InvalidateRect(h, 0, TRUE);
		return 0;
	}
	case WM_SETFOCUS:
		l->foco = 1;
		InvalidateRect(h, 0, TRUE);
		if (w->estilo & LBS_NOTIFY)
			notificar(w, 4);
		return 0;
	case WM_KILLFOCUS:
		l->foco = 0;
		InvalidateRect(h, 0, TRUE);
		if (w->estilo & LBS_NOTIFY)
			notificar(w, 5);
		return 0;
	case WM_GETDLGCODE:
		return 1 | 0x80;                                  /* DLGC_WANTARROWS | WANTCHARS */
	case WM_SETFONT:
		SetPropA(h, "#letra", (HANDLE)wp);
		if (lp)
			InvalidateRect(h, 0, TRUE);
		return 0;
	case WM_GETFONT:
		return (LRESULT)letra_de(w);
	case WM_ENABLE:
		InvalidateRect(h, 0, TRUE);
		return 0;
	case WM_NCDESTROY:
		if (w->ctl) {
			l_vaciar(l);
			m_soltar(l->it);
			m_soltar(l);
			w->ctl = 0;
		}
		return 0;
	}
	return DefWindowProcA(h, m, wp, lp);
}

/* ---------------------------------------------------------------------------
 *  COMBOBOX: una lista y, arriba, lo elegido (o un EDIT, en los desplegables
 *  que dejan escribir); la lista se abre como menú emergente del núcleo
 * ------------------------------------------------------------------------- */
typedef struct { Lista l; Wnd *ed; int foco, abierta; i32 alto_item; } Combo;
static Combo *combo(Wnd *w)
{
	if (!w->ctl) {
		Combo *c = m_cero(sizeof(Combo));
		c->l.cur = -1;
		c->l.alto = 16;
		w->ctl = c;
	}
	return w->ctl;
}
static u32 tipo_combo(Wnd *w) { return w->estilo & 3; }
void lista_emergente(Wnd *w, i32 x, i32 y, const char *ops, void (*f)(Wnd *, u32));
static void combo_elegido(Wnd *w, u32 op)
{
	Combo *c;
	if (!wnd((HWND)w))
		return;
	c = combo(w);
	c->abierta = 0;
	if (op != 0xFFFFFFFF && (i32)op < c->l.n) {
		c->l.cur = op;
		if (c->ed)
			SetWindowTextA((HWND)c->ed, c->l.it[op].t ? c->l.it[op].t : "");
		InvalidateRect((HWND)w, 0, TRUE);
		notificar(w, 9);                                  /* CBN_SELENDOK */
		notificar(w, 1);                                  /* CBN_SELCHANGE */
	} else
		notificar(w, 10);
	notificar(w, 8);                                      /* CBN_CLOSEUP */
}
static void combo_abrir(Wnd *w)
{
	Combo *c = combo(w);
	char op[256];
	u32 o = 0;
	i32 k;
	POINT p;
	RECT r;
	if (!c->l.n)
		return;
	notificar(w, 7);                                      /* CBN_DROPDOWN */
	for (k = 0; k < c->l.n && o < sizeof op - 4; k++) {
		const char *t = c->l.it[k].t ? c->l.it[k].t : "";
		u32 j;
		if (k)
			op[o++] = '|';
		if (!*t)
			op[o++] = ' ';
		for (j = 0; t[j] && o < sizeof op - 2; j++)
			op[o++] = t[j] == '|' ? '/' : a437c((u8)t[j]);
	}
	op[o] = 0;
	GetClientRect((HWND)w, &r);
	dentro_en_lienzo(w, &p);
	c->abierta = 1;
	lista_emergente(w, p.x, p.y + r.bottom, op, combo_elegido);
}
API(LRESULT) proc_combo(HWND h, u32 m, WPARAM wp, LPARAM lp)
{
	Wnd *w = wnd(h);
	Combo *c;
	int hecho;
	LRESULT r;
	if (!w)
		return 0;
	c = combo(w);
	if (m >= 0x143 && m <= 0x161 && m != 0x14F && m != 0x157 && m != 0x152 && m != 0x153 && m != 0x154 && m != 0x155 && m != 0x156) {
		/* los CB_ de la lista, como los LB_ */
		static const u16 a_lb[] = {
			0x180, 0x182, 0x18D, 0x18B, 0x188, 0x189, 0x18A, 0x181, 0x184, 0x18F, 0x18C, 0x186, 0,
			0x199, 0x19A, 0, 0x1A0, 0x1A1, 0, 0, 0, 0x1A2, 0, 0, 0x18E, 0x197, 0x193, 0x194, 0, 0, 0x1A8 };
		u32 lb = a_lb[m - 0x143];
		if (lb) {
			r = l_mensaje(w, &c->l, lb, wp, lp, &hecho);
			if (m == 0x14E || m == 0x14D) {           /* CB_SETCURSEL, CB_SELECTSTRING */
				if (c->ed)
					SetWindowTextA((HWND)c->ed, c->l.cur >= 0 && c->l.it[c->l.cur].t ? c->l.it[c->l.cur].t : "");
				InvalidateRect(h, 0, TRUE);
			}
			if (m == 0x14B && c->ed)
				SetWindowTextA((HWND)c->ed, "");
			return r;
		}
	}
	switch (m) {
	case WM_CREATE: {
		RECT rc;
		GetClientRect(h, &rc);
		if (tipo_combo(w) == 2 || tipo_combo(w) == 1) {
			c->ed = wnd(CreateWindowExA(0, "EDIT", "", 0x50000080 | (w->estilo & 0x40 ? 0 : 0), 2, 2, rc.right - 22, 20, h, (HMENU)1001, w->inst, 0));
		}
		/* el combo mide lo de arriba: lo de la lista no se ve (es un menú) */
		if (rc.bottom > 26) {
			RECT wr = w->r;
			w->r.bottom = wr.top + 24;
		}
		return 0;
	}
	case WM_SIZE:
		if (c->ed) {
			RECT rc;
			GetClientRect(h, &rc);
			MoveWindow((HWND)c->ed, 2, 2, rc.right - 22, rc.bottom - 4, TRUE);
		}
		return 0;
	case WM_PAINT: {
		PAINTSTRUCT_ ps;
		HDC d = dc_control(w, &ps);
		RECT rc, b;
		GetClientRect(h, &rc);
		if (letra_de(w))
			SelectObject(d, letra_de(w));
		FillRect(d, &rc, GetSysColorBrush(COLOR_WINDOW));
		DrawEdge(d, &rc, 0x0A, 0x0F);
		b = rc;
		b.left = b.right - 18;
		InflateRect_(&b, -2, -2);
		b.left += 2;
		relleno_3d(d, b, 0);
		{
			i32 cx = (b.left + b.right) / 2, cy = (b.top + b.bottom) / 2, k;
			for (k = 0; k < 4; k++) {
				RECT l = { cx - 3 + k, cy - 1 + k, cx + 4 - k, cy + k };
				FillRect(d, &l, GetSysColorBrush(IsWindowEnabled(h) ? COLOR_BTNTEXT : COLOR_GRAYTEXT));
			}
		}
		if (!c->ed && c->l.cur >= 0) {
			RECT t = { 4, 0, rc.right - 20, rc.bottom };
			if (c->foco) {
				RECT s = { 3, 3, rc.right - 21, rc.bottom - 3 };
				FillRect(d, &s, GetSysColorBrush(COLOR_HIGHLIGHT));
				SetTextColor(d, GetSysColor(COLOR_HIGHLIGHTTEXT));
			} else
				SetTextColor(d, GetSysColor(IsWindowEnabled(h) ? COLOR_WINDOWTEXT : COLOR_GRAYTEXT));
			SetBkMode(d, 1);
			if (w->estilo & 0x30) {
				DRAWITEMSTRUCT di;
				memset(&di, 0, sizeof di);
				di.tipo = 3; di.id = (u32)w->menu; di.item = c->l.cur; di.accion = 1;
				di.estado = 0x1000 | (c->foco ? 1 : 0); di.h = h; di.dc = d; di.r = t; di.dato = c->l.it[c->l.cur].dato;
				SendMessageA((HWND)w->padre, WM_DRAWITEM, (WPARAM)w->menu, (LPARAM)&di);
			} else
				DrawTextA(d, c->l.it[c->l.cur].t ? c->l.it[c->l.cur].t : "", -1, &t, 0x24 | 0x800);
		}
		EndPaint(h, (void *)&ps);
		return 0;
	}
	case WM_ERASEBKGND:
		return 1;
	case WM_LBUTTONDOWN:
		SetFocus(c->ed ? (HWND)c->ed : h);
		combo_abrir(w);
		return 0;
	case WM_KEYDOWN: {
		i32 k = c->l.cur;
		if (wp == 0x26) k--;
		else if (wp == 0x28) {
			if (teclas_alt()) { combo_abrir(w); return 0; }
			k++;
		} else if (wp == 0x73) { combo_abrir(w); return 0; }  /* F4 */
		else return 0;
		if (k < 0) k = 0;
		if (k >= c->l.n) k = c->l.n - 1;
		if (k != c->l.cur && k >= 0) {
			c->l.cur = k;
			if (c->ed)
				SetWindowTextA((HWND)c->ed, c->l.it[k].t ? c->l.it[k].t : "");
			InvalidateRect(h, 0, TRUE);
			notificar(w, 1);
		}
		return 0;
	}
	case WM_CHAR: {
		char t[2] = { (char)wp, 0 };
		i32 k = l_busca(&c->l, c->l.cur, t, 0);
		if (k >= 0 && wp >= 32 && k != c->l.cur) {
			c->l.cur = k;
			InvalidateRect(h, 0, TRUE);
			notificar(w, 1);
		}
		return 0;
	}
	case WM_COMMAND:
		if (c->ed && (HWND)lp == (HWND)c->ed) {
			if (HIWORD(wp) == 0x300) notificar(w, 5);     /* CBN_EDITCHANGE */
			if (HIWORD(wp) == 0x400) notificar(w, 6);
		}
		return 0;
	case WM_SETFOCUS:
		c->foco = 1;
		if (c->ed)
			SetFocus((HWND)c->ed);
		InvalidateRect(h, 0, TRUE);
		notificar(w, 3);
		return 0;
	case WM_KILLFOCUS:
		c->foco = 0;
		InvalidateRect(h, 0, TRUE);
		notificar(w, 4);
		return 0;
	case WM_SETTEXT:
		if (c->ed)
			return SetWindowTextA((HWND)c->ed, (const char *)lp);
		{
			i32 k = l_busca(&c->l, -1, (const char *)lp, 1);
			c->l.cur = k;
			InvalidateRect(h, 0, TRUE);
			return k >= 0;
		}
	case WM_GETTEXT:
		if (c->ed)
			return GetWindowTextA((HWND)c->ed, (char *)lp, wp);
		if (!wp)
			return 0;
		copia_n((char *)lp, c->l.cur >= 0 && c->l.it[c->l.cur].t ? c->l.it[c->l.cur].t : "", wp);
		return strlen((char *)lp);
	case WM_GETTEXTLENGTH:
		if (c->ed)
			return GetWindowTextLengthA_((HWND)c->ed);
		return c->l.cur >= 0 && c->l.it[c->l.cur].t ? (i32)strlen(c->l.it[c->l.cur].t) : 0;
	case 0x14F:                                           /* CB_SHOWDROPDOWN */
		if (wp && !c->abierta)
			combo_abrir(w);
		return 1;
	case 0x157: return c->abierta;                        /* CB_GETDROPPEDSTATE */
	case 0x152: GetWindowRect(h, (RECT *)lp); ((RECT *)lp)->bottom += 100; return 1;
	case 0x153: if (LOWORD(lp)) c->l.alto = LOWORD(lp); return 0;
	case 0x154: return (i32)wp == -1 ? 20 : c->l.alto;
	case 0x155: case 0x156: return 0;
	case 0x140:                                           /* CB_GETEDITSEL */
		return c->ed ? SendMessageA((HWND)c->ed, 0xB0, wp, lp) : 0;
	case 0x142:
		return c->ed ? SendMessageA((HWND)c->ed, 0xB1, (i16)LOWORD(lp), (i16)HIWORD(lp)) : 0;
	case 0x141:
		return c->ed ? SendMessageA((HWND)c->ed, 0xC5, wp, 0) : 1;
	case WM_GETDLGCODE:
		return 1 | 0x80;
	case WM_SETFONT:
		SetPropA(h, "#letra", (HANDLE)wp);
		if (c->ed)
			SendMessageA((HWND)c->ed, WM_SETFONT, wp, lp);
		if (lp)
			InvalidateRect(h, 0, TRUE);
		return 0;
	case WM_GETFONT:
		return (LRESULT)letra_de(w);
	case WM_ENABLE:
		if (c->ed)
			EnableWindow((HWND)c->ed, wp);
		InvalidateRect(h, 0, TRUE);
		return 0;
	case WM_NCDESTROY:
		if (w->ctl) {
			l_vaciar(&c->l);
			m_soltar(c->l.it);
			m_soltar(c);
			w->ctl = 0;
		}
		return 0;
	}
	return DefWindowProcA(h, m, wp, lp);
}

/* ---------------------------------------------------------------------------
 *  SCROLLBAR (el control)
 * ------------------------------------------------------------------------- */
API(LRESULT) proc_barra(HWND h, u32 m, WPARAM wp, LPARAM lp)
{
	Wnd *w = wnd(h);
	Barra *b;
	int vertical;
	if (!w)
		return 0;
	if (!w->ctl) {
		w->ctl = m_cero(sizeof(Barra));
		((Barra *)w->ctl)->max = 100;
	}
	b = w->ctl;
	vertical = (w->estilo & 1) != 0;                     /* SBS_VERT */
	switch (m) {
	case WM_PAINT: {
		PAINTSTRUCT_ ps;
		HDC d = dc_control(w, &ps);
		RECT r;
		GetClientRect(h, &r);
		barra_pintar(d, r, b, vertical, IsWindowEnabled(h));
		EndPaint(h, (void *)&ps);
		return 0;
	}
	case WM_ERASEBKGND:
		return 1;
	case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK: {
		RECT r;
		POINT o;
		GetClientRect(h, &r);
		dentro_en_lienzo(w, &o);
		OffsetRect_(&r, o.x, o.y);
		barra_clic(w, b, r, vertical, (i16)LOWORD(lp) + o.x, (i16)HIWORD(lp) + o.y, vertical ? WM_VSCROLL : WM_HSCROLL, (HWND)w->padre, h);
		return 0;
	}
	case 0xE0: { i32 a = b->pos; b->pos = wp; if (b->pos < b->min) b->pos = b->min; if (b->pos > b->max) b->pos = b->max; if (lp) InvalidateRect(h, 0, TRUE); return a; }   /* SBM_SETPOS */
	case 0xE1: return b->pos;
	case 0xE2: case 0xE6: b->min = wp; b->max = lp; if (b->pos < b->min) b->pos = b->min; if (b->pos > b->max) b->pos = b->max; InvalidateRect(h, 0, TRUE); return 0;
	case 0xE3: if (wp) *(i32 *)wp = b->min; if (lp) *(i32 *)lp = b->max; return 0;
	case 0xE9: return SetScrollInfo(h, 2, (void *)lp, wp);   /* SBM_SETSCROLLINFO */
	case 0xEA: return GetScrollInfo_(h, 2, (void *)lp);
	case 0xE4: return 1;
	case WM_GETDLGCODE: return 1;
	}
	return DefWindowProcA(h, m, wp, lp);
}
