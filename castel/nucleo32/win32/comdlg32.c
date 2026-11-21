/* comdlg32.c - WIN32.BIN: COMDLG32, los cuadros comunes. Abrir y guardar
 * (GetOpenFileNameA, GetSaveFileNameA) es un cuadro hecho con los controles
 * de USER32: la carpeta, la lista de lo que hay (carpetas entre corchetes y
 * las unidades), el nombre, los tipos del filtro y los botones. Los demás
 * (color, letra, imprimir, buscar) dicen «cancelado» de momento.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "ventanas.h"
#include "k32.h"

#define API(t) t WINAPI
typedef struct {
	u32 tam;
	HWND dueno;
	HINSTANCE inst;
	const char *filtro;
	char *filtro_prop;
	u32 max_prop, indice;
	char *archivo;
	u32 max_archivo;
	char *titulo_arch;
	u32 max_titulo;
	const char *carpeta_ini, *titulo;
	u32 fl;
	u16 desp_nombre, desp_ext;
	const char *ext_def;
	LPARAM dato;
	void *gancho;
	const char *plantilla;
} OFN;
BOOL WINAPI SetCurrentDirectoryA(const char *d);
u32 WINAPI GetCurrentDirectoryA(u32 n, char *d);
u32 WINAPI GetFileAttributesA(const char *n);

static u32 error_cd;
API(u32) CommDlgExtendedError(void) { return error_cd; }

#define ID_CARPETA 1101
#define ID_LISTA 1102
#define ID_NOMBRE 1103
#define ID_TIPOS 1104
typedef struct { OFN *o; int guardar; char carpeta[260]; char patron[64]; i32 res; int fin; } Cuadro;
static Cuadro *cuadro;

/* el patrón del tipo elegido («*.TXT;*.DOC» -> el primero) */
static void patron_de(OFN *o, u32 i, char *d, u32 cabe)
{
	const char *f = o->filtro;
	u32 k = 1;
	copia_n(d, "*.*", cabe);
	if (!f)
		return;
	while (*f) {
		const char *nombre = f, *pat;
		(void)nombre;
		f += strlen(f) + 1;
		pat = f;
		if (!*f)
			break;
		f += strlen(f) + 1;
		if (k == i || (i == 0 && k == 1)) {
			const char *sc = strchr(pat, ';');
			u32 l = sc ? (u32)(sc - pat) : strlen(pat);
			if (l >= cabe)
				l = cabe - 1;
			memcpy(d, pat, l);
			d[l] = 0;
			return;
		}
		k++;
	}
}
static void llenar(HWND h)
{
	char t[300];
	HWND l = GetDlgItem(h, ID_LISTA);
	SendMessageA(l, 0x184, 0, 0);
	fmt(t, sizeof t, "%s%s%s", cuadro->carpeta, cuadro->carpeta[strlen(cuadro->carpeta) - 1] == '\\' ? "" : "\\", "*.*");
	/* las carpetas, y lo que casa con el patrón */
	SendMessageA(l, 0x18D, 0x8010, (LPARAM)t);
	fmt(t, sizeof t, "%s%s%s", cuadro->carpeta, cuadro->carpeta[strlen(cuadro->carpeta) - 1] == '\\' ? "" : "\\", cuadro->patron);
	SendMessageA(l, 0x18D, 0x0000, (LPARAM)t);
	SendMessageA(l, 0x18D, 0xC000, (LPARAM)"");
	SetDlgItemTextA_(h, ID_CARPETA, cuadro->carpeta);
}
BOOL WINAPI SetDlgItemTextA(HWND h, i32 id, const char *t);
u32 WINAPI GetDlgItemTextA(HWND h, i32 id, char *d, i32 n);
void SetDlgItemTextA_(HWND h, i32 id, const char *t) { SetDlgItemTextA(h, id, t); }
static void entrar(HWND h, const char *n)
{
	char t[300];
	if (n[0] == '[' && n[1] == '-') {
		fmt(cuadro->carpeta, sizeof cuadro->carpeta, "%c:\\", mayus(n[2]));
	} else {
		char nom[260];
		u32 l = strlen(n);
		copia_n(nom, n + 1, sizeof nom);
		if (l >= 2)
			nom[l - 2] = 0;
		if (!strcmp(nom, "..")) {
			char *b = strrchr(cuadro->carpeta, '\\');
			if (b && b > cuadro->carpeta + 2)
				*b = 0;
			else if (b)
				b[1] = 0;
		} else {
			fmt(t, sizeof t, "%s%s%s", cuadro->carpeta, cuadro->carpeta[strlen(cuadro->carpeta) - 1] == '\\' ? "" : "\\", nom);
			copia_n(cuadro->carpeta, t, sizeof cuadro->carpeta);
		}
	}
	llenar(h);
}
/* acepta: da 1 si vale (y lo deja en el OFN) */
static int aceptar(HWND h)
{
	char n[260], r[300];
	OFN *o = cuadro->o;
	u32 at;
	GetDlgItemTextA(h, ID_NOMBRE, n, sizeof n);
	if (!n[0])
		return 0;
	if (strchr(n, '*') || strchr(n, '?')) {
		copia_n(cuadro->patron, n, sizeof cuadro->patron);
		llenar(h);
		return 0;
	}
	if (n[1] == ':' || n[0] == '\\')
		ruta_entera(n, r, sizeof r);
	else {
		char t[300];
		fmt(t, sizeof t, "%s%s%s", cuadro->carpeta, cuadro->carpeta[strlen(cuadro->carpeta) - 1] == '\\' ? "" : "\\", n);
		ruta_entera(t, r, sizeof r);
	}
	at = GetFileAttributesA(r);
	if (at != 0xFFFFFFFF && (at & 0x10)) {
		copia_n(cuadro->carpeta, r, sizeof cuadro->carpeta);
		SetDlgItemTextA(h, ID_NOMBRE, "");
		llenar(h);
		return 0;
	}
	if (!strchr(strrchr(r, '\\') ? strrchr(r, '\\') : r, '.') && o->ext_def && o->ext_def[0]) {
		strcat(r, ".");
		strcat(r, o->ext_def);
		at = GetFileAttributesA(r);
	}
	if (!cuadro->guardar && (o->fl & 0x1000) && at == 0xFFFFFFFF) {   /* OFN_FILEMUSTEXIST */
		char m[400];
		fmt(m, sizeof m, "%s\nNo se encuentra el archivo.", r);
		MessageBoxA(h, m, cuadro->o->titulo ? cuadro->o->titulo : "Abrir", 0x30);
		return 0;
	}
	if (cuadro->guardar && (o->fl & 2) && at != 0xFFFFFFFF) {           /* OFN_OVERWRITEPROMPT */
		char m[400];
		fmt(m, sizeof m, "%s ya existe.\n¿Quiere reemplazarlo?", r);
		if (MessageBoxA(h, m, cuadro->o->titulo ? cuadro->o->titulo : "Guardar como", 0x34) != 6)
			return 0;
	}
	if (strlen(r) + 1 > o->max_archivo) {
		error_cd = 0x3003;                                 /* FNERR_BUFFERTOOSMALL */
		return 0;
	}
	strcpy(o->archivo, r);
	{
		char *b = strrchr(o->archivo, '\\'), *p;
		o->desp_nombre = b ? b + 1 - o->archivo : 0;
		p = strrchr(o->archivo + o->desp_nombre, '.');
		o->desp_ext = p ? p + 1 - o->archivo : strlen(o->archivo);
		if (o->titulo_arch && o->max_titulo)
			copia_n(o->titulo_arch, o->archivo + o->desp_nombre, o->max_titulo);
	}
	return 1;
}
static LRESULT WINAPI proc_archivos(HWND h, u32 m, WPARAM wp, LPARAM lp)
{
	switch (m) {
	case WM_COMMAND:
		switch (LOWORD(wp)) {
		case 1:
			if (aceptar(h)) {
				cuadro->res = 1;
				cuadro->fin = 1;
			}
			return 0;
		case 2:
			cuadro->res = 0;
			cuadro->fin = 1;
			return 0;
		case ID_LISTA:
			if (HIWORD(wp) == 1 || HIWORD(wp) == 2) {        /* LBN_SELCHANGE, LBN_DBLCLK */
				char t[280];
				i32 k = SendMessageA((HWND)lp, 0x188, 0, 0);
				if (k < 0)
					return 0;
				SendMessageA((HWND)lp, 0x189, k, (LPARAM)t);
				if (t[0] == '[') {
					if (HIWORD(wp) == 2)
						entrar(h, t);
				} else {
					SetDlgItemTextA(h, ID_NOMBRE, t);
					if (HIWORD(wp) == 2 && aceptar(h)) {
						cuadro->res = 1;
						cuadro->fin = 1;
					}
				}
			}
			return 0;
		case ID_TIPOS:
			if (HIWORD(wp) == 1) {
				i32 k = SendMessageA((HWND)lp, 0x147, 0, 0);
				patron_de(cuadro->o, k + 1, cuadro->patron, sizeof cuadro->patron);
				cuadro->o->indice = k + 1;
				llenar(h);
			}
			return 0;
		}
		return 0;
	case WM_CLOSE:
		cuadro->res = 0;
		cuadro->fin = 1;
		return 0;
	}
	return DefWindowProcA(h, m, wp, lp);
}
static BOOL cuadro_archivos(OFN *o, int guardar)
{
	Cuadro c;
	HWND h, t;
	Wnd *w, *dueno = o->dueno ? raiz_de(wnd(o->dueno)) : 0;
	i32 an = 480, al = 340;
	RECT v = { 0, 0, an, al };
	char antes[260];
	prepara_user();
	memset(&c, 0, sizeof c);
	c.o = o;
	c.guardar = guardar;
	cuadro = &c;
	error_cd = 0;
	GetCurrentDirectoryA(sizeof antes, antes);
	if (o->carpeta_ini && o->carpeta_ini[0])
		ruta_entera(o->carpeta_ini, c.carpeta, sizeof c.carpeta);
	else if (o->archivo && o->archivo[0] && strchr(o->archivo, '\\')) {
		char *b;
		ruta_entera(o->archivo, c.carpeta, sizeof c.carpeta);
		b = strrchr(c.carpeta, '\\');
		if (b && b > c.carpeta + 2) *b = 0; else if (b) b[1] = 0;
	} else
		copia_n(c.carpeta, antes, sizeof c.carpeta);
	patron_de(o, o->indice, c.patron, sizeof c.patron);
	AdjustWindowRectEx_(&v);
	h = CreateWindowExA(WS_EX_DLGMODALFRAME, "#32770", o->titulo ? o->titulo : guardar ? "Guardar como" : "Abrir",
		WS_POPUP | WS_CAPTION | WS_SYSMENU, (pantalla_an - (v.right - v.left)) / 2, (pantalla_al - 36 - (v.bottom - v.top)) / 2,
		v.right - v.left, v.bottom - v.top, (HWND)dueno, 0, 0, 0);
	w = wnd(h);
	if (!w)
		return FALSE;
	w->proc = proc_archivos;
	w->es_dialogo = 1;
	CreateWindowExA(0, "STATIC", "Carpeta:", WS_CHILD | WS_VISIBLE, 12, 12, 72, 18, h, (HMENU)0xFFFF, 0, 0);
	CreateWindowExA(0, "STATIC", "", WS_CHILD | WS_VISIBLE | 0xC, 88, 12, an - 100, 18, h, (HMENU)ID_CARPETA, 0, 0);
	CreateWindowExA(WS_EX_CLIENTEDGE, "LISTBOX", "", WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | 1 | 2, 12, 38, an - 24, 188, h, (HMENU)ID_LISTA, 0, 0);
	CreateWindowExA(0, "STATIC", "&Nombre:", WS_CHILD | WS_VISIBLE, 12, 238, 80, 18, h, (HMENU)0xFFFF, 0, 0);
	t = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | WS_TABSTOP | 0x80, 96, 234, an - 108, 24, h, (HMENU)ID_NOMBRE, 0, 0);
	CreateWindowExA(0, "STATIC", "&Tipo:", WS_CHILD | WS_VISIBLE, 12, 270, 80, 18, h, (HMENU)0xFFFF, 0, 0);
	{
		HWND cb = CreateWindowExA(0, "COMBOBOX", "", WS_CHILD | WS_VISIBLE | WS_TABSTOP | 3, 96, 266, an - 108, 24, h, (HMENU)ID_TIPOS, 0, 0);
		const char *f = o->filtro;
		if (f)
			while (*f) {
				const char *n = f;
				f += strlen(f) + 1;
				if (!*f)
					break;
				SendMessageA(cb, 0x143, 0, (LPARAM)n);
				f += strlen(f) + 1;
			}
		else
			SendMessageA(cb, 0x143, 0, (LPARAM)"Todos los archivos (*.*)");
		SendMessageA(cb, 0x14E, o->indice ? o->indice - 1 : 0, 0);
	}
	CreateWindowExA(0, "BUTTON", guardar ? "&Guardar" : "&Abrir", WS_CHILD | WS_VISIBLE | WS_TABSTOP | 1, an - 212, al - 40, 96, 28, h, (HMENU)1, 0, 0);
	CreateWindowExA(0, "BUTTON", "Cancelar", WS_CHILD | WS_VISIBLE | WS_TABSTOP, an - 108, al - 40, 96, 28, h, (HMENU)2, 0, 0);
	if (o->archivo && o->archivo[0]) {
		const char *b = strrchr(o->archivo, '\\');
		SetWindowTextA(t, b ? b + 1 : o->archivo);
	}
	llenar(h);
	w->dlg_foco = wnd(t);
	w->dlg_usuario = 0x10001;
	if (dueno)
		dueno->modal++;
	ShowWindow(h, 5);
	SetActiveWindow(h);
	SetFocus(t);
	bucle_modal(w, &c.fin);
	if (dueno && wnd((HWND)dueno))
		dueno->modal--;
	if (wnd(h))
		DestroyWindow(h);
	if (dueno && wnd((HWND)dueno) && dueno->vn)
		SetActiveWindow((HWND)dueno);
	/* (como en Windows 9x: la carpeta de trabajo, la que se ha visto, salvo OFN_NOCHANGEDIR) */
	if (o->fl & 8)
		SetCurrentDirectoryA(antes);
	cuadro = 0;
	return c.res;
}
void AdjustWindowRectEx_(RECT *r) { r->left -= BORDE_NUCLEO; r->right += BORDE_NUCLEO; r->top -= TITULO_NUCLEO; r->bottom += BORDE_NUCLEO; }
API(BOOL) GetOpenFileNameA(OFN *o) { traza("GetOpenFileNameA"); return cuadro_archivos(o, 0); }
API(BOOL) GetSaveFileNameA(OFN *o) { traza("GetSaveFileNameA"); return cuadro_archivos(o, 1); }
API(i16) GetFileTitleA(const char *a, char *b, u16 n)
{
	const char *p = strrchr(a, '\\');
	p = p ? p + 1 : a;
	if (!b || n <= strlen(p))
		return strlen(p) + 1;
	strcpy(b, p);
	return 0;
}
API(BOOL) ChooseColorA(void *c) { (void)c; error_cd = 0; return FALSE; }
API(BOOL) ChooseFontA(void *c) { (void)c; error_cd = 0; return FALSE; }
API(BOOL) PrintDlgA(void *c) { (void)c; error_cd = 0; return FALSE; }
API(BOOL) PageSetupDlgA(void *c) { (void)c; return FALSE; }
API(HWND) FindTextA(void *c) { (void)c; return 0; }
API(HWND) ReplaceTextA(void *c) { (void)c; return 0; }
