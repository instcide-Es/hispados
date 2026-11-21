/* ventanas.h - WIN32.BIN: lo que comparten USER32 y GDI32 por dentro (las
 * ventanas, las superficies, los DC, los objetos de GDI y los menús).
 *
 * Cada ventana de primer nivel es una ventana de Hispa-Pentium (la llamada
 * 182 y las de siempre, 32 a 39): el núcleo le pone el título y el marco, y lo
 * de dentro es un lienzo de 32 bits de WIN32.BIN donde se pintan ella (su menú,
 * sus barras de desplazamiento) y todas sus hijas; lo que cambia se le da al
 * núcleo como un mapa de puntos (la 34).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef VENTANAS_H
#define VENTANAS_H
#include "w32.h"

/* ---- una superficie: donde se pinta (el lienzo de una ventana, un mapa de bits, una sección DIB) ---- */
typedef struct Sup {
	u8 *bits;
	i32 an, al, paso;        /* paso en octetos (de arriba abajo, siempre) */
	int bpp;                 /* 1, 4, 8, 16, 24, 32 */
	u32 pal[256];            /* (8 bits o menos) la paleta: 0x00RRGGBB */
	u32 npal;
	u32 mr, mg, mb;          /* (16 y 32 bits) las máscaras */
	int mono;                /* mapa de bits de 1 bit (BitBlt a color: tinta y fondo) */
	int de_abajo;            /* (DIB) las filas de abajo arriba: bits apunta a la de arriba y paso es negativo */
} Sup;

/* ---- los objetos de GDI ---- */
enum { G_PLUMA = 1, G_BROCHA, G_LETRA, G_MAPA, G_REGION, G_PALETA, G_DC };
#define MAX_RECTS 48
typedef struct { int n; RECT r[MAX_RECTS]; } Region;
typedef struct Gdi {
	u32 magia;
	int tipo, stock;
	union {
		struct { int estilo, ancho; COLORREF color; } pluma;
		struct { int estilo; COLORREF color; u32 trama; struct Gdi *mapa; } brocha;
		struct { i32 alto, ancho, peso; u8 cursiva, subraya, tachada, fija; char cara[32]; } letra;
		struct { Sup s; int seccion, seleccionado; u8 *propio; } mapa;
		Region region;
		struct { u32 n; u32 e[256]; } paleta;
	} u;
} Gdi;

struct Wnd;
typedef struct Dc {
	u32 magia;
	Sup *s;                  /* dónde pinta */
	struct Wnd *w;           /* (de una ventana) */
	int origen_x, origen_y;  /* dónde cae el (0,0) del DC en la superficie */
	RECT tope;               /* lo que se puede pintar (en la superficie) */
	Region clip;             /* (en la superficie) */
	int hay_clip_app;        /* SelectClipRgn del programa */
	Region clip_app;         /* (en coordenadas del DC) */
	Gdi *pluma, *brocha, *letra, *mapa, *paleta;
	COLORREF tinta, fondo;
	int modo_fondo;          /* 1 TRANSPARENT, 2 OPAQUE */
	int rop2, estirar, alinea, mapeo;
	POINT pos, vp, ve, bo;   /* la posición, el origen de la vista y de la ventana, el de la brocha */
	int memoria;             /* CreateCompatibleDC */
	int pintando;            /* de BeginPaint */
	struct Dc *guardado;     /* SaveDC */
	int nivel;
} Dc;
#define MAGIA_DC 0x44436864
#define MAGIA_GDI 0x47446869

/* ---- los menús ---- */
typedef struct Item {
	u32 id, fl, tipo;        /* MF_... ; MFT_... */
	struct Menu *sub;
	char *texto;
	u32 dato;
	HBITMAP mapa;
} Item;
typedef struct Menu {
	u32 magia;
	int n, cabe;
	Item *it;
	int barra;               /* es la barra de una ventana */
	u32 def;
} Menu;
#define MAGIA_MENU 0x4D656E75

/* ---- las clases y las ventanas ---- */
typedef struct Clase {
	char nombre[64];
	u32 estilo;
	WNDPROC proc;
	i32 extra_clase, extra;
	HINSTANCE inst;
	HICON icono, icono_peq;
	HCURSOR cursor;
	HBRUSH fondo;
	char menu[64];
	u32 menu_id;
	u16 atomo;
	u8 *datos_clase;
	int propia;              /* de dentro (BUTTON, EDIT...) */
	struct Clase *sig;
} Clase;

typedef struct Prop { char nombre[32]; u16 atomo; HANDLE v; struct Prop *sig; } Prop;
typedef struct { i32 min, max, pag, pos, track; int ver, activa; } Barra;

typedef struct Wnd {
	u32 magia;
	Clase *clase;
	WNDPROC proc;
	u32 estilo, exestilo;
	struct Wnd *padre, *dueno, *hijas, *sig;
	HMENU menu;              /* (las hijas: su número de control) */
	HINSTANCE inst;
	RECT r;                  /* la ventana: en la pantalla (primer nivel) o en lo de dentro de su madre */
	char *texto;
	u32 usuario;
	u8 *extra;
	int destruida, creada;
	/* lo que hay que pintar (en coordenadas de lo de dentro de ella) */
	RECT inval;
	int hay_inval, borrar, nc_sucio;
	Barra bv, bh;
	Prop *props;
	/* las de primer nivel */
	u32 vn;                  /* su ventana del núcleo (0: aún no, o es de las que no se ven) */
	Sup lienzo;
	RECT sucio;              /* lo del lienzo que hay que llevar al núcleo */
	int hay_sucio;
	int maxi, mini;
	int nucleo_marco;        /* el núcleo le pone título y marco */
	int modal;               /* un cuadro modal está encima: no le llega nada */
	/* los cuadros de diálogo */
	DLGPROC dlgproc;
	i32 dlg_res, dlg_fin, dlg_usuario;
	struct Wnd *dlg_foco;
	int es_dialogo;
	/* los controles de dentro */
	void *ctl;
	struct Wnd *activa_antes;
} Wnd;
#define MAGIA_WND 0x576E6448

typedef struct { HDC hdc; BOOL borrar; RECT r; BOOL res, inc; u8 rgb[32]; } PAINTSTRUCT_;
HDC WINAPI BeginPaint(HWND h, PAINTSTRUCT_ *ps);
BOOL WINAPI EndPaint(HWND h, const PAINTSTRUCT_ *ps);
HDC WINAPI GetDC(HWND h);
i32 WINAPI ReleaseDC(HWND h, HDC d);
u32 WINAPI GetTickCount(void);
#define GetTickCount_() GetTickCount()
i32 WINAPI GetWindowTextLengthA(HWND h);
#define GetWindowTextLengthA_ GetWindowTextLengthA
BOOL WINAPI GetScrollInfo(HWND h, i32 n, void *si);
#define GetScrollInfo_ GetScrollInfo
BOOL WINAPI SetPropA(HWND h, const char *n, HANDLE v);
HANDLE WINAPI GetPropA(HWND h, const char *n);
BOOL WINAPI CreateCaret(HWND h, HBITMAP b, i32 an, i32 al);
BOOL WINAPI DestroyCaret(void);
BOOL WINAPI SetCaretPos(i32 x, i32 y);
BOOL WINAPI ShowCaret(HWND h);
i32 WINAPI FrameRect(HDC h, const RECT *r, HBRUSH b);
HDC WINAPI CreateCompatibleDC(HDC h);
COLORREF WINAPI GetTextColor(HDC h);
HWND WINAPI SetActiveWindow(HWND h);
HBITMAP WINAPI CreateBitmap(i32 an, i32 al, u32 planos, u32 bpp, const void *bits);
void SetDlgItemTextA_(HWND h, i32 id, const char *t);
void AdjustWindowRectEx_(RECT *r);
BOOL WINAPI DeleteDC(HDC h);
static inline void InflateRect_(RECT *r, i32 x, i32 y) { r->left -= x; r->right += x; r->top -= y; r->bottom += y; }
static inline void OffsetRect_(RECT *r, i32 x, i32 y) { r->left += x; r->right += x; r->top += y; r->bottom += y; }
int IsChild_(Wnd *p, Wnd *h);
void SetRectEmpty_(RECT *r);
void lista_emergente(Wnd *w, i32 x, i32 y, const char *ops, void (*f)(Wnd *, u32));
extern void (*tic_dx_prog)(void);
void prepara_user(void);

/* user32.c */
Wnd *wnd(HWND h);
Wnd *raiz_de(Wnd *w);
extern Wnd *escritorio;
void lo_de_dentro(Wnd *w, RECT *r);           /* GetClientRect */
void dentro_en_lienzo(Wnd *w, POINT *p);      /* dónde cae el (0,0) de dentro de w en el lienzo de su raíz */
void ventana_en_lienzo(Wnd *w, POINT *p);     /* y el (0,0) de la ventana entera */
void visible_en_lienzo(Wnd *w, RECT *r, int dentro);  /* lo que se ve de ella, en el lienzo */
void ensuciar(Wnd *raiz, const RECT *r);      /* (en el lienzo) para llevar al núcleo */
void presentar(Wnd *raiz);
void presentar_todo(void);
int ventana_visible(Wnd *w);
void invalidar(Wnd *w, const RECT *r, int borrar);
void pintar_nc(Wnd *w);
void nc_medidas(Wnd *w, RECT *nc);            /* lo que ocupa lo de fuera: left, top, right, bottom */
LRESULT enviar(Wnd *w, u32 m, WPARAM wp, LPARAM lp);
void poner_mensaje(Wnd *w, u32 m, WPARAM wp, LPARAM lp);
void caret_esconder(Wnd *w);
void caret_ensenar(Wnd *w);
extern COLORREF colores_sistema[31];
extern Wnd *foco, *activa, *captura;
int teclas_mayus(void), teclas_control(void), teclas_alt(void);
LRESULT WINAPI DefWindowProcA(HWND h, u32 m, WPARAM wp, LPARAM lp);
LRESULT WINAPI SendMessageA(HWND h, u32 m, WPARAM wp, LPARAM lp);
BOOL WINAPI PostMessageA(HWND h, u32 m, WPARAM wp, LPARAM lp);
HWND WINAPI CreateWindowExA(u32 ex, const char *clase, const char *nombre, u32 estilo, i32 x, i32 y, i32 an, i32 al, HWND padre, HMENU menu, HINSTANCE inst, void *param);
BOOL WINAPI DestroyWindow(HWND h);
BOOL WINAPI InvalidateRect(HWND h, const RECT *r, BOOL borrar);
BOOL WINAPI GetClientRect(HWND h, RECT *r);
BOOL WINAPI GetWindowRect(HWND h, RECT *r);
BOOL WINAPI ShowWindow(HWND h, i32 c);
BOOL WINAPI UpdateWindow(HWND h);
HWND WINAPI SetFocus(HWND h);
BOOL WINAPI EnableWindow(HWND h, BOOL e);
BOOL WINAPI MoveWindow(HWND h, i32 x, i32 y, i32 an, i32 al, BOOL pintar);
HWND WINAPI SetCapture(HWND h);
BOOL WINAPI ReleaseCapture(void);
i32 WINAPI GetWindowTextA(HWND h, char *d, i32 n);
BOOL WINAPI SetWindowTextA(HWND h, const char *t);
HWND WINAPI GetParent(HWND h);
i32 WINAPI GetDlgCtrlID(HWND h);
HWND WINAPI GetDlgItem(HWND h, i32 id);
HWND WINAPI GetWindow(HWND h, u32 c);
BOOL WINAPI IsWindowEnabled(HWND h);
BOOL WINAPI IsWindowVisible(HWND h);
i32 WINAPI GetWindowLongA(HWND h, i32 i);
i32 WINAPI SetWindowLongA(HWND h, i32 i, i32 v);
BOOL WINAPI ClientToScreen(HWND h, POINT *p);
BOOL WINAPI ScreenToClient(HWND h, POINT *p);
i32 WINAPI SetScrollInfo(HWND h, i32 n, const void *si, BOOL pintar);
u32 WINAPI GetSysColor(i32 i);
HBRUSH WINAPI GetSysColorBrush(i32 i);
i32 WINAPI MessageBoxA(HWND h, const char *t, const char *titulo, u32 tipo);
BOOL WINAPI IsDialogMessageA(HWND h, MSG *m);
BOOL WINAPI TranslateMessage(const MSG *m);
LRESULT WINAPI DispatchMessageA(const MSG *m);
BOOL WINAPI PeekMessageA(MSG *m, HWND h, u32 a, u32 b, u32 quitar);
BOOL WINAPI GetMessageA(MSG *m, HWND h, u32 a, u32 b);
void bucle_modal(Wnd *w, i32 *fin);
Clase *clase_de(const char *n);
void registrar_propias(void);
BOOL WINAPI EndDialog(HWND h, i32 r);
HMENU WINAPI GetSubMenu(HMENU m, i32 p);
extern int pantalla_an, pantalla_al;
int raton_x(void), raton_y(void);
int sc_es_teclado(void);
u32 WINAPI MapVirtualKeyA(u32 c, u32 t);

/* gdi32.c */
HDC dc_de_ventana(Wnd *w, int dentro, int clip_hijas);
void dc_soltar(HDC h);
Dc *dc(HDC h);
Gdi *gdi(HGDIOBJ h);
u32 a_punto(Sup *s, COLORREF c);              /* COLORREF -> el punto de esa superficie */
COLORREF de_punto(Sup *s, u32 v);
u32 rgb(COLORREF c);                          /* COLORREF -> 0x00RRGGBB */
void rellenar(Sup *s, const Region *clip, RECT r, u32 color);   /* (color: 0x00RRGGBB) */
void sup_rect(Sup *s, const Region *clip, i32 x, i32 y, i32 an, i32 al, COLORREF c);
void reg_rect(Region *g, const RECT *r);
void reg_corta(Region *g, const RECT *r);    /* intersección */
void reg_quita(Region *g, const RECT *r);    /* resta */
int reg_vacia(const Region *g);
void texto_en(Sup *s, const Region *clip, i32 x, i32 y, const char *t, i32 n, u32 tinta, int k, int negrita);  /* letra de 8x16, k veces */
HBRUSH WINAPI CreateSolidBrush(COLORREF c);
HGDIOBJ WINAPI GetStockObject(i32 i);
HGDIOBJ WINAPI SelectObject(HDC h, HGDIOBJ o);
BOOL WINAPI DeleteObject(HGDIOBJ o);
COLORREF WINAPI SetTextColor(HDC h, COLORREF c);
COLORREF WINAPI SetBkColor(HDC h, COLORREF c);
i32 WINAPI SetBkMode(HDC h, i32 m);
BOOL WINAPI TextOutA(HDC h, i32 x, i32 y, const char *t, i32 n);
BOOL WINAPI GetTextExtentPoint32A(HDC h, const char *t, i32 n, SIZE *s);
i32 WINAPI FillRect(HDC h, const RECT *r, HBRUSH b);
i32 WINAPI DrawTextA(HDC h, const char *t, i32 n, RECT *r, u32 f);
BOOL WINAPI Rectangle(HDC h, i32 a, i32 b, i32 c, i32 d);
BOOL WINAPI MoveToEx(HDC h, i32 x, i32 y, POINT *p);
BOOL WINAPI LineTo(HDC h, i32 x, i32 y);
HPEN WINAPI CreatePen(i32 e, i32 a, COLORREF c);
BOOL WINAPI DrawEdge(HDC h, RECT *r, u32 borde, u32 fl);
BOOL WINAPI DrawFocusRect(HDC h, const RECT *r);
BOOL WINAPI StretchDIBits_(HDC h, i32 xd, i32 yd, i32 wd, i32 hd, i32 xs, i32 ys, i32 ws, i32 hs, const void *bits, const void *bmi, u32 uso, u32 rop);
i32 alto_letra(Dc *d);
i32 ancho_letra(Dc *d);
void relleno_3d(HDC h, RECT r, int hundido);
HBITMAP mapa_de_dib(const u8 *bmi_y_bits, u32 tam);   /* un BITMAP de recurso -> mapa de bits */
void dibuja_icono(HDC h, i32 x, i32 y, HICON ic, i32 an, i32 al);
extern const u8 letra8x16[];

/* menus.c */
Menu *menu(HMENU h);
void menu_barra_pintar(Wnd *w);
int menu_barra_clic(Wnd *w, i32 x, i32 y);   /* (en el lienzo) */
void menu_suceso(Wnd *raiz, u32 opcion, u32 etiqueta);
int menu_tecla(Wnd *w, u32 vk, int alt);
HMENU menu_de_recurso(const u8 *p, int ex, const u8 **fin);
i32 alto_menu(Wnd *w);
int menu_abierto(void);

/* controles.c */
LRESULT WINAPI proc_boton(HWND, u32, WPARAM, LPARAM);
LRESULT WINAPI proc_estatico(HWND, u32, WPARAM, LPARAM);
LRESULT WINAPI proc_edicion(HWND, u32, WPARAM, LPARAM);
LRESULT WINAPI proc_lista(HWND, u32, WPARAM, LPARAM);
LRESULT WINAPI proc_combo(HWND, u32, WPARAM, LPARAM);
LRESULT WINAPI proc_barra(HWND, u32, WPARAM, LPARAM);
void barra_pintar(HDC h, RECT r, Barra *b, int vertical, int activa);
int barra_clic(Wnd *w, Barra *b, RECT r, int vertical, i32 x, i32 y, u32 msg, HWND a_quien, HWND ctl);

/* dialogos.c */
LRESULT WINAPI DefDlgProcA(HWND h, u32 m, WPARAM wp, LPARAM lp);
void cuadro_texto(Wnd *w, const char *titulo, const char *texto);

/* las 1252 <-> el núcleo */
static inline u8 a437c(u8 c) { return c < 128 ? c : t1252_437[c - 128]; }
static inline u8 a1252c(u8 c) { return c < 128 ? c : t437_1252[c - 128]; }

/* los mensajes */
#define WM_CREATE 0x0001
#define WM_DESTROY 0x0002
#define WM_MOVE 0x0003
#define WM_SIZE 0x0005
#define WM_ACTIVATE 0x0006
#define WM_SETFOCUS 0x0007
#define WM_KILLFOCUS 0x0008
#define WM_ENABLE 0x000A
#define WM_SETREDRAW 0x000B
#define WM_SETTEXT 0x000C
#define WM_GETTEXT 0x000D
#define WM_GETTEXTLENGTH 0x000E
#define WM_PAINT 0x000F
#define WM_CLOSE 0x0010
#define WM_QUIT 0x0012
#define WM_ERASEBKGND 0x0014
#define WM_SYSCOLORCHANGE 0x0015
#define WM_SHOWWINDOW 0x0018
#define WM_ACTIVATEAPP 0x001C
#define WM_SETCURSOR 0x0020
#define WM_MOUSEACTIVATE 0x0021
#define WM_GETMINMAXINFO 0x0024
#define WM_NEXTDLGCTL 0x0028
#define WM_DRAWITEM 0x002B
#define WM_MEASUREITEM 0x002C
#define WM_DELETEITEM 0x002D
#define WM_SETFONT 0x0030
#define WM_GETFONT 0x0031
#define WM_WINDOWPOSCHANGING 0x0046
#define WM_WINDOWPOSCHANGED 0x0047
#define WM_NOTIFY 0x004E
#define WM_HELP 0x0053
#define WM_CONTEXTMENU 0x007B
#define WM_STYLECHANGED 0x007D
#define WM_GETICON 0x007F
#define WM_SETICON 0x0080
#define WM_NCCREATE 0x0081
#define WM_NCDESTROY 0x0082
#define WM_NCCALCSIZE 0x0083
#define WM_NCHITTEST 0x0084
#define WM_NCPAINT 0x0085
#define WM_NCACTIVATE 0x0086
#define WM_GETDLGCODE 0x0087
#define WM_NCMOUSEMOVE 0x00A0
#define WM_NCLBUTTONDOWN 0x00A1
#define WM_KEYDOWN 0x0100
#define WM_KEYUP 0x0101
#define WM_CHAR 0x0102
#define WM_SYSKEYDOWN 0x0104
#define WM_SYSKEYUP 0x0105
#define WM_SYSCHAR 0x0106
#define WM_INITDIALOG 0x0110
#define WM_COMMAND 0x0111
#define WM_SYSCOMMAND 0x0112
#define WM_TIMER 0x0113
#define WM_HSCROLL 0x0114
#define WM_VSCROLL 0x0115
#define WM_INITMENU 0x0116
#define WM_INITMENUPOPUP 0x0117
#define WM_MENUSELECT 0x011F
#define WM_ENTERIDLE 0x0121
#define WM_CTLCOLORMSGBOX 0x0132
#define WM_CTLCOLOREDIT 0x0133
#define WM_CTLCOLORLISTBOX 0x0134
#define WM_CTLCOLORBTN 0x0135
#define WM_CTLCOLORDLG 0x0136
#define WM_CTLCOLORSCROLLBAR 0x0137
#define WM_CTLCOLORSTATIC 0x0138
#define WM_MOUSEMOVE 0x0200
#define WM_LBUTTONDOWN 0x0201
#define WM_LBUTTONUP 0x0202
#define WM_LBUTTONDBLCLK 0x0203
#define WM_RBUTTONDOWN 0x0204
#define WM_RBUTTONUP 0x0205
#define WM_RBUTTONDBLCLK 0x0206
#define WM_MBUTTONDOWN 0x0207
#define WM_MBUTTONUP 0x0208
#define WM_MOUSEWHEEL 0x020A
#define WM_PARENTNOTIFY 0x0210
#define WM_ENTERMENULOOP 0x0211
#define WM_EXITMENULOOP 0x0212
#define WM_SIZING 0x0214
#define WM_CAPTURECHANGED 0x0215
#define WM_MOVING 0x0216
#define WM_DROPFILES 0x0233
#define WM_MDIGETACTIVE 0x0229
#define WM_CUT 0x0300
#define WM_COPY 0x0301
#define WM_PASTE 0x0302
#define WM_CLEAR 0x0303
#define WM_UNDO 0x0304
#define WM_USER 0x0400

#define WS_OVERLAPPED 0x00000000u
#define WS_POPUP 0x80000000u
#define WS_CHILD 0x40000000u
#define WS_MINIMIZE 0x20000000u
#define WS_VISIBLE 0x10000000u
#define WS_DISABLED 0x08000000u
#define WS_CLIPSIBLINGS 0x04000000u
#define WS_CLIPCHILDREN 0x02000000u
#define WS_MAXIMIZE 0x01000000u
#define WS_CAPTION 0x00C00000u
#define WS_BORDER 0x00800000u
#define WS_DLGFRAME 0x00400000u
#define WS_VSCROLL 0x00200000u
#define WS_HSCROLL 0x00100000u
#define WS_SYSMENU 0x00080000u
#define WS_THICKFRAME 0x00040000u
#define WS_GROUP 0x00020000u
#define WS_TABSTOP 0x00010000u
#define WS_MINIMIZEBOX 0x00020000u
#define WS_MAXIMIZEBOX 0x00010000u
#define WS_EX_DLGMODALFRAME 0x00000001u
#define WS_EX_TOPMOST 0x00000008u
#define WS_EX_TOOLWINDOW 0x00000080u
#define WS_EX_WINDOWEDGE 0x00000100u
#define WS_EX_CLIENTEDGE 0x00000200u
#define WS_EX_STATICEDGE 0x00020000u
#define WS_EX_CONTROLPARENT 0x00010000u

#define COLOR_SCROLLBAR 0
#define COLOR_BACKGROUND 1
#define COLOR_ACTIVECAPTION 2
#define COLOR_MENU 4
#define COLOR_WINDOW 5
#define COLOR_WINDOWFRAME 6
#define COLOR_MENUTEXT 7
#define COLOR_WINDOWTEXT 8
#define COLOR_CAPTIONTEXT 9
#define COLOR_HIGHLIGHT 13
#define COLOR_HIGHLIGHTTEXT 14
#define COLOR_BTNFACE 15
#define COLOR_BTNSHADOW 16
#define COLOR_GRAYTEXT 17
#define COLOR_BTNTEXT 18
#define COLOR_BTNHIGHLIGHT 20
#define COLOR_3DDKSHADOW 21
#define COLOR_3DLIGHT 22
#define COLOR_INFOTEXT 23
#define COLOR_INFOBK 24

#define RGB(r, g, b) ((COLORREF)((u8)(r) | ((u8)(g) << 8) | ((u8)(b) << 16)))
#define LOWORD(l) ((u16)((u32)(l) & 0xFFFF))
#define HIWORD(l) ((u16)((u32)(l) >> 16))
#define MAKELONG(a, b) ((i32)(((u16)(a)) | ((u32)((u16)(b))) << 16))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
