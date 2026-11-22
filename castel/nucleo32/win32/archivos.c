/* archivos.c - WIN32.BIN: la parte de KERNEL32 de los archivos, las carpetas,
 * las búsquedas y la consola, sobre las llamadas de Hispa-Pentium (7 a 28).
 * Los nombres van al núcleo en su página (la 437) y vuelven en la 1252; las
 * búsquedas ven los nombres largos (la llamada 28); crear, de momento, es
 * de 8+3, como el núcleo.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "w32.h"
#include "k32.h"

#define API(t) t WINAPI
typedef struct { u32 lo, hi; } FILETIME;
u32 WINAPI GetLastError(void);
BOOL WINAPI DosDateTimeToFileTime(u16 fe, u16 ho, FILETIME *f);
BOOL WINAPI FileTimeToDosDateTime(const FILETIME *f, u16 *fe, u16 *ho);
void w32_salir(u32 c);
void dx_tic(void);

void prepara_objs(void)
{
	objs[0].tipo = O_CONSOLA_ENT;
	objs[1].tipo = O_CONSOLA_SAL;
	objs[2].tipo = O_CONSOLA_SAL;
}
void cerrar_todo(void)
{
	int i;
	for (i = 3; i < MAX_OBJ; i++)
		if (objs[i].tipo == O_ARCHIVO || objs[i].tipo == O_BUSQUEDA) {
			sys_cerrar(objs[i].fd);
			objs[i].tipo = 0;
		}
}
static void nombre_437(const char *n, char *d, u32 cabe)
{
	char e[300];
	ruta_entera(n, e, sizeof e);
	a437(d, e, cabe);
}

/* ---- abrir y cerrar ---- */
API(HANDLE) CreateFileA(const char *n, u32 acceso, u32 comp, void *seg, u32 como, u32 fl, HANDLE plan)
{
	char c[300];
	u32 m, modo;
	int existe;
	HANDLE h;
	Entrada e;
	(void)comp; (void)seg; (void)plan;
	if (!n || !n[0]) {
		pon_error(3);
		return INVALID_HANDLE_VALUE;
	}
	if (!stricmp_(n, "CON") || !stricmp_(n, "CONOUT$"))
		return (HANDLE)(acceso & 0x40000000 ? -11 : -10);
	if (!stricmp_(n, "CONIN$"))
		return (HANDLE)-10;
	if (!stricmp_(n, "NUL")) {
		h = nuevo_obj(O_ARCHIVO);
		if (h)
			obj(h)->fd = 0xFFFFFFFF;
		return h ? h : INVALID_HANDLE_VALUE;
	}
	nombre_437(n, c, sizeof c);
	existe = entrada_de(n, &e, 0);
	if (existe && (e.atributos & 0x10) && !(fl & 0x02000000)) {
		pon_error(5);
		return INVALID_HANDLE_VALUE;
	}
	switch (como) {
	case 1:                                    /* CREATE_NEW */
		if (existe) { pon_error(80); return INVALID_HANDLE_VALUE; }
		modo = 2;
		break;
	case 2: modo = 2; break;                   /* CREATE_ALWAYS */
	case 3:                                    /* OPEN_EXISTING */
		if (!existe) { pon_error(2); return INVALID_HANDLE_VALUE; }
		modo = (acceso & 0x40000000) ? 1 : 0;
		break;
	case 4:                                    /* OPEN_ALWAYS */
		modo = existe ? ((acceso & 0x40000000) ? 1 : 0) : 2;
		break;
	case 5:                                    /* TRUNCATE_EXISTING */
		if (!existe) { pon_error(2); return INVALID_HANDLE_VALUE; }
		modo = 2;
		break;
	default:
		pon_error(87);
		return INVALID_HANDLE_VALUE;
	}
	m = sys_abrir(c, modo);
	traza("CreateFileA(%s, %x, %u) = %x", n, acceso, como, m);
	if (ES_ERROR(m)) {
		pon_error(error_de(m));
		return INVALID_HANDLE_VALUE;
	}
	h = nuevo_obj(O_ARCHIVO);
	if (!h) {
		sys_cerrar(m);
		return INVALID_HANDLE_VALUE;
	}
	obj(h)->fd = m;
	obj(h)->escribe = modo != 0;
	pon_error(existe && (como == 2 || como == 4) ? 183 : 0);
	return h;
}
API(HANDLE) CreateFileW(const WCHAR *n, u32 a, u32 c, void *s, u32 como, u32 f, HANDLE p)
{
	char b[300];
	de_w(b, n, sizeof b);
	return CreateFileA(b, a, c, s, como, f, p);
}
API(BOOL) CloseHandle(HANDLE h)
{
	Obj *o = obj(h);
	if (!o) {
		if (h == (HANDLE)-1 || h == (HANDLE)-2)
			return TRUE;
		pon_error(6);
		return FALSE;
	}
	if ((u32)h < 0x100C)
		return TRUE;          /* (la consola) */
	if ((o->tipo == O_ARCHIVO || o->tipo == O_BUSQUEDA) && o->fd != 0xFFFFFFFF)
		sys_cerrar(o->fd);
	if (o->tipo == O_MAPA)
		m_soltar(o->f);
	if (o->tipo == O_HILO)          /* (JUEGOS) */
		hilo_cierra(o);
	if ((o->tipo == O_EVENTO || o->tipo == O_MUTEX || o->tipo == O_SEMAFORO) && !sinc_cierra(o))
		return TRUE;            /* (abierto otra vez por su nombre: sigue) */
	o->tipo = 0;
	return TRUE;
}
API(u32) _lclose(HANDLE h) { return CloseHandle(h) ? 0 : (u32)-1; }

/* ---- la consola ---- */
static char ent_linea[256];
static u32 ent_n, ent_pos;
static u32 cons_modo = 7;
static void leer_linea(void)
{
	u32 n = 0;
	for (;;) {
		u32 c = sys_tecla(1);
		if (c == '\n' || c == '\r') {
			sys_escribir("\n", 1);
			break;
		}
		if (c == 8) {
			if (n) {
				n--;
				sys_escribir("\b \b", 3);
			}
			continue;
		}
		if (c == 3 || c == 26) {
			if (!n) {
				ent_n = ent_pos = 0;
				return;
			}
			continue;
		}
		if (c >= 0x100 || c < 32)
			continue;
		if (n + 3 < sizeof ent_linea) {
			char k = (char)c;
			ent_linea[n++] = k;
			sys_escribir(&k, 1);
		}
	}
	ent_linea[n++] = '\r';
	ent_linea[n++] = '\n';
	ent_n = n;
	ent_pos = 0;
}
static u32 leer_consola(char *d, u32 n)
{
	u32 k = 0;
	if (!(cons_modo & 2)) {
		/* sin ENABLE_LINE_INPUT: tecla a tecla */
		while (k < n) {
			u32 c = sys_tecla(k == 0);
			if (!c)
				break;
			if (c >= 0x100)
				continue;
			d[k++] = c == '\n' ? '\r' : (char)c;
		}
		return k;
	}
	if (ent_pos >= ent_n)
		leer_linea();
	while (k < n && ent_pos < ent_n)
		d[k++] = ent_linea[ent_pos++];
	return k;
}
API(HANDLE) GetStdHandle(u32 n)
{
	if (n == (u32)-10 || n == (u32)-11 || n == (u32)-12) {
		Obj *o = obj((HANDLE)n);
		if (o && (o->tipo == O_ARCHIVO))
			return (HANDLE)(0x1000 + (n == (u32)-10 ? 0 : n == (u32)-11 ? 4 : 8));
		return (HANDLE)n;
	}
	pon_error(6);
	return INVALID_HANDLE_VALUE;
}
API(BOOL) SetStdHandle(u32 n, HANDLE h)
{
	Obj *o = obj(h);
	int i = n == (u32)-10 ? 0 : n == (u32)-11 ? 1 : n == (u32)-12 ? 2 : -1;
	if (i < 0 || !o)
		return FALSE;
	objs[i] = *o;
	return TRUE;
}
API(BOOL) WriteConsoleA(HANDLE h, const void *p, u32 n, u32 *esc, void *r)
{
	(void)h; (void)r;
	sys_escribir(p, n);
	if (esc)
		*esc = n;
	return TRUE;
}
API(BOOL) WriteConsoleW(HANDLE h, const WCHAR *p, u32 n, u32 *esc, void *r)
{
	char b[256];
	u32 k = 0, t = 0;
	(void)h; (void)r;
	while (t < n) {
		u8 c = de_uni(p[t++]);
		b[k++] = c < 128 ? c : t1252_437[c - 128];
		if (k == sizeof b) {
			sys_escribir(b, k);
			k = 0;
		}
	}
	if (k)
		sys_escribir(b, k);
	if (esc)
		*esc = n;
	return TRUE;
}
API(BOOL) ReadConsoleA(HANDLE h, void *d, u32 n, u32 *leidos, void *c)
{
	(void)h; (void)c;
	*leidos = leer_consola(d, n);
	return TRUE;
}
API(BOOL) ReadConsoleW(HANDLE h, WCHAR *d, u32 n, u32 *leidos, void *c)
{
	char b[256];
	u32 k, r;
	(void)h; (void)c;
	r = leer_consola(b, n < sizeof b ? n : sizeof b);
	for (k = 0; k < r; k++)
		d[k] = a_uni((u8)b[k] < 128 ? b[k] : t437_1252[(u8)b[k] - 128]);
	*leidos = r;
	return TRUE;
}
API(BOOL) GetConsoleMode(HANDLE h, u32 *m)
{
	Obj *o = obj(h);
	if (!o || (o->tipo != O_CONSOLA_ENT && o->tipo != O_CONSOLA_SAL)) {
		pon_error(6);
		return FALSE;
	}
	*m = o->tipo == O_CONSOLA_ENT ? cons_modo : 3;
	return TRUE;
}
API(BOOL) SetConsoleMode(HANDLE h, u32 m)
{
	Obj *o = obj(h);
	if (o && o->tipo == O_CONSOLA_ENT)
		cons_modo = m;
	return o != 0;
}
typedef struct { i16 x, y; } COORD;
typedef struct { COORD tam, cursor; u16 color; i16 izq, arr, der, aba; COORD max; } CSBI;
API(BOOL) GetConsoleScreenBufferInfo(HANDLE h, CSBI *b)
{
	Obj *o = obj(h);
	if (!o || o->tipo != O_CONSOLA_SAL) {
		pon_error(6);
		return FALSE;
	}
	b->tam.x = 80; b->tam.y = 25;
	b->cursor.x = 0; b->cursor.y = 24;
	b->color = 7;
	b->izq = 0; b->arr = 0; b->der = 79; b->aba = 24;
	b->max.x = 80; b->max.y = 25;
	return TRUE;
}
API(BOOL) SetConsoleTextAttribute(HANDLE h, u16 c) { (void)h; (void)c; return TRUE; }
API(BOOL) SetConsoleCursorPosition(HANDLE h, COORD c) { (void)h; (void)c; return TRUE; }
API(BOOL) GetConsoleCursorInfo(HANDLE h, u32 *i) { (void)h; i[0] = 25; i[1] = 1; return TRUE; }
API(BOOL) SetConsoleCursorInfo(HANDLE h, const u32 *i) { (void)h; (void)i; return TRUE; }
API(BOOL) SetConsoleCtrlHandler(void *f, BOOL a) { (void)f; (void)a; return TRUE; }
API(BOOL) AllocConsole(void) { return TRUE; }
API(BOOL) FreeConsole(void) { return TRUE; }
API(BOOL) SetConsoleTitleA(const char *t) { (void)t; return TRUE; }
API(u32) GetConsoleTitleA(char *d, u32 n) { copia_n(d, "Consola", n); return strlen(d); }
API(BOOL) FillConsoleOutputCharacterA(HANDLE h, char c, u32 n, COORD p, u32 *esc) { (void)h; (void)c; (void)p; if (esc) *esc = n; return TRUE; }
API(BOOL) FillConsoleOutputAttribute(HANDLE h, u16 a, u32 n, COORD p, u32 *esc) { (void)h; (void)a; (void)p; if (esc) *esc = n; return TRUE; }
API(BOOL) FlushConsoleInputBuffer(HANDLE h) { (void)h; while (sys_tecla(0)) ; ent_n = ent_pos = 0; return TRUE; }
API(HANDLE) GetConsoleWindow(void) { return 0; }
/* las teclas como sucesos (KEY_EVENT_RECORD) */
static u32 tecla_guardada;
static u16 vk_de(u32 c, u16 *sc)
{
	static const u8 vks[0x54] = {
		0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', 0xBD, 0xBB, 8, 9,
		'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', 0xDB, 0xDD, 13, 17, 'A', 'S',
		'D', 'F', 'G', 'H', 'J', 'K', 'L', 0xBA, 0xDE, 0xC0, 16, 0xDC, 'Z', 'X', 'C', 'V',
		'B', 'N', 'M', 0xBC, 0xBE, 0xBF, 16, 0x6A, 18, 32, 20, 0x70, 0x71, 0x72, 0x73, 0x74,
		0x75, 0x76, 0x77, 0x78, 0x79, 0x90, 0x91, 0x24, 0x26, 0x21, 0x6D, 0x25, 0x0C, 0x27, 0x6B, 0x23,
		0x28, 0x22, 0x2D, 0x2E };
	*sc = 0;
	if (c >= 0x100) {
		u32 s = c & 0xFF;
		*sc = s;
		return s < 0x54 ? vks[s] : 0;
	}
	if (c == '\n' || c == '\r') { *sc = 0x1C; return 13; }
	if (c == 8) { *sc = 0x0E; return 8; }
	if (c == 9) { *sc = 0x0F; return 9; }
	if (c == 27) { *sc = 1; return 27; }
	if (c == ' ') { *sc = 0x39; return 32; }
	if (c >= 'a' && c <= 'z') return c - 32;
	if (c >= 'A' && c <= 'Z') return c;
	if (c >= '0' && c <= '9') return c;
	return 0;
}
static void pon_suceso(u8 *r, u32 c)
{
	u16 sc, vk = vk_de(c, &sc);
	memset(r, 0, 20);
	r[0] = 1;                        /* KEY_EVENT */
	*(u32 *)(r + 4) = 1;             /* bKeyDown */
	*(u16 *)(r + 8) = 1;
	*(u16 *)(r + 10) = vk;
	*(u16 *)(r + 12) = sc;
	r[14] = c < 0x100 ? (c == '\n' ? '\r' : (u8)c) : 0;
}
API(BOOL) GetNumberOfConsoleInputEvents(HANDLE h, u32 *n)
{
	(void)h;
	if (!tecla_guardada)
		tecla_guardada = sys_tecla(0);
	*n = tecla_guardada ? 1 : 0;
	return TRUE;
}
API(BOOL) PeekConsoleInputA(HANDLE h, u8 *r, u32 n, u32 *leidos)
{
	(void)h;
	if (!tecla_guardada)
		tecla_guardada = sys_tecla(0);
	if (!tecla_guardada || !n) {
		*leidos = 0;
		return TRUE;
	}
	pon_suceso(r, tecla_guardada);
	*leidos = 1;
	return TRUE;
}
API(BOOL) ReadConsoleInputA(HANDLE h, u8 *r, u32 n, u32 *leidos)
{
	(void)h;
	if (!n) {
		*leidos = 0;
		return TRUE;
	}
	if (!tecla_guardada)
		tecla_guardada = sys_tecla(1);
	pon_suceso(r, tecla_guardada);
	tecla_guardada = 0;
	*leidos = 1;
	return TRUE;
}
API(BOOL) ReadConsoleInputW(HANDLE h, u8 *r, u32 n, u32 *leidos) { return ReadConsoleInputA(h, r, n, leidos); }
API(BOOL) PeekConsoleInputW(HANDLE h, u8 *r, u32 n, u32 *leidos) { return PeekConsoleInputA(h, r, n, leidos); }

/* ---- leer y escribir ---- */
API(BOOL) ReadFile(HANDLE h, void *d, u32 n, u32 *leidos, void *sol)
{
	Obj *o = obj(h);
	u32 r;
	(void)sol;
	if (leidos)
		*leidos = 0;
	if (!o) {
		pon_error(6);
		return FALSE;
	}
	if (o->tipo == O_CONSOLA_ENT) {
		r = leer_consola(d, n);
		if (leidos)
			*leidos = r;
		return TRUE;
	}
	if (o->tipo != O_ARCHIVO) {
		pon_error(6);
		return FALSE;
	}
	if (o->fd == 0xFFFFFFFF)
		return TRUE;
	r = sys_leer(o->fd, d, n);
	if (ES_ERROR(r)) {
		pon_error(error_de(r));
		return FALSE;
	}
	if (leidos)
		*leidos = r;
	return TRUE;
}
API(BOOL) WriteFile(HANDLE h, const void *p, u32 n, u32 *esc, void *sol)
{
	Obj *o = obj(h);
	u32 r;
	(void)sol;
	if (esc)
		*esc = 0;
	if (!o) {
		pon_error(6);
		return FALSE;
	}
	if (o->tipo == O_CONSOLA_SAL) {
		sys_escribir(p, n);
		if (esc)
			*esc = n;
		return TRUE;
	}
	if (o->tipo != O_ARCHIVO) {
		pon_error(6);
		return FALSE;
	}
	if (o->fd == 0xFFFFFFFF) {
		if (esc)
			*esc = n;
		return TRUE;
	}
	if (!o->escribe) {
		pon_error(5);
		return FALSE;
	}
	r = n ? sys_escribir_archivo(o->fd, p, n) : 0;
	if (ES_ERROR(r)) {
		pon_error(error_de(r));
		return FALSE;
	}
	if (esc)
		*esc = r;
	if (r < n) {
		pon_error(112);
		return FALSE;
	}
	return TRUE;
}
API(u32) SetFilePointer(HANDLE h, i32 d, i32 *alto, u32 desde)
{
	Obj *o = obj(h);
	u32 r;
	if (!o || o->tipo != O_ARCHIVO) {
		if (o && o->tipo != O_ARCHIVO)
			return 0;
		pon_error(6);
		return 0xFFFFFFFF;
	}
	if (alto) {
		if (*alto && !(*alto == -1 && d < 0)) {
			pon_error(87);
			return 0xFFFFFFFF;
		}
		*alto = 0;
	}
	if (o->fd == 0xFFFFFFFF)
		return 0;
	if (desde == 0 && d < 0) {
		pon_error(131);
		return 0xFFFFFFFF;
	}
	r = sys_mover(o->fd, d, desde);
	if (ES_ERROR(r)) {
		pon_error(error_de(r));
		return 0xFFFFFFFF;
	}
	pon_error(0);
	return r;
}
API(BOOL) SetFilePointerEx(HANDLE h, i64 d, i64 *nueva, u32 desde)
{
	u32 r = SetFilePointer(h, (i32)d, 0, desde);
	if (r == 0xFFFFFFFF && GetLastError())
		return FALSE;
	if (nueva)
		*nueva = r;
	return TRUE;
}
API(u32) GetFileSize(HANDLE h, u32 *alto)
{
	Obj *o = obj(h);
	if (alto)
		*alto = 0;
	if (!o || o->tipo != O_ARCHIVO) {
		pon_error(6);
		return 0xFFFFFFFF;
	}
	if (o->fd == 0xFFFFFFFF)
		return 0;
	return sys_largo(o->fd);
}
API(BOOL) GetFileSizeEx(HANDLE h, i64 *t)
{
	u32 r = GetFileSize(h, 0);
	if (r == 0xFFFFFFFF)
		return FALSE;
	*t = r;
	return TRUE;
}
API(u32) GetFileType(HANDLE h)
{
	Obj *o = obj(h);
	if (!o)
		return 0;
	return o->tipo == O_ARCHIVO ? (o->fd == 0xFFFFFFFF ? 2 : 1) : (o->tipo == O_CONSOLA_ENT || o->tipo == O_CONSOLA_SAL) ? 2 : 0;
}
API(BOOL) FlushFileBuffers(HANDLE h) { return obj(h) != 0; }
API(BOOL) SetEndOfFile(HANDLE h)
{
	Obj *o = obj(h);
	u32 r;
	if (!o || o->tipo != O_ARCHIVO) {
		pon_error(6);
		return FALSE;
	}
	r = sys_cortar(o->fd);
	if (ES_ERROR(r)) {
		pon_error(error_de(r));
		return FALSE;
	}
	return TRUE;
}
API(BOOL) LockFile(HANDLE h, u32 a, u32 b, u32 c, u32 d) { (void)h; (void)a; (void)b; (void)c; (void)d; return TRUE; }
API(BOOL) UnlockFile(HANDLE h, u32 a, u32 b, u32 c, u32 d) { (void)h; (void)a; (void)b; (void)c; (void)d; return TRUE; }
API(BOOL) GetFileTime(HANDLE h, FILETIME *c, FILETIME *a, FILETIME *e)
{
	FILETIME f;
	(void)h;
	DosDateTimeToFileTime(0x5B75, 0, &f);    /* (el núcleo no da la fecha de un archivo abierto) */
	if (c) *c = f;
	if (a) *a = f;
	if (e) *e = f;
	return TRUE;
}
API(BOOL) SetFileTime(HANDLE h, const FILETIME *c, const FILETIME *a, const FILETIME *e) { (void)h; (void)c; (void)a; (void)e; return TRUE; }
API(BOOL) GetFileInformationByHandle(HANDLE h, u32 *i)
{
	Obj *o = obj(h);
	FILETIME f;
	if (!o || o->tipo != O_ARCHIVO)
		return FALSE;
	memset(i, 0, 52);
	i[0] = 0x20;
	DosDateTimeToFileTime(0x5B75, 0, &f);
	i[1] = i[3] = i[5] = f.lo;
	i[2] = i[4] = i[6] = f.hi;
	i[7] = 0x48495350;
	i[9] = sys_largo(o->fd);
	i[10] = 1;
	i[12] = (u32)h;
	return TRUE;
}

/* ---- borrar, mover, copiar, carpetas ---- */
static BOOL resultado(u32 r)
{
	if (ES_ERROR(r)) {
		pon_error(error_de(r));
		return FALSE;
	}
	return TRUE;
}
API(BOOL) DeleteFileA(const char *n) { char c[300]; nombre_437(n, c, sizeof c); traza("DeleteFileA(%s)", n); return resultado(sys_borrar(c)); }
API(BOOL) DeleteFileW(const WCHAR *n) { char b[300]; de_w(b, n, sizeof b); return DeleteFileA(b); }
API(BOOL) MoveFileA(const char *a, const char *b)
{
	char c[300], d[300];
	nombre_437(a, c, sizeof c);
	nombre_437(b, d, sizeof d);
	return resultado(sys_renombrar(c, d));
}
API(BOOL) MoveFileExA(const char *a, const char *b, u32 fl)
{
	if (!b)
		return DeleteFileA(a);
	if (fl & 1) {
		Entrada e;
		if (entrada_de(b, &e, 0))
			DeleteFileA(b);
	}
	return MoveFileA(a, b);
}
API(BOOL) MoveFileW(const WCHAR *a, const WCHAR *b) { char x[300], y[300]; de_w(x, a, sizeof x); de_w(y, b, sizeof y); return MoveFileA(x, y); }
API(BOOL) MoveFileExW(const WCHAR *a, const WCHAR *b, u32 f) { char x[300], y[300]; de_w(x, a, sizeof x); if (b) de_w(y, b, sizeof y); return MoveFileExA(x, b ? y : 0, f); }
API(BOOL) CopyFileA(const char *a, const char *b, BOOL si_esta)
{
	char c[300], d[300], *buf;
	u32 m1, m2, n;
	Entrada e;
	if (si_esta && entrada_de(b, &e, 0)) {
		pon_error(80);
		return FALSE;
	}
	nombre_437(a, c, sizeof c);
	nombre_437(b, d, sizeof d);
	m1 = sys_abrir(c, 0);
	if (ES_ERROR(m1))
		return resultado(m1);
	m2 = sys_abrir(d, 2);
	if (ES_ERROR(m2)) {
		sys_cerrar(m1);
		return resultado(m2);
	}
	buf = m_pedir(32768);
	while ((n = sys_leer(m1, buf, 32768)) && !ES_ERROR(n))
		if (sys_escribir_archivo(m2, buf, n) != n) {
			m_soltar(buf);
			sys_cerrar(m1);
			sys_cerrar(m2);
			pon_error(112);
			return FALSE;
		}
	m_soltar(buf);
	sys_cerrar(m1);
	sys_cerrar(m2);
	return TRUE;
}
API(BOOL) CopyFileW(const WCHAR *a, const WCHAR *b, BOOL s) { char x[300], y[300]; de_w(x, a, sizeof x); de_w(y, b, sizeof y); return CopyFileA(x, y, s); }
API(BOOL) CreateDirectoryA(const char *n, void *s)
{
	char c[300];
	u32 r;
	(void)s;
	nombre_437(n, c, sizeof c);
	r = sys_crear_carpeta(c);
	if (r == E_YA_EXISTE) {
		pon_error(183);
		return FALSE;
	}
	return resultado(r);
}
API(BOOL) CreateDirectoryW(const WCHAR *n, void *s) { char b[300]; de_w(b, n, sizeof b); return CreateDirectoryA(b, s); }
API(BOOL) RemoveDirectoryA(const char *n) { char c[300]; nombre_437(n, c, sizeof c); return resultado(sys_borrar_carpeta(c)); }
API(BOOL) RemoveDirectoryW(const WCHAR *n) { char b[300]; de_w(b, n, sizeof b); return RemoveDirectoryA(b); }
API(u32) GetFileAttributesA(const char *n)
{
	Entrada e;
	if (!n) {
		pon_error(87);
		return 0xFFFFFFFF;
	}
	if (!entrada_de(n, &e, 0)) {
		char c[300];
		/* ¿una carpeta que solo se ve abriéndola? */
		pon_error(2);
		(void)c;
		return 0xFFFFFFFF;
	}
	return e.atributos & 0x37 ? (u32)(e.atributos & 0x37) : 0x80;
}
API(u32) GetFileAttributesW(const WCHAR *n) { char b[300]; de_w(b, n, sizeof b); return GetFileAttributesA(b); }
typedef struct { u32 at; FILETIME c, a, e; u32 alto, bajo; } WIN32_FILE_ATTRIBUTE_DATA;
API(BOOL) GetFileAttributesExA(const char *n, u32 nivel, WIN32_FILE_ATTRIBUTE_DATA *d)
{
	Entrada e;
	(void)nivel;
	if (!entrada_de(n, &e, 0)) {
		pon_error(2);
		return FALSE;
	}
	d->at = e.atributos & 0x37 ? (u32)(e.atributos & 0x37) : 0x80;
	DosDateTimeToFileTime(e.fecha, e.hora, &d->e);
	d->c = d->a = d->e;
	d->alto = 0;
	d->bajo = e.tam;
	return TRUE;
}
API(BOOL) GetFileAttributesExW(const WCHAR *n, u32 nivel, WIN32_FILE_ATTRIBUTE_DATA *d) { char b[300]; de_w(b, n, sizeof b); return GetFileAttributesExA(b, nivel, d); }
API(BOOL) SetFileAttributesA(const char *n, u32 a) { Entrada e; (void)a; return entrada_de(n, &e, 0); }
API(BOOL) SetFileAttributesW(const WCHAR *n, u32 a) { char b[300]; de_w(b, n, sizeof b); return SetFileAttributesA(b, a); }

/* ---- buscar ---- */
typedef struct {
	u32 at;
	FILETIME c, a, e;
	u32 alto, bajo, r0, r1;
	char nombre[260];
	char corto[14];
} WIN32_FIND_DATAA;
static int casa_patron(const char *p, const char *n)
{
	if (!strcmp(p, "*.*") || !strcmp(p, "*"))
		return 1;
	for (; *p; p++, n++) {
		if (*p == '*') {
			const char *q;
			p++;
			if (!*p)
				return 1;
			for (q = n; ; q++) {
				if (casa_patron(p, q))
					return 1;
				if (!*q)
					return 0;
			}
		}
		if (*p == '?') {
			if (!*n)
				return !strcmp(p, "?") || casa_patron(p + 1, n);
			continue;
		}
		if (mayus(*p) != mayus(*n)) {
			/* «nombre.*» y «nombre.» valen para un nombre sin extensión */
			if (!*n && *p == '.' && (!p[1] || !strcmp(p + 1, "*")))
				return 1;
			return 0;
		}
	}
	return !*n;
}
static int busca_siguiente(Obj *o, WIN32_FIND_DATAA *d)
{
	Entrada e;
	char l[260];
	for (;;) {
		l[0] = 0;
		if (sys_entrada_larga(o->fd, &e, l) != 1)
			return 0;
		if (e.atributos & 8)
			continue;
		memset(d, 0, sizeof *d);
		a1252(d->corto, e.nombre, sizeof d->corto);
		if (l[0])
			a1252(d->nombre, l, sizeof d->nombre);
		else
			strcpy(d->nombre, d->corto);
		if (!casa_patron(o->patron, d->nombre) && !casa_patron(o->patron, d->corto))
			continue;
		if (!strcmp(d->nombre, d->corto))
			d->corto[0] = 0;
		d->at = e.atributos & 0x37 ? (u32)(e.atributos & 0x37) : 0x80;
		DosDateTimeToFileTime(e.fecha, e.hora, &d->e);
		d->c = d->a = d->e;
		d->bajo = e.tam;
		return 1;
	}
}
API(HANDLE) FindFirstFileA(const char *ruta, WIN32_FIND_DATAA *d)
{
	char e[300], carpeta[300], *b;
	HANDLE h;
	u32 m;
	ruta_entera(ruta, e, sizeof e);
	b = strrchr(e, '\\');
	if (!b) {
		pon_error(3);
		return INVALID_HANDLE_VALUE;
	}
	h = nuevo_obj(O_BUSQUEDA);
	if (!h)
		return INVALID_HANDLE_VALUE;
	copia_n(obj(h)->patron, b + 1, sizeof obj(h)->patron);
	if (!obj(h)->patron[0])
		strcpy(obj(h)->patron, "*");
	if (b == e + 2)
		b[1] = 0;
	else
		*b = 0;
	a437(carpeta, e, sizeof carpeta);
	m = sys_abrir(carpeta, 0);
	traza("FindFirstFileA(%s): %s %s = %x", ruta, carpeta, obj(h)->patron, m);
	if (ES_ERROR(m)) {
		obj(h)->tipo = 0;
		pon_error(m == E_NO_HAY ? 3 : error_de(m));
		return INVALID_HANDLE_VALUE;
	}
	obj(h)->fd = m;
	if (!busca_siguiente(obj(h), d)) {
		sys_cerrar(m);
		obj(h)->tipo = 0;
		pon_error(2);
		return INVALID_HANDLE_VALUE;
	}
	return h;
}
API(BOOL) FindNextFileA(HANDLE h, WIN32_FIND_DATAA *d)
{
	Obj *o = obj(h);
	if (!o || o->tipo != O_BUSQUEDA) {
		pon_error(6);
		return FALSE;
	}
	if (!busca_siguiente(o, d)) {
		pon_error(18);              /* ERROR_NO_MORE_FILES */
		return FALSE;
	}
	return TRUE;
}
API(BOOL) FindClose(HANDLE h)
{
	Obj *o = obj(h);
	if (!o || o->tipo != O_BUSQUEDA)
		return FALSE;
	sys_cerrar(o->fd);
	o->tipo = 0;
	return TRUE;
}
/* las de Unicode: lo mismo, con los nombres en UTF-16 */
typedef struct { u32 at; FILETIME c, a, e; u32 alto, bajo, r0, r1; WCHAR nombre[260]; WCHAR corto[14]; } WIN32_FIND_DATAW;
static void a_w_find(WIN32_FIND_DATAA *a, WIN32_FIND_DATAW *w)
{
	memcpy(w, a, 44);
	a_w(w->nombre, a->nombre, 260);
	a_w(w->corto, a->corto, 14);
}
API(HANDLE) FindFirstFileW(const WCHAR *r, WIN32_FIND_DATAW *d)
{
	char b[300];
	WIN32_FIND_DATAA a;
	HANDLE h;
	de_w(b, r, sizeof b);
	h = FindFirstFileA(b, &a);
	if (h != INVALID_HANDLE_VALUE)
		a_w_find(&a, d);
	return h;
}
API(BOOL) FindNextFileW(HANDLE h, WIN32_FIND_DATAW *d)
{
	WIN32_FIND_DATAA a;
	if (!FindNextFileA(h, &a))
		return FALSE;
	a_w_find(&a, d);
	return TRUE;
}
API(HANDLE) FindFirstFileExA(const char *r, u32 n, WIN32_FIND_DATAA *d, u32 o, void *f, u32 x) { (void)n; (void)o; (void)f; (void)x; return FindFirstFileA(r, d); }
API(HANDLE) FindFirstChangeNotificationA(const char *r, BOOL s, u32 f) { (void)r; (void)s; (void)f; return nuevo_obj(O_CAMBIOS); }
API(BOOL) FindNextChangeNotification(HANDLE h) { (void)h; return TRUE; }
API(BOOL) FindCloseChangeNotification(HANDLE h) { return CloseHandle(h); }

/* ---- la carpeta de trabajo y las rutas ---- */
API(u32) GetCurrentDirectoryA(u32 n, char *d)
{
	char c[300];
	carpeta_de_trabajo(c, sizeof c);
	if (n <= strlen(c))
		return strlen(c) + 1;
	strcpy(d, c);
	return strlen(d);
}
API(u32) GetCurrentDirectoryW(u32 n, WCHAR *d)
{
	char c[300];
	u32 r = GetCurrentDirectoryA(sizeof c, c);
	if (n <= r)
		return r + 1;
	a_w(d, c, n);
	return r;
}
API(BOOL) SetCurrentDirectoryA(const char *d)
{
	char e[300], c[300];
	u32 r;
	ruta_entera(d, e, sizeof e);
	a437(c, e, sizeof c);
	r = sys_unidad(mayus(c[0]));
	if (ES_ERROR(r) || mayus(r) != mayus(c[0])) {
		pon_error(15);
		return FALSE;
	}
	r = sys_cambiar_carpeta(c + 2);
	traza("SetCurrentDirectoryA(%s) = %x", e, r);
	return resultado(r);
}
API(BOOL) SetCurrentDirectoryW(const WCHAR *d) { char b[300]; de_w(b, d, sizeof b); return SetCurrentDirectoryA(b); }
API(u32) GetFullPathNameA(const char *r, u32 n, char *d, char **parte)
{
	char e[300];
	ruta_entera(r, e, sizeof e);
	/* (con la barra del final si la traía) */
	if (r[0] && (r[strlen(r) - 1] == '\\' || r[strlen(r) - 1] == '/') && e[strlen(e) - 1] != '\\')
		strcat(e, "\\");
	if (n <= strlen(e))
		return strlen(e) + 1;
	strcpy(d, e);
	if (parte) {
		char *b = strrchr(d, '\\');
		*parte = b && b[1] ? b + 1 : 0;
	}
	return strlen(d);
}
API(u32) GetFullPathNameW(const WCHAR *r, u32 n, WCHAR *d, WCHAR **parte)
{
	char a[300], b[300];
	u32 k;
	de_w(a, r, sizeof a);
	k = GetFullPathNameA(a, sizeof b, b, 0);
	if (n <= k)
		return k + 1;
	a_w(d, b, n);
	if (parte) {
		WCHAR *p = 0, *q;
		for (q = d; *q; q++)
			if (*q == '\\')
				p = q + 1;
		*parte = p && *p ? p : 0;
	}
	return k;
}
API(u32) GetShortPathNameA(const char *a, char *b, u32 n) { copia_n(b, a, n); return strlen(b); }
API(u32) GetLongPathNameA(const char *a, char *b, u32 n) { copia_n(b, a, n); return strlen(b); }
API(u32) SearchPathA(const char *ruta, const char *n, const char *ext, u32 cabe, char *d, char **parte)
{
	char t[300], sitios[4][200];
	int k, ns = 0;
	Entrada e;
	if (ruta)
		copia_n(sitios[ns++], ruta, 200);
	else {
		char *p;
		copia_n(sitios[ns], ruta_exe, 200);
		p = strrchr(sitios[ns], '\\');
		if (p)
			*p = 0;
		ns++;
		GetCurrentDirectoryA(200, sitios[ns++]);
		copia_n(sitios[ns++], carpeta_w32, 200);
	}
	for (k = 0; k < ns; k++) {
		u32 l = strlen(sitios[k]);
		if (l && sitios[k][l - 1] == '\\')
			sitios[k][l - 1] = 0;
		if (strchr(n, '\\') || strchr(n, ':'))
			copia_n(t, n, sizeof t);
		else
			fmt(t, sizeof t, "%s\\%s", sitios[k], n);
		if (ext && !strchr(n, '.'))
			strcat(t, ext);
		if (entrada_de(t, &e, 0) && !(e.atributos & 0x10)) {
			char x[300];
			ruta_entera(t, x, sizeof x);
			if (cabe <= strlen(x))
				return strlen(x) + 1;
			strcpy(d, x);
			if (parte)
				*parte = strrchr(d, '\\') + 1;
			return strlen(d);
		}
	}
	pon_error(2);
	return 0;
}
API(u32) GetDriveTypeA(const char *r)
{
	char c[8];
	u32 u = 0;
	struct { u8 letra, clase, medio, sl; u32 total, libre, sist; char n[41], et[12]; u8 q, rel[2]; } i;
	if (!r || !r[0])
		return 3;
	c[0] = mayus(r[0]);
	for (u = 0; u < 8; u++)
		if (sc(43, u, LIN(&i), 0) == 1 && i.letra == (u8)c[0])
			return i.medio == 3 || i.q ? 2 : i.medio == 0 ? 6 : 3;
	return 1;
}
API(u32) GetDriveTypeW(const WCHAR *r) { char b[8]; de_w(b, r, sizeof b); return GetDriveTypeA(b); }
API(u32) GetLogicalDrives(void)
{
	u32 u, m = 0;
	struct { u8 letra, clase, medio, sl; u32 total, libre, sist; char n[41], et[12]; u8 q, rel[2]; } i;
	for (u = 0; u < 8; u++)
		if (sc(43, u, LIN(&i), 0) == 1 && i.letra >= 'A' && i.letra <= 'Z')
			m |= 1u << (i.letra - 'A');
	return m;
}
API(u32) GetLogicalDriveStringsA(u32 n, char *d)
{
	u32 m = GetLogicalDrives(), o = 0, k;
	for (k = 0; k < 26; k++)
		if (m & (1u << k)) {
			if (o + 5 > n)
				return 4 * 26 + 1;
			d[o++] = 'A' + k; d[o++] = ':'; d[o++] = '\\'; d[o++] = 0;
		}
	d[o] = 0;
	return o;
}
API(BOOL) GetDiskFreeSpaceA(const char *r, u32 *spc, u32 *bps, u32 *libres, u32 *total)
{
	u32 cl = sys_dato(6), li = sys_dato(7), to = sys_dato(8);
	(void)r;
	if (!cl)
		cl = 4096;
	if (spc) *spc = cl / 512;
	if (bps) *bps = 512;
	if (libres) *libres = li == 0xFFFFFFFF ? 0x10000 : li;
	if (total) *total = to;
	return TRUE;
}
API(BOOL) GetDiskFreeSpaceExA(const char *r, u64 *a, u64 *t, u64 *l)
{
	u32 spc, bps, li, to;
	GetDiskFreeSpaceA(r, &spc, &bps, &li, &to);
	if (a) *a = (u64)li * spc * bps;
	if (t) *t = (u64)to * spc * bps;
	if (l) *l = (u64)li * spc * bps;
	return TRUE;
}
API(BOOL) GetVolumeInformationA(const char *r, char *nom, u32 nn, u32 *serie, u32 *largo, u32 *fl, char *sist, u32 ns)
{
	(void)r;
	if (nom && nn) copia_n(nom, "HISPA", nn);
	if (serie) *serie = 0x48495350;
	if (largo) *largo = 255;
	if (fl) *fl = 3;
	if (sist && ns) copia_n(sist, "FAT", ns);
	return TRUE;
}
API(u32) GetTempFileNameA(const char *ruta, const char *pre, u32 u, char *d)
{
	static u32 n = 0x1A2;
	Entrada e;
	char p[4];
	copia_n(p, pre ? pre : "TMP", 4);
	for (;;) {
		u32 v = u ? u : ++n;
		fmt(d, 260, "%s%s%s%04X.TMP", ruta, ruta[0] && ruta[strlen(ruta) - 1] != '\\' ? "\\" : "", p, v & 0xFFFF);
		if (u || !entrada_de(d, &e, 0)) {
			if (!u) {
				HANDLE h = CreateFileA(d, 0x40000000, 0, 0, 1, 0, 0);
				if (h != INVALID_HANDLE_VALUE)
					CloseHandle(h);
			}
			return v & 0xFFFF;
		}
	}
}

/* ---- archivos en memoria (CreateFileMapping: se lee entero) ---- */
API(HANDLE) CreateFileMappingA(HANDLE f, void *s, u32 prot, u32 alto, u32 bajo, const char *n)
{
	Obj *o = obj(f);
	HANDLE h;
	u32 t;
	(void)s; (void)prot; (void)alto; (void)n;
	t = bajo;
	if (o && o->tipo == O_ARCHIVO && !t)
		t = sys_largo(o->fd);
	h = nuevo_obj(O_MAPA);
	if (!h)
		return 0;
	obj(h)->f = m_cero(t + 1);
	obj(h)->a = t;
	if (o && o->tipo == O_ARCHIVO) {
		u32 pos = sys_mover(o->fd, 0, 1);
		sys_mover(o->fd, 0, 0);
		sys_leer(o->fd, obj(h)->f, t);
		sys_mover(o->fd, (i32)pos, 0);
	}
	return h;
}
API(void *) MapViewOfFile(HANDLE h, u32 a, u32 alto, u32 bajo, u32 n)
{
	Obj *o = obj(h);
	(void)a; (void)alto; (void)n;
	if (!o || o->tipo != O_MAPA)
		return 0;
	return (u8 *)o->f + bajo;
}
API(BOOL) UnmapViewOfFile(const void *p) { (void)p; return TRUE; }
