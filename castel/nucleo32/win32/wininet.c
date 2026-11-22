/* wininet.c - WIN32.BIN (HL): WININET.DLL, lo que piden los juegos de
 * 1998-2000 para buscar servidores, ver las noticias o bajar una
 * actualización: HTTP de verdad (HTTP/1.1 con «Connection: close», con
 * Content-Length, en trozos o hasta que se cierre; las redirecciones) sobre
 * el Winsock de WIN32.BIN (red.c, el servicio de red). HTTPS y FTP aún no:
 * dicen que no se puede conectar, limpio. Sin red, «sin conexión»
 * (InternetGetConnectedState da FALSE) y cada orden su error de WinINet.
 * Hecho con la documentación pública de WinINet.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "w32.h"
#define API(t) t WINAPI

/* los errores de WinINet */
#define E_TIEMPO 12002
#define E_URL 12005
#define E_ESQUEMA 12006
#define E_NOMBRE 12007
#define E_TIPO 12018
#define E_CONECTAR 12029
#define E_CORTADA 12030
#define E_CABECERA 12150
#define E_SIN_RED 12163
#define E_CORTO 122

/* las de Winsock (red.c) */
typedef struct { i16 fam; u16 puerto; u32 ip; u8 cero[8]; } SIN;
u32 WINAPI socket(i32, i32, i32);
i32 WINAPI connect(u32, const SIN *, i32);
i32 WINAPI send(u32, const char *, i32, i32);
i32 WINAPI recv(u32, char *, i32, i32);
i32 WINAPI closesocket(u32);
i32 WINAPI WSAGetLastError(void);
u32 WINAPI inet_addr(const char *);
void *WINAPI gethostbyname(const char *);
i32 WINAPI HispaRedConfig(u8 *c, u8 *mac);
void diag_apunta(const char *f, ...);
static const char *busca_txt(const char *s, const char *t)
{
	u32 l = strlen(t);
	for (; *s; s++)
		if (!strncmp(s, t, l))
			return s;
	return 0;
}

enum { H_LIBRE, H_SESION, H_CONEXION, H_PEDIDO };
#define MAX_H 16
#define BASE_H 0x00CC0001u
typedef struct {
	int tipo;
	char host[96];
	u16 puerto;
	int https;
	char verbo[12], camino[256];
	char *cab_mas;           /* las cabeceras que añade el programa */
	u32 s;                   /* el enchufe (0xFFFFFFFF: ninguno) */
	char cab[2048];          /* las cabeceras de la respuesta (sin la línea vacía) */
	u32 estado;
	u8 bufer[2048];
	u32 b_ini, b_fin;
	int trozos;              /* Transfer-Encoding: chunked */
	u32 queda;               /* lo que queda del cuerpo (o del trozo); 0xFFFFFFFF: hasta que se cierre */
	int acabado;
	u32 flags;
} HInet;
static HInet hs[MAX_H];
static char ultima_resp[256];

static HInet *hi(HANDLE h, int tipo)
{
	u32 k = (u32)h - BASE_H;
	if (k >= MAX_H || hs[k].tipo == H_LIBRE || (tipo && hs[k].tipo != tipo)) {
		pon_error(k < MAX_H && hs[k].tipo ? E_TIPO : 6);
		return 0;
	}
	return &hs[k];
}
static HANDLE nuevo(int tipo)
{
	u32 k;
	for (k = 0; k < MAX_H; k++)
		if (!hs[k].tipo) {
			memset(&hs[k], 0, sizeof hs[k]);
			hs[k].tipo = tipo;
			hs[k].s = 0xFFFFFFFF;
			return (HANDLE)(BASE_H + k);
		}
	pon_error(8);
	return 0;
}
static int hay_red(void) { u8 c[16]; return HispaRedConfig(c, 0) != 0; }

API(HANDLE) InternetOpenA(const char *agente, u32 acceso, const char *proxy, const char *noproxy, u32 fl)
{
	HANDLE h = nuevo(H_SESION);
	(void)agente; (void)acceso; (void)proxy; (void)noproxy;
	if (h)
		hi(h, 0)->flags = fl;
	traza("InternetOpenA(%s) = %x", agente ? agente : "", (u32)h);
	return h;
}
API(HANDLE) InternetConnectA(HANDLE ses, const char *host, u16 puerto, const char *u, const char *c, u32 serv, u32 fl, u32 ctx)
{
	HInet *x;
	HANDLE h;
	(void)u; (void)c; (void)ctx;
	if (!hi(ses, H_SESION))
		return 0;
	if (serv != 3) {             /* INTERNET_SERVICE_HTTP; FTP y gopher, no */
		pon_error(E_CONECTAR);
		return 0;
	}
	h = nuevo(H_CONEXION);
	if (!h)
		return 0;
	x = hi(h, 0);
	copia_n(x->host, host ? host : "", sizeof x->host);
	x->https = puerto == 443 || (fl & 0x00800000);    /* INTERNET_FLAG_SECURE */
	x->puerto = puerto ? puerto : x->https ? 443 : 80;
	x->flags = fl;
	return h;
}
API(HANDLE) HttpOpenRequestA(HANDLE con, const char *verbo, const char *obj, const char *ver, const char *ref, const char **tipos, u32 fl, u32 ctx)
{
	HInet *c = hi(con, H_CONEXION), *x;
	HANDLE h;
	(void)ver; (void)ref; (void)tipos; (void)ctx;
	if (!c)
		return 0;
	h = nuevo(H_PEDIDO);
	if (!h)
		return 0;
	x = hi(h, 0);
	c = hi(con, 0);
	copia_n(x->host, c->host, sizeof x->host);
	x->puerto = c->puerto;
	x->https = c->https || (fl & 0x00800000);
	copia_n(x->verbo, verbo && verbo[0] ? verbo : "GET", sizeof x->verbo);
	copia_n(x->camino, obj && obj[0] ? obj : "/", sizeof x->camino);
	x->flags = fl;
	return h;
}
API(BOOL) HttpAddRequestHeadersA(HANDLE h, const char *c, i32 n, u32 mod)
{
	HInet *x = hi(h, H_PEDIDO);
	u32 l, a;
	char *p;
	(void)mod;
	if (!x || !c)
		return FALSE;
	l = n < 0 ? strlen(c) : (u32)n;
	a = x->cab_mas ? strlen(x->cab_mas) : 0;
	p = m_pedir(a + l + 3);
	if (!p)
		return FALSE;
	if (a)
		memcpy(p, x->cab_mas, a);
	memcpy(p + a, c, l);
	p[a + l] = 0;
	if (a + l < 2 || p[a + l - 1] != '\n')
		strcat(p, "\r\n");
	if (x->cab_mas)
		m_soltar(x->cab_mas);
	x->cab_mas = p;
	return TRUE;
}

/* ---- leer de la conexión ---- */
static int llena(HInet *x)
{
	i32 r;
	if (x->b_ini < x->b_fin)
		return 1;
	if (x->s == 0xFFFFFFFF)
		return 0;
	r = recv(x->s, (char *)x->bufer, sizeof x->bufer, 0);
	if (r <= 0)
		return 0;
	x->b_ini = 0;
	x->b_fin = (u32)r;
	return 1;
}
static int linea(HInet *x, char *d, u32 cabe)
{
	u32 k = 0;
	for (;;) {
		u8 c;
		if (!llena(x))
			break;
		c = x->bufer[x->b_ini++];
		if (c == '\n') {
			d[k] = 0;
			if (k && d[k - 1] == '\r')
				d[--k] = 0;
			return 1;
		}
		if (k + 1 < cabe)
			d[k++] = (char)c;
	}
	d[k] = 0;
	return k > 0;
}
/* el valor de una cabecera de la respuesta (sin mirar mayúsculas) */
static const char *cabecera(HInet *x, const char *n, char *d, u32 cabe)
{
	const char *p = x->cab;
	u32 l = strlen(n);
	while (*p) {
		const char *fin = strchr(p, '\n');
		if (!fin)
			fin = p + strlen(p);
		if (!strnicmp_(p, n, l) && p[l] == ':') {
			const char *v = p + l + 1;
			u32 k = 0;
			while (*v == ' ')
				v++;
			while (v < fin && *v != '\r' && k + 1 < cabe)
				d[k++] = *v++;
			d[k] = 0;
			return d;
		}
		p = *fin ? fin + 1 : fin;
	}
	return 0;
}
static u32 numero(const char *s, int hex)
{
	u32 v = 0;
	for (;; s++) {
		int c = *s;
		if (c >= '0' && c <= '9')
			v = v * (hex ? 16 : 10) + (u32)(c - '0');
		else if (hex && c >= 'a' && c <= 'f')
			v = v * 16 + (u32)(c - 'a' + 10);
		else if (hex && c >= 'A' && c <= 'F')
			v = v * 16 + (u32)(c - 'A' + 10);
		else
			return v;
	}
}
static void cierra_enchufe(HInet *x)
{
	if (x->s != 0xFFFFFFFF)
		closesocket(x->s);
	x->s = 0xFFFFFFFF;
}

/* manda el pedido y lee las cabeceras de la respuesta */
static BOOL pedir(HInet *x, const char *extra, u32 nextra, const void *cuerpo, u32 ncuerpo)
{
	SIN a;
	u32 ip;
	char *p, l[512], v[32];
	u32 n = 0;
	cierra_enchufe(x);
	x->b_ini = x->b_fin = 0;
	x->cab[0] = 0;
	x->acabado = 0;
	x->trozos = 0;
	if (x->https) {
		pon_error(E_CONECTAR);       /* (HTTPS: aún no) */
		diag_apunta("WININET: https://%s%s: HTTPS aún no (no se puede conectar)", x->host, x->camino);
		return FALSE;
	}
	if (!hay_red()) {
		pon_error(E_SIN_RED);
		return FALSE;
	}
	ip = inet_addr(x->host);
	if (ip == 0xFFFFFFFF) {
		struct { char *n; char **a; i16 t, l; char **lista; } *he = gethostbyname(x->host);
		if (!he) {
			pon_error(E_NOMBRE);
			return FALSE;
		}
		memcpy(&ip, he->lista[0], 4);
	}
	x->s = socket(2, 1, 6);
	if (x->s == 0xFFFFFFFF) {
		pon_error(E_CONECTAR);
		return FALSE;
	}
	memset(&a, 0, sizeof a);
	a.fam = 2;
	a.puerto = (u16)((x->puerto >> 8) | (x->puerto << 8));
	a.ip = ip;
	if (connect(x->s, &a, sizeof a)) {
		cierra_enchufe(x);
		pon_error(E_CONECTAR);
		return FALSE;
	}
	p = m_pedir(1024 + (x->cab_mas ? strlen(x->cab_mas) : 0) + nextra);
	if (!p)
		return FALSE;
	fmt(p, 1024, "%s %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: Hispa-Pentium WinINet\r\nConnection: close\r\n", x->verbo, x->camino, x->host);
	if (ncuerpo) {
		fmt(l, sizeof l, "Content-Length: %u\r\n", ncuerpo);
		strcat(p, l);
	}
	if (x->cab_mas)
		strcat(p, x->cab_mas);
	if (extra && nextra) {
		u32 q = strlen(p);
		memcpy(p + q, extra, nextra);
		p[q + nextra] = 0;
		if (p[q + nextra - 1] != '\n')
			strcat(p, "\r\n");
	}
	strcat(p, "\r\n");
	if (send(x->s, p, strlen(p), 0) < 0 || (ncuerpo && send(x->s, cuerpo, ncuerpo, 0) < 0)) {
		m_soltar(p);
		cierra_enchufe(x);
		pon_error(E_CORTADA);
		return FALSE;
	}
	m_soltar(p);
	/* la respuesta: «HTTP/1.1 200 OK» y las cabeceras */
	if (!linea(x, l, sizeof l) || strncmp(l, "HTTP/", 5)) {
		cierra_enchufe(x);
		pon_error(E_CORTADA);
		return FALSE;
	}
	copia_n(x->cab, l, sizeof x->cab - 4);
	strcat(x->cab, "\r\n");
	n = strlen(x->cab);
	{
		const char *e = strchr(l, ' ');
		x->estado = e ? numero(e + 1, 0) : 0;
	}
	while (linea(x, l, sizeof l) && l[0]) {
		u32 t = strlen(l);
		if (n + t + 3 < sizeof x->cab) {
			memcpy(x->cab + n, l, t);
			memcpy(x->cab + n + t, "\r\n", 3);
			n += t + 2;
		}
	}
	if (cabecera(x, "Transfer-Encoding", v, sizeof v) && !stricmp_(v, "chunked")) {
		x->trozos = 1;
		x->queda = 0;
	} else if (cabecera(x, "Content-Length", v, sizeof v))
		x->queda = numero(v, 0);
	else
		x->queda = 0xFFFFFFFF;
	if (!strcmp(x->verbo, "HEAD") || x->estado == 204 || x->estado == 304)
		x->queda = 0, x->trozos = 0;
	copia_n(ultima_resp, x->cab, sizeof ultima_resp);
	traza("WININET %s http://%s:%u%s: %u", x->verbo, x->host, x->puerto, x->camino, x->estado);
	pon_error(0);
	return TRUE;
}
API(BOOL) HttpSendRequestA(HANDLE h, const char *cab, u32 ncab, void *op, u32 nop)
{
	HInet *x = hi(h, H_PEDIDO);
	if (!x)
		return FALSE;
	return pedir(x, cab, cab ? (ncab == 0xFFFFFFFF ? strlen(cab) : ncab) : 0, op, nop);
}
API(BOOL) HttpSendRequestExA(HANDLE h, void *a, void *b, u32 f, u32 c) { (void)a; (void)b; (void)f; (void)c; return HttpSendRequestA(h, 0, 0, 0, 0); }
API(BOOL) HttpEndRequestA(HANDLE h, void *a, u32 f, u32 c) { (void)h; (void)a; (void)f; (void)c; return TRUE; }

/* el cuerpo: de «queda» en «queda», o trozo a trozo */
static u32 lee_cuerpo(HInet *x, u8 *d, u32 n)
{
	u32 hecho = 0;
	while (hecho < n && !x->acabado) {
		u32 t;
		if (x->trozos && !x->queda) {
			char l[64];
			if (x->trozos == 2)              /* el «\r\n» del final del trozo de antes */
				linea(x, l, sizeof l);
			if (!linea(x, l, sizeof l)) {
				x->acabado = 1;
				break;
			}
			x->queda = numero(l, 1);
			x->trozos = 2;
			if (!x->queda) {
				while (linea(x, l, sizeof l) && l[0])
					;
				x->acabado = 1;
				break;
			}
		}
		if (!x->trozos && !x->queda) {
			x->acabado = 1;
			break;
		}
		if (!llena(x)) {
			x->acabado = 1;
			break;
		}
		t = x->b_fin - x->b_ini;
		if (t > n - hecho)
			t = n - hecho;
		if (x->queda != 0xFFFFFFFF && t > x->queda)
			t = x->queda;
		memcpy(d + hecho, x->bufer + x->b_ini, t);
		x->b_ini += t;
		hecho += t;
		if (x->queda != 0xFFFFFFFF)
			x->queda -= t;
	}
	if (x->acabado)
		cierra_enchufe(x);
	return hecho;
}
API(BOOL) InternetReadFile(HANDLE h, void *d, u32 n, u32 *leidos)
{
	HInet *x = hi(h, H_PEDIDO);
	if (leidos)
		*leidos = 0;
	if (!x)
		return FALSE;
	if (leidos)
		*leidos = lee_cuerpo(x, d, n);
	return TRUE;
}
API(BOOL) InternetQueryDataAvailable(HANDLE h, u32 *n, u32 f, u32 c)
{
	HInet *x = hi(h, H_PEDIDO);
	(void)f; (void)c;
	if (!x)
		return FALSE;
	if (!x->acabado)
		llena(x);
	*n = x->acabado ? 0 : x->b_fin - x->b_ini;
	if (x->queda != 0xFFFFFFFF && !x->trozos && *n > x->queda)
		*n = x->queda;
	return TRUE;
}
API(BOOL) InternetWriteFile(HANDLE h, const void *d, u32 n, u32 *esc)
{
	HInet *x = hi(h, H_PEDIDO);
	if (esc)
		*esc = 0;
	if (!x || x->s == 0xFFFFFFFF)
		return FALSE;
	if (send(x->s, d, n, 0) < 0) {
		pon_error(E_CORTADA);
		return FALSE;
	}
	if (esc)
		*esc = n;
	return TRUE;
}
API(u32) InternetSetFilePointer(HANDLE h, i32 d, i32 *a, u32 m, u32 c) { (void)h; (void)d; (void)a; (void)m; (void)c; pon_error(12011); return 0xFFFFFFFF; }

API(BOOL) HttpQueryInfoA(HANDLE h, u32 nivel, void *d, u32 *n, u32 *ind)
{
	HInet *x = hi(h, H_PEDIDO);
	char v[512];
	const char *r = 0;
	u32 que = nivel & 0xFFFF, l;
	(void)ind;
	if (!x)
		return FALSE;
	if (!x->cab[0]) {
		pon_error(E_CABECERA);
		return FALSE;
	}
	switch (que) {
	case 19:                     /* HTTP_QUERY_STATUS_CODE */
		fmt(v, sizeof v, "%u", x->estado);
		r = v;
		break;
	case 20: {                   /* HTTP_QUERY_STATUS_TEXT */
		const char *e = strchr(x->cab, ' ');
		u32 k = 0;
		if (e)
			e = strchr(e + 1, ' ');
		if (e)
			for (e++; *e && *e != '\r' && k + 1 < sizeof v; )
				v[k++] = *e++;
		v[k] = 0;
		r = v;
		break;
	}
	case 18: {                   /* HTTP_QUERY_VERSION */
		u32 k = 0;
		while (x->cab[k] && x->cab[k] != ' ' && k + 1 < sizeof v) {
			v[k] = x->cab[k];
			k++;
		}
		v[k] = 0;
		r = v;
		break;
	}
	case 21:                     /* HTTP_QUERY_RAW_HEADERS: con un 0 entre una y otra */
	case 22:                     /* HTTP_QUERY_RAW_HEADERS_CRLF */
		r = x->cab;
		break;
	case 65535:                  /* HTTP_QUERY_CUSTOM: el nombre, en el búfer */
		copia_n(v, d ? (const char *)d : "", sizeof v);
		{
			char w[480];
			r = cabecera(x, v, w, sizeof w);
			if (r) {
				copia_n(v, w, sizeof v);
				r = v;
			}
		}
		break;
	default: {
		static const struct { u16 n; const char *c; } t[] = {
			{ 1, "Content-Type" }, { 5, "Content-Length" }, { 6, "Content-Language" }, { 9, "Date" },
			{ 10, "Expires" }, { 11, "Last-Modified" }, { 33, "Location" }, { 37, "Server" }, { 39, "Accept-Ranges" },
			{ 41, "Connection" }, { 42, "Content-Encoding" }, { 43, "Set-Cookie" }, { 44, "Cache-Control" }, { 49, "ETag" },
			{ 50, "Transfer-Encoding" } };
		u32 k;
		for (k = 0; k < sizeof t / sizeof t[0]; k++)
			if (t[k].n == que)
				r = cabecera(x, t[k].c, v, sizeof v);
	}
	}
	if (!r) {
		pon_error(E_CABECERA);
		return FALSE;
	}
	if (nivel & 0x20000000) {    /* HTTP_QUERY_FLAG_NUMBER */
		if (*n < 4) {
			*n = 4;
			pon_error(E_CORTO);
			return FALSE;
		}
		*(u32 *)d = numero(r, 0);
		*n = 4;
		return TRUE;
	}
	l = strlen(r);
	if (!d || *n < l + 1) {
		*n = l + 1;
		pon_error(E_CORTO);
		return FALSE;
	}
	memcpy(d, r, l + 1);
	if (que == 21) {
		char *p = d;
		for (; *p; p++)
			if (*p == '\r' && p[1] == '\n')
				p[0] = 0, p[1] = 0, p++;
	}
	*n = l;
	return TRUE;
}

/* ---- las URL ---- */
typedef struct {
	u32 tam;
	char *esquema; u32 nesquema; i32 tipo;
	char *host; u32 nhost; u16 puerto;
	char *usuario; u32 nusuario;
	char *clave; u32 nclave;
	char *camino; u32 ncamino;
	char *extra; u32 nextra;
} URLC;
/* «http://host:puerto/camino?extra» -> los trozos (1 bien) */
static int parte_url(const char *u, char *esq, u32 ne, char *host, u32 nh, u16 *puerto, char *cam, u32 nc)
{
	const char *p = busca_txt(u, "://"), *h, *f;
	u32 k;
	if (!p)
		return 0;
	k = (u32)(p - u);
	if (k + 1 > ne)
		return 0;
	memcpy(esq, u, k);
	esq[k] = 0;
	h = p + 3;
	{
		const char *arroba = 0, *q;
		for (q = h; *q && *q != '/'; q++)
			if (*q == '@')
				arroba = q;
		if (arroba)
			h = arroba + 1;
	}
	for (f = h; *f && *f != '/' && *f != ':' && *f != '?'; f++)
		;
	k = (u32)(f - h);
	if (!k || k + 1 > nh)
		return 0;
	memcpy(host, h, k);
	host[k] = 0;
	*puerto = !stricmp_(esq, "https") ? 443 : !stricmp_(esq, "ftp") ? 21 : 80;
	if (*f == ':') {
		*puerto = (u16)numero(f + 1, 0);
		for (f++; *f >= '0' && *f <= '9'; f++)
			;
	}
	copia_n(cam, *f ? f : "/", nc);
	if (cam[0] == '?') {
		char t[256];
		fmt(t, sizeof t, "/%s", cam);
		copia_n(cam, t, nc);
	}
	return 1;
}
static void pon_trozo(char **d, u32 *n, const char *s, u32 l)
{
	if (!*n)
		return;
	if (*d) {
		if (l + 1 > *n) {
			l = *n - 1;
		}
		memcpy(*d, s, l);
		(*d)[l] = 0;
	} else
		*d = (char *)s;      /* (con el largo puesto y sin búfer: un puntero dentro de la URL) */
	*n = l;
}
API(BOOL) InternetCrackUrlA(const char *u, u32 n, u32 f, URLC *c)
{
	char esq[16], host[96], cam[256];
	u16 puerto;
	const char *q;
	(void)n; (void)f;
	if (!u || !c || !parte_url(u, esq, sizeof esq, host, sizeof host, &puerto, cam, sizeof cam)) {
		pon_error(E_URL);
		return FALSE;
	}
	c->tipo = !stricmp_(esq, "http") ? 3 : !stricmp_(esq, "https") ? 4 : !stricmp_(esq, "ftp") ? 1 : !stricmp_(esq, "file") ? 5 : 0;
	c->puerto = puerto;
	pon_trozo(&c->esquema, &c->nesquema, u, strlen(esq));
	q = busca_txt(u, host);
	pon_trozo(&c->host, &c->nhost, q ? q : host, strlen(host));
	q = busca_txt(u + strlen(esq) + 3, "/");
	{
		const char *ex = q ? strchr(q, '?') : 0;
		u32 lc = q ? (ex ? (u32)(ex - q) : strlen(q)) : 0;
		pon_trozo(&c->camino, &c->ncamino, q ? q : "", lc);
		pon_trozo(&c->extra, &c->nextra, ex ? ex : "", ex ? strlen(ex) : 0);
	}
	pon_trozo(&c->usuario, &c->nusuario, "", 0);
	pon_trozo(&c->clave, &c->nclave, "", 0);
	return TRUE;
}
API(BOOL) InternetCanonicalizeUrlA(const char *u, char *d, u32 *n, u32 f)
{
	u32 l = strlen(u);
	(void)f;
	if (*n < l + 1) {
		*n = l + 1;
		pon_error(E_CORTO);
		return FALSE;
	}
	memcpy(d, u, l + 1);
	*n = l;
	return TRUE;
}
API(BOOL) InternetCombineUrlA(const char *b, const char *r, char *d, u32 *n, u32 f)
{
	char t[512];
	(void)f;
	if (busca_txt(r, "://"))
		copia_n(t, r, sizeof t);
	else if (r[0] == '/') {
		const char *p = busca_txt(b, "://");
		const char *q = p ? strchr(p + 3, '/') : 0;
		u32 k = q ? (u32)(q - b) : strlen(b);
		if (k > 400)
			k = 400;
		memcpy(t, b, k);
		t[k] = 0;
		strcat(t, r);
	} else {
		const char *q = strrchr(b, '/');
		u32 k = q ? (u32)(q - b + 1) : strlen(b);
		if (k > 400)
			k = 400;
		memcpy(t, b, k);
		t[k] = 0;
		copia_n(t + k, r, sizeof t - k);
	}
	return InternetCanonicalizeUrlA(t, d, n, 0);
}

/* InternetOpenUrlA: la conexión y el pedido de una vez (con las redirecciones) */
API(HANDLE) InternetOpenUrlA(HANDLE ses, const char *url, const char *cab, u32 ncab, u32 fl, u32 ctx)
{
	char esq[16], host[96], cam[256], u[400];
	u16 puerto;
	int vueltas;
	HANDLE h;
	HInet *x;
	(void)ctx;
	if (!hi(ses, H_SESION))
		return 0;
	copia_n(u, url ? url : "", sizeof u);
	h = nuevo(H_PEDIDO);
	if (!h)
		return 0;
	x = hi(h, 0);
	for (vueltas = 0; vueltas < 6; vueltas++) {
		char loc[400];
		if (!parte_url(u, esq, sizeof esq, host, sizeof host, &puerto, cam, sizeof cam)) {
			x->tipo = 0;
			pon_error(E_URL);
			return 0;
		}
		if (stricmp_(esq, "http") && stricmp_(esq, "https")) {
			x->tipo = 0;
			pon_error(stricmp_(esq, "ftp") ? E_ESQUEMA : E_CONECTAR);
			return 0;
		}
		copia_n(x->host, host, sizeof x->host);
		x->puerto = puerto;
		x->https = !stricmp_(esq, "https");
		strcpy(x->verbo, "GET");
		copia_n(x->camino, cam, sizeof x->camino);
		if (!pedir(x, cab, cab ? (ncab == 0xFFFFFFFF ? strlen(cab) : ncab) : 0, 0, 0)) {
			u32 e = GetLastError_();
			x->tipo = 0;
			pon_error(e);
			return 0;
		}
		if ((fl & 0x00200000) || (x->estado != 301 && x->estado != 302 && x->estado != 303 && x->estado != 307)
		    || !cabecera(x, "Location", loc, sizeof loc))
			return h;
		{
			char nueva[400];
			u32 nn = sizeof nueva;
			cierra_enchufe(x);
			if (!InternetCombineUrlA(u, loc, nueva, &nn, 0))
				return h;
			copia_n(u, nueva, sizeof u);
		}
	}
	return h;
}

API(BOOL) InternetCloseHandle(HANDLE h)
{
	HInet *x = hi(h, 0);
	if (!x)
		return FALSE;
	cierra_enchufe(x);
	if (x->cab_mas)
		m_soltar(x->cab_mas);
	x->tipo = 0;
	return TRUE;
}

/* ---- el estado de la conexión y lo demás ---- */
API(BOOL) InternetGetConnectedState(u32 *fl, u32 r)
{
	int si = hay_red();
	(void)r;
	if (fl)
		*fl = si ? 2 : 0x20;         /* INTERNET_CONNECTION_LAN; sin red, INTERNET_CONNECTION_OFFLINE */
	return si;
}
API(BOOL) InternetGetConnectedStateExA(u32 *fl, char *n, u32 nn, u32 r)
{
	if (n && nn)
		copia_n(n, "Red de Hispa-Pentium", nn);
	return InternetGetConnectedState(fl, r);
}
API(u32) InternetAttemptConnect(u32 r) { (void)r; return hay_red() ? 0 : E_SIN_RED; }
API(BOOL) InternetCheckConnectionA(const char *u, u32 f, u32 r) { (void)u; (void)f; (void)r; if (hay_red()) return TRUE; pon_error(E_SIN_RED); return FALSE; }
API(BOOL) InternetAutodial(u32 f, HWND h) { (void)f; (void)h; return hay_red(); }
API(BOOL) InternetAutodialHangup(u32 r) { (void)r; return TRUE; }
API(BOOL) InternetSetOptionA(HANDLE h, u32 o, void *b, u32 n) { (void)h; (void)o; (void)b; (void)n; return TRUE; }
API(BOOL) InternetSetOptionExA(HANDLE h, u32 o, void *b, u32 n, u32 f) { (void)f; return InternetSetOptionA(h, o, b, n); }
API(BOOL) InternetQueryOptionA(HANDLE h, u32 o, void *b, u32 *n)
{
	(void)h;
	if ((o == 2 || o == 5 || o == 6 || o == 7) && n && *n >= 4 && b) {   /* los tiempos */
		*(u32 *)b = 60000;
		*n = 4;
		return TRUE;
	}
	pon_error(12009);          /* ERROR_INTERNET_INVALID_OPTION */
	return FALSE;
}
API(void *) InternetSetStatusCallback(HANDLE h, void *f) { (void)h; (void)f; return 0; }
API(void *) InternetSetStatusCallbackA(HANDLE h, void *f) { (void)h; (void)f; return 0; }
API(BOOL) InternetGetLastResponseInfoA(u32 *e, char *d, u32 *n)
{
	u32 l = strlen(ultima_resp);
	if (e)
		*e = 0;
	if (!d || *n < l + 1) {
		*n = l + 1;
		pon_error(E_CORTO);
		return FALSE;
	}
	memcpy(d, ultima_resp, l + 1);
	*n = l;
	return TRUE;
}
API(u32) InternetErrorDlg(HWND h, HANDLE r, u32 e, u32 f, void **d) { (void)h; (void)r; (void)e; (void)f; (void)d; return 1223; }   /* ERROR_CANCELLED */
API(BOOL) InternetTimeToSystemTimeA(const char *t, void *s, u32 r) { (void)t; (void)s; (void)r; pon_error(87); return FALSE; }
API(BOOL) InternetTimeFromSystemTimeA(const void *s, u32 r, char *t, u32 n) { (void)s; (void)r; if (t && n) t[0] = 0; pon_error(87); return FALSE; }
API(BOOL) InternetGetCookieA(const char *u, const char *n, char *d, u32 *l) { (void)u; (void)n; (void)d; (void)l; pon_error(12006); return FALSE; }
API(BOOL) InternetSetCookieA(const char *u, const char *n, const char *d) { (void)u; (void)n; (void)d; return FALSE; }
API(BOOL) InternetLockRequestFile(HANDLE h, HANDLE *l) { (void)h; *l = 0; return FALSE; }
API(BOOL) InternetUnlockRequestFile(HANDLE l) { (void)l; return TRUE; }
API(BOOL) InternetFindNextFileA(HANDLE h, void *d) { (void)h; (void)d; pon_error(18); return FALSE; }   /* ERROR_NO_MORE_FILES */
API(BOOL) GetUrlCacheEntryInfoA(const char *u, void *i, u32 *n) { (void)u; (void)i; (void)n; pon_error(2); return FALSE; }
API(BOOL) DeleteUrlCacheEntry(const char *u) { (void)u; pon_error(2); return FALSE; }
API(BOOL) DeleteUrlCacheEntryA(const char *u) { (void)u; pon_error(2); return FALSE; }

/* FTP: aún no (no se puede conectar) */
static BOOL sin_ftp(void) { pon_error(E_CONECTAR); return FALSE; }
API(HANDLE) FtpOpenFileA(HANDLE c, const char *n, u32 a, u32 f, u32 x) { (void)c; (void)n; (void)a; (void)f; (void)x; sin_ftp(); return 0; }
API(BOOL) FtpGetFileA(HANDLE c, const char *r, const char *l, BOOL e, u32 a, u32 f, u32 x) { (void)c; (void)r; (void)l; (void)e; (void)a; (void)f; (void)x; return sin_ftp(); }
API(BOOL) FtpPutFileA(HANDLE c, const char *l, const char *r, u32 f, u32 x) { (void)c; (void)l; (void)r; (void)f; (void)x; return sin_ftp(); }
API(BOOL) FtpSetCurrentDirectoryA(HANDLE c, const char *d) { (void)c; (void)d; return sin_ftp(); }
API(BOOL) FtpGetCurrentDirectoryA(HANDLE c, char *d, u32 *n) { (void)c; (void)d; (void)n; return sin_ftp(); }
API(BOOL) FtpDeleteFileA(HANDLE c, const char *n) { (void)c; (void)n; return sin_ftp(); }
API(BOOL) FtpCreateDirectoryA(HANDLE c, const char *n) { (void)c; (void)n; return sin_ftp(); }
API(BOOL) FtpRenameFileA(HANDLE c, const char *a, const char *b) { (void)c; (void)a; (void)b; return sin_ftp(); }
API(HANDLE) FtpFindFirstFileA(HANDLE c, const char *n, void *d, u32 f, u32 x) { (void)c; (void)n; (void)d; (void)f; (void)x; sin_ftp(); return 0; }
API(u32) FtpGetFileSize(HANDLE f, u32 *a) { (void)f; if (a) *a = 0; sin_ftp(); return 0xFFFFFFFF; }
