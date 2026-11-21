/* hispa.c - lo común de los programas en C del anillo 3 de Hispa-Pentium
 * (hispa.h): el registro, los archivos, el azar, la red y las ventanas.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stdarg.h>
#include "hispa.h"
#include "monocypher.h"

u32 h_hz = 100;
char h_nombre_prog[16] = "c";
char h_sistema = 'C';
u32 h_cab[8];

u32 h_tics(void) { return sc(SC_TICS, 0, 0, 0); }
void h_dormir(u32 t) { sc(SC_DORMIR, t, 0, 0); }

void h_salir(int codigo)
{
	sc(SC_SALIR, (u32)codigo, 0, 0);
	for (;;) ;
}

void h_escribe(const char *t)
{
	sc(SC_ESCRIBIR, (u32)t, strlen(t), 0);
}

void h_escribef(const char *f, ...)
{
	static char l[1024];
	va_list va;
	int n;

	va_start(va, f);
	n = vformatear(l, sizeof l, f, va);
	va_end(va);
	sc(SC_ESCRIBIR, (u32)l, n, 0);
}

/* ---- el registro: en la memoria y por el puerto serie ---- */
#define CABE_REG 6000
char h_registro[CABE_REG];
u32 h_registro_largo;

void h_apunta(const char *f, ...)
{
	char l[400], s[420];
	va_list va;
	u32 n;

	va_start(va, f);
	n = vformatear(l, sizeof l, f, va);
	va_end(va);
	if (h_registro_largo + n + 2 > CABE_REG) {
		memcpy(h_registro, h_registro + CABE_REG / 2, h_registro_largo - CABE_REG / 2);
		h_registro_largo -= CABE_REG / 2;
	}
	memcpy(h_registro + h_registro_largo, l, n);
	h_registro_largo += n;
	h_registro[h_registro_largo++] = '\r';
	h_registro[h_registro_largo++] = '\n';
	n = formatear(s, sizeof s, "%s: %s\r\n", h_nombre_prog, l);
	sc(SC_SERIE, (u32)s, n, 0);
}

u32 h_tecla(int espera)
{
	return sc(SC_TECLA, espera ? 0 : 1, 0, 0);
}

int h_argumentos(char *d, int cabe)
{
	u32 n = sc(SC_ARGUMENTOS, (u32)d, cabe - 1, 0);

	if (ES_ERROR(n))
		n = 0;
	if ((int)n > cabe - 1)
		n = cabe - 1;
	d[n] = 0;
	return n;
}

void *h_memoria(u32 n)
{
	u32 r = sc(SC_MAS_MEMORIA, (n + 15) & ~15u, 0, 0);

	if (ES_ERROR(r))
		return 0;
	return (void *)r;
}

static int minus(int c)
{
	return c >= 'A' && c <= 'Z' ? c + 32 : c;
}

int h_es_ascii_igual(const char *a, const char *b)
{
	while (*a && minus((u8)*a) == minus((u8)*b))
		a++, b++;
	return *a == 0 && *b == 0;
}

int h_empieza(const char *t, const char *p)
{
	while (*p && minus((u8)*t) == minus((u8)*p))
		t++, p++;
	return *p == 0;
}

void h_copia(char *d, const char *o, int cabe)
{
	int i;

	for (i = 0; i < cabe - 1 && o[i]; i++)
		d[i] = o[i];
	d[i] = 0;
}

int h_hex(const u8 *d, int n, char *t)
{
	int i;

	for (i = 0; i < n; i++) {
		t[2 * i] = "0123456789abcdef"[d[i] >> 4];
		t[2 * i + 1] = "0123456789abcdef"[d[i] & 15];
	}
	t[2 * n] = 0;
	return 2 * n;
}

static int cifra_hex(int c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	c = minus(c);
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	return -1;
}

int h_de_hex(const char *t, u8 *d, int n)
{
	int i;

	for (i = 0; i < n; i++) {
		int a = cifra_hex((u8)t[2 * i]), b = a < 0 ? -1 : cifra_hex((u8)t[2 * i + 1]);
		if (a < 0 || b < 0)
			return 0;
		d[i] = a << 4 | b;
	}
	return 1;
}

static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int h_base64(const u8 *d, int n, char *t, int cabe)
{
	int i, k = 0;

	for (i = 0; i < n; i += 3) {
		u32 v = d[i] << 16 | (i + 1 < n ? d[i + 1] << 8 : 0) | (i + 2 < n ? d[i + 2] : 0);
		if (k + 5 > cabe)
			break;
		t[k++] = b64[v >> 18 & 63];
		t[k++] = b64[v >> 12 & 63];
		t[k++] = i + 1 < n ? b64[v >> 6 & 63] : '=';
		t[k++] = i + 2 < n ? b64[v & 63] : '=';
	}
	t[k] = 0;
	return k;
}

int h_de_base64(const char *t, int n, u8 *d, int cabe)
{
	u32 v = 0;
	int bits = 0, k = 0, i;

	for (i = 0; i < n; i++) {
		int c = (u8)t[i], x;
		const char *p;
		if (c == '=' || c == '\r' || c == '\n' || c == ' ')
			continue;
		for (p = b64; *p && *p != c; p++) ;
		if (!*p)
			return -1;
		x = p - b64;
		v = v << 6 | x;
		bits += 6;
		if (bits >= 8) {
			bits -= 8;
			if (k >= cabe)
				return -1;
			d[k++] = v >> bits;
		}
	}
	return k;
}

/* ---- los archivos ---- */
static int con_hispa = -1;

void h_ruta(char *d, int cabe, const char *dentro)
{
	if (con_hispa < 0) {
		char r[16];
		u32 m;
		formatear(r, sizeof r, "%c:\\HISPA", h_sistema);
		m = sc(SC_ABRIR, (u32)r, 0, 0);
		con_hispa = !ES_ERROR(m);
		if (con_hispa)
			sc(SC_CERRAR, m, 0, 0);
	}
	formatear(d, cabe, "%c:\\%s%s", h_sistema, con_hispa ? "HISPA\\" : "", dentro);
}

int h_lee_archivo(const char *ruta, void *d, u32 cabe)
{
	u32 m, n, total = 0;

	m = sc(SC_ABRIR, (u32)ruta, 0, 0);
	if (ES_ERROR(m))
		return -1;
	while (total < cabe) {
		n = sc(SC_LEER, m, (u32)d + total, cabe - total);
		if (ES_ERROR(n)) {
			sc(SC_CERRAR, m, 0, 0);
			return -1;
		}
		if (n == 0)
			break;
		total += n;
	}
	sc(SC_CERRAR, m, 0, 0);
	return total;
}

int h_escribe_archivo(const char *ruta, const void *d, u32 n)
{
	u32 m, r;

	m = sc(SC_ABRIR, (u32)ruta, 2, 0);
	if (ES_ERROR(m))
		return -1;
	r = n ? sc(SC_ESCRIBIR_ARCHIVO, m, (u32)d, n) : 0;
	sc(SC_CERRAR, m, 0, 0);
	return r == n ? 0 : -1;
}

int h_existe(const char *ruta)
{
	u32 m = sc(SC_ABRIR, (u32)ruta, 0, 0);

	if (ES_ERROR(m))
		return 0;
	sc(SC_CERRAR, m, 0, 0);
	return 1;
}

void h_carpeta(const char *ruta)
{
	if (!h_existe(ruta))
		sc(SC_CREAR_CARPETA, (u32)ruta, 0, 0);
}

int h_ini(const char *t, const char *clave, char *valor, int cabe)
{
	int n = strlen(clave), i = 0;

	while (t[i]) {
		while (t[i] == ' ' || t[i] == '\t')
			i++;
		if (h_empieza(t + i, clave)) {
			int j = i + n, k = 0;
			while (t[j] == ' ' || t[j] == '\t')
				j++;
			if (t[j] == '=') {
				j++;
				while (t[j] == ' ' || t[j] == '\t')
					j++;
				while (t[j] && t[j] != '\r' && t[j] != '\n' && k < cabe - 1)
					valor[k++] = t[j++];
				while (k > 0 && (valor[k - 1] == ' ' || valor[k - 1] == '\t'))
					k--;
				valor[k] = 0;
				return 1;
			}
		}
		while (t[i] && t[i] != '\n')
			i++;
		if (t[i])
			i++;
	}
	return 0;
}

/* ---- el azar: un pozo BLAKE2b con el contador de ciclos, los tics, la
 * fecha y una semilla que se guarda de una vez para otra ---- */
static u8 pozo[64];
static u32 pozo_cuenta;
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

static void mezcla(const void *d, u32 n)
{
	crypto_blake2b_ctx c;

	crypto_blake2b_init(&c, 64);
	crypto_blake2b_update(&c, pozo, 64);
	crypto_blake2b_update(&c, d, n);
	crypto_blake2b_final(&c, pozo);
}

void h_ruido(void)
{
	u32 v[4];

	v[0] = h_tics();
	v[1] = tsc();
	v[2] = sc(SC_DATO, DATO_FECHA, 0, 0);
	v[3] = (u32)&v;
	mezcla(v, sizeof v);
}

void h_azar(void *d, u32 n)
{
	u8 b[64];
	u32 k;

	h_ruido();
	while (n) {
		crypto_blake2b_ctx c;
		pozo_cuenta++;
		crypto_blake2b_init(&c, 64);
		crypto_blake2b_update(&c, pozo, 64);
		crypto_blake2b_update(&c, (u8 *)&pozo_cuenta, 4);
		crypto_blake2b_update(&c, (const u8 *)"sale", 4);
		crypto_blake2b_final(&c, b);
		k = n < 32 ? n : 32;
		memcpy(d, b, k);
		d = (u8 *)d + k;
		n -= k;
		mezcla(b + 32, 32);
	}
	crypto_wipe(b, sizeof b);
}

static void azar_inicia(void)
{
	char r[48], nom[24];
	u8 sem[32];
	int n, i;

	hay_tsc = cpuid_tsc();
	h_ruido();
	for (i = 0; h_nombre_prog[i] && i < 8; i++)
		nom[i] = h_nombre_prog[i] >= 'a' && h_nombre_prog[i] <= 'z' ? h_nombre_prog[i] - 32 : h_nombre_prog[i];
	nom[i] = 0;
	formatear(nom + i, 8, ".SEM");
	h_ruta(r, sizeof r, nom);
	n = h_lee_archivo(r, sem, sizeof sem);
	if (n > 0)
		mezcla(sem, n);
	/* (un poco de espera con el contador de ciclos: el momento exacto varía) */
	for (i = 0; i < 8; i++) {
		h_dormir(1);
		h_ruido();
	}
	h_azar(sem, sizeof sem);
	h_escribe_archivo(r, sem, sizeof sem);
	if (n <= 0)
		h_apunta("sin semilla de antes (%s): el azar, del reloj y del contador de ciclos", nom);
	crypto_wipe(sem, sizeof sem);
}

void h_inicia(const char *nombre)
{
	struct { u8 letra, clase, medio, solo; u32 total, libre, del_sistema; u8 resto[60]; } u;
	u32 i;

	h_copia(h_nombre_prog, nombre, sizeof h_nombre_prog);
	h_hz = sc(SC_DATO, DATO_HZ, 0, 0);
	if (!h_hz || ES_ERROR(h_hz))
		h_hz = 100;
	for (i = 0; i < 26 && sc(SC_INFO_UNIDAD, i, (u32)&u, 0) == 1; i++)
		if (u.del_sistema == 1 && u.letra)
			h_sistema = u.letra;
	azar_inicia();
}

/* ---- la red ---- */
u32 h_red(u32 orden, u32 a, u32 b, u32 c, const void *d, u32 largo, void *dest, u32 cabe)
{
	u32 r;

	h_cab[0] = orden, h_cab[1] = a, h_cab[2] = b, h_cab[3] = c;
	h_cab[4] = h_cab[5] = h_cab[6] = 0;
	h_cab[7] = largo;
	if (largo && dest && dest != d)
		memcpy(dest, d, largo);
	r = sc(SC_RED, (u32)h_cab, (u32)(dest ? dest : (void *)d), cabe);
	h_ruido();
	return r;
}

static u32 tls_pide(u32 orden, u32 a, u32 b, u32 c, const void *d, u32 largo, void *dest, u32 cabe)
{
	u32 r;

	h_cab[0] = orden, h_cab[1] = a, h_cab[2] = b, h_cab[3] = c;
	h_cab[4] = h_cab[5] = h_cab[6] = 0;
	h_cab[7] = largo;
	if (largo && dest && dest != d)
		memcpy(dest, d, largo);
	r = sc(SC_TLS, (u32)h_cab, (u32)(dest ? dest : (void *)d), cabe);
	h_ruido();
	return r;
}

const char *h_texto_error_red(u32 e)
{
	switch (e) {
	case TLS_CERTIFICADO: return "el certificado del servidor no vale";
	case TLS_FALLO: return "no se ha podido hacer la conexión segura";
	case SIN_TLS: return "no hay servicio TLS (falta TLS.BIN)";
	case TLS_SIN_HORA: return "no se sabe la fecha: no se pueden comprobar los certificados";
	case SIN_RED: return "no hay servicio de red (falta RED.BIN o se arrancó con «sinred»)";
	case SIN_TARJETA: return "no hay ninguna tarjeta de red que Hispa-Pentium sepa llevar";
	case SIN_CABLE: return "la tarjeta de red no tiene enlace (¿está enchufado el cable?)";
	case SIN_IP: return "todavía no hay IP (nadie ha contestado por DHCP)";
	case TIEMPO: return "no ha contestado a tiempo";
	case RECHAZADA: return "no hay nadie en ese puerto (conexión rechazada)";
	case NO_RESUELVE: return "ese nombre no existe";
	case CORTADA_RED: return "la conexión se ha cortado";
	case NO_CONEXION: return "esa conexión no existe (o no quedan)";
	case INALCANZABLE: return "no se llega";
	}
	return "no puede ser";
}

int h_texto_a_ip(const char *t, u32 *ip)
{
	u32 v, r = 0;
	int n, i = 0, cifras;

	for (n = 0; n < 4; n++) {
		v = 0;
		cifras = 0;
		while (t[i] >= '0' && t[i] <= '9') {
			v = v * 10 + (t[i++] - '0');
			if (++cifras > 3)
				return 0;
		}
		if (!cifras || v > 255)
			return 0;
		r |= v << (n * 8);
		if (n < 3 && t[i++] != '.')
			return 0;
	}
	if (t[i])
		return 0;
	*ip = r;
	return 1;
}

void h_ip_a_texto(u32 ip, char *t)
{
	formatear(t, 16, "%u.%u.%u.%u", ip & 255, ip >> 8 & 255, ip >> 16 & 255, ip >> 24);
}

static void espera_red(void)
{
	u32 ip, e;
	int i;

	for (i = 0; i < 100; i++) {
		e = sc(SC_ESTADO_RED, (u32)&ip, 0, 0);
		if (e != 3 && e != 2)		/* RED_BUSCANDO, RED_SIN_CABLE */
			return;
		if (e == 2 && i >= 60)
			return;
		h_dormir(10);
	}
}

static u32 con_red(u32 orden, u32 a, u32 b, u32 c, const void *d, u32 largo, void *dest, u32 cabe)
{
	u32 r = h_red(orden, a, b, c, d, largo, dest, cabe);

	if (r == SIN_IP || r == SIN_CABLE) {
		espera_red();
		r = h_red(orden, a, b, c, d, largo, dest, cabe);
	}
	return r;
}

u32 h_mi_ip(void)
{
	u32 ip = 0, e = sc(SC_ESTADO_RED, (u32)&ip, 0, 0);

	return e == 4 ? ip : 0;
}

u32 h_resolver(const char *nombre, u32 *ip)
{
	static char t[256];
	u32 r;

	if (h_texto_a_ip(nombre, ip))
		return 0;
	h_copia(t, nombre, sizeof t);
	r = con_red(O_RESOLVER, 0, 0, 0, t, strlen(t), t, 0);
	if (r == 0)
		*ip = h_cab[1];
	return r;
}

u32 h_conectar(u32 ip, u32 puerto, u32 espera)
{
	return con_red(O_CONECTAR, ip, puerto, espera, 0, 0, 0, 0);
}

u32 h_conectar_seguro(const char *nombre, u32 ip, u32 puerto, u32 espera)
{
	static char t[200];
	u32 r;

	h_copia(t, nombre, 64);
	r = tls_pide(O_CONECTAR, ip, puerto, espera, t, strlen(t), t, 199);
	if (r == SIN_IP || r == SIN_CABLE) {
		espera_red();
		h_copia(t, nombre, 64);
		r = tls_pide(O_CONECTAR, ip, puerto, espera, t, strlen(t), t, 199);
	}
	return r;
}

static int segura(u32 s) { return s >= PRIMERA_SEGURA && s < PRIMERA_SEGURA + 0x100; }

u32 h_enviar(u32 s, const void *d, u32 n)
{
	static u8 trozo[MAX_DATOS];
	u32 hecho = 0, k, r;

	while (hecho < n) {
		k = n - hecho;
		if (k > MAX_DATOS)
			k = MAX_DATOS;
		memcpy(trozo, (const u8 *)d + hecho, k);
		if (segura(s))
			r = tls_pide(O_ENVIAR, s, 0, 0, trozo, k, trozo, 0);
		else
			r = h_red(O_ENVIAR, s, 0, 0, trozo, k, trozo, 0);
		if (ES_ERROR(r))
			return r;
		hecho += r;		/* (0: no había sitio; otra vez) */
	}
	return n;
}

u32 h_recibir(u32 s, void *d, u32 cabe, u32 espera)
{
	if (cabe > MAX_DATOS)
		cabe = MAX_DATOS;
	if (segura(s)) {
		/* (TLS.BIN da por pasado el tiempo antes de mirar la red si la espera es 0) */
		if (espera < 2)
			espera = 2;
		return tls_pide(O_RECIBIR, s, espera, 0, 0, 0, d, cabe);
	}
	return h_red(O_RECIBIR, s, espera, 0, 0, 0, d, cabe);
}

void h_cerrar_red(u32 s)
{
	if (segura(s))
		tls_pide(O_CERRAR, s, 0, 0, 0, 0, 0, 0);
	else
		h_red(O_CERRAR, s, 0, 0, 0, 0, 0, 0);
}

u32 h_escuchar(u32 puerto)
{
	return con_red(O_ESCUCHAR, puerto, 0, 0, 0, 0, 0, 0);
}

u32 h_aceptar(u32 l, u32 espera, u32 *ip, u32 *puerto)
{
	u32 r = h_red(O_ACEPTAR, l, espera, 0, 0, 0, 0, 0);

	if (!ES_ERROR(r)) {
		if (ip)
			*ip = h_cab[1];
		if (puerto)
			*puerto = h_cab[2];
	}
	return r;
}

u32 h_udp_abrir(u32 puerto)
{
	return con_red(O_UDP, puerto, 0, 0, 0, 0, 0, 0);
}

u32 h_udp_enviar(u32 s, u32 ip, u32 puerto, const void *d, u32 n)
{
	static u8 trozo[MAX_DATOS];

	if (n > MAX_DATOS)
		return NO_VALE;
	memcpy(trozo, d, n);
	return h_red(O_UDP_ENVIAR, s, ip, puerto, trozo, n, trozo, 0);
}

u32 h_udp_recibir(u32 s, void *d, u32 cabe, u32 espera, u32 *ip, u32 *puerto)
{
	u32 r;

	if (cabe > MAX_DATOS)
		cabe = MAX_DATOS;
	r = h_red(O_RECIBIR, s, espera, 0, 0, 0, d, cabe);
	if (!ES_ERROR(r)) {
		if (ip)
			*ip = h_cab[1];
		if (puerto)
			*puerto = h_cab[2];
	}
	return r;
}

/* ---- las ventanas ---- */
struct orden { u32 op; int x, y, ancho, alto; u32 tinta, tinta2; const void *datos; int n; };

int h_hay_escritorio(void)
{
	return sc(SC_DATO, DATO_ESCRITORIO, 0, 0) == 1;
}

u32 h_ventana(const char *nombre, u32 ancho, u32 alto)
{
	return sc(SC_VENTANA, (u32)nombre, ancho, alto);
}

int h_suceso(u32 v, struct suceso *s, int espera)
{
	return sc(SC_SUCESO, v, (u32)s, espera ? 1 : 0) == 1;
}

void h_rectangulo(u32 v, int x, int y, int ancho, int alto, u32 color)
{
	struct orden o = { 1, x, y, ancho, alto, color, 0, 0, 0 };

	sc(SC_DIBUJAR, v, (u32)&o, 0);
}

void h_texto_fondo(u32 v, int x, int y, const char *t, int n, u32 tinta, u32 fondo)
{
	struct orden o = { 6, x, y, 0, 0, tinta, fondo, t, n };

	sc(SC_DIBUJAR, v, (u32)&o, 0);
}

void h_titulo(u32 v, const char *t)
{
	sc(SC_TITULO, v, (u32)t, 0);
}

/* ---- la página 437 ---- */
static const u16 alta437[128] = {
	0x00C7, 0x00FC, 0x00E9, 0x00E2, 0x00E4, 0x00E0, 0x00E5, 0x00E7, 0x00EA, 0x00EB, 0x00E8, 0x00EF, 0x00EE, 0x00EC, 0x00C4, 0x00C5,
	0x00C9, 0x00E6, 0x00C6, 0x00F4, 0x00F6, 0x00F2, 0x00FB, 0x00F9, 0x00FF, 0x00D6, 0x00DC, 0x00A2, 0x00A3, 0x00A5, 0x20A7, 0x0192,
	0x00E1, 0x00ED, 0x00F3, 0x00FA, 0x00F1, 0x00D1, 0x00AA, 0x00BA, 0x00BF, 0x2310, 0x00AC, 0x00BD, 0x00BC, 0x00A1, 0x00AB, 0x00BB,
	0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556, 0x2555, 0x2563, 0x2551, 0x2557, 0x255D, 0x255C, 0x255B, 0x2510,
	0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x255E, 0x255F, 0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0x2567,
	0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256B, 0x256A, 0x2518, 0x250C, 0x2588, 0x2584, 0x258C, 0x2590, 0x2580,
	0x03B1, 0x00DF, 0x0393, 0x03C0, 0x03A3, 0x03C3, 0x00B5, 0x03C4, 0x03A6, 0x0398, 0x03A9, 0x03B4, 0x221E, 0x03C6, 0x03B5, 0x2229,
	0x2261, 0x00B1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00F7, 0x2248, 0x00B0, 0x2219, 0x00B7, 0x221A, 0x207F, 0x00B2, 0x25A0, 0x00A0,
};

int h_utf8_de_437(u8 c, char *d)
{
	u32 u = c < 128 ? c : alta437[c - 128];

	if (u < 0x80) {
		d[0] = u;
		return 1;
	}
	if (u < 0x800) {
		d[0] = 0xC0 | u >> 6;
		d[1] = 0x80 | (u & 63);
		return 2;
	}
	d[0] = 0xE0 | u >> 12;
	d[1] = 0x80 | (u >> 6 & 63);
	d[2] = 0x80 | (u & 63);
	return 3;
}

int h_437_de_unicode(u32 u)
{
	int i;

	if (u < 0x80)
		return u;
	for (i = 0; i < 128; i++)
		if (alta437[i] == u)
			return 128 + i;
	/* lo que se parece */
	switch (u) {
	case 0x00C1: return 'A';	/* Á */
	case 0x00CD: return 'I';
	case 0x00D3: return 'O';
	case 0x00DA: return 'U';
	case 0x00C0: return 'A';
	case 0x00C8: return 'E';
	case 0x2018: case 0x2019: return '\'';
	case 0x201C: case 0x201D: return '"';
	case 0x2013: case 0x2014: return '-';
	case 0x2026: return '.';
	case 0x20AC: return 'E';	/* € */
	case 0x25CF: case 0x2022: return 0xF9;
	case 0x2192: return 0x1A;
	case 0x2190: return 0x1B;
	case 0x2191: return 0x18;
	case 0x2193: return 0x19;
	case 0x256D: return 0xDA;	/* las esquinas redondeadas */
	case 0x256E: return 0xBF;
	case 0x256F: return 0xD9;
	case 0x2570: return 0xC0;
	case 0x2501: return 0xC4;
	case 0x2503: return 0xB3;
	}
	return '?';
}
