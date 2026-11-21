/* sb16.c - (SONIDO) la Sound Blaster 16 en SONIDO.BIN (la de los emuladores:
 * QEMU, VirtualBox, Bochs, DOSBox), como la de HSPSND (sb_detect y sb_start
 * de drivers/hspsnd.asm): el DSP en 220h, 240h, 260h o 280h (versión 4 o
 * más), 44.100 Hz, 16 bits con signo y estéreo por el canal 5 del 8237
 * (DMA de 16 bits) en modo automático sobre un anillo de 64 K por debajo
 * de los 16 MB (la llamada 196 con EDX = 1). El mezclador, al máximo (el
 * volumen, en la mezcla). La posición, del contador del canal; la
 * interrupción no hace falta (se da por leída en cada vuelta).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "sonido.h"

static u16 base;
static u8 *anillo_bajo;

static int dsp_escribe(u8 v)
{
	int i;

	for (i = 0; i < 2000; i++) {
		if (!(inb(base + 0x0C) & 0x80)) {
			outb(base + 0x0C, v);
			return 1;
		}
		espera_us(2);
	}
	return 0;
}

static int dsp_lee(void)
{
	int i;

	for (i = 0; i < 2000; i++) {
		if (inb(base + 0x0E) & 0x80)
			return inb(base + 0x0A);
		espera_us(2);
	}
	return -1;
}

static int dsp_reinicia(void)
{
	int i;

	outb(base + 0x06, 1);
	espera_us(10);
	outb(base + 0x06, 0);
	for (i = 0; i < 50; i++)
		if (dsp_lee() == 0xAA)
			return 1;
	return 0;
}

static void mezclador(u8 r, u8 v)
{
	outb(base + 4, r);
	outb(base + 5, v);
}

static int empezar(void)
{
	u32 f = anillo_fis, palabras = ANILLO / 2;
	u32 b = cli_();

	outb(0xD4, 0x04 | 1);			/* el canal 5, tapado */
	outb(0xD8, 0);				/* (el biestable) */
	outb(0xD6, 0x58 | 1);			/* automático, de la memoria al aparato, canal 5 */
	outb(0xC4, (f >> 1) & 0xFF);
	outb(0xC4, (f >> 9) & 0xFF);
	outb(0x8B, (f >> 16) & 0xFE);
	outb(0xD8, 0);
	outb(0xC6, (palabras - 1) & 0xFF);
	outb(0xC6, ((palabras - 1) >> 8) & 0xFF);
	outb(0xD4, 1);				/* destapado */
	sti_(b);
	dsp_escribe(0x41);			/* la frecuencia de salida */
	dsp_escribe(44100 >> 8);
	dsp_escribe(44100 & 0xFF);
	dsp_escribe(0xB6);			/* 16 bits, salida, automático, con FIFO */
	dsp_escribe(0x30);			/* estéreo, con signo */
	dsp_escribe((4096 - 1) & 0xFF);		/* avisa cada 4.096 muestras (8 K) */
	dsp_escribe((4096 - 1) >> 8);
	dsp_escribe(0xD1);			/* el altavoz, encendido */
	return 1;
}

static u32 posicion(void)
{
	u32 b = cli_(), lo, hi, n;

	outb(0xD8, 0);
	lo = inb(0xC6);
	hi = inb(0xC6);
	sti_(b);
	n = (lo | hi << 8) + 1;			/* las palabras que faltan para la vuelta */
	n *= 2;
	if (n > ANILLO)
		n = ANILLO;
	return (ANILLO - n) % ANILLO;
}

static void cada_vez(void)
{
	inb(base + 0x0F);			/* (la interrupción de 16 bits, leída) */
}

static void vigilar(void)
{
}

static void callar(void)
{
	dsp_escribe(0xD5);			/* pausa de 16 bits */
	dsp_escribe(0xD3);			/* el altavoz, apagado */
	outb(0xD4, 0x04 | 1);
	dsp_reinicia();
}

int sb16_buscar(struct tarjeta *t)
{
	static const u16 bases[] = {0x220, 0x240, 0x260, 0x280};
	unsigned i;
	int alto, bajo;

	for (i = 0; i < sizeof bases / sizeof bases[0]; i++) {
		base = bases[i];
		if (inb(base + 0x0E) == 0xFF && inb(base + 0x0A) == 0xFF && inb(base + 0x0C) == 0xFF)
			continue;	/* (no hay nada) */
		if (!dsp_reinicia())
			continue;
		dsp_escribe(0xE1);
		alto = dsp_lee();
		bajo = dsp_lee();
		if (alto < 4) {
			h_apunta("Sound Blaster en %x, versión %d.%d: sin 16 bits (no se lleva)", base, alto, bajo);
			continue;
		}
		/* el anillo, por debajo de los 16 MB y sin cruzar 64 K */
		if (!(anillo_bajo = dma(ANILLO / 4096, &anillo_fis, 1))) {
			h_apunta("Sound Blaster 16: no hay memoria por debajo de los 16 MB para la DMA");
			return 0;
		}
		anillo = anillo_bajo;
		mezclador(0x30, 0xF8);			/* maestro, izquierdo y derecho */
		mezclador(0x31, 0xF8);
		mezclador(0x32, 0xF8);			/* la voz (lo que se toca) */
		mezclador(0x33, 0xF8);
		hz_salida = 44100;
		estado.tarjeta = TARJETA_SB16;
		estado.pci = 0xFFFFFFFF;
		estado.dma_isa = 5;
		estado.puertos = base;
		estado.irq = 5;
		estado.salida = SALIDA_ALTAVOZ;
		formatear(estado.nombre, sizeof estado.nombre, "Sound Blaster 16 (%x)", base);
		formatear(estado.codec, sizeof estado.codec, "DSP %d.%02d", alto, bajo);
		h_apunta("Sound Blaster 16 en %x: DSP %d.%d; la DMA, en el canal 5 (%x)", base, alto, bajo, anillo_fis);
		t->empezar = empezar;
		t->posicion = posicion;
		t->cada_vez = cada_vez;
		t->vigilar = vigilar;
		t->callar = callar;
		return 1;
	}
	return 0;
}
