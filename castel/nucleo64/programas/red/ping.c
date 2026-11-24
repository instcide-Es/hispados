/* Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * nucleo64/programas/red/ping.c - Hispa-64, etapa 9: PING, el eco de ICMP.
 *   PING [-c N] [-s OCTETOS] [-W ms] [-i ms] DESTINO
 * DESTINO, un nombre o una IP. -c: cuántos (4 por omisión); -s: los octetos
 * de datos (56: 64 con la cabecera de ICMP); -W: lo que se espera cada
 * respuesta (1000 ms); -i: cada cuánto se manda (1000 ms).
 * Primero con el «ping socket» (socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP):
 * el núcleo pone el identificador y quita la cabecera IP); si no se deja
 * (EACCES, EPERM, EPROTONOSUPPORT), con SOCK_RAW (llega con la cabecera IP).
 * Código 0 si llegó alguna respuesta; 1 si no; 2 si no se puede. */
#include <unistd.h>
#include <poll.h>
#include <stdint.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include "red.h"

#define MAX_DATOS 65000
#define MAX_N 100000

static uint16_t suma(const void *d, size_t n)
{
	const uint8_t *p = d;
	uint32_t s = 0;
	while (n > 1) {
		s += (p[0] << 8) | p[1];
		p += 2;
		n -= 2;
	}
	if (n)
		s += p[0] << 8;
	while (s >> 16)
		s = (s & 0xffff) + (s >> 16);
	return htons(~s & 0xffff);
}

static void uso(void)
{
	printf("uso: PING [-c N] [-s OCTETOS] [-W ms] [-i ms] DESTINO\n");
	exit(2);
}

int main(int argc, char **argv)
{
	int n = 4, datos = 56, espera = 1000, intervalo = 1000, i, fd, crudo = 0;
	int enviados = 0, recibidos = 0, uno = 1;
	const char *destino = 0;
	struct sockaddr_in sa;
	static uint8_t pq[8 + MAX_DATOS + 128], rb[70000];
	static double salida[MAX_N + 1];
	static char visto[MAX_N + 1];
	double mín = 1e18, máx = 0, total = 0;
	uint16_t id = getpid() & 0xffff;
	char ip[INET_ADDRSTRLEN];

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-c") && i + 1 < argc) n = atoi(argv[++i]);
		else if (!strcmp(argv[i], "-s") && i + 1 < argc) datos = atoi(argv[++i]);
		else if (!strcmp(argv[i], "-W") && i + 1 < argc) espera = atoi(argv[++i]);
		else if (!strcmp(argv[i], "-i") && i + 1 < argc) intervalo = atoi(argv[++i]);
		else if (argv[i][0] == '-') uso();
		else destino = argv[i];
	}
	if (!destino || n < 1 || n > MAX_N || datos < 0 || datos > MAX_DATOS || espera < 1 || intervalo < 0)
		uso();
	if (busca("PING", destino, 0, SOCK_RAW, &sa))
		return 2;
	inet_ntop(AF_INET, &sa.sin_addr, ip, sizeof ip);

	fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
	if (fd < 0 && (errno == EACCES || errno == EPERM || errno == EPROTONOSUPPORT)) {
		int e = errno;
		fd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
		if (fd < 0) {
			printf("PING: el sistema no deja mandar ICMP: ni con el ping socket (%s; en Linux, "
			       "net.ipv4.ping_group_range) ni con SOCK_RAW (%s; hace falta ser root)\n",
			       strerror(e), strerror(errno));
			return 2;
		}
		crudo = 1;
	}
	if (fd < 0) {
		printf("PING: no se abre el socket de ICMP: %s\n", strerror(errno));
		return 2;
	}
	/* el ttl de cada respuesta, en un mensaje de control de recvmsg */
	setsockopt(fd, IPPROTO_IP, IP_RECVTTL, &uno, sizeof uno);
	printf("PING %s (%s): %d octetos de datos, %s\n", destino, ip, datos,
	       crudo ? "con SOCK_RAW" : "con el ping socket");
	fflush(stdout);

	for (i = 1; i <= n; i++) {
		struct icmphdr *h = (struct icmphdr *)pq;
		double t0, límite, próximo;
		int k;
		memset(pq, 0, 8);
		h->type = ICMP_ECHO;
		h->code = 0;
		h->un.echo.id = htons(id);
		h->un.echo.sequence = htons(i & 0xffff);
		for (k = 0; k < datos; k++)
			pq[8 + k] = k & 0xff;
		h->checksum = 0;
		h->checksum = suma(pq, 8 + datos);
		t0 = ms_ahora();
		salida[i] = t0;
		if (sendto(fd, pq, 8 + datos, 0, (struct sockaddr *)&sa, sizeof sa) < 0) {
			printf("PING: no sale el n.º %d: %s\n", i, strerror(errno));
			fflush(stdout);
		} else
			enviados++;
		límite = t0 + espera;
		próximo = t0 + intervalo;
		for (;;) {
			double ahora = ms_ahora(), fin = (i == n) ? límite : (límite < próximo ? próximo : límite);
			struct pollfd p = { fd, POLLIN, 0 };
			struct msghdr m;
			struct iovec v = { rb, sizeof rb };
			char ctl[256];
			struct cmsghdr *c;
			struct icmphdr *r;
			int ttl = -1, lon, ihl = 0;
			ssize_t q;
			uint16_t sec;
			if (i < n && ahora >= próximo && (ahora >= límite || visto[i]))
				break;
			if (ahora >= fin)
				break;
			if (poll(&p, 1, (int)(fin - ahora) + 1) <= 0)
				continue;
			memset(&m, 0, sizeof m);
			m.msg_iov = &v;
			m.msg_iovlen = 1;
			m.msg_control = ctl;
			m.msg_controllen = sizeof ctl;
			q = recvmsg(fd, &m, 0);
			if (q <= 0)
				continue;
			for (c = CMSG_FIRSTHDR(&m); c; c = CMSG_NXTHDR(&m, c))
				if (c->cmsg_level == IPPROTO_IP && c->cmsg_type == IP_TTL)
					ttl = *(int *)CMSG_DATA(c);
			if (crudo) {
				struct iphdr *iph = (struct iphdr *)rb;
				if (q < 20)
					continue;
				ihl = iph->ihl * 4;
				if (ttl < 0)
					ttl = iph->ttl;
			}
			lon = q - ihl;
			if (lon < 8)
				continue;
			r = (struct icmphdr *)(rb + ihl);
			if (r->type != ICMP_ECHOREPLY || (crudo && ntohs(r->un.echo.id) != id))
				continue;
			sec = ntohs(r->un.echo.sequence);
			for (k = (i > 65535 ? i - 65535 : 1); k <= i; k++)
				if ((k & 0xffff) == sec)
					break;
			if (k > i || visto[k])
				continue;
			visto[k] = 1;
			recibidos++;
			{
				double t = ms_ahora() - salida[k];
				total += t;
				if (t < mín) mín = t;
				if (t > máx) máx = t;
				if (ttl >= 0)
					printf("%d octetos de %s: n.º %d, ttl=%d, %s ms\n", lon, ip, k, ttl, coma(t, 2));
				else
					printf("%d octetos de %s: n.º %d, %s ms\n", lon, ip, k, coma(t, 2));
				fflush(stdout);
			}
		}
	}
	printf("PING %s: %d enviados, %d recibidos, %d %% perdidos", ip, enviados, recibidos,
	       enviados ? (enviados - recibidos) * 100 / enviados : 100);
	if (recibidos)
		printf("; tiempo mín/media/máx = %s/%s/%s ms", coma(mín, 2), coma(total / recibidos, 2), coma(máx, 2));
	printf("\n");
	close(fd);
	return recibidos ? 0 : 1;
}
