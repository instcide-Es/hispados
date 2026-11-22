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
 *  WIN32 COMPRUEBA programa.exe | carpeta ... [/CON carpeta] [/MAQUINA]
 *  (HL) Las DLL que importa cada archivo se buscan como al cargar (y como
 *  Windows): en la carpeta del programa principal (la del .EXE que arranca:
 *  el de la orden, o el de más arriba de la carpeta que se mira), en la de
 *  trabajo, en \WIN32 y en el PATH; no junto a la DLL que importa.
 * ========================================================================= */
static u32 total_faltan, total_mirados, total_dll_faltan;
static int maquina;            /* (HL) /MAQUINA: una línea por hecho, con tabuladores */
int propia_tiene_ord(const char *dll, u32 n);   /* cargador.c */

static void sale(const char *b)
{
	char b4[480];
	u32 k;
	a437(b4, b, sizeof b4);
	/* (a437 cambia «/» por «\»; aquí no hace falta) */
	for (k = 0; b[k]; k++)
		if (b[k] == '/')
			b4[k] = '/';
	sys_escribir(b4, strlen(b4));
}
static void di(const char *f, ...)
{
	char b[460];
	va_list va;
	if (maquina)
		return;
	va_start(va, f);
	formato_win(b, sizeof b - 2, f, va);
	va_end(va);
	strcat(b, "\n");
	sale(b);
}
/* (HL) /MAQUINA: «QUÉ<TAB>campo<TAB>campo...» */
static void dm(const char *f, ...)
{
	char b[460];
	va_list va;
	if (!maquina)
		return;
	va_start(va, f);
	formato_win(b, sizeof b - 2, f, va);
	va_end(va);
	strcat(b, "\n");
	sale(b);
}

/* ---- (HL) SHA-256 (FIPS 180-4), para /MAQUINA ---- */
static const u32 sha_k[64] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2 };
#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
static void sha_bloque(u32 h[8], const u8 *p)
{
	u32 w[64], a, b, c, d, e, f, g, hh, k, t1, t2;
	for (k = 0; k < 16; k++)
		w[k] = (u32)p[k * 4] << 24 | (u32)p[k * 4 + 1] << 16 | (u32)p[k * 4 + 2] << 8 | p[k * 4 + 3];
	for (k = 16; k < 64; k++) {
		u32 s0 = ROR(w[k - 15], 7) ^ ROR(w[k - 15], 18) ^ (w[k - 15] >> 3);
		u32 s1 = ROR(w[k - 2], 17) ^ ROR(w[k - 2], 19) ^ (w[k - 2] >> 10);
		w[k] = w[k - 16] + s0 + w[k - 7] + s1;
	}
	a = h[0]; b = h[1]; c = h[2]; d = h[3]; e = h[4]; f = h[5]; g = h[6]; hh = h[7];
	for (k = 0; k < 64; k++) {
		t1 = hh + (ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25)) + ((e & f) ^ (~e & g)) + sha_k[k] + w[k];
		t2 = (ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
		hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
	}
	h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}
static void sha256_texto(const u8 *d, u32 n, char *sal)
{
	u32 h[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
	u8 ult[128];
	u32 k, r, resto;
	for (k = 0; k + 64 <= n; k += 64)
		sha_bloque(h, d + k);
	resto = n - k;
	memset(ult, 0, sizeof ult);
	memcpy(ult, d + k, resto);
	ult[resto] = 0x80;
	r = resto < 56 ? 64 : 128;
	{
		u32 bits_alto = n >> 29, bits = n << 3;
		ult[r - 8] = (u8)(bits_alto >> 24); ult[r - 7] = (u8)(bits_alto >> 16); ult[r - 6] = (u8)(bits_alto >> 8); ult[r - 5] = (u8)bits_alto;
		ult[r - 4] = (u8)(bits >> 24); ult[r - 3] = (u8)(bits >> 16); ult[r - 2] = (u8)(bits >> 8); ult[r - 1] = (u8)bits;
	}
	sha_bloque(h, ult);
	if (r == 128)
		sha_bloque(h, ult + 64);
	for (k = 0; k < 8; k++)
		fmt(sal + k * 8, 9, "%08x", h[k]);
}

/* un archivo leído entero; y, si es un PE, sus secciones para pasar de RVA al archivo */
typedef struct { u8 *d; u32 n, pe, nsec, topc; } PeArch;
static int lee_entero(const char *ruta, PeArch *a)
{
	char c[200];
	u32 m, n;
	a->d = 0;
	a->n = 0;
	a437(c, ruta, sizeof c);
	m = sys_abrir(c, 0);
	if (ES_ERROR(m))
		return 0;
	n = sys_largo(m);
	if (ES_ERROR(n) || n > 0x4000000) {
		sys_cerrar(m);
		return -1;
	}
	a->d = m_pedir(n ? n : 1);
	if (!a->d) {
		sys_cerrar(m);
		return -1;
	}
	a->n = n ? sys_leer(m, a->d, n) : 0;
	sys_cerrar(m);
	if (a->n != n) {
		m_soltar(a->d);
		a->d = 0;
		return -1;
	}
	return 1;
}
static int pe_valida(PeArch *a)
{
	u32 n = a->n;
	if (n < 0x200 || a->d[0] != 'M' || a->d[1] != 'Z')
		return 0;
	a->pe = *(u32 *)(a->d + 0x3C);
	if (a->pe + 248 > n || memcmp(a->d + a->pe, "PE\0\0", 4) || *(u16 *)(a->d + a->pe + 4) != 0x14C)
		return 0;
	a->nsec = *(u16 *)(a->d + a->pe + 6);
	a->topc = *(u16 *)(a->d + a->pe + 20);
	if (a->pe + 24 + a->topc + a->nsec * 40 > n)
		return 0;
	return 1;
}
static int pe_leer(const char *ruta, PeArch *a)
{
	int r = lee_entero(ruta, a);
	if (r <= 0)
		return r;
	if (!pe_valida(a)) {
		m_soltar(a->d);
		a->d = 0;
		return -1;
	}
	return 1;
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

/* las exportaciones de una DLL de fuera (por nombre y por número); (HL) cada
 * una, con la carpeta del programa desde la que se ha buscado */
#define MAX_DLLS 64
#define APP_MAX 400        /* (HL) las carpetas del programa que pueden ser, separadas por «|» */
static struct { char nombre[32]; char desde[APP_MAX]; char ruta[160]; int hay; PeArch a; u32 ed, et; } dlls[MAX_DLLS];
static int ndlls;
static int dll_de_fuera(const char *dll, const char *carpeta)
{
	int k;
	char ruta[160];
	u32 m;
	for (k = 0; k < ndlls; k++)
		if (!strcmp(dlls[k].nombre, dll) && !stricmp_(dlls[k].desde, carpeta))
			return k;
	if (ndlls >= MAX_DLLS)
		return -1;
	k = ndlls++;
	memset(&dlls[k], 0, sizeof dlls[k]);
	copia_n(dlls[k].nombre, dll, sizeof dlls[k].nombre);
	copia_n(dlls[k].desde, carpeta, sizeof dlls[k].desde);
	{
		/* (HL) desde cada carpeta del programa que pueda ser (la de más cerca primero) */
		const char *p = carpeta;
		m = E_NO_HAY;
		do {
			char una[160];
			u32 j = 0;
			while (*p && *p != '|') {
				if (j + 1 < sizeof una)
					una[j++] = *p;
				p++;
			}
			una[j] = 0;
			if (*p == '|')
				p++;
			m = abrir_dll_desde(una, dll, ruta);
		} while (ES_ERROR(m) && *p);
	}
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
	if (!fn) {
		u32 *f = pe_rva(a, dir[7]);
		return f && ord >= dir[4] && ord - dir[4] < dir[5] && f[ord - dir[4]];
	}
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

/* lo que queda por mirar: las DLL de fuera que piden, con la carpeta del programa */
#define MAX_COLA 64
static struct { char ruta[160]; char app[APP_MAX]; } cola[MAX_COLA];
static int ncola;
static char mirados[128][160];
static int nmirados;
static const char *plural(u32 n, const char *uno, const char *varios) { return n == 1 ? uno : varios; }
static void carpeta_de(const char *ruta, char *d, u32 cabe)
{
	char *p;
	copia_n(d, ruta, cabe);
	p = strrchr(d, '\\');
	if (p)
		p[1] = 0;
	else
		d[0] = 0;
}

/* mira un PE: sus importaciones, una por una. «app»: la carpeta del programa
 * principal (para un .EXE, la suya; para una DLL, la del .EXE que la carga) */
static void comprueba_pe(const char *ruta, const char *app_dll)
{
	PeArch a;
	u32 id, nfun = 0, nfal = 0, ndll = 0;
	u8 *d;
	char carpeta[APP_MAX], base[40], sha[72];
	int r, j, es_dll;
	for (j = 0; j < nmirados; j++)
		if (!stricmp_(mirados[j], ruta))
			return;
	if (nmirados < 128)
		copia_n(mirados[nmirados++], ruta, sizeof mirados[0]);
	r = lee_entero(ruta, &a);
	{
		const char *b = strrchr(ruta, '\\');
		copia_n(base, b ? b + 1 : ruta, sizeof base);
	}
	if (r == 0) {
		di("%s: no se encuentra.", ruta);
		dm("NOHAY\t%s", ruta);
		return;
	}
	if (r > 0)
		sha256_texto(a.d, a.n, sha);
	if (r < 0 || !pe_valida(&a)) {
		di("%s: no es un programa de Windows de 32 bits (PE de 386).", base);
		dm("ARCHIVO\t%s\tno-pe\t%u\t%s", ruta, r > 0 ? a.n : 0, r > 0 ? sha : "-");
		if (a.d)
			m_soltar(a.d);
		return;
	}
	total_mirados++;
	{
		u16 sub = *(u16 *)(a.d + a.pe + 24 + 68), car = *(u16 *)(a.d + a.pe + 22);
		es_dll = (car & 0x2000) != 0;
		di("%s: %s", base, es_dll ? "DLL" : sub == 2 ? "programa de ventanas" : sub == 3 ? "programa de consola" : "programa");
		dm("ARCHIVO\t%s\t%s\t%u\t%s", ruta, es_dll ? "dll" : sub == 2 ? "ventanas" : sub == 3 ? "consola" : "otro", a.n, sha);
	}
	if (es_dll && app_dll && app_dll[0])
		copia_n(carpeta, app_dll, sizeof carpeta);
	else
		carpeta_de(ruta, carpeta, sizeof carpeta);
	id = pe_dir(&a, 1, 0);
	d = id ? pe_rva(&a, id) : 0;
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
				dm("FALTADLL\t%s\t%s\t%u", ruta, dll, n);
				diag_apunta("COMPRUEBA %s: falta la DLL %s", base, dll);
				nfal += n;
				nfun += n;
				total_dll_faltan++;
				continue;
			}
			dm("USA\t%s\t%s\t%s", ruta, dll, dlls[k].ruta);
			/* (y la de fuera, que también se mire después, con la misma carpeta del programa) */
			{
				int j2, ya = 0;
				for (j2 = 0; j2 < ncola; j2++)
					if (!stricmp_(cola[j2].ruta, dlls[k].ruta))
						ya = 1;
				if (!ya && ncola < MAX_COLA) {
					copia_n(cola[ncola].ruta, dlls[k].ruta, sizeof cola[0].ruta);
					copia_n(cola[ncola].app, carpeta, sizeof cola[0].app);
					ncola++;
				}
			}
		} else
			dm("USA\t%s\t%s\tWIN32.BIN", ruta, dll);
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
				bien = fn ? propia_tiene(dll, fn) > 0 || propia_tiene(0, fn) > 0 : propia_tiene_ord(dll, ord) > 0;
			else
				bien = exporta(k, fn, ord);
			if (!bien) {
				nfal++;
				di("  falta %s!%s", dll, fn ? fn : num);
				dm("FALTA\t%s\t%s\t%s", ruta, dll, fn ? fn : num);
				diag_apunta("COMPRUEBA %s: falta %s!%s", base, dll, fn ? fn : num);
			}
		}
	}
	total_faltan += nfal;
	if (nfal)
		di("  -> %u DLL, %u %s: %s %u.", ndll, nfun, plural(nfun, "función", "funciones"), plural(nfal, "falta", "faltan"), nfal);
	else
		di("  -> %u DLL, %u %s: está todo.", ndll, nfun, plural(nfun, "función", "funciones"));
	dm("RESUMEN\t%s\t%u\t%u\t%u", ruta, ndll, nfun, nfal);
	m_soltar(a.d);
}

/* (HL) la carpeta del programa principal de una carpeta: la del .EXE de más
 * arriba (la propia, si tiene alguno; si no, la primera de dentro que lo
 * tenga, nivel a nivel). «» si no hay ningún .EXE */
static int tiene_exe(const char *c, char *hija, u32 cabe, int primera_hija)
{
	char c4[200], l[260], t[260];
	Entrada e;
	u32 m;
	int hay = 0, n = 0;
	a437(c4, c, sizeof c4);
	m = sys_abrir(c4, 0);
	if (ES_ERROR(m))
		return 0;
	for (;;) {
		const char *x;
		l[0] = 0;
		if (sys_entrada_larga(m, &e, l) != 1)
			break;
		if (e.atributos & 8)
			continue;
		a1252(t, l[0] ? l : e.nombre, sizeof t);
		if (!strcmp(t, ".") || !strcmp(t, ".."))
			continue;
		if (e.atributos & 0x10) {
			if (primera_hija >= 0 && n++ == primera_hija)
				fmt(hija, cabe, "%s%s%s", c, c[strlen(c) - 1] == '\\' ? "" : "\\", t);
			continue;
		}
		x = strrchr(t, '.');
		if (x && !stricmp_(x, ".EXE"))
			hay = 1;
	}
	sys_cerrar(m);
	return hay;
}
static void carpeta_programa(const char *c, char *app, u32 cabe)
{
	/* a lo ancho, hasta tres niveles */
	static char nivel[2][16][200];
	int nn[2], k, j, cur = 0, h;
	app[0] = 0;
	copia_n(nivel[0][0], c, sizeof nivel[0][0]);
	nn[0] = 1;
	for (k = 0; k < 4; k++) {
		nn[cur ^ 1] = 0;
		for (j = 0; j < nn[cur]; j++) {
			char x[2];
			if (tiene_exe(nivel[cur][j], x, 0, -1)) {
				fmt(app, cabe, "%s%s", nivel[cur][j], nivel[cur][j][strlen(nivel[cur][j]) - 1] == '\\' ? "" : "\\");
				return;
			}
		}
		for (j = 0; j < nn[cur]; j++)
			for (h = 0; nn[cur ^ 1] < 16; h++) {
				char hija[200];
				hija[0] = 0;
				tiene_exe(nivel[cur][j], hija, sizeof hija, h);
				if (!hija[0])
					break;
				copia_n(nivel[cur ^ 1][nn[cur ^ 1]++], hija, sizeof nivel[0][0]);
			}
		cur ^= 1;
		if (!nn[cur])
			return;
	}
}

/* una carpeta: sus .EXE y .DLL (y las de dentro, hasta tres niveles). (HL)
 * Las DLL, desde la carpeta del programa: la de más cerca hacia arriba que
 * tenga un .EXE (un juego dentro de C:\JUEGOS es de su carpeta), o «app»;
 * con «fijo» (/CON), siempre «app» */
static void comprueba_carpeta(const char *c, int nivel, const char *app_arriba, int fijo)
{
	char c4[200], hijo[200], app[APP_MAX];
	Entrada e;
	u32 m;
	char l[260];
	copia_n(app, app_arriba, sizeof app);
	if (!fijo && tiene_exe(c, 0, 0, -1)) {
		/* esta y, detrás, las de arriba («C:\JUEGOS\HL\|C:\JUEGOS\») */
		char f[200];
		u32 lf;
		fmt(f, sizeof f, "%s%s", c, c[strlen(c) - 1] == '\\' ? "" : "\\");
		lf = strlen(f);
		if (strnicmp_(app_arriba, f, lf) || (app_arriba[lf] && app_arriba[lf] != '|'))
			fmt(app, sizeof app, "%s%s%s", f, app_arriba[0] ? "|" : "", app_arriba);
	}
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
					comprueba_carpeta(hijo, nivel + 1, app, fijo);
			} else if (x && (!stricmp_(x, ".EXE") || !stricmp_(x, ".DLL")))
				comprueba_pe(hijo, app);
		}
	}
	sys_cerrar(m);
}

/* la orden: «WIN32 COMPRUEBA uno.exe [otro.dll | carpeta]... [/CON carpeta]
 * [/MAQUINA]». Da el código de salida */
static const char *palabra(const char *p, char *una, u32 cabe)
{
	u32 k2 = 0;
	while (*p == ' ')
		p++;
	if (*p == '"') {
		p++;
		while (*p && *p != '"' && k2 + 1 < cabe)
			una[k2++] = *p++;
		if (*p == '"')
			p++;
	} else
		while (*p && *p != ' ' && k2 + 1 < cabe)
			una[k2++] = *p++;
	una[k2] = 0;
	return p;
}
int comprueba(const char *args)
{
	char una[200], r[200], app[160];
	static char cosas[16][200];
	int ncosas = 0, k, fijo = 0;
	const char *p = args;
	Entrada e;
	app[0] = 0;
	/* primero, las opciones (y la carpeta de /CON va con lo que se mira) */
	while (*p) {
		p = palabra(p, una, sizeof una);
		if (!una[0])
			break;
		if (!stricmp_(una, "/MAQUINA") || !stricmp_(una, "/MÁQUINA"))
			maquina = 1;
		else if (!stricmp_(una, "/CON")) {
			fijo = 1;
			p = palabra(p, una, sizeof una);
			if (una[0] && ncosas < 16)
				copia_n(cosas[ncosas++], una, sizeof cosas[0]);
		} else if (ncosas < 16)
			copia_n(cosas[ncosas++], una, sizeof cosas[0]);
	}
	di("WIN32 COMPRUEBA: las funciones de Windows que piden, y las que aún no hay");
	if (ruta_log[0])
		di("(lo que falta se apunta también en %s)", ruta_log);
	/* la carpeta del programa principal: la del primer .EXE que se nombra; si no,
	 * la del .EXE de más arriba de la primera carpeta */
	for (k = 0; k < ncosas && !app[0]; k++) {
		const char *x = strrchr(cosas[k], '.');
		ruta_entera(cosas[k], r, sizeof r);
		if (x && !stricmp_(x, ".EXE") && !(entrada_de(r, &e, 0) && (e.atributos & 0x10)))
			carpeta_de(r, app, sizeof app);
	}
	for (k = 0; k < ncosas && !app[0]; k++) {
		ruta_entera(cosas[k], r, sizeof r);
		if (entrada_de(r, &e, 0) && (e.atributos & 0x10))
			carpeta_programa(r, app, sizeof app);
	}
	if (app[0]) {
		di("(las DLL, desde la carpeta del programa: %s)", app);
		dm("PROGRAMA\t%s", app);
	}
	for (k = 0; k < ncosas; k++) {
		ruta_entera(cosas[k], r, sizeof r);
		if (entrada_de(r, &e, 0) && (e.atributos & 0x10))
			comprueba_carpeta(r, 0, app, fijo);
		else
			comprueba_pe(r, app);
	}
	/* las DLL de fuera que piden (de la carpeta del programa, de \WIN32...), si no se han mirado ya */
	for (k = 0; k < ncola; k++)
		if (strnicmp_(cola[k].ruta, carpeta_w32, strlen(carpeta_w32)))
			comprueba_pe(cola[k].ruta, cola[k].app);
	if (!total_mirados) {
		di("No hay nada que mirar. Uso: WIN32 COMPRUEBA programa.exe (o una carpeta) [/CON carpeta-del-juego] [/MAQUINA]");
		dm("TOTAL\t0\t0\t0");
		return 2;
	}
	if (total_faltan)
		di("En total: %u %s; %s %u %s%s.", total_mirados, plural(total_mirados, "archivo", "archivos"), plural(total_faltan, "falta", "faltan"),
		   total_faltan, plural(total_faltan, "función", "funciones"), total_dll_faltan ? " (con las de las DLL que no están)" : "");
	else
		di("En total: %u %s; está todo (no falta nada).", total_mirados, plural(total_mirados, "archivo", "archivos"));
	dm("TOTAL\t%u\t%u\t%u", total_mirados, total_faltan, total_dll_faltan);
	return total_faltan ? 1 : 0;
}
