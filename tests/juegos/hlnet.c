/* HLNET.EXE - (HL) prueba de WININET.DLL: HTTP de verdad sobre el servicio
 * de red (InternetOpenUrl de un archivo grande con Content-Length, en
 * trozos, una redirección; InternetConnect, HttpOpenRequest, HttpSendRequest
 * y HttpQueryInfo; InternetCrackUrl), y lo que no hay dicho limpio (HTTPS,
 * FTP). «HLNET servidor puerto» contra el servidor de pruebas de aquí
 * (nucleo32/servidor_prueba.py); «HLNET /SINRED» sin red: «sin conexión».
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "hl.h"

static u8 buf[8192];
/* baja la URL entera: el largo y la suma de sus octetos */
static int baja(HANDLE s, const char *url, u32 *suma, u32 *estado)
{
	HANDLE u = InternetOpenUrlA(s, url, 0, 0, 0, 0);
	u32 n, total = 0, k, t = sizeof *estado;
	*suma = 0;
	*estado = 0;
	if (!u)
		return -(int)GetLastError();
	HttpQueryInfoA(u, 19 | 0x20000000, estado, &t, 0);    /* STATUS_CODE | FLAG_NUMBER */
	while (InternetReadFile(u, buf, sizeof buf, &n) && n) {
		for (k = 0; k < n; k++)
			*suma += buf[k];
		total += n;
	}
	InternetCloseHandle(u);
	return (int)total;
}

int main(int argc, char **argv)
{
	HANDLE s;
	u32 fl = 0, suma, estado;
	int n, si;
	char url[200], b[256];
	si = InternetGetConnectedState(&fl, 0);
	printf("conectado: %d %x\n", si, fl);
	s = InternetOpenA("HLNET", 0, 0, 0, 0);
	printf("internetopen: %s\n", s ? "si" : "no");
	{
		URL_COMPONENTSA c;
		char host[64], cam[64];
		memset(&c, 0, sizeof c);
		c.tam = sizeof c;
		c.host = host;
		c.nhost = sizeof host;
		c.camino = cam;
		c.ncamino = sizeof cam;
		n = InternetCrackUrlA("http://servidores.ejemplo.es:27950/lista.txt?juego=hl", 0, 0, &c);
		printf("crackurl: %d %d %s %u %s\n", n, c.tipo, host, c.puerto, cam);
	}
	if (argc > 1 && !strcmp(argv[1], "/SINRED")) {
		n = baja(s, "http://10.0.2.2/nada.txt", &suma, &estado);
		printf("sin_red: %d\n", n);
		InternetCloseHandle(s);
		printf("FIN HLNET\n");
		return 0;
	}
	if (argc < 3)
		return 2;
	sprintf(url, "http://%s:%s/grande.bin", argv[1], argv[2]);
	n = baja(s, url, &suma, &estado);
	printf("grande: %d %u %u\n", n, suma, estado);
	sprintf(url, "http://%s:%s/trozos.bin", argv[1], argv[2]);
	n = baja(s, url, &suma, &estado);
	printf("trozos: %d %u %u\n", n, suma, estado);
	sprintf(url, "http://%s:%s/redirige", argv[1], argv[2]);
	n = baja(s, url, &suma, &estado);
	printf("redirige: %d %u\n", n, estado);
	sprintf(url, "http://%s:%s/no-esta", argv[1], argv[2]);
	n = baja(s, url, &suma, &estado);
	printf("no_esta: %u\n", estado);
	{
		HANDLE c = InternetConnectA(s, argv[1], (u16)atoi(argv[2]), 0, 0, 3, 0, 0);
		HANDLE r = c ? HttpOpenRequestA(c, "GET", "/ultima.txt", 0, 0, 0, 0, 0) : 0;
		u32 t = sizeof b, l = 0;
		int ok = r && HttpSendRequestA(r, 0, 0, 0, 0);
		memset(b, 0, sizeof b);
		HttpQueryInfoA(r, 20, b, &t, 0);                  /* STATUS_TEXT */
		printf("httpsendrequest: %d [%s]", ok, b);
		memset(b, 0, sizeof b);
		InternetReadFile(r, b, sizeof b - 1, &l);
		printf(" %u %s\n", l, strstr(b, "version=0.8") ? "con su texto" : "MAL");
		t = sizeof b;
		strcpy(b, "Content-Length");
		ok = HttpQueryInfoA(r, 65535, b, &t, 0);
		printf("httpqueryinfo_custom: %d %s\n", ok, b);
		InternetCloseHandle(r);
		InternetCloseHandle(c);
	}
	n = baja(s, "https://10.0.2.2/seguro.txt", &suma, &estado);
	printf("https: %d\n", n);
	n = baja(s, "ftp://10.0.2.2/archivo.zip", &suma, &estado);
	printf("ftp: %d\n", n);
	InternetCloseHandle(s);
	printf("FIN HLNET\n");
	return 0;
}
