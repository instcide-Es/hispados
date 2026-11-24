/* Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * nucleo64/programas/red/descarga.c - Hispa-64, etapa 9: DESCARGA, el
 * cliente de HTTP/1.1 y HTTPS (TLS 1.2 con BearSSL 0.6: terceros/bearssl,
 * licencia MIT, Copyright (c) 2016 Thomas Pornin).
 *   DESCARGA [-o ARCHIVO] [-n NOMBRE] [-c CA.pem|CA.der] [-k] URL
 *   -o  lo guarda en ARCHIVO (si no, solo cuenta los octetos y su sha256)
 *   -n  el nombre con el que se comprueba el certificado y que va en el SNI
 *       (por si la URL lleva una IP)
 *   -c  más autoridades (además de las de w32/tls/anclas.h): un archivo PEM
 *       (con uno o más CERTIFICATE) o un certificado en DER
 *   -k  sin comprobar el certificado (avisándolo)
 * Sigue hasta 5 redirecciones (301, 302, 303, 307, 308); entiende
 * Content-Length, «Transfer-Encoding: chunked» y hasta que se cierre. La hora
 * para el certificado, de time(). Si va bien, dice
 *   DESCARGA: N octetos en T s (V MB/s); sha256 = <64 hex>
 * y acaba con 0. Si no, dice por qué y acaba con:
 *   1 mal usado          2 no se encuentra el nombre   3 no se conecta
 *   4 TLS o certificado  5 el servidor dice que no (4xx, 5xx)
 *   6 respuesta mal hecha o cortada   7 no se puede guardar   8 demasiadas redirecciones */
#define _GNU_SOURCE
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <ctype.h>
#include <strings.h>
#include <sys/random.h>
#include "red.h"
#include "bearssl.h"
#include "anclas.h"

#define PLAZO_MS 20000
#define MAX_REDIR 5

static const char *salida, *nombre_cert, *archivo_ca;
static int sin_mirar;

/* ---- las autoridades: las de anclas.h y las de -c ---- */
static br_x509_trust_anchor *anclas;
static size_t n_anclas, n_propias;

struct trozos { unsigned char *p; size_t n; };

static void junta(void *c, const void *b, size_t n)
{
	struct trozos *t = c;
	t->p = realloc(t->p, t->n + n);
	memcpy(t->p + t->n, b, n);
	t->n += n;
}

static void *copia(const void *p, size_t n)
{
	void *q = malloc(n ? n : 1);
	memcpy(q, p, n);
	return q;
}

static int ancla_der(const unsigned char *der, size_t n)
{
	br_x509_decoder_context dc;
	struct trozos dn = { 0, 0 };
	br_x509_pkey *pk;
	br_x509_trust_anchor *ta;

	br_x509_decoder_init(&dc, junta, &dn);
	br_x509_decoder_push(&dc, der, n);
	pk = br_x509_decoder_get_pkey(&dc);
	if (!pk) {
		printf("DESCARGA: un certificado de %s no se entiende (código de BearSSL %d)\n",
		       archivo_ca, br_x509_decoder_last_error(&dc));
		free(dn.p);
		return -1;
	}
	anclas = realloc(anclas, (n_anclas + 1) * sizeof *anclas);
	ta = &anclas[n_anclas++];
	memset(ta, 0, sizeof *ta);
	ta->dn.data = dn.p;
	ta->dn.len = dn.n;
	ta->flags = br_x509_decoder_isCA(&dc) ? BR_X509_TA_CA : 0;
	ta->pkey.key_type = pk->key_type;
	if (pk->key_type == BR_KEYTYPE_RSA) {
		ta->pkey.key.rsa.n = copia(pk->key.rsa.n, pk->key.rsa.nlen);
		ta->pkey.key.rsa.nlen = pk->key.rsa.nlen;
		ta->pkey.key.rsa.e = copia(pk->key.rsa.e, pk->key.rsa.elen);
		ta->pkey.key.rsa.elen = pk->key.rsa.elen;
	} else {
		ta->pkey.key.ec.curve = pk->key.ec.curve;
		ta->pkey.key.ec.q = copia(pk->key.ec.q, pk->key.ec.qlen);
		ta->pkey.key.ec.qlen = pk->key.ec.qlen;
	}
	n_propias++;
	return 0;
}

static int lee_ca(const char *ruta)
{
	FILE *f = fopen(ruta, "rb");
	unsigned char *b = 0;
	size_t n = 0, k;
	if (!f) {
		printf("DESCARGA: no se abre %s: %s\n", ruta, strerror(errno));
		return -1;
	}
	for (;;) {
		b = realloc(b, n + 65536);
		k = fread(b + n, 1, 65536, f);
		if (!k)
			break;
		n += k;
	}
	fclose(f);
	if (n >= 10 && memmem(b, n, "-----BEGIN", 10)) {
		/* PEM: lo de entre «-----BEGIN CERTIFICATE-----» y «-----END», en base 64 */
		const char *p = (const char *)b, *fin = (const char *)b + n;
		for (;;) {
			const char *ini = memmem(p, fin - p, "-----BEGIN ", 11), *cab, *pie;
			if (!ini)
				break;
			cab = memmem(ini + 11, fin - ini - 11, "-----", 5);
			if (!cab)
				break;
			pie = memmem(cab + 5, fin - cab - 5, "-----END", 8);
			if (!pie) {
				printf("DESCARGA: %s no es un PEM bien hecho (falta «-----END»)\n", ruta);
				return -1;
			}
			if ((cab - ini == 22 && !memcmp(ini + 11, "CERTIFICATE", 11)) ||
			    (cab - ini == 27 && !memcmp(ini + 11, "X509 CERTIFICATE", 16))) {
				unsigned char *der = malloc(pie - cab);
				size_t k = 0;
				unsigned v = 0;
				int bits = 0;
				const char *q;
				for (q = cab + 5; q < pie; q++) {
					int d;
					char c = *q;
					if (c >= 'A' && c <= 'Z') d = c - 'A';
					else if (c >= 'a' && c <= 'z') d = c - 'a' + 26;
					else if (c >= '0' && c <= '9') d = c - '0' + 52;
					else if (c == '+') d = 62;
					else if (c == '/') d = 63;
					else continue;        /* saltos de línea, espacios, «=» */
					v = (v << 6) | d;
					bits += 6;
					if (bits >= 8) {
						bits -= 8;
						der[k++] = (v >> bits) & 0xff;
					}
				}
				if (ancla_der(der, k) < 0)
					return -1;
				free(der);
			}
			p = pie + 8;
		}
	} else if (ancla_der(b, n) < 0)
		return -1;
	free(b);
	if (!n_propias) {
		printf("DESCARGA: en %s no hay ningún certificado\n", ruta);
		return -1;
	}
	return 0;
}

/* ---- -k: una comprobación que no comprueba (solo saca la clave del primero) ---- */
struct sin_comprobar {
	const br_x509_class *vtable;
	br_x509_decoder_context dc;
	int n;
};

static void sc_start_chain(const br_x509_class **c, const char *nombre)
{
	struct sin_comprobar *s = (void *)c;
	(void)nombre;
	s->n = 0;
}
static void sc_start_cert(const br_x509_class **c, uint32_t n)
{
	struct sin_comprobar *s = (void *)c;
	(void)n;
	if (s->n == 0)
		br_x509_decoder_init(&s->dc, 0, 0);
}
static void sc_append(const br_x509_class **c, const unsigned char *b, size_t n)
{
	struct sin_comprobar *s = (void *)c;
	if (s->n == 0)
		br_x509_decoder_push(&s->dc, b, n);
}
static void sc_end_cert(const br_x509_class **c)
{
	struct sin_comprobar *s = (void *)c;
	s->n++;
}
static unsigned sc_end_chain(const br_x509_class **c)
{
	struct sin_comprobar *s = (void *)c;
	return (s->n && br_x509_decoder_get_pkey(&s->dc)) ? 0 : BR_ERR_X509_EMPTY_CHAIN;
}
static const br_x509_pkey *sc_get_pkey(const br_x509_class *const *c, unsigned *usos)
{
	struct sin_comprobar *s = (void *)c;
	if (usos)
		*usos = BR_KEYTYPE_KEYX | BR_KEYTYPE_SIGN;
	return br_x509_decoder_get_pkey(&s->dc);
}
static const br_x509_class sc_vtable = {
	sizeof(struct sin_comprobar), sc_start_chain, sc_start_cert, sc_append,
	sc_end_cert, sc_end_chain, sc_get_pkey
};
static struct sin_comprobar sc = { &sc_vtable };

/* ---- la conexión: el socket, y encima TLS si es https ---- */
static struct {
	int fd, tls, cerrado;
	br_ssl_client_context cc;
	br_x509_minimal_context xc;
	unsigned char iobuf[BR_SSL_BUFSIZE_BIDI];
	unsigned char rb[16384];
	size_t rp, rn;
} C;

static int espera(int fd, short ev)
{
	struct pollfd p = { fd, ev, 0 };
	int r;
	do
		r = poll(&p, 1, PLAZO_MS);
	while (r < 0 && errno == EINTR);
	if (r == 0)
		errno = ETIMEDOUT;
	return r > 0 ? 0 : -1;
}

static int sock_lee(void *c, unsigned char *b, size_t n)
{
	ssize_t r;
	(void)c;
	for (;;) {
		if (espera(C.fd, POLLIN))
			return -1;
		r = read(C.fd, b, n);
		if (r > 0)
			return r;
		if (r == 0) {
			C.cerrado = 1;
			return -1;
		}
		if (errno != EINTR && errno != EAGAIN)
			return -1;
	}
}

static int sock_escribe(void *c, const unsigned char *b, size_t n)
{
	ssize_t r;
	(void)c;
	for (;;) {
		if (espera(C.fd, POLLOUT))
			return -1;
		r = write(C.fd, b, n);
		if (r >= 0)
			return r;
		if (errno != EINTR && errno != EAGAIN)
			return -1;
	}
}

static const char *error_tls(int e)
{
	static char t[160];
	const char *s;
	switch (e) {
	case BR_ERR_X509_NOT_TRUSTED: s = "es de una autoridad que no se conoce (prueba con -c CA.pem)"; break;
	case BR_ERR_X509_BAD_SERVER_NAME: s = "es de otro nombre (prueba con -n NOMBRE)"; break;
	case BR_ERR_X509_EXPIRED: s = "está caducado o aún no vale (¿va bien la hora?)"; break;
	case BR_ERR_X509_TIME_UNKNOWN: s = "no se sabe la hora"; break;
	case BR_ERR_X509_BAD_SIGNATURE: s = "tiene la firma mal"; break;
	case BR_ERR_X509_NOT_CA: s = "lo firma uno que no es autoridad"; break;
	case BR_ERR_X509_FORBIDDEN_KEY_USAGE: s = "no vale para un servidor (uso de la clave)"; break;
	case BR_ERR_X509_WEAK_PUBLIC_KEY: s = "tiene una clave demasiado corta"; break;
	case BR_ERR_X509_UNSUPPORTED: s = "usa algo que no se sabe comprobar"; break;
	case BR_ERR_X509_EMPTY_CHAIN: s = "no ha llegado"; break;
	default: s = "no se entiende";
	}
	if (e >= BR_ERR_X509_OK && e < 64)
		snprintf(t, sizeof t, "certificado no válido: %s (código de BearSSL %d)", s, e);
	else if (e & BR_ERR_RECV_FATAL_ALERT)
		snprintf(t, sizeof t, "el servidor ha cortado el TLS con la alerta %d", e & 255);
	else if (e & BR_ERR_SEND_FATAL_ALERT)
		snprintf(t, sizeof t, "se ha cortado el TLS con la alerta %d (código de BearSSL %d)", e & 255, e);
	else if (e == BR_ERR_UNSUPPORTED_VERSION || e == BR_ERR_BAD_VERSION || e == BR_ERR_BAD_LENGTH || e == BR_ERR_UNKNOWN_TYPE)
		snprintf(t, sizeof t, "el servidor no habla TLS 1.2 (¿es http://?) (código de BearSSL %d)", e);
	else if (e == BR_ERR_IO)
		snprintf(t, sizeof t, "la conexión se ha cortado en medio del TLS%s", C.cerrado ? "" : " (o no contesta)");
	else
		snprintf(t, sizeof t, "error de TLS (código de BearSSL %d)", e);
	return t;
}

/* BearSSL pregunta por el generador de azar del sistema: se le da la semilla
 * a mano (br_ssl_engine_inject_entropy), de getrandom o de /dev/urandom */
/* (de ssl_engine.c, sin cabecera pública: como lo hace sslio.c) */
void br_ssl_engine_fail(br_ssl_engine_context *cc, int err);

br_prng_seeder br_prng_seeder_system(const char **nombre)
{
	if (nombre)
		*nombre = "ninguno";
	return 0;
}

static int azar(unsigned char *b, size_t n)
{
	int fd;
	if (getrandom(b, n, 0) == (ssize_t)n)
		return 0;
	fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return -1;
	if (read(fd, b, n) != (ssize_t)n) {
		close(fd);
		return -1;
	}
	close(fd);
	return 0;
}

/* el motor de TLS: manda y recibe registros hasta que se pueda lo que se
 * quiere (BR_SSL_SENDAPP o BR_SSL_RECVAPP); -1 si se cierra o falla */
static int bombea(unsigned quiero)
{
	br_ssl_engine_context *e = &C.cc.eng;
	for (;;) {
		unsigned st = br_ssl_engine_current_state(e);
		unsigned char *b;
		size_t n;
		int r;
		if (st & BR_SSL_CLOSED)
			return -1;
		if (st & BR_SSL_SENDREC) {
			b = br_ssl_engine_sendrec_buf(e, &n);
			r = sock_escribe(0, b, n);
			if (r < 0) {
				br_ssl_engine_fail(e, BR_ERR_IO);
				return -1;
			}
			br_ssl_engine_sendrec_ack(e, r);
			continue;
		}
		if (st & quiero)
			return 0;
		if (st & BR_SSL_RECVAPP)
			return -1;      /* llegan datos y se quería mandar: no pasa en HTTP */
		if (st & BR_SSL_RECVREC) {
			b = br_ssl_engine_recvrec_buf(e, &n);
			r = sock_lee(0, b, n);
			if (r < 0) {
				br_ssl_engine_fail(e, BR_ERR_IO);
				return -1;
			}
			br_ssl_engine_recvrec_ack(e, r);
			continue;
		}
		br_ssl_engine_flush(e, 0);
	}
}

static int tls_manda(const unsigned char *d, size_t n)
{
	br_ssl_engine_context *e = &C.cc.eng;
	while (n) {
		unsigned char *b;
		size_t q;
		if (bombea(BR_SSL_SENDAPP))
			return -1;
		b = br_ssl_engine_sendapp_buf(e, &q);
		if (q > n)
			q = n;
		memcpy(b, d, q);
		br_ssl_engine_sendapp_ack(e, q);
		d += q;
		n -= q;
	}
	br_ssl_engine_flush(e, 0);
	return bombea(BR_SSL_SENDAPP);
}

static int tls_recibe(unsigned char *d, size_t n)
{
	br_ssl_engine_context *e = &C.cc.eng;
	unsigned char *b;
	size_t q;
	if (bombea(BR_SSL_RECVAPP))
		return -1;
	b = br_ssl_engine_recvapp_buf(e, &q);
	if (q > n)
		q = n;
	memcpy(d, b, q);
	br_ssl_engine_recvapp_ack(e, q);
	return q;
}

static int conecta(const char *equipo, int puerto, int tls)
{
	struct sockaddr_in sa;
	int e = 0, fl;
	socklen_t l = sizeof e;
	char ip[INET_ADDRSTRLEN];

	if (busca("DESCARGA", equipo, puerto, SOCK_STREAM, &sa))
		return 2;
	inet_ntop(AF_INET, &sa.sin_addr, ip, sizeof ip);
	C.fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (C.fd < 0) {
		printf("DESCARGA: no se abre el socket: %s\n", strerror(errno));
		return 3;
	}
	fl = fcntl(C.fd, F_GETFL);
	fcntl(C.fd, F_SETFL, fl | O_NONBLOCK);
	if (connect(C.fd, (struct sockaddr *)&sa, sizeof sa) < 0) {
		if (errno != EINPROGRESS)
			e = errno;
		else if (espera(C.fd, POLLOUT))
			e = ETIMEDOUT;
		else if (getsockopt(C.fd, SOL_SOCKET, SO_ERROR, &e, &l) < 0)
			e = errno;
	}
	if (e) {
		printf("DESCARGA: no se conecta con %s:%d: %s\n", ip, puerto,
		       e == ECONNREFUSED ? "rechazada (no escucha nadie)" :
		       e == ETIMEDOUT ? "no contesta (plazo)" :
		       e == ENETUNREACH ? "no hay camino (¿hay red?)" : strerror(e));
		close(C.fd);
		return 3;
	}
	printf("DESCARGA: conectado con %s:%d%s\n", ip, puerto, tls ? " (TLS)" : "");
	C.tls = tls;
	C.cerrado = 0;
	C.rp = C.rn = 0;
	if (tls) {
		time_t t = time(0);
		const char *sni = nombre_cert ? nombre_cert : equipo;
		static const uint16_t suites[] = {
			BR_TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256,
			BR_TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256,
			BR_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
			BR_TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
			BR_TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
			BR_TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384
		};
		unsigned char az[32];
		br_ssl_engine_context *e = &C.cc.eng;
		/* el perfil de TLS.BIN de Hispa-Pentium (castel/nucleo32/tls/tls.c):
		 * TLS 1.2, ECDHE con ChaCha20-Poly1305 o AES-GCM */
		br_ssl_client_zero(&C.cc);
		br_ssl_engine_set_versions(e, BR_TLS12, BR_TLS12);
		br_ssl_engine_set_suites(e, suites, sizeof suites / sizeof suites[0]);
		br_ssl_engine_set_default_rsavrfy(e);
		br_ssl_engine_set_default_ecdsa(e);
		br_ssl_engine_set_ec(e, &br_ec_all_m31);
		br_ssl_engine_set_hash(e, br_sha256_ID, &br_sha256_vtable);
		br_ssl_engine_set_hash(e, br_sha384_ID, &br_sha384_vtable);
		br_ssl_engine_set_prf_sha256(e, &br_tls12_sha256_prf);
		br_ssl_engine_set_prf_sha384(e, &br_tls12_sha384_prf);
		br_ssl_engine_set_chacha20(e, &br_chacha20_ct_run);
		br_ssl_engine_set_poly1305(e, &br_poly1305_ctmul32_run);
		br_ssl_engine_set_chapol(e, &br_sslrec_in_chapol_vtable, &br_sslrec_out_chapol_vtable);
		br_ssl_engine_set_aes_ctr(e, &br_aes_big_ctr_vtable);
		br_ssl_engine_set_ghash(e, &br_ghash_ctmul32);
		br_ssl_engine_set_gcm(e, &br_sslrec_in_gcm_vtable, &br_sslrec_out_gcm_vtable);
		br_x509_minimal_init(&C.xc, &br_sha256_vtable, anclas, n_anclas);
		br_x509_minimal_set_rsa(&C.xc, &br_rsa_i31_pkcs1_vrfy);
		br_x509_minimal_set_ecdsa(&C.xc, &br_ec_all_m31, &br_ecdsa_i31_vrfy_asn1);
		br_x509_minimal_set_hash(&C.xc, br_sha256_ID, &br_sha256_vtable);
		br_x509_minimal_set_hash(&C.xc, br_sha384_ID, &br_sha384_vtable);
		br_x509_minimal_set_hash(&C.xc, br_sha512_ID, &br_sha512_vtable);
		/* la hora: días desde el 1 de enero del año 0 y segundos del día */
		br_x509_minimal_set_time(&C.xc, (uint32_t)(t / 86400 + 719528), (uint32_t)(t % 86400));
		br_ssl_engine_set_x509(e, sin_mirar ? &sc.vtable : &C.xc.vtable);
		br_ssl_engine_set_buffer(e, C.iobuf, sizeof C.iobuf, 1);
		if (azar(az, sizeof az)) {
			printf("DESCARGA: no hay azar para el TLS (ni getrandom ni /dev/urandom)\n");
			close(C.fd);
			return 4;
		}
		br_ssl_engine_inject_entropy(e, az, sizeof az);
		if (!br_ssl_client_reset(&C.cc, sni, 0)) {
			printf("DESCARGA: no arranca el TLS: %s\n", error_tls(br_ssl_engine_last_error(e)));
			close(C.fd);
			return 4;
		}
	}
	return 0;
}

static int manda(const void *b, size_t n)
{
	if (C.tls)
		return tls_manda(b, n);
	while (n) {
		int r = sock_escribe(0, b, n);
		if (r < 0)
			return -1;
		b = (const char *)b + r;
		n -= r;
	}
	return 0;
}

/* lo que llega: >0 octetos, 0 fin, -1 error */
static int recibe(void *b, size_t n)
{
	if (C.rp < C.rn) {
		size_t k = C.rn - C.rp < n ? C.rn - C.rp : n;
		memcpy(b, C.rb + C.rp, k);
		C.rp += k;
		return k;
	}
	if (C.tls) {
		int r = tls_recibe(b, n);
		if (r >= 0)
			return r;
		r = br_ssl_engine_last_error(&C.cc.eng);
		/* cerrado como es debido, o el otro ha cerrado el socket sin close_notify */
		if (r == BR_ERR_OK || (r == BR_ERR_IO && C.cerrado))
			return 0;
		return -1;
	}
	{
		int r = sock_lee(0, b, n);
		if (r < 0 && C.cerrado)
			return 0;
		return r;
	}
}

static int línea(char *l, size_t max)
{
	size_t n = 0;
	for (;;) {
		char c;
		if (C.rp >= C.rn) {
			int r = recibe(C.rb, sizeof C.rb);
			if (r <= 0)
				return -1;
			C.rp = 0;
			C.rn = r;
		}
		c = C.rb[C.rp++];
		if (c == '\n')
			break;
		if (n + 1 < max)
			l[n++] = c;
	}
	if (n && l[n - 1] == '\r')
		n--;
	l[n] = 0;
	return n;
}

static void cierra(void)
{
	if (C.tls && !(br_ssl_engine_current_state(&C.cc.eng) & BR_SSL_CLOSED)) {
		/* el close_notify, sin esperar la contestación */
		br_ssl_engine_close(&C.cc.eng);
		while (br_ssl_engine_current_state(&C.cc.eng) & BR_SSL_SENDREC) {
			size_t n;
			unsigned char *b = br_ssl_engine_sendrec_buf(&C.cc.eng, &n);
			int r = write(C.fd, b, n);
			if (r <= 0)
				break;
			br_ssl_engine_sendrec_ack(&C.cc.eng, r);
		}
	}
	close(C.fd);
}

static int parte_url(const char *url, int *tls, char *equipo, size_t ne, int *puerto, char *ruta, size_t nr)
{
	const char *p, *f;
	size_t n;
	if (!strncasecmp(url, "http://", 7)) { *tls = 0; *puerto = 80; p = url + 7; }
	else if (!strncasecmp(url, "https://", 8)) { *tls = 1; *puerto = 443; p = url + 8; }
	else return -1;
	f = p + strcspn(p, "/?#");
	n = f - p;
	{
		const char *dp = memchr(p, ':', n);
		size_t ln = dp ? (size_t)(dp - p) : n;
		if (!ln || ln >= ne)
			return -1;
		memcpy(equipo, p, ln);
		equipo[ln] = 0;
		if (dp) {
			*puerto = atoi(dp + 1);
			if (*puerto <= 0 || *puerto > 65535)
				return -1;
		}
	}
	if (*f == '/')
		snprintf(ruta, nr, "%.*s", (int)strcspn(f, "#"), f);
	else
		snprintf(ruta, nr, "/%.*s", (int)strcspn(f, "#"), f);
	return 0;
}

int main(int argc, char **argv)
{
	const char *url0 = 0;
	static char url[4096], equipo[256], ruta[4096], l[8192], loc[4096];
	int i, tls, puerto, r, redir;
	unsigned char h[32];
	long long total = 0;
	double t0;
	br_sha256_context sh;
	FILE *f = 0;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-o") && i + 1 < argc) salida = argv[++i];
		else if (!strcmp(argv[i], "-n") && i + 1 < argc) nombre_cert = argv[++i];
		else if (!strcmp(argv[i], "-c") && i + 1 < argc) archivo_ca = argv[++i];
		else if (!strcmp(argv[i], "-k")) sin_mirar = 1;
		else if (argv[i][0] == '-' || url0) url0 = 0, i = argc;
		else url0 = argv[i];
	}
	if (!url0) {
		printf("uso: DESCARGA [-o ARCHIVO] [-n NOMBRE] [-c CA.pem|CA.der] [-k] URL\n");
		return 1;
	}
	anclas = malloc(sizeof TAs);
	memcpy(anclas, TAs, sizeof TAs);
	n_anclas = TAs_NUM;
	if (archivo_ca) {
		if (lee_ca(archivo_ca))
			return 1;
		printf("DESCARGA: %zu %s de %s, además de las %d de siempre\n", n_propias,
		       n_propias == 1 ? "autoridad" : "autoridades", archivo_ca, TAs_NUM);
	}
	snprintf(url, sizeof url, "%s", url0);
	t0 = ms_ahora();
	for (redir = 0;; redir++) {
		int estado = 0, troceado = 0, cierra_sin_largo;
		long long largo = -1;
		if (parte_url(url, &tls, equipo, sizeof equipo, &puerto, ruta, sizeof ruta)) {
			printf("DESCARGA: la URL «%s» no se entiende (http://equipo[:puerto]/ruta o https://...)\n", url);
			return 1;
		}
		if (tls && sin_mirar && redir == 0)
			printf("DESCARGA: AVISO: el certificado no se comprueba (-k): cualquiera podría estar en medio\n");
		if ((r = conecta(equipo, puerto, tls)))
			return r;
		if ((tls ? puerto != 443 : puerto != 80))
			snprintf(l, sizeof l, "GET %s HTTP/1.1\r\nHost: %s:%d\r\nUser-Agent: DESCARGA (Hispa-64)\r\n"
			         "Accept: */*\r\nAccept-Encoding: identity\r\nConnection: close\r\n\r\n", ruta, equipo, puerto);
		else
			snprintf(l, sizeof l, "GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: DESCARGA (Hispa-64)\r\n"
			         "Accept: */*\r\nAccept-Encoding: identity\r\nConnection: close\r\n\r\n", ruta, equipo);
		if (manda(l, strlen(l))) {
			if (tls)
				printf("DESCARGA: %s\n", error_tls(br_ssl_engine_last_error(&C.cc.eng)));
			else
				printf("DESCARGA: no sale el pedido: %s\n", strerror(errno));
			close(C.fd);
			return tls ? 4 : 3;
		}
		if (tls) {
			br_ssl_session_parameters sp;
			br_ssl_engine_get_session_parameters(&C.cc.eng, &sp);
			if (sin_mirar)
				printf("DESCARGA: TLS 1.2 (suite 0x%04X), certificado sin comprobar\n", sp.cipher_suite);
			else
				printf("DESCARGA: TLS 1.2 (suite 0x%04X), certificado de «%s» bien\n", sp.cipher_suite,
				       nombre_cert ? nombre_cert : equipo);
		}
		if (línea(l, sizeof l) < 0) {
			printf("DESCARGA: %s\n", tls && br_ssl_engine_last_error(&C.cc.eng) != BR_ERR_OK && !C.cerrado ?
			       error_tls(br_ssl_engine_last_error(&C.cc.eng)) : "el servidor no contesta nada");
			close(C.fd);
			return tls ? 4 : 6;
		}
		if (sscanf(l, "HTTP/%*d.%*d %d", &estado) != 1) {
			printf("DESCARGA: la respuesta no es de HTTP: «%.80s»\n", l);
			cierra();
			return 6;
		}
		loc[0] = 0;
		for (;;) {
			if (línea(l, sizeof l) < 0) {
				printf("DESCARGA: la cabecera de la respuesta se corta\n");
				cierra();
				return 6;
			}
			if (!l[0])
				break;
			if (!strncasecmp(l, "Content-Length:", 15))
				largo = atoll(l + 15);
			else if (!strncasecmp(l, "Transfer-Encoding:", 18) && strcasestr(l + 18, "chunked"))
				troceado = 1;
			else if (!strncasecmp(l, "Location:", 9)) {
				char *p = l + 9;
				while (*p == ' ' || *p == '\t')
					p++;
				snprintf(loc, sizeof loc, "%.4000s", p);
			}
		}
		printf("DESCARGA: HTTP %d", estado);
		if (troceado) printf(", a trozos (chunked)");
		else if (largo >= 0) printf(", %lld octetos (Content-Length)", largo);
		else printf(", hasta que se cierre");
		printf("\n");
		if ((estado == 301 || estado == 302 || estado == 303 || estado == 307 || estado == 308) && loc[0]) {
			cierra();
			if (redir >= MAX_REDIR) {
				printf("DESCARGA: demasiadas redirecciones (más de %d)\n", MAX_REDIR);
				return 8;
			}
			if (!strncasecmp(loc, "http://", 7) || !strncasecmp(loc, "https://", 8))
				snprintf(l, sizeof l, "%s", loc);
			else {
				const char *esq = tls ? "https" : "http";
				char base[4096];
				if (loc[0] == '/')
					snprintf(base, sizeof base, "%s", loc);
				else {
					char *b = strrchr(ruta, '/');
					snprintf(base, sizeof base, "%.*s%s", (int)(b - ruta + 1), ruta, loc);
				}
				snprintf(l, sizeof l, "%s://%s:%d%s", esq, equipo, puerto, base);
			}
			snprintf(url, sizeof url, "%s", l);
			printf("DESCARGA: redirige a %s\n", url);
			continue;
		}
		if (estado < 200 || estado > 299) {
			printf("DESCARGA: el servidor dice que no: HTTP %d\n", estado);
			cierra();
			return 5;
		}
		if (salida && !(f = fopen(salida, "wb"))) {
			printf("DESCARGA: no se puede escribir %s: %s\n", salida, strerror(errno));
			cierra();
			return 7;
		}
		br_sha256_init(&sh);
		cierra_sin_largo = !troceado && largo < 0;
		for (;;) {
			long long falta;
			static unsigned char b[65536];
			if (troceado) {
				char *fin;
				if (línea(l, sizeof l) < 0)
					goto cortada;
				falta = strtoll(l, &fin, 16);
				if (fin == l || falta < 0) {
					printf("DESCARGA: un trozo (chunked) mal hecho: «%.40s»\n", l);
					goto mal;
				}
				if (falta == 0) {
					while (línea(l, sizeof l) > 0)
						;      /* lo de detrás (trailer) */
					break;
				}
			} else
				falta = cierra_sin_largo ? -1 : largo - total;
			while (falta != 0) {
				size_t q = (falta < 0 || falta > (long long)sizeof b) ? sizeof b : (size_t)falta;
				r = recibe(b, q);
				if (r == 0 && cierra_sin_largo)
					break;
				if (r <= 0)
					goto cortada;
				br_sha256_update(&sh, b, r);
				if (f && fwrite(b, 1, r, f) != (size_t)r) {
					printf("DESCARGA: no se puede escribir %s: %s\n", salida, strerror(errno));
					fclose(f);
					unlink(salida);
					cierra();
					return 7;
				}
				total += r;
				if (falta > 0)
					falta -= r;
			}
			if (!troceado)
				break;
			if (línea(l, sizeof l) != 0) {
				printf("DESCARGA: un trozo (chunked) no acaba en CRLF\n");
				goto mal;
			}
		}
		break;
cortada:
		if (C.tls && br_ssl_engine_last_error(&C.cc.eng) != BR_ERR_OK && !C.cerrado)
			printf("DESCARGA: se corta a los %lld octetos: %s\n", total, error_tls(br_ssl_engine_last_error(&C.cc.eng)));
		else
			printf("DESCARGA: se corta a los %lld octetos%s\n", total, errno == ETIMEDOUT ? " (no llega nada: plazo)" : "");
mal:
		if (f) {
			fclose(f);
			unlink(salida);
		}
		cierra();
		return 6;
	}
	cierra();
	if (f && fclose(f)) {
		printf("DESCARGA: no se puede escribir %s: %s\n", salida, strerror(errno));
		return 7;
	}
	br_sha256_out(&sh, h);
	{
		double s = (ms_ahora() - t0) / 1000;
		printf("DESCARGA: %lld octetos en %s s (%s MB/s); sha256 = ", total, coma(s, 2),
		       coma(total / 1048576.0 / (s > 0.001 ? s : 0.001), 1));
		for (i = 0; i < 32; i++)
			printf("%02x", h[i]);
		printf("\n");
	}
	if (salida)
		printf("DESCARGA: guardado en %s\n", salida);
	return 0;
}
