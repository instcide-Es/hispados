/* otros.c - WIN32.BIN: lo poco que piden los programas de ADVAPI32 (el
 * registro: no hay, todo «no está»), SHELL32, VERSION y WINSPOOL.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "w32.h"
#include "k32.h"
#define API(t) t WINAPI
u32 WINAPI GetModuleFileNameA(HMODULE h, char *d, u32 n);
BOOL WINAPI CreateProcessA(const char *, char *, void *, void *, BOOL, u32, void *, const char *, void *, void *);

/* el registro: no hay */
API(i32) RegOpenKeyExA(HKEY k, const char *s, u32 o, u32 a, HKEY *r) { (void)k; (void)s; (void)o; (void)a; *r = 0; return 2; }
API(i32) RegOpenKeyExW(HKEY k, const WCHAR *s, u32 o, u32 a, HKEY *r) { (void)k; (void)s; (void)o; (void)a; *r = 0; return 2; }
API(i32) RegOpenKeyA(HKEY k, const char *s, HKEY *r) { (void)k; (void)s; *r = 0; return 2; }
API(i32) RegCreateKeyExA(HKEY k, const char *s, u32 r, char *c, u32 o, u32 a, void *sg, HKEY *res, u32 *d) { (void)k; (void)s; (void)r; (void)c; (void)o; (void)a; (void)sg; *res = (HKEY)0x80001234; if (d) *d = 1; return 0; }
API(i32) RegCreateKeyA(HKEY k, const char *s, HKEY *r) { (void)k; (void)s; *r = (HKEY)0x80001234; return 0; }
API(i32) RegCloseKey(HKEY k) { (void)k; return 0; }
API(i32) RegQueryValueExA(HKEY k, const char *n, u32 *r, u32 *t, u8 *d, u32 *l) { (void)k; (void)n; (void)r; (void)t; (void)d; (void)l; return 2; }
API(i32) RegQueryValueExW(HKEY k, const WCHAR *n, u32 *r, u32 *t, u8 *d, u32 *l) { (void)k; (void)n; (void)r; (void)t; (void)d; (void)l; return 2; }
API(i32) RegQueryValueA(HKEY k, const char *s, char *d, i32 *l) { (void)k; (void)s; (void)d; (void)l; return 2; }
API(i32) RegSetValueExA(HKEY k, const char *n, u32 r, u32 t, const u8 *d, u32 l) { (void)k; (void)n; (void)r; (void)t; (void)d; (void)l; return 0; }
API(i32) RegDeleteKeyA(HKEY k, const char *s) { (void)k; (void)s; return 0; }
API(i32) RegDeleteValueA(HKEY k, const char *s) { (void)k; (void)s; return 0; }
API(i32) RegEnumKeyExA(HKEY k, u32 i, char *n, u32 *l, u32 *r, char *c, u32 *cl, void *t) { (void)k; (void)i; (void)n; (void)l; (void)r; (void)c; (void)cl; (void)t; return 259; }
API(i32) RegEnumValueA(HKEY k, u32 i, char *n, u32 *l, u32 *r, u32 *t, u8 *d, u32 *dl) { (void)k; (void)i; (void)n; (void)l; (void)r; (void)t; (void)d; (void)dl; return 259; }
API(i32) RegFlushKey(HKEY k) { (void)k; return 0; }
API(BOOL) GetUserNameA(char *d, u32 *n)
{
	char b[64];
	b[0] = 0;
	sc(68, 1, LIN(b), 0);
	if (!b[0])
		strcpy(b, "Usuario");
	{
		char c[64];
		a1252(c, b, sizeof c);
		copia_n(d, c, *n);
	}
	*n = strlen(d) + 1;
	return TRUE;
}
API(BOOL) OpenProcessToken(HANDLE p, u32 a, HANDLE *t) { (void)p; (void)a; *t = 0; return FALSE; }
API(BOOL) AllocateAndInitializeSid(void *a, u8 n, u32 s0, u32 s1, u32 s2, u32 s3, u32 s4, u32 s5, u32 s6, u32 s7, void **r) { (void)a; (void)n; (void)s0; (void)s1; (void)s2; (void)s3; (void)s4; (void)s5; (void)s6; (void)s7; *r = 0; return FALSE; }
API(void *) FreeSid(void *s) { (void)s; return 0; }

/* == SHELL32 == */
API(void) DragAcceptFiles(HWND h, BOOL a) { (void)h; (void)a; }
API(u32) DragQueryFile(HANDLE d, u32 i, char *b, u32 n) { (void)d; (void)i; (void)b; (void)n; return 0; }
API(u32) DragQueryFileA(HANDLE d, u32 i, char *b, u32 n) { (void)d; (void)i; (void)b; (void)n; return 0; }
API(void) DragFinish(HANDLE d) { (void)d; }
API(HINSTANCE) ShellExecuteA(HWND h, const char *op, const char *f, const char *p, const char *dir, i32 v)
{
	char l[400];
	(void)h; (void)op; (void)v;
	fmt(l, sizeof l, "%s%s%s", f ? f : "", p ? " " : "", p ? p : "");
	if (CreateProcessA(0, l, 0, 0, 0, 0, 0, dir, 0, 0))
		return (HINSTANCE)33;
	return (HINSTANCE)2;
}
API(void *) SHBrowseForFolderA(void *b) { (void)b; return 0; }
API(BOOL) SHGetPathFromIDListA(void *l, char *d) { (void)l; d[0] = 0; return FALSE; }
API(i32) SHGetSpecialFolderPathA(HWND h, char *d, i32 c, BOOL cr) { (void)h; (void)c; (void)cr; strcpy(d, carpeta_w32); return TRUE; }
API(i32) SHGetFolderPathA(HWND h, i32 c, HANDLE t, u32 f, char *d) { (void)h; (void)c; (void)t; (void)f; strcpy(d, carpeta_w32); if (strlen(d) > 3) d[strlen(d) - 1] = 0; return 0; }
API(HICON) ExtractIconA(HINSTANCE i, const char *f, u32 n) { (void)i; (void)f; (void)n; return 0; }
API(u32) SHGetFileInfoA(const char *p, u32 a, void *i, u32 n, u32 f) { (void)p; (void)a; (void)f; memset(i, 0, n); return 0; }
API(BOOL) Shell_NotifyIconA(u32 m, void *d) { (void)m; (void)d; return TRUE; }
API(void) SHChangeNotify(i32 e, u32 f, const void *a, const void *b) { (void)e; (void)f; (void)a; (void)b; }

/* == VERSION == */
API(u32) GetFileVersionInfoSizeA(const char *f, u32 *h) { (void)f; if (h) *h = 0; return 0; }
API(BOOL) GetFileVersionInfoA(const char *f, u32 h, u32 n, void *d) { (void)f; (void)h; (void)n; (void)d; return FALSE; }
API(BOOL) VerQueryValueA(const void *b, const char *s, void **d, u32 *n) { (void)b; (void)s; *d = 0; *n = 0; return FALSE; }

/* == WINSPOOL == */
API(BOOL) EnumPrintersA(u32 f, char *n, u32 l, u8 *b, u32 c, u32 *nec, u32 *r) { (void)f; (void)n; (void)l; (void)b; (void)c; *nec = 0; *r = 0; return TRUE; }
API(BOOL) OpenPrinterA(char *n, HANDLE *h, void *d) { (void)n; (void)d; *h = 0; return FALSE; }
API(BOOL) ClosePrinter(HANDLE h) { (void)h; return TRUE; }
