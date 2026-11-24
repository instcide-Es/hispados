/* Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * nucleo64/programas/red/red.h - Hispa-64, etapa 9: lo común de los
 * programas de red del anillo 3 (PING, IFCONFIG, DESCARGA, NTP, ECO, MRED):
 * el reloj en milisegundos, los números con coma decimal y buscar un
 * nombre (getaddrinfo de musl, solo IPv4). */
#ifndef RED_H
#define RED_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

/* el reloj que no salta (CLOCK_MONOTONIC), en milisegundos */
static double ms_ahora(void)
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}

/* un número con DEC decimales y coma (a la española): en un búfer que rota */
static const char *coma(double v, int dec)
{
	static char b[8][32];
	static int k;
	char *p = b[k++ & 7], *q;
	snprintf(p, 32, "%.*f", dec, v);
	if ((q = strchr(p, '.')))
		*q = ',';
	return p;
}

/* el texto de getaddrinfo, en español */
static const char *error_gai(int e)
{
	switch (e) {
	case EAI_NONAME: return "no existe ese nombre";
	case EAI_AGAIN: return "el servidor de nombres no contesta (¿hay red? ¿hay DNS en /etc/resolv.conf?)";
	case EAI_FAIL: return "el servidor de nombres ha fallado";
	case EAI_MEMORY: return "sin memoria";
	case EAI_SYSTEM: return strerror(errno);
	default: return gai_strerror(e);
	}
}

/* NOMBRE (o IP) a dirección IPv4; 0 si bien; si no, dice por qué */
static int busca(const char *quién, const char *nombre, int puerto, int tipo, struct sockaddr_in *sa)
{
	struct addrinfo h, *r;
	int e;
	memset(&h, 0, sizeof h);
	h.ai_family = AF_INET;
	h.ai_socktype = tipo;
	e = getaddrinfo(nombre, 0, &h, &r);
	if (e) {
		printf("%s: no se encuentra «%s»: %s\n", quién, nombre, error_gai(e));
		return -1;
	}
	memcpy(sa, r->ai_addr, sizeof *sa);
	sa->sin_port = htons(puerto);
	freeaddrinfo(r);
	return 0;
}
#endif
