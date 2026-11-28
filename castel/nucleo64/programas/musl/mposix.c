/* Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * nucleo64/programas/musl/mposix.c - Hispa-64, etapa 10 g (docs/64/HISPA-64.md,
 * sección 24): MPOSIX, lo de POSIX que le faltaba al núcleo para Ladybird:
 *   - los enlaces simbólicos (en /tmp): symlink, readlink, lstat (S_IFLNK),
 *     stat y open a través de ellos (también de una carpeta, y por en medio
 *     de una ruta), getdents64 (DT_LNK), EEXIST, EINVAL, ELOOP (dos que se
 *     apuntan; O_NOFOLLOW), O_CREAT con O_EXCL, uno colgado (ENOENT al
 *     seguirlo), unlink, rename (del enlace y de la carpeta que los tiene),
 *     rmdir (ENOTEMPTY) y symlinkat;
 *   - más descriptores: RLIMIT_NOFILE (getrlimit, setrlimit y prlimit), 20.000
 *     abiertos a la vez (y heredados al bifurcar), EMFILE con el límite bajo,
 *     dup2 por encima del límite (EBADF), poll del más alto;
 *   - el otro lado cerrado de un socket de Unix (poll: POLLIN y POLLHUP, con
 *     POLLRDHUP; recv: lo que quedaba y después 0; send: EPIPE; epoll: EPOLLIN,
 *     EPOLLHUP y EPOLLRDHUP), como LibIPC lo espera;
 *   - un memfd de 65 MB como los de LibIPC (ftruncate, write, los sellos,
 *     fstat, SEEK_HOLE, mmap compartido; de 4 MB si hay poca memoria);
 *   - madvise(MADV_DONTNEED): lo privado de un archivo vuelve a ser lo del
 *     archivo; lo anónimo y privado, ceros; lo anónimo compartido, lo suyo;
 *   - mremap: achicar, crecer en su sitio, EFAULT (sin nada puesto), EINVAL;
 *     y pthread_getattr_np del hilo principal (que mide la pila con mremap);
 *   - F_SETLKW con un abrazo mortal entre dos procesos: EDEADLK;
 *   - sched_getcpu (en Hispa-64 y en Linux, por el vDSO: __vdso_getcpu);
 *   - flock, statfs y fstatfs, chmod y fchmod, /proc/self/status (lo que
 *     pidió después el navegador);
 *   - [etapa 10 h] mmap MAP_SHARED de escribir de un archivo (lo que pide
 *     sqlite en modo WAL): dos mmap del mismo archivo se ven, write y read
 *     con lo mapeado, un hijo que escribe (lo ve el padre), msync (y sus
 *     errores), lo de más allá del final, ftruncate (a cero al crecer), lo
 *     escrito en el archivo al soltarlo, EACCES de solo leer;
 *   - [etapa 10 i] pidfd (pidfd_open y pidfd_send_signal: el Hub de Ladybird
 *     vigila así a la interfaz): sin acabar, poll no lo da listo; al acabar el
 *     hijo, poll (sin plazo) y epoll despiertan enseguida (sin mirar de vez en
 *     cuando); de un zombi, listo ya; read, EINVAL; O_CLOEXEC; los errores
 *     (EINVAL, ESRCH, EBADF); una señal por el pidfd; después de wait, ESRCH.
 * Al final, «MPOSIX: todo bien (N pruebas)» y código 0. Corre igual en Linux. */
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <poll.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/sysinfo.h>
#include <sys/file.h>
#include <sys/vfs.h>
#include <sys/wait.h>
#include "comun.h"

#define D "/tmp/mposix"
#define PIDFD_NONBLOCK_ O_NONBLOCK

static void borra_todo(const char *c)
{
	DIR *d = opendir(c);
	struct dirent *e;
	char r[512];
	struct stat st;
	if (!d) return;
	while ((e = readdir(d))) {
		if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
		snprintf(r, sizeof r, "%s/%s", c, e->d_name);
		if (!lstat(r, &st) && S_ISDIR(st.st_mode)) borra_todo(r);
		else unlink(r);
	}
	closedir(d);
	rmdir(c);
}

static int escribe_archivo(const char *r, const char *t)
{
	int fd = open(r, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0) return -errno;
	int n = write(fd, t, strlen(t));
	close(fd);
	return n == (int)strlen(t) ? 0 : -1;
}

static int lee_archivo(const char *r, char *b, int n)
{
	int fd = open(r, O_RDONLY);
	if (fd < 0) return -errno;
	int k = read(fd, b, n - 1);
	close(fd);
	if (k < 0) return -errno;
	b[k] = 0;
	return k;
}


/* lo que hay en la carpeta (sin «.» ni «..»), ordenado, con su tipo: «d_nombre l_otro f_x» */
static void lista(const char *c, char *sal, int n)
{
	char nombres[32][272];
	int k = 0;
	DIR *d = opendir(c);
	struct dirent *e;
	sal[0] = 0;
	if (!d) { snprintf(sal, n, "(error %d)", errno); return; }
	while ((e = readdir(d)) && k < 32) {
		if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
		snprintf(nombres[k++], 272, "%c_%s", e->d_type == DT_LNK ? 'l' : e->d_type == DT_DIR ? 'd' : e->d_type == DT_REG ? 'f' : '?', e->d_name);
	}
	closedir(d);
	for (int i = 0; i < k; i++)
		for (int j = i + 1; j < k; j++)
			if (strcmp(nombres[i] + 2, nombres[j] + 2) > 0) { char t[272]; memcpy(t, nombres[i], 272); memcpy(nombres[i], nombres[j], 272); memcpy(nombres[j], t, 272); }
	for (int i = 0; i < k; i++) {
		strncat(sal, nombres[i], n - strlen(sal) - 2);
		if (i + 1 < k) strncat(sal, " ", n - strlen(sal) - 1);
	}
}

static void enlaces(void)
{
	char b[256], l[512];
	struct stat st;
	ssize_t r;
	borra_todo(D);
	MIRA(mkdir(D, 0755) == 0 && mkdir(D "/d", 0755) == 0, "mkdir de " D);
	MIRA(escribe_archivo(D "/f", "hola") == 0 && escribe_archivo(D "/d/x", "equis") == 0, "los archivos de " D);
	MIRA(symlink("f", D "/l") == 0, "symlink de un archivo");
	r = readlink(D "/l", b, sizeof b);
	MIRA(r == 1 && b[0] == 'f', "readlink");
	MIRA(lstat(D "/l", &st) == 0 && S_ISLNK(st.st_mode) && st.st_size == 1, "lstat: S_IFLNK, el largo de lo que dice");
	MIRA(stat(D "/l", &st) == 0 && S_ISREG(st.st_mode) && st.st_size == 4, "stat a través del enlace");
	MIRA(fstatat(AT_FDCWD, D "/l", &st, AT_SYMLINK_NOFOLLOW) == 0 && S_ISLNK(st.st_mode), "fstatat con AT_SYMLINK_NOFOLLOW");
	MIRA(lee_archivo(D "/l", b, sizeof b) == 4 && !strcmp(b, "hola"), "open y read a través del enlace");
	MIRA(symlink(D "/d", D "/ld") == 0, "symlink de una carpeta (ruta absoluta)");
	MIRA(lee_archivo(D "/ld/x", b, sizeof b) == 5 && !strcmp(b, "equis"), "una ruta con el enlace en medio");
	MIRA(escribe_archivo(D "/ld/y", "y") == 0 && lee_archivo(D "/d/y", b, sizeof b) == 1, "crear a través del enlace de la carpeta");
	lista(D "/ld", l, sizeof l);
	MIRA(!strcmp(l, "f_x f_y"), "opendir y readdir a través del enlace");
	lista(D, l, sizeof l);
	printf("  " D ": %s\n", l);
	MIRA(!strcmp(l, "d_d f_f l_l l_ld"), "getdents64: los enlaces, DT_LNK");
	errno = 0;
	MIRA(readlink(D "/f", b, sizeof b) < 0 && errno == EINVAL, "readlink de un archivo: EINVAL");
	errno = 0;
	MIRA(readlink(D "/nada", b, sizeof b) < 0 && errno == ENOENT, "readlink de lo que no hay: ENOENT");
	errno = 0;
	MIRA(symlink("otro", D "/l") < 0 && errno == EEXIST, "symlink encima de un enlace: EEXIST");
	errno = 0;
	MIRA(symlink("otro", D "/f") < 0 && errno == EEXIST, "symlink encima de un archivo: EEXIST");
	errno = 0;
	MIRA(mkdir(D "/l", 0755) < 0 && errno == EEXIST, "mkdir encima de un enlace: EEXIST");
	MIRA(symlink("b", D "/a") == 0 && symlink("a", D "/b") == 0, "dos enlaces que se apuntan");
	errno = 0;
	MIRA(open(D "/a", O_RDONLY) < 0 && errno == ELOOP, "abrirlos: ELOOP");
	errno = 0;
	MIRA(stat(D "/a", &st) < 0 && errno == ELOOP, "stat: ELOOP");
	errno = 0;
	MIRA(open(D "/l", O_RDONLY | O_NOFOLLOW) < 0 && errno == ELOOP, "O_NOFOLLOW: ELOOP");
	errno = 0;
	MIRA(open(D "/l", O_WRONLY | O_CREAT | O_EXCL, 0644) < 0 && errno == EEXIST, "O_CREAT y O_EXCL en un enlace: EEXIST");
	MIRA(symlink("no-esta", D "/colgado") == 0, "un enlace colgado");
	errno = 0;
	MIRA(stat(D "/colgado", &st) < 0 && errno == ENOENT && lstat(D "/colgado", &st) == 0, "colgado: stat ENOENT, lstat bien");
	MIRA(rename(D "/l", D "/l2") == 0 && readlink(D "/l2", b, sizeof b) == 1 && lstat(D "/l", &st) < 0, "rename de un enlace");
	MIRA(rename(D "/f", D "/colgado") == 0 && lstat(D "/colgado", &st) == 0 && S_ISREG(st.st_mode) && lee_archivo(D "/l2", b, sizeof b) < 0, "rename de un archivo encima de un enlace");
	MIRA(rename(D "/colgado", D "/f") == 0 && lee_archivo(D "/l2", b, sizeof b) == 4, "y de vuelta");
	MIRA(mkdir(D "/e", 0755) == 0 && symlink("../f", D "/e/sube") == 0, "un enlace dentro de una carpeta");
	errno = 0;
	MIRA(rmdir(D "/e") < 0 && errno == ENOTEMPTY, "rmdir de la carpeta con el enlace: ENOTEMPTY");
	MIRA(rename(D "/e", D "/e2") == 0 && readlink(D "/e2/sube", b, sizeof b) == 4 && !memcmp(b, "../f", 4), "rename de la carpeta: el enlace va con ella");
	MIRA(lee_archivo(D "/e2/sube", b, sizeof b) == 4, "un enlace relativo con «..»");
	errno = 0;
	MIRA(rmdir(D "/ld") < 0 && errno == ENOTDIR, "rmdir de un enlace: ENOTDIR");
	MIRA(unlink(D "/e2/sube") == 0 && rmdir(D "/e2") == 0, "unlink del enlace y rmdir");
	int dfd = open(D, O_RDONLY | O_DIRECTORY);
	MIRA(dfd >= 0 && symlinkat("d/x", dfd, "lat") == 0 && readlinkat(dfd, "lat", b, sizeof b) == 3, "symlinkat y readlinkat");
	if (dfd >= 0) close(dfd);
	MIRA(chdir(D "/ld") == 0 && lee_archivo("x", b, sizeof b) == 5 && getcwd(b, sizeof b) && !strcmp(b, D "/d"), "chdir a través del enlace: getcwd, lo de verdad");
	chdir("/");
	borra_todo(D);
	MIRA(lstat(D, &st) < 0, "todo borrado");
}

static void descriptores(void)
{
	struct rlimit lim, viejo;
	int r, i, n, alto = -1, base;
	static int fds[20000];
	MIRA(getrlimit(RLIMIT_NOFILE, &viejo) == 0 && viejo.rlim_cur <= viejo.rlim_max && viejo.rlim_cur >= 256, "getrlimit(RLIMIT_NOFILE)");
	lim = viejo;
	lim.rlim_cur = viejo.rlim_max < 65536 ? viejo.rlim_max : 65536;
	MIRA(setrlimit(RLIMIT_NOFILE, &lim) == 0, "setrlimit: hasta 65536 (o el máximo)");
	MIRA(syscall(SYS_prlimit64, 0, RLIMIT_NOFILE, NULL, &lim) == 0 && lim.rlim_cur >= 25000, "prlimit64: el nuevo");
	n = 0;
	base = open("/dev/null", O_RDONLY);
	for (i = 0; i < 20000; i++) {
		r = open("/dev/null", O_RDONLY);
		if (r < 0) break;
		fds[n++] = r;
		if (r > alto) alto = r;
	}
	MIRA(n == 20000 && alto >= 20000, "20000 descriptores abiertos a la vez");
	printf("  20000 descriptores a la vez: %s\n", n == 20000 && alto >= 20000 ? "sí" : "no");
	struct pollfd pf = { .fd = alto, .events = POLLIN };
	MIRA(poll(&pf, 1, 0) == 1 && (pf.revents & POLLIN), "poll del más alto");
	MIRA(dup2(base, 30000) == 30000 && fcntl(30000, F_GETFD) == 0 && close(30000) == 0, "dup2 al 30000");
	pid_t h = fork();
	if (h == 0) _exit(fcntl(alto, F_GETFD) == 0 && fcntl(fds[n / 2], F_GETFD) == 0 ? 0 : 1);
	int st = 0;
	waitpid(h, &st, 0);
	MIRA(WIFEXITED(st) && WEXITSTATUS(st) == 0, "heredados al bifurcar");
	for (i = 0; i < n; i++) close(fds[i]);
	close(base);
	lim.rlim_cur = 64;
	MIRA(setrlimit(RLIMIT_NOFILE, &lim) == 0, "setrlimit: 64");
	n = 0;
	errno = 0;
	for (i = 0; i < 100; i++) {
		r = open("/dev/null", O_RDONLY);
		if (r < 0) break;
		fds[n++] = r;
	}
	MIRA(errno == EMFILE && n > 0 && fds[n - 1] == 63, "EMFILE en el 64");
	errno = 0;
	MIRA(dup2(0, 100) < 0 && errno == EBADF, "dup2 por encima del límite: EBADF");
	errno = 0;
	MIRA(fcntl(0, F_DUPFD, 64) < 0 && errno == EINVAL, "F_DUPFD por encima del límite: EINVAL");
	for (i = 0; i < n; i++) close(fds[i]);
	lim.rlim_cur = lim.rlim_max + 1;
	if (lim.rlim_max != RLIM_INFINITY) {
		errno = 0;
		MIRA(setrlimit(RLIMIT_NOFILE, &lim) < 0 && errno == EINVAL, "setrlimit por encima del máximo: EINVAL");
	}
	MIRA(setrlimit(RLIMIT_NOFILE, &viejo) == 0, "setrlimit: el de antes");
}

static void par_cerrado(void)
{
	int sv[2];
	char b[16];
	struct pollfd pf;
	MIRA(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0, "socketpair");
	MIRA(write(sv[1], "xy", 2) == 2, "lo que manda antes de cerrar");
	close(sv[1]);
	pf.fd = sv[0];
	pf.events = POLLIN | POLLRDHUP;
	MIRA(poll(&pf, 1, 1000) == 1, "poll con el otro cerrado");
	printf("  el otro lado cerrado: poll, 0x%x", pf.revents);
	MIRA(pf.revents == (POLLIN | POLLHUP | POLLRDHUP), "POLLIN, POLLHUP y POLLRDHUP");
	pf.events = POLLOUT;
	poll(&pf, 1, 0);
	printf("; con POLLOUT, 0x%x", pf.revents);
	MIRA(pf.revents == (POLLOUT | POLLHUP), "con POLLOUT: POLLOUT y POLLHUP");
	int ep = epoll_create1(0);
	struct epoll_event ev = { .events = EPOLLIN | EPOLLRDHUP }, out;
	MIRA(epoll_ctl(ep, EPOLL_CTL_ADD, sv[0], &ev) == 0 && epoll_wait(ep, &out, 1, 1000) == 1, "epoll con el otro cerrado");
	printf("; epoll, 0x%x", out.events);
	MIRA(out.events == (EPOLLIN | EPOLLHUP | EPOLLRDHUP), "EPOLLIN, EPOLLHUP y EPOLLRDHUP");
	close(ep);
	MIRA(recv(sv[0], b, sizeof b, MSG_DONTWAIT) == 2 && recv(sv[0], b, sizeof b, MSG_DONTWAIT) == 0, "recv: lo que quedaba y después 0");
	errno = 0;
	MIRA(send(sv[0], "z", 1, MSG_NOSIGNAL) < 0 && errno == EPIPE, "send: EPIPE");
	errno = 0;
	MIRA(recv(sv[0], b, sizeof b, 0) == 0, "recv que espera: 0, sin esperar");
	printf("; recv, 0; send, EPIPE\n");
	close(sv[0]);
	/* sin nada que leer */
	MIRA(socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0, sv) == 0, "socketpair SOCK_NONBLOCK");
	close(sv[1]);
	pf.fd = sv[0];
	pf.events = POLLIN;
	MIRA(poll(&pf, 1, 1000) == 1 && pf.revents == (POLLIN | POLLHUP), "sin nada que leer: POLLIN y POLLHUP");
	MIRA(read(sv[0], b, sizeof b) == 0, "read: 0");
	close(sv[0]);
}

static void memfd_grande(void)
{
	/* 65 MB (como el de TestConnection de LibIPC) si hay memoria de sobra; si
	 * no (los arranques de 128 MB), 4 MB: lo que se mira es lo mismo */
	struct sysinfo si;
	size_t tam = 65u << 20;
	if (sysinfo(&si) == 0 && (unsigned long long)si.freeram * si.mem_unit < (400ull << 20)) tam = 4u << 20;
	char *a = malloc(tam);
	MIRA(a != NULL, "la memoria para el memfd");
	if (!a) return;
	for (size_t i = 0; i < tam; i++) a[i] = (char)(i * 7 + (i >> 12));
	int fd = memfd_create("", MFD_CLOEXEC | MFD_ALLOW_SEALING);
	MIRA(fd >= 0, "memfd_create con MFD_ALLOW_SEALING");
	MIRA(ftruncate(fd, tam) == 0, "ftruncate");
	size_t h = 0;
	while (h < tam) {
		ssize_t k = write(fd, a + h, tam - h);
		if (k <= 0) break;
		h += k;
	}
	MIRA(h == tam, "write de todo");
	MIRA(fcntl(fd, F_ADD_SEALS, F_SEAL_WRITE | F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_SEAL) == 0, "F_ADD_SEALS");
	int s = fcntl(fd, F_GET_SEALS);
	MIRA(s == (F_SEAL_WRITE | F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_SEAL), "F_GET_SEALS");
	struct stat st;
	MIRA(fstat(fd, &st) == 0 && (size_t)st.st_size == tam && st.st_blksize == 4096, "fstat: el tamaño y st_blksize");
	off_t hueco = lseek(fd, 0, SEEK_HOLE);
	MIRA(hueco == (off_t)tam, "lseek con SEEK_HOLE: el final");
	MIRA(lseek(fd, 4096, SEEK_DATA) == 4096, "lseek con SEEK_DATA");
	char *m = mmap(NULL, tam, PROT_READ, MAP_SHARED, fd, 0);
	MIRA(m != MAP_FAILED && !memcmp(m, a, tam), "mmap compartido, igual que lo escrito");
	printf("  un memfd grande como los de LibIPC (los sellos, SEEK_HOLE, mmap compartido): %s\n", m != MAP_FAILED && !memcmp(m, a, tam) ? "bien" : "mal");
	if (m != MAP_FAILED) munmap(m, tam);
	close(fd);
	free(a);
}

static void dontneed(void)
{
	char r[] = "/tmp/mposix.map";
	char t[8192];
	for (int i = 0; i < 8192; i++) t[i] = 'a' + i % 26;
	int fd = open(r, O_RDWR | O_CREAT | O_TRUNC, 0644);
	MIRA(fd >= 0 && write(fd, t, 8192) == 8192, "el archivo para madvise");
	char *m = mmap(NULL, 8192, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	MIRA(m != MAP_FAILED, "mmap privado de escribir");
	if (m == MAP_FAILED) return;
	memset(m, 'Z', 8192);
	MIRA(madvise(m + 4096, 4096, MADV_DONTNEED) == 0, "madvise(MADV_DONTNEED) de lo de un archivo");
	MIRA(m[0] == 'Z' && m[4096] == t[4096] && !memcmp(m + 4096, t + 4096, 4096), "lo privado, otra vez lo del archivo (y lo de antes, como estaba)");
	MIRA(madvise(m, 8192, MADV_DONTNEED) == 0 && !memcmp(m, t, 8192), "las dos páginas, del archivo");
	munmap(m, 8192);
	close(fd);
	unlink(r);
	char *an = mmap(NULL, 8192, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	memset(an, 7, 8192);
	MIRA(madvise(an, 8192, MADV_DONTNEED) == 0 && an[0] == 0 && an[8191] == 0, "lo anónimo y privado: ceros");
	munmap(an, 8192);
	char *co = mmap(NULL, 8192, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	memset(co, 9, 8192);
	MIRA(madvise(co, 8192, MADV_DONTNEED) == 0 && co[0] == 9 && co[8191] == 9, "lo anónimo y compartido: lo suyo");
	munmap(co, 8192);
	printf("  madvise(MADV_DONTNEED): de un archivo, lo del archivo; anónimo, ceros; compartido, lo suyo\n");
}

static void remapeo(void)
{
	long pg = 4096;
	char *a = mmap(NULL, 3 * pg, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	MIRA(a != MAP_FAILED, "mmap de 3 páginas");
	a[0] = 1; a[pg] = 2;
	MIRA(mremap(a, 3 * pg, 2 * pg, 0) == a && a[pg] == 2, "mremap: achicar");
	MIRA(munmap(a + pg, pg) == 0, "munmap de la segunda");
	char *b = mremap(a, pg, 3 * pg, 0);
	MIRA(b == a && a[0] == 1 && a[pg] == 0 && a[2 * pg] == 0, "mremap: crecer en su sitio");
	errno = 0;
	MIRA(mremap(a + 64 * pg, pg, 2 * pg, 0) == MAP_FAILED && errno == EFAULT, "mremap sin nada puesto: EFAULT");
	errno = 0;
	MIRA(mremap(a + 1, pg, 2 * pg, 0) == MAP_FAILED && errno == EINVAL, "mremap mal alineado: EINVAL");
	errno = 0;
	MIRA(mremap(a, pg, 2 * pg, 0x80) == MAP_FAILED && errno == EINVAL, "mremap con marcas que no hay: EINVAL");
	char *c = mremap(a, 3 * pg, 200 * pg, MREMAP_MAYMOVE);
	MIRA(c != MAP_FAILED || errno == ENOMEM, "mremap con MREMAP_MAYMOVE (o ENOMEM)");
	munmap(c != MAP_FAILED ? c : a, c != MAP_FAILED ? 200 * pg : 3 * pg);
	pthread_attr_t at;
	size_t tam = 0;
	void *dir = NULL;
	MIRA(pthread_getattr_np(pthread_self(), &at) == 0 && pthread_attr_getstack(&at, &dir, &tam) == 0 && tam >= 8192 && (char *)&at > (char *)dir && (char *)&at < (char *)dir + tam,
	     "pthread_getattr_np del hilo principal: la pila (con mremap)");
	printf("  mremap (achicar, crecer en su sitio, EFAULT, EINVAL) y pthread_getattr_np del hilo principal: %s\n", tam >= 8192 ? "bien" : "mal");
}

static int cerrojo(int fd, int orden, off_t ini)
{
	struct flock f = { .l_type = F_WRLCK, .l_whence = SEEK_SET, .l_start = ini, .l_len = 1 };
	return fcntl(fd, orden, &f) == 0 ? 0 : -errno;
}

static void abrazo(void)
{
	int fd = open("/tmp/mposix.cand", O_RDWR | O_CREAT | O_TRUNC, 0644);
	int t1[2], t2[2];
	char c;
	MIRA(fd >= 0 && write(fd, "ab", 2) == 2 && pipe(t1) == 0 && pipe(t2) == 0, "el archivo y las tuberías");
	MIRA(cerrojo(fd, F_SETLK, 0) == 0, "el padre, el octeto 0");
	pid_t h = fork();
	if (h == 0) {
		int r = cerrojo(fd, F_SETLK, 1);          /* el hijo, el 1 */
		write(t1[1], "1", 1);
		read(t2[0], &c, 1);                        /* el padre ya espera por el 1 */
		usleep(100000);
		r = cerrojo(fd, F_SETLKW, 0);              /* el 0: el padre espera por el 1, del hijo: abrazo */
		_exit(r == -EDEADLK ? 0 : 1);
	}
	read(t1[0], &c, 1);
	write(t2[1], "2", 1);
	int r = cerrojo(fd, F_SETLKW, 1);              /* espera al hijo: le dan EDEADLK, acaba y suelta */
	int st = 0;
	waitpid(h, &st, 0);
	MIRA(WIFEXITED(st) && WEXITSTATUS(st) == 0, "F_SETLKW con un abrazo mortal: EDEADLK al que lo cierra");
	MIRA(r == 0, "y el otro lo consigue cuando el primero acaba");
	printf("  F_SETLKW con un abrazo mortal entre dos procesos: %s\n", WIFEXITED(st) && WEXITSTATUS(st) == 0 ? "EDEADLK" : "otra cosa");
	close(fd);
	unlink("/tmp/mposix.cand");
}

/* ¿tiene el vDSO __vdso_getcpu? (la tabla de símbolos del ELF de AT_SYSINFO_EHDR) */
static int vdso_tiene(const char *n)
{
	Elf64_Ehdr *e = (void *)getauxval(AT_SYSINFO_EHDR);
	if (!e) return 0;
	Elf64_Phdr *ph = (void *)((char *)e + e->e_phoff);
	uintptr_t base = (uintptr_t)-1;
	Elf64_Dyn *dyn = NULL;
	for (int i = 0; i < e->e_phnum; i++) {
		if (ph[i].p_type == PT_LOAD && base == (uintptr_t)-1) base = (uintptr_t)e + ph[i].p_offset - ph[i].p_vaddr;
		if (ph[i].p_type == PT_DYNAMIC) dyn = (void *)((char *)e + ph[i].p_offset);
	}
	if (!dyn) return 0;
	Elf64_Sym *sym = NULL;
	char *str = NULL;
	uint32_t *hash = NULL;
	for (; dyn->d_tag; dyn++) {
		if (dyn->d_tag == DT_SYMTAB) sym = (void *)(base + dyn->d_un.d_ptr);
		if (dyn->d_tag == DT_STRTAB) str = (void *)(base + dyn->d_un.d_ptr);
		if (dyn->d_tag == DT_HASH) hash = (void *)(base + dyn->d_un.d_ptr);
	}
	if (!sym || !str || !hash) return 0;
	for (uint32_t i = 0; i < hash[1]; i++)
		if (sym[i].st_name && !strcmp(str + sym[i].st_name, n) && sym[i].st_shndx) return 1;
	return 0;
}

/* [etapa 10 g, 2] lo que pidió después el navegador: flock, statfs y fstatfs,
 * /proc/self/status (TracerPid), chmod y fchmod */
static void mas_cosas(void)
{
	int fd = open("/tmp/mposix.flock", O_RDWR | O_CREAT | O_TRUNC, 0644);
	MIRA(fd >= 0 && flock(fd, LOCK_EX) == 0, "flock LOCK_EX");
	pid_t h = fork();
	if (h == 0) {
		int g = open("/tmp/mposix.flock", O_RDWR);
		int r1 = flock(g, LOCK_EX | LOCK_NB) < 0 && errno == EWOULDBLOCK;
		int r2 = flock(g, LOCK_SH | LOCK_NB) < 0 && errno == EWOULDBLOCK;
		struct flock fl = { .l_type = F_WRLCK, .l_whence = SEEK_SET, .l_start = 0, .l_len = 0 };
		int r3 = fcntl(g, F_SETLK, &fl) == 0;       /* (los de fcntl, aparte) */
		_exit(r1 && r2 && r3 ? 0 : 1);
	}
	int st = 0;
	waitpid(h, &st, 0);
	MIRA(WIFEXITED(st) && WEXITSTATUS(st) == 0, "flock de otro: EWOULDBLOCK; y fcntl, aparte");
	MIRA(flock(fd, LOCK_UN) == 0 && flock(fd, LOCK_SH) == 0 && flock(fd, LOCK_UN) == 0, "LOCK_UN y LOCK_SH");
	errno = 0;
	MIRA(flock(fd, 0) < 0 && errno == EINVAL, "flock con una orden que no hay: EINVAL");
	MIRA(fchmod(fd, 0600) == 0 && chmod("/tmp/mposix.flock", 0644) == 0, "chmod y fchmod");
	struct statfs sf;
	MIRA(fstatfs(fd, &sf) == 0 && sf.f_bsize > 0 && sf.f_namelen == 255, "fstatfs");
	MIRA(statfs("/tmp", &sf) == 0 && sf.f_bsize > 0, "statfs");
	errno = 0;
	MIRA(statfs("/no-esta-aqui", &sf) < 0 && errno == ENOENT, "statfs de lo que no hay: ENOENT");
	close(fd);
	unlink("/tmp/mposix.flock");
	char b[2048];
	int n = 0, t = open("/proc/self/status", O_RDONLY);
	if (t >= 0) { n = read(t, b, sizeof b - 1); close(t); }
	if (n < 0) n = 0;
	b[n] = 0;
	MIRA(strstr(b, "\nTracerPid:\t0\n") != NULL && strstr(b, "\nThreads:\t1\n") != NULL, "/proc/self/status: TracerPid y Threads");
	printf("  flock, statfs y fstatfs, chmod, /proc/self/status: bien\n");
}

static void getcpu_(void)
{
	long n = sysconf(_SC_NPROCESSORS_ONLN);
	int c = sched_getcpu();
	MIRA(c >= 0 && c < n, "sched_getcpu");
	MIRA(vdso_tiene("__vdso_getcpu"), "el vDSO tiene __vdso_getcpu");
	unsigned cpu = 99, nodo = 99;
	MIRA(syscall(SYS_getcpu, &cpu, &nodo, NULL) == 0 && cpu < (unsigned)n && nodo == 0, "getcpu (la llamada)");
	printf("  sched_getcpu, y __vdso_getcpu en el vDSO: %s\n", vdso_tiene("__vdso_getcpu") ? "sí" : "no");
}

/* [etapa 10 h] mmap MAP_SHARED de escribir de un archivo */
static void compartido_archivo(void)
{
	const char *r = "/tmp/mposix.comp";
	char t[3 * 4096], u[3 * 4096];
	for (int i = 0; i < (int)sizeof t; i++) t[i] = 'a' + i % 23;
	unlink(r);
	int fd = open(r, O_RDWR | O_CREAT | O_TRUNC, 0644);
	MIRA(fd >= 0 && write(fd, t, 8292) == 8292, "el archivo para MAP_SHARED (8 KB y 100 octetos)");
	char *m = mmap(NULL, 3 * 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	char *m2 = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 4096);
	MIRA(m != MAP_FAILED && m2 != MAP_FAILED, "mmap MAP_SHARED de escribir de un archivo (dos veces)");
	if (m == MAP_FAILED || m2 == MAP_FAILED) { close(fd); return; }
	int bien = !memcmp(m, t, 8292) && !memcmp(m2, t + 4096, 4096) && m[8292] == 0 && m[3 * 4096 - 1] == 0;
	MIRA(bien, "lo del archivo; más allá del final (en la última página), ceros");
	m[5000] = 'X';
	MIRA(m2[5000 - 4096] == 'X', "lo escrito en un mmap, en el otro");
	char c = 0;
	MIRA(pread(fd, &c, 1, 5000) == 1 && c == 'X', "y con read");
	MIRA(pwrite(fd, "QR", 2, 100) == 2 && m[100] == 'Q' && m[101] == 'R', "lo escrito con write, en el mmap");
	pid_t h = fork();
	if (h == 0) { m2[10] = 'H'; _exit(0); }
	int st = -1;
	waitpid(h, &st, 0);
	MIRA(WIFEXITED(st) && m[4096 + 10] == 'H', "lo que escribe un hijo, el padre lo ve");
	MIRA(msync(m, 3 * 4096, MS_SYNC) == 0 && msync(m, 4096, MS_ASYNC) == 0, "msync (MS_SYNC y MS_ASYNC)");
	errno = 0;
	MIRA(msync(m + 1, 4096, MS_SYNC) == -1 && errno == EINVAL, "msync mal alineado: EINVAL");
	errno = 0;
	MIRA(msync(m, 4096, MS_SYNC | MS_ASYNC) == -1 && errno == EINVAL, "msync con MS_SYNC y MS_ASYNC: EINVAL");
	char *n = mmap(NULL, 4096, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	munmap(n, 4096);
	errno = 0;
	MIRA(msync(n, 4096, MS_SYNC) == -1 && errno == ENOMEM, "msync de lo que no está puesto: ENOMEM");
	/* lo de más allá del final no va al archivo; ftruncate: lo de después, a cero */
	m[8292 + 5] = 'F';
	struct stat sb;
	MIRA(fstat(fd, &sb) == 0 && sb.st_size == 8292, "el tamaño, el de antes (lo de más allá del final no cuenta)");
	MIRA(ftruncate(fd, 4096) == 0 && ftruncate(fd, 8292) == 0 && m[4096 + 10] == 0 && m[5000] == 0 && m[8292 + 5] == 0 && m[0] == 'a', "ftruncate más corto y otra vez más largo: lo de después, a cero");
	m[6000] = 'S';
	munmap(m2, 4096);
	munmap(m, 3 * 4096);
	close(fd);
	fd = open(r, O_RDONLY);
	memcpy(u, t, 4096);
	u[100] = 'Q'; u[101] = 'R';
	memset(u + 4096, 0, 4096 + 100);
	u[6000] = 'S';
	memset(u + 8192, 0, 100);
	char v[8292 + 10];
	MIRA(fd >= 0 && read(fd, v, sizeof v) == 8292 && !memcmp(v, u, 8292), "al soltarlo, lo escrito en la memoria está en el archivo");
	errno = 0;
	char *ro = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	MIRA(ro == MAP_FAILED && errno == EACCES, "MAP_SHARED de escribir de uno abierto para leer: EACCES");
	ro = mmap(NULL, 4096, PROT_READ, MAP_SHARED, fd, 0);
	MIRA(ro != MAP_FAILED && ro[100] == 'Q', "MAP_SHARED de solo leer, sí");
	if (ro != MAP_FAILED) munmap(ro, 4096);
	close(fd);
	unlink(r);
	printf("  mmap MAP_SHARED de escribir de un archivo: dos mmap, read y write, un hijo, msync, ftruncate, al archivo al soltarlo: bien\n");
}

/* [etapa 10 i] pidfd */
static long ahora_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static int pidfd_abre(pid_t pid, unsigned marcas)
{
	return (int)syscall(434, pid, marcas);
}

static void pidfd_(void)
{
	/* un hijo que acaba a los 300 ms: poll sin plazo despierta enseguida */
	pid_t h = fork();
	if (h == 0) { usleep(300000); _exit(7); }
	int fd = pidfd_abre(h, 0);
	MIRA(fd >= 0, "pidfd_open de un hijo");
	MIRA(fcntl(fd, F_GETFD) == FD_CLOEXEC, "con O_CLOEXEC (siempre)");
	struct pollfd pf = { .fd = fd, .events = POLLIN };
	MIRA(poll(&pf, 1, 0) == 0 && pf.revents == 0, "sin acabar, poll no lo da listo");
	char c;
	errno = 0;
	MIRA(read(fd, &c, 1) == -1 && errno == EINVAL, "read de un pidfd: EINVAL");
	long t0 = ahora_ms();
	pf.revents = 0;
	int r = poll(&pf, 1, -1);
	long dt = ahora_ms() - t0;
	MIRA(r == 1 && (pf.revents & POLLIN) && dt >= 200 && dt < 1000, "al acabar el hijo, poll sin plazo despierta enseguida");
	int st = -1;
	MIRA(waitpid(h, &st, 0) == h && WIFEXITED(st) && WEXITSTATUS(st) == 7, "y el hijo, recogido (código 7)");
	pf.revents = 0;
	MIRA(poll(&pf, 1, 0) == 1 && (pf.revents & POLLIN), "recogido, sigue listo");
	close(fd);
	/* epoll, con otro hijo */
	h = fork();
	if (h == 0) { usleep(200000); _exit(0); }
	fd = pidfd_abre(h, PIDFD_NONBLOCK_);
	int ep = epoll_create1(EPOLL_CLOEXEC);
	struct epoll_event e = { .events = EPOLLIN, .data.u64 = 42 }, sal;
	epoll_ctl(ep, EPOLL_CTL_ADD, fd, &e);
	MIRA(fd >= 0 && (fcntl(fd, F_GETFL) & O_NONBLOCK), "PIDFD_NONBLOCK");
	t0 = ahora_ms();
	r = epoll_wait(ep, &sal, 1, 5000);
	dt = ahora_ms() - t0;
	MIRA(r == 1 && sal.data.u64 == 42 && (sal.events & EPOLLIN) && dt < 1000, "epoll con un pidfd: despierta al acabar");
	waitpid(h, &st, 0);
	close(ep);
	close(fd);
	/* de un zombi: listo ya */
	h = fork();
	if (h == 0) _exit(0);
	usleep(100000);
	fd = pidfd_abre(h, 0);
	pf.fd = fd;
	pf.revents = 0;
	MIRA(fd >= 0 && poll(&pf, 1, 0) == 1 && (pf.revents & POLLIN), "de un zombi: listo ya");
	close(fd);
	waitpid(h, &st, 0);
	errno = 0;
	MIRA(pidfd_abre(h, 0) == -1 && errno == ESRCH, "después de recogerlo: ESRCH");
	/* una señal por el pidfd */
	h = fork();
	if (h == 0) { for (;;) pause(); }
	fd = pidfd_abre(h, 0);
	MIRA(syscall(424, fd, SIGTERM, NULL, 0) == 0 && waitpid(h, &st, 0) == h && WIFSIGNALED(st) && WTERMSIG(st) == SIGTERM, "pidfd_send_signal: SIGTERM");
	errno = 0;
	MIRA(syscall(424, fd, SIGTERM, NULL, 0) == -1 && errno == ESRCH, "pidfd_send_signal a uno ya acabado: ESRCH");
	close(fd);
	/* los errores */
	errno = 0;
	MIRA(pidfd_abre(0, 0) == -1 && errno == EINVAL, "pidfd_open(0): EINVAL");
	errno = 0;
	MIRA(pidfd_abre(getpid(), 0x1) == -1 && errno == EINVAL, "marcas que no son: EINVAL");
	errno = 0;
	MIRA(syscall(424, 0, SIGTERM, NULL, 0) == -1 && errno == EBADF, "pidfd_send_signal de uno que no es pidfd: EBADF");
	fd = pidfd_abre(getpid(), 0);
	MIRA(fd >= 0, "pidfd_open de uno mismo");
	close(fd);
	printf("  pidfd: poll y epoll despiertan al acabar el proceso, de un zombi listo ya, read EINVAL, O_CLOEXEC, pidfd_send_signal, ESRCH y EINVAL: bien\n");
}

int main(void)
{
	setvbuf(stdout, NULL, _IOLBF, 0);
	printf("MPOSIX: lo de POSIX que pide Ladybird (enlaces, descriptores, el otro lado cerrado, memfd, madvise, mremap, EDEADLK, getcpu); %s\n", cómo());
	enlaces();
	descriptores();
	par_cerrado();
	memfd_grande();
	dontneed();
	remapeo();
	abrazo();
	getcpu_();
	mas_cosas();
	compartido_archivo();
	pidfd_();
	return acaba("MPOSIX");
}
