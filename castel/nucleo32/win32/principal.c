/* principal.c - WIN32.BIN: el arranque. El núcleo lo lanza con «U:\RUTA\
 * PROG.EXE resto» cuando se pide un programa que empieza por «MZ»
 * (usuario.cas). Prepara la TEB y la PEB (FS), el entorno y la línea de
 * órdenes, carga el .EXE (cargador.c) con sus DLL, le pone su pila y salta a
 * su entrada; si vuelve, acaba con lo que devuelva (ExitProcess).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "w32.h"
#include "k32.h"
#include "../escudo/gancho_win32.h"	/* (ESCUDO) */

static char ruta_437[160];		/* (ESCUDO) la ruta tal cual llega (en la 437) */

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

char letra_sistema;               /* (JUEGOS) la de la unidad del sistema */
void prepara_carpeta_usuario(void);   /* (JUEGOS) diagnostico.c */
void diag_apunta(const char *f, ...);
void diag_donde(u32 dir, char *d, u32 cabe);
const char *diag_ruta_log(void);
int comprueba(const char *args);

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
	letra_sistema = l;
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
	/* la de la unidad del programa (el CD sin instalar: el sistema es M:, en
	   memoria, y \WIN32 está en el CD) */
	if (ruta_exe[0] && ruta_exe[1] == ':' && ruta_exe[0] != l) {
		fmt(c, sizeof c, "%c:\\WIN32", ruta_exe[0]);
		if (entrada_de(c, &e, 0) && (e.atributos & 0x10)) {
			fmt(carpeta_w32, sizeof carpeta_w32, "%c:\\WIN32\\", ruta_exe[0]);
			return;
		}
	}
	/* sin carpeta: al lado del programa */
	copia_n(carpeta_w32, ruta_exe, sizeof carpeta_w32);
	{
		char *p = strrchr(carpeta_w32, '\\');
		if (p)
			p[1] = 0;
	}
}

/* (HL) el aviso de lo que falta, con el botón «Comprobar compatibilidad» si
 * está el Asistente de compatibilidad (\COMPAT.BIN de la unidad del sistema,
 * o su \HISPA\COMPAT.BIN): lo lanza con «/FALLO <ruta del .EXE>» */
extern const char *msg_boton_extra;
i32 WINAPI MessageBoxA(HWND padre, const char *texto, const char *titulo, u32 tipo);
static void aviso_con_compat(const char *t)
{
	char c[64], c4[64];
	static const char *sitios[] = { "%c:\\COMPAT.BIN", "%c:\\HISPA\\COMPAT.BIN" };
	u32 k, m;
	c[0] = 0;
	for (k = 0; k < 2; k++) {
		fmt(c, sizeof c, sitios[k], letra_sistema ? letra_sistema : 'C');
		a437(c4, c, sizeof c4);
		m = sys_abrir(c4, 0);
		if (!ES_ERROR(m)) {
			sys_cerrar(m);
			break;
		}
		c[0] = 0;
	}
	if (!c[0] || !es_ventanas || sys_dato(13) != 1) {
		mensaje_fatal(t);
		if (c[0])
			aviso_serie("(para mirar qué le falta: %s /FALLO %s)", c, ruta_exe);
		return;
	}
	aviso_serie("%s", t);
	msg_boton_extra = "Comprobar compatibilidad";
	if (MessageBoxA(0, t, "Hispa-Pentium", 0x10) == 8) {
		char r4[200];
		a437(r4, ruta_exe, sizeof r4);
		traza("WIN32: lanzo %s /FALLO %s", c, ruta_exe);
		{
			char a[220];
			fmt(a, sizeof a, "/FALLO %s", r4);
			sys_lanzar(c4, a, 1);
		}
	}
}
void error_carga(const char *t)
{
	aviso_con_compat(t);
}

/* (JUEGOS) el programa llama a una función que aún no hay: se dice cuál
 * (DLL!función, y desde dónde), se apunta y se acaba */
void falta_funcion(const char *dll, const char *f, u32 desde)
{
	char t[400], d[48];
	diag_donde(desde, d, sizeof d);
	diag_apunta("LLAMA A UNA QUE FALTA: %s!%s (desde %s); se acaba", dll, f, d);
	fmt(t, sizeof t, "Falta la función %s!%s\n\n%s la ha llamado (desde %s) y Hispa-Pentium aún no la tiene: el programa tiene que acabar.\n\n"
	    "Queda apuntado en %s. Mándenos una foto de este aviso.", dll, f, mod_exe ? mod_exe->nombre : "El programa", d, diag_ruta_log());
	aviso_con_compat(t);
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

/* lo de TLS de la imagen (.tls, __declspec(thread)): sus datos, para el
 * hilo de la TEB «teb» (cada hilo, los suyos: (JUEGOS) hilos.c) y, con
 * «llama» (1 al empezar el proceso, 2 un hilo), sus funciones */
void tls_de_imagen(u32 teb, int llama)
{
	Modulo *m = mod_exe;
	u32 pe, t, *d, n;
	u8 *bloque;
	if (!m)
		return;
	pe = m->base + *(u32 *)(m->base + 0x3C);
	t = *(u32 *)(pe + 24 + 96 + 9 * 8);
	if (!t)
		return;
	d = (u32 *)(m->base + t);
	if (!llama || llama == 1) {
		n = d[1] - d[0];
		bloque = m_cero(n + d[4] + 16);
		if (!bloque)
			return;
		memcpy(bloque, (void *)d[0], n);
		((u32 *)(teb + 0xF80))[0] = (u32)bloque;
		if (d[2])
			*(u32 *)d[2] = 0;
	}
	if (llama && d[3]) {
		typedef void (WINAPI *TLSF)(void *, u32, void *);
		TLSF *f;
		for (f = (TLSF *)d[3]; *f; f++)
			(*f)((void *)m->base, (u32)llama, 0);
	}
}
static void prepara_tls(Modulo *m) { (void)m; tls_de_imagen(TEB_DIR, 1); }

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
	sc(183, 9, 0, 0);                   /* (JUEGOS) sin las primeras 64 K: el puntero nulo falla */
	/* la TEB y la PEB, ya: FS apunta ahí */
	if (!v_reservar(TEB_DIR, 0x2000, 1)) {
		sys_serie("[w32] no hay memoria para la TEB\n");
		sys_salir(255);
	}
	prepara_teb(0, 0);
	hilos_prepara(TEB_DIR, 0);          /* (JUEGOS) la TEB al núcleo (FS de cada hilo) y los hilos */
	{
		void seh_prepara(void);
		seh_prepara();              /* (JUEGOS) las excepciones del procesador, a seh.c */
	}
	prepara_objs();
	n = sys_argumentos(args, sizeof args);
	if (ES_ERROR(n) || !n || !args[0]) {
		static const char t[] = "WIN32: corre los programas de Windows de 32 bits (lo lanza el sistema solo).\n";
		sys_escribir(t, sizeof t - 1);
		sys_salir(1);
	}
	/* (JUEGOS) «WIN32 COMPRUEBA programa.exe...»: mirar sin ejecutar */
	if (!strnicmp_(args, "COMPRUEBA", 9) && (args[9] == ' ' || !args[9])) {
		char resto[400];
		busca_carpeta();
		prepara_entorno();
		prepara_carpeta_usuario();
		a1252(resto, args + 9, sizeof resto);
		sys_salir(comprueba(resto));
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
		memcpy(ruta_437, r, k + 1);	/* (ESCUDO) */
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
	prepara_carpeta_usuario();          /* (JUEGOS) \USUARIOS\<usuario>: el registro de WIN32 */
	{
		char t[16];
		if (GetEnvironmentVariableA("HISPA32", t, sizeof t) && (t[0] == 'T' || t[0] == 't'))
			traza_si = t[5] == '3' ? 3 : t[5] == '2' ? 2 : 1;
	}
	traza("WIN32: %s (Win32 en %s)", linea_ordenes, carpeta_w32);
	/* (ESCUDO) antes de cargarlo, que lo mire Escudo (si está y con su opción
	 * puesta): si encuentra algo y no se pide abrirlo igual, no se abre */
	if (escudo_antes(ruta_437)) {
		traza("WIN32: Escudo dice que %s no se abra", ruta_437);
		sys_salir(255);
	}
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
	hilos_prepara(TEB_DIR, *(u32 *)(pe + 24 + 72) ? *(u32 *)(pe + 24 + 72) : 0x100000);   /* (la pila de los hilos: la que pide el .EXE) */
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
