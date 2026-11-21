/* principal.c - WIN32.BIN: el arranque. El núcleo lo lanza con «U:\RUTA\
 * PROG.EXE resto» cuando se pide un programa que empieza por «MZ»
 * (usuario.cas). Prepara la TEB y la PEB (FS), el entorno y la línea de
 * órdenes, carga el .EXE (cargador.c) con sus DLL, le pone su pila y salta a
 * su entrada; si vuelve, acaba con lo que devuelva (ExitProcess).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "w32.h"
#include "k32.h"

extern char linea_ordenes[400];
void prepara_entorno(void);
void prepara_objs(void);
Modulo *cargar_exe(const char *ruta, int *falta);
void WINAPI ExitProcess(u32 c);
u32 WINAPI GetEnvironmentVariableA(const char *n, char *d, u32 cabe);
void (*al_salir_user)(void);
int es_ventanas;                 /* el programa es de ventanas (subsistema 2) */
void mensaje_fatal(const char *t);   /* user32.c: un cuadro (o la consola) */

typedef struct { u8 letra, clase, medio, sl; u32 total, libre, sist; char n[41], et[12]; u8 q, rel[2]; } INFO_UNIDAD;

/* la carpeta de las DLL: \WIN32 de la unidad del sistema, o su \HISPA\WIN32 */
static void busca_carpeta(void)
{
	u32 u;
	INFO_UNIDAD i;
	char l = 0, c[40];
	Entrada e;
	for (u = 0; u < 8; u++)
		if (sc(43, u, LIN(&i), 0) == 1 && i.sist == 1 && i.letra) {
			l = i.letra;
			break;
		}
	if (!l)
		l = 'C';
	fmt(c, sizeof c, "%c:\\WIN32", l);
	if (entrada_de(c, &e, 0) && (e.atributos & 0x10)) {
		fmt(carpeta_w32, sizeof carpeta_w32, "%c:\\WIN32\\", l);
		return;
	}
	fmt(c, sizeof c, "%c:\\HISPA\\WIN32", l);
	if (entrada_de(c, &e, 0) && (e.atributos & 0x10)) {
		fmt(carpeta_w32, sizeof carpeta_w32, "%c:\\HISPA\\WIN32\\", l);
		return;
	}
	/* sin carpeta: al lado del programa */
	copia_n(carpeta_w32, ruta_exe, sizeof carpeta_w32);
	{
		char *p = strrchr(carpeta_w32, '\\');
		if (p)
			p[1] = 0;
	}
}

void error_carga(const char *t)
{
	mensaje_fatal(t);
}

void falta_funcion(const char *dll, const char *f)
{
	char t[200];
	fmt(t, sizeof t, "Hispa-Pentium: falta la función %s (%s).", f, dll);
	aviso_serie("%s", t);
	mensaje_fatal(t);
	w32_salir(255);
}

static void prepara_teb(u32 pila_tope, u32 pila_fondo)
{
	u32 *teb = (u32 *)TEB_DIR, *peb = (u32 *)PEB_DIR;
	memset(teb, 0, 0x1000);
	memset(peb, 0, 0x1000);
	teb[0] = 0xFFFFFFFF;             /* la cadena de SEH, vacía */
	teb[1] = pila_tope;
	teb[2] = pila_fondo;
	teb[0x18 / 4] = TEB_DIR;
	teb[0x20 / 4] = sys_tarea();
	teb[0x24 / 4] = sys_tarea();
	teb[0x2C / 4] = TEB_DIR + 0xF80;   /* las TLS de las imágenes (ThreadLocalStoragePointer) */
	teb[0x30 / 4] = PEB_DIR;
	peb[8 / 4] = mod_exe ? mod_exe->base : 0x400000;
	peb[0x18 / 4] = 0x00150000;      /* el montón del proceso */
	peb[0xA4 / 4] = 4;               /* OSMajorVersion */
	peb[0xA8 / 4] = 10;
	peb[0xB0 / 4] = 1;               /* (Windows 9x) */
}

/* lo de TLS de la imagen (.tls): sus datos y sus funciones */
static void prepara_tls(Modulo *m)
{
	u32 pe = m->base + *(u32 *)(m->base + 0x3C), t = *(u32 *)(pe + 24 + 96 + 9 * 8), *d, n;
	u8 *bloque;
	if (!t)
		return;
	d = (u32 *)(m->base + t);
	n = d[1] - d[0];
	bloque = m_cero(n + d[4] + 16);
	memcpy(bloque, (void *)d[0], n);
	((u32 *)(TEB_DIR + 0xF80))[0] = (u32)bloque;
	if (d[2])
		*(u32 *)d[2] = 0;
	if (d[3]) {
		typedef void (WINAPI *TLSF)(void *, u32, void *);
		TLSF *f;
		for (f = (TLSF *)d[3]; *f; f++)
			(*f)((void *)m->base, 1, 0);
	}
}

static u32 entrada_exe;
static u32 __attribute__((used)) llama_entrada(void)
{
	typedef u32 (*ENT)(void);
	return ((ENT)entrada_exe)();
}

/* (entrada.asm) */
int w32_principal(void)
{
	char args[400], *p;
	u32 n, pila, tam_pila, pe;
	int falta = 0;
	__asm__ volatile("fninit");
	/* la TEB y la PEB, ya: FS apunta ahí */
	if (!v_reservar(TEB_DIR, 0x2000, 1)) {
		sys_serie("[w32] no hay memoria para la TEB\n");
		sys_salir(255);
	}
	prepara_teb(0, 0);
	prepara_objs();
	n = sys_argumentos(args, sizeof args);
	if (ES_ERROR(n) || !n || !args[0]) {
		static const char t[] = "WIN32: corre los programas de Windows de 32 bits (lo lanza el sistema solo).\n";
		sys_escribir(t, sizeof t - 1);
		sys_salir(1);
	}
	/* «U:\RUTA\PROG.EXE resto»: la ruta, en la 1252 */
	p = args;
	{
		char r[160];
		u32 k = 0;
		while (*p && *p != ' ' && k + 1 < sizeof r)
			r[k++] = *p++;
		r[k] = 0;
		a1252(ruta_exe, r, sizeof ruta_exe);
		if (*p == ' ')
			p++;
	}
	{
		char resto[300];
		a1252(resto, p, sizeof resto);
		if (strchr(ruta_exe, ' '))
			fmt(linea_ordenes, sizeof linea_ordenes, "\"%s\"%s%s", ruta_exe, resto[0] ? " " : "", resto);
		else
			fmt(linea_ordenes, sizeof linea_ordenes, "%s%s%s", ruta_exe, resto[0] ? " " : "", resto);
	}
	busca_carpeta();
	prepara_entorno();
	{
		char t[16];
		if (GetEnvironmentVariableA("HISPA32", t, sizeof t) && (t[0] == 'T' || t[0] == 't'))
			traza_si = 1;
	}
	traza("WIN32: %s (Win32 en %s)", linea_ordenes, carpeta_w32);
	if (!cargar_exe(ruta_exe, &falta)) {
		char t[200];
		if (falta == 4)
			sys_salir(255);
		fmt(t, sizeof t, falta == 1 ? "No se encuentra %s." : falta == 3 ? "No hay memoria para %s." : "%s no es un programa de Windows de 32 bits.", ruta_exe);
		mensaje_fatal(t);
		sys_salir(255);
	}
	pe = mod_exe->base + *(u32 *)(mod_exe->base + 0x3C);
	es_ventanas = *(u16 *)(pe + 24 + 68) == 2;
	tam_pila = *(u32 *)(pe + 24 + 72);
	if (tam_pila < 0x40000)
		tam_pila = 0x40000;
	if (tam_pila > 0x1000000)
		tam_pila = 0x1000000;
	pila = v_reservar(0, tam_pila, 1);
	if (!pila) {
		mensaje_fatal("No hay memoria para la pila del programa.");
		sys_salir(255);
	}
	{
		u32 *teb = (u32 *)TEB_DIR;
		teb[1] = pila + tam_pila;
		teb[2] = pila;
		((u32 *)PEB_DIR)[2] = mod_exe->base;
	}
	prepara_tls(mod_exe);
	entrada_exe = mod_exe->entrada;
	traza("a la entrada: %x (pila %x, %s)", entrada_exe, pila + tam_pila, es_ventanas ? "ventanas" : "consola");
	/* a su pila, y a la entrada */
	__asm__ volatile(
		"movl %0, %%esp\n"
		"call llama_entrada\n"
		"pushl %%eax\n"
		"call ExitProcess\n"
		: : "r"(pila + tam_pila - 16) : "memory");
	return 0;
}
