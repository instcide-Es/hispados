/* dx.c - WIN32.BIN: las cuatro funciones propias de USER32 que usan las DLL
 * de DirectX de Hispa-486 (w32/dx/: DDRAW, DINPUT, DSOUND, WINMM; las mismas,
 * sin tocar, salvo la hora de WINMM), hechas para Hispa-Pentium:
 *   HispaDXPantalla  la pantalla completa: la ventana del juego pasa a ser
 *                    una de Hispa-Pentium sin marco, encima de todo y del
 *                    tamaño de la pantalla (V_COMPLETA, win32.cas); su lienzo
 *                    de 32 bits es el «marco lineal» donde DDRAW pinta, y lo
 *                    que cambia se le da al núcleo (la llamada 34).
 *   HispaDXEntrada   las teclas pulsadas (la llamada 181: el mapa del núcleo,
 *                    con las que se han pulsado y soltado entre dos miradas),
 *                    el ratón (en las coordenadas del modo del juego); mandos,
 *                    no hay.
 *   HispaDXSonido    (SONIDO) un canal del sonido de Hispa-Pentium (las llamadas
 *                    190 a 193, nucleo32/sonido.cas): 48 kHz, 16 bits, estéreo,
 *                    como el HD Audio de HSPSND; la mezcla de WINMM le da
 *                    trozos de 2.048 tramas. Sin tarjeta, la de antes: en
 *                    silencio, y el tiempo pasa como si sonara.
 *   HispaDXTic       la función del programa (la mezcla y los temporizadores
 *                    de WINMM) a la que se llama al pedir mensajes y en Sleep.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "ventanas.h"
#include "k32.h"

#define API(t) t WINAPI
typedef struct { u32 ancho, alto, paso, bits, octetos; u8 *lfb; u8 masc[8]; } HXPANT;

static Wnd *juego;              /* la ventana que tiene la pantalla */
static struct { u32 estilo, ex; RECT r; int marco; } antes;
static i32 geo[6] = { 640, 480, 0, 0, 640, 480 };   /* el modo del juego y dónde se ve */
static int sucio;

static void rehacer_ventana(Wnd *w, int completa)
{
	/* la ventana del núcleo, otra vez: con las banderas de ahora */
	extern void crear_en_nucleo_(Wnd *w, u32 marcas);
	if (w->vn) {
		sys_cerrar_ventana(w->vn);
		w->vn = 0;
	}
	crear_en_nucleo_(w, completa ? (V_SIN_MARCO | V_ENCIMA | V_COMPLETA) : 0xFFFFFFFF);
}
static void llenar(HXPANT *p, Wnd *w)
{
	memset(p, 0, sizeof *p);
	p->ancho = pantalla_an;
	p->alto = pantalla_al;
	p->paso = pantalla_an * 4;
	p->bits = 32;
	p->octetos = 4;
	p->masc[0] = 8; p->masc[1] = 16;      /* rojo: 8 bits desde el 16 */
	p->masc[2] = 8; p->masc[3] = 8;
	p->masc[4] = 8; p->masc[5] = 0;
	if (w && w->lienzo.bits) {
		p->ancho = w->lienzo.an;
		p->alto = w->lienzo.al;
		p->paso = w->lienzo.paso;
		p->lfb = w->lienzo.bits;
	}
}
static void marca_caja(void)
{
	RECT r;
	if (!juego)
		return;
	r.left = geo[2]; r.top = geo[3];
	r.right = geo[2] + geo[4]; r.bottom = geo[3] + geo[5];
	ensuciar(juego, &r);
	sucio = 1;
}
API(i32) HispaDXPantalla(i32 op, u32 a, u32 b, u32 c)
{
	(void)c;
	switch (op) {
	case 0: {                                 /* tomarla para la ventana a: da su HXPANT en b */
		Wnd *w = raiz_de(wnd((HWND)a));
		if (!w)
			w = activa;
		if (!w) {
			/* (sin ventana: una para el juego) */
			HWND h = CreateWindowExA(0, "#32770", "", WS_POPUP | WS_VISIBLE, 0, 0, pantalla_an, pantalla_al, 0, 0, 0, 0);
			w = wnd(h);
			if (!w)
				return 0;
		}
		if (juego != w) {
			juego = w;
			antes.estilo = w->estilo;
			antes.ex = w->exestilo;
			antes.r = w->r;
			antes.marco = w->nucleo_marco;
			w->estilo = (w->estilo & ~(WS_CAPTION | WS_THICKFRAME | WS_BORDER)) | WS_POPUP | WS_VISIBLE;
			w->r.left = w->r.top = 0;
			w->r.right = pantalla_an;
			w->r.bottom = pantalla_al;
			rehacer_ventana(w, 1);
			sys_ventana_op(3, w->vn, 0);
			{
				u32 k;
				for (k = 0; k < (u32)(w->lienzo.an * w->lienzo.al); k++)
					((u32 *)w->lienzo.bits)[k] = 0;
			}
			ensuciar(w, 0);
			presentar(w);
			traza("DirectDraw: la pantalla, para %x (%dx%d)", (u32)w, w->lienzo.an, w->lienzo.al);
		}
		llenar((HXPANT *)b, w);
		return 1;
	}
	case 1:                                   /* soltarla */
		if (juego && wnd((HWND)juego)) {
			Wnd *w = juego;
			juego = 0;
			w->estilo = antes.estilo;
			w->exestilo = antes.ex;
			w->r = antes.r;
			rehacer_ventana(w, 0);
			if (w->estilo & WS_VISIBLE)
				sys_ventana_op(2, w->vn, 1);
			w->nc_sucio = 1;
			invalidar(w, 0, 1);
		}
		juego = 0;
		return 1;
	case 2:                                   /* la paleta (en 32 bits no hace falta) */
		return 1;
	case 3:                                   /* esperar el retrazo */
		if (juego) {
			marca_caja();
			presentar(juego);
		}
		sys_dormir(1);
		return 1;
	case 4:                                   /* ¿la sigue teniendo? (lo que pinte, a la pantalla en la vuelta siguiente) */
		if (!juego || !wnd((HWND)juego))
			return 0;
		marca_caja();
		return activa == juego || !activa;
	case 5: {                                 /* ¿en el retrazo? */
		static int v;
		v = !v;
		return v;
	}
	case 6:                                   /* cómo se ve: el modo y la caja */
		memcpy(geo, (void *)a, sizeof geo);
		return 1;
	case 7:                                   /* cómo es la pantalla, sin tomarla */
		llenar((HXPANT *)a, 0);
		((HXPANT *)a)->lfb = 0;
		return 1;
	}
	return 0;
}

/* ---- (JUEGOS) la pantalla del juego: con una ventana a pantalla completa
 * (DirectDraw u OpenGL con ChangeDisplaySettings), para el programa la
 * pantalla es la de su modo (640 x 480...), aunque se vea agrandada y en
 * el centro: el ratón, GetSystemMetrics y lo de su ventana, en ese modo ---- */
int en_modo_juego(void) { return juego && wnd((HWND)juego) && geo[0] > 0 && geo[1] > 0 && geo[4] > 0 && geo[5] > 0; }
int es_del_juego(Wnd *w) { return en_modo_juego() && w && raiz_de(w) == juego; }
void modo_medida(i32 *an, i32 *al) { *an = geo[0]; *al = geo[1]; }
void real_a_modo(i32 *x, i32 *y)
{
	*x = (*x - geo[2]) * geo[0] / geo[4];
	*y = (*y - geo[3]) * geo[1] / geo[5];
	if (*x < 0) *x = 0;
	if (*y < 0) *y = 0;
	if (*x >= geo[0]) *x = geo[0] - 1;
	if (*y >= geo[1]) *y = geo[1] - 1;
}
void modo_a_real(i32 *x, i32 *y)
{
	*x = geo[2] + (*x * geo[4] + geo[4] / 2) / geo[0];
	*y = geo[3] + (*y * geo[5] + geo[5] / 2) / geo[1];
}

/* ---- (JUEGOS) el CD de sonido (la llamada 186) y si hay hilos de verdad:
 * para WINMM.DLL de Pentium (MCI «cdaudio», la mezcla en otro hilo) ---- */
API(u32) HispaCDAudio(u32 op, u32 letra, void *destino, u32 lba, u32 n)
{
	u32 r, k = gc_suelta_todo();       /* (leer el CD tarda: los demás hilos, que sigan) */
	if (op == 0)
		r = sc(186, 0, letra, LIN(destino));
	else {
		static u32 d[4];
		u32 dd[4];
		(void)d;
		dd[0] = letra; dd[1] = lba; dd[2] = n; dd[3] = LIN(destino);
		r = sc(186, 1, 0, LIN(dd));
	}
	gc_retoma(k);
	return r;
}
API(BOOL) HispaHilosDeVerdad(void) { return k_hilos; }

/* ---- las teclas y el ratón ---- */
static u8 mapa_bios[128];
static u8 bios[0x500];
API(u32) HispaDXEntrada(i32 op, u32 a, u32 b)
{
	static u32 m[16];
	u32 k;
	switch (op) {
	case 0:                                   /* las pulsadas (por su código de la BIOS) */
		{
			extern int leer_teclas(u32 *m, int de_dx);
			leer_teclas(m, 1);
		}
		for (k = 1; k < 128; k++)
			mapa_bios[k] = ((m[k >> 5] >> (k & 31)) & 1) || ((m[(k + 128) >> 5] >> ((k + 128) & 31)) & 1);
		return (u32)mapa_bios;
	case 4: {                                 /* las pulsadas desde la vez anterior (a «a», 128) */
		u8 *d = (u8 *)a;
		for (k = 0; k < 128; k++)
			d[k] = ((m[8 + (k >> 5)] >> (k & 31)) & 1) || ((m[8 + ((k + 128) >> 5)] >> ((k + 128) & 31)) & 1);
		return 1;
	}
	case 1: {                                 /* el ratón: x, y (del modo) y los botones */
		i32 d[3], *r = (i32 *)a;
		sys_ventana_op(6, 0, LIN(d));
		if (juego && geo[4] && geo[5]) {
			r[0] = (d[0] - geo[2]) * geo[0] / geo[4];
			r[1] = (d[1] - geo[3]) * geo[1] / geo[5];
		} else {
			r[0] = d[0];
			r[1] = d[1];
		}
		r[2] = d[2] & 7;
		return 1;
	}
	case 2:                                   /* un mando: no hay */
		(void)b;
		return 0;
	case 5: {                                 /* (JUEGOS) lo movido de verdad (dx, dy), sin el borde */
		extern BOOL WINAPI HispaRatonMovido(i32 *dx, i32 *dy);
		i32 *r = (i32 *)a;
		return HispaRatonMovido(&r[0], &r[1]) ? 1 : 0;
	}
	case 3:                                   /* «la memoria de la BIOS» (la hora: 0x46C) */
		*(u32 *)(bios + 0x46C) = sys_tics() * 182 / 1000;
		return (u32)bios;
	}
	return 0;
}

/* ---- (SONIDO) el sonido: un canal de Hispa-Pentium; sin tarjeta, una de
 * mentira de 8 bits a 22.050 Hz, en silencio ---- */
#define SC_ABRIR_CANAL 190
#define SC_DAR 191
#define SC_CANAL 192
#define C_VACIAR 3
#define C_EN_COLA 4
#define TROZO_SND 8192                 /* 2.048 tramas de 16 bits en estéreo */
#define EN_COLA_MAX (3 * TROZO_SND)    /* (unos 130 ms: lo que un juego puede ir por delante) */
static u32 canal_snd;                  /* 0: aún no se ha mirado; 0xFFFFFFFF: no hay tarjeta */
static int hay_canal(void)
{
	if (!canal_snd) {
		u32 c = sc(SC_ABRIR_CANAL, 48000, 2 | 16 << 8, 0);
		canal_snd = ES_ERROR(c) || c > 64 ? 0xFFFFFFFF : c;
		traza("DirectSound/WinMM: %s", canal_snd == 0xFFFFFFFF ? "no hay tarjeta de sonido (en silencio)" : "el canal de sonido, abierto");
	}
	return canal_snd != 0xFFFFFFFF;
}
static u32 cola_hasta;          /* (ms) cuándo acaba lo que se le ha dado (sin tarjeta) */
API(i32) HispaDXSonido(i32 op, u32 a, u32 b, u32 c)
{
	u32 ahora = sys_tics() * 10;
	(void)c;
	if (hay_canal()) {
		switch (op) {
		case 0:
			return 3;                         /* «AC'97»: 48 kHz, 16 bits, estéreo */
		case 1:                               /* un trozo: b octetos en a */
			sc(SC_DAR, canal_snd, LIN(a), b);
			return 1;
		case 2: {                             /* cuántos trozos caben sin esperar */
			u32 n = sc(SC_CANAL, canal_snd, C_EN_COLA, 0);
			if (ES_ERROR(n) || n >= EN_COLA_MAX)
				return 0;
			return (EN_COLA_MAX - n) / TROZO_SND;
		}
		case 3:
			sc(SC_CANAL, canal_snd, C_VACIAR, 0);
			return 1;
		}
		return 0;
	}
	switch (op) {
	case 0:
		return 1;                             /* «una Sound Blaster de 8 bits» */
	case 1: {                                 /* un trozo: b muestras */
		u32 ms = b * 1000 / 22050;
		if ((i32)(cola_hasta - ahora) < 0)
			cola_hasta = ahora;
		cola_hasta += ms;
		return 1;
	}
	case 2: {                                 /* cuántos trozos caben sin esperar (de 4.096 muestras: 186 ms) */
		i32 falta = (i32)(cola_hasta - ahora);
		if (falta <= 0)
			return 2;
		return falta < 186 ? 1 : 0;
	}
	case 3:
		cola_hasta = ahora;
		return 1;
	}
	return 0;
}

/* ---- la función del programa ---- */
/* (la de WINMM: stdcall con cuatro argumentos, que no mira) */
static i32 (WINAPI *f_tic)(i32, i32, i32, i32);
static void tic(void)
{
	if (f_tic)
		f_tic(0, 0, 0, 0);
}
API(i32) HispaDXTic(void *f)
{
	f_tic = f;
	tic_dx_prog = tic;
	return 1;
}

/* [GL] El puente de OPENGL32.DLL (HispaGL, w32/gl/; docs/486/OPENGL.md): en
 * Windows, SwapBuffers y las de los formatos de punto son de GDI32, que llama a
 * OPENGL32; aquí OPENGL32 da su función al cargarse (HispaGLPuente) y estas le
 * pasan la llamada (op, a, b, c), como Hispa Ventanas en Hispa-486
 * (ventanas/win32gl.inc). ChangeDisplaySettingsA (user32.c) también. */
static u32 (WINAPI *gl_puente)(u32, u32, u32, u32);
u32 gl_llama(u32 op, u32 a, u32 b, u32 c) { return gl_puente ? gl_puente(op, a, b, c) : 0; }
/* == GDI32 == */
API(i32) HispaGLPuente(void *f) { gl_puente = (u32 (WINAPI *)(u32, u32, u32, u32))f; return 1; }
API(i32) SwapBuffers(HDC h) { return (i32)gl_llama(1, (u32)h, 0, 0); }
API(i32) ChoosePixelFormat(HDC h, const void *p) { return (i32)gl_llama(2, (u32)h, (u32)p, 0); }
API(i32) SetPixelFormat(HDC h, i32 n, const void *p) { return (i32)gl_llama(3, (u32)h, (u32)n, (u32)p); }
API(i32) DescribePixelFormat(HDC h, i32 n, u32 t, void *p) { (void)h; return (i32)gl_llama(4, (u32)n, t, (u32)p); }
API(i32) GetPixelFormat(HDC h) { return (i32)gl_llama(5, (u32)h, 0, 0); }
API(i32) GetDeviceGammaRamp(HDC h, void *r) { (void)h; (void)r; return 0; }   /* (el brillo: «no se puede») */
API(i32) SetDeviceGammaRamp(HDC h, void *r) { (void)h; (void)r; return 0; }
