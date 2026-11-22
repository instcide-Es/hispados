/* juego.h - (JUEGOS) lo común de los juegos de Hispa-Pentium (juego.c,
 * dibujo.c, sintes.c): la ventana con su lienzo propio (se pinta en la
 * memoria del programa y se sube a la ventana solo lo que ha cambiado), la
 * pantalla completa, la letra de 8x16 a cualquier escala, las teclas (las
 * pulsadas de verdad, por la llamada 181, y las que llegan como sucesos), el
 * pulso de 100 por segundo y los cuadros (60 por segundo), los menús, los
 * cuadros de aviso, los récords en la carpeta del usuario, el sonido
 * sintetizado (sintes.c) y lo que gasta el procesador (/MIDE).
 * Los textos se escriben en UTF-8 en el fuente y se pasan a la página 437.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef JUEGO_H
#define JUEGO_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "pentium.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int16_t i16;
typedef int32_t i32;
typedef uint64_t u64;

/* ---------------------------------------------------------------- el lienzo */
typedef struct {
	u32 *p;			/* 0x00RRGGBB */
	int an, al, paso;	/* «paso»: los puntos de una fila a la siguiente */
} lienzo;

/* el de la ventana (lo que se sube) */
extern lienzo LZ;

void lz_nuevo(lienzo *l, int an, int al);	/* (con malloc; p = 0 si no hay memoria) */
void lz_libre(lienzo *l);
void lz_trozo(lienzo *t, const lienzo *de, int x, int y, int an, int al);
void lz_rect(lienzo *l, int x, int y, int an, int al, u32 c);
void lz_marco(lienzo *l, int x, int y, int an, int al, u32 c);
void lz_degradado(lienzo *l, int x, int y, int an, int al, u32 arriba, u32 abajo);
void lz_degradado_h(lienzo *l, int x, int y, int an, int al, u32 izq, u32 der);
void lz_redondo(lienzo *l, int x, int y, int an, int al, int r, u32 c);
void lz_mezcla(lienzo *l, int x, int y, int an, int al, u32 c, int alfa);	/* alfa 0-256 */
void lz_linea(lienzo *l, int x1, int y1, int x2, int y2, u32 c);
void lz_linea_gruesa(lienzo *l, int x1, int y1, int x2, int y2, int grueso, u32 c);
void lz_circulo(lienzo *l, int cx, int cy, int r, u32 c);		/* lleno */
void lz_circulo_suave(lienzo *l, float cx, float cy, float r, u32 c);	/* lleno, con el borde suavizado */
void lz_anillo(lienzo *l, int cx, int cy, int r, int grueso, u32 c);
void lz_elipse(lienzo *l, int cx, int cy, int rx, int ry, u32 c);
void lz_poligono(lienzo *l, const float *xy, int n, u32 c);		/* x0,y0,x1,y1...; cualquiera (par-impar) */
void lz_poligono_mezcla(lienzo *l, const float *xy, int n, u32 c, int alfa);
void lz_copia(lienzo *d, int x, int y, const lienzo *o, int ox, int oy, int an, int al);
void lz_copia_clave(lienzo *d, int x, int y, const lienzo *o, int ox, int oy, int an, int al, u32 clave);
u32 mezcla(u32 a, u32 b, int alfa);	/* de a hacia b (alfa 0-256) */
u32 aclara(u32 c, int n);		/* n de -256 (negro) a 256 (blanco) */
/* un dibujo de letras («.» transparente; cada letra, su color en «colores»: 'a'..'z', '0'..'9') */
void lz_dibujo(lienzo *l, int x, int y, const char *d, int an, int al, int escala, const u32 *tabla);

/* la letra de 8x16 (la de Hispa Ventanas), a escala entera; el texto en UTF-8 */
int lz_texto(lienzo *l, int x, int y, const char *t, u32 c, int escala);
int lz_texto_sombra(lienzo *l, int x, int y, const char *t, u32 c, u32 sombra, int escala);
int lz_negrita(lienzo *l, int x, int y, const char *t, u32 c, int escala);
int ancho_texto(const char *t, int escala);
int lz_texto_centro(lienzo *l, int cx, int y, const char *t, u32 c, int escala, int negrita);
/* un párrafo partido por palabras en «max» puntos de ancho: da los renglones */
int lz_parrafo(lienzo *l, int x, int y, int max, const char *t, u32 c, int escala, int interlinea);
/* las 16 filas (un octeto cada una) de la letra c de la página 437 */
const unsigned char *jg_letra(int c);
/* UTF-8 a la página 437 */
int a437(const char *utf8, char *d, int cabe);

/* --------------------------------------------------------------- la ventana */
#define JV_FIJA 1		/* (sin uso: todas se pueden estirar) */
#define JV_COMPLETA 2		/* empezar a pantalla completa */
int jg_abrir(const char *titulo, int an, int al, int marcas);
void jg_titulo(const char *titulo);
void jg_pantalla_completa(int si);
extern int jg_completa;			/* a pantalla completa */
extern int jg_activa;			/* la ventana activa (si no, las teclas no son suyas) */
void jg_sucio(int x, int y, int an, int al);	/* ha cambiado ese trozo de LZ */
void jg_todo_sucio(void);
void jg_subir(void);			/* sube lo sucio a la ventana */
void jg_tamano_maximo(int *an, int *al);	/* lo que cabe en la pantalla */

/* ---------------------------------------------------------------- el tiempo */
extern u32 jg_hz;			/* los tics por segundo (100) */
u32 jg_tics(void);
static inline u64 rdtsc(void)
{
	u32 a, d;

	__asm__ volatile("rdtsc" : "=a"(a), "=d"(d));
	return ((u64)d << 32) | a;
}

/* ---------------------------------------------------------------- las teclas */
/* los códigos del teclado (juego 1); las de 0xE0, + 0x80 */
#define K_ESC 0x01
#define K_1 0x02
#define K_TAB 0x0F
#define K_Q 0x10
#define K_W 0x11
#define K_E 0x12
#define K_R 0x13
#define K_P 0x19
#define K_INTRO 0x1C
#define K_CTRL 0x1D
#define K_A 0x1E
#define K_S 0x1F
#define K_D 0x20
#define K_F 0x21
#define K_MAYUS 0x2A
#define K_Z 0x2C
#define K_X 0x2D
#define K_C 0x2E
#define K_V 0x2F
#define K_M 0x32
#define K_COMA 0x33
#define K_PUNTO 0x34
#define K_MENOS 0x35		/* la «/» del teclado de EE. UU. */
#define K_MAYUS_D 0x36
#define K_ALT 0x38
#define K_ESPACIO 0x39
#define K_F1 0x3B
#define K_F2 0x3C
#define K_NUM_ARRIBA 0x48
#define K_NUM_IZQ 0x4B
#define K_NUM_DER 0x4D
#define K_NUM_ABAJO 0x50
#define K_F11 0x57
#define K_F12 0x58
#define K_CTRL_D (0x80 | 0x1D)
#define K_ARRIBA (0x80 | 0x48)
#define K_IZQ (0x80 | 0x4B)
#define K_DER (0x80 | 0x4D)
#define K_ABAJO (0x80 | 0x50)
int tecla(int k);			/* ¿está pulsada ahora? */
int tecla_vino(int k);			/* ¿se acaba de pulsar (desde el pulso anterior, aunque ya se haya soltado; la repetición no cuenta)? */
void teclas_mirar(void);		/* (lo hace el bucle en cada pulso) */
void teclas_olvidar(void);

/* ------------------------------------------------------------ el bucle */
struct juego {
	void (*pulso)(void);			/* 100 por segundo */
	void (*pinta)(void);			/* cuando toca un cuadro y jg_cambio */
	void (*suceso)(const struct suceso *s);	/* las teclas, el ratón, los menús */
	void (*tamano)(int an, int al);		/* la ventana ha cambiado de tamaño (LZ ya es el nuevo) */
	int (*al_cerrar)(void);			/* 1: se puede cerrar */
	int cuadros;				/* por segundo (60 si 0) */
};
extern int jg_cambio;			/* hay que pintar un cuadro */
extern int jg_salir;
extern int jg_mide;			/* /MIDE: lo que gasta, por el puerto serie */
int jg_correr(const struct juego *j);
void jg_pausa_tics(u32 n);		/* (las pruebas: /PRUEBA) */

/* ------------------------------------------------------------ los menús */
/* la barra de arriba: «Juego|Ayuda»; cada menú, sus opciones (con «|», «-» la raya) */
#define ALTO_BARRA 26
void jg_menus(const char *barra, const char *const *opciones);
void jg_pinta_barra(void);
int jg_clic_barra(int x, int y);	/* 1 si era en la barra (y abre el menú) */
void jg_menu_en(int x, int y, int cual);	/* abre el menú «cual» ahí (el botón secundario) */
void jg_menu_grises(int cual, u32 grises);
/* llega como EV_MENU: s->a la opción, s->b el menú (lo da jg_menu_elegido) */
int jg_menu_elegido(const struct suceso *s, int *menu);

/* ------------------------------------------------------------ los cuadros */
/* un cartel encima del juego, que espera a Intro, Esc o un clic (el juego, quieto) */
void jg_aviso(const char *titulo, const char *texto);
/* sí o no: 1 sí */
int jg_pregunta(const char *titulo, const char *texto, const char *si, const char *no);
/* elegir uno de n (devuelve su número o -1) */
int jg_elige(const char *titulo, const char *const *opciones, int n, int actual);
/* un cartel en medio de un rectángulo (lo pinta el juego en su cuadro) */
void jg_cartel(lienzo *l, int x, int y, int an, int al, const char *titulo, const char *texto);

/* ------------------------------------------------------------ los récords */
void rec_abrir(const char *nombre);	/* «BLOQUES.REC»: lo lee de la carpeta de ajustes */
const char *rec_texto(const char *clave);	/* 0 si no está */
int rec_entero(const char *clave, int si_no);
void rec_poner(const char *clave, const char *valor);
void rec_poner_entero(const char *clave, int n);
int rec_guardar(void);			/* 1 si se ha escrito */
const char *rec_ruta(void);		/* dónde (para las pruebas y la ayuda) */
/* una tabla de los mejores: «clave1=puntos|nombre|fecha» ... (de mayor a menor o al revés) */
#define REC_N 10
struct record { int puntos; char nombre[24]; char fecha[12]; };
int rec_tabla(const char *clave, struct record *t, int n);	/* cuántos hay */
int rec_meter(const char *clave, int n, int puntos, int menos_es_mejor);	/* el sitio (0..) o -1 */
void rec_ver(const char *titulo, const char *clave, int n, const char *unidad, int menos_es_mejor, int resaltar);
const char *jg_usuario(void);		/* el nombre del que juega («Jugador» si no se sabe) */
void jg_fecha(char *d, int cabe);	/* dd/mm/aaaa */

/* ------------------------------------------------------------ varios */
void serie(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
u32 azar(void);
int azar_n(int n);			/* 0..n-1 */
void azar_semilla(u32 s);
extern char jg_args[256];		/* los argumentos, en mayúsculas */
int arg_hay(const char *opcion);	/* «/MIDE» */
int arg_entero(const char *opcion, int si_no);	/* «/S=» */

/* ------------------------------------------------------------ las cuentas */
static inline float m_raiz(float x)
{
	float r;

	if (x <= 0)
		return 0;
	__asm__("fsqrt" : "=t"(r) : "0"(x));
	return r;
}
static inline float m_sen(float x)
{
	float r;

	__asm__("fsin" : "=t"(r) : "0"(x));
	return r;
}
static inline float m_cos(float x)
{
	float r;

	__asm__("fcos" : "=t"(r) : "0"(x));
	return r;
}
static inline float m_atan2(float y, float x)
{
	float r;

	__asm__("fpatan" : "=t"(r) : "0"(x), "u"(y) : "st(1)");
	return r;
}
static inline float m_abs(float x) { return x < 0 ? -x : x; }
static inline int m_redondo(float x) { return x >= 0 ? (int)(x + 0.5f) : -(int)(-x + 0.5f); }
static inline int m_suelo(float x) { int i = (int)x; return (x < (float)i) ? i - 1 : i; }
#define M_PI_F 3.14159265f
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define LIMITA(v, a, b) ((v) < (a) ? (a) : (v) > (b) ? (b) : (v))

/* ------------------------------------------------------------ el sonido (sintes.c) */
/* una voz: la forma, la frecuencia de ida y de llegada (Hz), lo que dura (ms),
 * el volumen (0-100), el ataque y la caída (ms) */
#define ON_CUADRADA 0
#define ON_TRIANGULO 1
#define ON_SIERRA 2
#define ON_RUIDO 3
#define ON_SENO 4
#define ON_CAMPANA 5		/* seno con su octava y una caída larga */
struct efecto { u8 onda; u16 f0, f1; u16 ms; u8 vol; u16 ataque, caida; };
int snd_abrir(void);			/* 1 si hay tarjeta */
void snd_efecto(const struct efecto *e);
void snd_nota(int onda, float hz, int ms, int vol);
void snd_nota_tras(int onda, float hz, int ms, int vol, int tras_ms);	/* (para arpegios) */
void snd_efecto_tras(const struct efecto *e, int tras_ms);
float nota_hz(int midi);			/* la 69: 440 Hz */
/* la música: una partitura en texto («T120 O4 L8 a b >c d e4 r8 ...», ver sintes.c), dos voces */
void snd_musica(const char *voz1, const char *voz2, int bucle);
void snd_musica_parar(void);
void snd_alimentar(void);		/* (lo hace el bucle) */
void snd_cerrar(void);
extern int snd_hay;			/* hay tarjeta y canal */
extern int snd_mudo;			/* los efectos, callados (lo elige el jugador) */
extern int snd_sin_musica;		/* la música, callada */
extern u32 snd_efectos_dados;		/* (las pruebas) */

#endif
