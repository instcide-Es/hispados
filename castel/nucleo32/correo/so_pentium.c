/* so_pentium.c - Correo en Hispa-Pentium: lo de so.h y ui.h con las
 * llamadas del núcleo (int 0x80; docs/PENTIUM.md, sección 6), la red de
 * RED.BIN (llamada 90) y las conexiones cifradas de TLS.BIN (llamada 99,
 * con STARTTLS desde la rama correo: tls.c, «(CORREO)»). Las rutas y los
 * textos que van al núcleo pasan de UTF-8 a la 437 de Hispa (juegos.c).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include <stdio.h>
#include "so.h"
#include "ui.h"
#include "mime.h"
#include "pentium.h"

#define RUTA 260

static u32 hz_ = 0;

/* ---- lo de las rutas: de UTF-8 a la 437 ---- */
static void a437(const char *t, char *d, u32 cabe)
{
	utf8_a_437(t, (u32)strlen(t), d, cabe);
}

static void de437(const char *t, char *d, u32 cabe)
{
	u32 k = 0;

	for (; *t && k + 4 < cabe; t++) {
		u8 c = (u8)*t;
		u32 l = c < 0x80 ? c : letra_de_437(c);

		k += (u32)utf8_pon(l, d + k);
	}
	d[k] = 0;
}

/* ---- los archivos ---- */
char so_barra(void) { return '\\'; }

void *so_abre(const char *ruta, int modo)
{
	char r[RUTA];
	u32 m, md = modo == SO_CREAR ? PARA_CREAR : modo == SO_ANADIR ? 3 : modo == SO_CAMBIAR ? 1 : PARA_LEER;

	a437(ruta, r, sizeof r);
	m = sc(SC_ABRIR, (u32)r, md, 0);
	if (ES_ERROR(m) && modo == SO_CAMBIAR)
		m = sc(SC_ABRIR, (u32)r, PARA_CREAR, 0);
	if (ES_ERROR(m))
		return NULL;
	return (void *)(size_t)(m + 1);
}

#define MAN(f) ((u32)(size_t)(f) - 1)

int so_lee(void *f, void *d, u32 n)
{
	u32 r = sc(SC_LEER, MAN(f), (u32)d, n);

	return ES_ERROR(r) ? -1 : (int)r;
}

int so_escribe(void *f, const void *d, u32 n)
{
	const u8 *p = d;

	while (n) {
		u32 k = n > 32768 ? 32768 : n, r = sc(SC_ESCRIBIR_ARCHIVO, MAN(f), (u32)p, k);

		if (ES_ERROR(r) || r == 0)
			return -1;
		p += r;
		n -= r;
	}
	return 0;
}

int so_posicion(void *f, u32 pos) { return sc(SC_MOVER, MAN(f), pos, 0) == pos ? 0 : -1; }

u32 so_largo(void *f)
{
	u32 r = sc(SC_LARGO, MAN(f), 0, 0);

	return ES_ERROR(r) ? 0 : r;
}

int so_cierra(void *f)
{
	if (!f)
		return -1;
	return ES_ERROR(sc(SC_CERRAR, MAN(f), 0, 0)) ? -1 : 0;
}

u8 *so_lee_archivo(const char *ruta, u32 *largo, u32 max)
{
	void *f = so_abre(ruta, SO_LEER);
	u32 n, k = 0;
	u8 *d;

	*largo = 0;
	if (!f)
		return NULL;
	n = so_largo(f);
	if (n > max) {
		so_cierra(f);
		return NULL;
	}
	d = so_pide(n + 1);
	if (!d) {
		so_cierra(f);
		return NULL;
	}
	while (k < n) {
		int r = so_lee(f, d + k, n - k);

		if (r <= 0)
			break;
		k += (u32)r;
	}
	so_cierra(f);
	d[k] = 0;
	*largo = k;
	return d;
}

int so_borra(const char *ruta)
{
	char r[RUTA];

	a437(ruta, r, sizeof r);
	return ES_ERROR(sc(SC_BORRAR, (u32)r, 0, 0)) ? -1 : 0;
}

int so_renombra(const char *de, const char *a)
{
	char r[RUTA], s[RUTA];

	a437(de, r, sizeof r);
	a437(a, s, sizeof s);
	return ES_ERROR(sc(SC_RENOMBRAR, (u32)r, (u32)s, 0)) ? -1 : 0;
}

int so_existe(const char *ruta, int *es_carpeta)
{
	char r[RUTA];
	struct entrada e;
	u32 m, x;

	a437(ruta, r, sizeof r);
	if (es_carpeta)
		*es_carpeta = 0;
	if (r[0] && r[1] == ':' && (!r[2] || (r[2] == '\\' && !r[3]))) {
		if (es_carpeta)
			*es_carpeta = 1;
		return 1;
	}
	m = sc(SC_ABRIR, (u32)r, PARA_LEER, 0);
	if (ES_ERROR(m))
		return 0;
	x = sc(SC_ENTRADA, m, (u32)&e, 0);
	sc(SC_CERRAR, m, 0, 0);
	if (es_carpeta)
		*es_carpeta = !ES_ERROR(x);
	return 1;
}

int so_haz_carpeta(const char *ruta)
{
	char r[RUTA];
	int c;

	a437(ruta, r, sizeof r);
	if (!ES_ERROR(sc(SC_HACER_CARPETA, (u32)r, 0, 0)))
		return 0;
	return so_existe(ruta, &c) && c ? 0 : -1;
}

int so_carpeta(const char *ruta, int (*f)(void *u, const char *nombre, u32 tam, int es_carpeta), void *u)
{
	char r[RUTA], largo[MAX_LARGO], nom[MAX_LARGO * 3];
	struct entrada e;
	u32 m, x;

	a437(ruta, r, sizeof r);
	m = sc(SC_ABRIR, (u32)r, PARA_LEER, 0);
	if (ES_ERROR(m))
		return -1;
	for (;;) {
		memset(largo, 0, sizeof largo);
		x = sc(SC_ENTRADA_LARGA, m, (u32)&e, (u32)largo);
		if (ES_ERROR(x) || x == 0)
			break;
		e.nombre[12] = 0;
		if (!strcmp(e.nombre, ".") || !strcmp(e.nombre, "..") || (e.atributos & 0x08))
			continue;
		largo[MAX_LARGO - 1] = 0;
		de437(largo[0] ? largo : e.nombre, nom, sizeof nom);
		if (f(u, nom, e.tamano, (e.atributos & 0x10) != 0))
			break;
	}
	sc(SC_CERRAR, m, 0, 0);
	return 0;
}

/* la unidad del sistema (la de trabajo al empezar: la de \HISPA) */
static char unidad_sis(void)
{
	static char u;

	if (!u) {
		u32 l = sc(SC_UNIDAD, 0, 0, 0);

		u = (char)(l >= 'a' && l <= 'z' ? l - 32 : l >= 'A' && l <= 'Z' ? l : 'C');
	}
	return u;
}

static int hay_usuarios(void)
{
	u32 r = sc(SC_USUARIOS, 0, 0, 0);

	return !ES_ERROR(r) && r > 0 && r < 1000;
}

int so_carpeta_datos(char *d, int cabe)
{
	char t[RUTA], u[RUTA];
	int c;

	if (hay_usuarios()) {
		u32 r = sc(SC_USUARIOS, 4, (u32)t, sizeof t);

		if (!ES_ERROR(r) && r > 2 && r < sizeof t) {
			t[r] = 0;
			while (r > 3 && t[r - 1] == '\\')
				t[--r] = 0;
			de437(t, u, sizeof u);
			snprintf(d, (size_t)cabe, "%s\\CORREO", u);
			so_haz_carpeta(d);
			return 0;
		}
	}
	snprintf(t, sizeof t, "%c:\\HISPA", unidad_sis());
	if (so_existe(t, &c) && c) {
		snprintf(d, (size_t)cabe, "%c:\\HISPA\\CORREO", unidad_sis());
		if (!so_haz_carpeta(d))
			return 0;
		/* (un núcleo de antes de la rama correo: sin el sello de CORREO.BIN no
		 * se escribe en \HISPA; entonces, en la raíz, y sin el secreto) */
		so_apunta("sin el sello de Correo (el paquete base es de antes): lo guardado va en %c:\\CORREO, y no es secreto", unidad_sis());
	}
	snprintf(d, (size_t)cabe, "%c:\\CORREO", unidad_sis());
	return so_haz_carpeta(d);
}

int so_carpeta_paso(char *d, int cabe)
{
	snprintf(d, (size_t)cabe, "X:\\CORREO.TMP");
	if (!so_haz_carpeta(d))
		return 0;
	snprintf(d, (size_t)cabe, "%c:\\CORREO.TMP", unidad_sis());
	return so_haz_carpeta(d);
}

int so_carpeta_documentos(char *d, int cabe)
{
	char t[RUTA], u[RUTA];
	int c;

	if (hay_usuarios()) {
		u32 r = sc(SC_USUARIOS, 4, (u32)t, sizeof t);

		if (!ES_ERROR(r) && r > 2 && r < sizeof t) {
			t[r] = 0;
			while (r > 3 && t[r - 1] == '\\')
				t[--r] = 0;
			de437(t, u, sizeof u);
			snprintf(d, (size_t)cabe, "%s\\DOCUMENT", u);
			if (so_existe(d, &c) && c)
				return 0;
		}
	}
	snprintf(d, (size_t)cabe, "%c:\\DOCUMENT", unidad_sis());
	if (so_existe(d, &c) && c)
		return 0;
	snprintf(d, (size_t)cabe, "%c:\\", unidad_sis());
	return 0;
}

/* ---- el tiempo ---- */
u32 so_tics(void) { return sc(SC_TICS, 0, 0, 0); }

u32 so_hz(void)
{
	if (!hz_) {
		u32 h = sc(SC_DATO, DATO_HZ, 0, 0);

		hz_ = ES_ERROR(h) || !h ? 100 : h;
	}
	return hz_;
}

static u32 a_tics(u32 ms)
{
	u64 t = ((u64)ms * so_hz() + 999) / 1000;

	return t > 0x7FFFFFFF ? 0x7FFFFFFF : (u32)t;
}

void so_duerme(u32 ms)
{
	u32 t = a_tics(ms);

	sc(SC_DORMIR, t ? t : 1, 0, 0);
}

/* los días desde 1970 de una fecha (el algoritmo de los días civiles) */
static i64 dias_civiles(int a, int m, int d)
{
	int era, yoe, doy, doe;

	a -= m <= 2;
	era = (a >= 0 ? a : a - 399) / 400;
	yoe = a - era * 400;
	doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
	doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
	return (i64)era * 146097 + doe - 719468;
}

/* el último domingo de un mes, a la 01:00 UTC (el cambio de hora de la UE) */
static i64 ultimo_domingo(int a, int m)
{
	i64 d = dias_civiles(a, m + 1, 1) - 1;	/* el último día del mes */
	int sem = (int)((d + 4) % 7);		/* 0: domingo (el 1/1/1970 fue jueves) */

	if (sem < 0)
		sem += 7;
	return (d - sem) * 86400 + 3600;
}

int so_zona(i64 cuando)
{
	/* la hora de España (la península): +1, y +2 en verano */
	int a = 1970 + (int)(cuando / 31556952);
	i64 ini, fin;

	if (a < 1980 || a > 2100)
		return 60;
	ini = ultimo_domingo(a, 3);
	fin = ultimo_domingo(a, 10);
	return cuando >= ini && cuando < fin ? 120 : 60;
}

i64 so_ahora(void)
{
	u32 f = sc(SC_DATO, DATO_FECHA, 0, 0), fe = f >> 16, ho = f & 0xFFFF;
	int a = 1980 + (int)(fe >> 9), m = (int)((fe >> 5) & 15), d = (int)(fe & 31);
	i64 local;

	if (m < 1 || m > 12 || d < 1)
		return 0;
	local = dias_civiles(a, m, d) * 86400 + (ho >> 11) * 3600 + ((ho >> 5) & 63) * 60 + (ho & 31) * 2;
	/* (la zona de esa hora local: se mira con la UTC aproximada) */
	return local - (i64)so_zona(local - 3600) * 60;
}

/* el azar: el contador de ciclos, los tics, la fecha y lo de antes, mezclados
 * (no es para cifrar: es para los Message-ID y las etiquetas) */
void so_azar(void *d, u32 n)
{
	static u64 s = 0x9E3779B97F4A7C15ull;
	u8 *p = d;

	while (n--) {
		u32 a, e;

		__asm__ volatile("rdtsc" : "=a"(a), "=d"(e));
		s ^= ((u64)e << 32 | a) + so_tics();
		s ^= s >> 12;
		s ^= s << 25;
		s ^= s >> 27;
		*p++ = (u8)((s * 0x2545F4914F6CDD1Dull) >> 56);
	}
}

/* ---- lo que pasa: al puerto serie ---- */
void so_apunta(const char *f, ...)
{
	char t[600], u[640];
	va_list a;
	int n;

	va_start(a, f);
	vsnprintf(t, sizeof t, f, a);
	va_end(a);
	snprintf(u, sizeof u, "correo: %s\n", t);
	/* (en la 437, como lo de los demás: las pruebas lo pasan a UTF-8) */
	n = (int)utf8_a_437(u, (u32)strlen(u), t, sizeof t);
	sc(SC_SERIE, (u32)t, (u32)n, 0);
}

void so_dice(const char *t)
{
	char d[1200];
	u32 n = (u32)strlen(t), i = 0;

	while (i < n) {
		u32 k = n - i > 380 ? 380 : n - i, m;

		while (k < n - i && ((u8)t[i + k] & 0xC0) == 0x80)
			k--;
		m = utf8_a_437(t + i, k, d, sizeof d);
		sc(SC_ESCRIBIR, (u32)d, m, 0);
		i += k;
	}
}

/* ---- la red ---- */
static u32 cab[8];
static u32 pide(u32 sv, u32 orden, u32 a, u32 b, u32 c, u32 d4, const void *datos, u32 largo, void *dest, u32 cabe)
{
	cab[0] = orden, cab[1] = a, cab[2] = b, cab[3] = c;
	cab[4] = d4, cab[5] = cab[6] = 0;
	cab[7] = largo;
	if (largo && dest && dest != datos)
		memcpy(dest, datos, largo);
	return sc(sv, (u32)cab, (u32)(dest ? dest : (void *)datos), cabe);
}

static void espera_red(void)
{
	int i;

	for (i = 0; i < 100; i++) {
		u32 ip, e = sc(SC_ESTADO_RED, (u32)&ip, 0, 0);

		if (e != RED_BUSCANDO && e != RED_SIN_CABLE)
			return;
		if (e == RED_SIN_CABLE && i >= 60)
			return;
		sc(SC_DORMIR, so_hz() / 10, 0, 0);
	}
}

int so_hay_red(void)
{
	u32 ip, e = sc(SC_ESTADO_RED, (u32)&ip, 0, 0);

	return e == RED_CONECTADA;
}

static const char *texto_red(u32 e)
{
	switch (e) {
	case SIN_RED: return "No hay red: el servicio de red (RED.BIN) no est\xc3\xa1 en marcha.";
	case SIN_TARJETA: return "No hay red: no se ha encontrado una tarjeta de red que se sepa llevar.";
	case SIN_CABLE: return "No hay red: el cable no est\xc3\xa1 enchufado (ni hay Wi-Fi).";
	case SIN_IP: return "No hay red: todav\xc3\xad" "a no hay direcci\xc3\xb3n IP.";
	case TIEMPO: return "El servidor no contesta.";
	case RECHAZADA: return "El servidor no contesta (conexi\xc3\xb3n rechazada): \xc2\xbf" "es ese el puerto?";
	case NO_RESUELVE: return "No se encuentra el servidor (\xc2\xbf" "est\xc3\xa1 bien escrito su nombre?).";
	case CORTADA_RED: return "Se ha cortado la conexi\xc3\xb3n.";
	case INALCANZABLE: return "No se llega al servidor.";
	case SIN_TLS: return "Falta el servicio de las conexiones seguras (TLS.BIN): instale el paquete \xc2\xabtls\xc2\xbb.";
	case TLS_SIN_HORA: return "El reloj del equipo no est\xc3\xa1 en hora: sin la fecha no se pueden comprobar los certificados.";
	}
	return "La red ha fallado.";
}

static int de_error(u32 e)
{
	switch (e) {
	case SIN_RED: case SIN_TARJETA: case SIN_CABLE: case SIN_IP: return SO_E_RED;
	case NO_RESUELVE: return SO_E_NOMBRE;
	case TIEMPO: case RECHAZADA: case INALCANZABLE: case NO_CONEXION: return SO_E_CONEXION;
	case CORTADA_RED: return SO_E_CORTADA;
	case TLS_CERTIFICADO: return SO_E_CERTIFICADO;
	case TLS_FALLO: case SIN_TLS: case TLS_SIN_HORA: return SO_E_TLS;
	}
	return SO_E_OTRO;
}

/* las conexiones: el número de RED.BIN o de TLS.BIN (>= 0x1000) */
#define N_CON 8
static struct { u32 num, segura, cerrada; } cons[N_CON];

static int nueva(u32 num, int segura)
{
	int k;

	for (k = 0; k < N_CON; k++)
		if (!cons[k].num) {
			cons[k].num = num;
			cons[k].segura = (u32)segura;
			cons[k].cerrada = 0;
			return k + 1;
		}
	pide(segura ? SC_TLS : SC_RED, O_CERRAR, num, 0, 0, 0, NULL, 0, NULL, 0);
	return SO_E_OTRO;
}

int so_conecta(const char *servidor, u32 puerto, int modo, u32 espera_ms, char *error, int cabe)
{
	char nom[128], t[220];
	u32 r, n, ip = 0;
	u32 tt = a_tics(espera_ms ? espera_ms : 20000);
	u32 st = modo == SO_STARTTLS_IMAP ? 1 : modo == SO_STARTTLS_SMTP ? 2 : modo == SO_STARTTLS_POP3 ? 3 : 0;

	if (error && cabe)
		error[0] = 0;
	n = (u32)strlen(servidor);
	if (n == 0 || n > 63) {
		if (error)
			snprintf(error, (size_t)cabe, "El nombre del servidor no vale.");
		return SO_E_NOMBRE;
	}
	memcpy(nom, servidor, n + 1);
	if (modo == SO_PLANO) {
		r = pide(SC_RED, O_RESOLVER, 0, 0, 0, 0, nom, n, t, sizeof t);
		if (r == SIN_IP || r == SIN_CABLE) {
			espera_red();
			r = pide(SC_RED, O_RESOLVER, 0, 0, 0, 0, nom, n, t, sizeof t);
		}
		if (r != 0) {
			if (error)
				snprintf(error, (size_t)cabe, "%s", texto_red(r));
			return de_error(r);
		}
		ip = cab[1];
		r = pide(SC_RED, O_CONECTAR, ip, puerto, tt, 0, NULL, 0, NULL, 0);
		if (ES_ERROR_RED(r)) {
			if (error)
				snprintf(error, (size_t)cabe, "%s", texto_red(r));
			return de_error(r);
		}
		return nueva(r, 0);
	}
	memcpy(t, nom, n + 1);
	r = pide(SC_TLS, O_CONECTAR, 0, puerto, tt, st, nom, n, t, sizeof t - 1);
	if (r == SIN_IP || r == SIN_CABLE) {
		espera_red();
		r = pide(SC_TLS, O_CONECTAR, 0, puerto, tt, st, nom, n, t, sizeof t - 1);
	}
	if (ES_ERROR_RED(r)) {
		int e = de_error(r);

		if ((r == TLS_CERTIFICADO || r == TLS_FALLO || r == TLS_SIN_HORA) && cab[7] > 0 && cab[7] < sizeof t) {
			t[cab[7]] = 0;
			if (r == TLS_FALLO && cab[1] == 0x5354)
				e = SO_E_STARTTLS;
			if (error)
				snprintf(error, (size_t)cabe, "%s", t);
		} else if (error)
			snprintf(error, (size_t)cabe, "%s", texto_red(r));
		return e;
	}
	return nueva(r, 1);
}

int so_envia(int c, const void *d, u32 n)
{
	const u8 *p = d;

	if (c < 1 || c > N_CON || !cons[c - 1].num)
		return SO_E_OTRO;
	while (n) {
		u32 k = n > MAX_DATOS ? MAX_DATOS : n;
		u32 r = pide(cons[c - 1].segura ? SC_TLS : SC_RED, O_ENVIAR, cons[c - 1].num, 0, 0, 0, p, k, NULL, 0);

		if (ES_ERROR_RED(r))
			return de_error(r);
		if (r == 0)
			return SO_E_CORTADA;
		p += r;
		n -= r;
	}
	return 0;
}

int so_recibe(int c, void *d, u32 cabe, u32 espera_ms)
{
	u32 r, t;

	if (c < 1 || c > N_CON || !cons[c - 1].num)
		return SO_E_OTRO;
	if (cons[c - 1].cerrada)
		return 0;
	if (cabe > MAX_DATOS)
		cabe = MAX_DATOS;
	t = espera_ms ? a_tics(espera_ms) : 0;
	r = pide(cons[c - 1].segura ? SC_TLS : SC_RED, O_RECIBIR, cons[c - 1].num, t, 0, 0, NULL, 0, d, cabe);
	if (r == TIEMPO)
		return SO_E_ESPERA;
	if (ES_ERROR_RED(r))
		return de_error(r);
	if (r == 0)
		cons[c - 1].cerrada = 1;
	return (int)r;
}

void so_cierra_con(int c)
{
	if (c < 1 || c > N_CON || !cons[c - 1].num)
		return;
	pide(cons[c - 1].segura ? SC_TLS : SC_RED, O_CERRAR, cons[c - 1].num, 0, 0, 0, NULL, 0, NULL, 0);
	cons[c - 1].num = 0;
}

int so_cifrado(int c, char *d, int cabe)
{
	u32 r;

	if (cabe < 2 || c < 1 || c > N_CON || !cons[c - 1].num || !cons[c - 1].segura)
		return 0;
	r = pide(SC_TLS, O_INFO, cons[c - 1].num, 0, 0, 0, NULL, 0, d, (u32)cabe - 1);
	if (ES_ERROR_RED(r)) {
		d[0] = 0;
		return 0;
	}
	d[cab[7] < (u32)cabe ? cab[7] : (u32)cabe - 1] = 0;
	return 1;
}

int so_dns(const u8 *pregunta, u32 n, u8 *respuesta, u32 cabe, u32 espera_ms)
{
	u32 info[32], s, r, dns, puerto;

	memset(info, 0, sizeof info);
	r = pide(SC_RED, O_ESTADO, 0, 0, 0, 0, NULL, 0, info, sizeof info);
	if (ES_ERROR_RED(r))
		return -1;
	dns = info[6];		/* (Info_red: estado, mac[8], ip, máscara, puerta, dns...) */
	if (!dns || n > 512)
		return -1;
	so_azar(&puerto, sizeof puerto);
	puerto = 40000 + puerto % 20000;
	s = pide(SC_RED, O_UDP, puerto, 0, 0, 0, NULL, 0, NULL, 0);
	if (ES_ERROR_RED(s))
		return -1;
	r = pide(SC_RED, O_UDP_ENVIAR, s, dns, 53, 0, pregunta, n, NULL, 0);
	if (!ES_ERROR_RED(r)) {
		if (cabe > MAX_DATOS)
			cabe = MAX_DATOS;
		r = pide(SC_RED, O_RECIBIR, s, a_tics(espera_ms), 0, 0, NULL, 0, respuesta, cabe);
	}
	pide(SC_RED, O_CERRAR, s, 0, 0, 0, NULL, 0, NULL, 0);
	return ES_ERROR_RED(r) ? -1 : (int)r;
}

/* ---- los otros programas ---- */
int so_lanza_espera(const char *programa, const char *argumentos)
{
	char p[RUTA], a[400];
	u32 t;

	a437(programa, p, sizeof p);
	a437(argumentos ? argumentos : "", a, sizeof a);
	t = sc(SC_LANZAR, (u32)p, (u32)a, 0);
	if (ES_ERROR(t))
		return -1;
	return (int)sc(SC_ESPERAR, t, 0, 0);
}

int so_lanza(const char *programa, const char *argumentos)
{
	char p[RUTA], a[400];
	u32 t;

	a437(programa, p, sizeof p);
	a437(argumentos ? argumentos : "", a, sizeof a);
	t = sc(SC_LANZAR_SUELTO, (u32)p, (u32)a, 0);
	return ES_ERROR(t) ? -1 : 0;
}

int so_programa(const char *nombre, char *ruta, int cabe)
{
	int c;

	snprintf(ruta, (size_t)cabe, "%c:\\HISPA\\%s", unidad_sis(), nombre);
	if (so_existe(ruta, &c) && !c)
		return 1;
	snprintf(ruta, (size_t)cabe, "%c:\\%s", unidad_sis(), nombre);
	return so_existe(ruta, &c) && !c;
}

int so_argumentos(char *d, int cabe)
{
	char t[400];
	u32 n = sc(SC_ARGUMENTOS, (u32)t, sizeof t - 1, 0);

	if (ES_ERROR(n))
		n = 0;
	if (n > sizeof t - 1)
		n = sizeof t - 1;
	t[n] = 0;
	de437(t, d, (u32)cabe);
	return (int)strlen(d);
}

/* ---- el portapapeles (el del sistema es de la 437) ---- */
int so_al_portapapeles(const char *t, u32 n)
{
	char *d = so_pide(n + 1);
	u32 k;

	if (!d)
		return -1;
	k = utf8_a_437(t, n, d, n + 1);
	sc(SC_PORTAPAPELES, 0, (u32)d, k);
	so_suelta(d);
	return 0;
}

char *so_del_portapapeles(u32 *n)
{
	u32 k = sc(SC_PORTAPAPELES, 2, 0, 0), j, m = 0;
	char *t, *d;

	*n = 0;
	if (ES_ERROR(k) || k == 0 || k > (4u << 20))
		return NULL;
	t = so_pide(k + 1);
	d = so_pide(k * 3 + 1);
	if (!t || !d) {
		so_suelta(t);
		so_suelta(d);
		return NULL;
	}
	k = sc(SC_PORTAPAPELES, 1, (u32)t, k);
	if (ES_ERROR(k))
		k = 0;
	for (j = 0; j < k; j++) {
		u8 c = (u8)t[j];

		m += (u32)utf8_pon(c < 0x80 ? c : letra_de_437(c), d + m);
	}
	d[m] = 0;
	so_suelta(t);
	*n = m;
	return d;
}

/* =============================================================================
 *  La ventana (ui.h): las llamadas 32 a 39 y 80
 * ============================================================================= */
static u32 vent;
static struct orden ord;

static void dibuja(u32 op, int x, int y, int an, int al, u32 t1, u32 t2, const void *d, int n)
{
	ord.op = op;
	ord.x = x;
	ord.y = y;
	ord.ancho = an;
	ord.alto = al;
	ord.tinta = t1;
	ord.tinta2 = t2;
	ord.datos = d;
	ord.n = n;
	sc(SC_DIBUJAR, vent, (u32)&ord, 0);
}

int ui_abre(const char *titulo, int an, int al)
{
	char t[128];

	a437(titulo, t, sizeof t);
	vent = sc(SC_VENTANA, (u32)t, (u32)an, (u32)al);
	if (ES_ERROR(vent) || !vent)
		return -1;
	sc(SC_COLOCAR, vent, 0xFFFFFFFFu, 0);
	return 0;
}

int ui_abre_aviso(int an, int al)
{
	int pa, pal;

	vent = sc(SC_VENTANA, (u32)"Correo nuevo", (u32)an, (u32)al);
	if (ES_ERROR(vent) || !vent)
		return -1;
	ui_pantalla(&pa, &pal);
	if (pa > an + 30 && pal > al + 60)
		sc(SC_COLOCAR, vent, (u32)(pa - an - 20), (u32)(pal - al - 34));
	return 0;
}

void ui_cierra(void)
{
	if (vent)
		sc(SC_CERRAR_VENTANA, vent, 0, 0);
	vent = 0;
}

void ui_titulo(const char *t)
{
	char d[128];

	a437(t, d, sizeof d);
	sc(SC_TITULO, vent, (u32)d, 0);
}

void ui_pantalla(int *an, int *al)
{
	u32 a = sc(SC_DATO, DATO_ANCHO_PANTALLA, 0, 0), b = sc(SC_DATO, DATO_ALTO_LIBRE, 0, 0);

	*an = ES_ERROR(a) || !a ? 800 : (int)a;
	*al = ES_ERROR(b) || !b ? 560 : (int)b;
}

void ui_maximiza(void) { sc(SC_MAXIMIZAR, vent, 0, 0); }

int ui_suceso(Suceso *s, u32 espera_ms)
{
	struct suceso x;
	u32 hasta = so_tics() + a_tics(espera_ms);

	for (;;) {
		if (sc(SC_SUCESO, vent, (u32)&x, 0) == 1) {
			s->clase = x.clase;
			s->a = x.a;
			s->b = x.b;
			s->c = x.c;
			if (x.clase == EV_TECLA) {
				/* la letra, de la 437 a Unicode (las de control y las 0x1xx, tal cual) */
				if (x.a >= 0x80 && x.a < 0x100)
					s->a = letra_de_437((u8)x.a);
				else if (x.a == 10)
					s->a = UT_INTRO;
			}
			return 1;
		}
		if (!espera_ms || (int)(hasta - so_tics()) <= 0)
			return 0;
		sc(SC_DORMIR, 1, 0, 0);
	}
}

void ui_rect(int x, int y, int an, int al, u32 c) { if (an > 0 && al > 0) dibuja(OP_RECTANGULO, x, y, an, al, c, 0, 0, 0); }
void ui_degradado(int x, int y, int an, int al, u32 a, u32 b) { if (an > 0 && al > 0) dibuja(OP_DEGRADADO, x, y, an, al, a, b, 0, 0); }
void ui_redondo(int x, int y, int an, int al, int r, u32 a, u32 b) { if (an > 0 && al > 0) dibuja(OP_REDONDEADO, x, y, an, al, a, b, 0, r); }
void ui_marco(int x, int y, int an, int al, u32 c) { if (an > 0 && al > 0) dibuja(OP_MARCO, x, y, an, al, c, 0, 0, 0); }
void ui_linea(int x1, int y1, int x2, int y2, u32 c) { dibuja(OP_LINEA, x1, y1, x2, y2, c, 0, 0, 0); }
void ui_mapa(int x, int y, int an, int al, const u32 *p) { if (an > 0 && al > 0) dibuja(OP_MAPA, x, y, an, al, 0, 0, p, 0); }

void ui_texto(int x, int y, const char *t, u32 n, int max, u32 c, int negrita)
{
	char d[400];
	u32 k;

	if (max <= 0)
		return;
	if (max > 390)
		max = 390;
	k = utf8_a_437(t, n, d, sizeof d);
	if ((int)k > max) {
		k = (u32)max;
		if (max > 3)
			memcpy(d + max - 3, "...", 3);
	}
	if (k)
		dibuja(negrita ? OP_NEGRITA : OP_TEXTO, x, y, 0, 0, c, 0, d, (int)k);
}

void ui_menu(int x, int y, const char *opciones, u32 grises, u32 etiqueta)
{
	static struct pide_menu p;
	static char t[1200];

	a437(opciones, t, sizeof t);
	p.x = x;
	p.y = y;
	p.opciones = t;
	p.inactivas = grises;
	p.etiqueta = etiqueta;
	sc(SC_MENU, vent, (u32)&p, 0);
}

/* ---- la entrada ---- */
int correo_principal(void);

int correo_pentium(void)
{
	return correo_principal();
}
