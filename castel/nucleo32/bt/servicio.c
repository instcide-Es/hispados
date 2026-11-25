/* servicio.c - (BLUETOOTH) BT.BIN, el servicio Bluetooth de Hispa-Pentium
 * (docs/PENTIUM.md, «El Bluetooth»), y la orden BT de la Consola.
 *
 * Como servicio («/SERVICIO»: lo pone en marcha el núcleo cuando hay un
 * adaptador Bluetooth USB —de la clase E0/01/01— y cuando un programa le
 * pide algo): se apunta (la llamada 240: solo el BT.BIN de la carpeta del
 * sistema, por su sello), espera a su adaptador (la 241), lo pone en marcha
 * (hci.c, y antes intel.c con los de Intel), y en cada vuelta recoge lo que
 * llega por el USB (la 243), atiende lo que pide la ventana (la 247 y la
 * 248) y le dice al núcleo cómo está (la 245: el Administrador de
 * dispositivos). Lo que teclean y mueven los teclados y ratones va al núcleo
 * por la 244 (usbhid.cas: lo mismo que los USB).
 *
 * Lo suyo, en la carpeta del sistema (\HISPA, o la raíz de la memoria USB):
 *   BT.INI  los emparejados y si está encendido (claves.c: las claves, cifradas)
 *   BT.LLV  la llave de este equipo para ellas (16 octetos al azar)
 *   BT.LOG  lo que pasa, nuevo en cada arranque, SIN claves ni códigos y con
 *           las direcciones recortadas a sus dos últimos octetos (como
 *           WIFI.LOG); mientras arranca se escribe y se cierra en cada línea
 * Los firmwares de Intel que hagan falta, en \HISPA\FIRMWARE o \FIRMWARE de
 * cualquier unidad (con su nombre de linux-firmware).
 *
 * Como orden (BT en la Consola) y como ventana («BT /VENTANA»): ventana.c.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stdarg.h>
#include "../hispac/hispa.h"
#include "bt.h"

int claves_texto(char *t, int cabe, const u8 llv[16], int encendido);
int claves_lee(const char *t, const u8 llv[16], int *encendido);
int cliente(const char *a);		/* ventana.c */

static int quiere_encendido = 1;
static u8 llv[16];
static int hay_llv;
static int guardar, cambio_estado;
static u32 guardar_desde;
static char adaptador[64];
static int estado_bt = BTE_SIN_ADAPTADOR;

/* ========================================================== formatear */
/* (el de libc.c, con %X en mayúsculas: lo usa toda la pila; bt.h lo llama
 * bt_formatear en Hispa-Pentium) */
int bt_vformatear(char *d, int cabe, const char *p, va_list va)
{
	int k = 0;

	for (; *p; p++) {
		char t[12];
		int ancho = 0, ceros = 0, i, n;
		u32 v;
		const char *s;

		if (*p != '%') {
			if (k < cabe - 1)
				d[k] = *p;
			k++;
			continue;
		}
		p++;
		if (*p == '0')
			ceros = 1, p++;
		while (*p >= '0' && *p <= '9')
			ancho = ancho * 10 + (*p++ - '0');
		n = 0;
		switch (*p) {
		case 's':
			s = va_arg(va, const char *);
			for (i = 0; s[i]; i++)
				;
			for (n = i; n < ancho; n++, k++)
				if (k < cabe - 1)
					d[k] = ' ';
			for (i = 0; s[i]; i++, k++)
				if (k < cabe - 1)
					d[k] = s[i];
			continue;
		case 'c':
			if (k < cabe - 1)
				d[k] = (char)va_arg(va, int);
			k++;
			continue;
		case 'd':
		case 'u':
		case 'x':
		case 'X':
			v = va_arg(va, u32);
			if (*p == 'd' && (int)v < 0) {
				if (k < cabe - 1)
					d[k] = '-';
				k++;
				v = -(int)v;
			}
			do {
				u32 b = *p == 'x' || *p == 'X' ? 16 : 10;
				t[n++] = (*p == 'X' ? "0123456789ABCDEF" : "0123456789abcdef")[v % b];
				v /= b;
			} while (v);
			for (i = n; i < ancho; i++, k++)
				if (k < cabe - 1)
					d[k] = ceros ? '0' : ' ';
			while (n) {
				if (k < cabe - 1)
					d[k] = t[n - 1];
				k++, n--;
			}
			continue;
		default:
			if (k < cabe - 1)
				d[k] = *p;
			k++;
			if (!*p)
				p--;
		}
	}
	if (cabe > 0)
		d[k < cabe ? k : cabe - 1] = 0;
	return k < cabe ? k : cabe - 1;
}

int bt_formatear(char *d, int cabe, const char *p, ...)
{
	va_list va;
	int r;

	va_start(va, p);
	r = bt_vformatear(d, cabe, p, va);
	va_end(va);
	return r;
}

/* ================================================================ el reloj */
u32 bt_tics(void) { return sc(SC_TICS, 0, 0, 0); }

/* ============================================================== el USB */
int tr_envia(int clase, const u8 *d, int n)
{
	u32 r = sc(SC_BT_ENVIAR, clase, (u32)d, n);

	return ES_ERROR(r) ? -1 : (int)r;
}

int tr_recibe(u8 *d, int cabe, int *de, u32 espera)
{
	u32 r = sc(SC_BT_RECIBIR, (u32)d, cabe, espera);

	if (ES_ERROR(r))
		return -1;
	*de = r >> 16;
	return r & 0xFFFF;
}

void bt_hid(int op, int ranura, int clase, const u8 *d, int n)
{
	sc(SC_BT_HID, op | ranura << 8 | clase << 16, (u32)d, n);
}

void bt_azar(void *d, u32 n)
{
	u8 *p = d;

	while (n) {
		u32 k = n > 256 ? 256 : n;
		if (sc(SC_AZAR, (u32)p, k, 0) != k) {
			/* (sin el pozo: los tics y el contador de ciclos, mezclados) */
			u32 i, t;
			__asm__ __volatile__("rdtsc" : "=a"(t) : : "edx");
			for (i = 0; i < k; i++)
				p[i] = (t >> (8 * (i & 3))) ^ (i * 131) ^ bt_tics();
		}
		p += k, n -= k;
	}
}

void bt_guarda(void)
{
	guardar = 1;
	guardar_desde = bt_tics();
}

void bt_estado_cambia(void) { cambio_estado = 1; }

void bt_a_437(char *d, const u8 *s, int n, int cabe)
{
	int i = 0, k = 0;

	while (i < n && k < cabe - 1) {
		u32 u = s[i++];
		if (u >= 0xC0 && u < 0xE0 && i < n && (s[i] & 0xC0) == 0x80)
			u = ((u & 0x1F) << 6) | (s[i++] & 0x3F);
		else if (u >= 0xE0 && u < 0xF0 && i + 1 < n) {
			u = ((u & 0x0F) << 12) | ((s[i] & 0x3F) << 6) | (s[i + 1] & 0x3F);
			i += 2;
		} else if (u >= 0x80) {
			while (i < n && (s[i] & 0xC0) == 0x80)
				i++;
			u = '?';
		}
		d[k++] = u < 32 ? '?' : h_437_de_unicode(u);
	}
	d[k] = 0;
}

/* ============================================================ BT.LOG */
static char pend[6144];
static int pend_n, cada_linea = 1;
static u32 log_t0;

static int escribe(const char *nombre, const void *d, u32 n, int al_final)
{
	char r[48];
	u32 m, e;

	h_ruta(r, sizeof r, nombre);
	m = sc(SC_ABRIR, (u32)r, al_final ? 3 : 2, 0);
	if (ES_ERROR(m))
		return -1;
	e = n ? sc(SC_ESCRIBIR_ARCHIVO, m, (u32)d, n) : 0;
	sc(SC_CERRAR, m, 0, 0);
	return ES_ERROR(e) || e != n ? -1 : 0;
}

static void vuelca(void)
{
	if (pend_n) {
		escribe("BT.LOG", pend, pend_n, 1);
		pend_n = 0;
	}
}

void bt_log(const char *f, ...)
{
	char l[400], s[440];
	va_list va;
	u32 t = bt_tics() - log_t0;
	int n, k;

	va_start(va, f);
	n = bt_vformatear(l, sizeof l, f, va);
	va_end(va);
	while (n > 0 && (l[n - 1] == '\n' || l[n - 1] == '\r'))
		l[--n] = 0;
	k = formatear(s, sizeof s, "bt: %s\r\n", l);
	sc(SC_SERIE, (u32)s, k, 0);
	k = formatear(s, sizeof s, "%5u,%02u  %s\r\n", t / 100, t % 100, l);
	if (pend_n + k > (int)sizeof pend)
		vuelca();
	memcpy(pend + pend_n, s, k);
	pend_n += k;
	if (cada_linea)
		vuelca();
}

/* ======================================================= los firmwares */
int bt_firmware(const char *nombre, u8 *d, u32 cabe)
{
	char r[96], mirado[400];
	int n, k = 0;
	char u;

	/* \HISPA\FIRMWARE (o \FIRMWARE de la memoria USB del sistema), y luego
	 * \FIRMWARE de cada unidad (la del sistema instalado y la memoria con
	 * el firmware, a la vez) */
	mirado[0] = 0;
	h_ruta(r, sizeof r, "FIRMWARE\\");
	{
		int l = strlen(r);
		formatear(r + l, sizeof r - l, "%s", nombre);
	}
	n = h_lee_archivo(r, d, cabe);
	k = formatear(mirado, sizeof mirado, "%s", r);
	for (u = 'C'; n < 0 && u <= 'Z'; u++) {
		struct { u8 letra, clase, medio, solo; u32 total, libre, del_sistema; u8 resto[60]; } info;
		u32 i;
		int hay = 0;
		for (i = 0; i < 26 && sc(SC_INFO_UNIDAD, i, (u32)&info, 0) == 1; i++)
			if (info.letra == (u8)u)
				hay = 1;
		if (!hay)
			continue;
		formatear(r, sizeof r, "%c:\\FIRMWARE\\%s", u, nombre);
		n = h_lee_archivo(r, d, cabe);
		if (k < (int)sizeof mirado - 40)
			k += formatear(mirado + k, sizeof mirado - k, ", %c:\\FIRMWARE\\", u);
	}
	if (n < 0)
		bt_log("Firmware %s: no está (mirado en %s)\n", nombre, mirado);
	else
		bt_log("Firmware %s: %u octetos (%s)\n", nombre, n, r);
	return n;
}

/* ============================================================ BT.INI */
static void lee_llave(void)
{
	char r[48];

	h_ruta(r, sizeof r, "BT.LLV");
	if (h_lee_archivo(r, llv, 16) == 16) {
		hay_llv = 1;
		return;
	}
	bt_azar(llv, 16);
	hay_llv = 1;
	if (escribe("BT.LLV", llv, 16, 0) < 0)
		bt_log("BT.LLV: no se puede guardar la llave (¿disco de solo lectura?): las claves no se recordarán\n");
	else
		bt_log("BT.LLV: llave nueva para cifrar las claves de BT.INI\n");
}

static void lee_ini(void)
{
	static char t[20000];
	char r[48], v[16];
	int n;

	h_ruta(r, sizeof r, "BT.INI");
	n = h_lee_archivo(r, t, sizeof t - 1);
	if (n <= 0)
		return;
	t[n] = 0;
	n = claves_lee(t, llv, &quiere_encendido);
	if (h_ini(t, "Registro", v, sizeof v) && h_es_ascii_igual(v, "TODO"))
		bt_registro_todo = 1;
	bt_log("BT.INI: %u emparejados; %s%s\n", n, quiere_encendido ? "encendido" : "apagado", bt_registro_todo ? "; registro de todo" : "");
}

static void guarda_ini(void)
{
	static char t[20000];
	int n;

	guardar = 0;
	n = claves_texto(t, sizeof t, llv, quiere_encendido);
	if (bt_registro_todo && n + 20 < (int)sizeof t)
		n += formatear(t + n, sizeof t - n, "Registro=TODO\r\n");
	if (escribe("BT.INI", t, n, 0) < 0)
		bt_log("BT.INI: no se puede guardar\n");
	memset(t, 0, sizeof t);
}

/* ============================================= lo que pide la ventana */
static void dice_estado(void)
{
	char t[80];

	cambio_estado = 0;
	if (estado_bt == BTE_ENCENDIDO || estado_bt == BTE_APAGADO) {
		int i, emp = 0, con = 0;
		for (i = 0; i < MAX_APARATOS; i++) {
			if (aparatos[i].usado && aparatos[i].emparejado)
				emp++;
			if (aparatos[i].usado && aparatos[i].estado == A_LISTO && aparatos[i].es_hid)
				con++;
		}
		formatear(t, sizeof t, "%s; %u emparejados, %u conectados", estado_bt == BTE_ENCENDIDO ? "encendido" : "apagado", emp, con);
	} else if (estado_bt == BTE_FALLO)
		formatear(t, sizeof t, "no se ha podido poner en marcha (BT.LOG)");
	else
		formatear(t, sizeof t, "sin adaptador");
	sc(SC_BT_ESTADO, estado_bt, (u32)t, 0);
}

static struct bt_estado est;

static void atiende(struct bt_pedido *p)
{
	u32 r = 0;
	int n = 4;

	switch (p->op) {
	case BO_ESTADO:
		memset(&est, 0, sizeof est);
		est.estado = estado_bt;
		memcpy(est.adaptador, adaptador, sizeof est.adaptador);
		bt_lista(&est);
		if (estado_bt == BTE_FALLO && !est.aviso[0])
			formatear(est.aviso, sizeof est.aviso, "No se ha podido poner en marcha: lo dice BT.LOG");
		r = 0;
		sc(SC_BT_CONTESTAR, (u32)&est, sizeof est, 0);
		return;
	case BO_ENCENDER:
		quiere_encendido = 1;
		r = bt_enciende() < 0;
		if (!r)
			estado_bt = BTE_ENCENDIDO;
		bt_guarda();
		break;
	case BO_APAGAR:
		quiere_encendido = 0;
		bt_apaga();
		if (bt.listo)
			estado_bt = BTE_APAGADO;
		bt_guarda();
		break;
	case BO_BUSCAR:
		bt_busca();
		break;
	case BO_EMPAREJAR:
		if (!p->admin)
			r = 1;
		else
			r = bt_empareja(p->dir) < 0 ? 2 : 0;
		break;
	case BO_OLVIDAR:
		if (!p->admin)
			r = 1;
		else
			r = bt_olvida(p->dir) < 0 ? 2 : 0;
		break;
	case BO_RESPONDER:
		bt_responde(p->a);
		break;
	case BO_CANCELAR:
		bt_cancela();
		break;
	case BO_CONECTAR:
		r = bt_conecta(p->dir) < 0 ? 2 : 0;
		break;
	default:
		r = 2;
	}
	dice_estado();
	sc(SC_BT_CONTESTAR, (u32)&r, n, 0);
}

/* ============================================================ el bucle */
static void nombre_adaptador(struct bt_info *i)
{
	const char *q = "";

	if (i->fabricante == 0x8087)
		switch (i->producto) {
		case 0x07DC: q = " (Wireless 7260)"; break;
		case 0x0A2A: q = " (Wireless 7265/3165)"; break;
		case 0x0A2B: q = " (Wireless 8260/8265)"; break;
		case 0x0AAA: q = " (Wireless 9460/9560)"; break;
		case 0x0025: q = " (Wireless 9160/9260)"; break;
		case 0x0026: q = " (Wi-Fi 6 AX201)"; break;
		case 0x0029: q = " (Wi-Fi 6 AX200)"; break;
		case 0x0032: q = " (Wi-Fi 6E AX210)"; break;
		case 0x0033: q = " (Wi-Fi 6E AX211)"; break;
		}
	formatear(adaptador, sizeof adaptador, "%s %04X:%04X%s",
		  i->fabricante == 0x8087 ? "Intel" : i->fabricante == 0x0A12 ? "CSR" : i->fabricante == 0x0BDA ? "Realtek" :
		  i->fabricante == 0x13D3 ? "IMC Networks" : i->fabricante == 0x0A5C ? "Broadcom" : i->fabricante == 0x0CF3 ? "Qualcomm Atheros" : "USB",
		  i->fabricante, i->producto, q);
}

static int servicio(void)
{
	struct bt_info info;
	u32 visto = 0xFFFFFFFF, ult = bt_tics(), arrancado = bt_tics();
	static u8 pedido[320];
	u32 r;

	if (sc(SC_BT_APUNTARSE, 0, 0, 0) != 0)
		return 1;
	sc(SC_ATENDER_CORTE, 1, 0, 0);
	log_t0 = bt_tics();
	escribe("BT.LOG", "", 0, 0);
	bt_log("Hispa-Pentium: el servicio Bluetooth (BT.BIN %s), en marcha. Sin claves ni códigos en este archivo; de las direcciones, solo los 2 últimos octetos.\n", BT_VERSION);
	formatear(bt.nombre, sizeof bt.nombre, "Hispa-Pentium");
	sc(SC_AJUSTES, 3, (u32)bt.nombre, sizeof bt.nombre);
	if (!bt.nombre[0])
		formatear(bt.nombre, sizeof bt.nombre, "Hispa-Pentium");
	lee_llave();
	lee_ini();
	dice_estado();
	for (;;) {
		u32 n = sc(SC_BT_INFO, 0, (u32)&info, 0);
		if (ES_ERROR(n) || n == 0) {
			if (estado_bt != BTE_SIN_ADAPTADOR) {
				bt_log("Sin adaptador Bluetooth\n");
				estado_bt = BTE_SIN_ADAPTADOR;
				bt.listo = 0;
				adaptador[0] = 0;
				dice_estado();
			}
			visto = 0xFFFFFFFF;
		} else if (info.cambios != visto) {
			visto = info.cambios;
			nombre_adaptador(&info);
			bt_log("Adaptador: %s (aparato USB %u)\n", adaptador, info.aparato);
			cada_linea = 1;
			if (bt_arranca(info.fabricante, info.producto) < 0) {
				estado_bt = BTE_FALLO;
				bt_log("DIAGNOSTICO: el adaptador %s no se ha podido poner en marcha. Mande este archivo (BT.LOG) para arreglarlo.\n", adaptador);
			} else {
				estado_bt = BTE_APAGADO;
				if (quiere_encendido && bt_enciende() == 0) {
					estado_bt = BTE_ENCENDIDO;
					bt_reconecta_todos();
				}
				bt_log("Listo: %s\n", estado_bt == BTE_ENCENDIDO ? "encendido" : "apagado (se enciende desde la ventana del Bluetooth)");
			}
			dice_estado();
		}
		if (bt.listo)
			bt_vuelta();
		else
			h_dormir(estado_bt == BTE_SIN_ADAPTADOR ? 20 : 10);
		/* lo que pide la ventana (o la orden BT) */
		r = sc(SC_BT_SIGUIENTE, (u32)pedido, 0, 0);
		if (!ES_ERROR(r) && r >= sizeof(struct bt_pedido))
			atiende((struct bt_pedido *)pedido);
		if (guardar && bt_tics() - guardar_desde > 100)
			guarda_ini();
		if (cambio_estado)
			dice_estado();
		/* (tras el primer minuto, BT.LOG se vuelca cada segundo y no en cada línea) */
		if (cada_linea && bt_tics() - arrancado > 6000)
			cada_linea = 0;
		if (bt_tics() - ult > 100) {
			ult = bt_tics();
			vuelca();
		}
		if (sc(SC_HUBO_CORTE, 0, 0, 0) == 1)
			break;
	}
	if (guardar)
		guarda_ini();
	bt_log("BT.BIN se acaba\n");
	vuelca();
	return 0;
}

static void inicia(void)
{
	struct { u8 letra, clase, medio, solo; u32 total, libre, del_sistema; u8 resto[60]; } u;
	u32 i;

	h_copia(h_nombre_prog, "bt", sizeof h_nombre_prog);
	h_hz = sc(SC_DATO, DATO_HZ, 0, 0);
	if (!h_hz || ES_ERROR(h_hz))
		h_hz = 100;
	for (i = 0; i < 26 && sc(SC_INFO_UNIDAD, i, (u32)&u, 0) == 1; i++)
		if (u.del_sistema == 1 && u.letra)
			h_sistema = u.letra;
}

int principal(void)
{
	static char a[256];

	inicia();
	h_argumentos(a, sizeof a);
	if (h_empieza(a, "/SERVICIO"))
		return servicio();
	return cliente(a);
}
