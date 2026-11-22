/* MOD\DLLS\SERVIDOR.DLL del «juego de mentira» JUEGO99 (HL): la parte del
 * servidor de un mod, en su subcarpeta: Winsock por número (UDP), las de
 * archivos de 16 bits y la seguridad de ADVAPI32.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "../hl.h"
int WINAPI __WSAFDIsSet(u32, FD_SET_J *);
void *volatile importa_sv[] = { (void *)WSAGetLastError, (void *)select, (void *)recvfrom, (void *)sendto, (void *)bind, (void *)GetKernelObjectSecurity };
int WINAPI ServidorIniciar(void)
{
	u32 s = socket(2, 2, 17), v = 1;
	u8 sd[20];
	FD_SET_J f;
	char n[64];
	int r = 0;
	if (s != 0xFFFFFFFF) {
		ioctlsocket(s, FIONBIO, &v);
		f.n = 1;
		f.s[0] = s;
		r = __WSAFDIsSet(s, &f) ? 1 : 0;
		closesocket(s);
	}
	gethostname(n, sizeof n);
	r += InitializeSecurityDescriptor(sd, 1) ? 10 : 0;
	r += _lopen("NOHAY.CFG", 0) == (HANDLE)-1 ? 100 : 0;
	r += ntohl(htonl(5)) == 5 ? 1000 : 0;
	return r;
}
int WINAPI DllMain(void *h, unsigned r, void *x) { (void)h; (void)r; (void)x; return 1; }
