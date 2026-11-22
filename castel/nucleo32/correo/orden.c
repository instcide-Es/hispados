/* orden.c - Correo: las órdenes (CORREO /ORDEN ... en la Consola de
 * Hispa-Pentium, y lo mismo en Linux para las pruebas), sin ventana:
 *
 *   cuenta CORREO CLAVE [ENTRADA PUERTO MODO SALIDA PUERTO MODO] [/POP3] [/SINCIFRAR]
 *        (sin los servidores, los busca: autocfg.c). MODO: tls, starttls, sin-cifrar
 *   autocfg CORREO          lo que encuentra la autoconfiguración
 *   cuentas                 las cuentas
 *   carpetas [N]            las carpetas de la cuenta N (desde 1), con sus contadores
 *   al-dia [N] [CARPETA]    pone al día una carpeta (la entrada) y dice cuánto ha tardado
 *   lista [N] [CARPETA]     las fichas (lo guardado; sin red)
 *   lee UID [N] [CARPETA]   el mensaje: cabeceras, texto y adjuntos (y lo marca leído)
 *   envia PARA ASUNTO TEXTO [ADJUNTO...]   (desde la cuenta 1)
 *   busca TEXTO [N] [CARPETA]   en el servidor (y, sin red, en lo bajado)
 *   mueve UID CARPETA [N] [DE]  /  marca UID leido|noleido|destacado [N]
 *   imap-respuestas ARCHIVO     (las pruebas) el analizador con lo que diría un servidor
 *   autocfg-xml ARCHIVO CORREO  (las pruebas) un XML de autoconfiguración
 * La contraseña de «cuenta» se guarda (es para las pruebas: así lo hace la
 * ventana si se marca «Recordar la contraseña»).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include <stdio.h>
#include "motor.h"
#include "html.h"

static void di(const char *f, ...)
{
	char t[1200];
	va_list a;

	va_start(a, f);
	vsnprintf(t, sizeof t, f, a);
	va_end(a);
	so_dice(t);
}

static int trocea(char *t, char **v, int max)
{
	int n = 0;

	while (*t && n < max) {
		while (*t == ' ')
			t++;
		if (!*t)
			break;
		if (*t == '"') {
			v[n++] = ++t;
			while (*t && *t != '"')
				t++;
		} else {
			v[n++] = t;
			while (*t && *t != ' ')
				t++;
		}
		if (*t)
			*t++ = 0;
	}
	return n;
}

static u32 num(const char *t)
{
	u32 v = 0;

	while (t && *t >= '0' && *t <= '9')
		v = v * 10 + (u32)(*t++ - '0');
	return v;
}

static int modo(const char *t, int starttls)
{
	if (!strcmp(t, "starttls"))
		return starttls;
	if (!strcmp(t, "sin-cifrar"))
		return SO_PLANO;
	return SO_TLS;
}

static void paso(void *u, const char *que)
{
	(void)u;
	di("  %s\n", que);
}

static int busca_carpeta(Sesion *s, const char *nombre)
{
	int i;

	if (!nombre)
		return motor_carpeta_especial(s, CM_ENTRADA);
	for (i = 0; i < s->n_car; i++)
		if (!strcmp(s->car[i].nombre, nombre) || !strcmp(s->car[i].crudo, nombre))
			return i;
	return -1;
}

static int abre_cuenta(Sesion *s, const char *n, int conecta)
{
	u32 k = n ? num(n) : 1;

	if (k < 1 || (int)k > n_cuentas) {
		di("No hay cuenta %u (hay %d).\n", (unsigned)k, n_cuentas);
		return -1;
	}
	motor_abre(s, (int)k - 1);
	if (conecta && s->c->tipo == CT_IMAP) {
		if (motor_conecta(s)) {
			di("Sin conexi\xc3\xb3n: %s\n", s->error);
			return 1;
		}
		di("Conectado con %s:%u (%s)\n", s->c->entrada, (unsigned)s->c->puerto_entrada,
		   s->imap.con.cifrado[0] ? s->imap.con.cifrado : "SIN CIFRAR");
		if (motor_carpetas(s) < 0) {
			di("No se pueden ver las carpetas: %s\n", s->error);
			return 1;
		}
	}
	return 0;
}

static void lista(Sesion *s, u32 max)
{
	u32 i, k = 0;
	char f[40];

	for (i = s->buz.n; i-- > 0 && k < max; k++) {
		Ficha *x = &s->buz.f[i];

		mime_fecha_corta(x->fecha, so_zona(x->fecha), so_ahora(), f, sizeof f);
		di("%6u %c%c%c%c %-16s %-24.24s %s\n", (unsigned)x->uid, x->banderas & B_LEIDO ? ' ' : 'N',
		   x->banderas & B_DESTACADO ? '*' : ' ', x->banderas & B_ADJUNTO ? '@' : ' ', x->banderas & B_BAJADO ? 'b' : ' ',
		   f, x->de, x->asunto);
	}
}

static void muestra(Sesion *s, Ficha *f)
{
	u32 n, i;
	u8 *d = motor_mensaje(s, f, &n);
	Mensaje *m;
	char *v, mot[300];
	int p, k;

	if (!d) {
		di("No se puede leer: %s\n", s->error);
		return;
	}
	m = mime_analiza(d, n);
	if (!m) {
		so_suelta(d);
		return;
	}
	{
		static const char *const cab[] = { "From", "To", "Cc", "Subject", "Date" };

		for (i = 0; i < 5; i++) {
			v = mime_cabecera_texto(m, 0, cab[i]);
			if (v) {
				di("%s: %s\n", cab[i], v);
				so_suelta(v);
			}
		}
	}
	if ((k = mime_suplantacion(m, mot, sizeof mot)) > 0)
		di("AVISO (%s): %s\n", k >= 2 ? "grave" : "aviso", mot);
	p = mime_principal(m, 1);
	if (p >= 0) {
		u32 tn;
		char *t = mime_texto(m, p, &tn);
		Doc *doc = NULL;

		if (t) {
			doc = !strcmp(m->partes[p].tipo, "text/html") ? html_a_doc(t, tn) : texto_a_doc(t, tn);
			so_suelta(t);
		}
		if (doc) {
			u32 ln;
			char *pl = doc_a_texto(doc, &ln);

			di("-- (%s%s%s) --\n", m->partes[p].tipo, doc->habia_scripts ? ", sin scripts" : "", doc->remotas ? ", con im\xc3\xa1genes de fuera sin cargar" : "");
			for (i = 0; i < doc->n_enlaces; i++)
				if (doc->enlaces[i].enganoso)
					di("AVISO: enlace enga\xc3\xb1oso: %s\n", doc->enlaces[i].motivo);
			if (pl)
				so_dice(pl), so_dice("\n");
			so_suelta(pl);
			doc_libera(doc);
		}
	}
	for (k = 0; k < mime_n_adjuntos(m); k++) {
		int a = mime_adjunto(m, k);

		di("Adjunto %d: %s (%s, %u octetos)\n", k + 1, m->partes[a].nombre[0] ? m->partes[a].nombre : "(sin nombre)", m->partes[a].tipo,
		   (unsigned)m->partes[a].tam);
	}
	mime_libera(m);
	so_suelta(d);
	if (!(f->banderas & B_LEIDO)) {
		Ficha *v1[1] = { f };

		motor_marca(s, v1, 1, B_LEIDO, 1);
	}
}

int correo_orden(char *linea)
{
	char *a[16];
	int n = trocea(linea, a, 16), r = 0;
	Sesion *s = so_pide(sizeof(Sesion));

	if (!s)
		return 3;
	if (n == 0) {
		di("Correo %s: las \xc3\xb3rdenes, en orden.c (cuenta, carpetas, al-dia, lista, lee, envia, busca, mueve, marca).\n", CORREO_VERSION);
		so_suelta(s);
		return 0;
	}
	if (!strcmp(a[0], "cuenta") && n >= 3) {
		Cuenta c;
		Autocfg ac;
		int i, pop = 0, plano = 0;

		for (i = 3; i < n; i++) {
			if (!strcmp(a[i], "/POP3"))
				pop = 1;
			if (!strcmp(a[i], "/SINCIFRAR"))
				plano = 1;
		}
		if (n >= 9 && a[3][0] != '/') {
			memset(&c, 0, sizeof c);
			snprintf(c.correo, sizeof c.correo, "%s", a[1]);
			snprintf(c.usuario, sizeof c.usuario, "%s", a[1]);
			snprintf(c.nombre, sizeof c.nombre, "%s", a[1]);
			snprintf(c.entrada, sizeof c.entrada, "%s", a[3]);
			c.puerto_entrada = num(a[4]);
			c.modo_entrada = modo(a[5], pop ? SO_STARTTLS_POP3 : SO_STARTTLS_IMAP);
			snprintf(c.salida, sizeof c.salida, "%s", a[6]);
			c.puerto_salida = num(a[7]);
			c.modo_salida = modo(a[8], SO_STARTTLS_SMTP);
			c.tipo = pop ? CT_POP3 : CT_IMAP;
			c.dejar_en_servidor = 1;
			c.avisar = 1;
		} else {
			autocfg_busca(a[1], &ac, paso, NULL);
			c = ac.c;
			di("Autoconfiguraci\xc3\xb3n: %s\n", ac.encontrada ? ac.de_donde : "no se ha encontrado");
		}
		c.sin_cifrar = plano;
		snprintf(c.clave, sizeof c.clave, "%s", a[2]);
		c.guardar_clave = 1;
		if (cuenta_nueva(&c) < 0 || cuentas_guarda()) {
			di("No se ha podido guardar la cuenta.\n");
			r = 1;
		} else
			di("Cuenta %d: %s (%s %s:%u, SMTP %s:%u)\n", n_cuentas, c.correo, c.tipo == CT_POP3 ? "POP3" : "IMAP", c.entrada,
			   (unsigned)c.puerto_entrada, c.salida, (unsigned)c.puerto_salida);
	} else if (!strcmp(a[0], "autocfg") && n >= 2) {
		Autocfg ac;

		if (autocfg_busca(a[1], &ac, paso, NULL))
			di("%s: entrada %s:%u (%d), salida %s:%u (%d), de %s\n%s\n", a[1], ac.c.entrada, (unsigned)ac.c.puerto_entrada,
			   ac.c.modo_entrada, ac.c.salida, (unsigned)ac.c.puerto_salida, ac.c.modo_salida, ac.de_donde, ac.aviso);
		else {
			di("No se han encontrado los servidores de %s.\n", a[1]);
			r = 1;
		}
	} else if (!strcmp(a[0], "autocfg-xml") && n >= 3) {
		u32 l;
		u8 *d = so_lee_archivo(a[1], &l, 1u << 20);
		Cuenta c;

		if (d && autocfg_xml((char *)d, l, a[2], &c))
			di("entrada %s %s:%u (%d) usuario %s; salida %s:%u (%d)\n", c.tipo == CT_POP3 ? "pop3" : "imap", c.entrada,
			   (unsigned)c.puerto_entrada, c.modo_entrada, c.usuario, c.salida, (unsigned)c.puerto_salida, c.modo_salida);
		else
			r = 1;
		so_suelta(d);
	} else if (!strcmp(a[0], "cuentas")) {
		int i;

		for (i = 0; i < n_cuentas; i++)
			di("%d. %s (%s %s:%u)\n", i + 1, cuentas[i].correo, cuentas[i].tipo == CT_POP3 ? "POP3" : "IMAP", cuentas[i].entrada,
			   (unsigned)cuentas[i].puerto_entrada);
	} else if (!strcmp(a[0], "carpetas")) {
		int i;

		r = abre_cuenta(s, n > 1 ? a[1] : NULL, 1);
		if (r >= 0)
			for (i = 0; i < s->n_car; i++)
				di("%*s%s (%u, %u sin leer)%s%s%s%s\n", s->car[i].nivel * 2, "", s->car[i].nombre, (unsigned)s->car[i].total,
				   (unsigned)s->car[i].no_leidos, s->car[i].marcas & CM_ENVIADOS ? " [enviados]" : "",
				   s->car[i].marcas & CM_BORRADORES ? " [borradores]" : "", s->car[i].marcas & CM_PAPELERA ? " [papelera]" : "",
				   s->car[i].marcas & CM_NOSELECT ? " [sin abrir]" : "");
		motor_cierra(s);
	} else if (!strcmp(a[0], "al-dia") || !strcmp(a[0], "lista") || !strcmp(a[0], "busca") || !strcmp(a[0], "lee") ||
		   !strcmp(a[0], "mueve") || !strcmp(a[0], "marca")) {
		int conecta = strcmp(a[0], "lista") != 0, k, base = 1;
		const char *cuenta = NULL, *car = NULL;

		if (!strcmp(a[0], "busca") || !strcmp(a[0], "lee") || !strcmp(a[0], "mueve") || !strcmp(a[0], "marca"))
			base = !strcmp(a[0], "mueve") || !strcmp(a[0], "marca") ? 3 : 2;
		if (n > base)
			cuenta = a[base];
		if (n > base + 1)
			car = a[base + 1];
		r = abre_cuenta(s, cuenta, conecta);
		if (r < 0)
			goto fin;
		k = busca_carpeta(s, car);
		if (k < 0) {
			di("No hay tal carpeta: %s\n", car ? car : "(la entrada)");
			r = 1;
			motor_cierra(s);
			goto fin;
		}
		{
			u32 t0 = so_tics();
			int x = motor_elige(s, k, conecta && strcmp(a[0], "lee"));

			if (!strcmp(a[0], "al-dia")) {
				if (x)
					di("No se ha podido: %s\n", s->error);
				di("%s: %u mensajes, %u sin leer (%u nuevos) en %u ms\n", s->car[k].nombre, (unsigned)s->car[k].total,
				   (unsigned)s->car[k].no_leidos, (unsigned)s->nuevos, (unsigned)((so_tics() - t0) * 1000 / so_hz()));
			} else if (!strcmp(a[0], "lista"))
				lista(s, 100000);
			else if (!strcmp(a[0], "lee") && n > 1) {
				Ficha *f = buzon_busca(&s->buz, num(a[1]));

				if (!f && conecta && !motor_al_dia(s))
					f = buzon_busca(&s->buz, num(a[1]));
				if (f)
					muestra(s, f);
				else {
					di("No hay mensaje %s.\n", a[1]);
					r = 1;
				}
			} else if (!strcmp(a[0], "busca") && n > 1) {
				Ficha **v;
				int m, i;

				if (!motor_busca(s, a[1], 0, &v, &m)) {
					for (i = 0; i < m; i++)
						di("%6u %s | %s\n", (unsigned)v[i]->uid, v[i]->de, v[i]->asunto);
					di("%d encontrados\n", m);
					so_suelta(v);
				} else
					r = 1;
			} else if (!strcmp(a[0], "mueve") && n > 2) {
				Ficha *f = buzon_busca(&s->buz, num(a[1]));
				int d = busca_carpeta(s, a[2]);

				if (!f || d < 0 || motor_mueve(s, &f, 1, d)) {
					di("No se ha podido mover: %s\n", s->error);
					r = 1;
				} else
					di("Movido a %s.\n", s->car[d].nombre);
			} else if (!strcmp(a[0], "marca") && n > 2) {
				Ficha *f = buzon_busca(&s->buz, num(a[1]));

				if (!f)
					r = 1;
				else if (!strcmp(a[2], "leido"))
					motor_marca(s, &f, 1, B_LEIDO, 1);
				else if (!strcmp(a[2], "noleido"))
					motor_marca(s, &f, 1, B_LEIDO, 0);
				else if (!strcmp(a[2], "destacado"))
					motor_marca(s, &f, 1, B_DESTACADO, 1);
			}
		}
		motor_cierra(s);
	} else if (!strcmp(a[0], "envia") && n >= 4) {
		Redactado red;
		Adjunto adj[8];
		int i, na = 0;
		Direccion d[16];
		int nd;
		char destinos[1200];
		u32 k = 0;

		r = abre_cuenta(s, NULL, 0);
		if (r < 0)
			goto fin;
		memset(&red, 0, sizeof red);
		memset(adj, 0, sizeof adj);
		snprintf(red.de.nombre, sizeof red.de.nombre, "%s", s->c->visible);
		snprintf(red.de.dir, sizeof red.de.dir, "%s", s->c->correo);
		red.para = a[1];
		red.asunto = a[2];
		red.texto = a[3];
		red.programa = CORREO_PROGRAMA;
		red.zona = so_zona(so_ahora());
		for (i = 4; i < n && na < 8; i++) {
			const char *b = strrchr(a[i], so_barra());

			adj[na].ruta = a[i];
			adj[na].nombre = b ? b + 1 : a[i];
			na++;
		}
		red.adjuntos = adj;
		red.n_adjuntos = na;
		nd = mime_direcciones(a[1], (u32)strlen(a[1]), d, 16);
		destinos[0] = 0;
		for (i = 0; i < nd; i++)
			if (mime_dir_valida(d[i].dir))
				k += (u32)snprintf(destinos + k, sizeof destinos - k, "%s%s", k ? "," : "", d[i].dir);
		if (!k) {
			di("No hay ning\xc3\xban destinatario v\xc3\xa1lido.\n");
			r = 1;
		} else {
			int x = motor_envia(s, &red, destinos);

			if (x < 0) {
				di("No se ha enviado: %s\n", s->error);
				r = 1;
			} else
				di(x ? "%s\n" : "Enviado.\n", s->error);
		}
		motor_cierra(s);
	} else if (!strcmp(a[0], "imap-respuestas") && n >= 2) {
		u32 l;
		u8 *d = so_lee_archivo(a[1], &l, 64u << 20);

		if (d) {
			di("%d\n", imap_prueba_respuestas(d, l));
			so_suelta(d);
		} else
			r = 2;
	} else {
		di("No entiendo la orden: %s\n", a[0]);
		r = 2;
	}
fin:
	so_suelta(s);
	return r < 0 ? 1 : r;
}
