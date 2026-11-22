/* zip.c - la oficina de 32 bits: los ZIP (oficina/comun/zip.h).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "zip.h"
#include "miniz_cfg.h"

struct zip_lector {
	mz_zip_archive a;
	u32 sacado;
	char nombre[512];
};

struct zip_escritor {
	mz_zip_archive a;
	int mal;
};

static void *pide(void *u, size_t n, size_t t)
{
	(void)u;
	if (t && n > 0x7FFFFFFFu / t)
		return NULL;
	return so_pide(n * t);
}

static void suelta(void *u, void *p)
{
	(void)u;
	so_suelta(p);
}

static void *repide(void *u, void *p, size_t n, size_t t)
{
	(void)u;
	if (t && n > 0x7FFFFFFFu / t)
		return NULL;
	return so_repide(p, n * t);
}

zip_lector *zip_abre(const u8 *d, u32 n)
{
	zip_lector *z = so_pide(sizeof *z);

	if (!z)
		return NULL;
	memset(z, 0, sizeof *z);
	z->a.m_pAlloc = pide;
	z->a.m_pFree = suelta;
	z->a.m_pRealloc = repide;
	if (n < 22 || !mz_zip_reader_init_mem(&z->a, d, n, 0)) {
		so_suelta(z);
		return NULL;
	}
	if (mz_zip_reader_get_num_files(&z->a) > ZIP_MAX_ENTRADAS) {
		zip_cierra(z);
		return NULL;
	}
	return z;
}

void zip_cierra(zip_lector *z)
{
	if (!z)
		return;
	mz_zip_reader_end(&z->a);
	so_suelta(z);
}

int zip_cuantos(zip_lector *z)
{
	return (int)mz_zip_reader_get_num_files(&z->a);
}

const char *zip_nombre(zip_lector *z, int i)
{
	z->nombre[0] = 0;
	if (i < 0 || i >= zip_cuantos(z))
		return z->nombre;
	mz_zip_reader_get_filename(&z->a, (mz_uint)i, z->nombre, sizeof z->nombre);
	return z->nombre;
}

static int igual_nombre(const char *a, const char *b)
{
	while (*a == '/' || (a[0] == '.' && a[1] == '/'))
		a += *a == '/' ? 1 : 2;
	while (*b == '/' || (b[0] == '.' && b[1] == '/'))
		b += *b == '/' ? 1 : 2;
	return iguales_sin_caja(a, b);
}

int zip_busca(zip_lector *z, const char *nombre)
{
	int i, n = zip_cuantos(z);

	for (i = 0; i < n; i++)
		if (igual_nombre(zip_nombre(z, i), nombre))
			return i;
	return -1;
}

u8 *zip_saca_i(zip_lector *z, int i, u32 *n)
{
	mz_zip_archive_file_stat st;
	u8 *d;
	u32 m;

	*n = 0;
	if (i < 0 || i >= zip_cuantos(z) || !mz_zip_reader_file_stat(&z->a, (mz_uint)i, &st) || st.m_is_directory)
		return NULL;
	if (st.m_uncomp_size > ZIP_MAX_ARCHIVO || st.m_uncomp_size + z->sacado > ZIP_MAX_TOTAL)
		return NULL;
	m = (u32)st.m_uncomp_size;
	d = so_pide(m + 1);
	if (!d)
		return NULL;
	/* (si lo de dentro crece más de lo que dice, miniz falla: no se pasa del tope) */
	if (m && !mz_zip_reader_extract_to_mem(&z->a, (mz_uint)i, d, m, 0)) {
		so_suelta(d);
		return NULL;
	}
	d[m] = 0;
	z->sacado += m;
	*n = m;
	return d;
}

u8 *zip_saca(zip_lector *z, const char *nombre, u32 *n)
{
	*n = 0;
	return zip_saca_i(z, zip_busca(z, nombre), n);
}

zip_escritor *zip_nuevo(void)
{
	zip_escritor *z = so_pide(sizeof *z);

	if (!z)
		return NULL;
	memset(z, 0, sizeof *z);
	z->a.m_pAlloc = pide;
	z->a.m_pFree = suelta;
	z->a.m_pRealloc = repide;
	if (!mz_zip_writer_init_heap(&z->a, 0, 64 * 1024)) {
		so_suelta(z);
		return NULL;
	}
	return z;
}

int zip_mete(zip_escritor *z, const char *nombre, const void *d, u32 n, int guardar)
{
	if (!z || z->mal)
		return -1;
	if (!mz_zip_writer_add_mem(&z->a, nombre, d ? d : "", n, guardar ? MZ_NO_COMPRESSION : 6))
		z->mal = 1;
	return z->mal ? -1 : 0;
}

int zip_acaba(zip_escritor *z, bufer *b)
{
	void *d = NULL;
	size_t n = 0;
	int r = -1;

	if (!z)
		return -1;
	if (!z->mal && mz_zip_writer_finalize_heap_archive(&z->a, &d, &n)) {
		buf_pon(b, d, (u32)n);
		so_suelta(d);
		r = b->mal ? -1 : 0;
	}
	mz_zip_writer_end(&z->a);
	so_suelta(z);
	return r;
}

void zip_deja(zip_escritor *z)
{
	if (!z)
		return;
	mz_zip_writer_end(&z->a);
	so_suelta(z);
}
