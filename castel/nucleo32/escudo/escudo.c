/* escudo.c - ESCUDO.BIN, el antivirus de Hispa-Pentium: las órdenes, las
 * revisiones (archivos, carpetas, unidades), la cuarentena, el registro, las
 * opciones y lo que vigila (las memorias USB y los CD que se conectan).
 *
 *   ESCUDO [RUTA...]        revisa (un archivo, una carpeta, «X:», o «*»: todo)
 *                           y dice lo encontrado; sin ruta y con el escritorio,
 *                           la ventana «Escudo»
 *   ESCUDO /LISTA RUTA...   lo mismo, con una línea por archivo
 *   ESCUDO /CUARENTENA RUTA revisa y pone en cuarentena lo encontrado
 *   ESCUDO /ESTADO          las firmas (cuántas, de cuándo) y la cuarentena
 *   ESCUDO /ACTUALIZA       trae las firmas nuevas (PAQUETES ACTUALIZA)
 *   ESCUDO /RESTAURA n | /BORRA n   lo n.º n de la cuarentena
 *   ESCUDO /VENTANA [pestaña]  la ventana (lo lanza el menú Inicio y la bandeja)
 *   ESCUDO /VIGILAR         (lo lanza el escritorio al arrancar) pregunta al
 *                           conectar una memoria USB o meter un CD
 *   ESCUDO /PREGUNTA X:     (lo lanza /VIGILAR) «¿Revisar la memoria?»
 *   ESCUDO /DESCARGA RUTA   (lo lanza el Navegador al acabar una descarga)
 *   ESCUDO /ANTES RUTA      (lo lanza WIN32.BIN antes de abrir un .EXE): da 0
 *                           si se puede abrir, 1 si no
 * Ver LEEME.txt y docs/PENTIUM.md, «Escudo».
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stdarg.h>
#include "logica.h"

int vformatear(char *d, int cabe, const char *p, va_list va);
extern u32 motor_max_archivo;
extern int motor_zip;

char dir_escudo[RUTA], dir_cuarentena[RUTA], prog_escudo[RUTA];
opciones op;
info_firmas inf;
int firmas_cargadas;
revision rev;
void (*al_avanzar)(void);
en_cuarentena cua[MAX_CUARENTENA];
u32 n_cua;
int pl_atiende;

static void ruta_en(char *d, const char *carpeta, const char *nombre)
{
	formatear(d, RUTA, "%s\\%s", carpeta, nombre);
}

/* dónde está todo: instalado (\HISPA\ESCUDO.BIN) o no (\ESCUDO.BIN) */
static void busca_carpetas(void)
{
	char u = (char)unidad_sistema();
	formatear(prog_escudo, RUTA, "%c:\\HISPA\\ESCUDO.BIN", u);
	if (existe(prog_escudo)) {
		formatear(dir_escudo, RUTA, "%c:\\HISPA\\ESCUDO", u);
		formatear(dir_cuarentena, RUTA, "%c:\\HISPA\\CUARENTENA", u);
	} else {
		formatear(prog_escudo, RUTA, "%c:\\ESCUDO.BIN", u);
		formatear(dir_escudo, RUTA, "%c:\\ESCUDO", u);
		formatear(dir_cuarentena, RUTA, "%c:\\CUARENTENA", u);
	}
	/* la FAT de Hispa-Pentium solo escribe nombres de 8+3: «CUARENTENA» (10
	 * letras) se hace como CUARENTE; si ya hay una CUARENTENA con su nombre
	 * largo (hecha desde otro sistema), se usa esa (se abre por el largo) */
	if (!es_carpeta(dir_cuarentena))
		dir_cuarentena[strlen(dir_cuarentena) - 2] = 0;
}

/* ------------------------------------------------------------ las opciones */
static int valor_ini(const char *t, const char *clave, char *v, int cabe)
{
	u32 n = (u32)strlen(clave);
	const char *p = t;
	while (*p) {
		while (*p == ' ' || *p == '\t')
			p++;
		if (!strncasecmp_es(p, clave, n) && p[n] == '=') {
			int k = 0;
			p += n + 1;
			while (*p && *p != '\r' && *p != '\n' && k + 1 < cabe)
				v[k++] = *p++;
			v[k] = 0;
			return 1;
		}
		while (*p && *p != '\n')
			p++;
		if (*p)
			p++;
	}
	return 0;
}

static int si_no(const char *t, const char *clave, int defecto)
{
	char v[8];
	if (!valor_ini(t, clave, v, sizeof v))
		return defecto;
	return v[0] == 's' || v[0] == 'S' || v[0] == '1';
}

static u32 numero(const char *s)
{
	u32 n = 0;
	while (*s >= '0' && *s <= '9')
		n = n * 10 + (u32)(*s++ - '0');
	return n;
}

void lee_opciones(void)
{
	char t[2048], r[RUTA], v[16];
	int n;
	ruta_en(r, dir_escudo, "ESCUDO.INI");
	n = lee_todo(r, t, sizeof t - 1);
	if (n < 0)
		n = 0;
	t[n] = 0;
	op.preguntar = si_no(t, "preguntar", 1);
	op.descargas = si_no(t, "descargas", 1);
	op.antes_exe = si_no(t, "antes_exe", 1);
	op.zip = si_no(t, "zip", 1);
	op.cuarentena_sola = si_no(t, "cuarentena_sola", 0);
	op.actualizar_solo = si_no(t, "actualizar_solo", 1);
	/* de fábrica, según la memoria: 4 MB con 64, 8 con 128, 16 con más */
	{
		u32 megas = sc(SC_DATO, 1, 0, 0) / 256;
		op.max_mb = valor_ini(t, "max_mb", v, sizeof v) ? numero(v) : megas <= 72 ? 4 : megas <= 144 ? 8 : 16;
	}
	if (op.max_mb < 1 || op.max_mb > 128)
		op.max_mb = 16;
	op.ultima_act = valor_ini(t, "ultima_act", v, sizeof v) ? numero(v) : 0;
	motor_max_archivo = op.max_mb << 20;
	motor_zip = op.zip;
}

void guarda_opciones(void)
{
	char t[1024], r[RUTA];
	int n;
	hacer_carpeta(dir_escudo);
	n = formatear(t, sizeof t,
		      "; Escudo, el antivirus de Hispa-Pentium: sus opciones (las cambia su ventana)\r\n"
		      "preguntar=%s\r\ndescargas=%s\r\nantes_exe=%s\r\nzip=%s\r\ncuarentena_sola=%s\r\n"
		      "actualizar_solo=%s\r\nmax_mb=%u\r\nultima_act=%u\r\n",
		      op.preguntar ? "si" : "no", op.descargas ? "si" : "no", op.antes_exe ? "si" : "no",
		      op.zip ? "si" : "no", op.cuarentena_sola ? "si" : "no", op.actualizar_solo ? "si" : "no", op.max_mb,
		      op.ultima_act);
	ruta_en(r, dir_escudo, "ESCUDO.INI");
	escribe_todo(r, t, (u32)n);
	motor_max_archivo = op.max_mb << 20;
	motor_zip = op.zip;
}

/* ------------------------------------------------------------ el registro */
void apunta(const char *f, ...)
{
	char t[600], l[700], r[RUTA];
	va_list va;
	u32 d = fecha_hoy(), h = hora_ahora();
	char *q;
	va_start(va, f);
	vformatear(t, sizeof t, f, va);
	va_end(va);
	for (q = t; *q; q++)	/* (un nombre con saltos de línea, de un ZIP, no rompe el registro) */
		if ((u8)*q < 0x20)
			*q = '?';
	serie("escudo: %s", t);
	formatear(l, sizeof l, "%u-%02u-%02u %02u:%02u  %s\r\n", d / 10000, d / 100 % 100, d % 100, h / 100, h % 100, t);
	hacer_carpeta(dir_escudo);
	ruta_en(r, dir_escudo, "REGISTRO.TXT");
	anade_a(r, l);
}

int lee_registro(char *d, u32 cabe)
{
	char r[RUTA];
	int h, n;
	u32 tam;
	ruta_en(r, dir_escudo, "REGISTRO.TXT");
	h = pl_abrir(r);
	if (h < 0)
		return 0;
	tam = pl_largo(h);
	if (tam > cabe - 1)
		pl_mover(h, tam - (cabe - 1));
	n = pl_leer(h, d, cabe - 1);
	pl_cerrar(h);
	if (n < 0)
		n = 0;
	d[n] = 0;
	return n;
}

/* ------------------------------------------------------------ las firmas */
/* (RÁPIDO) lo libre de la memoria del equipo, en K (para motor.c) */
static u32 libre_kb(void) { return sc(SC_DATO, 0, 0, 0) * 4; }

int carga_firmas(void)
{
	char c[RUTA], r[RUTA], largo[256];
	struct entrada e;
	int h, pasada;
	u32 t0 = pl_ms();
	if (firmas_cargadas)
		return 0;
	memset(&inf, 0, sizeof inf);
	motor_inicia();
	motor_memoria_libre = libre_kb;
	ruta_en(c, dir_escudo, "FIRMAS");
	/* (RÁPIDO) primero lo más nuevo (lo del día y del mes), luego la base:
	 * si la memoria no da para todo, lo que no cabe se deja (motor.c lo
	 * mira antes de cargar) y queda lo más reciente */
	{
		static const char *const orden[3] = { "DIA.ESC", "MES.ESC", "BASE.ESC" };
		int k;
		for (k = 0; k < 3; k++) {
			ruta_en(r, c, orden[k]);
			if (existe(r) && motor_carga_como(r, orden[k], &inf) < 0) {
				serie("escudo: %s", inf.error);
				apunta("%s", inf.error);
			}
		}
	}
	/* luego los otros FIRMAS.ESC, y lo demás (firmas en texto) */
	for (pasada = 0; pasada < 2; pasada++) {
		h = abre_carpeta(c);
		if (h < 0)
			break;
		while (entrada_siguiente(h, &e, largo)) {
			const char *x = strrchr(largo, '.');
			int es_esc = x && !strncasecmp_es(x, ".ESC", 4);
			if (e.atributos & (AT_CARPETA | AT_VOLUMEN) || es_esc != (pasada == 0))
				continue;
			if (es_esc && (!strncasecmp_es(e.nombre, "DIA.ESC", 8) || !strncasecmp_es(e.nombre, "MES.ESC", 8) ||
				       !strncasecmp_es(e.nombre, "BASE.ESC", 9)))
				continue;	/* (ya cargados) */
			ruta_en(r, c, e.nombre);
			if (motor_carga_como(r, largo, &inf) < 0)
				serie("escudo: %s", inf.error);
		}
		pl_cerrar(h);
	}
	motor_prepara(&inf);
	firmas_cargadas = 1;
	serie("escudo: firmas cargadas en %u ms", pl_ms() - t0);
	return 0;
}

static u32 dias_desde_1970(u32 f)
{
	i32 y = (i32)(f / 10000), m = (i32)(f / 100 % 100), d = (i32)(f % 100), era, yoe, doy, doe;
	if (m <= 2)
		y--;
	era = y / 400;
	yoe = y - era * 400;
	doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
	doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
	return (u32)(era * 146097 + doe - 719468);
}

u32 dias_firmas(void)
{
	u32 h = fecha_hoy();
	if (!inf.fecha)
		return 0xFFFF;
	if (h < inf.fecha)
		return 0;
	return dias_desde_1970(h) - dias_desde_1970(inf.fecha);
}

void texto_estado(char *d, int cabe, int *bien)
{
	u32 dias = dias_firmas(), total = inf.sumas + inf.cuerpo + inf.logicas;
	if (!total) {
		formatear(d, cabe, "Sin firmas de virus todavía: pulse «Actualizar firmas». (Solo reconoce el archivo de prueba EICAR.)");
		*bien = 0;
	} else if (!inf.fecha) {
		formatear(d, cabe, "Firmas sin fecha (propias o de prueba): %S. Las de ClamAV se traen con «Actualizar firmas».", total);
		*bien = 0;
	} else if (dias > 7) {
		formatear(d, cabe, "Firmas del %02u/%02u/%u: tienen %u días. Conviene actualizarlas. (%S firmas)", inf.fecha % 100,
			  inf.fecha / 100 % 100, inf.fecha / 10000, dias, total);
		*bien = 0;
	} else {
		formatear(d, cabe, "Firmas al día: del %02u/%02u/%u (%S firmas de ClamAV).", inf.fecha % 100,
			  inf.fecha / 100 % 100, inf.fecha / 10000, total);
		*bien = 1;
	}
}

/* ------------------------------------------------------------ revisar */
static int lista_todo;
static int en_cuarentena_ya;		/* /CUARENTENA: poner lo encontrado */

static void avanza(void)
{
	if (al_avanzar)
		al_avanzar();
}

static void nuevo_hallado(const char *ruta, const char *nombre, const char *dentro, int aviso)
{
	hallado *h;
	if (rev.n_hallados >= MAX_HALLADOS)
		return;
	h = &rev.h[rev.n_hallados++];
	memset(h, 0, sizeof *h);
	formatear(h->ruta, RUTA, "%s", ruta);
	formatear(h->nombre, sizeof h->nombre, "%s", nombre);
	formatear(h->dentro, sizeof h->dentro, "%s", dentro);
	h->aviso = aviso;
}

/* las carpetas que no se miran: la cuarentena y las firmas */
static int se_salta(const char *ruta)
{
	char f[RUTA];
	ruta_en(f, dir_escudo, "FIRMAS");
	return !strncasecmp_es(ruta, dir_cuarentena, (u32)strlen(dir_cuarentena) + 1) ||
	       !strncasecmp_es(ruta, f, (u32)strlen(f) + 1);
}

/* (RÁPIDO) el archivo que más ha tardado (va al registro: si una revisión es
 * lenta en un equipo, se ve dónde) */
static char lento_ruta[RUTA];
static u32 lento_ms;

static void revisa_uno(const char *ruta, const char *largo, u32 tam)
{
	resultado r;
	const char *nombre = largo && largo[0] ? largo : ruta;
	int x;
	u32 t0;
	formatear(rev.actual, RUTA, "%s", ruta);
	avanza();
	t0 = pl_ms();
	x = motor_revisa_archivo(ruta, nombre, &r);
	if (pl_ms() - t0 > lento_ms) {
		lento_ms = pl_ms() - t0;
		formatear(lento_ruta, RUTA, "%s (%u K)", ruta, tam / 1024);
	}
	if (x < 0) {
		rev.errores++;
		if (lista_todo)
			escribef("%s: no se puede abrir\n", ruta);
		return;
	}
	rev.revisados++;
	rev.octetos += tam;
	rev.sin_mirar += (u32)r.sin_mirar;
	rev.cifrados += (u32)r.cifrados;
	if (r.cortado)
		rev.cortada = 1;
	if (r.hallado) {
		nuevo_hallado(ruta, r.nombre, r.dentro, 0);
		apunta("encontrado: %s%s%s: %s", ruta, r.dentro[0] ? " > " : "", r.dentro, r.nombre);
		escribef("%s%s%s: %s ENCONTRADO\n", ruta, r.dentro[0] ? " > " : "", r.dentro, r.nombre);
		if (op.cuarentena_sola || en_cuarentena_ya) {
			if (!a_cuarentena(ruta, r.nombre))
				rev.h[rev.n_hallados - 1].hecho = 1;
		}
	} else if (lista_todo)
		escribef("%s: limpio%s\n", ruta, r.cifrados ? " (con partes cifradas sin mirar)" : "");
}

/* recorre una carpeta: «contar» solo cuenta (para la barra) */
static void recorre(const char *carpeta, int prof, int contar)
{
	char r[RUTA], largo[256];
	struct entrada e;
	int h;
	if (prof > 24 || se_salta(carpeta))
		return;
	h = abre_carpeta(carpeta);
	if (h < 0) {
		rev.errores++;
		return;
	}
	while (entrada_siguiente(h, &e, largo)) {
		if (e.nombre[0] == '.' && (!e.nombre[1] || (e.nombre[1] == '.' && !e.nombre[2])))
			continue;
		if (e.atributos & AT_VOLUMEN)
			continue;
		if (strlen(carpeta) + strlen(e.nombre) + 2 >= RUTA)
			continue;
		formatear(r, RUTA, "%s%s%s", carpeta, carpeta[strlen(carpeta) - 1] == '\\' ? "" : "\\", e.nombre);
		if (e.atributos & AT_CARPETA) {
			recorre(r, prof + 1, contar);
		} else if (contar) {
			rev.contados++;
			rev.octetos_total += e.tamano;
			if ((rev.contados & 63) == 0)
				avanza();
		} else {
			/* AUTORUN.INF en la raíz: en Hispa-Pentium no hace nada; en un
			 * Windows viejo podría lanzar un programa al meter la memoria */
			if (prof == 0 && !strncasecmp_es(e.nombre, "AUTORUN.INF", 12))
				nuevo_hallado(r, "AUTORUN.INF", "", 1);
			revisa_uno(r, largo, e.tamano);
		}
		if (pl_cortar()) {
			rev.cortada = 1;
			break;
		}
	}
	pl_cerrar(h);
}

static void revisa_ruta(const char *ruta)
{
	char r[RUTA];
	formatear(r, RUTA, "%s", ruta);
	if (strlen(r) == 2 && r[1] == ':')	/* «X:» es la raíz */
		formatear(r, RUTA, "%s\\", ruta);
	if (es_carpeta(r)) {
		rev.contando = 1;
		recorre(r, 0, 1);
		rev.contando = 0;
		if (!rev.cortada)
			recorre(r, 0, 0);
	} else {
		int h = pl_abrir(r);
		u32 tam = 0;
		if (h >= 0) {
			tam = pl_largo(h);
			pl_cerrar(h);
		}
		rev.contados++;
		rev.octetos_total += tam;
		revisa_uno(r, 0, tam);
	}
}

void revisa(const char *ruta, int lista)
{
	memset(&rev, 0, sizeof rev);
	formatear(rev.raiz, RUTA, "%s", ruta);
	rev.empezo_ms = pl_ms();
	lento_ms = 0, lento_ruta[0] = 0;
	lista_todo = lista;
	carga_firmas();
	if (!strcmp(ruta, "*")) {	/* todas las unidades */
		struct info_unidad u;
		u32 i;
		for (i = 0; info_unidad(i, &u) && !rev.cortada; i++) {
			char x[4];
			if (!u.letra)
				continue;
			formatear(x, sizeof x, "%c:", u.letra);
			revisa_ruta(x);
		}
	} else
		revisa_ruta(ruta);
	rev.acabada = 1;
	rev.ms = pl_ms() - rev.empezo_ms;
	apunta("revisado %s%s %u archivos (%u K) en %u s; %u encontrados%s", ruta, ruta[strlen(ruta) - 1] == ':' ? "" : ":",
	       rev.revisados, rev.octetos / 1024,
	       rev.ms / 1000, rev.n_hallados, rev.cortada ? " (cancelado)" : "");
	serie("escudo: %u octetos en %u ms", rev.octetos, rev.ms);
	if (lento_ms >= 2000)
		apunta("lo que más ha tardado: %s, %u ms", lento_ruta, lento_ms);
	{
		extern u32 motor_disparos, motor_leidas, motor_ms_sumas, motor_ms_disparos, motor_ms_candidatas, motor_bloques;
		serie("escudo: tiempos: sumas %u ms, disparadores %u ms (%u saltan), firmas del disco %u ms (%u leídas, %u bloques)",
		      motor_ms_sumas, motor_ms_disparos, motor_disparos, motor_ms_candidatas, motor_leidas, motor_bloques);
	}
	avanza();
}

/* ------------------------------------------------------------ la cuarentena
 * \HISPA\CUARENTENA\Qnnnnnnn.CUA: el archivo con sus octetos cambiados (XOR
 * 0xA5: así no se abre ni se ejecuta por error, ni lo vuelve a «encontrar»
 * nadie) e INDICE.TXT, una línea por archivo:
 *     Qnnnnnnn.CUA|ruta de donde salió|nombre de la firma|AAAA-MM-DD HH:MM|octetos */
void lee_cuarentena(void)
{
	static char t[48 * 1024];
	char r[RUTA], *p, *q;
	int n;
	n_cua = 0;
	ruta_en(r, dir_cuarentena, "INDICE.TXT");
	n = lee_todo(r, t, sizeof t - 1);
	if (n <= 0)
		return;
	t[n] = 0;
	for (p = t; *p && n_cua < MAX_CUARENTENA; p = q) {
		char *campo[5];
		int k = 0;
		q = p;
		while (*q && *q != '\n')
			q++;
		if (*q)
			*q++ = 0;
		if (q > p + 1 && q[-2] == '\r')
			q[-2] = 0;
		campo[k++] = p;
		while (k < 5 && (p = strchr(p, '|'))) {
			*p++ = 0;
			campo[k++] = p;
		}
		if (k < 5)
			continue;
		formatear(cua[n_cua].archivo, sizeof cua[0].archivo, "%s", campo[0]);
		formatear(cua[n_cua].ruta, RUTA, "%s", campo[1]);
		formatear(cua[n_cua].nombre, sizeof cua[0].nombre, "%s", campo[2]);
		formatear(cua[n_cua].cuando, sizeof cua[0].cuando, "%s", campo[3]);
		cua[n_cua].tam = numero(campo[4]);
		n_cua++;
	}
}

static int escribe_indice(void)
{
	char r[RUTA], l[600];
	u32 i;
	int h;
	ruta_en(r, dir_cuarentena, "INDICE.TXT");
	h = pl_crear(r);
	if (h < 0)
		return -1;
	for (i = 0; i < n_cua; i++) {
		int n = formatear(l, sizeof l, "%s|%s|%s|%s|%u\r\n", cua[i].archivo, cua[i].ruta, cua[i].nombre, cua[i].cuando,
				  cua[i].tam);
		pl_escribir(h, l, (u32)n);
	}
	pl_cerrar(h);
	return 0;
}

/* copia cambiando los octetos (XOR 0xA5): para entrar y para salir */
static int copia_xor(const char *de, const char *a)
{
	static u8 b[16384];
	int h = pl_abrir(de), g, n, bien = 1;
	if (h < 0)
		return -1;
	g = pl_crear(a);
	if (g < 0) {
		pl_cerrar(h);
		return -1;
	}
	while ((n = pl_leer(h, b, sizeof b)) > 0) {
		int i;
		for (i = 0; i < n; i++)
			b[i] ^= 0xA5;
		if (pl_escribir(g, b, (u32)n) != n) {
			bien = 0;
			break;
		}
	}
	if (n < 0)
		bien = 0;
	pl_cerrar(h);
	pl_cerrar(g);
	if (!bien)
		borrar(a);
	return bien ? 0 : -1;
}

int a_cuarentena(const char *ruta, const char *nombre)
{
	char a[RUTA], cual[16];
	u32 i, mayor = 0, d = fecha_hoy(), hh = hora_ahora(), tam = 0;
	int h;
	en_cuarentena *c;
	hacer_carpeta(dir_cuarentena);
	lee_cuarentena();
	if (n_cua >= MAX_CUARENTENA)
		return -1;
	for (i = 0; i < n_cua; i++) {
		u32 k = numero(cua[i].archivo + 1);
		if (k > mayor)
			mayor = k;
	}
	formatear(cual, sizeof cual, "Q%07u.CUA", mayor + 1);
	ruta_en(a, dir_cuarentena, cual);
	h = pl_abrir(ruta);
	if (h >= 0) {
		tam = pl_largo(h);
		pl_cerrar(h);
	}
	if (copia_xor(ruta, a)) {
		apunta("no se puede poner en cuarentena %s (¿no queda sitio?)", ruta);
		return -1;
	}
	if (borrar(ruta)) {
		borrar(a);
		apunta("no se puede quitar %s de su sitio (¿es de solo lectura?)", ruta);
		return -1;
	}
	c = &cua[n_cua++];
	formatear(c->archivo, sizeof c->archivo, "%s", cual);
	formatear(c->ruta, RUTA, "%s", ruta);
	formatear(c->nombre, sizeof c->nombre, "%s", nombre);
	formatear(c->cuando, sizeof c->cuando, "%u-%02u-%02u %02u:%02u", d / 10000, d / 100 % 100, d % 100, hh / 100, hh % 100);
	c->tam = tam;
	escribe_indice();
	apunta("en cuarentena: %s (%s) -> %s", ruta, nombre, cual);
	return 0;
}

int restaura(u32 i, char *donde)
{
	char a[RUTA], destino[RUTA];
	u32 k;
	lee_cuarentena();
	if (i >= n_cua)
		return -1;
	ruta_en(a, dir_cuarentena, cua[i].archivo);
	formatear(destino, RUTA, "%s", cua[i].ruta);
	/* si ya hay otro con ese nombre, al lado con otro: NOMBRE.R01... */
	for (k = 1; existe(destino) && k < 100; k++) {
		char *p = strrchr(destino, '.'), *b = strrchr(destino, '\\');
		if (p && b && p > b)
			*p = 0;
		formatear(destino + strlen(destino), RUTA - (int)strlen(destino), ".R%02u", k);
	}
	if (copia_xor(a, destino)) {
		apunta("no se puede devolver %s a %s", cua[i].archivo, destino);
		return -1;
	}
	borrar(a);
	apunta("restaurado de la cuarentena: %s (%s)", destino, cua[i].nombre);
	if (donde)
		formatear(donde, RUTA, "%s", destino);
	for (k = i; k + 1 < n_cua; k++)
		cua[k] = cua[k + 1];
	n_cua--;
	escribe_indice();
	return 0;
}

int borra_de_cuarentena(u32 i)
{
	char a[RUTA];
	u32 k;
	lee_cuarentena();
	if (i >= n_cua)
		return -1;
	ruta_en(a, dir_cuarentena, cua[i].archivo);
	borrar(a);
	apunta("borrado de la cuarentena: %s (%s, de %s)", cua[i].archivo, cua[i].nombre, cua[i].ruta);
	for (k = i; k + 1 < n_cua; k++)
		cua[k] = cua[k + 1];
	n_cua--;
	escribe_indice();
	return 0;
}

int borra_archivo(const char *ruta, const char *nombre)
{
	if (borrar(ruta))
		return -1;
	apunta("borrado: %s (%s)", ruta, nombre);
	return 0;
}

/* ------------------------------------------------------------ (CORTAFUEGOS)
 * Las reglas van en CORTAFUE.INI (las escribe esto; RED.BIN las lee al
 * arrancar y cuando se le pide: rcorta.cas); lo que pasa en la red lo dice
 * RED.BIN con O_CORTAFUEGOS (28): su informe, en texto. */
cortafuegos cf;
#define O_CORTAFUEGOS 28
#define SC_RED 90
#define SC_ESTADO_RED 97

/* un pedido al servicio de red (como redes.cas: la llamada 90) */
static u32 pide_red(u32 orden, u32 a, u32 b, void *datos, u32 cabe)
{
	u32 cab[8];
	memset(cab, 0, sizeof cab);
	cab[0] = orden, cab[1] = a, cab[2] = b;
	return sc(SC_RED, (u32)cab, (u32)datos, cabe);
}

/* ¿está el servicio de red? (sin ponerlo en marcha: la 97 solo pregunta) */
static int hay_servicio_red(void)
{
	u32 ip;
	return sc(SC_ESTADO_RED, (u32)&ip, 0, 0) != 0;
}

const char *nombre_puerto(u32 p)
{
	switch (p) {
	case 21: return "FTP";
	case 22: return "SSH";
	case 23: return "Telnet";
	case 25: return "correo";
	case 53: return "DNS";
	case 80: return "web";
	case 443: return "web segura";
	case 445: return "carpetas de Windows";
	case 3389: return "escritorio remoto";
	case 41641: return "Tailscale";
	case 51820: return "WireGuard";
	}
	return "";
}

const char *texto_de(u32 de)
{
	switch (de) {
	case CF_TODOS: return "desde cualquier sitio";
	case CF_RED: return "desde la red local y Tailscale";
	case CF_LOCAL: return "solo desde la red local";
	case CF_TAILSCALE: return "solo desde Tailscale";
	case CF_IP: return "desde una red";
	}
	return "no entra";
}

static void ruta_cf(char *r)
{
	ruta_en(r, dir_escudo, "CORTAFUE.INI");
}

static u32 de_texto(const char *p)
{
	if (!strncasecmp_es(p, "todos", 5))
		return CF_TODOS;
	if (!strncasecmp_es(p, "red", 3))
		return CF_RED;
	if (!strncasecmp_es(p, "local", 5))
		return CF_LOCAL;
	if (!strncasecmp_es(p, "tailscale", 9))
		return CF_TAILSCALE;
	if (*p >= '0' && *p <= '9')
		return CF_IP;
	return CF_NADIE;
}

void lee_cortafuegos(void)
{
	static char t[8192];
	char r[RUTA], *p;
	int n;
	/* lo de fábrica (como RED.BIN sin el archivo) */
	memset(&cf, 0, sizeof cf);
	cf.activo = 1, cf.avisar = 1, cf.ping = 1;
	cf.reglas[0].puerto = 22, cf.reglas[0].de = CF_RED, cf.n_reglas = 1;
	ruta_cf(r);
	n = lee_todo(r, t, sizeof t - 1);
	if (n > 0) {
		t[n] = 0;
		cf.n_reglas = 0;
		for (p = t; *p;) {
			char *q = p;
			while (*q && *q != '\n')
				q++;
			if (!strncasecmp_es(p, "activo=", 7))
				cf.activo = (p[7] | 32) != 'n';
			else if (!strncasecmp_es(p, "avisar=", 7))
				cf.avisar = (p[7] | 32) != 'n';
			else if (!strncasecmp_es(p, "ping=", 5))
				cf.ping = (p[5] | 32) == 's' ? 2 : (p[5] | 32) == 'r' ? 1 : 0;
			else if (!strncasecmp_es(p, "regla=tcp ", 10) && cf.n_reglas < MAX_CF) {
				regla_cf *g = &cf.reglas[cf.n_reglas];
				char *x = p + 10;
				g->puerto = numero(x);
				while (*x >= '0' && *x <= '9')
					x++;
				while (*x == ' ')
					x++;
				g->de = de_texto(x);
				if (g->de == CF_IP) {
					int k = 0;
					while (k < 23 && x[k] && x[k] != '\r' && x[k] != '\n' && x[k] != ' ')
						g->ip[k] = x[k], k++;
					g->ip[k] = 0;
				}
				if (g->puerto)
					cf.n_reglas++;
			}
			p = *q ? q + 1 : q;
		}
	}
	/* lo de RED.BIN */
	cf.hay_red = 0;
	if (hay_servicio_red()) {
		u32 m = pide_red(O_CORTAFUEGOS, 1, 0, t, sizeof t - 1);
		if (!ES_ERROR(m) && m < 0xFFFFFFE0u && m < sizeof t) {
			t[m] = 0;
			cf.hay_red = 1;
			for (p = t; *p;) {
				char *q = p;
				while (*q && *q != '\n')
					q++;
				if (!strncasecmp_es(p, "bloqueados=", 11))
					cf.bloqueados = numero(p + 11);
				else if (!strncasecmp_es(p, "escucha=", 8) && cf.n_escuchas < MAX_CF) {
					escucha_cf *e = &cf.escuchas[cf.n_escuchas++];
					char *x = p + 12;
					e->udp = (p[8] | 32) == 'u';
					e->puerto = numero(x);
					while (*x >= '0' && *x <= '9')
						x++;
					e->tarea = numero(x + 1);
				} else if (!strncasecmp_es(p, "pendiente=", 10) && cf.n_pendientes < 4) {
					escucha_cf *e = &cf.pendientes[cf.n_pendientes++];
					char *x = p + 10;
					e->puerto = numero(x);
					while (*x >= '0' && *x <= '9')
						x++;
					e->tarea = numero(x + 1);
				} else if (!strncasecmp_es(p, "bloqueado=", 10) && cf.n_ultimos < 8) {
					bloqueo_cf *b = &cf.ultimos[cf.n_ultimos++];
					char *x = p + 10;
					int k = 0;
					b->hora = numero(x);
					while (*x && *x != ' ')
						x++;
					b->proto = numero(++x);
					while (*x && *x != ' ')
						x++;
					x++;
					while (k < 15 && *x && *x != ' ')
						b->ip[k++] = *x++;
					b->ip[k] = 0;
					b->puerto = numero(x + 1);
				}
				p = *q ? q + 1 : q;
			}
		}
	}
}

int guarda_cortafuegos(void)
{
	char t[2048], r[RUTA];
	int n;
	u32 i;
	static const char *const de[] = { "nadie", "todos", "red", "local", "tailscale" };
	hacer_carpeta(dir_escudo);
	n = formatear(t, sizeof t,
		      "; El cortafuegos de Hispa-Pentium (lo escribe Escudo; lo lee RED.BIN)\r\n"
		      "activo=%s\r\nping=%s\r\navisar=%s\r\n",
		      cf.activo ? "si" : "no", cf.ping == 2 ? "si" : cf.ping == 1 ? "red" : "no", cf.avisar ? "si" : "no");
	for (i = 0; i < cf.n_reglas; i++)
		n += formatear(t + n, (int)sizeof t - n, "regla=tcp %u %s\r\n", cf.reglas[i].puerto,
			       cf.reglas[i].de == CF_IP ? cf.reglas[i].ip : de[cf.reglas[i].de < 5 ? cf.reglas[i].de : 0]);
	ruta_cf(r);
	if (escribe_todo(r, t, (u32)n))
		return -1;
	if (hay_servicio_red())
		pide_red(O_CORTAFUEGOS, 0, 0, 0, 0);
	apunta("cortafuegos: %s; ping %s; %u reglas", cf.activo ? "encendido" : "APAGADO",
	       cf.ping == 2 ? "de todos" : cf.ping == 1 ? "de la red" : "de nadie", cf.n_reglas);
	return 0;
}

void pon_regla_cf(u32 puerto, u32 de)
{
	u32 i;
	for (i = 0; i < cf.n_reglas && cf.reglas[i].puerto != puerto; i++) ;
	if (i == cf.n_reglas) {
		if (cf.n_reglas >= MAX_CF)
			return;
		cf.n_reglas++;
	}
	cf.reglas[i].puerto = puerto;
	cf.reglas[i].de = de;
	cf.reglas[i].ip[0] = 0;
	guarda_cortafuegos();
	if (hay_servicio_red())
		pide_red(O_CORTAFUEGOS, 2, puerto, 0, 0);
	apunta("cortafuegos: el puerto TCP %u%s%s: %s", puerto, nombre_puerto(puerto)[0] ? " de " : "", nombre_puerto(puerto),
	       texto_de(de));
}

int ssh_encendido(void)
{
	char r[RUTA], t[512], v[16];
	int n;
	formatear(r, RUTA, "%c:\\HISPA\\SSH\\SSHD.INI", unidad_sistema());
	n = lee_todo(r, t, sizeof t - 1);
	if (n <= 0)
		return 0;
	t[n] = 0;
	return valor_ini(t, "encendido", v, sizeof v) && v[0] == '1';
}

int apaga_ssh(void)
{
	char r[RUTA];
	u32 t, c;
	formatear(r, RUTA, "%c:\\HISPA\\SSHD.BIN", unidad_sistema());
	t = sc(SC_LANZAR, (u32)r, (u32)"/APAGAR", 0);
	if (ES_ERROR(t))
		return -1;
	c = sc(SC_ESPERAR, t, 0, 0);
	apunta("SSH apagado desde Escudo (SSHD /APAGAR: %u)", c);
	return c == 0 ? 0 : -1;
}

/* las firmas nuevas: los paquetes «escbase» (de main.cvd), «escmes» (de
 * daily.cvd, cada mes) y «escdia» (lo nuevo de cada día): los hace
 * tools/escudo_firmas.py en el VPS con las bases de ClamAV; el catálogo va
 * firmado. La primera vez, PAQUETES INSTALA escdia (trae los otros dos, que
 * necesita); luego, PAQUETES ACTUALIZA de los tres */
u32 act_hecho, act_total;
char act_texto[100], act_dice[200];
int act_cancelando;

/* «hecho total texto» de C:\HISPA\PAQUETES\AVANCE.TXT */
static void lee_avance_paquetes(const char *ruta)
{
	char t[160], *p;
	int n = lee_todo(ruta, t, sizeof t - 1);
	u32 h = 0, tot = 0;
	if (n < 5)
		return;
	t[n] = 0;
	for (p = t; *p >= '0' && *p <= '9'; p++)
		h = h * 10 + (u32)(*p - '0');
	while (*p == ' ')
		p++;
	for (; *p >= '0' && *p <= '9'; p++)
		tot = tot * 10 + (u32)(*p - '0');
	while (*p == ' ')
		p++;
	act_hecho = h, act_total = tot;
	formatear(act_texto, sizeof act_texto, "%s", p);
	for (p = act_texto; *p; p++)
		if (*p == '\r' || *p == '\n')
			*p = 0;
}

int actualiza_firmas_con(int (*cada)(void))
{
	char r[RUTA], q[RUTA], a[RUTA], av[RUTA], ca[RUTA], sal[RUTA];
	const char *orden;
	u32 t, c;
	int m = -1;
	formatear(r, RUTA, "%c:\\HISPA\\PAQUETES.BIN", unidad_sistema());
	if (!existe(r))
		return -2;
	formatear(q, RUTA, "%c:\\HISPA\\PAQUETES\\ESCDIA.PQI", unidad_sistema());
	orden = existe(q) ? "ACTUALIZA escbase escmes escdia" : "INSTALA escdia";
	op.ultima_act = fecha_hoy();
	guarda_opciones();
	act_hecho = act_total = 0, act_cancelando = 0;
	act_texto[0] = act_dice[0] = 0;
	if (!cada) {
		t = sc(SC_LANZAR, (u32)r, (u32)orden, 0);
		if (ES_ERROR(t))
			return -1;
		c = sc(SC_ESPERAR, t, 0, 0);
	} else {
		/* (RÁPIDO) con /AVANCE: la barra y «Cancelar» (CANCELA.TXT: PAQUETES
		 * para en cuanto puede y deja todo como estaba); lo que dice, a
		 * \ESCUDO\PAQUETES.TXT, para enseñar lo último (p. ej. que no cabe) */
		formatear(a, RUTA, "%s /AVANCE", orden);
		formatear(av, RUTA, "%c:\\HISPA\\PAQUETES\\AVANCE.TXT", unidad_sistema());
		formatear(ca, RUTA, "%c:\\HISPA\\PAQUETES\\CANCELA.TXT", unidad_sistema());
		ruta_en(sal, dir_escudo, "PAQUETES.TXT");
		borrar(ca);
		m = pl_crear(sal);
		if (m >= 0)
			sc(SC_REDIRIGIR, (u32)m, 0, 0);
		t = sc(SC_LANZAR, (u32)r, (u32)a, 0);
		if (m >= 0)
			sc(SC_REDIRIGIR, 0xFFFFFFFFu, 0, 0);
		if (ES_ERROR(t)) {
			if (m >= 0)
				pl_cerrar(m);
			return -1;
		}
		while ((c = sc(SC_ESPERAR, t, 1, 0)) == SIGUE) {
			lee_avance_paquetes(av);
			if (cada() && !act_cancelando) {
				int h = pl_crear(ca);
				if (h >= 0)
					pl_cerrar(h);
				act_cancelando = 1;
			}
			dormir(hz() / 10 ? hz() / 10 : 1);
		}
		if (m >= 0) {
			static char tx[4096];
			int n;
			char *p, *ult = 0;
			pl_cerrar(m);
			n = lee_todo(sal, tx, sizeof tx - 1);
			if (n > 0) {
				tx[n] = 0;
				for (p = tx; *p; p++)
					if (*p == '\r')
						*p = ' ';
				/* la última línea con algo de «paquetes: » */
				for (p = tx; (p = strstr(p, "paquetes: ")); p++)
					ult = p + 10;
				if (ult) {
					char *e = strchr(ult, '\n');
					if (e)
						*e = 0;
					formatear(act_dice, sizeof act_dice, "%s", ult);
				}
			}
		}
		borrar(ca);
	}
	apunta("actualizar las firmas (PAQUETES %s): %s%s%s", orden, c == 0 ? "hecho" : act_cancelando ? "cancelado" : "no ha podido",
	       act_dice[0] ? ": " : "", act_dice);
	if (c == 0) {
		motor_suelta();
		firmas_cargadas = 0;
	}
	return c == 0 ? 0 : act_cancelando ? -3 : -1;
}

int actualiza_firmas(void)
{
	return actualiza_firmas_con(0);
}

/* ------------------------------------------------------------ vigilar
 * Cada segundo mira si han cambiado las unidades (la llamada 131); por cada
 * una nueva que se pueda quitar (una memoria USB) o que sea un CD, si la
 * opción está puesta, lanza «ESCUDO /PREGUNTA X:». Y una vez al día, si se
 * quiere, trae las firmas nuevas. */
static int vigilar(void)
{
	char vistas[32], ahora[32];
	u32 cambios = cambios_unidades(), i, preguntado = 0, bloq_vistos = 0;
	struct info_unidad u;
	int nv = 0;
	serie("escudo: vigilando las unidades (memorias USB y CD)");
	if (hay_servicio_red()) {	/* (lo bloqueado antes de arrancar Escudo no se cuenta otra vez) */
		lee_cortafuegos();
		bloq_vistos = cf.bloqueados;
	}
	for (i = 0; info_unidad(i, &u) && nv < 31; i++)
		if (u.letra)
			vistas[nv++] = (char)u.letra;
	vistas[nv] = 0;
	for (;;) {
		u32 c;
		dormir(hz());
		c = cambios_unidades();
		if (c != cambios) {
			int na = 0;
			cambios = c;
			lee_opciones();
			for (i = 0; info_unidad(i, &u) && na < 31; i++) {
				if (!u.letra)
					continue;
				ahora[na++] = (char)u.letra;
				if (strchr(vistas, u.letra))
					continue;
				serie("escudo: unidad nueva %c: (%s, medio %u)", u.letra, u.etiqueta, u.medio);
				if (op.preguntar && !u.del_sistema && (u.quitable || u.medio == MEDIO_USB || u.medio > MEDIO_USB)) {
					char a[16];
					formatear(a, sizeof a, "/PREGUNTA %c:", u.letra);
					sc(SC_LANZAR_SUELTO, (u32)prog_escudo, (u32)a, 0);
				}
			}
			ahora[na] = 0;
			memcpy(vistas, ahora, (u32)na + 1);
		}
		/* (CORTAFUEGOS) lo que dice RED.BIN: un programa que quiere recibir
		 * conexiones (se pregunta: ESCUDO /PERMITIR) y lo bloqueado (al registro) */
		if (hay_servicio_red() && (tics() / hz()) % 2 == 0) {
			u32 k;
			lee_cortafuegos();
			for (k = 0; k < cf.n_pendientes; k++)
				if (cf.pendientes[k].puerto != preguntado) {
					char a[32];
					preguntado = cf.pendientes[k].puerto;
					formatear(a, sizeof a, "/PERMITIR %u %u", cf.pendientes[k].puerto, cf.pendientes[k].tarea);
					sc(SC_LANZAR_SUELTO, (u32)prog_escudo, (u32)a, 0);
				}
			if (cf.bloqueados > bloq_vistos) {
				u32 nuevos = cf.bloqueados - bloq_vistos;
				for (k = 0; k < nuevos && k < cf.n_ultimos && k < 4; k++) {
					bloqueo_cf *b = &cf.ultimos[k];
					if (b->proto == 6)
						apunta("cortafuegos: bloqueada una conexión TCP desde %s al puerto %u", b->ip, b->puerto);
					else
						apunta("cortafuegos: bloqueado un ping desde %s", b->ip);
				}
				if (nuevos > 4)
					apunta("cortafuegos: y %u más", nuevos - 4);
				bloq_vistos = cf.bloqueados;
			}
		}
		/* las firmas, una vez al día (como mucho: el día que no se ha intentado) */
		if (op.actualizar_solo && op.ultima_act != fecha_hoy() && (tics() / hz()) % 600 == 300) {
			lee_opciones();
			if (op.ultima_act != fecha_hoy())
				actualiza_firmas();
		}
	}
	return 0;
}

/* ------------------------------------------------------------ las órdenes */
/* ---- lo ya revisado (para /ANTES: no cargar todas las firmas cada vez que se
 * abre el mismo programa): «MD5 tamaño firmas» de lo que ha salido limpio,
 * en REVISADO.TXT (los últimos 256); «firmas»: lo que miden DIA.ESC y
 * MES.ESC (si cambian las firmas, se mira otra vez) ---- */
static int clave_revisado(const char *ruta, char *d)
{
	static u8 b[16384];
	md5_ctx m;
	u8 h[16];
	char r[RUTA];
	int f = pl_abrir(ruta), n, i;
	u32 tam, v = 0;
	if (f < 0)
		return 0;
	tam = pl_largo(f);
	md5_inicia(&m);
	while ((n = pl_leer(f, b, sizeof b)) > 0)
		md5_suma(&m, b, (u32)n);
	pl_cerrar(f);
	md5_acaba(&m, h);
	ruta_en(r, dir_escudo, "FIRMAS\\DIA.ESC");
	if ((f = pl_abrir(r)) >= 0)
		v += pl_largo(f), pl_cerrar(f);
	ruta_en(r, dir_escudo, "FIRMAS\\MES.ESC");
	if ((f = pl_abrir(r)) >= 0)
		v = v * 31 + pl_largo(f), pl_cerrar(f);
	for (i = 0; i < 16; i++)
		formatear(d + 2 * i, 3, "%02x", h[i]);
	formatear(d + 32, 40, " %u %u", tam, v);
	return 1;
}

static int ya_revisado(const char *clave)
{
	static char t[256 * 64];
	char r[RUTA];
	int n;
	ruta_en(r, dir_escudo, "REVISADO.TXT");
	n = lee_todo(r, t, sizeof t - 1);
	if (n <= 0)
		return 0;
	t[n] = 0;
	return strstr(t, clave) != 0;
}

static void apunta_revisado(const char *clave)
{
	static char t[256 * 64];
	char r[RUTA], *p;
	int n, lineas = 0;
	ruta_en(r, dir_escudo, "REVISADO.TXT");
	n = lee_todo(r, t, sizeof t - 1);
	if (n < 0)
		n = 0;
	t[n] = 0;
	for (p = t; *p; p++)
		if (*p == '\n')
			lineas++;
	p = t;
	while (lineas >= 255 && (p = strchr(p, '\n')))	/* (los más viejos, fuera) */
		p++, lineas--;
	if (!p)
		p = t;
	{
		char nuevo[96];
		int k = formatear(nuevo, sizeof nuevo, "%s\r\n", clave);
		int h = pl_crear(r);
		if (h >= 0) {
			pl_escribir(h, p, (u32)strlen(p));
			pl_escribir(h, nuevo, (u32)k);
			pl_cerrar(h);
		}
	}
}

static void ayuda(void)
{
	escribe("Escudo " ESCUDO_VERSION ", el antivirus y el cortafuegos de Hispa-Pentium (con las firmas de ClamAV).\n"
		"  ESCUDO RUTA...          revisa archivos, carpetas o unidades (X:); * revisa todo\n"
		"  ESCUDO /LISTA RUTA...   lo mismo, con una línea por archivo\n"
		"  ESCUDO /CUARENTENA RUTA revisa y pone en cuarentena lo que encuentre\n"
		"  ESCUDO /ESTADO          las firmas y la cuarentena\n"
		"  ESCUDO /ACTUALIZA       trae las firmas nuevas\n"
		"  ESCUDO /RESTAURA n      devuelve a su sitio lo n.º n de la cuarentena\n"
		"  ESCUDO /BORRA n         borra para siempre lo n.º n de la cuarentena\n"
		"  ESCUDO /CORTAFUEGOS     el cortafuegos: cómo está, lo que escucha y lo bloqueado\n"
		"  ESCUDO /CORTAFUEGOS 22 red   deja entrar al puerto TCP 22 (red, todos, local, tailscale o no)\n"
		"  ESCUDO /CORTAFUEGOS SI|NO    lo enciende o lo apaga\n"
		"  ESCUDO                  (con el escritorio) la ventana\n");
}

static void estado(void)
{
	char t[300];
	int bien;
	u32 i;
	carga_firmas();
	texto_estado(t, sizeof t, &bien);
	escribef("Escudo %s: %s %s\n", ESCUDO_VERSION, bien ? "[OK]" : "[!]", t);
	escribef("  Sumas: %S; del cuerpo: %S; lógicas: %S; que no se saben mirar: %S; en memoria: %S K.\n", inf.sumas,
		 inf.cuerpo, inf.logicas, inf.no_soportadas, inf.memoria_k);
	if (inf.version_daily || inf.version_main)
		escribef("  Versiones de ClamAV: daily %u, main %u, bytecode %u.\n", inf.version_daily, inf.version_main,
			 inf.version_bytecode);
	lee_cuarentena();
	escribef("En cuarentena (%s): %u.\n", dir_cuarentena, n_cua);
	for (i = 0; i < n_cua; i++)
		escribef("  %u. %s  %s  (de %s)\n", i + 1, cua[i].cuando, cua[i].nombre, cua[i].ruta);
}

static const char *arg_siguiente(const char **p, char *d, int cabe)
{
	int k = 0;
	const char *s = *p;
	while (*s == ' ')
		s++;
	if (!*s)
		return 0;
	if (*s == '"') {
		s++;
		while (*s && *s != '"' && k + 1 < cabe)
			d[k++] = *s++;
		if (*s == '"')
			s++;
	} else
		while (*s && *s != ' ' && k + 1 < cabe)
			d[k++] = *s++;
	d[k] = 0;
	*p = s;
	return d;
}

static int resumen_consola(void)
{
	u32 i;
	escribef("\nRevisados: %S archivos (%S K) en %u s. ", rev.revisados, rev.octetos / 1024, rev.ms / 1000);
	if (!rev.n_hallados)
		escribe("No se ha encontrado nada.\n");
	else {
		escribef("Encontrados: %u.\n", rev.n_hallados);
		for (i = 0; i < rev.n_hallados; i++)
			escribef("  %s %s%s%s: %s%s\n", rev.h[i].aviso ? "[i]" : "[!]", rev.h[i].ruta, rev.h[i].dentro[0] ? " > " : "",
				 rev.h[i].dentro, rev.h[i].aviso ? "aviso (en un Windows viejo podría lanzar un programa)" : rev.h[i].nombre,
				 rev.h[i].hecho == 1 ? " (en cuarentena)" : "");
	}
	if (rev.cifrados)
		escribef("Partes cifradas que no se han podido mirar: %u.\n", rev.cifrados);
	if (rev.cortada)
		escribe("(Cancelado antes de acabar.)\n");
	for (i = 0; i < rev.n_hallados; i++)
		if (!rev.h[i].aviso)
			return 1;
	return 0;
}

int escudo_principal(void)
{
	static char args[600];
	char a[RUTA];
	const char *p;
	u32 n = sc(SC_ARGUMENTOS, (u32)args, sizeof args, 0);
	int lista = 0, hecho = 0, malo = 0;
	if (ES_ERROR(n))
		args[0] = 0;
	busca_carpetas();
	lee_opciones();
	p = args;
	if (!arg_siguiente(&p, a, sizeof a)) {
		if (escritorio_en_marcha())
			return ventana_principal(0, 0);
		ayuda();
		return 0;
	}
	if (!strncasecmp_es(a, "/VIGILAR", 9))
		return vigilar();
	if (!strncasecmp_es(a, "/VENTANA", 9)) {
		int pest = arg_siguiente(&p, a, sizeof a) ? (int)numero(a) : 0;
		return ventana_principal(0, pest);
	}
	if (!strncasecmp_es(a, "/AYUDA", 7) || !strcmp(a, "/?")) {
		ayuda();
		return 0;
	}
	if (!strncasecmp_es(a, "/ESTADO", 8)) {
		estado();
		return 0;
	}
	if (!strncasecmp_es(a, "/ACTUALIZA", 11)) {
		int r = actualiza_firmas();
		escribe(r == 0 ? "Firmas al día.\n" : r == -2 ? "No está PAQUETES (el sistema no está instalado).\n" :
								"No se han podido traer las firmas (¿hay red?).\n");
		return r ? 2 : 0;
	}
	if (!strncasecmp_es(a, "/RESTAURA", 10) || !strncasecmp_es(a, "/BORRA", 7)) {
		int restaurar = (a[1] | 32) == 'r';
		char donde[RUTA];
		u32 k;
		if (!arg_siguiente(&p, a, sizeof a) || !(k = numero(a))) {
			escribe("Diga el número (ESCUDO /ESTADO los dice).\n");
			return 2;
		}
		if (restaurar ? restaura(k - 1, donde) : borra_de_cuarentena(k - 1)) {
			escribe("No se ha podido.\n");
			return 2;
		}
		if (restaurar)
			escribef("Devuelto a %s.\n", donde);
		else
			escribe("Borrado para siempre.\n");
		return 0;
	}
	if (!strncasecmp_es(a, "/PERMITIR", 10)) {	/* (CORTAFUEGOS) */
		u32 puerto, tarea = 0;
		int r;
		if (!arg_siguiente(&p, a, sizeof a))
			return 2;
		puerto = numero(a);
		if (arg_siguiente(&p, a, sizeof a))
			tarea = numero(a);
		lee_cortafuegos();
		r = pregunta_puerto(puerto, tarea);
		if (r >= 0)
			pon_regla_cf(puerto, (u32)r);
		else if (hay_servicio_red())
			pide_red(O_CORTAFUEGOS, 2, puerto, 0, 0);
		return 0;
	}
	if (!strncasecmp_es(a, "/CORTAFUEGOS", 13)) {	/* (CORTAFUEGOS) en la Consola: cómo está, o una regla */
		u32 k;
		lee_cortafuegos();
		if (arg_siguiente(&p, a, sizeof a)) {
			/* ESCUDO /CORTAFUEGOS PUERTO red|todos|local|tailscale|no ; o SI|NO (encenderlo, apagarlo) */
			if (!strncasecmp_es(a, "si", 3) || !strncasecmp_es(a, "no", 3)) {
				cf.activo = (a[0] | 32) == 's';
				guarda_cortafuegos();
			} else if (numero(a)) {
				u32 puerto = numero(a);
				char d[16];
				if (!arg_siguiente(&p, d, sizeof d)) {
					escribe("Diga de dónde: red, todos, local, tailscale o no.\n");
					return 2;
				}
				pon_regla_cf(puerto, (d[0] | 32) == 'n' ? CF_NADIE : de_texto(d));
			}
			lee_cortafuegos();
		}
		escribef("Cortafuegos: %s. Ping: %s. Bloqueado desde que arrancó la red: %u.\n",
			 cf.activo ? "encendido (no entra nada que no pida el equipo, salvo lo de las reglas)" : "APAGADO (entra todo)",
			 cf.ping == 2 ? "de todos" : cf.ping == 1 ? "de la red local y Tailscale" : "de nadie", cf.bloqueados);
		for (k = 0; k < cf.n_reglas; k++)
			escribef("  regla: TCP %u%s%s%s: %s\n", cf.reglas[k].puerto, nombre_puerto(cf.reglas[k].puerto)[0] ? " (" : "",
				 nombre_puerto(cf.reglas[k].puerto), nombre_puerto(cf.reglas[k].puerto)[0] ? ")" : "",
				 cf.reglas[k].de == CF_IP ? cf.reglas[k].ip : texto_de(cf.reglas[k].de));
		if (!cf.hay_red)
			escribe("  (el servicio de red no está en marcha)\n");
		for (k = 0; k < cf.n_escuchas; k++)
			escribef("  escucha: %s %u (la tarea %u)\n", cf.escuchas[k].udp ? "UDP" : "TCP", cf.escuchas[k].puerto, cf.escuchas[k].tarea);
		for (k = 0; k < cf.n_ultimos; k++)
			escribef("  bloqueado: %s desde %s%s\n", cf.ultimos[k].proto == 6 ? "TCP" : "ping", cf.ultimos[k].ip,
				 cf.ultimos[k].proto == 6 ? "" : "");
		return 0;
	}
	if (!strncasecmp_es(a, "/PREGUNTA", 10)) {
		struct info_unidad u;
		char que[120];
		u32 i;
		if (!arg_siguiente(&p, a, sizeof a))
			return 2;
		formatear(que, sizeof que, "la unidad %c:", a[0]);
		for (i = 0; info_unidad(i, &u); i++)
			if (u.letra == (u8)(a[0] & ~32))
				formatear(que, sizeof que, "%s %c:%s%s%s",
					  u.medio == MEDIO_USB || u.quitable ? "la memoria USB" : u.medio > MEDIO_USB ? "el CD o DVD" : "el disco",
					  u.letra, u.etiqueta[0] ? " (" : "", u.etiqueta, u.etiqueta[0] ? ")" : "");
		if (ventana_pregunta((char)(a[0] & ~32), que)) {
			char x[4];
			formatear(x, sizeof x, "%c:", a[0] & ~32);
			return ventana_principal(x, 0);
		}
		return 0;
	}
	if (!strncasecmp_es(a, "/DESCARGA", 10) || !strncasecmp_es(a, "/ANTES", 7)) {
		int antes = (a[1] | 32) == 'a';
		resultado r;
		if (!arg_siguiente(&p, a, sizeof a))
			return 2;
		if ((antes && !op.antes_exe) || (!antes && !op.descargas))
			return 0;
		{
			char clave[80];
			if (antes && clave_revisado(a, clave) && ya_revisado(clave)) {
				serie("escudo: antes de abrir %s: ya revisado, limpio", a);
				return 0;
			}
			carga_firmas();
			if (motor_revisa_archivo(a, a, &r) < 0)
				return 0;
			if (antes && !r.hallado && clave_revisado(a, clave))
				apunta_revisado(clave);
		}
		serie("escudo: %s %s: %s", antes ? "antes de abrir" : "descarga", a, r.hallado ? r.nombre : "limpio");
		if (!r.hallado)
			return 0;
		apunta("encontrado%s: %s%s%s: %s", antes ? " al ir a abrirlo" : " en una descarga", a, r.dentro[0] ? " > " : "",
		       r.dentro, r.nombre);
		{
			int x = op.cuarentena_sola ? H_CUARENTENA : ventana_hallazgo(a, r.nombre, r.dentro, antes);
			if (x == H_CUARENTENA)
				a_cuarentena(a, r.nombre);
			else if (x == H_BORRAR)
				borra_archivo(a, r.nombre);
			else if (x == H_ABRIR) {
				apunta("abierto de todos modos (lo ha pedido el usuario): %s", a);
				return 0;
			}
		}
		return 1;
	}
	if (!strncasecmp_es(a, "/LISTA", 7)) {
		lista = 1;
		if (!arg_siguiente(&p, a, sizeof a)) {
			ayuda();
			return 2;
		}
	} else if (!strncasecmp_es(a, "/CUARENTENA", 12)) {
		en_cuarentena_ya = 1;
		if (!arg_siguiente(&p, a, sizeof a)) {
			ayuda();
			return 2;
		}
	}
	/* revisar desde la Consola */
	carga_firmas();
	{
		char t[300];
		int bien;
		texto_estado(t, sizeof t, &bien);
		escribef("Escudo %s. %s %s\n", ESCUDO_VERSION, bien ? "[OK]" : "[!]", t);
	}
	sc(SC_ATENDER_CORTE, 1, 0, 0);
	do {
		escribef("Revisando %s...\n", a);
		revisa(a, lista);
		malo |= resumen_consola();
		hecho++;
	} while (!rev.cortada && arg_siguiente(&p, a, sizeof a));
	return malo ? 1 : 0;
}
