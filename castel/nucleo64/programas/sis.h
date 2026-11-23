/* sis.h - Hispa-64, etapa 4 (docs/64/HISPA-64.md, sección 10): lo mínimo
 * para los programas de prueba en C sin biblioteca (gcc -static -nostdlib
 * -ffreestanding): las llamadas al sistema de Linux x86-64 con «syscall»
 * (las mismas en Hispa-64), el arranque (_start: la pila de Linux, con argc,
 * argv, envp y el vector auxiliar) y unas funciones para escribir.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef SIS_H
#define SIS_H

typedef unsigned long u64;
typedef long i64;

static inline i64 sis6(i64 n, i64 a, i64 b, i64 c, i64 d, i64 e, i64 f)
{
	register i64 r10 __asm__("r10") = d;
	register i64 r8 __asm__("r8") = e;
	register i64 r9 __asm__("r9") = f;
	i64 r;
	__asm__ volatile ("syscall" : "=a"(r) : "a"(n), "D"(a), "S"(b), "d"(c), "r"(r10), "r"(r8), "r"(r9)
			  : "rcx", "r11", "memory");
	return r;
}
#define sis(n, a, b, c) sis6((n), (i64)(a), (i64)(b), (i64)(c), 0, 0, 0)

enum { S_READ = 0, S_WRITE = 1, S_OPEN = 2, S_CLOSE = 3, S_LSEEK = 8, S_MMAP = 9, S_MPROTECT = 10,
       S_MUNMAP = 11, S_BRK = 12, S_IOCTL = 16, S_WRITEV = 20, S_YIELD = 24, S_NANOSLEEP = 35,
       S_GETPID = 39, S_EXIT = 60, S_UNAME = 63, S_ARCH_PRCTL = 158, S_SET_TID = 218,
       S_CLOCK_GETTIME = 228, S_EXIT_GROUP = 231, S_OPENAT = 257 };
enum { EPERM = 1, ENOENT = 2, EBADF = 9, ENOMEM = 12, EFAULT = 14, ENODEV = 19, EINVAL = 22,
       ENOTTY = 25, ESPIPE = 29, EROFS = 30, ENOSYS = 38 };

/* gcc puede llamarlas aunque no estén en el programa */
void *memset(void *d, int c, u64 n);
void *memcpy(void *d, const void *s, u64 n);

/* el arranque: la pila tal cual la deja el sistema, a principal(sp) */
int principal(i64 *sp);
__asm__(".text\n.globl _start\n_start:\n\txor %ebp, %ebp\n\tmov %rsp, %rdi\n\tand $-16, %rsp\n"
	"\tcall principal\n\tmov %eax, %edi\n\tmov $231, %eax\n\tsyscall\n\tud2\n");

static u64 largo(const char *s) { u64 n = 0; while (s[n]) n++; return n; }
/* lo que se escribe va por líneas enteras (un write por línea): con varios
 * programas escribiendo a la vez, las líneas no se mezclan */
static char sis_línea[512];
static u64 sis_n;
static void pon(const char *s)
{
	while (*s) {
		sis_línea[sis_n++] = *s;
		if (*s++ == '\n' || sis_n == sizeof sis_línea) { sis(S_WRITE, 1, sis_línea, sis_n); sis_n = 0; }
	}
}
static void pon_n(i64 v)
{
	char t[24]; int i = 23; int neg = v < 0; u64 u = neg ? -(u64)v : (u64)v;
	t[i] = 0;
	do { t[--i] = '0' + u % 10; u /= 10; } while (u);
	if (neg) t[--i] = '-';
	pon(t + i);
}
static void pon_hex(u64 v)
{
	char t[19]; int i;
	t[0] = '0'; t[1] = 'x'; t[18] = 0;
	for (i = 0; i < 16; i++) t[17 - i] = "0123456789ABCDEF"[(v >> (4 * i)) & 15];
	pon(t);
}

void *memset(void *d, int c, u64 n) { unsigned char *p = d; while (n--) *p++ = (unsigned char)c; return d; }
void *memcpy(void *d, const void *s, u64 n) { unsigned char *p = d; const unsigned char *q = s; while (n--) *p++ = *q++; return d; }

static int fallos, pruebas;
static void mira(int bien, const char *qué)
{
	pruebas++;
	if (!bien) { fallos++; pon("  MAL: "); pon(qué); pon("\n"); }
}

#endif
