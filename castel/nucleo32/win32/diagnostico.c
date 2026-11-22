/* diagnostico.c - WIN32.BIN (JUEGOS): lo que le falta a un programa de
 * Windows, dicho claro. Cada función que un .EXE o una DLL importa y que
 * Hispa-Pentium aún no tiene se apunta, con su DLL («WINMM.DLL!mixerOpen»),
 * en el registro de WIN32 (\USUARIOS\<usuario>\WIN32.LOG de la unidad del
 * sistema, fuera de la zona protegida) y por el puerto serie; si el programa
 * llega a llamarla, un cuadro lo dice antes de acabar. Lo mismo con las DLL
 * que no se encuentran y con lo que se pide por GetProcAddress y no está.
 * Y la orden «WIN32 COMPRUEBA programa.exe» (o una carpeta entera: todos
 * sus .EXE y .DLL), que mira las importaciones de cada uno sin ejecutarlo.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "w32.h"
#include "k32.h"

char carpeta_usuario[128];      /* «C:\USUARIOS\CARLOS\» (en la 1252), o «» */
static char ruta_log[160];
static int log_listo;
BOOL WINAPI GetUserNameA(char *d, u32 *n);
typedef struct { u16 an, me, ds, di, ho, mi, se, ms; } HORA_W;
void WINAPI GetLocalTime(HORA_W *s);
int propia_tiene(const char *dll, const char *fn);   /* cargador.c */
void nombre_dll(const char *r, char *d, u32 cabe);   /* cargador.c */
u32 abrir_dll_desde(const char *carpeta, const char *nombre, char *ruta);
extern char letra_sistema;

/* la carpeta del usuario, hecha si no está (con la letra del sistema) */
void prepara_carpeta_usuario(void)
{
	char u[64], n[12], c[160], c4[160];
	u32 tam = sizeof u, k, j = 0;
	Entrada e;
	u[0] = 0;
	GetUserNameA(u, &tam);
	for (k = 0; u[k] && j < 8; k++) {
		int ch = mayus((u8)u[k]);
		if ((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_' || ch == '-')
			n[j++] = (char)ch;
	}
	n[j] = 0;
	if (!j)
		strcpy(n, "USUARIO");
	fmt(c, sizeof c, "%c:\\USUARIOS", letra_sistema ? letra_sistema : 'C');
	a437(c4, c, sizeof c4);
	if (!entrada_de(c, &e, 0))
		sys_crear_carpeta(c4);
	fmt(c, sizeof c, "%c:\\USUARIOS\\%s", letra_sistema ? letra_sistema : 'C', n);
	a437(c4, c, sizeof c4);
	if (!entrada_de(c, &e, 0))
		sys_crear_carpeta(c4);
	if (entrada_de(c, &e, 0) && (e.atributos & 0x10)) {
		fmt(carpeta_usuario, sizeof carpeta_usuario, "%s\\", c);
		fmt(ruta_log, sizeof ruta_log, "%sWIN32.LOG", carpeta_usuario);
	}
}

/* añade una línea al registro (con la fecha, la hora y el programa) */
void diag_apunta(const char *f, ...)
{
	char b[400], l[480], r4[160];
	va_list va;
	HORA_W h;
	u32 m;
	const char *prog = mod_exe ? mod_exe->nombre : "WIN32";
	va_start(va, f);
	formato_win(b, sizeof b, f, va);
	va_end(va);
	aviso_serie("%s", b);
	if (!ruta_log[0])
		return;
	GetLocalTime(&h);
	fmt(l, sizeof l, "%02u/%02u/%04u %02u:%02u:%02u %s: %s\r\n", h.di, h.me, h.an, h.ho, h.mi, h.se, prog, b);
	a437(r4, ruta_log, sizeof r4);
	m = sys_abrir(r4, 1);
	if (ES_ERROR(m))
		m = sys_abrir(r4, 2);
	if (ES_ERROR(m))
		return;
	sys_mover(m, 0, 2);
	{
		char l4[480];
		a437(l4, l, sizeof l4);
		/* (a437 cambia «/» por «\»: la fecha, tal cual) */
		memcpy(l4, l, 10);
		sys_escribir_archivo(m, l4, strlen(l4));
	}
	sys_cerrar(m);
	if (!log_listo)
		log_listo = 1;
}

const char *diag_ruta_log(void) { return ruta_log[0] ? ruta_log : "el puerto serie"; }

/* ---- lo que falta al cargar (lo apunta el cargador; aquí se resume) ---- */
#define MAX_VISTAS 64
static char vistas[MAX_VISTAS][72];
static int nvistas;
static int ya_vista(const char *t)
{
	int k;
	for (k = 0; k < nvistas; k++)
		if (!strcmp(vistas[k], t))
			return 1;
	if (nvistas < MAX_VISTAS)
		copia_n(vistas[nvistas++], t, sizeof vistas[0]);
	return 0;
}
void diag_falta_importada(const char *quien, const char *dll, const char *f)
{
	char t[100];
	fmt(t, sizeof t, "%s!%s", dll, f);
	if (ya_vista(t))
		return;
	diag_apunta("%s importa %s, que aún no hay (si la llama, se dirá)", quien, t);
}
void diag_falta_dll(const char *quien, const char *dll)
{
	char t[100];
	fmt(t, sizeof t, "DLL %s", dll);
	if (ya_vista(t))
		return;
	diag_apunta("no se encuentra %s, que necesita %s", dll, quien);
}
void diag_getprocaddress(const char *dll, const char *f)
{
	char t[100];
	fmt(t, sizeof t, "GPA %s!%s", dll, f);
	if (ya_vista(t))
		return;
	diag_apunta("GetProcAddress(%s, %s): no está (el programa sigue)", dll, f);
}

/* el módulo en el que cae una dirección: «HW.DLL+0x1234» */
void diag_donde(u32 dir, char *d, u32 cabe)
{
	Modulo *m;
	for (m = modulos; m; m = m->sig)
		if (!m->propias && dir >= m->base && dir < m->base + m->tam) {
			fmt(d, cabe, "%s+0x%x", m->nombre, dir - m->base);
			return;
		}
	if (dir < 0x400000)
		fmt(d, cabe, "WIN32.BIN+0x%x", dir);
	else
		fmt(d, cabe, "0x%08x", dir);
}

/* =========================================================================
 *  WIN32 COMPRUEBA programa.exe | carpeta ...
 * ========================================================================= */
static u32 total_faltan, total_mirados, total_dll_faltan;

static void di(const char *f, ...)
{
	char b[400], b4[400];
	va_list va;
	va_start(va, f);
	formato_win(b, sizeof b - 2, f, va);
	va_end(va);
	strcat(b, "\n");
	a437(b4, b, sizeof b4);
	{       /* (a437 cambia «/» por «\»; aquí no hace falta) */
		u32 k;
		for (k = 0; b[k]; k++)
			if (b[k] == '/')
				b4[k] = '/';
	}
	sys_escribir(b4, strlen(b4));
}

/* un PE leído entero en memoria, con sus secciones para pasar de RVA al archivo */
typedef struct { u8 *d; u32 n, pe, nsec, topc; } PeArch;
static int pe_leer(const char *ruta, PeArch *a)
{
	char c[200];
	u32 m, n;
	a437(c, ruta, sizeof c);
	m = sys_abrir(c, 0);
	if (ES_ERROR(m))
		return 0;
	n = sys_largo(m);
	if (ES_ERROR(n) || n < 0x200 || n > 0x4000000) {
		sys_cerrar(m);
		return -1;
	}
	a->d = m_pedir(n);
	if (!a->d) {
		sys_cerrar(m);
		return -1;
	}
	a->n = sys_leer(m, a->d, n);
	sys_cerrar(m);
	if (a->n != n || a->d[0] != 'M' || a->d[1] != 'Z')
		goto mal;
	a->pe = *(u32 *)(a->d + 0x3C);
	if (a->pe + 248 > n || memcmp(a->d + a->pe, "PE\0\0", 4) || *(u16 *)(a->d + a->pe + 4) != 0x14C)
		goto mal;
	a->nsec = *(u16 *)(a->d + a->pe + 6);
	a->topc = *(u16 *)(a->d + a->pe + 20);
	if (a->pe + 24 + a->topc + a->nsec * 40 > n)
		goto mal;
	return 1;
mal:
	m_soltar(a->d);
	a->d = 0;
	return -1;
}
static void *pe_rva(PeArch *a, u32 rva)
{
	u32 k;
	for (k = 0; k < a->nsec; k++) {
		u8 *s = a->d + a->pe + 24 + a->topc + k * 40;
		u32 va = *(u32 *)(s + 12), vt = *(u32 *)(s + 8), traw = *(u32 *)(s + 16), praw = *(u32 *)(s + 20);
		u32 t = vt > traw ? vt : traw;
		if (rva >= va && rva < va + t) {
			if (rva - va >= traw || praw + (rva - va) >= a->n)
				return 0;
			return a->d + praw + (rva - va);
		}
	}
	if (rva < a->n && rva < 0x1000)
		return a->d + rva;
	return 0;
}
static u32 pe_dir(PeArch *a, int i, u32 *tam)
{
	u8 *o = a->d + a->pe + 24;
	if (*(u16 *)o != 0x10B || 96 + i * 8 + 8 > a->topc)
		return 0;
	if (tam)
		*tam = *(u32 *)(o + 96 + i * 8 + 4);
	return *(u32 *)(o + 96 + i * 8);
}

/* las exportaciones de una DLL de fuera (por nombre y por número) */
#define MAX_DLLS 48
static struct { char nombre[32]; char ruta[160]; int hay; PeArch a; u32 ed, et; } dlls[MAX_DLLS];
static int ndlls;
static int dll_de_fuera(const char *dll, const char *carpeta)
{
	int k;
	char ruta[160];
	u32 m;
	for (k = 0; k < ndlls; k++)
		if (!strcmp(dlls[k].nombre, dll))
			return k;
	if (ndlls >= MAX_DLLS)
		return -1;
	k = ndlls++;
	memset(&dlls[k], 0, sizeof dlls[k]);
	copia_n(dlls[k].nombre, dll, sizeof dlls[k].nombre);
	m = abrir_dll_desde(carpeta, dll, ruta);
	if (ES_ERROR(m))
		return k;
	sys_cerrar(m);
	copia_n(dlls[k].ruta, ruta, sizeof dlls[k].ruta);
	if (pe_leer(ruta, &dlls[k].a) == 1) {
		dlls[k].hay = 1;
		dlls[k].ed = pe_dir(&dlls[k].a, 0, &dlls[k].et);
	} else
		dlls[k].hay = -1;
	return k;
}
static int exporta(int k, const char *fn, u32 ord)
{
	PeArch *a = &dlls[k].a;
	u32 *dir, *nom, j;
	if (dlls[k].hay != 1 || !dlls[k].ed)
		return 0;
	dir = pe_rva(a, dlls[k].ed);
	if (!dir)
		return 0;
	if (!fn)
		return ord >= dir[4] && ord - dir[4] < dir[5];
	nom = pe_rva(a, dir[8]);
	if (!nom)
		return 0;
	for (j = 0; j < dir[6]; j++) {
		const char *s = pe_rva(a, nom[j]);
		if (s && !strcmp(s, fn))
			return 1;
	}
	return 0;
}

static void comprueba_pe(const char *ruta, int solo_si_falta);
static char cola[32][160];
static int ncola;

/* mira un PE: sus importaciones, una por una */
static char mirados[64][160];
static int nmirados;
static const char *plural(u32 n, const char *uno, const char *varios) { return n == 1 ? uno : varios; }
static void comprueba_pe(const char *ruta, int callado)
{
	PeArch a;
	u32 id, nfun = 0, nfal = 0, ndll = 0;
	u8 *d;
	char carpeta[160], *p, base[40];
	int r, j;
	(void)callado;
	for (j = 0; j < nmirados; j++)
		if (!stricmp_(mirados[j], ruta))
			return;
	if (nmirados < 64)
		copia_n(mirados[nmirados++], ruta, sizeof mirados[0]);
	r = pe_leer(ruta, &a);
	{
		const char *b = strrchr(ruta, '\\');
		copia_n(base, b ? b + 1 : ruta, sizeof base);
	}
	if (r == 0) {
		di("%s: no se encuentra.", ruta);
		return;
	}
	if (r < 0) {
		di("%s: no es un programa de Windows de 32 bits (PE de 386).", base);
		return;
	}
	total_mirados++;
	copia_n(carpeta, ruta, sizeof carpeta);
	p = strrchr(carpeta, '\\');
	if (p)
		p[1] = 0;
	id = pe_dir(&a, 1, 0);
	d = id ? pe_rva(&a, id) : 0;
	{
		u16 sub = *(u16 *)(a.d + a.pe + 24 + 68), car = *(u16 *)(a.d + a.pe + 22);
		di("%s: %s%s", base, (car & 0x2000) ? "DLL" : sub == 2 ? "programa de ventanas" : sub == 3 ? "programa de consola" : "programa",
		   (car & 0x2000) ? "" : "");
	}
	for (; d && *(u32 *)(d + 12); d += 20) {
		const char *dn = pe_rva(&a, *(u32 *)(d + 12));
		u32 *ilt = pe_rva(&a, *(u32 *)d ? *(u32 *)d : *(u32 *)(d + 16));
		char dll[32];
		int prop, k = -1;
		if (!dn || !ilt)
			break;
		nombre_dll(dn, dll, sizeof dll);
		ndll++;
		prop = propia_tiene(dll, 0);
		if (!prop) {
			k = dll_de_fuera(dll, carpeta);
			if (k < 0 || dlls[k].hay != 1) {
				u32 n = 0;
				for (; *ilt; ilt++)
					n++;
				di("  FALTA LA DLL %s (%u %s)%s", dll, n, plural(n, "función", "funciones"), k >= 0 && dlls[k].hay < 0 ? ": está, pero no es de 32 bits" : "");
				diag_apunta("COMPRUEBA %s: falta la DLL %s", base, dll);
				nfal += n;
				nfun += n;
				total_dll_faltan++;
				continue;
			}
			/* (y la de fuera, que también se mire después) */
			{
				int j, ya = 0;
				for (j = 0; j < ncola; j++)
					if (!stricmp_(cola[j], dlls[k].ruta))
						ya = 1;
				if (!ya && ncola < 32)
					copia_n(cola[ncola++], dlls[k].ruta, sizeof cola[0]);
			}
		}
		for (; *ilt; ilt++) {
			const char *fn = 0;
			char num[12];
			u32 ord = 0;
			int bien;
			nfun++;
			if (*ilt & 0x80000000u) {
				ord = *ilt & 0xFFFF;
				fmt(num, sizeof num, "#%u", ord);
			} else {
				const char *s = pe_rva(&a, *ilt + 2);
				fn = s ? s : "?";
			}
			if (prop)
				bien = fn ? propia_tiene(dll, fn) > 0 || propia_tiene(0, fn) > 0 : 0;
			else
				bien = exporta(k, fn, ord);
			if (!bien) {
				nfal++;
				di("  falta %s!%s", dll, fn ? fn : num);
				diag_apunta("COMPRUEBA %s: falta %s!%s", base, dll, fn ? fn : num);
			}
		}
	}
	total_faltan += nfal;
	if (nfal)
		di("  -> %u DLL, %u %s: %s %u.", ndll, nfun, plural(nfun, "función", "funciones"), plural(nfal, "falta", "faltan"), nfal);
	else
		di("  -> %u DLL, %u %s: está todo.", ndll, nfun, plural(nfun, "función", "funciones"));
	m_soltar(a.d);
}

/* una carpeta: sus .EXE y .DLL (y las de dentro, hasta tres niveles) */
static void comprueba_carpeta(const char *c, int nivel)
{
	char c4[200], hijo[200];
	Entrada e;
	u32 m;
	char l[260];
	a437(c4, c, sizeof c4);
	m = sys_abrir(c4, 0);
	if (ES_ERROR(m))
		return;
	for (;;) {
		const char *n, *x;
		l[0] = 0;
		if (sys_entrada_larga(m, &e, l) != 1)
			break;
		if (e.atributos & 8)
			continue;
		{
			char t[260];
			a1252(t, l[0] ? l : e.nombre, sizeof t);
			fmt(hijo, sizeof hijo, "%s%s%s", c, c[strlen(c) - 1] == '\\' ? "" : "\\", t);
			n = t;
			if (!strcmp(n, ".") || !strcmp(n, ".."))
				continue;
			x = strrchr(n, '.');
			if (e.atributos & 0x10) {
				if (nivel < 3)
					comprueba_carpeta(hijo, nivel + 1);
			} else if (x && (!stricmp_(x, ".EXE") || !stricmp_(x, ".DLL")))
				comprueba_pe(hijo, 0);
		}
	}
	sys_cerrar(m);
}

/* la orden: «WIN32 COMPRUEBA uno.exe [otro.dll | carpeta]...». Da el código de salida */
int comprueba(const char *args)
{
	char una[200], r[200];
	const char *p = args;
	int k;
	di("WIN32 COMPRUEBA: las funciones de Windows que piden, y las que aún no hay");
	if (ruta_log[0])
		di("(lo que falta se apunta también en %s)", ruta_log);
	while (*p) {
		u32 k2 = 0;
		Entrada e;
		while (*p == ' ')
			p++;
		if (!*p)
			break;
		if (*p == '"') {
			p++;
			while (*p && *p != '"' && k2 + 1 < sizeof una)
				una[k2++] = *p++;
			if (*p == '"')
				p++;
		} else
			while (*p && *p != ' ' && k2 + 1 < sizeof una)
				una[k2++] = *p++;
		una[k2] = 0;
		ruta_entera(una, r, sizeof r);
		if (entrada_de(r, &e, 0) && (e.atributos & 0x10))
			comprueba_carpeta(r, 0);
		else
			comprueba_pe(r, 0);
	}
	/* las DLL de fuera que piden (de su carpeta o de \WIN32), si no se han mirado ya */
	for (k = 0; k < ncola; k++) {
		int j, ya = 0;
		for (j = 0; j < k; j++)
			if (!stricmp_(cola[j], cola[k]))
				ya = 1;
		if (!ya && strnicmp_(cola[k], carpeta_w32, strlen(carpeta_w32)))
			comprueba_pe(cola[k], 1);
	}
	if (!total_mirados) {
		di("No hay nada que mirar. Uso: WIN32 COMPRUEBA programa.exe (o una carpeta)");
		return 2;
	}
	if (total_faltan)
		di("En total: %u %s; %s %u %s%s.", total_mirados, plural(total_mirados, "archivo", "archivos"), plural(total_faltan, "falta", "faltan"),
		   total_faltan, plural(total_faltan, "función", "funciones"), total_dll_faltan ? " (con las de las DLL que no están)" : "");
	else
		di("En total: %u %s; no falta nada.", total_mirados, plural(total_mirados, "archivo", "archivos"));
	return 0;
}
