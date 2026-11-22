/* motor.c - el Reproductor: el motor (ver motor.h).
 *
 *   decodificador -> ReplayGain -> velocidad (WSOLA) -> remuestrear a la
 *   frecuencia de la tarjeta  ==> (el fundido: dos voces sumadas) ->
 *   ecualizador -> lo oído (para la visualización) -> 16 bits -> la cola
 *   del canal del servicio de sonido (SONIDO.BIN, la llamada 191)
 *
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include <math.h>
#include "motor.h"
#include "util.h"

#define TROZO 1024			/* tramas de salida por vuelta (21 ms a 48 kHz) */
#define OBJETIVO_MS 400			/* lo que se deja en la cola por delante (al empezar) */
#define OBJETIVO_MAX 1200		/* (si se queda corta, crece hasta aquí: otro programa gasta el procesador) */
#define ABRIR_ANTES_S 12		/* la siguiente se empieza a abrir a falta de tanto */

/* ---------------------------------------------------------------- la voz */
static void voz_cierra(voz *v)
{
	if (v->abierta)
		dec_cierra(&v->d);
	rm_suelta(&v->rm);
	es_suelta(&v->es);
	etiquetas_suelta(&v->et);
	memset(v, 0, sizeof *v);
	v->cancion = -1;
}

static float ganancia_rg(motor *m, voz *v)
{
	int c = 0, pico = 0;

	if (m->replaygain == RG_NO)
		return 1;
	if (m->replaygain == RG_DISCO && v->et.hay_rg_album)
		c = v->et.rg_album, pico = v->et.rg_album_pico;
	else if (v->et.hay_rg_pista)
		c = v->et.rg_pista, pico = v->et.rg_pista_pico;
	else if (v->et.hay_rg_album)
		c = v->et.rg_album, pico = v->et.rg_album_pico;
	else
		return 1;
	{
		float g = db_a_veces(c);

		/* que no recorte: como mucho hasta el pico */
		if (pico > 0 && g * (float)pico / 1000.0f > 1.0f)
			g = 1000.0f / (float)pico;
		return g;
	}
}

/* empieza a abrir la canción c en la voz v */
static int voz_empieza(motor *m, voz *v, int c)
{
	voz_cierra(v);
	v->cancion = c;
	if (c < 0 || c >= m->l->n)
		return -1;
	if (dec_abre(&v->d, &m->l->c[c].o)) {
		copia(m->error, sizeof m->error, v->d.error);
		dec_cierra(&v->d);
		v->fallo = 1;
		return -1;
	}
	v->abierta = 1;
	return 0;
}

/* un paso más de abrirla; 1 cuando está lista */
static int voz_paso(motor *m, voz *v)
{
	int r;

	if (v->lista)
		return 1;
	if (!v->abierta || v->fallo)
		return -1;
	r = dec_paso(&v->d);
	if (r < 0) {
		fmt(m->error, sizeof m->error, "%s: %s", m->l->c[v->cancion].titulo, v->d.error);
		v->fallo = 1;
		return -1;
	}
	if (r == 0)
		return 0;
	/* lista: lo de alrededor */
	{
		cancion *c = &m->l->c[v->cancion];
		origen *o = &c->o;

		if (!o->lector && !o->largo)
			etiquetas_lee(o->ruta, &v->et, 0);
		v->ganancia = ganancia_rg(m, v);
		rm_prepara(&v->rm, v->d.hz, m->hz);
		es_prepara(&v->es, v->d.hz);
		es_velocidad(&v->es, m->velocidad);
		if (o->tramo_desde && v->d.buscable)
			dec_busca(&v->d, (u64)o->tramo_desde * v->d.hz / 75);
		if (o->tramo_hasta)
			v->fin_tramo = (u64)o->tramo_hasta * v->d.hz / 75;
		/* la duración, a la lista */
		{
			u64 t = v->d.total;

			if (v->fin_tramo && v->fin_tramo < t + 1)
				t = v->fin_tramo;
			if (o->tramo_desde)
				t = t > (u64)o->tramo_desde * v->d.hz / 75 ? t - (u64)o->tramo_desde * v->d.hz / 75 : 0;
			if (t && v->d.hz)
				c->duracion = (u32)(t / v->d.hz);
		}
		if (!c->leida && (v->et.titulo[0] || v->et.artista[0])) {
			if (v->et.titulo[0])
				copia(c->titulo, sizeof c->titulo, v->et.titulo);
			copia(c->artista, sizeof c->artista, v->et.artista);
			copia(c->album, sizeof c->album, v->et.album);
			c->leida = 1;
			m->l->cambios++;
		}
	}
	v->lista = 1;
	return 1;
}

/* lo que le falta a la voz, en tramas de salida (0 si no se sabe) */
static u64 voz_falta(motor *m, voz *v)
{
	u64 fin = v->fin_tramo ? v->fin_tramo : v->d.total;

	if (!v->lista || !fin || !v->d.hz)
		return 0;
	if (v->d.pos >= fin)
		return 1;
	return (u64)((double)(fin - v->d.pos) * m->hz / v->d.hz / m->velocidad) + 1;
}

/* las tramas de salida que corresponden a lo que ha entrado */
static u64 esperadas(motor *m, voz *v)
{
	return (u64)((double)v->entrada * m->hz / v->d.hz / m->velocidad) + v->heredadas;
}

/* (sin huecos) la de antes se acaba y la siguiente ya está lista, a la
 * misma frecuencia: el remuestreador y el estirador pasan a la siguiente
 * con lo que tenían dentro, y siguen como si fuera una sola canción */
static int pasa_el_filtro(motor *m, voz *v)
{
	voz *b = &m->v[!m->va];
	remuestreo r;
	estira e;
	u64 hay;

	if (v != &m->v[m->va] || !m->sin_huecos || (m->fundido && strcmp(v->d.o.ruta, b->d.o.ruta)) || !b->lista || b->fallo || b->d.hz != v->d.hz ||
	    b->entrada || b->sacadas)
		return 0;
	hay = esperadas(m, v);
	r = b->rm;
	b->rm = v->rm;
	v->rm = r;
	e = b->es;
	b->es = v->es;
	v->es = e;
	b->heredadas = hay > v->sacadas ? hay - v->sacadas : 0;
	return 1;
}

/* hasta «n» tramas de salida de la voz (estéreo, a m->hz); las hechas. Lo
 * que sobra se queda en el remuestreador y en el estirador; al acabarse el
 * decodificador, se saca lo que quede dentro (con ceros detrás) hasta lo
 * que tocaba: así no se pierde nada entre una canción y la siguiente */
static int voz_saca(motor *m, voz *v, float *s, int n)
{
	int hechas = 0, vueltas = 0;

	while (hechas < n && !v->acabada && vueltas++ < 64) {
		int quiero, leidas, e, r;
		u32 c0 = so_ciclos();

		/* lo que haya dentro del remuestreador */
		r = rm_hace(&v->rm, NULL, 0, s + 2 * hechas, n - hechas);
		if (v->vaciando) {
			u64 hay = esperadas(m, v);

			if (v->sacadas + (u64)r > hay)
				r = (int)(hay > v->sacadas ? hay - v->sacadas : 0);
		}
		hechas += r;
		v->sacadas += (u64)r;
		m->ciclos_dsp += so_ciclos() - c0;
		if (hechas >= n)
			break;
		if (v->vaciando) {
			if (v->sacadas >= esperadas(m, v) || r == 0) {
				v->acabada = 1;
				break;
			}
			continue;
		}
		/* lo que hay que leer de la canción para sacar lo que falta */
		quiero = (int)((double)(n - hechas) * v->d.hz / m->hz * m->velocidad) + 1;
		if (quiero > m->tmp_n)
			quiero = m->tmp_n;
		if (quiero < 64)
			quiero = 64;
		if (v->fin_tramo && v->d.pos + (u64)quiero > v->fin_tramo)
			quiero = (int)(v->fin_tramo > v->d.pos ? v->fin_tramo - v->d.pos : 0);
		c0 = so_ciclos();
		leidas = quiero ? dec_lee(&v->d, m->tmp1, quiero) : 0;
		m->ciclos_dec += so_ciclos() - c0;
		c0 = so_ciclos();
		if (leidas <= 0 && pasa_el_filtro(m, v)) {
			v->acabada = 1;
			break;
		}
		if (leidas <= 0) {
			/* se acabó: ceros detrás, para sacar lo de dentro */
			int z = RM_TAPS * 2 + (m->velocidad != 1.0f ? v->es.n * 2 + v->es.busca * 2 : 0);

			if (z > m->tmp_n)
				z = m->tmp_n;
			memset(m->tmp1, 0, (size_t)z * 2 * sizeof(float));
			v->vaciando = 1;
			leidas = z;
		} else {
			v->entrada += (u64)leidas;
			if (v->ganancia != 1.0f) {
				int i;

				for (i = 0; i < 2 * leidas; i++)
					m->tmp1[i] *= v->ganancia;
			}
		}
		e = es_hace(&v->es, m->tmp1, leidas, m->tmp2, m->tmp_n * 2);
		rm_hace(&v->rm, m->tmp2, e, NULL, 0);
		m->ciclos_dsp += so_ciclos() - c0;
	}
	return hechas;
}

/* ---------------------------------------------------------------- las marcas */
static void pon_marca(motor *m, int cancion, u64 pos, u32 hz)
{
	marca *k;

	if (m->n_marcas >= MAX_MARCAS) {
		m->primera_marca = (m->primera_marca + 1) % MAX_MARCAS;
		m->n_marcas--;
	}
	k = &m->marcas[(m->primera_marca + m->n_marcas) % MAX_MARCAS];
	k->salida = m->salida;
	k->cancion = cancion;
	k->pos = pos;
	k->hz = hz;
	k->velocidad = m->velocidad;
	m->n_marcas++;
}

static u64 tocadas(motor *m)
{
	if (m->sin_tarjeta) {
		u64 t = (u64)m->ms_sin_tarjeta * m->hz / 1000;

		return t > m->salida ? m->salida : t;
	}
	return so_sonido_tocadas();
}

/* qué se oye ahora y por dónde va */
static void mira_oida(motor *m)
{
	u64 t = tocadas(m);
	int i, mejor = -1;

	for (i = 0; i < m->n_marcas; i++) {
		marca *k = &m->marcas[(m->primera_marca + i) % MAX_MARCAS];

		if (k->salida <= t)
			mejor = i;
	}
	if (mejor < 0) {
		if (m->n_marcas)
			mejor = 0;
		else
			return;
	}
	{
		marca *k = &m->marcas[(m->primera_marca + mejor) % MAX_MARCAS];
		u64 dentro = t > k->salida ? t - k->salida : 0;
		double pos = (double)k->pos + (double)dentro * k->hz / m->hz * k->velocidad;

		if (k->cancion != m->oida) {
			m->oida = k->cancion;
			m->cambio++;
		}
		m->pos_oida_ms = k->hz ? (u32)(pos * 1000.0 / k->hz) : 0;
		/* (las de antes de la que vale ya no hacen falta) */
		if (mejor > 0) {
			m->primera_marca = (m->primera_marca + mejor) % MAX_MARCAS;
			m->n_marcas -= mejor;
		}
	}
}

/* ---------------------------------------------------------------- la salida */
int motor_prepara(motor *m, lista *l)
{
	memset(m, 0, sizeof *m);
	m->l = l;
	m->v[0].cancion = m->v[1].cancion = -1;
	m->oida = -1;
	m->velocidad = 1;
	m->volumen = 80;
	m->sin_huecos = 1;
	m->replaygain = RG_PISTA;
	m->objetivo_ms = OBJETIVO_MS;
	m->hz = so_hz_tarjeta();
	if (!m->hz)
		m->hz = 48000;
	dmod_hz = m->hz;
	if (!so_sonido_abre(m->hz)) {
		m->sin_tarjeta = 1;
		copia(m->error, sizeof m->error, "No hay tarjeta de sonido: no se oirá nada.");
	}
	so_sonido_volumen(m->volumen);
	eq_prepara(&m->eq, m->hz);
	m->tmp_n = TROZO * 5;
	m->tmp1 = so_pide((size_t)m->tmp_n * 2 * sizeof(float));
	m->tmp2 = so_pide((size_t)m->tmp_n * 4 * sizeof(float));
	m->tmp3 = so_pide((size_t)TROZO * 2 * sizeof(float));
	m->s16 = so_pide((size_t)TROZO * 2 * sizeof(i16));
	return m->tmp1 && m->tmp2 && m->tmp3 && m->s16 ? 0 : -1;
}

static void da_salida(motor *m, float *s, int n)
{
	int i;

	eq_hace(&m->eq, s, n);
	for (i = 0; i < n; i++) {
		float l = s[2 * i], r = s[2 * i + 1];
		int a, b;

		m->vis[(m->salida + (u64)i) % VIS_N] = (l + r) * 0.5f;
		a = (int)(l * 32767.0f);
		b = (int)(r * 32767.0f);
		m->s16[2 * i] = (i16)(a > 32767 ? 32767 : a < -32768 ? -32768 : a);
		m->s16[2 * i + 1] = (i16)(b > 32767 ? 32767 : b < -32768 ? -32768 : b);
	}
	if (!m->sin_tarjeta) {
		u32 dadas = 0;
		int vueltas = 0;

		/* (cabe: se ha mirado antes cuánto hay en la cola) */
		while (dadas < (u32)n && vueltas++ < 4) {
			u32 k = so_sonido_da(m->s16 + 2 * dadas, (u32)n - dadas);

			dadas += k;
			if (!k)
				so_duerme(5);
		}
	}
	m->salida += (u64)n;
	m->tramas_medidas += (u64)n;
}

int motor_cola_ms(motor *m)
{
	u64 t = tocadas(m);
	u64 c = m->sin_tarjeta ? (m->salida > t ? m->salida - t : 0) : so_sonido_en_cola();

	return (int)(c * 1000 / m->hz);
}

static void olvida_siguiente(motor *m)
{
	voz_cierra(&m->v[!m->va]);
}

/* ¿la siguiente que hay abierta sigue siendo la que toca? */
static void mira_siguiente(motor *m)
{
	voz *a = &m->v[m->va], *b = &m->v[!m->va];

	if (b->cancion >= 0) {
		if (b->cancion >= m->l->n || memcmp(&m->l->c[b->cancion].o, &b->d.o, sizeof(origen)))
			olvida_siguiente(m);
	}
	if (b->cancion < 0 && a->lista && m->estado != M_PARADO) {
		u64 falta = voz_falta(m, a);

		if ((falta && falta < (u64)(ABRIR_ANTES_S + m->fundido) * m->hz) || a->acabada) {
			int guarda = m->l->pos_orden, ac = m->l->actual, s;

			m->l->actual = a->cancion;
			s = lista_siguiente(m->l, 0);
			m->l->pos_orden = guarda;
			m->l->actual = ac;
			if (s >= 0)
				voz_empieza(m, b, s);
			else
				b->cancion = -2;	/* (no hay siguiente: que no se mire otra vez) */
		}
	}
}

/* pasa a la siguiente (la que ya estaba abriéndose) */
static int pasa_a_siguiente(motor *m)
{
	voz *b = &m->v[!m->va];

	if (b->cancion < 0)
		return 0;
	voz_cierra(&m->v[m->va]);
	m->va = !m->va;
	m->l->actual = b->cancion;
	/* (para el orden barajado: como si se hubiera pedido la siguiente) */
	if (m->l->aleatorio)
		lista_siguiente(m->l, 0), m->l->actual = b->cancion;
	return 1;
}

void motor_ciclo(motor *m, u32 presupuesto)
{
	u32 t0 = so_ms(), ahora;
	static u32 antes;

	ahora = so_ms();
	if (m->sin_tarjeta && m->estado == M_SONANDO)
		m->ms_sin_tarjeta += ahora - antes;
	antes = ahora;
	mira_oida(m);
	/* si la cola se ha quedado casi vacía sonando (otro programa gasta el
	 * procesador), que vaya más por delante */
	if (m->estado == M_SONANDO && !m->cebando && m->objetivo_ms < OBJETIVO_MAX && motor_cola_ms(m) < 120 && m->v[m->va].lista && !m->v[m->va].acabada) {
		m->objetivo_ms += 200;
		reg("la cola se ha quedado en %d ms: ahora, %d ms por delante", motor_cola_ms(m), m->objetivo_ms);
	}
	if (m->estado != M_SONANDO) {
		/* aún así: abrir de a poco la que toca */
		return;
	}
	for (;;) {
		voz *a = &m->v[m->va], *b;
		int n, hechas;
		float *s = m->tmp3;

		mira_siguiente(m);
		b = &m->v[!m->va];
		/* la que suena, lista */
		if (!a->lista) {
			int r = voz_paso(m, a);

			if (r < 0) {
				if (a->cancion >= 0 && a->cancion < m->l->n) {
					m->l->c[a->cancion].fallo = 1;
					m->l->cambios++;
				}
				/* a la siguiente que se pueda */
				{
					int s2;

					m->l->actual = a->cancion;
					s2 = lista_siguiente(m->l, 0);
					voz_cierra(a);
					if (s2 < 0 || s2 == m->l->actual) {
						m->estado = M_PARADO;
						return;
					}
					m->l->actual = s2;
					voz_empieza(m, a, s2);
				}
				continue;
			}
			if (r == 0) {
				if (so_ms() - t0 >= presupuesto)
					return;
				continue;
			}
			pon_marca(m, a->cancion, a->d.pos, a->d.hz);
		}
		/* la siguiente, de a poco (con lo que sobre) */
		if (b->cancion >= 0 && !b->lista && b->abierta && !b->fallo)
			voz_paso(m, b);
		/* ¿cabe más en la cola? */
		if (motor_cola_ms(m) >= m->objetivo_ms) {
			if (m->cebando) {
				m->cebando = 0;
				so_sonido_pausa(0);
			}
			return;
		}
		if (so_ms() - t0 >= presupuesto)
			return;
		n = TROZO;
		hechas = voz_saca(m, a, s, n);
		/* el fundido: lo de la siguiente, sumado */
		/* (entre las pistas de un mismo disco —una hoja .CUE—, sin fundido: van seguidas) */
		if (m->fundido > 0 && b->lista && !b->fallo && strcmp(a->d.o.ruta, b->d.o.ruta)) {
			u64 falta = voz_falta(m, a), F = (u64)m->fundido * m->hz;

			if ((falta && falta <= F) || a->acabada) {
				float *t = m->tmp2;
				int i, k;

				if (b->d.pos == 0 && b->rm.en_ent <= RM_TAPS)
					pon_marca(m, b->cancion, 0, b->d.hz);
				k = voz_saca(m, b, t, n);
				for (i = 0; i < n; i++) {
					double x = falta > (u64)i ? (double)(falta - (u64)i) / (double)F : 0;
					float ga, gb, la = i < hechas ? s[2 * i] : 0, ra = i < hechas ? s[2 * i + 1] : 0;
					float lb = i < k ? t[2 * i] : 0, rb = i < k ? t[2 * i + 1] : 0;

					if (x > 1)
						x = 1;
					ga = (float)sin(x * 1.5707963);
					gb = (float)cos(x * 1.5707963);
					s[2 * i] = la * ga + lb * gb;
					s[2 * i + 1] = ra * ga + rb * gb;
				}
				if (k > hechas)
					hechas = k;
				if (a->acabada && hechas < n) {
					/* la de antes se ha acabado: la siguiente es la que suena */
					da_salida(m, s, hechas);
					pasa_a_siguiente(m);
					continue;
				}
				if (a->acabada) {
					da_salida(m, s, hechas);
					pasa_a_siguiente(m);
					continue;
				}
			}
		}
		if (hechas > 0)
			da_salida(m, s, hechas);
		if (a->acabada) {
			/* sin huecos: la siguiente, ya abierta, sigue justo aquí */
			if (b->cancion >= 0 && !b->fallo) {
				if (!m->sin_huecos && !b->lista) {
					/* (con huecos: se espera a abrirla) */
				}
				pasa_a_siguiente(m);
				a = &m->v[m->va];
				if (a->lista)
					pon_marca(m, a->cancion, a->d.pos, a->d.hz);
				if (!m->sin_huecos) {
					/* un silencio corto entre canciones */
					memset(s, 0, (size_t)TROZO * 2 * sizeof(float));
					da_salida(m, s, m->hz / 50);
				}
				continue;
			}
			if (b->cancion == -2 || b->fallo) {
				/* se acabó la lista */
				voz_cierra(b);
				pon_marca(m, -1, 0, m->hz);
				voz_cierra(a);
				m->estado = M_PARADO;
				m->l->actual = -1;
				if (m->cebando) {
					m->cebando = 0;
					so_sonido_pausa(0);
				}
				return;
			}
			if (so_ms() - t0 >= presupuesto)
				return;
		}
	}
}

void motor_toca(motor *m, int c)
{
	if (c < 0 || c >= m->l->n)
		return;
	voz_cierra(&m->v[0]);
	voz_cierra(&m->v[1]);
	m->va = 0;
	if (!m->sin_tarjeta)
		so_sonido_vacia();
	m->salida = m->sin_tarjeta ? m->salida : so_sonido_tocadas();
	if (m->sin_tarjeta)
		m->ms_sin_tarjeta = (u32)(m->salida * 1000 / m->hz);
	m->n_marcas = 0;
	m->l->actual = c;
	m->l->c[c].fallo = 0;
	m->error[0] = 0;
	eq_vacia(&m->eq);
	voz_empieza(m, &m->v[0], c);
	m->estado = M_SONANDO;
	/* la tarjeta, en pausa hasta que haya cola: así no suena un silencio
	 * entre el primer trozo y los siguientes */
	so_sonido_pausa(1);
	m->cebando = 1;
	m->oida = c;
	m->pos_oida_ms = 0;
	m->cambio++;
}

void motor_pausa(motor *m)
{
	if (m->estado == M_SONANDO) {
		m->estado = M_PAUSA;
		so_sonido_pausa(1);
	} else if (m->estado == M_PAUSA) {
		m->estado = M_SONANDO;
		if (!m->cebando)
			so_sonido_pausa(0);
	} else if (m->l->n)
		motor_toca(m, m->l->actual >= 0 ? m->l->actual : (m->oida >= 0 && m->oida < m->l->n ? m->oida : 0));
}

void motor_para(motor *m)
{
	voz_cierra(&m->v[0]);
	voz_cierra(&m->v[1]);
	if (!m->sin_tarjeta)
		so_sonido_vacia();
	so_sonido_pausa(0);
	m->estado = M_PARADO;
	m->n_marcas = 0;
	m->pos_oida_ms = 0;
	m->cambio++;
}

void motor_siguiente(motor *m)
{
	int s;

	m->l->actual = m->oida >= 0 ? m->oida : m->l->actual;
	s = lista_siguiente(m->l, 1);
	if (s >= 0)
		motor_toca(m, s);
}

void motor_anterior(motor *m)
{
	int s;

	/* pasados tres segundos, al principio de esta */
	if (m->estado != M_PARADO && motor_pos_ms(m) > 3000 && m->oida >= 0) {
		motor_toca(m, m->oida);
		return;
	}
	m->l->actual = m->oida >= 0 ? m->oida : m->l->actual;
	s = lista_anterior(m->l);
	if (s >= 0)
		motor_toca(m, s);
}

void motor_busca_ms(motor *m, u32 ms)
{
	voz *a = &m->v[m->va];
	u64 t;

	if (m->estado == M_PARADO || !a->lista || !a->d.buscable)
		return;
	t = (u64)ms * a->d.hz / 1000 + (a->d.o.tramo_desde ? (u64)a->d.o.tramo_desde * a->d.hz / 75 : 0);
	if (dec_busca(&a->d, t))
		return;
	a->acabada = a->vaciando = 0;
	a->entrada = a->sacadas = 0;
	rm_vacia(&a->rm);
	es_vacia(&a->es);
	olvida_siguiente(m);
	if (!m->sin_tarjeta)
		so_sonido_vacia();
	m->salida = m->sin_tarjeta ? m->salida : so_sonido_tocadas();
	m->n_marcas = 0;
	pon_marca(m, a->cancion, a->d.pos - (a->d.o.tramo_desde ? (u64)a->d.o.tramo_desde * a->d.hz / 75 : 0), a->d.hz);
	m->pos_oida_ms = ms;
	if (m->estado == M_SONANDO) {
		so_sonido_pausa(1);
		m->cebando = 1;
	}
}

void motor_velocidad(motor *m, float v)
{
	int i;

	if (v < 0.5f)
		v = 0.5f;
	if (v > 2.0f)
		v = 2.0f;
	m->velocidad = v;
	for (i = 0; i < 2; i++)
		if (m->v[i].lista)
			es_velocidad(&m->v[i].es, v);
	/* (desde aquí, las marcas con la nueva) */
	if (m->v[m->va].lista)
		pon_marca(m, m->v[m->va].cancion, m->v[m->va].d.pos, m->v[m->va].d.hz);
}

void motor_volumen(motor *m, int v)
{
	m->volumen = v < 0 ? 0 : v > 100 ? 100 : v;
	so_sonido_volumen(m->volumen);
}

u32 motor_pos_ms(motor *m)
{
	cancion *c;
	u32 p = m->pos_oida_ms;

	if (m->oida >= 0 && m->oida < m->l->n) {
		c = &m->l->c[m->oida];
		if (c->o.tramo_desde) {
			u32 d = c->o.tramo_desde * 1000 / 75;

			p = p > d ? p - d : 0;
		}
	}
	return p;
}

u32 motor_total_ms(motor *m)
{
	if (m->oida >= 0 && m->oida < m->l->n)
		return m->l->c[m->oida].duracion * 1000;
	return 0;
}

voz *motor_voz_oida(motor *m)
{
	int i;

	for (i = 0; i < 2; i++)
		if (m->v[i].cancion == m->oida && m->v[i].lista)
			return &m->v[i];
	return NULL;
}

void motor_lo_oido(motor *m, float *d, int n)
{
	u64 t = tocadas(m);
	int i;

	for (i = 0; i < n; i++) {
		u64 k = t + (u64)i;

		k = k >= (u64)n ? k - (u64)n : 0;
		d[i] = (m->estado == M_PARADO || k + VIS_N < m->salida) ? 0 : m->vis[k % VIS_N];
	}
}

/* (la lista ha cambiado de orden) los números de lo que suena, otra vez:
 * por su origen (el archivo, el trozo, la pista) */
void motor_reordena(motor *m)
{
	int i, k, j, antes[2], nuevo[2], oida = m->oida, hecha = 0;

	for (i = 0; i < 2; i++) {
		voz *v = &m->v[i];

		antes[i] = nuevo[i] = v->cancion;
		if (v->cancion < 0 || !v->abierta)
			continue;
		for (k = 0; k < m->l->n; k++)
			if (!memcmp(&m->l->c[k].o, &v->d.o, sizeof(origen)))
				break;
		if (k < m->l->n)
			nuevo[i] = k;
	}
	/* (de una vez: los números viejos a los nuevos) */
	for (j = 0; j < m->n_marcas; j++) {
		marca *mk = &m->marcas[(m->primera_marca + j) % MAX_MARCAS];

		for (i = 0; i < 2; i++)
			if (antes[i] >= 0 && mk->cancion == antes[i]) {
				mk->cancion = nuevo[i];
				break;
			}
		if (i == 2)
			mk->cancion = -1;	/* (una que ya no está en la lista) */
	}
	for (i = 0; i < 2 && !hecha; i++)
		if (antes[i] >= 0 && oida == antes[i]) {
			m->oida = nuevo[i];
			hecha = 1;
		}
	if (!hecha)
		m->oida = -1;
	for (i = 0; i < 2; i++)
		m->v[i].cancion = nuevo[i];
}

void motor_cierra(motor *m)
{
	voz_cierra(&m->v[0]);
	voz_cierra(&m->v[1]);
	so_sonido_cierra();
	so_suelta(m->tmp1);
	so_suelta(m->tmp2);
	so_suelta(m->tmp3);
	so_suelta(m->s16);
}

/* ---------------------------------------------------------------- la duración */
static u32 dur_ogg(const origen *o)
{
	so_archivo *a = so_abre(o->ruta);
	u8 *b;
	u32 hz = 0, n, i;
	u64 largo, gr = 0;
	int k;

	if (!a)
		return 0;
	b = so_pide(65536 + 16);
	if (!b) {
		so_cierra(a);
		return 0;
	}
	k = so_lee(a, b, 4096);
	/* la frecuencia: la de la cabecera de Vorbis (o de Opus: siempre 48 kHz) */
	if (k > 60 && !memcmp(b, "OggS", 4)) {
		u32 p = 27 + b[26];

		if (p + 16 < (u32)k && !memcmp(b + p, "\x01vorbis", 7))
			hz = le32(b + p + 12);
		else if (p + 8 < (u32)k && !memcmp(b + p, "OpusHead", 8))
			hz = 48000;
	}
	largo = so_largo(a);
	n = largo > 65536 ? 65536 : (u32)largo;
	if (hz && so_salta_a(a, largo - n) == 0 && so_lee(a, b, n) == (int)n) {
		for (i = n >= 27 ? n - 27 : 0; i > 0; i--)
			if (!memcmp(b + i, "OggS", 4)) {
				gr = (u64)le32(b + i + 6) | (u64)le32(b + i + 10) << 32;
				break;
			}
	}
	so_suelta(b);
	so_cierra(a);
	return hz && gr != (u64)-1 ? (u32)(gr / hz) : 0;
}

u32 duracion_rapida(const origen *o)
{
	dec d;
	u32 s = 0;
	int i, f;
	u8 c[64];
	so_archivo *a;

	if (o->lector)
		return o->sectores / 75;
	if (o->largo)
		return (u32)(o->largo / (2352 * 75));
	a = so_abre(o->ruta);
	if (!a)
		return 0;
	i = so_lee(a, c, sizeof c);
	so_cierra(a);
	f = dec_formato(c, i > 0 ? (u32)i : 0, o->ruta);
	if (f == F_VORBIS || f == F_OPUS)
		return dur_ogg(o);
	if (f != F_MP3 && f != F_FLAC && f != F_WAV)
		return 0;
	if (dec_abre(&d, o))
		return 0;
	for (i = 0; i < 6; i++) {
		int r = dec_paso(&d);

		if (r)
			break;
	}
	if (d.listo && d.hz && d.total)
		s = (u32)(d.total / d.hz);
	dec_cierra(&d);
	if (o->tramo_hasta > o->tramo_desde)
		s = (o->tramo_hasta - o->tramo_desde) / 75;
	return s;
}
