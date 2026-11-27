/* Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * nucleo64/programas/red/eco.c - Hispa-64, etapa 9: ECO, el eco por TCP.
 *   ECO servidor PUERTO [VECES]
 *       escucha en 0.0.0.0:PUERTO (0: el que dé el sistema; lo dice),
 *       acepta VECES conexiones (1 por omisión), una tras otra, y devuelve
 *       lo que llega hasta que la otra punta cierra
 *   ECO cliente EQUIPO PUERTO OCTETOS
 *       manda OCTETOS octetos de un patrón conocido (a trozos, leyendo a la
 *       vez con poll), cierra su lado de escribir (shutdown SHUT_WR), lee
 *       todo lo que vuelve y mira que sea igual
 * Código 0 si bien. */
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <netinet/tcp.h>
#include "red.h"

#define PLAZO_MS 30000

static unsigned char patrón(long long i)
{
	return (unsigned char)(i * 31 + (i >> 8) * 7 + (i >> 16));
}

static int servidor(int puerto, int veces)
{
	int fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0), uno = 1, k;
	struct sockaddr_in sa;
	socklen_t l = sizeof sa;
	long long todo = 0;
	static unsigned char b[65536];

	/* [etapa 10 e] un cliente que se va sin leer (EPIPE) no mata al servidor:
	 * esa conexión no cuenta y se espera otra */
	signal(SIGPIPE, SIG_IGN);
	if (fd < 0) {
		printf("ECO: no se abre el socket: %s\n", strerror(errno));
		return 1;
	}
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &uno, sizeof uno);
	memset(&sa, 0, sizeof sa);
	sa.sin_family = AF_INET;
	sa.sin_port = htons(puerto);
	sa.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(fd, (struct sockaddr *)&sa, sizeof sa) < 0 || listen(fd, 8) < 0) {
		printf("ECO: no se puede escuchar en el %d: %s\n", puerto, strerror(errno));
		return 1;
	}
	getsockname(fd, (struct sockaddr *)&sa, &l);
	printf("ECO: escuchando en 0.0.0.0:%d\n", ntohs(sa.sin_port));
	fflush(stdout);
	for (k = 1; k <= veces; k++) {
		struct sockaddr_in o;
		socklen_t lo = sizeof o;
		char ip[INET_ADDRSTRLEN];
		long long n = 0;
		int rota = 0;
		int c = accept(fd, (struct sockaddr *)&o, &lo);
		if (c < 0) {
			if (errno == EINTR) {
				k--;
				continue;
			}
			printf("ECO: accept: %s\n", strerror(errno));
			return 1;
		}
		inet_ntop(AF_INET, &o.sin_addr, ip, sizeof ip);
		for (;;) {
			struct pollfd p = { c, POLLIN, 0 };
			ssize_t r, w, h;
			int e = poll(&p, 1, PLAZO_MS);
			if (e == 0) {
				printf("ECO: conexión %d: no llega nada en %d s; se corta\n", k, PLAZO_MS / 1000);
				break;
			}
			if (e < 0) {
				if (errno == EINTR)
					continue;
				break;
			}
			r = read(c, b, sizeof b);
			if (r < 0 && errno == EINTR)
				continue;
			if (r <= 0)
				break;
			for (h = 0; h < r; h += w) {
				w = write(c, b + h, r - h);
				if (w < 0) {
					if (errno == EINTR) {
						w = 0;
						continue;
					}
					printf("ECO: conexión %d: write: %s (cortada por el otro lado: no cuenta)\n", k, strerror(errno));
					rota = 1;
					goto fin;
				}
			}
			n += r;
		}
fin:
		close(c);
		if (rota) {
			k--;
			continue;
		}
		printf("ECO: conexión %d de %s:%d: %lld octetos devueltos\n", k, ip, ntohs(o.sin_port), n);
		fflush(stdout);
		todo += n;
	}
	close(fd);
	printf("ECO: servidor: %d conexiones, %lld octetos devueltos\n", veces, todo);
	return 0;
}

static int cliente(const char *equipo, int puerto, long long octetos)
{
	struct sockaddr_in sa;
	int fd, uno = 1, cerrado_escribir = 0;
	long long ido = 0, vuelto = 0, distintos = -1;
	double t0;
	static unsigned char b[65536];
	char ip[INET_ADDRSTRLEN];

	if (busca("ECO", equipo, puerto, SOCK_STREAM, &sa))
		return 2;
	inet_ntop(AF_INET, &sa.sin_addr, ip, sizeof ip);
	fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
	/* [etapa 10 e] un cliente que se va sin leer (EPIPE) no mata al servidor:
	 * esa conexión no cuenta y se espera otra */
	signal(SIGPIPE, SIG_IGN);
	if (fd < 0) {
		printf("ECO: no se abre el socket: %s\n", strerror(errno));
		return 1;
	}
	t0 = ms_ahora();
	if (connect(fd, (struct sockaddr *)&sa, sizeof sa) < 0) {
		printf("ECO: no se conecta con %s:%d: %s\n", ip, puerto,
		       errno == ECONNREFUSED ? "rechazada (no escucha nadie)" : strerror(errno));
		return 3;
	}
	setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &uno, sizeof uno);
	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
	for (;;) {
		struct pollfd p = { fd, POLLIN, 0 };
		int e;
		if (ido == octetos && !cerrado_escribir) {
			shutdown(fd, SHUT_WR);
			cerrado_escribir = 1;
		}
		if (!cerrado_escribir)
			p.events |= POLLOUT;
		e = poll(&p, 1, PLAZO_MS);
		if (e == 0) {
			printf("ECO: no llega nada en %d s (ida %lld, vuelta %lld)\n", PLAZO_MS / 1000, ido, vuelto);
			return 4;
		}
		if (e < 0) {
			if (errno == EINTR)
				continue;
			printf("ECO: poll: %s\n", strerror(errno));
			return 4;
		}
		if ((p.revents & POLLOUT) && !cerrado_escribir) {
			long long q = octetos - ido, i;
			ssize_t w;
			if (q > (long long)sizeof b)
				q = sizeof b;
			for (i = 0; i < q; i++)
				b[i] = patrón(ido + i);
			w = write(fd, b, q);
			if (w > 0)
				ido += w;
			else if (w < 0 && errno != EAGAIN && errno != EINTR) {
				printf("ECO: write: %s\n", strerror(errno));
				return 4;
			}
		}
		if (p.revents & (POLLIN | POLLHUP | POLLERR)) {
			ssize_t r = read(fd, b, sizeof b), i;
			if (r == 0)
				break;
			if (r < 0) {
				if (errno == EAGAIN || errno == EINTR)
					continue;
				printf("ECO: read: %s\n", strerror(errno));
				return 4;
			}
			for (i = 0; i < r; i++)
				if (b[i] != patrón(vuelto + i) && distintos < 0)
					distintos = vuelto + i;
			vuelto += r;
		}
	}
	close(fd);
	{
		double s = (ms_ahora() - t0) / 1000;
		if (vuelto != octetos) {
			printf("ECO: MAL: se mandan %lld octetos y vuelven %lld\n", octetos, vuelto);
			return 5;
		}
		if (distintos >= 0) {
			printf("ECO: MAL: vuelven %lld octetos, pero distintos desde el %lld\n", vuelto, distintos);
			return 5;
		}
		printf("ECO: %lld octetos de ida y vuelta, iguales, en %s s\n", octetos, coma(s, 2));
	}
	return 0;
}

int main(int argc, char **argv)
{
	if (argc >= 3 && !strcmp(argv[1], "servidor"))
		return servidor(atoi(argv[2]), argc >= 4 ? atoi(argv[3]) : 1);
	if (argc >= 5 && !strcmp(argv[1], "cliente"))
		return cliente(argv[2], atoi(argv[3]), atoll(argv[4]));
	printf("uso: ECO servidor PUERTO [VECES]\n     ECO cliente EQUIPO PUERTO OCTETOS\n");
	return 1;
}
