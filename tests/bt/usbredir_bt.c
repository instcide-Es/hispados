/* usbredir_bt.c - (BLUETOOTH) un adaptador Bluetooth USB de mentira para
 * QEMU: el modelo de tests/bt/modelo.c (el controlador y sus aparatos),
 * enchufado como aparato USB por usb-redir (el protocolo de usbredir, de
 * SPICE: usbredirproto.h, LGPL-2.1+; aquí solo sus números y la forma de
 * sus cabeceras, escritas de nuevo). QEMU 5.0 quitó «-device
 * usb-bt-dongle» (y su Bluetooth entero), así que es la manera de probar
 * el HCI por USB de verdad: el núcleo lo enumera, usbbt.cas le habla por
 * el extremo 0, el de interrupción y los de granel, y BT.BIN, por encima.
 *
 *   build/usbredir_bt PUERTO [--clase generico|intel-rom|intel-cargador]
 *                     [--fw CARPETA] [--estado ARCHIVO] [--teclea TEXTO]
 *
 * Escucha en 127.0.0.1:PUERTO y QEMU se conecta con
 *   -chardev socket,id=bt,host=127.0.0.1,port=PUERTO -device usb-redir,chardev=bt,bus=...
 * Con --teclea, «la persona» empareja sola el teclado (teclea el código) y
 * el ratón, y luego teclea TEXTO y mueve el ratón; con --estado, las claves
 * de los aparatos se guardan ahí (y, si ya estaban, se despiertan solos: el
 * segundo arranque). Lo que pasa, por stderr.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "modelo.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

/* ---- los números de usbredir ---- */
enum { T_HELLO = 0, T_CONNECT, T_DISCONNECT, T_RESET, T_INTERFACE_INFO, T_EP_INFO, T_SET_CONFIG, T_GET_CONFIG, T_CONFIG_STATUS,
       T_SET_ALT, T_GET_ALT, T_ALT_STATUS, T_START_ISO, T_STOP_ISO, T_ISO_STATUS, T_START_INT, T_STOP_INT, T_INT_STATUS,
       T_ALLOC_STREAMS, T_FREE_STREAMS, T_STREAMS_STATUS, T_CANCEL, T_FILTER_REJECT, T_FILTER, T_DISCONNECT_ACK,
       T_START_BULK_RECV, T_STOP_BULK_RECV, T_BULK_RECV_STATUS,
       T_CONTROL = 100, T_BULK, T_ISO, T_INTERRUPT, T_BUFFERED_BULK };
#define CAP_CONNECT_VERSION (1 << 1)
#define CAP_EP_MAX_PACKET (1 << 4)
#define CAP_64BITS_IDS (1 << 5)
#define CAP_32BITS_BULK (1 << 6)

static int s = -1, ids64, bulk32;
static u8 entrada[1 << 20];
static int ent_n;
static int configuracion = 0, int_recibiendo;
static const char *estado;

static void pon16(u8 *p, u16 v) { p[0] = v, p[1] = v >> 8; }
static void pon32(u8 *p, u32 v) { pon16(p, v), pon16(p + 2, v >> 16); }
static u16 le16(const u8 *p) { return p[0] | p[1] << 8; }
static u32 le32(const u8 *p) { return le16(p) | (u32)le16(p + 2) << 16; }

static void manda(u32 tipo, u64 id, const void *cab, int ncab, const void *d, int n)
{
	u8 b[8192];
	int h = ids64 ? 16 : 12, k = 0;

	pon32(b, tipo);
	pon32(b + 4, ncab + n);
	pon32(b + 8, id);
	if (ids64)
		pon32(b + 12, id >> 32);
	memcpy(b + h, cab, ncab);
	if (n)
		memcpy(b + h + ncab, d, n);
	while (k < h + ncab + n) {
		int r = write(s, b + k, h + ncab + n - k);
		if (r <= 0) {
			perror("usbredir_bt: write");
			exit(1);
		}
		k += r;
	}
}

/* ---- los descriptores: un adaptador Bluetooth de USB 1.1 de lo más normal ---- */
static u8 desc_aparato[18] = { 18, 1, 0x10, 0x01, 0xE0, 0x01, 0x01, 64, 0, 0, 0, 0, 0x00, 0x01, 1, 2, 0, 1 };
static const u8 desc_config[] = {
	9, 2, 39, 0, 1, 1, 0, 0xE0, 50,
	9, 4, 0, 0, 3, 0xE0, 0x01, 0x01, 0,
	7, 5, 0x81, 3, 16, 0, 1,		/* los sucesos */
	7, 5, 0x82, 2, 64, 0, 0,		/* ACL de entrada */
	7, 5, 0x02, 2, 64, 0, 0,		/* ACL de salida */
};

static int texto_usb(u8 *d, const char *t)
{
	int n = strlen(t), i;

	d[0] = 2 + 2 * n, d[1] = 3;
	for (i = 0; i < n; i++)
		d[2 + 2 * i] = t[i], d[3 + 2 * i] = 0;
	return 2 + 2 * n;
}

static void conecta(void)
{
	u8 c[160];

	pon16(desc_aparato + 8, mod_vid());
	pon16(desc_aparato + 10, mod_pid());

	/* interface_info: una interfaz, la 0 (E0/01/01) */
	memset(c, 0, 132);
	pon32(c, 1);
	c[4] = 0;
	c[36] = 0xE0, c[68] = 0x01, c[100] = 0x01;
	manda(T_INTERFACE_INFO, 0, c, 132, 0, 0);
	/* ep_info: el 0 de control, el 0x81 de interrupción, los 0x82 y 0x02 a granel */
	{
		u8 e[32 * 3 + 32 * 2];	/* (sin los «max_streams»: no se ofrecen las «bulk streams») */
		int i;
		memset(e, 0, sizeof e);
		for (i = 0; i < 32; i++)
			e[i] = 255;
		e[0] = 0, e[16] = 0;
		e[17] = 3, e[18] = 2, e[2] = 2;
		e[32 + 17] = 1;
		pon16(e + 96 + 0 * 2, 64), pon16(e + 96 + 16 * 2, 64);
		pon16(e + 96 + 17 * 2, 16), pon16(e + 96 + 18 * 2, 64), pon16(e + 96 + 2 * 2, 64);
		manda(T_EP_INFO, 0, e, sizeof e, 0, 0);
	}
	c[0] = 1;				/* velocidad: plena (12 Mbit/s) */
	c[1] = 0xE0, c[2] = 0x01, c[3] = 0x01;
	pon16(c + 4, mod_vid());
	pon16(c + 6, mod_pid());
	pon16(c + 8, 0x0100);
	manda(T_CONNECT, 0, c, 10, 0, 0);
	fprintf(stderr, "usbredir_bt: el adaptador %04X:%04X, enchufado\n", mod_vid(), mod_pid());
}

/* las peticiones de granel de entrada que esperan datos */
#define MAX_ESPERAN 32
static struct { u64 id; int n; } esperan[MAX_ESPERAN];
static int n_esperan;
/* un trozo sacado del modelo que aún no ha podido salir */
static u8 guardado[64];
static int guardado_n, guardado_de;

static void control(u64 id, const u8 *c, const u8 *d, int n)
{
	u8 r[10], dat[256];
	int k = 0, pide = c[2], peticion = c[1], valor = le16(c + 4), largo = le16(c + 8);

	memcpy(r, c, 10);
	r[3] = 0;				/* bien */
	if (pide & 0x80) {
		if (peticion == 6) {
			int t = valor >> 8;
			if (t == 1)
				memcpy(dat, desc_aparato, k = 18);
			else if (t == 2)
				memcpy(dat, desc_config, k = sizeof desc_config);
			else if (t == 3) {
				int i = valor & 0xFF;
				if (i == 0)
					dat[0] = 4, dat[1] = 3, dat[2] = 0x09, dat[3] = 0x04, k = 4;
				else
					k = texto_usb(dat, i == 1 ? "Hispa (modelo)" : "Adaptador Bluetooth de prueba");
			} else
				r[3] = 4;		/* STALL */
		} else if (peticion == 0) {
			dat[0] = dat[1] = 0, k = 2;
		} else
			r[3] = 4;
		if (k > largo)
			k = largo;
		pon16(r + 8, r[3] ? 0 : k);
		manda(T_CONTROL, id, r, 10, dat, r[3] ? 0 : k);
		return;
	}
	if ((pide & 0x60) == 0x20 && peticion == 0) {
		/* una orden HCI */
		mod_recibe(1, d, n);
	} else if (pide == 0x02 && peticion == 1) {
		/* CLEAR_FEATURE(ENDPOINT_HALT): bien */
	} else
		fprintf(stderr, "usbredir_bt: petición de control %02X %02X sin hacer\n", pide, peticion);
	pon16(r + 8, n);
	manda(T_CONTROL, id, r, 10, 0, 0);
}

static void bulk(u64 id, const u8 *c, int nc, const u8 *d, int n)
{
	u8 r[10];
	int ep = c[0], largo = le16(c + 2) | (bulk32 && nc >= 10 ? le16(c + 8) << 16 : 0);

	if (ep & 0x80) {
		if (n_esperan < MAX_ESPERAN) {
			esperan[n_esperan].id = id;
			esperan[n_esperan].n = largo;
			n_esperan++;
		}
		return;
	}
	mod_recibe(mod_en_cargador() ? 3 : 2, d, n);
	memset(r, 0, 10);
	r[0] = ep;
	pon16(r + 2, n);
	pon16(r + 8, n >> 16);
	manda(T_BULK, id, r, bulk32 ? 10 : 8, 0, 0);
}

static void cancela(u64 id)
{
	int i;
	u8 r[10];

	for (i = 0; i < n_esperan; i++)
		if (esperan[i].id == id) {
			memset(r, 0, 10);
			r[0] = 0x82, r[1] = 1;	/* cancelada */
			manda(T_BULK, id, r, bulk32 ? 10 : 8, 0, 0);
			memmove(esperan + i, esperan + i + 1, (n_esperan - i - 1) * sizeof esperan[0]);
			n_esperan--;
			return;
		}
}

static int traza;

static void paquete(u32 tipo, u64 id, const u8 *p, int n)
{
	u8 r[4];

	if (traza)
		fprintf(stderr, "usbredir_bt: llega %u (id %llu, %d octetos)%s\n", tipo, (unsigned long long)id, n,
			tipo == T_CONTROL && n >= 10 ? (p[2] & 0x80 ? " pide" : " manda") : "");

	switch (tipo) {
	case T_HELLO:
		if (n >= 68) {
			u32 caps = le32(p + 64);
			fprintf(stderr, "usbredir_bt: QEMU dice «%.64s» (capacidades %08X)\n", p, caps);
			ids64 = (caps & CAP_64BITS_IDS) != 0;
			bulk32 = (caps & CAP_32BITS_BULK) != 0;
		}
		break;
	case T_RESET:
		break;
	case T_SET_CONFIG:
		configuracion = p[0];
		r[0] = 0, r[1] = configuracion;
		manda(T_CONFIG_STATUS, id, r, 2, 0, 0);
		break;
	case T_GET_CONFIG:
		r[0] = 0, r[1] = configuracion;
		manda(T_CONFIG_STATUS, id, r, 2, 0, 0);
		break;
	case T_SET_ALT:
		r[0] = 0, r[1] = p[0], r[2] = p[1];
		manda(T_ALT_STATUS, id, r, 3, 0, 0);
		break;
	case T_GET_ALT:
		r[0] = 0, r[1] = p[0], r[2] = 0;
		manda(T_ALT_STATUS, id, r, 3, 0, 0);
		break;
	case T_START_INT:
		int_recibiendo = 1;
		r[0] = 0, r[1] = p[0];
		manda(T_INT_STATUS, id, r, 2, 0, 0);
		break;
	case T_STOP_INT:
		int_recibiendo = 0;
		r[0] = 0, r[1] = p[0];
		manda(T_INT_STATUS, id, r, 2, 0, 0);
		break;
	case T_CANCEL:
		cancela(id);
		break;
	case T_CONTROL:
		if (n >= 10)
			control(id, p, p + 10, n - 10);
		break;
	case T_BULK: {
		int nc = bulk32 ? 10 : 8;
		if (n >= nc)
			bulk(id, p, nc, p + nc, n - nc);
		break;
	}
	case T_FILTER_REJECT: case T_FILTER: case T_DISCONNECT_ACK:
		break;
	default:
		fprintf(stderr, "usbredir_bt: un paquete que no se conoce (%u)\n", tipo);
	}
}

/* lo del modelo, a QEMU: los sucesos por el de interrupción (cuando QEMU
 * los pide), lo de granel a cada petición que espera */
static void saca(void)
{
	static u64 id_int = 1;
	int de;

	for (;;) {
		if (!guardado_n) {
			guardado_n = mod_saca(guardado, sizeof guardado, &de, 16, 64);
			guardado_de = de;
			if (!guardado_n)
				return;
		}
		if (guardado_de == 1) {
			u8 r[4];
			if (!int_recibiendo)
				return;
			r[0] = 0x81, r[1] = 0;
			pon16(r + 2, guardado_n);
			manda(T_INTERRUPT, id_int++, r, 4, guardado, guardado_n);
		} else {
			u8 r[10];
			if (!n_esperan)
				return;
			memset(r, 0, 10);
			r[0] = 0x82;
			pon16(r + 2, guardado_n);
			manda(T_BULK, esperan[0].id, r, bulk32 ? 10 : 8, guardado, guardado_n);
			memmove(esperan, esperan + 1, (n_esperan - 1) * sizeof esperan[0]);
			n_esperan--;
		}
		guardado_n = 0;
	}
}

static u32 centesimas(void)
{
	struct timeval t;

	gettimeofday(&t, 0);
	return (u32)(t.tv_sec * 100 + t.tv_usec / 10000);
}

int main(int argc, char **argv)
{
	int puerto, l, i, clase = MOD_GENERICO, una = 1, claves = 0;
	const char *fw = 0, *texto = 0;
	struct sockaddr_in a;
	u32 t0;

	if (argc < 2) {
		fprintf(stderr, "uso: usbredir_bt PUERTO [--clase generico|intel-rom|intel-cargador] [--fw CARPETA] [--estado ARCHIVO] [--teclea TEXTO]\n");
		return 2;
	}
	puerto = atoi(argv[1]);
	for (i = 2; i + 1 < argc; i += 2) {
		if (!strcmp(argv[i], "--clase"))
			clase = !strcmp(argv[i + 1], "intel-rom") ? MOD_INTEL_ROM : !strcmp(argv[i + 1], "intel-cargador") ? MOD_INTEL_CARGADOR : MOD_GENERICO;
		else if (!strcmp(argv[i], "--fw"))
			fw = argv[i + 1];
		else if (!strcmp(argv[i], "--estado"))
			estado = argv[i + 1];
		else if (!strcmp(argv[i], "--teclea"))
			texto = argv[i + 1];
	}
	traza = getenv("BT_TRAZA") != 0;
	mod_inicia(clase, fw);
	if (estado)
		mod_lee(estado);
	if (texto) {
		/* («\n» escrito así en la orden: Intro) */
		static char t[64];
		int k = 0;
		for (i = 0; texto[i] && k < 63; i++)
			if (texto[i] == '\\' && texto[i + 1] == 'n')
				t[k++] = '\n', i++;
			else
				t[k++] = texto[i];
		t[k] = 0;
		mod_auto(t);
	}
	l = socket(AF_INET, SOCK_STREAM, 0);
	setsockopt(l, SOL_SOCKET, SO_REUSEADDR, &una, sizeof una);
	memset(&a, 0, sizeof a);
	a.sin_family = AF_INET;
	a.sin_port = htons(puerto);
	a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if (bind(l, (struct sockaddr *)&a, sizeof a) < 0 || listen(l, 1) < 0) {
		perror("usbredir_bt: bind");
		return 1;
	}
	fprintf(stderr, "usbredir_bt: escuchando en 127.0.0.1:%d\n", puerto);
	s = accept(l, 0, 0);
	if (s < 0) {
		perror("usbredir_bt: accept");
		return 1;
	}
	/* nuestro saludo (con la cabecera de 32 bits: aún no se sabe lo de QEMU) */
	{
		u8 h[64 + 4];
		memset(h, 0, sizeof h);
		snprintf((char *)h, 64, "hispa usbredir_bt 0.1");
		pon32(h + 64, CAP_CONNECT_VERSION | CAP_EP_MAX_PACKET | CAP_64BITS_IDS | CAP_32BITS_BULK);
		manda(T_HELLO, 0, h, sizeof h, 0, 0);
	}
	t0 = centesimas();
	int hola = 0, enchufado = 0;
	for (;;) {
		fd_set f;
		struct timeval tv = { 0, 5000 };
		int n;

		FD_ZERO(&f);
		FD_SET(s, &f);
		n = select(s + 1, &f, 0, 0, &tv);
		if (n > 0) {
			int r = read(s, entrada + ent_n, sizeof entrada - ent_n);
			if (r <= 0) {
				fprintf(stderr, "usbredir_bt: QEMU se ha ido\n");
				break;
			}
			ent_n += r;
		}
		/* los paquetes enteros que haya */
		for (;;) {
			int h = (hola && ids64) ? 16 : 12;
			u32 tipo, largo;
			u64 id;
			if (ent_n < h)
				break;
			tipo = le32(entrada), largo = le32(entrada + 4);
			id = le32(entrada + 8);
			if (h == 16)
				id |= (u64)le32(entrada + 12) << 32;
			if (ent_n < h + (int)largo)
				break;
			paquete(tipo, id, entrada + h, largo);
			if (tipo == T_HELLO)
				hola = 1;
			memmove(entrada, entrada + h + largo, ent_n - h - largo);
			ent_n -= h + largo;
			if (hola && !enchufado) {
				conecta();
				enchufado = 1;
			}
		}
		mod_tic(centesimas() - t0);
		saca();
		if (estado && mod_cambio_claves != claves) {
			claves = mod_cambio_claves;
			mod_guarda(estado);
		}
	}
	if (estado)
		mod_guarda(estado);
	fprintf(stderr, "usbredir_bt: el modelo ha visto %d cosas mal hechas; tecleado %d, movido %d; %u órdenes HCI\n", mod_fallos, mod_tecleado, mod_movido, mod_ordenes);
	return 0;
}
