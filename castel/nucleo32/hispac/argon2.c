/* argon2.c - (USUARIOS) ARGON2.BIN, el que resume las contraseñas de los
 * usuarios de Hispa-Pentium con Argon2id (RFC 9106; el de Monocypher 4,
 * terceros/monocypher). Lo pone en marcha el núcleo cada vez que hace falta
 * (cuentas.cas, usuario.cas «argon2»: al entrar, al poner una contraseña,
 * en el modo administrador y por SSH) desde \HISPA, con su sello: pide el
 * pedido (la llamada 209, 0: la contraseña, su sal y lo que cuesta), lo
 * resume, da los 32 octetos (209, 1) y acaba. El núcleo no le da el pedido
 * a nadie más (ni a otro ARGON2.BIN lanzado a mano: lo conoce por su número
 * de tarea), y aquí no se escribe ni se lee nada del disco.
 *
 * La contraseña llega en la página 437 (la del teclado de Hispa-Pentium) o
 * en UTF-8 (la de SSH): se resume siempre en UTF-8, para que sea la misma
 * desde la pantalla y desde otro equipo.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "hispa.h"
#include "monocypher.h"

#define SC_CUENTAS_ARGON 209
#define MAGIA_ARGON 0x4E4F4741u	/* «AGON» */

/* lo que da el núcleo (usuario.cas, Pedido_argon: la misma forma) */
struct pedido {
	u32 magia, kib, pasadas, utf8, n;
	u8 sal[16];
	u8 clave[256];
};

int h_utf8_de_437(u8 c, char *d);

/* la clave, en UTF-8 (a lo más tres octetos por letra) */
static u8 utf8[3 * 256];

/* lo que hace el trabajo (aparte, para las pruebas en Linux: argon2_prueba.c) */
int argon2_resume(const struct pedido *p, u8 *trabajo, u8 resumen[32])
{
	crypto_argon2_config cf;
	crypto_argon2_inputs en;
	u32 i, k = 0;

	if (p->magia != MAGIA_ARGON || p->n > 255 || p->kib < 8 || p->kib > 65536 || p->pasadas < 1 || p->pasadas > 10)
		return -1;
	if (p->utf8) {
		memcpy(utf8, p->clave, p->n);
		k = p->n;
	} else {
		for (i = 0; i < p->n; i++)
			k += h_utf8_de_437(p->clave[i], (char *)utf8 + k);
	}
	cf.algorithm = CRYPTO_ARGON2_ID;
	cf.nb_blocks = p->kib;
	cf.nb_passes = p->pasadas;
	cf.nb_lanes = 1;
	en.pass = utf8;
	en.pass_size = k;
	en.salt = p->sal;
	en.salt_size = 16;
	crypto_argon2(resumen, 32, trabajo, cf, en, crypto_argon2_no_extras);
	crypto_wipe(utf8, sizeof utf8);
	return 0;
}

#ifndef ARGON2_LINUX
/* (las cuentas de 64 bits que pide crypto_argon2 en un 386: sin la libgcc
 * de 32 bits, que no siempre está; división larga, bit a bit) */
u64 __udivdi3(u64 a, u64 b);
u64 __umoddi3(u64 a, u64 b);

static u64 divide64(u64 a, u64 b, u64 *resto)
{
	u64 q = 0, r = 0;
	int i;

	if (!b) {
		*resto = 0;
		return 0;
	}
	for (i = 63; i >= 0; i--) {
		r = r << 1 | (a >> i & 1);
		if (r >= b) {
			r -= b;
			q |= (u64)1 << i;
		}
	}
	*resto = r;
	return q;
}

u64 __udivdi3(u64 a, u64 b)
{
	u64 r;

	return divide64(a, b, &r);
}

u64 __umoddi3(u64 a, u64 b)
{
	u64 r;

	divide64(a, b, &r);
	return r;
}

static struct pedido p;

int principal(void)
{
	u8 r[32];
	u8 *trabajo;
	u32 k, kib;

	k = sc(SC_CUENTAS_ARGON, 0, (u32)&p, sizeof p);
	if (k != sizeof p || p.magia != MAGIA_ARGON || p.kib < 8 || p.kib > 65536) {
		crypto_wipe(&p, sizeof p);
		return 1;	/* (no lo ha puesto el núcleo, o el pedido no vale) */
	}
	kib = p.kib;
	trabajo = h_memoria(kib * 1024);
	if (!trabajo) {
		crypto_wipe(&p, sizeof p);
		return 2;
	}
	if (argon2_resume(&p, trabajo, r) < 0) {
		crypto_wipe(&p, sizeof p);
		return 3;
	}
	crypto_wipe(&p, sizeof p);
	crypto_wipe(trabajo, kib * 1024);
	sc(SC_CUENTAS_ARGON, 1, (u32)r, 0);
	crypto_wipe(r, sizeof r);
	return 0;
}
#endif
