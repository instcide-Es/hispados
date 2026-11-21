/* ac97.c - (SONIDO) AC'97 en SONIDO.BIN: las controladoras de Intel (ICH y
 * las compatibles: nForce, AMD) como en HSPSND (ac97_init de
 * drivers/hspsnd.asm), la SiS 7012 (la misma, con dos registros cambiados
 * de sitio y la cuenta en octetos) y la VIA VT8233/8235/8237 (su «SGD»,
 * otra cosa: sin probar en un equipo). Con el códec reiniciado, el
 * maestro, los auriculares y el PCM a 0 dB (el volumen, en la mezcla), EAPD
 * encendido (o, con «eapd=1» en \HISPA\SONIDO.INI, al revés: hay placas que
 * lo llevan invertido), 48 kHz.
 * El anillo de 64 K son las 32 entradas de la lista de búferes, de 2 K cada
 * una; en cada vuelta del servicio, la última válida (LVI) se pone justo
 * detrás de la que suena (CIV), para que no se pare nunca.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "sonido.h"

static u32 pci;
static u16 nam, nabm;
static int clase;			/* 0 Intel, 1 SiS, 2 VIA */
static u32 *bdl;
static u32 bdl_fis;

#define TROZO (ANILLO / 32)

/* ---- el códec ---- */
static void escribe_codec(u32 r, u16 v)
{
	if (clase == 2) {
		int i;

		for (i = 0; i < 1000 && (inl(nabm + 0x80) & (1u << 24)); i++)
			espera_us(2);
		outl(nabm + 0x80, (r & 0x7F) << 16 | v);
		for (i = 0; i < 1000 && (inl(nabm + 0x80) & (1u << 24)); i++)
			espera_us(2);
		return;
	}
	outw(nam + r, v);
}

static u16 lee_codec(u32 r)
{
	if (clase == 2) {
		u32 x = 0;
		int i;

		for (i = 0; i < 1000 && (inl(nabm + 0x80) & (1u << 24)); i++)
			espera_us(2);
		outl(nabm + 0x80, (r & 0x7F) << 16 | 1u << 23);
		for (i = 0; i < 1000; i++) {
			x = inl(nabm + 0x80);
			if (!(x & (1u << 24)) && (x & (1u << 25)))
				break;
			espera_us(2);
		}
		return x & 0xFFFF;
	}
	return inw(nam + r);
}

static const char *nombre_codec(u32 id)
{
	switch (id) {
	case 0x41445370: return "Analog Devices AD1980";
	case 0x41445372: return "Analog Devices AD1981A";
	case 0x41445374: return "Analog Devices AD1981B";
	case 0x41445375: return "Analog Devices AD1985";
	case 0x41445378: return "Analog Devices AD1986";
	case 0x83847600: return "SigmaTel STAC9700";
	case 0x83847609: return "SigmaTel STAC9721/23";
	case 0x83847652: return "SigmaTel STAC9752";
	case 0x414C4710: return "Realtek ALC200";
	case 0x414C4720: return "Realtek ALC650";
	case 0x414C4760: return "Realtek ALC655";
	case 0x414C4780: return "Realtek ALC658";
	case 0x414C4790: return "Realtek ALC850";
	case 0x56494161: return "VIA VT1612A";
	case 0x56494170: return "VIA VT1617A";
	case 0x434D4941: return "C-Media CMI9738";
	case 0x434D4961: return "C-Media CMI9739";
	case 0x43585429: return "Conexant CX20468";
	}
	return 0;
}

/* ---- la DMA ---- */
static int empezar(void)
{
	u32 i;

	if (clase == 2) {
		/* VIA: la tabla del SGD (dirección; cuenta, con FLAG y EOL en la última) */
		outb(nabm + 0x01, 0x40);		/* terminar lo que hubiera */
		espera_us(50);
		outb(nabm + 0x00, 0xFF);
		for (i = 0; i < 32; i++) {
			bdl[2 * i] = anillo_fis + i * TROZO;
			bdl[2 * i + 1] = TROZO | (i == 31 ? 0x80000000u : 0);
		}
		outl(nabm + 0x04, bdl_fis);
		outb(nabm + 0x02, 0);			/* el volumen del canal: 0 dB */
		outb(nabm + 0x03, 0);
		/* 48 kHz (0xFFFFF), 16 bits y estéreo; sin índice de parada */
		outl(nabm + 0x08, 0xFF000000u | 0x00300000u | 0xFFFFF);
		outb(nabm + 0x01, 0x80 | 0x20);		/* en marcha, volviendo a empezar solo */
		h_dormir(2);
		return (inb(nabm + 0x00) & 0x80) != 0;
	}
	outb(nabm + 0x1B, 0x02);			/* reinicio del canal de salida */
	for (i = 0; i < 100 && (inb(nabm + 0x1B) & 2); i++)
		espera_us(10);
	for (i = 0; i < 32; i++) {
		bdl[2 * i] = anillo_fis + i * TROZO;
		bdl[2 * i + 1] = clase == 1 ? TROZO : TROZO / 2;	/* (en muestras; la SiS, en octetos) */
	}
	outl(nabm + 0x10, bdl_fis);
	outb(nabm + 0x15, 31);
	outb(nabm + 0x1B, 0x01);			/* en marcha */
	h_dormir(2);
	return 1;
}

static u32 posicion(void)
{
	u32 civ, resto;

	if (clase == 2) {
		u32 c = inl(nabm + 0x0C);

		civ = (c >> 24) & 31;
		resto = c & 0xFFFFFF;
		if (resto > TROZO)
			resto = TROZO;
		return (civ * TROZO + TROZO - resto) % ANILLO;
	}
	civ = inb(nabm + 0x14) & 31;
	resto = inw(nabm + (clase == 1 ? 0x16 : 0x18));
	if (clase == 0)
		resto *= 2;
	if (resto > TROZO)
		resto = TROZO;
	return (civ * TROZO + TROZO - resto) % ANILLO;
}

static void cada_vez(void)
{
	u32 civ, sr;

	if (clase == 2) {
		if (inb(nabm + 0x00) & 3)
			outb(nabm + 0x00, 3);
		return;
	}
	civ = inb(nabm + 0x14) & 31;
	outb(nabm + 0x15, (civ + 31) & 31);
	sr = inw(nabm + (clase == 1 ? 0x18 : 0x16));
	if (sr & 0x1E)
		outw(nabm + (clase == 1 ? 0x18 : 0x16), sr & 0x1E);
	if (sr & 1) {				/* (parada: otra vez) */
		h_apunta("AC'97: el canal se había parado (CIV %u): otra vez en marcha", civ);
		outb(nabm + 0x1B, 0x01);
	}
}

static void vigilar(void)
{
}

static void callar(void)
{
	escribe_codec(0x02, 0x8000);
	escribe_codec(0x04, 0x8000);
	escribe_codec(0x18, 0x8808);
	if (clase == 2)
		outb(nabm + 0x01, 0x40);
	else
		outb(nabm + 0x1B, 0);
}

int ac97_buscar(struct tarjeta *t)
{
	u32 id, cid, i, desde = 0xFFFFFFFF;
	u16 v;
	char e[8];
	const char *nc;

	while ((pci = pci_busca(0x040100, 0xFFFF00, desde)) != 0xFFFFFFFF) {
		desde = pci;
		id = pci_lee(pci, 0, 4);
		clase = 0;
		if ((id & 0xFFFF) == 0x1039)
			clase = 1;
		if ((id & 0xFFFF) == 0x1106) {
			if ((id >> 16) != 0x3059) {
				h_apunta("AC'97 de VIA %x: de las de antes de la VT8233 (no se lleva)", id >> 16);
				continue;
			}
			clase = 2;
		}
		if (clase == 2) {
			nabm = pci_lee(pci, 0x10, 4) & 0xFFFC;
			nam = 0;
		} else {
			/* (ICH4 en adelante: los puertos, encendidos aunque haya memoria) */
			if ((id & 0xFFFF) == 0x8086)
				pci_escribe(pci, 0x41, pci_lee(pci, 0x41, 1) | 1, 1);
			nam = pci_lee(pci, 0x10, 4) & 0xFFFC;
			nabm = pci_lee(pci, 0x14, 4) & 0xFFFC;
			if (!nam)
				continue;
		}
		if (!nabm)
			continue;
		pci_escribe(pci, 0x04, pci_lee(pci, 0x04, 2) | 5, 2);
		if (!bdl && !(bdl = dma(1, &bdl_fis, 0)))
			return 0;
		if (clase == 2) {
			/* VIA: el enlace AC (40h estado, 41h control) */
			pci_escribe(pci, 0x41, 0xE0, 1);	/* encendido, sin reinicio, SYNC */
			espera_us(100);
			pci_escribe(pci, 0x41, 0x00, 1);	/* reinicio */
			espera_us(100);
			pci_escribe(pci, 0x41, 0xCC, 1);	/* encendido, PCM, frecuencia variable */
			for (i = 0; i < 100 && !(pci_lee(pci, 0x40, 1) & 1); i++)
				h_dormir(1);
		} else {
			/* reinicio en frío; el códec puede tardar medio segundo largo */
			outl(nabm + 0x2C, 0);
			h_dormir(1);
			outl(nabm + 0x2C, 2);
			for (i = 0; i < 120 && !(inl(nabm + 0x30) & 0x100); i++)
				h_dormir(1);
			if (!(inl(nabm + 0x30) & 0x100)) {
				h_apunta("AC'97 %x: el códec no está listo", id);
				continue;
			}
			if (clase == 1)
				outw(nabm + 0x4C, inw(nabm + 0x4C) | 1);	/* SiS: la salida, abierta */
		}
		escribe_codec(0x00, 0);
		h_dormir(2);
		cid = (u32)lee_codec(0x7C) << 16 | lee_codec(0x7E);
		escribe_codec(0x02, 0);			/* maestro: 0 dB */
		escribe_codec(0x04, 0);			/* auriculares */
		escribe_codec(0x18, 0x0808);		/* PCM */
		v = lee_codec(0x26);
		if (h_ini(ini_texto, "eapd", e, sizeof e) && e[0] == '1')
			v |= 0x8000;
		else
			v &= 0x7FFF;
		escribe_codec(0x26, v);
		if (lee_codec(0x28) & 1) {		/* frecuencia variable: 48 kHz */
			escribe_codec(0x2A, lee_codec(0x2A) | 1);
			escribe_codec(0x2C, 48000);
		}
		hz_salida = 48000;
		estado.tarjeta = TARJETA_AC97;
		estado.pci = pci;
		estado.puertos = clase == 2 ? nabm : nam;
		estado.irq = pci_lee(pci, 0x3C, 1);
		estado.salida = SALIDA_ALTAVOZ;
		if (id == 0x24158086)
			formatear(estado.nombre, sizeof estado.nombre, "Intel AC'97 (ICH)");
		else
			formatear(estado.nombre, sizeof estado.nombre, "%s AC'97 (%x:%x)",
				  clase == 1 ? "SiS" : clase == 2 ? "VIA" : (id & 0xFFFF) == 0x8086 ? "Intel" : "", id & 0xFFFF, id >> 16);
		nc = nombre_codec(cid);
		if (nc)
			formatear(estado.codec, sizeof estado.codec, "%s", nc);
		else
			formatear(estado.codec, sizeof estado.codec, "códec %x", cid);
		h_apunta("AC'97 %x: puertos %x y %x, códec %x", id, nam, nabm, cid);
		t->empezar = empezar;
		t->posicion = posicion;
		t->cada_vez = cada_vez;
		t->vigilar = vigilar;
		t->callar = callar;
		return 1;
	}
	return 0;
}
