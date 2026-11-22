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
		u32 n;
		cab[0] = orden; cab[1] = a; cab[2] = b; cab[3] = c; cab[4] = d; cab[5] = cab[6] = 0; cab[7] = largo;
		n = gc_suelta_todo();               /* (JUEGOS) el servicio tarda: los otros hilos, que sigan */
		r = sc(90, LIN(cab), datos ? LIN(datos) : 0, cabe);
		gc_retoma(n);
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
	int dgrama;              /* (JUEGOS, UDP) hay un datagrama en «pre» (de pre_ip:pre_puerto) */
	u32 pre_ip, pre_puerto;
} Sock;
static Sock socks[MAX_SOCK];
static i32 trae_udp(Sock *x, u32 espera);     /* (JUEGOS) */
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
		if (x->tipo == 2) {                      /* (JUEGOS) UDP: lo que mide el siguiente datagrama */
			*v = trae_udp(x, 0) > 0 ? x->npre : 0;
			return 0;
		}
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
				else if (s && s->tipo == 2 && trae_udp(s, 0) > 0)   /* (JUEGOS) UDP: un datagrama esperando */
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
/* (JUEGOS) el puerto UDP del socket, abierto en el servicio (uno cualquiera
 * si no se ha dicho: aquí, para saber cuál es) */
static i32 abre_udp(Sock *x)
{
	u32 r, k;
	if (x->con != 0xFFFFFFFF)
		return 0;
	for (k = 0; k < 20; k++) {
		u16 p = x->puerto_local;
		if (!p)
			p = (u16)(49152 + (sys_tics() * 7919u + k * 104729u + (u32)(x - socks) * 31u) % 16000u);
		r = pide(O_UDP, p, 0, 0, 0, 0, 0, 0);
		if (!ES_ERROR_RED(r)) {
			x->con = r;
			x->puerto_local = p;
			traza("UDP: el puerto %u (%x)", p, r);
			return 0;
		}
		if (x->puerto_local) {
			ultimo = r == 0xFFFFFFE1u || r == 0xFFFFFFE2u || r == 0xFFFFFFE3u ? 10048 : 10048;   /* WSAEADDRINUSE */
			if (r == SIN_RED || r == SIN_TARJETA || r == SIN_CABLE || r == SIN_IP)
				return falla(r);
			return -1;
		}
	}
	return falla(r);
}
/* (JUEGOS) un datagrama al búfer, si llega en «espera» ms: 1 sí, 0 no, -1 error */
static i32 trae_udp(Sock *x, u32 espera)
{
	u32 r;
	if (x->dgrama)
		return 1;
	if (abre_udp(x) < 0)
		return -1;
	if (!x->pre) {
		x->pre = m_pedir(MAX_DATOS);
		if (!x->pre) {
			ultimo = 10055;                  /* WSAENOBUFS */
			return -1;
		}
	}
	r = pide(O_RECIBIR, x->con, espera, 0, 0, x->pre, 0, MAX_DATOS);
	if (r == TIEMPO)
		return 0;
	if (ES_ERROR_RED(r)) {
		falla(r);
		return -1;
	}
	x->dgrama = 1;
	x->npre = r;
	x->pre_ip = cab[1];
	x->pre_puerto = cab[2];
	return 1;
}

API(i32) bind(u32 s, const SOCKADDR_IN *a, i32 n)
{
	Sock *x = sock(s);
	(void)n;
	if (!x)
		return -1;
	x->puerto_local = vuelta16(a->puerto);
	if (x->tipo == 2)                            /* (JUEGOS) UDP: ya recibe (un servidor) */
		return abre_udp(x);
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
	if (abre_udp(x) < 0)
		return -1;
	if (n > 1472) {
		ultimo = 10040;                          /* WSAEMSGSIZE */
		return -1;
	}
	r = pide(O_UDP_ENVIAR, x->con, a ? a->ip : x->ip, a ? vuelta16(a->puerto) : x->puerto, 0, (void *)p, n, 0);
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
	/* (JUEGOS) un datagrama entero (del búfer, si ya lo trajo select); sin
	 * bloqueo, WSAEWOULDBLOCK si no hay; con bloqueo, se espera lo que haga falta */
	for (;;) {
		i32 k = trae_udp(x, x->sin_bloqueo ? 0 : 1000);
		if (k < 0)
			return -1;
		if (k)
			break;
		if (x->sin_bloqueo) {
			ultimo = 10035;                  /* WSAEWOULDBLOCK */
			return -1;
		}
	}
	r = x->npre;
	if (r > (u32)n)
		r = (u32)n;
	memcpy(p, x->pre, r);
	if (a) {
		a->fam = 2;
		a->ip = x->pre_ip;
		a->puerto = vuelta16(x->pre_puerto);
		memset(a->cero, 0, 8);
		if (al)
			*al = 16;
	}
	if (!(fl & 2))                               /* MSG_PEEK: se queda */
		x->dgrama = 0;
	if (x->npre > (u32)n) {
		x->dgrama = 0;
		ultimo = 10040;                          /* WSAEMSGSIZE: no cabía (lo que cabe, dado) */
		return -1;
	}
	return (i32)r;
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
i32 WINAPI gethostname(char *d, i32 n);
API(void *) gethostbyname(const char *n)
{
	u32 ip, r;
	if (!n) {
		ultimo = 10014;
		return 0;
	}
	{
		/* (JUEGOS) el nombre del propio equipo (y «localhost»): su IP, sin el DNS */
		char yo[64];
		gethostname(yo, sizeof yo);
		if (!stricmp_(n, yo) || !stricmp_(n, "localhost")) {
			ip = 0;
			sc(97, LIN(&ip), 0, 0);
			if (!stricmp_(n, "localhost") || !ip)
				ip = 0x0100007F;
			goto tiene;
		}
	}
	if (!texto_ip(n, &ip)) {
		r = pide(O_RESOLVER, 0, 0, 0, 0, (void *)n, strlen(n), 0);
		if (ES_ERROR_RED(r)) {
			falla(r);
			return 0;
		}
		ip = cab[1];
	}
tiene:
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

/* ---- (HL) lo que piden los juegos de 1998-2000 con Winsock 1.1 ---- */
BOOL WINAPI PostMessageA(HWND h, u32 m, u32 wp, u32 lp);
static u32 asinc_n = 0x100;
/* el nombre resuelto ya (el DNS espera aquí), y el aviso a la ventana, como
 * Windows al acabar: wParam la tarea, lParam el largo y el error */
API(HANDLE) WSAAsyncGetHostByName(HWND h, u32 m, const char *n, char *b, i32 l)
{
	struct { char *nombre; char **alias; i16 tipo, largo; char **lista; } *r = gethostbyname(n);
	u32 t = asinc_n++, err = 0, largo = 0;
	if (!r)
		err = ultimo ? (u32)ultimo : 11001;
	else {
		/* la hostent, con sus datos detrás, en el búfer del programa */
		u32 nl = strlen(r->nombre) + 1, hace = 16 + 8 + 4 + 8 + nl;
		if ((i32)hace > l)
			err = 10055;            /* WSAENOBUFS */
		else {
			u8 *p = (u8 *)b;
			char **alias = (char **)(p + 16), **lista = (char **)(p + 24);
			u32 *ip = (u32 *)(p + 32);
			char *nom = (char *)(p + 36);
			memcpy(ip, r->lista[0], 4);
			memcpy(nom, r->nombre, nl);
			alias[0] = 0;
			lista[0] = (char *)ip;
			lista[1] = 0;
			*(char **)p = nom;
			*(char ***)(p + 4) = alias;
			*(i16 *)(p + 8) = 2;
			*(i16 *)(p + 10) = 4;
			*(char ***)(p + 12) = lista;
			largo = hace;
		}
	}
	PostMessageA(h, m, t, (err << 16) | (largo & 0xFFFF));
	return (HANDLE)t;
}
API(HANDLE) WSAAsyncGetHostByAddr(HWND h, u32 m, const char *a, i32 al, i32 tipo, char *b, i32 l)
{
	char t[20];
	u32 ip;
	(void)al; (void)tipo;
	memcpy(&ip, a, 4);
	fmt(t, sizeof t, "%u.%u.%u.%u", ip & 255, (ip >> 8) & 255, (ip >> 16) & 255, ip >> 24);
	return WSAAsyncGetHostByName(h, m, t, b, l);
}
static HANDLE asinc_no(HWND h, u32 m)
{
	u32 t = asinc_n++;
	PostMessageA(h, m, t, 11004u << 16);   /* WSANO_DATA */
	return (HANDLE)t;
}
API(HANDLE) WSAAsyncGetServByName(HWND h, u32 m, const char *n, const char *p, char *b, i32 l) { (void)n; (void)p; (void)b; (void)l; return asinc_no(h, m); }
API(HANDLE) WSAAsyncGetServByPort(HWND h, u32 m, i32 pu, const char *p, char *b, i32 l) { (void)pu; (void)p; (void)b; (void)l; return asinc_no(h, m); }
API(HANDLE) WSAAsyncGetProtoByName(HWND h, u32 m, const char *n, char *b, i32 l) { (void)n; (void)b; (void)l; return asinc_no(h, m); }
API(HANDLE) WSAAsyncGetProtoByNumber(HWND h, u32 m, i32 n, char *b, i32 l) { (void)n; (void)b; (void)l; return asinc_no(h, m); }
API(i32) WSACancelAsyncRequest(HANDLE t) { (void)t; ultimo = 10037; return -1; }   /* WSAEALREADY: ya acabó */
static void *gancho;
API(void *) WSASetBlockingHook(void *f) { void *a = gancho; gancho = f; return a; }
API(i32) WSAUnhookBlockingHook(void) { gancho = 0; return 0; }
API(void *) getprotobynumber(i32 n) { (void)n; ultimo = 11004; return 0; }
API(void *) getnetbyname(const char *n) { (void)n; ultimo = 11004; return 0; }
API(u32) inet_network(const char *s) { u32 ip = inet_addr(s); return ip == 0xFFFFFFFF ? ip : htonl(ip); }
API(i32) WSARecvEx(u32 s, char *p, i32 n, i32 *fl) { if (fl) *fl = 0; return recv(s, p, n, 0); }
API(i32) EnumProtocolsA(i32 *p, void *b, u32 *n) { (void)p; (void)b; if (n) *n = 0; return 0; }
API(i32) GetAddressByNameA(u32 ns, void *t, char *n, i32 *p, u32 r, void *a, void *b, u32 *bl, char *al, u32 *all) { (void)ns; (void)t; (void)n; (void)p; (void)r; (void)a; (void)b; (void)bl; (void)al; (void)all; ultimo = 10045; return -1; }
API(BOOL) AcceptEx(u32 l, u32 a, void *b, u32 n, u32 la, u32 ra, u32 *r, void *o) { (void)l; (void)a; (void)b; (void)n; (void)la; (void)ra; (void)r; (void)o; ultimo = 10045; return FALSE; }
API(void) GetAcceptExSockaddrs(void *b, u32 n, u32 la, u32 ra, void **l, i32 *ll, void **r, i32 *rl) { (void)b; (void)n; (void)la; (void)ra; *l = *r = 0; *ll = *rl = 0; }
API(BOOL) TransmitFile(u32 s, HANDLE f, u32 n, u32 np, void *o, void *b, u32 fl) { (void)s; (void)f; (void)n; (void)np; (void)o; (void)b; (void)fl; ultimo = 10045; return FALSE; }
API(i32) WEP(void) { return 1; }
API(void) s_perror(const char *m, i32 e) { (void)m; (void)e; }
API(u32) WsControl(u32 p, u32 a, void *b, u32 *bl, void *c, u32 *cl) { (void)p; (void)a; (void)b; (void)bl; (void)c; (void)cl; return 10045; }

/* (HL) los números de las funciones de WSOCK32.DLL y de WS2_32.DLL, como los
 * de Windows (los juegos de entonces importan WSOCK32 por número). Los
 * números, comprobados con la lista de exportaciones pública de Wine
 * (dlls/wsock32/wsock32.spec y dlls/ws2_32/ws2_32.spec), solo como
 * referencia. Las dos son iguales salvo del 10 al 12. */
#define ORD_COMUN \
	{ 1, "accept" }, { 2, "bind" }, { 3, "closesocket" }, { 4, "connect" }, { 5, "getpeername" }, { 6, "getsockname" }, \
	{ 7, "getsockopt" }, { 8, "htonl" }, { 9, "htons" }, { 13, "listen" }, { 14, "ntohl" }, { 15, "ntohs" }, { 16, "recv" }, \
	{ 17, "recvfrom" }, { 18, "select" }, { 19, "send" }, { 20, "sendto" }, { 21, "setsockopt" }, { 22, "shutdown" }, \
	{ 23, "socket" }, { 51, "gethostbyaddr" }, { 52, "gethostbyname" }, { 53, "getprotobyname" }, { 54, "getprotobynumber" }, \
	{ 55, "getservbyname" }, { 56, "getservbyport" }, { 57, "gethostname" }, { 101, "WSAAsyncSelect" }, \
	{ 102, "WSAAsyncGetHostByAddr" }, { 103, "WSAAsyncGetHostByName" }, { 104, "WSAAsyncGetProtoByNumber" }, \
	{ 105, "WSAAsyncGetProtoByName" }, { 106, "WSAAsyncGetServByPort" }, { 107, "WSAAsyncGetServByName" }, \
	{ 108, "WSACancelAsyncRequest" }, { 109, "WSASetBlockingHook" }, { 110, "WSAUnhookBlockingHook" }, \
	{ 111, "WSAGetLastError" }, { 112, "WSASetLastError" }, { 113, "WSACancelBlockingCall" }, { 114, "WSAIsBlocking" }, \
	{ 115, "WSAStartup" }, { 116, "WSACleanup" }, { 151, "__WSAFDIsSet" }, { 500, "WEP" }
const Ord ord_wsock32[] = {
	{ 10, "inet_addr" }, { 11, "inet_ntoa" }, { 12, "ioctlsocket" }, ORD_COMUN,
	{ 1001, "WsControl" }, { 1100, "inet_network" }, { 1101, "getnetbyname" }, { 1107, "WSARecvEx" }, { 1108, "s_perror" },
	{ 1109, "GetAddressByNameA" }, { 1111, "EnumProtocolsA" }, { 1140, "TransmitFile" }, { 1141, "AcceptEx" },
	{ 1142, "GetAcceptExSockaddrs" }, { 0, 0 }
};
const Ord ord_ws2_32[] = {
	{ 10, "ioctlsocket" }, { 11, "inet_addr" }, { 12, "inet_ntoa" }, ORD_COMUN, { 0, 0 }
};
