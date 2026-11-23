/* so_anfitrion.c - Correo en Linux: lo de so.h con POSIX y OpenSSL, para
 * las pruebas (con ASan) y como modelo para Hispa-64. El STARTTLS lo hace
 * aquí lo mismo que TLS.BIN en Hispa-Pentium (lo plano de antes, sin que lo
 * vea el resto del programa). Las autoridades: las del sistema, o la del
 * archivo de CORREO_CA (la raíz de prueba). La carpeta del correo: la de
 * CORREO_DATOS (o ./correo-datos). La ventana (ui.h), nada.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <poll.h>
#include <netdb.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509v3.h>
#include "so.h"
#include "ui.h"

/* ---- la memoria ---- */
static u32 usada;
void *so_pide(size_t n) { return malloc(n ? n : 1); }
void *so_repide(void *p, size_t n) { return realloc(p, n ? n : 1); }
void so_suelta(void *p) { free(p); }
u32 so_memoria_usada(void) { return usada; }

/* ---- los archivos ---- */
char so_barra(void) { return '/'; }

u8 *so_lee_archivo(const char *ruta, u32 *largo, u32 max)
{
	FILE *f = fopen(ruta, "rb");
	long n;
	u8 *d;

	*largo = 0;
	if (!f)
		return NULL;
	fseek(f, 0, SEEK_END);
	n = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (n < 0 || (unsigned long)n > max) {
		fclose(f);
		return NULL;
	}
	d = malloc((size_t)n + 1);
	if (!d) {
		fclose(f);
		return NULL;
	}
	if (fread(d, 1, (size_t)n, f) != (size_t)n) {
		fclose(f);
		free(d);
		return NULL;
	}
	fclose(f);
	d[n] = 0;
	*largo = (u32)n;
	return d;
}

void *so_abre(const char *ruta, int modo)
{
	int fl = modo == SO_CREAR ? O_RDWR | O_CREAT | O_TRUNC : modo == SO_ANADIR ? O_WRONLY | O_CREAT | O_APPEND :
		 modo == SO_CAMBIAR ? O_RDWR | O_CREAT : O_RDONLY;
	int fd = open(ruta, fl, 0600);

	if (fd < 0)
		return NULL;
	return (void *)(intptr_t)(fd + 1);
}
#define FD(f) ((int)(intptr_t)(f) - 1)
int so_lee(void *f, void *d, u32 n) { ssize_t r = read(FD(f), d, n); return r < 0 ? -1 : (int)r; }
int so_escribe(void *f, const void *d, u32 n)
{
	const u8 *p = d;

	while (n) {
		ssize_t r = write(FD(f), p, n);

		if (r <= 0)
			return -1;
		p += r;
		n -= (u32)r;
	}
	return 0;
}
int so_posicion(void *f, u32 pos) { return lseek(FD(f), pos, SEEK_SET) == (off_t)pos ? 0 : -1; }
u32 so_largo(void *f) { struct stat s; return fstat(FD(f), &s) ? 0 : (u32)s.st_size; }
int so_cierra(void *f) { return f ? close(FD(f)) : -1; }
int so_borra(const char *ruta) { return unlink(ruta); }
int so_renombra(const char *de, const char *a) { return rename(de, a); }
int so_haz_carpeta(const char *ruta) { return mkdir(ruta, 0700) == 0 || errno == EEXIST ? 0 : -1; }
int so_existe(const char *ruta, int *es_carpeta)
{
	struct stat s;

	if (stat(ruta, &s))
		return 0;
	if (es_carpeta)
		*es_carpeta = S_ISDIR(s.st_mode);
	return 1;
}
int so_carpeta(const char *ruta, int (*f)(void *u, const char *nombre, u32 tam, int es_carpeta), void *u)
{
	DIR *d = opendir(ruta);
	struct dirent *e;

	if (!d)
		return -1;
	while ((e = readdir(d))) {
		char r[1024];
		struct stat s;

		if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
			continue;
		snprintf(r, sizeof r, "%s/%s", ruta, e->d_name);
		if (stat(r, &s))
			continue;
		if (f(u, e->d_name, (u32)s.st_size, S_ISDIR(s.st_mode)))
			break;
	}
	closedir(d);
	return 0;
}

int so_carpeta_datos(char *d, int cabe)
{
	const char *e = getenv("CORREO_DATOS");

	snprintf(d, (size_t)cabe, "%s", e && e[0] ? e : "correo-datos");
	return so_haz_carpeta(d);
}
int so_carpeta_paso(char *d, int cabe)
{
	char t[200];

	so_carpeta_datos(t, sizeof t);
	snprintf(d, (size_t)cabe, "%s/paso", t);
	return so_haz_carpeta(d);
}
int so_carpeta_documentos(char *d, int cabe)
{
	char t[200];

	so_carpeta_datos(t, sizeof t);
	snprintf(d, (size_t)cabe, "%s/documentos", t);
	return so_haz_carpeta(d);
}

/* ---- el tiempo ---- */
static u64 ms_ahora(void)
{
	struct timespec t;

	clock_gettime(CLOCK_MONOTONIC, &t);
	return (u64)t.tv_sec * 1000 + (u64)t.tv_nsec / 1000000;
}
u32 so_tics(void) { return (u32)(ms_ahora() / 10); }
u32 so_hz(void) { return 100; }
void so_duerme(u32 ms) { usleep(ms * 1000); }
i64 so_ahora(void)
{
	const char *e = getenv("CORREO_AHORA");

	return e ? strtoll(e, NULL, 10) : (i64)time(NULL);
}
int so_zona(i64 cuando)
{
	(void)cuando;
	return 60;
}
void so_azar(void *d, u32 n)
{
	static int ini;
	u8 *p = d;

	if (!ini) {
		srandom((unsigned)ms_ahora() ^ (unsigned)getpid());
		ini = 1;
	}
	while (n--)
		*p++ = (u8)random();
}

void so_apunta(const char *f, ...)
{
	va_list a;

	if (getenv("CORREO_CALLA"))
		return;
	va_start(a, f);
	fputs("correo: ", stderr);
	vfprintf(stderr, f, a);
	fputc('\n', stderr);
	va_end(a);
}
void so_dice(const char *t) { fputs(t, stdout); fflush(stdout); }

/* ---- la red ---- */
#define N_CON 8
static struct { int fd; SSL *ssl; int usada, cerrada; } cons[N_CON];
static SSL_CTX *ctx;

static int nueva_tcp(const char *servidor, u32 puerto, u32 espera_ms, char *error, int cabe)
{
	struct addrinfo h, *r, *p;
	char pu[16];
	int fd = -1, e;

	memset(&h, 0, sizeof h);
	h.ai_family = AF_UNSPEC;
	h.ai_socktype = SOCK_STREAM;
	snprintf(pu, sizeof pu, "%u", (unsigned)puerto);
	e = getaddrinfo(servidor, pu, &h, &r);
	if (e) {
		snprintf(error, (size_t)cabe, "No se encuentra el servidor (%s).", servidor);
		return SO_E_NOMBRE;
	}
	for (p = r; p; p = p->ai_next) {
		struct pollfd q;

		fd = socket(p->ai_family, p->ai_socktype | SOCK_NONBLOCK, p->ai_protocol);
		if (fd < 0)
			continue;
		if (connect(fd, p->ai_addr, p->ai_addrlen) == 0)
			break;
		if (errno == EINPROGRESS) {
			int er = 0;
			socklen_t l = sizeof er;

			q.fd = fd;
			q.events = POLLOUT;
			if (poll(&q, 1, (int)espera_ms) == 1 && !getsockopt(fd, SOL_SOCKET, SO_ERROR, &er, &l) && !er)
				break;
		}
		close(fd);
		fd = -1;
	}
	freeaddrinfo(r);
	if (fd < 0) {
		snprintf(error, (size_t)cabe, "El servidor no contesta.");
		return SO_E_CONEXION;
	}
	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) & ~O_NONBLOCK);
	return fd;
}

/* un renglón de lo plano (para STARTTLS), sin leer nada de más */
static int renglon_plano(int fd, char *b, int cabe)
{
	int k = 0;

	while (k + 1 < cabe) {
		struct pollfd q = { fd, POLLIN, 0 };
		char c;

		if (poll(&q, 1, 15000) != 1 || read(fd, &c, 1) != 1)
			return -1;
		b[k++] = c;
		if (c == '\n')
			break;
	}
	b[k] = 0;
	return k;
}

static int hay_mas(int fd)
{
	struct pollfd q = { fd, POLLIN, 0 };

	return poll(&q, 1, 50) == 1;
}

static int starttls(int fd, int modo, char *error, int cabe)
{
	char b[1024];
	int hay = 0;

	for (;;) {
		if (renglon_plano(fd, b, sizeof b) < 0)
			goto mudo;
		if (modo == SO_STARTTLS_IMAP ? strncmp(b, "* OK", 4) : modo == SO_STARTTLS_POP3 ? strncmp(b, "+OK", 3) : strncmp(b, "220", 3)) {
			snprintf(error, (size_t)cabe, "STARTTLS: el servidor de correo no saluda bien");
			return SO_E_TLS;
		}
		if (!(modo == SO_STARTTLS_SMTP && b[3] == '-'))
			break;
	}
	if (modo == SO_STARTTLS_SMTP) {
		if (write(fd, "EHLO hispa-pentium\r\n", 20) != 20)
			goto mudo;
		for (;;) {
			if (renglon_plano(fd, b, sizeof b) < 0)
				goto mudo;
			if (strncmp(b, "250", 3)) {
				snprintf(error, (size_t)cabe, "STARTTLS: el servidor no acepta EHLO");
				return SO_E_TLS;
			}
			if (!strncmp(b + 4, "STARTTLS", 8))
				hay = 1;
			if (b[3] != '-')
				break;
		}
		if (!hay)
			goto sin;
	}
	{
		const char *o = modo == SO_STARTTLS_IMAP ? "h1 STARTTLS\r\n" : modo == SO_STARTTLS_POP3 ? "STLS\r\n" : "STARTTLS\r\n";

		if (write(fd, o, strlen(o)) != (ssize_t)strlen(o))
			goto mudo;
	}
	for (;;) {
		if (renglon_plano(fd, b, sizeof b) < 0)
			goto mudo;
		if (modo == SO_STARTTLS_IMAP && b[0] == '*')
			continue;
		break;
	}
	if (modo == SO_STARTTLS_IMAP ? strncmp(b, "h1 OK", 5) : modo == SO_STARTTLS_POP3 ? strncmp(b, "+OK", 3) : strncmp(b, "220", 3))
		goto sin;
	if (hay_mas(fd)) {
		snprintf(error, (size_t)cabe, "STARTTLS: el servidor manda de m\xc3\xa1s detr\xc3\xa1s del s\xc3\xad: no se sigue");
		return SO_E_TLS;
	}
	return 0;
sin:
	snprintf(error, (size_t)cabe, "el servidor de correo no ofrece STARTTLS: la conexi\xc3\xb3n ir\xc3\xad" "a sin cifrar");
	return SO_E_STARTTLS;
mudo:
	snprintf(error, (size_t)cabe, "STARTTLS: el servidor de correo no contesta");
	return SO_E_CONEXION;
}

int so_conecta(const char *servidor, u32 puerto, int modo, u32 espera_ms, char *error, int cabe)
{
	int fd, k, e;
	SSL *s = NULL;
	char tmp[256];

	if (!error) {
		error = tmp;
		cabe = sizeof tmp;
	}
	error[0] = 0;
	for (k = 0; k < N_CON && cons[k].usada; k++)
		;
	if (k == N_CON)
		return SO_E_OTRO;
	fd = nueva_tcp(servidor, puerto, espera_ms ? espera_ms : 20000, error, cabe);
	if (fd < 0)
		return fd;
	if (modo >= SO_STARTTLS_IMAP && modo <= SO_STARTTLS_POP3) {
		e = starttls(fd, modo, error, cabe);
		if (e) {
			close(fd);
			return e;
		}
	}
	if (modo != SO_PLANO) {
		X509_VERIFY_PARAM *vp;
		long v;

		if (!ctx) {
			const char *ca = getenv("CORREO_CA");

			ctx = SSL_CTX_new(TLS_client_method());
			SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
			if (getenv("CORREO_SOLO_TLS12"))
				SSL_CTX_set_max_proto_version(ctx, TLS1_2_VERSION);
			if (ca)
				SSL_CTX_load_verify_locations(ctx, ca, NULL);
			else
				SSL_CTX_set_default_verify_paths(ctx);
			SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);
		}
		s = SSL_new(ctx);
		SSL_set_fd(s, fd);
		SSL_set_tlsext_host_name(s, servidor);
		vp = SSL_get0_param(s);
		if (inet_addr(servidor) != INADDR_NONE)
			X509_VERIFY_PARAM_set1_ip_asc(vp, servidor);
		else
			X509_VERIFY_PARAM_set1_host(vp, servidor, 0);
		if (SSL_connect(s) != 1) {
			snprintf(error, (size_t)cabe, "La conexi\xc3\xb3n segura con %s no se ha podido hacer.", servidor);
			SSL_free(s);
			close(fd);
			return SO_E_TLS;
		}
		v = SSL_get_verify_result(s);
		if (v != X509_V_OK) {
			snprintf(error, (size_t)cabe, "%s",
				 v == X509_V_ERR_HOSTNAME_MISMATCH || v == X509_V_ERR_IP_ADDRESS_MISMATCH ?
				 "el certificado es de otro sitio" : "el certificado no lo firma ninguna autoridad conocida");
			snprintf(error + strlen(error), (size_t)cabe - strlen(error), " (%s)", X509_verify_cert_error_string(v));
			SSL_free(s);
			close(fd);
			return SO_E_CERTIFICADO;
		}
	}
	cons[k].fd = fd;
	cons[k].ssl = s;
	cons[k].usada = 1;
	cons[k].cerrada = 0;
	return k + 1;
}

int so_envia(int c, const void *d, u32 n)
{
	const u8 *p = d;

	if (c < 1 || c > N_CON || !cons[c - 1].usada)
		return SO_E_OTRO;
	while (n) {
		int r = cons[c - 1].ssl ? SSL_write(cons[c - 1].ssl, p, (int)n) : (int)write(cons[c - 1].fd, p, n);

		if (r <= 0)
			return SO_E_CORTADA;
		p += r;
		n -= (u32)r;
	}
	return 0;
}

int so_recibe(int c, void *d, u32 cabe, u32 espera_ms)
{
	int r;

	if (c < 1 || c > N_CON || !cons[c - 1].usada)
		return SO_E_OTRO;
	if (cons[c - 1].cerrada)
		return 0;
	if (!cons[c - 1].ssl || !SSL_pending(cons[c - 1].ssl)) {
		struct pollfd q = { cons[c - 1].fd, POLLIN, 0 };

		if (poll(&q, 1, (int)espera_ms) != 1)
			return SO_E_ESPERA;
	}
	if (cons[c - 1].ssl) {
		r = SSL_read(cons[c - 1].ssl, d, (int)cabe);
		if (r <= 0) {
			int e = SSL_get_error(cons[c - 1].ssl, r);

			if (e == SSL_ERROR_WANT_READ)
				return SO_E_ESPERA;
			if (e == SSL_ERROR_ZERO_RETURN || e == SSL_ERROR_SYSCALL) {
				cons[c - 1].cerrada = 1;
				return 0;
			}
			return SO_E_CORTADA;
		}
	} else {
		r = (int)read(cons[c - 1].fd, d, cabe);
		if (r < 0)
			return SO_E_CORTADA;
		if (r == 0)
			cons[c - 1].cerrada = 1;
	}
	return r;
}

void so_cierra_con(int c)
{
	if (c < 1 || c > N_CON || !cons[c - 1].usada)
		return;
	if (cons[c - 1].ssl) {
		SSL_shutdown(cons[c - 1].ssl);
		SSL_free(cons[c - 1].ssl);
	}
	close(cons[c - 1].fd);
	cons[c - 1].usada = 0;
}

int so_cifrado(int c, char *d, int cabe)
{
	if (c < 1 || c > N_CON || !cons[c - 1].usada || !cons[c - 1].ssl)
		return 0;
	snprintf(d, (size_t)cabe, "%s, %s", SSL_get_version(cons[c - 1].ssl), SSL_get_cipher(cons[c - 1].ssl));
	return 1;
}

int so_dns(const u8 *pregunta, u32 n, u8 *respuesta, u32 cabe, u32 espera_ms)
{
	(void)pregunta, (void)n, (void)respuesta, (void)cabe, (void)espera_ms;
	return -1;
}

int so_hay_red(void) { return 1; }

/* ---- los otros programas ---- */
int so_lanza_espera(const char *programa, const char *argumentos)
{
	char t[1200];
	int r;

	snprintf(t, sizeof t, "%s %s", programa, argumentos ? argumentos : "");
	r = system(t);
	return r < 0 ? -1 : WEXITSTATUS(r);
}
int so_lanza(const char *programa, const char *argumentos)
{
	(void)programa;
	(void)argumentos;
	return -1;
}
int so_programa(const char *nombre, char *ruta, int cabe)
{
	const char *e = getenv("CORREO_PROGRAMAS");

	if (!e)
		return 0;
	snprintf(ruta, (size_t)cabe, "%s/%s", e, nombre);
	return access(ruta, X_OK) == 0;
}
static int argc_;
static char **argv_;
int so_argumentos(char *d, int cabe)
{
	int i, k = 0;

	d[0] = 0;
	for (i = 1; i < argc_ && k < cabe; i++)
		k += snprintf(d + k, (size_t)(cabe - k), strchr(argv_[i], ' ') ? "%s\"%s\"" : "%s%s", i > 1 ? " " : "", argv_[i]);
	return k;
}
int so_al_portapapeles(const char *t, u32 n) { (void)t; (void)n; return 0; }
void so_puntero_zona(int x, int y, int an, int al, int forma) { (void)x; (void)y; (void)an; (void)al; (void)forma; }     /* (PUNTEROS) */
char *so_del_portapapeles(u32 *n) { *n = 0; return NULL; }

/* ---- la ventana: nada ---- */
int ui_abre(const char *t, int an, int al) { (void)t, (void)an, (void)al; return -1; }
int ui_abre_aviso(int an, int al) { (void)an, (void)al; return -1; }
void ui_cierra(void) {}
void ui_titulo(const char *t) { (void)t; }
void ui_pantalla(int *an, int *al) { *an = 1024; *al = 570; }
void ui_maximiza(void) {}
int ui_soltado(char *ruta, int cabe) { (void)cabe; ruta[0] = 0; return 0; }
int ui_suceso(Suceso *s, u32 espera_ms) { (void)s; if (espera_ms) usleep(espera_ms * 1000); return 0; }
int ui_espera(Suceso *s, u32 espera_ms) { return ui_suceso(s, espera_ms ? espera_ms : 1000); }
void ui_vuelca(void) {}
void ui_callado(int si) { (void)si; }
void ui_ensucia(int y, int al) { (void)y, (void)al; }
void ui_cuenta_raton(u32 *m, u32 *r, u32 *v) { *m = *r = *v = 0; }
void ui_rect(int x, int y, int an, int al, u32 c) { (void)x, (void)y, (void)an, (void)al, (void)c; }
void ui_degradado(int x, int y, int an, int al, u32 a, u32 b) { (void)x, (void)y, (void)an, (void)al, (void)a, (void)b; }
void ui_redondo(int x, int y, int an, int al, int r, u32 a, u32 b) { (void)x, (void)y, (void)an, (void)al, (void)r, (void)a, (void)b; }
void ui_marco(int x, int y, int an, int al, u32 c) { (void)x, (void)y, (void)an, (void)al, (void)c; }
void ui_linea(int x1, int y1, int x2, int y2, u32 c) { (void)x1, (void)y1, (void)x2, (void)y2, (void)c; }
void ui_mapa(int x, int y, int an, int al, const u32 *p) { (void)x, (void)y, (void)an, (void)al, (void)p; }
void ui_texto(int x, int y, const char *t, u32 n, int max, u32 c, int negrita) { (void)x, (void)y, (void)t, (void)n, (void)max, (void)c, (void)negrita; }
void ui_menu(int x, int y, const char *o, u32 g, u32 e) { (void)x, (void)y, (void)o, (void)g, (void)e; }

int correo_principal(void);

int main(int argc, char **argv)
{
	argc_ = argc;
	argv_ = argv;
	SSL_library_init();
	return correo_principal();
}
