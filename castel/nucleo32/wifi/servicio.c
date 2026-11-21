/* =============================================================================
 *  servicio.c - WIFI.BIN, el servicio Wi-Fi de Hispa-Pentium (docs/PENTIUM.md,
 *  «El Wi-Fi»): la tarjeta Wi-Fi, la gestión 802.11 y WPA2, para RED.BIN.
 *  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 *
 *  Es el núcleo Wi-Fi de Hispa-486 (wifi/: w80211.c, wpa.c, la radio de las
 *  Broadcom con brcmsmac y bcma de Linux, la de las Atheros con ath9k de
 *  Linux, y la radio de mentira de las pruebas), los mismos fuentes, como un
 *  programa del anillo 3. Lo de aquí hace lo que en Hispa-486 hacían el
 *  residente (drivers/hspwifi.asm) y wifi/nucleo/nucleo.c:
 *   - se apunta como servicio Wi-Fi (llamada 91: el núcleo le da los puertos,
 *     IOPL 3), pide 2 MB de memoria física seguida para el montón del
 *     controlador (94: los anillos de DMA y los paquetes salen de ahí, y así
 *     la dirección física de cualquier cosa del montón es una suma) y pone los
 *     registros de la tarjeta a su vista (95);
 *   - busca la tarjeta, la monta y enciende la radio, en tres pasos que se
 *     apuntan en WIFI.LOG (cerrado tras cada línea mientras arranca: si algo
 *     se cuelga, lo último del archivo es lo último que se hizo);
 *   - es «una tarjeta más» de RED.BIN: le pasa las tramas Ethernet que llegan
 *     y recoge las que hay que enviar (la orden O_WIFI_CAMBIO por la llamada
 *     90, en cada vuelta), con el enlace («asociada y con las claves
 *     puestas»); la IP la pide RED.BIN (DHCP), como con el cable;
 *   - atiende a la ventana del Wi-Fi (REDWIFI.BIN) por la llamada 100:
 *     buscar, la lista, conectar (la contraseña se convierte en la clave de
 *     WPA2 a trozos, sin parar el servicio), desconectar, olvidar;
 *   - recuerda las redes en WIFI.INI: el nombre y la clave ya preparada (la
 *     PMK, nunca la contraseña), cifrada con AES con una llave del equipo
 *     (WIFI.LLV, hecha al azar la primera vez), y si se conecta sola;
 *   - y se conecta sola a la mejor red recordada que se vea; si se pierde,
 *     w80211.c vuelve a buscarla cada 5 segundos.
 *  Lo dice al núcleo (llamada 96) para el icono de la bandeja: el estado, las
 *  barras de la señal y la tarjeta (para pararle el DMA si el servicio se
 *  acaba, y para el Administrador de dispositivos).
 *  Con «/SIMULA» (o «WifiSimulada=SI» en RED.INI, que lo pasa RED.BIN), la
 *  radio de mentira de wifi/nucleo/sim.c: una red «HispaPrueba» (WPA2, clave
 *  «clave de prueba») con DHCP, ARP y eco en 192.168.77.1: las pruebas.
 * ============================================================================= */
#include "hispa_linux.h"
#include "base.h"
#include "nucleo.h"
#include "wifi.h"

#define VERSION "0.1"

int brcm_busca(void);
int brcm_monta(struct radio **r);
int brcm_enciende(void);
void brcm_cuentas(d32 *bien, d32 *mal, int *vel);
int ath_busca(void);
int ath_monta(struct radio **r);
int ath_enciende(void);
void ath_cuentas(d32 *bien, d32 *mal, int *vel);
void ath_resumen(void);
int vsnprintf(char *buf, size_t tam, const char *f, va_list ap);
int snprintf(char *b, size_t n, const char *f, ...);

/* las tarjetas que se saben llevar (las de nucleo.c de Hispa-486) */
struct conductor {
	const char *familia;
	int (*busca)(void);
	int (*monta)(struct radio **r);
	int (*enciende)(void);
	void (*cuentas)(d32 *bien, d32 *mal, int *vel);
	void (*resumen)(void);
};
static const struct conductor conductores[] = {
	{ "Broadcom BCM43224/43225 (brcmsmac)", brcm_busca, brcm_monta, brcm_enciende, brcm_cuentas, NULL },
	{ "Atheros AR9285/AR9280/AR9287 (ath9k)", ath_busca, ath_monta, ath_enciende, ath_cuentas, ath_resumen },
};
static const struct conductor *cond;
static const u16 ids_brcm[] = { 0x4353, 0x4357, 0x4727, 0x0576, 0xa8d8, 0 };
static const u16 ids_ath[] = { 0x002B, 0x002A, 0x002E, 0x0029, 0x002D, 0x0024, 0x0023, 0x0027, 0x002C, 0 };

#ifdef CHIP_SIMULADO
/* WIFISIM.BIN (las pruebas): el controlador de verdad (brcmsmac y bcma) contra
 * el modelo de la tarjeta de tests/wifi/tarjeta_sim.c, con el punto de acceso
 * de mentira detrás: prueba todo lo de aquí (la memoria de DMA, los registros,
 * el microcódigo del disco) sin la tarjeta */
#include "tarjeta_sim.h"
static struct radio *ap_sim;
static int (*canal_real)(int);
static void (*sondear_real)(void);
static int canal_sim(int c) { tsim_canal = c; return canal_real(c); }
static void sondear_sim(void) { ap_sim->canal(tsim_canal); ap_sim->sondear(); sondear_real(); }
#endif

static struct radio *la_radio;
static int listo, simulada;
static u32 aparato = 0xFFFFFFFF;	/* la tarjeta en el PCI (bus << 16 | aparato << 11 | función << 8) */
static char nombre_tarjeta[48] = "Wi-Fi";
static u32 hz = 100;
static char carpeta[16] = "\\";		/* «\HISPA\» o «\» */

/* ============================================================== el tiempo */
u32 hl_pentium_tics(void) { return sc(SC_TICS, 0, 0, 0); }
u32 hl_pentium_hz(void) { return hz; }

/* ========================================== el registro: serie y WIFI.LOG */
/* Cada línea del registro (ya en la página 437: base.c la convierte) va al
 * puerto serie («wifi: ...», lo que leen las pruebas) y a WIFI.LOG: mientras
 * arranca, el archivo se escribe y se cierra en cada línea; luego, una vez
 * por segundo y en lo importante. */
static char linea[200];
static int linea_n, linea_salta;
static int cada_linea = 1;
static char pend[8192];
static int pend_n;
static int escribe_archivo(const char *nombre, const void *d, u32 n, int al_final);

static void vuelca_registro(void)
{
	if (pend_n > 0) {
		escribe_archivo("WIFI.LOG", pend, pend_n, 1);
		pend_n = 0;
	}
}

void hl_letra_pentium(char c)
{
	if (pend_n < (int)sizeof pend)
		pend[pend_n++] = c;
	if (c == '\r')
		return;
	if (c == '\n') {
		char s[220];
		int n = snprintf(s, sizeof s, "wifi: %s\r\n", linea);
		sc(SC_SERIE, (u32)s, n, 0);
		linea_n = 0;
		linea[0] = 0;
		linea_salta = 8;		/* la hora de la línea («%7lu »): no al puerto serie */
		if (cada_linea || pend_n > (int)sizeof pend - 600)
			vuelca_registro();
		return;
	}
	if (linea_salta > 0) {
		linea_salta--;
		return;
	}
	if (linea_n < (int)sizeof(linea) - 1) {
		linea[linea_n++] = c;
		linea[linea_n] = 0;
	}
}

/* de UTF-8 a la página 437 (lo que enseñan las ventanas de Castel) */
static void a437(char *d, const char *s, int cabe)
{
	static const u16 t[] = {
		0xe1, 0xa0, 0xe9, 0x82, 0xed, 0xa1, 0xf3, 0xa2, 0xfa, 0xa3, 0xf1, 0xa4, 0xd1, 0xa5,
		0xfc, 0x81, 0xdc, 0x9a, 0xbf, 0xa8, 0xa1, 0xad, 0xab, 0xae, 0xbb, 0xaf, 0xc9, 0x90,
		0xba, 0xa7, 0xaa, 0xa6, 0xe7, 0x87, 0xc7, 0x80, 0xe0, 0x85, 0xe8, 0x8a, 0xf2, 0x95, 0
	};
	int n = 0, i;

	while (*s && n < cabe - 1) {
		u32 u = (u8)*s++;
		if (u >= 0xc0 && u < 0xe0 && (*s & 0xc0) == 0x80)
			u = ((u & 0x1f) << 6) | (*s++ & 0x3f);
		else if (u >= 0xe0) {
			while ((*s & 0xc0) == 0x80)
				s++;
			u = '?';
		}
		if (u >= 128) {
			for (i = 0; t[i] && t[i] != u; i += 2)
				;
			u = t[i] ? t[i + 1] : '?';
		}
		d[n++] = u;
	}
	d[n] = 0;
}

/* ============================================================ los archivos */
static void ruta(char *d, const char *nombre)
{
	snprintf(d, 40, "%s%s", carpeta, nombre);
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

static int escribe_archivo(const char *nombre, const void *d, u32 n, int al_final)
{
	char r[40];
	u32 m, e;

	ruta(r, nombre);
	m = sc(SC_ABRIR, (u32)r, al_final ? 3 : 2, 0);
	if (ES_ERROR(m))
		return -1;
	e = sc(SC_ESCRIBIR_ARCHIVO, m, (u32)d, n);
	sc(SC_CERRAR, m, 0, 0);
	return ES_ERROR(e) || e != n ? -1 : 0;
}

/* ============================================ la memoria y los registros */
#define DMA_PAGINAS 512			/* 2 MB: el montón del controlador */
static u8 *dma_v;
static u32 dma_f, dma_tam;
#define MAX_MAPAS 8
static struct { u32 f, tam; u8 *v; } mapas[MAX_MAPAS];
static int n_mapas;
static u8 basura[4096] __attribute__((aligned(4096)));

void *hl_mapea(u32 fis, u32 tam)
{
	u32 base = fis & ~0xFFFu, des = fis - base, r;
	int i;

	for (i = 0; i < n_mapas; i++)
		if (fis >= mapas[i].f && fis + tam <= mapas[i].f + mapas[i].tam)
			return mapas[i].v + (fis - mapas[i].f);
	if (n_mapas >= MAX_MAPAS) {
		hl_log(0, "no caben más registros a la vista (%08X)\n", fis);
		return basura;
	}
	tam = (des + tam + 0xFFF) & ~0xFFFu;
	r = sc(SC_REGISTROS, base, tam, 0);
	if (ES_ERROR(r)) {
		hl_log(0, "el núcleo no deja ver los registros de %08X (%u octetos): %08X\n", base, tam, r);
		return basura;
	}
	mapas[n_mapas].f = base;
	mapas[n_mapas].tam = tam;
	mapas[n_mapas].v = (u8 *)(r & ~0xFFFu);
	n_mapas++;
	hl_log(3, "Registros %08X (%u K), a la vista en %08X\n", base, tam >> 10, r & ~0xFFFu);
	return mapas[n_mapas - 1].v + des;
}

void *hl_virt_f(u32 fis)
{
	int i;

	if (fis >= dma_f && fis < dma_f + dma_tam)
		return dma_v + (fis - dma_f);
	for (i = 0; i < n_mapas; i++)
		if (fis >= mapas[i].f && fis < mapas[i].f + mapas[i].tam)
			return mapas[i].v + (fis - mapas[i].f);
	return hl_mapea(fis, 4096);
}

u32 hl_fis_f(const void *v)
{
	const u8 *p = v;
	int i;

	if (p >= dma_v && p < dma_v + dma_tam)
		return dma_f + (p - dma_v);
	for (i = 0; i < n_mapas; i++)
		if (p >= mapas[i].v && p < mapas[i].v + mapas[i].tam)
			return mapas[i].f + (p - mapas[i].v);
	{
		static int dicho;
		if (!dicho++)
			hl_log(0, "DMA de una memoria que no es del montón (%p): la tarjeta no la verá\n", v);
	}
	return 0;
}

/* ==================================================== las redes recordadas */
#define MAX_RECORDADAS 16
static struct recordada {
	u8 ssid[33], ssid_n;
	u8 pmk[32];
	u8 abierta, autom;
} rec[MAX_RECORDADAS];
static int n_rec;
static struct aes llave;
static int hay_llave;

static void pon_llave(void)
{
	u8 k[16];
	int i;

	if (hay_llave)
		return;
	if (lee_archivo("WIFI.LLV", k, 16) != 16) {
		/* la primera vez: al azar (el contador de ciclos, los tics) */
		for (i = 0; i < 16; i++) {
			u32 t0 = hl_azar();
			sc(SC_DORMIR, 0, 0, 0);
			k[i] = (u8)(t0 ^ (hl_azar() >> 8) ^ hl_pentium_tics());
		}
		escribe_archivo("WIFI.LLV", k, 16, 0);
		hl_log(2, "WIFI.LLV: llave nueva para cifrar las claves de WIFI.INI\n");
	}
	aes_clave(&llave, k);
	memset(k, 0, sizeof k);
	hay_llave = 1;
}

static int hex(int c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	c |= 32;
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	return -1;
}

static int de_hex(const char *s, u8 *d, int max)
{
	int n = 0;

	while (n < max && hex(s[0]) >= 0 && hex(s[1]) >= 0) {
		d[n++] = hex(s[0]) * 16 + hex(s[1]);
		s += 2;
	}
	return n;
}

static char *a_hex(char *d, const u8 *s, int n)
{
	static const char c[] = "0123456789ABCDEF";
	while (n--) {
		*d++ = c[*s >> 4];
		*d++ = c[*s++ & 15];
	}
	*d = 0;
	return d;
}

/* WIFI.INI: una línea «Red=AUTO,nombre en hexadecimal,clave cifrada en
 * hexadecimal» por red (la clave, vacía: abierta), con un comentario
 * delante con el nombre a la vista */
static void lee_recordadas(void)
{
	static char b[6000];
	int n = lee_archivo("WIFI.INI", b, sizeof b - 1), i;
	char *l, *s;

	n_rec = 0;
	if (n <= 0)
		return;
	b[n] = 0;
	pon_llave();
	for (l = b; *l && n_rec < MAX_RECORDADAS; ) {
		char *fin = l;
		while (*fin && *fin != '\n')
			fin++;
		if (*fin)
			*fin++ = 0;
		if ((l[0] | 32) == 'r' && (l[1] | 32) == 'e' && (l[2] | 32) == 'd' && l[3] == '=') {
			struct recordada *r = &rec[n_rec];
			u8 c[32];
			memset(r, 0, sizeof *r);
			s = l + 4;
			r->autom = *s == '1';
			while (*s && *s != ',')
				s++;
			if (*s == ',')
				s++;
			r->ssid_n = de_hex(s, r->ssid, 32);
			r->ssid[r->ssid_n] = 0;
			while (*s && *s != ',')
				s++;
			if (*s == ',')
				s++;
			if (de_hex(s, c, 32) == 32) {
				aes_descifra(&llave, c, r->pmk);
				aes_descifra(&llave, c + 16, r->pmk + 16);
			} else {
				r->abierta = 1;
			}
			memset(c, 0, sizeof c);
			if (r->ssid_n)
				n_rec++;
		}
		l = fin;
	}
	hl_log(2, "WIFI.INI: %d redes recordadas\n", n_rec);
	for (i = 0; i < n_rec; i++)
		hl_log(2, "  «%s»%s%s\n", rec[i].ssid, rec[i].abierta ? " (abierta)" : " (WPA2)", rec[i].autom ? ", se conecta sola" : "");
}

static void guarda_recordadas(void)
{
	static char b[6000];
	char *p = b;
	int i, k;

	pon_llave();
	p += snprintf(p, 400, "; WIFI.INI: las redes Wi-Fi que recuerda Hispa-Pentium (WIFI.BIN).\r\n"
		      "; No se guarda la contrase\xa4""a: se guarda la clave ya preparada de la red,\r\n"
		      "; cifrada con la llave de este equipo (WIFI.LLV). No tocar a mano.\r\n");
	for (i = 0; i < n_rec; i++) {
		struct recordada *r = &rec[i];
		u8 c[32];
		p += snprintf(p, 60, "; ");
		for (k = 0; k < r->ssid_n; k++)
			*p++ = (r->ssid[k] >= 32 && r->ssid[k] < 127) ? r->ssid[k] : '?';
		p += snprintf(p, 60, "\r\nRed=%d,", r->autom ? 1 : 0);
		p = a_hex(p, r->ssid, r->ssid_n);
		*p++ = ',';
		if (!r->abierta) {
			aes_cifra(&llave, r->pmk, c);
			aes_cifra(&llave, r->pmk + 16, c + 16);
			p = a_hex(p, c, 32);
			memset(c, 0, sizeof c);
		}
		p += snprintf(p, 4, "\r\n");
	}
	if (escribe_archivo("WIFI.INI", b, p - b, 0) < 0)
		hl_log(0, "no se puede escribir WIFI.INI\n");
	memset(b, 0, sizeof b);
}

static struct recordada *busca_rec(const u8 *ssid, int n)
{
	int i;
	for (i = 0; i < n_rec; i++)
		if (rec[i].ssid_n == n && memcmp(rec[i].ssid, ssid, n) == 0)
			return &rec[i];
	return NULL;
}

static void recuerda(const u8 *ssid, int n, const u8 *pmk, int autom)
{
	struct recordada *r = busca_rec(ssid, n);

	if (!r) {
		if (n_rec == MAX_RECORDADAS) {
			memmove(&rec[0], &rec[1], sizeof(rec[0]) * (MAX_RECORDADAS - 1));
			n_rec--;
		}
		r = &rec[n_rec++];
	}
	memset(r, 0, sizeof *r);
	memcpy(r->ssid, ssid, n);
	r->ssid_n = n;
	if (pmk)
		memcpy(r->pmk, pmk, 32);
	r->abierta = pmk == NULL;
	r->autom = autom;
	guarda_recordadas();
}

static void olvida(const u8 *ssid, int n)
{
	struct recordada *r = busca_rec(ssid, n);
	if (!r)
		return;
	memmove(r, r + 1, (char *)&rec[n_rec] - (char *)(r + 1));
	n_rec--;
	guarda_recordadas();
}

/* ======================================================= la cola para RED */
#define NCOLA 32
static u8 cola[NCOLA][1514];
static u16 cola_n[NCOLA];
static int cola_cab, cola_fin;
static u32 perdidas;

void w_entrega_eth(const o8 *t, int n)
{
	int sig = (cola_fin + 1) % NCOLA;

	if (sig == cola_cab || n > 1514) {
		perdidas++;
		return;
	}
	memcpy(cola[cola_fin], t, n);
	cola_n[cola_fin] = n;
	cola_fin = sig;
}

/* ============================================= lo que se le dice a quien mira */
static char aviso[96];
static void avisa(const char *f, ...)
{
	char t[200];
	va_list ap;
	va_start(ap, f);
	vsnprintf(t, sizeof t, f, ap);
	va_end(ap);
	a437(aviso, t, sizeof aviso);
}

/* ===================================================== conectar, a trozos */
static struct pbkdf2 pb;
static int calculando;			/* preparando la clave (PBKDF2) */
static u8 c_ssid[33], c_ssid_n, c_autom, c_pmk[32];
static int c_nueva;			/* conectando a una red que aún no está en WIFI.INI */
static int a_mano;			/* «Desconectar»: no conectarse sola hasta que se pida */
static u32 t_auto;			/* cuándo se mira otra vez si hay una red recordada a la vista */

static struct w_red *red_vista(const u8 *ssid, int n)
{
	struct w_red *l, *m = NULL;
	int k = w80211_redes(&l), i;

	for (i = 0; i < k; i++)
		if (l[i].ssid_n == n && memcmp(l[i].ssid, ssid, n) == 0 && (!m || l[i].senal > m->senal))
			m = &l[i];
	return m;
}

static void empieza(const u8 *ssid, int n, const u8 *pmk, int nueva, int autom)
{
	memcpy(c_ssid, ssid, n);
	c_ssid[n] = 0;
	c_ssid_n = n;
	c_nueva = nueva;
	c_autom = autom;
	if (pmk)
		memcpy(c_pmk, pmk, 32);
	a_mano = 0;
	w80211_conectar(ssid, n, pmk);
	avisa("Conectando con «%s»...", c_ssid);
}

/* la mejor red recordada (que se conecte sola) que se vea */
static void conecta_sola(void)
{
	struct w_info i;
	struct recordada *m = NULL;
	int k, mejor = -1000;

	if (a_mano || calculando || !listo || w80211_barriendo())
		return;
	w80211_estado(&i);
	if (i.estado != W_PARADO)
		return;
	/* si ya se está intentando una que se ve, se deja a w80211.c */
	if (i.ssid_n && i.fallo != W_F_NO_ESTA && red_vista(i.ssid, i.ssid_n))
		return;
	for (k = 0; k < n_rec; k++) {
		struct w_red *v;
		if (!rec[k].autom)
			continue;
		v = red_vista(rec[k].ssid, rec[k].ssid_n);
		if (v && v->senal > mejor && (v->seguridad == W_SEG_WPA2 || (v->seguridad == W_SEG_ABIERTA && rec[k].abierta))) {
			mejor = v->senal;
			m = &rec[k];
		}
	}
	if (!m) {
		if (!i.ssid_n && (int)(hl_pentium_tics() - t_auto) >= 0 && n_rec) {
			/* ninguna a la vista: se busca otra vez de vez en cuando */
			t_auto = hl_pentium_tics() + 30 * hz;
			w80211_barrer();
		}
		return;
	}
	if (i.ssid_n == m->ssid_n && memcmp(i.ssid, m->ssid, m->ssid_n) == 0)
		return;
	hl_log(2, "Se conecta sola a la red recordada «%s» (%d dBm)\n", m->ssid, mejor);
	empieza(m->ssid, m->ssid_n, m->abierta ? NULL : m->pmk, 0, 1);
}

/* lo que pasa con la conexión (cada vuelta) */
static void sigue_conexion(void)
{
	static int visto = -1, fallo_visto = -1;
	struct w_info i;

	if (calculando) {
		if (pbkdf2_sigue(&pb, 256, c_pmk)) {
			calculando = 0;
			memset(&pb, 0, sizeof pb);
			hl_log(2, "Clave de «%s» preparada\n", c_ssid);
			empieza(c_ssid, c_ssid_n, c_pmk, 1, c_autom);
		}
		return;
	}
	if (!listo)
		return;
	w80211_estado(&i);
	if (i.estado == visto && i.fallo == fallo_visto)
		return;
	visto = i.estado;
	fallo_visto = i.fallo;
	if (i.estado == W_CONECTADO) {
		avisa("Conectado a «%s».", i.ssid);
		if (c_ssid_n && i.ssid_n == c_ssid_n && memcmp(i.ssid, c_ssid, c_ssid_n) == 0 && c_nueva) {
			/* la primera vez que conecta: se recuerda */
			struct w_red *v = red_vista(c_ssid, c_ssid_n);
			recuerda(c_ssid, c_ssid_n, v && v->seguridad == W_SEG_ABIERTA ? NULL : c_pmk, c_autom);
			c_nueva = 0;
			hl_log(2, "La red «%s» queda recordada en WIFI.INI%s\n", c_ssid, c_autom ? " (se conectará sola)" : "");
		}
		vuelca_registro();
		return;
	}
	if (i.estado == W_PARADO && i.fallo != W_F_NADA) {
		static const char *const por[] = {
			"", "la red no se ve", "el punto de acceso no contesta", "el punto de acceso no deja entrar",
			"el punto de acceso no contesta a la asociación", "el punto de acceso no acepta la asociación",
			"la contraseña no es la de esta red", "la red usa una seguridad que no se admite (hace falta WPA2-Personal con AES)",
			"el punto de acceso nos ha echado", "se ha perdido la red"
		};
		avisa("No se conecta a «%s»: %s.", i.ssid, i.fallo < 10 ? por[i.fallo] : "?");
		if (c_nueva && (i.fallo == W_F_CLAVE || i.fallo == W_F_SEGURIDAD)) {
			/* una red nueva con la contraseña mal: no se insiste */
			w80211_desconectar();
			c_nueva = 0;
			a_mano = 1;
			hl_log(1, "No se recuerda «%s»: %s\n", c_ssid, por[i.fallo]);
		}
		vuelca_registro();
	}
}

/* ==================================================== RED.BIN: las tramas */
static u8 cambio[MAX_DATOS];
static int ultimo_enlace = -1;
static int con_red;			/* RED.BIN ya sabe de nosotros */

static int senal_ahora(void)
{
	struct w_info i;
	if (!listo)
		return 0;
	w80211_estado(&i);
	return i.senal;
}

static int enlace_ahora(void)
{
	struct w_info i;
	if (!listo)
		return 0;
	w80211_estado(&i);
	return i.estado == W_CONECTADO;
}

static int ponerse_en_red(void)
{
	u32 cab[8];
	u8 mac[8];
	int n = strlen(nombre_tarjeta);

	memset(cab, 0, sizeof cab);
	memset(mac, 0, sizeof mac);
	if (la_radio)
		memcpy(mac, la_radio->mac, 6);
	cab[0] = O_WIFI_PONER;
	memcpy(&cab[1], mac, 8);
	cab[3] = aparato;
	cab[4] = aparato != 0xFFFFFFFF ? hl_pci_lee(aparato >> 16, (aparato >> 8) & 0xff, 0) : 0;
	cab[7] = n;
	memcpy(cambio, nombre_tarjeta, n);
	return sc(SC_RED, (u32)cab, (u32)cambio, 0);
}

/* una vuelta con RED.BIN: lo que ha llegado y lo que hay que enviar. Da 1 si
 * hay que pararse (RED.BIN usa el cable) y 2 si ha habido tramas */
static int con_red_bin(void)
{
	u32 cab[8], r, n, k;
	int hubo = 0, e = enlace_ahora();

	memset(cab, 0, sizeof cab);
	cab[0] = O_WIFI_CAMBIO;
	cab[1] = e ? 1 : 0;
	cab[2] = (u32)senal_ahora();
	n = 0;
	while (cola_cab != cola_fin && n + 2 + cola_n[cola_cab] <= MAX_DATOS) {
		cambio[n] = cola_n[cola_cab];
		cambio[n + 1] = cola_n[cola_cab] >> 8;
		memcpy(cambio + n + 2, cola[cola_cab], cola_n[cola_cab]);
		n += 2 + cola_n[cola_cab];
		cola_cab = (cola_cab + 1) % NCOLA;
		hubo = 2;
	}
	cab[7] = n;
	r = sc(SC_RED, (u32)cab, (u32)cambio, MAX_DATOS);
	if (r == 0xFFFFFFEFu)			/* SIN_RED */
		return 1;
	ultimo_enlace = e;
	if (ES_ERROR(r))
		return 0;
	n = cab[7];
	for (k = 0; k + 2 <= n; ) {
		u32 l = cambio[k] | (cambio[k + 1] << 8);
		if (k + 2 + l > n || l < 14)
			break;
		if (listo && e)
			w80211_enviar_eth(cambio + k + 2, l);
		k += 2 + l;
		hubo = 2;
	}
	if (cab[1] & 1)
		return 1;
	return hubo;
}

/* ========================================= el núcleo: el icono y la tarjeta */
static void dilo(void)
{
	static u32 dicho = 0xFFFFFFFF, senal_dicha;
	struct w_info i;
	u32 e, b = 0;

	if (!listo) {
		e = WE_SIN_TARJETA;
		i.senal = 0;
	} else {
		w80211_estado(&i);
		if (i.estado == W_CONECTADO) {
			e = WE_CONECTADA;
			/* las barras, como WIFI REDES de Hispa-486 */
			b = i.senal >= -55 ? 4 : i.senal >= -67 ? 3 : i.senal >= -75 ? 2 : 1;
		} else if (i.estado != W_PARADO || calculando) {
			e = WE_CONECTANDO;
		} else {
			e = WE_SIN_CONECTAR;
		}
	}
	e |= b << 8;
	if (e == dicho && (u32)i.senal == senal_dicha)
		return;
	dicho = e;
	senal_dicha = i.senal;
	sc(SC_ESTADO, e, (u32)i.senal, aparato);
}

/* ======================================================== los pedidos */
static u8 pedido[64 + MAX_DATOS];
static u8 resp_b[MAX_DATOS];

static void contesta(const struct pedido *p, u32 r, u32 uno, const void *d, u32 n)
{
	struct respuesta re;

	if (p->hueco == 0xFFFFFFFF)
		return;
	memset(&re, 0, sizeof re);
	re.serie = p->serie;
	re.datos = (u32)d;
	re.cab[0] = r;
	re.cab[1] = uno;
	re.cab[7] = n > p->cabe ? p->cabe : n;
	sc(SC_CONTESTAR, p->hueco, (u32)&re, 0);
}

static void estado_ui(struct w_estado_ui *s)
{
	struct w_info i;

	memset(s, 0, sizeof *s);
	s->radio = listo;
	s->calculando = calculando;
	memcpy(s->tarjeta, nombre_tarjeta, sizeof s->tarjeta);
	memcpy(s->aviso, aviso, sizeof s->aviso);
	if (!listo)
		return;
	w80211_estado(&i);
	s->estado = i.estado;
	s->fallo = i.fallo;
	s->canal = i.canal;
	s->senal = i.senal;
	s->seguridad = i.seguridad;
	s->barriendo = i.barriendo;
	s->nredes = i.nredes;
	s->tx = i.tx;
	s->rx = i.rx;
	s->reconexiones = i.reconexiones;
	s->ssid_n = i.ssid_n;
	memcpy(s->ssid, i.ssid, 33);
	memcpy(s->mac, i.mac, 6);
	if (calculando) {
		s->ssid_n = c_ssid_n;
		memcpy(s->ssid, c_ssid, 33);
	}
	{
		struct recordada *r = s->ssid_n ? busca_rec(s->ssid, s->ssid_n) : NULL;
		s->autom = r ? r->autom : c_autom;
	}
}

static int conectar(const struct pedido *p, const u8 *d)
{
	u32 n = p->cab[1], nc = p->cab[2];
	struct w_red *v;
	struct recordada *r;
	char clave[64];

	if (!listo)
		return W_R_SIN_RADIO;
	if (n == 0 || n > 32 || n + nc > p->largo || nc > 64)
		return W_R_NO_SE_VE;
	v = red_vista(d, n);
	r = busca_rec(d, n);
	if (v && v->seguridad != W_SEG_ABIERTA && v->seguridad != W_SEG_WPA2)
		return W_R_NO_ADMITIDA;
	if (v && v->seguridad == W_SEG_ABIERTA) {
		empieza(d, n, NULL, 1, p->cab[3] & 1);
		return W_R_BIEN;
	}
	if (nc == 0) {
		if (r && !r->abierta) {
			if (r->autom != (p->cab[3] & 1)) {
				r->autom = p->cab[3] & 1;
				guarda_recordadas();
			}
			empieza(d, n, r->pmk, 0, r->autom);
			return W_R_BIEN;
		}
		return W_R_FALTA_CLAVE;
	}
	if (nc == 64) {
		/* las 64 cifras de la clave ya preparada, en vez de la contraseña */
		u8 k[32];
		memcpy(clave, d + n, 64);
		if (de_hex(clave, k, 32) == 32) {
			empieza(d, n, k, 1, p->cab[3] & 1);
			memset(k, 0, sizeof k);
			memset(clave, 0, sizeof clave);
			return W_R_BIEN;
		}
	}
	if (nc < 8 || nc > 63)
		return W_R_CLAVE_CORTA;
	memcpy(clave, d + n, nc);
	clave[nc] = 0;
	memcpy(c_ssid, d, n);
	c_ssid[n] = 0;
	c_ssid_n = n;
	c_autom = p->cab[3] & 1;
	pbkdf2_empieza(&pb, clave, d, n);
	memset(clave, 0, sizeof clave);
	calculando = 1;
	w80211_desconectar();
	avisa("Preparando la clave de «%s»...", c_ssid);
	hl_log(2, "Se pide conectar con «%s» (con contraseña: se prepara la clave)\n", c_ssid);
	return W_R_BIEN;
}

static void atiende(struct pedido *p, u8 *d)
{
	switch (p->cab[0]) {
	case AVISO_ACABO:
		return;
	case WO_ESTADO: {
		struct w_estado_ui s;
		estado_ui(&s);
		contesta(p, 0, 0, &s, sizeof s);
		return;
	}
	case WO_BUSCAR:
		if (listo && !w80211_barriendo())
			w80211_barrer();
		contesta(p, listo ? 0 : W_R_SIN_RADIO, 0, NULL, 0);
		return;
	case WO_REDES: {
		struct w_red *l;
		struct w_red_ui *u = (struct w_red_ui *)resp_b;
		struct w_info i;
		int n = listo ? w80211_redes(&l) : 0, k, m = 0;
		memset(&i, 0, sizeof i);
		if (listo)
			w80211_estado(&i);
		for (k = 0; k < n && (m + 1) * (int)sizeof(*u) <= (int)sizeof resp_b; k++) {
			struct recordada *r = busca_rec(l[k].ssid, l[k].ssid_n);
			int j, ya = 0;
			/* una red con varios puntos de acceso: una vez, con el que mejor se oye */
			for (j = 0; j < m; j++)
				if (u[j].ssid_n == l[k].ssid_n && memcmp(u[j].ssid, l[k].ssid, l[k].ssid_n) == 0) {
					ya = 1;
					if (l[k].senal > u[j].senal) {
						u[j].senal = l[k].senal;
						u[j].canal = l[k].canal;
						memcpy(u[j].bssid, l[k].bssid, 6);
					}
				}
			if (ya)
				continue;
			memset(&u[m], 0, sizeof u[m]);
			u[m].canal = l[k].canal;
			u[m].senal = l[k].senal;
			u[m].seguridad = l[k].seguridad;
			u[m].ssid_n = l[k].ssid_n;
			memcpy(u[m].ssid, l[k].ssid, 32);
			memcpy(u[m].bssid, l[k].bssid, 6);
			if (r)
				u[m].marcas |= WR_RECORDADA | (r->autom ? WR_AUTO : 0);
			if (i.estado == W_CONECTADO && i.ssid_n == l[k].ssid_n && memcmp(i.ssid, l[k].ssid, i.ssid_n) == 0)
				u[m].marcas |= WR_CONECTADA;
			m++;
		}
		/* la de más señal, arriba */
		for (k = 1; k < m; k++) {
			struct w_red_ui t = u[k];
			int j = k - 1;
			while (j >= 0 && u[j].senal < t.senal) {
				u[j + 1] = u[j];
				j--;
			}
			u[j + 1] = t;
		}
		contesta(p, m, 0, u, m * sizeof(*u));
		return;
	}
	case WO_CONECTAR:
		contesta(p, conectar(p, d), 0, NULL, 0);
		memset(d, 0, p->largo);
		return;
	case WO_DESCONECTAR:
		calculando = 0;
		if (listo)
			w80211_desconectar();
		a_mano = 1;
		c_nueva = 0;
		avisa("Desconectado.");
		hl_log(2, "Desconectado (a mano)\n");
		contesta(p, 0, 0, NULL, 0);
		return;
	case WO_OLVIDAR: {
		struct w_info i;
		u32 n = p->largo > 32 ? 32 : p->largo;
		if (listo) {
			w80211_estado(&i);
			if (i.ssid_n == n && memcmp(i.ssid, d, n) == 0) {
				w80211_desconectar();
				a_mano = 1;
			}
		}
		olvida(d, n);
		avisa("Red olvidada.");
		contesta(p, 0, 0, NULL, 0);
		return;
	}
	case WO_AUTO: {
		u32 n = p->largo > 32 ? 32 : p->largo;
		struct recordada *r = busca_rec(d, n);
		if (r) {
			r->autom = p->cab[1] & 1;
			guarda_recordadas();
		}
		if (c_ssid_n == n && memcmp(c_ssid, d, n) == 0)
			c_autom = p->cab[1] & 1;
		contesta(p, r ? 0 : 1, 0, NULL, 0);
		return;
	}
	case WO_REGISTRO: {
		/* lo que queda por volcar va antes a WIFI.LOG; aquí, el resumen */
		struct w_estado_ui s;
		int n;
		estado_ui(&s);
		n = snprintf((char *)resp_b, sizeof resp_b,
			     "WIFI.BIN %s: %s; radio %s; estado %u, fallo %u, red «%s», canal %u, señal %d dBm; "
			     "enviadas %u, recibidas %u, reconexiones %u, perdidas (cola) %u; registro en %sWIFI.LOG\r\n",
			     VERSION, nombre_tarjeta, listo ? "en marcha" : "PARADA", s.estado, s.fallo, s.ssid, s.canal, s.senal,
			     s.tx, s.rx, s.reconexiones, perdidas, carpeta);
		vuelca_registro();
		{
			static char t437[sizeof resp_b];
			a437(t437, (char *)resp_b, sizeof t437);
			n = strlen(t437);
			contesta(p, n, 0, t437, n);
		}
		return;
	}
	}
	contesta(p, 0xFFFFFFF9u, 0, NULL, 0);
}

/* ============================================== buscar y encender la tarjeta */
static void busca_aparato(void)
{
	int bus, df, i;

	for (bus = 0; bus < 256; bus++)
		for (df = 0; df < 256; df++) {
			u32 id = hl_pci_lee(bus, df, 0);
			if (id == 0xffffffff || (id & 0xffff) == 0xffff) {
				if (!(df & 7))
					df += 7;
				continue;
			}
			if ((id & 0xffff) == 0x14e4)
				for (i = 0; ids_brcm[i]; i++)
					if ((id >> 16) == ids_brcm[i])
						goto hallado;
			if ((id & 0xffff) == 0x168c)
				for (i = 0; ids_ath[i]; i++)
					if ((id >> 16) == ids_ath[i])
						goto hallado;
			if ((hl_pci_lee(bus, df, 8) >> 24) == 0x02)
				hl_log(2, "PCI %02x:%02x.%d: %04X:%04X (red; clase %06X)\n", bus, df >> 3, df & 7,
				       id & 0xffff, id >> 16, hl_pci_lee(bus, df, 8) >> 8);
			if (!(df & 7) && !(hl_pci_lee(bus, df, 0x0c) & 0x800000))
				df += 7;
			continue;
hallado:
			aparato = (bus << 16) | (df << 8);
			return;
		}
}

/* ¿ha dejado el firmware la protección de DMA de Intel (VT-d) cerrada? (como
 * mira_vtd de wifi/nucleo/nucleo.c: solo mira) */
static void mira_vtd(void)
{
	u32 id = hl_pci_lee(0, 0, 0), mch, *m, *u, bar, i;
	static const u16 donde[2] = { 0x5400, 0x5410 };

	if ((id & 0xffff) != 0x8086 || (hl_pci_lee(0, 0, 8) >> 16) != 0x0600)
		return;
	mch = hl_pci_lee(0, 0, 0x48);
	if (hl_pci_lee(0, 0, 0x4c) || !(mch & 1) || (mch & 0x7ffe) || (mch & ~0x7fffu) < 0x100000)
		return;
	m = hl_mapea(mch & ~0x7fffu, 0x8000);
	if ((u8 *)m == basura)
		return;
	for (i = 0; i < 2; i++) {
		bar = m[donde[i] / 4];
		if (!(bar & 1) || m[donde[i] / 4 + 1] || (bar & ~0xfffu) < 0x100000)
			continue;
		u = hl_mapea(bar & ~0xfffu, 0x1000);
		if ((u8 *)u == basura || u[0] < 0x10 || u[0] > 0x7f || u[2] == 0xffffffff || !(u[2] & 0x1f00))
			continue;
		hl_log(2, "VT-d: unidad %s en %08X: traducción de DMA %s\n", i ? "de los aparatos" : "de la gráfica",
		       bar & ~0xfffu, (u[0x1c / 4] & 0x80000000) ? "ENCENDIDA" : "apagada");
		if (i && (u[0x1c / 4] & 0x80000000))
			hl_log(0, "VT-d: el firmware ha dejado cerrada la DMA: la tarjeta no podrá recibir ni enviar\n");
	}
}

static int carga_fw(int i, const char *archivo, const char *nombre)
{
	char r[40];
	u32 m, n, l;
	u8 *b;

	ruta(r, archivo);
	m = sc(SC_ABRIR, (u32)r, 0, 0);
	if (ES_ERROR(m)) {
		hl_log(1, "No está %s (el microcódigo de las Broadcom)\n", r);
		return -1;
	}
	l = sc(SC_LARGO, m, 0, 0);
	b = hl_malloc(l + 16);
	n = b ? sc(SC_LEER, m, (u32)b, l) : 0;
	sc(SC_CERRAR, m, 0, 0);
	if (!b || n != l) {
		hl_log(0, "No se puede leer %s\n", r);
		return -1;
	}
	hl_fw_pon(i, nombre, b, l);
	hl_log(2, "%s: %u octetos\n", r, l);
	return 0;
}

static int arranca(void)
{
	u32 r, t, u, pk;
	int k, e = 1;

	r = hl_tiempo_inicia();
	if ((int)r < 0) {
		hl_log(0, "este procesador no tiene contador de tiempo (TSC) (%d)\n", (int)r);
		return -1;
	}
	hl_log(2, "Procesador: %u MHz (TSC)\n", hl_tsc_khz() / 1000);
	if (simulada) {
		hl_log(2, "RADIO SIMULADA: red «%s», canal 6 (para probar sin la tarjeta)\n", "HispaPrueba");
		sim_configura("HispaPrueba", "clave de prueba", 6);
		la_radio = sim_radio();
		strcpy(nombre_tarjeta, "Wi-Fi simulada (pruebas)");
		w80211_inicia(la_radio);
		listo = 1;
		return 0;
	}
#ifdef CHIP_SIMULADO
	hl_log(2, "NÚCLEO DE PRUEBAS: tarjeta BCM43224 simulada, red «HispaPrueba» en el canal 6\n");
	sim_configura("HispaPrueba", "clave de prueba", 6);
	ap_sim = sim_radio();
	tarjeta_sim_monta(ap_sim);
#endif
	busca_aparato();
	mira_vtd();
	for (k = 0; k < (int)ARRAY_SIZE(conductores) && e == 1; k++) {
		e = conductores[k].busca();
		if (e == 0)
			cond = &conductores[k];
	}
	vuelca_registro();
	if (e == 1) {
		hl_log(0, "no hay ninguna tarjeta Wi-Fi de las admitidas:\n");
		for (k = 0; k < (int)ARRAY_SIZE(conductores); k++)
			hl_log(0, "  %s\n", conductores[k].familia);
		return -1;
	}
	if (e < 0)
		return -1;
	hl_log(2, "Se usa la radio de las %s\n", cond->familia);
	hl_log(2, "PASO 1 de 3 hecho: tarjeta encontrada. Sigue el 2: despertarla, leer sus datos de fábrica y montar el controlador\n");
	vuelca_registro();
	if (cond->busca == brcm_busca) {
		carga_fw(0, "BCM43XX.FW", "brcm/bcm43xx-0.fw");
		carga_fw(1, "BCM43XXH.FW", "brcm/bcm43xx_hdr-0.fw");
	}
	e = cond->monta(&la_radio);
	hl_monton_info(&t, &u, &pk);
	if (e) {
		hl_log(0, "PASO 2 de 3: el controlador no se ha podido montar (%d)\n", e);
		return -1;
	}
	hl_log(2, "PASO 2 de 3 hecho: controlador montado (montón: %u KB). Sigue el 3: encender la radio\n", u >> 10);
	vuelca_registro();
	e = cond->enciende();
	if (e < 0) {
		hl_log(0, "PASO 3 de 3: la radio no se enciende (%d)\n", e);
		return -1;
	}
	if (la_radio->nombre)
		snprintf(nombre_tarjeta, sizeof nombre_tarjeta, "%s", la_radio->nombre);
#ifdef CHIP_SIMULADO
	canal_real = la_radio->canal; la_radio->canal = canal_sim;
	sondear_real = la_radio->sondear; la_radio->sondear = sondear_sim;
#endif
	w80211_inicia(la_radio);
	listo = 1;
	hl_monton_info(&t, &u, &pk);
	if (hl_monton_repasa() < 0)
		return -1;
	hl_log(2, "PASO 3 de 3 hecho: radio en marcha (montón: %u KB, pico %u KB de %u)\n", u >> 10, pk >> 10, t >> 10);
	return 0;
}

static void para(void)
{
	if (listo && la_radio->parar)
		la_radio->parar();
	listo = 0;
	if (con_red) {
		u32 cab[8];
		memset(cab, 0, sizeof cab);
		cab[0] = O_WIFI_QUITAR;
		sc(SC_RED, (u32)cab, 0, 0);
	}
}

/* ================================================================ la entrada */
int servicio_wifi(void)
{
	char args[128];
	u32 r, m, n, ultimo_volcado, ultimo_tic;
	int quietas = 0;

	sc(SC_CORTE, 1, 0, 0);
	n = sc(SC_ARGUMENTOS, (u32)args, sizeof args - 1, 0);
	args[ES_ERROR(n) || n >= sizeof args ? 0 : n] = 0;
	{
		char *a;
		for (a = args; *a; a++)
			if ((a[0] == '/' || a[0] == '-') && (a[1] | 32) == 's' && (a[2] | 32) == 'i' && (a[3] | 32) == 'm')
				simulada = 1;
	}
	if (sc(SC_APUNTARSE, 0, 0, 0) != 0)
		return 1;
	hz = sc(SC_DATO, DATO_HZ, 0, 0);
	if (!hz || ES_ERROR(hz))
		hz = 100;
	m = sc(SC_ABRIR, (u32)"\\HISPA", 0, 0);
	if (!ES_ERROR(m)) {
		sc(SC_CERRAR, m, 0, 0);
		strcpy(carpeta, "\\HISPA\\");
	}
	escribe_archivo("WIFI.LOG", "", 0, 0);		/* (nuevo en cada arranque) */
	/* el montón del controlador: 2 MB de memoria física seguida */
	r = sc(SC_DMA, DMA_PAGINAS, (u32)&dma_f, 0);
	if (ES_ERROR(r)) {
		hl_log(0, "no hay 2 MB de memoria física seguida para la tarjeta (%08X)\n", r);
		vuelca_registro();
		return 2;
	}
	dma_v = (u8 *)r;
	dma_tam = DMA_PAGINAS * 4096;
	hl_monton(dma_v, dma_tam);
	hl_log(2, "Hispa-Pentium: el servicio Wi-Fi (WIFI.BIN %s, %s)\n", VERSION, __DATE__);
	hl_log(2, "Memoria para la tarjeta: %u KB en %08X (se ve en %08X)\n", dma_tam >> 10, dma_f, (u32)dma_v);
	lee_recordadas();
	vuelca_registro();
	if (arranca() < 0) {
		hl_log(0, "El Wi-Fi no se ha podido poner en marcha. Mande el archivo %sWIFI.LOG para arreglarlo.\n", carpeta);
		listo = 0;
		dilo();
		vuelca_registro();
		/* se queda para contestar a la ventana («sin tarjeta») */
	} else {
		w80211_barrer();
	}
	cada_linea = 0;
	vuelca_registro();
	r = ponerse_en_red();
	con_red = !ES_ERROR(r);
	if (!con_red)
		hl_log(1, "RED.BIN no contesta (%08X)\n", r);
	dilo();
	ultimo_volcado = ultimo_tic = hl_pentium_tics();
	for (;;) {
		struct pedido *p = (struct pedido *)pedido;
		int hubo = 0, k;

		if (listo) {
			la_radio->sondear();
			w80211_tic();
		}
		sigue_conexion();
		conecta_sola();
		if (con_red) {
			k = con_red_bin();
			if (k == 1) {
				hl_log(2, "RED.BIN usa el cable: el Wi-Fi se para\n");
				break;
			}
			hubo |= k;
		}
		dilo();
		/* los pedidos de la ventana; si no hay nada que hacer, se espera un tic */
		if (calculando || hubo || cola_cab != cola_fin)
			quietas = 0;
		else
			quietas++;
		if (sc(SC_SIGUIENTE, (u32)p, quietas > 2 ? 1 : 0, 0) == 1)
			atiende(p, pedido + sizeof(struct pedido));
		if (hl_pentium_tics() - ultimo_volcado >= hz) {
			ultimo_volcado = hl_pentium_tics();
			vuelca_registro();
		}
	}
	para();
	vuelca_registro();
	return 0;
}
