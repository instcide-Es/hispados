/* vista.c - Correo: el mensaje abierto. Arriba, sus cabeceras (de, para,
 * fecha, asunto) y los avisos en un recuadro con «[!]» (el remitente que no
 * cuadra, los enlaces engañosos, las imágenes de fuera sin cargar, con
 * «Mostrar imágenes»); los adjuntos, un botón cada uno (Abrir: primero por
 * Escudo; Guardar...); y el cuerpo: el documento de html.c o el texto,
 * cortado a lo ancho de la ventana, con los enlaces subrayados (un clic lo
 * pregunta antes de abrir el Navegador; si engaña, lo dice) y las imágenes
 * de dentro (cid:) pintadas. Las de fuera, solo si se pide (los píxeles de
 * rastreo dicen al que lo manda que se ha leído).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include <stdio.h>
#include "ventana.h"

u32 *img_lee(const u8 *d, u32 n, int max_an, int max_al, int *an, int *al);

typedef struct {
	u32 ini, n;		/* en doc->texto */
	u16 estilo, enlace;
	i16 x;
} Pieza;

typedef struct {
	u32 p0, np;		/* sus piezas */
	i16 sangria;
	u8 clase, nivel;
	u16 alto;
	int imagen;		/* (una línea con una imagen: su número, o -1) */
} Linea;

typedef struct {
	u32 *px;
	int an, al;
	int probada;
} Img;

static struct {
	u32 uid;
	int hay;
	u8 *d;
	u32 n;
	Mensaje *m;
	Doc *doc;
	int es_html;
	char supl[300];
	int grave;
	int mostrar;		/* las imágenes de fuera, pedidas */
	Img *img;
	/* lo cortado a lo ancho */
	Pieza *pz;
	u32 npz, cpz;
	Linea *ln;
	u32 nln, cln;
	int ancho;		/* para el que está hecho (0: hay que hacerlo) */
	int desde;		/* lo que se ha bajado (en puntos) */
	int alto_total, alto_cab;
	int x0, y0, an, al;
	char de[300], para[400], cc[400], fecha[80], asunto[400];
} V;

int vista_hay(void) { return V.hay; }
Mensaje *vista_mensaje(void) { return V.m; }
const u8 *vista_datos(u32 *n) { *n = V.n; return V.d; }

char *vista_texto_plano(u32 *n)
{
	*n = 0;
	if (!V.doc)
		return NULL;
	return doc_a_texto(V.doc, n);
}

void vista_cierra(void)
{
	u32 i;

	if (V.img && V.doc)
		for (i = 0; i < V.doc->n_imagenes; i++)
			so_suelta(V.img[i].px);
	so_suelta(V.img);
	if (V.doc)
		doc_libera(V.doc);
	if (V.m)
		mime_libera(V.m);
	so_suelta(V.d);
	so_suelta(V.pz);
	so_suelta(V.ln);
	memset(&V, 0, sizeof V);
}

static void cab(const char *nombre, char *d, u32 cabe)
{
	char *v = mime_cabecera_texto(V.m, 0, nombre);

	d[0] = 0;
	if (v) {
		copia_utf8(d, cabe, v, (u32)strlen(v));
		so_suelta(v);
	}
}

void vista_abre(Ficha *f)
{
	u32 n;
	u8 *d;
	int p;

	vista_cierra();
	if (!f || !ses)
		return;
	if (f->banderas & B_GRANDE && !f->largo) {
		char t[300];

		snprintf(t, sizeof t, "Este mensaje mide %u KB, m\xc3\xa1s de lo que se baja solo. \xc2\xbf" "Bajarlo?", (unsigned)(f->tam / 1024));
		if (pregunta("Mensaje grande", t, "Bajarlo", "No", NULL) != 0)
			return;
	}
	cancelado = 0;
	red_espera = espera_red;
	d = motor_mensaje(ses, f, &n);
	if (!d) {
		di_estado("%s", ses->error);
		return;
	}
	V.uid = f->uid;
	V.d = d;
	V.n = n;
	V.m = mime_analiza(d, n);
	if (!V.m) {
		vista_cierra();
		return;
	}
	V.hay = 1;
	cab("From", V.de, sizeof V.de);
	cab("To", V.para, sizeof V.para);
	cab("Cc", V.cc, sizeof V.cc);
	cab("Subject", V.asunto, sizeof V.asunto);
	{
		char *v = mime_cabecera(V.m, 0, "Date", 0);
		i64 s = v ? mime_fecha(v, (u32)strlen(v)) : 0;

		so_suelta(v);
		if (s)
			mime_fecha_larga(s, so_zona(s), V.fecha, sizeof V.fecha);
	}
	V.grave = mime_suplantacion(V.m, V.supl, sizeof V.supl);
	p = mime_principal(V.m, ajustes.html);
	if (p >= 0) {
		u32 tn;
		char *t = mime_texto(V.m, p, &tn);

		if (t) {
			V.es_html = !strcmp(V.m->partes[p].tipo, "text/html");
			V.doc = V.es_html ? html_a_doc(t, tn) : texto_a_doc(t, tn);
			so_suelta(t);
		}
	}
	if (!V.doc)
		V.doc = texto_a_doc("", 0);
	if (V.doc && V.doc->n_imagenes) {
		V.img = so_pide(V.doc->n_imagenes * sizeof(Img));
		if (V.img)
			memset(V.img, 0, V.doc->n_imagenes * sizeof(Img));
	}
	so_apunta("le\xc3\xad" "do: %u \xc2\xab%s\xc2\xbb (%s, %u adjuntos%s%s%s)", (unsigned)V.uid, V.asunto, V.es_html ? "HTML" : "texto",
		  (unsigned)mime_n_adjuntos(V.m), V.doc && V.doc->habia_scripts ? ", sin scripts" : "",
		  V.doc && V.doc->remotas ? ", im\xc3\xa1genes de fuera sin cargar" : "", V.grave ? ", AVISO de suplantaci\xc3\xb3n" : "");
	if (V.doc)
		for (n = 0; n < V.doc->n_enlaces; n++)
			if (V.doc->enlaces[n].enganoso)
				so_apunta("aviso: enlace enga\xc3\xb1oso: %s", V.doc->enlaces[n].motivo);
}

/* ---- las imágenes ---- */
static void carga_imagen(u32 k, int max_an)
{
	Imagen *im = &V.doc->imagenes[k];
	Img *g = &V.img[k];
	u8 *d = NULL;
	u32 n = 0;
	int libre = 0;

	if (g->probada)
		return;
	g->probada = 1;
	if (!im->src)
		return;
	if (!strncmp(im->src, "cid:", 4)) {
		int p = mime_por_id(V.m, im->src + 4);

		if (p >= 0) {
			d = mime_cuerpo(V.m, p, &n);
			libre = 1;
		}
	} else if (im->remota && V.mostrar && !im->rastreo) {
		char e[200];
		int r;

		cancelado = 0;
		red_espera = espera_red;
		r = http_get(im->src, &d, &n, 3u << 20, 0, e, sizeof e);
		so_apunta("imagen de fuera %s: %d", im->src, r);
		if (r != 200) {
			so_suelta(d);
			d = NULL;
		}
		libre = 1;
	}
	if (d) {
		int mal = max_an, mah = 600;

		if (im->ancho > 0 && im->ancho < mal)
			mal = im->ancho;
		g->px = img_lee(d, n, mal, mah, &g->an, &g->al);
	}
	if (libre)
		so_suelta(d);
}

/* ---- cortar a lo ancho ---- */
static int pon_pieza(u32 ini, u32 n, u16 estilo, u16 enlace, int x)
{
	if (V.npz == V.cpz) {
		u32 c = V.cpz ? V.cpz * 2 : 512;
		Pieza *q = so_repide(V.pz, c * sizeof(Pieza));

		if (!q)
			return -1;
		V.pz = q;
		V.cpz = c;
	}
	V.pz[V.npz].ini = ini;
	V.pz[V.npz].n = n;
	V.pz[V.npz].estilo = estilo;
	V.pz[V.npz].enlace = enlace;
	V.pz[V.npz].x = (i16)x;
	V.npz++;
	return 0;
}

static Linea *linea_nueva(int sangria, u8 clase, u8 nivel)
{
	Linea *l;

	if (V.nln == V.cln) {
		u32 c = V.cln ? V.cln * 2 : 256;
		Linea *q = so_repide(V.ln, c * sizeof(Linea));

		if (!q)
			return NULL;
		V.ln = q;
		V.cln = c;
	}
	l = &V.ln[V.nln++];
	l->p0 = V.npz;
	l->np = 0;
	l->sangria = (i16)sangria;
	l->clase = clase;
	l->nivel = nivel;
	l->alto = LT + 2;
	l->imagen = -1;
	return l;
}

static void corta(int ancho)
{
	u32 p, t;
	const Doc *d = V.doc;

	V.npz = V.nln = 0;
	V.ancho = ancho;
	if (!d)
		return;
	for (p = 0; p < d->n_parrafos; p++) {
		const Parrafo *pa = &d->parrafos[p];
		int sangria = pa->nivel * 2 * LA + (pa->clase == P_CITA ? LA : 0), cols = (ancho - sangria) / LA, col = 0, h;
		Linea *l;

		if (cols < 10)
			cols = 10;
		for (h = 0; h < pa->hueco; h++) {
			l = linea_nueva(0, P_NORMAL, 0);
			if (!l)
				return;
			l->alto = 8;
		}
		l = linea_nueva(sangria, pa->clase, pa->nivel);
		if (!l)
			return;
		if (pa->clase == P_RAYA) {
			l->alto = 10;
			continue;
		}
		for (t = pa->ini; t < pa->fin && t < d->n_tramos; t++) {
			const Tramo *tr = &d->tramos[t];
			const char *s = d->texto + tr->ini;
			u32 i = 0, n = tr->n;

			if (tr->estilo & E_IMAGEN) {
				u32 k = tr->enlace;

				if (k < d->n_imagenes && V.img) {
					carga_imagen(k, ancho - sangria);
					if (V.img[k].px) {
						Linea *li;

						if (l->np)
							l = linea_nueva(sangria, pa->clase, pa->nivel);
						if (!l)
							return;
						l->imagen = (int)k;
						l->alto = (u16)(V.img[k].al + 4);
						li = linea_nueva(sangria, pa->clase, pa->nivel);
						if (!li)
							return;
						l = li;
						col = 0;
						continue;
					}
				}
				{
					/* sin imagen: su texto («[imagen: ...]»), como una pieza más */
					static char alt[160];
					const Imagen *im = k < d->n_imagenes ? &d->imagenes[k] : NULL;

					snprintf(alt, sizeof alt, "[%s%s%s]", im && im->remota ? "imagen de fuera" : "imagen",
						 im && im->alt && im->alt[0] ? ": " : "", im && im->alt ? im->alt : "");
					(void)alt;
					if (col + 10 > cols && col) {
						l = linea_nueva(sangria, pa->clase, pa->nivel);
						if (!l)
							return;
						col = 0;
					}
					pon_pieza(tr->ini, tr->n, (u16)(tr->estilo | E_IMAGEN), tr->enlace, col * LA);
					l->np++;
					col += 10;
				}
				continue;
			}
			while (i < n) {
				/* la palabra siguiente (con los espacios de delante), o un \n */
				u32 j = i, letras = 0, k;

				if (s[i] == '\n') {
					l = linea_nueva(sangria, pa->clase, pa->nivel);
					if (!l)
						return;
					col = 0;
					i++;
					continue;
				}
				while (j < n && s[j] == ' ')
					j++, letras++;
				while (j < n && s[j] != ' ' && s[j] != '\n') {
					if (((u8)s[j] & 0xC0) != 0x80)
						letras++;
					j++;
				}
				if (col + (int)letras > cols && col > 0) {
					/* a la siguiente, sin los espacios de delante */
					while (i < j && s[i] == ' ')
						i++, letras--;
					l = linea_nueva(sangria, pa->clase, pa->nivel);
					if (!l)
						return;
					col = 0;
				}
				/* una palabra más larga que la línea: a trozos */
				while ((int)letras > cols) {
					u32 c = 0;

					k = i;
					while (k < j && (int)c < cols) {
						if (((u8)s[k] & 0xC0) != 0x80)
							c++;
						k++;
					}
					while (k < j && ((u8)s[k] & 0xC0) == 0x80)
						k++;
					pon_pieza(tr->ini + i, k - i, tr->estilo, tr->enlace, col * LA);
					l->np++;
					letras -= c;
					i = k;
					l = linea_nueva(sangria, pa->clase, pa->nivel);
					if (!l)
						return;
					col = 0;
				}
				if (j > i) {
					pon_pieza(tr->ini + i, j - i, tr->estilo, tr->enlace, col * LA);
					l->np++;
					col += (int)letras;
				}
				i = j;
			}
		}
	}
	V.alto_total = 0;
	for (p = 0; p < V.nln; p++)
		V.alto_total += V.ln[p].alto;
}

/* ---- pintar ---- */
static int alto_cabecera(int an)
{
	int h = 8 + 4 * 18 + 6;

	(void)an;
	if (V.cc[0])
		h += 18;
	if (V.supl[0])
		h += 30;
	if (V.doc && (V.doc->enganosos || V.doc->habia_scripts))
		h += 24;
	if (V.doc && V.doc->remotas && !V.mostrar)
		h += 32;
	if (mime_n_adjuntos(V.m))
		h += ALTO_BOTON + 8;
	return h;
}

static void linea_cab(int x, int y, int an, const char *r, const char *v, int neg)
{
	negrita(x, y, r, 10, C_SUAVE);
	ui_texto(x + 80, y, v, (u32)strlen(v), (an - 90) / LA, C_TINTA, neg);
}

void vista_pinta(int x, int y, int an, int al)
{
	int cy = y + 8, i, bx;
	u32 k;

	V.x0 = x;
	V.y0 = y;
	V.an = an;
	V.al = al;
	ui_rect(x, y, an, al, C_PAPEL);
	if (!V.hay) {
		texto(x + 16, y + 16, "Elija un mensaje de la lista.", (an - 32) / LA, C_SUAVE);
		return;
	}
	/* las cabeceras */
	ui_rect(x, y, an, alto_cabecera(an) - 2, C_BARRA);
	linea_cab(x + 10, cy, an, "De:", V.de, 1);
	cy += 18;
	linea_cab(x + 10, cy, an, "Para:", V.para, 0);
	cy += 18;
	if (V.cc[0]) {
		linea_cab(x + 10, cy, an, "CC:", V.cc, 0);
		cy += 18;
	}
	linea_cab(x + 10, cy, an, "Fecha:", V.fecha, 0);
	cy += 18;
	linea_cab(x + 10, cy, an, "Asunto:", V.asunto, 1);
	cy += 24;
	if (V.supl[0]) {
		ui_rect(x + 8, cy - 2, an - 16, 26, C_ROJO);
		ui_rect(x + 10, cy, an - 20, 22, C_ROJO_CLARO);
		icono(IC_AVISO, x + 14, cy + 5, C_ROJO, C_ROJO_CLARO);
		negrita(x + 30, cy + 3, V.grave >= 2 ? "[!] Cuidado:" : "[!] Ojo:", 13, C_ROJO);
		ui_texto(x + 30 + 13 * LA, cy + 3, V.supl, (u32)strlen(V.supl), (an - 50) / LA - 13, C_ROJO, 0);
		cy += 30;
	}
	if (V.doc && (V.doc->enganosos || V.doc->habia_scripts)) {
		char t[300];

		snprintf(t, sizeof t, "%s%s%s", V.doc->enganosos ? "[!] Hay enlaces que dicen una direcci\xc3\xb3n y llevan a otra (se avisa al pulsarlos)." : "",
			 V.doc->enganosos && V.doc->habia_scripts ? " " : "", V.doc->habia_scripts ? "[i] Se han quitado sus programas (JavaScript)." : "");
		ui_texto(x + 10, cy, t, (u32)strlen(t), (an - 20) / LA, V.doc->enganosos ? C_ROJO : C_SUAVE, V.doc->enganosos);
		cy += 24;
	}
	if (V.doc && V.doc->remotas && !V.mostrar) {
		char t[200];

		ui_rect(x + 8, cy - 2, an - 16, 28, C_AMBAR_CLARO);
		snprintf(t, sizeof t, "[i] %u im\xc3\xa1genes de fuera sin cargar (as\xc3\xad no se sabe que lo ha le\xc3\xad" "do).", (unsigned)V.doc->remotas);
		texto(x + 14, cy + 4, t, (an - 200) / LA, C_TINTA);
		boton(60, x + an - 16 - ancho_texto("Mostrar im\xc3\xa1genes") - 20, cy, 0, "Mostrar im\xc3\xa1genes", 0, 0, 0);
		cy += 32;
	}
	bx = x + 10;
	for (i = 0; i < mime_n_adjuntos(V.m) && i < 20; i++) {
		int a = mime_adjunto(V.m, i), w;
		char t[300];
		const char *nm = V.m->partes[a].nombre[0] ? V.m->partes[a].nombre : "(sin nombre)";

		snprintf(t, sizeof t, "%s (%u KB)", nm, (unsigned)((V.m->partes[a].tam + 1023) / 1024));
		w = ancho_texto(t) + 40;
		if (w > an - 20)
			w = an - 20;
		if (bx + w > x + an - 10 && bx > x + 10)
			break;
		ui_redondo(bx, cy, w, ALTO_BOTON, 4, C_BORDE, C_BORDE);
		ui_redondo(bx + 1, cy + 1, w - 2, ALTO_BOTON - 2, 4, 0xE5E7EB, 0xE5E7EB);
		icono(IC_CLIP, bx + 6, cy + 7, C_TINTA, 0xE5E7EB);
		ui_texto(bx + 22, cy + 5, t, (u32)strlen(t), (w - 30) / LA, C_TINTA, 0);
		zona(70 + i, bx, cy, w, ALTO_BOTON);
		bx += w + 6;
	}
	V.alto_cab = alto_cabecera(an);
	/* el cuerpo */
	if (V.ancho != an - 28)
		corta(an - 28);
	{
		int ty = y + V.alto_cab + 4 - V.desde, alto = al - V.alto_cab - 4;
		u32 li;

		if (V.desde > V.alto_total - alto + 20)
			V.desde = V.alto_total - alto + 20 > 0 ? V.alto_total - alto + 20 : 0;
		for (li = 0; li < V.nln; li++) {
			Linea *l = &V.ln[li];
			int lx = x + 12 + l->sangria;

			if (ty + l->alto < y + V.alto_cab) {
				ty += l->alto;
				continue;
			}
			if (ty > y + al)
				break;
			if (ty >= y + V.alto_cab - 2) {
				if (l->clase == P_CITA)
					for (k = 0; k <= l->nivel; k++)
						ui_rect(x + 6 + (int)k * 2 * LA, ty, 3, l->alto, 0x9CA3AF);
				if (l->clase == P_RAYA)
					ui_rect(x + 12, ty + 4, an - 28, 1, C_GRIS);
				if (l->imagen >= 0 && V.img && V.img[l->imagen].px && ty + l->alto <= y + al)
					ui_mapa(lx, ty + 2, V.img[l->imagen].an, V.img[l->imagen].al, V.img[l->imagen].px);
				for (k = 0; k < l->np; k++) {
					const Pieza *p = &V.pz[l->p0 + k];
					u32 tinta = p->estilo & E_ENLACE ? C_AZUL : l->clase == P_CITA ? C_SUAVE : C_TINTA;
					int neg = (p->estilo & (E_NEGRITA | E_TITULO)) != 0, px = lx + p->x;

					if (p->estilo & E_IMAGEN) {
						const Imagen *im = p->enlace < V.doc->n_imagenes ? &V.doc->imagenes[p->enlace] : NULL;
						char t[160];

						snprintf(t, sizeof t, "[%s%s%.60s]", im && im->remota ? "imagen de fuera" : "imagen",
							 im && im->alt && im->alt[0] ? ": " : "", im && im->alt ? im->alt : "");
						ui_texto(px, ty, t, (u32)strlen(t), 80, C_SUAVE, 0);
						continue;
					}
					ui_texto(px, ty, V.doc->texto + p->ini, p->n, (x + an - 8 - px) / LA, tinta, neg);
					if (p->estilo & (E_ENLACE | E_SUBRAYADO)) {
						int letras = (int)utf8_cuenta(V.doc->texto + p->ini, p->n), sp = 0;

						while (sp < (int)p->n && V.doc->texto[p->ini + sp] == ' ')
							sp++;
						ui_rect(px + sp * LA, ty + LT - 1, (letras - sp) * LA, 1, tinta);
					}
					if (p->estilo & E_TACHADO)
						ui_rect(px, ty + LT / 2, (int)utf8_cuenta(V.doc->texto + p->ini, p->n) * LA, 1, tinta);
				}
			}
			ty += l->alto;
		}
		/* la barra de desplazar, si no cabe */
		if (V.alto_total > alto) {
			int bh = alto * alto / V.alto_total, by = y + V.alto_cab + (alto - bh) * V.desde / (V.alto_total - alto + 20);

			if (bh < 20)
				bh = 20;
			ui_rect(x + an - 8, y + V.alto_cab, 8, alto, C_BARRA);
			ui_rect(x + an - 7, by, 6, bh, C_BORDE);
		}
		ui_rect(x, y + V.alto_cab - 2, an, 1, C_GRIS);
		(void)ty;
	}
}

void vista_rueda(int pasos)
{
	V.desde -= pasos * 3 * (LT + 2);
	if (V.desde < 0)
		V.desde = 0;
}

int vista_tecla(u32 k, u32 mod)
{
	int pag = V.al - V.alto_cab - 40;

	(void)mod;
	if (!V.hay)
		return 0;
	if (k == UT_AV_PAG || k == ' ')
		V.desde += pag;
	else if (k == UT_RE_PAG)
		V.desde -= pag;
	else if (k == UT_ABAJO)
		V.desde += LT + 2;
	else if (k == UT_ARRIBA)
		V.desde -= LT + 2;
	else if (k == UT_INICIO)
		V.desde = 0;
	else if (k == UT_FIN)
		V.desde = V.alto_total;
	else
		return 0;
	if (V.desde < 0)
		V.desde = 0;
	return 1;
}

/* ---- los adjuntos ---- */
/* un nombre de 8+3 para el disco, de un nombre cualquiera */
static void nombre_83(const char *nombre, char *d, u32 cabe)
{
	char *p;
	u32 n, i, k = 0, e = 0;
	const char *punto;
	char base[16], ext[8];

	p = utf8_plano(nombre, (u32)strlen(nombre), &n);
	punto = p ? strrchr(p, '.') : NULL;
	for (i = 0; p && p[i] && p + i != punto && k < 8; i++) {
		char c = p[i];

		if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')
			base[k++] = (char)(c >= 'a' && c <= 'z' ? c - 32 : c);
	}
	base[k] = 0;
	if (punto)
		for (i = 1; punto[i] && e < 3; i++) {
			char c = punto[i];

			if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
				ext[e++] = (char)(c >= 'a' && c <= 'z' ? c - 32 : c);
		}
	ext[e] = 0;
	so_suelta(p);
	if (!k)
		snprintf(base, sizeof base, "ADJUNTO");
	snprintf(d, cabe, "%s%s%s", base, e ? "." : "", ext);
}

static int es_programa(const char *nombre)
{
	static const char *const malos[] = { ".exe", ".com", ".bat", ".cmd", ".scr", ".pif", ".js", ".jse", ".vbs", ".vbe", ".wsf",
		".hta", ".msi", ".lnk", ".jar", ".ps1", ".bin", ".cpl", ".dll", ".reg", ".docm", ".xlsm" };
	u32 n = (u32)strlen(nombre), i;

	for (i = 0; i < sizeof malos / sizeof malos[0]; i++) {
		u32 k = (u32)strlen(malos[i]), j;

		if (n < k)
			continue;
		for (j = 0; j < k; j++)
			if ((nombre[n - k + j] | 32) != malos[i][j])
				break;
		if (j == k)
			return 1;
	}
	return 0;
}

static int escribe(const char *ruta, const u8 *d, u32 n)
{
	void *f = so_abre(ruta, SO_CREAR);
	int r;

	if (!f)
		return -1;
	r = so_escribe(f, d, n);
	return so_cierra(f) || r ? -1 : 0;
}

void vista_adjunto(int k, int guardar)
{
	int a = mime_adjunto(V.m, k);
	const char *nm;
	char n83[20], ruta[300], t[600];
	u8 *d;
	u32 n;

	if (a < 0)
		return;
	nm = V.m->partes[a].nombre[0] ? V.m->partes[a].nombre : "adjunto";
	d = mime_cuerpo(V.m, a, &n);
	if (!d)
		return;
	nombre_83(nm, n83, sizeof n83);
	if (guardar) {
		char doc[260];

		so_carpeta_documentos(doc, sizeof doc);
		snprintf(ruta, sizeof ruta, "%s%s%s", doc, doc[strlen(doc) - 1] == so_barra() ? "" : (so_barra() == '\\' ? "\\" : "/"), n83);
		if (elige_archivo("Guardar el adjunto", ruta, sizeof ruta, 1, NULL)) {
			if (escribe(ruta, d, n)) {
				snprintf(t, sizeof t, "No se ha podido guardar en %s.", ruta);
				aviso("Guardar", t);
			} else {
				so_apunta("adjunto guardado: %s (%u octetos)", ruta, (unsigned)n);
				di_estado("Guardado en %s", ruta);
			}
		}
		so_suelta(d);
		return;
	}
	/* abrir: a la carpeta de paso, Escudo y, si no dice nada, con su programa */
	{
		char paso[260], esc[260], eq[260];
		int r;

		so_carpeta_paso(paso, sizeof paso);
		snprintf(ruta, sizeof ruta, "%s%c%s", paso, so_barra(), n83);
		if (escribe(ruta, d, n)) {
			so_suelta(d);
			snprintf(t, sizeof t, "No se ha podido sacar el adjunto a %s.", ruta);
			aviso("Abrir", t);
			return;
		}
		so_suelta(d);
		if (so_programa("ESCUDO.BIN", esc, sizeof esc)) {
			snprintf(t, sizeof t, "/DESCARGA %s", ruta);
			di_estado("Escudo est\xc3\xa1 revisando %s...", nm);
			repinta_todo();
			r = so_lanza_espera(esc, t);
			so_apunta("adjunto %s: Escudo dice %d", ruta, r);
			if (r == 1) {
				snprintf(t, sizeof t, "Escudo ha encontrado algo malo en \xc2\xab%s\xc2\xbb: no se abre.", nm);
				pregunta_peligro("Adjunto peligroso", t, "Entendido", "Cerrar");
				so_borra(ruta);
				return;
			}
		} else {
			snprintf(t, sizeof t, "Escudo (el antivirus) no est\xc3\xa1 instalado: \xc2\xab%s\xc2\xbb no se ha revisado. \xc2\xbf" "Abrirlo igual?", nm);
			if (!pregunta_peligro("Sin revisar", t, "Abrirlo", "No")) {
				so_borra(ruta);
				return;
			}
		}
		if (es_programa(nm)) {
			snprintf(t, sizeof t, "\xc2\xab%s\xc2\xbb es un programa (o puede llevar uno). Un programa que llega por correo es la forma "
				 "m\xc3\xa1s corriente de colar un virus. \xc2\xbf" "Seguro que quiere abrirlo?", nm);
			so_apunta("adjunto %s: es un programa: se pregunta", ruta);
			if (!pregunta_peligro("Es un programa", t, "Abrirlo", "No abrirlo")) {
				so_borra(ruta);
				return;
			}
		}
		if (so_programa("EQUIPO.BIN", eq, sizeof eq)) {
			snprintf(t, sizeof t, "/ABRIR %s", ruta);
			so_lanza(eq, t);
			so_apunta("adjunto abierto: %s", ruta);
			di_estado("Abierto %s", nm);
		} else
			aviso("Abrir", "No hay con qu\xc3\xa9 abrirlo (falta Mi equipo).");
	}
}

/* ---- el ratón ---- */
static void abre_enlace(u32 k)
{
	Enlace *e;
	char t[700], nav[260];

	if (!V.doc || k >= V.doc->n_enlaces)
		return;
	e = &V.doc->enlaces[k];
	if (!e->href)
		return;
	if (!strncmp(e->href, "mailto:", 7)) {
		redacta(RD_NUEVO, e->href + 7);
		return;
	}
	if (e->enganoso) {
		snprintf(t, sizeof t, "%s Lleva de verdad a: %.300s. \xc2\xbf" "Abrirlo igual?", e->motivo, e->href);
		so_apunta("enlace enga\xc3\xb1oso pulsado: %s", e->href);
		if (!pregunta_peligro("Enlace enga\xc3\xb1oso", t, "Abrirlo", "No"))
			return;
	} else {
		snprintf(t, sizeof t, "\xc2\xbf" "Abrir en el Navegador %.400s?", e->href);
		if (pregunta("Abrir el enlace", t, "Abrir", "No", NULL) != 0)
			return;
	}
	if (so_programa("NAVEGA\\NAVEGA.BIN", nav, sizeof nav) || so_programa("NAVEGA.BIN", nav, sizeof nav))
		so_lanza(nav, e->href);
	else
		aviso("Abrir el enlace", "No est\xc3\xa1 el Navegador.");
}

int vista_raton(int x, int y, u32 botones)
{
	int z = zona_en(x, y);
	u32 li;
	int ty;

	if (!V.hay)
		return 0;
	if (z == 60) {
		V.mostrar = 1;
		so_apunta("im\xc3\xa1genes de fuera: pedidas");
		if (V.img) {
			u32 i;

			for (i = 0; i < V.doc->n_imagenes; i++)
				if (V.doc->imagenes[i].remota)
					V.img[i].probada = 0;
		}
		V.ancho = 0;
		return 1;
	}
	if (z >= 70 && z < 90) {
		int e = (botones & UB_DER) ? -1 : 0;

		ui_menu(x, y, "Abrir (revis\xc3\xa1ndolo antes con Escudo)|Guardar...", 0, (u32)(1000 + z - 70));
		(void)e;
		return 1;
	}
	/* ¿un enlace? */
	ty = V.y0 + V.alto_cab + 4 - V.desde;
	for (li = 0; li < V.nln; li++) {
		Linea *l = &V.ln[li];

		if (y >= ty && y < ty + l->alto) {
			u32 k;

			for (k = 0; k < l->np; k++) {
				const Pieza *p = &V.pz[l->p0 + k];
				int px = V.x0 + 12 + l->sangria + p->x, w = (int)utf8_cuenta(V.doc->texto + p->ini, p->n) * LA;

				if ((p->estilo & E_ENLACE) && x >= px && x < px + w) {
					abre_enlace(p->enlace);
					return 1;
				}
			}
			return 0;
		}
		ty += l->alto;
	}
	return 0;
}

u32 uid_abierto(void) { return V.hay ? V.uid : 0; }

/* un .EML suelto (Mi equipo, dos clics): lo mismo, sin cuenta */
int vista_abre_datos(u8 *d, u32 n)
{
	Ficha f;

	vista_cierra();
	memset(&f, 0, sizeof f);
	V.d = d;
	V.n = n;
	V.m = mime_analiza(d, n);
	if (!V.m) {
		vista_cierra();
		return -1;
	}
	V.hay = 1;
	cab("From", V.de, sizeof V.de);
	cab("To", V.para, sizeof V.para);
	cab("Cc", V.cc, sizeof V.cc);
	cab("Subject", V.asunto, sizeof V.asunto);
	{
		char *v = mime_cabecera(V.m, 0, "Date", 0);
		i64 s = v ? mime_fecha(v, (u32)strlen(v)) : 0;

		so_suelta(v);
		if (s)
			mime_fecha_larga(s, so_zona(s), V.fecha, sizeof V.fecha);
	}
	V.grave = mime_suplantacion(V.m, V.supl, sizeof V.supl);
	{
		int p = mime_principal(V.m, ajustes.html);

		if (p >= 0) {
			u32 tn;
			char *t = mime_texto(V.m, p, &tn);

			if (t) {
				V.es_html = !strcmp(V.m->partes[p].tipo, "text/html");
				V.doc = V.es_html ? html_a_doc(t, tn) : texto_a_doc(t, tn);
				so_suelta(t);
			}
		}
	}
	if (!V.doc)
		V.doc = texto_a_doc("", 0);
	if (V.doc && V.doc->n_imagenes) {
		V.img = so_pide(V.doc->n_imagenes * sizeof(Img));
		if (V.img)
			memset(V.img, 0, V.doc->n_imagenes * sizeof(Img));
	}
	(void)f;
	return 0;
}
