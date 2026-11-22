/* archivo.c - Pluma 2: abrir y guardar (pluma/archivo.h), y el texto llano.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "archivo.h"
#include "texto.h"

int arch_formato_ext(const char *ruta)
{
	char e[8];

	extension(ruta, e);
	if (iguales(e, "PLU"))
		return FMT_PLU;
	if (iguales(e, "ODT"))
		return FMT_ODT;
	if (iguales(e, "DOCX") || iguales(e, "DOC"))
		return FMT_DOCX;
	if (iguales(e, "RTF"))
		return FMT_RTF;
	if (iguales(e, "PDF"))
		return FMT_PDF;
	if (iguales(e, "TXT") || iguales(e, "TEXT") || iguales(e, "ASC") || iguales(e, "MD"))
		return FMT_TXT;
	return FMT_NADA;
}

static int contiene(const u8 *d, u32 n, const char *s)
{
	u32 m = largo(s), i;

	for (i = 0; i + m <= n; i++)
		if (!memcmp(d + i, s, m))
			return 1;
	return 0;
}

int arch_formato_datos(const u8 *d, u32 n)
{
	if (n >= 8 && !memcmp(d, "PLUMA1\x1a", 7))
		return FMT_PLU;
	if (n >= 5 && !memcmp(d, "{\\rtf", 5))
		return FMT_RTF;
	if (n >= 4 && !memcmp(d, "PK\x03\x04", 4)) {
		/* el «mimetype» del ODT va el primero, sin comprimir */
		if (n > 80 && contiene(d, n < 200 ? n : 200, "application/vnd.oasis.opendocument.text"))
			return FMT_ODT;
		if (contiene(d, n < 4096 ? n : 4096, "word/") || contiene(d, n, "word/document.xml"))
			return FMT_DOCX;
		if (contiene(d, n, "content.xml"))
			return FMT_ODT;
		return FMT_DOCX;
	}
	if (n >= 4 && !memcmp(d, "%PDF", 4))
		return FMT_PDF;
	return FMT_TXT;
}

const char *arch_nombre_formato(int f)
{
	switch (f) {
	case FMT_PLU: return "Documento de Pluma (.PLU)";
	case FMT_ODT: return "Documento de texto OpenDocument (.ODT)";
	case FMT_DOCX: return "Documento de Word (.DOCX)";
	case FMT_RTF: return "Texto enriquecido (.RTF)";
	case FMT_TXT: return "Texto sin formato (.TXT)";
	case FMT_PDF: return "PDF (.PDF)";
	}
	return "?";
}

const char *arch_extension(int f)
{
	switch (f) {
	case FMT_PLU: return "PLU";
	case FMT_ODT: return "ODT";
	case FMT_DOCX: return "DOCX";
	case FMT_RTF: return "RTF";
	case FMT_TXT: return "TXT";
	case FMT_PDF: return "PDF";
	}
	return "";
}

documento *arch_abre(const char *ruta, int *formato, char *error, u32 cabe)
{
	const char *e = NULL;
	u32 n;
	u8 *d = so_lee_archivo(ruta, &n, ARCH_MAX, &e);
	documento *doc = NULL;
	int f;

	if (!d) {
		fmt(error, cabe, "No se puede abrir %s: %s.", sin_carpeta(ruta), e ? e : "?");
		return NULL;
	}
	f = arch_formato_datos(d, n);
	error[0] = 0;
	switch (f) {
	case FMT_PLU: doc = plu_lee(d, n, error, cabe); break;
	case FMT_ODT: doc = odt_lee(d, n, error, cabe); break;
	case FMT_DOCX: doc = docx_lee(d, n, error, cabe); break;
	case FMT_RTF: doc = rtf_lee(d, n, error, cabe); break;
	case FMT_PDF:
		copia(error, "Pluma no abre los PDF (para eso está Hispa-PDF): sí los hace, con Archivo > Exportar a PDF.", cabe);
		break;
	default: doc = txt_lee(d, n, error, cabe); break;
	}
	so_suelta(d);
	if (!doc && !error[0])
		fmt(error, cabe, "%s no se puede leer: está estropeado o no es un documento.", sin_carpeta(ruta));
	if (doc) {
		doc->cambiado = 0;
		if (formato)
			*formato = f;
	}
	return doc;
}

/* el nombre 8+3 de uno largo (si el sistema no deja crear nombres largos) */
static void corto(const char *ruta, char *d, u32 cabe)
{
	const char *n = sin_carpeta(ruta);
	char base[16], ext[4];
	u32 k = 0, i, carpeta = (u32)(n - ruta);
	const char *punto = NULL;
	int largo_ = 0;

	for (i = 0; n[i]; i++)
		if (n[i] == '.')
			punto = n + i;
	for (i = 0; n + i != punto && n[i]; i++) {
		u8 c = (u8)n[i];

		if (c == ' ' || c == '.')
			continue;
		if (c >= 'a' && c <= 'z')
			c = (u8)(c - 32);
		if (c >= 0x80 || strchr("\"*+,/:;<=>?[\\]|", c))
			c = '_';
		if (k < 8)
			base[k++] = (char)c;
		else
			largo_ = 1;
	}
	if (largo_ && k > 6)
		k = 6;
	base[k] = 0;
	if (largo_)
		anade(base, "~1", sizeof base);
	k = 0;
	if (punto)
		for (i = 1; punto[i] && k < 3; i++) {
			u8 c = (u8)punto[i];

			if (c >= 'a' && c <= 'z')
				c = (u8)(c - 32);
			if (c >= 0x80 || strchr("\"*+,/:;<=>?[\\]| .", c))
				continue;
			ext[k++] = (char)c;
		}
	ext[k] = 0;
	if (carpeta >= cabe)
		carpeta = cabe - 1;
	memcpy(d, ruta, carpeta);
	d[carpeta] = 0;
	anade(d, base[0] ? base : "DOCUMENT", cabe);
	if (ext[0]) {
		anade(d, ".", cabe);
		anade(d, ext, cabe);
	}
}

static int es_83(const char *ruta)
{
	const char *n = sin_carpeta(ruta), *p = strchr(n, '.');
	u32 i;

	if ((p ? (u32)(p - n) : largo(n)) > 8 || (p && largo(p + 1) > 3) || (p && strchr(p + 1, '.')))
		return 0;
	for (i = 0; n[i]; i++)
		if ((u8)n[i] >= 0x80 || n[i] == ' ')
			return 0;
	return 1;
}

int arch_guarda(documento *d, const char *ruta, int formato, char *ruta_real, u32 cabe_ruta, char *error, u32 cabe)
{
	bufer b = { 0 };
	int r = -1;
	char tmp[600], final_[600];
	const char *e = NULL;
	void *f;

	switch (formato) {
	case FMT_PLU: r = plu_escribe(d, &b); break;
	case FMT_ODT: r = odt_escribe(d, &b); break;
	case FMT_DOCX: r = docx_escribe(d, &b); break;
	case FMT_RTF: r = rtf_escribe(d, &b); break;
	case FMT_TXT: r = txt_escribe(d, &b, 0); break;
	case FMT_PDF: r = pdf_exporta(d, &b); break;
	}
	if (r || b.mal) {
		buf_suelta(&b);
		copia(error, b.mal ? "No hay memoria para guardarlo." : "No se ha podido preparar el archivo.", cabe);
		return -1;
	}
	/* el nombre: si no hay nombres largos y este no es 8+3, el corto */
	copia(final_, ruta, sizeof final_);
	if (!so_nombres_largos() && !es_83(ruta) && so_existe(ruta) != 1)
		corto(ruta, final_, sizeof final_);
	/* a un temporal al lado, y luego en su sitio */
	{
		u32 k = (u32)(sin_carpeta(final_) - final_);

		memcpy(tmp, final_, k);
		tmp[k] = 0;
		anade(tmp, "PLUMATMP.$$$", sizeof tmp);
	}
	f = so_crea(tmp, &e);
	if (!f) {
		/* (a lo mejor no se puede crear al lado: directamente) */
		f = so_crea(final_, &e);
		if (!f) {
			fmt(error, cabe, "No se puede guardar %s: %s.", sin_carpeta(final_), e ? e : "?");
			buf_suelta(&b);
			return -1;
		}
		tmp[0] = 0;
	}
	if (so_escribe(f, b.d, b.n) || so_cierra(f)) {
		if (tmp[0])
			so_borra(tmp);
		fmt(error, cabe, "No se ha podido escribir %s (¿el disco está lleno?).", sin_carpeta(final_));
		buf_suelta(&b);
		return -1;
	}
	buf_suelta(&b);
	if (tmp[0]) {
		if (so_existe(final_) == 1 && so_borra(final_)) {
			so_borra(tmp);
			fmt(error, cabe, "No se puede cambiar %s (¿es de solo lectura o está abierto?).", sin_carpeta(final_));
			return -1;
		}
		if (so_renombra(tmp, final_)) {
			fmt(error, cabe, "Guardado como %s, pero no se ha podido poner el nombre %s.", sin_carpeta(tmp), sin_carpeta(final_));
			copia(ruta_real, tmp, cabe_ruta);
			return 0;
		}
	}
	copia(ruta_real, final_, cabe_ruta);
	if (formato != FMT_PDF)
		d->cambiado = 0;
	return 0;
}

/* ---- para los lectores ---- */
parrafo *arch_parrafo(documento *d)
{
	return doc_inserta_parrafos(d, d->n_par, 1);
}

void arch_letras(documento *d, const u32 *t, u32 n, u16 f)
{
	parrafo *p;
	u32 i;

	if (!d->n_par && !arch_parrafo(d))
		return;
	p = &d->par[d->n_par - 1];
	if (!par_cabe(p, p->n + n)) {
		d->sin_memoria = 1;
		return;
	}
	for (i = 0; i < n; i++) {
		p->t[p->n + i] = t[i];
		p->f[p->n + i] = f;
	}
	p->n += n;
	p->ancho_maq = -1;
}

/* ---- el texto llano ---- */
static int es_utf8(const u8 *d, u32 n)
{
	u32 i = 0, c;
	int altos = 0;

	while (i < n) {
		int k = utf8_lee(d + i, n - i, &c);

		if (c == 0xFFFD && !(k == 3 && d[i] == 0xEF))
			return 0;
		if (k > 1)
			altos = 1;
		i += (u32)k;
	}
	return altos || 1;
}

documento *txt_lee(const u8 *d, u32 n, char *error, u32 cabe)
{
	documento *doc = doc_nuevo();
	u32 i = 0, k;
	int u8_ = es_utf8(d, n);
	u16 f;
	u32 trozo[512];

	(void)error;
	(void)cabe;
	if (!doc)
		return NULL;
	/* el texto, con la letra de ancho fijo si parece una tabla de texto (no: la Normal) */
	f = doc_fmt(doc, &doc->est[EST_NORMAL].letra);
	doc->par[0].p = doc->est[EST_SIN_ESPACIO].p;
	if (n >= 3 && d[0] == 0xEF && d[1] == 0xBB && d[2] == 0xBF)
		i = 3;
	k = 0;
	while (i < n) {
		u32 c;

		if (u8_)
			i += (u32)utf8_lee(d + i, n - i, &c);
		else
			c = de437(d[i++]);
		if (c == '\r')
			continue;
		if (c == 0x1A)
			break;
		if (c == '\n') {
			parrafo *p;

			arch_letras(doc, trozo, k, f);
			k = 0;
			p = arch_parrafo(doc);
			if (!p)
				break;
			p->p = doc->est[EST_SIN_ESPACIO].p;
			continue;
		}
		if (c == 0x0C) {
			/* (un salto de página de DOS) */
			arch_letras(doc, trozo, k, f);
			k = 0;
			doc->par[doc->n_par - 1].p.salto_antes = 1;
			continue;
		}
		if (c < 0x20 && c != '\t')
			continue;
		trozo[k++] = c;
		if (k == 512) {
			arch_letras(doc, trozo, k, f);
			k = 0;
		}
		if (doc->sin_memoria)
			break;
	}
	arch_letras(doc, trozo, k, f);
	if (doc->sin_memoria) {
		doc_suelta(doc);
		copia(error, "No hay memoria para abrirlo.", cabe);
		return NULL;
	}
	return doc;
}

int txt_escribe(documento *d, bufer *b, int utf8)
{
	u32 i, k;

	for (i = 0; i < d->n_par; i++) {
		const parrafo *p = &d->par[i];

		if (p->p.salto_antes && i)
			buf_c(b, 0x0C);
		for (k = 0; k < p->n; k++) {
			u32 c = p->t[k];

			if (c == SALTO_LINEA)
				buf_txt(b, "\r\n");
			else if (es_objeto(c))
				continue;
			else if (utf8)
				buf_u32(b, c);
			else
				buf_c(b, a437(c));
		}
		if (p->p.tabla && i + 1 < d->n_par && d->par[i + 1].p.tabla == p->p.tabla && d->par[i + 1].p.fila == p->p.fila)
			buf_c(b, '\t');
		else
			buf_txt(b, "\r\n");
	}
	return b->mal ? -1 : 0;
}
