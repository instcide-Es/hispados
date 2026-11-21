/* kernel32.c - WIN32.BIN: KERNEL32 sobre las llamadas de Hispa-Pentium. Se
 * comporta como el de Windows 98 (4.10) donde importa: los archivos y las
 * carpetas (con los números de error de Win32), las búsquedas con nombres
 * largos, la consola (por la llamada 1 y las teclas), la hora (FILETIME desde
 * 1601, sin husos), la memoria (monton.c), los módulos (cargador.c), los
 * recursos, los .INI, los átomos, el entorno, los procesos (lanzar y
 * esperar) y los hilos como en HW32 de Hispa-486: uno solo (CreateThread
 * corre el hilo entero antes de volver).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "w32.h"
#include "k32.h"

#define API(t) t WINAPI
u32 ultimo_error;

/* ---- la TEB ---- */
#define TEB ((u32 *)TEB_DIR)
void pon_error(u32 e) { TEB[0x34 / 4] = e; }
static u32 da_error(void) { return TEB[0x34 / 4]; }

/* los errores del núcleo, como los de Win32 */
u32 error_de(u32 r)
{
	switch (r) {
	case E_NO_HAY: return 2;              /* ERROR_FILE_NOT_FOUND */
	case E_SIN_DISCO: return 15;          /* ERROR_INVALID_DRIVE */
	case E_LLENO: return 4;               /* ERROR_TOO_MANY_OPEN_FILES */
	case E_MAL: return 6;
	case E_SIN_MEMORIA: return 8;
	case E_NO_VALE: return 87;
	case E_DISCO_LLENO: return 112;
	case E_YA_EXISTE: return 183;          /* ERROR_ALREADY_EXISTS */
	case E_EN_USO: return 32;              /* ERROR_SHARING_VIOLATION */
	case E_PROTEGIDO: return 5;            /* ERROR_ACCESS_DENIED */
	case E_NO_VACIA: return 145;
	case E_NOMBRE_MALO: return 123;
	case E_ES_CARPETA: return 5;
	case E_NO_CARPETA: return 3;           /* ERROR_PATH_NOT_FOUND */
	}
	return 31;
}

/* ---- las letras: la 1252 (Windows) y la del núcleo (437 con las de Ventanas) ---- */
#include "datos.h"
void a437(char *d, const char *s, u32 cabe)
{
	u32 k;
	for (k = 0; s[k] && k + 1 < cabe; k++) {
		u8 c = s[k];
		d[k] = c < 128 ? c : t1252_437[c - 128];
		if (d[k] == '/')
			d[k] = '\\';
	}
	d[k] = 0;
}
void a1252(char *d, const char *s, u32 cabe)
{
	u32 k;
	for (k = 0; s[k] && k + 1 < cabe; k++) {
		u8 c = s[k];
		d[k] = c < 128 ? c : t437_1252[c - 128];
	}
	d[k] = 0;
}
/* de la 1252 a Unicode y al revés (lo de 80h a 9Fh, con su tabla) */
static const u16 u80[32] = { 0x20AC, 0x81, 0x201A, 0x192, 0x201E, 0x2026, 0x2020, 0x2021, 0x2C6, 0x2030, 0x160, 0x2039, 0x152, 0x8D, 0x17D, 0x8F,
	0x90, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x2DC, 0x2122, 0x161, 0x203A, 0x153, 0x9D, 0x17E, 0x178 };
u16 a_uni(u8 c) { return c >= 0x80 && c < 0xA0 ? u80[c - 0x80] : c; }
u8 de_uni(u16 w)
{
	int k;
	if (w < 0x80 || (w >= 0xA0 && w < 0x100))
		return (u8)w;
	for (k = 0; k < 32; k++)
		if (u80[k] == w)
			return 0x80 + k;
	return '?';
}
void de_w(char *d, const WCHAR *s, u32 cabe)
{
	u32 k;
	for (k = 0; s && s[k] && k + 1 < cabe; k++)
		d[k] = de_uni(s[k]);
	d[k] = 0;
}
void a_w(WCHAR *d, const char *s, u32 cabe)
{
	u32 k;
	for (k = 0; s[k] && k + 1 < cabe; k++)
		d[k] = a_uni(s[k]);
	d[k] = 0;
}

/* ---- los objetos (HANDLE) ---- */
Obj objs[MAX_OBJ];
HANDLE nuevo_obj(int tipo)
{
	int i;
	for (i = 3; i < MAX_OBJ; i++)
		if (!objs[i].tipo) {
			memset(&objs[i], 0, sizeof objs[i]);
			objs[i].tipo = tipo;
			return (HANDLE)(0x1000 + i * 4);
		}
	pon_error(4);
	return 0;
}
Obj *obj(HANDLE h)
{
	u32 v = (u32)h;
	if (v == (u32)-10) v = 0x1000;
	else if (v == (u32)-11) v = 0x1004;
	else if (v == (u32)-12) v = 0x1008;
	if (v < 0x1000 || ((v - 0x1000) & 3) || (v - 0x1000) / 4 >= MAX_OBJ)
		return 0;
	if (!objs[(v - 0x1000) / 4].tipo)
		return 0;
	return &objs[(v - 0x1000) / 4];
}

/* ---- las rutas ---- */
/* la carpeta de trabajo, «X:\UNA\OTRA» (en la 1252) */
void carpeta_de_trabajo(char *d, u32 cabe)
{
	char c[160];
	u32 l = sys_unidad(0);
	c[0] = 0;
	sys_carpeta_actual(c, sizeof c);
	if (c[0] && c[0] != '\\') {
		/* (sin la barra delante) */
		memmove(c + 1, c, strlen(c) + 1);
		c[0] = '\\';
	}
	{
		char t[170];
		fmt(t, sizeof t, "%c:%s", (int)(ES_ERROR(l) || !l ? 'C' : l), c[0] ? c : "\\");
		a1252(d, t, cabe);
	}
}
/* la ruta entera (con su letra, sin «.» ni «..», con «\») */
void ruta_entera(const char *r, char *d, u32 cabe)
{
	char t[400], *p, *q;
	char partes[32][64];
	int n = 0, k;
	if (!r)
		r = "";
	if (r[0] && r[1] == ':') {
		if (r[2] == '\\' || r[2] == '/')
			fmt(t, sizeof t, "%s", r);
		else {
			char c[200];
			carpeta_de_trabajo(c, sizeof c);
			if (mayus(c[0]) == mayus(r[0]))
				fmt(t, sizeof t, "%s\\%s", c, r + 2);
			else
				fmt(t, sizeof t, "%c:\\%s", r[0], r + 2);
		}
	} else if (r[0] == '\\' || r[0] == '/') {
		char c[200];
		carpeta_de_trabajo(c, sizeof c);
		fmt(t, sizeof t, "%c:%s", c[0], r);
	} else {
		char c[200];
		carpeta_de_trabajo(c, sizeof c);
		fmt(t, sizeof t, "%s\\%s", c, r);
	}
	for (p = t; *p; p++)
		if (*p == '/')
			*p = '\\';
	/* trocear lo de detrás de «X:» */
	p = t + 2;
	while (*p) {
		while (*p == '\\')
			p++;
		if (!*p)
			break;
		q = p;
		while (*q && *q != '\\')
			q++;
		k = q - p;
		if (k == 1 && p[0] == '.')
			;
		else if (k == 2 && p[0] == '.' && p[1] == '.') {
			if (n)
				n--;
		} else if (n < 32) {
			if (k > 63)
				k = 63;
			memcpy(partes[n], p, k);
			partes[n][k] = 0;
			n++;
		}
		p = q;
	}
	{
		u32 o = 0;
		char res[400];
		res[o++] = mayus(t[0]);
		res[o++] = ':';
		if (!n)
			res[o++] = '\\';
		for (k = 0; k < n && o + 66 < sizeof res; k++) {
			res[o++] = '\\';
			strcpy(res + o, partes[k]);
			o += strlen(partes[k]);
		}
		res[o] = 0;
		copia_n(d, res, cabe);
	}
}

/* ---- buscar un nombre en su carpeta ---- */
static int casa(const char *p, const char *n)
{
	/* los comodines de Windows: * y ?, sin distinguir mayúsculas; «*.*» vale para todo */
	if (!strcmp(p, "*.*") || !strcmp(p, "*"))
		return 1;
	for (; *p; p++, n++) {
		if (*p == '*') {
			p++;
			if (!*p)
				return 1;
			if (*p == '.' && !strcmp(p, ".*"))
				return 1;
			for (; *n; n++)
				if (casa(p, n))
					return 1;
			return casa(p, n);
		}
		if (*p == '?') {
			if (!*n) {
				/* «?» al final puede no ser nada */
				continue;
			}
			continue;
		}
		if (mayus(*p) != mayus(*n)) {
			if (*p == '.' && !*n && (!p[1] || !strcmp(p + 1, "*")))
				return 1;
			return 0;
		}
	}
	return !*n;
}

/* parte «X:\A\B\patrón» en la carpeta (de la del núcleo) y el patrón */
static void partir(const char *ruta, char *carpeta, u32 cc, char *patron, u32 cp)
{
	char e[300], *b;
	ruta_entera(ruta, e, sizeof e);
	b = strrchr(e, '\\');
	if (!b) {
		copia_n(carpeta, e, cc);
		patron[0] = 0;
		return;
	}
	copia_n(patron, b + 1, cp);
	if (b == e + 2)
		b[1] = 0;
	else
		*b = 0;
	{
		char t[300];
		a437(t, e, sizeof t);
		copia_n(carpeta, t, cc);
	}
}

/* lee la entrada siguiente de una carpeta abierta: nombre (el largo, o el
 * corto) en la 1252 */
static int siguiente(u32 m, Entrada *en, char *largo, char *corto)
{
	char l[260];
	for (;;) {
		l[0] = 0;
		if (sys_entrada_larga(m, en, l) != 1)
			return 0;
		if (en->atributos & 8)
			continue;            /* la etiqueta del volumen */
		a1252(corto, en->nombre, 14);
		if (l[0])
			a1252(largo, l, 260);
		else
			strcpy(largo, corto);
		return 1;
	}
}

/* la entrada de esa ruta (falso si no está); con la raíz, una de mentira */
int entrada_de(const char *ruta, Entrada *en, char *largo)
{
	char carpeta[300], patron[260], corto[14];
	u32 m;
	partir(ruta, carpeta, sizeof carpeta, patron, sizeof patron);
	memset(en, 0, sizeof *en);
	if (!patron[0]) {
		en->atributos = 0x10;
		if (largo)
			strcpy(largo, "");
		return 1;
	}
	m = sys_abrir(carpeta, 0);
	if (ES_ERROR(m))
		return 0;
	{
		char l[260];
		while (siguiente(m, en, l, corto))
			if (!stricmp_(l, patron) || !stricmp_(corto, patron)) {
				if (largo)
					strcpy(largo, l);
				sys_cerrar(m);
				return 1;
			}
	}
	sys_cerrar(m);
	return 0;
}

/* ---- la hora ---- */
/* días desde el 1-1-1601 */
static u32 dias_de(u32 a, u32 m, u32 d)
{
	static const u16 acum[12] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
	u32 y = a - 1601, n = y * 365 + y / 4 - y / 100 + y / 400 + acum[m - 1] + d - 1;
	if (m > 2 && ((a % 4 == 0 && a % 100) || a % 400 == 0))
		n++;
	return n;
}
typedef struct { u16 a, m, ds, d, h, mi, s, ms; } SYSTEMTIME;
typedef struct { u32 lo, hi; } FILETIME;
static void ft_de(u64 v, FILETIME *f) { f->lo = (u32)v; f->hi = (u32)(v >> 32); }
static u64 v_de(const FILETIME *f) { return ((u64)f->hi << 32) | f->lo; }
static u64 hx_div64(u64 n, u32 d)
{
	u32 hi = (u32)(n >> 32), lo = (u32)n, qh, ql, r;
	qh = hi / d; r = hi % d;
	__asm__("divl %4" : "=a"(ql), "=d"(r) : "a"(lo), "d"(r), "rm"(d));
	return ((u64)qh << 32) | ql;
}
static u64 st_a_v(const SYSTEMTIME *s)
{
	u64 dias = dias_de(s->a < 1601 ? 1601 : s->a, s->m ? s->m : 1, s->d ? s->d : 1);
	u64 seg = dias * 86400 + s->h * 3600 + s->mi * 60 + s->s;
	return seg * 10000000ull + s->ms * 10000ull;
}
static void v_a_st(u64 v, SYSTEMTIME *s)
{
	u64 seg = hx_div64(v, 10000000);
	u32 dias = (u32)hx_div64(seg, 86400), r = (u32)(seg - (u64)dias * 86400), a = 1601, m;
	static const u8 dm[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
	s->ms = (u16)((u32)(v - seg * 10000000ull) / 10000);
	s->ds = (dias + 1) % 7;          /* el 1-1-1601 fue lunes */
	for (;;) {
		u32 n = ((a % 4 == 0 && a % 100) || a % 400 == 0) ? 366 : 365;
		if (dias < n)
			break;
		dias -= n;
		a++;
	}
	for (m = 0; m < 12; m++) {
		u32 n = dm[m] + (m == 1 && ((a % 4 == 0 && a % 100) || a % 400 == 0));
		if (dias < n)
			break;
		dias -= n;
	}
	s->a = a; s->m = m + 1; s->d = dias + 1;
	s->h = r / 3600; s->mi = r / 60 % 60; s->s = r % 60;
}
static void fat_a_st(u16 fe, u16 ho, SYSTEMTIME *s)
{
	memset(s, 0, sizeof *s);
	s->a = 1980 + (fe >> 9); s->m = (fe >> 5) & 15; s->d = fe & 31;
	if (!s->m) s->m = 1;
	if (!s->d) s->d = 1;
	s->h = ho >> 11; s->mi = (ho >> 5) & 63; s->s = (ho & 31) * 2;
}
static void ahora(SYSTEMTIME *s)
{
	u32 f = sys_dato(10);
	static u32 base_tic, base_f;
	u32 t = sys_tics();
	fat_a_st(f >> 16, f & 0xFFFF, s);
	if (f != base_f) { base_f = f; base_tic = t; }
	s->ms = (u16)(((t - base_tic) * 10) % 1000);
	s->ds = (dias_de(s->a, s->m, s->d) + 1) % 7;
}
u32 tics_ms(void) { return sys_tics() * 10; }

/* ---- el proceso ---- */
char linea_ordenes[400];
static WCHAR linea_w[400];
static char *entorno;
static WCHAR *entorno_w;
extern void (*al_salir_user)(void);
void w32_salir(u32 c)
{
	static int saliendo;
	if (!saliendo) {
		saliendo = 1;
		mods_acabar();
		cerrar_todo();
		if (al_salir_user)
			al_salir_user();
	}
	sys_salir(c);
}
API(void) ExitProcess(u32 c) { traza("ExitProcess(%u)", c); w32_salir(c); }
API(BOOL) TerminateProcess(HANDLE h, u32 c)
{
	if (h == (HANDLE)-1)
		w32_salir(c);
	pon_error(5);
	return FALSE;
}
API(HANDLE) GetCurrentProcess(void) { return (HANDLE)-1; }
API(HANDLE) GetCurrentThread(void) { return (HANDLE)-2; }
API(u32) GetCurrentProcessId(void) { return sys_tarea(); }
API(u32) GetCurrentThreadId(void) { return sys_tarea() + hilo_n * 0x10000; }
API(HMODULE) GetModuleHandleA(const char *n)
{
	Modulo *m;
	if (!n)
		return (HMODULE)mod_exe->base;
	m = mod_por_nombre(n);
	if (!m) {
		/* sin «.DLL»... o el propio .EXE por su nombre */
		char b[40];
		const char *p = strrchr(n, '\\');
		copia_n(b, p ? p + 1 : n, sizeof b);
		if (!stricmp_(b, mod_exe->nombre))
			return (HMODULE)mod_exe->base;
		pon_error(126);
		return 0;
	}
	return (HMODULE)m->base;
}
API(HMODULE) GetModuleHandleW(const WCHAR *n)
{
	char b[160];
	if (!n)
		return GetModuleHandleA(0);
	de_w(b, n, sizeof b);
	return GetModuleHandleA(b);
}
API(u32) GetModuleFileNameA(HMODULE h, char *d, u32 n)
{
	Modulo *m = mod_de(h);
	if (!m || !n)
		return 0;
	copia_n(d, m->es_exe ? ruta_exe : m->ruta, n);
	return strlen(d);
}
API(u32) GetModuleFileNameW(HMODULE h, WCHAR *d, u32 n)
{
	char b[200];
	u32 r = GetModuleFileNameA(h, b, sizeof b);
	if (!n)
		return 0;
	a_w(d, b, n);
	return r < n ? r : n - 1;
}
API(HMODULE) LoadLibraryA(const char *n)
{
	int f = 0;
	Modulo *m;
	if (!n) {
		pon_error(87);
		return 0;
	}
	m = mod_cargar(n, &f);
	traza("LoadLibraryA(%s) = %x", n, m ? m->base : 0);
	if (!m) {
		pon_error(f == 1 ? 126 : 193);
		return 0;
	}
	return (HMODULE)m->base;
}
API(HMODULE) LoadLibraryW(const WCHAR *n)
{
	char b[200];
	de_w(b, n, sizeof b);
	return LoadLibraryA(b);
}
API(HMODULE) LoadLibraryExA(const char *n, HANDLE f, u32 b) { (void)f; (void)b; return LoadLibraryA(n); }
API(HMODULE) LoadLibraryExW(const WCHAR *n, HANDLE f, u32 b) { (void)f; (void)b; return LoadLibraryW(n); }
API(BOOL) FreeLibrary(HMODULE h)
{
	Modulo *m = mod_de(h);
	if (m && h)
		mod_soltar(m);
	return TRUE;
}
API(void *) GetProcAddress(HMODULE h, const char *n)
{
	Modulo *m = mod_de(h);
	void *p = m ? mod_funcion(m, n) : 0;
	if ((u32)n < 0x10000)
		traza("GetProcAddress(%s, #%u) = %x", m ? m->nombre : "?", (u32)n, (u32)p);
	else
		traza("GetProcAddress(%s, %s) = %x", m ? m->nombre : "?", n, (u32)p);
	if (!p)
		pon_error(127);
	return p;
}
API(char *) GetCommandLineA(void) { return linea_ordenes; }
API(WCHAR *) GetCommandLineW(void)
{
	if (!linea_w[0])
		a_w(linea_w, linea_ordenes, 400);
	return linea_w;
}
typedef struct { u32 cb; char *res, *escritorio, *titulo; u32 x, y, xs, ys, xc, yc, relleno, fl; u16 show, cbr2; void *r2; HANDLE in, out, err; } STARTUPINFOA;
API(void) GetStartupInfoA(STARTUPINFOA *s)
{
	memset(s, 0, sizeof *s);
	s->cb = sizeof *s;
	s->titulo = ruta_exe;
	s->show = 1;
	s->in = (HANDLE)-10;
	s->out = (HANDLE)-11;
	s->err = (HANDLE)-12;
}
API(void) GetStartupInfoW(STARTUPINFOA *s) { GetStartupInfoA(s); s->titulo = 0; }
API(u32) GetVersion(void) { return 0xC0000A04u; }         /* Windows 98: 4.10 */
typedef struct { u32 tam, mayor, menor, comp, plat; char csd[128]; } OSVERSIONINFOA;
API(BOOL) GetVersionExA(OSVERSIONINFOA *v)
{
	if (!v)
		return FALSE;
	v->mayor = 4; v->menor = 10; v->comp = 0x040A07CE; v->plat = 1;
	memset(v->csd, 0, sizeof v->csd);
	strcpy(v->csd, " A ");
	return TRUE;
}
API(BOOL) GetVersionExW(u32 *v)
{
	if (!v)
		return FALSE;
	v[1] = 4; v[2] = 10; v[3] = 0x040A07CE; v[4] = 1;
	memset(v + 5, 0, 256);
	return TRUE;
}

/* el entorno: lo de siempre y lo de ENTORNO.TXT (carpeta de Win32) */
void prepara_entorno(void)
{
	char t[1200], *p;
	u32 o = 0, m, n;
	char exe_dir[160], c[200];
	copia_n(exe_dir, ruta_exe, sizeof exe_dir);
	p = strrchr(exe_dir, '\\');
	if (p)
		*p = 0;
	o += fmt(t + o, sizeof t - o, "COMSPEC=%c:\\ORDENES.BIN", carpeta_w32[0] ? carpeta_w32[0] : 'C') + 1;
	o += fmt(t + o, sizeof t - o, "PATH=%s;%s", exe_dir, carpeta_w32) + 1;
	o += fmt(t + o, sizeof t - o, "TEMP=%c:\\", carpeta_w32[0] ? carpeta_w32[0] : 'C') + 1;
	o += fmt(t + o, sizeof t - o, "TMP=%c:\\", carpeta_w32[0] ? carpeta_w32[0] : 'C') + 1;
	o += fmt(t + o, sizeof t - o, "WINDIR=%s", carpeta_w32) + 1;
	if (o > 0 && t[o - 2] == '\\') { t[o - 2] = 0; o--; }
	{
		/* (la de los .INC de FASM, si está al lado del programa) */
		Entrada e;
		fmt(c, sizeof c, "%s\\INCLUDE", exe_dir);
		if (entrada_de(c, &e, 0) && (e.atributos & 0x10))
			o += fmt(t + o, sizeof t - o, "INCLUDE=%s", c) + 1;
	}
	fmt(c, sizeof c, "%sENTORNO.TXT", carpeta_w32);
	{
		char k[160];
		a437(k, c, sizeof k);
		m = sys_abrir(k, 0);
	}
	if (!ES_ERROR(m)) {
		char b[800];
		n = sys_leer(m, b, sizeof b - 1);
		sys_cerrar(m);
		if (!ES_ERROR(n)) {
			char *l = b;
			b[n] = 0;
			while (*l) {
				char *f = l;
				while (*f && *f != '\r' && *f != '\n')
					f++;
				if (*f)
					*f++ = 0;
				if (strchr(l, '=') && *l != ';' && o + strlen(l) + 2 < sizeof t) {
					strcpy(t + o, l);
					o += strlen(l) + 1;
				}
				while (*f == '\r' || *f == '\n')
					f++;
				l = f;
			}
		}
	}
	t[o++] = 0;
	entorno = m_pedir(o + 512);
	memcpy(entorno, t, o);
}
static u32 largo_entorno(const char *e)
{
	const char *p = e;
	while (*p)
		p += strlen(p) + 1;
	return p - e + 1;
}
API(char *) GetEnvironmentStrings(void) { return entorno; }
API(char *) GetEnvironmentStringsA(void) { return entorno; }
API(WCHAR *) GetEnvironmentStringsW(void)
{
	u32 n = largo_entorno(entorno), k;
	m_soltar(entorno_w);
	entorno_w = m_pedir(n * 2);
	for (k = 0; k < n; k++)
		entorno_w[k] = a_uni(entorno[k]);
	return entorno_w;
}
API(BOOL) FreeEnvironmentStringsA(char *p) { (void)p; return TRUE; }
API(BOOL) FreeEnvironmentStringsW(WCHAR *p) { (void)p; return TRUE; }
static char *busca_var(const char *n)
{
	char *p = entorno;
	u32 l = strlen(n);
	while (*p) {
		if (!strnicmp_(p, n, l) && p[l] == '=')
			return p;
		p += strlen(p) + 1;
	}
	return 0;
}
API(u32) GetEnvironmentVariableA(const char *n, char *d, u32 cabe)
{
	char *p = busca_var(n);
	u32 l;
	if (!p) {
		pon_error(203);
		if (d && cabe)
			d[0] = 0;
		return 0;
	}
	p += strlen(n) + 1;
	l = strlen(p);
	if (!d || l + 1 > cabe)
		return l + 1;
	strcpy(d, p);
	return l;
}
API(u32) GetEnvironmentVariableW(const WCHAR *n, WCHAR *d, u32 cabe)
{
	char a[100], b[600];
	u32 r;
	de_w(a, n, sizeof a);
	r = GetEnvironmentVariableA(a, b, sizeof b);
	if (!r || r >= sizeof b)
		return r;
	if (!d || r + 1 > cabe)
		return r + 1;
	a_w(d, b, cabe);
	return r;
}
API(BOOL) SetEnvironmentVariableA(const char *n, const char *v)
{
	char *p = busca_var(n), *nuevo;
	u32 t = largo_entorno(entorno), l;
	if (p) {
		l = strlen(p) + 1;
		memmove(p, p + l, entorno + t - (p + l));
		t -= l;
	}
	if (!v)
		return TRUE;
	l = strlen(n) + strlen(v) + 2;
	nuevo = m_pedir(t + l);
	memcpy(nuevo, entorno, t - 1);
	fmt(nuevo + t - 1, l, "%s=%s", n, v);
	nuevo[t - 1 + l] = 0;
	m_soltar(entorno);
	entorno = nuevo;
	return TRUE;
}
API(u32) ExpandEnvironmentStringsA(const char *s, char *d, u32 cabe)
{
	char t[1024];
	u32 o = 0;
	while (*s && o + 1 < sizeof t) {
		if (*s == '%') {
			const char *f = strchr(s + 1, '%');
			if (f) {
				char n[64], v[400];
				u32 l = f - s - 1;
				if (l < sizeof n) {
					memcpy(n, s + 1, l);
					n[l] = 0;
					if (GetEnvironmentVariableA(n, v, sizeof v)) {
						copia_n(t + o, v, sizeof t - o);
						o += strlen(t + o);
						s = f + 1;
						continue;
					}
				}
			}
		}
		t[o++] = *s++;
	}
	t[o] = 0;
	if (d && cabe > o)
		strcpy(d, t);
	return o + 1;
}

/* ---- los errores y lo del depurador ---- */
API(u32) GetLastError(void) { return da_error(); }
API(void) SetLastError(u32 e) { pon_error(e); }
API(u32) SetErrorMode(u32 m) { static u32 v; u32 a = v; v = m; return a; }
API(void *) SetUnhandledExceptionFilter(void *f) { static void *v; void *a = v; v = f; return a; }
API(i32) UnhandledExceptionFilter(void *p) { (void)p; return 0; }
API(BOOL) IsDebuggerPresent(void) { return FALSE; }
API(void) OutputDebugStringA(const char *s) { traza("depuración: %s", s); }
API(void) DebugBreak(void) { }
API(void) RaiseException(u32 c, u32 f, u32 n, const u32 *a)
{
	(void)f; (void)n; (void)a;
	aviso_serie("RaiseException(%x): se acaba el programa", c);
	w32_salir(255);
}
API(void) RtlUnwind(void *a, void *b, void *c, void *d) { (void)a; (void)b; (void)c; (void)d; }
API(BOOL) IsProcessorFeaturePresent(u32 f) { return f == 0 || f == 2 || f == 3; }  /* FPU, RDTSC... */
API(BOOL) IsBadReadPtr(const void *p, u32 n) { return p == 0 && n; }
API(BOOL) IsBadWritePtr(void *p, u32 n) { return p == 0 && n; }
API(BOOL) IsBadCodePtr(void *p) { return p == 0; }
API(BOOL) IsBadStringPtrA(const char *p, u32 n) { return p == 0 && n; }
API(void) SetHandleCount(u32 n) { (void)n; }
/* (SONIDO) Beep: un tono de verdad (cuadrado, suave), por un canal propio; sin
 * tarjeta, solo el tiempo */
API(BOOL) Beep(u32 f, u32 d)
{
	static i16 b[2 * 960];
	u32 c, hechas = 0, total, fase = 0, paso, k;
	if (f < 37 || f > 32767 || d > 60000)
		return FALSE;
	c = sc(190, 48000, 2 | 16 << 8, 0);
	if (ES_ERROR(c) || c > 64) {
		sys_dormir(d / 10);
		return TRUE;
	}
	total = 48 * d;
	paso = f * 89478u + f * 485u / 1000u;     /* (2^32 / 48.000 por hercio) */
	while (hechas < total) {
		u32 n = total - hechas > 960 ? 960 : total - hechas;
		for (k = 0; k < n; k++, fase += paso)
			b[2 * k] = b[2 * k + 1] = (fase & 0x80000000u) ? -6000 : 6000;
		if (sc(191, c, LIN(b), n * 4) != n * 4)
			break;
		hechas += n;
	}
	sc(192, c, 5, 0);
	sc(192, c, 0, 0);
	return TRUE;
}
API(void) InitializeSListHead(void *p) { memset(p, 0, 8); }
API(u32) GetTickCount(void) { return tics_ms(); }
API(u64) GetTickCount64(void) { return tics_ms(); }
static u64 rdtsc(void) { u32 a, d; __asm__ volatile("rdtsc" : "=a"(a), "=d"(d)); return ((u64)d << 32) | a; }
static u32 tsc_hz;
static void mide_tsc(void)
{
	u32 t;
	u64 a, b;
	if (tsc_hz)
		return;
	t = sys_tics();
	while (sys_tics() == t)
		;
	a = rdtsc();
	t = sys_tics();
	while (sys_tics() < t + 5)
		;
	b = rdtsc();
	tsc_hz = (u32)((b - a) * 20);       /* 5 tics = 50 ms */
	if (!tsc_hz)
		tsc_hz = 1;
}
API(BOOL) QueryPerformanceFrequency(u64 *f) { mide_tsc(); *f = tsc_hz; return TRUE; }
API(BOOL) QueryPerformanceCounter(u64 *c) { mide_tsc(); *c = rdtsc(); return TRUE; }
API(void) Sleep(u32 ms)
{
	u32 fin = tics_ms() + ms;
	dx_tic();
	if (!ms) {
		sys_dormir(0);
		return;
	}
	while ((i32)(fin - tics_ms()) > 0) {
		u32 q = (fin - tics_ms() + 9) / 10;
		sys_dormir(q > 2 ? 2 : (q ? q : 1));
		dx_tic();
	}
}
API(u32) SleepEx(u32 ms, BOOL a) { (void)a; Sleep(ms); return 0; }
API(void) GetSystemTime(SYSTEMTIME *s) { ahora(s); }
API(void) GetLocalTime(SYSTEMTIME *s) { ahora(s); }
API(BOOL) SetLocalTime(const SYSTEMTIME *s) { (void)s; return TRUE; }
API(BOOL) SetSystemTime(const SYSTEMTIME *s) { (void)s; return TRUE; }
API(void) GetSystemTimeAsFileTime(FILETIME *f) { SYSTEMTIME s; ahora(&s); ft_de(st_a_v(&s), f); }
API(BOOL) FileTimeToSystemTime(const FILETIME *f, SYSTEMTIME *s) { v_a_st(v_de(f), s); return TRUE; }
API(BOOL) SystemTimeToFileTime(const SYSTEMTIME *s, FILETIME *f) { ft_de(st_a_v(s), f); return TRUE; }
API(BOOL) FileTimeToLocalFileTime(const FILETIME *a, FILETIME *b) { *b = *a; return TRUE; }
API(BOOL) LocalFileTimeToFileTime(const FILETIME *a, FILETIME *b) { *b = *a; return TRUE; }
API(i32) CompareFileTime(const FILETIME *a, const FILETIME *b) { u64 x = v_de(a), y = v_de(b); return x < y ? -1 : x > y; }
API(BOOL) FileTimeToDosDateTime(const FILETIME *f, u16 *fe, u16 *ho)
{
	SYSTEMTIME s;
	v_a_st(v_de(f), &s);
	if (s.a < 1980)
		s.a = 1980;
	*fe = ((s.a - 1980) << 9) | (s.m << 5) | s.d;
	*ho = (s.h << 11) | (s.mi << 5) | (s.s / 2);
	return TRUE;
}
API(BOOL) DosDateTimeToFileTime(u16 fe, u16 ho, FILETIME *f) { SYSTEMTIME s; fat_a_st(fe, ho, &s); ft_de(st_a_v(&s), f); return TRUE; }
API(u32) GetTimeZoneInformation(void *t) { memset(t, 0, 172); return 0; }
typedef struct { u16 arq, res; u32 pag; void *min, *max; u32 cpus_masc, ncpu, tipo, gran; u16 nivel, rev; } SYSTEM_INFO;
API(void) GetSystemInfo(SYSTEM_INFO *s)
{
	memset(s, 0, sizeof *s);
	s->pag = 4096; s->min = (void *)0x10000; s->max = (void *)0x7FFEFFFF;
	s->cpus_masc = 1; s->ncpu = 1; s->tipo = 586; s->gran = 0x10000; s->nivel = 5;
}
typedef struct { u32 tam, carga, fisica, fisica_libre, pag, pag_libre, virt, virt_libre; } MEMORYSTATUS;
API(void) GlobalMemoryStatus(MEMORYSTATUS *m)
{
	u32 libres = sys_dato(0), total = sys_dato(1);
	m->tam = sizeof *m;
	m->fisica = total * 4096; m->fisica_libre = libres * 4096;
	m->carga = total ? 100 - libres * 100 / total : 0;
	m->pag = m->fisica; m->pag_libre = m->fisica_libre;
	m->virt = 0x7FFE0000; m->virt_libre = 0x60000000;
}
API(BOOL) GlobalMemoryStatusEx(u32 *m)
{
	u32 libres = sys_dato(0), total = sys_dato(1);
	m[1] = total ? 100 - libres * 100 / total : 0;
	m[2] = total * 4096; m[3] = 0; m[4] = libres * 4096; m[5] = 0;
	m[6] = m[2]; m[7] = 0; m[8] = m[4]; m[9] = 0;
	m[10] = 0x7FFE0000; m[11] = 0; m[12] = 0x60000000; m[13] = 0; m[14] = 0; m[15] = 0;
	return TRUE;
}

/* ---- las letras y los idiomas ---- */
API(u32) GetACP(void) { return 1252; }
API(u32) GetOEMCP(void) { return 437; }
API(u32) GetConsoleCP(void) { return 437; }
API(u32) GetConsoleOutputCP(void) { return 437; }
API(BOOL) SetConsoleCP(u32 c) { (void)c; return TRUE; }
API(BOOL) SetConsoleOutputCP(u32 c) { (void)c; return TRUE; }
API(BOOL) IsValidCodePage(u32 c) { return c == 1252 || c == 437 || c == 850 || c == 65001; }
API(BOOL) GetCPInfo(u32 c, u8 *i)
{
	(void)c;
	memset(i, 0, 20);
	*(u32 *)i = 1;
	i[4] = '?';
	return TRUE;
}
API(BOOL) IsDBCSLeadByte(u8 c) { (void)c; return FALSE; }
API(BOOL) IsDBCSLeadByteEx(u32 cp, u8 c) { (void)cp; (void)c; return FALSE; }
API(i32) MultiByteToWideChar(u32 cp, u32 fl, const char *s, i32 n, WCHAR *d, i32 cabe)
{
	i32 k, l;
	(void)fl;
	if (n < 0)
		n = strlen(s) + 1;
	if (cp == 65001) {
		/* UTF-8 */
		i32 o = 0;
		for (k = 0; k < n; ) {
			u8 c = s[k];
			u32 w;
			if (c < 0x80) { w = c; k++; }
			else if ((c & 0xE0) == 0xC0 && k + 1 < n) { w = ((c & 31) << 6) | (s[k + 1] & 63); k += 2; }
			else if ((c & 0xF0) == 0xE0 && k + 2 < n) { w = ((c & 15) << 12) | ((s[k + 1] & 63) << 6) | (s[k + 2] & 63); k += 3; }
			else { w = '?'; k++; }
			if (cabe) {
				if (o >= cabe) { pon_error(122); return 0; }
				d[o] = (WCHAR)w;
			}
			o++;
		}
		return o;
	}
	l = n;
	if (!cabe)
		return l;
	if (cabe < l) {
		pon_error(122);
		return 0;
	}
	for (k = 0; k < n; k++)
		d[k] = cp == 437 || cp == 1 ? a_uni(s[k] & 0x80 ? t437_1252[(u8)s[k] - 128] : s[k]) : a_uni(s[k]);
	return l;
}
API(i32) WideCharToMultiByte(u32 cp, u32 fl, const WCHAR *s, i32 n, char *d, i32 cabe, const char *def, BOOL *usado)
{
	i32 k, o = 0;
	(void)fl; (void)def;
	if (usado)
		*usado = FALSE;
	if (n < 0) {
		for (n = 0; s[n]; n++)
			;
		n++;
	}
	for (k = 0; k < n; k++) {
		u32 w = s[k];
		char b[3];
		int nb = 1;
		if (cp == 65001) {
			if (w < 0x80) b[0] = w;
			else if (w < 0x800) { b[0] = 0xC0 | (w >> 6); b[1] = 0x80 | (w & 63); nb = 2; }
			else { b[0] = 0xE0 | (w >> 12); b[1] = 0x80 | ((w >> 6) & 63); b[2] = 0x80 | (w & 63); nb = 3; }
		} else
			b[0] = de_uni(w);
		if (cabe) {
			int j;
			if (o + nb > cabe) { pon_error(122); return 0; }
			for (j = 0; j < nb; j++)
				d[o + j] = b[j];
		}
		o += nb;
	}
	return o;
}
API(i32) lstrlenA(const char *s) { return s ? (i32)strlen(s) : 0; }
API(i32) lstrlenW(const WCHAR *s) { i32 n = 0; if (s) while (s[n]) n++; return n; }
API(char *) lstrcpyA(char *d, const char *s) { return strcpy(d, s); }
API(char *) lstrcpynA(char *d, const char *s, i32 n) { if (n > 0) copia_n(d, s, n); return d; }
API(char *) lstrcatA(char *d, const char *s) { return strcat(d, s); }
API(WCHAR *) lstrcpyW(WCHAR *d, const WCHAR *s) { WCHAR *r = d; while ((*d++ = *s++)) ; return r; }
API(i32) lstrcmpA(const char *a, const char *b) { i32 r = strcmp(a, b); return r < 0 ? -1 : r > 0; }
API(i32) lstrcmpiA(const char *a, const char *b) { i32 r = stricmp_(a, b); return r < 0 ? -1 : r > 0; }
API(i32) lstrcmpW(const WCHAR *a, const WCHAR *b) { while (*a && *a == *b) a++, b++; return *a < *b ? -1 : *a > *b; }
API(i32) lstrcmpiW(const WCHAR *a, const WCHAR *b) { while (*a && mayus(*a) == mayus(*b)) a++, b++; return mayus(*a) < mayus(*b) ? -1 : mayus(*a) > mayus(*b); }
API(i32) MulDiv(i32 a, i32 b, i32 c)
{
	i64 r;
	if (!c)
		return -1;
	r = (i64)a * b;
	if ((r < 0) != (c < 0))
		r -= c / 2;
	else
		r += c / 2;
	{
		i64 q;
		int neg = 0;
		u64 ur, uc;
		if (r < 0) { neg = !neg; ur = -r; } else ur = r;
		if (c < 0) { neg = !neg; uc = -c; } else uc = c;
		q = (i64)hx_div64(ur, (u32)uc);
		return (i32)(neg ? -q : q);
	}
}
API(BOOL) GetStringTypeA(u32 lc, u32 t, const char *s, i32 n, u16 *d)
{
	i32 k;
	(void)lc;
	if (n < 0)
		n = strlen(s);
	for (k = 0; k < n; k++) {
		u8 c = s[k];
		u16 v = 0;
		if (t == 1) {
			if (c >= 'A' && c <= 'Z') v |= 1 | 0x100;
			if (c >= 'a' && c <= 'z') v |= 2 | 0x100;
			if (c >= 0xC0 && c != 0xD7 && c != 0xF7) v |= (c < 0xE0 ? 1 : 2) | 0x100;
			if (c >= '0' && c <= '9') v |= 4;
			if (c == ' ' || (c >= 9 && c <= 13)) v |= 8;
			if (c == ' ' || c == 9) v |= 0x40;
			if ((c >= '!' && c <= '/') || (c >= ':' && c <= '@') || (c >= '[' && c <= '`') || (c >= '{' && c <= '~')) v |= 0x10;
			if (c < 32 || c == 127) v |= 0x20;
			if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) v |= 0x80;
		}
		d[k] = v;
	}
	return TRUE;
}
API(BOOL) GetStringTypeW(u32 t, const WCHAR *s, i32 n, u16 *d)
{
	char b[512];
	i32 k;
	if (n < 0)
		for (n = 0; s[n]; n++)
			;
	if (n > 511)
		n = 511;
	for (k = 0; k < n; k++)
		b[k] = de_uni(s[k]);
	return GetStringTypeA(0, t, b, n, d);
}
API(BOOL) GetStringTypeExA(u32 lc, u32 t, const char *s, i32 n, u16 *d) { return GetStringTypeA(lc, t, s, n, d); }
API(i32) LCMapStringA(u32 lc, u32 fl, const char *s, i32 n, char *d, i32 cabe)
{
	i32 k;
	(void)lc;
	if (n < 0)
		n = strlen(s) + 1;
	if (!cabe)
		return n;
	for (k = 0; k < n && k < cabe; k++)
		d[k] = fl & 0x200 ? mayus(s[k]) : fl & 0x100 ? minus(s[k]) : s[k];
	return k;
}
API(i32) LCMapStringW(u32 lc, u32 fl, const WCHAR *s, i32 n, WCHAR *d, i32 cabe)
{
	i32 k;
	(void)lc;
	if (n < 0)
		for (n = 0; s[n++]; )
			;
	if (!cabe)
		return n;
	for (k = 0; k < n && k < cabe; k++)
		d[k] = s[k] < 256 ? (WCHAR)(fl & 0x200 ? mayus(s[k]) : fl & 0x100 ? minus(s[k]) : s[k]) : s[k];
	return k;
}
API(i32) CompareStringA(u32 lc, u32 fl, const char *a, i32 na, const char *b, i32 nb)
{
	i32 k;
	(void)lc;
	if (na < 0) na = strlen(a);
	if (nb < 0) nb = strlen(b);
	for (k = 0; k < na && k < nb; k++) {
		int x = (u8)a[k], y = (u8)b[k];
		if (fl & 1) { x = mayus(x); y = mayus(y); }
		if (x != y)
			return x < y ? 1 : 3;
	}
	return na < nb ? 1 : na > nb ? 3 : 2;
}
API(i32) CompareStringW(u32 lc, u32 fl, const WCHAR *a, i32 na, const WCHAR *b, i32 nb)
{
	i32 k;
	(void)lc;
	if (na < 0) for (na = 0; a[na]; na++) ;
	if (nb < 0) for (nb = 0; b[nb]; nb++) ;
	for (k = 0; k < na && k < nb; k++) {
		u32 x = a[k], y = b[k];
		if (fl & 1) { x = x < 256 ? (u32)mayus(x) : x; y = y < 256 ? (u32)mayus(y) : y; }
		if (x != y)
			return x < y ? 1 : 3;
	}
	return na < nb ? 1 : na > nb ? 3 : 2;
}
API(u32) GetUserDefaultLCID(void) { return 0x0C0A; }     /* español (moderno) */
API(u32) GetSystemDefaultLCID(void) { return 0x0C0A; }
API(u32) GetThreadLocale(void) { return 0x0C0A; }
API(u16) GetUserDefaultLangID(void) { return 0x0C0A; }
API(u16) GetSystemDefaultLangID(void) { return 0x0C0A; }
API(u16) GetUserDefaultUILanguage(void) { return 0x0C0A; }
API(BOOL) IsValidLocale(u32 l, u32 f) { (void)l; (void)f; return TRUE; }
API(i32) GetLocaleInfoA(u32 lc, u32 t, char *d, i32 cabe)
{
	const char *v = "";
	char b[16];
	(void)lc;
	switch (t & 0xFFFF) {
	case 0x01: v = "0c0a"; break;                 /* LOCALE_ILANGUAGE */
	case 0x02: v = "Español"; break;
	case 0x03: v = "ESN"; break;
	case 0x0E: v = ","; break;                    /* LOCALE_SDECIMAL */
	case 0x0F: v = "."; break;                    /* LOCALE_STHOUSAND */
	case 0x10: v = "3;0"; break;
	case 0x14: v = "€"; break;
	case 0x1D: v = "/"; break;                    /* LOCALE_SDATE */
	case 0x1E: v = ":"; break;
	case 0x1F: v = "dd/MM/yyyy"; break;
	case 0x20: v = "d' de 'MMMM' de 'yyyy"; break;
	case 0x1004: v = "1252"; break;               /* LOCALE_IDEFAULTANSICODEPAGE */
	case 0x0B: v = "437"; break;                  /* LOCALE_IDEFAULTCODEPAGE */
	case 0x5A: v = "es"; break;
	case 0x5B: v = "ES"; break;
	default:
		fmt(b, sizeof b, "%u", 0);
		v = b;
	}
	if (!cabe)
		return strlen(v) + 1;
	copia_n(d, v, cabe);
	return strlen(d) + 1;
}
API(i32) GetLocaleInfoW(u32 lc, u32 t, WCHAR *d, i32 cabe)
{
	char b[64];
	i32 r = GetLocaleInfoA(lc, t, b, sizeof b);
	if (!cabe)
		return r;
	a_w(d, b, cabe);
	return r;
}

/* ---- los cerrojos, los eventos y los hilos (uno solo) ---- */
API(void) InitializeCriticalSection(void *c) { memset(c, 0, 24); }
API(BOOL) InitializeCriticalSectionAndSpinCount(void *c, u32 n) { (void)n; memset(c, 0, 24); return TRUE; }
API(BOOL) InitializeCriticalSectionEx(void *c, u32 n, u32 f) { (void)n; (void)f; memset(c, 0, 24); return TRUE; }
API(void) EnterCriticalSection(void *c) { (void)c; }
API(void) LeaveCriticalSection(void *c) { (void)c; }
API(BOOL) TryEnterCriticalSection(void *c) { (void)c; return TRUE; }
API(void) DeleteCriticalSection(void *c) { (void)c; }
API(i32) InterlockedIncrement(volatile i32 *p) { return ++*p; }
API(i32) InterlockedDecrement(volatile i32 *p) { return --*p; }
API(i32) InterlockedExchange(volatile i32 *p, i32 v) { i32 a = *p; *p = v; return a; }
API(i32) InterlockedExchangeAdd(volatile i32 *p, i32 v) { i32 a = *p; *p += v; return a; }
API(i32) InterlockedCompareExchange(volatile i32 *p, i32 v, i32 c) { i32 a = *p; if (a == c) *p = v; return a; }
API(HANDLE) CreateEventA(void *s, BOOL manual, BOOL ini, const char *n)
{
	HANDLE h = nuevo_obj(O_EVENTO);
	(void)s; (void)n;
	if (h) { obj(h)->a = ini; obj(h)->b = manual; }
	return h;
}
API(HANDLE) CreateEventW(void *s, BOOL manual, BOOL ini, const WCHAR *n) { (void)n; return CreateEventA(s, manual, ini, 0); }
API(BOOL) SetEvent(HANDLE h) { Obj *o = obj(h); if (o) o->a = 1; return o != 0; }
API(BOOL) ResetEvent(HANDLE h) { Obj *o = obj(h); if (o) o->a = 0; return o != 0; }
API(BOOL) PulseEvent(HANDLE h) { (void)h; return TRUE; }
API(HANDLE) CreateMutexA(void *s, BOOL d, const char *n) { (void)s; (void)d; (void)n; return nuevo_obj(O_MUTEX); }
API(HANDLE) CreateMutexW(void *s, BOOL d, const WCHAR *n) { (void)s; (void)d; (void)n; return nuevo_obj(O_MUTEX); }
API(HANDLE) OpenMutexA(u32 a, BOOL h, const char *n) { (void)a; (void)h; (void)n; pon_error(2); return 0; }
API(BOOL) ReleaseMutex(HANDLE h) { (void)h; return TRUE; }
API(HANDLE) CreateSemaphoreA(void *s, i32 ini, i32 max, const char *n) { HANDLE h = nuevo_obj(O_SEMAFORO); (void)s; (void)max; (void)n; if (h) obj(h)->a = ini; return h; }
API(BOOL) ReleaseSemaphore(HANDLE h, i32 n, i32 *antes) { Obj *o = obj(h); if (antes) *antes = o ? (i32)o->a : 0; if (o) o->a += n; return TRUE; }

int hilo_n;
static void *hilo_vuelta[5];
static u32 hilo_codigo;
typedef u32 (WINAPI *HILO)(void *);
static int corre_hilo(HANDLE h);
API(HANDLE) CreateThread(void *s, u32 pila, HILO f, void *arg, u32 fl, u32 *id)
{
	HANDLE h = nuevo_obj(O_HILO);
	Obj *o = obj(h);
	(void)s; (void)pila;
	if (!o)
		return 0;
	o->f = (void *)f;
	o->arg = arg;
	o->codigo = 259;              /* STILL_ACTIVE */
	if (id)
		*id = sys_tarea() + (hilo_n + 1) * 0x10000;
	traza("CreateThread(%x, %x)%s", (u32)f, (u32)arg, fl & 4 ? " (dormido)" : "");
	if (!(fl & 4))
		corre_hilo(h);
	return h;
}
/* corre el hilo entero (ExitThread vuelve aquí) */
static int corre_hilo(HANDLE h)
{
	Obj *o = obj(h);
	u32 c;
	if (!o || o->hecho || hilo_n >= 8)
		return 0;
	o->hecho = 1;
	{
		void *guarda[5];
		memcpy(guarda, hilo_vuelta, sizeof guarda);
		hilo_n++;
		if (__builtin_setjmp(hilo_vuelta) == 0)
			c = ((HILO)o->f)(o->arg);
		else
			c = hilo_codigo;
		hilo_n--;
		memcpy(hilo_vuelta, guarda, sizeof guarda);
	}
	o = obj(h);
	if (o)
		o->codigo = c;
	return 1;
}
API(void) ExitThread(u32 c)
{
	if (!hilo_n)
		w32_salir(c);
	hilo_codigo = c;
	__builtin_longjmp(hilo_vuelta, 1);
}
API(u32) ResumeThread(HANDLE h) { corre_hilo(h); return 1; }
API(u32) SuspendThread(HANDLE h) { (void)h; return 0; }
API(BOOL) TerminateThread(HANDLE h, u32 c) { Obj *o = obj(h); if (o) { o->hecho = 1; o->codigo = c; } return TRUE; }
API(BOOL) GetExitCodeThread(HANDLE h, u32 *c) { Obj *o = obj(h); *c = o ? o->codigo : 0; return o != 0; }
API(BOOL) SetThreadPriority(HANDLE h, i32 p) { (void)h; (void)p; return TRUE; }
API(i32) GetThreadPriority(HANDLE h) { (void)h; return 0; }
API(BOOL) SetPriorityClass(HANDLE h, u32 p) { (void)h; (void)p; return TRUE; }
API(u32) GetPriorityClass(HANDLE h) { (void)h; return 0x20; }
API(BOOL) SwitchToThread(void) { sys_dormir(0); return TRUE; }
API(u32) WaitForSingleObject(HANDLE h, u32 ms)
{
	Obj *o = obj(h);
	(void)ms;
	if (o && o->tipo == O_PROCESO) {
		if (!o->hecho) {
			if (ms == 0) {
				u32 r = sys_esperar(o->a, 1);
				if (r == 0xFFFFFF00u)
					return 258;          /* WAIT_TIMEOUT */
				o->codigo = r;
			} else
				o->codigo = sys_esperar(o->a, 0);
			o->hecho = 1;
		}
		return 0;
	}
	if (o && o->tipo == O_HILO && !o->hecho)
		corre_hilo(h);
	if (o && o->tipo == O_EVENTO && !o->a && ms == 0)
		return 258;
	if (o && o->tipo == O_EVENTO && !o->b)
		o->a = 0;
	return 0;                        /* WAIT_OBJECT_0 */
}
API(u32) WaitForSingleObjectEx(HANDLE h, u32 ms, BOOL a) { (void)a; return WaitForSingleObject(h, ms); }
API(u32) WaitForMultipleObjects(u32 n, const HANDLE *h, BOOL todos, u32 ms)
{
	u32 k;
	for (k = 0; k < n; k++)
		if (WaitForSingleObject(h[k], todos ? ms : 0) == 0 && !todos)
			return k;
	return todos ? 0 : 258;
}
API(u32) MsgWaitForMultipleObjects(u32 n, const HANDLE *h, BOOL todos, u32 ms, u32 m)
{
	(void)m;
	if (n)
		return WaitForMultipleObjects(n, h, todos, 0) == 258 ? n : 0;
	if (ms)
		Sleep(ms > 10 ? 10 : ms);
	return n;
}
u32 tls_usados[2];
API(u32) TlsAlloc(void)
{
	u32 k;
	for (k = 0; k < 64; k++)
		if (!(tls_usados[k / 32] & (1u << (k % 32)))) {
			tls_usados[k / 32] |= 1u << (k % 32);
			TEB[0xE10 / 4 + k] = 0;
			return k;
		}
	return 0xFFFFFFFF;
}
API(BOOL) TlsFree(u32 k) { if (k < 64) tls_usados[k / 32] &= ~(1u << (k % 32)); return k < 64; }
API(void *) TlsGetValue(u32 k) { pon_error(0); return k < 64 ? (void *)TEB[0xE10 / 4 + k] : 0; }
API(BOOL) TlsSetValue(u32 k, void *v) { if (k >= 64) return FALSE; TEB[0xE10 / 4 + k] = (u32)v; return TRUE; }
API(u32) FlsAlloc(void *f) { (void)f; return TlsAlloc(); }
API(BOOL) FlsFree(u32 k) { return TlsFree(k); }
API(void *) FlsGetValue(u32 k) { return TlsGetValue(k); }
API(BOOL) FlsSetValue(u32 k, void *v) { return TlsSetValue(k, v); }

/* ---- los procesos ---- */
typedef struct { HANDLE proc, hilo; u32 pid, tid; } PROCESS_INFORMATION;
API(BOOL) CreateProcessA(const char *app, char *linea, void *a, void *b, BOOL her, u32 fl, void *ent, const char *dir, void *si, PROCESS_INFORMATION *pi)
{
	char prog[200], args[300], c1[200], c2[300];
	const char *p;
	u32 t;
	HANDLE h;
	(void)a; (void)b; (void)her; (void)fl; (void)ent; (void)si;
	args[0] = 0;
	p = linea ? linea : "";
	/* el programa: «app» o lo primero de la línea */
	{
		u32 k = 0;
		while (*p == ' ')
			p++;
		if (*p == '"') {
			p++;
			while (*p && *p != '"' && k + 1 < sizeof prog)
				prog[k++] = *p++;
			if (*p == '"')
				p++;
		} else
			while (*p && *p != ' ' && k + 1 < sizeof prog)
				prog[k++] = *p++;
		prog[k] = 0;
		while (*p == ' ')
			p++;
		copia_n(args, p, sizeof args);
	}
	if (app)
		copia_n(prog, app, sizeof prog);
	if (!strchr(prog, '.'))
		strcat(prog, ".EXE");
	{
		char e[200];
		Entrada en;
		ruta_entera(prog, e, sizeof e);
		if (!entrada_de(e, &en, 0) && !strchr(prog, '\\')) {
			/* en la carpeta del programa, o en la de Win32 */
			char d[200], *q;
			copia_n(d, ruta_exe, sizeof d);
			q = strrchr(d, '\\');
			if (q)
				q[1] = 0;
			strcat(d, prog);
			if (entrada_de(d, &en, 0))
				copia_n(e, d, sizeof e);
		}
		a437(c1, e, sizeof c1);
	}
	a437(c2, args, sizeof c2);
	if (dir) {
		char d[200];
		ruta_entera(dir, d, sizeof d);
		SetCurrentDirectoryA(d);
	}
	t = sys_lanzar(c1, c2[0] ? c2 : 0, 0);
	traza("CreateProcessA(%s, %s) = %x", c1, c2, t);
	if (ES_ERROR(t)) {
		pon_error(error_de(t));
		return FALSE;
	}
	h = nuevo_obj(O_PROCESO);
	obj(h)->a = t;
	if (pi) {
		pi->proc = h;
		pi->hilo = nuevo_obj(O_MUTEX);
		pi->pid = t;
		pi->tid = t;
	}
	return TRUE;
}
API(BOOL) GetExitCodeProcess(HANDLE h, u32 *c)
{
	Obj *o = obj(h);
	if (h == (HANDLE)-1) { *c = 259; return TRUE; }
	if (!o)
		return FALSE;
	if (!o->hecho) {
		u32 r = sys_esperar(o->a, 1);
		if (r == 0xFFFFFF00u) {
			*c = 259;
			return TRUE;
		}
		o->hecho = 1;
		o->codigo = r;
	}
	*c = o->codigo;
	return TRUE;
}
API(u32) WinExec(const char *l, u32 v)
{
	PROCESS_INFORMATION pi;
	(void)v;
	if (!CreateProcessA(0, (char *)l, 0, 0, 0, 0, 0, 0, 0, &pi))
		return 2;
	return 33;
}
API(HANDLE) OpenProcess(u32 a, BOOL h, u32 id) { (void)a; (void)h; (void)id; pon_error(5); return 0; }
API(BOOL) DuplicateHandle(HANDLE p1, HANDLE h, HANDLE p2, HANDLE *d, u32 a, BOOL her, u32 o)
{
	(void)p1; (void)p2; (void)a; (void)her; (void)o;
	*d = h;
	return TRUE;
}

/* ---- los átomos ---- */
#define MAX_ATOMOS 128
static char *atomos[MAX_ATOMOS];
API(u16) FindAtomA(const char *n)
{
	int k;
	if ((u32)n < 0x10000)
		return (u16)(u32)n;
	for (k = 0; k < MAX_ATOMOS; k++)
		if (atomos[k] && !stricmp_(atomos[k], n))
			return 0xC000 + k;
	pon_error(2);
	return 0;
}
API(u16) AddAtomA(const char *n)
{
	int k;
	u16 a = (u32)n < 0x10000 ? (u16)(u32)n : 0;
	if (a)
		return a;
	if ((a = FindAtomA(n)))
		return a;
	for (k = 0; k < MAX_ATOMOS; k++)
		if (!atomos[k]) {
			atomos[k] = m_pedir(strlen(n) + 1);
			strcpy(atomos[k], n);
			return 0xC000 + k;
		}
	return 0;
}
API(u16) DeleteAtom(u16 a)
{
	if (a >= 0xC000 && a < 0xC000 + MAX_ATOMOS && atomos[a - 0xC000]) {
		m_soltar(atomos[a - 0xC000]);
		atomos[a - 0xC000] = 0;
		return 0;
	}
	return a;
}
API(u32) GetAtomNameA(u16 a, char *d, i32 n)
{
	if (a >= 0xC000 && a < 0xC000 + MAX_ATOMOS && atomos[a - 0xC000]) {
		copia_n(d, atomos[a - 0xC000], n);
		return strlen(d);
	}
	if (a < 0xC000) {
		fmt(d, n, "#%u", a);
		return strlen(d);
	}
	return 0;
}
API(u16) GlobalAddAtomA(const char *n) { return AddAtomA(n); }
API(u16) GlobalFindAtomA(const char *n) { return FindAtomA(n); }
API(u16) GlobalDeleteAtom(u16 a) { return DeleteAtom(a); }
API(u32) GlobalGetAtomNameA(u16 a, char *d, i32 n) { return GetAtomNameA(a, d, n); }

/* ---- la memoria ---- */
API(void *) VirtualAlloc(void *dir, u32 n, u32 tipo, u32 prot)
{
	u32 d = (u32)dir, r;
	(void)prot;
	if (d && (tipo & 0x1000) && !(tipo & 0x2000) && v_hay(d, n)) {
		/* poner las páginas de lo ya reservado */
		if (ES_ERROR(sys_memoria(d & ~4095u, ((d & 4095) + n + 4095) & ~4095u, 0))) {
			pon_error(8);
			return 0;
		}
		return (void *)(d & ~4095u);
	}
	if (d)
		d &= ~0xFFFFu;
	/* reservar: se ponen ya las páginas si no es muy grande */
	r = v_reservar(d, n, (tipo & 0x1000) || n <= 0x4000000);
	traza("VirtualAlloc(%x, %x, %x) = %x", (u32)dir, n, tipo, r);
	if (!r) {
		pon_error(d ? 487 : 8);
		return 0;
	}
	if (!(tipo & 0x1000) && n > 0x4000000) {
		/* (se dirá MEM_COMMIT luego) */
	}
	return (void *)r;
}
API(BOOL) VirtualFree(void *dir, u32 n, u32 tipo)
{
	if (tipo & 0x8000)
		return v_soltar((u32)dir, 0);
	/* MEM_DECOMMIT: las páginas se quedan (a cero no hace falta) */
	(void)n;
	return TRUE;
}
API(BOOL) VirtualProtect(void *d, u32 n, u32 p, u32 *antes) { (void)d; (void)n; (void)p; if (antes) *antes = 4; return TRUE; }
typedef struct { void *base, *abase; u32 aprot, tam, estado, prot, tipo; } MBI;
API(u32) VirtualQuery(const void *d, MBI *m, u32 n)
{
	u32 b = v_hay((u32)d, 1);
	extern u32 v_tam_zona(u32);
	(void)n;
	memset(m, 0, sizeof *m);
	m->base = (void *)((u32)d & ~4095u);
	if (b) {
		m->abase = (void *)b;
		m->tam = b + v_tam_zona(b) - (u32)m->base;
		m->estado = 0x1000;
		m->prot = m->aprot = 4;
		m->tipo = 0x20000;
	} else {
		m->tam = 4096;
		m->estado = 0x10000;
		m->prot = 1;
	}
	return sizeof *m;
}
API(HANDLE) GetProcessHeap(void) { return (HANDLE)0x00150000; }
API(u32) GetProcessHeaps(u32 n, HANDLE *h) { if (n) h[0] = GetProcessHeap(); return 1; }
API(HANDLE) HeapCreate(u32 o, u32 i, u32 m) { (void)o; (void)i; (void)m; return (HANDLE)0x00160000; }
API(BOOL) HeapDestroy(HANDLE h) { (void)h; return TRUE; }
API(void *) HeapAlloc(HANDLE h, u32 fl, u32 n)
{
	void *p = (fl & 8) ? m_cero(n) : m_pedir(n);
	(void)h;
	if (!p)
		pon_error(8);
	return p;
}
API(void *) HeapReAlloc(HANDLE h, u32 fl, void *p, u32 n)
{
	u32 v = m_tam(p);
	void *q;
	(void)h;
	if (fl & 0x10) {           /* HEAP_REALLOC_IN_PLACE_ONLY */
		return n <= v ? p : 0;
	}
	q = m_cambiar(p, n);
	if (q && (fl & 8) && n > v)
		memset((u8 *)q + v, 0, n - v);
	return q;
}
API(BOOL) HeapFree(HANDLE h, u32 fl, void *p) { (void)h; (void)fl; m_soltar(p); return TRUE; }
API(u32) HeapSize(HANDLE h, u32 fl, const void *p) { (void)h; (void)fl; return p ? m_tam((void *)p) : (u32)-1; }
API(BOOL) HeapValidate(HANDLE h, u32 fl, const void *p) { (void)h; (void)fl; (void)p; return TRUE; }
API(u32) HeapCompact(HANDLE h, u32 fl) { (void)h; (void)fl; return 0x100000; }
API(BOOL) HeapLock(HANDLE h) { (void)h; return TRUE; }
API(BOOL) HeapUnlock(HANDLE h) { (void)h; return TRUE; }
API(BOOL) HeapSetInformation(HANDLE h, u32 c, void *i, u32 n) { (void)h; (void)c; (void)i; (void)n; return TRUE; }
/* GlobalAlloc y LocalAlloc: el puntero es el manejador (GMEM_MOVEABLE igual) */
API(HGLOBAL) GlobalAlloc(u32 fl, u32 n) { void *p = (fl & 0x40) ? m_cero(n ? n : 1) : m_pedir(n ? n : 1); if (!p) pon_error(8); return p; }
API(HGLOBAL) GlobalReAlloc(HGLOBAL h, u32 n, u32 fl)
{
	u32 v = m_tam(h);
	void *q;
	if (fl & 0x80)               /* GMEM_MODIFY */
		return h;
	q = m_cambiar(h, n ? n : 1);
	if (q && (fl & 0x40) && n > v)
		memset((u8 *)q + v, 0, n - v);
	return q;
}
API(void *) GlobalLock(HGLOBAL h) { return h; }
API(BOOL) GlobalUnlock(HGLOBAL h) { (void)h; return TRUE; }
API(HGLOBAL) GlobalFree(HGLOBAL h) { m_soltar(h); return 0; }
API(u32) GlobalSize(HGLOBAL h) { return m_tam(h); }
API(HGLOBAL) GlobalHandle(const void *p) { return (HGLOBAL)p; }
API(u32) GlobalFlags(HGLOBAL h) { (void)h; return 0; }
API(u32) GlobalCompact(u32 n) { (void)n; return 0x1000000; }
API(HGLOBAL) LocalAlloc(u32 fl, u32 n) { return GlobalAlloc(fl, n); }
API(HGLOBAL) LocalReAlloc(HGLOBAL h, u32 n, u32 fl) { return GlobalReAlloc(h, n, fl); }
API(void *) LocalLock(HGLOBAL h) { return h; }
API(BOOL) LocalUnlock(HGLOBAL h) { (void)h; return TRUE; }
API(HGLOBAL) LocalFree(HGLOBAL h) { m_soltar(h); return 0; }
API(u32) LocalSize(HGLOBAL h) { return m_tam(h); }
API(HGLOBAL) LocalHandle(const void *p) { return (HGLOBAL)p; }

/* ---- los recursos ---- */
static int mismo_nombre(u32 rsrc, u32 e, const char *n)
{
	u16 *s = (u16 *)(rsrc + (e & 0x7FFFFFFF));
	u32 k;
	for (k = 0; k < s[0]; k++)
		if (!n[k] || mayus(s[1 + k]) != mayus(n[k]))
			return 0;
	return n[k] == 0;
}
static u32 rs_busca(u32 rsrc, u32 dir, const char *n)
{
	u16 nn = *(u16 *)(dir + 12), ni = *(u16 *)(dir + 14);
	u32 *e = (u32 *)(dir + 16), k;
	if ((u32)n < 0x10000) {
		for (k = nn; k < (u32)nn + ni; k++)
			if (e[k * 2] == (u32)n)
				return e[k * 2 + 1];
		return 0;
	}
	if (n[0] == '#') {
		u32 v = 0;
		const char *p = n + 1;
		while (*p >= '0' && *p <= '9')
			v = v * 10 + *p++ - '0';
		return rs_busca(rsrc, dir, (const char *)v);
	}
	for (k = 0; k < nn; k++)
		if ((e[k * 2] & 0x80000000) && mismo_nombre(rsrc, e[k * 2], n))
			return e[k * 2 + 1];
	return 0;
}
/* el recurso (tipo, nombre) del módulo: su ficha de datos (o 0) */
u32 recurso_ficha(Modulo *m, const char *tipo, const char *nombre)
{
	u32 e, d;
	if (!m || !m->rsrc)
		return 0;
	e = rs_busca(m->rsrc, m->rsrc, tipo);
	if (!(e & 0x80000000))
		return 0;
	e = rs_busca(m->rsrc, m->rsrc + (e & 0x7FFFFFFF), nombre);
	if (!(e & 0x80000000))
		return 0;
	/* el idioma: el primero */
	d = m->rsrc + (e & 0x7FFFFFFF);
	if (*(u16 *)(d + 12) + *(u16 *)(d + 14) == 0)
		return 0;
	e = *(u32 *)(d + 16 + 4);
	if (e & 0x80000000)
		return 0;
	return m->rsrc + e;
}
void *recurso(HMODULE h, const char *tipo, const char *nombre, u32 *tam)
{
	Modulo *m = mod_de(h);
	u32 f = recurso_ficha(m, tipo, nombre);
	if (!f)
		return 0;
	if (tam)
		*tam = *(u32 *)(f + 4);
	return (void *)(m->base + *(u32 *)f);
}
API(HRSRC) FindResourceA(HMODULE h, const char *n, const char *t)
{
	Modulo *m = mod_de(h);
	u32 f = recurso_ficha(m, t, n);
	if (!f)
		pon_error(1814);
	return (HRSRC)f;
}
API(HRSRC) FindResourceExA(HMODULE h, const char *t, const char *n, u16 l) { (void)l; return FindResourceA(h, n, t); }
API(HRSRC) FindResourceW(HMODULE h, const WCHAR *n, const WCHAR *t)
{
	char a[64], b[64];
	if ((u32)n >= 0x10000) de_w(a, n, sizeof a);
	if ((u32)t >= 0x10000) de_w(b, t, sizeof b);
	return FindResourceA(h, (u32)n < 0x10000 ? (const char *)n : a, (u32)t < 0x10000 ? (const char *)t : b);
}
API(HGLOBAL) LoadResource(HMODULE h, HRSRC r)
{
	Modulo *m = mod_de(h);
	if (!r || !m)
		return 0;
	return (HGLOBAL)(m->base + *(u32 *)r);
}
API(void *) LockResource(HGLOBAL h) { return h; }
API(BOOL) FreeResource(HGLOBAL h) { (void)h; return FALSE; }
API(u32) SizeofResource(HMODULE h, HRSRC r) { (void)h; return r ? *(u32 *)((u32)r + 4) : 0; }

/* ---- los .INI ---- */
static char *ini_leer(const char *archivo, u32 *n)
{
	char r[200], c[200], *b;
	u32 m, t;
	if (!strchr(archivo, '\\') && !strchr(archivo, ':'))
		fmt(r, sizeof r, "%s%s", carpeta_w32, archivo);
	else
		copia_n(r, archivo, sizeof r);
	a437(c, r, sizeof c);
	m = sys_abrir(c, 0);
	if (ES_ERROR(m)) {
		*n = 0;
		return 0;
	}
	t = sys_largo(m);
	b = m_pedir(t + 2);
	*n = sys_leer(m, b, t);
	sys_cerrar(m);
	if (ES_ERROR(*n))
		*n = 0;
	b[*n] = 0;
	return b;
}
static void ini_ruta(const char *archivo, char *c, u32 cabe)
{
	char r[200];
	if (!strchr(archivo, '\\') && !strchr(archivo, ':'))
		fmt(r, sizeof r, "%s%s", carpeta_w32, archivo);
	else
		copia_n(r, archivo, sizeof r);
	a437(c, r, cabe);
}
/* la línea siguiente (sin espacios a los lados); da dónde sigue */
static char *linea_ini(char *p, char *d, u32 cabe)
{
	u32 k = 0;
	while (*p == ' ' || *p == '\t')
		p++;
	while (*p && *p != '\r' && *p != '\n') {
		if (k + 1 < cabe)
			d[k++] = *p;
		p++;
	}
	while (k && (d[k - 1] == ' ' || d[k - 1] == '\t'))
		k--;
	d[k] = 0;
	while (*p == '\r' || *p == '\n')
		p++;
	return p;
}
static u32 pon_lista(char *d, u32 cabe, u32 o, const char *s)
{
	u32 l = strlen(s);
	if (o + l + 2 > cabe)
		return o;
	strcpy(d + o, s);
	return o + l + 1;
}
API(u32) GetPrivateProfileStringA(const char *sec, const char *clave, const char *def, char *d, u32 cabe, const char *archivo)
{
	u32 n, o = 0;
	char *b = ini_leer(archivo ? archivo : "WIN.INI", &n), *p, l[300], secact[100];
	int dentro = 0;
	if (!cabe)
		return 0;
	d[0] = 0;
	secact[0] = 0;
	if (b) {
		for (p = b; *p; ) {
			p = linea_ini(p, l, sizeof l);
			if (l[0] == '[') {
				char *f = strchr(l, ']');
				if (f)
					*f = 0;
				copia_n(secact, l + 1, sizeof secact);
				dentro = sec && !stricmp_(secact, sec);
				if (!sec)
					o = pon_lista(d, cabe, o, secact);
				continue;
			}
			if (!dentro || l[0] == ';' || !l[0])
				continue;
			{
				char *ig = strchr(l, '='), *v;
				if (!ig)
					continue;
				*ig = 0;
				v = ig + 1;
				{
					u32 k = strlen(l);
					while (k && (l[k - 1] == ' ' || l[k - 1] == '\t'))
						l[--k] = 0;
				}
				while (*v == ' ' || *v == '\t')
					v++;
				if (!clave) {
					o = pon_lista(d, cabe, o, l);
					continue;
				}
				if (!stricmp_(l, clave)) {
					u32 k = strlen(v);
					if (k >= 2 && (v[0] == '"' || v[0] == '\'') && v[k - 1] == v[0]) {
						v[k - 1] = 0;
						v++;
					}
					copia_n(d, v, cabe);
					m_soltar(b);
					return strlen(d);
				}
			}
		}
		m_soltar(b);
	}
	if (!sec || !clave) {
		if (o + 1 < cabe)
			d[o] = 0;
		else if (cabe >= 2) {
			d[cabe - 2] = d[cabe - 1] = 0;
			return cabe - 2;
		}
		return o;
	}
	copia_n(d, def ? def : "", cabe);
	{
		u32 k = strlen(d);
		while (k && d[k - 1] == ' ')
			d[--k] = 0;
	}
	return strlen(d);
}
API(u32) GetPrivateProfileIntA(const char *sec, const char *clave, i32 def, const char *archivo)
{
	char b[40], *p = b;
	i32 v = 0, neg = 0;
	if (!GetPrivateProfileStringA(sec, clave, "", b, sizeof b, archivo))
		return def;
	if (*p == '-')
		neg = 1, p++;
	if (*p < '0' || *p > '9')
		return 0;
	while (*p >= '0' && *p <= '9')
		v = v * 10 + *p++ - '0';
	return neg ? -v : v;
}
API(BOOL) WritePrivateProfileStringA(const char *sec, const char *clave, const char *val, const char *archivo)
{
	u32 n, o = 0, m;
	char *b = ini_leer(archivo ? archivo : "WIN.INI", &n), *p, l[300], *sal, c[200];
	int dentro = 0, hecho = 0, vista = 0;
	if (!sec)
		return TRUE;
	sal = m_pedir(n + 600);
	if (b) {
		for (p = b; *p; ) {
			char *ini = p;
			p = linea_ini(p, l, sizeof l);
			if (l[0] == '[') {
				char nom[100], *f;
				if (dentro && !hecho && clave && val) {
					o += fmt(sal + o, 600 + n - o, "%s=%s\r\n", clave, val);
					hecho = 1;
				}
				copia_n(nom, l + 1, sizeof nom);
				f = strchr(nom, ']');
				if (f)
					*f = 0;
				dentro = !stricmp_(nom, sec);
				if (dentro)
					vista = 1;
				if (dentro && !clave)
					continue;
			} else if (dentro) {
				char k[100], *ig = strchr(l, '=');
				if (!clave)
					continue;
				if (ig) {
					u32 t = ig - l;
					if (t >= sizeof k)
						t = sizeof k - 1;
					memcpy(k, l, t);
					k[t] = 0;
					while (t && (k[t - 1] == ' ' || k[t - 1] == '\t'))
						k[--t] = 0;
					if (!stricmp_(k, clave)) {
						if (val && !hecho)
							o += fmt(sal + o, 600 + n - o, "%s=%s\r\n", clave, val);
						hecho = 1;
						continue;
					}
				}
			}
			(void)ini;
			o += fmt(sal + o, 600 + n - o, "%s\r\n", l);
		}
		m_soltar(b);
	}
	if (!hecho && clave && val) {
		if (!vista || !dentro)
			if (!vista)
				o += fmt(sal + o, 600 + n - o, "[%s]\r\n", sec);
		if (vista && !dentro) {
			/* la sección estaba antes: se añade otra vez al final (vale igual) */
			o += fmt(sal + o, 600 + n - o, "[%s]\r\n", sec);
		}
		o += fmt(sal + o, 600 + n - o, "%s=%s\r\n", clave, val);
	}
	ini_ruta(archivo ? archivo : "WIN.INI", c, sizeof c);
	m = sys_abrir(c, 2);
	if (ES_ERROR(m)) {
		m_soltar(sal);
		pon_error(error_de(m));
		return FALSE;
	}
	sys_escribir_archivo(m, sal, o);
	sys_cerrar(m);
	m_soltar(sal);
	return TRUE;
}
API(u32) GetProfileStringA(const char *s, const char *k, const char *d, char *b, u32 n) { return GetPrivateProfileStringA(s, k, d, b, n, "WIN.INI"); }
API(u32) GetProfileIntA(const char *s, const char *k, i32 d) { return GetPrivateProfileIntA(s, k, d, "WIN.INI"); }
API(BOOL) WriteProfileStringA(const char *s, const char *k, const char *v) { return WritePrivateProfileStringA(s, k, v, "WIN.INI"); }
API(u32) GetPrivateProfileSectionA(const char *s, char *d, u32 n, const char *a)
{
	char claves[2000], *p;
	u32 o = 0;
	GetPrivateProfileStringA(s, 0, "", claves, sizeof claves, a);
	for (p = claves; *p; p += strlen(p) + 1) {
		char v[300], l[400];
		GetPrivateProfileStringA(s, p, "", v, sizeof v, a);
		fmt(l, sizeof l, "%s=%s", p, v);
		o = pon_lista(d, n, o, l);
	}
	if (o < n)
		d[o] = 0;
	return o;
}

/* ---- FormatMessage, las fechas en texto ---- */
API(u32) FormatMessageA(u32 fl, const void *f, u32 id, u32 l, char *d, u32 n, va_list *a)
{
	char t[200];
	(void)f; (void)l; (void)a;
	switch (id) {
	case 2: strcpy(t, "No se encuentra el archivo.\r\n"); break;
	case 3: strcpy(t, "No se encuentra la ruta.\r\n"); break;
	case 5: strcpy(t, "Acceso denegado.\r\n"); break;
	case 8: strcpy(t, "No hay memoria suficiente.\r\n"); break;
	default: fmt(t, sizeof t, "Error %u.\r\n", id);
	}
	if (fl & 0x100) {               /* FORMAT_MESSAGE_ALLOCATE_BUFFER */
		char *p = m_pedir(strlen(t) + 1);
		strcpy(p, t);
		*(char **)d = p;
		return strlen(t);
	}
	copia_n(d, t, n);
	return strlen(d);
}
API(i32) GetDateFormatA(u32 lc, u32 fl, const SYSTEMTIME *s, const char *f, char *d, i32 n)
{
	SYSTEMTIME h;
	char t[40];
	(void)lc; (void)fl; (void)f;
	if (!s) { ahora(&h); s = &h; }
	fmt(t, sizeof t, "%02u/%02u/%04u", s->d, s->m, s->a);
	if (!n)
		return strlen(t) + 1;
	copia_n(d, t, n);
	return strlen(d) + 1;
}
API(i32) GetTimeFormatA(u32 lc, u32 fl, const SYSTEMTIME *s, const char *f, char *d, i32 n)
{
	SYSTEMTIME h;
	char t[40];
	(void)lc; (void)fl; (void)f;
	if (!s) { ahora(&h); s = &h; }
	fmt(t, sizeof t, "%02u:%02u:%02u", s->h, s->mi, s->s);
	if (!n)
		return strlen(t) + 1;
	copia_n(d, t, n);
	return strlen(d) + 1;
}
API(BOOL) GetComputerNameA(char *d, u32 *n)
{
	char b[64], c[64];
	b[0] = 0;
	sc(68, 3, LIN(b), 0);
	if (!b[0])
		strcpy(b, "HISPA-PENTIUM");
	a1252(c, b, sizeof c);
	copia_n(d, c, *n);
	*n = strlen(d);
	return TRUE;
}
API(u32) GetSystemDirectoryA(char *d, u32 n)
{
	char t[160];
	copia_n(t, carpeta_w32, sizeof t);
	if (strlen(t) > 3 && t[strlen(t) - 1] == '\\')
		t[strlen(t) - 1] = 0;
	if (n <= strlen(t))
		return strlen(t) + 1;
	strcpy(d, t);
	return strlen(d);
}
API(u32) GetWindowsDirectoryA(char *d, u32 n) { return GetSystemDirectoryA(d, n); }
API(u32) GetSystemWindowsDirectoryA(char *d, u32 n) { return GetSystemDirectoryA(d, n); }
API(u32) GetTempPathA(u32 n, char *d)
{
	char t[8];
	fmt(t, sizeof t, "%c:\\", carpeta_w32[0] ? carpeta_w32[0] : 'C');
	if (n <= strlen(t))
		return strlen(t) + 1;
	strcpy(d, t);
	return strlen(d);
}
API(u32) GetTempPathW(u32 n, WCHAR *d)
{
	char b[16];
	u32 r = GetTempPathA(sizeof b, b);
	if (n <= r)
		return r + 1;
	a_w(d, b, n);
	return r;
}
