/* =============================================================================
 *  sistema.c - NAVEGA.BIN, el Navegador de Hispa-Pentium: lo de Windows que
 *  piden la biblioteca de C de Hispa-486 (w32/msvcrt.c), NetSurf (w32/surf)
 *  y QuickJS (w32/js/hspjs.c), hecho con las llamadas al sistema de
 *  Hispa-Pentium (docs/PENTIUM.md §6 y §14). En Hispa-486 esto lo daban
 *  KERNEL32, USER32, WSOCK32 y HSPTLS (HW32); aquí, con los mismos nombres y
 *  la misma manera de llamar (stdcall), los fuentes de allí no se tocan:
 *    - los archivos y las carpetas (CreateFileA, FindFirstFileA...), con los
 *      manejadores del núcleo (16 abiertos a la vez en todo el sistema);
 *    - la memoria: VirtualAlloc con la llamada 17 (el final del programa),
 *      con los trozos sueltos guardados para volver a darlos; HeapAlloc, con
 *      el malloc de w32/surf/memoria.c (uno solo para todo el programa);
 *    - la hora: la del reloj del PC (como en HW32, sin huso) y los tics;
 *    - la red: los «sockets» de WSOCK32 con las conexiones TCP de RED.BIN
 *      (llamada 90) y las de HSPTLS con las cifradas de TLS.BIN (llamada 99),
 *      tal como las usa w32/navega/http.c.
 *  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * ============================================================================= */
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include "pentium.h"

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;
typedef void *HANDLE;
#define WINAPI __attribute__((stdcall))
#define NULL_H ((HANDLE)0)
#define INVALIDO ((HANDLE)-1)

void *memcpy(void *, const void *, size_t);
void *memset(void *, int, size_t);
size_t strlen(const char *);
int vsnprintf(char *, size_t, const char *, va_list);
void *malloc(size_t);
void *calloc(size_t, size_t);
void *realloc(void *, size_t);
void free(void *);

static u32 ultimo_error;
static void pon_error(u32 e)
{
	ultimo_error = e == E_NO_HAY ? 2 : e == E_LLENO ? 4 : e == E_MAL ? 6 : e == E_SIN_MEMORIA ? 8 : e == E_DISCO_LLENO ? 112
	    : e == E_YA_EXISTE ? 80 : e == E_EN_USO ? 32 : e == E_PROTEGIDO ? 5 : e == E_NO_VACIA ? 145 : e == E_NOMBRE_MALO ? 123
	    : e == E_ES_CARPETA ? 5 : e == E_NO_CARPETA ? 3 : e == E_SIN_DISCO ? 21 : 87;
}
u32 WINAPI GetLastError(void) { return ultimo_error; }
void WINAPI SetLastError(u32 e) { ultimo_error = e; }

/* =============================================================================
 *  El puerto serie y los tics
 * ============================================================================= */
static u32 hz_v;
u32 pent_hz(void)
{
	if (!hz_v) {
		hz_v = sc(SC_DATO, DATO_HZ, 0, 0);
		if (!hz_v || ES_ERROR(hz_v))
			hz_v = 100;
	}
	return hz_v;
}
u32 pent_tics(void) { return sc(SC_TICS, 0, 0, 0); }
u32 WINAPI GetTickCount(void) { return (u32)((uint64_t)pent_tics() * 1000u / pent_hz()); }
void WINAPI Sleep(u32 ms)
{
	u32 t = (u32)(((uint64_t)ms * pent_hz() + 999) / 1000);
	sc(SC_DORMIR, t ? t : (ms ? 1 : 0), 0, 0);
}
void pent_serie(const char *f, ...)
{
	char b[400];
	va_list ap;
	int n;
	va_start(ap, f);
	n = vsnprintf(b, sizeof b - 2, f, ap);
	va_end(ap);
	if (n < 0)
		return;
	if (n > (int)sizeof b - 2)
		n = sizeof b - 2;
	b[n++] = '\n';
	sc(SC_SERIE, (u32)b, n, 0);
}

/* =============================================================================
 *  Acabar, los argumentos y dónde está el programa
 * ============================================================================= */
void WINAPI ExitProcess(u32 c)
{
	sc(SC_ACABAR, c, 0, 0);
	for (;;) ;
}
char pent_carpeta[260];
static char unidad_sistema(void)
{
	u32 u = sc(SC_UNIDAD, 0, 0, 0);
	return u >= 'A' && u <= 'Z' ? (char)u : u >= 'a' && u <= 'z' ? (char)(u - 32) : 'C';
}
static int existe(const char *n)
{
	u32 m = sc(SC_ABRIR, (u32)n, PARA_LEER, 0);
	if (ES_ERROR(m))
		return 0;
	sc(SC_CERRAR, m, 0, 0);
	return 1;
}
/* la carpeta del programa: \HISPA\NAVEGA (instalado, el paquete «navegador»)
 * o \NAVEGA (sin instalar); el núcleo no dice de dónde se cargó */
static void busca_carpeta(void)
{
	static const char *const sitios[] = { "\\HISPA\\NAVEGA", "\\NAVEGA", NULL };
	char n[300];
	int k;
	if (pent_carpeta[0])
		return;
	for (k = 0; sitios[k]; k++) {
		u32 i;
		for (i = 0; sitios[k][i]; i++)
			n[i] = sitios[k][i];
		memcpy(n + i, "\\NAVEGA.BIN", 12);
		if (existe(n))
			break;
	}
	if (!sitios[k])
		k = 0;
	pent_carpeta[0] = unidad_sistema();
	pent_carpeta[1] = ':';
	memcpy(pent_carpeta + 2, sitios[k], strlen(sitios[k]) + 1);
}
u32 WINAPI GetModuleFileNameA(HANDLE h, char *d, u32 cabe)
{
	u32 n;
	(void)h;
	busca_carpeta();
	n = strlen(pent_carpeta);
	if (n + 12 > cabe)
		return 0;
	memcpy(d, pent_carpeta, n);
	memcpy(d + n, "\\NAVEGA.BIN", 12);
	return n + 11;
}
HANDLE WINAPI GetModuleHandleA(const char *n) { (void)n; return (HANDLE)0x40000000; }
char *WINAPI GetCommandLineA(void)
{
	static char l[300];
	if (!l[0]) {
		u32 n;
		memcpy(l, "NAVEGA.BIN ", 11);
		n = sc(SC_ARGUMENTOS, (u32)(l + 11), sizeof l - 12, 0);
		if (ES_ERROR(n))
			l[11] = 0;
	}
	return l;
}
char *WINAPI GetEnvironmentStrings(void)
{
	static char e[4];
	return e;
}

/* =============================================================================
 *  La memoria: VirtualAlloc y VirtualFree con el final del programa (llamada
 *  17). Las zonas sueltas se apuntan y se vuelven a dar (juntando vecinas); la
 *  de arriba del todo, si queda libre, se devuelve al sistema.
 * ============================================================================= */
#define PAGINA 4096u
#define N_ZONAS 1024
static struct { char *p; u32 n; u8 libre; } zona[N_ZONAS];
static int n_zonas;
static u32 pedido_sistema;	/* lo que tiene el programa de más (octetos) */
u32 pent_memoria_kb(void) { return pedido_sistema >> 10; }
void *WINAPI VirtualAlloc(void *dir, u32 n, u32 tipo, u32 proteccion)
{
	int k, mejor = -1;
	u32 r;
	(void)dir; (void)tipo; (void)proteccion;
	if (!n)
		return NULL;
	n = (n + PAGINA - 1) & ~(PAGINA - 1);
	for (k = 0; k < n_zonas; k++)
		if (zona[k].libre && zona[k].n >= n && (mejor < 0 || zona[k].n < zona[mejor].n))
			mejor = k;
	if (mejor >= 0) {
		char *p = zona[mejor].p;
		if (zona[mejor].n > n && n_zonas < N_ZONAS) {
			for (k = n_zonas; k > mejor + 1; k--)
				zona[k] = zona[k - 1];
			n_zonas++;
			zona[mejor + 1].p = p + n;
			zona[mejor + 1].n = zona[mejor].n - n;
			zona[mejor + 1].libre = 1;
			zona[mejor].n = n;
		}
		zona[mejor].libre = 0;
		memset(p, 0, zona[mejor].n);
		return p;
	}
	if (n_zonas == N_ZONAS)
		return NULL;
	r = sc(SC_MEMORIA, n, 0, 0);
	if (ES_ERROR(r))
		return NULL;
	/* (el núcleo da las páginas a cero) */
	zona[n_zonas].p = (char *)r;
	zona[n_zonas].n = n;
	zona[n_zonas].libre = 0;
	n_zonas++;
	pedido_sistema += n;
	return (void *)r;
}
int WINAPI VirtualFree(void *dir, u32 n, u32 tipo)
{
	int k;
	(void)n; (void)tipo;
	for (k = 0; k < n_zonas; k++)
		if (zona[k].p == dir && !zona[k].libre)
			break;
	if (k == n_zonas)
		return 0;
	zona[k].libre = 1;
	/* juntar con la de detrás y con la de delante (si son seguidas) */
	if (k + 1 < n_zonas && zona[k + 1].libre && zona[k].p + zona[k].n == zona[k + 1].p) {
		int j;
		zona[k].n += zona[k + 1].n;
		for (j = k + 1; j + 1 < n_zonas; j++)
			zona[j] = zona[j + 1];
		n_zonas--;
	}
	if (k > 0 && zona[k - 1].libre && zona[k - 1].p + zona[k - 1].n == zona[k].p) {
		int j;
		zona[k - 1].n += zona[k].n;
		for (j = k; j + 1 < n_zonas; j++)
			zona[j] = zona[j + 1];
		n_zonas--;
		k--;
	}
	/* la de arriba del todo, libre: al sistema */
	if (k == n_zonas - 1 && zona[k].libre) {
		u32 fin = sc(SC_MEMORIA, 0, 0, 0);
		if ((char *)fin == zona[k].p + zona[k].n && !ES_ERROR(sc(SC_MEMORIA, 0u - zona[k].n, 0, 0))) {
			pedido_sistema -= zona[k].n;
			n_zonas--;
		}
	}
	return 1;
}
/* el montón de KERNEL32 (el de MSVCRT): el malloc de w32/surf/memoria.c */
HANDLE WINAPI GetProcessHeap(void) { return (HANDLE)1; }
void *WINAPI HeapAlloc(HANDLE h, u32 marcas, u32 n)
{
	(void)h;
	return marcas & 8 ? calloc(1, n ? n : 1) : malloc(n ? n : 1);
}
int WINAPI HeapFree(HANDLE h, u32 marcas, void *p)
{
	(void)h; (void)marcas;
	free(p);
	return 1;
}
/* lo que mide un trozo de memoria.c: su cabecera ([anterior][tamaño|marcas]) */
u32 WINAPI HeapSize(HANDLE h, u32 marcas, void *p)
{
	(void)h; (void)marcas;
	return p ? (((u32 *)p)[-1] & ~7u) - 8 : 0;
}
void *WINAPI HeapReAlloc(HANDLE h, u32 marcas, void *p, u32 n)
{
	(void)h;
	if (marcas & 0x10)		/* (solo donde está: si no cabe, nada) */
		return n <= HeapSize(h, 0, p) ? p : NULL;
	return realloc(p, n ? n : 1);
}

/* =============================================================================
 *  La hora: la del reloj del PC (dato 10, como la FAT: de 2 en 2 segundos),
 *  y los tics desde entonces; sin huso (como HW32 en Hispa-486)
 * ============================================================================= */
typedef struct { u32 lo, hi; } FT;
typedef struct { u16 anio, mes, sem, dia, hora, min, seg, ms; } ST;
static u32 dias_desde_1601(int a, int m, int d)
{
	/* (días desde el 1-1-1601, calendario gregoriano) */
	int y = a - (m <= 2), era, yoe, doy, doe;
	era = y / 400;
	yoe = y - era * 400;
	doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
	doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
	return (u32)(era * 146097 + doe - 584388);	/* (el 1-3-0 ... al 1-1-1601) */
}
static uint64_t base_100ns;
static u32 base_tic;
static uint64_t ahora_100ns(void)
{
	if (!base_100ns) {
		u32 f = sc(SC_DATO, DATO_FECHA, 0, 0), fe = f >> 16, ho = f & 0xFFFF;
		int a = 1980 + (fe >> 9), m = (fe >> 5) & 15, d = fe & 31;
		uint64_t s;
		if (m < 1 || m > 12) m = 1;
		if (d < 1) d = 1;
		s = (uint64_t)dias_desde_1601(a, m, d) * 86400u + (ho >> 11) * 3600u + ((ho >> 5) & 63) * 60u + (ho & 31) * 2u;
		base_100ns = s * 10000000u;
		base_tic = pent_tics();
	}
	return base_100ns + (uint64_t)(pent_tics() - base_tic) * (10000000u / pent_hz());
}
void WINAPI GetSystemTimeAsFileTime(FT *f)
{
	uint64_t t = ahora_100ns();
	f->lo = (u32)t;
	f->hi = (u32)(t >> 32);
}
static void civil(u32 z, int *a, int *m, int *d)
{
	/* de los días desde el 1-1-1601 al año, el mes y el día */
	int era, doe, yoe, y, doy, mp;
	z += 584388;
	era = (int)(z / 146097);
	doe = (int)(z - (u32)era * 146097);
	yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
	y = yoe + era * 400;
	doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
	mp = (5 * doy + 2) / 153;
	*d = doy - (153 * mp + 2) / 5 + 1;
	*m = mp < 10 ? mp + 3 : mp - 9;
	*a = y + (*m <= 2);
}
int WINAPI FileTimeToSystemTime(const FT *f, ST *s)
{
	uint64_t t = (uint64_t)f->hi << 32 | f->lo;
	u32 seg, dias;
	int a, m, d;
	s->ms = (u16)((t / 10000) % 1000);
	t /= 10000000u;
	dias = (u32)(t / 86400u);
	seg = (u32)(t % 86400u);
	civil(dias, &a, &m, &d);
	s->anio = (u16)a; s->mes = (u16)m; s->dia = (u16)d;
	s->sem = (u16)((dias + 1) % 7);		/* (el 1-1-1601 fue lunes) */
	s->hora = (u16)(seg / 3600); s->min = (u16)(seg / 60 % 60); s->seg = (u16)(seg % 60);
	return 1;
}
int WINAPI SystemTimeToFileTime(const ST *s, FT *f)
{
	uint64_t t;
	if (s->mes < 1 || s->mes > 12 || s->dia < 1 || s->dia > 31 || s->anio < 1601)
		return 0;
	t = ((uint64_t)dias_desde_1601(s->anio, s->mes, s->dia) * 86400u + s->hora * 3600u + s->min * 60u + s->seg) * 10000000u
	    + (uint64_t)s->ms * 10000u;
	f->lo = (u32)t;
	f->hi = (u32)(t >> 32);
	return 1;
}
void WINAPI GetLocalTime(ST *s)
{
	FT f;
	GetSystemTimeAsFileTime(&f);
	FileTimeToSystemTime(&f, s);
}
void WINAPI GetSystemTime(ST *s) { GetLocalTime(s); }
static void fat_a_ft(u16 fe, u16 ho, FT *f)
{
	ST s;
	s.anio = (u16)(1980 + (fe >> 9)); s.mes = (u16)((fe >> 5) & 15); s.dia = (u16)(fe & 31);
	s.hora = (u16)(ho >> 11); s.min = (u16)((ho >> 5) & 63); s.seg = (u16)((ho & 31) * 2); s.ms = 0;
	if (!SystemTimeToFileTime(&s, f))
		f->lo = f->hi = 0;
}

/* =============================================================================
 *  Los archivos
 * ============================================================================= */
#define N_ARCH 24
#define H_BASE 0x100
#define H_ENTRADA ((HANDLE)0x11)
#define H_SALIDA ((HANDLE)0x12)
#define H_ERRORES ((HANDLE)0x13)
static struct { u8 usado; u32 m; } arch[N_ARCH];
static int de_handle(HANDLE h)
{
	u32 k = (u32)h - H_BASE;
	return k < N_ARCH && arch[k].usado ? (int)k : -1;
}
HANDLE WINAPI GetStdHandle(int n) { return n == -10 ? H_ENTRADA : n == -11 ? H_SALIDA : H_ERRORES; }
u32 WINAPI GetFileType(HANDLE h) { return h == H_ENTRADA || h == H_SALIDA || h == H_ERRORES ? 2 : 1; }
/* el nombre, sin «\\?\» ni barras de Unix */
static void limpia(const char *n, char *d, u32 cabe)
{
	u32 k = 0;
	if (n[0] == '\\' && n[1] == '\\' && n[2] == '?' && n[3] == '\\')
		n += 4;
	for (; *n && k + 1 < cabe; n++)
		d[k++] = *n == '/' ? '\\' : *n;
	d[k] = 0;
}
static u32 abre(const char *n, u32 modo)
{
	char t[300];
	limpia(n, t, sizeof t);
	return sc(SC_ABRIR, (u32)t, modo, 0);
}
HANDLE WINAPI CreateFileA(const char *n, u32 acceso, u32 compartir, void *seg, u32 como, u32 marcas, HANDLE plantilla)
{
	u32 m;
	int k, escribe = (acceso & 0x40000000u) != 0;
	(void)compartir; (void)seg; (void)marcas; (void)plantilla;
	for (k = 0; k < N_ARCH && arch[k].usado; k++) ;
	if (k == N_ARCH) { ultimo_error = 4; return INVALIDO; }
	switch (como) {
	case 1:			/* CREATE_NEW */
		if (existe(n)) { ultimo_error = 80; return INVALIDO; }
		m = abre(n, PARA_CREAR);
		break;
	case 2:			/* CREATE_ALWAYS */
		m = abre(n, PARA_CREAR);
		break;
	case 4:			/* OPEN_ALWAYS */
		m = abre(n, escribe ? PARA_ESCRIBIR : PARA_LEER);
		if (m == E_NO_HAY)
			m = abre(n, PARA_CREAR);
		break;
	case 5:			/* TRUNCATE_EXISTING */
		if (!existe(n)) { ultimo_error = 2; return INVALIDO; }
		m = abre(n, PARA_CREAR);
		break;
	default:		/* OPEN_EXISTING */
		m = abre(n, escribe ? PARA_ESCRIBIR : PARA_LEER);
		break;
	}
	if (ES_ERROR(m)) { pon_error(m); return INVALIDO; }
	arch[k].usado = 1;
	arch[k].m = m;
	return (HANDLE)(H_BASE + k);
}
int WINAPI CloseHandle(HANDLE h)
{
	int k = de_handle(h);
	if (k < 0)
		return h == H_ENTRADA || h == H_SALIDA || h == H_ERRORES;
	sc(SC_CERRAR, arch[k].m, 0, 0);
	arch[k].usado = 0;
	return 1;
}
int WINAPI ReadFile(HANDLE h, void *b, u32 n, u32 *leidos, void *sol)
{
	int k = de_handle(h);
	u32 r;
	(void)sol;
	if (leidos) *leidos = 0;
	if (k < 0)
		return h == H_ENTRADA;	/* (sin consola: nada) */
	r = sc(SC_LEER, arch[k].m, (u32)b, n);
	if (ES_ERROR(r)) { pon_error(r); return 0; }
	if (leidos) *leidos = r;
	return 1;
}
int WINAPI WriteFile(HANDLE h, const void *b, u32 n, u32 *escritos, void *sol)
{
	int k = de_handle(h);
	u32 r;
	(void)sol;
	if (escritos) *escritos = 0;
	if (k < 0) {
		if (h == H_SALIDA || h == H_ERRORES) {		/* (al puerto serie) */
			sc(SC_SERIE, (u32)b, n, 0);
			if (escritos) *escritos = n;
			return 1;
		}
		ultimo_error = 6;
		return 0;
	}
	r = sc(SC_ESCRIBIR_ARCHIVO, arch[k].m, (u32)b, n);
	if (ES_ERROR(r)) { pon_error(r); return 0; }
	if (escritos) *escritos = r;
	return r == n || (pon_error(E_DISCO_LLENO), 0);
}
u32 WINAPI SetFilePointer(HANDLE h, int d, int *alto, u32 desde)
{
	int k = de_handle(h);
	u32 r;
	if (alto) *alto = 0;
	if (k < 0) { ultimo_error = 6; return 0xFFFFFFFFu; }
	r = sc(SC_MOVER, arch[k].m, (u32)d, desde);
	if (ES_ERROR(r)) { pon_error(r); return 0xFFFFFFFFu; }
	return r;
}
u32 WINAPI GetFileSize(HANDLE h, u32 *alto)
{
	int k = de_handle(h);
	if (alto) *alto = 0;
	if (k < 0) return 0xFFFFFFFFu;
	return sc(SC_LARGO, arch[k].m, 0, 0);
}
int WINAPI SetEndOfFile(HANDLE h)
{
	int k = de_handle(h);
	return k >= 0 && !ES_ERROR(sc(SC_CORTAR, arch[k].m, 0, 0));
}
int WINAPI FlushFileBuffers(HANDLE h) { (void)h; return 1; }
static int hecho(u32 r)
{
	if (ES_ERROR(r)) { pon_error(r); return 0; }
	return 1;
}
static int con_nombre(u32 sc_n, const char *a, const char *b)
{
	char t[300], u[300];
	limpia(a, t, sizeof t);
	if (b) limpia(b, u, sizeof u);
	return hecho(sc(sc_n, (u32)t, b ? (u32)u : 0, 0));
}
int WINAPI DeleteFileA(const char *n) { return con_nombre(SC_BORRAR, n, NULL); }
int WINAPI MoveFileA(const char *a, const char *b) { return con_nombre(SC_RENOMBRAR, a, b); }
int WINAPI CreateDirectoryA(const char *n, void *s) { (void)s; return con_nombre(SC_HACER_CARPETA, n, NULL); }
int WINAPI RemoveDirectoryA(const char *n) { return con_nombre(SC_BORRAR_CARPETA, n, NULL); }
int WINAPI SetCurrentDirectoryA(const char *n)
{
	char t[300];
	limpia(n, t, sizeof t);
	if (t[0] && t[1] == ':') {
		sc(SC_UNIDAD, (u8)t[0], 0, 0);
		if (!t[2])
			return 1;
	}
	return hecho(sc(SC_CAMBIAR_CARPETA, (u32)t, 0, 0));
}
u32 WINAPI GetCurrentDirectoryA(u32 cabe, char *d)
{
	char t[300];
	u32 n = sc(SC_CARPETA, (u32)(t + 2), sizeof t - 3, 0);
	if (ES_ERROR(n)) n = 0;
	t[2 + n] = 0;
	t[0] = unidad_sistema();
	t[1] = ':';
	if (t[2] != '\\') {		/* (la raíz es «»; las demás, sin la barra: delante) */
		int j;
		for (j = (int)n; j >= 0; j--) t[3 + j] = t[2 + j];
		t[2] = '\\';
	}
	n = strlen(t);
	if (n + 1 > cabe) return n + 1;
	memcpy(d, t, n + 1);
	return n;
}
/* ¿es una carpeta? (abierta, se puede leer su primera entrada) */
static int es_carpeta(u32 m)
{
	struct entrada e;
	u32 r = sc(SC_ENTRADA, m, (u32)&e, 0);
	return !ES_ERROR(r);
}
u32 WINAPI GetFileAttributesA(const char *n)
{
	u32 m = abre(n, PARA_LEER), a;
	char t[300];
	limpia(n, t, sizeof t);
	if (ES_ERROR(m)) {
		/* «C:» o «C:\»: la raíz */
		if (t[0] && t[1] == ':' && (!t[2] || (t[2] == '\\' && !t[3]))) return 0x10;
		pon_error(m);
		return 0xFFFFFFFFu;
	}
	a = es_carpeta(m) ? 0x10 : 0x20;
	sc(SC_CERRAR, m, 0, 0);
	return a;
}
u32 WINAPI GetFullPathNameA(const char *n, u32 cabe, char *d, char **parte)
{
	char t[600];
	u32 k;
	limpia(n, t + 300, 300);
	if (t[300] && t[301] == ':') {
		memcpy(t, t + 300, strlen(t + 300) + 1);
	} else if (t[300] == '\\') {
		t[0] = unidad_sistema(); t[1] = ':';
		memcpy(t + 2, t + 300, strlen(t + 300) + 1);
	} else {
		k = GetCurrentDirectoryA(300, t);
		if (k && t[k - 1] != '\\') t[k++] = '\\';
		memcpy(t + k, t + 300, strlen(t + 300) + 1);
	}
	k = strlen(t);
	if (k + 1 > cabe) return k + 1;
	memcpy(d, t, k + 1);
	if (parte) {
		char *p = d + k;
		while (p > d && p[-1] != '\\') p--;
		*parte = p;
	}
	return k;
}

/* ---- buscar archivos: FindFirstFileA, FindNextFileA ---- */
typedef struct { u32 atrib; FT c, a, e; u32 tam_hi, tam_lo, r0, r1; char nombre[260]; char alt[14]; } FINDA;
#define N_BUSCA 4
static struct { u8 usado; u32 m; char mascara[64]; } busca[N_BUSCA];
static int min(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
static int casa(const char *m, const char *n)
{
	/* «*» y «?»; «*.*» vale para todo (también sin punto) */
	if (m[0] == '*' && m[1] == '.' && m[2] == '*' && !m[3]) return 1;
	for (; *m; m++, n++) {
		if (*m == '*') {
			while (m[1] == '*') m++;
			if (!m[1]) return 1;
			for (; *n; n++) if (casa(m + 1, n)) return 1;
			return casa(m + 1, n);
		}
		if (!*n) return *m == '.' && m[1] == '*' && !m[2];
		if (*m != '?' && min((u8)*m) != min((u8)*n)) return 0;
	}
	return !*n;
}
static int siguiente(int k, FINDA *f)
{
	struct entrada e;
	char largo[MAX_LARGO];
	for (;;) {
		u32 r = sc(SC_ENTRADA_LARGA, busca[k].m, (u32)&e, (u32)largo);
		u32 n;
		if (r != 1) return 0;
		if (!casa(busca[k].mascara, largo) && !casa(busca[k].mascara, e.nombre)) continue;
		memset(f, 0, sizeof *f);
		f->atrib = e.atributos ? e.atributos : 0x80;
		fat_a_ft(e.fecha, e.hora, &f->e);
		f->c = f->a = f->e;
		f->tam_lo = e.tamano;
		n = strlen(largo);
		if (n > 259) n = 259;
		memcpy(f->nombre, largo, n);
		f->nombre[n] = 0;
		n = strlen(e.nombre);
		if (n > 13) n = 13;
		memcpy(f->alt, e.nombre, n);
		f->alt[n] = 0;
		return 1;
	}
}
HANDLE WINAPI FindFirstFileA(const char *patron, FINDA *f)
{
	char t[300];
	int k, j, barra = -1;
	u32 m;
	for (k = 0; k < N_BUSCA && busca[k].usado; k++) ;
	if (k == N_BUSCA) { ultimo_error = 4; return INVALIDO; }
	limpia(patron, t, sizeof t);
	for (j = 0; t[j]; j++) if (t[j] == '\\' || (j == 1 && t[j] == ':')) barra = j;
	if (strlen(t + barra + 1) >= sizeof busca[k].mascara) { ultimo_error = 123; return INVALIDO; }
	memcpy(busca[k].mascara, t + barra + 1, strlen(t + barra + 1) + 1);
	if (barra < 0) { t[0] = '.'; t[1] = 0; }
	else if (barra == 0 || (barra == 2 && t[1] == ':')) t[barra + 1] = 0;	/* la raíz: «\» */
	else if (barra == 1) { t[2] = '.'; t[3] = 0; }	/* «C:*.*»: la de trabajo */
	else t[barra] = 0;
	m = sc(SC_ABRIR, (u32)t, PARA_LEER, 0);
	if (ES_ERROR(m)) { pon_error(m); return INVALIDO; }
	busca[k].usado = 1;
	busca[k].m = m;
	if (!siguiente(k, f)) {
		sc(SC_CERRAR, m, 0, 0);
		busca[k].usado = 0;
		ultimo_error = 2;
		return INVALIDO;
	}
	return (HANDLE)(0x80 + k);
}
int WINAPI FindNextFileA(HANDLE h, FINDA *f)
{
	u32 k = (u32)h - 0x80;
	if (k >= N_BUSCA || !busca[k].usado) return 0;
	if (!siguiente(k, f)) { ultimo_error = 18; return 0; }
	return 1;
}
int WINAPI FindClose(HANDLE h)
{
	u32 k = (u32)h - 0x80;
	if (k >= N_BUSCA || !busca[k].usado) return 0;
	sc(SC_CERRAR, busca[k].m, 0, 0);
	busca[k].usado = 0;
	return 1;
}

/* lo de los procesos que pide MSVCRT (system, _spawn): aquí, nada */
int WINAPI GetExitCodeProcess(HANDLE h, u32 *c) { (void)h; if (c) *c = 0; return 0; }

/* MessageBoxA: el cuadro, en frontal.c */
int WINAPI MessageBoxA(HANDLE h, const char *t, const char *titulo, u32 tipo)
{
	(void)h;
	return frontal_aviso(t, titulo, tipo);
}

/* =============================================================================
 *  La red: WSOCK32 y HSPTLS como los usa w32/navega/http.c. Una conexión
 *  TCP (RED.BIN) se hace al conectar (connect), esperando como mucho 20 s;
 *  una cifrada (TLS.BIN: resuelve, conecta y hace el apretón de manos él),
 *  en TlsConectar: http.c dice antes (red_pentium_seguro) cuál va a ser.
 *  Lo que llega se pide sin quedarse esperando (un tic como mucho).
 * ============================================================================= */
int red_pentium_seguro;
#define N_CON 12
typedef struct {
	u8 usado, seguro, hecha, cerrada;
	u32 ip, puerto, con, error;
	char servidor[128];
	char texto[200];
} Con;
static Con cons[N_CON];
static u32 cab[8];
static u32 pide(u32 n, u32 orden, u32 a, u32 b, u32 c, void *datos, u32 largo, u32 cabe)
{
	cab[0] = orden; cab[1] = a; cab[2] = b; cab[3] = c;
	cab[4] = cab[5] = cab[6] = 0; cab[7] = largo;
	return sc(n, (u32)cab, (u32)datos, cabe);
}
/* si la red aún se pone en marcha (el enlace, la IP), esperar (hasta 10 s), como redes.cas */
static void espera_red(void)
{
	int i;
	for (i = 0; i < 100; i++) {
		u32 ip, e = sc(SC_ESTADO_RED, (u32)&ip, 0, 0);
		if (e != RED_BUSCANDO && e != RED_SIN_CABLE) return;
		if (e == RED_SIN_CABLE && i >= 60) return;
		sc(SC_DORMIR, pent_hz() / 10, 0, 0);
	}
}
static u32 pide_red(u32 orden, u32 a, u32 b, u32 c, void *datos, u32 largo, u32 cabe)
{
	u32 r = pide(SC_RED, orden, a, b, c, datos, largo, cabe);
	if (r == SIN_IP || r == SIN_CABLE) {
		espera_red();
		r = pide(SC_RED, orden, a, b, c, datos, largo, cabe);
	}
	return r;
}
const char *pent_texto_red(u32 e)
{
	switch (e) {
	case SIN_RED: return "No hay red: el servicio de red (RED.BIN) no est\xc3\xa1 en marcha.";
	case SIN_TARJETA: return "No hay red: no se ha encontrado una tarjeta de red que se sepa llevar.";
	case SIN_CABLE: return "No hay red: el cable no est\xc3\xa1 enchufado.";
	case SIN_IP: return "No hay red: todav\xc3\xad" "a no hay direcci\xc3\xb3n IP (DHCP).";
	case TIEMPO: return "El servidor no contesta.";
	case RECHAZADA: return "El servidor no contesta (conexi\xc3\xb3n rechazada).";
	case NO_RESUELVE: return "No se encuentra el servidor.";
	case CORTADA_RED: return "Se ha cortado la conexi\xc3\xb3n.";
	case INALCANZABLE: return "No se llega al servidor.";
	case SIN_TLS: return "Falta el servicio TLS (TLS.BIN): instale el paquete \xc2\xabtls\xc2\xbb.";
	}
	return "La red ha fallado.";
}
int WINAPI WSAStartup(u32 v, void *d) { (void)v; (void)d; return 0; }
int WINAPI WSACleanup(void) { return 0; }
static int wsa_error;
int WINAPI WSAGetLastError(void) { return wsa_error; }
u16 WINAPI htons(u16 x) { return (u16)(x << 8 | x >> 8); }
struct hostent { char *h_name; char **h_aliases; short h_addrtype, h_length; char **h_addr_list; };
static int a_ip(const char *t, u32 *ip)
{
	u32 v = 0, parte = 0;
	int k = 0, cifras = 0;
	for (;; t++) {
		if (*t >= '0' && *t <= '9') { parte = parte * 10 + (*t - '0'); if (parte > 255 || ++cifras > 3) return 0; }
		else if ((*t == '.' || !*t) && cifras) { v |= parte << (8 * k); k++; parte = 0; cifras = 0; if (!*t) break; if (k == 4) return 0; }
		else return 0;
	}
	if (k != 4) return 0;
	*ip = v;
	return 1;
}
struct hostent *WINAPI gethostbyname(const char *n)
{
	static struct hostent h;
	static u32 ip;
	static char *lista[2];
	u32 r;
	if (!a_ip(n, &ip)) {
		r = pide_red(O_RESOLVER, 0, 0, 0, (void *)n, strlen(n), 0);
		if (r != 0) {
			wsa_error = r == SIN_RED || r == SIN_TARJETA || r == SIN_CABLE || r == SIN_IP ? 10050 : 11001;
			return NULL;
		}
		ip = cab[1];
	}
	lista[0] = (char *)&ip;
	lista[1] = NULL;
	h.h_name = (char *)n;
	h.h_addrtype = 2;
	h.h_length = 4;
	h.h_addr_list = lista;
	return &h;
}
static Con *de_s(u32 s) { return s >= 1 && s <= N_CON && cons[s - 1].usado ? &cons[s - 1] : NULL; }
u32 WINAPI socket(int f, int t, int p)
{
	int k;
	(void)f; (void)t; (void)p;
	for (k = 0; k < N_CON && cons[k].usado; k++) ;
	if (k == N_CON) { wsa_error = 10024; return ~0u; }
	memset(&cons[k], 0, sizeof cons[k]);
	cons[k].usado = 1;
	return k + 1;
}
struct sockaddr_in { short sin_family; u16 sin_port; u32 sin_addr; char sin_zero[8]; };
int WINAPI connect(u32 s, const struct sockaddr_in *a, int n)
{
	Con *c = de_s(s);
	u32 r;
	(void)n;
	if (!c) { wsa_error = 10038; return -1; }
	c->ip = a->sin_addr;
	c->puerto = htons(a->sin_port);
	c->seguro = (u8)red_pentium_seguro;
	if (c->seguro) { wsa_error = 10035; return -1; }	/* (lo hace TlsConectar) */
	r = pide_red(O_CONECTAR, c->ip, c->puerto, 20 * pent_hz(), NULL, 0, 0);
	if (ES_ERROR_RED(r)) { c->error = r; wsa_error = r == RECHAZADA ? 10061 : 10060; return -1; }
	c->con = r;
	c->hecha = 1;
	wsa_error = 10035;
	return -1;
}
typedef struct { u32 fd_count; u32 fd_array[64]; } fd_set;
int WINAPI select(int n, fd_set *lee, fd_set *esc, fd_set *ex, const void *t)
{
	u32 k, j, total = 0;
	(void)n; (void)lee; (void)t;
	/* (http.c solo lo usa para saber si ya está conectado: lo está, o ha fallado) */
	if (esc) {
		for (k = j = 0; k < esc->fd_count; k++) {
			Con *c = de_s(esc->fd_array[k]);
			if (c && !c->error) esc->fd_array[j++] = esc->fd_array[k];
		}
		esc->fd_count = j;
		total += j;
	}
	if (ex) {
		for (k = j = 0; k < ex->fd_count; k++) {
			Con *c = de_s(ex->fd_array[k]);
			if (!c || c->error) ex->fd_array[j++] = ex->fd_array[k];
		}
		ex->fd_count = j;
		total += j;
	}
	return (int)total;
}
int WINAPI ioctlsocket(u32 s, u32 orden, u32 *v) { (void)s; (void)orden; (void)v; return 0; }
static u32 al_servicio(Con *c) { return c->seguro ? SC_TLS : SC_RED; }
static int envia(Con *c, const char *b, int n)
{
	int hecho = 0;
	while (hecho < n) {
		u32 trozo = (u32)(n - hecho) > MAX_DATOS ? MAX_DATOS : (u32)(n - hecho);
		u32 r = pide(al_servicio(c), O_ENVIAR, c->con, 0, 0, (void *)(b + hecho), trozo, 0);
		if (ES_ERROR_RED(r)) { c->error = r; return -1; }
		hecho += (int)r;
	}
	return hecho;
}
/* lo que haya llegado (esperando como mucho «espera» tics): n, 0 si ha cerrado, -2 si aún nada, -1 si error */
static int recibe(Con *c, char *b, int n, u32 espera)
{
	u32 r;
	if (c->cerrada) return 0;
	if (n > MAX_DATOS) n = MAX_DATOS;
	r = pide(al_servicio(c), O_RECIBIR, c->con, espera, 0, b, 0, (u32)n);
	if (r == TIEMPO) return -2;
	if (ES_ERROR_RED(r)) { c->error = r; return -1; }
	if (r == 0) c->cerrada = 1;
	return (int)r;
}
int WINAPI send(u32 s, const char *b, int n, int f)
{
	Con *c = de_s(s);
	(void)f;
	if (!c || !c->hecha) { wsa_error = 10057; return -1; }
	if (envia(c, b, n) < 0) { wsa_error = 10054; return -1; }
	return n;
}
int WINAPI recv(u32 s, char *b, int n, int f)
{
	Con *c = de_s(s);
	int r;
	(void)f;
	if (!c || !c->hecha) { wsa_error = 10057; return -1; }
	r = recibe(c, b, n, 1);
	if (r == -2) { wsa_error = 10035; return -1; }
	if (r < 0) { wsa_error = 10054; return -1; }
	return r;
}
int WINAPI closesocket(u32 s)
{
	Con *c = de_s(s);
	if (!c) return -1;
	if (c->hecha) pide(al_servicio(c), O_CERRAR, c->con, 0, 0, NULL, 0, 0);
	c->usado = 0;
	return 0;
}
/* ---- HSPTLS ---- */
typedef Con TLS;
TLS *WINAPI TlsAbrir(u32 s, const char *servidor)
{
	Con *c = de_s(s);
	u32 n;
	if (!c) return NULL;
	n = strlen(servidor);
	if (n > 127) n = 127;
	memcpy(c->servidor, servidor, n);
	c->servidor[n] = 0;
	c->seguro = 1;
	return c;
}
/* 1 hecha, -1 no se ha podido (TlsError y TlsTextoError dicen por qué) */
int WINAPI TlsConectar(TLS *c, int espera)
{
	u32 r, n;
	(void)espera;
	if (c->hecha) return 1;
	n = strlen(c->servidor);
	memcpy(c->texto, c->servidor, n + 1);
	r = pide(SC_TLS, O_CONECTAR, c->ip, c->puerto, 30 * pent_hz(), c->texto, n, sizeof c->texto - 1);
	if (r == SIN_IP || r == SIN_CABLE) {
		espera_red();
		memcpy(c->texto, c->servidor, n + 1);
		r = pide(SC_TLS, O_CONECTAR, c->ip, c->puerto, 30 * pent_hz(), c->texto, n, sizeof c->texto - 1);
	}
	if (ES_ERROR_RED(r)) {
		c->error = r;
		if ((r == TLS_CERTIFICADO || r == TLS_FALLO || r == TLS_SIN_HORA) && cab[7] > 0 && cab[7] < sizeof c->texto)
			c->texto[cab[7]] = 0;
		else {
			const char *t = pent_texto_red(r);
			n = strlen(t);
			memcpy(c->texto, t, n + 1);
		}
		return -1;
	}
	c->con = r;
	c->hecha = 1;
	return 1;
}
int WINAPI TlsError(TLS *c) { return c ? (int)c->error : 0; }
const char *WINAPI TlsTextoError(int e)
{
	/* (el texto de la última que falló: el de TLS.BIN, en UTF-8 o en la página 437) */
	static char t[220];
	int k;
	for (k = 0; k < N_CON; k++)
		if (cons[k].usado && cons[k].error == (u32)e && cons[k].texto[0]) {
			memcpy(t, cons[k].texto, sizeof cons[k].texto);
			return t;
		}
	return pent_texto_red((u32)e);
}
int WINAPI TlsLeer(TLS *c, void *b, int n, int espera)
{
	(void)espera;
	return recibe(c, b, n, 1);
}
int WINAPI TlsEscribir(TLS *c, const void *b, int n) { return envia(c, b, n); }
void WINAPI TlsCerrar(TLS *c)
{
	if (c && c->hecha) {
		pide(SC_TLS, O_CERRAR, c->con, 0, 0, NULL, 0, 0);
		c->hecha = 0;
	}
}
int WINAPI TlsInfo(TLS *c, char *d, int cabe)
{
	u32 r;
	if (!c || !c->hecha || cabe < 2) return 0;
	r = pide(SC_TLS, O_INFO, c->con, 0, 0, d, 0, (u32)cabe - 1);
	if (ES_ERROR_RED(r)) { d[0] = 0; return 0; }
	d[cab[7] < (u32)cabe ? cab[7] : (u32)cabe - 1] = 0;
	return 1;
}
