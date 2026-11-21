/* pentium.h - ESCUDO.BIN: las llamadas al sistema de Hispa-Pentium (int 0x80;
 * docs/PENTIUM.md §6, castel/nucleo32/llamadas.cas) y lo que se dicen
 * plataforma.c, escudo.c y ventana.c.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef ESCUDO_PENTIUM_H
#define ESCUDO_PENTIUM_H
#include "escudo.h"

static inline u32 sc(u32 n, u32 a, u32 b, u32 c)
{
	u32 r;
	__asm__ volatile("int $0x80" : "=a"(r) : "a"(n), "b"(a), "c"(b), "d"(c) : "memory");
	return r;
}
#define ES_ERROR(r) ((u32)(r) >= 0xFFFFFFF0u)

#define SC_ESCRIBIR 1
#define SC_DORMIR 3
#define SC_TICS 4
#define SC_ABRIR 7
#define SC_LEER 8
#define SC_CERRAR 9
#define SC_LANZAR 10
#define SC_ESPERAR 11
#define SC_LARGO 13
#define SC_ARGUMENTOS 14
#define SC_DATO 15
#define SC_MEMORIA 17
#define SC_ESCRIBIR_ARCHIVO 18
#define SC_MOVER 19
#define SC_BORRAR 21
#define SC_RENOMBRAR 22
#define SC_HACER_CARPETA 23
#define SC_ENTRADA_LARGA 28
#define SC_ATENDER_CORTE 29
#define SC_HUBO_CORTE 30
#define SC_VENTANA 32
#define SC_CERRAR_VENTANA 33
#define SC_DIBUJAR 34
#define SC_SUCESO 35
#define SC_TITULO 36
#define SC_LANZAR_SUELTO 41
#define SC_UNIDAD 42
#define SC_INFO_UNIDAD 43
#define SC_SERIE 61
#define SC_AJUSTES 68
#define SC_COLOCAR 80
#define SC_CAMBIOS_UNIDADES 131

#define PARA_LEER 0
#define PARA_ESCRIBIR 1
#define PARA_CREAR 2
#define PARA_ANADIR 3

#define E_NO_HAY 0xFFFFFFFFu
#define E_YA_EXISTE 0xFFFFFFF7u
#define SIGUE 0xFFFFFF00u

#define DATO_VERSION 3
#define DATO_HZ 9
#define DATO_FECHA 10
#define DATO_ESCRITORIO 13

#define AT_OCULTO 0x02
#define AT_SISTEMA 0x04
#define AT_VOLUMEN 0x08
#define AT_CARPETA 0x10

struct entrada {
	char nombre[13];
	u8 atributos;
	u16 reservado;
	u32 tamano;
	u16 fecha, hora;
	u32 cluster, libre;
};

/* Info_unidad (llamadas.cas): 72 octetos */
#define MEDIO_MEMORIA 0
#define MEDIO_IDE 1
#define MEDIO_SATA 2
#define MEDIO_USB 3
struct info_unidad {
	u8 letra, clase, medio, solo_lectura;
	u32 total_k, libre_k;
	u32 del_sistema;
	char nombre[41];
	char etiqueta[12];
	u8 quitable;
	u8 relleno[2];
};

/* las ventanas (ventanas.cas) */
#define EV_TECLA 1
#define EV_RATON_BAJA 2
#define EV_RATON_SUBE 3
#define EV_RATON_MUEVE 4
#define EV_CERRAR 5
#define EV_TAMANO 6
#define EV_FOCO 7
#define EV_SALE 9
#define EV_MENU 10
#define EV_UNIDADES 12
#define EV_RUEDA 13
struct suceso { u32 clase, a, b, c; };
struct orden {
	u32 op;
	i32 x, y, ancho, alto;
	u32 tinta, tinta2;
	const void *datos;
	i32 n;
};
#define OP_MAPA 7
#define T_ARRIBA 0x148
#define T_ABAJO 0x150
#define T_IZQUIERDA 0x14B
#define T_DERECHA 0x14D
#define T_INICIO 0x147
#define T_FIN 0x14F
#define T_SUPR 0x153
#define T_RE_PAG 0x149
#define T_AV_PAG 0x151
#define T_F5 0x13F
#define T_MAYUS_TAB 0x10F
#define T_CTRL_TAB 0x110
#define T_CTRL_MAYUS_TAB 0x111

/* ---- plataforma.c ---- */
void escribe(const char *t);		/* a la pantalla (o al terminal): de UTF-8 a la 437 */
void escribef(const char *f, ...);
void serie(const char *f, ...);		/* al puerto serie (lo que miran las pruebas): «escudo: ...» */
u32 a437(char *d, u32 cabe, const char *utf8);	/* de UTF-8 (o ya en la 437) a la 437 */
u32 hz(void);
u32 tics(void);
void dormir(u32 t);
int existe(const char *ruta);
int es_carpeta(const char *ruta);
int borrar(const char *ruta);
int renombrar(const char *a, const char *b);
int hacer_carpeta(const char *ruta);
int lee_todo(const char *ruta, char *d, u32 cabe);	/* los leídos, o <0 */
int escribe_todo(const char *ruta, const void *d, u32 n);
int anade_a(const char *ruta, const char *t);
int copia(const char *de, const char *a);
u32 fecha_hoy(void);			/* AAAAMMDD */
u32 hora_ahora(void);			/* HHMM */
int unidad_sistema(void);		/* la letra */
int info_unidad(u32 n, struct info_unidad *u);
u32 cambios_unidades(void);
int escritorio_en_marcha(void);
/* recorrer una carpeta: 1 si hay entrada (nombre corto en e.nombre, largo en «largo») */
int abre_carpeta(const char *ruta);
int entrada_siguiente(int h, struct entrada *e, char *largo);
extern int pl_atiende;			/* (escudo.c) para pl_cortar: la ventana */
int atiende_ventana(void);		/* (ventana.c) mira la ventana mientras se revisa: 1 si cancelar */
size_t strcspn(const char *s, const char *r);
char *strstr(const char *a, const char *b);
int strncasecmp_es(const char *a, const char *b, u32 n);
#endif
