/* principal.c - el Reproductor: por dónde empieza. En el escritorio (dos
 * clics en una canción desde Mi equipo, el menú Inicio), la ventana
 * (vista.c); en la Consola (la orden REPRODUCE), lo sencillo de aquí:
 *
 *   REPRODUCE [archivo|carpeta|lista.m3u|D:] ... [/ALEATORIO] [/REPITE]
 *             [/VOLUMEN n] [/EQ nombre] [/VELOCIDAD 1,5] [/FUNDIDO s]
 *             [/VENTANA] [/MIDE]
 *
 *   Espacio pausa; N o flecha abajo, la siguiente; P o flecha arriba, la
 *   anterior; las flechas izquierda y derecha, 10 segundos atrás o adelante;
 *   + y -, el volumen; Q o Esc, se acaba.
 *
 * /MIDE decodifica sin sonar (lo más deprisa que puede) y dice lo que cuesta
 * cada segundo de sonido: para saber cómo irá en un equipo lento.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "motor.h"
#include "util.h"
#include "vista.h"

static char args[1100];

/* el siguiente trozo de los argumentos (entre comillas si lleva espacios) */
static const char *trozo(const char *p, char *d, int cabe)
{
	int k = 0;

	while (*p == ' ')
		p++;
	if (*p == '"') {
		p++;
		while (*p && *p != '"' && k < cabe - 1)
			d[k++] = *p++;
		if (*p == '"')
			p++;
	} else
		while (*p && *p != ' ' && k < cabe - 1)
			d[k++] = *p++;
	d[k] = 0;
	return p;
}

/* la ruta siguiente de los argumentos: entre comillas; o, sin ellas, los
 * trozos que hagan falta para que exista (los nombres largos con espacios:
 * «\MUSICA\Mi canción.mp3» llega sin comillas desde Mi equipo) */
const char *siguiente_ruta(const char *p, char *a, int cabe)
{
	const char *q;
	char t[MAX_RUTA], j[MAX_RUTA];

	p = trozo(p, a, cabe);
	if (!a[0] || es_opcion(a) || so_existe(a, NULL))
		return p;
	copia(j, sizeof j, a);
	for (q = p; *q;) {
		q = trozo(q, t, sizeof t);
		if (!t[0] || es_opcion(t) || (int)(strlen(j) + strlen(t) + 2) > (int)sizeof j)
			break;
		anade(j, sizeof j, " ");
		anade(j, sizeof j, t);
		if (so_existe(j, NULL)) {
			copia(a, cabe, j);
			return q;
		}
	}
	return p;	/* (no existe ni juntando: el primer trozo, tal cual) */
}

int es_opcion(const char *a)
{
	return a[0] == '/' && (so_separador() != '/' || !strchr(a + 1, '/'));
}

/* añade lo que diga el argumento (un archivo, una carpeta, una lista, un CD) */
int anade_argumento(lista *l, const char *a)
{
	int carpeta = 0;

	if (((a[0] >= 'A' && a[0] <= 'Z') || (a[0] >= 'a' && a[0] <= 'z')) && a[1] == ':' && !a[2]) {
		/* una unidad: si es un CD de música, sus pistas */
		so_toc t;

		if (!so_cd_toc((char)mayus(a[0]), &t) && t.pistas)
			return lista_anade_cd(l, (char)mayus(a[0]), &t);
	}
	if (!so_existe(a, &carpeta)) {
		char t[400];

		fmt(t, sizeof t, "REPRODUCE: no está «%s».\n", a);
		so_dice(t);
		return -1;
	}
	return lista_anade_archivo(l, a);
}

static int mide(lista *l)
{
	int i;
	char t[300];

	for (i = 0; i < l->n; i++) {
		dec d;
		static float b[4096 * 2];
		u32 t0, ms, c0;
		u64 n = 0, ciclos = 0;
		int k, j;

		if (dec_abre(&d, &l->c[i].o))
			continue;
		for (j = 0; j < 1000 && !dec_paso(&d); j++)
			;
		if (!d.listo) {
			dec_cierra(&d);
			continue;
		}
		t0 = so_ms();
		for (;;) {
			c0 = so_ciclos();
			k = dec_lee(&d, b, 4096);
			ciclos += (u32)(so_ciclos() - c0);
			if (k <= 0)
				break;
			n += (u64)k;
		}
		ms = so_ms() - t0;
		fmt(t, sizeof t, "MIDE: %s: %s, %u Hz: %u s de sonido en %u ms (%u,%u %% del procesador); %u M ciclos\n", l->c[i].titulo,
		    d.nombre_formato, d.hz, (u32)(n / (d.hz ? d.hz : 1)), ms,
		    n ? (u32)((u64)ms * d.hz / n / 10) : 0, n ? (u32)((u64)ms * d.hz * 10 / n / 10 % 10) : 0, (u32)(ciclos / 1000000));
		so_dice(t);
		reg("%s", t);
		dec_cierra(&d);
	}
	return 0;
}

/* (para las pruebas) la tabla de las pistas del lector y la suma (CRC-32) de
 * lo que trae cada sector en crudo (READ CD, la llamada 134 con ECX = 2): en
 * un CD de datos, los 2048 octetos de dentro (desde el 16) tienen que ser
 * los de la imagen */
static u32 crc32(const u8 *d, u32 n)
{
	u32 c = 0xFFFFFFFFu, i;
	int k;

	for (i = 0; i < n; i++) {
		c ^= d[i];
		for (k = 0; k < 8; k++)
			c = c & 1 ? (c >> 1) ^ 0xEDB88320u : c >> 1;
	}
	return ~c;
}

int so_cd_lee_crudo(char letra, u32 lba, u32 n, void *d);

static int prueba_cd(char letra, u32 lba, u32 n)
{
	so_toc t;
	char s[200];
	static u8 b[2352 * 32];
	u32 i;

	if (so_cd_toc(letra, &t)) {
		so_dice("PRUEBACD: no se puede leer la tabla de las pistas.\n");
		return 1;
	}
	for (i = 0; i <= (u32)t.pistas; i++) {
		fmt(s, sizeof s, "PRUEBACD: pista %u: lba %u, control %u\n", t.pista[i].numero, t.pista[i].lba, t.pista[i].control);
		so_dice(s);
	}
	if (n > 32)
		n = 32;
	if (so_cd_lee_crudo(letra, lba, n, b)) {
		so_dice("PRUEBACD: READ CD falla.\n");
		return 1;
	}
	for (i = 0; i < n; i++) {
		fmt(s, sizeof s, "PRUEBACD: sector %u: crc %08x (de dentro) sincronía %02x%02x%02x\n", lba + i, crc32(b + i * 2352 + 16, 2048),
		    b[i * 2352], b[i * 2352 + 1], b[i * 2352 + 11]);
		so_dice(s);
	}
	return 0;
}

static void linea_estado(motor *m, char *t, int cabe)
{
	char a[16], b[16], nom[200];
	u32 p = motor_pos_ms(m) / 1000, tot = motor_total_ms(m) / 1000;

	a_tiempo(p, a, sizeof a);
	a_tiempo(tot, b, sizeof b);
	if (m->oida >= 0 && m->oida < m->l->n)
		lista_nombre(&m->l->c[m->oida], nom, sizeof nom);
	else
		copia(nom, sizeof nom, "");
	if (strlen(nom) > 46) {
		nom[44] = 0;
		anade(nom, sizeof nom, "…");
	}
	fmt(t, cabe, "\r%s %s / %s  %s  %d %%   ", m->estado == M_PAUSA ? "[pausa] " : "", a, tot ? b : "--:--", nom, m->volumen);
}

static int consola(lista *l, int volumen, const char *eq, float velocidad, int fundido)
{
	motor m;
	char t[300], antes[300] = "";
	int oida = -2, i;
	u32 t0, p0;
	u64 ocupado = 0, total = 0;

	if (motor_prepara(&m, l)) {
		so_dice("REPRODUCE: no hay memoria.\n");
		return 1;
	}
	if (m.sin_tarjeta)
		so_dice("REPRODUCE: no hay tarjeta de sonido (o no se ha podido poner en marcha).\n");
	motor_volumen(&m, volumen);
	if (velocidad != 1.0f)
		motor_velocidad(&m, velocidad);
	m.fundido = fundido;
	if (eq && eq[0]) {
		for (i = 0; i < n_preajustes; i++)
			if (compara_llano(preajustes[i].nombre, eq) == 0) {
				int b;

				m.eq.activo = 1;
				m.eq.preamp = preajustes[i].v[0] * 10;
				for (b = 0; b < BANDAS; b++)
					m.eq.banda[b] = preajustes[i].v[b + 1] * 10;
				eq_cambia(&m.eq);
			}
		if (!m.eq.activo)
			so_dice("REPRODUCE: no hay tal ecualización (Rock, Pop, Clásica, Jazz, Voz, Graves...).\n");
	}
	motor_toca(&m, l->aleatorio ? (int)(azar() % (u32)l->n) : 0);
	t0 = so_ms();
	p0 = so_ms_propios();
	while (m.estado != M_PARADO) {
		int k = so_tecla();
		u32 c_vuelta = so_ciclos();

		if (k == ' ')
			motor_pausa(&m);
		else if (k == 'n' || k == 'N' || k == T_ABAJO)
			motor_siguiente(&m);
		else if (k == 'p' || k == 'P' || k == T_ARRIBA)
			motor_anterior(&m);
		else if (k == T_DERECHA)
			motor_busca_ms(&m, motor_pos_ms(&m) + 10000);
		else if (k == T_IZQUIERDA)
			motor_busca_ms(&m, motor_pos_ms(&m) > 10000 ? motor_pos_ms(&m) - 10000 : 0);
		else if (k == '+')
			motor_volumen(&m, m.volumen + 5);
		else if (k == '-')
			motor_volumen(&m, m.volumen - 5);
		else if (k == 'q' || k == 'Q' || k == 27 || k == 3)
			break;
		motor_ciclo(&m, 40);
		if (m.oida != oida && m.oida >= 0) {
			oida = m.oida;
			if (antes[0])
				so_dice("\n");
			reg("suena: %s", l->c[oida].titulo);
		}
		linea_estado(&m, t, sizeof t);
		if (strcmp(t, antes)) {
			so_dice(t);
			copia(antes, sizeof antes, t);
		}
		if (m.error[0]) {
			fmt(t, sizeof t, "\nREPRODUCE: %s\n", m.error);
			so_dice(t);
			reg("%s", m.error);
			m.error[0] = 0;
		}
		ocupado += (u64)(u32)(so_ciclos() - c_vuelta);
		so_duerme(motor_cola_ms(&m) > 150 ? 30 : 10);
		total += (u64)(u32)(so_ciclos() - c_vuelta);
	}
	so_dice("\n");
	if (total)
		reg("ha gastado el %u,%u %% del procesador sonando (%u ms)", (u32)(ocupado * 100 / total), (u32)(ocupado * 1000 / total % 10), so_ms() - t0);
	(void)p0;
	/* lo que quede en la cola, que suene (como mucho dos segundos; no si se
	 * ha pedido salir) */
	for (i = 0; i < 200 && motor_cola_ms(&m) > 0 && m.estado == M_PARADO; i++)
		so_duerme(10);
	motor_cierra(&m);
	return 0;
}

int principal(void)
{
	static lista l;
	char a[MAX_RUTA], eq[32] = "";
	const char *p;
	int volumen = 80, ventana = 0, medir = 0, fundido = 0;
	float velocidad = 1;

	so_argumentos(args, sizeof args);
	siembra(so_ciclos() ^ so_ms());
	lista_nueva(&l);
	/* en el escritorio y sin terminal: la ventana */
	if (!so_en_consola())
		ventana = 1;
	for (p = args;;) {
		const char *antes = p;

		p = trozo(p, a, sizeof a);
		if (!a[0])
			break;
		if (!es_opcion(a))
			p = siguiente_ruta(antes, a, sizeof a);
		if (es_opcion(a)) {
			if (igual_sin_caja(a, "/ALEATORIO"))
				l.aleatorio = 1;
			else if (igual_sin_caja(a, "/REPITE"))
				l.repetir = REPITE_TODO;
			else if (igual_sin_caja(a, "/VENTANA"))
				ventana = 1;
			else if (igual_sin_caja(a, "/CONSOLA"))
				ventana = 0;
			else if (igual_sin_caja(a, "/MIDE"))
				medir = 1;
			else if (igual_sin_caja(a, "/VOLUMEN")) {
				p = trozo(p, a, sizeof a);
				volumen = a_entero(a);
			} else if (igual_sin_caja(a, "/EQ")) {
				p = trozo(p, a, sizeof a);
				so_a_utf8(a, eq, sizeof eq);
			} else if (igual_sin_caja(a, "/VELOCIDAD")) {
				p = trozo(p, a, sizeof a);
				velocidad = (float)decimas_de(a) / 10.0f;
			} else if (igual_sin_caja(a, "/PRUEBACD")) {
				char l[8], b2[16], c2[16];

				p = trozo(p, l, sizeof l);
				p = trozo(p, b2, sizeof b2);
				p = trozo(p, c2, sizeof c2);
				return prueba_cd((char)mayus(l[0]), (u32)a_entero(b2), (u32)a_entero(c2));
			} else if (igual_sin_caja(a, "/FUNDIDO")) {
				p = trozo(p, a, sizeof a);
				fundido = a_entero(a);
			} else if (igual_sin_caja(a, "/?") || igual_sin_caja(a, "/AYUDA")) {
				so_dice("REPRODUCE [archivo|carpeta|lista|D:] ... [/ALEATORIO] [/REPITE] [/VOLUMEN n] [/EQ Rock]\n"
					"          [/VELOCIDAD 1,5] [/FUNDIDO s] [/VENTANA] [/MIDE]\n"
					"Suenan MP3, MP2, OGG, FLAC, WAV, MOD, XM, S3M, IT y los CD de música (y sus imágenes .CUE).\n"
					"Espacio: pausa. N y P: siguiente y anterior. Flechas: 10 s. + y -: volumen. Q: salir.\n");
				return 0;
			}
			continue;
		}
		if (!ventana || medir)
			anade_argumento(&l, a);
		else {
			/* (para la ventana: se le pasan tal cual) */
		}
	}
	if (ventana && !medir)
		return vista_principal(args);
	while (lista_paso(&l))
		;
	if (!l.n) {
		so_dice("REPRODUCE: no hay nada que tocar. REPRODUCE /? dice cómo se usa.\n");
		return 1;
	}
	if (medir)
		return mide(&l);
	return consola(&l, volumen, eq, velocidad, fundido);
}
