/* red.c - WIN32.BIN: WSOCK32 y WS2_32 (Winsock 1.1, lo de los clientes)
 * sobre el servicio de red de Hispa-Pentium (RED.BIN, la llamada 90: lo
 * mismo que usa redes.cas): TCP (connect, send, recv, closesocket, con y sin
 * bloqueo, select), UDP (sendto, recvfrom), el DNS (gethostbyname) y lo de
 * siempre (inet_addr, htons...). Escuchar (listen, accept) no: el servicio
 * aún no lo da. HispaRedConfig, la de Hispa-486, da la IP y la MAC.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "w32.h"
#define API(t) t WINAPI

/* ---- el servicio de red (redes.cas: las órdenes y los errores) ---- */
enum { O_ESTADO = 1, O_RESOLVER, O_PING, O_CONECTAR, O_ENVIAR, O_RECIBIR, O_CERRAR, O_UDP, O_UDP_ENVIAR };
#define SIN_RED 0xFFFFFFEFu
#define SIN_TARJETA 0xFFFFFFEEu
#define SIN_CABLE 0xFFFFFFEDu
#define SIN_IP 0xFFFFFFECu
#define TIEMPO 0xFFFFFFEBu
#define RECHAZADA 0xFFFFFFEAu
#define NO_RESUELVE 0xFFFFFFE9u
#define CORTADA 0xFFFFFFE8u
#define ES_ERROR_RED(r) ((u32)(r) >= 0xFFFFFFE0u)
#define MAX_DATOS 8192
static u32 cab[8];
static u32 pide(u32 orden, u32 a, u32 b, u32 c, u32 d, void *datos, u32 largo, u32 cabe)
{
	u32 r, k;
	for (k = 0; k < 3; k++) {
		cab[0] = orden; cab[1] = a; cab[2] = b; cab[3] = c; cab[4] = d; cab[5] = cab[6] = 0; cab[7] = largo;
		r = sc(90, LIN(cab), datos ? LIN(datos) : 0, cabe);
		/* (la red se está poniendo en marcha: se espera un poco) */
		if ((r != SIN_IP && r != SIN_CABLE) || orden == O_ESTADO)
			return r;
		sys_dormir(100);
	}
	return r;
}

static i32 ultimo;
API(i32) WSAGetLastError(void) { return ultimo; }
API(void) WSASetLastError(i32 e) { ultimo = e; }
static i32 falla(u32 r)
{
	switch (r) {
	case SIN_RED: case SIN_TARJETA: case SIN_CABLE: case SIN_IP: ultimo = 10050; break;   /* WSAENETDOWN */
	case TIEMPO: ultimo = 10060; break;          /* WSAETIMEDOUT */
	case RECHAZADA: ultimo = 10061; break;       /* WSAECONNREFUSED */
	case NO_RESUELVE: ultimo = 11001; break;     /* WSAHOST_NOT_FOUND */
	case CORTADA: ultimo = 10054; break;         /* WSAECONNRESET */
	case 0xFFFFFFE6u: ultimo = 10065; break;     /* WSAEHOSTUNREACH */
	default: ultimo = 10050;
	}
	return -1;
}

/* ---- los sockets ---- */
#define MAX_SOCK 32
typedef struct {
	int usado, tipo, sin_bloqueo, conectado, cerrado_otro;
	u32 con;                 /* la conexión del servicio (TCP) o el puerto abierto (UDP) */
	u32 ip, puerto;          /* con quién */
	u16 puerto_local;
	u8 *pre;                 /* lo recibido de antemano (por select) */
	u32 npre, ipre;
} Sock;
static Sock socks[MAX_SOCK];
static Sock *sock(u32 s)
{
	if (s < 1 || s > MAX_SOCK || !socks[s - 1].usado) {
		ultimo = 10038;                          /* WSAENOTSOCK */
		return 0;
	}
	return &socks[s - 1];
}
typedef struct { i16 fam; u16 puerto; u32 ip; u8 cero[8]; } SOCKADDR_IN;
static u16 vuelta16(u16 v) { return (v >> 8) | (v << 8); }

API(i32) WSAStartup(u16 v, u8 *d)
{
	memset(d, 0, 400);
	*(u16 *)d = v > 0x0202 ? 0x0202 : v;
	*(u16 *)(d + 2) = 0x0202;
	strcpy((char *)d + 4, "Hispa-Pentium Winsock 1.1");
	strcpy((char *)d + 261, "En marcha");
	*(u16 *)(d + 390) = MAX_SOCK;
	*(u16 *)(d + 392) = 8192;
	return 0;
}
API(i32) WSACleanup(void) { return 0; }
API(BOOL) WSAIsBlocking(void) { return FALSE; }
API(i32) WSACancelBlockingCall(void) { return 0; }
API(u32) socket(i32 fam, i32 tipo, i32 prot)
{
	i32 k;
	(void)prot;
	if (fam != 2 || (tipo != 1 && tipo != 2)) {
		ultimo = 10047;                          /* WSAEAFNOSUPPORT */
		return 0xFFFFFFFF;
	}
	for (k = 0; k < MAX_SOCK; k++)
		if (!socks[k].usado) {
			memset(&socks[k], 0, sizeof socks[k]);
			socks[k].usado = 1;
			socks[k].tipo = tipo;
			socks[k].con = 0xFFFFFFFF;
			return k + 1;
		}
	ultimo = 10024;                              /* WSAEMFILE */
	return 0xFFFFFFFF;
}
API(i32) connect(u32 s, const SOCKADDR_IN *a, i32 n)
{
	Sock *x = sock(s);
	u32 r;
	(void)n;
	if (!x)
		return -1;
	x->ip = a->ip;
	x->puerto = vuelta16(a->puerto);
	if (x->tipo == 2) {                          /* UDP: solo apunta a quién */
		x->conectado = 1;
		return 0;
	}
	r = pide(O_CONECTAR, x->ip, x->puerto, 1000, 0, 0, 0, 0);
	traza("connect(%x:%u) = %x", x->ip, x->puerto, r);
	if (ES_ERROR_RED(r))
		return falla(r);
	x->con = r;
	x->conectado = 1;
	return 0;
}
API(i32) send(u32 s, const char *p, i32 n, i32 fl)
{
	Sock *x = sock(s);
	i32 hecho = 0;
	(void)fl;
	if (!x)
		return -1;
	if (!x->conectado) {
		ultimo = 10057;                          /* WSAENOTCONN */
		return -1;
	}
	if (x->tipo == 2) {
		extern i32 WINAPI sendto(u32, const char *, i32, i32, const SOCKADDR_IN *, i32);
		SOCKADDR_IN a = { 2, vuelta16(x->puerto), x->ip, { 0 } };
		return sendto(s, p, n, fl, &a, sizeof a);
	}
	while (hecho < n) {
		u32 t = n - hecho > MAX_DATOS ? MAX_DATOS : n - hecho, r;
		r = pide(O_ENVIAR, x->con, 0, 0, 0, (void *)(p + hecho), t, 0);
		if (ES_ERROR_RED(r))
			return hecho ? hecho : falla(r);
		hecho += r;
	}
	return hecho;
}
/* lo que haya llegado, a su búfer de antes (espera: en tics). 1 si hay algo
 * (o la otra punta ha cerrado), 0 si no, -1 si error */
static int traer(Sock *x, u32 espera)
{
	u32 r;
	if (x->npre > x->ipre || x->cerrado_otro)
		return 1;
	if (!x->pre)
		x->pre = m_pedir(MAX_DATOS);
	r = pide(O_RECIBIR, x->con, espera, 0, 0, x->pre, 0, MAX_DATOS);
	if (r == TIEMPO)
		return 0;
	if (ES_ERROR_RED(r)) {
		falla(r);
		return -1;
	}
	x->ipre = 0;
	x->npre = r;
	if (!r)
		x->cerrado_otro = 1;
	return 1;
}
API(i32) recv(u32 s, char *p, i32 n, i32 fl)
{
	Sock *x = sock(s);
	i32 k;
	if (!x)
		return -1;
	if (x->tipo == 2) {
		extern i32 WINAPI recvfrom(u32, char *, i32, i32, SOCKADDR_IN *, i32 *);
		return recvfrom(s, p, n, fl, 0, 0);
	}
	if (!x->conectado) {
		ultimo = 10057;
		return -1;
	}
	for (;;) {
		k = traer(x, x->sin_bloqueo ? 0 : 50);
		if (k < 0)
			return -1;
		if (k > 0)
			break;
		if (x->sin_bloqueo) {
			ultimo = 10035;                      /* WSAEWOULDBLOCK */
			return -1;
		}
	}
	if (x->cerrado_otro && x->npre <= x->ipre)
		return 0;
	k = x->npre - x->ipre;
	if (k > n)
		k = n;
	memcpy(p, x->pre + x->ipre, k);
	if (!(fl & 2))                               /* MSG_PEEK */
		x->ipre += k;
	return k;
}
API(i32) closesocket(u32 s)
{
	Sock *x = sock(s);
	if (!x)
		return -1;
	if (x->con != 0xFFFFFFFF)
		pide(O_CERRAR, x->con, 0, 0, 0, 0, 0, 0);
	m_soltar(x->pre);
	x->usado = 0;
	return 0;
}
API(i32) shutdown(u32 s, i32 como) { (void)s; (void)como; return 0; }
API(i32) ioctlsocket(u32 s, u32 orden, u32 *v)
{
	Sock *x = sock(s);
	if (!x)
		return -1;
	if (orden == 0x8004667Eu) {                  /* FIONBIO */
		x->sin_bloqueo = *v != 0;
		return 0;
	}
	if (orden == 0x4004667Fu) {                  /* FIONREAD */
		if (x->tipo == 1 && x->conectado)
			traer(x, 0);
		*v = x->npre > x->ipre ? x->npre - x->ipre : 0;
		return 0;
	}
	ultimo = 10022;
	return -1;
}
typedef struct { u32 n; u32 s[64]; } FD_SET;
typedef struct { i32 seg, useg; } TIMEVAL;
API(i32) select(i32 nfds, FD_SET *l, FD_SET *e, FD_SET *x, const TIMEVAL *t)
{
	u32 fin = sys_tics() + (t ? (u32)(t->seg * 100 + t->useg / 10000) : 0xFFFFFFF);
	FD_SET rl, re;
	i32 total;
	(void)nfds;
	for (;;) {
		u32 k;
		total = 0;
		rl.n = re.n = 0;
		if (l)
			for (k = 0; k < l->n; k++) {
				Sock *s = sock(l->s[k]);
				if (s && s->tipo == 1 && s->conectado && traer(s, 0) != 0)
					rl.s[rl.n++] = l->s[k];
			}
		if (e)
			for (k = 0; k < e->n; k++) {
				Sock *s = sock(e->s[k]);
				if (s && (s->conectado || s->tipo == 2))
					re.s[re.n++] = e->s[k];
			}
		total = rl.n + re.n;
		if (total || (i32)(sys_tics() - fin) >= 0)
			break;
		sys_dormir(1);
	}
	if (l) *l = rl;
	if (e) *e = re;
	if (x) x->n = 0;
	return total;
}
API(i32) __WSAFDIsSet(u32 s, FD_SET *f)
{
	u32 k;
	for (k = 0; k < f->n; k++)
		if (f->s[k] == s)
			return 1;
	return 0;
}
API(i32) bind(u32 s, const SOCKADDR_IN *a, i32 n)
{
	Sock *x = sock(s);
	(void)n;
	if (!x)
		return -1;
	x->puerto_local = vuelta16(a->puerto);
	return 0;
}
API(i32) listen(u32 s, i32 n) { (void)s; (void)n; ultimo = 10045; return -1; }   /* WSAEOPNOTSUPP */
API(u32) accept(u32 s, SOCKADDR_IN *a, i32 *n) { (void)s; (void)a; (void)n; ultimo = 10045; return 0xFFFFFFFF; }
API(i32) sendto(u32 s, const char *p, i32 n, i32 fl, const SOCKADDR_IN *a, i32 al)
{
	Sock *x = sock(s);
	u32 r;
	(void)fl; (void)al;
	if (!x)
		return -1;
	if (x->tipo == 1)
		return send(s, p, n, fl);
	if (x->con == 0xFFFFFFFF) {
		r = pide(O_UDP, x->puerto_local, 0, 0, 0, 0, 0, 0);
		if (ES_ERROR_RED(r))
			return falla(r);
		x->con = r;
	}
	r = pide(O_UDP_ENVIAR, x->con, a->ip, vuelta16(a->puerto), 0, (void *)p, n > MAX_DATOS ? MAX_DATOS : n, 0);
	if (ES_ERROR_RED(r))
		return falla(r);
	return n;
}
API(i32) recvfrom(u32 s, char *p, i32 n, i32 fl, SOCKADDR_IN *a, i32 *al)
{
	Sock *x = sock(s);
	u32 r;
	(void)fl;
	if (!x)
		return -1;
	if (x->tipo == 1)
		return recv(s, p, n, fl);
	if (x->con == 0xFFFFFFFF) {
		r = pide(O_UDP, x->puerto_local, 0, 0, 0, 0, 0, 0);
		if (ES_ERROR_RED(r))
			return falla(r);
		x->con = r;
	}
	r = pide(O_RECIBIR, x->con, x->sin_bloqueo ? 0 : 1000, 0, 0, p, 0, n > MAX_DATOS ? MAX_DATOS : n);
	if (r == TIEMPO && x->sin_bloqueo) {
		ultimo = 10035;
		return -1;
	}
	if (ES_ERROR_RED(r))
		return falla(r);
	if (a) {
		a->fam = 2;
		a->ip = cab[1];
		a->puerto = vuelta16(cab[2]);
		if (al)
			*al = 16;
	}
	return r;
}
API(i32) setsockopt(u32 s, i32 n, i32 o, const char *v, i32 l) { (void)s; (void)n; (void)o; (void)v; (void)l; return 0; }
API(i32) getsockopt(u32 s, i32 n, i32 o, char *v, i32 *l)
{
	(void)s; (void)n; (void)o;
	if (v && l && *l >= 4) {
		*(u32 *)v = 0;
		*l = 4;
	}
	return 0;
}
API(i32) getpeername(u32 s, SOCKADDR_IN *a, i32 *n)
{
	Sock *x = sock(s);
	if (!x)
		return -1;
	a->fam = 2; a->ip = x->ip; a->puerto = vuelta16(x->puerto);
	if (n) *n = 16;
	return 0;
}
API(i32) getsockname(u32 s, SOCKADDR_IN *a, i32 *n)
{
	Sock *x = sock(s);
	u32 ip = 0;
	if (!x)
		return -1;
	sc(97, LIN(&ip), 0, 0);
	a->fam = 2; a->ip = ip; a->puerto = vuelta16(x->puerto_local);
	if (n) *n = 16;
	return 0;
}

/* ---- los nombres ---- */
static struct { char *nombre; char **alias; i16 tipo, largo; char **lista; } he;
static char he_nombre[128];
static u32 he_ip;
static char *he_ips[2], *he_alias[1];
static i32 texto_ip(const char *t, u32 *ip);
API(void *) gethostbyname(const char *n)
{
	u32 ip, r;
	if (!n) {
		ultimo = 10014;
		return 0;
	}
	if (!texto_ip(n, &ip)) {
		r = pide(O_RESOLVER, 0, 0, 0, 0, (void *)n, strlen(n), 0);
		if (ES_ERROR_RED(r)) {
			falla(r);
			return 0;
		}
		ip = cab[1];
	}
	copia_n(he_nombre, n, sizeof he_nombre);
	he_ip = ip;
	he_ips[0] = (char *)&he_ip;
	he_ips[1] = 0;
	he_alias[0] = 0;
	he.nombre = he_nombre;
	he.alias = he_alias;
	he.tipo = 2;
	he.largo = 4;
	he.lista = he_ips;
	return &he;
}
API(void *) gethostbyaddr(const char *a, i32 l, i32 t)
{
	(void)l; (void)t;
	memcpy(&he_ip, a, 4);
	fmt(he_nombre, sizeof he_nombre, "%u.%u.%u.%u", he_ip & 255, (he_ip >> 8) & 255, (he_ip >> 16) & 255, he_ip >> 24);
	return gethostbyname(he_nombre);
}
API(i32) gethostname(char *d, i32 n)
{
	char b[64], c[64];
	b[0] = 0;
	sc(68, 3, LIN(b), 0);
	if (!b[0])
		strcpy(b, "HISPA-PENTIUM");
	a1252(c, b, sizeof c);
	copia_n(d, c, n);
	return 0;
}
static struct { char *nombre; char **alias; i16 puerto; char *prot; } se;
API(void *) getservbyname(const char *n, const char *p)
{
	static const struct { const char *n; u16 p; } t[] = { { "http", 80 }, { "https", 443 }, { "ftp", 21 }, { "smtp", 25 }, { "pop3", 110 }, { "telnet", 23 }, { "domain", 53 } };
	static char *nada[1];
	u32 k;
	for (k = 0; k < sizeof t / sizeof t[0]; k++)
		if (!stricmp_(n, t[k].n)) {
			se.nombre = (char *)t[k].n;
			se.alias = nada;
			se.puerto = vuelta16(t[k].p);
			se.prot = (char *)(p ? p : "tcp");
			return &se;
		}
	ultimo = 11001;
	return 0;
}
API(void *) getservbyport(i32 puerto, const char *p) { (void)puerto; (void)p; return 0; }
API(void *) getprotobyname(const char *n) { (void)n; return 0; }
API(u16) htons(u16 v) { return vuelta16(v); }
API(u16) ntohs(u16 v) { return vuelta16(v); }
API(u32) htonl(u32 v) { return (v >> 24) | ((v >> 8) & 0xFF00) | ((v << 8) & 0xFF0000) | (v << 24); }
API(u32) ntohl(u32 v) { return htonl(v); }
static i32 texto_ip(const char *s, u32 *ip)
{
	u32 r = 0, k, v;
	for (k = 0; k < 4; k++) {
		v = 0;
		if (*s < '0' || *s > '9')
			return 0;
		while (*s >= '0' && *s <= '9')
			v = v * 10 + *s++ - '0';
		if (v > 255)
			return 0;
		r |= v << (k * 8);
		if (k < 3 && *s++ != '.')
			return 0;
	}
	if (*s)
		return 0;
	*ip = r;
	return 1;
}
API(u32) inet_addr(const char *s)
{
	u32 ip;
	return texto_ip(s, &ip) ? ip : 0xFFFFFFFF;
}
API(char *) inet_ntoa(u32 a)
{
	static char b[20];
	fmt(b, sizeof b, "%u.%u.%u.%u", a & 255, (a >> 8) & 255, (a >> 16) & 255, a >> 24);
	return b;
}
API(HANDLE) WSAAsyncGetHostByName(HWND h, u32 m, const char *n, char *b, i32 l) { (void)h; (void)m; (void)n; (void)b; (void)l; ultimo = 10045; return 0; }
API(i32) WSAAsyncSelect(u32 s, HWND h, u32 m, i32 e) { (void)s; (void)h; (void)m; (void)e; ultimo = 10045; return -1; }

/* la de Hispa-486: la IP, la máscara, la puerta y el DNS (16 octetos) y la MAC (6) */
typedef struct { u32 estado; u8 mac[8]; u32 ip, mascara, puerta, dns, servidor; u32 resto[24]; } INFO_RED;
API(i32) HispaRedConfig(u8 *c, u8 *mac)
{
	static INFO_RED i;
	u32 r = pide(O_ESTADO, 0, 0, 0, 0, &i, 0, sizeof i), k;
	for (k = 0; k < 30 && (ES_ERROR_RED(r) || i.estado != 4); k++) {
		/* (la IP aún no está: DHCP) */
		sys_dormir(20);
		r = pide(O_ESTADO, 0, 0, 0, 0, &i, 0, sizeof i);
	}
	if (ES_ERROR_RED(r) || i.estado != 4)
		return 0;
	if (c) {
		memcpy(c, &i.ip, 4);
		memcpy(c + 4, &i.mascara, 4);
		memcpy(c + 8, &i.puerta, 4);
		memcpy(c + 12, &i.dns, 4);
	}
	if (mac)
		memcpy(mac, i.mac, 6);
	return 1;
}
