/* pentium.h - Comprimidos y el Mirador de Hispa-Pentium: las llamadas al
 * sistema (int 0x80; castel/nucleo32/usuario.cas, docs/PENTIUM.md §6).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef PENTIUM_H
#define PENTIUM_H
#include <stdint.h>

static inline uint32_t sc(uint32_t n, uint32_t a, uint32_t b, uint32_t c)
{
	uint32_t r;

	__asm__ volatile("int $0x80" : "=a"(r) : "a"(n), "b"(a), "c"(b), "d"(c) : "memory");
	return r;
}
#define ES_ERROR(r) ((uint32_t)(r) >= 0xFFFFFFF0u)

#define SC_ESCRIBIR 1
#define SC_ACABAR 2
#define SC_DORMIR 3
#define SC_TICS 4
#define SC_TECLA 6
#define SC_ABRIR 7
#define SC_LEER 8
#define SC_CERRAR 9
#define SC_LANZAR 10
#define SC_ESPERAR 11
#define SC_ENTRADA 12
#define SC_LARGO 13
#define SC_ARGUMENTOS 14
#define SC_DATO 15
#define SC_MEMORIA 17
#define SC_ESCRIBIR_ARCHIVO 18
#define SC_MOVER 19
#define SC_CORTAR 20
#define SC_BORRAR 21
#define SC_RENOMBRAR 22
#define SC_HACER_CARPETA 23
#define SC_CARPETA 26
#define SC_ENTRADA_LARGA 28
#define SC_ATENDER_CORTE 29
#define SC_HUBO_CORTE 30
#define SC_VENTANA 32
#define SC_CERRAR_VENTANA 33
#define SC_DIBUJAR 34
#define SC_SUCESO 35
#define SC_TITULO 36
#define SC_MENU 39
#define SC_PORTAPAPELES 40
#define SC_LANZAR_SUELTO 41
#define SC_UNIDAD 42
#define SC_SERIE 61
#define SC_COLOCAR 80
#define SC_MAXIMIZAR 81
#define SC_SOLTAR_FUERA 171	/* (explorador) lo soltado fuera de la ventana: al tapiz o al dock */
#define SC_TAPIZ 172		/* (MIRADOR) una imagen como tapiz del escritorio */
#define SC_VENTANAS_W32 182	/* [WIN32] ventanas sin marco o a pantalla completa */

#define PARA_LEER 0
#define PARA_ESCRIBIR 1
#define PARA_CREAR 2
#define E_NO_HAY 0xFFFFFFFFu
#define E_YA_EXISTE 0xFFFFFFF7u
#define SIGUE 0xFFFFFF00u

#define DATO_HZ 9
#define DATO_FECHA 10
#define DATO_ANCHO_PANTALLA 11
#define DATO_ALTO_PANTALLA 12
#define DATO_ESCRITORIO 13
#define DATO_ALTO_LIBRE 14

/* una entrada de carpeta (Entrada, llamadas.cas: 32 octetos) */
struct entrada {
	char nombre[13];
	uint8_t atributos;
	uint16_t reservado;
	uint32_t tamano;
	uint16_t fecha, hora;
	uint32_t cluster, libre;
};
#define MAX_LARGO 256

/* ---- las ventanas ---- */
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
#define BOTON_IZQUIERDO 1
#define BOTON_DERECHO 2
#define CON_MAYUS 0x100
#define CON_CONTROL 0x200
#define SIN_ELEGIR 0xFFFFFFFFu
struct suceso { uint32_t clase, a, b, c; };
struct orden {
	uint32_t op;
	int32_t x, y, ancho, alto;
	uint32_t tinta, tinta2;
	const void *datos;
	int32_t n;
};
#define OP_RECTANGULO 1
#define OP_DEGRADADO 2
#define OP_MARCO 3
#define OP_LINEA 4
#define OP_TEXTO 5
#define OP_TEXTO_FONDO 6
#define OP_MAPA 7
#define OP_REDONDEADO 9
#define OP_NEGRITA 10
struct pide_menu { int32_t x, y; const char *opciones; uint32_t inactivas, etiqueta; };
struct suelta { int32_t x, y; const char *ruta; };
struct pide_ventana { const char *nombre; int32_t x, y, ancho, alto; uint32_t marcas; };
#define V_SIN_MARCO 1
#define V_ENCIMA 2
#define V_COMPLETA 256

#define T_ARRIBA 0x148
#define T_ABAJO 0x150
#define T_IZQUIERDA 0x14B
#define T_DERECHA 0x14D
#define T_INICIO 0x147
#define T_FIN 0x14F
#define T_SUPR 0x153
#define T_RE_PAG 0x149
#define T_AV_PAG 0x151
#define T_F2 0x13C
#define T_F5 0x13F
#define T_F11 0x157
#define T_MAYUS_TAB 0x10F
#define T_TAB 9
#define T_INTRO 13
#define T_ESC 27
#define T_BORRAR 8

/* de base.c */
int trocea(char *t, char **argv, int max);
uint32_t memoria_pedida(void);
#endif
