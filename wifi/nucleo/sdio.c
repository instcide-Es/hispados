/*
 * sdio.c - SDIO sobre una controladora SDHCI, para el Wi-Fi de Hispa-DOS
 * (la tarjeta Realtek RTL8723BS de las tabletas Bay Trail: radio_rtl.c).
 * Lo explica sdio.h. Según las normas públicas «SD Host Controller
 * Simplified Specification 3.00» y «SDIO Simplified Specification 3.00»;
 * la manera de arrancar la controladora es la de castel/nucleo32/sdhci.cas
 * (el eMMC y la microSD de las mismas tabletas, probada en el Acer One 10).
 *
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 */
#include "sdio.h"

/* los registros de la controladora */
#define R_BLOQUE	0x04	/* (16) lo que mide un bloque; (16, en 0x06) cuántos */
#define R_ARGUMENTO	0x08
#define R_MODO		0x0C	/* (16) el modo; (16, en 0x0E) la orden */
#define R_RESPUESTA	0x10
#define R_DATOS		0x20	/* el puerto de datos (PIO) */
#define R_ESTADO	0x24	/* «Present State» */
#define R_HOST		0x28	/* (8) Host Control 1 */
#define R_CORRIENTE	0x29	/* (8) Power Control */
#define R_RELOJ		0x2C	/* (16) Clock Control */
#define R_ESPERA	0x2E	/* (8) Timeout Control */
#define R_REINICIO	0x2F	/* (8) Software Reset */
#define R_AVISOS	0x30	/* (32) los normales (16) y los de error (16) */
#define R_AVISOS_VALEN	0x34	/* (32) cuáles se apuntan */
#define R_AVISOS_LLAMAN	0x38	/* (32) cuáles dan interrupción (ninguno) */
#define R_CAP		0x40
#define R_CAP2		0x44
#define R_VERSION	0xFE	/* (16) */

#define E_ORDEN		0x01	/* la línea CMD, ocupada */
#define E_DATOS		0x02	/* las DAT, ocupadas */
#define E_TARJETA	0x10000
#define E_DAT0		0x100000

#define A_ORDEN		0x0001
#define A_ACABADA	0x0002
#define A_ESCRIBIR	0x0010
#define A_LEER		0x0020
#define A_TARJETA	0x0100	/* la interrupción de la tarjeta (DAT1) */
#define A_ERROR		0x8000

#define R_NINGUNA	0x00
#define R_R1		0x1A	/* 48 bits, con CRC e índice (R1, R5, R6) */
#define R_R1B		0x1B	/* y la tarjeta ocupada después */
#define R_R4		0x02	/* 48 bits sin CRC ni índice (el OCR de CMD5) */

/* el CCCR (función 0) */
#define CCCR_REV	0x00
#define CCCR_SD		0x01
#define CCCR_IOE	0x02
#define CCCR_IOR	0x03
#define CCCR_IEN	0x04
#define CCCR_ABORT	0x06
#define CCCR_BUS	0x07
#define CCCR_CAPS	0x08
#define CCCR_CIS	0x09
#define CCCR_VEL	0x13

static d32 L(struct sdio *s, d32 r, int a) { return s->p->lee(r, a); }
static void P(struct sdio *s, d32 r, int a, d32 v) { s->p->pon(r, a, v); }

/* espera (hasta «ms») a que los bits «m» del registro de 32 bits r valgan «v» */
static int espera(struct sdio *s, d32 r, d32 m, d32 v, d32 ms)
{
	d32 t0 = s->p->ms();
	unsigned vueltas = 0;

	while ((L(s, r, 4) & m) != v) {
		if (++vueltas > 64)
			s->p->espera_us(5);
		if (s->p->ms() - t0 > ms && vueltas > 100)
			return -1;
	}
	return 0;
}

static int reinicia(struct sdio *s, d32 bits)
{
	d32 t0 = s->p->ms();

	P(s, R_REINICIO, 1, bits);
	while (L(s, R_REINICIO, 1) & bits) {
		s->p->espera_us(10);
		if (s->p->ms() - t0 > 300)
			return -1;
	}
	return 0;
}

/* el reloj de la tarjeta, a «khz» como mucho (como pon_reloj de sdhci.cas) */
static void pon_reloj(struct sdio *s, d32 khz)
{
	d32 d, r, real;

	P(s, R_RELOJ, 2, 0);
	if (s->version >= 2) {
		if (s->base_khz <= khz) {
			d = 0;
			real = s->base_khz;
		} else {
			d = (s->base_khz + 2 * khz - 1) / (2 * khz);
			if (d > 1023)
				d = 1023;
			real = s->base_khz / (2 * d);
		}
		r = ((d & 0xFF) << 8) | (((d >> 8) & 3) << 6);
	} else {
		d = 0;
		real = s->base_khz;
		while (real > khz && d < 128) {
			d = d ? d * 2 : 1;
			real = s->base_khz / (2 * d);
		}
		r = d << 8;
	}
	P(s, R_RELOJ, 2, r | 1);			/* el de dentro */
	if (espera(s, R_RELOJ, 2, 2, 20) < 0)
		hl_log(1, "sdio: el reloj de dentro no se estabiliza (%04X)\n", L(s, R_RELOJ, 2));
	P(s, R_RELOJ, 2, L(s, R_RELOJ, 2) | 4);	/* y el de la tarjeta */
	s->p->espera_us(1000);
	s->khz = real;
}

/* manda la orden «idx» con «arg»; si lleva datos, «bloques» de «largo»
 * octetos, por PIO, de o a «buf». 0 bien; -1 error (s->ultimo_error) */
static int orden(struct sdio *s, int idx, d32 arg, int resp, int bloques, int largo, int lee, void *buf)
{
	d32 k, modo = 0, ord, a, t0;
	o8 *b = buf;
	int n, i;

	k = E_ORDEN | (bloques || resp == R_R1B ? E_DATOS : 0);
	if (espera(s, R_ESTADO, k, 0, 100) < 0) {
		reinicia(s, 6);
		if (espera(s, R_ESTADO, k, 0, 50) < 0) {
			s->ultimo_error = 0xFFFF;
			s->errores++;
			return -1;
		}
	}
	P(s, R_AVISOS, 4, 0xFFFFFEFF);		/* (la de la tarjeta no se borra: es de nivel) */
	ord = (idx << 8) | resp;
	if (bloques) {
		ord |= 0x20;
		P(s, R_BLOQUE, 2, largo & 0xFFF);
		P(s, R_BLOQUE + 2, 2, bloques);
		modo = 0x02 | (lee ? 0x10 : 0) | (bloques > 1 ? 0x20 : 0);
	}
	P(s, R_ARGUMENTO, 4, arg);
	P(s, R_MODO, 4, modo | (ord << 16));
	/* que acabe la orden */
	t0 = s->p->ms();
	i = 0;
	while (!((a = L(s, R_AVISOS, 4)) & (A_ORDEN | A_ERROR))) {
		if (++i > 64)
			s->p->espera_us(2);
		if (s->p->ms() - t0 > 100 && i > 100) {
			s->ultimo_error = 0xFFFE;
			s->errores++;
			reinicia(s, 6);
			return -1;
		}
	}
	if (a & A_ERROR) {
		s->ultimo_error = a >> 16;
		s->errores++;
		P(s, R_AVISOS, 4, 0xFFFFFEFF);
		reinicia(s, 6);
		return -1;
	}
	P(s, R_AVISOS, 4, A_ORDEN);
	s->resp = L(s, R_RESPUESTA, 4);
	if (!bloques && resp != R_R1B)
		return 0;
	/* los datos, bloque a bloque por el puerto (de 4 en 4 octetos) */
	for (n = 0; n < bloques; n++) {
		k = lee ? A_LEER : A_ESCRIBIR;
		t0 = s->p->ms();
		i = 0;
		while (!((a = L(s, R_AVISOS, 4)) & (k | A_ERROR))) {
			if (++i > 64)
				s->p->espera_us(2);
			if (s->p->ms() - t0 > 200 && i > 100) {
				s->ultimo_error = 0xFFFD;
				s->errores++;
				reinicia(s, 6);
				return -1;
			}
		}
		if (a & A_ERROR)
			break;
		P(s, R_AVISOS, 4, k);
		for (i = 0; i < largo; i += 4) {
			d32 w;
			int q = largo - i < 4 ? largo - i : 4, j;
			if (lee) {
				w = L(s, R_DATOS, 4);
				for (j = 0; j < q; j++)
					b[i + j] = w >> (8 * j);
			} else {
				for (w = 0, j = 0; j < q; j++)
					w |= (d32)b[i + j] << (8 * j);
				P(s, R_DATOS, 4, w);
			}
		}
		b += largo;
	}
	/* que acabe la transferencia (en R1b, que la tarjeta deje de estar ocupada) */
	t0 = s->p->ms();
	i = 0;
	while (!((a = L(s, R_AVISOS, 4)) & (A_ACABADA | A_ERROR))) {
		if (++i > 64)
			s->p->espera_us(2);
		if (resp == R_R1B && !bloques && i > 1000 && (L(s, R_ESTADO, 4) & (E_DAT0 | E_DATOS)) == E_DAT0)
			break;
		if (s->p->ms() - t0 > 300 && i > 100) {
			s->ultimo_error = 0xFFFC;
			s->errores++;
			reinicia(s, 6);
			return -1;
		}
	}
	if (a & A_ERROR) {
		s->ultimo_error = a >> 16;
		s->errores++;
		P(s, R_AVISOS, 4, 0xFFFFFEFF);
		reinicia(s, 6);
		return -1;
	}
	P(s, R_AVISOS, 4, 0xFFFFFEFF);
	return 0;
}

/* --------------------------------------------------------- CMD52 y CMD53 */
static int cmd52(struct sdio *s, int esc, int f, d32 dir, o8 v, o8 *sal)
{
	d32 arg = ((d32)esc << 31) | ((d32)(f & 7) << 28) | ((dir & 0x1FFFF) << 9) | v;

	s->ordenes52++;
	if (orden(s, 52, arg, R_R1, 0, 0, 0, NULL) < 0)
		return -1;
	/* R5: los indicadores (CRC, orden ilegal, error, función, fuera de rango) */
	if ((s->resp >> 8) & 0xCB) {
		s->ultimo_error = 0x10000 | ((s->resp >> 8) & 0xFF);
		s->errores++;
		return -1;
	}
	if (sal)
		*sal = s->resp & 0xFF;
	return 0;
}

int sdio_lee8(struct sdio *s, int f, d32 dir, o8 *v)
{
	return cmd52(s, 0, f, dir, 0, v);
}

int sdio_pon8(struct sdio *s, int f, d32 dir, o8 v)
{
	return cmd52(s, 1, f, dir, v, NULL);
}

static int cmd53(struct sdio *s, int esc, int f, d32 dir, void *b, int n)
{
	d32 arg = ((d32)esc << 31) | ((d32)(f & 7) << 28) | (1u << 26) | ((dir & 0x1FFFF) << 9);
	int bl = s->bloque1 ? s->bloque1 : 512;

	if (n <= 0)
		return 0;
	s->ordenes53++;
	if (n <= bl)		/* de octeto en octeto: la cuenta, 0 = 512 */
		return orden(s, 53, arg | (n & 0x1FF), R_R1, 1, n, !esc, b);
	n = (n + bl - 1) / bl;
	if (n > 511)
		return -1;
	return orden(s, 53, arg | (1u << 27) | n, R_R1, n, bl, !esc, b);
}

int sdio_lee(struct sdio *s, int f, d32 dir, void *b, int n)
{
	return cmd53(s, 0, f, dir, b, n);
}

int sdio_pon(struct sdio *s, int f, d32 dir, const void *b, int n)
{
	return cmd53(s, 1, f, dir, (void *)b, n);
}

int sdio_avisa(struct sdio *s)
{
	return (L(s, R_AVISOS, 4) & A_TARJETA) != 0;
}

/* --------------------------------------------------------------- el CIS */
static void lee_cis(struct sdio *s, d32 cis, int func)
{
	int t, i;
	o8 c, l, d[8];

	for (t = 0; t < 40 && cis && cis < 0x20000; t++) {
		if (sdio_lee8(s, 0, cis, &c) < 0 || c == 0xFF)
			return;
		if (c == 0x00) {		/* CISTPL_NULL */
			cis++;
			continue;
		}
		if (sdio_lee8(s, 0, cis + 1, &l) < 0 || l == 0xFF)
			return;
		for (i = 0; i < 8 && i < l; i++)
			if (sdio_lee8(s, 0, cis + 2 + i, &d[i]) < 0)
				return;
		if (c == 0x20 && l >= 4 && func == 0) {		/* CISTPL_MANFID */
			s->manf = d[0] | (d[1] << 8);
			s->carta = d[2] | (d[3] << 8);
		} else if (c == 0x22 && l >= 4 && func == 0 && d[0] == 0) {	/* CISTPL_FUNCE de la función 0 */
			s->bloque0_max = d[1] | (d[2] << 8);
		}
		cis += 2 + l;
	}
}

/* --------------------------------------------------------- la puesta en marcha */
int sdio_arranca(struct sdio *s, const struct sdio_plat *p)
{
	d32 e, volt, ocr, t0, cis;
	o8 v;
	int i, forzada = 0;

	memset(s, 0, sizeof *s);
	s->p = p;
	s->version = L(s, R_VERSION, 2) & 0xFF;
	s->cap = L(s, R_CAP, 4);
	if (s->version >= 2)
		s->cap2 = L(s, R_CAP2, 4);
	if (s->cap == 0 || s->cap == 0xFFFFFFFF || s->version > 5) {
		hl_log(0, "sdio: aquí no hay una controladora SDHCI (versión %04X, CAP %08X)\n", L(s, R_VERSION, 2), s->cap);
		return -1;
	}
	s->base_khz = (s->version >= 2 ? (s->cap >> 8) & 0xFF : (s->cap >> 8) & 0x3F) * 1000;
	if (!s->base_khz)
		s->base_khz = 100000;
	hl_log(2, "sdio: controladora SDHCI %s, CAP %08X %08X, reloj de %u MHz; estado %08X\n",
	       s->version == 0 ? "1.00" : s->version == 1 ? "2.00" : s->version == 2 ? "3.00" : "4.x",
	       s->cap, s->cap2, s->base_khz / 1000, L(s, R_ESTADO, 4));
	if (reinicia(s, 1) < 0) {
		hl_log(0, "sdio: la controladora no acaba de reiniciarse\n");
		return -1;
	}
	P(s, R_AVISOS_VALEN, 4, 0x03FF01FF);	/* (con la de la tarjeta, que se mira) */
	P(s, R_AVISOS_LLAMAN, 4, 0);
	P(s, R_ESPERA, 1, 0x0E);
	/* la ranura del Wi-Fi no tiene patilla de «hay tarjeta» (va soldada):
	 * si la controladora dice que no hay, se le dice que sí (CD test) */
	e = L(s, R_ESTADO, 4);
	if (!(e & E_TARJETA)) {
		P(s, R_HOST, 1, L(s, R_HOST, 1) | 0xC0);
		forzada = 1;
		s->p->espera_us(20000);
	}
	/* la corriente: la tensión más alta que sepa (como sdhci.cas) */
	volt = 0x0E;
	if (!(s->cap & 0x01000000))
		volt = (s->cap & 0x02000000) ? 0x0C : (s->cap & 0x04000000) ? 0x0A : 0x0E;
	P(s, R_CORRIENTE, 1, 0);
	s->p->espera_us(10000);
	P(s, R_CORRIENTE, 1, volt | 1);
	s->p->espera_us(20000);
	pon_reloj(s, 400);
	P(s, R_HOST, 1, L(s, R_HOST, 1) & ~0x26);	/* 1 bit, sin alta velocidad */
	hl_log(3, "sdio: %s, %s V, reloj a %u kHz\n", forzada ? "patilla de la tarjeta forzada" : "la controladora ve la tarjeta",
	       volt == 0x0E ? "3,3" : volt == 0x0C ? "3,0" : "1,8", s->khz);
	/* como Linux (sdio_reset y mmc_go_idle): reiniciar la E/S y CMD0 */
	if (cmd52(s, 0, 0, CCCR_ABORT, 0, &v) < 0)
		v = 0;
	cmd52(s, 1, 0, CCCR_ABORT, v | 0x08, NULL);
	orden(s, 0, 0, R_NINGUNA, 0, 0, 0, NULL);
	s->p->espera_us(2000);
	/* CMD5: ¿es una tarjeta SDIO? (el OCR, cuántas funciones) */
	for (i = 0; i < 3; i++) {
		if (orden(s, 5, 0, R_R4, 0, 0, 0, NULL) == 0)
			break;
		s->p->espera_us(10000);
	}
	if (i == 3) {
		hl_log(0, "sdio: la tarjeta no contesta a CMD5 (error %04X): no hay nada en la ranura, o el chip del Wi-Fi está apagado\n",
		       s->ultimo_error);
		return -2;
	}
	ocr = s->resp;
	s->nfunc = (ocr >> 28) & 7;
	hl_log(2, "sdio: CMD5: OCR %08X (%d funciones de E/S%s)\n", ocr, s->nfunc, (ocr & 0x08000000) ? ", con memoria" : "");
	if (!s->nfunc) {
		hl_log(0, "sdio: la tarjeta no tiene funciones de E/S\n");
		return -3;
	}
	/* que se ponga en marcha con su tensión */
	e = ocr & 0x00FF8000;
	if (!e)
		e = ocr & 0x00FFFFFF;
	t0 = s->p->ms();
	for (;;) {
		if (orden(s, 5, e, R_R4, 0, 0, 0, NULL) == 0 && (s->resp & 0x80000000u))
			break;
		if (s->p->ms() - t0 > 1500) {
			hl_log(0, "sdio: la tarjeta no acaba de ponerse en marcha (CMD5: %08X)\n", s->resp);
			return -4;
		}
		s->p->espera_us(10000);
	}
	s->ocr = s->resp;
	/* CMD3 (su dirección) y CMD7 (elegirla) */
	if (orden(s, 3, 0, R_R1, 0, 0, 0, NULL) < 0) {
		hl_log(0, "sdio: CMD3 no contesta (%04X)\n", s->ultimo_error);
		return -5;
	}
	s->rca = s->resp >> 16;
	if (orden(s, 7, (d32)s->rca << 16, R_R1B, 0, 0, 0, NULL) < 0) {
		hl_log(0, "sdio: CMD7 no contesta (%04X)\n", s->ultimo_error);
		return -6;
	}
	/* el CCCR */
	if (sdio_lee8(s, 0, CCCR_REV, &v) < 0) {
		hl_log(0, "sdio: no se puede leer el CCCR (%05X)\n", s->ultimo_error);
		return -7;
	}
	s->cccr = v & 0x0F;
	s->sdio_rev = v >> 4;
	sdio_lee8(s, 0, CCCR_SD, &s->sd_rev);
	sdio_lee8(s, 0, CCCR_CAPS, &s->caps);
	sdio_lee8(s, 0, CCCR_VEL, &s->hs);
	for (cis = 0, i = 0; i < 3; i++) {
		sdio_lee8(s, 0, CCCR_CIS + i, &v);
		cis |= (d32)v << (8 * i);
	}
	lee_cis(s, cis, 0);
	sdio_lee8(s, 0, 0x100, &s->interfaz1);
	hl_log(2, "sdio: RCA %04X; CCCR %u, SDIO %u, SD %u; capacidades %02X; fabricante %04X, tarjeta %04X; función 1: clase %02X\n",
	       s->rca, s->cccr, s->sdio_rev, s->sd_rev & 0x0F, s->caps, s->manf, s->carta, s->interfaz1 & 0x0F);
	/* alta velocidad (50 MHz) si la tarjeta y la controladora la saben */
	if ((s->hs & 1) && (s->cap & 0x00200000)) {
		if (sdio_pon8(s, 0, CCCR_VEL, s->hs | 2) == 0) {
			P(s, R_HOST, 1, L(s, R_HOST, 1) | 0x04);
			s->alta = 1;
		}
	}
	/* 4 bits, salvo las de poca velocidad que no los saben */
	if (!(s->caps & 0x40) || (s->caps & 0x80)) {
		sdio_lee8(s, 0, CCCR_BUS, &v);
		if (sdio_pon8(s, 0, CCCR_BUS, (v & ~3) | 2) == 0) {
			P(s, R_HOST, 1, (L(s, R_HOST, 1) & ~0x20) | 0x02);
			s->ancho = 4;
		}
	}
	if (!s->ancho)
		s->ancho = 1;
	pon_reloj(s, s->alta ? 50000 : 25000);
	/* la función 1: encenderla y esperar a que esté lista */
	sdio_lee8(s, 0, CCCR_IOE, &v);
	sdio_pon8(s, 0, CCCR_IOE, v | 2);
	t0 = s->p->ms();
	for (;;) {
		if (sdio_lee8(s, 0, CCCR_IOR, &v) == 0 && (v & 2))
			break;
		if (s->p->ms() - t0 > 1000) {
			hl_log(0, "sdio: la función 1 no se pone en marcha (IOR %02X)\n", v);
			return -8;
		}
		s->p->espera_us(1000);
	}
	/* su bloque: 512 (como Linux con la Realtek) */
	s->bloque1 = 512;
	if (sdio_pon8(s, 0, 0x110, 512 & 0xFF) < 0 || sdio_pon8(s, 0, 0x111, 512 >> 8) < 0) {
		hl_log(0, "sdio: no se puede poner el bloque de la función 1\n");
		return -9;
	}
	/* sus interrupciones (la de la tarjeta se mira sin interrupción) */
	sdio_pon8(s, 0, CCCR_IEN, 0x03);
	hl_log(2, "sdio: lista: %d bits, %u kHz%s; bloque de 512 en la función 1\n", s->ancho, s->khz, s->alta ? " (alta velocidad)" : "");
	return 0;
}
