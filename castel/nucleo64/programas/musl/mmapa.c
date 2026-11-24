/* Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * nucleo64/programas/musl/mmapa.c - Hispa-64, etapa 7 (docs/64/HISPA-64.md,
 * sección 13): mmap de un archivo (el propio programa: MAP_PRIVATE de
 * leer, con desplazamiento, de escribir sin cambiar el archivo, y
 * MAP_SHARED de solo leer; más allá del final, a cero; MAP_SHARED de
 * escribir, aún no), MAP_SHARED anónima entre un padre y su hijo (una
 * cuenta con atómicos), munmap y mprotect con SIGSEGV atendida,
 * MADV_DONTNEED, y 64 MB tocados. Estático (MMAPA) y dinámico (MMAPAD). */
#include "comun.h"
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>

static sigjmp_buf salto;
static volatile void *dir_;
static void al_fallo(int s, siginfo_t *i, void *uc) { (void)s; (void)uc; dir_ = i->si_addr; siglongjmp(salto, 1); }

static int toca(volatile char *p) { if (sigsetjmp(salto, 1)) return 0; (void)*p; return 1; }

int main(int argc, char **argv)
{
	int fd, i, st, ok;
	struct stat sb;
	unsigned char *todo, *m;
	volatile long *comp;
	char *p;
	pid_t h;
	struct sigaction a;
	(void)argc;

	printf("MMAPA: mmap de archivos y memoria compartida; %s\n", cómo());
	memset(&a, 0, sizeof a);
	a.sa_sigaction = al_fallo;
	a.sa_flags = SA_SIGINFO | SA_NODEFER;
	sigaction(SIGSEGV, &a, 0);
	/* el propio programa, leído y con mmap */
	fd = open(argv[0][0] == '/' ? argv[0] : "/MMAPA", O_RDONLY);
	if (fd < 0) fd = open("/MMAPA", O_RDONLY);
	MIRA(fd >= 0 && fstat(fd, &sb) == 0 && sb.st_size > 8192, "abrir el programa");
	todo = malloc(sb.st_size);
	MIRA(read(fd, todo, sb.st_size) == sb.st_size, "leerlo entero");
	m = mmap(0, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	MIRA(m != MAP_FAILED && !memcmp(m, todo, sb.st_size), "MAP_PRIVATE de leer: lo mismo que read");
	munmap(m, sb.st_size);
	m = mmap(0, 8192, PROT_READ, MAP_PRIVATE, fd, 4096);
	MIRA(m != MAP_FAILED && !memcmp(m, todo + 4096, 8192), "con desplazamiento (4096)");
	munmap(m, 8192);
	m = mmap(0, sb.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	MIRA(m != MAP_FAILED, "MAP_PRIVATE de escribir");
	m[100] ^= 0xFF;
	{
		unsigned char c;
		MIRA(pread(fd, &c, 1, 100) == 1 && c == todo[100] && m[100] == (todo[100] ^ 0xFF), "lo escrito es suyo: el archivo no cambia");
	}
	munmap(m, sb.st_size);
	m = mmap(0, sb.st_size, PROT_READ, MAP_SHARED, fd, 0);
	MIRA(m != MAP_FAILED && !memcmp(m, todo, sb.st_size), "MAP_SHARED de solo leer");
	munmap(m, sb.st_size);
	MIRA(mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0) == MAP_FAILED && errno == EACCES, "MAP_SHARED de escribir con O_RDONLY: EACCES");
	close(fd);
	printf("  mmap de %ld octetos del programa: MAP_PRIVATE (de leer, con desplazamiento, de escribir sin cambiar el archivo) y MAP_SHARED de solo leer\n", (long)sb.st_size);
	/* más allá del final del archivo: a cero */
	fd = open("/SALUDO.TXT", O_RDONLY);
	m = mmap(0, 3 * 4096, PROT_READ, MAP_PRIVATE, fd, 0);
	ok = m != MAP_FAILED && !memcmp(m, "Hola", 4);
	for (i = 100; ok && i < 3 * 4096; i++) if (m[i]) ok = 0;
	MIRA(ok, "un archivo corto: lo de después del final, a cero");
	munmap(m, 3 * 4096);
	close(fd);
	/* MAP_SHARED anónima: compartida con el hijo */
	comp = mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	MIRA(comp != MAP_FAILED, "MAP_SHARED | MAP_ANONYMOUS");
	comp[0] = 0;
	comp[1] = 0;
	h = fork();
	if (h == 0) {
		for (i = 0; i < 20000; i++) __atomic_add_fetch(&comp[0], 1, __ATOMIC_SEQ_CST);
		comp[1] = 12345;
		_exit(0);
	}
	for (i = 0; i < 20000; i++) __atomic_add_fetch(&comp[0], 1, __ATOMIC_SEQ_CST);
	waitpid(h, &st, 0);
	MIRA(comp[1] == 12345 && comp[0] == 40000, "lo que escribe el hijo lo ve el padre; la cuenta de los dos, exacta");
	printf("  MAP_SHARED anónima: padre e hijo suman a la vez con atómicos: %ld (exacto)\n", comp[0]);
	munmap((void *)comp, 4096);
	/* munmap, mprotect y MADV_DONTNEED */
	p = mmap(0, 3 * 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	memset(p, 'a', 3 * 4096);
	munmap(p + 4096, 4096);
	MIRA(toca(p) && !toca(p + 4096) && dir_ == p + 4096 && toca(p + 8192), "munmap de la del medio: SIGSEGV en ella, las otras siguen");
	mprotect(p, 4096, PROT_NONE);
	MIRA(!toca(p), "mprotect a PROT_NONE: SIGSEGV");
	mprotect(p, 4096, PROT_READ);
	MIRA(toca(p) && p[10] == 'a', "mprotect otra vez a leer: está lo de antes");
	madvise(p + 8192, 4096, MADV_DONTNEED);
	MIRA(p[8192] == 0, "MADV_DONTNEED: a cero");
	munmap(p, 3 * 4096);
	/* 64 MB tocados */
	p = mmap(0, 64 << 20, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	for (i = 0; i < (64 << 20); i += 4096) p[i] = (char)i;
	ok = 1;
	for (i = 0; i < (64 << 20); i += 4096) if (p[i] != (char)i) ok = 0;
	MIRA(ok && munmap(p, 64 << 20) == 0, "64 MB, tocados y soltados");
	printf("  munmap, mprotect (PROT_NONE y otra vez de leer) y MADV_DONTNEED, con SIGSEGV atendida; 64 MB tocados\n");
	free(todo);
	return acaba("MMAPA");
}
