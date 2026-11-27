/* Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * nucleo64/programas/musl/munix.c - Hispa-64, etapa 10 d (docs/64/HISPA-64.md,
 * sección 21): MUNIX, la prueba de los sockets de Unix (AF_UNIX) y de la
 * memoria compartida por descriptor (memfd_create), lo que pide Ladybird:
 *   - socketpair de flujo, de datagramas y SOCK_SEQPACKET: lo que se manda
 *     llega (y los límites de cada mensaje), MSG_PEEK, MSG_TRUNC, FIONREAD,
 *     el final al cerrar el otro, EPIPE (con MSG_NOSIGNAL) y SIGPIPE;
 *   - O_NONBLOCK: EAGAIN al leer sin nada y al escribir con la cola llena;
 *     poll (POLLIN, POLLOUT, POLLHUP) y epoll (EPOLLIN, EPOLLRDHUP);
 *   - bind a una ruta (getsockname, stat: S_IFSOCK, EADDRINUSE, unlink y
 *     ENOENT después), listen, connect, accept, SO_PEERCRED; uno abstracto
 *     y el nombre automático; datagramas con sendto y recvfrom;
 *   - SCM_RIGHTS: una tubería y un eventfd que pasan a otro proceso (hecho
 *     antes: no los hereda), MSG_CTRUNC con los que no caben; SCM_CREDENTIALS
 *     con SO_PASSCRED;
 *   - memfd_create, ftruncate, fstat, mmap MAP_SHARED en dos procesos (el
 *     otro lo recibe por el socket), los sellos (F_ADD_SEALS);
 *   - dos hilos que se pasan 4.000 mensajes (2.000 de ida y vuelta), 4 MB de un lado a otro con
 *     su suma, y 300 descriptores pasados y cerrados (el núcleo mira después
 *     que no queda ningún socket ni memoria).
 * Al final, «MUNIX: todo bien (N pruebas)» y código 0. Corre igual en Linux. */
#define _GNU_SOURCE
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <stddef.h>
#include <sched.h>
#include <sys/syscall.h>
#ifndef FUTEX_WAIT_PRIVATE
#define FUTEX_WAIT_PRIVATE 128
#define FUTEX_WAKE_PRIVATE 129
#endif
#include "comun.h"

static volatile int sigpipes;
static void al_sigpipe(int s) { (void)s; sigpipes++; }

/* manda «n» octetos de d con los descriptores fds (k), por s */
static ssize_t manda_fds(int s, const void *d, size_t n, const int *fds, int k)
{
	struct msghdr m;
	struct iovec v = { (void *)d, n };
	char c[CMSG_SPACE(sizeof(int) * 16)];
	memset(&m, 0, sizeof m);
	m.msg_iov = &v;
	m.msg_iovlen = 1;
	if (k > 0) {
		struct cmsghdr *h;
		memset(c, 0, sizeof c);
		m.msg_control = c;
		m.msg_controllen = CMSG_SPACE(sizeof(int) * k);
		h = CMSG_FIRSTHDR(&m);
		h->cmsg_level = SOL_SOCKET;
		h->cmsg_type = SCM_RIGHTS;
		h->cmsg_len = CMSG_LEN(sizeof(int) * k);
		memcpy(CMSG_DATA(h), fds, sizeof(int) * k);
	}
	return sendmsg(s, &m, MSG_NOSIGNAL);
}

/* recibe hasta n octetos y hasta «cabe» descriptores (en fds; cuántos, en *k); las marcas en *fl */
static ssize_t recibe_fds(int s, void *d, size_t n, int *fds, int cabe, int *k, int *fl)
{
	struct msghdr m;
	struct iovec v = { d, n };
	char c[CMSG_SPACE(sizeof(int) * 16)];
	struct cmsghdr *h;
	ssize_t r;
	memset(&m, 0, sizeof m);
	m.msg_iov = &v;
	m.msg_iovlen = 1;
	m.msg_control = c;
	m.msg_controllen = cabe ? CMSG_SPACE(sizeof(int) * cabe) : 0;
	if (!cabe) m.msg_control = NULL;
	*k = 0;
	r = recvmsg(s, &m, MSG_CMSG_CLOEXEC);
	if (fl) *fl = m.msg_flags;
	if (r < 0) return r;
	for (h = CMSG_FIRSTHDR(&m); h; h = CMSG_NXTHDR(&m, h))
		if (h->cmsg_level == SOL_SOCKET && h->cmsg_type == SCM_RIGHTS) {
			int j, q = (h->cmsg_len - CMSG_LEN(0)) / sizeof(int);
			for (j = 0; j < q && *k < 16; j++) fds[(*k)++] = ((int *)CMSG_DATA(h))[j];
		}
	return r;
}

static void pares(void)
{
	int s[2], d[2], q[2], r, n;
	char b[64];
	struct stat st;
	struct sockaddr_un a;
	socklen_t l;
	MIRA(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, s) == 0, "socketpair de flujo");
	MIRA(write(s[0], "hola ", 5) == 5 && write(s[0], "mundo", 5) == 5, "write por un socketpair");
	n = 0;
	MIRA(ioctl(s[1], FIONREAD, &n) == 0 && n == 10, "FIONREAD de flujo");
	r = recv(s[1], b, sizeof b, MSG_PEEK);
	MIRA(r == 10 && memcmp(b, "hola mundo", 10) == 0, "MSG_PEEK de flujo: los dos envíos seguidos");
	r = read(s[1], b, 3);
	MIRA(r == 3 && memcmp(b, "hol", 3) == 0, "leer a trozos (3)");
	r = read(s[1], b, sizeof b);
	MIRA(r == 7 && memcmp(b, "a mundo", 7) == 0, "leer lo que queda (7)");
	MIRA(write(s[1], "y vuelta", 8) == 8 && read(s[0], b, sizeof b) == 8, "de vuelta, por el otro extremo");
	MIRA(fstat(s[0], &st) == 0 && S_ISSOCK(st.st_mode), "fstat: S_IFSOCK");
	n = 0; l = sizeof n;
	MIRA(getsockopt(s[0], SOL_SOCKET, SO_TYPE, &n, &l) == 0 && n == SOCK_STREAM, "SO_TYPE");
	n = 0; l = sizeof n;
	MIRA(getsockopt(s[0], SOL_SOCKET, SO_DOMAIN, &n, &l) == 0 && n == AF_UNIX, "SO_DOMAIN");
	l = sizeof a;
	MIRA(getsockname(s[0], (struct sockaddr *)&a, &l) == 0 && l == sizeof(sa_family_t) && a.sun_family == AF_UNIX, "getsockname de un socketpair: sin nombre");
	MIRA((fcntl(s[0], F_GETFD) & FD_CLOEXEC) != 0, "SOCK_CLOEXEC");
	close(s[0]);
	MIRA(read(s[1], b, sizeof b) == 0, "el final al cerrar el otro (read da 0)");
	errno = 0;
	MIRA(send(s[1], "x", 1, MSG_NOSIGNAL) == -1 && errno == EPIPE, "EPIPE con MSG_NOSIGNAL");
	signal(SIGPIPE, al_sigpipe);
	errno = 0;
	MIRA(write(s[1], "x", 1) == -1 && errno == EPIPE && sigpipes == 1, "write al cerrado: EPIPE y SIGPIPE");
	signal(SIGPIPE, SIG_DFL);
	close(s[1]);

	MIRA(socketpair(AF_UNIX, SOCK_DGRAM, 0, d) == 0, "socketpair de datagramas");
	send(d[0], "uno", 3, 0);
	send(d[0], "dos dos", 7, 0);
	send(d[0], "", 0, 0);
	n = 0;
	MIRA(ioctl(d[1], FIONREAD, &n) == 0 && n == 3, "FIONREAD de datagramas: el primero");
	MIRA(recv(d[1], b, sizeof b, 0) == 3 && recv(d[1], b, 2, MSG_TRUNC) == 7 && recv(d[1], b, sizeof b, 0) == 0,
	     "datagramas con sus límites, MSG_TRUNC y uno vacío");
	close(d[0]); close(d[1]);

	MIRA(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, q) == 0, "socketpair SOCK_SEQPACKET");
	send(q[0], "abc", 3, 0);
	send(q[0], "defgh", 5, 0);
	{
		struct msghdr m; struct iovec v = { b, 2 };
		memset(&m, 0, sizeof m); m.msg_iov = &v; m.msg_iovlen = 1;
		r = recvmsg(q[1], &m, 0);
		MIRA(r == 2 && (m.msg_flags & MSG_TRUNC), "SOCK_SEQPACKET: el mensaje cortado, con MSG_TRUNC");
	}
	MIRA(recv(q[1], b, sizeof b, 0) == 5, "SOCK_SEQPACKET: el siguiente, entero");
	close(q[0]);
	MIRA(recv(q[1], b, sizeof b, 0) == 0, "SOCK_SEQPACKET: el final");
	close(q[1]);
	MIRA(socketpair(AF_INET, SOCK_STREAM, 0, s) == -1 && errno == EOPNOTSUPP, "socketpair de AF_INET: EOPNOTSUPP");
	printf("  socketpair de flujo, datagramas y SOCK_SEQPACKET: MSG_PEEK, MSG_TRUNC, FIONREAD, el final, EPIPE y SIGPIPE\n");
}

static void sin_esperar(void)
{
	int s[2], ep, n = 0, r;
	char b[4096];
	struct pollfd p[1];
	struct epoll_event ev, sal[4];
	MIRA(socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0, s) == 0, "socketpair con SOCK_NONBLOCK");
	errno = 0;
	MIRA(read(s[1], b, sizeof b) == -1 && errno == EAGAIN, "leer sin nada: EAGAIN");
	p[0].fd = s[1]; p[0].events = POLLIN | POLLOUT;
	MIRA(poll(p, 1, 0) == 1 && p[0].revents == POLLOUT, "poll: solo POLLOUT");
	memset(b, 'x', sizeof b);
	while ((r = write(s[0], b, sizeof b)) > 0) n += r;
	MIRA(r == -1 && errno == EAGAIN && n >= 65536, "escribir hasta llenar: EAGAIN");
	p[0].fd = s[0]; p[0].events = POLLOUT;
	MIRA(poll(p, 1, 0) == 0, "lleno: poll sin POLLOUT");
	ep = epoll_create1(EPOLL_CLOEXEC);
	ev.events = EPOLLIN | EPOLLRDHUP; ev.data.u64 = 77;
	MIRA(epoll_ctl(ep, EPOLL_CTL_ADD, s[1], &ev) == 0, "epoll_ctl de un socket de Unix");
	MIRA(epoll_wait(ep, sal, 4, 100) == 1 && sal[0].data.u64 == 77 && (sal[0].events & EPOLLIN), "epoll: EPOLLIN");
	while (read(s[1], b, sizeof b) > 0) ;
	p[0].fd = s[0]; p[0].events = POLLOUT;
	MIRA(poll(p, 1, 100) == 1 && (p[0].revents & POLLOUT), "vaciado: POLLOUT otra vez");
	close(s[0]);
	MIRA(epoll_wait(ep, sal, 4, 100) == 1 && (sal[0].events & EPOLLRDHUP) && (sal[0].events & EPOLLHUP), "epoll al cerrar el otro: EPOLLRDHUP y EPOLLHUP");
	close(ep); close(s[1]);
	printf("  O_NONBLOCK: EAGAIN al leer y con la cola llena (%d KB); poll y epoll (EPOLLIN, EPOLLRDHUP, EPOLLHUP)\n", n / 1024);
}

static char ruta_s[200], ruta_d[200];
static void rutas(void)
{
	char c[150];
	if (!getcwd(c, sizeof c)) strcpy(c, "/");
	snprintf(ruta_s, sizeof ruta_s, "%s%smunix.sock", c, c[strlen(c) - 1] == '/' ? "" : "/");
	snprintf(ruta_d, sizeof ruta_d, "%s%smunix.dg", c, c[strlen(c) - 1] == '/' ? "" : "/");
}

static void con_nombre(void)
{
	int sv, c, a, d1, d2, n;
	struct sockaddr_un dir, otra;
	socklen_t l;
	struct stat st;
	struct ucred cr;
	char b[64];
	unlink(ruta_s);
	sv = socket(AF_UNIX, SOCK_STREAM, 0);
	memset(&dir, 0, sizeof dir);
	dir.sun_family = AF_UNIX;
	strcpy(dir.sun_path, ruta_s);
	MIRA(bind(sv, (struct sockaddr *)&dir, sizeof dir) == 0, "bind a una ruta");
	MIRA(listen(sv, 4) == 0, "listen");
	MIRA(stat(ruta_s, &st) == 0 && S_ISSOCK(st.st_mode), "stat de la ruta: S_IFSOCK");
	l = sizeof otra;
	MIRA(getsockname(sv, (struct sockaddr *)&otra, &l) == 0 && l == offsetof(struct sockaddr_un, sun_path) + strlen(ruta_s) + 1 && strcmp(otra.sun_path, ruta_s) == 0, "getsockname: la ruta");
	c = socket(AF_UNIX, SOCK_STREAM, 0);
	MIRA(bind(c, (struct sockaddr *)&dir, sizeof dir) == -1 && errno == EADDRINUSE, "la misma ruta otra vez: EADDRINUSE");
	MIRA(connect(c, (struct sockaddr *)&dir, sizeof dir) == 0, "connect a la ruta");
	a = accept(sv, NULL, NULL);
	MIRA(a >= 0, "accept");
	l = sizeof cr;
	MIRA(getsockopt(a, SOL_SOCKET, SO_PEERCRED, &cr, &l) == 0 && l == sizeof cr && cr.pid == getpid(), "SO_PEERCRED: el pid del otro");
	l = sizeof otra;
	MIRA(getpeername(c, (struct sockaddr *)&otra, &l) == 0 && strcmp(otra.sun_path, ruta_s) == 0, "getpeername: la ruta del que escucha");
	MIRA(write(c, "ping", 4) == 4 && read(a, b, sizeof b) == 4 && write(a, "pong", 4) == 4 && read(c, b, sizeof b) == 4 && memcmp(b, "pong", 4) == 0, "ping y pong por la conexión");
	close(c); close(a);
	MIRA(unlink(ruta_s) == 0, "unlink de la ruta");
	c = socket(AF_UNIX, SOCK_STREAM, 0);
	MIRA(connect(c, (struct sockaddr *)&dir, sizeof dir) == -1 && errno == ENOENT, "connect después de unlink: ENOENT");
	close(c); close(sv);
	/* abstracto */
	sv = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	memset(&dir, 0, sizeof dir);
	dir.sun_family = AF_UNIX;
	memcpy(dir.sun_path, "\0hispa-abstracto", 16);
	l = offsetof(struct sockaddr_un, sun_path) + 16;
	MIRA(bind(sv, (struct sockaddr *)&dir, l) == 0 && listen(sv, 1) == 0, "bind y listen de uno abstracto");
	c = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_NONBLOCK, 0);
	MIRA(connect(c, (struct sockaddr *)&dir, l) == 0, "connect al abstracto");
	a = accept4(sv, NULL, NULL, SOCK_CLOEXEC);
	MIRA(a >= 0 && send(c, "abs", 3, 0) == 3 && recv(a, b, sizeof b, 0) == 3, "accept4 y un mensaje");
	l = sizeof otra;
	MIRA(getsockname(a, (struct sockaddr *)&otra, &l) == 0 && l == offsetof(struct sockaddr_un, sun_path) + 16 && memcmp(otra.sun_path, "\0hispa-abstracto", 16) == 0, "getsockname del aceptado: el abstracto");
	close(a); close(c); close(sv);
	c = socket(AF_UNIX, SOCK_STREAM, 0);
	MIRA(connect(c, (struct sockaddr *)&dir, offsetof(struct sockaddr_un, sun_path) + 16) == -1 && errno == ECONNREFUSED, "el abstracto cerrado: ECONNREFUSED");
	close(c);
	/* datagramas con nombre y el automático */
	d1 = socket(AF_UNIX, SOCK_DGRAM, 0);
	d2 = socket(AF_UNIX, SOCK_DGRAM, 0);
	memset(&dir, 0, sizeof dir);
	dir.sun_family = AF_UNIX;
	MIRA(bind(d1, (struct sockaddr *)&dir, sizeof(sa_family_t)) == 0, "bind con el nombre automático");
	l = sizeof dir;
	MIRA(getsockname(d1, (struct sockaddr *)&dir, &l) == 0 && l == offsetof(struct sockaddr_un, sun_path) + 6 && dir.sun_path[0] == 0, "el automático: «\\0» y cinco cifras");
	MIRA(sendto(d2, "sin nombre", 10, 0, (struct sockaddr *)&dir, l) == 10, "sendto al automático");
	l = sizeof otra;
	n = recvfrom(d1, b, sizeof b, 0, (struct sockaddr *)&otra, &l);
	MIRA(n == 10 && l == 0, "recvfrom de uno sin nombre: largo 0");
	memset(&otra, 0, sizeof otra);
	otra.sun_family = AF_UNIX;
	strcpy(otra.sun_path, ruta_d);
	MIRA(bind(d2, (struct sockaddr *)&otra, sizeof otra) == 0, "bind del otro a una ruta");
	sendto(d2, "con nombre", 10, 0, (struct sockaddr *)&dir, offsetof(struct sockaddr_un, sun_path) + 6);
	l = sizeof otra;
	n = recvfrom(d1, b, sizeof b, 0, (struct sockaddr *)&otra, &l);
	MIRA(n == 10 && strcmp(otra.sun_path, ruta_d) == 0, "recvfrom: la ruta de quien lo manda");
	close(d1); close(d2);
	unlink(ruta_d);
	printf("  bind a una ruta (stat: S_IFSOCK, EADDRINUSE, unlink), listen, connect, accept, SO_PEERCRED; abstracto y automático; sendto y recvfrom\n");
}

/* el hijo: recibe por s una tubería, un eventfd y un memfd; escribe en cada uno */
static int hijo_fds(int s)
{
	int f[16], k, fl, mal = 0;
	char b[16];
	uint64_t uno = 1;
	ssize_t r = recibe_fds(s, b, sizeof b, f, 16, &k, &fl);
	if (r != 4 || k != 3) return 10;
	if ((fcntl(f[0], F_GETFD) & FD_CLOEXEC) == 0) mal |= 1;            /* MSG_CMSG_CLOEXEC */
	if (write(f[0], "desde el hijo", 13) != 13) mal |= 2;
	if (write(f[1], &uno, 8) != 8) mal |= 4;
	{
		struct stat st;
		char *m;
		if (fstat(f[2], &st) != 0 || st.st_size != 65536) return 20;
		m = mmap(NULL, 65536, PROT_READ | PROT_WRITE, MAP_SHARED, f[2], 0);
		if (m == MAP_FAILED) return 21;
		if (strcmp(m, "del padre") != 0) mal |= 8;
		strcpy(m + 32768, "del hijo");
		munmap(m, 65536);
	}
	/* SCM_CREDENTIALS: el padre las pide con SO_PASSCRED */
	if (send(s, "fin", 3, 0) != 3) mal |= 16;
	close(f[0]); close(f[1]); close(f[2]);
	return mal;
}

static void pasar(void)
{
	int s[2], t[2], e, m, f[16], k, fl, st, i, on = 1;
	pid_t h;
	char b[64], *mem;
	uint64_t x = 0;
	MIRA(socketpair(AF_UNIX, SOCK_STREAM, 0, s) == 0, "socketpair para pasar descriptores");
	h = fork();
	if (h == 0) { close(s[0]); _exit(hijo_fds(s[1])); }
	close(s[1]);
	/* lo que se pasa, hecho después del fork: el hijo no lo tiene */
	MIRA(pipe(t) == 0, "pipe");
	e = eventfd(0, EFD_CLOEXEC);
	m = memfd_create("munix", MFD_CLOEXEC | MFD_ALLOW_SEALING);
	MIRA(m >= 0 && ftruncate(m, 65536) == 0, "memfd_create y ftruncate");
	mem = mmap(NULL, 65536, PROT_READ | PROT_WRITE, MAP_SHARED, m, 0);
	MIRA(mem != MAP_FAILED, "mmap MAP_SHARED del memfd");
	strcpy(mem, "del padre");
	f[0] = t[1]; f[1] = e; f[2] = m;
	setsockopt(s[0], SOL_SOCKET, SO_PASSCRED, &on, sizeof on);
	MIRA(manda_fds(s[0], "fds!", 4, f, 3) == 4, "sendmsg con SCM_RIGHTS (3 descriptores)");
	close(t[1]);
	MIRA(read(t[0], b, sizeof b) == 13 && memcmp(b, "desde el hijo", 13) == 0, "la tubería pasada: lo que escribe el hijo llega");
	MIRA(read(e, &x, 8) == 8 && x == 1, "el eventfd pasado: el hijo lo suma");
	{
		struct msghdr mh; struct iovec v = { b, sizeof b };
		char c[CMSG_SPACE(sizeof(struct ucred))];
		struct cmsghdr *ch;
		struct ucred cr = { 0, 1, 1 };
		memset(&mh, 0, sizeof mh); mh.msg_iov = &v; mh.msg_iovlen = 1; mh.msg_control = c; mh.msg_controllen = sizeof c;
		MIRA(recvmsg(s[0], &mh, 0) == 3, "recvmsg con SO_PASSCRED");
		ch = CMSG_FIRSTHDR(&mh);
		if (ch && ch->cmsg_type == SCM_CREDENTIALS) memcpy(&cr, CMSG_DATA(ch), sizeof cr);
		MIRA(ch && ch->cmsg_level == SOL_SOCKET && ch->cmsg_type == SCM_CREDENTIALS && cr.pid == h && cr.uid == 0 && cr.gid == 0, "SCM_CREDENTIALS: el pid del hijo");
	}
	MIRA(waitpid(h, &st, 0) == h && WIFEXITED(st) && WEXITSTATUS(st) == 0, "el hijo: todo bien con lo pasado");
	MIRA(strcmp(mem + 32768, "del hijo") == 0, "el memfd, compartido de verdad: lo del hijo está");
	/* los sellos */
	MIRA(fcntl(m, F_ADD_SEALS, F_SEAL_SHRINK) == 0 && (fcntl(m, F_GET_SEALS) & F_SEAL_SHRINK), "F_ADD_SEALS y F_GET_SEALS");
	MIRA(ftruncate(m, 4096) == -1 && errno == EPERM && ftruncate(m, 131072) == 0, "sellado para encoger: EPERM; crecer, sí");
	munmap(mem, 65536);
	close(m); close(e); close(t[0]);
	m = memfd_create("sin sellos", 0);
	MIRA(m >= 0 && fcntl(m, F_ADD_SEALS, F_SEAL_GROW) == -1 && errno == EPERM, "sin MFD_ALLOW_SEALING: EPERM");
	close(m);
	/* MSG_CTRUNC: tres descriptores y sitio para uno */
	{
		int u[2], p3[3];
		socketpair(AF_UNIX, SOCK_DGRAM, 0, u);
		p3[0] = p3[1] = p3[2] = u[0];
		manda_fds(u[0], "t", 1, p3, 3);
		/* (en CMSG_SPACE de uno caben dos: el relleno hasta 8, como en Linux) */
		MIRA(recibe_fds(u[1], b, sizeof b, f, 1, &k, &fl) == 1 && k == 2 && (fl & MSG_CTRUNC), "MSG_CTRUNC: caben dos de tres");
		for (i = 0; i < k; i++) close(f[i]);
		manda_fds(u[0], "t", 1, p3, 2);
		MIRA(recibe_fds(u[1], b, sizeof b, f, 0, &k, &fl) == 1 && k == 0 && (fl & MSG_CTRUNC), "sin búfer de control: cerrados, MSG_CTRUNC");
		close(u[0]); close(u[1]);
	}
	/* 300 descriptores pasados y cerrados (el núcleo mira luego que no queda nada) */
	{
		int u[2], ok = 0;
		socketpair(AF_UNIX, SOCK_STREAM, 0, u);
		for (i = 0; i < 300; i++) {
			int ev = eventfd(i + 1, 0), r2;
			uint64_t v = 0;
			manda_fds(u[0], "e", 1, &ev, 1);
			close(ev);
			if (recibe_fds(u[1], b, 1, f, 1, &k, &fl) == 1 && k == 1 && read(f[0], &v, 8) == 8 && v == (uint64_t)i + 1) ok++;
			if (k) close(f[0]);
			(void)r2;
		}
		/* y unos cuantos que se quedan en la cola al cerrar */
		for (i = 0; i < 5; i++) { int ev = eventfd(0, 0); manda_fds(u[0], "q", 1, &ev, 1); close(ev); }
		close(u[0]); close(u[1]);
		MIRA(ok == 300, "300 eventfd pasados uno a uno, cada uno con lo suyo");
	}
	printf("  SCM_RIGHTS a otro proceso (una tubería, un eventfd y un memfd hechos después del fork), MSG_CTRUNC; SCM_CREDENTIALS; memfd compartido y sus sellos; 300 pasados\n");
}

/* ---- dos hilos que se pasan mensajes ---- */
static int ph[2];
static void *eco(void *x)
{
	char b[256];
	int n;
	(void)x;
	while ((n = read(ph[1], b, sizeof b)) > 0)
		if (write(ph[1], b, n) != n) break;
	return NULL;
}

static void *manda_mucho(void *x)
{
	int fd = *(int *)x, i;
	unsigned char b[8192];
	for (i = 0; i < 512; i++) {
		int j;
		for (j = 0; j < (int)sizeof b; j++) b[j] = (unsigned char)(i * 7 + j);
		if (write(fd, b, sizeof b) != (ssize_t)sizeof b) break;
	}
	close(fd);
	return NULL;
}

static void hilos(void)
{
	pthread_t h;
	int i, ok = 0, s[2];
	char b[32], c[32];
	struct timespec t0, t1;
	unsigned long suma = 0, esperada = 0, total = 0;
	unsigned char k[65536];
	ssize_t n;
	socketpair(AF_UNIX, SOCK_SEQPACKET, 0, ph);
	pthread_create(&h, NULL, eco, NULL);
	clock_gettime(CLOCK_MONOTONIC, &t0);
	for (i = 0; i < 2000; i++) {
		int l = snprintf(b, sizeof b, "mensaje %d", i);
		if (write(ph[0], b, l) == l && read(ph[0], c, sizeof c) == l && memcmp(b, c, l) == 0) ok++;
	}
	clock_gettime(CLOCK_MONOTONIC, &t1);
	close(ph[0]);
	pthread_join(h, NULL);
	close(ph[1]);
	MIRA(ok == 2000, "2.000 mensajes de ida y vuelta entre dos hilos");
	socketpair(AF_UNIX, SOCK_STREAM, 0, s);
	pthread_create(&h, NULL, manda_mucho, &s[0]);
	while ((n = read(s[1], k, sizeof k)) > 0) {
		ssize_t j;
		for (j = 0; j < n; j++) suma += k[j];
		total += n;
	}
	pthread_join(h, NULL);
	close(s[1]);
	for (i = 0; i < 512; i++) { int j; for (j = 0; j < 8192; j++) esperada += (unsigned char)(i * 7 + j); }
	MIRA(total == 4u << 20 && suma == esperada, "4 MB por un socket de flujo, con su suma");
	printf("  dos hilos: 2.000 mensajes de ida y vuelta en %ld ms; 4 MB por un socket de flujo, la suma bien\n",
	       (long)((t1.tv_sec - t0.tv_sec) * 1000 + (t1.tv_nsec - t0.tv_nsec) / 1000000));
}

/* ---- [auditoría H4 y H11] las carreras de los cerrojos nuevos ---- */
static volatile int palabra, parar_c;
static void *espera_futex(void *x)
{
	int i;
	struct timespec p = { 0, 1000000 };
	(void)x;
	for (i = 0; i < 20 && !parar_c; i++) syscall(SYS_futex, &palabra, FUTEX_WAIT_PRIVATE, 0, &p, 0, 0);
	return NULL;
}

static void *sale_ya(void *x)
{
	(void)x;
	sched_yield();
	syscall(SYS_exit, 0);
	return NULL;
}

static void carreras(void)
{
	int r, i, bien_f = 0, bien_s = 0;
	for (r = 0; r < 40; r++) {
		pthread_t h[8];
		parar_c = 0;
		for (i = 0; i < 8; i++) pthread_create(&h[i], NULL, espera_futex, NULL);
		for (i = 0; i < 400; i++) syscall(SYS_futex, &palabra, FUTEX_WAKE_PRIVATE, 8, 0, 0, 0);
		parar_c = 1;
		for (i = 0; i < 8; i++) pthread_join(h[i], NULL);
		bien_f++;
	}
	for (r = 0; r < 30; r++) {
		pid_t h = fork();
		int st = 0;
		if (h == 0) {
			pthread_t t;
			for (i = 0; i < 8; i++) pthread_create(&t, NULL, sale_ya, NULL);
			if (r & 1) sched_yield();
			_exit(7);                      /* exit_group, con los hilos saliendo a la vez */
		}
		if (waitpid(h, &st, 0) == h && WIFEXITED(st) && WEXITSTATUS(st) == 7) bien_s++;
	}
	MIRA(bien_f == 40, "40 rondas de 8 hilos con FUTEX_WAIT de 1 ms y FUTEX_WAKE a la vez (H11)");
	MIRA(bien_s == 30, "30 procesos con 8 hilos que salen mientras el proceso acaba (H4)");
	printf("  carreras: 40 rondas de futex con plazo contra FUTEX_WAKE y 30 procesos que acaban con sus hilos saliendo a la vez\n");
}

/* [etapa 10 d] /tmp, de escribir (en Hispa-64, el disco en memoria) */
static void tmp(void)
{
	static char b[100000], c[100000];
	int fd, i, ok = 0;
	for (i = 0; i < (int)sizeof b; i++) b[i] = (char)(i * 7 + 3);
	fd = open("/tmp/munix.dat", O_CREAT | O_TRUNC | O_RDWR, 0644);
	if (fd >= 0 && write(fd, b, sizeof b) == (ssize_t)sizeof b && lseek(fd, 0, SEEK_SET) == 0
	    && read(fd, c, sizeof c) == (ssize_t)sizeof c && memcmp(b, c, sizeof b) == 0) ok = 1;
	if (fd >= 0) close(fd);
	MIRA(ok && unlink("/tmp/munix.dat") == 0, "/tmp: escribir 100 KB, leerlos y borrarlos");
	printf("  /tmp: 100 KB escritos, leídos y borrados\n");
}

int main(void)
{
	setvbuf(stdout, NULL, _IOLBF, 0);
	printf("MUNIX: los sockets de Unix (AF_UNIX) y memfd_create; %s\n", cómo());
	fflush(stdout);
	rutas();
	pares();
	sin_esperar();
	con_nombre();
	pasar();
	hilos();
	carreras();
	tmp();
	return acaba("MUNIX");
}
