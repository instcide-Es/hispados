/* REDUDP.EXE - prueba de UDP de WSOCK32 (JUEGOS): lo que hace un juego en
 * red local (como cliente y como servidor): gethostname y gethostbyname del
 * propio equipo, sendto y recvfrom con y sin bloqueo (FIONBIO, el «no hay»
 * al momento), select, FIONREAD, bind a un puerto (27015, el de los juegos
 * de entonces) y contestar al que llega sin haberlo pedido, getsockname y
 * la difusión (SO_BROADCAST). Con un eco de UDP en ECO (10.0.2.2) y el
 * puerto 27015 llevado desde fuera (QEMU: hostfwd). «REDUDP puerto_eco».
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "j.h"
static void dir_de(SOCKADDR_IN *a, u32 ip, u16 p) { memset(a, 0, sizeof *a); a->familia = 2; a->ip = ip; a->puerto = htons(p); }
int main(int argc, char **argv)
{
	WSADATA w;
	char b[600], yo[64];
	HOSTENT *h;
	u32 s, sv, uno = 1, k, t0;
	SOCKADDR_IN a, de;
	int n, l;
	u16 eco = argc > 1 ? (u16)atoi(argv[1]) : 7777;
	printf("wsastartup: %d\n", WSAStartup(0x0101, &w));
	/* cliente: al eco y vuelta */
	s = socket(2, 2, 17);
	dir_de(&a, inet_addr("10.0.2.2"), eco);
	n = sendto(s, "hola eco", 8, 0, &a, 16);
	l = 16;
	n = recvfrom(s, b, sizeof b - 1, 0, &de, &l);
	b[n > 0 ? n : 0] = 0;
	printf("eco: %d [%s] de %s:%u\n", n, b, inet_ntoa(de.ip), ntohs(de.puerto));
	gethostname(yo, sizeof yo);
	h = gethostbyname(yo);                  /* (el propio equipo: su IP, sin DNS) */
	printf("mi_ip: %s\n", h ? inet_ntoa(*(u32 *)h->dirs[0]) : "no");
	l = 16;
	getsockname(s, &de, &l);
	printf("getsockname: %s\n", ntohs(de.puerto) >= 1024 ? "puerto propio" : "MAL");
	/* sin bloqueo: «no hay» al momento */
	ioctlsocket(s, FIONBIO, &uno);
	t0 = GetTickCount();
	for (k = 0; k < 100; k++)
		n = recvfrom(s, b, sizeof b, 0, &de, &l);
	printf("sin_bloqueo: %d %d %s\n", n, WSAGetLastError(), GetTickCount() - t0 < 1000 ? "deprisa" : "despacio");
	/* select: hasta que llegue el eco */
	sendto(s, "otra vez", 8, 0, &a, 16);
	{
		FD_SET_J f;
		TIMEVAL tv = { 3, 0 };
		u32 cuanto = 0;
		f.n = 1;
		f.s[0] = s;
		n = select(0, &f, 0, 0, &tv);
		ioctlsocket(s, 0x4004667F, &cuanto);
		printf("select: %d %u\n", n, cuanto);
		n = recvfrom(s, b, sizeof b - 1, 0, &de, &l);
		b[n > 0 ? n : 0] = 0;
		printf("tras_select: %d [%s]\n", n, b);
	}
	/* difusión */
	setsockopt(s, 0xFFFF, 0x20, (const char *)&uno, 4);
	dir_de(&a, 0xFFFFFFFF, 27016);
	printf("difusion: %d\n", sendto(s, "quien hay", 9, 0, &a, 16));
	closesocket(s);
	/* servidor: en el 27015, al que llega */
	sv = socket(2, 2, 17);
	dir_de(&a, 0, 27015);
	printf("bind: %d\n", bind(sv, &a, 16));
	{
		u32 s2 = socket(2, 2, 17);
		int r2 = bind(s2, &a, 16);
		printf("bind_ocupado: %d %d\n", r2, WSAGetLastError());
		closesocket(s2);
	}
	printf("escuchando 27015\n");
	fflush(stdout);
	{
		FD_SET_J f;
		TIMEVAL tv = { 30, 0 };
		f.n = 1;
		f.s[0] = sv;
		n = select(0, &f, 0, 0, &tv);
		l = 16;
		n = n > 0 ? recvfrom(sv, b, sizeof b - 1, 0, &de, &l) : -1;
		b[n > 0 ? n : 0] = 0;
		printf("servidor_recibe: %d [%s]\n", n, b);
		if (n > 0) {
			n = sendto(sv, "respuesta del servidor", 22, 0, &de, 16);
			printf("servidor_contesta: %d\n", n);
		}
	}
	closesocket(sv);
	WSACleanup();
	printf("FIN REDUDP\n");
	return 0;
}
