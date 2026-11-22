/* envio.c - (TERMO) «Enviar a Carlos»: sube por HTTPS la carpeta TERMO (los
 * .CSV, .LOG, .TXT y .BMP, y el \ARRANQUE.TXT y el \ACPI.TXT del arranque)
 * al receptor del servidor (tools/termo-subida/subir.php), para verlos sin
 * tener que sacar la memoria del portátil. Lo hace TERMO.BIN /ENVIAR: desde
 * la Consola (lo dice en la pantalla) o lanzado por el botón del Termómetro
 * (/ENVIAR /VENTANA: lo va dejando en TERMO\ENVIO.EST, que la ventana lee).
 *
 * TERMO\ENVIO.INI (lo hace usb.sh con TERMO=1; nunca se sube):
 *   servidor=https://actualiza.hispados.es/termo/subir.php   (uno o dos)
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

/* ---- lo que se dice: a la Consola, o a ENVIO.EST para la ventana ---- */
static void di(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void di(const char *fmt, ...)
{
	char t[400], u[420];
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

static const char *como_conectar =
	"Enchufe el cable de red, o conecte el Wi-Fi: Inicio > Configuración > Red (elija la red y escriba su contraseña), "
	"o en la Consola: REDWIFI /REDES y REDWIFI /CONECTAR nombre. Luego, otra vez «Enviar a Carlos».";

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
	if (!*b || ES_ERROR(r) || r != l) {
		free(*b);
		*b = 0;
		return 0;
	}
	*n = l;
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
	if (ES_ERROR_RED(c)) {
		free(cuerpo);
		return c == TLS_CERTIFICADO || c == TLS_SIN_HORA || c == SIN_TLS || c == NO_RESUELVE ? 3 : 2;
	}
	r = envia(u, c, (const u8 *)cab_http, (u32)m);
	if (!r)
		r = envia(u, c, (const u8 *)cuerpo, nc);
	free(cuerpo);
	if (!r)
		r = recibe(u, c, resp, sizeof resp, &nr);
	cierra(u, c);
	if (r) {
		snprintf(por_que, (size_t)cabe, "%s", texto_red(r));
		return 2;
	}
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
		return 2;
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

int envio_principal(const char *carpeta_termo, char unidad, int en_consola)
{
	static char nombres[MAX_ARCHIVOS][13], rutas[MAX_ARCHIVOS][80];
	struct entrada e;
	struct url us[2];
	char lote[24], por_que[300];
	u32 m, a, me, d, h, mi, s, f, ho, hn;
	int nn = 0, k, buenos = 0, malos = 0, iguales = 0, i, sv = 0;
	u8 az[4];

	consola = en_consola;
	snprintf(carpeta, sizeof carpeta, "%s", carpeta_termo);
	snprintf(ruta_est, sizeof ruta_est, "%s\\ENVIO.EST", carpeta);
	snprintf(ruta_hec, sizeof ruta_hec, "%s\\ENVIO.HEC", carpeta);
	if (!lee_ini()) {
		di("Esta copia del Termómetro no está preparada para enviar (falta %s\\ENVIO.INI con el servidor y el testigo).", carpeta);
		return 1;
	}
	for (k = 0; k < n_servidores; k++)
		if (!parte_url(servidores[k], &us[k]) || (!us[k].segura && !prueba)) {
			di("El servidor «%s» no vale: tiene que ser https://.", servidores[k]);
			return 1;
		}
	if (!hay_red()) {
		di("No hay red. %s", como_conectar);
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
	di("Enviando %d archivos a %s (lote %s)...", nn, us[0].host, lote);
	for (k = 0; k < nn; k++) {
		u8 *b;
		u32 n;
		int r = 2;

		if (!lee_archivo(rutas[k], &b, &n)) {
			di("%s: no se puede leer (o pasa de 4 MB): no se manda.", nombres[k]);
			malos++;
			continue;
		}
		if (ya_mandado(nombres[k], n)) {
			free(b);
			iguales++;
			continue;
		}
		{
			int vez = 0, probados = 1;

			for (i = 0; i < 4 * n_servidores && r == 2; i++, vez++) {
				static const u32 espera[4] = { 0, 2, 5, 10 };

				if (vez % 4)
					sc(SC_DORMIR, espera[vez % 4] * jg_hz, 0, 0);
				di("Enviando %s (%d de %d, %u octetos)%s...", nombres[k], k + 1, nn, n, vez ? ", otra vez" : "");
				r = manda(&us[sv], lote, nombres[k], b, n, por_que, sizeof por_que);
				/* (cuatro veces mal, o con este no hay manera: el otro servidor) */
				if ((r == 3 || (r == 2 && vez % 4 == 3)) && n_servidores > 1 && probados < 2) {
					sv = 1 - sv;
					probados++;
					vez = -1;
					r = 2;
				}
			}
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
			apunta_mandado(nombres[k], n);
		} else {
			malos++;
			di("%s: no se ha podido enviar: %s.", nombres[k], por_que);
			if (r == 1 && busca_en(por_que, "testigo"))
				break;	/* (sin testigo, lo demás tampoco va a ir) */
			if (r == 2 && !hay_red()) {
				di("Se ha ido la red. %s", como_conectar);
				return 2;
			}
		}
	}
	if (malos)
		di("Enviados %d de %d archivos (%d ya estaban); %d sin enviar: %s. Pruebe otra vez.", buenos, nn - iguales, iguales, malos, por_que);
	else if (buenos)
		di("Hecho: %d archivos enviados a Carlos%s (lote %s).", buenos, iguales ? " (los demás ya estaban)" : "", lote);
	else
		di("No hay nada nuevo: los %d archivos ya estaban enviados.", iguales);
	return malos ? 3 : 0;
}
