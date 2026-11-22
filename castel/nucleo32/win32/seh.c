/* seh.c - WIN32.BIN (JUEGOS): las excepciones estructuradas de Win32 (SEH).
 *
 * Cuando el procesador no puede seguir con un programa de Windows (dividir
 * por cero, una instrucción que no existe, un puntero que no vale), el
 * núcleo no lo acaba: lo manda a «despacha» (la 183, 2; win32.cas) con lo
 * que ha pasado en su pila. Desde ahí, lo de Windows: el EXCEPTION_RECORD y
 * el CONTEXT, la cadena de manejadores de FS:[0] (los de los __try/__except
 * de VC6: _except_handler3 de MSVCRT, o los suyos), RtlUnwind para los que
 * se saltan, el filtro de SetUnhandledExceptionFilter y, si nadie la quiere,
 * un aviso claro (qué ha pasado y dónde: «HW.DLL+0x1234») y se acaba.
 * RaiseException entra por el mismo sitio.
 * Hecho con la documentación pública de Win32 y del formato de los marcos
 * de SEH (sala limpia).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "w32.h"
#include "k32.h"
#define API(t) t WINAPI

typedef struct REC {
	u32 codigo, banderas;
	struct REC *otra;
	u32 dir, n;
	u32 info[15];
} REC;
typedef struct {
	u32 banderas, dr[6];
	u32 cw, sw, tw, eoff, esel, doff, dsel;
	u8 regs[80];
	u32 cr0npx;
	u32 gs, fs, es, ds, edi, esi, ebx, edx, ecx, eax, ebp, eip, cs, eflags, esp, ss;
	u8 ext[512];
} CTX;
typedef struct { REC *rec; CTX *ctx; } PUNTEROS;
typedef struct MARCO { struct MARCO *sig; void *manejador; } MARCO;
/* lo que deja el núcleo en la pila (win32.cas, a_su_despachador) */
typedef struct { u32 vector, error, cr2, eip, eflags, esp, eax, ecx, edx, ebx, ebp, esi, edi, cs, ss; } NUCLEO;

#define EXCEPTION_NONCONTINUABLE 1
#define EXCEPTION_UNWINDING 2
#define EXCEPTION_EXIT_UNWIND 4
#define CONTEXT_FULL 0x1001F

static inline u32 teb_lee(u32 o) { u32 v; __asm__ volatile("movl %%fs:(%1), %0" : "=r"(v) : "r"(o)); return v; }
static inline void teb_pon(u32 o, u32 v) { __asm__ volatile("movl %0, %%fs:(%1)" : : "r"(v), "r"(o) : "memory"); }

typedef i32 (WINAPI *FILTRO)(PUNTEROS *);
static FILTRO filtro;
API(FILTRO) SetUnhandledExceptionFilter(FILTRO f) { FILTRO a = filtro; filtro = f; return a; }

void diag_apunta(const char *f, ...);
void diag_donde(u32 dir, char *d, u32 cabe);
const char *diag_ruta_log(void);
void mensaje_fatal(const char *t);

static const char *nombre_de(u32 c)
{
	switch (c) {
	case 0xC0000005: return "EXCEPTION_ACCESS_VIOLATION (un puntero que no vale)";
	case 0xC0000094: return "EXCEPTION_INT_DIVIDE_BY_ZERO (dividir por cero)";
	case 0xC0000095: return "EXCEPTION_INT_OVERFLOW";
	case 0xC000001D: return "EXCEPTION_ILLEGAL_INSTRUCTION (una instrucción que no hay)";
	case 0xC0000096: return "EXCEPTION_PRIV_INSTRUCTION (una instrucción del sistema)";
	case 0xC00000FD: return "EXCEPTION_STACK_OVERFLOW";
	case 0xC000008C: return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
	case 0xC000008E: return "EXCEPTION_FLT_DIVIDE_BY_ZERO";
	case 0x80000003: return "EXCEPTION_BREAKPOINT (int 3)";
	case 0x80000004: return "EXCEPTION_SINGLE_STEP";
	case 0xC0000025: return "EXCEPTION_NONCONTINUABLE_EXCEPTION";
	case 0xE06D7363: return "una excepción de C++ (throw) que nadie recoge";
	}
	return "una excepción del programa (RaiseException)";
}

/* llama a un manejador (cdecl) guardando lo que guarda Windows: EBX, ESI,
 * EDI, EBP y la pila (un manejador de VC6 cambia EBP) */
u32 llama_manejador(void *f, REC *r, MARCO *m, CTX *c, void *dc);
__asm__(".text\n"
	"llama_manejador:\n"
	"  pushl %ebp\n  pushl %ebx\n  pushl %esi\n  pushl %edi\n"
	"  movl %esp, %ebp\n"
	"  pushl 36(%ebp)\n  pushl 32(%ebp)\n  pushl 28(%ebp)\n  pushl 24(%ebp)\n"
	"  call *20(%ebp)\n"
	"  movl %ebp, %esp\n"
	"  popl %edi\n  popl %esi\n  popl %ebx\n  popl %ebp\n"
	"  ret\n");

/* vuelve al programa con los registros del contexto (no vuelve) */
void __attribute__((noreturn)) vuelve_contexto(CTX *c);
__asm__(".text\n"
	"vuelve_contexto:\n"
	"  movl 4(%esp), %ebx\n"
	"  frstor 28(%ebx)\n"            /* (la coma flotante: FloatSave) */
	"  movl 196(%ebx), %eax\n"       /* esp */
	"  subl $12, %eax\n"
	"  movl 176(%ebx), %ecx\n  movl %ecx, (%eax)\n"      /* eax */
	"  movl 192(%ebx), %ecx\n  movl %ecx, 4(%eax)\n"     /* eflags */
	"  movl 184(%ebx), %ecx\n  movl %ecx, 8(%eax)\n"     /* eip */
	"  movl 156(%ebx), %edi\n  movl 160(%ebx), %esi\n"
	"  movl 168(%ebx), %edx\n  movl 172(%ebx), %ecx\n"
	"  movl 180(%ebx), %ebp\n"
	"  movl 164(%ebx), %ebx\n"
	"  movl %eax, %esp\n"
	"  popl %eax\n  popfl\n  ret\n");

static int en_la_pila(u32 d)
{
	u32 tope = teb_lee(4), fondo = teb_lee(8);
	if (!tope || !fondo)
		return 1;
	return d >= fondo && d + 8 <= tope && !(d & 3);
}

/* la cadena de FS:[0]: da 1 si alguien dice que se siga (ContinueExecution) */
static int recorre(REC *r, CTX *c)
{
	MARCO *m = (MARCO *)teb_lee(0);
	u32 dc = 0;
	while ((u32)m != 0xFFFFFFFF && m) {
		u32 x;
		if (!en_la_pila((u32)m)) {
			traza("SEH: marco %x fuera de la pila: no se sigue", (u32)m);
			break;
		}
		traza("SEH: %x al manejador %x (marco %x)", r->codigo, (u32)m->manejador, (u32)m);
		x = llama_manejador(m->manejador, r, m, c, &dc);
		if (x == 0) {
			if (r->banderas & EXCEPTION_NONCONTINUABLE)
				return 0;
			return 1;
		}
		m = m->sig;
	}
	return 0;
}

static void __attribute__((noreturn)) sin_manejador(REC *r, CTX *c)
{
	char t[500], d[48];
	if (filtro) {
		PUNTEROS p = { r, c };
		i32 x = filtro(&p);
		if (x == -1)
			vuelve_contexto(c);
		if (x == 1)
			w32_salir(r->codigo);
	}
	diag_donde(r->dir, d, sizeof d);
	if (r->codigo == 0xC0000005 && r->n >= 2)
		fmt(t, sizeof t, "%s ha fallado: %s en %s, al %s 0x%08x.\n\nQueda apuntado en %s.", mod_exe ? mod_exe->nombre : "El programa",
		    nombre_de(r->codigo), d, r->info[0] ? "escribir en" : "leer de", r->info[1], diag_ruta_log());
	else
		fmt(t, sizeof t, "%s ha fallado: %s (0x%08x) en %s.\n\nQueda apuntado en %s.", mod_exe ? mod_exe->nombre : "El programa",
		    nombre_de(r->codigo), r->codigo, d, diag_ruta_log());
	diag_apunta("EXCEPCIÓN SIN MANEJADOR: 0x%08x en %s (EAX %x EBX %x ECX %x EDX %x ESI %x EDI %x EBP %x ESP %x)%s%x", r->codigo, d,
		    c->eax, c->ebx, c->ecx, c->edx, c->esi, c->edi, c->ebp, c->esp, r->n >= 2 ? "; dirección 0x" : "", r->n >= 2 ? r->info[1] : 0);
	mensaje_fatal(t);
	w32_salir(r->codigo);
}

/* lo común: con el registro y el contexto ya hechos */
static void __attribute__((noreturn)) despacha_rec(REC *r, CTX *c)
{
	static int dentro;
	if (dentro > 8) {
		aviso_serie("SEH: excepciones dentro de excepciones: se acaba");
		w32_salir(r->codigo);
	}
	dentro++;
	if (recorre(r, c)) {
		dentro--;
		vuelve_contexto(c);
	}
	if (r->banderas & EXCEPTION_NONCONTINUABLE && recorre(r, c) == 0) {
		/* (alguien quiso seguir con una que no se puede) */
	}
	dentro--;
	sin_manejador(r, c);
}

/* adonde manda el núcleo las excepciones del procesador (win32.cas) */
void __attribute__((used, noreturn)) despacha(NUCLEO *k)
{
	REC r;
	CTX c;
	u8 op = 0;
	memset(&r, 0, sizeof r);
	memset(&c, 0, sizeof c);
	c.banderas = CONTEXT_FULL;
	c.eip = k->eip; c.eflags = k->eflags; c.esp = k->esp;
	c.eax = k->eax; c.ecx = k->ecx; c.edx = k->edx; c.ebx = k->ebx;
	c.ebp = k->ebp; c.esi = k->esi; c.edi = k->edi;
	c.cs = k->cs; c.ss = c.ds = c.es = k->ss;
	__asm__ volatile("movl %%fs, %0" : "=r"(c.fs));
	__asm__ volatile("fnsave %0\n frstor %0" : "+m"(c.cw) : : "memory");
	r.dir = k->eip;
	if (k->eip >= 0x10000 && k->eip < 0x7FFF0000 && v_hay(k->eip & ~0xFFFu, 1))
		op = *(u8 *)k->eip;
	switch (k->vector) {
	case 0: r.codigo = 0xC0000094; break;
	case 6: r.codigo = 0xC000001D; break;
	case 13:
		if (k->error == 3 * 8 + 2)
			r.codigo = 0x80000003;          /* int 3 (la puerta 3 no es del anillo 3) */
		else if (op == 0xFA || op == 0xFB || op == 0xF4 || (op >= 0xE4 && op <= 0xE7) || (op >= 0xEC && op <= 0xEF) || (op >= 0x6C && op <= 0x6F) || op == 0x0F)
			r.codigo = 0xC0000096;
		else {
			r.codigo = 0xC0000005;
			r.n = 2;
			r.info[0] = 0;
			r.info[1] = 0xFFFFFFFF;
		}
		break;
	case 14:
		r.codigo = 0xC0000005;
		r.n = 2;
		r.info[0] = (k->error & 2) ? 1 : 0;
		r.info[1] = k->cr2 - BASE_W32;
		if (r.info[1] >= teb_lee(8) - 0x10000 && r.info[1] < teb_lee(8))
			r.codigo = 0xC00000FD;          /* (justo debajo de la pila) */
		break;
	default:
		r.codigo = 0xC0000005;
		r.n = 2;
		r.info[1] = 0xFFFFFFFF;
	}
	traza("SEH: el procesador (vector %u, error %x, cr2 %x) en %x: %x", k->vector, k->error, k->cr2, k->eip, r.codigo);
	despacha_rec(&r, &c);
}

/* RaiseException: con el contexto de quien la llama (se sigue detrás de la llamada) */
void __attribute__((used, noreturn)) lanza_exc(u32 *p)
{
	/* p: lo que guarda la entrada (eflags, edi, esi, ebp, esp, ebx, edx, ecx,
	 * eax de pushad) y, detrás, la vuelta y los cuatro argumentos */
	REC r;
	CTX c;
	u32 *a = p + 10, n, k;
	memset(&r, 0, sizeof r);
	memset(&c, 0, sizeof c);
	c.banderas = CONTEXT_FULL;
	c.eflags = p[0];
	c.edi = p[1]; c.esi = p[2]; c.ebp = p[3]; c.ebx = p[5]; c.edx = p[6]; c.ecx = p[7]; c.eax = p[8];
	c.eip = p[9];
	c.esp = (u32)(p + 14);                  /* (stdcall: los 16 octetos de argumentos, fuera) */
	__asm__ volatile("movl %%cs, %0\n movl %%ss, %1" : "=r"(c.cs), "=r"(c.ss));
	c.ds = c.es = c.ss;
	__asm__ volatile("fnsave %0\n frstor %0" : "+m"(c.cw) : : "memory");
	r.codigo = a[0];
	r.banderas = a[1] & EXCEPTION_NONCONTINUABLE;
	r.dir = p[9];
	n = a[2] > 15 ? 15 : a[2];
	r.n = a[3] ? n : 0;
	for (k = 0; k < r.n; k++)
		r.info[k] = ((u32 *)a[3])[k];
	traza("RaiseException(%x) desde %x", r.codigo, r.dir);
	despacha_rec(&r, &c);
}
__asm__(".text\n"
	".globl RaiseException\n"
	"RaiseException:\n"
	"  pushal\n  pushfl\n"
	"  pushl %esp\n"
	"  call lanza_exc\n");
/* (para hazexp.py: es de KERNEL32)
API(void) RaiseException(u32 c, u32 f, u32 n, const u32 *a) */

/* RtlUnwind(marco, adónde, registro, valor): llama a los manejadores de la
 * cadena, de arriba abajo, con «se está deshaciendo», y los quita, hasta
 * llegar al marco (sin marco: todos). Vuelve a quien la llama (lo que hace
 * _global_unwind2 de VC6, que la llama con «adónde» justo detrás). */
API(void) RtlUnwind(void *destino, void *adonde, REC *rec, void *valor)
{
	REC r;
	CTX c;
	MARCO *m = (MARCO *)teb_lee(0);
	u32 dc = 0;
	(void)adonde; (void)valor;
	if (!rec) {
		memset(&r, 0, sizeof r);
		r.codigo = 0xC0000027;              /* STATUS_UNWIND */
		rec = &r;
	}
	rec->banderas |= destino ? EXCEPTION_UNWINDING : (EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND);
	memset(&c, 0, sizeof c);
	while ((u32)m != 0xFFFFFFFF && m && m != destino) {
		if (!en_la_pila((u32)m))
			break;
		llama_manejador(m->manejador, rec, m, &c, &dc);
		m = m->sig;
		teb_pon(0, (u32)m);
	}
}

/* el despachador, al núcleo (principal.c, al empezar) */
void seh_prepara(void)
{
	sc(183, 2, (u32)despacha, 0);
}

API(i32) UnhandledExceptionFilter(PUNTEROS *p)
{
	if (filtro)
		return filtro(p);
	return 0;
}
API(void) DebugBreak(void) { __asm__ volatile("int3"); }
API(void) RtlCaptureContext(CTX *c)
{
	memset(c, 0, sizeof *c);
	c->banderas = CONTEXT_FULL;
	__asm__ volatile("movl %%ebx, %0\n movl %%esi, %1\n movl %%edi, %2\n movl %%ebp, %3" : "=m"(c->ebx), "=m"(c->esi), "=m"(c->edi), "=m"(c->ebp));
	c->eip = (u32)__builtin_return_address(0);
}

/* IsBad*Ptr: mira de verdad si hay memoria ahí (las zonas de WIN32.BIN) */
API(BOOL) IsBadReadPtr(const void *p, u32 n) { return n && (!p || !v_hay((u32)p, n)); }
API(BOOL) IsBadWritePtr(void *p, u32 n) { return n && (!p || !v_hay((u32)p, n)); }
API(BOOL) IsBadCodePtr(void *p) { return !p || !v_hay((u32)p, 1); }
API(BOOL) IsBadStringPtrA(const char *p, u32 n)
{
	u32 k;
	if (!p)
		return n != 0;
	for (k = 0; k < n; k++) {
		if (!v_hay((u32)p + k, 1))
			return TRUE;
		if (!p[k])
			break;
	}
	return FALSE;
}
