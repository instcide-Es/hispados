/* argon2_prueba.c - (USUARIOS) las pruebas de ARGON2.BIN en Linux (con
 * AddressSanitizer y UBSan: castel/pruebas.sh, la parte de los usuarios):
 *   1. el vector de la RFC 9106 (Argon2id, 5.3) con el Monocypher de terceros/;
 *   2. lo de argon2.c (argon2_resume): la misma contraseña en la página 437
 *      y en UTF-8 da el mismo resumen; otra sal u otra contraseña, otro;
 *   3. N pedidos al azar (de los que puede mandar el núcleo y de los que no:
 *      la magia mala, la clave de más de 255, la memoria fuera de 8..65536,
 *      las pasadas fuera de 1..10), cada uno con su memoria justa (ASan ve
 *      si se escribe fuera): ni un fallo, y los que no valen se rechazan.
 * Se compila con argon2.c (-DARGON2_LINUX), terceros/monocypher y la tabla
 * de la página 437 de hispa.c (h_utf8_de_437).
 * Uso: argon2_prueba [N]
 *      argon2_prueba --resume CLAVE SAL KIB PASADAS   (la línea de USUARIOS.TXT de
 *          esa contraseña, en la página 437: «argon2id$KIB$PASADAS$SAL$RESUMEN»;
 *          SAL, 32 cifras hexadecimales: para hacer discos de prueba)
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "monocypher.h"

typedef uint32_t u32;
typedef uint8_t u8;

struct pedido {
	u32 magia, kib, pasadas, utf8, n;
	u8 sal[16];
	u8 clave[256];
};
int argon2_resume(const struct pedido *p, u8 *trabajo, u8 resumen[32]);

static u32 semilla = 12345;
static u32 al_azar(void)
{
	semilla ^= semilla << 13;
	semilla ^= semilla >> 17;
	semilla ^= semilla << 5;
	return semilla;
}

static int malos;

static void mira(int bien, const char *que)
{
	if (!bien) {
		printf("argon2_prueba: MAL: %s\n", que);
		malos++;
	}
}

static int resume_linea(char **a)
{
	struct pedido p;
	u8 r[32];
	u32 i, x;
	u8 *w;

	memset(&p, 0, sizeof p);
	p.magia = 0x4E4F4741u;
	p.n = strlen(a[0]);
	if (p.n > 255 || strlen(a[1]) != 32)
		return 2;
	memcpy(p.clave, a[0], p.n);
	for (i = 0; i < 16; i++) {
		if (sscanf(a[1] + 2 * i, "%2x", &x) != 1)
			return 2;
		p.sal[i] = x;
	}
	p.kib = atoi(a[2]);
	p.pasadas = atoi(a[3]);
	w = malloc(p.kib * 1024);
	if (!w || argon2_resume(&p, w, r) < 0)
		return 2;
	printf("argon2id$%u$%u$%s$", p.kib, p.pasadas, a[1]);
	for (i = 0; i < 32; i++)
		printf("%02x", r[i]);
	printf("\n");
	free(w);
	return 0;
}

int main(int argc, char **argv)
{
	int n = argc > 1 ? atoi(argv[1]) : 2000, i, rechazados = 0;
	if (argc == 6 && !strcmp(argv[1], "--resume"))
		return resume_linea(argv + 2);
	/* 1. RFC 9106, 5.3 (Argon2id) */
	u8 clave[32], sal[16], secreto[8], ad[12], r[32];
	static const u8 rfc[32] = {
		0x0d, 0x64, 0x0d, 0xf5, 0x8d, 0x78, 0x76, 0x6c, 0x08, 0xc0, 0x37, 0xa3, 0x4a, 0x8b, 0x53, 0xc9,
		0xd0, 0x1e, 0xf0, 0x45, 0x2d, 0x75, 0xb6, 0x5e, 0xb5, 0x25, 0x20, 0xe9, 0x6b, 0x01, 0xe6, 0x59 };
	crypto_argon2_config cf = { CRYPTO_ARGON2_ID, 32, 3, 4 };
	crypto_argon2_inputs en;
	crypto_argon2_extras ex;
	void *t = malloc(32 * 1024);
	memset(clave, 1, 32);
	memset(sal, 2, 16);
	memset(secreto, 3, 8);
	memset(ad, 4, 12);
	en.pass = clave;
	en.pass_size = 32;
	en.salt = sal;
	en.salt_size = 16;
	ex.key = secreto;
	ex.key_size = 8;
	ex.ad = ad;
	ex.ad_size = 12;
	crypto_argon2(r, 32, t, cf, en, ex);
	free(t);
	mira(!memcmp(r, rfc, 32), "el vector de la RFC 9106 (Argon2id)");
	/* 2. la página 437 y UTF-8 */
	{
		struct pedido a, b;
		u8 ra[32], rb[32], rc[32];
		u8 *w = malloc(64 * 1024);
		memset(&a, 0, sizeof a);
		a.magia = 0x4E4F4741u;
		a.kib = 64;
		a.pasadas = 2;
		memset(a.sal, 7, 16);
		memcpy(a.clave, "Espa\xa4" "a", 6);		/* «España» en la 437 (ñ = 0xA4) */
		a.n = 6;
		b = a;
		b.utf8 = 1;
		memcpy(b.clave, "Espa\xc3\xb1" "a", 7);	/* y en UTF-8 */
		b.n = 7;
		mira(argon2_resume(&a, w, ra) == 0 && argon2_resume(&b, w, rb) == 0 && !memcmp(ra, rb, 32),
		     "la misma contraseña en la página 437 y en UTF-8 da el mismo resumen");
		b.sal[0] ^= 1;
		mira(argon2_resume(&b, w, rc) == 0 && memcmp(ra, rc, 32), "con otra sal, otro resumen");
		a.clave[0] = 'e';
		mira(argon2_resume(&a, w, rc) == 0 && memcmp(ra, rc, 32), "con otra contraseña, otro resumen");
		free(w);
	}
	/* 3. al azar */
	for (i = 0; i < n; i++) {
		struct pedido p;
		u8 *w;
		u32 k;
		int bueno, x;
		memset(&p, 0, sizeof p);
		for (k = 0; k < sizeof p; k++)
			((u8 *)&p)[k] = al_azar();
		p.magia = al_azar() % 8 ? 0x4E4F4741u : al_azar();
		p.kib = al_azar() % 10 ? 8 + al_azar() % 57 : al_azar();
		p.pasadas = al_azar() % 12;
		p.n = al_azar() % 10 ? al_azar() % 256 : al_azar();
		p.utf8 = al_azar() & 1;
		if (p.kib > 64 && p.kib <= 65536)
			p.kib = 64;
		bueno = p.magia == 0x4E4F4741u && p.n <= 255 && p.kib >= 8 && p.kib <= 65536 && p.pasadas >= 1 && p.pasadas <= 10;
		/* (la memoria justa, si se puede; con una de más de 64 KB, la de 64 KB:
		 * no debe llegar a usarla, se rechaza antes) */
		w = malloc((p.kib <= 64 ? p.kib : 64) * 1024 + 1);
		x = argon2_resume(&p, w, r);
		free(w);
		if (!bueno)
			rechazados++;
		mira(bueno ? x == 0 : x < 0, bueno ? "un pedido bueno no se ha resumido" : "un pedido malo se ha resumido");
		if (malos > 10)
			break;
	}
	printf("argon2_prueba: %d pedidos al azar (%d rechazados, como debe), %d fallos\n", i, rechazados, malos);
	return malos != 0;
}
