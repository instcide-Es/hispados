/* Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * nucleo64/programas/red/mred.c - Hispa-64, etapa 9: MRED, la prueba de
 * las llamadas de red y de lo que piden, sin tarjeta de red (solo por
 * 127.0.0.1), como MDISP: una línea por grupo y, al final,
 * «MRED: todo bien (N pruebas)» y código 0 (o «MAL: ...» y código 1).
 * Los grupos: los sockets (TCP, UDP, SOCK_NONBLOCK, SOCK_CLOEXEC); bind y
 * getsockname; TCP bloqueante (listen, connect, accept, getpeername); TCP
 * sin esperar (connect con EINPROGRESS, poll POLLOUT, SO_ERROR, accept4 con
 * SOCK_CLOEXEC|SOCK_NONBLOCK); 256 KB en las dos direcciones a la vez (con
 * hilos y poll; send/recv y write/read); shutdown(SHUT_WR); ECONNREFUSED;
 * EADDRINUSE y SO_REUSEADDR; UDP (sendto/recvfrom, MSG_DONTWAIT, connect y
 * send); epoll; sendmsg/recvmsg con dos trozos; las opciones (TCP_NODELAY,
 * SO_RCVBUF, SO_SNDBUF, SO_KEEPALIVE, SO_TYPE); getaddrinfo; eventfd;
 * timerfd; /dev/urandom, /dev/random y getrandom; [etapa 10] /etc/localtime
 * (TZif con la zona de TZ al final: localtime sin TZ da lo mismo que con TZ). */
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <pthread.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <sys/random.h>
#include <sys/uio.h>
#include <netinet/tcp.h>
#include "red.h"
#include "../musl/comun.h"

#define GRANDE (256 * 1024)

static struct sockaddr_in lo(int puerto)
{
	struct sockaddr_in s;
	memset(&s, 0, sizeof s);
	s.sin_family = AF_INET;
	s.sin_port = htons(puerto);
	s.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	return s;
}

static int puerto_de(int fd)
{
	struct sockaddr_in s;
	socklen_t l = sizeof s;
	if (getsockname(fd, (struct sockaddr *)&s, &l) < 0)
		return -1;
	return ntohs(s.sin_port);
}

/* un socket TCP que escucha en 127.0.0.1:(el que dé el sistema) */
static int escucha(int *puerto)
{
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in s = lo(0);
	if (fd < 0 || bind(fd, (struct sockaddr *)&s, sizeof s) < 0 || listen(fd, 8) < 0)
		return -1;
	*puerto = puerto_de(fd);
	return fd;
}

/* un par de sockets TCP conectados por 127.0.0.1 (cliente y aceptado) */
static int par(int *c, int *a)
{
	int p, l = escucha(&p);
	struct sockaddr_in s = lo(p);
	if (l < 0)
		return -1;
	*c = socket(AF_INET, SOCK_STREAM, 0);
	if (*c < 0 || connect(*c, (struct sockaddr *)&s, sizeof s) < 0)
		return -1;
	*a = accept(l, 0, 0);
	close(l);
	return *a < 0 ? -1 : 0;
}

static int lee_todo(int fd, void *b, size_t n)
{
	size_t h = 0;
	while (h < n) {
		ssize_t r = read(fd, (char *)b + h, n - h);
		if (r <= 0)
			return -1;
		h += r;
	}
	return 0;
}

static void pruebas_sockets(void)
{
	int t = socket(AF_INET, SOCK_STREAM, 0), u = socket(AF_INET, SOCK_DGRAM, 0);
	int nb = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0), v = 0;
	socklen_t l = sizeof v;
	MIRA(t >= 0, "socket(AF_INET, SOCK_STREAM)");
	MIRA(u >= 0, "socket(AF_INET, SOCK_DGRAM)");
	MIRA(nb >= 0, "socket con SOCK_NONBLOCK|SOCK_CLOEXEC");
	MIRA(fcntl(nb, F_GETFL) & O_NONBLOCK, "SOCK_NONBLOCK: F_GETFL dice O_NONBLOCK");
	MIRA(fcntl(nb, F_GETFD) & FD_CLOEXEC, "SOCK_CLOEXEC: F_GETFD dice FD_CLOEXEC");
	MIRA(!(fcntl(t, F_GETFL) & O_NONBLOCK), "sin SOCK_NONBLOCK, bloqueante");
	MIRA(getsockopt(t, SOL_SOCKET, SO_TYPE, &v, &l) == 0 && v == SOCK_STREAM, "getsockopt SO_TYPE de TCP = SOCK_STREAM");
	l = sizeof v;
	MIRA(getsockopt(u, SOL_SOCKET, SO_TYPE, &v, &l) == 0 && v == SOCK_DGRAM, "getsockopt SO_TYPE de UDP = SOCK_DGRAM");
	MIRA(socket(AF_INET, 77, 0) < 0, "socket de un tipo que no hay: falla");
	{
		struct sockaddr_in s = lo(0);
		int p;
		MIRA(bind(t, (struct sockaddr *)&s, sizeof s) == 0, "bind a 127.0.0.1:0");
		p = puerto_de(t);
		MIRA(p > 0, "getsockname: el puerto que ha dado el sistema");
		MIRA(bind(u, (struct sockaddr *)&s, sizeof s) == 0 && puerto_de(u) > 0, "bind de UDP a 127.0.0.1:0");
		printf("  sockets: TCP, UDP, SOCK_NONBLOCK|SOCK_CLOEXEC; bind a 127.0.0.1:0 da el puerto %d\n", p);
	}
	close(t);
	close(u);
	close(nb);
}

static void pruebas_tcp(void)
{
	int p, l = escucha(&p), c, a, pc;
	struct sockaddr_in s = lo(p), o;
	socklen_t ol = sizeof o;
	char b[16];

	MIRA(l >= 0, "listen en 127.0.0.1");
	c = socket(AF_INET, SOCK_STREAM, 0);
	MIRA(connect(c, (struct sockaddr *)&s, sizeof s) == 0, "connect (bloqueante) a 127.0.0.1");
	a = accept(l, (struct sockaddr *)&o, &ol);
	MIRA(a >= 0, "accept");
	pc = puerto_de(c);
	MIRA(ol == sizeof o && o.sin_family == AF_INET && ntohs(o.sin_port) == pc &&
	     o.sin_addr.s_addr == htonl(INADDR_LOOPBACK), "accept da la dirección del otro");
	ol = sizeof o;
	MIRA(getpeername(c, (struct sockaddr *)&o, &ol) == 0 && ntohs(o.sin_port) == p, "getpeername del que conecta: el puerto que escucha");
	ol = sizeof o;
	MIRA(getpeername(a, (struct sockaddr *)&o, &ol) == 0 && ntohs(o.sin_port) == pc, "getpeername del aceptado: el puerto del otro");
	MIRA(puerto_de(a) == p, "getsockname del aceptado: el puerto que escucha");
	MIRA(write(c, "hola", 4) == 4 && read(a, b, sizeof b) == 4 && !memcmp(b, "hola", 4), "write y read");
	MIRA(send(a, "adiós", 6, 0) == 6 && recv(c, b, sizeof b, 0) == 6 && !memcmp(b, "adiós", 6), "send y recv");
	MIRA(recv(c, b, sizeof b, MSG_DONTWAIT) < 0 && errno == EAGAIN, "recv con MSG_DONTWAIT sin nada: EAGAIN");
	MIRA(send(a, "xyz", 3, 0) == 3 && recv(c, b, 2, MSG_PEEK) == 2 && recv(c, b, 3, MSG_WAITALL) == 3 && !memcmp(b, "xyz", 3), "MSG_PEEK y MSG_WAITALL");
	/* shutdown(SHUT_WR): la otra punta lee 0, pero aún puede contestar */
	MIRA(shutdown(c, SHUT_WR) == 0, "shutdown(SHUT_WR)");
	MIRA(read(a, b, sizeof b) == 0, "tras shutdown(SHUT_WR), la otra punta lee 0");
	MIRA(write(a, "ok", 2) == 2 && read(c, b, sizeof b) == 2, "medio cerrado: la otra punta aún contesta");
	close(a);
	MIRA(read(c, b, sizeof b) == 0, "cerrado el otro: read da 0");
	close(c);
	printf("  TCP bloqueante: listen/connect/accept por 127.0.0.1:%d, getpeername, send/recv, shutdown(SHUT_WR)\n", p);

	/* sin esperar */
	{
		int nb = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0), r, e = -1, cómo;
		socklen_t el = sizeof e;
		struct pollfd q = { 0, POLLOUT, 0 };
		r = connect(nb, (struct sockaddr *)&s, sizeof s);
		cómo = (r < 0) ? errno : 0;
		MIRA(r == 0 || errno == EINPROGRESS, "connect sin esperar: EINPROGRESS (o 0)");
		q.fd = nb;
		MIRA(poll(&q, 1, 1000) == 1 && (q.revents & POLLOUT), "poll POLLOUT cuando ya está conectado");
		MIRA(getsockopt(nb, SOL_SOCKET, SO_ERROR, &e, &el) == 0 && e == 0, "getsockopt SO_ERROR = 0");
		a = accept4(l, 0, 0, SOCK_CLOEXEC | SOCK_NONBLOCK);
		MIRA(a >= 0, "accept4 con SOCK_CLOEXEC|SOCK_NONBLOCK");
		MIRA((fcntl(a, F_GETFL) & O_NONBLOCK) && (fcntl(a, F_GETFD) & FD_CLOEXEC), "accept4: el nuevo, con O_NONBLOCK y FD_CLOEXEC");
		MIRA(read(a, b, sizeof b) < 0 && errno == EAGAIN, "read sin esperar y sin nada: EAGAIN");
		fcntl(l, F_SETFL, fcntl(l, F_GETFL) | O_NONBLOCK);
		MIRA(accept(l, 0, 0) < 0 && errno == EAGAIN, "accept sin esperar y sin nadie: EAGAIN");
		printf("  TCP sin esperar: connect da %s, poll POLLOUT, SO_ERROR = 0, accept4 con SOCK_CLOEXEC|SOCK_NONBLOCK\n",
		       cómo == EINPROGRESS ? "EINPROGRESS" : cómo == 0 ? "0" : strerror(cómo));
		close(a);
		close(nb);
	}
	close(l);
}

/* ---- 256 KB en las dos direcciones a la vez ---- */
struct envío { int fd, con_send; const unsigned char *b; int bien; };

static void *envía(void *x)
{
	struct envío *e = x;
	size_t h = 0;
	while (h < GRANDE) {
		ssize_t w = e->con_send ? send(e->fd, e->b + h, GRANDE - h, 0) : write(e->fd, e->b + h, GRANDE - h);
		if (w <= 0)
			return 0;
		h += w;
	}
	e->bien = 1;
	return 0;
}

static void pruebas_grande(void)
{
	static unsigned char p1[GRANDE], p2[GRANDE], r1[GRANDE], r2[GRANDE];
	int c, a, i;
	size_t n1 = 0, n2 = 0;
	pthread_t h1, h2;
	struct envío e1, e2;
	double t0 = ms_ahora();

	for (i = 0; i < GRANDE; i++) {
		p1[i] = i * 7 + (i >> 9);
		p2[i] = i * 13 + (i >> 11) + 1;
	}
	MIRA(par(&c, &a) == 0, "un par de sockets TCP conectados");
	e1 = (struct envío){ c, 1, p1, 0 };
	e2 = (struct envío){ a, 0, p2, 0 };
	MIRA(pthread_create(&h1, 0, envía, &e1) == 0 && pthread_create(&h2, 0, envía, &e2) == 0, "dos hilos que mandan");
	while (n1 < GRANDE || n2 < GRANDE) {
		struct pollfd q[2] = { { a, n1 < GRANDE ? POLLIN : 0, 0 }, { c, n2 < GRANDE ? POLLIN : 0, 0 } };
		ssize_t r;
		if (poll(q, 2, 2000) <= 0)
			break;
		if (q[0].revents & POLLIN) {
			r = recv(a, r1 + n1, GRANDE - n1, 0);
			if (r <= 0)
				break;
			n1 += r;
		}
		if (q[1].revents & POLLIN) {
			r = read(c, r2 + n2, GRANDE - n2);
			if (r <= 0)
				break;
			n2 += r;
		}
	}
	pthread_join(h1, 0);
	pthread_join(h2, 0);
	MIRA(e1.bien && e2.bien, "send y write de 256 KB, enteros");
	MIRA(n1 == GRANDE && !memcmp(p1, r1, GRANDE), "256 KB de ida (send → recv), iguales");
	MIRA(n2 == GRANDE && !memcmp(p2, r2, GRANDE), "256 KB de vuelta (write → read), iguales");
	printf("  256 KB en las dos direcciones a la vez (dos hilos y poll): %s ms\n", coma(ms_ahora() - t0, 1));
	close(c);
	close(a);
}

static void pruebas_errores(void)
{
	int u = socket(AF_INET, SOCK_STREAM, 0), c = socket(AF_INET, SOCK_STREAM, 0), p, l, l2, uno = 1, v = 0;
	struct sockaddr_in s = lo(0);
	socklen_t vl = sizeof v;

	/* un puerto atado pero donde no escucha nadie */
	bind(u, (struct sockaddr *)&s, sizeof s);
	s = lo(puerto_de(u));
	MIRA(connect(c, (struct sockaddr *)&s, sizeof s) < 0 && errno == ECONNREFUSED, "connect donde no escucha nadie: ECONNREFUSED");
	close(c);
	close(u);

	/* el que escucha, con SO_REUSEADDR (como un servidor: el TIME_WAIT de
	 * sus conexiones lo hereda, y así luego se puede volver a atar) */
	l = socket(AF_INET, SOCK_STREAM, 0);
	setsockopt(l, SOL_SOCKET, SO_REUSEADDR, &uno, sizeof uno);
	s = lo(0);
	bind(l, (struct sockaddr *)&s, sizeof s);
	listen(l, 4);
	p = puerto_de(l);
	s = lo(p);
	l2 = socket(AF_INET, SOCK_STREAM, 0);
	MIRA(bind(l2, (struct sockaddr *)&s, sizeof s) < 0 && errno == EADDRINUSE, "bind dos veces al mismo puerto: EADDRINUSE");
	MIRA(setsockopt(l2, SOL_SOCKET, SO_REUSEADDR, &uno, sizeof uno) == 0, "setsockopt SO_REUSEADDR");
	MIRA(getsockopt(l2, SOL_SOCKET, SO_REUSEADDR, &v, &vl) == 0 && v, "getsockopt SO_REUSEADDR");
	MIRA(bind(l2, (struct sockaddr *)&s, sizeof s) < 0 && errno == EADDRINUSE, "con SO_REUSEADDR, si el otro escucha: EADDRINUSE");
	close(l2);
	/* el que cierra primero se queda en TIME_WAIT: sin SO_REUSEADDR no se
	 * puede volver a atar el puerto, con SO_REUSEADDR sí */
	{
		int c2 = socket(AF_INET, SOCK_STREAM, 0), a, n1, n2;
		char b;
		connect(c2, (struct sockaddr *)&s, sizeof s);
		a = accept(l, 0, 0);
		close(a);                       /* el servidor cierra primero */
		read(c2, &b, 1);
		close(c2);
		close(l);
		n1 = socket(AF_INET, SOCK_STREAM, 0);
		MIRA(bind(n1, (struct sockaddr *)&s, sizeof s) < 0 && errno == EADDRINUSE, "con uno en TIME_WAIT, sin SO_REUSEADDR: EADDRINUSE");
		n2 = socket(AF_INET, SOCK_STREAM, 0);
		setsockopt(n2, SOL_SOCKET, SO_REUSEADDR, &uno, sizeof uno);
		MIRA(bind(n2, (struct sockaddr *)&s, sizeof s) == 0 && listen(n2, 1) == 0, "con uno en TIME_WAIT y SO_REUSEADDR: bind y listen");
		close(n1);
		close(n2);
	}
	printf("  errores: ECONNREFUSED, EADDRINUSE; SO_REUSEADDR con TIME_WAIT\n");
}

static void pruebas_udp(void)
{
	int u1 = socket(AF_INET, SOCK_DGRAM, 0), u2 = socket(AF_INET, SOCK_DGRAM, 0), p1, p2;
	struct sockaddr_in s = lo(0), d, o;
	socklen_t ol = sizeof o;
	char b[512];

	bind(u1, (struct sockaddr *)&s, sizeof s);
	bind(u2, (struct sockaddr *)&s, sizeof s);
	p1 = puerto_de(u1);
	p2 = puerto_de(u2);
	d = lo(p2);
	MIRA(recvfrom(u2, b, sizeof b, MSG_DONTWAIT, 0, 0) < 0 && errno == EAGAIN, "UDP: recvfrom con MSG_DONTWAIT sin nada: EAGAIN");
	MIRA(sendto(u1, "uno", 3, 0, (struct sockaddr *)&d, sizeof d) == 3, "UDP: sendto a 127.0.0.1");
	MIRA(sendto(u1, "dos-dos", 7, 0, (struct sockaddr *)&d, sizeof d) == 7, "UDP: otro sendto");
	MIRA(recvfrom(u2, b, sizeof b, 0, (struct sockaddr *)&o, &ol) == 3 && !memcmp(b, "uno", 3), "UDP: recvfrom, el primero entero (con sus límites)");
	MIRA(ol == sizeof o && ntohs(o.sin_port) == p1 && o.sin_addr.s_addr == htonl(INADDR_LOOPBACK), "UDP: recvfrom da quién lo manda");
	MIRA(recv(u2, b, 2, 0) == 2, "UDP: un búfer corto se lleva solo lo que cabe");
	MIRA(recv(u2, b, sizeof b, MSG_DONTWAIT) < 0 && errno == EAGAIN, "UDP: y el resto del mensaje se pierde");
	MIRA(connect(u1, (struct sockaddr *)&d, sizeof d) == 0, "UDP: connect");
	MIRA(send(u1, "tres", 4, 0) == 4 && recv(u2, b, sizeof b, 0) == 4 && !memcmp(b, "tres", 4), "UDP: send tras connect");
	ol = sizeof o;
	MIRA(getpeername(u1, (struct sockaddr *)&o, &ol) == 0 && ntohs(o.sin_port) == p2, "UDP: getpeername tras connect");
	MIRA(write(u1, "cuatro", 6) == 6 && read(u2, b, sizeof b) == 6, "UDP: write y read");
	/* sendmsg y recvmsg con dos trozos, por UDP (con su dirección) y por TCP */
	{
		struct iovec iv[2] = { { "hola, ", 6 }, { "mundo", 5 } }, ir[2];
		struct msghdr m;
		char x[3], y[64];
		int c, a, bien;
		memset(&m, 0, sizeof m);
		m.msg_name = &d;
		m.msg_namelen = sizeof d;
		m.msg_iov = iv;
		m.msg_iovlen = 2;
		MIRA(sendmsg(u1, &m, 0) == 11, "UDP: sendmsg con dos trozos (y su dirección)");
		ir[0] = (struct iovec){ x, 3 };
		ir[1] = (struct iovec){ y, sizeof y };
		memset(&m, 0, sizeof m);
		m.msg_name = &o;
		m.msg_namelen = sizeof o;
		m.msg_iov = ir;
		m.msg_iovlen = 2;
		MIRA(recvmsg(u2, &m, 0) == 11 && !memcmp(x, "hol", 3) && !memcmp(y, "a, mundo", 8) &&
		     ntohs(o.sin_port) == p1, "UDP: recvmsg en dos trozos, con la dirección");
		bien = par(&c, &a) == 0;
		memset(&m, 0, sizeof m);
		m.msg_iov = iv;
		m.msg_iovlen = 2;
		bien = bien && sendmsg(c, &m, 0) == 11;
		ir[1].iov_len = 8;              /* 3 + 8 = 11: MSG_WAITALL espera los dos trozos llenos */
		m.msg_iov = ir;
		memset(x, 0, sizeof x);
		memset(y, 0, sizeof y);
		bien = bien && recvmsg(a, &m, MSG_WAITALL) == 11 && !memcmp(x, "hol", 3) && !memcmp(y, "a, mundo", 8);
		MIRA(bien, "TCP: sendmsg y recvmsg con dos trozos");
		close(c);
		close(a);
	}
	printf("  UDP: sendto/recvfrom por 127.0.0.1:%d con quién lo manda, MSG_DONTWAIT, connect y send; sendmsg/recvmsg con dos trozos\n", p2);
	close(u1);
	close(u2);
}

static void pruebas_epoll(void)
{
	int p, l = escucha(&p), ep = epoll_create1(EPOLL_CLOEXEC), c, a;
	struct epoll_event e = { 0 }, v[4];
	struct sockaddr_in s = lo(p);
	char b[8];

	MIRA(ep >= 0, "epoll_create1(EPOLL_CLOEXEC)");
	MIRA(fcntl(ep, F_GETFD) & FD_CLOEXEC, "epoll con FD_CLOEXEC");
	e.events = EPOLLIN;
	e.data.u64 = 0x4c49;
	MIRA(epoll_ctl(ep, EPOLL_CTL_ADD, l, &e) == 0, "epoll_ctl ADD del que escucha");
	MIRA(epoll_ctl(ep, EPOLL_CTL_ADD, l, &e) < 0 && errno == EEXIST, "epoll_ctl ADD dos veces: EEXIST");
	MIRA(epoll_wait(ep, v, 4, 0) == 0, "epoll_wait sin nada: 0");
	c = socket(AF_INET, SOCK_STREAM, 0);
	connect(c, (struct sockaddr *)&s, sizeof s);
	MIRA(epoll_wait(ep, v, 4, 1000) == 1 && (v[0].events & EPOLLIN) && v[0].data.u64 == 0x4c49, "epoll_wait: EPOLLIN al llegar una conexión (con su dato)");
	a = accept(l, 0, 0);
	MIRA(epoll_wait(ep, v, 4, 0) == 0, "aceptada, ya no hay EPOLLIN");
	e.events = EPOLLIN | EPOLLOUT;
	e.data.fd = a;
	MIRA(epoll_ctl(ep, EPOLL_CTL_ADD, a, &e) == 0, "epoll_ctl ADD del conectado");
	MIRA(epoll_wait(ep, v, 4, 100) == 1 && v[0].data.fd == a && (v[0].events & EPOLLOUT) && !(v[0].events & EPOLLIN), "el conectado: EPOLLOUT y no EPOLLIN");
	e.events = EPOLLIN;
	MIRA(epoll_ctl(ep, EPOLL_CTL_MOD, a, &e) == 0, "epoll_ctl MOD a solo EPOLLIN");
	MIRA(epoll_wait(ep, v, 4, 0) == 0, "con solo EPOLLIN, nada");
	write(c, "dato", 4);
	MIRA(epoll_wait(ep, v, 4, 1000) == 1 && v[0].data.fd == a && (v[0].events & EPOLLIN), "al llegar algo: EPOLLIN");
	read(a, b, sizeof b);
	close(c);
	MIRA(epoll_wait(ep, v, 4, 1000) == 1 && (v[0].events & (EPOLLIN | EPOLLRDHUP | EPOLLHUP)), "al cerrar el otro: EPOLLIN (y read da 0)");
	MIRA(read(a, b, sizeof b) == 0, "read da 0");
	MIRA(epoll_ctl(ep, EPOLL_CTL_DEL, a, 0) == 0, "epoll_ctl DEL");
	MIRA(epoll_ctl(ep, EPOLL_CTL_DEL, a, 0) < 0 && errno == ENOENT, "epoll_ctl DEL de uno que no está: ENOENT");
	printf("  epoll: el que escucha (EPOLLIN al llegar una conexión) y el conectado (EPOLLOUT, MOD, EPOLLIN, DEL)\n");
	close(a);
	close(l);
	close(ep);
}

static void pruebas_opciones(void)
{
	int c, a, uno = 1, v, tam = 65536;
	socklen_t l;
	MIRA(par(&c, &a) == 0, "un par para las opciones");
	MIRA(setsockopt(c, IPPROTO_TCP, TCP_NODELAY, &uno, sizeof uno) == 0, "setsockopt TCP_NODELAY");
	l = sizeof v;
	MIRA(getsockopt(c, IPPROTO_TCP, TCP_NODELAY, &v, &l) == 0 && v, "getsockopt TCP_NODELAY");
	MIRA(setsockopt(c, SOL_SOCKET, SO_RCVBUF, &tam, sizeof tam) == 0, "setsockopt SO_RCVBUF");
	MIRA(setsockopt(c, SOL_SOCKET, SO_SNDBUF, &tam, sizeof tam) == 0, "setsockopt SO_SNDBUF");
	l = sizeof v;
	MIRA(getsockopt(c, SOL_SOCKET, SO_RCVBUF, &v, &l) == 0 && v > 0, "getsockopt SO_RCVBUF");
	l = sizeof v;
	MIRA(getsockopt(c, SOL_SOCKET, SO_SNDBUF, &v, &l) == 0 && v > 0, "getsockopt SO_SNDBUF");
	MIRA(setsockopt(c, SOL_SOCKET, SO_KEEPALIVE, &uno, sizeof uno) == 0, "setsockopt SO_KEEPALIVE");
	l = sizeof v;
	MIRA(getsockopt(c, SOL_SOCKET, SO_KEEPALIVE, &v, &l) == 0 && v, "getsockopt SO_KEEPALIVE");
	MIRA(write(c, "z", 1) == 1 && read(a, &v, 1) == 1, "con las opciones, sigue funcionando");
	printf("  opciones: TCP_NODELAY, SO_RCVBUF, SO_SNDBUF, SO_KEEPALIVE\n");
	close(c);
	close(a);
}

static void pruebas_nombres(void)
{
	struct addrinfo h, *r = 0;
	int e;
	memset(&h, 0, sizeof h);
	h.ai_family = AF_INET;
	h.ai_socktype = SOCK_STREAM;
	e = getaddrinfo("localhost", 0, &h, &r);
	MIRA(e == 0 && r && ((struct sockaddr_in *)r->ai_addr)->sin_addr.s_addr == htonl(INADDR_LOOPBACK),
	     "getaddrinfo(\"localhost\") = 127.0.0.1 (¿hay /etc/hosts con «127.0.0.1 localhost»?)");
	if (r)
		freeaddrinfo(r);
	r = 0;
	h.ai_flags = AI_NUMERICHOST;
	e = getaddrinfo("127.0.0.1", "80", &h, &r);
	MIRA(e == 0 && r && r->ai_family == AF_INET && ((struct sockaddr_in *)r->ai_addr)->sin_port == htons(80) &&
	     ((struct sockaddr_in *)r->ai_addr)->sin_addr.s_addr == htonl(INADDR_LOOPBACK), "getaddrinfo(\"127.0.0.1\", \"80\") con AI_NUMERICHOST");
	if (r)
		freeaddrinfo(r);
	r = 0;
	MIRA(getaddrinfo("localhost", 0, &h, &r) == EAI_NONAME, "con AI_NUMERICHOST, un nombre no vale: EAI_NONAME");
	if (r)
		freeaddrinfo(r);
	printf("  nombres: getaddrinfo de «localhost» y de «127.0.0.1»:80 con AI_NUMERICHOST\n");
}

static void pruebas_eventfd(void)
{
	int fd = eventfd(0, 0), nb = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC), se = eventfd(0, EFD_SEMAPHORE | EFD_NONBLOCK);
	uint64_t v;
	struct pollfd q = { 0, POLLIN | POLLOUT, 0 };
	MIRA(fd >= 0 && nb >= 0 && se >= 0, "eventfd (normal, EFD_NONBLOCK|EFD_CLOEXEC y EFD_SEMAPHORE)");
	v = 3;
	MIRA(write(fd, &v, 8) == 8, "eventfd: escribir 3");
	v = 4;
	MIRA(write(fd, &v, 8) == 8, "eventfd: escribir 4");
	q.fd = fd;
	MIRA(poll(&q, 1, 0) == 1 && (q.revents & POLLIN) && (q.revents & POLLOUT), "eventfd: poll con POLLIN y POLLOUT");
	v = 0;
	MIRA(read(fd, &v, 8) == 8 && v == 7, "eventfd: leer 7");
	MIRA(read(fd, &v, 4) < 0 && errno == EINVAL, "eventfd: leer menos de 8 octetos: EINVAL");
	q.fd = fd;
	q.revents = 0;
	MIRA(poll(&q, 1, 0) == 1 && !(q.revents & POLLIN), "eventfd a 0: sin POLLIN");
	MIRA(fcntl(nb, F_GETFD) & FD_CLOEXEC, "EFD_CLOEXEC");
	MIRA(read(nb, &v, 8) < 0 && errno == EAGAIN, "eventfd con EFD_NONBLOCK a 0: EAGAIN");
	v = 2;
	write(se, &v, 8);
	MIRA(read(se, &v, 8) == 8 && v == 1, "EFD_SEMAPHORE: lee 1");
	MIRA(read(se, &v, 8) == 8 && v == 1, "EFD_SEMAPHORE: lee 1 otra vez");
	MIRA(read(se, &v, 8) < 0 && errno == EAGAIN, "EFD_SEMAPHORE: y luego EAGAIN");
	printf("  eventfd: 3 + 4 = 7, EFD_NONBLOCK (EAGAIN), EFD_SEMAPHORE, poll\n");
	close(fd);
	close(nb);
	close(se);
}

static void pruebas_timerfd(void)
{
	int fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC), nb = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
	struct itimerspec t = { { 0, 20000000 }, { 0, 50000000 } }, g;
	struct pollfd q = { 0, POLLIN, 0 };
	uint64_t n = 0, n2 = 0;
	double t0;
	MIRA(fd >= 0 && nb >= 0, "timerfd_create(CLOCK_MONOTONIC)");
	MIRA(fcntl(fd, F_GETFD) & FD_CLOEXEC, "TFD_CLOEXEC");
	t0 = ms_ahora();
	MIRA(timerfd_settime(fd, 0, &t, 0) == 0, "timerfd_settime: 50 ms y luego cada 20 ms");
	MIRA(timerfd_gettime(fd, &g) == 0 && g.it_interval.tv_nsec == 20000000 && g.it_value.tv_sec == 0 &&
	     g.it_value.tv_nsec > 0 && g.it_value.tv_nsec <= 50000000, "timerfd_gettime: lo que falta y el intervalo");
	q.fd = fd;
	MIRA(poll(&q, 1, 0) == 0, "timerfd: antes de tiempo, poll sin nada");
	q.revents = 0;
	MIRA(poll(&q, 1, 2000) == 1 && (q.revents & POLLIN), "timerfd: poll lo despierta");
	MIRA(ms_ahora() - t0 >= 45, "timerfd: no antes de los 50 ms");
	MIRA(read(fd, &n, 8) == 8 && n >= 1, "timerfd: read da los vencimientos (≥1)");
	usleep(70000);
	MIRA(read(fd, &n2, 8) == 8 && n2 >= 2, "timerfd: a los 70 ms más, ≥2 vencimientos (cada 20 ms)");
	t.it_value.tv_nsec = 0;
	t.it_interval.tv_nsec = 0;
	MIRA(timerfd_settime(fd, 0, &t, &g) == 0 && g.it_interval.tv_nsec == 20000000, "timerfd_settime a 0 (parar), con el de antes");
	t.it_value.tv_sec = 10;
	timerfd_settime(nb, 0, &t, 0);
	MIRA(read(nb, &n, 8) < 0 && errno == EAGAIN, "timerfd con TFD_NONBLOCK sin vencer: EAGAIN");
	printf("  timerfd: 50 ms y cada 20 ms; poll, read (%llu y %llu vencimientos), gettime, parar\n",
	       (unsigned long long)n, (unsigned long long)n2);
	close(fd);
	close(nb);
}

static int iguales(const unsigned char *b, int n)
{
	int i;
	for (i = 1; i < n; i++)
		if (b[i] != b[0])
			return 0;
	return 1;
}

static void pruebas_azar(void)
{
	static unsigned char a[4096], b[4096], c[4096], g[64];
	int cuenta[256] = { 0 }, i, vistos = 0, fu = open("/dev/urandom", O_RDONLY | O_CLOEXEC), fr = open("/dev/random", O_RDONLY | O_CLOEXEC);
	MIRA(fu >= 0, "abrir /dev/urandom");
	MIRA(fr >= 0, "abrir /dev/random");
	MIRA(fu >= 0 && lee_todo(fu, a, sizeof a) == 0, "leer 4096 octetos de /dev/urandom");
	MIRA(fu >= 0 && lee_todo(fu, b, sizeof b) == 0, "leer otros 4096 de /dev/urandom");
	MIRA(fr >= 0 && lee_todo(fr, c, sizeof c) == 0, "leer 4096 octetos de /dev/random");
	MIRA(!iguales(a, sizeof a) && !iguales(c, sizeof c), "no todos iguales");
	MIRA(memcmp(a, b, sizeof a) && memcmp(a, c, sizeof a), "distintos en dos lecturas");
	for (i = 0; i < 4096; i++) {
		cuenta[a[i]]++;
		cuenta[b[i]]++;
	}
	for (i = 0; i < 256; i++)
		vistos += cuenta[i] > 0;
	MIRA(vistos == 256, "en 8192 octetos de /dev/urandom salen los 256 valores");
	memset(cuenta, 0, sizeof cuenta);
	for (vistos = i = 0; i < 4096; i++)
		cuenta[c[i]]++;
	for (i = 0; i < 256; i++)
		vistos += cuenta[i] > 0;
	MIRA(vistos >= 250, "en 4096 octetos de /dev/random salen (casi) los 256 valores");
	MIRA(getrandom(g, sizeof g, 0) == sizeof g && !iguales(g, sizeof g), "getrandom de 64 octetos");
	MIRA(getrandom(g, 16, GRND_NONBLOCK) == 16, "getrandom con GRND_NONBLOCK");
	printf("  azar: /dev/urandom, /dev/random (4096 octetos, los 256 valores) y getrandom\n");
	close(fu);
	close(fr);
}

/* [etapa 10] /etc/localtime: musl la lee si no hay TZ */
static void pruebas_localtime(void)
{
	char b[512], tz[80], zv[72], z1[16], z2[16];
	const char *e = getenv("TZ");
	int f = open("/etc/localtime", O_RDONLY), n, i, igual = 1;
	time_t t;
	struct tm a, c;
	MIRA(f >= 0, "abrir /etc/localtime");
	n = f >= 0 ? (int)read(f, b, sizeof b - 1) : 0;
	if (f >= 0) close(f);
	b[n > 0 ? n : 0] = 0;
	snprintf(tz, sizeof tz, "\n%s\n", e ? e : "");
	snprintf(zv, sizeof zv, "%s", e ? e : "");
	MIRA(n > 60 && !memcmp(b, "TZif2", 5) && e && n > (int)strlen(tz) && !memcmp(b + n - strlen(tz), tz, strlen(tz)), "/etc/localtime: TZif 2, con la zona de TZ al final");
	/* el 15/1 y el 1/7 de 2026 a las 12:00 UTC, con TZ y sin ella */
	for (i = 0; i < 2 && e; i++) {
		t = i ? 1782907200 : 1768478400;
		localtime_r(&t, &a);
		strftime(z1, sizeof z1, "%H:%M %Z", &a);
		unsetenv("TZ");
		tzset();
		localtime_r(&t, &c);
		strftime(z2, sizeof z2, "%H:%M %Z", &c);
		setenv("TZ", zv, 1);
		tzset();
		if (strcmp(z1, z2) || a.tm_gmtoff != c.tm_gmtoff) igual = 0;
		if (i) printf("  /etc/localtime: el 1/7/2026 a las 12:00 UTC son las %s (con TZ, %s)\n", z2, z1);
	}
	MIRA(e && igual, "localtime sin TZ (de /etc/localtime) da lo mismo que con TZ, en invierno y en verano");
}

static void parado(int s)
{
	static const char m[] = "  MAL: MRED lleva 120 s parado en una llamada (SIGALRM)\nMRED: mal (parado)\n";
	(void)s;
	write(1, m, sizeof m - 1);
	_exit(1);
}

int main(void)
{
	double t0 = ms_ahora();
	signal(SIGALRM, parado);
	alarm(120);
	printf("MRED: las llamadas de red, por 127.0.0.1 (sin tarjeta), eventfd, timerfd y el azar; %s\n", cómo());
	fflush(stdout);
	pruebas_sockets();
	pruebas_tcp();
	pruebas_grande();
	pruebas_errores();
	pruebas_udp();
	pruebas_epoll();
	pruebas_opciones();
	pruebas_nombres();
	pruebas_eventfd();
	pruebas_timerfd();
	pruebas_azar();
	pruebas_localtime();
	printf("  en %s ms\n", coma(ms_ahora() - t0, 0));
	return acaba("MRED");
}
