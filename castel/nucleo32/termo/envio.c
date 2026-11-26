/* envio.c - (TERMO) «Enviar a Carlos»: sube por HTTPS la carpeta TERMO (los
 * .CSV, .LOG, .TXT y .BMP, y el \ARRANQUE.TXT y el \ACPI.TXT del arranque)
 * al receptor del servidor (tools/termo-subida/subir.php), para verlos sin
 * tener que sacar la memoria del portátil. Lo hace TERMO.BIN /ENVIAR: desde
 * la Consola (lo dice en la pantalla) o lanzado por el botón del Termómetro
 * (/ENVIAR /VENTANA: lo va dejando en TERMO\ENVIO.EST, que la ventana lee).
 *
 * TERMO\ENVIO.INI (lo hace usb.sh con TERMO=1; nunca se sube):
 *   servidor=https://hispados.es/termo/subir.php   (uno o dos)
 *   equipo=Portatil-gamer
 *   testigo=64 cifras hexadecimales (32 octetos al azar; el servidor solo
 *           guarda su SHA-256 y lo revoca borrando su línea)
 *   prueba=si   (solo las pruebas: deja un servidor http://, sin cifrar)
 * Un POST multipart/form-data por archivo (testigo, equipo, lote y el
 * archivo), con Connection: close; contesta «OK ...» o «ERROR ...». Si se
 * corta, hasta 4 intentos por archivo (a los 2, 5 y 10 s), y luego el otro
 * servidor; lo que el servidor rechaza (testigo, tamaño, tipo, tope del
 * día) no se repite. En TERMO\ENVIO.HEC, lo ya mandado (nombre y tamaño):
 * la vez siguiente solo va lo nuevo o lo que ha crecido.
 * (TERMO 1.2) Lo que se dice, para la pantalla (ENVIO.EST, una línea):
 * «Enviando... (archivo 3 de 9: NOMBRE)» mientras tanto y, al acabar, una
 * que empieza por «ENVIADO:» o por «NO ENVIADO:», con el motivo en llano
 * (sin red, el DNS, no se puede conectar, el certificado, el testigo
 * desconocido -403-, el tope del día -429-...) y qué hacer. Un servidor
 * con el que no se llega a conectar (el primer archivo) no se reintenta:
 * se pasa al otro enseguida; y hispados.es va antes que actualiza.hispados.es
 * (que no tiene HTTPS; las memorias de antes lo llevan primero).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "juego.h"
#include <stdarg.h>
#include "envio.h"

#define SC_RED 90
#define SC_ESTADO_RED 97
#define SC_TLS 99
#define ES_ERROR_RED(r) ((u32)(r) >= 0xFFFFFFE0u)
#define O_RESOLVER 2
#define O_CONECTAR 4
#define O_ENVIAR 5
#define O_RECIBIR 6
#define O_CERRAR 7
#define SIN_RED 0xFFFFFFEFu
#define SIN_TARJETA 0xFFFFFFEEu
#define SIN_CABLE 0xFFFFFFEDu
#define SIN_IP 0xFFFFFFECu
#define TIEMPO 0xFFFFFFEBu
#define RECHAZADA 0xFFFFFFEAu
#define NO_RESUELVE 0xFFFFFFE9u
#define CORTADA_RED 0xFFFFFFE8u
#define INALCANZABLE 0xFFFFFFE6u
#define TLS_CERTIFICADO 0xFFFFFFE5u
#define TLS_FALLO 0xFFFFFFE4u
#define SIN_TLS 0xFFFFFFE3u
#define TLS_SIN_HORA 0xFFFFFFE2u
#define MAX_DATOS 8192
#define RED_CONECTADA 4
#define MAX_ARCHIVO (4u << 20)
#define MAX_ARCHIVOS 200

static int consola;

static char *busca_en(const char *t, const char *o)
{
	size_t n = strlen(o);

	for (; *t; t++)
		if (!strncmp(t, o, n))
			return (char *)t;
	return 0;
}
static char carpeta[64], ruta_est[80], ruta_hec[80];
static char servidores[2][200], equipo[48], testigo[72];
static int n_servidores, prueba;
/* (TERMO 1.2) el último fallo: el de la red (o 0) y el código HTTP (o 0) */
static u32 fallo_red;
static int fallo_http;

/* ---- lo que se dice: a la Consola, o a ENVIO.EST para la ventana ---- */
static void di(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void di(const char *fmt, ...)
{
	char t[800], u[820];
	va_list v;
	int n;

	va_start(v, fmt);
	vsnprintf(t, sizeof t, fmt, v);
	va_end(v);
	serie("termo: envío: %s\n", t);
	if (consola) {
		snprintf(u, sizeof u, "%s\n", t);
		n = a437(u, t, sizeof t);
		sc(SC_ESCRIBIR, (u32)t, (u32)n, 0);
	} else {
		u32 m = sc(SC_ABRIR, (u32)ruta_est, PARA_CREAR, 0);

		if (!ES_ERROR(m)) {
			sc(SC_ESCRIBIR_ARCHIVO, m, (u32)t, (u32)strlen(t));
			sc(SC_CERRAR, m, 0, 0);
		}
	}
}

/* ---- la red (como Correo: RED.BIN, la 90, y TLS.BIN, la 99) ---- */
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

static const char *texto_red(u32 e)
{
	switch (e) {
	case SIN_RED: return "el servicio de red (RED.BIN) no está en marcha";
	case SIN_TARJETA: return "no hay una tarjeta de red que se sepa llevar";
	case SIN_CABLE: return "no hay cable ni Wi-Fi conectado";
	case SIN_IP: return "todavía no hay dirección IP";
	case TIEMPO: return "el servidor no contesta a tiempo";
	case RECHAZADA: return "el servidor rechaza la conexión";
	case NO_RESUELVE: return "no se encuentra el servidor (¿hay salida a Internet?)";
	case CORTADA_RED: return "se ha cortado la conexión";
	case INALCANZABLE: return "no se llega al servidor";
	case TLS_CERTIFICADO: return "el certificado del servidor no vale";
	case TLS_FALLO: return "la conexión cifrada ha fallado";
	case SIN_TLS: return "falta el servicio TLS (TLS.BIN)";
	case TLS_SIN_HORA: return "el reloj del equipo no está en hora (hace falta para los certificados)";
	}
	return "la red ha fallado";
}

/* (TERMO 1.2) el motivo del último fallo, en llano, y qué hacer */
static void motivo_llano(char *d, int cabe, const char *por_que)
{
	if (fallo_http == 403)
		snprintf(d, (size_t)cabe, "el servidor no reconoce esta memoria («%s»: el testigo es desconocido o se ha revocado). Avise a Carlos para que lo dé de alta.", por_que);
	else if (fallo_http == 429)
		snprintf(d, (size_t)cabe, "el servidor dice que por hoy ya se ha mandado bastante (64 MB o 400 archivos). Pulse E mañana.");
	else if (fallo_http == 413 || fallo_http == 415 || fallo_http == 400)
		snprintf(d, (size_t)cabe, "el servidor no acepta un archivo (%s).", por_que);
	else if (fallo_http)
		snprintf(d, (size_t)cabe, "el servidor ha contestado con un error (%d: %s). Pruebe más tarde (E).", fallo_http, por_que);
	else if (fallo_red == SIN_CABLE || fallo_red == SIN_IP || fallo_red == SIN_RED || fallo_red == SIN_TARJETA)
		snprintf(d, (size_t)cabe, "sin red (%s). Enchufe el cable de red, conecte el Wi-Fi o el móvil por USB (enchúfelo y active el Anclaje de USB: en Android, Ajustes › Conexión y compartir › Anclaje de USB) y pulse E otra vez.", texto_red(fallo_red));
	else if (fallo_red == NO_RESUELVE)
		snprintf(d, (size_t)cabe, "no se encuentra el servidor (el DNS no da su dirección: ¿hay salida a Internet?). Compruébelo y pulse E otra vez.");
	else if (fallo_red == TLS_CERTIFICADO)	/* (1.2.1: por_que ya lo dice: «el certificado del servidor no vale (...)») */
		snprintf(d, (size_t)cabe, "%s. Avise a Carlos.", por_que);
	else if (fallo_red == TLS_SIN_HORA)
		snprintf(d, (size_t)cabe, "el reloj del equipo no está en hora y sin ella no se pueden comprobar los certificados. Póngalo en hora (Panel de control) y pulse E otra vez.");
	else if (fallo_red == SIN_TLS)
		snprintf(d, (size_t)cabe, "falta el servicio TLS (TLS.BIN), que hace falta para HTTPS.");
	else if (fallo_red == RECHAZADA || fallo_red == INALCANZABLE || fallo_red == TIEMPO || fallo_red == TLS_FALLO || fallo_red == CORTADA_RED)
		snprintf(d, (size_t)cabe, "no se puede conectar con el servidor (%s). Pruebe otra vez dentro de un rato (E).", por_que);
	else
		snprintf(d, (size_t)cabe, "%s. Pruebe otra vez (E).", por_que);
}

/* ¿hay red? espera hasta 20 s a que haya dirección (el DHCP del Wi-Fi tarda) */
static int hay_red(void)
{
	u32 ip, e = 0;
	int i;

	for (i = 0; i < 40; i++) {
		e = sc(SC_ESTADO_RED, (u32)&ip, 0, 0);
		if (e == RED_CONECTADA)
			return 1;
		if (i == 0)
			di("Esperando a la red...");
		sc(SC_DORMIR, jg_hz / 2, 0, 0);
	}
	return 0;
}

struct url { char host[128], camino[160]; u32 puerto; int segura; };

static int parte_url(const char *u, struct url *r)
{
	const char *p = u;
	int i = 0;

	memset(r, 0, sizeof *r);
	if (!strncmp(p, "https://", 8))
		r->segura = 1, p += 8, r->puerto = 443;
	else if (!strncmp(p, "http://", 7))
		p += 7, r->puerto = 80;
	else
		return 0;
	while (*p && *p != '/' && *p != ':' && i < (int)sizeof r->host - 1)
		r->host[i++] = *p++;
	if (!i)
		return 0;
	if (*p == ':') {
		r->puerto = 0;
		for (p++; *p >= '0' && *p <= '9'; p++)
			r->puerto = r->puerto * 10 + (u32)(*p - '0');
		if (!r->puerto || r->puerto > 65535)
			return 0;
	}
	snprintf(r->camino, sizeof r->camino, "%s", *p ? p : "/");
	return 1;
}

static u32 tics_de(u32 s) { return s * jg_hz; }

/* una conexión: el número de RED.BIN o de TLS.BIN, o un error */
static u32 conecta(const struct url *u, char *error, int cabe)
{
	char nom[160], t[240];
	u32 n = (u32)strlen(u->host), r;

	error[0] = 0;
	memcpy(nom, u->host, n + 1);
	if (!u->segura) {
		r = pide(SC_RED, O_RESOLVER, 0, 0, 0, 0, nom, n, t, sizeof t);
		if (r != 0) {
			snprintf(error, (size_t)cabe, "%s", texto_red(r));
			return r ? r : TIEMPO;
		}
		r = pide(SC_RED, O_CONECTAR, cab[1], u->puerto, tics_de(20), 0, NULL, 0, NULL, 0);
		if (ES_ERROR_RED(r))
			snprintf(error, (size_t)cabe, "%s", texto_red(r));
		return r;
	}
	r = pide(SC_TLS, O_CONECTAR, 0, u->puerto, tics_de(30), 0, nom, n, t, sizeof t - 1);
	if (ES_ERROR_RED(r)) {
		if ((r == TLS_CERTIFICADO || r == TLS_FALLO || r == TLS_SIN_HORA) && cab[7] > 0 && cab[7] < sizeof t) {
			char x[300];

			t[cab[7]] = 0;
			snprintf(x, sizeof x, "%s (%s)", texto_red(r), t);
			snprintf(error, (size_t)cabe, "%s", x);
		} else
			snprintf(error, (size_t)cabe, "%s", texto_red(r));
	}
	return r;
}

static void cierra(const struct url *u, u32 c)
{
	pide(u->segura ? SC_TLS : SC_RED, O_CERRAR, c, 0, 0, 0, NULL, 0, NULL, 0);
}

static u32 envia(const struct url *u, u32 c, const u8 *d, u32 n)
{
	while (n) {
		u32 k = n > MAX_DATOS ? MAX_DATOS : n;
		u32 r = pide(u->segura ? SC_TLS : SC_RED, O_ENVIAR, c, 0, 0, 0, d, k, NULL, 0);

		if (ES_ERROR_RED(r))
			return r;
		if (r == 0)
			return CORTADA_RED;
		d += r;
		n -= r;
	}
	return 0;
}

/* la respuesta entera (hasta que el servidor cierra): lo de dentro, a d */
static u32 recibe(const struct url *u, u32 c, char *d, u32 cabe, u32 *n)
{
	*n = 0;
	for (;;) {
		u32 r = pide(u->segura ? SC_TLS : SC_RED, O_RECIBIR, c, tics_de(30), 0, 0, NULL, 0, d + *n, cabe - 1 - *n > MAX_DATOS ? MAX_DATOS : cabe - 1 - *n);

		if (r == 0 || r == CORTADA_RED)
			break;
		if (ES_ERROR_RED(r))
			return *n ? 0 : r;
		*n += r;
		if (*n >= cabe - 1)
			break;
	}
	d[*n] = 0;
	return 0;
}

/* ---- lo que se manda ---- */
static int lee_archivo(const char *ruta, u8 **b, u32 *n)
{
	u32 m = sc(SC_ABRIR, (u32)ruta, PARA_LEER, 0), l, r;

	*b = 0;
	if (ES_ERROR(m))
		return 0;
	l = sc(SC_LARGO, m, 0, 0);
	if (ES_ERROR(l) || l > MAX_ARCHIVO) {
		sc(SC_CERRAR, m, 0, 0);
		return 0;
	}
	*b = (u8 *)malloc(l + 1);
	r = *b ? sc(SC_LEER, m, (u32)*b, l) : 0;
	sc(SC_CERRAR, m, 0, 0);
	/* (TERMO 1.2) el .LOG que el Termómetro está escribiendo a la vez puede
	 * dar menos de lo que medía: se manda lo leído */
	if (!*b || ES_ERROR(r) || r == 0 || r > l) {
		free(*b);
		*b = 0;
		return 0;
	}
	*n = r;
	return 1;
}

static char hecho[8192];	/* ENVIO.HEC: «NOMBRE tamaño» por línea */

static int ya_mandado(const char *nombre, u32 tam)
{
	char l[40];

	snprintf(l, sizeof l, "%s %u\r\n", nombre, tam);
	return busca_en(hecho, l) != 0;
}

static void apunta_mandado(const char *nombre, u32 tam)
{
	size_t n = strlen(hecho);
	u32 m;

	if (n + 40 < sizeof hecho)
		snprintf(hecho + n, sizeof hecho - n, "%s %u\r\n", nombre, tam);
	m = sc(SC_ABRIR, (u32)ruta_hec, PARA_CREAR, 0);
	if (!ES_ERROR(m)) {
		sc(SC_ESCRIBIR_ARCHIVO, m, (u32)hecho, (u32)strlen(hecho));
		sc(SC_CERRAR, m, 0, 0);
	}
}

/* un archivo, al servidor u: 0 bien; 1 lo rechaza (no se repite); 2 se puede
 * repetir; 3 con ese servidor no hay manera (el certificado, la hora, el
 * nombre, sin TLS.BIN): el otro, si lo hay */
static int ningun_bueno = 1;	/* (TERMO 1.2) aún no ha ido bien ningún archivo */
static int manda(const struct url *u, const char *lote, const char *nombre, const u8 *d, u32 n, char *por_que, int cabe)
{
	static const char lim[] = "----HispaTermo7d1f0b2a9c";
	char *cuerpo, cab_http[600];
	static char resp[2048];
	u32 c, r, nr, nc = 0, cabe_c = n + 1400;
	int m;
	const char *p;

	cuerpo = (char *)malloc(cabe_c);
	if (!cuerpo) {
		snprintf(por_que, (size_t)cabe, "no hay memoria");
		return 1;
	}
	nc = (u32)snprintf(cuerpo, cabe_c,
			   "--%s\r\nContent-Disposition: form-data; name=\"testigo\"\r\n\r\n%s\r\n"
			   "--%s\r\nContent-Disposition: form-data; name=\"equipo\"\r\n\r\n%s\r\n"
			   "--%s\r\nContent-Disposition: form-data; name=\"lote\"\r\n\r\n%s\r\n"
			   "--%s\r\nContent-Disposition: form-data; name=\"archivo\"; filename=\"%s\"\r\nContent-Type: application/octet-stream\r\n\r\n",
			   lim, testigo, lim, equipo, lim, lote, lim, nombre);
	memcpy(cuerpo + nc, d, n);
	nc += n;
	nc += (u32)snprintf(cuerpo + nc, cabe_c - nc, "\r\n--%s--\r\n", lim);
	m = snprintf(cab_http, sizeof cab_http,
		     "POST %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: Hispa-Pentium-Termometro/1.0\r\n"
		     "Content-Type: multipart/form-data; boundary=%s\r\nContent-Length: %u\r\nConnection: close\r\n\r\n",
		     u->camino, u->host, lim, nc);
	c = conecta(u, por_que, cabe);
	fallo_red = 0;
	fallo_http = 0;
	if (ES_ERROR_RED(c)) {
		free(cuerpo);
		fallo_red = c;
		/* (TERMO 1.2) sin conectar, con el primer archivo: el otro servidor
		 * ya (el ASUS GL553VD: actualiza.hispados.es no tiene HTTPS y se
		 * iban minutos en reintentos antes de probar hispados.es) */
		if (c == SIN_CABLE || c == SIN_IP || c == SIN_RED || c == SIN_TARJETA)
			return 2;
		return c == TLS_CERTIFICADO || c == TLS_SIN_HORA || c == SIN_TLS || c == NO_RESUELVE || ningun_bueno ? 3 : 2;
	}
	r = envia(u, c, (const u8 *)cab_http, (u32)m);
	if (!r)
		r = envia(u, c, (const u8 *)cuerpo, nc);
	free(cuerpo);
	if (!r)
		r = recibe(u, c, resp, sizeof resp, &nr);
	cierra(u, c);
	if (r) {
		fallo_red = r;
		snprintf(por_que, (size_t)cabe, "%s", texto_red(r));
		/* (TERMO 1.2) tampoco se reintenta con el primer archivo si se corta
		 * antes de la respuesta (TLS.BIN puede dar el fallo del apretón de
		 * manos al enviar): el otro servidor ya */
		return ningun_bueno ? 3 : 2;
	}
	if (!strncmp(resp, "HTTP/1.", 7))
		fallo_http = (resp[9] - '0') * 100 + (resp[10] - '0') * 10 + (resp[11] - '0');
	/* «HTTP/1.1 200 OK» ... y, detrás de la línea en blanco, «OK ...» o «ERROR ...» */
	p = busca_en(resp, "\r\n\r\n");
	p = p ? p + 4 : resp;
	{
		char linea[200];
		int i = 0;

		while (p[i] && p[i] != '\r' && p[i] != '\n' && i < (int)sizeof linea - 1) {
			linea[i] = p[i];
			i++;
		}
		linea[i] = 0;
		if (!strncmp(resp, "HTTP/1.", 7) && !strncmp(resp + 9, "200", 3) && !strncmp(linea, "OK", 2))
			return 0;
		if (!strncmp(linea, "ERROR", 5)) {
			snprintf(por_que, (size_t)cabe, "el servidor dice: %s", linea + 6);
			/* (500: algo suyo, que puede ir luego; lo demás, no se repite) */
			return !strncmp(resp + 9, "500", 3) ? 2 : 1;
		}
		snprintf(por_que, (size_t)cabe, "respuesta rara del servidor: %.60s", resp);
		/* (TERMO 1.2.1) un 403, 429, 413, 415 o 400 sin «ERROR ...» (el de
		 * nginx, delante): tampoco se repite */
		return fallo_http == 403 || fallo_http == 429 || fallo_http == 413 || fallo_http == 415 || fallo_http == 400 ? 1 : 2;
	}
}

/* ---- ENVIO.INI ---- */
static int lee_ini(void)
{
	char r[80], *b = 0, *p;
	u32 n;

	snprintf(r, sizeof r, "%s\\ENVIO.INI", carpeta);
	if (!lee_archivo(r, (u8 **)&b, &n))
		return 0;
	b[n] = 0;
	for (p = b; *p;) {
		char *f = p, *v;

		while (*f && *f != '\r' && *f != '\n')
			f++;
		if (*f)
			*f++ = 0;
		v = busca_en(p, "=");
		if (v && *p != ';' && *p != '#') {
			*v++ = 0;
			if (!strcmp(p, "servidor") && n_servidores < 2)
				snprintf(servidores[n_servidores++], sizeof servidores[0], "%s", v);
			else if (!strcmp(p, "equipo"))
				snprintf(equipo, sizeof equipo, "%s", v);
			else if (!strcmp(p, "testigo"))
				snprintf(testigo, sizeof testigo, "%s", v);
			else if (!strcmp(p, "prueba"))
				prueba = !strcmp(v, "si");
		}
		while (*f == '\r' || *f == '\n')
			f++;
		p = f;
	}
	free(b);
	return n_servidores > 0 && strlen(testigo) == 64;
}

int envio_hay(const char *carpeta_termo)
{
	char r[80];
	u32 m;

	snprintf(r, sizeof r, "%s\\ENVIO.INI", carpeta_termo);
	m = sc(SC_ABRIR, (u32)r, PARA_LEER, 0);
	if (ES_ERROR(m))
		return 0;
	sc(SC_CERRAR, m, 0, 0);
	return 1;
}

static int extension_vale(const char *n)
{
	const char *p = busca_en(n, ".");

	return p && (!strcmp(p, ".CSV") || !strcmp(p, ".LOG") || !strcmp(p, ".TXT") || !strcmp(p, ".BMP"));
}

/* (TERMO 1.2.1) lo que ha pasado con cada servidor */
static char motivo_sv[2][300];
static u32 fallo_sv[2];
static int http_sv[2], muerto[2];

/* «en hispados.es: ...; en actualiza.hispados.es: ...» (los probados), y
 * fallo_red/fallo_http, los del primero que haya fallado por algo que no sea
 * el certificado (lo que dice qué hacer) */
static void junta_motivos(char *d, int cabe, const struct url *us)
{
	int k, m = 0, cual = -1;

	for (k = 0; k < 2; k++)
		if (motivo_sv[k][0]) {
			m += snprintf(d + m, (size_t)(cabe > m ? cabe - m : 0), "%sen %s: %s", m ? "; " : "", us[k].host, motivo_sv[k]);
			if (cual < 0 || (fallo_sv[cual] == TLS_CERTIFICADO && fallo_sv[k] != TLS_CERTIFICADO))
				cual = k;
		}
	if (cual >= 0) {
		fallo_red = fallo_sv[cual];
		fallo_http = http_sv[cual];
	}
	if (m >= cabe)
		d[cabe - 1] = 0;
}

int envio_principal(const char *carpeta_termo, char unidad, int en_consola)
{
	static char nombres[MAX_ARCHIVOS][13], rutas[MAX_ARCHIVOS][80];
	struct entrada e;
	struct url us[2];
	char lote[24], por_que[300];
	u32 m, a, me, d, h, mi, s, f, ho, hn;
	int nn = 0, k, buenos = 0, malos = 0, iguales = 0, i, sv = 0, no_leidos = 0;
	char no_leido[16] = "", nl[80] = "";
	u8 az[4];

	consola = en_consola;
	snprintf(carpeta, sizeof carpeta, "%s", carpeta_termo);
	snprintf(ruta_est, sizeof ruta_est, "%s\\ENVIO.EST", carpeta);
	snprintf(ruta_hec, sizeof ruta_hec, "%s\\ENVIO.HEC", carpeta);
	if (!lee_ini()) {
		di("NO ENVIADO: esta copia del Termómetro no está preparada para enviar (falta %s\\ENVIO.INI con el servidor y el testigo). Súbalo al FTP: los archivos siguen en %s.", carpeta, carpeta);
		return 1;
	}
	/* (TERMO 1.2) hispados.es antes que actualiza.hispados.es (sin HTTPS) */
	if (n_servidores == 2 && !strncmp(servidores[0], "https://actualiza.", 18) && strncmp(servidores[1], "https://actualiza.", 18)) {
		char x[200];

		memcpy(x, servidores[0], sizeof x);
		memcpy(servidores[0], servidores[1], sizeof x);
		memcpy(servidores[1], x, sizeof x);
	}
	for (k = 0; k < n_servidores; k++)
		if (!parte_url(servidores[k], &us[k]) || (!us[k].segura && !prueba)) {
			di("NO ENVIADO: el servidor «%s» de ENVIO.INI no vale (tiene que ser https://). Súbalo al FTP: los archivos siguen en %s.", servidores[k], carpeta);
			return 1;
		}
	if (!hay_red()) {
		u32 ip, e = sc(SC_ESTADO_RED, (u32)&ip, 0, 0);

		if (e == 3) {	/* (TERMO 1.2.1) con enlace, pidiendo la IP: no «no hay cable» */
			di("NO ENVIADO: sin red todavía: hay cable (o Wi-Fi o el móvil), pero aún no hay dirección IP (se está pidiendo por DHCP). Espere unos segundos y pulse E otra vez; si no llega, mire el router (o desenchufe y enchufe el cable). O súbalo al FTP: los archivos siguen en %s.", carpeta);
			return 2;
		}
		di("NO ENVIADO: sin red (%s). Enchufe el cable de red, conecte el Wi-Fi o el móvil por USB (enchúfelo y active el Anclaje de USB: en Android, Ajustes › Conexión y compartir › Anclaje de USB) y pulse E otra vez; o súbalo al FTP: los archivos siguen en %s.",
		   ES_ERROR_RED(e) ? texto_red(e) : "no hay cable ni Wi-Fi conectado, o todavía no hay dirección IP", carpeta);
		return 2;
	}
	/* lo ya mandado */
	{
		u8 *b;
		u32 n;

		hecho[0] = 0;
		if (lee_archivo(ruta_hec, &b, &n)) {
			if (n >= sizeof hecho)
				n = sizeof hecho - 1;
			memcpy(hecho, b, n);
			hecho[n] = 0;
			free(b);
		}
	}
	/* la lista: lo de TERMO y, de la raíz, lo del arranque */
	m = sc(SC_ABRIR, (u32)carpeta, PARA_LEER, 0);
	if (!ES_ERROR(m)) {
		while (nn < MAX_ARCHIVOS - 2 && sc(SC_ENTRADA, m, (u32)&e, 0) == 1) {
			char nb[14];

			memcpy(nb, e.nombre, 13);
			nb[13] = 0;
			if ((e.atributos & 0x10) || !extension_vale(nb) || e.tamano == 0)
				continue;
			snprintf(nombres[nn], sizeof nombres[0], "%s", nb);
			snprintf(rutas[nn], sizeof rutas[0], "%s\\%s", carpeta, nb);
			nn++;
		}
		sc(SC_CERRAR, m, 0, 0);
	}
	if (unidad) {
		static const char *const raiz[] = { "ARRANQUE.TXT", "ACPI.TXT" };

		for (k = 0; k < 2; k++) {
			snprintf(rutas[nn], sizeof rutas[0], "%c:\\%s", unidad, raiz[k]);
			snprintf(nombres[nn], sizeof nombres[0], "%s", raiz[k]);
			m = sc(SC_ABRIR, (u32)rutas[nn], PARA_LEER, 0);
			if (!ES_ERROR(m)) {
				sc(SC_CERRAR, m, 0, 0);
				nn++;
			}
		}
	}
	if (!nn) {
		di("No hay nada que enviar en %s.", carpeta);
		return 1;
	}
	f = sc(SC_DATO, DATO_FECHA, 0, 0);
	ho = f & 0xFFFF;
	hn = f >> 16;
	a = 1980 + (hn >> 9), me = (hn >> 5) & 15, d = hn & 31, h = ho >> 11, mi = (ho >> 5) & 63, s = (ho & 31) * 2;
	{
		u64 t = rdtsc();

		az[0] = (u8)t, az[1] = (u8)(t >> 8), az[2] = (u8)(t >> 16), az[3] = (u8)(t >> 24);
	}
	snprintf(lote, sizeof lote, "%04u%02u%02u-%02u%02u%02u-%02X%02X", a, me, d, h, mi, s, az[0] ^ az[2], az[1] ^ az[3]);
	di("Enviando... (%d archivos a %s, lote %s)", nn, us[0].host, lote);
	for (k = 0; k < nn; k++) {
		u8 *b;
		u32 n;
		int r = 2;

		if (!lee_archivo(rutas[k], &b, &n)) {
			di("%s: no se puede leer (o pasa de 4 MB): no se manda.", nombres[k]);
			no_leidos++;	/* (TERMO 1.2) aparte: no es culpa de la red ni del servidor */
			snprintf(no_leido, sizeof no_leido, "%s", nombres[k]);
			continue;
		}
		if (ya_mandado(nombres[k], n)) {
			free(b);
			iguales++;
			continue;
		}
		{
			/* (TERMO 1.2.1) cada servidor, con su motivo: el ASUS GL553VD
			 * (26/11/2026) dijo «el certificado es de otro sitio, no de
			 * actualiza.hispados.es» y no lo que había pasado con
			 * hispados.es, el primero. Un servidor «muerto» (el certificado,
			 * la hora, sin TLS.BIN: con él no hay manera) no se vuelve a
			 * probar; si el otro falla por otra cosa (no contesta, se corta),
			 * se vuelve al primero: hasta 4 intentos con cada uno */
			int vez = 0, intentos[2] = { 0, 0 };

			for (i = 0; i < 4 * n_servidores && r == 2; i++) {
				static const u32 espera[4] = { 0, 2, 5, 10 };

				if (muerto[sv] || (n_servidores > 1 && intentos[sv] >= 4)) {
					if (n_servidores > 1 && !muerto[1 - sv] && intentos[1 - sv] < 4)
						sv = 1 - sv, vez = 0;
					else
						break;
				}
				if (vez % 4)
					sc(SC_DORMIR, espera[vez % 4] * jg_hz, 0, 0);
				di("Enviando... (archivo %d de %d: %s, %u octetos, a %s)%s%s", k + 1, nn, nombres[k], n, us[sv].host, vez ? ", otra vez: antes, " : "", vez ? por_que : "");
				r = manda(&us[sv], lote, nombres[k], b, n, por_que, sizeof por_que);
				intentos[sv]++;
				vez++;
				if (r != 0) {
					snprintf(motivo_sv[sv], sizeof motivo_sv[0], "%s", por_que);
					fallo_sv[sv] = fallo_red;
					http_sv[sv] = fallo_http;
				}
				if (r == 3) {
					if (fallo_red == TLS_CERTIFICADO || fallo_red == TLS_SIN_HORA || fallo_red == SIN_TLS)
						muerto[sv] = 1;
					r = 2;
					if (n_servidores > 1 && !muerto[1 - sv] && intentos[1 - sv] < 4)
						sv = 1 - sv, vez = 0;
					else if (muerto[sv])
						r = 3;
				} else if (r == 2 && n_servidores > 1 && vez % 2 == 0 && !muerto[1 - sv] && intentos[1 - sv] < 4)
					sv = 1 - sv, vez = 0;	/* (dos veces mal: el otro, y luego otra vez este) */
			}
			if (r == 2 && n_servidores > 1 && (muerto[0] || intentos[0] >= 4) && (muerto[1] || intentos[1] >= 4))
				r = 3;
			if (r >= 2 && n_servidores > 1)	/* (lo que dice un servidor que contesta, tal cual) */
				junta_motivos(por_que, sizeof por_que, us);
			if (r == 3) {
				/* (con ningún servidor: lo demás tampoco va a ir) */
				free(b);
				malos++;
				di("%s: no se ha podido enviar: %s.", nombres[k], por_que);
				break;
			}
		}
		free(b);
		if (r == 0) {
			buenos++;
			ningun_bueno = 0;
			apunta_mandado(nombres[k], n);
		} else {
			malos++;
			di("%s: no se ha podido enviar: %s.", nombres[k], por_que);
			if (r == 1 && (busca_en(por_que, "testigo") || fallo_http == 429 || fallo_http == 403))
				break;	/* (sin testigo, lo demás tampoco va a ir) */
			if (r == 2 && !hay_red()) {
				di("NO ENVIADO: se ha ido la red a mitad (enviados %d de %d). Enchufe el cable de red, conecte el Wi-Fi o el móvil por USB (enchúfelo y active el Anclaje de USB: en Android, Ajustes › Conexión y compartir › Anclaje de USB) y pulse E otra vez (solo irá lo que falta); o súbalo al FTP: los archivos siguen en %s.",
				   buenos, nn - iguales, carpeta);
				return 2;
			}
		}
	}
	if (no_leidos)
		snprintf(nl, sizeof nl, "; %d no se %s podido leer (%s%s)", no_leidos, no_leidos == 1 ? "ha" : "han", no_leido, no_leidos > 1 ? " y otros" : "");
	if (malos) {
		char mot[600];

		motivo_llano(mot, sizeof mot, por_que);
		if (buenos)
			di("NO ENVIADO del todo: %d de %d archivos enviados a %s (lote %s); %d sin enviar: %s Súbalos al FTP si no va: los archivos siguen en %s.", buenos, nn - iguales,
			   us[sv].host, lote, malos, mot, carpeta);
		else
			di("NO ENVIADO: %s Si no hay manera, súbalo al FTP: los archivos siguen en %s.", mot, carpeta);
	} else if (buenos)
		di("ENVIADO: %d archivos a %s (lote %s)%s%s.", buenos, us[sv].host, lote, iguales ? (snprintf(por_que, sizeof por_que, "; otros %d ya estaban", iguales), por_que) : "", nl);
	else if (iguales)
		di("ENVIADO: no había nada nuevo (los %d archivos ya estaban en el servidor)%s.", iguales, nl);
	else
		di("NO ENVIADO: no se ha podido leer ningún archivo de %s%s. Súbalos al FTP: siguen en %s.", carpeta, nl, carpeta);
	return malos ? 3 : 0;
}
