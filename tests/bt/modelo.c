/* modelo.c - (BLUETOOTH) un adaptador Bluetooth de mentira (lo que hace un
 * controlador HCI según la norma, Core 5.3 volumen 4 parte E) con cinco
 * aparatos al otro lado, cada uno con su L2CAP, su SDP y su HID: un teclado
 * (SSP, el código se teclea en él), un ratón (Just Works, sin protocolo de
 * arranque: su descriptor), un teléfono (comparación numérica), un teclado
 * antiguo (el PIN) y un ratón BLE (solo anuncia). También hace de Intel:
 * el 8087:0A2A (de ROM) y el 8087:0AAA en su cargador, que pide su firmware
 * por «secure send» por el extremo a granel. Lo que el anfitrión hace mal
 * (una orden con el largo mal, un canal que no existe, una clave que no es)
 * se cuenta en mod_fallos y se dice por stderr.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "modelo.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

#define DE_INT 1
#define DE_GRANEL 2

int mod_fallos, mod_firmware_bien, mod_ddc, mod_parche;
u32 mod_ordenes;
static int clase_ad;
static u32 ahora;
static const u8 mi_dir[6] = { 0x21, 0x3F, 0x55, 0x44, 0x33, 0x22 };
static int scan_enable, le_scan, ssp_mode;
static int cargador, arrancado;		/* (Intel 0AAA) */
static u8 *fw_esperado;
static int fw_n, fw_llegado;
static u8 fw_rec[2 * 1024 * 1024];
static int fw_tipo_visto;

static void fallo(const char *t, int v)
{
	mod_fallos++;
	fprintf(stderr, "modelo: FALLO: %s (%d)\n", t, v);
}

static u16 le16(const u8 *p) { return p[0] | (p[1] << 8); }
static void pon16(u8 *p, u16 v) { p[0] = v, p[1] = v >> 8; }

/* ================================================== lo que sale al anfitrión */
#define MAX_SALE 512
static struct { u8 de; u16 n, pos; u8 d[1100]; } sale_cola[MAX_SALE];
static int s_ini, s_fin;

static void sale(int de, const u8 *d, int n)
{
	int k = s_fin;

	if ((s_fin + 1) % MAX_SALE == s_ini) {
		fallo("la cola de salida del modelo se ha llenado", n);
		return;
	}
	sale_cola[k].de = de, sale_cola[k].n = n, sale_cola[k].pos = 0;
	memcpy(sale_cola[k].d, d, n);
	s_fin = (s_fin + 1) % MAX_SALE;
}

int mod_saca(u8 *d, int cabe, int *de, int pi, int pg)
{
	int k, n, max;

	if (s_ini == s_fin)
		return 0;
	k = s_ini;
	*de = sale_cola[k].de;
	max = *de == DE_INT ? pi : pg;
	n = sale_cola[k].n - sale_cola[k].pos;
	if (n > max)
		n = max;
	if (n > cabe)
		n = cabe;
	memcpy(d, sale_cola[k].d + sale_cola[k].pos, n);
	sale_cola[k].pos += n;
	if (sale_cola[k].pos >= sale_cola[k].n)
		s_ini = (s_ini + 1) % MAX_SALE;
	return n;
}

static int sucesos_por_granel;

static void ev(int code, const u8 *p, int n)
{
	u8 b[260];

	b[0] = code, b[1] = n;
	memcpy(b + 2, p, n);
	sale(sucesos_por_granel ? DE_GRANEL : DE_INT, b, n + 2);
}

static void cc(u16 op, const u8 *r, int n)
{
	u8 p[260];

	p[0] = 1;
	pon16(p + 1, op);
	memcpy(p + 3, r, n);
	ev(0x0E, p, 3 + n);
}

static void cc0(u16 op)
{
	u8 z = 0;

	cc(op, &z, 1);
}

static void cs(u16 op, int st)
{
	u8 p[4] = { st, 1, op, op >> 8 };

	ev(0x0F, p, 4);
}

/* ============================================================ los aparatos */
static const u8 desc_teclado[] = {
	0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x85, 0x01, 0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7, 0x15, 0x00,
	0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02, 0x95, 0x01, 0x75, 0x08, 0x81, 0x01, 0x95, 0x05,
	0x75, 0x01, 0x05, 0x08, 0x19, 0x01, 0x29, 0x05, 0x91, 0x02, 0x95, 0x01, 0x75, 0x03, 0x91, 0x01,
	0x95, 0x06, 0x75, 0x08, 0x15, 0x00, 0x25, 0x65, 0x05, 0x07, 0x19, 0x00, 0x29, 0x65, 0x81, 0x00,
	0xC0
};
/* un ratón con su número de informe (2): tres botones, X, Y y la rueda, de 8 bits */
static const u8 desc_raton[] = {
	0x05, 0x01, 0x09, 0x02, 0xA1, 0x01, 0x85, 0x02, 0x09, 0x01, 0xA1, 0x00, 0x05, 0x09, 0x19, 0x01,
	0x29, 0x03, 0x15, 0x00, 0x25, 0x01, 0x95, 0x03, 0x75, 0x01, 0x81, 0x02, 0x95, 0x01, 0x75, 0x05,
	0x81, 0x01, 0x05, 0x01, 0x09, 0x30, 0x09, 0x31, 0x09, 0x38, 0x15, 0x81, 0x25, 0x7F, 0x75, 0x08,
	0x95, 0x03, 0x81, 0x06, 0xC0, 0xC0
};

#define MAX_CANAL 4
struct canal_r { u16 psm, mio, suyo; int estado, conf_mia, conf_suya; };
static struct remoto {
	const char *nombre;
	u8 dir[6];
	u32 clase;
	int io, ssp, hid, boot, le, eir;
	const u8 *desc;
	int desc_n;
	int encendido, emparejable;
	u8 clave[16];
	int con_clave;
	/* la conexión */
	int conectado, cifrado, sale_el;	/* sale_el: la ha pedido él (se ha despertado) */
	u16 handle;
	struct canal_r c[MAX_CANAL];
	int protocolo;
	u32 codigo;
	int esperando, confirma_host, confirma_el, emparejando;
	u8 ens[1024];
	int ens_n, ens_quiere;
	u16 cid_sig;
} rem[N_REMOTOS];

static struct remoto *por_dir(const u8 *d)
{
	int i;

	for (i = 0; i < N_REMOTOS; i++)
		if (!memcmp(rem[i].dir, d, 6))
			return &rem[i];
	return 0;
}

static struct remoto *por_handle(u16 h)
{
	int i;

	for (i = 0; i < N_REMOTOS; i++)
		if (rem[i].conectado && rem[i].handle == h)
			return &rem[i];
	return 0;
}

/* ============================================================ los relojes */
#define MAX_PEND 64
#define P_INQ_RESULT 1
#define P_INQ_FIN 2
#define P_CONN 3
#define P_CONN_FALLA 4
#define P_LE 5
static struct { int usado, que, r; u32 cuando; } pend[MAX_PEND];
static u16 siguiente_handle = 0x0B;

static void programa(int que, int r, u32 dentro)
{
	int i;

	for (i = 0; i < MAX_PEND; i++)
		if (!pend[i].usado) {
			pend[i].usado = 1, pend[i].que = que, pend[i].r = r, pend[i].cuando = ahora + dentro;
			return;
		}
	fallo("demasiados relojes en el modelo", que);
}

static void quita(int que)
{
	int i;

	for (i = 0; i < MAX_PEND; i++)
		if (pend[i].usado && pend[i].que == que)
			pend[i].usado = 0;
}

static void conn_complete(struct remoto *r, int st)
{
	u8 p[11];

	p[0] = st;
	if (!st) {
		r->handle = siguiente_handle++;
		r->conectado = 1;
		r->cifrado = 0;
		memset(r->c, 0, sizeof r->c);
		r->ens_n = r->ens_quiere = 0;
		r->protocolo = -1;
	}
	pon16(p + 1, st ? 0 : r->handle);
	memcpy(p + 3, r->dir, 6);
	p[9] = 1, p[10] = 0;
	ev(0x03, p, 11);
}

static void anuncio_le(struct remoto *r)
{
	u8 p[64];
	int n = 0, k;

	p[n++] = 0x02;				/* LE Advertising Report */
	p[n++] = 1;
	p[n++] = 0x00;				/* ADV_IND */
	p[n++] = 0x01;				/* dirección al azar */
	memcpy(p + n, r->dir, 6), n += 6;
	k = n++;
	p[n++] = 2, p[n++] = 0x01, p[n++] = 0x06;
	p[n++] = 3, p[n++] = 0x19, p[n++] = 0xC2, p[n++] = 0x03;	/* apariencia: ratón */
	p[n++] = 3, p[n++] = 0x03, p[n++] = 0x12, p[n++] = 0x18;	/* HID */
	p[n++] = 1 + strlen(r->nombre), p[n++] = 0x09;
	memcpy(p + n, r->nombre, strlen(r->nombre)), n += strlen(r->nombre);
	p[k] = n - k - 1;
	p[n++] = (u8)-70;
	ev(0x3E, p, n);
}

static void inq_result(struct remoto *r)
{
	u8 p[255];

	memset(p, 0, sizeof p);
	if (r->eir) {
		/* Extended Inquiry Result, con el nombre */
		int n = strlen(r->nombre);
		p[0] = 1;
		memcpy(p + 1, r->dir, 6);
		p[7] = 1;				/* R1 */
		p[9] = r->clase, p[10] = r->clase >> 8, p[11] = r->clase >> 16;
		pon16(p + 12, 0x1234);
		p[14] = (u8)-55;
		p[15] = n + 1, p[16] = 0x09;
		memcpy(p + 17, r->nombre, n);
		ev(0x2F, p, 255);
	} else {
		/* Inquiry Result with RSSI (sin nombre: habrá que pedirlo) */
		p[0] = 1;
		memcpy(p + 1, r->dir, 6);
		p[7] = 1;
		p[9] = r->clase, p[10] = r->clase >> 8, p[11] = r->clase >> 16;
		pon16(p + 12, 0x2345);
		p[14] = (u8)-62;
		ev(0x22, p, 15);
	}
}

/* ====================================================== L2CAP del aparato */
static void manda_l2(struct remoto *r, u16 cid, const u8 *d, int n)
{
	u8 l2[1100], b[64];
	int i = 0, primero = 1;

	pon16(l2, n);
	pon16(l2 + 2, cid);
	memcpy(l2 + 4, d, n);
	n += 4;
	/* en trozos ACL de 48 (para que el anfitrión tenga que juntarlos) */
	while (i < n) {
		int k = n - i > 48 ? 48 : n - i;
		pon16(b, r->handle | (primero ? 0x2000 : 0x1000));
		pon16(b + 2, k);
		memcpy(b + 4, l2 + i, k);
		sale(DE_GRANEL, b, 4 + k);
		i += k, primero = 0;
	}
}

static u8 sig_id = 0x80;

static void senal(struct remoto *r, u8 cod, u8 id, const u8 *d, int n)
{
	u8 b[64];

	b[0] = cod, b[1] = id;
	pon16(b + 2, n);
	memcpy(b + 4, d, n);
	manda_l2(r, 1, b, 4 + n);
}

static struct canal_r *canal_psm(struct remoto *r, u16 psm)
{
	int i;

	for (i = 0; i < MAX_CANAL; i++)
		if (r->c[i].estado && r->c[i].psm == psm)
			return &r->c[i];
	return 0;
}

static struct canal_r *canal_mio(struct remoto *r, u16 cid)
{
	int i;

	for (i = 0; i < MAX_CANAL; i++)
		if (r->c[i].estado && r->c[i].mio == cid)
			return &r->c[i];
	return 0;
}

static struct canal_r *canal_nuevo(struct remoto *r, u16 psm)
{
	int i;

	for (i = 0; i < MAX_CANAL; i++)
		if (!r->c[i].estado) {
			memset(&r->c[i], 0, sizeof r->c[i]);
			r->c[i].psm = psm;
			r->c[i].mio = 0x0040 + (r->cid_sig++ % 32);
			r->c[i].estado = 1;
			return &r->c[i];
		}
	return 0;
}

static void pide_conf(struct remoto *r, struct canal_r *c)
{
	u8 d[8];

	pon16(d, c->suyo);
	pon16(d + 2, 0);
	d[4] = 1, d[5] = 2;
	pon16(d + 6, 672);
	senal(r, 0x04, sig_id++, d, 8);
}

/* el aparato abre un canal (al reconectarse él) */
static void abre_el(struct remoto *r, u16 psm)
{
	struct canal_r *c = canal_nuevo(r, psm);
	u8 d[4];

	if (!c)
		return;
	c->estado = 2;				/* pedido */
	pon16(d, psm);
	pon16(d + 2, c->mio);
	senal(r, 0x02, sig_id++, d, 4);
}

/* SDP: lo de su registro HID, partido de 100 en 100 (para que haga falta seguir) */
static int registro(struct remoto *r, u8 *d)
{
	int n = 0, k, ini, ini2;

	if (!r->hid) {
		d[n++] = 0x35, d[n++] = 0;	/* ningún registro */
		return n;
	}
	d[n++] = 0x36, ini = n, n += 2;		/* la lista de registros */
	d[n++] = 0x36, ini2 = n, n += 2;	/* el registro */
	d[n++] = 0x09, d[n++] = 0x02, d[n++] = 0x01, d[n++] = 0x09, d[n++] = 0x01, d[n++] = 0x11;
	d[n++] = 0x09, d[n++] = 0x02, d[n++] = 0x02, d[n++] = 0x08, d[n++] = r->clase & 0xFC;
	d[n++] = 0x09, d[n++] = 0x02, d[n++] = 0x04, d[n++] = 0x28, d[n++] = 1;
	d[n++] = 0x09, d[n++] = 0x02, d[n++] = 0x05, d[n++] = 0x28, d[n++] = 1;
	d[n++] = 0x09, d[n++] = 0x02, d[n++] = 0x06;
	k = r->desc_n;
	d[n++] = 0x35, d[n++] = k + 6;
	d[n++] = 0x35, d[n++] = k + 4;
	d[n++] = 0x08, d[n++] = 0x22;
	d[n++] = 0x25, d[n++] = k;
	memcpy(d + n, r->desc, k), n += k;
	d[n++] = 0x09, d[n++] = 0x02, d[n++] = 0x0D, d[n++] = 0x28, d[n++] = 1;
	d[n++] = 0x09, d[n++] = 0x02, d[n++] = 0x0E, d[n++] = 0x28, d[n++] = r->boot;
	d[ini] = (n - ini - 2) >> 8, d[ini + 1] = n - ini - 2;
	d[ini2] = (n - ini2 - 2) >> 8, d[ini2 + 1] = n - ini2 - 2;
	return n;
}

static void sdp(struct remoto *r, struct canal_r *c, const u8 *d, int n)
{
	u8 reg[1024], sal[200];
	int total, desde = 0, k, cont;

	if (n < 5 || d[0] != 0x06) {
		fallo("SDP: el anfitrión pide algo que no es ServiceSearchAttributeRequest", n ? d[0] : -1);
		return;
	}
	if (n < 18 || d[5] != 0x35 || d[7] != 0x19 || d[8] != 0x11 || d[9] != 0x24)
		fallo("SDP: no busca el servicio HID (0x1124)", 0);
	/* la continuación, tras la búsqueda (5), lo más (2) y los atributos (7):
	 * 0, o 2 octetos con por dónde va */
	if (n < 20 || d[12] != 0x35 || d[13] != 5) {
		fallo("SDP: la lista de atributos no es la que se espera", n);
		return;
	}
	cont = d[19];
	if (cont == 2 && n >= 22)
		desde = (d[20] << 8) | d[21];
	else if (cont)
		fallo("SDP: una continuación que el aparato no ha dado", cont);
	total = registro(r, reg);
	k = total - desde > 100 ? 100 : total - desde;
	sal[0] = 0x07, sal[1] = d[1], sal[2] = d[2];
	sal[5] = k >> 8, sal[6] = k;
	memcpy(sal + 7, reg + desde, k);
	if (desde + k < total) {
		sal[7 + k] = 2, sal[8 + k] = (desde + k) >> 8, sal[9 + k] = desde + k;
		k += 3;
	} else
		sal[7 + k] = 0, k += 1;
	sal[3] = (k + 2) >> 8, sal[4] = k + 2;
	manda_l2(r, c->suyo, sal, 7 + k - 0);
}

static void canal_abierto(struct remoto *r, struct canal_r *c)
{
	(void)c;
	/* si se ha despertado él: tras el de control, el de interrupción */
	if (r->sale_el && c->psm == 0x11 && !canal_psm(r, 0x13))
		abre_el(r, 0x13);
}

static void mira_abierto(struct remoto *r, struct canal_r *c)
{
	if (c->estado == 3 && c->conf_mia && c->conf_suya) {
		c->estado = 4;
		canal_abierto(r, c);
	}
}

static void senales(struct remoto *r, const u8 *d, int n)
{
	while (n >= 4) {
		u8 cod = d[0], id = d[1], o[16];
		int l = le16(d + 2);
		const u8 *p = d + 4;
		struct canal_r *c;

		if (4 + l > n) {
			fallo("L2CAP: una señal con el largo mal", l);
			return;
		}
		switch (cod) {
		case 0x02:
			if (l != 4) {
				fallo("L2CAP: Connection Request con el largo mal", l);
				break;
			}
			if (!r->cifrado && le16(p) != 0x0001)
				fallo("L2CAP: el anfitrión abre un canal HID sin cifrar", le16(p));
			c = canal_nuevo(r, le16(p));
			if (!c || (le16(p) != 1 && !r->hid) || (le16(p) != 1 && le16(p) != 0x11 && le16(p) != 0x13)) {
				pon16(o, 0), pon16(o + 2, le16(p + 2)), pon16(o + 4, 2), pon16(o + 6, 0);
				senal(r, 0x03, id, o, 8);
				if (c)
					c->estado = 0;
				break;
			}
			if (le16(p) == 0x13 && !canal_psm(r, 0x11))
				fallo("HID: el de interrupción antes que el de control", 0);
			c->suyo = le16(p + 2);
			c->estado = 3;
			pon16(o, c->mio), pon16(o + 2, c->suyo), pon16(o + 4, 0), pon16(o + 6, 0);
			senal(r, 0x03, id, o, 8);
			pide_conf(r, c);
			break;
		case 0x03:
			if (l != 8 || !(c = canal_mio(r, le16(p + 2)))) {
				fallo("L2CAP: Connection Response de un canal que no existe", l);
				break;
			}
			if (le16(p + 4) == 1)
				break;		/* pendiente */
			if (le16(p + 4)) {
				c->estado = 0;
				break;
			}
			if (!r->cifrado)
				fallo("L2CAP: el anfitrión acepta un canal HID sin cifrar", c->psm);
			c->suyo = le16(p);
			c->estado = 3;
			pide_conf(r, c);
			break;
		case 0x04:
			if (l < 4 || !(c = canal_mio(r, le16(p)))) {
				fallo("L2CAP: Configuration Request de un canal que no existe", l);
				break;
			}
			pon16(o, c->suyo), pon16(o + 2, 0), pon16(o + 4, 0);
			senal(r, 0x05, id, o, 6);
			c->conf_suya = 1;
			mira_abierto(r, c);
			break;
		case 0x05:
			if (l < 6 || !(c = canal_mio(r, le16(p)))) {
				fallo("L2CAP: Configuration Response de un canal que no existe", l);
				break;
			}
			if (le16(p + 4))
				fallo("L2CAP: el anfitrión no acepta la configuración", le16(p + 4));
			c->conf_mia = 1;
			mira_abierto(r, c);
			break;
		case 0x06:
			if (l != 4 || !(c = canal_mio(r, le16(p)))) {
				fallo("L2CAP: Disconnection Request de un canal que no existe", l);
				break;
			}
			memcpy(o, p, 4);
			senal(r, 0x07, id, o, 4);
			c->estado = 0;
			break;
		case 0x07:
			break;
		case 0x0B:
		case 0x09:
			break;
		default:
			fallo("L2CAP: una señal que no se esperaba", cod);
		}
		d += 4 + l, n -= 4 + l;
	}
}

static void l2_del_anfitrion(struct remoto *r, const u8 *d, int n)
{
	u16 l = le16(d), cid = le16(d + 2);
	struct canal_r *c;

	if (l + 4 != n) {
		fallo("L2CAP: el largo no cuadra con el del ACL", l);
		return;
	}
	d += 4;
	if (cid == 1) {
		senales(r, d, l);
		return;
	}
	c = canal_mio(r, cid);
	if (!c || c->estado != 4) {
		fallo("L2CAP: datos a un canal sin abrir", cid);
		return;
	}
	if (c->psm == 1)
		sdp(r, c, d, l);
	else if (c->psm == 0x11) {
		if (l == 1 && (d[0] & 0xFE) == 0x70) {
			u8 h = 0x00;	/* HANDSHAKE: bien */
			r->protocolo = d[0] & 1;
			manda_l2(r, c->suyo, &h, 1);
		} else if (l == 1 && d[0] == 0x15) {
			r->con_clave = 0;	/* «virtual cable unplug»: nos olvida */
		} else
			fallo("HID: una orden de control que no se esperaba", d[0]);
	} else
		fallo("HID: el anfitrión manda algo por el canal de interrupción", l);
}

/* ============================================================== emparejar */
static void acaba_emparejar(struct remoto *r, int st)
{
	u8 p[23];

	if (r->ssp) {
		p[0] = st;
		memcpy(p + 1, r->dir, 6);
		ev(0x36, p, 7);
	}
	if (!st) {
		int i;
		for (i = 0; i < 16; i++)
			r->clave[i] = rand();
		r->con_clave = 1;
		r->emparejable = 0;
		memcpy(p, r->dir, 6);
		memcpy(p + 6, r->clave, 16);
		p[22] = r->io == 3 ? 0x04 : 0x05;
		ev(0x18, p, 23);
	}
	p[0] = st;
	pon16(p + 1, r->handle);
	ev(0x06, p, 3);
	r->emparejando = r->esperando = 0;
}

int mod_teclea_codigo(int i, u32 codigo)
{
	struct remoto *r = &rem[i];
	u8 p[7];
	int k;

	if (!r->esperando)
		return -1;
	memcpy(p, r->dir, 6);
	if (r->ssp) {
		p[6] = 0;		/* empieza */
		ev(0x3C, p, 7);
		for (k = 0; k < 6; k++) {
			p[6] = 1;
			ev(0x3C, p, 7);
		}
		p[6] = 4;		/* Intro */
		ev(0x3C, p, 7);
	}
	acaba_emparejar(r, codigo == r->codigo ? 0 : 0x05);
	return codigo == r->codigo ? 0 : -1;
}

void mod_acepta(int i, int si)
{
	struct remoto *r = &rem[i];

	r->confirma_el = si ? 1 : 2;
	if (r->confirma_host)
		acaba_emparejar(r, r->confirma_host == 1 && r->confirma_el == 1 ? 0 : 0x05);
}

/* ==================================================== las órdenes de Intel */
static void intel(u16 op, const u8 *p, int n, int por_granel)
{
	u8 r[32];

	if (op == 0xFC09) {
		if (clase_ad != MOD_INTEL_CARGADOR || !cargador) {
			fallo("Intel: 0xFC09 sin estar en el cargador", 0);
			cc0(op);
			return;
		}
		if (!por_granel)
			fallo("Intel: 0xFC09 en el cargador tiene que ir por el extremo a granel", 0);
		if (n < 2 || n > 253)
			fallo("Intel: 0xFC09 con el largo mal", n);
		if (fw_esperado) {
			int k = n - 1;
			if (fw_llegado + k > fw_n)
				fallo("Intel: llega más firmware del que hay", fw_llegado + k);
			else
				memcpy(fw_rec + fw_llegado, p + 1, k);
			/* (los tres primeros trozos, por su clase: 0 la cabecera, 3 la
			 * llave, 2 la firma; los 4 octetos de detrás de la llave, el
			 * exponente, no se mandan: btintel_sfi_rsa_header_secure_send) */
			if ((fw_llegado < 128 && p[0] != 0) || (fw_llegado >= 128 && fw_llegado < 384 && p[0] != 3) ||
			    (fw_llegado >= 384 && fw_llegado < 640 && p[0] != 2) || (fw_llegado >= 640 && p[0] != 1))
				fallo("Intel: un trozo del firmware con la clase mal", p[0]);
			fw_llegado += k;
			fw_tipo_visto |= 1 << p[0];
		}
		sucesos_por_granel = 1;
		cc0(op);
		if (fw_esperado && fw_llegado == fw_n) {
			u8 e[5] = { 0x06, 0x00, 0x09, 0xFC, 0x00 };
			mod_firmware_bien = !memcmp(fw_rec, fw_esperado, fw_n);
			if (!mod_firmware_bien)
				fallo("Intel: el firmware no ha llegado igual", 0);
			ev(0xFF, e, 5);
		}
		sucesos_por_granel = 0;
		return;
	}
	switch (op) {
	case 0xFC05:
		r[0] = 0, r[1] = 0x37;
		if (clase_ad == MOD_INTEL_ROM) {
			u8 v[] = { 0x08, 0x10, 0x01, 0x10, 0x03, 0x11, 0x0E, 0x00 };
			memcpy(r + 2, v, 8);
		} else if (cargador) {
			u8 v[] = { 0x11, 0x10, 0x06, 0x01, 0x00, 0x00, 0x00, 0x00 };
			memcpy(r + 2, v, 8);
		} else {
			u8 v[] = { 0x11, 0x10, 0x23, 0x01, 0x22, 0x30, 0x19, 0x00 };
			memcpy(r + 2, v, 8);
		}
		cc(op, r, 10);
		return;
	case 0xFC0D:
		if (!cargador) {
			fallo("Intel: 0xFC0D fuera del cargador", 0);
			cc0(op);
			return;
		}
		memset(r, 0, 24);
		pon16(r + 4, 0x0010);
		r[6] = 1;
		memcpy(r + 12, mi_dir, 6);
		cc(op, r, 24);
		return;
	case 0xFC01:
		/* (no contesta: arranca y avisa) */
		if (!cargador || n != 8)
			fallo("Intel: 0xFC01 fuera de su sitio", n);
		if (!mod_firmware_bien)
			fallo("Intel: 0xFC01 sin el firmware entero", 0);
		if (n == 8 && (p[4] | p[5] << 8 | p[6] << 16 | (u32)p[7] << 24) != 0x00A55A00)
			fallo("Intel: 0xFC01 sin la dirección de arranque del firmware", p[4]);
		cargador = 0;
		arrancado = 1;
		{
			u8 e[1] = { 0x02 };
			ev(0xFF, e, 1);
		}
		return;
	case 0xFC8B:
		if (cargador)
			fallo("Intel: la configuración antes de arrancar", 0);
		mod_ddc++;
		cc0(op);
		return;
	case 0xFC11:
	case 0xFC8E:
	case 0xFC52:
		mod_parche++;
		cc0(op);
		return;
	}
	r[0] = 0x01;			/* orden desconocida */
	cc(op, r, 1);
}

/* ============================================================== una orden */
static void orden(const u8 *d, int n, int por_granel)
{
	u16 op = le16(d);
	int l = d[2];
	const u8 *p = d + 3;
	struct remoto *r;
	u8 o[260];
	int i;

	mod_ordenes++;
	if (l + 3 != n) {
		fallo("una orden con el largo mal", op);
		return;
	}
	if (por_granel && op != 0xFC09)
		fallo("una orden que no es 0xFC09 por el extremo a granel", op);
	if (cargador && (op & 0xFC00) != 0xFC00) {
		fallo("una orden normal con el Intel aún en su cargador", op);
		o[0] = 0x0C;
		cc(op, o, 1);
		return;
	}
	if ((op & 0xFC00) == 0xFC00) {
		if (clase_ad == MOD_GENERICO) {
			o[0] = 0x01;
			cc(op, o, 1);
		} else
			intel(op, p, l, por_granel);
		return;
	}
	switch (op) {
	case 0x0C03:
		scan_enable = 0, le_scan = 0;
		for (i = 0; i < N_REMOTOS; i++)
			rem[i].conectado = 0;
		memset(pend, 0, sizeof pend);
		cc0(op);
		return;
	case 0x1001:
		o[0] = 0, o[1] = 0x08;
		pon16(o + 2, 0x1234);
		o[4] = 0x08;
		pon16(o + 5, clase_ad == MOD_GENERICO ? 10 : 2);
		pon16(o + 7, 0x0100);
		cc(op, o, 9);
		return;
	case 0x1009:
		o[0] = 0;
		memcpy(o + 1, mi_dir, 6);
		cc(op, o, 7);
		return;
	case 0x1003:
		{
			u8 f[9] = { 0, 0xFF, 0xFF, 0x8F, 0xFE, 0xDB, 0xFF, 0x5B, 0x87 };
			f[4] |= 0x40, f[5] |= 0x40, f[7] |= 0x09;	/* con señal, BLE, EIR y SSP */
			cc(op, f, 9);
		}
		return;
	case 0x1005:
		o[0] = 0;
		pon16(o + 1, 310), o[3] = 64;
		pon16(o + 4, 10), pon16(o + 6, 8);
		cc(op, o, 8);
		return;
	case 0x0C13:
		if (l != 248)
			fallo("Write_Local_Name sin sus 248 octetos", l);
		cc0(op);
		return;
	case 0x0C56:
		ssp_mode = p[0];
		cc0(op);
		return;
	case 0x0C1A:
		scan_enable = p[0];
		cc0(op);
		return;
	case 0x0C01: case 0x0C45: case 0x0C24: case 0x0C18: case 0x080F: case 0x0C6D: case 0x2001: case 0x200B:
		cc0(op);
		return;
	case 0x200C:
		le_scan = p[0];
		if (le_scan && rem[R_BLE].encendido)
			programa(P_LE, R_BLE, 30);
		cc0(op);
		return;
	case 0x0401:
		if (l != 5 || p[0] != 0x33 || p[1] != 0x8B || p[2] != 0x9E)
			fallo("Inquiry sin el GIAC", l);
		cs(op, 0);
		for (i = 0; i < N_REMOTOS; i++)
			if (rem[i].encendido && rem[i].emparejable && !rem[i].le)
				programa(P_INQ_RESULT, i, 40 + i * 30);
		programa(P_INQ_FIN, 0, 300);
		return;
	case 0x0402:
		quita(P_INQ_RESULT);
		quita(P_INQ_FIN);
		cc0(op);
		return;
	case 0x0405:
		cs(op, 0);
		r = por_dir(p);
		if (r && r->encendido && !r->le && (r->emparejable || r->con_clave)) {
			r->sale_el = 0;
			programa(P_CONN, r - rem, 10);
		} else if (r)
			programa(P_CONN_FALLA, r - rem, 500);
		else
			fallo("Create_Connection a nadie", 0);
		return;
	case 0x0409:
		cs(op, 0);
		if ((r = por_dir(p)))
			conn_complete(r, 0);
		return;
	case 0x040A:
		cs(op, 0);
		if ((r = por_dir(p))) {
			o[0] = p[6];
			memset(o + 1, 0, 2);
			memcpy(o + 3, r->dir, 6);
			o[9] = 1, o[10] = 0;
			ev(0x03, o, 11);
		}
		return;
	case 0x0406:
		cs(op, 0);
		if ((r = por_handle(le16(p)))) {
			r->conectado = r->cifrado = 0;
			o[0] = 0;
			pon16(o + 1, r->handle);
			o[3] = 0x16;
			ev(0x05, o, 4);
		} else
			fallo("Disconnect de una conexión que no hay", le16(p));
		return;
	case 0x0411:
		cs(op, 0);
		if (!(r = por_handle(le16(p)))) {
			fallo("Authentication_Requested sin conexión", le16(p));
			return;
		}
		memcpy(o, r->dir, 6);
		ev(0x17, o, 6);			/* Link Key Request */
		return;
	case 0x040B:
		o[0] = 0;
		memcpy(o + 1, p, 6);
		cc(op, o, 7);
		if (!(r = por_dir(p)))
			return;
		o[0] = r->con_clave && !memcmp(r->clave, p + 6, 16) ? 0 : 0x06;
		if (o[0])
			fallo("el anfitrión da una clave que el aparato no tiene", 0);
		pon16(o + 1, r->handle);
		ev(0x06, o, 3);
		return;
	case 0x040C:
		o[0] = 0;
		memcpy(o + 1, p, 6);
		cc(op, o, 7);
		if (!(r = por_dir(p)))
			return;
		if (!r->emparejable) {
			o[0] = 0x06;
			pon16(o + 1, r->handle);
			ev(0x06, o, 3);
			return;
		}
		r->emparejando = 1;
		memcpy(o, r->dir, 6);
		ev(r->ssp && ssp_mode ? 0x31 : 0x16, o, 6);	/* IO Capability Request, o PIN Code Request */
		return;
	case 0x042B:
		o[0] = 0;
		memcpy(o + 1, p, 6);
		cc(op, o, 7);
		if (!(r = por_dir(p)) || !r->emparejando)
			return;
		if (l != 9 || p[6] != 0x01)
			fallo("IO Capability Reply: se esperaba «pantalla y sí/no»", p[6]);
		memcpy(o, r->dir, 6);
		o[6] = r->io, o[7] = 0, o[8] = 0x04;
		ev(0x32, o, 9);
		r->codigo = 100000 + rand() % 900000;
		memcpy(o, r->dir, 6);
		o[6] = r->codigo, o[7] = r->codigo >> 8, o[8] = r->codigo >> 16, o[9] = 0;
		if (r->io == 2) {
			r->esperando = 1;
			ev(0x3B, o, 10);	/* User Passkey Notification */
		} else {
			r->confirma_host = r->confirma_el = 0;
			if (r->io == 3)
				r->confirma_el = 1;
			ev(0x33, o, 10);	/* User Confirmation Request */
		}
		return;
	case 0x0434:
		o[0] = 0;
		memcpy(o + 1, p, 6);
		cc(op, o, 7);
		if ((r = por_dir(p)) && r->conectado)
			acaba_emparejar(r, 0x18);
		return;
	case 0x042C:
	case 0x042D:
		o[0] = 0;
		memcpy(o + 1, p, 6);
		cc(op, o, 7);
		if (!(r = por_dir(p)))
			return;
		r->confirma_host = op == 0x042C ? 1 : 2;
		if (r->confirma_el)
			acaba_emparejar(r, r->confirma_host == 1 && r->confirma_el == 1 ? 0 : 0x05);
		return;
	case 0x040D:
		o[0] = 0;
		memcpy(o + 1, p, 6);
		cc(op, o, 7);
		if (!(r = por_dir(p)))
			return;
		/* el PIN: lo que haya que teclear en el teclado */
		r->codigo = atoi((const char *)p + 7);
		r->esperando = 1;
		return;
	case 0x040E:
		o[0] = 0;
		memcpy(o + 1, p, 6);
		cc(op, o, 7);
		if ((r = por_dir(p)))
			acaba_emparejar(r, 0x06);
		return;
	case 0x042F:
		o[0] = 0;
		memcpy(o + 1, p, 6);
		cc(op, o, 7);
		return;
	case 0x0413:
		cs(op, 0);
		if (!(r = por_handle(le16(p))))
			return;
		r->cifrado = 1;
		o[0] = 0;
		pon16(o + 1, r->handle);
		o[3] = 1;
		ev(0x08, o, 4);
		/* (el que se ha despertado ya había pedido el de control: ahora sí) */
		return;
	case 0x0419:
		cs(op, 0);
		if (!(r = por_dir(p)))
			return;
		memset(o, 0, 255);
		memcpy(o + 1, r->dir, 6);
		memcpy(o + 7, r->nombre, strlen(r->nombre));
		ev(0x07, o, 255);
		return;
	case 0x041A:
		o[0] = 0;
		memcpy(o + 1, p, 6);
		cc(op, o, 7);
		return;
	}
	fallo("una orden que el modelo no conoce", op);
	o[0] = 0x01;
	cc(op, o, 1);
}

static void acl(const u8 *d, int n)
{
	u16 h = le16(d) & 0x0FFF, pb = (le16(d) >> 12) & 3, l = le16(d + 2);
	struct remoto *r = por_handle(h);
	u8 e[5];

	if (l + 4 != n) {
		fallo("ACL: el largo no cuadra", l);
		return;
	}
	if (l > 310)
		fallo("ACL: más largo de lo que admite el adaptador (310)", l);
	e[0] = 1;
	pon16(e + 1, h);
	pon16(e + 3, 1);
	ev(0x13, e, 5);
	if (!r) {
		fallo("ACL a una conexión que no hay", h);
		return;
	}
	if (pb == 2) {
		if (l < 2)
			return;
		r->ens_quiere = 4 + le16(d + 4);
		r->ens_n = 0;
	} else if (pb != 1 || !r->ens_quiere) {
		fallo("ACL: un trozo sin principio", pb);
		return;
	}
	memcpy(r->ens + r->ens_n, d + 4, l);
	r->ens_n += l;
	if (r->ens_n >= r->ens_quiere) {
		l2_del_anfitrion(r, r->ens, r->ens_quiere);
		r->ens_quiere = 0;
	}
}

void mod_recibe(int clase, const u8 *d, int n)
{
	if (n < 3) {
		fallo("un paquete demasiado corto", n);
		return;
	}
	if (clase == 1 || clase == 3)
		orden(d, n, clase == 3);
	else if (clase == 2)
		acl(d, n);
}

void mod_tic(u32 t)
{
	int i;

	ahora = t;
	for (i = 0; i < MAX_PEND; i++) {
		struct remoto *r;
		u8 o[8];
		if (!pend[i].usado || (int)(ahora - pend[i].cuando) < 0)
			continue;
		pend[i].usado = 0;
		r = &rem[pend[i].r];
		switch (pend[i].que) {
		case P_INQ_RESULT:
			inq_result(r);
			break;
		case P_INQ_FIN:
			o[0] = 0;
			ev(0x01, o, 1);
			break;
		case P_CONN:
			conn_complete(r, 0);
			break;
		case P_CONN_FALLA:
			conn_complete(r, 0x04);
			break;
		case P_LE:
			if (le_scan) {
				anuncio_le(r);
				programa(P_LE, pend[i].r, 100);
			}
			break;
		}
	}
	/* el que se ha despertado: cifrado ya, abre el de control */
	for (i = 0; i < N_REMOTOS; i++) {
		struct remoto *r = &rem[i];
		if (r->conectado && r->sale_el == 1) {
			/* (como los de verdad: lo pide en cuanto se conecta, antes de cifrar) */
			r->sale_el = 2;
			abre_el(r, 0x11);
		}
	}
}

/* ============================================================== la persona */
void mod_emparejable(int r, int si) { rem[r].emparejable = si; rem[r].encendido = 1; }
int mod_conectado(int r) { return rem[r].conectado && rem[r].cifrado; }
int mod_protocolo(int r) { return rem[r].conectado ? rem[r].protocolo : -1; }
int mod_emparejado(int r) { return rem[r].con_clave; }
void mod_apaga(int r) { rem[r].encendido = 0; }

void mod_despierta(int i)
{
	struct remoto *r = &rem[i];
	u8 o[10];

	r->encendido = 1;
	if (!r->con_clave || r->conectado || !(scan_enable & 2))
		return;
	memcpy(o, r->dir, 6);
	o[6] = r->clase, o[7] = r->clase >> 8, o[8] = r->clase >> 16, o[9] = 1;
	r->sale_el = 1;
	ev(0x04, o, 10);
}

static int canal_int(struct remoto *r)
{
	struct canal_r *c = canal_psm(r, 0x13);

	return c && c->estado == 4 ? c->suyo : 0;
}

int mod_tecla(int i, u8 mods, u8 uso)
{
	struct remoto *r = &rem[i];
	u8 d[10] = { 0xA1, 0x01, mods, 0, uso, 0, 0, 0, 0, 0 };
	int c = canal_int(r);

	if (!c || !r->cifrado)
		return -1;
	manda_l2(r, c, d, 10);
	return 0;
}

int mod_mueve(int i, int dx, int dy, int b, int rueda)
{
	struct remoto *r = &rem[i];
	u8 d[6] = { 0xA1, 0x02, b, dx, dy, rueda };
	int c = canal_int(r);

	if (!c || !r->cifrado)
		return -1;
	manda_l2(r, c, d, r->protocolo == 1 ? 6 : 5);
	return 0;
}

uint16_t mod_vid(void) { return clase_ad == MOD_GENERICO ? 0x0A12 : 0x8087; }
uint16_t mod_pid(void) { return clase_ad == MOD_GENERICO ? 0x0001 : clase_ad == MOD_INTEL_ROM ? 0x0A2A : 0x0AAA; }

static void pon_remoto(int i, const char *nombre, u8 ultimo, u32 clase, int io, int ssp, int hid, int boot, int eir, const u8 *desc, int n)
{
	struct remoto *r = &rem[i];

	memset(r, 0, sizeof *r);
	r->nombre = nombre;
	r->dir[0] = ultimo, r->dir[1] = 0x10 + i, r->dir[2] = 0xBE, r->dir[3] = 0xBA, r->dir[4] = 0xAD, r->dir[5] = 0xDE;
	r->clase = clase, r->io = io, r->ssp = ssp, r->hid = hid, r->boot = boot, r->eir = eir;
	r->desc = desc, r->desc_n = n;
	r->encendido = 1;
	r->protocolo = -1;
}

void mod_inicia(int clase, const char *carpeta)
{
	clase_ad = clase;
	memset(pend, 0, sizeof pend);
	s_ini = s_fin = 0;
	scan_enable = le_scan = ssp_mode = 0;
	mod_firmware_bien = mod_ddc = mod_parche = 0;	/* (mod_fallos se suma de una vez a otra) */
	cargador = clase == MOD_INTEL_CARGADOR;
	arrancado = 0;
	srand(1234);
	pon_remoto(R_TECLADO, "Teclado de prueba", 0x01, 0x002540, 2, 1, 1, 1, 1, desc_teclado, sizeof desc_teclado);
	pon_remoto(R_RATON, "Ratón de prueba", 0x02, 0x002580, 3, 1, 1, 0, 0, desc_raton, sizeof desc_raton);
	pon_remoto(R_TELEFONO, "Teléfono de prueba", 0x03, 0x5A020C, 1, 1, 0, 0, 1, 0, 0);
	pon_remoto(R_ANTIGUO, "Teclado antiguo", 0x04, 0x000540, 2, 0, 1, 1, 0, desc_teclado, sizeof desc_teclado);
	pon_remoto(R_BLE, "Ratón BLE", 0x05, 0, 3, 0, 0, 0, 0, 0, 0);
	rem[R_BLE].le = 1;
	free(fw_esperado);
	fw_esperado = 0;
	fw_n = fw_llegado = 0;
	if (clase == MOD_INTEL_CARGADOR && carpeta) {
		char ruta[512];
		FILE *f;
		snprintf(ruta, sizeof ruta, "%s/ibt-17-16-1.sfi", carpeta);
		if ((f = fopen(ruta, "rb"))) {
			fw_esperado = malloc(sizeof fw_rec);
			fw_n = fread(fw_esperado, 1, sizeof fw_rec, f);
			fclose(f);
			/* (lo que tiene que llegar: todo menos los octetos 384 a 387) */
			if (fw_n > 388) {
				memmove(fw_esperado + 384, fw_esperado + 388, fw_n - 388);
				fw_n -= 4;
			}
		}
	}
}
