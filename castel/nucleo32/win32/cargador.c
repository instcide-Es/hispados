/* cargador.c - WIN32.BIN: el cargador de PE. Pone el .EXE donde dice su
 * cabecera (ImageBase: el segmento con base 0x40000000 deja que la 0x400000
 * sea suya) y las DLL donde quepan (reubicadas); resuelve las importaciones
 * con las DLL de verdad (MSVCRT.DLL, COMCTL32.DLL, DDRAW.DLL... de Hispa-486:
 * las mismas) o con las de dentro de WIN32.BIN (KERNEL32, USER32, GDI32,
 * COMDLG32, ADVAPI32, SHELL32), y llama a sus DllMain. Lo que falta tiene un
 * trampolín que, si se llama, lo dice y acaba el programa.
 * Hecho con la especificación pública del formato PE/COFF.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "w32.h"

Modulo *modulos, *mod_exe;
char ruta_exe[160], carpeta_w32[128];
u32 v_tam_zona(u32 base);

/* las DLL de dentro (cada pieza, su tabla) */
extern const Exp exp_kernel32[], exp_user32[], exp_gdi32[], exp_comdlg32[], exp_advapi32[], exp_shell32[], exp_wsock32[], exp_version[], exp_winspool[];
extern const u32 n_kernel32, n_user32, n_gdi32, n_comdlg32, n_advapi32, n_shell32, n_wsock32, n_version, n_winspool;
static const struct { const char *nombre; const Exp *t; const u32 *n; } propias[] = {
	{ "KERNEL32.DLL", exp_kernel32, &n_kernel32 },
	{ "USER32.DLL", exp_user32, &n_user32 },
	{ "GDI32.DLL", exp_gdi32, &n_gdi32 },
	{ "COMDLG32.DLL", exp_comdlg32, &n_comdlg32 },
	{ "ADVAPI32.DLL", exp_advapi32, &n_advapi32 },
	{ "SHELL32.DLL", exp_shell32, &n_shell32 },
	{ "WSOCK32.DLL", exp_wsock32, &n_wsock32 },
	{ "WS2_32.DLL", exp_wsock32, &n_wsock32 },
	{ "VERSION.DLL", exp_version, &n_version },
	{ "WINSPOOL.DRV", exp_winspool, &n_winspool },
	{ "KERNELBASE.DLL", exp_kernel32, &n_kernel32 },
	{ "NTDLL.DLL", exp_kernel32, &n_kernel32 },
};

/* ---- los que faltan ---- */
#define MAX_FALTAN 256
static struct { char dll[32]; char f[40]; } faltan[MAX_FALTAN];
static int nfaltan;
static u8 *trampolines;
void falta_funcion(const char *dll, const char *f) __attribute__((noreturn));

static void __attribute__((used, regparm(1))) falta_n(u32 i)
{
	falta_funcion(faltan[i].dll, faltan[i].f);
}
__asm__(".text\n falta_comun:\n push %eax\n call falta_n\n");

static void *trampolin(const char *dll, const char *f)
{
	u8 *p;
	u32 i;
	extern u8 falta_comun[];
	if (nfaltan >= MAX_FALTAN)
		return (void *)falta_comun;
	if (!trampolines)
		trampolines = m_pedir(MAX_FALTAN * 12);
	i = nfaltan++;
	copia_n(faltan[i].dll, dll, sizeof faltan[i].dll);
	copia_n(faltan[i].f, f, sizeof faltan[i].f);
	p = trampolines + i * 12;
	p[0] = 0xB8;                     /* mov eax, i */
	*(u32 *)(p + 1) = i;
	p[5] = 0xE9;                     /* jmp falta_comun */
	*(u32 *)(p + 6) = (u32)falta_comun - (u32)(p + 10);
	traza("falta %s!%s (trampolín)", dll, f);
	return p;
}

/* ---- los nombres ---- */
static void nombre_base(const char *r, char *d, u32 cabe)
{
	const char *b = r, *p;
	u32 k;
	for (p = r; *p; p++)
		if (*p == '\\' || *p == '/' || *p == ':')
			b = p + 1;
	for (k = 0; b[k] && k + 1 < cabe; k++)
		d[k] = mayus(b[k]);
	d[k] = 0;
	if (!strchr(d, '.') && k + 4 < cabe)
		strcat(d, ".DLL");
}
static void alias(char *n)
{
	if (!strncmp(n, "API-MS-WIN-CRT-", 15) || !strcmp(n, "VCRUNTIME140.DLL") || !strcmp(n, "UCRTBASE.DLL") || !strcmp(n, "VCRUNTIME140_1.DLL"))
		strcpy(n, "UCRT.DLL");
	else if (!strncmp(n, "API-MS-WIN-", 11))
		strcpy(n, "KERNEL32.DLL");
	else if (!strcmp(n, "MSVCRT40.DLL") || !strcmp(n, "MSVCRT20.DLL") || !strcmp(n, "CRTDLL.DLL"))
		strcpy(n, "MSVCRT.DLL");
	else if (!strcmp(n, "COMCTL32.DLL") || !strcmp(n, "MSVCRT.DLL"))
		;
}

Modulo *mod_por_nombre(const char *nombre)
{
	char n[32];
	Modulo *m;
	nombre_base(nombre, n, sizeof n);
	alias(n);
	for (m = modulos; m; m = m->sig)
		if (!strcmp(m->nombre, n))
			return m;
	return 0;
}
Modulo *mod_de(HANDLE h)
{
	Modulo *m;
	if (!h)
		return mod_exe;
	for (m = modulos; m; m = m->sig)
		if (m->base == (u32)h)
			return m;
	return 0;
}

/* ---- las exportaciones ---- */
static void *exp_de(Modulo *m, const char *nombre, u32 ordinal);
void *mod_funcion(Modulo *m, const char *nombre)
{
	if ((u32)nombre < 0x10000)
		return exp_de(m, 0, (u32)nombre);
	return exp_de(m, nombre, 0);
}
static void *exp_de(Modulo *m, const char *nombre, u32 ordinal)
{
	u32 *dir, *fun, *nom, ed, et, k, r = 0;
	u16 *ord;
	if (!m)
		return 0;
	if (m->propias) {
		if (!nombre)
			return 0;
		for (k = 0; k < m->nprop; k++)
			if (!strcmp(m->propias[k].nombre, nombre))
				return m->propias[k].f;
		return 0;
	}
	{
		u32 pe = m->base + *(u32 *)(m->base + 0x3C);
		ed = *(u32 *)(pe + 0x78);
		et = *(u32 *)(pe + 0x7C);
	}
	if (!ed)
		return 0;
	dir = (u32 *)(m->base + ed);
	fun = (u32 *)(m->base + dir[7]);
	nom = (u32 *)(m->base + dir[8]);
	ord = (u16 *)(m->base + dir[9]);
	if (nombre) {
		for (k = 0; k < dir[6]; k++)
			if (!strcmp((const char *)(m->base + nom[k]), nombre)) {
				r = fun[ord[k]];
				break;
			}
	} else if (ordinal >= dir[4] && ordinal - dir[4] < dir[5])
		r = fun[ordinal - dir[4]];
	if (!r)
		return 0;
	if (r >= ed && r < ed + et) {
		/* reenviada: «OTRA.Funcion» */
		char d[48], *p;
		Modulo *o;
		int f;
		copia_n(d, (const char *)(m->base + r), sizeof d);
		p = strchr(d, '.');
		if (!p)
			return 0;
		*p++ = 0;
		o = mod_cargar(d, &f);
		return o ? mod_funcion(o, p) : 0;
	}
	return (void *)(m->base + r);
}

/* ---- leer el archivo ---- */
static int leer_en(u32 m, u32 pos, void *d, u32 n)
{
	if (sys_mover(m, (i32)pos, 0) != pos)
		return 0;
	return sys_leer(m, d, n) == n;
}

static u32 abrir_dll(const char *nombre, char *ruta)
{
	char t[160], c[160];
	u32 m;
	int k;
	const char *sitios[4];
	if (strchr(nombre, '\\') || strchr(nombre, ':')) {
		copia_n(ruta, nombre, 128);
		a437(c, ruta, sizeof c);
		return sys_abrir(c, 0);
	}
	/* la carpeta del programa, la de Win32 y la de trabajo */
	copia_n(t, ruta_exe, sizeof t);
	{
		char *p = strrchr(t, '\\');
		if (p)
			p[1] = 0;
		else
			t[0] = 0;
	}
	sitios[0] = t;
	sitios[1] = carpeta_w32;
	sitios[2] = "";
	sitios[3] = 0;
	for (k = 0; sitios[k]; k++) {
		fmt(ruta, 128, "%s%s", sitios[k], nombre);
		a437(c, ruta, sizeof c);
		m = sys_abrir(c, 0);
		if (!ES_ERROR(m))
			return m;
	}
	return E_NO_HAY;
}

static int importar(Modulo *m, u32 *falta);

/* carga un PE (es_exe: el programa). Da el módulo o 0 (y en *falta, por qué:
 * 1 no está, 2 no es un PE de 386, 3 no hay memoria, 4 le falta una DLL) */
static Modulo *cargar_pe(const char *nombre, const char *base_nombre, int es_exe, int *falta)
{
	u8 cab[1024];
	char ruta[128];
	u32 f, pe, nsec, topc, ib, tam, tcab, base, k, delta;
	Modulo *mo;
	if (es_exe) {
		char c[160];
		copia_n(ruta, nombre, sizeof ruta);
		a437(c, ruta, sizeof c);
		f = sys_abrir(c, 0);
	} else
		f = abrir_dll(base_nombre, ruta);
	if (ES_ERROR(f)) {
		*falta = 1;
		return 0;
	}
	if (!leer_en(f, 0, cab, sizeof cab) || cab[0] != 'M' || cab[1] != 'Z')
		goto no_pe;
	pe = *(u32 *)(cab + 0x3C);
	if (pe > sizeof cab - 248 || memcmp(cab + pe, "PE\0\0", 4) || *(u16 *)(cab + pe + 4) != 0x14C || *(u16 *)(cab + pe + 24) != 0x10B)
		goto no_pe;
	nsec = *(u16 *)(cab + pe + 6);
	topc = *(u16 *)(cab + pe + 20);
	ib = *(u32 *)(cab + pe + 24 + 28);
	tam = *(u32 *)(cab + pe + 24 + 56);
	tcab = *(u32 *)(cab + pe + 24 + 60);
	if (pe + 24 + topc + nsec * 40 > sizeof cab)
		goto no_pe;
	base = v_reservar(ib, tam, 1);
	if (!base) {
		/* donde quepa, si se puede reubicar */
		u32 rel = *(u32 *)(cab + pe + 24 + 96 + 5 * 8);
		if (!rel || (*(u16 *)(cab + pe + 22) & 1)) {
			aviso_serie("%s no cabe en 0x%x y no se puede reubicar", ruta, ib);
			*falta = 3;
			sys_cerrar(f);
			return 0;
		}
		base = v_reservar(es_exe ? 0 : 0, tam, 1);
		if (!base) {
			*falta = 3;
			sys_cerrar(f);
			return 0;
		}
	}
	/* las cabeceras y las secciones */
	if (!tcab || tcab > tam)
		tcab = 0x400;
	if (!leer_en(f, 0, (void *)base, tcab < 4096 ? tcab : 4096))
		goto mal;
	for (k = 0; k < nsec; k++) {
		u8 *s = cab + pe + 24 + topc + k * 40;
		u32 va = *(u32 *)(s + 12), traw = *(u32 *)(s + 16), praw = *(u32 *)(s + 20), vt = *(u32 *)(s + 8);
		if (vt && traw > vt)
			traw = vt;
		if (va + traw > tam)
			traw = va < tam ? tam - va : 0;
		if (traw && !leer_en(f, praw, (void *)(base + va), traw))
			goto mal;
	}
	sys_cerrar(f);
	mo = m_cero(sizeof *mo);
	copia_n(mo->nombre, base_nombre, sizeof mo->nombre);
	copia_n(mo->ruta, ruta, sizeof mo->ruta);
	mo->base = base;
	mo->tam = tam;
	mo->es_exe = es_exe;
	mo->refs = 1;
	pe = base + *(u32 *)(base + 0x3C);
	mo->entrada = *(u32 *)(pe + 24 + 16) ? base + *(u32 *)(pe + 24 + 16) : 0;
	mo->rsrc = *(u32 *)(pe + 24 + 96 + 2 * 8) ? base + *(u32 *)(pe + 24 + 96 + 2 * 8) : 0;
	mo->rsrc_tam = *(u32 *)(pe + 24 + 96 + 2 * 8 + 4);
	/* los reubicados */
	delta = base - ib;
	if (delta) {
		u32 rel = *(u32 *)(pe + 24 + 96 + 5 * 8), rt = *(u32 *)(pe + 24 + 96 + 5 * 8 + 4), o = 0;
		*(u32 *)(pe + 24 + 28) = base;
		while (rel && o + 8 <= rt) {
			u32 pg = *(u32 *)(base + rel + o), n = *(u32 *)(base + rel + o + 4), j;
			if (n < 8)
				break;
			for (j = 8; j < n; j += 2) {
				u16 e = *(u16 *)(base + rel + o + j);
				if ((e >> 12) == 3)
					*(u32 *)(base + pg + (e & 0xFFF)) += delta;
			}
			o += n;
		}
	}
	mo->sig = modulos;
	modulos = mo;
	if (es_exe)
		mod_exe = mo;
	traza("cargado %s en 0x%x (%u octetos)", mo->ruta, base, tam);
	{
		u32 fl = 0;
		if (!importar(mo, &fl)) {
			*falta = 4;
			return 0;
		}
	}
	return mo;
no_pe:
	sys_cerrar(f);
	*falta = 2;
	return 0;
mal:
	sys_cerrar(f);
	v_soltar(base, 0);
	*falta = 2;
	return 0;
}

static int importar(Modulo *m, u32 *fl)
{
	u32 pe = m->base + *(u32 *)(m->base + 0x3C);
	u32 id = *(u32 *)(pe + 24 + 96 + 8), d;
	(void)fl;
	if (!id)
		return 1;
	for (d = m->base + id; *(u32 *)(d + 12); d += 20) {
		const char *dll = (const char *)(m->base + *(u32 *)(d + 12));
		u32 *ilt = (u32 *)(m->base + (*(u32 *)d ? *(u32 *)d : *(u32 *)(d + 16)));
		u32 *iat = (u32 *)(m->base + *(u32 *)(d + 16));
		int f = 0;
		Modulo *o = mod_cargar(dll, &f);
		if (!o) {
			char t[200];
			fmt(t, sizeof t, "No se encuentra %s, que necesita %s.", dll, m->nombre);
			aviso_serie("%s", t);
			extern void error_carga(const char *);
			error_carga(t);
			return 0;
		}
		for (; *ilt; ilt++, iat++) {
			void *p;
			const char *fn;
			char num[12];
			if (*ilt & 0x80000000u) {
				p = mod_funcion(o, (const char *)(*ilt & 0xFFFF));
				fmt(num, sizeof num, "#%u", *ilt & 0xFFFF);
				fn = num;
			} else {
				fn = (const char *)(m->base + *ilt + 2);
				p = mod_funcion(o, fn);
			}
			if (!p && o->propias) {
				/* (por si está en otra de las de dentro) */
				u32 j, q;
				for (j = 0; !p && j < sizeof propias / sizeof propias[0]; j++)
					for (q = 0; q < *propias[j].n; q++)
						if (!strcmp(propias[j].t[q].nombre, fn)) {
							p = propias[j].t[q].f;
							traza("%s está en %s, no en %s", fn, propias[j].nombre, o->nombre);
							break;
						}
			}
			if (!p)
				p = trampolin(o->nombre, fn);
			*iat = (u32)p;
		}
	}
	return 1;
}

static void dllmain(Modulo *m, u32 por_que)
{
	typedef BOOL (WINAPI *DM)(HANDLE, u32, void *);
	if (m->es_exe || m->propias || !m->entrada)
		return;
	traza("DllMain de %s (%u)", m->nombre, por_que);
	((DM)m->entrada)((HANDLE)m->base, por_que, 0);
}

/* carga la DLL «nombre» (o da la que ya está, con una cuenta más) */
Modulo *mod_cargar(const char *nombre, int *falta)
{
	char n[32];
	Modulo *m;
	u32 k;
	nombre_base(nombre, n, sizeof n);
	alias(n);
	for (m = modulos; m; m = m->sig)
		if (!strcmp(m->nombre, n)) {
			m->refs++;
			return m;
		}
	for (k = 0; k < sizeof propias / sizeof propias[0]; k++)
		if (!strcmp(propias[k].nombre, n)) {
			m = m_cero(sizeof *m);
			copia_n(m->nombre, n, sizeof m->nombre);
			fmt(m->ruta, sizeof m->ruta, "%s%s", carpeta_w32, n);
			m->propias = propias[k].t;
			m->nprop = *propias[k].n;
			/* una cabecera de mentira, para que tenga dirección (HMODULE) */
			m->base = (u32)m_cero(256);
			((u8 *)m->base)[0] = 'M';
			((u8 *)m->base)[1] = 'Z';
			m->refs = 1;
			m->sig = modulos;
			modulos = m;
			return m;
		}
	m = cargar_pe(nombre, n, 0, falta);
	if (m && !m->iniciada) {
		m->iniciada = 1;
		dllmain(m, 1);
	}
	return m;
}

/* el programa */
Modulo *cargar_exe(const char *ruta, int *falta)
{
	char n[32];
	nombre_base(ruta, n, sizeof n);
	return cargar_pe(ruta, n, 1, falta);
}

void mod_soltar(Modulo *m)
{
	Modulo **p;
	if (!m || m->es_exe || --m->refs > 0)
		return;
	if (!m->propias) {
		dllmain(m, 0);
		v_soltar(m->base, 0);
	}
	for (p = &modulos; *p; p = &(*p)->sig)
		if (*p == m) {
			*p = m->sig;
			break;
		}
	m_soltar(m);
}

void mods_acabar(void)
{
	Modulo *m;
	for (m = modulos; m; m = m->sig)
		if (!m->es_exe && !m->propias && m->iniciada) {
			m->iniciada = 0;
			dllmain(m, 0);
		}
}
