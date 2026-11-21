/* servicio.c - (SONIDO) SONIDO.BIN, el servicio de sonido de Hispa-Pentium
 * (docs/PENTIUM.md, «El sonido»), y la orden SONIDO de la Consola.
 *
 * Como servicio («/SERVICIO»: lo lanza el núcleo cuando un programa pide un
 * canal; «/ARRANQUE»: el escritorio al arrancar, y entonces suena el
 * sonido de inicio): se apunta (la llamada 195, que le da los puertos),
 * busca la tarjeta (HD Audio, AC'97 y la Sound Blaster 16, por ese orden),
 * pone en marcha su DMA sobre un anillo de 64 K y, en cada tic mientras
 * suena algo (si no, cada dos décimas o cuando el núcleo lo despierta):
 * el intercambio con el núcleo (la 198: el volumen, los canales de los
 * programas y las órdenes), mira por dónde va la tarjeta y mezcla por
 * delante hasta 80 ms, con 120 ms de silencio detrás (si el servicio se
 * retrasa, suena silencio y no lo de la vuelta anterior). Si no hay
 * tarjeta lo dice y acaba (el núcleo no lo vuelve a lanzar).
 * Lo suyo, en \HISPA\SONIDO.INI: volumen=, silencio= y sonidos= (los guarda
 * él, a los dos segundos de un cambio) y, para la tarjeta, pines=, gpio=,
 * nogpio= y eapd= (hda.c, ac97.c). Los sonidos del sistema, INICIO.WAV,
 * APAGAR.WAV, AVISO.WAV y ERROR.WAV de la carpeta del sistema (hechos por
 * sonido/hazwav.py, nuestros).
 *
 * Como orden (SONIDO en la Consola, con lo de «SONIDO /?»), un programa
 * más que usa las llamadas del sonido: el estado, el volumen, tocar un .WAV
 * o un tono, probar los altavoces; y las ventanas de Configuración > Sonido
 * y del icono de la bandeja (ventana.c).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "sonido.h"

struct estado_sonido estado;
int estado_cambiado;
char ini_texto[2048];
u8 *anillo;
u32 anillo_fis;
u32 hz_salida = 48000;

int ventana_ajustes(void);		/* ventana.c */
int ventana_bandeja(void);

/* ---- el PCI (con IOPL 3: los puertos CF8h y CFCh, con las interrupciones
 * tapadas para que no se cuele el núcleo) ---- */
u32 pci_lee(u32 a, u32 r, int n)
{
	u32 f = cli_(), v;

	outl(0xCF8, 0x80000000u | (a & 0xFFFF00) | (r & 0xFC));
	if (n == 1)
		v = inb(0xCFC + (r & 3));
	else if (n == 2)
		v = inw(0xCFC + (r & 2));
	else
		v = inl(0xCFC);
	sti_(f);
	return v;
}

void pci_escribe(u32 a, u32 r, u32 v, int n)
{
	u32 f = cli_();

	outl(0xCF8, 0x80000000u | (a & 0xFFFF00) | (r & 0xFC));
	if (n == 1)
		outb(0xCFC + (r & 3), v);
	else if (n == 2)
		outw(0xCFC + (r & 2), v);
	else
		outl(0xCFC, v);
	sti_(f);
}

/* el siguiente aparato (después de «desde») de esa clase (0xCCSSII) */
u32 pci_busca(u32 clase, u32 mascara, u32 desde)
{
	u32 bus, ap, fn, a;
	int pasado = desde == 0xFFFFFFFF;

	for (bus = 0; bus < 8; bus++)
		for (ap = 0; ap < 32; ap++)
			for (fn = 0; fn < 8; fn++) {
				u32 id;

				a = bus << 16 | ap << 11 | fn << 8;
				id = pci_lee(a, 0, 4);
				if ((id & 0xFFFF) == 0xFFFF) {
					if (fn == 0)
						break;
					continue;
				}
				if (!pasado) {
					if (a == desde)
						pasado = 1;
				} else if (((pci_lee(a, 0x08, 4) >> 8) & mascara) == (clase & mascara))
					return a;
				if (fn == 0 && !(pci_lee(a, 0x0C, 4) & 0x800000))
					break;	/* (no tiene más funciones) */
			}
	return 0xFFFFFFFF;
}

void *registros(u32 fisica, u32 octetos)
{
	u32 v = sc(SC_REGISTROS, fisica, octetos, 0);

	return ES_ERROR(v) ? 0 : (void *)v;
}

void *dma(u32 paginas, u32 *fisica, int baja)
{
	u32 v = sc(SC_DMA, paginas, (u32)fisica, baja ? 1 : 0);

	return ES_ERROR(v) ? 0 : (void *)v;
}

u32 divide64(u64 n, u32 d)
{
	u64 r = 0, q = 0;
	int i;

	for (i = 63; i >= 0; i--) {
		r = r << 1 | ((n >> i) & 1);
		if (r >= d) {
			r -= d;
			q |= (u64)1 << i;
		}
	}
	return (u32)q;
}

void espera_us(u32 us)
{
	while (us--)
		outb(0x80, 0);
}

/* ---- los sonidos del sistema ---- */
static u8 *sistema[N_SISTEMA];
static u32 sistema_n[N_SISTEMA];
static const char *const nombres_sistema[N_SISTEMA] = {"INICIO.WAV", "APAGAR.WAV", "AVISO.WAV", "ERROR.WAV", 0};

static void lee_sistema(void)
{
	char r[64];
	int i;

	for (i = 0; i < N_SISTEMA; i++) {
		u32 m, n;

		if (!nombres_sistema[i])
			continue;
		h_ruta(r, sizeof r, nombres_sistema[i]);
		m = sc(SC_ABRIR, (u32)r, 0, 0);
		if (ES_ERROR(m)) {
			formatear(r, sizeof r, "%c:\\%s", h_sistema, nombres_sistema[i]);
			m = sc(SC_ABRIR, (u32)r, 0, 0);
		}
		if (ES_ERROR(m))
			continue;
		n = sc(SC_LARGO, m, 0, 0);
		if (!ES_ERROR(n) && n > 44 && n < 512 * 1024 && (sistema[i] = h_memoria(n)))
			sistema_n[i] = sc(SC_LEER, m, (u32)sistema[i], n);
		sc(SC_CERRAR, m, 0, 0);
	}
}

static void suena_sistema(u32 cual)
{
	if (cual == SS_VOLUMEN) {
		voz_tono(880, 60, 3, 70);
		return;
	}
	if (cual < N_SISTEMA && sistema[cual] && !ES_ERROR(sistema_n[cual]))
		voz_memoria(sistema[cual], sistema_n[cual], 0, 0, 3, 100);
	else if (cual == SS_AVISO)
		voz_tono(660, 150, 3, 80);
	else if (cual == SS_ERROR)
		voz_tono(220, 300, 3, 80);
}

/* ---- SONIDO.INI ---- */
static char ruta_ini[64];
static u32 guardado_vol = 0xFFFFFFFF, guardado_sil, guardado_sis;

static void guarda_ini(u32 vol, u32 sil, u32 sis)
{
	static char t[2200];
	const char *p = ini_texto;
	int k = 0;

	/* lo que hubiera, menos lo nuestro, y lo nuestro al principio */
	k = formatear(t, sizeof t, "volumen=%u\r\nsilencio=%u\r\nsonidos=%u\r\n", vol, sil, sis);
	while (*p) {
		const char *q = p;
		int n;

		while (*q && *q != '\n')
			q++;
		n = q - p + (*q ? 1 : 0);
		if (!h_empieza(p, "volumen") && !h_empieza(p, "silencio") && !h_empieza(p, "sonidos") && k + n < (int)sizeof t - 1) {
			memcpy(t + k, p, n);
			k += n;
		}
		p += n;
	}
	t[k] = 0;
	if (h_escribe_archivo(ruta_ini, t, k) == 0) {
		h_copia(ini_texto, t, sizeof ini_texto);
		guardado_vol = vol, guardado_sil = sil, guardado_sis = sis;
	}
}

static u32 ini_num(const char *clave, u32 si_no)
{
	char v[16];
	u32 n = 0;
	int i;

	if (!h_ini(ini_texto, clave, v, sizeof v) || v[0] < '0' || v[0] > '9')
		return si_no;
	for (i = 0; v[i] >= '0' && v[i] <= '9'; i++)
		n = n * 10 + v[i] - '0';
	return n;
}

/* ---- el servicio ---- */
static struct tarjeta tarjeta;
static struct para_servicio para;
static u32 w, z;			/* por dónde va lo mezclado y hasta dónde hay silencio puesto */
static int callado, en_reposo;

#define TRAMA 4
static u32 ms_a_octetos(u32 ms) { return hz_salida * ms / 1000 * TRAMA; }

static void pon_ceros(u32 desde, u32 n)
{
	while (n) {
		u32 p = desde % ANILLO, t = ANILLO - p;

		if (t > n)
			t = n;
		memset(anillo + p, 0, t);
		desde += t;
		n -= t;
	}
}

static void rellena(void)
{
	u32 pos = tarjeta.posicion() & ~(TRAMA - 1), delante, objetivo = ms_a_octetos(80), guarda = ms_a_octetos(120);

	delante = (w - pos) % ANILLO;
	if (delante > ANILLO - guarda - objetivo) {
		/* (se ha quedado atrás: desde un poco por delante de la tarjeta) */
		w = pos + ms_a_octetos(10);
		z = w;
		delante = ms_a_octetos(10);
	}
	while (delante < objetivo) {
		u32 p = w % ANILLO, n = objetivo - delante;

		if (n > ANILLO - p)
			n = ANILLO - p;
		mezcla((i16 *)(anillo + p), n / TRAMA);
		w += n;
		delante += n;
	}
	/* el silencio de detrás, solo lo que falta */
	if ((i32)(z - w) < 0)
		z = w;
	if (z - w < guarda) {
		pon_ceros(z, w + guarda - z);
		z = w + guarda;
	}
}

static void orden(struct orden_sonido *o)
{
	switch (o->clase) {
	case O_SISTEMA:
		if (para.sistema)
			suena_sistema(o->a);
		break;
	case O_PROBAR:
		/* izquierdo: un do (523 Hz); derecho: un mi (659 Hz); los dos, un sol */
		voz_tono(o->a == 1 ? 523 : o->a == 2 ? 659 : 784, 900, o->a & 3, 100);
		break;
	case O_WAV:
		if (!voz_archivo(o->ruta, o->a, o->b))
			h_apunta("no se puede tocar %s (no está, o no es un WAV PCM)", o->ruta);
		break;
	case O_PARAR_VOZ:
		voz_parar(o->a);
		break;
	case O_ACABO:
		/* (un .WAV de la 194 sigue sonando aunque su programa acabe: «tocar
		 * y olvidarse»; lo para C_CERRAR con su número) */
		break;
	case O_CALLAR:
		/* (se va a apagar: lo cambiado, guardado ya) */
		if (para.volumen != guardado_vol || para.silencio != guardado_sil || para.sistema != guardado_sis)
			guarda_ini(para.volumen, para.silencio, para.sistema);
		if (!callado && tarjeta.callar)
			tarjeta.callar();
		callado = 1;
		break;
	}
}

static int servicio(int arranque)
{
	u32 r, i, cambio_ini = 0, ult_vigila = 0, voces = 0;

	r = sc(SC_APUNTARSE, 0, 0, 0);
	if (r)
		return r == 0xFFFFFFF6 ? 0 : 1;	/* (EN_USO: ya hay uno) */
	h_ruta(ruta_ini, sizeof ruta_ini, "SONIDO.INI");
	r = h_lee_archivo(ruta_ini, ini_texto, sizeof ini_texto - 1);
	ini_texto[r > 0 ? r : 0] = 0;
	/* lo guardado: el volumen, el silencio y los sonidos del sistema */
	sc(SC_SONIDO, G_VOLUMEN, 0, ini_num("volumen", 80));
	sc(SC_SONIDO, G_SILENCIO, 0, ini_num("silencio", 0) ? 1 : 0);
	sc(SC_SONIDO, G_SONIDOS, 0, ini_num("sonidos", 1) ? 1 : 0);
	guardado_vol = ini_num("volumen", 80);
	guardado_sil = ini_num("silencio", 0) ? 1 : 0;
	guardado_sis = ini_num("sonidos", 1) ? 1 : 0;
	estado.version = 1;
	estado.pci = 0xFFFFFFFF;
	estado.dma_isa = 0xFFFFFFFF;
	if (!hda_buscar(&tarjeta) && !ac97_buscar(&tarjeta) && !sb16_buscar(&tarjeta)) {
		h_apunta("no hay tarjeta de sonido que se sepa llevar");
		estado.tarjeta = TARJETA_NINGUNA;
		sc(SC_INTERCAMBIO, (u32)&estado, (u32)&para, 0);
		return 0;
	}
	if (!anillo && !(anillo = dma(ANILLO / 4096, &anillo_fis, 0))) {
		h_apunta("no hay memoria para la DMA");
		estado.tarjeta = TARJETA_NINGUNA;
		sc(SC_INTERCAMBIO, (u32)&estado, (u32)&para, 0);
		return 1;
	}
	memset(anillo, 0, ANILLO);
	mezcla_prepara();
	lee_sistema();
	estado.hz = hz_salida;
	if (!estado.detalle[0])
		formatear(estado.detalle, sizeof estado.detalle, "%u Hz, 16 bits, estéreo", hz_salida);
	if (!tarjeta.empezar()) {
		formatear(estado.detalle, sizeof estado.detalle, "la DMA de la tarjeta no anda");
		h_apunta("la DMA de la tarjeta no anda");
	}
	tarjeta.vigilar();
	h_apunta("en marcha: %s, %s; %u Hz", estado.nombre, estado.codec, hz_salida);
	w = z = tarjeta.posicion();
	r = sc(SC_INTERCAMBIO, (u32)&estado, (u32)&para, 0);
	if (arranque && para.sistema && !para.silencio)
		suena_sistema(SS_INICIO);
	for (;;) {
		int activo = mezcla_activa();

		r = sc(SC_INTERCAMBIO, estado_cambiado ? (u32)&estado : 0, (u32)&para, activo || !en_reposo ? 1 : 20);
		estado_cambiado = 0;
		if (ES_ERROR(r))
			r = 0;
		vol_general = para.volumen > 100 ? 100 : para.volumen;
		en_silencio = para.silencio;
		mezcla_canales(&para);
		for (i = 0; i < r && i < MAX_ORDENES; i++)
			orden(&para.ordenes[i]);
		if (!callado) {
			tarjeta.cada_vez();
			if (mezcla_activa()) {
				en_reposo = 0;
				rellena();
			} else if (!en_reposo) {
				/* sin nada más que tocar: cuando la tarjeta pasa lo último
				 * mezclado, silencio en todo el anillo (y a esperar sin prisa) */
				u32 d = (w - tarjeta.posicion()) % ANILLO;

				if (d > ANILLO / 2 || d < ms_a_octetos(3)) {
					memset(anillo, 0, ANILLO);
					en_reposo = 1;
				}
			}
		}
		if (h_tics() - ult_vigila >= h_hz / 2) {
			ult_vigila = h_tics();
			tarjeta.vigilar();
		}
		if (mezcla_voces() != voces) {
			voces = mezcla_voces();
			estado.voces = voces;
			estado_cambiado = 1;
		}
		/* lo cambiado, a SONIDO.INI a los dos segundos */
		if (para.volumen != guardado_vol || para.silencio != guardado_sil || para.sistema != guardado_sis) {
			if (!cambio_ini)
				cambio_ini = h_tics() | 1;
			else if (h_tics() - cambio_ini > 2 * h_hz) {
				cambio_ini = 0;
				guarda_ini(para.volumen, para.silencio, para.sistema);
				guardado_vol = para.volumen, guardado_sil = para.silencio, guardado_sis = para.sistema;
			}
		} else
			cambio_ini = 0;
	}
}

/* ---- la orden SONIDO ---- */
static const char *const nombres_tarjeta[] = {"ninguna", "Sound Blaster", "Sound Blaster 16", "AC'97", "HD Audio"};

static int pide_estado(struct estado_sonido *e)
{
	u32 r = sc(SC_SONIDO, G_ESTADO, (u32)e, 0);

	if (r != 1) {
		/* (si no hay servicio: abrir un canal lo pone en marcha) */
		u32 c = sc(SC_ABRIR_CANAL, 48000, 2 | 16 << 8, 0);

		if (!ES_ERROR(c))
			sc(SC_CANAL, c, C_CERRAR, 0);
		r = sc(SC_SONIDO, G_ESTADO, (u32)e, 0);
	}
	return r == 1;
}

static void dice_estado(void)
{
	struct estado_sonido e;
	struct info_canal c[MAX_CANALES];
	int i;

	if (!pide_estado(&e)) {
		h_escribe("No hay tarjeta de sonido que Hispa-Pentium sepa llevar.\n");
		return;
	}
	h_escribef("Tarjeta:   %s (%s)\n", e.nombre, nombres_tarjeta[e.tarjeta < 5 ? e.tarjeta : 0]);
	h_escribef("Códec:     %s\n", e.codec);
	h_escribef("Salida:    %s%s; %u Hz, 16 bits, estéreo\n", e.salida == SALIDA_AURICULARES ? "auriculares" : e.salida == SALIDA_LINEA ? "línea" : "altavoces",
		   e.detecta ? (e.auriculares ? " (auriculares enchufados)" : " (sin auriculares)") : "", e.hz);
	if (e.detalle[0])
		h_escribef("Detalle:   %s\n", e.detalle);
	h_escribef("Volumen:   %u %%%s\n", e.volumen, e.silencio ? ", en silencio" : "");
	h_escribef("Sonidos del sistema: %s\n", e.sistema ? "sí" : "no");
	if (sc(SC_SONIDO, G_CANALES, (u32)c, 0) == MAX_CANALES)
		for (i = 0; i < MAX_CANALES; i++)
			if (c[i].dueno)
				h_escribef("Canal %d:   %s (tarea %u), %u Hz, %u bits, %s, volumen %u %%\n", i + 1, c[i].nombre, c[i].dueno, c[i].hz, c[i].bits,
					   c[i].canales == 2 ? "estéreo" : "mono", c[i].volumen);
}

/* un tono (en un canal propio, como cualquier programa) */
static int tono(u32 hz, u32 ms, int lados)
{
	static i16 b[2 * 1200];
	u32 c = sc(SC_ABRIR_CANAL, 48000, 2 | 16 << 8, 0), hechas = 0, total = 48 * ms, fase = 0, paso = divide64((u64)hz << 32, 48000);
	i16 seno[64];
	int i;

	if (ES_ERROR(c)) {
		h_escribe("No hay tarjeta de sonido.\n");
		return 1;
	}
	/* (un seno de 64 puntos, con la aproximación de Bhaskara) */
	for (i = 0; i < 64; i++) {
		int x = (i & 31) * 4, s = 24000 * (16 * x * (128 - x)) / (5 * 128 * 128 - 4 * x * (128 - x));

		seno[i] = (i16)(i < 32 ? s : -s);
	}
	while (hechas < total) {
		u32 n = total - hechas > 1200 ? 1200 : total - hechas, k;

		for (k = 0; k < n; k++) {
			i16 s = seno[fase >> 26];

			b[2 * k] = lados & 1 ? s : 0;
			b[2 * k + 1] = lados & 2 ? s : 0;
			fase += paso;
		}
		if (sc(SC_DAR, c, (u32)b, n * 4) != n * 4)
			break;
		hechas += n;
	}
	sc(SC_CANAL, c, C_ESPERAR, 0);
	sc(SC_CANAL, c, C_CERRAR, 0);
	return 0;
}

/* un .WAV por un canal propio (con la frecuencia y la forma que traiga) */
static int toca(const char *ruta)
{
	static u8 b[16384];
	u32 m = sc(SC_ABRIR, (u32)ruta, 0, 0), n, hz, can, bits, largo, ini, c;

	if (ES_ERROR(m)) {
		h_escribef("No está %s.\n", ruta);
		return 1;
	}
	n = sc(SC_LEER, m, (u32)b, sizeof b);
	if (ES_ERROR(n) || !(ini = wav_cabecera(b, n, &hz, &can, &bits, &largo))) {
		sc(SC_CERRAR, m, 0, 0);
		h_escribef("%s no es un WAV que se sepa tocar (PCM de 8 o 16 bits).\n", ruta);
		return 1;
	}
	c = sc(SC_ABRIR_CANAL, hz, can | bits << 8, 0);
	if (ES_ERROR(c)) {
		sc(SC_CERRAR, m, 0, 0);
		h_escribe("No hay tarjeta de sonido.\n");
		return 1;
	}
	h_escribef("%s: %u Hz, %u bits, %s; %u s.\n", ruta, hz, bits, can == 2 ? "estéreo" : "mono", largo / (hz * can * bits / 8));
	n -= ini;
	if (n > largo)
		n = largo;
	memmove(b, b + ini, n);
	while (largo && n && !ES_ERROR(n)) {
		if (sc(SC_DAR, c, (u32)b, n) != n)
			break;
		largo -= n;
		if (sc(SC_HUBO_CORTE, 0, 0, 0) == 1)
			break;
		n = sc(SC_LEER, m, (u32)b, largo < sizeof b ? largo : sizeof b);
	}
	sc(SC_CERRAR, m, 0, 0);
	sc(SC_CANAL, c, C_ESPERAR, 0);
	sc(SC_CANAL, c, C_CERRAR, 0);
	return 0;
}

static u32 numero(const char *t, const char **sigue)
{
	u32 n = 0;

	while (*t == ' ')
		t++;
	while (*t >= '0' && *t <= '9')
		n = n * 10 + *t++ - '0';
	if (sigue)
		*sigue = t;
	return n;
}

static const char *palabra(const char *t, char *d, int cabe)
{
	int k = 0;

	while (*t == ' ')
		t++;
	while (*t && *t != ' ' && k < cabe - 1)
		d[k++] = *t++;
	d[k] = 0;
	return t;
}

static int cliente(const char *a)
{
	char p[128];
	const char *s;

	a = palabra(a, p, sizeof p);
	if (!p[0] || h_es_ascii_igual(p, "/ESTADO")) {
		dice_estado();
		return 0;
	}
	if (h_es_ascii_igual(p, "/?") || h_es_ascii_igual(p, "/AYUDA")) {
		h_escribe("SONIDO                      cómo está el sonido (la tarjeta, la salida, el volumen)\n"
			  "SONIDO archivo.wav          lo toca (y espera a que acabe)\n"
			  "SONIDO /FONDO archivo.wav   lo toca el servicio, sin esperar\n"
			  "SONIDO /TONO hz [ms] [I|D]  un tono (I: solo por la izquierda; D: por la derecha)\n"
			  "SONIDO /VOLUMEN n           el volumen general (0 a 100)\n"
			  "SONIDO /SILENCIO [SI|NO]    en silencio, o no (sin nada: lo cambia)\n"
			  "SONIDO /SISTEMA SI|NO       los sonidos del sistema (inicio, apagar, avisos)\n"
			  "SONIDO /PROBAR I|D          prueba el altavoz izquierdo o el derecho\n"
			  "SONIDO /AVISO, /ERROR       el sonido del sistema\n"
			  "SONIDO /VENTANA             Configuración > Sonido\n"
			  "Con /SUELTO al final, sigue sin esperar (otro programa que suena a la vez).\n");
		return 0;
	}
	/* /SUELTO: la misma orden en otro programa, que se queda sonando */
	{
		int n = strlen(a);

		if (n >= 7 && h_es_ascii_igual(a + n - 7, "/SUELTO")) {
			static char r[200];
			char sis[48];

			formatear(r, sizeof r, "%s %s", p, a);
			r[strlen(r) - 7] = 0;
			h_ruta(sis, sizeof sis, "SONIDO.BIN");
			if (ES_ERROR(sc(SC_LANZAR_SUELTO, (u32)sis, (u32)r, 0))) {
				formatear(sis, sizeof sis, "%c:\\SONIDO.BIN", h_sistema);
				sc(SC_LANZAR_SUELTO, (u32)sis, (u32)r, 0);
			}
			return 0;
		}
	}
	if (h_es_ascii_igual(p, "/TONO")) {
		u32 hz = numero(a, &s), ms;
		char l[8];

		ms = numero(s, &s);
		palabra(s, l, sizeof l);
		return tono(hz ? hz : 440, ms ? ms : 1000, h_es_ascii_igual(l, "I") ? 1 : h_es_ascii_igual(l, "D") ? 2 : 3);
	}
	if (h_es_ascii_igual(p, "/VOLUMEN")) {
		u32 v = numero(a, 0);

		sc(SC_SONIDO, G_VOLUMEN, 0, v > 100 ? 100 : v);
		h_escribef("Volumen: %u %%\n", sc(SC_SONIDO, G_VOLUMEN, 0, 0xFFFFFFFF));
		return 0;
	}
	if (h_es_ascii_igual(p, "/SILENCIO")) {
		palabra(a, p, sizeof p);
		h_escribef("Silencio: %s\n", sc(SC_SONIDO, G_SILENCIO, 0, !p[0] ? 2 : h_es_ascii_igual(p, "NO") ? 0 : 1) ? "sí" : "no");
		return 0;
	}
	if (h_es_ascii_igual(p, "/SISTEMA")) {
		palabra(a, p, sizeof p);
		h_escribef("Sonidos del sistema: %s\n", sc(SC_SONIDO, G_SONIDOS, 0, !p[0] ? 2 : h_es_ascii_igual(p, "NO") ? 0 : 1) ? "sí" : "no");
		return 0;
	}
	if (h_es_ascii_igual(p, "/PROBAR")) {
		struct estado_sonido e;

		palabra(a, p, sizeof p);
		if (!pide_estado(&e)) {
			h_escribe("No hay tarjeta de sonido.\n");
			return 1;
		}
		sc(SC_SONIDO, G_PROBAR, 0, h_es_ascii_igual(p, "I") ? 1 : h_es_ascii_igual(p, "D") ? 2 : 3);
		h_dormir(h_hz);
		return 0;
	}
	if (h_es_ascii_igual(p, "/AVISO") || h_es_ascii_igual(p, "/ERROR") || h_es_ascii_igual(p, "/INICIO") || h_es_ascii_igual(p, "/APAGAR")) {
		struct estado_sonido e;

		if (pide_estado(&e))
			sc(SC_SONIDO, G_SISTEMA, 0, h_es_ascii_igual(p, "/AVISO") ? SS_AVISO : h_es_ascii_igual(p, "/ERROR") ? SS_ERROR : h_es_ascii_igual(p, "/INICIO") ? SS_INICIO : SS_APAGAR);
		h_dormir(h_hz * 2);
		return 0;
	}
	if (h_es_ascii_igual(p, "/FONDO")) {
		u32 v;

		palabra(a, p, sizeof p);
		v = sc(SC_TOCAR_WAV, (u32)p, 0, 0);
		if (ES_ERROR(v)) {
			h_escribe("No hay tarjeta de sonido.\n");
			return 1;
		}
		return 0;
	}
	if (h_es_ascii_igual(p, "/VENTANA"))
		return ventana_ajustes();
	if (h_es_ascii_igual(p, "/BANDEJA"))
		return ventana_bandeja();
	if (p[0] == '/') {
		h_escribef("No sé qué es %s (SONIDO /? dice lo que hay).\n", p);
		return 1;
	}
	return toca(p);
}

/* como h_inicia, sin el azar (que guarda su semilla en el disco: aquí no hace falta) */
static void inicia(void)
{
	struct { u8 letra, clase, medio, solo; u32 total, libre, del_sistema; u8 resto[60]; } u;
	u32 i;

	h_copia(h_nombre_prog, "sonido", sizeof h_nombre_prog);
	h_hz = sc(SC_DATO, DATO_HZ, 0, 0);
	if (!h_hz || ES_ERROR(h_hz))
		h_hz = 100;
	for (i = 0; i < 26 && sc(SC_INFO_UNIDAD, i, (u32)&u, 0) == 1; i++)
		if (u.del_sistema == 1 && u.letra)
			h_sistema = u.letra;
}

int principal(void)
{
	static char a[256];

	inicia();
	h_argumentos(a, sizeof a);
	if (h_empieza(a, "/SERVICIO"))
		return servicio(0);
	if (h_empieza(a, "/ARRANQUE"))
		return servicio(1);
	sc(SC_ATENDER_CORTE, 0, 0, 0);
	return cliente(a);
}
