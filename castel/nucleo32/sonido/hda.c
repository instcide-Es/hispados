/* hda.c - (SONIDO) Intel High Definition Audio en SONIDO.BIN, pasado de
 * HSPSND de Hispa-DOS (drivers/hspsnd.asm, docs/AUDICION.md): lo mismo que
 * hace él, en C y para el anillo 3 de Hispa-Pentium.
 *
 *  - la controladora: sus registros (BAR0) a la vista (la llamada 197), la
 *    memoria y el bus master encendidos, TCSEL a 0 en las Intel y el
 *    «snoop» de las AMD/ATI; el enlace reiniciado (CRST) y los códecs que
 *    contestan (STATESTS);
 *  - [VTD] si el firmware dejó encendida la traducción de DMA de VT-d (el
 *    MacBook Air de 2017: la controladora no podía leer nada de la
 *    memoria), se apaga, como haría un sistema al arrancar (lib/vtd.inc);
 *  - las órdenes al códec, inmediatas (como HSPSND) y, si la controladora no
 *    las tiene, por CORB/RIRB;
 *  - el códec: su grupo de audio en D0; los pines que pueden sacar sonido
 *    (altavoces internos, auriculares y línea, por la configuración que
 *    traen) y, desde cada uno, un camino hasta un DAC por mezcladores y
 *    selectores; todos los nodos del camino en D0, sus amplificadores a
 *    0 dB y sin silencio (el volumen se hace en la mezcla: así no depende
 *    del códec, como pasaba con el Realtek ALC269 del Toshiba NB250), EAPD
 *    encendido y las GPIO a 1 (lo que enciende el amplificador de los
 *    altavoces en muchos portátiles); en los Cirrus de los Mac, las GPIO de
 *    sus amplificadores y su widget de proceso (lib/cirrus.inc). Si ningún
 *    pin dice lo que es (el Realtek ALC888 del MSI MS-7468 [SINCFG]), todos
 *    los que pueden sacar sonido a la vez;
 *  - los auriculares: si su conector sabe si hay algo enchufado, cada medio
 *    segundo se mira, y con ellos puestos los altavoces se callan;
 *  - el flujo de salida 1 (48 kHz, 16 bits, estéreo) sobre el anillo de 64 K,
 *    en 16 trozos de 4 K; la posición, de LPIB.
 * En \HISPA\SONIDO.INI se puede forzar: «pines=14,1b» (en hexadecimal),
 * «gpio=03», «nogpio=1», «eapd=0» (lo que AUDICION de Hispa-DOS averigua).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "sonido.h"

static volatile u8 *R;
static u32 pci, cad;
static int por_corb, sordo;
static u8 *pag;				/* la página de la lista de búferes (BDL), CORB y RIRB */
static u32 pag_fis, rirb_lee;
static u32 sd;				/* el primer flujo de salida */
static u32 afg, w_ini, w_n, fmt;
static u32 codec_id, codec_sub;

#define R8(o) (*(volatile u8 *)(R + (o)))
#define R16(o) (*(volatile u16 *)(R + (o)))
#define R32(o) (*(volatile u32 *)(R + (o)))

/* los pines puestos, por clase */
#define MAX_PINES 6
static struct { u8 nid, clase, ctl, detecta; } pines[MAX_PINES];
static int n_pines;
static u8 dacs[4];
static int n_dacs;

/* ---- las órdenes al códec ---- */
static u32 orden_inmediata(u32 o)
{
	int i;

	for (i = 0; i < 2000 && (R16(0x68) & 1); i++)
		espera_us(1);
	if (R16(0x68) & 1)
		R16(0x68) = 2;
	R16(0x68) = 2;			/* IRV a 0 */
	R32(0x60) = o;
	R16(0x68) = 1;			/* ICB: que se mande */
	for (i = 0; i < 4000; i++) {
		if ((R16(0x68) & 3) == 2) {
			u32 r = R32(0x64);

			R16(0x68) = 2;
			return r;
		}
		espera_us(1);
	}
	R16(0x68) = 2;
	return 0xFFFFFFFF;
}

static u32 orden_corb(u32 o)
{
	u32 *corb = (u32 *)(pag + 0x400), *rirb = (u32 *)(pag + 0x800);
	u32 wp = (R16(0x48) + 1) & 0xFF;
	int i;

	corb[wp] = o;
	R16(0x48) = wp;
	for (i = 0; i < 4000; i++) {
		u32 rp = R16(0x58) & 0xFF;

		while (rirb_lee != rp) {
			rirb_lee = (rirb_lee + 1) & 0xFF;
			if (!(rirb[rirb_lee * 2 + 1] & 0x10))	/* (no las espontáneas) */
				return rirb[rirb_lee * 2];
		}
		espera_us(1);
	}
	return 0xFFFFFFFF;
}

static u32 manda(u32 nid, u32 o)
{
	u32 c = cad << 28 | (nid & 0x7F) << 20 | (o & 0xFFFFF);

	if (sordo > 8)
		return 0xFFFFFFFF;
	c = por_corb ? orden_corb(c) : orden_inmediata(c);
	if (c == 0xFFFFFFFF)
		sordo++;
	return c;
}
static u32 v12(u32 nid, u32 verbo, u32 dato) { return manda(nid, verbo << 8 | (dato & 0xFF)); }
static u32 v4(u32 nid, u32 verbo, u32 dato) { return manda(nid, verbo << 16 | (dato & 0xFFFF)); }
static u32 param(u32 nid, u32 p) { return v12(nid, 0xF00, p); }

static void pon_corb(void)
{
	int i;

	R8(0x4C) = 0;
	R8(0x5C) = 0;
	h_dormir(1);
	R32(0x40) = pag_fis + 0x400;
	R32(0x44) = 0;
	R8(0x4E) = 2;			/* 256 entradas */
	R16(0x4A) = 0x8000;
	for (i = 0; i < 1000 && !(R16(0x4A) & 0x8000); i++)
		espera_us(1);
	R16(0x4A) = 0;
	for (i = 0; i < 1000 && (R16(0x4A) & 0x8000); i++)
		espera_us(1);
	R16(0x48) = 0;
	R32(0x50) = pag_fis + 0x800;
	R32(0x54) = 0;
	R8(0x5E) = 2;
	R16(0x58) = 0x8000;
	R16(0x5A) = 0xFF;
	rirb_lee = 0;
	R8(0x4C) = 2;
	R8(0x5C) = 2;
	h_dormir(1);
	por_corb = 1;
}

/* ---- [VTD] la traducción de DMA que deja encendida un firmware ---- */
static void vtd(void)
{
	u32 a, b, c, d, mch, unidad, i;
	volatile u8 *m, *u;

	__asm__ __volatile__("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1), "c"(0));
	if (!(c & 0x20) || (pci_lee(0, 0, 2) != 0x8086))
		return;
	mch = pci_lee(0, 0x48, 4);
	if (!(mch & 1) || pci_lee(0, 0x4C, 4))
		return;
	mch &= 0xFFFF8000;
	m = registros(mch + 0x5000, 0x1000);
	if (!m)
		return;
	unidad = *(volatile u32 *)(m + 0x410);
	if (!(unidad & 1) || *(volatile u32 *)(m + 0x414))
		return;
	unidad &= 0xFFFFF000;
	u = registros(unidad, 0x1000);
	if (!u)
		return;
	if (!(*(volatile u32 *)(u + 0x1C) & 0x80000000))
		return;
	h_apunta("VT-d: el firmware dejó encendida la traducción de DMA (la unidad en %x): se apaga", unidad);
	if ((*(volatile u32 *)(u + 0x08) & 0x60) && (*(volatile u32 *)(u + 0x64) & 1)) {
		*(volatile u32 *)(u + 0x64) = 0;
		for (i = 0; i < 100 && (*(volatile u32 *)(u + 0x64) & 1); i++)
			h_dormir(1);
	}
	*(volatile u32 *)(u + 0x18) = *(volatile u32 *)(u + 0x1C) & 0x16FFFFFF;
	for (i = 0; i < 100 && (*(volatile u32 *)(u + 0x1C) & 0x80000000); i++)
		h_dormir(1);
	if (*(volatile u32 *)(u + 0x1C) & 0x80000000)
		formatear(estado.detalle, sizeof estado.detalle, "VT-d: la traducción de DMA no se ha podido apagar");
	else
		formatear(estado.detalle, sizeof estado.detalle, "VT-d: la traducción de DMA del firmware, apagada");
}

/* ---- el códec ---- */
static u32 tipo(u32 nid, u32 *caps)
{
	u32 c = param(nid, 0x09);

	if (caps)
		*caps = c;
	if (c == 0xFFFFFFFF)
		return 0xFF;
	return c >> 20 & 0xF;
}

/* la lista de conexiones del nodo (hasta 16) */
static int conexiones(u32 nid, u8 *d)
{
	u32 l = param(nid, 0x0E), n, largo, i, k = 0, ant = 0;

	if (l == 0xFFFFFFFF)
		return 0;
	n = l & 0x7F;
	largo = l & 0x80;
	for (i = 0; i < n && k < 16;) {
		u32 r = v12(nid, 0xF02, i), j, por = largo ? 2 : 4;

		for (j = 0; j < por && i < n && k < 16; j++, i++) {
			u32 e = largo ? (r >> (16 * j)) & 0xFFFF : (r >> (8 * j)) & 0xFF;
			u32 rango = largo ? e & 0x8000 : e & 0x80, v = largo ? e & 0x7FFF : e & 0x7F;

			if (rango && ant)
				while (++ant < v && k < 16)
					d[k++] = ant;
			if (k < 16)
				d[k++] = v;
			ant = v;
		}
	}
	return k;
}

static u32 caps_amp(u32 nid, u32 caps, int salida)
{
	return param(caps & 8 ? nid : afg, salida ? 0x12 : 0x0D);
}

/* los amplificadores del nodo a 0 dB y sin silencio (la entrada «ent»; -1: la salida) */
static void abre_amp(u32 nid, u32 caps, int ent)
{
	u32 a;

	if (ent < 0) {
		if (!(caps & 4))
			return;
		a = caps_amp(nid, caps, 1);
		v4(nid, 3, 0xB000 | (a & 0x7F));
	} else {
		if (!(caps & 2))
			return;
		a = caps_amp(nid, caps, 0);
		v4(nid, 3, 0x7000 | (ent & 0xF) << 8 | (a & 0x7F));
	}
}

static int es_dac(u32 nid)
{
	u32 c, t = tipo(nid, &c);

	return t == 0 && !(c & 0x200);
}

static void pon_dac(u32 nid)
{
	int i;

	for (i = 0; i < n_dacs; i++)
		if (dacs[i] == nid)
			return;
	if (n_dacs >= 4)
		return;
	dacs[n_dacs++] = nid;
	v12(nid, 0x705, 0);
	v12(nid, 0x706, 0x10);		/* el flujo 1, desde el canal 0 */
	v4(nid, 2, fmt);
	abre_amp(nid, param(nid, 0x09), -1);
}

/* un camino desde nid hasta un DAC (como find_dac de HSPSND: primero uno
 * conectado directamente, luego por mezcladores y selectores, dos niveles) */
static int camino(u32 nid, int nivel)
{
	u8 c[16];
	int n = conexiones(nid, c), i;
	u32 caps, t = tipo(nid, &caps);

	for (i = 0; i < n; i++)
		if (es_dac(c[i]))
			break;
	if (i == n) {
		if (nivel >= 2)
			return 0;
		for (i = 0; i < n; i++) {
			u32 t2 = tipo(c[i], 0);

			if ((t2 == 2 || t2 == 3) && camino(c[i], nivel + 1))
				break;
		}
		if (i == n)
			return 0;
	} else
		pon_dac(c[i]);
	/* este nodo, puesto: encendido, la entrada elegida y sus amplificadores */
	v12(nid, 0x705, 0);
	if (t != 2 && n > 1)
		v12(nid, 0x701, i);
	if (t != 4)
		abre_amp(nid, caps, i);
	abre_amp(nid, caps, -1);
	return 1;
}

/* ---- los Cirrus de los Mac (lib/cirrus.inc) ---- */
static int cirrus;			/* 1 CS4206/7, 2 CS4208 */
static u32 cirrus_pincfg(u32 nid, u32 cfg)
{
	u32 ss = codec_sub & 0xFFFF;

	if (cirrus == 2 && (ss == 0x7100 || ss == 0x7200)) {
		if (nid == 0x12)
			return 0x90100110;
		if (nid == 0x10)
			return 0x032B4020;
	}
	return cfg;
}
static void cirrus_prepara(u32 gpios)
{
	static const u16 s420x[][3] = {
		{0x11, 0x0006, 0x9999}, {0x11, 0x0017, 0xA412}, {0x11, 0x0001, 0x0009}, {0x11, 0x0017, 0x2412},
		{0x11, 0x0008, 0x0000}, {0x11, 0x0006, 0x0000}, {0x11, 0x0003, 0x146A}, {0x11, 0x0002, 0x000A},
		{0x11, 0x0004, 0x0007}};
	static const u16 s4208[][3] = {{0x24, 0x0033, 0x0001}, {0x24, 0x0034, 0x1C01}};
	u32 g = 0, ss = codec_sub & 0xFFFF;
	unsigned i;

	if ((codec_sub >> 16) != 0x106B)
		return;
	if (codec_id == 0x10134206 || codec_id == 0x10134207) {
		cirrus = 1;
		g = 0x08 | ((ss == 0x2800 || ss == 0x0900 || ss == 0x2000 || ss == 0x0600) ? 0x04 : 0x02);
		v12(0x11, 0x703, 1);
		v12(0x07, 0x705, 0);
		v12(0x08, 0x705, 0);
		for (i = 0; i < sizeof s420x / sizeof s420x[0]; i++) {
			v4(s420x[i][0], 5, s420x[i][1]);
			v4(s420x[i][0], 4, s420x[i][2]);
		}
	} else if (codec_id == 0x10134208) {
		cirrus = 2;
		g = 0x01;
		v12(0x24, 0x703, 1);
		for (i = 0; i < sizeof s4208 / sizeof s4208[0]; i++) {
			v4(s4208[i][0], 5, s4208[i][1]);
			v4(s4208[i][0], 4, s4208[i][2]);
		}
	} else
		return;
	g &= gpios;
	v12(afg, 0x716, g);
	v12(afg, 0x717, g);
	v12(afg, 0x715, g);
	h_apunta("Cirrus de Apple (%x): las GPIO de los amplificadores (%x), encendidas", codec_id, g);
}

static int de_hex(const char *t, u32 *v)
{
	int n = 0;

	*v = 0;
	while ((*t >= '0' && *t <= '9') || (*t >= 'a' && *t <= 'f') || (*t >= 'A' && *t <= 'F')) {
		*v = *v * 16 + (*t <= '9' ? *t - '0' : (*t | 0x20) - 'a' + 10);
		t++;
		n++;
	}
	return n;
}

static void pon_pin(u32 nid, int clase, u32 pcaps, u32 cfg)
{
	int i;

	if (n_pines >= MAX_PINES)
		return;
	for (i = 0; i < n_pines; i++)
		if (pines[i].nid == nid)
			return;
	if (!camino(nid, 0))
		return;
	pines[n_pines].nid = nid;
	pines[n_pines].clase = clase;
	pines[n_pines].ctl = (clase == SALIDA_AURICULARES && (pcaps & 8)) ? 0xC0 : 0x40;
	pines[n_pines].detecta = (pcaps & 4) && (cfg >> 30) == 0 && clase == SALIDA_AURICULARES;
	v12(nid, 0x707, pines[n_pines].ctl);
	if (pcaps & 0x10000) {		/* EAPD */
		u32 e = v12(nid, 0xF0C, 0) & 5;
		char t[8];

		if (!(h_ini(ini_texto, "eapd", t, sizeof t) && t[0] == '0'))
			e |= 2;
		v12(nid, 0x70C, e);
	}
	h_apunta("HD Audio: el pin %x (%s) suena, desde el DAC %x", nid,
		 clase == SALIDA_ALTAVOZ ? "altavoz" : clase == SALIDA_AURICULARES ? "auriculares" : clase == SALIDA_LINEA ? "línea" : "sin decir", dacs[n_dacs - 1]);
	n_pines++;
}

static int prepara_codec(void)
{
	u32 r, i, n, gp, clase;
	char t[40];

	codec_id = param(0, 0);
	if (codec_id == 0xFFFFFFFF || codec_id == 0)
		return 0;
	r = param(0, 0x04);
	afg = 0;
	for (i = 0, n = r & 0xFF; i < n; i++)
		if ((param((r >> 16 & 0xFF) + i, 0x05) & 0xFF) == 1)
			afg = (r >> 16 & 0xFF) + i;
	if (!afg)
		return 0;
	v12(afg, 0x705, 0);
	h_dormir(2);
	codec_sub = v12(afg, 0xF20, 0);
	r = param(afg, 0x04);
	w_ini = r >> 16 & 0xFF;
	w_n = r & 0xFF;
	gp = param(afg, 0x11) & 0xFF;
	gp = gp >= 8 ? 0xFF : (1u << gp) - 1;
	cirrus = 0;
	n_pines = n_dacs = 0;
	cirrus_prepara(gp);
	/* las GPIO: todas a 1, como HSPSND sin opciones (o lo de SONIDO.INI) */
	if (!cirrus && gp && !h_ini(ini_texto, "nogpio", t, sizeof t)) {
		u32 g = gp;

		if (h_ini(ini_texto, "gpio", t, sizeof t))
			de_hex(t, &g);
		v12(afg, 0x716, gp);
		v12(afg, 0x717, gp);
		v12(afg, 0x715, g & gp);
	}
	/* los pines de SONIDO.INI */
	if (h_ini(ini_texto, "pines", t, sizeof t)) {
		const char *p = t;
		u32 v;
		int k;

		while ((k = de_hex(p, &v)) > 0) {
			pon_pin(v, SALIDA_ALTAVOZ, param(v, 0x0C), 0);
			p += k;
			if (*p == ',')
				p++;
		}
	}
	/* por su configuración: los altavoces, los auriculares y la línea */
	for (clase = 0; clase <= 3 && n_pines < MAX_PINES; clase++) {
		if (clase == 3 && n_pines)
			break;
		for (i = 0; i < w_n; i++) {
			u32 nid = w_ini + i, caps, pc, cfg, dev, k;

			if (tipo(nid, &caps) != 4 || (caps & 0x200))
				continue;
			pc = param(nid, 0x0C);
			if (!(pc & 0x10))
				continue;
			cfg = cirrus_pincfg(nid, v12(nid, 0xF1C, 0));
			dev = cfg >> 20 & 0xF;
			k = dev == 1 ? SALIDA_ALTAVOZ : dev == 2 ? SALIDA_AURICULARES : dev == 0 ? SALIDA_LINEA : 9;
			if (clase < 3) {
				if ((cfg >> 30) == 1 || cfg == 0 || k != clase)
					continue;
			} else if (k == 9 && cfg)
				continue;	/* (todo abierto: también lo que no dice qué es) */
			pon_pin(nid, clase == 3 ? SALIDA_ALTAVOZ : k, pc, cfg);
		}
	}
	return n_pines > 0;
}

/* ---- la tarjeta ---- */
static int empezar(void)
{
	u32 *bdl = (u32 *)pag, i;

	R8(sd) = 0;
	for (i = 0; i < 1000 && (R8(sd) & 2); i++)
		espera_us(1);
	R8(sd) = 1;
	for (i = 0; i < 1000 && !(R8(sd) & 1); i++)
		espera_us(1);
	R8(sd) = 0;
	for (i = 0; i < 1000 && (R8(sd) & 1); i++)
		espera_us(1);
	for (i = 0; i < ANILLO / 4096; i++) {
		bdl[4 * i] = anillo_fis + i * 4096;
		bdl[4 * i + 1] = 0;
		bdl[4 * i + 2] = 4096;
		bdl[4 * i + 3] = 0;
	}
	R32(sd + 0x18) = pag_fis;
	R32(sd + 0x1C) = 0;
	R32(sd + 0x08) = ANILLO;
	R16(sd + 0x0C) = ANILLO / 4096 - 1;
	R16(sd + 0x12) = fmt;
	R8(sd + 3) = 0x1C;		/* (los avisos de estado, borrados) */
	R8(sd + 2) = 0x10;		/* el flujo número 1 */
	R8(sd) = 2;			/* RUN */
	h_dormir(2);
	if (!(R8(sd) & 2) || (R8(sd + 3) & 0x10)) {
		h_apunta("HD Audio: el flujo no anda (CTL %x, STS %x, LPIB %x)", R32(sd), R8(sd + 3), R32(sd + 4));
		return 0;
	}
	return 1;
}

static u32 posicion(void)
{
	return R32(sd + 0x04) % ANILLO;
}

static void cada_vez(void)
{
	if (R8(sd + 3) & 0x1C)
		R8(sd + 3) = 0x1C;
}

/* los auriculares: con algo enchufado, los altavoces se callan */
static void vigilar(void)
{
	int i, hay = -1, detecta = 0;

	for (i = 0; i < n_pines; i++)
		if (pines[i].detecta) {
			u32 pc = param(pines[i].nid, 0x0C), s;

			detecta = 1;
			if (pc & 2) {		/* (hay que pedir que mire) */
				v12(pines[i].nid, 0x709, 0);
				espera_us(100);
			}
			s = v12(pines[i].nid, 0xF09, 0);
			if (s != 0xFFFFFFFF && s != 0x7FFFFFFF)
				hay = (s >> 31) & 1;
		}
	estado.detecta = detecta;
	if (hay < 0 || (u32)hay == estado.auriculares)
		return;
	estado.auriculares = hay;
	estado.salida = hay ? SALIDA_AURICULARES : SALIDA_ALTAVOZ;
	estado_cambiado = 1;
	for (i = 0; i < n_pines; i++)
		if (pines[i].clase == SALIDA_ALTAVOZ)
			v12(pines[i].nid, 0x707, hay ? 0 : pines[i].ctl);
	h_apunta("HD Audio: auriculares %s", hay ? "enchufados (los altavoces, callados)" : "fuera");
}

static void callar(void)
{
	int i;

	for (i = 0; i < n_pines; i++)
		v12(pines[i].nid, 0x707, 0);
	R8(sd) = 0;
	v12(afg, 0x705, 3);
}

static const char *nombre_marca(u32 v)
{
	switch (v) {
	case 0x10EC: return "Realtek";
	case 0x111D: case 0x92F1: return "IDT";
	case 0x8384: return "SigmaTel";
	case 0x14F1: return "Conexant";
	case 0x1013: return "Cirrus Logic";
	case 0x11D4: return "Analog Devices";
	case 0x1AF4: return "QEMU";
	case 0x8086: return "Intel";
	case 0x1002: case 0x1022: return "AMD";
	case 0x10DE: return "NVIDIA";
	case 0x1106: return "VIA";
	}
	return "";
}

static void nombre_codec(void)
{
	u32 v = codec_id >> 16, d = codec_id & 0xFFFF;
	const char *m = nombre_marca(v);

	if (codec_id == 0x83847680)
		formatear(estado.codec, sizeof estado.codec, "SigmaTel STAC9221");
	else if (v == 0x10EC)
		formatear(estado.codec, sizeof estado.codec, "Realtek ALC%x", d);
	else if (v == 0x111D || v == 0x92F1)
		formatear(estado.codec, sizeof estado.codec, "IDT 92HD (%x:%x)", v, d);
	else if (v == 0x1013)
		formatear(estado.codec, sizeof estado.codec, "Cirrus Logic CS%x", d);
	else if (v == 0x14F1)
		formatear(estado.codec, sizeof estado.codec, "Conexant CX%x", d);
	else if (v == 0x1AF4)
		formatear(estado.codec, sizeof estado.codec, "QEMU HDA (%s)", d == 0x22 ? "dúplex" : d == 0x21 ? "salida" : d == 0x23 ? "micrófono" : "códec");
	else if (*m)
		formatear(estado.codec, sizeof estado.codec, "%s (%x:%x)", m, v, d);
	else
		formatear(estado.codec, sizeof estado.codec, "códec %x:%x", v, d);
}

int hda_buscar(struct tarjeta *t)
{
	u32 bar, id, i, gcap, st, desde = 0xFFFFFFFF;

	while ((pci = pci_busca(0x040300, 0xFFFF00, desde)) != 0xFFFFFFFF) {
		desde = pci;
		id = pci_lee(pci, 0, 4);
		bar = pci_lee(pci, 0x10, 4);
		if (bar & 1)
			continue;
		if ((bar & 6) == 4 && pci_lee(pci, 0x14, 4))
			continue;	/* (por encima de los 4 GB: no se llega) */
		pci_escribe(pci, 0x04, pci_lee(pci, 0x04, 2) | 6, 2);
		if ((id & 0xFFFF) == 0x8086)
			pci_escribe(pci, 0x44, pci_lee(pci, 0x44, 1) & 0xF8, 1);	/* TCSEL */
		if ((id & 0xFFFF) == 0x1002 || (id & 0xFFFF) == 0x1022)
			pci_escribe(pci, 0x42, (pci_lee(pci, 0x42, 1) & 0xF8) | 2, 1);	/* «snoop» */
		R = registros(bar & 0xFFFFFFF0, 0x4000);
		if (!R)
			continue;
		if (!pag && !(pag = dma(1, &pag_fis, 0)))
			return 0;
		vtd();
		/* el enlace, reiniciado */
		R8(0x4C) = 0;
		R8(0x5C) = 0;
		R32(0x08) = 0;
		for (i = 0; i < 100 && (R32(0x08) & 1); i++)
			espera_us(10);
		h_dormir(1);
		R32(0x08) = 1;
		for (i = 0; i < 100 && !(R32(0x08) & 1); i++)
			h_dormir(1);
		if (!(R32(0x08) & 1)) {
			h_apunta("HD Audio %x: el enlace no sale del reinicio", id);
			continue;
		}
		h_dormir(3);
		st = R16(0x0E);
		gcap = R16(0x00);
		sd = 0x80 + ((gcap >> 8) & 0xF) * 0x20;
		if (!((gcap >> 12) & 0xF)) {
			h_apunta("HD Audio %x: sin flujos de salida", id);
			continue;
		}
		h_apunta("HD Audio %x en %x: GCAP %x, códecs %x", id, bar & 0xFFFFFFF0, gcap, st);
		fmt = 0x0011;		/* 48 kHz, 16 bits, estéreo */
		hz_salida = 48000;
		for (cad = 0; cad < 15; cad++) {
			if (!(st & (1u << cad)))
				continue;
			por_corb = 0;
			sordo = 0;
			if (param(0, 0) == 0xFFFFFFFF) {
				h_apunta("HD Audio: el códec %u no contesta a las órdenes inmediatas: por CORB/RIRB", cad);
				pon_corb();
				sordo = 0;
			}
			if (prepara_codec())
				break;
		}
		if (cad >= 15)
			continue;
		estado.tarjeta = TARJETA_HDA;
		estado.pci = pci;
		estado.memoria = bar & 0xFFFFFFF0;
		estado.irq = pci_lee(pci, 0x3C, 1);
		estado.salida = SALIDA_ALTAVOZ;
		for (i = 0; (int)i < n_pines && pines[i].clase != SALIDA_ALTAVOZ; i++) ;
		if ((int)i == n_pines)
			estado.salida = pines[0].clase == SALIDA_AURICULARES ? SALIDA_AURICULARES : SALIDA_LINEA;
		{
			const char *m = nombre_marca(id & 0xFFFF);

			formatear(estado.nombre, sizeof estado.nombre, "%s HD Audio (%x:%x)", *m ? m : "", id & 0xFFFF, id >> 16);
			if (id == 0x26688086)
				formatear(estado.nombre, sizeof estado.nombre, "Intel HD Audio (ICH6)");
			else if (id == 0x293E8086)
				formatear(estado.nombre, sizeof estado.nombre, "Intel HD Audio (ICH9)");
			else if (id == 0x27D88086)
				formatear(estado.nombre, sizeof estado.nombre, "Intel HD Audio (ICH7/NM10)");
		}
		nombre_codec();
		t->empezar = empezar;
		t->posicion = posicion;
		t->cada_vez = cada_vez;
		t->vigilar = vigilar;
		t->callar = callar;
		return 1;
	}
	return 0;
}
