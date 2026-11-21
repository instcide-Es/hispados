/* dialogos.c - WIN32.BIN: los cuadros de diálogo de USER32: de una plantilla
 * (DLGTEMPLATE y DLGTEMPLATEEX, de los recursos o de memoria), modales
 * (DialogBox) y sin modo (CreateDialog), el gestor (IsDialogMessage: Tab,
 * las flechas, Intro, Esc y las letras subrayadas), DefDlgProc, lo de sus
 * controles (GetDlgItem, SetDlgItemText...) y MessageBox. Las medidas de la
 * plantilla, en las unidades de Windows: la letra es de 8x16, así que una
 * unidad son 2 puntos de ancho y 2 de alto.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "ventanas.h"
#include "k32.h"

#define API(t) t WINAPI
#define DS_SETFONT 0x40
#define DS_MODALFRAME 0x80
#define DS_CENTER 0x800
#define DM_GETDEFID 0x400
#define DM_SETDEFID 0x401
#define DLGC_WANTARROWS 1
#define DLGC_WANTTAB 2
#define DLGC_WANTALLKEYS 4
#define DLGC_WANTMESSAGE 4
#define DLGC_HASSETSEL 8
#define DLGC_DEFPUSHBUTTON 0x10
#define DLGC_UNDEFPUSHBUTTON 0x20
#define DLGC_RADIOBUTTON 0x40
#define DLGC_WANTCHARS 0x80
#define DLGC_STATIC 0x100
#define DLGC_BUTTON 0x2000
HFONT WINAPI CreateFontA(i32 al, i32 an, i32 esc, i32 ori, i32 peso, u32 cur, u32 sub, u32 tach, u32 juego, u32 ps, u32 pr, u32 cal, u32 pf, const char *cara);
BOOL WINAPI AdjustWindowRectEx(RECT *r, u32 estilo, BOOL menu, u32 ex);

static i32 ux(i32 v) { return v * 2; }
static i32 uy(i32 v) { return v * 2; }

/* ---------------------------------------------------------------------------
 *  La plantilla
 * ------------------------------------------------------------------------- */
static const u16 *palabra_o_numero(const u16 *p, char *d, u32 cabe, u32 *num)
{
	u32 k = 0;
	*num = 0;
	d[0] = 0;
	if (*p == 0)
		return p + 1;
	if (*p == 0xFFFF) {
		*num = p[1];
		return p + 2;
	}
	while (*p) {
		if (k + 1 < cabe)
			d[k++] = de_uni(*p);
		p++;
	}
	d[k] = 0;
	return p + 1;
}
static const char *clase_propia(u32 n)
{
	switch (n) {
	case 0x80: return "BUTTON";
	case 0x81: return "EDIT";
	case 0x82: return "STATIC";
	case 0x83: return "LISTBOX";
	case 0x84: return "SCROLLBAR";
	case 0x85: return "COMBOBOX";
	}
	return "STATIC";
}
static Wnd *primero_tab(Wnd *d)
{
	Wnd *w;
	for (w = d->hijas; w; w = w->sig)
		if ((w->estilo & WS_TABSTOP) && (w->estilo & WS_VISIBLE) && !(w->estilo & WS_DISABLED))
			return w;
	for (w = d->hijas; w; w = w->sig)
		if ((w->estilo & WS_VISIBLE) && !(w->estilo & WS_DISABLED) && !(SendMessageA((HWND)w, WM_GETDLGCODE, 0, 0) & DLGC_STATIC))
			return w;
	return 0;
}
static HWND crear_dialogo(HINSTANCE inst, const u8 *t, HWND padre, DLGPROC proc, LPARAM param, int modal)
{
	const u16 *p = (const u16 *)t;
	int ex = p[0] == 1 && p[1] == 0xFFFF;
	u32 estilo, exestilo, n, k, num;
	i32 x, y, cx, cy;
	char menu_n[64], clase[64], titulo[256], cara[64];
	u32 menu_id = 0, clase_id = 0;
	u16 puntos = 0, peso = 400;
	u8 cursiva = 0;
	HWND h;
	Wnd *d;
	HFONT letra = 0;
	RECT r;
	if (ex) {
		exestilo = *(const u32 *)(p + 4);
		estilo = *(const u32 *)(p + 6);
		n = p[8];
		x = (i16)p[9]; y = (i16)p[10]; cx = (i16)p[11]; cy = (i16)p[12];
		p += 13;
	} else {
		estilo = *(const u32 *)p;
		exestilo = *(const u32 *)(p + 2);
		n = p[4];
		x = (i16)p[5]; y = (i16)p[6]; cx = (i16)p[7]; cy = (i16)p[8];
		p += 9;
	}
	p = palabra_o_numero(p, menu_n, sizeof menu_n, &menu_id);
	p = palabra_o_numero(p, clase, sizeof clase, &clase_id);
	p = palabra_o_numero(p, titulo, sizeof titulo, &num);
	cara[0] = 0;
	if (estilo & DS_SETFONT) {
		puntos = *p++;
		if (ex) {
			peso = *p++;
			cursiva = *(const u8 *)p;
			p++;
		}
		p = palabra_o_numero(p, cara, sizeof cara, &num);
		letra = CreateFontA(-puntos * 4 / 3, 0, 0, 0, peso, cursiva, 0, 0, 0, 0, 0, 0, 0, cara);
	}
	/* la ventana del cuadro: lo de dentro mide (cx, cy) unidades */
	r.left = 0; r.top = 0; r.right = ux(cx); r.bottom = uy(cy);
	{
		u32 e = estilo & ~(WS_VISIBLE | DS_SETFONT);
		if (!(e & WS_CHILD))
			e |= WS_POPUP;
		AdjustWindowRectEx(&r, e, menu_n[0] || menu_id, exestilo);
		if ((estilo & DS_CENTER) || modal || 1) {
			x = (pantalla_an - (r.right - r.left)) / 2;
			y = (pantalla_al - 36 - (r.bottom - r.top)) / 2;
			if (padre && !(estilo & DS_CENTER) && 0) {
				POINT o = { ux(x), uy(y) };
				ClientToScreen(padre, &o);
				x = o.x;
				y = o.y;
			}
		}
		if (e & WS_CHILD) {
			x = ux((i16)(ex ? ((const u16 *)t)[9] : ((const u16 *)t)[5]));
			y = uy((i16)(ex ? ((const u16 *)t)[10] : ((const u16 *)t)[6]));
		}
		if (x < 0) x = 0;
		if (y < 0) y = 0;
		{
			extern HMENU WINAPI LoadMenuA(HINSTANCE, const char *);
			HMENU mn = menu_n[0] ? LoadMenuA(inst, menu_n) : menu_id ? LoadMenuA(inst, (const char *)menu_id) : 0;
			h = CreateWindowExA(exestilo | WS_EX_CONTROLPARENT, clase[0] ? clase : clase_id ? (const char *)clase_id : "#32770", titulo,
				e, x, y, r.right - r.left, r.bottom - r.top, padre, mn, inst, 0);
		}
	}
	d = wnd(h);
	if (!d)
		return 0;
	d->es_dialogo = 1;
	d->dlgproc = proc;
	if (!clase[0] && !clase_id)
		d->proc = DefDlgProcA;
	if (letra)
		SetPropA(h, "#letra", letra);
	/* los controles */
	for (k = 0; k < n; k++) {
		u32 ce, cex, id;
		i32 a, b, c, e2;
		char ccl[64], ctit[256];
		u32 ccl_id, ctit_id;
		u16 extra;
		const void *datos;
		HWND hc;
		p = (const u16 *)(((u32)p + 3) & ~3u);
		if (ex) {
			cex = *(const u32 *)(p + 2);
			ce = *(const u32 *)(p + 4);
			a = (i16)p[6]; b = (i16)p[7]; c = (i16)p[8]; e2 = (i16)p[9];
			id = *(const u32 *)(p + 10);
			p += 12;
		} else {
			ce = *(const u32 *)p;
			cex = *(const u32 *)(p + 2);
			a = (i16)p[4]; b = (i16)p[5]; c = (i16)p[6]; e2 = (i16)p[7];
			id = p[8];
			p += 9;
		}
		p = palabra_o_numero(p, ccl, sizeof ccl, &ccl_id);
		p = palabra_o_numero(p, ctit, sizeof ctit, &ctit_id);
		extra = *p++;
		datos = extra ? (const void *)p : 0;
		p = (const u16 *)((const u8 *)p + extra);
		if (ctit_id && !ctit[0]) {
			/* (un icono: «#n») */
			fmt(ctit, sizeof ctit, "#%u", ctit_id);
		}
		hc = CreateWindowExA(cex, ccl[0] ? ccl : clase_propia(ccl_id), ctit, ce | WS_CHILD, ux(a), uy(b), ux(c), uy(e2), h, (HMENU)id, inst, (void *)datos);
		if (hc && letra)
			SendMessageA(hc, WM_SETFONT, (WPARAM)letra, 0);
		if (hc && ctit_id && (ccl_id == 0x82 || !stricmp_(ccl, "STATIC")) && ((ce & 0x1F) == 3 || (ce & 0x1F) == 0xE)) {
			extern HICON WINAPI LoadIconA(HINSTANCE, const char *);
			extern HBITMAP WINAPI LoadBitmapA(HINSTANCE, const char *);
			HANDLE im = (ce & 0x1F) == 3 ? (HANDLE)LoadIconA(inst, (const char *)ctit_id) : (HANDLE)LoadBitmapA(inst, (const char *)ctit_id);
			SendMessageA(hc, (ce & 0x1F) == 3 ? 0x170 : 0x172, (ce & 0x1F) == 3 ? (WPARAM)im : 0, (LPARAM)im);
		}
	}
	{
		Wnd *f = primero_tab(d);
		LRESULT r2 = proc ? proc(h, WM_INITDIALOG, (WPARAM)f, param) : 1;
		if (!wnd(h))
			return 0;
		if (r2 && f)
			SetFocus((HWND)f);
		if (f && !d->dlg_foco)
			d->dlg_foco = foco && IsChild_(d, foco) ? foco : f;
	}
	if ((estilo & WS_VISIBLE) && !modal)
		ShowWindow(h, 5);
	return h;
}
static const u8 *plantilla(HINSTANCE inst, const char *n)
{
	u32 tam;
	return recurso(inst, (const char *)5, n, &tam);
}
API(HWND) CreateDialogIndirectParamA(HINSTANCE inst, const void *t, HWND p, DLGPROC f, LPARAM lp) { return crear_dialogo(inst, t, p, f, lp, 0); }
API(HWND) CreateDialogParamA(HINSTANCE inst, const char *n, HWND p, DLGPROC f, LPARAM lp)
{
	const u8 *t = plantilla(inst, n);
	if (!t) {
		pon_error(1813);
		return 0;
	}
	return crear_dialogo(inst, t, p, f, lp, 0);
}
API(HWND) CreateDialogParamW(HINSTANCE inst, const WCHAR *n, HWND p, DLGPROC f, LPARAM lp)
{
	char b[64];
	if ((u32)n < 0x10000)
		return CreateDialogParamA(inst, (const char *)n, p, f, lp);
	de_w(b, n, sizeof b);
	return CreateDialogParamA(inst, b, p, f, lp);
}
static i32 modal(HWND h, HWND padre)
{
	Wnd *d = wnd(h), *dueno = padre ? raiz_de(wnd(padre)) : 0;
	i32 r;
	if (!d)
		return -1;
	if (dueno) {
		dueno->modal++;
		EnableWindow((HWND)dueno, FALSE);
	}
	ShowWindow(h, 5);
	SetActiveWindow(h);
	{
		Wnd *f = d->dlg_foco ? d->dlg_foco : primero_tab(d);
		if (f && wnd((HWND)f) && (!foco || !IsChild_(d, foco)))
			SetFocus((HWND)f);
	}
	d->dlg_fin = 0;
	bucle_modal(d, &d->dlg_fin);
	r = wnd(h) ? d->dlg_res : 0;
	if (dueno && wnd((HWND)dueno)) {
		dueno->modal--;
		EnableWindow((HWND)dueno, TRUE);
	}
	if (wnd(h))
		DestroyWindow(h);
	if (dueno && wnd((HWND)dueno) && dueno->vn)
		SetActiveWindow((HWND)dueno);
	return r;
}
API(i32) DialogBoxIndirectParamA(HINSTANCE inst, const void *t, HWND p, DLGPROC f, LPARAM lp)
{
	HWND h = crear_dialogo(inst, t, p, f, lp, 1);
	if (!h)
		return -1;
	return modal(h, p);
}
API(i32) DialogBoxParamA(HINSTANCE inst, const char *n, HWND p, DLGPROC f, LPARAM lp)
{
	const u8 *t = plantilla(inst, n);
	traza("DialogBoxParamA(%x)", (u32)n);
	if (!t) {
		pon_error(1813);
		return -1;
	}
	return DialogBoxIndirectParamA(inst, t, p, f, lp);
}
API(i32) DialogBoxParamW(HINSTANCE inst, const WCHAR *n, HWND p, DLGPROC f, LPARAM lp)
{
	char b[64];
	if ((u32)n < 0x10000)
		return DialogBoxParamA(inst, (const char *)n, p, f, lp);
	de_w(b, n, sizeof b);
	return DialogBoxParamA(inst, b, p, f, lp);
}
API(i32) DialogBoxIndirectParamW(HINSTANCE inst, const void *t, HWND p, DLGPROC f, LPARAM lp) { return DialogBoxIndirectParamA(inst, t, p, f, lp); }
API(HWND) CreateDialogIndirectParamW(HINSTANCE inst, const void *t, HWND p, DLGPROC f, LPARAM lp) { return crear_dialogo(inst, t, p, f, lp, 0); }
API(BOOL) EndDialog(HWND h, i32 r)
{
	Wnd *d = wnd(h);
	if (!d)
		return FALSE;
	d->dlg_res = r;
	d->dlg_fin = 1;
	return TRUE;
}

/* ---------------------------------------------------------------------------
 *  DefDlgProc y el gestor de los cuadros
 * ------------------------------------------------------------------------- */
static Wnd *def_boton(Wnd *d)
{
	Wnd *w;
	LRESULT id = SendMessageA((HWND)d, DM_GETDEFID, 0, 0);
	if (HIWORD(id) == 0x534B) {
		w = wnd(GetDlgItem((HWND)d, LOWORD(id)));
		if (w)
			return w;
	}
	for (w = d->hijas; w; w = w->sig)
		if (SendMessageA((HWND)w, WM_GETDLGCODE, 0, 0) & DLGC_DEFPUSHBUTTON)
			return w;
	return 0;
}
API(LRESULT) DefDlgProcA(HWND h, u32 m, WPARAM wp, LPARAM lp)
{
	Wnd *d = wnd(h);
	if (!d)
		return 0;
	if (d->dlgproc) {
		LRESULT r;
		d->dlg_res = 0;
		r = d->dlgproc(h, m, wp, lp);
		if (!wnd(h))
			return 0;
		if (r) {
			if (m == WM_INITDIALOG || (m >= WM_CTLCOLORMSGBOX && m <= WM_CTLCOLORSTATIC) || m == 0x39 || m == 0x2F || m == 0x37 || m == 0x2E)
				return r;
			return d->dlg_res;
		}
	}
	switch (m) {
	case WM_ERASEBKGND: {
		RECT r;
		HBRUSH b = (HBRUSH)SendMessageA(h, WM_CTLCOLORDLG, wp, (LPARAM)h);
		GetClientRect(h, &r);
		FillRect((HDC)wp, &r, b ? b : GetSysColorBrush(COLOR_BTNFACE));
		return 1;
	}
	case WM_CTLCOLORDLG:
		return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);
	case WM_CLOSE: {
		HWND c = GetDlgItem(h, 2);
		if (!c || IsWindowEnabled(c))
			PostMessageA(h, WM_COMMAND, 2, (LPARAM)c);
		return 0;
	}
	case DM_GETDEFID: {
		Wnd *w;
		if (d->dlg_usuario & 0x10000)
			return MAKELONG(d->dlg_usuario & 0xFFFF, 0x534B);
		for (w = d->hijas; w; w = w->sig)
			if (SendMessageA((HWND)w, WM_GETDLGCODE, 0, 0) & DLGC_DEFPUSHBUTTON)
				return MAKELONG((u32)w->menu, 0x534B);
		return 0;
	}
	case DM_SETDEFID:
		d->dlg_usuario = (d->dlg_usuario & ~0x1FFFF) | 0x10000 | (wp & 0xFFFF);
		return 1;
	case WM_NEXTDLGCTL: {
		Wnd *f;
		extern HWND WINAPI GetNextDlgTabItem(HWND, HWND, BOOL);
		if (lp)
			f = wnd((HWND)wp);
		else
			f = wnd(GetNextDlgTabItem(h, (HWND)foco, wp != 0));
		if (f)
			SetFocus((HWND)f);
		return 0;
	}
	case WM_SETFOCUS:
		if (d->dlg_foco && wnd((HWND)d->dlg_foco))
			SetFocus((HWND)d->dlg_foco);
		else {
			Wnd *f = primero_tab(d);
			if (f)
				SetFocus((HWND)f);
		}
		return 0;
	case WM_ACTIVATE:
		if (LOWORD(wp) && d->dlg_foco && wnd((HWND)d->dlg_foco))
			SetFocus((HWND)d->dlg_foco);
		else if (!LOWORD(wp) && foco && IsChild_(d, foco))
			d->dlg_foco = foco;
		return 0;
	case WM_INITDIALOG:
		return 0;
	case WM_GETFONT:
		return (LRESULT)GetPropA(h, "#letra");
	case WM_SETFONT:
		SetPropA(h, "#letra", (HANDLE)wp);
		return 0;
	case WM_COMMAND:
		return 0;
	}
	return DefWindowProcA(h, m, wp, lp);
}
API(LRESULT) DefDlgProcW(HWND h, u32 m, WPARAM wp, LPARAM lp) { return DefDlgProcA(h, m, wp, lp); }
static int tabulable(Wnd *w)
{
	return (w->estilo & WS_TABSTOP) && (w->estilo & WS_VISIBLE) && !(w->estilo & WS_DISABLED);
}
API(HWND) GetNextDlgTabItem(HWND hd, HWND hc, BOOL antes)
{
	Wnd *d = wnd(hd), *c = wnd(hc), *lista[128], *x;
	i32 n = 0, k, i = -1;
	if (!d)
		return 0;
	for (x = d->hijas; x && n < 128; x = x->sig) {
		if (x == c)
			i = n;
		lista[n++] = x;
	}
	for (k = 1; k <= n; k++) {
		i32 j = antes ? ((i < 0 ? 0 : i) - k + 2 * n) % n : ((i < 0 ? -1 : i) + k) % n;
		if (tabulable(lista[j]))
			return (HWND)lista[j];
	}
	return hc;
}
API(HWND) GetNextDlgGroupItem(HWND hd, HWND hc, BOOL antes)
{
	Wnd *d = wnd(hd), *c = wnd(hc), *lista[128], *x;
	i32 n = 0, k, i = -1, ini, fin;
	if (!d || !c)
		return hc;
	for (x = d->hijas; x && n < 128; x = x->sig) {
		if (x == c)
			i = n;
		lista[n++] = x;
	}
	if (i < 0)
		return hc;
	/* el grupo: de la anterior con WS_GROUP (incluida) a la siguiente con WS_GROUP */
	for (ini = i; ini > 0 && !(lista[ini]->estilo & WS_GROUP); ini--)
		;
	for (fin = i + 1; fin < n && !(lista[fin]->estilo & WS_GROUP); fin++)
		;
	for (k = 1; k < fin - ini; k++) {
		i32 j = antes ? ini + ((i - ini) - k + 2 * (fin - ini)) % (fin - ini) : ini + ((i - ini) + k) % (fin - ini);
		if ((lista[j]->estilo & WS_VISIBLE) && !(lista[j]->estilo & WS_DISABLED))
			return (HWND)lista[j];
	}
	return hc;
}
/* la letra subrayada de un texto («&Abrir» -> A) */
static char letra_atajo(const char *t)
{
	for (; t && *t; t++)
		if (*t == '&' && t[1] && t[1] != '&')
			return mayus(t[1]);
	return 0;
}
API(BOOL) IsDialogMessageA(HWND hd, MSG *m)
{
	Wnd *d = wnd(hd), *t = wnd(m->hwnd);
	u32 codigo;
	if (!d || !t || (t != d && !IsChild_(d, t)))
		return FALSE;
	if (m->message == WM_KEYDOWN || m->message == WM_CHAR || m->message == WM_SYSCHAR) {
		codigo = SendMessageA(m->hwnd, WM_GETDLGCODE, m->wParam, (LPARAM)m);
		if (codigo & (DLGC_WANTALLKEYS | DLGC_WANTMESSAGE)) {
			DispatchMessageA(m);
			return TRUE;
		}
		if (m->message == WM_KEYDOWN) {
			switch (m->wParam) {
			case 0x09:                                     /* Tab */
				if (codigo & DLGC_WANTTAB)
					break;
				{
					HWND n = GetNextDlgTabItem(hd, m->hwnd, teclas_mayus());
					if (n) {
						SetFocus(n);
						if (SendMessageA(n, WM_GETDLGCODE, 0, 0) & DLGC_HASSETSEL)
							SendMessageA(n, 0xB1, 0, -1);
					}
				}
				return TRUE;
			case 0x25: case 0x26: case 0x27: case 0x28:
				if (codigo & DLGC_WANTARROWS)
					break;
				{
					HWND n = GetNextDlgGroupItem(hd, m->hwnd, m->wParam == 0x25 || m->wParam == 0x26);
					if (n && n != m->hwnd) {
						SetFocus(n);
						if (SendMessageA(n, WM_GETDLGCODE, 0, 0) & DLGC_RADIOBUTTON)
							SendMessageA(n, 0xF5, 0, 0);
					}
				}
				return TRUE;
			case 0x0D: {                                   /* Intro */
				Wnd *b = (codigo & (DLGC_DEFPUSHBUTTON | DLGC_UNDEFPUSHBUTTON)) ? t : def_boton(d);
				u32 id = b ? (u32)b->menu : 1;
				if (b && !IsWindowEnabled((HWND)b))
					return TRUE;
				SendMessageA(hd, WM_COMMAND, MAKELONG(id, 0), (LPARAM)b);
				return TRUE;
			}
			case 0x1B: {                                   /* Esc */
				HWND c = GetDlgItem(hd, 2);
				SendMessageA(hd, WM_COMMAND, 2, (LPARAM)c);
				return TRUE;
			}
			}
		}
		if ((m->message == WM_CHAR && !(codigo & DLGC_WANTCHARS)) || m->message == WM_SYSCHAR) {
			/* la letra subrayada */
			char c = mayus((u8)m->wParam);
			Wnd *x;
			for (x = d->hijas; x; x = x->sig)
				if ((x->estilo & WS_VISIBLE) && !(x->estilo & WS_DISABLED) && letra_atajo(x->texto) == c && c) {
					u32 cd = SendMessageA((HWND)x, WM_GETDLGCODE, 0, 0);
					if (cd & DLGC_STATIC) {
						/* la etiqueta: al control que la sigue */
						Wnd *s = x->sig;
						while (s && !tabulable(s))
							s = s->sig;
						if (s)
							SetFocus((HWND)s);
					} else if (cd & DLGC_BUTTON) {
						SetFocus((HWND)x);
						SendMessageA((HWND)x, 0xF5, 0, 0);
					} else
						SetFocus((HWND)x);
					return TRUE;
				}
			if (m->message == WM_SYSCHAR)
				return TRUE;
		}
	}
	TranslateMessage(m);
	DispatchMessageA(m);
	return TRUE;
}
API(BOOL) IsDialogMessageW(HWND hd, MSG *m) { return IsDialogMessageA(hd, m); }
API(HWND) GetDlgItem(HWND h, i32 id)
{
	Wnd *d = wnd(h), *w;
	if (!d)
		return 0;
	for (w = d->hijas; w; w = w->sig)
		if ((u32)w->menu == (u32)id)
			return (HWND)w;
	return 0;
}
API(i32) GetDlgCtrlID(HWND h) { Wnd *w = wnd(h); return w && (w->estilo & WS_CHILD) ? (i32)w->menu : 0; }
API(LRESULT) SendDlgItemMessageA(HWND h, i32 id, u32 m, WPARAM wp, LPARAM lp)
{
	HWND c = GetDlgItem(h, id);
	return c ? SendMessageA(c, m, wp, lp) : 0;
}
API(LRESULT) SendDlgItemMessageW(HWND h, i32 id, u32 m, WPARAM wp, LPARAM lp) { return SendDlgItemMessageA(h, id, m, wp, lp); }
API(BOOL) SetDlgItemTextA(HWND h, i32 id, const char *t) { HWND c = GetDlgItem(h, id); return c && SetWindowTextA(c, t); }
API(BOOL) SetDlgItemTextW(HWND h, i32 id, const WCHAR *t) { char b[1024]; de_w(b, t, sizeof b); return SetDlgItemTextA(h, id, b); }
API(u32) GetDlgItemTextA(HWND h, i32 id, char *d, i32 n)
{
	HWND c = GetDlgItem(h, id);
	if (!c) {
		if (n > 0)
			d[0] = 0;
		return 0;
	}
	return GetWindowTextA(c, d, n);
}
API(u32) GetDlgItemTextW(HWND h, i32 id, WCHAR *d, i32 n)
{
	char b[1024];
	u32 r = GetDlgItemTextA(h, id, b, sizeof b);
	if (n > 0)
		a_w(d, b, n);
	return r;
}
API(BOOL) SetDlgItemInt(HWND h, i32 id, u32 v, BOOL signo)
{
	char b[16];
	fmt(b, sizeof b, signo ? "%d" : "%u", v);
	return SetDlgItemTextA(h, id, b);
}
API(u32) GetDlgItemInt(HWND h, i32 id, BOOL *bien, BOOL signo)
{
	char b[32], *p = b;
	i32 v = 0, neg = 0, hay = 0;
	GetDlgItemTextA(h, id, b, sizeof b);
	while (*p == ' ')
		p++;
	if (signo && *p == '-') { neg = 1; p++; }
	while (*p >= '0' && *p <= '9') { v = v * 10 + *p++ - '0'; hay = 1; }
	while (*p == ' ')
		p++;
	if (bien)
		*bien = hay && !*p;
	return neg ? -v : v;
}
API(BOOL) CheckDlgButton(HWND h, i32 id, u32 c) { SendDlgItemMessageA(h, id, 0xF1, c, 0); return TRUE; }
API(u32) IsDlgButtonChecked(HWND h, i32 id) { return SendDlgItemMessageA(h, id, 0xF0, 0, 0); }
API(BOOL) CheckRadioButton(HWND h, i32 a, i32 b, i32 c)
{
	i32 k;
	for (k = a; k <= b; k++)
		SendDlgItemMessageA(h, k, 0xF1, k == c, 0);
	return TRUE;
}
API(BOOL) MapDialogRect(HWND h, RECT *r) { (void)h; r->left = ux(r->left); r->right = ux(r->right); r->top = uy(r->top); r->bottom = uy(r->bottom); return TRUE; }
API(i32) GetDialogBaseUnits(void) { return MAKELONG(8, 16); }

/* ---------------------------------------------------------------------------
 *  MessageBox: un cuadro hecho a mano, con sus botones en español
 * ------------------------------------------------------------------------- */
typedef struct { Wnd *w; i32 res; } CajaMsg;
static LRESULT WINAPI proc_msg(HWND h, u32 m, WPARAM wp, LPARAM lp)
{
	Wnd *w = wnd(h);
	if (m == WM_COMMAND && w && LOWORD(wp) >= 1 && LOWORD(wp) <= 7) {
		w->dlg_res = LOWORD(wp);
		w->dlg_fin = 1;
		return 0;
	}
	if (m == WM_CLOSE && w) {
		w->dlg_res = 2;
		w->dlg_fin = 1;
		return 0;
	}
	if (m == WM_PAINT && w && GetPropA(h, "#icono")) {
		PAINTSTRUCT_ ps;
		HDC d = BeginPaint(h, &ps);
		dibuja_icono(d, 16, 16, GetPropA(h, "#icono"), 32, 32);
		EndPaint(h, &ps);
		return 0;
	}
	return DefWindowProcA(h, m, wp, lp);
}
BOOL WINAPI MessageBeep(u32 t);           /* (SONIDO) user32.c */
API(i32) MessageBoxA(HWND padre, const char *texto, const char *titulo, u32 tipo)
{
	static const char *nombres[8] = { "", "Aceptar", "Cancelar", "Anular", "Reintentar", "Omitir", "Sí", "No" };
	static const u8 botones[6][3] = { { 1 }, { 1, 2 }, { 3, 4, 5 }, { 6, 7, 2 }, { 6, 7 }, { 4, 2 } };
	u32 cual = tipo & 15, nb, k, defecto = (tipo >> 8) & 3;
	i32 an, al, tx, anb = 96, hay_icono = (tipo & 0xF0) != 0;
	RECT r;
	HWND h, hs;
	Wnd *w, *dueno = padre ? raiz_de(wnd(padre)) : activa;
	HDC d;
	prepara_user();
	traza("MessageBoxA(%s, %s)", titulo ? titulo : "", texto ? texto : "");
	if (hay_icono)
		MessageBeep(tipo & 0xF0);           /* (SONIDO) */
	if (sys_dato(13) != 1) {
		/* sin escritorio: por la consola */
		char b[600];
		fmt(b, sizeof b, "%s: %s\n", titulo ? titulo : "Error", texto ? texto : "");
		{
			char c[600];
			a437(c, b, sizeof c);
			sys_escribir(c, strlen(c));
		}
		return cual == 4 || cual == 3 ? 6 : 1;
	}
	if (cual > 5)
		cual = 0;
	for (nb = 0; nb < 3 && botones[cual][nb]; nb++)
		;
	/* lo que mide el texto */
	d = GetDC(0);
	r.left = r.top = 0;
	r.right = MIN(pantalla_an - 160, 560);
	r.bottom = 0;
	DrawTextA(d, texto ? texto : "", -1, &r, 0x400 | 0x10 | 0x800);     /* DT_CALCRECT | WORDBREAK | NOPREFIX */
	ReleaseDC(0, d);
	tx = hay_icono ? 64 : 20;
	an = MAX(tx + r.right + 24, (i32)nb * (anb + 10) + 30);
	if (an < 240)
		an = 240;
	al = MAX(r.bottom, hay_icono ? 36 : 0) + 24 + 52;
	{
		RECT v = { 0, 0, an, al };
		AdjustWindowRectEx(&v, WS_POPUP | WS_CAPTION, 0, 0);
		h = CreateWindowExA(WS_EX_DLGMODALFRAME, "#32770", titulo ? titulo : "Error", WS_POPUP | WS_CAPTION | WS_SYSMENU,
			(pantalla_an - (v.right - v.left)) / 2, (pantalla_al - 36 - (v.bottom - v.top)) / 2, v.right - v.left, v.bottom - v.top, (HWND)dueno, 0, 0, 0);
	}
	w = wnd(h);
	if (!w)
		return 0;
	w->proc = proc_msg;
	w->es_dialogo = 1;
	if (hay_icono) {
		extern HICON WINAPI LoadIconA(HINSTANCE, const char *);
		u32 i = (tipo & 0xF0) == 0x10 ? 32513 : (tipo & 0xF0) == 0x20 ? 32514 : (tipo & 0xF0) == 0x30 ? 32515 : 32516;
		SetPropA(h, "#icono", LoadIconA(0, (const char *)i));
	}
	hs = CreateWindowExA(0, "STATIC", texto ? texto : "", WS_CHILD | WS_VISIBLE | 0x80, tx, 20, r.right + 4, MAX(r.bottom, 16), h, (HMENU)0xFFFF, 0, 0);
	(void)hs;
	for (k = 0; k < nb; k++) {
		u32 id = botones[cual][k];
		i32 x = (an - (i32)nb * (anb + 10) + 10) / 2 + k * (anb + 10);
		CreateWindowExA(0, "BUTTON", nombres[id], WS_CHILD | WS_VISIBLE | WS_TABSTOP | (k == defecto ? 1 : 0), x, al - 40, anb, 28, h, (HMENU)id, 0, 0);
	}
	{
		HWND b = GetDlgItem(h, botones[cual][defecto < nb ? defecto : 0]);
		w->dlg_foco = wnd(b);
		w->dlg_usuario = 0x10000 | botones[cual][defecto < nb ? defecto : 0];
	}
	w->proc = proc_msg;
	{
		/* como un cuadro modal (DefDlgProc no: proc_msg y el gestor) */
		i32 res;
		if (dueno)
			dueno->modal++;
		ShowWindow(h, 5);
		SetActiveWindow(h);
		if (w->dlg_foco)
			SetFocus((HWND)w->dlg_foco);
		w->dlg_fin = 0;
		bucle_modal(w, &w->dlg_fin);
		res = wnd(h) ? w->dlg_res : 2;
		if (dueno && wnd((HWND)dueno))
			dueno->modal--;
		if (wnd(h))
			DestroyWindow(h);
		if (dueno && wnd((HWND)dueno) && dueno->vn)
			SetActiveWindow((HWND)dueno);
		return res;
	}
}
API(i32) MessageBoxW(HWND h, const WCHAR *t, const WCHAR *ti, u32 tipo)
{
	char a[1024], b[256];
	if (t) de_w(a, t, sizeof a);
	if (ti) de_w(b, ti, sizeof b);
	return MessageBoxA(h, t ? a : 0, ti ? b : 0, tipo);
}
API(i32) MessageBoxExA(HWND h, const char *t, const char *ti, u32 tipo, u16 l) { (void)l; return MessageBoxA(h, t, ti, tipo); }
API(i32) MessageBoxIndirectA(const u32 *p) { return MessageBoxA((HWND)p[1], (const char *)p[3], (const char *)p[4], p[5]); }
