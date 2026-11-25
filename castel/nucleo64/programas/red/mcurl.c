/* Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * nucleo64/programas/red/mcurl.c - Hispa-64, etapa 10 b (docs/64/HISPA-64.md,
 * sección 19): la prueba de libcurl 8.14.1 (con BearSSL), lo que pedirá el
 * navegador para bajar las páginas.
 *
 *   MCURL               las pruebas por 127.0.0.1, con un servidor de HTTP/1.1
 *                       dentro (un hilo por conexión, con keep-alive): GET,
 *                       conexiones reutilizadas, a trozos (chunked), las
 *                       redirecciones, 404, POST, «Connection: close», el
 *                       plazo, conexión rechazada, «localhost» (con IPv6
 *                       pedido: sin él, por IPv4), 16 a la vez con la interfaz
 *                       «multi», curl_url y escape; dice «MCURL: todo bien (N
 *                       pruebas)»
 *   MCURL [-c CA.PEM] [-r NOMBRE:PUERTO:IP] DIRECCIÓN...
 *                       las baja a la vez (la interfaz «multi»), cada una con
 *                       su código, sus octetos y su sha256 (BearSSL); -c, la
 *                       autoridad para HTTPS; -r, un nombre sin DNS (como
 *                       --resolve de curl). Acaba con 0 si todas dan 200.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <curl/curl.h>
#include "bearssl.h"
#include "red.h"
#include "../musl/comun.h"

/* ---- el contenido de prueba: el octeto i de /n/... ---- */
static unsigned char octeto(size_t i) { return (unsigned char)(i * 7 + 3 + (i >> 9)); }

/* ---- el servidor de HTTP/1.1 de prueba (127.0.0.1) ---- */
static int srv_fd, srv_puerto;
static volatile int conexiones;                  /* las que ha aceptado */

static int manda_todo(int s, const void *d, size_t n)
{
	const char *p = d;
	while (n) {
		ssize_t k = send(s, p, n, MSG_NOSIGNAL);
		if (k <= 0) { if (k < 0 && errno == EINTR) continue; return -1; }
		p += k; n -= (size_t)k;
	}
	return 0;
}

static void manda_cuerpo(int s, size_t n)
{
	unsigned char b[8192];
	size_t i = 0;
	while (i < n) {
		size_t k = n - i < sizeof b ? n - i : sizeof b;
		for (size_t j = 0; j < k; j++) b[j] = octeto(i + j);
		if (manda_todo(s, b, k)) return;
		i += k;
	}
}

static void *atiende(void *x)
{
	int s = (int)(long)x;
	char cab[4096];
	size_t n = 0;
	for (;;) {
		/* la cabecera, hasta la línea en blanco */
		char *fin;
		while (!(fin = memmem(cab, n, "\r\n\r\n", 4))) {
			ssize_t k = n < sizeof cab ? recv(s, cab + n, sizeof cab - n, 0) : -1;
			if (k <= 0) goto cierra;
			n += (size_t)k;
		}
		size_t lc = (size_t)(fin - cab) + 4;
		char met[8] = "", ruta[256] = "";
		sscanf(cab, "%7s %255s", met, ruta);
		int cerrar = memmem(cab, lc, "Connection: close", 17) != NULL;
		size_t largo = 0;
		char *cl = memmem(cab, lc, "Content-Length: ", 16);
		if (cl) largo = strtoul(cl + 16, NULL, 10);
		/* el cuerpo del pedido (POST): lo que ya ha llegado y lo que falte */
		static __thread unsigned char cuerpo[65536];
		size_t ya = n - lc < largo ? n - lc : largo;
		if (largo > sizeof cuerpo) goto cierra;
		memcpy(cuerpo, cab + lc, ya);
		memmove(cab, cab + lc + ya, n - lc - ya);   /* lo que queda: el pedido siguiente */
		n -= lc + ya;
		while (ya < largo) {
			ssize_t k = recv(s, cuerpo + ya, largo - ya, 0);
			if (k <= 0) goto cierra;
			ya += (size_t)k;
		}
		char r[512];
		if (!strncmp(ruta, "/n/", 3)) {                 /* /n/N: N octetos con su largo */
			size_t t = strtoul(ruta + 3, NULL, 10);
			snprintf(r, sizeof r, "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: %zu\r\n\r\n", t);
			if (manda_todo(s, r, strlen(r))) goto cierra;
			manda_cuerpo(s, t);
		} else if (!strncmp(ruta, "/c/", 3)) {          /* /c/N: a trozos de 1000 */
			size_t t = strtoul(ruta + 3, NULL, 10), i = 0;
			snprintf(r, sizeof r, "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n");
			if (manda_todo(s, r, strlen(r))) goto cierra;
			while (i < t) {
				size_t k = t - i < 1000 ? t - i : 1000;
				unsigned char b[1000];
				for (size_t j = 0; j < k; j++) b[j] = octeto(i + j);
				snprintf(r, sizeof r, "%zx\r\n", k);
				if (manda_todo(s, r, strlen(r)) || manda_todo(s, b, k) || manda_todo(s, "\r\n", 2)) goto cierra;
				i += k;
			}
			if (manda_todo(s, "0\r\n\r\n", 5)) goto cierra;
		} else if (!strncmp(ruta, "/r/", 3)) {          /* /r/K: redirige K veces */
			int k = atoi(ruta + 3);
			if (k > 0) snprintf(r, sizeof r, "HTTP/1.1 302 Found\r\nLocation: /r/%d\r\nContent-Length: 0\r\n\r\n", k - 1);
			else snprintf(r, sizeof r, "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nfin");
			if (manda_todo(s, r, strlen(r))) goto cierra;
		} else if (!strcmp(ruta, "/eco") && !strcmp(met, "POST")) {
			snprintf(r, sizeof r, "HTTP/1.1 200 OK\r\nContent-Length: %zu\r\n\r\n", largo);
			if (manda_todo(s, r, strlen(r)) || manda_todo(s, cuerpo, largo)) goto cierra;
		} else if (!strcmp(ruta, "/cerrar")) {          /* sin largo: hasta cerrar */
			snprintf(r, sizeof r, "HTTP/1.1 200 OK\r\nConnection: close\r\n\r\n");
			if (manda_todo(s, r, strlen(r)) == 0) manda_cuerpo(s, 50000);
			goto cierra;
		} else if (!strcmp(ruta, "/lento")) {           /* tarda 2 s */
			sleep(2);
			snprintf(r, sizeof r, "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nya");
			if (manda_todo(s, r, strlen(r))) goto cierra;
		} else {
			snprintf(r, sizeof r, "HTTP/1.1 404 Not Found\r\nContent-Length: 9\r\n\r\nno existe");
			if (manda_todo(s, r, strlen(r))) goto cierra;
		}
		if (cerrar) goto cierra;
	}
cierra:
	close(s);
	return NULL;
}

static void *servidor(void *x)
{
	(void)x;
	for (;;) {
		int s = accept(srv_fd, NULL, NULL);
		if (s < 0) { if (errno == EINTR) continue; return NULL; }
		conexiones++;
		pthread_t h;
		pthread_attr_t a;
		pthread_attr_init(&a);
		pthread_attr_setdetachstate(&a, PTHREAD_CREATE_DETACHED);
		pthread_attr_setstacksize(&a, 256 * 1024);
		if (pthread_create(&h, &a, atiende, (void *)(long)s)) close(s);
		pthread_attr_destroy(&a);
	}
}

static int arranca_servidor(void)
{
	struct sockaddr_in d = { .sin_family = AF_INET, .sin_addr.s_addr = htonl(INADDR_LOOPBACK) };
	socklen_t l = sizeof d;
	int uno = 1;
	pthread_t h;
	srv_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (srv_fd < 0) return -1;
	setsockopt(srv_fd, SOL_SOCKET, SO_REUSEADDR, &uno, sizeof uno);
	if (bind(srv_fd, (struct sockaddr *)&d, sizeof d) || listen(srv_fd, 64) || getsockname(srv_fd, (struct sockaddr *)&d, &l)) return -1;
	srv_puerto = ntohs(d.sin_port);
	return pthread_create(&h, NULL, servidor, NULL) ? -1 : 0;
}

/* ---- lo que se baja: los octetos, comprobados con el contenido de prueba, y su sha256 ---- */
struct bajada {
	size_t n;
	int iguales;                          /* ¿es lo de octeto(i)? */
	br_sha256_context sha;
	char texto[64];                       /* lo primero que llega, en texto */
};

static size_t escribe(char *d, size_t t, size_t k, void *x)
{
	struct bajada *b = x;
	size_t n = t * k;
	for (size_t i = 0; i < n; i++)
		if ((unsigned char)d[i] != octeto(b->n + i)) b->iguales = 0;
	if (b->n < sizeof b->texto - 1) {
		size_t c = n < sizeof b->texto - 1 - b->n ? n : sizeof b->texto - 1 - b->n;
		memcpy(b->texto + b->n, d, c);
		b->texto[b->n + c] = 0;
	}
	br_sha256_update(&b->sha, d, n);
	b->n += n;
	return n;
}

static void empieza(struct bajada *b)
{
	memset(b, 0, sizeof *b);
	b->iguales = 1;
	br_sha256_init(&b->sha);
}

static CURL *nuevo(const char *ruta, struct bajada *b)
{
	char u[300];
	CURL *c = curl_easy_init();
	if (!c) return NULL;
	snprintf(u, sizeof u, "http://127.0.0.1:%d%s", srv_puerto, ruta);
	curl_easy_setopt(c, CURLOPT_URL, u);
	curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, escribe);
	curl_easy_setopt(c, CURLOPT_WRITEDATA, b);
	curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(c, CURLOPT_TIMEOUT_MS, 20000L);
	empieza(b);
	return c;
}

static long código(CURL *c)
{
	long r = 0;
	curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &r);
	return r;
}

static int pruebas(void)
{
	struct bajada b;
	CURL *c;
	CURLcode e;
	long v;
	char m[200];

	printf("MCURL: libcurl %s, con la interfaz «multi», por 127.0.0.1\n", curl_version());
	MIRA(curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK, "curl_global_init");
	curl_version_info_data *vi = curl_version_info(CURLVERSION_NOW);
	MIRA(vi && (vi->features & CURL_VERSION_SSL) && (vi->features & CURL_VERSION_IPV6) && (vi->features & CURL_VERSION_ASYNCHDNS)
	     && vi->ssl_version && strstr(vi->ssl_version, "BearSSL"), "curl_version_info: SSL (BearSSL), IPv6, AsynchDNS");
	MIRA(arranca_servidor() == 0, "el servidor de prueba (127.0.0.1)");

	/* GET de 1 MB, comprobado octeto a octeto */
	c = nuevo("/n/1048576", &b);
	e = curl_easy_perform(c);
	MIRA(e == CURLE_OK && código(c) == 200 && b.n == 1048576 && b.iguales, "GET de 1 MB");
	curl_off_t cl = -1;
	curl_easy_getinfo(c, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &cl);
	MIRA(cl == 1048576, "CURLINFO_CONTENT_LENGTH_DOWNLOAD_T");
	/* la misma conexión, cinco veces más (keep-alive) */
	int nuevas = 0, vale = 1;
	for (int i = 0; i < 5; i++) {
		empieza(&b);
		snprintf(m, sizeof m, "http://127.0.0.1:%d/n/%d", srv_puerto, 1000 + i * 3333);
		curl_easy_setopt(c, CURLOPT_URL, m);
		e = curl_easy_perform(c);
		curl_easy_getinfo(c, CURLINFO_NUM_CONNECTS, &v);
		nuevas += (int)v;
		if (e != CURLE_OK || b.n != (size_t)(1000 + i * 3333) || !b.iguales) vale = 0;
	}
	MIRA(vale && nuevas == 0, "cinco GET más por la misma conexión (keep-alive: ninguna nueva)");
	printf("  GET de 1 MB y cinco más por la misma conexión (%d nuevas)\n", nuevas);
	curl_easy_cleanup(c);

	/* a trozos */
	c = nuevo("/c/300001", &b);
	e = curl_easy_perform(c);
	MIRA(e == CURLE_OK && código(c) == 200 && b.n == 300001 && b.iguales, "a trozos (chunked), 300001 octetos");
	curl_easy_cleanup(c);

	/* redirecciones */
	c = nuevo("/r/3", &b);
	curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
	e = curl_easy_perform(c);
	curl_easy_getinfo(c, CURLINFO_REDIRECT_COUNT, &v);
	MIRA(e == CURLE_OK && código(c) == 200 && v == 3 && !strcmp(b.texto, "fin"), "tres redirecciones (302)");
	curl_easy_cleanup(c);
	c = nuevo("/r/5", &b);
	curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(c, CURLOPT_MAXREDIRS, 2L);
	e = curl_easy_perform(c);
	MIRA(e == CURLE_TOO_MANY_REDIRECTS, "CURLOPT_MAXREDIRS: demasiadas redirecciones");
	curl_easy_cleanup(c);

	/* 404 */
	c = nuevo("/nada", &b);
	e = curl_easy_perform(c);
	MIRA(e == CURLE_OK && código(c) == 404 && !strcmp(b.texto, "no existe"), "404");
	curl_easy_setopt(c, CURLOPT_FAILONERROR, 1L);
	empieza(&b);
	e = curl_easy_perform(c);
	MIRA(e == CURLE_HTTP_RETURNED_ERROR, "404 con CURLOPT_FAILONERROR");
	curl_easy_cleanup(c);

	/* POST: lo devuelve tal cual */
	{
		static char d[40000];
		for (size_t i = 0; i < sizeof d; i++) d[i] = (char)octeto(i);
		c = nuevo("/eco", &b);
		curl_easy_setopt(c, CURLOPT_POSTFIELDS, d);
		curl_easy_setopt(c, CURLOPT_POSTFIELDSIZE, (long)sizeof d);
		e = curl_easy_perform(c);
		MIRA(e == CURLE_OK && código(c) == 200 && b.n == sizeof d && b.iguales, "POST de 40000 octetos, devuelto");
		curl_easy_cleanup(c);
	}

	/* Connection: close, sin largo */
	c = nuevo("/cerrar", &b);
	e = curl_easy_perform(c);
	MIRA(e == CURLE_OK && b.n == 50000 && b.iguales, "sin Content-Length, hasta que se cierra");
	curl_easy_cleanup(c);

	/* el plazo */
	c = nuevo("/lento", &b);
	curl_easy_setopt(c, CURLOPT_TIMEOUT_MS, 300L);
	double t0 = ms_ahora();
	e = curl_easy_perform(c);
	double t = ms_ahora() - t0;
	MIRA(e == CURLE_OPERATION_TIMEDOUT && t >= 290 && t < 1500, "CURLOPT_TIMEOUT_MS de 300 ms");
	printf("  el plazo de 300 ms: vence a los %.0f ms\n", t);
	curl_easy_cleanup(c);

	/* conexión rechazada (un puerto en el que no escucha nadie) */
	c = curl_easy_init();
	curl_easy_setopt(c, CURLOPT_URL, "http://127.0.0.1:1/");
	curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
	e = curl_easy_perform(c);
	MIRA(e == CURLE_COULDNT_CONNECT, "conexión rechazada (CURLE_COULDNT_CONNECT)");
	curl_easy_cleanup(c);

	/* «localhost», con el resolutor en su hilo (AsynchDNS); IPv4 o IPv6, lo que haya */
	c = curl_easy_init();
	snprintf(m, sizeof m, "http://localhost:%d/n/777", srv_puerto);
	curl_easy_setopt(c, CURLOPT_URL, m);
	curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, escribe);
	curl_easy_setopt(c, CURLOPT_WRITEDATA, &b);
	curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
	empieza(&b);
	e = curl_easy_perform(c);
	char *ip = NULL;
	curl_easy_getinfo(c, CURLINFO_PRIMARY_IP, &ip);
	MIRA(e == CURLE_OK && b.n == 777 && b.iguales && ip && !strcmp(ip, "127.0.0.1"), "«localhost» (por 127.0.0.1)");
	printf("  «localhost»: por %s\n", ip ? ip : "?");
	curl_easy_cleanup(c);

	/* 16 a la vez, con la interfaz «multi» */
	{
		enum { N = 16 };
		struct bajada bb[N];
		CURL *cc[N];
		CURLM *mu = curl_multi_init();
		int corriendo = 0, bien = 0, vueltas = 0;
		char r[32];
		curl_multi_setopt(mu, CURLMOPT_MAX_TOTAL_CONNECTIONS, 8L);
		for (int i = 0; i < N; i++) {
			snprintf(r, sizeof r, "/%c/%d", i & 1 ? 'c' : 'n', 100000 + i * 12345);
			cc[i] = nuevo(r, &bb[i]);
			curl_easy_setopt(cc[i], CURLOPT_PRIVATE, (void *)(long)i);
			curl_multi_add_handle(mu, cc[i]);
		}
		t0 = ms_ahora();
		do {
			CURLMcode mc = curl_multi_perform(mu, &corriendo);
			if (mc != CURLM_OK) break;
			if (corriendo) curl_multi_poll(mu, NULL, 0, 1000, NULL);
			vueltas++;
			CURLMsg *msg;
			int q;
			while ((msg = curl_multi_info_read(mu, &q)))
				if (msg->msg == CURLMSG_DONE) {
					long i;
					curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, (char **)&i);
					if (msg->data.result == CURLE_OK && código(msg->easy_handle) == 200
					    && bb[i].n == (size_t)(100000 + i * 12345) && bb[i].iguales) bien++;
				}
		} while (corriendo && ms_ahora() - t0 < 60000);
		t = ms_ahora() - t0;
		MIRA(bien == N, "16 a la vez con curl_multi (8 conexiones como mucho)");
		size_t total = 0;
		for (int i = 0; i < N; i++) { total += bb[i].n; curl_multi_remove_handle(mu, cc[i]); curl_easy_cleanup(cc[i]); }
		curl_multi_cleanup(mu);
		printf("  curl_multi: %d de %d bien a la vez (%zu octetos, la mitad a trozos), en %.0f ms y %d vueltas de curl_multi_poll\n", bien, N, total, t, vueltas);
	}

	/* curl_url y escape */
	{
		CURLU *u = curl_url();
		char *h = NULL, *p = NULL, *q = NULL, *x = NULL;
		MIRA(curl_url_set(u, CURLUPART_URL, "https://usuario@ejemplo.es:8443/a/b/../c?x=1#parte", 0) == CURLUE_OK
		     && curl_url_get(u, CURLUPART_HOST, &h, 0) == CURLUE_OK && !strcmp(h, "ejemplo.es")
		     && curl_url_get(u, CURLUPART_PATH, &p, 0) == CURLUE_OK && !strcmp(p, "/a/c")
		     && curl_url_get(u, CURLUPART_QUERY, &q, 0) == CURLUE_OK && !strcmp(q, "x=1"), "curl_url: el nombre, la ruta (sin ..) y la pregunta");
		MIRA(curl_url_set(u, CURLUPART_URL, "../d/e.html", 0) == CURLUE_OK && curl_url_get(u, CURLUPART_URL, &x, 0) == CURLUE_OK
		     && !strcmp(x, "https://usuario@ejemplo.es:8443/d/e.html"), "curl_url: una dirección relativa");
		curl_free(h); curl_free(p); curl_free(q); curl_free(x);
		curl_url_cleanup(u);
		c = curl_easy_init();
		char *es = curl_easy_escape(c, "año ñandú/€", 0);
		int ln = 0;
		char *un = es ? curl_easy_unescape(c, es, 0, &ln) : NULL;
		MIRA(es && !strcmp(es, "a%C3%B1o%20%C3%B1and%C3%BA%2F%E2%82%AC") && un && !strcmp(un, "año ñandú/€"), "curl_easy_escape y unescape (UTF-8)");
		curl_free(es); curl_free(un);
		curl_easy_cleanup(c);
	}
	printf("  el servidor de prueba: %d conexiones aceptadas\n", conexiones);
	close(srv_fd);
	curl_global_cleanup();
	return acaba("MCURL");
}

/* ---- bajar direcciones a la vez, con su sha256 ---- */
static int baja(int argc, char **argv)
{
	const char *ca = NULL;
	int i0 = 1, n, malas = 0;
	struct curl_slist *res = NULL;
	for (;;) {
		if (argc > i0 + 1 && !strcmp(argv[i0], "-c")) { ca = argv[i0 + 1]; i0 += 2; }
		else if (argc > i0 + 1 && !strcmp(argv[i0], "-r")) { res = curl_slist_append(res, argv[i0 + 1]); i0 += 2; }
		else break;
	}
	n = argc - i0;
	if (n <= 0) { fprintf(stderr, "uso: MCURL [-c CA.PEM] [-r NOMBRE:PUERTO:IP] DIRECCIÓN...\n"); return 2; }
	curl_global_init(CURL_GLOBAL_DEFAULT);
	CURLM *mu = curl_multi_init();
	struct bajada *bb = calloc((size_t)n, sizeof *bb);
	CURL **cc = calloc((size_t)n, sizeof *cc);
	double t0 = ms_ahora();
	for (int i = 0; i < n; i++) {
		cc[i] = curl_easy_init();
		empieza(&bb[i]);
		curl_easy_setopt(cc[i], CURLOPT_URL, argv[i0 + i]);
		curl_easy_setopt(cc[i], CURLOPT_WRITEFUNCTION, escribe);
		curl_easy_setopt(cc[i], CURLOPT_WRITEDATA, &bb[i]);
		curl_easy_setopt(cc[i], CURLOPT_NOSIGNAL, 1L);
		curl_easy_setopt(cc[i], CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(cc[i], CURLOPT_TIMEOUT_MS, 120000L);
		if (ca) curl_easy_setopt(cc[i], CURLOPT_CAINFO, ca);
		if (res) curl_easy_setopt(cc[i], CURLOPT_RESOLVE, res);
		curl_multi_add_handle(mu, cc[i]);
	}
	int corriendo;
	do {
		if (curl_multi_perform(mu, &corriendo) != CURLM_OK) break;
		if (corriendo) curl_multi_poll(mu, NULL, 0, 1000, NULL);
	} while (corriendo);
	CURLMsg *msg;
	int q;
	while ((msg = curl_multi_info_read(mu, &q)))
		if (msg->msg == CURLMSG_DONE)
			for (int i = 0; i < n; i++)
				if (cc[i] == msg->easy_handle) bb[i].iguales = msg->data.result == CURLE_OK ? 1 : -(int)msg->data.result;
	for (int i = 0; i < n; i++) {
		unsigned char h[32];
		char x[65];
		br_sha256_out(&bb[i].sha, h);
		for (int j = 0; j < 32; j++) sprintf(x + 2 * j, "%02x", h[j]);
		long r = código(cc[i]);
		if (bb[i].iguales < 0) {
			printf("MCURL: %s: %s\n", argv[i0 + i], curl_easy_strerror((CURLcode)-bb[i].iguales));
			malas++;
		} else {
			char *esq = NULL;
			long ver = 0;
			curl_easy_getinfo(cc[i], CURLINFO_SCHEME, &esq);
			curl_easy_getinfo(cc[i], CURLINFO_HTTP_VERSION, &ver);
			printf("MCURL: %s: %ld, %zu octetos; sha256 = %s (%s, HTTP/1.%d)\n", argv[i0 + i], r, bb[i].n, x, esq ? esq : "?", ver == CURL_HTTP_VERSION_1_0 ? 0 : 1);
			if (r != 200) malas++;
		}
		curl_multi_remove_handle(mu, cc[i]);
		curl_easy_cleanup(cc[i]);
	}
	printf("MCURL: %d direcciones a la vez en %.0f ms, %d mal\n", n, ms_ahora() - t0, malas);
	curl_multi_cleanup(mu);
	curl_global_cleanup();
	curl_slist_free_all(res);
	free(bb); free(cc);
	return malas ? 1 : 0;
}

int main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IOLBF, 0);
	return argc > 1 ? baja(argc, argv) : pruebas();
}
