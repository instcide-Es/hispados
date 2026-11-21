/*
 * w80211.c - La gestión 802.11 de Hispa-486, en modo estación: búsqueda de
 * redes, autenticación y asociación abiertas, WPA2-Personal (con wpa.c), el
 * paso de tramas Ethernet II a 802.11 y al revés, y la reconexión.
 *
 * Es lo que en Linux hace mac80211, reducido a un cliente de una red de
 * casa en 2,4 GHz. Habla con la radio por «struct radio» (radio_brcm.c para
 * la Broadcom, sim.c para las pruebas).
 *
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 */
#include "nucleo.h"

#define T_CANAL 130		/* ms en cada canal al buscar */
#define T_RESP 400		/* espera de la respuesta de autenticación o asociación */
#define T_CLAVES 4000		/* espera del acuerdo de claves */
#define T_PERDIDA 8000		/* sin oír al punto de acceso: se ha ido */
#define T_REINTENTO 5000	/* entre intentos de reconexión */
#define T_VIVO 20000		/* trama vacía si no se envía nada */
#define INTENTOS 4

static struct {
	struct radio *r;
	struct w_red redes[W_MAX_REDES];
	int nredes;
	/* búsqueda */
	int barrido;		/* 0 no; si no, el canal que se escucha */
	d32 t_canal;
	int barridos;
	/* lo que se quiere */
	int quiere;
	o8 ssid[33];
	int ssid_n;
	o8 pmk[32];
	int con_clave;
	/* la conexión */
	int estado, fallo;
	struct w_red ap;
	int intentos;
	d32 t_limite;
	d32 t_oido, t_enviado, t_reintento;
	p16 aid;
	p16 seq_rx;
	int hay_seq;
	struct wpa wpa;
	int busca_y_conecta;
	d32 tx, rx, tx_err, rx_desc, reconexiones;
	int senal;
} w;

struct radio *radio;

static const o8 difusion[6] = { 255, 255, 255, 255, 255, 255 };
static const o8 snap[6] = { 0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00 };

static const char *const nombres_estado[] = {
	"parado", "buscando la red", "autenticando", "asociando", "acordando las claves", "conectado"
};

static void estado(int e)
{
	if (w.estado != e)
		hl_log(2, "802.11: %s -> %s\n", nombres_estado[w.estado], nombres_estado[e]);
	w.estado = e;
}

void w80211_inicia(struct radio *r)
{
	memset(&w, 0, sizeof(w));
	w.r = r;
	radio = r;
}

/* la velocidad más baja que admite el punto de acceso (para la gestión) */
static int vel_baja(const struct w_red *a)
{
	int i, v = 0;

	for (i = 0; i < a->nvel; i++) {
		int x = a->velocidades[i] & 0x7f;
		if ((a->velocidades[i] & 0x80) && (!v || x < v))
			v = x;
	}
	if (!v)
		for (i = 0; i < a->nvel; i++) {
			int x = a->velocidades[i] & 0x7f;
			if (!v || x < v)
				v = x;
		}
	return v ? v : 2;
}

/* ------------------------------------------------------------- búsqueda */
static int pon_cab_gestion(o8 *t, int subtipo, const o8 *a1, const o8 *a3)
{
	memset(t, 0, 24);
	t[0] = subtipo;
	memcpy(t + 4, a1, 6);
	memcpy(t + 10, w.r->mac, 6);
	memcpy(t + 16, a3, 6);
	return 24;
}

static int pon_velocidades(o8 *t)
{
	static const o8 v[] = { 0x82, 0x84, 0x8b, 0x96, 0x0c, 0x12, 0x18, 0x24 };
	static const o8 x[] = { 0x30, 0x48, 0x60, 0x6c };

	t[0] = 1; t[1] = 8; memcpy(t + 2, v, 8);
	t[10] = 50; t[11] = 4; memcpy(t + 12, x, 4);
	return 16;
}

static void manda_sondeo(const o8 *ssid, int n)
{
	o8 t[100];
	int l = pon_cab_gestion(t, 0x40, difusion, difusion);

	t[l++] = 0; t[l++] = n;
	memcpy(t + l, ssid, n); l += n;
	l += pon_velocidades(t + l);
	w.r->enviar(t, l, 2);
}

static void vacia(int ahorro)
{
	o8 t[24];

	memset(t, 0, 24);
	t[0] = 0x48; t[1] = 0x01 | (ahorro ? 0x10 : 0);
	memcpy(t + 4, w.ap.bssid, 6);
	memcpy(t + 10, w.r->mac, 6);
	memcpy(t + 16, w.ap.bssid, 6);
	w.r->enviar(t, 24, vel_baja(&w.ap));
	w.t_enviado = hl_jiffies();
}

static void barrido_canal(int c)
{
	w.barrido = c;
	w.t_canal = hl_jiffies() + T_CANAL;
	w.r->canal(c);
	/* en los canales 12 y 13 solo se escucha (hay países que no dejan
	 * emitir sin haber oído antes una baliza) */
	if (c <= 11) {
		manda_sondeo((const o8 *)"", 0);
		if (w.quiere && w.ssid_n)
			manda_sondeo(w.ssid, w.ssid_n);	/* por si la red está oculta */
	}
}

void w80211_barrer(void)
{
	if (w.barrido)
		return;
	hl_log(2, "802.11: buscando redes (canales 1 a 13)\n");
	if (w.estado == W_CONECTADO)
		vacia(1);		/* que el punto de acceso nos guarde lo que llegue */
	w.nredes = 0;
	w.barridos++;
	if (w.r->barrido)
		w.r->barrido(1);
	barrido_canal(1);
}

int w80211_barriendo(void)
{
	return w.barrido != 0;
}

int w80211_redes(struct w_red **l)
{
	*l = w.redes;
	return w.nredes;
}

static const char *nombre_seg(int s)
{
	static const char *const n[] = { "abierta", "WEP", "WPA (TKIP)", "WPA2", "otra", "WPA2 (grupo TKIP)" };
	return n[s];
}

static void fin_barrido(void)
{
	int i;

	w.barrido = 0;
	if (w.r->barrido)
		w.r->barrido(0);
	hl_log(2, "802.11: búsqueda terminada, %d redes\n", w.nredes);
	for (i = 0; i < w.nredes; i++) {
		struct w_red *r = &w.redes[i];
		hl_log(2, "  %02x:%02x:%02x:%02x:%02x:%02x canal %2d %4d dBm %-17s «%s»\n",
		       r->bssid[0], r->bssid[1], r->bssid[2], r->bssid[3], r->bssid[4], r->bssid[5],
		       r->canal, r->senal, nombre_seg(r->seguridad), r->ssid);
	}
	if (w.estado == W_CONECTADO) {
		w.r->canal(w.ap.canal);
		vacia(0);
		w.t_oido = hl_jiffies();
	}
}

/* el elemento RSN: ¿WPA2-Personal con CCMP? Deja en r->rsn el nuestro. */
static int mira_rsn(struct w_red *r, const o8 *e, int n)
{
	int i, np, na, ccmp = 0, psk = 0, grupo, cap = 0;

	if (n < 8 || e[0] != 1 || e[1] != 0)
		return W_SEG_OTRA;
	if (e[2] != 0x00 || e[3] != 0x0f || e[4] != 0xac)
		return W_SEG_OTRA;
	grupo = e[5];
	np = e[6] | (e[7] << 8);
	i = 8;
	if (i + 4 * np + 2 > n)
		return W_SEG_OTRA;
	for (; np > 0; np--, i += 4)
		if (e[i] == 0x00 && e[i + 1] == 0x0f && e[i + 2] == 0xac && e[i + 3] == 4)
			ccmp = 1;
	na = e[i] | (e[i + 1] << 8);
	i += 2;
	if (i + 4 * na > n)
		return W_SEG_OTRA;
	for (; na > 0; na--, i += 4)
		if (e[i] == 0x00 && e[i + 1] == 0x0f && e[i + 2] == 0xac && e[i + 3] == 2)
			psk = 1;
	if (i + 2 <= n)
		cap = e[i] | (e[i + 1] << 8);
	if (!ccmp || !psk || (cap & 0x40))	/* 0x40: exige tramas de gestión protegidas */
		return W_SEG_OTRA;
	/* el nuestro: mismo cifrado de grupo, CCMP y clave compartida */
	{
		static const o8 m[] = { 48, 20, 1, 0, 0x00, 0x0f, 0xac, 4, 1, 0, 0x00, 0x0f, 0xac, 4,
					1, 0, 0x00, 0x0f, 0xac, 2, 0, 0 };
		memcpy(r->rsn, m, sizeof(m));
		r->rsn[7] = grupo;
		r->rsn_n = sizeof(m);
	}
	if (grupo == 2)
		return W_SEG_WPA2_TKIP;
	return grupo == 4 ? W_SEG_WPA2 : W_SEG_OTRA;
}

static void baliza(const o8 *t, int n, int senal, int canal_rx)
{
	struct w_red x, *r;
	int i = 36, hay_rsn = 0, hay_wpa = 0, seg_rsn = 0, k;

	if (n < 36)
		return;
	memset(&x, 0, sizeof(x));
	memcpy(x.bssid, t + 16, 6);
	x.intervalo = t[32] | (t[33] << 8);
	x.capacidad = t[34] | (t[35] << 8);
	x.canal = canal_rx;
	x.senal = senal;
	if (!(x.capacidad & 1))
		return;			/* no es un punto de acceso (ad hoc) */
	while (i + 2 <= n) {
		int id = t[i], l = t[i + 1];
		const o8 *d = t + i + 2;
		if (i + 2 + l > n)
			break;
		switch (id) {
		case 0:
			if (l <= 32) {
				memcpy(x.ssid, d, l);
				x.ssid_n = l;
			}
			break;
		case 1:
		case 50:
			for (k = 0; k < l && x.nvel < 16; k++)
				x.velocidades[x.nvel++] = d[k];
			break;
		case 3:
			if (l == 1)
				x.canal = d[0];
			break;
		case 48:
			hay_rsn = 1;
			seg_rsn = mira_rsn(&x, d, l);
			break;
		case 221:
			if (l >= 4 && d[0] == 0x00 && d[1] == 0x50 && d[2] == 0xf2 && d[3] == 1)
				hay_wpa = 1;
			break;
		}
		i += 2 + l;
	}
	/* una baliza oída de refilón desde el canal de al lado no vale */
	if (x.canal != canal_rx && w.barrido)
		return;
	if (hay_rsn)
		x.seguridad = seg_rsn;
	else if (hay_wpa)
		x.seguridad = W_SEG_WPA;
	else if (x.capacidad & 0x10)
		x.seguridad = W_SEG_WEP;
	else
		x.seguridad = W_SEG_ABIERTA;
	x.visto = hl_jiffies();
	/* los octetos raros del nombre, fuera (para enseñarlo) */
	for (r = w.redes; r < w.redes + w.nredes; r++)
		if (memcmp(r->bssid, x.bssid, 6) == 0)
			break;
	if (r == w.redes + w.nredes) {
		if (w.nredes == W_MAX_REDES) {
			/* la tabla está llena: se quita la más débil */
			struct w_red *p = w.redes;
			for (r = w.redes; r < w.redes + w.nredes; r++)
				if (r->senal < p->senal)
					p = r;
			if (p->senal >= x.senal)
				return;
			r = p;
		} else {
			w.nredes++;
		}
	} else if (x.ssid_n == 0 || (x.ssid[0] == 0 && r->ssid_n)) {
		/* baliza de red oculta: se guarda el nombre que dio al contestar */
		memcpy(x.ssid, r->ssid, 33);
		x.ssid_n = r->ssid_n;
	}
	*r = x;
}

/* ------------------------------------------------------------- conexión */
static void suelta(void)
{
	if (w.estado >= W_AUTENTICANDO)
		w.r->bss(NULL, 0, 0, 0, NULL, 0);
	memset(&w.wpa, 0, sizeof(w.wpa));
	w.hay_seq = 0;
}

static void falla(int motivo, const char *texto)
{
	hl_log(0, "802.11: %s\n", texto);
	w.fallo = motivo;
	suelta();
	estado(W_PARADO);
	w.t_reintento = hl_jiffies() + T_REINTENTO;
}

static void manda_aut(void)
{
	o8 t[40];
	int l = pon_cab_gestion(t, 0xb0, w.ap.bssid, w.ap.bssid);

	t[l++] = 0; t[l++] = 0;		/* sistema abierto */
	t[l++] = 1; t[l++] = 0;		/* paso 1 */
	t[l++] = 0; t[l++] = 0;
	w.r->enviar(t, l, vel_baja(&w.ap));
	w.t_limite = hl_jiffies() + T_RESP;
	hl_log(2, "802.11: autenticación (intento %d)\n", w.intentos + 1);
}

static void manda_asoc(void)
{
	o8 t[160];
	int l = pon_cab_gestion(t, 0x00, w.ap.bssid, w.ap.bssid);
	p16 cap = 0x0001 | (w.ap.capacidad & 0x0400) | (w.ap.capacidad & 0x0020);

	if (w.ap.seguridad != W_SEG_ABIERTA)
		cap |= 0x0010;
	t[l++] = cap; t[l++] = cap >> 8;
	t[l++] = 10; t[l++] = 0;	/* cada cuántas balizas escuchamos */
	t[l++] = 0; t[l++] = w.ap.ssid_n;
	memcpy(t + l, w.ap.ssid, w.ap.ssid_n); l += w.ap.ssid_n;
	l += pon_velocidades(t + l);
	if (w.ap.seguridad == W_SEG_WPA2) {
		memcpy(t + l, w.ap.rsn, w.ap.rsn_n);
		l += w.ap.rsn_n;
	}
	w.r->enviar(t, l, vel_baja(&w.ap));
	w.t_limite = hl_jiffies() + T_RESP;
	hl_log(2, "802.11: asociación (intento %d)\n", w.intentos + 1);
}

/* elegir el punto de acceso de la red pedida (el que mejor se oye) y empezar */
static void empieza_conexion(void)
{
	struct w_red *r, *m = NULL;

	/* primero, los de la seguridad que se pide (con clave, WPA2; sin ella,
	 * abierta): una red abierta con el mismo nombre que una WPA2 (una falsa)
	 * no se coge aunque se oiga mejor; si no hay ninguno así, el que mejor se
	 * oiga, para decir por qué no se puede */
	for (r = w.redes; r < w.redes + w.nredes; r++)
		if (r->ssid_n == w.ssid_n && memcmp(r->ssid, w.ssid, w.ssid_n) == 0 &&
		    (w.con_clave ? r->seguridad == W_SEG_WPA2 : r->seguridad == W_SEG_ABIERTA) &&
		    (!m || r->senal > m->senal))
			m = r;
	if (!m)
		for (r = w.redes; r < w.redes + w.nredes; r++)
			if (r->ssid_n == w.ssid_n && memcmp(r->ssid, w.ssid, w.ssid_n) == 0 &&
			    (!m || r->senal > m->senal))
				m = r;
	if (!m) {
		hl_log(1, "802.11: la red «%s» no aparece\n", w.ssid);
		w.fallo = W_F_NO_ESTA;
		estado(W_PARADO);
		w.t_reintento = hl_jiffies() + T_REINTENTO;
		return;
	}
	if ((m->seguridad == W_SEG_ABIERTA) != !w.con_clave || (w.con_clave && m->seguridad != W_SEG_WPA2)) {
		if (m->seguridad == W_SEG_ABIERTA)
			hl_log(0, "802.11: la red «%s» es abierta y se ha dado una contraseña\n", w.ssid);
		else if (m->seguridad == W_SEG_WPA2)
			hl_log(0, "802.11: la red «%s» pide contraseña (WPA2)\n", w.ssid);
		else
			hl_log(0, "802.11: la red «%s» usa una seguridad que no se admite (%s); hace falta WPA2-Personal con AES\n",
			       w.ssid, nombre_seg(m->seguridad));
		w.fallo = W_F_SEGURIDAD;
		estado(W_PARADO);
		w.t_reintento = hl_jiffies() + 6 * T_REINTENTO;
		return;
	}
	w.ap = *m;
	w.senal = m->senal;
	hl_log(2, "802.11: conectando con «%s» (%02x:%02x:%02x:%02x:%02x:%02x, canal %d, %d dBm, %s)\n",
	       w.ap.ssid, m->bssid[0], m->bssid[1], m->bssid[2], m->bssid[3], m->bssid[4], m->bssid[5],
	       m->canal, m->senal, nombre_seg(m->seguridad));
	w.r->canal(w.ap.canal);
	w.r->bss(w.ap.bssid, 0, w.ap.intervalo, (w.ap.capacidad & 0x0400) != 0, w.ap.velocidades, w.ap.nvel);
	w.intentos = 0;
	w.hay_seq = 0;
	estado(W_AUTENTICANDO);
	manda_aut();
}

void w80211_conectar(const o8 *ssid, int n, const o8 *pmk)
{
	w80211_desconectar();
	if (n > 32)
		n = 32;
	memcpy(w.ssid, ssid, n);
	w.ssid[n] = 0;
	w.ssid_n = n;
	w.con_clave = pmk != NULL;
	if (pmk)
		memcpy(w.pmk, pmk, 32);
	w.quiere = 1;
	w.fallo = W_F_NADA;
	w.t_reintento = hl_jiffies();
	hl_log(2, "802.11: se pide conectar con «%s»%s\n", w.ssid, pmk ? " (WPA2)" : " (abierta)");
}

void w80211_desconectar(void)
{
	if (w.estado >= W_ASOCIANDO) {
		o8 t[30];
		int l = pon_cab_gestion(t, 0xc0, w.ap.bssid, w.ap.bssid);
		t[l++] = 3; t[l++] = 0;		/* la estación se va */
		w.r->enviar(t, l, vel_baja(&w.ap));
	}
	suelta();
	w.quiere = 0;
	estado(W_PARADO);
}

static void conectado(void)
{
	estado(W_CONECTADO);
	w.fallo = W_F_NADA;
	w.t_oido = w.t_enviado = hl_jiffies();
	hl_log(2, "802.11: CONECTADO a «%s»\n", w.ap.ssid);
}

static void gestion(const o8 *t, int n, int senal)
{
	int sub = t[0] & 0xf0;
	int nuestro_ap = w.estado >= W_AUTENTICANDO && memcmp(t + 16, w.ap.bssid, 6) == 0;

	if (sub == 0x80 || sub == 0x50) {
		if (w.barrido)
			baliza(t, n, senal, w.barrido);
		if (nuestro_ap) {
			w.t_oido = hl_jiffies();
			w.senal = senal;
		}
		return;
	}
	if (!nuestro_ap || memcmp(t + 4, w.r->mac, 6) != 0)
		return;
	w.t_oido = hl_jiffies();
	if (sub == 0xb0 && w.estado == W_AUTENTICANDO && n >= 30) {
		int paso = t[26] | (t[27] << 8), st = t[28] | (t[29] << 8);
		if (paso != 2)
			return;
		if (st != 0) {
			hl_log(0, "802.11: código de rechazo %d\n", st);
			falla(W_F_AUT_RECHAZADA, "el punto de acceso rechaza la autenticación");
			return;
		}
		hl_log(2, "802.11: autenticado\n");
		w.intentos = 0;
		estado(W_ASOCIANDO);
		manda_asoc();
	} else if (sub == 0x10 && w.estado == W_ASOCIANDO && n >= 30) {
		int st = t[26] | (t[27] << 8);
		if (st != 0) {
			hl_log(0, "802.11: código de rechazo %d\n", st);
			falla(W_F_ASOC_RECHAZADA, "el punto de acceso rechaza la asociación");
			return;
		}
		w.aid = (t[28] | (t[29] << 8)) & 0x3fff;
		hl_log(2, "802.11: asociado (identificador %d)\n", w.aid);
		w.r->bss(w.ap.bssid, 1, w.ap.intervalo, (w.ap.capacidad & 0x0400) != 0, w.ap.velocidades, w.ap.nvel);
		if (w.ap.seguridad == W_SEG_WPA2) {
			wpa_empieza(&w.wpa, w.pmk, w.ap.bssid, w.r->mac, w.ap.rsn, w.ap.rsn_n);
			estado(W_CLAVES);
			w.t_limite = hl_jiffies() + T_CLAVES;
		} else {
			conectado();
		}
	} else if ((sub == 0xc0 || sub == 0xa0) && w.estado >= W_ASOCIANDO && n >= 26) {
		int motivo = t[24] | (t[25] << 8);
		hl_log(1, "802.11: el punto de acceso nos echa (%s, motivo %d)\n",
		       sub == 0xc0 ? "desautenticación" : "desasociación", motivo);
		if (w.estado == W_CLAVES && (motivo == 15 || motivo == 2 || motivo == 14))
			falla(W_F_CLAVE, "la contraseña no es la de esta red");
		else
			falla(W_F_EXPULSADO, "conexión cortada por el punto de acceso");
		w.reconexiones++;
	}
}

/* manda datos (cabecera LLC/SNAP incluida en «carga») al punto de acceso */
static int manda_datos(const o8 *destino, const o8 *origen, p16 tipo, const o8 *carga, int n, int cifrar)
{
	static o8 t[2400];
	int l = 24;

	if (n > 2304)
		return -1;
	memset(t, 0, 24);
	t[0] = 0x08; t[1] = 0x01;		/* datos, hacia el punto de acceso */
	memcpy(t + 4, w.ap.bssid, 6);
	memcpy(t + 10, origen, 6);
	memcpy(t + 16, destino, 6);
	memcpy(t + l, snap, 6); l += 6;
	t[l++] = tipo >> 8; t[l++] = tipo;
	memcpy(t + l, carga, n); l += n;
	if (cifrar) {
		int i;
		for (i = 0; i < 6 && ++w.wpa.pn_tx[i] == 0; i++)
			;
		l = ccmp_cifra(&w.wpa.tk, t, l, 24, w.wpa.pn_tx, 0);
	}
	w.t_enviado = hl_jiffies();
	if (w.r->enviar(t, l, tipo == 0x888e ? vel_baja(&w.ap) : 0) < 0) {
		w.tx_err++;
		return -1;
	}
	w.tx++;
	return 0;
}

int w80211_enviar_eth(const o8 *e, int n)
{
	if (w.estado != W_CONECTADO || n < 14 || n > 1514)
		return -1;
	if (memcmp(e + 6, w.r->mac, 6) != 0)
		return -1;		/* solo con nuestra dirección */
	return manda_datos(e, e + 6, (e[12] << 8) | e[13], e + 14, n - 14, w.wpa.ptk_puesta);
}

void w80211_tx_hecho(int acuse)
{
	if (!acuse)
		w.tx_err++;
}

static void datos(o8 *t, int n)
{
	static o8 e[2400];
	int cab = 24, grupo = t[4] & 1, tipo, ne, claro = 0;

	if ((t[0] & 0x70) != 0)
		return;			/* tramas vacías y demás */
	if ((t[1] & 3) != 2)
		return;			/* solo las que vienen del punto de acceso */
	if (w.estado < W_CLAVES || memcmp(t + 10, w.ap.bssid, 6) != 0)
		return;
	if (!grupo && memcmp(t + 4, w.r->mac, 6) != 0)
		return;
	if (memcmp(t + 16, w.r->mac, 6) == 0)
		return;			/* el eco de lo que mandamos a todos */
	if (t[0] & 0x80)
		cab = 26;		/* QoS */
	if (n < cab + 8)
		return;
	w.t_oido = hl_jiffies();
	if ((t[1] & 0x04) || (t[22] & 0x0f)) {
		w.rx_desc++;		/* fragmentos: no se admiten */
		return;
	}
	if (t[1] & 0x40) {
		o8 pn[6], *ult;
		struct aes *k;
		int i, id;
		if (!w.wpa.ptk_puesta) {
			w.rx_desc++;
			return;
		}
		id = t[cab + 3] >> 6;
		if (grupo) {
			if (!w.wpa.gtk_puesta || id != w.wpa.gtk_id) {
				w.rx_desc++;
				return;
			}
			k = &w.wpa.gk; ult = w.wpa.pn_rx_g;
		} else {
			k = &w.wpa.tk; ult = w.wpa.pn_rx;
		}
		n = ccmp_descifra(k, t, n, cab, pn);
		if (n < 0) {
			w.rx_desc++;
			hl_log(3, "802.11: trama con el MIC mal\n");
			return;
		}
		/* el número de paquete tiene que crecer (contra las repetidas) */
		for (i = 5; i >= 0; i--)
			if (pn[i] != ult[i])
				break;
		if (i < 0 || pn[i] < ult[i]) {
			w.rx_desc++;
			return;
		}
		memcpy(ult, pn, 6);
	} else {
		claro = 1;
		/* repetidas (en las redes abiertas no hay número de paquete) */
		if (!grupo) {
			p16 s = t[22] | (t[23] << 8);
			if ((t[1] & 0x08) && w.hay_seq && s == w.seq_rx)
				return;
			w.seq_rx = s;
			w.hay_seq = 1;
		}
	}
	if (n < cab + 8 || n - cab > 2312)
		return;
	if (memcmp(t + cab, snap, 3) == 0 && t[cab + 3] == 0 && t[cab + 4] == 0 &&
	    (t[cab + 5] == 0 || t[cab + 5] == 0xf8)) {
		tipo = (t[cab + 6] << 8) | t[cab + 7];
		ne = n - cab - 8;
		memcpy(e + 14, t + cab + 8, ne);
	} else {
		/* 802.3 con LLC: la longitud en el sitio del tipo */
		ne = n - cab;
		tipo = ne;
		memcpy(e + 14, t + cab, ne);
	}
	memcpy(e, t + 4, 6);
	memcpy(e + 6, t + 16, 6);
	e[12] = tipo >> 8; e[13] = tipo;
	/* sin cifrar en una red cifrada solo vale el acuerdo de claves (el punto
	 * de acceso repite el paso 3 en claro si no le llegó nuestro paso 4) */
	if (claro && w.ap.seguridad != W_SEG_ABIERTA && tipo != 0x888e) {
		w.rx_desc++;
		return;
	}
	if (tipo == 0x888e) {
		static o8 r[300];
		int cifrar, nr;
		if (w.ap.seguridad != W_SEG_WPA2 || w.estado < W_CLAVES)
			return;
		nr = wpa_eapol(&w.wpa, e + 14, ne, r);
		if (nr < 0) {
			/* (con el MIC bueno: lo manda el punto de acceso, pero no cuadra) */
			falla(W_F_ACUERDO, "el acuerdo de claves con el punto de acceso no sale bien");
			return;
		}
		/* las respuestas del acuerdo de 4 pasos van en claro (el punto
		 * de acceso aún no ha puesto la clave); la de grupo, cifrada */
		cifrar = w.wpa.ptk_puesta && !(nr > 6 && (r[6] & 0x08));
		if (nr > 0)
			manda_datos(w.ap.bssid, w.r->mac, 0x888e, r, nr, cifrar);
		if (w.estado == W_CLAVES && w.wpa.paso == 2)
			conectado();
		return;
	}
	if (w.estado != W_CONECTADO)
		return;
	if (ne + 14 > 1514) {
		w.rx_desc++;
		return;
	}
	w.rx++;
	w_entrega_eth(e, ne + 14);
}

void w80211_rx(const o8 *trama, int n, int senal, int canal)
{
	static o8 t[2400];

	(void)canal;
	if (n < 24 || n > (int)sizeof(t))
		return;
	if ((trama[0] & 0x0c) == 0x00) {
		gestion(trama, n, senal);
	} else if ((trama[0] & 0x0c) == 0x08) {
		memcpy(t, trama, n);
		datos(t, n);
	}
}

void w80211_tic(void)
{
	d32 ya = hl_jiffies();

	if (w.barrido) {
		if ((int)(ya - w.t_canal) < 0)
			return;
		if (w.barrido < 13) {
			barrido_canal(w.barrido + 1);
			return;
		}
		fin_barrido();
		if (w.busca_y_conecta) {
			w.busca_y_conecta = 0;
			if (w.quiere && w.estado == W_BUSCANDO)
				empieza_conexion();
		}
		return;
	}
	switch (w.estado) {
	case W_PARADO:
		if (w.quiere && (int)(ya - w.t_reintento) >= 0) {
			estado(W_BUSCANDO);
			w.busca_y_conecta = 1;
			w80211_barrer();
		}
		break;
	case W_AUTENTICANDO:
	case W_ASOCIANDO:
		if ((int)(ya - w.t_limite) < 0)
			break;
		if (++w.intentos >= INTENTOS) {
			if (w.estado == W_AUTENTICANDO)
				falla(W_F_SIN_RESPUESTA_AUT, "el punto de acceso no contesta a la autenticación");
			else
				falla(W_F_SIN_RESPUESTA_ASOC, "el punto de acceso no contesta a la asociación");
		} else if (w.estado == W_AUTENTICANDO) {
			manda_aut();
		} else {
			manda_asoc();
		}
		break;
	case W_CLAVES:
		if ((int)(ya - w.t_limite) < 0)
			break;
		if (w.wpa.fallos_mic)
			falla(W_F_ACUERDO, "el acuerdo de claves no termina y han llegado mensajes con la firma mal "
			      "(¿otro equipo haciéndose pasar por el punto de acceso?)");
		else
			falla(W_F_CLAVE, w.wpa.paso ?
			      "el acuerdo de claves no termina (¿la contraseña no es la de esta red?)" :
			      "el punto de acceso no empieza el acuerdo de claves");
		break;
	case W_CONECTADO:
		if ((int)(ya - w.t_oido) > T_PERDIDA) {
			w.reconexiones++;
			falla(W_F_PERDIDA, "se ha perdido la red (no se oye al punto de acceso)");
			w.t_reintento = ya;
		} else if ((int)(ya - w.t_enviado) > T_VIVO) {
			vacia(0);
		}
		break;
	}
}

void w80211_estado(struct w_info *i)
{
	memset(i, 0, sizeof(*i));
	i->estado = w.estado;
	i->fallo = w.fallo;
	i->nredes = w.nredes;
	i->barriendo = w.barrido != 0;
	memcpy(i->mac, w.r->mac, 6);
	i->tx = w.tx; i->rx = w.rx; i->tx_err = w.tx_err; i->rx_desc = w.rx_desc;
	i->reconexiones = w.reconexiones;
	if (w.quiere) {
		memcpy(i->ssid, w.ssid, 33);
		i->ssid_n = w.ssid_n;
	}
	if (w.estado >= W_AUTENTICANDO) {
		i->canal = w.ap.canal;
		i->senal = w.senal;
		memcpy(i->bssid, w.ap.bssid, 6);
		i->seguridad = w.ap.seguridad;
	}
}
