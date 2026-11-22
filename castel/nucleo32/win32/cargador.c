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
extern const Exp exp_kernel32[], exp_user32[], exp_gdi32[], exp_comdlg32[], exp_advapi32[], exp_shell32[], exp_wsock32[], exp_version[], exp_winspool[], exp_wininet[];
extern const u32 n_kernel32, n_user32, n_gdi32, n_comdlg32, n_advapi32, n_shell32, n_wsock32, n_version, n_winspool, n_wininet;
extern const Ord ord_wsock32[], ord_ws2_32[];      /* (HL) red.c: los números de Winsock */
static const struct { const char *nombre; const Exp *t; const u32 *n; const Ord *o; } propias[] = {
	{ "KERNEL32.DLL", exp_kernel32, &n_kernel32, 0 },
	{ "USER32.DLL", exp_user32, &n_user32, 0 },
	{ "GDI32.DLL", exp_gdi32, &n_gdi32, 0 },
	{ "COMDLG32.DLL", exp_comdlg32, &n_comdlg32, 0 },
	{ "ADVAPI32.DLL", exp_advapi32, &n_advapi32, 0 },
	{ "SHELL32.DLL", exp_shell32, &n_shell32, 0 },
	{ "WSOCK32.DLL", exp_wsock32, &n_wsock32, ord_wsock32 },
	{ "WS2_32.DLL", exp_wsock32, &n_wsock32, ord_ws2_32 },
	{ "VERSION.DLL", exp_version, &n_version, 0 },
	{ "WINSPOOL.DRV", exp_winspool, &n_winspool, 0 },
	{ "WININET.DLL", exp_wininet, &n_wininet, 0 },       /* (HL) */
	{ "KERNELBASE.DLL", exp_kernel32, &n_kernel32, 0 },
	{ "NTDLL.DLL", exp_kernel32, &n_kernel32, 0 },
};

/* (HL) el nombre de la función número «n» de una DLL de dentro (0 si no lo tiene) */
static const char *nombre_de_ordinal(const Ord *o, u32 n)
{
	if (!o)
		return 0;
	for (; o->nombre; o++)
		if (o->n == n)
			return o->nombre;
	return 0;
}

/* ---- los que faltan ---- */
#define MAX_FALTAN 256
static struct { char dll[32]; char f[40]; } faltan[MAX_FALTAN];
static int nfaltan;
static u8 *trampolines;
void falta_funcion(const char *dll, const char *f, u32 desde) __attribute__((noreturn));
void diag_falta_importada(const char *quien, const char *dll, const char *f);   /* (JUEGOS) diagnostico.c */
void diag_falta_dll(const char *quien, const char *dll);

/* (JUEGOS) el trampolín de la que falta: con la dirección desde la que se ha llamado */
static void __attribute__((used, regparm(2))) falta_n(u32 i, u32 desde)
{
	falta_funcion(faltan[i].dll, faltan[i].f, desde);
}
__asm__(".text\n falta_comun:\n movl (%esp), %edx\n push %eax\n call falta_n\n");

static void *trampolin(const char *quien, const char *dll, const char *f)
{
	u8 *p;
	u32 i;
	extern u8 falta_comun[];
	if (nfaltan >= MAX_FALTAN) {
		diag_falta_importada(quien, dll, f);
		return (void *)falta_comun;
	}
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
	diag_falta_importada(quien, dll, f);       /* (JUEGOS) al registro */
	return p;
}

/* ---- con HISPA32=TRAZA2, cada llamada a las de dentro sale por el puerto
 * serie (TRAZA3: también las de las DLL de fuera) ---- */
static void __attribute__((used)) dice_llamada(const char *n, u32 *pila)
{
	/* (pila: lo de pushad; detrás, la vuelta y los argumentos) */
	aviso_serie("-> %s(%x, %x, %x, %x) desde %x", n, pila[9], pila[10], pila[11], pila[12], pila[8]);
}
static void *espia(const char *nombre, void *f)
{
	u8 *p = m_pedir(32);
	char *n = m_pedir(strlen(nombre) + 1);
	strcpy(n, nombre);
	p[0] = 0x60;                                   /* pushad */
	p[1] = 0x54;                                   /* push esp */
	p[2] = 0x68; *(u32 *)(p + 3) = (u32)n;         /* push nombre */
	p[7] = 0xE8; *(u32 *)(p + 8) = (u32)dice_llamada - (u32)(p + 12);
	p[12] = 0x83; p[13] = 0xC4; p[14] = 0x08;      /* add esp, 8 */
	p[15] = 0x61;                                  /* popad */
	p[16] = 0xE9; *(u32 *)(p + 17) = (u32)f - (u32)(p + 21);
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

void nombre_dll(const char *r, char *d, u32 cabe)
{
	nombre_base(r, d, cabe);
	alias(d);
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
		if (!nombre)        /* (HL) por número: WSOCK32 y WS2_32 */
			nombre = nombre_de_ordinal(m->ords, ordinal);
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

/* (JUEGOS) la DLL «nombre» buscada como al cargar, pero desde la carpeta
 * «carpeta» (la del programa; «» si no): para WIN32 COMPRUEBA.
 * (HL) Como Windows: un nombre sin carpeta se busca en la carpeta del .EXE
 * que arranca el programa (no en la de la DLL que lo importa: el
 * «valve\cl_dlls\client.dll» de un juego pide VGUI.DLL, que está junto a
 * HL.EXE), luego en la de trabajo, luego en la del sistema (\WIN32) y luego
 * en cada carpeta del PATH */
static u32 prueba_en(const char *sitio, const char *nombre, char *ruta)
{
	char c[160];
	u32 l = strlen(sitio);
	if (l && sitio[l - 1] != '\\' && sitio[l - 1] != ':')
		fmt(ruta, 128, "%s\\%s", sitio, nombre);
	else
		fmt(ruta, 128, "%s%s", sitio, nombre);
	a437(c, ruta, sizeof c);
	return sys_abrir(c, 0);
}
u32 WINAPI GetEnvironmentVariableA(const char *n, char *d, u32 cabe);
u32 abrir_dll_desde(const char *carpeta, const char *nombre, char *ruta)
{
	char c[160], camino[400];
	u32 m;
	const char *p;
	if (strchr(nombre, '\\') || strchr(nombre, '/') || strchr(nombre, ':')) {
		/* con su carpeta: tal cual (desde la de trabajo); (JUEGOS) y, si es
		 * relativa y no está ahí, desde la del programa («valve\dlls\hl.dll») */
		char e[160];
		ruta_entera(nombre, e, sizeof e);
		copia_n(ruta, e, 128);
		a437(c, ruta, sizeof c);
		m = sys_abrir(c, 0);
		if (!ES_ERROR(m) || strchr(nombre, ':') || nombre[0] == '\\' || nombre[0] == '/' || !carpeta[0])
			return m;
		fmt(ruta, 128, "%s%s", carpeta, nombre);
		a437(c, ruta, sizeof c);
		return sys_abrir(c, 0);
	}
	/* la carpeta del programa, la de trabajo, la del sistema y el PATH */
	if (carpeta[0] && !ES_ERROR(m = prueba_en(carpeta, nombre, ruta)))
		return m;
	{
		char t[160];
		ruta_entera(nombre, t, sizeof t);    /* (la de trabajo) */
		copia_n(ruta, t, 128);
		a437(c, ruta, sizeof c);
		m = sys_abrir(c, 0);
		if (!ES_ERROR(m))
			return m;
	}
	if (!ES_ERROR(m = prueba_en(carpeta_w32, nombre, ruta)))
		return m;
	camino[0] = 0;
	GetEnvironmentVariableA("PATH", camino, sizeof camino);
	for (p = camino; *p;) {
		char d[160];
		u32 k = 0;
		while (*p && *p != ';') {
			if (k + 1 < sizeof d)
				d[k++] = *p;
			p++;
		}
		d[k] = 0;
		if (*p == ';')
			p++;
		if (k && !ES_ERROR(m = prueba_en(d, nombre, ruta)))
			return m;
	}
	return E_NO_HAY;
}
/* (HL) la carpeta del .EXE que arranca el programa («C:\JUEGOS\HL\») */
void carpeta_del_exe(char *t, u32 cabe)
{
	char *p;
	copia_n(t, ruta_exe, cabe);
	p = strrchr(t, '\\');
	if (p)
		p[1] = 0;
	else
		t[0] = 0;
}
static u32 abrir_dll(const char *nombre, char *ruta)
{
	char t[160];
	carpeta_del_exe(t, sizeof t);
	return abrir_dll_desde(t, nombre, ruta);
}

/* (JUEGOS) ¿tiene la DLL de dentro «dll» la función «fn»? 1 sí, -1 no; 0 si
 * «dll» no es de las de dentro. Con fn = 0, solo si es de dentro (1); con
 * dll = 0, si la tiene alguna (como al importar) */
int propia_tiene(const char *dll, const char *fn)
{
	u32 k, q;
	for (k = 0; k < sizeof propias / sizeof propias[0]; k++)
		if (!dll || !strcmp(propias[k].nombre, dll)) {
			if (!fn)
				return 1;
			for (q = 0; q < *propias[k].n; q++)
				if (!strcmp(propias[k].t[q].nombre, fn))
					return 1;
			if (dll)
				return -1;
		}
	return dll ? 0 : -1;
}
/* (HL) ¿tiene la DLL de dentro «dll» la función número «n»? 1 sí, -1 no, 0 no es de dentro */
int propia_tiene_ord(const char *dll, u32 n)
{
	u32 k;
	const char *f;
	for (k = 0; k < sizeof propias / sizeof propias[0]; k++)
		if (!strcmp(propias[k].nombre, dll)) {
			f = nombre_de_ordinal(propias[k].o, n);
			return f && propia_tiene(dll, f) > 0 ? 1 : -1;
		}
	return 0;
}
/* (JUEGOS) el nombre de una DLL como la busca el cargador (con sus alias) */
static void alias(char *n);
void nombre_dll(const char *r, char *d, u32 cabe);

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
	} else    /* ((JUEGOS) con su carpeta si la lleva: «SUB\\JDLL.DLL») */
		f = abrir_dll(strchr(nombre, '\\') || strchr(nombre, '/') || strchr(nombre, ':') ? nombre : base_nombre, ruta);
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
		/* ((JUEGOS) sin .reloc pero sin la marca de «quitados», como
		 * Windows: se puede poner en otro sitio, no hay nada que cambiar) */
		if ((!rel && es_exe) || (*(u16 *)(cab + pe + 22) & 1)) {
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
			diag_falta_dll(m->nombre, dll);     /* (JUEGOS) */
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
				p = trampolin(m->nombre, o->nombre, fn);
			else if (traza_si >= 2 && (o->propias || traza_si >= 3))
				p = espia(fn, p);
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
			m->ords = propias[k].o;          /* (HL) */
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
