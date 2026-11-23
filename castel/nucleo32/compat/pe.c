/* pe.c - (COMPAT) mirar un PE por encima (pe.h). Todo se comprueba contra
 * lo que mide: un archivo mal hecho da -1.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "pe.h"

static u32 le16(const u8 *p) { return p[0] | (u32)p[1] << 8; }
static u32 le32(const u8 *p) { return p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }

/* de RVA a la posición en el archivo (o 0xFFFFFFFF) */
static u32 rva(const u8 *d, u32 n, u32 pe, u32 r)
{
	u32 ns = le16(d + pe + 6), op = le16(d + pe + 20), k;

	for (k = 0; k < ns; k++) {
		u32 s = pe + 24 + op + k * 40;
		u32 va, vt, tr, pr;

		if (s + 40 > n)
			return 0xFFFFFFFFu;
		vt = le32(d + s + 8);
		va = le32(d + s + 12);
		tr = le32(d + s + 16);
		pr = le32(d + s + 20);
		if (r >= va && r - va < (vt > tr ? vt : tr)) {
			if (r - va >= tr || pr + (r - va) >= n)
				return 0xFFFFFFFFu;
			return pr + (r - va);
		}
	}
	return 0xFFFFFFFFu;
}

int pe_mira(const u8 *d, u32 n, pe_info *i)
{
	u32 pe, op, ed;

	memset(i, 0, sizeof *i);
	if (n < 0x40 || d[0] != 'M' || d[1] != 'Z')
		return -1;
	pe = le32(d + 0x3C);
	if (pe > n || n - pe < 24 + 96 || memcmp(d + pe, "PE\0\0", 4))
		return -1;
	i->maquina = (u16)le16(d + pe + 4);
	if (i->maquina != 0x14C)
		return -1;
	i->dll = (le16(d + pe + 22) & 0x2000) != 0;
	op = pe + 24;
	if (le16(d + op) != 0x10B || le16(d + pe + 20) < 96 + 8)
		return 0;
	ed = le32(d + op + 96);
	if (ed) {
		u32 f = rva(d, n, pe, ed);

		if (f != 0xFFFFFFFFu && f + 40 <= n) {
			u32 nn = rva(d, n, pe, le32(d + f + 12)), j = 0;

			i->n_exporta = le32(d + f + 24);
			if (nn != 0xFFFFFFFFu)
				while (nn + j < n && d[nn + j] && j + 1 < sizeof i->nombre) {
					char c = (char)d[nn + j];

					i->nombre[j++] = (c >= 'a' && c <= 'z') ? (char)(c - 32) : c;
				}
			i->nombre[j] = 0;
		}
	}
	return 0;
}

/* el subdirectorio de recursos que casa (por número o por nombre) */
static u32 entrada_rec(const u8 *d, u32 n, u32 base, u32 dir, u32 num, const char *nombre)
{
	u32 nn, ni, k;

	if (dir + 16 > n)
		return 0xFFFFFFFFu;
	nn = le16(d + dir + 12);
	ni = le16(d + dir + 14);
	for (k = 0; k < nn + ni; k++) {
		u32 e = dir + 16 + k * 8, nm, of;

		if (e + 8 > n)
			return 0xFFFFFFFFu;
		nm = le32(d + e);
		of = le32(d + e + 4);
		if (nombre ? (nm & 0x80000000u) != 0 : !(nm & 0x80000000u)) {
			int casa = 0;

			if (!nombre)
				casa = !num || nm == num;
			else {
				u32 s = base + (nm & 0x7FFFFFFFu), l, j;

				if (s + 2 <= n) {
					l = le16(d + s);
					casa = l == strlen(nombre) && s + 2 + l * 2 <= n;
					for (j = 0; casa && j < l; j++) {
						u32 c = le16(d + s + 2 + j * 2);

						casa = (c >= 'a' && c <= 'z' ? c - 32 : c) == (u32)(u8)nombre[j];
					}
				}
			}
			if (casa)
				return of;
		}
	}
	return 0xFFFFFFFFu;
}

int pe_recurso(const u8 *d, u32 n, u32 tipo, const char *nombre, u32 *desde, u32 *tam)
{
	u32 pe, op, rd, base, e;

	if (n < 0x40 || d[0] != 'M' || d[1] != 'Z')
		return -1;
	pe = le32(d + 0x3C);
	if (pe > n || n - pe < 24 + 96 + 24 || memcmp(d + pe, "PE\0\0", 4))
		return -1;
	op = pe + 24;
	if (le16(d + op) != 0x10B)
		return -1;
	rd = le32(d + op + 96 + 2 * 8);
	if (!rd)
		return -1;
	base = rva(d, n, pe, rd);
	if (base == 0xFFFFFFFFu)
		return -1;
	e = entrada_rec(d, n, base, base, tipo, NULL);
	if (e == 0xFFFFFFFFu || !(e & 0x80000000u))
		return -1;
	e = entrada_rec(d, n, base, base + (e & 0x7FFFFFFFu), 0, nombre);
	if (e == 0xFFFFFFFFu || !(e & 0x80000000u))
		return -1;
	e = entrada_rec(d, n, base, base + (e & 0x7FFFFFFFu), 0, NULL);	/* (el primer idioma) */
	if (e == 0xFFFFFFFFu || (e & 0x80000000u) || base + e + 16 > n)
		return -1;
	*tam = le32(d + base + e + 4);
	*desde = rva(d, n, pe, le32(d + base + e));
	if (*desde == 0xFFFFFFFFu || *tam > n || *desde > n - *tam)
		return -1;
	return 0;
}
