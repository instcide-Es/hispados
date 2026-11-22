/* SEH.EXE - prueba de las excepciones estructuradas de WIN32.BIN (JUEGOS):
 * los fallos del procesador (puntero nulo, dividir por cero, instrucción que
 * no hay, int 3, una del sistema) entregados al programa por la cadena de
 * FS:[0], seguir donde diga el contexto, RaiseException, RtlUnwind con dos
 * marcos, el __try/__except y el __try/__finally de Visual C++ 6 (el marco
 * que hace el compilador, escrito aquí a mano en ensamblador, con
 * _except_handler3 de MSVCRT.DLL), en otro hilo, longjmp, IsBadReadPtr y
 * SetUnhandledExceptionFilter. Cada línea, «nombre: valor».
 *   SEH          todo
 *   SEH /SOLO    un fallo sin manejador: el aviso de WIN32 (y se acaba)
 *   SEH /FILTRO  el mismo, con SetUnhandledExceptionFilter (EXECUTE_HANDLER)
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "j.h"

typedef struct MARCO { struct MARCO *sig; void *manejador; } MARCO;
static volatile u32 visto_codigo, visto_info0, visto_info1, visto_banderas, visto_n, deshecho;
static volatile int salta;              /* lo que hay que saltar en EIP para seguir */

static int manejador_salta(EXCEPTION_RECORD *r, MARCO *m, CONTEXT *c, void *dc)
{
	(void)m; (void)dc;
	visto_codigo = r->ExceptionCode;
	visto_info0 = r->NumberParameters > 0 ? r->ExceptionInformation[0] : 0xAAAA;
	visto_info1 = r->NumberParameters > 1 ? r->ExceptionInformation[1] : 0xAAAA;
	visto_n = r->NumberParameters;
	if (r->ExceptionCode == 0xC0000094) {
		c->Ecx = 4;                     /* el divisor: que ahora sí se pueda */
		return 0;
	}
	if (r->ExceptionCode == 0xE0001234)
		return 0;                       /* RaiseException: vuelve */
	c->Eip += salta;
	return 0;                               /* ExceptionContinueExecution */
}
static void pon(MARCO *m, void *f) { m->sig = lee_fs0(); m->manejador = f; pon_fs0(m); }
static void quita(MARCO *m) { pon_fs0(m->sig); }

static u32 escribe_nulo(void)
{
	u32 r;
	__asm__ volatile("xorl %%ecx, %%ecx\n movl $7, %%eax\n movl %%eax, (%%ecx)\n movl $55, %0" : "=r"(r) : : "eax", "ecx", "memory");
	return r;
}
static u32 lee_de(u32 d)
{
	u32 r;
	__asm__ volatile("movl %1, %%ecx\n movl $66, %%eax\n movl (%%ecx), %%eax\n movl %%eax, %0" : "=r"(r) : "r"(d) : "eax", "ecx", "memory");
	return r;
}
static u32 divide(u32 a)
{
	u32 r;
	__asm__ volatile("xorl %%edx, %%edx\n xorl %%ecx, %%ecx\n divl %%ecx\n" : "=a"(r) : "a"(a) : "ecx", "edx");
	return r;
}

/* ---- dos marcos: el de dentro no la quiere, el de fuera deshace y sigue en «a_salvo» ---- */
static volatile u32 esp_salvo, ebp_salvo;
extern char a_salvo[];
static int manejador_dentro(EXCEPTION_RECORD *r, MARCO *m, CONTEXT *c, void *dc)
{
	(void)m; (void)c; (void)dc;
	if (r->ExceptionFlags & 2)
		deshecho++;                     /* se está deshaciendo */
	return 1;                               /* ExceptionContinueSearch */
}
static int manejador_fuera(EXCEPTION_RECORD *r, MARCO *m, CONTEXT *c, void *dc)
{
	(void)dc;
	if (r->ExceptionFlags & 2)
		return 1;
	visto_codigo = r->ExceptionCode;
	RtlUnwind(m, 0, r, 0);                  /* (los de debajo, avisados y fuera) */
	c->Eip = (u32)a_salvo;
	c->Esp = esp_salvo;
	c->Ebp = ebp_salvo;
	return 0;
}
static void __attribute__((noinline)) falla_dentro(void)
{
	MARCO m;
	pon(&m, manejador_dentro);
	lee_de(0x10);
	quita(&m);
}
static int dos_marcos(void)
{
	int bien = 0;
	volatile MARCO fuera_m;         /* (en la pila: Windows no mira los marcos de fuera de ella) */
	pon((MARCO *)&fuera_m, manejador_fuera);
	__asm__ volatile("movl %%esp, %0\n movl %%ebp, %1" : "=m"(esp_salvo), "=m"(ebp_salvo));
	falla_dentro();
	__asm__ volatile(".globl a_salvo\na_salvo:" : : : "ebx", "esi", "edi", "memory");
	bien = lee_fs0() == (void *)&fuera_m;
	quita((MARCO *)&fuera_m);
	return bien;
}

/* ---- el __try/__except de VC6: el marco del compilador, a mano ----
 * int vc6(int que): __try { if (que == 1) *(int*)0 = 1; else if (que == 2)
 * RaiseException(0xE0000001...); return 1; } __except (filtro: AV ->
 * EXECUTE_HANDLER; si no, CONTINUE_SEARCH) { return 42; }
 * y con __finally dentro: vc6_fin(): __try { otra(); } __finally { fin++; } */
volatile int fin_hecho;
int vc6(int que);
int vc6_finally(void);
void WINAPI RaiseException(u32, u32, u32, const u32 *);
int _except_handler3(void *, void *, void *, void *);
void _local_unwind2(void *, int);
__asm__(".text\n"
	".globl vc6\n"
	"vc6:\n"
	"  pushl %ebp\n  movl %esp, %ebp\n"
	"  pushl $-1\n"                         /* [ebp-4]: el nivel */
	"  pushl $vc6_tabla\n"                  /* [ebp-8]: la tabla */
	"  pushl $_except_handler3\n"           /* [ebp-0Ch] */
	"  movl %fs:0, %eax\n  pushl %eax\n"    /* [ebp-10h]: el de antes */
	"  movl %esp, %fs:0\n"
	"  subl $8, %esp\n"                     /* [ebp-14h] los punteros, [ebp-18h] la pila */
	"  pushl %ebx\n  pushl %esi\n  pushl %edi\n"
	"  movl %esp, -0x18(%ebp)\n"
	"  movl $0, -4(%ebp)\n"                 /* __try */
	"  movl 8(%ebp), %eax\n"
	"  cmpl $1, %eax\n  jne 1f\n"
	"  xorl %ecx, %ecx\n  movl $1, (%ecx)\n"
	"1: cmpl $2, %eax\n  jne 2f\n"
	"  pushl $0\n  pushl $0\n  pushl $0\n  pushl $0xE0000001\n  call RaiseException\n"
	"2: movl $-1, -4(%ebp)\n"
	"  movl $1, %eax\n"
	"  jmp vc6_fin\n"
	"vc6_filtro:\n"                         /* el filtro: con el EBP de la función */
	"  movl -0x14(%ebp), %eax\n  movl (%eax), %eax\n  movl (%eax), %eax\n"
	"  cmpl $0xC0000005, %eax\n  jne 3f\n  movl $1, %eax\n  ret\n"
	"3: xorl %eax, %eax\n  ret\n"
	"vc6_bloque:\n"                         /* el bloque __except */
	"  movl -0x18(%ebp), %esp\n"
	"  movl $-1, -4(%ebp)\n"
	"  movl $42, %eax\n"
	"vc6_fin:\n"
	"  movl -0x10(%ebp), %ecx\n  movl %ecx, %fs:0\n"
	"  popl %edi\n  popl %esi\n  popl %ebx\n"
	"  movl %ebp, %esp\n  popl %ebp\n  ret\n"
	".section .rodata\n"
	"vc6_tabla: .long -1, vc6_filtro, vc6_bloque\n"
	".text\n"
	/* el de fuera (__except) llama a uno con __finally que falla */
	".globl vc6_finally\n"
	"vc6_finally:\n"
	"  pushl %ebp\n  movl %esp, %ebp\n"
	"  pushl $-1\n  pushl $vf_tabla\n  pushl $_except_handler3\n"
	"  movl %fs:0, %eax\n  pushl %eax\n  movl %esp, %fs:0\n"
	"  subl $8, %esp\n  pushl %ebx\n  pushl %esi\n  pushl %edi\n  movl %esp, -0x18(%ebp)\n"
	"  movl $0, -4(%ebp)\n"
	"  call con_finally\n"
	"  movl $-1, -4(%ebp)\n  movl $1, %eax\n  jmp vf_fin\n"
	"vf_filtro:\n  movl $1, %eax\n  ret\n"
	"vf_bloque:\n  movl -0x18(%ebp), %esp\n  movl $-1, -4(%ebp)\n  movl $43, %eax\n"
	"vf_fin:\n  movl -0x10(%ebp), %ecx\n  movl %ecx, %fs:0\n"
	"  popl %edi\n  popl %esi\n  popl %ebx\n  movl %ebp, %esp\n  popl %ebp\n  ret\n"
	"con_finally:\n"
	"  pushl %ebp\n  movl %esp, %ebp\n"
	"  pushl $-1\n  pushl $cf_tabla\n  pushl $_except_handler3\n"
	"  movl %fs:0, %eax\n  pushl %eax\n  movl %esp, %fs:0\n"
	"  subl $8, %esp\n  pushl %ebx\n  pushl %esi\n  pushl %edi\n  movl %esp, -0x18(%ebp)\n"
	"  movl $0, -4(%ebp)\n"
	"  xorl %ecx, %ecx\n  movl (%ecx), %eax\n"      /* falla dentro del __try/__finally */
	"  movl $-1, -4(%ebp)\n  call cf_bloque\n"      /* (sin fallo: el __finally, al salir) */
	"  movl -0x10(%ebp), %ecx\n  movl %ecx, %fs:0\n"
	"  popl %edi\n  popl %esi\n  popl %ebx\n  movl %ebp, %esp\n  popl %ebp\n  ret\n"
	"cf_bloque:\n  incl fin_hecho\n  ret\n"         /* el __finally */
	".section .rodata\n"
	"vf_tabla: .long -1, vf_filtro, vf_bloque\n"
	"cf_tabla: .long -1, 0, cf_bloque\n"
	".text\n");

/* ---- en otro hilo, su propia cadena ---- */
static u32 WINAPI hilo_falla(void *p)
{
	(void)p;
	return (u32)vc6(1);
}

static i32 WINAPI filtro_final(EXCEPTION_POINTERS *p)
{
	printf("filtro_final: %08x en %s\n", p->ExceptionRecord->ExceptionCode, p->ContextRecord->Eip ? "su sitio" : "?");
	fflush(stdout);
	return 1;                               /* EXCEPTION_EXECUTE_HANDLER: se acaba */
}

int main(int argc, char **argv)
{
	MARCO m;
	u32 r;
	jmp_buf jb;
	if (argc > 1 && !strcmp(argv[1], "/SOLO")) {
		printf("solo: va a escribir en 0x24\n");
		fflush(stdout);
		*(volatile int *)0x24 = 1;
		printf("MAL: sigue\n");
		return 0;
	}
	if (argc > 1 && !strcmp(argv[1], "/FILTRO")) {
		SetUnhandledExceptionFilter(filtro_final);
		lee_de(0x30);
		printf("MAL: sigue\n");
		return 0;
	}
	pon(&m, manejador_salta);
	salta = 2;
	r = escribe_nulo();
	printf("nulo_escritura: %08x %u %08x %u\n", visto_codigo, visto_info0, visto_info1, r);
	r = lee_de(0x400);
	printf("nulo_lectura: %08x %u %08x\n", visto_codigo, visto_info0, visto_info1);
	r = lee_de(0x6FFF0000);
	printf("sin_memoria: %08x %08x\n", visto_codigo, visto_info1);
	r = divide(100);
	printf("division: %08x %u\n", visto_codigo, r);
	__asm__ volatile("ud2");
	printf("ud2: %08x\n", visto_codigo);
	salta = 1;
	__asm__ volatile("int3");
	printf("int3: %08x\n", visto_codigo);
	__asm__ volatile("cli");
	printf("cli: %08x\n", visto_codigo);
	{
		u32 a[2] = { 11, 22 };
		RaiseException(0xE0001234, 0, 2, a);
		printf("raise: %08x %u %u %u\n", visto_codigo, visto_n, visto_info0, visto_info1);
	}
	quita(&m);
	printf("cadena_vacia: %s\n", (u32)lee_fs0() != 0xFFFFFFFF ? "no" : "si");
	r = dos_marcos();
	printf("dos_marcos: %08x %u %s\n", visto_codigo, deshecho, r ? "bien" : "mal");
	r = vc6(0);
	printf("vc6_except: %u %d\n", r, vc6(1));
	fin_hecho = 0;
	r = vc6_finally();
	printf("vc6_finally: %u %d\n", r, fin_hecho);
	{
		HANDLE t = CreateThread(0, 0, hilo_falla, 0, 0, 0);
		u32 c = 0;
		WaitForSingleObject(t, 5000);
		GetExitCodeThread(t, &c);
		printf("en_otro_hilo: %u\n", c);
	}
	{
		void *antes = lee_fs0();
		volatile MARCO m2;
		if (!_setjmp3(jb, 0)) {
			pon((MARCO *)&m2, manejador_salta);
			longjmp(jb, 1);
		}
		printf("longjmp_cadena: %s\n", lee_fs0() == antes ? "bien" : "mal");
	}
	printf("isbad: %d %d %d %d\n", IsBadReadPtr(0, 4), IsBadReadPtr(&r, 4), IsBadWritePtr((void *)0x6FFF0000, 4), IsBadReadPtr((void *)0x10, 1));
	printf("FIN SEH\n");
	return 0;
}
