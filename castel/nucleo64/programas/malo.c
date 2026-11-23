/* malo.c - Hispa-64, etapa 4 (docs/64/HISPA-64.md, sección 10): un programa
 * malicioso, para ver que no puede con el núcleo. «MALO punteros» pasa al
 * núcleo punteros que no son suyos (del núcleo, de nada, nulos, de solo
 * leer) en cada llamada que los lleva: todas tienen que dar su error
 * (-EFAULT...) y el programa sigue. Los demás modos hacen algo que lo cierra
 * (y solo a él): leer, escribir o ejecutar donde no debe, dividir entre cero,
 * una instrucción que no existe, una privilegiada, desbordar la pila...
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "sis.h"

extern char _start[];
#define NUCLEO ((void *)0xFFFFFFFF80100000UL)
#define DIRECTO ((void *)0xFFFF800000100000UL)
#define NADA ((void *)0x0000500000000000UL)

static int igual(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return *a == *b; }

static i64 hondo(i64 n)
{
	volatile char relleno[4096];
	relleno[n & 4095] = (char)n;
	return hondo(n + 1) + relleno[(n + 7) & 4095];
}

/* «MALO modo: voy a ...», de una vez (con los demás escribiendo a la vez, sin mezclarse) */
static void malo(const char *qué, const char *qué_hace)
{
	char t[160]; u64 n = 0;
	const char *trozos[4] = { "MALO ", qué, ": voy a ", qué_hace };
	int i;
	for (i = 0; i < 4; i++) { const char *c = trozos[i]; while (*c && n < sizeof t) t[n++] = *c++; }
	sis(S_WRITE, 1, t, n);
}

int principal(i64 *sp)
{
	char **argv = (char **)(sp + 1);
	const char *m = sp[0] > 1 ? argv[1] : "punteros";
	i64 fd;
	u64 ts[2] = { 0, 1000 };

	if (igual(m, "punteros")) {
		mira(sis(S_WRITE, 1, NUCLEO, 16) == -EFAULT, "write desde el n\xc3\xba" "cleo");
		mira(sis(S_WRITE, 1, DIRECTO, 16) == -EFAULT, "write desde el mapa directo");
		mira(sis(S_WRITE, 1, NADA, 16) == -EFAULT, "write desde nada");
		mira(sis(S_WRITE, 1, 0, 16) == -EFAULT, "write desde el puntero nulo");
		mira(sis(S_WRITE, 1, (void *)0x00007FFFFFFFFFF0UL, 64) == -EFAULT, "write que se sale de la mitad baja");
		mira(sis(S_WRITE, 1, (void *)0xFFFFFFFFFFFFFFF0UL, 64) == -EFAULT, "write que da la vuelta");
		fd = sis(S_OPEN, "SALUDO.TXT", 0, 0);
		mira(fd >= 3, "open de SALUDO.TXT");
		mira(sis(S_READ, fd, NUCLEO, 16) == -EFAULT, "read al n\xc3\xba" "cleo");
		mira(sis(S_READ, fd, DIRECTO, 16) == -EFAULT, "read al mapa directo");
		mira(sis(S_READ, fd, _start, 16) == -EFAULT, "read a su c\xc3\xb3" "digo (de solo leer)");
		mira(sis(S_OPEN, NUCLEO, 0, 0) == -EFAULT, "open con la ruta en el n\xc3\xba" "cleo");
		mira(sis(S_CLOCK_GETTIME, 1, NUCLEO, 0) == -EFAULT, "clock_gettime al n\xc3\xba" "cleo");
		mira(sis(S_CLOCK_GETTIME, 1, _start, 0) == -EFAULT, "clock_gettime a su c\xc3\xb3" "digo");
		mira(sis(S_UNAME, DIRECTO, 0, 0) == -EFAULT, "uname al mapa directo");
		mira(sis(S_NANOSLEEP, NUCLEO, 0, 0) == -EFAULT, "nanosleep desde el n\xc3\xba" "cleo");
		mira(sis(S_WRITEV, 1, NUCLEO, 1) == -EFAULT, "writev con la lista en el n\xc3\xba" "cleo");
		mira(sis(S_ARCH_PRCTL, 0x1002, NUCLEO, 0) == -EPERM, "arch_prctl con FS en el n\xc3\xba" "cleo");
		mira(sis(S_ARCH_PRCTL, 0x1003, NUCLEO, 0) == -EFAULT, "arch_prctl(ARCH_GET_FS) al n\xc3\xba" "cleo");
		mira(sis6(S_MMAP, (i64)NUCLEO, 4096, 3, 0x32, -1, 0) == -ENOMEM, "mmap fijo en el n\xc3\xba" "cleo");
		mira(sis6(S_MMAP, 0, 4096, 3, 0x32, -1, 0) == -EPERM, "mmap fijo en la p\xc3\xa1gina 0");
		mira(sis(S_MUNMAP, NUCLEO, 4096, 0) == -EINVAL, "munmap del n\xc3\xba" "cleo");
		mira(sis(S_MPROTECT, NUCLEO, 4096, 7) == -ENOMEM, "mprotect del n\xc3\xba" "cleo");
		mira(sis(S_BRK, NUCLEO, 0, 0) != (i64)NUCLEO, "brk hasta el n\xc3\xba" "cleo");
		mira(sis(S_NANOSLEEP, ts, 0, 0) == 0, "y sigue vivo (nanosleep)");
		if (fallos == 0) { pon("MALO punteros: "); pon_n(pruebas); pon(" llamadas con punteros malos, todas con su error (-EFAULT, -EPERM...); sigue vivo\n"); }
		else { pon("MALO punteros: "); pon_n(fallos); pon(" MAL\n"); }
		return fallos;
	}
	if (igual(m, "leer")) { malo(m, "leer la memoria del n\xc3\xba" "cleo\n"); fd = *(volatile i64 *)NUCLEO; }
	else if (igual(m, "escribir")) { malo(m, "escribir en el mapa directo del n\xc3\xba" "cleo\n"); *(volatile i64 *)DIRECTO = 1; }
	else if (igual(m, "nulo")) { malo(m, "leer del puntero nulo\n"); fd = *(volatile i64 *)8; }
	else if (igual(m, "division")) { malo(m, "dividir entre cero\n"); __asm__ volatile ("xor %%edx, %%edx; xor %%ecx, %%ecx; mov $7, %%eax; div %%ecx" ::: "rax", "rcx", "rdx"); }
	else if (igual(m, "ud")) { malo(m, "ejecutar una instrucci\xc3\xb3n que no existe (ud2)\n"); __asm__ volatile ("ud2"); }
	else if (igual(m, "cli")) { malo(m, "quitar las interrupciones (cli)\n"); __asm__ volatile ("cli"); }
	else if (igual(m, "puerto")) { malo(m, "leer el puerto del teclado (in al, 0x60)\n"); __asm__ volatile ("inb $0x60, %%al" ::: "rax"); }
	else if (igual(m, "nx")) { malo(m, "ejecutar en la pila\n"); { volatile unsigned char c[16] = { 0xC3 }; ((void (*)(void))c)(); } }
	else if (igual(m, "codigo")) { malo(m, "escribir en su propio c\xc3\xb3" "digo\n"); *(volatile char *)_start = 0; }
	else if (igual(m, "pila")) { malo(m, "desbordar la pila (una funci\xc3\xb3n que se llama a s\xc3\xad misma sin fin)\n"); fd = hondo(0); }
	else if (igual(m, "saltar")) { malo(m, "saltar al n\xc3\xba" "cleo\n"); ((void (*)(void))NUCLEO)(); }
	else { pon("MALO: no s\xc3\xa9 hacer \xc2\xab"); pon(m); pon("\xc2\xbb\n"); return 2; }
	pon("MALO "); pon(m); pon(": \xc2\xa1sigo vivo!\n");
	return 1;
}
