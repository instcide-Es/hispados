/* l2cap.c - (BLUETOOTH) la pila de BT.BIN: L2CAP (el canal de señales y los
 * canales en modo básico), el cliente SDP (para el descriptor HID del
 * aparato) y HID por Bluetooth clásico (los canales de control, PSM 0x11,
 * y de interrupción, PSM 0x13; el protocolo de arranque para los teclados y
 * los ratones, o el de informes con el descriptor, que interpreta el mismo
 * hiddesc.cas del núcleo que los ratones USB).
 *
 * Los números, de la norma (Core 5.3, volumen 3, parte A: L2CAP; parte B:
 * SDP; y el «HID Profile» 1.1.1); los pasos, los de BlueZ
 * (net/bluetooth/l2cap_core.c, profiles/input/device.c y hidp/core.c).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "bt.h"

#define PSM_SDP 0x0001
#define PSM_HID_CTRL 0x0011
#define PSM_HID_INT 0x0013
#define CID_SENALES 0x0001

#define L2_CMD_REJECT 0x01
#define L2_CONN_REQ 0x02
#define L2_CONN_RSP 0x03
#define L2_CONF_REQ 0x04
#define L2_CONF_RSP 0x05
#define L2_DISCONN_REQ 0x06
#define L2_DISCONN_RSP 0x07
#define L2_ECHO_REQ 0x08
#define L2_ECHO_RSP 0x09
#define L2_INFO_REQ 0x0A
#define L2_INFO_RSP 0x0B

#define MI_MTU 672

static u16 le16(const u8 *p) { return p[0] | (p[1] << 8); }
static void pon16(u8 *p, u16 v) { p[0] = v, p[1] = v >> 8; }

/* ---- los canales ---- */
#define MAX_CANALES 16
#define C_LIBRE 0
#define C_PIDIENDO 1		/* le hemos pedido conectar */
#define C_PENDIENTE 2		/* nos lo ha pedido y esperamos a cifrar */
#define C_CONFIG 3		/* conectado: configurándose */
#define C_ABIERTO 4
#define C_CERRANDO 5
static struct canal {
	u8 estado, mia_ok, suya_ok, ident;
	struct aparato *a;
	u16 psm, lcid, rcid, rmtu;
	u32 desde;
} canales[MAX_CANALES];
static u16 siguiente_cid = 0x0040;
static u8 siguiente_ident = 1;

static struct canal *canal_de(struct aparato *a, u16 psm)
{
	int i;

	for (i = 0; i < MAX_CANALES; i++)
		if (canales[i].estado != C_LIBRE && canales[i].a == a && canales[i].psm == psm)
			return &canales[i];
	return 0;
}

static struct canal *canal_lcid(struct aparato *a, u16 cid)
{
	int i;

	for (i = 0; i < MAX_CANALES; i++)
		if (canales[i].estado != C_LIBRE && canales[i].a == a && canales[i].lcid == cid)
			return &canales[i];
	return 0;
}

static struct canal *canal_nuevo(struct aparato *a, u16 psm)
{
	int i;

	for (i = 0; i < MAX_CANALES; i++)
		if (canales[i].estado == C_LIBRE) {
			memset(&canales[i], 0, sizeof canales[i]);
			canales[i].a = a;
			canales[i].psm = psm;
			canales[i].lcid = siguiente_cid++;
			if (siguiente_cid < 0x0040)
				siguiente_cid = 0x0040;
			canales[i].rmtu = 672;
			canales[i].desde = bt_tics();
			return &canales[i];
		}
	return 0;
}

/* ---- mandar ---- */
static int manda(struct aparato *a, u16 cid, const u8 *d, int n)
{
	u8 b[MI_MTU + 8];

	if (n > MI_MTU)
		return -1;
	pon16(b, n);
	pon16(b + 2, cid);
	memcpy(b + 4, d, n);
	return hci_acl(a->handle, b, 4 + n);
}

static int senal(struct aparato *a, u8 codigo, u8 ident, const u8 *d, int n)
{
	u8 b[64];

	b[0] = codigo, b[1] = ident;
	pon16(b + 2, n);
	memcpy(b + 4, d, n);
	return manda(a, CID_SENALES, b, 4 + n);
}

static u8 ident_nuevo(void)
{
	u8 i = siguiente_ident++;

	if (!siguiente_ident)
		siguiente_ident = 1;
	return i;
}

static void pide_config(struct canal *c)
{
	u8 d[8];

	pon16(d, c->rcid);
	pon16(d + 2, 0);
	d[4] = 0x01, d[5] = 2;			/* la MTU que recibimos */
	pon16(d + 6, MI_MTU);
	senal(c->a, L2_CONF_REQ, ident_nuevo(), d, 8);
}

static void abre(struct aparato *a, u16 psm)
{
	struct canal *c = canal_nuevo(a, psm);
	u8 d[4];

	if (!c)
		return;
	c->estado = C_PIDIENDO;
	c->ident = ident_nuevo();
	pon16(d, psm);
	pon16(d + 2, c->lcid);
	senal(a, L2_CONN_REQ, c->ident, d, 4);
}

static void cierra(struct canal *c)
{
	u8 d[4];

	if (c->estado == C_ABIERTO || c->estado == C_CONFIG) {
		pon16(d, c->rcid);
		pon16(d + 2, c->lcid);
		senal(c->a, L2_DISCONN_REQ, ident_nuevo(), d, 4);
		c->estado = C_CERRANDO;
		c->desde = bt_tics();
	} else
		c->estado = C_LIBRE;
}

static void responde_conexion(struct canal *c, u16 resultado)
{
	u8 d[8];

	pon16(d, resultado == 0 || resultado == 1 ? c->lcid : 0);
	pon16(d + 2, c->rcid);
	pon16(d + 4, resultado);
	pon16(d + 6, resultado == 1 ? 1 : 0);	/* (pendiente: «falta autenticar») */
	senal(c->a, L2_CONN_RSP, c->ident, d, 8);
}

static void abierto(struct canal *c);

/* ============================================================ SDP: cliente */
/* «ServiceSearchAttributeRequest» del servicio HID (0x1124) con los
 * atributos 0x0200-0x02FF: lo del perfil HID (el descriptor, 0x0206; el
 * protocolo de arranque, 0x020E; si se reconecta solo, 0x0205) */
static struct { struct aparato *a; u8 b[2048]; int n; u8 cont[17]; u16 tid; } sdp;

static void sdp_pide(struct canal *c)
{
	u8 d[32];
	int n = 0, k;

	d[n++] = 0x06;				/* ServiceSearchAttributeRequest */
	d[n++] = sdp.tid >> 8, d[n++] = sdp.tid;
	n += 2;					/* (el largo, luego) */
	d[n++] = 0x35, d[n++] = 3, d[n++] = 0x19, d[n++] = 0x11, d[n++] = 0x24;
	d[n++] = 0x02, d[n++] = 0x80;		/* hasta 640 octetos por respuesta */
	d[n++] = 0x35, d[n++] = 5, d[n++] = 0x0A, d[n++] = 0x02, d[n++] = 0x00, d[n++] = 0x02, d[n++] = 0xFF;
	k = sdp.cont[0];
	memcpy(d + n, sdp.cont, k + 1);
	n += k + 1;
	d[3] = (n - 5) >> 8, d[4] = n - 5;
	manda(c->a, c->rcid, d, n);
}

/* un elemento de SDP: su clase y dónde están sus datos */
static int elemento(const u8 *p, int n, int *clase, int *dentro)
{
	int t, s, l;

	if (n < 1)
		return -1;
	t = p[0] >> 3, s = p[0] & 7;
	*clase = t;
	if (t == 0) {
		*dentro = 1;
		return 0;
	}
	if (s < 5) {
		*dentro = 1;
		return s == 0 ? 1 : s == 1 ? 2 : s == 2 ? 4 : s == 3 ? 8 : 16;
	}
	if (s == 5 && n >= 2) {
		*dentro = 2;
		return p[1];
	}
	if (s == 6 && n >= 3) {
		*dentro = 3;
		return (p[1] << 8) | p[2];
	}
	if (s == 7 && n >= 5) {
		*dentro = 5;
		l = ((u32)p[1] << 24) | ((u32)p[2] << 16) | (p[3] << 8) | p[4];
		return l;
	}
	return -1;
}

/* busca dentro de lo de SDP el atributo «id» del primer registro: su valor */
static const u8 *atributo(const u8 *p, int n, u16 id, int *largo)
{
	int c, d, l, i;

	/* la lista de registros, el primer registro... */
	l = elemento(p, n, &c, &d);
	if (l < 0 || c != 6 || d + l > n)
		return 0;
	p += d, n = l;
	l = elemento(p, n, &c, &d);
	if (l < 0 || c != 6 || d + l > n)
		return 0;
	p += d, n = l;
	/* ...y en él, parejas de identificador y valor */
	i = 0;
	while (i < n) {
		int lv, dv, cv;
		u16 este;
		l = elemento(p + i, n - i, &c, &d);
		if (l != 2 || c != 1)
			return 0;
		este = (p[i + d] << 8) | p[i + d + 1];
		i += d + l;
		lv = elemento(p + i, n - i, &cv, &dv);
		if (lv < 0 || i + dv + lv > n)
			return 0;
		if (este == id) {
			*largo = dv + lv;
			return p + i;
		}
		i += dv + lv;
	}
	return 0;
}

static void sdp_acaba(struct aparato *a)
{
	const u8 *v;
	int n, c, d, l, k;

	a->es_hid = 0;
	v = atributo(sdp.b, sdp.n, 0x0206, &n);
	if (v) {
		/* HIDDescriptorList: una lista de (clase 0x22, el descriptor) */
		l = elemento(v, n, &c, &d);
		if (l >= 0 && c == 6) {
			v += d, n = l;
			l = elemento(v, n, &c, &d);
			if (l >= 0 && c == 6) {
				const u8 *q = v + d;
				int m = l, cl, dl;
				l = elemento(q, m, &cl, &dl);
				if (l == 1 && q[dl] == 0x22) {
					q += dl + 1, m -= dl + 1;
					l = elemento(q, m, &cl, &dl);
					if (l > 0 && cl == 4 && dl + l <= m) {
						k = l > MAX_DESC ? MAX_DESC : l;
						memcpy(a->desc, q + dl, k);
						a->desc_n = k;
						a->es_hid = 1;
					}
				}
			}
		}
	}
	v = atributo(sdp.b, sdp.n, 0x020E, &n);
	a->boot = v && n >= 2 && v[0] == 0x28 && v[1];
	if (!a->es_hid && atributo(sdp.b, sdp.n, 0x0201, &n))
		a->es_hid = 1;		/* (sin descriptor, pero HID: con el de arranque) */
	/* qué es, por su descriptor: Generic Desktop, teclado (06) o ratón (02) */
	a->hid_clase = 0;
	for (k = 0; k + 3 < a->desc_n; k++)
		if (a->desc[k] == 0x05 && a->desc[k + 1] == 0x01 && a->desc[k + 2] == 0x09)
			a->hid_clase |= a->desc[k + 3] == 0x06 ? HID_TECLADO : a->desc[k + 3] == 0x02 ? HID_RATON : 0;
	if (!a->hid_clase)
		a->hid_clase = ((a->clase >> 6) & 3);	/* (la clase del aparato: 1 teclado, 2 ratón, 3 los dos) */
	bt_log("SDP de %s: %s%s; descriptor de %u octetos%s\n", bt_dir_corta(a->dir), a->es_hid ? "HID" : "no es HID",
	       a->hid_clase == 3 ? " (teclado y ratón)" : a->hid_clase == 1 ? " (teclado)" : a->hid_clase == 2 ? " (ratón)" : "",
	       a->desc_n, a->boot ? "; tiene el protocolo de arranque" : "");
	bt_guarda();
}

static void sdp_llega(struct canal *c, const u8 *d, int n)
{
	int k, cl;

	if (n < 7 || d[0] != 0x07) {
		bt_log("SDP: una respuesta que no se esperaba (%02X)\n", n ? d[0] : 0);
		cierra(c);
		return;
	}
	k = (d[5] << 8) | d[6];
	if (7 + k + 1 > n || sdp.n + k > (int)sizeof sdp.b) {
		cierra(c);
		return;
	}
	memcpy(sdp.b + sdp.n, d + 7, k);
	sdp.n += k;
	cl = d[7 + k];
	if (cl > 16 || 7 + k + 1 + cl > n) {
		cierra(c);
		return;
	}
	memcpy(sdp.cont, d + 7 + k, cl + 1);
	if (cl) {
		sdp.tid++;
		sdp_pide(c);
		return;
	}
	sdp_acaba(c->a);
	cierra(c);
	if (c->a->es_hid) {
		c->a->estado = A_HID;
		c->a->desde = bt_tics();
		abre(c->a, PSM_HID_CTRL);
	} else {
		bt_log("%s no es un teclado ni un ratón: no se usa\n", bt_dir_corta(c->a->dir));
		c->a->estado = A_LISTO;
	}
}

/* ================================================================== HID */
static int ranura_libre(void)
{
	int i, r;

	for (r = 0; r < 4; r++) {
		for (i = 0; i < MAX_APARATOS; i++)
			if (aparatos[i].usado && (aparatos[i].r_tec == r || aparatos[i].r_rat == r))
				break;
		if (i == MAX_APARATOS)
			return r;
	}
	return -1;
}

static void hid_listo(struct aparato *a)
{
	struct canal *k = canal_de(a, PSM_HID_CTRL);
	u8 d[1];

	/* el protocolo: el de arranque, salvo que no lo tenga (un ratón con su descriptor) */
	a->protocolo = 0;
	if (!a->boot && a->desc_n && !(a->hid_clase & HID_TECLADO))
		a->protocolo = 1;
	if (k) {
		d[0] = 0x70 | a->protocolo;	/* SET_PROTOCOL */
		manda(a, k->rcid, d, 1);
	}
	a->estado = A_LISTO;
	bt_log("HID listo: %s (%s), con el protocolo %s\n", bt_dir_corta(a->dir), a->nombre[0] ? a->nombre : "sin nombre",
	       a->protocolo ? "de informes (su descriptor)" : "de arranque");
	bt_estado_cambia();
}

static void hid_informe(struct aparato *a, const u8 *d, int n)
{
	int clase;
	static int dichos;

	if (n < 2 || d[0] != 0xA1)
		return;
	d++, n--;
	if (a->protocolo == 0) {
		/* de arranque: el número de informe (1 teclado, 2 ratón) y lo de siempre */
		clase = d[0] == 1 ? HID_TECLADO : d[0] == 2 ? HID_RATON : 0;
		if (!clase)
			return;
		d++, n--;
	} else
		clase = HID_RATON;
	if (dichos < 6) {
		dichos++;
		bt_log("Informe de %s: %s, %u octetos\n", bt_dir_corta(a->dir), clase == HID_TECLADO ? "teclado" : "ratón", n);
	}
	if (clase == HID_TECLADO) {
		if (a->r_tec < 0) {
			if ((a->r_tec = ranura_libre()) < 0)
				return;
			bt_hid(HID_ALTA, a->r_tec, HID_TECLADO, 0, 0);
		}
		if (n >= 8)
			bt_hid(HID_INFORME, a->r_tec, HID_TECLADO, d, 8);
	} else {
		if (a->r_rat < 0) {
			if ((a->r_rat = ranura_libre()) < 0)
				return;
			if (a->protocolo)
				bt_hid(HID_ALTA, a->r_rat, HID_RATON, a->desc, a->desc_n);
			else
				bt_hid(HID_ALTA, a->r_rat, HID_RATON, 0, 0);
		}
		bt_hid(HID_INFORME, a->r_rat, HID_RATON, d, n);
	}
}

static void hid_baja(struct aparato *a)
{
	if (a->r_tec >= 0)
		bt_hid(HID_BAJA, a->r_tec, HID_TECLADO, 0, 0);
	if (a->r_rat >= 0)
		bt_hid(HID_BAJA, a->r_rat, HID_RATON, 0, 0);
	a->r_tec = a->r_rat = -1;
}

void l2_corta_hid(struct aparato *a, int unplug)
{
	struct canal *c = canal_de(a, PSM_HID_CTRL);
	u8 d[1] = { 0x15 };			/* HID_CONTROL: VIRTUAL_CABLE_UNPLUG */

	if (c && c->estado == C_ABIERTO && unplug)
		manda(a, c->rcid, d, 1);
	hid_baja(a);
	if ((c = canal_de(a, PSM_HID_INT)))
		cierra(c);
	if ((c = canal_de(a, PSM_HID_CTRL)))
		cierra(c);
}

/* ======================================================= abrir y cerrar */
static void abierto(struct canal *c)
{
	struct aparato *a = c->a;

	bt_log("Canal L2CAP abierto con %s: PSM %04X\n", bt_dir_corta(a->dir), c->psm);
	if (c->psm == PSM_SDP && !a->entrante) {
		memset(&sdp, 0, sizeof sdp);
		sdp.a = a;
		sdp.tid = 1;
		sdp_pide(c);
	} else if (c->psm == PSM_HID_CTRL && !a->entrante)
		abre(a, PSM_HID_INT);
	else if (c->psm == PSM_HID_INT) {
		struct canal *k = canal_de(a, PSM_HID_CTRL);
		if (k && k->estado == C_ABIERTO)
			hid_listo(a);
	} else if (c->psm == PSM_HID_CTRL) {
		struct canal *k = canal_de(a, PSM_HID_INT);
		if (k && k->estado == C_ABIERTO)
			hid_listo(a);
	}
}

static void mira_config(struct canal *c)
{
	if (c->estado == C_CONFIG && c->mia_ok && c->suya_ok) {
		c->estado = C_ABIERTO;
		abierto(c);
	}
}

void l2_conectado(struct aparato *a)
{
	int i;

	/* (lo que quedara de una conexión de antes) */
	for (i = 0; i < MAX_CANALES; i++)
		if (canales[i].a == a)
			canales[i].estado = C_LIBRE;
}

void l2_reinicia(void)
{
	memset(canales, 0, sizeof canales);
}

void l2_desconectado(struct aparato *a)
{
	int i;

	hid_baja(a);
	for (i = 0; i < MAX_CANALES; i++)
		if (canales[i].a == a)
			canales[i].estado = C_LIBRE;
}

void l2_cifrado(struct aparato *a)
{
	int i;

	/* lo que nos había pedido el aparato antes de cifrar: ya sí */
	for (i = 0; i < MAX_CANALES; i++)
		if (canales[i].a == a && canales[i].estado == C_PENDIENTE) {
			canales[i].estado = C_CONFIG;
			responde_conexion(&canales[i], 0);
			pide_config(&canales[i]);
		}
}

void l2_empieza_hid(struct aparato *a)
{
	if (a->es_hid && a->desc_n) {
		/* ya se sabe su descriptor (de BT.INI): directamente a HID */
		a->estado = A_HID;
		abre(a, PSM_HID_CTRL);
	} else {
		a->estado = A_SDP;
		abre(a, PSM_SDP);
	}
}

/* =========================================================== lo que llega */
static void senales(struct aparato *a, const u8 *d, int n)
{
	while (n >= 4) {
		u8 cod = d[0], id = d[1];
		int l = le16(d + 2);
		const u8 *p = d + 4;
		struct canal *c;
		u8 r[16];
		int i;

		if (4 + l > n)
			break;
		switch (cod) {
		case L2_CONN_REQ:
			if (l < 4)
				break;
			{
				u16 psm = le16(p), scid = le16(p + 2);
				if ((psm != PSM_HID_CTRL && psm != PSM_HID_INT && psm != PSM_SDP) || !(c = canal_nuevo(a, psm))) {
					pon16(r, 0), pon16(r + 2, scid), pon16(r + 4, 2), pon16(r + 6, 0);	/* PSM no admitido */
					senal(a, L2_CONN_RSP, id, r, 8);
					break;
				}
				c->rcid = scid;
				c->ident = id;
				if (psm == PSM_SDP) {
					/* (no somos servidor SDP: no hay nada que darle) */
					c->estado = C_LIBRE;
					pon16(r, 0), pon16(r + 2, scid), pon16(r + 4, 2), pon16(r + 6, 0);
					senal(a, L2_CONN_RSP, id, r, 8);
					break;
				}
				if (a->estado == A_LISTO || a->estado == A_NADA)
					a->estado = A_HID;
				bt_log("%s abre el canal HID %04X\n", bt_dir_corta(a->dir), psm);
				if (!a->cifrado) {
					c->estado = C_PENDIENTE;
					responde_conexion(c, 1);
				} else {
					c->estado = C_CONFIG;
					responde_conexion(c, 0);
					pide_config(c);
				}
			}
			break;
		case L2_CONN_RSP:
			if (l < 8 || !(c = canal_lcid(a, le16(p + 2))) || c->estado != C_PIDIENDO)
				break;
			if (le16(p + 4) == 1)
				break;			/* pendiente: llegará otra */
			if (le16(p + 4)) {
				bt_log("%s no deja abrir el canal %04X (%u)\n", bt_dir_corta(a->dir), c->psm, le16(p + 4));
				c->estado = C_LIBRE;
				if (c->psm == PSM_SDP && !a->desc_n) {
					/* sin SDP: con la clase del aparato y el de arranque */
					a->es_hid = ((a->clase >> 8) & 0x1F) == 5;
					a->boot = 1;
					a->hid_clase = (a->clase >> 6) & 3;
					if (a->es_hid) {
						a->estado = A_HID;
						abre(a, PSM_HID_CTRL);
					}
				}
				break;
			}
			c->rcid = le16(p);
			c->estado = C_CONFIG;
			pide_config(c);
			break;
		case L2_CONF_REQ:
			if (l < 4)
				break;
			c = canal_lcid(a, le16(p));
			pon16(r, c ? c->rcid : 0);
			pon16(r + 2, 0);
			if (!c) {
				pon16(r + 4, 0x0002);	/* rechazada */
				senal(a, L2_CONF_RSP, id, r, 6);
				break;
			}
			/* sus opciones: su MTU (lo demás, lo de siempre) */
			for (i = 4; i + 2 <= l && i + 2 + p[i + 1] <= l; i += 2 + p[i + 1])
				if ((p[i] & 0x7F) == 0x01 && p[i + 1] == 2)
					c->rmtu = le16(p + i + 2);
			pon16(r + 4, 0);
			senal(a, L2_CONF_RSP, id, r, 6);
			c->suya_ok = 1;
			mira_config(c);
			break;
		case L2_CONF_RSP:
			if (l < 6 || !(c = canal_lcid(a, le16(p))))
				break;
			if (le16(p + 4)) {
				bt_log("%s no acepta la configuración del canal %04X (%u)\n", bt_dir_corta(a->dir), c->psm, le16(p + 4));
				cierra(c);
				break;
			}
			c->mia_ok = 1;
			mira_config(c);
			break;
		case L2_DISCONN_REQ:
			if (l < 4)
				break;
			memcpy(r, p, 4);
			senal(a, L2_DISCONN_RSP, id, r, 4);
			if ((c = canal_lcid(a, le16(p)))) {
				if (c->psm == PSM_HID_CTRL || c->psm == PSM_HID_INT) {
					hid_baja(a);
					if (a->estado == A_LISTO)
						a->estado = A_HID;
				}
				c->estado = C_LIBRE;
			}
			break;
		case L2_DISCONN_RSP:
			if (l >= 4 && (c = canal_lcid(a, le16(p + 2))))
				c->estado = C_LIBRE;
			break;
		case L2_ECHO_REQ:
			senal(a, L2_ECHO_RSP, id, p, l > 32 ? 32 : l);
			break;
		case L2_INFO_REQ:
			if (l < 2)
				break;
			pon16(r, le16(p));
			if (le16(p) == 2) {
				pon16(r + 2, 0);
				memset(r + 4, 0, 4);	/* sin funciones extendidas: solo el modo básico */
				senal(a, L2_INFO_RSP, id, r, 8);
			} else if (le16(p) == 3) {
				pon16(r + 2, 0);
				memset(r + 4, 0, 8);
				r[4] = 0x02;		/* el canal de señales */
				senal(a, L2_INFO_RSP, id, r, 12);
			} else {
				pon16(r + 2, 1);	/* no se admite */
				senal(a, L2_INFO_RSP, id, r, 4);
			}
			break;
		case L2_CMD_REJECT:
			bt_log("%s rechaza una orden L2CAP\n", bt_dir_corta(a->dir));
			break;
		}
		d += 4 + l, n -= 4 + l;
	}
}

void l2_llega(struct aparato *a, const u8 *d, int n)
{
	u16 l = le16(d), cid = le16(d + 2);
	struct canal *c;

	if (l + 4 > n)
		return;
	d += 4;
	if (cid == CID_SENALES) {
		senales(a, d, l);
		return;
	}
	if (!(c = canal_lcid(a, cid)) || c->estado != C_ABIERTO)
		return;
	if (c->psm == PSM_SDP)
		sdp_llega(c, d, l);
	else if (c->psm == PSM_HID_INT)
		hid_informe(a, d, l);
	else if (c->psm == PSM_HID_CTRL && l >= 1 && (d[0] & 0xF0) == 0x00 && d[0])
		bt_log("HID de %s: el aparato contesta %02X a SET_PROTOCOL\n", bt_dir_corta(a->dir), d[0]);
}

void l2_vuelta(void)
{
	int i;
	u32 t = bt_tics();

	for (i = 0; i < MAX_CANALES; i++)
		if (canales[i].estado == C_CERRANDO && t - canales[i].desde > 300)
			canales[i].estado = C_LIBRE;
}
