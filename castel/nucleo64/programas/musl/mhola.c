/* Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * nucleo64/programas/musl/mhola.c - Hispa-64, etapa 7 (docs/64/HISPA-64.md,
 * sección 13): el «hola» de musl: printf, malloc, el entorno, los
 * archivos, la hora, qsort, getrandom... Se hace tres veces: estático
 * (MHOLA), dinámico con ld-musl y libsaludo.so (MHOLAD) y estático de
 * posición libre (MHOLAP). Con «-q», solo dice la línea del final (para
 * leerla por una tubería: MPROCESOS). */
#include "comun.h"
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/utsname.h>
#include <sys/random.h>
#include <sys/syscall.h>

#ifdef CON_LIB
const char *saludo(int n);
#endif

static int compara(const void *a, const void *b)
{
	int x = *(const int *)a, y = *(const int *)b;
	return (x > y) - (x < y);
}

static long ms_desde(struct timespec *t0)
{
	struct timespec t1;
	clock_gettime(CLOCK_MONOTONIC, &t1);
	return (t1.tv_sec - t0->tv_sec) * 1000 + (t1.tv_nsec - t0->tv_nsec) / 1000000;
}

int main(int argc, char **argv)
{
	char t[200], *e;
	int i, *v, callado = argc > 1 && !strcmp(argv[1], "-q");
	struct utsname u;
	struct timespec t0, d = { 0, 20000000 };
	unsigned char r1[32], r2[32];
	FILE *f;
	long n;

	if (callado) {
		printf("MHOLA (por una tubería): %s, argc %d\n", cómo(), argc);
		return 0;
	}
	printf("MHOLA: hola desde musl (printf); %s\n", cómo());
	MIRA(argc == 3 && !strcmp(argv[1], "uno") && !strcmp(argv[2], "dos"), "argv");
	e = getenv("HOME");
	MIRA(e && (!strcmp(e, "/") || !strcmp(e, "/casa")), "getenv(HOME)");   /* [etapa 11 a] la casa, si la hay */
	/* printf y compañía */
	snprintf(t, sizeof t, "%d|%x|%s|%5.2f|%e|%c|%-4s|%05d", -42, 255, "ñandú", 3.14159, 12345.678, 'Z', "ab", 42);
	MIRA(!strcmp(t, "-42|ff|ñandú| 3.14|1.234568e+04|Z|ab  |00042"), "snprintf");
	MIRA(strtod("3.25", 0) == 3.25 && strtol("-0x1F", 0, 16) == -31, "strtod y strtol");
	/* malloc: grandes y pequeños */
	v = malloc(100000 * sizeof *v);
	MIRA(v != 0, "malloc de 400 KB");
	for (i = 0; i < 100000; i++) v[i] = (i * 7919) % 100003;
	qsort(v, 100000, sizeof *v, compara);
	for (i = 1; i < 100000 && v[i - 1] <= v[i]; i++) ;
	MIRA(i == 100000, "qsort de 100.000");
	v = realloc(v, 4000000 * sizeof *v);
	MIRA(v != 0 && v[99999] >= v[0], "realloc a 16 MB");
	free(v);
	{
		char *p[1000];
		int ok = 1;
		for (i = 0; i < 1000; i++) { p[i] = malloc(1 + i * 37 % 3000); memset(p[i], i & 255, 1 + i * 37 % 3000); }
		for (i = 0; i < 1000; i++) { if ((unsigned char)p[i][i * 37 % 3000] != (i & 255)) ok = 0; free(p[i]); }
		MIRA(ok, "1000 malloc y free");
		p[0] = calloc(1000, 1000);
		for (i = 0; i < 1000000 && !p[0][i]; i++) ;
		MIRA(i == 1000000, "calloc de 1 MB, a cero");
		free(p[0]);
	}
	/* la hora */
	MIRA(time(0) > 1767225600, "time(): después del 1/1/2026");
	clock_gettime(CLOCK_MONOTONIC, &t0);
	nanosleep(&d, 0);
	n = ms_desde(&t0);
	MIRA(n >= 19 && n < 500, "nanosleep de 20 ms");
	/* un archivo de la unidad en memoria */
	f = fopen("/SALUDO.TXT", "r");
	MIRA(f && fgets(t, sizeof t, f) && !strncmp(t, "Hola", 4), "fopen y fgets de /SALUDO.TXT");
	if (f) fclose(f);
	MIRA(fopen("/NO-ESTA.TXT", "r") == 0 && errno == ENOENT, "fopen de uno que no está: ENOENT");
	/* el sistema */
	MIRA(uname(&u) == 0 && !strcmp(u.sysname, "Hispa-64") && !strcmp(u.machine, "x86_64"), "uname");
	printf("  uname: %s %s %s (%s)\n", u.sysname, u.release, u.machine, u.version);
	MIRA(getpid() > 0 && syscall(SYS_gettid) == getpid(), "getpid y gettid");
	MIRA(sysconf(_SC_PAGESIZE) == 4096, "sysconf(_SC_PAGESIZE)");
	n = sysconf(_SC_NPROCESSORS_ONLN);
	MIRA(n >= 1, "sysconf(_SC_NPROCESSORS_ONLN)");
	printf("  procesadores (sched_getaffinity): %ld\n", n);
	MIRA(isatty(1) == 1 && isatty(0) == 1, "isatty: la consola es una terminal");
	/* getrandom */
	MIRA(getrandom(r1, 32, 0) == 32 && getrandom(r2, 32, 0) == 32 && memcmp(r1, r2, 32), "getrandom: dos veces, distintos");
	fprintf(stderr, "  (esto va por stderr)\n");
#ifdef CON_LIB
	{
		const char *s = saludo(2);
		printf("  %s\n", s);
		MIRA(!strcmp(s, "libsaludo.so: hola (42)"), "la biblioteca libsaludo.so (ld-musl la carga con mmap)");
	}
#endif
	return acaba("MHOLA");
}
