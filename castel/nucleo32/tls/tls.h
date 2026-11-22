/* tls.h - TLS.BIN, el servicio TLS de Hispa-Pentium: las llamadas al núcleo,
 * las órdenes de los pedidos y los errores (los mismos números que
 * castel/nucleo32/redes.cas, rbase.cas y red.cas: si cambia uno, el otro).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef HISPA_TLS_H
#define HISPA_TLS_H
#include <stddef.h>

typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;

/* ---- las llamadas al sistema (int 0x80; usuario.cas) ---- */
#define SC_SERIE_ESCRIBIR 1
#define SC_DORMIR 3
#define SC_TICS 4
#define SC_ABRIR 7		/* EBX nombre, ECX modo (0 leer, 2 crear) -> manejador */
#define SC_LEER 8
#define SC_CERRAR 9
#define SC_ENTRADA 12		/* la entrada siguiente de la carpeta EBX, a ECX -> 1 o 0 */
#define SC_DATO 15
#define SC_MAS_MEMORIA 17
#define SC_ESCRIBIR_ARCHIVO 18
#define SC_CORTE 29
#define SC_SERIE 61
#define SC_RED 90		/* pedir al servicio de red (RED.BIN) */
#define SC_APUNTARSE 91
#define SC_SIGUIENTE 92
#define SC_CONTESTAR 93
#define DATO_HZ 9
#define DATO_FECHA 10		/* fecha (arriba) y hora (abajo), como en la FAT */
#define ES_ERROR(r) ((u32)(r) >= 0xFFFFFFE0u)

/* ---- las órdenes (cab[0]): las de RED.BIN (rbase.cas) y las de aquí ---- */
#define O_ESTADO 1		/* (TLS) cómo está: un texto */
#define O_RESOLVER 2
#define O_CONECTAR 4		/* (TLS) los octetos: el nombre; cab[1] IP (0: se busca), cab[2] puerto, cab[3] espera; (CORREO) cab[4] STARTTLS: 1 IMAP, 2 SMTP, 3 POP3 */
#define O_ENVIAR 5
#define O_RECIBIR 6
#define O_CERRAR 7
#define O_REGISTRO 11
#define O_INFO 12		/* (TLS) cab[1] conexión -> el texto del cifrado; cab[1] ms del apretón, cab[2] banderas, cab[3] K ciclos */
#define AVISO_ACABO 0xFFFF

/* ---- los errores: los de la red (redes.cas) y los de TLS ---- */
#define SIN_RED 0xFFFFFFEFu
#define SIN_IP 0xFFFFFFECu
#define SIN_CABLE 0xFFFFFFEDu
#define TIEMPO 0xFFFFFFEBu
#define CORTADA_RED 0xFFFFFFE8u
#define NO_CONEXION 0xFFFFFFE7u
#define TLS_CERTIFICADO 0xFFFFFFE5u	/* el certificado del servidor no vale */
#define TLS_FALLO 0xFFFFFFE4u		/* la conexión segura no se ha podido hacer */
#define SIN_TLS 0xFFFFFFE3u		/* no hay servicio TLS (el núcleo) */
#define TLS_SIN_HORA 0xFFFFFFE2u	/* no se sabe la fecha: no se pueden mirar los certificados */
#define NO_VALE 0xFFFFFFF9u
#define SIN_MEMORIA 0xFFFFFFFAu

#define PRIMERA_TLS 0x1000	/* el número de una conexión cifrada: 0x1000 + su sitio */
#define MAX_DATOS 8192

/* lo que da la llamada 92 (Pedido_red de red.cas); detrás, los octetos */
struct pedido {
	u32 hueco, serie, tarea, cabe, largo;
	u32 relleno[3];
	u32 cab[8];
};
/* lo que se le da a la 93 (Respuesta_red) */
struct respuesta {
	u32 serie, datos;
	u32 cab[8];
};
/* una entrada de una carpeta (la llamada 12: Entrada de llamadas.cas) */
struct entrada {
	char nombre[13];
	u8 atributos;
	u16 reservado;
	u32 tamano;
	u16 fecha, hora;
	u32 cluster, libre;
};

static inline u32 sc(u32 n, u32 a, u32 b, u32 c)
{
	u32 r;

	__asm__ __volatile__("int $0x80" : "=a"(r) : "a"(n), "b"(a), "c"(b), "d"(c) : "memory");
	return r;
}

int formatear(char *d, int cabe, const char *p, ...);
void *memcpy(void *, const void *, size_t);
void *memset(void *, int, size_t);
void *memmove(void *, const void *, size_t);	/* (CORREO) */
int memcmp(const void *, const void *, size_t);
size_t strlen(const char *);
int strcmp(const char *, const char *);
#endif
