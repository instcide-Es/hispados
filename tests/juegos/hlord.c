/* HLORD.EXE - (HL) prueba de Winsock por número: los juegos de 1998-2000
 * (Half-Life y los de su motor) importan WSOCK32.DLL por número, no por
 * nombre. Este programa también (lo hace así juegos.sh: «-i
 * WSOCK32.dll:htons@9,...»), y WS2_32.DLL, que tiene otros números del 10
 * al 12; y GetProcAddress con un número. Una línea por cosa.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "hl.h"
int WINAPI WSASetLastError(int);
int WINAPI __WSAFDIsSet(u32, FD_SET_J *);
void *WINAPI getprotobynumber(int);
/* las de WS2_32, con otro nombre aquí (lo que cuenta es su número) */
u32 WINAPI ws2_inet_addr(const char *);
char *WINAPI ws2_inet_ntoa(u32);
int WINAPI ws2_ioctlsocket(u32, i32, u32 *);
u32 WINAPI ws2_ntohl(u32);

int main(void)
{
	WSADATA w;
	u32 s, v = 1;
	int r;
	FD_SET_J f;
	TIMEVAL t = { 0, 0 };
	char nom[64];
	r = WSAStartup(0x101, &w);
	printf("wsastartup: %d %x\n", r, w.version);
	printf("htons: %04x %04x\n", htons(0x1234), ntohs(0x3412));
	printf("htonl: %08x\n", ntohl(htonl(0x01020304)) == 0x01020304 ? htonl(0x01020304) : 0);
	printf("inet_addr: %08x %s\n", inet_addr("10.0.2.15"), inet_ntoa(inet_addr("192.168.1.20")));
	WSASetLastError(10035);
	printf("wsalasterror: %d\n", WSAGetLastError());
	r = gethostname(nom, sizeof nom);
	printf("gethostname: %d %s\n", r, nom[0] ? "con nombre" : "MAL");
	printf("getprotobynumber: %s\n", getprotobynumber(17) ? "algo" : "nulo");
	s = socket(2, 2, 17);
	printf("socket_udp: %s\n", s != 0xFFFFFFFF ? "si" : "no");
	if (s != 0xFFFFFFFF) {
		r = ioctlsocket(s, FIONBIO, &v);
		printf("ioctlsocket: %d\n", r);
		f.n = 1;
		f.s[0] = s;
		printf("wsafdisset: %d %d\n", __WSAFDIsSet(s, &f) != 0, __WSAFDIsSet(s + 1, &f) != 0);
		closesocket(s);
	}
	/* WS2_32: 10 es ioctlsocket, 11 inet_addr, 12 inet_ntoa */
	printf("ws2_32: %08x %s %08x\n", ws2_inet_addr("10.0.2.2"), ws2_inet_ntoa(0x0F02000A), ws2_ntohl(0x04030201));
	{
		HMODULE m = LoadLibraryA("WSOCK32.DLL");
		void *p10 = GetProcAddress(m, (const char *)10), *p52 = GetProcAddress(m, (const char *)52), *p999 = GetProcAddress(m, (const char *)999);
		u32 (WINAPI *ia)(const char *) = p10;
		printf("getprocaddress_numero: %s %s %s\n", ia && ia("1.2.3.4") == 0x04030201 ? "inet_addr" : "MAL", p52 ? "gethostbyname" : "MAL", p999 ? "MAL" : "nulo");
		HMODULE m2 = LoadLibraryA("WS2_32.DLL");
		void *q10 = GetProcAddress(m2, (const char *)10), *q11 = GetProcAddress(m2, (const char *)11);
		printf("getprocaddress_ws2: %s %s\n", q10 != p10 ? "otra" : "MAL", q11 == p10 ? "inet_addr" : "MAL");
	}
	printf("wsacleanup: %d\n", WSACleanup());
	printf("FIN HLORD\n");
	return 0;
}
