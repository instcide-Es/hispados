/* Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * nucleo64/programas/ladybird/mak.c - Hispa-64, etapa 10 e (docs/64/HISPA-64.md,
 * sección 22): MAK corre TestAK (las pruebas de AK de Ladybird, de
 * terceros/ladybird/Tests/AK, compiladas con hazak.sh) con lo que dice a
 * /tmp/testak.txt (son unas 2.000 líneas: al puerto serie, solo el resumen)
 * y dice cuántos grupos y casos han salido bien, y los que no (hasta 8
 * líneas). TestAK está en la carpeta de MAK (/proc/self/exe).
 * «MAK todas»: TestAK entero (los 85 grupos: lanza un proceso por grupo; con
 * QEMU sin KVM, unos 200 s); «MAK» a secas: 12 grupos (TestAK --suite cada
 * uno), lo de cada arranque de las pruebas.
 * Al final, «MAK: todo bien (N pruebas)» (N, los casos) y código 0. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <spawn.h>
#include <time.h>
#include <sys/wait.h>

extern char **environ;

/* los 12 grupos de «MAK» a secas */
static const char *doce[] = { "TestVector", "TestHashMap", "TestFormat", "TestJSON", "TestUtf8View", "TestUtf16View", "TestByteString",
	"TestStringView", "TestOptional", "TestVariant", "TestLexicalPath", "TestFlyString" };

/* corre TestAK con argv a (sin el programa) y lo que dice, a sal (añadido) */
static int corre(const char *prog, const char *sal, const char *a1, const char *a2, int *st)
{
	posix_spawn_file_actions_t fa;
	posix_spawn_file_actions_init(&fa);
	posix_spawn_file_actions_addopen(&fa, 1, sal, O_WRONLY | O_CREAT | O_APPEND, 0644);
	posix_spawn_file_actions_adddup2(&fa, 1, 2);
	char *argv[] = { (char *)prog, (char *)a1, (char *)a2, NULL };
	pid_t pid;
	int e = posix_spawn(&pid, prog, &fa, NULL, argv, environ);
	posix_spawn_file_actions_destroy(&fa);
	if (e) return e;
	waitpid(pid, st, 0);
	return 0;
}

int main(int argc, char **argv)
{
	char carpeta[512], prog[600];
	ssize_t r = readlink("/proc/self/exe", carpeta, sizeof carpeta - 1);
	if (r > 0) { carpeta[r] = 0; char *b = strrchr(carpeta, '/'); if (b) *b = 0; } else strcpy(carpeta, ".");
	snprintf(prog, sizeof prog, "%s/TestAK", carpeta);
	setvbuf(stdout, NULL, _IOLBF, 0);
	printf("MAK: las pruebas de AK de Ladybird (TestAK: C++23 con GCC %d.%d y musl)%s\n", __GNUC__, __GNUC_MINOR__, argc > 1 && strcmp(argv[1], "todas") == 0 ? ", todas" : ", 12 grupos");
	const char *sal = "/tmp/testak.txt";
	int todas = argc > 1 && strcmp(argv[1], "todas") == 0;
	struct timespec t0, t1;
	int st = 0, e = 0;
	unlink(sal);
	clock_gettime(CLOCK_MONOTONIC, &t0);
	if (todas) e = corre(prog, sal, NULL, NULL, &st);
	else
		for (unsigned i = 0; i < sizeof doce / sizeof doce[0] && !e; i++) {
			int s1 = 0;
			e = corre(prog, sal, "--suite", doce[i], &s1);
			if (!e && !(WIFEXITED(s1) && WEXITSTATUS(s1) == 0) && !(st && !(WIFEXITED(st) && WEXITSTATUS(st) == 0))) st = s1;
		}
	if (e) { printf("  TestAK no se puede poner en marcha (%s)\nMAK: 1 mal de 1\n", strerror(e)); return 1; }
	clock_gettime(CLOCK_MONOTONIC, &t1);
	long ms = (t1.tv_sec - t0.tv_sec) * 1000 + (t1.tv_nsec - t0.tv_nsec) / 1000000;
	FILE *f = fopen(sal, "r");
	int casos = 0, grupos = 0, bien = 0, malas = 0, dichas = 0;
	char l[1024];
	while (f && fgets(l, sizeof l, f)) {
		int n;
		if (strncmp(l, "Completed test '", 16) == 0) casos++;
		else if (sscanf(l, "All %d tests passed.", &n) == 1 || sscanf(l, "All %d test passed.", &n) == 1) { grupos++; bien += n; }
		else if (strstr(l, "FAIL") || strstr(l, "failed") || strstr(l, "Failed") || strstr(l, "ASSERTION") || strstr(l, "VERIFICATION")) {
			malas++;
			if (dichas++ < 8) { l[strcspn(l, "\n")] = 0; printf("  TestAK: %.200s\n", l); }
		}
	}
	int ok = WIFEXITED(st) && WEXITSTATUS(st) == 0 && malas == 0 && casos > 0 && bien == casos;
	/* si no sale bien, lo último que ha dicho (hasta 8 líneas) */
	if (!ok && f) {
		char u[8][256];
		int n = 0;
		rewind(f);
		while (fgets(l, sizeof l, f)) { l[strcspn(l, "\n")] = 0; snprintf(u[n % 8], sizeof u[0], "%.250s", l); n++; }
		for (int i = n > 8 ? n - 8 : 0; i < n; i++) printf("  TestAK dice: %s\n", u[i % 8]);
	}
	if (f) fclose(f);
	unlink(sal);
	printf("  %d grupos, %d casos: %s\n", grupos, casos, ok ? "todos bien" : "no todos bien");
	printf("  TestAK acaba con %s %d, en %ld ms\n", WIFEXITED(st) ? "el código" : "la señal", WIFEXITED(st) ? WEXITSTATUS(st) : WTERMSIG(st), ms);
	if (ok) printf("MAK: todo bien (%d pruebas)\n", casos);
	else printf("MAK: %d mal de %d\n", malas ? malas : 1, casos);
	return ok ? 0 : 1;
}
