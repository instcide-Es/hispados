/* hilos.c - WIN32.BIN (JUEGOS): los hilos de verdad y lo que los sincroniza.
 *
 * Cada hilo de un programa de Windows es una tarea del núcleo en el mismo
 * espacio de direcciones (la llamada 183, win32.cas), con su pila y su TEB
 * (FS: el núcleo pone la base del descriptor 0x43 en cada cambio de tarea).
 * Esperar es la 184 (dormir mientras en una dirección haya un valor, como
 * un «futex») y despertar, la 185.
 *
 * WIN32.BIN no es de varios hilos por dentro (sus listas de ventanas, el
 * montón, los objetos...): lo guarda un cerrojo grande («gc», como el de
 * los núcleos de antes). Cada función que se exporta (KERNEL32, USER32...)
 * entra por un trampolín (hazexp.py, gc_puente) que lo toma y, al volver,
 * lo suelta; las que esperan (dormir, esperar a un objeto, leer una tecla,
 * un menú) lo sueltan entero mientras esperan. Si el programa sale de una
 * función de las nuestras sin volver (longjmp, un __except que salta por
 * encima), lo que quedó apuntado se suelta en la siguiente llamada (las
 * vueltas apuntadas por debajo de la pila de ahora ya no valen).
 *
 * Los cerrojos de los programas (CRITICAL_SECTION) no pasan por el cerrojo
 * grande: un entero con tres estados y la 184/185 (sin llamar al núcleo si
 * nadie espera). Eventos, mutex y semáforos: su estado, guardado por el
 * cerrojo grande, y un contador de cambios en el que esperan todos (sirve
 * igual para WaitForMultipleObjects con uno o con todos).
 *
 * Sin la 183 (un núcleo de antes), lo de siempre: un hilo corre entero al
 * crearlo (como HW32 de Hispa-486).
 * Hecho con la documentación pública de Win32 (sala limpia).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "w32.h"
#include "k32.h"
#define API(t) t WINAPI
#define TEB_HILO 0xFC8           /* (en la TEB) el Hilo del que corre */
#define TEB_VUELTAS 0xFC0        /* las vueltas apuntadas por gc_puente */
#define TEB_NVUELTAS 0xFC4
#define MAX_VUELTAS 500
#define WAIT_TIMEOUT 258
#define WAIT_ABANDONED 0x80
#define STILL_ACTIVE 259
#define WAIT_FAILED 0xFFFFFFFFu

int k_hilos;                     /* el núcleo da hilos (la 183) */
typedef u32 (WINAPI *HILO)(void *);

static inline u32 teb_lee(u32 o) { u32 v; __asm__ volatile("movl %%fs:(%1), %0" : "=r"(v) : "r"(o)); return v; }
static inline void teb_pon(u32 o, u32 v) { __asm__ volatile("movl %0, %%fs:(%1)" : : "r"(v), "r"(o) : "memory"); }
static inline u32 yo(void) { return teb_lee(0x24); }
static inline u32 cmpxchg(volatile u32 *p, u32 antes, u32 nuevo)
{
	u32 r;
	__asm__ volatile("lock cmpxchgl %2, %1" : "=a"(r), "+m"(*p) : "r"(nuevo), "0"(antes) : "memory");
	return r;
}
static inline u32 xchg(volatile u32 *p, u32 v)
{
	__asm__ volatile("xchgl %0, %1" : "+r"(v), "+m"(*p) : : "memory");
	return v;
}
static inline u32 xadd(volatile u32 *p, u32 v)
{
	__asm__ volatile("lock xaddl %0, %1" : "+r"(v), "+m"(*p) : : "memory");
	return v;
}

/* =========================================================================
 *  El cerrojo grande
 * ========================================================================= */
static volatile u32 gc_dueno, gc_esperan;
static u32 gc_veces;

static void gc_toma_n(u32 n)
{
	u32 y = yo();
	if (gc_dueno == y) {
		gc_veces += n;
		return;
	}
	for (;;) {
		u32 d = cmpxchg(&gc_dueno, 0, y);
		if (!d)
			break;
		xadd(&gc_esperan, 1);
		/* (cada segundo, por si su dueño ha acabado sin soltarlo: un hilo
		 * cortado con el cerrojo puesto) */
		if (sc(184, LIN(&gc_dueno), d, 1000) == 1 && gc_dueno == d && !sc(183, 6, d, 0))
			cmpxchg(&gc_dueno, d, 0);
		xadd(&gc_esperan, (u32)-1);
	}
	gc_veces = n;
}
void gc_toma(void) { gc_toma_n(1); }
void gc_suelta(void)
{
	if (gc_dueno != yo() || !gc_veces)
		return;
	if (--gc_veces)
		return;
	gc_dueno = 0;
	if (gc_esperan)
		sc(185, LIN(&gc_dueno), 1, 0);
}
/* lo suelta del todo (para esperar); da cuántas veces se tenía */
u32 gc_suelta_todo(void)
{
	u32 n;
	if (!k_hilos || gc_dueno != yo())
		return 0;
	n = gc_veces;
	gc_veces = 1;
	gc_suelta();
	return n;
}
void gc_retoma(u32 n)
{
	if (n)
		gc_toma_n(n);
}

/* la entrada de gc_puente: suelta lo de las vueltas que ya no están en la
 * pila (el programa ha saltado por encima: longjmp, SEH) y toma el
 * cerrojo. Da 1 si se apunta la vuelta (0: no cabe; se llama sin cerrojo) */
u32 __attribute__((used)) gc_entra(u32 esp_vuelta)
{
	u32 *v = (u32 *)teb_lee(TEB_VUELTAS), n = teb_lee(TEB_NVUELTAS);
	if (!v)
		return 0;
	while (n && v[2 * (n - 1) + 1] <= esp_vuelta) {
		n--;
		gc_suelta();
	}
	teb_pon(TEB_NVUELTAS, n);
	if (n >= MAX_VUELTAS)
		return 0;
	gc_toma();
	return 1;
}
__asm__(".text\n"
	".globl gc_puente\n"
	"gc_puente:\n"                 /* [esp] = la función, [esp+4] = la vuelta del que llama */
	"  pushl %eax\n  pushl %ecx\n  pushl %edx\n"
	"  leal 16(%esp), %eax\n"
	"  pushl %eax\n"
	"  call gc_entra\n"
	"  addl $4, %esp\n"
	"  testl %eax, %eax\n"
	"  jz 1f\n"
	"  movl %fs:0xFC0, %ecx\n"
	"  movl %fs:0xFC4, %edx\n"
	"  movl 16(%esp), %eax\n"
	"  movl %eax, (%ecx,%edx,8)\n"
	"  leal 16(%esp), %eax\n"
	"  movl %eax, 4(%ecx,%edx,8)\n"
	"  incl %edx\n"
	"  movl %edx, %fs:0xFC4\n"
	"  movl $gc_vuelta, 16(%esp)\n"
	"1:\n  popl %edx\n  popl %ecx\n  popl %eax\n"
	"  ret\n"
	"gc_vuelta:\n"
	"  pushl %eax\n  pushl %eax\n  pushl %ecx\n  pushl %edx\n"
	"  movl %fs:0xFC4, %edx\n"
	"  decl %edx\n"
	"  movl %edx, %fs:0xFC4\n"
	"  movl %fs:0xFC0, %ecx\n"
	"  movl (%ecx,%edx,8), %eax\n"
	"  movl %eax, 12(%esp)\n"
	"  call gc_suelta\n"
	"  popl %edx\n  popl %ecx\n  popl %eax\n"
	"  ret\n");

/* esperar en una dirección (soltando el cerrojo grande mientras tanto) */
u32 fx_espera(volatile void *a, u32 v, u32 ms)
{
	u32 n = gc_suelta_todo(), r;
	r = sc(184, LIN(a), v, ms);
	gc_retoma(n);
	return r;
}
void fx_despierta(volatile void *a, u32 n) { if (k_hilos) sc(185, LIN(a), n, 0); }

/* =========================================================================
 *  Los hilos
 * ========================================================================= */
typedef struct Hilo {
	u32 tid;                 /* su número (el del núcleo) */
	volatile u32 hecho;      /* ha acabado (se espera aquí) */
	u32 codigo;
	HILO f;
	void *arg;
	u32 pila, tam_pila, teb; /* lo suyo (se suelta cuando ya no corre) */
	int refs;                /* los HANDLE y él mismo, mientras corre */
	int prio;
	int dormido;             /* CREATE_SUSPENDED: aún no ha empezado */
	struct Hilo *sig;
} Hilo;
static Hilo principal_h, *hilos;
static volatile u32 cambios;     /* cada vez que un objeto puede dejar pasar a alguien */
static u32 tam_pila_exe = 0x100000;
#define TODOS 0x7FFFFFFF

static void avisa(void)
{
	cambios++;
	fx_despierta(&cambios, TODOS);
}

/* lo del hilo principal (principal.c, después de la TEB) */
void hilos_prepara(u32 teb, u32 tam_pila)
{
	u32 r;
	if (principal_h.refs) {          /* (la segunda vez: la pila de los hilos) */
		if (tam_pila)
			tam_pila_exe = tam_pila;
		return;
	}
	r = sc(183, 7, teb, 0);
	k_hilos = !ES_ERROR(r);
	if (k_hilos) {
		u32 t = sc(183, 1, 0, 0);
		((u32 *)teb)[0x24 / 4] = t;
	}
	if (tam_pila)
		tam_pila_exe = tam_pila;
	principal_h.tid = ((u32 *)teb)[0x24 / 4];
	principal_h.refs = 1000;
	principal_h.codigo = STILL_ACTIVE;
	((u32 *)teb)[TEB_HILO / 4] = (u32)&principal_h;
	/* las vueltas de gc_puente: una página aparte */
	((u32 *)teb)[TEB_VUELTAS / 4] = v_reservar(0, 0x1000, 1);
	((u32 *)teb)[TEB_NVUELTAS / 4] = 0;
	traza("hilos: %s (hilo principal %u)", k_hilos ? "de verdad" : "como en HW32", principal_h.tid);
}

static Hilo *hilo_actual(void) { return (Hilo *)teb_lee(TEB_HILO); }

/* suelta lo de los hilos que ya no corren y nadie mira */
static void recoge_hilos(void)
{
	Hilo **p = &hilos;
	while (*p) {
		Hilo *h = *p;
		if (h->hecho && h->refs <= 0 && !ES_ERROR(sc(183, 6, h->tid, 0)) && !sc(183, 6, h->tid, 0)) {
			*p = h->sig;
			if (h->pila)
				v_soltar(h->pila, 0);
			if (h->teb)
				v_soltar(h->teb, 0);
			m_soltar(h);
			continue;
		}
		p = &h->sig;
	}
}
static void hilo_suelta(Hilo *h)
{
	if (h && h != &principal_h && --h->refs <= 0)
		recoge_hilos();
}

/* las DLL, al empezar y al acabar un hilo (DLL_THREAD_ATTACH = 2, DETACH = 3) */
static void dll_hilos(u32 por_que)
{
	typedef BOOL (WINAPI *DM)(HANDLE, u32, void *);
	Modulo *m;
	for (m = modulos; m; m = m->sig)
		if (!m->es_exe && !m->propias && m->entrada && m->iniciada && !m->sin_hilos)
			((DM)m->entrada)((HANDLE)m->base, por_que, 0);
}
void tls_de_imagen(u32 teb, int llama);   /* principal.c */

static void __attribute__((noreturn)) hilo_acaba(u32 c)
{
	Hilo *h = hilo_actual();
	gc_toma();
	dll_hilos(3);
	tls_de_imagen(teb_lee(0x18), 3);
	if (h) {
		h->codigo = c;
		h->hecho = 1;
		h->refs--;
	}
	avisa();
	gc_suelta_todo();
	traza("hilo %u: acaba con %u", h ? h->tid : 0, c);
	for (;;)
		sc(183, 4, c, 0);
}
/* donde empieza cada hilo nuevo (con su pila y su TEB) */
static void __attribute__((used, noreturn)) hilo_empieza(Hilo *h)
{
	u32 c;
	__asm__ volatile("fninit");
	{
		u16 cw = 0x27F;          /* la palabra de control de Windows */
		__asm__ volatile("fldcw %0" : : "m"(cw));
	}
	teb_pon(0x24, sc(183, 1, 0, 0));
	gc_toma();
	tls_de_imagen(teb_lee(0x18), 2);
	dll_hilos(2);
	gc_suelta();
	c = h->f(h->arg);
	hilo_acaba(c);
}

static int arranca(Hilo *h)
{
	u32 teb, pila, *t, d[3], r, *s;
	pila = v_reservar(0, h->tam_pila, 1);
	teb = v_reservar(0, 0x2000, 1);
	if (!pila || !teb) {
		if (pila)
			v_soltar(pila, 0);
		if (teb)
			v_soltar(teb, 0);
		return 0;
	}
	h->pila = pila;
	h->teb = teb;
	t = (u32 *)teb;
	memset(t, 0, 0x2000);
	t[0] = 0xFFFFFFFF;                         /* la cadena de SEH, vacía */
	t[1] = pila + h->tam_pila;
	t[2] = pila;
	t[0x18 / 4] = teb;
	t[0x20 / 4] = sys_tarea();
	t[0x2C / 4] = teb + 0xF80;                 /* las TLS de la imagen */
	t[0x30 / 4] = PEB_DIR;
	t[TEB_HILO / 4] = (u32)h;
	t[TEB_VUELTAS / 4] = teb + 0x1000;
	t[TEB_NVUELTAS / 4] = 0;
	tls_de_imagen(teb, 0);
	/* su pila: «hilo_empieza(h)» con una vuelta de mentira */
	s = (u32 *)(pila + h->tam_pila - 16);
	s[0] = 0;
	s[1] = (u32)h;
	d[0] = (u32)hilo_empieza;
	d[1] = (u32)s;
	d[2] = teb;
	h->refs++;                                 /* (él, mientras corre) */
	r = sc(183, 0, LIN(d), 0);
	if (ES_ERROR(r)) {
		h->refs--;
		v_soltar(pila, 0);
		v_soltar(teb, 0);
		h->pila = h->teb = 0;
		return 0;
	}
	h->tid = r;
	h->dormido = 0;
	traza("CreateThread: hilo %u (pila %x, TEB %x)", r, pila, teb);
	return 1;
}

/* ---- como en HW32: el hilo corre entero al crearlo (sin la 183) ---- */
int hilo_n;
static void *hilo_vuelta[5];
static u32 hilo_codigo;
static void corre_entero(Hilo *h)
{
	void *guarda[5];
	u32 c;
	if (h->hecho || hilo_n >= 8)
		return;
	memcpy(guarda, hilo_vuelta, sizeof guarda);
	hilo_n++;
	h->dormido = 0;
	if (__builtin_setjmp(hilo_vuelta) == 0)
		c = h->f(h->arg);
	else
		c = hilo_codigo;
	hilo_n--;
	memcpy(hilo_vuelta, guarda, sizeof guarda);
	h->codigo = c;
	h->hecho = 1;
}

API(HANDLE) CreateThread(void *s, u32 pila, HILO f, void *arg, u32 fl, u32 *id)
{
	HANDLE hd;
	Hilo *h;
	(void)s;
	recoge_hilos();
	hd = nuevo_obj(O_HILO);
	if (!hd)
		return 0;
	h = m_cero(sizeof *h);
	if (!h) {
		obj(hd)->tipo = 0;
		pon_error(8);
		return 0;
	}
	h->f = f;
	h->arg = arg;
	h->codigo = STILL_ACTIVE;
	h->refs = 1;
	h->dormido = 1;
	h->tam_pila = pila ? pila : tam_pila_exe;
	if (h->tam_pila < 0x10000)
		h->tam_pila = 0x10000;
	if (h->tam_pila > 0x400000)
		h->tam_pila = 0x400000;
	h->tam_pila = (h->tam_pila + 0xFFF) & ~0xFFFu;
	obj(hd)->f = h;
	h->sig = hilos;
	hilos = h;
	traza("CreateThread(%x, %x)%s", (u32)f, (u32)arg, fl & 4 ? " (dormido)" : "");
	if (!k_hilos) {
		static u32 falso_tid = 0x10000;
		h->tid = sys_tarea() + (falso_tid += 0x10000);
		if (id)
			*id = h->tid;
		if (!(fl & 4))
			corre_entero(h);
		return hd;
	}
	if (!(fl & 4) && !arranca(h)) {
		obj(hd)->tipo = 0;
		h->refs = 0;
		h->hecho = 1;
		pon_error(8);
		return 0;
	}
	if (id)
		*id = h->tid;
	return hd;
}
static Hilo *hilo_de_handle(HANDLE hd)
{
	Obj *o;
	if (hd == (HANDLE)-2)
		return hilo_actual();
	o = obj(hd);
	return o && o->tipo == O_HILO ? (Hilo *)o->f : 0;
}
void hilo_cierra(Obj *o) { hilo_suelta((Hilo *)o->f); }   /* (CloseHandle) */
API(BOOL) DisableThreadLibraryCalls(HMODULE m)
{
	Modulo *mo = mod_de(m);
	if (mo)
		mo->sin_hilos = 1;
	return TRUE;
}

API(void) ExitThread(u32 c)
{
	Hilo *h = hilo_actual();
	if (!k_hilos) {
		if (!hilo_n)
			w32_salir(c);
		hilo_codigo = c;
		__builtin_longjmp(hilo_vuelta, 1);
	}
	if (!h || h == &principal_h)
		w32_salir(c);           /* el principal: se acaba el proceso */
	hilo_acaba(c);
}
API(u32) ResumeThread(HANDLE hd)
{
	Hilo *h = hilo_de_handle(hd);
	if (!h)
		return 0xFFFFFFFF;
	if (!h->dormido)
		return 0;
	if (!k_hilos)
		corre_entero(h);
	else
		arranca(h);
	return 1;
}
API(u32) SuspendThread(HANDLE h) { (void)h; return 0; }
API(BOOL) TerminateThread(HANDLE hd, u32 c)
{
	Hilo *h = hilo_de_handle(hd);
	if (!h)
		return FALSE;
	if (h == hilo_actual())
		ExitThread(c);
	if (k_hilos && !h->hecho && !h->dormido)
		sc(183, 5, h->tid, 0);
	if (!h->hecho) {
		h->codigo = c;
		h->hecho = 1;
		h->refs--;
	}
	avisa();
	return TRUE;
}
API(BOOL) GetExitCodeThread(HANDLE hd, u32 *c)
{
	Hilo *h = hilo_de_handle(hd);
	if (!h)
		return FALSE;
	*c = h->hecho ? h->codigo : STILL_ACTIVE;
	return TRUE;
}
/* las prioridades: se guardan (el núcleo reparte igual entre todas las
 * tareas); la más baja (THREAD_PRIORITY_IDLE) cede el procesador al dormir */
API(BOOL) SetThreadPriority(HANDLE hd, i32 p)
{
	Hilo *h = hilo_de_handle(hd);
	if (!h)
		return FALSE;
	h->prio = p;
	return TRUE;
}
API(i32) GetThreadPriority(HANDLE hd) { Hilo *h = hilo_de_handle(hd); return h ? h->prio : 0x7FFFFFFF; }
API(BOOL) SetThreadPriorityBoost(HANDLE h, BOOL b) { (void)h; (void)b; return TRUE; }
API(BOOL) SetPriorityClass(HANDLE h, u32 p) { (void)h; (void)p; return TRUE; }
API(u32) GetPriorityClass(HANDLE h) { (void)h; return 0x20; }
API(u32) SetThreadAffinityMask(HANDLE h, u32 m) { (void)h; (void)m; return 1; }
API(BOOL) SetProcessAffinityMask(HANDLE h, u32 m) { (void)h; (void)m; return TRUE; }
API(BOOL) GetProcessAffinityMask(HANDLE h, u32 *a, u32 *b) { (void)h; *a = *b = 1; return TRUE; }
API(u32) SetThreadIdealProcessor(HANDLE h, u32 p) { (void)h; (void)p; return 0; }
API(HANDLE) GetCurrentThread(void) { return (HANDLE)-2; }
API(u32) GetCurrentThreadId(void) { return yo(); }
API(BOOL) SwitchToThread(void)
{
	if (k_hilos) {
		u32 n = gc_suelta_todo();
		sc(183, 3, 0, 0);
		gc_retoma(n);
	} else
		sys_dormir(0);
	return TRUE;
}
API(void) Sleep(u32 ms)
{
	u32 fin = tics_ms() + ms;
	dx_tic();
	if (!ms) {
		SwitchToThread();
		return;
	}
	while ((i32)(fin - tics_ms()) > 0) {
		u32 q = (fin - tics_ms() + 9) / 10;
		sys_dormir(q > 2 ? 2 : (q ? q : 1));
		dx_tic();
	}
}
API(u32) SleepEx(u32 ms, BOOL a) { (void)a; Sleep(ms); return 0; }

/* =========================================================================
 *  Los objetos: eventos, mutex, semáforos (y los hilos y los procesos)
 * ========================================================================= */
/* los nombres (los objetos con nombre de este proceso) */
#define MAX_NOMBRES 32
static struct { char n[40]; HANDLE h; int tipo; } nombres[MAX_NOMBRES];
static HANDLE con_nombre(const char *n, int tipo, int *otro_tipo)
{
	int k;
	*otro_tipo = 0;
	if (!n || !n[0])
		return 0;
	for (k = 0; k < MAX_NOMBRES; k++)
		if (nombres[k].h && obj(nombres[k].h) && !stricmp_(nombres[k].n, n)) {
			if (nombres[k].tipo != tipo) {
				*otro_tipo = 1;
				return 0;
			}
			obj(nombres[k].h)->escribe++;     /* (una referencia más) */
			return nombres[k].h;
		}
	return 0;
}
static void pon_nombre(const char *n, HANDLE h, int tipo)
{
	int k;
	if (!n || !n[0])
		return;
	for (k = 0; k < MAX_NOMBRES; k++)
		if (!nombres[k].h || !obj(nombres[k].h)) {
			copia_n(nombres[k].n, n, sizeof nombres[k].n);
			nombres[k].h = h;
			nombres[k].tipo = tipo;
			return;
		}
}
/* CloseHandle de uno de estos: si tiene más referencias (abierto otra vez
 * por su nombre), sigue */
int sinc_cierra(Obj *o)
{
	if (o->escribe) {
		o->escribe--;
		return 0;
	}
	return 1;
}

static HANDLE nuevo_sinc(int tipo, const char *n)
{
	int otro;
	HANDLE h = con_nombre(n, tipo, &otro);
	if (otro) {
		pon_error(6);                   /* ERROR_INVALID_HANDLE */
		return 0;
	}
	if (h) {
		pon_error(183);                 /* ERROR_ALREADY_EXISTS */
		return h;
	}
	h = nuevo_obj(tipo);
	if (h) {
		pon_nombre(n, h, tipo);
		pon_error(0);
	}
	return h;
}

API(HANDLE) CreateEventA(void *s, BOOL manual, BOOL ini, const char *n)
{
	HANDLE h;
	(void)s;
	h = nuevo_sinc(O_EVENTO, n);
	if (h && GetLastError_() != 183) {
		obj(h)->a = ini ? 1 : 0;
		obj(h)->b = manual;
	}
	return h;
}
API(HANDLE) CreateEventW(void *s, BOOL manual, BOOL ini, const WCHAR *n)
{
	char b[64];
	if (n)
		de_w(b, n, sizeof b);
	return CreateEventA(s, manual, ini, n ? b : 0);
}
static HANDLE abre(int tipo, const char *n)
{
	int otro;
	HANDLE h = con_nombre(n, tipo, &otro);
	if (!h)
		pon_error(2);
	return h;
}
API(HANDLE) OpenEventA(u32 a, BOOL her, const char *n) { (void)a; (void)her; return abre(O_EVENTO, n); }
API(BOOL) SetEvent(HANDLE h)
{
	Obj *o = obj(h);
	if (!o || o->tipo != O_EVENTO)
		return FALSE;
	o->a = 1;
	avisa();
	return TRUE;
}
API(BOOL) ResetEvent(HANDLE h)
{
	Obj *o = obj(h);
	if (!o || o->tipo != O_EVENTO)
		return FALSE;
	o->a = 0;
	return TRUE;
}
/* PulseEvent: deja pasar a los que esperan ahora (con el número de pulsos,
 * que miran) y lo deja apagado */
API(BOOL) PulseEvent(HANDLE h)
{
	Obj *o = obj(h);
	if (!o || o->tipo != O_EVENTO)
		return FALSE;
	o->codigo++;
	o->a = 0;
	avisa();
	return TRUE;
}

API(HANDLE) CreateMutexA(void *s, BOOL dueno, const char *n)
{
	HANDLE h;
	(void)s;
	h = nuevo_sinc(O_MUTEX, n);
	if (h && GetLastError_() != 183 && dueno) {
		obj(h)->a = yo();
		obj(h)->b = 1;
	}
	return h;
}
API(HANDLE) CreateMutexW(void *s, BOOL d, const WCHAR *n)
{
	char b[64];
	if (n)
		de_w(b, n, sizeof b);
	return CreateMutexA(s, d, n ? b : 0);
}
API(HANDLE) OpenMutexA(u32 a, BOOL her, const char *n) { (void)a; (void)her; return abre(O_MUTEX, n); }
API(BOOL) ReleaseMutex(HANDLE h)
{
	Obj *o = obj(h);
	if (!o || o->tipo != O_MUTEX)
		return FALSE;
	if (o->a != yo() || !o->b) {
		pon_error(288);                 /* ERROR_NOT_OWNER */
		return FALSE;
	}
	if (!--o->b) {
		o->a = 0;
		avisa();
	}
	return TRUE;
}
API(HANDLE) CreateSemaphoreA(void *s, i32 ini, i32 max, const char *n)
{
	HANDLE h;
	(void)s;
	if (max <= 0 || ini < 0 || ini > max) {
		pon_error(87);
		return 0;
	}
	h = nuevo_sinc(O_SEMAFORO, n);
	if (h && GetLastError_() != 183) {
		obj(h)->a = ini;
		obj(h)->b = max;
	}
	return h;
}
API(HANDLE) CreateSemaphoreW(void *s, i32 ini, i32 max, const WCHAR *n)
{
	char b[64];
	if (n)
		de_w(b, n, sizeof b);
	return CreateSemaphoreA(s, ini, max, n ? b : 0);
}
API(HANDLE) OpenSemaphoreA(u32 a, BOOL her, const char *n) { (void)a; (void)her; return abre(O_SEMAFORO, n); }
API(BOOL) ReleaseSemaphore(HANDLE h, i32 n, i32 *antes)
{
	Obj *o = obj(h);
	if (!o || o->tipo != O_SEMAFORO || n <= 0)
		return FALSE;
	if (o->a + (u32)n > o->b) {
		pon_error(298);                 /* ERROR_TOO_MANY_POSTS */
		return FALSE;
	}
	if (antes)
		*antes = (i32)o->a;
	o->a += n;
	avisa();
	return TRUE;
}

/* ¿deja pasar el objeto? (y, si «tomar», lo toma: el evento automático se
 * apaga, el mutex pasa a ser mío, el semáforo baja uno). Da 0 sí, 1 no,
 * WAIT_ABANDONED, o WAIT_FAILED si no es un objeto que se pueda esperar */
static u32 deja(HANDLE hd, int tomar, u32 pulsos)
{
	Obj *o;
	Hilo *h;
	if (hd == (HANDLE)-1)
		return 1;                       /* (el proceso propio no acaba mientras espera) */
	h = hilo_de_handle(hd);
	if (h)
		return h->hecho ? 0 : 1;
	o = obj(hd);
	if (!o)
		return WAIT_FAILED;
	switch (o->tipo) {
	case O_EVENTO:
		if (o->a || o->codigo != pulsos) {
			if (tomar && !o->b)
				o->a = 0;
			return 0;
		}
		return 1;
	case O_MUTEX:
		if (o->a && o->a != yo() && k_hilos && !sc(183, 6, o->a, 0)) {
			/* su dueño ha acabado sin soltarlo */
			if (tomar) {
				o->a = yo();
				o->b = 1;
			}
			return WAIT_ABANDONED;
		}
		if (!o->a || o->a == yo()) {
			if (tomar) {
				o->a = yo();
				o->b++;
			}
			return 0;
		}
		return 1;
	case O_SEMAFORO:
		if (o->a) {
			if (tomar)
				o->a--;
			return 0;
		}
		return 1;
	case O_PROCESO:
		if (!o->hecho) {
			u32 r = sys_esperar(o->a, 1);
			if (r == 0xFFFFFF00u)
				return 1;
			o->codigo = r;
			o->hecho = 1;
		}
		return 0;
	case O_ARCHIVO: case O_CONSOLA_ENT: case O_CONSOLA_SAL: case O_CAMBIOS:
		return 0;
	}
	return WAIT_FAILED;
}

u32 espera_objetos(u32 n, const HANDLE *hs, BOOL todos, u32 ms)
{
	u32 fin = tics_ms() + ms, k, pulsos[64];
	if (!n || n > 64) {
		pon_error(87);
		return WAIT_FAILED;
	}
	for (k = 0; k < n; k++) {
		Obj *o = obj(hs[k]);
		pulsos[k] = o && o->tipo == O_EVENTO ? o->codigo : 0;
	}
	for (;;) {
		u32 vistos = cambios, r;
		if (todos) {
			u32 listos = 0, aband = 0;
			for (k = 0; k < n; k++) {
				r = deja(hs[k], 0, pulsos[k]);
				if (r == WAIT_FAILED) {
					pon_error(6);
					return WAIT_FAILED;
				}
				if (r != 1)
					listos++;
				if (r == WAIT_ABANDONED)
					aband = 1;
			}
			if (listos == n) {
				for (k = 0; k < n; k++)
					deja(hs[k], 1, pulsos[k]);
				return aband ? WAIT_ABANDONED : 0;
			}
		} else
			for (k = 0; k < n; k++) {
				r = deja(hs[k], 1, pulsos[k]);
				if (r == WAIT_FAILED) {
					pon_error(6);
					return WAIT_FAILED;
				}
				if (r != 1)
					return r + k;
			}
		if (!ms)
			return WAIT_TIMEOUT;
		{
			u32 espera = 0xFFFFFFFFu;
			int procesos = 0;
			for (k = 0; k < n; k++) {
				Obj *o = obj(hs[k]);
				if (o && o->tipo == O_PROCESO)
					procesos = 1;
			}
			if (ms != 0xFFFFFFFFu) {
				i32 q = (i32)(fin - tics_ms());
				if (q <= 0)
					return WAIT_TIMEOUT;
				espera = (u32)q;
			}
			if (procesos && espera > 50)
				espera = 50;    /* (de un proceso no llega aviso: se mira cada poco) */
			if (!k_hilos) {
				/* sin hilos de verdad: un hilo que no ha acabado, que corra */
				for (k = 0; k < n; k++) {
					Hilo *h = hilo_de_handle(hs[k]);
					if (h && !h->hecho && h->dormido == 0 && h != hilo_actual())
						corre_entero(h);
				}
				dx_tic();
				if (vistos == cambios)
					sys_dormir(1);
				continue;
			}
			dx_tic();
			fx_espera(&cambios, vistos, espera);
		}
	}
}
API(u32) WaitForSingleObject(HANDLE h, u32 ms) { return espera_objetos(1, &h, FALSE, ms); }
API(u32) WaitForSingleObjectEx(HANDLE h, u32 ms, BOOL a) { (void)a; return espera_objetos(1, &h, FALSE, ms); }
API(u32) WaitForMultipleObjects(u32 n, const HANDLE *h, BOOL todos, u32 ms) { return espera_objetos(n, h, todos, ms); }
API(u32) WaitForMultipleObjectsEx(u32 n, const HANDLE *h, BOOL todos, u32 ms, BOOL a) { (void)a; return espera_objetos(n, h, todos, ms); }
API(u32) SignalObjectAndWait(HANDLE s, HANDLE w, u32 ms, BOOL a)
{
	Obj *o = obj(s);
	(void)a;
	if (o && o->tipo == O_EVENTO)
		SetEvent(s);
	else if (o && o->tipo == O_MUTEX)
		ReleaseMutex(s);
	else if (o && o->tipo == O_SEMAFORO)
		ReleaseSemaphore(s, 1, 0);
	return espera_objetos(1, &w, FALSE, ms);
}
int hay_mensajes(void);   /* user32.c */
API(u32) MsgWaitForMultipleObjects(u32 n, const HANDLE *h, BOOL todos, u32 ms, u32 m)
{
	u32 fin = tics_ms() + ms;
	(void)m;
	for (;;) {
		if (n) {
			u32 r = espera_objetos(n, h, todos, 0);
			if (r != WAIT_TIMEOUT)
				return r;
		}
		if (hay_mensajes())
			return n;
		if (ms != 0xFFFFFFFFu && (i32)(fin - tics_ms()) <= 0)
			return WAIT_TIMEOUT;
		if (!ms)
			return WAIT_TIMEOUT;
		if (k_hilos && n) {
			u32 vistos = cambios;
			fx_espera(&cambios, vistos, 10);
		} else
			sys_dormir(1);
	}
}
API(u32) MsgWaitForMultipleObjectsEx(u32 n, const HANDLE *h, u32 ms, u32 m, u32 f) { return MsgWaitForMultipleObjects(n, h, (f & 1) != 0, ms, m); }

/* =========================================================================
 *  Los cerrojos de los programas (CRITICAL_SECTION) y lo atómico
 * ========================================================================= */
typedef struct { void *info; volatile u32 cuenta; i32 veces; u32 dueno; u32 sem, giros; } CS;
/* (cuenta: 0xFFFFFFFF libre, 0 tomado, 1 tomado y alguien espera) */
API(void) InitializeCriticalSection(void *c) { CS *s = c; memset(s, 0, sizeof *s); s->cuenta = 0xFFFFFFFF; }
API(BOOL) InitializeCriticalSectionAndSpinCount(void *c, u32 n) { InitializeCriticalSection(c); ((CS *)c)->giros = n; return TRUE; }
API(BOOL) InitializeCriticalSectionEx(void *c, u32 n, u32 f) { (void)f; return InitializeCriticalSectionAndSpinCount(c, n); }
API(u32) SetCriticalSectionSpinCount(void *c, u32 n) { u32 a = ((CS *)c)->giros; ((CS *)c)->giros = n; return a; }
API(void) EnterCriticalSection(void *c)
{
	CS *s = c;
	u32 y = yo(), v;
	if (s->dueno == y && s->veces > 0) {
		s->veces++;
		return;
	}
	v = cmpxchg(&s->cuenta, 0xFFFFFFFF, 0);
	if (v != 0xFFFFFFFF) {
		if (!k_hilos) {
			/* (sin hilos de verdad no puede estar tomado por otro: se toma) */
			s->cuenta = 0;
		} else {
			if (v != 1)
				v = xchg(&s->cuenta, 1);
			while (v != 0xFFFFFFFF) {
				fx_espera(&s->cuenta, 1, 0xFFFFFFFFu);
				v = xchg(&s->cuenta, 1);
			}
		}
	}
	s->dueno = y;
	s->veces = 1;
}
API(BOOL) TryEnterCriticalSection(void *c)
{
	CS *s = c;
	u32 y = yo();
	if (s->dueno == y && s->veces > 0) {
		s->veces++;
		return TRUE;
	}
	if (cmpxchg(&s->cuenta, 0xFFFFFFFF, 0) != 0xFFFFFFFF)
		return FALSE;
	s->dueno = y;
	s->veces = 1;
	return TRUE;
}
API(void) LeaveCriticalSection(void *c)
{
	CS *s = c;
	if (s->veces <= 0)
		return;
	if (--s->veces > 0)
		return;
	s->dueno = 0;
	if (xadd(&s->cuenta, (u32)-1) != 0) {
		s->cuenta = 0xFFFFFFFF;
		fx_despierta(&s->cuenta, 1);
	}
}
API(void) DeleteCriticalSection(void *c) { (void)c; }
API(i32) InterlockedIncrement(volatile i32 *p) { return (i32)xadd((volatile u32 *)p, 1) + 1; }
API(i32) InterlockedDecrement(volatile i32 *p) { return (i32)xadd((volatile u32 *)p, (u32)-1) - 1; }
API(i32) InterlockedExchange(volatile i32 *p, i32 v) { return (i32)xchg((volatile u32 *)p, (u32)v); }
API(i32) InterlockedExchangeAdd(volatile i32 *p, i32 v) { return (i32)xadd((volatile u32 *)p, (u32)v); }
API(i32) InterlockedCompareExchange(volatile i32 *p, i32 v, i32 c) { return (i32)cmpxchg((volatile u32 *)p, (u32)c, (u32)v); }
/* EXPORTA InterlockedCompareExchangePointer=InterlockedCompareExchange */
/* EXPORTA InterlockedExchangePointer=InterlockedExchange */

/* =========================================================================
 *  TLS (TlsAlloc: sus 64 casillas en la TEB de cada hilo)
 * ========================================================================= */
u32 tls_usados[2];
static void tls_limpia(u32 k)
{
	Hilo *h;
	((u32 *)teb_lee(0x18))[0xE10 / 4 + k] = 0;
	((u32 *)TEB_DIR)[0xE10 / 4 + k] = 0;
	for (h = hilos; h; h = h->sig)
		if (h->teb && !h->hecho)
			((u32 *)h->teb)[0xE10 / 4 + k] = 0;
}
API(u32) TlsAlloc(void)
{
	u32 k;
	for (k = 0; k < 64; k++)
		if (!(tls_usados[k / 32] & (1u << (k % 32)))) {
			tls_usados[k / 32] |= 1u << (k % 32);
			tls_limpia(k);
			return k;
		}
	return 0xFFFFFFFF;
}
API(BOOL) TlsFree(u32 k)
{
	if (k >= 64)
		return FALSE;
	tls_usados[k / 32] &= ~(1u << (k % 32));
	tls_limpia(k);
	return TRUE;
}
API(void *) TlsGetValue(u32 k) { teb_pon(0x34, 0); return k < 64 ? (void *)teb_lee(0xE10 + 4 * k) : 0; }
API(BOOL) TlsSetValue(u32 k, void *v) { if (k >= 64) return FALSE; teb_pon(0xE10 + 4 * k, (u32)v); return TRUE; }
API(u32) FlsAlloc(void *f) { (void)f; return TlsAlloc(); }
API(BOOL) FlsFree(u32 k) { return TlsFree(k); }
API(void *) FlsGetValue(u32 k) { return TlsGetValue(k); }
API(BOOL) FlsSetValue(u32 k, void *v) { return TlsSetValue(k, v); }
