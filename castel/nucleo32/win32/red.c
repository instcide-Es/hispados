/* red.c - WIN32.BIN: WSOCK32 y WS2_32 (Winsock). De momento, lo de arrancar
 * y la red «caída»: los programas que la miran siguen sin colgarse.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "w32.h"
#define API(t) t WINAPI
static i32 ultimo;
API(i32) WSAStartup(u16 v, u8 *d)
{
	memset(d, 0, 400);
	*(u16 *)d = v;
	*(u16 *)(d + 2) = 0x0202;
	strcpy((char *)d + 4, "Hispa-Pentium Winsock");
	strcpy((char *)d + 261, "Sin red");
	*(u16 *)(d + 390) = 32;
	return 0;
}
API(i32) WSACleanup(void) { return 0; }
API(i32) WSAGetLastError(void) { return ultimo; }
API(void) WSASetLastError(i32 e) { ultimo = e; }
API(u32) socket(i32 a, i32 t, i32 p) { (void)a; (void)t; (void)p; ultimo = 10050; return 0xFFFFFFFF; }
API(i32) closesocket(u32 s) { (void)s; return 0; }
API(void *) gethostbyname(const char *n) { (void)n; ultimo = 11001; return 0; }
API(i32) gethostname(char *d, i32 n) { copia_n(d, "HISPA-PENTIUM", n); return 0; }
API(u16) htons(u16 v) { return (v >> 8) | (v << 8); }
API(u16) ntohs(u16 v) { return (v >> 8) | (v << 8); }
API(u32) htonl(u32 v) { return (v >> 24) | ((v >> 8) & 0xFF00) | ((v << 8) & 0xFF0000) | (v << 24); }
API(u32) ntohl(u32 v) { return htonl(v); }
API(u32) inet_addr(const char *s)
{
	u32 r = 0, k, v;
	for (k = 0; k < 4; k++) {
		v = 0;
		if (*s < '0' || *s > '9')
			return 0xFFFFFFFF;
		while (*s >= '0' && *s <= '9')
			v = v * 10 + *s++ - '0';
		r |= (v & 255) << (k * 8);
		if (k < 3 && *s++ != '.')
			return 0xFFFFFFFF;
	}
	return r;
}
API(char *) inet_ntoa(u32 a)
{
	static char b[20];
	fmt(b, sizeof b, "%u.%u.%u.%u", a & 255, (a >> 8) & 255, (a >> 16) & 255, a >> 24);
	return b;
}
