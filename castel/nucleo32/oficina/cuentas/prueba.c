/* prueba.c - Cuentas 2: los guiones de pruebas (CUENTAS --guion GUION):
 * una orden por línea, para escribir en las celdas, hacer cosas con el
 * libro y mirar lo que sale. Dice «mal: ...» por cada cosa que no casa y,
 * al final, «== N bien, M mal».
 *   # comentario
 *   hoja NOMBRE              a esa hoja (si no está, una nueva)
 *   loc es|en                cómo se escribe (es: «1,5» y «;»; en: «1.5» y «,», en inglés)
 *   pon A1 TEXTO             lo que se teclea en A1
 *   num A1 3.25              A1 vale ese número (con 1e-9 de margen)
 *   txt A1 TEXTO             A1 es ese texto
 *   log A1 1|0               A1 es VERDADERO o FALSO
 *   err A1 #¡DIV/0!          A1 es ese error
 *   ver A1 TEXTO             en A1 se ve ese texto (con su formato)
 *   form A1 TEXTO            lo que sale al corregir A1 (la fórmula, con «=»)
 *   vacia A1                 A1 no tiene nada
 *   circ                     se ha visto una referencia circular
 *   fmt A1:B2 CODIGO         el formato de número (como en los .XLSX)
 *   insfila 3 [N] | borfila 3 [N] | inscol C [N] | borcol C [N]
 *   deshaz | rehaz
 *   copia A1:B2 | corta A1:B2 | pega C5 [valores|formatos|formulas|transp]
 *   rellena A1:A2 A1:A10     el controlador de rellenar
 *   ordena A1:C9 B [desc] [titulos]
 *   nombre NOMBRE =Hoja1!$A$1:$A$5
 *   guarda RUTA | abre RUTA
 *   celdas N                 la hoja tiene N celdas
 *   estilo A1:B2 COSAS       negrita cursiva subrayado tachado tam=14 letra=DejaVu_Serif
 *                            color=FF0000 fondo=FFFF00 trama=6 ctrama=000000 alin=izq|centro|der
 *                            valin=arriba|centro|abajo ajustar borde=1 (todos) bizq= barr= bder= bab=
 *   ancho C 120 | alto 3 40 | combina A1:C1 | comenta A1 TEXTO | inmoviliza A2
 *   grafico columnas|barras|lineas|sectores|dispersion A1:C5 [filas] [titulo=...] [en=E2]
 *   condicional A1:A9 mayor|menor|entre|igual 10 [20] simbolo=1 negrita fondo=FFC0C0
 *   area A1:D20 | salto 20 | horizontal
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "archivo.h"
#include "operar.h"
#include "deshacer.h"
#include "calculo.h"
#include "formula.h"
#include "numeros.h"
#include "cuentas.h"
#include "grafico.h"

static int bien, mal;

static long strtol_16(const char *s)
{
	long v = 0;

	while ((*s >= '0' && *s <= '9') || (*s >= 'a' && *s <= 'f') || (*s >= 'A' && *s <= 'F')) {
		v = v * 16 + (*s <= '9' ? *s - '0' : (*s | 0x20) - 'a' + 10);
		s++;
	}
	return v;
}
static u32 linea;

static void falla(const char *f, const char *a, const char *b)
{
	mal++;
	dice("mal: línea %u: %s: «%s» (se esperaba «%s»)", linea, f, a, b);
}

static int ref(const char *s, u32 *f, u32 *c, const char **fin)
{
	return cu_lee_ref(s, f, c, fin);
}

int cuentas_guion(const char *ruta)
{
	const char *e;
	u32 n, i = 0;
	u8 *d = so_lee_archivo(ruta, &n, 16u * 1024 * 1024, &e);
	libro *l;
	hoja *h;

	if (!d) {
		dice("cuentas: guion %s: %s", ruta, e);
		return 1;
	}
	l = lib_nuevo(1);
	if (!l) {
		so_suelta(d);
		return 1;
	}
	h = l->h[0];
	bien = mal = 0;
	linea = 0;
	while (i < n) {
		char t[4096], o[32];
		u32 k = 0;
		const char *p, *q;
		u32 f, c;

		while (i < n && d[i] != '\n') {
			if (k + 1 < sizeof t && d[i] != '\r')
				t[k++] = (char)d[i];
			i++;
		}
		i++;
		t[k] = 0;
		linea++;
		if (!t[0] || t[0] == '#')
			continue;
		p = t;
		k = 0;
		while (*p && *p != ' ' && k + 1 < sizeof o)
			o[k++] = *p++;
		o[k] = 0;
		if (*p == ' ')
			p++;
		if (iguales(o, "hoja")) {
			hoja *x = lib_hoja_nombre(l, p, largo(p));

			if (!x)
				x = lib_hoja_nueva(l, p, -1);
			if (x) {
				h = x;
				l->activa = (u32)lib_hoja_indice(l, x->id);
			}
			continue;
		}
		if (iguales(o, "loc")) {
			l->loc = iguales(p, "en") ? CU_ARCHIVO : CU_ES;
			continue;
		}
		if (iguales(o, "circ")) {
			if (l->circular)
				bien++;
			else
				falla("circ", "no hay", "una referencia circular");
			continue;
		}
		if (iguales(o, "deshaz") || iguales(o, "rehaz")) {
			int r = iguales(o, "deshaz") ? dh_deshace(l) : dh_rehace(l);

			if (r)
				bien++;
			else
				falla(o, "no había nada", "un paso");
			h = lib_hoja(l, l->activa);
			continue;
		}
		if (iguales(o, "celdas")) {
			char a[32], b[32];

			fmt(a, sizeof a, "%u", h->n_cel);
			copia(b, p, sizeof b);
			if (iguales(a, b))
				bien++;
			else
				falla("celdas", a, b);
			continue;
		}
		if (iguales(o, "insfila") || iguales(o, "borfila") || iguales(o, "inscol") || iguales(o, "borcol")) {
			u32 desde, cuantas = 1;
			const char *fin;
			int eje = o[3] == 'f' ? 0 : 1;

			if (eje == 0)
				desde = (u32)numero(p, &fin) - 1;
			else {
				char x[16];

				fmt(x, sizeof x, "%s1", p);
				if (!ref(x, &f, &desde, &fin)) {
					falla(o, p, "una columna");
					continue;
				}
				while (*fin && *fin != ' ')
					fin++;
				fin = strchr(p, ' ') ? strchr(p, ' ') : "";
			}
			if (*fin == ' ')
				cuantas = (u32)numero(fin + 1, NULL);
			if (o[0] == 'i')
				op_inserta(l, h, eje, desde, cuantas);
			else
				op_elimina(l, h, eje, desde, cuantas);
			continue;
		}
		if (iguales(o, "copia") || iguales(o, "corta")) {
			rango r;
			char *x;

			if (!cu_lee_rango(p, &r, &q)) {
				falla(o, p, "un rango");
				continue;
			}
			x = op_copia(l, h, r, o[1] == 'o' && o[2] == 'r', NULL);
			so_suelta(x);
			continue;
		}
		if (iguales(o, "pega")) {
			char err[200];
			int modo = PG_TODO, tr = 0;

			if (!ref(p, &f, &c, &q)) {
				falla(o, p, "una celda");
				continue;
			}
			if (busca(q, "valores"))
				modo = PG_VALORES;
			if (busca(q, "formatos"))
				modo = PG_FORMATOS;
			if (busca(q, "formulas"))
				modo = PG_FORMULAS;
			if (busca(q, "transp"))
				tr = 1;
			if (op_pega(l, h, f, c, modo, tr, NULL, err, sizeof err))
				falla("pega", err, "pegado");
			continue;
		}
		if (iguales(o, "rellena")) {
			rango a, b;

			if (!cu_lee_rango(p, &a, &q) || *q != ' ' || !cu_lee_rango(q + 1, &b, &q)) {
				falla(o, p, "dos rangos");
				continue;
			}
			op_rellena(l, h, a, b);
			continue;
		}
		if (iguales(o, "ordena")) {
			rango r;
			int col[3] = { -1, -1, -1 }, desc[3] = { 0, 0, 0 };
			char x[16];
			u32 ff, cc;
			const char *fin;

			if (!cu_lee_rango(p, &r, &q) || *q != ' ') {
				falla(o, p, "un rango y una columna");
				continue;
			}
			k = 0;
			q++;
			while (*q && *q != ' ' && k < 8)
				x[k++] = *q++;
			x[k] = 0;
			anade(x, "1", sizeof x);
			if (ref(x, &ff, &cc, &fin))
				col[0] = (int)cc;
			desc[0] = busca(q, "desc") != NULL;
			op_ordena(l, h, r, col, desc, busca(q, "titulos") != NULL);
			continue;
		}
		if (iguales(o, "nombre")) {
			char nom[64], err[200];
			const char *sp = strchr(p, ' ');

			if (!sp) {
				falla(o, p, "un nombre y su referencia");
				continue;
			}
			copia(nom, p, (u32)(sp - p) + 1 < sizeof nom ? (u32)(sp - p) + 1 : sizeof nom);
			if (op_nombre(l, h, nom, sp + 1, err, sizeof err))
				falla("nombre", err, sp + 1);
			continue;
		}
		if (iguales(o, "guarda")) {
			char err[300], real[600];

			if (arch_guarda(l, p, arch_formato_ext(p) ? arch_formato_ext(p) : FMT_HOJ, real, sizeof real, err, sizeof err))
				falla("guarda", err, p);
			else
				bien++;
			continue;
		}
		if (iguales(o, "abre")) {
			char err[300];
			int fm;
			libro *nl = arch_abre(p, &fm, err, sizeof err);

			if (!nl) {
				falla("abre", err, p);
				continue;
			}
			dh_suelta(l);
			lib_suelta(l);
			l = nl;
			h = lib_hoja(l, l->activa);
			if (!h)
				h = l->h[0];
			bien++;
			continue;
		}
		if (iguales(o, "estilo")) {
			rango r;
			u32 ff, cc;
			char pal[64];

			if (!cu_lee_rango(p, &r, &q)) {
				falla(o, p, "un rango");
				continue;
			}
			for (ff = r.f0; ff <= r.f1; ff++)
				for (cc = r.c0; cc <= r.c1; cc++) {
					estilo s = *lib_estilo(l, hj_estilo(h, ff, cc));
					const char *z = q;

					while (*z) {
						u32 kk = 0;
						const char *v;

						while (*z == ' ')
							z++;
						while (*z && *z != ' ' && kk + 1 < sizeof pal)
							pal[kk++] = *z++;
						pal[kk] = 0;
						if (!kk)
							break;
						v = strchr(pal, '=');
						v = v ? v + 1 : "";
						if (iguales(pal, "negrita"))
							s.negrita = 1;
						else if (iguales(pal, "cursiva"))
							s.cursiva = 1;
						else if (iguales(pal, "subrayado"))
							s.subrayado = 1;
						else if (iguales(pal, "tachado"))
							s.tachado = 1;
						else if (iguales(pal, "ajustar"))
							s.ajustar = 1;
						else if (empieza(pal, "tam="))
							s.tam = (u16)(numero(v, NULL) * 2);
						else if (empieza(pal, "letra=")) {
							u32 j;

							copia(s.familia, v, sizeof s.familia);
							for (j = 0; s.familia[j]; j++)
								if (s.familia[j] == '_')
									s.familia[j] = ' ';
						} else if (empieza(pal, "color="))
							s.color = (u32)strtol_16(v);
						else if (empieza(pal, "fondo=")) {
							s.fondo = (u32)strtol_16(v);
							if (s.trama == TR_NADA)
								s.trama = TR_SOLIDO;
						} else if (empieza(pal, "trama="))
							s.trama = (u8)numero(v, NULL);
						else if (empieza(pal, "ctrama="))
							s.color_trama = (u32)strtol_16(v);
						else if (empieza(pal, "alin="))
							s.alin_h = iguales(v, "izq") ? AH_IZQ : iguales(v, "centro") ? AH_CENTRO : iguales(v, "der") ? AH_DER : AH_GENERAL;
						else if (empieza(pal, "valin="))
							s.alin_v = iguales(v, "arriba") ? AV_ARRIBA : iguales(v, "centro") ? AV_CENTRO : AV_ABAJO;
						else if (empieza(pal, "borde=")) {
							int j;

							for (j = 0; j < 4; j++)
								s.borde[j] = (u8)numero(v, NULL);
						} else if (empieza(pal, "bizq="))
							s.borde[BO_IZQ] = (u8)numero(v, NULL);
						else if (empieza(pal, "barr="))
							s.borde[BO_ARRIBA] = (u8)numero(v, NULL);
						else if (empieza(pal, "bder="))
							s.borde[BO_DER] = (u8)numero(v, NULL);
						else if (empieza(pal, "bab="))
							s.borde[BO_ABAJO] = (u8)numero(v, NULL);
					}
					lib_pon_estilo(l, h, ff, cc, lib_estilo_id(l, &s));
				}
			continue;
		}
		if (iguales(o, "ancho") || iguales(o, "alto")) {
			const char *sp = strchr(p, ' ');
			u32 v = sp ? (u32)numero(sp + 1, NULL) : 0;

			if (o[1] == 'n') {
				char x[16];
				const char *fin;

				copia(x, p, sp && (u32)(sp - p) + 1 < 8 ? (u32)(sp - p) + 1 : 8);
				anade(x, "1", sizeof x);
				if (ref(x, &f, &c, &fin))
					op_ancho(l, h, c, c, v);
			} else
				op_alto(l, h, (u32)numero(p, NULL) - 1, (u32)numero(p, NULL) - 1, v);
			continue;
		}
		if (iguales(o, "combina")) {
			rango r;

			if (cu_lee_rango(p, &r, &q))
				op_combina(l, h, r, 1);
			continue;
		}
		if (iguales(o, "inmoviliza")) {
			if (ref(p, &f, &c, &q))
				op_inmoviliza(l, h, f, c);
			continue;
		}
		if (iguales(o, "area")) {
			rango r;

			if (cu_lee_rango(p, &r, &q))
				op_area(l, h, &r);
			continue;
		}
		if (iguales(o, "salto")) {
			op_salto(l, h, 0, (u32)numero(p, NULL) - 1, 1);
			continue;
		}
		if (iguales(o, "horizontal")) {
			pagina pg = h->pag;

			pg.horizontal = 1;
			op_pagina(l, h, &pg);
			continue;
		}
		if (iguales(o, "grafico")) {
			static const char *const tipos[] = { "columnas", "barras", "lineas", "sectores", "dispersion" };
			char tp[32];
			rango r;
			grafico g;
			int t = 0;
			u32 kk = 0;
			const char *z;

			while (*p && *p != ' ' && kk + 1 < sizeof tp)
				tp[kk++] = *p++;
			tp[kk] = 0;
			while (*p == ' ')
				p++;
			for (kk = 0; kk < 5; kk++)
				if (iguales(tp, tipos[kk]))
					t = (int)kk;
			if (!cu_lee_rango(p, &r, &q)) {
				falla(o, p, "un rango");
				continue;
			}
			gr_nuevo(&g, h, r, t);
			if (busca(q, " filas"))
				g.en_filas = 1;
			z = busca(q, "titulo=");
			if (z) {
				u32 j = 0;

				z += 7;
				while (z[j] && z[j] != '|' && j + 1 < sizeof g.titulo) {
					g.titulo[j] = z[j];
					j++;
				}
				g.titulo[j] = 0;
			}
			z = busca(q, "en=");
			if (z && ref(z + 3, &f, &c, NULL)) {
				g.f = f;
				g.c = (u16)c;
			}
			op_grafico(l, h, -1, &g);
			continue;
		}
		if (iguales(o, "condicional")) {
			static const char *const conds[] = { "mayor", "menor", "entre", "igual", "distinto", "mayorigual", "menorigual", "texto" };
			condicional cd;
			rango r;
			char w[32];
			u32 kk = 0;
			const char *z;

			if (!cu_lee_rango(p, &r, &q) || *q != ' ') {
				falla(o, p, "un rango y una condición");
				continue;
			}
			memset(&cd, 0, sizeof cd);
			cd.r = r;
			cd.color = CU_AUTO;
			cd.fondo = CU_AUTO;
			cd.trama = TR_SOLIDO;
			q++;
			while (*q && *q != ' ' && kk + 1 < sizeof w)
				w[kk++] = *q++;
			w[kk] = 0;
			for (kk = 0; kk < 8; kk++)
				if (iguales(w, conds[kk]))
					cd.cond = (u8)kk;
			while (*q == ' ')
				q++;
			cd.v1 = num_de_texto_c(q);
			if (cd.cond == FC_TEXTO) {
				u32 j = 0;

				while (q[j] && q[j] != ' ' && j + 1 < sizeof cd.texto) {
					cd.texto[j] = q[j];
					j++;
				}
				cd.texto[j] = 0;
			}
			z = strchr(q, ' ');
			if (z && cd.cond == FC_ENTRE)
				cd.v2 = num_de_texto_c(z + 1);
			z = busca(q, "simbolo=");
			if (z)
				cd.simbolo = (u8)numero(z + 8, NULL);
			if (busca(q, "negrita"))
				cd.negrita = 1;
			z = busca(q, "fondo=");
			if (z)
				cd.fondo = (u32)strtol_16(z + 6);
			z = busca(q, "color=");
			if (z)
				cd.color = (u32)strtol_16(z + 6);
			op_condicional(l, h, &cd);
			continue;
		}
		if (iguales(o, "fmt")) {
			rango r;
			u32 ff, cc;

			if (!cu_lee_rango(p, &r, &q) || *q != ' ') {
				falla(o, p, "un rango y un código");
				continue;
			}
			for (ff = r.f0; ff <= r.f1; ff++)
				for (cc = r.c0; cc <= r.c1; cc++) {
					estilo s = *lib_estilo(l, hj_estilo(h, ff, cc));

					s.numfmt = lib_fmt_id(l, q + 1);
					lib_pon_estilo(l, h, ff, cc, lib_estilo_id(l, &s));
				}
			continue;
		}
		/* las de una celda */
		if (!ref(p, &f, &c, &q)) {
			falla(o, p, "una celda");
			continue;
		}
		if (*q == ' ')
			q++;
		if (iguales(o, "comenta")) {
			op_comentario(l, h, f, c, q);
			continue;
		}
		if (iguales(o, "pon")) {
			char err[300];

			if (op_entrada(l, h, f, c, q, err, sizeof err))
				falla("pon", err, q);
			continue;
		}
		{
			const valor *v = lib_valor(h, f, c);
			char a[1200];

			a[0] = 0;
			if (v->t == V_NUM)
				num_a_texto_c(v->n, a, sizeof a);
			else if (v->t == V_TXT)
				copia(a, v->s ? v->s : "", sizeof a);
			else if (v->t == V_LOG)
				copia(a, v->n != 0 ? "VERDADERO" : "FALSO", sizeof a);
			else if (v->t == V_ERR)
				copia(a, cu_error_texto(v->e, 0), sizeof a);
			else
				copia(a, "(vacía)", sizeof a);
			if (iguales(o, "num")) {
				double x = num_de_texto_c(q);

				if (v->t == V_NUM && (so_abs(v->n - x) <= 1e-9 * (so_abs(x) > 1 ? so_abs(x) : 1)))
					bien++;
				else
					falla(p, a, q);
			} else if (iguales(o, "txt")) {
				if (v->t == V_TXT && iguales(v->s ? v->s : "", q))
					bien++;
				else
					falla(p, a, q);
			} else if (iguales(o, "log")) {
				if (v->t == V_LOG && (v->n != 0) == (q[0] == '1' || q[0] == 'V'))
					bien++;
				else
					falla(p, a, q);
			} else if (iguales(o, "err")) {
				if (v->t == V_ERR && iguales(cu_error_texto(v->e, 0), q))
					bien++;
				else
					falla(p, a, q);
			} else if (iguales(o, "vacia")) {
				if (v->t == V_VACIO)
					bien++;
				else
					falla(p, a, "(vacía)");
			} else if (iguales(o, "ver")) {
				const celda *x = hj_celda(h, f, c);
				char t2[1200];

				t2[0] = 0;
				if (x)
					lib_texto_visible(l, x, t2, sizeof t2, NULL);
				if (iguales(t2, q))
					bien++;
				else
					falla(p, t2, q);
			} else if (iguales(o, "form")) {
				char t2[1200];

				lib_texto_edicion(l, h, f, c, t2, sizeof t2);
				if (iguales(t2, q))
					bien++;
				else
					falla(p, t2, q);
			} else {
				mal++;
				dice("mal: línea %u: no se entiende «%s»", linea, o);
			}
		}
	}
	so_suelta(d);
	dh_suelta(l);
	lib_suelta(l);
	dice("cuentas: guion %s == %d bien, %d mal", sin_carpeta(ruta), bien, mal);
	return mal ? 1 : 0;
}
