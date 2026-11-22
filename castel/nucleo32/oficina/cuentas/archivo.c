/* archivo.c - Cuentas 2: abrir y guardar por formatos (archivo.h). Cada
 * formato, en su f_*.c.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "archivo.h"
#include "calculo.h"
#include "deshacer.h"

csv_opc arch_csv = { 0, CSV_AUTO, 0, 0, 0 };

/* la paleta de Hispa Ventanas (ventanas/datos.inc) */
const u32 PALETA386[16] = { 0x1A1410, 0x3B2A20, 0x1F4E5A, 0x2C6675, 0xC2542D, 0xE07B4F, 0xC9B79C, 0x8F7F68,
			    0xFBF3E0, 0xFFFFFF, 0xE8B33A, 0x6B8E3D, 0x2F4B8F, 0x9E2B2B, 0x7FC6BF, 0xD8D2C4 };

int arch_formato_ext(const char *ruta)
{
	char e[8];

	extension(ruta, e);
	if (iguales(e, "HOJ"))
		return FMT_HOJ;
	if (iguales(e, "XLSX") || iguales(e, "XLSM") || iguales(e, "XLS"))
		return FMT_XLSX;
	if (iguales(e, "ODS"))
		return FMT_ODS;
	if (iguales(e, "CSV") || iguales(e, "TXT") || iguales(e, "TSV"))
		return FMT_CSV;
	if (iguales(e, "PDF"))
		return FMT_PDF;
	return 0;
}

int arch_formato_de(const u8 *d, u32 n)
{
	if (n >= 4 && d[0] == 'P' && d[1] == 'K' && d[2] == 3 && d[3] == 4) {
		/* un ZIP: el ODS lleva su «mimetype» delante; el XLSX, xl/workbook.xml */
		if (n > 80 && memcmp(d + 30, "mimetype", 8) == 0 && busca((const char *)d + 38, "spreadsheet"))
			return FMT_ODS;
		{
			u32 i;

			for (i = 0; i + 15 < n && i < 4u * 1024 * 1024; i++)
				if (d[i] == 'x' && memcmp(d + i, "xl/workbook.xml", 15) == 0)
					return FMT_XLSX;
			for (i = 0; i + 11 < n && i < 4u * 1024 * 1024; i++)
				if (d[i] == 'c' && memcmp(d + i, "content.xml", 11) == 0)
					return FMT_ODS;
		}
		return FMT_XLSX;
	}
	if (n >= 8 && d[0] == 0xD0 && d[1] == 0xCF && d[2] == 0x11 && d[3] == 0xE0)
		return FMT_XLS;
	if (n >= 6 && memcmp(d, "HOJA ", 5) == 0)
		return FMT_HOJ;
	if (n >= 4 && memcmp(d, "%PDF", 4) == 0)
		return FMT_PDF;
	return FMT_CSV;
}

const char *arch_extension(int f)
{
	switch (f) {
	case FMT_HOJ: return "HOJ";
	case FMT_XLSX: return "XLSX";
	case FMT_ODS: return "ODS";
	case FMT_CSV: return "CSV";
	case FMT_PDF: return "PDF";
	case FMT_XLS: return "XLS";
	}
	return "";
}

const char *arch_nombre_formato(int f)
{
	switch (f) {
	case FMT_HOJ: return "Libro de Cuentas (.HOJ)";
	case FMT_XLSX: return "Libro de Excel (.XLSX)";
	case FMT_ODS: return "Hoja de cálculo OpenDocument (.ODS)";
	case FMT_CSV: return "Texto separado por comas (.CSV)";
	case FMT_PDF: return "PDF";
	case FMT_XLS: return "Libro de Excel 97-2003 (.XLS)";
	}
	return "?";
}

libro *arch_abre(const char *ruta, int *formato, char *err, u32 cabe)
{
	const char *e = NULL;
	u32 n;
	u8 *d = so_lee_archivo(ruta, &n, ARCH_MAX, &e);
	libro *l = NULL;
	int f;

	if (formato)
		*formato = 0;
	if (!d) {
		fmt(err, cabe, "No se puede abrir %s: %s", sin_carpeta(ruta), e ? e : "no se puede leer");
		return NULL;
	}
	f = arch_formato_de(d, n);
	if (f == FMT_CSV && arch_formato_ext(ruta) == FMT_HOJ && n == 0)
		f = FMT_HOJ;
	switch (f) {
	case FMT_HOJ:
		l = hoj_lee(d, n, err, cabe);
		break;
	case FMT_XLSX:
		l = xlsx_lee(d, n, err, cabe);
		break;
	case FMT_ODS:
		l = ods_lee(d, n, err, cabe);
		break;
	case FMT_CSV:
		l = csv_lee(d, n, &arch_csv, err, cabe);
		if (l && l->n_h) {
			/* la hoja, con el nombre del archivo */
			char nom[96];
			char *p;

			copia(nom, sin_carpeta(ruta), sizeof nom);
			p = strchr(nom, '.');
			if (p)
				*p = 0;
			if (nom[0])
				copia(l->h[0]->nombre, nom, 32);
		}
		break;
	case FMT_XLS:
		copia(err, "Es un libro de Excel 97-2003 (.XLS binario): Cuentas aún no lo lee. Guárdelo en Excel o LibreOffice como .XLSX u .ODS", cabe);
		break;
	case FMT_PDF:
		copia(err, "Es un PDF: se abre con Hispa-PDF", cabe);
		break;
	}
	so_suelta(d);
	if (!l)
		return NULL;
	if (formato)
		*formato = f;
	calc_todo(l);
	l->cambiado = 0;
	if (l->activa >= l->n_h)
		l->activa = 0;
	return l;
}

/* el nombre 8+3 que dará el sistema (si no crea nombres largos) */
static void nombre_83(const char *ruta, char *d, u32 cabe)
{
	const char *n = sin_carpeta(ruta), *pt = strrchr(n, '.');
	u32 i, k = 0, pre = (u32)(n - ruta);
	char t[600];

	copia(t, ruta, pre + 1 < sizeof t ? pre + 1 : sizeof t);
	k = largo(t);
	for (i = 0; n + i < (pt ? pt : n + largo(n)) && i < 8; i++) {
		char c = n[i];

		if (c == ' ')
			c = '_';
		if (c >= 'a' && c <= 'z')
			c = (char)(c - 32);
		if (k + 1 < sizeof t)
			t[k++] = c;
	}
	if (pt && pt[1]) {
		if (k + 1 < sizeof t)
			t[k++] = '.';
		for (i = 1; pt[i] && i <= 3; i++) {
			char c = pt[i];

			if (c >= 'a' && c <= 'z')
				c = (char)(c - 32);
			if (k + 1 < sizeof t)
				t[k++] = c;
		}
	}
	t[k] = 0;
	copia(d, t, cabe);
}

int arch_guarda(libro *l, const char *ruta, int formato, char *real, u32 cabe_real, char *err, u32 cabe)
{
	bufer b = { 0 };
	int r = 1;
	char destino[600], tmp[620];
	void *f;
	const char *e = NULL;

	calc_recalcula(l);
	switch (formato) {
	case FMT_HOJ:
		r = hoj_escribe(l, &b, err, cabe);
		break;
	case FMT_XLSX:
		r = xlsx_escribe(l, &b, err, cabe);
		break;
	case FMT_ODS:
		r = ods_escribe(l, &b, err, cabe);
		break;
	case FMT_CSV: {
		csv_opc o = arch_csv;

		o.hoja = l->activa;
		r = csv_escribe(l, &o, &b, err, cabe);
		break;
	}
	case FMT_PDF:
		r = pdf_exporta(l, (int)l->activa, sin_carpeta(ruta), &b, err, cabe);
		break;
	default:
		copia(err, "Ese formato no se puede guardar", cabe);
	}
	if (r || b.mal) {
		if (!r)
			copia(err, "No hay memoria para guardar", cabe);
		buf_suelta(&b);
		return 1;
	}
	if (so_nombres_largos())
		copia(destino, ruta, sizeof destino);
	else
		nombre_83(ruta, destino, sizeof destino);
	/* (primero a uno de paso y luego en su sitio: si falla a medias, lo de antes sigue) */
	fmt(tmp, sizeof tmp, "%s", destino);
	{
		char *pt = strrchr(tmp, '.');
		char *bs = strrchr(tmp, '\\') > strrchr(tmp, '/') ? strrchr(tmp, '\\') : strrchr(tmp, '/');

		if (pt && (!bs || pt > bs))
			copia(pt, ".$$$", 5);
		else
			anade(tmp, ".$$$", sizeof tmp);
	}
	f = so_crea(tmp, &e);
	if (!f) {
		fmt(err, cabe, "No se puede crear %s: %s", sin_carpeta(destino), e ? e : "");
		buf_suelta(&b);
		return 1;
	}
	if (so_escribe(f, b.d, b.n) || so_cierra(f)) {
		fmt(err, cabe, "No se puede escribir %s (¿el disco lleno?)", sin_carpeta(destino));
		so_borra(tmp);
		buf_suelta(&b);
		return 1;
	}
	buf_suelta(&b);
	if (so_existe(destino) == 1)
		so_borra(destino);
	if (so_renombra(tmp, destino)) {
		fmt(err, cabe, "No se puede poner %s en su sitio", sin_carpeta(destino));
		return 1;
	}
	if (real)
		copia(real, destino, cabe_real);
	if (formato != FMT_PDF && formato != FMT_CSV)
		l->cambiado = 0;
	return 0;
}
