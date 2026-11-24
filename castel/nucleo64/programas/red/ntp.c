/* Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * nucleo64/programas/red/ntp.c - Hispa-64, etapa 9: NTP, poner el reloj en
 * hora con SNTP (RFC 4330) por UDP.
 *   NTP [-n] [-p PUERTO] [SERVIDOR]      (pool.ntp.org y el 123 por omisión)
 * Tres intentos de 2 s cada uno. Dice la hora del servidor en UTC y en la
 * hora local (localtime de musl: la zona, en la variable TZ; en Hispa-64,
 * «CET-1CEST,M3.5.0,M10.5.0/3», Europa/Madrid) y lo que iba mal el reloj
 * del sistema; y lo pone en hora con clock_settime(CLOCK_REALTIME), salvo
 * con -n. Código 0 si bien; 1 mal usado; 2 no se encuentra el servidor;
 * 3 no contesta o contesta mal; 4 no se deja poner la hora. */
#include <unistd.h>
#include <poll.h>
#include <stdint.h>
#include "red.h"

#define DE_1900_A_1970 2208988800ULL

/* segundos y nanosegundos desde 1970 a la marca de NTP (desde 1900, 32.32) */
static uint64_t a_ntp(const struct timespec *t)
{
	return ((uint64_t)(t->tv_sec + DE_1900_A_1970) << 32) | (uint64_t)(((uint64_t)t->tv_nsec << 32) / 1000000000ULL);
}

/* de la marca de NTP a segundos desde 1970 (con decimales); era 0, hasta 2036 */
static double de_ntp(uint64_t m)
{
	return (double)(m >> 32) - (double)DE_1900_A_1970 + (double)(m & 0xffffffffu) / 4294967296.0;
}

static uint64_t lee64(const unsigned char *p)
{
	uint64_t v = 0;
	int i;
	for (i = 0; i < 8; i++)
		v = (v << 8) | p[i];
	return v;
}

static void pon64(unsigned char *p, uint64_t v)
{
	int i;
	for (i = 7; i >= 0; i--, v >>= 8)
		p[i] = v & 0xff;
}

int main(int argc, char **argv)
{
	const char *servidor = "pool.ntp.org";
	int puerto = 123, sin_poner = 0, i, fd, intento;
	struct sockaddr_in sa;
	char ip[INET_ADDRSTRLEN];
	unsigned char pq[48], rp[128];

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-n")) sin_poner = 1;
		else if (!strcmp(argv[i], "-p") && i + 1 < argc) puerto = atoi(argv[++i]);
		else if (argv[i][0] == '-') {
			printf("uso: NTP [-n] [-p PUERTO] [SERVIDOR]\n");
			return 1;
		} else servidor = argv[i];
	}
	if (puerto <= 0 || puerto > 65535) {
		printf("uso: NTP [-n] [-p PUERTO] [SERVIDOR]\n");
		return 1;
	}
	if (busca("NTP", servidor, puerto, SOCK_DGRAM, &sa))
		return 2;
	inet_ntop(AF_INET, &sa.sin_addr, ip, sizeof ip);
	fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
	if (fd < 0 || connect(fd, (struct sockaddr *)&sa, sizeof sa) < 0) {
		printf("NTP: no se abre el socket con %s:%d: %s\n", ip, puerto, strerror(errno));
		return 3;
	}
	for (intento = 1; intento <= 3; intento++) {
		struct timespec t1, t4;
		uint64_t m1;
		double fin;
		memset(pq, 0, sizeof pq);
		pq[0] = (0 << 6) | (4 << 3) | 3;       /* sin aviso de segundo, versión 4, cliente */
		clock_gettime(CLOCK_REALTIME, &t1);
		m1 = a_ntp(&t1);
		pon64(pq + 40, m1);                    /* la marca de salida */
		if (send(fd, pq, sizeof pq, 0) < 0) {
			printf("NTP: intento %d: no sale: %s\n", intento, strerror(errno));
			fflush(stdout);
			usleep(200000);
			continue;
		}
		fin = ms_ahora() + 2000;
		for (;;) {
			struct pollfd p = { fd, POLLIN, 0 };
			double queda = fin - ms_ahora();
			ssize_t n;
			if (queda <= 0) {
				printf("NTP: intento %d: %s:%d no contesta en 2 s\n", intento, ip, puerto);
				fflush(stdout);
				break;
			}
			if (poll(&p, 1, (int)queda + 1) <= 0)
				continue;
			n = recv(fd, rp, sizeof rp, 0);
			clock_gettime(CLOCK_REALTIME, &t4);
			if (n < 0) {
				printf("NTP: intento %d: %s\n", intento,
				       errno == ECONNREFUSED ? "rechazado (no escucha nadie en ese puerto)" : strerror(errno));
				fflush(stdout);
				usleep(200000);
				break;
			}
			if (n < 48 || (rp[0] & 7) != 4 || lee64(rp + 24) != m1)
				continue;      /* no es la respuesta a este pedido */
			if (rp[1] == 0) {
				printf("NTP: %s:%d dice que no («%.4s», estrato 0)\n", ip, puerto, (char *)rp + 12);
				return 3;
			}
			if ((rp[0] >> 6) == 3 || lee64(rp + 40) == 0) {
				printf("NTP: %s:%d no está en hora (sin sincronizar)\n", ip, puerto);
				return 3;
			}
			{
				double T1 = de_ntp(m1), T2 = de_ntp(lee64(rp + 32)), T3 = de_ntp(lee64(rp + 40));
				double T4 = t4.tv_sec + t4.tv_nsec / 1e9;
				double desfase = ((T2 - T1) + (T3 - T4)) / 2, retardo = (T4 - T1) - (T3 - T2);
				double ahora = T4 + desfase;
				time_t s = (time_t)ahora;
				struct tm u, l;
				long ms = (long)(desfase * 1000 + (desfase < 0 ? -0.5 : 0.5));
				char tu[64], tl[64];
				gmtime_r(&s, &u);
				localtime_r(&s, &l);
				strftime(tu, sizeof tu, "%d/%m/%Y %H:%M:%S", &u);
				strftime(tl, sizeof tl, "%H:%M:%S", &l);
				printf("NTP: %s:%d dice %s UTC (%s %s); ", ip, puerto, tu, tl, l.tm_zone ? l.tm_zone : "hora local");
				if (ms > 0)
					printf("el reloj iba %ld ms atrasado", ms);
				else if (ms < 0)
					printf("el reloj iba %ld ms adelantado", -ms);
				else
					printf("el reloj iba en hora");
				printf(" (estrato %d, ida y vuelta %s ms); ", rp[1], coma(retardo * 1000, 1));
				if (sin_poner) {
					printf("no se toca (-n)\n");
					return 0;
				}
				{
					struct timespec ya, nuevo;
					double v;
					clock_gettime(CLOCK_REALTIME, &ya);
					v = ya.tv_sec + ya.tv_nsec / 1e9 + desfase;
					nuevo.tv_sec = (time_t)v;
					nuevo.tv_nsec = (long)((v - nuevo.tv_sec) * 1e9);
					if (nuevo.tv_nsec < 0) nuevo.tv_nsec = 0;
					if (nuevo.tv_nsec > 999999999) nuevo.tv_nsec = 999999999;
					if (clock_settime(CLOCK_REALTIME, &nuevo) < 0) {
						printf("no se deja poner en hora: %s\n", strerror(errno));
						return 4;
					}
				}
				printf("puesto en hora\n");
				return 0;
			}
		}
	}
	printf("NTP: %s:%d no contesta (3 intentos)\n", ip, puerto);
	return 3;
}
