/* Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * nucleo64/programas/red/mred6.c - Hispa-64, etapa 10 c (docs/64/HISPA-64.md,
 * sección 20): MRED6, la prueba de IPv6, como MRED: una línea por grupo y,
 * al final, «MRED6: todo bien (N pruebas)» y código 0 (o «MAL: ...» y 1).
 *
 *   MRED6              por ::1, sin tarjeta: socket(AF_INET6); TCP por ::1
 *                      (bind, listen, connect, accept, getpeername, 200 KB
 *                      de ida y vuelta); los dos a la vez (uno de IPv6 en
 *                      [::] recibe a los de IPv4, como ::ffff:127.0.0.1) e
 *                      IPV6_V6ONLY (ya no); UDP por ::1 y a ::ffff:127.0.0.1;
 *                      el puerto inalcanzable de ICMPv6 (ECONNREFUSED); el
 *                      ping de ICMPv6 (IPPROTO_ICMPV6) a ::1; inet_pton,
 *                      inet_ntop y getaddrinfo de «::1»; los errores
 *                      (AF_INET con una de IPv6, EAFNOSUPPORT).
 *   MRED6 red NOMBRE [PUERTO]
 *                      con la red: getaddrinfo(NOMBRE, AF_INET6) (el AAAA del
 *                      DNS), tres pings de ICMPv6 a lo que dé y, con PUERTO,
 *                      un GET de HTTP por TCP sobre IPv6 (lo que llega, con
 *                      su tamaño).
 * Corre igual en Linux. */
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <time.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/icmp6.h>
#include <arpa/inet.h>
#include <sys/uio.h>
#include "../musl/comun.h"

#define MUCHO (200 * 1024)

static struct sockaddr_in6 dir6(const char *ip, int puerto)
{
	struct sockaddr_in6 s;
	memset(&s, 0, sizeof s);
	s.sin6_family = AF_INET6;
	s.sin6_port = htons(puerto);
	inet_pton(AF_INET6, ip, &s.sin6_addr);
	return s;
}

static struct sockaddr_in dir4(const char *ip, int puerto)
{
	struct sockaddr_in s;
	memset(&s, 0, sizeof s);
	s.sin_family = AF_INET;
	s.sin_port = htons(puerto);
	inet_pton(AF_INET, ip, &s.sin_addr);
	return s;
}

static int puerto_de(int fd)
{
	struct sockaddr_in6 s;
	socklen_t l = sizeof s;
	if (getsockname(fd, (struct sockaddr *)&s, &l) < 0) return -1;
	return ntohs(s.sin6_port);
}

/* un socket que escucha en IP (de IPv6), con V6ONLY si se pide; su puerto en *p */
static int escucha6(const char *ip, int v6solo, int *p)
{
	int fd = socket(AF_INET6, SOCK_STREAM, 0), uno = 1;
	struct sockaddr_in6 s = dir6(ip, 0);
	if (fd < 0) return -1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &uno, sizeof uno);
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &v6solo, sizeof v6solo) < 0) { close(fd); return -1; }
	if (bind(fd, (struct sockaddr *)&s, sizeof s) < 0 || listen(fd, 4) < 0) { close(fd); return -1; }
	*p = puerto_de(fd);
	return fd;
}

static unsigned char ida[MUCHO], vuelta[MUCHO];
static int eco_fd;

/* el otro lado: lo que le llega, de vuelta */
static void *eco(void *a)
{
	char t[8192];
	ssize_t r;
	(void)a;
	while ((r = recv(eco_fd, t, sizeof t, 0)) > 0) {
		ssize_t h = 0;
		while (h < r) {
			ssize_t w = send(eco_fd, t + h, r - h, MSG_NOSIGNAL);
			if (w <= 0) return 0;
			h += w;
		}
	}
	shutdown(eco_fd, SHUT_WR);
	return 0;
}

static long ms_desde(struct timespec *t0)
{
	struct timespec t1;
	clock_gettime(CLOCK_MONOTONIC, &t1);
	return (t1.tv_sec - t0->tv_sec) * 1000 + (t1.tv_nsec - t0->tv_nsec) / 1000000;
}

/* un ping de ICMPv6 a s (IPPROTO_ICMPV6, SOCK_DGRAM): el tiempo en µs, o -1 */
static long ping6(struct sockaddr_in6 *s, int seq)
{
	int fd = socket(AF_INET6, SOCK_DGRAM, IPPROTO_ICMPV6);
	unsigned char p[64], r[128];
	struct pollfd pf;
	struct timespec t0, t1;
	ssize_t n;
	if (fd < 0) return errno == EACCES || errno == EPERM ? -2 : -1;      /* (Linux: ping_group_range) */
	memset(p, 0, sizeof p);
	p[0] = ICMP6_ECHO_REQUEST;
	p[6] = seq >> 8;
	p[7] = seq & 0xFF;
	memcpy(p + 8, "Hispa-64 y IPv6", 15);
	clock_gettime(CLOCK_MONOTONIC, &t0);
	if (sendto(fd, p, 32, 0, (struct sockaddr *)s, sizeof *s) != 32) { close(fd); return -1; }
	pf.fd = fd;
	pf.events = POLLIN;
	for (;;) {
		if (poll(&pf, 1, 3000) != 1) { close(fd); return -1; }
		n = recv(fd, r, sizeof r, 0);
		if (n >= 32 && r[0] == ICMP6_ECHO_REPLY && r[6] == p[6] && r[7] == p[7] && memcmp(r + 8, p + 8, 15) == 0) break;
	}
	clock_gettime(CLOCK_MONOTONIC, &t1);
	close(fd);
	return (t1.tv_sec - t0.tv_sec) * 1000000 + (t1.tv_nsec - t0.tv_nsec) / 1000;
}

static int por_lo(void)
{
	int l, c, a, p, r, k, uno = 1, x;
	socklen_t sl;
	struct sockaddr_in6 s6, de6;
	struct sockaddr_in s4;
	char ip[INET6_ADDRSTRLEN], t[64];
	pthread_t h;
	size_t hecho, vuelto;
	struct timespec t0;
	struct addrinfo pista, *res;

	printf("MRED6: IPv6 por ::1, sin tarjeta\n");
	/* lo de C: inet_pton, inet_ntop, getaddrinfo */
	MIRA(inet_pton(AF_INET6, "fe80::5054:ff:fe12:3456", &s6.sin6_addr) == 1 && inet_ntop(AF_INET6, &s6.sin6_addr, ip, sizeof ip)
	     && strcmp(ip, "fe80::5054:ff:fe12:3456") == 0, "inet_pton e inet_ntop de una de IPv6");
	memset(&pista, 0, sizeof pista);
	pista.ai_family = AF_INET6;
	pista.ai_socktype = SOCK_STREAM;
	pista.ai_flags = AI_NUMERICHOST;
	res = 0;
	MIRA(getaddrinfo("::1", "80", &pista, &res) == 0 && res && res->ai_family == AF_INET6 && res->ai_addrlen == sizeof(struct sockaddr_in6)
	     && ntohs(((struct sockaddr_in6 *)res->ai_addr)->sin6_port) == 80, "getaddrinfo de «::1»");
	if (res) freeaddrinfo(res);
	printf("  inet_pton, inet_ntop y getaddrinfo de «::1»\n");

	/* TCP por ::1 */
	l = escucha6("::1", 1, &p);
	MIRA(l >= 0 && p > 0, "socket(AF_INET6, SOCK_STREAM), bind a [::1] y listen");
	c = socket(AF_INET6, SOCK_STREAM, 0);
	s6 = dir6("::1", p);
	clock_gettime(CLOCK_MONOTONIC, &t0);
	MIRA(c >= 0 && connect(c, (struct sockaddr *)&s6, sizeof s6) == 0, "connect a [::1]");
	sl = sizeof de6;
	a = accept(l, (struct sockaddr *)&de6, &sl);
	MIRA(a >= 0 && sl == sizeof de6 && de6.sin6_family == AF_INET6 && memcmp(&de6.sin6_addr, &in6addr_loopback, 16) == 0, "accept: el otro es ::1 (sockaddr_in6)");
	sl = sizeof de6;
	MIRA(getpeername(c, (struct sockaddr *)&de6, &sl) == 0 && ntohs(de6.sin6_port) == p && memcmp(&de6.sin6_addr, &in6addr_loopback, 16) == 0, "getpeername: [::1] y su puerto");
	sl = sizeof x;
	x = 0;
	MIRA(getsockopt(c, SOL_SOCKET, SO_DOMAIN, &x, &sl) == 0 && x == AF_INET6, "SO_DOMAIN: AF_INET6");
	for (k = 0; k < MUCHO; k++) ida[k] = (unsigned char)(k * 7 + k / 251);
	eco_fd = a;
	pthread_create(&h, 0, eco, 0);
	hecho = 0;
	vuelto = 0;
	while (vuelto < MUCHO) {
		struct pollfd pf = { c, POLLIN | (hecho < MUCHO ? POLLOUT : 0), 0 };
		if (poll(&pf, 1, 10000) != 1) break;
		if ((pf.revents & POLLOUT) && hecho < MUCHO) {
			ssize_t w = send(c, ida + hecho, MUCHO - hecho > 16384 ? 16384 : MUCHO - hecho, MSG_DONTWAIT);
			if (w > 0) hecho += w;
			if (hecho == MUCHO) shutdown(c, SHUT_WR);
		}
		if (pf.revents & (POLLIN | POLLHUP)) {
			ssize_t q = recv(c, vuelta + vuelto, MUCHO - vuelto, MSG_DONTWAIT);
			if (q == 0) break;
			if (q > 0) vuelto += q;
		}
	}
	pthread_join(h, 0);
	MIRA(vuelto == MUCHO && memcmp(ida, vuelta, MUCHO) == 0, "200 KB por TCP sobre ::1, de ida y vuelta, iguales");
	printf("  TCP por [::1]:%d: connect, accept, getpeername, 200 KB de ida y vuelta en %ld ms\n", p, ms_desde(&t0));
	close(c);
	close(a);
	close(l);

	/* los dos a la vez: uno de IPv6 en [::] (sin V6ONLY) recibe a uno de IPv4 */
	l = escucha6("::", 0, &p);
	MIRA(l >= 0, "un socket de IPv6 en [::] sin IPV6_V6ONLY");
	c = socket(AF_INET, SOCK_STREAM, 0);
	s4 = dir4("127.0.0.1", p);
	MIRA(connect(c, (struct sockaddr *)&s4, sizeof s4) == 0, "connect de IPv4 a 127.0.0.1 (lo recibe el de IPv6)");
	sl = sizeof de6;
	a = accept(l, (struct sockaddr *)&de6, &sl);
	inet_ntop(AF_INET6, &de6.sin6_addr, ip, sizeof ip);
	MIRA(a >= 0 && de6.sin6_family == AF_INET6 && strcmp(ip, "::ffff:127.0.0.1") == 0, "accept: el de IPv4, como ::ffff:127.0.0.1");
	MIRA(a >= 0 && send(a, "hola", 4, 0) == 4 && recv(c, t, sizeof t, 0) == 4 && memcmp(t, "hola", 4) == 0, "y se hablan");
	printf("  los dos a la vez: uno de IPv6 en [::] recibe a 127.0.0.1 como %s\n", ip);
	close(c);
	if (a >= 0) close(a);
	close(l);
	l = escucha6("::", 1, &p);
	sl = sizeof x;
	x = 0;
	MIRA(l >= 0 && getsockopt(l, IPPROTO_IPV6, IPV6_V6ONLY, &x, &sl) == 0 && x == 1, "IPV6_V6ONLY puesto (getsockopt)");
	c = socket(AF_INET, SOCK_STREAM, 0);
	s4 = dir4("127.0.0.1", p);
	errno = 0;
	r = connect(c, (struct sockaddr *)&s4, sizeof s4);
	MIRA(r < 0 && errno == ECONNREFUSED, "con IPV6_V6ONLY, el de IPv4 no llega (ECONNREFUSED)");
	printf("  IPV6_V6ONLY: el de IPv4, rechazado\n");
	close(c);
	close(l);

	/* UDP por ::1 y a ::ffff:127.0.0.1 */
	l = socket(AF_INET6, SOCK_DGRAM, 0);
	s6 = dir6("::1", 0);
	MIRA(l >= 0 && bind(l, (struct sockaddr *)&s6, sizeof s6) == 0, "UDP: bind a [::1]");
	p = puerto_de(l);
	c = socket(AF_INET6, SOCK_DGRAM, 0);
	s6 = dir6("::1", p);
	MIRA(sendto(c, "uno de IPv6", 11, 0, (struct sockaddr *)&s6, sizeof s6) == 11, "sendto a [::1]");
	sl = sizeof de6;
	r = recvfrom(l, t, sizeof t, 0, (struct sockaddr *)&de6, &sl);
	MIRA(r == 11 && memcmp(t, "uno de IPv6", 11) == 0 && de6.sin6_family == AF_INET6 && memcmp(&de6.sin6_addr, &in6addr_loopback, 16) == 0
	     && ntohs(de6.sin6_port) == puerto_de(c), "recvfrom: lo de ::1, con su puerto");
	close(l);
	l = socket(AF_INET, SOCK_DGRAM, 0);
	s4 = dir4("127.0.0.1", 0);
	bind(l, (struct sockaddr *)&s4, sizeof s4);
	sl = sizeof s4;
	getsockname(l, (struct sockaddr *)&s4, &sl);
	s6 = dir6("::ffff:127.0.0.1", ntohs(s4.sin_port));
	MIRA(sendto(c, "a uno de IPv4", 13, 0, (struct sockaddr *)&s6, sizeof s6) == 13, "sendto de uno de IPv6 a ::ffff:127.0.0.1");
	{
		struct sockaddr_in d4;
		sl = sizeof d4;
		r = recvfrom(l, t, sizeof t, 0, (struct sockaddr *)&d4, &sl);
		MIRA(r == 13 && memcmp(t, "a uno de IPv4", 13) == 0 && d4.sin_family == AF_INET && d4.sin_addr.s_addr == htonl(INADDR_LOOPBACK), "lo recibe el de IPv4, de 127.0.0.1");
	}
	close(l);
	/* el puerto inalcanzable: ICMPv6 de vuelta, y el conectado lo dice */
	s6 = dir6("::1", 9);
	MIRA(connect(c, (struct sockaddr *)&s6, sizeof s6) == 0 && send(c, "x", 1, 0) == 1, "UDP conectado a [::1]:9");
	usleep(100000);
	errno = 0;
	r = recv(c, t, sizeof t, MSG_DONTWAIT);
	MIRA(r < 0 && errno == ECONNREFUSED, "el puerto inalcanzable de ICMPv6: ECONNREFUSED");
	close(c);
	printf("  UDP por [::1] y a ::ffff:127.0.0.1; el puerto inalcanzable (ICMPv6): ECONNREFUSED\n");

	/* el ping de ICMPv6 */
	s6 = dir6("::1", 0);
	{
		long us = ping6(&s6, 1);
		if (us != -2) MIRA(us >= 0, "el ping de ICMPv6 (IPPROTO_ICMPV6) a ::1");
		printf("  ping de ICMPv6 a ::1: %s\n", us >= 0 ? "contesta" : us == -2 ? "sin permiso (ping_group_range)" : "no contesta");
	}

	/* los errores */
	c = socket(AF_INET, SOCK_STREAM, 0);
	s6 = dir6("::1", 80);
	errno = 0;
	r = connect(c, (struct sockaddr *)&s6, sizeof s6);
	MIRA(r < 0 && (errno == EAFNOSUPPORT || errno == EINVAL), "uno de IPv4 con una dirección de IPv6: EAFNOSUPPORT");
	close(c);
	c = socket(AF_INET6, SOCK_STREAM, 0);
	setsockopt(c, IPPROTO_IPV6, IPV6_V6ONLY, &uno, sizeof uno);
	s6 = dir6("::ffff:127.0.0.1", 80);
	errno = 0;
	r = connect(c, (struct sockaddr *)&s6, sizeof s6);
	MIRA(r < 0 && errno == ENETUNREACH, "con IPV6_V6ONLY, a ::ffff:127.0.0.1: ENETUNREACH");
	close(c);
	/* (auditoría H2) sendmsg con trozos que dan la vuelta al sumarlos: un error, sin estropear el núcleo */
	{
		static char b[8];
		struct iovec v[2] = { { b, (size_t)-1 }, { b, 2 } };
		struct msghdr mh;
		c = socket(AF_INET6, SOCK_DGRAM, 0);
		s6 = dir6("::1", 9);
		memset(&mh, 0, sizeof mh);
		mh.msg_name = &s6;
		mh.msg_namelen = sizeof s6;
		mh.msg_iov = v;
		mh.msg_iovlen = 2;
		errno = 0;
		r = sendmsg(c, &mh, 0);
		MIRA(r < 0 && (errno == EMSGSIZE || errno == EINVAL || errno == EFAULT), "sendmsg con trozos que dan la vuelta: un error (y el núcleo sigue)");
		close(c);
	}
	printf("  los errores: EAFNOSUPPORT, ENETUNREACH; sendmsg con trozos que dan la vuelta, rechazado\n");
	return acaba("MRED6");
}

static int con_red(const char *nombre, const char *puerto)
{
	struct addrinfo pista, *res = 0;
	struct sockaddr_in6 s6;
	char ip[INET6_ADDRSTRLEN], t[4096];
	int k, bien = 0, fd;
	long us, máx = 0, total;
	struct timespec t0;

	printf("MRED6: IPv6 con la red: %s\n", nombre);
	memset(&pista, 0, sizeof pista);
	pista.ai_family = AF_INET6;
	pista.ai_socktype = SOCK_STREAM;
	k = getaddrinfo(nombre, puerto ? puerto : "80", &pista, &res);
	MIRA(k == 0 && res && res->ai_family == AF_INET6, "getaddrinfo con AF_INET6 (el AAAA del DNS)");
	if (k != 0 || !res) { printf("  getaddrinfo: %s\n", gai_strerror(k)); return acaba("MRED6"); }
	s6 = *(struct sockaddr_in6 *)res->ai_addr;
	freeaddrinfo(res);
	inet_ntop(AF_INET6, &s6.sin6_addr, ip, sizeof ip);
	printf("  %s es %s (AAAA)\n", nombre, ip);
	/* (hasta que haya camino: el anuncio del router, tras el enlace) */
	for (k = 0; k < 100; k++) {
		int u = socket(AF_INET6, SOCK_DGRAM, 0), r = connect(u, (struct sockaddr *)&s6, sizeof s6);
		close(u);
		if (r == 0) break;
		usleep(100000);
	}
	for (k = 1; k <= 3; k++) {
		us = ping6(&s6, k);
		if (us >= 0) { bien++; if (us > máx) máx = us; }
	}
	MIRA(bien == 3, "tres pings de ICMPv6");
	printf("  ping de ICMPv6 a %s: 3 enviados, %d recibidos (máx = %ld µs)\n", ip, bien, máx);
	if (puerto) {
		clock_gettime(CLOCK_MONOTONIC, &t0);
		fd = socket(AF_INET6, SOCK_STREAM, 0);
		MIRA(fd >= 0 && connect(fd, (struct sockaddr *)&s6, sizeof s6) == 0, "connect por TCP sobre IPv6");
		snprintf(t, sizeof t, "GET /a.bin HTTP/1.0\r\nHost: [%s]\r\n\r\n", ip);
		MIRA(send(fd, t, strlen(t), 0) == (ssize_t)strlen(t), "el GET");
		total = 0;
		for (;;) {
			ssize_t r = recv(fd, t, sizeof t, 0);
			if (r <= 0) break;
			total += r;
		}
		close(fd);
		MIRA(total > 1048576, "lo que llega (más de 1 MB, con la cabecera)");
		printf("  HTTP por TCP sobre IPv6, [%s]:%s: %ld octetos en %ld ms\n", ip, puerto, total, ms_desde(&t0));
	}
	return acaba("MRED6");
}

int main(int argc, char **argv)
{
	if (argc >= 3 && strcmp(argv[1], "red") == 0) return con_red(argv[2], argc >= 4 ? argv[3] : 0);
	return por_lo();
}
