/* Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * nucleo64/programas/musl/mprocesos.c - Hispa-64, etapa 7 (docs/64/
 * HISPA-64.md, sección 13): procesos con musl: fork con copiar al escribir
 * (16 MB: el hijo cambia lo suyo, el padre no lo ve, y al revés), execve
 * con la salida por una tubería (dup2), posix_spawn (clone con CLONE_VM y
 * CLONE_VFORK), wait de varios, tuberías (100 KB, el fin de archivo, poll,
 * O_CLOEXEC al hacer execve), fork con varios hilos y execve con varios
 * hilos. Estático (MPROCESOS) y dinámico (MPROCESOSD). */
#include "comun.h"
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <pthread.h>
#include <sys/wait.h>

extern char **environ;
#define GRANDE (16 << 20)
static int global = 10;

static long lee_todo(int fd, char *t, long máx)
{
	long n = 0, r;
	while (n < máx - 1 && (r = read(fd, t + n, máx - 1 - n)) > 0) n += r;
	t[n] = 0;
	return n;
}

static void *dormilón(void *a) { (void)a; for (;;) sleep(10); return 0; }

int main(int argc, char **argv)
{
	unsigned char *g;
	pid_t h, hs[5];
	int st, i, ok, t[2], u[2];
	long n, suma;
	char buf[4096], *texto;
	char *args[] = { "MHOLA", "-q", 0 };
	posix_spawn_file_actions_t fa;
	struct pollfd pf;
	pthread_t th;

	/* (los hijos con execve: MPROCESOS cierra-fd N, mira que N no esté) */
	if (argc == 3 && !strcmp(argv[1], "cierra-fd")) return fcntl(atoi(argv[2]), F_GETFD) == -1 && errno == EBADF ? 0 : 1;
	printf("MPROCESOS: fork, execve, wait, tuberías y posix_spawn; %s\n", cómo());
	/* fork con copiar al escribir */
	g = malloc(GRANDE);
	for (i = 0; i < GRANDE; i += 4096) g[i] = (unsigned char)(i >> 12);
	h = fork();
	if (h == 0) {
		ok = getppid() > 1;
		for (i = 0; i < GRANDE; i += 4096) if (g[i] != (unsigned char)(i >> 12)) ok = 0;
		for (i = 0; i < GRANDE; i += 4096) g[i] = 0xEE;
		global = 99;
		_exit(ok ? 7 : 1);
	}
	MIRA(h > 0, "fork");
	MIRA(waitpid(h, &st, 0) == h && WIFEXITED(st) && WEXITSTATUS(st) == 7, "el hijo ve la memoria del padre y acaba con 7");
	ok = global == 10;
	for (i = 0; i < GRANDE; i += 4096) if (g[i] != (unsigned char)(i >> 12)) ok = 0;
	MIRA(ok, "copiar al escribir: lo que escribe el hijo, el padre no lo ve");
	h = fork();
	if (h == 0) { usleep(30000); _exit(g[4096 * 5] == 5 ? 0 : 1); }
	g[4096 * 5] = 77;
	MIRA(waitpid(h, &st, 0) == h && WEXITSTATUS(st) == 0, "lo que escribe el padre después, el hijo no lo ve");
	free(g);
	printf("  fork con 16 MB: el hijo los lee y los cambia (copiar al escribir); el padre no ve sus cambios, ni el hijo los del padre\n");
	/* una tubería: 100 KB del padre al hijo, la suma de vuelta */
	pipe(t);
	pipe(u);
	h = fork();
	if (h == 0) {
		close(t[1]);
		close(u[0]);
		suma = 0;
		while ((n = read(t[0], buf, sizeof buf)) > 0) for (i = 0; i < n; i++) suma += (unsigned char)buf[i];
		write(u[1], &suma, sizeof suma);
		_exit(0);
	}
	close(t[0]);
	close(u[1]);
	suma = 0;
	for (i = 0; i < 100000; i++) { buf[i % 4096] = (char)(i * 31); suma += (unsigned char)(i * 31); if (i % 4096 == 4095) write(t[1], buf, 4096); }
	write(t[1], buf, 100000 % 4096);
	close(t[1]);
	n = 0;
	MIRA(read(u[0], &n, sizeof n) == sizeof n && n == suma, "100 KB por una tubería, la suma de vuelta por otra");
	MIRA(read(u[0], buf, 1) == 0, "y luego, fin de archivo");
	close(u[0]);
	waitpid(h, &st, 0);
	printf("  100 KB por una tubería (y la suma de vuelta por otra, y el fin de archivo)\n");
	/* poll de una tubería */
	pipe(t);
	pf.fd = t[0];
	pf.events = POLLIN;
	MIRA(poll(&pf, 1, 50) == 0, "poll de una tubería vacía: 0 al acabar el plazo");
	write(t[1], "x", 1);
	MIRA(poll(&pf, 1, 50) == 1 && (pf.revents & POLLIN), "poll: POLLIN cuando hay algo");
	close(t[0]);
	close(t[1]);
	/* fork, dup2 y execve: la salida de MHOLA por una tubería */
	texto = malloc(65536);
	pipe(t);
	h = fork();
	if (h == 0) {
		dup2(t[1], 1);
		close(t[0]);
		close(t[1]);
		execve("/MHOLA", args, environ);
		_exit(127);
	}
	close(t[1]);
	n = lee_todo(t[0], texto, 65536);
	close(t[0]);
	MIRA(waitpid(h, &st, 0) == h && WIFEXITED(st) && WEXITSTATUS(st) == 0, "execve de /MHOLA: acaba con 0");
	MIRA(strstr(texto, "MHOLA (por una tubería): estático, argc 2") != 0, "lo que escribe MHOLA, por la tubería");
	printf("  fork, dup2 y execve de /MHOLA: «%.*s»\n", (int)strcspn(texto, "\n"), texto);
	/* posix_spawn (clone con CLONE_VM y CLONE_VFORK) */
	pipe(t);
	posix_spawn_file_actions_init(&fa);
	posix_spawn_file_actions_adddup2(&fa, t[1], 1);
	posix_spawn_file_actions_addclose(&fa, t[0]);
	MIRA(posix_spawn(&h, "/MHOLA", &fa, 0, args, environ) == 0, "posix_spawn");
	close(t[1]);
	n = lee_todo(t[0], texto, 65536);
	close(t[0]);
	MIRA(waitpid(h, &st, 0) == h && WIFEXITED(st) && WEXITSTATUS(st) == 0 && strstr(texto, "MHOLA (por una tubería)"), "posix_spawn de /MHOLA, con su salida");
	MIRA(posix_spawn(&h, "/NOESTA", 0, 0, args, environ) == ENOENT, "posix_spawn de uno que no está: ENOENT");
	printf("  posix_spawn (clone con CLONE_VM y CLONE_VFORK) de /MHOLA, con su salida por una tubería; y de uno que no está, ENOENT\n");
	/* O_CLOEXEC al hacer execve */
	pipe2(t, O_CLOEXEC);
	h = fork();
	if (h == 0) {
		char n_[12], *a2[] = { "MPROCESOS", "cierra-fd", n_, 0 };
		snprintf(n_, sizeof n_, "%d", t[0]);
		execve("/MPROCESOS", a2, environ);
		execve("/MPROCESOSD", a2, environ);
		_exit(127);
	}
	MIRA(waitpid(h, &st, 0) == h && WEXITSTATUS(st) == 0, "pipe2 con O_CLOEXEC: cerrado en el programa nuevo");
	close(t[0]);
	close(t[1]);
	/* cinco hijos, y wait de todos */
	for (i = 0; i < 5; i++) { hs[i] = fork(); if (hs[i] == 0) { usleep(10000 * (5 - i)); _exit(20 + i); } }
	suma = 0;
	for (i = 0; i < 5; i++) { h = wait(&st); if (h > 0 && WIFEXITED(st)) suma += WEXITSTATUS(st); }
	MIRA(suma == 20 + 21 + 22 + 23 + 24 && wait(&st) == -1 && errno == ECHILD, "cinco hijos con wait, y luego ECHILD");
	/* fork con varios hilos: el hijo, solo con el que lo hace */
	pthread_create(&th, 0, dormilón, 0);
	h = fork();
	if (h == 0) { printf("  (el hijo de un proceso con 2 hilos: sigue con uno)\n"); fflush(stdout); _exit(5); }
	MIRA(waitpid(h, &st, 0) == h && WEXITSTATUS(st) == 5, "fork con 2 hilos");
	/* execve con varios hilos: los demás acaban */
	h = fork();
	if (h == 0) {
		pthread_t a, b;
		pthread_create(&a, 0, dormilón, 0);
		pthread_create(&b, 0, dormilón, 0);
		usleep(10000);
		pipe(t);
		dup2(t[1], 1);
		execve("/MHOLA", args, environ);
		_exit(127);
	}
	MIRA(waitpid(h, &st, 0) == h && WIFEXITED(st) && WEXITSTATUS(st) == 0, "execve con 3 hilos: los otros dos acaban");
	printf("  fork y execve con varios hilos; cinco hijos con wait\n");
	free(texto);
	return acaba("MPROCESOS");
}
