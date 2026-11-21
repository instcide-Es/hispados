/* hispa.h - lo común de los programas en C del anillo 3 de Hispa-Pentium
 * (SSH.BIN, SSHD.BIN, WG.BIN, TAILSCALE.BIN...): las llamadas al núcleo
 * (docs/PENTIUM.md sección 6), la red (RED.BIN, la llamada 90: los mismos
 * números que redes.cas y rbase.cas), los archivos, el azar, las ventanas y
 * un registro por el puerto serie. Sin biblioteca del sistema
 * (-ffreestanding): la libc mínima es la de TLS.BIN (nucleo32/tls/libc.c).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef HISPA_C_H
#define HISPA_C_H
#include <stddef.h>
#include <stdint.h>

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;
typedef uint64_t u64;

/* ---- las llamadas (int 0x80) ---- */
#define SC_ESCRIBIR 1
#define SC_SALIR 2
#define SC_DORMIR 3
#define SC_TICS 4
#define SC_TECLA 6		/* EBX = 0: la espera; 1: solo si la hay */
#define SC_ABRIR 7		/* 0 leer, 1 leer y escribir, 2 crear, 3 al final */
#define SC_LEER 8
#define SC_CERRAR 9
#define SC_LANZAR 10
#define SC_ESPERAR 11		/* ECX = 1: sin esperar (SIGUE si no ha acabado) */
#define SC_ENTRADA 12
#define SC_LARGO 13
#define SC_ARGUMENTOS 14
#define SC_DATO 15
#define SC_BORRAR_PANTALLA 16
#define SC_MAS_MEMORIA 17
#define SC_ESCRIBIR_ARCHIVO 18
#define SC_BORRAR 21
#define SC_RENOMBRAR 22
#define SC_CREAR_CARPETA 23
#define SC_ATENDER_CORTE 29
#define SC_HUBO_CORTE 30
#define SC_VENTANA 32
#define SC_CERRAR_VENTANA 33
#define SC_DIBUJAR 34
#define SC_SUCESO 35
#define SC_TITULO 36
#define SC_TERMINAL 37		/* 0 hacer, 1 leer, 2 una tecla, 3 Ctrl+C */
#define SC_PORTAPAPELES 40
#define SC_LANZAR_SUELTO 41
#define SC_INFO_UNIDAD 43
#define SC_TEXTO_EN 60
#define SC_SERIE 61
#define SC_CURSOR_EN 62
#define SC_AJUSTES 68		/* 1 el usuario (a ECX), 3 el nombre del equipo */
#define SC_RED 90
#define SC_ESTADO_RED 97
#define SC_TLS 99

#define DATO_VERSION 3
#define DATO_HZ 9
#define DATO_FECHA 10
#define DATO_ESCRITORIO 13

#define SIGUE 0xFFFFFF00u
#define ES_ERROR(r) ((u32)(r) >= 0xFFFFFFE0u)

/* ---- las teclas (llamadas.cas) ---- */
#define T_ESPECIAL 0x100
#define T_ARRIBA 0x148
#define T_ABAJO 0x150
#define T_IZQUIERDA 0x14B
#define T_DERECHA 0x14D
#define T_INICIO 0x147
#define T_FIN 0x14F
#define T_SUPR 0x153
#define T_INSERT 0x152
#define T_RE_PAG 0x149
#define T_AV_PAG 0x151
#define T_F1 0x13B		/* F1 a F10: 0x13B a 0x144 */
#define T_F11 0x157
#define T_F12 0x158

/* ---- los sucesos de una ventana ---- */
#define EV_TECLA 1
#define EV_RATON_BAJA 2
#define EV_CERRAR 5
#define EV_TAMANO 6
#define EV_FOCO 7
struct suceso { u32 clase, a, b, c; };

/* ---- la red: las órdenes (rbase.cas) y los errores (redes.cas) ---- */
#define O_ESTADO 1
#define O_RESOLVER 2
#define O_CONECTAR 4
#define O_ENVIAR 5
#define O_RECIBIR 6
#define O_CERRAR 7
#define O_UDP 8
#define O_UDP_ENVIAR 9
#define O_ESCUCHAR 20		/* (SSH) */
#define O_ACEPTAR 21		/* (SSH) */
#define O_TUNEL 22		/* (WireGuard) poner la interfaz del túnel: cab[1] IP, cab[2] máscara (bits) */
#define O_TUNEL_LEER 23		/* (WireGuard) lo que sale por el túnel: un paquete IP */
#define O_TUNEL_ENTRA 24	/* (WireGuard) un paquete IP que ha llegado por el túnel */
#define O_TUNEL_QUITAR 25	/* (WireGuard) quitar la interfaz */
#define O_TUNEL_DNS 26		/* (Tailscale) los nombres de la red: «nombre ip\n...» */

#define SIN_RED 0xFFFFFFEFu
#define SIN_TARJETA 0xFFFFFFEEu
#define SIN_CABLE 0xFFFFFFEDu
#define SIN_IP 0xFFFFFFECu
#define TIEMPO 0xFFFFFFEBu
#define RECHAZADA 0xFFFFFFEAu
#define NO_RESUELVE 0xFFFFFFE9u
#define CORTADA_RED 0xFFFFFFE8u
#define NO_CONEXION 0xFFFFFFE7u
#define INALCANZABLE 0xFFFFFFE6u
#define TLS_CERTIFICADO 0xFFFFFFE5u
#define TLS_FALLO 0xFFFFFFE4u
#define SIN_TLS 0xFFFFFFE3u
#define TLS_SIN_HORA 0xFFFFFFE2u
#define NO_VALE 0xFFFFFFF9u
#define NO_HAY 0xFFFFFFFFu
#define PRIMERA_SEGURA 0x1000
#define MAX_DATOS 8192

static inline u32 sc(u32 n, u32 a, u32 b, u32 c)
{
	u32 r;

	__asm__ __volatile__("int $0x80" : "=a"(r) : "a"(n), "b"(a), "c"(b), "d"(c) : "memory");
	return r;
}

/* ---- libc (nucleo32/tls/libc.c) ---- */
int formatear(char *d, int cabe, const char *p, ...);
int vformatear(char *d, int cabe, const char *p, __builtin_va_list va);
void *memcpy(void *, const void *, size_t);
void *memmove(void *, const void *, size_t);
void *memset(void *, int, size_t);
int memcmp(const void *, const void *, size_t);
size_t strlen(const char *);
int strcmp(const char *, const char *);

/* ---- hispa.c ---- */
extern u32 h_hz;			/* los tics por segundo */
extern char h_nombre_prog[16];		/* para el registro: «ssh», «sshd», «wg»... */
void h_inicia(const char *nombre);	/* lo primero: el registro, los tics, el azar */
u32 h_tics(void);
void h_dormir(u32 tics);
void h_escribe(const char *t);		/* a la pantalla (o al terminal) */
void h_escribef(const char *f, ...);
void h_apunta(const char *f, ...);	/* al registro (memoria y puerto serie: «nombre: ...») */
extern char h_registro[];
extern u32 h_registro_largo;
void h_salir(int codigo) __attribute__((noreturn));
u32 h_tecla(int espera);		/* 0 si no hay (sin espera) */
int h_argumentos(char *d, int cabe);
void *h_memoria(u32 n);			/* más memoria (la llamada 17), a cero; 0 si no hay */
int h_es_ascii_igual(const char *a, const char *b);	/* sin mirar mayúsculas */
int h_empieza(const char *t, const char *p);	/* ¿t empieza por p? (sin mirar mayúsculas) */
void h_copia(char *d, const char *o, int cabe);
int h_hex(const u8 *d, int n, char *t);	/* t: 2n letras y el 0 */
int h_de_hex(const char *t, u8 *d, int n);	/* 1 si eran 2n cifras buenas */
int h_base64(const u8 *d, int n, char *t, int cabe);
int h_de_base64(const char *t, int n, u8 *d, int cabe);	/* los octetos, o -1 */

/* los archivos: «ruta» completa, o h_ruta() para los del sistema */
extern char h_sistema;			/* la letra de la unidad del sistema */
void h_ruta(char *d, int cabe, const char *dentro);	/* «S:\HISPA\» + dentro (o «S:\» + dentro sin \HISPA) */
int h_lee_archivo(const char *ruta, void *d, u32 cabe);	/* los octetos leídos, o -1 */
int h_escribe_archivo(const char *ruta, const void *d, u32 n);	/* 0, o -1 */
int h_existe(const char *ruta);
void h_carpeta(const char *ruta);	/* la hace si no está */
/* una línea «clave=valor» de un texto (de un .INI): 1 si está */
int h_ini(const char *texto, const char *clave, char *valor, int cabe);

/* el azar: el contador de ciclos, los tics, la fecha y una semilla guardada */
void h_azar(void *d, u32 n);
void h_ruido(void);

/* la red (RED.BIN) */
extern u32 h_cab[8];			/* la cabecera de la última respuesta */
u32 h_red(u32 orden, u32 a, u32 b, u32 c, const void *d, u32 largo, void *dest, u32 cabe);
const char *h_texto_error_red(u32 e);
int h_texto_a_ip(const char *t, u32 *ip);
void h_ip_a_texto(u32 ip, char *t);	/* (t: 16) */
u32 h_resolver(const char *nombre, u32 *ip);
u32 h_conectar(u32 ip, u32 puerto, u32 espera);
u32 h_enviar(u32 s, const void *d, u32 n);	/* todo: n, o un error */
u32 h_recibir(u32 s, void *d, u32 cabe, u32 espera);	/* n; 0: cerrada; TIEMPO */
void h_cerrar_red(u32 s);
u32 h_escuchar(u32 puerto);
u32 h_aceptar(u32 l, u32 espera, u32 *ip, u32 *puerto);
u32 h_udp_abrir(u32 puerto);
u32 h_udp_enviar(u32 s, u32 ip, u32 puerto, const void *d, u32 n);
u32 h_udp_recibir(u32 s, void *d, u32 cabe, u32 espera, u32 *ip, u32 *puerto);
u32 h_mi_ip(void);			/* 0 si aún no hay */
/* por el servicio TLS (TLS.BIN, la llamada 99): una conexión cifrada */
u32 h_conectar_seguro(const char *nombre, u32 ip, u32 puerto, u32 espera);

/* las ventanas (el escritorio) */
int h_hay_escritorio(void);
u32 h_ventana(const char *nombre, u32 ancho, u32 alto);
int h_suceso(u32 v, struct suceso *s, int espera);
void h_rectangulo(u32 v, int x, int y, int ancho, int alto, u32 color);
void h_texto_fondo(u32 v, int x, int y, const char *t, int n, u32 tinta, u32 fondo);
void h_titulo(u32 v, const char *t);

/* la página 437 y el UTF-8 */
int h_utf8_de_437(u8 c, char *d);	/* los octetos (1 a 3) */
int h_437_de_unicode(u32 u);		/* la letra, o '?' */
#endif
