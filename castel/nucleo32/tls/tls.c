/* =============================================================================
 *  tls.c - TLS.BIN, el servicio TLS de Hispa-Pentium (docs/PENTIUM.md, «HTTPS:
 *  el servicio TLS»): las conexiones cifradas (TLS 1.2 de cliente) para los
 *  programas de Castel, que las piden como una TCP (redes.cas:
 *  conectar_seguro, y luego enviar, recibir y cerrar_conexión como siempre).
 *  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 *
 *  Es un programa del anillo 3, como RED.BIN, que pone en marcha el núcleo
 *  la primera vez que un programa pide algo por la llamada 99 (red.cas). Se
 *  apunta como servicio TLS (91), coge los pedidos (92) y los contesta (93);
 *  las conexiones TCP de debajo se las pide a RED.BIN por la llamada 90,
 *  como cualquier programa. No hay hilos: un bucle que atiende los pedidos y
 *  mueve los registros TLS de cada conexión entre BearSSL y RED.BIN; lo que
 *  un programa espera (el apretón de manos, lo que tiene que llegar) se
 *  contesta cuando está, y mientras tanto se atiende a los demás.
 *
 *  La criptografía y el protocolo son de BearSSL 0.6 (Thomas Pornin, licencia
 *  MIT: terceros/bearssl). Lo de aquí es lo de HSPTLS.DLL de Hispa-486
 *  (w32/tls/hsptls.c), pasado a este sistema:
 *   - el perfil: solo TLS 1.2 (BearSSL no tiene TLS 1.3), ECDHE (Curve25519,
 *     P-256, P-384), certificado ECDSA o RSA, ChaCha20-Poly1305 o AES-GCM;
 *     con SNI (el nombre del servidor va en el saludo);
 *   - las autoridades raíz: las de HSPTLS (w32/tls/anclas.h, de
 *     w32/tls/raices con tools/mkanclas.sh) y los *.CER (X.509 en DER) de
 *     \HISPA\RAICES y de \RAICES de la unidad del sistema;
 *   - la memoria de certificados: una cadena ya comprobada para un servidor
 *     (su resumen SHA-256, con el nombre) vale una semana sin volver a
 *     comprobar sus firmas (en la memoria del servicio y en TLS.CCH); y la
 *     reanudación de sesiones (4 servidores);
 *   - el azar: el contador de ciclos (TSC) cada vez que llega algo de la red,
 *     los tics, la fecha y una semilla que se guarda de una vez para otra
 *     (TLS.SEM);
 *   - LA HORA, para las fechas de los certificados: la del reloj del PC (la
 *     CMOS, llamada 15) si es creíble (no antes del día en que se hizo este
 *     TLS.BIN, ni 30 años después); si no, la de la cabecera «Date» de una
 *     petición HTTP (sin cifrar) a actualiza.hispados.es (o al servidor que
 *     diga «hora=» en TLS.INI). Eso es un riesgo, y se apunta: quien pueda
 *     cambiar esa respuesta puede hacer pasar por bueno un certificado
 *     caducado (no uno falso: la firma se sigue comprobando).
 *  TLS.CCH, TLS.SEM y TLS.INI van en \HISPA de la unidad del sistema (o en
 *  su raíz si no hay \HISPA).
 * ============================================================================= */
#include "bearssl.h"
#include "anclas.h"
#include "tls.h"

#ifndef COMPILADO
#define COMPILADO 20261120	/* el día en que se hizo (lo pone hacer.sh) */
#endif

#define MAXC 8			/* conexiones a la vez */
#define SALIDA (4096 + 85)	/* lo que se envía: registros de hasta 4 K */
#define CAD_MAX 10240		/* la cadena de certificados, para recordarla */
#define CAD_N 8

enum { P_NADA, P_CONECTAR, P_RECIBIR };

struct conex {
	br_ssl_client_context cc;
	br_x509_minimal_context xc;
	const br_x509_class *xv;	/* lo que ve BearSSL: la memoria, delante */
	br_x509_decoder_context dc;
	u32 cad_n, cad_len[CAD_N], n_cert, dias;
	u8 directo, de_memoria, reanudada;
	int usada, hecho, fin_tcp, error;
	u32 error_red;		/* lo que dijo RED.BIN si se cortó */
	u32 dueno, tcp, num;
	int p_clase;		/* lo que espera el programa */
	u32 p_hueco, p_serie, p_cabe, p_limite;
	u32 t_ini, ms;		/* el apretón de manos: tics al empezar, y lo que ha tardado */
	u32 recibidos, enviados;	/* los octetos de los datos (descifrados y por cifrar) */
	unsigned long long ciclos, ciclos_hs;
	char servidor[64];
	u8 cadena[CAD_MAX];
	u8 entrada[BR_SSL_BUFSIZE_INPUT];
	u8 salida[SALIDA];
};
static struct conex *conex[MAXC];

static u8 pedido[64 + MAX_DATOS];
static u8 trozo[MAX_DATOS];
static u32 hz = 100, n_apretones, memoria_usada;
static char carpeta[16] = "\\";		/* «\HISPA\» o «\» */

static u32 tics(void) { return sc(SC_TICS, 0, 0, 0); }

/* ---- el registro: en la memoria (O_REGISTRO) y por el puerto serie ---- */
#define CABE_REG 6000
static char registro[CABE_REG];
static u32 registro_largo;

static void apunta(const char *f, ...)
{
	char l[360], s[370];
	__builtin_va_list va;
	int vformatear(char *, int, const char *, __builtin_va_list);
	u32 n;

	__builtin_va_start(va, f);
	n = vformatear(l, sizeof l, f, va);
	__builtin_va_end(va);
	if (registro_largo + n + 2 > CABE_REG) {
		memcpy(registro, registro + CABE_REG / 2, registro_largo - CABE_REG / 2);
		registro_largo -= CABE_REG / 2;
	}
	memcpy(registro + registro_largo, l, n);
	registro_largo += n;
	registro[registro_largo++] = '\r';
	registro[registro_largo++] = '\n';
	n = formatear(s, sizeof s, "tls: %s\r\n", l);
	sc(SC_SERIE, (u32)s, n, 0);
}

/* ---- los archivos (de la unidad del sistema) ---- */
static u32 ruta(char *d, const char *nombre)
{
	return formatear(d, 40, "%s%s", carpeta, nombre);
}

static int lee_archivo(const char *nombre, void *d, u32 cabe)
{
	char r[40];
	u32 m, n;

	ruta(r, nombre);
	m = sc(SC_ABRIR, (u32)r, 0, 0);
	if (ES_ERROR(m))
		return -1;
	n = sc(SC_LEER, m, (u32)d, cabe);
	sc(SC_CERRAR, m, 0, 0);
	return ES_ERROR(n) ? -1 : (int)n;
}

static void escribe_archivo(const char *nombre, const void *d, u32 n)
{
	char r[40];
	u32 m;

	ruta(r, nombre);
	m = sc(SC_ABRIR, (u32)r, 2, 0);
	if (ES_ERROR(m))
		return;		/* (un disco sin permiso de escritura: da igual) */
	sc(SC_ESCRIBIR_ARCHIVO, m, (u32)d, n);
	sc(SC_CERRAR, m, 0, 0);
}

/* ---- la memoria: cada conexión, la primera vez que hace falta (llamada 17) ---- */
static void *mas_memoria(u32 n)
{
	u32 r = sc(SC_MAS_MEMORIA, (n + 15) & ~15u, 0, 0);

	if (ES_ERROR(r))
		return 0;
	memoria_usada += n;
	return (void *)r;
}

/* ---- el azar ---- */
static br_sha256_context pozo;
static int hay_tsc;

static u32 tsc(void)
{
	u32 a = 0, d;

	if (hay_tsc)
		__asm__ volatile("rdtsc" : "=a"(a), "=d"(d));
	return a;
}

static int cpuid_tsc(void)
{
	u32 a, b, c, d, f1, f2;

	__asm__ volatile("pushfl; popl %0; movl %0, %1; xorl $0x200000, %0; pushl %0; popfl; pushfl; popl %0; pushl %1; popfl"
			 : "=&r"(f1), "=&r"(f2));
	if (!((f1 ^ f2) & 0x200000))
		return 0;
	__asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(0));
	if (a < 1)
		return 0;
	__asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1));
	return (d >> 4) & 1;
}

static void ruido(void)
{
	u32 v[4];

	v[0] = tics();
	v[1] = tsc();
	v[2] = hay_tsc ? 0 : sc(SC_DATO, DATO_FECHA, 0, 0);
	v[3] = (u32)&v;
	br_sha256_update(&pozo, v, sizeof v);
}

static void pozo_inicia(void)
{
	u8 sem[32];
	int n;

	br_sha256_init(&pozo);
	hay_tsc = cpuid_tsc();
	ruido();
	n = lee_archivo("TLS.SEM", sem, sizeof sem);
	if (n > 0)
		br_sha256_update(&pozo, sem, n);
	ruido();
	br_sha256_update(&pozo, "semilla", 7);
	br_sha256_out(&pozo, sem);
	escribe_archivo("TLS.SEM", sem, sizeof sem);
	br_sha256_update(&pozo, "uso", 3);
	if (n <= 0)
		apunta("sin semilla de antes (TLS.SEM): el azar, del reloj y del contador de ciclos");
}

/* BearSSL pregunta por el generador de azar del sistema: no hay (se le da
 * con br_ssl_engine_inject_entropy) */
br_prng_seeder br_prng_seeder_system(const char **nombre)
{
	if (nombre)
		*nombre = "ninguno";
	return 0;
}

/* ---- las autoridades raíz: las de anclas.h y los *.CER ---- */
#define N_MAS 8
static br_x509_trust_anchor anclas[TAs_NUM + N_MAS];
static int n_anclas;
static u8 mas_dn[N_MAS][300], mas_clave[N_MAS][600];
static u32 dn_n;

static void dn_trozo(void *ctx, const void *d, size_t n)
{
	u8 *b = ctx;

	if (dn_n + n <= 300)
		memcpy(b + dn_n, d, n);
	dn_n += n;
}

static void ancla_de_archivo(const char *r)
{
	static u8 der[4096];
	br_x509_decoder_context dc;
	br_x509_pkey *pk;
	br_x509_trust_anchor *ta;
	u32 m, n;
	int k = n_anclas - TAs_NUM;

	if (k >= N_MAS)
		return;
	m = sc(SC_ABRIR, (u32)r, 0, 0);
	if (ES_ERROR(m))
		return;
	n = sc(SC_LEER, m, (u32)der, sizeof der);
	sc(SC_CERRAR, m, 0, 0);
	if (ES_ERROR(n) || n < 64 || n == sizeof der)
		return;
	dn_n = 0;
	br_x509_decoder_init(&dc, dn_trozo, mas_dn[k]);
	br_x509_decoder_push(&dc, der, n);
	pk = br_x509_decoder_get_pkey(&dc);
	if (!pk || dn_n > 300 || !br_x509_decoder_isCA(&dc)) {
		apunta("%s no es un certificado de autoridad (X.509 en DER): no se usa", r);
		return;
	}
	ta = &anclas[n_anclas];
	ta->dn.data = mas_dn[k];
	ta->dn.len = dn_n;
	ta->flags = BR_X509_TA_CA;
	ta->pkey = *pk;
	if (pk->key_type == BR_KEYTYPE_RSA) {
		if (pk->key.rsa.nlen + pk->key.rsa.elen > 600)
			return;
		memcpy(mas_clave[k], pk->key.rsa.n, pk->key.rsa.nlen);
		memcpy(mas_clave[k] + pk->key.rsa.nlen, pk->key.rsa.e, pk->key.rsa.elen);
		ta->pkey.key.rsa.n = mas_clave[k];
		ta->pkey.key.rsa.e = mas_clave[k] + pk->key.rsa.nlen;
	} else if (pk->key_type == BR_KEYTYPE_EC) {
		if (pk->key.ec.qlen > 600)
			return;
		memcpy(mas_clave[k], pk->key.ec.q, pk->key.ec.qlen);
		ta->pkey.key.ec.q = mas_clave[k];
	} else
		return;
	n_anclas++;
	apunta("autoridad de más: %s", r);
}

static void anclas_de(const char *dir)
{
	struct entrada e;
	char r[64];
	u32 m;

	m = sc(SC_ABRIR, (u32)dir, 0, 0);
	if (ES_ERROR(m))
		return;
	while (sc(SC_ENTRADA, m, (u32)&e, 0) == 1) {
		u32 l = strlen(e.nombre);
		if (l > 4 && !strcmp(e.nombre + l - 4, ".CER") && !(e.atributos & 0x10)) {
			formatear(r, sizeof r, "%s\\%s", dir, e.nombre);
			ancla_de_archivo(r);
		}
	}
	sc(SC_CERRAR, m, 0, 0);
}

static void anclas_inicia(void)
{
	memcpy(anclas, TAs, sizeof TAs);
	n_anclas = TAs_NUM;
	anclas_de("\\HISPA\\RAICES");
	anclas_de("\\RAICES");
}

/* ---- LA HORA ---- */
static int fuente_hora;		/* 0 ninguna, 1 el reloj del PC, 2 la cabecera Date */
static u32 h_dias, h_seg, h_tic;	/* (la de Date: días desde el año 0, segundos del día y el tic en que llegó) */
static char servidor_hora[64] = "actualiza.hispados.es";
static u32 puerto_hora = 80;
static u32 reloj_fecha;		/* lo último que dijo el reloj del PC (como en la FAT) */

static int bisiesto(u32 a) { return (a % 4 == 0 && a % 100 != 0) || a % 400 == 0; }

/* días desde el 1 de enero del año 0 (lo que quiere BearSSL) */
static u32 dias_de(u32 a, u32 m, u32 d)
{
	u32 k, r = 365u * a + (a + 3) / 4 - (a + 99) / 100 + (a + 399) / 400;

	for (k = 1; k < m; k++)
		r += k == 2 ? 28 + bisiesto(a) : (k == 4 || k == 6 || k == 9 || k == 11) ? 30 : 31;
	return r + d - 1;
}

/* y al revés: el año, el mes y el día */
static void fecha_de(u32 dias, u32 *a, u32 *m, u32 *d)
{
	u32 y = dias / 366, k;

	while (dias_de(y + 1, 1, 1) <= dias)
		y++;
	dias -= dias_de(y, 1, 1);
	for (k = 1; k < 12; k++) {
		u32 l = k == 2 ? 28 + bisiesto(y) : (k == 4 || k == 6 || k == 9 || k == 11) ? 30 : 31;
		if (dias < l)
			break;
		dias -= l;
	}
	*a = y, *m = k, *d = dias + 1;
}

static int cifras(const char *p, int n)
{
	int v = 0;

	while (n--) {
		if (*p < '0' || *p > '9')
			return -1;
		v = v * 10 + (*p++ - '0');
	}
	return v;
}

/* «Date: Fri, 20 Nov 2026 17:57:29 GMT» (RFC 7231, IMF-fixdate) */
static int lee_date(const char *p, u32 *dias, u32 *seg)
{
	static const char meses[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
	int d, a, h, mi, s, m;

	while (*p == ' ')
		p++;
	if (p[3] != ',' || p[4] != ' ')
		return 0;
	p += 5;
	d = cifras(p, 2);
	for (m = 0; m < 12 && memcmp(p + 3, meses + 3 * m, 3); m++) ;
	a = cifras(p + 7, 4);
	h = cifras(p + 12, 2);
	mi = cifras(p + 15, 2);
	s = cifras(p + 18, 2);
	if (d < 1 || d > 31 || m == 12 || a < 2000 || h < 0 || h > 23 || mi < 0 || mi > 59 || s < 0 || s > 60)
		return 0;
	*dias = dias_de(a, m + 1, d);
	*seg = h * 3600 + mi * 60 + s;
	return 1;
}

static u32 red_pide(u32 orden, u32 a, u32 b, u32 c, const void *d, u32 largo, void *dest, u32 cabe, u32 *cab_r);

/* «a.b.c.d» -> la IP (como va en la red: el primer octeto, abajo); 0 si no lo es */
static u32 ip_de_texto(const char *t)
{
	u32 ip = 0, k, v, n;

	for (k = 0; k < 4; k++) {
		for (v = 0, n = 0; *t >= '0' && *t <= '9' && n < 4; t++, n++)
			v = v * 10 + (*t - '0');
		if (!n || v > 255 || (k < 3 && *t++ != '.'))
			return 0;
		ip |= v << (8 * k);
	}
	return *t ? 0 : ip;
}

/* de un nombre, su IP (RED.BIN pregunta al DNS): 0 o el error */
static u32 resuelve(const char *nombre, u32 *ip)
{
	u32 cab[8], r;

	*ip = ip_de_texto(nombre);
	if (*ip)
		return 0;
	r = red_pide(O_RESOLVER, 0, 0, 0, nombre, strlen(nombre), 0, 0, cab);
	if (!ES_ERROR(r))
		*ip = cab[1];
	return r;
}

static int contiene(const char *t, const char *b)
{
	u32 n = strlen(b);

	for (; *t; t++)
		if (!memcmp(t, b, n))
			return 1;
	return 0;
}

/* (seguridad) el último día en que una cadena de certificados se comprobó
 * entera y valía con la fecha del reloj del PC (TLS.HOR): de ahí para atrás
 * no puede ser hoy. Solo sube con una cadena buena, así que no pasa de lo que
 * dura un certificado de verdad */
static u32 dia_visto;
static int dia_leido;

static u32 dia_sabido(void)
{
	if (!dia_leido) {
		u32 d = 0;

		dia_leido = 1;
		if (lee_archivo("TLS.HOR", &d, sizeof d) == sizeof d && d < dias_de(COMPILADO / 10000 + 30, 1, 1))
			dia_visto = d;
	}
	return dia_visto;
}

static void dia_bueno(u32 dias)
{
	if (dias > dia_sabido()) {
		dia_visto = dias;
		escribe_archivo("TLS.HOR", &dia_visto, sizeof dia_visto);
	}
}

static int hora_http(void)
{
	char pet[160], *p;
	u32 ip, s, n = 0, r, dias, seg;
	int k;

	r = resuelve(servidor_hora, &ip);
	/* (la red se está poniendo en marcha) */
	for (k = 0; k < 10 && (r == SIN_IP || r == SIN_CABLE); k++) {
		sc(SC_DORMIR, hz, 0, 0);
		r = resuelve(servidor_hora, &ip);
	}
	if (ES_ERROR(r)) {
		apunta("la hora: no se encuentra %s (0x%x)", servidor_hora, r);
		return 0;
	}
	s = red_pide(O_CONECTAR, ip, puerto_hora, 5 * hz, 0, 0, 0, 0, 0);
	for (k = 0; k < 10 && (s == SIN_IP || s == SIN_CABLE); k++) {
		sc(SC_DORMIR, hz, 0, 0);
		s = red_pide(O_CONECTAR, ip, puerto_hora, 5 * hz, 0, 0, 0, 0, 0);
	}
	if (ES_ERROR(s)) {
		apunta("la hora: no se conecta con %s:%u (0x%x)", servidor_hora, puerto_hora, s);
		return 0;
	}
	r = formatear(pet, sizeof pet, "HEAD / HTTP/1.1\r\nHost: %s\r\nUser-Agent: Hispa-Pentium-TLS\r\nConnection: close\r\n\r\n", servidor_hora);
	red_pide(O_ENVIAR, s, 0, 0, pet, r, 0, 0, 0);
	while (n < sizeof trozo - 1) {
		r = red_pide(O_RECIBIR, s, 5 * hz, 0, 0, 0, trozo + n, sizeof trozo - 1 - n, 0);
		if (r == 0 || ES_ERROR(r))
			break;
		n += r;
		trozo[n] = 0;
		if (contiene((char *)trozo, "\r\n\r\n"))
			break;
	}
	red_pide(O_CERRAR, s, 0, 0, 0, 0, 0, 0, 0);
	trozo[n] = 0;
	for (p = (char *)trozo; *p; p++)
		if ((p[0] == '\n') && (p[1] == 'D' || p[1] == 'd') && (p[2] == 'a' || p[2] == 'A') && (p[3] == 't' || p[3] == 'T')
		    && (p[4] == 'e' || p[4] == 'E') && p[5] == ':' && lee_date(p + 6, &dias, &seg)) {
			/* (seguridad) esa respuesta va sin cifrar: no se cree una fecha de
			 * antes del día en que se hizo este TLS.BIN, ni de antes del último
			 * día en que se comprobó entera una cadena de certificados
			 * (TLS.HOR): quien la cambie no puede llevar el reloj atrás para
			 * hacer pasar un certificado caducado hace tiempo */
			u32 suelo = dias_de(COMPILADO / 10000, COMPILADO / 100 % 100, COMPILADO % 100);

			if (suelo < dia_sabido())
				suelo = dia_sabido();
			if (dias < suelo) {
				apunta("la hora: %s da una fecha de antes de lo que se sabe (¿alguien en medio?): no se cree", servidor_hora);
				return 0;
			}
			h_dias = dias, h_seg = seg, h_tic = tics();
			return 1;
		}
	apunta("la hora: %s no ha dado la cabecera Date", servidor_hora);
	return 0;
}

/* la hora de ahora (días desde el año 0 y segundos del día): 0 si no se sabe */
static int hora(u32 *dias, u32 *seg)
{
	u32 f, a, m, d, hfat, ahora;

	f = sc(SC_DATO, DATO_FECHA, 0, 0);
	hfat = f & 0xFFFF;
	f >>= 16;
	a = 1980 + (f >> 9), m = (f >> 5) & 15, d = f & 31;
	reloj_fecha = f;
	if (fuente_hora != 2 && a * 10000 + m * 100 + d >= COMPILADO && a <= COMPILADO / 10000 + 30) {
		if (fuente_hora != 1)
			apunta("la hora, del reloj del PC: %02u/%02u/%u", d, m, a);
		fuente_hora = 1;
		*dias = dias_de(a, m, d);
		*seg = (hfat >> 11) * 3600 + ((hfat >> 5) & 63) * 60 + (hfat & 31) * 2;
		return 1;
	}
	if (fuente_hora != 2) {
		apunta("el reloj del PC dice %02u/%02u/%u: no puede ser (este TLS.BIN es del %02u/%02u/%u); se pregunta la hora a %s (sin cifrar: RIESGO, quien cambie esa respuesta puede hacer pasar por bueno un certificado caducado)",
		       d, m, a, COMPILADO % 100, COMPILADO / 100 % 100, COMPILADO / 10000, servidor_hora);
		if (!hora_http())
			return 0;
		fuente_hora = 2;
		fecha_de(h_dias, &a, &m, &d);
		apunta("la hora, de la cabecera Date de %s: %02u/%02u/%u %02u:%02u:%02u (UTC)", servidor_hora, d, m, a, h_seg / 3600,
		       h_seg / 60 % 60, h_seg % 60);
	}
	ahora = h_seg + (tics() - h_tic) / hz;
	*dias = h_dias + ahora / 86400;
	*seg = ahora % 86400;
	return 1;
}

/* ---- lo que se le pide a RED.BIN (la llamada 90, como redes.cas) ---- */
static u32 red_pide(u32 orden, u32 a, u32 b, u32 c, const void *d, u32 largo, void *dest, u32 cabe, u32 *cab_r)
{
	u32 cab[8], r;

	cab[0] = orden, cab[1] = a, cab[2] = b, cab[3] = c;
	cab[4] = cab[5] = cab[6] = 0;
	cab[7] = largo;
	/* (los octetos que van y los que vuelven, en el mismo sitio) */
	if (largo && dest && dest != d)
		memcpy(dest, d, largo);
	r = sc(SC_RED, (u32)cab, (u32)(dest ? dest : (void *)d), cabe);
	ruido();
	if (cab_r)
		memcpy(cab_r, cab, sizeof cab);
	return r;
}

/* ---- la memoria de certificados (como HSPTLS) ---- */
#define MEM_N 16
#define MEM_DIAS 7
static struct { u8 h[32]; u32 dia; } memoria[MEM_N];
static int mem_leida, mem_sig;
#define CONEX_DE(c) ((struct conex *)((char *)(c) - offsetof(struct conex, xv)))

static void mem_resumen(struct conex *t, u8 *h)
{
	br_sha256_context c;

	br_sha256_init(&c);
	br_sha256_update(&c, t->servidor, strlen(t->servidor) + 1);
	br_sha256_update(&c, t->cad_len, t->n_cert * sizeof t->cad_len[0]);
	br_sha256_update(&c, t->cadena, t->cad_n);
	br_sha256_out(&c, h);
}

static void mem_vuelca(struct conex *t)
{
	const br_x509_class **x = &t->xc.vtable;
	u32 k, o = 0;

	(*x)->start_chain(x, t->servidor);
	for (k = 0; k < t->n_cert; k++) {
		(*x)->start_cert(x, t->cad_len[k]);
		(*x)->append(x, t->cadena + o, t->cad_len[k]);
		(*x)->end_cert(x);
		o += t->cad_len[k];
	}
}

static void xm_start_chain(const br_x509_class **c, const char *servidor)
{
	struct conex *t = CONEX_DE(c);

	(void)servidor;
	t->cad_n = 0, t->n_cert = 0, t->directo = 0, t->de_memoria = 0;
}

static void xm_start_cert(const br_x509_class **c, uint32_t n)
{
	struct conex *t = CONEX_DE(c);

	if (!t->directo && (t->n_cert == CAD_N || t->cad_n + n > CAD_MAX)) {
		mem_vuelca(t);
		t->directo = 1;
	}
	if (t->directo)
		t->xc.vtable->start_cert(&t->xc.vtable, n);
	else
		t->cad_len[t->n_cert] = n;
}

static void xm_append(const br_x509_class **c, const unsigned char *b, size_t n)
{
	struct conex *t = CONEX_DE(c);

	if (t->directo)
		t->xc.vtable->append(&t->xc.vtable, b, n);
	else if (t->cad_n + n <= CAD_MAX) {
		memcpy(t->cadena + t->cad_n, b, n);
		t->cad_n += n;
	}
}

static void xm_end_cert(const br_x509_class **c)
{
	struct conex *t = CONEX_DE(c);

	if (t->directo)
		t->xc.vtable->end_cert(&t->xc.vtable);
	else
		t->n_cert++;
}

static unsigned xm_end_chain(const br_x509_class **c)
{
	struct conex *t = CONEX_DE(c);
	u8 h[32];
	unsigned r;
	int k;

	if (t->directo)
		return t->xc.vtable->end_chain(&t->xc.vtable);
	if (!t->n_cert)
		return BR_ERR_X509_EMPTY_CHAIN;
	if (!mem_leida) {
		mem_leida = 1;
		if (lee_archivo("TLS.CCH", memoria, sizeof memoria) != sizeof memoria)
			memset(memoria, 0, sizeof memoria);
	}
	mem_resumen(t, h);
	for (k = 0; k < MEM_N; k++)
		if (memoria[k].dia && !memcmp(memoria[k].h, h, 32) && t->dias >= memoria[k].dia && t->dias < memoria[k].dia + MEM_DIAS) {
			br_x509_decoder_init(&t->dc, 0, 0);
			br_x509_decoder_push(&t->dc, t->cadena, t->cad_len[0]);
			if (br_x509_decoder_get_pkey(&t->dc)) {
				t->de_memoria = 1;
				return 0;
			}
		}
	mem_vuelca(t);
	r = t->xc.vtable->end_chain(&t->xc.vtable);
	if (r == 0) {
		for (k = 0; k < MEM_N && memcmp(memoria[k].h, h, 32); k++) ;
		if (k == MEM_N)
			k = mem_sig = (mem_sig + 1) % MEM_N;
		memcpy(memoria[k].h, h, 32);
		memoria[k].dia = t->dias;
		/* (solo con la fecha del reloj del PC: con la de Date, quien esté en
		 * medio podría adelantarla y dejar el equipo sin HTTPS unos meses) */
		if (fuente_hora == 1)
			dia_bueno(t->dias);
		escribe_archivo("TLS.CCH", memoria, sizeof memoria);
	}
	return r;
}

static const br_x509_pkey *xm_get_pkey(const br_x509_class *const *c, unsigned *usos)
{
	struct conex *t = CONEX_DE(c);

	if (t->de_memoria) {
		if (usos)
			*usos = BR_KEYTYPE_KEYX | BR_KEYTYPE_SIGN;
		return br_x509_decoder_get_pkey(&t->dc);
	}
	return t->xc.vtable->get_pkey(&t->xc.vtable, usos);
}

static const br_x509_class xm_vtable = {
	sizeof(struct conex), xm_start_chain, xm_start_cert, xm_append, xm_end_cert, xm_end_chain, xm_get_pkey
};

/* ---- las sesiones, para reanudarlas ---- */
static struct { char servidor[64]; br_ssl_session_parameters sp; } sesiones[4];
static int ses_sig;

/* ---- los textos de los errores (en la página 437, como los de Castel) ---- */
static void texto_error(struct conex *c, u32 codigo, char *d, int cabe)
{
	int e = c ? c->error : 0;
	u32 a, m, dd;

	if (codigo == TLS_SIN_HORA) {
		formatear(d, cabe, "el reloj del equipo no está en hora y no se ha podido preguntar la hora a %s: sin la fecha no se pueden comprobar los certificados",
			  servidor_hora);
		return;
	}
	switch (e) {
	case BR_ERR_X509_EXPIRED:
		fecha_de(c->dias, &a, &m, &dd);
		formatear(d, cabe, "el certificado de %s ha caducado o aún no vale (con la fecha %02u/%02u/%u%s); compruebe la fecha del equipo",
			  c->servidor, dd, m, a, fuente_hora == 2 ? ", de la cabecera Date" : ", del reloj del PC");
		return;
	case BR_ERR_X509_NOT_TRUSTED:
		formatear(d, cabe, "el certificado de %s no lo firma ninguna autoridad conocida", c->servidor);
		return;
	case BR_ERR_X509_BAD_SERVER_NAME:
		formatear(d, cabe, "el certificado es de otro sitio, no de %s", c->servidor);
		return;
	case BR_ERR_X509_BAD_SIGNATURE:
		formatear(d, cabe, "la firma del certificado de %s no es buena", c->servidor);
		return;
	case BR_ERR_X509_WEAK_PUBLIC_KEY:
	case BR_ERR_X509_UNSUPPORTED:
	case BR_ERR_X509_WRONG_KEY_TYPE:
		formatear(d, cabe, "el certificado de %s usa una clave que aquí no se admite", c->servidor);
		return;
	case BR_ERR_BAD_VERSION:
	case BR_ERR_UNSUPPORTED_VERSION:
		formatear(d, cabe, "%s no admite TLS 1.2", c->servidor);
		return;
	case BR_ERR_BAD_CIPHER_SUITE:
	case BR_ERR_RECV_FATAL_ALERT + 40:
		formatear(d, cabe, "%s no admite ninguno de los cifrados que hay aquí", c->servidor);
		return;
	case BR_ERR_BAD_MAC:
		formatear(d, cabe, "han llegado datos cifrados que no cuadran");
		return;
	}
	if (codigo == TIEMPO)
		formatear(d, cabe, "%s no ha contestado a tiempo", c ? c->servidor : "el servidor");
	else if (c && (c->fin_tcp || e == 0))
		formatear(d, cabe, "%s ha cortado la conexión en el apretón de manos", c->servidor);
	else if (e >= BR_ERR_X509_OK && e < 64)
		formatear(d, cabe, "el certificado de %s no es válido (error %d de BearSSL)", c->servidor, e);
	else if (e >= BR_ERR_RECV_FATAL_ALERT)
		formatear(d, cabe, "%s ha rechazado la conexión segura (alerta %d)", c->servidor, e - BR_ERR_RECV_FATAL_ALERT);
	else
		formatear(d, cabe, "ha fallado la conexión segura con %s (error %d de BearSSL)", c ? c->servidor : "el servidor", e);
}

static u32 codigo_error(struct conex *c)
{
	if (c->error >= BR_ERR_X509_OK && c->error < 64)
		return TLS_CERTIFICADO;
	if (c->error == 0 && c->fin_tcp && c->error_red)
		return c->error_red;
	return TLS_FALLO;
}

/* «TLS 1.2, ECDHE-ECDSA, ChaCha20-Poly1305» */
static const char *cifrado(struct conex *c, char *r, int cabe)
{
	br_ssl_session_parameters sp;
	const char *f, *x;

	br_ssl_engine_get_session_parameters(&c->cc.eng, &sp);
	switch (sp.cipher_suite) {
	case BR_TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256: f = "ECDHE-ECDSA"; x = "ChaCha20-Poly1305"; break;
	case BR_TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256: f = "ECDHE-RSA"; x = "ChaCha20-Poly1305"; break;
	case BR_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256: f = "ECDHE-ECDSA"; x = "AES-128-GCM"; break;
	case BR_TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256: f = "ECDHE-RSA"; x = "AES-128-GCM"; break;
	case BR_TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384: f = "ECDHE-ECDSA"; x = "AES-256-GCM"; break;
	default: f = "ECDHE-RSA"; x = "AES-256-GCM"; break;
	}
	formatear(r, cabe, "TLS 1.2, %s, %s", f, x);
	return r;
}

/* ---- contestar a un programa ---- */
static void contesta(u32 hueco, u32 serie, u32 r, u32 uno, u32 dos, u32 tres, const void *d, u32 n)
{
	struct respuesta x;

	if (hueco == 0xFFFFFFFFu)
		return;
	memset(&x, 0, sizeof x);
	x.serie = serie;
	x.datos = (u32)d;
	x.cab[0] = r, x.cab[1] = uno, x.cab[2] = dos, x.cab[3] = tres;
	x.cab[7] = n;
	sc(SC_CONTESTAR, hueco, (u32)&x, 0);
}

static void contesta_pendiente(struct conex *c, u32 r, const void *d, u32 n)
{
	if (c->p_clase == P_NADA)
		return;
	c->p_clase = P_NADA;
	contesta(c->p_hueco, c->p_serie, r, 0, 0, 0, d, n);
}

/* ---- las conexiones ---- */
static void suelta(struct conex *c)
{
	if (c->tcp)
		red_pide(O_CERRAR, c->tcp, 0, 0, 0, 0, 0, 0, 0);
	c->tcp = 0;
	c->usada = 0;
}

static struct conex *de(u32 num, u32 dueno)
{
	u32 k = num - PRIMERA_TLS;

	if (k >= MAXC || !conex[k] || !conex[k]->usada || conex[k]->dueno != dueno)
		return 0;
	return conex[k];
}

/* pasa registros entre BearSSL y RED.BIN mientras se pueda; si espera_rx
 * no es negativo, también lee de la red (esperando eso como mucho). Cuenta
 * los ciclos que se van en BearSSL (todo menos la red). 1 si ha movido algo */
static int bombea(struct conex *c, int espera_rx)
{
	br_ssl_engine_context *e = &c->cc.eng;
	int algo = 0;
	u32 c0 = tsc(), r;

	for (;;) {
		unsigned st = br_ssl_engine_current_state(e);
		size_t n;
		u8 *b;

		if (st & BR_SSL_CLOSED) {
			if (!c->error)
				c->error = br_ssl_engine_last_error(e);
			break;
		}
		if (st & BR_SSL_SENDREC) {
			b = br_ssl_engine_sendrec_buf(e, &n);
			if (n > MAX_DATOS)
				n = MAX_DATOS;
			c->ciclos += tsc() - c0;
			r = red_pide(O_ENVIAR, c->tcp, 0, 0, b, n, 0, 0, 0);
			c0 = tsc();
			if (ES_ERROR(r)) {
				c->fin_tcp = 1;
				c->error_red = r;
				break;
			}
			if (r > 0) {
				br_ssl_engine_sendrec_ack(e, r);
				algo = 1;
			}
			continue;	/* (0: no había sitio; RED.BIN ha esperado a que lo haya) */
		}
		if ((st & BR_SSL_RECVREC) && !c->fin_tcp && espera_rx >= 0) {
			b = br_ssl_engine_recvrec_buf(e, &n);
			if (n > MAX_DATOS)
				n = MAX_DATOS;
			c->ciclos += tsc() - c0;
			r = red_pide(O_RECIBIR, c->tcp, espera_rx, 0, 0, 0, b, n, 0);
			c0 = tsc();
			if (r == TIEMPO)
				break;
			if (r == 0 || ES_ERROR(r)) {
				c->fin_tcp = 1;
				if (r)
					c->error_red = r;
				break;
			}
			br_ssl_engine_recvrec_ack(e, r);
			algo = 1;
			if (br_ssl_engine_current_state(e) & BR_SSL_RECVAPP)
				break;
			continue;
		}
		break;
	}
	c->ciclos += tsc() - c0;
	return algo;
}

/* lo que espera el programa de esta conexión, si ya se puede contestar */
static void mira(struct conex *c)
{
	unsigned st = br_ssl_engine_current_state(&c->cc.eng);
	char t[200];

	if (c->p_clase == P_CONECTAR) {
		if (st & (BR_SSL_SENDAPP | BR_SSL_RECVAPP)) {
			int k;
			c->hecho = 1;
			c->ms = (tics() - c->t_ini) * 1000 / hz;
			c->ciclos_hs = c->ciclos;
			n_apretones++;
			for (k = 0; k < 4 && strcmp(sesiones[k].servidor, c->servidor); k++) ;
			if (k == 4)
				k = ses_sig = (ses_sig + 1) & 3;
			memcpy(sesiones[k].servidor, c->servidor, sizeof c->servidor);
			br_ssl_engine_get_session_parameters(&c->cc.eng, &sesiones[k].sp);
			apunta("%s: %s; apretón de manos en %u ms (BearSSL: %u,%u M ciclos)%s", c->servidor, cifrado(c, t, sizeof t), c->ms,
			       (u32)(c->ciclos >> 10) / 9766, (u32)(c->ciclos >> 10) / 977 % 10, c->de_memoria ? ", la cadena ya comprobada (de memoria)"
			       : c->n_cert == 0 ? ", sesión reanudada" : ", la cadena comprobada");
			contesta_pendiente(c, c->num, 0, 0);
			return;
		}
		if ((st & BR_SSL_CLOSED) || c->fin_tcp || tics() - c->p_limite < 0x80000000u) {
			u32 cod = (st & BR_SSL_CLOSED) || c->fin_tcp ? codigo_error(c) : TIEMPO;
			texto_error(c, cod, t, sizeof t);
			apunta("%s: %s", c->servidor, t);
			contesta(c->p_hueco, c->p_serie, cod, (u32)c->error, 0, 0, t, c->p_cabe ? (strlen(t) + 1 < c->p_cabe ? strlen(t) + 1 : c->p_cabe) : 0);
			c->p_clase = P_NADA;
			suelta(c);
		}
		return;
	}
	if (c->p_clase == P_RECIBIR) {
		if (st & BR_SSL_RECVAPP) {
			size_t q;
			u8 *p = br_ssl_engine_recvapp_buf(&c->cc.eng, &q);
			if (q > c->p_cabe)
				q = c->p_cabe;
			contesta_pendiente(c, q, p, q);
			br_ssl_engine_recvapp_ack(&c->cc.eng, q);
			c->recibidos += q;
			return;
		}
		if (st & BR_SSL_CLOSED) {
			contesta_pendiente(c, (c->error == 0 || c->error == BR_ERR_OK) ? 0 : TLS_FALLO, 0, 0);
			return;
		}
		if (c->fin_tcp) {	/* cortó sin el aviso de cierre */
			contesta_pendiente(c, c->error_red ? c->error_red : 0, 0, 0);
			return;
		}
		if (tics() - c->p_limite < 0x80000000u)
			contesta_pendiente(c, TIEMPO, 0, 0);
	}
}


/* ---- (CORREO) STARTTLS: lo plano de antes del apretón de manos, para el
 * correo (IMAP en el 143, SMTP en el 587, POP3 en el 110). Lo hace el
 * servicio, no el programa: así el programa no ve nunca la conexión sin
 * cifrar y no puede mandar nada por ella (la contraseña, tampoco). Se espera
 * el saludo, se pide STARTTLS (con EHLO antes en SMTP) y, si el servidor dice
 * que sí, sigue el apretón de manos como siempre. Lo que llegue de más
 * detrás del «sí» (la inyección de órdenes de antes del cifrado, el fallo de
 * tantos clientes) hace que no se siga. Da 0 o el error, con su texto. */
#define ST_IMAP 1
#define ST_SMTP 2
#define ST_POP3 3
static int st_renglon(u32 tcp, char *b, u32 *n, u32 cabe, u32 limite, int *de_mas)
{
	/* lee hasta tener un renglón entero (acabado en \n) al principio de b */
	u32 k, r;

	for (;;) {
		for (k = 0; k < *n; k++)
			if (b[k] == '\n') {
				*de_mas = *n > k + 1;
				return (int)(k + 1);
			}
		if (*n + 1 >= cabe || (int)(limite - tics()) <= 0)
			return -1;
		r = red_pide(O_RECIBIR, tcp, hz / 2, 0, 0, 0, b + *n, cabe - 1 - *n, 0);
		if (r == TIEMPO)
			continue;
		if (ES_ERROR(r) || r == 0)
			return -1;
		*n += r;
	}
}

static void st_quita(char *b, u32 *n, int k)
{
	memmove(b, b + k, *n - (u32)k);
	*n -= (u32)k;
}

static int st_empieza(const char *b, const char *p)
{
	while (*p) {
		char x = *b >= 'a' && *b <= 'z' ? *b - 32 : *b;
		if (x != *p)
			return 0;
		b++, p++;
	}
	return 1;
}

static u32 starttls(u32 tcp, u32 modo, u32 espera, char *t, int cabe, u32 *sin)
{
	static char b[2048];
	u32 n = 0, limite = tics() + (espera > 30 * hz ? 30 * hz : espera < 5 * hz ? 5 * hz : espera);
	int k, de_mas = 0, hay = 0;
	const char *orden;

	*sin = 0;
	/* el saludo (en SMTP, de varios renglones: «220-...») */
	for (;;) {
		k = st_renglon(tcp, b, &n, sizeof b, limite, &de_mas);
		if (k < 0)
			goto mudo;
		if (modo == ST_IMAP) {
			if (!st_empieza(b, "* OK")) {
				formatear(t, cabe, "STARTTLS: el servidor de correo no saluda como IMAP (o no deja entrar)");
				return TLS_FALLO;
			}
		} else if (modo == ST_POP3) {
			if (!st_empieza(b, "+OK")) {
				formatear(t, cabe, "STARTTLS: el servidor de correo no saluda como POP3");
				return TLS_FALLO;
			}
		} else if (!st_empieza(b, "220")) {
			formatear(t, cabe, "STARTTLS: el servidor de correo no saluda como SMTP");
			return TLS_FALLO;
		}
		hay = modo == ST_SMTP && k > 3 && b[3] == '-';
		st_quita(b, &n, k);
		if (!hay)
			break;
	}
	if (modo == ST_SMTP) {
		static const char ehlo[] = "EHLO hispa-pentium\r\n";
		red_pide(O_ENVIAR, tcp, 0, 0, ehlo, sizeof ehlo - 1, 0, 0, 0);
		hay = 0;
		for (;;) {
			k = st_renglon(tcp, b, &n, sizeof b, limite, &de_mas);
			if (k < 0)
				goto mudo;
			if (!st_empieza(b, "250")) {
				formatear(t, cabe, "STARTTLS: el servidor de correo no acepta EHLO");
				return TLS_FALLO;
			}
			if (k >= 12 && st_empieza(b + 4, "STARTTLS"))
				hay = 1;
			if (k > 3 && b[3] == '-') {
				st_quita(b, &n, k);
				continue;
			}
			st_quita(b, &n, k);
			break;
		}
		if (!hay) {
			*sin = 0x5354;
			formatear(t, cabe, "el servidor de correo no ofrece STARTTLS: la conexi\xc3\xb3n ir\xc3\xad" "a sin cifrar");
			return TLS_FALLO;
		}
	}
	orden = modo == ST_IMAP ? "h1 STARTTLS\r\n" : modo == ST_POP3 ? "STLS\r\n" : "STARTTLS\r\n";
	if (n) {
		formatear(t, cabe, "STARTTLS: el servidor manda de m\xc3\xa1s antes de tiempo");
		return TLS_FALLO;
	}
	red_pide(O_ENVIAR, tcp, 0, 0, orden, strlen(orden), 0, 0, 0);
	for (;;) {
		k = st_renglon(tcp, b, &n, sizeof b, limite, &de_mas);
		if (k < 0)
			goto mudo;
		if (modo == ST_IMAP && b[0] == '*') {
			st_quita(b, &n, k);	/* (lo que diga antes: CAPABILITY...) */
			continue;
		}
		break;
	}
	if ((modo == ST_IMAP && !st_empieza(b, "H1 OK")) || (modo == ST_POP3 && !st_empieza(b, "+OK")) ||
	    (modo == ST_SMTP && !st_empieza(b, "220"))) {
		*sin = 0x5354;
		formatear(t, cabe, "el servidor de correo no ofrece STARTTLS: la conexi\xc3\xb3n ir\xc3\xad" "a sin cifrar");
		return TLS_FALLO;
	}
	if (de_mas) {
		formatear(t, cabe, "STARTTLS: el servidor manda de m\xc3\xa1s detr\xc3\xa1s del s\xc3\xad (\xc2\xbf" "alguien en medio?): no se sigue");
		return TLS_FALLO;
	}
	return 0;
mudo:
	formatear(t, cabe, "STARTTLS: el servidor de correo no contesta");
	return TIEMPO;
}

static void conectar(struct pedido *p, const u8 *datos)
{
	static const uint16_t suites[] = {
		BR_TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256,
		BR_TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256,
		BR_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
		BR_TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
		BR_TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
		BR_TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384
	};
	struct conex *c = 0;
	char nombre[64], t[200];
	u32 n = p->largo, ip = p->cab[1], puerto = p->cab[2], espera = p->cab[3], r, dias, seg, k;
	u8 az[32];
	int reanuda = 0;

	if (n == 0 || n > 63) {
		contesta(p->hueco, p->serie, NO_VALE, 0, 0, 0, 0, 0);
		return;
	}
	memcpy(nombre, datos, n);
	nombre[n] = 0;
	if (!puerto)
		puerto = 443;
	if (!espera)
		espera = 10 * hz;
	if (!hora(&dias, &seg)) {
		texto_error(0, TLS_SIN_HORA, t, sizeof t);
		contesta(p->hueco, p->serie, TLS_SIN_HORA, 0, 0, 0, t, strlen(t) + 1 < p->cabe ? strlen(t) + 1 : p->cabe);
		return;
	}
	if (!ip) {
		r = resuelve(nombre, &ip);
		if (ES_ERROR(r)) {
			contesta(p->hueco, p->serie, r, 0, 0, 0, 0, 0);
			return;
		}
	}
	for (k = 0; k < MAXC; k++)
		if (!conex[k] || !conex[k]->usada)
			break;
	if (k == MAXC) {
		contesta(p->hueco, p->serie, NO_CONEXION, 0, 0, 0, 0, 0);
		return;
	}
	if (!conex[k]) {
		conex[k] = mas_memoria(sizeof(struct conex));
		if (!conex[k]) {
			contesta(p->hueco, p->serie, SIN_MEMORIA, 0, 0, 0, 0, 0);
			return;
		}
	}
	c = conex[k];
	r = red_pide(O_CONECTAR, ip, puerto, espera, 0, 0, 0, 0, 0);
	if (ES_ERROR(r)) {
		contesta(p->hueco, p->serie, r, 0, 0, 0, 0, 0);
		return;
	}
	if (p->cab[4] >= ST_IMAP && p->cab[4] <= ST_POP3) {	/* (CORREO) STARTTLS */
		u32 sin, e = starttls(r, p->cab[4], espera, t, sizeof t, &sin);
		if (e) {
			red_pide(O_CERRAR, r, 0, 0, 0, 0, 0, 0, 0);
			apunta("%s: %s", nombre, t);
			contesta(p->hueco, p->serie, e, sin, 0, 0, t, strlen(t) + 1 < p->cabe ? strlen(t) + 1 : p->cabe);
			return;
		}
		apunta("%s: STARTTLS (%s): s\xc3\xad", nombre, p->cab[4] == ST_IMAP ? "IMAP" : p->cab[4] == ST_SMTP ? "SMTP" : "POP3");
	}
	memset(c, 0, offsetof(struct conex, cadena));
	c->usada = 1;
	c->tcp = r;
	c->num = PRIMERA_TLS + k;
	c->dueno = p->tarea;
	memcpy(c->servidor, nombre, n + 1);
	c->dias = dias;

	br_ssl_client_zero(&c->cc);
	br_ssl_engine_set_versions(&c->cc.eng, BR_TLS12, BR_TLS12);
	br_ssl_engine_set_suites(&c->cc.eng, suites, sizeof suites / sizeof suites[0]);
	br_ssl_engine_set_default_rsavrfy(&c->cc.eng);
	br_ssl_engine_set_default_ecdsa(&c->cc.eng);
	br_ssl_engine_set_ec(&c->cc.eng, &br_ec_all_m31);
	br_ssl_engine_set_hash(&c->cc.eng, br_sha256_ID, &br_sha256_vtable);
	br_ssl_engine_set_hash(&c->cc.eng, br_sha384_ID, &br_sha384_vtable);
	br_ssl_engine_set_prf_sha256(&c->cc.eng, &br_tls12_sha256_prf);
	br_ssl_engine_set_prf_sha384(&c->cc.eng, &br_tls12_sha384_prf);
	br_ssl_engine_set_chacha20(&c->cc.eng, &br_chacha20_ct_run);
	br_ssl_engine_set_poly1305(&c->cc.eng, &br_poly1305_ctmul32_run);
	br_ssl_engine_set_chapol(&c->cc.eng, &br_sslrec_in_chapol_vtable, &br_sslrec_out_chapol_vtable);
	br_ssl_engine_set_aes_ctr(&c->cc.eng, &br_aes_big_ctr_vtable);
	br_ssl_engine_set_ghash(&c->cc.eng, &br_ghash_ctmul32);
	br_ssl_engine_set_gcm(&c->cc.eng, &br_sslrec_in_gcm_vtable, &br_sslrec_out_gcm_vtable);

	br_x509_minimal_init(&c->xc, &br_sha256_vtable, anclas, n_anclas);
	br_x509_minimal_set_rsa(&c->xc, &br_rsa_i31_pkcs1_vrfy);
	br_x509_minimal_set_ecdsa(&c->xc, &br_ec_all_m31, &br_ecdsa_i31_vrfy_asn1);
	br_x509_minimal_set_hash(&c->xc, br_sha256_ID, &br_sha256_vtable);
	br_x509_minimal_set_hash(&c->xc, br_sha384_ID, &br_sha384_vtable);
	br_x509_minimal_set_hash(&c->xc, br_sha512_ID, &br_sha512_vtable);
	br_x509_minimal_set_time(&c->xc, dias, seg);
	c->xv = &xm_vtable;
	br_ssl_engine_set_x509(&c->cc.eng, &c->xv);
	br_ssl_engine_set_buffers_bidi(&c->cc.eng, c->entrada, sizeof c->entrada, c->salida, sizeof c->salida);
	ruido();
	br_sha256_out(&pozo, az);
	br_sha256_update(&pozo, az, 8);
	br_ssl_engine_inject_entropy(&c->cc.eng, az, sizeof az);
	for (k = 0; k < 4; k++)
		if (!strcmp(sesiones[k].servidor, nombre)) {
			br_ssl_engine_set_session_parameters(&c->cc.eng, &sesiones[k].sp);
			reanuda = 1;
		}
	c->reanudada = reanuda;
	br_ssl_client_reset(&c->cc, c->servidor, reanuda);
	c->t_ini = tics();
	c->p_clase = P_CONECTAR;
	c->p_hueco = p->hueco;
	c->p_serie = p->serie;
	c->p_cabe = p->cabe;
	c->p_limite = tics() + (espera > 30 * hz ? espera : 30 * hz);
	bombea(c, 1);
	mira(c);
}

static void enviar(struct pedido *p, const u8 *datos)
{
	struct conex *c = de(p->cab[1], p->tarea);
	br_ssl_engine_context *e;
	u32 hecho = 0, n = p->largo;

	if (!c || !c->hecho) {
		contesta(p->hueco, p->serie, NO_CONEXION, 0, 0, 0, 0, 0);
		return;
	}
	e = &c->cc.eng;
	while (hecho < n) {
		unsigned st;
		size_t q;
		u8 *b;

		bombea(c, -1);
		st = br_ssl_engine_current_state(e);
		if ((st & BR_SSL_CLOSED) || c->fin_tcp) {
			contesta(p->hueco, p->serie, c->error_red ? c->error_red : CORTADA_RED, 0, 0, 0, 0, 0);
			return;
		}
		if (st & BR_SSL_SENDAPP) {
			b = br_ssl_engine_sendapp_buf(e, &q);
			if (q > n - hecho)
				q = n - hecho;
			memcpy(b, datos + hecho, q);
			br_ssl_engine_sendapp_ack(e, q);
			hecho += q;
		}
	}
	br_ssl_engine_flush(e, 0);
	bombea(c, -1);
	c->enviados += n;
	contesta(p->hueco, p->serie, n, 0, 0, 0, 0, 0);
}

static void cerrar(struct conex *c)
{
	if (c->hecho && !c->fin_tcp && !(br_ssl_engine_current_state(&c->cc.eng) & BR_SSL_CLOSED)) {
		br_ssl_engine_close(&c->cc.eng);	/* el aviso de cierre */
		bombea(c, -1);
	}
	contesta_pendiente(c, CORTADA_RED, 0, 0);
	/* lo que ha costado cifrar y descifrar los datos (sin el apretón de manos) */
	if (c->hecho)
		apunta("%s: cerrada; %u octetos recibidos y %u enviados, que han costado a BearSSL %u K ciclos", c->servidor, c->recibidos,
		       c->enviados, (u32)((c->ciclos - c->ciclos_hs) >> 10));
	suelta(c);
}

static void estado(struct pedido *p)
{
	static char t[1024];
	u32 dias, seg, a, m, d, k, n = 0, abiertas = 0;
	int bien = hora(&dias, &seg);

	for (k = 0; k < MAXC; k++)
		if (conex[k] && conex[k]->usada)
			abiertas++;
	n += formatear(t + n, sizeof t - n, "  Servicio  . . . . . . : TLS.BIN (BearSSL 0.6), TLS 1.2\r\n");
	if (bien) {
		fecha_de(dias, &a, &m, &d);
		n += formatear(t + n, sizeof t - n, "  Fecha . . . . . . . . : %02u/%02u/%u %02u:%02u (%s)\r\n", d, m, a, seg / 3600, seg / 60 % 60,
			       fuente_hora == 2 ? "de la cabecera Date: el reloj del PC no está en hora" : "del reloj del PC");
	} else
		n += formatear(t + n, sizeof t - n, "  Fecha . . . . . . . . : no se sabe (el reloj del PC no está en hora)\r\n");
	n += formatear(t + n, sizeof t - n, "  Autoridades raíz  . . : %u\r\n", n_anclas);
	n += formatear(t + n, sizeof t - n, "  Conexiones  . . . . . : %u abiertas, %u apretones de manos\r\n", abiertas, n_apretones);
	n += formatear(t + n, sizeof t - n, "  Memoria . . . . . . . : %u K de conexiones\r\n", memoria_usada / 1024);
	if (n > p->cabe)
		n = p->cabe;
	contesta(p->hueco, p->serie, 0, fuente_hora, n_anclas, abiertas, t, n);
}

static void atiende(struct pedido *p, const u8 *datos)
{
	struct conex *c;
	char t[80];
	u32 k, n;

	switch (p->cab[0]) {
	case AVISO_ACABO:
		for (k = 0; k < MAXC; k++)
			if (conex[k] && conex[k]->usada && conex[k]->dueno == p->tarea) {
				conex[k]->p_clase = P_NADA;
				suelta(conex[k]);
			}
		return;
	case O_CONECTAR:
		conectar(p, datos);
		return;
	case O_ENVIAR:
		enviar(p, datos);
		return;
	case O_RECIBIR:
		c = de(p->cab[1], p->tarea);
		if (!c || !c->hecho) {
			contesta(p->hueco, p->serie, NO_CONEXION, 0, 0, 0, 0, 0);
			return;
		}
		if (!p->cabe) {
			contesta(p->hueco, p->serie, NO_VALE, 0, 0, 0, 0, 0);
			return;
		}
		c->p_clase = P_RECIBIR;
		c->p_hueco = p->hueco;
		c->p_serie = p->serie;
		c->p_cabe = p->cabe;
		c->p_limite = tics() + p->cab[2];
		mira(c);
		return;
	case O_CERRAR:
		c = de(p->cab[1], p->tarea);
		if (!c) {
			contesta(p->hueco, p->serie, NO_CONEXION, 0, 0, 0, 0, 0);
			return;
		}
		cerrar(c);
		contesta(p->hueco, p->serie, 0, 0, 0, 0, 0, 0);
		return;
	case O_INFO:
		c = de(p->cab[1], p->tarea);
		if (!c || !c->hecho) {
			contesta(p->hueco, p->serie, NO_CONEXION, 0, 0, 0, 0, 0);
			return;
		}
		cifrado(c, t, sizeof t);
		n = strlen(t) + 1;
		contesta(p->hueco, p->serie, 0, c->ms, (c->de_memoria ? 2 : 0) | (c->n_cert == 0 ? 1 : 0), (u32)(c->ciclos >> 10), t,
			 n < p->cabe ? n : p->cabe);
		return;
	case O_ESTADO:
		estado(p);
		return;
	case O_REGISTRO:
		n = registro_largo < p->cabe ? registro_largo : p->cabe;
		contesta(p->hueco, p->serie, n, 0, 0, 0, registro + registro_largo - n, n);
		return;
	}
	contesta(p->hueco, p->serie, NO_VALE, 0, 0, 0, 0, 0);
}

/* TLS.INI: «hora=servidor[:puerto]» */
static void lee_ajustes(void)
{
	static char t[512];
	int n = lee_archivo("TLS.INI", t, sizeof t - 1), i, k;

	if (n <= 0)
		return;
	t[n] = 0;
	for (i = 0; i < n; i++) {
		if ((i == 0 || t[i - 1] == '\n') && !memcmp(t + i, "hora=", 5)) {
			for (k = 0, i += 5; k < 63 && t[i] > ' ' && t[i] != ':'; k++, i++)
				servidor_hora[k] = t[i];
			servidor_hora[k] = 0;
			if (t[i] == ':') {
				puerto_hora = 0;
				for (i++; t[i] >= '0' && t[i] <= '9'; i++)
					puerto_hora = puerto_hora * 10 + (t[i] - '0');
			}
			apunta("TLS.INI: la hora, de %s:%u si el reloj del PC no está en hora", servidor_hora, puerto_hora);
		}
	}
}

int servicio_tls(void)
{
	struct pedido *p = (struct pedido *)pedido;
	u32 m, k;

	sc(SC_CORTE, 1, 0, 0);
	if (sc(SC_APUNTARSE, 0, 0, 0) != 0)
		return 1;
	apunta("Hispa-Pentium 0.8: el servicio TLS (TLS.BIN, BearSSL 0.6, TLS 1.2)");
	hz = sc(SC_DATO, DATO_HZ, 0, 0);
	if (!hz || ES_ERROR(hz))
		hz = 100;
	m = sc(SC_ABRIR, (u32)"\\HISPA", 0, 0);
	if (!ES_ERROR(m)) {
		sc(SC_CERRAR, m, 0, 0);
		memcpy(carpeta, "\\HISPA\\", 8);
	}
	pozo_inicia();
	anclas_inicia();
	lee_ajustes();
	apunta("%u autoridades raíz", n_anclas);
	for (;;) {
		int algo = 0, espera_n = 0;
		u32 espera;

		for (k = 0; k < MAXC; k++) {
			struct conex *c = conex[k];
			int rx;

			if (!c || !c->usada)
				continue;
			rx = c->p_clase == P_CONECTAR || (c->p_clase == P_RECIBIR && !(br_ssl_engine_current_state(&c->cc.eng) & BR_SSL_RECVAPP)) ? 1 : -1;
			algo |= bombea(c, rx);
			mira(c);
			if (c->usada && c->p_clase != P_NADA)
				espera_n = 1;
		}
		espera = algo || espera_n ? 0 : 100;
		if (sc(SC_SIGUIENTE, (u32)p, espera, 0) == 1)
			atiende(p, pedido + 64);
	}
}
