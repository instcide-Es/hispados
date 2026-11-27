/* Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * nucleo64/programas/ladybird/mjs.c - Hispa-64, etapa 10 g (docs/64/HISPA-64.md,
 * sección 24): MJS corre las pruebas de LibJS de Ladybird (Tests/LibJS/Runtime:
 * 1.324 archivos de pruebas en JavaScript, con test-common.js) con
 * test-js-runtime (el corredor de Ladybird, LibJS en C++ y Rust, compilado con
 * su CMake por hazweb.sh) y dice cuántas salen bien.
 * MJS lleva dentro las pruebas (un tar, js.tar, con .incbin), las deja en
 * /tmp/js y corre «test-js-runtime /tmp/js/Runtime /tmp/js/Runtime/test-common.js»
 * con un plazo (900 s); lo que dice va a /tmp/testjs.txt, y al puerto serie el
 * resumen y lo que no sale. Al acabar, borra /tmp/js.
 * Los fallos «conocidos» (abajo, con el porqué: los mismos en Linux) no
 * cuentan; cualquier otro, sí. Con «MJS FILTRO», solo las que encajen.
 * Al final, «MJS: todo bien (N pruebas)» y el código 0. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <spawn.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <ftw.h>
#include <signal.h>

extern char **environ;
extern const unsigned char js_tar[], js_tar_fin[];

/* los archivos que no salen, también en Linux, y por qué */
static const struct { const char *archivo, *porqué; } conocidos[] = {
	{ "gc-moving-cells.js", "pide la función «jit» del JIT de LibJS, que va apagado (ENABLE_JS_JIT=OFF)" },
};
#define N_CONOCIDOS (sizeof conocidos / sizeof conocidos[0])

static int mkdir_p(char *ruta)
{
	for (char *p = ruta + 1; *p; p++)
		if (*p == '/') { *p = 0; if (mkdir(ruta, 0755) && errno != EEXIST) return -1; *p = '/'; }
	return mkdir(ruta, 0755) && errno != EEXIST ? -1 : 0;
}

/* abre el tar (ustar) en base */
static int abre_tar(const char *base, int *n)
{
	const unsigned char *p = js_tar, *fin = js_tar_fin;
	char ruta[1024];
	*n = 0;
	while (p + 512 <= fin && p[0]) {
		char nombre[256], prefijo[156];
		memcpy(nombre, p, 100); nombre[100] = 0;
		memcpy(prefijo, p + 345, 155); prefijo[155] = 0;
		unsigned long largo = strtoul((const char *)p + 124, NULL, 8);
		char tipo = p[156];
		if (strstr(nombre, "..") || strstr(prefijo, "..")) return -1;
		snprintf(ruta, sizeof ruta, "%s/%s%s%s", base, prefijo, prefijo[0] ? "/" : "", nombre);
		p += 512;
		if (p + largo > fin) return -1;
		if (tipo == '5') {
			size_t l = strlen(ruta);
			if (l && ruta[l - 1] == '/') ruta[l - 1] = 0;
			if (mkdir_p(ruta)) return -1;
		} else if (tipo == '0' || tipo == 0) {
			char *b = strrchr(ruta, '/');
			*b = 0; if (mkdir_p(ruta)) return -1; *b = '/';
			int fd = open(ruta, O_WRONLY | O_CREAT | O_TRUNC, 0644);
			if (fd < 0) return -1;
			for (unsigned long h = 0; h < largo;) {
				ssize_t w = write(fd, p + h, largo - h);
				if (w <= 0) { close(fd); return -1; }
				h += w;
			}
			close(fd);
			(*n)++;
		}
		p += (largo + 511) / 512 * 512;
	}
	return 0;
}

static int borra_uno(const char *ruta, const struct stat *st, int tipo, struct FTW *ftw)
{
	(void)st; (void)tipo; (void)ftw;
	return remove(ruta);
}

static void borra(const char *ruta)
{
	nftw(ruta, borra_uno, 16, FTW_DEPTH | FTW_PHYS);
}

/* corre argv (lo que dice, a sal) con un plazo: 1 si acaba (estado en *st), 0
 * si no (lo cierra con SIGKILL), -errno si no arranca; en *ms, lo que tarda */
static int corre(const char *prog, char **argv, const char *sal, int plazo, int *st, long *ms)
{
	posix_spawn_file_actions_t fa;
	posix_spawn_file_actions_init(&fa);
	posix_spawn_file_actions_addopen(&fa, 1, sal, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	posix_spawn_file_actions_adddup2(&fa, 1, 2);
	pid_t pid;
	struct timespec g0, g1;
	clock_gettime(CLOCK_MONOTONIC, &g0);
	int e = posix_spawn(&pid, prog, &fa, NULL, argv, environ);
	posix_spawn_file_actions_destroy(&fa);
	if (e) return -e;
	int hecho = 0;
	for (;;) {
		if (waitpid(pid, st, WNOHANG) == pid) { hecho = 1; break; }
		clock_gettime(CLOCK_MONOTONIC, &g1);
		if (g1.tv_sec - g0.tv_sec >= plazo) break;
		struct timespec d = { 0, 20000000 };
		nanosleep(&d, NULL);
	}
	if (!hecho) { kill(pid, SIGKILL); waitpid(pid, st, 0); }
	clock_gettime(CLOCK_MONOTONIC, &g1);
	*ms = (g1.tv_sec - g0.tv_sec) * 1000 + (g1.tv_nsec - g0.tv_nsec) / 1000000;
	return hecho;
}

static const char *conocido(const char *l)
{
	for (unsigned i = 0; i < N_CONOCIDOS; i++)
		if (strstr(l, conocidos[i].archivo)) return conocidos[i].porqué;
	return NULL;
}

int main(int argc, char **argv)
{
	char carpeta[512], prog[600];
	ssize_t r = readlink("/proc/self/exe", carpeta, sizeof carpeta - 1);
	if (r > 0) { carpeta[r] = 0; char *b = strrchr(carpeta, '/'); if (b) *b = 0; } else strcpy(carpeta, ".");
	snprintf(prog, sizeof prog, "%s/test-js-runtime", carpeta);
	setvbuf(stdout, NULL, _IOLBF, 0);
	printf("MJS: las pruebas de LibJS de Ladybird (test-js-runtime: C++23 con GCC %d.%d, Rust y musl)\n", __GNUC__, __GNUC_MINOR__);
	int n = 0;
	char base[] = "/tmp/js";
	borra(base);
	if (abre_tar(base, &n)) { printf("  no se pueden dejar las pruebas en /tmp/js (%s)\nMJS: 1 mal de 1\n", strerror(errno)); return 1; }
	printf("  %d archivos de las pruebas (%zu octetos) en /tmp/js\n", n, (size_t)(js_tar_fin - js_tar));
	const char *sal = "/tmp/testjs.txt";
	char *av[8];
	int k = 0;
	av[k++] = prog;
	av[k++] = "--verbose";             /* (cada archivo antes de correrlo: si se cae, se sabe en cuál) */
	if (argc > 1) { av[k++] = "--filter"; av[k++] = argv[1]; }
	av[k++] = "/tmp/js/Runtime";
	av[k++] = "/tmp/js/Runtime/test-common.js";
	av[k] = NULL;
	int st = 0;
	long ms = 0;
	int c = corre(prog, av, sal, 900, &st, &ms);
	if (c < 0) { printf("  test-js-runtime no se puede poner en marcha (%s)\nMJS: 1 mal de 1\n", strerror(-c)); return 1; }
	/* el resumen: «Test Suites: ...», «Tests: ... N passed ...», «Files: N total» */
	FILE *f = fopen(sal, "r");
	char l[2048], resumen[3][256] = { "", "", "" };
	int pasan = 0, fallan = 0, archivos = 0, malos = 0, sabidos = 0, dichos = 0;
	while (f && fgets(l, sizeof l, f)) {
		l[strcspn(l, "\n")] = 0;
		char *t;
		if (strncmp(l, " FAIL ", 6) == 0) {
			const char *q = conocido(l);
			if (q) { sabidos++; printf("  conocido (igual en Linux): %s, %s\n", l + 6, q); }
			else { malos++; if (dichos++ < 12) printf("  no sale: %.200s\n", l + 6); }
		} else if (strncmp(l, "Test Suites:", 12) == 0) snprintf(resumen[0], sizeof resumen[0], "%.250s", l);
		else if (strncmp(l, "Tests:", 6) == 0) {
			snprintf(resumen[1], sizeof resumen[1], "%.250s", l);
			if ((t = strstr(l, " passed"))) { while (t > l && t[-1] >= '0' && t[-1] <= '9') t--; pasan = atoi(t); }
			if ((t = strstr(l, " failed,")) || (t = strstr(l, " failed"))) {
				char *u = t;
				while (u > l && u[-1] >= '0' && u[-1] <= '9') u--;
				/* («expected failed» no son fallos) */
				if (!(u - l >= 9 && strncmp(u - 9, "expected ", 9) == 0)) fallan = atoi(u);
			}
		} else if (strncmp(l, "Files:", 6) == 0) { snprintf(resumen[2], sizeof resumen[2], "%.250s", l); archivos = atoi(l + 6); }
	}
	if (f) fclose(f);
	/* si no acaba bien, lo último que ha dicho */
	if (c != 1 || !WIFEXITED(st)) {
		char cola[16][300];
		int nc = 0;
		f = fopen(sal, "r");
		while (f && fgets(l, sizeof l, f)) { l[strcspn(l, "\n")] = 0; snprintf(cola[nc % 16], sizeof cola[0], "%.290s", l); nc++; }
		if (f) fclose(f);
		for (int i = nc > 16 ? nc - 16 : 0; i < nc; i++) printf("  | %s\n", cola[i % 16]);
	}
	for (int i = 0; i < 3; i++) if (resumen[i][0]) printf("  %s\n", resumen[i]);
	printf("  test-js-runtime: %s %d, en %ld ms\n", !c ? "colgado, cerrado con la señal" : WIFEXITED(st) ? "acaba con el código" : "acaba con la señal",
	       !c ? SIGKILL : WIFEXITED(st) ? WEXITSTATUS(st) : WTERMSIG(st), ms);
	borra(base);
	/* bien: acaba, sin más fallos que los conocidos; con filtro, sin contar los conocidos que no se corren */
	int ok = c == 1 && WIFEXITED(st) && malos == 0 && pasan > 0 && archivos > 0 && (argc > 1 || sabidos == (int)N_CONOCIDOS) && fallan <= sabidos;
	if (ok) printf("MJS: todo bien (%d pruebas)\n", pasan);
	else printf("MJS: %d mal de %d\n", malos ? malos : 1, pasan + (malos ? malos : 1));
	return ok ? 0 : 1;
}
