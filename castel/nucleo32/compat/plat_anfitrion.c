/* plat_anfitrion.c - (COMPAT) plat.h en Linux, para las pruebas (y el
 * modelo para Hispa-64). Lo de Hispa-Pentium, de mentira y por el entorno:
 *   COMPAT_SISTEMA  la carpeta que hace de unidad del sistema (con HISPA\COMPAT,
 *                   HISPA\PAQUETES y WIN32 dentro); si no, «./sistema»
 *   COMPAT_USUARIO  la carpeta del usuario (si no, COMPAT_SISTEMA/USUARIOS/PRUEBA)
 *   COMPAT_WIN32    la orden que hace de WIN32.BIN (con «COMPRUEBA ...» detrás)
 *   COMPAT_ADMIN    1: en modo administrador; COMPAT_PIDE_ADMIN=1: se da si se pide
 * La red, con sockets de Linux y sin cifrar (TLS solo en Hispa-Pentium).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include "plat.h"
#include "texto.h"

static void ruta_linux(const char *r, char *d, u32 cabe)
{
	u32 i;

	copia(d, r, cabe);
	for (i = 0; d[i]; i++)
		if (d[i] == '\\')
			d[i] = '/';
}

static const char *sistema(void)
{
	const char *s = getenv("COMPAT_SISTEMA");

	return s && *s ? s : "./sistema";
}

int plat_comprueba(const char *args, const char *salida, int *codigo, void (*espera)(void *), void *u, char *error, u32 cabe)
{
	const char *w = getenv("COMPAT_WIN32");
	char o[4096], s[1024], a[1200];
	u32 i, j = 0;
	int r;

	(void)espera;
	(void)u;
	if (!w || !*w) {
		copia(error, "sin COMPAT_WIN32 (en Linux, la orden que hace de WIN32.BIN)", cabe);
		return -1;
	}
	ruta_linux(salida, s, sizeof s);
	/* (los argumentos, con sus «\» de Hispa como «/», entre comillas simples) */
	for (i = 0; args[i] && j + 4 < sizeof a; i++) {
		if (args[i] == '\'')
			continue;
		a[j++] = args[i] == '"' ? '\'' : args[i] == '\\' ? '/' : args[i];
	}
	a[j] = 0;
	snprintf(o, sizeof o, "%s %s > '%s'", w, a, s);
	r = system(o);
	*codigo = r;
	return 0;
}

int plat_carpeta_usuario(char *d, u32 cabe)
{
	const char *u = getenv("COMPAT_USUARIO");

	if (u && *u)
		copia(d, u, cabe);
	else
		fmt(d, cabe, "%s/USUARIOS/PRUEBA", sistema());
	plat_haz_carpeta(d);
	return 0;
}

int plat_ruta_log(char *d, u32 cabe)
{
	char u[512];

	plat_carpeta_usuario(u, sizeof u);
	fmt(d, cabe, "%s/WIN32.LOG", u);
	return 0;
}

void plat_carpeta_compat(char *d, u32 cabe) { fmt(d, cabe, "%s/HISPA/COMPAT", sistema()); }
void plat_carpeta_win32(char *d, u32 cabe) { fmt(d, cabe, "%s/WIN32", sistema()); }
void plat_carpeta_paquetes(char *d, u32 cabe) { fmt(d, cabe, "%s/HISPA/PAQUETES", sistema()); }

int plat_haz_carpeta(const char *ruta)
{
	char r[1024], *p;

	ruta_linux(ruta, r, sizeof r);
	for (p = r + 1; *p; p++)
		if (*p == '/') {
			*p = 0;
			mkdir(r, 0755);
			*p = '/';
		}
	return mkdir(r, 0755) && so_existe(ruta) != 2 ? -1 : 0;
}

int plat_azar(u8 *d, u32 n)
{
	FILE *f = fopen("/dev/urandom", "rb");
	int r;

	if (!f)
		return -1;
	r = fread(d, 1, n, f) == n ? 0 : -1;
	fclose(f);
	return r;
}

static int admin;
int plat_es_admin(void) { return admin || (getenv("COMPAT_ADMIN") && atoi(getenv("COMPAT_ADMIN"))); }
int plat_pide_admin(void)
{
	admin = getenv("COMPAT_PIDE_ADMIN") && atoi(getenv("COMPAT_PIDE_ADMIN"));
	fprintf(stderr, "compat: se pide el modo administrador: %s\n", admin ? "sí" : "no");
	return admin;
}
void plat_deja_admin(void) { admin = 0; }
void plat_coloca(int v, int x, int y) { fprintf(stderr, "compat: ventana %d a (%d, %d)\n", v, x, y); }
int plat_lanza(const char *programa, const char *args)
{
	fprintf(stderr, "compat: se lanzaría %s %s\n", programa, args);
	return 0;
}
void plat_escribe(const char *t) { fputs(t, stdout); fflush(stdout); }
int plat_hay_escritorio(void) { return getenv("OFI_GUION") != NULL; }

void *plat_abre(const char *ruta, u32 *tam)
{
	char r[1024];
	FILE *f;
	long n;

	ruta_linux(ruta, r, sizeof r);
	f = fopen(r, "rb");
	if (!f)
		return NULL;
	if (fseek(f, 0, SEEK_END) || (n = ftell(f)) < 0 || n > 0x7FFFFFFFL) {
		fclose(f);
		return NULL;
	}
	*tam = (u32)n;
	return f;
}
i32 plat_lee_en(void *f, u32 pos, void *d, u32 n)
{
	if (fseek(f, (long)pos, SEEK_SET))
		return -1;
	return (i32)fread(d, 1, n, f);
}
void plat_cierra(void *f) { fclose(f); }

/* ---- la red ---- */
struct plat_con { int s; };

plat_con *plat_conecta(const char *host, u32 puerto, int tls, char *error, u32 cabe)
{
	struct addrinfo h, *r;
	char p[12];
	plat_con *c;
	int s;
	struct timeval tv = { 30, 0 };

	if (tls) {
		copia(error, "en Linux, el Asistente no cifra (eso es de TLS.BIN, en Hispa-Pentium)", cabe);
		return NULL;
	}
	memset(&h, 0, sizeof h);
	h.ai_socktype = SOCK_STREAM;
	snprintf(p, sizeof p, "%u", puerto);
	if (getaddrinfo(host, p, &h, &r)) {
		copia(error, "no se encuentra el servidor", cabe);
		return NULL;
	}
	s = socket(r->ai_family, SOCK_STREAM, 0);
	if (s < 0 || connect(s, r->ai_addr, r->ai_addrlen)) {
		freeaddrinfo(r);
		if (s >= 0)
			close(s);
		copia(error, "el servidor rechaza la conexión", cabe);
		return NULL;
	}
	freeaddrinfo(r);
	setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
	c = malloc(sizeof *c);
	c->s = s;
	return c;
}
int plat_envia(plat_con *c, const void *d, u32 n)
{
	const u8 *p = d;

	while (n) {
		ssize_t k = send(c->s, p, n, 0);

		if (k <= 0)
			return -1;
		p += k;
		n -= (u32)k;
	}
	return 0;
}
i32 plat_recibe(plat_con *c, void *d, u32 cabe, u32 segundos)
{
	ssize_t k;

	(void)segundos;
	k = recv(c->s, d, cabe, 0);
	return k < 0 ? -1 : (i32)k;
}
void plat_desconecta(plat_con *c)
{
	if (!c)
		return;
	close(c->s);
	free(c);
}
int plat_hay_red(u32 segundos)
{
	(void)segundos;
	return 1;
}
