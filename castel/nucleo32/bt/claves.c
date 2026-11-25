/* claves.c - (BLUETOOTH) BT.INI: lo que se recuerda de los aparatos
 * emparejados (su dirección, qué son, su nombre, su descriptor HID y su
 * clave de enlace) y si el Bluetooth está encendido.
 *
 * La clave de enlace (la que da el adaptador al emparejar) va cifrada y
 * firmada (XChaCha20-Poly1305 de Monocypher) con una llave sacada de la de
 * este equipo (BT.LLV, 16 octetos al azar hechos la primera vez), con la
 * dirección del aparato como dato asociado: una clave cambiada en el
 * archivo, o pasada a otro aparato, no se usa. Como en WIFI.INI, es una
 * ofuscación y no una protección de verdad (la llave está en el mismo
 * disco): protege de quien vea solo BT.INI (una copia, una foto), no de
 * quien tenga el disco entero; y lo dice el propio archivo. Los dos son de
 * la zona protegida (permisos.cas): solo los escribe BT.BIN y solo los lee
 * él (secretos), como WIFI.INI y WIFI.LLV.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "bt.h"
#include "monocypher.h"

int claves_texto(char *t, int cabe, const u8 llv[16], int encendido);
int claves_lee(const char *t, const u8 llv[16], int *encendido);

static void llave_de(const u8 llv[16], u8 k[32])
{
	crypto_blake2b_keyed(k, 32, llv, 16, (const u8 *)"BT.INI cifrar", 13);
}

static void hex(char *t, const u8 *d, int n)
{
	static const char h[] = "0123456789ABCDEF";
	int i;

	for (i = 0; i < n; i++)
		t[2 * i] = h[d[i] >> 4], t[2 * i + 1] = h[d[i] & 15];
	t[2 * n] = 0;
}

static int cifra(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	return -1;
}

/* n octetos de 2n cifras; da cuántas letras ha leído (o -1) */
static int de_hex(const char *t, u8 *d, int n)
{
	int i;

	for (i = 0; i < n; i++) {
		int a = cifra(t[2 * i]), b = a < 0 ? -1 : cifra(t[2 * i + 1]);
		if (b < 0)
			return -1;
		d[i] = a << 4 | b;
	}
	return 2 * n;
}

static int pon(char *t, int k, int cabe, const char *s)
{
	int n = strlen(s);

	if (k + n < cabe) {
		memcpy(t + k, s, n);
		t[k + n] = 0;
		return k + n;
	}
	return k;
}

int claves_texto(char *t, int cabe, const u8 llv[16], int encendido)
{
	int i, k = 0;
	u8 llave[32];
	char l[1400];

	llave_de(llv, llave);
	t[0] = 0;
	k = pon(t, k, cabe, "; BT.INI: el Bluetooth de Hispa-Pentium (lo escribe BT.BIN).\r\n"
		"; Los aparatos emparejados. Su clave va cifrada y firmada con la llave de\r\n"
		"; este equipo (BT.LLV, que est\xa0 al lado): protege de quien vea solo este\r\n"
		"; archivo, no de quien tenga el disco entero.\r\n");
	k = pon(t, k, cabe, encendido ? "Encendido=SI\r\n" : "Encendido=NO\r\n");
	for (i = 0; i < MAX_APARATOS; i++) {
		struct aparato *a = &aparatos[i];
		u8 claro[17], sale[17], mac[16], nonce[24];
		char h1[16], h2[100], h3[50], h4[40], h5[34];
		int n;
		if (!a->usado || !a->emparejado)
			continue;
		memcpy(claro, a->clave, 16);
		claro[16] = a->tipo_clave;
		bt_azar(nonce, 24);
		crypto_aead_lock(sale, mac, llave, nonce, a->dir, 6, claro, 17);
		crypto_wipe(claro, sizeof claro);
		hex(h1, a->dir, 6);
		n = strlen(a->nombre);
		hex(h2, (const u8 *)a->nombre, n > 47 ? 47 : n);
		hex(h3, nonce, 24);
		hex(h4, sale, 17);
		hex(h5, mac, 16);
		formatear(l, sizeof l, "; %s (%s)\r\nAparato=%s,%x,%u%u%u,%s,%s%s%s\r\n", a->nombre[0] ? a->nombre : "sin nombre",
			  bt_clase_texto(a->clase, a->aspecto), h1, a->clase, a->es_hid, a->boot, a->hid_clase, h2, h3, h5, h4);
		k = pon(t, k, cabe, l);
		if (a->desc_n) {
			char d[2 * MAX_DESC + 1];
			hex(d, a->desc, a->desc_n);
			formatear(l, sizeof l, "Descriptor=%s,", h1);
			k = pon(t, k, cabe, l);
			k = pon(t, k, cabe, d);
			k = pon(t, k, cabe, "\r\n");
		}
	}
	crypto_wipe(llave, sizeof llave);
	return k;
}

static const char *linea(const char *t, const char *clave)
{
	int n = strlen(clave);

	return !memcmp(t, clave, n) ? t + n : 0;
}

int claves_lee(const char *t, const u8 llv[16], int *encendido)
{
	u8 llave[32];
	int n_bien = 0;

	llave_de(llv, llave);
	while (*t) {
		const char *v;
		if ((v = linea(t, "Encendido="))) {
			*encendido = v[0] == 'S' || v[0] == 's';
		} else if ((v = linea(t, "Aparato="))) {
			u8 dir[6], nonce[24], mac[16], c[17], claro[17];
			u32 clase = 0;
			int k, f[3], i;
			char nombre[48];
			struct aparato *a;
			if (de_hex(v, dir, 6) < 0 || v[12] != ',')
				goto siguiente;
			v += 13;
			while ((k = cifra(*v)) >= 0)
				clase = clase << 4 | k, v++;
			if (*v++ != ',')
				goto siguiente;
			for (i = 0; i < 3; i++)
				f[i] = *v++ - '0';
			if (*v++ != ',')
				goto siguiente;
			for (i = 0; i < 47 && cifra(v[0]) >= 0 && cifra(v[1]) >= 0; i++, v += 2)
				nombre[i] = cifra(v[0]) << 4 | cifra(v[1]);
			nombre[i] = 0;
			if (*v++ != ',' || de_hex(v, nonce, 24) < 0 || de_hex(v + 48, mac, 16) < 0 || de_hex(v + 80, c, 17) < 0)
				goto siguiente;
			if (crypto_aead_unlock(claro, mac, llave, nonce, dir, 6, c, 17)) {
				bt_log("BT.INI: la clave de %s no lleva la firma de este equipo (cambiada, o de otro BT.LLV): no se usa\n", bt_dir_corta(dir));
				goto siguiente;
			}
			if (!(a = bt_aparato(dir, 1)))
				goto siguiente;
			memcpy(a->clave, claro, 16);
			a->tipo_clave = claro[16];
			crypto_wipe(claro, sizeof claro);
			a->clase = clase;
			a->es_hid = f[0] == 1, a->boot = f[1] == 1, a->hid_clase = f[2] & 3;
			memcpy(a->nombre, nombre, i + 1);
			a->sin_nombre = !i;
			a->emparejado = 1;
			n_bien++;
		} else if ((v = linea(t, "Descriptor="))) {
			u8 dir[6];
			struct aparato *a;
			int n = 0;
			if (de_hex(v, dir, 6) < 0 || v[12] != ',' || !(a = bt_aparato(dir, 0)))
				goto siguiente;
			v += 13;
			while (n < MAX_DESC && de_hex(v + 2 * n, a->desc + n, 1) > 0)
				n++;
			a->desc_n = n;
		}
siguiente:
		while (*t && *t != '\n')
			t++;
		if (*t)
			t++;
	}
	crypto_wipe(llave, sizeof llave);
	return n_bien;
}
