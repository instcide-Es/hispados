/* hci.c - (BLUETOOTH) la pila de BT.BIN: el HCI. Poner en marcha el
 * adaptador (reset, dirección, versión, lo que sabe hacer), buscar aparatos
 * (BR/EDR con «inquiry» y su nombre; BLE con el escaneo), conectarse y
 * emparejar con SSP (Just Works, comparación numérica y el código que se
 * teclea en el teclado) o con el PIN de antes, guardar las claves de enlace
 * y reconectar los emparejados. Lo de encima (L2CAP, SDP, HID), l2cap.c.
 *
 * Los números, de la norma (Bluetooth Core 5.3, volumen 4, parte E); el
 * orden de lo que se hace al arrancar y al emparejar, el de BlueZ
 * (net/bluetooth/hci_sync.c: hci_init1..4_sync; hci_event.c:
 * hci_io_capa_request_evt, hci_user_confirm_request_evt). docs/PENTIUM.md,
 * «El Bluetooth».
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "bt.h"

struct aparato aparatos[MAX_APARATOS];
struct bt_pila bt;
int bt_registro_todo;

/* ---- las órdenes y los sucesos que se usan ---- */
#define OP_INQUIRY 0x0401
#define OP_INQUIRY_CANCEL 0x0402
#define OP_CREATE_CONN 0x0405
#define OP_DISCONNECT 0x0406
#define OP_ACCEPT_CONN 0x0409
#define OP_REJECT_CONN 0x040A
#define OP_LINK_KEY_REPLY 0x040B
#define OP_LINK_KEY_NEG 0x040C
#define OP_PIN_REPLY 0x040D
#define OP_PIN_NEG 0x040E
#define OP_AUTH_REQUESTED 0x0411
#define OP_SET_CONN_ENCRYPT 0x0413
#define OP_REMOTE_NAME 0x0419
#define OP_REMOTE_NAME_CANCEL 0x041A
#define OP_IO_CAPA_REPLY 0x042B
#define OP_USER_CONFIRM_REPLY 0x042C
#define OP_USER_CONFIRM_NEG 0x042D
#define OP_USER_PASSKEY_NEG 0x042F
#define OP_IO_CAPA_NEG 0x0434
#define OP_WRITE_DEF_POLICY 0x080F
#define OP_SET_EVENT_MASK 0x0C01
#define OP_RESET 0x0C03
#define OP_WRITE_LOCAL_NAME 0x0C13
#define OP_WRITE_PAGE_TIMEOUT 0x0C18
#define OP_WRITE_SCAN_ENABLE 0x0C1A
#define OP_WRITE_CLASS 0x0C24
#define OP_WRITE_INQUIRY_MODE 0x0C45
#define OP_WRITE_SSP_MODE 0x0C56
#define OP_WRITE_LE_HOST 0x0C6D
#define OP_READ_VERSION 0x1001
#define OP_READ_FEATURES 0x1003
#define OP_READ_BUFFER 0x1005
#define OP_READ_BDADDR 0x1009
#define OP_LE_EVENT_MASK 0x2001
#define OP_LE_SCAN_PARAM 0x200B
#define OP_LE_SCAN_ENABLE 0x200C

#define EV_INQUIRY_COMPLETE 0x01
#define EV_INQUIRY_RESULT 0x02
#define EV_CONN_COMPLETE 0x03
#define EV_CONN_REQUEST 0x04
#define EV_DISCONN_COMPLETE 0x05
#define EV_AUTH_COMPLETE 0x06
#define EV_REMOTE_NAME 0x07
#define EV_ENCRYPT_CHANGE 0x08
#define EV_CMD_COMPLETE 0x0E
#define EV_CMD_STATUS 0x0F
#define EV_HW_ERROR 0x10
#define EV_NUM_COMP_PKTS 0x13
#define EV_PIN_REQUEST 0x16
#define EV_LINK_KEY_REQUEST 0x17
#define EV_LINK_KEY_NOTIFY 0x18
#define EV_INQUIRY_RSSI 0x22
#define EV_EXT_INQUIRY 0x2F
#define EV_ENCRYPT_REFRESH 0x30
#define EV_IO_CAPA_REQUEST 0x31
#define EV_IO_CAPA_RESPONSE 0x32
#define EV_USER_CONFIRM 0x33
#define EV_USER_PASSKEY_REQ 0x34
#define EV_SSP_COMPLETE 0x36
#define EV_USER_PASSKEY_NOTIFY 0x3B
#define EV_KEYPRESS_NOTIFY 0x3C
#define EV_LE_META 0x3E
#define EV_VENDOR 0xFF

/* lo de Intel (intel.c) que llega como suceso del fabricante */
void intel_suceso(const u8 *p, int n);

static u16 le16(const u8 *p) { return p[0] | (p[1] << 8); }
static u32 le24(const u8 *p) { return p[0] | (p[1] << 8) | ((u32)p[2] << 16); }
static u32 le32(const u8 *p) { return p[0] | (p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24); }

/* ============================================================ los nombres */
static char dirt[2][24];
static int dirt_k;

const char *bt_dir_texto(const u8 *d)
{
	static const char h[] = "0123456789ABCDEF";
	char *t = dirt[dirt_k ^= 1];
	int i;

	/* (la dirección va al revés: el octeto 5 es el primero que se escribe) */
	for (i = 0; i < 6; i++) {
		t[i * 3] = h[d[5 - i] >> 4];
		t[i * 3 + 1] = h[d[5 - i] & 15];
		t[i * 3 + 2] = i < 5 ? ':' : 0;
	}
	return t;
}

/* (BT.LOG) solo los dos últimos octetos, como WIFI.LOG */
const char *bt_dir_corta(const u8 *d)
{
	char *t = (char *)bt_dir_texto(d);

	memcpy(t, "xx:xx:xx:xx", 11);
	return t;
}

const char *bt_clase_texto(u32 clase, u16 aspecto)
{
	u32 mayor = (clase >> 8) & 0x1F, menor = (clase >> 2) & 0x3F;

	if (aspecto == 0x03C1)
		return "teclado";
	if (aspecto == 0x03C2)
		return "ratón";
	if (aspecto >= 0x03C0 && aspecto <= 0x03CF)
		return "aparato de entrada";
	switch (mayor) {
	case 1: return "ordenador";
	case 2: return "teléfono";
	case 3: return "red";
	case 4: return "sonido";
	case 5:
		switch (menor >> 4) {
		case 1: return "teclado";
		case 2: return "ratón";
		case 3: return "teclado y ratón";
		}
		return (menor & 0x0F) == 1 || (menor & 0x0F) == 2 ? "mando de juegos" : "aparato de entrada";
	case 6: return "imagen";
	}
	return "aparato";
}

struct aparato *bt_aparato(const u8 *dir, int crear)
{
	int i, libre = -1, viejo = -1;

	for (i = 0; i < MAX_APARATOS; i++) {
		if (aparatos[i].usado && !memcmp(aparatos[i].dir, dir, 6))
			return &aparatos[i];
		if (!aparatos[i].usado && libre < 0)
			libre = i;
		/* (si no cabe, el primero visto que no esté emparejado ni conectado) */
		if (aparatos[i].usado && !aparatos[i].emparejado && !aparatos[i].conectado && aparatos[i].estado == A_NADA && viejo < 0)
			viejo = i;
	}
	if (!crear)
		return 0;
	if (libre < 0)
		libre = viejo;
	if (libre < 0)
		return 0;
	memset(&aparatos[libre], 0, sizeof aparatos[libre]);
	aparatos[libre].usado = 1;
	memcpy(aparatos[libre].dir, dir, 6);
	aparatos[libre].r_tec = aparatos[libre].r_rat = -1;
	aparatos[libre].sin_nombre = 1;
	return &aparatos[libre];
}

static struct aparato *por_handle(u16 h)
{
	int i;

	for (i = 0; i < MAX_APARATOS; i++)
		if (aparatos[i].usado && aparatos[i].conectado && aparatos[i].handle == h)
			return &aparatos[i];
	return 0;
}

static void aviso(const char *t)
{
	int n = strlen(t);

	if (n > (int)sizeof bt.aviso - 1)
		n = sizeof bt.aviso - 1;
	memcpy(bt.aviso, t, n);
	bt.aviso[n] = 0;
	bt_estado_cambia();
}

static const char *nombre_de(struct aparato *a)
{
	return a->nombre[0] ? a->nombre : bt_clase_texto(a->clase, a->aspecto);
}

/* ======================================================== lo que llega */
/* Lo que da el USB son trozos (de 64 octetos como mucho): se juntan aquí,
 * cada extremo por su lado, hasta tener un suceso (2 + su largo) o un
 * paquete ACL (4 + su largo) entero, como btusb_recv_intr y btusb_recv_bulk */
static struct junta { u8 b[1200]; int n; } j_int, j_gra;
static void suceso(const u8 *p, int n);
static void acl(const u8 *p, int n);

static void junta(struct junta *j, const u8 *d, int n, int sucesos)
{
	while (n > 0) {
		int k = n, quiere;
		if (k > (int)sizeof j->b - j->n)
			k = sizeof j->b - j->n;
		memcpy(j->b + j->n, d, k);
		j->n += k, d += k, n -= k;
		for (;;) {
			if (sucesos)
				quiere = j->n >= 2 ? 2 + j->b[1] : 0;
			else
				quiere = j->n >= 4 ? 4 + le16(j->b + 2) : 0;
			if (!quiere || j->n < quiere) {
				if (quiere > (int)sizeof j->b) {
					bt_log("Un paquete de %u octetos no cabe: se tira lo que había\n", quiere);
					j->n = 0;
				}
				break;
			}
			if (sucesos)
				suceso(j->b, quiere);
			else
				acl(j->b, quiere);
			memmove(j->b, j->b + quiere, j->n - quiere);
			j->n -= quiere;
		}
	}
}

/* lo que haya llegado (esperando hasta «espera» tics): 1 si ha llegado algo */
int hci_recoge(u32 espera)
{
	u8 b[512];
	int de, n, algo = 0;

	while ((n = tr_recibe(b, sizeof b, &de, algo ? 0 : espera)) > 0) {
		algo = 1;
		if (de == DE_INTERRUPCION)
			junta(&j_int, b, n, 1);
		else
			junta(&j_gra, b, n, bt.intel_cargador);
	}
	if (n < 0 && bt.listo) {
		bt.listo = 0;
		bt_log("El adaptador ya no está (desenchufado)\n");
	}
	return algo;
}

/* ================================================================ mandar */
/* las que no caben (el adaptador dice cuántas admite a la vez) esperan aquí */
#define MAX_COLA 24
static struct { u16 op; u8 n; u8 p[64]; } cola[MAX_COLA];
static int n_cola;

static int manda_ya(u16 op, const void *p, int n)
{
	u8 b[260];

	b[0] = op, b[1] = op >> 8, b[2] = n;
	memcpy(b + 3, p, n);
	if (bt_registro_todo)
		bt_log("HCI orden %04X (%u octetos)\n", op, n);
	bt.ordenes_libres--;
	/* (Intel en su cargador: la 0xFC09, por el de granel, como btusb_send_frame_intel) */
	return tr_envia(bt.intel_cargador == 2 && op == 0xFC09 ? TR_ORDEN_GRANEL : TR_ORDEN, b, 3 + n) < 0 ? -1 : 0;
}

int hci_manda(u16 op, const void *p, int n)
{
	if (bt.ordenes_libres > 0 && !n_cola)
		return manda_ya(op, p, n);
	if (n_cola >= MAX_COLA || n > 64) {
		bt_log("La cola de órdenes está llena: no va la %04X\n", op);
		return -1;
	}
	cola[n_cola].op = op;
	cola[n_cola].n = n;
	memcpy(cola[n_cola].p, p, n);
	n_cola++;
	return 0;
}

static void saca_cola(void)
{
	while (n_cola && bt.ordenes_libres > 0) {
		manda_ya(cola[0].op, cola[0].p, cola[0].n);
		memmove(cola, cola + 1, (n_cola - 1) * sizeof cola[0]);
		n_cola--;
	}
}

/* esperar a una orden: su Command Complete (o un Command Status con fallo) */
static u16 esp_op;
static int esp_hecha, esp_n;
static u8 *esp_r;
static int esp_cabe;

int hci_espera(u16 op, const void *p, int n, u8 *r, int cabe, u32 tics)
{
	u32 t0 = bt_tics();

	esp_op = op, esp_hecha = 0, esp_r = r, esp_cabe = cabe, esp_n = 0;
	/* (si el adaptador no ha dicho aún que admite órdenes, se le manda igual) */
	if (bt.ordenes_libres <= 0)
		bt.ordenes_libres = 1;
	if (manda_ya(op, p, n) < 0) {
		esp_op = 0;
		return -1;
	}
	while (!esp_hecha && bt_tics() - t0 < tics) {
		/* (sin dormir: el firmware de Intel son miles de órdenes seguidas) */
		hci_recoge(0x80000000u | 2);
	}
	esp_op = 0;
	if (!esp_hecha) {
		bt_log("La orden %04X no ha contestado en %u s\n", op, tics / 100);
		return -1;
	}
	return esp_n;
}

int hci_acl(u16 h, const u8 *d, int n)
{
	u8 b[1100];
	int primero = 1, k, mtu = bt.acl_mtu ? bt.acl_mtu : 27;

	if (mtu > 1024)
		mtu = 1024;
	while (n > 0) {
		k = n > mtu ? mtu : n;
		b[0] = h, b[1] = ((h >> 8) & 0x0F) | (primero ? 0x20 : 0x10);	/* PB: 10 el primero, 01 lo que sigue */
		b[2] = k, b[3] = k >> 8;
		memcpy(b + 4, d, k);
		if (tr_envia(TR_ACL, b, 4 + k) < 0)
			return -1;
		bt.acl_libres--;
		d += k, n -= k, primero = 0;
	}
	return 0;
}

/* ============================================= los sucesos de la búsqueda */
static void pon_nombre(struct aparato *a, const u8 *t, int n)
{
	int i;

	for (i = 0; i < n && t[i]; i++)
		;
	/* (llega en UTF-8; se guarda como lo enseñan las ventanas) */
	bt_a_437(a->nombre, t, i, sizeof a->nombre);
	if (a->nombre[0])
		a->sin_nombre = 0;
}

/* el EIR (BR/EDR) y lo anunciado (BLE) tienen la misma forma: largo, tipo, datos */
static void mira_anuncio(struct aparato *a, const u8 *d, int n)
{
	int i = 0, k;

	while (i < n && d[i]) {
		int l = d[i], t;
		if (i + 1 + l > n)
			break;
		t = d[i + 1];
		if (t == 0x08 || t == 0x09) {
			if (t == 0x09 || !a->nombre[0])
				pon_nombre(a, d + i + 2, l - 1);
		} else if (t == 0x19 && l >= 3)
			a->aspecto = le16(d + i + 2);
		else if ((t == 0x02 || t == 0x03) && l >= 3)
			for (k = 0; k + 1 < l; k += 2)
				if (le16(d + i + 2 + k) == 0x1812)
					a->hid_le = 1;
		i += 1 + l;
	}
}

static void visto(const u8 *dir, int psrm, u32 clase, u16 reloj, int rssi, const u8 *eir, int n_eir)
{
	struct aparato *a = bt_aparato(dir, 1);
	int nuevo;

	if (!a)
		return;
	nuevo = !a->visto;
	a->visto = 1;
	a->psrm = psrm;
	if (clase)
		a->clase = clase;
	a->reloj = reloj | 0x8000;
	a->rssi = rssi;
	if (eir)
		mira_anuncio(a, eir, n_eir);
	if (nuevo)
		bt_log("Visto: %s, %s%s%s (clase %06X, %d dBm)\n", bt_dir_corta(dir), bt_clase_texto(a->clase, 0),
		       a->nombre[0] ? ", " : "", a->nombre, a->clase, rssi);
	bt_estado_cambia();
}

/* ======================================================= el emparejamiento */
static void em_pon(int em, struct aparato *a, u32 codigo)
{
	bt.em = em;
	bt.em_aparato = a ? (int)(a - aparatos) : -1;
	bt.em_codigo = codigo;
	bt.em_tecleados = 0;
	bt.em_desde = bt_tics();
	bt_estado_cambia();
}

static void desconecta(struct aparato *a, int motivo)
{
	u8 p[3];

	if (!a->conectado)
		return;
	p[0] = a->handle, p[1] = a->handle >> 8, p[2] = motivo;
	hci_manda(OP_DISCONNECT, p, 3);
}

static void fallo_conexion(struct aparato *a, const char *por)
{
	char t[120];

	formatear(t, sizeof t, "No se ha podido emparejar con %s: %s", nombre_de(a), por);
	bt_log("%s (%s)\n", t, bt_dir_corta(a->dir));
	aviso(t);
	if (bt.em != EM_NADA && bt.em_aparato == a - aparatos)
		em_pon(EM_MAL, a, 0);
	a->quiere_emparejar = 0;
	a->estado = A_NADA;
	desconecta(a, 0x13);
}

static void conecta(struct aparato *a)
{
	u8 p[13];

	memcpy(p, a->dir, 6);
	p[6] = 0x18, p[7] = 0xCC;		/* DM1, DH1, DM3, DH3, DM5, DH5 */
	p[8] = a->psrm, p[9] = 0;
	p[10] = a->reloj, p[11] = a->reloj >> 8;
	p[12] = 1;				/* que pueda cambiar de papel */
	a->estado = A_CONECTANDO;
	a->desde = bt_tics();
	a->entrante = 0;
	hci_manda(OP_CREATE_CONN, p, 13);
}

/* ya conectado (y, si hacía falta, emparejado y cifrado): lo de encima */
static void cifrado(struct aparato *a)
{
	char t[120];

	a->cifrado = 1;
	if (a->quiere_emparejar) {
		a->quiere_emparejar = 0;
		formatear(t, sizeof t, "Emparejado con %s", nombre_de(a));
		aviso(t);
		if (bt.em != EM_NADA && bt.em_aparato == a - aparatos)
			em_pon(EM_BIEN, a, 0);
	}
	l2_cifrado(a);
	if (!a->entrante)
		l2_empieza_hid(a);
	else
		a->estado = A_HID;
	a->desde = bt_tics();
}

/* ============================================================== un suceso */
static void suceso(const u8 *s, int n)
{
	int ev = s[0], l = s[1], i;
	const u8 *p = s + 2;
	struct aparato *a;
	u8 r[32];

	if (bt_registro_todo && ev != EV_NUM_COMP_PKTS)
		bt_log("HCI suceso %02X (%u octetos)\n", ev, l);
	switch (ev) {
	case EV_CMD_COMPLETE:
		if (l < 3)
			break;
		bt.ordenes_libres = p[0];
		if (esp_op && le16(p + 1) == esp_op) {
			esp_n = l - 3 < esp_cabe ? l - 3 : esp_cabe;
			memcpy(esp_r, p + 3, esp_n);
			esp_hecha = 1;
		} else if (l >= 4 && p[3] && le16(p + 1) != OP_REMOTE_NAME_CANCEL && le16(p + 1) != OP_INQUIRY_CANCEL)
			bt_log("La orden %04X ha fallado: %02X\n", le16(p + 1), p[3]);
		saca_cola();
		break;
	case EV_CMD_STATUS:
		if (l < 4)
			break;
		bt.ordenes_libres = p[1];
		if (esp_op && le16(p + 2) == esp_op && p[0]) {
			esp_n = 1;
			esp_r[0] = p[0];
			esp_hecha = 1;
		}
		if (p[0]) {
			u16 op = le16(p + 2);
			bt_log("La orden %04X no se admite ahora: %02X\n", op, p[0]);
			if (op == OP_CREATE_CONN)
				for (i = 0; i < MAX_APARATOS; i++)
					if (aparatos[i].usado && aparatos[i].estado == A_CONECTANDO)
						fallo_conexion(&aparatos[i], "el adaptador no deja conectar");
			if (op == OP_INQUIRY) {
				bt.buscando = 0;
				bt_estado_cambia();
			}
		}
		saca_cola();
		break;
	case EV_NUM_COMP_PKTS:
		for (i = 0; i < p[0] && 1 + i * 4 + 4 <= l; i++)
			bt.acl_libres += le16(p + 1 + i * 4 + 2);
		if (bt.acl_libres > bt.acl_max)
			bt.acl_libres = bt.acl_max;
		break;
	case EV_HW_ERROR:
		bt_log("El adaptador dice que ha tenido un fallo de hardware (%02X)\n", l ? p[0] : 0);
		break;
	case EV_INQUIRY_RESULT:
		for (i = 0; i < p[0] && 1 + i * 14 + 14 <= l; i++) {
			const u8 *q = p + 1 + i * 14;
			visto(q, q[6], le24(q + 9), le16(q + 12), 0, 0, 0);
		}
		break;
	case EV_INQUIRY_RSSI:
		for (i = 0; i < p[0] && 1 + i * 14 + 14 <= l; i++) {
			const u8 *q = p + 1 + i * 14;
			visto(q, q[6], le24(q + 8), le16(q + 11), (s8)q[13], 0, 0);
		}
		break;
	case EV_EXT_INQUIRY:
		if (l >= 15)
			visto(p + 1, p[7], le24(p + 9), le16(p + 12), (s8)p[14], p + 15, l - 15);
		break;
	case EV_INQUIRY_COMPLETE:
		bt.buscando = 0;
		bt_log("Búsqueda acabada\n");
		bt_estado_cambia();
		break;
	case EV_REMOTE_NAME:
		if (l >= 7 && (a = bt_aparato(p + 1, 0))) {
			if (!p[0]) {
				pon_nombre(a, p + 7, l - 7);
				bt_log("Nombre de %s: «%s»\n", bt_dir_corta(a->dir), a->nombre);
				bt_guarda();
			} else
				a->sin_nombre = 2;	/* (no lo da: no se pide más) */
			bt_estado_cambia();
		}
		break;
	case EV_LE_META:
		if (l >= 2 && p[0] == 0x02) {
			int k, m = p[1], j = 2;
			for (k = 0; k < m && j + 9 <= l; k++) {
				int dl = p[j + 8];
				if (j + 9 + dl + 1 > l)
					break;
				a = bt_aparato(p + j + 2, 1);
				if (a) {
					int nuevo = !a->visto;
					a->le = 1, a->le_tipo = p[j + 1], a->visto = 1;
					a->rssi = (s8)p[j + 9 + dl];
					mira_anuncio(a, p + j + 9, dl);
					if (nuevo || (a->hid_le && !(a->sin_nombre & 0x80)))
						bt_log("Visto por BLE: %s, %s%s%s%s (%d dBm)\n", bt_dir_corta(a->dir), bt_clase_texto(0, a->aspecto),
						       a->nombre[0] ? ", " : "", a->nombre, a->hid_le ? ", HID por GATT" : "", a->rssi);
					if (a->hid_le)
						a->sin_nombre |= 0x80;
					bt_estado_cambia();
				}
				j += 10 + dl;
			}
		}
		break;
	case EV_CONN_REQUEST:
		if (l < 10)
			break;
		a = bt_aparato(p, 0);
		memcpy(r, p, 6);
		if (a && (a->emparejado || a->quiere_emparejar) && bt.encendido && p[9] == 1) {
			r[6] = 0x01;		/* (nos quedamos de esclavo: lo que admiten todos) */
			a->entrante = 1;
			a->estado = A_CONECTANDO;
			a->desde = bt_tics();
			if (le24(p + 6))
				a->clase = le24(p + 6);
			bt_log("%s quiere conectarse: se acepta\n", bt_dir_corta(p));
			hci_manda(OP_ACCEPT_CONN, r, 7);
		} else {
			r[6] = 0x0F;		/* «dirección no aceptada» */
			bt_log("%s quiere conectarse y no está emparejado: no\n", bt_dir_corta(p));
			hci_manda(OP_REJECT_CONN, r, 7);
		}
		break;
	case EV_CONN_COMPLETE:
		if (l < 11)
			break;
		a = bt_aparato(p + 3, 0);
		if (!a || p[9] != 1)
			break;
		if (p[0]) {
			a->estado = A_NADA;
			bt_log("No se conecta con %s: %02X\n", bt_dir_corta(a->dir), p[0]);
			if (a->quiere_emparejar)
				fallo_conexion(a, p[0] == 0x04 ? "no contesta (¿está en modo de emparejar?)" : "no se ha podido conectar");
			break;
		}
		a->handle = le16(p + 1) & 0x0FFF;
		a->conectado = 1;
		a->cifrado = 0;
		bt_log("Conectado con %s (%s)\n", bt_dir_corta(a->dir), nombre_de(a));
		l2_conectado(a);
		/* autenticarse (con la clave guardada, o emparejando) y cifrar */
		a->estado = A_AUTENTICANDO;
		a->desde = bt_tics();
		r[0] = a->handle, r[1] = a->handle >> 8;
		hci_manda(OP_AUTH_REQUESTED, r, 2);
		bt_estado_cambia();
		break;
	case EV_DISCONN_COMPLETE:
		if (l < 4 || !(a = por_handle(le16(p + 1) & 0x0FFF)))
			break;
		bt_log("Desconectado de %s (motivo %02X)\n", bt_dir_corta(a->dir), p[3]);
		l2_desconectado(a);
		if (a->quiere_emparejar)
			fallo_conexion(a, "se ha cortado");
		a->conectado = a->cifrado = 0;
		a->estado = A_NADA;
		bt_estado_cambia();
		break;
	case EV_LINK_KEY_REQUEST:
		a = bt_aparato(p, 0);
		memcpy(r, p, 6);
		if (a && a->emparejado && !a->quiere_emparejar) {
			memcpy(r + 6, a->clave, 16);
			hci_manda(OP_LINK_KEY_REPLY, r, 22);
		} else
			hci_manda(OP_LINK_KEY_NEG, r, 6);
		break;
	case EV_PIN_REQUEST:
		/* el emparejamiento de antes de 2.1: un teclado, el PIN tecleado en él */
		a = bt_aparato(p, 0);
		memcpy(r, p, 6);
		if (!a || !a->quiere_emparejar) {
			hci_manda(OP_PIN_NEG, r, 6);
			break;
		}
		if (((a->clase >> 8) & 0x1F) == 5 && ((a->clase >> 6) & 1)) {
			u32 v;
			bt_azar(&v, 4);
			v = 100000 + v % 900000;
			formatear((char *)r + 7, 17, "%u", v);
			em_pon(EM_PIN, a, v);
			bt_log("Emparejamiento antiguo (PIN) con un teclado: se enseña el PIN\n");
		} else {
			memcpy(r + 7, "0000", 5);
			bt_log("Emparejamiento antiguo (PIN): se prueba «0000»\n");
		}
		r[6] = strlen((char *)r + 7);
		memset(r + 7 + r[6], 0, 16 - r[6]);
		hci_manda(OP_PIN_REPLY, r, 23);
		break;
	case EV_IO_CAPA_REQUEST:
		a = bt_aparato(p, 0);
		memcpy(r, p, 6);
		if (!a || (!a->quiere_emparejar && !a->emparejado)) {
			r[6] = 0x18;		/* «emparejar no se permite» */
			hci_manda(OP_IO_CAPA_NEG, r, 7);
			break;
		}
		r[6] = 0x01;			/* pantalla y Sí/No (DisplayYesNo) */
		r[7] = 0x00;			/* sin OOB */
		r[8] = 0x05;			/* «General Bonding», con MITM si se puede */
		hci_manda(OP_IO_CAPA_REPLY, r, 9);
		if (bt.em == EM_NADA || bt.em >= EM_BIEN)
			em_pon(EM_EN_CURSO, a, 0);
		break;
	case EV_IO_CAPA_RESPONSE:
		if (l >= 9 && (a = bt_aparato(p, 0))) {
			static const char *const io[] = { "solo pantalla", "pantalla y sí/no", "solo teclado", "nada (Just Works)", "teclado y pantalla" };
			bt_log("SSP: %s tiene «%s»\n", bt_dir_corta(p), p[6] < 5 ? io[p[6]] : "?");
			a->intentos = p[6];	/* (se guarda su IO para la confirmación) */
		}
		break;
	case EV_USER_CONFIRM:
		a = bt_aparato(p, 0);
		memcpy(r, p, 6);
		if (!a || !a->quiere_emparejar) {
			hci_manda(OP_USER_CONFIRM_NEG, r, 6);
			break;
		}
		/* sin pantalla en el otro (un ratón): Just Works, se acepta solo;
		 * si no, la comparación numérica: lo dice quien está delante */
		if (a->intentos == 3 || a->intentos == 0 || a->intentos == 2) {
			bt_log("SSP: Just Works: se acepta\n");
			hci_manda(OP_USER_CONFIRM_REPLY, r, 6);
		} else {
			em_pon(EM_COMPARAR, a, le32(p + 6) % 1000000);
			bt_log("SSP: comparación numérica: se pregunta\n");
		}
		break;
	case EV_USER_PASSKEY_REQ:
		memcpy(r, p, 6);
		hci_manda(OP_USER_PASSKEY_NEG, r, 6);
		break;
	case EV_USER_PASSKEY_NOTIFY:
		if (l >= 10 && (a = bt_aparato(p, 0))) {
			em_pon(EM_TECLEAR, a, le32(p + 6) % 1000000);
			bt_log("SSP: el código, a teclear en %s\n", bt_dir_corta(a->dir));
		}
		break;
	case EV_KEYPRESS_NOTIFY:
		if (l >= 7 && bt.em == EM_TECLEAR) {
			if (p[6] == 1 && bt.em_tecleados < 6)
				bt.em_tecleados++;
			else if (p[6] == 2 && bt.em_tecleados > 0)
				bt.em_tecleados--;
			else if (p[6] == 3)
				bt.em_tecleados = 0;
			bt_estado_cambia();
		}
		break;
	case EV_SSP_COMPLETE:
		if (l >= 7 && (a = bt_aparato(p + 1, 0)) && p[0])
			bt_log("SSP con %s: no ha salido (%02X)\n", bt_dir_corta(a->dir), p[0]);
		break;
	case EV_LINK_KEY_NOTIFY:
		if (l >= 23 && (a = bt_aparato(p, 0))) {
			memcpy(a->clave, p + 6, 16);
			a->tipo_clave = p[22];
			a->emparejado = 1;
			bt_log("Clave de enlace nueva para %s (de la clase %u; no se escribe aquí)\n", bt_dir_corta(a->dir), a->tipo_clave);
			bt_guarda();
		}
		break;
	case EV_AUTH_COMPLETE:
		if (l < 3 || !(a = por_handle(le16(p + 1) & 0x0FFF)))
			break;
		if (p[0]) {
			bt_log("Autenticación con %s: no (%02X)\n", bt_dir_corta(a->dir), p[0]);
			if (p[0] == 0x06 && a->emparejado && !a->quiere_emparejar) {
				/* (la clave ya no le vale: lo han emparejado con otro) */
				aviso("Un aparato emparejado ya no reconoce la clave: emparéjelo otra vez");
				desconecta(a, 0x05);
				a->estado = A_NADA;
			} else if (a->quiere_emparejar)
				fallo_conexion(a, p[0] == 0x05 || p[0] == 0x06 ? "no se ha aceptado el código" : p[0] == 0x18 ? "no deja emparejar" : "la autenticación no ha salido");
			else {
				desconecta(a, 0x05);
				a->estado = A_NADA;
			}
			break;
		}
		a->estado = A_CIFRANDO;
		r[0] = a->handle, r[1] = a->handle >> 8, r[2] = 1;
		hci_manda(OP_SET_CONN_ENCRYPT, r, 3);
		break;
	case EV_ENCRYPT_CHANGE:
		if (l < 4 || !(a = por_handle(le16(p + 1) & 0x0FFF)))
			break;
		if (p[0] || !p[3]) {
			bt_log("Cifrado con %s: no (%02X)\n", bt_dir_corta(a->dir), p[0]);
			if (a->quiere_emparejar)
				fallo_conexion(a, "no se ha podido cifrar");
			else
				desconecta(a, 0x05);
			break;
		}
		if (!a->cifrado) {
			bt_log("Cifrado con %s\n", bt_dir_corta(a->dir));
			cifrado(a);
		}
		break;
	case EV_ENCRYPT_REFRESH:
		break;
	case EV_VENDOR:
		intel_suceso(p, l);
		break;
	}
}

static void acl(const u8 *b, int n)
{
	u16 h = le16(b) & 0x0FFF, pb = (le16(b) >> 12) & 3, l = le16(b + 2);
	struct aparato *a = por_handle(h);
	static struct { u16 h; int n, quiere; u8 d[1024]; } ens;

	if (!a)
		return;
	b += 4;
	if (pb == 2 || pb == 0) {
		/* el principio de un paquete L2CAP: su largo, en sus 2 primeros octetos */
		if (l < 4)
			return;
		ens.quiere = 4 + le16(b);
		ens.n = 0;
		ens.h = h;
	} else if (ens.h != h || !ens.quiere)
		return;
	if (ens.n + l > (int)sizeof ens.d) {
		ens.quiere = 0;
		return;
	}
	memcpy(ens.d + ens.n, b, l);
	ens.n += l;
	if (ens.n >= ens.quiere) {
		l2_llega(a, ens.d, ens.quiere);
		ens.quiere = 0;
	}
}

/* ============================================================= arrancar */
static int orden_bien(u16 op, const void *p, int n, u8 *r, int cabe)
{
	int k = hci_espera(op, p, n, r, cabe, 300);

	if (k < 1 || r[0]) {
		bt_log("Al arrancar, la orden %04X no ha salido (%s%02X)\n", op, k < 1 ? "sin respuesta; " : "", k < 1 ? 0 : r[0]);
		return -1;
	}
	return k;
}

static const char *fabricante(u16 f)
{
	switch (f) {
	case 2: return "Intel";
	case 10: return "CSR (Qualcomm)";
	case 13: return "Texas Instruments";
	case 15: return "Broadcom";
	case 29: return "Qualcomm";
	case 70: return "MediaTek";
	case 93: return "Realtek";
	case 1521: return "Actions";
	}
	return 0;
}

int bt_arranca(u16 vid, u16 pid)
{
	u8 r[260], p[248];
	static const char *const ver[] = { "1.0b", "1.1", "1.2", "2.0", "2.1", "3.0", "4.0", "4.1", "4.2", "5.0", "5.1", "5.2", "5.3", "5.4" };
	int k, ssp, le;
	const char *f;

	memset(&j_int, 0, sizeof j_int);
	memset(&j_gra, 0, sizeof j_gra);
	n_cola = 0;
	l2_reinicia();
	bt.vid = vid, bt.pid = pid;
	bt.listo = bt.encendido = bt.buscando = 0;
	bt.intel_cargador = 0;
	bt.ordenes_libres = 1;
	/* lo del fabricante antes del reset (Linux: hdev->setup, que va antes de hci_init) */
	if (vid == 0x8087 && intel_prepara(pid) < 0)
		return -1;
	if (orden_bien(OP_RESET, 0, 0, r, sizeof r) < 0)
		return -1;
	if ((k = orden_bien(OP_READ_VERSION, 0, 0, r, sizeof r)) < 9)
		return -1;
	bt.version = r[1], bt.revision = le16(r + 2), bt.lmp = r[4], bt.fabricante_lmp = le16(r + 5), bt.lmp_sub = le16(r + 7);
	f = fabricante(bt.fabricante_lmp);
	bt_log("Versión: Bluetooth %s (HCI %u, revisión %04X; LMP %u.%04X), del fabricante %u%s%s%s\n",
	       bt.version < 14 ? ver[bt.version] : "nueva", bt.version, bt.revision, bt.lmp, bt.lmp_sub,
	       bt.fabricante_lmp, f ? " (" : "", f ? f : "", f ? ")" : "");
	if (orden_bien(OP_READ_BDADDR, 0, 0, r, sizeof r) < 7)
		return -1;
	memcpy(bt.dir, r + 1, 6);
	bt_log("Dirección: %s\n", bt_dir_corta(bt.dir));
	if (orden_bien(OP_READ_FEATURES, 0, 0, r, sizeof r) < 9)
		return -1;
	memcpy(bt.rasgos, r + 1, 8);
	if (orden_bien(OP_READ_BUFFER, 0, 0, r, sizeof r) < 8)
		return -1;
	bt.acl_mtu = le16(r + 1), bt.acl_max = le16(r + 4);
	if (!bt.acl_max)
		bt.acl_max = 1;
	bt.acl_libres = bt.acl_max;
	ssp = bt.rasgos[6] & 0x08;
	le = bt.rasgos[4] & 0x40;
	bt_log("Sabe: %s%s%s%s; ACL de %u octetos (%u a la vez)\n", ssp ? "SSP" : "sin SSP (solo el PIN de antes)",
	       bt.rasgos[3] & 0x40 ? ", búsqueda con señal" : "", bt.rasgos[6] & 0x01 ? ", EIR" : "", le ? ", BLE" : ", sin BLE",
	       bt.acl_mtu, bt.acl_max);
	/* los sucesos (los de hci_setup_event_mask de BlueZ) */
	memset(p, 0, 8);
	p[0] = 0xFF, p[1] = 0xFF, p[2] = 0xFB, p[3] = 0xFF;
	if (bt.rasgos[3] & 0x40)
		p[4] |= 0x02;			/* Inquiry Result with RSSI */
	if (bt.rasgos[6] & 0x01)
		p[5] |= 0x40;			/* Extended Inquiry Result */
	if (bt.rasgos[0] & 0x04)
		p[5] |= 0x80;			/* Encryption Key Refresh Complete */
	if (ssp) {
		p[6] |= 0x3F;			/* IO Capability Request ... Simple Pairing Complete */
		p[7] |= 0x1C;			/* User Passkey Notification, Keypress Notification... */
	}
	if (le)
		p[7] |= 0x20;			/* LE Meta */
	orden_bien(OP_SET_EVENT_MASK, p, 8, r, sizeof r);
	if (ssp) {
		p[0] = 1;
		orden_bien(OP_WRITE_SSP_MODE, p, 1, r, sizeof r);
	}
	p[0] = bt.rasgos[6] & 0x01 ? 2 : bt.rasgos[3] & 0x40 ? 1 : 0;
	if (p[0])
		orden_bien(OP_WRITE_INQUIRY_MODE, p, 1, r, sizeof r);
	p[0] = 0x0C, p[1] = 0x01, p[2] = 0x00;	/* un portátil (ordenador, portátil) */
	orden_bien(OP_WRITE_CLASS, p, 3, r, sizeof r);
	memset(p, 0, 248);
	memcpy(p, bt.nombre, strlen(bt.nombre));
	orden_bien(OP_WRITE_LOCAL_NAME, p, 248, r, sizeof r);
	p[0] = 0x00, p[1] = 0x20;		/* 5,12 s para conectar */
	orden_bien(OP_WRITE_PAGE_TIMEOUT, p, 2, r, sizeof r);
	p[0] = 0x05, p[1] = 0x00;		/* cambiar de papel y «sniff» (los teclados lo usan) */
	orden_bien(OP_WRITE_DEF_POLICY, p, 2, r, sizeof r);
	if (le) {
		p[0] = 1, p[1] = 0;
		orden_bien(OP_WRITE_LE_HOST, p, 2, r, sizeof r);
		memset(p, 0, 8);
		p[0] = 0x1F;
		orden_bien(OP_LE_EVENT_MASK, p, 8, r, sizeof r);
	}
	bt.listo = 1;
	return 0;
}

int bt_enciende(void)
{
	u8 p[1] = { 0x02 }, r[8];	/* se le puede conectar (no se deja ver: eso, al emparejar) */

	if (!bt.listo)
		return -1;
	if (hci_espera(OP_WRITE_SCAN_ENABLE, p, 1, r, sizeof r, 200) < 1 || r[0])
		return -1;
	bt.encendido = 1;
	bt_log("Encendido: los emparejados se pueden conectar\n");
	bt_estado_cambia();
	return 0;
}

void bt_apaga(void)
{
	u8 p[1] = { 0 }, r[8];
	int i;

	if (!bt.listo)
		return;
	if (bt.buscando)
		hci_manda(OP_INQUIRY_CANCEL, 0, 0);
	for (i = 0; i < MAX_APARATOS; i++)
		if (aparatos[i].usado && aparatos[i].conectado)
			desconecta(&aparatos[i], 0x15);
	/* (que salga lo de antes) */
	{
		u32 t0 = bt_tics();
		while (bt_tics() - t0 < 50)
			hci_recoge(5);
	}
	hci_espera(OP_WRITE_SCAN_ENABLE, p, 1, r, sizeof r, 200);
	bt.encendido = 0;
	bt.buscando = 0;
	bt_log("Apagado: ni busca ni deja conectar\n");
	bt_estado_cambia();
}

/* ================================================================ buscar */
void bt_busca(void)
{
	u8 p[7];
	int i;

	if (!bt.listo || !bt.encendido || bt.buscando)
		return;
	for (i = 0; i < MAX_APARATOS; i++)
		if (aparatos[i].usado) {
			aparatos[i].visto = 0;
			aparatos[i].sin_nombre &= 0x7F;
			if (aparatos[i].sin_nombre == 2)
				aparatos[i].sin_nombre = 1;
		}
	p[0] = 0x33, p[1] = 0x8B, p[2] = 0x9E;	/* GIAC */
	p[3] = 0x08;				/* 10,24 s */
	p[4] = 0;
	bt.buscando = 1;
	bt.fin_busqueda = bt_tics() + 1500;
	hci_manda(OP_INQUIRY, p, 5);
	if (bt.rasgos[4] & 0x40) {
		/* y BLE a la vez: activo, cada 60 ms durante 30 ms */
		p[0] = 1, p[1] = 0x60, p[2] = 0, p[3] = 0x30, p[4] = 0, p[5] = 0, p[6] = 0;
		hci_manda(OP_LE_SCAN_PARAM, p, 7);
		p[0] = 1, p[1] = 1;
		hci_manda(OP_LE_SCAN_ENABLE, p, 2);
		bt.buscando_le = 1;
	}
	bt_log("Buscando aparatos (10 s)%s\n", bt.buscando_le ? ", también por BLE" : "");
	aviso("Buscando aparatos...");
}

/* lo que no se ha dicho su nombre en la búsqueda: se le pregunta, de uno en uno */
static int pidiendo_nombre = -1;
static u32 nombre_desde;

static void pide_nombres(void)
{
	int i;
	u8 p[10];

	if (pidiendo_nombre >= 0) {
		struct aparato *a = &aparatos[pidiendo_nombre];
		if (!a->usado || a->sin_nombre != 1 || bt_tics() - nombre_desde > 1200) {
			if (a->usado && a->sin_nombre == 1)
				a->sin_nombre = 2;
			pidiendo_nombre = -1;
		} else
			return;
	}
	if (bt.buscando)
		return;
	for (i = 0; i < MAX_APARATOS; i++) {
		struct aparato *a = &aparatos[i];
		if (a->usado && a->visto && !a->le && a->sin_nombre == 1 && a->estado == A_NADA) {
			memcpy(p, a->dir, 6);
			p[6] = a->psrm, p[7] = 0, p[8] = a->reloj, p[9] = a->reloj >> 8;
			hci_manda(OP_REMOTE_NAME, p, 10);
			pidiendo_nombre = i;
			nombre_desde = bt_tics();
			return;
		}
	}
}

/* ============================================================ emparejar */
int bt_empareja(const u8 *dir)
{
	struct aparato *a = bt_aparato(dir, 0);

	if (!a || !bt.listo || !bt.encendido)
		return -1;
	if (a->le && !a->clase) {
		aviso("Ese aparato es de Bluetooth de bajo consumo (BLE): emparejarlos llega en la etapa 2 b");
		return -1;
	}
	if (bt.buscando) {
		hci_manda(OP_INQUIRY_CANCEL, 0, 0);
		bt.buscando = 0;
	}
	if (bt.buscando_le) {
		u8 p[2] = { 0, 0 };
		hci_manda(OP_LE_SCAN_ENABLE, p, 2);
		bt.buscando_le = 0;
	}
	if (a->emparejado) {
		/* (emparejarlo otra vez: la clave de antes no vale) */
		a->emparejado = 0;
		memset(a->clave, 0, 16);
	}
	a->quiere_emparejar = 1;
	em_pon(EM_EN_CURSO, a, 0);
	bt_log("Emparejar con %s (%s)\n", bt_dir_corta(a->dir), nombre_de(a));
	if (a->conectado) {
		u8 r[2] = { a->handle, a->handle >> 8 };
		a->estado = A_AUTENTICANDO;
		hci_manda(OP_AUTH_REQUESTED, r, 2);
	} else
		conecta(a);
	return 0;
}

void bt_responde(int si)
{
	struct aparato *a;
	u8 r[6];

	if (bt.em != EM_COMPARAR || bt.em_aparato < 0)
		return;
	a = &aparatos[bt.em_aparato];
	memcpy(r, a->dir, 6);
	hci_manda(si ? OP_USER_CONFIRM_REPLY : OP_USER_CONFIRM_NEG, r, 6);
	em_pon(EM_EN_CURSO, a, 0);
}

void bt_cancela(void)
{
	int i;

	for (i = 0; i < MAX_APARATOS; i++)
		if (aparatos[i].usado && aparatos[i].quiere_emparejar)
			fallo_conexion(&aparatos[i], "se ha cancelado");
	bt.em = EM_NADA;
	bt_estado_cambia();
}

int bt_olvida(const u8 *dir)
{
	struct aparato *a = bt_aparato(dir, 0);

	if (!a)
		return -1;
	bt_log("Olvidar %s\n", bt_dir_corta(a->dir));
	if (a->conectado) {
		l2_corta_hid(a, 1);
		desconecta(a, 0x13);
	}
	a->emparejado = 0;
	a->es_hid = 0;
	a->desc_n = 0;
	memset(a->clave, 0, sizeof a->clave);
	bt_guarda();
	bt_estado_cambia();
	return 0;
}

int bt_conecta(const u8 *dir)
{
	struct aparato *a = bt_aparato(dir, 0);

	if (!a || !a->emparejado || a->conectado || a->estado != A_NADA || !bt.encendido)
		return -1;
	conecta(a);
	return 0;
}

/* al arrancar, uno a uno, los emparejados (los que no estén cerca: a los 5 s, el siguiente) */
static int reconectando = -1;

void bt_reconecta_todos(void)
{
	reconectando = 0;
}

static void sigue_reconectando(void)
{
	int i;

	if (reconectando < 0)
		return;
	for (i = 0; i < MAX_APARATOS; i++)
		if (aparatos[i].estado == A_CONECTANDO)
			return;
	while (reconectando < MAX_APARATOS) {
		struct aparato *a = &aparatos[reconectando++];
		if (a->usado && a->emparejado && a->es_hid && !a->conectado && a->estado == A_NADA) {
			bt_log("Reconectando con %s (%s)\n", bt_dir_corta(a->dir), nombre_de(a));
			conecta(a);
			return;
		}
	}
	reconectando = -1;
}

/* ============================================================ la vuelta */
void bt_vuelta(void)
{
	int i;
	u32 t;

	hci_recoge(1);
	t = bt_tics();
	if (!bt.listo)
		return;
	if (bt.buscando && (int)(t - bt.fin_busqueda) > 0) {
		bt.buscando = 0;
		bt_estado_cambia();
	}
	if (bt.buscando_le && !bt.buscando) {
		u8 p[2] = { 0, 0 };
		hci_manda(OP_LE_SCAN_ENABLE, p, 2);
		bt.buscando_le = 0;
		aviso("Búsqueda acabada");
	}
	pide_nombres();
	sigue_reconectando();
	/* lo que se ha quedado a medias */
	for (i = 0; i < MAX_APARATOS; i++) {
		struct aparato *a = &aparatos[i];
		if (!a->usado || a->estado == A_NADA || a->estado == A_LISTO)
			continue;
		if ((int)(t - a->desde) > (a->quiere_emparejar ? 6000 : 2500)) {
			bt_log("%s: se ha quedado en el paso %u\n", bt_dir_corta(a->dir), a->estado);
			if (a->quiere_emparejar)
				fallo_conexion(a, "se ha pasado el tiempo");
			else {
				desconecta(a, 0x13);
				a->estado = A_NADA;
			}
		}
	}
	/* (el resultado del emparejamiento se enseña unos segundos) */
	if ((bt.em == EM_BIEN || bt.em == EM_MAL) && (int)(t - bt.em_desde) > 1500) {
		bt.em = EM_NADA;
		bt_estado_cambia();
	}
	l2_vuelta();
}

/* ========================================================= para la ventana */
void bt_lista(struct bt_estado *e)
{
	int i, k = 0, pasada;

	e->buscando = bt.buscando;
	memcpy(e->dir, bt.dir, 6);
	memcpy(e->aviso, bt.aviso, sizeof e->aviso);
	e->em = bt.em;
	e->codigo = bt.em_codigo;
	e->tecleados = bt.em_tecleados;
	e->em_nombre[0] = 0;
	if (bt.em != EM_NADA && bt.em_aparato >= 0) {
		const char *t = nombre_de(&aparatos[bt.em_aparato]);
		int n = strlen(t);
		if (n > 47)
			n = 47;
		memcpy(e->em_nombre, t, n);
		e->em_nombre[n] = 0;
	}
	/* los emparejados primero; luego los vistos, de más señal a menos */
	for (pasada = 0; pasada < 2; pasada++)
		for (i = 0; i < MAX_APARATOS && k < MAX_LISTA; i++) {
			struct aparato *a = &aparatos[i];
			struct bt_fila *f;
			int j;
			if (!a->usado || (pasada == 0) != (a->emparejado != 0) || (pasada == 1 && !a->visto))
				continue;
			f = &e->fila[k];
			if (pasada == 1) {
				/* (por la señal) */
				for (j = k; j > 0 && !(e->fila[j - 1].banderas & BL_EMPAREJADO) && e->fila[j - 1].rssi < a->rssi; j--)
					e->fila[j] = e->fila[j - 1];
				f = &e->fila[j];
			}
			memset(f, 0, sizeof *f);
			memcpy(f->dir, a->dir, 6);
			f->rssi = a->rssi;
			f->clase = a->clase;
			f->aspecto = a->aspecto;
			f->hid = a->hid_clase;
			f->banderas = (a->emparejado ? BL_EMPAREJADO : 0) | (a->estado == A_LISTO ? BL_CONECTADO : 0) |
				      (a->visto ? BL_VISTO : 0) | (a->le ? BL_LE : 0) | (a->hid_le ? BL_HID_LE : 0) |
				      (a->estado != A_NADA && a->estado != A_LISTO ? BL_CONECTANDO : 0);
			j = strlen(a->nombre);
			if (j > 47)
				j = 47;
			memcpy(f->nombre, a->nombre, j);
			k++;
		}
	e->n = k;
}
